
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
 * @file rwmsg_query.c
 * @author RIFT.io <info@riftio.com>
 * @date 2014/09/15
 * @brief DTS  Query  and member API definitions
 */

#include <stdlib.h>
#include <rwtypes.h>
#include <rwdts.h>
#include <rwvcs.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwlib.h>
#include <rwdts_api_gi.h>
#include <rwdts_int.h>
#include <rw_rand.h>
#include <rwdts_xpath.h>

#define RWDTS_READ_MAX_CREDIT 100
#define RWDTS_OTHER_MAX_CREDIT 1

/***********************************************************
 * static function prototypes                              *
 ***********************************************************/
static inline RWDtsQuery* rwdts_xact_init_empty_query(rwdts_member_registration_t *reg);
static void rwdts_xact_query_cb(RWDtsXactResult* rsp,
                                rwmsg_request_t*  rwreq,
                                void*             ud);

static void rwdts_xact_end_rsp(RWDtsStatus*      status,
                               rwmsg_request_t*  rwreq,
                               void*             ud);

static void rwdts_xact_abort_rsp(RWDtsStatus*     status,
                                 rwmsg_request_t*  rwreq,
                                 void*             ud);

static rw_status_t
rwdts_xact_send(rwdts_xact_t *xact);


/*******************************************************************************
 *                      Static functions                                       *
 *******************************************************************************/

static void
rwdts_fill_commit_serials(RWDtsQuery*                  query,
                          rwdts_member_registration_t* reg)
{
  rwdts_pub_serial_t* sl;
  int i = 0;

  RW_ASSERT(query->n_commit_serials == 0);
  RW_ASSERT(query->commit_serials == NULL);

  query->n_commit_serials= RW_SKLIST_LENGTH(&reg->committed_serials);
  query->commit_serials = (uint64_t*)RW_MALLOC0(sizeof(uint64_t) * query->n_commit_serials);

  RW_SKLIST_FOREACH(sl, rwdts_pub_serial_t, &reg->committed_serials, element) {
    query->commit_serials[i++] = sl->serial;
  }
  RW_ASSERT(i == query->n_commit_serials);
  return;
}

/* Allocate and initialize an empty query structure.
 * internal function.
 * The caller owns the allocated query
 */

static RWDtsQuery*
rwdts_xact_init_empty_query(rwdts_member_registration_t *reg)
{
  RWDtsQuery *query = (RWDtsQuery*)RW_MALLOC0(sizeof(RWDtsQuery));
  RW_ASSERT(query);
  rwdts_query__init(query);
  if (reg) {
    query->serial = (RwDtsQuerySerial*)protobuf_c_message_duplicate(NULL,
                                                                    &reg->serial.base,
                                                                    reg->serial.base.descriptor);
    rwdts_fill_commit_serials(query, reg);
  }
  return query;
}

RWDtsQuery*
rwdts_xact_test_init_empty_query(rwdts_member_registration_t *reg)
{
  RWDtsQuery* query = rwdts_xact_init_empty_query(reg);
  return (query);
}

static void
rwdts_xact_replenish_credits(rwdts_xact_t *xact)
{
  RW_ASSERT(xact);
  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  int i, j;
  RWDtsXactBlock *block = NULL;
  RWDtsQuery *query = NULL;

  for (i = 0; i < xact->blocks_ct; i++) {
    block = &xact->blocks[i]->subx;
    for (j = 0; j < block->n_query; j++) {
      query = block->query[j];
      RWDTS_FREE_CREDITS(apih->credits, query->credits);
    }
  }
  return;
}

void rwdts_client_dump_xact_info(rwdts_xact_t *xact ,
                                 const char *reason)
{
  int i;
  char tmp_log_xact_id_str[128] = "";
  char *ignore = NULL;
  DTS_PRINT ("***********Xact Dump ***********\n Reason: %s\n", reason);
  DTS_PRINT ("Client Name : %s \n", (xact->apih)?xact->apih->client_path:"NA");
  DTS_PRINT ("xact[%s] : state %s \n", (char*)rwdts_xact_id_str(&(xact)->id, tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)), "NA");
  DTS_PRINT("\nCounts \n");
  PRINT_XACT_INFO_ELEM(blocks_ct, ignore);
  PRINT_XACT_INFO_ELEM(rsp_ct, ignore);
  PRINT_XACT_INFO_ELEM(track.num_query_results, ignore);
  PRINT_XACT_INFO_ELEM(req_out, ignore);
  PRINT_XACT_INFO_ELEM(ref_cnt, ignore);
  DTS_PRINT("\nStats: \n");
  PRINT_XACT_INFO_ELEM(num_prepares_dispatched, ignore);
  PRINT_XACT_INFO_ELEM(num_responses_received, ignore);
  PRINT_XACT_INFO_ELEM(num_prepares_dispatched, ignore);
  DTS_PRINT("\nStatus: \n");
  PRINT_XACT_INFO_ELEM(evtrsp, ignore);
  PRINT_XACT_INFO_ELEM(status, ignore);
  DTS_PRINT("\nFlags :%x\n", xact->flags);
  PRINT_XACT_INFO_ELEM(bounced, ignore);
  PRINT_XACT_INFO_ELEM(trace, ignore);

  if (xact->err_report) {
    DTS_PRINT ("\nError Report\n");
    int i;
    for (i = 0 ;i < xact->err_report->n_ent; i++)
    {
      RWDtsErrorEntry *ent = xact->err_report->ent[i];
      DTS_PRINT("%s: Client:%s, Keystr %s, corrid %lu, cause %d, error %s\n",
                xact->apih->client_path,
                ent->client_path,
                ent->keystr_buf ? ent->keystr_buf : "~NA~",
                ent->corrid,
                ent->cause,
                ent->errstr);
    }
  }

  DTS_PRINT ("\nFSM State\n");

  if (xact->curr_trace_index < MAX_TRACE_EVT)
  {
    for (i = 0; i < xact->curr_trace_index; i++)
    {
      time_t evtime = xact->fsm_log[i].time.tv_sec;
      struct tm* tm = gmtime(&evtime);
      char tmstr[128];
      int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
      sprintf(tmstr+tmstroff, ".%06uZ", (int)xact->fsm_log[i].time.tv_usec/1000);
      DTS_PRINT("%d:State %s By Event %s at Time %s\n",
                i+1,
                rwdts_member_state_to_str(xact->fsm_log[i].state),
                rwdts_member_event_to_str(xact->fsm_log[i].evt),
                tmstr);
    }
  }
  else
  {
    DTS_PRINT(" TOO Many states %d\n", xact->curr_trace_index);
  }
}

rw_status_t
rwdts_report_error(rwdts_api_t *apih,
                   RWDtsErrorReport * error_report)
{
  int i;
  for (i = 0 ;i < error_report->n_ent; i++)
  {
    RWDtsErrorEntry *ent = error_report->ent[i];
    RWTRACE_ERROR(apih->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET,
                  "%s: Client:%s, Keystr %s, corrid %lu, cause %d, error %s",
                  apih->client_path,
                  ent->client_path,
                  ent->keystr_buf ? ent->keystr_buf : "~NA~",
                  ent->corrid,
                  ent->cause,
                  ent->errstr);
  }

  return RW_STATUS_SUCCESS;
}

static RWDtsQueryAction rwdts_get_query_action (rwdts_xact_t *xact,
                                                uint32_t bid,
                                                uint32_t qid)
{
  int i , j;
  for (i=0;i<xact->blocks_ct;i++)
  {
    if (xact->blocks[i]->subx.block->blockidx != bid)
    {
      continue;
    }
    for (j=0;j<xact->blocks[i]->subx.n_query;j++)
    {
      if (xact->blocks[i]->subx.query[j]->queryidx == qid)
      {
        return xact->blocks[i]->subx.query[j]->action;
      }
    }
  }
  return RWDTS_QUERY_INVALID;
}
/*  Callback for the entire transaction
 *  The callback invokes the API's event callback routine
 *  internal  function
 */
static void rwdts_xact_query_cb(RWDtsXactResult* rsp_in,
                                rwmsg_request_t*  rwreq,
                                void*             ud)
{
  rwdts_xact_t *xact = (rwdts_xact_t*)ud;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RWDtsXactResult *rsp = NULL;
  bool *block_responses_done = RW_MALLOC0(sizeof(bool) * xact->blocks_ct);
  RW_ASSERT (block_responses_done);

  RW_ASSERT(!rsp_in || rsp_in->has_status);


  rwdts_api_t *apih = xact->apih;
  if (!rsp_in) {
    xact->apih->stats.client_query_bounced++;
    if (!(xact->apih->stats.client_query_bounced % 10)) {
      RWDTS_API_LOG_XACT_EVENT(xact->apih,
                               xact, RwDtsApiLog_notif_QueryBounced,
                               "rwdts_xact_query_cb, no rsp_in, bounce", 0);
    }
    RWMEMLOG(&xact->rwml_buffer,
	     RWMEMLOG_MEM0,
	     "Got execute msg BNC");
  } else {
    RWDTS_API_LOG_XACT_EVENT(xact->apih,
                             xact, RwDtsApiLog_notif_XactRspRcvd,
                             RWLOG_ATTR_SPRINTF("%s rwdts_xact_query_cb rsp_in status=%d has_more=%d more=%d n_result=%d", apih->client_path, rsp_in->status, rsp_in->has_more, rsp_in->more, (int)rsp_in->n_result));

    rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                              rwreq, 
                              __PRETTY_FUNCTION__,
                              __LINE__,
                              "(Execute Rsp)");
  }

  int res_ct = 0;
  gboolean more_rcvd = FALSE;

  if (rsp_in)
  {
    int i, j, k;
    if (rsp_in->has_more && rsp_in->more) {
      xact->apih->stats.more_received++;
      more_rcvd = TRUE;
    }

    rsp = (RWDtsXactResult*)protobuf_c_message_duplicate(NULL,
                                                         &rsp_in->base,
                                                         rsp_in->base.descriptor);
    RW_ASSERT(rsp);
  if (rsp && rsp->dbg)
  {
    if ((rsp->dbg->dbgopts1&RWDTS_DEBUG_TRACEROUTE) && rsp->dbg->tr)
    {

      /* Record elapsed time */
      assert(rsp->dbg->tr->ent[0]->what == RWDTS_TRACEROUTE_WHAT_REQ
             && rsp->dbg->tr->ent[0]->event == RWDTS_EVT_QUERY);
      rsp->dbg->tr->ent[0]->state = RWDTS_TRACEROUTE_STATE_RESPONSE;
      struct timeval tvzero, now, delta;
      gettimeofday(&now, NULL);
      tvzero.tv_sec = rsp->dbg->tr->ent[0]->tv_sec;
      tvzero.tv_usec = rsp->dbg->tr->ent[0]->tv_usec;
      timersub(&now, &tvzero, &delta);
      rsp->dbg->tr->ent[0]->elapsed_us = (uint64_t)delta.tv_usec + (uint64_t)delta.tv_sec * (uint64_t)1000000;
      rsp->dbg->tr->ent[0]->res_code = rsp->evtrsp;
      rsp->dbg->tr->ent[0]->res_count = xact->track.num_query_results;

      if (xact->parent) {
        RW_CRASH();
        //TBD: rwdts_dbg_tracert_append(xact->parent..., rsp->dbg->tr);
      }
      if (1)
      {
        rwdts_dbg_tracert_dump(rsp->dbg->tr, xact);
      }
    }
    if (rsp->dbg->err_report)
    {
      rwdts_report_error(xact->apih, rsp->dbg->err_report);
    }
    if (rsp->dbg && rsp->dbg->tr && rsp->dbg->tr->break_end)
    {
      G_BREAKPOINT();
    }

  }

    for (i = 0; i < rsp->n_result; i++) {
      RWDtsXactBlockResult *b_result = rsp->result[i];
      RW_ASSERT(b_result);

      /* So is this block now done? */
      {
        int blk_count;
        for (blk_count = 0; blk_count < xact->blocks_ct; blk_count++) {
          if (xact->blocks[blk_count]
              && xact->blocks[blk_count]->subx.block->blockidx == b_result->blockidx) {
            if (b_result->has_evtrsp &&
                b_result->evtrsp == RWDTS_EVTRSP_INTERNAL) {
              xact->blocks[blk_count]->evtrsp_internal = true;
            }
            if (xact->blocks[blk_count]->responses_done) {
              RW_ASSERT (!(b_result->n_result));
            }
            else if (b_result->has_more && !b_result->more) {
              xact->blocks[blk_count]->responses_done = TRUE;
              block_responses_done[blk_count] = TRUE;
            }
          }
        }
      }

      /* Shove these results into the results list */
      res_ct += b_result->n_result;
      for (j = 0; j < b_result->n_result; j++) {
        RWDtsQueryResult *q_result = b_result->result[j];
        RW_ASSERT(q_result);
        rwdts_xact_result_t *res = RW_MALLOC0_TYPE(sizeof(rwdts_xact_result_t), rwdts_xact_result_t);
        res->result = q_result;
        res->blockidx = b_result->blockidx;

        for (k = 0; k < q_result->n_result; k++)
        {
          RWDtsQuerySingleResult *single_result = q_result->result[k];
          rwdts_log_payload(&apih->payload_stats,
                            rwdts_get_query_action(xact, b_result->blockidx,q_result->queryidx),
                            RW_DTS_MEMBER_ROLE_CLIENT,
                            RW_DTS_XACT_LEVEL_SINGLERESULT,
                            single_result->result->paybuf.len);
        }
        RW_DL_ENQUEUE(&(xact->track.pending), res, elem);
      }
    }
    if (rsp) {
      // Insert the result to the transaction so that it can be cleaned up when the transaction is cleaned up
      rwdts_xact_result_track_t* track_result = RW_MALLOC0_TYPE(sizeof(rwdts_xact_result_track_t), rwdts_xact_result_track_t);
      track_result->result = rsp;
      RW_DL_PUSH(&(xact->track.results), track_result, res_elem);
    }
    xact->xres = rsp;
  }
  else
  {
    if (!(xact->flags & RWDTS_XACT_FLAG_IMM_BOUNCE)) {

      char xact_str[64] = "";
      RWTRACE_INFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "[%s]: received xact query msg bounce code=%d client=%s",
                   (char*)rwdts_xact_id_str(&xact->id, xact_str, sizeof(xact_str)),
                   (int)rwmsg_request_get_response_bounce(rwreq),
                   xact->apih->client_path);

      RWDTS_API_LOG_XACT_EVENT(xact->apih,
                               xact,
                               RwDtsApiLog_notif_QueryBounced,
                               "Query request bounced",
                               (int)rwmsg_request_get_response_bounce(rwreq));

      /* How do we even abort this xact?  Guess we need a retry?  Or
         something, there's an impedence mismatch here between rwmsg
         multibroker bounce semantics and dts's unlimited client
         usage.  The DTS API probably must be flow control / feedme
         aware. */
    }

    xact->bounced = 1;
/* FIXME This flag is just broken */
  }

  // Do not invoke application callbacks when the router state is still running.
  // Well how the heck is that suppose to work???
  rwdts_xact_status_t *xact_status = RW_MALLOC0(sizeof(rwdts_xact_status_t));
  xact_status->xact_done = false;

  bool xact_responses_done = FALSE;
  if (xact->bounced) {
    // FIXME stat or something
    RWDTS_API_LOG_XACT_EVENT(xact->apih,
                             xact, RwDtsApiLog_notif_QueryBounced,
                             "rwdts_xact_query_cb GOT A BOUNCE", 0);
    xact_status->status = RWDTS_XACT_FAILURE;
    xact_status->xact_done = TRUE;
    xact_responses_done = TRUE;
  }
  else if (xact->xres) {
    xact_status->status = xact->xres->status;
    xact_responses_done = (xact->xres->has_more && !xact->xres->more);
  }

  if (rsp_in) {
    switch (rsp_in->status) {
      case RWDTS_XACT_COMMITTED :
      case RWDTS_XACT_ABORTED :
      case RWDTS_XACT_FAILURE :
         if (rsp_in->has_more) {
           RW_ASSERT(!rsp_in->more);
         }
         xact_status->xact_done = TRUE;
         break;
      default:
         break;
    }
  }

  RWDTS_API_LOG_XACT_EVENT(xact->apih,
                           xact, RwDtsApiLog_notif_XactRspRcvd,
                           RWLOG_ATTR_SPRINTF("Client (%s) rwdts_xact_querycb xact_status status=%d xact_done=%d xact_responses_done=%d res_ct=%d", 
                                              apih->client_path, xact_status->status, xact_status->xact_done, xact_responses_done, res_ct));

  /* This is nuts.  We should just filter out the one uninteresting
     case of a probe request that came back empty with more set */
  {
    int i,j;
    for (i = 0; i < xact->blocks_ct; i++) {
      if (xact->blocks[i] && xact->blocks[i]->cb.cb) {
        xact_status->block = xact->blocks[i];
        xact_status->responses_done = xact->blocks[i]->responses_done;
        bool block_in_result = false;
        for (j=0; rsp_in && j < rsp_in->n_result; j++) {
          if (xact->blocks[i]->subx.block->blockidx == rsp_in->result[j]->blockidx) {
            block_in_result = true;
            break;
          }
        }
        if (xact_status->xact_done || block_in_result || block_responses_done[i]) {
          RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_ClientCallbackTriggered,
                                   "Block client callback triggered",
                                   rwdts_get_app_addr_res(xact->apih, xact->blocks[i]->cb.cb),
                                   rsp_in ?rsp_in->n_result:0);
          rwdts_xact_block_get_more_results(xact->blocks[i]);


          RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);

          xact->blocks[i]->cb.cb(xact, xact_status, (void*)xact->blocks[i]->cb.ud);

          RWVCS_LATENCY_CHK_POST (apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                                  xact->blocks[i]->cb.cb, "blocks.cb at"); // %s", apih->client_path);
          if (xact_status->xact_done) {
            xact->blocks[i]->cb.cb = NULL;
          }
        }
        else {
          RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_ClientCallbackSkipped,
                                   RWLOG_ATTR_SPRINTF("rwdts_query_cb: skipping %d block callback, xact_done=%d block_in_result=%d block_responses_done=%d",
                                                       i, xact_status->xact_done, block_in_result, block_responses_done[i]));
        }
      }
    }
    RW_FREE (block_responses_done);
  }


  if (!rsp_in || res_ct || xact_status->xact_done || xact_responses_done) {
    if (xact->cb && xact->cb->cb) {
      /* Block and responses_done adjusted for the xact-wide state */
      xact_status->block = NULL;
      xact_status->responses_done = xact_responses_done;

      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_ClientCallbackTriggered,
                               "Xact Client callback triggered",
                               rwdts_get_app_addr_res(xact->apih, xact->cb->cb),
                               (rsp_in)?rsp_in->n_result:0);


      RWVCS_LATENCY_CHK_PRE(apih->tasklet->instance);

      (xact->cb->cb)(xact, xact_status, (void*)xact->cb->ud);

      RWVCS_LATENCY_CHK_POST (apih->rwtrace_instance, RWTRACE_CATEGORY_RWTASKLET,
                              xact->cb->cb, "xact cb at %s", apih->client_path);
      if (xact_status->xact_done) {
        xact->cb->cb = NULL;
      }
    }
  } else {
    RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_ClientCallbackSkipped,
                             RWLOG_ATTR_SPRINTF("rwdts_query_cb: skipping xact callback, rsp_in=%p res_ct=%d xact_responses_done=%d xact_done=%d",
                                                rsp_in, res_ct, xact_responses_done, xact_status->xact_done));
  }

  rwdts_xact_replenish_credits(xact);


  /* We want to keep one request outstanding */
  RW_ASSERT(xact->req_out > 0);
  xact->req_out--;
  if ((xact->req_out < 1)
      && (more_rcvd || !xact_status->xact_done)) {
    rw_status_t rs = rwdts_xact_send(xact);
    /* FIXME support flowid etc from rwmsg interface; for now we
       assume immediate failures only on bootstrap-time sends. */
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  if (xact_status->xact_done) {
//    if (xact_status->status == RWDTS_XACT_FAILURE) return;
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);

  RW_FREE(xact_status);                /* BUG put this on stack, let GObject copy it to heap.  Or is it reference counted?  Yikes */

  return;
}

/*
 * Router responded to an end request
 */

static void rwdts_xact_end_rsp(RWDtsStatus*      status,
                               rwmsg_request_t*  rwreq,
                               void*             ud)
{
  rwdts_xact_unref((rwdts_xact_t*)ud, __PRETTY_FUNCTION__, __LINE__);
  return;
}

static void rwdts_xact_abort_rsp(RWDtsStatus*      status,
                                 rwmsg_request_t*  rwreq,
                                 void*             ud)
{
  // char xact_str[64] = "";
  rwdts_xact_t *xact = (rwdts_xact_t*)ud;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);

  rwdts_api_t *apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return;
}

/*  Provide credits for the query
 *
 */
static uint32_t
rwdts_xact_query_get_credits(rwdts_api_t*  apih, uint32_t max_credit)
{
  return RWDTS_GET_CREDITS(apih->credits, max_credit);
}

uint64_t rwdts_get_serialnum (rwdts_api_t*                 apih,
                              rwdts_member_registration_t* regn)
{
  if(regn) {
    return 0;
  } else {
    return ++apih->tx_serialnum;
  }
}

/*  Initialize a query structure from the passed various query argumements
 *  internal  function
 */
RWDtsQuery*
rwdts_xact_init_query(rwdts_api_t*                  apih,
                      RWDtsKey*                     key,
                      RWDtsQueryAction              action,
                      RWDtsPayloadType              payload_type,
                      uint8_t*                      payload,
                      uint32_t                      payload_len,
                      uint64_t                      corrid,
                      const RWDtsDebug*             dbg,
                      uint32_t                      query_flags,
                      rwdts_member_registration_t*  reg)
{

  static uint32_t queryidx  = 1 ; // ATTN fix this
  uint32_t max_credit = 0;
  RWDtsQuery *query = rwdts_xact_init_empty_query(reg);
  query->key = key;
  query->action = action;
  asprintf(&query->source, "%s", apih->client_path);

  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  if (action == RWDTS_QUERY_CREATE ||
      action == RWDTS_QUERY_UPDATE) {
    RW_ASSERT(key);
    RW_ASSERT(payload);
  }

  if (action == RWDTS_QUERY_READ) {
    max_credit = RWDTS_READ_MAX_CREDIT;
  } else {
    max_credit = RWDTS_OTHER_MAX_CREDIT;
  }

  if (payload) {
    query->payload = (RWDtsPayload*)RW_MALLOC0(sizeof(RWDtsPayload));
    RW_ASSERT(query->payload);
    rwdts_payload__init(query->payload);
    query->payload->ptype = payload_type;
    // Copy the payload
    query->payload->has_paybuf = TRUE;
    query->payload->paybuf.len  = payload_len;
    query->payload->paybuf.data  = payload;
  }
  rwdts_log_payload(&apih->payload_stats,
                    query->action,
                    RW_DTS_MEMBER_ROLE_CLIENT,
                    RW_DTS_XACT_LEVEL_QUERY,
                    payload_len);

  query->corrid  = corrid;
  query->has_corrid  = !!corrid;

  if (apih) {
    uint32_t credits = rwdts_xact_query_get_credits(apih, max_credit);
    query->credits = credits;
    query->has_credits = !!credits;
  }

  if (dbg) {
    query->dbg = (RWDtsDebug*)protobuf_c_message_duplicate(NULL, &dbg->base, dbg->base.descriptor);
  }

  query->flags = query_flags;

  if (query->flags & RWDTS_XACT_FLAG_DEPTH_FULL) {
    query->flags &= ~(RWDTS_XACT_FLAG_DEPTH_LISTS|RWDTS_XACT_FLAG_DEPTH_ONE);
  } else if (query->flags & RWDTS_XACT_FLAG_DEPTH_LISTS) {
    query->flags &= ~RWDTS_XACT_FLAG_DEPTH_ONE;
  }

  if (!(query->flags & (RWDTS_XACT_FLAG_DEPTH_LISTS|RWDTS_XACT_FLAG_DEPTH_ONE))) {
    query->flags |= RWDTS_XACT_FLAG_DEPTH_FULL;
  }

  query->has_queryidx = 1;
  query->queryidx = queryidx++;

  return query;
}

/*
 * Create key from keyspec
 */

RWDtsKey*
rwdts_init_key_from_keyspec_int(const rw_keyspec_path_t* keyspec)
{
  RWDtsKey*   key = NULL;
  const uint8_t *key_data = NULL;

  key = (RWDtsKey*)RW_MALLOC0(sizeof(RWDtsKey));
  RW_ASSERT(key);
  rwdts_key__init(key);

  key->ktype = RWDTS_KEY_RWKEYSPEC;
  key->has_keybuf = TRUE;

  rw_keyspec_path_create_dup(keyspec, NULL , (rw_keyspec_path_t**)&key->keyspec);
  //  key->keyspec = (RwSchemaPathSpec*) keyspec;//RW_KEYSPEC_PATH_CREATE_DUP_of_type(keyspec, &rw_schema_path_spec__descriptor);
  RW_ASSERT(key->keyspec);

  rw_status_t rs = rw_keyspec_path_get_binpath((rw_keyspec_path_t*)key->keyspec, NULL, &key_data, &key->keybuf.len);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(key_data != NULL);
  RW_ASSERT(key->keybuf.len > 0);

  key->keybuf.data = RW_MALLOC0(key->keybuf.len);
  memcpy(key->keybuf.data, key_data, key->keybuf.len);

  return key;
}

/*
 * Init query from a proto
 */
RWDtsQuery*
rwdts_xact_init_query_from_pb(rwdts_api_t*                 apih,
                              const rw_keyspec_path_t*     keyspec,
                              const ProtobufCMessage*      msg,
                              RWDtsQueryAction             action,
                              uint64_t                     corrid,
                              const RWDtsDebug*            dbg,
                              uint32_t                     query_flags,
                              rwdts_member_registration_t* reg)
{
  size_t      payload_len = 0;
  uint8_t*    payload = NULL;
  RWDtsQuery* query = NULL;
  RWDtsKey*   key = NULL;

  RW_ASSERT(keyspec);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  key = rwdts_init_key_from_keyspec_int(keyspec);

  if (action == RWDTS_QUERY_CREATE ||
      action == RWDTS_QUERY_UPDATE) {
    RW_ASSERT(keyspec);
    RW_ASSERT(msg);
    RW_ASSERT(msg->descriptor);
    //    RW_ASSERT(!rw_keyspec_path_has_wildcards(keyspec));
  }

  if (msg) {
    payload = protobuf_c_message_serialize(NULL, msg, &payload_len);
  }

  query = rwdts_xact_init_query(apih,
                                key,
                                action,
                                RWDTS_PAYLOAD_PBRAW,
                                payload,
                                payload_len,
                                corrid,
                                dbg,
                                query_flags,
                                reg);
  return query;
}

/*
 * End block entry / prepare phase of a transaction.
 *
 * Only legal in xact originator, ought to check for that, how?
 */
rw_status_t rwdts_xact_commit(rwdts_xact_t *xact)
{

  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t *apih = xact->apih;

  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_request_t *req_out = NULL;

  RW_ASSERT(apih);

  RW_ASSERT(apih->client.ep);
  RW_ASSERT(apih->client.cc);
  RW_ASSERT(apih->client.dest);

  // Take  a reference
  rwdts_xact_ref(xact,__PRETTY_FUNCTION__, __LINE__);

  rwmsg_closure_t clo = {.pbrsp = (rwmsg_pbapi_cb)rwdts_xact_end_rsp, .ud = xact};
  rs = rwdts_query_router__end(&apih->client.service,
                               apih->client.dest,
                               &(xact->id),
                               &clo,
                               &req_out);

  if (rs != RW_STATUS_SUCCESS) {
    /* Well this is almost certainly wrong */
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "Failed to send end request to router - destroying xact [%p]", xact);
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }

  return RW_STATUS_SUCCESS;
}

/*
 * Abort a DTS transaction
 */

rw_status_t rwdts_xact_abort(rwdts_xact_t *xact,
                             rw_status_t status,
                             const char* errstr)
{

  RW_ASSERT(xact);
  rwdts_api_t *apih = xact->apih;

  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_request_t *req_out = NULL;

  RW_ASSERT(apih);

  RW_ASSERT(apih->client.ep);
  RW_ASSERT(apih->client.cc);
  RW_ASSERT(apih->client.dest);

  // update error report
  rwdts_member_send_err_on_abort (xact, status, errstr);

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);

  rwmsg_closure_t clo = {.pbrsp = (rwmsg_pbapi_cb)rwdts_xact_abort_rsp, .ud = xact};

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  rs = rwdts_query_router__abort(&apih->client.service,
                                 apih->client.dest,
                                 &(xact->id),
                                 &clo,
                                 &req_out);

  if (rs != RW_STATUS_SUCCESS) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    RWTRACE_CRIT(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "Failed to send abort request to router - destroying xact [%p]", xact);
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }

  return RW_STATUS_SUCCESS;
}

rwdts_xact_status_t *rwdts_xact_status_new() {
  rwdts_xact_status_t *xs = RW_MALLOC0_TYPE(sizeof(rwdts_xact_status_t), rwdts_xact_status_t);
  xs->ref_cnt = 1;
  return xs;
}
void rwdts_xact_status_deinit(rwdts_xact_status_t *s) {
  RW_FREE_TYPE(s, rwdts_xact_status_t);
}

rwdts_xact_status_t *rwdts_xact_block_get_status_gi(rwdts_xact_block_t *blk) {
  RW_ASSERT(blk);

  rwdts_xact_status_t *xs = rwdts_xact_status_new();
  rwdts_xact_block_get_status(blk, xs);
  return xs;
}

rwdts_xact_status_t *rwdts_xact_get_status_gi(rwdts_xact_t *xact) {
  RW_ASSERT(xact);

  rwdts_xact_status_t *xs = rwdts_xact_status_new();
  rwdts_xact_get_status(xact, xs);
  return xs;
}

RWDtsXactMainState rwdts_xact_status_get_status(rwdts_xact_status_t *status) {
  return status->status;
}

gboolean rwdts_xact_status_get_xact_done(rwdts_xact_status_t *status) {
  return status->xact_done;
}
gboolean rwdts_xact_status_get_responses_done(rwdts_xact_status_t *status) {
  return status->responses_done;
}
rwdts_xact_block_t*
rwdts_xact_status_get_block(rwdts_xact_status_t *status) {
  if (status->block) {
    rwdts_xact_block_ref(status->block, __PRETTY_FUNCTION__, __LINE__);
  }
  return status->block;
}

rw_status_t rwdts_xact_get_status(rwdts_xact_t*        xact,
                                  rwdts_xact_status_t* status_out)
{

  RW_ASSERT(xact);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);
  RW_ASSERT(status_out);

  if (xact->bounced) {
    status_out->status = RWDTS_XACT_FAILURE;
    status_out->responses_done = TRUE;
  }
  else if (xact->xres) {
    status_out->status = xact->xres->status;
    status_out->responses_done = !(xact->xres->more);
  }

  // ATTN -- When there is failure, status out should provide additional data to the calle abt failure reason, failed members etc ....
  return  RW_STATUS_SUCCESS;
}

rw_status_t rwdts_xact_block_get_status(rwdts_xact_block_t*  block,
                                        rwdts_xact_status_t* status_out)
{
  RW_ASSERT(block);
  rwdts_xact_t *xact = block->xact;
  RW_ASSERT(xact);
  rwdts_api_t *apih = xact->apih;
  int i;

  RW_ASSERT(apih);
  RW_ASSERT(status_out);

  if (xact->bounced) {
    status_out->status = RWDTS_XACT_FAILURE;
    status_out->responses_done = TRUE;
    return  RW_STATUS_SUCCESS;
  }

  status_out->status = RWDTS_XACT_RUNNING;
  status_out->responses_done = TRUE;
  if (xact->xres) {
    for (i = 0; i < xact->xres->n_result; i++) {
      if (xact->xres->result[i]->blockidx == block->subx.block->blockidx) {
        status_out->status = xact->xres->status;
        status_out->responses_done = xact->blocks[i]->responses_done;//?? !xact->xres->more;
        break;
      }
    }
  }

  // ATTN -- When there is failure, status out should provide additional data to the calle abt failure reason, failed members etc ....
  return  RW_STATUS_SUCCESS;
}

/*
 * Get results from the DTS transaction
 */

rw_status_t rwdts_xact_get_result(rwdts_xact_t*          xact,
                                  uint64_t               corrid,
                                  rwdts_xact_result_t**  const result)
{
  return rwdts_xact_get_result_pbraw(xact, NULL, corrid, result, NULL);
}

rw_status_t rwdts_xact_block_get_result(rwdts_xact_block_t     *blk,
                                        uint64_t               corrid,
                                        rwdts_xact_result_t**  const result)
{
  return rwdts_xact_get_result_pbraw(blk->xact, blk, corrid, result, NULL);
}

/*
 * Get results from the DTS transaction in XML format
 */
rw_status_t rwdts_xact_get_result_xml(rwdts_xact_t*         xact,
                                      uint64_t               corrid,
                                      rwdts_xact_result_t** const result)
{
  return RW_STATUS_FAILURE;
}


/*
 * Get results from the DTS transaction in PBRAW format.  The next
 * result matching the given blk or corrid.  Both blk and corrid are
 * optional.
 */
rw_status_t rwdts_xact_get_result_pbraw(rwdts_xact_t*         xact,
                                               rwdts_xact_block_t   *blk,
                                               uint64_t              corrid,
                                               rwdts_xact_result_t** const result,
                                               RWDtsQuery** query)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  RW_ASSERT(result);

  *result = NULL;
  RWDtsQuery *queryfound = NULL;

  if (!blk)
  {
    RW_ASSERT(xact->blocks_ct);
    blk = xact->blocks[0];
  }
  // Check if we have pending results
  if (RW_DL_LENGTH(&xact->track.pending)) {
    rwdts_xact_result_t *res;
    // If corrid is non zero filter based on the passed corrid
    for (res = RW_DL_HEAD(&xact->track.pending, rwdts_xact_result_t, elem);
         res != NULL ;
         res = RW_DL_NEXT(res, rwdts_xact_result_t, elem)) {
      RW_ASSERT(res->result);
      RW_ASSERT(res->result->block);
      if ((!blk || (res->blockidx == blk->subx.block->blockidx))
          && (!corrid
              || (res->result->has_corrid &&
                  res->result->corrid == corrid))) {
        int qid;// optimise by putting query check here
        for (qid = 0; qid < blk->subx.n_query; qid++)
        {
           if (!corrid
               || (blk->subx.query[qid]->corrid == corrid))
           {
             queryfound = blk->subx.query[qid];
             break;
           }
        }
        RW_DL_REMOVE(&xact->track.pending, res, elem);
        break;
      }
    }

    if (!res) {
      return RW_STATUS_FAILURE;
    }

    if (query && queryfound)
    {
      *query = queryfound;
    }
    *result = res;
    RW_DL_ENQUEUE(&(xact->track.returned), res, elem);
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_SUCCESS;
}


/* Get errors in a xact */
uint32_t
rwdts_xact_get_available_errors_count(rwdts_xact_t* xact,
                                      uint64_t      corrid)
{
  return rwdts_xact_get_error_count (xact, corrid);
}

rw_status_t
rwdts_xact_get_error(rwdts_xact_t*         xact,
                     uint64_t              corrid,
                     rwdts_query_error_t** const error)
{
  *error = rwdts_xact_query_error(xact, corrid);
  return ((*error)?RW_STATUS_SUCCESS:RW_STATUS_NOTFOUND);
}

rw_status_t
rwdts_xact_get_error_cause(const rwdts_query_error_t* query_error)
{
  return rwdts_query_error_get_cause(query_error);
}

const char*
rwdts_xact_get_error_errstr(const rwdts_query_error_t* query_error)
{
  return rwdts_query_error_get_errstr(query_error);
}

const rw_keyspec_path_t*
rwdts_xact_get_error_keyspec(const rwdts_query_error_t* query_error)
{
  return rwdts_query_error_get_keyspec(query_error);
}

const char*
rwdts_xact_get_error_xpath(const rwdts_query_error_t* query_error)
{
  return rwdts_query_error_get_xpath(query_error);
}

const char*
rwdts_xact_get_error_keystr(const rwdts_query_error_t* query_error)
{
  return rwdts_query_error_get_keystr(query_error);
}

void
rwdts_xact_destroy_error(rwdts_query_error_t** query_error)
{
  rwdts_query_error_destroy(*query_error);
  *query_error = NULL;
}

/*
 *  Begin a (sub)transaction with advise query based on a proto payload
 */

rwdts_xact_t*
rwdts_advise_query_xml(rwdts_api_t*      apih,
                       rwdts_xact_t*     xact_parent,
                       const char*       xml_in,
                       rwdts_event_cb_t* cb)
{
  RW_ASSERT_MESSAGE(0, "Unsupported API");
  return NULL;
}

static rw_status_t
rwdts_xact_block_add_query_int(rwdts_xact_block_t*          block,
                               const rw_keyspec_path_t*     key,
                               const char*                  xpath,
                               RWDtsQueryAction             action,
                               uint32_t                     flags,
                               uint64_t                     corrid,
                               const RWDtsDebug*            dbg,
                               const ProtobufCMessage*      msg,
                               rwdts_member_registration_t* reg)
{
  int freekey = FALSE;
  rw_status_t status = RW_STATUS_SUCCESS;
  RW_ASSERT(block);
  rwdts_xact_t *xact = block->xact;
  RW_ASSERT(xact);
  rwdts_api_t * apih = xact->apih;
  RW_ASSERT(apih);
  RW_ASSERT(block);
  RW_ASSERT(action != RWDTS_QUERY_INVALID);

  if (block->exec) {
    return RW_STATUS_FAILURE;
  }

  if (flags & RWDTS_XACT_FLAG_RETURN_PAYLOAD) {
    if ((action != RWDTS_QUERY_CREATE) &&
        (action != RWDTS_QUERY_UPDATE)) {
      return RW_STATUS_FAILURE;
    }
  }

  if (flags & RWDTS_XACT_FLAG_SUB_READ) {
    if (action != RWDTS_QUERY_READ) {
      return RW_STATUS_FAILURE;
    }
  }

  ProtobufCMessage *dup_msg = NULL;
  if (msg) {
    dup_msg = protobuf_c_message_duplicate(NULL, msg, msg->descriptor);
    RW_ASSERT(dup_msg);
  }

  if (!key) {
    RW_ASSERT(xpath);
    key = RW_KEYSPEC_PATH_FROM_XPATH(rwdts_api_get_ypbc_schema(apih),
                                     (char*)xpath,
                                     RW_XPATH_KEYSPEC, NULL, NULL);

    if (key == NULL) {
      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "Invalid xpath [%s], xpath to keyspec conversion failed \n", xpath);
      return RW_STATUS_FAILURE;
    }
    freekey = TRUE;
  }

  if (!(flags & RWDTS_XACT_FLAG_KEYLESS)) {
    if ((action != RWDTS_QUERY_READ) && (action != RWDTS_QUERY_DELETE)) {
      char *tmp_str = NULL;
      if (rw_keyspec_path_has_wildcards(key)) {
        rw_keyspec_path_get_new_print_buffer(key,
                                             NULL ,
                                             rwdts_api_get_ypbc_schema(apih),
                                             &tmp_str);
        RW_ASSERT_MESSAGE(0,"Wildcarded Key %s for member %s \n",
                          tmp_str, apih->client_path);
      }
      if (dup_msg) {
        if (!rw_keyspec_path_matches_message (key, &apih->ksi, dup_msg, true)) {
          rw_keyspec_path_get_new_print_buffer(key,
                                               NULL ,
                                               rwdts_api_get_ypbc_schema(apih),
                                               &tmp_str);        
          RW_ASSERT_MESSAGE(0,"Msg not matching keyspec %s for member %s \n",
                            tmp_str, apih->client_path);
        }
      }
      if (tmp_str) {
        free(tmp_str);
      }
    }
  }

  RWDtsQuery* query = rwdts_xact_init_query_from_pb(apih,
                                                    key,
                                                    dup_msg,
                                                    action,
                                                    corrid,
                                                    dbg,
                                                    flags,
                                                    reg);

  if (!query->key->keystr) {
    if (xpath) {
      query->key->keystr = strdup(xpath);
    } else {
      rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)key, NULL, rwdts_api_get_ypbc_schema(apih), &query->key->keystr);
    }
  }
  if (dup_msg) {
    protobuf_c_message_free_unpacked(NULL, dup_msg);
    dup_msg = NULL;
  }

  if (freekey) {
    RW_KEYSPEC_PATH_FREE((rw_keyspec_path_t *)key, NULL, NULL );
    key = NULL;
  }

  if (block->subx.n_query == 0) {
    block->subx.query =
        (RWDtsQuery**)RW_MALLOC0(sizeof(RWDtsQuery*) * RW_DTS_QUERY_BLOCK_ALLOC_CHUNK_SIZE);
  } else if ((block->subx.n_query % RW_DTS_QUERY_BLOCK_ALLOC_CHUNK_SIZE) == 0) {
    block->subx.query = realloc(block->subx.query,
              ((block->subx.n_query+RW_DTS_QUERY_BLOCK_ALLOC_CHUNK_SIZE) * sizeof(block->subx.query[0])));
  }
  block->subx.query[block->subx.n_query] = query;
  block->subx.n_query++;
  query->queryidx = block->subx.n_query;
  query->has_queryidx = TRUE;

  return status;
}

rw_status_t
rwdts_xact_block_add_query_reduction(rwdts_xact_block_t* block,
                                     const char* xpath,
                                     RWDtsQueryAction action,
                                     uint32_t flags,
                                     uint64_t corrid,
                                     const ProtobufCMessage* msg)
{
  // TODO: implement
  return RW_STATUS_FAILURE;
}


/*
 *  Begin a (sub)transaction with advise query based on a proto payload
 */
rw_status_t
rwdts_advise_query_proto_int(rwdts_api_t*                  apih,
                             rw_keyspec_path_t*           keyspec,
                             rwdts_xact_t*                xact_parent,
                             const ProtobufCMessage*      msg,
                             uint64_t                     corrid,
                             RWDtsDebug*                  dbg,
                             RWDtsQueryAction             action,
                             const rwdts_event_cb_t*      cb,
                             uint32_t                     flags,
                             rwdts_member_registration_t* reg,
                             rwdts_xact_t**               out_xact)
{
  rwdts_xact_t* xact = xact_parent;
  rw_status_t rw_status = RW_STATUS_FAILURE;
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(xact_parent == NULL); /* not supported */
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  RW_ASSERT(!(flags & RWDTS_XACT_FLAG_DEPTH_OBJECT));
  RW_ASSERT(!(flags & RWDTS_XACT_FLAG_DEPTH_ONE));
  RW_ASSERT((!xact_parent && ((flags&RWDTS_XACT_FLAG_END))) ||
            xact_parent);

  if (out_xact) *out_xact = NULL;
  if (!xact) {
    xact = rwdts_api_xact_create(apih,
                                flags|RWDTS_XACT_FLAG_ADVISE,
                                cb ? cb->cb : NULL,
                                cb ? cb->ud : NULL);

    if (xact == NULL) {
      return rw_status;
    }
  }

  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  if (blk == NULL) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return rw_status;
  }

  rs = rwdts_xact_block_add_query_int(blk,
                                      keyspec,
                                      NULL,
                                      action,
                                      flags|RWDTS_XACT_FLAG_ADVISE,
                                      corrid,
                                      NULL,
                                      msg,
                                      reg);

  if (rs != RW_STATUS_SUCCESS && !xact_parent) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return rw_status;
  }

  rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
  if (rs != RW_STATUS_SUCCESS && !xact_parent) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return rw_status;
  }

  if (action == RWDTS_QUERY_CREATE || action == RWDTS_QUERY_UPDATE) {
    RW_ASSERT(!rw_keyspec_path_has_wildcards((rw_keyspec_path_t*)keyspec));
  }
  if (out_xact) *out_xact = xact;

  return RW_STATUS_SUCCESS;
}

/*
 * Get results from the DTS transaction
 */

rw_status_t
rwdts_xact_get_result_pb(rwdts_xact_t*                     xact,
                         uint64_t                          corrid,
                         const ProtobufCMessageDescriptor* desc,
                         ProtobufCMessage**                result,
                         rw_keyspec_path_t**               keyspecs,
                         size_t                            index,
                         size_t                            size_result,
                         uint32_t*                         n_result)
{
  RW_ASSERT(xact);
  rwdts_xact_result_t *res = NULL;
  *n_result = 0;

  rw_status_t rw_status = rwdts_xact_get_result_pbraw(xact, NULL, corrid, &res, NULL);
  if (rw_status != RW_STATUS_SUCCESS || !res || !res->result || !res->result->result ) {
    return (RW_STATUS_FAILURE);
  }
  if (res->result->n_result <= index) {
    return ((res->result->n_result == index)? RW_STATUS_NOTFOUND:RW_STATUS_FAILURE);
  }
  return rwdts_xact_get_result_pb_int(xact, corrid, desc, result, keyspecs, index, size_result, n_result, res, NULL);
}

static uint32_t
rwdts_reroot_response(rwdts_xact_t*                 xact,
                      ProtobufCMessage*             matchmsg,
                      rw_keyspec_path_t*            result_keyspec,
                      uint32_t*                     n_result,
                      ProtobufCMessage***           result,
                      rw_keyspec_path_t***          keyspecs,
                      rw_keyspec_path_t*            keyspec_level)
{
  ProtobufCMessage* l_msg = NULL;
  rw_keyspec_path_t* lks = result_keyspec;
  rw_keyspec_path_t* local_ks = NULL;
  uint32_t count = *n_result;
  uint32_t num_count = 0;
  rw_keyspec_path_t *ret_keyspec = NULL;

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, lks , keyspec_level, NULL)) {
    return 0;
  }

  if (matchmsg) {
    RW_ASSERT ((void *)(((ProtobufCMessage*)(result_keyspec))->descriptor) == 
               (void *)(&rw_schema_path_spec__descriptor));
    ret_keyspec = rw_keyspec_path_create_dup_of_type(result_keyspec, &xact->ksi,
                                                     ((ProtobufCMessage*)(keyspec_level))->descriptor);
    
    RW_ASSERT(lks != NULL);

    size_t depth_i = rw_keyspec_path_get_depth(lks);
    size_t depth_o = rw_keyspec_path_get_depth(keyspec_level);

    if (depth_i <= depth_o) {
      rw_keyspec_path_reroot_iter_state_t state;
      rw_keyspec_path_reroot_iter_init(lks, &xact->ksi, &state, matchmsg, keyspec_level);
      while (rw_keyspec_path_reroot_iter_next(&state)) {
        l_msg = rw_keyspec_path_reroot_iter_take_msg(&state);
        if (l_msg) {
          *result   = realloc(*result, ((count+1) * sizeof(*result[0])));
          *keyspecs = realloc(*keyspecs, ((count+1) * sizeof(*keyspecs[0])));
          local_ks = rw_keyspec_path_reroot_iter_take_ks(&state);
          RW_ASSERT(local_ks);
          RW_ASSERT(local_ks != lks);
          (*result)[count] = l_msg;
          (*keyspecs)[count] = local_ks;
          count++;
          num_count++;
          l_msg = NULL; local_ks = NULL;
        }
      }
      rw_keyspec_path_reroot_iter_done(&state);
    } else {
      l_msg = RW_KEYSPEC_PATH_REROOT(lks, NULL, matchmsg, keyspec_level, NULL);
      if (l_msg) {
        rw_status_t rs = RW_KEYSPEC_PATH_TRUNC_SUFFIX_N(ret_keyspec, NULL, rw_keyspec_path_get_depth(keyspec_level), NULL);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
        *result   = realloc(*result, ((count+1) * sizeof(*result[0])));
        *keyspecs = realloc(*keyspecs, ((count+1) * sizeof(*keyspecs[0])));
        (*result)[count] = l_msg;
        (*keyspecs)[count] = ret_keyspec;
        ret_keyspec = NULL;
        count++;
        num_count++;
        matchmsg = NULL;
      }
    }
  }

  if (ret_keyspec) RW_KEYSPEC_PATH_FREE(ret_keyspec, NULL, NULL );

  *n_result += num_count;
  return num_count;
}

rw_status_t
rwdts_xact_get_result_pb_int_alloc(rwdts_xact_t*                     xact,
                                   uint64_t                          corrid,
                                   rwdts_xact_result_t*              res,
                                   ProtobufCMessage***               result,
                                   rw_keyspec_path_t***              keyspecs,
                                   uint32_t**                        types,
                                   rw_keyspec_path_t*                keyspec_level,
                                   uint32_t*                         n_result)
{
  int i;
  *n_result = 0;
  *result = NULL;
  *types = NULL;
  rw_status_t rs;

  for (i = 0; i < res->result->n_result; i ++) {
    RW_ASSERT(res->result->result[i]->result);
    RW_ASSERT(res->result->result[i]->result->ptype == RWDTS_PAYLOAD_PBRAW);
    RW_ASSERT(res->result->result[i]->path->ktype == RWDTS_KEY_RWKEYSPEC);

    const rw_yang_pb_schema_t* ks_schema = ((ProtobufCMessage*)keyspec_level)->descriptor->ypbc_mdesc->module->schema?
                ((ProtobufCMessage*)keyspec_level)->descriptor->ypbc_mdesc->module->schema:rwdts_api_get_ypbc_schema(xact->apih);

    RW_ASSERT(ks_schema);
    ProtobufCMessage*  matchmsg = NULL;
    if (res->result->result[i]->redn_resp_id < RWDTS_XPATH_PBCM_MSG) {
      rw_yang_pb_msgdesc_t*       desc_result   = NULL;
      rs = rw_keyspec_path_find_msg_desc_schema((rw_keyspec_path_t *)res->result->result[i]->path->keyspec,
                                                &xact->ksi,
                                                ks_schema,
                                                (const rw_yang_pb_msgdesc_t **)&desc_result,
                                                NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

      matchmsg = protobuf_c_message_unpack(NULL, desc_result->u->msg_msgdesc.pbc_mdesc,
                                           res->result->result[i]->result->paybuf.len,
                                           res->result->result[i]->result->paybuf.data);
    }
    else {
      matchmsg = protobuf_c_message_unpack(NULL,
                                           (ProtobufCMessageDescriptor*)&rwdts_redn_resp_entry__concrete_descriptor,
                                           res->result->result[i]->result->paybuf.len,
                                           res->result->result[i]->result->paybuf.data);
    }
    
    if (matchmsg)  {
      if (res->result->result[i]->redn_resp_id >= RWDTS_XPATH_PBCM_LEAF) {
        rw_keyspec_path_t *tmp_keyspec = NULL;
        *result   = realloc(*result, ((*n_result+1) * sizeof(*result[0])));
        *keyspecs = realloc(*keyspecs, ((*n_result+1) * sizeof(*keyspecs[0])));
        *types = realloc(*types, ((*n_result+1) * sizeof(uint32_t)));
        (*result)[*n_result] = protobuf_c_message_duplicate(NULL, matchmsg, matchmsg->descriptor);
        rw_keyspec_path_create_dup((rw_keyspec_path_t *)res->result->result[i]->path->keyspec,
                                   &xact->ksi, &tmp_keyspec);
        (*keyspecs)[*n_result] = tmp_keyspec;
        (*types)[*n_result] = res->result->result[i]->redn_resp_id;
        *n_result += 1;
      }
      else {
        rwdts_reroot_response(xact,
                              matchmsg,
                              (rw_keyspec_path_t *)res->result->result[i]->path->keyspec,
                              n_result,
                              result,
                              keyspecs,
                              keyspec_level);
      }

      protobuf_c_message_free_unpacked(NULL, matchmsg);
    }
    else {
      char *ks_str = NULL;
      rw_keyspec_path_get_new_print_buffer((rw_keyspec_path_t*)res->result->result[i]->path->keyspec,
                                           NULL,
                                           NULL,
                                           &ks_str);
      RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_NullMsgRcvd, 
                               RWLOG_ATTR_SPRINTF("Got null message for keyspec %s, i[%d], n_result[%d], paybuf_len[%d], paybuf_data[%p], type[%u]",
                                                  ks_str, i, (int)res->result->n_result, (int)res->result->result[i]->result->paybuf.len,
                                                  res->result->result[i]->result->paybuf.data, res->result->result[i]->redn_resp_id));
      free(ks_str);
    }
  }
  return RW_STATUS_SUCCESS;
}

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
                             rw_keyspec_path_t*                keyspec_level)
{
  int i;
  *n_result = 0;
  *result = NULL;

  for (i = index; i < res->result->n_result; i ++) {
    RW_ASSERT(res->result->result[i]->result);
    RW_ASSERT(res->result->result[i]->result->ptype == RWDTS_PAYLOAD_PBRAW);

    result[*n_result] = protobuf_c_message_unpack(NULL, desc, res->result->result[i]->result->paybuf.len,
                                                  res->result->result[i]->result->paybuf.data);

    if (result[*n_result] && keyspecs) {
      RW_ASSERT(res->result->result[i]->path->ktype == RWDTS_KEY_RWKEYSPEC);
      keyspecs[*n_result] = RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE((rw_keyspec_path_t*)res->result->result[i]->path->keyspec,
                                                               NULL,
                                                               &rw_schema_path_spec__descriptor,
                                                               NULL);
      RW_ASSERT(keyspecs[*n_result] != NULL);
    }
    if (result[*n_result]) {
      (*n_result)++;
      if ((*n_result) >= size_result) {
        break;
      }
    }
  }
  return RW_STATUS_SUCCESS;
}

uint32_t rwdts_xact_get_available_results_count(rwdts_xact_t* xact,
                                                rwdts_xact_block_t   *blk,
                                                uint64_t      corrid)
{
  uint32_t count = 0;

  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  // Check if we have pending results
  if (RW_DL_LENGTH(&xact->track.pending)) {
    rwdts_xact_result_t *res;
    for (res = RW_DL_HEAD(&xact->track.pending, rwdts_xact_result_t, elem);
         res != NULL ;
         res = RW_DL_NEXT(res, rwdts_xact_result_t, elem)) {
      RW_ASSERT(res->result);
      RW_ASSERT(res->result->block);
      if ((!blk || (res->result->block->blockidx == blk->subx.block->blockidx))
          && (!corrid || (res->result->has_corrid &&  res->result->corrid == corrid))) {
        count += res->result->n_result;
      }
    }
  }
  return count;
}

uint32_t
rwdts_xact_get_block_count(rwdts_xact_t *xact)
{
  RW_ASSERT(xact);

  return (xact->blocks_ct);

}

rw_status_t
rwdts_xact_trace(rwdts_xact_t* xact)
{
  xact->trace=1;
  return RW_STATUS_SUCCESS;
}

/*
 * Get the result of a block query.
 */
rwdts_query_result_t*
rwdts_xact_block_query_result_int(rwdts_xact_block_t *blk,
                              rwdts_xact_t* xact,
                              uint64_t      corrid)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_api_t *apih =  xact->apih;
  rwdts_xact_result_t *res = NULL;
  RWDtsQuery *query = NULL;
  RW_ASSERT_TYPE(apih, rwdts_api_t);


  if (xact->track.query_results_next_idx) {
    RW_ASSERT(xact->track.query_results);
    if (xact->track.query_results_next_idx < xact->track.num_query_results) {
      return (&xact->track.query_results[xact->track.query_results_next_idx++]);
    }
  }
  rw_status_t rw_status = rwdts_xact_get_result_pbraw(xact, blk, corrid, &res, &query);

  if (rw_status != RW_STATUS_SUCCESS || !res || !res->result ||
      !res->result->result || res->result->n_result == 0)
  {
    return (NULL);
  }
  RWDTS_ASSERT_MESSAGE(query, apih, 0,0, " No corresponding query found");
  uint32_t  idx = 0;
  uint32_t  n_dts_data  = 0;
  ProtobufCMessage**           msgs = NULL;
  rw_keyspec_path_t**          keyspecs = NULL;
  uint32_t*                    types = NULL;
  const  rw_yang_pb_schema_t* schema   = rwdts_api_get_ypbc_schema(apih);
  rwdts_xact_query_result_list_deinit(xact);
  RW_ASSERT(xact->track.query_results == NULL);

  rw_keyspec_path_t *reroot_keyspec = NULL;
  if (RW_SCHEMA_CATEGORY_RPC_INPUT == rw_keyspec_path_get_category((rw_keyspec_path_t* )query->key->keyspec)) {
    reroot_keyspec = rw_keyspec_path_create_output_from_input((rw_keyspec_path_t* )query->key->keyspec, &xact->ksi, schema);
  }
  else {
    rw_keyspec_path_create_dup((rw_keyspec_path_t* )query->key->keyspec, &xact->ksi, &reroot_keyspec);
  }
  RW_ASSERT (reroot_keyspec);

  rwdts_xact_get_result_pb_int_alloc(xact,
                                     corrid,
                                     res,
                                     &msgs,
                                     &keyspecs,
                                     &types,
                                     (rw_keyspec_path_t *)reroot_keyspec,
                                     &n_dts_data);
  rw_keyspec_path_free(reroot_keyspec, NULL);
  if (n_dts_data == 0)
  {
    return (NULL);
  }
  xact->track.query_results = (rwdts_query_result_t*)RW_MALLOC0(sizeof(rwdts_query_result_t) * n_dts_data);
  for(; idx <n_dts_data;idx++, xact->track.num_query_results++)
  {
    xact->track.query_results[idx].apih = apih;
    xact->track.query_results[idx].message =  msgs[idx];
    xact->track.query_results[idx].keyspec = keyspecs[idx];
    xact->track.query_results[idx].corrid = res->result->corrid;
    if (types) {
      xact->track.query_results[idx].type = types[idx];
    }
    RW_ASSERT(xact->track.query_results[idx].keyspec);
    xact->track.query_results[idx].key_xpath = rw_keyspec_path_to_xpath(xact->track.query_results[idx].keyspec,
                                                                        schema, NULL);
  }

  RW_ASSERT(xact->track.query_results);
  RW_ASSERT(xact->track.query_results_next_idx < xact->track.num_query_results);

  xact->track.query_results_next_idx++;
  free(keyspecs);
  free(msgs);
  if (types) { 
    free(types);
    types = NULL;
  }
  keyspecs = NULL, msgs = NULL;
  return (&xact->track.query_results[0]);
}

/*
 * Get the result of a query
 */
rwdts_query_result_t*
rwdts_xact_query_result(rwdts_xact_t* xact,
                        uint64_t      corrid)
{
  return (rwdts_xact_block_query_result_int(NULL,xact, corrid));
}
/*
 * Get the result of a query - GI version -- takes a reference on xact on get
 */
rwdts_query_result_t*
rwdts_xact_query_result_gi(rwdts_xact_t* xact,
                           uint64_t      corrid)
{
  rwdts_query_result_t *qrslt = rwdts_xact_block_query_result_int(NULL,xact, corrid);

  if (!qrslt) {
    return NULL;
  }

  rwdts_query_result_t *dup_qrslt = rwdts_qrslt_dup(qrslt);
  RW_ASSERT(dup_qrslt);

  return dup_qrslt;
}

/*
 * Get the result of a query from a block
 */
rwdts_query_result_t*
rwdts_xact_block_query_result(rwdts_xact_block_t* blk,
                              uint64_t            corrid)
{
  return(rwdts_xact_block_query_result_int(blk,blk->xact,corrid));
}

/*
 * Get the result of a query from a block  -- GI versio -- takes reference on the transaction
 */

rwdts_query_result_t*
rwdts_xact_block_query_result_gi(rwdts_xact_block_t* blk,
                                 uint64_t            corrid)
{
  rwdts_query_result_t *qrslt =  rwdts_xact_block_query_result_int(blk,blk->xact,corrid);

  if (!qrslt) {
    return NULL;
  }

  rwdts_query_result_t *dup_qrslt = rwdts_qrslt_dup(qrslt);
  RW_ASSERT(dup_qrslt);

  return dup_qrslt;
}


/*
 * return keyspec from query result
 */
const rw_keyspec_path_t*
rwdts_query_result_get_keyspec(const rwdts_query_result_t *query_result)
{
  RW_ASSERT(query_result);
  return query_result->keyspec;
}

/*
 * return protobuf result from query result
 */
const ProtobufCMessage*
rwdts_query_result_get_protobuf(const rwdts_query_result_t *query_result)
{
  RW_ASSERT(query_result);
  return query_result->message;
}

/*
 * return xpath result from query result
 */
const char*
rwdts_query_result_get_xpath(const rwdts_query_result_t *query_result)
{
  RW_ASSERT(query_result);
  return  (query_result->key_xpath);
}


rw_status_t
rwdts_query_result_get_value(const rwdts_query_result_t *query_result,
                             ProtobufCType *type,
                             void **value)
{
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(query_result);
  RW_ASSERT(query_result->keyspec);
  RW_ASSERT(query_result->message);
  RW_ASSERT(type);
  RW_ASSERT(value);

  if (query_result->type >= RWDTS_XPATH_PBCM_MSG) {
    RWDtsRednRespEntry *entry = (RWDtsRednRespEntry*)query_result->message;
    if (entry->has_val_double) {
      *type = PROTOBUF_C_TYPE_DOUBLE;
      double *val = RW_MALLOC0(sizeof(double));
      if (val) {
        *val = entry->val_double;
        *value = (void*)val;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else if (entry->has_val_uint64) {
      *type = PROTOBUF_C_TYPE_UINT64;
      uint64_t *val = RW_MALLOC0(sizeof(uint64_t));
      if (val) {
        *val = entry->val_uint64;
        *value = (void*)val;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else if (entry->has_val_sint64) {
      *type = PROTOBUF_C_TYPE_SINT64;
      int64_t *val = RW_MALLOC0(sizeof(int64_t));
      if (val) {
        *val = entry->val_sint64;
        *value = (void*)val;
        rs = RW_STATUS_SUCCESS;
      }
    }
    else if (entry->has_count) {
      *type = PROTOBUF_C_TYPE_UINT64;
      uint64_t *val = RW_MALLOC0(sizeof(uint64_t));
      if (val) {
        *val = entry->count;
        *value = (void*)val;
        rs = RW_STATUS_SUCCESS;
      }
    }
  }
  else {
    if (rw_keyspec_path_is_leafy(query_result->keyspec)) {
      rw_keyspec_path_t* new_ks = NULL;
      if (rw_keyspec_path_is_generic(query_result->keyspec)) {
        rwdts_api_t *apih = query_result->apih;
        RW_ASSERT_TYPE(apih, rwdts_api_t);

        if (RW_STATUS_SUCCESS != rw_keyspec_path_find_spec_ks(query_result->keyspec,
                                                              NULL,
                                                              rwdts_api_get_ypbc_schema(apih),
                                                              &new_ks)) {
          if (new_ks) {
            rw_keyspec_path_free(new_ks, NULL);
            new_ks = NULL;
          }
        }
      }

      const ProtobufCFieldDescriptor* fd = NULL;
      if (new_ks) {
        fd = rw_keyspec_path_get_leaf_field_desc(new_ks);
        rw_keyspec_path_free(new_ks, NULL);
        new_ks = NULL;
      }
      else {
        fd = rw_keyspec_path_get_leaf_field_desc(query_result->keyspec);
      }
      RW_ASSERT(fd);

      ProtobufCFieldInfo finfo;
      bool ok = protobuf_c_message_get_field_instance(NULL, query_result->message,
                                                      fd, 0, &finfo);
      if (ok) {
        *type = fd->type;
        void *val = RW_MALLOC0(sizeof(finfo.len));
        if (val) {
          memcpy(val, finfo.element, finfo.len);
          *value = val;
          rs = RW_STATUS_SUCCESS;
        }
      }
    }
  }
  return rs;
}
/*
 * Get the probable error cause
 */
rw_status_t
rwdts_xact_get_error_heuristic (rwdts_xact_t* xact,
                                uint64_t corrid,
                                char** xpath,
                                rw_status_t *err_cause,
                                char** errstr)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(*xpath == NULL);
  RW_ASSERT(*errstr == NULL);
  RWDtsErrorReport *rep = NULL;
  rw_status_t rc = RW_STATUS_FAILURE;

  if (xact->xres && xact->xres->dbg && xact->xres->dbg->err_report) {
    rep = xact->xres->dbg->err_report;
  }
  else if (xact->err_report) {
    rep = xact->err_report;
  }

  rw_status_t cause = RW_STATUS_SUCCESS;
  char *str = NULL;
  RWDtsKey *key = NULL;
  if (rep && rep->n_ent) {
    int i = 0;
    for (i = 0; i<rep->n_ent; i++) {
      if (!corrid || (rep->ent[i]->corrid == corrid)) {
        RWDtsErrorEntry *ent = rep->ent[i];
        // Simple logic to see which error has the bigger cause code
        // and give preference to externally generated error string
        // Identifying the internally generated error strings
        // using 'RWDTS: ' prefix
        // FIXME: add better heurisitcs
        static const char* prefix = "RWDTS: ";
        if (ent->cause > cause) {
          // If the new errstr is not DTS generated
          if (ent->errstr && !strstr(ent->errstr, prefix)) {
            cause = ent->cause;
            str = ent->errstr;
            key = ent->key;
          }
          // If the current errstr is DTS generated
          else if (!str || strstr(str, prefix)) {
            cause = ent->cause;
            str = ent->errstr;
            key = ent->key;
          }
        }
        else if (ent->errstr && !strstr(ent->errstr, prefix)
                 && strstr(str, prefix)) {
          cause = ent->cause;
          str = ent->errstr;
          key = ent->key;
        }
        rc = RW_STATUS_SUCCESS;
      }
    }
  } else {
    if (xact->bounced) {
      *errstr = RW_STRDUP("RWDTS: MESSAGE BOUNCED");
      cause = rc = RW_STATUS_FAILURE;
    }
  }

  *err_cause = cause;
  if (str) {
    *errstr = RW_STRDUP(str);
  }
  if (key) {
    RW_ASSERT(key->keyspec);
    const rw_yang_pb_schema_t*  schema =
        rwdts_api_get_ypbc_schema(xact->apih);
    if (schema) {
      *xpath = rw_keyspec_path_to_xpath(
          (rw_keyspec_path_t*)key->keyspec,
          schema,
          &xact->apih->ksi);
    }
  }

  return rc;
}

/*
 * Get the error count
 */
guint32
rwdts_xact_get_error_count (rwdts_xact_t* xact,
                              uint64_t corrid)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(xact->apih);
  RWDtsErrorReport *rep = NULL;

  if (xact->xres && xact->xres->dbg && xact->xres->dbg->err_report) {
    rep = xact->xres->dbg->err_report;
  }
  else if (xact->err_report) {
    rep = xact->err_report;
  }

  int count = 0;
  if (rep && rep->n_ent) {
    int i = 0;
    for (i = 0; i<rep->n_ent; i++) {
      if (!corrid || rep->ent[i]->corrid == corrid) {
        count++;
      }
    }
    xact->track.error_idx = 0; // Reset the last error index
  }

  return count;
}

/*
 * Get the errors in a block query.
 */
rwdts_query_error_t*
rwdts_xact_block_query_error_int(rwdts_xact_t* xact,
                              uint64_t      corrid)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(xact->apih);

  rwdts_api_t *apih =  xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_query_error_t *res = NULL;
  RWDtsErrorReport *rep = NULL;
  if (xact->xres && xact->xres->dbg && xact->xres->dbg->err_report) {
    rep = xact->xres->dbg->err_report;
  }
  else if (xact->err_report) {
    rep = xact->err_report;
  }
  if (rep && rep->n_ent) {
    int i = 0;
    for (i = xact->track.error_idx; i<rep->n_ent; i++) {
      if (!corrid || (rep->ent[i]->corrid == corrid)) {
        RWDtsErrorEntry *ent = rep->ent[i];
        RW_ASSERT(ent);
        res = rwdts_query_error_new();
        if (res) {
          res->apih = xact->apih;
          res->error =
              protobuf_c_message_duplicate(&protobuf_c_default_instance,
                                           &ent->base, ent->base.descriptor);
          res->cause = ent->cause;
          if (ent->key) {
            RW_ASSERT(ent->key->keyspec);
            rw_keyspec_path_create_dup ((rw_keyspec_path_t *)ent->key->keyspec,
                                        &xact->ksi, &res->keyspec);
            RW_ASSERT(res->keyspec);
            const rw_yang_pb_schema_t*  schema =
                rwdts_api_get_ypbc_schema(xact->apih);
            RW_ASSERT(schema);
            res->key_xpath = rw_keyspec_path_to_xpath(
                (rw_keyspec_path_t*)ent->key->keyspec,
                schema,
                NULL);
            if (!res->key_xpath) {
              res->key_xpath = ent->keystr_buf;
            }
          }
          else {
            res->keyspec = NULL;
            res->key_xpath = NULL;
          }
          res->keystr = RW_STRDUP(ent->keystr_buf);
          res->errstr = RW_STRDUP(ent->errstr);
          xact->track.error_idx = i+1; //Go past the current index for next query
        }
        break;
      }
    }
  }
  return res;
}

/*
 * Get the errors in a query
 */
rwdts_query_error_t*
rwdts_xact_query_error(rwdts_xact_t* xact,
                        uint64_t      corrid)
{
  return (rwdts_xact_block_query_error_int(xact, corrid));
}

/*
 * return cause from query error
 */
rw_status_t
rwdts_query_error_get_cause(const rwdts_query_error_t* query_error)
{
  RW_ASSERT(query_error);
  return (query_error->cause);
}

/*
 * return error string from query error
 */
const char *
rwdts_query_error_get_errstr(const rwdts_query_error_t* query_error)
{
  RW_ASSERT(query_error);
  return (query_error->errstr);
}

/*
 * return keyspec from query error
 */
const rw_keyspec_path_t*
rwdts_query_error_get_keyspec(const rwdts_query_error_t *query_error)
{
  RW_ASSERT(query_error);
  return (query_error->keyspec);
}

/*
 * return protobuf error from query error
 */
const ProtobufCMessage*
rwdts_query_error_get_protobuf(const rwdts_query_error_t *query_error)
{
  RW_ASSERT(query_error);
  return query_error->error;
}

/*
 * return xpath from query error
 */
const char *
rwdts_query_error_get_xpath(const rwdts_query_error_t *query_error)
{
  RW_ASSERT(query_error);
  return (query_error->key_xpath);
}

/*
 * return keystr from query error
 */
const char*
rwdts_query_error_get_keystr(const rwdts_query_error_t *query_error)
{
  RW_ASSERT(query_error);
  return (query_error->keystr);
}

/*
 * return xml error from query error
 */
gchar*
rwdts_query_error_get_xml(const rwdts_query_error_t *query_error)
{
  gchar* xml_str = NULL;
  RW_ASSERT(query_error);
  RW_ASSERT(query_error->apih);
  if (query_error->error->descriptor->ypbc_mdesc) {
    rwdts_api_t *apih = query_error->apih;
    RW_ASSERT_TYPE(apih, rwdts_api_t);

    rw_xml_manager_t* mgr        = rw_xml_manager_create_xerces();
    RW_ASSERT(mgr);
    rw_yang_model_t*  model = rw_xml_manager_get_yang_model(mgr);
    RW_ASSERT(model);
    rw_status_t rw_status =
        rw_yang_model_load_schema_ypbc(model, rwdts_api_get_ypbc_schema(apih));
    RW_ASSERT(rw_status == RW_STATUS_SUCCESS);

    xml_str = (gchar*) rw_xml_manager_pb_to_xml_alloc_string(mgr,
                                                             query_error->error);
    rw_xml_manager_release(mgr);
  }
  
  return xml_str;
}

gchar *
rwdts_json_from_pbcm(const ProtobufCMessage* pbcm)
{
  gchar *json_str = NULL;
  rw_json_pbcm_to_json(pbcm, NULL,&json_str);
  return json_str;
}
/*
 * Destroy the error object
 */
void
rwdts_query_error_destroy(rwdts_query_error_t* query_error)
{
  rwdts_query_error_unref(query_error);
}


rwdts_xact_block_t*
rwdts_xact_block_create_gi(rwdts_xact_t *xact) {
  rwdts_xact_block_t *block;

  block = rwdts_xact_block_create(xact);
  if (block) {
    rwdts_xact_block_ref(block, __PRETTY_FUNCTION__, __LINE__);
  }
  return block;
}

rwdts_xact_block_t*
rwdts_xact_block_create(rwdts_xact_t *xact) {
  RW_ASSERT(xact);
  rwdts_api_t *apih = (rwdts_api_t *)xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  static uint32_t blockidx = 1;

  rwdts_xact_block_t *block = RW_MALLOC0(sizeof(rwdts_xact_block_t));
  block->xact = xact;
  rwdts_xact_block_ref(block, __PRETTY_FUNCTION__, __LINE__);  // xact ref

  RW_ASSERT(block);
  rwdts_xact_block__init(&block->subx);
  block->subx.block = RW_MALLOC0(sizeof(RWDtsXactBlkID));
  rwdts_xact_blk_id__init((RWDtsXactBlkID *)block->subx.block);
  block->subx.block->blockidx = blockidx;
  block->subx.block->routeridx = apih->router_idx;
  if (!apih->client_idx) {
    block->subx.block->clientidx = rw_random() + RWDTS_STR_HASH(apih->client_path);
  }
  else {
    block->subx.block->clientidx = apih->client_idx;
  }
  blockidx++;

  if (HASH_CNT(hh, xact->queries)) {
    block->newblockadd_notify = RWDTS_NEWBLOCK_TO_NOTIFY;
  }

  xact->blocks_ct++;
  xact->blocks = realloc(xact->blocks, (xact->blocks_ct * sizeof(xact->blocks[0])));
  xact->blocks[xact->blocks_ct-1] = block;

  /* Caller must take a ref if he is Python or will refer to the block beyond the xact lifespan */
  return block;
}



rw_status_t
rwdts_xact_block_add_query(rwdts_xact_block_t* block,
                           const char* xpath,
                           RWDtsQueryAction action,
                           uint32_t flags,
                           uint64_t corrid,
                           const ProtobufCMessage* msg)
{
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  return rwdts_xact_block_add_query_int(block, NULL, xpath, action, flags, corrid, NULL, msg, NULL);
}

rw_status_t
rwdts_xact_block_add_query_ks_reg(rwdts_xact_block_t* block,
                                  const rw_keyspec_path_t* key,
                                  RWDtsQueryAction action,
                                  uint32_t flags,
                                  uint64_t corrid,
                                  const ProtobufCMessage* msg,
                                  rwdts_member_registration_t* reg)
{
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  return rwdts_xact_block_add_query_int(block, key, NULL, action, flags, corrid, NULL, msg, reg);
}
rw_status_t
rwdts_xact_block_add_query_ks(rwdts_xact_block_t* block,
                              const rw_keyspec_path_t* key,
                              RWDtsQueryAction action,
                              uint32_t flags,
                              uint64_t corrid,
                              const ProtobufCMessage* msg)
{
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  return rwdts_xact_block_add_query_int(block, key, NULL, action, flags, corrid, NULL, msg, NULL);
}

rw_status_t
rwdts_xact_block_execute_immediate_gi(rwdts_xact_block_t *block,
                                      uint32_t flags,
                                      rwdts_query_event_cb_routine callback,
                                      const void *userdata,
                                      GDestroyNotify ud_dtor) {
  block->gdestroynotify = ud_dtor;
  return rwdts_xact_block_execute_immediate(block, flags, callback, userdata);
}
rw_status_t
rwdts_xact_block_execute_immediate(rwdts_xact_block_t* block,
                                   uint32_t flags,
                                   rwdts_query_event_cb_routine callback,
                                   const void *userdata) {
  return rwdts_xact_block_execute(block,
                                  (flags | RWDTS_XACT_FLAG_EXECNOW),
                                  callback,
                                  userdata,
                                  NULL);
}

rw_status_t
rwdts_xact_block_execute_gi(rwdts_xact_block_t* block,
                            uint32_t flags,
                            rwdts_query_event_cb_routine callback,
                            const void *userdata,
                            GDestroyNotify ud_dtor,
                            rwdts_xact_block_t *after_block)
{
  block->gdestroynotify = ud_dtor;
  return rwdts_xact_block_execute(block, flags, callback, userdata, after_block);
}

rw_status_t
rwdts_xact_block_execute_reduction(rwdts_xact_block_t *block,
                                   uint32_t flags,
                                   rwdts_query_event_cb_routine callback,
                                   const void *user_data,
                                   GDestroyNotify ud_dtor,
                                   rwdts_xact_block_t *after_block)
{
  // TODO: implement
  return RW_STATUS_FAILURE;
}


static void rwdts_apply_trace(RWDtsXact *rpc_xact, rwdts_xact_t *xact, rwdts_api_t *apih)
{
  int traceme = !!xact->trace;
  int i;
  rwdts_trace_filter_t *filt = NULL;
  for (i = 0; i < rpc_xact->n_subx; i++) {
    int j;
    for (j = 0; j < rpc_xact->subx[i]->n_query; j++) {
      RWDtsQuery *query = rpc_xact->subx[i]->query[j];
      RW_ASSERT(query);
      rw_keyspec_path_t *ks = (rw_keyspec_path_t*)query->key->keyspec;
      if (!ks) {
        // bogus keying for unit tests only, assume string and copy
        RW_ASSERT(query->key->keybuf.data);
        RW_ASSERT(isascii(query->key->keybuf.data[0]));
        query->key->keystr = strdup((const char *)query->key->keybuf.data);
        continue;
      }
      char *ks_str = NULL;
      rw_keyspec_path_get_new_print_buffer(ks, NULL , NULL, &ks_str);
      if (!query->key->keystr && ks_str) {
        query->key->keystr = strdup(ks_str);
      }

      if (1) {
        int k;
        for (k=0; k < apih->conf1.tracert.filters_ct; k++) {
          filt = apih->conf1.tracert.filters[k];
          const rw_yang_pb_schema_t* schema = NULL;
          /* Find the schema, it is composite  for Uagent  */
          schema = rwdts_api_get_ypbc_schema(apih);
          if (schema) {
            size_t index = 0;
            if (RW_KEYSPEC_PATH_IS_MATCH_DETAIL(NULL, ks, filt->path, &index, NULL, NULL)) {
              RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, BEGIN_XACT_QUERY,"Beginning DTS transaction (TRACING)", query->queryidx, ks_str ? ks_str : "");
              traceme = TRUE;
              break;
            }
          }
        }

        if (!traceme) {
          if (xact->tr) {
            rwdts_trace_filter_t filt;
            RW_ZERO_VARIABLE(&filt);

            filt.print = xact->tr->print_;
            filt.break_start = xact->tr->break_start;
            filt.break_prepare = xact->tr->break_prepare;
            filt.break_end = xact->tr->break_end;
            rwdts_xact_dbg_tracert_start(rpc_xact, &filt);
          }
        }
        RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, BEGIN_XACT_QUERY, "Beginning DTS transaction (not tracing)", query->queryidx, ks_str ? ks_str : "");
      } else {
        RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, BEGIN_XACT_QUERY, "Beginning DTS transaction", query->queryidx, ks_str ? ks_str : "");
      }
      free(ks_str);
    }
  }
  if (traceme) {
    /* FIXME, how shall we trigger this? */
    rwdts_xact_dbg_tracert_start(rpc_xact, filt);
  }

  if (rpc_xact->dbg && rpc_xact->dbg->tr) {
    RWDtsTracerouteEnt ent;
    char xact_id_str[128];
    rwdts_traceroute_ent__init(&ent);
    ent.func = (char*)__PRETTY_FUNCTION__;
    ent.line = __LINE__;
    ent.what = RWDTS_TRACEROUTE_WHAT_REQ;
    ent.event = RWDTS_EVT_QUERY;
    ent.dstpath = apih->router_path;
    ent.srcpath = apih->client_path;
    ent.state = RWDTS_TRACEROUTE_STATE_SENT;
    rwdts_dbg_tracert_add_ent(rpc_xact->dbg->tr, &ent);
    if (rpc_xact->dbg->tr->print_) {
      DTS_PRINT("%s:%u: TYPE:%s, EVENT:%s, DST:%s, SRC:%s, STATE:%s\n", ent.func, ent.line,
                rwdts_print_ent_type(ent.what), rwdts_print_ent_event(ent.event),
                ent.dstpath, ent.srcpath, rwdts_print_ent_state(ent.state));
    }
    if (rpc_xact->dbg->tr->break_start) {
      G_BREAKPOINT();
    }
    RWDTS_TRACE_EVENT_REQ(xact->apih->rwlog_instance, RwDtsApiLog_notif_TraceReq,rwdts_xact_id_str(&xact->id,xact_id_str,128),ent);
  }

}
rw_status_t
rwdts_xact_block_execute(rwdts_xact_block_t* block,
                         uint32_t flags,
                         rwdts_query_event_cb_routine callback,
                         const void *userdata,
                         rwdts_xact_block_t *after_block)
{
  RW_ASSERT(block);
  rwdts_xact_t *xact = block->xact;
  RW_ASSERT(xact);

  rwdts_api_t* apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (block->exec) {
    return RW_STATUS_FAILURE;
  }

  /*
   * If member had done a block execute(append)
   * from prepare callback , then we take additonal ref
   * to hold on to this xact until this is over
   */
  if (xact->mbr_state == RWDTS_MEMB_XACT_ST_PREPARE) {
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  }

  if (callback) {
    block->cb.cb = callback;
    block->cb.ud = userdata;
  }

  block->subx.flags |= flags;
  block->exec = TRUE;

  RW_ASSERT(!after_block);        /* not supported! */


  if ((flags&RWDTS_XACT_FLAG_TRACE) || apih->trace.on) {
    rwdts_xact_trace(xact);
  }
  return rwdts_xact_send(xact);
}


/* Protocol bowdlerized simplification:

 * We send one request with everything
 * The router sends one response with everything, generally at least one block being done in each response
    + We keep one request outstanding, except
    + We send a gratuitous request on execute
*/

/* Send all blocks.  TODO: Jettison payloads / queries on first send, the router keeps the first copy.  */
static rw_status_t
rwdts_xact_send(rwdts_xact_t *xact) 
{
  rwdts_api_t * apih = xact->apih;
  rwmsg_request_t *req_out = NULL;
  rw_status_t rs;

  RW_ASSERT(xact);
  RW_ASSERT(apih);

  xact->apih->stats.sent_credits++;
  RWDtsXact rpc_xact;

  rwdts_xact__init(&rpc_xact);

  rpc_xact.block = RW_MALLOC0(sizeof(RWDtsXactBlkID));
  rwdts_xact_blk_id__init((RWDtsXactBlkID *)rpc_xact.block);
  rpc_xact.block->clientidx = apih->client_idx;
  rpc_xact.block->routeridx = apih->router_idx;
  protobuf_c_message_memcpy(&rpc_xact.id.base, &xact->id.base);
  rpc_xact.client_path = RW_STRDUP(apih->client_path);

  rpc_xact.n_subx = xact->blocks_ct;
  RW_ASSERT(xact->blocks_ct);
  rpc_xact.subx = (RWDtsXactBlock**) xact->blocks;
  rpc_xact.flags |= RWDTS_XACT_FLAG_UPDATE_CREDITS; /* why is there even a flag, just always do it */
  rpc_xact.flags |= (xact->flags & RWDTS_XACT_FLAG_PEER_REG);
  rpc_xact.flags |= (xact->flags & RWDTS_XACT_FLAG_REG);
  rpc_xact.has_flags = TRUE;

  rwmsg_closure_t clo = {.pbrsp = (rwmsg_pbapi_cb)rwdts_xact_query_cb, .ud = xact};

  if (xact->flags & RWDTS_XACT_FLAG_IMM_BOUNCE) {
    rwmsg_destination_set_noconndontq(apih->client.dest);
  }
  xact->bounced = FALSE;
  ///FIXME: trace this
#if 0
{
      int b;
      for (b=0; b<xact->blocks_ct; b++) {
        int q;
        for (q=0; q<xact->blocks[b]->subx.n_query; q++) {
          if (strstr("C,/nsr:ns-instance-config", (xact->blocks[b]->subx.query[q]->key->keystr))) {
            xact->trace = 1;
            xact->flags |= RWDTS_XACT_FLAG_TRACE;
            rpc_xact.flags |= RWDTS_XACT_FLAG_TRACE;
          }
        }
      }
    }
#endif
  rwdts_apply_trace(&rpc_xact, xact, apih);

  rs = rwdts_query_router__execute(&apih->client.service,
                                   apih->client.dest,
                                   &rpc_xact,
                                   &clo,
                                   &req_out);
  if (xact->flags & RWDTS_XACT_FLAG_IMM_BOUNCE) {
    rwmsg_destination_unset_noconndontq(apih->client.dest);
  }
  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            req_out, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Execute Req)");

  if (rs == RW_STATUS_SUCCESS) {
    xact->req_out++;
    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
    /* Let there be no more requests without keystr filled in. It makes debugging unpossible. */
    {
      int b;
      for (b=0; b<xact->blocks_ct; b++) {
        int q;
        for (q=0; q<xact->blocks[b]->subx.n_query; q++) {
          RW_ASSERT(xact->blocks[b]->subx.query[q]->key);
          RW_ASSERT(xact->blocks[b]->subx.query[q]->key->keystr);
          rwdts_member_notify_newblock(xact, xact->blocks[b]);
        }
      }
    }

  }
  RW_FREE(rpc_xact.block);
  RW_FREE(rpc_xact.client_path);
  if (rpc_xact.dbg && rpc_xact.dbg->tr) {
    rwdts_traceroute__free_unpacked(NULL,rpc_xact.dbg->tr); 
    rpc_xact.dbg->tr = NULL;
  }
  if (rpc_xact.dbg) {
    RW_FREE(rpc_xact.dbg);
    rpc_xact.dbg = NULL;
  }

  return rs;
}

rw_status_t
rwdts_advise_append_query_proto(rwdts_xact_t*                xact,
                                const rw_keyspec_path_t*     keyspec,
                                rwdts_xact_t*                xact_parent,
                                const ProtobufCMessage*      msg,
                                uint64_t                     corrid,
                                RWDtsDebug*                  dbg,
                                RWDtsQueryAction             action,
                                const rwdts_event_cb_t*      cb,
                                uint32_t                     flags,
                                rwdts_member_registration_t* reg)
{
  RW_ASSERT(xact);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rw_status_t rs;

  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  if (action == RWDTS_QUERY_CREATE || action == RWDTS_QUERY_UPDATE) {
    RW_ASSERT(!rw_keyspec_path_has_wildcards((rw_keyspec_path_t*)keyspec));
  }

  rwdts_xact_block_t* block = rwdts_xact_block_create(xact);

  rs = rwdts_xact_block_add_query_int(block,
                                      (rw_keyspec_path_t *)keyspec,
                                      NULL,
                                      action,
                                      flags,
                                      corrid,
                                      dbg,
                                      msg,
                                      NULL);

  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  rs = rwdts_xact_block_execute(block, flags, cb->cb, cb->ud, NULL);

  if (rs != RW_STATUS_SUCCESS) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }
  return rs;
}


/* It is assumed that all the queries will be of the same action when
 * block-merge flag is used by the client
 * FIXME assert that it is so in add_query or block_execute
 */
bool rwdts_reroot_merge_blocks(RWDtsXactBlockResult* bres, ProtobufCBinaryData *msg_out, rw_keyspec_path_t **ks_out)
{
  int i;
  RW_ASSERT(msg_out);
  RW_ASSERT(ks_out);

  *ks_out = NULL;

  msg_out->len = 0;
  msg_out->data = NULL;

  for (i=0; i < bres->n_result; i++) {
    RWDtsQueryResult *qres = bres->result[i];
    if (rwdts_reroot_merge_queries(qres, msg_out, ks_out) == false) {
      return false;
    }
  }
  return true;
}

bool rwdts_reroot_merge_queries(RWDtsQueryResult* qres, ProtobufCBinaryData *msg_out, rw_keyspec_path_t **ks_out)
{
  int j;
  rw_status_t rs = RW_STATUS_SUCCESS;
  ProtobufCBinaryData msg_in;
  rw_keyspec_path_t *ks_in = NULL;
  RW_ASSERT(msg_out);
  RW_ASSERT(ks_out);
  bool reroot_done = FALSE;

  if (!msg_out->data) {
    msg_in.len = 0;
    msg_in.data = NULL;
    *ks_out = NULL;
  }

  for (j = 0; j < qres->n_result; j++) {
    RWDtsQuerySingleResult *sres = qres->result[j];

    if (sres->result->paybuf.len == 0) {
      continue;
    }

    if (!ks_in && !msg_in.data) {
      msg_in.data = RW_MALLOC0(sres->result->paybuf.len);
      msg_in.len = sres->result->paybuf.len;
      memcpy(msg_in.data, sres->result->paybuf.data, msg_in.len);
      RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t*)sres->path->keyspec, NULL , &ks_in, NULL);
      continue;
    } else if (msg_out->data && msg_out->len) {
      RW_KEYSPEC_PATH_FREE(ks_in, NULL , NULL);
      ks_in = NULL;
      RW_KEYSPEC_PATH_CREATE_DUP(*ks_out, NULL , &ks_in, NULL);
      RW_FREE(msg_in.data);
      msg_in.len = msg_out->len;
      msg_in.data = RW_MALLOC(msg_in.len);
      memcpy(msg_in.data, msg_out->data, msg_in.len);
    }

    if (*ks_out) {
      RW_KEYSPEC_PATH_FREE(*ks_out, NULL , NULL);
      *ks_out = NULL;
    }

    if (msg_out->data) {
      RW_FREE(msg_out->data);
    }

    RW_ASSERT(ks_in);
    RW_ASSERT(msg_in.data);

    msg_out->len = 0;
    msg_out->data = NULL;

    rs = RW_KEYSPEC_PATH_REROOT_AND_MERGE_OPAQUE(NULL, ks_in, (rw_keyspec_path_t*)sres->path->keyspec,
                                                 &msg_in, &sres->result->paybuf,
                                                 ks_out, msg_out, NULL);


    if (rs != RW_STATUS_SUCCESS) {
      reroot_done = false;
      break;
    } else {
      RW_ASSERT(*ks_out);
      reroot_done = true;
    }
  }
  if (ks_in) {
    RW_KEYSPEC_PATH_FREE(ks_in, NULL , NULL);
  }
  if (msg_in.data) {
    RW_FREE(msg_in.data);
  }
  if (!reroot_done) {
    if (*ks_out) {
      RW_KEYSPEC_PATH_FREE(*ks_out, NULL , NULL);
      *ks_out = NULL;
    }
    if (msg_out->data) {
      RW_FREE(msg_out->data);
      msg_out->data = NULL;
      msg_out->len = 0;
    }
  }

  return reroot_done;
}


gboolean rwdts_xact_get_more_results(rwdts_xact_t*  xact)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  /* If xact is done then return FALSE */
  if(xact->bounced || (xact->xres && xact->xres->status >= RWDTS_XACT_COMMITTED)) {
    return FALSE;
  }

  /* Looks like xres holds last XactResult; so checking for more flag here to
   * know abt xact level more flag . Or if local tracking structure has more
   * result then also return true */
  if(rwdts_xact_get_available_results_count(xact,NULL,0) || (xact->xres && xact->xres->has_more && xact->xres->more)) {
    return TRUE;
  }

  return FALSE;
}

gboolean rwdts_xact_block_get_more_results(rwdts_xact_block_t *block)
{
  RW_ASSERT(block);
  rwdts_xact_t *xact = block->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  /* Local tracking structure has more results we can return TRUE */
  if(rwdts_xact_get_available_results_count(xact,block,0)) {
    return TRUE;
  }

  /* This is latched to TRUE when we see a BlockResult without more
     set, ie upon end of results arriving from the router. */
  return !block->responses_done;
}

gboolean rwdts_xact_block_get_query_more_results(rwdts_xact_block_t *block,uint64_t corrid)
{
  RW_ASSERT(block);
  rwdts_xact_t *xact = block->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  /* Local tracking structure has more results we can return TRUE */
  if(rwdts_xact_get_available_results_count(xact,block,corrid)) {
    return TRUE;
  }

  /* Hmm, this doesn't end until the whole block does?  Should track
     end on a per query basis as well so as to be able to do this
     properly. */
  return !block->responses_done;
}

rwdts_query_result_t*
rwdts_qrslt_dup(rwdts_query_result_t* qrslt)
{
  rwdts_query_result_t *dup_qrslt = NULL;

  RW_ASSERT(qrslt);
  if (!qrslt) {
    return NULL;
  }

  dup_qrslt = RW_MALLOC0(sizeof(rwdts_query_result_t));

  if (qrslt->message) {
    dup_qrslt->message = protobuf_c_message_duplicate(NULL,
                                                      qrslt->message, qrslt->message->descriptor);
    RW_ASSERT(dup_qrslt->message);
    if (!dup_qrslt->message) {
      return NULL;
    }
  }

  if (qrslt->keyspec) {
    rw_status_t rs = rw_keyspec_path_create_dup(qrslt->keyspec, NULL, &dup_qrslt->keyspec);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    if (rs != RW_STATUS_SUCCESS) {
      return NULL;
    }
  }

  if (qrslt->key_xpath) {
    dup_qrslt->key_xpath = RW_STRDUP(qrslt->key_xpath);
  }

  dup_qrslt->corrid = qrslt->corrid;
  dup_qrslt->type = qrslt->type;

  return dup_qrslt;
} 
