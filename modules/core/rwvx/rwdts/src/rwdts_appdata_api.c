
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file   rwdts_appdata_api.c
 * @author Prashanth Bhaskar <prashanth.bhaskar@riftio.com>
 * @date   2015/08/18
 * @brief  DTS application data framework 
 *
 */

#include <rwdts_int.h>

#define RWDTS_UPDATE_SAFE_CB(x) \
    RW_SKLIST_PARAMS_DECL(safe_data_list_,rwdts_safe_put_data_t, safe_id, \
                          rw_sklist_comp_uint32_t, element); \
    RW_SKLIST_INIT(&(shard->appdata->safe_data_list),&safe_data_list_); \
    shard->appdata->s##x->getnext = getnext; \
    shard->appdata->s##x->take = take; \
    shard->appdata->s##x->put = put; \
    shard->appdata->s##x->create= create; \
    shard->appdata->s##x->delete_fn = delete_fn; \
    shard->appdata->s##x->ctx = ud; \
    shard->appdata->s##x->dtor = dtor;

#define RWDTS_UPDATE_UNSAFE_CB(x) \
    shard->appdata->u##x->getnext = getnext; \
    shard->appdata->u##x->get = get; \
    shard->appdata->u##x->create= create; \
    shard->appdata->u##x->delete_fn = delete_fn; \
    shard->appdata->u##x->ctx = ud; \
    shard->appdata->u##x->dtor = dtor;

#define RWDTS_UPDATE_QUEUE_CB(x) \
    shard->appdata->q##x->getnext = getnext; \
    shard->appdata->q##x->copy = copy; \
    shard->appdata->q##x->pbdelta= pbdelta; \
    shard->appdata->q##x->ctx = ud; \
    shard->appdata->q##x->dtor = dtor;

#define RWDTS_APPDATA_SAFE_TIMEOUT (5 * NSEC_PER_SEC)

typedef enum {
  RWDTS_APPDATA_KEYSPEC = 1,
  RWDTS_APPDATA_PATHENTRY = 2,
  RWDTS_APPDATA_MINIKEY = 3
} rwdts_appdata_keytype_t;

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_keyspec_installed(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_path_t* keyspec);

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_pathentry_installed(rwdts_shard_handle_t* shard,
                                                   rw_keyspec_entry_t* pathentry);

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_minikey_installed(rwdts_shard_handle_t* shard,
                                                 rw_schema_minikey_t* minikey);

static rw_status_t
rwdts_shard_handle_appdata_get_next_keyspec_installed(rwdts_shard_handle_t* shard,
                                                      rw_keyspec_path_t* keyspec,
                                                      rw_keyspec_path_t** out_keyspec);

static void 
rwdts_appdata_stop_put_tmr(rwdts_api_t* apih,
                           rwdts_appdata_t* appdata,
                           rwdts_safe_put_data_t* safe_data)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(appdata);
  RW_ASSERT(safe_data);

  rwsched_dispatch_source_cancel(apih->tasklet, safe_data->safe_put_timer);
  rwsched_dispatch_release(apih->tasklet, safe_data->safe_put_timer);
  safe_data->safe_put_timer = NULL;
  rwdts_safe_put_data_t* safe_info;
  RW_SKLIST_REMOVE_BY_KEY(&appdata->safe_data_list,
                          &safe_data->safe_id,
                          &safe_info);
  if (safe_info) {
    RW_FREE(safe_info);
  }
  return;
}

static void
rwdts_appdata_safe_put(rwdts_shard_handle_t* shard_tab,
                       rwdts_appdata_t* appdata,
                       rwdts_safe_put_data_t* safe_data)
{
  rwdts_shard_t* shard = shard_tab;

  switch (appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
      {
        if (appdata->sk_cb && appdata->sk_cb->put) {
          appdata->sk_cb->put(shard, safe_data->keyspec, safe_data->out_msg, appdata->sk_cb->ctx);
        }
        break;
      }
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
      {
        if (appdata->sp_cb && appdata->sp_cb->put) {
          appdata->sp_cb->put(shard, safe_data->pe, safe_data->out_msg, appdata->sp_cb->ctx);
        }
        break;
      }
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
      {
        if (appdata->sm_cb && appdata->sm_cb->put) {
          appdata->sm_cb->put(shard, safe_data->mk, safe_data->out_msg, appdata->sm_cb->ctx);
        }
        break;
      }
    default:
      RW_ASSERT_MESSAGE(0, "Should not be here");
  }

  return;
}

static void 
rwdts_appdata_safe_put_timer(void* ud)
{
  rwdts_safe_put_data_t *safe_data = (rwdts_safe_put_data_t*) ud;
  RW_ASSERT(safe_data);
  rwdts_shard_t* shard = safe_data->shard;
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = safe_data->appdata;
  RW_ASSERT(appdata);
  rwdts_api_t* apih = shard->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_appdata_safe_put(shard, appdata, safe_data);
 
  rwdts_appdata_stop_put_tmr(apih, appdata, safe_data);
  return;
}

static void
rwdts_appdata_start_put_tmr(rwdts_shard_handle_t* shard_tab,
                            rwdts_appdata_t* appdata,
                            ProtobufCMessage* out_msg,
                            void* key, rwdts_appdata_keytype_t key_type)
{
  RW_ASSERT(shard_tab);
  RW_ASSERT(appdata);
  rwdts_shard_t* shard = shard_tab;

  rwdts_safe_put_data_t* safe_put_data = RW_MALLOC0(sizeof(rwdts_safe_put_data_t));
  safe_put_data->appdata = appdata;
  safe_put_data->shard = shard;
  safe_put_data->out_msg = out_msg;
  safe_put_data->safe_id = ++appdata->safe_data_id;
  rw_status_t rs = RW_SKLIST_INSERT(&(appdata->safe_data_list), safe_put_data);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  shard->appdata->scratch_safe_id = safe_put_data->safe_id;

  switch(key_type) {
    case (RWDTS_APPDATA_KEYSPEC):
      {
        safe_put_data->keyspec = (rw_keyspec_path_t*)key; // Will keyspec survive till the timer expiry?
        break;
      }
    case (RWDTS_APPDATA_PATHENTRY):
      {
        safe_put_data->pe = (rw_keyspec_entry_t*)key; // Will pe survive till the timer expiry?
        break;
      }
    case (RWDTS_APPDATA_MINIKEY):
      {
        safe_put_data->mk = (rw_schema_minikey_t *)key; // Will mk survive till the timer expiry?
        break;
      }
    default:
      RW_ASSERT_MESSAGE(0, "Should not be here");
  }

  safe_put_data->safe_put_timer = rwsched_dispatch_source_create(shard->apih->tasklet,
                      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER, 0, 0, shard->apih->client.rwq);
  rwsched_dispatch_source_set_event_handler_f(shard->apih->tasklet,
                      safe_put_data->safe_put_timer, rwdts_appdata_safe_put_timer);
  rwsched_dispatch_set_context(shard->apih->tasklet, safe_put_data->safe_put_timer, safe_put_data);
  rwsched_dispatch_source_set_timer(shard->apih->tasklet, safe_put_data->safe_put_timer,
                      dispatch_time(DISPATCH_TIME_NOW, RWDTS_APPDATA_SAFE_TIMEOUT),
                      (RWDTS_APPDATA_SAFE_TIMEOUT), 0);
  rwsched_dispatch_resume(shard->apih->tasklet, safe_put_data->safe_put_timer);

  return;
}

void
rwdts_shard_handle_appdata_safe_put_exec(rwdts_shard_handle_t* shard_tab, 
                                         uint32_t safe_id)
{
  rwdts_shard_t* shard = (rwdts_shard_t*)shard_tab;
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata); 

  if ((appdata->appdata_type != RWDTS_APPDATA_TYPE_SAFE_KEYSPEC) &&
      (appdata->appdata_type != RWDTS_APPDATA_TYPE_SAFE_PE) &&
      (appdata->appdata_type != RWDTS_APPDATA_TYPE_SAFE_MINIKEY)) {
    return;
  }
  rwdts_safe_put_data_t *safe_data;
  rw_status_t rs = RW_SKLIST_LOOKUP_BY_KEY(&(appdata->safe_data_list), &safe_id,
                                           &safe_data);   
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(safe_data);

  rwdts_api_t* apih = shard->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_appdata_safe_put(shard, appdata, safe_data);

  rwdts_appdata_stop_put_tmr(apih, appdata, safe_data);
  return;
}

rw_status_t
rwdts_shard_handle_appdata_get_element(rwdts_shard_handle_t* shard_tab,
                                       rw_keyspec_path_t* keyspec,
                                       ProtobufCMessage** out_msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;

  *out_msg = NULL;

  switch(appdata->appdata_type) {
    case RWDTS_APPDATA_TYPE_SAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC:
    {
      rs = rwdts_shard_handle_appdata_get_element_keyspec(shard_tab, keyspec, out_msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_PE:
    case RWDTS_APPDATA_TYPE_UNSAFE_PE:
    case RWDTS_APPDATA_TYPE_QUEUE_PE:
    {
      /* Get the last PE of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rs = rwdts_shard_handle_appdata_get_element_pathentry(shard_tab, pe, out_msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_QUEUE_MINIKEY:
    {
      /* Get the last minikey of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rw_schema_minikey_t *minikey;
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rs = rwdts_shard_handle_appdata_get_element_minikey(shard_tab, minikey, out_msg);
      RW_FREE(minikey);
      minikey = NULL;
      break;
    }

    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_create_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;

  switch(appdata->appdata_type) {
    case RWDTS_APPDATA_TYPE_SAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC:
    {
      rs = rwdts_shard_handle_appdata_create_element_keyspec(shard_tab, keyspec, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_PE:
    case RWDTS_APPDATA_TYPE_UNSAFE_PE:
    case RWDTS_APPDATA_TYPE_QUEUE_PE:
    {
      /* Get the last PE of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rs = rwdts_shard_handle_appdata_create_element_pathentry(shard_tab, pe, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_QUEUE_MINIKEY:
    {
      /* Get the last minikey of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rw_schema_minikey_t *minikey;
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rs = rwdts_shard_handle_appdata_create_element_minikey(shard_tab, minikey, msg);
      RW_FREE(minikey);
      minikey = NULL;
      break;
    }
    
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_delete_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;

  switch(appdata->appdata_type) {
    case RWDTS_APPDATA_TYPE_SAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC:
    {
      rs = rwdts_shard_handle_appdata_delete_element_keyspec(shard_tab, keyspec, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_PE:
    case RWDTS_APPDATA_TYPE_UNSAFE_PE:
    case RWDTS_APPDATA_TYPE_QUEUE_PE:
    {
      /* Get the last PE of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rs = rwdts_shard_handle_appdata_delete_element_pathentry(shard_tab, pe, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_QUEUE_MINIKEY:
    {
      /* Get the last minikey of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rw_schema_minikey_t* minikey;
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rs = rwdts_shard_handle_appdata_delete_element_minikey(shard_tab, minikey, msg);
      RW_FREE(minikey);
      minikey = NULL;
      break;
    }

    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_update_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          const ProtobufCMessage* msg)
{                            
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;

  switch(appdata->appdata_type) {
    case RWDTS_APPDATA_TYPE_SAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC:
    case RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC:
    {
      rs = rwdts_shard_handle_appdata_update_element_keyspec(shard_tab, keyspec, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_PE:
    case RWDTS_APPDATA_TYPE_UNSAFE_PE:
    case RWDTS_APPDATA_TYPE_QUEUE_PE:
    {
      /* Get the last PE of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rs = rwdts_shard_handle_appdata_update_element_pathentry(shard_tab, pe, msg);
      break;
    }

    case RWDTS_APPDATA_TYPE_SAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY:
    case RWDTS_APPDATA_TYPE_QUEUE_MINIKEY:
    {
      /* Get the last minikey of the keyspec */
      size_t depth = rw_keyspec_path_get_depth(keyspec);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(keyspec, depth-1);
      rw_schema_minikey_t* minikey;
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rs = rwdts_shard_handle_appdata_update_element_minikey(shard_tab, minikey, msg);
      RW_FREE(minikey);
      minikey = NULL;
      break;
    }

    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_get_element_keyspec(rwdts_shard_handle_t* shard_tab,
                                               rw_keyspec_path_t* keyspec,
                                               ProtobufCMessage** out_msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_FAILURE;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb && appdata->sk_cb->take) {
        rs = appdata->sk_cb->take(shard_tab, keyspec, out_msg, appdata->sk_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        rwdts_appdata_start_put_tmr(shard_tab, appdata, *out_msg, (void *)keyspec, RWDTS_APPDATA_KEYSPEC);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      *out_msg = rwdts_shard_handle_appdata_get_keyspec_installed(shard_tab, keyspec);

      if (*out_msg) {
        return RW_STATUS_SUCCESS;
      }
      if (appdata->uk_cb && appdata->uk_cb->get) {
        rs = appdata->uk_cb->get(shard_tab, keyspec, out_msg, appdata->uk_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb && appdata->qk_cb->copy) {
        rs = appdata->qk_cb->copy(shard_tab, keyspec, out_msg, appdata->qk_cb->ctx);
      }
      break;
    }
    default:
      break;
  }
  return rs;
}


rw_status_t
rwdts_shard_handle_appdata_get_element_pathentry(rwdts_shard_handle_t* shard_tab,
                                                 rw_keyspec_entry_t* pe,
                                                 ProtobufCMessage** out_msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_FAILURE;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb && appdata->sp_cb->take) {
        rs = appdata->sp_cb->take(shard_tab, pe, out_msg, appdata->sp_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        rwdts_appdata_start_put_tmr(shard_tab, appdata, *out_msg, (void *)pe, RWDTS_APPDATA_PATHENTRY);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      *out_msg = rwdts_shard_handle_appdata_get_pathentry_installed(shard_tab, pe);

      if (*out_msg) {
        return RW_STATUS_SUCCESS;
      }

      if (appdata->up_cb && appdata->up_cb->get) {
        rs = appdata->up_cb->get(shard_tab, pe, out_msg, appdata->up_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb && appdata->qp_cb->copy) {
        rs = appdata->qp_cb->copy(shard_tab, pe, out_msg, appdata->qp_cb->ctx);
      }
      break;
    }
    default:
      break;
  } 
  return rs;
}


rw_status_t
rwdts_shard_handle_appdata_get_element_minikey(rwdts_shard_handle_t* shard_tab,
                                               rw_schema_minikey_t* minikey,
                                               ProtobufCMessage** out_msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_FAILURE;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb && appdata->sm_cb->take) {
        rs = appdata->sm_cb->take(shard_tab, minikey, out_msg, appdata->sm_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        rwdts_appdata_start_put_tmr(shard_tab, appdata, *out_msg, (void *)minikey, RWDTS_APPDATA_MINIKEY);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      *out_msg = rwdts_shard_handle_appdata_get_minikey_installed(shard_tab, minikey);

      if (*out_msg) {
        return RW_STATUS_SUCCESS;
      }

      if (appdata->um_cb && appdata->um_cb->get) {
        rs = appdata->um_cb->get(shard_tab, minikey, out_msg, appdata->um_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb && appdata->qm_cb->copy) {
        rs = appdata->qm_cb->copy(shard_tab, minikey, out_msg, appdata->qm_cb->ctx);
      }
      break;
    }
    default:
      break;
  } 
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_create_element_keyspec(rwdts_shard_handle_t* shard_tab,
                                                  rw_keyspec_path_t* keyspec,
                                                  const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb && appdata->sk_cb->create) {
        rs = appdata->sk_cb->create(shard_tab, keyspec, (ProtobufCMessage *)msg, appdata->sk_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      if (appdata->uk_cb && appdata->uk_cb->create) {
        rs = appdata->uk_cb->create(shard_tab, keyspec, (ProtobufCMessage *)msg, appdata->uk_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb && appdata->qk_cb->pbdelta) {
        rs = appdata->qk_cb->pbdelta(shard_tab, keyspec, (ProtobufCMessage *)msg, appdata->qk_cb->ctx);
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_create_element_pathentry(rwdts_shard_handle_t* shard_tab,
                                                    rw_keyspec_entry_t* pe,
                                                    const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb && appdata->sp_cb->create) {
        rs = appdata->sp_cb->create(shard_tab, pe, (ProtobufCMessage *)msg, appdata->sp_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      if (appdata->up_cb && appdata->up_cb->create) {
        rs = appdata->up_cb->create(shard_tab, pe, (ProtobufCMessage *)msg, appdata->up_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb && appdata->qp_cb->pbdelta) {
        rs = appdata->qp_cb->pbdelta(shard_tab, pe, (ProtobufCMessage *)msg, appdata->qp_cb->ctx);
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_create_element_minikey(rwdts_shard_handle_t* shard_tab,
                                                  rw_schema_minikey_t* minikey,
                                                  const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb && appdata->sm_cb->create) {
        rs = appdata->sm_cb->create(shard_tab, minikey, (ProtobufCMessage *)msg, appdata->sm_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      if (appdata->um_cb && appdata->um_cb->create) {
        rs = appdata->um_cb->create(shard_tab, minikey, (ProtobufCMessage *)msg, appdata->um_cb->ctx);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb && appdata->qm_cb->pbdelta) {
        rs = appdata->qm_cb->pbdelta(shard_tab, minikey, (ProtobufCMessage *)msg, appdata->qm_cb->ctx);
      }
      break;
    }
    default:
      break;
  }
  return rs;
}


rw_status_t
rwdts_shard_handle_appdata_delete_element_keyspec(rwdts_shard_handle_t* shard_tab,
                                                  rw_keyspec_path_t* keyspec,
                                                  ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb && appdata->sk_cb->delete_fn) {
        rs = appdata->sk_cb->delete_fn(shard_tab, keyspec, appdata->sk_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      if (appdata->uk_cb && appdata->uk_cb->delete_fn) {
        rs = appdata->uk_cb->delete_fn(shard_tab, keyspec, appdata->uk_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb && appdata->qk_cb->pbdelta) {
        rs = appdata->qk_cb->pbdelta(shard_tab, keyspec, msg, appdata->qk_cb->ctx); /* msg may be needed */
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_delete_element_pathentry(rwdts_shard_handle_t* shard_tab,
                                                    rw_keyspec_entry_t* pe,
                                                    ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb && appdata->sp_cb->delete_fn) {
        rs = appdata->sp_cb->delete_fn(shard_tab, pe, appdata->sp_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      if (appdata->up_cb && appdata->up_cb->delete_fn) {
        rs = appdata->up_cb->delete_fn(shard_tab, pe, appdata->up_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb && appdata->qp_cb->pbdelta) {
        rs = appdata->qp_cb->pbdelta(shard_tab, pe, msg, appdata->qp_cb->ctx); /* msg may be needed */
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_delete_element_minikey(rwdts_shard_handle_t* shard_tab,
                                                  rw_schema_minikey_t* minikey,
                                                  ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb && appdata->sm_cb->delete_fn) {
        rs = appdata->sm_cb->delete_fn(shard_tab, minikey, appdata->sm_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      if (appdata->um_cb && appdata->um_cb->delete_fn) {
        rs = appdata->um_cb->delete_fn(shard_tab, minikey, appdata->um_cb->ctx); /* msg may be needed */
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb && appdata->qm_cb->pbdelta) {
        rs = appdata->qm_cb->pbdelta(shard_tab, minikey, msg, appdata->qm_cb->ctx); /* msg may be needed */
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_update_element_keyspec(rwdts_shard_handle_t* shard_tab,
                                                  rw_keyspec_path_t* keyspec,
                                                  const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb && appdata->sk_cb->take) {
        rs = appdata->sk_cb->take(shard_tab, keyspec, &out_msg, appdata->sk_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE);        
        if (appdata->sk_cb && appdata->sk_cb->put) {
          rs = appdata->sk_cb->put(shard_tab, keyspec, out_msg, appdata->sk_cb->ctx);
        }
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      out_msg = rwdts_shard_handle_appdata_get_keyspec_installed(shard, keyspec);
      if (!out_msg) {
        if (appdata->uk_cb && appdata->uk_cb->get) {
          rs = appdata->uk_cb->get(shard_tab, keyspec, &out_msg, appdata->uk_cb->ctx);
        }
      }

      if (out_msg && (rs == RW_STATUS_SUCCESS)) {
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE); 
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb && appdata->qk_cb->copy) {
        rs = appdata->qk_cb->copy(shard_tab, keyspec, &out_msg, appdata->qk_cb->ctx);

        if (rs == RW_STATUS_SUCCESS) {
          /* Update the out_msg with new message (msg) with new info */
          protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
          RW_ASSERT(ret == TRUE);  
        }
      } 
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_update_element_pathentry(rwdts_shard_handle_t* shard_tab,
                                                    rw_keyspec_entry_t* pe,
                                                    const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb && appdata->sp_cb->take) {
        rs = appdata->sp_cb->take(shard_tab, pe, &out_msg, appdata->sp_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE);
        if (appdata->sp_cb && appdata->sp_cb->put) {
          rs = appdata->sp_cb->put(shard_tab, pe, out_msg, appdata->sp_cb->ctx);
        }
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      out_msg = rwdts_shard_handle_appdata_get_pathentry_installed(shard, pe);
      if (!out_msg) {
        if (appdata->up_cb && appdata->up_cb->get) {
          rs = appdata->up_cb->get(shard_tab, pe, &out_msg, appdata->up_cb->ctx);
        }
      }

      if (out_msg && (rs == RW_STATUS_SUCCESS)) {
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE);  
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb && appdata->qp_cb->copy) {
        rs = appdata->qp_cb->copy(shard_tab, pe, &out_msg, appdata->qp_cb->ctx);

        if (rs == RW_STATUS_SUCCESS) {
          /* Update the out_msg with new message (msg) with new info */
          protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
          RW_ASSERT(ret == TRUE);
        }
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

rw_status_t
rwdts_shard_handle_appdata_update_element_minikey(rwdts_shard_handle_t* shard_tab,
                                                  rw_schema_minikey_t* minikey,
                                                  const ProtobufCMessage* msg)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;
  RW_ASSERT(shard->appdata);
  rwdts_appdata_t *appdata = shard->appdata;
  rw_status_t rs = RW_STATUS_SUCCESS;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb && appdata->sm_cb->take) {
        rs = appdata->sm_cb->take(shard_tab, minikey, &out_msg, appdata->sm_cb->ctx);
        if (rs != RW_STATUS_SUCCESS) {
          return RW_STATUS_FAILURE;
        }
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE);

        if (appdata->sm_cb && appdata->sm_cb->put) {
          rs = appdata->sm_cb->put(shard_tab, minikey, out_msg, appdata->sm_cb->ctx);
        }
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      out_msg = rwdts_shard_handle_appdata_get_minikey_installed(shard, minikey);
      if (!out_msg) {
        if (appdata->um_cb && appdata->um_cb->get) {
          rs = appdata->um_cb->get(shard_tab, minikey, &out_msg, appdata->um_cb->ctx);
        }
      }

      if (out_msg && (rs == RW_STATUS_SUCCESS)) {
        /* Update the out_msg with new message (msg) with new info */
        protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
        RW_ASSERT(ret == TRUE);
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb && appdata->qm_cb->copy) {
        rs = appdata->qm_cb->copy(shard_tab, minikey, &out_msg, appdata->qm_cb->ctx);

        if (rs == RW_STATUS_SUCCESS) {
          /* Update the out_msg with new message (msg) with new info */
          protobuf_c_boolean ret = protobuf_c_message_merge(NULL, out_msg, (ProtobufCMessage *)msg);
          RW_ASSERT(ret == TRUE);
        }
      }
      break;
    }
    default:
      break;
  }
  return rs;
}

const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_keyspec(rwdts_shard_handle_t*    shard_tab,
                                                        rwdts_appdata_cursor_t*    cursorh,
                                                        rw_keyspec_path_t**       keyspec)
{
  rwdts_appdata_cursor_impl_t*  cursor   = (rwdts_appdata_cursor_impl_t*) cursorh;
  rw_status_t rs;

  RW_ASSERT(shard_tab);
  RW_ASSERT_TYPE(cursor, rwdts_appdata_cursor_impl_t);

  rwdts_appdata_t* appdata = shard_tab->appdata;

  if (!appdata) {
    return NULL;
  }

  rwdts_api_t *apih = shard_tab->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rw_keyspec_path_t* out_keyspec = NULL;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb && appdata->sk_cb->getnext) {
        rs = appdata->sk_cb->getnext(cursor->keyspec, &out_keyspec, appdata->sk_cb->ctx);

        if ((rs != RW_STATUS_SUCCESS) || (!out_keyspec)) {
          return NULL;
        }

        if (appdata->sk_cb->take) {
          rs = appdata->sk_cb->take(shard_tab, out_keyspec, &out_msg, appdata->sk_cb->ctx);

          if (rs != RW_STATUS_SUCCESS) {
            return NULL;
          }

          cursor->keyspec = out_keyspec;

          if (keyspec)
          {
            rw_keyspec_path_t *merged_keyspec = NULL;
            rw_status_t rw_status = RW_KEYSPEC_PATH_CREATE_DUP(out_keyspec,
                                                               NULL,
                                                               &merged_keyspec,
                                                               NULL);
            if (rw_status != RW_STATUS_SUCCESS) {
              return NULL;
            }
            *keyspec = merged_keyspec;
          }
          rwdts_appdata_start_put_tmr(shard_tab, appdata, out_msg, (void *)out_keyspec, RWDTS_APPDATA_KEYSPEC);
        }
      }
      break;
    }

    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      if (appdata->installed) {
        rs = rwdts_shard_handle_appdata_get_next_keyspec_installed(shard_tab, cursor->keyspec, &out_keyspec);

        if ((rs != RW_STATUS_SUCCESS) || (!out_keyspec)) {
          return NULL;
        }

        out_msg = rwdts_shard_handle_appdata_get_keyspec_installed(shard_tab, out_keyspec);

        if (!out_msg) {
          return NULL;
        }

        cursor->keyspec = out_keyspec;

          if (keyspec)
          {
            rw_keyspec_path_t *merged_keyspec = NULL;
            rw_status_t rw_status = RW_KEYSPEC_PATH_CREATE_DUP(out_keyspec,
                                                               NULL,
                                                               &merged_keyspec,
                                                               NULL);
            if (rw_status != RW_STATUS_SUCCESS) {
              return NULL;
            }
            *keyspec = merged_keyspec;
          }
      } else {

        if (appdata->uk_cb && appdata->uk_cb->getnext) {
          rs = appdata->uk_cb->getnext(cursor->keyspec, &out_keyspec, appdata->uk_cb->ctx);

          if ((rs != RW_STATUS_SUCCESS) || (!out_keyspec)) {
            return NULL;
          }

          if (appdata->uk_cb && appdata->uk_cb->get) {
            rs = appdata->uk_cb->get(shard_tab, out_keyspec, &out_msg, appdata->uk_cb->ctx);

            if (rs != RW_STATUS_SUCCESS) {
              return NULL;
            }

            cursor->keyspec = out_keyspec;

            if (keyspec)
            {
              rw_keyspec_path_t *merged_keyspec = NULL;
              rw_status_t rw_status = RW_KEYSPEC_PATH_CREATE_DUP(out_keyspec,
                                                                 NULL,
                                                                 &merged_keyspec,
                                                                 NULL);
              if (rw_status != RW_STATUS_SUCCESS) {
                return NULL;
              }
              *keyspec = merged_keyspec;
            }
          }
        }
      }
      break;
    }

    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb && appdata->qk_cb->getnext) {
        rs = appdata->qk_cb->getnext(cursor->keyspec, &out_keyspec, appdata->qk_cb->ctx);

        if ((rs != RW_STATUS_SUCCESS) || (!out_keyspec)) {
          return NULL;
        }

        if (appdata->qk_cb->copy) {
          rs = appdata->qk_cb->copy(shard_tab, out_keyspec, &out_msg, appdata->qk_cb->ctx);
        }

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        cursor->keyspec = out_keyspec;

        if (keyspec)
        {
          rw_keyspec_path_t *merged_keyspec = NULL;
          rw_status_t rw_status = RW_KEYSPEC_PATH_CREATE_DUP(out_keyspec,
                                                             NULL,
                                                             &merged_keyspec,
                                                             NULL);
          if (rw_status != RW_STATUS_SUCCESS) {
            return NULL;
          }
          *keyspec = merged_keyspec;
        }
      }
      break;
    }

    default:
      break;
  }

  return out_msg;
}

const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_pathentry(rwdts_shard_handle_t*    shard_tab,
                                                          rwdts_appdata_cursor_t*   cursorh,
                                                          rw_keyspec_entry_t**     pathentry)
{
  rwdts_appdata_cursor_impl_t*  cursor   = (rwdts_appdata_cursor_impl_t*) cursorh;
  rw_status_t rs;

  RW_ASSERT(shard_tab);
  RW_ASSERT_TYPE(cursor, rwdts_appdata_cursor_impl_t);

  rwdts_appdata_t* appdata = shard_tab->appdata;

  if (!appdata) {
    return NULL;
  }

  rwdts_api_t *apih = shard_tab->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rw_keyspec_entry_t* out_entry = NULL;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {

    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb && appdata->sp_cb->getnext) {
        rs = appdata->sp_cb->getnext(cursor->pathentry, &out_entry, appdata->sp_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->sp_cb->take) {
          rs = appdata->sp_cb->take(shard_tab, out_entry, &out_msg, appdata->sp_cb->ctx);

          if (rs != RW_STATUS_SUCCESS) {
            return NULL;
          }

          cursor->pathentry = out_entry;

          if (pathentry)
          {
            rw_keyspec_entry_t *merged_entry = NULL;
            rw_status_t rw_status = rw_keyspec_entry_create_dup(out_entry,
                                                                NULL,
                                                                &merged_entry);
            if (rw_status != RW_STATUS_SUCCESS) {
              return NULL;
            }
            *pathentry = merged_entry;
          }

          rwdts_appdata_start_put_tmr(shard_tab, appdata, out_msg, (void *)out_entry, RWDTS_APPDATA_PATHENTRY);
        }
      }
      break;
    }

    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      if (appdata->up_cb && appdata->up_cb->getnext) {
        rs = appdata->up_cb->getnext(cursor->pathentry, &out_entry, appdata->up_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->up_cb->get) {
          rs = appdata->up_cb->get(shard_tab, out_entry, &out_msg, appdata->up_cb->ctx);

          if (rs != RW_STATUS_SUCCESS) {
            return NULL;
          }

          cursor->pathentry = out_entry;

          if (pathentry)
          {
            rw_keyspec_entry_t *merged_entry = NULL;
            rw_status_t rw_status = rw_keyspec_entry_create_dup(out_entry,
                                                                NULL,
                                                                &merged_entry);
            if (rw_status != RW_STATUS_SUCCESS) {
              return NULL;
            }
            *pathentry = merged_entry;
          }
        }
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb && appdata->qp_cb->getnext) {
        rs = appdata->qp_cb->getnext(cursor->pathentry, &out_entry, appdata->qp_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->qp_cb->copy) {
          rs = appdata->qp_cb->copy(shard_tab, out_entry, &out_msg, appdata->qp_cb->ctx);

          if (rs != RW_STATUS_SUCCESS) {
            return NULL;
          }

          cursor->pathentry = out_entry;

          if (pathentry)
          {
            rw_keyspec_entry_t *merged_entry = NULL;
            rw_status_t rw_status = rw_keyspec_entry_create_dup(out_entry,
                                                                NULL,
                                                                &merged_entry);
            if (rw_status != RW_STATUS_SUCCESS) {
              return NULL;
            }
            *pathentry = merged_entry;
          }
        }
      }
      break;
    }
    default:
      break;
  }

  return out_msg;
}

const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_minikey(rwdts_shard_handle_t*    shard_tab,
                                                        rwdts_appdata_cursor_t*   cursorh,
                                                        rw_schema_minikey_t** minikey)
{
  rwdts_appdata_cursor_impl_t*  cursor   = (rwdts_appdata_cursor_impl_t*) cursorh;
  rw_status_t rs;

  RW_ASSERT(shard_tab);
  RW_ASSERT_TYPE(cursor, rwdts_appdata_cursor_impl_t);

  rwdts_appdata_t* appdata = shard_tab->appdata;

  if (!appdata) {
    return NULL;
  }

  rwdts_api_t *apih = shard_tab->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rw_schema_minikey_t* out_minikey = NULL;
  ProtobufCMessage* out_msg = NULL;

  switch(appdata->appdata_type) {

    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb && appdata->sm_cb->getnext) {
        rs = appdata->sm_cb->getnext(cursor->minikey, &out_minikey, appdata->sm_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->sm_cb->take) {
          rs = appdata->sm_cb->take(shard_tab, out_minikey, &out_msg, appdata->sm_cb->ctx);

          if (rs != RW_STATUS_SUCCESS) {
            return NULL;
          }

          cursor->minikey = out_minikey;

          if (minikey)
          {
            rw_schema_minikey_t* merged_key = RW_MALLOC0(sizeof(rw_schema_minikey_t));
            memcpy(merged_key, out_minikey, sizeof(rw_schema_minikey_t));
            *minikey = merged_key;
          }

          rwdts_appdata_start_put_tmr(shard_tab, appdata, out_msg, (void *)out_minikey, RWDTS_APPDATA_MINIKEY);
        }
      }
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      if (appdata->um_cb && appdata->um_cb->getnext) {
        rs = appdata->um_cb->getnext(cursor->minikey, &out_minikey, appdata->um_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->um_cb->get) {
          rs = appdata->um_cb->get(shard_tab, out_minikey, &out_msg, appdata->um_cb->ctx);
        }

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        cursor->minikey = out_minikey;

         if (minikey)
         {
           rw_schema_minikey_t* merged_key = RW_MALLOC0(sizeof(rw_schema_minikey_t));
           memcpy(merged_key, out_minikey, sizeof(rw_schema_minikey_t));
           *minikey = merged_key;
         }
      }
      break;
    }

    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb && appdata->qm_cb->getnext) {
        rs = appdata->qm_cb->getnext(cursor->minikey, &out_minikey, appdata->qm_cb->ctx);

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        if (appdata->qm_cb->copy) {
          rs = appdata->qm_cb->copy(shard_tab, out_minikey, &out_msg, appdata->qm_cb->ctx);
        }

        if (rs != RW_STATUS_SUCCESS) {
          return NULL;
        }

        cursor->minikey = out_minikey;

         if (minikey)
         {
           rw_schema_minikey_t* merged_key = RW_MALLOC0(sizeof(rw_schema_minikey_t));
           memcpy(merged_key, out_minikey, sizeof(rw_schema_minikey_t));
           *minikey = merged_key;
         }
      }
      break;
    }

    default:
      break;
  }

  return out_msg;
}

rw_status_t
rwdts_shard_handle_appdata_register_keyspec_pbref_install(rwdts_shard_handle_t* shard,
                                                          rw_keyspec_path_t* keyspec,
                                                          ProtobufCMessage* pbcm,
                                                          void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rw_keyspec_path_t *dup_keyspec = NULL;
  rw_status_t rw_status;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC) {
    return RW_STATUS_FAILURE;
  }

  appdata->installed = true;

  rw_status = RW_KEYSPEC_PATH_CREATE_DUP(keyspec, NULL , &dup_keyspec, NULL);

  if (rw_status != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(dup_keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    RW_KEYSPEC_PATH_FREE(dup_keyspec, NULL, NULL);
    return RW_STATUS_FAILURE;
  }

  rwdts_shard_appdata_object_t* app_obj = RW_MALLOC0(sizeof(rwdts_shard_appdata_object_t));
  app_obj->key = key;
  app_obj->key_len = key_len;

  app_obj->keyspec = dup_keyspec;
  app_obj->msg = pbcm;

  HASH_ADD_KEYPTR(hh_data, shard->obj_list, app_obj->key, app_obj->key_len, app_obj);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_keyspec_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                            rw_keyspec_path_t* keyspec,
                                                            void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rwdts_shard_appdata_object_t* app_obj = NULL;
  rw_status_t rw_status;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC) {
    return RW_STATUS_FAILURE;
  }

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }

  HASH_FIND(hh_data, shard->obj_list, key, key_len, app_obj);

  if (!app_obj) {
    return RW_STATUS_FAILURE;
  }

  HASH_DELETE(hh_data, shard->obj_list, app_obj);
  
  RW_KEYSPEC_PATH_FREE(app_obj->keyspec, NULL, NULL);

  RW_FREE(app_obj);
   
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_pathentry_pbref_install(rwdts_shard_handle_t* shard,
                                                            rw_keyspec_entry_t* pathentry,
                                                            ProtobufCMessage* pbcm,
                                                            void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rw_keyspec_entry_t *dup_entry = NULL;
  rw_status_t rw_status;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_PE) {
    return RW_STATUS_FAILURE;
  }

  rw_status = rw_keyspec_entry_create_dup(pathentry, NULL , &dup_entry);

  if (rw_status != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }
 
  key = protobuf_c_message_serialize(NULL, (const struct ProtobufCMessage *)pathentry, &key_len); 
  if (!key) {
    rw_keyspec_entry_free(dup_entry, NULL);
    return RW_STATUS_FAILURE;
  }
  
  rwdts_shard_appdata_object_t* app_obj = RW_MALLOC0(sizeof(rwdts_shard_appdata_object_t));
  app_obj->key = key;
  app_obj->key_len = key_len;

  app_obj->pathentry = dup_entry;
  app_obj->msg = pbcm;
  
  HASH_ADD_KEYPTR(hh_data, shard->obj_list, app_obj->key, app_obj->key_len, app_obj);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_pathentry_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                              rw_keyspec_entry_t* pathentry,
                                                              void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rwdts_shard_appdata_object_t* app_obj = NULL;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_PE) {
    return RW_STATUS_FAILURE;
  }

  key = protobuf_c_message_serialize(NULL, (const struct ProtobufCMessage *)pathentry, &key_len);
  if (!key) {
    return RW_STATUS_FAILURE;
  }

  HASH_FIND(hh_data, shard->obj_list, key, key_len, app_obj);

  if (!app_obj) {
    RW_FREE(key);
    return RW_STATUS_FAILURE;
  }

  HASH_DELETE(hh_data, shard->obj_list, app_obj);

  RW_FREE(key);
  rw_keyspec_entry_free(app_obj->pathentry, NULL);
  RW_FREE(app_obj->key);

  RW_FREE(app_obj);

  return RW_STATUS_SUCCESS;

}

rw_status_t
rwdts_shard_handle_appdata_register_minikey_pbref_install(rwdts_shard_handle_t* shard,
                                                          rw_schema_minikey_t* minikey,
                                                          ProtobufCMessage* pbcm,
                                                          void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*     dup_minikey = NULL;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY) {
    return RW_STATUS_FAILURE;
  }

  dup_minikey = RW_MALLOC0(sizeof(rw_schema_minikey_t));
  RW_ASSERT(dup_minikey);
  if (!dup_minikey) {
    return RW_STATUS_FAILURE;
  }

  memcpy((char *)dup_minikey, (char *)minikey, sizeof(rw_schema_minikey_t));

  rwdts_shard_appdata_object_t* app_obj = RW_MALLOC0(sizeof(rwdts_shard_appdata_object_t));
  app_obj->key = dup_minikey;
  app_obj->key_len = sizeof(rw_schema_minikey_t);

  app_obj->minikey = (rw_schema_minikey_t *)dup_minikey;
  app_obj->msg = pbcm;

  HASH_ADD_KEYPTR(hh_data, shard->obj_list, app_obj->key, app_obj->key_len, app_obj);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_minikey_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                            rw_schema_minikey_t* minikey,
                                                            void* ud)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  rwdts_shard_appdata_object_t* app_obj = NULL;

  if (appdata->appdata_type != RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY) {
    return RW_STATUS_FAILURE;
  }

  HASH_FIND(hh_data, shard->obj_list, minikey, sizeof(rw_schema_minikey_t), app_obj);

  if (!app_obj) {
    return RW_STATUS_FAILURE;
  }

  HASH_DELETE(hh_data, shard->obj_list, app_obj);

  RW_FREE(app_obj->key);

  RW_FREE(app_obj);

  return RW_STATUS_SUCCESS;

}

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_keyspec_installed(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_path_t* keyspec)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rwdts_shard_appdata_object_t* app_obj = NULL;
  rw_status_t rw_status;

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    return NULL;
  }

  HASH_FIND(hh_data, shard->obj_list, key, key_len, app_obj);

  if (!app_obj) {
    return NULL;
  }

  return app_obj->msg;
}

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_pathentry_installed(rwdts_shard_handle_t* shard,
                                                   rw_keyspec_entry_t* pathentry)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  uint8_t*  key = NULL;
  size_t    key_len = 0;                    
  rwdts_shard_appdata_object_t* app_obj = NULL;

  key = protobuf_c_message_serialize(NULL, (const struct ProtobufCMessage *)pathentry, &key_len);
  if (!key) {
    return NULL;
  }
                                            
  HASH_FIND(hh_data, shard->obj_list, key, key_len, app_obj);

  RW_FREE(key);    
  if (!app_obj) {
    return NULL;
  }

  return app_obj->msg;
}       

static ProtobufCMessage*
rwdts_shard_handle_appdata_get_minikey_installed(rwdts_shard_handle_t* shard,
                                                 rw_schema_minikey_t* minikey)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  rwdts_shard_appdata_object_t* app_obj = NULL;

  HASH_FIND(hh_data, shard->obj_list, minikey, sizeof(rw_schema_minikey_t), app_obj);

  if (!app_obj) {
    return NULL;
  }

  return app_obj->msg;
}     

rw_status_t
rwdts_shard_handle_appdata_register_safe_pe(rwdts_shard_handle_t *shard_tab,
                                            rwdts_appdata_cb_getnext_pe getnext,
                                            rwdts_appdata_cb_safe_pe_rwref_take take,
                                            rwdts_appdata_cb_safe_pe_rwref_put put,
                                            rwdts_appdata_cb_safe_pe_create create,
                                            rwdts_appdata_cb_safe_pe_delete delete_fn,
                                            void *ud,
                                            GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_SAFE_PE;
  shard->appdata->sp_cb = RW_MALLOC0(sizeof(rwdts_appdata_safe_pe_cbset_t));
  RW_ASSERT(shard->appdata->sp_cb);
  RWDTS_UPDATE_SAFE_CB(p_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_unsafe_pe(rwdts_shard_handle_t *shard_tab,
                                              rwdts_appdata_cb_getnext_pe getnext,
                                              rwdts_appdata_cb_unsafe_pe_rwref_get get,
                                              rwdts_appdata_cb_unsafe_pe_create create,
                                              rwdts_appdata_cb_unsafe_pe_delete delete_fn,
                                              void *ud,
                                              GDestroyNotify dtor)   
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;


  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_UNSAFE_PE;
  shard->appdata->up_cb = RW_MALLOC0(sizeof(rwdts_appdata_unsafe_pe_cbset_t));
  RW_ASSERT(shard->appdata->up_cb);
  RWDTS_UPDATE_UNSAFE_CB(p_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_queue_pe(rwdts_shard_handle_t *shard_tab,
                                             rwdts_appdata_cb_getnext_pe getnext,
                                             rwdts_appdata_cb_pe_copy_get copy,
                                             rwdts_appdata_cb_pe_pbdelta pbdelta,
                                             void *ud,
                                             GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_QUEUE_PE;
  shard->appdata->qp_cb = RW_MALLOC0(sizeof(rwdts_appdata_queue_pe_cbset_t));
  RW_ASSERT(shard->appdata->qp_cb);
  RWDTS_UPDATE_QUEUE_CB(p_cb);

  return RW_STATUS_SUCCESS;
} 

rw_status_t
rwdts_shard_handle_appdata_register_safe_keyspec(rwdts_shard_handle_t *shard_tab,
                                                 rwdts_appdata_cb_getnext_keyspec getnext,
                                                 rwdts_appdata_cb_safe_ks_rwref_take take,
                                                 rwdts_appdata_cb_safe_ks_rwref_put put,
                                                 rwdts_appdata_cb_safe_ks_create create,
                                                 rwdts_appdata_cb_safe_ks_delete delete_fn,
                                                 void *ud,
                                                 GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);
  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_SAFE_KEYSPEC;
  shard->appdata->sk_cb = RW_MALLOC0(sizeof(rwdts_appdata_safe_keyspec_cbset_t));
  RW_ASSERT(shard->appdata->sk_cb);
  RWDTS_UPDATE_SAFE_CB(k_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_unsafe_keyspec(rwdts_shard_handle_t *shard_tab,
                                                   rwdts_appdata_cb_getnext_keyspec getnext,
                                                   rwdts_appdata_cb_unsafe_ks_rwref_get get,
                                                   rwdts_appdata_cb_unsafe_ks_create create,
                                                   rwdts_appdata_cb_unsafe_ks_delete delete_fn,
                                                   void *ud,
                                                   GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC;
  shard->appdata->uk_cb = RW_MALLOC0(sizeof(rwdts_appdata_unsafe_keyspec_cbset_t));
  RW_ASSERT(shard->appdata->uk_cb);
  RWDTS_UPDATE_UNSAFE_CB(k_cb);

  return RW_STATUS_SUCCESS;
}  

rw_status_t
rwdts_shard_handle_appdata_register_queue_keyspec(rwdts_shard_handle_t *shard_tab,
                                                  rwdts_appdata_cb_getnext_keyspec getnext,
                                                  rwdts_appdata_cb_ks_copy_get copy,
                                                  rwdts_appdata_cb_ks_pbdelta pbdelta,
                                                  void *ud,
                                                  GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC;
  shard->appdata->qk_cb = RW_MALLOC0(sizeof(rwdts_appdata_queue_keyspec_cbset_t));
  RW_ASSERT(shard->appdata->qk_cb);
  RWDTS_UPDATE_QUEUE_CB(k_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_safe_minikey(rwdts_shard_handle_t *shard_tab,
                                                 rwdts_appdata_cb_getnext_minikey getnext,
                                                 rwdts_appdata_cb_safe_mk_rwref_take take,
                                                 rwdts_appdata_cb_safe_mk_rwref_put put,
                                                 rwdts_appdata_cb_safe_mk_create create,
                                                 rwdts_appdata_cb_safe_mk_delete delete_fn,
                                                 void *ud,
                                                 GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_SAFE_MINIKEY;
  shard->appdata->sm_cb = RW_MALLOC0(sizeof(rwdts_appdata_safe_minikey_cbset_t));
  RW_ASSERT(shard->appdata->sm_cb);
  RWDTS_UPDATE_SAFE_CB(m_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_unsafe_minikey(rwdts_shard_handle_t *shard_tab,
                                                   rwdts_appdata_cb_getnext_minikey getnext,
                                                   rwdts_appdata_cb_unsafe_mk_rwref_get get,
                                                   rwdts_appdata_cb_unsafe_mk_create create,
                                                   rwdts_appdata_cb_unsafe_mk_delete delete_fn,
                                                   void *ud,
                                                   GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY;
  shard->appdata->um_cb = RW_MALLOC0(sizeof(rwdts_appdata_unsafe_minikey_cbset_t));
  RW_ASSERT(shard->appdata->um_cb);
  RWDTS_UPDATE_UNSAFE_CB(m_cb);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_handle_appdata_register_queue_minikey(rwdts_shard_handle_t *shard_tab,
                                                  rwdts_appdata_cb_getnext_minikey getnext,
                                                  rwdts_appdata_cb_mk_copy_get copy,
                                                  rwdts_appdata_cb_mk_pbdelta pbdelta,
                                                  void *ud,
                                                  GDestroyNotify dtor)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (shard->appdata) {
    return RW_STATUS_FAILURE;
  }

  shard->appdata = RW_MALLOC0(sizeof(rwdts_appdata_t));
  RW_ASSERT(shard->appdata);

  shard->appdata->appdata_type = RWDTS_APPDATA_TYPE_QUEUE_MINIKEY;
  shard->appdata->qm_cb = RW_MALLOC0(sizeof(rwdts_appdata_queue_minikey_cbset_t));
  RW_ASSERT(shard->appdata->qm_cb);
  RWDTS_UPDATE_QUEUE_CB(m_cb);

  return RW_STATUS_SUCCESS;
}

void
rwdts_shard_handle_appdata_delete(rwdts_shard_handle_t* shard_tab)
{
  RW_ASSERT(shard_tab);
  rwdts_shard_t* shard = shard_tab;

  if (!shard->appdata) {
    return;
  }

  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  switch(appdata->appdata_type) {
    case (RWDTS_APPDATA_TYPE_SAFE_MINIKEY):
    {
      if (appdata->sm_cb->dtor) {
        appdata->sm_cb->dtor(appdata->sm_cb->ctx);
      }
      RW_FREE(appdata->sm_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_MINIKEY):
    {
      if (appdata->qm_cb->dtor) {
        appdata->qm_cb->dtor(appdata->qm_cb->ctx);
      }
      RW_FREE(appdata->qm_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY):
    {
      if (appdata->um_cb->dtor) {
        appdata->um_cb->dtor(appdata->um_cb->ctx);
      }
      RW_FREE(appdata->um_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_SAFE_KEYSPEC):
    {
      if (appdata->sk_cb->dtor) {
        appdata->sk_cb->dtor(appdata->sk_cb->ctx);
      }
      RW_FREE(appdata->sk_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC):
    {
      if (appdata->uk_cb->dtor) {
        appdata->uk_cb->dtor(appdata->uk_cb->ctx);
      }
      RW_FREE(appdata->uk_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC):
    {
      if (appdata->qk_cb->dtor) {
        appdata->qk_cb->dtor(appdata->qk_cb->ctx);
      }
      RW_FREE(appdata->qk_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_SAFE_PE):
    {
      if (appdata->sp_cb->dtor) {
        appdata->sp_cb->dtor(appdata->sp_cb->ctx);
      }
      RW_FREE(appdata->sp_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_UNSAFE_PE):
    {
      if (appdata->up_cb->dtor) {
        appdata->up_cb->dtor(appdata->up_cb->ctx);
      }
      RW_FREE(appdata->up_cb);
      break;
    }
    case (RWDTS_APPDATA_TYPE_QUEUE_PE):
    {
      if (appdata->qp_cb->dtor) {
        appdata->qp_cb->dtor(appdata->qp_cb->ctx);
      }
      RW_FREE(appdata->qp_cb);
      break;
    }
    default:
      break;
  }

  // Remove this registrations registered dta members
  rwdts_shard_appdata_object_t *appobj, *tmp;

  HASH_ITER(hh_data, shard->obj_list, appobj, tmp) {
    // Delete this object from the hash list
    HASH_DELETE(hh_data, shard->obj_list, appobj);
    /* Should DELETE event be sent to the subscribers if the
       shard being removed is for publishers? */
    if (appobj->keyspec) {
      RW_KEYSPEC_PATH_FREE(appobj->keyspec, NULL, NULL);
    } else {
      if (appobj->pathentry) {
        rw_keyspec_entry_free(appobj->pathentry, NULL);
      }
      RW_FREE(appobj->key);
    }
    RW_FREE(appobj); 
  }

  RW_FREE(appdata);
  return;
}

rw_status_t
rwdts_shard_handle_appdata_create_element_xpath(rwdts_shard_handle_t* shard,
                                                const char* xpath,
                                                const ProtobufCMessage*  msg)
{
  rw_keyspec_path_t *ks = NULL;
  rw_status_t rs = rwdts_api_keyspec_from_xpath(shard->apih, xpath, &ks);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  return rwdts_shard_handle_appdata_create_element_keyspec(shard, ks, msg);
}

rwdts_appdata_cursor_t*
rwdts_shard_handle_appdata_get_current_cursor(rwdts_shard_handle_t* shard)
{
  RW_ASSERT(shard);

  if (!shard->appdata) {
    return NULL;
  }

  rwdts_appdata_t* appdata = shard->appdata;

  if (!appdata->cursor) {
    appdata->cursor = RW_MALLOC0_TYPE(sizeof(rwdts_appdata_cursor_impl_t), rwdts_appdata_cursor_impl_t);
    RW_ASSERT(appdata->cursor);
  }
  return ((rwdts_appdata_cursor_t *)appdata->cursor);
}

void
rwdts_shard_handle_appdata_reset_cursor(rwdts_shard_handle_t* shard)
{
  RW_ASSERT(shard);

  if (!shard->appdata) {
    return;
  }

  rwdts_appdata_t* appdata = shard->appdata;

  if (!appdata->cursor) {
    return;
  }

  appdata->cursor->keyspec = NULL;
  appdata->cursor->pathentry = NULL;
  appdata->cursor->minikey = NULL;

  return;
}

static rw_status_t
rwdts_shard_handle_appdata_get_next_keyspec_installed(rwdts_shard_handle_t* shard,
                                                      rw_keyspec_path_t* keyspec,
                                                      rw_keyspec_path_t** out_keyspec)
{
  RW_ASSERT(shard);
  rwdts_appdata_t* appdata = shard->appdata;
  RW_ASSERT(appdata);

  if (!keyspec) {
    rwdts_shard_appdata_object_t *appobj, *tmp;
    HASH_ITER(hh_data, shard->obj_list, appobj, tmp) {
      *out_keyspec = appobj->keyspec;
      return RW_STATUS_SUCCESS;
    }
  }
      
  uint8_t*  key = NULL;
  size_t    key_len = 0;
  rwdts_shard_appdata_object_t* app_obj = NULL;
  rwdts_shard_appdata_object_t* tmp_obj = NULL;
  rw_status_t rw_status;

  rw_status = RW_KEYSPEC_PATH_GET_BINPATH(keyspec, NULL ,
                                          (const uint8_t **)&key,
                                          &key_len, NULL);
  if (rw_status != RW_STATUS_SUCCESS) {
    return RW_STATUS_FAILURE;
  }

  HASH_FIND(hh_data, shard->obj_list, key, key_len, app_obj);

  if (!app_obj) {
    return RW_STATUS_FAILURE;
  }

  tmp_obj = (rwdts_shard_appdata_object_t *)app_obj->hh_data.next; 
 
  if (!tmp_obj) {
    return RW_STATUS_FAILURE;
  } 

  *out_keyspec = tmp_obj->keyspec;
  return RW_STATUS_SUCCESS;
}

void
rwdts_appdata_cursor_reset(rwdts_appdata_cursor_t *cursor)
{
  // ATTN -- the following typecasting is risky --
  // Add proper typechecking once 7144 is supported
  ((rwdts_appdata_cursor_impl_t*)cursor)->keyspec = NULL;
  ((rwdts_appdata_cursor_impl_t*)cursor)->pathentry = NULL;
  ((rwdts_appdata_cursor_impl_t*)cursor)->minikey = NULL;
}

rwdts_appdata_cursor_t*
rwdts_appdata_cursor_ref(rwdts_appdata_cursor_t *boxed)
{
  return boxed;
}

void
rwdts_appdata_cursor_unref(rwdts_appdata_cursor_t *boxed)
{
  rwdts_appdata_cursor_reset(boxed);
  return;
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_appdata_cursor_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_appdata_cursor_t,
                    rwdts_appdata_cursor,
                    rwdts_appdata_cursor_ref,
                    rwdts_appdata_cursor_unref);
