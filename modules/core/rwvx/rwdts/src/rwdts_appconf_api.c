
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file   rwdts_appconf_api.c
 * @author RIFT.io <info@riftio.com>
 * @date   2014/10/17
 * @brief  DTS application configuration framework
 *
 */

#include <rwdts.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_query_api.h>
#include <rwdts_appconf_api.h>


/* Transaction specific structure; kept as registration group scratch.
   The scratch in here is from the application proper. */
struct rwdts_appconf_xact_s {
  rwdts_appconf_t *ac;

  union {
    void *scratch;                /* app's scratch */
    GValue *scratch_gi;
  };

  uint32_t queries_in;
  uint32_t queries_out;

  uint32_t precommit;
  uint32_t reg_ready;
  uint32_t commit;
  uint32_t abort;

  struct {
    uint64_t corrid; //?? probably need a keyspec on each?
    rw_status_t rs;
    char *str;
  } *errs;
  uint32_t errs_ct;
};



static void *rwdts_appconf_xact_init(rwdts_group_t *grp,
                                     rwdts_xact_t *xact,
                                     void *ctx) {
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_appconf_t *ac = (rwdts_appconf_t *)ctx;
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);

  rwdts_appconf_xact_t *appx = NULL;

  appx = RW_MALLOC0_TYPE(sizeof(*appx), rwdts_appconf_xact_t);
  appx->ac = ac;
  if (ac->cb.xact_init) {
    if (ac->cb.xact_init_dtor) {
      appx->scratch_gi = ac->cb.xact_init_gi(ac, xact, ac->cb.ctx);
    } else {
      appx->scratch = ac->cb.xact_init(ac, xact, ac->cb.ctx);
    }
  }

  return appx;
}

static void rwdts_appconf_xact_deinit(rwdts_group_t *grp,
                                      rwdts_xact_t *xact,
                                      void *ctx,
                                      void *scratch) {
  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);
  rwdts_appconf_t *ac = appx->ac;
  int k;

  if (ac->cb.xact_deinit) {
    if (ac->cb.xact_deinit_dtor) {
      ac->cb.xact_deinit_gi(ac,
                            xact,
                            ac->cb.ctx,
                            appx->scratch_gi);
      if (appx->scratch_gi) {
        g_value_unset(appx->scratch_gi);
        appx->scratch_gi = NULL;
      }
    } else {
      ac->cb.xact_deinit(ac,
                         xact,
                         ac->cb.ctx,
                         appx->scratch);
      appx->scratch = NULL;
    }
  } else {
    if (ac->cb.xact_init_dtor) {
      if (appx->scratch_gi) {
        g_value_unset(appx->scratch_gi);
        appx->scratch_gi = NULL;
      }
    } else {
      RW_ASSERT(!appx->scratch);
    }
  }
  for(k=0;k<appx->errs_ct;free(appx->errs[k].str),k++);
  free(appx->errs);
  appx->errs = NULL;
  appx->errs_ct = 0;
  RW_FREE_TYPE(appx, rwdts_appconf_xact_t);
  appx = NULL;
}
static rwdts_member_rsp_code_t rwdts_appconf_xact_event(rwdts_api_t *apih,
                                                        rwdts_group_t *grp,
                                                        rwdts_xact_t *xact,
                                                        rwdts_member_event_t xact_event, /* PRECOMMIT/COMMIT/ABORT/... */
                                                        void *ctx,
                                                        void *scratch_in) {
  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)scratch_in;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);
  rwdts_appconf_t *ac = appx->ac;
  int k;

  switch (xact_event) {
    default:
      break;
    case RWDTS_MEMBER_EVENT_ABORT:
      appx->abort++;
      RW_ASSERT(appx->abort == 1);
      RW_ASSERT(appx->commit == 0);
      appx->errs_ct = 0;

      /* Apply with xact=NULL to revert*/
      if (ac->cb.xact_init_dtor) {
        ac->cb.config_apply_gi(apih,
                               ac,
                               NULL,
                               RWDTS_APPCONF_ACTION_INSTALL,
                               ac->cb.ctx,
                               NULL);
      } else {
        ac->cb.config_apply(apih,
                            ac,
                            NULL,
                            RWDTS_APPCONF_ACTION_INSTALL,
                            ac->cb.ctx,
                            NULL);
      }
      if (appx->errs_ct) {
        // Whoops, now we're doomed
        RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_AppconfInstallFailed,
                                 "Appconf INSTALL failure", appx->errs_ct, rwdts_get_app_addr_res(apih, ac->cb.config_apply), apih->client_path);
        int i;
        for (i=0; i<appx->errs_ct; i++) {
          RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_AppconfInstallErrors,
                                   i, appx->errs[i].rs, appx->errs[i].str);
        }
        RW_CRASH();
      }
      break;

    case RWDTS_MEMBER_EVENT_PRECOMMIT:
      appx->precommit++;
      RW_ASSERT(appx->precommit == 1);
      RW_ASSERT(appx->commit == 0);
      RW_ASSERT(appx->abort == 0);

      /* Validate */
      if (ac->cb.config_validate) {
        if (ac->cb.xact_init_dtor) {
          ac->cb.config_validate_gi(apih,
                                    ac,
                                    (rwdts_xact_t*)xact,
                                    ac->cb.ctx,
                                    appx->scratch);
        } else {
          ac->cb.config_validate(apih,
                                 ac,
                                 (rwdts_xact_t*)xact,
                                 ac->cb.ctx,
                                 appx->scratch);
        }
      }
      if (appx->errs_ct) {
        /* How to actually get xact->appconf->err[] table up to uagent etc? */
        return RWDTS_ACTION_NOT_OK;
      }

      /* Reconcile */
      if (ac->cb.xact_init_dtor) {
        ac->cb.config_apply_gi(apih,
                               ac,
                               (rwdts_xact_t*)xact,
                               RWDTS_APPCONF_ACTION_RECONCILE,
                               ac->cb.ctx,
                               appx->scratch);
      } else {
        ac->cb.config_apply(apih,
                            ac,
                            (rwdts_xact_t*)xact,
                            RWDTS_APPCONF_ACTION_RECONCILE,
                            ac->cb.ctx,
                            appx->scratch);
      }
      if (!appx->errs_ct) {
        return RWDTS_ACTION_OK;
      }

      /* How to actually get xact->appconf->err[] table up to uagent etc? */
      for (k = 0; k < appx->errs_ct;free(appx->errs[k].str),k++);
      free(appx->errs);
      appx->errs = NULL;
      appx->errs_ct = 0;

      return RWDTS_ACTION_NOT_OK;        /* will trigger PRECOMMIT -> ABORT */
      break;
    case RWDTS_MEMBER_EVENT_COMMIT:
      appx->commit++;
      RW_ASSERT(appx->commit == 1);
      RW_ASSERT(appx->precommit == 1);
      RW_ASSERT(appx->abort == 0);
      break;
  }

  return RWDTS_ACTION_OK;
}

static void rwdts_appconf_regn_check_timer(void *ctx)
{
  rwdts_appconf_t *ac = (rwdts_appconf_t *)ctx;
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  rwdts_api_t* apih = ac->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT_MESSAGE(ac->phase_complete, "Appconf Registration not complete on time");

  rwsched_dispatch_source_cancel(apih->tasklet,
                                 ac->regn_timer);
  rwsched_dispatch_release(apih->tasklet,
                           ac->regn_timer);
  ac->regn_timer = NULL;

  return;
}

rwdts_appconf_t *rwdts_appconf_group_create(rwdts_api_t *apih,
                                            rwdts_xact_t* xact,
                                            rwdts_appconf_cbset_t *cbset)
{
  rw_status_t rs;
  RW_ASSERT(cbset);
  RW_ASSERT(cbset->config_apply);

  if (cbset == NULL) {
    RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_AppconfError,
                             "AppconfGroup Create invalid callback param",
                              apih->client_path);
    return NULL;
  }
  if (!cbset->config_apply) {
    RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_AppconfError,
                             "AppconfGroup Create invalid config apply param",
                              apih->client_path);
    return NULL;
  }
  rwdts_appconf_t *ac = RW_MALLOC0_TYPE(sizeof(rwdts_appconf_t), rwdts_appconf_t);
  ac->apih = apih;
  memcpy(&ac->cb, cbset, sizeof(ac->cb));
  rs = rwdts_api_group_create(ac->apih, rwdts_appconf_xact_init,
                  rwdts_appconf_xact_deinit, rwdts_appconf_xact_event, 
                  ac, &ac->group);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(ac->group);

  if ((rs != RW_STATUS_SUCCESS) || !ac->group) {
    RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_AppconfError,
                             "AppconfGroup API Create failure",
                              apih->client_path);
    return NULL;
  }

  ac->phase_complete = false;
  ac->reg_pend = 0;
  ac->group->xact = xact;
  if (xact) {
    rwdts_xact_ref(xact,__PRETTY_FUNCTION__, __LINE__);
  }

  rwdts_appconf_ref(ac);

  ac->regn_timer = rwsched_dispatch_source_create(apih->tasklet,
                                                  RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                  0,
                                                  0,
                                                  apih->client.rwq);
  rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                              ac->regn_timer,
                                              rwdts_appconf_regn_check_timer);
  rwsched_dispatch_set_context(apih->tasklet,
                               ac->regn_timer,
                               ac);

  rwsched_dispatch_source_set_timer(apih->tasklet,
                                    ac->regn_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC),
                                    (60 * NSEC_PER_SEC),
                                    0);
  rwsched_dispatch_resume(apih->tasklet,
                          ac->regn_timer);


  return ac;
}

void rwdts_appconf_group_destroy(rwdts_appconf_t *ac) {
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  if (ac->cb.xact_init_dtor) {
    ac->cb.xact_init_dtor(ac->cb.ctx);
  }
  if (ac->cb.xact_deinit_dtor) {
    ac->cb.xact_deinit_dtor(NULL);
  }

  if (ac->regn_timer) {
    rwsched_dispatch_source_cancel(ac->apih->tasklet,
                                   ac->regn_timer);
    rwsched_dispatch_release(ac->apih->tasklet,
                             ac->regn_timer);
    ac->regn_timer = NULL;
  }

  memset(&ac->cb, 0, sizeof(ac->cb));
  rwdts_group_destroy(ac->group);
  RW_FREE_TYPE(ac, rwdts_appconf_t);
}

static void
rwdts_appconf_cb_reg_ready(rwdts_member_reg_handle_t  regh,
                           rw_status_t                reg_status,
                           void*                      ud)
{
  rwdts_appconf_t *ac = (rwdts_appconf_t *)ud;
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  if (!((rwdts_member_registration_t *)regh)->appconf )
  {
    //printf("No appconf\n");
    return;
  }

  if (ac->reg_pend) {
    ac->reg_pend--;
  }
  /* Apply with xact=NULL to revert*/
  if (!ac->reg_pend && ac->phase_complete) {
    ac->cb.config_apply(ac->apih,
                      ac,
                      NULL, /* no xact -> original, committed data */
                      RWDTS_APPCONF_ACTION_INSTALL,
                      ac->cb.ctx,
                      NULL); /* no scratch? */
  }
}

uint32_t
rwdts_get_stored_keys(rwdts_member_registration_t* reg,
                      rw_keyspec_path_t*** keyspecs)
{

  ProtobufCMessage* msg = NULL;
  rw_keyspec_path_t* out_ks= NULL;
  uint count = 0;
  rwdts_member_cursor_t *cursor = rwdts_member_data_get_current_cursor(NULL,
                                                    (rwdts_member_reg_handle_t)reg);

   rwdts_member_data_reset_cursor(NULL, (rwdts_member_reg_handle_t)reg);

   while((msg =
               (ProtobufCMessage*)rwdts_member_reg_handle_get_next_element(
                                                  (rwdts_member_reg_handle_t)reg,
                                                  cursor, NULL,
                                                  &out_ks)) != NULL) {
     RW_ASSERT(msg);
     if (out_ks) {
       count++;
       *keyspecs = realloc(*keyspecs, (count * sizeof(*keyspecs[0])));
       (*keyspecs)[count -1] = out_ks;
     }
   }

   return count;
}

static rwdts_member_rsp_code_t
rwdts_appconf_find_keys_call_prepare(rwdts_member_registration_t* reg,
                                     const rw_keyspec_path_t *keyspec,
                                     const rwdts_xact_info_t *xact_info)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)xact_info->scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);
  rwdts_appconf_t *ac = appx->ac;
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
 
  rw_keyspec_path_t** keyspecs = NULL;
  ProtobufCMessage *msg = NULL;
  uint32_t i;
  bool wildcard = rw_keyspec_path_has_wildcards(keyspec);

  uint32_t key_count = rwdts_get_stored_keys(reg, &keyspecs);

  if (!key_count) {
    appx->queries_in++;
    rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
    return RWDTS_ACTION_NOT_OK;
  }

  for (i=0; i < key_count; i++) {

    if (!wildcard) {
      if(!rw_keyspec_path_is_a_sub_keyspec(NULL, keyspec, keyspecs[i])) {
        continue;
      }
    }

    appx->queries_in++;
    msg = rw_keyspec_path_create_delete_delta(keyspecs[i], &xact->ksi,
                                              reg->keyspec);
    if (!msg) {
      rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
      RW_FREE(keyspecs);
      return RWDTS_ACTION_NOT_OK;
    }

    if (reg->appconf->cb.prepare_dtor) {
      /* Take a ref (make a copy, really) for Gi callers, who may ref
         to keep it, and will unref when returning from the prepare
         callback */
      rwdts_xact_info_t* copy_xact_info = rwdts_xact_info_ref((rwdts_xact_info_t*)xact_info);
      rw_keyspec_path_t *local_ks = NULL;
      rw_status_t rs = rw_keyspec_path_create_dup((rw_keyspec_path_t*)keyspecs[i], &xact_info->xact->ksi,&local_ks);
      if (rs != RW_STATUS_SUCCESS) {
        rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
        protobuf_c_message_free_unpacked(NULL, msg);
        return RWDTS_ACTION_NOT_OK;
      }
      reg->appconf->cb.prepare_gi(xact_info->apih,
                                  reg->appconf->ac,
                                  xact,
                                  copy_xact_info,
                                  local_ks,
                                  msg,
                                  ac->cb.ctx,
                                  appx->scratch,
                                  reg->appconf->cb.prepare_ud);
    } else {
      reg->appconf->cb.prepare(xact_info->apih,
                               reg->appconf->ac,
                               xact,
                               xact_info,
                               (rw_keyspec_path_t*)keyspecs[i],
                               msg,
                               ac->cb.ctx,
                               appx->scratch);
    }
   protobuf_c_message_free_unpacked(NULL, msg);
  }
  if (keyspecs) {
    RW_FREE(keyspecs);
  }
  return RWDTS_ACTION_ASYNC;
}
    
static rwdts_member_rsp_code_t
rwdts_appconf_cb_prepare(const rwdts_xact_info_t *xact_info,
                         RWDtsQueryAction action,
                         const rw_keyspec_path_t *keyspec,
                         const ProtobufCMessage *msg,
                         void* user_data)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);

  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)xact_info->scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);
  rwdts_appconf_t *ac = appx->ac;
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);

  rwdts_member_registration_t* reg = (rwdts_member_registration_t *)xact_info->regh;
  if (reg->appconf->cb.prepare) {
    /* This is a temporary code to avoid system blow-up. This code will
       be removed once pb-delta is available */
    if (action == RWDTS_QUERY_DELETE) {
      if (!(reg->flags & RWDTS_FLAG_DELTA_READY)) {
        appx->queries_in++;
        rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
        return RWDTS_ACTION_NOT_OK;
      }
      RW_ASSERT(!msg);
      msg = rw_keyspec_path_create_delete_delta(keyspec, &xact->ksi,
                                                reg->keyspec);
      if (!msg) {
        size_t depth_d = rw_keyspec_path_get_depth(keyspec);
        size_t depth_r = rw_keyspec_path_get_depth(reg->keyspec);
        if ((depth_d <= depth_r) && (reg->flags & RWDTS_FLAG_DELTA_READY)) {   
          return rwdts_appconf_find_keys_call_prepare(reg, keyspec, xact_info);
        } else {
          appx->queries_in++;
          rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
          return RWDTS_ACTION_NOT_OK;
        }
      }
      RW_ASSERT(msg);
    }
    appx->queries_in++;
    if (reg->appconf->cb.prepare_dtor) {
      /* Take a ref (make a copy, really) for Gi callers, who may ref
         to keep it, and will unref when returning from the prepare
         callback */
      xact_info = rwdts_xact_info_ref((rwdts_xact_info_t*)xact_info);
      rw_keyspec_path_t *local_ks = NULL;
      rw_status_t rs = rw_keyspec_path_create_dup((rw_keyspec_path_t*)keyspec, &xact_info->xact->ksi,&local_ks);
      if (rs != RW_STATUS_SUCCESS) {
        rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
        if (msg && (action == RWDTS_QUERY_DELETE)) {
          protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage*)msg);
          msg = NULL;
        }
        return RWDTS_ACTION_NOT_OK;
      }
      reg->appconf->cb.prepare_gi(xact_info->apih,
                                  reg->appconf->ac,
                                  xact,
                                  xact_info,
                                  local_ks,
                                  msg,
                                  ac->cb.ctx,
                                  appx->scratch,
                                  reg->appconf->cb.prepare_ud);
    } else {
      reg->appconf->cb.prepare(xact_info->apih,
                               reg->appconf->ac,
                               xact,
                               xact_info,
                               (rw_keyspec_path_t*)keyspec,
                               msg,
                               ac->cb.ctx,
                               appx->scratch);
    }
  } else {
    appx->queries_in++;
    rwdts_appconf_prepare_complete_ok(reg->appconf->ac, xact_info);
  }
  if (msg && (action == RWDTS_QUERY_DELETE)) {
     protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage *)msg);
     msg = NULL;
  }
  return RWDTS_ACTION_ASYNC;
}

void rwdts_appconf_register_deinit(rwdts_member_registration_t *reg) {
  if (reg->appconf) {
    if (reg->appconf->cb.prepare_dtor) {
      reg->appconf->cb.prepare_dtor(reg->appconf->cb.prepare_ud);
    }
    RW_ASSERT_TYPE(reg->appconf, rwdts_appconf_reg_t);
    RW_ASSERT_TYPE(reg->appconf->ac, rwdts_appconf_t);
    RW_FREE_TYPE(reg->appconf, rwdts_appconf_reg_t);
    reg->appconf = NULL;
  }
}

static rwdts_member_reg_handle_t rwdts_appconf_register_int(rwdts_appconf_t *ac,
                                                            const rw_keyspec_path_t *ks,
                                                            rwdts_shard_info_detail_t *shard,
                                                            const ProtobufCMessageDescriptor *pbdesc,
                                                            uint32_t flags,
                                                            rwdts_appconf_prepare_cb_t prepare_cb,
                                                            void *prepare_ud,
                                                            GDestroyNotify prepare_cb_dtor) {
  RW_ASSERT(ac);
  if (!ac) {
    return NULL;
  }
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);

  rwdts_member_reg_handle_t regh = NULL;

  RwSchemaCategory category = rw_keyspec_path_get_category((rw_keyspec_path_t*)ks);

  if (category != RW_SCHEMA_CATEGORY_CONFIG) {
    return NULL;
  }

  flags &= (~RWDTS_FLAG_PUBLISHER);
  flags |= RWDTS_FLAG_SUBSCRIBER;
  flags |= RWDTS_FLAG_CACHE;

  rwdts_group_register_keyspec(ac->group, ks,
        rwdts_appconf_cb_reg_ready, rwdts_appconf_cb_prepare,
        NULL, NULL, NULL, flags, ac, &regh);
  if (regh) {
    rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

    rwdts_appconf_reg_t *appr = RW_MALLOC0_TYPE(sizeof(*appr), rwdts_appconf_reg_t);
    reg->appconf = appr;
    appr->ac = ac;
    appr->cb.prepare = prepare_cb; /* may be NULL! */
    appr->cb.prepare_ud = prepare_ud;
    appr->cb.prepare_dtor = prepare_cb_dtor;
  }
  ac->reg_pend++;
  return regh;
}

rwdts_member_reg_handle_t rwdts_appconf_register(rwdts_appconf_t *ac,
                                                 const rw_keyspec_path_t *ks,
                                                 rwdts_shard_info_detail_t *shard,
                                                 const ProtobufCMessageDescriptor *pbdesc,
                                                 uint32_t flags,
                                                 rwdts_appconf_prepare_cb_t prepare_cb) {
  return rwdts_appconf_register_int(ac,ks,shard,pbdesc,flags,prepare_cb,NULL,NULL);
}
rw_status_t rwdts_appconf_register_keyspec(rwdts_appconf_t *ac,
                                           const rw_keyspec_path_t *ks,
                                           uint32_t flags,
                                           rwdts_appconf_prepare_cb_t prepare_cb,
                                           rwdts_member_reg_handle_t *handle_out) {
  return rwdts_appconf_register_keyspec_gi(ac, ks, flags, (rwdts_appconf_prepare_cb_gi)prepare_cb, NULL, NULL, handle_out);
}
rw_status_t rwdts_appconf_register_keyspec_gi(rwdts_appconf_t *ac,
                                              const rw_keyspec_path_t *ks,
                                              uint32_t flags,
                                              rwdts_appconf_prepare_cb_gi prepare_cb,
                                              void *prepare_ud,
                                              GDestroyNotify prepare_cb_dtor,
                                              rwdts_member_reg_handle_t *handle_out) {
  rw_keyspec_path_t *remainder_ks = NULL;
  const rw_yang_pb_msgdesc_t *result = NULL;
  const rw_yang_pb_schema_t* schema = NULL;

  schema = ((ProtobufCMessage*)ks)->descriptor->ypbc_mdesc->module->schema;
  if (schema == NULL) {
    schema = rwdts_api_get_ypbc_schema(ac->apih);
  }
  if (!schema) {
    RWTRACE_CRIT(ac->apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "register_xpath[%s] failed - Schema is NULL", "!!");
    return (RW_STATUS_FAILURE);
  }
  rw_status_t rs = rw_keyspec_path_find_msg_desc_schema((rw_keyspec_path_t*)ks,
                                                        NULL,
                                                        schema,
                                                        (const rw_yang_pb_msgdesc_t **)&result,
                                                        &remainder_ks);
  if (rs != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(ac->apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "rw_keyspec_path_find_msg_desc_schema failed for xapth[%s]", "!!");
    return (rs);
  }
  if (remainder_ks != NULL) {
    char *remainder_ks_str;
    rw_keyspec_path_get_new_print_buffer(remainder_ks,
                                NULL,
                                schema,
                                &remainder_ks_str);
    RWTRACE_CRIT(ac->apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "Unknown keyspec in xapth[%s] - remainder_keyspec = [%s]", "!!",
                 remainder_ks_str ? remainder_ks_str : "");
    rw_keyspec_path_free(remainder_ks, NULL );
    free(remainder_ks_str);
    remainder_ks = NULL;
  }

  rwdts_member_reg_handle_t han = rwdts_appconf_register_int(ac,ks,NULL,result->u->msg_msgdesc.pbc_mdesc,flags,(rwdts_appconf_prepare_cb_t)prepare_cb,prepare_ud,prepare_cb_dtor);
  *handle_out = han;
  if (han == NULL) {
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


void rwdts_appconf_phase_complete(rwdts_appconf_t *ac, enum rwdts_appconf_phase_e phase) {
  switch (phase) {
    case RWDTS_APPCONF_PHASE_REGISTER:
      ac->phase_complete = TRUE;
      if (ac->regn_timer) {
        rwsched_dispatch_source_cancel(ac->apih->tasklet,
                                       ac->regn_timer);
        rwsched_dispatch_release(ac->apih->tasklet,
                                 ac->regn_timer);
        ac->regn_timer = NULL;
      }

      if (ac->group && ac->group->xact) {
        rwdts_xact_unref(ac->group->xact,__PRETTY_FUNCTION__, __LINE__);
        ac->group->xact = NULL;
      }

      if (ac->phase_complete && !ac->reg_pend) {
        ac->cb.config_apply(ac->apih,
                            ac,
                            NULL, /* no xact -> original, committed data */
                            RWDTS_APPCONF_ACTION_INSTALL,
                            ac->cb.ctx,
                            NULL);
      }
      break;
    default:
      RW_ASSERT_MESSAGE(0, "Unknown appconf phase %d !?\n", (int)phase);
      break;
  }
}

void rwdts_appconf_prepare_complete_fail(rwdts_appconf_t *ac,
                                         const rwdts_xact_info_t *xact_info,
                                         rw_status_t rs,
                                         const char *errstr) {
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  rwdts_appconf_xact_t *appx =
      (rwdts_appconf_xact_t *)xact_info->xact->group[ac->group->id]->scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);

  RW_ASSERT(appx->queries_in > 0);
  appx->queries_out++;
  RW_ASSERT(appx->queries_out <= appx->queries_in);

  int idx = appx->errs_ct++;
  appx->errs = realloc(appx->errs, sizeof(appx->errs[0]) * appx->errs_ct);
  appx->errs[idx].str = strdup(errstr);
  appx->errs[idx].rs = rs;
  appx->errs[idx].corrid = (xact_info->queryh &&
                            ((RWDtsQuery*)xact_info->queryh)->has_corrid ?
                            ((RWDtsQuery*)xact_info->queryh)->corrid : 0);

  rwdts_member_send_error(xact_info->xact, NULL,
                          (RWDtsQuery*)xact_info->queryh,
                          NULL, NULL, rs, errstr);
  RWTRACE_CRIT(xact_info->apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "APPCONF prepare_complete_fail code %d str '%s'\n",
               rs,
               errstr);

  rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_NACK,
                                  NULL, NULL);
}

void rwdts_appconf_prepare_complete_ok(rwdts_appconf_t *ac,
                                       const rwdts_xact_info_t *xact_info) {
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)xact_info->xact->group[ac->group->id]->scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);

  RW_ASSERT(appx->queries_in > 0);
  appx->queries_out++;
  RW_ASSERT(appx->queries_out <= appx->queries_in);

  rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_ACK, NULL, NULL);
}

void rwdts_appconf_prepare_complete_na(rwdts_appconf_t *ac,
                                       const rwdts_xact_info_t *xact_info) {
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  rwdts_appconf_xact_t *appx = (rwdts_appconf_xact_t *)xact_info->xact->group[ac->group->id]->scratch;
  RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);

  RW_ASSERT(appx->queries_in > 0);
  appx->queries_out++;
  RW_ASSERT(appx->queries_out <= appx->queries_in);

  rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_NA, NULL, NULL);
}

void rwdts_appconf_xact_add_issue(rwdts_appconf_t *ac,
                                  rwdts_xact_t *xact,
                                  rw_status_t rs,
                                  const char *errstr) {
  RW_ASSERT_TYPE(ac, rwdts_appconf_t);

  if (xact) {
    RW_ASSERT_TYPE(xact, rwdts_xact_t);

    rwdts_appconf_xact_t *appx =
        (rwdts_appconf_xact_t *)xact->group[ac->group->id]->scratch;
    RW_ASSERT_TYPE(appx, rwdts_appconf_xact_t);

    int idx = appx->errs_ct++;
    appx->errs = realloc(appx->errs, sizeof(appx->errs[0]) * appx->errs_ct);
    appx->errs[idx].str = strdup(errstr);
    appx->errs[idx].rs = rs;
    appx->errs[idx].corrid = 0;  //??

    // Send to error_report also
    rwdts_member_send_error(xact, NULL, NULL, ac->apih, NULL, rs, errstr);

  }
  else if (ac->group && ac->group->xact) {
    RW_ASSERT_TYPE(ac->group->xact, rwdts_xact_t);
    rwdts_member_send_error(ac->group->xact, NULL, NULL, ac->apih,
                            NULL, rs, errstr);
  }
  else {
    if (ac->apih) {
      RWTRACE_CRIT(ac->apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: No xact, rs=%d, errstr=%s\n",
                   __FUNCTION__, rs, errstr);
    }
    else {
      DTS_PRINT("%s: No xact, rs=%d, errstr=%s\n",
                __FUNCTION__, rs, errstr);
    }
  }

  if (ac->apih) {
    RWTRACE_ERROR(ac->apih->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET,
                  "APPCONF xact issue for group %d, rs %d, errstr '%s'\n",
                  ac->group->id,
                  rs,
                  errstr);
  }
  else {
    DTS_PRINT("APPCONF xact issue for group %d, rs %d, errstr '%s'\n",
              ac->group->id,
              rs,
              errstr);
  }
}





rw_status_t
rwdts_api_appconf_group_create_gi(rwdts_api_t*            apih,
                                  rwdts_xact_t*           xact,
                                  appconf_xact_init_gi    init,
                                  appconf_xact_deinit_gi  deinit,
                                  appconf_config_validate_gi validate,
                                  appconf_config_apply_gi    apply,
                                  void*                 user_data,
                                  rwdts_appconf_t**       appconf,
                                  GDestroyNotify xact_init_dtor,
                                  GDestroyNotify xact_deinit_dtor,
                                  GDestroyNotify config_validate_dtor,
                                  GDestroyNotify config_apply_dtor) {
  RW_ASSERT(appconf);
  if (!appconf) {
    return RW_STATUS_FAILURE;
  }
  rwdts_appconf_cbset_t cb = {
    .xact_init_gi    = init,
    .xact_deinit_gi  = deinit,
    .config_validate_gi = validate,
    .config_apply_gi    = apply,
    .ctx             = user_data,
    .xact_init_dtor =  xact_init_dtor,
    .xact_deinit_dtor =  xact_deinit_dtor,
    .config_validate_dtor = config_validate_dtor,
    .config_apply_dtor = config_apply_dtor
  };

  if (init) {
    if (!xact_init_dtor && !deinit) {
      RWDTS_API_LOG_EVENT(apih, AppconfDeinitMissing, "rwdts_api_appconf_group_create_gi: Use a managed langauge or provide a xact_deinit callback");
      RW_ASSERT(deinit);
    }
  }

  *appconf = rwdts_appconf_group_create(apih, xact, &cb);

  if (*appconf == NULL) {
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Invoking state change callback", apih->client_path);
    return (RW_STATUS_FAILURE);
  }

  return (RW_STATUS_SUCCESS);
}

rw_status_t
rwdts_api_appconf_group_create(rwdts_api_t*            apih,
                               rwdts_xact_t*           xact,
                               appconf_xact_init       init,
                               appconf_xact_deinit     deinit,
                               appconf_config_validate validate,
                               appconf_config_apply    apply,
                               void*                   user_data,
                               rwdts_appconf_t**       appconf) {
  RW_ASSERT(appconf);
  if (!appconf) {
    return RW_STATUS_FAILURE;
  }
  rwdts_appconf_cbset_t cb = {
    .xact_init       = init,
    .xact_deinit     = deinit,
    .config_validate = validate,
    .config_apply    = apply,
    .ctx             = user_data,
  };

  *appconf = rwdts_appconf_group_create(apih, xact, &cb);

  if (*appconf == NULL) {
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Invoking state change callback", apih->client_path);
    return (RW_STATUS_FAILURE);
  }

  return (RW_STATUS_SUCCESS);
}

/*
 * appconf register xpath
 */
rw_status_t
rwdts_appconf_register_xpath_gi(rwdts_appconf_t*                  ac,
                                const char*                       xpath,
                                uint32_t                          flags,
                                rwdts_appconf_prepare_cb_gi        prepare,
                                void *prepare_ud,
                                GDestroyNotify prepare_dtor,
                                rwdts_member_reg_handle_t*        handle)
{
  rw_status_t rs;
  rw_keyspec_path_t *keyspec = NULL;
  const rw_yang_pb_schema_t* schema = NULL;
  rwdts_api_t *apih;

  RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  apih = ac->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

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

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rs =  rwdts_appconf_register_keyspec_gi(ac,
                                          keyspec,
                                          flags,
                                          prepare,
                                          prepare_ud,
                                          prepare_dtor,
                                          handle);

  rw_keyspec_path_free(keyspec, NULL);

  return(rs);
}

