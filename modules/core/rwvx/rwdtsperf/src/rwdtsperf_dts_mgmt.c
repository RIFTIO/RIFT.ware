
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



#include "rwdtsperf.h"
#include "rwdtsperf_dts_mgmt.h"

static void* rwdtsperf_dts_mgmt_xact_init(rwdts_appconf_t *ac,
                                          rwdts_xact_t *xact,
                                          void *ud)
{
  //rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t)ud;

  rwdtsperf_config_scratch_t *scratch;
  
  scratch = RW_MALLOC0_TYPE(sizeof(*scratch), rwdtsperf_config_scratch_t);
  
  if (!scratch){
    return NULL;
  }
  return scratch;
}

static void rwdtsperf_dts_mgmt_xact_deinit(rwdts_appconf_t *ac,
                                           rwdts_xact_t *xact, 
                                           void *ud,
                                           void *scratch_in)
{
  rwdtsperf_config_scratch_t *scratch = (rwdtsperf_config_scratch_t *)scratch_in;
  int i = 0;
  
  for (; i < scratch->count; i++) {
    protobuf_c_message_free_unpacked(NULL, scratch->config[i]);
    scratch->config[i] = NULL;
  }
  RW_ASSERT(scratch);
  RW_FREE_TYPE(scratch, rwdtsperf_config_scratch_t);
}

static void rwdtsperf_dts_mgmt_config_validate(rwdts_api_t *apih,
                                               rwdts_appconf_t *ac,
                                               rwdts_xact_t *xact, 
                                               void *ctx, 
                                               void *scratch_in)
{
  //Any Validation !!
  return;
}

static __inline__ rw_status_t
rwdtsperf_apply_subscriber_config(rwdtsperf_instance_ptr_t instance,
                                  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_SubscriberConfig) *subsc_cfg)
{
  rw_status_t status = RW_STATUS_FAILURE;
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_SubscriberConfig_RspFlavor) **rsp_flavor_in;
  rsp_flavor_ptr_t rsp_flavor = NULL;
  int index;

  rsp_flavor_in = subsc_cfg->rsp_flavor;
  for (index = 0; index < subsc_cfg->n_rsp_flavor; index ++) {
    status = RW_SKLIST_LOOKUP_BY_KEY (&(instance->config.subsc_cfg.rsp_flavor_list),
                                      rsp_flavor_in[index]->rsp_flavor_name,
                                      &rsp_flavor);
    if (!rsp_flavor) {
      rsp_flavor = RW_MALLOC0_TYPE(sizeof(*rsp_flavor), rsp_flavor_t);
      RW_ASSERT (rsp_flavor);
      strncpy (rsp_flavor->rsp_flavor_name, rsp_flavor_in[index]->rsp_flavor_name,
               sizeof(rsp_flavor->rsp_flavor_name));
      status = RW_SKLIST_INSERT(&(instance->config.subsc_cfg.rsp_flavor_list), rsp_flavor);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
      {
        rwdts_registration_t regn = {};
  
  
        RWDTSPERF_PREPARE_KEYSPEC(rsp_flavor->rsp_flavor_name);
        regn.keyspec = (rw_keyspec_path_t *)&pathspec;
        rw_keyspec_path_set_category (regn.keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

        regn.desc = RWPB_G_MSG_PBCMD(RwDtsperf_data_XactMsg_Content);
        regn.flags = RWDTS_FLAG_SUBSCRIBER;
        if (rsp_flavor_in[index]->rsp_cache) {
          regn.flags |= RWDTS_FLAG_CACHE;
        }
        regn.callback.cb.prepare = rwdtsperf_handle_xact;
        regn.callback.ud = instance;
        rwdts_member_register(NULL, instance->dts_h,regn.keyspec, &regn.callback,
                              regn.desc, regn.flags, NULL);

        regn.flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED;
        if (rsp_flavor_in[index]->rsp_cache) {
          regn.flags |= RWDTS_FLAG_CACHE;
        }
        rwdts_member_register(NULL, instance->dts_h,regn.keyspec, &regn.callback,
                              regn.desc, regn.flags, NULL);

        RWDTSPERF_FREE_KEYSPEC;
      }
    }

    IS_PRESENT_SET_ENUM(rsp_flavor, (rsp_flavor_in[index]), rsp_character);
    IS_PRESENT_SET_ENUM(rsp_flavor, (rsp_flavor_in[index]), rsp_invoke_xact);
    IS_PRESENT_SET(rsp_flavor, (rsp_flavor_in[index]), num_rsp);
    IS_PRESENT_SET(rsp_flavor, (rsp_flavor_in[index]), rsp_delay_interval);
    IS_PRESENT_SET(rsp_flavor, (rsp_flavor_in[index]), payload_len);
    IS_PRESENT_SET_ENUM(rsp_flavor, (rsp_flavor_in[index]), rsp_type);
    if (rsp_flavor_in[index]->next_xact_name) {
      strncpy (rsp_flavor->next_xact_name, rsp_flavor_in[index]->next_xact_name, sizeof(rsp_flavor->next_xact_name));
    }
    if (rsp_flavor_in[index]->payload_pattern) {
      strncpy (rsp_flavor->payload_pattern, rsp_flavor_in[index]->payload_pattern, sizeof(rsp_flavor->payload_pattern));
    }
  }
  return status;
}

static __inline__ rw_status_t
rwdtsperf_apply_xact_config(rwdtsperf_instance_ptr_t instance,
                            RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_XactConfig) *xact_cfg)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_XactConfig_Xact) **xact;
  xact_detail_ptr_t xact_detail = NULL;
  int index;

  if (xact_cfg->xact_repeat) {
    if (xact_cfg->xact_repeat->delay_and_repeat) {
      instance->config.xact_cfg.ordering = RWDTSPERF_PERIODIC_INVOKE;
      IS_PRESENT_SET((&(instance->config.xact_cfg)), xact_cfg->xact_repeat->delay_and_repeat, delay_between_xacts);
    }
    if (xact_cfg->xact_repeat->outstanding_and_repeat) {
      instance->config.xact_cfg.ordering = RWDTSPERF_OUTSTANDING;
      IS_PRESENT_SET((&(instance->config.xact_cfg)), xact_cfg->xact_repeat->outstanding_and_repeat, num_xact_outstanding);
      IS_PRESENT_SET((&(instance->config.xact_cfg)), xact_cfg->xact_repeat->outstanding_and_repeat, xact_max_with_outstanding);
    }
    if (xact_cfg->xact_repeat->run_till_end) {
      instance->config.xact_cfg.ordering = RWDTSPERF_SEQUENTIAL;
    }
  }

  xact = xact_cfg->xact;
  for (index = 0; index < xact_cfg->n_xact; index ++) {
    status = RW_SKLIST_LOOKUP_BY_KEY (&(instance->config.xact_cfg.xact_detail_list),
                                      xact[index]->xact_name,
                                      &xact_detail);
    if (!xact_detail) {
      xact_detail = RW_MALLOC0_TYPE(sizeof(*xact_detail), xact_detail_t);
      RW_ASSERT (xact_detail);
      strncpy (xact_detail->xact_name, xact[index]->xact_name, 
               sizeof(xact_detail->xact_name));
      status = RW_SKLIST_INSERT(&(instance->config.xact_cfg.xact_detail_list), xact_detail);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
    } 
    
    RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_XactConfig_Xact_XactDetail) *xact_detail_in;

    xact_detail_in = xact[index]->xact_detail;
    IS_PRESENT_SET_ENUM(xact_detail, xact_detail_in, xact_operation);
    IS_PRESENT_SET_ENUM(xact_detail, xact_detail_in, xact_type);
    IS_PRESENT_SET(xact_detail, xact_detail_in, in_xact_repeat_count);
    IS_PRESENT_SET(xact_detail, xact_detail_in, in_xact_repeat_delay);
    IS_PRESENT_SET_ENUM(xact_detail, xact_detail_in, in_xact_repeat_character);
    IS_PRESENT_SET(xact_detail, xact_detail_in, xact_repeat_count);
    IS_PRESENT_SET(xact_detail, xact_detail_in, xact_delay_interval);
    IS_PRESENT_SET(xact_detail, xact_detail_in, payload_len);
    if (xact_detail_in->xact_rsp_flavor) {
      xact_detail->xact_rsp_flavor = strdup(xact_detail_in->xact_rsp_flavor);
    }
    if (xact_detail_in->payload_pattern) {
      strncpy (xact_detail->payload_pattern, xact_detail_in->payload_pattern, sizeof(xact_detail->payload_pattern));
    }
    xact_detail->action = (xact_detail->xact_operation == RWDTSPERF_CREATE)? RWDTS_QUERY_CREATE:
                            ((xact_detail->xact_operation == RWDTSPERF_UPDATE)? RWDTS_QUERY_UPDATE:
                            RWDTS_QUERY_READ);
    xact_detail->corr_id = (unsigned long)(xact_detail);
    if (xact_detail_in->xact_stream_flag) {
      xact_detail->flags |= RWDTS_XACT_FLAG_STREAM;
    }
    if (xact_detail_in->xact_blockmerge_flag) {
      xact_detail->flags |= RWDTS_XACT_FLAG_BLOCK_MERGE;
    }
  }
  return status;
}

static __inline__ rw_status_t
rwdtsperf_apply_config(rwdtsperf_instance_ptr_t instance,
                       RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig) *config)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_SubscriberConfig) *subsc_cfg = NULL;
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig_ConfigAll_XactConfig) *xact_cfg = NULL;

  if (config->config_task_instance &&
      config->config_task_instance->instance_id[0] &&
      (config->config_task_instance->instance_id[0]->task_instance_id == instance->rwtasklet_info->identity.rwtasklet_instance_id) &&
      config->config_task_instance->instance_id[0]->instance_config) {
    xact_cfg = config->config_task_instance->instance_id[0]->instance_config->xact_config;
    subsc_cfg = config->config_task_instance->instance_id[0]->instance_config->subscriber_config;
  }
  else if (config->config_all) {
    xact_cfg = config->config_all->xact_config;
    subsc_cfg = config->config_all->subscriber_config;
  }

  if (xact_cfg) {
    status = rwdtsperf_apply_xact_config (instance, xact_cfg);
  }

  if (subsc_cfg) {
    status = rwdtsperf_apply_subscriber_config (instance, subsc_cfg);
  }
  return status;
}

static void 
rwdtsperf_dts_mgmt_config_apply(rwdts_api_t *apih,
                                rwdts_appconf_t *ac,
                                rwdts_xact_t *xact,
                                rwdts_appconf_action_t action,
                                void *ctx, 
                                void *scratch_in)
{
  rwdtsperf_instance_ptr_t instance;
  rwdtsperf_config_scratch_t *scratch = (rwdtsperf_config_scratch_t *)scratch_in;
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig) *config;
  rw_status_t status = RW_STATUS_SUCCESS;
  char reason[] = "DTSPerf task config apply failed";
  
  instance = (rwdtsperf_instance_ptr_t)ctx;
  rwdts_member_reg_handle_t member_handle = instance->member_handle;
  
  switch(action){
    case RWDTS_APPCONF_ACTION_INSTALL:
      if (!scratch) {
        rw_keyspec_path_t*            keyspec = NULL;
        rwdts_member_cursor_t *cursor = rwdts_member_data_get_cursor(NULL,
                                                                     member_handle);
        while ((config = (RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig) *)
                rwdts_member_reg_handle_get_next_element(member_handle,
                                                        cursor, NULL, 
                                                        &keyspec)) != NULL){
          status = rwdtsperf_apply_config (instance, config);
          if (status != RW_STATUS_SUCCESS) {
            goto error;
          }
        }
        rwdts_member_data_delete_cursors(NULL, member_handle);
      }
      return;
    break;
    case RWDTS_APPCONF_ACTION_RECONCILE:
    MSG_PRN ("%p scratch %p", scratch, instance);
      {
        int i;
        for (i = 0; i < scratch->count; i++){
          config = scratch->config[i];
          status = rwdtsperf_apply_config (instance, config);
          if (status != RW_STATUS_SUCCESS) {
            goto error;
          }
        }
      }
      break;
    default:
      RW_CRASH();
      break;
  }

  return;
error:
  rwdts_appconf_xact_add_issue(ac, xact, status, reason);
  return;
}


static void
rwdtsperf_dts_mgmt_prepare_config(rwdts_api_t *apih,
                                  rwdts_appconf_t *ac, 
                                  rwdts_xact_t *xact,
                                  const rwdts_xact_info_t *queryh,
                                  rw_keyspec_path_t *keyspec, 
                                  const ProtobufCMessage *pbmsg, 
                                  void *ctx, 
                                  void *scratch_in)
{
  RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig) *config = (RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig)*)pbmsg;
  rwdtsperf_instance_t *instance = (rwdtsperf_instance_t *)ctx;
  rwdtsperf_config_scratch_t *scratch = (rwdtsperf_config_scratch_t *)scratch_in;
  
  RWTRACE_INFO(instance->rwtasklet_info->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "RW.Dtsperf<%d>: Prepare callback for xact: %p for trafsim\n",
               instance->rwtasklet_info->identity.rwtasklet_instance_id,
               xact);
  

  scratch->config[scratch->count] = (RWPB_T_MSG(RwDtsperf_data_PerfdtsConfig) *) protobuf_c_message_duplicate(NULL,
                                   (const ProtobufCMessage *)config,
                                   config->base.descriptor);
  RW_ASSERT(scratch->count < 128);
  RW_ASSERT(scratch->config[scratch->count] != NULL);
  scratch->count++;
  
  rwdts_appconf_prepare_complete_ok(ac, queryh);
  
  return;
}

static rw_keyspec_path_t* rwdtsperf_populate_dtsperf_keyspec(rwdtsperf_instance_ptr_t instance,
                                       RWPB_T_PATHSPEC(RwDtsperf_data_PerfdtsConfig) *pathspec)
{
  rw_keyspec_path_t *keyspec;

  *pathspec = (*RWPB_G_PATHSPEC_VALUE(RwDtsperf_data_PerfdtsConfig));
  keyspec = (rw_keyspec_path_t *)pathspec;
  rw_keyspec_path_set_category (keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  return keyspec;
}

rw_status_t rwdtsperf_dts_mgmt_register_dtsperf_config (rwdtsperf_instance_ptr_t instance)
{
  rwdts_appconf_cbset_t cbset = {
    .xact_init          = rwdtsperf_dts_mgmt_xact_init,
    .xact_deinit        = rwdtsperf_dts_mgmt_xact_deinit,
    .config_validate    = rwdtsperf_dts_mgmt_config_validate,
    .config_apply       = rwdtsperf_dts_mgmt_config_apply,
    .ctx                = instance
  };
  RWPB_T_PATHSPEC(RwDtsperf_data_PerfdtsConfig) pathspec;
  rw_keyspec_path_t  *keyspec = NULL;
  
  void *mgmt_handle;
  
  mgmt_handle = (void *)rwdts_appconf_group_create(instance->dts_h,
                                                   NULL,
                                                   &cbset);
  RW_ASSERT(mgmt_handle);
  if (!mgmt_handle){
    return RW_STATUS_FAILURE;
  }
  
  keyspec = rwdtsperf_populate_dtsperf_keyspec(instance,
                                              &pathspec);
  RW_ASSERT(keyspec != NULL);
  
  instance->member_handle = (void *)
      rwdts_appconf_register(mgmt_handle,
                             keyspec,
                             NULL, /*shard*/
                             RWPB_G_MSG_PBCMD(RwDtsperf_data_PerfdtsConfig),
                             ( RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_CACHE | 0 ),
                             &rwdtsperf_dts_mgmt_prepare_config);
  
  RW_ASSERT(instance->member_handle != NULL);
  
  /* Say we're finished */
  rwdts_appconf_phase_complete(mgmt_handle, RWDTS_APPCONF_PHASE_REGISTER);

  return RW_STATUS_SUCCESS;
}
