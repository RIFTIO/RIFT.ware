
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


/**
 * @file rwuagent_instance.cpp
 *
 * Management agent instance.
 */

#include <algorithm>
#include <memory>


#include <rw_pb_schema.h>
#include <rw_schema_defs.h>

#include "rwuagent.hpp"
#include "rwuagent_msg_client.h"
#include "rwuagent_log_file_manager.hpp"
#include "rwuagent_request_mode.hpp"

using namespace rw_uagent;
using namespace rw_yang;
namespace fs = boost::filesystem;


RW_CF_TYPE_DEFINE("RW.uAgent RWTasklet Component Type", rwuagent_component_t);
RW_CF_TYPE_DEFINE("RW.uAgent RWTasklet Instance Type", rwuagent_instance_t);

const char *rw_uagent::uagent_yang_ns = "http://riftio.com/ns/riftware-1.0/rw-mgmtagt";
const char *rw_uagent::uagent_dts_yang_ns = "http://riftio.com/ns/riftware-1.0/rw-mgmtagt-dts";
const char *rw_uagent::uagent_confd_yang_ns = "http://riftio.com/ns/riftware-1.0/rw-mgmtagt-confd";

// Predefined Netconf Notification streams
static std::vector<netconf_stream_info_t> netconf_stream_initializer {
  { "uagent_notification", "RW.MgmtAgent notifications stream", true },
  { "ha_notification", "RW.HA notifications stream", true }
};

// Defines a static mapping between a Yang Node and the notifications stream.
// If there is no mapping defined, then it defaults to uagent_notifications
// stream.
static std::map<NotificationYangNode, std::string> notifcation_stream_map_initializer {
  { {"test-tasklet-failed", "http://riftio.com/ns/core/mgmt/rwuagent/test/notif"}, 
      "ha_notification" },
  { {"heartbeat-alarm", "http://riftio.com/ns/riftware-1.0/rw-vcs"},
      "ha_notification" },
  { {"task-restart-begin", "http://riftio.com/ns/riftware-1.0/rwdts"}, 
      "ha_notification" },
  { {"task-restarted", "http://riftio.com/ns/riftware-1.0/rwdts"},
      "ha_notification" },
  { {"vmstate-change", "http://riftio.com/ns/riftware-1.0/rw-vcs"},
      "ha_notification" }
};

rwuagent_component_t rwuagent_component_init(void)
{
  rwuagent_component_t component = (rwuagent_component_t)RW_CF_TYPE_MALLOC0(sizeof(*component), rwuagent_component_t);
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);

  return component;
}

void rwuagent_component_deinit(
    rwuagent_component_t component)
{
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
}

rwuagent_instance_t rwuagent_instance_alloc(
    rwuagent_component_t component,
    struct rwtasklet_info_s * rwtasklet_info,
    RwTaskletPlugin_RWExecURL *instance_url)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
  RW_ASSERT(instance_url);

  // Allocate a new rwuagent_instance structure
  rwuagent_instance_t instance = (rwuagent_instance_t) RW_CF_TYPE_MALLOC0(sizeof(*instance), rwuagent_instance_t);
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);
  instance->component = component;

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  // Allocate the real instance structure
  instance->instance = new Instance(instance);

  // Return the allocated instance
  return instance;
}

void rwuagent_instance_free(
    rwuagent_component_t component,
    rwuagent_instance_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);
}

rwtrace_ctx_t* rwuagent_get_rwtrace_instance(
    rwuagent_instance_t instance)
{
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);
  return instance->rwtasklet_info ? instance->rwtasklet_info->rwtrace_instance : NULL;
}

void rwuagent_instance_start(
    rwuagent_component_t component,
    rwuagent_instance_t instance)
{
  RW_CF_TYPE_VALIDATE(component, rwuagent_component_t);
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);
  RW_ASSERT(instance->component == component);
  RW_ASSERT(instance->instance);
  instance->instance->start();
}

namespace rw_uagent {

void rw_management_agent_xml_log_cb(void *user_data,
                                    rw_xml_log_level_e level,
                                    const char *fn,
                                    const char *log_msg)
{
  /*
   * ATTN: These messages need to get back to the transaction that
   * generated them, so that they can be included in the NETCONF error
   * response, if any.
   *
   * ATTN: I think RW.XML needs more context when generating messages -
   * just binding the errors to manage is insufficient - need to
   * actually bind the messages to a particular client/xact.
   */
  auto *inst = static_cast<Instance*>(user_data);

  switch (level) {
    case RW_XML_LOG_LEVEL_DEBUG:
      RW_MA_DOMMGR_LOG (inst, DommgrDebug, fn, log_msg);
      break;
    case RW_XML_LOG_LEVEL_INFO:
      RW_MA_DOMMGR_LOG (inst, DommgrNotice, fn, log_msg);
      break;
    case RW_XML_LOG_LEVEL_ERROR:
      RW_MA_DOMMGR_LOG (inst, DommgrError, fn, log_msg);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
}
}


/**
 * Construct a uAgent instance.
 */
Instance::Instance(rwuagent_instance_t rwuai)
    : memlog_inst_("MgmtAgent", 200),
      memlog_buf_(
          memlog_inst_,
          "Instance",
          reinterpret_cast<intptr_t>(this)),
      rwuai_(rwuai),
      initializing_composite_schema_(true),
      netconf_streams_(netconf_stream_initializer),
      notification_stream_map_(notifcation_stream_map_initializer)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "Instance constructor");
  RW_CF_TYPE_VALIDATE(rwuai_, rwuagent_instance_t);

  RW_ASSERT (rwuai_ && rwuai_->rwtasklet_info);
  RwmgmtAgentMode agent_mode = RW_MANIFEST_RWMGMT_AGENT_MODE_AUTOMODE;
  if (   rwuai_->rwtasklet_info->rwvcs
      && rwuai_->rwtasklet_info->rwvcs->pb_rwmanifest
      && rwuai_->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase
      && rwuai_->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt
      && rwuai_->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->has_agent_mode) {
    agent_mode = rwuai_->rwtasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->agent_mode;
  }

  switch (agent_mode) {
    case RW_MANIFEST_RWMGMT_AGENT_MODE_CONFD:
      request_mode_ = RequestMode::CONFD;
      break;

    case RW_MANIFEST_RWMGMT_AGENT_MODE_RWXML:
      request_mode_ = RequestMode::XML;
      break;

    default:
    case RW_MANIFEST_RWMGMT_AGENT_MODE_AUTOMODE: {
      auto prototype_conf_file = get_rift_var_root() + "/" + RW_SCHEMA_CONFD_PROTOTYPE_CONF;
      boost::system::error_code ec;
      if (fs::exists(prototype_conf_file)) {
        request_mode_ = RequestMode::CONFD;
      } else {
        request_mode_ = RequestMode::XML;
      }
      break;
    }
  }
}

/**
 * Destroy a uAgent instance.
 */
Instance::~Instance()
{
  // ATTN: close the messaging services and channels

  // ATTN: Iterate through all the Clients, destroying them

  rwmemlog_instance_dts_deregister(memlog_inst_, false/*dts_internal*/);

  // De-register the memlog instance of the xmlmgr.
  rwmemlog_instance_dts_deregister(xml_mgr_->get_memlog_inst(), false/*dts_internal*/);

  RW_CF_TYPE_VALIDATE(rwuai_, rwuagent_instance_t);


  // QueueManager destructor
  for (auto const & entry : queues_) {
    QueueType const & type = entry.first;
    release_queue(type);
  }

}

void Instance::async_start(void *ctxt)
{
  auto* self = static_cast<Instance*>(ctxt);
  RW_ASSERT (self);
  auto& memlog_buf = self->memlog_buf_;

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "setup dom" );
  self->setup_dom("rw-mgmtagt-composite");

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "load schema" );

#if 0 // RIFT-10722
  char* err_str = nullptr;
  rw_yang_validate_schema("rw-mgmtagt-composite", &err_str);

  if (err_str) {
    RW_MA_INST_LOG(self, InstanceError, err_str);
    free (err_str);
  }
#endif

  // Load the schema specified at boot time
  self->ypbc_schema_ = rw_load_schema("librwuagent_yang_gen.so", "rw-mgmtagt-composite");
  RW_ASSERT(self->ypbc_schema_);

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "register schema" );
  self->yang_model()->load_schema_ypbc(self->ypbc_schema_);

  rw_status_t status = self->yang_model()->register_ypbc_schema(self->ypbc_schema_);
  if ( RW_STATUS_SUCCESS != status ) {
    RW_MA_INST_LOG(self, InstanceCritInfo, "Error while registering for ypbc schema.");
  }

  rwsched_dispatch_async_f(self->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(self->rwsched()),
                           self,
                           Instance::async_start_dts);
}

void Instance::async_start_dts(void *ctxt)
{
  auto* self = static_cast<Instance*>(ctxt);
  RW_ASSERT (self);
  auto& memlog_buf = self->memlog_buf_;

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "start dts member" );
  self->dts_.reset(new DtsMember(self));

  // register with dts for boot-time schema
  self->dts_->load_registrations(self->yang_model());

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "register rwmemlog" );
  rwmemlog_instance_dts_register( self->memlog_inst_,
                                  self->rwtasklet(),
                                  self->dts_->api() );

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "register XMLMgr rwmemlog instance" );
  rwmemlog_instance_dts_register( self->xml_mgr_->get_memlog_inst(),
                                  self->rwtasklet(),
                                  self->dts_->api() );

  // ATTN: Move this to a manager?
  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "start dynamic schema driver" );
  self->schema_driver_.reset( new DynamicSchemaDriver(self, self->dts_->api()) );
  rwsched_dispatch_async_f(self->rwsched_tasklet(), // ATTN: Should be in constr?
                           rwsched_dispatch_get_main_queue(self->rwsched()),
                           self->schema_driver_.get(),
                           DynamicSchemaDriver::run);

  RWMEMLOG( memlog_buf, RWMEMLOG_MEM2, "register schema listener" );
  if (AgentDynSchemaHelper::register_for_dynamic_schema(self) != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG(self, InstanceError, "Error while registering for dynamic schema.");
    return;
  }

  rwsched_dispatch_async_f(self->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(self->rwsched()),
                           self,
                           Instance::async_start_mgmt_system);

}

void Instance::async_start_mgmt_system(void* ctxt)
{
  auto* self = static_cast<Instance*>(ctxt);
  RW_ASSERT (self);

  if (self->initializing_composite_schema_) {
    // still loading composite schema, try again
    RWMEMLOG( self->memlog_buf_, RWMEMLOG_MEM2, "waiting on composite to start northbound" );
    rwsched_dispatch_after_f(self->rwsched_tasklet(),
                             dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC),
                             rwsched_dispatch_get_main_queue(self->rwsched()),
                             self,
                             Instance::async_start_mgmt_system);
    return;
  }

  rwsched_dispatch_async_f(self->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(self->rwsched()),
                           self->mgmt_handler(),
                           BaseMgmtSystem::system_startup_cb);
  return;
}

void Instance::start_messaging_client()
{
   //Instantiate the rw-msg interfaces
   RWMEMLOG ( memlog_buf_, RWMEMLOG_MEM2, "start messaging client" );
   msg_client_.reset(new MsgClient(this));
}

void Instance::dyn_schema_dts_registration(void *ctxt)
{
  auto *self = static_cast<Instance*>(ctxt);
  RW_ASSERT (self);

  self->dts_->load_registrations(self->yang_model());

  self->initializing_composite_schema_ = false;
}

bool Instance::registrations_propagated()
{
  if(exported_modules_app_data_.size() == 0 &&
      is_ha_master()) {
    // modules haven't been loaded yet
    return false;
  }

  auto comparison = [](northbound_dts_registrations_app_data value) -> bool
  {
    return value.propagated;
  };

  bool const registrations_are_propagated = std::all_of(exported_modules_app_data_.begin(),
                                                        exported_modules_app_data_.end(),
                                                        comparison);

  return exported_modules_app_data_.size() == 0 ? true : registrations_are_propagated;
}

void Instance::start()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "start agent");

  rw_status_t rwstatus;
  char cmdargs_var_str[] = "$cmdargs_str";
  char cmdargs_str[1024];
  int argc = 0;
  char **argv = NULL;
  gboolean ret;

  yang_schema_ = "rwuagent";
  std::string log_string;
  RW_MA_INST_LOG(this, InstanceInfo, (log_string = "Schema configured is " + yang_schema_).c_str());

  // HACK: This eventually need to come from management agent
  rwstatus = rwvcs_variable_evaluate_str(rwvcs(),
                                         cmdargs_var_str,
                                         cmdargs_str,
                                         sizeof(cmdargs_str));
  RW_ASSERT(RW_STATUS_SUCCESS == rwstatus);

  std::string log_str = "cmdargs_str";
  log_str += cmdargs_str;

  RW_MA_INST_LOG(this, InstanceCritInfo, log_str.c_str());

  ret = g_shell_parse_argv(cmdargs_str, &argc, &argv, NULL);
  RW_ASSERT(ret == TRUE);

  rwyangutil::ArgumentParser arg_parser(argv, argc);
  ret = parse_cmd_args(arg_parser);
  if (!ret) {
    RW_MA_INST_LOG (this, InstanceError, "Bad arguments given to Agent."
        "Using default arguments");
  }

  auto vcs_inst = rwvcs();
  auto set = inet_aton(vcs_inst->identity.vm_ip_address, &confd_inet_addr_.sin_addr);
  if (set == 0) {
    RW_MA_INST_LOG (this, InstanceError, "Failed to convert vm-address to network byte");
  }

  g_strfreev (argv);

  // set rift environment variables for rw.cli to connect to confd
  // ATTN: this should be read from the manifest when the agent generates confd.conf at runtime (RIFT-5059)
  rw_status_t status = rw_setenv("NETCONF_PORT_NUMBER","2022");

  if (status != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG(this,
                   InstanceError,
                   "Couldn't set NETCONF port number in Rift environment variable");
  }

  // Set the Management agent mode as a RIFT env variable. This can be used by
  // other processes like RW.CLI to decide on which mode to operate.
  const char* agent_mode = nullptr;
  if (RequestMode::CONFD == request_mode_) {
    agent_mode = "CONFD";
  } else {
    agent_mode = "XML";
  }

  status = rw_setenv("RWMGMT_AGENT_MODE", agent_mode);
  if (status != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG(this,
                   InstanceError,
                   "Couldn't set RW_MGMT_AGENT_MODE in Rift environment variable");
  }

  // Set the instance name
  instance_name_ = rwtasklet()->identity.rwtasklet_name;

  // Set the HA mode
  if (rwtasklet()->mode.has_mode_active &&
      rwtasklet()->mode.mode_active)
  {
    RW_MA_INST_LOG (this, InstanceCritInfo, "Agent started as HA-Master.");
    ha_mode_ = HA_MASTER;
  } else {
    RW_MA_INST_LOG (this, InstanceCritInfo, "Agent started as HA-Slave.");
    ha_mode_ = HA_SLAVE;
  }

  if (request_mode_ == RequestMode::CONFD) {
    if (!MgmtSystemFactory::get().is_registered("ConfdMgmtSystem")) {
      RW_MA_INST_LOG(this, InstanceCritInfo, "ConfdMgmtSystem Not Supported..Agent Will not start");
      return;
    }
    mgmt_handler_.reset(
        MgmtSystemFactory::get().create_object("ConfdMgmtSystem", this).release());
  } else {
    RW_ASSERT (MgmtSystemFactory::get().is_registered("XMLMgmtSystem"));
    mgmt_handler_.reset(
        MgmtSystemFactory::get().create_object("XMLMgmtSystem", this).release());
  }

  rwsched_dispatch_async_f(rwsched_tasklet(),
                           get_queue(QueueType::SchemaLoading),
                           this,
                           Instance::async_start);
}


bool Instance::parse_cmd_args(const rwyangutil::ArgumentParser& arg_parser)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "parse cmd args");
  bool status = true;

  std::string proto;
  if (arg_parser.exists("--confd-proto")) {
    proto = arg_parser.get_param("--confd-proto");
  } else {
    RW_MA_INST_LOG(this, InstanceError, "Confd proto not provided. Taking default as AF_INET");
    proto = "AF_INET";
    status = false;
  }

  if (proto == "AF_INET") {
    if (arg_parser.exists("--confd-ip")) {
      auto ret = inet_aton(arg_parser.get_param("--confd-ip").c_str(),
                           &confd_inet_addr_.sin_addr);
      if (ret == 0) {
        std::string log;
        log = "Incorrect IP address passed: " + arg_parser.get_param("--confd-ip");
        RW_MA_INST_LOG (this, InstanceError, log.c_str());
        inet_aton("127.0.0.1", &confd_inet_addr_.sin_addr);
        status = false;
      }
    } else {
      auto ret = inet_aton("127.0.0.1", &confd_inet_addr_.sin_addr);
      if (ret == 0) {
        RW_MA_INST_LOG (this, InstanceError, "Failed to convert localhost to network byte");
        status = false;
      }
    }
#define CONFD_PORT 4565 //ATTN:- This is a temporary hack for now until we move this code to ConfdMgmtSystem
    uint32_t port = CONFD_PORT;
    if (arg_parser.exists("--confd-port")) {
      port = atoi(arg_parser.get_param("--confd-port").c_str());
      if (port == 0 || port > 65535) {
        RW_MA_INST_LOG (this, InstanceError, "Port must be between 0 and 65536");
        port = CONFD_PORT;
        status = false;
      }
    }
    confd_inet_addr_.sin_port = htons(port);
    confd_inet_addr_.sin_family = AF_INET;
    confd_inet_addr_.sin_addr.s_addr = INADDR_ANY;
    confd_addr_ = (struct sockaddr *) &confd_inet_addr_;
    confd_addr_size_ = sizeof (confd_inet_addr_);

  } else {
    RW_MA_INST_LOG (this, InstanceCritInfo, "Unsupported/Unknown proto option specified");
    status = false;
  }

  if (arg_parser.exists("--unique")) {
    non_persist_ws_ = true;
  }

  return status;
}


rw_status_t Instance::handle_dynamic_schema_update(const int batch_size,
                                                   rwdynschema_module_t * modules)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "handle dynamic schema");
  RW_ASSERT (batch_size != 0);

  rw_status_t status = RW_STATUS_SUCCESS;
  update_dyn_state(RW_MGMT_SCHEMA_APPLICATION_STATE_WORKING);

  for (int i = 0; i < batch_size; ++i) {
    RW_ASSERT(modules[i].module_name);
    RW_ASSERT(modules[i].so_filename);

    module_details_t mod;
    mod.module_name = modules[i].module_name;
    mod.so_filename = modules[i].so_filename;
    mod.exported    = modules[i].exported;

    pending_schema_modules_.emplace_front(mod);
  }
  // start confd in-service upgrade
  // ATTN: Signature to be changed based upon
  // the future of CLI command for upgrade
  if (initializing_composite_schema_) {
    // loading composite schema on boot
    perform_dynamic_schema_update();
  } else {
    // normal dynamic schema operation
    mgmt_handler_->start_upgrade(batch_size);
  }
  RW_MA_INST_LOG(this, InstanceInfo, "Dynamic schema update callback completed");

  return status;
}


void Instance::perform_dynamic_schema_update()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "perform dynamic schema");

  after_modules_loaded_cb_ = perform_dynamic_schema_update_end;

  rwsched_dispatch_async_f(rwsched_tasklet(),
                           get_queue(QueueType::SchemaLoading),
                           this,
                           perform_dynamic_schema_update_load_modules);

}

void Instance::perform_dynamic_schema_update_load_modules(void * context)
{
  Instance * instance = static_cast<Instance*>(context);
  RW_ASSERT (instance);

  const module_details_t module = instance->pending_schema_modules_.front();
  instance->pending_schema_modules_.pop_front();

  auto module_name = module.module_name.c_str();
  auto so_filename = module.so_filename.c_str();

  const rw_yang_pb_schema_t* new_schema = rw_load_schema(so_filename, module_name);
  RW_ASSERT(new_schema);

  // make a temporary model in case of failure
  auto *tmp_model = rw_yang::YangModelNcx::create_model();
  RW_ASSERT(tmp_model);

  tmp_model->load_schema_ypbc(new_schema);
  rw_status_t status = tmp_model->register_ypbc_schema(new_schema);
  if ( RW_STATUS_SUCCESS != status ) {
    RW_MA_INST_LOG(instance, InstanceCritInfo, "Error while registering for ypbc schema.");
  }

  // Create new schema
  const rw_yang_pb_schema_t * merged_schema = rw_schema_merge(nullptr, instance->ypbc_schema_, new_schema);

  if (!merged_schema) {
    std::string log_str;
    RW_MA_INST_LOG(instance, InstanceError,
                   (log_str=std::string("Dynamic schema update for ")
                    + module_name + so_filename+ " failed").c_str());

    instance->loading_modules_success_ = false;
    rwsched_dispatch_async_f(instance->rwsched_tasklet(),
                             rwsched_dispatch_get_main_queue(instance->rwsched()),
                             instance,
                             instance->after_modules_loaded_cb_);
    return;
  }

  RW_MA_INST_LOG(instance, InstanceDebug, "Load and merge completed.");

  instance->load_module(module_name);
  // Overwrite the old schema with new
  instance->ypbc_schema_ = merged_schema;

  if (module.exported) {
    instance->exported_modules_.emplace(module.module_name);
  }

  instance->tmp_models_.emplace_back(tmp_model);
  instance->yang_model()->load_schema_ypbc(merged_schema);

  status = instance->yang_model()->register_ypbc_schema(merged_schema);
  if ( RW_STATUS_SUCCESS != status ) {
    RW_MA_INST_LOG(instance, InstanceCritInfo, "Error while registering for ypbc schema.");
  }

  if (instance->pending_schema_modules_.size() > 0) {
    // keep loading modules
    rwsched_dispatch_async_f(instance->rwsched_tasklet(),
                             instance->get_queue(QueueType::SchemaLoading),
                             instance,
                             perform_dynamic_schema_update_load_modules);
   return;
  }

  rwsched_dispatch_async_f(instance->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(instance->rwsched()),
                           instance,
                           instance->after_modules_loaded_cb_);
}

void Instance::schema_registration_ready(rwdts_member_reg_handle_t  reg)
{
  auto comparison = [reg](northbound_dts_registrations_app_data const value) -> bool
  {
    return reg == value.registration;
  };

  exported_modules_app_data_itr_t found_value =
      std::find_if(exported_modules_app_data_.begin(),
                   exported_modules_app_data_.end(),
                   comparison);

  RW_ASSERT(found_value != exported_modules_app_data_.end());

  found_value->propagated = true;
}

void Instance::perform_dynamic_schema_update_end(void * context)
{
  Instance * instance = static_cast<Instance*>(context);
  RW_ASSERT (instance);
  instance->after_modules_loaded_cb_ = nullptr;

  RWMEMLOG_TIME_SCOPE(instance->memlog_buf_,
                      RWMEMLOG_MEM2,
                      "perform dynamic schema dts registrations and confd info");

  if (!instance->loading_modules_success_) {
    RW_MA_INST_LOG (instance, InstanceNotice,
                    "RW.uAgent - module loading failed.");
    return;
  }

  rwdts_api_set_ypbc_schema( instance->dts_api(), instance->ypbc_schema_ );

  rwsched_dispatch_async_f(instance->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(instance->rwsched()),
                           instance,
                           Instance::dyn_schema_dts_registration);

  instance->pending_schema_modules_.clear();

  instance->mgmt_handler_->annotate_schema();
}

void Instance::setup_dom(const char *module_name)
{
  // ATTN: Split this func. XMLMgr and model stuff belongs in instance constr.
  //    ... the doc create and model load belong in separate funcs, and async
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "setup dom");

  RW_MA_INST_LOG (this, InstanceDebug, "Setting up configuration dom");
  RW_ASSERT (!xml_mgr_.get()); // ATTN:- Assumption that there is a single instance.
  xml_mgr_ = std::move(xml_manager_create_xerces());
  xml_mgr_->set_log_cb(rw_management_agent_xml_log_cb,this);

  auto *module = load_module(module_name);
  module->mark_imports_explicit();

  dom_ = std::move(xml_mgr_->create_document());
}

YangModule* Instance::load_module(const char* module_name)
{
  RW_MA_INST_LOG(this, InstanceDebug, "Reloading dom with new module");

  YangModule *module = yang_model()->load_module(module_name);
  // ATTN: Should not crash!
  RW_ASSERT(module);

  //ATTN: Need to call mark_imports_explicit for
  //dynamically loaded modules ??
  // locked_cache_set_flag_only

  return module;
}

void Instance::enqueue_pb_request(NbReqInternal* nbreq)
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "enqueue_pb_request" );

  auto main_q = rwsched_dispatch_get_main_queue(rwsched());
  dispatch_queue_t q = dispatch_get_current_queue();
  RW_ASSERT((void*)q == *(void**)main_q);

  pb_req_q_.emplace_back(nbreq);
  RW_MA_INST_LOG(this, InstanceDebug, "Enqueued pb-req");

  if (pb_req_q_.size() == 1) {
    rwsched_dispatch_async_f(rwsched_tasklet(),
                             main_q,
                             this,
                             Instance::dispatch_next_pb_req);
  }
}

void Instance::dispatch_next_pb_req(void* ud)
{
  RW_ASSERT (ud);
  auto self = static_cast<Instance*>(ud);
  if (self->pb_req_q_.size() == 0) return;

  self->pb_req_q_.front()->execute();
}

void Instance::dequeue_pb_request()
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "dequeue_pb_request" );

  auto main_q = rwsched_dispatch_get_main_queue(rwsched());
  dispatch_queue_t q = dispatch_get_current_queue();
  RW_ASSERT((void*)q == *(void**)main_q);
  RW_ASSERT (pb_req_q_.size());

  pb_req_q_.pop_front();
  RW_MA_INST_LOG(this, InstanceDebug, "Dequeued pb-req");

  rwsched_dispatch_async_f(rwsched_tasklet(),
                           main_q,
                           this,
                           Instance::dispatch_next_pb_req);
}

void Instance::start_tasks_ready_timer()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "start readiness timer");
  rwsched_tasklet_ptr_t tasklet = rwsched_tasklet();
  RW_MA_INST_LOG(this, InstanceInfo, "Starting timer to wait for critical tasklets");

  tasks_ready_timer_ = rwsched_dispatch_source_create(
      tasklet,
      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
      0,
      0,
      rwsched_dispatch_get_main_queue(rwsched()));

  rwsched_dispatch_source_set_event_handler_f(
      tasklet,
      tasks_ready_timer_,
      Instance::tasks_ready_timer_expire_cb);

  rwsched_dispatch_set_context(
      tasklet,
      tasks_ready_timer_,
      this);

  rwsched_dispatch_source_set_timer(
      tasklet,
      tasks_ready_timer_,
      dispatch_time(DISPATCH_TIME_NOW, CRITICAL_TASKLETS_WAIT_TIME),
      0,
      0);

  rwsched_dispatch_resume(tasklet, tasks_ready_timer_);
}

void Instance::tasks_ready_timer_expire_cb(void* ctx)
{
  RW_ASSERT(ctx);
  static_cast<Instance*>(ctx)->tasks_ready_timer_expire();
}

void Instance::tasks_ready_timer_expire()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "ready timer expired, start mgmt server anyway");
  RW_MA_INST_LOG(this, InstanceError,
                 "Critical tasks not ready after 5 minutes, continuing");
  tasks_ready();
}

void Instance::stop_tasks_ready_timer()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "stop ready timer",
           RWMEMLOG_ARG_PRINTF_INTPTR("t=%" PRIdPTR, (intptr_t)tasks_ready_timer_));
  if (tasks_ready_timer_) {
    rwsched_tasklet_ptr_t tasklet = rwsched_tasklet();
    rwsched_dispatch_source_cancel(tasklet, tasks_ready_timer_);
    rwsched_dispatch_release(tasklet, tasks_ready_timer_);
  }
  tasks_ready_timer_ = nullptr;
}

void Instance::tasks_ready_cb(void* ctx)
{
  RW_ASSERT(ctx);
  auto self = static_cast<Instance*>(ctx);
  RW_MA_INST_LOG(self, InstanceCritInfo, "Critical tasklets are in running state.");

  self->tasks_ready();
}

void Instance::tasks_ready()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "tasks ready");
  stop_tasks_ready_timer();

  try_start_mgmt_instance();
}

void Instance::try_start_mgmt_instance_cb(void* ctx)
{
  RW_ASSERT(ctx);
  auto self = static_cast<Instance*>(ctx);
  self->try_start_mgmt_instance();
}

void Instance::try_start_mgmt_instance()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "try management start");
  RW_ASSERT (!mgmt_handler_->is_instance_ready());

  if (!registrations_propagated()) {
    auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1);
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "wait for registrations");

    rwsched_dispatch_after_f(rwsched_tasklet(),
                             when,
                             rwsched_dispatch_get_main_queue(rwsched()),
                             this,
                             try_start_mgmt_instance_cb);
    return;
  }

  rw_status_t rs = mgmt_handler_->create_mgmt_directory();
  if (rs != RW_STATUS_SUCCESS) {
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "create directory failed",
             RWMEMLOG_ARG_PRINTF_INTPTR("rs=%" PRIX64,(intptr_t)rs) );
    RW_MA_INST_LOG(this, InstanceError, "Directory creation failed for "
        "management system. Retrying.");

    auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1);
    rwsched_dispatch_after_f(rwsched_tasklet(),
                             when,
                             rwsched_dispatch_get_main_queue(rwsched()),
                             this,
                             try_start_mgmt_instance_cb);
    return;
  }

  mgmt_handler_->start_mgmt_instance();
}

rw_status_t Instance::wait_for_critical_tasklets()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "wait for critical tasklets");
  RW_ASSERT (!mgmt_handler_->is_instance_ready());

  rw_status_t ret = RW_STATUS_SUCCESS;
  // Wait for critical tasklets to come in Running state
  RW_ASSERT(dts_api());

  start_tasks_ready_timer();
  rwdts_api_config_ready_register( dts_api(),
                                   tasks_ready_cb,
                                   this);

  return ret;
}

void Instance::subscribe_for_vm_state_notification()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "subscribe vm notif");
  if (ha_mode_ == HA_MASTER) return;

  rwdts_api_t* apih = dts_api();
  rwha_api_vm_state_cb_t cb;
  cb.cb_fn = Instance::agent_ha_state_change_cb;
  cb.ud = static_cast<void*>(this);

  RW_MA_INST_LOG (this, InstanceDebug, "Agent subscribing for the vm_state updates");

  rwha_api_register_vm_state_notification(
        apih,
        RWVCS_TYPES_VM_STATE_MGMTSTANDBY,
        &cb);
}


void Instance::agent_ha_state_change_cb(
    void* ud, 
    vcs_vm_state prev_state, 
    vcs_vm_state new_state)
{
  RW_ASSERT (ud);
  RW_ASSERT (prev_state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY);
  auto self = static_cast<Instance*>(ud);

  RW_MA_INST_LOG (self, InstanceCritInfo, "Received VM state change notification");

  //ATTN: Master -> Standby transition is handled.
  //As of now, if master is to be made as standby,
  //then the agent itself has to be restarted.

  if (new_state == RWVCS_TYPES_VM_STATE_MGMTACTIVE
      && prev_state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY) {
    self->update_ha_mode(HA_MASTER);
    self->dts()->load_registrations(self->yang_model());
    self->mgmt_handler()->on_ha_state_change_event(STANDBY_TO_MASTER_TRANSITION);
  } else {
    std::string log_str;
    log_str = "Received unexpected VM state change notification: "
      + std::to_string(new_state);

    RW_MA_INST_LOG (self, InstanceError, log_str.c_str());
  }
}

static inline void recalculate_mean (uint32_t *mean,
                                     uint32_t old_count,
                                     uint32_t new_value)
{
  RW_ASSERT(mean);
  uint64_t sum = (*mean) * old_count;
  *mean = (sum + new_value)/(old_count + 1);
}


void Instance::update_stats(RwMgmtagt_SbReqType type,
                            const char *req,
                            RWPB_T_MSG(RwMgmtagt_SpecificStatistics_ProcessingTimes) *sbreq_stats)
{
  RW_ASSERT(type < RW_MGMTAGT_SB_REQ_TYPE_MAXIMUM);

  if (nullptr == statistics_[type].get()) {
    statistics_[type].reset(new OperationalStats());
    RWPB_F_MSG_INIT (RwMgmtagt_Statistics, &statistics_[type]->statistics);
    statistics_[type]->statistics.operation = type;
    statistics_[type]->statistics.has_operation = 1;
    gettimeofday (&statistics_[type]->start_time_, 0);
    statistics_[type]->statistics.has_processing_times = 1;
    statistics_[type]->statistics.has_request_count = 1;
    statistics_[type]->statistics.has_parsing_failed = 1;

  }

  OperationalStats *stats = statistics_[type].get();

  RW_ASSERT(stats->commands.size() <= RWUAGENT_MAX_CMD_STATS);

  if (RWUAGENT_MAX_CMD_STATS == stats->commands.size()) {
    stats->commands.pop_front();
  }
  CommandStat t;
  t.request = req;
  t.statistics = *sbreq_stats;

  stats->commands.push_back (t);
  stats->statistics.request_count++;
  // Update the instance level stats
  if (!sbreq_stats->has_transaction_start_time) {
    stats->statistics.parsing_failed++;
    return;
  }

  // the success count has to be the old success count for recalulating mean
  uint32_t success = stats->statistics.request_count - stats->statistics.parsing_failed - 1;

  RWPB_T_MSG(RwMgmtagt_Statistics_ProcessingTimes) *pt =
      &stats->statistics.processing_times;

  if (!success) {
    // set all the present flags
    pt->has_request_parse_time = 1;
    pt->has_transaction_start_time = 1;
    pt->has_dts_response_time = 1;
    pt->has_response_parse_time = 1;
  }

  recalculate_mean (&pt->request_parse_time, success, sbreq_stats->request_parse_time);
  recalculate_mean (&pt->transaction_start_time, success, sbreq_stats->transaction_start_time);
  recalculate_mean (&pt->dts_response_time, success, sbreq_stats->dts_response_time);
  recalculate_mean (&pt->response_parse_time, success, sbreq_stats->response_parse_time);
}

bool Instance::module_is_exported(std::string const & module_name)
{
  return exported_modules_.count(module_name) > 0;
}

std::unique_ptr<AsyncFileWriter> Instance::create_async_file_writer(std::string const & filename)
{
  return std::unique_ptr<AsyncFileWriter>(new AsyncFileWriter(filename,
                                                              get_queue(QueueType::LogFileWriting),
                                                              rwsched(),
                                                              rwsched_tasklet()));
}

void Instance::create_queue(QueueType const & type)
{
  dispatch_queue_attr_t queue_attribute = DISPATCH_QUEUE_SERIAL;
  std::string queue_name = "default name";

  switch(type) {
    case QueueType::DefaultConcurrent:
      queue_attribute = DISPATCH_QUEUE_CONCURRENT;
      queue_name = "agent-cc-queue";
      break;
    case QueueType::DefaultSerial:
      queue_attribute = DISPATCH_QUEUE_SERIAL;
      queue_name = "agent-serial-queue";
      break;
    case QueueType::LogFileWriting:
      queue_attribute = DISPATCH_QUEUE_SERIAL;
      queue_name = "agent-log-file-writing-queue";
      break;
    case QueueType::Main:
      RW_ASSERT_NOT_REACHED();
      break;
    case QueueType::SchemaLoading:
      queue_attribute = DISPATCH_QUEUE_SERIAL;
      queue_name = "schema-load-queue";
      break;
    case QueueType::XmlAgentEditConfig:
      queue_attribute = DISPATCH_QUEUE_SERIAL;
      queue_name = "xml-agent-queue";
      break;
  }

  rwsched_dispatch_queue_t new_queue = rwsched_dispatch_queue_create(
      rwsched_tasklet(),
      queue_name.c_str(),
      queue_attribute);
  
  queues_[type] = new_queue;
}

rwsched_dispatch_queue_t Instance::get_queue(QueueType const & type)
{
  if (type == QueueType::Main){
      // special case
      return rwsched_dispatch_get_main_queue(rwsched());
  }

  if (queues_.count(type) <= 0) {
    create_queue(type);
  }

  return queues_[type];
}

void Instance::release_queue(QueueType const & type)
{
  if (queues_.count(type) <= 0) {
    std::string const log_message = "Tried to release queue that doesn't exist";
    RW_MA_INST_LOG(this, InstanceError, log_message.c_str());
    return;
  }

  rwsched_dispatch_release(rwsched_tasklet(), queues_[type]);
  queues_.erase(type);
}

std::string Instance::lookup_notification_stream(
                    const std::string& node_name,
                    const std::string& node_ns)
{
  auto it = notification_stream_map_.find({node_name, node_ns});
  if (it == notification_stream_map_.end()) {
    return std::string();
  }

  return it->second;
}

