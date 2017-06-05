/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_audit.c
 * @author Rajesh Velandy
 * @date 2015/12/15
 * @brief DTS  member Audit/reconcile related functions and APIs
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
#include "rwdts_int.h"

#define RWDTS_DEBUG_AUDIT 0

/* The following defines delay between fetches */
#define RWDTS_AUDIT_FETCH_DELAY  (NSEC_PER_SEC * 1/40)
/*
 * A DTS audit is when started against registration will cause
 * the audit API to fetch the current data from the publisher
 * and compare it against the cached version of the published
 * data.
 *
 */

/* static function  prototypes */

/* rwdts_audit_evt_t */
static const char*  _rwdts_audit_evt_values[] =  {
  "RW_DTS_AUDIT_EVT_NULL",
  "RW_DTS_AUDIT_EVT_BEGIN_AUDIT",
  "RW_DTS_AUDIT_EVT_FETCH, ",
  "RW_DTS_AUDIT_EVT_FETCH_DONE",
  "RW_DTS_AUDIT_EVT_FETCH_FAILED",
  "RW_DTS_AUDIT_EVT_COMPARISON_DONE",
  "RW_DTS_AUDIT_EVT_COMPARISON_FAILED",
  "RW_DTS_AUDIT_EVT_CHANGES_OCCURED",
  "RW_DTS_AUDIT_EVT_APP_AUDIT",
  "RW_DTS_AUDIT_EVT_APP_AUDIT_DONE",
  "RW_DTS_AUDIT_EVT_APP_AUDIT_FAILED",
  "RW_DTS_AUDIT_EVT_RECONCILE",
  "RW_DTS_AUDIT_EVT_RECONCILE_DONE",
  "RW_DTS_AUDIT_EVT_RECONCILE_FAILED",
  "RW_DTS_AUDIT_EVT_ABORT",
  "RW_DTS_AUDIT_EVT_REPORT",
  "RW_DTS_AUDIT_EVT_COMPLETE"
};

static const char* rwdts_audit_evt_str(RwDts__YangEnum__AuditEvt__E evt)
{
  if (evt < RW_DTS_AUDIT_EVT_NULL || evt > RW_DTS_AUDIT_EVT_MAX_VALUE) {
    return "";
  }
  return _rwdts_audit_evt_values[evt];
}

/* rwdts_audit_status_t */
static const char*  _rwdts_audit_status_values[] =  {
  "RW_DTS_AUDIT_STATUS_NULL",
  "RW_DTS_AUDIT_STATUS_SUCCESS",
  "RW_DTS_AUDIT_STATUS_FAILURE",
  "RW_DTS_AUDIT_STATUS_ABORTED"
};

static const char* rwdts_audit_status_str(RwDts__YangEnum__AuditStatus__E status)
{
  if (status < RW_DTS_AUDIT_STATUS_NULL || status > RW_DTS_AUDIT_STATUS_ABORTED) {
    return "";
  }
  return _rwdts_audit_status_values[status];
}

/* rwdts_audit_state_t  */
static const char*  _rwdts_audit_state_values[] =  {
  "RW_DTS_AUDIT_STATE_NULL",
  "RW_DTS_AUDIT_STATE_BEGIN",
  "RW_DTS_AUDIT_STATE_FETCH",
  "RW_DTS_AUDIT_STATE_COMPARE",
  "RW_DTS_AUDIT_STATE_APP_AUDIT",
  "RW_DTS_AUDIT_STATE_RECONCILE",
  "RW_DTS_AUDIT_STATE_FAILED",
  "RW_DTS_AUDIT_STATE_COMPLETED"
};

#if RWDTS_DEBUG_AUDIT
#define RWDTS_AUDIT_DEBUG_DUMP_MESSAGE(__mobj__, __reg__) \
do { \
  char *ks_str = NULL; \
  char *message_str = NULL;\
  rwdts_api_t *apih = ((rwdts_member_registration_t*)(__reg__))->apih; \
  rw_keyspec_path_get_new_print_buffer((__mobj__)->keyspec, NULL , \
    rwdts_api_get_ypbc_schema(apih), &ks_str); \
  rw_json_pbcm_to_json((__mobj__)->msg, \
                     (__mobj__)->msg->descriptor, \
                     &message_str); \
  fprintf(stderr, "  ks[%s] : msg[%p %s] cr_audit[%c], upd_audit[%c]\n", \
          ks_str ? ks_str : "", __mobj__, message_str, \
          (__mobj__)->created_by_audit ? 'Y':'N', \
          (__mobj__)->updated_by_audit? 'Y':'N'); \
  RW_FREE(message_str); \
  free(ks_str); \
} while(0)


#define RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(__reg__, __descr__) \
do { \
  rwdts_api_t *apih = ((rwdts_member_registration_t*)(__reg__))->apih; \
  fprintf(stderr, "%s:%d:", __FILE__, __LINE__); \
  fprintf(stderr, "%s - Audit %d, Reg %s:%d {\n",  \
          __descr__, (__reg__)->audit.audit_serial, \
         apih->client_path, (__reg__)->reg_id); \
  rwdts_member_data_object_t *data = NULL, *tmpdata = NULL; \
  HASH_ITER(hh_data, (__reg__)->obj_list, data, tmpdata) { \
    RWDTS_AUDIT_DEBUG_DUMP_MESSAGE(data, __reg__); \
  } \
  fprintf(stderr, "}\n"); \
} while(0)

#define RWDTS_AUDIT_DEBUG_DUMP_DTS_LIST(__reg__, __descr__) \
do { \
  int i; \
  rwdts_api_t *apih = ((rwdts_member_registration_t*)(__reg__))->apih; \
  fprintf(stderr, "%s:%d:", __FILE__, __LINE__); \
  fprintf(stderr, "%s - Audit %d, Reg %s:%d {\n",  \
          __descr__, (__reg__)->audit.audit_serial, \
          apih->client_path, (__reg__)->reg_id); \
  rwdts_member_data_object_t *data = NULL; \
  for (i = 0; i < (__reg__)->audit.n_dts_data; i++) { \
    data = (__reg__)->audit.dts_data[i]; \
    RWDTS_AUDIT_DEBUG_DUMP_MESSAGE(data, __reg__); \
  } \
  fprintf(stderr, "}\n"); \
} while(0)
#else

#define RWDTS_AUDIT_DEBUG_DUMP_MESSAGE(__reg__, __descr__) \
do { \
} while(0)

#define RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(__reg__, __descr__) \
do { \
} while(0)

#define RWDTS_AUDIT_DEBUG_DUMP_DTS_LIST(__reg__, __descr__) \
do { \
} while(0)

#endif

static const char* rwdts_audit_state_str(RwDts__YangEnum__AuditState__E state)
{
  if (state < RW_DTS_AUDIT_STATE_NULL || state >= RW_DTS_AUDIT_STATE_MAX_VALUE) {
    return "";
  }
  return _rwdts_audit_state_values[state];
}

static rw_status_t
rwdts_member_reg_audit_fetch(rwdts_member_registration_t *reg);

static rw_status_t
rwdts_member_reg_audit_fetch_f(rwdts_member_registration_t *reg);

static void
rwdts_member_reg_audit_fetch_cb(rwdts_xact_t*        xact,
                                rwdts_xact_status_t* xact_status,
                                void*                ud);
static rw_status_t
rwdts_member_reg_audit_compare_datasets(rwdts_member_registration_t *reg);

static rw_status_t
rwdts_member_reg_audit_report(rwdts_member_registration_t *reg);

static rw_status_t
rwdts_member_reg_reconcile(rwdts_member_registration_t *reg);

static rw_status_t
rwdts_member_reg_reconcile_cache(rwdts_member_registration_t *reg);

static rw_status_t
rwdts_member_reg_audit_finish(rwdts_member_registration_t *reg);

static void
rwdts_member_reg_audit_run(rwdts_member_registration_t *reg,
                           RwDts__YangEnum__AuditEvt__E  evt,
                           void*                        ctx);
static void
rwdts_member_send_audit_rpc_response(rwdts_audit_t*           audit,
                                     const rwdts_xact_info_t* xact_info,
                                     rwdts_member_registration_t* reg);

static void
rwdts_member_fill_audit_summary(rwdts_api_t*                               apih,
                                RwDts__YangData__RwDts__Dts__AuditSummary* summary,
                                rwdts_member_registration_t*               reg_only);


static void
rwdts_member_reg_audit_deinit_fetched_data(rwdts_member_registration_t *reg,
                                           rwdts_audit_t *audit);


#if RWDTS_ENABLE_AUDIT_DEBUG
static inline void
rwdts_member_reg_audit_print_audit_trace(rwdts_member_registration_t *reg);
#endif /* RWDTS_ENABLE_AUDIT_DEBUG */

/* End of static function prototypes */

rw_status_t
rwdts_member_reg_handle_audit(rwdts_member_reg_handle_t        regh,
                              rwdts_audit_action_t             action,
                              rwdts_audit_done_cb_t            audit_done,
                              void*                            user_data)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t*   apih = reg->apih;
  rwdts_audit_t* audit = &reg->audit;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  
  if (!rwdts_api_in_correct_queue(apih)){
    RW_ASSERT_MESSAGE(0, "API %s called in wrong thread for registration %s\n",
                      __FUNCTION__, reg->keystr);
  }


  // Do some asserts to make sure data structure consistency
  //
  RW_ASSERT((sizeof(_rwdts_audit_evt_values)/sizeof(_rwdts_audit_evt_values[0])) == RW_DTS_AUDIT_EVT_MAX_VALUE);
  RW_ASSERT((sizeof(_rwdts_audit_state_values)/sizeof(_rwdts_audit_state_values[0])) == RW_DTS_AUDIT_STATE_MAX_VALUE);

  if (!RWDTS_AUDIT_IN_INIT_STATE(audit)) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Failed to start audit - invalid current audit state - %s",
                 apih->client_path, rwdts_audit_state_str(audit->audit_state));
    return (RW_STATUS_FAILURE);
  }
  rwdts_audit_init(&reg->audit, audit_done,  user_data, action);

  time(&reg->audit.audit_begin);
  audit->audit_stats.num_audits_started++;

  // Now run the fsm to begin audit
  rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_BEGIN_AUDIT, NULL);

  return RW_STATUS_SUCCESS;
}


/*
 * fetch records from the publisher/datastore for this registration
 */

static rw_status_t
rwdts_member_reg_audit_fetch(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  uint64_t delay = (reg->audit.fetch_attempts + 1) * RWDTS_AUDIT_FETCH_DELAY;

  if (!reg->audit.timer) {
    reg->audit.timer = rwsched_dispatch_source_create(apih->tasklet,
                                                      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                      0,
                                                      0,
                                                      apih->client.rwq);
    /*This timer is not using the RWDTS_TIMEOUT_QUANTUM_MULTIPLE since there is no dependency on
      whether we are running in collapsed mode or not.*/
    rwsched_dispatch_source_set_timer(apih->tasklet,
                                      reg->audit.timer,
                                      dispatch_time(DISPATCH_TIME_NOW, delay),
                                      0,
                                      delay/2);

    rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                                reg->audit.timer,
                                                (rwsched_dispatch_function_t)rwdts_member_reg_audit_fetch_f);


    rwsched_dispatch_set_context(apih->tasklet,
                                 reg->audit.timer,
                                 reg);


    rwsched_dispatch_resume(apih->tasklet,
                            reg->audit.timer);
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_audit_fetch_f(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t*   apih = reg->apih;
  rwdts_xact_t*  xact = NULL;
  rw_status_t    rs = RW_STATUS_SUCCESS;
  uint32_t       flags = 0;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT_TYPE(reg->apih, rwdts_api_t);
  RW_ASSERT(reg->keyspec);

  RWDTS_API_LOG_REG_EVENT(reg->apih, reg, BeginAudit, "Begining DTS registration audit",
                          reg->reg_id);

  /* if running a audit fecth timer - stop this */
  if (reg->audit.timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, reg->audit.timer);
    rwsched_dispatch_source_release(apih->tasklet, reg->audit.timer);
    reg->audit.timer = NULL;
  }

  if (reg->audit.fetch_attempts >= RWDTS_MAX_FETCH_ATTEMPTS) {
    char* ks_str = NULL;
    rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , rwdts_api_get_ypbc_schema(reg->apih), &ks_str);
    if (!reg->silent_retry) {
      RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Audit failed to converge after %u attempts!!! -"
                 " Giving up audit in state - %s for regn id[%u] ks[%s]",
                 apih->client_path,
                 reg->audit.fetch_attempts,
                 rwdts_audit_state_str(reg->audit.audit_state),
                 reg->reg_id,
                 ks_str);
    }
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_RECONCILE_FAILED, NULL);
    RW_FREE(ks_str);
    return RW_STATUS_SUCCESS;
  }

  // ATTN -- Until datastore is in place and use only audit subscriber registrations with cache
  if (!((reg->flags & RWDTS_FLAG_SUBSCRIBER) && (reg->flags & RWDTS_FLAG_CACHE))) {
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_COMPLETE, NULL);
    return RW_STATUS_SUCCESS;
  }

  reg->audit.fetch_attempts++;

  // Deinit all the fecthed data for this registration's audit
  rwdts_member_reg_audit_deinit_fetched_data(reg, &reg->audit);

  reg->outstanding_requests++;

  rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg);
  xact = rwdts_api_query_ks(reg->apih,
                            reg->keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            rwdts_member_reg_audit_fetch_cb,
                            reg,
                            NULL);

  rs = xact?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;

  return rs;
}




/*
 * callback for the  Member registration handle audit
 */

static void
rwdts_member_reg_audit_fetch_cb(rwdts_xact_t*        xact,
                                rwdts_xact_status_t* xact_status,
                                void*                ud)
{
  rw_status_t                  rs;
  rwdts_member_registration_t* reg = (rwdts_member_registration_t*)ud;
  rwdts_api_t*                 apih = reg->apih;


  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(reg->outstanding_requests > 0);
  RW_ASSERT(!reg->audit.dts_data);
  if (!xact_status->xact_done) {
    return;
  }

  if (reg->audit.audit_state != RW_DTS_AUDIT_STATE_FETCH) {
    goto cleanup;
  }

  switch(xact_status->status) {
    case RWDTS_XACT_COMMITTED:
    case RWDTS_XACT_RUNNING:
      // ATTN this does not support multiple callbacks -- For now assuming a single result callback
      // result presence is orthogonal to error --
      // you can and will get results from a transaction that ultimately aborts or fails outright

      reg->audit.fetch_cb_success++;

      if (reg->audit.n_dts_data != 0) {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "Received more than one[%d] callbacks for  a single fetch",
                     reg->audit.n_dts_data);
        reg->audit.fetch_failed = true;
        reg->audit.fetch_failure++;
        goto cleanup;
      }

      rwdts_query_result_t *query_result = rwdts_xact_query_result(xact, 0);
      if (xact->track.num_query_results == 0) {
        reg->audit.n_dts_data = 0;
        reg->audit.dts_data = NULL;
        
        if (!reg->dts_internal 
            && RW_SCHEMA_CATEGORY_CONFIG == rw_keyspec_path_get_category (reg->keyspec)) {
          reg->audit.fetch_failed = true;
          reg->silent_retry = true;

          char *ks_str = NULL;
          rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , rwdts_api_get_ypbc_schema(apih), &ks_str);
          RWDTS_API_LOG_REG_EVENT(reg->apih, reg, AuditConfigNoResult, ks_str);
          RW_FREE(ks_str);
          goto cleanup;
        }
        break;
      }

      if (!query_result) {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "Audit failed - get result returned [%d]", rs);
        reg->audit.audit_state  = RW_DTS_AUDIT_STATE_FAILED;
        reg->audit.audit_status =  RW_DTS_AUDIT_STATUS_FAILURE;
        reg->audit.fetch_failed = true;
        goto cleanup;
      }

      reg->audit.dts_data = rwdts_get_member_data_array_from_msgs_and_keyspecs(reg,
                                                                               query_result,
                                                                               xact->track.num_query_results,
                                                                               &reg->audit.n_dts_data);
      RW_ASSERT(reg->audit.dts_data);
      rwdts_xact_query_result_list_deinit(xact);
      break;

    case RWDTS_XACT_ABORTED:
    case RWDTS_XACT_FAILURE:
    case RWDTS_XACT_INIT:
      reg->audit.fetch_cb_failure++;
      rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_FETCH_FAILED, NULL);
      break;
    default:
      RW_ASSERT_MESSAGE(0, "Unknown status %u\n", xact_status->status);
      break;
  }

cleanup:
  /* Destroy the xact when it is done.*/
  if (xact_status->xact_done == true) {
    if (reg->audit.fetch_failed) {
      reg->audit.fetch_failed = false;
      rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_FETCH_FAILED, NULL);
    } else {
      rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_FETCH_DONE, NULL);
    }
    reg->outstanding_requests--;
    if (reg->pending_delete && reg->outstanding_requests == 0) {
      rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_ABORT, NULL);
      rs = rwdts_member_registration_deinit(reg);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
    }
  }

  rwdts_member_reg_handle_unref((rwdts_member_reg_handle_t)reg);
  return;
}

static rw_status_t
rwdts_member_reg_audit_compare_datasets(rwdts_member_registration_t *reg)
{
  int          i;
  rwdts_api_t* apih = NULL;
  int          comp_failed = 0;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  reg->audit.mismatched_count = 0;
  // If the number of records that we fetched does not equal the
  // number of records  in the tasklet then  fail the audit

  // If there were changes during the fetch post that event
  if (reg->audit.fetch_invalidated) {
    char *ks_str = "";

    rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , rwdts_api_get_ypbc_schema(apih), &ks_str);
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s : Changes occured while audit in progress reg-id [%u], ks[%s] -  discarding %d records",
                 apih->client_path, reg->reg_id, ks_str, reg->audit.n_dts_data);
    RW_FREE(ks_str);
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_CHANGES_OCCURED, NULL);
    // The comparison was still successfull - -return success
    return RW_STATUS_SUCCESS;
  }

#ifdef _RWDTS_ENABLED_COUNT_MATCH__
  if (reg->audit.n_dts_data !=  rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg)) {
    reg->audit.audit_status = RW_DTS_AUDIT_STATUS_FAILURE;
    reg->audit.mismatched_count = 1;
    snprintf(reg->audit.audit_status_message, sizeof(reg->audit.audit_status_message),
             "Number of records is wrong between dts[%d] and %s[%d] for registration id %u\n",
             reg->audit.n_dts_data, apih->client_path, rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg),
             reg->reg_id);
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s:Audit failed - Number of records does not match between dts[%d] and task[%d] for registration id %u\n",
                 apih->client_path, reg->audit.n_dts_data, rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg),
                 reg->reg_id);
    comp_failed = 1;
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_COMPARISON_FAILED, NULL);
    // The comparison was still successfull - -return success
    return RW_STATUS_SUCCESS;
  }

  if (0 == rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg) &&
      reg->audit.n_dts_data != 0) {
    snprintf(reg->audit.audit_status_message, sizeof(reg->audit.audit_status_message),
             "Number of records is wrong between dts[%d] and %s[%d] for registration id %u\n",
             reg->audit.n_dts_data, apih->client_path, rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg),
             reg->reg_id);
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s:Comparison failed - Number of records does not match between dts[%d] and task[%d] for registration id %u\n",
                 apih->client_path, reg->audit.n_dts_data, rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg),
                 reg->reg_id);
    comp_failed = 1;
    reg->audit.objs_missing_in_cache = reg->audit.n_dts_data;
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_COMPARISON_FAILED, NULL);
    // The comparison was still successfull - -return success
    return RW_STATUS_SUCCESS;
  }
#endif /* _RWDTS_ENABLED_COUNT_MATCH__ */

  rwdts_member_data_object_t* data     = NULL;
  rwdts_member_data_object_t* tmpdata  = NULL;

  // Asssume everything is missing and decrement as we find them in cache
  reg->audit.objs_missing_in_dts       = HASH_CNT(hh_data, reg->obj_list);

  // Update some counters
  reg->audit.objs_in_cache       = HASH_CNT(hh_data, reg->obj_list);
  reg->audit.objs_from_dts       = reg->audit.n_dts_data;

  // Iterate through our list of data records and mark everything as missing in dts
  HASH_ITER(hh_data, reg->obj_list, data, tmpdata) {
    // Allocate a data audit record ...
    if (!data->audit) {
      data->audit = DTS_APIH_MALLOC0_TYPE(reg->apih,RW_DTS_DTS_MEMORY_TYPE_MEMBER_DATA_AUDIT,
                                          sizeof(data->audit[0]), rwdts_member_data_object_audit_t);
    }
    data->audit->missing_in_dts       = 1;
    data->audit->missing_in_cache     = 0;
    data->audit->mis_matched_contents = 0;
  }
  // Debug dump cache list
  RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(reg, "Cache in compare data sets");

  // Check if the objects in DTS are all present in the tasklet
  for (i = 0; i < reg->audit.n_dts_data; i++) {

    rwdts_member_data_object_t *mobj    = NULL;
    rwdts_member_data_object_t *dts_obj = reg->audit.dts_data[i];
    // Does the contents match?
    ProtobufCPackOptions proto_comp_opts =
    {
      .sort_all = FALSE,
      .discard_unknown = TRUE,
    };

    RW_ASSERT(dts_obj);
    RW_ASSERT(dts_obj->keyspec);
    RW_ASSERT(dts_obj->key);

    HASH_FIND(hh_data, reg->obj_list, dts_obj->key, dts_obj->key_len, mobj);
    char *ks_str = NULL;
    rw_keyspec_path_get_new_print_buffer(dts_obj->keyspec, NULL , rwdts_api_get_ypbc_schema(apih), &ks_str);

    /* Did we find a match */
    if (mobj) {
      RW_ASSERT(mobj->audit);
      mobj->audit->missing_in_dts = 0;

      // Deep compare the data ignoring unknown fields
      if (!protobuf_c_message_is_equal_deep_opts(NULL, mobj->msg, dts_obj->msg, &proto_comp_opts)) {
        comp_failed = 1;
        reg->audit.audit_status = RW_DTS_AUDIT_STATUS_FAILURE;
        reg->audit.objs_mis_matched_contents++;
        if (dts_obj->audit == NULL) {
          dts_obj->audit = DTS_APIH_MALLOC0_TYPE(apih,
                                                 RW_DTS_DTS_MEMORY_TYPE_MEMBER_DATA_AUDIT,
                                                 sizeof(data->audit[0]), rwdts_member_data_object_audit_t);
        }
        dts_obj->audit->mis_matched_contents =  mobj->audit->mis_matched_contents = 1;
        reg->stats.audit.mismatched_count++;
        RWTRACE_INFO(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "Audit failed - DTS and cached versions of message does not match for keypsec %s\n",
                     ks_str ? ks_str : "");
        RWDTS_API_LOG_REG_EVENT(reg->apih, reg, AuditMismatchCacheDts, "Object mismatch between cache and dts", ks_str);
      }

      // For each matched decrement missing in DTS by one
      // we had assumed everything to be missing
      RW_ASSERT(reg->audit.objs_missing_in_dts > 0);
      reg->audit.objs_missing_in_dts--;
      reg->audit.objs_matched++;
    } else {
      comp_failed = 1;

      if (dts_obj->audit == NULL) {
        dts_obj->audit = DTS_APIH_MALLOC0_TYPE(apih,
                                               RW_DTS_DTS_MEMORY_TYPE_MEMBER_DATA_AUDIT,
                                               sizeof(data->audit[0]), rwdts_member_data_object_audit_t);
      }
      dts_obj->audit->missing_in_cache = 1;
      reg->audit.audit_status = RW_DTS_AUDIT_STATUS_FAILURE;
      reg->audit.objs_missing_in_cache++;
      reg->stats.audit.missing_in_cache++;
      RWTRACE_INFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: Audit failed - dts object (ks = %s) is missing in cache for registration id %u\n",
                   apih->client_path, ks_str ? ks_str : "", reg->reg_id);
      RWDTS_API_LOG_REG_EVENT(reg->apih, reg, AuditMissingCache, "Object is missing in cache", ks_str);
    }
    free(ks_str);
  }
  if (reg->audit.objs_missing_in_dts) {
    comp_failed = 1;
    reg->stats.audit.missing_in_dts += reg->audit.objs_missing_in_dts;
  }
  if (comp_failed) {
    RWTRACE_WARN(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s:  Comparison failed - for registration id %u"
                 "\nNum objs missing in dts     : %u"
                 "\nNum objs missing in cache   : %u"
                 "\nNum objs mismatch dts&cache : %u"
                 "\nNum objs compared&matched   : %u",
                 apih->client_path, reg->reg_id,
                 reg->audit.objs_missing_in_dts,
                 reg->audit.objs_missing_in_cache,
                 reg->audit.objs_mis_matched_contents,
                 reg->audit.objs_matched);
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_COMPARISON_FAILED, NULL);
  } else {
    rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_COMPARISON_DONE, NULL);
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_audit_report(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t *apih = reg->apih;
#if RWDTS_ENABLE_AUDIT_DEBUG
  char  tmp_buf[128];
  char  *ks_str = NULL;
#endif /* RWDTS_ENABLE_AUDIT_DEBUG */

  if (reg->audit.audit_action == RWDTS_AUDIT_ACTION_RECOVER) {
    // For recovery just print a single line status
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s:  Audit completed for regn-id %d %s"
                 "miss_dts[%u] miss_cache[%u] mismatch[%u] matched[%u]  status[%s] state[%s]",
                 apih->client_path, reg->reg_id,reg->keystr,
                 reg->audit.objs_missing_in_dts,
                 reg->audit.objs_missing_in_cache,
                 reg->audit.objs_mis_matched_contents,
                 reg->audit.objs_matched,
                 rwdts_audit_status_str(reg->audit.audit_status),
                 rwdts_audit_state_str(reg->audit.audit_state));
    return (RW_STATUS_SUCCESS);
  }

#if RWDTS_ENABLE_AUDIT_DEBUG
  rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , rwdts_api_get_ypbc_schema(reg->apih), &ks_str);
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWTRACE_INFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "Last Audit status for registration %s:%d:ks[%s]\n"
               "  Audit serial : %u\n"
               "  Audit State  : %s\n"
               "  Audit Status : %s\n"
               "  Audit Begin  : %s\n"
               "  Audit End    : %s\n"
               "  Audit Stats :\n"
               "    Num Audit started  :   %d\n"
               "    Num Audit succeeded:   %d\n"
               "    Num Audit fetches  :   %d\n"
               "    Num objs from DTS  :   %d\n"
               "    Num objs in cache  :   %d\n"
               "    Num objects missing in dts     : %d\n"
               "    Num objects missing in cache   : %d\n"
               "    Num objects mis-matched content: %d\n"
               "    Num objects compared & matched : %d\n"
               "    Num objs updated by audit      : %d\n"
               "    Num Audit error count: %d\n"
               "    Num objects after audit: %d\n",
               reg->apih->client_path,reg->reg_id, ks_str ? ks_str : "",
               reg->audit.audit_serial,
               rwdts_audit_state_str(reg->audit.audit_state),
               rwdts_audit_status_str(reg->audit.audit_status),
               ctime_r(&reg->audit.audit_begin, tmp_buf),
               ctime_r(&reg->audit.audit_end, tmp_buf),
               reg->audit.audit_stats.num_audits_started,
               reg->audit.audit_stats.num_audits_succeeded,
               reg->audit.fetch_attempts,
               reg->audit.objs_from_dts,
               reg->audit.objs_in_cache,
               reg->audit.objs_missing_in_dts,
               reg->audit.objs_missing_in_cache
               reg->audit.objs_mis_matched_contents,
               reg->audit.objs_matched,
               reg->audit.objs_updated_by_audit,
               reg->audit.audit_issues.errs_ct,
               HASH_CNT(hh_data, reg->obj_list));

  int i;
  for(i = 0; i < reg->audit.audit_issues.errs_ct; i++) {
    RWTRACE_INFO(apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                 "%s#%-2d %-4d %-70s\n", pad2, i,
                 reg->audit.audit_issues.errs[i].rs,
                 reg->audit.audit_issues.errs[i].str);
  }
  rwdts_member_reg_audit_print_audit_trace(reg);
  free(ks_str);
#endif

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_app_audit(rwdts_member_registration_t *reg)
{
  bool grp_found = false;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih          = reg->apih;
  rwdts_appconf_reg_t* acreg = reg->appconf;
  rwdts_appconf_t* ac;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (reg->audit.audit_status != RW_DTS_AUDIT_STATUS_SUCCESS)  {
    RWTRACE_INFO(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Audit already in failed status %d - no app audit will be done for reg [%d]",
                 apih->client_path, reg->audit.audit_status, reg->reg_id);
    rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_APP_AUDIT_FAILED, NULL);
    return (RW_STATUS_FAILURE);
  }

  // ATTN -- appconf group is  must for app audits  - else assume success
  if (acreg != NULL && acreg->ac != NULL) {
    RW_ASSERT_TYPE(acreg, rwdts_appconf_reg_t);
    ac = acreg->ac;
    RW_ASSERT_TYPE(ac, rwdts_appconf_t);

    reg->audit.audit_issues.errs_ct = 0;
    if (ac->cb.xact_init_dtor) {
      ac->cb.config_apply_gi(apih,
                             ac,
                             NULL,
                             RWDTS_APPCONF_ACTION_AUDIT,
                             ac->cb.ctx,
                             NULL);
    } else {
      ac->cb.config_apply(apih,
                          ac,
                          NULL,
                          RWDTS_APPCONF_ACTION_AUDIT,
                          ac->cb.ctx,
                          NULL);
    }

    // Check  if the app added any audit related issues?
    if (reg->audit.audit_issues.errs_ct) {
      rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_APP_AUDIT_FAILED, NULL);
      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: Audit failed for registration id %d",
                   apih->client_path, reg->reg_id);
      return RW_STATUS_SUCCESS;
    }
    grp_found = true;
  } else if (reg->ingroup) {
    RW_ASSERT_TYPE(reg->group.group, rwdts_group_t);
    reg->audit.audit_issues.errs_ct = 0;
      reg->group.group->cb.xact_event(apih,
                                      reg->group.group,
                                      NULL,
                                      RWDTS_MEMBER_EVENT_AUDIT,
                                      reg->group.group->cb.ctx,
                                      NULL);

    if (reg->audit.audit_issues.errs_ct) {
      rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_APP_AUDIT_FAILED, NULL);
      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: Audit failed for registration id %d",
                   apih->client_path, reg->reg_id);
      return RW_STATUS_SUCCESS;
     }
    grp_found = TRUE;
  }

  if (!grp_found) {
      RWTRACE_WARN(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "%s: No appconf group found for this reg id[%d]- no reconciliation will be done",
                   apih->client_path, reg->reg_id);
  }

  // Nothing failed - post APP Audit done
  rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_APP_AUDIT_DONE, NULL);
  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_reconcile(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_appconf_reg_t*        acreg = reg->appconf;
  rwdts_appconf_t*            ac = NULL;
  rwdts_api_t*                apih = NULL;

  apih = reg->apih;

  if (acreg) {
    RW_ASSERT_TYPE(acreg, rwdts_appconf_reg_t);
    ac = acreg->ac;
    RW_ASSERT_TYPE(ac, rwdts_appconf_t);
  }
  apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

#if 0
  // ATTN -- appconf group is  must for audits
  if (ac == NULL && (!reg->ingroup || reg->group.group == NULL)) {
    RWTRACE_WARN(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Audit failed to reconile  for reg [%d] - No appconf/reg group",
                 apih->client_path,  reg->reg_id);
    rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_RECONCILE_FAILED, NULL);
    return (RW_STATUS_FAILURE);
  }
#endif

  if (ac) {
    if (ac->cb.xact_init_dtor) {
      ac->cb.config_apply_gi(reg->apih,
                             ac,
                             NULL,
                             RWDTS_APPCONF_ACTION_INSTALL,
                             ac->cb.ctx,
                             NULL);
    } else {
      ac->cb.config_apply(reg->apih,
                          ac,
                          NULL,
                          RWDTS_APPCONF_ACTION_INSTALL,
                          ac->cb.ctx,
                          NULL);
    }
  } else if (reg->ingroup) {
    RW_ASSERT_TYPE(reg->group.group, rwdts_group_t);
    rwdts_group_reg_install_event(reg->group.group,
                                  reg->reg_id);
  }
  rwdts_member_reg_audit_run(reg,RW_DTS_AUDIT_EVT_RECONCILE_DONE, NULL);
  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_reconcile_cache(rwdts_member_registration_t *reg)
{
  int                         i;
  rwdts_member_data_object_t* mobj = NULL;
  rw_status_t                 rs = RW_STATUS_SUCCESS;
  ProtobufCMessage*           oldmsg;

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RWTRACE_INFO(reg->apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s:Reconcile cache, dts count [%d] and task count [%d] for registration id %u\n",
               reg->apih->client_path, reg->audit.n_dts_data,
               rwdts_member_data_get_count(NULL, (rwdts_member_reg_handle_t)reg),
               reg->reg_id);


  // Debug dump cache list
  RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(reg, "In cache List");

  // Debug dump the objects in from DTS
  RWDTS_AUDIT_DEBUG_DUMP_DTS_LIST(reg, "From DTS List");

  for (i = 0; i < reg->audit.n_dts_data; i++) {
    rwdts_member_data_object_t *dts_obj = reg->audit.dts_data[i];
    rwdts_member_data_object_t *in_cache_obj  = NULL;

    RW_ASSERT((dts_obj->audit == NULL) ||
              (dts_obj->audit->missing_in_dts == 0));
    if (dts_obj->audit && (dts_obj->audit->mis_matched_contents)){
      // Create the object in cache
      RW_ASSERT(dts_obj->audit->missing_in_dts == 0);
      RW_ASSERT(dts_obj->key);
      RW_ASSERT(dts_obj->key_len > 0);
      mobj = NULL;
      HASH_FIND(hh_data, reg->obj_list, dts_obj->key, dts_obj->key_len, mobj);
      RW_ASSERT(mobj);
      RW_ASSERT(mobj->audit);
      RW_ASSERT(mobj->audit->mis_matched_contents);
      mobj->audit->mis_matched_contents = 0;
      mobj->updated_by_audit = 1;
      oldmsg = mobj->msg;
      mobj->msg = protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                               dts_obj->msg,
                                               dts_obj->msg->descriptor);
      RW_ASSERT(mobj->msg);
      
      protobuf_c_message_free_unpacked_usebody(&protobuf_c_default_instance, oldmsg);
      RW_ASSERT(rw_keyspec_path_is_equal(&reg->apih->ksi, mobj->keyspec,dts_obj->keyspec));
      reg->audit.objs_updated_by_audit++;
      /* Add an audit trail that this record is created */
      rwdts_member_data_object_add_audit_trail(mobj,
                                               RW_DTS_AUDIT_TRAIL_EVENT_RECOVERY,
                                               RW_DTS_AUDIT_TRAIL_ACTION_OBJ_UPDATE);
    }else if (!dts_obj->audit || (dts_obj->audit->missing_in_cache)){
            rwdts_shard_handle_t* shard = reg->shard;
      if (shard && shard->appdata) {
        ProtobufCMessage* msg = protobuf_c_message_duplicate(NULL, dts_obj->msg, dts_obj->msg->descriptor);
        rw_keyspec_path_t* keyspec = NULL;
        rw_status_t rs = RW_KEYSPEC_PATH_CREATE_DUP(dts_obj->keyspec, NULL , &keyspec, NULL);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        rwdts_shard_handle_appdata_create_element(shard, keyspec, msg);
      } else {
        // Means this object is missing in cache - update
        mobj = rwdts_member_data_init(reg, dts_obj->msg, dts_obj->keyspec, false, false);
        mobj->created_by_audit = 1;
        RW_ASSERT(dts_obj->key);
        RW_ASSERT(dts_obj->key_len > 0);
        HASH_FIND(hh_data, reg->obj_list,  mobj->key, mobj->key_len, in_cache_obj);
        if (in_cache_obj == NULL) {
          HASH_ADD_KEYPTR(hh_data, reg->obj_list, mobj->key, mobj->key_len, mobj);
          reg->stats.audit.num_objs_created++;
          reg->audit.objs_updated_by_audit++;
          /* Add an audit trail that this record is created */
          rwdts_member_data_object_add_audit_trail(mobj,
                                                   RW_DTS_AUDIT_TRAIL_EVENT_RECOVERY,
                                                   RW_DTS_AUDIT_TRAIL_ACTION_OBJ_CREATE);
        } else {
          protobuf_c_message_free_unpacked_usebody(&protobuf_c_default_instance, dts_obj->msg);
          dts_obj->msg = mobj->msg = NULL;
          rw_keyspec_path_free(dts_obj->keyspec, NULL);
          dts_obj->keyspec = mobj->keyspec = NULL;
        }
      }
    }
  }

  // Debug print cache objects before deletes occur
  RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(reg, "During reconciliation before deletes");


  rwdts_member_data_object_t *data = NULL, *tmpdata = NULL;
  HASH_ITER(hh_data, reg->obj_list, data, tmpdata) {
    if (data->audit) {
      RW_ASSERT(data->audit->missing_in_cache == 0);     // We can't have the missing_in_cache flag set for anything in cache
      RW_ASSERT(data->audit->mis_matched_contents == 0); // The last iteration should have fixed this
      if (data->audit->missing_in_dts) {
        // ATTN - No audit trail for deleted elements since they immediately go away
        HASH_DELETE(hh_data, reg->obj_list, data);
        rs = rwdts_member_data_deinit(reg->apih, data);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        reg->stats.audit.num_objs_deleted++;
      }
    }
  } /* HASH_ITER */

  // Debug print cache objects before deletes occur
  RWDTS_AUDIT_DEBUG_DUMP_CACHE_LIST(reg, "After reconciliation  incuding deletes");

  // Done with  cache reconcile -- trigger a fetch again
  //
  rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_FETCH, NULL);

  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_member_reg_audit_finish(rwdts_member_registration_t *reg)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  char  *ks_str = NULL;
  rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , rwdts_api_get_ypbc_schema(reg->apih), &ks_str);

  // Finish the audit and
  RWTRACE_INFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Finish audit  for regn id %d in state [%s] for registration %d ks[%s]",
               apih->client_path, reg->reg_id, rwdts_audit_state_str(reg->audit.audit_state), reg->reg_id, ks_str);

  RW_FREE(ks_str);
  ks_str = NULL;

  // record audit end time
  time(&reg->audit.audit_end);
  reg->audit.audit_stats.num_audits_succeeded++;

  // Invoke the audit done callback
  if (reg->audit.audit_done) {
    rwdts_audit_result_t *audit_result = rwdts_audit_result_new(reg->apih);
    audit_result->audit_status = reg->audit.audit_status;
    audit_result->audit_msg = strdup(reg->audit.audit_status_message);
    if (reg->reg_state != RWDTS_REG_DEL_PENDING){
      (reg->audit.audit_done)(reg->apih,
                              (rwdts_member_reg_handle_t)reg,
                              audit_result,
                              reg->audit.audit_done_ctx);
    }
    rwdts_audit_result_unref(audit_result);
  }

  rwdts_member_reg_audit_run(reg, RW_DTS_AUDIT_EVT_REPORT, NULL);
  return RW_STATUS_SUCCESS;
}

static inline void
rwdts_member_reg_audit_record_event(rwdts_member_registration_t *reg,
                                    RwDts__YangEnum__AuditEvt__E evt)
{
  if (reg->audit.n_audit_trace >= RWDTS_MAX_CAPTURED_AUDIT_EVENTS) {
    return;
  }
  reg->audit.audit_trace[reg->audit.n_audit_trace].state = reg->audit.audit_state;
  reg->audit.audit_trace[reg->audit.n_audit_trace].event = evt;
  reg->audit.n_audit_trace++;
}
#if RWDTS_ENABLE_AUDIT_DEBUG
static inline void
rwdts_member_reg_audit_print_audit_trace(rwdts_member_registration_t *reg)
{
  int i;
  for (i = 0; i < reg->audit.n_audit_trace; i++) {
    RWTRACE_INFO(reg->apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                 "%-30s %-30s\n",
                 rwdts_audit_state_str(reg->audit.audit_trace[i].state),
                 rwdts_audit_evt_str(reg->audit.audit_trace[i].event));
  }
  return;
}
#endif /* RWDTS_ENABLE_AUDIT_DEBUG */
static void
rwdts_member_reg_audit_run(rwdts_member_registration_t *reg,
                           RwDts__YangEnum__AuditEvt__E evt,
                           void*                        ctx)
{
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_api_t*   apih  = reg->apih;
  rwdts_audit_t* audit = &reg->audit;
  rw_status_t    rs    = RW_STATUS_SUCCESS;
  reg->audit.fetch_failed = false;
  rwdts_member_reg_audit_record_event(reg, evt);

  switch(audit->audit_state) {
    case RW_DTS_AUDIT_STATE_FAILED:
      if (evt == RW_DTS_AUDIT_EVT_REPORT) {
        rwdts_member_reg_audit_report(reg);
        return;
      }
      // Fall thru
      //
    case RW_DTS_AUDIT_STATE_NULL:
    case RW_DTS_AUDIT_STATE_BEGIN:
      if (evt == RW_DTS_AUDIT_EVT_BEGIN_AUDIT) {
        // Transition to the fetching phase and wait for the fetch to proceed
        audit->audit_state = RW_DTS_AUDIT_STATE_FETCH;
        rs = rwdts_member_reg_audit_fetch(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed to fetch records for registration id - %d in state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else if (evt == RW_DTS_AUDIT_EVT_COMPLETE) {
        // Transition to the fetching phase and wait for the fetch to proceed
        audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
        audit->audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
        rs = rwdts_member_reg_audit_finish(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit finish failed for registration id - %d in state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "%s: Audit failed - invalid event [%s] in  state [%s]",
                     apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
        goto audit_failure;
      }
      // break
      break;

    case RW_DTS_AUDIT_STATE_FETCH:
      if (evt == RW_DTS_AUDIT_EVT_FETCH_DONE) {
        // Okay we got the records from the fetch - Now proceed with the audit
        // Transition to the fetching phase and wait for the fetch to proceed
        audit->audit_state = RW_DTS_AUDIT_STATE_COMPARE;
        rs = rwdts_member_reg_audit_compare_datasets(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed to perform comparison for registration id - %d in state  %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else if (evt == RW_DTS_AUDIT_EVT_FETCH_FAILED) {
        // We failed fetch -- Retry until the max retries are set
        audit->audit_state = RW_DTS_AUDIT_STATE_FETCH;
        rs = rwdts_member_reg_audit_fetch(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed to perform fetch records for registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else if (evt == RW_DTS_AUDIT_EVT_COMPLETE) {
        // We failed fetch -- Retry until the max retries are set
        audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
        reg->audit.audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
        rs = rwdts_member_reg_audit_finish(reg);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
      } else {
        if (!reg->silent_retry) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed - invalid event [%s] in  state [%s]",
                       apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
        }
        goto audit_failure;
      }
      break;

    case RW_DTS_AUDIT_STATE_COMPARE:
      if (evt == RW_DTS_AUDIT_EVT_FETCH) {
        // Transition to the fetching phase and wait for the fetch to proceed
        audit->audit_state = RW_DTS_AUDIT_STATE_FETCH;
        rs = rwdts_member_reg_audit_fetch(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed to perform fetch records for registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else if (evt == RW_DTS_AUDIT_EVT_COMPARISON_DONE) {
        if (audit->audit_action == RWDTS_AUDIT_ACTION_REPORT_ONLY) {
          // Transition to the app audit phase
          audit->audit_status = RWDTS_AUDIT_STATUS_SUCCESS; // Mark the audit as success -- The comparison was successfull
          audit->audit_state = RW_DTS_AUDIT_STATE_APP_AUDIT;
          rs = rwdts_member_reg_app_audit(reg);
          if (rs != RW_STATUS_SUCCESS) {
            RWTRACE_CRIT(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: Audit failed to perform applicaton audit for registration id - %d, state %s",
                         apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
            goto audit_failure;
          }
        } else if (audit->audit_action == RWDTS_AUDIT_ACTION_RECOVER) {
          // Transition to the Done phase
          audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
          reg->audit.audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
          rs = rwdts_member_reg_audit_finish(reg);
          if (rs != RW_STATUS_SUCCESS) {
            RWTRACE_CRIT(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: Audit reconcile finish failed for registration id - %d, state %s",
                         apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
            goto audit_failure;
          }
        } else {
          RW_ASSERT(audit->audit_action == RWDTS_AUDIT_ACTION_RECONCILE);
          // Transition to the reconcile state and complete reconciliation
          audit->audit_state = RW_DTS_AUDIT_STATE_RECONCILE;
          rs = rwdts_member_reg_reconcile(reg);
          if (rs != RW_STATUS_SUCCESS) {
            RWTRACE_CRIT(apih->rwtrace_instance,
                          RWTRACE_CATEGORY_RWTASKLET,
                        "%s: Audit failed to perform reonciliation for registration id - %d, state %s",
                         apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
            goto audit_failure;
          }
        }
      } else if (evt == RW_DTS_AUDIT_EVT_COMPARISON_FAILED) {
        audit->found_errors = 1;
        if (audit->audit_action == RWDTS_AUDIT_ACTION_RECONCILE ||
            audit->audit_action == RWDTS_AUDIT_ACTION_RECOVER) {
          audit->audit_state = RW_DTS_AUDIT_STATE_RECONCILE;
          rs = rwdts_member_reg_reconcile_cache(reg);
          if (rs != RW_STATUS_SUCCESS) {
            RWTRACE_CRIT(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: Audit failed to reconiliation of cache for registration id - %d, state %s",
                         apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
            goto audit_failure;
          }
        } else { // Audit action is RWDTS_AUDIT_ACTION_REPORT_ONLY
          audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
          reg->audit.audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
          rs = rwdts_member_reg_audit_finish(reg);
          if (rs != RW_STATUS_SUCCESS) {
            RWTRACE_CRIT(apih->rwtrace_instance,
                         RWTRACE_CATEGORY_RWTASKLET,
                         "%s: Audit finish failed - audit action is RWDTS_AUDIT_ACTION_REPORT_ONLY event [%s] in  state [%s]",
                          apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
            goto audit_failure;
          }
        }
      } else if (evt == RW_DTS_AUDIT_EVT_CHANGES_OCCURED) {
        reg->audit.fetch_invalidated = 0;
        audit->audit_state = RW_DTS_AUDIT_STATE_FETCH;
        rs = rwdts_member_reg_audit_fetch(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "%s: Audit failed - invalid event [%s] in  state [%s]",
                      apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
        goto audit_failure;
      }
      break;

    case RW_DTS_AUDIT_STATE_APP_AUDIT:
      if (evt == RW_DTS_AUDIT_EVT_APP_AUDIT_DONE) {
        audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
        reg->audit.audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
        rs = rwdts_member_reg_audit_finish(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit finish failed for registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "%s: Audit failed - invalid event [%s] in  state [%s]",
                     apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));

        goto audit_failure;
      }
      break;

    case RW_DTS_AUDIT_STATE_RECONCILE:
      if (evt == RW_DTS_AUDIT_EVT_FETCH) {
        // Transition to the fetching phase and wait for the fetch to proceed
        audit->audit_state = RW_DTS_AUDIT_STATE_FETCH;
        rs = rwdts_member_reg_audit_fetch(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit failed to perform fetch records for registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else if (evt == RW_DTS_AUDIT_EVT_RECONCILE_DONE) {
        audit->audit_state = RW_DTS_AUDIT_STATE_COMPLETED;
        reg->audit.audit_status = RWDTS_AUDIT_STATUS_SUCCESS;
        rs = rwdts_member_reg_audit_finish(reg);
        if (rs != RW_STATUS_SUCCESS) {
          RWTRACE_CRIT(apih->rwtrace_instance,
                       RWTRACE_CATEGORY_RWTASKLET,
                       "%s: Audit reconcile finish failed for registration id - %d, state %s",
                       apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
          goto audit_failure;
        }
      } else {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "%s: Audit failed - invalid event [%s] in  state [%s]",
                     apih->client_path, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
        goto audit_failure;
      }
      break;
    case RW_DTS_AUDIT_STATE_COMPLETED:
      if (evt == RW_DTS_AUDIT_EVT_BEGIN_AUDIT) {
        RWTRACE_DEBUG(apih->rwtrace_instance,
                      RWTRACE_CATEGORY_RWTASKLET,
                      "%s: Begin audit  for regn id %d in state [%s]",
                      apih->client_path, reg->reg_id, rwdts_audit_state_str(audit->audit_state));
        audit->audit_state = RW_DTS_AUDIT_STATE_BEGIN;
      } else if (evt == RW_DTS_AUDIT_EVT_REPORT) {
        rwdts_member_reg_audit_report(reg);
      } else if ((evt == RW_DTS_AUDIT_EVT_FETCH_FAILED) ||
                (evt == RW_DTS_AUDIT_EVT_FETCH_DONE)) {
        /* Do nothing. Ideally we should not be here. As per design, there is
         * a possibilty of multiple READ queries triggered for the same audit.
         * If we are here, the audit is completed and any failures of same
         * READ should be ignored. Ideally, multiple READ triggers should be
         * avoided in this case as this messes up the FSM. */
      } else {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "%s: Audit failed - reg_id %d str %s invalid event [%s] in  state [%s]",
                     apih->client_path, reg->reg_id, reg->keystr, rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
        goto audit_failure;
      }
      break;
    default:
      break;
  } /* switch (audit->audit_state) */

  return;

audit_failure:
  RW_ASSERT(audit);
  if (!reg->silent_retry) {
    RWTRACE_CRIT(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "%s: Audit failed reg_id %d str %s- invalid event [%s] in  state [%s]",
               apih->client_path,
               reg->reg_id, reg->keystr,
               rwdts_audit_evt_str(evt), rwdts_audit_state_str(audit->audit_state));
  }
  audit->audit_state = RW_DTS_AUDIT_STATE_FAILED;
  audit->audit_status = RWDTS_AUDIT_STATUS_FAILURE;
  rwdts_member_reg_audit_finish(reg);
  return;
} /* rwdts_member_reg_audit_run */

rw_status_t
rwdts_audit_deinit(rwdts_audit_t *audit)
{
  int i;

  for (i = 0;  i < audit->audit_issues.errs_ct; i++) {
    RW_FREE(audit->audit_issues.errs[i].str);
    audit->audit_issues.errs[i].str = NULL;
  }

  audit->audit_issues.errs_ct = 0;
  RW_FREE(audit->audit_issues.errs);
  audit->audit_issues.errs = NULL;

  return RW_STATUS_SUCCESS;
}

void
rwdts_audit_init(rwdts_audit_t *audit,
                 rwdts_audit_done_cb_t audit_done,
                 void *user_data,
                 rwdts_audit_action_t action)
{
  rwdts_audit_deinit(audit);
  audit->objs_missing_in_cache=0;
  audit->objs_missing_in_dts=0;
  audit->objs_mis_matched_contents=0;
  audit->objs_matched=0;
  audit->mismatched_count=0;
  audit->n_audit_trace = 0;
  audit->objs_in_cache = 0;
  audit->objs_from_dts = 0;
  audit->objs_updated_by_audit = 0;
  audit->n_audit_trace = 0;
  audit->audit_serial++;
  audit->found_errors = 0;

  audit->audit_state = RW_DTS_AUDIT_STATE_NULL;
  audit->audit_status = RW_DTS_AUDIT_STATUS_NULL;
  audit->audit_action = action;

  audit->audit_done = audit_done;
  audit->audit_done_ctx =  user_data;

  audit->fetch_attempts = 0;

  return;
}

static RwDts__YangEnum__AuditStatus__E
rwdts_convert_audit_status_to_yang (rwdts_audit_status_t status)
{
  switch (status) {
    case RWDTS_AUDIT_STATUS_SUCCESS:
      return RW_DTS_AUDIT_STATUS_SUCCESS;

    case RWDTS_AUDIT_STATUS_FAILURE:
      return RW_DTS_AUDIT_STATUS_FAILURE;

    case RWDTS_AUDIT_STATUS_ABORTED:
      return RW_DTS_AUDIT_STATUS_ABORTED;

    default:
      break;
  }

  return RWDTS_AUDIT_STATUS_NULL;
}

static rwdts_audit_action_t
rwdts_convert_yang_audit_action (RwDts__YangEnum__AuditAction__E action)
{
  switch (action) {
    case RW_DTS_AUDIT_ACTION_REPORT_ONLY:
      return RWDTS_AUDIT_ACTION_REPORT_ONLY;

    case RW_DTS_AUDIT_ACTION_RECONCILE:
      return RWDTS_AUDIT_ACTION_RECONCILE;

    case RW_DTS_AUDIT_ACTION_RECOVER:
      return RWDTS_AUDIT_ACTION_RECOVER;

    default:
      RW_CRASH();
  }

  /* Should not reach here */
  return RW_DTS_AUDIT_ACTION_REPORT_ONLY;
}

/*
 * public API --  Adds an issue to an audit running on a registration
 */

void
rwdts_member_reg_handle_audit_add_issue(rwdts_member_reg_handle_t    regh,
                                        rw_status_t                  rs,
                                        const char*                  errstr)
{
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  rwdts_api_t*   apih  = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (RWDTS_MAX_AUDIT_ISSUES_TRACKED >= reg->audit.audit_issues.errs_ct) {
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Audit add issue failed-The number of issues tracked[%d] exceeds limit[%d]",
                 apih->client_path, reg->audit.audit_issues.errs_ct,  RWDTS_MAX_AUDIT_ISSUES_TRACKED);
  }
  int idx = reg->audit.audit_issues.errs_ct++;
  reg->audit.audit_issues.errs = realloc(reg->audit.audit_issues.errs,
                                         (reg->audit.audit_issues.errs_ct  *
                                          sizeof(reg->audit.audit_issues.errs[0])));
  reg->audit.audit_issues.errs[idx].str    = strdup(errstr);
  reg->audit.audit_issues.errs[idx].rs     = rs;
  reg->audit.audit_issues.errs[idx].corrid = 0;

  RWTRACE_WARN(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "rwdts audit issue code [%d] str[%s] \n", rs, errstr);

  return;
}

static void
rwdts_member_send_audit_rpc_response(rwdts_audit_t*               audit,
                                     const rwdts_xact_info_t*     xact_info,
                                     rwdts_member_registration_t* reg)
{
  rwdts_member_query_rsp_t rsp;
  RWPB_T_MSG(RwDts_output_StartDts) audit_output;
  RWPB_F_MSG_INIT(RwDts_output_StartDts,&audit_output);
  ProtobufCMessage *output_msgs[1];

  RW_ASSERT(xact_info);
  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwDts_output_StartDts);
  rsp.msgs = output_msgs;
  rsp.msgs[0] = (ProtobufCMessage *)&audit_output;
  rsp.n_msgs = 1;
  rsp.ks = ks;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  /* Audit related status */
  audit_output.has_audit_status = TRUE;
  audit_output.audit_status.has_state = TRUE;
  audit_output.audit_status.state = audit->audit_state;
  audit_output.audit_status.has_status = TRUE;
  audit_output.audit_status.status = rwdts_convert_audit_status_to_yang(audit->audit_status);
  audit_output.audit_status.has_status_message = TRUE;
  strncpy(audit_output.audit_status.status_message,
          audit->audit_status_message,
          sizeof(audit_output.audit_status.status_message));

  RWPB_M_MSG_DECL_INIT(RwDts_output_StartDts_AuditSummary, audit_summary);
  RWPB_T_MSG(RwDts_output_StartDts_AuditSummary) *audit_summary_p;

  audit_summary_p = &audit_summary;

  audit_output.audit_summary = &audit_summary_p;
  audit_output.n_audit_summary = 1;

  rwdts_member_fill_audit_summary(xact_info->apih,
                                  audit_output.audit_summary[0],
                                  reg);

  rw_status_t rs_status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(rs_status == RW_STATUS_SUCCESS);
  protobuf_c_message_free_unpacked_usebody(NULL, &audit_summary.base);

  return;
}


static void
rwdts_member_audit_finished(rwdts_api_t*                apih,
                            rwdts_member_reg_handle_t   regh,
                            const rwdts_audit_result_t* audit_result,
                            const void*                 user_data)
{
  rwdts_xact_info_t *xact_info = (rwdts_xact_info_t*)user_data;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  if (reg) {

    rwdts_member_send_audit_rpc_response(&reg->audit, xact_info, reg);
  } else {
    rwdts_member_send_audit_rpc_response(&apih->audit, xact_info, reg);
  }
  rwdts_xact_info_unref(xact_info);

  return;
}
rwdts_member_rsp_code_t
rwdts_member_start_audit(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t* key,
                         const ProtobufCMessage*  msg,
                         uint32_t                 credits,
                         void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL){
    return RWDTS_ACTION_NA;
  }
  rwdts_api_t *apih = (rwdts_api_t*)xact_info->ud;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWPB_T_MSG(RwDts_input_StartDts_Audit)* audit = NULL;
  rwdts_member_registration_t*            reg = NULL;
  rw_status_t                             rs = RW_STATUS_SUCCESS;
  bool                                    all = false;
  rwdts_audit_action_t                    audit_action = RWDTS_AUDIT_ACTION_REPORT_ONLY;
  int                                     i = 0;
  bool                                    member_found = false;

  RW_DEBUG_ASSERT(action != RWDTS_QUERY_INVALID);
  audit =  (RWPB_T_MSG(RwDts_input_StartDts_Audit) *)msg;

  /*
   * if no all or no member respond with nothing
   */
  if (!audit->has_all && audit->n_member == 0) {
    return RWDTS_ACTION_NA;
  }

  /*
   * if audit has all then count this member ans set all to true
   */
  if (audit->has_all) {
    member_found = true;
    all = true;
  }

  // Check if the name is present if so whether they macth
  for (i = 0; i < audit->n_member; i++) {
    if (strstr(apih->client_path, audit->member[0].name)) {
      member_found = true;
      all = true;
      if (audit->member[i].has_registration_id) {
        rs = RW_SKLIST_LOOKUP_BY_KEY(&(apih->reg_list),
                                     &audit->member[i].registration_id,
                                     (void *)&reg);
        if (rs !=  RW_STATUS_SUCCESS || reg == NULL) {
          return RWDTS_ACTION_NA;
        }
        all = false;
      }
    }
  }

  if (audit->has_action) {
    audit_action = rwdts_convert_yang_audit_action(audit->action);
  }

  /*
   * if this member is not in the RPC input return NA
   */
  if (!member_found) {
    return RWDTS_ACTION_NA;
  }

  /*
   * If all is set then do the audit and audit action for every registration.
   */
  rwdts_xact_info_t *xact_info_dup = rwdts_xact_info_ref(xact_info);
  if (true == all) {
    rs = rwdts_api_audit(apih,
                         audit_action,
                         rwdts_member_audit_finished,
                         (void*)xact_info_dup);
  } else {
    RW_ASSERT(reg);
    rs = rwdts_member_reg_handle_audit((rwdts_member_reg_handle_t)reg,
                                       audit_action,
                                       rwdts_member_audit_finished,
                                       (void*)xact_info_dup);
  }

  if (rs != RW_STATUS_SUCCESS) {
   RWTRACE_CRIT(apih->rwtrace_instance,
                RWTRACE_CATEGORY_RWTASKLET,
                   "%s: Begining audit failed  for registration id[%d]",
                   apih->client_path, reg?reg->reg_id:0);
   return RWDTS_ACTION_NOT_OK;
  } else  {
    return RWDTS_ACTION_ASYNC;
  }

  return RWDTS_ACTION_OK;
}

struct audit_track_s {
  int num_pending_audit;
};

static void
rwdts_reg_audit_cb(rwdts_api_t*                apih,
                   rwdts_member_reg_handle_t   regh,
                   const rwdts_audit_result_t* audit_result,
                   const void*                 user_data)
{
  struct audit_track_s *audit_track = (struct audit_track_s*)user_data;
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;

  RW_ASSERT(audit_track);
  RW_ASSERT(audit_track->num_pending_audit > 0);
  audit_track->num_pending_audit--;

  // Mark the autdit status as failure if one of the registration audit failed
  if (audit_result->audit_status !=  RWDTS_AUDIT_STATUS_SUCCESS) {
    apih->audit.audit_status  = audit_result->audit_status;
  }

  if (audit_track->num_pending_audit == 0) {
    if (apih->audit.audit_done) {
      rwdts_audit_result_t *audit_result = rwdts_audit_result_new(reg->apih);
      audit_result->audit_status = reg->audit.audit_status;
      audit_result->audit_msg = strdup(reg->audit.audit_status_message);
      (apih->audit.audit_done)(reg->apih,
                               NULL,
                               audit_result,
                               apih->audit.audit_done_ctx);
      rwdts_audit_result_unref(audit_result);
    }
    // Free the user data
    RW_FREE(audit_track);
  }
  return;
}

rw_status_t
rwdts_api_audit(rwdts_api_t*                    apih,
                rwdts_audit_action_t            audit_action,
                rwdts_audit_done_cb_t           audit_done,
                void*                           user_data)
{
  rwdts_member_registration_t* reg      = NULL;
  rw_status_t                  rs       = RW_STATUS_SUCCESS;
  rw_status_t                  ret_code = RW_STATUS_SUCCESS;

  struct audit_track_s *audit_track =  RW_MALLOC0(sizeof(struct audit_track_s));
  RW_ASSERT(audit_track);

  audit_track->num_pending_audit = RW_SKLIST_LENGTH(&(apih->reg_list));
  rwdts_audit_init(&apih->audit, audit_done,  user_data, audit_action);

  reg = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (reg) {
    RWTRACE_INFO(apih->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET,
                  "%s: Begining audit for registration id %d",
                  apih->client_path, reg?0:reg->reg_id);
    rs = rwdts_member_reg_handle_audit((rwdts_member_reg_handle_t)reg,
                                        audit_action,
                                        rwdts_reg_audit_cb,
                                        audit_track);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    reg = RW_SKLIST_NEXT(reg, rwdts_member_registration_t, element);
  }
  return ret_code;
}


static void
rwdts_member_reg_audit_deinit_fetched_data(rwdts_member_registration_t *reg,
                                           rwdts_audit_t *audit)
{
  int i = 0;

  for (i = 0; i < audit->n_dts_data ; i++) {
    rwdts_member_data_object_t *dts_obj = audit->dts_data[i];
    rw_status_t rs = rwdts_member_data_deinit(reg->apih, dts_obj);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  // Free the audit fetched data array
  if (audit->dts_data) {
    DTS_APIH_FREE_TYPE(reg->apih, RW_DTS_DTS_MEMORY_TYPE_DATA_OBJECT,
                       audit->dts_data, rwdts_member_data_object_t*);
    audit->dts_data = NULL;
  }
  audit->n_dts_data = 0;
  return;
}

static void
rwdts_member_fill_audit_summary(rwdts_api_t*                    apih,
                                RWPB_T_MSG(RwDts_AuditSummary)* summary,
                                rwdts_member_registration_t*    reg_only)
{

  RW_ASSERT(summary != NULL);
  if (summary == NULL) {
    return;
  }

  rwdts_member_registration_t *reg;

  summary->member_name = strdup(apih->client_path);
  summary->has_reg_failed_audit = 1;
  summary->has_reg_succeeded_audit = 1;

  reg = reg_only?reg_only:RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while(reg) {
    if (reg->audit.found_errors)  {
      // There was discrepancies foud during audit which could
      // have got fixed or still has the issues -- Add them to the summary
      summary->failed_reg = (RWPB_T_MSG(RwDts_AuditSummaryFailedReg)**)realloc(summary->failed_reg,
                             (summary->n_failed_reg+1) * sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedReg)*));

      RWPB_T_MSG(RwDts_AuditSummaryFailedReg) *failed_reg =
      summary->failed_reg[summary->n_failed_reg] = (RWPB_T_MSG(RwDts_AuditSummaryFailedReg)*)
                                                   RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedReg)));
      RWPB_F_MSG_INIT(RwDts_AuditSummaryFailedReg, failed_reg);
      failed_reg->reg_id = reg->reg_id;
      failed_reg->has_reg_id = 1;
      rw_keyspec_path_get_new_print_buffer(reg->keyspec,
                                           NULL,
                                           rwdts_api_get_ypbc_schema(apih),
                                           &failed_reg->reg_key);


      /* Iterate through the cached object list and fill in the response */

      rwdts_member_data_object_t *data = NULL, *tmpdata = NULL;
      RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjCache) **obj_cache;
      obj_cache = failed_reg->obj_cache = (RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjCache)**)
        RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedReg)*) * HASH_CNT(hh_data, reg->obj_list));
      HASH_ITER(hh_data, reg->obj_list, data, tmpdata) {
        obj_cache[failed_reg->n_obj_cache] = (RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjCache)*)RW_MALLOC0(
                                             sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjCache)));
        RWPB_F_MSG_INIT(RwDts_AuditSummaryFailedRegObjCache, obj_cache[failed_reg->n_obj_cache]);
        rw_keyspec_path_get_new_print_buffer(data->keyspec,
                                             NULL,
                                             rwdts_api_get_ypbc_schema(apih),
                                             &obj_cache[failed_reg->n_obj_cache]->obj_key);
        rw_json_pbcm_to_json(data->msg,
                           data->msg->descriptor,
                           &obj_cache[failed_reg->n_obj_cache]->obj_contents);
        obj_cache[failed_reg->n_obj_cache]->n_audit_trail = data->n_audit_trail;
        obj_cache[failed_reg->n_obj_cache]->audit_trail = RW_MALLOC0(data->n_audit_trail*
          sizeof(RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail*));
        int j;
        for (j = 0; j < data->n_audit_trail; j++) {
          obj_cache[failed_reg->n_obj_cache]->audit_trail[j] =
            (RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail*)
            protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                         &data->audit_trail[j]->base,
                                         data->audit_trail[j]->base.descriptor);
        }
        failed_reg->n_obj_cache++;
      }

      /* Iterate through the fetched object list from DTS and fill in the response */
      int i;
      RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjDts) **obj_dts;
      obj_dts = failed_reg->obj_dts = (RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjDts)**)
        RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjDts)*) * reg->audit.n_dts_data);

      for (i = 0; i < reg->audit.n_dts_data; i++) {
        data = reg->audit.dts_data[i];
        obj_dts[failed_reg->n_obj_dts] = (RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjDts)*)RW_MALLOC0(
                                             sizeof(RWPB_T_MSG(RwDts_AuditSummaryFailedRegObjDts)));
        RWPB_F_MSG_INIT(RwDts_AuditSummaryFailedRegObjDts, obj_dts[failed_reg->n_obj_dts]);
        rw_keyspec_path_get_new_print_buffer(data->keyspec,
                                             NULL,
                                             rwdts_api_get_ypbc_schema(apih),
                                             &obj_dts[failed_reg->n_obj_dts]->obj_key);
        rw_json_pbcm_to_json(data->msg,
                           data->msg->descriptor,
                           &obj_dts[failed_reg->n_obj_dts]->obj_contents);
        obj_dts[failed_reg->n_obj_dts]->n_audit_trail = data->n_audit_trail;
        obj_dts[failed_reg->n_obj_dts]->audit_trail = RW_MALLOC0(data->n_audit_trail*
          sizeof(RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail*));
        int j;
        for (j = 0; j < data->n_audit_trail; j++) {
          obj_dts[failed_reg->n_obj_dts]->audit_trail[j] =
            (RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail*)
            protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                         &data->audit_trail[j]->base,
                                         data->audit_trail[j]->base.descriptor);
        }
        failed_reg->n_obj_dts++;
      }

      summary->n_failed_reg++;
      summary->reg_failed_audit++;
    } else {
      summary->reg_succeeded_audit++;
    }

    if (reg_only) break;

    reg = RW_SKLIST_NEXT(reg, rwdts_member_registration_t, element);
  }
  return;
}

rw_status_t
rwdts_member_reg_audit_invalidate_fetch_results(rwdts_member_registration_t *reg)
{
  reg->audit.fetch_invalidated = 1;
  return (RW_STATUS_SUCCESS);
}

rwdts_audit_status_t
rwdts_audit_result_get_audit_status(rwdts_audit_result_t *audit_result)
{
  return (audit_result->audit_status);
}

const char*
rwdts_audit_result_get_audit_msg(rwdts_audit_result_t *audit_result)
{
  return (audit_result->audit_msg[0] ? audit_result->audit_msg : NULL);
}
