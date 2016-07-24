
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_member_registration.c
 * @author Rajesh Velandy
 * @date 2014/11/15
 * @brief DTS  member Registrations related functions and APIs
 */

#include <stdlib.h>
#include <rwtypes.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <rwdts.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwlib.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rw-dts.pb-c.h>


#define RWDTS_REG_PRINT_THRESH 15
#define RWDTS_ENABLED_LIVE_RECOVERY 0

struct rwdts_tmp_reg_data_s {
  rwdts_xact_t*           xact;
  rwdts_api_t*            apih;
};

#define RWDTS_MAX_MESSAGE_PER_QUERY_RESULT 512

struct rwdts_tmp_query_rsp_s {
  rwdts_xact_t*           xact;
  rwdts_xact_query_t      *xquery;
};

/*
 * Important - Update the following list of values
 * whenever "registration-evt" in "rw-dts.yang"
 * is updated.
 */
 
static const char*  _rwdts_registration_evt_values[] =  {
  "RW_DTS_REGISTRATION_EVT_BEGIN_REG",
  "RW_DTS_REGISTRATION_EVT_REGISTERED",
  "RW_DTS_REGISTRATION_EVT_BEGIN_SYNC",
  "RW_DTS_REGISTRATION_EVT_SOLICIT_REQ",
  "RW_DTS_REGISTRATION_EVT_SOLICIT_RSP",
  "RW_DTS_REGISTRATION_EVT_ADVISE",
  "RW_DTS_REGISTRATION_EVT_RECONCILE",
  "RW_DTS_REGISTRATION_EVT_READY",
  "RW_DTS_REGISTRATION_EVT_DEREGISTER"
};

static const char*
rwdts_registration_evt_str(RwDts__YangEnum__RegistrationEvt__E evt)
{
  if (evt < RW_DTS_REGISTRATION_EVT_BEGIN_REG ||
      evt > RW_DTS_REGISTRATION_EVT_READY) {
    return "";
  }
  uint32_t idx = evt - RW_DTS_REGISTRATION_EVT_BEGIN_REG;
  RW_ASSERT(idx < ((sizeof(_rwdts_registration_evt_values)/sizeof(_rwdts_registration_evt_values[0]))));
  return _rwdts_registration_evt_values[idx];
}

/*
 * Important - Update the following list of values
 * whenever "registration-state" in "rw-dts.yang"
 * is updated.
 */
static const char*  _rwdts_member_state_values[] =  {
  "RW_DTS_REGISTRATION_STATE_INIT",
  "RW_DTS_REGISTRATION_STATE_REGISTERING",
  "RW_DTS_REGISTRATION_STATE_WAITING_SYNC",
  "RW_DTS_REGISTRATION_STATE_IN_SYNC",
  "RW_DTS_REGISTRATION_STATE_RECONCILE",
  "RW_DTS_REGISTRATION_STATE_REG_READY",
  "RW_DTS_REGISTRATION_STATE_DEREGISTERING"
};

static const char*
rwdts_registration_state_str(RwDts__YangEnum__RegistrationState__E state)
{
  if (state < RW_DTS_REGISTRATION_STATE_INIT ||
      state > RW_DTS_REGISTRATION_STATE_REG_READY) {
    return "";
  }
  uint32_t idx = state - RW_DTS_REGISTRATION_STATE_INIT;
  RW_ASSERT(idx < ((sizeof(_rwdts_member_state_values)/sizeof(_rwdts_member_state_values[0]))));
  return _rwdts_member_state_values[idx];
}

/*******************************************************************************
 *                      Static prototypes                                      *
 *******************************************************************************/
static void
rwdts_member_reg_deinit(rwdts_member_registration_t *reg);

static void
rwdts_send_registrations_to_router(void* ud);
static void
rwdts_send_deregistrations_to_router(void* ud);

static void
rwdts_update_registrations_to_router(void* ud);

static void
rwdts_member_reg_advise_cb(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);

static void
rwdts_member_reg_update_advise_cb(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);

static void
rwdts_send_deregister_path(void *ud);

static rwdts_member_reg_handle_t
rwdts_member_register_int(rwdts_xact_t*                     xact,
                          rwdts_api_t*                      apih,
                          rwdts_group_t*                    group,
                          const rw_keyspec_path_t*          keyspec,
                          const rwdts_member_event_cb_t*    cb,
                          const ProtobufCMessageDescriptor* desc,
                          uint32_t                          flags,
                          const rwdts_shard_info_detail_t*  shard_detail);
static rwdts_member_rsp_code_t
rwdts_member_handle_prepare(const rwdts_xact_info_t* xact_info,
                            RWDtsQueryAction         action,
                            const rw_keyspec_path_t* keyspec,
                            const ProtobufCMessage*  msg,
                            uint32_t                 credits,
                            void*                    get_next_key);

static rwdts_member_rsp_code_t
rwdts_member_handle_precommit(const rwdts_xact_info_t*      xact_info,
                              uint32_t                      n_crec,
                              const rwdts_commit_record_t** crec);

static rwdts_member_rsp_code_t
rwdts_member_handle_commit(const rwdts_xact_info_t*      xact_info,
                           uint32_t                      n_crec,
                           const rwdts_commit_record_t** crec);

static rwdts_member_rsp_code_t
rwdts_member_handle_abort(const rwdts_xact_info_t*      xact_info,
                          uint32_t                      n_crec,
                          const rwdts_commit_record_t** crec);

static void
rwdts_process_pending_advise(void *arg);
/*******************************************************************************
 *                      Static functions                                       *
 *******************************************************************************/

static void
rwdts_member_reg_deinit(rwdts_member_registration_t *reg)
{
  if (reg->memb_reg) {
    RW_ASSERT_TYPE(reg->memb_reg, rwdts_member_reg_t);
    RW_FREE_TYPE(reg->memb_reg, rwdts_member_reg_t);
    reg->memb_reg = NULL;
  }
  return;
}

static char*
rwdts_get_file_dbname(char *client_path)
{
  char *str1, *saveptr1 = NULL, *token = NULL;
  int j;
  const char *delim = "/";
  char file_name[250];
  char *name_ptr = file_name;
  int count = 0;

  file_name[0] = '\0';

  for(j = 1, str1 = client_path; ;j++, str1 = NULL) {
    token = strtok_r(str1, delim, &saveptr1);
    if (token == NULL) {
      break;
    }
    count = sprintf(name_ptr, "%s-", token);
    name_ptr += count;
  }
  free(client_path);
  return strdup(file_name);
}

static void
rwdts_sched_timer (struct rwdts_tmp_reg_data_s *temp_reg)
{
  RW_ASSERT(temp_reg);

  rwdts_api_t *apih = temp_reg->apih;
  RW_ASSERT(apih);

  if (apih->timer != NULL) {
    if (rwsched_dispatch_get_context(apih->tasklet, apih->timer) != temp_reg) {
      RW_FREE (temp_reg);
    }
    return;
  }
  if (temp_reg->xact) {
    rwdts_xact_ref(temp_reg->xact, __PRETTY_FUNCTION__, __LINE__);
  }
  apih->timer = rwsched_dispatch_source_create(apih->tasklet,
                                               RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                               0,
                                               0,
                                               apih->client.rwq);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    apih->timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 0.10 *NSEC_PER_SEC),
                                    0,
                                    0);

  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              apih->timer,
                                              rwdts_send_registrations_to_router);

  rwsched_dispatch_set_context(apih->tasklet,
                               apih->timer,
                               temp_reg);


  rwsched_dispatch_resume(apih->tasklet,
                          apih->timer);

  RWDTS_API_LOG_EVENT(apih, TimerStarted, "DTS API timer started");
  return;
}

static void
rwdts_sched_update_timer (struct rwdts_tmp_reg_data_s *temp_reg)
{
  RW_ASSERT(temp_reg);

  rwdts_api_t *apih = temp_reg->apih;
  RW_ASSERT(apih);

  if (apih->update_timer != NULL) {
    if (rwsched_dispatch_get_context(apih->tasklet, apih->update_timer) != temp_reg) {
      RW_FREE (temp_reg);
    }
    return;
  }
  if (temp_reg->xact) {
    rwdts_xact_ref(temp_reg->xact, __PRETTY_FUNCTION__, __LINE__);
  }
  apih->update_timer = rwsched_dispatch_source_create(apih->tasklet,
                                               RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                               0,
                                               0,
                                               apih->client.rwq);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    apih->update_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 0.10 *NSEC_PER_SEC),
                                    0,
                                    0);

  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              apih->update_timer,
                                              rwdts_update_registrations_to_router);

  rwsched_dispatch_set_context(apih->tasklet,
                               apih->update_timer,
                               temp_reg);


  rwsched_dispatch_resume(apih->tasklet,
                          apih->update_timer);

  RWDTS_API_LOG_EVENT(apih, TimerStarted, "DTS API update timer started");
  return;
}


void
rwdts_member_store_registration(rwdts_api_t *apih,
                                RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)  *rcvreg)
{
  RW_ASSERT(rcvreg);
  int i;
  rw_status_t rs;

  for (i=0; i < rcvreg->n_registration; i++) {
    if (rcvreg->registration[i]->rtr_reg_id) {
      RWPB_T_MSG(RwDts_data_RtrInitRegId_Member_Registration) store_data;
      RWPB_F_MSG_INIT(RwDts_data_RtrInitRegId_Member_Registration,&store_data);

      RWPB_T_PATHENTRY(RwDts_data_RtrInitRegId_Member_Registration) path_entry = *(RWPB_G_PATHENTRY_VALUE(RwDts_data_RtrInitRegId_Member_Registration));

      store_data.keystr = rcvreg->registration[i]->keystr;
      store_data.has_keyspec = 1;
      store_data.keyspec.len = rcvreg->registration[i]->keyspec.len;
      store_data.keyspec.data = rcvreg->registration[i]->keyspec.data;
      store_data.flags = rcvreg->registration[i]->flags;
      store_data.has_flavor = rcvreg->registration[i]->has_flavor;
      store_data.flavor = rcvreg->registration[i]->flavor;
      store_data.has_index = rcvreg->registration[i]->has_index;
      store_data.index = rcvreg->registration[i]->index;
      store_data.has_start = rcvreg->registration[i]->has_start;
      store_data.start = rcvreg->registration[i]->start;
      store_data.has_stop = rcvreg->registration[i]->has_stop;
      store_data.stop = rcvreg->registration[i]->stop;

      RW_ASSERT(rcvreg->registration[i]->flavor);
      if (rcvreg->registration[i]->has_minikey) {
        store_data.has_minikey = 1;
        store_data.minikey.len = rcvreg->registration[i]->minikey.len;
        store_data.minikey.data = rcvreg->registration[i]->minikey.data;
      }

      if (rcvreg->registration[i]->has_descriptor) {
        strcpy(store_data.descriptor, rcvreg->registration[i]->descriptor);
      }
      store_data.rtr_reg_id = rcvreg->registration[i]->rtr_reg_id;
      store_data.rtr_id = rcvreg->rtr_id;

      path_entry.has_key00 = TRUE;
      path_entry.key00.rtr_reg_id = rcvreg->registration[i]->rtr_reg_id;
      path_entry.has_key01 = TRUE;
      path_entry.key01.rtr_id = rcvreg->rtr_id;


      rs = rwdts_member_reg_handle_create_element(apih->init_regidh,
                                                 (rw_keyspec_entry_t *)&path_entry,
                                                  &store_data.base,
                                                  NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

    }
  }
  return;
}

static void
rwdts_recovery_audit_done(rwdts_api_t*                apih,
                          rwdts_member_reg_handle_t   regh,
                          const rwdts_audit_result_t* audit_result,
                          const void*                 user_data)
{
  rwdts_member_data_object_t *mobj, *tmp;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  RWDTS_API_LOG_EVENT(apih, AuditDone, RWLOG_ATTR_SPRINTF("Audit done for %s, %p", reg->keystr, reg->obj_list));

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  HASH_ITER(hh_data, reg->obj_list, mobj, tmp) {
    if (reg->cb.cb.reg_ready_old) {
      (*reg->cb.cb.reg_ready_old)((rwdts_member_reg_handle_t)reg, mobj->keyspec, mobj->msg, reg->cb.ud);
    }
  } /* HASH_ITER */

  /* The  new version of reg ready is invoked just once */
  if (reg->cb.cb.reg_ready) {
    (*reg->cb.cb.reg_ready)((rwdts_member_reg_handle_t)reg, RW_STATUS_SUCCESS, reg->cb.ud);
  }
  rwdts_state_transition(reg->apih, RW_DTS_STATE_CONFIG);
}

static void
rwdts_begin_recovery_audit(rwdts_member_registration_t *reg)
{
  rw_status_t rs = rwdts_member_reg_handle_audit((rwdts_member_reg_handle_t)reg,
                                                 RW_DTS_AUDIT_ACTION_RECOVER,
                                                 rwdts_recovery_audit_done,
                                                 reg);
  if (rs != RW_STATUS_SUCCESS) {
    reg->audit.recovery_audit_attempts++;
    RW_ASSERT_MESSAGE(0, "Audit failure");
  }
}

rwdts_kv_light_reply_status_t
rwdts_kv_light_get_all_cb(void **key, int *key_len, void **val,
                          int *val_len, int total, void *callbk_data)
{

  rw_keyspec_path_t **keyspecs = NULL;
  ProtobufCMessage **msgs = NULL;
  int key_count = 0;

  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)callbk_data;
  RW_ASSERT(reg);
  rwdts_member_reg_handle_unref((rwdts_member_reg_handle_t)reg);

  if (!total) {
    return RWDTS_KV_LIGHT_REPLY_DONE;
  }
  int i;
  for(i=0; i < total; i++) {
    ProtobufCBinaryData bin_key;
    bin_key.len = key_len[i];
    bin_key.data = (uint8_t *)key[i];
    rw_keyspec_path_t *keyspec = NULL;
    ProtobufCMessage* msg = NULL;

    keyspec = rw_keyspec_path_create_concrete_with_dompath_binary_data(NULL, &bin_key, reg->keyspec->base.descriptor);
    RW_ASSERT(keyspec);
    msg = protobuf_c_message_unpack(NULL, reg->desc,
                                         val_len[i], (const uint8_t*)val[i]); 
    RW_ASSERT(msg);   
    key_count++;      
    keyspecs = realloc(keyspecs, (key_count * sizeof(*keyspecs[0])));
    keyspecs[key_count-1] = keyspec;
    msgs = realloc(msgs, (key_count * sizeof(*msgs[0])));
    msgs[key_count-1] = msg;
    free(val[i]);     
    free(key[i]);     
  }                 
  free(val);        
  free(key);        

  for (i = 0; i < key_count; i++) {
    rwdts_member_reg_handle_create_element_keyspec((rwdts_member_reg_handle_t)reg,
                                                    keyspecs[i],   
                                                    msgs[i],       
                                                    NULL);         
    rw_keyspec_path_free(keyspecs[i], NULL);
    protobuf_c_message_free_unpacked(NULL, msgs[i]);
  }

  if (keyspecs) {
    free(keyspecs);
  }               
  if (msgs) {     
    free(msgs);     
  }               

  if (reg->group.group && reg->group.group->cb.xact_event) {
    reg->group.group->cb.xact_event(reg->apih, reg->group.group, NULL,
                                    RWDTS_MEMBER_EVENT_INSTALL,
                                    reg->group.group->cb.ctx, NULL);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static void rwdts_redis_ready(rwdts_member_registration_t* reg, rw_status_t status)
{
  RW_ASSERT(reg);
  rw_status_t rs;
  if (status != RW_STATUS_SUCCESS) {
    /* TODO */
    /* This code is for active VM only, hence fail this VM and switch the
       standby to active */
    return;
  }
  rs = rwdts_kv_handle_get_all(reg->kv_handle, 0, (void *)rwdts_kv_light_get_all_cb, (void *)reg); 
  if (rs != RW_STATUS_SUCCESS) {
    /* TODO */
    /* This code is for active VM only, hence fail this VM and switch the
     * standby to active */
  }
  return;
}

static void
rwdts_member_reg_advise_cb(rwdts_xact_t* xact,
                            rwdts_xact_status_t* xact_status,
                            void*         ud)
{
  rwdts_api_reg_sent_list_t *api_sent_list = (rwdts_api_reg_sent_list_t *)ud;
  RW_ASSERT(api_sent_list);
  rwdts_api_t *apih = api_sent_list->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_registration_t* reg = NULL;
  int i;

  bool bounced = xact->bounced;
  if (!xact->bounced) {
    apih->stats.member_reg_advise_done++;
    RWDTS_API_LOG_EVENT(apih, RegRetry, "DTS registration retries still under limit - retrying",
                        apih->reg_retry);


    bool aborted = FALSE;

    switch (xact_status->status) {
      case RWDTS_XACT_INIT:
      case RWDTS_XACT_RUNNING:
        {
          struct timeval tv_now, delta;
          gettimeofday(&tv_now, NULL);
          timersub(&tv_now, &api_sent_list->tv_sent, &delta);
          int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
          RWTRACE_INFO(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: reg xact still init/running after %dms",
                       apih->client_path,
                       elapsed);
        }
        return;
      default:
        break;
    }
    /* This may be handled better once the block call back controls the 
     * registration handling
     */
    if ((xact->blocks_ct == 1 && xact->blocks[0]->evtrsp_internal) ||
      (xact_status->block && xact_status->block->evtrsp_internal)||
      ((xact_status->status == RWDTS_XACT_ABORTED) &&
       (apih->free_and_retry_count < RWDTS_MEMBER_MAX_RETRIES_REG))) {
      RWTRACE_INFO(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "%s: Free and retry registration to %s",
                    apih->client_path,
                    apih->router_path);
      bounced = true;
      if (xact_status->status == RWDTS_XACT_ABORTED) {
        apih->free_and_retry_count++;
      }
      goto free_and_retry;
    }
    apih->free_and_retry_count = 0;


    {
      struct timeval tv_now, delta;
      gettimeofday(&tv_now, NULL);
      timersub(&tv_now, &api_sent_list->tv_sent, &delta);
      int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
      RWTRACE_INFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: reg xact received after %dms",
                   apih->client_path,
                   elapsed);
    }


    struct timeval tv_now, msging_time;
    gettimeofday(&tv_now, NULL);
    timersub(&tv_now, &api_sent_list->tv_sent, &msging_time);

    for (i = 0; i < api_sent_list->reg_count; i++) {
      rwdts_api_reg_info_t* sent_reg = NULL;
      RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_list,
                                   &api_sent_list->reg_id[i],
                                   &sent_reg);
      if (sent_reg) {
        rw_keyspec_path_free(sent_reg->keyspec, NULL );
        free(sent_reg->keystr);
        RW_FREE(sent_reg);
      }

      if ((xact_status->status == RWDTS_XACT_ABORTED) ||
          (xact_status->status == RWDTS_XACT_FAILURE)) {

        reg = NULL;
        RW_SKLIST_REMOVE_BY_KEY(&(apih->reg_list),&api_sent_list->reg_id[i],
                                     (void *)&reg);
        aborted = TRUE;

        /* Can this really be absent? */
        if (reg) {
          RW_ASSERT(reg->reg_state != RWDTS_REG_USABLE);

          {
            struct timeval tv_now, delta;
            gettimeofday(&tv_now, NULL);
            timersub(&tv_now, &reg->tv_init, &delta);
            int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
            RWDTS_API_LOG_EVENT(apih, RegAborted, "registration ABORTED after", elapsed, reg->keystr);
          }
        }

      } else {
        RW_SKLIST_LOOKUP_BY_KEY(&(apih->reg_list), &api_sent_list->reg_id[i],
                                  (void *)&reg);

        if (reg && (xact_status->status == RWDTS_XACT_COMMITTED)) {

          RW_ASSERT(reg->reg_state != RWDTS_REG_USABLE);

          reg->reg_state = RWDTS_REG_USABLE;

          {
            struct timeval delta;
            timersub(&tv_now, &reg->tv_init, &delta);
            RWTRACE_INFO(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: registration usable after %dms, keystr='%s'",
                         apih->client_path,
                         (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000),
                         reg->keystr);
            RWMEMLOG(&xact->rwml_buffer, 
                     RWMEMLOG_MEM0, 
                     "registration usable at", 
                     RWMEMLOG_ARG_STRNCPY(128, apih->client_path),
                     RWMEMLOG_ARG_STRNCPY(128, apih->router_path),
                     RWMEMLOG_ARG_PRINTF_INTPTR("after msec %"PRIdPTR, (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000)),
                     RWMEMLOG_ARG_PRINTF_INTPTR("msging msec %"PRIdPTR, (int)(msging_time.tv_sec * 1000 + msging_time.tv_usec / 1000)),
                     RWMEMLOG_ARG_STRNCPY(128, reg->keystr));
          }

#ifdef RWDTS_SHARDING
          if (!reg->shard) {
            reg->shard = rwdts_shard_key_init((rwdts_member_reg_handle_t)reg, NULL, reg->flags, 
                                              reg->has_index?reg->index:-1,
                                              &apih->rootshard, reg->shard_flavor,
                                              (reg->flags&RWDTS_FLAG_SHARDING)?&reg->params:NULL, 
                                              RWDTS_SHARD_KEYFUNC_NOP, NULL,
                                              RWDTS_SHARD_DEMUX_ROUND_ROBIN, NULL);
            RW_ASSERT(reg->shard);
          }
          rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg->shard,&reg->params);
          RW_ASSERT(chunk);
          rwdts_chunk_member_info_t mbr_info;
          mbr_info.member = (rwdts_member_reg_handle_t) reg;
          mbr_info.flags = reg->flags;
          if (reg->flags & RWDTS_FLAG_PUBLISHER) {
            rwdts_member_shard_create_element(reg->shard, 
                                              chunk,   
                                              &mbr_info, 
                                              true, 
                                              &chunk->chunk_id, 
                                              reg->apih->client_path);
            if (apih->rwtasklet_info) {
              if ((reg->group.group && reg->group.group->cb.xact_event) && 
                  (reg->flags & RWDTS_FLAG_DATASTORE) &&
                  ((apih->rwtasklet_info->data_store != RWVCS_TYPES_DATA_STORE_NONE) &&
                   (apih->rwtasklet_info->data_store != RWVCS_TYPES_DATA_STORE_NOSTORE))) {
                if (!reg->kv_handle) {
                  rw_status_t res;
                  char *installdir = getenv("INSTALLDIR");
                  char *db_name = rwdts_get_file_dbname(strdup(apih->client_path));
                  if (apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB) {
                    reg->kv_handle = rwdts_kv_allocate_handle(BK_DB);
                    snprintf(reg->kv_handle->file_name_shard, 255, "%s%s%s%u%s", installdir, "/", db_name, reg->reg_id,".db");
                    res = rwdts_kv_handle_db_connect(reg->kv_handle, NULL, NULL, NULL, reg->kv_handle->file_name_shard, NULL, NULL, NULL);
                    if (res == RW_STATUS_SUCCESS) {
                      rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg);
                      rwdts_kv_handle_get_all(reg->kv_handle, 0, (void *)rwdts_kv_light_get_all_cb, (void *)reg);
                    }
                  } else {
                    reg->kv_handle = rwdts_kv_allocate_handle(REDIS_DB);
                    snprintf(reg->kv_handle->file_name_shard, 255, "%s%s%s%u%s", installdir, "/", db_name, reg->reg_id,".db");
                    char address_port[20];
                    int len = 0;
                    len = sprintf(address_port, "%s%c%d", apih->rwtasklet_info->vm_ip_address, ':', 9999);
                    address_port[len] = '\0';
                    rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg); 
                    rwdts_kv_handle_db_connect(reg->kv_handle, apih->sched,
                                               apih->tasklet, address_port,
                                               reg->kv_handle->file_name_shard,
                                               NULL, (void *)rwdts_redis_ready,
                                               (void *)reg);
                  }
                  free(db_name);
                } 
              }
            }
          }
          if (reg->flags & RWDTS_FLAG_SUBSCRIBER) {
            rwdts_member_shard_create_element(reg->shard, 
                                              chunk,   
                                              &mbr_info, 
                                              false, 
                                              &chunk->chunk_id, 
                                              reg->apih->client_path);
          }
#endif
          /*
           * if this is a subscription registration and if RWDTS_FLAG_CACHE
           * then do a get to poulate cache
           *
           */
           if ((reg->flags & RWDTS_SUB_CACHE_POPULATE_FLAG) == RWDTS_SUB_CACHE_POPULATE_FLAG) {
             reg->apih->reg_usable++;
             RWTRACE_DEBUG(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                           "%s rwdts_begin_recovery_audit %s\n", reg->apih->client_path, reg->keystr);
             rwdts_begin_recovery_audit(reg);
           } else {
             RWDTS_API_LOG_REG_EVENT(reg->apih, reg, RegReadyCbCalled, "Calling Registration ready",
                                     rwdts_get_app_addr_res(apih, reg->cb.cb.reg_ready));
             if (reg->cb.cb.reg_ready_old) {
               (*reg->cb.cb.reg_ready_old)((rwdts_member_reg_handle_t)reg, reg->keyspec, NULL, reg->cb.ud);
             }
             if (reg->cb.cb.reg_ready) {
               (*reg->cb.cb.reg_ready)((rwdts_member_reg_handle_t)reg, RW_STATUS_SUCCESS, reg->cb.ud);
             }
           }
          reg->serial.has_id            = 1;
          reg->serial.id.has_pub_id     = 1;
          reg->serial.id.has_member_id  = 1;
          reg->serial.id.member_id      = apih->client_idx;
          reg->serial.id.router_id      = apih->router_idx;
          reg->serial.id.has_router_id  = 1;
          reg->serial.id.pub_id         = reg->reg_id;
          reg->serial.has_serial        = 1;
          rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_REGISTERED, ud);
        }
      }
    }

    if (apih->timer) {
      struct rwdts_tmp_reg_data_s* tmp_reg = rwsched_dispatch_get_context(apih->tasklet, apih->timer);
      rwsched_dispatch_source_cancel(apih->tasklet, apih->timer);
      rwsched_dispatch_release(apih->tasklet, apih->timer);
      apih->timer = NULL;
      RW_FREE(tmp_reg);
    }


    if (!apih->connected_to_router) {
      rwdts_state_transition(apih, RW_DTS_STATE_INIT);
      apih->connected_to_router = 1;
    }

    if (!aborted) {
      RWDTS_API_LOG_EVENT(apih, Registered, "Registered successfully with DTS Router",
                          apih->reg_retry);
    } else {
      RWDTS_API_LOG_EVENT(apih, Registered, "Registration failure", apih->reg_retry);
    }

    if (apih->init_regkeyh && !aborted)
    {
      RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)  *rcvreg = NULL;
      rwdts_query_result_t* qresults = rwdts_xact_query_result(xact,0);
      if (qresults) {
        rcvreg = ( RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *) qresults->message;
        if (!apih->client_idx) {
          apih->client_idx = rcvreg->client_id;

        }
        RW_ASSERT(apih->client_idx == rcvreg->client_id);
        RW_ASSERT(apih->router_idx == rcvreg->rtr_id);
        rwdts_member_store_registration(apih, rcvreg);
      }
    }
  }
  else
  {
    struct timeval tv_now, delta;
    gettimeofday(&tv_now, NULL);
    timersub(&tv_now, &api_sent_list->tv_sent, &delta);
    int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: reg bounce after %dms",
                 apih->client_path,
                 elapsed);

    apih->stats.member_reg_advise_bounced++;
  }

free_and_retry:
  if (api_sent_list->reg_id) {
    free(api_sent_list->reg_id);
    api_sent_list->reg_id = NULL;
    api_sent_list->reg_count = 0;
  }
  apih->reg_outstanding = FALSE;

  if (rwdts_xact_status_get_xact_done(xact_status)) {
    RW_FREE(api_sent_list);
  }

  if (RW_SKLIST_LENGTH(&apih->sent_reg_list)) {
    struct rwdts_tmp_reg_data_s* tmp_reg = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
    tmp_reg->xact = NULL;
    tmp_reg->apih = apih;
    if (bounced) {
      if (apih->timer) {
        struct rwdts_tmp_reg_data_s* tmp_reg = rwsched_dispatch_get_context(apih->tasklet, apih->timer);
        rwsched_dispatch_source_cancel(apih->tasklet, apih->timer);
        rwsched_dispatch_release(apih->tasklet, apih->timer);
        apih->timer = NULL;
        RW_FREE(tmp_reg);
      }
      rwdts_sched_timer(tmp_reg);
    }
    else {
      rwdts_send_registrations_to_router(tmp_reg);
    }
  }
  return;
}

static void
rwdts_member_reg_update_advise_cb(rwdts_xact_t* xact,
                            rwdts_xact_status_t* xact_status,
                            void*         ud)
{
  rwdts_api_reg_sent_list_t *api_sent_list = (rwdts_api_reg_sent_list_t *)ud;
  RW_ASSERT(api_sent_list);
  rwdts_api_t *apih = api_sent_list->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_registration_t* reg = NULL;
  int i;
  rw_status_t rs;

  bool bounced = xact->bounced;
  if (!xact->bounced) {
    apih->stats.member_reg_update_advise_done++;

    bool aborted = FALSE;

    switch (xact_status->status) {
      case RWDTS_XACT_INIT:
      case RWDTS_XACT_RUNNING:
        {
          struct timeval tv_now, delta;
          gettimeofday(&tv_now, NULL);
          timersub(&tv_now, &api_sent_list->tv_sent, &delta);
          int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
          RWTRACE_INFO(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: reg xact still init/running after %dms",
                       apih->client_path,
                       elapsed);
        }
        return;
      default:
        break;
    }

    /* This may be handled better once the block call back controls the 
     * registration handling
     */
    if ((xact->blocks_ct == 1 && xact->blocks[0]->evtrsp_internal) ||
      (xact_status->block && xact_status->block->evtrsp_internal)||
      ((xact_status->status == RWDTS_XACT_ABORTED) &&
       (apih->free_and_retry_count < RWDTS_MEMBER_MAX_RETRIES_REG))) {
      RWTRACE_INFO(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "%s: Free and retry registration to %s",
                    apih->client_path,
                    apih->router_path);
      bounced = true;
      if (xact_status->status == RWDTS_XACT_ABORTED) {
        apih->free_and_retry_count++;
      }
      goto free_and_retry;
    }
    apih->free_and_retry_count = 0;


    {
      struct timeval tv_now, delta;
      gettimeofday(&tv_now, NULL);
      timersub(&tv_now, &api_sent_list->tv_sent, &delta);
      int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
      RWTRACE_INFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: reg xact received after %dms",
                   apih->client_path,
                   elapsed);
    }


    struct timeval tv_now, msging_time;
    gettimeofday(&tv_now, NULL);
    timersub(&tv_now, &api_sent_list->tv_sent, &msging_time);

    for (i = 0; i < api_sent_list->reg_count; i++) {
      rwdts_api_reg_info_t* sent_reg = NULL;
      RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_update_list,
                                   &api_sent_list->reg_id[i],
                                   &sent_reg);
      if (sent_reg) {
        rw_keyspec_path_free(sent_reg->keyspec, NULL );
        free(sent_reg->keystr);
        RW_FREE(sent_reg);
      }

      if ((xact_status->status == RWDTS_XACT_ABORTED) ||
          (xact_status->status == RWDTS_XACT_FAILURE)) {

        reg = NULL;
        RW_SKLIST_REMOVE_BY_KEY(&(apih->reg_list),&api_sent_list->reg_id[i],
                                     (void *)&reg);
        aborted = TRUE;

        /* Can this really be absent? */
        if (reg) {
          RW_ASSERT(reg->reg_state != RWDTS_REG_USABLE);

          {
            struct timeval tv_now, delta;
            gettimeofday(&tv_now, NULL);
            timersub(&tv_now, &reg->tv_init, &delta);
            RWTRACE_INFO(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: registration ABORTED after %dms, keystr='%s'",
                         apih->client_path,
                         (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000),
                         reg->keystr);
          }
        }

      } else {
        RW_SKLIST_LOOKUP_BY_KEY(&(apih->reg_list), &api_sent_list->reg_id[i],
                                  (void *)&reg);

        if (reg && (xact_status->status == RWDTS_XACT_COMMITTED)) {

          RW_ASSERT(reg->reg_state != RWDTS_REG_USABLE);

          reg->reg_state = RWDTS_REG_USABLE;

          {
            struct timeval delta;
            timersub(&tv_now, &reg->tv_init, &delta);
            RWTRACE_INFO(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: registration usable after %dms, keystr='%s'",
                         apih->client_path,
                         (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000),
                         reg->keystr);
            RWMEMLOG(&xact->rwml_buffer, 
                     RWMEMLOG_MEM0, 
                     "registration usable at", 
                     RWMEMLOG_ARG_STRNCPY(128, apih->client_path),
                     RWMEMLOG_ARG_STRNCPY(128, apih->router_path),
                     RWMEMLOG_ARG_PRINTF_INTPTR("after msec %"PRIdPTR, (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000)),
                     RWMEMLOG_ARG_PRINTF_INTPTR("msging msec %"PRIdPTR, (int)(msging_time.tv_sec * 1000 + msging_time.tv_usec / 1000)),
                     RWMEMLOG_ARG_STRNCPY(128, reg->keystr));
          }

#ifdef RWDTS_SHARDING
          RW_ASSERT(reg->shard);
          rs = rwdts_shard_update(reg->shard, reg->shard_flavor, &reg->params);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg->shard,&reg->params);
          RW_ASSERT(chunk);
#endif
          /*
           * if this is a subscription registration and if RWDTS_FLAG_CACHE
           * then do a get to poulate cache
           *
           */
          RWDTS_API_LOG_REG_EVENT(reg->apih, reg, RegReadyCbCalled, "Calling Registration update ready",
                                  rwdts_get_app_addr_res(apih, reg->cb.cb.reg_ready));
          if (reg->cb.cb.reg_ready_old) {
            (*reg->cb.cb.reg_ready_old)((rwdts_member_reg_handle_t)reg, reg->keyspec, NULL, reg->cb.ud);
          }
          if (reg->cb.cb.reg_ready) {
            (*reg->cb.cb.reg_ready)((rwdts_member_reg_handle_t)reg, RW_STATUS_SUCCESS, reg->cb.ud);
          }
        }
      }
    }

    if (apih->update_timer) {
      rwsched_dispatch_source_cancel(apih->tasklet, apih->update_timer);
      rwsched_dispatch_release(apih->tasklet, apih->update_timer);
      apih->update_timer = NULL;
    }


    if (!apih->connected_to_router) {
      rwdts_state_transition(apih, RW_DTS_STATE_INIT);
      apih->connected_to_router = 1;
    }

    if (!aborted) {
      RWDTS_API_LOG_EVENT(apih, Registered, "Registered successfully with DTS Router",
                          apih->reg_retry);
    } else {
      RWDTS_API_LOG_EVENT(apih, Registered, "Registration failure", apih->reg_retry);
    }

  }
  else
  {
    struct timeval tv_now, delta;
    gettimeofday(&tv_now, NULL);
    timersub(&tv_now, &api_sent_list->tv_sent, &delta);
    int elapsed = (int)(delta.tv_sec * 1000 + delta.tv_usec / 1000);
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: reg bounce after %dms",
                 apih->client_path,
                 elapsed);

    apih->stats.member_reg_update_advise_bounced++;
  }

free_and_retry:
  if (api_sent_list->reg_id) {
    free(api_sent_list->reg_id);
    api_sent_list->reg_id = NULL;
    api_sent_list->reg_count = 0;
  }
  apih->reg_outstanding = FALSE;

  if (rwdts_xact_status_get_xact_done(xact_status)) {
    RW_FREE(api_sent_list);
  }

  if (RW_SKLIST_LENGTH(&apih->sent_reg_update_list)) {
    struct rwdts_tmp_reg_data_s* tmp_reg = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
    tmp_reg->xact = NULL;
    tmp_reg->apih = apih;
    if (bounced) {
      if (apih->update_timer) {
        rwsched_dispatch_source_cancel(apih->tasklet, apih->update_timer);
        rwsched_dispatch_release(apih->tasklet, apih->update_timer);
        apih->update_timer = NULL;
      }
      rwdts_sched_update_timer(tmp_reg);
    }
    else {
      rwdts_update_registrations_to_router(tmp_reg);
    }
  }
  return;
}


static void
rwdts_send_registrations_to_router(void* ud)
{
  RW_ASSERT(ud);
  struct rwdts_tmp_reg_data_s* tmp_reg = (struct rwdts_tmp_reg_data_s*)ud;
  rwdts_api_t* apih = tmp_reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_xact_t* xact = tmp_reg->xact;

  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) member, *member_p;
  uint32_t flags = 0;
  uint64_t corrid = 1000;
  int i = 0;
  rwdts_api_reg_sent_list_t *api_sent_list = NULL;
  rwdts_api_reg_info_t *reg = NULL;
  rw_status_t rs;

  if (apih->timer &&
      (rwsched_dispatch_get_context(apih->tasklet, apih->timer) == tmp_reg)) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->timer);
    rwsched_dispatch_release(apih->tasklet, apih->timer);
    apih->timer = NULL;
  }

  if (apih->reg_outstanding) {
    rwdts_api_reg_info_t *entry = RW_SKLIST_HEAD(&(apih->sent_reg_list), rwdts_api_reg_info_t);
    while (entry) {
      rwdts_member_registration_t *reg;
      rs = RW_SKLIST_LOOKUP_BY_KEY (&(apih->reg_list), &(entry->reg_id), &reg);
      RW_ASSERT (reg);
      if (reg->reg_state != RWDTS_REG_SENT_TO_ROUTER) {
        reg->pend_outstanding++;
        if (!(reg->pend_outstanding % RWDTS_REG_PRINT_THRESH)) {
          RWTRACE_INFO(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "@%s reg_id-%d Pending too long %d: %s\n", apih->client_path,
                       reg->reg_id, reg->pend_outstanding, reg->keystr);
        }
      }
      entry = RW_SKLIST_NEXT(entry, rwdts_api_reg_info_t, element);
    }
    rwdts_sched_timer(tmp_reg);
    goto unref_xact;
  }

  member_p = &member;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member, member_p);

  strcpy(member_p->name, apih->client_path);

  member_p->n_registration = RW_SKLIST_LENGTH(&(apih->sent_reg_list));

  member_p->registration = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)**)
    calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*) * member_p->n_registration);
  member_p->has_state = 1;
  member_p->state = apih->dts_state;
  member_p->has_rtr_id = 1;
  member_p->rtr_id = apih->router_idx;


  reg = RW_SKLIST_HEAD(&(apih->sent_reg_list), rwdts_api_reg_info_t);

  while (reg) {
    if (!api_sent_list) {
      size_t rsz;
      api_sent_list = RW_MALLOC0(sizeof(rwdts_api_reg_sent_list_t));
      rsz = (RW_SKLIST_LENGTH(&(apih->sent_reg_list)) * sizeof(api_sent_list->reg_id[0]));
      api_sent_list->reg_id = realloc(api_sent_list->reg_id, rsz);
      api_sent_list->apih = apih;
    }

    member_p->registration[i] = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*)
                                 calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)));
    RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member_Registration, member_p->registration[i]);

    size_t bp_len = 0;
    const uint8_t *bp = NULL;
    rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, &bp, &bp_len);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    if (rs == RW_STATUS_SUCCESS) {
      RW_ASSERT(bp);
      member_p->registration[i]->keyspec.data = malloc(bp_len);
      memcpy(member_p->registration[i]->keyspec.data, bp, bp_len);
      member_p->registration[i]->keyspec.len = bp_len;
      member_p->registration[i]->has_keyspec = TRUE;
    }
    member_p->registration[i]->flags = reg->flags;
    member_p->registration[i]->keystr = strdup(reg->keystr);
    api_sent_list->reg_id[i] = reg->reg_id;
    api_sent_list->reg_count++;
    rwdts_member_registration_t *reg_entry;
    RW_SKLIST_LOOKUP_BY_KEY (&apih->reg_list,
                             &reg->reg_id,
                             &reg_entry);
    RW_ASSERT(reg_entry);
    RW_ASSERT(reg_entry->shard_flavor);
    member_p->registration[i]->has_flavor = 1;
    member_p->registration[i]->flavor = reg_entry->shard_flavor;
    member_p->registration[i]->has_index = reg_entry->has_index;
    member_p->registration[i]->index = reg_entry->index;
    if (reg_entry->shard_flavor == RW_DTS_SHARD_FLAVOR_RANGE) {
      member_p->registration[i]->has_start = 1;
      member_p->registration[i]->start = reg_entry->params.range.start_range;
      member_p->registration[i]->has_stop = 1;
      member_p->registration[i]->stop = reg_entry->params.range.end_range;
    }
    if (reg_entry->flags&RWDTS_FLAG_SHARDING && (reg_entry->shard_flavor == RW_DTS_SHARD_FLAVOR_IDENT)) {
      member_p->registration[i]->minikey.data = RW_MALLOC0(reg_entry->params.identparams.keylen);
      memcpy(member_p->registration[i]->minikey.data, &reg_entry->params.identparams.keyin, reg_entry->params.identparams.keylen);
      member_p->registration[i]->minikey.len = reg_entry->params.identparams.keylen;
      member_p->registration[i]->has_minikey = TRUE;
    }
    i++;
    if (reg_entry->reg_state == RWDTS_REG_SENT_TO_ROUTER) {
      reg_entry->retry_count ++;
      if (!(reg_entry->retry_count % RWDTS_REG_PRINT_THRESH)) {
        RWTRACE_INFO(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "@%s reg_id-%d Too many retries %d: %s\n", apih->client_path,
                     reg_entry->reg_id, reg_entry->retry_count, reg_entry->keystr);
      }
    }
    else {
      reg_entry->reg_state = RWDTS_REG_SENT_TO_ROUTER;
    }
    reg =  RW_SKLIST_NEXT(reg, rwdts_api_reg_info_t, element);
  }

  if (api_sent_list) {
    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_ks = NULL;

    /* register for DTS state */
    rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member),
                                   (xact)?&xact->ksi:&apih->ksi,
                                   (rw_keyspec_path_t**)&dts_ks);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    dts_ks->has_dompath = 1;
    dts_ks->dompath.path001.has_key00 = 1;
    strncpy(dts_ks->dompath.path001.key00.name,
            apih->client_path, sizeof(dts_ks->dompath.path001.key00.name));

    rwdts_event_cb_t advice_cb = {};
    advice_cb.cb = rwdts_member_reg_advise_cb;
    advice_cb.ud = api_sent_list;

    gettimeofday(&api_sent_list->tv_sent, NULL);

    flags |= RWDTS_XACT_FLAG_ADVISE;
    flags |= RWDTS_XACT_FLAG_IMM_BOUNCE;
    flags |= RWDTS_XACT_FLAG_REG;
    if (!xact) {
      flags |= RWDTS_XACT_FLAG_END;
      apih->stats.member_reg_advise_sent++;
      rs = rwdts_advise_query_proto_int(apih,
                                        (rw_keyspec_path_t *)dts_ks,
                                        NULL,
                                        (const ProtobufCMessage*)&member_p->base,
                                        corrid,
                                        NULL,
                                        RWDTS_QUERY_UPDATE,
                                        &advice_cb,
                                        flags /* | RWDTS_FLAG_TRACE */,
                                        NULL, NULL);
      if (rs != RW_STATUS_SUCCESS) {
        if (api_sent_list->reg_id) {
          free(api_sent_list->reg_id);
        }
        RW_FREE(api_sent_list);
        struct rwdts_tmp_reg_data_s* tmp_reg = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
        tmp_reg->xact = NULL;
        tmp_reg->apih = apih;

        rwdts_sched_timer (tmp_reg);
      } else {
        apih->reg_outstanding = TRUE;
      }
    } else {
      rwdts_advise_append_query_proto(xact,
                                      (rw_keyspec_path_t *)dts_ks,
                                      NULL,
                                      (const ProtobufCMessage*)&member_p->base,
                                      corrid,
                                      NULL,
                                      RWDTS_QUERY_UPDATE,
                                      &advice_cb,
                                      flags /* | RWDTS_FLAG_TRACE */,
                                      NULL);
      apih->reg_outstanding = TRUE;
    }

    rw_keyspec_path_free((rw_keyspec_path_t *)dts_ks, NULL);
  }

  RW_FREE(tmp_reg);
  RW_ASSERT(member_p == &member);
  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
unref_xact:
  if (xact) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
}

static void
rwdts_update_registrations_to_router(void* ud)
{
  RW_ASSERT(ud);
  struct rwdts_tmp_reg_data_s* tmp_reg = (struct rwdts_tmp_reg_data_s*)ud;
  rwdts_api_t* apih = tmp_reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_xact_t* xact = tmp_reg->xact;

  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) member, *member_p;
  uint32_t flags = 0;
  uint64_t corrid = 1000;
  int i = 0;
  rwdts_api_reg_sent_list_t *api_sent_list = NULL;
  rwdts_api_reg_info_t *reg = NULL;
  rw_status_t rs;

  if (apih->update_timer &&
      (rwsched_dispatch_get_context(apih->tasklet, apih->update_timer) == tmp_reg)) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->update_timer);
    rwsched_dispatch_release(apih->tasklet, apih->update_timer);
    apih->update_timer = NULL;
  }

  if (apih->reg_outstanding) {
    rwdts_api_reg_info_t *entry = RW_SKLIST_HEAD(&(apih->sent_reg_update_list), rwdts_api_reg_info_t);
    while (entry) {
      rwdts_member_registration_t *reg;
      rs = RW_SKLIST_LOOKUP_BY_KEY (&(apih->reg_list), &(entry->reg_id), &reg);
      RW_ASSERT (reg);
      if (reg->reg_state != RWDTS_REG_SENT_TO_ROUTER) {
        reg->pend_outstanding++;
        if (!(reg->pend_outstanding % RWDTS_REG_PRINT_THRESH)) {
          RWTRACE_INFO(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "@%s reg_id-%d Pending too long %d: %s\n", apih->client_path,
                       reg->reg_id, reg->pend_outstanding, reg->keystr);
        }
      }
      entry = RW_SKLIST_NEXT(entry, rwdts_api_reg_info_t, element);
    }
    rwdts_sched_update_timer(tmp_reg);
    goto unref_xact;
  }

  member_p = &member;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member, member_p);

  strcpy(member_p->name, apih->client_path);

  member_p->n_registration = RW_SKLIST_LENGTH(&(apih->sent_reg_update_list));

  member_p->registration = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)**)
    calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*) * member_p->n_registration);
  member_p->has_state = 1;
  member_p->state = apih->dts_state;
  member_p->has_rtr_id = 1;
  member_p->rtr_id = apih->router_idx;


  reg = RW_SKLIST_HEAD(&(apih->sent_reg_update_list), rwdts_api_reg_info_t);

  while (reg) {
    if (!api_sent_list) {
      size_t rsz;
      api_sent_list = RW_MALLOC0(sizeof(rwdts_api_reg_sent_list_t));
      rsz = (RW_SKLIST_LENGTH(&(apih->sent_reg_update_list)) * sizeof(api_sent_list->reg_id[0]));
      api_sent_list->reg_id = realloc(api_sent_list->reg_id, rsz);
      api_sent_list->apih = apih;
    }

    member_p->registration[i] = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*)
                                 calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)));
    RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member_Registration, member_p->registration[i]);

    size_t bp_len = 0;
    const uint8_t *bp = NULL;
    rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, &bp, &bp_len);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    if (rs == RW_STATUS_SUCCESS) {
      RW_ASSERT(bp);
      member_p->registration[i]->keyspec.data = malloc(bp_len);
      memcpy(member_p->registration[i]->keyspec.data, bp, bp_len);
      member_p->registration[i]->keyspec.len = bp_len;
      member_p->registration[i]->has_keyspec = TRUE;
    }
    member_p->registration[i]->flags = reg->flags;
    member_p->registration[i]->keystr = strdup(reg->keystr);
    api_sent_list->reg_id[i] = reg->reg_id;
    api_sent_list->reg_count++;
    rwdts_member_registration_t *reg_entry;
    RW_SKLIST_LOOKUP_BY_KEY (&apih->reg_list,
                             &reg->reg_id,
                             &reg_entry);
    RW_ASSERT(reg_entry);
    RW_ASSERT(reg_entry->shard_flavor);
    member_p->registration[i]->has_flavor = 1;
    member_p->registration[i]->flavor = reg_entry->shard_flavor;
    member_p->registration[i]->has_index = reg_entry->has_index;
    member_p->registration[i]->index = reg_entry->index;
    if (reg_entry->shard_flavor == RW_DTS_SHARD_FLAVOR_RANGE) {
      member_p->registration[i]->has_start = 1;
      member_p->registration[i]->start = reg_entry->params.range.start_range;
      member_p->registration[i]->has_stop = 1;
      member_p->registration[i]->stop = reg_entry->params.range.end_range;
    }
    if (reg_entry->flags&RWDTS_FLAG_SHARDING && (reg_entry->shard_flavor == RW_DTS_SHARD_FLAVOR_IDENT)) {
      member_p->registration[i]->minikey.data = RW_MALLOC0(reg_entry->params.identparams.keylen);
      memcpy(member_p->registration[i]->minikey.data, &reg_entry->params.identparams.keyin, reg_entry->params.identparams.keylen);
      member_p->registration[i]->minikey.len = reg_entry->params.identparams.keylen;
      member_p->registration[i]->has_minikey = TRUE;
    }
    i++;
    if (reg_entry->reg_state == RWDTS_REG_SENT_TO_ROUTER) {
      reg_entry->retry_count ++;
      if (!(reg_entry->retry_count % RWDTS_REG_PRINT_THRESH)) {
        RWTRACE_INFO(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "@%s reg_id-%d Too many retries %d: %s\n", apih->client_path,
                     reg_entry->reg_id, reg_entry->retry_count, reg_entry->keystr);
      }
    }
    else {
      reg_entry->reg_state = RWDTS_REG_SENT_TO_ROUTER;
    }
    reg =  RW_SKLIST_NEXT(reg, rwdts_api_reg_info_t, element);
  }

  if (api_sent_list) {
    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_ks = NULL;

    /* register for DTS state */
    rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member),
                                   (xact)?&xact->ksi:&apih->ksi,
                                   (rw_keyspec_path_t**)&dts_ks);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    dts_ks->has_dompath = 1;
    dts_ks->dompath.path001.has_key00 = 1;
    strncpy(dts_ks->dompath.path001.key00.name,
            apih->client_path, sizeof(dts_ks->dompath.path001.key00.name));

    rwdts_event_cb_t advice_cb = {};
    advice_cb.cb = rwdts_member_reg_update_advise_cb;
    advice_cb.ud = api_sent_list;

    gettimeofday(&api_sent_list->tv_sent, NULL);

    flags |= RWDTS_XACT_FLAG_ADVISE;
    flags |= RWDTS_XACT_FLAG_IMM_BOUNCE;
    flags |= RWDTS_XACT_FLAG_REG;
    if (!xact) {
      flags |= RWDTS_XACT_FLAG_END;
      apih->stats.member_reg_advise_sent++;
      rs = rwdts_advise_query_proto_int(apih,
                                        (rw_keyspec_path_t *)dts_ks,
                                        NULL,
                                        (const ProtobufCMessage*)&member_p->base,
                                        corrid,
                                        NULL,
                                        RWDTS_QUERY_UPDATE,
                                        &advice_cb,
                                        flags /* | RWDTS_FLAG_TRACE */,
                                        NULL, NULL);
      if (rs != RW_STATUS_SUCCESS) {
        if (api_sent_list->reg_id) {
          free(api_sent_list->reg_id);
        }
        RW_FREE(api_sent_list);
        struct rwdts_tmp_reg_data_s* tmp_reg = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
        tmp_reg->xact = NULL;
        tmp_reg->apih = apih;

        rwdts_sched_update_timer (tmp_reg);
      } else {
        apih->reg_outstanding = TRUE;
      }
    } else {
      rwdts_advise_append_query_proto(xact,
                                      (rw_keyspec_path_t *)dts_ks,
                                      NULL,
                                      (const ProtobufCMessage*)&member_p->base,
                                      corrid,
                                      NULL,
                                      RWDTS_QUERY_UPDATE,
                                      &advice_cb,
                                      flags /* | RWDTS_FLAG_TRACE */,
                                      NULL);
      apih->reg_outstanding = TRUE;
    }

    rw_keyspec_path_free((rw_keyspec_path_t *)dts_ks, NULL);
  }

  RW_FREE(tmp_reg);
  RW_ASSERT(member_p == &member);
  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
unref_xact:
  if (xact) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
}

rwdts_member_registration_t*
rwdts_member_registration_init_local(rwdts_api_t*                apih,
                               const rw_keyspec_path_t*          keyspec,
                               rwdts_member_event_cb_t*          cb,
                               uint32_t                          flags,
                               const ProtobufCMessageDescriptor* desc)
{
  rw_keyspec_path_t            *dup_ks = NULL;
  char                         *tmp_str = NULL;
  rwdts_member_registration_t  *reg = NULL;

  rw_status_t rs  = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &(dup_ks));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(dup_ks);

  reg = RW_MALLOC0_TYPE(sizeof(rwdts_member_registration_t), rwdts_member_registration_t);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  memcpy(&reg->cb, cb, sizeof(reg->cb));
  reg->desc    = desc;
  reg->flags   = flags;
  reg->apih    = apih;
  reg->keyspec = dup_ks;
  reg->reg_id  = ++apih->member_reg_id_assigned; // ATTN this should probably be assigend by the router --- check with Grant
  reg->listy   = rw_keyspec_path_is_listy(dup_ks);

  reg->reg_state   = RWDTS_REG_INIT;

  rw_dts_query_serial__init(&reg->serial);

  rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, (const uint8_t **)&reg->ks_binpath, &reg->ks_binpath_len);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(reg->ks_binpath);

  rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL ,
                                  rwdts_api_get_ypbc_schema(apih),
                                  &tmp_str);
  if (tmp_str) {
    // tmp_str is allocated on heap, no strdup needed
    reg->keystr = tmp_str;
  }

  RWDTS_API_LOG_REG_EVENT(apih, reg, RegAdded, "Added registration entry", reg->flags);

  // Initialize the  fsm state to initit, so that this can move forward.
  reg->fsm.state = RW_DTS_REGISTRATION_STATE_INIT;

  rs = RW_SKLIST_INSERT(&(apih->reg_list), reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  rwdts_init_commit_serial_list(&reg->committed_serials);
  rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg);

  return reg;
}

/*
 * init a registration
 */

rwdts_member_registration_t*
rwdts_member_registration_init(rwdts_api_t*                      apih,
                               const rw_keyspec_path_t*               keyspec,
                               const rwdts_member_event_cb_t*    cb,
                               uint32_t                          flags,
                               const ProtobufCMessageDescriptor* desc,
                               const rwdts_shard_info_detail_t* shard_detail)
{
  rw_keyspec_path_t            *dup_ks = NULL, *sent_ks = NULL;
  char                         *tmp_str = NULL;
  rwdts_member_registration_t* reg;
  rwdts_api_reg_info_t*        sent_reg = NULL;

  rw_status_t rs  = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &(dup_ks));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(dup_ks);

  rs  = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &(sent_ks));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(sent_ks);

  reg = RW_MALLOC0_TYPE(sizeof(rwdts_member_registration_t), rwdts_member_registration_t);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  memcpy(&reg->cb, cb, sizeof(reg->cb));
  reg->desc    = desc;
  reg->flags   = flags;
  reg->apih    = apih;
  reg->keyspec = dup_ks;
  reg->reg_id  = ++apih->member_reg_id_assigned; // ATTN this should probably be assigend by the router --- check with Grant
  reg->listy   = rw_keyspec_path_is_listy(dup_ks);

  reg->reg_state   = RWDTS_REG_INIT;
  if (flags & RWDTS_FLAG_SHARDING) {
    RW_ASSERT(shard_detail);
    reg->shard_flavor = shard_detail->shard_flavor;
    reg->params = shard_detail->params;
    reg->has_index = shard_detail->has_index;
    reg->index = shard_detail->index;
  }
  else {
    reg->shard_flavor = RW_DTS_SHARD_FLAVOR_NULL;
  }

  gettimeofday(&reg->tv_init, NULL);

  rw_dts_query_serial__init(&reg->serial);

  reg->fsm.state = RW_DTS_REGISTRATION_STATE_INIT;

  rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, (const uint8_t **)&reg->ks_binpath, &reg->ks_binpath_len);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(reg->ks_binpath);

  rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL ,
                              rwdts_api_get_ypbc_schema(apih),
                              &tmp_str);
  if (tmp_str) {
    // tmp_str is allocated on heap, no strdup needed
    reg->keystr = tmp_str;
  }

  RWDTS_API_LOG_REG_EVENT(apih, reg, RegAdded, "Added registration entry", reg->flags);

  rs = RW_SKLIST_INSERT(&(apih->reg_list), reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  sent_reg = RW_MALLOC0(sizeof(rwdts_api_reg_info_t));
  RW_ASSERT(sent_reg);
  sent_reg->reg_id = reg->reg_id;
  sent_reg->flags = reg->flags;
  sent_reg->keyspec = sent_ks;
  sent_reg->keystr = strdup(reg->keystr);


  RWDTS_API_LOG_REG_EVENT(apih, reg, RegAddSentList, 
                          RWLOG_ATTR_SPRINTF("rwdts_member_registration_init adding sent_reg=%p id=%u to apih->sent_reg_list\n", sent_reg, sent_reg->reg_id));

  rs = RW_SKLIST_INSERT(&(apih->sent_reg_list), sent_reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  rwdts_init_commit_serial_list(&reg->committed_serials);

  return reg;
}

/*******************************************************************************
 *                      public APIs                                            *
 *******************************************************************************/

/*
 * deinit a registration
 */

rw_status_t
rwdts_member_registration_deinit(rwdts_member_registration_t *reg)
{
  rw_status_t rs;
  rwdts_member_registration_t *removed;
  if (reg == NULL) {
    return RW_STATUS_SUCCESS;
  }
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_shard_member_delete_element(reg->shard, reg->apih->client_path, reg->flags&RWDTS_FLAG_PUBLISHER);

  rwdts_shard_deref(reg->shard);
  reg->shard = NULL;

  // Remove this registrations registered dta members
  rwdts_member_data_object_t *mobj, *tmp;

  /* if running a audit fecth timer - stop this */
  if (reg->audit.timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, reg->audit.timer);
    rwsched_dispatch_release(apih->tasklet, reg->audit.timer);
    reg->audit.timer = NULL;
  }

  HASH_ITER(hh_data, reg->obj_list, mobj, tmp) {
    // Delete this object from the hash list
    HASH_DELETE(hh_data, reg->obj_list, mobj);

    rs = rwdts_member_data_deinit(mobj);

    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  reg->obj_list = NULL;

  if (reg->pending_delete == FALSE) {
    // Remove this registration from  the API's registrations
    rs = RW_SKLIST_REMOVE_BY_KEY(&apih->reg_list,
                                 &reg->reg_id,
                                 &removed);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rwdts_api_reg_info_t* sent_reg = NULL;
    rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_list,
                                 &reg->reg_id,
                                 &sent_reg);
    if (sent_reg) {
      rw_keyspec_path_free(sent_reg->keyspec, NULL );
      free(sent_reg->keystr);
      RW_FREE(sent_reg);
    }
    sent_reg = NULL;
    rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_update_list,
                                 &reg->reg_id,
                                 &sent_reg);
    if (sent_reg) {
      rw_keyspec_path_free(sent_reg->keyspec, NULL );
      free(sent_reg->keystr);
      RW_FREE(sent_reg);
    }
    rwdts_api_reg_info_t* sent_dereg = NULL;
    rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_dereg_list,
                                 &reg->reg_id,
                                 &sent_dereg);
    if (sent_dereg) {
      rw_keyspec_path_free(sent_dereg->keyspec, NULL );
      free(sent_dereg->keystr);
      RW_FREE(sent_dereg);
    }
  }

  rwdts_appconf_register_deinit(reg);

  rwdts_member_reg_deinit(reg);

  if (reg->ingroup) {
    RW_DL_REMOVE(&reg->group.group->regs, reg, group.elem);
    reg->group.group = NULL;
    reg->ingroup = FALSE;
  }

  /*
   * Itertate through the transactions associated with this member and free
   * ATTN-- Can a registrtion go away when a transaction is in progress ?
   */

  rwdts_xact_t *xact = NULL, *xtmp = NULL;

  HASH_ITER(hh, apih->xacts, xact, xtmp) {
    rwdts_reg_commit_record_t *reg_crec = NULL;

    RW_ASSERT_TYPE(xact, rwdts_xact_t);

    rs = RW_SKLIST_LOOKUP_BY_KEY(&(xact->reg_cc_list), &reg->reg_id,
                                 (void *)&reg_crec);

    if (reg_crec) {
      rs = rwdts_member_reg_commit_record_deinit(reg_crec);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
     }
  }

  // Free any audit related data
  rwdts_audit_deinit(&reg->audit);

  // Deinit the committed serial list
  rwdts_deinit_commit_serial_list(&reg->committed_serials);

  if (reg->kv_handle) {
    //rwdts_kv_handle_file_remove(reg->kv_handle);
    RW_FREE(reg->kv_handle);
    reg->kv_handle = NULL;
  }

  if (reg->outstanding_requests) {
    reg->pending_delete = 1;
  } else {
    // Finally free the entry
    if (reg->keystr) {
      free(reg->keystr);
    }
    if (reg->keyspec) {
      rw_keyspec_path_free(reg->keyspec, NULL );
    }
    RW_FREE_TYPE(reg, rwdts_member_registration_t);
  }

  return RW_STATUS_SUCCESS;
}
/**
 * Register to DTS as a subscriber/publisher/RPC
 *
 */

static rwdts_member_reg_handle_t
rwdts_member_register_int(rwdts_xact_t*   xact,
                          rwdts_api_t*                      apih,
                          rwdts_group_t*                    group,
                          const rw_keyspec_path_t*          keyspec,
                          const rwdts_member_event_cb_t*    cb,
                          const ProtobufCMessageDescriptor* desc,
                          uint32_t                          flags,
                          const rwdts_shard_info_detail_t*  shard_detail)
{
  RW_ASSERT(apih);
  RW_ASSERT(keyspec);
  RW_ASSERT(cb);
  RW_ASSERT(desc);

  if ((apih == NULL) || (keyspec == NULL) || (cb == NULL) || (desc == NULL)) {
    return NULL;
  }

  rwdts_member_registration_t*   reg;

  if (flags & RWDTS_FLAG_DEPTH_FULL) {
    flags &= ~(RWDTS_FLAG_DEPTH_LISTS|RWDTS_FLAG_DEPTH_ONE);
  } else if (flags & RWDTS_FLAG_DEPTH_LISTS) {
    flags &= ~RWDTS_FLAG_DEPTH_ONE;
  }

  if (!(flags & (RWDTS_FLAG_DEPTH_LISTS|RWDTS_FLAG_DEPTH_ONE))) {
    flags |= RWDTS_FLAG_DEPTH_FULL;
  }

  RwSchemaCategory category = rw_keyspec_path_get_category((rw_keyspec_path_t*)keyspec);

  RW_ASSERT(category != RW_SCHEMA_CATEGORY_RPC_OUTPUT);

  if ((flags & RWDTS_FLAG_PUBLISHER) && (category == RW_SCHEMA_CATEGORY_ANY)) {
    RW_ASSERT_MESSAGE(0, "Category should not be ANY for a PUBLISHER");
  }

  if (!(flags & RWDTS_FLAG_PUBLISHER) && !(flags & RWDTS_FLAG_SUBSCRIBER)) {
    RW_ASSERT_MESSAGE(0, "Neither PUBLISHER nor SUBSCRIBER flag enabled");
  }

  if ((flags & RWDTS_FLAG_PUBLISHER) && (flags & RWDTS_FLAG_SUBSCRIBER)) {
    RW_ASSERT_MESSAGE(0, "Both PUBLISHER nor SUBSCRIBER flag enabled");
  }

  reg =  rwdts_member_registration_init(apih, keyspec, cb, flags, desc, shard_detail);
  RW_ASSERT_MESSAGE(reg, "Reg not allocated");

  if (group) {
    reg->ingroup = TRUE;
    reg->group.group = group;
    RW_DL_INSERT_HEAD(&group->regs, reg, group.elem);
  }

  apih->stats.num_registrations++;
  reg->stats.num_registrations++;

  if ((flags&RWDTS_SUB_CACHE_POPULATE_FLAG) == RWDTS_SUB_CACHE_POPULATE_FLAG)
  {
    apih->reg_cache_ct++;
  }
  if (flags & RWDTS_FLAG_SUBSCRIBER) rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  struct rwdts_tmp_reg_data_s* tmp_reg = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
  tmp_reg->xact = xact;
  tmp_reg->apih = apih;
  if (xact) {
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  rwdts_send_registrations_to_router(tmp_reg);

  rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg);

  return (rwdts_member_reg_handle_t)reg;
}

rwdts_member_reg_handle_t
rwdts_member_update_registration_int(rwdts_xact_t*   xact,
                          rwdts_api_t*                      apih,
                          rwdts_member_reg_handle_t         reg_handle,
                          const rw_keyspec_path_t*          keyspec,
                          const rwdts_member_event_cb_t*    cb,
                          uint32_t                          flags,
                          const ProtobufCMessageDescriptor* desc,
                          const rwdts_shard_info_detail_t*  shard_detail)
{
  rw_keyspec_path_t *sent_ks = NULL;

  RW_ASSERT(apih);
  RW_ASSERT(keyspec);
  RW_ASSERT(cb);
  RW_ASSERT(desc);
  RW_ASSERT(shard_detail);

  if ((apih == NULL) || (keyspec == NULL) || (cb == NULL) 
       || (desc == NULL) || (shard_detail == NULL)) {
    return NULL;
  }

  rwdts_member_registration_t*   reg = (rwdts_member_registration_t*) reg_handle;
  RwSchemaCategory category = rw_keyspec_path_get_category((rw_keyspec_path_t*)keyspec);

  RW_ASSERT(category != RW_SCHEMA_CATEGORY_RPC_OUTPUT);

  rwdts_member_registration_t *tmp_reg = NULL;
  RW_SKLIST_LOOKUP_BY_KEY (&(apih->reg_list), &(reg->reg_id), &tmp_reg);

  RW_ASSERT_MESSAGE(tmp_reg, "Reg not found");
  reg->desc    = desc;
  memcpy(&reg->cb, cb, sizeof(reg->cb));
  //reg->reg_state   = RWDTS_REG_INIT; // TBD ok?
  reg->shard_flavor = shard_detail->shard_flavor;
  reg->params = shard_detail->params;
  reg->has_index = shard_detail->has_index;
  reg->index = shard_detail->index;
  apih->stats.num_reg_updates++;
  reg->stats.num_reg_updates++;

  rw_status_t rs  = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &(sent_ks));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(sent_ks);
  rwdts_api_reg_info_t *sent_reg = 
     RW_MALLOC0(sizeof(rwdts_api_reg_info_t));
  RW_ASSERT(sent_reg);
  sent_reg->reg_id = reg->reg_id;
  reg->flags |= flags;
  sent_reg->flags = reg->flags;
  sent_reg->keyspec = sent_ks;
  sent_reg->keystr = strdup(reg->keystr);

  rs = RW_SKLIST_INSERT(&(apih->sent_reg_update_list), sent_reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);


  struct rwdts_tmp_reg_data_s* tmp_reg_data = RW_MALLOC0(sizeof(struct rwdts_tmp_reg_data_s));
  tmp_reg_data->xact = xact;
  tmp_reg_data->apih = apih;
  if (xact) {
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  rwdts_update_registrations_to_router(tmp_reg_data);

  return (rwdts_member_reg_handle_t)reg;
}

static rwdts_member_reg_handle_t
rwdts_member_register_w_group(rwdts_xact_t*                     xact,
                              rwdts_api_t*                      apih,
                              const rw_keyspec_path_t*          keyspec,
                              const rwdts_member_event_cb_t*    cb,
                              const ProtobufCMessageDescriptor* desc,
                              uint32_t                          flags,
                              const rwdts_shard_info_detail_t*  shard_detail,
                              rwdts_group_t*                    group)
{
  rwdts_registration_args_t args = {};
 
  args.xact = xact;
  args.apih = apih;
  args.group = NULL;
  args.keyspec = keyspec;
  args.cb = cb;
  args.desc = desc;
  args.flags = flags;
  args.shard_detail = shard_detail;
  args.group = group;
 
  return rwdts_member_reg_run(NULL, RW_DTS_REGISTRATION_EVT_BEGIN_REG, &args);
}

rwdts_member_reg_handle_t
rwdts_member_register(rwdts_xact_t*                     xact,
                      rwdts_api_t*                      apih,
                      const rw_keyspec_path_t*          keyspec,
                      const rwdts_member_event_cb_t*    cb,
                      const ProtobufCMessageDescriptor* desc,
                      uint32_t                          flags,
                      const rwdts_shard_info_detail_t*  shard_detail)
{
  return rwdts_member_register_w_group(xact,
                                       apih,
                                       keyspec,
                                       cb,
                                       desc,
                                       flags,
                                       shard_detail,
                                       NULL);
}

rw_status_t rwdts_shard_lookup_keystr(rwdts_shard_tab_t *shtab,
                                      void *key,
                                      uint32_t key_sz,
                                      rwdts_shard_tab_ent_t **shtabent_out)
{
  int tidx = 0;

  switch (shtab->mapping) {
  case RWDTS_SHARD_MAPPING_STRKEY:
    for (tidx=0; tidx < shtab->tab_len; tidx++) {
      rwdts_shard_tab_ent_t *ent = &shtab->tab[tidx];
      if (key_sz == ent->map.shard_key_detail.u.byte_key.k.key_len
          && 0 == memcmp(ent->map.shard_key_detail.u.byte_key.k.key, key, key_sz)) {
        *shtabent_out = ent;
        return RW_STATUS_SUCCESS;
      }
    }
    break;

  default:
    return RW_STATUS_FAILURE;
    break;
  }

  return RW_STATUS_NOTFOUND;
}

/* Keyint -> Chunk Lookup */
rw_status_t
rwdts_shard_lookup_keyint(rwdts_shard_tab_t*      shtab,
                          rwdts_shard_keyint_t    keyint,
                          rwdts_shard_tab_ent_t** shtabent_out)
{

  int tidx = 0;
  switch (shtab->mapping) {
  case RWDTS_SHARD_MAPPING_IDENT:
  case RWDTS_SHARD_MAPPING_RANGE:
    for (tidx=0; tidx < shtab->tab_len; tidx++) {
      rwdts_shard_tab_ent_t *ent = &shtab->tab[tidx];
      if (ent->map.shard_key_detail.u.range.key_first.k.key <= keyint && ent->map.shard_key_detail.u.range.key_last.k.key >= keyint) {
        *shtabent_out = ent;
        return RW_STATUS_SUCCESS;
      }
    }
    break;

  case RWDTS_SHARD_MAPPING_HASH:
    {
      tidx = keyint % shtab->tab_len;
      rwdts_shard_tab_ent_t *ent = &shtab->tab[tidx];
      *shtabent_out = ent;
      return RW_STATUS_SUCCESS;
    }
    break;

  case RWDTS_SHARD_MAPPING_STRKEY:
    return RW_STATUS_FAILURE;
    break;

  default:
    return RW_STATUS_FAILURE;
    break;
  }

  return RW_STATUS_NOTFOUND;
}

rw_status_t
rwdts_shard_add_member_registration(rwdts_shard_tab_t*              shtab,
                                    const char*                     msgpath,
                                    const rwdts_shard_key_detail_t* regdet,
                                    uint32_t*                       shard_chunk_id)
{

  int midx, tidx;
  rwdts_shard_tab_ent_t *tabent = NULL;
  size_t dsz;
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(shtab);
  RW_ASSERT(msgpath);
  RW_ASSERT(regdet);

  if (!(shtab&&msgpath&&regdet)) {
    return RW_STATUS_FAILURE;
  }

  for (midx=0; midx<shtab->member_len; midx++) {
    if (0==strcmp(msgpath, shtab->member[midx].msgpath)) {
      break;
    }
  }

  if (midx == shtab->member_len) {
    shtab->member_len++;
    size_t msz = shtab->member_len * sizeof(shtab->member[0]);
    shtab->member = realloc(shtab->member, msz);
    memset(&shtab->member[midx], 0, sizeof(shtab->member[midx]));
  }
  else {
    shtab->member[midx].refct++;
  }

  RW_ASSERT(midx < shtab->member_len);
  rwdts_shard_member_t *member = &shtab->member[midx];
  if (!member->refct) {
    RW_ASSERT(!member->msgpath);
    member->msgpath = strdup(msgpath);
  } else {
    RW_ASSERT(0==strcmp(msgpath, member->msgpath));

  }

  for (midx=0; midx<member->registration_detail_len; midx++) {
    rwdts_shard_registration_detail_t *mdet = &member->registration_detail_tab[midx];
    switch (shtab->mapping) {
    case RWDTS_SHARD_MAPPING_IDENT:
    case RWDTS_SHARD_MAPPING_HASH:
    case RWDTS_SHARD_MAPPING_RANGE:
      if (0==memcmp(&mdet->shard_key_detail.u.range, &regdet->u.range, sizeof(rwdts_shard_key_range_t))) {
        /* Dup? */
        goto dup;
      }
      break;
    case RWDTS_SHARD_MAPPING_STRKEY:
      if (regdet->u.byte_key.k.key_len == mdet->shard_key_detail.u.byte_key.k.key_len
          && 0==memcmp(mdet->shard_key_detail.u.byte_key.k.key, regdet->u.byte_key.k.key, mdet->shard_key_detail.u.byte_key.k.key_len)) {
        /* Dup? */
        goto dup;
      }
      break;
    default:
      RW_ASSERT_MESSAGE(0, "Unknown mapping %u\n", shtab->mapping);
      break;
    }
  }

  midx = member->registration_detail_len;
  member->registration_detail_len++;
  dsz = member->registration_detail_len * sizeof(member->registration_detail_tab[0]);
  member->registration_detail_tab = realloc(member->registration_detail_tab, dsz);
  RW_ASSERT(member->registration_detail_tab);
  RW_ZERO_VARIABLE(&member->registration_detail_tab[midx]);

//  memcpy(&member->registration_detail_tab[midx].shard_key_detail, regdet, sizeof(member->registration_detail_tab[0].shard_key_detail));
  member->registration_detail_tab[midx].shard_chunk_id = 0;
  if (shtab->mapping == RWDTS_SHARD_MAPPING_STRKEY) {
    //member->registration_detail_tab[midx].u.strkey.key = RW_STRDUP(regdet->u.strkey.key);
    member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key = malloc(regdet->u.byte_key.k.key_len);
    RW_ASSERT(member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key);
    memcpy(member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key, regdet->u.byte_key.k.key, regdet->u.byte_key.k.key_len);
    member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key_len = regdet->u.byte_key.k.key_len;
  }

//#error handle default entries.  member can be only default, only concrete, or both a default and concrete registration.  need to populate def_ent, etc

  /* Now add to the actual hash/lookup sharding table.  Either it's
     already got a table entry, or we add one on the end (and
     sort?) */
  switch (shtab->mapping) {
  case RWDTS_SHARD_MAPPING_STRKEY:
    rs = rwdts_shard_lookup_keystr(shtab, regdet->u.byte_key.k.key, regdet->u.byte_key.k.key_len, &tabent);
    break;
  case RWDTS_SHARD_MAPPING_IDENT:
  case RWDTS_SHARD_MAPPING_RANGE:
  case RWDTS_SHARD_MAPPING_HASH:
    rs = rwdts_shard_lookup_keyint(shtab, regdet->u.range.key_first.k.key, &tabent);
    break;
  }

  if (rs == RW_STATUS_SUCCESS) {
    RW_ASSERT(tabent);
    tidx = tabent - &(shtab->tab[0]);
    RW_ASSERT(tidx >= 0);
    RW_ASSERT(tidx < shtab->tab_len);
    *shard_chunk_id = tabent->shard_chunk_id;
    if (member->registration_detail_tab[midx].shard_chunk_id == 0) {
      member->registration_detail_tab[midx].shard_chunk_id = tabent->shard_chunk_id;
    }
  } else {
    RW_ASSERT(RWDTS_SHARD_MAPPING_HASH != shtab->mapping);
    shtab->tab_len++;
    dsz = shtab->tab_len * sizeof(shtab->tab[0]);
    shtab->tab = realloc(shtab->tab, dsz);
    tabent = &(shtab->tab[shtab->tab_len-1]);
    memset(tabent, 0, sizeof(shtab->tab[0]));
    tabent->empty = TRUE;
    shtab->next_chunkno++;
    tabent->shard_chunk_id = shtab->next_chunkno;
    *shard_chunk_id = shtab->next_chunkno;
  }

  if (member->registration_detail_tab[midx].shard_chunk_id == 0) {
    member->registration_detail_tab[midx].shard_chunk_id = shtab->next_chunkno;
  }

  if (tabent->empty) {
    /* Fill in map */
    memcpy(&tabent->map, &member->registration_detail_tab[midx], sizeof(tabent->map));
    tabent->empty = FALSE;
    if (shtab->mapping == RWDTS_SHARD_MAPPING_STRKEY) {
      tabent->map.kv_database_use = member->registration_detail_tab[midx].kv_database_use;
      tabent->map.shard_key_detail.u.byte_key.k.key = malloc(member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key_len);
      RW_ASSERT(tabent->map.shard_key_detail.u.byte_key.k.key);

      memcpy(tabent->map.shard_key_detail.u.byte_key.k.key, member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key,
             member->registration_detail_tab[midx].shard_key_detail.u.byte_key.k.key_len);
    }
  } else {
      /* lotsa assert that map is the same as reg_detail[didx] */
  }

  // find member if already here
  // else add member and bump refct
  if (tabent) {
    for (midx=0; midx<tabent->u.member_tab_len; midx++) {
      if (0==strcmp(msgpath, tabent->member_tab[midx].msgpath)) {
        goto dup;
      }
    }

    //#error  we don't seem to use member_idx ever, it's all just member_tab[]

    RW_ASSERT(midx == tabent->u.member_tab_len);
    if (midx == tabent->u.member_tab_len) {
      tabent->u.member_tab_len++;
      size_t msz = tabent->u.member_tab_len * sizeof(tabent->member_tab[0]);
      tabent->member_tab = realloc(tabent->member_tab, msz);
      memset(&tabent->member_tab[midx], 0, sizeof(tabent->member_tab[midx]));
    }

    rwdts_shard_member_t *member = &tabent->member_tab[midx];
    member->msgpath = strdup(msgpath);
  }

//#error fixme


 dup:
  return RW_STATUS_SUCCESS;
}

void
rwdts_member_get_shard_db_info_keyspec(rwdts_api_t*                apih,
                                       rw_keyspec_path_t*          keyspec,
                                       rwdts_shard_info_detail_t*  shard_detail,
                                       rwdts_shard_db_num_info_t*  shard_db_num_info)
{
  RW_ASSERT(apih);
  RW_ASSERT(keyspec);

  rw_keyspec_path_t* local_keyspec = NULL;
  rw_keyspec_path_t* dup_keyspec = NULL;
  rwdts_shard_tab_t *shard_tab = NULL;
  int depth, len;
  unsigned char strbuf[255];
  const rw_keyspec_entry_t *path_entry;
  rw_status_t rw_status;
  rwdts_shard_tab_ent_t *tabent = NULL;
  uint8_t *local_ks_binpath;
  size_t local_ks_binpath_len;
  size_t rsz;

  // Duplicate the keyspec -- ATTN there is no free keyspec API yet
  rw_status = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &local_keyspec);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
  rw_status = rw_keyspec_path_create_dup(keyspec, &apih->ksi , &dup_keyspec);
  RW_ASSERT(rw_status == RW_STATUS_SUCCESS);

  depth = rw_keyspec_path_get_depth(local_keyspec);

  if (depth > 1) {
    rw_keyspec_path_trunc_suffix_n(local_keyspec, &apih->ksi , depth-1);
    rw_status = rw_keyspec_path_get_binpath(local_keyspec, &apih->ksi, (const uint8_t **)&local_ks_binpath, &local_ks_binpath_len);
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);

    HASH_FIND(hh_shard, apih->shard_tab, local_ks_binpath, local_ks_binpath_len, shard_tab);

    if (!shard_tab) {
      return;
    }

    if (shard_tab) {
      tabent = NULL;
      rwdts_shard_lookup_keystr(shard_tab, shard_detail->shard_key_detail.u.byte_key.k.key,
                                shard_detail->shard_key_detail.u.byte_key.k.key_len, &tabent);
      if (tabent != NULL) {
        shard_db_num_info->shard_db_num_cnt++;
        rsz = shard_db_num_info->shard_db_num_cnt * sizeof(shard_db_num_info->shard_db_num[0]);
        shard_db_num_info->shard_db_num = realloc(shard_db_num_info->shard_db_num, rsz);
        shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].shard_chunk_id = tabent->shard_chunk_id;
        shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].db_number = shard_tab->table_id;
      }
    }
  }

  path_entry = rw_keyspec_path_get_path_entry(dup_keyspec, depth-1);
  len = protobuf_c_message_pack(NULL, (const ProtobufCMessage *)path_entry, &strbuf[0]);
  HASH_FIND(hh_shard, apih->shard_tab, strbuf, len, shard_tab);

  if (!shard_tab) {
    return;
  }

  if (shard_tab) {
    tabent = NULL;
    rwdts_shard_lookup_keystr(shard_tab, shard_detail->shard_key_detail.u.byte_key.k.key,
                              shard_detail->shard_key_detail.u.byte_key.k.key_len, &tabent);
    if (tabent != NULL) {
      shard_db_num_info->shard_db_num_cnt++;
      rsz = shard_db_num_info->shard_db_num_cnt * sizeof(shard_db_num_info->shard_db_num[0]);
      shard_db_num_info->shard_db_num = realloc(shard_db_num_info->shard_db_num, rsz);
      shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].shard_chunk_id = tabent->shard_chunk_id;
      shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].db_number = shard_tab->table_id;
    }
  }

  rw_keyspec_path_free(local_keyspec, NULL);
  rw_keyspec_path_free(dup_keyspec, NULL);
  return;
}

void
rwdts_member_get_shard_db_info(rwdts_api_t*               apih,
                               rwdts_shard_db_num_info_t* shard_db_num_info)
{
  RW_ASSERT(apih);
  rwdts_shard_tab_t *shard_tab = NULL, *temp_shard_tab = NULL;
  size_t rsz;

  HASH_ITER(hh_shard, apih->shard_tab, shard_tab, temp_shard_tab) {
    int i;
    for (i=0; i < shard_tab->member_len; i++) {
      if (!strcmp(shard_tab->member[i].msgpath, apih->client_path)) {
        int j;
        rsz = ((shard_db_num_info->shard_db_num_cnt + shard_tab->member[i].registration_detail_len) * sizeof(shard_db_num_info->shard_db_num[0]));
        shard_db_num_info->shard_db_num = realloc(shard_db_num_info->shard_db_num, rsz);

        for (j=0; j < shard_tab->member[i].registration_detail_len; j++) {
          shard_db_num_info->shard_db_num_cnt++;
          shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].shard_chunk_id = shard_tab->member[i].registration_detail_tab[j].shard_chunk_id;
          shard_db_num_info->shard_db_num[shard_db_num_info->shard_db_num_cnt-1].db_number = shard_tab->table_id;
        }
        break;
      }
    }
  }
  return;
}

static void
rwdts_get_shard_db_info_from_router(rwdts_api_t* apih)
{
  RW_ASSERT(apih);
  RWDtsGetDbShardInfoReq reg;

  rwdts_get_db_shard_info_req__init(&reg);

  reg.member = apih->client_path;

  RWDTS_API_LOG_EVENT(apih, SendShardInfo, "Sending Get shard info to DtsRouter");

  rwmsg_request_t *req_out = NULL;
  RW_ASSERT(apih->fetch_callbk_fn != NULL);
  rwmsg_closure_t clo = { .pbrsp = (rwmsg_pbapi_cb)apih->fetch_callbk_fn, .ud = apih };

  rwdts_member_router__get_db_shard_info(&apih->client.mbr_service,
                                         apih->client.dest,
                                         &reg,
                                         &clo,
                                         &req_out);
  return;
}

void
rwdts_get_shard_db_info_sched(rwdts_api_t* apih, void *callbk_fn)
{
  RW_ASSERT(apih);
  RW_ASSERT(callbk_fn);

  apih->fetch_callbk_fn = callbk_fn;

  rwdts_get_shard_db_info_from_router(apih);
  return;
}

void
rwdts_member_delete_shard_db_info(rwdts_api_t*    apih)
{
  RW_ASSERT(apih);
  rwdts_shard_tab_t *shard_tab = NULL, *temp_shard_tab = NULL;

  HASH_ITER(hh_shard, apih->shard_tab, shard_tab, temp_shard_tab) {
    int i, j;
    for (i=0; i < shard_tab->member_len; i++) {
      if (shard_tab->member[i].msgpath) {
        free(shard_tab->member[i].msgpath);
      }
      for (j=0; j < shard_tab->member[i].registration_detail_len; j++) {
        if (shard_tab->member[i].registration_detail_tab[j].shard_key_detail.u.byte_key.k.key) {
          free(shard_tab->member[i].registration_detail_tab[j].shard_key_detail.u.byte_key.k.key);
        }
      }
      if (shard_tab->member[i].registration_detail_tab) {
        free(shard_tab->member[i].registration_detail_tab);
      }
    }
    if (shard_tab->member) {
      free(shard_tab->member);
    }
    for (i=0; i<shard_tab->tab_len; i++) {
      if (shard_tab->tab[i].map.shard_key_detail.u.byte_key.k.key) {
        free(shard_tab->tab[i].map.shard_key_detail.u.byte_key.k.key);
      }
      if (shard_tab->tab[i].member_tab) {
        free(shard_tab->tab[i].member_tab);
      }
    }
    if (shard_tab->tab) {
      free(shard_tab->tab);
    }
  }
  return;
}

static void
rwdts_sched_dereg_timer(rwdts_api_t *apih)
{
  RW_ASSERT(apih);

  if (apih->dereg_timer != NULL) {
    return;
  }
  apih->dereg_timer = rwsched_dispatch_source_create(apih->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                    0,
                                                    0,
                                                    apih->client.rwq);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    apih->dereg_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC),
                                    0,
                                    0);

  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              apih->dereg_timer,
                                              rwdts_send_deregistrations_to_router);

  rwsched_dispatch_set_context(apih->tasklet,
                               apih->dereg_timer,
                               apih);

  rwsched_dispatch_resume(apih->tasklet,
                          apih->dereg_timer);
  return;
}

static void
rwdts_member_dereg_advise_cb(rwdts_xact_t* xact,
                              rwdts_xact_status_t* xact_status,
                              void*         ud)
{
  rwdts_api_reg_sent_list_t *api_sent_list = (rwdts_api_reg_sent_list_t *)ud;
  RW_ASSERT(api_sent_list);
  rwdts_api_t *apih = api_sent_list->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_api_reg_info_t* sent_reg = NULL;
  int i;

  // Broken callback, assumes standalone xact
  RW_ASSERT(!xact_status->block);

  if (!xact_status->xact_done) {
    return;
  }

  apih->dereg_outstanding = FALSE;
  bool bounced = xact->bounced;
  if (!xact->bounced) {
    apih->dereg_outstanding = FALSE;
    if (xact_status->xact_done) {
      RWDTS_API_LOG_EVENT(apih, RegRetry, "DTS deregistration retries still under limit - retrying",
                          apih->reg_retry);


      rw_status_t rs;
      for (i = 0; i < api_sent_list->reg_count; i++) {
         rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_dereg_list,
                                      &api_sent_list->reg_id[i],
                                      &sent_reg);
         if (rs == RW_STATUS_SUCCESS) {
           RW_KEYSPEC_PATH_FREE(sent_reg->keyspec, NULL , NULL);
           sent_reg->keyspec = NULL;
           free(sent_reg->keystr);
           RW_FREE(sent_reg);
         }
      }

      if (apih->dereg_timer) {
        rwsched_dispatch_source_cancel(apih->tasklet, apih->dereg_timer);
        rwsched_dispatch_release(apih->tasklet, apih->dereg_timer);
        apih->dereg_timer = NULL;
      }

      RWDTS_API_LOG_EVENT(apih, Registered, "DeRegistered successfully with DTS Router",
                          apih->reg_retry);
    }

    if (api_sent_list->reg_id) {
      free(api_sent_list->reg_id);
    }
    RW_FREE(api_sent_list);
  }

  if (RW_SKLIST_LENGTH(&apih->sent_dereg_list)) {
    if (bounced) {
      rwdts_sched_dereg_timer(apih);
    }
    else {
      rwdts_send_deregistrations_to_router(apih);
    }
  }
  return;
}

static void
rwdts_send_deregistrations_to_router(void* ud)
{
  RW_ASSERT(ud);
  rwdts_api_t* apih = (rwdts_api_t*)ud;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) member, *member_p;
  uint32_t flags = 0;
  uint64_t corrid = 1000;
  int i = 0;
  rwdts_api_reg_sent_list_t *api_sent_list = NULL;
  rwdts_api_reg_info_t *reg = NULL;
  rw_status_t rs;

  if (apih->dereg_outstanding) {
    rwdts_sched_dereg_timer(apih);
    return;
  }
  if (0 == apih->client_idx) {
    // No router?  This happens in some unit tests.
    return;
  }

  member_p = &member;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member, member_p);

  strcpy(member_p->name, apih->client_path);
  member_p->has_rtr_id = 1;
  member_p->rtr_id = apih->router_idx;
  RW_ASSERT(apih->client_idx);
  member_p->has_client_id = 1;
  member_p->client_id = apih->client_idx;


  member_p->n_registration = RW_SKLIST_LENGTH(&(apih->sent_dereg_list));

  member_p->registration = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)**)
    calloc(member_p->n_registration,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*));


  reg = RW_SKLIST_HEAD(&(apih->sent_dereg_list), rwdts_api_reg_info_t);

  while (reg) {
    if (!api_sent_list) {
      size_t rsz;
      api_sent_list = RW_MALLOC0(sizeof(rwdts_api_reg_sent_list_t));
      rsz = (RW_SKLIST_LENGTH(&(apih->sent_dereg_list)) * sizeof(api_sent_list->reg_id[0]));
      api_sent_list->reg_id = realloc(api_sent_list->reg_id, rsz);
      api_sent_list->apih = apih;
    }

    member_p->registration[i] = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*)
      calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)));
    RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member_Registration, member_p->registration[i]);

    member_p->registration[i]->flags = reg->flags;

    size_t bp_len = 0;
    const uint8_t *bp = NULL;
    rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, &bp, &bp_len);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    if (rs == RW_STATUS_SUCCESS) {
      RW_ASSERT(bp);
      member_p->registration[i]->keyspec.data = malloc(bp_len);
      memcpy(member_p->registration[i]->keyspec.data, bp, bp_len);
      member_p->registration[i]->keyspec.len = bp_len;
      member_p->registration[i]->has_keyspec = TRUE;
    }
    RW_ASSERT(reg->keystr);
    member_p->registration[i]->keystr = strdup(reg->keystr);
    api_sent_list->reg_id[i] = reg->reg_id;
    api_sent_list->reg_count++;
    i++;

    reg =  RW_SKLIST_NEXT(reg, rwdts_api_reg_info_t, element);
  }

  if (api_sent_list) {
    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_ks = NULL;

    /* register for DTS state */
    rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member), &apih->ksi ,
                               (rw_keyspec_path_t**)&dts_ks);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    dts_ks->has_dompath = 1;
    dts_ks->dompath.path001.has_key00 = 1;
    strncpy(dts_ks->dompath.path001.key00.name,
            apih->client_path, sizeof(dts_ks->dompath.path001.key00.name));

    rwdts_event_cb_t advice_cb = {};
    advice_cb.cb = rwdts_member_dereg_advise_cb;
    advice_cb.ud = api_sent_list;

    flags |= RWDTS_XACT_FLAG_NOTRAN;
    flags |= RWDTS_XACT_FLAG_END;
    flags |= RWDTS_XACT_FLAG_ADVISE;
    rs = rwdts_advise_query_proto_int(apih,
                                      (rw_keyspec_path_t *)dts_ks,
                                      NULL,
                                      (const ProtobufCMessage*)&member_p->base,
                                      corrid,
                                      NULL,
                                      RWDTS_QUERY_DELETE,
                                      &advice_cb,
                                      flags,
                                      NULL, NULL);
    if (rs != RW_STATUS_SUCCESS) {
      if (api_sent_list->reg_id) {
        free (api_sent_list->reg_id);
      }
      RW_FREE (api_sent_list);
      rwdts_sched_dereg_timer(apih);
    }
    apih->dereg_outstanding = TRUE;
    rw_keyspec_path_free((rw_keyspec_path_t *)dts_ks, NULL);
  }
  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
}

void rwdts_member_registration_delete_cb(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  apih->dereg_end = 1;
  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, REGISTER_CB,
                                 "Received callback for data member reg delete API triggered advise");
  return;
}

rw_status_t
rwdts_member_registration_delete(rwdts_member_registration_t *reg)
{
  rw_status_t rs;
  uint32_t flags = 0;
  uint32_t corrid = 1000;
  rwdts_member_registration_t *removed;
  if (reg == NULL) {
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_event_cb_t advice_cb = {};
  advice_cb.cb = rwdts_member_registration_delete_cb;
  advice_cb.ud = NULL;

  rwdts_deinit_cursor((rwdts_member_cursor_t *)reg->cursor);
  rwdts_shard_member_delete_element(reg->shard, reg->apih->client_path, reg->flags&RWDTS_FLAG_PUBLISHER);
  rwdts_shard_deref(reg->shard);
  reg->shard = NULL;

  if (reg->cb.cb.dtor) {
    reg->cb.cb.dtor(reg->cb.ud);
  }

  // Remove this registrations registered dta members
  rwdts_member_data_object_t *mobj, *tmp;

  HASH_ITER(hh_data, reg->obj_list, mobj, tmp) {
    // Delete this object from the hash list
    HASH_DELETE(hh_data, reg->obj_list, mobj);
    if ((reg->flags & RWDTS_FLAG_PUBLISHER) > 0) {
      flags |= RWDTS_XACT_FLAG_NOTRAN;
      flags |= RWDTS_XACT_FLAG_ADVISE;
      flags |= RWDTS_XACT_FLAG_END;
      rwdts_advise_query_proto_int(apih,
                               mobj->keyspec,
                               NULL,
                               mobj->msg,
                               corrid,
                               NULL,
                               RWDTS_QUERY_DELETE,
                               &advice_cb,
                               flags,
                               NULL, NULL);
    }

    rs = rwdts_member_data_deinit(mobj);

    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  if(reg->audit.timer)
  {
    rwsched_dispatch_source_cancel(apih->tasklet, reg->audit.timer);
    rwsched_dispatch_release(apih->tasklet, reg->audit.timer);
    reg->audit.timer = NULL;
  }
  reg->obj_list = NULL;

  // Remove this registration from  the API's registrations
  rs = RW_SKLIST_REMOVE_BY_KEY(&apih->reg_list,
                               &reg->reg_id,
                               &removed);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  rwdts_api_reg_info_t* sent_reg = NULL;
  rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_list,
                               &reg->reg_id,
                               &sent_reg);
  if (sent_reg) {
    rw_keyspec_path_free(sent_reg->keyspec, NULL );
    free(sent_reg->keystr);
    RW_FREE(sent_reg);
  }
  sent_reg = NULL;
  rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_reg_update_list,
                               &reg->reg_id,
                               &sent_reg);
  if (sent_reg) {
    rw_keyspec_path_free(sent_reg->keyspec, NULL );
    free(sent_reg->keystr);
    RW_FREE(sent_reg);
  }

  rwdts_api_reg_info_t* sent_dereg = NULL;
  rs = RW_SKLIST_REMOVE_BY_KEY(&apih->sent_dereg_list,
                               &reg->reg_id,
                               &sent_dereg);
  if (sent_dereg) {
    rw_keyspec_path_free(sent_dereg->keyspec, NULL );
    free(sent_dereg->keystr);
    RW_FREE(sent_dereg);
  }

  rwdts_appconf_register_deinit(reg);


  if (reg->kv_handle) {
    //rwdts_kv_handle_file_remove(reg->kv_handle);
    RW_FREE(reg->kv_handle);
    reg->kv_handle = NULL;
  }

  if (reg->keystr) {
    free(reg->keystr);
  }

  if (reg->keyspec) {
    rw_keyspec_path_free(reg->keyspec, NULL );
  }
 
  RW_FREE_TYPE(reg, rwdts_member_registration_t);

  return RW_STATUS_SUCCESS;
}

/*
 * Deregister a member registration
 */

rw_status_t
rwdts_member_deregister_keyspec(rwdts_api_t*  apih,
                                rw_keyspec_path_t* keyspec)
{
  rw_status_t                  status;
  uint8_t *                    local_ks_binpath = NULL;
  size_t                       local_ks_binpath_len;

  status = rw_keyspec_path_get_binpath(keyspec, &apih->ksi, (const uint8_t **)&local_ks_binpath, &local_ks_binpath_len);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  rwdts_member_registration_t* reg = NULL, *next_reg = NULL;
  reg = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (reg) {
    next_reg = RW_SKLIST_NEXT(reg, rwdts_member_registration_t, element);
    if ((reg->ks_binpath_len == local_ks_binpath_len) &&
        (memcmp(reg->ks_binpath, local_ks_binpath, local_ks_binpath_len))) {
      rwdts_member_deregister((rwdts_member_reg_handle_t)reg);
    }
    reg = next_reg;
  }

  return RW_STATUS_SUCCESS;
}

void
rwdts_member_deregister_f(rwdts_member_reg_handle_t  regh)
{
  rwdts_member_registration_t* reg = NULL;
  rwdts_member_registration_t* lookup_reg = NULL;
  rw_keyspec_path_t*                local_keyspec = NULL;
  rw_status_t                  rs;
  char                         *tmp_str = NULL;
  rwdts_api_t*                 apih = NULL;
  rwdts_api_reg_info_t*        sent_dereg = NULL;

  reg = (rwdts_member_registration_t*)regh;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rs = RW_SKLIST_LOOKUP_BY_KEY(&(apih->reg_list), &reg->reg_id,
                                    (void *)&lookup_reg);
  if (lookup_reg == NULL) {
    // ATTN - return failure ? Assert for now
    RW_ASSERT_MESSAGE(0, "Regn not found in lookup");
  }
  RW_ASSERT(reg == lookup_reg);

  rs = rw_keyspec_path_create_dup(reg->keyspec, &apih->ksi , &local_keyspec);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(local_keyspec);

  apih->stats.num_deregistrations++;
  reg->stats.num_deregistrations++;

  rw_keyspec_path_get_new_print_buffer(local_keyspec, NULL , rwdts_api_get_ypbc_schema(apih),
                                &tmp_str);
  RWDTS_API_LOG_REG_EVENT(apih, reg, RegDelKeyspec,
                          RWLOG_ATTR_SPRINTF("%s Deleted registration for keyspec [%s], registration [0x%p]", apih->client_path, tmp_str ? tmp_str : "", reg));
  free(tmp_str);

  reg->reg_state = RWDTS_REG_DEL_PENDING;
  sent_dereg = RW_MALLOC0(sizeof(rwdts_api_reg_info_t));
  sent_dereg->reg_id = reg->reg_id;
  sent_dereg->keyspec = local_keyspec;
  sent_dereg->keystr = strdup(reg->keystr);
  sent_dereg->flags = reg->flags;
  rs = RW_SKLIST_INSERT(&(apih->sent_dereg_list), sent_dereg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  rwdts_send_deregistrations_to_router(apih);
  return;
}

rw_status_t
rwdts_member_deregister(rwdts_member_reg_handle_t  regh)
{
  rwdts_member_deregister_f(regh);
  /* The unref should be called twice */
  rwdts_member_reg_handle_unref(regh);
  rwdts_member_reg_handle_unref(regh);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_deregister_gi(rwdts_member_reg_handle_t  regh)
{
  rwdts_member_deregister_f(regh);  
  rwdts_member_reg_handle_unref(regh);
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_reg_handle_deregister(rwdts_member_reg_handle_t  regh)
{
  rwdts_member_deregister_gi(regh);
  return RW_STATUS_SUCCESS;
}

static void
rwdts_sched_deregp_timer(rwdts_api_t *apih)
{
  RW_ASSERT(apih);

  if (apih->deregp_timer != NULL) {
    return;
  }
  apih->deregp_timer = rwsched_dispatch_source_create(apih->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                    0,
                                                    0,
                                                    apih->client.rwq);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    apih->deregp_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC),
                                    1ull * NSEC_PER_SEC,  /* 1 secs */
                                    0);

  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              apih->deregp_timer,
                                              rwdts_send_deregister_path);

  rwsched_dispatch_set_context(apih->tasklet,
                               apih->deregp_timer,
                               apih);

  rwsched_dispatch_resume(apih->tasklet,
                          apih->deregp_timer);
  return;
}

static void
rwdts_member_dereg_path_advise_cb(rwdts_xact_t* xact,
                                  rwdts_xact_status_t* xact_status,
                                  void*         ud)
{
  rwdts_api_dereg_path_info_t *api_dereg_info = (rwdts_api_dereg_path_info_t *)ud;
  RW_ASSERT(api_dereg_info);
  rwdts_api_t *apih = api_dereg_info->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  apih->deregp_outstanding = FALSE;
  if (!xact->bounced) {
    RWDTS_API_LOG_EVENT(apih, RegRetry, "DTS deregistration path retries still under limit - retrying",
                        apih->deregp_retry);


    rwdts_api_dereg_path_info_t *dereg = NULL, *tmp_dereg = NULL;
    HASH_ITER(hh_dereg, apih->path_dereg, dereg, tmp_dereg) {
      if (strcmp(dereg->path, api_dereg_info->path)) {
        continue;
      }
      HASH_DELETE(hh_dereg, apih->path_dereg, dereg);
      RW_FREE(dereg->path);
      RW_FREE(dereg);
      break;
    }

    if (apih->deregp_timer) {
      rwsched_dispatch_source_cancel(apih->tasklet, apih->deregp_timer);
      rwsched_dispatch_release(apih->tasklet, apih->deregp_timer);
      apih->deregp_timer = NULL;
    }

    RWDTS_API_LOG_EVENT(apih, Registered, "DeRegistered path successfully with DTS Router",
                        apih->deregp_retry);

  }

  if (apih->path_dereg) {
    rwdts_send_deregister_path(apih);
  }
  return;
}

static void
rwdts_send_deregister_path(void* ud)
{
  RW_ASSERT(ud);
  rwdts_api_t* apih = (rwdts_api_t*)ud;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) member, *member_p;
  uint32_t flags = 0;
  uint64_t corrid = 1000;
  rw_status_t rs;

  if (apih->deregp_outstanding) {
    rwdts_sched_deregp_timer(apih);
    return;
  }

  member_p = &member;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member, member_p);

  rwdts_api_dereg_path_info_t* dereg = NULL, *tmp_dereg = NULL;
  HASH_ITER(hh_dereg, apih->path_dereg, dereg, tmp_dereg) {
    strcpy(member_p->name, dereg->path);
    //member_p->all_regs = TRUE;
    //member_p->has_all_regs = TRUE;
    member_p->has_recovery_action = true;
    member_p->recovery_action = dereg->recovery_action;

    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_ks = NULL;

    /* register for DTS state */
    rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member),
                                    &apih->ksi ,
                                    (rw_keyspec_path_t**)&dts_ks);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    dts_ks->has_dompath = 1;
    dts_ks->dompath.path001.has_key00 = 1;
    strncpy(dts_ks->dompath.path001.key00.name,
            dereg->path, sizeof(dts_ks->dompath.path001.key00.name));

    rwdts_event_cb_t advice_cb = {};
    advice_cb.cb = rwdts_member_dereg_path_advise_cb;
    advice_cb.ud = dereg;

    flags |= RWDTS_XACT_FLAG_NOTRAN;
    flags |= RWDTS_XACT_FLAG_ADVISE;
    flags |= RWDTS_XACT_FLAG_END;
    rwdts_advise_query_proto_int(apih,
                             (rw_keyspec_path_t *)dts_ks,
                             NULL,
                             (const ProtobufCMessage*)&member_p->base,
                             corrid,
                             NULL,
                             RWDTS_QUERY_DELETE,
                             &advice_cb,
                             flags,
                             NULL, NULL);

    apih->deregp_outstanding = TRUE;
    rw_keyspec_path_free((rw_keyspec_path_t *)dts_ks, NULL);
    break;
  }
  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
  return;
}

rw_status_t
rwdts_member_deregister_path(rwdts_api_t *apih,
                             char* path,
                             vcs_recovery_type recovery_action)
{

  rwdts_api_dereg_path_info_t *dereg_path = NULL;

  dereg_path = RW_MALLOC0(sizeof(rwdts_api_dereg_path_info_t));

  dereg_path->path = RW_STRDUP(path);
  dereg_path->apih = apih;
  dereg_path->recovery_action = recovery_action;
  HASH_ADD_KEYPTR(hh_dereg, apih->path_dereg, dereg_path->path, strlen(dereg_path->path), dereg_path);

  rwdts_send_deregister_path(apih);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_group_register_keyspec_int(rwdts_group_t*                  group,
                             const rw_keyspec_path_t*        keyspec,
                             rwdts_member_reg_ready_callback reg_ready,
                             rwdts_member_prepare_callback   prepare,
                             rwdts_member_precommit_callback precommit,
                             rwdts_member_commit_callback    commit,
                             rwdts_member_abort_callback     abort,
                             uint32_t                        flags,
                             void*                           user_data,
                             rwdts_member_reg_handle_t*      reg_handle,
                             GDestroyNotify                  reg_deleted)
{
  const rw_yang_pb_schema_t* schema = NULL;
  rw_status_t  rs = RW_STATUS_SUCCESS;
  const rw_yang_pb_msgdesc_t *result = NULL;
  rw_keyspec_path_t *remainder_ks = NULL;
  char *ks_str = NULL;

  RW_ASSERT_TYPE(group, rwdts_group_t);
  rwdts_api_t *apih = group->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(reg_handle);

  rwdts_member_event_cb_t cb = {
    .cb = {
      .prepare   = rwdts_member_handle_prepare,
      .precommit = rwdts_member_handle_precommit,
      .commit    = rwdts_member_handle_commit,
      .abort     = rwdts_member_handle_abort,
      .reg_ready = reg_ready,
      .dtor      = reg_deleted,
    },
    .ud = user_data,
  };

  *reg_handle = NULL;

  /* Find the schema  */
  schema = (((ProtobufCMessage*)keyspec)->descriptor->ypbc_mdesc)?
            ((ProtobufCMessage*)keyspec)->descriptor->ypbc_mdesc->module->schema:
            rwdts_api_get_ypbc_schema(apih);
  if (!schema) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "register_xpath[%s] failed - Schema is NULL", ks_str);
    return (RW_STATUS_FAILURE);
  }

  rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)keyspec, NULL ,
                              schema,
                              &ks_str);

  rs = rw_keyspec_path_find_msg_desc_schema((rw_keyspec_path_t*)keyspec,
                                            NULL,
                                            schema,
                                            (const rw_yang_pb_msgdesc_t **)&result,
                                            &remainder_ks);
  if (rs != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "rw_keyspec_path_find_msg_desc_schema failed for xapth[%s]",
                 ks_str ? ks_str : "");
    free(ks_str);
    return (rs);
  }

  if (remainder_ks != NULL) {
    char *remainder_ks_str = NULL;

    rw_keyspec_path_get_new_print_buffer(remainder_ks, NULL ,
                                schema,
                                &remainder_ks_str);

    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "Unknown keyspec in xapth[%s] - remainder_keyspec = [%s]",
                 ks_str ? ks_str : "", remainder_ks_str ? remainder_ks_str : "");
    rw_keyspec_path_free(remainder_ks, NULL );
    free(remainder_ks_str);
    remainder_ks = NULL;
  }

  RW_ASSERT(result);
  *reg_handle = rwdts_member_register_w_group(group->xact,
                                              group->apih,
                                              keyspec,
                                              &cb,
                                              result->u->msg_msgdesc.pbc_mdesc,
                                              flags,
                                              NULL,
                                              group);

  if (*reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t*)(*reg_handle);

    rwdts_member_reg_t *memb_reg = RW_MALLOC0_TYPE(sizeof(*(memb_reg)), rwdts_member_reg_t);
    reg->memb_reg       = memb_reg;
    memb_reg->user_data = user_data;
    memb_reg->prepare   = prepare;
    memb_reg->precommit = precommit;
    memb_reg->commit    = commit;
    memb_reg->abort     = abort;

    free(ks_str);
    return RW_STATUS_SUCCESS;
  }

  free(ks_str);
  return RW_STATUS_FAILURE;
}

/*
 * register an xpath to a group
 */
rw_status_t
rwdts_group_register_xpath_int(rwdts_group_t*                  group,
                           const char*                     xpath,
                           rwdts_member_reg_ready_callback reg_ready,
                           rwdts_member_prepare_callback   prepare,
                           rwdts_member_precommit_callback precommit,
                           rwdts_member_commit_callback    commit,
                           rwdts_member_abort_callback     abort,
                           uint32_t                        flags,
                           void*                           user_data,
                           rwdts_member_reg_handle_t*      reg_handle,
                           GDestroyNotify                  reg_deleted)
{
  const rw_yang_pb_schema_t* schema = NULL;
  rw_keyspec_path_t *keyspec = NULL;

  RW_ASSERT_TYPE(group, rwdts_group_t);
  rwdts_api_t *apih = group->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if ((xpath == NULL) || (reg_handle == NULL)) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(xpath);
  RW_ASSERT(reg_handle);

  *reg_handle = NULL;

  /* Find the schema  */
  schema = rwdts_api_get_ypbc_schema(apih);

  /* Create keyspec from xpath */
  keyspec  = rw_keyspec_path_from_xpath(schema, (char*)xpath, RW_XPATH_KEYSPEC, &apih->ksi);
  if (!keyspec) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "keyspec from xpath failed for xpath[%s]", xpath);
    return (RW_STATUS_FAILURE);
  }

  RW_ASSERT(keyspec != NULL);

  RwSchemaCategory category = rw_keyspec_path_get_category(keyspec);

  if ((category == RW_SCHEMA_CATEGORY_ANY) &&
      (flags & RWDTS_FLAG_PUBLISHER)) {
    rw_keyspec_path_set_category(keyspec, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);
  }

  rw_status_t rs = 
     rwdts_group_register_keyspec_int(group,
                                      keyspec,
                                      reg_ready,
                                      prepare,
                                      precommit,
                                      commit,
                                      abort,
                                      flags,
                                      user_data,
                                      (rwdts_member_reg_handle_t*)reg_handle,
                                      reg_deleted);

  rw_keyspec_path_free(keyspec, NULL);
  return(rs);
}

rw_status_t
rwdts_group_register_xpath(rwdts_group_t*                  group,
                           const char*                     xpath,
                           rwdts_member_reg_ready_callback reg_ready,
                           rwdts_member_prepare_callback   prepare,
                           rwdts_member_precommit_callback precommit,
                           rwdts_member_commit_callback    commit,
                           rwdts_member_abort_callback     abort,
                           uint32_t                        flags,
                           void*                           user_data,
                           rwdts_member_reg_handle_t*      reg_handle)
{

  return(rwdts_group_register_xpath_int(group,xpath,reg_ready,prepare,precommit,
          commit,abort,flags,user_data,reg_handle,NULL));
}
rw_status_t
rwdts_group_register_xpath_gi(rwdts_group_t*                  group,
                           const char*                     xpath,
                           rwdts_member_reg_ready_callback reg_ready,
                           rwdts_member_prepare_callback   prepare,
                           rwdts_member_precommit_callback precommit,
                           rwdts_member_commit_callback    commit,
                           rwdts_member_abort_callback     abort,
                           uint32_t                        flags,
                           void*                           user_data,
                           rwdts_member_reg_handle_t*      reg_handle,
                           GDestroyNotify                  reg_deleted)
{
  rw_status_t rs =
    rwdts_group_register_xpath_int(group, xpath, reg_ready, prepare,
                           precommit, commit, abort, flags,
                           user_data, reg_handle, reg_deleted);
  if ((rs == RW_STATUS_SUCCESS) && *reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t *)(*reg_handle);
    reg->gi_app = 1;
  }
  return rs;
}


rw_status_t
rwdts_group_register_keyspec(rwdts_group_t*                  group,
                             const rw_keyspec_path_t*        keyspec,
                             rwdts_member_reg_ready_callback reg_ready,
                             rwdts_member_prepare_callback   prepare,
                             rwdts_member_precommit_callback precommit,
                             rwdts_member_commit_callback    commit,
                             rwdts_member_abort_callback     abort,
                             uint32_t                        flags,
                             void*                           user_data,
                             rwdts_member_reg_handle_t*      reg_handle)
{
  return(rwdts_group_register_keyspec_int(group,keyspec,reg_ready,prepare,
          precommit,commit,abort, flags,user_data,reg_handle,NULL));
}

rw_status_t
rwdts_group_register_keyspec_gi(rwdts_group_t*                  group,
                             const rw_keyspec_path_t*        keyspec,
                             rwdts_member_reg_ready_callback reg_ready,
                             rwdts_member_prepare_callback   prepare,
                             rwdts_member_precommit_callback precommit,
                             rwdts_member_commit_callback    commit,
                             rwdts_member_abort_callback     abort,
                             uint32_t                        flags,
                             void*                           user_data,
                             rwdts_member_reg_handle_t*      reg_handle,
                             GDestroyNotify                  reg_deleted)
{
  rw_status_t rs = 
  rwdts_group_register_keyspec_int(group, keyspec, reg_ready, prepare,
    precommit, commit, abort, flags, user_data, reg_handle, reg_deleted);
  if ((rs == RW_STATUS_SUCCESS) && (*reg_handle)) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t *) (*reg_handle);
    reg->gi_app = 1;
  }
  return rs;
}

/*
 * register member with a keyspec with additional shard params
 */

rw_status_t
rwdts_api_member_register_keyspec_shard_int(rwdts_api_t*                  apih,
                                     const rw_keyspec_path_t*         keyspec,
                                     rwdts_xact_t*                    xact,
                                     uint32_t                         flags,
                                     rwdts_shard_flavor               flavor,
                                     uint8_t                          *keyin,
                                     size_t                           keylen,
                                     int                              index,
                                     int64_t                          start,
                                     int64_t                          stop,
                                     rwdts_member_reg_ready_callback  reg_ready,
                                     rwdts_member_prepare_callback    prepare,
                                     rwdts_member_precommit_callback  precommit,
                                     rwdts_member_commit_callback     commit,
                                     rwdts_member_abort_callback      abort,
                                     void*                            user_data,
                                     rwdts_member_reg_handle_t*       reg_handle,
                                     GDestroyNotify                   dtor) 
{
  const rw_yang_pb_schema_t* schema = NULL;
  rw_status_t  rs = RW_STATUS_SUCCESS;
  char *ks_str = NULL;
  rw_yang_pb_msgdesc_t* result = NULL;
  rw_keyspec_path_t* remainder_ks = NULL;
  rwdts_shard_info_detail_t shard_detail;
  rwdts_shard_info_detail_t *shard_detail_p;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(keyspec);
  RW_ASSERT(reg_handle);
  if ((keyspec == NULL) || (reg_handle == NULL)) {
    return RW_STATUS_FAILURE;
  }
  *reg_handle = NULL;

  rwdts_member_event_cb_t cb = {
    .cb = {
      .prepare   = rwdts_member_handle_prepare,
      .precommit = rwdts_member_handle_precommit,
      .commit    = rwdts_member_handle_commit,
      .abort     = rwdts_member_handle_abort,
      .reg_ready = reg_ready,
      .dtor      = dtor, 
    },
    .ud = user_data,
  };

  /* Find the schema  */
  schema = ((ProtobufCMessage*)keyspec)->descriptor->ypbc_mdesc->module->schema;
  if (schema == NULL) {
    schema = rwdts_api_get_ypbc_schema(apih);
  }

  RW_ASSERT(keyspec != NULL);
  rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)keyspec, NULL ,
                              schema,
                              &ks_str);
  if (!schema) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "register_xpath[%s] failed - Schema is NULL", ks_str ? ks_str : "");
    free(ks_str);
    return(RW_STATUS_FAILURE);
  }

  rs = rw_keyspec_path_find_msg_desc_schema((rw_keyspec_path_t*)keyspec,
                                            NULL,
                                            schema,
                                            (const rw_yang_pb_msgdesc_t **)&result,
                                            &remainder_ks);
  if (rs != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "rw_keyspec_path_find_msg_desc_schema failed for xapth[%s]", ks_str ? ks_str : "");
    free(ks_str);
    return (rs);
  }

  if (remainder_ks != NULL) {
    char *remainder_ks_str = NULL;

    rw_keyspec_path_get_new_print_buffer(remainder_ks, NULL ,
                                schema,
                                &remainder_ks_str);

    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "Unknown keyspec in xapth[%s] - remainder_keyspec = [%s]",
                 ks_str ? ks_str : "", remainder_ks_str ? remainder_ks_str : "");
    rw_keyspec_path_free(remainder_ks, NULL);
    free(remainder_ks_str);
    remainder_ks = NULL;
  }
  RW_ASSERT(result);

  if (flavor != RW_DTS_SHARD_FLAVOR_NULL) {
    flags |= RWDTS_FLAG_SHARDING;
    shard_detail_p = &shard_detail;
    memset(shard_detail_p, 0, sizeof(shard_detail));
    shard_detail_p->shard_flavor = flavor;
    memcpy(&shard_detail_p->params.identparams.keyin[0], keyin, keylen);
    shard_detail_p->params.identparams.keylen = keylen;
    if (index != -1) {
      shard_detail_p->has_index = 1;
      shard_detail_p->index = index;
    }
    if (flavor == RW_DTS_SHARD_FLAVOR_RANGE) {
      shard_detail_p->params.range.start_range = start;
      shard_detail_p->params.range.end_range =  stop; 
    }
  }
  else {
    shard_detail_p = NULL;
  }
   // Register the keyspec
   //
  *reg_handle = rwdts_member_register(xact,
                                      apih,
                                      keyspec,
                                      &cb,
                                      result->u->msg_msgdesc.pbc_mdesc,
                                      flags,
                                      shard_detail_p);
  if (*reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t*)(*reg_handle);

    rwdts_member_reg_t *memb_reg = RW_MALLOC0_TYPE(sizeof(*(memb_reg)), rwdts_member_reg_t);
    reg->memb_reg       = memb_reg;
    memb_reg->user_data = user_data;
    memb_reg->prepare   = prepare;
    memb_reg->precommit = precommit;
    memb_reg->commit    = commit;
    memb_reg->abort     = abort;
  }

  if (*reg_handle == NULL) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "rwdts_member_register() failed for xpath %s", ks_str ? ks_str : "");
    free(ks_str);
    return (RW_STATUS_FAILURE);
  }

  free(ks_str);
  return (RW_STATUS_SUCCESS);
}

rw_status_t
rwdts_api_member_register_keyspec_shard(rwdts_api_t*                  apih,
                                     const rw_keyspec_path_t*         keyspec,
                                     rwdts_xact_t*                    xact,
                                     uint32_t                         flags,
                                     rwdts_shard_flavor               flavor,
                                     uint8_t                          *keyin,
                                     size_t                           keylen,
                                     int                              index,
                                     int64_t                          start,
                                     int64_t                          stop,
                                     rwdts_member_reg_ready_callback  reg_ready,
                                     rwdts_member_prepare_callback    prepare,
                                     rwdts_member_precommit_callback  precommit,
                                     rwdts_member_commit_callback     commit,
                                     rwdts_member_abort_callback      abort,
                                     void*                            user_data,
                                     rwdts_member_reg_handle_t*       reg_handle)
{
  return(rwdts_api_member_register_keyspec_shard_int(apih,keyspec,xact,flags,flavor,keyin,keylen,
                  index,start,stop,reg_ready,prepare,precommit,commit,abort,user_data,reg_handle,NULL));
}
 
/* gi function for register keyspec with shard 
 * 
 */ 
rw_status_t
rwdts_api_member_register_keyspec_shard_gi(rwdts_api_t*                  apih,
                                     const rw_keyspec_path_t*         keyspec,
                                     rwdts_xact_t*                    xact,
                                     uint32_t                         flags,
                                     rwdts_shard_flavor               flavor,
                                     uint8_t                          *keyin,
                                     size_t                           keylen,
                                     int                              index,
                                     int64_t                          start,
                                     int64_t                          stop,
                                     rwdts_member_reg_ready_callback  reg_ready,
                                     rwdts_member_prepare_callback    prepare,
                                     rwdts_member_precommit_callback  precommit,
                                     rwdts_member_commit_callback     commit,
                                     rwdts_member_abort_callback      abort,
                                     void*                            user_data,
                                     rwdts_member_reg_handle_t*       reg_handle,
                                     GDestroyNotify                   dtor) 
{
  rw_status_t rs = 
      rwdts_api_member_register_keyspec_shard_int(apih, keyspec, xact, flags, flavor,
                                     keyin, keylen, index, start, stop,
                                     reg_ready, prepare, precommit, commit, abort,
                                     user_data, reg_handle, dtor);
  if ((rs == RW_STATUS_SUCCESS) && *reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t *) (*reg_handle);
    reg->gi_app = 1;
  }
  return rs;
}

rw_status_t
rwdts_api_member_register_xpath_int(rwdts_api_t*                     apih,
                                const char*                      xpath,
                                rwdts_xact_t*                    xact,
                                uint32_t                         flags,
                                rwdts_shard_flavor             flavor,
                                uint8_t                          *keyin,
                                size_t                           keylen,
                                int                              index,
                                int64_t                          start,
                                int64_t                          stop,
                                rwdts_member_reg_ready_callback  reg_ready,
                                rwdts_member_prepare_callback    prepare,
                                rwdts_member_precommit_callback  precommit,
                                rwdts_member_commit_callback     commit,
                                rwdts_member_abort_callback      abort,
                                void*                            user_data,
                                rwdts_member_reg_handle_t*       reg_handle,
                                GDestroyNotify                   dtor)
{
  const rw_yang_pb_schema_t* schema = NULL;
  rw_keyspec_path_t *keyspec = NULL;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(xpath);
  RW_ASSERT(reg_handle);

  if ((xpath == NULL) || (reg_handle == NULL)) {
    return RW_STATUS_FAILURE;
  }

  *reg_handle = NULL;

  /* Find the schema  */
  schema = rwdts_api_get_ypbc_schema(apih);

  if (!schema) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "register_xpath[%s] failed - Schema is NULL", xpath);
    return(RW_STATUS_SUCCESS);
  }

  /* Create keyspec from xpath */
  keyspec  = rw_keyspec_path_from_xpath(schema, (char*)xpath, RW_XPATH_KEYSPEC, &apih->ksi);
  if (!keyspec) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "keyspec from xpath failed for xpath[%s]", xpath);
    return (RW_STATUS_FAILURE);
  }

  RW_ASSERT(keyspec != NULL);

  RwSchemaCategory category = rw_keyspec_path_get_category(keyspec);

  if ((category == RW_SCHEMA_CATEGORY_ANY) &&
      (flags & RWDTS_FLAG_PUBLISHER)) {
    rw_keyspec_path_set_category(keyspec, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);
  }

  rw_status_t rs = 
  rwdts_api_member_register_keyspec_shard_int(apih,
                                           keyspec,
                                           xact,
                                           flags,
                                           flavor,
                                           keyin,
                                           keylen,
                                           index,
                                           start,
                                           stop,
                                           reg_ready,
                                           prepare,
                                           precommit,
                                           commit,
                                           abort,
                                           user_data,
                                           reg_handle,
                                           dtor);
  rw_keyspec_path_free(keyspec, NULL);
  return(rs);
}

rw_status_t
rwdts_api_member_register_xpath(rwdts_api_t*                     apih,
                                const char*                      xpath,
                                rwdts_xact_t*                    xact,
                                uint32_t                         flags,
                                rwdts_shard_flavor             flavor,
                                uint8_t                          *keyin,
                                size_t                           keylen,
                                int                              index,
                                int64_t                          start,
                                int64_t                          stop,
                                rwdts_member_reg_ready_callback  reg_ready,
                                rwdts_member_prepare_callback    prepare,
                                rwdts_member_precommit_callback  precommit,
                                rwdts_member_commit_callback     commit,
                                rwdts_member_abort_callback      abort,
                                void*                            user_data,
                                rwdts_member_reg_handle_t*       reg_handle)
{
 return( 
   rwdts_api_member_register_xpath_int(apih,xpath,xact,flags,flavor,keyin,keylen,index,
         start,stop,reg_ready, prepare,precommit,commit,abort,user_data,reg_handle,NULL));
}

/*******************************************************************
 * The following APIs have been GIed                               *
 *******************************************************************/

/*
 * register member with an xpath
 */
rw_status_t
rwdts_api_member_register_xpath_gi(rwdts_api_t*                  apih,
                                const char*                      xpath,
                                rwdts_xact_t*                    xact,
                                uint32_t                         flags,
                                rwdts_shard_flavor               flavor,
                                uint8_t                          *keyin,
                                size_t                           keylen,
                                int                              index,
                                int64_t                          start,
                                int64_t                          stop,
                                rwdts_member_reg_ready_callback  reg_ready,
                                rwdts_member_prepare_callback    prepare,
                                rwdts_member_precommit_callback  precommit,
                                rwdts_member_commit_callback     commit,
                                rwdts_member_abort_callback      abort,
                                void*                            user_data,
                                rwdts_member_reg_handle_t*       reg_handle,
                                GDestroyNotify                   dtor)
{
  rw_status_t rs =
  rwdts_api_member_register_xpath_int(apih, xpath, xact, flags, flavor, keyin,
                                     keylen, index, start, stop, reg_ready,
                                     prepare, precommit, commit, abort, 
                                     user_data, reg_handle, dtor);
  if (rs == RW_STATUS_SUCCESS && *reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t*)(*reg_handle);
    reg->gi_app = 1;
  }
  return rs;
}
/*
 * register member with a keyspec
 */

rw_status_t
rwdts_api_member_register_keyspec(rwdts_api_t*                  apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle)
{

  return (rwdts_api_member_register_keyspec_shard_int(apih, keyspec, xact, flags,
                                                  RW_DTS_SHARD_FLAVOR_NULL,
                                                  NULL, 0, 0, 0, 0,
                                                  reg_ready, prepare,
                                                  precommit, commit,
                                                  abort, user_data,
                                                  reg_handle, NULL));
}

rw_status_t
rwdts_api_member_register_keyspec_gi(rwdts_api_t*                  apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle,
                                  GDestroyNotify                   dtor) 
{
  rw_status_t rs =   
    rwdts_api_member_register_keyspec_shard_int(apih,keyspec,xact,flags,RW_DTS_SHARD_FLAVOR_NULL,NULL,0,0,0,0,
                      reg_ready, prepare, precommit, commit, abort, user_data, reg_handle, dtor);

  if ((rs == RW_STATUS_SUCCESS) && *reg_handle) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t *) (*reg_handle);
    reg->gi_app = 1;
  }
  return rs;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_prepare(const rwdts_xact_info_t* xact_info,
                            RWDtsQueryAction         action,
                            const rw_keyspec_path_t* keyspec,
                            const ProtobufCMessage*  msg,
                            uint32_t                 credits,
                            void*                    get_next_key)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_member_reg_t *memb_reg = ((rwdts_member_registration_t*)xact_info->regh)->memb_reg;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)(xact_info->regh);
  rw_keyspec_path_t *local_ks = NULL;

  RW_ASSERT(action != RWDTS_QUERY_INVALID);

  if (reg->gi_app) { /* python lets dup the keyspec before giving to python world */
    rw_keyspec_path_create_dup(keyspec, &xact_info->xact->ksi,&local_ks);
  }
  if (memb_reg->prepare) {
    return (memb_reg->prepare)(xact_info, action, local_ks?local_ks:keyspec, msg, xact_info->ud);
  }
  return ((action == RWDTS_QUERY_READ && reg->flags|RWDTS_FLAG_NO_PREP_READ)? RWDTS_ACTION_ASYNC:RWDTS_ACTION_OK);
}

static rwdts_member_rsp_code_t
rwdts_member_handle_precommit(const rwdts_xact_info_t*      xact_info,
                              uint32_t                      n_crec,
                              const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_member_reg_t *memb_reg = ((rwdts_member_registration_t*)xact_info->regh)->memb_reg;

  if (memb_reg->precommit) {
    return (memb_reg->precommit)(xact_info, xact_info->ud);
  }
  return (RWDTS_ACTION_OK);
}

static rwdts_member_rsp_code_t
rwdts_member_handle_commit(const rwdts_xact_info_t*      xact_info,
                           uint32_t                      n_crec,
                           const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_member_reg_t *memb_reg = ((rwdts_member_registration_t*)xact_info->regh)->memb_reg;

  if (memb_reg->commit) {
    return (memb_reg->commit)(xact_info, xact_info->ud);
  }
  return (RWDTS_ACTION_OK);
}

static rwdts_member_rsp_code_t
rwdts_member_handle_abort(const rwdts_xact_info_t*      xact_info,
                          uint32_t                      n_crec,
                          const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_member_reg_t *memb_reg = ((rwdts_member_registration_t*)xact_info->regh)->memb_reg;

  if (memb_reg->abort) {
    return (memb_reg->abort)(xact_info, xact_info->ud);
  }
  return (RWDTS_ACTION_OK);
}

/*
 * temporary structure for dispatch
 */

#define RWDTS_MAX_REG_ID_PER_MATCH 32

struct rwdts_advise_args_s {
  rwdts_api_t* apih;
  uint32_t     n_reg_id;
  uint64_t     reg_id[RWDTS_MAX_REG_ID_PER_MATCH];
};

rwdts_member_rsp_code_t
rwdts_member_handle_solicit_advise_ks(const rwdts_xact_info_t* xact_info,
                                      RWDtsQueryAction         action,
                                      const rw_keyspec_path_t* key,
                                      const ProtobufCMessage*  msg,
                                      uint32_t                 credits,
                                      void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rw_status_t  rs;
  rwdts_api_t* apih = (rwdts_api_t*)xact_info->ud;
  int          i;
  rw_sklist_t  matches;
  RWPB_T_PATHSPEC(RwDts_output_SolicitAdvise) out_ks_entry;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_input_SolicitAdvise) *solicit;

  RW_DEBUG_ASSERT(action == RWDTS_QUERY_RPC);

  RW_SKLIST_PARAMS_DECL(tab_list_,rwdts_match_info_t, matchid,
                          rw_sklist_comp_uint32_t, match_elt);
  RW_SKLIST_INIT(&(matches),&tab_list_);

  solicit =  (RWPB_T_MSG(RwDts_input_SolicitAdvise)*)msg;
  RW_ASSERT(solicit);

  out_ks_entry = (*RWPB_G_PATHSPEC_VALUE(RwDts_output_SolicitAdvise));
  RWPB_M_MSG_DECL_INIT(RwDts_output_SolicitAdvise, out_msg);

  /*
   * Get the requested xpaths from the input and find  the
   * matching registrations
   */

  for (i = 0; i < solicit->n_paths; i++) {
    rw_keyspec_path_t* solicit_ks = NULL;

    if (solicit->paths[i]->keyspec.len) {
      RW_ASSERT(solicit->paths[i]->keyspec.data);
      solicit_ks = rw_keyspec_path_create_with_binpath_buf(&apih->ksi,
                                                           solicit->paths[i]->keyspec.len,
                                                           solicit->paths[i]->keyspec.data);
      if (solicit_ks == NULL) {
        RWTRACE_ERROR(apih->rwtrace_instance,
                      RWTRACE_CATEGORY_RWTASKLET,
                      "Failed to convert keyspec string  from binpath: %s",
                      solicit->paths[i]->keyspec_str);
        continue;
      }
    } else {
      rs = rwdts_api_keyspec_from_xpath(apih,
                                        solicit->paths[i]->keyspec_str,
                                        &solicit_ks);
      if (rs != RW_STATUS_SUCCESS) {
        RWTRACE_ERROR(apih->rwtrace_instance,
                      RWTRACE_CATEGORY_RWTASKLET,
                      "Failed to convert keyspec string : %s",
                      solicit->paths[i]->keyspec_str);
        continue;
      }
    }

    RW_ASSERT(solicit_ks);

    rs = rwdts_member_find_matches_for_keyspec(apih,
                                               solicit_ks,
                                               &matches,
                                               RWDTS_FLAG_PUBLISHER);
    if (rs != RW_STATUS_SUCCESS) {
      RWTRACE_DEBUG(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "%s: No matches found for keyspec: %s",
                    apih->client_path,
                    solicit->paths[i]->keyspec_str);
      continue;
    }
    rw_keyspec_path_free(solicit_ks, NULL );
  }

  rwdts_match_info_t *match = NULL;
  rwdts_member_registration_t *reg = NULL;

  struct rwdts_advise_args_s *adv_arg = RW_MALLOC0(sizeof(struct rwdts_advise_args_s));

  adv_arg->apih = apih;
  match = RW_SKLIST_HEAD((&matches), rwdts_match_info_t);
  while(match) {
    reg = match->reg;
    RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
    // Does this reg has objects to send avise?
    if (HASH_CNT(hh_data,reg->obj_list) != 0) {
      match->reg->pending_advise = 1;
    }
    adv_arg->reg_id[adv_arg->n_reg_id++] = match->reg->reg_id;
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }

  rwdts_match_list_deinit(&matches);


  /* Fill in the member name in the keyspec */
  rw_keyspec_path_t *out_ks = (rw_keyspec_path_t*)&out_ks_entry;
  out_ks_entry.has_dompath = 1;


  rs = rwdts_xact_info_respond_keyspec(xact_info,
                                       RWDTS_XACT_RSP_CODE_ACK,
                                       out_ks,
                                       &out_msg.base);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  if (rs != RW_STATUS_SUCCESS) {
    return RWDTS_ACTION_NOT_OK;
  }


  if (adv_arg->n_reg_id) {
    // Async dispatch the advise function to send advises for pending registrations
    rwsched_dispatch_async_f(apih->tasklet,
                             apih->client.rwq,
                             adv_arg,
                             rwdts_process_pending_advise);
  } else {
    RW_FREE(adv_arg);
  }

  return RWDTS_ACTION_ASYNC;
}

static void
rwdts_process_pending_advise(void *arg)
{
  struct rwdts_advise_args_s*  adv_arg =  (struct rwdts_advise_args_s*)arg;
  rwdts_member_registration_t* reg = NULL;
  rw_status_t                  rs;
  const ProtobufCMessage*      msg;
  rw_keyspec_path_t*           keyspec = NULL;
  rwdts_member_cursor_t*       cursor;
  char*                        ks_str = NULL;
  rwdts_api_t*                 apih = adv_arg->apih;
  int                          i;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  for (i = 0; i < adv_arg->n_reg_id; i++) {
    rs = RW_SKLIST_LOOKUP_BY_KEY(&apih->reg_list,
                                 &adv_arg->reg_id[i],
                                 (void *)&reg);

    if (rs != RW_STATUS_SUCCESS || reg == NULL) {
      rw_keyspec_path_get_new_print_buffer(reg->keyspec,
                                           NULL,
                                           rwdts_api_get_ypbc_schema(apih),
                                           &ks_str);
      RWTRACE_DEBUG(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "%s: No registration found with registration id, [%lu], ks [%s]",
                    apih->client_path,
                    adv_arg->reg_id[i],
                    ks_str);

      RW_FREE(ks_str);
      return;
    }

    if (reg->pending_advise == 0) {
      rw_keyspec_path_get_new_print_buffer(reg->keyspec,
                                           NULL,
                                           rwdts_api_get_ypbc_schema(apih),
                                           &ks_str);
      RWTRACE_DEBUG(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "%s: No pending advise for registration id [%lu], ks [%s]",
                    apih->client_path,
                    adv_arg->reg_id[i],
                    ks_str);
      RW_FREE(ks_str);
      return;
    }

    // Now send the advise and reset the pending advise flag

    rwdts_event_cb_t advise_cb = {};
    rwdts_memer_data_adv_event_t* member_adv_event = rwdts_memer_data_adv_init(&advise_cb,
                                                                               reg,
                                                                               true,
                                                                               true);
    RW_ASSERT_TYPE(member_adv_event, rwdts_memer_data_adv_event_t);

    advise_cb.ud = member_adv_event;
    advise_cb.cb = rwdts_member_data_advise_cb;

    cursor = rwdts_member_data_get_current_cursor(NULL,
                                                  (rwdts_member_reg_handle_t)reg);
    // Iterate through the registration's obj list and
    // trigger update advises

    rwdts_xact_t *xact;
    xact = rwdts_api_xact_create(reg->apih, 
                                 RWDTS_XACT_FLAG_ADVISE,  
                                 advise_cb.cb, 
                                 advise_cb.ud);

    rwdts_xact_block_t* block = rwdts_xact_block_create(xact);

    while ((msg = rwdts_member_reg_handle_get_next_element(
                                                          (rwdts_member_reg_handle_t)reg,
                                                          cursor, NULL,
                                                          &keyspec))) {
      rwdts_member_data_advice_query_action_xact(xact,
                                                 block,
                                                 (rwdts_member_reg_handle_t)reg,
                                                 keyspec,
                                                 msg,
                                                 RWDTS_QUERY_UPDATE,
                                                 RWDTS_XACT_FLAG_ADVISE|RWDTS_XACT_FLAG_SOLICIT_RSP,
                                                 false);
    }

    rwdts_xact_block_execute(block, 
                             RWDTS_XACT_FLAG_ADVISE|RWDTS_XACT_FLAG_SOLICIT_RSP|RWDTS_XACT_FLAG_END, 
                             NULL,
                             NULL, 
                             NULL);
     
    reg->pending_advise = 0;
  }

  // Consume the arg
  RW_FREE(adv_arg); adv_arg = arg = NULL;

  return;
}

void
rwdts_member_reg_handle_inc_serial(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  reg->serial.serial++;
  reg->serial.has_serial = 1;
}

rw_status_t
rwdts_member_reg_handle_update_pub_serial(rwdts_member_registration_t* reg,
                                          RwDtsQuerySerial*            ps)
{
  rwdts_pub_serial_t *pub_serial = NULL;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(ps);
  if (!ps) {
    return RW_STATUS_FAILURE;
  }
  rwdts_pub_identifier_t pub_id;

  pub_id.member_id =  ps->id.member_id;
  pub_id.router_id =  ps->id.router_id;
  pub_id.pub_id    =  ps->id.pub_id;


  if (ps->has_serial) {
    pub_serial = rwdts_pub_serial_init(reg, &pub_id);
    RW_ASSERT_TYPE(pub_serial, rwdts_pub_serial_t);

    if (ps->serial > pub_serial->serial) {
      pub_serial->serial = ps->serial;
    }
  } else {
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


rwdts_pub_serial_t*
rwdts_pub_serial_init(rwdts_member_registration_t* reg,
                      rwdts_pub_identifier_t*      pub_id)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(pub_id);
  if (!pub_id) {
    return NULL;
  }
  rwdts_pub_serial_t *pub_serial = NULL;

  HASH_FIND(hh,
            reg->pub_serials,
            pub_id,
            sizeof(*pub_id),
            pub_serial);

  if (pub_serial == NULL) {
    pub_serial = RW_MALLOC0_TYPE(sizeof(rwdts_pub_serial_t), rwdts_pub_serial_t);
    pub_serial->id =  *pub_id;
    pub_serial->reg = reg;
    HASH_ADD(hh, reg->pub_serials, id, sizeof(pub_serial->id), pub_serial);
  }
  return pub_serial;
}

void
rwdts_pub_serial_deinit(rwdts_pub_serial_t *pub_serial)
{
  rwdts_pub_serial_t *tmp = NULL;
  RW_ASSERT_TYPE(pub_serial, rwdts_pub_serial_t);
  rwdts_member_registration_t *reg = pub_serial->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  HASH_FIND(hh,
            reg->pub_serials,
            &pub_serial->id,
            sizeof(pub_serial->id),
            tmp);
  RW_ASSERT(tmp == pub_serial);
  HASH_DEL(reg->pub_serials, tmp);
  RW_FREE_TYPE(tmp, rwdts_pub_serial_t);

  return;
}

void
rwdts_pub_serial_hash_deinit(rwdts_member_registration_t *reg)
{
  rwdts_pub_serial_t *ps, *tmp;

  HASH_ITER(hh, reg->pub_serials, ps, tmp) {
    HASH_DEL(reg->pub_serials, ps);
     RW_FREE_TYPE(tmp, rwdts_pub_serial_t);
  }

  return;
}


rw_status_t
rwdts_update_registration_serials(rwdts_xact_t *xact)
{
  rwdts_reg_commit_record_t *reg_commit = NULL;
  int i;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  reg_commit = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);

  while(reg_commit != NULL) {
    RW_ASSERT(reg_commit->xact == xact);
    for (i =  0; i < reg_commit->n_commits; i++) {
      if (reg_commit->commits[i]->serial) {
        rwdts_member_reg_handle_update_pub_serial(reg_commit->reg, reg_commit->commits[i]->serial);
      }
    }
    reg_commit = RW_SKLIST_NEXT(reg_commit, rwdts_reg_commit_record_t, element);
  }
  return RW_STATUS_SUCCESS;
}

/*
static int
rw_sklist_comp_serial(void *k1, void *k2)
{
  RW_ASSERT(k1);
  RW_ASSERT(k1);

  rwdts_pub_serial_t *p1 = (rwdts_pub_serial_t*)k1;
  rwdts_pub_serial_t *p2 = (rwdts_pub_serial_t*)k2;

  if (p1->serial > p2->serial) {
    return 1;
  }
  if (p1->serial < p2->serial) {
    return -1;
  }
  return 0;
}
*/

void
rwdts_init_commit_serial_list(rw_sklist_t* skl)
{
  RW_ASSERT(skl);
  RW_SKLIST_PARAMS_DECL(sk_params,rwdts_pub_serial_t, serial,
                        rw_sklist_comp_uint64_t, element);
  RW_SKLIST_INIT(skl,  &sk_params);

  return;
}

void
rwdts_deinit_commit_serial_list(rw_sklist_t* skl)
{
  RW_ASSERT(skl);
  rwdts_pub_serial_t* entry;
  rwdts_pub_serial_t* removed_entry;
  while ((entry = RW_SKLIST_HEAD(skl, rwdts_pub_serial_t))) {
    RW_SKLIST_REMOVE_BY_KEY(skl,
                            &entry->serial,
                            &removed_entry);
    RW_ASSERT(removed_entry == entry);
    RW_FREE_TYPE(removed_entry, rwdts_pub_serial_t);
  }
  return;
}

void
rwdts_add_entry_to_commit_serial_list(rw_sklist_t*              skl,
                                      const rwdts_pub_serial_t* entry,
                                      uint64_t*                 high_commit_serial)
{
  rwdts_pub_serial_t* new_entry;
  rwdts_pub_serial_t* next_entry;
  rwdts_pub_serial_t* remove_entry;
  rwdts_pub_serial_t* removed_entry;
  RW_ASSERT(skl);
  RW_ASSERT(entry);

  RW_SKLIST_LOOKUP_BY_KEY(skl,
                          &entry->serial,
                          &new_entry);

  if (new_entry != NULL) {
    // Serial already in list -- return
    return;
  }

  new_entry = RW_MALLOC0_TYPE(sizeof(rwdts_pub_serial_t), rwdts_pub_serial_t);
  new_entry->id  = entry->id;
  new_entry->reg = entry->reg;
  new_entry->serial =  entry->serial;
 

  // If  the currently added serial is in sequnce
  // with the current number of serials, compact
  // until the last.

  RW_ASSERT(entry->serial > (*high_commit_serial));
  RW_SKLIST_INSERT(skl, new_entry);

  if (entry->serial == ((*high_commit_serial)+1)) {
    remove_entry = NULL;
    *high_commit_serial = entry->serial;
    for (next_entry = RW_SKLIST_HEAD(skl, rwdts_pub_serial_t);
         next_entry && next_entry->serial < entry->serial;
         next_entry = RW_SKLIST_NEXT(next_entry, rwdts_pub_serial_t, element)) {
      if (remove_entry) {
        RW_SKLIST_REMOVE_BY_KEY(skl,
                              &remove_entry->serial,
                              &removed_entry);
        RW_ASSERT(removed_entry == remove_entry);
        RW_FREE_TYPE(removed_entry, rwdts_pub_serial_t);
      }
      remove_entry = next_entry;
    }
    if (remove_entry) {
      RW_SKLIST_REMOVE_BY_KEY(skl,
                              &remove_entry->serial,
                              &removed_entry);
      RW_ASSERT(removed_entry == remove_entry);
      RW_FREE_TYPE(removed_entry, rwdts_pub_serial_t);
      removed_entry = remove_entry = NULL;
    }
  }

  RW_SKLIST_INSERT(skl, new_entry);

  remove_entry = RW_SKLIST_HEAD(skl, rwdts_pub_serial_t);
  while (remove_entry) {
     next_entry = RW_SKLIST_NEXT(remove_entry, rwdts_pub_serial_t, element);
     RW_ASSERT(remove_entry->serial >= (*high_commit_serial));
     if (next_entry &&
         next_entry->serial == (*high_commit_serial+1)) {
      *high_commit_serial = next_entry->serial;
       RW_SKLIST_REMOVE_BY_KEY(skl,
                               &remove_entry->serial,
                               &removed_entry);
       RW_ASSERT(removed_entry == remove_entry);
       remove_entry = next_entry;
       RW_FREE_TYPE(removed_entry, rwdts_pub_serial_t);
    } else {
      break;
    }
  }
  return;
}

void
dump_commit_serial_list(rw_sklist_t* skl)
{
  RW_ASSERT(skl);

  rwdts_pub_serial_t* sl;

  fprintf(stderr, "Serial commit list [%p] {\n", skl);
  RW_SKLIST_FOREACH(sl, rwdts_pub_serial_t, skl, element) {
    fprintf(stderr, "%lu ", sl->serial);
  }
  fprintf(stderr, "\n}\n");
  return;
}

/*
 *  Registration FSM functions
 */

static rw_status_t
rwdts_member_reg_add_registration(rwdts_registration_args_t*    arg,
                                  rwdts_member_registration_t** reg)
{
  RW_ASSERT(reg);
  if (reg == NULL) {
    return RW_STATUS_FAILURE;
  }

  *reg = (rwdts_member_registration_t*)
         rwdts_member_register_int(arg->xact,
                                   arg->apih,
                                   arg->group,
                                   arg->keyspec,
                                   arg->cb,
                                   arg->desc,
                                   arg->flags,
                                   arg->shard_detail);

  if (*reg) {
    rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)*reg);
  }
  return ((*reg)?RW_STATUS_SUCCESS:RW_STATUS_FAILURE);
}

static inline void
rwdts_member_reg_fsm_record_event(rwdts_member_registration_t*        reg,
                                  RwDts__YangEnum__RegistrationEvt__E evt)
{
  if (reg->fsm.n_trace >= RWDTS_MAX_FSM_TRACE) {
    return;
  }
  reg->fsm.trace[reg->fsm.n_trace].state = reg->fsm.state;
  reg->fsm.trace[reg->fsm.n_trace].event = evt;
  reg->fsm.n_trace++;
  return;
}

static inline void
rwdts_member_reg_transition(rwdts_member_registration_t*          reg,
                           RwDts__YangEnum__RegistrationEvt__E    evt,
                            RwDts__YangEnum__RegistrationState__E state)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  if (reg->fsm.n_trace < RWDTS_MAX_FSM_TRACE) {
    reg->fsm.trace[reg->fsm.n_trace].state = reg->fsm.state;
    reg->fsm.trace[reg->fsm.n_trace++].event = evt;
  }
  reg->fsm.state = state;

}


static void
member_member_reg_handle_advise_solicit_rpc_cb(rwdts_xact_t*           xact,
                                               rwdts_xact_status_t*    xact_status,
                                               void*                   user_data)
{
  if (xact_status->xact_done) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t*)user_data;
    RW_ASSERT_TYPE(reg,  rwdts_member_registration_t);

    rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_SOLICIT_RSP, xact);
  }
 return;
}


rw_status_t
rwdts_member_reg_handle_solicit_advise(rwdts_member_reg_handle_t  regh)
{
  rwdts_xact_t *xact;
  rwdts_api_t *apih;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWPB_T_PATHSPEC(RwDts_input_SolicitAdvise) solicit_advise_ks =
    *(RWPB_G_PATHSPEC_VALUE(RwDts_input_SolicitAdvise));
  RWPB_M_MSG_DECL_INIT(RwDts_input_SolicitAdvise, solicit_advise_msg);
  RWPB_T_MSG(RwDts_input_SolicitAdvise_Paths)* path_array[1];
  RWPB_M_MSG_DECL_INIT(RwDts_input_SolicitAdvise_Paths, path);

  solicit_advise_msg.paths = (RWPB_T_MSG(RwDts_input_SolicitAdvise_Paths)**)path_array;
  solicit_advise_msg.paths[0] = &path;
  solicit_advise_msg.n_paths = 1;
  path.keyspec_str = reg->keystr;
  path.keyspec.len = reg->ks_binpath_len;
  path.keyspec.data = reg->ks_binpath;
 
  rwdts_pub_serial_t* sl = NULL;

  // Add the serials associated with this registration
  RW_SKLIST_FOREACH(sl, rwdts_pub_serial_t, &reg->committed_serials, element) {
    path.serials[path.n_serials]->has_serial_num = 1;
    path.serials[path.n_serials]->serial_num = sl->serial;
    path.n_serials++;
  }
 
  xact = rwdts_api_query_ks(apih,
                            (rw_keyspec_path_t*)&solicit_advise_ks,
                            RWDTS_QUERY_RPC,
                            RWDTS_XACT_FLAG_BLOCK_MERGE|RWDTS_XACT_FLAG_END,
                            member_member_reg_handle_advise_solicit_rpc_cb,
                            regh,
                            &solicit_advise_msg.base);
  RW_ASSERT(xact);

  return xact?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
}

static rw_status_t
rwdts_member_reg_begin_reg(rwdts_member_registration_t*        reg,
                           RwDts__YangEnum__RegistrationEvt__E evt,
                           void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Begin Registration id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_REGISTERING);

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_begin_sync(rwdts_member_registration_t*        reg,
                            RwDts__YangEnum__RegistrationEvt__E evt,
                            void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Begin Sync reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));
 
  // Trigger a solicit to begin synchronization with publishers/datastore
  // ATTN for now we will skip  this for publisher registrations

  reg->in_sync = false;

#if RWDTS_ENABLED_LIVE_RECOVERY
  if (!(reg->flags & RWDTS_FLAG_SUBSCRIBER) ||
      !(reg->flags & RWDTS_SUB_CACHE_POPULATE_FLAG)) {
    // transition to the REG READY STATE
    reg->in_sync = true;
    rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_REG_READY);
  } else {
    rwdts_member_reg_handle_solicit_advise(rwdts_member_registration_to_handle(reg));
    rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_WAITING_SYNC);
  }
#else /* RWDTS_ENABLED_LIVE_RECOVERY */
  // ATTN - Fix this -- for now short circuit fsm
  rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_REG_READY);
#endif /* RWDTS_ENABLED_LIVE_RECOVERY */

  return RW_STATUS_SUCCESS;
}

rwdts_member_reg_handle_t
rwdts_member_registration_to_handle(rwdts_member_registration_t* reg)
{
  return ((rwdts_member_reg_handle_t) reg);
}

rwdts_member_registration_t*                                                    
rwdts_member_reg_handle_to_reg(rwdts_member_reg_handle_t regh)
{
  return ((rwdts_member_registration_t*)regh);
}

static rw_status_t
rwdts_member_reg_router_registered(rwdts_member_registration_t*        reg,
                                   RwDts__YangEnum__RegistrationEvt__E evt,
                                   void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Router registered reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  // Registered with the router - trigger syncing
  // with DTS
  rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_BEGIN_SYNC, ud);

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_solicit_req(rwdts_member_registration_t*        reg,
                             RwDts__YangEnum__RegistrationEvt__E evt,
                             void*                               ud)
{
  rw_status_t rs;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Solicit req sent for reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  // Trigger an advise solicit  from  the publishers
  rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_WAITING_SYNC);
  rs = rwdts_member_reg_handle_solicit_advise((rwdts_member_reg_handle_t)reg);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  return rs;
}


static rw_status_t
rwdts_member_reg_ready(rwdts_member_registration_t*        reg,
                       RwDts__YangEnum__RegistrationEvt__E evt,
                       void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Registration is ready for reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_REG_READY);

  return RW_STATUS_SUCCESS;
}

static bool
rwdts_is_registration_in_sync(rwdts_member_registration_t* reg)
{
  if (reg->in_sync) {
    return true;
  }
  return false;
}

static rw_status_t
rwdts_member_reg_solicit_rsp(rwdts_member_registration_t*        reg,
                             RwDts__YangEnum__RegistrationEvt__E evt,
                             void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  int i;

  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Solicit advise rsp rcvd for reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  rwdts_xact_t *xact = (rwdts_xact_t*)ud;
 
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);
  RW_ASSERT(qrslt);
  if (!qrslt) {
    return RW_STATUS_FAILURE;
  }

  RWPB_T_MSG(RwDts_output_SolicitAdvise)* rpc_out;
 
  rpc_out = (RWPB_T_MSG(RwDts_output_SolicitAdvise)*) (qrslt->message);

  RW_ASSERT(rpc_out->base.descriptor == RWPB_G_MSG_PBCMD(RwDts_output_SolicitAdvise));

  /* Walk through the list of serials from the publishers and add it to
   * the list of serials expected from this registration
   */
  for (i = 0; i < rpc_out->n_publishers; i++) {
    rwdts_pub_identifier_t pub_serial;

    pub_serial.member_id = rpc_out->publishers[i]->member_id;
    pub_serial.router_id = rpc_out->publishers[i]->router_id;
    pub_serial.pub_id    = rpc_out->publishers[i]->pub_id;
    
    RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Received RPC o/p for %lu:%lu:%lu",
               apih->client_path, pub_serial.member_id, pub_serial.router_id, pub_serial.pub_id);
    /* ATTN -- fix this add this to a list */
  }

  if (rwdts_is_registration_in_sync(reg)) {
    rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_IN_SYNC);
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_advise(rwdts_member_registration_t*        reg,
                        RwDts__YangEnum__RegistrationEvt__E evt,
                        void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Advise rcvd for reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  rwdts_match_info_t *match = (rwdts_match_info_t*)ud;
  RW_ASSERT_TYPE(match, rwdts_match_info_t);

  if (rwdts_is_registration_in_sync(reg)) {
    RWTRACE_DEBUG(apih->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET,
                  "The registrations are in sync --- start recovering");
    rwdts_member_reg_transition(reg, evt, RW_DTS_REGISTRATION_STATE_IN_SYNC);
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_reconcile(rwdts_member_registration_t*        reg,
                           RwDts__YangEnum__RegistrationEvt__E evt,
                           void*                               ud)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Reconcile reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_deregister(rwdts_member_registration_t*        reg,
                            RwDts__YangEnum__RegistrationEvt__E evt,
                            void*                               ud)
{
  rw_status_t rs;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih;
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_DEBUG(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Deregister reg id[%d] key[%s] evt [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_registration_evt_str(evt),
               rwdts_registration_state_str(reg->fsm.state));

  // ATTN - relook at this since deregister may need to check for current state
  rs = rwdts_member_deregister((rwdts_member_reg_handle_t)reg);
  return rs;
}

/*
 * Run the registration fsm
 */
rwdts_member_reg_handle_t
rwdts_member_reg_run(rwdts_member_registration_t*        reg,
                     RwDts__YangEnum__RegistrationEvt__E evt,
                     void*                               ud)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  // If the regn passed is NULL this is a brand new registration.
  // Using the passed arguments create and add a new registration.
  if (reg == NULL) {
    rwdts_registration_args_t *arg = (rwdts_registration_args_t*)ud;
    RW_ASSERT(evt == RW_DTS_REGISTRATION_EVT_BEGIN_REG);
    rs = rwdts_member_reg_add_registration(arg, &reg);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_ASSERT(reg->fsm.state == RW_DTS_REGISTRATION_STATE_INIT);
  }

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_member_registration_fsm_t* fsm = &reg->fsm;

  rwdts_api_t* apih =  reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_reg_fsm_record_event(reg, evt);

  switch(fsm->state) {
    case RW_DTS_REGISTRATION_STATE_INIT:
      if (evt == RW_DTS_REGISTRATION_EVT_BEGIN_REG) {
        rs = rwdts_member_reg_begin_reg(reg, evt, NULL);
      } else if  (evt == RW_DTS_REGISTRATION_EVT_DEREGISTER) {
        rs = rwdts_member_reg_deregister(reg, evt, ud);
      } else {
       goto invalid_trans;
      }
      break;

    case RW_DTS_REGISTRATION_STATE_REGISTERING:
      if (evt == RW_DTS_REGISTRATION_EVT_REGISTERED) {
        rs = rwdts_member_reg_router_registered(reg, evt, NULL);
      } else if (evt == RW_DTS_REGISTRATION_EVT_DEREGISTER) {
        rs = rwdts_member_reg_deregister(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_BEGIN_SYNC) {
        rs = rwdts_member_reg_begin_sync(reg, evt, NULL);
      } else {
       goto invalid_trans;
      }
      break;

    case RW_DTS_REGISTRATION_STATE_WAITING_SYNC:
      if (evt == RW_DTS_REGISTRATION_EVT_BEGIN_SYNC) {
        rs = rwdts_member_reg_begin_sync(reg, evt, NULL);
      } else if (evt == RW_DTS_REGISTRATION_EVT_SOLICIT_REQ) {
        rs = rwdts_member_reg_solicit_req(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_SOLICIT_RSP) {
        rs = rwdts_member_reg_solicit_rsp(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_ADVISE) {
        rs = rwdts_member_reg_advise(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_DEREGISTER) {
        rs = rwdts_member_reg_deregister(reg, evt, ud);
      } else {
       goto invalid_trans;
      }
      break;

    case RW_DTS_REGISTRATION_STATE_IN_SYNC:
      if (evt == RW_DTS_REGISTRATION_EVT_SOLICIT_REQ) {
         rs = rwdts_member_reg_solicit_req(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_READY) {
         rs = rwdts_member_reg_ready(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_DEREGISTER) {
         rs = rwdts_member_reg_deregister(reg, evt, ud);
      } else {
        goto invalid_trans;
      }
      break;

    case RW_DTS_REGISTRATION_STATE_RECONCILE:
      RW_CRASH(); // ATTN - implement this
      break;

    case RW_DTS_REGISTRATION_STATE_REG_READY:
      if (evt == RW_DTS_REGISTRATION_EVT_RECONCILE) {
         rs = rwdts_member_reg_reconcile(reg, evt, ud);
      } else if (evt == RW_DTS_REGISTRATION_EVT_DEREGISTER) {
         rs = rwdts_member_reg_deregister(reg, evt, ud);
      } else {
        goto invalid_trans;
      }
      break;

    default:
     
      rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_DEREGISTER, ud);

      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: Invalid transition for reg_id %d ,"
                   "keys[%s]- invalid event [%s] in  state [%s]",
                   apih->client_path,
                   reg->reg_id, reg->keystr,
                   rwdts_registration_evt_str(evt),
                   rwdts_registration_state_str(reg->fsm.state));
      break;
  }
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  return (rwdts_member_reg_handle_t) reg;

invalid_trans:

  return NULL;
}

static void
rwdts_member_promote_advise_cb(rwdts_xact_t* xact,
                               rwdts_xact_status_t* xact_status,
                               void*         ud)
{
  rwdts_member_registration_t* reg = (rwdts_member_registration_t*)ud;
  reg->flags &= ~RWDTS_FLAG_SUBSCRIBER;;
  reg->flags |= RWDTS_FLAG_PUBLISHER;
  fprintf(stderr, "Inside rwdts_member_promote_advise_cb\n");
  return;
}

void
rwdts_send_sub_promotion_to_router(void *ud)
{
  RW_ASSERT(ud);
  rwdts_member_registration_t* reg = (rwdts_member_registration_t*)ud;

  rwdts_api_t* apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) member, *member_p;
  uint32_t flags = 0;
  uint64_t corrid = 6666;
  int i = 0;
  rw_status_t rs;

  member_p = &member;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member, member_p);

  strcpy(member_p->name, apih->client_path);

  member_p->n_registration = 1;
  member_p->promote_sub = 1;
  member_p->has_promote_sub = 1;

  member_p->registration = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)**)
    calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*) * member_p->n_registration);
  member_p->has_state = 1;
  member_p->state = apih->dts_state;
  member_p->has_rtr_id = 1;
  member_p->rtr_id = apih->router_idx;


  member_p->registration[i] = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)*)
                               calloc(1,sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)));
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member_Registration, member_p->registration[i]);

  size_t bp_len = 0;
  const uint8_t *bp = NULL;
  rs = rw_keyspec_path_get_binpath(reg->keyspec, &apih->ksi, &bp, &bp_len);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  if (rs == RW_STATUS_SUCCESS) {
    RW_ASSERT(bp);
    member_p->registration[i]->keyspec.data = malloc(bp_len);
    memcpy(member_p->registration[i]->keyspec.data, bp, bp_len);
    member_p->registration[i]->keyspec.len = bp_len;
    member_p->registration[i]->has_keyspec = TRUE;
  }
  member_p->registration[i]->keystr = strdup(reg->keystr);

  RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_ks = NULL;

  /* register for DTS state */
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member),
                                  &apih->ksi,
                                  (rw_keyspec_path_t**)&dts_ks);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  dts_ks->has_dompath = 1;
  dts_ks->dompath.path001.has_key00 = 1;
  strncpy(dts_ks->dompath.path001.key00.name,
          apih->client_path, sizeof(dts_ks->dompath.path001.key00.name));

  rwdts_event_cb_t advice_cb = {};
  advice_cb.cb = rwdts_member_promote_advise_cb;
  advice_cb.ud = reg;

  flags |= RWDTS_XACT_FLAG_IMM_BOUNCE;
  flags |= RWDTS_XACT_FLAG_REG;
  flags |= RWDTS_XACT_FLAG_END;
  apih->stats.member_reg_advise_sent++;
  rs = rwdts_advise_query_proto_int(apih,
                                    (rw_keyspec_path_t *)dts_ks,
                                    NULL,
                                    (const ProtobufCMessage*)&member_p->base,
                                    corrid,
                                    NULL,
                                    RWDTS_QUERY_UPDATE,
                                    &advice_cb,
                                    flags /* | RWDTS_FLAG_TRACE */,
                                    NULL, NULL);

  rw_keyspec_path_free((rw_keyspec_path_t *)dts_ks, NULL);

  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
  return;
}
