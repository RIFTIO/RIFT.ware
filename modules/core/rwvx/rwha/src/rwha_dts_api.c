#include "rwdts.h"
#include "rw-ha.pb-c.h"
#include <rwha_dts_api.h>

static rwdts_member_rsp_code_t
rwha_api_member_handle_mode_change_info(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* key,
    const ProtobufCMessage*  msg,
    uint32_t                 credits,
    void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "xact_info NULL in rwha_api_member_handle_mode_change_info");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  } 
  rwha_api_vm_state_info_t *vm_state_info = (rwha_api_vm_state_info_t *)xact_info->ud;
  RW_ASSERT_TYPE(vm_state_info, rwha_api_vm_state_info_t);
  RWPB_T_MSG(RwHa_data_ModeChangeInfo_Instances) *inp;

  RWPB_T_PATHSPEC(RwHa_data_ModeChangeInfo_Instances) *mode_ks = 
      (RWPB_T_PATHSPEC(RwHa_data_ModeChangeInfo_Instances)*) key;

  RW_ASSERT_MESSAGE((action != RWDTS_QUERY_INVALID), "Invalid action:%d", action);

  if (vm_state_info->rwvm_name 
      && (!mode_ks->dompath.path000.has_key00 
          || strcmp(vm_state_info->rwvm_name, mode_ks->dompath.path000.key00.rwvm_instance_name))) {
    return RWDTS_ACTION_NA;
  }
  if (!mode_ks->dompath.path001.has_key00 
      || strcmp(vm_state_info->instance_name, mode_ks->dompath.path001.key00.instance_name)) {
    return RWDTS_ACTION_NA;
  }

  inp = (RWPB_T_MSG(RwHa_data_ModeChangeInfo_Instances)*)msg;
  if (vm_state_info->vm_state != inp->vm_state) {
    if (vm_state_info->cb.cb_fn) {
      vm_state_info->cb.cb_fn (vm_state_info->cb.ud, vm_state_info->vm_state, inp->vm_state);
    }
    vm_state_info->vm_state = inp->vm_state;
  }

  return RWDTS_ACTION_OK;
}

/*
 * register vm state change notification
 */
void rwha_api_register_vm_state_notification(
    rwdts_api_t *apih,
    vcs_vm_state vm_state,
    rwha_api_vm_state_cb_t *cb)
{
  rwvcs_instance_ptr_t rwvcs = rwdts_api_get_vcs_inst(apih);
  if (!rwvcs) {
    return;
  }
  char *rwvm_name = rwvcs->identity.rwvm_name;
  char *instance_name = rwdts_api_get_instance_name(apih);

  RW_ASSERT(instance_name);

  rwha_api_vm_state_info_t *vm_state_info = RW_MALLOC0_TYPE(sizeof(rwha_api_vm_state_info_t), rwha_api_vm_state_info_t);
  RW_ASSERT_TYPE(vm_state_info, rwha_api_vm_state_info_t);
  vm_state_info->instance_name = strdup(instance_name);
  if(rwvm_name) {
    vm_state_info->rwvm_name = strdup(rwvm_name);
  }
  vm_state_info->vm_state = vm_state;
  if (cb) {
    vm_state_info->cb.cb_fn = cb->cb_fn;
    vm_state_info->cb.ud = cb->ud;
  }

  RWPB_T_PATHSPEC(RwHa_data_ModeChangeInfo_Instances) *mode_ks = NULL;
  rw_status_t status = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwHa_data_ModeChangeInfo_Instances),
                                                  NULL,
                                                  (rw_keyspec_path_t**)&mode_ks);
  RW_ASSERT(RW_STATUS_SUCCESS == status);
  mode_ks->has_dompath = TRUE;
  if (rwvm_name) {
    mode_ks->dompath.path000.has_key00 = 1;
    mode_ks->dompath.path000.key00.rwvm_instance_name = strdup(rwvm_name);
  }
  mode_ks->dompath.path001.has_key00 = 1;
  mode_ks->dompath.path001.key00.instance_name = strdup(instance_name);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)mode_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_registration_t regns[] = {
    { 
      .keyspec = (rw_keyspec_path_t*)mode_ks,
      .desc    = RWPB_G_MSG_PBCMD(RwHa_data_ModeChangeInfo_Instances),
      .flags   = RWDTS_FLAG_SUBSCRIBER /*|RWDTS_FLAG_CACHE*/,
      .callback = {
        .cb.prepare = rwha_api_member_handle_mode_change_info,
        .ud = vm_state_info
      }
    },
  };

  int i = 0;
  {
    rwdts_member_register(NULL, apih,
                          regns[i].keyspec,
                          &regns[i].callback,
                          regns[i].desc,
                          regns[i].flags,
                          NULL);
  }

  rw_keyspec_path_free((rw_keyspec_path_t*)mode_ks, NULL);
  RW_FREE(instance_name);

  return;
}

static void 
rwha_api_member_active_mgmt_info_invoke_cb(RWPB_T_MSG(RwHa_data_ActiveMgmtInfo)* active_mgmt_info_msgp,
                                               rwha_api_active_mgmt_info_t *active_mgmt_info)
{
  if (active_mgmt_info_msgp) {
    active_mgmt_info->active_mgmt_info_cb_fn(active_mgmt_info_msgp->mgmt_ip_address,
                                             active_mgmt_info_msgp->mgmt_vm_instance_name,
                                             active_mgmt_info->ud);
  }
}

static void
rwha_api_handle_active_mgmt_info_ready(rwdts_member_reg_handle_t regh,
                                           rw_status_t               rs,
                                           void*                     ctx)
{
  rwha_api_active_mgmt_info_t *active_mgmt_info= ((rwha_api_active_mgmt_info_t *)ctx);
  RW_ASSERT_TYPE(active_mgmt_info, rwha_api_active_mgmt_info_t);
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  RWPB_T_MSG(RwHa_data_ActiveMgmtInfo) *tmp = (RWPB_T_MSG(RwHa_data_ActiveMgmtInfo)*) rwdts_member_reg_handle_get_next_element(
      regh, cur, NULL, NULL);
  rwha_api_member_active_mgmt_info_invoke_cb(tmp, active_mgmt_info);

  return;
}

static rwdts_member_rsp_code_t
rwha_api_handle_active_mgmt_info_prepare(const rwdts_xact_info_t* xact_info,
                                             RWDtsQueryAction         action,
                                             const rw_keyspec_path_t* keyspec,
                                             const ProtobufCMessage*  msg,
                                             uint32_t                 credits,
                                             void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "xact_info NULL in rwha_api_handle_active_mgmt_info_prepare");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  } 

  rwha_api_active_mgmt_info_t *active_mgmt_info= ((rwha_api_active_mgmt_info_t *)xact_info->ud);
  RW_ASSERT_TYPE(active_mgmt_info, rwha_api_active_mgmt_info_t);

  RWPB_T_MSG(RwHa_data_ActiveMgmtInfo) *tmp = (RWPB_T_MSG(RwHa_data_ActiveMgmtInfo)*) msg;
  rwha_api_member_active_mgmt_info_invoke_cb(tmp, active_mgmt_info);

  return RWDTS_ACTION_OK;
}

rw_status_t
rwha_api_register_active_mgmt_info(rwdts_api_t *apih,
                                           rwha_api_active_mgmt_info_cb_fn active_mgmt_info_cb_fn,
                                           void *ud)
{
  rwha_api_active_mgmt_info_t * active_mgmt_info = RW_MALLOC0_TYPE(sizeof(rwha_api_active_mgmt_info_t), rwha_api_active_mgmt_info_t);
  RW_ASSERT_TYPE(active_mgmt_info, rwha_api_active_mgmt_info_t);
  active_mgmt_info->active_mgmt_info_cb_fn = active_mgmt_info_cb_fn;
  active_mgmt_info->ud = ud;

  rw_keyspec_path_t *active_mgmt_info_ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_ActiveMgmtInfo)->rw_keyspec_path_t,
                             NULL,
                             &active_mgmt_info_ks);
  RW_ASSERT_MESSAGE(active_mgmt_info_ks, "keyspec duplication failed");
  rw_keyspec_path_set_category (active_mgmt_info_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_registration_t regn = {
    .keyspec = (rw_keyspec_path_t*)active_mgmt_info_ks,
    .desc    = RWPB_G_MSG_PBCMD(RwHa_data_ActiveMgmtInfo),
    .flags   = RWDTS_FLAG_SUBSCRIBER| RWDTS_FLAG_CACHE,
    .callback = {
      .cb.reg_ready = rwha_api_handle_active_mgmt_info_ready,
      .cb.prepare = rwha_api_handle_active_mgmt_info_prepare,
      .ud = active_mgmt_info
    }
  };

  rwdts_member_register(NULL, apih,
                        regn.keyspec,
                        &regn.callback,
                        regn.desc,
                        regn.flags,
                        NULL);

  rw_keyspec_path_free(active_mgmt_info_ks, NULL);
  return RW_STATUS_SUCCESS;
}

static void
rwha_api_publish_uagent_state_data(rwdts_member_reg_handle_t regh,
                                   rw_keyspec_path_t         *keyspec,
                                   bool *ready)
{
  RWPB_T_MSG(RwHa_data_HaUagentState) ha_uagent_state_msg, *ha_uagent_state_msgp;
  ha_uagent_state_msgp = &ha_uagent_state_msg;
  RWPB_F_MSG_INIT(RwHa_data_HaUagentState, ha_uagent_state_msgp);
  RWPB_T_MSG(RwHa_data_HaUagentState_HaConfigState) ha_uagent_configstate_msg, *ha_uagent_configstate_msgp;
  ha_uagent_configstate_msgp = &ha_uagent_configstate_msg;
  RWPB_F_MSG_INIT(RwHa_data_HaUagentState_HaConfigState, ha_uagent_configstate_msgp);
  ha_uagent_configstate_msgp->has_ready = 1;
  ha_uagent_configstate_msgp->ready = (*ready);
  ha_uagent_state_msgp->ha_config_state = ha_uagent_configstate_msgp;

  rw_status_t rs = rwdts_member_reg_handle_update_element_keyspec(regh,
                                                          keyspec,
                                                          &ha_uagent_state_msgp->base,
                                                          RWDTS_XACT_FLAG_REPLACE,
                                                          NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
}

static void
rwha_api_publish_uagent_state_ready(rwdts_member_reg_handle_t regh,
                                        rw_status_t               rs,
                                        void*                     ctx)
{
  bool *ready = (bool *) ctx;
    
  rw_keyspec_path_t *ha_uagent_state_ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_HaUagentState)->rw_keyspec_path_t,
                             NULL,
                             &ha_uagent_state_ks);
  RW_ASSERT_MESSAGE(ha_uagent_state_ks, "keyspec duplication failed");
  rw_keyspec_path_set_category (ha_uagent_state_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwha_api_publish_uagent_state_data(regh, (rw_keyspec_path_t *)ha_uagent_state_ks, ready);

  rw_keyspec_path_free(ha_uagent_state_ks, NULL);

  return;
}

rw_status_t
rwha_api_publish_uagent_state(rwdts_api_t *apih,
                              bool        ready)
{
  rw_keyspec_path_t *ha_uagent_state_ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_HaUagentState)->rw_keyspec_path_t,
                             NULL,
                             &ha_uagent_state_ks);
  RW_ASSERT_MESSAGE(ha_uagent_state_ks, "keyspec duplication failed");
  rw_keyspec_path_set_category (ha_uagent_state_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

  bool *ready_ud = RW_MALLOC0(sizeof(bool));
  *ready_ud = ready;
  rwdts_member_reg_handle_t ha_uagent_state_regh = rwdts_api_get_uagent_state_regh(apih);
  if (!ha_uagent_state_regh) {
    rwdts_registration_t regn = {
      .keyspec = (rw_keyspec_path_t*)ha_uagent_state_ks,
      .desc    = RWPB_G_MSG_PBCMD(RwHa_data_HaUagentState),
      .flags   = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_CACHE|RWDTS_FLAG_NO_PREP_READ,
      .callback = {
        .cb.reg_ready = rwha_api_publish_uagent_state_ready,
        .ud = ready_ud
      }
    };
    ha_uagent_state_regh = rwdts_member_register(NULL, apih,
                                                 regn.keyspec,
                                                 &regn.callback,
                                                 regn.desc,
                                                 regn.flags,
                                                 NULL);
    rwdts_api_set_uagent_state_regh(apih, ha_uagent_state_regh);

  }
  else {
    rwha_api_publish_uagent_state_data(ha_uagent_state_regh, (rw_keyspec_path_t *)ha_uagent_state_ks, &ready);
  }
  rw_keyspec_path_free(ha_uagent_state_ks, NULL);
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwha_api_add_modeinfo_query_to_block(
    rwdts_xact_block_t *block,
    char *rwvm_name,
    char *instance_name,
    vcs_vm_state vm_state)
{
  NEW_DBG_PRINTS("Inform /%s/%s the state %d\n", rwvm_name, instance_name, vm_state);
  rw_keyspec_path_t *mode_kspath = NULL;
  rw_status_t status = rw_keyspec_path_create_dup(
      (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwHa_data_ModeChangeInfo_Instances),
      NULL,
      &mode_kspath);
  RW_ASSERT(mode_kspath);
  RWPB_T_PATHSPEC(RwHa_data_ModeChangeInfo_Instances) *mode_ks = 
      ((RWPB_T_PATHSPEC(RwHa_data_ModeChangeInfo_Instances)*)mode_kspath);
  

  mode_ks->has_dompath = TRUE;
  mode_ks->dompath.path000.has_key00 = 1;
  mode_ks->dompath.path000.key00.rwvm_instance_name = strdup(rwvm_name);
  mode_ks->dompath.path001.has_key00 = 1;
  mode_ks->dompath.path001.key00.instance_name = strdup(instance_name);

  RWPB_T_MSG(RwHa_data_ModeChangeInfo_Instances) *mode_msg
      = RW_MALLOC0(sizeof(RWPB_T_MSG(RwHa_data_ModeChangeInfo_Instances)));
  RWPB_F_MSG_INIT(RwHa_data_ModeChangeInfo_Instances, mode_msg);
  mode_msg->instance_name = strdup(instance_name);
  mode_msg->has_vm_state = true;
  mode_msg->vm_state = vm_state;

  status = rwdts_xact_block_add_query_ks(
      block,
      (rw_keyspec_path_t *)mode_ks,
      RWDTS_QUERY_UPDATE,
      RWDTS_XACT_FLAG_TRACE | RWDTS_XACT_FLAG_ADVISE,
      (unsigned long)instance_name,
      (ProtobufCMessage *)mode_msg);
  rw_keyspec_path_free(mode_kspath, NULL);
  protobuf_c_message_free_unpacked(NULL, &(mode_msg)->base);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  return status;
}

