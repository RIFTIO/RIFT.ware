
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwlogd_display.h"
#include "rwlogd_utils.h"


extern "C" 
{

typedef struct {
  ProtobufCMessage *log_input;
  struct timeval start_t;
  void *xact;
  void *queryh;
}rwlogd_fetch_ud;

static rw_status_t rwlogd_merge_and_get_logs(rwdts_xact_t *xact,
                                             RWPB_T_MSG(RwlogMgmt_output_ShowLogs) **output_logs,
                                             ProtobufCMessage *log_input,
                                             struct timeval *start_t);

static void
rwlogd_fetch_logs_cb(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  rwlogd_fetch_ud *userdata = (rwlogd_fetch_ud *)ud;
  rwdts_xact_t *cb_xact = (rwdts_xact_t *)userdata->xact;
  rwdts_member_query_rsp_t rsp;
  RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *output_logs = NULL;
  ProtobufCMessage *output_msgs[1];
  int i = 0;

  //  fprintf(stderr, "%%%%%% rwlogd_fetch_logs_cb .more=%d .status=%d\n", rwdtsstatus.more, rwdtsstatus.status);

  /* This bit doesn't work, but it should.  Rajesh V's new API will provide some decent clarity around end of transactions */
#if 0
  if (rwdtsstatus.status == RWDTS_XACT_RUNNING || rwdtsstatus.status == RWDTS_XACT_INIT) {
    return;
  } else {
    switch (rwdtsstatus.status) {
    case RWDTS_XACT_COMMITTED:
    case RWDTS_XACT_ABORTED:
    case RWDTS_XACT_FAILURE:
      break;
    default:
      RW_CRASH();
      break;
    }
  }
#endif

#if 1
  if (!xact_status->xact_done) {
    return;
  }
#endif

  RW_ASSERT(userdata->queryh);
  if (!userdata->queryh) {
    return;
  }
  rwdts_query_handle_t cb_queryh = (rwdts_query_handle_t)userdata->queryh;
  userdata->queryh = NULL;
  RW_ASSERT_TYPE(cb_queryh, rwdts_match_info_t);

  rwlogd_merge_and_get_logs(xact, &output_logs,userdata->log_input,&userdata->start_t);
  RW_ASSERT_TYPE(cb_queryh, rwdts_match_info_t);

  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwlogMgmt_output_ShowLogs);
  rsp.msgs = output_msgs;
  rsp.msgs[0] = (ProtobufCMessage *)output_logs;
  rsp.n_msgs = 1;
  rsp.ks = ks;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  RW_ASSERT_TYPE(cb_queryh, rwdts_match_info_t);
  rw_status_t rs_status = rwdts_member_send_response(cb_xact, cb_queryh, &rsp);
  RW_ASSERT(rs_status == RW_STATUS_SUCCESS);

  protobuf_c_message_free_unpacked(NULL, userdata->log_input);
  RW_FREE(userdata);

  RW_FREE(output_logs->logs);
  if (output_logs->n_log_summary) {
    for(i =0;i<(int)output_logs->n_log_summary; i++) {
      RW_FREE(output_logs->log_summary[i]->trailing_timestamp);
      RW_FREE(output_logs->log_summary[i]);
    }
  }
  RW_FREE(output_logs->log_summary);
  RW_FREE(output_logs);

}

rw_status_t
rwlogd_fetch_logs_from_other_rwlogd(const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg)
{
  RW_ASSERT_TYPE(xact_info->queryh, rwdts_match_info_t);
  rw_status_t rs;
  UNUSED(key);
  rwlogd_fetch_ud *ud = (rwlogd_fetch_ud *)RW_MALLOC0(sizeof(rwlogd_fetch_ud));

  RW_ASSERT(ud);
  if (!ud) {
    return RW_STATUS_FAILURE;
  }

  ud->log_input = protobuf_c_message_duplicate(NULL,
                                           msg,
                                           msg->descriptor);

  ud->xact = (void *)xact_info->xact;
  ud->queryh = (void *)xact_info->queryh;
  RW_ASSERT_TYPE(ud->queryh, rwdts_match_info_t);
  gettimeofday(&ud->start_t, NULL);

  rw_keyspec_path_t *ks =
      (rw_keyspec_path_t *) RWPB_G_PATHSPEC_VALUE(RwLog_input_ShowLogsInternal);

  rwdts_xact_t *xact =  rwdts_api_query_ks(xact_info->apih,ks,
                                RWDTS_QUERY_RPC,0,
                                rwlogd_fetch_logs_cb,ud,msg);

  rs = (xact) ? RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
  return rs;
}




static bool cmp_show_logts( RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) * v1, 
                            RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) * v2)
{

  if (v1->tv_sec && v2->tv_sec) {
    if(v1->tv_sec < v2->tv_sec) {
     return TRUE;
    }else if(v1->tv_sec >  v2->tv_sec) {
     return FALSE;
    }
  }
  if (v1->tv_usec && v2->tv_usec) {
    if(v1->tv_usec < v2->tv_usec) {
      return TRUE;
    }
    else if (v1->tv_usec > v2->tv_usec) {
     return FALSE;
    }
  }
  if (v1->hostname[0] && v2->hostname[0] ) {
    int res = strncmp(v1->hostname,v2->hostname,sizeof(v1->hostname));
    if(res < 0) {
      return TRUE;
    }
    else if(res > 0) {
      return FALSE;
    }
  }
  if (v1->process_id && v2->process_id) {
    if(v1->process_id < v2->process_id) {
      return TRUE;
    }
    else if(v1->process_id > v2->process_id) {
      return FALSE;
    }
  }
  if (v1->thread_id && v2->thread_id) {
    if(v1->thread_id < v2->thread_id) {
      return TRUE;
    }
    else if(v1->thread_id > v2->thread_id) {
      return FALSE;
    }
  }
  if (v1->seqno && v2->seqno) {
    if(v1->seqno < v2->seqno) {
      return TRUE;
    }
    else if(v1->seqno > v2->seqno) {
      return FALSE;
    }
  }
  return FALSE;
}


static bool cmp_rev_show_logts( RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) * v1, 
                            RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) * v2)
{

  if (v1->tv_sec && v2->tv_sec) {
    if(v1->tv_sec > v2->tv_sec) {
     return TRUE;
    }else if(v1->tv_sec <  v2->tv_sec) {
     return FALSE;
    }
  }
  if (v1->tv_usec && v2->tv_usec) {
    if(v1->tv_usec > v2->tv_usec) {
      return TRUE;
    }
    else if (v1->tv_usec < v2->tv_usec) {
     return FALSE;
    }
  }
  if (v1->hostname[0] && v2->hostname[0] ) {
    int res = strncmp(v1->hostname,v2->hostname,sizeof(v1->hostname));
    if(res > 0) {
      return TRUE;
    }
    else if(res < 0) {
      return FALSE;
    }
  }
  if (v1->process_id && v2->process_id) {
    if(v1->process_id > v2->process_id) {
      return TRUE;
    }
    else if(v1->process_id < v2->process_id) {
      return FALSE;
    }
  }
  if (v1->thread_id && v2->thread_id) {
    if(v1->thread_id > v2->thread_id) {
      return TRUE;
    }
    else if(v1->thread_id < v2->thread_id) {
      return FALSE;
    }
  }
  if (v1->seqno && v2->seqno) {
    if(v1->seqno > v2->seqno) {
      return TRUE;
    }
    else if(v1->seqno < v2->seqno) {
      return FALSE;
    }
  }
  return FALSE;
}

static rw_status_t rwlogd_merge_and_get_logs(rwdts_xact_t *xact,
                                             RWPB_T_MSG(RwlogMgmt_output_ShowLogs) **output_logs,
                                             ProtobufCMessage *input,
                                             struct timeval *start_t)
{
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *)input;
  struct timeval tv;
  uint32_t logs_muted = 0;
  int j;
  rwdts_query_result_t* qresult;
  struct timeval  end_t;
  int count = log_input->has_count?(int)log_input->count:CLI_DEFAULT_LOG_LINES;
  bool (*comp_f) (RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs)*, RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs)*);  
  comp_f = log_input->has_tail?cmp_rev_show_logts:cmp_show_logts;
  ring_buffer <RwlogMgmt__YangOutput__RwlogMgmt__ShowLogs__Logs *> log_ring(count,comp_f);
  RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *log;
  RWPB_T_MSG(RwlogMgmt_output_ShowLogs_LogSummary) *log_summary = 
      (RWPB_T_MSG(RwlogMgmt_output_ShowLogs_LogSummary) *)RW_MALLOC(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs_LogSummary)));
  RWPB_F_MSG_INIT(RwlogMgmt_output_ShowLogs_LogSummary,log_summary);

  while ((qresult = rwdts_xact_query_result(xact,0))) {
    log = ( RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *) qresult->message;
    if(log->log_summary && log->log_summary[0]) {
      logs_muted += log->log_summary[0]->logs_muted;
    }

    for(j=0;j<(int)log->n_logs;j++) {
      log_ring.reverse_add(log->logs[j]);
    }
  }

  RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) *log_msg;

  int i, k;
  log = (RWPB_T_MSG(RwlogMgmt_output_ShowLogs) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs)));
  RWPB_F_MSG_INIT(RwlogMgmt_output_ShowLogs,log);


  log->n_logs = MIN(log_ring.get_last_position()+1,count);
  log->logs = (RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) **)(RW_MALLOC0(log->n_logs * sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) *)));

  i = log_input->has_tail?log->n_logs-1:0; 

  for(k=0; k< (int)log->n_logs; k++,i+=log_input->has_tail?-1:1)  {
    log_msg  = (RWPB_T_MSG(RwlogMgmt_output_ShowLogs_Logs) *)(log_ring.get(i));
     log->logs[k] = log_msg;
  }

  if(!log->n_logs &&  log_input->start_time) {
    log_summary->trailing_timestamp = RW_STRDUP(log_input->start_time);
  }
  else {
    char tmstr[128];
    tmstr[0]='\0';
    if(log->n_logs) {
      log_msg = log->logs[log->n_logs-1];
      tv.tv_sec =  log_msg->tv_sec;
      tv.tv_usec = log_msg->tv_usec+1;
      if (tv.tv_usec > 1000000) {
        tv.tv_usec = 0;
        tv.tv_sec++;
      }
    }
    else {
      gettimeofday(&tv, NULL);
    }

    struct tm* tm = gmtime(&tv.tv_sec);
    int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
    int bytes = snprintf(tmstr+tmstroff, sizeof(tmstr)-tmstroff, ".%06luZ", tv.tv_usec);
    RW_ASSERT(bytes < (int)sizeof(tmstr) && bytes != -1);
    log_summary->trailing_timestamp = RW_STRDUP(tmstr);
  }

  gettimeofday(&end_t, NULL);

  log_summary->log_count = log->n_logs;
  log_summary->has_log_count = 1;
  log_summary->time_spent = ((end_t.tv_sec - start_t->tv_sec) * 1000) + 
      ((end_t.tv_usec - start_t->tv_usec)/1000);
  log_summary->has_time_spent =TRUE;
  log_summary->logs_muted = logs_muted;
  log_summary->has_logs_muted = 1;
  if (log->n_logs) {
    log_summary->has_tv_sec = 1;
    log_summary->tv_sec = log->logs[log->n_logs-1]->tv_sec;
    log_summary->has_tv_usec = 1;
    log_summary->tv_usec = log->logs[log->n_logs-1]->tv_usec;
    log_summary->has_process_id = 1;
    log_summary->process_id = log->logs[log->n_logs-1]->process_id;
    log_summary->has_thread_id = 1;
    log_summary->thread_id = log->logs[log->n_logs-1]->thread_id;
    log_summary->has_seqno = 1;
    log_summary->seqno = log->logs[log->n_logs-1]->seqno;
    log_summary->has_hostname = 1;
    strncpy(log_summary->hostname,log->logs[log->n_logs-1]->hostname,
            sizeof(log_summary->hostname)-1);
  }
  else {
    RW_ASSERT(gethostname(log_summary->hostname, MAX_HOSTNAME_SZ-1) == 0);
  }
  RW_ASSERT(gethostname(log_summary->hostname, MAX_HOSTNAME_SZ-1) == 0);

  log->log_summary = (RWPB_T_MSG(RwlogMgmt_output_ShowLogs_LogSummary) **)RW_MALLOC(sizeof(RWPB_T_MSG(RwlogMgmt_output_ShowLogs_LogSummary) *));

  log->n_log_summary = 1;
  log->log_summary[0] = log_summary;

  *output_logs = log;

  return RW_STATUS_SUCCESS;
}

}

