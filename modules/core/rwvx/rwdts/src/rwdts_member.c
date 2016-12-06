
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
 * @file rwdts_member.c
 * @author Rajesh Velandy
 * @date 2014/09/15
 * @brief DTS  member API definitions
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

/*******************************************************************************
 *                      Static prototypes                                      *
 *******************************************************************************/

struct rwdts_tmp_split_rsp_s {
  rwdts_query_handle_t queryh;
  rwdts_xact_t*        xact;
  rwdts_xact_query_t*  xquery;
  rwdts_member_query_rsp_t rsp;
};

struct rwdts_tmp_abort_rsp_s {
  rwdts_xact_t*    xact;
  rw_status_t      status;
  char*            errstr;
};

void rwdts_dispatch_query_response(void *ctx);

void rwdts_member_split_rsp(void *ctx);

static rw_status_t
rwdts_xact_query_pend_response_deinit(rwdts_xact_query_pend_rsp_t *xact_rsp);

static RWDtsEventRsp
rwdts_xact_rsp_to_evtrsp(rwdts_xact_rsp_code_t xrsp_code);


/*******************************************************************************
 *                      Static functions                                       *
 *******************************************************************************/

/*
 * deinit a xact query response
 */

static rw_status_t
rwdts_xact_query_response_deinit(rwdts_xact_query_rsp_t *xact_rsp)
{
  int i;

  RW_ASSERT(xact_rsp);
  if (!xact_rsp) {
    return RW_STATUS_FAILURE;
  }
  /* free keyspec */
  if (xact_rsp->rsp.ks) {
    rw_keyspec_path_free(xact_rsp->rsp.ks, NULL);
    xact_rsp->rsp.ks = NULL;
  }
  /* free the protobufs if any */
  if (xact_rsp->rsp.msgs) {
    for (i = 0; i < xact_rsp->rsp.n_msgs; i++) {
      protobuf_c_message_free_unpacked(NULL, xact_rsp->rsp.msgs[i]);
      xact_rsp->rsp.msgs[i] = NULL;
    }
    RW_FREE(xact_rsp->rsp.msgs);
  }

  /* correlation id */
  if (xact_rsp->rsp.corr_id) {
    RW_FREE(xact_rsp->rsp.corr_id);
    xact_rsp->rsp.corr_id_len = 0;
  }

  RW_FREE(xact_rsp);
  return RW_STATUS_SUCCESS;
}

rwdts_return_status_t
rwdts_merge_rsp_msgs(rwdts_xact_t*             xact,
                     rwdts_xact_query_t*       xquery,
                     RWDtsQueryResult*         qres,
                     rwdts_member_query_rsp_t* rsp)
{
  if (rsp->n_msgs) {
    int i = 0;
    rw_status_t rs;
    qres->n_result++;
    qres->result = realloc(qres->result, (qres->n_result * sizeof(RWDtsQuerySingleResult*)));

    RWDtsQuerySingleResult *sqres = qres->result[qres->n_result-1] =
        (RWDtsQuerySingleResult*)RW_MALLOC0(sizeof(RWDtsQuerySingleResult));
    rwdts_query_single_result__init(sqres);
    sqres->has_evtrsp = 1;
    sqres->evtrsp = RWDTS_EVTRSP_ACK;
    sqres->result = (RWDtsPayload*)RW_MALLOC0(sizeof(RWDtsPayload));
    rwdts_payload__init(sqres->result);
    sqres->result->ptype = RWDTS_PAYLOAD_PBRAW;
    sqres->result->has_paybuf = TRUE;
    sqres->path = (RWDtsKey *)RW_MALLOC0 (sizeof (RWDtsKey));
    rwdts_key__init (sqres->path);
    sqres->path->ktype = RWDTS_KEY_RWKEYSPEC;
    sqres->path->has_keybuf = true;
    rs = rw_keyspec_path_serialize_dompath (rsp->ks, (xact)? &xact->ksi:NULL, &sqres->path->keybuf);
    if(rs != RW_STATUS_SUCCESS) {
      return RWDTS_RETURN_KEYSPEC_ERROR;
    }

    sqres->path->keyspec = (RwSchemaPathSpec*) rw_keyspec_path_create_dup_of_type(rsp->ks,
                                                                                  (xact)? &xact->ksi:NULL ,
                                                                                  &rw_schema_path_spec__descriptor);
    if(!sqres->path->keyspec) {
      return RWDTS_RETURN_KEYSPEC_ERROR;
    }
    for (i = 0; i < rsp->n_msgs; i++) {
      ProtobufCBinaryData msg_out, tmp_msg;
      msg_out.len = 0; msg_out.data = NULL;
      tmp_msg.len = 0; tmp_msg.data = NULL;

      if (rsp->msgs[i]) {
        tmp_msg.data = protobuf_c_message_serialize(NULL, rsp->msgs[i], &tmp_msg.len);
        msg_out.len = sqres->result->paybuf.len + tmp_msg.len;
        msg_out.data = RW_MALLOC0(msg_out.len);

        memcpy(msg_out.data, sqres->result->paybuf.data, sqres->result->paybuf.len);
        memcpy(msg_out.data+sqres->result->paybuf.len, tmp_msg.data, tmp_msg.len);
        free(sqres->result->paybuf.data);
        free(tmp_msg.data);
        sqres->result->paybuf.data = msg_out.data;
        sqres->result->paybuf.len = msg_out.len;
      }
    }
  }
  return RWDTS_RETURN_SUCCESS;
}

RWDtsQuerySingleResult*
rwdts_alloc_single_query_result(rwdts_xact_t*      xact,
                                ProtobufCMessage*  matchmsg,
                                rw_keyspec_path_t* matchks)
{
  RWDtsQuerySingleResult *sqres = NULL;
  rw_status_t rs;

  RW_ASSERT(matchks);
  if (!matchks) {
    return NULL;
  }

  sqres = (RWDtsQuerySingleResult*)RW_MALLOC0(sizeof(RWDtsQuerySingleResult));
  RW_ASSERT(sqres);
  rwdts_query_single_result__init(sqres);

  sqres->has_evtrsp = 1;
  sqres->evtrsp = RWDTS_EVTRSP_ACK;

  sqres->result = (RWDtsPayload*)RW_MALLOC0(sizeof(RWDtsPayload));
  RW_ASSERT(sqres->result);
  rwdts_payload__init(sqres->result);

  sqres->result->ptype = RWDTS_PAYLOAD_PBRAW;
  sqres->result->has_paybuf = TRUE;

  sqres->path = (RWDtsKey *)RW_MALLOC0 (sizeof (RWDtsKey));
  RW_ASSERT(sqres->path);
  rwdts_key__init (sqres->path);

  sqres->path->ktype = RWDTS_KEY_RWKEYSPEC;
  sqres->path->has_keybuf = true;

  if (matchmsg) {
    sqres->result->paybuf.data  = protobuf_c_message_serialize(NULL,
                                                               matchmsg,
                                                               &sqres->result->paybuf.len);
    // ATTN this should really be an assert
    // RW_ASSERT(sqres->result->paybuf.data != NULL);
    if (sqres->result->paybuf.data == NULL) {
      char *ks_str = NULL;
      rw_keyspec_path_get_new_print_buffer(matchks,
                                           NULL,
                                           NULL,
                                           &ks_str);
      RWTRACE_CRIT(xact->apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "protobuf_c_message_serialize() returned NULL, paybuf_len[%d]",
                   (int)sqres->result->paybuf.len);
      if (ks_str) {
        free(ks_str);
      }
      sqres->result->paybuf.len = 0;
    }
  }
  rs = rw_keyspec_path_serialize_dompath (matchks, (xact)? &xact->ksi:NULL, &sqres->path->keybuf);
  if (rs != RW_STATUS_SUCCESS) {
    protobuf_c_message_free_unpacked(&protobuf_c_default_instance, &sqres->base);
    return NULL;
  }
  sqres->path->keyspec =
      (RwSchemaPathSpec*)rw_keyspec_path_create_dup_of_type(matchks,
                                                            (xact)? &xact->ksi:NULL,
                                                            &rw_schema_path_spec__descriptor);
  if (rs != RW_STATUS_SUCCESS) {
    protobuf_c_message_free_unpacked(&protobuf_c_default_instance, &sqres->base);
    return NULL;
  }
  return sqres;
}

static rwdts_return_status_t
rwdts_msg_reroot(rwdts_xact_t*       xact,
                 ProtobufCMessage*   msg,
                 rw_keyspec_path_t*  ks,
                 rwdts_xact_query_t* xquery,
                 RWDtsQueryResult*   qres)
{
  RW_ASSERT(msg);
  RW_ASSERT(ks);
  RW_ASSERT(xquery);
  RW_ASSERT(qres);

  if (!msg || !ks || !xquery || !qres) {
    return RWDTS_RETURN_FAILURE;
  }

  ProtobufCMessage* matchmsg = NULL;
  rw_keyspec_path_t*     matchks  = NULL;
  rw_status_t rs;

  rw_keyspec_path_t *reroot_keyspec = NULL;
  rwdts_api_t *apih =  xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  const  rw_yang_pb_schema_t* schema   = rwdts_api_get_ypbc_schema(apih);
  if (RW_SCHEMA_CATEGORY_RPC_INPUT == rw_keyspec_path_get_category((rw_keyspec_path_t* )xquery->query->key->keyspec)) {
    reroot_keyspec = rw_keyspec_path_create_output_from_input((rw_keyspec_path_t* )xquery->query->key->keyspec, &xact->ksi, schema);
  } else {
    rw_keyspec_path_create_dup((rw_keyspec_path_t* )xquery->query->key->keyspec, &xact->ksi, &reroot_keyspec);
  }
  RW_ASSERT (reroot_keyspec);

  if (!RW_KEYSPEC_PATH_IS_A_SUB_KEYSPEC(NULL,
                                       ks, reroot_keyspec, NULL)) {
    rw_keyspec_path_free(reroot_keyspec, NULL);
    return RWDTS_RETURN_SUCCESS;
  }

  size_t depth_o = rw_keyspec_path_get_depth(reroot_keyspec);  
  size_t depth_i = rw_keyspec_path_get_depth(ks);
      
  if (depth_i < depth_o) {
    rw_keyspec_path_reroot_iter_state_t state;
    rw_keyspec_path_reroot_iter_init(ks,
                                     (xact)? &xact->ksi:NULL,
                                     &state,
                                     msg,
                                     reroot_keyspec);
     while (rw_keyspec_path_reroot_iter_next(&state)) {
       matchmsg = rw_keyspec_path_reroot_iter_take_msg(&state);
       matchks = rw_keyspec_path_reroot_iter_take_ks(&state);
       if (!matchmsg ||
           matchmsg == msg ||
           matchks == ks ||
           rw_keyspec_path_has_wildcards(matchks)) {
         rw_keyspec_path_reroot_iter_done(&state);
         rw_keyspec_path_free(reroot_keyspec, NULL);
         RWDTS_XACT_ABORT(xact, RW_STATUS_NOTFOUND, RWDTS_ERRSTR_KEY_MISMATCH);
         return RWDTS_RETURN_KEYSPEC_ERROR;
       }
       qres->result = realloc(qres->result, (qres->n_result + 1)* sizeof(RWDtsQuerySingleResult*));
       qres->result[qres->n_result]=rwdts_alloc_single_query_result(xact, matchmsg, matchks);
       RW_ASSERT(qres->result[qres->n_result]);
       protobuf_c_message_free_unpacked(&protobuf_c_default_instance, matchmsg);
       rw_keyspec_path_free(matchks, NULL);
       qres->n_result++;
     }
     rw_keyspec_path_reroot_iter_done(&state);
  } else {
    matchmsg = rw_keyspec_path_reroot(ks, (xact)? &xact->ksi:NULL, msg, reroot_keyspec);
    if (matchmsg) {
      RW_ASSERT(matchmsg != msg);
      rs = rw_keyspec_path_create_dup(ks, (xact)? &xact->ksi:NULL, &matchks);
      if(rs == RW_STATUS_SUCCESS) {
        rs = rw_keyspec_path_trunc_suffix_n(matchks, (xact)? &xact->ksi:NULL, depth_o);
        if (rs == RW_STATUS_SUCCESS) {
          qres->result = realloc(qres->result, (qres->n_result + 1)* sizeof(RWDtsQuerySingleResult*));
          qres->result[qres->n_result]=rwdts_alloc_single_query_result(xact, matchmsg, matchks);
          RW_ASSERT(qres->result[qres->n_result]);
          protobuf_c_message_free_unpacked(&protobuf_c_default_instance, matchmsg);
          rw_keyspec_path_free(matchks, NULL );
          qres->n_result++;
        }
        rw_keyspec_path_free(reroot_keyspec, NULL);
        return RWDTS_RETURN_SUCCESS;
      }
      rw_keyspec_path_free(reroot_keyspec, NULL);
      RWDTS_XACT_ABORT(xact, rs, RWDTS_ERRSTR_KEY_DUP);
      return RWDTS_RETURN_KEYSPEC_ERROR;
    } 
  }
  rw_keyspec_path_free(reroot_keyspec, NULL);
  return RWDTS_RETURN_SUCCESS;
}

static inline void
rwdts_add_serialnum(RWDtsQueryResult* qres,
                    rwdts_api_t*      apih)

{
  qres->has_serialnum = 1;
  qres->serialnum = rwdts_get_serialnum(apih, NULL);
  qres->has_client_idx = 1;
  qres->client_idx = apih->client_idx;
  qres->has_router_idx = 1;
  qres->router_idx = apih->router_idx;
  return;
}


/*
 * Indicate to dts that the member is ready
 */

void
rwdts_dispatch_query_response(void *ctx)
{
  rwdts_xact_query_rsp_t *xact_rsp = (rwdts_xact_query_rsp_t*) ctx;
  rwdts_member_query_rsp_t *rsp = &xact_rsp->rsp;

  RW_ASSERT(xact_rsp);

  rwdts_xact_t* xact = (rwdts_xact_t*) xact_rsp->xact;
  rwdts_match_info_t* match_info = NULL;
  RWDtsQuery* query = NULL;
  uint32_t queryidx = 0;

  RWDTS_LOG_CODEPOINT(xact, dispatch_query_response_f);

  if (xact_rsp->queryh) {
    RW_ASSERT(xact_rsp->queryh);
    match_info = (rwdts_match_info_t*) (xact_rsp->queryh);
    RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
    query = match_info->query;
    RW_ASSERT(query);
    queryidx = query->queryidx;

    if (match_info) {
      RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
      query = match_info->query;
      /* if RWDTS_EVTRSP_NA was sent previously
       * then we accept only RWDTS_EVTRS_NA only.
       */
      if (match_info->sent_evtrsp == RWDTS_EVTRSP_NA) {
        RW_ASSERT(rsp->evtrsp == RWDTS_EVTRSP_NA);
      }
      match_info->sent_evtrsp = rsp->evtrsp;
      RWMEMLOG(&xact->rwml_buffer,
           RWMEMLOG_MEM0,
           "rwdts_dispatch_query_response:sent_evtrsp",
           RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (rsp->evtrsp)));
    }
  }
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  /**
   * Find the matching  query in the transaction
   */

  RW_ASSERT(rsp);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, QUERY_RSP_DISPATCH, "Responding to query",
                           queryidx, query->corrid, (char*)rwdts_evtrsp_to_str(rsp->evtrsp));

  rwdts_xact_query_t *xquery = rwdts_xact_find_query_by_id(xact, queryidx);

  if (xquery == NULL) {
    RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_QueryRspDispatchFailed, "No matching query found",
                             queryidx, 0, (char*)rwdts_evtrsp_to_str(rsp->evtrsp));
    goto finish;
  }

  // Copy the new results if the response contain messages
  RWDtsQueryResult *qres = NULL;

  if (xquery->qres) {
    qres = xquery->qres;
  } else {
    qres = (RWDtsQueryResult*) RW_MALLOC0(sizeof(RWDtsQueryResult));
    RW_ASSERT(qres);
    rwdts_query_result__init(qres);
    qres->has_evtrsp = 1;
    qres->evtrsp = rsp->evtrsp;
    qres->has_queryidx = 1;
    qres->queryidx = queryidx;
    rwdts_add_serialnum(qres, apih);

    if (query) {
      qres->has_corrid = query->has_corrid;
      qres->corrid = query->corrid;
    }
  }
  {
    int i;
    for (i = 0; i < rsp->n_msgs; i++) {
      if (rsp->msgs[i]) {
        if(rwdts_msg_reroot(xact, rsp->msgs[i], rsp->ks, xquery, qres)
           != RWDTS_RETURN_SUCCESS)
        {
          RWDTS_XACT_ABORT(xact, RW_STATUS_FAILURE, RWDTS_ERRSTR_MSG_REROOT);
          rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
          protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage *)qres);
          return;
        }
      }
    }
    // ATTN append to the exitsing results
    if (query) {
      RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_QueryAppendRsp, "Appending response to query",
                              queryidx, query->corrid, (char*)rwdts_evtrsp_to_str(rsp->evtrsp));
    }
  }

  xquery->qres = qres;
  xquery->evtrsp = rwdts_final_rsp_code(xquery);
  if (xquery->clo && (xquery->evtrsp != RWDTS_EVTRSP_ASYNC)) {
    rwdts_respond_router(xquery->clo, /* FIXME */
    xact, rsp->evtrsp, xquery->rwreq, TRUE);
  }


finish:
  rwdts_xact_query_response_deinit(xact_rsp);
  if (apih->xact_timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->xact_timer);
    rwsched_dispatch_release(apih->tasklet, apih->xact_timer);
  }

  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  return;
}

/*
 * callback for the rwdts_populate_registration_cache
 */
rw_status_t
rwdts_xact_get_result_pb_local(rwdts_xact_t*                xact,
                               uint64_t                     corrid,
                               rwdts_member_registration_t* reg,
                               ProtobufCMessage***          result,
                               rw_keyspec_path_t***         keyspecs,
                               uint32_t*                    n_result)
{
  RW_ASSERT(xact);
  if (!xact) {
    return RW_STATUS_FAILURE;
  }
  rwdts_xact_result_t *res = NULL;

  rw_status_t rw_status = rwdts_xact_get_result_pbraw(xact, NULL, 0, &res, NULL);
  if (rw_status != RW_STATUS_SUCCESS || !res || !res->result || !res->result->result ) {
    return (RW_STATUS_FAILURE);
  }

  RWDTS_API_LOG_XACT_DEBUG_EVENT(xact->apih, xact, GetResult, "Getting result decoded");

  uint32_t* types = NULL;
  rw_status =  rwdts_xact_get_result_pb_int_alloc(xact,
                                                  corrid,
                                                  res,
                                                  result,
                                                  keyspecs,
                                                  &types,
                                                  reg->keyspec,
                                                  n_result);
  if (types) {
    RW_FREE(types);
  }
  return rw_status;
}


/*
 *  FInd a query matching a queryidx from  RWDtsQueryResult
 */

/*******************************************************************************
 *                      public APIs                                            *
 *******************************************************************************/

static void
rwdts_xact_query_deinit(rwdts_xact_query_t* xquery)
{
  if (xquery->query) {
    rwdts_query__free_unpacked(NULL, xquery->query);
    xquery->query = NULL;
  }

  RW_FREE_TYPE(xquery, rwdts_xact_query_t);
}

static int rwdts_xact_count;
rwdts_xact_query_t*
rwdts_xact_query_ref(rwdts_xact_query_t *x, const char *file, int line)
{
  RW_ASSERT_TYPE(x, rwdts_xact_query_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(x->xact->apih, x->xact, QresultRefIncr,
                                 RWLOG_ATTR_SPRINTF("xquery=%p ref_cnt=%d+1, from=%s:%d tot_count=%d + 1",
                                                    x, x->ref_cnt, file?file:"(nil)", line, rwdts_xact_count));
  g_atomic_int_inc(&x->ref_cnt);
  g_atomic_int_inc(&rwdts_xact_count);
  return x;
}

void
rwdts_xact_query_unref(rwdts_xact_query_t *x, const char *file, int line)
{

  RW_ASSERT_TYPE(x, rwdts_xact_query_t);
  RWDTS_API_LOG_XACT_DEBUG_EVENT(x->xact->apih, x->xact, QresultRefDecr,
                                 RWLOG_ATTR_SPRINTF("xquery=%p ref_cnt=%d-1, from=%s:%d tot_count=%d - 1",
                                                    x, x->ref_cnt, file?file:"(nil)", line, rwdts_xact_count));
  if(g_atomic_int_dec_and_test(&rwdts_xact_count))
    RW_ASSERT_TYPE(x, rwdts_xact_query_t);
  if (!g_atomic_int_dec_and_test(&x->ref_cnt)) {
    return;
  }
  rwdts_xact_query_deinit(x);
}

/*
 *
 * init a xact query
 */

static rwdts_xact_query_t*
rwdts_xact_query_init_internal(rwdts_xact_t *xact, RWDtsQuery *query)
{

  rwdts_xact_query_t *xquery = rwdts_xact_find_query_by_id(xact, query->queryidx);
  rwdts_api_t *apih = (rwdts_api_t *)xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RW_ASSERT(xquery == NULL);

  xquery = RW_MALLOC0_TYPE(sizeof(rwdts_xact_query_t), rwdts_xact_query_t);
  RW_ASSERT(xquery);

  xquery->query = query;
  xquery->xact = xact;
  xquery->xact_rsp_pending = NULL;

  if (query->flags&RWDTS_XACT_FLAG_NOTRAN) {
    apih->stats.total_nontrans_queries++;
  } else {
    apih->stats.total_trans_queries++;
  }
  HASH_ADD(hh, xact->queries, query->queryidx, sizeof(xquery->query->queryidx), xquery);
  RW_SKLIST_PARAMS_DECL(tab_list_,rwdts_match_info_t, matchid,
                        rw_sklist_comp_uint32_t, match_elt);
  RW_SKLIST_INIT(&(xquery->reg_matches), &tab_list_);
  return xquery;
}

void rwdts_member_prep_timer_cancel(rwdts_match_info_t* match, bool timeout, int line)
{
  // If running stop
  if (match->prep_timer_cancel) {
    rwdts_xact_t *xact;
    rwdts_api_t* apih;

    xact = match->xact_info.xact;
    RW_ASSERT_TYPE(xact,rwdts_xact_t);
    apih = xact->apih;
    RW_ASSERT_TYPE(apih, rwdts_api_t);

    rwsched_dispatch_source_cancel(apih->tasklet, match->prep_timer_cancel);
    rwsched_dispatch_release(apih->tasklet, match->prep_timer_cancel);
    match->prep_timer_cancel = NULL;
    if (!timeout) {
      rwdts_xact_unref(xact, __PRETTY_FUNCTION__, line);
    }
  }
  return;
}

static void rwdts_member_prep_check_timer(void* hndl)
{
  rwdts_match_info_t *match = (rwdts_match_info_t*) hndl;
  rwdts_xact_t *xact;
  rwdts_api_t* apih;
  xact = match->xact_info.xact;
  RW_ASSERT_TYPE(xact,rwdts_xact_t);
  apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_prep_timer_cancel(match, true, __LINE__);

  if (g_atomic_int_get(&match->resp_done)) {
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return;
  }

  RWDTS_API_LOG_XACT_EVENT(apih,xact,RwDtsApiLog_notif_MemberRspTimeout, rwdts_get_app_addr_res(xact->apih, match->reg->cb.cb.prepare), xact->num_prepares_dispatched);

  rwdts_member_query_rsp_t rsp;
  RW_ZERO_VARIABLE(&rsp);
  rsp.evtrsp = rwdts_is_transactional(xact)?RWDTS_EVTRSP_NACK:RWDTS_EVTRSP_NA;
  if (xact->tr) {
    RWDtsTracerouteEnt ent;
    char tmp_log_xact_id_str[128] = "";
    rwdts_traceroute_ent__init(&ent);
    ent.line = __LINE__;
    ent.func = (char*)__PRETTY_FUNCTION__;
    ent.what = RWDTS_TRACEROUTE_WHAT_PREP_TO;
    ent.srcpath = apih->client_path;
    rwdts_dbg_tracert_add_ent(xact->tr, &ent);
    if (xact->tr->print_) {
      fprintf(stderr, "%s:%u: TYPE:%s, MEMBER:%s\n", ent.func, ent.line,
              rwdts_print_ent_type(ent.what), ent.srcpath);
    }
    RWLOG_EVENT(apih->rwlog_instance, RwDtsApiLog_notif_TracePrepcheckTimeout, rwdts_xact_id_str(&xact->id,tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)),
                apih->client_path);
  }
  rwdts_member_send_response(xact, match->xact_info.queryh,&rsp);
  return;
}

#define RWDTS_ASYNC_TIMEOUT (150 * NSEC_PER_SEC)
void rwdts_member_start_prep_timer_cancel(rwdts_match_info_t* match)
{
  // If not running start
  if (!match->prep_timer_cancel) {
    rwdts_xact_t *xact;
    rwdts_api_t* apih;
    xact = match->xact_info.xact;
    RW_ASSERT_TYPE(xact,rwdts_xact_t);
    apih = xact->apih;
    RW_ASSERT_TYPE(apih, rwdts_api_t);

    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);

    match->prep_timer_cancel = rwsched_dispatch_source_create(apih->tasklet,
                                                              RWSCHED_DISPATCH_SOURCE_TYPE_TIMER, 0, 0, apih->client.rwq);
    rwsched_dispatch_source_set_event_handler_f(apih->tasklet,
                                                match->prep_timer_cancel, rwdts_member_prep_check_timer);
    rwsched_dispatch_set_context(apih->tasklet, match->prep_timer_cancel, match);
    rwsched_dispatch_source_set_timer(apih->tasklet, match->prep_timer_cancel,
                                      dispatch_time(DISPATCH_TIME_NOW, RWDTS_ASYNC_TIMEOUT),
                                      (RWDTS_ASYNC_TIMEOUT), 0);
    rwsched_dispatch_resume(apih->tasklet, match->prep_timer_cancel);

  }
  return;
}

rw_status_t
rwdts_xact_rmv_query(rwdts_xact_t *xact, rwdts_xact_query_t* xquery)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT_TYPE(xquery, rwdts_xact_query_t);

  HASH_DELETE(hh, xact->queries, xquery);
  RW_ASSERT(xquery);
  xquery->xact = NULL;

  // free the matched entry list
  rwdts_match_list_deinit((&xquery->reg_matches));

  if (xquery->xact_rsp_pending) {
    rwdts_xact_query_pend_response_deinit(xquery->xact_rsp_pending);
    xquery->xact_rsp_pending = NULL;
  }
  if (xquery->qres) {
    protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage *)xquery->qres);
  }
  rwdts_xact_query_unref(xquery, __PRETTY_FUNCTION__, __LINE__);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_match_list_deinit(rw_sklist_t *match_list)
{
  rwdts_match_info_t *match = NULL, *next_match = NULL, *removed = NULL;
  rw_status_t rs;

  match = RW_SKLIST_HEAD(match_list, rwdts_match_info_t);
  while(match) {
    next_match = RW_SKLIST_NEXT(match, rwdts_match_info_t, match_elt);
    RWDTS_ASSERT_MESSAGE(match->sent_evtrsp != RWDTS_EVTRSP_ASYNC,
                         0,
                         match->ks,
                         NULL,
                         "Destroyed in running state");
    if (match->ks) {
      rw_keyspec_path_free(match->ks, NULL);
    }
    if (match->in_ks) {
      rw_keyspec_path_free(match->in_ks, NULL);
    }
    if (match->msg) {
      protobuf_c_message_free_unpacked(NULL, match->msg);
      match->msg = NULL;
    }
    if (match->prep_timer_cancel) {
      rwdts_member_prep_timer_cancel(match, false, __LINE__);
    }
    rs = RW_SKLIST_REMOVE_BY_KEY(match_list,
                                 &match->matchid,
                                 &removed);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rwdts_member_reg_handle_unref((rwdts_member_reg_handle_t)match->reg);
    RW_FREE_TYPE(match, rwdts_match_info_t);
    match = next_match;
  }
  return RW_STATUS_SUCCESS;
}

static void
rwdts_member_send_error_f(void *ud)
{
  rwdts_xact_err_t *xact_err = (rwdts_xact_err_t *)ud;
  RWDtsErrorEntry *ent = xact_err->ent;
  rwdts_xact_t *xact = xact_err->xact;
  RWDtsQuery *xquery = xact_err->xquery;
  RWDtsErrorReport *err_report = NULL;

  if (xact) {
    err_report = xact->err_report;
  }
  else if (xquery && (xquery->action != RWDTS_QUERY_INVALID)) {
    if (!xquery->dbg) {
      xquery->dbg = RW_MALLOC0(sizeof(RWDtsDebug));
      rwdts_debug__init(xquery->dbg);
    }
    err_report = xquery->dbg->err_report;
  }
  else {
    // Need to have xact or xquery
    RW_ASSERT(xact || xquery);
  }

  if (!err_report) {
    err_report = rwdts_xact_create_err_report();
  }

  // Append it to report
  rwdts_xact_append_err_report(err_report, ent);

  if (xact) {
    xact->err_report = err_report;
  }
  else {
    xquery->dbg->err_report = err_report;
  }

  if (ent->key) {
   rwdts_key__free_unpacked(NULL, ent->key);
   ent->key = NULL;
  }
  if (ent->errstr) {
    RW_FREE(ent->errstr);
    ent->errstr = NULL;
  }
  if (ent->client_path) {
    RW_FREE(ent->client_path);
    ent->client_path = NULL;
  }
  if (ent->router_path) {
    RW_FREE(ent->router_path);
    ent->router_path = NULL;
  }
  if (ent->tasklet_name) {
    RW_FREE(ent->tasklet_name);
    ent->tasklet_name = NULL;
  }
  if (ent->keystr_buf) {
    RW_FREE(ent->keystr_buf);
    ent->keystr_buf = NULL;
  }
  RW_FREE(ent);
  ent = NULL;
  RW_FREE(xact_err);
  xact_err = NULL;
}

rw_status_t
rwdts_member_send_error(rwdts_xact_t*                   xact,
                        const rw_keyspec_path_t*        keyspec,
                        RWDtsQuery*                     xquery,
                        rwdts_api_t*                    apih,
                        const rwdts_member_query_rsp_t* rsp,
                        rw_status_t                     cause,
                        const char*                     errstr)
{
  rw_status_t rc = RW_STATUS_FAILURE;
  RWDtsQuery *query = NULL;
  if (xact) {
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
    if (!apih) {
      apih = xact->apih;
    }
  }
  else if (xquery && (xquery->action != RWDTS_QUERY_INVALID)) {
    query = xquery;
  }
  else {
    // Need atleast xquery or xact to append the error
    RW_ASSERT(xact || xquery);
    goto bailout;
  }
  if (apih) {
    RW_ASSERT_TYPE(apih, rwdts_api_t);
  }

  if (!query && xact) {
    if (xact->queries && xact->queries->query) {
      query = (RWDtsQuery *)xact->queries->query;
    }
    if (!query && xact->blocks_ct) {
      uint32_t idx = xact->blocks_ct;
      while (!query && idx) {
        idx--;
        rwdts_xact_block_t* block = xact->blocks[idx];
        if (block->subx.n_query) {
          query = block->subx.query[block->subx.n_query-1];
        }
      }
    }
    if (!query && xact->xact && xact->xact->n_subx) {
      RWDtsXactBlock *block = NULL;
      int i = 0;
      for (i=0; i<xact->xact->n_subx; i++) {
        if (xact->xact->block->blockidx ==
            xact->xact->subx[i]->block->blockidx) {
          block = xact->xact->subx[i];
          break;
        }
      }
      if (block && block->n_query) {
        query = block->query[block->n_query-1];
      }
    }
  }

  RWDtsErrorEntry *ent = RW_MALLOC(sizeof(RWDtsErrorEntry));
  // Fill ENTRY
  rwdts_error_entry__init(ent);
  if (errstr) {
    ent->errstr = RW_STRDUP(errstr);
  }
  ent->cause = cause;

  rw_keyspec_instance_t *ksi = NULL;
  if (apih) {
    ksi = &apih->ksi;
  }
  else if (xact) {
    ksi = &xact->ksi;
  }

  char *keystr_buf = NULL;
  const rw_keyspec_path_t* ks = NULL;
  if (keyspec) {
    ks = keyspec;
  }
  else if (rsp && rsp->ks) {
    ks = rsp->ks;
  }
  else if (query && (query->action != RWDTS_QUERY_INVALID) && query->key
           && (query->key->ktype == RWDTS_KEY_RWKEYSPEC)){
    ks = (rw_keyspec_path_t*)query->key->keyspec;
    if ((query->key->keystr) && (strlen(query->key->keystr))) {
      keystr_buf = RW_STRDUP(query->key->keystr);
    }
  }
  else if (xact && xact->mbr_data && xact->mbr_data->ks) {
    ks = xact->mbr_data->ks;
  }

  rw_keyspec_path_t *output = NULL;
  if (ks) {
    rc = rw_keyspec_path_create_dup(ks,
                                    ksi,
                                    &output);
    if (keystr_buf == NULL) {
      // Create a keystr buffer
      rw_keyspec_path_get_new_print_buffer(ks,
                                           NULL,
                                           (apih)?apih->ypbc_schema:NULL,
                                         &keystr_buf);
    }
  }

  if ((rc == RW_STATUS_SUCCESS ) && output) {
    ent->key = RW_MALLOC(sizeof(RWDtsKey));
    rwdts_key__init(ent->key);
    ent->key->keyspec = (RwSchemaPathSpec *)output;
    output = NULL;
  }

  if (!keystr_buf) {
    keystr_buf = RW_STRDUP("~~Key spec NA~~");
  }
  ent->keystr_buf = keystr_buf;

  if (query) {
    if (query->has_corrid) {
      ent->corrid = query->corrid;
    }
    if (query->has_queryidx) {
      ent->queryidx = query->queryidx;
    }
    if (query->serial) {
      if (query->serial->id.has_member_id) {
        ent->client_idx = query->serial->id.member_id;
      }
      if (query->serial->id.has_router_id) {
        ent->router_idx = query->serial->id.router_id;
      }
      if (query->serial->id.has_pub_id) {
        ent->serialnum = query->serial->id.pub_id;
      }
      if (query->serial->has_serial) {
        ent->serialnum = query->serial->serial;
      }
    }
    ent->action = query->action;
    ent->query_flags = query->flags;
  }

  if (apih->client_path) {
    ent->client_path = RW_STRDUP(apih->client_path);
  }
  if (apih->router_path) {
    ent->router_path = RW_STRDUP(apih->router_path);
  }
  if (apih->rwtasklet_info) {
    if (apih->rwtasklet_info->identity.rwtasklet_name) {
      ent->tasklet_name =
          RW_STRDUP(apih->rwtasklet_info->identity.rwtasklet_name);
      ent->tasklet_inst =
          apih->rwtasklet_info->identity.rwtasklet_instance_id;
    }
  }

  if (apih) {
    // Send netconf notification
    if (apih->rwlog_instance) {
      RWLOG_EVENT (apih->rwlog_instance,
                   RwDtsRouterLog_notif_NotifyXactQueryError,
                   keystr_buf,
                   ent->cause,
                   ent->errstr,
                   ent->corrid,
                   ent->queryidx,
                   ent->serialnum,
                   ent->client_idx,
                   ent->router_idx,
                   (ent->block && ent->block)?ent->block->blockidx:0,
                   (ent->client_path)?ent->client_path:"",
                   (ent->router_path)?ent->router_path:"",
                   (ent->tasklet_name)?ent->tasklet_name:"",
                   ent->tasklet_inst,
                   ent->action,
                   ent->query_flags);
    }
  }

  rwdts_xact_err_t *xact_err = RW_MALLOC(sizeof(rwdts_xact_err_t));
  xact_err->ent = ent;
  xact_err->xact = xact;
  xact_err->xquery = xquery;
  if (0 && apih)
  {
    rwsched_dispatch_async_f(apih->tasklet,
                             apih->client.rwq,
                             xact_err,
                             rwdts_member_send_error_f);
    rc = RW_STATUS_SUCCESS;
  }
  else
  {
    rwdts_member_send_error_f(xact_err);
    rc = RW_STATUS_SUCCESS;
  }


bailout:
  if (rc != RW_STATUS_SUCCESS) {
    if (apih) {
      RWTRACE_ERROR(apih->rwtrace_instance,
                    RWTRACE_CATEGORY_RWTASKLET,
                    "Transaction error key %s, cause %d, msg %s",
                    keystr_buf,
                    cause,
                    (errstr)?errstr:"");
    }
    else {
      DTS_PRINT("Transaction error key %s, cause %d, msg %s",
                keystr_buf, cause, (errstr)?errstr:"");
    }
    /* Free the allocated memory */
    if (ent) {
      if (ent->key) {
        RW_FREE(ent->key);
        ent->key = NULL;
      }
      if (ent->errstr) {
        RW_FREE(ent->errstr);
        ent->errstr = NULL;
      }
      if (keystr_buf) {
        RW_FREE(keystr_buf);
        keystr_buf = 0;
      }
      RW_FREE(ent);
      ent = NULL;
    }
  }

  return rc;
}

rw_status_t
rwdts_member_send_err_on_abort (rwdts_xact_t* xact,
                                rw_status_t   cause,
                                const char*   errstr)
{
  return rwdts_member_send_error(xact, NULL, NULL, NULL, NULL, cause, errstr);
}

void rwdts_member_send_xact_abort(void* ctx)
{
  struct rwdts_tmp_abort_rsp_s* abort_rsp = (struct rwdts_tmp_abort_rsp_s*)ctx;

  char* errstr = abort_rsp->errstr;
  rw_status_t status = abort_rsp->status;
  rwdts_xact_t* xact = abort_rsp->xact;

  RW_ASSERT(xact);
  char err_msg[512];

  snprintf(err_msg, 512, "%s[%d]: %s", __FUNCTION__, __LINE__, errstr);
  rwdts_xact_abort(xact, status, err_msg);

  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  RW_FREE(abort_rsp);
  return;
}

static rw_status_t
rwdts_member_copy_rsp(rwdts_api_t* apih,
                      rwdts_xact_t* xact,
                      rwdts_query_handle_t  queryh,
                      rwdts_member_query_rsp_t*   src_rsp,
                      rwdts_member_query_rsp_t*   dst_rsp)
{
  int i;
  uint32_t howmany = 0;

  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RW_ASSERT(xact);
  if (!xact) {
    return RW_STATUS_FAILURE;
  }
  
  if (src_rsp->ks) {
    if(rw_keyspec_path_has_wildcards(src_rsp->ks))
    {
      char *tmp_str = NULL;
      rw_keyspec_path_get_new_print_buffer(src_rsp->ks,
                                           NULL ,
                                           rwdts_api_get_ypbc_schema(apih),
                                           &tmp_str);
      RW_ASSERT_MESSAGE(0,"Incorrect Key %s for member %s \n",
                        apih->client_path,
                        tmp_str);
      free(tmp_str);
    }
 
    rw_status_t rs = rw_keyspec_path_create_dup(src_rsp->ks, (xact)? &xact->ksi:&apih->ksi , &dst_rsp->ks);
    if(rs != RW_STATUS_SUCCESS)
    {
      rwdts_match_info_t *match_info = (rwdts_match_info_t *)queryh;
      if (match_info) {
        RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
        /* if RWDTS_EVTRSP_NA was sent previously
         * then we accept only RWDTS_EVTRS_NA only.
         */
        if (match_info->sent_evtrsp == RWDTS_EVTRSP_NA) {
          RW_ASSERT(src_rsp->evtrsp == RWDTS_EVTRSP_NA);
        }
        match_info->sent_evtrsp = src_rsp->evtrsp;
        RWMEMLOG(&xact->rwml_buffer,
           RWMEMLOG_MEM0,
           "rwdts_member_copy_rsp(1):sent_evtrsp",
           RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (src_rsp->evtrsp)));
      }
      struct rwdts_tmp_abort_rsp_s* abort_rsp = RW_MALLOC0(sizeof(struct rwdts_tmp_abort_rsp_s));
      abort_rsp->xact = xact;
      abort_rsp->status = rs;
      abort_rsp->errstr = RWDTS_ERRSTR_KEY_DUP;
      rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
      rwsched_dispatch_async_f(apih->tasklet,
                               apih->client.rwq,
                               abort_rsp,
                               rwdts_member_send_xact_abort);
      return RW_STATUS_FAILURE;
    }
  }

  dst_rsp->evtrsp = src_rsp->evtrsp;
  dst_rsp->n_msgs = src_rsp->n_msgs;
  for ( i = 0; i < src_rsp->n_msgs; i++)
  {
    if (protobuf_c_message_unknown_get_count(src_rsp->msgs[i])) {
      rwdts_report_unknown_fields(src_rsp->ks, apih, src_rsp->msgs[i]);
      protobuf_c_message_delete_unknown_all(NULL, src_rsp->msgs[i]);
    }
    if(!rw_keyspec_path_matches_message (src_rsp->ks, (xact)? &xact->ksi:&apih->ksi, src_rsp->msgs[i], true))
    {
      rwdts_match_info_t *match_info = (rwdts_match_info_t *)queryh;
      if (match_info) {
        RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
        /* if RWDTS_EVTRSP_NA was sent previously
         * then we accept only RWDTS_EVTRS_NA only.
         */
        if (match_info->sent_evtrsp == RWDTS_EVTRSP_NA) {
          RW_ASSERT(src_rsp->evtrsp == RWDTS_EVTRSP_NA);
        }
        match_info->sent_evtrsp = src_rsp->evtrsp;
        RWMEMLOG(&xact->rwml_buffer,
           RWMEMLOG_MEM0,
           "rwdts_member_copy_rsp(2):sent_evtrsp",
           RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (src_rsp->evtrsp)));
      }
      struct rwdts_tmp_abort_rsp_s* abort_rsp = RW_MALLOC0(sizeof(struct rwdts_tmp_abort_rsp_s));
      abort_rsp->xact = xact;
      abort_rsp->status = RW_STATUS_NOTFOUND;
      abort_rsp->errstr = RWDTS_ERRSTR_KEY_MISMATCH;
      rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
      rwsched_dispatch_async_f(apih->tasklet,
                               apih->client.rwq,
                               abort_rsp,
                               rwdts_member_send_xact_abort);
    return RW_STATUS_FAILURE;
    }
  }

  dst_rsp->flags = src_rsp->flags;
  if (src_rsp->n_msgs)
  {
    if (!howmany)
    {
      howmany = dst_rsp->n_msgs;
    }
    RW_ASSERT(dst_rsp->n_msgs);
    dst_rsp->msgs = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*) * dst_rsp->n_msgs);
    for (i = 0; i < howmany; i++) {
      RW_ASSERT(src_rsp->msgs[i]);
      dst_rsp->msgs[i] = protobuf_c_message_duplicate(NULL,
                                                      src_rsp->msgs[i],
                                                      src_rsp->msgs[i]->descriptor);
      RWDTS_ASSERT_MESSAGE(dst_rsp->msgs[i], apih, 0, xact, " Protobuf dup failed");
    }
  }

  // ATTN: This check is invalid and always fail.
  if (dst_rsp->corr_id) {
    dst_rsp->corr_id_len = src_rsp->corr_id_len;
    dst_rsp->corr_id = (uint8_t*)RW_MALLOC0(sizeof(uint8_t) * src_rsp->corr_id_len);

    RWDTS_ASSERT_MESSAGE(dst_rsp->corr_id, apih, 0, xact,"Malloc failed");
    memcpy(dst_rsp->corr_id, src_rsp->corr_id, src_rsp->corr_id_len);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_send_response_int(rwdts_xact_t*                   xact,
                               rwdts_query_handle_t            queryh,
                               rwdts_member_query_rsp_t*       rsp)
{
  RW_ASSERT(rsp);
  if (!rsp) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  rwdts_api_t *apih = xact->apih;
  RWDtsQuery *query = NULL;
  rwdts_match_info_t *match_info = (rwdts_match_info_t*)queryh;

  if (match_info) {
    RW_ASSERT_TYPE(match_info, rwdts_match_info_t);

    if (g_atomic_int_get(&match_info->resp_done)) {
      //Log this 
      return RW_STATUS_FAILURE;
    }
    else if (rsp->evtrsp != RWDTS_EVTRSP_ASYNC) {
      RW_ASSERT(g_atomic_int_get(&match_info->resp_done) == 0);
      g_atomic_int_set(&match_info->resp_done, 1);
    }

    query = match_info->query;
    match_info->evtrsp = RWDTS_EVTRSP_ASYNC;
  }

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, MEMBER_XACT_RSP, "Member responding to transaction",
                           (char*)rwdts_evtrsp_to_str(rsp->evtrsp));

  rwdts_xact_query_t *xquery = NULL;
  if (query) {
    xquery = rwdts_xact_find_query_by_id(xact, query->queryidx);
  }
  if (rsp->evtrsp == RWDTS_EVTRSP_NACK) {
#if 0
    RWDTS_MEMBER_SEND_ERROR (xact, NULL, (xquery)?xquery->query:NULL,
                             apih, rsp, RW_STATUS_FAILURE,
                             "ERROR member sending nack response");
#endif
  }
  if (xquery && xquery->responded) {

    /* Let's not get more responses when we (every registration, mind you) are already done responding */
    RW_ASSERT(xquery->evtrsp != RWDTS_EVTRSP_NA);
    //RW_ASSERT(xquery->evtrsp != RWDTS_EVTRSP_NACK);

    /* Add the response(s) in rsp to xquery's xact_rsp_pending response accumulator */
    struct rwdts_tmp_split_rsp_s *split_rsp = RW_MALLOC0(sizeof(struct rwdts_tmp_split_rsp_s));
    split_rsp->queryh = queryh;
    split_rsp->xact = xact;
    split_rsp->xquery = xquery;
    rw_status_t rs = rwdts_member_copy_rsp(apih, xact, queryh, rsp, &split_rsp->rsp);
    if (rs != RW_STATUS_SUCCESS) {
      RW_FREE(split_rsp);
      return RW_STATUS_FAILURE;
    }

    RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, SplitRspDispatch, 
                                   RWLOG_ATTR_SPRINTF("Member[%s]: rwdts_member_send_response dispatch results %d code %s",
                                                      apih->client_path, split_rsp->rsp.n_msgs, rwdts_evtrsp_to_str(split_rsp->rsp.evtrsp)));


    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
    RWDTS_LOG_CODEPOINT(xact, dispatch_query_response);
    rwsched_dispatch_async_f(apih->tasklet,
                             apih->client.rwq,
                             split_rsp,
                             rwdts_member_split_rsp);
  } else {
    rwdts_xact_query_rsp_t *xact_rsp = (rwdts_xact_query_rsp_t*)RW_MALLOC0(sizeof(rwdts_xact_query_rsp_t));

    xact_rsp->xact = xact;
    xact_rsp->queryh = queryh;

    rw_status_t rs = rwdts_member_copy_rsp(apih, xact, queryh, rsp, &xact_rsp->rsp);
    if (rs != RW_STATUS_SUCCESS) {
      RW_FREE(xact_rsp);
      return RW_STATUS_FAILURE;
    }

    //  Dispatch async..
    RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, DispatchQueryRsp,
                                   RWLOG_ATTR_SPRINTF("Member[%s]: rwdts_member_send_response dispatch results %d code %s",
                                                      apih->client_path, xact_rsp->rsp.n_msgs, rwdts_evtrsp_to_str(xact_rsp->rsp.evtrsp)));

    rwdts_xact_ref(xact, __PRETTY_FUNCTION__, __LINE__);
    RWDTS_LOG_CODEPOINT(xact, dispatch_query_response);
    rwsched_dispatch_async_f(apih->tasklet,
                             apih->client.rwq,
                             xact_rsp,
                             rwdts_dispatch_query_response);
  }
  if (match_info 
      && match_info->reg
      && match_info->reg->cb.cb.prepare 
      && g_atomic_int_get(&match_info->prepared)
      && rsp->evtrsp != RWDTS_EVTRSP_ASYNC) {
    if (match_info->prep_timer_cancel) {
      rwdts_member_prep_timer_cancel(match_info, false, __LINE__);
    }
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_member_send_response(rwdts_xact_t*                   xact,
                           rwdts_query_handle_t            queryh,
                           const rwdts_member_query_rsp_t* rsp)

{
  rwdts_member_query_rsp_t rsp_int = *rsp;
  rsp_int.getnext_ptr = 0;
  return rwdts_member_send_response_int(xact, queryh, &rsp_int);
}

/*
 *  Send  a reponse from member to router
 */
rw_status_t
rwdts_member_send_response_more(rwdts_xact_t*                   xact,
                                rwdts_query_handle_t            queryh,
                                const rwdts_member_query_rsp_t* rsp,
                                void*                           getnext_ptr)
{
  RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, MemberRspMore, 
                                 RWLOG_ATTR_SPRINTF("Member[%s]: rwdts_member_send_response_more", xact->apih->client_path));
  rwdts_member_query_rsp_t rsp_int = *rsp;
  rsp_int.getnext_ptr = getnext_ptr;
  return rwdts_member_send_response_int(xact, queryh, &rsp_int);
}

static rw_status_t
rwdts_xact_split_response_deinit(struct rwdts_tmp_split_rsp_s* split_rsp)
{
  int i;

  RW_ASSERT(split_rsp);
  if (!split_rsp) {
    return RW_STATUS_FAILURE;
  }
  /* free keyspec */
  if (split_rsp->rsp.ks) {
    rw_keyspec_path_free(split_rsp->rsp.ks, NULL);
    split_rsp->rsp.ks = NULL;
  }
  /* free the protobufs if any */
  if (split_rsp->rsp.msgs) {
    for (i = 0; i < split_rsp->rsp.n_msgs; i++) {
      protobuf_c_message_free_unpacked(NULL, split_rsp->rsp.msgs[i]);
      split_rsp->rsp.msgs[i] = NULL;
    }
    RW_FREE(split_rsp->rsp.msgs);
  }

  /* correlation id */
  if (split_rsp->rsp.corr_id) {
    RW_FREE(split_rsp->rsp.corr_id);
    split_rsp->rsp.corr_id_len = 0;
  }

  RW_FREE(split_rsp);
  return RW_STATUS_SUCCESS;
}

/* Takes the last part of response and puts in xact */
void
rwdts_member_split_rsp(void *ctx)
{
  //rwdts_member_query_rsp_t  *new_rsp = NULL;
  struct rwdts_tmp_split_rsp_s* split_rsp = (struct rwdts_tmp_split_rsp_s*)ctx;
  RW_ASSERT(split_rsp);

  rwdts_query_handle_t      queryh = split_rsp->queryh;
  rwdts_xact_t*             xact   = split_rsp->xact;
  rwdts_xact_query_t*       xquery = split_rsp->xquery;
  rwdts_member_query_rsp_t* rsp    = &split_rsp->rsp;

  int i, j;
  int howmanypending = (rsp->n_msgs > xquery->credits)? (rsp->n_msgs-xquery->credits): rsp->n_msgs;
  rwdts_xact_query_pend_rsp_t *xact_rsp = xquery->xact_rsp_pending;
  rwdts_match_info_t *match_info = (rwdts_match_info_t *)queryh;
  uint32_t matchid = match_info->matchid-1;
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWDTS_LOG_CODEPOINT(xact, member_split_rsp);

  if (match_info) {
    RW_ASSERT_TYPE(match_info, rwdts_match_info_t);
    /* if RWDTS_EVTRSP_NA was sent previously
     * then we accept only RWDTS_EVTRS_NA only.
     */
    if (match_info->sent_evtrsp == RWDTS_EVTRSP_NA) {
      RW_ASSERT(rsp->evtrsp == RWDTS_EVTRSP_NA);
    }
     RWMEMLOG(&xact->rwml_buffer,
        RWMEMLOG_MEM0,
           "rwdts_member_split_rsp(1):sent_evtrsp",
           RWMEMLOG_ARG_ENUM_FUNC(rwdts_evtrsp_to_str, "", (rsp->evtrsp)));
    match_info->sent_evtrsp = rsp->evtrsp;
  }

  if (!xact_rsp) {
    uint32_t total_matches = RW_SKLIST_LENGTH(&(xquery->reg_matches));
    xact_rsp = (rwdts_xact_query_pend_rsp_t*)RW_MALLOC0(sizeof(rwdts_xact_query_pend_rsp_t));
    xact_rsp->xact = xact;
    xact_rsp->queryh = (rwdts_query_handle_t)match_info->query;
    xquery->xact_rsp_pending = xact_rsp;
    xact_rsp->n_rsp = total_matches;
    xact_rsp->rsp = RW_MALLOC0(xact_rsp->n_rsp * sizeof(rwdts_member_query_rsp_t*));
  }

  RW_ASSERT(matchid < xact_rsp->n_rsp);

  if (!xact_rsp->rsp[matchid]) {
    xact_rsp->rsp[matchid] = RW_MALLOC0(sizeof(rwdts_member_query_rsp_t));
  }
  RW_ASSERT(xact_rsp->rsp[matchid]);

  if (rsp->ks) {
    RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp->ks));
    if (!xact_rsp->rsp[matchid]->ks) {
      rw_status_t rs = rw_keyspec_path_create_dup(rsp->ks, (xact)? &xact->ksi:NULL , &xact_rsp->rsp[matchid]->ks);
      if(rs != RW_STATUS_SUCCESS)
      {
        RWDTS_XACT_ABORT(xact, rs, RWDTS_ERRSTR_KEY_DUP);
        RW_FREE(xact_rsp);
        RW_FREE(split_rsp);
        rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
        return;
      }
    }
  }

  uint32_t prev_count = xact_rsp->rsp[matchid]->n_msgs;
  xact_rsp->rsp[matchid]->n_msgs += howmanypending;
  //Everything above credits is pending.

  if (rsp->n_msgs) {
    xact_rsp->rsp[matchid]->msgs = (ProtobufCMessage**)realloc(xact_rsp->rsp[matchid]->msgs, sizeof(ProtobufCMessage*) * xact_rsp->rsp[matchid]->n_msgs);
    RW_ASSERT(xact_rsp->rsp[matchid]->msgs);
    for(i = 0, j  = prev_count; i < howmanypending;i++, j++)
    {
      RW_ASSERT(rsp->msgs[i+(rsp->n_msgs-howmanypending)]);
      if (protobuf_c_message_unknown_get_count(rsp->msgs[i+(rsp->n_msgs-howmanypending)])) {
        rwdts_report_unknown_fields(rsp->ks, apih, rsp->msgs[i+(rsp->n_msgs-howmanypending)]);
        protobuf_c_message_delete_unknown_all(NULL, rsp->msgs[i+(rsp->n_msgs-howmanypending)]);
      }
      xact_rsp->rsp[matchid]->msgs[j] = protobuf_c_message_duplicate(NULL,
                                                           rsp->msgs[i+(rsp->n_msgs-howmanypending)],
                                                           rsp->msgs[i+(rsp->n_msgs-howmanypending)]->descriptor);
      RW_ASSERT(xact_rsp->rsp[matchid]->msgs[j]);
    }
  }

  xact_rsp->rsp[matchid]->flags = rsp->flags;

  // ATTN: This check is invalid and always fail.
  if (xact_rsp->rsp[matchid]->corr_id) {
    xact_rsp->rsp[matchid]->corr_id_len = rsp->corr_id_len;
    xact_rsp->rsp[matchid]->corr_id = (uint8_t*)RW_MALLOC0(sizeof(uint8_t) * rsp->corr_id_len);

    RW_ASSERT(xact_rsp->rsp[matchid]->corr_id);
    memcpy(xact_rsp->rsp[matchid]->corr_id, rsp->corr_id, rsp->corr_id_len);
  }
  xquery->evtrsp = rwdts_final_rsp_code(xquery);
  xact_rsp->rsp[matchid]->evtrsp = rsp->evtrsp;

  if (rsp->evtrsp != RWDTS_EVTRSP_ASYNC) {
    if (xquery->rwreq) {
      /* Build actual toplevel response from that pending xact_rsp_pending. */
      rwdts_dispatch_query_response(xquery->xact_rsp_pending);
      RW_ASSERT(xquery->qres);
    } else {
      rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    }
  } 
  rwdts_xact_split_response_deinit(split_rsp);
  return;
}

/*
 * Find  a query by id
 */
rwdts_xact_query_t*
rwdts_xact_find_query_by_id(const rwdts_xact_t* xact,
                            uint32_t            queryidx)
{
  RW_ASSERT(xact);
  if (!xact) {
    return NULL;
  }
  rwdts_xact_query_t *query = NULL;
  HASH_FIND(hh, xact->queries, &(queryidx), sizeof(queryidx), query);
  return query;
}

/*
 * Add a query to a xact
 */

rw_status_t
rwdts_xact_add_query_internal(rwdts_xact_t*        xact,
                     const RWDtsQuery*             query,
                     rwdts_xact_query_t**          query_out)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT(query);
  if (!query) {
    return RW_STATUS_FAILURE;
  }

  rwdts_xact_query_t *xquery = rwdts_xact_find_query_by_id(xact, query->queryidx);

  if (xquery != NULL) {
    if (query_out) {
      *query_out = xquery;
    }
    return RW_STATUS_DUPLICATE;
  }
  RWDtsQuery *query_dup = (RWDtsQuery*)protobuf_c_message_duplicate(NULL, &query->base, query->base.descriptor);
  RW_ASSERT(query_dup != NULL);

  xquery = rwdts_xact_query_init_internal(xact, query_dup);

  RW_ASSERT(xquery);
  if (!xquery) {
    return RW_STATUS_FAILURE;
  }
  rwdts_xact_query_ref(xquery, __PRETTY_FUNCTION__, __LINE__);

  if (query_out) {
    *query_out = xquery;
  }
  if (query->has_credits)
  {
    xquery->credits = query->credits;
  }
  return RW_STATUS_SUCCESS;
}

static rwdts_kv_light_reply_status_t 
rwdts_store_cache_set_callback(rwdts_kv_light_set_status_t status,
                               int serial_num, void *userdata)
{
  //rwdts_kv_handle_t *handle = (rwdts_kv_handle_t *)userdata;

  if(RWDTS_KV_LIGHT_SET_SUCCESS == status) {
    /* Do nothing */
  }

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static void
rwdts_reg_store_cache_obj(rwdts_api_t *apih,
                          rwdts_xact_t* xact,                       
                          rwdts_member_data_object_t *mobj)
{
  rwdts_member_data_object_t **obj_list_p = NULL;
  rwdts_member_data_object_t *newobj = NULL;
  rwdts_member_registration_t *reg = mobj->reg;
  bool listy;
  obj_list_p = &reg->obj_list;


  listy = rw_keyspec_path_is_listy(reg->keyspec);
  if (!listy) {
    HASH_FIND(hh_data, (*obj_list_p), mobj->key, mobj->key_len, newobj);

    if ((mobj->create_flag) ||
        ((mobj->update_flag) && (newobj == NULL))){
      RW_ASSERT(mobj->keyspec);
      if (reg->shard && reg->shard->appdata) {
        rwdts_shard_handle_appdata_create_element(reg->shard, mobj->keyspec, mobj->msg);
      } else {
        newobj = rwdts_member_data_init(reg, (ProtobufCMessage*)mobj->msg, mobj->keyspec, false, false);

        /* Add an audit trail that this record is updated */

        rwdts_member_data_object_add_audit_trail(newobj,
                                                 RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                 RW_DTS_AUDIT_TRAIL_ACTION_OBJ_CREATE);
        HASH_ADD_KEYPTR(hh_data, (*obj_list_p), newobj->key, newobj->key_len, newobj);
        apih->stats.num_committed_create_obj++;
      }
      if (mobj->create_flag) {
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_CREATE);
      } else {
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_UPDATE);
      }
    } else if (mobj->delete_mark) {
      apih->stats.num_committed_delete_obj++;
      rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_DELETE);
      if (reg->shard && reg->shard->appdata) {
        rwdts_shard_handle_appdata_delete_element(reg->shard, mobj->keyspec, mobj->msg);
      } else {
        if (newobj != NULL) {
          HASH_DELETE(hh_data, (*obj_list_p), newobj);

          rwdts_member_data_object_add_audit_trail(newobj,
                                                   RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                   RW_DTS_AUDIT_TRAIL_ACTION_OBJ_DELETE);

          rw_status_t rs = rwdts_member_data_deinit(newobj);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
        }
      }
    } else if (mobj->update_flag) {
      RW_ASSERT(newobj != NULL);
      RW_ASSERT(mobj->keyspec);
      RW_ASSERT(newobj->keyspec);
      apih->stats.num_committed_update_obj++;
      if (reg->shard && reg->shard->appdata) {
        rwdts_shard_handle_appdata_update_element(reg->shard, mobj->keyspec, mobj->msg);
      } else {
        if (!mobj->replace_flag) {
          //merge(incoming/new, existing/old) => result in old
          protobuf_c_boolean ret = protobuf_c_message_merge(NULL, mobj->msg, newobj->msg);
          RW_ASSERT(ret == TRUE);
        } else {
          protobuf_c_message_free_unpacked(NULL, newobj->msg);
          newobj->msg =  protobuf_c_message_duplicate(NULL, mobj->msg, mobj->msg->descriptor);
        }

        rwdts_member_data_object_add_audit_trail(newobj,
                                                 RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                 RW_DTS_AUDIT_TRAIL_ACTION_OBJ_UPDATE);
      }
      /* Should a update value be sent to KV? */
      rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_UPDATE);
    }
  } else {
    HASH_FIND(hh_data, (*obj_list_p), mobj->key, mobj->key_len, newobj);
    if (((mobj->create_flag) && (newobj == NULL)) ||
        ((mobj->update_flag) && (newobj == NULL))){
      RW_ASSERT(mobj->keyspec);
      if (reg->shard && reg->shard->appdata) {
        rwdts_shard_handle_appdata_create_element(reg->shard, mobj->keyspec, mobj->msg);
      } else {
        newobj = rwdts_member_data_init(reg, (ProtobufCMessage*)mobj->msg, mobj->keyspec, false, true);
        mobj->keyspec = NULL;
        apih->stats.num_committed_create_obj++;
        HASH_ADD_KEYPTR(hh_data, (*obj_list_p), newobj->key, newobj->key_len, newobj);

        rwdts_member_data_object_add_audit_trail(newobj,
                                                 RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                 RW_DTS_AUDIT_TRAIL_ACTION_OBJ_CREATE);
      }
      if (mobj->create_flag) {
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_CREATE);
      } else {
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_UPDATE);
      }
      if ((reg->flags & RWDTS_FLAG_PUBLISHER) && (apih->rwtasklet_info &&
          ((apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB)||
          (apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
        if (reg->kv_handle) {
          uint8_t *payload;
          size_t  payload_len;
          payload = protobuf_c_message_serialize(NULL, newobj->msg, &payload_len);
          rw_status_t rs = rwdts_kv_handle_add_keyval(reg->kv_handle, 0, (char *)newobj->key, newobj->key_len,
                                                     (char *)payload, (int)payload_len, rwdts_store_cache_set_callback, (void *)reg->kv_handle);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
          if (rs != RW_STATUS_SUCCESS) {
            /* Fail the VM and switchover to STANDBY */
          }
        }
      }
    } else if (mobj->delete_mark) {
      apih->stats.num_committed_delete_obj++;
      rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_DELETE);
      if (reg->shard && reg->shard->appdata) {
        rwdts_shard_handle_appdata_delete_element(reg->shard, mobj->keyspec, mobj->msg);
      } else {
        if (newobj != NULL) {

          rwdts_member_data_object_add_audit_trail(newobj,
                                                   RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                   RW_DTS_AUDIT_TRAIL_ACTION_OBJ_DELETE);
          HASH_DELETE(hh_data, (*obj_list_p), newobj);
          if ((reg->flags & RWDTS_FLAG_PUBLISHER) && (apih->rwtasklet_info &&
              ((apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB)||
              (apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
            if (reg->kv_handle) {
              rw_status_t rs = rwdts_kv_handle_del_keyval(reg->kv_handle, 0, (char *)newobj->key, newobj->key_len,
                                                          rwdts_store_cache_set_callback, (void *)reg->kv_handle);
              RW_ASSERT(rs == RW_STATUS_SUCCESS);
              if (rs != RW_STATUS_SUCCESS) {
                /* Fail the VM and switchover to STANDBY */
              }
            }
          }
          rw_status_t rs = rwdts_member_data_deinit(newobj);
          RW_ASSERT(rs == RW_STATUS_SUCCESS);
        }
      }
    } else if ((mobj->update_flag) || (newobj&&mobj->create_flag)) {
      RW_ASSERT(newobj != NULL);
      RW_ASSERT(mobj->keyspec);
      RW_ASSERT(newobj->keyspec);
      apih->stats.num_committed_update_obj++;
      if (mobj->msg) {
        /* Should a update value be sent to KV? */
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_UPDATE);

        if (reg->shard && reg->shard->appdata) {
          rwdts_shard_handle_appdata_update_element(reg->shard, mobj->keyspec, mobj->msg);
        } else {
          if (!mobj->replace_flag) {
            // merge(incoming/new, existing/old) => result in old
            protobuf_c_boolean ret = protobuf_c_message_merge(NULL, mobj->msg, newobj->msg);
            RW_ASSERT(ret == TRUE);
          } else {
            protobuf_c_message_free_unpacked(NULL, newobj->msg);
            newobj->msg =  protobuf_c_message_duplicate(NULL, mobj->msg, mobj->msg->descriptor);
          }
          rwdts_member_data_object_add_audit_trail(newobj,
                                                   RW_DTS_AUDIT_TRAIL_EVENT_TRANSACTION,
                                                   RW_DTS_AUDIT_TRAIL_ACTION_OBJ_UPDATE);
        }
        /* Should a update value be sent to KV? */
        rwdts_kv_update_db_xact_commit(mobj, RWDTS_QUERY_UPDATE);
        if ((reg->flags & RWDTS_FLAG_PUBLISHER) && (apih->rwtasklet_info &&
            ((apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_BDB) ||
            (apih->rwtasklet_info->data_store == RWVCS_TYPES_DATA_STORE_REDIS)))) {
          if (reg->kv_handle) {
            uint8_t *payload;
            size_t  payload_len;
            payload = protobuf_c_message_serialize(NULL, newobj->msg, &payload_len);
            rw_status_t rs = rwdts_kv_handle_add_keyval(reg->kv_handle, 0, (char *)newobj->key, newobj->key_len,
                                                       (char *)payload, (int)payload_len, rwdts_store_cache_set_callback,
                                                       (void *)reg->kv_handle);
            RW_ASSERT(rs == RW_STATUS_SUCCESS);
            if (rs != RW_STATUS_SUCCESS) {
              /* Fail the VM and switchover to STANDBY */
            }
          }
        }
      } else {
        RW_ASSERT_MESSAGE(0, "MSG NULL for update operation !!!!!!");
      }
    }
  }

  if (xact->flags&RWDTS_XACT_FLAG_SOLICIT_RSP) {
    rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_ADVISE, newobj);
  }

  return;
}

/*
 * Store a cache object
 */

rw_status_t
rwdts_store_cache_obj(rwdts_xact_t* xact)
{
  rwdts_member_data_object_t **xact_obj_p = NULL;
  rwdts_api_t *apih                       = NULL;

  rwdts_member_data_object_t *mobj, *tmp;

  apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  // Update the serials based on the xact
  rwdts_update_registration_serials(xact);

  xact_obj_p = &xact->pub_obj_list;
  HASH_ITER(hh_data, xact->pub_obj_list, mobj, tmp) {
    rwdts_reg_store_cache_obj(apih, xact, mobj);

    HASH_DELETE(hh_data, (*xact_obj_p), mobj);
    rw_status_t rs = rwdts_member_data_deinit(mobj);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  xact_obj_p = &xact->sub_obj_list;
  HASH_ITER(hh_data, xact->sub_obj_list, mobj, tmp) {
    rwdts_reg_store_cache_obj(apih, xact, mobj);

    HASH_DELETE(hh_data, (*xact_obj_p), mobj);
    rw_status_t rs = rwdts_member_data_deinit(mobj);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  return RW_STATUS_SUCCESS;
}

static
rwdts_group_t *rwdts_group_create(rwdts_api_t *apih,
                                  rwdts_group_cbset_t *cbset) {
  rwdts_group_t *grp = RW_MALLOC0_TYPE(sizeof(rwdts_group_t), rwdts_group_t);
  RW_ASSERT(cbset);
  grp->apih = apih;
  memcpy(&grp->cb, cbset, sizeof(grp->cb));

  RW_SKLIST_PARAMS_DECL(group_reg_list_,rwdts_group_reg_info_t, reg_id,
                        rw_sklist_comp_uint32_t, element);
  RW_SKLIST_INIT(&(grp->reg_list),&group_reg_list_);

  /* There is an internal ID, just an index, really, used to correlate
     transaction records to the apih-wide group object. */
  int id=0;
  if (apih->group_len) {
    RW_ASSERT(apih->group);
    for (id=0; id<apih->group_len; id++) {
      if (!apih->group[id]) {
        goto gotit;
      }
    }
    apih->group_len++;
    RW_ASSERT(apih->group_len == id + 1);
    apih->group = realloc(apih->group, sizeof(apih->group[0]) * apih->group_len);
    apih->group[id] = NULL;
  } else {
    apih->group_len = 1;
    apih->group = realloc(apih->group, sizeof(apih->group[0]) * apih->group_len);
    apih->group[id] = NULL;
  }

 gotit:
  grp->id = id;
  apih->group[id] = grp;

  return grp;
}

void rwdts_group_destroy(rwdts_group_t *grp) {
  RW_ASSERT(grp);
  RW_ASSERT_TYPE(grp, rwdts_group_t);

  rwdts_api_t *apih = grp->apih;

  rwdts_group_reg_info_t* g_reg = NULL, *n_reg = NULL;
  while((g_reg = RW_SKLIST_HEAD(&(grp->reg_list), rwdts_group_reg_info_t))) {
    RW_SKLIST_REMOVE_BY_KEY(&(grp->reg_list),
                            &g_reg->reg_id,
                            &n_reg);
    RW_ASSERT(n_reg == g_reg);
    RW_FREE(g_reg);
  }

  RW_ASSERT(apih->group);
  RW_ASSERT(grp->id < apih->group_len);
  RW_ASSERT(apih->group[grp->id] == grp);

  if (grp->cb.xact_init_dtor) {
    grp->cb.xact_init_dtor(grp->cb.ctx);
  }
  if (grp->cb.xact_deinit_dtor) {
    grp->cb.xact_deinit_dtor(grp->cb.ctx);
  }
  if (grp->cb.xact_event_dtor) {
    grp->cb.xact_event_dtor(grp->cb.ctx);
  }
  if (grp->xact) {
    rwdts_xact_unref(grp->xact, __PRETTY_FUNCTION__, __LINE__);
    grp->xact = NULL;
  }
  memset(&grp->cb, 0, sizeof(grp->cb));

  apih->group[grp->id] = NULL;
  RW_FREE_TYPE(grp, rwdts_group_t);
  grp = NULL;
}

void rwdts_group_phase_complete(rwdts_group_t *group,
                                enum rwdts_group_phase_e phase) {
  return;
}

static rw_status_t
rwdts_xact_query_pend_response_deinit(rwdts_xact_query_pend_rsp_t *xact_rsp)
{
  int i;

  RW_ASSERT(xact_rsp);
  if (!xact_rsp) {
    return RW_STATUS_FAILURE;
  }

  for (i = 0; i < xact_rsp->n_rsp; i++) {
    if (!xact_rsp->rsp[i]) {
      continue;
    }
    /* free keyspec */
    if (xact_rsp->rsp[i]->ks) {
      rw_keyspec_path_free(xact_rsp->rsp[i]->ks, NULL);
      xact_rsp->rsp[i]->ks = NULL;
    }
    /* free the protobufs if any */
    if (xact_rsp->rsp[i]->msgs) {
      int j;
      for (j = 0; j < xact_rsp->rsp[i]->n_msgs; j++) {
        protobuf_c_message_free_unpacked(NULL, xact_rsp->rsp[i]->msgs[j]);
        xact_rsp->rsp[i]->msgs[j] = NULL;
      }
      RW_FREE(xact_rsp->rsp[i]->msgs);
    }

    /* correlation id */
    if (xact_rsp->rsp[i]->corr_id) {
      RW_FREE(xact_rsp->rsp[i]->corr_id);
      xact_rsp->rsp[i]->corr_id_len = 0;
    }
    RW_FREE(xact_rsp->rsp[i]);
    xact_rsp->rsp[i] = NULL;
  }

  RW_FREE(xact_rsp->rsp);
  xact_rsp->rsp = NULL;
  RW_FREE(xact_rsp);
  return RW_STATUS_SUCCESS;
}

void
rwdts_dispatch_pending_query_response(void *ctx)
{
  rwdts_xact_query_pend_rsp_t *xact_rsp = (rwdts_xact_query_pend_rsp_t*) ctx;
  int i;

  RW_ASSERT(xact_rsp);
  rwdts_xact_t* xact = (rwdts_xact_t*) xact_rsp->xact;
  RW_ASSERT(xact);
  rwdts_api_t* apih = xact->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  RWDtsQuery* query = (RWDtsQuery*)xact_rsp->queryh;
  RW_ASSERT(query);
  uint32_t queryidx = query->queryidx;

  RW_ASSERT(xact_rsp->rsp);

  rwdts_xact_query_t *xquery = rwdts_xact_find_query_by_id(xact, queryidx);

  if (xquery == NULL) {
    goto finish;
  }

  for (i = 0; i < xact_rsp->n_rsp; i++)
  {
    rwdts_member_query_rsp_t *rsp = xact_rsp->rsp[i];
    if (!rsp) {
      continue;
    }

    /**
     * Find the matching  query in the transaction
     */

    RWDTS_API_LOG_XACT_DEBUG_EVENT(apih, xact, QUERY_RSP_DISPATCH, "Responding to query",
                             queryidx, query->corrid, (char*)rwdts_evtrsp_to_str(rsp[i]->evtrsp));

    // Copy the new results if the response contain messages
    RWDtsXactResult result;
    rwdts_xact_result__init(&result);
    RWDtsQueryResult *qres = NULL;

    if (xquery->qres) {
      qres = xquery->qres;
    } else {
      qres = (RWDtsQueryResult*) RW_MALLOC0(sizeof(RWDtsQueryResult));
      RW_ASSERT(qres);
      rwdts_query_result__init(qres);
      qres->has_evtrsp = 1;
      qres->evtrsp = rsp->evtrsp;
      qres->queryidx = queryidx;
      qres->has_queryidx = 1;
      if (query) {
        qres->has_corrid = query->has_corrid;
        qres->corrid = query->corrid;
      }
    }

    int j;
    for (j = 0; j < rsp->n_msgs; j++)
    {
      if (rsp->msgs[j])
      {
        if(rwdts_msg_reroot(xact, rsp->msgs[j], rsp->ks, xquery, qres)
           != RWDTS_RETURN_SUCCESS)
        {
          RWDTS_XACT_ABORT(xact, RW_STATUS_FAILURE, RWDTS_ERRSTR_MSG_REROOT);
          return;
        }
      }
      RWDTS_API_LOG_XACT_EVENT(apih, xact, RwDtsApiLog_notif_QueryAppendRsp, "Appending response to query",
                               queryidx, query->corrid, (char*)rwdts_evtrsp_to_str(rsp->evtrsp));
    }

    if (query && !(query->flags & RWDTS_XACT_FLAG_STREAM))
    {
      if (rwdts_merge_rsp_msgs(xact, xquery, qres, rsp) != RWDTS_RETURN_SUCCESS)
      {
        RWDTS_XACT_ABORT(xact, RW_STATUS_FAILURE, RWDTS_ERRSTR_RSP_MERGE);
        return;
      }
    }
    xquery->qres = qres;
  }
  xquery->xact_rsp_pending = NULL;

finish:
  rwdts_xact_query_pend_response_deinit(xact_rsp);
  xquery->xact_rsp_pending = NULL;
  if (apih->xact_timer) {
    rwsched_dispatch_source_cancel(apih->tasklet, apih->xact_timer);
    rwsched_dispatch_release(apih->tasklet, apih->xact_timer);
  }

  return;
}


rw_status_t
rwdts_api_group_create(rwdts_api_t*          apih,
                       xact_init_callback    xact_init,
                       xact_deinit_callback  xact_deinit,
                       xact_event_callback   xact_event,
                       void*                 ctx,
                       rwdts_group_t**       group)
{
  rwdts_group_cbset_t ac_cbset = {
    .xact_init = xact_init,
    .xact_deinit = xact_deinit,
    .xact_event = xact_event,
    .ctx = ctx,
  };
  *group = rwdts_group_create(apih,&ac_cbset);
  return (RW_STATUS_SUCCESS);
}

rw_status_t
rwdts_api_group_create_new(rwdts_api_t*          apih,
                           xact_init_callback    xact_init,
                           xact_deinit_callback  xact_deinit,
                           xact_event_callback   xact_event,
                           void*                 ctx,
                           rwdts_group_t**       group)
{
  rwdts_group_cbset_t ac_cbset = {
    .xact_init = xact_init,
    .xact_deinit = xact_deinit,
    .xact_event = xact_event,
    .ctx = ctx,
  };
  *group = rwdts_group_create(apih,&ac_cbset);
  return (RW_STATUS_SUCCESS);
}

rw_status_t
rwdts_api_group_create_gi(rwdts_api_t*          apih,
                          xact_init_callback    xact_init,
                          xact_deinit_callback  xact_deinit,
                          xact_event_callback   xact_event,
                          void*                 ctx,
        rwdts_group_t**       group,
                          GDestroyNotify xact_init_dtor,
                          GDestroyNotify xact_deinit_dtor,
                          GDestroyNotify xact_event_dtor)
{
  rwdts_group_cbset_t cbset = {
    .xact_init = xact_init,
    .xact_deinit = xact_deinit,
    .xact_event = xact_event,
    .ctx = ctx,
    .xact_init_dtor = xact_init_dtor,
    .xact_deinit_dtor = xact_deinit_dtor,
    .xact_event_dtor = xact_event_dtor
  };
  *group = rwdts_group_create(apih,&cbset);
  return (RW_STATUS_SUCCESS);
}


rw_status_t
rwdts_xact_info_send_error_xpath(const rwdts_xact_info_t*  xact_info,
                                 rw_status_t  status,
                                 const char*  xpath,
                                 const char*  errstr)
{
  const rw_yang_pb_schema_t* schema = NULL;
  rw_status_t                rs = RW_STATUS_SUCCESS;
  rw_keyspec_path_t*         keyspec = NULL;
  rwdts_api_t*               apih = NULL;

  RW_ASSERT(xact_info);
  if (!xact_info) {
    return(RW_STATUS_FAILURE);
  }
  apih = xact_info->apih;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (xpath) {

    /* Find the schema  */
    schema = rwdts_api_get_ypbc_schema(apih);

    if (!schema) {
      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "register_xpath[%s] failed - Schema is NULL", xpath);
      return(RW_STATUS_FAILURE);
    }

    /* Create keyspec from xpath */
    keyspec  = rw_keyspec_path_from_xpath(schema, (char*)xpath,
                                          RW_XPATH_KEYSPEC, &apih->ksi);
    if (!keyspec) {
      RWTRACE_CRIT(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "keyspec from xpath failed for xpath[%s]", xpath);
      return (RW_STATUS_FAILURE);
    }
  }

  rs =  rwdts_xact_info_send_error_keyspec(xact_info,
                                           status,
                                           keyspec,
                                           errstr);
  if (keyspec) {
    rw_keyspec_path_free(keyspec, NULL );
  }
  return rs;

}
rw_status_t
rwdts_xact_info_send_error_keyspec(const rwdts_xact_info_t* xact_info,
                                   rw_status_t status,
                                   const rw_keyspec_path_t* keyspec,
                                   const char *errstr)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RW_STATUS_FAILURE;
  }
  rwdts_match_info_t *queryh = NULL;
  if (xact_info->queryh) {
    queryh = (rwdts_match_info_t *)xact_info->queryh;
    if (queryh->reg) {
      queryh->reg->stats.send_error++;
    }
  }

  return rwdts_member_send_error(xact_info->xact,
                                 keyspec,
                                 (RWDtsQuery *)xact_info->queryh,
                                 xact_info->apih,
                                 NULL,
                                 status,
                                 errstr);
}

gboolean
rwdts_xact_info_transactional(const rwdts_xact_info_t *xact_info)
{
  return(rwdts_is_transactional(xact_info->xact));
}

rw_status_t
rwdts_xact_info_respond_xpath(const rwdts_xact_info_t* xact_info,
                              rwdts_xact_rsp_code_t    rsp_code,
                              const char*              xpath,
                              const ProtobufCMessage*  msg)
{

  const rw_yang_pb_schema_t* schema = NULL;
  rw_status_t                rs = RW_STATUS_SUCCESS;
  rw_keyspec_path_t*         keyspec = NULL;
  rwdts_api_t*               apih = NULL;

  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RW_STATUS_FAILURE;
  }
  apih = xact_info->apih;

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
  if (xpath) {
      keyspec = rw_keyspec_path_from_xpath(schema, (char*)xpath, RW_XPATH_KEYSPEC, &apih->ksi);
      if (!keyspec) {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "keyspec from xpath failed for xpath[%s]", xpath);
        return (RW_STATUS_FAILURE);
      }
  }

  rs =  rwdts_xact_info_respond_keyspec(xact_info,
                                        rsp_code,
                                        keyspec,
                                        msg);
  if (keyspec) {
      rw_keyspec_path_free(keyspec, NULL);
  }
  return rs;
}

rw_status_t
rwdts_xact_info_respond_keyspec(const rwdts_xact_info_t* xact_info,
                                rwdts_xact_rsp_code_t    rsp_code,
                                const rw_keyspec_path_t* keyspec,
                                const ProtobufCMessage*  msg)
{
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage* msgs[1];
  RW_ZERO_VARIABLE(&rsp);

  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RW_STATUS_FAILURE;
  }

  rsp.evtrsp = rwdts_xact_rsp_to_evtrsp(rsp_code);
  rsp.n_msgs = msg ? 1 : 0;
  rsp.msgs = msgs;
  rsp.ks = (rw_keyspec_path_t*)keyspec;
  msgs[0] = (ProtobufCMessage*)msg;
  return rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
}

static RWDtsEventRsp
rwdts_xact_rsp_to_evtrsp(rwdts_xact_rsp_code_t xrsp_code)
{
  RWDtsEventRsp evtrsp;

  switch(xrsp_code) {
    case RWDTS_XACT_RSP_CODE_ACK:
      evtrsp =  RWDTS_EVTRSP_ACK;
      break;
    case RWDTS_XACT_RSP_CODE_NACK:
      evtrsp = RWDTS_EVTRSP_NACK;
      break;
    case RWDTS_XACT_RSP_CODE_NA:
      evtrsp = RWDTS_EVTRSP_NA;
      break;
    case RWDTS_XACT_RSP_CODE_MORE:
      evtrsp =  RWDTS_EVTRSP_ASYNC;
      break;
    default:
     // Unknown xact_rsp_code
      RW_ASSERT_MESSAGE(0, "Unknown xact_rsp_code %u \n", xrsp_code);
  }
  return evtrsp;
}
void
rwdts_member_reg_handle_mark_internal(rwdts_member_reg_handle_t handle)
{
  RW_ASSERT_TYPE(handle, rwdts_member_registration_t);
  ((rwdts_member_registration_t*)handle)->dts_internal = 1;
}
