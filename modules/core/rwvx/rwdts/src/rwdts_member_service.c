
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file   rwdts_member_service.c
 * @author Rajesh Velandy(rajesh.velandy@riftio.com)
 * @date   2014/09/17
 * @brief  Implements the server side functions for the member API
 *
 */
#include <string.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_query_api.h>

void
rwdts_dispatch_pending_query_response(void *ctx);
/*
 * static proto types
 */

const char* rwdts_evtrsp_to_str(RWDtsEventRsp evt) {
  static char str[64];
  switch(evt) {
    case RWDTS_EVTRSP_NULL:
      strcpy(str, "RWDTS_EVTRSP_NULL");
      break;
    case RWDTS_EVTRSP_ASYNC:
      strcpy(str, "RWDTS_EVTRSP_ASYNC");
      break;
    case RWDTS_EVTRSP_NA:
      strcpy(str, "RWDTS_EVTRSP_NA");
      break;
    case RWDTS_EVTRSP_ACK:
      strcpy(str, "RWDTS_EVTRSP_ACK");
      break;
    case RWDTS_EVTRSP_NACK:
      strcpy(str, "RWDTS_EVTRSP_NACK");
      break;
    case RWDTS_EVTRSP_INTERNAL:
      strcpy(str, "RWDTS_EVTRSP_INTERNAL");
      break;
    default:
      snprintf(str,63, "Invalid - %d\n",evt);
      break;
  }
  return str;
}

struct rwdts_tmp_response_s {
  RWDtsXactResult_Closure clo;
  rwdts_xact_t*           xact;
  RWDtsEventRsp           evtrsp;
  bool                    immediate;
  rwmsg_request_t*        rwreq;
};

struct rwdts_tmp_msgreq_s {
  RWDtsMember_Service* mysrv;
  RWDtsXact* input;
  rwdts_xact_t* xact;
  rwdts_api_t* apih;
  RWDtsXactResult_Closure clo;       
  rwmsg_request_t* rwreq;            
};

static void
rwdts_merge_single_responses(RWDtsQueryResult* qres);

static void 
rwdts_member_msg_prepare_f(void *msg_req);

static void 
rwdts_member_msg_pre_commit_f(void *args);

static void
rwdts_member_msg_abort_f(void *args);

static void
rwdts_member_msg_commit_f(void *args);

static void
rwdts_member_match_keyspecs(rwdts_api_t* apih, rwdts_member_registration_t *entry,
                            RWDtsQuery*  query, rw_sklist_t* matches, rw_keyspec_path_t* ks_query,
                            RwSchemaCategory in_category, uint32_t* matchid);

RWDtsEventRsp rwdts_final_rsp_code(rwdts_xact_query_t *xquery)
{
  rwdts_match_info_t *match;

  RWDtsEventRsp ret_rsp = RWDTS_EVTRSP_NA;

  match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
  while (match) {
    RWDtsEventRsp evtrsp;
    if (match->sent_evtrsp != 0) {
      evtrsp = match->sent_evtrsp;
    } else {
      evtrsp = match->evtrsp;
    }
    /* FIXME what in blazes ^^ .  It's ASYNC, until there is a
       terminal code (ACK/NACK) passed in from the application.  Then
       it's that terminal code, once, and we're done.  */

    //// This does not look right . AYSNC or NACK is picked up based on the
    //matchinfo order. the Result code should have priority.
    if (evtrsp == RWDTS_EVTRSP_ASYNC) {
      match->reg->stats.num_async_response++;
      return RWDTS_EVTRSP_ASYNC;
    }
    if (evtrsp == RWDTS_EVTRSP_NACK) {
      match->reg->stats.num_nack_response++;
      return RWDTS_EVTRSP_NACK;
    }

    if (evtrsp == RWDTS_EVTRSP_ACK) {
      match->reg->stats.num_ack_response++;
      ret_rsp = RWDTS_EVTRSP_ACK;
    }
    else if (evtrsp == RWDTS_EVTRSP_INTERNAL) {
      match->reg->stats.num_internal_response++;
      ret_rsp = RWDTS_EVTRSP_INTERNAL;
    }
    else {
      match->reg->stats.num_na_response++;
    }
    match->reg->stats.num_response++;
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }
  //RW_ASSERT(ret_rsp!=RWDTS_EVTRSP_NA);
  return ret_rsp;
}

static bool rwdts_is_getnext(rwdts_xact_query_t *xquery)
{
  rwdts_match_info_t *match;

  match = RW_SKLIST_HEAD(&(xquery->reg_matches), rwdts_match_info_t);
  while (match) {
    if (match->getnext_ptr) {
      return true;
    }
    match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
  }
  return false;
}

static rw_status_t
rwdts_rsp_add_err_and_trace (rwdts_xact_t *xact,
                           RWDtsXactResult *rsp)
{
  RWDtsDebug *dbg =  NULL;
  if(xact->tr || xact->err_report)
  {
    dbg =  (RWDtsDebug*) RW_MALLOC0(sizeof(RWDtsDebug));
    rwdts_debug__init(dbg);
    rsp->dbg = dbg;
  }
  else
  {
    return RW_STATUS_SUCCESS;
  }
  if (xact->tr)
  {
    dbg->tr =
        (RWDtsTraceroute *)protobuf_c_message_duplicate(NULL,
                                                        &xact->tr->base,
                                                        xact->tr->base.descriptor);
  }

  if (xact->err_report)
  {
    dbg->err_report =
        (RWDtsErrorReport *) protobuf_c_message_duplicate(NULL,
                                                          &xact->err_report->base,
                                                          xact->err_report->base.descriptor);
    RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_ErrReportXact, "Updated error report in xact");
    protobuf_c_message_free_unpacked(NULL, &xact->err_report->base);
    xact->err_report = NULL;
  }

  return RW_STATUS_SUCCESS;
}

void rwdts_member_notify_newblock(rwdts_xact_t *xact,
                                  rwdts_xact_block_t *block)
{
  if (HASH_CNT(hh, xact->queries) 
      && (block->newblockadd_notify == RWDTS_NEWBLOCK_TO_NOTIFY)
      && (xact->mbr_state == RWDTS_MEMB_XACT_ST_PREPARE)) {
    xact->n_member_new_blocks++;
    xact->member_new_blocks = 
        RW_REALLOC(xact->member_new_blocks, 
                   sizeof(xact->member_new_blocks[0]) * xact->n_member_new_blocks);
    RW_ASSERT(xact->member_new_blocks);
    xact->member_new_blocks[xact->n_member_new_blocks - 1] = block;
    block->newblockadd_notify = RWDTS_NEWBLOCK_NOTIFY;
    rwdts_xact_block_ref(block, __PRETTY_FUNCTION__, __LINE__);  
  }
}

void rwdts_respond_router_f(void *arg)
{
  struct rwdts_tmp_response_s *t = (struct rwdts_tmp_response_s*)arg;

  RW_ASSERT(t);

  RWDtsXactResult_Closure clo = t->clo;
  rwdts_xact_t*           xact = t->xact;
  RWDtsEventRsp           evtrsp = t->evtrsp;
  rwmsg_request_t*        rwreq = t->rwreq;
  rwdts_api_t*  apih = xact->apih;
  RW_ASSERT(apih);

  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RWDTS_LOG_CODEPOINT(xact, respond_router_f);

  RWDtsXactResult *rsp;
  uint32_t n_queries;

  rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;

  /*
   * Alloc a response structure
   */

  rsp = (RWDtsXactResult*) RW_MALLOC0(sizeof(RWDtsXactResult));
  rwdts_xact_result__init(rsp);
  protobuf_c_message_memcpy(&rsp->id.base, &xact->id.base);
  rsp->has_evtrsp = 1;
  rsp->evtrsp = evtrsp;

  if (xact->member_new_blocks && (xact->mbr_state < RWDTS_MEMB_XACT_ST_PRECOMMIT)) {
    rwdts_xact_block_t *member_new_blocks;
    rsp->n_new_blocks = xact->n_member_new_blocks;
    rsp->new_blocks = (RWDtsXactBlkID**) RW_MALLOC0(sizeof(RWDtsXactBlkID*) * rsp->n_new_blocks);
    int i=0;
    for (; i < rsp->n_new_blocks; i++) {
      member_new_blocks = xact->member_new_blocks[i];
      rsp->new_blocks[i] = (RWDtsXactBlkID*) RW_MALLOC0(sizeof(RWDtsXactBlkID));
      rwdts_xact_blk_id__init((RWDtsXactBlkID *)rsp->new_blocks[i]);
      protobuf_c_message_memcpy(&rsp->new_blocks[i]->base, &(member_new_blocks->subx.block->base));
      member_new_blocks->newblockadd_notify = RWDTS_NEWBLOCK_NOTIFIED;
      rwdts_xact_block_unref(member_new_blocks, __PRETTY_FUNCTION__, __LINE__);
    }
    RW_FREE(xact->member_new_blocks);
    xact->member_new_blocks = NULL;
    xact->n_member_new_blocks = 0;
  }

  if ((xact->mbr_state >= RWDTS_MEMB_XACT_ST_PRECOMMIT) &&
      (xact->mbr_state <= RWDTS_MEMB_XACT_ST_ABORT_RSP)) {
    if (!t->immediate) {
      rsp->evtrsp = xact->evtrsp;
    }
    RWMEMLOG(&xact->rwml_buffer, 
             RWMEMLOG_MEM0, 
             "Respond to router",
             RWMEMLOG_ARG_ENUM_FUNC(rwdts_member_state_to_str, "", (xact->mbr_state)),
             RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (rsp->evtrsp)));
  } else {
    /*
     * Alloc a single block
     */
    rsp->n_result = 1;
    rsp->result = (RWDtsXactBlockResult**) RW_MALLOC0(sizeof(RWDtsXactBlockResult*));
    RWDtsXactBlockResult* bres = rsp->result[0] = (RWDtsXactBlockResult*) RW_MALLOC0(sizeof(RWDtsXactBlockResult));
    rwdts_xact_block_result__init(rsp->result[0]);
    n_queries = HASH_COUNT(xact->queries);
    bres->result = (RWDtsQueryResult**)RW_MALLOC0(sizeof(RWDtsQueryResult) *n_queries);
    int n_queries_this_rwreq = 0;
    HASH_ITER(hh, xact->queries, xquery, qtmp) {
      /* We must respond only to the quer(ies) included in this
         request rwreq.  This code only handles query per req. */

      if (xquery->rwreq == rwreq) {

        n_queries_this_rwreq++;
        RW_ASSERT(n_queries_this_rwreq == 1); /* the bres allocation, among other things, only handles one query per req */

        /*
         * if already responded -- skip the query from the response
         */
        if (xquery->qres) {
          // The send_response call has put results for us to dispatch -- send  them
          RW_ASSERT(xquery->qres->base.descriptor == (const struct ProtobufCMessageDescriptor*)&rwdts_query_result__concrete_descriptor);

          RW_ASSERT(bres->n_result < n_queries);
          if (xquery->query && !(xquery->query->flags & RWDTS_XACT_FLAG_STREAM)
              && (xquery->query->action == RWDTS_QUERY_READ))
          {
            rwdts_merge_single_responses(xquery->qres);
            if (xquery->qres->n_result)
            {
              rwdts_log_payload(&apih->payload_stats,
                                xquery->query->action,
                                RW_DTS_MEMBER_ROLE_MEMBER,
                                RW_DTS_XACT_LEVEL_SINGLERESULT,
                                xquery->qres->result[0]->result->paybuf.len);
            }
          }
          else
          {
            ///Since READ may end up merging
            //Non-default behavior of STREAM or
            //non-READ with results
            int i;
            for (i = 0; i< xquery->qres->n_result;i++)
            {
              if (xquery->qres->result[i]->result)
              {
                rwdts_log_payload(&apih->payload_stats,
                                xquery->query->action,
                                RW_DTS_MEMBER_ROLE_MEMBER,
                                RW_DTS_XACT_LEVEL_SINGLERESULT,
                                xquery->qres->result[i]->result->paybuf.len);
              }
            }
          }

          bres->result[bres->n_result++] = xquery->qres;
          xquery->qres->evtrsp = rwdts_final_rsp_code(xquery);
          xquery->evtrsp = xquery->qres->evtrsp;
          RWMEMLOG(&xact->rwml_buffer, 
                   RWMEMLOG_MEM0, 
                   "Member responding for",
                   RWMEMLOG_ARG_PRINTF_INTPTR("qidx %"PRIdPTR, xquery->query->queryidx),
                   RWMEMLOG_ARG_PRINTF_INTPTR("corrid %"PRIdPTR, xquery->qres->corrid),
                   RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (xquery->qres->evtrsp)),
                   RWMEMLOG_ARG_PRINTF_INTPTR("n_result %"PRIdPTR, xquery->qres->n_result));

          xquery->qres = NULL;
        } else if (!xquery->responded) {
          RW_ASSERT(bres->n_result < n_queries);

          bres->result[bres->n_result] = (RWDtsQueryResult*)RW_MALLOC0(sizeof(RWDtsQueryResult));
          rwdts_query_result__init(bres->result[bres->n_result]);
          bres->result[bres->n_result]->has_evtrsp = 1;
          bres->result[bres->n_result]->evtrsp = (t->immediate?evtrsp:xquery->evtrsp);
          bres->result[bres->n_result]->has_queryidx = xquery->query->has_queryidx;
          bres->result[bres->n_result]->queryidx = xquery->query->queryidx;
          bres->result[bres->n_result]->has_corrid = xquery->query->has_corrid;
          bres->result[bres->n_result]->corrid = xquery->query->corrid;
          RWMEMLOG(&xact->rwml_buffer, 
                   RWMEMLOG_MEM0, 
                   "Member responding for",
                   RWMEMLOG_ARG_PRINTF_INTPTR("qidx %"PRIdPTR, xquery->query->queryidx),
                   RWMEMLOG_ARG_PRINTF_INTPTR("corrid %"PRIdPTR, bres->result[bres->n_result]->corrid),
                   RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (bres->result[bres->n_result]->evtrsp)));
          bres->n_result++;
          apih->stats.num_query_response++;
        }
        xquery->responded = 1;
        RWDTS_API_LOG_XACT_EVENT(xact->apih, xact, RwDtsApiLog_notif_QueryPendingRsp,
                                 "Responded to router ",
                                 xquery->responded, apih->stats.num_query_response);

        switch(xquery->evtrsp) {
          case RWDTS_EVTRSP_ASYNC:
            apih->stats.num_async_response++;
            break;
          case RWDTS_EVTRSP_NA:
            apih->stats.num_na_response++;
            break;
          case RWDTS_EVTRSP_ACK:
            apih->stats.num_ack_response++;
            break;
          case RWDTS_EVTRSP_NACK:
            apih->stats.num_nack_response++;
            break;
          default:
            break;
        }

        /* Do we still have more results for this query */
        if (xquery->evtrsp ==  RWDTS_EVTRSP_ASYNC) {
          xquery->pending_response = 1;
        } else {
          xquery->pending_response = 0;
        }
      }
    }
  }

  /* Forget the rwreq in the xqueries.  Probably better done with just
     xquery->rwreq=NULL up by responded=1? */
  qtmp=NULL;
  xquery=NULL;

  uint32_t q_cnt = HASH_CNT(hh, xact->queries);
  uint32_t end_cnt = 0;
  HASH_ITER(hh, xact->queries, xquery, qtmp) {
    if (xquery->rwreq == rwreq) {
      xquery->rwreq = NULL;
      xquery->clo   = NULL;
    }

    if (xquery->evtrsp != RWDTS_EVTRSP_ASYNC) {
      if (!(xquery->query->flags&RWDTS_XACT_FLAG_NOTRAN) &&
          (xquery->query->action != RWDTS_QUERY_READ)) {
        if (xquery->evtrsp == RWDTS_EVTRSP_NA ) {
          end_cnt++;
        }
      } else {
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_QUERY_RSP, xquery);
      }
    }
  }

  xact->rsp_ct++;
  if(xact->err_report || xact->tr)
  {
    if (xact->tr && getenv("RWMSG_DEBUG"))
    {
      RWDtsTracerouteEnt ent;
      rwdts_traceroute_ent__init(&ent);
      ent.line = __LINE__;
      ent.func = (char*)__PRETTY_FUNCTION__;
      ent.what = RWDTS_TRACEROUTE_WHAT_CODEPOINTS;
      //ent.matchpath = reg->keystr;
      ent.dstpath = apih->client_path;
      if (xact->tr->print) {
        fprintf(stderr, "%s:%u: TYPE:%s, DST:%s, SRC:%s, MATCH-PATH:%s\n", ent.func, ent.line,
                rwdts_print_ent_type(ent.what), ent.dstpath, ent.srcpath, ent.matchpath);
      }
      int i;
      ent.n_evt = xact->n_evt;
      for (i = 0; i <xact->n_evt; i++) ent.evt[i] = xact->evt[i];

      rwdts_dbg_tracert_add_ent(xact->tr, &ent);
    }
    // Put it only for the last send
    rwdts_rsp_add_err_and_trace(xact, rsp);
  }
  rwdts_calc_log_payload(&apih->payload_stats, RW_DTS_MEMBER_ROLE_MEMBER, RW_DTS_XACT_LEVEL_SINGLERESULT, rsp);
  clo(rsp, rwreq);
  protobuf_c_message_free_unpacked(NULL, &rsp->base);
  RW_FREE(t);

  if (q_cnt && xact->rsp_ct >= q_cnt) {
    if (q_cnt == end_cnt) {
      if ((xact->mbr_state != RWDTS_MEMB_XACT_ST_COMMIT) &&
          (xact->mbr_state != RWDTS_MEMB_XACT_ST_ABORT)) { 
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_END, xquery);
      }
    }
  }
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  xact = NULL;

  return;
}

void
rwdts_respond_router(RWDtsXactResult_Closure clo,
                     rwdts_xact_t*           xact,
                     RWDtsEventRsp           evtrsp,
                     rwmsg_request_t*        rwreq,
                     bool                    immediate) {
  struct rwdts_tmp_response_s *t = (struct rwdts_tmp_response_s*) RW_MALLOC0(sizeof(struct rwdts_tmp_response_s));
  t->clo    = clo;
  t->xact   = xact;
  t->evtrsp = evtrsp;
  t->immediate = immediate;
  t->rwreq  = rwreq;
  RW_ASSERT(xact);
  rwdts_api_t *apih = (rwdts_api_t*)xact->apih;
  RW_ASSERT(apih);

  RWMEMLOG(&xact->rwml_buffer, 
           RWMEMLOG_MEM0, 
           "Scheduling Respond to router",
           RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (evtrsp)));

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  if (immediate) {
    rwdts_respond_router_f(t);
    RWDTS_LOG_CODEPOINT(xact, respond_router_immediate);
  } else {
    RWDTS_LOG_CODEPOINT(xact, respond_router);
    rwsched_dispatch_async_f(apih->tasklet,
                             apih->client.rwq,
                             t,
                             rwdts_respond_router_f);
  }
}

static  rw_status_t
rwdts_add_match_info(rw_sklist_t*                 skl,
                     rwdts_member_registration_t* reg,
                     rw_keyspec_path_t*           ks,
                     ProtobufCMessage*            msg,
                     uint32_t                     matchid,
                     RWDtsQuery*                  query,
                     rw_keyspec_path_t*           in_ks)
{
  RW_ASSERT(skl);
  rwdts_match_info_t *match_info  = RW_MALLOC0_TYPE(sizeof(rwdts_match_info_t), rwdts_match_info_t);
  RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
  RW_ASSERT(reg);
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);
  RW_ASSERT(ks);
  rwdts_api_t* apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  match_info->reg     = reg;

  //ATTN: Temporary solution so that the delete/create/update/read all get the same keyspec. Below is change so the create/update get the same as delete/read.
  {
    rw_keyspec_path_t * type_ks = RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(ks, NULL ,
                                                           ((ProtobufCMessage*)(reg->keyspec))->descriptor, NULL);
    if (type_ks) {
      RW_KEYSPEC_PATH_FREE(ks, NULL ,NULL);
      ks = type_ks;
      type_ks = NULL;
    }
    else if (query) {
      RWDTS_MEMBER_SEND_ERROR(NULL, ks, query, apih, NULL, RW_STATUS_FAILURE,
                       RWDTS_ERRSTR_KEY_DUP);
      return RW_STATUS_FAILURE;
    }
  }

  RW_ASSERT((void*)&rw_schema_path_spec__concrete_descriptor!= (void*)((ProtobufCMessage*)ks)->descriptor);
  if (query &&
     (query->action == RWDTS_QUERY_CREATE || query->action == RWDTS_QUERY_UPDATE)) {
    if (rw_keyspec_path_has_wildcards(ks)) {
      RWDTS_MEMBER_SEND_ERROR(NULL, ks, query, apih, NULL, RW_STATUS_FAILURE,
                              RWDTS_ERRSTR_KEY_WILDCARDS);
      return RW_STATUS_FAILURE;
    }
  }

  if (msg && protobuf_c_message_unknown_get_count(msg)) {
    rwdts_report_unknown_fields(ks, apih, msg);
    protobuf_c_message_delete_unknown_all(NULL, msg);
    RW_ASSERT(!protobuf_c_message_unknown_get_count(msg));
  }
  match_info->query   = query;
  match_info->msg     = msg;
  match_info->ks      = ks;
  match_info->in_ks   = in_ks;
  match_info->matchid = matchid;

  RW_SKLIST_INSERT(skl, match_info);
  rwdts_member_reg_handle_ref((rwdts_member_reg_handle_t)reg);

  /* Check if this registration has an ongoing write transaction */

  if (RWDTS_AUDIT_IS_IN_PROGRESS(&reg->audit) && query && query->action != RWDTS_QUERY_READ) {
    char tmp_buf[128] = "";
    char *ks_str = NULL;
    // ATTN this should be WARRN or less - for now log a CRIT so that
    // we have some traces on what is going on during audit recovery.
    rw_keyspec_path_get_new_print_buffer(reg->keyspec, NULL , NULL, &ks_str);

    RWTRACE_INFO(reg->apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: Received query with action = %s while audit is in progress for regn id[%u] reg_ks[%s]!\n",
                 reg->apih->client_path,
                 rwdts_query_action_to_str(query->action, tmp_buf, sizeof(tmp_buf)),
                 reg->reg_id,
                 ks_str);
    rwdts_member_reg_audit_invalidate_fetch_results(reg);
    RW_FREE(ks_str);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_find_matches_for_keyspec(rwdts_api_t*             apih,
                                      const rw_keyspec_path_t* keyspec,
                                      rw_sklist_t*             matches,
                                      uint32_t                 flags)
{
  rw_keyspec_path_t*            matchks           = NULL;
  const  rw_yang_pb_schema_t*   ks_schema         = NULL;
  RwSchemaCategory              in_category;
  RwSchemaCategory              reg_category;
  rw_status_t                   rs;
  rw_status_t                   retval            = RW_STATUS_FAILURE;
  uint32_t                      matchid           = 0;
  rwdts_member_registration_t*  entry = NULL;

  RW_ASSERT(keyspec);
  in_category = rw_keyspec_path_get_category(keyspec);
  // check the registrations to see if there is a detailed match
  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (entry) {
    reg_category = rw_keyspec_path_get_category(keyspec);

    /* If the categories do not match  skip */
    if ((in_category != reg_category) &&
        (reg_category != RW_SCHEMA_CATEGORY_ANY) &&
        (in_category != RW_SCHEMA_CATEGORY_ANY)) {
      entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
      continue;
    }

    // Get the schema for this registration
    ks_schema = ((ProtobufCMessage*)entry->keyspec)->descriptor->ypbc_mdesc->module->schema?
                ((ProtobufCMessage*)entry->keyspec)->descriptor->ypbc_mdesc->module->schema:rwdts_api_get_ypbc_schema(apih);

    RW_ASSERT(ks_schema);

    /*
     * if the registration and request doesn't match skip this entry
     */

    if (!(flags & entry->flags & (RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_PUBLISHER))) {
      entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
      continue;
    }

    if (RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , keyspec, NULL)) {
      /* Both the input keyspecs are not modified.
       * The result of the merge is in the returned keyspec.
       */
      matchks = rw_keyspec_path_merge(NULL, keyspec, entry->keyspec);
      RW_ASSERT(matchks);

      /* Convert the generic keyspec to concrete */
      rw_keyspec_path_t *spec_matchks =
          RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(matchks,
                                             NULL,
                                             ((ProtobufCMessage*)
                                              (entry->keyspec))->descriptor,
                                             NULL);
      RW_ASSERT(spec_matchks);
      RW_KEYSPEC_PATH_FREE(matchks, NULL, NULL);
      matchks = spec_matchks;
      rs = rwdts_add_match_info(matches, entry, matchks, NULL, ++matchid, NULL, NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      matchks = NULL;
      retval = RW_STATUS_SUCCESS;
    }
    entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
  }
  return retval;
}

rw_status_t
rwdts_member_find_matches(rwdts_api_t* apih,
                          RWDtsQuery*  query,
                          rw_sklist_t* matches)
{
  rw_keyspec_path_t*            ks_query          = NULL;
  RWDtsQueryAction              action;
  RwSchemaCategory              in_category;
  uint32_t                      matchid           = 0;
  rw_status_t                   retval            = RW_STATUS_FAILURE;

  RW_ASSERT(query);
  RW_ASSERT(query->key);
  RW_ASSERT(query->key->ktype == RWDTS_KEY_RWKEYSPEC); // The only key we support is keyspec for now

  // If the registrations are empty nothing to go forward with
  if (!RW_SKLIST_LENGTH(&(apih->reg_list))) {
    RWTRACE_WARN(apih->rwtrace_instance,
                 RWTRACE_CATEGORY_RWTASKLET,
                 "%s: No registrations in apih!", apih->client_path);
    RWDTS_MEMBER_SEND_ERROR(NULL, NULL, query, apih, NULL,
                            RW_STATUS_FAILURE,
                            "No registrations in apih");
    goto done;
  }

  RW_ASSERT(matches);

  /* convert the binpath to keyspec */
  ks_query = RW_KEYSPEC_PATH_CREATE_WITH_BINPATH_BUF(NULL, query->key->keybuf.len,
                                                query->key->keybuf.data, NULL);
  if (!ks_query) {
    RWDTS_MEMBER_SEND_ERROR(NULL, NULL, query, apih, NULL,
                            RW_STATUS_FAILURE,
                            RWDTS_ERRSTR_KEY_BINPATH);
    goto done;
  }

  /*
   * Search for complete matches - The keyspec registered and the keyspec in the
   * query are at the same level
   */
  in_category = rw_keyspec_path_get_category(ks_query);

  // Get Query action
  action = query->action;
  RW_ASSERT(action != RWDTS_QUERY_INVALID);
  if (action == RWDTS_QUERY_CREATE || action == RWDTS_QUERY_UPDATE) {
    if (rw_keyspec_path_has_wildcards(ks_query)) {
      RWDTS_MEMBER_SEND_ERROR(NULL, NULL, query, apih, NULL,
                              RW_STATUS_FAILURE,
                              RWDTS_ERRSTR_KEY_WILDCARDS);
      goto done;
    }
  }

  rwdts_member_registration_t *entry = NULL;
  rwdts_shard_chunk_info_t **chunks = NULL; 
  uint32_t isadvise = ((query->flags & RWDTS_XACT_FLAG_ADVISE) || (query->flags & RWDTS_XACT_FLAG_SUB_READ));
  rwdts_shard_t *head = apih->rootshard;
  int j;
  uint32_t n_chunks = 0;
  chunks = NULL;
  rwdts_shard_match_pathelm_recur(ks_query, head, &chunks, &n_chunks, NULL);

  for (j = 0; j < n_chunks; j++) {
    if (isadvise) {
      if (chunks[j]->elems.member_info.n_sub_reg) {
        rwdts_chunk_member_info_t *mbr_info= NULL, *tmp_mbr_info = NULL;
        HASH_ITER(hh_mbr_record, chunks[j]->elems.member_info.sub_mbr_info, mbr_info, tmp_mbr_info) { //TBD other flavors
          if (mbr_info->flags & RWDTS_FLAG_SUBSCRIBER) { 
            entry = (rwdts_member_registration_t *) mbr_info->member;
            rwdts_member_match_keyspecs(apih, entry, query, matches, ks_query, in_category, &matchid);
          }
        }
      } 
    }
    else {
      if (chunks[j]->elems.member_info.n_pub_reg) {
        rwdts_chunk_member_info_t *mbr_info= NULL, *tmp_mbr_info = NULL;
        HASH_ITER(hh_mbr_record, chunks[j]->elems.member_info.pub_mbr_info, mbr_info, tmp_mbr_info) { // TBD other flavors
          if (mbr_info->flags & RWDTS_FLAG_PUBLISHER) { 
            entry = (rwdts_member_registration_t *) mbr_info->member;
            rwdts_member_match_keyspecs(apih, entry, query, matches, ks_query, in_category, &matchid);
          }
        }
      }
    }
  }
  if (chunks) {
    free(chunks);
    chunks = NULL;
  }

  retval = RW_STATUS_SUCCESS;

done:
  if (ks_query) {
    RW_KEYSPEC_PATH_FREE(ks_query, NULL ,NULL);
  }
  return retval;
}

/*
 *  A helper iterator funcion
 *  This function iterates through all the queries in all the blocks in the passed transaction
 *  and executes the passed callback with the passed event.
 */


static void rwdts_member_msg_prepare(RWDtsMember_Service *mysrv,
                                     const RWDtsXact *input,
                                     void *self,
                                     RWDtsXactResult_Closure clo,
                                     rwmsg_request_t *rwreq)
{
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDtsXact *dup_input = (RWDtsXact *)protobuf_c_message_duplicate(NULL,
                                                      (const struct ProtobufCMessage *)&input->base,
                                                      input->base.descriptor);
 
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)RW_MALLOC0(sizeof(struct rwdts_tmp_msgreq_s));

  msg_req->mysrv = mysrv;
  msg_req->apih = apih;
  msg_req->clo = clo;
  msg_req->rwreq = rwreq;
  msg_req->input = dup_input;

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           msg_req,
                           rwdts_member_msg_prepare_f);

  return;
}

static void rwdts_member_msg_prepare_f(void *arg)
{
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)arg;

  RW_ASSERT(msg_req);

  rwdts_api_t *apih = msg_req->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWDtsEventRsp evtrsp =  RWDTS_EVTRSP_ACK;

  rwmsg_request_t *rwreq = msg_req->rwreq;
  RWDtsXact *input = msg_req->input;
  RWDtsXactResult_Closure clo = msg_req->clo;

  RW_FREE(msg_req);

  int i,j ;

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, PREPARE_RCVD, "Member received prepare", input->n_subx);

  apih->stats.num_prepare_frm_rtr++;

  rwdts_xact_t *xact = rwdts_member_xact_init(apih, input);
  RW_ASSERT(xact);

  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            rwreq, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Prepare)");

  if (input->dbg && (input->dbg->dbgopts1&RWDTS_DEBUG_TRACEROUTE)) {
    if (!xact->tr) {
      xact->tr = RW_MALLOC0(sizeof(RWDtsTraceroute));
      rwdts_traceroute__init(xact->tr);
    }
    if (input->dbg->tr) {
      xact->tr->has_print = input->dbg->tr->has_print;
      xact->tr->print = input->dbg->tr->print;
      xact->tr->has_break_start = input->dbg->tr->has_break_start;
      xact->tr->break_start = input->dbg->tr->break_start;
      xact->tr->has_break_prepare = input->dbg->tr->has_break_prepare;
      xact->tr->break_prepare = input->dbg->tr->break_prepare;
      xact->tr->has_break_end = input->dbg->tr->has_break_end;
      xact->tr->break_end = input->dbg->tr->break_end;
    }
  }

  // ATTN TODO -- if this is a subtransaction for which this member is not participating
  // Create an empty top level transaction so that, we can issue reads against that transaction.

  RW_ASSERT(input->n_subx <= 1); /* respond_router, et al, only handle 1 block */
  for (i = 0; i < input->n_subx; i++) {
    RWDtsXactBlock *block = input->subx[i];
    RW_ASSERT(block->n_query <= 1); /* respond_router, et al, only handle 1 query */
    for (j = 0; j < block->n_query; j++) {
      RWDtsQuery *query = block->query[j];
      rwdts_xact_query_t *xquery = NULL;

      if ((query->action == RWDTS_QUERY_CREATE) ||
          (query->action == RWDTS_QUERY_UPDATE)) {
        RW_ASSERT(query->payload);
      }
      rwdts_log_payload(&apih->payload_stats, query->action, RW_DTS_MEMBER_ROLE_MEMBER, RW_DTS_XACT_LEVEL_QUERY, (query->payload)?query->payload->paybuf.len:0);

      // Log the query
      char *ks_str = NULL;
      rw_keyspec_path_t *ks_query = RW_KEYSPEC_PATH_CREATE_WITH_BINPATH_BUF(NULL, query->key->keybuf.len, query->key->keybuf.data, NULL);
      if (ks_query) {
        rw_keyspec_path_get_new_print_buffer(ks_query, NULL , NULL, &ks_str);
        RW_KEYSPEC_PATH_FREE(ks_query, NULL ,NULL);
        RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, QUERY_RCVD,
                                       "Member received query", query->queryidx,
                                       ks_str ? ks_str : "");
        free(ks_str);
      }
      else {
        RWDTS_XACT_ABORT(xact, RW_STATUS_FAILURE, RWDTS_ERRSTR_KEY_BINPATH);
        protobuf_c_message_free_unpacked(NULL, (struct ProtobufCMessage *)&input->base);
        return;
      }

      rw_status_t rs = rwdts_xact_add_query_internal(xact, query, &xquery);

      RW_ASSERT(xquery);
      xquery->rwreq = rwreq;        /* note this in xquery for rwreq-vs-xquery correlation in rwdts_respond_router */

      if (rs == RW_STATUS_DUPLICATE)
      {
        //No Duplicates without pending RSP

        xquery->responded = 0;
        if (xquery->xact_rsp_pending)
        {
          // Dispatch async..
          rwdts_dispatch_pending_query_response(xquery->xact_rsp_pending);
          if (rwdts_final_rsp_code(xquery) != RWDTS_EVTRSP_ASYNC) {

            /* FIXME wut.  This use of evtrsp for special-purpose status as we juggle temporarily pending responses is nuts */

            evtrsp = RWDTS_EVTRSP_ACK;
          }
        }
        else if(!rwdts_is_getnext(xquery))
        {
          // this needs to wait on a timer for more responses!  just wait instead?
          xquery->evtrsp = RWDTS_EVTRSP_ASYNC;
          evtrsp = RWDTS_EVTRSP_ASYNC;

          //?? BUG add timer, respond after 10-20 second of being idle so the router can reissue
          // return here????  //?? //??
          protobuf_c_message_free_unpacked(NULL, (struct ProtobufCMessage *)&input->base);
          return;

        }
        else
        {
          //run with prepare with getnext ptr
          rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_PREPARE, xquery);
        }
      }
      else
      {
        rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_PREPARE, xquery);
        xquery->evtrsp = rwdts_final_rsp_code(xquery);
              if (xquery->evtrsp == RWDTS_EVTRSP_ASYNC) {
                RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, QueryRspAsync, "member prepare, RWDTS_EVTRSP_ASYNC, returning for now");
                //?? BUG add timer, respond after 10-20 second of being idle so the router can reissue
          xquery->clo = clo;
          RWMEMLOG(&xact->rwml_buffer, 
                   RWMEMLOG_MEM0, 
                   "Member prepare ASYNC",
                   RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (xquery->evtrsp)));
          protobuf_c_message_free_unpacked(NULL, (struct ProtobufCMessage *)&input->base);
          return;
              }
      }
    }
  }
  rwdts_respond_router(clo, xact, evtrsp, rwreq, false);
  protobuf_c_message_free_unpacked(NULL, (struct ProtobufCMessage *)&input->base);
}

static void rwdts_member_msg_sub_commit(RWDtsMember_Service *mysrv,
                                        const RWDtsXact *input,
                                        void *self,
                                        RWDtsXactResult_Closure clo,
                                        rwmsg_request_t *rwreq) {
  // ATTN -- not supported yet
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  apih->stats.num_sub_commit_frm_rtr++;
}

static void rwdts_member_msg_sub_abort(RWDtsMember_Service *mysrv,
                                       const RWDtsXact *input,
                                       void *self,
                                       RWDtsXactResult_Closure clo,
                                       rwmsg_request_t *rwreq) {

  // ATTN -- not supported yet
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  apih->stats.num_sub_abort_frm_rtr++;
  return;
}
static void rwdts_member_msg_pre_commit(RWDtsMember_Service *mysrv,
                                        const RWDtsXact *input,
                                        void *self,
                                        RWDtsXactResult_Closure clo,
                                        rwmsg_request_t *rwreq) {
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(input);
  RW_ASSERT(input->n_subx == 0);

  rwdts_xact_t *xact =  rwdts_lookup_xact_by_id(apih, &input->id);
  RW_ASSERT(xact);

  if (!xact) {
    RWDTS_API_LOG_XACT_EVENT(apih, input, RwDtsApiLog_notif_XactNotFound, "Member pre-commit - no matching xact found");
    rwdts_respond_router(clo, xact, RWDTS_EVTRSP_NACK, rwreq, true);
    return;
  }

  apih->stats.num_pre_commit_frm_rtr++;

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)RW_MALLOC0(sizeof(struct rwdts_tmp_msgreq_s));

  msg_req->mysrv = mysrv;
  msg_req->apih = apih;
  msg_req->clo = clo;
  msg_req->rwreq = rwreq;
  msg_req->xact = xact;

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           msg_req,
                           rwdts_member_msg_pre_commit_f);
}

static void rwdts_member_msg_pre_commit_f(void *args)
{
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)args;
  RW_ASSERT(msg_req);

  rwdts_api_t *apih = msg_req->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_xact_t *xact = msg_req->xact;
  RW_ASSERT(xact);

  RWDtsXactResult_Closure clo = msg_req->clo;
  rwmsg_request_t *rwreq = msg_req->rwreq;

  RW_FREE(msg_req);

  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            rwreq, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Precommit)");

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, PRECOMMIT_RCVD, "Member received pre-commit");
  rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_PRECOMMIT, xact);

  rwdts_respond_router(clo, xact, RWDTS_EVTRSP_ACK, rwreq, false);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return;
}

static void rwdts_member_msg_abort(RWDtsMember_Service *mysrv,
                                   const RWDtsXact *input,
                                   void *self,
                                   RWDtsXactResult_Closure clo,
                                   rwmsg_request_t *rwreq)
{
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(input);
  RW_ASSERT(input->n_subx == 0);

  apih->stats.num_abort_frm_rtr++;
  rwdts_xact_t *xact =  rwdts_lookup_xact_by_id(apih, &input->id);
  RW_ASSERT(xact);

  apih->stats.num_pre_commit_frm_rtr++;

  if (!xact) {
    RWDTS_API_LOG_XACT_EVENT(apih, input, RwDtsApiLog_notif_XactNotFound, "Member abort - no matching xact found");
    // FIXME : xact is null here and will assert in the function below
    rwdts_respond_router(clo, xact, RWDTS_EVTRSP_NACK, rwreq, true);
    return;
  }

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)RW_MALLOC0(sizeof(struct rwdts_tmp_msgreq_s));

  msg_req->mysrv = mysrv;
  msg_req->apih = apih;
  msg_req->clo = clo;
  msg_req->rwreq = rwreq;
  msg_req->xact = xact;

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           msg_req,
                           rwdts_member_msg_abort_f);
}

static void rwdts_member_msg_abort_f(void *args)
{
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)args;
  RW_ASSERT(msg_req);

  rwdts_api_t *apih = msg_req->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_xact_t *xact = msg_req->xact;
  RW_ASSERT(xact);  

  RWDtsXactResult_Closure clo = msg_req->clo;
  rwmsg_request_t *rwreq = msg_req->rwreq;
  
  RW_FREE(msg_req);

  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            rwreq, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Abort)");

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, ABORT_RCVD, "Member received abort");
  rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_ABORT, xact);

  rwdts_respond_router(clo, xact, RWDTS_EVTRSP_ACK, rwreq, true);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return;
}

/* Skeletal code - TBD Need to be tested */
static void rwdts_member_msg_commit(RWDtsMember_Service *mysrv,
                                    const RWDtsXact *input,
                                    void *self,
                                    RWDtsXactResult_Closure clo,
                                    rwmsg_request_t *rwreq)
{
  rwdts_api_t *apih = (rwdts_api_t *)self;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(input);
  RW_ASSERT(input->n_subx == 0);

  apih->stats.num_commit_frm_rtr++;

  rwdts_xact_t *xact =  rwdts_lookup_xact_by_id(apih, &input->id);

  if (!xact) {
    RWDTS_API_LOG_XACT_EVENT(apih, input, RwDtsApiLog_notif_XactNotFound, "Member commit - no matching xact found");
    // FIXME : xact is null here and will assert in the function below
    rwdts_respond_router(clo, xact, RWDTS_EVTRSP_NACK, rwreq, true);
    return;
  }

  rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)RW_MALLOC0(sizeof(struct rwdts_tmp_msgreq_s));

  msg_req->mysrv = mysrv;
  msg_req->apih = apih;
  msg_req->clo = clo;
  msg_req->rwreq = rwreq;
  msg_req->xact = xact;

  rwsched_dispatch_async_f(apih->tasklet,
                           apih->client.rwq,
                           msg_req,
                           rwdts_member_msg_commit_f);
}

static void rwdts_member_msg_commit_f(void *args)
{
  struct rwdts_tmp_msgreq_s *msg_req = (struct rwdts_tmp_msgreq_s *)args;
  RW_ASSERT(msg_req);

  rwdts_api_t *apih = msg_req->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_xact_t *xact = msg_req->xact;
  RW_ASSERT(xact);

  RWDtsXactResult_Closure clo = msg_req->clo;
  rwmsg_request_t *rwreq = msg_req->rwreq;

  RW_FREE(msg_req);

  rwmsg_request_memlog_hdr (&xact->rwml_buffer, 
                            rwreq, 
                            __PRETTY_FUNCTION__,
                            __LINE__,
                            "(Commit)");

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, COMMIT_RCVD, "Member received commit");
  rwdts_member_xact_run(xact, RWDTS_MEMB_XACT_EVT_COMMIT, xact);

  rwdts_respond_router(clo, xact, RWDTS_EVTRSP_ACK, rwreq, true);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return;
}


rw_status_t rwdts_member_service_init(struct rwdts_api_s *apih)
{
  RW_ASSERT(apih);
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDTS_MEMBER__INITSERVER(&apih->server.service, rwdts_member_msg_);

  return RW_STATUS_SUCCESS;
}

rw_status_t rwdts_member_service_deinit(struct rwdts_api_s *apih) {
  // Does notthing for now
  return RW_STATUS_SUCCESS;
}

RWDtsQueryAction
rwdts_member_get_query_action(rwdts_query_handle_t queryh) {
  rwdts_match_info_t *match = (rwdts_match_info_t*)queryh;

  RW_ASSERT_TYPE(match, rwdts_match_info_t);
  RW_ASSERT(match->query);
  RW_ASSERT(match->query->action != RWDTS_QUERY_INVALID);
  return match->query->action;
}

uint32_t
rwdts_member_get_query_flags(rwdts_query_handle_t queryh) {
  rwdts_match_info_t *match = (rwdts_match_info_t*)queryh;
  RW_ASSERT_TYPE(match, rwdts_match_info_t);

  RW_ASSERT(match->query);

  return match->query->flags;
}

static void
rwdts_merge_single_responses(RWDtsQueryResult *qres)
{

  rw_keyspec_path_t *ks = NULL;
  int i;
  ProtobufCBinaryData msg_out;
  msg_out.len = 0;
  msg_out.data = NULL;

  if (rwdts_reroot_merge_queries(qres, &msg_out, &ks)) {
    for (i = 0; i < qres->n_result; i++) {
      rwdts_query_single_result__free_unpacked(NULL, qres->result[i]);
    }
    if (qres->n_result && qres->result) {
      RW_FREE(qres->result);
    }
    qres->n_result = 1;
    qres->result = RW_MALLOC0(sizeof(RWDtsQuerySingleResult*));
    RWDtsQuerySingleResult* sres = qres->result[0] = RW_MALLOC0(sizeof(RWDtsQuerySingleResult));
    rwdts_query_single_result__init(sres);
    RWDtsKey* path = sres->path = RW_MALLOC0(sizeof(RWDtsKey));
    rwdts_key__init(path);
    path->keyspec = (RwSchemaPathSpec *)ks;
    path->has_keybuf = true;
    rw_status_t rs = RW_KEYSPEC_PATH_SERIALIZE_DOMPATH(ks, NULL, &path->keybuf, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RWDtsPayload* result = sres->result = RW_MALLOC0(sizeof(RWDtsPayload));
    rwdts_payload__init(result);
    result->has_paybuf = TRUE;
    result->ptype = RWDTS_PAYLOAD_PBRAW;
    result->paybuf.len = msg_out.len;
    result->paybuf.data = msg_out.data;
  }
  return;
}

static void
rwdts_member_match_read(rwdts_member_registration_t *entry, RWDtsQuery*  query,
                        rw_keyspec_path_t* ks_query, const rw_yang_pb_schema_t* ks_schema,
                        rw_sklist_t* matches, uint32_t* matchid)
{

  ProtobufCMessage* matchmsg = NULL;
  rw_keyspec_path_t* matchks = NULL;
  size_t depth_i = rw_keyspec_path_get_depth(ks_query);
  size_t depth_o = rw_keyspec_path_get_depth(entry->keyspec);
  rw_status_t rs;

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , ks_query, NULL)) {
    return;
  }

  if (query->flags & RWDTS_XACT_FLAG_DEPTH_LISTS) {
    if (!(((depth_i+1) == depth_o) || (depth_i == depth_o))){
      return;
    }
  }

  if (query->flags & RWDTS_XACT_FLAG_DEPTH_ONE) {
    if (depth_o != depth_i) {
      return;
    }
  }

  if (depth_i >= depth_o){
    rs = rw_keyspec_path_trunc_suffix_n(ks_query, NULL, depth_o);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    matchmsg = RW_KEYSPEC_PATH_SCHEMA_REROOT (ks_query, NULL, ks_schema, entry->keyspec, NULL);
  }else{
    if (query->payload) {
      matchmsg = protobuf_c_message_unpack(NULL,
                                           entry->desc,
                                           query->payload->paybuf.len,
                                           query->payload->paybuf.data);
    }
  }

  /* Both the input keyspecs are not modified.
   * The result of the merge is in the returned keyspec.
   */
  matchks = rw_keyspec_path_merge(NULL, ks_query, entry->keyspec);
  RW_ASSERT(matchks);
  // delete the binpath so that we don't have stale binpaths
  rw_keyspec_path_delete_binpath(matchks, &entry->apih->ksi);

  /* Convert the generic keyspec to concrete */
  rw_keyspec_path_t *spec_matchks =
      RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(matchks,
                                         NULL,
                                         ((ProtobufCMessage*)
                                         (entry->keyspec))->descriptor,
                                         NULL);
  RW_ASSERT(spec_matchks);
  RW_KEYSPEC_PATH_FREE(matchks, NULL, NULL);
  matchks = spec_matchks;

  rs = rwdts_add_match_info(matches, entry, matchks, matchmsg, ++(*matchid), query, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  return;
}

static void
rwdts_member_match_delete(rwdts_member_registration_t *entry, RWDtsQuery*  query,
                          rw_keyspec_path_t* ks_query, rw_sklist_t* matches,
                          uint32_t* matchid)
{
  ProtobufCMessage* matchmsg = NULL;
  rw_keyspec_path_t* matchks = NULL;
  size_t depth_i = rw_keyspec_path_get_depth(ks_query);
  size_t depth_o = rw_keyspec_path_get_depth(entry->keyspec);
  rw_status_t rs;

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , ks_query, NULL)){
    return;
  }

  if (depth_i == depth_o) {
    if (query->payload && query->payload->ptype == RWDTS_PAYLOAD_PBRAW) {
      matchmsg = protobuf_c_message_unpack(NULL,
                                           entry->desc,
                                           query->payload->paybuf.len,
                                           query->payload->paybuf.data);
    }

    if (rw_keyspec_path_has_wildcards(ks_query)) {
      rs = RW_KEYSPEC_PATH_CREATE_DUP(entry->keyspec, NULL ,  &matchks, NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      RW_ASSERT(matchks != NULL);
      rw_keyspec_path_t* in_ks = NULL;
      rs = RW_KEYSPEC_PATH_CREATE_DUP(ks_query, NULL , &in_ks, NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      RW_ASSERT(in_ks != NULL);
      rs = rwdts_add_match_info(matches, entry, matchks, matchmsg, ++(*matchid), query, in_ks);
    } else {
      rs = RW_KEYSPEC_PATH_CREATE_DUP(ks_query, NULL ,  &matchks, NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      RW_ASSERT(matchks != NULL);
      rs = rwdts_add_match_info(matches, entry, matchks, matchmsg, ++(*matchid), query, NULL);
    }
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  } else {
    rw_keyspec_path_t* in_ks = NULL;
    rs = RW_KEYSPEC_PATH_CREATE_DUP(ks_query, NULL , &in_ks, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_ASSERT(in_ks != NULL);
    /* Convert the received keyspec to reg keyspec level */
     matchks =
      RW_KEYSPEC_PATH_CREATE_DUP_OF_TYPE(ks_query,
                                         NULL,
                                         ((ProtobufCMessage*)
                                         (entry->keyspec))->descriptor,
                                         NULL);
    RW_ASSERT(matchks != NULL);
    rs = rwdts_add_match_info(matches, entry, matchks, matchmsg, ++(*matchid), query, in_ks);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  return;
}

static void
rwdts_member_match_create_update(rwdts_member_registration_t *entry, RWDtsQuery*  query,
                                 rw_keyspec_path_t* ks_query, const rw_yang_pb_schema_t* ks_schema,
                                 rw_sklist_t* matches, uint32_t* matchid)
{

  ProtobufCMessage* matchmsg = NULL;
  rw_keyspec_path_t* matchks = NULL;
  size_t index = 0;
  rw_yang_pb_msgdesc_t* result = NULL;
  rw_status_t rs;

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , ks_query, NULL)) {
    return;
  }

  // Do we have a direct match - if so add it to the matched list
  if (RW_KEYSPEC_PATH_IS_MATCH_DETAIL(NULL, entry->keyspec, ks_query, &index, NULL, NULL)) {
    if (query->payload && query->payload->ptype == RWDTS_PAYLOAD_PBRAW) {
      matchmsg = protobuf_c_message_unpack(NULL,
                                           entry->desc,
                                           query->payload->paybuf.len,
                                           query->payload->paybuf.data);
    }
    // Create a duplicate of the query keyspec to ppass to the caller
    // ks_quer is freed before the function exits
    rs = RW_KEYSPEC_PATH_CREATE_DUP(ks_query, NULL , &matchks, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    rs = rwdts_add_match_info(matches, entry, matchks, matchmsg, ++(*matchid), query, NULL);
    // This should not fail -- We cannot  have a duplicate regn  here
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    // Consumed matchks and matchmsg
    matchks = NULL;
    matchmsg = NULL;
  } else {
    rw_keyspec_path_t*           local_keyspec = NULL;
    ProtobufCMessage*       trans_msg     = NULL;

    size_t depth_i = rw_keyspec_path_get_depth(ks_query);
    size_t depth_o = rw_keyspec_path_get_depth(entry->keyspec);

    if ((entry->flags & RWDTS_FLAG_DEPTH_LISTS) &&
        (entry->flags & RWDTS_FLAG_SUBSCRIBER)) {
      if (RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , ks_query, NULL)) {
        if (!(((depth_i+1) == depth_o) || (depth_i == depth_o))) {
          return;
        }
      }
    }

    if ((entry->flags & RWDTS_FLAG_DEPTH_ONE) &&
        (entry->flags & RWDTS_FLAG_SUBSCRIBER)) {
      if (RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL, entry->keyspec , ks_query, NULL)) {
        if (depth_o != depth_i) {
          return;
        }
      }
    }

    rw_keyspec_path_find_msg_desc_schema(ks_query,
                                         NULL,
                                         ks_schema,
                                         (const rw_yang_pb_msgdesc_t **)&result,
                                         &local_keyspec);

    if (local_keyspec != NULL) {
      RW_KEYSPEC_PATH_FREE(local_keyspec, NULL ,NULL);
      local_keyspec = NULL;
      return;
    }

    if (result && query->payload) {
      trans_msg = protobuf_c_message_unpack(NULL,
                                            result->u->msg_msgdesc.pbc_mdesc,
                                            query->payload->paybuf.len,
                                            query->payload->paybuf.data);
    }
    if (trans_msg) {
      if (depth_i <= depth_o) {
        rw_keyspec_path_reroot_iter_state_t state;
        rw_keyspec_path_reroot_iter_init(ks_query,
          &entry->apih->ksi,
          &state, trans_msg, entry->keyspec);

        while (rw_keyspec_path_reroot_iter_next(&state)) {
          matchmsg = rw_keyspec_path_reroot_iter_take_msg(&state);
          if (matchmsg)  {
            matchks = rw_keyspec_path_reroot_iter_take_ks(&state);
            RW_ASSERT(matchks);
            RW_ASSERT(matchks != ks_query);
            RW_ASSERT(matchks != entry->keyspec);
            RW_ASSERT(!rw_keyspec_path_has_wildcards(matchks));
            RW_ASSERT(matchmsg != trans_msg);
            RW_ASSERT(!rw_keyspec_path_has_wildcards(matchks));
            rs = rwdts_add_match_info(matches, entry, matchks , matchmsg, ++(*matchid), query, NULL);
            RW_ASSERT(rs == RW_STATUS_SUCCESS);
            matchks = NULL; matchmsg = NULL;
          }
        }
        rw_keyspec_path_reroot_iter_done(&state);
      } else {
        matchmsg = RW_KEYSPEC_PATH_REROOT(ks_query, NULL, trans_msg, entry->keyspec, NULL);
        RW_ASSERT(matchmsg != trans_msg);
        if (matchmsg) {
          rs = RW_KEYSPEC_PATH_CREATE_DUP(ks_query, NULL, &matchks, NULL);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          rs = RW_KEYSPEC_PATH_TRUNC_SUFFIX_N(matchks, NULL, rw_keyspec_path_get_depth(entry->keyspec), NULL);
          RW_ASSERT(rs == RW_STATUS_SUCCESS && matchks);
          RW_ASSERT(!rw_keyspec_path_has_wildcards(matchks));
          rs = rwdts_add_match_info(matches, entry, matchks , matchmsg, ++(*matchid), query, NULL);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          matchks = NULL; matchmsg = NULL;
        }
      }
    }
    if (trans_msg && trans_msg != matchmsg) {
      protobuf_c_message_free_unpacked(&protobuf_c_default_instance, trans_msg);
      trans_msg = NULL;
    }
  }
  return;
}

static void
rwdts_member_match_keyspecs(rwdts_api_t* apih, rwdts_member_registration_t *entry,
                            RWDtsQuery*  query, rw_sklist_t* matches, rw_keyspec_path_t* ks_query,
                            RwSchemaCategory in_category, uint32_t* matchid)
{

  RwSchemaCategory  reg_category = rw_keyspec_path_get_category(entry->keyspec);;
  const rw_yang_pb_schema_t* ks_schema = NULL;
  char *reg_ks_str = NULL;

  /* If the categories do not match  skip */
  if ((in_category != reg_category) &&
      (reg_category != RW_SCHEMA_CATEGORY_ANY) &&
      (in_category != RW_SCHEMA_CATEGORY_ANY)) {
    return;
  }

  /*
   * if the registration and request doesn't match skp this entry
   */

  if (entry->reg_state == RWDTS_REG_DEL_PENDING) {
    goto done;
  }

  if (entry->flags & RWDTS_FLAG_SUBSCRIBER) {
    if (!(query->flags & RWDTS_XACT_FLAG_ADVISE)
        && !(query->flags & RWDTS_XACT_FLAG_SUB_READ)) {
      goto done;
    }
    if ((query->flags & RWDTS_XACT_FLAG_SUB_READ)
        && (query->action != RWDTS_QUERY_READ)) {
      goto done;
    }
  }
  if ((entry->flags & RWDTS_FLAG_PUBLISHER) && (query->flags & RWDTS_XACT_FLAG_ADVISE)) {
    goto done;
  }

  RW_ASSERT(entry->keyspec);
  // Get the schema for this registration
  ks_schema = ((ProtobufCMessage*)entry->keyspec)->descriptor->ypbc_mdesc->module->schema;

  // If registration schema is NULL, set it to the schema from API
  if (ks_schema == NULL) {
    ks_schema = rwdts_api_get_ypbc_schema(apih);
  }
  RW_ASSERT(ks_schema);

  // ATTN -- Thes are needed for debugging an understandable keyspec
  rw_keyspec_path_get_new_print_buffer(entry->keyspec,
                                       NULL, ks_schema, &reg_ks_str);

  // For READS  and DELETES do not need not have any payload
  if (query->action == RWDTS_QUERY_READ) {
    rwdts_member_match_read(entry, query, ks_query, ks_schema, matches, matchid);
    goto done;
  } else if (query->action == RWDTS_QUERY_DELETE) {
    rwdts_member_match_delete(entry, query, ks_query, matches, matchid);
    goto done;
  }

  rwdts_member_match_create_update(entry, query, ks_query, ks_schema, matches, matchid);

done:
  if (reg_ks_str) {
    free(reg_ks_str);
  }
  return;
}

