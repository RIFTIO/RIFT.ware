/* STANDARD_RIFT_IO_COPYRIGHT */
/* * 
 * @file rwuagent_dynamic_schema.cpp
 * @brief Private micro-agent dynamic schema state machine implementation
 */

#include "rwdts.h"
#include "rwdts_api_gi.h"
#include "rwsched.h"

#include "rwuagent.hpp"
#include "rwuagent_dynamic_schema.hpp"

namespace rw_uagent {

rwdynschema_dynamic_schema_registration_t *AgentDynSchemaHelper::dyn_schema_reg_h_ = nullptr;
std::mutex AgentDynSchemaHelper::schema_lck_;


Module::Module(const std::string& module_name_,
               const std::string& fxs_filename_,
               const std::string& so_filename_,
               const std::string& yang_filename_,
               const std::string& metainfo_filename_,
               const std::string& revision_,
               bool exported_)
    : module_name(module_name_),
      fxs_filename(fxs_filename_),
      so_filename(so_filename_),
      yang_filename(yang_filename_),
      metainfo_filename(metainfo_filename_),
      revision(revision_),
      exported(exported_)
{
}

DynamicSchemaDriver::DynamicSchemaDriver(Instance * agent_instance, rwdts_api_t * dts_handle)
    : agent_instance_(agent_instance),
      dts_handle_(dts_handle)
{
  register_publish_modules();
  register_subscribe_config();
}

void DynamicSchemaDriver::stop()
{
  ScopeLock lock(pending_mutex_);
  keep_spinning_ = false;
}

/**
 * @pre must be called from config_state_change
 */
void DynamicSchemaDriver::initiate_module_load()
{
  ScopeLock lock(pending_mutex_);
  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(
      RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps));

  rwdts_api_query_ks(dts_handle_,
                     keyspec,
                     RWDTS_QUERY_READ,
                     RWDTS_XACT_FLAG_NONE,
                     PendingLoad::initiate_load,
                     reinterpret_cast<gpointer>(&pending_loads_.front()),
                     nullptr);
}

void DynamicSchemaDriver::register_publish_modules()
{
  RWPB_M_PATHSPEC_DECL_INIT(
      RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules,
      keyspec_state);
  rw_keyspec_path_t * keyspec = (rw_keyspec_path_t *) &keyspec_state;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = reinterpret_cast<void *>(this);

  pub_modules_reg_ = rwdts_member_register(
      NULL,
      dts_handle_,
      keyspec,
      &reg_cb,
      RWPB_G_MSG_PBCMD(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules),
      RWDTS_FLAG_PUBLISHER,
      NULL);    
}

void DynamicSchemaDriver::register_subscribe_config()
{
  RWPB_M_PATHSPEC_DECL_INIT(
      RwMgmtSchema_data_RwMgmtSchema,
      keyspec_state);
  rw_keyspec_path_t * keyspec = (rw_keyspec_path_t *) &keyspec_state;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = config_state_change;
  reg_cb.ud = reinterpret_cast<void *>(this);

  sub_config_reg_ = rwdts_member_register(
      NULL,
      dts_handle_,
      keyspec,
      &reg_cb,
      RWPB_G_MSG_PBCMD(RwMgmtSchema_data_RwMgmtSchema),
      RWDTS_FLAG_SUBSCRIBER,
      NULL);    
}

rwdts_member_rsp_code_t DynamicSchemaDriver::config_state_change(const rwdts_xact_info_t * xact_info,
                                                                 RWDtsQueryAction action,
                                                                 const rw_keyspec_path_t * keyspec,
                                                                 const ProtobufCMessage * msg,
                                                                 uint32_t credits,
                                                                 void * get_next_key)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t * xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  DynamicSchemaDriver * instance = reinterpret_cast<DynamicSchemaDriver *>(xact_info->ud);
  RW_ASSERT(instance);

  const ProtobufCMessage * my_msg = protobuf_c_message_duplicate(NULL, msg, msg->descriptor);

  RWPB_M_MSG_DECL_CAST(RwMgmtSchema_data_RwMgmtSchema, state, my_msg);

  const size_t numel_modules = state->n_modules;
  std::vector<Module> batch;
  
  for (size_t itr = 0; itr < numel_modules; ++itr) {
    ConfigModule * module = state->modules[itr];

    batch.emplace_back(
        std::string(module->name),
        std::string(module->module->fxs_filename),
        std::string(module->module->so_filename),
        std::string(module->module->yang_filename),
        std::string(module->module->metainfo_filename),
        std::string(module->revision),
        module->module->exported);

  }

  {
    ScopeLock lock(instance->pending_mutex_);
    instance->pending_loads_.emplace_front(instance->agent_instance_,
                                           batch,
                                           instance->dts_handle_,
                                           instance->pub_modules_reg_);
    instance->initiate_module_load();
  }

  return RWDTS_ACTION_OK;
}

void DynamicSchemaDriver::run(void * instance_pointer)
{
  DynamicSchemaDriver * instance = reinterpret_cast<DynamicSchemaDriver *>(instance_pointer);
  RW_ASSERT(instance);

  std::list<PendingLoad> & pending_loads = instance->pending_loads_;
  auto *agent_instance = instance->agent_instance_;

  // clean up completed loads
  if (!pending_loads.empty()) {
    ScopeLock lock(instance->pending_mutex_);

    pending_loads.remove_if([](const PendingLoad& l) {
        return l.is_complete();
      });
  }

  if (agent_instance->schema_state_.state == 
      RW_MGMT_SCHEMA_APPLICATION_STATE_ERROR) {
    // ATTN: It could have been due to some db lock held up
    // during mapi upgrade. Must retry in that case
    // Clearing up the pending list of modules in agent
    // ATTN: should it be clear or accumulate ?
    agent_instance->pending_schema_modules_.clear();
    agent_instance->update_dyn_state(RW_MGMT_SCHEMA_APPLICATION_STATE_READY);
  }

  if (instance->keep_spinning_) {
    // wait 10 seconds and try again
    rwsched_instance_ptr_t sched = instance->agent_instance_->rwsched();
    rwsched_tasklet_ptr_t tasklet = instance->agent_instance_->rwsched_tasklet();

    const auto ten_seconds = NSEC_PER_SEC * 10LL;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, ten_seconds);
    rwsched_dispatch_after_f(tasklet,
                             when,
                             rwsched_dispatch_get_main_queue(sched),
                             instance,
                             run);
  }
}

PendingLoad::PendingLoad(Instance * agent_instance,
                         std::vector<Module> batch,
                         rwdts_api_t * dts_handle,
                         rwdts_member_reg_handle_t pub_modules_reg)
    : agent_instance_(agent_instance),
      dts_handle_(dts_handle),
      pub_modules_reg_(pub_modules_reg),
      batch_(batch)
{
  // empty
}

void PendingLoad::retry_load(void * instance_pointer)
{
  PendingLoad * instance = reinterpret_cast<PendingLoad *>(instance_pointer);
  RW_ASSERT(instance);

  const rw_keyspec_path_t * keyspec;
  keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(
      RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps));

  rwdts_api_query_ks(instance->dts_handle_,
                     keyspec,
                     RWDTS_QUERY_READ,
                     RWDTS_XACT_FLAG_NONE,
                     PendingLoad::initiate_load,
                     reinterpret_cast<gpointer>(instance),
                     nullptr);
}

bool all_loaded(size_t application_type, rwdts_xact_t * xact)
{
  rwdts_query_result_t * result = rwdts_xact_query_result(xact, 0);

  bool advance = true;

  while (result) {
    const ListeningApps * listening_app =
        reinterpret_cast<const ListeningApps *>(rwdts_query_result_get_protobuf(result));

    if (listening_app->app_type == application_type) {
      advance = advance && (listening_app->state == RW_MGMT_SCHEMA_APPLICATION_STATE_READY);
    }

    result = rwdts_xact_query_result(xact, 0);
  }
 
  return advance;
}

void PendingLoad::update_batch_state(RwMgmtSchema__YangEnum__ModuleState__E new_state)                        
{
  for (auto module : batch_) {
    RWPB_M_MSG_DECL_INIT(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules, dynamic_module);
    RWPB_M_MSG_DECL_INIT(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules_Module, dynamic_module_contents);

    dynamic_module.name = RW_STRDUP(module.module_name.c_str());
    dynamic_module.revision = RW_STRDUP(module.revision.c_str());
    dynamic_module.module = &dynamic_module_contents;
    dynamic_module.state = new_state;
    dynamic_module.has_state = 1;
    
    dynamic_module_contents.fxs_filename      = RW_STRDUP(module.fxs_filename.c_str());
    dynamic_module_contents.so_filename       = RW_STRDUP(module.so_filename.c_str());
    dynamic_module_contents.yang_filename     = RW_STRDUP(module.yang_filename.c_str());
    dynamic_module_contents.metainfo_filename = RW_STRDUP(module.metainfo_filename.c_str());
    dynamic_module_contents.exported = module.exported;
    dynamic_module_contents.has_exported = true;

    std::string xpath = "D,/rw-mgmt-schema:rw-mgmt-schema-state/rw-mgmt-schema:dynamic-modules[name='"
                        + module.module_name
                        + "'][revision='"
                        + module.revision
                        +"']";

    rwdts_member_reg_handle_update_element_xpath(pub_modules_reg_,
                                                 xpath.c_str(),
                                                 &dynamic_module.base,
                                                 RWDTS_FLAG_NONE,
                                                 nullptr);
  }
}

void PendingLoad::update_state(rwdts_xact_t * xact,
                               rwdts_xact_status_t * xact_status,
                               void * instance_pointer)
{
  PendingLoad * instance = reinterpret_cast<PendingLoad *>(instance_pointer);
  RW_ASSERT(instance);

  bool advance_state = false;
  RwMgmtSchema__YangEnum__ModuleState__E next_state = RW_MGMT_SCHEMA_MODULE_STATE_CONFIGURED;
  State my_next_state = Error;

  switch (instance->current_state_) {
    case Loading_Apps:
      advance_state = all_loaded(RW_MGMT_SCHEMA_APPLICATION_TYPE_CLIENT, xact);
      my_next_state = Loading_Mgmt;
      next_state = RW_MGMT_SCHEMA_MODULE_STATE_LOADING_MGMT;
      break;
    case Loading_Mgmt:
      advance_state = all_loaded(RW_MGMT_SCHEMA_APPLICATION_TYPE_UAGENT, xact);
      my_next_state = Loading_Nb;
      next_state = RW_MGMT_SCHEMA_MODULE_STATE_LOADING_NB_INTERFACES;
      break;
    case Loading_Nb:
      advance_state = all_loaded(RW_MGMT_SCHEMA_APPLICATION_TYPE_NB_INTERFACE, xact);
      my_next_state = Done;
      break;
    case Done:
      advance_state = true;
      break;
    case Error:
      RW_ASSERT(false);
  }

  if (advance_state && instance->current_state_ == Done) {
    instance->complete_ = true;
    return;
  } else if (advance_state) {
    instance->current_state_ = my_next_state;
    instance->update_batch_state(next_state);
  }

  // try again
  rwsched_instance_ptr_t sched = instance->agent_instance_->rwsched();
  rwsched_tasklet_ptr_t tasklet = instance->agent_instance_->rwsched_tasklet();

  const auto two_seconds = NSEC_PER_SEC * 2LL;
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, two_seconds);

  rwsched_dispatch_after_f(tasklet,
                           when,
                           rwsched_dispatch_get_main_queue(sched),
                           instance,
                           drive_state_machine);
  

}

void PendingLoad::drive_state_machine(void * instance_pointer)
{
  PendingLoad * instance = reinterpret_cast<PendingLoad *>(instance_pointer);
  RW_ASSERT(instance);

  const rw_keyspec_path_t * keyspec;
  keyspec = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(
      RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps));

  rwdts_api_query_ks(instance->dts_handle_,
                     keyspec,
                     RWDTS_QUERY_READ,
                     RWDTS_XACT_FLAG_NONE,
                     PendingLoad::update_state,
                     reinterpret_cast<gpointer>(instance),
                     nullptr);
}

void PendingLoad::initiate_load(rwdts_xact_t * xact,
                                rwdts_xact_status_t * xact_status,
                                void * instance_pointer)
{
  PendingLoad * instance = reinterpret_cast<PendingLoad *>(instance_pointer);
  RW_ASSERT(instance);

  rwdts_query_result_t * result = rwdts_xact_query_result(xact, 0);

  bool all_apps_ready = true;

  while (result) {
    const ListeningApps * listening_app =
        reinterpret_cast<const ListeningApps *>(rwdts_query_result_get_protobuf(result));

    if ((listening_app->state == RW_MGMT_SCHEMA_APPLICATION_STATE_ERROR) |
        (listening_app->state == RW_MGMT_SCHEMA_APPLICATION_STATE_INITIALIZING)) {
      all_apps_ready = false;
      break;
    } else {
      result = rwdts_xact_query_result(xact, 0);
    }    
  }

  if (all_apps_ready) {
    // copy the config module into the list of operational modules

    rwdts_xact_t * create_xact = rwdts_api_xact_create(instance->dts_handle_,
                                                       RWDTS_XACT_FLAG_NONE,
                                                       nullptr,
                                                       nullptr);
    
    for (auto& module : instance->batch_) {
      DataModule dynamic_module;
      DataModuleContents dynamic_module_contents;

      RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules, &dynamic_module);
      RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules_Module, &dynamic_module_contents);

      dynamic_module.name = RW_STRDUP(module.module_name.c_str());
      dynamic_module.revision = RW_STRDUP(module.revision.c_str());
      dynamic_module.module = &dynamic_module_contents;
      dynamic_module.state = RW_MGMT_SCHEMA_MODULE_STATE_LOADING_APPS;
      dynamic_module.has_state = 1;
      
      dynamic_module_contents.fxs_filename      = RW_STRDUP(module.fxs_filename.c_str());
      dynamic_module_contents.so_filename       = RW_STRDUP(module.so_filename.c_str());
      dynamic_module_contents.yang_filename     = RW_STRDUP(module.yang_filename.c_str());
      dynamic_module_contents.metainfo_filename = RW_STRDUP(module.metainfo_filename.c_str());
      dynamic_module_contents.exported = module.exported;
      dynamic_module_contents.has_exported = true;

      RWPB_T_PATHENTRY(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules) ks =
          *(RWPB_G_PATHENTRY_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_DynamicModules));
      rw_keyspec_entry_t * kpe = reinterpret_cast<rw_keyspec_entry_t *>(&ks);

      ks.has_key00 = 1;
      ks.key00.name = RW_STRDUP(module.module_name.c_str());
      ks.has_key01 = 1;
      ks.key01.revision = RW_STRDUP(module.revision.c_str());
      
      rwdts_member_reg_handle_create_element(instance->pub_modules_reg_,
                                             kpe,
                                             &dynamic_module.base,
                                             create_xact);
    }

    rwdts_xact_commit(create_xact);
                      
    // wait 2 seconds and start driving the state machine
    rwsched_instance_ptr_t sched = instance->agent_instance_->rwsched();
    rwsched_tasklet_ptr_t tasklet = instance->agent_instance_->rwsched_tasklet();

    const auto two_seconds = NSEC_PER_SEC * 2LL;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, two_seconds);
    rwsched_dispatch_after_f(tasklet,
                             when,
                             rwsched_dispatch_get_main_queue(sched),
                             instance,
                             drive_state_machine);
    

  } else {
    // wait 2 seconds and try again
    rwsched_instance_ptr_t sched = instance->agent_instance_->rwsched();
    rwsched_tasklet_ptr_t tasklet = instance->agent_instance_->rwsched_tasklet();

    const auto two_seconds = NSEC_PER_SEC * 2LL;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, two_seconds);
    rwsched_dispatch_after_f(tasklet,
                             when,
                             rwsched_dispatch_get_main_queue(sched),
                             instance,
                             retry_load);
  }


}


rw_status_t
AgentDynSchemaHelper::register_for_dynamic_schema(Instance* agent_inst)
{
  if (AgentDynSchemaHelper::register_listening_app(agent_inst) != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }
  if (AgentDynSchemaHelper::register_schema_updates(agent_inst) != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }
  if (AgentDynSchemaHelper::register_schema_version_info(agent_inst) != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t 
AgentDynSchemaHelper::register_listening_app(Instance* agent_inst)
{
  auto lstn_app_ps = *RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps);
  auto *ks_path = reinterpret_cast<rw_keyspec_path_t*>(&lstn_app_ps);
  rw_status_t status = RW_STATUS_SUCCESS;

  rw_keyspec_path_set_category(ks_path, nullptr, RW_SCHEMA_CATEGORY_DATA);

  lstn_app_ps.dompath.path001.has_key00 = 1;
  lstn_app_ps.dompath.path001.key00.name = &agent_inst->instance_name_[0u];

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = AgentDynSchemaHelper::dynamic_schema_tasklet_state_cb;
  reg_cb.ud = reinterpret_cast<void *>(agent_inst);

  auto res = rwdts_member_register(nullptr,
                                   agent_inst->dts_api(),
                                   ks_path,
                                   &reg_cb,
                                   RWPB_G_MSG_PBCMD(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps),
                                   RWDTS_FLAG_PUBLISHER,
                                   nullptr);

  if (res == NULL) {
    return RW_STATUS_FAILURE;
  }
  return status;
}


rw_status_t
AgentDynSchemaHelper::register_schema_version_info(Instance* agent_inst)
{
  auto schema_ver_ps = *RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchema_SchemaVersion);
  auto *ks_path = reinterpret_cast<rw_keyspec_path_t*>(&schema_ver_ps);
  rw_status_t status = RW_STATUS_SUCCESS;

  rw_keyspec_path_set_category(ks_path, nullptr, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.cb.prepare = AgentDynSchemaHelper::dynamic_schema_version_cb;
  reg_cb.ud = reinterpret_cast<void *>(agent_inst);

  auto res = rwdts_member_register(nullptr,
                                   agent_inst->dts_api(),
                                   ks_path,
                                   &reg_cb,
                                   RWPB_G_MSG_PBCMD(RwMgmtSchema_data_RwMgmtSchema_SchemaVersion),
                                   RWDTS_FLAG_PUBLISHER,
                                   nullptr);
  if (res == NULL) {
    return RW_STATUS_FAILURE;
  }
  return status;
}


rw_status_t 
AgentDynSchemaHelper::register_schema_updates(Instance* agent_inst)
{
  dyn_schema_reg_h_ = rwdynschema_instance_register(agent_inst->dts_api(),
                                                    AgentDynSchemaHelper::dynamic_schema_reg_cb,
                                                    agent_inst->instance_name_.c_str(),
                                                    RWDYNSCHEMA_APP_TYPE_AGENT,
                                                    agent_inst,
                                                    nullptr);
  if (dyn_schema_reg_h_ == nullptr) {
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


rwdts_member_rsp_code_t 
AgentDynSchemaHelper::dynamic_schema_tasklet_state_cb(
    const rwdts_xact_info_t    *xact_info,
    RWDtsQueryAction            action,
    const rw_keyspec_path_t    *key,
    const ProtobufCMessage     *msg,
    uint32_t                    credits,
    void                       *getnext_ptr)
{
  RW_ASSERT(xact_info && key);

  auto *agent_inst = reinterpret_cast<Instance*>(xact_info->ud);
  RW_ASSERT(agent_inst);

  rwdts_member_rsp_code_t rsp_code = RWDTS_ACTION_OK;
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage *msgs[1];

  RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps) listening_app_state;
  RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps, &listening_app_state);

  RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps_ErrorMessage) error_msg;
  RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps_ErrorMessage, &error_msg);

  listening_app_state.name = &agent_inst->instance_name_[0];
  listening_app_state.has_state = 1;
  listening_app_state.state = agent_inst->schema_state_.state;
  listening_app_state.app_type = RW_MGMT_SCHEMA_APPLICATION_TYPE_UAGENT;
  listening_app_state.has_app_type = 1;
  error_msg.message = &agent_inst->schema_state_.err_string[0u];
  listening_app_state.error_message = &error_msg;

  auto lstn_app_ps = *RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchemaState_ListeningApps);
  lstn_app_ps.dompath.path001.has_key00 = 1;
  lstn_app_ps.dompath.path001.key00.name = &agent_inst->instance_name_[0];

  rsp.msgs = msgs;
  rsp.msgs[0] = reinterpret_cast<ProtobufCMessage*>(&listening_app_state);
  rsp.n_msgs = 1;
  rsp.ks = reinterpret_cast<rw_keyspec_path_t*>(&lstn_app_ps);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  if (rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp)
      != RW_STATUS_SUCCESS) {
    return  RWDTS_ACTION_NOT_OK;
  }

  return rsp_code;
}

rwdts_member_rsp_code_t
AgentDynSchemaHelper::dynamic_schema_version_cb(
    const rwdts_xact_info_t    *xact_info,
    RWDtsQueryAction            action,
    const rw_keyspec_path_t    *key,
    const ProtobufCMessage     *msg,
    uint32_t                    credits,
    void                       *getnext_ptr)
{
  RW_ASSERT(xact_info && key);

  auto *agent_inst = reinterpret_cast<Instance*>(xact_info->ud);
  RW_ASSERT(agent_inst);

  rwdts_member_rsp_code_t rsp_code = RWDTS_ACTION_OK;
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage *msgs[1];

  RWPB_T_MSG(RwMgmtSchema_data_RwMgmtSchema_SchemaVersion) schema_version;
  RWPB_F_MSG_INIT(RwMgmtSchema_data_RwMgmtSchema_SchemaVersion, &schema_version);

  schema_version.has_yang_schema_version = 0;
  //schema_version.yang_schema_version = agent_inst->upgrade_ctxt_.schema_version_;

  auto schema_ver_ps = *RWPB_G_PATHSPEC_VALUE(RwMgmtSchema_data_RwMgmtSchema_SchemaVersion);

  rsp.msgs = msgs;
  rsp.msgs[0] = reinterpret_cast<ProtobufCMessage*>(&schema_version);
  rsp.n_msgs = 1;
  rsp.ks = reinterpret_cast<rw_keyspec_path_t*>(&schema_ver_ps);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  if (rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp)
      != RW_STATUS_SUCCESS) {
    return  RWDTS_ACTION_NOT_OK;
  }

  return rsp_code;
}


void 
AgentDynSchemaHelper::dynamic_schema_reg_cb(
    void * app_instance,
    int numel,
    rwdynschema_module_t* modules)
{
  RW_ASSERT(modules);
  std::lock_guard<std::mutex> _(schema_lck_);
  auto *agent_inst = reinterpret_cast<Instance*>(app_instance);

  agent_inst->handle_dynamic_schema_update(numel,
                                           modules);

  return;
}

}

