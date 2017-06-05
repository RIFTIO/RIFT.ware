/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_router.c
 * @author RIFT.io (info@riftio.com)
 * @date 2014/01/26
 * @brief DTS Router
 */

#include <rwtasklet.h>
#include <rwdts_int.h>
#include <rwdts.h>
#include <rwdts_router.h>
#include <rwdts_router_tasklet.h>
#include <rwmemlogdts.h>
#include "rw-base.pb-c.h"
#include "rw-dts.pb-c.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <rwvcs_rwzk.h>

// Exclude from covergae
//LCOV_EXCL_START

static void rwdts_router_calc_stat_timer(void *ctx);
static void rwdts_router_publish_state_handler(
    rwdts_router_t *dts,
    RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state,
    RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *keyspec_entry);
static void rwdts_router_publish_state_reg_ready(
    rwdts_member_reg_handle_t  regh,
    rw_status_t                rs,
    void*                      user_data);
static rwdts_member_rsp_code_t rwdts_router_publish_state_prepare(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* keyspec,
    const ProtobufCMessage*  msg,
    void*                    user_data);


static void
rwdts_restart_begin_notify(rwdts_api_t *apih, char *memb) ;
static void
rwdts_restart_end_notify(rwdts_api_t *apih, char *memb) ;



/*************************************************************************************************
 *                               STALE -XACT processing
 *************************************************************************************************/
rwdts_router_stale_xact_t *rwdts_router_find_stale_xact(const rwdts_router_t* dts,
                                                        const RWDtsXactID* id) {
  rwdts_router_stale_xact_t *xact = NULL;
  RWDTS_FIND_WITH_PBKEY_IN_HASH(hh, dts->stale_xacts, id, router_idx, xact);
  return xact;
}

static void rwdts_router_stale_xact_delete(rwdts_router_t *dts,
                                           struct timeval *end_time)
{
  rwdts_router_stale_xact_t *sxact, *next, *txact;
  struct timeval delta;
  
  /*Walk through the stale xacts and delete all that have existted for 10min*/
  sxact = RW_DL_HEAD(&dts->stale_xacts_head, rwdts_router_stale_xact_t, time_element);
  while (sxact){
    next = RW_DL_NEXT(sxact, rwdts_router_stale_xact_t, time_element);
    timersub(end_time, &sxact->end_time, &delta);
    if ((dts->stats.stale_xact_ct > RWDTS_STALE_XACT_MAX_ELEM) ||
        ((((uint64_t)delta.tv_usec * 1000) + ((uint64_t)delta.tv_sec * NSEC_PER_SEC)) >=
         RWDTS_TIMEOUT_QUANTUM_MULTIPLE(dts->apih, RWDTS_STALE_XACT_TIMEOUT_SCALE))){
      /*Can be deleted..*/
      RW_DL_REMOVE(&(dts->stale_xacts_head), sxact, time_element);
      //make sure to delete the id from the hash
      RWDTS_FIND_WITH_PBKEY_IN_HASH(hh, dts->stale_xacts, &sxact->id, router_idx, txact);
      RW_ASSERT(txact == sxact);
      HASH_DELETE(hh, dts->stale_xacts, sxact);
      RW_FREE_TYPE(sxact, rwdts_router_stale_xact_t);
      dts->stats.stale_xact_ct--;
    }else{
      break;
    }
    sxact = next;
  }
          
  return;
}

rw_status_t
rwdts_router_add_stale_xact(rwdts_router_t *dts,
                            rwdts_router_xact_t *xact,
                            struct timeval *end_time)
{
  rwdts_router_stale_xact_t *sxact;

  RWDTS_FIND_WITH_PBKEY_IN_HASH(hh, dts->stale_xacts, &xact->id, router_idx, sxact);
  if (sxact){
    /*The xact was already destroyed and is being destroyed again??*/
    RW_ASSERT_MESSAGE(0, "xact destroyed twice %s", xact->xact_id_str);

    return RW_STATUS_FAILURE;
  }else{
    sxact = RW_MALLOC0_TYPE(sizeof(*sxact), rwdts_router_stale_xact_t);
    rwdts_xact_id_memcpy(&sxact->id, &xact->id);
    RWDTS_ADD_PBKEY_TO_HASH(hh, dts->stale_xacts, (&sxact->id), router_idx, sxact);
    memcpy(&sxact->end_time, end_time, sizeof(sxact->end_time));
    //Insert at the tail
    RW_DL_ENQUEUE(&(dts->stale_xacts_head), sxact, time_element);
    dts->stats.stale_xact_ct++;
  }
  
  /*Check if we need to delete any existing stale entries*/
  rwdts_router_stale_xact_delete(dts, end_time);
  
  return RW_STATUS_SUCCESS;
}




void rwdts_router_stale_xact_init( rwdts_router_t *dts)
{
  RW_DL_INIT(&dts->stale_xacts_head);
}


static void rwdts_router_fill_reg_msg(RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo)* member_info, int j,
                                      rwdts_memb_router_registration_t *rmemb)
{
  member_info->registration[j] = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo_Registration)));
  RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_MemberInfo_Registration, member_info->registration[j]);

  member_info->registration[j]->id = rmemb->rtr_reg_id;
  member_info->registration[j]->has_id = 1;
  member_info->registration[j]->has_handle = TRUE;
  snprintf(member_info->registration[j]->handle,
           sizeof(member_info->registration[j]->handle), "%p", rmemb);

  member_info->registration[j]->has_keyspec = TRUE;
  strncpy(member_info->registration[j]->keyspec,
          rmemb->keystr,
          sizeof(member_info->registration[j]->keyspec)-1);
  member_info->registration[j]->has_flags = TRUE;
  if (rmemb->flags & RWDTS_FLAG_PUBLISHER) {
    strcat(member_info->registration[j]->flags, "P");
  }
  if (rmemb->flags & RWDTS_FLAG_SUBSCRIBER) {
    strcat(member_info->registration[j]->flags, "S");
  }
}

//#define DEBUG_LOCAL
#ifdef DEBUG_LOCAL
#define PRINT_LOCAL(fmt, args...) \
    fprintf(stderr, "[%s:%d][[ %s ]]" fmt " **** RoUTEr pRiNTs*****\n", __func__, __LINE__, dts->rwmsgpath, args);
#else
#define PRINT_LOCAL(fmt, args...) \
     do {} while (0);
#endif

#define PRINT_LOCAL_O(fmt, args...) PRINT_LOCAL(fmt, args)

typedef struct rwdts_xact_reg_s {
  rwdts_router_xact_t *rtr_xact;
  rwdts_xact_t        *xact;
} rwdts_xact_reg_t;


static void rwdts_router_fill_regs(rwdts_router_member_t *memb,
                            RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo)* member_info)
{
  rwdts_memb_router_registration_t *rmemb=NULL, *rmembnext=NULL;
  int j = 0;
  member_info->n_registration = HASH_CNT(hh_reg, memb->registrations) + HASH_CNT(hh_reg, memb->sub_registrations);
  member_info->registration = realloc(member_info->registration,
                                      member_info->n_registration * sizeof(member_info->registration));

  HASH_ITER(hh_reg, memb->registrations, rmemb, rmembnext)
  {
    rwdts_router_fill_reg_msg(member_info, j, rmemb);
    j++;
  }
  HASH_ITER(hh_reg, memb->sub_registrations, rmemb, rmembnext)
  {
    rwdts_router_fill_reg_msg(member_info, j, rmemb);
    j++;
  }
}

/*
 * caller owns the string
 */
static char*
rwdts_get_rtr_xact_state_string(rwdts_router_xact_state_t state)
{

  switch(state) {
    case RWDTS_ROUTER_XACT_INIT:
      return strdup("INIT");
    case RWDTS_ROUTER_XACT_PREPARE:
      return strdup("PREPARE");
    case RWDTS_ROUTER_XACT_PRECOMMIT:
      return strdup("PRECOMMIT");
    case RWDTS_ROUTER_XACT_COMMIT:
      return strdup("COMMIT");
    case RWDTS_ROUTER_XACT_ABORT:
      return strdup("ABORT");
    case RWDTS_ROUTER_XACT_SUBCOMMIT:
      return strdup("SUBCOMMIT");
    case RWDTS_ROUTER_XACT_SUBABORT:
      return strdup("SUBABORT");
    case RWDTS_ROUTER_XACT_DONE:
      return strdup("DONE");
    case RWDTS_ROUTER_XACT_TERM:
      return strdup("TERM");
    default:
      RW_ASSERT_MESSAGE(0, "Unknown state %u",state);
   }
   return strdup("INVALID");
}

static void rwdts_router_fill_xact_fsm(rwdts_router_xact_t *xact_entry,
                                       RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)* transaction)
{
  int j;
  char xact_id_str[64];

  strncpy(transaction->xid,
          rwdts_xact_id_str(&xact_entry->id, xact_id_str, sizeof(xact_id_str)),
                sizeof(transaction->xid) - 1);
  transaction->has_xid = 1;
  transaction->state = rwdts_get_rtr_xact_state_string(xact_entry->state);
  transaction->n_trace_result = xact_entry->curr_trace_index;
  transaction->trace_result = (RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction_TraceResult)**)
            RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction_TraceResult)*));
  for (j=0; j < xact_entry->curr_trace_index; j++) {
     transaction->trace_result[j] = (RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction_TraceResult)*)
                                  RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction_TraceResult)));
     RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_Transaction_TraceResult, transaction->trace_result[j]);
     transaction->trace_result[j]->id = j;
     transaction->trace_result[j]->has_id = 1;
     transaction->trace_result[j]->state_event = RW_STRDUP(rwdts_router_state_to_str(xact_entry->fsm_trace[j]));;
  }
  return;
}

/***********************************ROUTER CONFIG******************************/
static void*
rwdts_router_mgmt_xact_config_init(rwdts_appconf_t *ac,
                                  rwdts_xact_t *xact,
                                  void *ud)
{
  //no scratch for now..
  return NULL;
}


static void
rwdts_router_mgmt_xact_config_deinit(rwdts_appconf_t *ac,
                                   rwdts_xact_t *xact, 
                                   void *ud,
                                   void *scratch_in)
{
  //no scratchpad for now..
  return;
}


static void
rwdts_router_mgmt_prepare_config(rwdts_api_t *apih,
                                 rwdts_appconf_t *ac, 
                                 rwdts_xact_t *xact,
                                 const rwdts_xact_info_t *queryh,
                                 rw_keyspec_path_t *keyspec, 
                                 const ProtobufCMessage *pbmsg, 
                                 void *ctx, 
                                 void *scratch_in)
{
  rwdts_appconf_prepare_complete_ok(ac, queryh);
  return;
}



static void
rwdts_router_mgmt_config_validate(rwdts_api_t *apih,
                                  rwdts_appconf_t *ac,
                                  rwdts_xact_t *xact, 
                                  void *ctx, 
                                  void *scratch_in)
{
  //no validation for the config as of now...
  return;
}

static void
rwdts_router_mgmt_config_apply(rwdts_api_t *apih,
                               rwdts_appconf_t *ac,
                               rwdts_xact_t *xact,
                               rwdts_appconf_action_t action,
                               void *ctx, 
                               void *scratch_in)
{
  rwdts_router_t *dts = (rwdts_router_t *)ctx;
  RWPB_T_MSG(RwDts_data_Dts_Router) *msg;
  rwdts_member_cursor_t *cursor;
  rw_keyspec_path_t *keyspec;
  
  switch(action){
    case RWDTS_APPCONF_ACTION_INSTALL:
    case RWDTS_APPCONF_ACTION_AUDIT:
      {
        xact = NULL;
      }
      /*fallthrough*/
      break;
    case RWDTS_APPCONF_ACTION_RECONCILE:
      cursor = rwdts_member_data_get_cursor(xact,
                                            dts->config_member_handle);
      msg = (RWPB_T_MSG(RwDts_data_Dts_Router) *)rwdts_member_reg_handle_get_next_element(dts->config_member_handle, cursor, xact, &keyspec);
      if (msg) {
        if (msg->transaction_timer){
          if (msg->transaction_timer->has_enabled){
            dts->xact_timer_enable   = msg->transaction_timer->enabled;
          }else{
            dts->xact_timer_enable   = TRUE;
          }
          if (msg->transaction_timer->has_interval){
            dts->xact_timer_interval = msg->transaction_timer->interval *NSEC_PER_SEC;
          }else{
            dts->xact_timer_interval = RWDTS_TIMEOUT_QUANTUM_MULTIPLE(dts->apih, RWDTS_XACT_TIMEOUT_SCALE);
          }
        }else{
          dts->xact_timer_interval = RWDTS_TIMEOUT_QUANTUM_MULTIPLE(dts->apih, RWDTS_XACT_TIMEOUT_SCALE);
          dts->xact_timer_enable   = TRUE;
        }
      }else{
        /*Since there is no config for the same, the timer value and the
          enablement must revert back to defaults*/
        dts->xact_timer_interval = RWDTS_TIMEOUT_QUANTUM_MULTIPLE(dts->apih, RWDTS_XACT_TIMEOUT_SCALE);
        dts->xact_timer_enable   = TRUE;
      }
      rwdts_member_data_delete_cursors(xact,
                                       dts->config_member_handle);
      break;
    default:
      RW_CRASH();
      break;
  }
  return;
}


static void
rwdts_router_config_mgmt_init(rwdts_router_t *dts)
{
  rwdts_appconf_cbset_t cbset = {
    .xact_init          = rwdts_router_mgmt_xact_config_init,
    .xact_deinit        = rwdts_router_mgmt_xact_config_deinit,
    .config_validate    = rwdts_router_mgmt_config_validate,
    .config_apply       = rwdts_router_mgmt_config_apply,
    .ctx                = dts
  };
  rw_keyspec_path_t                     *keyspec = NULL;
  RWPB_T_PATHSPEC(RwDts_data_Dts_Router) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_Dts_Router));


  dts->xact_timer_interval = RWDTS_TIMEOUT_QUANTUM_MULTIPLE(dts->apih, RWDTS_XACT_TIMEOUT_SCALE);
  dts->xact_timer_enable   = TRUE;
        
  keyspec = (rw_keyspec_path_t *)&keyspec_entry;
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, NULL , RW_SCHEMA_CATEGORY_CONFIG, NULL );
  dts->config_group_handle =  rwdts_appconf_group_create(dts->apih, NULL,
                                                         &cbset);
  RW_ASSERT(dts->config_group_handle);

  dts->config_member_handle =  rwdts_appconf_register(dts->config_group_handle,
                                                      keyspec,
                                                      NULL,
                                                      RWPB_G_MSG_PBCMD(RwDts_data_Dts_Router),
                                                      ( RWDTS_FLAG_SUBSCRIBER |  RWDTS_FLAG_CACHE | RWDTS_FLAG_DELTA_READY | 0 ),
                                                      &rwdts_router_mgmt_prepare_config);
  RW_ASSERT(dts->config_member_handle != NULL);

  /* Say we're finished */
  rwdts_appconf_phase_complete(dts->config_group_handle,
                               RWDTS_APPCONF_PHASE_REGISTER);
  return;
}

/***********************************ROUTER CONFIG******************************/

static rwdts_member_rsp_code_t
rwdts_router_stats_prepare(const rwdts_xact_info_t* xact_info,
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t*      key,
                           const ProtobufCMessage*  msg,
                           uint32_t credits,
                           void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  if (xact) {
    /* Sometimes called with no xact, to exercise from test code */
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
  }

  rwdts_router_t *dts = (rwdts_router_t*)xact_info->ud;
  RW_ASSERT_TYPE(dts, rwdts_router_t);
  RWPB_T_MSG(RwDts_data_Dts_Routers) *inp;
  bool print_everything = false;

  inp = (RwDts__YangData__RwDts__Dts__Routers*)msg;


  // We can only handle reads and the CLI should only send those
  RW_ASSERT(action == RWDTS_QUERY_READ);

  // Repond to the request
  //
  RWPB_T_MSG(RwDts_data_Dts_Routers) data_router, *data_router_p;
  data_router_p = &data_router;
  RWPB_F_MSG_INIT(RwDts_data_Dts_Routers,data_router_p);

  if (inp &&
   //   (inp->stats == NULL) &&
   //   (inp->n_transaction== 0) &&
      (inp->n_member_info  == 0))
  {
    print_everything = true;
  }
  if (inp && ((inp->stats > 0) || print_everything))
  {
    int i;
    data_router_p->stats = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Stats)));
    RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_Stats, data_router_p->stats);

#define _s(xx) data_router_p->stats->has_##xx = TRUE; data_router_p->stats->xx = dts->stats.xx
    data_router_p->stats->n_topx_member_histo = 64;
    data_router_p->stats->n_req_blocks_histo = 64;
    data_router_p->stats->n_subx_retry_histo = 64;
    for (i=0; i<64; i++) {
      data_router_p->stats->topx_member_histo[i] = dts->stats.topx_member_histo[i];
      data_router_p->stats->req_blocks_histo[i] = dts->stats.req_blocks_histo[i];
      data_router_p->stats->subx_retry_histo[i] = dts->stats.subx_retry_histo[i];
    }

    dts->stats.topx_count_5s = 0;
    uint64_t total_latency = 0;
    for (i=0; i < 5; i++) {
      dts->stats.topx_count_5s += dts->stats.topx_xact_histo[i];
      total_latency += dts->stats.topx_latency_histo[i];
    }
    dts->stats.topx_rate_5s = dts->stats.topx_count_5s/5;
    dts->stats.topx_latency_5s = total_latency/5;

    dts->stats.topx_count_15s = 0;
    total_latency = 0;
    for (i=0; i < 5; i++) {
      dts->stats.topx_count_15s += dts->stats.topx_xact_15s_histo[i];
      total_latency += dts->stats.topx_latency_15s_histo[i];
    }
    dts->stats.topx_rate_15s = dts->stats.topx_count_15s/15;
    dts->stats.topx_latency_15s = total_latency/5;

    dts->stats.topx_count_1m = 0;
    total_latency = 0;
    for (i=0; i < 6; i++) {
      dts->stats.topx_count_1m += dts->stats.topx_xact_1m_histo[i];
      total_latency += dts->stats.topx_latency_1m_histo[i];
    }
    dts->stats.topx_rate_1m = dts->stats.topx_count_1m/60;
    dts->stats.topx_latency_1m = total_latency/6;

    dts->stats.topx_count_5m = 0;
    total_latency = 0;
    for (i=0; i < 10; i++) {
      dts->stats.topx_count_5m += dts->stats.topx_xact_5m_histo[i];
      total_latency += dts->stats.topx_latency_5m_histo[i];
    }
    dts->stats.topx_rate_5m = dts->stats.topx_count_5m/300;
    dts->stats.topx_latency_5m = total_latency/10;

    _s(topx_count_5s);
    _s(topx_rate_5s);
    _s(topx_latency_5s);
    _s(topx_count_15s);
    _s(topx_rate_15s);
    _s(topx_latency_15s);
    _s(topx_count_1m);
    _s(topx_rate_1m);
    _s(topx_latency_1m);
    _s(topx_count_5m);
    _s(topx_rate_5m);
    _s(topx_latency_5m);
    _s(topx_begin);
    _s(topx_begin_tran);
    _s(topx_begin_notran);
    _s(topx_end);
    _s(topx_end_notran_success);
    _s(topx_end_notran_fail);
    _s(topx_end_commit);
    _s(topx_end_abort);
    _s(topx_end_fail);
    _s(subx_begin);
    _s(subx_begin_notran);
    _s(subx_retry);
    _s(subx_retry_success);
    _s(subx_retry_failure);
    _s(subx_end);
    _s(subx_end_subcommit);
    _s(subx_end_subabort);
    _s(subx_end_suberr);
    _s(subx_end_notran_success);
    _s(subx_end_notran_fail);
    _s(prep_query_member_included);
    _s(prep_query_member_filtered);
    _s(req_sent);
    _s(req_bnc);
    _s(req_bnc_imm);
    _s(req_responded);
    _s(req_responded_async);
    _s(req_rcv_regist);
    _s(req_rcv_execute);
    _s(req_rcv_execute_topx);
    _s(req_rcv_execute_append);
    _s(req_rcv_execute_subx);
    _s(req_rcv_end);
    _s(req_rcv_abort);
    _s(req_rcv_flush);
    _s(req_rcv_update_fake_table);
    _s(req_rcv_mbr_query_response);
    _s(req_rcv_mbr_regist);
    _s(more_sent);
    _s(incorrect_routeridx);
    _s(incorrect_clientidx);
    _s(xact_life_less_50_ms);
    _s(xact_life_less_100_ms);
    _s(xact_life_less_500_ms);
    _s(xact_life_less_1_sec);
    _s(xact_life_more_1_sec);
    _s(xact_prep_time_less_50_ms);
    _s(xact_prep_time_less_100_ms);
    _s(xact_prep_time_less_500_ms);
    _s(xact_prep_time_less_1_sec);
    _s(xact_prep_time_more_1_sec);
    _s(xact_pcom_time_less_50_ms);
    _s(xact_pcom_time_less_100_ms);
    _s(xact_pcom_time_less_500_ms);
    _s(xact_pcom_time_less_1_sec);
    _s(xact_pcom_time_more_1_sec);
    _s(xact_com_time_less_50_ms);
    _s(xact_com_time_less_100_ms);
    _s(xact_com_time_less_500_ms);
    _s(xact_com_time_less_1_sec);
    _s(xact_com_time_more_1_sec);
    _s(xact_abort_time_less_50_ms);
    _s(xact_abort_time_less_100_ms);
    _s(xact_abort_time_less_500_ms);
    _s(xact_abort_time_less_1_sec);
    _s(xact_abort_time_more_1_sec);
    _s(member_ct);
    _s(xact_ct);
    _s(stale_xact_ct);
#undef _s
  }
  if (inp && ((inp->payload_stats != NULL) || print_everything))
  {
    data_router_p->payload_stats =
        (RWPB_T_MSG(RwDts_data_Dts_Routers_PayloadStats) *)protobuf_c_message_duplicate(NULL,
                                                                                        (const struct ProtobufCMessage *) &dts->payload_stats,
                                                                                        dts->payload_stats.base.descriptor);
  }

  if (inp && ((inp->n_member_info > 0)||print_everything))
  {
    rwdts_router_member_t *memb=NULL, *membnext=NULL;
    RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo)  *current_member;
    int i = 0;
    if ((inp->n_member_info > 0) &&
        (inp->member_info[0]->name[0] != '\0'))
    {
      HASH_FIND(hh, dts->members, inp->member_info[0]->name, strlen(inp->member_info[0]->name), memb);
      if(memb)
      {
        data_router_p->n_member_info = 1;
        data_router_p->member_info = realloc(data_router_p->member_info,
            data_router_p->n_member_info * sizeof(data_router_p->member_info));
        goto memb_found;
      }
    }
    data_router_p->n_member_info = HASH_CNT(hh, dts->members);
    data_router_p->member_info = RW_MALLOC0(data_router_p->n_member_info * sizeof(data_router_p->member_info));
    HASH_ITER(hh, dts->members, memb, membnext)
    {
memb_found:
      current_member = (RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo)*)
                                     RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo)));
      RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_MemberInfo, current_member);
      strncpy(current_member->name, memb->msgpath, sizeof(current_member->name)-1);
      current_member->has_name = 1;
      current_member->stats = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_MemberInfo_Stats)));
      RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_MemberInfo_Stats, current_member->stats);
      COPY_STATS_2(*current_member->stats , memb->stats, reg_precommit_called);
      COPY_STATS_2(*current_member->stats , memb->stats, reg_prepare_called);
      COPY_STATS_2(*current_member->stats , memb->stats, reg_added);
      rwdts_router_fill_regs(memb, current_member);
      data_router_p->member_info[i] = current_member;
      i++;
    }
  }

  if (inp && (inp->n_transaction > 0)) {
    if (inp->transaction[0]->xid[0] == '\0') {
      int i = 0;

      data_router_p->n_transaction = HASH_CNT(hh, dts->xacts);
      data_router_p->transaction = (RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)**)
                              RW_MALLOC0(data_router_p->n_transaction * sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)*));
      RW_ASSERT(data_router_p->transaction);

      rwdts_router_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
      HASH_ITER(hh, dts->xacts, xact_entry, tmp_xact_entry) {
        data_router_p->transaction[i] = (RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)*)
                                   RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)));
        RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_Transaction,data_router_p->transaction[i]);
        rwdts_router_fill_xact_fsm(xact_entry, data_router_p->transaction[i]);
        i++;
      }
    } else {
      char xact_id_str[64];
      rwdts_router_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
      HASH_ITER(hh, dts->xacts, xact_entry, tmp_xact_entry) {
        if (!strcmp(inp->transaction[0]->xid, rwdts_xact_id_str(&xact_entry->id, xact_id_str, sizeof(xact_id_str)))) {
          data_router_p->n_transaction = 1;
          data_router_p->transaction = RW_MALLOC(sizeof(data_router_p->transaction));
          data_router_p->transaction[0] = (RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)*)
                                     RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_Dts_Routers_Transaction)));
          RWPB_F_MSG_INIT(RwDts_data_Dts_Routers_Transaction,data_router_p->transaction[0]);
          rwdts_router_fill_xact_fsm(xact_entry, data_router_p->transaction[0]);
          break;
        }
      }
    }
  }

  rw_status_t rs = RW_STATUS_SUCCESS;

  RWPB_T_PATHSPEC(RwDts_data_Dts_Routers) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_Dts_Routers));

  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, dts->rwmsgpath, sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  strncpy(data_router_p->name,dts->rwmsgpath, sizeof(data_router_p->name) - 1);

  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, NULL , RW_SCHEMA_CATEGORY_DATA, NULL );
  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t*)keyspec,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&data_router_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  if (xact) {    // NULL when called from test code
    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  }

  // Free the protobuf
  protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)data_router_p);

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
rwdts_router_handle_clear_stats_req(const rwdts_xact_info_t* xact_info,
                                    RWDtsQueryAction         action,
                                    const rw_keyspec_path_t*      key,
                                    const ProtobufCMessage*  msg,
                                    uint32_t credits,
                                    void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  uint64_t tmp_xact_ct;
  if (xact) {
    /* Sometimes called with no xact, to exercise from test code */
    RW_ASSERT_TYPE(xact, rwdts_xact_t);
  }

  rwdts_router_t *dts = (rwdts_router_t*)xact_info->ud;
  RW_ASSERT_TYPE(dts, rwdts_router_t);
  RWPB_T_MSG(RwDts_input_ClearDts_Stats_Router) *inp;

  inp = (RWPB_T_MSG(RwDts_input_ClearDts_Stats_Router) *) msg;
  RW_ASSERT(inp);

  if (inp->name) {
    if (!strstr(dts->rwmsgpath, inp->name)) {
      return RWDTS_ACTION_NA;
    }
  } else {
    if (!inp->has_all) {
      return RWDTS_ACTION_NA;
    }
  }

  /* xact_ct state is checked for and ASSERTED during router-xact-destroy. So,
   * retaining the xact_ct. xact_ct will not be zeroed */

  tmp_xact_ct = dts->stats.xact_ct;
  memset(&dts->stats, 0, sizeof(dts->stats));
  dts->stats.xact_ct = tmp_xact_ct;
  return RWDTS_ACTION_OK;

}

static void
rwdts_router_remove_all_regs(rwdts_router_t *dts,
                             rwdts_router_member_t *memb)
{
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  HASH_ITER(hh_reg, memb->registrations, reg, tmp_reg) {
    if (reg->shard) {
      rwdts_shard_rtr_delete_element(reg->shard, memb->msgpath, true);
    }
    HASH_DELETE(hh_reg, memb->registrations, reg);
    RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
    RW_FREE(reg->ks_binpath);
    RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
  }
  HASH_ITER(hh_reg, memb->sub_registrations, reg, tmp_reg) {
    if (reg->shard) {
      rwdts_shard_rtr_delete_element(reg->shard, memb->msgpath, false);
    }
    HASH_DELETE(hh_reg, memb->sub_registrations, reg);
    RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
    RW_FREE(reg->ks_binpath);
    RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
  }
  /* Remove the data obj at the reg-handle for this member */
  return;
}

void 
rwdts_router_remove_all_regs_for_gtest(rwdts_router_t *dts,
                                       rwdts_router_member_t *memb)
{
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  HASH_ITER(hh_reg, memb->registrations, reg, tmp_reg) {
    if (reg->flags & RWDTS_FLAG_INTERNAL_REG) {
      continue;
    }
    HASH_DELETE(hh_reg, memb->registrations, reg);
    RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
    RW_FREE(reg->ks_binpath);
    RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
  }
  HASH_ITER(hh_reg, memb->sub_registrations, reg, tmp_reg) {
    if (reg->flags & RWDTS_FLAG_INTERNAL_REG) {
      continue;
    }
    HASH_DELETE(hh_reg, memb->sub_registrations, reg);
    RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
    RW_FREE(reg->ks_binpath);
    RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
  }
  return;
}

static void
rwdts_router_send_dereg_peer(rwdts_router_t *dts,
                             rwdts_router_member_t *memb,
                             rwdts_memb_router_registration_t *reg)
{
  rw_keyspec_path_t* keyspec;
  RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration) router_pub_ks  =
        (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration));

  keyspec = (rw_keyspec_path_t*)&router_pub_ks;
  UNUSED(keyspec);
 
  router_pub_ks.has_dompath = TRUE;
  router_pub_ks.dompath.has_path001 = TRUE;
  router_pub_ks.dompath.path001.has_key00 = TRUE;
  strncpy(router_pub_ks.dompath.path001.key00.name, dts->rwmsgpath, sizeof(router_pub_ks.dompath.path001.key00.name));

  router_pub_ks.dompath.has_path002 = TRUE;
  router_pub_ks.dompath.path002.has_key00 = TRUE;
  strncpy(router_pub_ks.dompath.path002.key00.name, memb->msgpath, sizeof(router_pub_ks.dompath.path002.key00.name));

  router_pub_ks.dompath.has_path003 = TRUE;
  router_pub_ks.dompath.path003.has_key00 = TRUE;
  router_pub_ks.dompath.path003.key00.keystr = RW_STRDUP(reg->keystr);
  router_pub_ks.dompath.path003.has_key01 = TRUE;
  router_pub_ks.dompath.path003.key01.flags = reg->flags;

// Attn :Pbdleta feature pending 
// TBD: uncomment this after implementing protob delta feature so that we 
// clean up all routers after de-reg of a specific keyspec from mmember
//
//  rw_status_t rs = rwdts_member_reg_handle_delete_element_keyspec(dts->local_regkeyh,
//  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_FREE(router_pub_ks.dompath.path003.key00.keystr);
}

static void
rwdts_router_remove_reg_keyspec(rwdts_router_t *dts,
                         const rw_keyspec_path_t *key,
                         uint32_t flags,
                         const uint8_t *bp,
                         size_t bp_len)
{
  rwdts_memb_router_registration_t *reg = NULL;

  if (!bp || !bp_len) {
    return;
  }

  RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) pathspec =
      *((RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member)*) key);

  RW_ASSERT(pathspec.dompath.path001.key00.name);

  rwdts_router_member_t *memb;
  HASH_FIND(hh, dts->members,
            pathspec.dompath.path001.key00.name,
            strlen(pathspec.dompath.path001.key00.name), memb);
  if (!memb) {
    return;
  }


  if (flags & RWDTS_FLAG_PUBLISHER) {
    HASH_FIND(hh_reg, memb->registrations,bp,bp_len, reg); 
    if (reg) {
      rwdts_router_send_dereg_peer(dts, memb, reg);
      HASH_DELETE(hh_reg, memb->registrations, reg);
      if (reg->shard && !HASH_CNT(hh_reg, memb->registrations)) {
        rwdts_shard_rtr_delete_element(reg->shard, memb->msgpath, true);
      }
      RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
      RW_FREE(reg->ks_binpath);
      RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
    }
  }

  if (flags & RWDTS_FLAG_SUBSCRIBER) {
    HASH_FIND(hh_reg, memb->sub_registrations,bp,bp_len, reg); 
    if (reg) {
      rwdts_router_send_dereg_peer(dts, memb, reg);
      if (reg->shard && !HASH_CNT(hh_reg, memb->sub_registrations)) {
        rwdts_shard_rtr_delete_element(reg->shard, memb->msgpath, false);
      }
      HASH_DELETE(hh_reg, memb->sub_registrations, reg);
      RW_KEYSPEC_PATH_FREE(reg->keyspec, NULL ,NULL);
      RW_FREE(reg->ks_binpath);
      RW_FREE_TYPE(reg, rwdts_memb_router_registration_t);
    }
  }

}

static __inline__ void
rwdts_router_fill_router_key(rwdts_router_t *dts,
                             RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)* router_pub_ks,
                             const char *name)
{
  router_pub_ks->has_dompath = TRUE;
  router_pub_ks->dompath.has_path001 = TRUE;
  router_pub_ks->dompath.path001.has_key00 = TRUE;
  strncpy(router_pub_ks->dompath.path001.key00.name, dts->rwmsgpath, sizeof(router_pub_ks->dompath.path001.key00.name));
  router_pub_ks->dompath.has_path002 = TRUE;
  router_pub_ks->dompath.path002.has_key00 = TRUE;
  strncpy(router_pub_ks->dompath.path002.key00.name, name, sizeof(router_pub_ks->dompath.path001.key00.name));
}

static __inline__ void
rwdts_router_fill_router_reg_key(rwdts_router_t *dts,
                                 RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration)* router_pub_ks,
                                 const char *name, const char *keystr, uint32_t flags)
{
  router_pub_ks->has_dompath = TRUE;
  router_pub_ks->dompath.has_path001 = TRUE;
  router_pub_ks->dompath.path001.has_key00 = TRUE;
  strncpy(router_pub_ks->dompath.path001.key00.name, dts->rwmsgpath, sizeof(router_pub_ks->dompath.path001.key00.name));
  router_pub_ks->dompath.has_path002 = TRUE;
  router_pub_ks->dompath.path002.has_key00 = TRUE;
  strncpy(router_pub_ks->dompath.path002.key00.name, name, sizeof(router_pub_ks->dompath.path001.key00.name));
  router_pub_ks->dompath.has_path003 = TRUE;
  router_pub_ks->dompath.path003.has_key00 = TRUE;
  router_pub_ks->dompath.path003.key00.keystr = RW_STRDUP(keystr);
  router_pub_ks->dompath.path003.has_key01 = TRUE;
  router_pub_ks->dompath.path003.key01.flags = flags;
}

static void
rwdts_router_fill_store_router_data_and_key(rwdts_router_t *dts,
                                            RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member)* store_router_data,
                                            RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)* router_pub_ks,
                                            RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p)
{
  RWPB_F_MSG_INIT(RwDts_data_RtrPeerRegKeyspec_Router_Member,store_router_data);
  strncpy(store_router_data->name, member_p->name, sizeof(store_router_data->name));
  store_router_data->has_name = 1;
  RW_ASSERT(member_p->has_rtr_id);
  RW_ASSERT(member_p->has_client_id);
  store_router_data->has_rtr_id = 1;
  store_router_data->rtr_id = member_p->rtr_id;
  store_router_data->has_client_id = 1;
  store_router_data->client_id = member_p->client_id;
  if (member_p->has_all_regs) {
    store_router_data->has_all_regs = 1;
    store_router_data->all_regs = member_p->all_regs;
  }
  store_router_data->n_registration = member_p->n_registration;
  store_router_data->registration = 
      RW_MALLOC0(sizeof(store_router_data->registration[0]) * store_router_data->n_registration);
  int i;
  for (i=0; i < member_p->n_registration; i++) {
    store_router_data->registration[i] = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration)));
    RWPB_F_MSG_INIT(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration, store_router_data->registration[i]);
    store_router_data->registration[i]->keystr = RW_STRDUP(member_p->registration[i]->keystr);
    store_router_data->registration[i]->has_keyspec = 1;
    store_router_data->registration[i]->keyspec.len = member_p->registration[i]->keyspec.len;
    store_router_data->registration[i]->keyspec.data = 
        RW_MALLOC0(store_router_data->registration[i]->keyspec.len);
    memcpy(store_router_data->registration[i]->keyspec.data, 
           member_p->registration[i]->keyspec.data, 
           store_router_data->registration[i]->keyspec.len);
    store_router_data->registration[i]->flags = member_p->registration[i]->flags;
    store_router_data->registration[i]->has_flags = 1;
    store_router_data->registration[i]->has_flavor = member_p->registration[i]->has_flavor;
    store_router_data->registration[i]->flavor = member_p->registration[i]->flavor;
    store_router_data->registration[i]->has_index = member_p->registration[i]->has_index;
    store_router_data->registration[i]->index = member_p->registration[i]->index;
    store_router_data->registration[i]->has_start = member_p->registration[i]->has_start;
    store_router_data->registration[i]->start = member_p->registration[i]->start;
    store_router_data->registration[i]->has_stop = member_p->registration[i]->has_stop;
    store_router_data->registration[i]->stop = member_p->registration[i]->stop;
    store_router_data->registration[i]->has_promote_sub = member_p->registration[i]->has_promote_sub;
    store_router_data->registration[i]->promote_sub = member_p->registration[i]->promote_sub;
    
    if (member_p->registration[i]->has_minikey) {
        store_router_data->registration[i]->has_minikey = 1;
        store_router_data->registration[i]->minikey.len = member_p->registration[i]->minikey.len;
        store_router_data->registration[i]->minikey.data = RW_MALLOC0(member_p->registration[i]->minikey.len);
        RW_ASSERT(store_router_data->registration[i]->minikey.data);
        memcpy(store_router_data->registration[i]->minikey.data, 
                 member_p->registration[i]->minikey.data, member_p->registration[i]->minikey.len);
      }

    if (member_p->registration[i]->has_descriptor) {
      strcpy(store_router_data->registration[i]->descriptor, 
             member_p->registration[i]->descriptor);
    }
    store_router_data->registration[i]->has_rtr_reg_id = 1;
    store_router_data->registration[i]->rtr_reg_id = member_p->registration[i]->rtr_reg_id;
  }
  rwdts_router_fill_router_key(dts, router_pub_ks, member_p->name);
}

static void
rwdts_router_reg_handle_delete(rwdts_router_t *dts,
                               rwdts_xact_t *xact,
                               const rw_keyspec_path_t*      key,
                               RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p)
{
  RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) pathspec =
      *((RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member)*) key);

  // only support memberall delete
  RW_ASSERT(pathspec.dompath.path001.key00.name);

  RW_ASSERT(member_p);

  rwdts_router_member_t *memb;
  HASH_FIND(hh, dts->members, 
            pathspec.dompath.path001.key00.name, 
            strlen(pathspec.dompath.path001.key00.name), memb);

  if (strstr(member_p->name, "uAgent")) {
    member_p->has_recovery_action = true;
    member_p->recovery_action = RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL;
  }
  bool all_regs = (member_p->has_recovery_action && (member_p->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL));
  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, RWLOG_ATTR_SPRINTF("%s Delete Received for regs %d", member_p->name, all_regs));
  if (memb)  {
    if (all_regs && member_p->n_registration) {
      RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration) router_pub_ks  =
          (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member_Registration));
      rwdts_router_fill_router_reg_key(dts, &router_pub_ks, memb->msgpath, member_p->registration[0]->keystr, member_p->registration[0]->flags);
      rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&router_pub_ks;
      PRINT_STR (dts->rwmsgpath," delete with %s member regns", memb->msgpath);
      RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterCritical, RWLOG_ATTR_SPRINTF("%s Delete all member regs", memb->msgpath));
      if(memb->router_idx == RWDTS_ROUTER_LOCAL_IDX(dts)) {
        rw_status_t rs = rwdts_member_reg_handle_delete_element_keyspec(dts->local_regkeyh,
                                                                        keyspec,
                                                                        NULL, NULL);
        RW_ASSERT(rs == RW_STATUS_SUCCESS);
      }
    }
    if (member_p->has_recovery_action && (member_p->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_RESTART)) {
    
     RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                                  RWLOG_ATTR_SPRINTF("WAIT_RESTART Setting for %s", memb->msgpath));
      memb->wait_restart = true;
      rwdts_restart_begin_notify(dts->apih, memb->msgpath);
      RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterCritical, RWLOG_ATTR_SPRINTF("%s member set wait recovery", memb->msgpath));
      if (memb->dest) {
        rwmsg_clichan_stream_reset(dts->clichan, memb->dest);
      }
      if (memb->dest) {
        rwmsg_destination_destroy(memb->dest);
        memb->dest = NULL;
      }
    }
    if (all_regs) {
      rwdts_router_stop_member_sing_reqs(memb);
      rwdts_router_remove_all_regs(dts, memb);
    }
    rwdts_shard_deinit_keyspec((rw_keyspec_path_t*)key, &dts->rootshard);
  }
  return;
}

static bool
rwdts_check_shared_flag(rwdts_router_t *dts,
                        RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member_Registration) *reg)
{
  rwdts_memb_router_registration_t *v = NULL;
  rw_keyspec_path_t* keyspec = RW_KEYSPEC_PATH_CREATE_WITH_BINPATH_BUF(NULL, reg->keyspec.len,
                                                             reg->keyspec.data, NULL);

  RwSchemaCategory category = rw_keyspec_path_get_category(keyspec);

  if (!rw_keyspec_path_has_wildcards(keyspec) && 
      (reg->flags & RWDTS_FLAG_PUBLISHER) && (category != RW_SCHEMA_CATEGORY_RPC_INPUT)) {
    rwdts_router_member_t *memb=NULL, *membnext=NULL;
    HASH_ITER(hh, dts->members, memb, membnext) {
      HASH_FIND(hh_reg, memb->registrations, reg->keyspec.data,
                reg->keyspec.len, v);
      if ((v) &&
          (v->flags & RWDTS_FLAG_PUBLISHER) &&
          (!(v->flags & RWDTS_FLAG_SHARED) ||
           !(reg->flags & RWDTS_FLAG_SHARED))) {
        RW_KEYSPEC_PATH_FREE(keyspec, NULL ,NULL);
        return TRUE;
      }
    }
  }
  RW_KEYSPEC_PATH_FREE(keyspec, NULL ,NULL);
  return FALSE;
}

typedef struct rwdts_router_peer_reg_advise_s {
  rwdts_router_t *dts;
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)  *reg_stored;
  const rw_keyspec_path_t* key;
  const rwdts_xact_info_t* xact_info;
  bool responded;
} rwdts_router_peer_reg_advise_t;

void rwdts_router_store_registration(rwdts_router_t *dts, 
                                     rwdts_router_member_t *memb,
                                     rwdts_xact_t *xact,
                                     RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)  *rcvreg)
{
  RW_ASSERT(rcvreg);
  rw_status_t rs;
  rw_keyspec_path_t *keyspec = NULL;

  RW_ASSERT(rcvreg->has_rtr_id);
  {
    RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member) store_router_data;
    RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member) *router_pub_ks = NULL;

    rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)
                                    (RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member)),
                                    NULL, (rw_keyspec_path_t**)&router_pub_ks, NULL);
    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)router_pub_ks, NULL , RW_SCHEMA_CATEGORY_DATA, NULL );

    rwdts_router_fill_store_router_data_and_key(dts,
                                                &store_router_data,
                                                router_pub_ks,
                                                rcvreg);

    keyspec = (rw_keyspec_path_t*)router_pub_ks;

    PRINT_STR (dts->rwmsgpath," update with %s member having %lu regns", rcvreg->name, rcvreg->n_registration);

    rs = rwdts_member_reg_handle_update_element_keyspec(dts->local_regkeyh,
                                                        keyspec, &store_router_data.base, 0,
                                                        NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                                RWLOG_ATTR_SPRINTF("publish %s:%s from %s", rcvreg->name, rcvreg->registration[0]->keystr, dts->rwmsgpath));
    protobuf_c_message_free_unpacked_usebody(NULL, &store_router_data.base);

    RW_KEYSPEC_PATH_FREE((rw_keyspec_path_t*)router_pub_ks, NULL ,NULL);
  }
  
  return;
}


static __inline__ void
rwdts_router_reg_response( const rwdts_xact_info_t* xact_info,
                          const rw_keyspec_path_t* key,
                          RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *init_member_p)
{
  rwdts_xact_t *xact = xact_info->xact;
  RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) *dts_init = NULL;

  rw_status_t rs = rw_keyspec_path_create_dup(key, &xact->ksi ,
                                              (rw_keyspec_path_t**)&dts_init);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  rwdts_member_query_rsp_t rsp = {
    .ks = (rw_keyspec_path_t *)dts_init,
    .n_msgs = 1,
    .msgs = (ProtobufCMessage**)&init_member_p,
    .evtrsp = RWDTS_EVTRSP_ACK
  };

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  protobuf_c_message_free_unpacked(NULL, (ProtobufCMessage*)init_member_p);
  RW_KEYSPEC_PATH_FREE((rw_keyspec_path_t*)dts_init, NULL,NULL);
}



static rwdts_router_member_t*
rwdts_router_get_router_member(rwdts_router_t *dts,
                               bool local,
                               RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p)
{
  rwdts_router_member_t *memb;
  HASH_FIND(hh, dts->members, member_p->name, strlen(member_p->name), memb);
  /* if the member is not there add the member to the list */
  if (!memb)  {
    memb = RW_MALLOC0_TYPE(sizeof(memb[0]), rwdts_router_member_t);
    memb->msgpath = strdup(member_p->name);

    RW_DL_INIT(&memb->queued_xacts);
    RW_DL_INIT(&memb->sing_reqs);
    
    HASH_ADD_KEYPTR(hh, dts->members, memb->msgpath, strlen(memb->msgpath), memb);
    if (!member_p->client_id) {
      memb->client_idx = ++dts->client_idx;
      if (local) {
        RW_ASSERT(member_p->rtr_id == dts->router_idx);
      }
      memb->router_idx = member_p->rtr_id;
    }
    else {
      memb->client_idx = member_p->client_id;
      memb->router_idx = member_p->rtr_id;
    }

    RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, RWLOG_ATTR_SPRINTF("DTSRouter: added member [%s]", memb->msgpath)); 
  } else {
    RWDTS_ROUTER_LOG_REG_EVENT(dts, MemberAlreadyPresent, "Member is already present in the registrations - ignoring the request", memb->msgpath);
  }
  RW_ASSERT(memb);
  RW_ASSERT(memb->client_idx);
  RW_ASSERT(memb->router_idx);
  dts->stats.member_ct = dts->members ? HASH_CNT(hh, dts->members) : 0;
  return memb;
}

static __inline__ void
rwdts_router_update_registration(rwdts_router_t *dts,
                                 rwdts_router_member_t *memb,
                                 RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p,
                                 RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *ret_member_p)
{
  rwdts_memb_router_registration_t *reg;
  int i;

  for (i = 0; i < member_p->n_registration; i++) 
  {
    if (rwdts_check_shared_flag(dts, member_p->registration[i])) {
      continue;
    }

    if (member_p->registration[i]->flags & RWDTS_FLAG_PUBLISHER) {
      HASH_FIND(hh_reg, 
                memb->registrations, 
                member_p->registration[i]->keyspec.data, 
                member_p->registration[i]->keyspec.len, 
                reg);
    } else {
      HASH_FIND(hh_reg, 
                memb->sub_registrations, 
                member_p->registration[i]->keyspec.data, 
                member_p->registration[i]->keyspec.len, 
                reg);
    }

    if (reg) 
    {
      reg->flags |= member_p->registration[i]->flags;

      if (member_p->registration[i]->has_flavor) 
      {
        // If flavor had changed update the shard 
        // TBD:move this to rwdts_router_reg_apply_crec() 
        if (member_p->registration[i]->has_flavor && 
            (reg->shard_flavor != member_p->registration[i]->flavor)) 
        {
           // TBD make the below if blocks to separate update shard params function call
           if (reg->shard_flavor == RW_DTS_SHARD_FLAVOR_RANGE) {
             reg->params.range.start_range = member_p->registration[i]->start;
             reg->params.range.end_range = member_p->registration[i]->stop;
           }

           if (member_p->registration[i]->flavor == RW_DTS_SHARD_FLAVOR_IDENT) {
             RW_ASSERT(member_p->registration[i]->has_minikey);
             RW_ASSERT(
                 member_p->registration[i]->minikey.len<=sizeof(reg->params.identparams.keyin));

             memcpy(&reg->params.identparams.keyin, member_p->registration[i]->minikey.data, 
                    (member_p->registration[i]->minikey.len<sizeof(reg->params.identparams.keyin)?
                     member_p->registration[i]->minikey.len:sizeof(reg->params.identparams.keyin)));

             reg->params.identparams.keylen = member_p->registration[i]->minikey.len; 
           }
         
           rw_status_t rs = 
             rwdts_shard_update(reg->shard, member_p->registration[i]->flavor, &reg->params);

           RW_ASSERT(rs == RW_STATUS_SUCCESS);

           reg->shard_flavor = member_p->registration[i]->flavor;
           rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg->shard,&reg->params);
           RW_ASSERT(chunk);

           rwdts_chunk_rtr_info_t rtr_info;
           rtr_info.member = memb;
           rtr_info.flags = member_p->registration[i]->flags | reg->flags;

           if (rtr_info.flags & RWDTS_FLAG_PUBLISHER) {
             rwdts_shard_rts_create_element(reg->shard, chunk, 
                 &rtr_info, true, &reg->chunk_id, &reg->membid, rtr_info.member->msgpath);
           } else {
             rwdts_shard_rts_create_element(reg->shard, chunk, 
                &rtr_info, false, &reg->chunk_id, &reg->membid, rtr_info.member->msgpath);
           }
         }
      } // end if

      reg->has_index = member_p->registration[i]->has_index;
      reg->index = member_p->registration[i]->index;

      if (member_p->registration[i]->has_minikey) {
        memcpy(&reg->params.identparams.keyin[0],
              member_p->registration[i]->minikey.data, member_p->registration[i]->minikey.len);
        reg->params.identparams.keylen = member_p->registration[i]->minikey.len;
      }
    } 
    else 
    {
      reg = RW_MALLOC0_TYPE(sizeof(rwdts_memb_router_registration_t),
                            rwdts_memb_router_registration_t);
      RW_ASSERT (reg);

      reg->flags = member_p->registration[i]->flags;
      reg->ks_binpath = RW_MALLOC0(member_p->registration[i]->keyspec.len);

      memcpy((char *)reg->ks_binpath, 
          member_p->registration[i]->keyspec.data, member_p->registration[i]->keyspec.len);

      reg->ks_binpath_len = member_p->registration[i]->keyspec.len;

      reg->keyspec = 
        rw_keyspec_path_create_with_binpath_buf(NULL, 
                                                reg->ks_binpath_len , 
                                                (const void*)reg->ks_binpath);

      reg->shard_flavor = member_p->registration[i]->flavor;
      reg->has_index = member_p->registration[i]->has_index;
      reg->index = member_p->registration[i]->index;

      if (reg->shard_flavor == RW_DTS_SHARD_FLAVOR_RANGE) {
        reg->params.range.start_range = member_p->registration[i]->start;
        reg->params.range.end_range = member_p->registration[i]->stop;
      }

      RW_ASSERT(member_p->registration[i]->flavor);

      if (member_p->registration[i]->has_minikey) {
        memcpy(&reg->params.identparams.keyin[0],
            member_p->registration[i]->minikey.data, member_p->registration[i]->minikey.len);
        reg->params.identparams.keylen = member_p->registration[i]->minikey.len;
      } 
      RW_ASSERT(reg->keyspec);
      strncpy(reg->keystr, member_p->registration[i]->keystr, sizeof(reg->keystr));
      reg->keystr[sizeof(reg->keystr)-1] = '\0';
      RW_STATIC_ASSERT(sizeof(reg->keystr) > sizeof(char*));
      
#ifdef RWDTS_SHARDING
      RW_ASSERT(reg->shard_flavor);
      reg->shard = rwdts_shard_init_keyspec(dts->apih, reg->keyspec,
                                            reg->has_index?reg->index:-1, 
                                            &dts->rootshard,
                                            reg->shard_flavor, &reg->params, 
                                            RWDTS_SHARD_KEYFUNC_NOP, 
                                            NULL,
                                            RWDTS_SHARD_DEMUX_ROUND_ROBIN);
      RW_ASSERT(reg->shard);
      rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg->shard,&reg->params);
      RW_ASSERT(chunk);
      rwdts_chunk_rtr_info_t rtr_info; 
      rtr_info.member = memb;
      rtr_info.flags = member_p->registration[i]->flags;
#endif
      
      if (member_p->registration[i]->flags & RWDTS_FLAG_PUBLISHER) {
#ifdef RWDTS_SHARDING
        rwdts_shard_rts_create_element(reg->shard, chunk, &rtr_info, true, &reg->chunk_id, &reg->membid, rtr_info.member->msgpath);
#endif
        HASH_ADD_KEYPTR(hh_reg, memb->registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
      } else {
#ifdef RWDTS_SHARDING
        rwdts_shard_rts_create_element(reg->shard, chunk, &rtr_info, false, &reg->chunk_id, &reg->membid, rtr_info.member->msgpath);
#endif
        HASH_ADD_KEYPTR(hh_reg, memb->sub_registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
      }
      
      if (!member_p->registration[i]->rtr_reg_id) {
        reg->rtr_reg_id = dts->rtr_reg_id++;
      }
      else {
        RW_ASSERT(member_p->registration[i]->has_rtr_reg_id &&
                  member_p->registration[i]->rtr_reg_id);
        reg->rtr_reg_id = member_p->registration[i]->rtr_reg_id;
      }
    }
    if (member_p->registration[i]->has_promote_sub){
      RW_ASSERT((member_p->registration[i]->flags & RWDTS_FLAG_PUBLISHER) == 0);
      RW_ASSERT((reg->flags & RWDTS_FLAG_PUBLISHER) == 0);
      HASH_DELETE(hh_reg, memb->sub_registrations, reg);
      rwdts_shard_rts_promote_element(reg->shard, reg->chunk_id,
                                      reg->membid, memb->msgpath);
      reg->flags &= ~RWDTS_FLAG_SUBSCRIBER;
      reg->flags |= RWDTS_FLAG_PUBLISHER;
      HASH_ADD_KEYPTR(hh_reg, 
                      memb->registrations,
                      member_p->registration[i]->keyspec.data, 
                      member_p->registration[i]->keyspec.len,
                      reg);
    }
    
    if (ret_member_p) {
      ret_member_p->registration[i]->rtr_reg_id = reg->rtr_reg_id;
      ret_member_p->registration[i]->has_rtr_reg_id = TRUE;
    }

    PRINT_LOCAL_O("%s =>registering %s [%lu:%lu:%lu]", memb->msgpath, 
        reg->keystr, memb->router_idx, memb->client_idx, reg->rtr_reg_id);
  }
}


static rwdts_member_rsp_code_t
rwdts_router_reg_handle_prepare(const rwdts_xact_info_t* xact_info,
                                RWDtsQueryAction         action,
                                const rw_keyspec_path_t*      key,
                                const ProtobufCMessage*  msg,
                                uint32_t credits,
                                void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (xact_info == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)*)msg;
  rwdts_router_t *dts = (rwdts_router_t *)xact_info->ud;
  rwdts_router_member_t *memb=NULL;
  rwdts_xact_t *xact = xact_info->xact;

  RW_ASSERT_TYPE(dts, rwdts_router_t);
  if (dts->pend_peer_table) {
    return RWDTS_ACTION_INTERNAL;
  }

  RW_ASSERT(member_p);

  if (action == RWDTS_QUERY_DELETE) {
    bool all_regs = (member_p->has_recovery_action && (member_p->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL));
    uint8_t *bp = NULL;
    size_t bp_len = 0;
    int k;
    rwdts_router_reg_handle_delete(dts, xact, key, member_p);
    if (!all_regs) {
      for (k=0; k< member_p->n_registration; k++) {
        if (member_p->registration[k]->has_keyspec) {
          bp = member_p->registration[k]->keyspec.data;
          bp_len = member_p->registration[k]->keyspec.len;
          rwdts_router_remove_reg_keyspec(dts,key, member_p->registration[k]->flags, bp, bp_len); 
        }
      }
    }
    return RWDTS_ACTION_OK;
  }

  if (!member_p->n_registration) {
    return RWDTS_ACTION_OK;
  }

  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *init_member_p;

  init_member_p = (RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *)protobuf_c_message_duplicate(NULL, msg, msg->descriptor);

  memb = rwdts_router_get_router_member (dts, true,  member_p);

  // We should not be receiving a request without member name
  RW_ASSERT(memb);

  if (!memb) {
    return RWDTS_ACTION_NOT_OK;
  }

  RW_ASSERT(init_member_p->has_rtr_id);
  if (init_member_p->rtr_id == RWDTS_ROUTER_LOCAL_IDX(dts)) {
    RW_ASSERT(init_member_p->rtr_id == dts->router_idx);
    RW_ASSERT(!init_member_p->client_id);
    init_member_p->client_id = memb->client_idx;
    init_member_p->has_client_id = TRUE;
  }

  rwdts_router_update_registration (dts,  memb, member_p, init_member_p);
  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("%s __reg_handle_prepare  memb added with %ld(client_id):%ld(router_id)",
                                                 memb->msgpath, memb->client_idx, memb->router_idx));

  rwdts_router_store_registration(dts, memb, xact, init_member_p);

  memb->stats.reg_prepare_called++;

  //THIS WILL ACK it...and since this results in a async dispatch, the ack will go after
  // the below return code..
  rwdts_router_reg_response(xact_info, key, init_member_p);

  
  return RWDTS_ACTION_ASYNC;
}


static rwdts_member_rsp_code_t
rwdts_router_peer_reg_handle_prepare(const rwdts_xact_info_t* xact_info,
                                RWDtsQueryAction         action,
                                const rw_keyspec_path_t*      key,
                                const ProtobufCMessage*  msg,
                                uint32_t credits,
                                void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }

  RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member) *router_member_p = 
      (RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member)*)msg;
  rwdts_router_t *dts = (rwdts_router_t *)xact_info->ud;
  RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member) pathspec =
      *((RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)*) key);


  RW_ASSERT(pathspec.has_dompath &&
            pathspec.dompath.has_path001 &&
            pathspec.dompath.path001.has_key00);
 
  if (!(pathspec.has_dompath &&
        pathspec.dompath.has_path001 &&
        pathspec.dompath.path001.has_key00)) {
    return RWDTS_ACTION_NOT_OK;
  }
            
  if (!strcmp(pathspec.dompath.path001.key00.name, dts->rwmsgpath)) {
    PRINT_STR (dts->rwmsgpath, "%s", "Router NOT NA ed");
    return RWDTS_ACTION_OK;
  }
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p = ((RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)*)router_member_p);
  if (action == RWDTS_QUERY_DELETE) {
    RWPB_T_PATHSPEC(RwDts_data_RtrInitRegKeyspec_Member) del_ks  = 
        (*RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member));
    del_ks.has_dompath = TRUE;
    del_ks.dompath.has_path001 = TRUE;
    del_ks.dompath.path001.has_key00 = TRUE;
    strncpy(del_ks.dompath.path001.key00.name, pathspec.dompath.path002.key00.name, sizeof(del_ks.dompath.path001.key00.name));
    rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&del_ks;
    bool all_regs = (router_member_p->has_recovery_action && (router_member_p->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL));
    uint8_t *bp = NULL;
    size_t bp_len = 0;
    int k;
    rwdts_xact_t *xact = xact_info->xact;
    rwdts_router_reg_handle_delete(dts, xact, key, member_p);
    if (!all_regs) {
      for (k=0; k< router_member_p->n_registration; k++) {
        if (router_member_p->registration[k]->has_keyspec) {
          bp = router_member_p->registration[k]->keyspec.data;
          bp_len = router_member_p->registration[k]->keyspec.len;
          rwdts_router_remove_reg_keyspec(dts,key,router_member_p->registration[k]->flags,bp,bp_len); 
        }
      }
    }
    rwdts_router_reg_handle_delete (dts, xact, keyspec, member_p);
    return RWDTS_ACTION_OK;
  }

  

  rwdts_router_member_t *memb = rwdts_router_get_router_member (dts, false, member_p);
  rwdts_router_update_registration (dts,  memb, member_p, NULL);
  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact_info->xact, DtsrouterXactDebug, 
                              RWLOG_ATTR_SPRINTF("%s memb added with %ld(client_id):%ld(router_id)",
                                                 memb->msgpath, memb->client_idx, memb->router_idx));
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t rwdts_router_peer_reg_handle_precommit(const rwdts_xact_info_t* xact_info,
                                                              uint32_t  n_crec,
                                                              const rwdts_commit_record_t** crec)
{
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t rwdts_router_peer_reg_handle_commit(const rwdts_xact_info_t* xact_info,
                                                              uint32_t  n_crec,
                                                              const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_router_t *dts = (rwdts_router_t *)xact_info->ud;
  rwdts_xact_t *xact = xact_info->xact;

  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("%s is called", __func__));

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t rwdts_router_peer_reg_handle_abort(const rwdts_xact_info_t* xact_info,
                                                             uint32_t  n_crec,
                                                             const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);

  if (!xact_info) {
    return RWDTS_ACTION_NOT_OK;
  }
  rwdts_router_t *dts = (rwdts_router_t *)xact_info->ud;
  rwdts_xact_t *xact = xact_info->xact;

  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("%s is called", __func__));

  return RWDTS_ACTION_OK;
}

static void
rwdts_router_ksi_err_cb(rw_keyspec_instance_t *ksi, const char *msg)
{
  rwdts_router_t *dts = (rwdts_router_t *)ksi->user_data;
  if (dts)
  {
    RWLOG_EVENT(dts->rwtaskletinfo->rwlog_instance,
                RwDtsRouterLog_notif_NotifyKeyspecError,
                msg);
  }
}
static void rwdts_router_ksi_init(rwdts_router_t *dts)
{
  memset(&dts->ksi, 0, sizeof(rw_keyspec_instance_t));
  dts->ksi.error = rwdts_router_ksi_err_cb;
  dts->ksi.user_data = dts;
}

static void
rwdts_router_peer_reg_process(rwdts_xact_t*            xact,
                              const rw_keyspec_path_t* key,
                              const ProtobufCMessage*  msg,
                              rwdts_router_t*          dts)
{
  RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member) *router_member_p = 
      (RWPB_T_MSG(RwDts_data_RtrPeerRegKeyspec_Router_Member)*)msg;
  RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member) pathspec =
      *((RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)*) key);

  RW_ASSERT(pathspec.has_dompath &&
            pathspec.dompath.has_path001 &&
            pathspec.dompath.path001.has_key00 &&
            strcmp(pathspec.dompath.path001.key00.name, dts->rwmsgpath));
  RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member) *member_p = ((RWPB_T_MSG(RwDts_data_RtrInitRegKeyspec_Member)*)router_member_p);

  rwdts_router_member_t *memb = rwdts_router_get_router_member (dts, false, member_p);
  rwdts_router_update_registration (dts,  memb, member_p, NULL);
  RWDTS_ROUTER_LOG_XACT_EVENT(dts, xact, DtsrouterXactDebug,
                              RWLOG_ATTR_SPRINTF("%s __peer_reg_process memb added with %ld(client_id):%ld(router_id)",
                                                 memb->msgpath, memb->client_idx, memb->router_idx));
}

static void
rwdts_router_get_peer_regs_cb (rwdts_xact_t*        xact,
                               rwdts_xact_status_t* xact_status,
                               void*                ud)
{
  rw_status_t                  rs;

  rwdts_router_peer_reg_t *peer_reg_cb = (rwdts_router_peer_reg_t *)ud;
  RW_ASSERT_TYPE (peer_reg_cb, rwdts_router_peer_reg_t);

  rwdts_member_registration_t *reg = peer_reg_cb->reg;
  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  rwdts_router_t *dts = peer_reg_cb->dts;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  rwdts_api_t *apih = reg->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  ProtobufCMessage**           msgs = NULL;
  rw_keyspec_path_t**          keyspecs = NULL;
  uint32_t                     n_dts_data = 0;
  uint32_t                     result_count = 0;

  switch(xact_status->status) {
    case RWDTS_XACT_COMMITTED:
    case RWDTS_XACT_RUNNING:
      result_count = rwdts_xact_get_available_results_count(xact, NULL, 0);

      if (result_count == 0) {
        break;
      }
      RW_ASSERT(result_count);
      rs = rwdts_xact_get_result_pb_local(xact,
                                          0,
                                          reg,
                                          &msgs,
                                          &keyspecs,
                                          &n_dts_data);

      if (rs != RW_STATUS_SUCCESS) {
        RWTRACE_CRIT(apih->rwtrace_instance,
                     RWTRACE_CATEGORY_RWTASKLET,
                     "get result returned [%d]", rs);
        goto cleanup;
      }
      int num_msgs;
      for(num_msgs = 0; num_msgs < result_count; num_msgs++) {
        rwdts_router_peer_reg_process(xact, keyspecs[num_msgs], msgs[num_msgs], dts);
      }
      RWTRACE_INFO(apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "Got from peer %s @ %s %d messages", peer_reg_cb->peer_rwmsgpath, apih->client_path, num_msgs);

      break;

    case RWDTS_XACT_FAILURE:
      peer_reg_cb->retry_count++;
      xact = rwdts_api_query_ks(reg->apih,
                                peer_reg_cb->keyspec,
                                RWDTS_QUERY_READ,
                                (RWDTS_XACT_FLAG_PEER_REG
                                 |RWDTS_XACT_FLAG_STREAM), //|RWDTS_FLAG_TRACE, 
                                rwdts_router_get_peer_regs_cb,
                                peer_reg_cb,
                                NULL);
      RWTRACE_INFO(apih->rwtrace_instance,
                   RWTRACE_CATEGORY_RWTASKLET,
                   "Failure from peer %s @ %s %d retry", peer_reg_cb->peer_rwmsgpath, apih->client_path, peer_reg_cb->retry_count);
      PRINT_LOCAL("Peer pend unchanged %d value retry count %d", 
                  dts->pend_peer_table,
                  peer_reg_cb->retry_count);
      RWDTS_ROUTER_LOG_EVENT(dts, PeerRegFailure, "Failure from peer", peer_reg_cb->peer_rwmsgpath, peer_reg_cb->retry_count);
      return;
    case RWDTS_XACT_ABORTED:
    case RWDTS_XACT_INIT:
    PRINT_LOCAL("%p Bounced value", xact);
      break;
    default:
      RW_ASSERT_MESSAGE(0, "Unknown status %u\n", xact_status->status);
      break;
  }

cleanup:
  PRINT_STR(apih->client_path, "result %d with xact_done as %d", result_count, xact_status->xact_done);
  if (xact_status->xact_done == true) {
    dts->pend_peer_table--;
    RW_ASSERT (dts->pend_peer_table >= 0);
    PRINT_LOCAL("Peer pend after DEC %d value", dts->pend_peer_table);
    RW_FREE(peer_reg_cb->peer_rwmsgpath);
    rw_keyspec_path_free(peer_reg_cb->keyspec, NULL);    
    RW_FREE_TYPE(peer_reg_cb, rwdts_router_peer_reg_t);
    if (!dts->pend_peer_table) {
      rwdts_router_replay_queued_msgs(dts);
    }
  }
  if (msgs) RW_FREE(msgs);
  if (keyspecs) RW_FREE(keyspecs);
  return;
}

static void
rwdts_router_register_single_peer_router(rwdts_router_t *dts, int vm_id)
{
  RW_ASSERT(vm_id);

  char *peer_rwmsgpath;
  int r  = asprintf (&peer_rwmsgpath,"/R/RW.DTSRouter/%d", vm_id);
  RW_ASSERT(r);
  RW_ASSERT(peer_rwmsgpath);

  rwdts_router_member_t *memb=NULL;
  HASH_FIND(hh, dts->members, peer_rwmsgpath, strlen(peer_rwmsgpath), memb);
  if (!memb) {
    PRINT_STR(dts->rwmsgpath, "connecting to ===>> %s", peer_rwmsgpath);
    memb = RW_MALLOC0_TYPE(sizeof(memb[0]), rwdts_router_member_t);
    memb->msgpath = strdup(peer_rwmsgpath);
    memb->router_idx = vm_id;
    memb->client_idx = 1; /* Always the first client in the router is router itself */
    HASH_ADD_KEYPTR(hh, dts->members, memb->msgpath, strlen(memb->msgpath), memb);

    rw_keyspec_path_t *keyspec = NULL;
    rw_status_t rs =
        RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member),
                                   NULL , (rw_keyspec_path_t**)&keyspec, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi ,
                                  RW_SCHEMA_CATEGORY_DATA, NULL );
    RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member) *router_pub_ks  =
        (RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)*)keyspec;
    router_pub_ks->has_dompath = TRUE;
    router_pub_ks->dompath.has_path001 = TRUE;
    router_pub_ks->dompath.path001.has_key00 = TRUE;
    strncpy(router_pub_ks->dompath.path001.key00.name, peer_rwmsgpath,
            sizeof(router_pub_ks->dompath.path001.key00.name));

    char *keystr = NULL;
    rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &keystr);
    RW_ASSERT(memb);
    rwdts_router_member_registration_init(dts, memb, keyspec, keystr ? keystr : "",
                                          RWDTS_FLAG_PUBLISHER, FALSE);
    free(keystr);
    rw_keyspec_path_free(keyspec, NULL);
    keyspec = NULL;

    rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member),
                                    NULL, (rw_keyspec_path_t**)&keyspec, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );
    keystr = NULL;
    rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &keystr);
    rwdts_router_member_registration_init(dts, memb, keyspec, keystr ? keystr : "",
                                          RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, FALSE);
    free(keystr);
    rw_keyspec_path_free(keyspec, NULL);
    keyspec = NULL;

    rs =
        RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member),
                                   NULL , (rw_keyspec_path_t**)&keyspec, NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi ,
                                  RW_SCHEMA_CATEGORY_DATA, NULL );

    RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)* router_sub_ks  = 
        (RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)*)keyspec;
    router_sub_ks->has_dompath = TRUE;
    router_sub_ks->dompath.has_path001 = TRUE;
    router_sub_ks->dompath.path001.has_key00 = TRUE;
    strncpy(router_sub_ks->dompath.path001.key00.name, peer_rwmsgpath,
            sizeof(router_sub_ks->dompath.path001.key00.name));

    rwdts_router_peer_reg_t *peer_reg_cb = RW_MALLOC0_TYPE(sizeof(*peer_reg_cb),
                                                           rwdts_router_peer_reg_t);
    RW_ASSERT (peer_reg_cb);
    rwdts_member_registration_t *reg = (rwdts_member_registration_t *)dts->peer_regkeyh;
    peer_reg_cb->reg = reg;
    peer_reg_cb->dts = dts;
    peer_reg_cb->keyspec = keyspec;
    peer_reg_cb->peer_rwmsgpath = peer_rwmsgpath;
    rwdts_xact_t*  xact = rwdts_api_query_ks(reg->apih,
                                             peer_reg_cb->keyspec,
                                             RWDTS_QUERY_READ,
                                             (RWDTS_XACT_FLAG_PEER_REG
                                              |RWDTS_XACT_FLAG_STREAM), //|RWDTS_FLAG_TRACE,
                                             rwdts_router_get_peer_regs_cb,
                                             peer_reg_cb,
                                             NULL);
  RWTRACE_INFO(dts->apih->rwtrace_instance,
               RWTRACE_CATEGORY_RWTASKLET,
               "Get from peer %s @ %s", peer_reg_cb->peer_rwmsgpath, dts->rwmsgpath);
    RW_ASSERT(xact);
    dts->pend_peer_table++;
    PRINT_LOCAL("Peer pend after INC %d value", dts->pend_peer_table);
  }
}

void 
rwdts_router_test_register_single_peer_router(rwdts_router_t *dts, int vm_id)
{
  rwdts_router_register_single_peer_router(dts, vm_id);
}

static void
rwdts_router_data_watcher_f(void *ctx)
{
  rwdts_router_t *dts = (rwdts_router_t *)ctx;
  rwtasklet_info_ptr_t ti = dts->rwtaskletinfo;
  dts->data_watcher_cb_count++;

  char zklock[256] = {0};
  char *rwzk_path = "/sys-router/R/RW.DTSRouter/";
  char my_rwzk_path[256] = {0};
  struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };
  char ** children = NULL;;
  rw_status_t status;

  sprintf(zklock, "%s-lOcK/routerlock", dts->rwmsgpath);
  RW_ASSERT(rwvcs_rwzk_exists(ti->rwvcs, zklock));
  status = rwvcs_rwzk_lock_path(ti->rwvcs, zklock, &tv);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  sprintf(my_rwzk_path, "/sys-router%s", dts->rwmsgpath);
  status = rwvcs_rwzk_get_children(ti->rwvcs, rwzk_path, &children);
  RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_NOTFOUND);
  if (children) {
    int i=0;
    while (children[i] != NULL) {
      char check_rwzk_path[256] = {0};
      sprintf(check_rwzk_path, "%s%s", rwzk_path, children[i]);
      if (strcmp (check_rwzk_path, my_rwzk_path)) {
        char *rwzk_get_data = NULL;
        status = rwvcs_rwzk_get(ti->rwvcs, rwzk_path, &rwzk_get_data);
        rwdts_router_register_single_peer_router(dts, atoi(children[i]));
        free(rwzk_get_data);
      }
      free(children[i]);
      i++;
    }
    free(children);
  }

  status = rwvcs_rwzk_unlock_path(ti->rwvcs, zklock);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  return;
}

static void
rwdts_router_manage_peers(rwdts_router_t *dts,
                          rwvcs_zk_module_ptr_t rwvcs_zk)
{
  rwtasklet_info_ptr_t ti = dts->rwtaskletinfo;
  if (ti && ti->rwvcs) {
    char zklock[256] = {0};
    char *rwzk_path = "/sys-router/R/RW.DTSRouter/";
    char my_rwzk_path[256] = {0};
    char rwzk_set_data[256] = {0};
    struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };
    char ** children = NULL;;
    rw_status_t status;

    sprintf(zklock, "%s-lOcK/routerlock", dts->rwmsgpath);
    if (!rwvcs_rwzk_exists(ti->rwvcs, zklock)) {
      status = rwvcs_rwzk_create(ti->rwvcs, zklock);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
    RW_ASSERT(rwvcs_rwzk_exists(ti->rwvcs, zklock));
    status = rwvcs_rwzk_lock_path(ti->rwvcs, zklock, &tv);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    sprintf(my_rwzk_path, "/sys-router%s", dts->rwmsgpath);
    if (rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path)) {
      PRINT_STR(dts->rwmsgpath, "ERROR: Entry exist %s: Stale Instance", my_rwzk_path);
    }
    else {
    RW_ASSERT(!rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path));
    status = rwvcs_rwzk_create(ti->rwvcs, my_rwzk_path);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    }

    sprintf(rwzk_set_data, ":%u:%s:somemore", 0, "JUnk Str");
    status = rwvcs_rwzk_set(ti->rwvcs, my_rwzk_path, rwzk_set_data);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    status = rwvcs_rwzk_get_children(ti->rwvcs, rwzk_path, &children);
    RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_NOTFOUND);
    if (children) {
      int i=0;
      while (children[i] != NULL) {
        char check_rwzk_path[256] = {0};
        char new_rwzk_set_data[256] = {0};
        sprintf(check_rwzk_path, "%s%s", rwzk_path, children[i]);
        if (strcmp (my_rwzk_path, check_rwzk_path)) {
          char *rwzk_get_data;
          status = rwvcs_rwzk_get(ti->rwvcs, check_rwzk_path, &rwzk_get_data);
          RW_ASSERT(status == RW_STATUS_SUCCESS);
          char *p = rwzk_get_data;
          char *pcount_b = strchr(p, ':');
          RW_ASSERT(pcount_b);
          pcount_b++;
          p = pcount_b;
          char *pcount_e = strchr(p, ':');
          RW_ASSERT(pcount_e);
          *pcount_e = '\0';
          pcount_e++;
          p = pcount_e;
          int count = atoi(pcount_b+1);

          char *pnum = strrchr(p, ':');
          RW_ASSERT(pnum);
          pnum++;
          RW_ASSERT(pnum);

          status = rwvcs_rwzk_get(ti->rwvcs, check_rwzk_path, &rwzk_get_data);

          rwdts_router_register_single_peer_router(dts, atoi(children[i]));

          count++;
          sprintf(new_rwzk_set_data, ":%u:%s:somemore", count, pcount_e);
          status = rwvcs_rwzk_set(ti->rwvcs, check_rwzk_path, new_rwzk_set_data);
          RW_ASSERT(status == RW_STATUS_SUCCESS);
        }
        free(children[i]);
        i++;
      }
      free(children);
    }

    dts->closure = rwvcs_rwzk_watcher_start(
        ti->rwvcs, 
        my_rwzk_path, 
        dts->taskletinfo,
        dts->rwq,
        &rwdts_router_data_watcher_f, 
        (void *)dts);
    RW_ASSERT(dts->closure);
    status = rwvcs_rwzk_unlock_path(ti->rwvcs, zklock);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }
}

typedef struct rwdts_router_init_s{
  rwdts_router_t *dts;
  rwvcs_zk_module_ptr_t rwvcs_zk;
} rwdts_router_init_t;
static void rwdts_router_continue_init_f(void *ud);

rwdts_router_t *rwdts_router_init(rwmsg_endpoint_t *ep,
                                  rwsched_dispatch_queue_t rwq,
                                  rwtasklet_info_ptr_t rwtasklet_info,
                                  const char *rwmsgpath,
          rwvcs_zk_module_ptr_t rwvcs_zk,
          uint64_t router_idx)
{
  rwdts_router_t *dts = NULL;

  dts = RW_MALLOC0_TYPE(sizeof(*dts), rwdts_router_t);

  RW_ASSERT(ep);
  dts->ep = ep;
  dts->pend_peer_table = 0;

  RW_ASSERT(rwq);
  dts->rwq = rwq;
  dts->taskletinfo = rwtasklet_info->rwsched_tasklet_info;
  dts->rwtaskletinfo = rwtasklet_info;
  dts->rwsched = rwmsg_endpoint_get_rwsched(ep);
  INIT_DTS_CREDITS(dts->credits, 1000);
  dts->client_idx = 0;
  dts->router_idx = router_idx;
  dts->rtr_reg_id = 10;
  
  RW_CF_TYPE_VALIDATE(dts->rwsched, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(dts->taskletinfo, rwsched_tasklet_ptr_t);

  dts->rwq_ismain = (dts->rwq == rwsched_dispatch_get_main_queue(dts->rwsched));

  RW_ASSERT(rwmsgpath);
  dts->rwmsgpath = strdup(rwmsgpath);
  PRINT_LOCAL("Peer pend %d value", dts->pend_peer_table);

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, RWLOG_ATTR_SPRINTF("Starting rwdts router path='%s' idx=%lu", rwmsgpath, router_idx));
  rwdts_router_init_t *router_init_p = RW_MALLOC0_TYPE(sizeof(*router_init_p), rwdts_router_init_t);
  RW_ASSERT(router_init_p);
  router_init_p->dts = dts;
  router_init_p->rwvcs_zk = rwvcs_zk;
  if (!dts->rwq_ismain) {
    rwsched_dispatch_async_f(dts->taskletinfo, dts->rwq, router_init_p,
                             rwdts_router_continue_init_f);
  }
  else {
    rwdts_router_continue_init_f(router_init_p);
  }
  return dts;
}

static void rwdts_router_state_changed(rwdts_api_t *apih,
				       rwdts_state_t state,
				       void*         user_data) {
  switch (state) {
  case RW_DTS_STATE_INIT:
    rwdts_api_set_state(apih, RW_DTS_STATE_REGN_COMPLETE);
    break;
  case RW_DTS_STATE_CONFIG:
    rwdts_api_set_state(apih, RW_DTS_STATE_RUN);
    break;
  default:
    break;
  }
}

static void rwdts_router_continue_init_f(void *ud)
{
  rwdts_router_init_t *router_init = (rwdts_router_init_t *)ud;
  RW_ASSERT_TYPE(router_init, rwdts_router_init_t);
  rwdts_router_t *dts = router_init->dts;
  rwvcs_zk_module_ptr_t rwvcs_zk = router_init->rwvcs_zk;
  RW_FREE_TYPE(router_init, rwdts_router_init_t);
  rw_status_t rs = RW_STATUS_FAILURE;

  /* Init member API; we are effectively speaking to ourselves, but no
     reason that should really matter. */
  rwdts_state_change_cb_t cbset = {
    .cb = rwdts_router_state_changed,
    .ud = dts
  };
  dts->apih = rwdts_api_init_internal(dts->rwtaskletinfo,
                                      dts->rwq,
                                      dts->rwtaskletinfo->rwsched_tasklet_info,
                                      dts->rwtaskletinfo->rwsched_instance,
                                      dts->rwtaskletinfo->rwmsg_endpoint,
                                      dts->rwmsgpath,
                                      dts->router_idx,
                                      0, 
				      &cbset);
  RW_ASSERT(dts->apih);
  rwdts_api_set_ypbc_schema(dts->apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwDts));
  RW_ASSERT(RWDTS_ROUTER_LOCAL_IDX(dts) == dts->router_idx);

  /* May be async */
  rs = rwdts_router_service_init(dts);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);


  RWDTS_ROUTER_LOG_EVENT(dts, RouterStarted, "DTS Router started");

  dts->rwmemlog = rwmemlog_instance_alloc(dts->rwmsgpath, 0, 0);
  RW_ASSERT(dts->rwmemlog);
  rwmemlog_instance_dts_register(dts->rwmemlog, dts->apih->rwtasklet_info, dts->apih);
  //initialize mem
  dts->rwml_buffer = rwmemlog_instance_get_buffer(dts->rwmemlog, "Router Xact", 0);
  
  
  rwdts_member_event_cb_t cb = { .cb = { rwdts_router_stats_prepare, NULL, NULL, NULL, NULL }, .ud = dts };

  RWPB_T_PATHSPEC(RwDts_data_Dts_Routers) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(RwDts_data_Dts_Routers));
  /* register for DTS state */

  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, dts->rwmsgpath, sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, NULL , RW_SCHEMA_CATEGORY_DATA, NULL );

  rwdts_member_reg_handle_t reg = rwdts_member_register(NULL, dts->apih,
                                                        keyspec,
                                                        &cb,
                                                        RWPB_G_MSG_PBCMD(RwDts_data_Dts_Routers),
                                                        RWDTS_FLAG_PUBLISHER,
                                                        NULL);
  RW_ASSERT(reg);
  dts->apih_reg = reg;

  rwdts_member_event_cb_t clear_cb = {.cb = { rwdts_router_handle_clear_stats_req, NULL, NULL, NULL, NULL }, .ud = dts };
  RWPB_T_PATHSPEC(RwDts_input_ClearDts_Stats_Router) *clear_ks = NULL;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_input_ClearDts_Stats_Router),
                                  NULL, (rw_keyspec_path_t**)&clear_ks);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  rw_keyspec_path_set_category ((rw_keyspec_path_t*)clear_ks, NULL, RW_SCHEMA_CATEGORY_RPC_INPUT);

  rwdts_member_register(NULL, dts->apih,
                        (rw_keyspec_path_t *)clear_ks,
                        &clear_cb,
                        RWPB_G_MSG_PBCMD(RwDts_input_ClearDts_Stats_Router),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
                        NULL);

  rw_keyspec_path_free((rw_keyspec_path_t*)clear_ks, NULL);


  //register to receive config changes
  rwdts_router_config_mgmt_init(dts);

  //Initialize the stale timer
  rwdts_router_stale_xact_init(dts);
  
  keyspec = NULL;
  rwdts_router_member_t *memb=NULL;
  memb = RW_MALLOC0_TYPE(sizeof(memb[0]), rwdts_router_member_t);
  memb->msgpath = strdup(dts->rwmsgpath);
  
  memb->client_idx = ++dts->client_idx;
  memb->router_idx  = RWDTS_ROUTER_LOCAL_IDX(dts);
  HASH_ADD_KEYPTR(hh, dts->members, memb->msgpath, strlen(memb->msgpath), memb);

  /* Free the init_regidh and init_regkeyh here first */

  rwdts_member_event_cb_t cb1 = {
    .cb = {
      rwdts_router_reg_handle_prepare,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL
    },
    .ud = dts
  };

  rw_keyspec_path_t *keyspec1 = NULL;

  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegKeyspec_Member), NULL ,
                             (rw_keyspec_path_t**)&keyspec1, NULL);

  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec1, &dts->apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );

  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  rwdts_router_ksi_init(dts);

  dts->stat_timer = rwdts_timer_create(dts,
                                       RWDTS_STATS_TIMEOUT,
                                       rwdts_router_calc_stat_timer,
                                       dts,
                                       1);

  dts->init_regkeyh =
      (rwdts_member_reg_handle_t )rwdts_member_registration_init_local(dts->apih,
                                                                       keyspec1,
                                                                       &cb1,
                                                                       RWDTS_FLAG_SUBSCRIBER,
                                                                       RWPB_G_MSG_PBCMD(RwDts_data_RtrInitRegKeyspec_Member));

#ifdef RWDTS_SHARDING
  rwdts_member_registration_t *reg1 = (rwdts_member_registration_t *) dts->init_regkeyh;
  RW_ASSERT(!reg1->shard);
//
// debug stuff start
// char *ks_str= NULL;
// rw_yang_pb_schema_t* debug_schema = (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwDts);
// rw_keyspec_path_get_new_print_buffer( ((rwdts_member_registration_t*)dts->init_regkeyh)->keyspec, NULL , debug_schema, &ks_str);
// printf("Shard Init router-%s :ks=%s\n",dts->rwmsgpath, ks_str);
// free(ks_str);
// debug stuff end
//
  reg1->shard = rwdts_shard_key_init(dts->init_regkeyh, NULL,
                                     RWDTS_FLAG_SUBSCRIBER, -1 , &dts->apih->rootshard, RW_DTS_SHARD_FLAVOR_NULL,NULL,
                                     RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN, NULL);
  RW_ASSERT(reg1->shard);
  rwdts_shard_chunk_info_t *chunk = rwdts_shard_add_chunk(reg1->shard,NULL);
  RW_ASSERT(chunk);
  rwdts_chunk_member_info_t mbr_info;
  mbr_info.member = (rwdts_member_reg_handle_t) reg1;
  mbr_info.flags = reg1->flags;
  if (reg1->flags & RWDTS_FLAG_PUBLISHER) {
    rwdts_shard_member_create_element(reg1->shard, chunk, &mbr_info,
                                      true, &chunk->chunk_id,
                                      reg1->apih->client_path);
  }
  if (reg1->flags & RWDTS_FLAG_SUBSCRIBER) {
    rwdts_shard_member_create_element(reg1->shard, chunk, &mbr_info,
                                      false, &chunk->chunk_id,
                                      reg1->apih->client_path);
  }

#endif

  char *keystr1 = NULL;
  rw_keyspec_path_get_new_print_buffer(keyspec1, NULL , NULL, &keystr1);

  rwdts_router_member_registration_init(dts, memb, keyspec1, keystr1 ? keystr1 : "",
                                        RWDTS_FLAG_SUBSCRIBER, TRUE);
  rw_keyspec_path_free(keyspec1, NULL);

  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrInitRegId_Member_Registration), NULL ,
                             (rw_keyspec_path_t**)&keyspec, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  free(keystr1);
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );

  dts->init_regidh = (rwdts_member_reg_handle_t )rwdts_member_registration_init_local(dts->apih, keyspec, &cb1, RWDTS_FLAG_SUBSCRIBER, RWPB_G_MSG_PBCMD(RwDts_data_RtrInitRegId_Member_Registration));
  rwdts_log_payload_init(&dts->payload_stats);

#ifdef RWDTS_SHARDING
  rwdts_member_registration_t *reg2 = (rwdts_member_registration_t *) dts->init_regidh;
  RW_ASSERT(!reg2->shard);
  reg2->shard = rwdts_shard_key_init(dts->init_regidh, NULL,
                                     RWDTS_FLAG_SUBSCRIBER, -1, &dts->apih->rootshard, RW_DTS_SHARD_FLAVOR_NULL,NULL,
                                     RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN, NULL);
  RW_ASSERT(reg2->shard);
  chunk = rwdts_shard_add_chunk(reg2->shard,NULL);
  RW_ASSERT(chunk);
  mbr_info.member = (rwdts_member_reg_handle_t) reg2;
  mbr_info.flags = reg2->flags;
  if (reg2->flags & RWDTS_FLAG_PUBLISHER) {
    rwdts_shard_member_create_element(reg2->shard, chunk, &mbr_info,
                                      true, &chunk->chunk_id,
                                      reg2->apih->client_path);
  }
  if (reg2->flags & RWDTS_FLAG_SUBSCRIBER) {
    rwdts_shard_member_create_element(reg2->shard, chunk, &mbr_info,
                                      false, &chunk->chunk_id,
                                      reg2->apih->client_path);
  }

#endif

  char *keystr = NULL;
  rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &keystr);

  rwdts_router_member_registration_init(dts, memb, keyspec, keystr ? keystr : "",
                                        RWDTS_FLAG_SUBSCRIBER, FALSE);

  RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterDebug, RWLOG_ATTR_SPRINTF("DTS router at '%s' registered for routerstats (?)", dts->rwmsgpath));
  free(keystr);
  rw_keyspec_path_free(keyspec, NULL);
  keyspec = NULL;

  rwdts_member_event_cb_t cb2 = {
    .cb = {
      NULL,
      NULL,
      rwdts_router_peer_reg_handle_precommit,
      rwdts_router_peer_reg_handle_commit,
      rwdts_router_peer_reg_handle_abort,
      NULL
    },
    .ud = dts
  };

  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member), NULL ,
                             (rw_keyspec_path_t**)&keyspec, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );
  RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member) *router_pub_ks  = (RWPB_T_PATHSPEC(RwDts_data_RtrPeerRegKeyspec_Router_Member)*)keyspec;
  router_pub_ks->has_dompath = TRUE;
  router_pub_ks->dompath.has_path001 = TRUE;
  router_pub_ks->dompath.path001.has_key00 = TRUE;
  strncpy(router_pub_ks->dompath.path001.key00.name, dts->rwmsgpath, sizeof(router_pub_ks->dompath.path001.key00.name));

  dts->local_regkeyh =
      (rwdts_member_reg_handle_t )rwdts_member_registration_init_local(dts->apih,
                                                                       keyspec,
                                                                       &cb2,
                                                                       RWDTS_FLAG_PUBLISHER,
                                                                       RWPB_G_MSG_PBCMD(RwDts_data_RtrPeerRegKeyspec_Router_Member));
 rw_keyspec_path_free(keyspec, NULL);  
 keyspec = NULL;
#ifdef RWDTS_SHARDING
  rwdts_member_registration_t *reg3 = (rwdts_member_registration_t *) dts->local_regkeyh;
  reg3->shard = rwdts_shard_key_init(dts->local_regkeyh, NULL,
                                     RWDTS_FLAG_PUBLISHER, -1, &dts->apih->rootshard, RW_DTS_SHARD_FLAVOR_NULL,NULL,
                                     RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN, NULL);
  RW_ASSERT(reg3->shard);
  chunk = rwdts_shard_add_chunk(reg3->shard,NULL);
  RW_ASSERT(chunk);
  mbr_info.member = (rwdts_member_reg_handle_t) reg3;
  mbr_info.flags = reg3->flags;
  if (reg3->flags & RWDTS_FLAG_PUBLISHER) {
    rwdts_shard_member_create_element(reg3->shard, chunk, &mbr_info,
                                      true, &chunk->chunk_id,
                                      reg3->apih->client_path);
  }
  if (reg3->flags & RWDTS_FLAG_SUBSCRIBER) {
    rwdts_shard_member_create_element(reg3->shard, chunk, &mbr_info,
                                      false, &chunk->chunk_id,
                                      reg3->apih->client_path);
 }

#endif

  rwdts_member_event_cb_t cb3 = {
    .cb = {
      rwdts_router_peer_reg_handle_prepare,
      NULL,
      rwdts_router_peer_reg_handle_precommit,
      rwdts_router_peer_reg_handle_commit,
      rwdts_router_peer_reg_handle_abort,
      NULL
    },
    .ud = dts
  };

  /* Perform a member only registration at router */
  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDts_data_RtrPeerRegKeyspec_Router_Member), NULL ,
                             (rw_keyspec_path_t**)&keyspec, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_KEYSPEC_PATH_SET_CATEGORY ((rw_keyspec_path_t*)keyspec, &dts->apih->ksi , RW_SCHEMA_CATEGORY_DATA, NULL );
  dts->peer_regkeyh =
      (rwdts_member_reg_handle_t )rwdts_member_registration_init_local(dts->apih,
                                                                       keyspec,
                                                                       &cb3,
                                                                       RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
                                                                       RWPB_G_MSG_PBCMD(RwDts_data_RtrPeerRegKeyspec_Router_Member));
 rw_keyspec_path_free(keyspec, NULL);  
 keyspec = NULL;
#ifdef RWDTS_SHARDING
  rwdts_member_registration_t *reg4 = (rwdts_member_registration_t *) dts->peer_regkeyh;
  reg4->shard = rwdts_shard_key_init(dts->peer_regkeyh, NULL,
                                     RWDTS_FLAG_SUBSCRIBER, -1, &dts->apih->rootshard, RW_DTS_SHARD_FLAVOR_NULL,NULL,
                                     RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN, NULL);
  RW_ASSERT(reg4->shard);
  chunk = rwdts_shard_add_chunk(reg4->shard,NULL);
  RW_ASSERT(chunk);
  mbr_info.member = (rwdts_member_reg_handle_t) reg4;
  mbr_info.flags = reg4->flags;
  if (reg4->flags & RWDTS_FLAG_PUBLISHER) {
    rwdts_shard_member_create_element(reg4->shard, chunk, &mbr_info,
                                      true, &chunk->chunk_id,
                                      reg4->apih->client_path);
  }
  if (reg4->flags & RWDTS_FLAG_SUBSCRIBER) {
    rwdts_shard_member_create_element(reg4->shard, chunk, &mbr_info,
                                      false, &chunk->chunk_id,
                                      reg4->apih->client_path);
 }

#endif

  rwdts_router_manage_peers (dts, rwvcs_zk);

  rwdts_api_member_register_xpath (dts->apih,
      RWDTS_API_VCS_INSTANCE_CHILD_STATE,
      NULL,
      RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
      RW_DTS_SHARD_FLAVOR_NULL, NULL, 0,-1,0,0,
      rwdts_router_publish_state_reg_ready,
      rwdts_router_publish_state_prepare,
			NULL, NULL, NULL,
      dts,
      &(dts->publish_state_regh));
}

void rwdts_router_deinit(rwdts_router_t *dts) {
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT_TYPE(dts, rwdts_router_t);

  rwtasklet_info_ptr_t ti = dts->rwtaskletinfo;
  char zklock[256] = {0};
  char my_rwzk_path[256] = {0};

  sprintf(zklock, "%s-lOcK/routerlock", dts->rwmsgpath);
  if (ti->rwvcs) {
    RW_ASSERT(rwvcs_rwzk_exists(ti->rwvcs, zklock));

    sprintf(my_rwzk_path, "/sys-router%s", dts->rwmsgpath);
    RW_ASSERT (rwvcs_rwzk_exists(ti->rwvcs, my_rwzk_path));

    rs = rwvcs_rwzk_watcher_stop(ti->rwvcs, my_rwzk_path, &dts->closure);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    rs = rwvcs_rwzk_delete_path(ti->rwvcs, my_rwzk_path);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    rs = rwvcs_rwzk_delete_path(ti->rwvcs, zklock);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }

  /* Handles dispatch to rwq as needed */
  rs = rwdts_router_service_deinit(dts);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
}


rwdts_router_t*
rwdtsrouter_instance_start(rwtasklet_info_ptr_t  rwtasklet_info,
                           int instance_id,
                           rwvcs_zk_module_ptr_t rwvcs_zk)
{
  char path[256] = {0};
  char rwq_name[256] = {0};
  if (!instance_id) {
    instance_id = 1;
  }
  snprintf(path, sizeof(path), "/R/RW.DTSRouter/%d", instance_id);

  rwsched_dispatch_queue_t rwq = NULL;
  if (getenv("RWDTS_ROUTER_MAINQ")) {
    fprintf(stderr, "DTS Router scheduler queue overridden; RWDTS_ROUTER_MAINQ is set\n");
    rwq = rwsched_dispatch_get_main_queue(rwmsg_endpoint_get_rwsched(rwtasklet_info->rwmsg_endpoint));
  } else {
    snprintf(rwq_name, sizeof(rwq_name), "dtsrouterq-%d", instance_id);
    rwq = rwsched_dispatch_queue_create(rwtasklet_info->rwsched_tasklet_info,
					rwq_name, DISPATCH_QUEUE_SERIAL);
  }

  RW_ASSERT(rwq);
  return (rwdts_router_init(rwtasklet_info->rwmsg_endpoint,
                            rwq,
                            rwtasklet_info,
                            path,
                            rwvcs_zk,
                            instance_id));
}


void *rwdts_test_router_init(rwmsg_endpoint_t *ep,
                             rwsched_dispatch_queue_t rwq,
                             void *taskletinfo,
                             int instance_id,
                             const char *rwmsgpath)
{
  return rwdts_router_init(ep, rwq, (rwtasklet_info_ptr_t)taskletinfo, rwmsgpath, NULL, instance_id);
}

void rwdts_test_router_deinit(void *dts) {
  return rwdts_router_deinit((rwdts_router_t *)dts);
}

void rwdts_router_test_misc(rwdts_router_t *dts) {
  rwdts_xact_info_t xi = { .ud = dts };

  RwDts__YangData__RwDts__Dts__Routers msg = { };
  rw_dts__yang_data__rw_dts__dts__routers__init(&msg);
  rwdts_router_stats_prepare(&xi, RWDTS_QUERY_READ, NULL, (ProtobufCMessage*)&msg, 0, NULL);
  protobuf_c_message_free_unpacked_usebody(NULL, &msg.base);

  //...
}

static void rwdts_router_calc_stat_timer(void *ctx)
{
  rwdts_router_t *dts = (rwdts_router_t *)ctx;
  RW_ASSERT(dts);

  dts->current_idx++;

  uint64_t diff_ct = dts->stats.total_xact - dts->stats.last_xact_count;
  dts->stats.topx_xact_histo[dts->current_5s_idx%5] = diff_ct;
  dts->stats.last_xact_count = dts->stats.total_xact;

  if (dts->stats.topx_latency_xact_count) {
    dts->stats.topx_latency_histo[dts->current_5s_idx%5] = dts->stats.topx_latency_inc/dts->stats.topx_latency_xact_count;
  } else {
    dts->stats.topx_latency_histo[dts->current_5s_idx%5] = 0;
  }

  dts->stats.topx_latency_inc = 0;
  dts->stats.topx_latency_xact_count = 0;

  if ((dts->current_idx%3) == 0) {
    uint64_t diff_ct = dts->stats.total_xact - dts->stats.last_xact_15s_count;
    dts->stats.topx_xact_15s_histo[dts->current_15s_idx%5] = diff_ct;
    dts->stats.last_xact_15s_count = dts->stats.total_xact;

    if (dts->stats.topx_latency_xact_15s_count) {
      dts->stats.topx_latency_15s_histo[dts->current_15s_idx%5] = dts->stats.topx_latency_15s_inc/dts->stats.topx_latency_xact_15s_count;
    } else {
      dts->stats.topx_latency_15s_histo[dts->current_15s_idx%5] = 0;
    }

    dts->stats.topx_latency_xact_15s_count = 0;
    dts->stats.topx_latency_15s_inc = 0;

    if (dts->current_15s_idx >= 5) {
      dts->current_15s_idx = 0;
    }
  }

  if ((dts->current_idx%10) == 0) {
    uint64_t diff_ct = dts->stats.total_xact - dts->stats.last_xact_1m_count;
    dts->stats.topx_xact_1m_histo[dts->current_1m_idx%6] = diff_ct;
    dts->stats.last_xact_1m_count = dts->stats.total_xact;

    if (dts->stats.topx_latency_xact_1m_count) {
      dts->stats.topx_latency_1m_histo[dts->current_1m_idx%6] = dts->stats.topx_latency_1m_inc/dts->stats.topx_latency_xact_1m_count;
    } else {
      dts->stats.topx_latency_1m_histo[dts->current_1m_idx%6] = 0;
    }

    dts->stats.topx_latency_1m_inc = 0;
    dts->stats.topx_latency_xact_1m_count = 0;

    dts->current_1m_idx++;
    if (dts->current_1m_idx >= 6) {
      dts->current_1m_idx = 0;
    }
  }

  if ((dts->current_idx%30) == 0) {
    uint64_t diff_ct = dts->stats.total_xact - dts->stats.last_xact_5m_count;
    dts->stats.topx_xact_5m_histo[dts->current_5m_idx%10] = diff_ct;
    dts->stats.last_xact_5m_count = dts->stats.total_xact;

    if (dts->stats.topx_latency_xact_5m_count) {
      dts->stats.topx_latency_5m_histo[dts->current_5m_idx%10] = dts->stats.topx_latency_5m_inc/dts->stats.topx_latency_xact_5m_count;
    } else {
      dts->stats.topx_latency_5m_histo[dts->current_5m_idx%10] = 0;
    }

    dts->stats.topx_latency_5m_inc = 0;
    dts->stats.topx_latency_xact_5m_count = 0;

    dts->current_5m_idx++;
    if (dts->current_5m_idx >= 10) {
      dts->current_5m_idx = 0;
    }
  }

  dts->current_5s_idx++;
  if (dts->current_5s_idx >= 5) {
    dts->current_5s_idx = 0;
  }

  if (dts->current_idx >= 30) {
    dts->current_idx = 0;
  }

  return;
}

static void rwdts_router_publish_state_handler(
    rwdts_router_t *dts,
    RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state,
    RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *keyspec_entry)
{
  if (publish_state) {
    RW_ASSERT(keyspec_entry->has_dompath &&
              keyspec_entry->dompath.has_path003 &&
              keyspec_entry->dompath.path003.has_key00 &&
              keyspec_entry->dompath.path003.key00.instance_name);

    char *instance_path = to_instance_path(keyspec_entry->dompath.path003.key00.instance_name);
    char *msgpath = NULL;
    int r = asprintf (&msgpath, "/R/%s", instance_path);
    RW_ASSERT(r > 0 );
    RW_FREE(instance_path);
  
    rwdts_router_member_t *memb;
    HASH_FIND(hh, dts->members, msgpath, strlen(msgpath), memb);
    RW_FREE(msgpath);

    if (memb) {
      memb->component_state = publish_state->state;
      if (publish_state->has_state) {
        if (publish_state->state == RW_BASE_STATE_TYPE_TO_RECOVER) {
          memb->to_recover = true;
        }
        else if (memb->to_recover 
                 && memb->wait_restart 
                 && (publish_state->state >= RW_BASE_STATE_TYPE_RECOVERING)) {
          memb->wait_restart = false;
          memb->to_recover = false;
          rwdts_restart_end_notify(dts->apih, memb->msgpath);
          RWDTS_ROUTER_LOG_EVENT(dts, DtsrouterCritical, RWLOG_ATTR_SPRINTF("%s member clears wait recovery", memb->msgpath));
          RWTRACE_CRIT(
              dts->apih->rwtrace_instance,
              RWTRACE_CATEGORY_RWTASKLET,
              "%s [router] recovery done and unpause %s [member]", 
              dts->rwmsgpath, 
              memb->msgpath
              );
          rwdts_router_queue_xact_t *queue_xact = 
              RW_MALLOC0_TYPE(sizeof(rwdts_router_queue_xact_t), rwdts_router_queue_xact_t);
          RW_ASSERT_TYPE(queue_xact, rwdts_router_queue_xact_t);
          queue_xact->dts = dts;
          queue_xact->member = memb;
          rwsched_dispatch_async_f(
              dts->taskletinfo, 
              dts->rwq, 
              queue_xact, 
              rwdts_router_process_queued_xact_f);
        }  
      }
    }
  }
}

static void rwdts_router_publish_state_reg_ready(
    rwdts_member_reg_handle_t  regh,
    rw_status_t                rs,
    void*                      user_data)
{
  rwdts_router_t *dts = (rwdts_router_t *)user_data;

  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = NULL;
  while ((publish_state = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState)*) rwdts_member_reg_handle_get_next_element(
                                                                                                regh,
                                                                                                cur,
                                                                                                NULL,
                                                                                                &elem_ks))) {
    rwdts_router_publish_state_handler(
        dts,
        publish_state,
        (RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *)elem_ks);
  }
  rwdts_member_data_delete_cursors(NULL, regh);
  return;
}

static rwdts_member_rsp_code_t rwdts_router_publish_state_prepare(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* keyspec,
    const ProtobufCMessage*  msg,
    void*                    user_data)
{
  rwdts_router_t *dts = (rwdts_router_t *)user_data;

  rwdts_member_rsp_code_t dts_ret = RWDTS_ACTION_OK;

  rwdts_api_t *api = xact_info->apih; 
  rwdts_xact_t *xact = xact_info->xact; 
  rwdts_member_reg_handle_t regh = xact_info->regh;

  RW_ASSERT_MESSAGE((api != NULL), "rwdts_router_publish_state_prepare:invalid api");
  if (api == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT_MESSAGE((xact != NULL), "rwdts_router_publish_state_prepare:invalid xact");
  if (xact == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }
  RW_ASSERT_MESSAGE((regh != NULL),"rwdts_router_publish_state_prepare:invalid regh");
  if (regh == NULL) {
    return RWDTS_ACTION_NOT_OK;
  }

  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *publish_state = 
   (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN_PublishState) *)msg;
  
  rwdts_router_publish_state_handler(
      dts,
      publish_state,
      (RWPB_T_PATHSPEC(RwBase_VcsInstance_Instance_ChildN_PublishState) *)keyspec);
    
  return dts_ret;
}



static void
rwdts_restart_begin_notify(rwdts_api_t *apih, char *memb) 
{
  RWPB_M_MSG_DECL_PTR_ALLOC(RwDts_notif_TaskRestartBegin, alarm);
  alarm->name = strdup(memb);
  rwdts_api_send_notification(apih, &(alarm->base));
  protobuf_c_message_free_unpacked(NULL, &(alarm->base));
}

static void
rwdts_restart_end_notify(rwdts_api_t *apih, char *memb)
{
  RWPB_M_MSG_DECL_PTR_ALLOC(RwDts_notif_TaskRestarted, alarm);
  alarm->name = strdup(memb);
  rwdts_api_send_notification(apih, &(alarm->base));
  protobuf_c_message_free_unpacked(NULL, &(alarm->base));
}



//LCOV_EXCL_STOP

