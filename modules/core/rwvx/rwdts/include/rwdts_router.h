
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


/*!
 * @file rwdts_router.h
 * @brief Router for RW.DTS
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_ROUTER_H
#define __RWDTS_ROUTER_H

#include <uthash.h>
#include <rw_tasklet_plugin.h>
#include <rwtasklet.h>
#include <rw_dts_int.pb-c.h>
#include <rw_cf_type_validate.h>
#include <rwmemlog.h>

#include <rwdts_query_api.h>
#include <rwdts_member_api.h>
#include <rwdts_keyspec.h>
#include "rwvcs_zk_api.h"
#include "rwdts_int.h"

__BEGIN_DECLS

typedef struct rwdts_router_s rwdts_router_t;
//typedef struct rwdts_router_member_s rwdts_router_member_t;
typedef struct rwdts_router_member_node_s rwdts_router_member_node_t;
typedef struct rwdts_router_xact_s rwdts_router_xact_t;
typedef struct rwdts_router_xact_req_s rwdts_router_xact_req_t;
typedef struct rwdts_router_xact_sing_req_s rwdts_router_xact_sing_req_t;
typedef struct rwdts_router_xact_rwmsg_ent_s rwdts_router_xact_rwmsg_ent_t;

typedef enum rwdts_router_comm_state_e {
  RWDTS_ROUTER_QUERY_NONE=0,    /* no state */
  RWDTS_ROUTER_QUERY_UNSENT=1,
  RWDTS_ROUTER_QUERY_SENT,        /* request sent, waiting for rsp */
  RWDTS_ROUTER_QUERY_ASYNC,        /* rsp received, no data, waiting for callback req */
  RWDTS_ROUTER_QUERY_ACK,        /* got rsp, ack */
  RWDTS_ROUTER_QUERY_NA,        /* got rsp, n/a */
  RWDTS_ROUTER_QUERY_NACK,        /* got rsp, nack */
  RWDTS_ROUTER_QUERY_INTERNAL,        /* got rsp, internal for router back pressure */
  RWDTS_ROUTER_QUERY_QUEUED,      /* member is recovering queue this req */
  RWDTS_ROUTER_QUERY_ERR        /* no rsp / err rsp */
} rwdts_router_comm_state_t;

typedef enum rwdts_router_xact_state_e {
  RWDTS_ROUTER_XACT_INIT=0,
  RWDTS_ROUTER_XACT_PREPARE,    /* note states > PREPARE deal only in events, not query/responses */
  RWDTS_ROUTER_XACT_PRECOMMIT,        /* not in subs */
  RWDTS_ROUTER_XACT_COMMIT,
  RWDTS_ROUTER_XACT_ABORT,
  RWDTS_ROUTER_XACT_SUBCOMMIT,  /* subx only */
  RWDTS_ROUTER_XACT_SUBABORT,        /* subx only */
  RWDTS_ROUTER_XACT_QUEUED,
  RWDTS_ROUTER_XACT_DONE,
  RWDTS_ROUTER_XACT_TERM
#define RWDTS_ROUTER_XACT_STATE_CT (RWDTS_ROUTER_XACT_TERM + 1)
} rwdts_router_xact_state_t;

typedef struct rwdts_router_queue_xact_s {
  rwdts_router_t *dts;
  rwdts_router_member_t *member;
} rwdts_router_queue_xact_t;

typedef struct rwdts_memb_router_registration_s {
  UT_hash_handle  hh_reg;         /*<  Hash by keyspec */
  rw_keyspec_path_t*   keyspec;        /*< keyspec associated with the registration */
  uint8_t*        ks_binpath;     /*< keyspec bin path */
  size_t          ks_binpath_len; /*< keyspec bin path len */
  uint64_t        rtr_reg_id;
  uint32_t        flags;
  rwdts_shard_t*  shard;
  rwdts_chunk_id_t chunk_id;
  uint32_t        membid;
  char keystr[256];
  rwdts_shard_flavor shard_flavor;
  union rwdts_shard_flavor_params_u params;
  bool has_index;
  int  index;
} rwdts_memb_router_registration_t;

typedef struct rwdts_memb_trans_registration_s {
  uint64_t xact_serialno;
  rw_sklist_element_t element;
  rwdts_memb_router_registration_t *registrations;
} rwdts_memb_trans_registration_t;

typedef struct router_member_stats_s {
  uint64_t reg_prepare_called;
  uint64_t reg_precommit_called;
  uint64_t reg_commit_called;
  uint64_t reg_added;
} router_member_stats_t;
/* Destination cache, including rwmsg dest objects and flow control
   state to each. Referenced by transactions and registration/routing
   data structures. */
struct rwdts_router_member_s {
  char *msgpath;
  bool wait_restart;
  bool to_recover;
  UT_hash_handle hh;                /* UTHASH by msgpath */

  /* heirarchical / in care of? */
  rwmsg_destination_t *dest;

  rwdts_memb_router_registration_t *registrations;
  rwdts_memb_router_registration_t *sub_registrations;
  rw_sklist_t trans_reglist;
  rw_dl_t     queued_xacts;
  rw_dl_t     sing_reqs;
  uint32_t reg_count;
  uint64_t client_idx;
  uint64_t router_idx;

  uint32_t refct;
  uint32_t dead:1;
  uint32_t xoff:1;                /* flow controlled */
  uint32_t _pad:30;

  /* Flow control pain.  Each destination has a flowid and associated
     flow control state (member variable xoff).  On BACKPRESSURE, we
     push the xact onto this flow control queue.  On FEEDME callback,
     we start running the queue to find the stuck requests.  Unclear
     if additional fancy is needed inside the xact code to leap
     directly to the backpressured requests, hopefully just brute
     iterating through the current block of query requests to find the
     unsent ones to this member is good enough. */
  rwmsg_flowid_t flowid;        /* flowid, if 0 we haven't ever gotten a rsp */
  struct {
    rwdts_router_xact_t *xact;
  } *flowq;
  uint32_t flowq_len;                /* size of table */
  uint32_t flowq_ct;                /* count of pending entries */
  uint32_t flowq_off;                /* offset of first entry waiting for dequeue */
  router_member_stats_t stats;
  rw_component_state    component_state;
};

/* See also grouping routerstats in data_rwdts.yang */
typedef struct rwdts_router_stats_s {
  uint64_t topx_begin;
  uint64_t topx_begin_tran;        /* how many were transactional */
  uint64_t topx_begin_notran;        /* how many were not transactional */
  uint64_t topx_end;
  uint64_t topx_end_notran_success;
  uint64_t topx_end_notran_fail;
  uint64_t topx_end_commit;        /* how many ended with a full commit */
  uint64_t topx_end_abort;        /* how many ended aborted */
  uint64_t topx_end_fail;        /* how many failed utterly */
  uint64_t topx_count_5s;
  uint64_t topx_rate_5s;
  uint64_t topx_latency_5s;
  uint64_t topx_count_15s;
  uint64_t topx_rate_15s;
  uint64_t topx_latency_15s;
  uint64_t topx_count_1m;
  uint64_t topx_rate_1m;
  uint64_t topx_latency_1m;
  uint64_t topx_count_5m;
  uint64_t topx_rate_5m;
  uint64_t topx_latency_5m;
  uint64_t topx_xact_histo[5];
  uint64_t topx_latency_histo[5];
  uint64_t topx_xact_15s_histo[5];
  uint64_t topx_xact_1m_histo[6];
  uint64_t topx_xact_5m_histo[10];
  uint64_t topx_latency_15s_histo[5];
  uint64_t topx_latency_1m_histo[6];
  uint64_t topx_latency_5m_histo[10];
  uint64_t topx_latency_inc;
  uint64_t topx_latency_xact_count;
  uint64_t topx_latency_15s_inc;
  uint64_t topx_latency_xact_15s_count;
  uint64_t topx_latency_1m_inc;
  uint64_t topx_latency_xact_1m_count;
  uint64_t topx_latency_5m_inc;
  uint64_t topx_latency_xact_5m_count;
  uint64_t last_xact_count;
  uint64_t last_xact_15s_count;
  uint64_t last_xact_1m_count;
  uint64_t last_xact_5m_count;
#define RWDTS_ROUTER_STATS_HISTO64_IDX(val) ((val) < 63 ? (val) : 63)
  uint64_t topx_member_histo[64]; /* how many participants overall, 0-62 or 63+ */
  uint64_t subx_begin;
  uint64_t subx_begin_notran;
  uint64_t subx_retry;
  uint64_t subx_retry_histo[64];/* how many retries are attempted */
  uint64_t subx_retry_success;        /* retry(ies) eventual outcome happy */
  uint64_t subx_retry_failure;        /* retry(ies) eventual outcome sad */
  uint64_t subx_end;
  uint64_t subx_end_subcommit;
  uint64_t subx_end_subabort;
  uint64_t subx_end_suberr;
  uint64_t subx_end_notran_success;
  uint64_t subx_end_notran_fail;
  uint64_t prep_query_member_included;
  uint64_t prep_query_member_filtered;
  uint64_t req_sent;
  uint64_t req_bnc;
  uint64_t req_bnc_imm;
  uint64_t req_responded;
  uint64_t req_responded_async;
  uint64_t req_rcv_regist;
  uint64_t req_rcv_execute_fwded;
  uint64_t req_rcv_end_fwded;
  uint64_t req_rcv_abort_fwded;
  uint64_t req_rcv_flush_fwded;
  uint64_t req_rcv_execute_fwdrsp;
  uint64_t req_rcv_end_fwdrsp;
  uint64_t req_rcv_abort_fwdrsp;
  uint64_t req_rcv_flush_fwdrsp;
  uint64_t req_rcv_execute;
  uint64_t req_rcv_execute_topx;
  uint64_t req_rcv_execute_append;
  uint64_t req_rcv_execute_subx;
  uint64_t req_blocks_histo[64]; /* how many subx/blocks per overall request, 0-62 or 63+ */
  uint64_t req_rcv_end;
  uint64_t req_rcv_abort;
  uint64_t req_rcv_flush;
  uint64_t req_rcv_update_fake_table;
  uint64_t req_rcv_mbr_query_response;
  uint64_t req_rcv_mbr_regist;
  uint64_t more_sent;
  uint64_t incorrect_clientidx;
  uint64_t incorrect_routeridx;
  uint64_t xact_life_less_50_ms;
  uint64_t xact_life_less_100_ms;
  uint64_t xact_life_less_500_ms;
  uint64_t xact_life_less_1_sec;
  uint64_t xact_life_more_1_sec;
  uint64_t xact_prep_time_less_50_ms;
  uint64_t xact_prep_time_less_100_ms;
  uint64_t xact_prep_time_less_500_ms;
  uint64_t xact_prep_time_less_1_sec;
  uint64_t xact_prep_time_more_1_sec;
  uint64_t xact_pcom_time_less_50_ms;
  uint64_t xact_pcom_time_less_100_ms;
  uint64_t xact_pcom_time_less_500_ms;
  uint64_t xact_pcom_time_less_1_sec;
  uint64_t xact_pcom_time_more_1_sec;
  uint64_t xact_com_time_less_50_ms;
  uint64_t xact_com_time_less_100_ms;
  uint64_t xact_com_time_less_500_ms;
  uint64_t xact_com_time_less_1_sec;
  uint64_t xact_com_time_more_1_sec;
  uint64_t xact_abort_time_less_50_ms;
  uint64_t xact_abort_time_less_100_ms;
  uint64_t xact_abort_time_less_500_ms;
  uint64_t xact_abort_time_less_1_sec;
  uint64_t xact_abort_time_more_1_sec;

  uint64_t member_ct;                /* number of known members */
  uint64_t xact_ct;                /* number of extant xact structures */
  uint64_t total_xact;
} rwdts_router_stats_t;

typedef struct rwdts_router_xact_stats_s {
  uint64_t a;
} rwdts_router_xact_stats_t;

struct rwdts_router_xact_sing_req_s {
  UT_hash_handle  hh_memb;
  rwdts_router_member_node_t *membnode;
  RWDtsPayloadType req_event;
  rwdts_router_comm_state_t xreq_state;
  uint32_t sent_before:1;
  uint32_t any_cast:1;
  uint32_t memb_xfered:1;
  uint32_t in_sing_reqs:1;
  uint32_t pad:28;
  int tracert_idx;                /* index into xres->dbg->tr->ent[] */
  rwmsg_request_t *req;                /* handle on our outstanding request */
  uint32_t credits; 
  rwdts_router_xact_req_t *xreq;
  rwdts_router_xact_t *xact;
  rw_dl_element_t memb_element;
#if 0
  RWDtsQueryResult result;        /* Query-dest's result(s). */
#endif
};

/* This implements one query per req.

   Now, how about request aggregation?  Combine the block_union with
   this struct and devolve the whole block into per-dest query sets,
   each to be sent as one msg.  */
struct rwdts_router_xact_req_s {
  uint32_t query_idx;                /* query in the subx/block */
  uint32_t block_idx;                /* into xact->xact_blocks[], NOT into rwmsg_tab[] etc */
  uint32_t query_serial;
  const char *keystr;                /* pointer to keystr in block */
  const RWDtsQuery *xquery;     /* pointer to the query itself, in the block/msg/etc */
  rwdts_router_xact_sing_req_t *sing_req;
  uint32_t sing_req_ct;
};

struct rwdts_router_member_node_s {
  rwdts_router_member_t *member;
  uint32_t bnc:1;
  uint32_t abrt:1;
  uint32_t _pad:30;
#ifdef SUB_XACT_SUPPRT
  UT_hash_handle hh_block;
#endif
  UT_hash_handle hh_xact;
};

typedef struct rwdts_router_async_info_s {
  rwdts_router_xact_t *xact;
  uint32_t idx;
} rwdts_router_async_info_t;

typedef struct rwdts_router_xact_block_s {
  uint32_t xact_blocks_idx;        /* index of this block in xact->xact_blocks[] */

  RWDtsXactBlkID blockid;        /* Primary key for non-event blocks */
  rwmsg_request_client_id_t rwmsg_cliid;        /* ID of client, for correlation with rwmsg_tab[] */

  RWDtsXactBlock *xbreq;        /* Copy of request */
  RWDtsXactBlockResult *xbres;  /* Results, ready to add into an RWDtsXactResult outgoing response message */

  enum rwdts_router_block_state_e {
    RWDTS_ROUTER_BLOCK_STATE_INIT=0,
    RWDTS_ROUTER_BLOCK_STATE_NEEDPREP,
    RWDTS_ROUTER_BLOCK_STATE_RUNNING,
    RWDTS_ROUTER_BLOCK_STATE_DONE,
    RWDTS_ROUTER_BLOCK_STATE_RESPONDED
  } block_state;

  uint32_t req_ct;              /* destination count for this block */
  uint32_t req_sent;                /* sent requests in table */
  uint32_t req_outstanding;        /* requests outstanding in table */
  uint32_t req_done;                /* completed requests in table */
  uint32_t req_done_nack;        /* error count from table's reqs */
  uint32_t req_done_internal;        /* internal count from table's reqs */
  uint32_t req_done_ack;        /* ack count from table's reqs */
  uint32_t req_done_na;                /* na count from table's reqs */
  uint32_t req_done_err;        /* na count from table's reqs */

  uint32_t event_block:1;        /* sending an event COMMIT/PRECOMMIT/ABORT/etc, not a query-block */
  uint32_t waiting:1;
  uint32_t pad:30;

  rwdts_router_xact_state_t evt; /* RWDTS_ROUTER_XACT_PREPARE/PRECOMMIT/COMMIT/ABORT event this block goes with */
} rwdts_router_xact_block_t;

typedef struct rwdts_router_pub_obj_s {
  rw_keyspec_path_t* keyspec;
  uint32_t flags;
} rwdts_router_pub_obj_t;

typedef struct rwdts_router_wait_blocks_s {
  UT_hash_handle  hh_wait_blocks;
  RWDtsXactBlkID  blockid;
  char*           member_msg_path;
} rwdts_router_wait_block_t;


#define MAX_RTR_TRACE_EVT 20
#define MAX_RTR_TRACE_STR_LEN 64
#define RWDTS_STATS_TIMEOUT (1 * NSEC_PER_SEC)

#define RWDTS_ROUTER_LOCAL_IDX(t_dts) \
    (t_dts->apih->router_idx)
#define RWDTS_ROUTER_IS_LOCAL_XACT(t_dts, t_id) \
  (RWDTS_ROUTER_LOCAL_IDX(t_dts) == t_id->router_idx)

#define RWDTS_MEMLOG_FLAGS_A ( RWMEMLOG_MEM0 | RWMEMLOG_CATEGORY_A )
#define RWDTS_RTR_DEBUG_TRACE_APPEND(t_xact, t_msg) {\
  if ((t_xact)->dbg && (t_xact)->dbg->tr && (t_msg)->dbg && (t_msg)->dbg->tr) {\
    rwdts_dbg_tracert_append((t_xact)->dbg->tr, (t_msg)->dbg->tr); \
  }\
}

#define RWDTS_RTR_ADD_TR_ENT_ENDED(t_xact) { \
  if (t_xact->dbg) {\
    RWDtsTracerouteEnt ent;\
    char xact_id_str[128];\
    rwdts_traceroute_ent__init(&ent);\
    ent.func = (char*) __PRETTY_FUNCTION__;\
    ent.line = __LINE__;\
    ent.event = RWDTS_EVT_PREPARE;\
    ent.what = RWDTS_TRACEROUTE_WHAT_END_FLAG;\
    ent.dstpath = t_xact->rwmsg_tab[0].client_path;\
    ent.srcpath = (char*)t_xact->dts->rwmsgpath;\
    if (t_xact->dbg->tr && t_xact->dbg->tr->print_) {\
      fprintf(stderr, "%s:%u: TYPE:%s, EVT:%s, DST:%s, SRC:%s\n", ent.func, ent.line,\
              rwdts_print_ent_type(ent.what), rwdts_print_ent_event(ent.event),\
              ent.dstpath, ent.srcpath);\
    }\
    rwdts_dbg_tracert_add_ent(t_xact->dbg->tr, &ent);\
    RWDTS_TRACE_EVENT_REQ(t_xact->dts->rwtaskletinfo->rwlog_instance, RwDtsApiLog_notif_TraceReq,rwdts_xact_id_str(&t_xact->id,xact_id_str,128),ent);\
  }\
}

typedef struct rwdts_router_queued_xact_s {
  rwdts_router_xact_t   *xact;
  rwdts_router_member_t *member;
  rw_dl_element_t       queued_xact_element;
  rw_dl_element_t       queued_member_element;
} rwdts_router_queued_xact_t;

/* Transaction.  There are the request block(s), each with query(s);
   the result(s), the destination */
struct rwdts_router_xact_s {
  UT_hash_handle hh;                /* hash handle for xacts by id (id in .xact.id) */

  rwdts_router_xact_state_t state;
  rwdts_router_xact_state_t fsm_trace[MAX_RTR_TRACE_EVT];
  unsigned char curr_trace_index;

  struct timeval tv_trace;

  rwmemlog_buffer_t *rwml_buffer;

  rwdts_router_t *dts;
  rw_dl_t        queued_members;

  struct rwdts_router_xact_rwmsg_ent_s {
    rwmsg_request_client_id_t rwmsg_cliid; /* Who is it from?  Tricky - this means there can be only one DTS API bound per RWMsg Clichan. */
    char *client_path; /* fallback lookup mechanism */
    rwmsg_request_t *rwmsg_req;    /* Original client request message(s) */

    /* Answer within this timer even if nothing is doing (circa 10s).  The client will re-request and we'll not be subject to rwmsg timeouts */
    rwdts_router_async_info_t *async_info;
    rwsched_dispatch_source_t async_timer;

    uint32_t done:1;                /* We have sent this client a terminal response (done/fail/commit/abort) */
    uint32_t dirty:1;                /* We have stuff to send ASAP */
    uint32_t _pad:30;
  } *rwmsg_tab;
  int rwmsg_tab_len;

  rwdts_router_xact_block_t **xact_blocks; /* One block per subx inside all the exec_req */
  uint32_t n_xact_blocks;

  /* xact_blocks for event announcements indexed by state */
  rwdts_router_xact_block_t *xact_blocks_event[RWDTS_ROUTER_XACT_STATE_CT];

  /* Stuff to go in the response(s) */
  RWDtsXactID id;                /* Primary ID */
  RWDtsDebug *dbg;                /* Accumulated trace; to be shoved in the final response to the original sender */

  //??  uint32_t done_xact;
  uint32_t has_parent:1;
  uint32_t do_commits:1;
  uint32_t abrt:1;
  uint32_t ended:1;
  uint32_t pending_del:1;
  uint32_t more_sent:1;
  uint32_t transient_reg:1;
  uint32_t blocks_advanced:1;        /* Set by block state machine to trigger further evaluation at xact fsm level */
  uint32_t move_to_precomm:1;
  uint32_t _pad1:23;

  rwdts_router_pub_obj_t** pub_obj_list;
  uint32_t n_obj_list;

  rwsched_dispatch_source_t timer;

  uint32_t query_serial;

  /* Current block execution state.  This dynamically sized array
     starts at the block/query in idx and has one entry per
     member. This block inturn has the list of individual requests per
     query/block being executed */
  rwdts_router_xact_req_t *req;
  uint32_t n_req;
  uint32_t req_sent;                /* sent requests in table */
  uint32_t req_done;                /* completed requests in table */
  uint32_t req_outstanding;        /* requests outstanding in table */
  uint32_t req_nack;                /* nack count from table's reqs */
  uint32_t req_err;                /* error count, not just responses, also bounces, other failure cases */

  uint32_t total_req_ct;
  bool isadvise;

  /* Stuff below here is for multiphase transactions (and aborts)
     only.  Second and further phases run around and do (pre)commit or
     abort actions using the transaction or subtransaction ID and
     these stored destination sets. */

  //??
  // only pre-sharding
  /* Table of members, used as values to be accumulated in the block
     and transaction destination sets.  No realloc, things can't move
     or UTHASH breaks.  Upon request xmit, the member is found (via
     lookup in both xact_union and block_union), or added, to destset.
     After block prepare phase completion the SUBCOMMIT/SUBABORT
     requests go to the block_union destination set.  If the block
     succeeds, then items in block_union are added as needed to
     xact_union. */
  rwdts_router_member_node_t *destset;
  uint32_t destset_len;
  uint32_t destset_ct;
  // end pre-sharding only

#ifdef SUB_XACT_SUPPRT
  /* Union set of (downstream) participants for just the currently
     executing block.  Only needed if we are doing subtransactions and
     will therefore need to send out SUBCOMMIT or SUBABORT messages.
     We don't add members which returned N/A.  This is a UTHASH, elem
     hh_block */
  rwdts_router_member_node_t *block_union;
#endif

  /* Total set of (downstream) participants.  This is updated after
     each block.  After PREPARE and SUBCOMMIT (if applicable),
     everyone gets the (pre)commit at once.  Come hierarchy, we'll
     uniq according to the in-care-of heirarchical delivery
     destination so as to send one PRECOMMIT/COMMIT.  This is a
     UTHASH, elem hh_xact. */
  rwdts_router_member_node_t *xact_union;

  uint32_t anycast_cnt;
  struct timeval start_time;
  int xact_prep_time;
  struct timeval precommit_time;
  int xact_pcom_time;
  struct timeval commit_time;
  int xact_com_time;
  struct timeval abort_time;
  int xact_abort_time;
  struct timeval done_time;
  int time_spent_in_done_state;

  rw_keyspec_instance_t ksi;
  char* ksi_errstr; //Overloaded. KSI error and other error strings.
  rwdts_router_xact_stats_t stats;
  rwdts_router_wait_block_t *wait_blocks;
};

typedef struct rwdts_router_peer_reg_s {
  rwdts_router_t *dts;
  rwdts_member_registration_t *reg;
  int            retry_count;
  rw_keyspec_path_t *keyspec;
  char           *peer_rwmsgpath;
} rwdts_router_peer_reg_t;

typedef union rwdts_router_queue_closure_u {
  RWDtsXactResult_Closure execute_clo;
  RWDtsStatus_Closure status_clo;
} rwdts_router_queue_closure_t;
typedef void (*rwdts_router_msg_queued_execute_fptr) (RWDtsQueryRouter_Service *,
                                                      const RWDtsXact *,
                                                      void *,
                                                      RWDtsXactResult_Closure ,
                                                      rwmsg_request_t *);
typedef void (*rwdts_router_msg_queued_general_fptr) (RWDtsQueryRouter_Service *,
                                                      const RWDtsXactID *,
                                                      void *,
                                                      RWDtsStatus_Closure,
                                                      rwmsg_request_t *);
typedef struct rwdts_router_queue_msg_s rwdts_router_queue_msg_t;
struct rwdts_router_queue_msg_s {
  RWDtsQueryRouter_Service *mysrv;
  ProtobufCMessage         *input;
  rwdts_router_queue_closure_t clo;
  rwmsg_request_t *rwreq;
  rwdts_router_queue_msg_t *next_msg;
  void   *fn;
};

struct rwdts_router_s {
  rwmsg_endpoint_t *ep;
  rwsched_instance_ptr_t rwsched;
  const char *rwmsgpath;

  uint32_t next_table_id;
  uint32_t _pad:14;
  uint32_t rwq_ismain:1;
  uint32_t current_5s_idx:3;
  uint32_t current_idx:5;
  uint32_t current_15s_idx:3;
  uint32_t current_1m_idx:3;
  uint32_t current_5m_idx:3;
  rwsched_dispatch_queue_t rwq;
  rwsched_tasklet_ptr_t taskletinfo;
  rwtasklet_info_ptr_t rwtaskletinfo;

  RWDtsQueryRouter_Service querysrv;
  RWDtsMember_Client membercli;

  RWDtsMemberRouter_Service membersrv;

  rwmsg_srvchan_t *srvchan;
  rwmsg_clichan_t *clichan;

  rwdts_router_member_t *members; /* UTHASH of all known destinations keyed on msg path */

  rwdts_router_xact_t *xacts;        /* UTHASH of all extant transactions keyed on xact ID */

  rwdts_shard_tab_t *shard_tab;

  rwdts_api_t *apih;                /* Query/member API handle */
  rwdts_member_reg_handle_t apih_reg;

  rwvcs_zk_closure_ptr_t closure;
  int data_watcher_cb_count;
  int pend_peer_table;
  rwdts_router_queue_msg_t *queued_msgs;
  rwdts_router_queue_msg_t *queued_msgs_tail;

  /* The router will itself eventually be a member, with member apih;
     registration and dts status queries will themselves be dts
     xacts. */

  rwdts_credit_obj_t credits;
  uint64_t client_idx;
  uint64_t router_idx;
  uint64_t rtr_reg_id;
  rwsched_dispatch_source_t stat_timer;

  rwdts_member_reg_handle_t init_regidh;
  rwdts_member_reg_handle_t init_regkeyh;
  rwdts_member_reg_handle_t local_regkeyh;
  rwdts_member_reg_handle_t peer_regkeyh;
  rwdts_member_reg_handle_t init_memb;
  rwdts_member_reg_handle_t publish_state_regh;
  rw_keyspec_instance_t ksi;
  rwdts_router_stats_t stats;
  RWPB_T_MSG(RwDts_data_Dts_Member_PayloadStats) payload_stats;
#ifdef RWDTS_SHARDING
  rwdts_shard_t *rootshard;
#endif

  rwmemlog_instance_t *rwmemlog;
};



/*!
 * Create a DTS router.  The router will live at the given rwmsg path
 * in the given rwmsg endpoint, and run on the given rwsched queue.
 * The router code is agnostic as to what actual tasklet it lives in,
 * and agnostic as to wether the given rwq is the main queue or some
 * other serial queue.  Use of a default ie non-serializing queue is
 * not a good idea.
 *
 * It is intended to evolve to a model where the router lives in the
 * broker, effectively as library calls made in broker channels'
 * execution contexts.  This has significant implications that need to
 * be sorted soon.  Transactions may have an owner channel/rwq or
 * somesuch concept to simplify; response messages will naturally
 * arrive at the right channel / rwq context, and requests in re a
 * known xact either come from the query api on the local channel or
 * can be redirected there (how? async_f? meh).
 *
 * For the moment the model will be standalone tasklet with single
 * threaded state machine and as little cross-transaction state as
 * possible to facilitate the necessary evolution.
 *
 * @param ep RWMsg endpoint
 * @param rwq RWSched dispatch queue, serializing, can be mainq or other serq
 * @param rwmsgpath Pathname to register router pbapi service(s) with
 * @param rwvcs_zk to allow interaction with zk for detecting peer router
 * instantiation
 */
rwdts_router_t *rwdts_router_init(rwmsg_endpoint_t *ep,
                                  rwsched_dispatch_queue_t rwq,
                                  rwtasklet_info_ptr_t rwtasklet_info,
                                  const char *rwmsgpath,
          rwvcs_zk_module_ptr_t rwvcs_zk,
          uint64_t router_idx);

void rwdts_router_deinit(rwdts_router_t *dts);

void rwdts_router_test_register_single_peer_router(rwdts_router_t *dts, int vm_id);

rw_status_t rwdts_router_service_init(rwdts_router_t *dts);
rw_status_t rwdts_router_service_deinit(rwdts_router_t *dts);


rwdts_router_xact_t *rwdts_router_xact_create(rwdts_router_t *dts,
                                              rwmsg_request_t *exec_req,
                                              RWDtsXact *input);
void rwdts_router_xact_update(rwdts_router_xact_t *xact,
                              rwmsg_request_t *exec_req,
                              RWDtsXact *input);
void rwdts_router_xact_destroy(rwdts_router_t *dts,
                               rwdts_router_xact_t *xact);
void rwdts_router_xact_run(rwdts_router_xact_t *xact);

/* Will also process requests if rsp_req=NULL */
void rwdts_router_xact_msg_rsp(const RWDtsXactResult *xresult,
                               rwmsg_request_t *rsp_req,
                               rwdts_router_t *dts);

void rwdts_router_delete_shard_db_info(rwdts_router_t*  dts);

void
rwdts_router_member_registration_init(rwdts_router_t*                   dts,
                                      rwdts_router_member_t*            memb,
                                      rw_keyspec_path_t*                     keyspec,
                                                              const char*                       keystr,
                                      uint32_t                          flags,
                                      bool                              create);

rwdts_router_xact_t *rwdts_router_xact_lookup_by_id(const rwdts_router_t* dts,
                                                    const RWDtsXactID* id);

rwsched_dispatch_source_t
rwdts_timer_create(rwdts_router_t *dts,
                   uint64_t timeout,
                   dispatch_function_t timer_cb,
                   void *ud,
                   bool start);

const char*
rwdts_router_state_to_str(rwdts_router_xact_state_t state);


void rwdts_router_test_misc(rwdts_router_t *dts);

void rwdts_router_dump_xact_info(rwdts_router_xact_t *xact, const char *reason);

void
rwdts_router_update_pub_obj(rwdts_router_member_t *memb, uint32_t* n_obj_list,
                            rwdts_router_pub_obj_t*** pub_obj_list, rw_keyspec_path_t* ks);

void
rwdts_router_sub_obj(rwdts_memb_router_registration_t* reg, uint32_t n_obj_list,
                     rwdts_router_pub_obj_t** pub_obj_list, int* pubsubmatch, bool* ksmatch, bool *flag,
                     char *matchdesc);

void
rwdts_router_pub_obj(rwdts_memb_router_registration_t* reg, rw_keyspec_path_t* ks,
                     int* pubsubmatch, bool* ksmatch, bool *flag, char *matchdesc,
                     size_t* missat);

void
rwdts_router_xact_move_to_precomm(rwdts_router_xact_t* xact);

void
rwdts_router_remove_all_regs_for_gtest(rwdts_router_t *dts,
                                       rwdts_router_member_t *memb);

void
rwdts_shard_del(rwdts_shard_t** parent);

void rwdts_router_replay_queued_msgs(rwdts_router_t *dts);
void rwdts_router_process_queued_xact_f(void *ctx);
void rwdts_router_process_xacts_list(rwdts_router_member_t *memb);
__END_DECLS


#endif /* __RWDTS_ROUTER_H */
