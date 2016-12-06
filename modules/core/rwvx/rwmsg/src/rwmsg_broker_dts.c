
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
 * @file rwmsg_broker_dts.c
 * @author RIFT.io <info@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG management
 */

#include "rwmsg_int.h"
#include "rwmsg_broker.h"
#include "rwmsg_broker_tasklet.h"
#include "rw-base.pb-c.h"
#include "rwmsg-data.pb-c.h"
#include "rwmsg_broker_mgmt.h"
#include "libxml/xpath.h"
#include "libxml/parser.h"
#include "libxml/tree.h"

RW_CF_TYPE_DEFINE("RW.Init RWTasklet Component Type", rwmsgbroker_component_ptr_t);
RW_CF_TYPE_DEFINE("RW.Init RWTasklet Instance Type", rwmsgbroker_instance_ptr_t);

static rwdts_member_rsp_code_t
rwmsg_broker_mgmt_handle_messaging_info_request_dts(const rwdts_xact_info_t* xact_info,
                                                    RWDtsQueryAction         action,
                                                    const rw_keyspec_path_t*      key,
                                                    const ProtobufCMessage*  msg,
                                                    uint32_t credits,
                                                    void *getnext_ptr)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(xact_info);
  rwmsgbroker_instance_ptr_t instance = (rwmsgbroker_instance_ptr_t)xact_info->ud;
  rwdts_api_t *apih = instance->dts_h;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwmsgData_data_Messaging_Info) *inp;


  inp = (RwmsgData__YangData__RwmsgData__Messaging__Info*)msg;

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);

  if (!inp) {
    return RWDTS_ACTION_NA;
  }

  char tasklet_name_str[512]; tasklet_name_str[0] = '\0';
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;
  sprintf(tasklet_name_str, "%s-%u",
          tasklet->identity.rwtasklet_name,
          tasklet->identity.rwtasklet_instance_id);
  /* instance->rwtasklet_info.identity */
  char *inp_tasklet_name = (inp->n_broker ?
                            (*inp->broker)->tasklet_name ?
                             *(*inp->broker)->tasklet_name ?
                              (*inp->broker)->tasklet_name : NULL : NULL : NULL);
  if (!(!inp_tasklet_name || (inp_tasklet_name != NULL && !strcmp(inp_tasklet_name, tasklet_name_str)))) {
    return RWDTS_ACTION_NA;
  }

  char uri_str[512]; uri_str[0] = '\0';
  rs = rwmsg_endpoint_get_property_string(instance->broker->ep, "/rwmsg/broker/nnuri", uri_str, sizeof(uri_str));
  RW_ASSERT(rs==RW_STATUS_SUCCESS);

  RWPB_T_MSG(RwmsgData_data_Messaging_Info) messaging_info, *messaging_info_p;
  RWPB_F_MSG_INIT(RwmsgData_data_Messaging_Info, &messaging_info);
  messaging_info_p = &messaging_info;


  RWPB_T_MSG(RwmsgData_Brokerinfo) info, *bi_ptr;
  RWPB_F_MSG_INIT(RwmsgData_Brokerinfo, &info);
  bi_ptr = &info;


  /* Broker info */
  messaging_info.n_broker = 1;
  messaging_info.broker = &bi_ptr;
  info.tasklet_name = tasklet_name_str;
  info.instance_uri = uri_str;

  /* Channel info */
  int n=0;
  RWPB_T_MSG(RwmsgData_Channelinfo) *chtab = NULL, **chtab_vec = NULL;
  if (RW_STATUS_SUCCESS == rwmsg_broker_mgmt_get_chtab_FREEME(instance->broker, &n, &chtab)
      && n > 0) {
    /* This is annoying, need option for Protobuf repeated fields
       in C to be a pointer to array (aka a table) instead of a
       pointer to pointer-array */
    chtab_vec = calloc(n, sizeof(chtab));
    int i;
    for (i=0; i<n; i++) {
      chtab_vec[i] = chtab+i;
    }
  }
  info.channels = chtab_vec;
  info.n_channels = n;

  RWPB_T_PATHSPEC(RwmsgData_data_Messaging_Info) *show_msg_info = NULL;

  /* register for DTS state */
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_Info), NULL ,
                             (rw_keyspec_path_t**)&show_msg_info);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)show_msg_info, NULL, RW_SCHEMA_CATEGORY_DATA);
  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t*)show_msg_info,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&messaging_info_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);

  // Free the protobuf
  rw_keyspec_path_free((rw_keyspec_path_t*)show_msg_info, NULL);

  // Free chtab
  if (chtab) rwmsg_broker_mgmt_get_chtab_free(n, chtab);
  if (chtab_vec) free(chtab_vec);

  return RWDTS_ACTION_OK;
}

static void rwmsg_audit_response_cb (rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud) {
  rwdts_query_result_t *res = NULL;
  rwmsgbroker_instance_ptr_t instance  = ud;

  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);

  char tasklet_name_str[512]; tasklet_name_str[0] = '\0';
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;
  sprintf(tasklet_name_str, "%s-%u",
          tasklet->identity.rwtasklet_name,
          tasklet->identity.rwtasklet_instance_id);

  rwmsg_broker_t *bro = instance->broker;
  bro->audit_completed = TRUE;
  gettimeofday(&bro->audit_completed_tv, NULL);

  switch (xact_status->status) 
  {
    case RWDTS_XACT_COMMITTED:
    case RWDTS_XACT_RUNNING:
      res = rwdts_xact_query_result(xact, 0);
      RW_ASSERT(res);

      const RWPB_T_MSG(RwmsgData_data_Messaging_Info) *msg_info;
      msg_info = (RWPB_T_MSG(RwmsgData_data_Messaging_Info)*)rwdts_query_result_get_protobuf(res);

      rwmsg_broker_channel_t *bch=NULL, *tmp=NULL;
      int my_methbct = 0;
      HASH_ITER(hh, bro->acc.bch_hash, bch, tmp) {
        if (bch->ch.chantype != RWMSG_CHAN_PEERSRV && bch->ch.chantype != RWMSG_CHAN_BROSRV)
          continue;
        RWMSG_RG_LOCK();
        struct rwmsg_methbinding_s *mb = NULL;
        for (mb = RW_DL_HEAD(&((rwmsg_broker_srvchan_t *)bch)->methbindings, struct rwmsg_methbinding_s, elem);
             mb;
             mb = RW_DL_NEXT(mb, struct rwmsg_methbinding_s, elem)) {
          int i; 
          for (i=0; i<mb->srvchans_ct; i++) {
            if (mb->srvchans[i].ch == &bch->ch) {
              my_methbct++;
            }
          }
        }
        RWMSG_RG_UNLOCK();
      }
      int i; for (i=0; i<msg_info->n_broker; i++) {
        unsigned int tot_methbindings = 0;
        RWPB_T_MSG(RwmsgData_Brokerinfo) *bi_ptr;
        bi_ptr = msg_info->broker[i];
        if (!strcmp(tasklet_name_str, bi_ptr->tasklet_name)) continue;
        int j; for (j=0; j<bi_ptr->n_channels; j++) {
          RWPB_T_MSG(RwmsgData_Channelinfo) *chtab = bi_ptr->channels[j];
          tot_methbindings += chtab->n_methbindings;
          int k; for (k=0; k<chtab->n_methbindings; k++) {
            //RWPB_T_MSG(RwmsgData_Methbinding) *methbtab = chtab->methbindings[k];
          }
        }
        bro->audit_succeeded = (my_methbct == tot_methbindings);
      }
      break;
    case RWDTS_XACT_INIT:
    case RWDTS_XACT_ABORTED:
    case RWDTS_XACT_FAILURE:
    default:
      bro->audit_succeeded = FALSE;
      break;
  }

}

static void rwmsg_start_broker_audit_f(void *ud) {
  rwmsgbroker_instance_ptr_t instance = ud;
  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);

  rw_status_t rs = RW_STATUS_SUCCESS;

  rw_keyspec_path_t *messaging_info_keyspec = 0;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_Info), NULL ,
                                  &messaging_info_keyspec);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  rw_keyspec_path_set_category (messaging_info_keyspec, NULL , RW_SCHEMA_CATEGORY_DATA);

  rwdts_xact_t *xact;
#if 1
  xact = rwdts_api_query_ks(instance->dts_h,     /* api_handle */
			    messaging_info_keyspec,
			    RWDTS_QUERY_READ,
			    0,   // return one result, not many
			    rwmsg_audit_response_cb,
			    instance,
			    NULL);
#else
  char *messaging_info_xpath = NULL;
  RW_ASSERT(rw_keyspec_path_to_xpath(messaging_info_keyspec,
                                     rwdts_api_get_ypbc_schema(instance->dts_h),
                                     &messaging_info_xpath) > 0);
  xact = rwdts_api_query(instance->dts_h,     /* api_handle */
                         messaging_info_xpath,
                         RWDTS_QUERY_READ,
                         RWDTS_FLAG_MERGE,   // return one result, not many
                         rwmsg_audit_response_cb,
                         instance,
                         NULL);
  free(messaging_info_xpath);
#endif
  RW_ASSERT(xact != NULL);

  rw_keyspec_path_free(messaging_info_keyspec, NULL );
}
rw_status_t rwmsg_start_broker_audit(rwmsgbroker_instance_ptr_t instance) {

  RW_CF_TYPE_VALIDATE(instance, rwmsgbroker_instance_ptr_t);
  rw_status_t rs = RW_STATUS_SUCCESS;
  rwmsg_start_broker_audit_f(instance);

  return rs;
}

static rwdts_member_rsp_code_t
rwmsg_broker_mgmt_handle_messaging_audit_request_dts(const rwdts_xact_info_t* xact_info,
                                                     RWDtsQueryAction         action,
                                                     const rw_keyspec_path_t* key,
                                                     const ProtobufCMessage*  msg,
                                                     uint32_t                 credits,
                                                     void                     *getnext_ptr)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(xact_info);
  rwmsgbroker_instance_ptr_t instance = (rwmsgbroker_instance_ptr_t)xact_info->ud;
  rwmsg_broker_t *bro = instance->broker;
  rwdts_api_t *apih = instance->dts_h;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwmsgData_data_Messaging_Audit) *inp;


  inp = (RwmsgData__YangData__RwmsgData__Messaging__Audit*)msg;

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);

  if (!inp) {
    return RWDTS_ACTION_NA;
  }

  char tasklet_name_str[512]; tasklet_name_str[0] = '\0';
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;
  sprintf(tasklet_name_str, "%s-%u",
          tasklet->identity.rwtasklet_name,
          tasklet->identity.rwtasklet_instance_id);
  /* instance->rwtasklet_info.identity */
  char *inp_tasklet_name = (inp->n_broker ?
                            (*inp->broker)->tasklet_name ?
                             *(*inp->broker)->tasklet_name ?
                              (*inp->broker)->tasklet_name : NULL : NULL : NULL);
  if (!(!inp_tasklet_name || (inp_tasklet_name != NULL && !strcmp(inp_tasklet_name, tasklet_name_str)))) {
    return RWDTS_ACTION_NA;
  }

  RWPB_T_MSG(RwmsgData_data_Messaging_Audit) messaging_audit, *messaging_audit_p;
  RWPB_F_MSG_INIT(RwmsgData_data_Messaging_Audit, &messaging_audit);
  messaging_audit_p = &messaging_audit;

  RWPB_T_MSG(RwmsgData_data_Messaging_Audit_Broker) broker, *broker_p;
  RWPB_F_MSG_INIT(RwmsgData_data_Messaging_Audit_Broker, &broker);
  broker_p = &broker;

  messaging_audit.n_broker = 1;
  messaging_audit.broker = &broker_p;

  broker.tasklet_name = strdup(tasklet_name_str);

#if 1
  broker.has_started = TRUE;
  if (rwmsg_start_broker_audit(instance) == RW_STATUS_SUCCESS) {
    broker.started = TRUE;
  }
  if (bro->audit_completed) {
    struct timeval now, delta;
    gettimeofday(&now, NULL);
    timersub(&now, &bro->audit_completed_tv, &delta);
    if (delta.tv_sec >= 0 && delta.tv_usec >= 0) {
      broker.has_completed_secs_back = TRUE;
      broker.completed_secs_back = delta.tv_usec / 1000000;
      broker.completed_secs_back += delta.tv_sec;
    }
  }
#endif

  broker.has_completed = TRUE;
  broker.completed = bro->audit_completed;
  broker.has_succeeded = TRUE;
  broker.succeeded = bro->audit_succeeded;

  RWPB_T_PATHSPEC(RwmsgData_data_Messaging_Audit) *show_msg_audit = NULL;
  /* register for DTS state */
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_Audit), NULL ,
                             (rw_keyspec_path_t**)&show_msg_audit);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)show_msg_audit, NULL , RW_SCHEMA_CATEGORY_DATA);
  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t*)show_msg_audit,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&messaging_audit_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);


  // Free the protobuf
  rw_keyspec_path_free((rw_keyspec_path_t*)show_msg_audit, NULL);
  free(broker.tasklet_name);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwmsg_broker_mgmt_handle_messaging_debug_info_request_dts(const rwdts_xact_info_t* xact_info,
                                                     RWDtsQueryAction         action,
                                                     const rw_keyspec_path_t* key,
                                                     const ProtobufCMessage*  msg,
                                                     uint32_t                 credits,
                                                     void                     *getnext_ptr)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_ASSERT(xact_info);
  rwmsgbroker_instance_ptr_t instance = (rwmsgbroker_instance_ptr_t)xact_info->ud;
  rwmsg_broker_t *bro = instance->broker;
  rwdts_api_t *apih = instance->dts_h;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwmsgData_data_Messaging_DebugInfo) *inp;


  inp = (RWPB_T_MSG(RwmsgData_data_Messaging_DebugInfo) *)msg;

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);

  if (!inp) {
    return RWDTS_ACTION_NA;
  }

  char tasklet_name_str[512]; tasklet_name_str[0] = '\0';
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;
  sprintf(tasklet_name_str, "%s-%u",
          tasklet->identity.rwtasklet_name,
          tasklet->identity.rwtasklet_instance_id);
  /* instance->rwtasklet_info.identity */
  char *inp_tasklet_name = (inp->n_broker ?
                            (*inp->broker)->tasklet_name ?
                             *(*inp->broker)->tasklet_name ?
                              (*inp->broker)->tasklet_name : NULL : NULL : NULL);
  if (!(!inp_tasklet_name || (inp_tasklet_name != NULL && !strcmp(inp_tasklet_name, tasklet_name_str)))) {
    return RWDTS_ACTION_NA;
  }

  RWPB_T_MSG(RwmsgData_data_Messaging_DebugInfo) messaging_debug_info, *messaging_debug_info_p;
  RWPB_F_MSG_INIT(RwmsgData_data_Messaging_DebugInfo, &messaging_debug_info);
  messaging_debug_info_p = &messaging_debug_info;

  RWPB_T_MSG(RwmsgData_data_Messaging_DebugInfo_Broker) broker, *broker_p;
  RWPB_F_MSG_INIT(RwmsgData_data_Messaging_DebugInfo_Broker, &broker);
  broker_p = &broker;

  messaging_debug_info.n_broker = 1;
  messaging_debug_info.broker = &broker_p;

  broker.tasklet_name = strdup(tasklet_name_str);

  /* Channel info */
  int n=0;
  RWPB_T_MSG(RwmsgData_ChannelDebugInfo) *chtab = NULL, **chtab_vec = NULL;
  if (RW_STATUS_SUCCESS == rwmsg_broker_mgmt_get_chtab_debug_info_FREEME(bro, &n, &chtab)
      && n > 0) {
    /* This is annoying, need option for Protobuf repeated fields
       in C to be a pointer to array (aka a table) instead of a
       pointer to pointer-array */
    chtab_vec = calloc(n, sizeof(chtab));
    int i;
    for (i=0; i<n; i++) {
      chtab_vec[i] = chtab+i;
    }
  }
  broker.channels = chtab_vec;
  broker.n_channels = n;

  RWPB_T_PATHSPEC(RwmsgData_data_Messaging_DebugInfo) *show_msg_debug_info = NULL;
  /* register for DTS state */
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_DebugInfo), NULL ,
                             (rw_keyspec_path_t**)&show_msg_debug_info);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)show_msg_debug_info, NULL , RW_SCHEMA_CATEGORY_DATA);
  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t*)show_msg_debug_info,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&messaging_debug_info_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);


  // Free the protobuf
  rw_keyspec_path_free((rw_keyspec_path_t*)show_msg_debug_info, NULL);
  free(broker.tasklet_name);
  // Free chtab
  if (chtab) rwmsg_broker_mgmt_get_chtab_debug_info_free(n, chtab);

  return RWDTS_ACTION_OK;
}

static void rwdts_broker_state_changed(rwdts_api_t *apih,
				       rwdts_state_t state,
				       void*         user_data) {
  switch (state) {
  case RW_DTS_STATE_INIT:
    rwdts_api_set_state(apih, RW_DTS_STATE_REGN_COMPLETE);
    break;
  case RW_DTS_STATE_CONFIG:
    rwdts_api_set_state(apih, RW_DTS_STATE_RUN);
    break;
  default:
    break;
  }
}

rw_status_t
rwmsg_broker_dts_registration(rwmsgbroker_instance_ptr_t instance) {
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;

  // get a DTS API handle
  instance->dts_h = rwdts_api_new (tasklet,
        (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwmsgData), rwdts_broker_state_changed, instance, NULL);

  RW_ASSERT(instance->dts_h);

  // Set the category for the configurations.
  rw_status_t rs;

  rw_keyspec_path_t *messaging_info_data = 0;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_Info), NULL , &messaging_info_data);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category (messaging_info_data, NULL , RW_SCHEMA_CATEGORY_DATA);

  rw_keyspec_path_t *messaging_audit_data = 0;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_Audit), NULL , &messaging_audit_data);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category (messaging_audit_data, NULL , RW_SCHEMA_CATEGORY_DATA);

#if 1
  rw_keyspec_path_t *messaging_debug_info_data = 0;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwmsgData_data_Messaging_DebugInfo), NULL , &messaging_debug_info_data);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category (messaging_debug_info_data, NULL , RW_SCHEMA_CATEGORY_DATA);
#endif

  rwdts_registration_t regns[] = {

    /*-------------------------------------------------*
     * Configuration                                   *
     *-------------------------------------------------*/

    /*-------------------------------------------------*
     * Data / Get                                      *
     *-------------------------------------------------*/

    /* show messaging */
    {.keyspec = messaging_info_data,
     //.desc = RWPB_G_MSG_PBCMD(RwmsgData_RwBase_data_Messaging),
     .desc = RWPB_G_MSG_PBCMD(RwmsgData_data_Messaging_Info),
     .flags =  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwmsg_broker_mgmt_handle_messaging_info_request_dts,
      }
    },

    {.keyspec = messaging_audit_data,
     //.desc = RWPB_G_MSG_PBCMD(RwmsgData_RwBase_data_Messaging),
     .desc = RWPB_G_MSG_PBCMD(RwmsgData_data_Messaging_Audit),
     .flags =  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwmsg_broker_mgmt_handle_messaging_audit_request_dts,
      }
    },

    {.keyspec = messaging_debug_info_data,
     //.desc = RWPB_G_MSG_PBCMD(RwmsgData_RwBase_data_Messaging),
     .desc = RWPB_G_MSG_PBCMD(RwmsgData_data_Messaging_DebugInfo),
     .flags =  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwmsg_broker_mgmt_handle_messaging_debug_info_request_dts,
      }
    },

    /*-------------------------------------------------*
     * RPC                                             *
     *-------------------------------------------------*/
  };

  int i;
  for (i = 0; i < sizeof (regns) / sizeof (rwdts_registration_t); i++) {
    regns[i].callback.ud = instance;
    rwdts_member_register(NULL, instance->dts_h,regns[i].keyspec, &regns[i].callback,
                          regns[i].desc, regns[i].flags, NULL);
  }

  rw_keyspec_path_free (messaging_info_data, NULL);
  rw_keyspec_path_free (messaging_audit_data, NULL);
  rw_keyspec_path_free (messaging_debug_info_data, NULL);
  return RW_STATUS_SUCCESS;
}
