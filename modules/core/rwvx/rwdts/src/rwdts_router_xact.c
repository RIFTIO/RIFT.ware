/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_router_xact.c
 * @author RIFT.io (info@riftio.com)
 * @date 2014/01/26
 * @brief DTS Router Transaction Machine
 */

#include <rw_keyspec.h>
#include <stdio.h>
#include <rwdts.h>
#include <rwdts_router.h>
#include "rw-base.pb-c.h"
#include "rw-dts.pb-c.h"
#include "rwdts_int.h"
#include "rwdts_macros.h"

#define RWDTS_EVENT_RSP_INVALID ((RWDtsEventRsp)-1)


#define RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact, str, ...) \
    int r = asprintf (&xact->ksi_errstr,"Line %d " str "", __LINE__, ##__VA_ARGS__); \
    RW_ASSERT(r > 0); \
    rwdts_router_xact_abort(xact);

static int rwdts_router_send_responses(rwdts_router_xact_t *xact,
                                       rwmsg_request_client_id_t *cliid);
static void rwdts_router_xact_sendreqs(rwdts_router_xact_t *xact,
                                       rwdts_router_xact_block_t *block);
static void rwdts_router_xact_block_prepreqs_prepare(rwdts_router_xact_t *xact,
                                                     rwdts_router_xact_block_t *block,
                                                     int blockidx);
static void rwdts_router_xact_block_done(rwdts_router_xact_t *xact,
                                         rwdts_router_xact_block_t *block);

static void rwdts_router_send_f(rwdts_router_xact_t *xact,
                                rwdts_router_xact_req_t *xreq,
                                rwdts_router_xact_block_t *block,
                                rwdts_router_xact_sing_req_t *sing_req,
                                RWDtsXact *req_xact);
static void
rwdts_router_xact_msg_rsp_internal(const RWDtsXactResult *xresult,
                                   rwmsg_request_t *rsp_req, /* our original request handle, or NULL for upcalls from the member */
                                   rwdts_router_t *dts,
                                   rwdts_router_xact_sing_req_t *sing_req);

static void rwdts_router_xact_free_req(rwdts_router_xact_t *xact);

static void
rwdts_router_update_sub_match(rwdts_router_xact_t *xact,
                              rwdts_router_xact_req_t *xreq, 
                              rwdts_shard_chunk_info_t *chunk,
                              bool subobj);

static void
rwdts_router_update_pub_match(rwdts_router_xact_t* xact, 
                              rwdts_router_xact_req_t *xreq, 
                              rwdts_shard_chunk_info_t *chunk,
                              uint32_t flags);
static rwdts_router_xact_block_t 
*rwdts_xact_block_lookup_by_id(const rwdts_router_xact_t *xact,
                               const RWDtsXactBlkID* id);

static bool
rwdts_router_check_subobj(rwdts_shard_chunk_info_t** shard, uint32_t n_shards);

static void
rwdts_free_pub_obj_list(rwdts_router_xact_t* xact);

static void
rwdts_router_update_client_for_precommit(rwdts_router_xact_t* xact, uint32_t blk);

static bool
rwdts_do_commits(rwdts_router_xact_t* xact);

static bool
rwdts_precommit_to_client(rwdts_router_xact_t* xact, uint32_t* blockid);

#define rtr_life_incr(xx) \
    if (xact->xx < 50) { \
      dts->stats.xx##_less_50_ms++; \
    } else if (xact->xx < 100) { \
      dts->stats.xx##_less_100_ms++; \
    } else if (xact->xx < 500) { \
      dts->stats.xx##_less_500_ms++; \
    } else if (xact->xx < 1000) { \
      dts->stats.xx##_less_1_sec++; \
    }else { \
      dts->stats.xx##_more_1_sec++; \
    }


static __inline__ char * rwdts_router_xact_rsp_bounce_str(rwmsg_bounce_t bcode)
{
  switch (bcode) {
    case RWMSG_BOUNCE_NOMETH:
      return "RWMSG_BOUNCE_NOMETH";
    case RWMSG_BOUNCE_NOPEER:
      return "RWMSG_BOUNCE_NOPEER";
    case RWMSG_BOUNCE_SRVRST:
      return "RWMSG_BOUNCE_SRVRST";
    default:
      return "RWMSG_BOUNCE_UNKOWN";
  }
}

static const char *
rwdts_action_to_str(RWDtsQueryAction act) {
  switch (act) {
  case RWDTS_QUERY_INVALID: return "INVALID";
  case RWDTS_QUERY_CREATE: return "CREATE";
  case RWDTS_QUERY_READ: return "READ";
  case RWDTS_QUERY_UPDATE: return "UPDATE";
  case RWDTS_QUERY_DELETE: return "DELETE";
  case RWDTS_QUERY_RPC: return "RPC";
  default: return "??ACTION??";
  }
  return "??";
}

static const char*
rwdts_xact_main_state_to_str(RWDtsXactMainState state)
{
  switch(state) {
  case RWDTS_XACT_INIT:
    return "RWDTS_XACT_INIT";
  case RWDTS_XACT_RUNNING:
    return "RWDTS_XACT_RUNNING";
  case RWDTS_XACT_COMMITTED:
    return "RWDTS_XACT_COMMITTED";
  case RWDTS_XACT_ABORTED:
    return "RWDTS_XACT_ABORTED";
  case RWDTS_XACT_FAILURE:
    return "RWDTS_XACT_FAILURE";
  default:
    return "??UNKNOWN??";
  }
}

static const char*
rwdts_block_state_to_str(rwdts_router_xact_state_t state)
{
#define RWDTS_BLOCK_STATE_TO_STR_MAX_STRLEN (64)
  switch(state) {
  case RWDTS_ROUTER_BLOCK_STATE_INIT:
    return "BS_INIT";
  case RWDTS_ROUTER_BLOCK_STATE_NEEDPREP:
    return "BS_NEEDPREP";
  case RWDTS_ROUTER_BLOCK_STATE_RUNNING:
    return "BS_RUNNING";
  case RWDTS_ROUTER_BLOCK_STATE_DONE:
    return "BS_DONE";
  case RWDTS_ROUTER_BLOCK_STATE_RESPONDED:
    return "BS_RESPONDED";
  default:
    return "??BS_UNKNOWN??";
  }
}

const char*
rwdts_router_state_to_str(rwdts_router_xact_state_t state)
{

#define RWDTS_ROUTER_STATE_TO_STR_MAX_STRLEN (64)
  switch(state) {
    case RWDTS_ROUTER_XACT_INIT:
      return "ST_INIT";
    case RWDTS_ROUTER_XACT_PREPARE:
      return "ST_PREPARE";
    case RWDTS_ROUTER_XACT_COMMIT:
      return "ST_COMMIT";
    case RWDTS_ROUTER_XACT_PRECOMMIT:
      return "ST_PRECOMMIT";
    case RWDTS_ROUTER_XACT_ABORT:
      return "ST_ABORT";
    case RWDTS_ROUTER_XACT_SUBCOMMIT:
      return "ST_SUBCOMMIT";
    case RWDTS_ROUTER_XACT_SUBABORT:
      return "ST_SUBABORT";
    case RWDTS_ROUTER_XACT_DONE:
      return "ST_DONE";
    case RWDTS_ROUTER_XACT_TERM:
      return "ST_TERM";
    case RWDTS_ROUTER_XACT_QUEUED:
      return "ST_QUEUED";
    default:
      RW_ASSERT_MESSAGE(0, "Unknown State %u", state);
  }
  return "??ST_UNKNOWN??";
}

static const char*
rwdts_router_event_to_str(rwdts_member_xact_evt_t evt)
{
  switch(evt) {
    case RWDTS_EVT_PREPARE:
      return "EVT_PREPARE";
    case RWDTS_EVT_PRECOMMIT:
      return "EVT_PRECOMMIT";
    case RWDTS_EVT_COMMIT:
      return "EVT_COMMIT";
    case RWDTS_EVT_ABORT:
      return "EVT_ABORT";
    default:
      return "??EVT_UNKNOWN??";
  }
  return "??EVT_UNKNOWN??";
}

/* How many clients are done? */
static int rwdts_router_xact_rwmsg_tab_done(rwdts_router_xact_t *xact) {
  int ct = 0;
  int i;
  for (i=0; i<xact->rwmsg_tab_len; i++) {
    if (xact->rwmsg_tab[i].done) {
      ct++;
    }
  }
  return ct;
}

/* How many xreqs are sendable? */
static int rwdts_router_xact_blocks_sendable_ct(rwdts_router_xact_t *xact) {
  int ct = 0;
  int i;
  for (i=0; i<xact->n_xact_blocks; i++) {
    if (xact->xact_blocks[i]->block_state != RWDTS_ROUTER_BLOCK_STATE_RESPONDED) {
      ct += xact->xact_blocks[i]->req_ct;
      ct -= xact->xact_blocks[i]->req_sent;
    }
  }
  return ct;
}

/* How many blocks are in the given state? */
static int rwdts_router_xact_blocks_state_ct(rwdts_router_xact_t *xact,
                                             enum rwdts_router_block_state_e state) {
  int ct = 0;
  int i;
  for (i=0; i<xact->n_xact_blocks; i++) {
    ct += ((xact->xact_blocks[i]->block_state == state) ? 1 : 0);
  }
  return ct;
}

void rwdts_router_xact_block_newstate(rwdts_router_xact_t *xact,
                                      rwdts_router_xact_block_t *block,
                                      enum rwdts_router_block_state_e newstate) {
  RW_ASSERT(newstate >= RWDTS_ROUTER_BLOCK_STATE_INIT);
  RW_ASSERT(newstate <= RWDTS_ROUTER_BLOCK_STATE_RESPONDED);
  if (block->block_state != newstate) {
    if (xact->dbg && getenv("RWMSG_DEBUG")){
      RWDtsTracerouteEnt ent;
      
      rwdts_traceroute_ent__init(&ent);
      ent.line = __LINE__;
      ent.func = (char*)__PRETTY_FUNCTION__;
      ent.what = RWDTS_TRACEROUTE_WHAT_RTR_BLOCK_STATE;
      ent.dstpath = (char *)rwdts_block_state_to_str(newstate);
      ent.srcpath = (char *)rwdts_block_state_to_str(block->block_state);
      ent.block = block->xbreq;
      ent.has_num_match_members = 1;
      ent.num_match_members = block->req_ct;
      
      rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
    }
    block->block_state = newstate;
    xact->blocks_advanced = TRUE;
  }
  RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A,
	   "New block state",
	   RWMEMLOG_ARG_PRINTF_INTPTR("bidx %"PRIdPTR, block->xact_blocks_idx),
	   RWMEMLOG_ARG_ENUM_FUNC(rwdts_block_state_to_str, "", block->block_state));
}

static void rwdts_router_xact_newstate(rwdts_router_xact_t *xact,
                                       rwdts_router_xact_state_t state) {
  xact->state = state;

  xact->fsm_trace[xact->curr_trace_index] = state;
  RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A, 
	   "New xact state", 
	   RWMEMLOG_ARG_ENUM_FUNC(rwdts_router_state_to_str, "", state));
  xact->curr_trace_index++;
  if (xact->curr_trace_index == MAX_RTR_TRACE_EVT) {
    xact->curr_trace_index = 0;
  }
}

void rwdts_router_xact_abort(rwdts_router_xact_t *xact)
{
  RW_ASSERT_MESSAGE(0,"Router Abort Xact with reason %s", xact->ksi_errstr);
  /// Code to abort xact on Router
}
bool
rwdts_router_is_transactional(RWDtsXactBlock *block)
{

  int i;
  for (i=0; i < block->n_query; i++) {
    if ((block->query[i]->action != RWDTS_QUERY_READ) &&
        !(block->query[i]->flags & RWDTS_XACT_FLAG_NOTRAN)) {
      return true;
    }
  }
  return false;
}

rwsched_dispatch_source_t
rwdts_timer_create(rwdts_router_t *dts,
                   uint64_t timeout,
                   dispatch_function_t timer_cb,
                   void *ud,
                   bool start)
{
  /* The originating request, which we respond to at the very end of
     the transaction. */

  rwsched_dispatch_source_t timer = NULL;
  timer = rwsched_dispatch_source_create(dts->rwtaskletinfo->rwsched_tasklet_info,
                                         RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                         0,
                                         0,
                                         dts->rwq);
  rwsched_dispatch_source_set_event_handler_f(dts->rwtaskletinfo->rwsched_tasklet_info,
                                              timer,
                                              timer_cb);
  rwsched_dispatch_set_context(dts->rwtaskletinfo->rwsched_tasklet_info,
                               timer,
                               ud);

  rwsched_dispatch_source_set_timer(dts->rwtaskletinfo->rwsched_tasklet_info,
                                    timer,
                                    dispatch_time(DISPATCH_TIME_NOW, timeout),
                                    timeout,
                                    0);
  if(start)
  {
    rwsched_dispatch_resume(dts->rwtaskletinfo->rwsched_tasklet_info,
                            timer);
  }
  return timer;
}

static void rwdts_router_incr_xact_life_stats(rwdts_router_t *dts,
                                              int life_time)
{
  if (life_time < 50) {
    dts->stats.xact_life_less_50_ms++;
  } else if (life_time < 100) {
    dts->stats.xact_life_less_100_ms++;
  } else if (life_time < 500) {
    dts->stats.xact_life_less_500_ms++;
  } else if (life_time < 1000) {
    dts->stats.xact_life_less_1_sec++;
  }else {
    dts->stats.xact_life_more_1_sec++;
  }
  dts->stats.topx_latency_xact_count++;
  dts->stats.topx_latency_xact_15s_count++;
  dts->stats.topx_latency_xact_1m_count++;
  dts->stats.topx_latency_xact_5m_count++;
  dts->stats.topx_latency_inc += life_time;
  dts->stats.topx_latency_15s_inc += life_time;
  dts->stats.topx_latency_1m_inc += life_time;
  dts->stats.topx_latency_5m_inc += life_time;
  return;
}

static void rwdts_router_dump_block_info(rwdts_router_xact_block_t *block, char **retlogdump)
{
  char *logdump = *retlogdump;
  RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump,
          "  Block[%d] state=%s evt=%s req_ct=%u req_sent/out/done=%u/%u/%u req_done_ack/na/nack/err=%u/%u/%u/%u",
          block->xact_blocks_idx,
          rwdts_router_state_to_str(block->evt),
          rwdts_block_state_to_str(block->block_state),
          block->req_ct, block->req_sent, block->req_outstanding, block->req_done, block->req_done_ack, block->req_done_na, block->req_done_nack, block->req_done_err);

  if (block->xbreq) {
    RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump,
            "    RWDtsXactBlockRequest: blockid=%lu/%lu/%u n_query=%d flags=%x",
            block->blockid.routeridx,
            block->blockid.clientidx,
            block->blockid.blockidx,
            (int)block->xbreq->n_query,
            block->xbreq->flags);
    int i;
    for (i=31; i>=0; i--) {
      static const char *flagdesc[32] = {
        "NOTRAN",
        "TRANS_CRUD",
        "UPDATE_CREDITS",
        "END",
        "EXECNOW",
        "WAIT",
        "IMM_BOUNCE",
        "REG",
        "PEER_REG",
        "BLOCK_MERGE",
        "STREAM",
        "TRACE",
        "SOLICIT_RSP",
        "ADVISE",
        "DEPTH_FULL",
        "DEPTH_OBJECT",
        "DEPTH_LISTS",
        "DEPTH_ONE",
        "RETURN_PAYLOAD",
        "ANYCAST",
        "REPLACE",
      };
      if (block->xbreq->flags & (1<<i)) {
        RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump,
                                      "|%s",
                                      flagdesc[i]);
      }
    }
    //RWDTS_PRINTF("\n");
    for (i=0; i<block->xbreq->n_query; i++) {
      const RWDtsQuery *query = block->xbreq->query[i];
      const char *action = rwdts_action_to_str(query->action);
      const char *kstr = "??KEY??";
      if (query->key && query->key->keystr) {
        kstr = query->key->keystr;
      }
      RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump,
              "      RWDtsQuery[%d] %s%s%s keystr='%s'%s",
              i,
              action,
              (query->flags & RWDTS_XACT_FLAG_ADVISE) ? "/ADVISE" : "",
              (query->flags & RWDTS_XACT_FLAG_ANYCAST) ? "/ANYCAST" : "",
              kstr,
              query->payload ? " w/payload" : "");
    }
  }
  *retlogdump = logdump;

  /* We keep all xreqs around from block execution.  So we could show
     each single req that was sent, with state, elapsed time, etc.
     Plus same for event blocks. */
}

void rwdts_router_dump_xact_info(rwdts_router_xact_t *xact,
                                 const char *reason)
{
  RW_ASSERT(xact);
  int blockidx;
  char *logdump = NULL;

  RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump, "\nTransaction Info Dump %s", reason);
  RWDTS_ROUTER__LOG_PRINT_TEMPL (logdump, "xact @%p xact[%s], state=%s router_xact_count=%u ", 
                                 xact, (char*)xact->xact_id_str,
                                 rwdts_router_state_to_str(xact->state),
                                 HASH_COUNT(xact->dts->xacts));
  
  PRINT_XACT_INFO_ELEM(rwmsg_tab_len, logdump);

  int tab_count =0;
  for (;tab_count < xact->rwmsg_tab_len;tab_count++) {
    RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump, " [%s]", xact->rwmsg_tab[tab_count].client_path);
  }

  PRINT_XACT_INFO_ELEM(n_xact_blocks, logdump);
  PRINT_XACT_INFO_ELEM(query_serial, logdump);
  PRINT_XACT_INFO_ELEM(n_req, logdump);
  PRINT_XACT_INFO_ELEM(req_sent, logdump);
  PRINT_XACT_INFO_ELEM(req_done, logdump);
  PRINT_XACT_INFO_ELEM(req_outstanding, logdump);
  PRINT_XACT_INFO_ELEM(req_nack, logdump);
  PRINT_XACT_INFO_ELEM(req_err, logdump);

  PRINT_XACT_INFO_ELEM(do_commits, logdump);
  PRINT_XACT_INFO_ELEM(abrt, logdump);
  PRINT_XACT_INFO_ELEM(ended, logdump);
  PRINT_XACT_INFO_ELEM(pending_del, logdump);
  PRINT_XACT_INFO_ELEM(more_sent, logdump);

  for (blockidx = 0; blockidx < xact->n_xact_blocks && xact->xact_blocks ; blockidx++)
  {
    rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];
    rwdts_router_dump_block_info(block, &logdump);
  }
  int reqidx;
  int na_count=0;
  for (reqidx = 0; reqidx < xact->n_req; reqidx++) {
    int keystr_shown = FALSE; /* req[j] is all for one block? */
    if (xact->req) {
      rwdts_router_xact_req_t *xreq = &xact->req[reqidx];
      rwdts_router_xact_sing_req_t *sing_req, *tmp_sing_req;
      HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
        if (sing_req->xreq_state == RWDTS_ROUTER_QUERY_NA)
        {
          na_count++;
          continue;
        }
        RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump,
              "  state='%s' blockidx=%d qidx=%u qser=%u msgpath='%s'",
              sing_req->xreq_state == RWDTS_ROUTER_QUERY_ERR ? "ERR"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_SENT ? "SENT"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_NONE ? "(none)"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_UNSENT ? "UNSENT"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_QUEUED ? "QUEUED"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_ASYNC ? "ASYNC"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_ACK ? "ACK"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_NA ? "NA"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_NACK ? "NACK"
              : sing_req->xreq_state == RWDTS_ROUTER_QUERY_INTERNAL ? "INTERNAL"
              : "????",
              xreq->block_idx,
              xreq->query_idx,
              xreq->query_serial,
              sing_req->membnode->member->msgpath); 
        if (xreq->keystr && !keystr_shown) {
          RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump, "           keystr='%s'", xreq->keystr);
          keystr_shown = TRUE;
        }
      }
    }
  }
  RWDTS_ROUTER__LOG_PRINT_TEMPL(logdump, "Count of NA members = %d %d \n", na_count, xact->n_req);

#if 1
  RWDTS_ROUTER_LOG_EVENT(
      xact->dts, 
      XactDumpTimeout, 
      logdump,
      xact->xact_id_str);
#else
  fprintf (stderr, "[[%s]]] -- BAIJU log is logdump --- [[%s]]\n",
           xact->xact_id_str,
           logdump);
#endif
  RW_FREE(logdump);
  return;
}

static void rwdts_router_block_xbres_init(rwdts_router_xact_t *xact,
                                          rwdts_router_xact_block_t *block) {
  RW_ASSERT(!block->xbres);

  block->xbres = calloc(1, sizeof(RWDtsXactBlockResult));
  rwdts_xact_block_result__init(block->xbres);
  block->xbres->has_blockidx = TRUE;
  block->xbres->blockidx = block->blockid.blockidx; /* client assigned */
}

static void rwdts_router_async_send_timer_cancel(rwdts_router_xact_t *xact, int rwmsg_tab_idx) {
  RW_ASSERT(rwmsg_tab_idx >= 0);
  RW_ASSERT(rwmsg_tab_idx < xact->rwmsg_tab_len);
  rwdts_router_xact_rwmsg_ent_t *rwmsg_ent = &xact->rwmsg_tab[rwmsg_tab_idx];

  if (rwmsg_ent->async_timer) {
    rwsched_dispatch_source_cancel(xact->dts->rwtaskletinfo->rwsched_tasklet_info,
                                   rwmsg_ent->async_timer);
    rwsched_dispatch_source_release(xact->dts->rwtaskletinfo->rwsched_tasklet_info,
                                    rwmsg_ent->async_timer);
    rwmsg_ent->async_timer = NULL;
  }

  if (rwmsg_ent->async_info) {
    RW_FREE_TYPE(rwmsg_ent->async_info, rwdts_router_async_info_t);
    rwmsg_ent->async_info = NULL;
  }
}

static void rwdts_router_async_send_timer(void *ctx)
{
  rwdts_router_async_info_t *async_info = (rwdts_router_async_info_t *)ctx;
  RW_ASSERT(async_info);
  RW_ASSERT_TYPE(async_info, rwdts_router_async_info_t);

  rwdts_router_xact_t *xact = async_info->xact;
  int rwmsg_idx = async_info->idx;

  RW_ASSERT(xact->rwmsg_tab[rwmsg_idx].async_info == async_info);

  /* FIXME, move into send_responses? */
    //    rwdts_calc_log_payload(&xact->dts->payload_stats, RW_DTS_MEMBER_ROLE_ROUTER,
    //                           RW_DTS_XACT_LEVEL_SINGLERESULT, xact->xres[blockidx]);

  rwdts_router_async_send_timer_cancel(xact, rwmsg_idx);
  async_info = NULL;

  /* Apparently we owe this guy a response */
  rwdts_router_send_responses(xact, &xact->rwmsg_tab[rwmsg_idx].rwmsg_cliid);
}

static void rwdts_router_async_send_timer_start(rwdts_router_xact_t *xact, int rwmsg_tab_idx) {
  RW_ASSERT(rwmsg_tab_idx >= 0);
  RW_ASSERT(rwmsg_tab_idx < xact->rwmsg_tab_len);
  rwdts_router_xact_rwmsg_ent_t *rwmsg_ent = &xact->rwmsg_tab[rwmsg_tab_idx];

  if (rwmsg_ent->async_timer || !rwmsg_ent->rwmsg_req) {
    return;
  }

  if ((xact->state != RWDTS_ROUTER_XACT_PREPARE)
      && (xact->state != RWDTS_ROUTER_XACT_INIT))
  {
    return;
  }

  rwmsg_ent->async_info = RW_MALLOC0_TYPE(sizeof(rwdts_router_async_info_t), rwdts_router_async_info_t);
  rwmsg_ent->async_info->xact = xact;
  rwmsg_ent->async_info->idx = rwmsg_tab_idx;
  rwmsg_ent->async_timer = rwdts_timer_create(xact->dts,
                                              RWDTS_TIMEOUT_QUANTUM_MULTIPLE(xact->dts->apih,
                                                                             RWDTS_ROUTER_ASYNC_TIMEOUT_SCALE),
                                              rwdts_router_async_send_timer,
                                              rwmsg_ent->async_info,
                                              TRUE);
}

static void rwdts_router_xact_timer(void *ctx)
{
  rwdts_router_xact_t *xact = (rwdts_router_xact_t *)ctx;
  int reqidx;
  rwdts_router_xact_req_t *xreq;
  rwdts_router_xact_sing_req_t *sing_req= NULL, *tmp_sing_req = NULL;
  RWDtsXactResult xresult;
  RWDtsXactBlockResult bres;
  RWDtsXactBlockResult *temp[2];
  rwmsg_request_t *req;
  rwdts_router_dump_xact_info(xact, "rwdts_router_xact_timer called xact");

  //FIXME
  // stuck async, fake a nack from that person
  // stuck responded without end=1, abort
  // ADD ERROR DETAIL TO THE XACT SO THE CLIENT WILL SEE WHAT HAPPENED
  if (xact->dts->xact_timer_enable){
    //find a sing_req which can be used to synthesize a response or a sing_req
    //for which there has not been a response...
    switch(xact->state){
      case RWDTS_ROUTER_XACT_INIT:
        break;
      case RWDTS_ROUTER_XACT_PREPARE:
      case RWDTS_ROUTER_XACT_PRECOMMIT:
      case RWDTS_ROUTER_XACT_COMMIT:
      case RWDTS_ROUTER_XACT_ABORT:
        if (xact->req) {
          for (reqidx = 0; reqidx < xact->n_req; reqidx++) {
            xreq = &xact->req[reqidx];
            HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
              if (sing_req->req){
                //cancel the request
                rwdts_xact_result__init(&xresult);
                xresult.n_new_blocks = 0;
                xresult.n_result = 0;
                xresult.has_evtrsp = 1;
                xresult.evtrsp = RWDTS_EVTRSP_CANCELLED;
                if (xact->state == RWDTS_ROUTER_XACT_PREPARE){
                  xresult.result = &temp[0];
                  xresult.result[0] = &bres;
                  xresult.n_result = 1;
                  rwdts_xact_block_result__init(&bres);
                  bres.has_evtrsp = 1;
                  bres.evtrsp = RWDTS_EVTRSP_CANCELLED;
                }
                /*Store the req so that it can be cancelled after prepending to get a response*/
                req = sing_req->req;               
                rwdts_router_xact_msg_rsp_internal(&xresult,
                                                   req, xact->dts, sing_req);
                rwmsg_request_cancel(req);
              }
            }
          }
        }
        break;
      case RWDTS_ROUTER_XACT_SUBCOMMIT:
      case RWDTS_ROUTER_XACT_SUBABORT:
        //Still to be implemented
        RW_ASSERT(0);
        break;
      case  RWDTS_ROUTER_XACT_QUEUED:
        //Should the timer be restarted here??? TBD
        break;
      case RWDTS_ROUTER_XACT_DONE:
        {
          int i;
          //Move into the term state. Could not send all the responses in time..or the whole transaction took longer than usual
          if (xact->do_commits) {
            /*The responses to all the clients did not go through fine either becuase we have tasklets
              recovering. Walk through all the clients to check which of the clients have not been
              responded to*/
            for (i=0; i<xact->rwmsg_tab_len; i++) {
              if (!xact->rwmsg_tab[i].done) {
                /*
                  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact,
                  DtsrouterXactError,
                  RWLOG_ATTR_SPRINTF("Unable to send response to %s", xact->rwmsg_tab[i].client_path));
                */
              }
            }
            //terminate the xact..
            rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_TERM);
            rwdts_router_xact_run(xact);
            //now the xact must have been terminated
            xact = NULL;
          }else{
            /*We still have blocks that are not in responded state. If this is the case, why
              is the xact in the DONE state and not in the prepare state??. Assert here*/
            RW_ASSERT(0);
          }
        }
        break;
      case RWDTS_ROUTER_XACT_TERM:
        RW_ASSERT(0);
        break;
      default:
        RW_ASSERT(0);
        break;
    }
  }
}



static void
rwdts_rtr_xact_err_cb(rw_keyspec_instance_t *ksi,
                      const char *msg)
{
  rwdts_router_xact_t *xact = (rwdts_router_xact_t *)ksi->user_data;
  if (xact)
  {
    xact->ksi_errstr = strdup(msg);
  }
}
static void
rwdts_rtr_xact_ksi_deinit(rwdts_router_xact_t *xact)
{
  if(xact->ksi_errstr)
  {
    free(xact->ksi_errstr);
  }
  xact->ksi_errstr = NULL;
}
static void
rwdts_rtr_xact_ksi_init(rwdts_router_xact_t *xact)
{
  memset(&xact->ksi, 0, sizeof(rw_keyspec_instance_t));
  xact->ksi.error = rwdts_rtr_xact_err_cb;
  xact->ksi.user_data = xact;
}



/*This routine is called whenever a single request needs to be sent out to the given
  member or member node. If the membernode is given, it must be a precommit/commit/abort
  request. If the membernode is not given, then it must be a prepare request
  returns 0 if no allocation was made.
  returns 1 if an allocation was made
*/
static rwdts_router_xact_sing_req_t*
rwdts_router_create_sing_req(rwdts_router_xact_req_t *xreq,
                             rwdts_router_member_t *member,
                             rwdts_router_member_node_t *membnode)
{
  rwdts_router_xact_sing_req_t *sing_req;
  rwdts_router_xact_t* xact = xreq->xact;
  rwdts_router_xact_block_t *block = xact->xact_blocks[xreq->block_idx];
  
  RW_ASSERT(xact);
  HASH_FIND(hh_memb, xreq->sing_req, member->msgpath,
            strlen(member->msgpath), sing_req);
  if (!sing_req) {
    sing_req = (rwdts_router_xact_sing_req_t *)RW_MALLOC0(sizeof(rwdts_router_xact_sing_req_t));
    
    if (membnode){
      /* Likely not in prepare phase*/
      sing_req->memb_xfered = 1;
    }else{
      /* Likely in the prepare phase*/
      membnode = RW_MALLOC0(sizeof(rwdts_router_member_node_t));
      RW_ASSERT(membnode);
      membnode->member = member;
      sing_req->memb_xfered = 0;
    }
    sing_req->membnode = membnode;
    sing_req->xreq_idx = xreq->index;
    sing_req->xact = xreq->xact;
    HASH_ADD_KEYPTR(hh_memb, xreq->sing_req, membnode->member->msgpath, strlen(membnode->member->msgpath), sing_req);
    xreq->sing_req_ct++;
    RW_DL_PUSH(
        &sing_req->membnode->member->sing_reqs, 
        sing_req, 
        memb_element);
    sing_req->xreq_state  = RWDTS_ROUTER_QUERY_UNSENT;
    block->req_ct++;
  }
  
  return sing_req;
}


static void
rwdts_router_delete_sing_req(rwdts_router_xact_t *xact,
                             rwdts_router_xact_sing_req_t *sing_req, int update_count)
{
  rwdts_router_xact_req_t *xreq  = &xact->req[sing_req->xreq_idx];
  rwdts_router_xact_block_t *block = xact->xact_blocks[xreq->block_idx];
  
  RW_ASSERT(xreq->xact == xact);
  RW_DL_REMOVE(
      &sing_req->membnode->member->sing_reqs, 
      sing_req, 
      memb_element);
  
  RWDTS_FREE_CREDITS(xact->dts->credits, sing_req->credits);
   if (sing_req->req) {
     rwmsg_request_cancel(sing_req->req);
     sing_req->req = NULL;
   }
   
  HASH_DELETE(hh_memb, xreq->sing_req, sing_req);
  if (!sing_req->memb_xfered) {
    if (sing_req->membnode) {
      RW_FREE(sing_req->membnode);
      sing_req->membnode = NULL;
    }
  }
  RW_FREE(sing_req);
  xreq->sing_req_ct--;
  if (update_count){
    block->req_ct--;
  }
  return;
}





rwdts_router_xact_t *rwdts_router_xact_create(rwdts_router_t *dts,
                                              rwmsg_request_t *exec_req,
                                              RWDtsXact *input)
{
  rwdts_router_xact_t *xact = NULL;


  xact = RW_MALLOC0_TYPE(sizeof(*xact), rwdts_router_xact_t);
  xact->dts = dts;
  dts->stats.xact_ct++;
  dts->stats.total_xact++;

  /*Copy the flags for the xact. These are only the xact flags and not the per query flags*/
  xact->flags = input->flags;
  
  gettimeofday(&xact->tv_trace, NULL);

  xact->do_commits = FALSE;        /* turned on by various query types */
  RW_DL_INIT(&xact->queued_members);


  /* The originating request, which we respond to at the very end of
     the transaction. */
  xact->timer = rwdts_timer_create(dts,
                                   dts->xact_timer_interval,
                                   rwdts_router_xact_timer,
                                   xact,
                                   TRUE);

  if (input->dbg && (input->dbg->dbgopts1&RWDTS_DEBUG_TRACEROUTE)) {
    /* Make a tracert object in our xact structure to attach to the final response */
    xact->dbg = (RWDtsDebug*)RW_MALLOC0(sizeof(RWDtsDebug));
    rwdts_debug__init(xact->dbg);
    xact->dbg->dbgopts1 |= RWDTS_DEBUG_TRACEROUTE;
    xact->dbg->has_dbgopts1 = TRUE;
    if (!xact->dbg->tr) {
      rwdts_dbg_add_tr(xact->dbg, NULL);
    }
  }

  rwdts_xact_id_memcpy(&xact->id, &input->id);
  RWDTS_ADD_PBKEY_TO_HASH(hh, dts->xacts, (&xact->id), router_idx, xact);

  xact->rwml_buffer = rwmemlog_instance_get_buffer(dts->rwmemlog, "Router Xact", dts->stats.xact_ct);

  {
    rwdts_xact_id_str(&xact->id, xact->xact_id_str, sizeof(xact->xact_id_str));
    RWMEMLOG(&xact->rwml_buffer, 
	     RWMEMLOG_MEM0, 
	     "Xact create",
	     RWMEMLOG_ARG_STRNCPY(sizeof(xact->xact_id_str), xact->xact_id_str));
    RWMEMLOG(&xact->dts->rwml_buffer, 
	     RWMEMLOG_MEM0,
             "Xact Start",
             RWMEMLOG_ARG_STRNCPY(sizeof(xact->xact_id_str), xact->xact_id_str));
  }

  rwdts_router_xact_update(xact, exec_req, input);

  if (input->has_flags) {
    if ((input->flags & RWDTS_XACT_FLAG_WAIT)) {
      xact->ended = TRUE;
      RWDTS_RTR_ADD_TR_ENT_ENDED(xact);   
    }
  }

  gettimeofday(&xact->start_time, NULL);
  rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_INIT);

  /* And we're off, running (at least) the first block... */
  rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_PREPARE);
  rwdts_router_xact_block_newstate(xact, xact->xact_blocks[0], RWDTS_ROUTER_BLOCK_STATE_NEEDPREP);
  rwdts_rtr_xact_ksi_init(xact);   // WTF FIXME
  rwdts_router_xact_run(xact);

  dts->stats.topx_begin++;
  return xact;
}

void rwdts_router_xact_destroy(rwdts_router_t *dts,
                               rwdts_router_xact_t *xact)
{
  int i;
  struct timeval end_time;
  int total_time_m_secs = 0;

                         
  gettimeofday(&end_time, NULL);

  total_time_m_secs = ((end_time.tv_sec - xact->start_time.tv_sec) * 1000+
                       (end_time.tv_usec - xact->start_time.tv_usec)/1000);

  rwdts_router_incr_xact_life_stats(dts, total_time_m_secs);

#if 0 /* FIXME does not compile */
  RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, RouterStateDestroy, total_time_m_secs, "_router_xact_destroy");
#endif

  {
    RWMEMLOG(&xact->rwml_buffer, 
             RWMEMLOG_MEM0, 
             "Xact destroy");
    RWMEMLOG(&xact->dts->rwml_buffer, 
             RWMEMLOG_MEM0,
             "Xact End",
             RWMEMLOG_ARG_STRNCPY(sizeof(xact->xact_id_str), xact->xact_id_str));
  }

  rwmemlog_buffer_return_all(xact->rwml_buffer);
  xact->rwml_buffer = NULL;
  
  uint64_t member_ct = xact->xact_union ? HASH_CNT(hh_xact, xact->xact_union) : 0;
  dts->stats.topx_member_histo[RWDTS_ROUTER_STATS_HISTO64_IDX(member_ct)]++;
  
  if (xact->timer) {
    rwsched_dispatch_source_cancel(dts->rwtaskletinfo->rwsched_tasklet_info,
                                   xact->timer);
    rwsched_dispatch_source_release(dts->rwtaskletinfo->rwsched_tasklet_info,
                                    xact->timer);
    xact->timer = NULL;
  }
  HASH_DELETE(hh, dts->xacts, xact);

  rwdts_router_xact_free_req(xact);
  xact->n_req = 0;
  if (xact->req) {
    free(xact->req);    // free the array of blocks req[]
    xact->req = NULL;
  }
  
  
  for (i = 0; i < xact->n_xact_blocks; i++) {
    rwdts_router_xact_block_t *block = xact->xact_blocks[i];

    if (block->xbreq) {
      protobuf_c_message_free_unpacked(NULL, &block->xbreq->base);
      block->xbreq = NULL;
    }

    if (block->xbres) {
      protobuf_c_message_free_unpacked(NULL, &block->xbres->base);
      block->xbres = NULL;
    }

    /* May not be present, as for event tracking blocks */
    if (block->blockid.base.descriptor) {
      protobuf_c_message_free_unpacked_usebody(NULL, &block->blockid.base);
    }

    RW_FREE_TYPE(block, rwdts_router_xact_block_t);
    xact->xact_blocks[i] = NULL;
  }
  free(xact->xact_blocks);
  xact->xact_blocks = NULL;

  /* Clear out cached destination data */
  while (RW_DL_LENGTH(&xact->queued_members)) {
    rwdts_router_queued_xact_t *queued_xact = RW_DL_POP(&xact->queued_members, rwdts_router_queued_xact_t, queued_member_element);
    RW_ASSERT_TYPE(queued_xact, rwdts_router_queued_xact_t);
    RW_ASSERT(queued_xact->xact == xact);
    rwdts_router_member_t *member = queued_xact->member;
    RW_ASSERT_TYPE(member, rwdts_router_member_t);
    RW_DL_REMOVE(&member->queued_xacts, queued_xact, queued_xact_element);
    RW_FREE_TYPE(queued_xact, rwdts_router_queued_xact_t);
  }

#ifdef RWDTS_SHARDING
  RW_ASSERT(xact->destset_ct == 0);
  RW_ASSERT(!xact->destset);
#endif
  for (i=0; i<xact->destset_ct; i++) {
    xact->destset[i].member = NULL;
  }
  
  free(xact->destset);
  xact->destset = NULL;

  rwdts_router_member_node_t *memb=NULL, *membtmp=NULL;
  HASH_ITER(hh_xact, xact->xact_union, memb, membtmp) {
    HASH_DELETE(hh_xact, xact->xact_union, memb);
    RW_FREE(memb);
  }
#ifdef SUB_XACT_SUPPRT
  HASH_ITER(hh_block, xact->block_union, memb, membtmp) {
    HASH_DELETE(hh_block, xact->block_union, memb);
  }
#endif

  for (i=0; i<xact->rwmsg_tab_len; i++) {
    rwdts_router_xact_rwmsg_ent_t *ent = &xact->rwmsg_tab[i];
    rwdts_router_async_send_timer_cancel(xact, i);
    RW_FREE(ent->client_path);
  }
  RW_FREE(xact->rwmsg_tab);
  xact->rwmsg_tab = NULL;

  if (xact->dbg) {
    protobuf_c_message_free_unpacked(NULL, &xact->dbg->base);
    xact->dbg = NULL;
  }

  // FIXME can't be doing this in every xact, can we?
  rwdts_rtr_xact_ksi_deinit(xact);

  rwdts_free_pub_obj_list(xact);

  rwdts_router_add_stale_xact(xact->dts, xact, &end_time);
  
  RW_FREE_TYPE(xact, rwdts_router_xact_t);
  xact = NULL;

  RW_ASSERT(dts->stats.xact_ct >= 1);
  dts->stats.xact_ct--;
}


/*This routine is called whenever an xact needs to be queued due to a
  member restarting and the member is reconcilable.*/
static void rwdts_router_xact_queue_restart(
    rwdts_router_xact_sing_req_t *sing_req,
    rwdts_router_xact_t *xact,
    bool push)
{
  rwdts_router_queued_xact_t *queued_xact;

  RW_ASSERT(sing_req->membnode->member->wait_restart);

  if ((sing_req->xreq_state == RWDTS_ROUTER_QUERY_QUEUED) ||
      (xact->state == RWDTS_ROUTER_XACT_QUEUED)){
    /*The sing_req and xact are already queued*/
    return;
  }
  queued_xact = 
      RW_MALLOC0_TYPE(sizeof(rwdts_router_queued_xact_t), 
                      rwdts_router_queued_xact_t);
  RW_ASSERT_TYPE(queued_xact, rwdts_router_queued_xact_t);
  queued_xact->xact = xact;
  queued_xact->member = sing_req->membnode->member;
  if (push) {
    RW_DL_PUSH(
        &sing_req->membnode->member->queued_xacts, 
        queued_xact, 
        queued_xact_element);
    RW_DL_PUSH(
        &xact->queued_members, 
        queued_xact, 
        queued_member_element);
    /*If the member restart is detected through the srvrst and not when sending out prepare,
      we need to mark the sing_req as not sent so that there is a retry and the xact can
      advance to the next state*/
    sing_req->sent_before = FALSE;
  }
  else {
    RW_DL_ENQUEUE(
        &sing_req->membnode->member->queued_xacts, 
        queued_xact, 
        queued_xact_element);
    RW_DL_ENQUEUE(
        &xact->queued_members, 
        queued_xact, 
        queued_member_element);
    /*This request should have never been sent before*/
    RW_ASSERT(sing_req->sent_before == FALSE);
  }
  sing_req->xreq_state = RWDTS_ROUTER_QUERY_QUEUED;
  xact->state = RWDTS_ROUTER_XACT_QUEUED;
  
  return;
}


/*This routine is called whenever there is a bounce no no result from the  peer*/
static void
rwdts_router_xact_query_bounce_rsp(rwdts_router_xact_t *xact,
                                   rwmsg_request_t *rsp_req, /* our original request handle, or NULL for upcalls from the member */
                                   rwdts_router_xact_req_t *xreq,
                                   uint32_t blockidx,
                                   rwdts_router_xact_sing_req_t *sing_req)
{
  rwmsg_bounce_t bcode;
  rwdts_router_t *dts;
  rwdts_router_xact_block_t *block;
  rwdts_router_member_node_t *membnode;
  
  RW_ASSERT(xact);
  RW_ASSERT(xreq);
  RW_ASSERT(rsp_req);
  RW_ASSERT(blockidx >= 0);
  RW_ASSERT(blockidx < xact->n_xact_blocks);
  dts = xact->dts;
  
  bcode = rwmsg_request_get_response_bounce(rsp_req);
  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            rsp_req, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Xact Rsp)");
  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("rwdts got and correlated a bounce code=%d response from member '%s' query_idx=%u query_seral=%u",
                                                 (int)bcode, sing_req->membnode->member->msgpath, xreq->query_idx, xreq->query_serial));
  
  block = xact->xact_blocks[blockidx];
  RW_ASSERT(block);
  if (rsp_req && xact->dbg) {
    if (sing_req->tracert_idx >= 0) {
      RW_ASSERT(sing_req->tracert_idx >= 0);
      RW_ASSERT(sing_req->tracert_idx < xact->dbg->tr->n_ent);
      xact->dbg->tr->ent[sing_req->tracert_idx]->state = RWDTS_TRACEROUTE_STATE_BOUNCE;
      struct timeval tvzero, now, delta;
      gettimeofday(&now, NULL);
      tvzero.tv_sec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_sec;
      tvzero.tv_usec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_usec;
      timersub(&now, &tvzero, &delta);
      xact->dbg->tr->ent[sing_req->tracert_idx]->elapsed_us = (uint64_t)delta.tv_usec + (uint64_t)delta.tv_sec * (uint64_t)1000000;
    }
  }
  sing_req->req = NULL;
  sing_req->membnode->bnc = TRUE;
  
  /*Check if the registrations for this member are marked as bounce-ok and if so
    then ignore the bounce and let the xact go ahead*/
  if (sing_req->membnode->ignore_bounce == 1) {
    //pretend it is NA now
    block->req_done_na++;
    //remove the member from the xact union so that this member is no longer needed for the xact
    HASH_FIND(hh_xact, xact->xact_union, sing_req->membnode->member->msgpath,
              strlen(sing_req->membnode->member->msgpath), membnode);
    if (membnode){
      HASH_DELETE(hh_xact, xact->xact_union, membnode);
      //the delete of sing_request will free the membnode
      sing_req->memb_xfered = 0;
    }
    //delete the sing-req
    rwdts_router_delete_sing_req(xact, sing_req, false);
    goto xact_run;
  }

  switch (bcode) {
    default:
      /* Actual communication failure: no such peer, timeout, etc. */
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
      block->req_done_nack++;
      xact->req_nack++;
      break;
    case RWMSG_BOUNCE_NOMETH:
     case RWMSG_BOUNCE_NOPEER:
    case RWMSG_BOUNCE_SRVRST:
       if (xact->state > RWDTS_ROUTER_XACT_PREPARE) {
         /* Nope, it's an error after prepare */
         sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
         block->req_done_nack++;
         xact->req_nack++;
       } 
       else if ((bcode == RWMSG_BOUNCE_SRVRST) 
                && sing_req->membnode->member->wait_restart) {
         block->req_outstanding--;
         xact->req_outstanding--;
         block->req_sent--;
         xact->req_sent--;
         rwdts_router_xact_queue_restart(sing_req, xact, true);
         rwdts_router_xact_run(xact);
         return;
       }
       else {
         char *log_str;
         int r = asprintf(&log_str, "Router: %s from %s: setting NACK",
                          rwdts_router_xact_rsp_bounce_str(bcode),
                          sing_req->membnode->member->msgpath);
         RW_ASSERT(r > 0);
         RWDTS_ROUTER_LOG_EVENT(xact->dts, MemberBounce, log_str);
         sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
         block->req_done_nack++;
         xact->req_nack++;
         RW_FREE(log_str);
       }
       break;
  }
xact_run:
  block->req_outstanding--;
  xact->req_outstanding--;
  block->req_done++;
  xact->req_done++;
  rwdts_router_xact_run(xact);
}



/* Actually accept, accumulate, propagate, etc a query-response
   (individual query level, from PREPARE) */
static void rwdts_router_xact_query_rsp(rwdts_router_xact_t *xact,
                                        rwdts_router_xact_req_t *xreq,
                                        RWDtsQueryResult *qresult,
                                        RWDtsEventRsp xevtrsp,
                                        uint32_t blockidx,
                                        rwdts_router_xact_sing_req_t *sing_req)
{

  RWMEMLOG(&xact->rwml_buffer, 
	   RWDTS_MEMLOG_FLAGS_A,
	   "Response from member",
	   RWMEMLOG_ARG_STRNCPY(64, sing_req->membnode->member->msgpath),
	   RWMEMLOG_ARG_PRINTF_INTPTR("query idx %"PRIdPTR, xreq->query_idx));

  RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, QueryRspMember, "rwdts_router_xact_query_rsp from member",
                                    sing_req->membnode->member->msgpath,
                                    xreq->query_idx,
                                    xreq->query_serial);

  bool send_response = FALSE;
  rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];
  RWDtsQueryResult fakeqres;
  rwdts_query_result__init(&fakeqres);
  if (!qresult) {
    qresult = &fakeqres;
  }
  if (!qresult->has_evtrsp) {
    if (xevtrsp != RWDTS_EVENT_RSP_INVALID) {
      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, NoQueryRspMember, "rwdts_router_xact_query_rsp from member",
                                        xevtrsp,
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      qresult->has_evtrsp = TRUE;
      qresult->evtrsp = xevtrsp;
    } else {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, MalformedQueryResult, "malformed queryresult: no evtrsp code from member at malformed queryresult",
                                  sing_req->membnode->member->msgpath,
                                  xreq->query_idx,
                                  xreq->query_serial);
      qresult->has_evtrsp = TRUE;
      qresult->evtrsp = RWDTS_EVENT_RSP_INVALID;
      sing_req->membnode->abrt = TRUE;
    }
  }

  if (sing_req->xreq_state != RWDTS_ROUTER_QUERY_SENT
      && sing_req->xreq_state != RWDTS_ROUTER_QUERY_ASYNC) {
    /* Not expecting this response, what what what? */

    RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, UnexpectedQueryResult, "unexpected queryresult faking NACK",
                                sing_req->xreq_state,
                                sing_req->membnode->member->msgpath,
                                xreq->query_idx,
                                xreq->query_serial);
    goto out;
  }

  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("query_rsp block %d: req_done=%d req_ct=%d req_sent=%d req_nack=%d, xact->state=%s",
                                                 blockidx, block->req_done, block->req_ct, block->req_sent, block->req_done_nack,
                                                 rwdts_router_state_to_str(xact->state)));
  bool add_to_xact_union = TRUE;
  bool capture_responses = FALSE;

  switch (qresult->evtrsp) {
    case RWDTS_EVTRSP_ASYNC:
      /* Not done, may also include responses; this is how
         more/more/more responses can be sent... */

      if (sing_req->xreq_state != RWDTS_ROUTER_QUERY_ASYNC) {
        RW_ASSERT(sing_req->xreq_state != RWDTS_ROUTER_QUERY_NONE); /* this is a rsp, shouldn't have null state! */
        sing_req->xreq_state = RWDTS_ROUTER_QUERY_ASYNC;
        xact->dts->stats.req_responded_async++; /* once per req that gets async responses */
      }
      xact->dts->stats.req_responded++;
      capture_responses = TRUE;
      add_to_xact_union = FALSE;

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, QueryResultAsync, "Query result - evtrsp ASYNC",
                                        sing_req->xreq_state,
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      break;
    case RWDTS_EVTRSP_ACK:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ACK;
      block->req_done++;
      block->req_done_ack++;
      xact->req_done++;
      capture_responses = TRUE;
      if ((block->xbreq) &&
          (block->xbreq->query[xreq->query_idx]) && 
          (/*(block->xbreq->query[xreq->query_idx]->flags & RWDTS_XACT_FLAG_NOTRAN) ||*/
          (block->xbreq->query[xreq->query_idx]->action == RWDTS_QUERY_READ))) {
        add_to_xact_union = FALSE;
      }
      xact->dts->stats.req_responded++;

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, QueryResultAck, "Query result - evtrsp ACK",
                                        sing_req->xreq_state,
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      break;
    case RWDTS_EVTRSP_INTERNAL:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_INTERNAL;
      block->req_done++;
      block->req_done_internal++;
      xact->req_done++;
      capture_responses = FALSE;
      add_to_xact_union = FALSE;
      xact->dts->stats.req_responded++;

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, QueryResultAck, "Query result - evtrsp INTERNAL",
                                        sing_req->xreq_state,
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      break;
    case RWDTS_EVTRSP_NA:
      block->req_done++;
      block->req_done_na++;
      xact->req_done++;
      add_to_xact_union = FALSE;
      xact->dts->stats.req_responded++;

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, QueryResultNa, "Query result - evtrsp NA",
                                        sing_req->xreq_state,
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      break;
    case RWDTS_EVTRSP_NACK:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_NACK;
      block->req_done++;
      block->req_done_nack++;
      xact->req_done++;
      sing_req->membnode->abrt = TRUE;
      xact->abrt = TRUE;
      xact->req_nack++;
      xact->dts->stats.req_responded++;
      if (xact->state == RWDTS_ROUTER_XACT_QUEUED) {
        xact->state = RWDTS_ROUTER_XACT_PREPARE;
      }

      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, QueryResultNack, "Query result evtrsp NACK",
                                  sing_req->xreq_state,
                                  sing_req->membnode->member->msgpath,
                                  xreq->query_idx,
                                  xreq->query_serial);
      break;
    default:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
      sing_req->membnode->abrt = TRUE;
      xact->abrt = TRUE;
      block->req_done++;
      block->req_done_err++;
      xact->req_done++;
      xact->req_err++;
      
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, QueryResultError, "Query result - evtrsp ERROR",
                                  sing_req->xreq_state,
                                  sing_req->membnode->member->msgpath,
                                  xreq->query_idx,
                                  xreq->query_serial);
      break;
  }

  if (add_to_xact_union) {
    rwdts_router_member_node_t *membnode=NULL;
    HASH_FIND(hh_xact, xact->xact_union, sing_req->membnode->member->msgpath, strlen(sing_req->membnode->member->msgpath), membnode);
    if (!membnode) {
      HASH_ADD_KEYPTR(hh_xact, xact->xact_union, sing_req->membnode->member->msgpath, strlen(sing_req->membnode->member->msgpath), sing_req->membnode);
      sing_req->memb_xfered = 1;
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("block %d: [%s] is being added to xact_union",
                                                     blockidx, sing_req->membnode->member->msgpath));
    }
  }

  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("RTR: Resp %s:xact->req_done %d xact_union size now %u results %d result %s for query %d ptr %p",
                                                 sing_req->membnode->member->msgpath, xact->req_done, HASH_CNT(hh_xact, xact->xact_union), 
                                                 (int)qresult->n_result, rwdts_evtrsp_to_str(qresult->evtrsp), (int)xreq->query_serial,
                                                 sing_req->req));

  RWDTS_FREE_CREDITS(xact->dts->credits, sing_req->credits);
  sing_req->credits = 0;
  RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactUnionSize, "xact union size",
                                    HASH_CNT(hh_xact, xact->xact_union));


  if (block->req_done == block->req_ct) {
    rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_DONE);
    send_response = TRUE;
  }

  if (capture_responses && qresult->n_result) {
    RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                RWLOG_ATTR_SPRINTF("query_rsp line=%d xact=%p xact->state=%s qresult->evtrsp capture_responses qresult->n_result=%d",
                                                   __LINE__, xact, rwdts_router_state_to_str(xact->state), (int)qresult->n_result));

    if (!block->xbres) {
      RW_ASSERT(block->block_state != RWDTS_ROUTER_BLOCK_STATE_RESPONDED);
      rwdts_router_block_xbres_init(xact, block);
    }
    const RWDtsXactBlock *xbreq = block->xbreq;
    RWDtsXactBlockResult *xbres = block->xbres;
    RW_ASSERT(xbres);
    RW_ASSERT(xbres->blockidx == xbreq->block->blockidx);
    if (xbres->n_result != xbreq->n_query) {
      xbres->n_result = xbreq->n_query;
      xbres->result = calloc(xbreq->n_query, sizeof(RWDtsQueryResult*));
      int j;
      for (j=0; j<xbreq->n_query; j++) {
        xbres->result[j] = calloc(1, sizeof(RWDtsQueryResult));
        rwdts_query_result__init(xbres->result[j]);
        RWDtsQueryResult *xqres = xbres->result[j];

        /* Set queryidx to query_idx (within block) as opposed to the
           value we sent the member, which was a serial number for the
           request */
        xqres->has_queryidx = TRUE;
        xqres->queryidx = xreq->query_idx;

        /* Set whole Block ID in here.  This would be nicer flat etc */
        xqres->block = (RWDtsXactBlkID*)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                     &xbreq->block->base,
                                                                     xbreq->block->base.descriptor);
      }
    }

    /* Add this message's singleresults to the block's query result list */
    RW_ASSERT(xreq->query_idx < xbres->n_result);
    RWDtsQueryResult *xqres = xbres->result[xreq->query_idx];
    xqres->has_corrid=qresult->has_corrid;
    xqres->corrid = qresult->corrid;
    xqres->has_client_idx=qresult->has_client_idx;
    xqres->client_idx = qresult->client_idx;
    xqres->has_router_idx=qresult->has_router_idx;
    xqres->router_idx = qresult->router_idx;
    xqres->has_serialnum=qresult->has_serialnum;
    xqres->serialnum = qresult->serialnum;

    if (block->req_done == block->req_ct) {
      xqres->more = FALSE;
    }

    if (qresult->n_result) {

      if ((xbreq->query[xreq->query_idx]->action == RWDTS_QUERY_READ)
          && !(xbreq->query[xreq->query_idx]->flags & RWDTS_XACT_FLAG_STREAM)
          && xqres->n_result) {

        /* Merge */
        int i, j;
        ProtobufCBinaryData msg_out;
        msg_out.len = 0; msg_out.data = NULL;
        for (i = 0; i < qresult->n_result; i++) {
          for (j = 0; j < xqres->n_result; j++) {
            if (rw_keyspec_path_is_equal(&xact->ksi, (rw_keyspec_path_t *)xqres->result[j]->path->keyspec, (rw_keyspec_path_t *)qresult->result[i]->path->keyspec)) {
              msg_out.len = qresult->result[i]->result->paybuf.len + xqres->result[j]->result->paybuf.len;
              msg_out.data = RW_MALLOC0(msg_out.len);
              memcpy(msg_out.data, xqres->result[j]->result->paybuf.data, xqres->result[j]->result->paybuf.len);
              memcpy(msg_out.data+xqres->result[j]->result->paybuf.len, qresult->result[i]->result->paybuf.data, qresult->result[i]->result->paybuf.len);
              free(xqres->result[j]->result->paybuf.data);
              xqres->result[j]->result->paybuf.len = msg_out.len;
              xqres->result[j]->result->paybuf.data = msg_out.data;
              break;
            }
          }
          if (j == xqres->n_result) {
            RW_ASSERT(&rwdts_query_single_result__descriptor == qresult->result[i]->base.descriptor);
            xqres->n_result++;
            xqres->result = realloc(xqres->result, xqres->n_result * sizeof(xqres->result[0]));
            xqres->result[xqres->n_result-1] = (RWDtsQuerySingleResult*)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                                                     &qresult->result[i]->base,
                                                                                                     qresult->result[i]->base.descriptor);
            rwdts_log_payload(&xact->dts->payload_stats, 0, RW_DTS_MEMBER_ROLE_ROUTER,
                              RW_DTS_XACT_LEVEL_SINGLERESULT,
                              xqres->result[xqres->n_result-1]->result->paybuf.len);
            RW_ASSERT(xqres->result[xqres->n_result-1]);
          }
        }

        RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                    RWLOG_ATTR_SPRINTF("Merging %d results from '%s'; block's xqres->n_result now %d",
                                                       (int)qresult->n_result, sing_req->membnode->member->msgpath, (int)xqres->n_result));

      } else {
        int i;

        /* Not merge */
        if (qresult->n_result) {
          i = xqres->n_result + qresult->n_result;
          xqres->result = realloc(xqres->result, i * sizeof(xqres->result[0]));
        }
        for (i=0; i<qresult->n_result; i++) {
          /* QuerySingleResult */
          RW_ASSERT(&rwdts_query_single_result__descriptor == qresult->result[i]->base.descriptor);
          xqres->result[xqres->n_result] = (RWDtsQuerySingleResult*)protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                                                                                 &qresult->result[i]->base,
                                                                                                 qresult->result[i]->base.descriptor);
          rwdts_log_payload(&xact->dts->payload_stats, 0, RW_DTS_MEMBER_ROLE_ROUTER,
                            RW_DTS_XACT_LEVEL_SINGLERESULT,
                            xqres->result[xqres->n_result]->result->paybuf.len);
          xqres->n_result++;
        }

        RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                    RWLOG_ATTR_SPRINTF("Streaming added %d results from '%s'; block's xqres->n_result now %d",
                                                       (int)qresult->n_result, sing_req->membnode->member->msgpath, (int)xqres->n_result));

      }

    }
    /* Ought to use nagle-themed criteria here.  Instead we send if
       the block is done or the client is not waiting for the end. */
    if (send_response || !block->waiting) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("Calling rwdts_router_send_responses for block %d", blockidx));
      rwdts_router_send_responses(xact, &block->rwmsg_cliid);
    }
  }
  else {
    if (send_response) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("Calling rwdts_router_send_responses for block %d", blockidx));
      rwdts_router_send_responses(xact, &block->rwmsg_cliid);
    }
  }


 out:
  /* If we are now done, there will be no reqs to send and the end of sendreqs will advance state */
  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("query_rsp line=%d xact=%p xact->state=%s",
                                                 __LINE__, xact, rwdts_router_state_to_str(xact->state)));

  rwdts_router_xact_run(xact);
}

/* Actually accept, accumulate, propagate, etc an event-response
   (generally block/subx or topx level, from SUBCOMMIT, PRECOMMIT,
   ABORT, COMMIT etc) */
static void rwdts_router_xact_event_rsp(rwdts_router_xact_t *xact,
                                        rwdts_router_xact_req_t *xreq,
                                        const RWDtsXactResult *xresult,
                                        RWDtsEventRsp xevtrsp,
                                        uint32_t blockidx,
                                        rwdts_router_xact_sing_req_t *sing_req)
{
  rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];

  RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactEvtRspMember, "rwdts_router_xact_event_rsp from member",
                                    sing_req->membnode->member->msgpath,
                                    xreq->query_idx,
                                    xreq->query_serial);

  RWDtsEventRsp evtrsp;
  if (!xresult || !xresult->has_evtrsp) {
    if (xevtrsp == RWDTS_EVENT_RSP_INVALID) {

      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, MalformedQueryResult, "malformed queryresult: no evtrsp code from member, faking NACK",
                                  sing_req->membnode->member->msgpath,
                                  xreq->query_idx,
                                  xreq->query_serial);

      evtrsp = RWDTS_EVTRSP_NACK;
      sing_req->membnode->abrt = TRUE;
    } else {
      /* !? caller used xresult to set xevtrsp?! */
      RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact, "caller used xresult to set xevtrsp?");
      evtrsp = xevtrsp;
      return;
    }
  } else {
    evtrsp = xresult->evtrsp;
  }

  if (sing_req->xreq_state != RWDTS_ROUTER_QUERY_SENT
      && sing_req->xreq_state != RWDTS_ROUTER_QUERY_ASYNC) {
    /* Not expecting this response, what what what? */
    RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, UnexpectedQueryResult, "unexpected event result: faking NACK",
                                sing_req->xreq_state,
                                sing_req->membnode->member->msgpath,
                                xreq->query_idx,
                                xreq->query_serial);
    goto run;
  }

  bool del_from_xact_union = FALSE;

  switch (evtrsp) {
    case RWDTS_EVTRSP_ASYNC:
      /* Not done, may also include responses; this is how
         more/more/more responses can be sent... */
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ASYNC;
      RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact,"More responses");
      return;
    case RWDTS_EVTRSP_NA:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_NA;
      block->req_done++;
      block->req_done_na++;
      xact->req_done++;
      del_from_xact_union = TRUE;
      break;
    case RWDTS_EVTRSP_ACK:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ACK;
      block->req_done++;
      block->req_done_ack++;
      xact->req_done++;
      break;
    case RWDTS_EVTRSP_NACK:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_NACK;
      sing_req->membnode->abrt = TRUE;
      if (xact->state != RWDTS_ROUTER_XACT_COMMIT) {
        xact->req_nack++;
      } else {
        xact->req_err++;
      }
      xact->abrt = TRUE;
      block->req_done++;
      block->req_done_nack++;
      xact->req_done++;
      break;
    case RWDTS_EVTRSP_INTERNAL: /* not expected here */
    default:
      sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
      sing_req->membnode->abrt = TRUE;
      xact->abrt = TRUE;
      block->req_done++;
      block->req_done_err++;
      xact->req_done++;
      xact->req_err++;
      break;
  }

  if (del_from_xact_union) {
    rwdts_router_member_node_t *membnode=NULL;
    HASH_FIND(hh_xact, xact->xact_union, sing_req->membnode->member->msgpath, strlen(sing_req->membnode->member->msgpath), membnode);
    if (membnode) {
      HASH_DELETE(hh_xact, xact->xact_union, membnode);
    }
  }

run:
  rwdts_router_xact_run(xact);
}

static rwdts_router_xact_sing_req_t*
rwdts_find_idx_and_sing_req(rwdts_router_xact_t *xact,
                            rwmsg_request_t *rsp_req,
                            uint32_t *blockidx,
                            rwdts_router_xact_req_t **xreq)
{
  uint32_t reqidx;

  rwdts_router_xact_sing_req_t *sing_req, *tmp_sing_req;

  for (reqidx=0; reqidx < xact->n_req; reqidx++) {
    *xreq = &xact->req[reqidx];
    if (*xreq) {
      HASH_ITER(hh_memb, (*xreq)->sing_req, sing_req, tmp_sing_req) {
        if (sing_req->req == rsp_req) {
          *blockidx = (*xreq)->block_idx;
          return sing_req;
        }
      }
    }
  }
  return NULL;
}

static void
rwdts_rtr_err_report(rwdts_router_xact_t *xact,
                     RWDtsErrorReport *err_report)
{
  RWDtsDebug *dbg = xact->dbg;
  if (!dbg) {
    dbg = (RWDtsDebug*) RW_MALLOC0(sizeof(RWDtsDebug));
    rwdts_debug__init(dbg);
    xact->dbg = dbg;
  }
  if (!dbg->err_report) {
    dbg->err_report = rwdts_xact_create_err_report();
  }
  rwdts_dbg_errrep_append(dbg->err_report, err_report);
}




/* A response to a prepare or event has arrived.  Correlate and handle it... */
static void
rwdts_router_xact_msg_rsp_internal(const RWDtsXactResult *xresult,
                                   rwmsg_request_t *rsp_req, /* our original request handle, or NULL for upcalls from the member */
                                   rwdts_router_t *dts,
                                   rwdts_router_xact_sing_req_t *sing_req)
{
  rwdts_router_xact_t *xact = NULL;
  rwdts_router_xact_req_t *xreq = NULL;
  uint32_t blockidx = 0;
  rwdts_router_xact_block_t *block = NULL;

  if (!rsp_req) {
    /* Request sent at us */
    RW_ASSERT(xresult);
  }
  

  if (!xresult) {
    dts->stats.req_bnc++;
    RW_ASSERT(rsp_req);
    rwdts_router_xact_t *xx = NULL;
    /* Fark, we can't correlate directly, do it the hard way */
    HASH_ITER(hh, dts->xacts, xact, xx) {
      sing_req = rwdts_find_idx_and_sing_req(xact, rsp_req, &blockidx, &xreq);
      if (sing_req) {
        rwdts_router_xact_query_bounce_rsp(xact, rsp_req, xreq, blockidx, sing_req);
        break;
      }
    }
    if (!sing_req){
      RWDTS_ROUTER_LOG_EVENT(dts, UnexpectedBounceResponse, "unexpected bounce response");
    }
    
  } else {
    int i;
    /*Check if this is a real response from the client or it is a synthesized response by the
      router itself to make the xact proceed further*/
    if (!sing_req) {
      /*Since there is no sing_req, this is a response from the client, find the xact, sing_req
        blockidx and xreq from the response*/
      xact = rwdts_router_xact_lookup_by_id(dts, &xresult->id);
      if (!xact) {
        /* Not found, huh? */
        RWDTS_ROUTER_LOG_XACT_ID_EVENT(dts, &xresult->id, XactNotFound,  "rwdts xreq response; xact not found");
        
        return;
      }
      
      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, ResponseRcvd, "rwdts xact got a response", (int)xresult->n_result);
      
      sing_req = rwdts_find_idx_and_sing_req(xact, rsp_req, &blockidx, &xreq);
      if (!sing_req) {
        RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, UnableToCorrelate,  "RWDTS router unable to correlate rsp in xact", (uint64_t)rsp_req);
        RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact, "RWDTS router unable to correlate rsp=%p in xact ...!?\n", rsp_req);
        // ^^ that aborts the xact
        return;
      }
    }else{
      /*This is a synthesized response by the router, use the sing_req to get the xreq, blockidx and
        xreq*/
      xact = sing_req->xact;
      xreq = &xact->req[sing_req->xreq_idx];
      blockidx = xreq->block_idx;
    }
    
    if (xresult->n_new_blocks) {
      int new_blocks_count = 0;
      for (; new_blocks_count < xresult->n_new_blocks; new_blocks_count++) {
        rwdts_router_xact_block_t *block = rwdts_xact_block_lookup_by_id(xact, xresult->new_blocks[new_blocks_count]);
        if (!block) {
          rwdts_router_wait_block_t* wait_block = RW_MALLOC0_TYPE(sizeof(*wait_block), rwdts_router_wait_block_t);
          rwdts_xact_blk_id__init((RWDtsXactBlkID *)(&wait_block->blockid));
          protobuf_c_message_memcpy(&wait_block->blockid.base, &xresult->new_blocks[new_blocks_count]->base);
          RWDTS_ADD_PBKEY_TO_HASH(hh_wait_blocks, xact->wait_blocks, (&wait_block->blockid), routeridx, wait_block);
          wait_block->member_msg_path = RW_STRDUP(sing_req->membnode->member->msgpath);

          xact->do_commits = true;
          RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                                      RWLOG_ATTR_SPRINTF("SET do_commits:processing sing_req %s wait block add",
                                                         sing_req->membnode->member->msgpath));
        }
      }
    }
    RW_ASSERT(blockidx >= 0);
    RW_ASSERT(blockidx < xact->n_xact_blocks);
    block = xact->xact_blocks[blockidx];
    RW_ASSERT(block);


    block->req_outstanding--;
    xact->req_outstanding--;

    RWDTS_RTR_DEBUG_TRACE_APPEND(xact, xresult);

    RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, RWLOG_ATTR_SPRINTF("RTR: rwdts_router_xact_msg_rsp %p", xresult->dbg));
    if (xresult->dbg && xresult->dbg->err_report)
    {
      rwdts_rtr_err_report(xact,
                           xresult->dbg->err_report);
    }

    /* If block id/idx matches, find it, in xact->req[], using query_idx (which is really query_serial to avoid ambiguity). */
    RWDtsQueryResult *qres = NULL;
    RWDtsXactBlockResult *bres = NULL;
    if (xresult->n_result) {

      /* Well this is a dog.  Use a hash or something. */
      for (i=0; i<xresult->n_result; i++) {
        bres = xresult->result[i];
        int j;
        for (j=0; j<bres->n_result; j++) {
          qres = bres->result[j];
          if (qres->has_queryidx) {
            RW_ASSERT(xreq->query_serial == qres->queryidx);
            goto foundxreq;
          }
        }
        
        RW_ASSERT(sing_req != NULL);
     foundxreq:
        {
          /* At present we send one query per req, or it might be 0 for async cases */
          RW_ASSERT(xresult->n_result <= 1);
          
          /* Take the most specific available evtrsp */
          RWDtsEventRsp xevtrsp = RWDTS_EVENT_RSP_INVALID;
          if (qres && qres->has_evtrsp) {
            xevtrsp = qres->evtrsp;
          } else if (bres && bres->has_evtrsp) {
            xevtrsp = bres->evtrsp;
          } else if (xresult && xresult->has_evtrsp) {
            xevtrsp = xresult->evtrsp;
          }
          
          if (rsp_req && xact->dbg) {
            if (sing_req->tracert_idx >= 0
                && sing_req->tracert_idx < xact->dbg->tr->n_ent) {
              /* FIXME check block/query id correlation in the trace */
              xact->dbg->tr->ent[sing_req->tracert_idx]->state = RWDTS_TRACEROUTE_STATE_RESPONSE;
              struct timeval tvzero, now, delta;
              gettimeofday(&now, NULL);
              tvzero.tv_sec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_sec;
              tvzero.tv_usec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_usec;
              timersub(&now, &tvzero, &delta);
              uint64_t elapsed = (uint64_t)delta.tv_usec + (uint64_t)delta.tv_sec * (uint64_t)1000000;
              xact->dbg->tr->ent[sing_req->tracert_idx]->elapsed_us = elapsed;
              xact->dbg->tr->ent[sing_req->tracert_idx]->res_code = (xevtrsp == RWDTS_EVENT_RSP_INVALID) ? RWDTS_EVTRSP_NA : xevtrsp;
              if (xresult->n_result) {
                RW_ASSERT(xresult->n_result == 1); // just one xact / req handled here
                xact->dbg->tr->ent[sing_req->tracert_idx]->res_count += xresult->result[0]->n_result;
              }
            }
          }
          /* We have a result for xreq! */
          sing_req->req = NULL;
          rwdts_router_xact_query_rsp(xact, xreq, qres, xevtrsp, blockidx, sing_req);
          
          return;
        }
      }
    } else {                        /* !n_result */

      RW_ASSERT(rsp_req);
      RW_ASSERT(xact->state >= RWDTS_ROUTER_XACT_PREPARE);

      /* Meh, find it the hard way, need correlation id/serial in top
         message to avoid this for > PREPARE.  PREPARE responses that
         get here probably had the wrong ID plugged in by the API. */

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactInfo, "rwdts router xact brute correlation for request");

    
      RWDTS_RTR_DEBUG_TRACE_APPEND(xact, xresult);

      /* Take the most specific available evtrsp */
      RWDtsEventRsp xevtrsp = RWDTS_EVENT_RSP_INVALID;
      if (xresult && xresult->has_evtrsp) {
        xevtrsp = xresult->evtrsp;
      }

      RW_ASSERT(sing_req->req == rsp_req);
      if (rsp_req && xact->dbg && sing_req->tracert_idx >= 0) {
        RW_ASSERT(sing_req->tracert_idx < xact->dbg->tr->n_ent);
        xact->dbg->tr->ent[sing_req->tracert_idx]->state = RWDTS_TRACEROUTE_STATE_RESPONSE;
        struct timeval tvzero, now, delta;
        gettimeofday(&now, NULL);
        tvzero.tv_sec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_sec;
        tvzero.tv_usec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_usec;
        timersub(&now, &tvzero, &delta);
        xact->dbg->tr->ent[sing_req->tracert_idx]->elapsed_us = (uint64_t)delta.tv_usec + (uint64_t)delta.tv_sec * (uint64_t)1000000;
        xact->dbg->tr->ent[sing_req->tracert_idx]->res_code = (xevtrsp == RWDTS_EVENT_RSP_INVALID) ? RWDTS_EVTRSP_NA : xevtrsp;
        xact->dbg->tr->ent[sing_req->tracert_idx]->res_count += xresult->n_result;
      }
      sing_req->req = NULL;
      rwdts_router_xact_event_rsp(xact, xreq, xresult, xevtrsp, blockidx, sing_req);
      return;
    }
    /* As it is a response, this should be unpossible.  We use the
       rwmsg cancel API to avoid orphan responses. */
    RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, UnableToCorrelate,  "rwdts unable to find request for this response", (uint64_t)rsp_req);
    RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact, "rwdts unable to find request for this response=%p !?!?!?\n", rsp_req);

    return;
  }

  return;
}



void
rwdts_router_xact_msg_rsp(const RWDtsXactResult *xresult,
                          rwmsg_request_t *rsp_req, /* our original request handle, or NULL for upcalls from the member */
                          rwdts_router_t *dts)
{
 dts->stats.req_responded++;


 return rwdts_router_xact_msg_rsp_internal(xresult, rsp_req, dts, NULL);
}



/* All blocks for some non-prepare state are done */
static void rwdts_router_xact_event_run(rwdts_router_xact_t *xact) {
  RW_ASSERT(xact->state != RWDTS_ROUTER_XACT_PREPARE);

  rwdts_router_t *dts = xact->dts;
  struct timeval curr_time;
  int timediff;

  gettimeofday(&curr_time, NULL);

  RW_ASSERT(xact->do_commits);

  /* Must be coming from an interesting block-transmitting state */
  RW_ASSERT(xact->xact_blocks_event[xact->state]);
  if (xact->xact_blocks_event[xact->state]->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE) {

    switch (xact->state) {
    case RWDTS_ROUTER_XACT_PRECOMMIT:
      rwdts_router_xact_newstate(xact, xact->abrt ? RWDTS_ROUTER_XACT_ABORT: RWDTS_ROUTER_XACT_COMMIT );
      timediff = ((curr_time.tv_sec - xact->precommit_time.tv_sec) * 1000+
                  (curr_time.tv_usec - xact->precommit_time.tv_usec)/1000);
      xact->xact_pcom_time = timediff;
      rtr_life_incr(xact_pcom_time);
      if (xact->state == RWDTS_ROUTER_XACT_COMMIT) {
        xact->commit_time = curr_time;
      } else {
        xact->abort_time = curr_time;
      }
      break;
    case RWDTS_ROUTER_XACT_ABORT:
      timediff = ((curr_time.tv_sec - xact->abort_time.tv_sec) * 1000+
                  (curr_time.tv_usec - xact->abort_time.tv_usec)/1000);
      xact->done_time = curr_time;
      xact->xact_abort_time = timediff;
      rtr_life_incr(xact_abort_time);
      rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_DONE);
      break;
    case RWDTS_ROUTER_XACT_COMMIT:
      timediff = ((curr_time.tv_sec - xact->commit_time.tv_sec) * 1000+
                  (curr_time.tv_usec - xact->commit_time.tv_usec)/1000);
      xact->done_time = curr_time;
      xact->xact_com_time = timediff;
      rtr_life_incr(xact_com_time);
      rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_DONE);
      break;
    case RWDTS_ROUTER_XACT_DONE:
      /* This state is terminal, stay here until all responding is finished. */
      break;
    default:
      RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("router xact->state=%d", xact->state));
      RW_CRASH();
      break;
    }
  }
}

/* Advance the state of xact and/or blocks within of prepare if possible */
static void rwdts_router_xact_prepare_run(rwdts_router_xact_t *xact)
{
  rwdts_router_t *dts = xact->dts;
  int i;

  /* Check for done blocks */
  for (i=0; i<xact->n_xact_blocks; i++) {
    if (xact->xact_blocks[i]->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING
        && xact->xact_blocks[i]->req_done == xact->xact_blocks[i]->req_ct) {
      rwdts_router_xact_block_done(xact, xact->xact_blocks[i]);
      RW_ASSERT(xact->xact_blocks[i]->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE
                || xact->xact_blocks[i]->block_state == RWDTS_ROUTER_BLOCK_STATE_RESPONDED);
    }
  }

  /* Prepare any blocks needing to be prepped */
  if (rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_NEEDPREP)) {

    RW_ASSERT(xact->state == RWDTS_ROUTER_XACT_PREPARE);

    int i;
    for (i=0; i<xact->n_xact_blocks; i++) {
      if (xact->xact_blocks[i]->block_state == RWDTS_ROUTER_BLOCK_STATE_NEEDPREP) {
        rwdts_router_xact_block_prepreqs_prepare(xact, xact->xact_blocks[i], i);
        break;
      }
    }
  } else {

    /* Check for not done blocks, currently executing.  We do nothing
       else if there are blocks underway. */
    if (rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_RUNNING)) {
      return;
    }

    if (rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_INIT)) {

      RW_ASSERT(xact->state == RWDTS_ROUTER_XACT_PREPARE);

      /* No blocks in needprep state to run next, so pick one to mark for needprep */
      int i;
      for (i=0; i<xact->n_xact_blocks; i++) {
        if (xact->xact_blocks[i]->block_state == RWDTS_ROUTER_BLOCK_STATE_INIT) {
          RW_ASSERT(!xact->xact_blocks[i]->event_block);
          rwdts_router_xact_block_newstate(xact, xact->xact_blocks[i], RWDTS_ROUTER_BLOCK_STATE_NEEDPREP);
          rwdts_router_xact_block_prepreqs_prepare(xact, xact->xact_blocks[i], i);
          break;
        }
      }
      if (i == xact->n_xact_blocks) {
        if (xact->n_xact_blocks == (rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_DONE)
                                    + rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_RESPONDED))) {
          /* Proceed */
          goto endprep;
        }
      }

    } else {
    endprep:
      /* Assert that all blocks are done, as we are about to advance the state */
      RW_ASSERT(xact->n_xact_blocks == (rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_DONE)
                                        + rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_RESPONDED)));

      struct timeval curr_time;
      int timediff;
      gettimeofday(&curr_time, NULL);
      timediff = ((curr_time.tv_sec - xact->start_time.tv_sec) * 1000+
                  (curr_time.tv_usec - xact->start_time.tv_usec)/1000);
      /* No parent => this is a topx */
      if (!HASH_CNT(hh_wait_blocks, xact->wait_blocks) &&
         (xact->ended || xact->abrt)) { /* ended => commit called so end msg sent, or END flag on a last block; abrt is endogenous */
        if (xact->do_commits) {
          /* Run transactional stages */
          if (xact->abrt) {
            xact->abort_time = curr_time;
            xact->xact_prep_time = timediff;
            rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_ABORT);
          } else {
            xact->precommit_time = curr_time;
            xact->xact_prep_time = timediff;
            rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_PRECOMMIT);
          }
        } else {
          /* Not a transaction, finish up */
          xact->done_time = curr_time;
          xact->xact_prep_time = timediff;
          rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_DONE);
        }
        rtr_life_incr(xact_prep_time);
      } else {
        /* Have not gotten End() or END flag from xact originator, wait for it or more blocks */
        RW_ASSERT(xact->state == RWDTS_ROUTER_XACT_PREPARE);
        xact->do_commits = rwdts_do_commits(xact);
        uint32_t cl_blk;
        if (!xact->total_req_ct && xact->isadvise && xact->do_commits &&
            rwdts_precommit_to_client(xact, &cl_blk)) {
          /* Check for any advise  and set the flag to move to PRECOMMIT */
          xact->move_to_precomm = 1; 
        }
      }
    }
  }
}


/* Call when an xreq finishes with one block.  It will activate the
   next block, or advance state */
static void rwdts_router_xact_block_done(rwdts_router_xact_t *xact,
                                         rwdts_router_xact_block_t *block) {
  RW_ASSERT(block->req_done >= block->req_ct);

  if (block->req_done_nack) {
    xact->abrt = TRUE;
  }

  RW_ASSERT(block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING);
  rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_DONE);

  if (block->event_block) {
    rwdts_router_xact_event_run(xact);
  } else {
    rwdts_router_xact_prepare_run(xact);
  }
}

static void
rwdts_router_select_anycast_member(rwdts_router_xact_t *xact,
                                   rwdts_router_xact_req_t *xreq)
{
  uint32_t select_node;
  uint32_t count = 0;
  rwdts_router_xact_sing_req_t *sing_req, *tmp_sing_req;
  
  if ((!xreq->anycast_cnt) || (xreq->anycast_cnt == 1)) {
    return;
  }

  select_node = rand() % xreq->anycast_cnt;
  HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
    RW_ASSERT(xreq->index == sing_req->xreq_idx);
    if (sing_req->any_cast) {
      if (count != select_node) {
        rwdts_router_delete_sing_req(xact, sing_req, true);
      }
      count++;
      if (count == xreq->anycast_cnt) {
        return;
      }
    }
  }

  return;
}

static void
rwdts_router_prepare_f(rwdts_router_xact_t *xact, rwdts_router_xact_sing_req_t *sing_req)
{
  rwdts_router_xact_req_t *xreq = &xact->req[sing_req->xreq_idx];
  uint32_t blockidx = xreq->block_idx;
  rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];

  RW_ASSERT(xreq->index == sing_req->xreq_idx);
  RW_ASSERT(xreq->xact == xact);
  
  if (!sing_req->membnode->member->dest) {
    sing_req->membnode->member->dest = rwmsg_destination_create(xact->dts->ep, RWMSG_ADDRTYPE_UNICAST, sing_req->membnode->member->msgpath);
  }

  if (sing_req->xreq_state == RWDTS_ROUTER_QUERY_NA) {
    /* These are already counted as done when set up in the first place */
    return;
  }
  
  /*Queue a xact if the member is in wait restart state. this is an early detection instead
    of waiting for the SRVRST when the prepare is sent. Note that the prepares might have 
    been sent to other members in this case even though this xact is going to get queued.*/
  if (sing_req->membnode->member->wait_restart) {
    rwdts_router_xact_queue_restart(sing_req, xact, false);
  }

  switch (sing_req->xreq_state) {
  case RWDTS_ROUTER_QUERY_UNSENT:
  case RWDTS_ROUTER_QUERY_NONE:
  case RWDTS_ROUTER_QUERY_ASYNC:
    break;
  case RWDTS_ROUTER_QUERY_QUEUED:
    /*DO not proceed with the prepare since this xact and sing_req are queued*/
    return;
  default:
    return;
  }

  RWDtsXact req_xact;
  rwdts_xact__init(&req_xact);
  //??  rwdts_xact_id__init(&req_xact.id);
  rwdts_xact_id_memcpy(&req_xact.id, &xact->id);
  req_xact.has_flags = 1;
  req_xact.flags = xact->flags;
  
  /* We send a block of queries, notionally just the applicable
     queries from the current block, but for now all queries in
     the block to everyone. */
  RWDtsXactBlock *xtab[1];
  RWDtsXactBlock req_blk;
  RWDtsQuery q1;
  RWDtsQuery *qtab[1];
  if (xact->state == RWDTS_ROUTER_XACT_PREPARE) {
    rwdts_xact_block__init(&req_blk);
    const RWDtsQuery *tmp_query = block->xbreq->query[xreq->query_idx];

    req_blk.n_query = 1;
    req_blk.query = qtab;
    qtab[0] = &q1;
    rwdts_query__init(qtab[0]);
    /* If sing_req[sindx]->send_before, we only really ought to need the queryidx, without payload stuff. */
    memcpy((char *)qtab[0], tmp_query, sizeof(RWDtsQuery));
    qtab[0]->has_queryidx = TRUE;
    qtab[0]->queryidx = xreq->query_serial;

    if(!sing_req->credits)
      {
        sing_req->credits = RWDTS_GET_CREDITS(xact->dts->credits, tmp_query->credits);
      }
    qtab[0]->credits = sing_req->credits;
    qtab[0]->has_credits = TRUE;

    /* Observe if this is a read-only query and/or has the non-transaction flag set.  Move to xact_create!!!!! */
    if (((qtab[0]->action != RWDTS_QUERY_READ) && 
        (!(qtab[0]->flags & RWDTS_XACT_FLAG_NOTRAN)))) {
      xact->do_commits = true;
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("SET do_commits:processing sing_req %s",
                                                     sing_req->membnode->member->msgpath));
    }
    else if ((xact->n_xact_blocks > 1)) {
      xact->do_commits = true;
    }

    xtab[0] = &req_blk;
    req_xact.n_subx = 1;
    req_xact.subx = xtab;

    sing_req->tracert_idx = -1;
    if (xact->dbg) {
      int j;

      if (xact->dbg->tr) {
        rwdts_trace_filter_t filt;
        RW_ZERO_VARIABLE(&filt);

        filt.print = xact->dbg->tr->print_;
        filt.break_start = xact->dbg->tr->break_start;
        filt.break_prepare = xact->dbg->tr->break_prepare;
        filt.break_end = xact->dbg->tr->break_end;

        /* Include the dbg block in the req, so downstream code will trace */
        rwdts_xact_dbg_tracert_start(&req_xact, &filt);
      } else {
        rwdts_xact_dbg_tracert_start(&req_xact, NULL);
      }

      /* Make up an entry in our response for this req */
      RWDtsTracerouteEnt ent;
      rwdts_traceroute_ent__init(&ent);
      ent.func = (char*) __PRETTY_FUNCTION__;
      ent.line = __LINE__;
      ent.event = RWDTS_EVT_PREPARE;
      ent.what = RWDTS_TRACEROUTE_WHAT_REQ;
      ent.dstpath = sing_req->membnode->member->msgpath;
      ent.srcpath = (char*)xact->dts->rwmsgpath;
      assert(req_blk.n_query == 1); /* not implemented above */
      ent.n_queryid = 0;
      for (j=0; j<req_blk.n_query && j<8; j++) {
        assert(j == 0);        /* not implemented above */
        uint32_t blockid = block->blockid.blockidx;
        ent.queryid[j] = (((0x00ffffffff&((uint64_t)blockid)) << 32) | ((uint64_t)(xreq->query_idx)));
        ent.n_queryid++;
      }
      if (xact->dbg->tr && xact->dbg->tr->print_) {
        fprintf(stderr, "%s:%u: TYPE:%s, EVT:%s, DST:%s, SRC:%s\n", ent.func, ent.line,
                rwdts_print_ent_type(ent.what), rwdts_print_ent_event(ent.event),
                ent.dstpath, ent.srcpath);
      }
      sing_req->tracert_idx = rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
      RWDTS_TRACE_EVENT_REQ(xact->dts->rwtaskletinfo->rwlog_instance, TraceReq,
                            xact->xact_id_str,ent);
    }

    RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendPrepare,  "rwdts_router_xact_sendreqs sending prepares",
                                      sing_req->membnode->member->msgpath,
                                      xreq->query_idx,
                                      xreq->query_serial,
                                      block->blockid.blockidx,
                                      qtab[0]->queryidx);
  }

  rwdts_router_send_f(xact, xreq, block, sing_req, &req_xact);
  if (req_xact.dbg) {
    rwdts_debug__free_unpacked(NULL, req_xact.dbg);
    req_xact.dbg = NULL;
  }
}

static void
rwdts_router_event_f(rwdts_router_xact_t *xact,
                     rwdts_router_xact_req_t *xreq,
                     uint32_t blockidx,
                     rwdts_router_xact_sing_req_t *sing_req) {
  rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];

  RW_ASSERT(sing_req->membnode->member->dest);

  RWDtsXact req_xact;
  rwdts_xact__init(&req_xact);
  rwdts_xact_id_memcpy(&req_xact.id, &xact->id);

  sing_req->tracert_idx = -1;
  if (xact->dbg) {
    if (xact->dbg->tr) {
      rwdts_trace_filter_t filt;
      RW_ZERO_VARIABLE(&filt);

      filt.print = xact->dbg->tr->print_;
      filt.break_start = xact->dbg->tr->break_start;
      filt.break_prepare = xact->dbg->tr->break_prepare;
      filt.break_end = xact->dbg->tr->break_end;

      /* Include the dbg block in the req, so downstream code will trace */
      rwdts_xact_dbg_tracert_start(&req_xact, &filt);
    } else {
      rwdts_xact_dbg_tracert_start(&req_xact, NULL);
    }


    /* Make up an entry in our response for this req */
    RWDtsTracerouteEnt ent;
    
    rwdts_traceroute_ent__init(&ent);
    ent.func = (char*)__PRETTY_FUNCTION__;
    ent.line = __LINE__;
    ent.what = RWDTS_TRACEROUTE_WHAT_ABORT;
    ent.dstpath = sing_req->membnode->member->msgpath;
    ent.srcpath = (char*)xact->dts->rwmsgpath;
    switch (xact->state) {
      case RWDTS_ROUTER_XACT_INIT:
        ent.event = RWDTS_EVT_INIT;
        break;
      case RWDTS_ROUTER_XACT_PREPARE:
        ent.event = RWDTS_EVT_PREPARE;
        break;
      case RWDTS_ROUTER_XACT_PRECOMMIT:
        ent.event = RWDTS_EVT_PRECOMMIT;
        break;
      case RWDTS_ROUTER_XACT_COMMIT:
        ent.event = RWDTS_EVT_COMMIT;
        break;
      case RWDTS_ROUTER_XACT_ABORT:
        ent.event = RWDTS_EVT_ABORT;
        break;
      case RWDTS_ROUTER_XACT_SUBCOMMIT:
        ent.event = RWDTS_EVT_SUBCOMMIT;
        break;
      case RWDTS_ROUTER_XACT_SUBABORT:
        ent.event = RWDTS_EVT_SUBABORT;
        break;
      case RWDTS_ROUTER_XACT_DONE:
        ent.event = RWDTS_EVT_DONE;
        break;
      case RWDTS_ROUTER_XACT_TERM:
        ent.event = RWDTS_EVT_TERM;
        break;
      default:
        ent.event = 0;
        RW_CRASH();
        break;
    }
    ent.n_queryid = 0;

    sing_req->tracert_idx = rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
    RWLOG_EVENT(xact->dts->rwtaskletinfo->rwlog_instance,RwDtsApiLog_notif_TraceAbort,xact->xact_id_str,
                ent.srcpath,ent.dstpath,rwdts_router_event_to_str(ent.event));
    if (xact->dbg->tr && xact->dbg->tr->print_) {
      fprintf(stderr, "%s:%u: TYPE:%s, DST:%s, SRC:%s\n", ent.func, ent.line,
              rwdts_print_ent_type(ent.what), ent.dstpath, ent.srcpath);
    }
    RWLOG_EVENT(xact->dts->rwtaskletinfo->rwlog_instance,RwDtsApiLog_notif_TraceAbort,
                xact->xact_id_str,ent.srcpath,ent.dstpath,rwdts_router_event_to_str(ent.event));
  }

  rwdts_router_send_f(xact, xreq, block, sing_req, &req_xact);
  if (req_xact.dbg) {
    rwdts_debug__free_unpacked(NULL, req_xact.dbg);
    req_xact.dbg = NULL;
  }
}

void rwdts_router_send_f(rwdts_router_xact_t *xact,
                         rwdts_router_xact_req_t *xreq,
                         rwdts_router_xact_block_t *block,
                         rwdts_router_xact_sing_req_t *sing_req,
                         RWDtsXact *req_xact) {

  /* This jumble of Pb methods is dumb, why isn't it just an enum code in the RWDtsXact already? */

  rwmsg_closure_t clo = { .pbrsp = (rwmsg_pbapi_cb)rwdts_router_xact_msg_rsp, .ud = xact->dts };
  //FIXME rwdts_calc_log_payload(&dts->payload_stats, RW_DTS_MEMBER_ROLE_ROUTER,
  //                       RW_DTS_XACT_LEVEL_QUERY, &req_xact);
  rw_status_t rs = RW_STATUS_FAILURE;
  switch (xact->state) {
    case RWDTS_ROUTER_XACT_PREPARE:
      RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A, "Sending PREPARE",
	       RWMEMLOG_ARG_STRNCPY(128, sing_req->membnode->member->msgpath));
      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendMsg,  "rwdts_router_xact_sendreqs sending prepare",
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      rs = rwdts_member__prepare(&xact->dts->membercli,
                                 sing_req->membnode->member->dest,
                                 req_xact,
                                 &clo,
                                 &sing_req->req);
      RW_ASSERT(sing_req->req);
      break;
    case RWDTS_ROUTER_XACT_PRECOMMIT:
      RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A, "Sending PRECOMMIT",
	       RWMEMLOG_ARG_STRNCPY(128, sing_req->membnode->member->msgpath));

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendMsg,  "rwdts_router_xact_sendreqs sending precommit",
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);

      rs = rwdts_member__pre_commit(&xact->dts->membercli,
                                    sing_req->membnode->member->dest,
                                    req_xact,
                                    &clo,
                                    &sing_req->req);
      break;
    case RWDTS_ROUTER_XACT_COMMIT:
      RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A, "Sending COMMIT",
	       RWMEMLOG_ARG_STRNCPY(128, sing_req->membnode->member->msgpath));
      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendMsg,  "rwdts_router_xact_sendreqs sending commit",
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);
      rs = rwdts_member__commit(&xact->dts->membercli,
                                sing_req->membnode->member->dest,
                                req_xact,
                                &clo,
                                &sing_req->req);
      break;
    case RWDTS_ROUTER_XACT_ABORT:
      RWMEMLOG(&xact->rwml_buffer, RWDTS_MEMLOG_FLAGS_A, "Sending ABORT",
	       RWMEMLOG_ARG_STRNCPY(128, sing_req->membnode->member->msgpath));

      RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendMsg,  "rwdts_router_xact_sendreqs sending abort",
                                        sing_req->membnode->member->msgpath,
                                        xreq->query_idx,
                                        xreq->query_serial);

      rs = rwdts_member__abort(&xact->dts->membercli,
                               sing_req->membnode->member->dest,
                               req_xact,
                               &clo,
                               &sing_req->req);
      break;
    default:
      RW_CRASH();
      break;
  }

  if (!sing_req->sent_before) {

    block->req_sent++;
    RW_ASSERT(block->req_sent <= block->req_ct);
    xact->req_sent++;
    sing_req->sent_before = TRUE;
  }
  if (rs != RW_STATUS_SUCCESS) {
    sing_req->xreq_state = RWDTS_ROUTER_QUERY_ERR;
    block->req_done++;
    xact->req_done++;
    block->req_done_err++;
    xact->req_err++;
    sing_req->req = NULL;
    xact->dts->stats.req_bnc++;
    xact->dts->stats.req_bnc_imm++;
    if (xact->dbg && sing_req->tracert_idx >= 0) {
      xact->dbg->tr->ent[sing_req->tracert_idx]->state = RWDTS_TRACEROUTE_STATE_BOUNCE;
    }
  } else {
    RW_ASSERT(sing_req->req);
    sing_req->xreq_state = RWDTS_ROUTER_QUERY_SENT;
    block->req_outstanding++;
    xact->req_outstanding++;
    xact->dts->stats.req_sent++;
    if (xact->dbg && sing_req->tracert_idx >= 0) {
      xact->dbg->tr->ent[sing_req->tracert_idx]->state = RWDTS_TRACEROUTE_STATE_SENT;
    }
  }
}


static void rwdts_router_xact_sendreqs(rwdts_router_xact_t *xact,
                                       rwdts_router_xact_block_t *block) {
  RW_ASSERT(xact);
  RW_ASSERT(xact->state > RWDTS_ROUTER_XACT_INIT);
  RW_ASSERT(block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING);
  RW_ASSERT(block->req_sent <= block->req_ct);
  RW_ASSERT(block->req_done <= block->req_ct);

  /* Do nothing much if we're done sending or done entirely */
  if (block->req_sent < block->req_ct) {

    /* Send any needed queries */
    int reqidx;
    int mark_queued = 0;
    for (reqidx=0; reqidx < xact->n_req; reqidx++) {
      rwdts_router_xact_req_t *xreq = &xact->req[reqidx];
      rwdts_router_xact_sing_req_t *sing_req = NULL, *tmp_sing_req = NULL;
      if (xreq->block_idx == block->xact_blocks_idx) {
        /* Mark all but one as the anycast destination */
        rwdts_router_select_anycast_member(xact, xreq);
        
        HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
          switch (xact->state) {
            case RWDTS_ROUTER_XACT_PREPARE:
              // actual query message
              rwdts_router_prepare_f(xact, sing_req);
              if (RW_DL_LENGTH(&sing_req->membnode->member->queued_xacts)) {
                mark_queued = 1;
                if (xact->dbg) {
                  /* Make up an entry in our response for this req */
                  RWDtsTracerouteEnt ent;
                  
                  rwdts_traceroute_ent__init(&ent);
                  ent.func = (char*) __PRETTY_FUNCTION__;
                  ent.line = __LINE__;
                  ent.event = RWDTS_EVT_QUEUED;
                  ent.what = RWDTS_TRACEROUTE_WHAT_QUEUED;
                  ent.dstpath = sing_req->membnode->member->msgpath;
                  ent.srcpath = (char*)xact->dts->rwmsgpath;
                  ent.n_queryid = 0;
                  if (xact->dbg && xact->dbg->tr) {
                    if (xact->dbg->tr->print_) {
                      fprintf(stderr, "%s:%u: TYPE:%s, EVT:%s, DST:%s, SRC:%s\n", ent.func, ent.line,
                              rwdts_print_ent_type(ent.what), rwdts_print_ent_event(ent.event),
                              ent.dstpath, ent.srcpath);
                    }
                    sing_req->tracert_idx = rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
                  }
                  RWDTS_TRACE_EVENT_REQ(xact->dts->rwtaskletinfo->rwlog_instance, TraceReq,
                                        xact->xact_id_str,ent);
                }
              }
              break;
            case RWDTS_ROUTER_XACT_PRECOMMIT:
            case RWDTS_ROUTER_XACT_COMMIT:
            case RWDTS_ROUTER_XACT_ABORT:
              RW_ASSERT(block == xact->xact_blocks_event[xact->state]);
              rwdts_router_event_f(xact, xreq, block->xact_blocks_idx, sing_req);
              break;
              
            case RWDTS_ROUTER_XACT_QUEUED:
              break;
              
            default:
              // state-specific subx/xact event message(s)
              RW_CRASH();
              break;
          }
        }
      }
      if (block->req_sent == block->req_ct) {
        break;
      }
    }

    if (mark_queued) {
      xact->state = RWDTS_ROUTER_XACT_QUEUED;
    }
    RW_ASSERT(block->req_sent <= block->req_ct);
    RW_ASSERT(block->req_done <= block->req_ct);

  } else if (block->req_done >= block->req_ct) {
    RW_ASSERT(block->req_done >= block->req_ct);
    RW_ASSERT(block->req_sent >= block->req_ct);
  }

  /* This can arise anew from bouncing lots as we send above */
  if (block->req_done >= block->req_ct
      && block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING) {
    rwdts_router_xact_block_done(xact, block);
  }
}

static void rwdts_router_xact_free_req(rwdts_router_xact_t *xact)
{
  int reqidx;

  for (reqidx=0; reqidx < xact->n_req; reqidx++) {
    rwdts_router_xact_req_t *xreq = &xact->req[reqidx];
    rwdts_router_xact_sing_req_t *sing_req, *tmp_sing_req;
    HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
      RW_ASSERT(xreq->index == sing_req->xreq_idx);
      rwdts_router_delete_sing_req(xact, sing_req, false);
    }
    //RW_ASSERT(xreq->sing_req_ct == 0);
    xreq->sing_req = NULL;
    xreq->xact = NULL;
  }
}

void rwdts_router_xact_prepreqs_event(rwdts_router_t *dts,
                                      rwdts_router_xact_t *xact,
                                      rwdts_router_xact_state_t evt) 
{
  int blockidx = 0;
  int      req_ct;
  RW_ASSERT(evt < RWDTS_ROUTER_XACT_STATE_CT);

  rwdts_router_xact_block_t *block = xact->xact_blocks_event[evt];
  if (!block) {
    /* New block */

    /* Create a new block */
    blockidx = xact->n_xact_blocks;
    xact->n_xact_blocks++;
    xact->xact_blocks = realloc(xact->xact_blocks, (xact->n_xact_blocks * sizeof(rwdts_router_xact_block_t*)));

    block = (rwdts_router_xact_block_t *)RW_MALLOC0_TYPE(sizeof(rwdts_router_xact_block_t), rwdts_router_xact_block_t);
    xact->xact_blocks[blockidx] = block;
    block->xact_blocks_idx = blockidx;

    block->block_state = RWDTS_ROUTER_BLOCK_STATE_NEEDPREP;
    block->evt = evt;
    block->event_block = TRUE;
    block->waiting = TRUE;

    xact->xact_blocks_event[evt] = block;

  } else {
    RW_ASSERT(block->block_state >= RWDTS_ROUTER_BLOCK_STATE_NEEDPREP);
    blockidx = block->xact_blocks_idx;
  }
  if (block->block_state <= RWDTS_ROUTER_BLOCK_STATE_NEEDPREP) {
    RW_ASSERT(!xact->req_outstanding);

    uint32_t cl_blk = 0;
    if ((xact->state != RWDTS_ROUTER_XACT_ABORT) &&
        rwdts_precommit_to_client(xact, &cl_blk)) {
      rwdts_router_update_client_for_precommit(xact, cl_blk);
    }

    /* Sent to all non-NA members */
    rwdts_router_xact_free_req(xact);
    req_ct = HASH_CNT(hh_xact, xact->xact_union);
    if (req_ct > xact->n_req) {
      xact->req = realloc(xact->req, req_ct * sizeof(rwdts_router_xact_req_t));
    }
    memset(&xact->req[0], 0, (req_ct * sizeof(rwdts_router_xact_req_t)));
    xact->n_req = 0;
    
    rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_RUNNING);

    if (req_ct) {
      rwdts_router_member_node_t *membnode=NULL, *membtmp=NULL;
      HASH_ITER(hh_xact, xact->xact_union, membnode, membtmp) {
        /*If the member is in a wait restart, the member must have crashed and its replacement is not yet
          up. For the xacts that have passed the prepare phase, we do ahead and abort the xact. Ideally we
          should be retryin the xact here rwdts_router_xact_abort_and_retry*/
        if (membnode->member->wait_restart) {
          HASH_DELETE(hh_xact, xact->xact_union, membnode);
          RW_FREE(membnode);
          if ((evt != RWDTS_ROUTER_XACT_COMMIT)
              && (evt != RWDTS_ROUTER_XACT_ABORT)) { 
            xact->abrt = true;
            block->req_done_nack = true;
            RWDTS_ROUTER_LOG_XACT_EVENT(
                xact->dts, xact, DtsrouterXactDebug,
                RWLOG_ATTR_SPRINTF("WAIT_RESTART set for %s: ABRTing xact", membnode->member->msgpath));
          }
          continue;
        }
        rwdts_router_xact_req_t *xreq = &xact->req[xact->n_req];
        rwdts_router_xact_sing_req_t *sing_req;
        xreq->xact = xact;
        xreq->query_idx = 0;
        xreq->block_idx = blockidx;
        xreq->query_serial = xact->query_serial++;
        xreq->index = xact->n_req;
        xact->n_req++;
        sing_req = rwdts_router_create_sing_req(xreq, membnode->member, membnode);
        sing_req->xreq_state = RWDTS_ROUTER_QUERY_UNSENT;
        
        
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(xact->dts, xact, XactSendMsg,  "rwdts_router_xact_prepreqs_event sending event",
                                          sing_req->membnode->member->msgpath,
                                          xreq->index,
                                          xreq->query_serial);
        
        
      }
    }
    /*All interested members in this xact are in wait-restart or there are no members
      interested in this xact. */
    if (!xact->n_req){
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, XactInfo, "router_xact_prepreqs_event: xact has no destinations...");
      rwdts_router_xact_block_done(xact, block);
    }
  }
}

void rwdts_router_xact_block_prepreqs_prepare(rwdts_router_xact_t *xact,
                                              rwdts_router_xact_block_t *block,
                                              int blockidx)
{
  uint32_t isadvise = 0;
  if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_NEEDPREP) {
    int queryct = 0;
    RW_ASSERT(!block->req_done_nack); /* cannot still be in PREPARE if there was a nack? */
    RW_ASSERT(!block->req_outstanding);

    rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_RUNNING);
    //xact->blocks_advanced = TRUE;

    /* In lieu of routing, send all queries to all known destinations */
    block->req_ct = 0;
    queryct = block->xbreq->n_query;
    block->req_sent = 0;
    block->req_done = 0;
    block->req_outstanding = 0;
    block->req_done_nack = 0;

    if (queryct) {
      const RWDtsXactBlock *blk = block->xbreq;
      int query_idx;
      uint32_t reqidx = xact->n_req;
      xact->req = realloc(xact->req, ((xact->n_req+queryct) * sizeof(rwdts_router_xact_req_t)));
      //reset the newly allocated reqs
      memset(&xact->req[reqidx], 0, (queryct *sizeof(rwdts_router_xact_req_t)));
      
      for (query_idx=0; query_idx < queryct; query_idx++) {
        const RwSchemaPathSpec *ps = NULL;
        rw_keyspec_path_t *ks = NULL;
        const char *keystr = NULL;

        const RWDtsQuery *query = blk->query[query_idx];

        if (query->key
            && query->key->ktype == RWDTS_KEY_RWKEYSPEC
            && query->key->keyspec) {
          isadvise = (query->flags & RWDTS_XACT_FLAG_ADVISE);
          xact->isadvise |= isadvise;
          isadvise |= (query->flags & RWDTS_XACT_FLAG_SUB_READ);
          ps = query->key->keyspec;
          if (ps->has_binpath) {
            ks = rw_keyspec_path_create_with_dompath_binary_data(NULL, &ps->binpath);
          }
          if (query->key->keystr) {
            /* ZOMG the lifespan of the xact_blocks[] subx/query items must be longer than the lifespan of this xreq */
            keystr = query->key->keystr;
          }
        }
        RW_ASSERT(keystr);
        rwdts_router_xact_req_t *xreq = &xact->req[xact->n_req];
        xreq->xact = xact;
        xreq->query_idx = query_idx;
        xreq->block_idx = block->xact_blocks_idx;
        xreq->query_serial = xact->query_serial++;
        xreq->keystr = keystr;
        xreq->index = xact->n_req;
        xact->n_req++;
        
        rwdts_shard_t *head;

        head = xact->dts->rootshard;
        uint32_t n_chunks = 0;
        rwdts_shard_chunk_info_t **chunks = NULL;
        size_t j;
        bool subobj = false;

        /*Get all the matching members*/
        rwdts_shard_match_pathelm_recur(ks, head, &chunks, &n_chunks, NULL);
        
        if (isadvise) {
          subobj = rwdts_router_check_subobj(chunks, n_chunks);
        }
        for (j=0; j<n_chunks; j++) {  /* TBD: use chunk iterator to fetch records */
          if (isadvise) {
            if (chunks[j]->elems.rtr_info.n_subrecords) {
              rwdts_router_update_sub_match(xact, xreq, chunks[j], subobj);
            } 
          } 
          else {
            if (chunks[j]->elems.rtr_info.n_pubrecords) {
              rwdts_router_update_pub_match(xact, xreq, chunks[j], query->flags);
            }
          }
        }
        if (chunks) {
          free(chunks);
          chunks = NULL;
        }
        if (xact->abrt) {
          block->req_ct = 0;
          break;
        }
        if (ks) {
          rw_keyspec_path_free(ks, NULL);
        }
      }
    } else {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, XactError, "router_xact_prepreqa_prepare:: xact has no destinations...");
    }
  }

  xact->total_req_ct += block->req_ct;
  /* Are we done right out of the box? */
  if (!block->req_ct) {
    rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_DONE);
    RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                RWLOG_ATTR_SPRINTF("Calling rwdts_router_send_responses for block %d",
                                                   blockidx));
    rwdts_router_send_responses(xact, &block->rwmsg_cliid);
  }
}

void rwdts_trace_add_ts_point(RWDtsDebug *dbg)
{
      if (dbg && dbg->tr)
      {
      if (dbg->tr->n_ent)
      {
      struct timeval now;
      gettimeofday(&now, NULL);
      dbg->tr->ent[0]->evt[dbg->tr->ent[0]->n_evt] = 1;
      dbg->tr->ent[0]->n_evt++;
      dbg->tr->ent[0]->evt[dbg->tr->ent[0]->n_evt] = (uint32_t)now.tv_sec;
      dbg->tr->ent[0]->n_evt++;
      dbg->tr->ent[0]->evt[dbg->tr->ent[0]->n_evt] = (uint32_t)now.tv_usec;
      dbg->tr->ent[0]->n_evt++;
      }
      }
}
/* Send all available response information to the given item in the
   rwmsg_tab[].  Or, if rwmsg_tab_idx is -1, do so for every entry in
   the rwmsg_tab[].

   Return count of responses sent. */
static int
rwdts_router_send_responses(rwdts_router_xact_t *xact,
                            rwmsg_request_client_id_t *cliid)
{
  int blockidx;
  int retval = 0;

  if (cliid) {
    if (!(RWMSG_REQUEST_CLIENT_ID_VALID(cliid))) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, XactError, "messaging end point invalid");
      return retval;
    }
  }

  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("rwdts_router_send_responses(cliid %s) rwmsg_tab_len=%d do_commits=%d", 
                                                 cliid ? "given" : "absent", xact->rwmsg_tab_len, xact->do_commits));

  int r;
  for (r=0; r<xact->rwmsg_tab_len; r++) {

    if (!(RWMSG_REQUEST_CLIENT_ID_VALID(&xact->rwmsg_tab[r].rwmsg_cliid))) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, XactError, "messaging end point invalid");
      continue;
    }

    /* Looking for a particular client, and this isn't the one */
    if (cliid && 0!=memcmp(cliid, &xact->rwmsg_tab[r].rwmsg_cliid, sizeof(*cliid))) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("rwdts_router_send_responses wrong cliid in rwmsg_tab[%d]", r));
      continue;
    } else if (cliid) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("rwdts_router_send_responses cliid match in rwmsg_tab[%d]", r));
    }

    /* We have no rwmsg request handle for this client right now */
    if (!xact->rwmsg_tab[r].rwmsg_req) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("rwdts_router_send_responses no rwmsg_req on hand in tab[%d]", r));
      xact->rwmsg_tab[r].dirty = TRUE;
      continue;
    }

    /* Build response! */
    RWDtsXactResult xres;
    rwdts_xact_result__init(&xres);

    xres.n_result = 0;

    xres.has_status = TRUE;
    xres.has_more = TRUE;

    xres.status = RWDTS_XACT_RUNNING;
    if (!xact->do_commits) {
      xres.more = FALSE;
      if (xact->abrt) {
        xres.status = RWDTS_XACT_FAILURE;
        xres.more = FALSE;
      } else {
        /* If all blocks for for this client are done or responded,
           that's good enough to signal the end for non-transactional
           xacts */
        /* Done state should not be sent if the end flag is not received.
         The client can add more blocks for execution */
        if (xact->ended) {
          xres.status = RWDTS_XACT_COMMITTED;
          xres.more = FALSE;
        }
        for (blockidx = 0; blockidx < xact->n_xact_blocks; blockidx++) {
          rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];
          if (!block->event_block) {
            if (0==memcmp(&block->rwmsg_cliid, &xact->rwmsg_tab[r].rwmsg_cliid, sizeof(block->rwmsg_cliid))) {
              if (!(block->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE
                    || block->block_state == RWDTS_ROUTER_BLOCK_STATE_RESPONDED)) {
                xres.status = RWDTS_XACT_RUNNING;
                xres.more = TRUE;
                break;
              }
            }
          }
        }
      }
    } else {
      /* Transaction, running unless an event block sets otherwise below */
      xres.status = RWDTS_XACT_RUNNING;
      xres.more = TRUE;
    }

    int rsp_blocks=0;
    int rsp_queries=0;

    for (blockidx = 0; blockidx < xact->n_xact_blocks; blockidx++) {
      rwdts_router_xact_block_t *block = xact->xact_blocks[blockidx];

      if (block->event_block) {
        /* Include only the final event block's status in the toplevel response message */
        if (blockidx == xact->n_xact_blocks-1) {
          RW_ASSERT(xact->state != RWDTS_ROUTER_XACT_PREPARE);
          RW_ASSERT(block->block_state != RWDTS_ROUTER_BLOCK_STATE_RESPONDED); /* unpossible for eventblocks, they respond to all, as noted by rwmsg_tab[].done on all tab entries */
          xres.more = TRUE;
          if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE) {
            if (block->req_done_err || block->req_done_nack) {
              /* Any failure or nack for event blocks ([pre]commit / commit / abort) represents a failure */
              xres.status =
                (block->evt==RWDTS_ROUTER_XACT_ABORT) ?
                  RWDTS_XACT_ABORTED: RWDTS_XACT_FAILURE;
              xres.more = FALSE;
              xact->rwmsg_tab[r].done = TRUE;
              RW_ASSERT(xact->n_xact_blocks-1 == blockidx); /* terminal state, should be last block */
            } else {
              switch (block->evt) {
              case RWDTS_ROUTER_XACT_PRECOMMIT:
                xres.status = RWDTS_XACT_RUNNING;
                xres.more = TRUE;
                break;
              case RWDTS_ROUTER_XACT_COMMIT:
                xres.status = RWDTS_XACT_COMMITTED;
                xact->rwmsg_tab[r].done = TRUE;
                RW_ASSERT(xact->n_xact_blocks-1 == blockidx); /* terminal state, should be last block */
                xres.more = FALSE;
                break;
              case RWDTS_ROUTER_XACT_ABORT:
                xres.status = RWDTS_XACT_ABORTED;
                xact->rwmsg_tab[r].done = TRUE;
                RW_ASSERT(xact->n_xact_blocks-1 == blockidx); /* terminal state, should be last block */
                xres.more = FALSE;
                break;
              case RWDTS_EVT_PREPARE:
              default:
                xres.more = TRUE;
                RW_CRASH();
              }
            }
          }
        }
      } else if (block->block_state != RWDTS_ROUTER_BLOCK_STATE_RESPONDED) {

        if (0==memcmp(&block->rwmsg_cliid, &xact->rwmsg_tab[r].rwmsg_cliid, sizeof(block->rwmsg_cliid))) {
          /* Query block */

          if (block->xbreq && block->xbreq->n_query) {
            int idx;
            bool cont_loop = false;
            if (xact->state == RWDTS_ROUTER_XACT_PREPARE) {
              for(idx = 0; idx < block->xbreq->n_query; idx++) {
                if (block->xbreq->query[idx]->flags & RWDTS_XACT_FLAG_RETURN_PAYLOAD) {
                  cont_loop = true;
                  break;
                }
              }
              if (cont_loop == true) {
                continue;
              }
            }
          }

          /* Send if it has responses or is finished */
          if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING
              || block->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE) {

            if (xres.status == RWDTS_XACT_FAILURE) {
              rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_DONE);
            }
            if (block->waiting && block->block_state != RWDTS_ROUTER_BLOCK_STATE_DONE) {
              /* Wait for all the responses */
              RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                          RWLOG_ATTR_SPRINTF("Block %d not done, waiting", blockidx));
              RW_ASSERT((xact->state == RWDTS_ROUTER_XACT_PREPARE) 
                        || (xact->state == RWDTS_ROUTER_XACT_QUEUED));
              continue;                /* next block */
            }

            /* Wait for all responses, regardless.  Do the polling / windowing / credit thing another day */
            if (TRUE && block->block_state != RWDTS_ROUTER_BLOCK_STATE_DONE) {
              /* Wait for all the responses */
              RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                          RWLOG_ATTR_SPRINTF("Block %d not done, waiting (forced on)", blockidx));
              RW_ASSERT((xact->state == RWDTS_ROUTER_XACT_PREPARE) 
                        || (xact->state == RWDTS_ROUTER_XACT_QUEUED));
              continue;                /* next block */
            }

            if (!xres.result) {
              xres.result = RW_MALLOC0(xact->n_xact_blocks * sizeof(RWDtsXactBlockResult *));
            }

            if (!block->xbres) {
              rwdts_router_block_xbres_init(xact, block);
              if (block->req_done_internal) {
                RW_ASSERT(block->req_done_internal == 1);
                RW_ASSERT(block->req_done_internal == block->req_done);
                block->xbres->has_evtrsp = TRUE;
                block->xbres->evtrsp = RWDTS_EVTRSP_INTERNAL;
              }
            }

            /* FIXME limit send size to credits */

            RWDtsXactBlockResult *xbres = block->xbres;
            RW_ASSERT(xbres);

            xres.result[xres.n_result++] = xbres;
            block->xbres = NULL;

            rsp_blocks ++;
            rsp_queries += xbres->n_result;

            if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING) {
              xbres->has_more = TRUE;
              xbres->more = TRUE;
            } else if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_DONE
                       && TRUE /* FIXME limit sending by credits */) {
              RW_ASSERT(!block->event_block);
              rwdts_router_xact_block_newstate(xact, block, RWDTS_ROUTER_BLOCK_STATE_RESPONDED);
              xbres->has_more = TRUE;
              xbres->more = FALSE;
            }
          }
        }
      }
    }


    RWMEMLOG(&xact->rwml_buffer,
	     RWDTS_MEMLOG_FLAGS_A,
	     "Responding",
	     RWMEMLOG_ARG_PRINTF_INTPTR("blocks %"PRIdPTR, rsp_blocks),
	     RWMEMLOG_ARG_PRINTF_INTPTR("queries %"PRIdPTR, rsp_queries),
	     RWMEMLOG_ARG_ENUM_FUNC(rwdts_xact_main_state_to_str, "", xres.status),
	     RWMEMLOG_ARG_PRINTF_INTPTR("more %"PRIdPTR, (xres.more?1:0)));
    

    /* Include dbg trace in final response to first client */
    if (r == 0 && (xact->rwmsg_tab[r].done || xres.status == RWDTS_XACT_COMMITTED
                   || xres.status == RWDTS_XACT_FAILURE
                   || xres.status == RWDTS_XACT_ABORTED)) {
      rwdts_trace_add_ts_point(xact->dbg);
      xres.dbg = xact->dbg;
      xact->dbg = NULL;
    }

    RW_ASSERT((void*)xres.base.descriptor == (void*)&rwdts_xact_result__descriptor);

    /* Actually skip this if we sent no block results and have no
       interesting / terminal toplevel state */
    if (rsp_blocks
        || xres.status != RWDTS_XACT_RUNNING
        || xact->rwmsg_tab[r].dirty) {
      retval++;
      xact->rwmsg_tab[r].dirty = FALSE;
      rwmsg_request_send_response_pbapi(xact->rwmsg_tab[r].rwmsg_req, &xres.base);
      xact->rwmsg_tab[r].rwmsg_req = NULL;
      rwdts_router_async_send_timer_cancel(xact, r);
    }

    protobuf_c_message_free_unpacked_usebody(NULL, &xres.base);

    if (cliid) {
      /* There should be only one for each client.  This should change
         to he M:N, really, or to support 1:1 behavior around
         streaming blocks, but for now let's keep it simple. */
      RW_ASSERT(0==memcmp(cliid, &xact->rwmsg_tab[r].rwmsg_cliid, sizeof(*cliid)));
      break;
    }
  }

  return retval;
}

/* Drive state forward by sending any requests we should; call after
   processing every event */
void rwdts_router_xact_run(rwdts_router_xact_t *xact)
{
  rwdts_router_t *dts = xact->dts;
  int once = 0;

  RW_ASSERT(xact);
  RW_ASSERT(xact->state > RWDTS_ROUTER_XACT_INIT);

  rwdts_router_xact_state_t ostate = xact->state;

  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("run line=%d xact=%p ostate=%s curstate=%s  START",
                                                 __LINE__, xact, rwdts_router_state_to_str(ostate),
                                                 rwdts_router_state_to_str(xact->state)));
  while (!once++
         || xact->blocks_advanced
         || ostate != xact->state
         || rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_NEEDPREP)
         || rwdts_router_xact_blocks_sendable_ct(xact)) {

    ostate = xact->state;
    xact->blocks_advanced = FALSE;


    rwdts_router_xact_block_t *evblock = xact->xact_blocks_event[xact->state];

    switch (xact->state) {
      case RWDTS_ROUTER_XACT_PREPARE:
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactInfo, "_router_xact_run state=PREPARE");

        /* The prepare state is its own little machine centered around
           deciding which block to execute etc.  So this function does
           that, and ultimately calls prepreqs_prepare(). */
        rwdts_router_xact_prepare_run(xact);
        break;

      case RWDTS_ROUTER_XACT_PRECOMMIT:
      case RWDTS_ROUTER_XACT_COMMIT:
      case RWDTS_ROUTER_XACT_ABORT:
#if 0 /* FIXME - following doesn't work like printf */
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactDebug, "_router_xact_run state=%s", rwdts_router_state_to_str(xact->state));
#endif
        if (!evblock || evblock->block_state <= RWDTS_ROUTER_BLOCK_STATE_NEEDPREP) {
          rwdts_router_xact_prepreqs_event(dts, xact, xact->state);
        }
        break;

      case RWDTS_ROUTER_XACT_DONE:
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactDebug, "_router_xact_run state=DONE");

        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactDebug, "sending accumulated response");

        rwdts_router_send_responses(xact, NULL);

        if (xact->do_commits) {
          /* Transactional termination: all clients have received a terminal state */
          RW_ASSERT(!xact->n_xact_blocks
                    || xact->xact_blocks_event[RWDTS_ROUTER_XACT_ABORT]
                    || xact->xact_blocks_event[RWDTS_ROUTER_XACT_COMMIT]
                    || xact->req_err);
          if (rwdts_router_xact_rwmsg_tab_done(xact) >= xact->rwmsg_tab_len) {
            rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_TERM);
          }
        } else {
          /* Nontransactional termination: all blocks are query blocks and have been responded to the client */
          if (xact->n_xact_blocks == rwdts_router_xact_blocks_state_ct(xact, RWDTS_ROUTER_BLOCK_STATE_RESPONDED)) {
            rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_TERM);
          }
        }
        break;

      case RWDTS_ROUTER_XACT_TERM:
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactDebug, "_router_xact_run state=TERM");
        rwdts_router_xact_destroy(dts, xact);
        // xact may be invalid now
        xact = NULL;
        return;

      case RWDTS_ROUTER_XACT_QUEUED:
        return;

      default:
      case RWDTS_ROUTER_XACT_INIT:
        RWDTS_ROUTER_LOG_XACT_DEBUG_EVENT(dts, xact, XactDebug, "_router_xact_run=INIT ??");
        RWDTS_ROUTER_XACT_ABORT_MESSAGE(xact,  "_router_xact_run state=%d ??\n", (int)xact->state);
        return;
    }

    /* Send any needed requests.  Also notice if the current xreq set is done, and advance state */
    if ((xact->state != RWDTS_ROUTER_XACT_TERM)) {

      int i=0;
      RW_ASSERT(xact->n_xact_blocks >= 1);
      for (i=0; i<xact->n_xact_blocks; i++) {
        rwdts_router_xact_block_t *block = xact->xact_blocks[i];
        if (block->block_state == RWDTS_ROUTER_BLOCK_STATE_RUNNING) {
          rwdts_router_xact_sendreqs(xact, block);
        }
      }
    }
  }

  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("run line=%d xact=%p ostate=%s curstate=%s  RETURN",
                                                 __LINE__, xact, rwdts_router_state_to_str(ostate),
                                                 rwdts_router_state_to_str(xact->state)));
}

rwdts_router_xact_t *rwdts_router_xact_lookup_by_id(const rwdts_router_t* dts,
                                                    const RWDtsXactID* id) {
  rwdts_router_xact_t *xact = NULL;
  RWDTS_FIND_WITH_PBKEY_IN_HASH(hh, dts->xacts, id, router_idx, xact);
  return xact;
}
static rwdts_router_xact_block_t *rwdts_xact_block_lookup_by_id(const rwdts_router_xact_t *xact,
                                                                const RWDtsXactBlkID* id) {
  int i=0;
  for (i=0; i<xact->n_xact_blocks; i++) {
    RWDtsXactBlkID *bid = &xact->xact_blocks[i]->blockid;
    if (bid->blockidx == id->blockidx
        && bid->clientidx == id->clientidx
        && bid->routeridx == id->routeridx) {
      return xact->xact_blocks[i];
    }
  }
  return NULL;
}


static void
rwdts_router_xact_update_credits(rwdts_router_xact_block_t *block, const RWDtsXact *cred)
{
  int i, j;

  for (i = 0; i < cred->n_subx; i++) {
    if (cred->subx[i] && cred->subx[i]->block->blockidx == block->blockid.blockidx) {
      RW_ASSERT(cred->subx[i]->n_query == block->xbreq->n_query);
      for (j = 0; j < cred->subx[i]->n_query; j++) {
        block->xbreq->query[j]->credits = cred->subx[i]->query[j]->credits;
      }
    }
  }
  return;
}

void
rwdts_router_xact_update(rwdts_router_xact_t *xact,
                         rwmsg_request_t *exec_req,
                         RWDtsXact *input)
{
  RW_ASSERT(xact);

  bool respond_now = FALSE;
  bool append_ok = TRUE;                /* Is it OK to add new blocks to this xact? */
  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            exec_req, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Xact Update)");

  if (input->dbg && (input->dbg->dbgopts1&RWDTS_DEBUG_TRACEROUTE)) {
    if (!xact->dbg) {
      /* Make a tracert object in our xact structure to attach to the final response */
      xact->dbg = (RWDtsDebug*)RW_MALLOC0(sizeof(RWDtsDebug));
      rwdts_debug__init(xact->dbg);
      xact->dbg->dbgopts1 |= RWDTS_DEBUG_TRACEROUTE;
      xact->dbg->has_dbgopts1 = TRUE;
      if (!xact->dbg->tr) {
        rwdts_dbg_add_tr(xact->dbg, NULL);
      }
    }
  }

  /* Dud state.  Fail the thing. */
  switch (xact->state) {
  case RWDTS_ROUTER_XACT_PREPARE:
  case RWDTS_ROUTER_XACT_INIT:
    break;
  default:
    break;
  }

  /* Get the client key from rwmsg */
  rwmsg_request_client_id_t rwmsg_cliid;
  rs = rwmsg_request_get_request_client_id(exec_req, &rwmsg_cliid);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  /* Find an entry for this request in the rwmsg request table */
  int rwmsg_tab_idx = 0;
  bool fallback_chk = false;
  for (rwmsg_tab_idx = 0; rwmsg_tab_idx < xact->rwmsg_tab_len; rwmsg_tab_idx++) {
    if (!(RWMSG_REQUEST_CLIENT_ID_VALID(&xact->rwmsg_tab[rwmsg_tab_idx].rwmsg_cliid))) {
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, XactError, "messaging end point invalid");
      continue;
    }
    if (0==memcmp(&rwmsg_cliid, &xact->rwmsg_tab[rwmsg_tab_idx].rwmsg_cliid, sizeof(rwmsg_cliid))) {
      /* Already have one */
      break;
    }
    if (!strcmp(xact->rwmsg_tab[rwmsg_tab_idx].client_path, input->client_path)) {
      fallback_chk = true;
      /*fall back to client_path compare */
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("fallback check passed though cliid is coming different from %s",
                                                     xact->rwmsg_tab[rwmsg_tab_idx].client_path));
      break;
    }
  }
  rwdts_router_xact_rwmsg_ent_t *rwmsg_ent = NULL;
  if (rwmsg_tab_idx == xact->rwmsg_tab_len) {
    xact->rwmsg_tab = realloc(xact->rwmsg_tab, (1+rwmsg_tab_idx)*sizeof(xact->rwmsg_tab[0]));
    RW_ASSERT(xact->rwmsg_tab);
    xact->rwmsg_tab_len++;
    rwmsg_ent = &xact->rwmsg_tab[rwmsg_tab_idx];
    memset(rwmsg_ent, 0, sizeof(*rwmsg_ent));
    memcpy(&rwmsg_ent->rwmsg_cliid, &rwmsg_cliid, sizeof(rwmsg_ent->rwmsg_cliid));
    rwmsg_ent->client_path = RW_STRDUP(input->client_path);
  } else {
    rwmsg_ent = &xact->rwmsg_tab[rwmsg_tab_idx];
    RW_ASSERT(!strcmp(rwmsg_ent->client_path, input->client_path));
    if (fallback_chk) {
      int i;
      rwdts_router_xact_block_t *block = NULL;
      for (i=0; i < xact->n_xact_blocks; i++) {
        block = xact->xact_blocks[i];
        if (!memcmp(&rwmsg_ent->rwmsg_cliid, &block->rwmsg_cliid,  sizeof(rwmsg_ent->rwmsg_cliid))) {
          memcpy(&block->rwmsg_cliid, &rwmsg_cliid, sizeof(block->rwmsg_cliid));
          break;
        }
      }
      memcpy(&rwmsg_ent->rwmsg_cliid, &rwmsg_cliid, sizeof(rwmsg_ent->rwmsg_cliid));
    }
  }
  RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("__xact_update %d len ", xact->rwmsg_tab_len));

  /* End flag? */
  int end = ( (input->flags & RWDTS_XACT_FLAG_END) );
  if (end) {
    if (rwmsg_tab_idx != 0) {
      /* BUG only the sender of request 0 should be allowed to send end */
      RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, NonOwnerXactEndAttempt,  "Non originator of the transaction has tried to end it");
      xact->abrt = TRUE;
      respond_now = TRUE;
    } else {
      xact->ended = TRUE;
      RWDTS_RTR_ADD_TR_ENT_ENDED(xact);   
    }
  }

  RWDTS_RTR_DEBUG_TRACE_APPEND(xact, input);

  /* Return the previous request from that client, if any */
  if (rwmsg_ent->rwmsg_req) {
    rwmsg_ent->dirty = TRUE;        /* force it out */
    int ct = rwdts_router_send_responses(xact, &rwmsg_cliid);
    RW_ASSERT(ct == 1);
  }

  /* Stash this exec_req in the rwmsg_tab */
  RW_ASSERT(!rwmsg_ent->rwmsg_req);
  rwmsg_ent->rwmsg_req = exec_req;

  if (rwmsg_ent->done) {
    /* Xact is done and this guy already was told. */
    append_ok = FALSE;
    respond_now = TRUE;
  } else if (rwmsg_ent->dirty) {
    respond_now = TRUE;
  }

  /* We are supposed to answer in 10s in case there's nothing happening while some member thinks. */
  rwdts_router_async_send_timer_start(xact, rwmsg_tab_idx);

  /* Now ponder the blocks.  There may be new ones, add them */
  int subxct = input->n_subx;
  int subidx = 0;
  
  for (subidx=0; subidx<subxct; subidx++) {
    int i;
    rwdts_router_xact_block_t *block = NULL;

    block = rwdts_xact_block_lookup_by_id(xact, input->subx[subidx]->block);
    if (!block) {
      if (!append_ok) {

        RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, IllegalBlockAddition,  "BUG BUG BUG Illegal block addition / aborting xact");

        /* No can do, bail */
        xact->abrt = TRUE;
        // TBD add error

        /* Jettison this one as well for simplicity */
        protobuf_c_message_free_unpacked(NULL, &input->subx[subidx]->base);
        input->subx[subidx] = NULL;

      } else {
        /* Create a new block for each unknown subx in the input message */
        i = xact->n_xact_blocks;
        xact->n_xact_blocks++;

        xact->xact_blocks = realloc(xact->xact_blocks, (xact->n_xact_blocks * sizeof(rwdts_router_xact_block_t*)));
        block = (rwdts_router_xact_block_t*)RW_MALLOC0_TYPE(sizeof(rwdts_router_xact_block_t), rwdts_router_xact_block_t);
        xact->xact_blocks[i] = block;
        block->xact_blocks_idx = i;
        memcpy(&block->rwmsg_cliid, &rwmsg_cliid, sizeof(block->rwmsg_cliid));

        block->evt = RWDTS_ROUTER_XACT_PREPARE;

        /* We are stealing this copy of the block from inside the request.  Note also the free of the array, below */
        block->xbreq = input->subx[subidx];
        input->subx[subidx] = NULL;

        rwdts_xact_blk_id__init(&block->blockid);
        block->blockid.routeridx = block->xbreq->block->routeridx;
        block->blockid.clientidx = block->xbreq->block->clientidx;
        block->blockid.blockidx = block->xbreq->block->blockidx;

        rwdts_router_wait_block_t *wait_block = NULL;
        RWDTS_FIND_WITH_PBKEY_IN_HASH(hh_wait_blocks, xact->wait_blocks, 
                                      block->xbreq->block, routeridx, wait_block);
        if (wait_block) {
          RWDTS_ROUTER_LOG_XACT_EVENT(xact->dts, xact, DtsrouterXactDebug,
                                      RWLOG_ATTR_SPRINTF("Waited block [%lu:%lu:%u] %p from %s got from %s",
                                                         wait_block->blockid.routeridx, wait_block->blockid.clientidx,
                                                         wait_block->blockid.blockidx, wait_block, wait_block->member_msg_path,
                                                         rwmsg_ent->client_path));
          HASH_DELETE(hh_wait_blocks, xact->wait_blocks, wait_block);
          RW_FREE(wait_block->member_msg_path);
          RW_FREE_TYPE(wait_block, rwdts_router_wait_block_t);
        }

        block->waiting = FALSE;
        if (input->has_flags) {
          if ( (input->flags & RWDTS_XACT_FLAG_WAIT) ) {
            block->waiting = TRUE;
          }
        }
        if ( block->xbreq->flags & RWDTS_XACT_FLAG_WAIT) {
          block->waiting = TRUE;
        }
        if ( block->xbreq->flags & RWDTS_XACT_FLAG_EXECNOW) {
          block->block_state = RWDTS_ROUTER_BLOCK_STATE_NEEDPREP;
        }
        if (!block->xact_blocks_idx &&  (block->xbreq->flags & RWDTS_XACT_FLAG_END)) {
          xact->ended = TRUE;
          RWDTS_RTR_ADD_TR_ENT_ENDED(xact);   
        }

        if (xact->dbg) {
          RWDtsTracerouteEnt ent;
          
          rwdts_traceroute_ent__init(&ent);
          ent.line = __LINE__;
          ent.func = (char*)__PRETTY_FUNCTION__;
          ent.what = RWDTS_TRACEROUTE_WHAT_ADDBLOCK;
          ent.dstpath = (char*)xact->dts->rwmsgpath;
          ent.block = block->xbreq;
          rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
          RWDTS_TRACE_EVENT_ADD_BLOCK(xact->dts->rwtaskletinfo->rwlog_instance,TraceAddBlock, xact->xact_id_str, NULL, ent);
        }

	RWMEMLOG(&xact->rwml_buffer,
		 RWDTS_MEMLOG_FLAGS_A,
		 "New block",
		 RWMEMLOG_ARG_PRINTF_INTPTR("bidx %"PRIdPTR, block->xact_blocks_idx),
		 RWMEMLOG_ARG_PRINTF_INTPTR("block id r%"PRIuPTR, block->blockid.routeridx),
		 RWMEMLOG_ARG_PRINTF_INTPTR("/c%"PRIuPTR, block->blockid.clientidx),
		 RWMEMLOG_ARG_PRINTF_INTPTR("/b%"PRIuPTR, block->blockid.blockidx),
		 RWMEMLOG_ARG_PRINTF_INTPTR(" queryct %"PRIdPTR, block->xbreq->n_query));
	int q;
	for (q=0; q<block->xbreq->n_query; q++) {
	  const RWDtsQuery *query = block->xbreq->query[q];
	  const char *keystr = ((query->key && query->key->keystr) ? query->key->keystr : "??");
	  int len = strlen(keystr);
	  const char *keytail = keystr + len - MIN(len, 80);

	  RWMEMLOG(&xact->rwml_buffer,
		   RWDTS_MEMLOG_FLAGS_A,
		   "  Query",
		   RWMEMLOG_ARG_PRINTF_INTPTR(" bidx %"PRIdPTR, block->xact_blocks_idx),
		   RWMEMLOG_ARG_PRINTF_INTPTR(" qidx %"PRIdPTR, q),
		   RWMEMLOG_ARG_ENUM_FUNC(rwdts_action_to_str, "", query->action),
		   RWMEMLOG_ARG_STRNCPY(80, keytail));
          RWMEMLOG(&xact->dts->rwml_buffer,
                   RWMEMLOG_MEM0, "Xact update",
                   RWMEMLOG_ARG_STRNCPY(sizeof(xact->xact_id_str), xact->xact_id_str),
                   RWMEMLOG_ARG_ENUM_FUNC(rwdts_action_to_str, "", query->action),
		   RWMEMLOG_ARG_STRNCPY(80, keytail));
	}
      }
      
    } else {
      
      // belongs in xact_update, known block case
      if (input->has_flags && (input->flags & RWDTS_XACT_FLAG_UPDATE_CREDITS)) {
        /* FIXME TODO this update re-iterates the whole input message every time */
        rwdts_router_xact_update_credits(block, input);
      }
      if (!block->xact_blocks_idx &&  (block->xbreq->flags & RWDTS_XACT_FLAG_END)) {
        xact->ended = TRUE;
        RWDTS_RTR_ADD_TR_ENT_ENDED(xact);   
      }

      /* Jettison this one as well for simplicity */
      protobuf_c_message_free_unpacked(NULL, &input->subx[subidx]->base);
      input->subx[subidx] = NULL;
    }
  }

  /* We are stealing these blocks, free them out from under the arriving Pb */
  RW_FREE(input->subx);
  input->subx = NULL;
  input->n_subx = 0;

  if (respond_now) {
    rwmsg_ent->dirty = TRUE;
    int ct = rwdts_router_send_responses(xact, &rwmsg_cliid);
    RW_ASSERT(ct == 1);
  }
  return;
}

void
rwdts_router_update_pub_obj(rwdts_router_member_t *memb, uint32_t* n_obj_list,
                            rwdts_router_pub_obj_t*** pub_obj_list, rw_keyspec_path_t* ks)
{
  size_t missat = 0;
  bool flag = 0;
  rw_status_t rw_status;
  RW_ASSERT(memb);
  RW_ASSERT(ks);
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  HASH_ITER(hh_reg, memb->registrations, reg, tmp_reg) {
    flag = rw_keyspec_path_is_match_detail(NULL, reg->keyspec, ks, &missat, NULL);
    if (flag) {
      *n_obj_list += 1;
      (*pub_obj_list) = realloc((*pub_obj_list), (*n_obj_list * sizeof(rwdts_router_pub_obj_t *)));
      (*pub_obj_list)[*n_obj_list-1] = RW_MALLOC0(sizeof(rwdts_router_pub_obj_t));
      rw_status = rw_keyspec_path_create_dup(reg->keyspec, NULL, &(*pub_obj_list)[*n_obj_list-1]->keyspec);
      RW_ASSERT(rw_status == RW_STATUS_SUCCESS);
      (*pub_obj_list)[*n_obj_list-1]->flags = reg->flags;
    }
  }
  return;
}

static void
rwdts_free_pub_obj_list(rwdts_router_xact_t* xact)
{
  int i;
  for (i =0; i < xact->n_obj_list; i++) {
    if (xact->pub_obj_list && xact->pub_obj_list[i]) {
      rw_keyspec_path_free(xact->pub_obj_list[i]->keyspec, &xact->ksi);
      RW_FREE(xact->pub_obj_list[i]);
      xact->pub_obj_list[i] = NULL;
    }
  }
  if (xact->pub_obj_list) {
    free(xact->pub_obj_list);
    xact->pub_obj_list = NULL;
  }
  xact->n_obj_list = 0;
  return;
}

void
rwdts_router_sub_obj(rwdts_memb_router_registration_t* reg, uint32_t n_obj_list,
                     rwdts_router_pub_obj_t** pub_obj_list, int* pubsubmatch, bool* ksmatch, bool *flag,
                     char *matchdesc)
{
  size_t reg_len = rw_keyspec_path_get_depth(reg->keyspec);
  if (reg->flags & RWDTS_FLAG_DEPTH_OBJECT) {
    int i=0;
    for (i=0; i < n_obj_list; i++) {
      if (pub_obj_list && pub_obj_list[i]) {
        size_t pub_len = rw_keyspec_path_get_depth(pub_obj_list[i]->keyspec);
        if (pub_obj_list[i]->flags & RWDTS_FLAG_SUBOBJECT) {
          if (pub_len > reg_len) {
            *flag = rw_keyspec_path_is_branch_detail(NULL, pub_obj_list[i]->keyspec, reg->keyspec, false, NULL, NULL);
            if (*flag) {
              *pubsubmatch = false;
              *ksmatch = true;
              strcpy(matchdesc, "object match");
              break;
            }
          }
        } else {
          *flag = rw_keyspec_path_is_match_detail(NULL, pub_obj_list[i]->keyspec, reg->keyspec, NULL, NULL);
          if (*flag) {
            *pubsubmatch = false;
            *ksmatch = true;
            strcpy(matchdesc, "specific match");
            break;
          }
        }
      }
    }
    if (i == n_obj_list) {
      *pubsubmatch = false;
      *ksmatch = false;
    }
  }
}

void
rwdts_router_pub_obj(rwdts_memb_router_registration_t* reg, rw_keyspec_path_t* ks,
                     int* pubsubmatch, bool* ksmatch, bool *flag, char *matchdesc,
                     size_t* missat)
{
  size_t ks_len = rw_keyspec_path_get_depth(ks);
  size_t reg_len = rw_keyspec_path_get_depth(reg->keyspec);
  if (reg->flags & RWDTS_FLAG_SUBOBJECT) {
    if (reg_len > ks_len) {
      *flag = rw_keyspec_path_is_branch_detail(NULL, reg->keyspec, ks, false, NULL, NULL);
      if (*flag) {
        *pubsubmatch = false;
        *ksmatch = true;
        strcpy(matchdesc, "object match");
        *missat = 0;
      }
    }
  } else {
    if (reg_len > ks_len) {
      *pubsubmatch = false;
      *ksmatch = false;
    }
  }
}

static bool
rwdts_router_check_subobj(rwdts_shard_chunk_info_t** chunks, uint32_t n_chunks)
{
  int j;
  rwdts_chunk_rtr_info_t *rtr_info, *tmp_rtr_info;
 
  for (j = 0; j < n_chunks; j++) {
    RW_ASSERT(chunks[j]);
    HASH_ITER(hh_rtr_record, chunks[j]->elems.rtr_info.pub_rtr_info, rtr_info, tmp_rtr_info) {
      if (rtr_info->flags & RWDTS_FLAG_SUBOBJECT) {
        return true;
      }
    }
  }
  return false;
}

static void
rwdts_router_update_sub_match(rwdts_router_xact_t *xact,
                              rwdts_router_xact_req_t *xreq, 
                              rwdts_shard_chunk_info_t *chunk,
                              bool subobj)
{
  rwdts_chunk_rtr_info_t *rtr_info= NULL, *tmp_rtr_info = NULL;
  rwdts_router_xact_sing_req_t *sing_req;
  HASH_ITER(hh_rtr_record, chunk->elems.rtr_info.sub_rtr_info, rtr_info, tmp_rtr_info) {

    if (rtr_info->flags & RWDTS_FLAG_DEPTH_OBJECT) {
      if (subobj == false) {
        continue;
      }
    }

    /*If the member is in a wait restart, the member must have crashed and its replacement is not yet
      up. This xact has not yet started the transaction, we do ahead and abort the xact. Ideally we
      should be retryin the xact here rwdts_router_xact_abort_and_retry*/
    if (rtr_info->member->wait_restart) {
      rwdts_router_member_node_t *membnode=NULL;
      HASH_FIND(hh_xact, xact->xact_union, rtr_info->member->msgpath,  strlen(rtr_info->member->msgpath),  membnode);
      if (membnode) {
        xact->abrt = true;
        RWDTS_ROUTER_LOG_XACT_EVENT(
            xact->dts, xact, DtsrouterXactDebug,
            RWLOG_ATTR_SPRINTF("WAIT_RESTART set for %s: ABRTing xact", rtr_info->member->msgpath));
        return;
      }
    }
    /*
      Even though there are multiple chunks with the same member information in the array of chunks, the HASH FIND for
      the sing requests will take care of not sending more than one prepare per member. This is done when creating the
      sing request below.
    */
    sing_req = rwdts_router_create_sing_req(xreq, rtr_info->member, NULL);
    RW_ASSERT(sing_req);
    if (sing_req){
      RW_ASSERT(sing_req->membnode);
      //Update the membnode flags from the rtr_info flags
      sing_req->membnode->ignore_bounce = (rtr_info->flags & RWDTS_FLAG_REGISTRATION_BOUNCE_OK)?1:0;
    }
  }
  return;
}

static void
rwdts_router_update_pub_match(rwdts_router_xact_t* xact, 
                              rwdts_router_xact_req_t *xreq, 
                              rwdts_shard_chunk_info_t *chunk,
                              uint32_t flags)
{
  rwdts_chunk_rtr_info_t *rtr_info= NULL, *tmp_rtr_info = NULL;
  
  HASH_ITER(hh_rtr_record, chunk->elems.rtr_info.pub_rtr_info, rtr_info, tmp_rtr_info) {
    rwdts_router_xact_sing_req_t* sing_req = NULL;

    if (flags & RWDTS_XACT_FLAG_DEPTH_OBJECT) {
      if (!(rtr_info->flags & RWDTS_FLAG_SUBOBJECT)) {
        continue;
      }
    }
    /*If the member is in a wait restart, the member must have crashed and its replacement is not yet
      up. This xact has not yet started the transaction, we do ahead and abort the xact. Ideally we
      should be retryin the xact here rwdts_router_xact_abort_and_retry*/
    if (rtr_info->member->wait_restart) {
      rwdts_router_member_node_t *membnode=NULL;
      HASH_FIND(hh_xact, xact->xact_union, rtr_info->member->msgpath,  strlen(rtr_info->member->msgpath),  membnode);
      if (membnode) {
        xact->abrt = true;
        RWDTS_ROUTER_LOG_XACT_EVENT(
            xact->dts, xact, DtsrouterXactDebug,
            RWLOG_ATTR_SPRINTF("WAIT_RESTART set for %s: ABRTing xact", rtr_info->member->msgpath));
        return;
      }
    }
    /*
      Even though there are multiple chunks with the same member information in the array of chunks, the HASH FIND for
      the sing requests will take care of not sending more than one prepare per member.This is done when creating the
      sing request below.
    */
    sing_req = rwdts_router_create_sing_req(xreq, rtr_info->member, NULL);
    
    RW_ASSERT(sing_req);
    if (sing_req){
      if (rtr_info->flags & RWDTS_XACT_FLAG_ANYCAST) {
        xreq->anycast_cnt++;
        //the any-cast is in the sing_req and not the membnode since it is used only in the prepare phase and not in the precommit/commit phase
        sing_req->any_cast = true;
      }
      RW_ASSERT(sing_req->membnode);
      //Update the membnode flags from the rtr_info flags
      sing_req->membnode->ignore_bounce = (rtr_info->flags & RWDTS_FLAG_REGISTRATION_BOUNCE_OK)?1:0;
    }
  }
  return;
}

static void
rwdts_router_update_client_for_precommit(rwdts_router_xact_t* xact, uint32_t blk) 
{
  rwdts_router_t *dts = xact->dts;
  rwdts_router_member_node_t *membnode=NULL;
  rwdts_router_xact_block_t *block = xact->xact_blocks[blk];

  rwdts_router_xact_rwmsg_ent_t *rwmsg_ent = xact->rwmsg_tab;
  while (rwmsg_ent
         && memcmp(&rwmsg_ent->rwmsg_cliid, &block->rwmsg_cliid,  sizeof(rwmsg_ent->rwmsg_cliid))) {
    rwmsg_ent++;
  }
  RW_ASSERT(rwmsg_ent);
  HASH_FIND(hh_xact, xact->xact_union, rwmsg_ent->client_path, strlen(rwmsg_ent->client_path), membnode);
  if (!membnode) {
    rwdts_router_member_t *memb = NULL;
    HASH_FIND(hh, dts->members, xact->rwmsg_tab[blk].client_path, strlen(xact->rwmsg_tab[blk].client_path), memb);
    if (memb) {
      membnode = RW_MALLOC0(sizeof(rwdts_router_member_node_t));
      memb->dest = rwmsg_destination_create(xact->dts->ep, RWMSG_ADDRTYPE_UNICAST, xact->rwmsg_tab[0].client_path);
      membnode->member = memb;
      HASH_ADD_KEYPTR(hh_xact, xact->xact_union, rwmsg_ent->client_path, strlen(rwmsg_ent->client_path), membnode);
      RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("[%s] is being added to xact_union",
                                                     xact->rwmsg_tab[blk].client_path));
    }
  }
  return;
}

static bool
rwdts_do_commits(rwdts_router_xact_t* xact) 
{
  int blks;
  if ((xact->n_xact_blocks > 1) || xact->do_commits) {
    return true;
  }
  for (blks=0; blks < xact->n_xact_blocks; blks++) {
     int qrs;
     RWDtsXactBlock *xbreq = xact->xact_blocks[blks]->xbreq;
     if (xbreq && xbreq->n_query) {
       for (qrs=0; qrs < xbreq->n_query; qrs++) {
         if ((xbreq->query[qrs]->action != RWDTS_QUERY_READ) &&
            !(xbreq->query[qrs]->flags & RWDTS_XACT_FLAG_NOTRAN)) {
           return true;
         }
       }
     }
  }
  return false;
}

void 
rwdts_router_xact_move_to_precomm(rwdts_router_xact_t* xact)
{
  rwdts_router_xact_newstate(xact, RWDTS_ROUTER_XACT_PRECOMMIT);
  return;
}

static bool
rwdts_precommit_to_client(rwdts_router_xact_t* xact, uint32_t* blockid)
{
  int blks;
  *blockid = 0;
  for (blks=0; blks < xact->n_xact_blocks; blks++) {
     int qrs;
     RWDtsXactBlock *xbreq = xact->xact_blocks[blks]->xbreq;
     if (xbreq && xbreq->n_query) {
       for (qrs=0; qrs < xbreq->n_query; qrs++) {
         if ((xbreq->query[qrs]->action != RWDTS_QUERY_READ) &&
            (xbreq->query[qrs]->flags & RWDTS_XACT_FLAG_TRANS_CRUD)) {
           *blockid = blks;
           // Only queries with transactional member data operation considered here
           return true;
         }
       }
     }
  }
  return false;
}

void rwdts_router_process_queued_xact_f(void *ctx)
{
  rwdts_router_queue_xact_t *queue_xact = (rwdts_router_queue_xact_t *)ctx;
  RW_ASSERT_TYPE(queue_xact, rwdts_router_queue_xact_t);
  //rwdts_router_t *dts = queue_xact->dts;
  rwdts_router_member_t *member = queue_xact->member;
  
  RW_FREE_TYPE(queue_xact, rwdts_router_queue_xact_t);
  
  while (RW_DL_LENGTH(&member->queued_xacts)) {
    rwdts_router_queued_xact_t *queued_xact = RW_DL_POP(&member->queued_xacts, rwdts_router_queued_xact_t, queued_xact_element);
    RW_ASSERT_TYPE(queued_xact, rwdts_router_queued_xact_t);
    RW_ASSERT(queued_xact->member == member);
    rwdts_router_xact_t *xact = queued_xact->xact;
    RW_ASSERT_TYPE(xact, rwdts_router_xact_t);
    RW_DL_REMOVE(&xact->queued_members, queued_xact, queued_member_element);
    RW_FREE_TYPE(queued_xact, rwdts_router_queued_xact_t);
    RW_ASSERT(xact->state == RWDTS_ROUTER_XACT_QUEUED);
    /*only the xacts that are in the prepare phase are the ones that are put into
      the queue when there is a bounce as a result of server reset or when a prepare
      was getting sent for the first time and we detect that the member is in
      wait-restart*/
    xact->state = RWDTS_ROUTER_XACT_PREPARE;
    int reqidx;
    for (reqidx=0; reqidx < xact->n_req; reqidx++) {
      rwdts_router_xact_req_t *xreq = &xact->req[reqidx];
      rwdts_router_xact_sing_req_t *sing_req = NULL, *tmp_sing_req = NULL;
      HASH_ITER(hh_memb, xreq->sing_req, sing_req, tmp_sing_req) {
        if (!strcmp(sing_req->membnode->member->msgpath, member->msgpath)
            && (sing_req->xreq_state == RWDTS_ROUTER_QUERY_QUEUED))  {
          sing_req->xreq_state = RWDTS_ROUTER_QUERY_UNSENT;
          if (xact->dbg) {
            /* Make up an entry in our response for this req */
            RWDtsTracerouteEnt ent;
            
            rwdts_traceroute_ent__init(&ent);
            ent.func = (char*) __PRETTY_FUNCTION__;
            ent.line = __LINE__;
            ent.event = RWDTS_EVT_PREPARE;
            ent.what = RWDTS_TRACEROUTE_WHAT_REQ;
            ent.dstpath = sing_req->membnode->member->msgpath;
            ent.srcpath = (char*)xact->dts->rwmsgpath;
            ent.n_queryid = 0;
            if (xact->dbg && xact->dbg->tr) {
              if (xact->dbg->tr->print_) {
                fprintf(stderr, "%s:%u: TYPE:%s, EVT:%s, DST:%s, SRC:%s\n", ent.func, ent.line,
                        rwdts_print_ent_type(ent.what), rwdts_print_ent_event(ent.event),
                        ent.dstpath, ent.srcpath);
              }
              sing_req->tracert_idx = rwdts_dbg_tracert_add_ent(xact->dbg->tr, &ent);
            }
            RWDTS_TRACE_EVENT_REQ(xact->dts->rwtaskletinfo->rwlog_instance, TraceReq,
                                  xact->xact_id_str,ent);
          }
          rwdts_router_prepare_f(xact, sing_req);
        }
      }
    }
  }
}

void 
rwdts_router_stop_member_sing_reqs(rwdts_router_member_t *memb)
{
  rwdts_router_xact_sing_req_t *sing_req = NULL, *next_sing_req;
  rwdts_router_member_node_t *sr_membnode;
  rwdts_router_member_node_t *membnode = NULL;
  rwdts_router_xact_req_t *xreq;
  rwdts_router_xact_t *xact;
  rwdts_router_xact_block_t *block = NULL;

  sing_req =  RW_DL_HEAD(&memb->sing_reqs, rwdts_router_xact_sing_req_t, memb_element);
  
  while (sing_req){
    next_sing_req = RW_DL_NEXT(sing_req,
                               rwdts_router_xact_sing_req_t, memb_element);
    sr_membnode = sing_req->membnode;
    membnode = NULL;
    xact = sing_req->xact;
    xreq = &xact->req[sing_req->xreq_idx];
    RW_ASSERT(xreq->block_idx >= 0);
    RW_ASSERT(xreq->block_idx < xact->n_xact_blocks);
    block = xact->xact_blocks[xreq->block_idx];
    RW_ASSERT(block);
    
    switch (sing_req->xreq_state) {
      case RWDTS_ROUTER_QUERY_SENT: 
      case RWDTS_ROUTER_QUERY_ASYNC: 
      case RWDTS_ROUTER_QUERY_QUEUED:
        {
          block->req_done++;
          block->req_done_na++;
          xact->req_done++;
          block->req_outstanding--;
          xact->req_outstanding--;
        }
        //Fall through
      case RWDTS_ROUTER_QUERY_UNSENT: 
      case RWDTS_ROUTER_QUERY_NA: 
      case RWDTS_ROUTER_QUERY_NACK: 
      case RWDTS_ROUTER_QUERY_INTERNAL:
      case RWDTS_ROUTER_QUERY_ERR:
      case RWDTS_ROUTER_QUERY_ACK:
        {
          HASH_FIND(hh_xact, xact->xact_union, sr_membnode->member->msgpath, strlen(sr_membnode->member->msgpath), membnode);
          if (membnode) {
            RW_ASSERT(sr_membnode == membnode);
            HASH_DELETE(hh_xact, xact->xact_union, membnode);
            //the delete of sing_request will free the membnode
            sing_req->memb_xfered = 0;
          }
          if (xact->dbg && sing_req->tracert_idx >= 0) {
            struct timeval tvzero, now, delta;
            RW_ASSERT(sing_req->tracert_idx < xact->dbg->tr->n_ent);
            gettimeofday(&now, NULL);
            tvzero.tv_sec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_sec;
            tvzero.tv_usec = xact->dbg->tr->ent[sing_req->tracert_idx]->tv_usec;
            timersub(&now, &tvzero, &delta);
            xact->dbg->tr->ent[sing_req->tracert_idx]->elapsed_us = (uint64_t)delta.tv_usec + (uint64_t)delta.tv_sec * (uint64_t)1000000;
            xact->dbg->tr->ent[sing_req->tracert_idx]->res_code = RWDTS_EVTRSP_CANCELLED;
          }
          rwdts_router_delete_sing_req(xact, sing_req, false);
        }
        break;
      default:
        break;
    }
    rwdts_router_xact_run(xact);
    sing_req = next_sing_req;
  }
}
