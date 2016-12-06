
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


/*
* @file rwuagent_confd_startup.cpp
*
* Management agent startup phase handler
*/
#include <sstream>
#include <chrono>
#include <fstream>

#include <confd_lib.h>
#include <confd_cdb.h>
#include <confd_dp.h>
#include <confd_maapi.h>

#include "rwuagent.hpp"
#include "rw-vcs.pb-c.h"
#include "rw-base.pb-c.h"
#include "reaper_client.h"
#include "rwuagent.hpp"
#include "rwuagent_confd.hpp"
#include "rwuagent_confd_mgmt_system.hpp"
#include "rw-mgmtagt-confd.pb-c.h"
#include "rwuagent_sb_req.hpp"
#include <rw_schema_defs.h>

using namespace rw_uagent;
namespace fs = boost::filesystem;

static const char* CONFD_INIT_FILE = ".init_complete";
static const int PHASE_0 = 0;
static const int PHASE_1 = 1;
static const int PHASE_2 = 2;


const char* ConfdMgmtSystem::ID = "ConfdMgmtSystem";
NbPluginRegister<ConfdMgmtSystem> ConfdMgmtSystem::registrator;

void ConfdMgmtSystem::start_confd_phase_1_cb(void* ctx)
{
  RW_ASSERT(ctx);
  static_cast<ConfdMgmtSystem*>(ctx)->start_confd_phase_1();
}

void ConfdMgmtSystem::start_confd_reload_cb(void* ctx)
{
  RW_ASSERT(ctx);
  static_cast<ConfdMgmtSystem*>(ctx)->start_confd_reload();
}

void ConfdMgmtSystem::start_confd_phase_2_cb(void* ctx)
{
  RW_ASSERT(ctx);
  static_cast<ConfdMgmtSystem*>(ctx)->start_confd_phase_2();
}

void ConfdMgmtSystem::config_mgmt_start_cb(
      rwdts_xact_t* xact,
      rwdts_xact_status_t* xact_status,
      void* user_data)
{
  auto* self = static_cast<ConfdMgmtSystem*>(user_data);
  self->config_mgmt_start( xact, xact_status );
}

void ConfdMgmtSystem::retry_mgmt_start_cb(void* user_data)
{
  auto* self = static_cast<ConfdMgmtSystem*>(user_data);
  self->start_mgmt_instance();
}

void ConfdMgmtSystem::configure_confd_ha_cb(void* ctx)
{
  RW_ASSERT (ctx);
  return static_cast<ConfdMgmtSystem*>(ctx)->configure_confd_ha();
}

void ConfdMgmtSystem::setup_confd_subscription_cb(void* ctx)
{
  RW_ASSERT (ctx);
  auto self = static_cast<ConfdMgmtSystem*>(ctx);
  self->setup_confd_subscription();
}

#define CREATE_STATE(S1, S2, Func)                                      \
    std::make_pair(std::make_pair(S1, S2), &ConfdMgmtSystem::Func)

#define ADD_STATE(S1, S2, Func)                 \
  state_mc_.insert(                             \
          CREATE_STATE(S1, S2, Func))

#define SET_STATE(curr_state, next_state) \
  state_.first = curr_state;              \
  state_.second = next_state;

ConfdMgmtSystem::ConfdMgmtSystem(Instance* instance):
                     BaseMgmtSystem(instance, "ConfdMgmtSystem"),
                     ws_mgr_(this, instance),
                     ha_mgr_(this, instance)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "ConfdMgmtSystem ctor");

  // ATTN: stdout?  is that for error logs?  Don't we want to capture that?
  confd_init ("rwUagent", stdout, CONFD_DEBUG);

  if (instance_->is_ha_master()) build_master_state_machine();
  else build_standby_state_machine();
}

void ConfdMgmtSystem::build_master_state_machine()
{
  ADD_STATE (CONFD_PHASE_0, CONFD_PHASE_1,   start_confd_phase_1_cb);
  ADD_STATE (TRANSITIONING, CONFD_PHASE_1,   start_confd_phase_1_cb);
  ADD_STATE (CONFD_PHASE_1, CONFD_HA_STATE,  configure_confd_ha_cb);
  ADD_STATE (CONFD_HA_STATE,SUBSCRIBE,       setup_confd_subscription_cb);
  ADD_STATE (SUBSCRIBE,     RELOAD,          start_confd_reload_cb);
  ADD_STATE (RELOAD,        CONFD_PHASE_2,   start_confd_phase_2_cb);
  ADD_STATE (TRANSITIONING, CONFD_PHASE_2,   start_confd_phase_2_cb);
  ADD_STATE (CONFD_PHASE_2, PUBLISH,         publish_agent_ready_state_cb);
}

void ConfdMgmtSystem::build_standby_state_machine()
{
  ADD_STATE (CONFD_PHASE_0, CONFD_PHASE_1,   start_confd_phase_1_cb);
  ADD_STATE (TRANSITIONING, CONFD_PHASE_1,   start_confd_phase_1_cb);
  ADD_STATE (CONFD_PHASE_1, CONFD_HA_STATE,  configure_confd_ha_cb);
  ADD_STATE (CONFD_HA_STATE,CONFD_PHASE_2,   start_confd_phase_2_cb);
  ADD_STATE (TRANSITIONING, CONFD_PHASE_2,   start_confd_phase_2_cb);
}

void ConfdMgmtSystem::on_ha_state_change_event(HAEventType event)
{
  ha_mgr_.on_ha_state_change_event(event);
}

void ConfdMgmtSystem::standby_to_master_transition()
{
  RW_MA_INST_LOG (instance_, InstanceCritInfo,
      "Agent received subscriber promotion event. Proceeding with state machine completion.");

  auto retry_transition = [](void* ud) -> void {
    auto self = static_cast<ConfdMgmtSystem*>(ud);
    self->standby_to_master_transition();
  };

  rw_status_t st = ha_mgr_.ha_init();
  if (st != RW_STATUS_SUCCESS) {
    auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5);
    rwsched_dispatch_after_f(instance_->rwsched_tasklet(),
                             when,
                             rwsched_dispatch_get_main_queue(instance_->rwsched()),
                             this,
                             retry_transition);
    return;
  }

  in_transition_ = true;
  // Rebuild the the state machine
  build_master_state_machine(); 

  // continue the state machine from 
  // setting of confd ha state
  SET_STATE (CONFD_PHASE_1, CONFD_HA_STATE);
  proceed_to_next_state();

  return;
}

#undef CREATE_STATE
#undef ADD_STATE

bool ConfdMgmtSystem::system_startup()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "Confd system startup");
  RW_MA_INST_LOG(instance_, InstanceDebug, "System startup for confd");

  create_proxy_manifest_config();

  ha_mgr_.subscribe_for_master_mgmt_info();

  return true;
}


void ConfdMgmtSystem::create_proxy_manifest_config()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "create proxy manifest cfg");
  RW_MA_INST_LOG(instance_, InstanceDebug, "Create manifest entry configuration");

  std::string error_out;
  rw_yang::XMLDocument::uptr_t req;

  std::string xml(manifest_cfg_);
  auto rift_var_root = get_rift_var_root();
  auto rift_install = get_rift_install();

  char hostname[RWUAGENT_MAX_HOSTNAME_SZ];
  hostname[RWUAGENT_MAX_HOSTNAME_SZ - 1] = 0;
  int res = gethostname(hostname, RWUAGENT_MAX_HOSTNAME_SZ - 2);
  RW_ASSERT(res != -1);
  hostname_.assign(hostname);

  if (instance_->non_persist_ws_) {
    unsigned long seconds_since_epoch =
          std::chrono::duration_cast<std::chrono::seconds>
                  (std::chrono::system_clock::now().time_since_epoch()).count();

    std::ostringstream oss;
    oss << RW_SCHEMA_MGMT_TEST_PREFIX << &hostname[0] << "." << seconds_since_epoch;
    confd_dir_ = std::move(oss.str());
  } else {

    rwtasklet_info_ptr_t tasklet_info = instance_->rwtasklet();

    if (   tasklet_info
        && tasklet_info->rwvcs
        && tasklet_info->rwvcs->pb_rwmanifest
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->persist_dir_name) {

      confd_dir_ = tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->persist_dir_name;

      std::size_t pos = confd_dir_.find(RW_SCHEMA_MGMT_PERSIST_PREFIX);
      if (pos == std::string::npos || pos != 0) {
        confd_dir_.insert(0, RW_SCHEMA_MGMT_PERSIST_PREFIX);
      }

    } else {
      std::ostringstream oss;
      oss << RW_SCHEMA_MGMT_PERSIST_PREFIX << &hostname[0];
      confd_dir_ = std::move(oss.str());
    }
  }

  confd_dir_ = rift_var_root + "/" + confd_dir_;
  size_t pos = xml.find("RIFT_INSTALL");
  RW_ASSERT (pos != std::string::npos);
  xml.replace(pos, strlen("RIFT_INSTALL"), rift_install);

  pos = xml.find("CONFD_DIR");
  RW_ASSERT (pos != std::string::npos);
  xml.replace(pos, strlen("CONFD_DIR"), confd_dir_);

  pos = xml.find("COMPONENT_NAME");
  RW_ASSERT (pos != std::string::npos);
  std::string component_name(get_component_name());
  if (!instance_->is_ha_master()) {
    component_name += "-stby-" + std::string(instance_->rwvcs()->identity.rwvm_name);
  }
  xml.replace(pos, strlen("COMPONENT_NAME"), component_name);

  req = std::move(instance_->xml_mgr()->create_document_from_string(xml.c_str(), error_out, false));
  RW_ASSERT(req.get());
  // ATTN: We are not publishing this data since
  // there is no subscriber for rw-manifest.
  // What happens is, when the vstart RPC is fired,
  // the specified parent instance reads this
  // configuration entry from uAgent
  // and creates a registration for it at runtime.
  instance_->dom()->merge(req.get());

  RW_MA_INST_LOG(instance_, InstanceInfo, "Manifest entry configuration created successfully");
}


void ConfdMgmtSystem::start_mgmt_instance()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "start management server" );

  // Initialize confd_daemon

  if (!confd_daemon_.get()) {
    log_file_manager_.reset(new LogFileManager(instance_));

    RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "start confd daemon" );
    confd_daemon_.reset(new ConfdDaemon(this->instance_));

    RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "attempt confd connection" );
    confd_daemon_->try_confd_connection();

    // Register for logrotate config subscription
    RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "register logrotate cfg subscription" );
    confd_daemon_->confd_log()->register_config();
  }

  // Invoke vstart for starting confd instance
  RWPB_T_PATHSPEC(RwVcs_input_Vstart) start_act = *(RWPB_G_PATHSPEC_VALUE(RwVcs_input_Vstart));
  RWPB_M_MSG_DECL_INIT(RwVcs_input_Vstart, start_act_msg);

  start_act_msg.parent_instance = const_cast<char*>(instance_->rwvcs()->identity.rwvm_name);
  std::string component_name(get_component_name());
  if (!instance_->is_ha_master()) {
    component_name += "-stby-" + std::string(start_act_msg.parent_instance);
  }
  start_act_msg.component_name = &component_name[0];

  std::string log("Starting management instance. ");
  log += "Parent instance: " + std::string(start_act_msg.parent_instance)
       + ". Component name: " + start_act_msg.component_name;
  RW_MA_INST_LOG(instance_, InstanceCritInfo, log.c_str());

  auto* xact = rwdts_api_query_ks(instance_->dts_api(),
                                  (rw_keyspec_path_t*)&start_act,
                                  RWDTS_QUERY_RPC,
                                  0,
                                  config_mgmt_start_cb,
                                  this,
                                  &start_act_msg.base);
  RW_ASSERT(xact);
}


void ConfdMgmtSystem::config_mgmt_start(
    rwdts_xact_t* xact,
    rwdts_xact_status_t* xact_status)
{
  RW_MA_INST_LOG(instance_, InstanceDebug, "confd vstart response");
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "confd vstart result",
           RWMEMLOG_ARG_PRINTF_INTPTR("st=%" PRIdPTR, (intptr_t)xact_status->status),
           RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact) );

  switch (xact_status->status) {
    case RWDTS_XACT_COMMITTED:
      RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "confd vstart complete");
      RW_MA_INST_LOG(instance_, InstanceCritInfo, "confd started");
      inst_ready_ = true;
      break;

    default:
    case RWDTS_XACT_FAILURE:
    case RWDTS_XACT_ABORTED: {
      RW_MA_INST_LOG(instance_, InstanceError, "Unable to start management instance, retrying");
      auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5);
      rwsched_dispatch_after_f(instance_->rwsched_tasklet(),
                               when,
                               rwsched_dispatch_get_main_queue(instance_->rwsched()),
                               this,
                               retry_mgmt_start_cb);
      break;
    }
  }
}


rw_status_t ConfdMgmtSystem::system_init()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "initialize connection" );
  RW_ASSERT (curr_phase() == CONFD_PHASE_0);
  std::string tmp_log;

  close_sockets();

  sock_ = socket(instance_->confd_addr_->sa_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
  RW_ASSERT (sock_ >= 0);

  auto ret = maapi_connect(sock_, instance_->confd_addr_, instance_->confd_addr_size_);
  if (ret != CONFD_OK) {
    tmp_log="MAAPI connect failed: " + std::string(confd_lasterr()) + ". Retrying.";
    RW_MA_INST_LOG (instance_, InstanceError, tmp_log.c_str());
    RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "failed maapi_connect" );
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG (instance_, InstanceNotice, "MAAPI connection succeeded");

  read_sock_ = socket(instance_->confd_addr_->sa_family, SOCK_STREAM, 0);
  RW_ASSERT (read_sock_ >= 0);

  ret = cdb_connect(read_sock_, CDB_READ_SOCKET, instance_->confd_addr_,
      instance_->confd_addr_size_);

  if (ret != CONFD_OK) {
    tmp_log="CDB read socket connection failed. Retrying. " + std::string(confd_lasterr());
    RW_MA_INST_LOG (instance_, InstanceError, tmp_log.c_str());
    RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "failed cdb_connect");
    return RW_STATUS_FAILURE;
  }

  return ha_mgr_.ha_init();
}

rw_status_t ConfdMgmtSystem::create_mgmt_directory()
{
  return ws_mgr_.create_mgmt_directory();
}

void ConfdMgmtSystem::start_upgrade(int modules)
{
  confd_daemon_->start_upgrade(modules);
}

rw_status_t ConfdMgmtSystem::annotate_schema()
{
  if (!instance_->initializing_composite_schema_
      && confd_load_schemas(instance_->confd_addr_,
                            instance_->confd_addr_size_) != CONFD_OK)
  {
    RW_MA_INST_LOG (instance_, InstanceNotice,
                    "RW.uAgent - load of  Confdb schema failed.");
    return RW_STATUS_FAILURE;
  }

  bool ret = instance_->xml_mgr()->get_yang_model()->app_data_get_token(
      YANGMODEL_ANNOTATION_KEY,
      YANGMODEL_ANNOTATION_CONFD_NS,
      &instance_->xml_mgr()->get_yang_model()->adt_confd_ns_);
  RW_ASSERT(ret);

  ret = instance_->xml_mgr()->get_yang_model()->app_data_get_token(
      YANGMODEL_ANNOTATION_KEY,
      YANGMODEL_ANNOTATION_CONFD_NAME,
      &instance_->xml_mgr()->get_yang_model()->adt_confd_name_);
  RW_ASSERT(ret);

  if (!instance_->initializing_composite_schema_) {
    confd_daemon_->confd_config()->annotate_yang_model_confd();
  }

  return RW_STATUS_SUCCESS;
}

void ConfdMgmtSystem::close_sockets()
{
  if (sock_ >= 0) close(sock_);
  sock_ = -1;

  if (read_sock_ >= 0) close(read_sock_);
  read_sock_ = -1;
}

void ConfdMgmtSystem::proceed_to_next_state()
{
  RW_ASSERT (next_phase() != DONE);

  rwsched_dispatch_async_f(
      instance_->rwsched_tasklet(),
      rwsched_dispatch_get_main_queue(instance_->rwsched()),
      this,
      state_mc_[state_]);
}

void ConfdMgmtSystem::retry_phase_cb(ConfdMgmtSystem::CB cb)
{
  auto curr_state_str = phase_2_str[curr_phase()];
  auto next_state_str = phase_2_str[next_phase()];
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "retry phase cb",
           RWMEMLOG_ARG_STRNCPY(sizeof(curr_state_str), curr_state_str),
           RWMEMLOG_ARG_STRNCPY(sizeof(next_state_str), next_state_str));

  auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC);
  rwsched_dispatch_after_f(
        instance_->rwsched_tasklet(),
        when,
        rwsched_dispatch_get_main_queue(instance_->rwsched()),
        this,
        cb);

  return;
}

#define OK_RETRY(retval, err_msg) \
  if (retval != CONFD_OK) { \
    const char* s_ = err_msg; \
    RW_MA_INST_LOG (instance_, InstanceError, s_); \
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "start confd err", \
             RWMEMLOG_ARG_STRCPY_MAX(s_,128) ); \
    return retry_phase_cb(cb); \
  }

void ConfdMgmtSystem::start_confd_phase_1()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "start confd phase 1");
  RW_MA_INST_LOG (instance_, InstanceInfo, "start confd phase 1");

  RW_ASSERT (next_phase() == CONFD_PHASE_1);
  std::string tmp_log;

  auto cb = state_mc_[state_];
  RW_ASSERT (cb);

  if (curr_phase() == CONFD_PHASE_0) {
    auto ret = maapi_start_phase(sock_, PHASE_1, 0/*async*/);
    OK_RETRY (ret,
              (tmp_log="Maapi start phase1 failed. Retrying. "
                        + std::string(confd_lasterr())).c_str());

    SET_STATE (TRANSITIONING, CONFD_PHASE_1);
    return retry_phase_cb(cb);
  }

  if ((curr_phase() == TRANSITIONING) && (next_phase() == CONFD_PHASE_1)) {
    struct cdb_phase cdb_phase;

    auto ret = cdb_get_phase(read_sock_, &cdb_phase);
    OK_RETRY (ret,
              (tmp_log="CDB get phase failed. Current phase 0. Retrying. "
                        + std::string(confd_lasterr())).c_str());

    if (cdb_phase.phase != PHASE_1) {
      return retry_phase_cb(cb);
    }
  }

  SET_STATE (CONFD_PHASE_1, CONFD_HA_STATE);

  RW_MA_INST_LOG(instance_, InstanceCritInfo,
        "Configuration management phase-1 finished.");

  proceed_to_next_state();
}

void ConfdMgmtSystem::configure_confd_ha()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "configure confd ha");
  RW_MA_INST_LOG (instance_, InstanceCritInfo, "Configuring confd HA mode");

  auto cb = state_mc_[state_];
  RW_ASSERT (cb);

  if (instance_->is_ha_master()) {
    SET_STATE (CONFD_HA_STATE, SUBSCRIBE);
  } else {
    if (!ha_mgr_.has_master_details()) {
      retry_phase_cb(cb);
      return;
    }
    SET_STATE (CONFD_HA_STATE, CONFD_PHASE_2);
  }

  ha_mgr_.set_confd_ha_state();
}

void ConfdMgmtSystem::setup_confd_subscription()
{
  SET_STATE (SUBSCRIBE, RELOAD);
  confd_daemon_->confd_config()->setup_confd_subscription();
}

void ConfdMgmtSystem::start_confd_reload()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "start confd reload phase");
  RW_MA_INST_LOG(instance_, InstanceInfo, "start confd reload phase");
  RW_ASSERT (next_phase() == RELOAD);

  is_under_reload_ = true;

  SET_STATE (RELOAD, CONFD_PHASE_2);
  rwsched_dispatch_async_f(
                        instance_->rwsched_tasklet(),
                        confd_daemon_->confd_config()->reload_q_,
                        confd_daemon_->confd_config(),
                        NbReqConfdConfig::reload_configuration);
}

void ConfdMgmtSystem::start_confd_phase_2()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "start confd phase 2");
  RW_MA_INST_LOG (instance_, InstanceInfo, "start confd phase 2");

  in_transition_ = false;

  RW_ASSERT (next_phase() == CONFD_PHASE_2);
  std::string tmp_log;

  auto cb = state_mc_[state_];
  RW_ASSERT (cb);

  // Confd reload is done if we are here
  is_under_reload_ = false;

  if (curr_phase() == RELOAD) {
    auto ret = maapi_start_phase(sock_, PHASE_2, 0/*async*/);
    OK_RETRY (ret,
        (tmp_log="Maapi start phase2 failed. Retrying. "
                 + std::string(confd_lasterr())).c_str());

    SET_STATE (TRANSITIONING, CONFD_PHASE_2);
    return retry_phase_cb(cb);
  }

  if ((curr_phase() == TRANSITIONING) && (next_phase() == CONFD_PHASE_2)) {
    struct cdb_phase cdb_phase;
    auto ret = cdb_get_phase(read_sock_, &cdb_phase);
    OK_RETRY (ret,
        (tmp_log="CDB get phase failed. Current phase 1. Retrying. "
                 + std::string(confd_lasterr())).c_str());

    if (cdb_phase.phase != PHASE_2) {
      return retry_phase_cb(cb);
    }
  }

  auto confd_init_file = confd_dir_ + "/" + CONFD_INIT_FILE;

  struct stat fst;
  if (stat(confd_init_file.c_str(), &fst) != 0) {
    RW_MA_INST_LOG (instance_, InstanceCritInfo,
        "Creating agent confd init file");
    std::fstream ofile(confd_init_file, std::ios::out);
    RW_ASSERT_MESSAGE(ofile.good(), "Failed to create confd init file");
    ofile.close();
  }

  RW_MA_INST_LOG(instance_, InstanceCritInfo,
      "Configuration management startup complete. Northbound interfaces are enabled.");

  if (instance_->is_ha_master()) {
    SET_STATE (CONFD_PHASE_2, PUBLISH);
    proceed_to_next_state();
  }

  instance_->dts()->publish_internal_hb_event();

  return;
}


void ConfdMgmtSystem::publish_agent_ready_state_cb(void* ctx)
{
  RW_ASSERT (ctx);
  auto self = static_cast<ConfdMgmtSystem*>(ctx);
  self->publish_agent_ready_state();
}

void ConfdMgmtSystem::publish_agent_ready_state()
{
  instance_->dts()->publish_config_state();
  ready_for_nb_clients_ = true;
}

#undef OK_RETRY
#undef SET_STATE


StartStatus ConfdMgmtSystem::handle_rpc(SbReqRpc* parent_rpc, const ProtobufCMessage* msg)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle rpc");
  std::string rpcn = protobuf_c_message_descriptor_xml_element_name(msg->descriptor);

  if (rpcn == "show-agent-logs") {
    RWPB_M_MSG_DECL_CAST(RwMgmtagt_input_ShowAgentLogs, req, msg);
    return confd_daemon_->confd_log()->show_logs(parent_rpc, req);
  }

  RWPB_M_MSG_DECL_SAFE_CAST(RwMgmtagtConfd_input_MgmtAgentConfd, mgmt_agent, msg);
  UniquePtrProtobufCMessage<>::uptr_t dup_msg;
  if (!mgmt_agent) {
    dup_msg.reset(
        protobuf_c_message_duplicate(
          nullptr,
          msg,
          RWPB_G_MSG_PBCMD(RwMgmtagtConfd_input_MgmtAgentConfd) ));

    if (!dup_msg.get()) {
      NetconfErrorList nc_errors;
      NetconfError& err = nc_errors.add_error();
      std::string err_str = "Cannot coerce RPC to rw-mgmtagt-confd:mgmt-agent";
      err.set_error_message(err_str.c_str());

      RW_MA_INST_LOG (instance_, InstanceError, err_str.c_str());
      parent_rpc->internal_done(&nc_errors);
      return StartStatus::Done;
    }
    mgmt_agent = RWPB_M_MSG_SAFE_CAST(RwMgmtagtConfd_input_MgmtAgentConfd, dup_msg.get());
  }

  if (mgmt_agent->clear_stats) {
    confd_daemon_->clear_statistics();
    for (auto& elem : instance_->statistics_) {
      elem.reset();
    }
  } else if (mgmt_agent->dom_refresh_period) {
    if (mgmt_agent->dom_refresh_period->has_cli_dom_refresh_period) {
      instance_->cli_dom_refresh_period_msec_ =
        mgmt_agent->dom_refresh_period->cli_dom_refresh_period;
    }
    if (mgmt_agent->dom_refresh_period->has_nc_rest_dom_refresh_period) {
      instance_->nc_rest_refresh_period_msec_ =
        mgmt_agent->dom_refresh_period->nc_rest_dom_refresh_period;
    }
  }

  parent_rpc->internal_done();
  return StartStatus::Done;
}

rwdts_member_rsp_code_t
ConfdMgmtSystem::handle_notification(const ProtobufCMessage* msg)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle notification");
  return confd_daemon_->handle_notification(msg);
}

StartStatus ConfdMgmtSystem::handle_get(SbReqGet* parent_get, XMLNode* node)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle get");
  typedef rw_yang_netconf_op_status_t (ConfdDaemon::*child_handler)(rw_yang::XMLNode *);

  const char *node_names[] = {"client", "dom-refresh-period"};
  child_handler handlers[] = {&ConfdDaemon::get_confd_daemon,
                              &ConfdDaemon::get_dom_refresh_period};

  uint8_t num_handlers = sizeof (node_names)/sizeof (char *);

  // ATTN: Code duplicated from SbReqGet
  if (nullptr == node->get_first_child()) {
    for (int i = 0; i < num_handlers; i++) {
      (confd_daemon()->*handlers[i])(node);
    }
  } else {
    XMLNode *child = node->get_first_child();
    RW_ASSERT(child);
    for (size_t i = 0; i < num_handlers; i++) {
      if (node_names[i] == child->get_local_name()) {
        node->remove_child (child);
        (confd_daemon()->*handlers[i])(node);
        break;
      }
    }
  }

  return StartStatus::Done;
}
