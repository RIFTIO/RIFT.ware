
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_api.c
 * @author RIFT.io (info@riftio.com)
 * @date 2014/01/26
 * @brief Core API
 */

#include <stdlib.h>
#include <stdio.h>

#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_xpath.h>
#include <sys/time.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rwdts_appconf_api.h>
#include <rw_rand.h>
#include <rw_pb_schema.h>
#include <rwmemlogdts.h>
#include "rw-dts.pb-c.h"
#include "rw-keyspec-stats.pb-c.h"
#include "rw-pbc-stats.pb-c.h"
#include "rwvcs_rwzk.h"
#include "rw_json.h"
#include "rwtasklet.h"
#include "rwvcs_manifest.h"

void
rwdts_log_handler (const gchar   *log_domain,
           GLogLevelFlags log_level,
           const gchar   *message,
           gpointer       unused_data)
{
  gint64 fd = (gint64)unused_data;
  write(fd, message, strlen(message));
}

/********************************************************************
 *  static prototypes                                               *
 ********************************************************************/
static inline void rwdts_query_result_list_deinit(rw_dl_t *list);
static inline void rwdts_xact_result_list_deinit(rw_dl_t *list);
static inline rw_status_t rwdts_xact_block_deinit(struct rwdts_xact_block_s *block);
static inline void rwdts_xact_result_list_deinit(rw_dl_t *list);
static void rwdts_member_remove_kv_handle(rwdts_api_t *apih,
                                          rwdts_kv_table_handle_t *kv_tab_handle);

static int rwdts_reg_ct_ext_state(rwdts_api_t *apih, reg_state_t ct_state);
static int rwdts_reg_ct_ext(rwdts_api_t *apih);

static void rwdts_bcast_state_change(rwdts_api_t *apih);
static rw_status_t rwdts_query_error_deinit(rwdts_query_error_t *err);
#define RWDTS_PROTO_MSG_STR_MAX_SIZE 1024

static uint64_t rwdts_api_get_router_id(rwtasklet_info_ptr_t tasklet_info);
/*************************************************************
 * rwdts_api_t  GI registration                              *
 *************************************************************/


static rwdts_api_t *rwdts_api_ref(rwdts_api_t *api, const char *file, int line)
{
  RW_ASSERT_TYPE(api, rwdts_api_t);

#if 0
  printf("rwdts_api_ref:apih=%p ref_cnt=%d+1, from=%s:%d\n",
          api, api->ref_cnt, file?file:"(nil)", line);
#endif

  g_atomic_int_inc(&api->ref_cnt);
  return api;
}

static void rwdts_api_unref(rwdts_api_t *api, const char *file, int line)
{
  RW_ASSERT_TYPE(api, rwdts_api_t);

#if 0
  printf("rwdts_api_unref:apih=%p ref_cnt=%d-1, from=%s:%d\n",
          api, api->ref_cnt, file?file:"(nil)", line);
#endif
  if (!g_atomic_int_dec_and_test(&api->ref_cnt)) {
    return;
  }
  rwdts_api_deinit(api);
}

static rwdts_api_t *rwdts_api_ref_gi(rwdts_api_t *boxed)
{
  return(rwdts_api_ref(boxed,"From Gi", __LINE__));  
}

static void rwdts_api_unref_gi(rwdts_api_t *boxed)
{
  return(rwdts_api_unref(boxed, "From Gi", __LINE__));
}


static void
rwdts_xact_keyspec_err_cb(rw_keyspec_instance_t *ksi,
                          const char *msg)
{
  rwdts_xact_t *xact = (rwdts_xact_t *)ksi->user_data;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  if (xact)
  {
    xact->ksi_errstr = strdup(msg);
    RWDTS_API_LOG_XACT_EVENT(xact->apih,xact,RwDtsApiLog_notif_XactKeyspecError,
                             xact->ksi_errstr);
    if (xact->apih && xact->apih->rwtrace_instance) {
      RWTRACE_ERROR(xact->apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "KeySpec error : %s", msg);
    }
    rwdts_member_send_error(xact, NULL, NULL, NULL, NULL,
                            RW_STATUS_FAILURE, msg);
    DTS_PRINT("KeySpec Error %s\n", xact->ksi_errstr);
    rwdts_client_dump_xact_info(xact, "Keyspec error for xact");
  }
}

rw_status_t rwdts_xact_print(rwdts_xact_t *xact)
{
  rwdts_client_dump_xact_info(xact, "Incorrect xact destroy pending_rsp or More expected");
  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_reg_print(rwdts_member_registration_t *regh)
{
  DTS_PRINT("\n\t\t Reg-id %u  Keystr = %s", regh->reg_id, regh->keystr);
  DTS_PRINT("\n\t\t Reg state  = %u", regh->reg_state);
  DTS_PRINT("\n");
  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_api_print(rwdts_api_t *apih)
{
  DTS_PRINT("\n\t\t  Printing Info for DTS object   ");
  DTS_PRINT("\n\t Client Path %s: ", apih->client_path);
  DTS_PRINT("\n\t Router Path %s: ", apih->router_path);
  DTS_PRINT("\n\t\t Reg list");
  rwdts_member_registration_t *entry = NULL, *next_entry = NULL;

  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (entry)
  {
    next_entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
    rwdts_reg_print(entry);
    entry = next_entry;
  }
  DTS_PRINT("\n");
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_api_set_state(rwdts_api_t *apih, rwdts_state_t state)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rw_component_state vcs_state = RW_BASE_STATE_TYPE_INITIALIZING;
  char *instance_name = NULL;
  rwtasklet_info_ptr_t ti = apih->rwtasklet_info;
  bool update_zk = false;
  if (state == apih->dts_state)
  {
    return status;
  }
  RWDTS_API_LOG_EVENT(apih, DtsapiStateChanged,
                      RWLOG_ATTR_SPRINTF("!!!!!!!!rwdts_api_set_state !!!!!! %s changed %s->%s", apih->client_path, rwdts_state_str(apih->dts_state), rwdts_state_str(state)));
  if ((state - apih->dts_state) != 1)
  {
    {
      RWLOG_EVENT(
          apih->rwlog_instance,
          RwDtsApiLog_notif_ApiStateJump,
          apih->client_path,
          rwdts_state_str(apih->dts_state),
          rwdts_state_str(state));
    }
  }
  if (state < apih->dts_state)
  {
    RW_ASSERT_MESSAGE(0, "%s : State inconsistent from %s to %s \n",
                      apih->client_path,
                      rwdts_state_str(apih->dts_state),
                      rwdts_state_str(state));
  }
  apih->dts_state_change_count++;
  apih->dts_state = state;
  gettimeofday(&(apih->trace_dts_state[apih->dts_state_change_count%MAX_DTS_STATES_TRACE_SZ].tv), NULL);
  apih->trace_dts_state[apih->dts_state_change_count%MAX_DTS_STATES_TRACE_SZ].state = state;
  rwdts_bcast_state_change(apih);
  if (apih->state_change.cb)
  {
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Invoking state change callback %s, New State %s",
                 apih->client_path, rwdts_get_app_addr_res(apih, (void *)apih->state_change.cb),
                 rwdts_state_str(state));
    RWLOG_EVENT(apih->rwlog_instance,
                RwDtsApiLog_notif_DtsStateChange,
                apih->client_path,
                rwdts_state_str(state));
    (*apih->state_change.cb)(apih, apih->dts_state, apih->state_change.ud);
  }
  else
  {
    // App has not defined any callback
    // We support that ??
    apih->dts_state = RW_DTS_STATE_RUN;
    vcs_state = RW_BASE_STATE_TYPE_RUNNING;
    update_zk = true;
    RWLOG_EVENT(apih->rwlog_instance,
                RwDtsApiLog_notif_StateChangeError,
                apih->client_path,
                rwdts_state_str(state),
                "App has not defined any callback, skipping ahead to running state");
  }

  switch(apih->dts_state)
  {
    case RW_DTS_STATE_NULL:
    case RW_DTS_STATE_INIT:
      break;
    case RW_DTS_STATE_REGN_COMPLETE:
      vcs_state = RW_BASE_STATE_TYPE_RECOVERING;
      update_zk = true;
      break;
    case RW_DTS_STATE_CONFIG:
      break;
    case RW_DTS_STATE_RUN:
      vcs_state = RW_BASE_STATE_TYPE_RUNNING;
      update_zk = true;
      break;
    default:
      RW_ASSERT_MESSAGE(0, "%s: Unknown state %u \n",
                apih->client_path,
                apih->dts_state);
      break;
  };

  if(update_zk)
  {
    if (ti && ti->rwvcs)
    {
      instance_name = to_instance_name(ti->identity.rwtasklet_name, ti->identity.rwtasklet_instance_id);
      status = rwvcs_rwzk_update_state(ti->rwvcs, instance_name, vcs_state);
      if (status != RW_STATUS_SUCCESS)
      {
        //LOG
        RWLOG_EVENT(apih->rwlog_instance,
                    RwDtsApiLog_notif_StateChangeError,
                    apih->client_path,
                    rwdts_state_str(state),
                    "Zookeeper update Failed");
      }
      free(instance_name);
    }
    else
    {
      RWLOG_EVENT(apih->rwlog_instance,
                  RwDtsApiLog_notif_StateChangeError,
                  apih->client_path,
                  rwdts_state_str(state),
                  "Missing tasklet info");
    }
  }

  /* We may be waiting for all registrations to be ready and cache
     recovery to be done.  Check if that's already true to avoid
     waiting forever.  Otherwise wait for the config event from cache
     recovery. */
  if (apih->dts_state == RW_DTS_STATE_REGN_COMPLETE
      && rwdts_reg_ct_ext_state(apih, RWDTS_REG_USABLE) == rwdts_reg_ct_ext(apih)) {
    /* Go straight to CONFIG. Skip the ZK update. */
    RWLOG_EVENT(apih->rwlog_instance,
		RwDtsApiLog_notif_DtsStateChangeAdvance,
		apih->client_path,
		apih->router_path,
		rwdts_state_str(apih->dts_state));
    status = rwdts_api_set_state(apih, RW_DTS_STATE_CONFIG);
  }
  
  return status;
}

rwdts_state_t
rwdts_api_get_state(rwdts_api_t *apih)
{
  return apih->dts_state;
}

/* Return true iff all registrations with cache enabled have reached
   registration ready (in routers) and usable (cache repopulated)
   status */
bool rwdts_cache_reg_ok(rwdts_api_t *apih)
{
  rwdts_member_registration_t *entry = NULL;
  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while(entry)
  {
    if (((entry->flags & RWDTS_SUB_CACHE_POPULATE_FLAG) == RWDTS_SUB_CACHE_POPULATE_FLAG)
        && entry->reg_state != RWDTS_REG_USABLE)
    {
      return false;
    }
    entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
  }
  return true;
}

/* Return a simple count of the EXTERNAL registrations in the given state */
static int rwdts_reg_ct_ext_state(rwdts_api_t *apih, reg_state_t ct_state) {
  int ct = 0;
  rwdts_member_registration_t *entry = NULL;
  for (entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
       entry;
       entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element)) {

    if ((entry->flags & RWDTS_FLAG_INTERNAL_REG)) {
      continue;
    }
    if (entry->reg_state == ct_state) {
      ct++;
    }
  }
  return ct;
}

/* Return count of EXTERNAL registrations */
static int rwdts_reg_ct_ext(rwdts_api_t *apih) {
  int ct = 0;
  rwdts_member_registration_t *entry = NULL;
  for (entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
       entry;
       entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element)) {
    if ((entry->flags & RWDTS_FLAG_INTERNAL_REG)) {
      continue;
    }
    ct++;
  }
  return ct;
}

void
rwdts_state_transition(rwdts_api_t *apih, rwdts_state_t state)
{
  uint32_t reg_cache_ct = apih->reg_cache_ct;
  switch(state)
  {
    case RW_DTS_STATE_NULL:
    case RW_DTS_STATE_INIT:
    case RW_DTS_STATE_REGN_COMPLETE:
    case RW_DTS_STATE_RUN:
      //above cases are called by APPs
      break;
    case RW_DTS_STATE_CONFIG:
      if (apih->dts_state != RW_DTS_STATE_REGN_COMPLETE)
      {
        return;
      }
      if (apih->strict_check &&
          !rwdts_cache_reg_ok(apih))
      {
        if (apih->reg_usable >= reg_cache_ct)
        {
          RWDTS_API_LOG_EVENT(apih, DtsapiStateChanged,
                              RWLOG_ATTR_SPRINTF("%s : Inconsistent data cache_reg\n", apih->client_path));
        }
        return;
      }
      if (apih->reg_usable < reg_cache_ct)
      {
        RWDTS_API_LOG_EVENT(apih, RegNotComplete, 
                            RWLOG_ATTR_SPRINTF("All Regn not complete %u->%u", apih->reg_usable, reg_cache_ct));
        return;
      }
      RWDTS_API_LOG_EVENT(apih, RegComplete,
                          RWLOG_ATTR_SPRINTF("All Regn complete %u->%u", apih->reg_usable, reg_cache_ct));
      break;
    default:
      RW_ASSERT_MESSAGE(0, "%s: Unknown state %u \n",
                apih->client_path,
                apih->dts_state);
      break;
  }
  rwdts_api_set_state(apih, state);
}
/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_api_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_api_t,
                    rwdts_api,
                    rwdts_api_ref_gi,
                    rwdts_api_unref_gi);


/*************************************************************
 * rwdts_xact_status_t GI registration                              *
 *************************************************************/

rwdts_xact_status_t
*rwdts_xact_status_ref(rwdts_xact_status_t *boxed)
{
  rwdts_xact_status_t* new_boxed = RW_MALLOC0_TYPE(sizeof(rwdts_xact_status_t), rwdts_xact_status_t);
  memcpy(new_boxed, boxed, sizeof(*new_boxed));
  return new_boxed;
}

void
rwdts_xact_status_unref(rwdts_xact_status_t *boxed)
{
  RW_ASSERT_TYPE(boxed, rwdts_xact_status_t);
  RW_FREE_TYPE(boxed, rwdts_xact_status_t);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_xact_status_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_xact_status_t,
                    rwdts_xact_status,
                    rwdts_xact_status_ref,
                    rwdts_xact_status_unref);




/*************************************************************
 * rwdts_xact_t GI registration                              *
 *************************************************************/

rwdts_xact_t
*rwdts_xact_ref(rwdts_xact_t *x, const char *file, int line)
{
  RW_ASSERT_TYPE(x, rwdts_xact_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(x->apih, x, XactRefIncr, 
                                 RWLOG_ATTR_SPRINTF("client=%s ref   ref_cnt=%d+1, from=%s:%d",
                                                    x->apih->client_path, x->ref_cnt, file?file:"(nil)", line));

  g_atomic_int_inc(&x->ref_cnt);
  return x;
}

void
rwdts_xact_unref(rwdts_xact_t *x, const char *file, int line)
{

  RW_ASSERT_TYPE(x, rwdts_xact_t);
   RWDTS_API_LOG_XACT_DEBUG_EVENT(x->apih, x, XactRefDecr, 
                                  RWLOG_ATTR_SPRINTF("client=%s unref ref_cnt=%d-1, from=%s:%d",
                                                     x->apih->client_path, x->ref_cnt, file?file:"(nil)", line));

  if (!g_atomic_int_dec_and_test(&x->ref_cnt)) {
    return;
  }
  rwdts_xact_deinit(x);
}

static rwdts_xact_t
*rwdts_xact_ref_gi(rwdts_xact_t *boxed)
{
  rwdts_xact_ref(boxed, "from Gi", 0);
  return boxed;
}
static void
rwdts_xact_unref_gi(rwdts_xact_t *boxed)
{
  rwdts_xact_unref(boxed, "from Gi", 0);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_xact_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_xact_t,
                    rwdts_xact,
                    rwdts_xact_ref_gi,
                    rwdts_xact_unref_gi);

/*************************************************************
 * rwdts_member_cursor_t  GI registration                    *
 *************************************************************/

rwdts_member_cursor_t*
rwdts_member_cursor_ref(rwdts_member_cursor_t *boxed)
{
  return boxed;
}

void
rwdts_member_cursor_unref(rwdts_member_cursor_t *boxed)
{
  rwdts_member_cursor_reset(boxed);
  return;
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_member_cursor_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_member_cursor_t,
                    rwdts_member_cursor,
                    rwdts_member_cursor_ref,
                    rwdts_member_cursor_unref);

/*************************************************************
 * rwdts_xact_block_t GI registration                              *
 *************************************************************/

rwdts_xact_block_t
*rwdts_xact_block_ref(rwdts_xact_block_t *boxed, const char *file, int line)
{
  rwdts_xact_t *x = boxed->xact;
  RW_ASSERT(x);
  RW_ASSERT_TYPE(x, rwdts_xact_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(x->apih, x, BlockRefIncr,
                                 RWLOG_ATTR_SPRINTF("client=%s block ref   ref_cnt=%d+1, from=%s:%d",
                                                    x->apih->client_path, boxed->ref_cnt, file?file:"(nil)", line));
  g_atomic_int_inc(&boxed->ref_cnt);
  return boxed;
}

void
rwdts_xact_block_unref(rwdts_xact_block_t *boxed, const char *file, int line)
{
  rwdts_xact_t *x = boxed->xact;
  RW_ASSERT(x);
  RW_ASSERT_TYPE(x, rwdts_xact_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(x->apih, x, BlockRefDecr,
                                 RWLOG_ATTR_SPRINTF("client=%s block unref   ref_cnt=%d-1, from=%s:%d",
                                                    x->apih->client_path, boxed->ref_cnt, file?file:"(nil)", line));
  if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) {
    return;
  }
  rwdts_xact_block_deinit(boxed);
}
static rwdts_xact_block_t
*rwdts_xact_block_ref_gi(rwdts_xact_block_t *boxed)
{
  rwdts_xact_block_ref(boxed, "from Gi", 0);
  return boxed;
}
static void
rwdts_xact_block_unref_gi(rwdts_xact_block_t *boxed)
{
  rwdts_xact_block_unref(boxed, "from Gi", 0);
}


/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_xact_block_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_xact_block_t,
                    rwdts_xact_block,
                    rwdts_xact_block_ref_gi,
                    rwdts_xact_block_unref_gi);

/*************************************************************
 * rwdts_member_reg_handle_t GI registration                 *
 *************************************************************/
static rwdts_member_reg_handle_t*
rwdts_member_reg_handle_gi_ref(rwdts_member_reg_handle_t* boxed)
{
  return (rwdts_member_reg_handle_t*)rwdts_member_reg_handle_ref(((rwdts_member_reg_handle_t)boxed));
}

static void
rwdts_member_reg_handle_gi_unref(rwdts_member_reg_handle_t* boxed)
{
  rwdts_member_reg_handle_unref((rwdts_member_reg_handle_t)boxed);
}

rwdts_member_reg_handle_t
rwdts_member_reg_handle_ref(rwdts_member_reg_handle_t boxed)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)boxed;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  g_atomic_int_inc(&reg->ref_cnt);
  return boxed;
}

void
rwdts_member_reg_handle_unref(rwdts_member_reg_handle_t boxed)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)boxed;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  if (!g_atomic_int_dec_and_test(&reg->ref_cnt)) {
    return;
  }
  rwdts_member_registration_delete(reg);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_member_reg_handle_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_member_reg_handle_t,
                    rwdts_member_reg_handle,
                    rwdts_member_reg_handle_gi_ref,
                    rwdts_member_reg_handle_gi_unref);


/*************************************************************
 * rwdts_group_t GI registration                             *
 *************************************************************/
static rwdts_group_t*
rwdts_group_ref(rwdts_group_t *boxed)
{
//  g_atomic_int_inc(&boxed->ref_cnt);
  return boxed;
}

static void
rwdts_group_unref(rwdts_group_t *boxed)
{
/*
  if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) {
    return;
  }
*/
}
/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_member_reg_handle_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_group_t,
                    rwdts_group,
                    rwdts_group_ref,
                    rwdts_group_unref);


/*************************************************************
 * rwdts_appconf_t GI registration                           *
 *************************************************************/
rwdts_appconf_t*
rwdts_appconf_ref(rwdts_appconf_t *boxed)
{
  g_atomic_int_inc(&boxed->ref_cnt);
  RWDTS_API_LOG_EVENT(boxed->apih, AppconfRefIncr, 
                      RWLOG_ATTR_SPRINTF("rwdts_appconf %p ref bumped now %d",
                                         boxed, boxed->ref_cnt));
  return boxed;
}

void
rwdts_appconf_unref(rwdts_appconf_t *boxed)
{
  RWDTS_API_LOG_EVENT(boxed->apih, AppconfRefDecr,
                      RWLOG_ATTR_SPRINTF("rwdts_appconf %p ref decrementing from %d",
                                         boxed, boxed->ref_cnt));
  if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) {
    return;
  }
  RWDTS_API_LOG_EVENT(boxed->apih, AppconfRefDecr,
                      RWLOG_ATTR_SPRINTF("rwdts_appconf %p ref hit zero now destroying", boxed));
  rwdts_appconf_group_destroy(boxed);
}

G_DEFINE_BOXED_TYPE(rwdts_appconf_t,
                    rwdts_appconf,
                    rwdts_appconf_ref,
                    rwdts_appconf_unref);



/*************************************************************
 * rwdts_xact_info_t GI registration                         *
 *************************************************************/
rwdts_xact_info_t*
rwdts_xact_info_ref(const rwdts_xact_info_t *boxed)
{
  rwdts_xact_info_t* new_boxed = RW_MALLOC0_TYPE(sizeof(rwdts_xact_info_t), rwdts_xact_info_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(boxed->apih, boxed->xact, XactinfoRefIncr,
                                 RWLOG_ATTR_SPRINTF("XACTINFO ref (copy %p -> %p)", boxed, new_boxed));
  memcpy(new_boxed, boxed, sizeof(*new_boxed));
  return new_boxed;
}

void
rwdts_xact_info_unref(rwdts_xact_info_t *boxed)
{
  RW_ASSERT_TYPE(boxed, rwdts_xact_info_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(boxed->apih, boxed->xact, XactinfoRefDecr,
                                 RWLOG_ATTR_SPRINTF("XACTINFO unref %p (free)", boxed));
  RW_FREE_TYPE(boxed, rwdts_xact_info_t);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_xact_info_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_xact_info_t,
                    rwdts_xact_info,
                    rwdts_xact_info_ref,
                    rwdts_xact_info_unref);

/*************************************************************
 * rwdts_state_t enum registration                           *
 *************************************************************/
static const GEnumValue _rwdts_state_enum_values[] = {
  { RW_DTS_STATE_NULL,        "NULL",        "NULL" },
  { RW_DTS_STATE_INIT,        "INIT",        "INIT" },
  { RW_DTS_STATE_REGN_COMPLETE,"REGN_COMPLETE", "REGN_COMPLETE" },
  { RW_DTS_STATE_CONFIG,      "CONFIG",      "CONFIG" },
  { RW_DTS_STATE_RUN,         "RUN",         "RUN" },
  { 0, NULL, NULL}
};
/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_state_get_type()
 */
GType rwdts_state_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_state_t",
                                  _rwdts_state_enum_values);

  return type;
}

/*************************************************************
 * RwDtsQueryAction enum registration                        *
 *************************************************************/

static const GEnumValue _rwdts_query_action_enum_values[] = {
  { RWDTS_QUERY_CREATE, "CREATE", "CREATE" },
  { RWDTS_QUERY_READ,   "READ",   "READ"   },
  { RWDTS_QUERY_UPDATE, "UPDATE", "UPDATE" },
  { RWDTS_QUERY_DELETE, "DELETE", "DELETE" },
  { RWDTS_QUERY_RPC,    "RPC",    "RPC"    },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_query_action_get_type()
 */
GType rwdts_query_action_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("RwDtsQueryAction",
                                  _rwdts_query_action_enum_values);

  return type;
}


/******
 * RWDtsXactMainState enum
 *****/

static const GEnumValue _rwdts_xact_status_enum_values[] = {
  { RWDTS_XACT_INIT,      "INIT", "INIT" },
  { RWDTS_XACT_RUNNING,   "RUNNING","RUNNING"   },
  { RWDTS_XACT_COMMITTED, "COMMITTED", "COMMITTED" },
  { RWDTS_XACT_ABORTED,   "ABORTED",    "ABORTED"    },
  { RWDTS_XACT_FAILURE,   "FAILURE",    "FAILURE"    },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 */
GType rwdts_xact_main_state_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("RwDtsXactMainState",
                                  _rwdts_xact_status_enum_values);

  return type;
}




/*************************************************************
 * rwdts_member_rsp_code_t enum registration                 *
 *************************************************************/
/*
 * this defines rwdts_member_rsp_code_get_type
 */
static const GEnumValue  _rwdts_member_rsp_code_values[] =  {
  { RWDTS_ACTION_OK,     "ACTION_OK",    "ACTION_OK" },
  { RWDTS_ACTION_ASYNC,  "ACTION_ASYNC", "ACTION_ASYNC" },
  { RWDTS_ACTION_NA,     "ACTION_NA",    "ACTION_NA" },
  { RWDTS_ACTION_NOT_OK, "ACTION_NOT_OK","ACTION_NOT_OK" },
  { 0, NULL, NULL}
};

GType rwdts_member_rsp_code_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_member_rsp_code_t",
                                  _rwdts_member_rsp_code_values);

  return type;
}

/*************************************************************
 * rwdts_event_t  enum registration                          *
 *************************************************************/

static const GEnumValue _rwdts_event_enum_values[] = {
  { RWDTS_EVENT_STATUS_CHANGE, "STATUS_CHANGE", "STATUS_CHANGE" },
  { RWDTS_EVENT_RESULT_READY,  "READY",         "READY" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_event_get_type()
 */
GType rwdts_event_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_event_t",
                                _rwdts_event_enum_values);

  return type;
}

/*************************************************************
 * rwdts_member_event_t enum registration                    *
 *************************************************************/
/*
 * this defines rwdts_member_event_get_type
 */
static const GEnumValue  _rwdts_member_event_values[] =  {
  { RWDTS_MEMBER_EVENT_PREPARE,  "PREPARE",   "PREPARE" },
  { RWDTS_MEMBER_EVENT_CHILD,    "CHILD",     "CHILD" },
  { RWDTS_MEMBER_EVENT_PRECOMMIT,"PRECOMMIT", "PRECOMMIT" },
  { RWDTS_MEMBER_EVENT_COMMIT,   "COMMIT",    "COMMIT" },
  { RWDTS_MEMBER_EVENT_PREPARE,  "ABORT",     "ABORT" },
  {RWDTS_MEMBER_EVENT_INSTALL, "INSTALL", "INSTALL" },
  { 0, NULL, NULL}
};

GType rwdts_member_event_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_member_event_t",
                                  _rwdts_member_event_values);

  return type;
}

/*************************************************************
 * RWDtsFlag            enum registration                    *
 *************************************************************/
/*
 * this defines rwdts_flag_get_type(void);
 */
static const GEnumValue  _rwdts_flag_values[] =  {
  { RWDTS_FLAG_SUBOBJECT,     "SUBOBJECT",  "SUBOBJECT" },
  { RWDTS_FLAG_DEPTH_FULL,    "DEPTH_FULL", "DEPTH_FULL" },
  { RWDTS_FLAG_DEPTH_LISTS,   "DEPTH_LISTS","DEPTH_LISTS" },
  { RWDTS_FLAG_DEPTH_ONE,     "DEPTH_ONE",  "DEPTH_ONE" },
  { RWDTS_FLAG_SUBSCRIBER,    "SUBSCRIBER", "SUBSCRIBER" },
  { RWDTS_FLAG_PUBLISHER,     "PUBLISHER",  "PUBLISHER" },
  { RWDTS_FLAG_DATASTORE,     "DATASTORE",  "DATASTORE" },
  { RWDTS_FLAG_CACHE,         "CACHE",      "CACHE" },
  { RWDTS_FLAG_SHARED,        "SHARED",     "SHARED" },
  { RWDTS_FLAG_NO_PREP_READ,  "NO_PREP_READ","NO_PREP_READ" },
  { RWDTS_FLAG_SHARDING,      "SHARDING",    "SHARDING" },
  { RWDTS_FLAG_DELTA_READY,   "DELTA_READY", "DELTA_READY" },
  { 0, NULL, NULL}
};

/*
 * this defines rwdts_xact_flag_get_type(void);
 */
static const GEnumValue  _rwdts_xact_flag_values[] =  {
  { RWDTS_XACT_FLAG_ADVISE,        "ADVISE",     "ADVISE" },
  { RWDTS_XACT_FLAG_BLOCK_MERGE,   "MERGE",      "MERGE" },
  { RWDTS_XACT_FLAG_STREAM,        "STREAM",     "STREAM" },
  { RWDTS_XACT_FLAG_TRACE,         "TRACE",      "TRACE" },
  { RWDTS_XACT_FLAG_RETURN_PAYLOAD,"RET_PAYLOAD","RET_PAYLOAD"},
  { RWDTS_XACT_FLAG_REPLACE,       "REPLACE",    "REPLACE" },
  { RWDTS_XACT_FLAG_ANYCAST,       "ANYCAST",    "ANYCAST" },
  { RWDTS_XACT_FLAG_SUB_READ,      "SUB_READ",   "SUB_READ" },
  { 0, NULL, NULL}
};

GType rwdts_flag_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("RWDtsFlag",
                                  _rwdts_flag_values);

  return type;
}

GType rwdts_xact_flag_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("RWDtsXactFlag",
                                  _rwdts_xact_flag_values);

  return type;
}

/*************************************************************
 * rwdts_group_phase_t enum registration                     *
 *************************************************************/

static const GEnumValue _rwdts_group_phase_enum_values[] = {
  { RWDTS_GROUP_PHASE_REGISTER, "REGISTER", "REGISTER" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_event_get_type()
 */
GType rwdts_group_phase_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_group_phase_t",
                                _rwdts_group_phase_enum_values);

  return type;
}

/*************************************************************
 * rwdts_appconf_action_t enum registration                  *
 *************************************************************/

static const GEnumValue _rwdts_appconf_action_enum_values[] = {
  { RWDTS_APPCONF_ACTION_INSTALL,   "INSTALL", "INSTALL" },
  { RWDTS_APPCONF_ACTION_RECONCILE, "RECONCILE", "RECONCILE" },
  { RWDTS_APPCONF_ACTION_AUDIT, "AUDIT", "AUDIT" },
  { RWDTS_APPCONF_ACTION_RECOVER, "RECOVER", "RECOVER" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_appconf_action_get_type()
 */
GType rwdts_appconf_action_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_appconf_action_t",
                                _rwdts_appconf_action_enum_values);

  return type;
}

/* Get the string associated with audit action
 *
 * This defines rwdts_appconf_action_get_type()
 */
const char * rwdts_action_str(rwdts_appconf_action_t action)
{
  if (action > RWDTS_APPCONF_ACTION_RECOVER)
    RW_CRASH();

  return _rwdts_appconf_action_enum_values[action-1].value_nick;
}

/*************************************************************
 * rwdts_xact_rsp_code_t enum registration                   *
 *************************************************************/

static const GEnumValue _rwdts_xact_rsp_code_values[] = {
  { RWDTS_XACT_RSP_CODE_ACK,  "ACK", "ACK" },
  { RWDTS_XACT_RSP_CODE_NACK, "NACK", "NACK" },
  { RWDTS_XACT_RSP_CODE_NA,   "NA",   "NA" },
  { RWDTS_XACT_RSP_CODE_MORE, "MORE", "MORE" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_xact_rsp_code_get_type()
 */
GType rwdts_xact_rsp_code_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_xact_rsp_code_t",
                                _rwdts_xact_rsp_code_values);
  return type;
}

/*************************************************************
 * rwdts_appconf_phase_t enum registration                   *
 *************************************************************/

static const GEnumValue _rwdts_appconf_phase_values[] = {
  { RWDTS_APPCONF_PHASE_REGISTER,  "REGISTER", "REGISTER" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_appconf_phase_get_type()
 */
GType rwdts_appconf_phase_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_appconf_phase_t",
                                  _rwdts_appconf_phase_values);
  return type;
}

/*************************************************************
 * rwdts_audit_action_t  enum registration                   *
 *************************************************************/

static const GEnumValue _rwdts_audit_action_values[] = {
  { RW_DTS_AUDIT_ACTION_REPORT_ONLY, "REPORT_ONLY", "REPORT_ONLY" },
  { RW_DTS_AUDIT_ACTION_RECONCILE,   "RECONCILE",   "RECONCILE" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_audit_action_get_type()
 */
GType rwdts_audit_action_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_audit_action_t",
                                  _rwdts_audit_action_values);
  return type;
}

/*************************************************************
 * rwdts_audit_status_t  enum registration                   *
 *************************************************************/

static const GEnumValue _rwdts_audit_status_values[] = {
  { RW_DTS_AUDIT_STATUS_SUCCESS, "SUCCESS", "SUCCESS" },
  { RW_DTS_AUDIT_STATUS_FAILURE, "FAILURE", "FAILURE" },
  { RW_DTS_AUDIT_STATUS_ABORTED, "ABORTED", "ABORTED" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_audit_status_get_type()
 */
GType rwdts_audit_status_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_audit_status_t",
                                  _rwdts_audit_status_values);
  return type;
}


/*************************************************************
 * rwdts_audit_result_t GI registration                      *
 *************************************************************/
rwdts_audit_result_t *
rwdts_audit_result_new ()
{
  rwdts_audit_result_t *res =
    RW_MALLOC0_TYPE(sizeof(rwdts_audit_result_t), rwdts_audit_result_t);
  g_atomic_int_set(&res->ref_cnt, 1);
  return res;
}

static rw_status_t
rwdts_audit_result_deinit(rwdts_audit_result_t *res)
{
  if (res == NULL) {
    return RW_STATUS_SUCCESS;
  }
  RW_ASSERT_TYPE(res, rwdts_audit_result_t);
  RW_ASSERT(res->ref_cnt == 0);
  if (res->ref_cnt) {
    return RW_STATUS_FAILURE;
  }
  if (res->audit_msg) {
    RW_FREE(res->audit_msg);
    res->audit_msg = NULL;
  }
  RW_FREE_TYPE(res, rwdts_audit_result_t);
  return RW_STATUS_SUCCESS;
}

rwdts_audit_result_t*
rwdts_audit_result_ref(rwdts_audit_result_t *boxed)
{
  g_atomic_int_inc(&boxed->ref_cnt);
  return boxed;
}

void
rwdts_audit_result_unref(rwdts_audit_result_t *boxed)
{
  if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) {
    return;
  }
  rwdts_audit_result_deinit(boxed);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_audit_result_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_audit_result_t,
                    rwdts_audit_result,
                    rwdts_audit_result_ref,
                    rwdts_audit_result_unref);

/*************************************************************
 * rwdts_query_result_t GI registration                      *
 *************************************************************/

static rwdts_query_result_t*
rwdts_query_result_ref(rwdts_query_result_t *qrslt)
{
  rwdts_query_result_t *dup_qrslt = rwdts_qrslt_dup(qrslt); 
  return dup_qrslt;
}

static void
rwdts_query_result_unref(rwdts_query_result_t *qrslt)
{
  rwdts_query_result_deinit(qrslt); 
  RW_FREE(qrslt);
  return;
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_query_result_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_query_result_t,
                    rwdts_query_result,
                    rwdts_query_result_ref,
                    rwdts_query_result_unref);

/*************************************************************
 * rwdts_query_error_t GI registration                      *
 *************************************************************/
rwdts_query_error_t *
rwdts_query_error_new ()
{
  rwdts_query_error_t *err =
    RW_MALLOC0_TYPE(sizeof(rwdts_query_error_t), rwdts_query_error_t);
  g_atomic_int_set(&err->ref_cnt, 1);
  return err;
}

static rw_status_t
rwdts_query_error_deinit(rwdts_query_error_t *err)
{
  if (err == NULL) {
    return RW_STATUS_SUCCESS;
  }
  RW_ASSERT_TYPE(err, rwdts_query_error_t);
  RW_ASSERT(err->ref_cnt == 0);
  if (err->ref_cnt) {
    return RW_STATUS_FAILURE;
  }
  if (err->error) {
    protobuf_c_message_free_unpacked(NULL, err->error);
    err->error = NULL;
  }
  if (err->keyspec) {
    rw_keyspec_path_free(err->keyspec, NULL);
    err->keyspec = NULL;
  }
  if (err->key_xpath) {
    RW_FREE(err->key_xpath);
    err->key_xpath = NULL;
  }
  if (err->keystr) {
    RW_FREE(err->keystr);
    err->keystr = NULL;
  }
  if (err->errstr) {
    RW_FREE(err->errstr);
    err->errstr = NULL;
  }
  RW_FREE_TYPE(err, rwdts_query_error_t);
  return RW_STATUS_SUCCESS;
}

rwdts_query_error_t*
rwdts_query_error_ref(rwdts_query_error_t *boxed)
{
  g_atomic_int_inc(&boxed->ref_cnt);
  return boxed;
}

void
rwdts_query_error_unref(rwdts_query_error_t *boxed)
{
  if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) {
    return;
  }
  rwdts_query_error_deinit(boxed);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_query_error_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_query_error_t,
                    rwdts_query_error,
                    rwdts_query_error_ref,
                    rwdts_query_error_unref);


static inline unsigned long
str_hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

const char*
rwdts_xact_id_str(const RWDtsXactID* id,
                  char*              str,
                  size_t             str_size)
{
  RW_ASSERT(id != NULL);
  RW_ASSERT(str != NULL);
  if ((str_size==0)||(str==NULL)) {
    return NULL;
  }
  str[0] = 0;
  if (id) {
    snprintf(str, str_size, "%lu:%lu:%lu", id->router_idx, id->client_idx, id->serialno);
  }
  return str;
}


char*
rwdts_xact_get_id(const rwdts_xact_t *xact)
{
  char id_str[999];
  return strdup(rwdts_xact_id_str(&xact->id, id_str, sizeof(id_str)-1));
}

/*
 * caller owns the string
 */
char*
rwdts_get_xact_state_string(rwdts_member_xact_state_t state)
{

  switch(state) {
    case RWDTS_MEMB_XACT_ST_INIT:
      return "INIT";
    case RWDTS_MEMB_XACT_ST_PREPARE:
      return "PREPARE";
    case RWDTS_MEMB_XACT_ST_PRECOMMIT:
      return "PRECOMMIT";
    case RWDTS_MEMB_XACT_ST_COMMIT:
      return "COMMIT";
    case RWDTS_MEMB_XACT_ST_COMMIT_RSP:
      return "COMMIT_RSP";
    case RWDTS_MEMB_XACT_ST_ABORT:
      return "ABORT";
    case RWDTS_MEMB_XACT_ST_ABORT_RSP:
      return "ABORT_RSP";
    case RWDTS_MEMB_XACT_ST_END:
      return "END";
    default:
      RWDTS_ASSERT_MESSAGE(0,0,0,0,"Xact Unknown state %u \n", state);
   }
   return strdup("INVALID");
}

static int
rwdts_fill_cache_entries(rwdts_member_registration_t *entry,
                         RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_CacheEntry)  **cache_entry)
{
  int num_entries = 0;
  rw_keyspec_path_t *keyspec = NULL;
  if (cache_entry)
  {
    if (entry)
    {
      rwdts_member_cursor_t *cursor = rwdts_member_data_get_current_cursor (NULL,
                                                                           (rwdts_member_reg_handle_t)entry);
      rwdts_member_data_reset_cursor(NULL,
                                     (rwdts_member_reg_handle_t)entry);
      const ProtobufCMessage *msg = rwdts_member_reg_handle_get_next_element ((rwdts_member_reg_handle_t)entry,
                                                                            cursor, NULL,
                                                                            &keyspec);
      while (msg)
      {
        RW_ASSERT(keyspec);
        cache_entry[num_entries] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_CacheEntry)*)
                                              RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_CacheEntry)));
        RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Registration_CacheEntry,cache_entry[num_entries]);
        rw_json_pbcm_to_json(msg,
                           NULL,
                           &cache_entry[num_entries]->pb);
        RW_KEYSPEC_PATH_GET_PRINT_BUFFER(keyspec, NULL, NULL, cache_entry[num_entries]->keyspec, sizeof(cache_entry[num_entries]->keyspec)-1, NULL);
        cache_entry[num_entries]->has_keyspec = true;
        keyspec = NULL;
        msg = rwdts_member_reg_handle_get_next_element ((rwdts_member_reg_handle_t)entry,
                                                       cursor,
                                                       NULL,
                                                       &keyspec);
        num_entries++;
      }
    }
  }
  return num_entries;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_mgmt_get_ks_ebuf(const rwdts_xact_info_t* xact_info,
                                     RWDtsQueryAction         action,
                                     const rw_keyspec_path_t* keyspec,
                                     const ProtobufCMessage*  msg,
                                     uint32_t                 credits,
                                     void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  rwdts_xact_t *xact = xact_info->xact;
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

#define ypb_path RwKeyspecStats_data_KeyspecEbuf_Tasklet

  RWPB_T_PATHSPEC(ypb_path) *ks = (RWPB_T_PATHSPEC(ypb_path) *)rw_keyspec_path_create_dup_of_type(
      keyspec, &xact->ksi, RWPB_G_PATHSPEC_PBCMD(ypb_path));
  RW_ASSERT(ks);
  if (ks == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "keyspec duplication failed");
    return RWDTS_ACTION_NOT_OK;
  }

  // Check if the name is present if so whether they match
  if (ks->dompath.path001.has_key00 &&
      !strstr(apih->client_path, ks->dompath.path001.key00.name)) {
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NA;
  }

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);
  if (action != RWDTS_QUERY_READ) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "only QUERY READ supported action=%s", action_str);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Form the response.
  RWPB_M_MSG_DECL_INIT(ypb_path, pb_rsp);
  strncpy(pb_rsp.name, apih->client_path, sizeof(pb_rsp.name) - 1);

  pb_rsp.n_error_records = 0;
  rw_keyspec_export_error_records(&apih->ksi, &pb_rsp.base, "error_records", "time_stamp", "error_str");

#undef ypb_path
  rw_keyspec_path_set_category (&ks->rw_keyspec_path_t, &xact->ksi, RW_SCHEMA_CATEGORY_DATA);
  ks->dompath.path001.has_key00 = 1;
  RW_ASSERT(strlen(apih->client_path) < sizeof(ks->dompath.path001.key00.name));
  strncpy(ks->dompath.path001.key00.name, apih->client_path, sizeof(ks->dompath.path001.key00.name)-1);

  ProtobufCMessage* rsp_arr[1] = { &pb_rsp.base };

  rwdts_member_query_rsp_t rsp = {
    .ks = &ks->rw_keyspec_path_t,
    .n_msgs = 1,
    .msgs = &rsp_arr[0],
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rw_status_t rs =
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  if (rs != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "QUERY READ send response failed:%d", rs);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
    return RWDTS_ACTION_NOT_OK;
  }
  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
  rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_mgmt_get_pbc_ebuf(const rwdts_xact_info_t* xact_info,
                                      RWDtsQueryAction         action,
                                      const rw_keyspec_path_t* keyspec,
                                      const ProtobufCMessage*  msg,
                                      uint32_t                 credits,
                                      void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  rwdts_xact_t *xact = xact_info->xact;
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

#define ypb_path RwPbcStats_data_PbcEbuf_Tasklet

  RWPB_T_PATHSPEC(ypb_path) *ks = (RWPB_T_PATHSPEC(ypb_path) *)rw_keyspec_path_create_dup_of_type(
      keyspec, &xact->ksi, RWPB_G_PATHSPEC_PBCMD(ypb_path));
  RW_ASSERT(ks);
  if (ks == NULL) {
   RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL,
           RWDTS_ACTION_NOT_OK, "keyspec duplication failed");
    return RWDTS_ACTION_NOT_OK;
  }

  // Check if the name is present if so whether they match
  if (ks->dompath.path001.has_key00 &&
      !strstr(apih->client_path, ks->dompath.path001.key00.name)) {
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NA;
  }

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);
  if (action != RWDTS_QUERY_READ) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "only QUERY READ supported action=%s", action_str);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Form the response.
  RWPB_M_MSG_DECL_INIT(ypb_path, pb_rsp);
  strncpy(pb_rsp.name, apih->client_path, sizeof(pb_rsp.name) - 1);

  /*
   * This is a kind of hack to export error buffers from protobufc library.
   * Since it is debug code is it ok?
   */
  pb_rsp.n_error_records = 0;
  protobuf_c_instance_export_ebuf(NULL, &pb_rsp.base, "error_records", "time_stamp", "error_str");

#undef ypb_path

  rw_keyspec_path_set_category (&ks->rw_keyspec_path_t, &xact->ksi, RW_SCHEMA_CATEGORY_DATA);
  ks->dompath.path001.has_key00 = 1;
  RW_ASSERT(strlen(apih->client_path) < sizeof(ks->dompath.path001.key00.name));
  strncpy(ks->dompath.path001.key00.name, apih->client_path, sizeof(ks->dompath.path001.key00.name)-1);

  ProtobufCMessage* rsp_arr[1] = { &pb_rsp.base };

  rwdts_member_query_rsp_t rsp = {
    .ks = &ks->rw_keyspec_path_t,
    .n_msgs = 1,
    .msgs = &rsp_arr[0],
    .evtrsp = RWDTS_EVTRSP_ACK
  };
  
  rw_status_t rs = 
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  if (rs != RW_STATUS_SUCCESS) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "send response failed for action=%s", action_str);
    protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }
  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
  rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_mgmt_get_pbc_stats(const rwdts_xact_info_t* xact_info,
                                       RWDtsQueryAction         action,
                                       const rw_keyspec_path_t* keyspec,
                                       const ProtobufCMessage*  msg,
                                       uint32_t                 credits,
                                       void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "rwdts_member_handle_mgmt_get_pbc_stats:invalid xact_info");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  rwdts_xact_t *xact = xact_info->xact;
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

#define ypb_path RwPbcStats_data_PbcStats_Member

  RWPB_T_PATHSPEC(ypb_path) *ks = (RWPB_T_PATHSPEC(ypb_path) *)rw_keyspec_path_create_dup_of_type(
      keyspec, &xact->ksi, RWPB_G_PATHSPEC_PBCMD(ypb_path));
  RW_ASSERT_MESSAGE(ks, "rwdts_member_handle_mgmt_get_pbc_stats:keyspec duplication failed");
  if (ks == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "rwdts_member_handle_mgmt_get_pbc_stats:keyspec duplication failed");
    return RWDTS_ACTION_NOT_OK;
  }

  // Check if the name is present if so whether they match
  if (ks->dompath.path001.has_key00 &&
      !strstr(apih->client_path, ks->dompath.path001.key00.name)) {
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NA;
  }

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);
  if (action != RWDTS_QUERY_READ) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "only QUERY_READ supported:action=%s", action_str);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Form the response.
  RWPB_M_MSG_DECL_INIT(ypb_path, pb_rsp);
  strncpy(pb_rsp.name, apih->client_path, sizeof(pb_rsp.name) - 1);

#define COPY_PBC_STATS(dest_, src_, name_) \
  if ((src_).name_ > 0) { \
    (dest_).has_##name_ = 1; \
    (dest_).name_ = (src_).name_; \
  }

  pb_rsp.has_fcall_stats = 1;
  pb_rsp.has_error_stats = 1;

  /*
   * DTS doesn't use a protobufc instance. For now using
   * the global default instance to fetch the stats. Fix this
   * once proper instance is in use.
   */

  ProtobufCInstance *pbc_inst = protobuf_c_instance_get(NULL); // default instance for now

  // copy path-spec stats...
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, pack);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, get_pack_sz);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, pack_to_buffer);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, unpack);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, check);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, merge);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, copy);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, free);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, duplicate);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, create);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, serialize);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, delete_unknown);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, delete_field);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, compare_keys);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, set_field_msg);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, set_field);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, get_field);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, get_field_txt);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, has_field);
  COPY_PBC_STATS(pb_rsp.fcall_stats, pbc_inst->stats.fcall, compare_fields);

  // copy error stats..
  COPY_PBC_STATS(pb_rsp.error_stats, pbc_inst->stats.error, total_errors);

#undef COPY_PBC_STATS
#undef ypb_path

  rw_keyspec_path_set_category (&ks->rw_keyspec_path_t, &xact->ksi, RW_SCHEMA_CATEGORY_DATA);
  ks->dompath.path001.has_key00 = 1;
  RW_ASSERT(strlen(apih->client_path) < sizeof(ks->dompath.path001.key00.name));
  strncpy(ks->dompath.path001.key00.name, apih->client_path, sizeof(ks->dompath.path001.key00.name)-1);

  ProtobufCMessage* rsp_arr[1] = { &pb_rsp.base };

  rwdts_member_query_rsp_t rsp = {
    .ks = &ks->rw_keyspec_path_t,
    .n_msgs = 1,
    .msgs = &rsp_arr[0],
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rw_status_t rs = rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  if (rs != RW_STATUS_SUCCESS) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "send response failed:action=%s", action_str);
    protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
  rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_mgmt_get_ks_stats(const rwdts_xact_info_t* xact_info,
                                      RWDtsQueryAction         action,
                                      const rw_keyspec_path_t* keyspec,
                                      const ProtobufCMessage*  msg,
                                      uint32_t                 credits,
                                      void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "xact_info NULL in rwdts_member_handle_mgmt_get_ks_stats");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  } 

  rwdts_xact_t *xact = xact_info->xact;
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

#define ypb_path RwKeyspecStats_data_KeyspecStats_Member

  RWPB_T_PATHSPEC(ypb_path) *ks = (RWPB_T_PATHSPEC(ypb_path) *)rw_keyspec_path_create_dup_of_type(
      keyspec, &xact->ksi, RWPB_G_PATHSPEC_PBCMD(ypb_path));
  RW_ASSERT(ks);
  if (ks==NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL,
           RWDTS_ACTION_NOT_OK, "keyspec duplication failed");
    return RWDTS_ACTION_NOT_OK;
  }

  // Check if the name is present if so whether they match
  if (ks->dompath.path001.has_key00 &&
      !strstr(apih->client_path, ks->dompath.path001.key00.name)) {
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NA;
  }

  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);
  if (action != RWDTS_QUERY_READ) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "only READ supported:action=%s", action_str);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Form the response.
  RWPB_M_MSG_DECL_INIT(ypb_path, pb_rsp);
  strncpy(pb_rsp.name, apih->client_path, sizeof(pb_rsp.name) - 1);

#define COPY_KS_STATS(dest_, src_, name_) \
  if ((src_).name_ > 0) { \
    (dest_).has_##name_ = 1; \
    (dest_).name_ = (src_).name_; \
  }

  pb_rsp.has_fcall_stats = 1;
  pb_rsp.fcall_stats.has_path_spec = 1;
  pb_rsp.fcall_stats.has_path_entry = 1;
  pb_rsp.has_error_stats = 1;

  // copy path-spec stats...
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, create_dup);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, create_dup_type);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, create_dup_type_trunc);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, find_spec_ks);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, get_binpath);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, set_category);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, has_entry);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_equal);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_equal_detail);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_match_detail);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_path_detail);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_branch_detail);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, append_entry);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, trunc_suffix);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, pack_dompath);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, create_with_binpath);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, create_with_dompath);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, free);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, find_mdesc);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, is_sub_ks);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, reroot);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, reroot_iter);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, reroot_and_merge);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, reroot_and_merge_op);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, add_keys);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, delete_proto_field);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, get_print_buff);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, from_xpath);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_spec, apih->ksi.stats.fcall.path, matches_msg);

  // copy path-entry stats..
  COPY_KS_STATS(pb_rsp.fcall_stats.path_entry, apih->ksi.stats.fcall.entry, create_dup);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_entry, apih->ksi.stats.fcall.entry, create_dup_type);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_entry, apih->ksi.stats.fcall.entry, pack);
  COPY_KS_STATS(pb_rsp.fcall_stats.path_entry, apih->ksi.stats.fcall.entry, unpack);

  // copy error stats..
  COPY_KS_STATS(pb_rsp.error_stats, apih->ksi.stats.error, total_errors);

#undef COPY_KS_STATS
#undef ypb_path

  rw_keyspec_path_set_category (&ks->rw_keyspec_path_t, &xact->ksi, RW_SCHEMA_CATEGORY_DATA);
  ks->dompath.path001.has_key00 = 1;
  RW_ASSERT(strlen(apih->client_path) < sizeof(ks->dompath.path001.key00.name));
  strncpy(ks->dompath.path001.key00.name, apih->client_path, sizeof(ks->dompath.path001.key00.name)-1);

  ProtobufCMessage* rsp_arr[1] = { &pb_rsp.base };

  rwdts_member_query_rsp_t rsp = {
    .ks = &ks->rw_keyspec_path_t,
    .n_msgs = 1,
    .msgs = &rsp_arr[0],
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rw_status_t rs = rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  if (rs != RW_STATUS_SUCCESS) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, keyspec, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "send response failed for action=%s", action_str);
    protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
    rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, &pb_rsp.base);
  rw_keyspec_path_free(&ks->rw_keyspec_path_t, NULL);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_mgmt_req(const rwdts_xact_info_t* xact_info,
                             RWDtsQueryAction         action,
                             const rw_keyspec_path_t* key,
                             const ProtobufCMessage*  msg,
                             uint32_t                 credits,
                             void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "xact_info NULL in rwdts_member_handle_mgmt_req");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  } 
  rwdts_xact_t *xact = xact_info->xact;
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_data_Dts_Member) *inp;

  RWPB_T_PATHSPEC(RwDts_data_Dts_Member) *dts_ks = NULL;

  dts_ks = (RWPB_T_PATHSPEC(RwDts_data_Dts_Member)*)rw_keyspec_path_create_dup_of_type(key, &xact->ksi, RWPB_G_PATHSPEC_PBCMD(RwDts_data_Dts_Member));
  RW_ASSERT(dts_ks);
  if (dts_ks == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact, key, NULL, apih, NULL,
           RWDTS_ACTION_NOT_OK, "keyspec duplication failed");
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT_MESSAGE((action != RWDTS_QUERY_INVALID), "Invalid action:%d", action);

  // Check if the name is present if so whether they macth
  if (dts_ks->dompath.path001.has_key00 && !strstr(apih->client_path, dts_ks->dompath.path001.key00.name)) {
    rw_keyspec_path_free((rw_keyspec_path_t*)dts_ks, NULL);
    return RWDTS_ACTION_NA;
  }
  rw_keyspec_path_free((rw_keyspec_path_t*)dts_ks, NULL);
  dts_ks = NULL;

  inp = (RwDts__YangData__RwDts__Dts__Member*)msg;


  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);
  if (action != RWDTS_QUERY_READ) {
    char action_str[32];
    rwdts_query_action_to_str(action, action_str,sizeof(action_str));
    RWDTS_MEMBER_SEND_ERROR(xact, key, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "only READ supported action=%s", action_str);
    rw_keyspec_path_free(&dts_ks->rw_keyspec_path_t, NULL);
    return RWDTS_ACTION_NOT_OK;
  }


  // Repond to the request
  //
  RWPB_T_MSG(RwDts_data_Dts_Member) data_member, *data_member_p;
  data_member_p = &data_member;
  RWPB_F_MSG_INIT(RwDts_data_Dts_Member,data_member_p);

  RWPB_M_MSG_DECL_INIT(RwDts_data_Dts_Member_State,mbr_state);

  RWPB_M_MSG_DECL_INIT(RwDts_data_Dts_Member_XactTrace, xact_trace);

  RWPB_T_MSG(RwDts_data_Dts_Member_State) *mbr_state_p = &mbr_state;

  RWPB_T_MSG(RwDts_data_Dts_Member_XactTrace) *xact_trace_p = &xact_trace;

  data_member_p->state = mbr_state_p;
  strncpy(data_member.name, apih->client_path, sizeof(data_member.name) - 1);
  data_member.dts_state = apih->dts_state;
  data_member.has_dts_state = 1;

  RWPB_M_MSG_DECL_INIT(RwDts_data_Dts_Member_Stats,mbr_stats);
  RWPB_T_MSG(RwDts_data_Dts_Member_Stats) *mbr_stats_p = &mbr_stats;

  if (inp && (inp->n_stats > 0)) {
    strcpy(mbr_stats.member_name, apih->client_path);
    mbr_stats.num_trans = apih->stats.num_transactions;
    mbr_stats.has_num_trans = 1;
    mbr_stats.num_prepare = apih->stats.num_prepare_frm_rtr;
    mbr_stats.has_num_prepare = 1;
    mbr_stats.num_pre_commit = apih->stats.num_pre_commit_frm_rtr;
    mbr_stats.has_num_pre_commit = 1;
    mbr_stats.num_commit = apih->stats.num_commit_frm_rtr;
    mbr_stats.has_num_commit = 1;
    mbr_stats.num_abort = apih->stats.num_abort_frm_rtr;
    mbr_stats.has_num_abort = 1;
    mbr_stats.num_sub_abort = apih->stats.num_sub_abort_frm_rtr;
    mbr_stats.has_num_sub_abort = 1;
    mbr_stats.num_xact_create = apih->stats.num_xact_create_objects;
    mbr_stats.has_num_xact_create = 1;
    mbr_stats.num_xact_update = apih->stats.num_xact_update_objects;
    mbr_stats.has_num_xact_update = 1;
    mbr_stats.num_xact_delete = apih->stats.num_xact_delete_objects;
    mbr_stats.has_num_xact_delete = 1;
    mbr_stats.num_commit_create = apih->stats.num_committed_create_obj;
    mbr_stats.has_num_commit_create = 1;
    mbr_stats.num_commit_update = apih->stats.num_committed_update_obj;
    mbr_stats.has_num_commit_update = 1;
    mbr_stats.num_commit_delete = apih->stats.num_committed_delete_obj;
    mbr_stats.has_num_commit_delete = 1;
    mbr_stats.num_reg = apih->stats.num_registrations;
    mbr_stats.has_num_reg = 1;
    mbr_stats.has_num_reg_updates = 1;
    mbr_stats.num_reg_updates = apih->stats.num_reg_updates;
    mbr_stats.num_dereg = apih->stats.num_deregistrations;
    mbr_stats.has_num_dereg = 1;
    mbr_stats.num_advice = apih->stats.num_advises;
    mbr_stats.has_num_advice = 1;
    mbr_stats.num_regs_retrans = apih->stats.num_regs_retran;
    mbr_stats.has_num_regs_retrans = 1;
    mbr_stats.num_advice_rsp = apih->stats.num_member_advise_rsp;
    mbr_stats.has_num_advice_rsp = 1;
    mbr_stats.num_advice_aborted = apih->stats.num_member_advise_aborted;
    mbr_stats.has_num_advice_aborted = 1;
    mbr_stats.num_advice_failed = apih->stats.num_member_advise_failed;
    mbr_stats.has_num_advice_failed = 1;
    mbr_stats.num_advice_success = apih->stats.num_member_advise_success;
    mbr_stats.has_num_advice_success = 1;
    mbr_stats.num_prep_evt_init_state = apih->stats.num_prepare_evt_init_state;
    mbr_stats.has_num_prep_evt_init_state = 1;
    mbr_stats.num_end_evt_init_state = apih->stats.num_end_evt_init_state;
    mbr_stats.has_num_end_evt_init_state = 1;
    mbr_stats.num_prep_evt_prep_state = apih->stats.num_prepare_evt_prepare_state;
    mbr_stats.has_num_prep_evt_prep_state = 1;
    mbr_stats.num_pcomm_evt_prep_state = apih->stats.num_pcommit_evt_prepare_state;
    mbr_stats.has_num_pcomm_evt_prep_state = 1;
    mbr_stats.num_abort_evt_prep_state = apih->stats.num_abort_evt_prepare_state;
    mbr_stats.has_num_abort_evt_prep_state = 1;
    mbr_stats.num_qrsp_evt_prep_state = apih->stats.num_qrsp_evt_prepare_state;
    mbr_stats.has_num_qrsp_evt_prep_state = 1;
    mbr_stats.num_end_evt_prep_state = apih->stats.num_end_evt_prepare_state;
    mbr_stats.has_num_end_evt_prep_state = 1;
    mbr_stats.num_prep_evt_pcomm_state = apih->stats.num_prepare_evt_pcommit_state;
    mbr_stats.has_num_prep_evt_pcomm_state = 1;
    mbr_stats.num_pcom_evt_pcom_state = apih->stats.num_pcommit_evt_pcommit_state;
    mbr_stats.has_num_pcom_evt_pcom_state = 1;
    mbr_stats.num_com_evt_pcom_state = apih->stats.num_commit_evt_pcommit_state;
    mbr_stats.has_num_com_evt_pcom_state = 1;
    mbr_stats.num_abor_evt_pcom_state = apih->stats.num_abort_evt_pcommit_state;
    mbr_stats.has_num_abor_evt_pcom_state = 1;
    mbr_stats.num_end_evt_comm_state = apih->stats.num_end_evt_commit_state;
    mbr_stats.has_num_end_evt_comm_state = 1;
    mbr_stats.num_end_evt_abort_state = apih->stats.num_end_evt_abort_state;
    mbr_stats.has_num_end_evt_abort_state = 1;
    mbr_stats.num_prepare_cb_exec = apih->stats.num_prepare_cb_exec;
    mbr_stats.has_num_prepare_cb_exec = 1;
    mbr_stats.num_commit_cb_exec = apih->stats.num_commit_cb_exec;
    mbr_stats.has_num_commit_cb_exec = 1;
    mbr_stats.num_pcommit_cb_exec = apih->stats.num_pcommit_cb_exec;
    mbr_stats.has_num_pcommit_cb_exec = 1;
    mbr_stats.num_end_exec = apih->stats.num_end_exec;
    mbr_stats.has_num_end_exec = 1;
    mbr_stats.total_nontrans_queries = apih->stats.total_nontrans_queries;
    mbr_stats.has_total_nontrans_queries = 1;
    mbr_stats.total_trans_queries = apih->stats.total_trans_queries;
    mbr_stats.has_total_trans_queries = 1;
    mbr_stats.num_async_response = apih->stats.num_async_response;
    mbr_stats.has_num_async_response = 1;
    mbr_stats.num_na_response = apih->stats.num_na_response;
    mbr_stats.has_num_na_response = 1;
    mbr_stats.num_ack_response = apih->stats.num_ack_response;
    mbr_stats.has_num_ack_response = 1;
    mbr_stats.num_nack_response = apih->stats.num_nack_response;
    mbr_stats.has_num_nack_response = 1;
    mbr_stats.num_query_response = apih->stats.num_query_response;
    mbr_stats.has_num_query_response = 1;
    mbr_stats.num_xact_rsp_dispatched = apih->stats.num_xact_rsp_dispatched;
    mbr_stats.has_num_xact_rsp_dispatched = 1;
    mbr_stats.has_member_reg_advise_sent = 1;
    mbr_stats.member_reg_advise_sent = apih->stats.member_reg_advise_sent;
    mbr_stats.has_member_reg_advise_done = 1;
    mbr_stats.member_reg_advise_done = apih->stats.member_reg_advise_done;
    mbr_stats.has_member_reg_update_advise_done = 1;
    mbr_stats.member_reg_update_advise_done = apih->stats.member_reg_update_advise_done;
    mbr_stats.has_member_reg_advise_bounced = 1;
    mbr_stats.member_reg_advise_bounced = apih->stats.member_reg_advise_bounced;
    mbr_stats.has_member_reg_update_advise_bounced = 1;
    mbr_stats.member_reg_update_advise_bounced = apih->stats.member_reg_update_advise_bounced;
    mbr_stats.has_more_received = 1;
    mbr_stats.more_received = apih->stats.more_received;
    mbr_stats.has_sent_keep_alive = 1;
    mbr_stats.sent_keep_alive = apih->stats.sent_keep_alive;
    mbr_stats.has_sent_credits = 1;
    mbr_stats.sent_credits = apih->stats.sent_credits;
    mbr_stats.has_reroot_done = 1;
    mbr_stats.reroot_done = apih->stats.reroot_done;
    mbr_stats.has_num_notif_rsp_count = 1;
    mbr_stats.num_notif_rsp_count = apih->stats.num_notif_rsp_count;
    data_member_p->stats = realloc(data_member_p->stats, sizeof(mbr_stats));
    data_member_p->stats[0] = mbr_stats_p;
    data_member_p->n_stats = 1;
  }

  //if( payload_stats )
  if (inp && (inp->payload_stats != NULL))
  {
    data_member_p->payload_stats = (RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *)protobuf_c_message_duplicate(NULL,
                                                                (const struct ProtobufCMessage *) &apih->payload_stats,
                                                                apih->payload_stats.base.descriptor);
  }


  // ATTN -- hack until CLI sends us whole keypath
  if (!inp || (inp && !inp->state) || (inp && inp->state && inp->state->registration) ||
      (inp && inp->state && !inp->state->n_registration && !inp->state->n_transaction)) {

    rwdts_member_registration_t *entry = NULL;

    int i = 0;

    entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
    while (entry) {
      if (inp && inp->state && inp->state->registration)
      {
        if ((inp->state->registration[0]->id) &&
          (entry->reg_id != inp->state->registration[0]->id))
        {
          entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
          continue;
        }
        if ((inp->state->registration[0]->keyspec[0] != '\0') &&
            strstr(entry->keystr, inp->state->registration[0]->keyspec) == NULL)
        {
          entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
          continue;
        }
        if (inp->state->registration[0]->flags)
        {
          if ((inp->state->registration[0]->flags == RW_DTS_DTS_REG_FLAGS_PUBLISHER)
              && !((entry->flags & RWDTS_FLAG_PUBLISHER)))
          {
            entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
            continue;
          }

          if ((inp->state->registration[0]->flags == RW_DTS_DTS_REG_FLAGS_SUBSCRIBER)
              && !(entry->flags & RWDTS_FLAG_SUBSCRIBER))
          {
            entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
            continue;
          }
        }
      }
      if (entry->reg_state == RWDTS_REG_DEL_PENDING) {
        entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
        continue;
      }

      mbr_state.n_registration++;
      mbr_state.registration = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration)**)
          realloc(mbr_state.registration, mbr_state.n_registration * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration)*));

      mbr_state.registration[i] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration)*)
                                  RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration)));
      RW_ASSERT(mbr_state.registration);
      RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Registration,mbr_state.registration[i]);

      /* Reg id */
      mbr_state.registration[i]->id = entry->reg_id;

      /* handle  */
      mbr_state.registration[i]->has_handle = TRUE;
      snprintf(mbr_state.registration[i]->handle, sizeof(mbr_state.registration[i]->handle), "%p", entry);

      /* keyspec */
      //mbr_state.registration[i]->has_keyspec = TRUE;
      RW_KEYSPEC_PATH_GET_PRINT_BUFFER(entry->keyspec, NULL, NULL, mbr_state.registration[i]->keyspec, sizeof(mbr_state.registration[i]->keyspec)-1, NULL);

      /* flags */
      mbr_state.registration[i]->has_regflags = TRUE;
      if ((entry->flags & RWDTS_FLAG_PUBLISHER) &&
          (entry->flags & RWDTS_FLAG_SHARED)) {
        strcat(mbr_state.registration[i]->regflags, "P-SH");
        mbr_state.registration[i]->flags = RW_DTS_DTS_REG_FLAGS_PUBLISHER;
      } else {
        if (entry->flags & RWDTS_FLAG_PUBLISHER) {
          strcat(mbr_state.registration[i]->regflags, "P");
          mbr_state.registration[i]->flags = RW_DTS_DTS_REG_FLAGS_PUBLISHER;
        }
      }
      if (entry->flags & RWDTS_FLAG_SUBSCRIBER) {
       strcat(mbr_state.registration[i]->regflags, "S");
       mbr_state.registration[i]->flags = RW_DTS_DTS_REG_FLAGS_SUBSCRIBER;
      }

      /* descriptor */
      snprintf(mbr_state.registration[i]->descriptor, sizeof(mbr_state.registration[i]->descriptor), "%p", entry->desc);
      mbr_state.registration[i]->has_descriptor = TRUE;

      /* number of objects */
      mbr_state.registration[i]->num_objects = HASH_CNT(hh_data, entry->obj_list);


      if(inp &&
         inp->state &&
         inp->state->n_registration &&
         inp->state->registration[0]->n_stats)
      {
        mbr_state.registration[i]->stats = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_Stats)**)
            RW_MALLOC0(1 * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_Stats)*));
        mbr_state.registration[i]->stats[0] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_Stats)*)
                                            RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_Stats)));
        mbr_state.registration[i]->n_stats = 1;
        RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_Stats) *reg_stats = mbr_state.registration[i]->stats[0];

        RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Registration_Stats,(reg_stats));

        (*reg_stats).reg_id = entry->reg_id;
        COPY_STATS_2(*reg_stats,entry->stats, num_prepare_rcvd);
        COPY_STATS_2(*reg_stats,entry->stats, num_precommit_rcvd);
        COPY_STATS_2(*reg_stats,entry->stats, num_commit_rcvd);
        COPY_STATS_2(*reg_stats,entry->stats, num_abort_rcvd);
        COPY_STATS_2(*reg_stats,entry->stats, num_prepare_cb_acked);
        COPY_STATS_2(*reg_stats,entry->stats, num_prepare_cb_nacked);
        COPY_STATS_2(*reg_stats,entry->stats, num_prepare_cb_async);
        COPY_STATS_2(*reg_stats,entry->stats, num_get_reqt_sent);
        COPY_STATS_2(*reg_stats,entry->stats, num_get_rsp_rcvd);
        COPY_STATS_2(*reg_stats,entry->stats, num_prepare_cb_invoked);
        COPY_STATS_2(*reg_stats,entry->stats, num_precommit_cb_invoked);
        COPY_STATS_2(*reg_stats,entry->stats, num_commit_cb_invoked);
        COPY_STATS_2(*reg_stats,entry->stats, num_abort_cb_invoked);
        COPY_STATS_2(*reg_stats,entry->stats, num_advises);
        COPY_STATS_2(*reg_stats,entry->stats, num_member_advise_rsp);
        COPY_STATS_2(*reg_stats,entry->stats, num_xact_create_objects);
        COPY_STATS_2(*reg_stats,entry->stats, num_xact_delete_objects);
        COPY_STATS_2(*reg_stats,entry->stats, num_xact_update_objects);
        COPY_STATS_2(*reg_stats,entry->stats, num_registrations);
        COPY_STATS_2(*reg_stats,entry->stats, num_deregistrations);
        COPY_STATS_2(*reg_stats,entry->stats, out_of_order_queries);
        COPY_STATS_2(*reg_stats,entry->stats, error_drop_advice);
        COPY_STATS_2(*reg_stats,entry->stats, error_drop_read);
        COPY_STATS_2(*reg_stats,entry->stats, not_ended);
        COPY_STATS_2(*reg_stats,entry->stats, dispatch_async_response);
        COPY_STATS_2(*reg_stats,entry->stats, dispatch_response_immediate);
        COPY_STATS_2(*reg_stats,entry->stats, send_error);
        COPY_STATS_2(*reg_stats,entry->stats, num_async_response);
        COPY_STATS_2(*reg_stats,entry->stats, num_nack_response);
        COPY_STATS_2(*reg_stats,entry->stats, num_ack_response);
        COPY_STATS_2(*reg_stats,entry->stats, num_response);
        COPY_STATS_2(*reg_stats,entry->stats, num_na_response);
      }
      mbr_state.registration[i]->n_cache_entry =  HASH_CNT(hh_data,entry->obj_list);
      if (mbr_state.registration[i]->n_cache_entry)
      {
        int num_entries = 0;
        mbr_state.registration[i]->cache_entry = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_CacheEntry)**)
            RW_MALLOC0(mbr_state.registration[i]->n_cache_entry * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_CacheEntry)*));
        num_entries = rwdts_fill_cache_entries(entry, mbr_state.registration[i]->cache_entry);
        RW_ASSERT(num_entries == mbr_state.registration[i]->n_cache_entry);
      }

      /* Audit related status */
      RWPB_T_MSG(RwDts_data_Dts_Member_State_Registration_AuditStatus)* audit_status = NULL;
      mbr_state.registration[i]->has_audit_status = TRUE;
      audit_status = &mbr_state.registration[i]->audit_status;

      audit_status->has_state          = TRUE;
      audit_status->state              = entry->audit.audit_state;
      audit_status->has_status         = TRUE;
      audit_status->status             = entry->audit.audit_status;
      audit_status->has_status_message = TRUE;
      strncpy(audit_status->status_message,
              entry->audit.audit_status_message,
              sizeof(audit_status->status_message));

      audit_status->has_statistics = TRUE;
      audit_status->statistics.has_num_audits_started = TRUE;
      audit_status->statistics.num_audits_started = entry->audit.audit_stats.num_audits_started;
      audit_status->statistics.has_num_audits_succeeded = TRUE;
      audit_status->statistics.num_audits_succeeded = entry->audit.audit_stats.num_audits_succeeded;
      audit_status->statistics.has_num_audits_failed = TRUE;
      audit_status->statistics.num_audits_failed = entry->audit.audit_stats.num_audits_failed;
      audit_status->statistics.has_num_audit_fetches = TRUE;
      audit_status->statistics.num_audit_fetches = entry->audit.fetch_attempts;
      audit_status->statistics.has_num_objs_missing_in_dts = TRUE;
      audit_status->statistics.has_num_objs_missing_in_cache = TRUE;
      audit_status->statistics.has_num_objs_mismatched_contents = TRUE;
      audit_status->statistics.has_num_objs_from_dts = TRUE;
      audit_status->statistics.has_num_objs_in_cache = TRUE;
      audit_status->statistics.num_objs_missing_in_dts = entry->audit.objs_missing_in_dts;
      audit_status->statistics.num_objs_missing_in_cache = entry->audit.objs_missing_in_cache;
      audit_status->statistics.num_objs_from_dts = entry->audit.objs_from_dts;
      audit_status->statistics.num_objs_in_cache = entry->audit.objs_in_cache;
      audit_status->statistics.num_objs_mismatched_contents = entry->audit.objs_mis_matched_contents;

      audit_status->n_audit_errors = entry->audit.audit_issues.errs_ct;
      if(audit_status->n_audit_errors > sizeof(audit_status->audit_errors)/sizeof(audit_status->audit_errors[0])) {
         audit_status->n_audit_errors = sizeof(audit_status->audit_errors)/sizeof(audit_status->audit_errors[0]);
      }
      int k = 0;
      for(k=0;k<audit_status->n_audit_errors;k++) {
        audit_status->audit_errors[k].has_status = TRUE;
        audit_status->audit_errors[k].status = entry->audit.audit_issues.errs[k].rs;
        audit_status->audit_errors[k].has_error_str = TRUE;
        strncpy(audit_status->audit_errors[k].error_str,entry->audit.audit_issues.errs[k].str,sizeof(audit_status->audit_errors[k].error_str));
      }

      audit_status->n_audit_traces = entry->audit.n_audit_trace;
      if(audit_status->n_audit_traces > sizeof(audit_status->audit_traces)/sizeof(audit_status->audit_traces[0])) {
         audit_status->n_audit_traces = sizeof(audit_status->audit_traces)/sizeof(audit_status->audit_traces[0]);
      }
      for(k=0;k<audit_status->n_audit_traces;k++) {
        audit_status->audit_traces[k].has_state = TRUE;
        audit_status->audit_traces[k].has_event = TRUE;
        audit_status->audit_traces[k].state = entry->audit.audit_trace[k].state;
        audit_status->audit_traces[k].event = entry->audit.audit_trace[k].event;
      }


      i++;
      entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
    }
  }
  if (!inp || (inp && !inp->state) || (inp && inp->state && inp->state->transaction) ||
      (inp && inp->state && !inp->state->n_registration && !inp->state->n_transaction)) {
    int i = 0;
    char xact_id_str[64];

    mbr_state.n_transaction = HASH_CNT(hh, apih->xacts);
    mbr_state.transaction = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction)**)
                            RW_MALLOC0(mbr_state.n_transaction * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction)*));
    RW_ASSERT(mbr_state.transaction);

    rwdts_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
    HASH_ITER(hh, apih->xacts, xact_entry, tmp_xact_entry) {
      mbr_state.transaction[i] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction)*)
                                 RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction)));
      RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Transaction,mbr_state.transaction[i]);
      strncpy(mbr_state.transaction[i]->id,
              rwdts_xact_id_str(&xact_entry->id, xact_id_str, sizeof(xact_id_str)),
              sizeof(mbr_state.transaction[i]->id) - 1);
      mbr_state.transaction[i]->state = strdup(rwdts_get_xact_state_string(xact_entry->mbr_state));
      if (xact_entry->queries) {
        mbr_state.transaction[i]->n_queries = HASH_CNT(hh, xact_entry->queries);
        mbr_state.transaction[i]->queries = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries)**)
            RW_MALLOC0(mbr_state.transaction[i]->n_queries * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries)*));
        rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
        rwdts_match_info_t *match = NULL;
        int j = 0;
        HASH_ITER(hh, xact_entry->queries, xquery, qtmp) {
          mbr_state.transaction[i]->queries[j] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries)*)
              RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries)));
          RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Transaction_Queries, mbr_state.transaction[i]->queries[j]);
          if (xquery->query) {
            mbr_state.transaction[i]->queries[j]->query_id = xquery->query->queryidx;
          }
          mbr_state.transaction[i]->queries[j]->n_match_reg = RW_SKLIST_LENGTH(&(xquery->reg_matches));
          mbr_state.transaction[i]->queries[j]->num_responses = (xquery->qres)?xquery->qres->n_result:0;
          mbr_state.transaction[i]->queries[j]->has_num_responses = 1;
          if (mbr_state.transaction[i]->queries[j]->n_match_reg) {
            int k = 0;
            match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
            mbr_state.transaction[i]->queries[j]->match_reg = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries_MatchReg)**)
                RW_MALLOC0(mbr_state.transaction[i]->queries[j]->n_match_reg * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries_MatchReg)*));
            while (match) {
              mbr_state.transaction[i]->queries[j]->match_reg[k] = (RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries_MatchReg)*)
                  RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_State_Transaction_Queries_MatchReg)));
              RWPB_F_MSG_INIT(RwDts_data_Dts_Member_State_Transaction_Queries_MatchReg, mbr_state.transaction[i]->queries[j]->match_reg[k]);
              mbr_state.transaction[i]->queries[j]->match_reg[k]->reg_id = match->reg->reg_id;
              k++;
              match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
            }
          }
          j++;
        }
      }
      i++;
    }
  }

  if (inp && inp->n_xact_trace && inp->xact_trace[0]->xact_id) {
    rwdts_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;

    int i = 0;
    char xact_id_str[64];
    HASH_ITER(hh, apih->xacts, xact_entry, tmp_xact_entry) {
      if (!strcmp(inp->xact_trace[0]->xact_id, rwdts_xact_id_str(&xact_entry->id, xact_id_str, sizeof(xact_id_str)))) {
        xact_trace.n_trace_result = xact_entry->curr_trace_index;
        xact_trace.trace_result = (RWPB_T_MSG(RwDts_data_Dts_Member_XactTrace_TraceResult)**)
                                  RW_MALLOC0(xact_trace.n_trace_result * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_XactTrace_TraceResult)*));
        RW_ASSERT(xact_trace.trace_result);
        for (i=0; i < xact_entry->curr_trace_index; i++) {
           xact_trace.trace_result[i] = (RWPB_T_MSG(RwDts_data_Dts_Member_XactTrace_TraceResult)*)
                                        RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_XactTrace_TraceResult)));
           RWPB_F_MSG_INIT(RwDts_data_Dts_Member_XactTrace_TraceResult, xact_trace.trace_result[i]);
           xact_trace.trace_result[i]->id = i;
           xact_trace.trace_result[i]->state_event = RW_STRDUP(rwdts_member_state_to_str(xact_entry->fsm_log[i].state));
        }
        data_member_p->xact_trace = realloc(data_member_p->xact_trace, sizeof(xact_trace));
        data_member_p->xact_trace[0] = xact_trace_p;
        data_member_p->n_xact_trace = 1;
        break;
      }
    }
  }

  RWPB_T_PATHSPEC(RwDts_data_Dts_Member) *dts_mbr = NULL;

  /* register for DTS state */
  rw_status_t rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_Dts_Member),
                                              &xact->ksi,
                                              (rw_keyspec_path_t**)&dts_mbr);
  if(RW_STATUS_SUCCESS != rs)
  {
    RWDTS_XACT_ABORT(xact, rs, RWDTS_ERRSTR_KEY_DUP);
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)mbr_state_p);
    rw_keyspec_path_free((rw_keyspec_path_t*)dts_mbr, NULL);
    return RWDTS_ACTION_NOT_OK;
  }
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)dts_mbr, &xact->ksi, RW_SCHEMA_CATEGORY_DATA);
  dts_mbr->dompath.path001.has_key00 = 1;
  RW_ASSERT(strlen(apih->client_path) < sizeof(dts_mbr->dompath.path001.key00.name));
  strncpy(dts_mbr->dompath.path001.key00.name, apih->client_path, sizeof(dts_mbr->dompath.path001.key00.name)-1);
  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t*)dts_mbr,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&data_member_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rs = 
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  if (rs != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact, key, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "send respnse failed");
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)mbr_state_p);
    rw_keyspec_path_free((rw_keyspec_path_t*)dts_mbr, NULL);
    return RWDTS_ACTION_NOT_OK;
  }

  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)mbr_state_p);
  rw_keyspec_path_free((rw_keyspec_path_t*)dts_mbr, NULL);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_handle_clear_stats_req(const rwdts_xact_info_t* xact_info,
                                    RWDtsQueryAction         action,
                                    const rw_keyspec_path_t* key,
                                    const ProtobufCMessage*  msg,
                                    uint32_t                 credits,
                                    void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "rwdts_member_handle_clear_stats_req:xact_info NULL");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_input_ClearDts_Stats_Member) *inp;

  inp = (RWPB_T_MSG(RwDts_input_ClearDts_Stats_Member) *) msg;
  RW_ASSERT(inp);
  if (inp == NULL) {
    RWDTS_MEMBER_SEND_ERROR(xact_info->xact, key, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "invalid input message");
    return RWDTS_ACTION_NOT_OK;
  }

  if (inp->name) {
    if (!strstr(apih->client_path, inp->name)) {
      return RWDTS_ACTION_NA;
    }
  } else {
    if (!inp->has_all) {
      return RWDTS_ACTION_NA;
    }
  }

  memset(&apih->stats, 0, sizeof(apih->stats));

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_member_start_tracing(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t* key,
                         const ProtobufCMessage*  msg,
                         uint32_t                 credits,
                         void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "rwdts_member_start_tracing:xact_info NULL");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_input_StartDts_Tracing) *trace;

  RW_DEBUG_ASSERT(action != RWDTS_QUERY_INVALID);

  trace =  (RWPB_T_MSG(RwDts_input_StartDts_Tracing) *)msg;
  if (!trace->member) {
    return RWDTS_ACTION_NA;
  }

  // Check if the name is present if so whether they macth
  if (!strstr(apih->client_path, trace->member)) {
    return RWDTS_ACTION_NA;
  }

  apih->trace.on = 1;

  RWPB_M_MSG_DECL_INIT(RwDts_output_StartDts_Tracing, me);
  me.n_member_name = 1;
  me.member_name = &apih->client_path;

  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwDts_output_StartDts_Tracing);

  rw_status_t rs_status = rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_ACK, ks, (ProtobufCMessage*)&me);
  if (rs_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact_info->xact, key, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "failed to send response");
    return RWDTS_ACTION_NOT_OK;
  }

  return RWDTS_ACTION_ASYNC;
}

static rwdts_member_rsp_code_t
rwdts_member_stop_tracing(const rwdts_xact_info_t* xact_info,
                          RWDtsQueryAction         action,
                          const rw_keyspec_path_t* key,
                          const ProtobufCMessage*  msg,
                          uint32_t                 credits,
                          void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "rwdts_member_stop_tracing:xact_info NULL");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWPB_T_MSG(RwDts_input_StopDts_Tracing) *trace =
      (RWPB_T_MSG(RwDts_input_StopDts_Tracing) *)msg;

  RW_DEBUG_ASSERT(action != RWDTS_QUERY_INVALID);

  if (trace->member  && (!strstr(apih->client_path, trace->member))) {
    return RWDTS_ACTION_NA;
  }

  apih->trace.on = 0;
  apih->trace.persistant = 0;

  rw_status_t rs_status = rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_ACK, NULL, NULL);
  RW_ASSERT_MESSAGE((rs_status == RW_STATUS_SUCCESS), "rwdts_xact_info_respond_keyspec failed");
  if (rs_status != RW_STATUS_SUCCESS) {
    RWDTS_MEMBER_SEND_ERROR(xact_info->xact, key, NULL, apih, NULL, 
             RWDTS_ACTION_NOT_OK, "failed to send response");
    return RWDTS_ACTION_NOT_OK;
  }

  return RWDTS_ACTION_ASYNC;
}

/* Apply conf1 */
static void rwdts_conf1_apply(rwdts_api_t *apih,
                              rwdts_appconf_t *ac,
                              rwdts_xact_t *xact,
                              rwdts_appconf_action_t action,
                              void *ctx,
                              void *scratch_in) {
  /* We don't even bother reconcile, we just reload from the newly committed state */
  int i;
  rw_keyspec_path_t *keyspec = NULL;
  const rw_yang_pb_schema_t* schema = NULL;

  /* Jettison existing filters */
  if (apih->conf1.tracert.filters) {
    for (i=0; i<apih->conf1.tracert.filters_ct; i++) {
      rw_keyspec_path_free(apih->conf1.tracert.filters[i]->path, NULL );
      apih->conf1.tracert.filters[i]->path = NULL;
      RW_FREE(apih->conf1.tracert.filters[i]);
      apih->conf1.tracert.filters[i] = NULL;
    }
    free(apih->conf1.tracert.filters);
    apih->conf1.tracert.filters = NULL;
    apih->conf1.tracert.filters_ct = 0;
  }

  /* Check for Xpath  validity before storing the config  */
  schema = rwdts_api_get_ypbc_schema(apih);
  if (!schema) {
    RWDTS_API_LOG_EVENT(apih, NoSchemaNoTrace, "WARNING DTS CLIENT HAS NO SCHEMA AND WILL NOT TRACE!!!!");
    // Do not raise an error - since the keyspec could be owned by a different
    // member
    return;
  }

  /* Replace whole config */
  RW_ASSERT_MESSAGE(!apih->conf1.tracert.filters_ct, "rwdts_conf1_apply:conf1.tracert.filters_ct is zero");
  rwdts_member_cursor_t *cur = rwdts_member_data_get_current_cursor(xact, apih->conf1.tracert.reg);
  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_data_reset_cursor(xact, apih->conf1.tracert.reg);
  RWPB_T_MSG(RwDts_Tracert) *tr;
  while ((tr = (RWPB_T_MSG(RwDts_Tracert)*) rwdts_member_reg_handle_get_next_element(apih->conf1.tracert.reg,
                                                                                     cur, xact, 
                                                                                     &elem_ks))) {

    if (tr->keyspec) {
      RWPB_T_MSG (RwDts_Tracert_Keyspec)* ks = tr->keyspec;
      if (!ks->has_path) {
        // Set the error?
        goto bail;
      }
      keyspec = RW_KEYSPEC_PATH_FROM_XPATH(schema, (char*)ks->path, RW_XPATH_KEYSPEC, NULL, NULL);
      if (!keyspec) {
        /* This keyspec does not belong to our schema */
        RWDTS_API_LOG_EVENT(apih, KeyspecNotInSchema, RWLOG_ATTR_SPRINTF("Cannot find path in its schema, path='%s'", ks->path));
        continue;
      }

      int newct = 1 + apih->conf1.tracert.filters_ct;
      apih->conf1.tracert.filters = (rwdts_trace_filter_t**)realloc(apih->conf1.tracert.filters, (newct * sizeof(rwdts_trace_filter_t*)));
      apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct] = RW_MALLOC0(sizeof(rwdts_trace_filter_t));
      RW_ASSERT(apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]);
      apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->path = keyspec;
      apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->id = tr->id;
      if (ks->has_print) {
        apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->print = ks->print;
      }
      if (ks->break_at) {
        RWPB_T_MSG (RwDts_Tracert_Keyspec_BreakAt) *break_at = ks->break_at;
        if (break_at->has_start) {
          apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->break_start = 1;
        }
        if (break_at->has_prepare) {
          apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->break_prepare = 1;
        }
        if (break_at->has_end) {
          apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->break_end = 1;
        }
      }
      keyspec = NULL;
      apih->conf1.tracert.filters_ct++;
      RWDTS_API_LOG_EVENT(apih, TraceSet, ks->path);
    }
    else if (tr->has_client) {
      // There is no reason to store this. If the whole configuration is
      // replayed every time, even after deletes, this is sufficient.

      if (strstr(apih->client_path, tr->client)) {
        apih->trace.on = 1;
        apih->trace.persistant = 1;
      }
    } else {
      // Set the error?
      goto bail;
    }
  }
  return;


bail:
  for (i=0; i<apih->conf1.tracert.filters_ct; i++) {
    rw_keyspec_path_free(apih->conf1.tracert.filters[i]->path, NULL);
    apih->conf1.tracert.filters[i]->path = NULL;
    RW_FREE(apih->conf1.tracert.filters[i]);
    apih->conf1.tracert.filters[i] = NULL;
  }
  free(apih->conf1.tracert.filters);
  apih->conf1.tracert.filters = NULL;
  apih->conf1.tracert.filters_ct = 0;
}


/*
 * Register keyspecs that will help in debugging
 * protobufc and keyspec related stuffs.
 */
static void rwdts_register_debug_keyspecs(rwdts_api_t *apih)
{
  static bool registered_for_eb = false;
  unsigned no_regs = 0;
  rwdts_registration_t regns[4] = {{ NULL }};

  /* Register for keyspec statistics */
  rw_keyspec_path_t *ks_stats = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwKeyspecStats_data_KeyspecStats_Member)->rw_keyspec_path_t,
                             &apih->ksi,
                             &ks_stats);
  RW_ASSERT_MESSAGE(ks_stats, "keyspec duplication failed");
  rw_keyspec_path_set_category (ks_stats, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

  regns[no_regs].keyspec = ks_stats;
  regns[no_regs].desc    = RWPB_G_MSG_PBCMD(RwKeyspecStats_data_KeyspecStats_Member);
  regns[no_regs].flags   = RWDTS_FLAG_PUBLISHER;
  regns[no_regs++].callback.cb.prepare = rwdts_member_handle_mgmt_get_ks_stats;

  /* Register for protobuf statistics */
  rw_keyspec_path_t *pb_stats = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwPbcStats_data_PbcStats_Member)->rw_keyspec_path_t,
                             &apih->ksi,
                             &pb_stats);
  RW_ASSERT(pb_stats);
  rw_keyspec_path_set_category (pb_stats, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

  regns[no_regs].keyspec = pb_stats;
  regns[no_regs].desc    = RWPB_G_MSG_PBCMD(RwPbcStats_data_PbcStats_Member);
  regns[no_regs].flags   = RWDTS_FLAG_PUBLISHER;
  regns[no_regs++].callback.cb.prepare = rwdts_member_handle_mgmt_get_pbc_stats;

  if (!registered_for_eb) {
    /*
     * Only one dts member within a process needs to register for exporting
     * library error buffers. Not sure how to achive this. For now using a
     * static variable. Hope this is thread safe. Even if it is not, nothing harm
     * will happen.
     */
    rw_keyspec_path_t *ks_eb = NULL;
    rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwKeyspecStats_data_KeyspecEbuf_Tasklet)->rw_keyspec_path_t,
                               &apih->ksi,
                               &ks_eb);
    RW_ASSERT(ks_eb);
    rw_keyspec_path_set_category (ks_eb, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

    regns[no_regs].keyspec = ks_eb;
    regns[no_regs].desc    = RWPB_G_MSG_PBCMD(RwKeyspecStats_data_KeyspecEbuf_Tasklet);
    regns[no_regs].flags   = RWDTS_FLAG_PUBLISHER;
    regns[no_regs++].callback.cb.prepare = rwdts_member_handle_mgmt_get_ks_ebuf;

    rw_keyspec_path_t *pb_eb = NULL;
    rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwPbcStats_data_PbcEbuf_Tasklet)->rw_keyspec_path_t,
                               &apih->ksi,
                               &pb_eb);
    RW_ASSERT(pb_eb);
    rw_keyspec_path_set_category (pb_eb, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

    regns[no_regs].keyspec = pb_eb;
    regns[no_regs].desc    = RWPB_G_MSG_PBCMD(RwPbcStats_data_PbcEbuf_Tasklet);
    regns[no_regs].flags   = RWDTS_FLAG_PUBLISHER;
    regns[no_regs++].callback.cb.prepare = rwdts_member_handle_mgmt_get_pbc_ebuf;

    registered_for_eb = true;
  }

  unsigned i = 0;
  for (i = 0; i < no_regs; i++) {
    regns[i].callback.ud = apih;
    rwdts_member_reg_handle_t reg_hdl = rwdts_member_register(NULL, apih,
                                                              regns[i].keyspec,
                                                              &regns[i].callback,
                                                              regns[i].desc,
                                                              regns[i].flags,
                                                              NULL);
    RW_ASSERT(reg_hdl);
    // Throwing away the handle, not storing it anywhere.
    rw_keyspec_path_free(regns[i].keyspec, NULL);
  }
}

#define RWDTS_STANDBY_REF_CODE_NOTIFY_STATE_CHANGE 

#ifdef RWDTS_STANDBY_REF_CODE_NOTIFY_STATE_CHANGE
static void rwdts_state_change_callback (
    void *ud, 
    vcs_vm_state p_state, 
    vcs_vm_state n_state)
{
  rwdts_api_t *apih= (rwdts_api_t *)ud;
  RW_ASSERT(apih);
  NEW_DBG_PRINTS("[[%s]] got state transition from %d ==> %d\n", apih->client_path, p_state, n_state);
}
#endif


/*
 * register DTS library related keyspecs
 */
static void rwdts_register_keyspecs(rwdts_api_t *apih)
{
  rw_status_t rs;
  RWPB_T_PATHSPEC(RwDts_data_Dts_Member) *dts_ks = NULL;
  RWPB_T_PATHSPEC(RwDts_input_ClearDts_Stats_Member) *clear_ks = NULL;
  RWPB_T_PATHSPEC(RwDts_input_SolicitAdvise) solicit_advise_ks;

  rw_keyspec_path_t *keyspec = NULL;

  if (!strstr(apih->client_path, "DTSRouter")) {
    rwdts_member_event_cb_t cb1 = { .cb = { NULL, NULL, NULL, NULL, NULL }, .ud = NULL };

    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegId_Member_Registration) keyspecid_entry  = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegId_Member_Registration));

    keyspec = (rw_keyspec_path_t*)&keyspecid_entry;


    strcpy(keyspecid_entry.dompath.path001.key00.name, apih->client_path);
    keyspecid_entry.dompath.path001.has_key00 = TRUE;

    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );
    apih->init_regidh = (rwdts_member_reg_handle_t) rwdts_member_registration_init_local(apih, keyspec, &cb1, RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_INTERNAL_REG, RWPB_G_MSG_PBCMD(RwDts_data_RtrInitRegId_Member_Registration));
    rwdts_member_reg_handle_mark_internal(apih->init_regidh);

    keyspec = NULL;

    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) keyspec_entry = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member));
    keyspec = (rw_keyspec_path_t*)&keyspec_entry;
    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );

    strcpy(keyspec_entry.dompath.path001.key00.name, apih->client_path);
    keyspec_entry.dompath.path001.has_key00 = TRUE;

    apih->init_regkeyh = (rwdts_member_reg_handle_t) rwdts_member_registration_init_local(apih, keyspec, &cb1, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_INTERNAL_REG, RWPB_G_MSG_PBCMD(RwDts_data_RtrInitRegKeyspec_Member));
    rwdts_member_reg_handle_mark_internal(apih->init_regkeyh);
  }

  //rw_keyspec_free(keyspec);

  /* register for DTS state */
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_Dts_Member),
                                  &apih->ksi,
                                  (rw_keyspec_path_t**)&dts_ks);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  dts_ks->has_dompath = TRUE;
  //  dts_ks->dompath.path001.has_key00 = 1;
  // strncpy(dts_ks->dompath.path001.key00.name,
  //        apih->client_path, sizeof(dts_ks->dompath.path001.key00.name));
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)dts_ks, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_input_ClearDts_Stats_Member),
                                  &apih->ksi,
                                  (rw_keyspec_path_t**)&clear_ks);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)clear_ks, &apih->ksi, RW_SCHEMA_CATEGORY_RPC_INPUT);

  solicit_advise_ks = (*RWPB_G_PATHSPEC_VALUE(RwDts_input_SolicitAdvise));

  rwdts_registration_t regns[] = {
    { // member state info
      .keyspec = (rw_keyspec_path_t*)dts_ks,
      .desc    = RWPB_G_MSG_PBCMD(RwDts_data_Dts_Member),
      .flags   = RWDTS_FLAG_PUBLISHER,
      .callback = {
        .cb.prepare = rwdts_member_handle_mgmt_req
      }
    },
    /* audit rpc  */
    {.keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_input_StartDts_Audit),
     .desc = RWPB_G_MSG_PBCMD(RwDts_input_StartDts_Audit),
     .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwdts_member_start_audit,
      }
    },
    /*start trace rpc  */
    {.keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_input_StartDts_Tracing),
     .desc = RWPB_G_MSG_PBCMD(RwDts_input_StartDts_Tracing),
     .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwdts_member_start_tracing,
      }
    },

    /* stop trace rpc  */
    {.keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_input_StopDts_Tracing),
     .desc = RWPB_G_MSG_PBCMD(RwDts_input_StopDts_Tracing),
     .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
        .cb.prepare = rwdts_member_stop_tracing,
      }
    },

    /* clear stats rpc */
    {.keyspec = (rw_keyspec_path_t*)clear_ks,
     .desc    = RWPB_G_MSG_PBCMD(RwDts_input_ClearDts_Stats_Member),
     .flags   = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
     .callback = {
       .cb.prepare = rwdts_member_handle_clear_stats_req,
     }
    },

    /* solicit advise */
    {.keyspec = (rw_keyspec_path_t*)&solicit_advise_ks,
     .desc    = RWPB_G_MSG_PBCMD(RwDts_input_SolicitAdvise),
     .flags   = RWDTS_FLAG_PUBLISHER,
     .callback = {
       .cb.prepare = rwdts_member_handle_solicit_advise_ks,
     }
    },
  };

  int i;

  for (i = 0; i < sizeof (regns) / sizeof (rwdts_registration_t); i++) {
    regns[i].callback.ud = apih;
    RW_ASSERT(i < RWDTS_MAX_INTERNAL_REG);
    apih->dts_regh[i] = rwdts_member_register(NULL, apih,
                                              regns[i].keyspec,
                                              &regns[i].callback,
                                              regns[i].desc,
                                              regns[i].flags|RWDTS_FLAG_INTERNAL_REG,
                                              NULL);
    rwdts_member_reg_handle_mark_internal(apih->dts_regh[i]);
  }

#ifdef RWDTS_STANDBY_REF_CODE_NOTIFY_STATE_CHANGE
  if (apih->rwtasklet_info
      && apih->rwtasklet_info->rwvcs
      && apih->rwtasklet_info->mode.has_mode_active
      && !apih->rwtasklet_info->mode.mode_active) {
    rwdts_api_vm_state_cb_t cb;
    cb.cb_fn = rwdts_state_change_callback;
    cb.ud = apih;
    NEW_DBG_PRINTS("[[%s]] registering for the vm_state updates\n", instance_name);
    rwdts_register_vm_state_notification(
        apih, 
        RWVCS_TYPES_VM_STATE_NONE,
        &cb);
  }
#endif

  // Done with the registrations -- free the keyspec
  rw_keyspec_path_free((rw_keyspec_path_t*)dts_ks, NULL);
  rw_keyspec_path_free((rw_keyspec_path_t*)clear_ks, NULL);

  // Register for keyspec and protobuf debug keyspecs.
  rwdts_register_debug_keyspecs(apih);


  /* Register for config */
  {
    // Create the config group
    rwdts_appconf_cbset_t appconf_cbs = {
      /* no init / deinit / validate, we just take it already */
      .config_apply = rwdts_conf1_apply,   // reconcile / apply config
      .ctx = apih
    };
    apih->conf1.appconf = rwdts_appconf_group_create(apih, NULL, &appconf_cbs);

    uint32_t flags = 0;

    RWPB_M_PATHSPEC_DECL_SET(RwDts_Tracert, ac_ks, RW_SCHEMA_CATEGORY_CONFIG);
    apih->conf1.tracert.reg = rwdts_appconf_register(apih->conf1.appconf, (rw_keyspec_path_t*)&ac_ks, NULL, RWPB_G_MSG_PBCMD(RwDts_Tracert), flags, NULL/*prepare*/);

    rwdts_appconf_phase_complete(apih->conf1.appconf, RWDTS_APPCONF_PHASE_REGISTER);
  }
  rwdts_journal_show_init(apih);

  return;
}

rwdts_api_t*
rwdts_api_init_internal(rwtasklet_info_ptr_t           rw_tasklet_info,
                        rwsched_dispatch_queue_t       q,
                        rwsched_tasklet_ptr_t          tasklet,
                        rwsched_instance_ptr_t         sched,
                        rwmsg_endpoint_t*              ep,
                        const char*                    my_path,
                        uint64_t                       router_idx,
                        uint32_t                       flags,
                        const rwdts_state_change_cb_t* cb)
{
  rwdts_api_t *apih;

  rw_status_t rs = RW_STATUS_FAILURE;

  apih = RW_MALLOC0_TYPE(sizeof(rwdts_api_t), rwdts_api_t);

  RW_ASSERT(apih);

  char router_path[256];
  snprintf (router_path, 256, "/R/RW.DTSRouter/%ld", router_idx);
  apih->client_path = strdup (my_path);
  apih->router_path = strdup (router_path);
  apih->client.ep  = apih->server.ep = ep;
  RW_ASSERT(apih->client.ep);
  apih->flags = flags;
  apih->router_idx = router_idx;
  RW_ASSERT(apih->router_idx);

  apih->rwmemlog = rwmemlog_instance_alloc("DTS API", 0, 0);

  if (rw_tasklet_info && rw_tasklet_info->rwtrace_instance) {
    apih->rwtrace_instance = rw_tasklet_info->rwtrace_instance;
  } else {
    // Init a trace instance
    apih->rwtrace_instance = rwtrace_init();
    RW_ASSERT(apih->rwtrace_instance);

    rwtrace_ctx_category_destination_set(apih->rwtrace_instance,
                                         RWTRACE_CATEGORY_RWTASKLET,
                                         RWTRACE_DESTINATION_CONSOLE);

    rwtrace_ctx_category_severity_set(apih->rwtrace_instance,
                                      RWTRACE_CATEGORY_RWTASKLET,
                                      RWTRACE_SEVERITY_ERROR);
  }

  if (rw_tasklet_info) {
    if (!rw_tasklet_info->apih) {
      rw_tasklet_info->apih = apih;
    }
  }

  RWTRACE_INFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: in rwdts_api_init_internal",
               apih->client_path);
  gettimeofday(&apih->tv_init, NULL);


  apih->tasklet = tasklet;
  apih->rwtasklet_info = rw_tasklet_info;
  if (rw_tasklet_info && rw_tasklet_info->rwlog_instance) {
    apih->rwlog_instance  = rw_tasklet_info->rwlog_instance;
    rwtasklet_info_ref(rw_tasklet_info);
    if ((apih->rwtasklet_info->data_store < RWVCS_TYPES_DATA_STORE_NOSTORE) || 
        (apih->rwtasklet_info->data_store > RWVCS_TYPES_DATA_STORE_BDB)) {
      apih->rwtasklet_info->data_store = RWVCS_TYPES_DATA_STORE_NOSTORE;
    }
  }  else {
    apih->rwlog_instance =  rwlog_init("RW.DtsAPI");
    apih->own_log_instance = 1;
  }

  apih->sched = sched;
  apih->client.cc = rwmsg_clichan_create(apih->client.ep);
  RW_ASSERT(apih->client.cc);
  apih->client.rwq = (q != NULL)?q:rwsched_dispatch_get_main_queue(sched);
  RW_ASSERT(apih->client.rwq);
  rs = rwmsg_clichan_bind_rwsched_queue(apih->client.cc, apih->client.rwq);
  RW_ASSERT(rs ==  RW_STATUS_SUCCESS);

  RWDTS_QUERY_ROUTER__INITCLIENT(&(apih->client.service));
  rwmsg_clichan_add_service(apih->client.cc, &(apih->client.service.base));

  RWDTS_MEMBER_ROUTER__INITCLIENT(&(apih->client.mbr_service));
  rwmsg_clichan_add_service(apih->client.cc, &(apih->client.mbr_service.base));

  apih->client.dest = rwmsg_destination_create(apih->client.ep,
                                               RWMSG_ADDRTYPE_UNICAST,
                                               apih->router_path);
  RW_ASSERT(apih->client.dest);

  RWDTS_API_LOG_EVENT(apih, DtsapiInited,
                      "DTS API initialized with router");

  apih->server.sc = rwmsg_srvchan_create(ep);
  RW_ASSERT(apih->server.sc);
  if (apih->server.sc == NULL) {
    RWDTS_API_LOG_EVENT(apih, DtsapiInitFailed,
                      "DTS API initilization  failed with router");
    rwdts_api_deinit(apih);
    return NULL;
  }

  // if callback copy the callback
  if (cb) {
    apih->state_change = *cb;
  }
  //RW_ASSERT(cb && cb->cb);

  // Bind the server channel to a scheduler queue
  //ATTN  May not be main -- Keep it it to main for now

  apih->server.rwq = (q != NULL)?q:rwsched_dispatch_get_main_queue(sched);
  RW_ASSERT(apih->server.rwq != NULL);

  /* Initalize the server side for Member API */
  rwdts_member_service_init(apih);

  rs = rwmsg_srvchan_bind_rwsched_queue(apih->server.sc, apih->server.rwq);
  if (rs != RW_STATUS_SUCCESS) {
    RWDTS_API_LOG_EVENT(apih, DtsapiInitFailed,
                      "DTS API initilization  failed with router");
    rwdts_api_deinit(apih);
    return NULL;
  }

  // Add service to the server channnel
  rs = rwmsg_srvchan_add_service(apih->server.sc, my_path, &apih->server.service.base, apih);

  if (rs != RW_STATUS_SUCCESS) {
    RWDTS_API_LOG_EVENT(apih, DtsapiInitFailed,
                      "DTS API initilization  failed with router");
    rwdts_api_deinit(apih);
    return NULL;
  }

  INIT_DTS_CREDITS(apih->credits,10000);

  RW_SKLIST_PARAMS_DECL(tab_list_,rwdts_kv_table_handle_t, db_num,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(apih->kv_table_handle_list),&tab_list_);

  RW_SKLIST_PARAMS_DECL(sent_reg_list_,rwdts_api_reg_info_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(apih->sent_reg_list),&sent_reg_list_);

  RW_SKLIST_PARAMS_DECL(sent_reg_update_list_,rwdts_api_reg_info_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(apih->sent_reg_update_list),&sent_reg_update_list_);

  RW_SKLIST_PARAMS_DECL(sent_dereg_list_,rwdts_api_reg_info_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(apih->sent_dereg_list),&sent_dereg_list_);

  RW_SKLIST_PARAMS_DECL(reg_list_,rwdts_member_registration_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(apih->reg_list),&reg_list_);

  rwdts_log_payload_init(&apih->payload_stats);

  // Register keyspecs
  rwdts_register_keyspecs(apih);

  rwmemlog_instance_dts_register(apih->rwmemlog, apih->rwtasklet_info, apih);

  return (apih);
}

#define RWDTS_CHECK_MULTIROUTER_ENABLE(taskletinfo) \
        (taskletinfo->rwvcs && \
              taskletinfo->rwvcs->pb_rwmanifest && \
              taskletinfo->rwvcs->pb_rwmanifest->init_phase && \
              taskletinfo->rwvcs->pb_rwmanifest->init_phase->settings && \
              taskletinfo->rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter && \
              taskletinfo->rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter && \
              taskletinfo->rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter->has_enable &&\
              taskletinfo->rwvcs->pb_rwmanifest->init_phase->settings->rwdtsrouter->multi_dtsrouter->enable)

static uint64_t
rwdts_api_get_router_id(rwtasklet_info_ptr_t tasklet_info)
{
  int router_id = RWDTS_HARDCODED_ROUTER_ID;
  if (RWDTS_CHECK_MULTIROUTER_ENABLE(tasklet_info)) {
    router_id = tasklet_info->rwvcs->identity.rwvm_instance_id?
        tasklet_info->rwvcs->identity.rwvm_instance_id: router_id;
  }
  return (uint64_t)router_id;
}

/*
 * public APIs
 */

/*
 *  Initialize an DTS API context
 *
 */
rwdts_api_t*
rwdts_api_init(rwtasklet_info_ptr_t           tasklet_info,
               const char*                    my_path,
               rwsched_dispatch_queue_t       q,
               uint32_t                       flags,
               const rwdts_state_change_cb_t* cb)
{


  return (rwdts_api_init_internal(tasklet_info,
                                  q,
                                  tasklet_info->rwsched_tasklet_info,
                                  tasklet_info->rwsched_instance,
                                  tasklet_info->rwmsg_endpoint,
                                  my_path,
                                  rwdts_api_get_router_id(tasklet_info),
                                  flags,
                                  cb));
}

/**
 * set the ypbc schema
 */

void
rwdts_api_set_ypbc_schema(rwdts_api_t*                             apih,
                          const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  //apih->ypbc_schema = schema;
  rwdts_api_add_ypbc_schema(apih, schema);

  return;
}

/**
 * Get the ypbc schema
 */

const rw_yang_pb_schema_t*
rwdts_api_get_ypbc_schema(rwdts_api_t* apih)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  return apih->ypbc_schema;
}

/**
 * Add a ypbc schema to the existing list
 * of schemas.
 */
rw_status_t
rwdts_api_add_ypbc_schema(rwdts_api_t*  apih,
                          const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT_TYPE (apih, rwdts_api_t);
  RW_ASSERT_MESSAGE(schema, "rwdts_api_add_ypbc_schema invalid schema");
  if (schema == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, 
        "rwdts_api_add_ypbc_schema invalid schema");
    return RW_STATUS_FAILURE;
  }

  const rw_yang_pb_schema_t* schema_c = NULL;

  if ( NULL == apih->ypbc_schema ) {
    // Add rw-dts schema implicitly
    schema_c = (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwDts);
  } else {
    schema_c = apih->ypbc_schema;
  }

  const rw_yang_pb_schema_t* unified = 
      rw_schema_merge (&apih->ksi, schema_c, schema);

  if (!unified) {

    char errbuf [1024];
    snprintf (errbuf, sizeof(errbuf), "Schema add failed (%s)", schema->schema_module->ns);

    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, errbuf);
    return RW_STATUS_FAILURE;
  }

  apih->ypbc_schema = unified;

  if (apih->xpath_lib_inst) {
    rwdts_xpath_load_schema(apih->xpath_lib_inst, schema);
  }
  return RW_STATUS_SUCCESS;
}

/*
 * Deinit an API context
 */
rw_status_t rwdts_api_deinit(rwdts_api_t *apih)
{
  if (apih == NULL) {
    return RW_STATUS_SUCCESS;
  }
  RWDTS_API_LOG_EVENT(apih, DtsapiDeinited, "DTS API deinit invoked");
  if (apih->rwmemlog) {
    rwmemlog_instance_dts_deregister(apih->rwmemlog, true/*dts_internal, RIFT-8791*/);
    apih->rwmemlog = NULL;
  }

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  /* If there is a destroy call registered with this handle
   * invoke the callback
   */
  if (apih->api_destroyed != NULL) {
    (*apih->api_destroyed)(apih->state_change.ud);
    apih->api_destroyed = NULL;
  }

  rwdts_kv_table_handle_t *kv_tab_handle = NULL;
  // Cleanup pending transactions

  // Deinit a trace instance
  if (apih->rwtrace_instance) {
    rwtrace_ctx_close(apih->rwtrace_instance);
  }

  // ATTN: Iterate through pending transactions and issue abort??
  rwdts_xact_t *xact, *xx;
  HASH_ITER(hh, apih->xacts, xact, xx) {
    rwdts_xact_deinit(xact);
  }
  apih->xacts = NULL;

  if (apih->rwmemlog) {
    rwmemlog_instance_destroy(apih->rwmemlog);
    apih->rwmemlog = NULL;
  }

  // ATTN:: Is there any other cleanup needed  here
  //
  RW_ASSERT(apih->client.dest);
  rwmsg_destination_release(apih->client.dest);

  RW_ASSERT(apih->client.cc);
  rwmsg_clichan_halt(apih->client.cc);

  apih->client.dest = NULL;
  apih->client.cc = NULL;
  apih->client.ep = NULL;

  // Cleanup the server side

  if (apih->server.sc) {
    rwmsg_srvchan_halt(apih->server.sc);
    apih->server.sc = NULL;
  }

  if (apih->timer) {
    struct rwdts_tmp_reg_data_s* tmp_reg = rwsched_dispatch_get_context(apih->tasklet, apih->timer);
    rwsched_dispatch_source_cancel(apih->tasklet, apih->timer);
    rwsched_dispatch_release(apih->tasklet, apih->timer);
    apih->timer = NULL;
    RW_FREE(tmp_reg);
  }

  if (apih->update_timer) {
    struct rwdts_tmp_reg_data_s* tmp_reg = rwsched_dispatch_get_context(apih->tasklet, apih->update_timer);
    rwsched_dispatch_source_cancel(apih->tasklet, apih->update_timer);
    rwsched_dispatch_release(apih->tasklet, apih->update_timer);
    apih->update_timer = NULL;
    RW_FREE(tmp_reg);
  }

  if (apih->dereg_timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->dereg_timer);
    rwsched_dispatch_release(apih->tasklet, apih->dereg_timer);
    apih->dereg_timer = NULL;
  }

  if (apih->xact_timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->xact_timer);
    rwsched_dispatch_release(apih->tasklet, apih->xact_timer);
    apih->xact_timer = NULL;
  }

  /* Deinit the sevice */
  rwdts_member_service_deinit(apih);

  /* Deinit all the registrations */
  rwdts_member_registration_t *entry = NULL;
  rw_status_t rs;

  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while(entry) {
    rs = rwdts_member_registration_deinit(entry);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  }

  /* Deinit member data */
  rwdts_member_data_t *mbr = NULL, *tmp=NULL;
  HASH_ITER(hh, apih->mbr_data, mbr, tmp) {
    HASH_DELETE(hh, apih->mbr_data, mbr);
    RW_FREE_TYPE(mbr, rwdts_member_data_t);
  }

  rwdts_member_delete_shard_db_info(apih);

  rwdts_shard_tab_t *shard_tab = NULL, *tmp1 = NULL;
  HASH_ITER(hh_shard, apih->shard_tab, shard_tab, tmp1) {
    HASH_DELETE(hh_shard, apih->shard_tab, shard_tab);
    if (shard_tab->hh_shard.key) {
      RW_FREE(shard_tab->hh_shard.key);
    }
    RW_FREE_TYPE(shard_tab, rwdts_shard_tab_t);
  }

  kv_tab_handle = RW_SKLIST_HEAD(&(apih->kv_table_handle_list), rwdts_kv_table_handle_t);

  while(kv_tab_handle != NULL) {
    rwdts_member_remove_kv_handle(apih, kv_tab_handle);
    kv_tab_handle = RW_SKLIST_HEAD(&(apih->kv_table_handle_list), rwdts_kv_table_handle_t);
  }

  /* free reg list and sent reg list */
  rwdts_api_reg_info_t* reg = NULL, *next_reg = NULL, *local_reg = NULL;

  reg = RW_SKLIST_HEAD(&(apih->sent_reg_list), rwdts_api_reg_info_t);
  while(reg) {
    next_reg = RW_SKLIST_NEXT(reg, rwdts_api_reg_info_t, element);
    rs = RW_SKLIST_REMOVE_BY_KEY(&(apih->sent_reg_list), &reg->reg_id, &local_reg);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rw_keyspec_path_free(local_reg->keyspec, NULL);
    free(local_reg->keystr);
    RW_FREE(local_reg);
    reg = next_reg;
  }

  reg = RW_SKLIST_HEAD(&(apih->sent_dereg_list), rwdts_api_reg_info_t);
  while(reg) {
    next_reg = RW_SKLIST_NEXT(reg, rwdts_api_reg_info_t, element);
    rs = RW_SKLIST_REMOVE_BY_KEY(&(apih->sent_dereg_list), &reg->reg_id, &local_reg);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rw_keyspec_path_free(local_reg->keyspec, NULL);
    free(local_reg->keystr);
    RW_FREE(local_reg);
    reg = next_reg;
  }

  rwdts_app_addr_res_t *app_add = NULL, *tmp_app_add = NULL;
  HASH_ITER(hh, apih->app_addr_res, app_add, tmp_app_add) {
    HASH_DELETE(hh, apih->app_addr_res, app_add);
    if (app_add->ptr) {
      RW_FREE(app_add->ptr);
    }
    if (app_add->api_name) {
      free(app_add->api_name);
    }
    RW_FREE(app_add);
  }

  if (apih->appconf) {
    free(apih->appconf);
    apih->appconf = NULL;
    apih->appconf_len = 0;
  }

  if (apih->xpath_lib_inst) {
    rwdts_xpath_lib_inst_free(apih->xpath_lib_inst);
    apih->xpath_lib_inst = NULL;
  }

  /* if the log instance is owned by the API close it */

  if (apih->own_log_instance) {
    rwlog_close(apih->rwlog_instance,FALSE);
    apih->rwlog_instance = NULL;
  }

  if (apih->group) {
    int i;
    for (i=0; i < apih->group_len; i++) {
      if (apih->group[i]) {
        RW_FREE_TYPE(apih->group[i], rwdts_group_t);
      }
    }
    free(apih->group);
    apih->group = NULL;
  }
  if (apih->rwtasklet_info) {
    apih->rwtasklet_info->apih = NULL;
    rwtasklet_info_unref(apih->rwtasklet_info);
    apih->rwtasklet_info = NULL;
  }
  protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage *)&apih->payload_stats);
  RW_FREE (apih->client_path);
  RW_FREE (apih->router_path);
  if (apih->xpath_lib_inst) {
    ncx_cleanup(apih->xpath_lib_inst);
  }


  if (apih->conf1.tracert.filters) {
    int i;
    for (i=0; i<apih->conf1.tracert.filters_ct; i++) {
      if (apih->conf1.tracert.filters[i]->path) {
        rw_keyspec_path_free(apih->conf1.tracert.filters[i]->path, NULL);
        apih->conf1.tracert.filters[i]->path = NULL;
      }
      RW_FREE(apih->conf1.tracert.filters[i]);
      apih->conf1.tracert.filters[i] = NULL;
    }
    free(apih->conf1.tracert.filters);
    apih->conf1.tracert.filters = NULL;
    apih->conf1.tracert.filters_ct = 0;
  }

  RW_FREE_TYPE(apih, rwdts_api_t);
  
  return RW_STATUS_SUCCESS;
}

static void rwdts_xact_ksi_init(rwdts_xact_t *xact)
{
  memset(&xact->ksi, 0, sizeof(rw_keyspec_instance_t));
  xact->ksi.error = rwdts_xact_keyspec_err_cb;
  xact->ksi.user_data = xact;
  xact->ksi_errstr = NULL;
}

static
rwdts_xact_t *rwdts_api_xact_create_internal(rwdts_api_t *apih,
                                       uint32_t flags,
                                       rwdts_query_event_cb_routine callback,
                                       const void *user_data,
                                       GDestroyNotify ud_dtor)
{
  struct timeval time_val;

  RW_ASSERT(apih);
  rwdts_xact_t *xact = RW_MALLOC0_TYPE(sizeof(rwdts_xact_t), rwdts_xact_t);

  RW_ASSERT(xact);

  xact->apih = apih;
  xact->flags = flags;

  xact->rwml_buffer = rwmemlog_instance_get_buffer(apih->rwmemlog, "Xact", -apih->stats.num_transactions);

  // If callback is present allocate and copy
  if (callback) {
    // FIXME why malloc
    xact->cb = RW_MALLOC0_TYPE(sizeof(rwdts_event_cb_t), rwdts_event_cb_t);
    RW_ASSERT(xact->cb);
    xact->cb->cb = callback;
    xact->cb->ud = user_data;
    xact->gdestroynotify = ud_dtor;
  }

  gettimeofday(&time_val, NULL);

  // ATTN: Check with Grant on how Id need to be allocated
  rwdts_xact_id__init(&xact->id);
  rwdts_xact_id__init(&xact->toplevelid);
  xact->id.serialno  = ++(apih->xact_id);
  xact->id.taskstamp =  time_val.tv_sec;

  RW_ASSERT(apih->router_idx);
  xact->id.router_idx = apih->router_idx;
  if (!apih->client_idx) {
    xact->id.client_idx = rw_random() + RWDTS_STR_HASH(apih->client_path);
  } else {
    xact->id.client_idx = apih->client_idx;
  }
  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);

  {
    char xact_id_str[128];
    rwdts_xact_id_str(&xact->id, xact_id_str, sizeof(xact_id_str));
    RWMEMLOG(&xact->rwml_buffer, 
	     RWMEMLOG_MEM0, 
	     "Xact create",
	     RWMEMLOG_ARG_STRNCPY(sizeof(xact_id_str), xact_id_str));
  }

  /* FIXME collapse the stuff below and the rwdts_member_xact_init() logic */

  // Init the internal tracking lists
  //
  rwdts_xact_ksi_init(xact);
  RW_DL_INIT(&xact->track.pending);
  RW_DL_INIT(&xact->track.returned);
  RW_DL_INIT(&xact->track.results);

  RW_SKLIST_PARAMS_DECL(tab_list_,rwdts_reg_commit_record_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(xact->reg_cc_list),&tab_list_);

  xact->status = RWDTS_XACT_INIT;
  // Add this transaction to the API hash
  RWDTS_ADD_PBKEY_TO_HASH(hh, apih->xacts, (&xact->id), router_idx, xact);

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, XACT_INITED, "Initialized transaction");

  if (
//      TRUE || 
      (flags&RWDTS_XACT_FLAG_TRACE) || apih->trace.on) {
    rwdts_xact_trace(xact);
  }

  // Only one transaction ?
  if (!apih->trace.persistant) {
    apih->trace.on = 0;
  }

  return xact;
}

rwdts_xact_t *
rwdts_api_xact_create(rwdts_api_t*      apih,
                      uint32_t          flags,
                      rwdts_query_event_cb_routine callback,
                      const void *user_data) {
  return (rwdts_api_xact_create_internal(apih, flags, callback, user_data, NULL));
}

rwdts_xact_t *rwdts_api_xact_create_gi(rwdts_api_t *apih,
                                       uint32_t flags,
                                       rwdts_query_event_cb_routine callback,
                                       const void *user_data,
                                       GDestroyNotify ud_dtor)
{

  rwdts_xact_t *xact = rwdts_api_xact_create_internal(apih,
                                                      flags,
                                                      callback,
                                                      user_data,
                                                      ud_dtor);

  if (xact) {
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  return(xact);
}

rwdts_xact_t*
rwdts_api_xact_create_reduction(rwdts_api_t *api,
                                uint32_t flags,
                                rwdts_query_event_cb_routine callback,
                                const void *user_data,
                                GDestroyNotify ud_dtor)
{
  // TODO: Implement
  return NULL;
}

/* BUG BUG BUG BUG This doesn't take a ref, caller would have to, but better to just call xact_init() below */
rwdts_xact_t *rwdts_lookup_xact_by_id(const rwdts_api_t* apih,
                                      const RWDtsXactID* id) {
  rwdts_xact_t *rw_xact = NULL;
  // Lookup and return if the transaction already exists
  RWDTS_FIND_WITH_PBKEY_IN_HASH(hh, apih->xacts, id, router_idx, rw_xact);
  return rw_xact;
}

rwdts_xact_t *rwdts_member_xact_init(rwdts_api_t*     apih,
                                     const RWDtsXact* xact) {
  rwdts_xact_t *rw_xact = NULL;
  char tmp_log_xact_id_str[128];

  RW_ASSERT(apih);

  // Lookup and return if the transaction already exists
  rw_xact = rwdts_lookup_xact_by_id(apih, &xact->id);

  if (rw_xact) {
    if (!rwdts_is_transactional(rw_xact)) {
      rwdts_xact_ref(rw_xact,__PRETTY_FUNCTION__, __LINE__);
    }
    else {
      if (rw_xact->mbr_state == RWDTS_MEMB_XACT_ST_END) {
        rwdts_xact_ref(rw_xact,__PRETTY_FUNCTION__, __LINE__);
        // Iterate through the old queries and clean them
        rwdts_xact_query_t *query=NULL, *qtmp=NULL;
        HASH_ITER(hh, rw_xact->queries, query, qtmp) {
          rwdts_xact_rmv_query(rw_xact, query);
        }
      }
    }

    if (!rw_xact->rwml_buffer) {
      rw_xact->rwml_buffer = rwmemlog_instance_get_buffer(apih->rwmemlog, "Xact", apih->stats.num_transactions);
    }

    return rw_xact;
  }
  rw_xact = RW_MALLOC0_TYPE(sizeof(rwdts_xact_t), rwdts_xact_t);

  rw_xact->apih = apih;

  apih->stats.num_transactions++;

  rw_xact->rwml_buffer = rwmemlog_instance_get_buffer(apih->rwmemlog, "Xact", apih->stats.num_transactions);

  // Check if this is a sub tranasaction
  if (xact->has_parentid) {
    rw_xact->parent = rwdts_lookup_xact_by_id(apih, &xact->parentid);
 }

  rwdts_xact_id__init(&rw_xact->id);
  protobuf_c_message_memcpy(&rw_xact->id.base, &xact->id.base);

  rwdts_xact_ref(rw_xact, __PRETTY_FUNCTION__, __LINE__);

  RW_DL_INIT(&rw_xact->track.pending);
  RW_DL_INIT(&rw_xact->track.returned);
  RW_DL_INIT(&rw_xact->track.results);

  RW_SKLIST_PARAMS_DECL(tab_list_,rwdts_reg_commit_record_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(rw_xact->reg_cc_list),&tab_list_);

  // Add this transaction to the API hash
  RWDTS_ADD_PBKEY_TO_HASH(hh, apih->xacts, (&rw_xact->id), router_idx, rw_xact);
  RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_XactInited, RWLOG_ATTR_SPRINTF("Xact inited for member %s", apih->client_path));

  RWMEMLOG(&rw_xact->rwml_buffer,
	   RWMEMLOG_MEM0,
	   "Xact created, id",
	   RWMEMLOG_ARG_STRNCPY(sizeof(tmp_log_xact_id_str), rwdts_xact_id_str(&(rw_xact)->id, tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str))));

  return rw_xact;
}

static inline void rwdts_query_result_list_deinit(rw_dl_t *list)  {

  while (RW_DL_LENGTH(list)) {
    rwdts_xact_result_t *res = RW_DL_POP(list, rwdts_xact_result_t, elem);
    RW_FREE_TYPE(res, rwdts_xact_result_t);
  }
  return;
}

static inline void rwdts_xact_result_list_deinit(rw_dl_t *list)  {

  while (RW_DL_LENGTH(list)) {
    rwdts_xact_result_track_t *res;
    res = RW_DL_POP(list, rwdts_xact_result_track_t, res_elem);
    RW_ASSERT_TYPE(res, rwdts_xact_result_track_t);

    RWDtsXactResult *result = res->result;
    RW_ASSERT(result);
    rwdts_xact_result__free_unpacked(NULL, result);
    res->result = NULL;
    RW_FREE_TYPE(res, rwdts_xact_result_track_t);
  }
  return;
}

/*
 * Deinit a transaction block allocated through query
 */

static rw_status_t
rwdts_xact_block_deinit(struct rwdts_xact_block_s *block_p)
{
  RWDtsXactBlock *block = &block_p->subx;

  RW_ASSERT_MESSAGE(block, "rwdts_xact_block_deinit:invalid block");
  if (block == NULL) {
    return RW_STATUS_FAILURE;
  }
  RWDTS_API_LOG_XACT_EVENT(block_p->xact->apih, block_p->xact, RwDtsApiLog_notif_XactDeinited,
                           RWLOG_ATTR_SPRINTF("Called rwdts_xact_block_deinit (block_p = 0x%p\n", block_p)); 

  if (block_p->gdestroynotify) {
    block_p->gdestroynotify((gpointer)block_p->cb.ud);
    block_p->gdestroynotify = NULL;
  }

  protobuf_c_message_free_unpacked(&protobuf_c_default_instance, &block->base);

  return RW_STATUS_SUCCESS;
}

/*
 * deinit query result
 */
void rwdts_query_result_deinit(rwdts_query_result_t* query_result)
{
  RW_ASSERT_MESSAGE(query_result, "rwdts_query_result_deinit:invalid query_result");
  if (query_result == NULL) {
    return;
  }
  if (query_result->keyspec) {
    rw_keyspec_path_free(query_result->keyspec, NULL);
    query_result->keyspec = NULL;
  }
  if (query_result->message) {
    protobuf_c_message_free_unpacked(NULL, query_result->message);
    query_result->message = NULL;
  }
  if (query_result->key_xpath) {
    RW_FREE(query_result->key_xpath);
    query_result->key_xpath = NULL;
  }
  return;
}
void rwdts_xact_query_result_list_deinit(rwdts_xact_t *xact)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  /* clear any query results */
  if (xact->track.query_results) {
    int i;
    for (i = 0; i < xact->track.num_query_results; i++) {
      rwdts_query_result_deinit(&xact->track.query_results[i]);
    }
    RW_FREE(xact->track.query_results);
    xact->track.query_results = NULL;
    xact->track.num_query_results = 0;
    xact->track.query_results_next_idx = 0;
  }
  return;
}
/*
 * Deinit a transaction block allocated through query
 */


static inline void
rwdts_xact_ksi_deinit(rwdts_xact_t *xact)
{
  if(xact->ksi_errstr)
  {
    free(xact->ksi_errstr);
  }
  xact->ksi_errstr = NULL;
}

rw_status_t rwdts_xact_deinit(rwdts_xact_t *xact)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t *apih = xact->apih;
  //xact->status = RWDTS_XACT_DONE;

  /* Is this a xact pending delete , then we will not have the API handle */


  if (xact->member_new_blocks) {
    rwdts_xact_block_t *member_new_blocks;
    int rem_blocks = 0;
    for (; rem_blocks < xact->n_member_new_blocks; rem_blocks++) {
      member_new_blocks = xact->member_new_blocks[rem_blocks];
      rwdts_xact_block_unref(member_new_blocks, __PRETTY_FUNCTION__, __LINE__);
    }
    RW_FREE(xact->member_new_blocks);
    xact->member_new_blocks = NULL;
    xact->n_member_new_blocks = 0;
  }
  if (apih == NULL) {
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
    RW_FREE_TYPE(xact, rwdts_xact_t);
    return RW_STATUS_SUCCESS;
  }

  RWDTS_XACT_LOG_STATE(xact, RWDTS_MEMB_XACT_ST_END, RWDTS_MEMB_XACT_EVT_END);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, XACT_DEINITED, "Deinitializing transaction");

  if (xact->err_report != NULL)
  {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "rwdts_xact_deinit destroying xact with error unreported");
    rwdts_report_error(apih, xact->err_report);
    RWTRACE_ERROR(apih->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET,
                  "End of unreported errors in xact");
    rwdts_error_report__free_unpacked(NULL, xact->err_report);
    xact->err_report = NULL;
  }

  rwdts_deinit_cursor((rwdts_member_cursor_t*)xact->cursor);
  if (xact->redn_pcb) {
    rwdts_xpath_pcb_t *rwpcb = (rwdts_xpath_pcb_t *)xact->redn_pcb;
    RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
    rwdts_xpath_pcb_free(rwpcb);
    xact->redn_pcb = NULL;
  }

  RW_ASSERT(apih);

  /*
   * Remove this transaction from API's hash
   */

  RWMEMLOG(&xact->rwml_buffer,
	   RWMEMLOG_MEM0,
	   "Xact dtor");
  rwmemlog_buffer_return_all(xact->rwml_buffer);
  xact->rwml_buffer = NULL;

  RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_XactDeinited,
                           RWLOG_ATTR_SPRINTF("Xact deinited for member %s", apih->client_path));
  HASH_DELETE(hh, apih->xacts, xact);
  if (xact->blocks) {
    int i;
    for (i = 0; i < xact->blocks_ct; i++) {
      if (xact->blocks[i]) {
        rwdts_xact_block_unref(xact->blocks[i], __PRETTY_FUNCTION__, __LINE__);
      }
      xact->blocks[i] = NULL;
    }

    RW_FREE(xact->blocks);
    xact->blocks = NULL;
  }
  xact->blocks_ct = xact->blocks_allocated = 0;

  if (xact->xact) {
    rwdts_xact__free_unpacked(NULL, xact->xact);
    xact->xact = NULL;
  }



  rwdts_query_result_list_deinit(&(xact->track.pending));

  rwdts_query_result_list_deinit(&(xact->track.returned));

  rwdts_xact_result_list_deinit(&(xact->track.results));

  rwdts_xact_query_result_list_deinit(xact);


  // ATTN -- There should be none -- Commit should move these into API
  rwdts_member_data_t *mbr=NULL, *tmp=NULL;
  HASH_ITER(hh, xact->mbr_data, mbr, tmp) {
    HASH_DELETE(hh, xact->mbr_data, mbr);
    RW_FREE_TYPE(mbr, rwdts_member_data_t);
  }

  // Iterate through the queries and clean them
  rwdts_xact_query_t *query=NULL, *qtmp=NULL;
  HASH_ITER(hh, xact->queries, query, qtmp) {
    rwdts_xact_rmv_query(xact, query);
  }
  // Check if the hashlist is empy
  RW_ASSERT(HASH_COUNT(xact->queries) == 0);


  if (apih->xact_timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->xact_timer);
    rwsched_dispatch_release(apih->tasklet, apih->xact_timer);
    apih->xact_timer = NULL;
  }
  if (xact->getnext_timer)
  {
    RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_XactDeinited,
                             RWLOG_ATTR_SPRINTF("Stop timer during xact deinit for member %s", apih->client_path));
    rwsched_dispatch_source_cancel(apih->tasklet, xact->getnext_timer);
    rwsched_dispatch_release(apih->tasklet, xact->getnext_timer);
    xact->getnext_timer = NULL; // Restart?
  }

  /*
   * Iterate though the transaction commit list and free data
   */
  rwdts_reg_commit_record_t *reg_commit = NULL;
  reg_commit = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);

  while(reg_commit != NULL) {
    RW_ASSERT(reg_commit->xact);
    RW_ASSERT(reg_commit->xact == xact);
    rw_status_t rs = rwdts_member_reg_commit_record_deinit(reg_commit);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    reg_commit = RW_SKLIST_HEAD(&(xact->reg_cc_list), rwdts_reg_commit_record_t);
  }

  if (xact->appconf) {
    free(xact->appconf);
    xact->appconf = NULL;
    xact->appconf_len = 0;
  }

  /* If there is a destroy call registered with our callbacks'
     userdata, invoke the dtor callback for it */
  if (xact->gdestroynotify != NULL) {
    (*xact->gdestroynotify)((gpointer)xact->cb->ud);
    xact->gdestroynotify = NULL;
    xact->cb->ud = NULL;
  }
  if (xact->cb) {
    RW_ASSERT_TYPE(xact->cb, rwdts_event_cb_t);
    RW_FREE_TYPE(xact->cb, rwdts_event_cb_t);
    xact->cb = NULL;
  }

  if (xact->tr) {
    protobuf_c_message_free_unpacked(NULL, &xact->tr->base);
    xact->tr = NULL;
  }
  rwdts_xact_ksi_deinit(xact);

  /* Now free the xact */

  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_FREE_TYPE(xact, rwdts_xact_t);

  return RW_STATUS_SUCCESS;
}

static void
rwdts_member_remove_kv_handle(rwdts_api_t *apih, rwdts_kv_table_handle_t *kv_tab_handle)
{
  rwdts_kv_table_handle_t *removed;
  rw_status_t rwstatus;

  rwstatus = RW_SKLIST_REMOVE_BY_KEY(&apih->kv_table_handle_list,
                                     &kv_tab_handle->db_num,
                                     &removed);
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);

  rwdts_kv_light_deregister_table(kv_tab_handle);
  return;
}

rw_status_t
rwdts_deinit_cursor(rwdts_member_cursor_t *cursor)
{
  rwdts_member_cursor_impl_t *cursor_impl = (rwdts_member_cursor_impl_t*)cursor;
  if (cursor == NULL) {
    return (RW_STATUS_SUCCESS);
  }
  RW_ASSERT_TYPE(cursor_impl, rwdts_member_cursor_impl_t);
  cursor_impl->position = 0;
  RW_FREE_TYPE(cursor_impl, rwdts_member_cursor_impl_t);
  return (RW_STATUS_SUCCESS);
}

void
rwdts_api_reset_stats(rwdts_api_t *apih)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  memset(&apih->stats, 0, sizeof(apih->stats));
  return;
}

static void
rwdts_api_ksi_err_cb(rw_keyspec_instance_t *ksi,
                 const char *msg)
{
  rwdts_api_t *apih = (rwdts_api_t *)ksi->user_data;
  if (apih)
  {
    RWLOG_EVENT(apih->rwlog_instance,
                RwDtsApiLog_notif_NotifyKeyspecError,
                msg);
  }
}
static void rwdts_apih_ksi_init(rwdts_api_t *apih)
{
  memset(&apih->ksi, 0, sizeof(rw_keyspec_instance_t));
  apih->ksi.error = rwdts_api_ksi_err_cb;
  apih->ksi.user_data = apih;
}

/*
 * API related routines
 */

/*
 *  Construct a new DTS api instance
 *
 */
rwdts_api_t*
rwdts_api_new(const rwtasklet_info_t*      tasklet,
              const rw_yang_pb_schema_t*   schema,
              rwdts_api_state_changed      callback,
              gpointer                     user_data,
              GDestroyNotify               api_destroyed)
{
   int r;
   rwdts_state_change_cb_t cb = { callback, user_data };
   char * path;

   r = asprintf(&path, "/R/%s/%d",
          tasklet->identity.rwtasklet_name,
          tasklet->identity.rwtasklet_instance_id);
   RW_ASSERT(r != -1);

   rwdts_api_t *apih = NULL;
   RWVCS_LATENCY_CHK_PRE(tasklet->rwsched_instance);
   apih = rwdts_api_init((rwtasklet_info_ptr_t)tasklet,
                                       path,
                                       NULL,
                                       0,
                                       &cb);
   RWVCS_LATENCY_CHK_POST(tasklet->rwtrace_instance, 
                          RWTRACE_CATEGORY_RWTASKLET, 
                          rwdts_api_init,"rwdts_api_init for %s", path);
   RW_FREE(path);
   apih->ref_cnt = 1;
   apih->print_error_logs = 1;

   apih->api_destroyed = api_destroyed;
   rwdts_apih_ksi_init(apih);

   rwdts_api_set_ypbc_schema(apih, schema);

   apih->xpath_lib_inst = rwdts_xpath_lib_inst_new(schema);

   return apih;
}

rw_status_t
rwdts_api_trace(rwdts_api_t*      apih,
                char*             path)
{
  int newct = 1 + apih->conf1.tracert.filters_ct;
  rw_keyspec_path_t *keyspec = NULL;
  const rw_yang_pb_schema_t* schema = NULL;

  /* Check for Xpath  validity before storing the config  */
  schema = rwdts_api_get_ypbc_schema(apih);
  if (!schema) {
      return RW_STATUS_FAILURE;
  }
  keyspec  = rw_keyspec_path_from_xpath(schema, path, RW_XPATH_KEYSPEC, &apih->ksi);
  if (!keyspec) {
    /* This keyspec does not belong to our schema */
    return RW_STATUS_FAILURE;
  }
  apih->conf1.tracert.filters = (rwdts_trace_filter_t**)realloc(apih->conf1.tracert.filters, (newct * sizeof(rwdts_trace_filter_t*)));
  apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct] = RW_MALLOC0(sizeof(rwdts_trace_filter_t));
  RW_ASSERT(apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]);
  apih->conf1.tracert.filters[apih->conf1.tracert.filters_ct]->path = keyspec;
  apih->conf1.tracert.filters_ct++;
  return RW_STATUS_SUCCESS;
}

static rwdts_xact_t*
rwdts_api_query_internal(rwdts_api_t*                apih,
                         const rw_keyspec_path_t*    keyspec,
                         RWDtsQueryAction            action,
                         uint32_t                    flags,
                         const rwdts_event_cb_t     *cb,
                         const ProtobufCMessage*     msg)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT_MESSAGE(keyspec,"rwdts_api_query_internal:invalid keyspec");
  RW_ASSERT_MESSAGE(cb,"rwdts_api_query_internal:invalid callback");
  if ((keyspec == NULL) ||(cb == NULL)) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical,
                "rwdts_api_query_internal - Invalid parameters");
    return NULL;
  }
  RW_ASSERT_MESSAGE((action != RWDTS_QUERY_INVALID), "rwdts_api_query_internal:action is invalid");
  if (action == RWDTS_QUERY_INVALID) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical,
                "rwdts_api_query_internal - invalid action");
    return NULL;
  }
  /* Set singleton query flags */
  if (action == RWDTS_QUERY_READ || action == RWDTS_QUERY_RPC) {
    flags |= (RWDTS_XACT_FLAG_NOTRAN);
  }
  flags |= (RWDTS_XACT_FLAG_EXECNOW | RWDTS_XACT_FLAG_END);

  if (msg && protobuf_c_message_unknown_get_count(msg)) {
    rwdts_report_unknown_fields((rw_keyspec_path_t*) keyspec, apih, (ProtobufCMessage*)msg);
  }

  rwdts_xact_t *xact = rwdts_api_xact_create(apih, flags, cb?cb->cb:NULL, cb?cb->ud:NULL);
  RW_ASSERT_MESSAGE(xact, "Xact init failed");
  if (xact == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "transaction creation failed");
    return NULL;
  }

  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  if (blk == NULL) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "block creation failed");
    return NULL;
  }
  rw_status_t rs = rwdts_xact_block_add_query_ks(blk,
                                                 keyspec,
                                                 action,
                                                 flags,
                                                 0,
                                                 msg);
  if (rs != RW_STATUS_SUCCESS) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "block add query failed");
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return NULL;
  }
  rs = rwdts_xact_block_execute(blk, flags | RWDTS_XACT_FLAG_END, NULL, NULL, NULL);
  if (rs != RW_STATUS_SUCCESS) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "block execute failed");
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return NULL;
  }

  /* No commit needed, we included XACT_END flag on last block */

  return (xact);
}


/*
 * DTS Create a transaction from a single query
 */

rwdts_xact_t*
rwdts_api_query(rwdts_api_t*                apih,
               const char*                  xpath,
               RWDtsQueryAction             action,
               uint32_t                     flags,
               rwdts_query_event_cb_routine callback,
               const void *               user_data,
               const ProtobufCMessage*      msg)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  if (action == RWDTS_QUERY_INVALID) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "invalid action");
    return NULL;
  }
  if (xpath == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "invalid xpath");
    return NULL;
  }
  rwdts_xact_t *xact = NULL;
  rwdts_event_cb_t cb = { callback, user_data };

  rw_keyspec_path_t *keyspec = NULL;
  keyspec = rw_keyspec_path_from_xpath(rwdts_api_get_ypbc_schema(apih),
                                       (char*)xpath,
                                       RW_XPATH_KEYSPEC,
                                       &apih->ksi);
  if (keyspec) {
    if (!rw_keyspec_path_is_leafy(keyspec)) {
      xact = rwdts_api_query_internal(apih, keyspec, action, flags, &cb, msg);
    }

    rw_keyspec_path_free(keyspec, NULL);
    keyspec = NULL;
  }

  // Try with the xpath reduction library
  if (!xact) {
    xact = rwdts_api_query_reduction(apih, xpath, action, flags, callback, user_data, msg);
  }

  return xact;
}

rwdts_xact_t*
rwdts_api_query_gi(rwdts_api_t*                 apih,
                   const char*                  xpath,
                   RWDtsQueryAction             action,
                   uint32_t                     flags,
                   rwdts_query_event_cb_routine callback,
                   const void *                 user_data,
                   GDestroyNotify               ud_dtor,
                   const ProtobufCMessage*      msg)
{
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  if (action == RWDTS_QUERY_INVALID) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "invalid action");
    return NULL;
  }
  rwdts_xact_t *xact = rwdts_api_query(apih, xpath, action, flags, callback, user_data, msg);
  if (xact) {
    xact->gdestroynotify = ud_dtor;
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  return xact;
}

/*
 * DTS Create a transaction from a single query - with keyspec instead of xpath
 */

rwdts_xact_t*
rwdts_api_query_ks(rwdts_api_t*                 apih,
                   const rw_keyspec_path_t*     keyspec,
                   RWDtsQueryAction             action,
                   uint32_t                     flags,
                   rwdts_query_event_cb_routine callback,
                   gpointer                     user_data,
                   const ProtobufCMessage*      msg)
{

  rwdts_event_cb_t cb = { callback, user_data };
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  if (keyspec == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_ks:invalid keyspec");
    return NULL;
  }
  RW_ASSERT_MESSAGE((action != RWDTS_QUERY_INVALID), "rwdts_api_query_ks:invalid action=%d", action);
  if (action == RWDTS_QUERY_INVALID) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_ks:invalid action");
    return NULL;
  }
  
  if (flags & RWDTS_XACT_FLAG_RETURN_PAYLOAD) {
    if ((action != RWDTS_QUERY_CREATE) &&
        (action != RWDTS_QUERY_UPDATE)) {
      RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_ks:invalid flag");
      return NULL;
    }
  }
  return rwdts_api_query_internal(apih, keyspec, action, flags, &cb, msg);
}

rwdts_xact_t*
rwdts_api_query_reduction(rwdts_api_t*                 apih,
                          const char*                  xpath,
                          RWDtsQueryAction             action,
                          uint32_t                     flags,
                          rwdts_query_event_cb_routine callback,
                          const void*                  user_data,
                          const ProtobufCMessage*      msg)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  RW_ASSERT_MESSAGE((xpath!=NULL), "rwdts_api_query_reduction:invalid xpath");
  if (xpath == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_reduction:invalid xpath");
    return NULL;
  }

  rwdts_xact_t *xact = NULL;
  rwdts_event_cb_t cb = { callback, user_data };
  rwdts_xpath_pcb_t *rwpcb = NULL;

  rw_status_t rs = rwdts_xpath_parse(xpath, RW_XPATH_KEYSPEC, NULL,
                                     apih, &rwpcb);
  if (RW_STATUS_SUCCESS == rs) {
    RW_ASSERT_TYPE(rwpcb, rwdts_xpath_pcb_t);
    if (rwpcb->debug) {
      rwdts_xpath_print_pcb(rwpcb, __FUNCTION__);
    }

    rs = rwdts_xpath_eval(rwpcb);
    if (rwpcb->debug) {
      rwdts_xpath_print_pcb(rwpcb, __FUNCTION__);
    }
    if (RW_STATUS_PENDING == rs) {
      rs = rwdts_xpath_query_int(rwpcb, action, flags, &cb, msg);
      if (RW_STATUS_SUCCESS == rs) {
        xact = rwpcb->xact;
        }
    }
    else {
      if (rwpcb) {
        // Free as either there is no further processing or error occurred
        rwdts_xpath_pcb_free(rwpcb);
        rwpcb = NULL;
      }
    }
  }
  else {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_reduction:xpath parse failed");
  }

  return xact;
}

static void
rwdts_notification_cb(rwdts_xact_t*           xact,
                      rwdts_xact_status_t*    xact_status,
                      void*                   user_data)
{
 RW_ASSERT_MESSAGE(xact, "rwdts_notification_cb - invalid transaction parameter");
 if (xact == NULL) {
   return;
 }
 rwdts_api_t* apih = xact->apih;
 RW_ASSERT_TYPE(apih, rwdts_api_t);

 apih->stats.num_notif_rsp_count++;
 return;
}

/*
 * DTS Send notification as transaction from a single query
 */

rwdts_xact_t*
rwdts_api_send_notification(rwdts_api_t*             apih,
                            const ProtobufCMessage*  msg)
{
  
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_xact_t *xact = NULL;
  rwdts_event_cb_t cb = { rwdts_notification_cb, NULL };

  rw_keyspec_path_t *keyspec = NULL;
  keyspec = (rw_keyspec_path_t *)msg->descriptor->ypbc_mdesc->schema_path_value;
  RW_ASSERT_MESSAGE(keyspec, "rwdts_api_send_notification:invalid keyspec");

  if (!keyspec) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_send_notification:invalid keyspec");
    return NULL;
  }
  xact = rwdts_api_query_internal(apih, keyspec, RWDTS_QUERY_UPDATE, 
                                  RWDTS_XACT_FLAG_ADVISE|RWDTS_XACT_FLAG_NOTRAN, &cb, msg);

  return xact;
}

rwdts_xact_t*
rwdts_api_query_poll(rwdts_api_t*                 api,
                     const char*                  xpath,
                     RWDtsQueryAction             action,
                     uint32_t                     flags,
                     rwdts_query_event_cb_routine callback,
                     const void*                  user_data,
                     GDestroyNotify ud_dtor,
                     const ProtobufCMessage*      msg,
                     uint64_t                     interval,
                     uint64_t                     count)
{
  // TODO: Implement
    RWLOG_EVENT(api->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_api_query_poll- Not implemented");
  return NULL;
}


/*
 * Begin  a query
 */

void*
rwdts_xact_info_get_next_key(const rwdts_xact_info_t *xact_info)
{
  RW_ASSERT_MESSAGE(xact_info, "rwdts_xact_info_get_next_key:xact_info - invalid parameter");
  if (xact_info == NULL) {
    return (NULL);
  }
  rwdts_match_info_t *match_info = (rwdts_match_info_t*)xact_info->queryh;
  if (match_info &&  match_info->query) {
      return (match_info->getnext_ptr);
  }
  return (NULL);
}

rwdts_xact_t *
rwdts_xact_info_get_xact(const rwdts_xact_info_t *xact_info)
{
  rwdts_xact_ref(xact_info->xact,__PRETTY_FUNCTION__, __LINE__);
  return (xact_info->xact);
}

rwdts_api_t *
rwdts_xact_info_get_api(const rwdts_xact_info_t *xact_info)
{
  rwdts_api_ref(xact_info->apih, __PRETTY_FUNCTION__, __LINE__);
  return (xact_info->apih);
}

RWDtsQueryAction
rwdts_xact_info_get_query_action(const rwdts_xact_info_t *xact_info)
{
  rwdts_match_info_t *match_info = (rwdts_match_info_t*)xact_info->queryh;

  if (match_info) {
    RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
    return match_info->query->action;
  }
  return 0;
}

uint32_t
rwdts_xact_info_get_credits(const rwdts_xact_info_t *xact_info)
{
  RW_ASSERT(xact_info);
  rwdts_match_info_t *match_info = (rwdts_match_info_t*)xact_info->queryh;
  if (match_info &&  match_info->query) {
    RWDtsQuery *query = match_info->query;
    rwdts_xact_query_t* xquery = rwdts_xact_find_query_by_id(xact_info->xact, query->queryidx);
    if (query->has_credits) {
      return (xquery->credits);
    }
  }
  return (0);  // ATTN fix this
}

void rwdts_log_payload_init(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *mbr_payload_stats_p)
{
    int i,j,k,l;
    RWPB_F_MSG_INIT(RwDts_data_Dts_Member_PayloadStats,mbr_payload_stats_p);

    mbr_payload_stats_p->n_role = RW_DTS_MEMBER_ROLE_CLIENT+1;
    mbr_payload_stats_p->role = RW_MALLOC(mbr_payload_stats_p->n_role * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role)*));
    for (i = 0 ; i < mbr_payload_stats_p->n_role; i++)
    {
      RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role) *role_p = RW_MALLOC(sizeof(*role_p));
      RWPB_F_MSG_INIT(RwDts_data_Dts_Member_PayloadStats_Role, role_p);
      mbr_payload_stats_p->role[i] = role_p;

      role_p->role_name = i;
      role_p->n_action = RWDTS_QUERY_RPC+1;
      role_p->action = RW_MALLOC(role_p->n_action * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action)*));
      for (j = 0; j < role_p->n_action; j++)
      {
        RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action) *action_p = RW_MALLOC(sizeof(*action_p));
        RWPB_F_MSG_INIT(RwDts_data_Dts_Member_PayloadStats_Role_Action, action_p);
        role_p->action[j] = action_p;

        action_p->action_name = j;
        action_p->n_levels = RW_DTS_XACT_LEVEL_XACT+1;
        action_p->levels = RW_MALLOC(action_p->n_levels * sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels)*));
        for (k = 0 ; k < action_p->n_levels; k++)
        {
          RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels) *levels_p = RW_MALLOC(sizeof(*levels_p));
          RWPB_F_MSG_INIT(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels, levels_p);
          action_p->levels[k] = levels_p;

          levels_p->level_name = k;
          levels_p->n_buckets = RW_DTS_BUCKET_BUCKET_GIGANTIC+1;
          levels_p->buckets = RW_MALLOC(levels_p->n_buckets* sizeof(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels_Buckets)*));
          for (l = 0; l < levels_p->n_buckets; l++)
          {
            RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels_Buckets) *buckets_p = RW_MALLOC(sizeof(*buckets_p));
            RWPB_F_MSG_INIT(RwDts_data_Dts_Member_PayloadStats_Role_Action_Levels_Buckets, buckets_p);
            levels_p->buckets[l] = buckets_p;
            buckets_p->bucket_name = l;
            buckets_p->stat = 0;
            buckets_p->has_stat = 1;
          }
        }
      }
    }
}
void  rwdts_log_payload (RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *payload_stats,
                   RWDtsQueryAction action,
                   RwDts__YangEnum__MemberRole__E role,
                   RwDts__YangEnum__XactLevel__E level,
                   uint32_t len)
{
  RwDts__YangEnum__Bucket__E which_bucket = 0;

  if (action>RWDTS_QUERY_RPC && action< RWDTS_QUERY_INVALID)
    return;
  if (role>RW_DTS_MEMBER_ROLE_CLIENT && role<RW_DTS_MEMBER_ROLE_MEMBER)
    return;
  if (level>RW_DTS_XACT_LEVEL_XACT && level<RW_DTS_XACT_LEVEL_XACT)
    return;
  RWDTS_SIZE_TO_BUCKET(len, which_bucket);
  payload_stats->role[role]->action[action]->levels[level]->buckets[which_bucket]->stat++;
}
void rwdts_calc_log_payload(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *payload_stats,
                      RwDts__YangEnum__MemberRole__E role,
                      RwDts__YangEnum__XactLevel__E level,
                      void *data)
{
#if DTS_DEBUG
  uint32_t len = protobuf_c_message_get_packed_size(NULL, (ProtobufCMessage *)data);
  RwDts__YangEnum__Bucket__E which_bucket = 0;
  RWDTS_SIZE_TO_BUCKET(len, which_bucket);
  payload_stats->role[role]->action[RWDTS_QUERY_INVALID]->levels[level]->buckets[which_bucket]->stat++;
#endif
}
const char *rwdts_state_str(rwdts_state_t state)
{
  return _rwdts_state_enum_values[state].value_name;
}
static void
rwdts_null_cb(rwdts_xact_t* xact,
              rwdts_xact_status_t* xact_status,
              void* ud)
{
}
void rwdts_bcast_state_change(rwdts_api_t *apih)
{
  RWPB_T_MSG(RwDts_data_MemberInfoBcast) member, *member_p;
  member_p =  &member;
  RWPB_F_MSG_INIT(RwDts_data_MemberInfoBcast, member_p);

  strcpy(member_p->name, apih->client_path);
  member_p->has_state = 1;
  member_p->state = apih->dts_state;
  rwdts_event_cb_t advice_cb = {};
  advice_cb.cb = rwdts_null_cb;
  advice_cb.ud = apih;

  rwdts_advise_query_proto_int(apih,
                           (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_MemberInfoBcast),
                           NULL,
                           (const ProtobufCMessage*)&member_p->base,
                           0xdeed,
                           NULL,
                           RWDTS_QUERY_UPDATE,
                           &advice_cb,
                           RWDTS_XACT_FLAG_ADVISE|RWDTS_XACT_FLAG_NOTRAN|RWDTS_XACT_FLAG_END,
                           NULL, NULL);
  // Free member_p
  protobuf_c_message_free_unpacked_usebody(NULL, &member_p->base);
  return;
}


rw_status_t
rwdts_api_keyspec_from_xpath(rwdts_api_t* apih, const char* xpath, rw_keyspec_path_t **out_keyspec)
{

  RW_ASSERT_MESSAGE(out_keyspec, "rwdts_api_keyspec_from_xpath:invalid param out_keyspec");
  if (out_keyspec == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical,
                "rwdts_api_keyspec_from_xpath:invalid param out_keyspec");
    return RW_STATUS_FAILURE;
  } 
  *out_keyspec = NULL;

  *out_keyspec = rw_keyspec_path_from_xpath(rwdts_api_get_ypbc_schema(apih),
                                            (char*)xpath,
                                            RW_XPATH_KEYSPEC, &apih->ksi);

  if (*out_keyspec == NULL) {
    RWDTS_API_LOG_EVENT(apih, XpathKeyConvFailed, "Invalid xpath, xpath to keyspec conversion failed", xpath);
    return RW_STATUS_FAILURE;
  }

  if (*out_keyspec == NULL) {
    return (RW_STATUS_FAILURE);
  }
  return (RW_STATUS_SUCCESS);
}

char*
rwdts_get_app_addr_res(rwdts_api_t* apih, void* addr)
{

  rwdts_app_addr_res_t* app_addr_res = NULL;
  HASH_FIND(hh, apih->app_addr_res, (uint64_t *)&addr, sizeof(uint64_t), app_addr_res);
  if (app_addr_res) {
    return app_addr_res->api_name;
  }
  app_addr_res = RW_MALLOC0(sizeof(rwdts_app_addr_res_t));
  app_addr_res->ptr = RW_MALLOC0(sizeof(uint64_t));
  memcpy((void *)app_addr_res->ptr, (void *)&addr, sizeof(uint64_t));
  app_addr_res->api_name = rw_btrace_get_proc_name(addr);
  RW_ASSERT(app_addr_res->api_name);
  HASH_ADD_KEYPTR(hh, apih->app_addr_res, app_addr_res->ptr, sizeof(uint64_t), app_addr_res);
  return app_addr_res->api_name;
}

RWDtsRtrConnState
rwdts_get_router_conn_state(rwdts_api_t*   apih)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t)
  if (apih->connected_to_router) {
    return RWDTS_RTR_STATE_UP;
  }
  return RWDTS_RTR_STATE_DOWN;
}

/*
 * Define a protobuf instance for  DTS
 */

static void* rwdts_pbc_instance_alloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  return malloc(size);
}

static void* rwdts_pbc_instance_zalloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  return calloc(size,1);
}

static void* rwdts_pbc_instance_realloc(ProtobufCInstance* instance, void* data, size_t size)
{
  (void)instance;
  return realloc(data, size);
}

static void rwdts_pbc_instance_free(ProtobufCInstance* instance, void *data)
{
  (void)instance;
  free(data);
}

static void rwdts_pbc_instance_error(ProtobufCInstance* instance, const char* errmsg)
{
  (void)instance;
  RWDTS_PRINTF("%s", errmsg);
}

ProtobufCInstance _rwdts_pbc_instance = {
  .alloc = &rwdts_pbc_instance_alloc,
  .zalloc = &rwdts_pbc_instance_zalloc,
  .realloc = &rwdts_pbc_instance_realloc,
  .free = &rwdts_pbc_instance_free,
  .error = &rwdts_pbc_instance_error,
  .data = NULL,
};

char *
rwdts_api_client_path(const rwdts_api_t* apih)
{
  return apih->client_path;
}

static const GEnumValue  _rwdts_shard_flavour_values[] =  {
  { RW_DTS_SHARD_FLAVOR_IDENT,  "IDENT",   "IDENT" },
  { RW_DTS_SHARD_FLAVOR_OPAQUE,    "OPAQUE",     "OPAQUE" },
  { RW_DTS_SHARD_FLAVOR_RANGE,"RANGE", "RANGE" },
  { RW_DTS_SHARD_FLAVOR_HASH,   "HASH",    "HASH" },
  { RW_DTS_SHARD_FLAVOR_CONSISTENT,  "CONSISTENT",     "CONSISTENT" },
  {RW_DTS_SHARD_FLAVOR_NULL, "NULL", "NULL"},
  { 0, NULL, NULL}
};

static const GEnumValue  _rwdts_shard_keyfunc_values[] =  {
  { RWDTS_SHARD_KEYFUNC_NOP,  "NOP",   "NOP" },
  { RWDTS_SHARD_KEYFUNC_BLOCKS,    "BLOCKS",     "BLOCKS" },
  { RWDTS_SHARD_KEYFUNC_HASH1,"HASH1", "HASH1" },
  { 0, NULL, NULL}
};

static const GEnumValue  _rwdts_shard_anycast_values[] =  {
  { RWDTS_SHARD_DEMUX_WEIGHTED,  "WEIGHTED",   "WEIGHTED" },
  { RWDTS_SHARD_DEMUX_ORDERED,    "ORDERED",     "ORDERED" },
  { RWDTS_SHARD_DEMUX_ROUND_ROBIN,"ROUND_ROBIN", "ROUND_ROBIN" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_shard_flavor_get_type()
 */
GType rwdts_shard_flavor_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_shard_flavor",
                                  _rwdts_shard_flavour_values);
  return type;
}

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_shard_keyfunc_get_type()
 */
GType rwdts_shard_keyfunc_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_shard_keyfunc_t",
                                  _rwdts_shard_keyfunc_values);
  return type;
}

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_shard_anycast_get_type()
 */
GType rwdts_shard_anycast_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_shard_anycast_t",
                                  _rwdts_shard_anycast_values);
  return type;
}


typedef struct rwdts_api_config_ready_handle_s {
  rwdts_api_t *apih;
  rwdts_api_config_ready_cb_fn config_ready_cb;
  rwsched_dispatch_source_t warning_timer;
  void *userdata;
  rwdts_member_reg_handle_t regh;
  int manifest_count;
  int in_running_state;;
  bool notified;
  rw_sklist_t components;
} rwdts_api_config_ready_handle_t;
typedef struct rwdts_api_config_ready_data_s* rwdts_api_config_ready_data_ptr_t;

typedef struct rwdts_api_config_ready_s {
  rw_sklist_element_t   config_ready_elem; /**skiplist element*/
  char *instance_name;
  RWPB_E(RwBase_StateType) state;
} rwdts_api_config_ready_t;

#define CONFIG_MONITOR_WARN_PERIOD (15/*s*/)

static void rwdts_api_config_ready_warn(void *ud) {
  RW_ASSERT_TYPE(ud, rwdts_api_config_ready_handle_t);
  rwdts_api_config_ready_handle_t *handle = (rwdts_api_config_ready_handle_t *)ud;

  /* Iterate the list, make a summary of unready items */
  int len = 0;
  char *str = NULL;
  int ct = 0;
  rwdts_api_config_ready_t *elem;
  static const RWPB_E(RwBase_StateType) waitstates[] = {
    RW_BASE_STATE_TYPE_CRASHED,
    RW_BASE_STATE_TYPE_LOST,
    RW_BASE_STATE_TYPE_STOPPING,
    RW_BASE_STATE_TYPE_STOPPED,
    RW_BASE_STATE_TYPE_STARTING,
    RW_BASE_STATE_TYPE_INITIALIZING,
    RW_BASE_STATE_TYPE_RECOVERING,
    //RW_BASE_STATE_TYPE_RUNNING,       /* Enable to note and whine about running ones, too */
  };
  const int waitstates_ct = sizeof(waitstates) / sizeof(waitstates[0]);
  int i;
  for (i=0; i<waitstates_ct; i++) {
    RWPB_E(RwBase_StateType) state = waitstates[i];
    int ws_ct=0;
    for (elem = RW_SKLIST_HEAD(&handle->components, rwdts_api_config_ready_t);
	 elem;
	 elem = RW_SKLIST_NEXT(elem, rwdts_api_config_ready_t, config_ready_elem)) {
      if (state != elem->state) {
	continue;
      }

      char *state_str = NULL;
      switch(elem->state) {
      case RW_BASE_STATE_TYPE_CRASHED:
	state_str = "CRASHED: ";
	break;
      case RW_BASE_STATE_TYPE_LOST:
	state_str = "LOST: ";
	break;
      case RW_BASE_STATE_TYPE_STOPPING:
	state_str = "STOPPING: ";
	break;
      case RW_BASE_STATE_TYPE_STOPPED:
	state_str = "STOPPED: ";
	break;
      case RW_BASE_STATE_TYPE_STARTING:
	state_str = "STARTING: ";
	break;
      case RW_BASE_STATE_TYPE_INITIALIZING:
	state_str = "INITIALIZING: ";
	break;
      case RW_BASE_STATE_TYPE_RECOVERING:
	state_str = "RECOVERING: ";
	break;
      case RW_BASE_STATE_TYPE_RUNNING:
	state_str = "RUNNING: ";
        break;
      default:
	RW_ASSERT_MESSAGE(0, "Unknown RW_BASE_STATE_TYPE_xxxx val %d", (int)elem->state);
	break;
      }
      
      if (state_str) {
	char *comma = "";
	if (ct++) {
	  comma = ", ";
	  if (ws_ct) {
	    state_str = "";
	  } else {
	    comma = "; ";
	  }
	}
	ws_ct++;
	const int bufspc = 128;
	str = realloc(str, len + 1 + bufspc);
	snprintf(str+len, bufspc, "%s%s%s", comma, state_str, elem->instance_name);
	str[len+bufspc] = '\0';
	len = strlen(str);
      }
    }
  }

  /* Log a critical-info warning showing the items which are not yet ready */
  if (str) {
    RWLOG_EVENT(handle->apih->rwlog_instance,
                RwDtsApiLog_notif_ConfigReadyMonitor,
                str);
    free(str);
    str = NULL;
  }
}
  
static void rwdts_api_config_ready_update(
    rwdts_api_config_ready_handle_t *rwdts_api_config_ready_handle,
    RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state,
    RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *keyspec_entry)
{
  rwdts_api_config_ready_t *rwdts_api_config_ready = NULL;
  rwdts_api_t  *apih = rwdts_api_config_ready_handle->apih;

  if (!publish_state) {
    if (keyspec_entry->dompath.has_path003 &&
        keyspec_entry->dompath.path003.has_key00) {
      RW_SKLIST_REMOVE_BY_KEY(
          &rwdts_api_config_ready_handle->components,
          &keyspec_entry->dompath.path003.key00.instance_name,
          &rwdts_api_config_ready);
      if (rwdts_api_config_ready) {
        RW_FREE(rwdts_api_config_ready->instance_name);
        RW_FREE_TYPE(rwdts_api_config_ready, rwdts_api_config_ready_t);
      }
    }
  }
  else if (publish_state->has_config_ready &&
           publish_state->config_ready) {
    RW_ASSERT(keyspec_entry->has_dompath &&
              keyspec_entry->dompath.has_path003 &&
              keyspec_entry->dompath.path003.has_key00 &&
              keyspec_entry->dompath.path003.key00.instance_name);

    RW_SKLIST_LOOKUP_BY_KEY(
        &rwdts_api_config_ready_handle->components,
        &keyspec_entry->dompath.path003.key00.instance_name,
        &rwdts_api_config_ready);
    if (!rwdts_api_config_ready) {
      rwdts_api_config_ready = RW_MALLOC0_TYPE(sizeof(*rwdts_api_config_ready), 
                                               rwdts_api_config_ready_t);
      rwdts_api_config_ready->state = RW_BASE_STATE_TYPE_STARTING;
      rwdts_api_config_ready->instance_name = RW_STRDUP(keyspec_entry->dompath.path003.key00.instance_name);
      RW_SKLIST_INSERT(&rwdts_api_config_ready_handle->components,
                       rwdts_api_config_ready);
    }
    if (rwdts_api_config_ready->state != publish_state->state) {
      if (publish_state->state == RW_BASE_STATE_TYPE_RUNNING) {
	rwdts_api_config_ready_handle->in_running_state++;
      } else if (rwdts_api_config_ready->state == RW_BASE_STATE_TYPE_RUNNING) {
	/* Things can crash etc! */
	rwdts_api_config_ready_handle->in_running_state--;
      }
      rwdts_api_config_ready->state = publish_state->state;
  RWTRACE_CRITINFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s -- Instance is reporting %d state ------ at %s api handle(%d:: %d:: %d)",
               rwdts_api_config_ready->instance_name,
               rwdts_api_config_ready->state,
               apih->client_path,
               rwdts_api_config_ready_handle->notified,
               rwdts_api_config_ready_handle->in_running_state,
               rwdts_api_config_ready_handle->manifest_count);
  NEW_DBG_PRINTS("%s -- Instance is reporting %d state ------ at %s api handle(%d:: %d:: %d)\n",
               rwdts_api_config_ready->instance_name,
               rwdts_api_config_ready->state,
               apih->client_path,
               rwdts_api_config_ready_handle->notified,
               rwdts_api_config_ready_handle->in_running_state,
               rwdts_api_config_ready_handle->manifest_count);
      
    }
    if (!rwdts_api_config_ready_handle->notified &&
        (rwdts_api_config_ready_handle->in_running_state == 
        rwdts_api_config_ready_handle->manifest_count)) {
      rwdts_api_config_ready_handle->notified = true;
      RWTRACE_CRITINFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s being notified running state of %d components",
                   apih->client_path, rwdts_api_config_ready_handle->in_running_state);
      
      rwdts_api_config_ready_handle->config_ready_cb(rwdts_api_config_ready_handle->userdata);

      rwdts_api_config_ready_t *r_rwdts_api_config_ready = NULL;

      rwdts_api_config_ready = RW_SKLIST_HEAD(&rwdts_api_config_ready_handle->components, rwdts_api_config_ready_t);
      while (rwdts_api_config_ready) {
        RW_SKLIST_REMOVE_BY_KEY(
            &rwdts_api_config_ready_handle->components,
            &rwdts_api_config_ready->instance_name,
            &r_rwdts_api_config_ready);
        RW_ASSERT((unsigned long)r_rwdts_api_config_ready == (unsigned long)rwdts_api_config_ready);
        RW_FREE(rwdts_api_config_ready->instance_name);
        RW_FREE_TYPE(rwdts_api_config_ready, rwdts_api_config_ready_t);
        rwdts_api_config_ready = RW_SKLIST_HEAD(&rwdts_api_config_ready_handle->components, rwdts_api_config_ready_t);
      }
      if (rwdts_api_config_ready_handle->warning_timer) {
	rwsched_dispatch_source_cancel(apih->tasklet, rwdts_api_config_ready_handle->warning_timer);
	rwsched_dispatch_release(apih->tasklet, rwdts_api_config_ready_handle->warning_timer);
	rwdts_api_config_ready_handle->warning_timer = NULL;
      }
    }
  }
}

static void rwdts_api_config_ready_reg_ready(
    rwdts_member_reg_handle_t  regh,
    rw_status_t                rs,
    void*                      user_data)
{
  rwdts_api_config_ready_handle_t *rwdts_api_config_ready_handle =
      (rwdts_api_config_ready_handle_t *)user_data;
  RW_ASSERT_TYPE(rwdts_api_config_ready_handle, rwdts_api_config_ready_handle_t);

  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_current_cursor(NULL, regh);
  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = NULL;
  while ((publish_state = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState)*) rwdts_member_reg_handle_get_next_element(
                                                                                                regh,
                                                                                                cur,
                                                                                                NULL,
                                                                                                &elem_ks))) {
    rwdts_api_config_ready_update(
        rwdts_api_config_ready_handle,
        publish_state,
        (RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *)elem_ks);
  }
  return;
}

static rwdts_member_rsp_code_t rwdts_api_config_ready_prepare(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* keyspec,
    const ProtobufCMessage*  msg,
    void*                    user_data)
{
  rwdts_api_config_ready_handle_t *rwdts_api_config_ready_handle =
      (rwdts_api_config_ready_handle_t *)user_data;
  RW_ASSERT_TYPE(rwdts_api_config_ready_handle, rwdts_api_config_ready_handle_t);

  rwdts_member_rsp_code_t dts_ret = RWDTS_ACTION_OK;

  rwdts_api_t *api = rwdts_xact_info_get_api(xact_info);
  rwdts_xact_t *xact = rwdts_xact_info_get_xact(xact_info);
  rwdts_member_reg_handle_t regh = xact_info->regh;

  RW_ASSERT_MESSAGE((api != NULL), "rwdts_api_config_ready_prepare:invalid api");
  if (api == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT_MESSAGE((xact != NULL), "rwdts_api_config_ready_prepare:invalid xact");
  if (xact == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT_MESSAGE((regh != NULL),"rwdts_api_config_ready_prepare:invalid regh");
  if (regh == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = 
   (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *)msg;
  
  rwdts_api_config_ready_update(
      rwdts_api_config_ready_handle,
      publish_state,
      (RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *)keyspec);
    
  return dts_ret;
}

#define RWDTS_VCS_GET(t_apih) \
   ((t_apih)->rwtasklet_info?(t_apih)->rwtasklet_info->rwvcs: NULL)

#if 0
void rwdts_api_config_ready_handle_async_f(void *ctx)
{
  rwdts_api_config_ready_handle_t *rwdts_api_config_ready_handle =
      (rwdts_api_config_ready_handle_t *)ctx;
  rwdts_api_config_ready_handle->config_ready_cb(rwdts_api_config_ready_handle->userdata);
  return;
}
#endif
rwdts_api_config_ready_data_ptr_t
rwdts_api_config_ready_register(
    rwdts_api_t *apih,
    rwdts_api_config_ready_cb_fn config_ready_cb,
    void *userdata)
{
  if (!RWDTS_VCS_GET(apih)) {
    return NULL;
  }
  rwdts_api_config_ready_handle_t *rwdts_api_config_ready_handle =
      RW_MALLOC0_TYPE(sizeof(*rwdts_api_config_ready_handle), rwdts_api_config_ready_handle_t);

  RW_ASSERT_TYPE(rwdts_api_config_ready_handle, rwdts_api_config_ready_handle_t);
  rwdts_api_config_ready_handle->config_ready_cb = config_ready_cb;
  rwdts_api_config_ready_handle->userdata = userdata;
  rwdts_api_config_ready_handle->apih = apih;
  RWTRACE_CRITINFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s registering for components running state",
               apih->client_path);
#if 0
  NEW_DBG_PRINTS("%s attemtpting critical readiness \n", apih->client_path);
  
  rwsched_dispatch_async_f(
      apih->tasklet,
      apih->client.rwq,
      rwdts_api_config_ready_handle,
      rwdts_api_config_ready_handle_async_f);
  if (1)
  return (rwdts_api_config_ready_data_ptr_t)rwdts_api_config_ready_handle;
#endif
  rwdts_api_config_ready_handle->warning_timer = rwdts_member_timer_create(apih,
									   (CONFIG_MONITOR_WARN_PERIOD * NSEC_PER_SEC),
									   rwdts_api_config_ready_warn,
									   rwdts_api_config_ready_handle,
									   TRUE);
  rwdts_api_member_register_xpath (apih,
      RWDTS_API_VCS_INSTANCE_CHILD_STATE,
      NULL,
      RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
      RW_DTS_SHARD_FLAVOR_NULL, NULL, 0,-1,0,0,
      rwdts_api_config_ready_reg_ready,
      rwdts_api_config_ready_prepare,
			NULL, NULL, NULL,
      rwdts_api_config_ready_handle,
      &(rwdts_api_config_ready_handle->regh));


    
  RW_SKLIST_PARAMS_DECL(
      config_ready_,
      rwdts_api_config_ready_t,
      instance_name,
      rw_sklist_comp_charptr,
      config_ready_elem);
  RW_SKLIST_INIT(
      &(rwdts_api_config_ready_handle->components),
      &config_ready_);

  rwdts_api_config_ready_handle->manifest_count = 
      rwvcs_manifest_config_ready_count(RWDTS_VCS_GET(apih));
  NEW_DBG_PRINTS("!!!!!! %s registered waiting %d active components\n", apih->client_path, rwdts_api_config_ready_handle->manifest_count);
  return (rwdts_api_config_ready_data_ptr_t)rwdts_api_config_ready_handle;
}

static void rwdts_api_update_element_reg_ready(
    rwdts_member_reg_handle_t  regh,
    rw_status_t                rs,
    void*                      user_data)
{
  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_current_cursor(NULL, regh);
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;
  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = NULL;
  while ((publish_state = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState)*) rwdts_member_reg_handle_get_next_element(
                                                                                                regh,
                                                                                                cur,
                                                                                                NULL,
                                                                                                &elem_ks))) {
    reg->cach_at_reg_ready++;
  }
  reg->reg_ready_done = true;
  if (reg->pub_bef_reg_ready) {
    rwdts_member_reg_handle_solicit_advise(regh);
  }
  return;
}

static void rwdts_api_update_element_async_f(void *ctx)
{
  rwdts_api_update_element_async_t *update_element_async_p = 
      (rwdts_api_update_element_async_t *) ctx;
  RW_ASSERT_TYPE(update_element_async_p, rwdts_api_update_element_async_t);

  RW_ASSERT(update_element_async_p->regh_p);
  if (!(*(update_element_async_p->regh_p))) {
    rwdts_api_member_register_xpath (
        update_element_async_p->apih,
        update_element_async_p->xpath,
        NULL,
        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_NO_PREP_READ|RWDTS_FLAG_CACHE,
        RW_DTS_SHARD_FLAVOR_NULL, NULL, 0, -1, 0, 0,
        rwdts_api_update_element_reg_ready, NULL, NULL, NULL, NULL, NULL,
        update_element_async_p->regh_p);
  }
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)(*update_element_async_p->regh_p);
  if (reg->reg_ready_done) {
    reg->pub_aft_reg_ready++;
  }
  else {
    reg->pub_bef_reg_ready++;
  }
  rwdts_member_reg_handle_update_element_xpath(
      *(update_element_async_p->regh_p),
      update_element_async_p->xpath, 
      update_element_async_p->msg, 
      0,
      NULL);
  RW_FREE(update_element_async_p->xpath);
  protobuf_c_message_free_unpacked(NULL, update_element_async_p->msg);
  RW_FREE_TYPE(update_element_async_p, rwdts_api_update_element_async_t);
  return;
}

void 
rwdts_config_ready_publish(rwdts_api_config_ready_data_t *config_ready_data)
{
  rwdts_api_t *apih = config_ready_data->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  char *xpath = config_ready_data->xpath;
  bool config_ready = config_ready_data->config_ready;
  RWPB_E(RwBase_StateType) state = config_ready_data->state;

  RW_ASSERT_MESSAGE(xpath, "rwdts_config_ready_publish:invalid xpath");
  if (xpath == NULL) {
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_DtsapiCritical, "rwdts_config_ready_publish:Invalid xpath");
    return;
  }

  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = 
      (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState)*)RW_MALLOC0(sizeof(*publish_state));
  RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN_PublishState, publish_state);
  RW_ASSERT(publish_state);
  publish_state->has_config_ready = true;
  publish_state->config_ready = config_ready;
  publish_state->has_state = true;
  publish_state->state = state;

  rwdts_api_update_element_async_t *update_element_async_p = 
      RW_MALLOC0_TYPE(sizeof(*update_element_async_p), rwdts_api_update_element_async_t);
  RW_ASSERT_TYPE(update_element_async_p, rwdts_api_update_element_async_t);
  update_element_async_p->apih = apih;
  update_element_async_p->regh_p = (rwdts_member_reg_handle_t *)(config_ready_data->regh_p);
  update_element_async_p->xpath = RW_STRDUP(xpath);
  update_element_async_p->msg = &(publish_state->base);
  
  rwsched_dispatch_async_f(
      apih->tasklet,
      apih->client.rwq,
      update_element_async_p,
      rwdts_api_update_element_async_f);
  return;
}

/*
 *  return associated api handle
 *
 */
rwdts_api_t*
rwdts_api_find_dtshandle(const rwtasklet_info_t*      tasklet)
{
  RW_ASSERT(tasklet);
  return (tasklet->apih);
}

rwdts_member_reg_handle_t
rwdts_api_find_reg_handle_for_xpath (rwdts_api_t *apih,
                                     const char  *xpath)
{
  rwdts_member_registration_t *reg;
  RW_SKLIST_FOREACH(reg, rwdts_member_registration_t, &apih->reg_list, element) {
    if(!strncmp(reg->keystr, xpath, strlen(reg->keystr))) {
      break;
    }
  }
  return (rwdts_member_reg_handle_t) reg;
}

rw_status_t
rwdts_api_add_modeinfo_query_to_block(
    rwdts_xact_block_t *block,
    char *rwvm_name,
    char *instance_name,
    vcs_vm_state vm_state)
{
  NEW_DBG_PRINTS("Inform /%s/%s the state %d\n", rwvm_name, instance_name, vm_state);
  rw_keyspec_path_t *mode_kspath = NULL;
  rw_status_t status = rw_keyspec_path_create_dup(
      (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_ModeChangeInfo_Instances),
      NULL,
      &mode_kspath);
  RW_ASSERT(mode_kspath);
  RWPB_T_PATHSPEC(RwDts_data_ModeChangeInfo_Instances) *mode_ks = 
      ((RWPB_T_PATHSPEC(RwDts_data_ModeChangeInfo_Instances)*)mode_kspath);
  

  mode_ks->has_dompath = TRUE;
  mode_ks->dompath.path000.has_key00 = 1;
  mode_ks->dompath.path000.key00.rwvm_instance_name = strdup(rwvm_name);
  mode_ks->dompath.path001.has_key00 = 1;
  mode_ks->dompath.path001.key00.instance_name = strdup(instance_name);

  RWPB_T_MSG(RwDts_data_ModeChangeInfo_Instances) *mode_msg
      = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_ModeChangeInfo_Instances)));
  RWPB_F_MSG_INIT(RwDts_data_ModeChangeInfo_Instances, mode_msg);
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

static rwdts_member_rsp_code_t
rwdts_member_handle_mode_change_info(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* key,
    const ProtobufCMessage*  msg,
    uint32_t                 credits,
    void*                    getnext_ptr)
{
  RW_ASSERT_MESSAGE(xact_info, "xact_info NULL in rwdts_member_handle_mode_change_info");
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  } 
  rwdts_api_vm_state_info_t *vm_state_info = (rwdts_api_vm_state_info_t *)xact_info->ud;
  RW_ASSERT_TYPE(vm_state_info, rwdts_api_vm_state_info_t);
  rwdts_api_t *apih = xact_info->xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWPB_T_MSG(RwDts_data_ModeChangeInfo_Instances) *inp;

  RWPB_T_PATHSPEC(RwDts_data_ModeChangeInfo_Instances) *mode_ks = 
      (RWPB_T_PATHSPEC(RwDts_data_ModeChangeInfo_Instances)*) key;

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

  inp = (RWPB_T_MSG(RwDts_data_ModeChangeInfo_Instances)*)msg;
  fprintf (stderr, "ModeChangeInfo Received curr state %d recvd state %d at %s\n", vm_state_info->vm_state, inp->vm_state, apih->client_path);
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
void rwdts_register_vm_state_notification(
    rwdts_api_t *apih,
    vcs_vm_state vm_state,
    rwdts_api_vm_state_cb_t *cb)
{
  if (!apih->rwtasklet_info
      || !apih->rwtasklet_info->rwvcs) {
    return;
  }
  rwvcs_instance_ptr_t rwvcs = apih->rwtasklet_info->rwvcs;
  char *rwvm_name = rwvcs->identity.rwvm_name;
  char *instance_name = to_instance_name(
      apih->rwtasklet_info->identity.rwtasklet_name, 
      apih->rwtasklet_info->identity.rwtasklet_instance_id);

  RW_ASSERT(instance_name);

  rwdts_api_vm_state_info_t *vm_state_info = RW_MALLOC0_TYPE(sizeof(rwdts_api_vm_state_info_t), rwdts_api_vm_state_info_t);
  RW_ASSERT_TYPE(vm_state_info, rwdts_api_vm_state_info_t);
  vm_state_info->instance_name = strdup(instance_name);
  if(rwvm_name) {
    vm_state_info->rwvm_name = strdup(rwvm_name);
  }
  vm_state_info->vm_state = vm_state;
  if (cb) {
    vm_state_info->cb.cb_fn = cb->cb_fn;
    vm_state_info->cb.ud = cb->ud;
  }

  RWPB_T_PATHSPEC(RwDts_data_ModeChangeInfo_Instances) *mode_ks = NULL;
  rw_status_t status = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_ModeChangeInfo_Instances),
                                  &apih->ksi,
                                  (rw_keyspec_path_t**)&mode_ks);
  RW_ASSERT(RW_STATUS_SUCCESS == status);
  mode_ks->has_dompath = TRUE;
  if (rwvm_name) {
    mode_ks->dompath.path000.has_key00 = 1;
    mode_ks->dompath.path000.key00.rwvm_instance_name = strdup(rwvm_name);
  }
  mode_ks->dompath.path001.has_key00 = 1;
  mode_ks->dompath.path001.key00.instance_name = strdup(instance_name);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)mode_ks, &apih->ksi, RW_SCHEMA_CATEGORY_DATA);

  rwdts_registration_t regns[] = {
    { 
      .keyspec = (rw_keyspec_path_t*)mode_ks,
      .desc    = RWPB_G_MSG_PBCMD(RwDts_data_ModeChangeInfo_Instances),
      .flags   = RWDTS_FLAG_SUBSCRIBER /*|RWDTS_FLAG_CACHE*/,
      .callback = {
        .cb.prepare = rwdts_member_handle_mode_change_info,
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

