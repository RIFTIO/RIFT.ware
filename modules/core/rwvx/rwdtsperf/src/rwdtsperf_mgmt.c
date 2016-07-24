
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#include "rwdtsperf.h"
#include "rwdtsperf_dts_mgmt.h"
#include "rwdts_api_gi.h"

static void
rwdtsperf_start_xact_detail (rwdtsperf_instance_ptr_t instance,
                             const rwdts_xact_info_t*  parent_xact,
                             int xact_count,
                             rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb,
                             xact_detail_ptr_t xact_detail);
static void 
rwdtsperf_continue_rsp(rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb);

rwsched_CFRunLoopTimerRef
rwdtsperf_start_timer(rwtasklet_info_ptr_t            rwtasklet_info,
                      CFTimeInterval                  timer_interval,
                      rwsched_CFRunLoopTimerCallBack  cbfunc,
                      void                           *cb_arg,
                      int32_t                         periodic)
{
  rwsched_CFRunLoopTimerContext  cf_context = {0, NULL, NULL, NULL, NULL};
  rwsched_CFRunLoopRef           runloop; 
  rwsched_CFRunLoopTimerRef      cftimer;
  
  
  RW_ASSERT(rwtasklet_info);
  
  
  runloop = rwsched_tasklet_CFRunLoopGetCurrent(rwtasklet_info->rwsched_tasklet_info);
  cf_context.info = cb_arg;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(rwtasklet_info->rwsched_tasklet_info,
                                                 kCFAllocatorDefault,
                                                 CFAbsoluteTimeGetCurrent() + (timer_interval/1000.0),
                                                 (periodic == 0? 0: timer_interval),
                                                 0,
                                                 0,
                                                 cbfunc,
                                                 &cf_context);

  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);

  rwsched_tasklet_CFRunLoopAddTimer(rwtasklet_info->rwsched_tasklet_info,
                                    runloop, 
                                    cftimer, 
                                    rwtasklet_info->rwsched_instance->main_cfrunloop_mode);

  return cftimer;
  
}

void                    
rwdtsperf_stop_timer(rwtasklet_info_ptr_t      rwtasklet_info,
                    rwsched_CFRunLoopTimerRef tmr_ref)
{
  
  RW_CF_TYPE_VALIDATE(tmr_ref, rwsched_CFRunLoopTimerRef);
  RW_ASSERT(rwtasklet_info);
  
  rwsched_tasklet_CFRunLoopTimerRelease(rwtasklet_info->rwsched_tasklet_info,
                                tmr_ref);

  return;
}

static void 
rwdtsperf_prepare_msg_content(RWPB_T_MSG(RwDtsperf_data_XactMsg_Content) *content,
                      int payload_len,
                      char *rsp_flavor_name,
                      char *payload_pattern)
{
  RWPB_F_MSG_INIT(RwDtsperf_data_XactMsg_Content, content);

  content->xact_detail = RW_STRDUP(rsp_flavor_name);
  content->input_payload = RW_MALLOC0(payload_len);
  char *fill_in = content->input_payload;
  int i = 0;
  while (i < payload_len) {
    int len = snprintf (fill_in, payload_len - i, payload_pattern);
    if (len < strlen(payload_pattern)) {
      break;
    }
    fill_in += len;
    i += len;
  }
}

static void rwdtsperf_free_msg_content(RWPB_T_MSG(RwDtsperf_data_XactMsg_Content) *content)
{
  RW_FREE (content->xact_detail);
  RW_FREE (content->input_payload);
}

static __inline__ xact_detail_ptr_t
rwdtsperf_get_next_xact_detail(xact_config_ptr_t xact_cfg)
{
  xact_detail_ptr_t xact_detail;

  if (!xact_cfg->curr_detail) {
    xact_detail = RW_SKLIST_HEAD(&xact_cfg->xact_detail_list, xact_detail_t);
  }
  else {
    xact_detail = RW_SKLIST_NEXT(xact_cfg->curr_detail, xact_detail_t, xact_detail_slist_element);
  }
  while (xact_detail && !xact_detail->xact_rsp_flavor) {
    xact_detail = RW_SKLIST_NEXT(xact_detail, xact_detail_t, xact_detail_slist_element);
  }
  xact_cfg->curr_detail = xact_detail;
  return xact_cfg->curr_detail;
}

#define RWDTSPERF_SET_TIME(_inst, _prefix, _time) { \
  _inst->_prefix##tv_sec = _time.tv_sec;        \
  _inst->_prefix##tv_usec = _time.tv_usec;      \
}

#define RWDTSPERF_SET_CURR_TIME(_inst, _prefix) { \
  struct timeval time;                            \
  gettimeofday(&time, NULL);                      \
  RWDTSPERF_SET_TIME(_inst, _prefix, time);       \
}

#define RWDTSPERF_CLR_TIME(_inst, _prefix) { \
  struct timeval time = {};                  \
  RWDTSPERF_SET_TIME(_inst, _prefix, time);  \
}

static void
rwdtsperf_delay_between_xact_timer(rwsched_CFRunLoopTimerRef timer,  void *info);
static void
rwdtsperf_start_next_detail(rwdtsperf_instance_ptr_t instance, int pending_count)
{
  xact_config_ptr_t xact_cfg = &(instance->config.xact_cfg);
  RWDTSPERF_SET_CURR_TIME(instance, end_);
  switch (xact_cfg->ordering) {
    case RWDTSPERF_SEQUENTIAL: {
      if (xact_cfg->curr_detail) {
        if (pending_count) {
          //sequental would allow only one xact
        }
        else if (!rwdtsperf_get_next_xact_detail (xact_cfg) ||
            xact_cfg->running == false) {
          // completed all transactions
          xact_cfg->running = false;
          fprintf (stderr, "All transactions complete SEQUENTIAL run\n");
        }
        else {
          rwdtsperf_start_xact_detail(instance, NULL, 
                                      xact_cfg->curr_detail->xact_repeat_count, 
                                      NULL, xact_cfg->curr_detail);
        }
      }
    }
    break;
    case RWDTSPERF_PERIODIC_INVOKE: {
      if (!xact_cfg->delay_between_xact_timer &&
          xact_cfg->curr_detail) {
        RW_ASSERT(xact_cfg->delay_between_xacts);
        xact_cfg->delay_between_xact_timer = rwdtsperf_start_timer (instance->rwtasklet_info,
                                                                    xact_cfg->delay_between_xacts,
                                                                    rwdtsperf_delay_between_xact_timer,
                                                                    instance,
                                                                    1);
      }
      else {
        if (xact_cfg->curr_detail) {
          if (!rwdtsperf_get_next_xact_detail (xact_cfg) ||
              xact_cfg->running == false) {
            rwdtsperf_stop_timer(instance->rwtasklet_info, xact_cfg->delay_between_xact_timer);
            xact_cfg->delay_between_xact_timer = NULL;
            // completed periodic invoke of transactions
            xact_cfg->running = false;
            fprintf (stderr, "All transactions complete Periodic run\n");
          }
          else {
            rwdtsperf_start_xact_detail(instance, NULL, 
                                        xact_cfg->curr_detail->xact_repeat_count, 
                                        NULL, xact_cfg->curr_detail);
          }
        }
        else if (xact_cfg->delay_between_xact_timer) {
          rwdtsperf_stop_timer(instance->rwtasklet_info, xact_cfg->delay_between_xact_timer);
          xact_cfg->delay_between_xact_timer = NULL;
          // completed periodic invoke of transactions
          xact_cfg->running = false;
          fprintf (stderr, "All transactions complete Periodic run\n");
        }
      }
    }
    break;
    case RWDTSPERF_OUTSTANDING: {
      while ((instance->statistics.curr_outstanding < xact_cfg->num_xact_outstanding) &&
             (instance->statistics.curr_xact_count < xact_cfg->xact_max_with_outstanding)) {
        if (!rwdtsperf_get_next_xact_detail (xact_cfg)) {
          rwdtsperf_get_next_xact_detail (xact_cfg);
        }
        if (xact_cfg->curr_detail) {
          //curr_outstanding gets incremented inside start_xact_detail
          rwdtsperf_start_xact_detail(instance, NULL, 
                                      xact_cfg->curr_detail->xact_repeat_count, 
                                      NULL, xact_cfg->curr_detail);
        }
        else {
          fprintf (stderr, "Fail continous run\n");
          xact_cfg->curr_xact_count = xact_cfg->xact_max_with_outstanding;
          break;
        }
      }
      if (xact_cfg->curr_xact_count >= xact_cfg->xact_max_with_outstanding) {
          fprintf (stderr, "All transactions complete outstanding run\n");
          xact_cfg->running = false;
      }
    }
    break;
  }
}

static void
rwdtsperf_xact_delay_interval_timer(rwsched_CFRunLoopTimerRef timer,  void *info)
{
  rwdtsperf_xact_detail_cb_ptr_t xact_detail_cb = (rwdtsperf_xact_detail_cb_ptr_t)info;
  const rwdts_xact_info_t*  parent_xact = xact_detail_cb->parent_xact;
  xact_detail_ptr_t xact_detail = xact_detail_cb->xact_detail;
  rwdtsperf_instance_ptr_t instance = xact_detail_cb->instance;
  int xact_count = xact_detail_cb->xact_count;
  rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb = xact_detail_cb->rsp_flavor_cb;
  
  xact_detail_cb->xact_delay_interval_timer = NULL;
  RW_FREE_TYPE(xact_detail_cb, rwdtsperf_xact_detail_cb_t);

  if (xact_count) {
    rwdtsperf_start_xact_detail(instance, parent_xact, 
                                xact_count, rsp_flavor_cb, xact_detail);
  }
  else if (rsp_flavor_cb) {
    rwdtsperf_continue_rsp(rsp_flavor_cb);
  }
  else {
    rwdtsperf_start_next_detail(instance, xact_count);
  }
}

static __inline__ void rwdtsperf_update_averages(xact_detail_ptr_t xact_detail,
                                                 unsigned long xact_duration)
{
  if (xact_detail->xact_repeat_count) {
    rwdtsperf_average_ptr_t ave = &xact_detail->curr_average_xact_time;
    if (ave->count) {
      ave->average = 
          ((ave->average * ave->count) + xact_duration) /
          (ave->count + 1);
    }
    else {
      ave->average = xact_duration;
      ave->min = xact_duration;
      ave->max = xact_duration;
      ave->variance = 0;
    }
    if (ave->min > xact_duration) {
      ave->min = xact_duration;
    }
    if (ave->max < xact_duration) {
      ave->max = xact_duration;
    }
    ave->count++;
  }
}

static void rwdtsperf_start_xact_detail_cb(rwdts_xact_t * xact,
                                           rwdts_xact_status_t* xact_status,
                                           void * ud)
{ 
  rwdtsperf_xact_detail_cb_ptr_t xact_detail_cb = (rwdtsperf_xact_detail_cb_ptr_t)ud;
  rwdtsperf_instance_ptr_t instance = xact_detail_cb->instance;
  const rwdts_xact_info_t*  parent_xact = xact_detail_cb->parent_xact;
  xact_detail_ptr_t xact_detail = xact_detail_cb->xact_detail;
  int xact_count = xact_detail_cb->xact_count;
  rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb = xact_detail_cb->rsp_flavor_cb;

  MSG_PRN ("%d:%d in callback\n", xact_status->xact_done, xact_status->responses_done);
      
  RWDTSPERF_UPDATE_STAT(tot_rsps);
  if (xact_status->xact_done) {
    switch (xact_status->status) {
      case RWDTS_XACT_ABORTED:
        RWDTSPERF_UPDATE_STAT(abrt_count);
        break;
      case RWDTS_XACT_FAILURE: 
        RWDTSPERF_UPDATE_STAT(fail_count);
        break; 
      case RWDTS_XACT_COMMITTED: 
        RWDTSPERF_UPDATE_STAT(succ_count);
        break; 
      default: 
        break;
    } 
    unsigned long xact_duration = RWDTSPERF_UPDATE_RTT_BUCKET(xact_detail_cb->start_time);
    RW_FREE_TYPE(xact_detail_cb, rwdtsperf_xact_detail_cb_t);

    rwdtsperf_update_averages(xact_detail, xact_duration);

    if (!rsp_flavor_cb) {
      instance->statistics.curr_outstanding--;
      RW_ASSERT (instance->statistics.curr_outstanding >= 0);
    }
    if (xact_count && !xact_detail->xact_delay_interval) {
      rwdtsperf_start_xact_detail (instance, parent_xact,
                                   xact_count, rsp_flavor_cb, xact_detail);
    }
    else if (rsp_flavor_cb) {
      rwdtsperf_continue_rsp(rsp_flavor_cb);
    }
    else {
      rwdtsperf_start_next_detail(instance, xact_count);
    }
  } 
}

static void
rwdtsperf_start_xact_detail (rwdtsperf_instance_ptr_t instance,
                             const rwdts_xact_info_t*  parent_xact,
                             int xact_count,
                             rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb,
                             xact_detail_ptr_t xact_detail)
{
  rwdtsperf_xact_detail_cb_ptr_t xact_detail_cb = RW_MALLOC0_TYPE(sizeof(*xact_detail_cb), 
                                                                  rwdtsperf_xact_detail_cb_t);
  RW_ASSERT(xact_detail_cb);
  xact_detail_cb->instance = instance;
  xact_detail_cb->xact_detail = xact_detail;
  xact_detail_cb->parent_xact = parent_xact;;
  xact_detail_cb->rsp_flavor_cb = rsp_flavor_cb;
  xact_detail_cb->xact_count = (xact_count?(xact_count - 1):0);
  if (xact_detail->xact_repeat_count && xact_detail->xact_repeat_count == xact_count) {
    xact_detail->curr_average_xact_time.average = 0;
    xact_detail->curr_average_xact_time.min = 0;
    xact_detail->curr_average_xact_time.max = 0;
    xact_detail->curr_average_xact_time.variance = 0;
    xact_detail->curr_average_xact_time.count = 0;
  }

  gettimeofday (&xact_detail_cb->start_time, NULL);


  RWDTSPERF_UPDATE_STAT(tot_xacts);
  if (rsp_flavor_cb) {
    if (parent_xact) {
      RWDTSPERF_UPDATE_STAT(tot_xacts_from_rsp);
    }
    else {
      RWDTSPERF_UPDATE_STAT(tot_subq_from_rsp);
    }
  }
  else {
    instance->statistics.curr_outstanding++;
    instance->statistics.curr_xact_count++;
  }
  rwdtsperf_prepare_msg_content(&xact_detail->content,
                                xact_detail->payload_len,
                                xact_detail->xact_rsp_flavor,
                                xact_detail->payload_pattern);

  RWDTSPERF_PREPARE_KEYSPEC(xact_detail->xact_rsp_flavor);

  xact_detail->corr_id++;
  rwdts_xact_t *creat_xact = rwdts_api_query_ks (instance->dts_h,
        (rw_keyspec_path_t *) &pathspec,
        xact_detail->action,
        xact_detail->flags,
        rwdtsperf_start_xact_detail_cb,
        xact_detail_cb,
        &(xact_detail->content.base));

  RW_ASSERT(creat_xact);
  //rwdts_xact_trace (creat_xact);
    

  RWDTSPERF_FREE_KEYSPEC;

  rwdtsperf_free_msg_content(&xact_detail->content);
  if (xact_count) {
    xact_count--;
    if (xact_detail->xact_delay_interval && xact_count) {
      rwdtsperf_xact_detail_cb_ptr_t xact_detail_timer_cb = RW_MALLOC0_TYPE(sizeof(*xact_detail_cb), 
                                                                  rwdtsperf_xact_detail_cb_t);
      RW_ASSERT(xact_detail_timer_cb);
      xact_detail_timer_cb->instance = instance;
      xact_detail_timer_cb->xact_detail = xact_detail;
      xact_detail_timer_cb->parent_xact = parent_xact;
      xact_detail_timer_cb->xact_count = xact_count;
      xact_detail_timer_cb->rsp_flavor_cb = rsp_flavor_cb;
      xact_detail_timer_cb->xact_delay_interval_timer = rwdtsperf_start_timer (instance->rwtasklet_info,
                                                                               xact_detail->xact_delay_interval,
                                                                               rwdtsperf_xact_delay_interval_timer,
                                                                               xact_detail_timer_cb,
                                                                               0);
    }
  }
}

static void
rwdtsperf_delay_between_xact_timer(rwsched_CFRunLoopTimerRef timer,  void *info)
{
  rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t) info;
  rwdtsperf_start_next_detail (instance, 0);
}

static void
rwdtsperf_send_rsp(rwdtsperf_instance_ptr_t      instance,
                   rsp_flavor_ptr_t              rsp_flavor,
                   const rwdts_xact_info_t       *xact_info,
                   bool                          done)
{
  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp = {};
  RWDtsEventRsp rsp_type = (rsp_flavor->rsp_type == RWDTSPERF_NACK)? RWDTS_EVTRSP_NACK:
      ((rsp_flavor->rsp_type == RWDTSPERF_NA)? RWDTS_EVTRSP_NA: RWDTS_EVTRSP_ACK);

  rwdtsperf_prepare_msg_content(&rsp_flavor->content,
                                rsp_flavor->payload_len,
                                rsp_flavor->rsp_flavor_name,
                                rsp_flavor->payload_pattern);
  rsp.msgs = msgs;
  rsp.msgs[0] = &rsp_flavor->content.base;
  rsp.n_msgs = 1;
  rsp.evtrsp = rsp_type;
  if (!done) {
    rsp.evtrsp = RWDTS_EVTRSP_ASYNC;
    rsp.more = true;
  }
  MSG_PRN ("%s is responding with %s:%s \n", rsp_flavor->rsp_flavor_name,
           rsp_flavor->content.input_payload,
           rsp_flavor->content.xact_detail);

  instance->statistics.send_rsps++;
  RWDTSPERF_PREPARE_KEYSPEC(rsp_flavor->rsp_flavor_name);
  rsp.ks = (rw_keyspec_path_t*)&pathspec;

  rw_status_t status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  rwdtsperf_free_msg_content(&rsp_flavor->content);
  RWDTSPERF_FREE_KEYSPEC;
}

static void
rwdtsperf_rsp_delay_interval_timer(rwsched_CFRunLoopTimerRef timer,  void *info)
{
  rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb = (rwdtsperf_rsp_flavor_cb_ptr_t)info;
  const rwdts_xact_info_t       *xact_info = rsp_flavor_cb->xact_info;
  rsp_flavor_ptr_t              rsp_flavor = rsp_flavor_cb->rsp_flavor;
  rwdtsperf_instance_ptr_t      instance = rsp_flavor_cb->instance;
  rwsched_CFRunLoopTimerRef     rsp_delay_interval_timer =
      rsp_flavor_cb->rsp_delay_interval_timer;
  
  MSG_PRN("%s timeout %d\n", rsp_flavor->rsp_flavor_name, rsp_flavor_cb->rsp_count);
  if ((rsp_flavor->rsp_character == RWDTSPERF_ASYNC_AND_RSP) ||
      (!rsp_flavor_cb->rsp_count)) {
    RW_FREE_TYPE(rsp_flavor_cb, rwdtsperf_rsp_flavor_cb_t);
    rwdtsperf_stop_timer(instance->rwtasklet_info, rsp_delay_interval_timer);

    rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, true);
  }
  else {
    rsp_flavor_cb->rsp_count--;
    rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, false);
  }
}

static void 
rwdtsperf_continue_rsp(rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb)
{
  rsp_flavor_ptr_t rsp_flavor = rsp_flavor_cb->rsp_flavor;
  rwdtsperf_instance_ptr_t instance = rsp_flavor_cb->instance;
  const rwdts_xact_info_t* xact_info = rsp_flavor_cb->xact_info;

  RW_FREE_TYPE (rsp_flavor_cb, rwdtsperf_rsp_flavor_cb_t);

  if (rsp_flavor->num_rsp) {
    if (rsp_flavor->rsp_delay_interval) {
      rsp_flavor_cb = RW_MALLOC0_TYPE(sizeof(*rsp_flavor_cb), rwdtsperf_rsp_flavor_cb_t);
      rsp_flavor_cb->rsp_count = rsp_flavor->num_rsp;
      rsp_flavor_cb->instance = instance;
      rsp_flavor_cb->rsp_flavor = rsp_flavor;
      rsp_flavor_cb->xact_info = xact_info;
      rsp_flavor_cb->rsp_delay_interval_timer = rwdtsperf_start_timer (instance->rwtasklet_info,
                                                                       rsp_flavor->rsp_delay_interval,
                                                                       rwdtsperf_rsp_delay_interval_timer,
                                                                       rsp_flavor_cb,
                                                                       1);
      return;
    }
    else {
      int index = rsp_flavor->num_rsp;
      for (; index > 1; index--) {
        rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, false);
      }
    }
  }
  rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, true);
}

rwdts_member_rsp_code_t 
rwdtsperf_handle_xact(const rwdts_xact_info_t* xact_info,
                      RWDtsQueryAction         action,
                      const rw_keyspec_path_t*      key,
                      const ProtobufCMessage*  msg,
                      uint32_t credits,
                      void *getnext_ptr)
{
  RWPB_T_MSG(RwDtsperf_data_XactMsg_Content) *input = (RWPB_T_MSG(RwDtsperf_XactMsg_Content) *)msg;
  RW_ASSERT (RWPB_G_MSG_PBCMD(RwDtsperf_data_XactMsg_Content) == input->base.descriptor);
  rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t) xact_info->ud;
  subscriber_config_ptr_t subsc_cfg = &(instance->config.subsc_cfg);
  rsp_flavor_ptr_t rsp_flavor = NULL;
  RWPB_T_PATHSPEC(RwDtsperf_data_XactMsg_Content) pathspec =
      *((RWPB_T_PATHSPEC(RwDtsperf_data_XactMsg_Content)*) key);
  rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb;

  MSG_PRN ("%s handle_xact called", pathspec.dompath.path001.key00.xact_detail);
  if (pathspec.dompath.path001.key00.xact_detail) {
    RW_SKLIST_LOOKUP_BY_KEY (&(subsc_cfg->rsp_flavor_list),
                             pathspec.dompath.path001.key00.xact_detail,
                             &rsp_flavor);
  }

  if (rsp_flavor) {
    xact_detail_ptr_t xact_detail;
    switch (rsp_flavor->rsp_character) {
     case RWDTSPERF_INVOKE_XACT_RSP: 
       {
         if (rsp_flavor->next_xact_name[0]) {
           RW_SKLIST_LOOKUP_BY_KEY (&(instance->config.xact_cfg.xact_detail_list),
                                    rsp_flavor->next_xact_name,
                                    &xact_detail);
           if (xact_detail) {
             rsp_flavor_cb = RW_MALLOC0_TYPE(sizeof(*rsp_flavor_cb), rwdtsperf_rsp_flavor_cb_t);
             rsp_flavor_cb->rsp_count = rsp_flavor->num_rsp;
             rsp_flavor_cb->instance = instance;
             rsp_flavor_cb->rsp_flavor = rsp_flavor;
             rsp_flavor_cb->xact_info = xact_info;
             const rwdts_xact_info_t* parent_xact = (rsp_flavor->rsp_invoke_xact == RWDTSPERF_SUB_XACT)?
                 xact_info: NULL;
             rwdtsperf_start_xact_detail (instance, parent_xact, 
                                          xact_detail->xact_repeat_count, 
                                          rsp_flavor_cb, xact_detail);
             return RWDTS_ACTION_ASYNC;
           }
         }
       }
       break;
     case RWDTSPERF_DELAY_RSP:
     case RWDTSPERF_ASYNC_AND_RSP:
       {
         if (rsp_flavor->rsp_delay_interval) {
           rsp_flavor_cb = RW_MALLOC0_TYPE(sizeof(*rsp_flavor_cb), rwdtsperf_rsp_flavor_cb_t);
           rsp_flavor_cb->rsp_count = rsp_flavor->num_rsp;

start_timer_with_async:
           rsp_flavor_cb->instance = instance;
           rsp_flavor_cb->rsp_flavor = rsp_flavor;
           rsp_flavor_cb->xact_info = xact_info;
           rsp_flavor_cb->rsp_delay_interval_timer = rwdtsperf_start_timer (instance->rwtasklet_info,
                                                                            rsp_flavor->rsp_delay_interval,
                                                                            rwdtsperf_rsp_delay_interval_timer,
                                                                            rsp_flavor_cb,
                                                                            1);
           return RWDTS_ACTION_ASYNC;
         }
       }
       break;
    default:
       break;
    }
    if (rsp_flavor->num_rsp) {
      if (rsp_flavor->rsp_delay_interval) {
        rsp_flavor_cb = RW_MALLOC0_TYPE(sizeof(*rsp_flavor_cb), rwdtsperf_rsp_flavor_cb_t);
        rsp_flavor_cb->rsp_count = rsp_flavor->num_rsp;
        rsp_flavor_cb->rsp_count--;
        rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, false);

        if (rsp_flavor_cb->rsp_count) {
          goto start_timer_with_async;
        }
        else {
          RW_FREE_TYPE (rsp_flavor_cb, rwdtsperf_rsp_flavor_cb_t);
        }
      }
      else {
        int index = rsp_flavor->num_rsp;
        for (; index > 1; index--) {
          rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, false);
        }
      }
    }
    rwdtsperf_send_rsp (instance, rsp_flavor, xact_info, true);
  }
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t 
rwdtsperf_handle_start_xact(const rwdts_xact_info_t* xact_info,
                            RWDtsQueryAction         action,
                            const rw_keyspec_path_t*      key,
                            const ProtobufCMessage*  msg,
                            uint32_t credits,
                            void *getnext_ptr)
{
  RW_ASSERT(xact_info);

  RWPB_T_MSG(RwDtsperf_input_StartXact)
      *input = (RWPB_T_MSG(RwDtsperf_input_StartXact) *)msg;
  RW_ASSERT (RWPB_G_MSG_PBCMD(RwDtsperf_input_StartXact) == input->base.descriptor);
  rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t) xact_info->ud;
  xact_config_ptr_t xact_cfg = &(instance->config.xact_cfg);
  xact_detail_ptr_t xact_detail;

  if (input->has_instance_id &&
      input->instance_id == instance->rwtasklet_info->identity.rwtasklet_instance_id) {
    if (xact_cfg->running) {
      return RWDTS_ACTION_OK;
    }
    xact_detail = RW_SKLIST_HEAD(&xact_cfg->xact_detail_list, xact_detail_t);
    RW_ASSERT(xact_detail);
    xact_cfg->curr_detail = xact_detail;
    xact_cfg->running = true;
    RWDTSPERF_SET_CURR_TIME(instance, start_);
    RWDTSPERF_CLR_TIME(instance, end_);
    instance->statistics.curr_xact_count = 0;
    instance->statistics.curr_outstanding = 0;
    RWTRACE_ERROR(instance->rwtasklet_info->rwtrace_instance,
                  RWTRACE_CATEGORY_RWTASKLET, 
                  "[[ %s ]] Tasklet received START\n", rwdts_api_client_path(instance->apih));
    rwdtsperf_start_xact_detail(instance, NULL /*xact_info*/, 
                                xact_cfg->curr_detail->xact_repeat_count, 
                                NULL, xact_cfg->curr_detail);
    rwdtsperf_start_next_detail(instance, xact_cfg->curr_detail->xact_repeat_count);
    return RWDTS_ACTION_OK;
  }
  return RWDTS_ACTION_OK;
}

#define RWDTSPERF_FILL_MSG(out, in, field) {\
  out->field = in->field;                   \
  out->has_##field = 1;                     \
}

static rwdts_member_rsp_code_t
rwdtsperf_handle_perf_statistics(const rwdts_xact_info_t* xact_info,
                                 RWDtsQueryAction         action,
                                 const rw_keyspec_path_t*      key,
                                 const ProtobufCMessage*  msg,
                                 uint32_t credits,
                                 void *getnext_ptr)
{
  RW_ASSERT(xact_info);

  RWPB_T_MSG(RwDtsperf_data_PerfStatistics) *input =
      (RWPB_T_MSG(RwDtsperf_data_PerfStatistics) *)msg;
  RW_ASSERT (RWPB_G_MSG_PBCMD(RwDtsperf_data_PerfStatistics) == input->base.descriptor);

  rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t) xact_info->ud;

  RWPB_T_PATHSPEC(RwDtsperf_data_PerfStatistics) pathspec =
      *((RWPB_T_PATHSPEC(RwDtsperf_data_PerfStatistics)*) key);
  if (pathspec.dompath.path000.has_key00 &&
      (pathspec.dompath.path000.key00.instance_id != 
       instance->rwtasklet_info->identity.rwtasklet_instance_id)) {
    return RWDTS_ACTION_NA;
  }
  pathspec.dompath.path000.has_key00 = 1;
  pathspec.dompath.path000.key00.instance_id = 
      instance->rwtasklet_info->identity.rwtasklet_instance_id;
 
  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp = {};
  rsp.msgs = msgs;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;
  rsp.ks = (rw_keyspec_path_t*)&pathspec;

  RWPB_T_MSG(RwDtsperf_data_PerfStatistics) *stats_p, stats_s;

  xact_stats_yang_ptr_t stats = ((xact_stats_yang_ptr_t ) &instance->statistics);
  //RW_ASSERT (sizeof(xact_stats_yang_t) == sizeof(xact_stats_t));

  stats_p = &stats_s;
  RWPB_F_MSG_INIT(RwDtsperf_data_PerfStatistics, stats_p);

  stats_p->instance_id = instance->rwtasklet_info->identity.rwtasklet_instance_id;
  RWDTSPERF_FILL_MSG (stats_p, instance, start_tv_sec);
  RWDTSPERF_FILL_MSG (stats_p, instance, start_tv_usec);
  RWDTSPERF_FILL_MSG (stats_p, instance, end_tv_sec);
  RWDTSPERF_FILL_MSG (stats_p, instance, end_tv_usec);
  RWDTSPERF_FILL_MSG (stats_p, stats, tot_xacts);
  RWDTSPERF_FILL_MSG (stats_p, stats, tot_xacts_from_rsp);
  RWDTSPERF_FILL_MSG (stats_p, stats, tot_subq_from_rsp);
  RWDTSPERF_FILL_MSG (stats_p, stats, tot_rsps);
  RWDTSPERF_FILL_MSG (stats_p, stats, send_rsps);
  RWDTSPERF_FILL_MSG (stats_p, stats, succ_count);
  RWDTSPERF_FILL_MSG (stats_p, stats, fail_count);
  RWDTSPERF_FILL_MSG (stats_p, stats, abrt_count);
  RWDTSPERF_FILL_MSG (stats_p, stats, curr_outstanding);
  RWDTSPERF_FILL_MSG (stats_p, stats, curr_xact_count);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_0);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_1);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_2);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_3);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_4);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_5);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_6);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_7);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_8);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_9);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_10);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_11);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_12);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_13);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_14);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_less_2_exp_15);
  RWDTSPERF_FILL_MSG (stats_p, stats, rtt_greater);

  xact_detail_ptr_t xact_detail;
  unsigned int xact_count = 0;
  for (xact_detail = RW_SKLIST_HEAD(&instance->config.xact_cfg.xact_detail_list, xact_detail_t);
       xact_detail;
       xact_detail = RW_SKLIST_NEXT(xact_detail, xact_detail_t, xact_detail_slist_element))
  {
    if (xact_detail->curr_average_xact_time.count) {
      xact_count++;
    }
  }
  if (xact_count) {
    stats_p->latency_averages = RW_MALLOC0(sizeof(stats_p->latency_averages) * xact_count);
    stats_p->n_latency_averages = xact_count;
    unsigned int indx = 0;
    for (xact_detail = RW_SKLIST_HEAD(&instance->config.xact_cfg.xact_detail_list, xact_detail_t);
         xact_detail;
         xact_detail = RW_SKLIST_NEXT(xact_detail, xact_detail_t, xact_detail_slist_element))
    {
      if (xact_detail->curr_average_xact_time.count) {
        stats_p->latency_averages[indx] = RW_MALLOC0(sizeof(RWPB_T_MSG(RwDtsperf_data_PerfStatistics_LatencyAverages)));
        RWPB_F_MSG_INIT(RwDtsperf_data_PerfStatistics_LatencyAverages, stats_p->latency_averages[indx]);
        stats_p->latency_averages[indx]->xact_name = RW_STRDUP(xact_detail->xact_name);
        stats_p->latency_averages[indx]->has_num_xacts = 
        stats_p->latency_averages[indx]->has_min_latency = 
        stats_p->latency_averages[indx]->has_max_latency = 
        stats_p->latency_averages[indx]->has_ave_latency = 1;
        stats_p->latency_averages[indx]->num_xacts = xact_detail->curr_average_xact_time.count;
        stats_p->latency_averages[indx]->ave_latency = xact_detail->curr_average_xact_time.average;
        stats_p->latency_averages[indx]->min_latency = xact_detail->curr_average_xact_time.min;
        stats_p->latency_averages[indx]->max_latency = xact_detail->curr_average_xact_time.max;
        stats_p->latency_averages[indx]->variance = xact_detail->curr_average_xact_time.variance;
        indx++;
      }
    }
    RW_ASSERT(indx == xact_count);
  }
  rsp.msgs[0] = &stats_s.base;
  rsp.n_msgs = 1;

  rw_status_t status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  return RWDTS_ACTION_OK;
}

static void rwdtsperf_dts_handle_state_change(rwdts_api_t*  apih, 
                                              rwdts_state_t state,
                                              void*         ud)
{
  int i;
  switch (state) { 
    case RW_DTS_STATE_INIT: {
      rwdtsperf_instance_ptr_t instance = (rwdtsperf_instance_ptr_t)ud;
      instance->apih = apih;
      RWPB_T_PATHSPEC(RwDtsperf_data_PerfStatistics) spec1 = *(RWPB_G_PATHSPEC_VALUE(RwDtsperf_data_PerfStatistics));
      RWPB_T_PATHSPEC(RwDtsperf_input_StartXact) spec2 = *(RWPB_G_PATHSPEC_VALUE(RwDtsperf_input_StartXact));
      rw_keyspec_path_set_category ((rw_keyspec_path_t *)&spec1, NULL, RW_SCHEMA_CATEGORY_DATA);

      rwdts_registration_t regns[] = {
        {.keyspec = (rw_keyspec_path_t *)&spec1,
          .desc = RWPB_G_MSG_PBCMD(RwDtsperf_data_PerfStatistics),
          .flags =  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
          .callback = {
            .cb.prepare = rwdtsperf_handle_perf_statistics,
          }
        },

        /*-------------------------------------------------*
         * * RPC                                             *
         * *-------------------------------------------------*/

        /* start-xact */
        {.keyspec = (rw_keyspec_path_t *)&spec2,
          .desc = RWPB_G_MSG_PBCMD(RwDtsperf_input_StartXact),
          .flags =  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
          .callback = {
            .cb.prepare = rwdtsperf_handle_start_xact,
          }
        },
      };

      for (i = 0; i < sizeof (regns) / sizeof (rwdts_registration_t); i++) {
        regns[i].callback.ud = instance;
        rwdts_member_register(NULL, instance->dts_h,regns[i].keyspec, &regns[i].callback,
                              regns[i].desc, regns[i].flags, NULL);
      }

      rwdtsperf_dts_mgmt_register_dtsperf_config (instance);
      rwdts_api_set_state(apih, RW_DTS_STATE_REGN_COMPLETE);
    }
    break;
    case RW_DTS_STATE_CONFIG: {
      rwdts_api_set_state(apih, RW_DTS_STATE_RUN);
    }
    break;
    case RW_DTS_STATE_RUN:
    case RW_DTS_STATE_REGN_COMPLETE:
      break;
    default:
      fprintf(stderr, "Invalid DTS state %u \n", state);
      break;
  }

  return;
}

void rwdtsperf_register_dts_callbacks(rwdtsperf_instance_ptr_t instance)
{
  rwtasklet_info_ptr_t tasklet = instance->rwtasklet_info;

  instance->dts_h = rwdts_api_new (tasklet, 
                                   (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwDtsperf),
                                   rwdtsperf_dts_handle_state_change, instance, NULL);

  RW_ASSERT(instance->dts_h);

  return;
}
