/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwdts_api.h
 * @brief Core API for RW.DTS
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_API_H
#define __RWDTS_API_H

#include <libgen.h>
#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmemlog.h>
#include <rwmsg.h>
#include <rw_dts_int.pb-c.h>
#include <rw_sklist.h>
#include <rwdts_query_api.h>
#include <rwdts_member_api.h>
#include <uthash.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include "rw-dts-api-log.pb-c.h"
#include "rw-dts-router-log.pb-c.h"
#include "rw-dts.pb-c.h"
#include <rwlog.h>
#include <rw_json.h>
#include "rwdts_macros.h"
#include "rwdts_shard.h"

__BEGIN_DECLS

#define RWDTS_TIMEOUT_QUANTUM_MULTIPLE(apih, scale) ((apih)->timeout_base * (scale))
#define RWDTS_CONFIG_MONITOR_WARN_PERIOD_SCALE (0.1)
#define RWDTS_ASYNC_TIMEOUT_SCALE (5)

#define RWDTS_MEMBER_MAX_RETRIES_REG 2 /* determine the retry for registrations getting aborts*/
#define RWDTS_TIMEIT_THRESH_MS (5)

#define RWDTS_TIMEIT(xxx) ({						\
        int shown=FALSE;						\
	unsigned int cbms=0;						\
	struct timeval tv_begin, tv_end, tv_delta;			\
									\
	gettimeofday(&tv_begin, NULL);					\
									\
	{xxx;}								\
									\
	gettimeofday(&tv_end, NULL);					\
	timersub(&tv_end, &tv_begin, &tv_delta);			\
	cbms = (tv_delta.tv_sec * 1000 + tv_delta.tv_usec / 1000);	\
	if (cbms >= RWDTS_TIMEIT_THRESH_MS) {				\
	  fprintf(stderr, "!!%s...%s@%d took %ums\n", __func__, #xxx, __LINE__, cbms); \
shown=TRUE;								\
	}								\
(shown);								\
})


#define DTS_APIH_MALLOC(_apih, _type, _size) \
  RW_MALLOC((_size));                           \
  if (_apih)                                    \
    (_apih)->memory_stats[(_type)]->num_allocs++;

#define DTS_APIH_MALLOC0(_apih, _type, _size)   \
  RW_MALLOC0((_size));                          \
  if (_apih)                                    \
    (_apih)->memory_stats[(_type)]->num_allocs++;

#define DTS_APIH_MALLOC_TYPE(_apih, _type, _size, _cftype) \
  RW_MALLOC_TYPE((_size), _cftype);                      \
  if (_apih)                                             \
    (_apih)->memory_stats[(_type)]->num_allocs++;


#define DTS_APIH_MALLOC0_TYPE(_apih, _type, _size, _cftype) \
  RW_MALLOC0_TYPE((_size), _cftype);                      \
  if (_apih)                                              \
    (_apih)->memory_stats[(_type)]->num_allocs++;

#define DTS_APIH_FREE(_apih, _type, _data)      \
  if (_apih)                                     \
    (_apih)->memory_stats[(_type)]->num_frees++; \
  RW_FREE((_data));                             

#define DTS_APIH_FREE_TYPE(_apih, _type, _data, _cftype) \
  if (_apih)                                    \
    (_apih)->memory_stats[(_type)]->num_frees++; \
  RW_FREE_TYPE((_data), _cftype);                
   

#if 0
#define PRINT_STR(path, fmt, args...) \
        if (strstr(path, "DTSRouter")) \
    fprintf(stderr, "[%s:%d][[ %s ]]" fmt " **** AyYOoo pRiNTs*****\n", __func__, __LINE__, path, args);
#else
#define PRINT_STR(path, fmt, args...) \
        do {} while (0);
#endif

#define RWDTS_STR_HASH(t_str) ({\
  unsigned long hash = 5381;\
  int c;\
  char *str = (char *)(t_str);\
  while ((c = *str++)) {\
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */\
  }\
  hash;\
})
#define RWDTS_SHARDING

#define MAX_DTS_STATES_TRACE_SZ 10
#define RWDTS_SUB_CACHE_POPULATE_FLAG (RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE)
#define RWDTS_MAX_AUDIT_ISSUES_TRACKED 16
#define RWDTS_MAX_FETCH_ATTEMPTS       16
#define RWDTS_CP_MAX 31
#define RWDTS_XACT_LOG_STATE(_xact_,_state_,__event__) \
    (_xact_)->fsm_log[(_xact_)->curr_trace_index%MAX_TRACE_EVT].state = (_state_);\
    (_xact_)->fsm_log[(_xact_)->curr_trace_index%MAX_TRACE_EVT].evt = (__event__);\
    gettimeofday(&(_xact_)->fsm_log[(_xact_)->curr_trace_index%MAX_TRACE_EVT].time, NULL);\
    (_xact_)->curr_trace_index++;

//Add Timestamp
#define COPY_STATS(to_stats,to,from_stats,from)  (to_stats).to = (from_stats).from; \
    (to_stats).has_##to = (from_stats).from;

#define COPY_STATS_2(to_stats,from_stats,stat)  COPY_STATS((to_stats), stat, (from_stats), stat)
#define RWDTS_USE_POPULATE_CACHE 0
#define RWDTS_MEMBER_OBJ_MAX_AUDIT_TRAIL 16
#define RWDTS_MAX_INTERNAL_REG           16

#define RWDTS_ERRSTR_KEY_DUP "RWDTS: Error creating duplicate keyspec"
#define RWDTS_ERRSTR_KEY_MISMATCH "RWDTS: Error, keyspec mismatch"
#define RWDTS_ERRSTR_KEY_WILDCARDS "RWDTS: Error, keyspec has wildcards"
#define RWDTS_ERRSTR_KEY_BINPATH "RWDTS: Error in key binpath"
#define RWDTS_ERRSTR_KEY_APPEND "RWDTS: Error in key append path"
#define RWDTS_ERRSTR_KEY_XPATH "RWDTS: Error in getting keyspec from xpath"
#define RWDTS_ERRSTR_RSP_MERGE "RWDTS: Error during response merge"
#define RWDTS_ERRSTR_MSG_REROOT "RWDTS: Error during reroot"
#define RWDTS_ERRSTR_QUERY_ACTION "RWDTS: Error on process query action"
#define RWDTS_ERRSTR_MEMB_CREATE "RWDTS: Error creating member object"
#define RWDTS_ERRSTR_KEY_PATH "RWDTS: Error getting path element"

#define RWDTS_PBCM_HASHKEYLEN(pb_key_ptr, field) \
  (sizeof(*(pb_key_ptr)) - offsetof(typeof(*(pb_key_ptr)), field))

#define RWDTS_ADD_PBKEY_TO_HASH(hash_handle, table, pb_key_ptr, field, elem) {\
  int keylen = RWDTS_PBCM_HASHKEYLEN(pb_key_ptr, field); \
  HASH_ADD_KEYPTR(hash_handle, table, (&((pb_key_ptr)->field)), keylen, elem);\
}

#define RWDTS_FIND_WITH_PBKEY_IN_HASH(hash_handle, table, pb_key_ptr, field, elem) {\
  int keylen = RWDTS_PBCM_HASHKEYLEN(pb_key_ptr, field); \
  HASH_FIND(hash_handle, table, &((pb_key_ptr)->field), keylen, elem);\
}

#define RWDTS_API_VCS_INSTANCE_CHILD_STATE "D,/rw-base:vcs/instances/instance/child-n/publish-state"

extern ProtobufCInstance _rwdts_pbc_instance;

typedef struct rwdts_member_registration_s rwdts_member_registration_t;
typedef struct rwdts_xact_s rwdts_xact_t;
typedef struct rwdts_group_xact_s rwdts_group_xact_t;
typedef struct rwdts_member_data_elem_s rwdts_member_data_elem_t;
typedef struct rwdts_journal_s rwdts_journal_t;

typedef struct rwdts_xact_query_rsp_s rwdts_xact_query_rsp_t;
typedef struct rwdts_xact_query_pend_rsp_s rwdts_xact_query_pend_rsp_t;
typedef struct rwdts_credit_obj_s rwdts_credit_obj_t;
typedef struct rwdts_trace_filter_s rwdts_trace_filter_t;

typedef  struct {
  struct timeval tv;
  rwdts_state_t state;
}fsm_trace_state_t ;

typedef struct rwdts_member_cursor_impl_s {
  rwdts_member_cursor_t base; // This should be the first element in this structure
  // ATTN this will need to support different kind of keys once the key infra is in in -- for now assume  index
  // uint8_t *bin_key;
  // uint32_t bin_key_len;
  int position;
  rwdts_member_registration_t* reg;
  rwdts_xact_t *xact;
} rwdts_member_cursor_impl_t;

struct rwdts_member_data_object_audit_s {
  uint32_t missing_in_cache:1;
  uint32_t missing_in_dts:1;
  uint32_t mis_matched_contents:1;
  uint32_t _pad:29;
  // ATTN - How to capture mismatch details  TBD ?
};
typedef struct rwdts_member_data_object_audit_s rwdts_member_data_object_audit_t;

typedef struct rwdts_member_data_object_s {
  UT_hash_handle hh_data;
  uint32_t outboard:1;
  uint32_t delete_mark:1;
  uint32_t create_flag:1;
  uint32_t update_flag:1;
  uint32_t created_by_audit:1;
  uint32_t updated_by_audit:1;
  uint32_t replace_flag:1;
  uint32_t magic:7;
  uint32_t _pad:17;
  uint8_t *key;
  uint32_t key_len;
  uint32_t shard_id;
  uint32_t db_number;
  uint32_t serial_num;
  rwdts_kv_table_handle_t *kv_tab_handle;
  ProtobufCMessage *msg;
  rwdts_member_registration_t *reg;
  rw_keyspec_path_t *keyspec;
  rwdts_member_data_object_audit_t *audit;
  int n_audit_trail;
  RwDts__YangOutput__RwDts__StartDts__AuditSummary__FailedReg__ObjDts__AuditTrail **audit_trail;
} rwdts_member_data_object_t;

typedef struct rwdts_appconf_reg_s rwdts_appconf_reg_t;

typedef enum {
  RWDTS_REG_INIT = 0,
  RWDTS_REG_SENT_TO_ROUTER,
  RWDTS_REG_USABLE,
  RWDTS_REG_DEL_PENDING
} reg_state_t;

typedef struct rwdts_pub_identifier_s rwdts_pub_identifier_t;

struct rwdts_pub_identifier_s {
  uint64_t member_id; /* unique identifier for member */
  uint64_t router_id; /* unique identifier for router */
  uint64_t pub_id;    /* unique identifier for pub_id wthin member */
};

typedef struct rwdts_pub_serial_s rwdts_pub_serial_t;

struct rwdts_pub_serial_s {
  UT_hash_handle               hh;    /* hash handle for pub serials by id */
  rw_sklist_element_t          element;
  rwdts_pub_identifier_t       id;
  uint64_t                     serial;
  rwdts_member_registration_t* reg;
  uint32_t                     solict_adv_completed:1;
  uint32_t                     _pad:31;
};

typedef enum {
  RWDTS_ROLE_MEMBER,
  RWDTS_ROLE_ROUTER,
  RWDTS_ROLE_CLIENT
}rwdts_member_role_t;
typedef enum {
  RWDTS_XACT_LEVEL_SINGLERESULT,
  RWDTS_XACT_LEVEL_QUERY,
  RWDTS_XACT_LEVEL_XACT
}rwdts_xact_level_t;
typedef struct rwdts_member_reg_s rwdts_member_reg_t;

#define RWDTS_AUDIT_IS_IN_PROGRESS(_audit_) \
  ((_audit_)->audit_state >= RW_DTS_AUDIT_STATE_BEGIN && \
   (_audit_)->audit_state <= RW_DTS_AUDIT_STATE_FAILED)

#define RWDTS_AUDIT_IS_DONE(_audit_) \
  ((_audit_)->audit_state == RW_DTS_AUDIT_STATE_FAILED ||\
   (_audit_)->audit_state == RW_DTS_AUDIT_STATE_COMPLETED)

#define RWDTS_AUDIT_IN_INIT_STATE(_audit_) \
  ((_audit_)->audit_state == RW_DTS_AUDIT_STATE_NULL ||  RWDTS_AUDIT_IS_DONE(_audit_))


/*!
 *  The statistics assocated with a DTS audit
 */
typedef struct rwdts_audit_stats_s {
  uint32_t num_audits_started;
  uint32_t num_audits_succeeded;
  uint32_t num_audits_failed;
} rwdts_audit_stats_t;

struct rwdts_audit_issue_s {
  struct {
    uint64_t    corrid; //?? probably need a keyspec on each?
    rw_status_t rs;
    char*       str;
  } *errs;
  uint32_t      errs_ct;
};
typedef struct rwdts_audit_issue_s rwdts_audit_issue_t;
/*!
 *  A tracking structure associated with a DTS audit - There is one instance of this per registration
 */

// Maximum number of events captured
#define RWDTS_MAX_CAPTURED_AUDIT_EVENTS  12
#define RWDTS_MAX_REG_RECOVERY_AUDIT_ATTEMPTS 4

typedef struct rwdts_audit_fsm_trans_s {
  RwDts__YangEnum__AuditState__E state;
  RwDts__YangEnum__AuditEvt__E   event;
}  rwdts_audit_fsm_trans_t;


struct rwdts_audit_s {
  uint32_t                     found_errors:1;
  uint32_t                     _pad:31;
  uint32_t                     n_dts_data;
  rwdts_member_data_object_t** dts_data;
  char                         audit_status_message[128];
  RwDts__YangEnum__AuditState__E  audit_state;
  RwDts__YangEnum__AuditStatus__E audit_status;
  rwdts_audit_stats_t          audit_stats;
  rwdts_audit_action_t         audit_action;
  rwdts_audit_issue_t          audit_issues;
  rwdts_audit_done_cb_t        audit_done; // callback to be called on audit completion
  void*                        audit_done_ctx;
  uint32_t                     audit_serial;
  uint32_t                     n_audit_trace;
  time_t                       audit_begin;
  time_t                       audit_end;
  rwdts_audit_fsm_trans_t      audit_trace[RWDTS_MAX_CAPTURED_AUDIT_EVENTS];
  // has the fetch failed? reset when the FETCH_FAILED is dispatched to the FSM
  bool                         fetch_failed;
  bool                         fetch_invalidated;
  // Audit related counters
  uint32_t                     objs_in_cache;
  uint32_t                     objs_from_dts;
  uint32_t                     objs_missing_in_cache;
  uint32_t                     objs_missing_in_dts;
  uint32_t                     objs_mis_matched_contents;
  uint32_t                     objs_matched;
  uint32_t                     mismatched_count;
  uint32_t                     objs_updated_by_audit;
  uint32_t                     fetch_attempts;
  uint32_t                     recovery_audit_attempts;
  uint32_t                     fetch_cb_success;
  uint32_t                     fetch_cb_failure;
  uint32_t                     fetch_failure;
  rwsched_dispatch_source_t    timer;
};

typedef struct rwdts_audit_s rwdts_audit_t;

#define RWDTS_MAX_FSM_TRACE  16

typedef struct rwdts_member_registration_fsm_stat_s {
  uint32_t advise_solicit_req;
  uint32_t advise_solicit_rsp;
  uint32_t advises;
} rwdts_member_registration_fsm_stat_t;

typedef struct rwdts_member_registration_fsm_trace_s {
  RwDts__YangEnum__RegistrationState__E state;
  RwDts__YangEnum__RegistrationEvt__E   event;
} rwdts_member_registration_fsm_trace_t;

typedef struct rwdts_member_registration_fsm_s  {
  RwDts__YangEnum__RegistrationState__E state;
  uint32_t                              n_trace;
  rwdts_member_registration_fsm_trace_t trace[RWDTS_MAX_FSM_TRACE];
  rwdts_member_registration_fsm_stat_t  counters;
} rwdts_member_registration_fsm_t;

typedef enum {
  RWDTS_APPDATA_NULL = 0,
  RWDTS_APPDATA_TYPE_SAFE_MINIKEY = 1,
  RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY,
  RWDTS_APPDATA_TYPE_QUEUE_MINIKEY,
  RWDTS_APPDATA_TYPE_SAFE_KEYSPEC,
  RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC,
  RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC,
  RWDTS_APPDATA_TYPE_SAFE_PE,
  RWDTS_APPDATA_TYPE_UNSAFE_PE,
  RWDTS_APPDATA_TYPE_QUEUE_PE,
  RWDTS_APPDATA_TYPE_SAFE_MINIKEY_PC,
  RWDTS_APPDATA_TYPE_UNSAFE_MINIKEY_PC,
  RWDTS_APPDATA_TYPE_SAFE_KEYSPEC_PC,
  RWDTS_APPDATA_TYPE_UNSAFE_KEYSPEC_PC,
  RWDTS_APPDATA_TYPE_SAFE_PE_PC,
  RWDTS_APPDATA_TYPE_UNSAFE_PE_PC
} rwdts_appdata_type_t;

#ifndef __GI_SCANNER__

/*!
 * An opaque structure to pass the cursor info back and forth  between member and AppData API.
 */

struct rwdts_appdata_cursor_s {
};
#endif /* __GI_SCANNER__ */

typedef struct rwdts_appdata_cursor_s rwdts_appdata_cursor_t;

typedef struct rwdts_appdata_cursor_impl_s {
  rwdts_appdata_cursor_t base; // This should be the first element in this structure
  rw_keyspec_path_t* keyspec;
  rw_keyspec_entry_t* pathentry;
  rw_schema_minikey_t* minikey;
} rwdts_appdata_cursor_impl_t;

typedef struct {
  rwdts_appdata_cb_getnext_minikey getnext;
  rwdts_appdata_cb_safe_mk_rwref_take take;
  rwdts_appdata_cb_safe_mk_rwref_put put;
  rwdts_appdata_cb_safe_mk_create create;
  rwdts_appdata_cb_safe_mk_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_safe_minikey_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_minikey getnext;
  rwdts_appdata_cb_unsafe_mk_rwref_get get;
  rwdts_appdata_cb_unsafe_mk_create create;
  rwdts_appdata_cb_unsafe_mk_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_unsafe_minikey_cbset_t;  

typedef struct {
  rwdts_appdata_cb_getnext_minikey getnext;
  rwdts_appdata_cb_mk_copy_get copy;
  rwdts_appdata_cb_mk_pbdelta pbdelta;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_queue_minikey_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_keyspec getnext;
  rwdts_appdata_cb_safe_ks_rwref_take take;
  rwdts_appdata_cb_safe_ks_rwref_put put;
  rwdts_appdata_cb_safe_ks_create create;
  rwdts_appdata_cb_safe_ks_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_safe_keyspec_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_keyspec getnext;
  rwdts_appdata_cb_unsafe_ks_rwref_get get;
  rwdts_appdata_cb_unsafe_ks_create create;
  rwdts_appdata_cb_unsafe_ks_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_unsafe_keyspec_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_keyspec getnext;
  rwdts_appdata_cb_ks_copy_get copy;
  rwdts_appdata_cb_ks_pbdelta pbdelta;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_queue_keyspec_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_pe getnext;
  rwdts_appdata_cb_safe_pe_rwref_take take;
  rwdts_appdata_cb_safe_pe_rwref_put put;
  rwdts_appdata_cb_safe_pe_create create;
  rwdts_appdata_cb_safe_pe_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_safe_pe_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_pe getnext;
  rwdts_appdata_cb_unsafe_pe_rwref_get get;
  rwdts_appdata_cb_unsafe_pe_create create;
  rwdts_appdata_cb_unsafe_pe_delete delete_fn;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_unsafe_pe_cbset_t;

typedef struct {
  rwdts_appdata_cb_getnext_pe getnext;
  rwdts_appdata_cb_pe_copy_get copy;
  rwdts_appdata_cb_pe_pbdelta pbdelta;
  void *ctx;
  GDestroyNotify dtor;
} rwdts_appdata_queue_pe_cbset_t;

typedef struct rwdts_safe_put_data_s {
  uint32_t safe_id;
  rwdts_appdata_t* appdata;
  rwdts_shard_handle_t* shard;
  rw_keyspec_path_t* keyspec;
  rw_keyspec_entry_t* pe;
  rw_schema_minikey_t* mk;  
  ProtobufCMessage* out_msg;
  rwsched_dispatch_source_t safe_put_timer;
  rw_sklist_element_t element;
}rwdts_safe_put_data_t;

struct rwdts_appdata_s {
  union {
    rwdts_appdata_safe_minikey_cbset_t* sm_cb; /* Safe minikey callback set */
    rwdts_appdata_unsafe_minikey_cbset_t* um_cb; /* unsafe minikey callback set */
    rwdts_appdata_queue_minikey_cbset_t* qm_cb; /* queue minikey callback set */
    rwdts_appdata_safe_keyspec_cbset_t* sk_cb;
    rwdts_appdata_unsafe_keyspec_cbset_t* uk_cb;
    rwdts_appdata_queue_keyspec_cbset_t* qk_cb;
    rwdts_appdata_safe_pe_cbset_t* sp_cb;
    rwdts_appdata_unsafe_pe_cbset_t* up_cb;
    rwdts_appdata_queue_pe_cbset_t* qp_cb;
  };
  rwdts_appdata_type_t appdata_type;
  rw_sklist_t safe_data_list;
  uint32_t safe_data_id;
  uint32_t scratch_safe_id;
  rwdts_appdata_cursor_impl_t* cursor;
  int ref_cnt;
  bool installed;
};

/*
 *  A structure for DTS member callback registrations
 *  DTS members acting as subscribers use these structures to register callbacks for
 *  the keyspecs in which they are interested in
 *  DTS invokes the cb outine  everytime there is a change in the  data
 *  associated with the input keyspec.
 *
 */

struct rwdts_member_registration_s {
  rwdts_api_t*                      apih;           /*< DTS API handle */
  int                               ref_cnt;
  uint32_t                          reg_id;         /*< unique reg identifier */
  UT_hash_handle                    hh_reg;         /*< Hash by keyspec */
  rw_keyspec_path_t*                keyspec;        /*< keyspec associated with the registration */
  char *                            keystr;         /*< pretty human readable key */
  uint8_t*                          ks_binpath;     /*< keyspec bin path */
  size_t                            ks_binpath_len; /*< keyspec bin path len */
  rwdts_member_event_cb_t           cb;             /*< callback routine - invoked by DTS on subscription events */
  uint32_t                          flags;          /*< Member registration flags --- publisher, subscriber or RPC? */
  const ProtobufCMessageDescriptor* desc;           /*< protobuf message descriptor asscoaited with this registration */
  rwdts_member_cursor_impl_t        **cursor;       /*< cursor for this registration */
  uint32_t                          n_cursor;
  uint32_t                          outstanding_requests;
  reg_state_t                       reg_state;
  rw_sklist_element_t               element;
  rwdts_audit_t                     audit;          /*< audit related data structure */

  RwDtsQuerySerial                  serial;

  rwdts_member_registration_fsm_t      fsm;
  rwdts_member_registration_fsm_stat_t fsm_stats;
  struct timeval tv_init;

  struct {
    rwdts_group_t *group;
    rw_dl_element_t elem;
  } group;

  uint32_t ingroup:1;
  uint32_t outboard:1;
  uint32_t inboard:1;
  uint32_t listy:1;
  uint32_t pending_delete:1;
  uint32_t pending_advise:1;
  uint32_t dts_internal:1;
  uint32_t in_sync:1;
  uint32_t gi_app:1;
  uint32_t pub_bef_reg_ready:4;
  uint32_t cach_at_reg_ready:4;
  uint32_t pub_aft_reg_ready:4;
  uint32_t reg_ready_done:1;
  uint32_t silent_retry:1; // try to overcome the commmit window
  uint32_t _pad:9;
  uint64_t tx_serialnum;
  int      retry_count;
  int      pend_outstanding;

  rwdts_pub_serial_t *pub_serials;
  rw_sklist_t         committed_serials;
  rwdts_pub_serial_t *expected_serials;
  uint64_t            highest_commit_serial;

  RwDts__YangData__RwDts__Dts__Member__State__Registration__Stats stats;

  rwdts_appconf_reg_t *appconf;

  rwdts_member_reg_t *memb_reg;

  rwdts_member_data_object_t *obj_list;

  rwdts_journal_t *journal;

  rwdts_kv_handle_t *kv_handle;

#ifdef RWDTS_SHARDING
  rwdts_shard_t *shard;
  rwdts_shard_flavor shard_flavor;
  union rwdts_shard_flavor_params_u params; 
  bool  has_index;
  int   index;
  rwdts_chunk_id_t chunk_id;
#endif

};

typedef struct rwdts_registration_args_s {
   rwdts_xact_t*                     xact;
   rwdts_api_t*                      apih;
   rwdts_group_t*                    group;
   const rw_keyspec_path_t*          keyspec;
   const rwdts_member_event_cb_t*    cb;
   const ProtobufCMessageDescriptor* desc;
   uint32_t                          flags;
   const rwdts_shard_info_detail_t*  shard_detail;
} rwdts_registration_args_t;

/*
 * Internal srtucture in API to hold the API client side details
 */

typedef struct rwdts_api_client_s {
  rwsched_dispatch_queue_t rwq;
  rwmsg_endpoint_t*        ep;  // Client endpoint
  rwmsg_clichan_t*         cc;  // Client channnel
  RWDtsQueryRouter_Client  service;
  RWDtsMemberRouter_Client mbr_service;
  rwmsg_destination_t*     dest;
  rwmsg_request_t*         rwreq;
} rwdts_api_client_t;


/*
 * Internal srtucture in API to hold the API server side details
 */
typedef struct rwdts_api_server_s {
  rwsched_dispatch_queue_t rwq;
  rwmsg_endpoint_t *ep;  // Server endpoint
  rwmsg_srvchan_t  *sc;  // Server channnel
  RWDtsMember_Service service;
  rwmsg_destination_t *dest;
} rwdts_api_server_t;

typedef struct rwdts_api_reg_info_s {
   uint32_t reg_id;
   uint32_t flags;
   rw_keyspec_path_t *keyspec;
   char *keystr;
   bool update; /*Will be set if this is a new registration or the registration is getting
                     updated in the router*/
   rw_sklist_element_t element;
} rwdts_api_reg_info_t;

typedef struct rwdts_api_reg_sent_list_s {
  struct timeval tv_sent;
  uint32_t *reg_id;
  uint32_t reg_count;
  rwdts_api_t *apih;
} rwdts_api_reg_sent_list_t;

typedef struct rwdts_api_dereg_path_info_s {
  char            *path;
  rwdts_api_t     *apih;
  vcs_recovery_type recovery_action;
  UT_hash_handle  hh_dereg;
} rwdts_api_dereg_path_info_t;

typedef enum rwdts_credit_status_e {
  RWDTS_CREDIT_STATUS_LWM = 0,
  RWDTS_CREDIT_STATUS_HWM
}rwdts_credit_status_t;

typedef enum rwdts_return_status_e {
  RWDTS_RETURN_SUCCESS,
  RWDTS_RETURN_KEYSPEC_ERROR,
  RWDTS_RETURN_FAILURE
}rwdts_return_status_t;

struct rwdts_credit_stats {
  uint64_t alloc_called;
  uint64_t free_called;
  uint64_t hwm_reached;
  uint64_t lwm_reached;
  uint64_t ooc;
  uint64_t invalid_op;
};

struct rwdts_credit_obj_s {
  uint32_t credits_total;
  uint32_t credits_remaining;
  uint32_t credit_alloc_rate;
  rwdts_credit_status_t credit_status;
  struct rwdts_credit_stats stats;
};

typedef struct rwdts_trace_filter_s rwdts_trace_filter_t;

struct rwdts_trace_filter_s {
  rw_keyspec_path_t* path;
  uint32_t id;
  bool print;
  bool break_start;
  bool break_prepare;
  bool break_end;
};

typedef struct rwdts_app_addr_res_s rwdts_app_addr_res_t;

struct rwdts_app_addr_res_s {
  uint64_t *ptr;
  char*    api_name;
  UT_hash_handle hh;
};

struct rwdts_api_s {
  uint32_t flags;
  int ref_cnt;
  char *client_path;
  char *router_path;
  rwdts_api_client_t client;
  rwdts_api_server_t server;
  uint64_t xact_id;
  rw_sklist_t sent_reg_list;
  rw_sklist_t sent_dereg_list;
  rw_sklist_t reg_list;

  struct timeval tv_init;
  rwmemlog_instance_t *rwmemlog;

  struct rwdts_xact_s *xacts;
  //rwsched_dispatch_source_t xact_timer;
  rwtrace_ctx_t *rwtrace_instance;
  rwdts_state_t  dts_state;
  uint32_t reg_usable;
  uint32_t reg_cache_ct;
  uint32_t free_and_retry_count;

  /* Configuration handling for DTS member API itself */
  struct {
    struct rwdts_appconf_s *appconf;
    struct {
      rwdts_member_reg_handle_t reg;
      rwdts_trace_filter_t **filters;
      int filters_ct;
    } tracert;
    //... add other configured items here, same appconf group etc!
  } conf1;

  /* keyspec and shard registration with Router */
  rwdts_shard_info_detail_t **shard_reg; /* table of shards and mapping */
  void *fetch_callbk_fn;
  rwdts_shard_tab_t *shard_tab;
  uint32_t next_table_id;
  rwdts_shard_t *shard_tree;
  uint32_t next_shard_id;
  rwsched_dispatch_source_t reg_timer;

  rw_sklist_t  kv_table_handle_list;
  rwdts_kv_handle_t *handle;
  char **dereg_path;
  rwdts_api_dereg_path_info_t *path_dereg;
  uint32_t path_dereg_cnt;
  rwsched_dispatch_source_t deregp_timer; //this can be deleted. the vcs should add the member and the individual registrations should be added by the member..AKKI TBD
 
  rwsched_dispatch_source_t dereg_timer;

  rwsched_tasklet_ptr_t tasklet;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwsched_instance_ptr_t sched;
  rwlog_ctx_t *rwlog_instance;
  rwdts_credit_obj_t credits;

  uint64_t member_reg_id_assigned; // Allocate reg_handles from this .. initialized to zero

  rwdts_state_change_cb_t state_change; /// DTS state change

  uint32_t reg_outstanding:1;
  uint32_t reg_retry:7;
  uint32_t db_up:1;
  uint32_t dereg_end:1; /* Used for cleanup for GTEST */
  uint32_t dereg_outstanding:1;
  uint32_t dereg_retry:4;
  uint32_t own_log_instance:1;
  uint32_t connected_to_router:1;
  uint32_t deregp_outstanding:1;
  uint32_t deregp_retry:3;
  uint32_t strict_check:1;
  uint32_t print_error_logs:1;
  uint32_t _pad:9;

  struct {
    // Put all the debugs in one place?
    uint32_t on:1;
    uint32_t persistant:1;
    uint32_t _pad:30;
  }trace;
  rw_keyspec_instance_t ksi;


  struct {
    // Structure to hold some  stats
    uint64_t num_transactions;
    uint64_t num_advises;
    uint64_t num_regs_retran;
    uint64_t num_member_advise_rsp;
    uint64_t num_member_advise_aborted;
    uint64_t num_member_advise_failed;
    uint64_t num_member_advise_success;
    uint64_t num_prepare_frm_rtr;
    uint64_t num_pre_commit_frm_rtr;
    uint64_t num_commit_frm_rtr;
    uint64_t num_abort_frm_rtr;
    uint64_t num_sub_commit_frm_rtr;
    uint64_t num_sub_abort_frm_rtr;
    uint64_t num_xact_create_objects;
    uint64_t num_xact_update_objects;
    uint64_t num_xact_delete_objects;
    uint64_t num_committed_create_obj;
    uint64_t num_committed_update_obj;
    uint64_t num_committed_delete_obj;
    uint64_t num_registrations;
    uint64_t num_reg_updates;
    uint64_t num_deregistrations;
    uint64_t num_prepare_evt_init_state;
    uint64_t num_end_evt_init_state;
    uint64_t num_prepare_evt_prepare_state;
    uint64_t num_pcommit_evt_prepare_state;
    uint64_t num_abort_evt_prepare_state;
    uint64_t num_qrsp_evt_prepare_state;
    uint64_t num_end_evt_prepare_state;
    uint64_t num_prepare_evt_pcommit_state;
    uint64_t num_pcommit_evt_pcommit_state;
    uint64_t num_commit_evt_pcommit_state;
    uint64_t num_abort_evt_pcommit_state;
    uint64_t num_end_evt_commit_state;
    uint64_t num_end_evt_abort_state;
    uint64_t num_prepare_cb_exec;
    uint64_t num_commit_cb_exec;
    uint64_t num_pcommit_cb_exec;
    uint64_t num_abort_cb_exec;
    uint64_t num_end_exec;
    uint64_t total_nontrans_queries;
    uint64_t total_trans_queries;
    uint64_t num_async_response;
    uint64_t num_na_response;
    uint64_t num_ack_response;
    uint64_t num_internal_response;
    uint64_t num_nack_response;
    uint64_t num_query_response;
    uint64_t num_xact_rsp_dispatched;
    uint64_t member_reg_advise_sent;
    uint64_t member_reg_advise_done;
    uint64_t member_reg_update_advise_done;
    uint64_t member_reg_advise_bounced;
    uint64_t member_reg_update_advise_bounced;
    uint32_t client_query_bounced;
    uint64_t more_received;
    uint64_t sent_keep_alive;
    uint64_t sent_credits;
    uint64_t reroot_done;
    uint64_t num_notif_rsp_count;
  } stats;
  RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) payload_stats;
  RWPB_T_MSG(RwDts_data_Dts_Member_State_MemoryStats) *memory_stats[RW_DTS_DTS_MEMORY_TYPE_MAX+1];
  //5x3x3x31

  uint64_t client_idx;
  uint64_t router_idx;

  uint64_t tx_serialnum;

  /* Table of all appconf groups */
  struct rwdts_appconf_s **appconf;
  uint32_t appconf_len;

  /* Table of all registration groups */
  rwdts_group_t **group;
  uint32_t group_len;

  rwdts_app_addr_res_t* app_addr_res;

  const rw_yang_pb_schema_t *ypbc_schema; // the ypbc schema
  rwdts_member_reg_handle_t init_regidh;
  rwdts_member_reg_handle_t init_regkeyh;
  rwdts_member_reg_handle_t dts_regh[RWDTS_MAX_INTERNAL_REG];
  rwdts_member_reg_handle_t journal_regh;
  rwdts_member_reg_handle_t ha_uagent_state_regh;
  uint32_t dts_state_change_count;
  fsm_trace_state_t trace_dts_state[MAX_DTS_STATES_TRACE_SZ];
  rwdts_audit_t  audit;          /*< audit related data structure */
#ifdef RWDTS_SHARDING
  rwdts_shard_t *rootshard;
#endif

  /* Store the xpath reduction library instance */
  void *xpath_lib_inst;

  /*  GDestroyNotify callback */
  GDestroyNotify api_destroyed;

  uint64_t timeout_base;
};

/*
 * DTS FSM return value
 */

typedef enum {
  FSM_FINISHED = 2,
  FSM_OK = 1,
  FSM_FAILED = 0
} rwdts_fsm_status_t;

/*
 * (sub) transaction state
 */

typedef enum {
  RWDTS_MEMB_XACT_ST_INIT = 0,
  RWDTS_MEMB_XACT_ST_PREPARE,
  RWDTS_MEMB_XACT_ST_PRECOMMIT,
  RWDTS_MEMB_XACT_ST_COMMIT,
  RWDTS_MEMB_XACT_ST_COMMIT_RSP,
  RWDTS_MEMB_XACT_ST_ABORT,
  RWDTS_MEMB_XACT_ST_ABORT_RSP,
  RWDTS_MEMB_XACT_ST_END
} rwdts_member_xact_state_t;

/*
 * sub transaction events
 */

typedef enum {
  RWDTS_MEMB_XACT_EVT_PREPARE = 1,
  RWDTS_MEMB_XACT_QUERY_RSP,
  RWDTS_MEMB_XACT_EVT_PRECOMMIT,
  RWDTS_MEMB_XACT_EVT_COMMIT,
  RWDTS_MEMB_XACT_EVT_COMMIT_RSP,
  RWDTS_MEMB_XACT_EVT_ABORT,
  RWDTS_MEMB_XACT_EVT_ABORT_RSP,
  RWDTS_MEMB_XACT_EVT_END
} rwdts_member_xact_evt_t;

#define RWDTS_VALID_MEMB_XACT_EVT(evt) ( (evt) >= RWDTS_MEMB_XACT_EVT_PREPARE && (evt) <= RWDTS_MEMB_XACT_EVT_END)

typedef rwdts_fsm_status_t (*rwdts_fsm_transition_routine)(rwdts_xact_t*           xact,
                                                           rwdts_member_xact_evt_t evt,
                                                           const void*             ud);

typedef struct rwdts_xact_query_journal_s rwdts_xact_query_journal_t;
/*
 * structure used on the member side to maintain
 * incoming queries within a transaction
 */

struct rwdts_xact_query_s {
  uint32_t responded:1;
  uint32_t has_results:1;
  uint32_t pending_response:1;
  uint32_t _pad:29;
  UT_hash_handle hh;                /* hash handle for queries by queryid  */
  RWDtsQuery *query;
  rwdts_xact_t *xact;
  RWDtsEventRsp evtrsp;
  RWDtsQueryResult *qres;
  uint32_t db_number;
  uint32_t shard_id;
  uint32_t serial_num;
  rwdts_kv_table_handle_t *kv_tab_handle;
  rwmsg_request_t *rwreq;
  RWDtsXactResult_Closure clo;
  uint32_t exp_rsp_count;
  uint32_t credits;
  rwdts_xact_query_pend_rsp_t *xact_rsp_pending;
  rw_sklist_t reg_matches;
  int ref_cnt;
  UT_hash_handle hh_inflt_query;
  rwdts_xact_query_journal_t *inflt_links;
};

typedef struct rwdts_xact_query_s rwdts_xact_query_t;

typedef struct rwdts_commit_record_int_s rwdts_commit_record_int_t;

struct rwdts_reg_commit_record_s {
  uint32_t pre_committed:1;
  uint32_t committed:1;
  uint32_t _pad:30;
  UT_hash_handle hh;
  uint32_t reg_id;      /* Key for hashing */
  rwdts_member_registration_t* reg;
  rwdts_xact_t *xact;
  rw_sklist_element_t element;

  /* The following is a list of transient commit records maintained within the transaction
   * on a per registration basis.  These are replayed in the precommit/commit playbacks.
   */

  uint32_t  n_commits;
  uint32_t  size_commits;

  rwdts_commit_record_int_t **commits;

  /*
   * The following is the data set at the registration within the member.
   */

  rwdts_member_data_object_t *obj_list;

  rwdts_xact_info_t xact_info;
};

typedef struct rwdts_reg_commit_record_s rwdts_reg_commit_record_t;

struct rwdts_commit_record_int_s {
  rwdts_commit_record_t      crec; /// This should be the first element of this structure
  rwdts_reg_commit_record_t* reg_crec;
  rwdts_xact_t*              xact;
  RwDtsQuerySerial*          serial;
};

typedef struct rwdts_appconf_xact_s rwdts_appconf_xact_t;


rwdts_appconf_t*
rwdts_appconf_ref(rwdts_appconf_t *boxed);

void
rwdts_appconf_unref(rwdts_appconf_t *boxed);

/* Query error structure
 * This structure is used to return a single error of a query
 */
typedef struct rwdts_query_error_s {
  ProtobufCMessage*  error;
  rw_status_t        cause;
  rw_keyspec_path_t* keyspec;
  char*              key_xpath;
  char*              keystr;
  char*              errstr;
  rwdts_api_t*       apih;
  int                ref_cnt;
} rwdts_query_error_t;

rwdts_query_error_t *
rwdts_query_error_new (rwdts_api_t *apih);

rwdts_query_error_t*
rwdts_query_error_ref(rwdts_query_error_t *boxed);

void
rwdts_query_error_unref(rwdts_query_error_t *boxed);

void rwdts_member_notify_newblock(rwdts_xact_t *xact,
                                  rwdts_xact_block_t *block);

#define MAX_TRACE_EVT 20
#define MAX_TRACE_STR_LEN 64
#ifdef RWDTS_XACT_DEBUG_REF
typedef struct xact_ref_trace_s {
  char *fn;
  int  line;
  int ref;
} xact_ref_trace_t;
#endif

/* In-API transaction handle.  This is the same in the Query and
   Member side; notably Members can issue queries using arriving
   requests' xact handles, and those queries then become
   subtransactions.  */
struct rwdts_xact_s {
  uint32_t bounced:1;
  uint32_t trace:1;
  uint32_t solicit_rsp:1;
  uint32_t _pad:13;
  uint32_t req_out:16;

  int ref_cnt;
#ifdef RWDTS_XACT_REF_DEBUG_ENABLE
#define RWDTS_XACT_REF_DEBUG_CNT 1000
  struct ref_debug_s {
    char *fn;
    int line;
    int ref;
  } ref_debug[RWDTS_XACT_REF_DEBUG_CNT];
  int ref_indx;
#endif // RWDTS_XACT_REF_DEBUG_ENABLE
  uint32_t flags;
  GDestroyNotify gdestroynotify;

  UT_hash_handle hh;                /* hash handle for xacts by id (id in .xact.id) */
  rwdts_api_t *apih;
  rwdts_xact_t *parent;
  RWDtsXactID id;
  RWDtsXactID toplevelid;
  rwmemlog_buffer_t *rwml_buffer;

  struct rwdts_xact_block_s **blocks;
  uint32_t blocks_ct;
  uint32_t blocks_allocated;
  uint32_t rsp_ct;      // member-side responses
  struct rwdts_event_cb_s *cb;
  RWDtsXact *xact;
  RWDtsXactMainState status;
  RWDtsTraceroute *tr;
  RWDtsEventRsp evtrsp;
  rwdts_member_cursor_impl_t **cursor;
  uint32_t n_cursor;
  rw_sklist_t  reg_cc_list;

  unsigned int n_member_new_blocks;
  rwdts_xact_block_t **member_new_blocks;

  /*
   * the following struture is used to keep
   * track of the results handed out to the querier
   * and implement an internal curosor
   */
  struct {
    rw_dl_t               pending;
    rw_dl_t               returned;
    rw_dl_t               results;
    uint32_t              num_query_results;
    uint32_t              query_results_next_idx;
    rwdts_query_result_t* query_results;
    uint32_t              error_idx;
  } track;

  RWDtsXactResult*            xres;
  rw_dl_t                     children;
  rwdts_member_xact_state_t   mbr_state;
  struct rwdts_xact_query_s*  queries;
  rwdts_member_data_object_t* pub_obj_list;
  rwdts_member_data_object_t* sub_obj_list;

  uint32_t num_prepares_dispatched; /* Incremented for every prepare callback issued; */
  uint32_t num_responses_received;  /* Number of queries responded by the member */

  struct {
    rwdts_member_xact_state_t  state;
    rwdts_member_xact_evt_t    evt;
    struct timeval             time;
  }fsm_log[MAX_TRACE_EVT];
  unsigned char              curr_trace_index;
  rw_keyspec_instance_t      ksi;
  char*                      ksi_errstr;
  rwdts_reg_commit_record_t* reg_commits;
  rwdts_appconf_xact_t**     appconf;
  uint32_t                   appconf_len;
  uint8_t                    evt[RWDTS_CP_MAX ];
  uint8_t                    n_evt;
  

  rwdts_group_xact_t ** group;        /* (sparse) table of groups involved, indexed by id slots from member's groups[] */
  uint32_t group_len;                /* len of table */

  RWDtsErrorReport *err_report;
  rwsched_dispatch_source_t  getnext_timer;

  /* Store the pointer to xpath reduction pcb */
  void *redn_pcb;
#ifdef RWDTS_XACT_DEBUG_REF
  int ref_trace_indx;
  xact_ref_trace_t ref_trace[30];
#endif
  bool       print; // debug -- make this better TBD AKKI
  char       xact_id_str[128];
};

struct rwdts_scratch_s {
  int ref_cnt;
};

struct rwdts_xact_err_s {
  RWDtsErrorEntry *ent;
  rwdts_xact_t  *xact;
  RWDtsQuery *xquery;
};
typedef struct rwdts_xact_err_s rwdts_xact_err_t;

typedef enum rwdts_newblockadd_notify_e {
  RWDTS_NEWBLOCK_NONE = 0,
  RWDTS_NEWBLOCK_TO_NOTIFY,
  RWDTS_NEWBLOCK_NOTIFY,
  RWDTS_NEWBLOCK_NOTIFIED
} rwdts_newblockadd_notify_t;

struct rwdts_xact_block_s {
  RWDtsXactBlock subx;
  struct {
    // unsent, pending, done, abort, ???
    // traceroute and suchlike?
    // temporary data conversion state?
    rwdts_xact_state_t  state;
    int _pad;
  } subx_state;  /* Transactions internal state */
  struct {
    rwdts_query_event_cb_routine cb;
    const void *ud;
  } cb;
  bool responses_done;
  rwdts_xact_t *xact;
  int ref_cnt;
  GDestroyNotify gdestroynotify;
  bool exec;
  bool evtrsp_internal;
  rwdts_newblockadd_notify_t newblockadd_notify;
};

struct rwdts_member_data_elem_s {
  ProtobufCMessage *msg;
  rw_keyspec_path_t *keyspec;
  const ProtobufCMessageDescriptor *descr;
};

struct rwdts_xact_query_rsp_s {
  rw_dl_element_t elem;
  rwdts_xact_t *xact;
  rwdts_query_handle_t queryh;
  rwdts_member_query_rsp_t rsp;
};

struct rwdts_xact_query_pend_rsp_s {
  rw_dl_element_t elem;
  rwdts_xact_t *xact;
  rwdts_query_handle_t queryh;
  uint32_t n_rsp;
  rwdts_member_query_rsp_t **rsp;
};

struct rwdts_match_info_s {
  rw_keyspec_path_t*            ks;
  ProtobufCMessage*             msg;
  rwdts_member_registration_t*  reg;
  uint32_t                      matchid;
  RWDtsEventRsp                 evtrsp;
  RWDtsQuery*                   query;
  RWDtsEventRsp                 sent_evtrsp;
  rw_keyspec_path_t*            in_ks;
  rw_sklist_element_t           match_elt;
  void*                         getnext_ptr;
  rwdts_xact_info_t             xact_info; /* passed to caller, lifespan of prepare phase */
  rwsched_dispatch_source_t     prep_timer_cancel;
  int                           resp_done;
  int                           prepared;
};

typedef struct rwdts_match_info_s rwdts_match_info_t;

/* Struct for an appconf group.  This API just wraps around the registration group API. */
struct rwdts_appconf_s {
  rwdts_api_t *apih;
  rwdts_group_t *group;
  rwdts_appconf_cbset_t cb;
  int ref_cnt;
  int reg_pend;
  bool phase_complete;
  rwsched_dispatch_source_t regn_timer;
};

typedef struct rwdts_appconf_s rwdts_appconf_t;

void rwdts_appconf_group_destroy(rwdts_appconf_t *ac);

#define RW_DTS_XACT_BLOCK_ALLOC_CHUNK_SIZE 5
#define RW_DTS_QUERY_BLOCK_ALLOC_CHUNK_SIZE 5
#define RW_DTS_QUERY_MEMBER_COMMIT_ALLOC_CHUNK_SIZE 8

const char*  rwdts_xact_id_str(const RWDtsXactID *id, char *str, size_t str_size);
rw_status_t rwdts_member_service_init(struct rwdts_api_s *apih);
rw_status_t rwdts_member_service_deinit(struct rwdts_api_s *apih);
rwdts_xact_t *rwdts_member_xact_init(rwdts_api_t* apih, const RWDtsXact* xact);

void rwdts_respond_router(RWDtsXactResult_Closure clo,
                          rwdts_xact_t*           xact,
                          RWDtsEventRsp           evtrsp,
                          rwmsg_request_t*        rwreq,
                          bool                    immediate);

rwdts_xact_t *rwdts_lookup_xact_by_id(const rwdts_api_t* apih, const RWDtsXactID* id);

rwdts_xact_query_t*
rwdts_xact_find_query_by_id(const rwdts_xact_t *xact, uint32_t queryidx);

rw_status_t
rwdts_xact_add_query_internal(rwdts_xact_t *xact, const RWDtsQuery *query, rwdts_xact_query_t **query_out);

rw_status_t 
rwdts_xact_rmv_query(rwdts_xact_t *xact, rwdts_xact_query_t* xquery);
rwdts_xact_query_t*
rwdts_xact_query_ref(rwdts_xact_query_t *x, const char *file, int line);
void
rwdts_xact_query_unref(rwdts_xact_query_t *x, const char *file, int line);

void rwdts_member_xact_run(rwdts_xact_t*           xact,
                           rwdts_member_xact_evt_t evt,
                           const void*             ud);

rw_status_t
rwdts_member_find_matches(rwdts_api_t* apih,
                          RWDtsQuery*  query,
                          rw_sklist_t* matches);


const char* rwdts_evtrsp_to_str(RWDtsEventRsp evt);

const char* rwdts_evtrsp_to_str(RWDtsEventRsp evt);

rwdts_member_registration_t* rwdts_member_registration_init_local(rwdts_api_t*  apih,
                                                            const rw_keyspec_path_t*          ks,
                                                            rwdts_member_event_cb_t*          cb,
                                                            uint32_t                          flags,
                                                            const ProtobufCMessageDescriptor* desc);

rw_status_t rwdts_member_registration_deinit(rwdts_member_registration_t* reg);

rw_status_t rwdts_member_cursor_deinit(rwdts_member_cursor_t *cursor);

rw_status_t
rwdts_member_reg_commit_record_deinit(rwdts_api_t *apih,
                                      rwdts_reg_commit_record_t *reg_creci);

rw_status_t
rwdts_member_commit_record_deinit(rwdts_api_t *apih,rwdts_commit_record_int_t *creci);

rwdts_reg_commit_record_t*
rwdts_member_find_reg_commit_record(rwdts_xact_t*  xact, rwdts_member_registration_t* reg);

rwdts_commit_record_int_t*
rwdts_add_commit_record(rwdts_xact_t*                xact,
                        rwdts_member_registration_t* reg,
                        rw_keyspec_path_t*           keyspec,
                        ProtobufCMessage*            msg,
                        rwdts_member_op_t            op,
                        rw_keyspec_path_t*           in_ks, 
                        RwDtsQuerySerial*            serial);
bool rwdts_member_responded_to_router(rwdts_xact_t* xact);

void rwdts_appconf_register_deinit(rwdts_member_registration_t *reg);

const char*
rwdts_member_state_to_str(rwdts_member_xact_state_t state);

rw_status_t
rwdts_store_cache_obj(rwdts_xact_t* xact);

/* rwdts_member_xact.c */

const char*
rwdts_query_action_to_str(RWDtsQueryAction action, char *str, size_t str_len);

/* rwdts_member_data_api.c */

rw_status_t
rwdts_member_data_deinit(rwdts_api_t*                 apih,
                         rwdts_member_data_object_t *mobj);

rwdts_member_data_object_t*
rwdts_member_data_init(rwdts_member_registration_t* reg,
                       ProtobufCMessage*            msg,
                       rw_keyspec_path_t*           keyspec,
                       bool                         usemsg,
                       bool                         useks);

const char*
rwdts_member_state_to_str(rwdts_member_xact_state_t state);

const char*
rwdts_member_event_to_str(rwdts_member_xact_evt_t evt);

/* rwdts_member_kv.c */

rw_status_t
rwdts_kv_update_db_update(rwdts_member_data_object_t *mobj, RWDtsQueryAction action);

rw_status_t
rwdts_kv_update_db_xact_precommit(rwdts_member_data_object_t *mobj, RWDtsQueryAction action);

rw_status_t
rwdts_kv_update_db_xact_commit(rwdts_member_data_object_t *mobj, RWDtsQueryAction action);

void
rwdts_trace_event_print_block(RWDtsTracerouteEnt *ent, char *logbuf, rw_yang_pb_schema_t *schema);

void
rwdts_trace_print_req(RWDtsTracerouteEnt *ent, char *logbuf,char *evt, char *state,char *res_code);

// Maximum possible is < 127 - 7 bits
#define RWDTS_MAX_REGISTRATION_RETRIES 64
#define RWDTS_MAX_LOGBUFSZ 2048

#define _RWDTS_PRINTF(args...) fprintf(stderr, args)
#if 1
#if 1
#define RWDTS_PRINTF(args...) \
  do { if (getenv("RWDTS_DEBUG")) { _RWDTS_PRINTF(args); } } while (FALSE)
#else
#define RWDTS_PRINTF _RWDTS_PRINTF
#endif
#else
#define RWDTS_PRINTF(args...) \
  do { if (FALSE) { _RWDTS_PRINTF(args); } } while (FALSE)
#endif


/*
 *  DTS API logging macros
 */

#define RWDTS_API_LOG_EVENT(__apih__, __evt__, ...)  \
  RWLOG_EVENT((__apih__)->rwlog_instance, RwDtsApiLog_notif_##__evt__, (__apih__)->client_path, \
              (__apih__)->router_path, __VA_ARGS__)

/*
 * DTS logging macro for xact related events
 */

#define RWDTS_API_LOG_XACT_EVENT(__apih__, __xact__, __evt__, ...)  \
  RWLOG_EVENT_CODE((__apih__)->rwlog_instance, RwDtsApiLog_notif_##__evt__, \
                   (/* code_before_event */ \
                       char RWDTS_API_tmp_log_xact_id_str[256] = "";              \
                       rwdts_xact_id_str(&(__xact__)->id, RWDTS_API_tmp_log_xact_id_str, sizeof(RWDTS_API_tmp_log_xact_id_str)); \
                     ),\
                   (/* code_after_event_ */), \
                   RWDTS_API_tmp_log_xact_id_str, __VA_ARGS__);

/*
 * DTS logging macro for xact related debug events
 */

#define RWDTS_API_LOG_XACT_DEBUG_EVENT(__apih__, __xact__, __evt__, ...)  \
  //    RWDTS_API_LOG_XACT_EVENT(__apih__, __xact__, __evt__, __VA_ARGS__)

/*
 * DTS logging macro for registration related events
 */
#define RWDTS_API_LOG_REG_EVENT(__apih__, __reg__, __evt__,  ...)  \
{ \
  char *tmp_log_reg_ks_str = NULL; \
  RW_ASSERT(__reg__); \
  RW_ASSERT((__reg__)->keyspec); \
  RW_ASSERT(((ProtobufCMessage*)(__reg__)->keyspec)->descriptor); \
  const  rw_yang_pb_schema_t* tmp_log_ks_schema = \
    ((ProtobufCMessage*)(__reg__)->keyspec)->descriptor->ypbc_mdesc->module->schema; \
  if (!tmp_log_ks_schema) { \
    tmp_log_ks_schema = rwdts_api_get_ypbc_schema(__apih__);\
  } \
  rw_keyspec_path_get_new_print_buffer((__reg__)->keyspec, NULL, tmp_log_ks_schema, \
                              &tmp_log_reg_ks_str); \
  RWLOG_EVENT((__apih__)->rwlog_instance, RwDtsApiLog_notif_##__evt__,  __apih__->client_path, \
              __apih__->router_path, tmp_log_reg_ks_str ? tmp_log_reg_ks_str:"", __VA_ARGS__); \
  free(tmp_log_reg_ks_str); \
}

/*
 *  DTS Router logging macros
 */

#define RWDTS_ROUTER_LOG_EVENT(__dts__, __evt__, ...)                        \
{                                                                        \
  if ((__dts__)->rwtaskletinfo->rwlog_instance) {                        \
    RWLOG_EVENT((__dts__)->rwtaskletinfo->rwlog_instance, RwDtsRouterLog_notif_##__evt__, (__dts__)->rwmsgpath,  __VA_ARGS__) \
  }                                                                        \
}

#define RWDTS_ROUTER_LOG_REG_EVENT(__dts__, __evt__, ...)                \
{                                                                        \
  RW_ASSERT((__dts__)->rwtaskletinfo);                                        \
  if ((__dts__)->rwtaskletinfo->rwlog_instance) {                        \
    RWLOG_EVENT((__dts__)->rwtaskletinfo->rwlog_instance, RwDtsRouterLog_notif_##__evt__, (__dts__)->rwmsgpath,  __VA_ARGS__); \
  }                                                                        \
}

#define RWDTS_ROUTER_LOG_XACT_EVENT(__dts__, __xact__, evvtt, ...)        \
{                                                                        \
  if ((__dts__)->rwtaskletinfo->rwlog_instance) {                        \
    RWLOG_EVENT_CODE((__dts__)->rwtaskletinfo->rwlog_instance, RwDtsRouterLog_notif_##evvtt, \
                     (/* code_before_event_ */ \
                         char RWDTS_ROUTER_LOG_tmp_log_xact_id_str[256] = "";    \
                         rwdts_xact_id_str(&(__xact__)->id,RWDTS_ROUTER_LOG_tmp_log_xact_id_str,sizeof(RWDTS_ROUTER_LOG_tmp_log_xact_id_str)); \
                      ), \
                     (/* code_after_event */), \
                     (__dts__)->rwmsgpath, RWDTS_ROUTER_LOG_tmp_log_xact_id_str, __VA_ARGS__); \
  }                                                                        \
}  

#define RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(__dts__, __xact__, __evt__, ...) \
  //  RWDTS_ROUTER_LOG_XACT_EVENT(__dts__, __xact__, __evt__, __VA_ARGS__)

#define RWDTS_Impl_Paste3(a,b,c) a##_##b##_##c

#define RWDTS_ROUTER_LOG_XACT_ID_EVENT(__dts__, __xact_id__, evvtt, ...)  \
  RWDTS_ROUTER_LOG_XACT_ID_EVENT_Step2(__dts__, __xact_id__, RWDTS_Impl_Paste3(RwDtsRouterLog, notif, evvtt), __VA_ARGS__)  \


#define RWDTS_ROUTER_LOG_XACT_ID_EVENT_Step2(__dts__, __xact_id__, evvtt, ...) \
{                                                                        \
  char tmp_log_xact_id_str[256] = "";                                        \
  if ((__dts__)->rwtaskletinfo->rwlog_instance) {                        \
    RWLOG_EVENT((__dts__)->rwtaskletinfo->rwlog_instance, evvtt, (__dts__)->rwmsgpath, \
      ((char*)rwdts_xact_id_str((__xact_id__),tmp_log_xact_id_str,sizeof(tmp_log_xact_id_str))), __VA_ARGS__); \
  }                                                                        \
}

/*
 * DTS tracing log event macro
 */
#define RWDTS_TRACE_EVENT_ADD_BLOCK(ctx_,path_, xact_id, schema, ent) \
{\
    char logbuf[RWDTS_MAX_LOGBUFSZ+1]; \
    int blkidx = ent.block->block?ent.block->block->blockidx:0;\
    rwdts_trace_event_print_block(&ent, logbuf, schema); \
    RWLOG_EVENT(ctx_,RwDtsApiLog_notif_##path_, xact_id, blkidx, ent.dstpath, logbuf); \
}\

#define RWDTS_TRACE_EVENT_REQ(ctx_,path_,xact_id, ent) \
{\
    char logbuf[RWDTS_MAX_LOGBUFSZ+1],evt[32],state[32],res_code[64]; \
    rwdts_trace_print_req(&ent, logbuf,evt, state,res_code);\
    RWLOG_EVENT(ctx_,RwDtsApiLog_notif_##path_,xact_id,evt,ent.srcpath,ent.dstpath,state,res_code,ent.res_count,ent.elapsed_us,logbuf);\
}



/* Debug traceroute / dump support functions */
extern void rwdts_xact_dbg_tracert_start(RWDtsXact *xact, rwdts_trace_filter_t *filt);
extern void rwdts_xactres_dbg_tracert_start(RWDtsXactResult *xact);
extern void rwdts_dbg_tracert_remove_all_ent(RWDtsTraceroute *tr);
extern void rwdts_dbg_add_tr(RWDtsDebug *dbg, rwdts_trace_filter_t *fit);
extern int rwdts_dbg_tracert_add_ent(RWDtsTraceroute *tr, RWDtsTracerouteEnt *ent);
extern void rwdts_dbg_tracert_append(RWDtsTraceroute *dst, RWDtsTraceroute *src);
extern void rwdts_dbg_errrep_append(RWDtsErrorReport *dst, RWDtsErrorReport *src);
extern void rwdts_dbg_tracert_dump(RWDtsTraceroute *tr, rwdts_xact_t *xact);
extern char*  rwdts_print_ent_type(RWDtsTracerouteEntType type);
extern char* rwdts_print_ent_state(RWDtsTracerouteEntState state);
extern char* rwdts_print_ent_event(RWDtsXactEvt evt);

rw_status_t
rwdts_get_merged_keyspec(rw_keyspec_entry_t* pe, rw_schema_minikey_opaque_t* mk,
                         rw_keyspec_path_t *reg_keyspec, rw_keyspec_path_t **merged_keyspec);

void rwdts_xact_query_result_list_deinit(rwdts_xact_t *xact);

bool
rwdts_reroot_merge_queries(RWDtsQueryResult* qres,
                           ProtobufCBinaryData *msg_out,
                           rw_keyspec_path_t **ks_out);

bool
rwdts_reroot_merge_blocks(RWDtsXactBlockResult* bres,
                          ProtobufCBinaryData *msg_out,
                          rw_keyspec_path_t **ks_out);

void
rwdts_state_transition(rwdts_api_t *apih, rwdts_state_t state);

typedef struct rwdts_group_reg_info_s {
   uint32_t reg_id;
   bool recov_audit_complete;
   rw_sklist_element_t element;
} rwdts_group_reg_info_t;

/* A registration group */
struct rwdts_group_s {
  rwdts_api_t *apih;
  uint32_t id; //??
  rwdts_group_cbset_t cb;

  rw_sklist_t reg_list;
  uint32_t audit_recov_comp_count;
  rwdts_xact_t* xact;
};

/* The registration group state for a transaction.  Note that there
   are 0-n of these for each transaction, one per group. */
struct rwdts_group_xact_s {
  void *scratch;

  uint32_t xinit:1;
  // gosh, and end-of-prepare would be nice, but is unpossible outside lead router scope
  uint32_t precommit:1;
  uint32_t commit:1;
  uint32_t abort:1;
  uint32_t xdeinit:1;
  uint32_t _pad:27;

};

/* Registration-specific callback(s) */
struct rwdts_appconf_reg_s {
  rwdts_appconf_t *ac;
  struct {
    union {
      rwdts_appconf_prepare_cb_t prepare;
      rwdts_appconf_prepare_cb_gi prepare_gi;
    };
    void *prepare_ud;
    GDestroyNotify prepare_dtor;
  } cb;
};

struct rwdts_audit_result_s {
  int                  ref_cnt;
  rwdts_audit_status_t audit_status;
  char*                audit_msg;
};

char* rwdts_get_xact_state_string(rwdts_member_xact_state_t state);

RWDtsQuery*
rwdts_xact_init_query_from_pb(rwdts_api_t*                  apih,
                              const rw_keyspec_path_t*      keyspec,
                              const ProtobufCMessage*       msg,
                              RWDtsQueryAction              action,
                              uint64_t                      corrid,
                              const RWDtsDebug*             dbg,
                              uint32_t                      query_flags,
                              rwdts_member_registration_t*  reg);

/***********************************
 * Credit management
 *
 **********************************/
// These are percentages
#define RWDTS_CREDIT_HWM 90
#define RWDTS_CREDIT_LWM 10

// Rate in percentage . Assuming no float support
// Allocate rate is 2%
#define RWDTS_CREDIT_ALLOC_RATE 2

// Rate reduces by 1/2 if HWM reaches
#define RWDTS_CREDIT_HWM_RATE_REDUX_FACTOR 2

#define RWDTS_GETNEXT_PRINT_ERR(args...)
#define IS_ABOVE_HWM(credit) (((credit.credits_total)-(credit.credits_remaining))*100/(credit.credits_total) > RWDTS_CREDIT_HWM)
#define IS_BELOW_LWM(credit) (((credit.credits_total)-(credit.credits_remaining))*100/(credit.credits_total) < RWDTS_CREDIT_LWM)
#define ALLOC_CREDITS(credit,req) ((req*(credit.credits_total))/100)

// RIFT-5821
//#define FREE_CREDITS(credit,req) ((credit.credits_remaining)+=req)
#define FREE_CREDITS(credit,req) { if (credit.credits_remaining < INT32_MAX) { credit.credits_remaining += req; } }
#define CHECK_CREDITS(credit) ((credit.credits_remaining) < (credit.credits_total))
#define VALIDATE_CREDITS(credit,free_req) (((credit.credits_remaining)+free_req) <= (credit.credits_total))
#define REDUCE_CREDITS(credit,req) ((credit.credits_remaining)-=req)

#define INIT_DTS_CREDITS(credits, initial_count) \
    credits.credits_total = initial_count; \
    credits.credits_remaining = initial_count; \
    credits.credit_alloc_rate = ((RWDTS_CREDIT_ALLOC_RATE*(credits.credits_total))/100);

#define RWDTS_GET_CREDITS(credit_obj,__max) \
({ \
  uint32_t __credits = (__max)?(__max):1; \
  credit_obj.stats.alloc_called++; \
  if (credit_obj.credit_alloc_rate < __credits) \
  {\
    __credits = credit_obj.credit_alloc_rate; \
  }\
  if (credit_obj.credit_status == RWDTS_CREDIT_STATUS_LWM) \
  { \
    if(IS_ABOVE_HWM(credit_obj)) \
    { \
      credit_obj.credit_status = RWDTS_CREDIT_STATUS_HWM; \
      credit_obj.stats.hwm_reached++; \
      credit_obj.credit_alloc_rate = (credit_obj.credit_alloc_rate)/RWDTS_CREDIT_HWM_RATE_REDUX_FACTOR; \
    } \
  } \
  else \
  { \
    if (!CHECK_CREDITS(credit_obj)) \
    { \
      credit_obj.stats.ooc++; \
      credit_obj.credits_remaining+=credit_obj.credits_total+1; \
      credit_obj.credits_total+=credit_obj.credits_total+1; \
      RWDTS_GETNEXT_PRINT_ERR("DTS ran out of credit New CT %u CR %u\n", credit_obj.credits_total, credit_obj.credits_remaining); \
    } \
  } \
  REDUCE_CREDITS(credit_obj, __credits); \
  __credits; \
})

#if 0
  printf("%d:CREDIT@%p GOT %d CT %u CR %u\n", __LINE__,&credit_obj, __credits, credit_obj.credits_total , credit_obj.credits_remaining); \
  printf("%d:CREDIT@%p FREE %d CT %u CR %u\n", __LINE__,&credit_obj, __credits, credit_obj.credits_total , credit_obj.credits_remaining);
#endif

#define RWDTS_FREE_CREDITS(credit_obj,__credits) \
{ \
  credit_obj.stats.free_called++; \
  if(__credits && VALIDATE_CREDITS(credit_obj, __credits)) \
  { \
    if(credit_obj.credit_status == RWDTS_CREDIT_STATUS_HWM  && IS_BELOW_LWM(credit_obj)) \
    { \
      credit_obj.credit_alloc_rate = credit_obj.credit_alloc_rate*RWDTS_CREDIT_HWM_RATE_REDUX_FACTOR;\
      credit_obj.credit_status = RWDTS_CREDIT_STATUS_LWM; \
      credit_obj.stats.lwm_reached++; \
    } \
  } \
  else \
  { \
    RWDTS_GETNEXT_PRINT_ERR("DTS Excess credit\n"); \
    credit_obj.stats.invalid_op++; \
  } \
  FREE_CREDITS(credit_obj, __credits); \
}

#define DUMP_CREDIT_OBJ(credit_obj) \
    printf ("Dump Stats\n" \
            "CT %u CR %u Rate %u, Stats:alloced %lu freed %lu LWM_rchd %lu HWM_rchd %lu ooc %lu inv_op %lu \n", \
            credit_obj.credits_total, \
            credit_obj.credits_remaining, \
            credit_obj.credit_alloc_rate, \
            credit_obj.stats.alloc_called, \
            credit_obj.stats.free_called, \
            credit_obj.stats.lwm_reached, \
            credit_obj.stats.hwm_reached, \
            credit_obj.stats.ooc, \
            credit_obj.stats.invalid_op);


#define RWDTS_CREATE_SHARD(reg_id, client, rtr) \
    char shard[100] = "\0"; \
    sprintf(shard, "%d-%s-%s", reg_id, client, rtr);

void rwdts_xact_status_deinit(rwdts_api_t *apih, rwdts_xact_status_t *s);

rwdts_xact_info_t *rwdts_xact_info_ref(const rwdts_xact_info_t *boxed);

void
rwdts_xact_info_unref(rwdts_xact_info_t *boxed);

/**
 *  rwdts_xact_info_get_next_key:
 */
void*
rwdts_xact_info_get_next_key(const rwdts_xact_info_t *xact_info);

uint64_t rwdts_get_serialnum (rwdts_api_t*           apih,
                              rwdts_member_registration_t* regn);

bool rwdts_serial_check(uint64_t client_idx,
                        uint64_t router_idx,
                        uint64_t reg_id,
                        uint64_t serialnum,
                        rwdts_member_registration_t *reg);

rwdts_member_data_object_t**
rwdts_get_member_data_array_from_msgs_and_keyspecs(rwdts_member_registration_t *reg,
                                                   rwdts_query_result_t        *query_result,
                                                   uint32_t                    n,
                                                   uint32_t                    *out_n);
void
rwdts_deinit_member_data_array(rwdts_member_data_object_t** array,
                               uint32_t                     n);
rw_status_t
rwdts_xact_get_result_pb_int(rwdts_xact_t*                     xact,
                             uint64_t                          corrid,
                             const ProtobufCMessageDescriptor* desc,
                             ProtobufCMessage**                result,
                             rw_keyspec_path_t**               keyspecs,
                             size_t                            index,
                             size_t                            size_result,
                             uint32_t*                         n_result,
                             rwdts_xact_result_t*              res,
                             rw_keyspec_path_t *               ks);

rw_status_t
rwdts_xact_get_result_pb_int_alloc(rwdts_xact_t*                     xact,
                                   uint64_t                          corrid,
                                   rwdts_xact_result_t*              res,
                                   ProtobufCMessage***               result,
                                   rw_keyspec_path_t***              keyspecs,
                                   uint32_t**                        types,
                                   rw_keyspec_path_t*                keyspec_level,
                                   uint32_t*                         n_result);

rw_status_t rwdts_xact_get_result_pbraw(rwdts_xact_t*         xact,
                                        rwdts_xact_block_t   *blk,
                                        uint64_t              corrid,
                                        rwdts_xact_result_t** const result,
                                        RWDtsQuery** query);

/* Internal global functions from rwdts_audit.c */

rw_status_t
rwdts_audit_deinit(rwdts_audit_t *audit);

void
rwdts_audit_init(rwdts_audit_t *audit,
                 rwdts_audit_done_cb_t audit_done,
                 void* user_data,
                 rwdts_audit_action_t action);

/* End of functions from rwdts_audit.c */

rwdts_audit_result_t * rwdts_audit_result_new();

rwdts_audit_result_t*
rwdts_audit_result_ref(rwdts_audit_result_t *boxed);

void
rwdts_audit_result_unref(rwdts_audit_result_t *boxed);

rw_status_t
rwdts_member_send_error(rwdts_xact_t *xact,
                        const rw_keyspec_path_t* keyspec,
                        RWDtsQuery *xquery,
                        rwdts_api_t *apih,
                        const rwdts_member_query_rsp_t* rsp,
                        rw_status_t cause,
                        const char* errstr);

rw_status_t
rwdts_member_send_err_on_abort (rwdts_xact_t *xact,
                                rw_status_t cause,
                                const char* errstr);

RWDtsErrorReport *
rwdts_xact_create_err_report();

rw_status_t
rwdts_xact_append_err_report(RWDtsErrorReport *rep,
                             RWDtsErrorEntry *ent);
rw_status_t
rwdts_member_send_response_int(rwdts_xact_t*                   xact,
                               rwdts_query_handle_t            queryh,
                               rwdts_member_query_rsp_t* rsp);

void rwdts_client_dump_xact_info(rwdts_xact_t *xact, const char *reason);

rwdts_member_rsp_code_t
rwdts_member_start_audit(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t* key,
                         const ProtobufCMessage*  msg,
                         uint32_t                 credits,
                         void*                    getnext_ptr);

rw_status_t
rwdts_xact_get_result_pb_local(rwdts_xact_t*                xact,
                               uint64_t                     corrid,
                               rwdts_member_registration_t* reg,
                               ProtobufCMessage***          result,
                               rw_keyspec_path_t***         keyspecs,
                               uint32_t*                    n_result);

RWDtsKey*
rwdts_init_key_from_keyspec_int(const rw_keyspec_path_t* keyspec);

rwdts_member_reg_handle_t
rwdts_member_register_int(rwdts_xact_t*   xact,
                          rwdts_api_t*                      apih,
                          rwdts_member_reg_handle_t         reg_handle,
                          rwdts_group_t*                    group,
                          const rw_keyspec_path_t*          keyspec,
                          rwdts_member_event_cb_t*    cb,
                          const ProtobufCMessageDescriptor* desc,
                          uint32_t                          flags,
                          const rwdts_shard_info_detail_t*  shard_detail);

rw_status_t
rwdts_advise_query_proto_int(rwdts_api_t*             apih,
                         rw_keyspec_path_t*           key,
                         rwdts_xact_t*                xact_parent,
                         const ProtobufCMessage*      msg,
                         uint64_t                     corrid,
                         RWDtsDebug*                  dbg,
                         RWDtsQueryAction             action,
                         const rwdts_event_cb_t*      cb,
                         uint32_t                     flags,
                         rwdts_member_registration_t* reg,
                         rwdts_xact_t**               out_xact);

RWDtsQuery*
rwdts_xact_init_query(rwdts_api_t*                 apih,
                      RWDtsKey*                    key,
                      RWDtsQueryAction             action,
                      RWDtsPayloadType             payload_type,
                      uint8_t*                     payload,
                      uint32_t                     payload_len,
                      uint64_t                     corrid,
                      const RWDtsDebug*            dbg,
                      uint32_t                     query_flags,
                      rwdts_member_registration_t* reg);
rwdts_api_t*
rwdts_api_init(rwtasklet_info_ptr_t           tasklet_info,
               const char*                    my_path,
               rwsched_dispatch_queue_t       q,
               uint32_t                       flags,
               const rwdts_state_change_cb_t* cb);

char*
rwdts_get_app_addr_res(rwdts_api_t* apih, void* addr);

rw_status_t
rwdts_member_data_object_add_audit_trail(rwdts_member_data_object_t*          mobj,
                                         RwDts__YangEnum__AuditTrailEvent__E  updated_by,
                                         RwDts__YangEnum__AuditTrailAction__E action);

rw_status_t
rwdts_member_reg_audit_invalidate_fetch_results(rwdts_member_registration_t *reg);

void
rwdts_report_unknown_fields(rw_keyspec_path_t* keyspec,
                            rwdts_api_t* apih,
                            ProtobufCMessage *msg);
rw_status_t
rwdts_xact_print(rwdts_xact_t *xact);


RWDtsEventRsp
rwdts_final_rsp_code(rwdts_xact_query_t *xquery);
rw_status_t
rwdts_reg_print(rwdts_member_registration_t *regh);

void
rwdts_log_handler (const gchar   *log_domain,
                   GLogLevelFlags log_level,
                   const gchar   *message,
                   gpointer       unused_data);

void rwdts_log_payload (RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *payload_stats,
                   RWDtsQueryAction action,
                   RwDts__YangEnum__MemberRole__E role,
                   RwDts__YangEnum__XactLevel__E level,
                   uint32_t len);
void rwdts_calc_log_payload(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *payload_stats,
                      RwDts__YangEnum__MemberRole__E role,
                      RwDts__YangEnum__XactLevel__E level,
                      void *data);
void rwdts_log_payload_init(RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) *mbr_payload_stats_p);
void rwdts_member_start_prep_timer_cancel(rwdts_match_info_t* match);
void rwdts_member_prep_timer_cancel(rwdts_match_info_t* match, bool timeout, int line);
bool rwdts_cache_reg_ok(rwdts_api_t *apih);
rw_status_t rwdts_report_error(rwdts_api_t *apih, RWDtsErrorReport * error_report);
void rwdts_xact_id_memcpy(RWDtsXactID *dst, const RWDtsXactID *src);

rwdts_member_rsp_code_t
rwdts_member_handle_solicit_advise_ks(const rwdts_xact_info_t* xact_info,
                                      RWDtsQueryAction         action,
                                      const rw_keyspec_path_t* key,
                                      const ProtobufCMessage*  msg,
                                      uint32_t                 credits,
                                      void*                    getnext_ptr);
rw_status_t
rwdts_match_list_deinit(rw_sklist_t *match_list);

rw_status_t
rwdts_member_reg_handle_issue_advise(rwdts_member_reg_handle_t regh);

rw_status_t
rwdts_query_fill_serial(RWDtsQuery*                  query,
                        rwdts_api_t*                 apih,
                        rwdts_member_registration_t* reg);

void rwdts_member_data_advise_cb(rwdts_xact_t*        xact,
                                 rwdts_xact_status_t* status,
                                 void*                ud);

rw_status_t
rwdts_member_find_matches_for_keyspec(rwdts_api_t*             apih,
                                      const rw_keyspec_path_t* keyspec,
                                      rw_sklist_t*             matches,
                                      uint32_t                 flags);
void
rwdts_member_reg_handle_mark_internal(rwdts_member_reg_handle_t handle);

void
rwdts_member_reg_handle_inc_serial(rwdts_member_registration_t *reg);

rw_status_t
rwdts_member_reg_handle_update_pub_serial(rwdts_member_registration_t* reg,
                                          RwDtsQuerySerial*            pub_serial);
rwdts_pub_serial_t*
rwdts_pub_serial_init(rwdts_member_registration_t* reg,
                      rwdts_pub_identifier_t*      pub_id);
void
rwdts_pub_serial_hash_deinit(rwdts_member_registration_t *reg);

rw_status_t
rwdts_update_registration_serials(rwdts_xact_t *xact);

rwdts_member_reg_handle_t
rwdts_member_reg_run(rwdts_member_registration_t*        reg,
                     RwDts__YangEnum__RegistrationEvt__E evt,
                     void*                               ud);

rwdts_member_reg_handle_t
rwdts_member_registration_to_handle(rwdts_member_registration_t* reg);

rwdts_member_registration_t*
rwdts_member_reg_handle_to_reg(rwdts_member_reg_handle_t regh);


/* commit serial list APIs */
void
rwdts_init_commit_serial_list(rw_sklist_t* skl);

void
rwdts_add_entry_to_commit_serial_list(rwdts_api_t *apih,
                                      rw_sklist_t*              skl,
                                      const rwdts_pub_serial_t* entry,
                                      uint64_t*                 high_commit_serial);
void 
dump_commit_serial_list(rw_sklist_t* skl);

void
rwdts_deinit_commit_serial_list(rwdts_api_t *apih,
                                rw_sklist_t* skl);


rwdts_xact_t*
rwdts_member_data_advice_query_action_xact(rwdts_xact_t*             xact,
                                           rwdts_xact_block_t*       block,
                                           rwdts_member_reg_handle_t regh,
                                           rw_keyspec_path_t*        keyspec,
                                           const ProtobufCMessage*   msg,
                                           RWDtsQueryAction          action,
                                           uint32_t                  flags,
                                           bool                      inc_serial);

rw_status_t
rwdts_xact_block_add_query_ks_reg(rwdts_xact_block_t* block,
                                  const rw_keyspec_path_t* key,
                                  RWDtsQueryAction action,
                                  uint32_t flags,
                                  uint64_t corrid,
                                  const ProtobufCMessage* msg,
                                  rwdts_member_registration_t* reg);

/* end of commit serial list APIs */

/*********** RWDTS Journal Changes START *******************/
#define RWDTS_JOURNAL_IN_USE_QLEN 10
typedef struct rwdts_journal_q_element_s {
  uint64_t router_idx;
  uint64_t client_idx;
  uint64_t serialno;
  rwdts_xact_query_t  *xquery;
  rwdts_member_xact_evt_t    evt;
  struct rwdts_journal_q_element_s  *next;
}rwdts_journal_q_element_t;

#define RWDTS_JOURNAL_USED(t_mode) \
    (((t_mode) == RWDTS_JOURNAL_IN_USE) || \
     ((t_mode) == RWDTS_JOURNAL_LIVE))
typedef enum  rwdts_journal_mode_s {
  RWDTS_JOURNAL_DISABLED,
  RWDTS_JOURNAL_IN_USE,
  RWDTS_JOURNAL_LIVE
} rwdts_journal_mode_t;

#define RWDTS_JOURNAL_QRY_ID_PTR(t_query) (&((t_query)->serial->id))
#define RWDTS_JOURNAL_QRY_SERIAL(t_query) ((t_query)->serial->serial)
#define RWDTS_JOURNAL_HD_QRY(t_entry) ((t_entry)->xquery->query)

#define RWDTS_JOURNAL_IS_VALID_QUERY(t_query) \
    ((t_query) \
    && ((t_query)->serial) \
    && ((t_query)->serial->has_id) \
    && ((t_query)->serial->has_serial))

#define RWDTS_JOURNAL_VALID_QUERY(t_query) \
    RW_ASSERT((t_query)); \
    RW_ASSERT((t_query)->serial); \
    RW_ASSERT((t_query)->serial->has_id); \
    RW_ASSERT((t_query)->serial->has_serial); 

typedef struct rwdts_journal_entry_s {
  UT_hash_handle     hh_journal_entry;
  RwDtsPubIdentifier id;
  unsigned long      least_done_serial;
  unsigned long      least_inflt_serial;

  int                        done_q_len;
  rwdts_journal_q_element_t  *done_q_tail;
  rwdts_journal_q_element_t  *done_q;
  int                        inflt_q_len;
  rwdts_xact_query_t         *inflt_q_tail;
  rwdts_xact_query_t         *inflt_q;
  rwdts_xact_query_t         *inflt_queries;

} rwdts_journal_entry_t;

typedef struct rwdts_journal_s {
  rwdts_journal_mode_t  journal_mode;

  rwdts_journal_entry_t *journal_entries;
}rwdts_journal_t;

struct rwdts_xact_query_journal_s {
  struct rwdts_xact_query_s *next;
  struct rwdts_xact_query_s *prev;
};

typedef struct rwdts_memer_data_adv_event_s {
  rwdts_event_cb_t             event_cb;
  rwdts_member_registration_t* reg;
  uint32_t                     reg_id;
  uint64_t                     serial;
  int                          solicit_triggered;
  int                          block_event;
} rwdts_memer_data_adv_event_t;

rwdts_memer_data_adv_event_t*
rwdts_memer_data_adv_init(const rwdts_event_cb_t*      advise_cb,
                          rwdts_member_registration_t* reg,
                          bool                         solicit_triggered,
                          bool                         block_event);

#define RWDTS_JOURNAL_XQUERY_NEXT(t_xquery) ((t_xquery)->inflt_links->next)
#define RWDTS_JOURNAL_XQUERY_PREV(t_xquery) ((t_xquery)->inflt_links->prev)

rw_status_t rwdts_journal_set_mode(rwdts_member_registration_t *reg,
                                   rwdts_journal_mode_t journal_mode);
rw_status_t rwdts_journal_update(rwdts_member_registration_t *reg,
                                 rwdts_xact_query_t *xquery,
                                 rwdts_member_xact_evt_t evt);
rw_status_t rwdts_journal_compare(rwdts_member_registration_t *reg);
rw_status_t rwdts_journal_merge(rwdts_member_registration_t *reg);
rw_status_t rwdts_journal_destroy(rwdts_member_registration_t *reg);
RWDtsQuery*
rwdts_xact_test_init_empty_query(rwdts_member_registration_t *reg);

rw_status_t
rwdts_member_registration_delete(rwdts_member_registration_t *reg);

void
rwdts_member_data_set_advise_cb_params(rwdts_member_data_record_t*  mbr_data,
                                       rwdts_event_cb_t*            advise_cb,
                                       uint32_t*                    flags,
                                       rwdts_member_registration_t* reg,
                                       bool                         solicit_triggered);

/*********** RWDTS Journal Changes END *******************/

/* Shard and App data related changes */
rw_status_t
rwdts_shard_handle_appdata_create_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          const ProtobufCMessage* msg);

rw_status_t
rwdts_shard_handle_appdata_get_element(rwdts_shard_handle_t* shard_tab,
                          rw_keyspec_path_t* keyspec,
                          ProtobufCMessage** out_msg);

rw_status_t
rwdts_shard_handle_appdata_delete_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          ProtobufCMessage* msg);

rw_status_t
rwdts_shard_handle_appdata_update_element(rwdts_shard_handle_t* shard_tab,
                                          rw_keyspec_path_t* keyspec,
                                          const ProtobufCMessage* msg);

rwdts_appdata_cursor_t*
rwdts_shard_handle_get_current_cursor(rwdts_shard_handle_t* shard);

void
rwdts_shard_handle_reset_cursor(rwdts_shard_handle_t* shard);

/* Shard and App data related changes END */

RWDtsQuerySingleResult*
rwdts_alloc_single_query_result(rwdts_xact_t*      xact,
                                ProtobufCMessage*  matchmsg,
                                rw_keyspec_path_t* matchks);

void
rwdts_shard_handle_appdata_delete(rwdts_shard_handle_t* shard);

typedef struct rwdts_api_config_ready_data_s {
  rwdts_api_t *apih;
  void *regh_p;
  char *xpath;
  bool config_ready;
  RWPB_E(RwBase_StateType) state;
} rwdts_api_config_ready_data_t;

typedef struct rwdts_api_update_element_async_s {
  rwdts_api_t *apih;
  rwdts_member_reg_handle_t *regh_p;
  char *xpath;
  ProtobufCMessage*   msg;
} rwdts_api_update_element_async_t;

void
rwdts_config_ready_publish(rwdts_api_config_ready_data_t *config_ready_data);

rwdts_member_reg_handle_t rwdts_config_ready_publish_register(
    rwdts_api_t *apih,
    char *xpath,
    char *child_name);

rwdts_query_result_t*
rwdts_qrslt_dup(rwdts_query_result_t* qrslt);

void rwdts_query_result_deinit(rwdts_query_result_t* query_result);

uint32_t
rwdts_get_stored_keys(rwdts_member_registration_t* reg,
                                            rw_keyspec_path_t*** keyspecs);
rw_status_t rwdts_journal_show_init(rwdts_api_t *apih);
void rwdts_journal_consume_xquery(rwdts_xact_query_t *xquery,
                                  rwdts_member_xact_evt_t evt);
rwsched_dispatch_source_t
rwdts_member_timer_create(rwdts_api_t *apih,
                          uint64_t timeout,
                          dispatch_function_t timer_cb,
                          void *ud,
                          bool start);
rw_status_t
rwdts_member_reg_handle_solicit_advise(rwdts_member_reg_handle_t  regh);

void
rwdts_group_reg_reset_audit_comp(rwdts_group_t *group);

void
rwdts_group_reg_install_event(rwdts_group_t *group,
                              uint32_t reg_id);

static inline bool
rwdts_query_allow_wildcards(RWDtsQuery *query)
{
  if ((query->action == RWDTS_QUERY_READ) ||
      (query->action == RWDTS_QUERY_DELETE) ||
      (query->action == RWDTS_QUERY_RPC)){
    return true;
  }
  
  if (((query->flags & RWDTS_XACT_FLAG_KEYLESS) &&
       (query->flags & RWDTS_XACT_FLAG_ADVISE) == 0) &&
      (query->action == RWDTS_QUERY_CREATE)){
    return true;
  }
  return false;
}

static inline bool rwdts_query_is_transactional(rwdts_xact_query_t *xquery)
{
  if ((xquery->query->action != RWDTS_QUERY_READ) &&
      !(xquery->query->flags&RWDTS_XACT_FLAG_NOTRAN)) {
    return true;
  }
  return false;
}

static inline bool rwdts_is_transactional(rwdts_xact_t *xact)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_xact_query_t *xquery=NULL, *xqtmp=NULL;
  HASH_ITER(hh, xact->queries, xquery, xqtmp) {
    if (rwdts_query_is_transactional(xquery)){
      return true;
    }
  }
  return false;
}

static inline bool rwdts_api_in_correct_queue(rwdts_api_t* apih)
{
  pid_t tid;

  
  if (apih->client.rwq != rwsched_dispatch_get_main_queue(apih->sched)){
    /*apih using its own queue and not the main queue. Once we have the ability
      to get the queue on which the dispatch is happening, we need to check that queue
      with the client rwq. Until then assume that things are happening on the correct
      queue. Currently the router is the only one using its own queue*/
    return true;
  }
  
  tid  = RWSCHED_GETTID();
  if (tid == apih->tasklet->cfrunloop_tid){
    return true;
  }
  return false;
}


__END_DECLS

#endif /* __RWDTS_API_H */
