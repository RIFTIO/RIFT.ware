
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
 * @file rwdts_router_service.c
 * @author RIFT.io <info@riftio.com>
 * @date 2014/09/23
 * @brief DTS Router Services
 */

#include <rwdts_int.h>
#include <rwdts.h>
#include <rwdts_router.h>
#include <rwmemlogdts.h>
#include "rw-base.pb-c.h"
#include "rw-dts.pb-c.h"

static bool rwdts_router_check_and_queue_msg(RWDtsQueryRouter_Service *mysrv,
                                             const ProtobufCMessage *message,
                                             const ProtobufCMessageDescriptor *desc,
                                             const ProtobufCMessage *id,
                                             uint32_t flags,
                                             rwdts_router_t *dts,
                                             rwdts_router_queue_closure_t *queue_closure, 
                                             void *fn,
                                             rwmsg_request_t *rwreq);
static void
rwdts_router_get_shard_db_info(rwdts_router_t *dts,
                               char *member,
                               RWDtsGetDbShardInfoRsp *rsp);

static void rwdts_router_msg_regist(RWDtsQueryRouter_Service *mysrv,
                                    const RWDtsRegisterReq *input,
                                    void *self,
                                    RWDtsRegisterRsp_Closure clo,
                                    rwmsg_request_t *rwreq) {
  rwdts_router_t *dts = (rwdts_router_t *)self;
  RW_ASSERT_TYPE(dts, rwdts_router_t);


  /* jettison this in favor of mbr_regist, dunno why we ended up with two */
  dts->stats.req_rcv_regist++;

  RWDtsRegisterRsp rsp;
  rwdts_register_rsp__init(&rsp);

  clo(&rsp, rwreq);
}

#define RWDTS_FORWARD_CB(t_rsp, t_rwreq, t_ud, t_type) {\
  rwdts_FORWARD_##t_type##_cb_data_t *FORWARD_##t_type##_cb_data = (rwdts_FORWARD_##t_type##_cb_data_t *)ud;\
  RW_ASSERT_TYPE(FORWARD_##t_type##_cb_data, rwdts_FORWARD_##t_type##_cb_data_t);\
  if (rsp) { /* need to handle the bounce better */\
    FORWARD_##t_type##_cb_data->clo(rsp, FORWARD_##t_type##_cb_data->rwreq);\
    FORWARD_##t_type##_cb_data->dts->stats.req_rcv_##t_type##_fwdrsp++;\
    RWDTS_ROUTER_LOG_EVENT(FORWARD_##t_type##_cb_data->dts, DtsrouterDebug, \
                           RWLOG_ATTR_SPRINTF("%s::  _rwdts_forward_cb_%s for [%lu:%lu:%lu] ", \
                                              FORWARD_##t_type##_cb_data->dts->rwmsgpath, \
                                              #t_type, FORWARD_##t_type##_cb_data->router_idx, \
                                              FORWARD_##t_type##_cb_data->client_idx, \
                                              FORWARD_##t_type##_cb_data->serialno)); \
  } \
  RW_FREE_TYPE(FORWARD_##t_type##_cb_data, rwdts_FORWARD_##t_type##_cb_data_t); \
}

#define RWDTS_FORWARD_MSG(t_dts, t_input, t_rwreq, t_ret_clo, t_id, t_type) {\
  rwdts_FORWARD_##t_type##_cb_data_t *FORWARD_##t_type##_cb_data = \
      RW_MALLOC0_TYPE (sizeof(*FORWARD_##t_type##_cb_data), rwdts_FORWARD_##t_type##_cb_data_t); \
  RW_ASSERT_TYPE(FORWARD_##t_type##_cb_data, rwdts_FORWARD_##t_type##_cb_data_t); \
  FORWARD_##t_type##_cb_data->dts = t_dts;\
  FORWARD_##t_type##_cb_data->rwreq = t_rwreq;\
  FORWARD_##t_type##_cb_data->clo = t_ret_clo;\
  FORWARD_##t_type##_cb_data->router_idx = t_id->router_idx;\
  FORWARD_##t_type##_cb_data->client_idx = t_id->client_idx;\
  FORWARD_##t_type##_cb_data->serialno = t_id->serialno;\
  FORWARD_##t_type##_cb_data->clo = t_ret_clo;\
  rwmsg_closure_t clo = {\
    .pbrsp = (rwmsg_pbapi_cb)rwdts_FORWARD_##t_type##_cb, \
    .ud = FORWARD_##t_type##_cb_data\
  };\
  char dest_router_path[256] = {0};\
  snprintf(dest_router_path, 256, "/R/RW.DTSRouter/%ld", t_id->router_idx);\
  /*Create hash table of router dest */\
  rwmsg_destination_t *dest = rwmsg_destination_create(dts->ep,\
                                                       RWMSG_ADDRTYPE_UNICAST,\
                                                       dest_router_path);\
  rwdts_api_t *apih = dts->apih;\
  rw_status_t rs = rwdts_query_router__##t_type(&apih->client.service,\
                                               dest,\
                                               t_input,\
                                               &clo,\
                                               &FORWARD_##t_type##_cb_data->req_out);\
  RW_ASSERT(rs == RW_STATUS_SUCCESS);\
  RWDTS_ROUTER_LOG_EVENT(FORWARD_##t_type##_cb_data->dts, DtsrouterDebug, \
                         RWLOG_ATTR_SPRINTF("%s::  _rwdts_forward_msg_%s for [%lu:%lu:%lu] ", \
                                            FORWARD_##t_type##_cb_data->dts->rwmsgpath, \
                                            #t_type, FORWARD_##t_type##_cb_data->router_idx, \
                                            FORWARD_##t_type##_cb_data->client_idx, \
                                            FORWARD_##t_type##_cb_data->serialno)); \
  dts->stats.req_rcv_##t_type##_fwded++;\
  rwmsg_destination_destroy(dest);\
}

typedef struct rwdts_FORWARD_execute_cb_data_s {
  rwdts_router_t *dts;
  rwmsg_request_t *rwreq;
  rwmsg_request_t *req_out;
  RWDtsXactResult_Closure clo;
  uint64_t  router_idx;
  uint64_t  client_idx;
  uint64_t  serialno;
} rwdts_FORWARD_execute_cb_data_t;


static void rwdts_FORWARD_execute_cb(RWDtsXactResult* rsp,
                                     rwmsg_request_t*  rwreq,
                                     void*             ud)
{
  RWDTS_FORWARD_CB(rsp, rwreq, ud, execute);
}

static void rwdts_FORWARD_execute_to_router(rwdts_router_t *dts,
                                            const RWDtsXact *input,
                                            rwmsg_request_t *rwreq,
                                            RWDtsXactResult_Closure ret_clo,
                                            uint64_t router_idx)
{
  RWDTS_FORWARD_MSG(dts, input, rwreq, ret_clo, (&input->id), execute);
}

static void rwdts_router_msg_execute(RWDtsQueryRouter_Service *mysrv,
                                     const RWDtsXact *input,
                                     void *self,
                                     RWDtsXactResult_Closure clo,
                                     rwmsg_request_t *rwreq) {
  rwdts_router_t *dts = (rwdts_router_t *)self;
  RW_ASSERT_TYPE(dts, rwdts_router_t);
  rwdts_router_xact_t* xact = NULL;

  rwdts_router_queue_closure_t queue_closure = {
    .execute_clo = clo
  };
  if (rwdts_router_check_and_queue_msg(mysrv, 
                                       &input->base,
                                       input->base.descriptor,
                                       &input->id.base,
                                       input->flags,
                                       dts,
                                       &queue_closure,
                                       (void *)rwdts_router_msg_execute,
                                       rwreq)) {
    return;
  }

  if (!RWDTS_ROUTER_IS_LOCAL_XACT(dts, (&input->id))) {
    rwdts_FORWARD_execute_to_router(dts, input, rwreq, clo, input->id.router_idx);
    return;
  }
  dts->stats.req_rcv_execute++;

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, "RWDtsRouter got execute, Call rwdts_router_xact_create()");
  xact = rwdts_router_xact_lookup_by_id(dts, &input->id);
  if (xact) {

    rwdts_router_xact_update(xact, rwreq, (RWDtsXact*)input);
    RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, RwDtsRouterLog_notif_DtsrouterXactDebug,
                                "_router_msg_execute --- __xact_update");
    rwdts_router_xact_run(xact);

  } else {
    if (input->n_subx) {
      rwdts_router_xact_create(dts, rwreq, (RWDtsXact*)input);
    } else {
      RWDtsXactResult result;
      rwdts_xact_result__init(&result);
      protobuf_c_message_memcpy(&result.id.base , &input->id.base);
      result.has_status = TRUE;
      RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, "RWDtsRouter got empty execute message for unknown xact, summarily returning committed");
      result.status = RWDTS_XACT_COMMITTED;
      clo(&result, rwreq);
    }
  }
}

typedef struct rwdts_FORWARD_end_cb_data_s {
  rwdts_router_t *dts;
  rwmsg_request_t *rwreq;
  rwmsg_request_t *req_out;
  RWDtsStatus_Closure clo;
  uint64_t  router_idx;
  uint64_t  client_idx;
  uint64_t  serialno;
} rwdts_FORWARD_end_cb_data_t;


static void rwdts_FORWARD_end_cb(RWDtsStatus* rsp,
                                 rwmsg_request_t*  rwreq,
                                 void*             ud)
{
  RWDTS_FORWARD_CB(rsp, rwreq, ud, end);
}

static void rwdts_FORWARD_end_to_router(rwdts_router_t *dts,
                                        const RWDtsXactID *input,
                                        rwmsg_request_t *rwreq,
                                        RWDtsStatus_Closure ret_clo,
                                        uint64_t router_idx)
{
  RWDTS_FORWARD_MSG(dts, input, rwreq, ret_clo, input, end);
}

static void rwdts_router_msg_end(RWDtsQueryRouter_Service *mysrv,
                                 const RWDtsXactID *input,
                                 void *self,
                                 RWDtsStatus_Closure clo,
                                 rwmsg_request_t *rwreq)
{
  rwdts_router_t *dts = (rwdts_router_t *)self;
  rwdts_router_xact_t* xact = NULL;
  bool router_xact_run = TRUE;

  rwdts_router_queue_closure_t queue_closure = {
    .status_clo = clo
  };
  if (rwdts_router_check_and_queue_msg(mysrv, 
                                       &input->base,
                                       input->base.descriptor,
                                       &input->base,
                                       0,
                                       dts,
                                       &queue_closure,
                                       (void *)rwdts_router_msg_end,
                                       rwreq)) {
    return;
  }


  if (!RWDTS_ROUTER_IS_LOCAL_XACT(dts, input)) {
    rwdts_FORWARD_end_to_router(dts, input, rwreq, clo, input->router_idx);
    return;
  }

  xact = rwdts_router_xact_lookup_by_id(dts, input);

  dts->stats.req_rcv_end++;

  RWDtsStatus rsp;
  rwdts_status__init(&rsp);

  rsp.id = (RWDtsXactID *)input;
  rsp.has_status = TRUE;

  if (xact) {

    rwmsg_request_client_id_t rwmsg_cliid;
    rw_status_t rs = rwmsg_request_get_request_client_id(rwreq, &rwmsg_cliid);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    if (memcmp(&rwmsg_cliid, &xact->rwmsg_tab[0].rwmsg_cliid, sizeof(rwmsg_cliid))) {
      RWDTS_ROUTER_LOG_EVENT(
          dts, 
          AbortStopFromMember, 
          "Someone who is not the originator of the transaction has tried to end it, aborting!");
      xact->abrt = TRUE;
    } else {
      xact->ended = TRUE;
      RWDTS_RTR_ADD_TR_ENT_ENDED(xact);
      if (xact->move_to_precomm) {
        rwdts_router_xact_move_to_precomm(xact);
      } else {
        if (!xact->rwmsg_tab[0].rwmsg_req) {
          RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, RwDtsRouterLog_notif_DtsrouterXactDebug,
                                      "NO Pending Req for rwdts_router_xact_run");
          router_xact_run = FALSE;
        }
      }
    }

    rsp.status = RWDTS_QUERY_STATUS_ACK;
    RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, RwDtsRouterLog_notif_DtsrouterXactDebug,
                                "RWDtsRouter Rcvd End from the client");
  } else {
    rsp.status = RWDTS_QUERY_STATUS_NACK;
  }

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, "RWDtsRouter Rcvd End from the client");
  clo(&rsp, rwreq);

  if (xact && router_xact_run) {
    rwdts_router_xact_run(xact);
  }
}

typedef struct rwdts_FORWARD_abort_cb_data_s {
  rwdts_router_t *dts;
  rwmsg_request_t *rwreq;
  rwmsg_request_t *req_out;
  RWDtsStatus_Closure clo;
  uint64_t  router_idx;
  uint64_t  client_idx;
  uint64_t  serialno;
} rwdts_FORWARD_abort_cb_data_t;


static void rwdts_FORWARD_abort_cb(RWDtsStatus* rsp,
                                 rwmsg_request_t*  rwreq,
                                 void*             ud)
{
  RWDTS_FORWARD_CB(rsp, rwreq, ud, abort);
}

static void rwdts_FORWARD_abort_to_router(rwdts_router_t *dts,
                                        const RWDtsXactID *input,
                                        rwmsg_request_t *rwreq,
                                        RWDtsStatus_Closure ret_clo,
                                        uint64_t router_idx)
{
  RWDTS_FORWARD_MSG(dts, input, rwreq, ret_clo, input, abort);
}

static void rwdts_router_msg_abort(RWDtsQueryRouter_Service *mysrv,
                                   const RWDtsXactID *input,
                                   void *self,
                                   RWDtsStatus_Closure clo,
                                   rwmsg_request_t *rwreq) {
  rwdts_router_t *dts = (rwdts_router_t *)self;
  rwdts_router_xact_t* xact = NULL;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  rwdts_router_queue_closure_t queue_closure = {
    .status_clo = clo
  };
  if (rwdts_router_check_and_queue_msg(mysrv, 
                                       &input->base,
                                       input->base.descriptor,
                                       &input->base,
                                       0,
                                       dts,
                                       &queue_closure,
                                       (void *)rwdts_router_msg_abort,
                                       rwreq)) {
    return;
  }

  if (!RWDTS_ROUTER_IS_LOCAL_XACT(dts, input)) {
    rwdts_FORWARD_abort_to_router(dts, input, rwreq, clo, input->router_idx);
    return;
  }

  dts->stats.req_rcv_abort++;

  xact = rwdts_router_xact_lookup_by_id(dts, input);

  if (xact) {
    xact->abrt = TRUE;
  }

  RWDtsStatus rsp;
  rwdts_status__init(&rsp);

  rsp.id = (RWDtsXactID *)input;
  rsp.has_status = TRUE;

  if (xact) {
    xact->abrt = true;
    rsp.status = RWDTS_QUERY_STATUS_ACK;
    RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, RwDtsRouterLog_notif_DtsrouterXactDebug,
                                "RWDtsRouter Rcvd Abort from the client");
    rwdts_router_xact_run(xact);
  } else {
    rsp.status = RWDTS_QUERY_STATUS_NACK;
  }

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, "RWDtsRouter Rcvd Abort from the client");
  clo(&rsp, rwreq);
}

typedef struct rwdts_FORWARD_flush_cb_data_s {
  rwdts_router_t *dts;
  rwmsg_request_t *rwreq;
  rwmsg_request_t *req_out;
  RWDtsStatus_Closure clo;
  uint64_t  router_idx;
  uint64_t  client_idx;
  uint64_t  serialno;
} rwdts_FORWARD_flush_cb_data_t;


static void rwdts_FORWARD_flush_cb(RWDtsStatus* rsp,
                                 rwmsg_request_t*  rwreq,
                                 void*             ud)
{
  RWDTS_FORWARD_CB(rsp, rwreq, ud, flush);
}

static void rwdts_FORWARD_flush_to_router(rwdts_router_t *dts,
                                        const RWDtsXactID *input,
                                        rwmsg_request_t *rwreq,
                                        RWDtsStatus_Closure ret_clo,
                                        uint64_t router_idx)
{
  RWDTS_FORWARD_MSG(dts, input, rwreq, ret_clo, input, flush);
}

static void rwdts_router_msg_flush(RWDtsQueryRouter_Service *mysrv,
                                   const RWDtsXactID *input,
                                   void *self,
                                   RWDtsStatus_Closure clo,
                                   rwmsg_request_t *rwreq) {
  rwdts_router_t *dts = (rwdts_router_t *)self;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  rwdts_router_queue_closure_t queue_closure = {
    .status_clo = clo
  };
  if (rwdts_router_check_and_queue_msg(mysrv, 
                                       &input->base,
                                       input->base.descriptor,
                                       &input->base,
                                       0,
                                       dts,
                                       &queue_closure,
                                       (void *)rwdts_router_msg_flush,
                                       rwreq)) {
    return;
  }

  if (!RWDTS_ROUTER_IS_LOCAL_XACT(dts, input)) {
    rwdts_FORWARD_flush_to_router(dts, input, rwreq, clo, input->router_idx);
    return;
  }
  dts->stats.req_rcv_flush++;

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, "RWDtsRouter got flush, goes nowhere");
  clo(NULL, rwreq);
}

static void rwdts_router_mbr_msg_get_db_shard_info(RWDtsQueryRouter_Service *mysrv,
                                                   const RWDtsGetDbShardInfoReq *input,
                                                   void *self,
                                                   RWDtsGetDbShardInfoRsp_Closure clo,
                                                   rwmsg_request_t *rwreq)
{
  rwdts_router_t *dts = (rwdts_router_t *)self;
  RW_ASSERT_TYPE(dts, rwdts_router_t);
  int i;
  rw_status_t rs = RW_STATUS_SUCCESS;

  RWDtsGetDbShardInfoRsp rsp;
  rwdts_get_db_shard_info_rsp__init(&rsp);

  rwdts_router_get_shard_db_info(dts, input->member, &rsp);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  clo(&rsp, rwreq);
  for (i=0; i<rsp.n_responses; i++) {
    RW_FREE(rsp.responses[i]);
  }
  if (rsp.responses) {
    free(rsp.responses);
  }
  return;
}

void rwdts_router_service_init_f(void *ud) {
  rwdts_router_t *dts = (rwdts_router_t *)ud;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  rw_status_t rs = RW_STATUS_SUCCESS;

  dts->srvchan = rwmsg_srvchan_create(dts->ep);
  RW_ASSERT(dts->srvchan);
  if (!dts->srvchan) {
    goto err;
  }
  rs = rwmsg_srvchan_bind_rwsched_queue(dts->srvchan, dts->rwq);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  if (rs != RW_STATUS_SUCCESS) {
    goto err;
  }
  RWDTS_QUERY_ROUTER__INITSERVER(&dts->querysrv, rwdts_router_msg_);
  rs = rwmsg_srvchan_add_service(dts->srvchan, dts->rwmsgpath, &dts->querysrv.base, dts);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, 
                         RWLOG_ATTR_SPRINTF("Router registered query service as '%s'", dts->rwmsgpath));
  if (rs != RW_STATUS_SUCCESS) {
    goto err;
  }
  RWDTS_MEMBER_ROUTER__INITSERVER(&dts->membersrv, rwdts_router_mbr_msg_);
  rs = rwmsg_srvchan_add_service(dts->srvchan, dts->rwmsgpath, &dts->membersrv.base, dts);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug,
                         RWLOG_ATTR_SPRINTF("Router registered member service as '%s'", dts->rwmsgpath));
  if (rs != RW_STATUS_SUCCESS) {
    goto err;
  }

  RWDTS_MEMBER__INITCLIENT(&dts->membercli);
  dts->clichan = rwmsg_clichan_create(dts->ep);
  RW_ASSERT(dts->clichan);
  if (!dts->clichan) {
    goto err;
  }
  rs = rwmsg_clichan_bind_rwsched_queue(dts->clichan, dts->rwq);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  if (rs != RW_STATUS_SUCCESS) {
    goto err;
  }
  rwmsg_clichan_add_service(dts->clichan, &dts->membercli.base);
  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug,
                         "Router bound client channel");

  return;

 err:
  return;
}
rw_status_t rwdts_router_service_init(rwdts_router_t *dts) {
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  if (dts->rwq_ismain) {
    rwdts_router_service_init_f(dts);
  } else {
    rwsched_dispatch_async_f(dts->taskletinfo,
                             dts->rwq,
                             dts,
                             rwdts_router_service_init_f);
  }
  return RW_STATUS_SUCCESS;
}


static void rwdts_router_service_deinit_f(void *ud)
{
  rwdts_router_t *dts = (rwdts_router_t *)ud;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  if (dts->rwmemlog) {
    rwmemlog_instance_dts_deregister(dts->rwmemlog, true/*dts_internal, RIFT-8791*/);
    dts->rwmemlog = NULL;
  }

  if (dts->stat_timer) {
    rwsched_dispatch_source_cancel(dts->rwtaskletinfo->rwsched_tasklet_info,
                                   dts->stat_timer);
    rwsched_dispatch_release(dts->rwtaskletinfo->rwsched_tasklet_info,
                             dts->stat_timer);
    dts->stat_timer = NULL;
  }

  rwdts_api_deinit(dts->apih);
  dts->apih = NULL;

  rwdts_shard_tab_t *shard_tab = NULL, *tmp1 = NULL;
  rwdts_router_delete_shard_db_info(dts);
  HASH_ITER(hh_shard, dts->shard_tab, shard_tab, tmp1) {
    HASH_DELETE(hh_shard, dts->shard_tab, shard_tab);
    RW_FREE_TYPE(shard_tab, rwdts_shard_tab_t);
  }

  if (dts->srvchan) {
    rwmsg_srvchan_halt(dts->srvchan);
    dts->srvchan = NULL;
  }
  if (dts->clichan) {
    rwmsg_clichan_halt(dts->clichan);
    dts->clichan = NULL;
  }

  rwdts_router_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
  HASH_ITER(hh, dts->xacts, xact_entry, tmp_xact_entry) {
    rwdts_router_xact_destroy(dts, xact_entry);
  }

  rwdts_router_member_t *memb=NULL, *membnext=NULL;
  HASH_ITER(hh, dts->members, memb, membnext) {
    rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
    HASH_ITER(hh_reg, memb->registrations, reg, tmp_reg) {
      HASH_DELETE(hh_reg, memb->registrations, reg);
      if (reg->keyspec) {
        rw_keyspec_path_free(reg->keyspec, NULL);
        reg->keyspec = NULL;
      }
      if (reg->ks_binpath) {
        RW_FREE(reg->ks_binpath);
      }
      RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
    }
    HASH_ITER(hh_reg, memb->sub_registrations, reg, tmp_reg) {
      HASH_DELETE(hh_reg, memb->sub_registrations, reg);
      if (reg->keyspec) {
        rw_keyspec_path_free(reg->keyspec, NULL);
        reg->keyspec = NULL;
      }
      if (reg->ks_binpath) {
        RW_FREE(reg->ks_binpath);
      }
      RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
    }
    HASH_DELETE(hh, dts->members, memb);
    // TBD fix this 
    //protobuf_c_message_free_unpacked_usebody(NULL, &(dts->payload_stats.base));
    if (dts->payload_stats.role) {
      int i;
      for (i=0; i < dts->payload_stats.n_role; i++) {
        int k;
        for (k=0; k < dts->payload_stats.role[i]->n_action; k++) {
          int l;
          for (l=0; l < dts->payload_stats.role[i]->action[k]->n_levels; l++) {
            int m;
            for (m=0; m < dts->payload_stats.role[i]->action[k]->levels[l]->n_buckets; m++) {
              RW_FREE(dts->payload_stats.role[i]->action[k]->levels[l]->buckets[m]);
              dts->payload_stats.role[i]->action[k]->levels[l]->buckets[m] = NULL;
            }
            RW_FREE(dts->payload_stats.role[i]->action[k]->levels[l]);
            dts->payload_stats.role[i]->action[k]->levels[l] = NULL;
          }
          RW_FREE(dts->payload_stats.role[i]->action[k]);
          dts->payload_stats.role[i]->action[k] = NULL;
        }
        RW_FREE(dts->payload_stats.role[i]);
        dts->payload_stats.role[i] = NULL;
      }
      RW_FREE(dts->payload_stats.role); 
      dts->payload_stats.role = NULL;
    }
    free(memb->msgpath);
    memb->msgpath = NULL;
    if (memb->dest) {
      rwmsg_destination_destroy(memb->dest);
    }
    memb->dest = NULL;
    RW_FREE_TYPE(memb, rwdts_router_member_t);
  }

  if (!dts->rwq_ismain) {
    rwsched_dispatch_release(dts->rwtaskletinfo->rwsched_tasklet_info, dts->rwq);
  }

  if (dts->rwmemlog)  {
    rwmemlog_instance_destroy(dts->rwmemlog);
    dts->rwmemlog = NULL;
  }

  free((void*)dts->rwmsgpath);
  dts->rwmsgpath = NULL;
  //DUMP_CREDIT_OBJ(dts->credits);
  RW_FREE_TYPE(dts, rwdts_router_t);
  dts = NULL;
}
rw_status_t rwdts_router_service_deinit(rwdts_router_t *dts) {
  rwsched_dispatch_async_f(dts->taskletinfo,
                           dts->rwq,
                           dts,
                           rwdts_router_service_deinit_f);
  return RW_STATUS_SUCCESS;
}

void
rwdts_router_member_registration_init(rwdts_router_t *                  dts,
                                      rwdts_router_member_t*            memb,
                                      rw_keyspec_path_t*                keyspec,
                                      const char*                       keystr,
                                      uint32_t                          flags,
                                      bool                              create)
{
  rwdts_memb_router_registration_t *v = NULL;
  uint8_t *ks_binpath = NULL;
  size_t ks_binpath_len;

  rwdts_memb_router_registration_t *reg = RW_MALLOC0_TYPE(sizeof(rwdts_memb_router_registration_t),
                                                     rwdts_memb_router_registration_t);
  RW_ASSERT_TYPE(reg, rwdts_memb_router_registration_t);

  rw_status_t rs  = rw_keyspec_path_create_dup(keyspec, &dts->ksi, &(reg->keyspec));
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  if (keystr) {
    strncpy(reg->keystr, keystr, sizeof(reg->keystr));
    reg->keystr[sizeof(reg->keystr)-1]='\0';
  }

  reg->flags = flags;

  rs = rw_keyspec_path_get_binpath(reg->keyspec, &dts->ksi, (const uint8_t **)&ks_binpath, &ks_binpath_len);
  reg->ks_binpath = RW_MALLOC0(ks_binpath_len);
  memcpy((char *)reg->ks_binpath, ks_binpath, ks_binpath_len);

  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  RW_ASSERT(reg->ks_binpath);


  if (flags & RWDTS_FLAG_PUBLISHER) {
    HASH_FIND(hh_reg, memb->registrations, reg->ks_binpath, reg->ks_binpath_len, v);
    if (v) {
      HASH_DELETE(hh_reg, memb->registrations, v);
    }
  } else {
    HASH_FIND(hh_reg, memb->sub_registrations, reg->ks_binpath, reg->ks_binpath_len, v);
    if (v) {
      HASH_DELETE(hh_reg, memb->sub_registrations, v);
    }
  }

  if (v) {
    RW_ASSERT(v);
    if (v->ks_binpath) {
      RW_FREE(v->ks_binpath);
    }
    if (v->keyspec) {
      rw_keyspec_path_free(v->keyspec, NULL);
    }
    RW_FREE_TYPE(v, rwdts_memb_router_registration_t);
  }

  reg->rtr_reg_id = dts->rtr_reg_id++;
#ifdef RWDTS_SHARDING
  reg->shard = rwdts_shard_init_keyspec(keyspec,-1, &dts->rootshard,
                                        RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
                                        RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  RW_ASSERT(reg->shard);
  rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg->shard,NULL);
  RW_ASSERT(chunk);
  rwdts_chunk_rtr_info_t rtr_info; 
  rtr_info.member = memb;
  rtr_info.flags = flags;
#endif

  // Check if this is already registered and if so update.
  if (flags & RWDTS_FLAG_PUBLISHER) {
    HASH_ADD_KEYPTR(hh_reg, memb->registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
#ifdef RWDTS_SHARDING
    rwdts_rts_shard_create_element(reg->shard, chunk, &rtr_info, true, &reg->chunk_id, &reg->membid,rtr_info.member->msgpath);
#endif
    
  } else {
    HASH_ADD_KEYPTR(hh_reg, memb->sub_registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
#ifdef RWDTS_SHARDING
    rwdts_rts_shard_create_element(reg->shard, chunk, &rtr_info, false, &reg->chunk_id, &reg->membid, rtr_info.member->msgpath);
#endif

  }

  if (create) {
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) store_data;
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member,&store_data);
  strncpy(store_data.name, memb->msgpath, sizeof(store_data.name));
  store_data.has_name = 1;
  rw_keyspec_path_t *keyspec1 = NULL;

  store_data.n_registration = 1;
  store_data.registration = RW_MALLOC0(sizeof(store_data.registration[0]));

  RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) ks  = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member));

  keyspec1 = (rw_keyspec_path_t*)&ks;
  ks.has_dompath = TRUE;
  ks.dompath.has_path001 = TRUE;
  ks.dompath.path001.has_key00 = TRUE;
  strncpy(ks.dompath.path001.key00.name, memb->msgpath, sizeof(ks.dompath.path001.key00.name));

  store_data.registration[0] = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration)));
  RWPB_F_MSG_INIT(RwDts_data_RtrInitRegKeyspec_Member_Registration, store_data.registration[0]);
  store_data.registration[0]->keystr = (char *)keystr;
  store_data.registration[0]->has_keyspec = 1;
  store_data.registration[0]->keyspec.len = reg->ks_binpath_len;
  store_data.registration[0]->keyspec.data = reg->ks_binpath;
  store_data.registration[0]->flags = flags;
  store_data.registration[0]->has_flags = 1;
  store_data.registration[0]->has_flavor = 1;
  store_data.registration[0]->flavor = RW_DTS_SHARD_FLAVOR_NULL;

  RWTRACE_INFO(dts->apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "Adding member %s into table", memb->msgpath);
  rs = rwdts_member_reg_handle_create_element_keyspec(
                                                   dts->init_regkeyh,
                                                   keyspec1,
                                                   &store_data.base,
                                                   NULL);
  RW_FREE(store_data.registration[0]);
  RW_FREE(store_data.registration);
  }

  return;
}


static void
rwdts_router_get_shard_db_info(rwdts_router_t *dts,
                               char *member,
                               RWDtsGetDbShardInfoRsp *rsp)
{
  RW_ASSERT(dts);
  rwdts_shard_tab_t *shard_tab = NULL, *temp_shard_tab = NULL;
  size_t rsz;
  RWDtsDbShardInfoRsp *db_shard_info = NULL;

  HASH_ITER(hh_shard, dts->shard_tab, shard_tab, temp_shard_tab) {
    int i;
    for (i=0; i < shard_tab->member_len; i++) {
      if (!strcmp(shard_tab->member[i].msgpath, member)) {
        int j;
        rsz = ((rsp->n_responses + shard_tab->member[i].registration_detail_len) * sizeof(rsp->responses[0]));
        rsp->responses = realloc(rsp->responses, rsz);

        for (j=0; j < shard_tab->member[i].registration_detail_len; j++) {
          rsp->n_responses++;
          db_shard_info = RW_MALLOC(sizeof(RWDtsDbShardInfoRsp));
          rwdts_db_shard_info_rsp__init(db_shard_info);
          rsp->responses[rsp->n_responses-1] = db_shard_info;
          rsp->responses[rsp->n_responses-1]->db_number = shard_tab->table_id;
          rsp->responses[rsp->n_responses-1]->has_db_number = 1;
          strcpy(rsp->responses[rsp->n_responses-1]->shard_chunk_id, "What");
          //rsp->responses[rsp->n_responses-1]->shard_chunk_id = shard_tab->member[i].registration_detail_tab[j].shard_chunk_id;
          //rsp->responses[rsp->n_responses-1]->has_shard_chunk_id = 1;
        }
        break;
      }
    }
  }
  return;
}

void
rwdts_router_delete_shard_db_info(rwdts_router_t*  dts)
{
  RW_ASSERT(dts);
  rwdts_shard_tab_t *shard_tab = NULL, *temp_shard_tab = NULL;

  HASH_ITER(hh_shard, dts->shard_tab, shard_tab, temp_shard_tab) {
    int i, j;
    for (i=0; i < shard_tab->member_len; i++) {
      if (shard_tab->member[i].msgpath) {
        free(shard_tab->member[i].msgpath);
      }
      for (j=0; j < shard_tab->member[i].registration_detail_len; j++) {
        if (shard_tab->member[i].registration_detail_tab[j].shard_key_detail.u.byte_key.k.key) {
          free(shard_tab->member[i].registration_detail_tab[j].shard_key_detail.u.byte_key.k.key);
        }
      }
      if (shard_tab->member[i].registration_detail_tab) {
        free(shard_tab->member[i].registration_detail_tab);
      }
    }
    if (shard_tab->member) {
      free(shard_tab->member);
    }
    for (i=0; i<shard_tab->tab_len; i++) {
      if (shard_tab->tab[i].map.shard_key_detail.u.byte_key.k.key) {
        free(shard_tab->tab[i].map.shard_key_detail.u.byte_key.k.key);
      }
      if (shard_tab->tab[i].member_tab) {
        free(shard_tab->tab[i].member_tab);
      }
    }
    if (shard_tab->tab) {
      free(shard_tab->tab);
    }
  }
  return;
}

void rwdts_router_replay_queued_msgs(rwdts_router_t *dts)
{
  rwdts_router_queue_msg_t *queued_msg = dts->queued_msgs;
  rwdts_router_queue_msg_t *next_queued_msg;
  dts->queued_msgs =
  dts->queued_msgs_tail = NULL;

  while (queued_msg) {
    RW_ASSERT_TYPE(queued_msg, rwdts_router_queue_msg_t);
    next_queued_msg = queued_msg->next_msg;
  
    if (queued_msg->fn == rwdts_router_msg_execute) {
      ((rwdts_router_msg_queued_execute_fptr)queued_msg->fn) (queued_msg->mysrv,
                                                              (const RWDtsXact *)queued_msg->input,
                                                              (void *)dts,
                                                              queued_msg->clo.execute_clo,
                                                              queued_msg->rwreq);
    }
    else {
      ((rwdts_router_msg_queued_general_fptr)queued_msg->fn) (queued_msg->mysrv,
                                                              (const RWDtsXactID *)queued_msg->input,
                                                              (void *)dts,
                                                              queued_msg->clo.status_clo,
                                                              queued_msg->rwreq);
    }

    RW_FREE_TYPE(queued_msg, rwdts_router_queue_msg_t);
    queued_msg = next_queued_msg;
  }
}

static bool rwdts_router_check_and_queue_msg(RWDtsQueryRouter_Service *mysrv,
                                             const ProtobufCMessage *message,
                                             const ProtobufCMessageDescriptor *desc,
                                             const ProtobufCMessage *id,
                                             uint32_t flags,
                                             rwdts_router_t *dts,
                                             rwdts_router_queue_closure_t *queue_closure, 
                                             void *fn,
                                             rwmsg_request_t *rwreq)
{
  /* Peer Reg Reads are passed through */
  if (flags & RWDTS_XACT_FLAG_PEER_REG) {
    return false;
  }
  /* Registration May need to handle this better */
  if (flags & RWDTS_XACT_FLAG_REG) {
    return false;
  }
  else if (dts->pend_peer_table || dts->queued_msgs_tail) {
    rwdts_router_queue_msg_t *queue_msg = 
        RW_MALLOC0_TYPE(sizeof(*queue_msg), rwdts_router_queue_msg_t);
    RW_ASSERT_TYPE(queue_msg, rwdts_router_queue_msg_t);
    queue_msg->mysrv = mysrv;
    queue_msg->input = protobuf_c_message_duplicate(NULL, message, desc);
    queue_msg->clo.execute_clo = queue_closure->execute_clo;
    queue_msg->fn = fn;
    queue_msg->rwreq = rwreq;
    if (dts->queued_msgs_tail) {
      RW_ASSERT(dts->queued_msgs != NULL);
      dts->queued_msgs_tail->next_msg = queue_msg;
      dts->queued_msgs_tail = queue_msg;
    }
    else {
      RW_ASSERT(dts->queued_msgs == NULL);
      RW_ASSERT(dts->queued_msgs_tail == NULL);
      dts->queued_msgs = queue_msg;
      dts->queued_msgs_tail = queue_msg;
    }
    return true;
  }
  return false;
}

