
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_dts.cpp
 *
 * Management agent RW.DTS member API - basic DTS integration.
 */

#include "rwuagent.hpp"
#include "rwuagent_nb_req.hpp"
#include "rwuagent_sb_req.hpp"

using namespace rw_uagent;
using namespace rw_yang;


DtsMember::DtsMember(
  Instance* instance)
: instance_(instance),
  memlog_buf_(
    instance_->get_memlog_inst(),
    "DtsMember",
    reinterpret_cast<intptr_t>(this))
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "created");

  api_ = rwdts_api_new(
    instance_->rwtasklet(),
    instance_->ypbc_schema(),
    &DtsMember::state_change_cb,
    this,
    nullptr);
  RW_ASSERT(api_);
}

DtsMember::~DtsMember()
{
  for (auto reg : all_dts_regs_) {
    rwdts_member_deregister(reg);
  }
}

RWDtsXactFlag DtsMember::get_flags()
{
  auto flags = flags_;
  flags_ = RWDTS_XACT_FLAG_NONE;
  return flags;
}

bool DtsMember::is_ready() const
{
  return api_ && ready_;
}

void DtsMember::trace_next()
{
  flags_ = RWDTS_XACT_FLAG_TRACE;
}

void DtsMember::state_change_cb(
  rwdts_api_t* apih,
  rwdts_state_t state,
  void* ud)
{
  RW_ASSERT(ud);
  auto dts = static_cast<DtsMember*>(ud);
  dts->state_change(state);
}

void DtsMember::state_change(
  rwdts_state_t state )
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "state change",
    RWMEMLOG_ARG_PRINTF_INTPTR("s=%" PRIu64, state) );

  switch (state)
  {
    case RW_DTS_STATE_INIT:
      dts_state_init();
      rwdts_api_set_state(api_, RW_DTS_STATE_REGN_COMPLETE);
      break;
    case RW_DTS_STATE_REGN_COMPLETE:
      break;
    case RW_DTS_STATE_CONFIG:
      dts_state_config();
      rwdts_api_set_state(api_, RW_DTS_STATE_RUN);
      break;
    case RW_DTS_STATE_RUN:
      // set the dynamic schema state to ready
      instance_->update_dyn_state(RW_MGMT_SCHEMA_APPLICATION_STATE_READY);
      break;
    default:
      RWMEMLOG(memlog_buf_, RWMEMLOG_ALWAYS, "bad state change!" );
      RW_MA_INST_LOG( instance_, InstanceError, "Invalid DTS state" );
      break;
  }
}

void DtsMember::register_rpc()
{
  /*
   * Setup command handler.
   */
  RWPB_M_PATHSPEC_DECL_INIT(RwMgmtagt_input_MgmtAgent, keyspec_rpc);
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&keyspec_rpc;
  rw_keyspec_path_set_category(keyspec, nullptr, RW_SCHEMA_CATEGORY_RPC_INPUT);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = &DtsMember::rpc_cb;
  reg_cb.ud = (void*)this;

  auto rpc_reg = rwdts_member_register(
                              nullptr,
                              api_,
                              keyspec,
                              &reg_cb,
                              RWPB_G_MSG_PBCMD(RwMgmtagt_input_MgmtAgent),
                              RWDTS_FLAG_PUBLISHER,
                              nullptr);
  RW_ASSERT(rpc_reg);
  all_dts_regs_.push_back(rpc_reg);

  RWPB_M_PATHSPEC_DECL_INIT(RwMgmtagtDts_input_MgmtAgentDts, keyspec_rpc_dts);
  keyspec = (rw_keyspec_path_t*)&keyspec_rpc_dts;
  rw_keyspec_path_set_category(keyspec, nullptr, RW_SCHEMA_CATEGORY_RPC_INPUT);

  rwdts_member_event_cb_t dts_reg_cb;
  RW_ZERO_VARIABLE(&dts_reg_cb);
  dts_reg_cb.cb.prepare = &DtsMember::rpc_cb;
  dts_reg_cb.ud = (void*)this;

  rpc_reg = rwdts_member_register(
                              nullptr,
                              api_,
                              keyspec,
                              &reg_cb,
                              RWPB_G_MSG_PBCMD(RwMgmtagtDts_input_MgmtAgentDts),
                              RWDTS_FLAG_PUBLISHER,
                              nullptr);
  RW_ASSERT(rpc_reg);
  all_dts_regs_.push_back(rpc_reg);
}


void DtsMember::dts_state_init()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "dts state init");
  register_rpc();
  ready_ = true;
}

void DtsMember::load_registrations(YangModel *model)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "load registrations");

  model->app_data_get_token(rwdynschema_app_data_namespace,
                            rwdynschema_dts_registrations_extension,
                            &dts_registrations_token_);

  YangNode *root = model->get_root_node();
  RW_ASSERT(root);
  YangNode *first_level = root->get_first_child();
  if (!first_level) {
    return;
  }

  while (nullptr != first_level) {
    // ATTN: So, who handles top-level leafy things?
    if (first_level->is_leafy() ||
        !(first_level->is_config()
          || first_level->is_notification()))
    {
      first_level = first_level->get_next_sibling();
      continue;
    }

    // ATTN: These seem like bugs. RPC is top level
    RW_ASSERT(!first_level->is_rpc());

    const std::string module_name{first_level->get_module()->get_name()};



    // if the app_data exists than the node has been registered already
    bool const already_registered = first_level->app_data_is_cached(
        &dts_registrations_token_);

    if (already_registered
        || (!instance_->module_is_exported(module_name))) {
      first_level = first_level->get_next_sibling();
      continue;
    }

    const rw_yang_pb_msgdesc_t * ypbc = first_level->get_ypbc_msgdesc();
    RW_ASSERT(ypbc);

    rw_keyspec_path_t *ks = nullptr;
    rw_status_t rs = rw_keyspec_path_create_dup(
      ypbc->schema_path_value, nullptr, &ks);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
    RW_ASSERT(ks);

    rwdts_member_reg_handle_t reg;
    rwdts_member_event_cb_t reg_cb = {};
    reg_cb.ud = this;
    reg_cb.cb.reg_ready = schema_registration_ready_cb;
    uint32_t flag;

    if (first_level->is_config()) {
      rw_keyspec_path_set_category (ks, nullptr, RW_SCHEMA_CATEGORY_CONFIG);
      reg_cb.cb.prepare = &DtsMember::get_config_cb;
      flag = RWDTS_FLAG_PUBLISHER;

    } else if(first_level->is_notification()) {
      rw_keyspec_path_set_category (ks, nullptr, RW_SCHEMA_CATEGORY_NOTIFICATION);
      reg_cb.cb.prepare = &DtsMember::notification_recv_cb;
      flag = RWDTS_FLAG_SUBSCRIBER;

    } else {
      RW_ASSERT_NOT_REACHED();
    }

    reg = rwdts_member_register(
                    nullptr,
                    api_,
                    ks,
                    &reg_cb,
                    ypbc->pbc_mdesc,
                    flag,
                    nullptr);

    all_dts_regs_.push_back(reg);

    instance_->exported_modules_app_data_.emplace_back(module_name, reg);

    first_level->app_data_set_and_keep_ownership(&dts_registrations_token_,
                                                 &instance_->exported_modules_app_data_.back());

    first_level = first_level->get_next_sibling();
  }
}

void DtsMember::schema_registration_ready_cb(rwdts_member_reg_handle_t reg,
                                            rw_status_t reg_state,
                                            void * user_data)
{
  auto self = static_cast<DtsMember*>(user_data);
  RW_ASSERT(self);

  self->instance_->schema_registration_ready(reg);
  RWMEMLOG(self->memlog_buf_, RWMEMLOG_MEM2, "register config ready");

  RWPB_M_PATHSPEC_DECL_INIT (
      RwMgmtagt_data_Uagent_State_ConfigState, ks);

  auto* keyspec = reinterpret_cast<rw_keyspec_path_t*>(&ks);
  rw_keyspec_path_set_category(keyspec, nullptr, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE (&reg_cb);
  reg_cb.ud = static_cast<void*>(self);
  reg_cb.cb.prepare = &DtsMember::config_state_prepare_cb;

  auto new_reg = rwdts_member_register(
                       nullptr,
                       self->api_,
                       keyspec,
                       &reg_cb,
                       RWPB_G_MSG_PBCMD (RwMgmtagt_data_Uagent_State_ConfigState),
                       RWDTS_FLAG_PUBLISHER,
                       nullptr);

  RW_ASSERT(new_reg);
  self->all_dts_regs_.push_back(new_reg);
}

void DtsMember::retry_config_state_cb(void* ctx)
{
  RW_ASSERT (ctx);
  auto self = static_cast<DtsMember*>(ctx);
  self->publish_config_state();
}

void DtsMember::publish_config_state(bool state)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "publish config state");

  auto keyspec_entry = (*RWPB_G_PATHSPEC_VALUE(RwMgmtagt_data_Uagent_State_ConfigState));

  auto* keyspec = reinterpret_cast<rw_keyspec_path_t*>(&keyspec_entry);
  RW_ASSERT (keyspec);

  RWPB_T_MSG(RwMgmtagt_data_Uagent_State_ConfigState) config_state;
  RWPB_F_MSG_INIT(RwMgmtagt_data_Uagent_State_ConfigState, &config_state);

  config_state.has_ready = true;
  config_state.ready     = state;

  rwdts_xact_t* xact = rwdts_api_query_ks(
                                  api_,
                                  keyspec,
                                  RWDTS_QUERY_CREATE,
                                  RWDTS_XACT_FLAG_ADVISE,
                                  config_state_cb,
                                  this,
                                  &(config_state.base));

  RW_ASSERT (xact);
  return;
}

void DtsMember::config_state_cb(rwdts_xact_t* xact,
                                rwdts_xact_status_t* xact_status,
                                void* ud)
{
  auto self = static_cast<DtsMember*>(ud);
  RW_ASSERT (self);
  RWMEMLOG(self->memlog_buf_, RWMEMLOG_MEM2, "config state cb",
      RWMEMLOG_ARG_PRINTF_INTPTR("state=%" PRIdPTR,
        (intptr_t)self->instance_->mgmt_handler()->is_ready_for_nb_clients()));

  RW_ASSERT (xact);
  RW_ASSERT (xact_status);

  switch (xact_status->status) {
    case RWDTS_XACT_FAILURE:
    case RWDTS_XACT_ABORTED:
    {
      rw_status_t rs;
      char *err_str = nullptr;
      char *xpath = nullptr;

      if (RW_STATUS_SUCCESS ==
            rwdts_xact_get_error_heuristic(xact, 0, &xpath, &rs, &err_str))
      {
        RW_MA_INST_LOG (self->instance_, InstanceError, "Config state publish failed.");
        if (err_str) {
          RW_MA_INST_LOG (self->instance_, InstanceError, err_str);
        }
      }
      auto when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1);
      rwsched_dispatch_after_f(self->instance_->rwsched_tasklet(),
                               when,
                               rwsched_dispatch_get_main_queue(self->instance_->rwsched()),
                               self,
                               retry_config_state_cb);
    }
      break;
    default:
      RW_MA_INST_LOG (self->instance_, InstanceCritInfo, "Published management agent"
          " config state successfully");
      break;
  };
}


rwdts_member_rsp_code_t DtsMember::config_state_prepare_cb(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks_in,
    const ProtobufCMessage* msg,
    uint32_t credits,
    void* getnext_ptr )
{
  (void)credits;
  (void)getnext_ptr;

  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  auto dts_mgr = static_cast<DtsMember*>( xact_info->ud );
  RW_ASSERT (dts_mgr);
  return dts_mgr->config_state_respond(
       xact_info->xact, xact_info->queryh, (rw_keyspec_path_t*)ks_in);
}


rwdts_member_rsp_code_t DtsMember::config_state_respond(
    rwdts_xact_t* xact,
    rwdts_query_handle_t qhdl,
    const rw_keyspec_path_t* ks)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "config state respond");
  rwdts_member_query_rsp_t rsp = {};

  RWPB_M_PATHSPEC_DECL_INIT (
      RwMgmtagt_data_Uagent_State_ConfigState, kspec);

  ProtobufCMessage *resp_msg[1];
  auto* keyspec = reinterpret_cast<rw_keyspec_path_t*>(&kspec);
  RWPB_T_MSG(RwMgmtagt_data_Uagent_State_ConfigState) config_state;
  RWPB_F_MSG_INIT(RwMgmtagt_data_Uagent_State_ConfigState, &config_state);

  config_state.has_ready = true;
  config_state.ready     = instance_->mgmt_handler()->is_ready_for_nb_clients();

  resp_msg[0] = &(config_state.base);

  rsp.n_msgs = 1;
  rsp.msgs = resp_msg;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;
  rsp.ks = keyspec;

  rwdts_member_send_response (xact, qhdl, &rsp);
  return RWDTS_ACTION_OK;
}


void DtsMember::dts_state_config()
{
  // ATTN: Recovery, reconciliation code goes here ..
}


rwdts_member_rsp_code_t DtsMember::notification_recv_cb(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t * keyspec,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void * get_next_key)
{
  RW_ASSERT(xact_info);
  RW_ASSERT(xact_info->ud);

  auto self = static_cast<DtsMember*>(xact_info->ud);

  return self->instance_->mgmt_handler()->handle_notification(msg);
}

rwdts_member_rsp_code_t DtsMember::get_config_cb(
  const rwdts_xact_info_t* xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg,
  uint32_t credits,
  void* getnext_ptr )
{
  RW_ASSERT(xact_info);
  RW_ASSERT(xact_info->ud);
  DtsMember *dts = static_cast<DtsMember*>( xact_info->ud );

  return dts->get_config(
    xact_info->xact, xact_info->queryh, (rw_keyspec_path_t*)ks );
}


rwdts_member_rsp_code_t DtsMember::get_config(
  rwdts_xact_t* xact,
  rwdts_query_handle_t qhdl,
  const rw_keyspec_path_t* ks )
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "send config data");

  char ks_str[256] = "";
  rw_keyspec_path_get_print_buffer(
    ks, NULL, instance_->ypbc_schema(), ks_str, sizeof(ks_str));
  std::string log_str = "KeySpec: ";
  log_str += ks_str;

  // From the DOM, find if there is anything at ks.
  std::list<XMLNode*> found;
  XMLNode *root = instance_->dom()->get_root_node();
  RW_ASSERT(root);
  rw_yang_netconf_op_status_t ncrs = root->find (ks, found);

  if ((RW_YANG_NETCONF_OP_STATUS_OK != ncrs) || (found.empty())) {
    log_str += " not found";
    RW_MA_INST_LOG (instance_, InstanceDebug, log_str.c_str());
    RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
      RWMEMLOG_ARG_CONST_STRING("failed to find keyspec"),
      RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES,ks_str),
      RWMEMLOG_ARG_PRINTF_INTPTR("ncrs=0x%" PRIXPTR, (intptr_t)ncrs),
      RWMEMLOG_ARG_PRINTF_INTPTR("found=%" PRIuPTR, (intptr_t)found.size()) );
    return RWDTS_ACTION_OK;
  }

  log_str += " found " + std::to_string(found.size()) + " entries";
  RW_MA_INST_LOG (instance_, InstanceDebug, log_str.c_str());

  // The simpler way to send responses to DTS is one at a time.
  ProtobufCMessage *resp_msg[1];
  rwdts_member_query_rsp_t rsp = {};
  rsp.n_msgs = 1;
  rsp.msgs = resp_msg;
  rsp.evtrsp = RWDTS_EVTRSP_ASYNC;

  size_t i = 0;
  size_t total = found.size();

  for (auto it = found.begin(); it != found.end(); it++) {
    rw_keyspec_path_t*  spec_key = nullptr;

    ncrs = (*it)->to_keyspec (&spec_key);
    if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
      log_str = (*it)->to_string();
      RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
        RWMEMLOG_ARG_CONST_STRING("failed to create keyspec"),
        RWMEMLOG_ARG_PRINTF_INTPTR("ncrs=0x%" PRIXPTR, (intptr_t)ncrs),
        RWMEMLOG_ARG_STRCPY_MAX(log_str.c_str(), RWMEMLOG_ARG_STRCPY_MAX_LIMIT) );
      continue;
    }

    ks_str[0] = 0;
    rw_keyspec_path_get_print_buffer(
      spec_key, NULL, instance_->ypbc_schema(), ks_str, sizeof(ks_str));
    rsp.ks = spec_key;
    ProtobufCMessage *message;
    ncrs = (*it)->to_pbcm (&message);

    if (ncrs != RW_YANG_NETCONF_OP_STATUS_OK) {
      log_str = (*it)->to_string();
      RW_MA_INST_LOG (instance_, InstanceError, "Protobuf conversion failed");
      RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
        RWMEMLOG_ARG_CONST_STRING("failed to convert XMLNode to PBCM"),
        RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES,ks_str),
        RWMEMLOG_ARG_PRINTF_INTPTR("ncrs=0x%" PRIXPTR, (intptr_t)ncrs),
        RWMEMLOG_ARG_STRCPY_MAX(log_str.c_str(), RWMEMLOG_ARG_STRCPY_MAX_LIMIT) );

      rw_keyspec_path_free (spec_key, nullptr);
      return RWDTS_ACTION_NOT_OK;
    }

    RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
      RWMEMLOG_ARG_CONST_STRING("found"),
      RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES,ks_str) );

    resp_msg[0] = message;
    i++;
    if (i == total) {
      rsp.evtrsp = RWDTS_EVTRSP_ACK;
    }

    rwdts_member_send_response (xact, qhdl, &rsp);
    rw_keyspec_path_free (spec_key, NULL );
    protobuf_c_message_free_unpacked (nullptr, message);
  }
  return RWDTS_ACTION_OK;
}

rwdts_member_rsp_code_t DtsMember::rpc_cb(
  const rwdts_xact_info_t* xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* ks_in,
  const ProtobufCMessage* msg,
  uint32_t credits,
  void* getnext_ptr )
{
  (void)credits;
  (void)getnext_ptr;

  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  auto dts_mgr = static_cast<DtsMember*>( xact_info->ud );
  RW_ASSERT (dts_mgr);
  return dts_mgr->rpc( xact_info, action, ks_in, msg );
}

rwdts_member_rsp_code_t DtsMember::rpc(
  const rwdts_xact_info_t* xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* ks_in,
  const ProtobufCMessage* msg )
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "receive rpc");
  auto nbreq = new NbReqDts( instance_, xact_info, action, ks_in, msg );
  auto ss = nbreq->execute();

  if (StartStatus::Async == ss) {
    return RWDTS_ACTION_ASYNC;
  }
  return RWDTS_ACTION_OK;
}

