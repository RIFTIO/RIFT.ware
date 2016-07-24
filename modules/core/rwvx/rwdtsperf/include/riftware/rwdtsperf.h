
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#ifndef __RWDTSPERF_H_
#define __RWDTSPERF_H_

#include <rw_tasklet_plugin.h>
#include "rw-dtsperf.pb-c.h"
#include "rwdts.h"
#include "rwdts_api_gi.h"
#include "rwlib.h"

__BEGIN_DECLS

#if 0
#define MSG_PRN(format, args ...) \
  fprintf (stderr,"[%s:%d %s]" format "\n", __FILE__, __LINE__, __FUNCTION__, args);
#else
#define MSG_PRN(format, args ...)  {do ; while (0);}
#endif

#define RWDTSPERF_PREPARE_KEYSPEC(rsp_flavor_name)                           \
  RWPB_T_PATHSPEC(RwDtsperf_data_XactMsg_Content) pathspec =                \
      (*RWPB_G_PATHSPEC_VALUE(RwDtsperf_data_XactMsg_Content));             \
  pathspec.dompath.path001.key00.xact_detail = RW_STRDUP (rsp_flavor_name); \
  pathspec.dompath.path001.has_key00 = 1;\
  rw_keyspec_path_set_category ((rw_keyspec_path_t *)&pathspec, NULL, RW_SCHEMA_CATEGORY_DATA);

#define RWDTSPERF_FREE_KEYSPEC {\
  RW_FREE(pathspec.dompath.path001.key00.xact_detail);\
}

#define IS_PRESENT_SET(out, in, field) \
  if (in->has_##field) { \
    out->field = in->field; \
  }

#define IS_PRESENT_SET_ENUM(out, in, field) \
  if (in->has_##field) { \
    out->field = in->field+1; \
  }

#define RWDTSPERF_UPDATE_STAT(stat) \
  xact_detail->statistics.stat++; \
  instance->statistics.stat++;

#define RWDTSPERF_RTT_BUCKET(t_duration) ({ \
 int index = 0;                           \
 while ((t_duration) > (0x1 << index)) {      \
   index++;                               \
   if (index > 15) {                      \
     break;                               \
   }                                      \
 }                                        \
 index;                                   \
})

#define RWDTSPERF_DUR(curr_time,start_time) \
  ((curr_time.tv_sec - start_time.tv_sec) * 1000000 +  \
    (curr_time.tv_usec - start_time.tv_usec))

  
#define RWDTSPERF_UPDATE_RTT_BUCKET(start_time) ({ \
  struct timeval curr_time; \
  gettimeofday (&curr_time, NULL); \
  unsigned long duration = RWDTSPERF_DUR(curr_time, start_time); \
  int rtt_indx = RWDTSPERF_RTT_BUCKET(duration/1000); \
  RW_ASSERT(rtt_indx < 17); \
  RWDTSPERF_UPDATE_STAT(rtt_buckets[rtt_indx]); \
  duration; \
})

typedef enum resp_character {
  RWDTSPERF_UNDEF_RSP       = 0,
  RWDTSPERF_DELAY_RSP       = 1,
  RWDTSPERF_ASYNC_AND_RSP   = 2,
  RWDTSPERF_INVOKE_XACT_RSP = 3,
} resp_character_e;

typedef enum resp_type {
  RWDTSPERF_UNDEF_TYPE = 0,
  RWDTSPERF_ACK        = 1,
  RWDTSPERF_NACK       = 2,
  RWDTSPERF_NA         = 3,
} resp_type_e;
  
typedef enum resp_invoke_xact {
  RWDTSPERF_UNDEF_INVO = 0,
  RWDTSPERF_SUB_XACT   = 1,
  RWDTSPERF_NEW_XACT   = 2,
} resp_invoke_xact_e;
  
struct rsp_flavor_s {
  char                rsp_flavor_name[32];
  char                next_xact_name[32];
  resp_character_e    rsp_character;
  resp_invoke_xact_e  rsp_invoke_xact;
  int                 num_rsp;
  int                 rsp_delay_interval;
  int                 payload_len;
  char                payload_pattern[128];
  resp_type_e         rsp_type;
  bool                rsp_cache;

  rw_sklist_element_t rsp_flavor_slist_element;
  RWPB_T_MSG(RwDtsperf_data_XactMsg_Content) content;
};

RW_TYPE_DECL(rsp_flavor);
RW_CF_TYPE_EXTERN(rsp_flavor_ptr_t);
struct subscriber_config_s {
  rw_sklist_t rsp_flavor_list;
  rsp_flavor_ptr_t curr_rsp_flavor;
};

RW_TYPE_DECL(subscriber_config);
RW_CF_TYPE_EXTERN(subscriber_config_ptr_t);

struct xact_stats_yang_s {
  unsigned int tot_xacts;
  unsigned int tot_xacts_from_rsp;
  unsigned int tot_subq_from_rsp;
  unsigned int tot_rsps;
  unsigned int send_rsps;

  unsigned int succ_count;
  unsigned int fail_count;
  unsigned int abrt_count;

  unsigned int curr_outstanding;
  unsigned int curr_xact_count;

  /*unsigned int rtt_buckets[17];*/
  unsigned int rtt_less_2_exp_0;
  unsigned int rtt_less_2_exp_1;
  unsigned int rtt_less_2_exp_2;
  unsigned int rtt_less_2_exp_3;
  unsigned int rtt_less_2_exp_4;
  unsigned int rtt_less_2_exp_5;
  unsigned int rtt_less_2_exp_6;
  unsigned int rtt_less_2_exp_7;
  unsigned int rtt_less_2_exp_8;
  unsigned int rtt_less_2_exp_9;
  unsigned int rtt_less_2_exp_10;
  unsigned int rtt_less_2_exp_11;
  unsigned int rtt_less_2_exp_12;
  unsigned int rtt_less_2_exp_13;
  unsigned int rtt_less_2_exp_14;
  unsigned int rtt_less_2_exp_15;
  unsigned int rtt_greater; 
};

RW_TYPE_DECL(xact_stats_yang);
RW_CF_TYPE_EXTERN(xact_stats_yang_ptr_t);

struct xact_stats_s {
  unsigned int tot_xacts;
  unsigned int tot_xacts_from_rsp;
  unsigned int tot_subq_from_rsp;
  unsigned int tot_rsps;
  unsigned int send_rsps;

  unsigned int succ_count;
  unsigned int fail_count;
  unsigned int abrt_count;

  unsigned int curr_outstanding;
  unsigned int curr_xact_count;

  unsigned int rtt_buckets[17];/*
  unsigned int rtt_less_2_exp_0;
  unsigned int rtt_less_2_exp_1;
  unsigned int rtt_less_2_exp_2;
  unsigned int rtt_less_2_exp_3;
  unsigned int rtt_less_2_exp_4;
  unsigned int rtt_less_2_exp_5;
  unsigned int rtt_less_2_exp_6;
  unsigned int rtt_less_2_exp_7;
  unsigned int rtt_less_2_exp_8;
  unsigned int rtt_less_2_exp_9;
  unsigned int rtt_less_2_exp_10;
  unsigned int rtt_less_2_exp_11;
  unsigned int rtt_less_2_exp_12;
  unsigned int rtt_less_2_exp_13;
  unsigned int rtt_less_2_exp_14;
  unsigned int rtt_less_2_exp_15;
  unsigned int rtt_greater;*/
};

RW_TYPE_DECL(xact_stats);
RW_CF_TYPE_EXTERN(xact_stats_ptr_t);

typedef enum xact_oper {
  RWDTSPERF_UNDEF_OPER = 0,
  RWDTSPERF_CREATE     = 1,
  RWDTSPERF_READ       = 2,
  RWDTSPERF_UPDATE     = 3,
} xact_oper_e;

typedef enum xact_type {
  RWDTSPERF_UNDEF_XACT  = 0,
  RWDTSPERF_TRANSACT    = 1,
  RWDTSPERF_NONTRANSACT = 2,
} xact_type_e;
  
typedef enum in_xact_repeat_mode {
  RWDTSPERF_UNDEF_REP       = 0,
  RWDTSPERF_WAIT_FINISH_REP = 1,
  RWDTSPERF_PERIODIC_REP    = 2,
} in_xact_repeat_mode_e;
  
struct rwdtsperf_average_s {
  unsigned int  count;
  unsigned long average;
  unsigned long min;
  unsigned long max;
  unsigned long variance;
};
RW_TYPE_DECL(rwdtsperf_average);
RW_CF_TYPE_EXTERN(rwdtsperf_average_ptr_t);

struct xact_detail_s {
  char                xact_name[32];
  xact_oper_e         xact_operation;
  xact_type_e         xact_type;
  int                 in_xact_repeat_count;
  int                 in_xact_repeat_delay;
  in_xact_repeat_mode_e in_xact_repeat_character;
  int                 xact_repeat_count;
  int                 xact_delay_interval;
  char                *xact_rsp_flavor;
  int                 payload_len;
  char                payload_pattern[128];
  xact_stats_t        statistics;
  rwdtsperf_average_t curr_average_xact_time;
  rw_sklist_element_t xact_detail_slist_element;
  RWPB_T_MSG(RwDtsperf_data_XactMsg_Content) content;
  RWDtsQueryAction action;
  uint64_t         corr_id;
  uint32_t         flags;
};

RW_TYPE_DECL(xact_detail);
RW_CF_TYPE_EXTERN(xact_detail_ptr_t);
typedef enum xact_order {
  RWDTSPERF_SEQUENTIAL      = 0,
  RWDTSPERF_PERIODIC_INVOKE = 1,
  RWDTSPERF_OUTSTANDING     = 2,
} xact_order_e;

struct xact_config_s {
  bool              running;
  rw_sklist_t       xact_detail_list;
  xact_detail_ptr_t curr_detail;
  rwsched_CFRunLoopTimerRef delay_between_xact_timer;
  int         delay_between_xacts;
  xact_order_e ordering;
  int curr_outstanding;
  int curr_xact_count;
  unsigned int num_xact_outstanding;
  unsigned int xact_max_with_outstanding;
};

RW_TYPE_DECL(xact_config);
RW_CF_TYPE_EXTERN(xact_config_ptr_t);

struct task_config_s {
  subscriber_config_t subsc_cfg;
  xact_config_t xact_cfg;
};

RW_TYPE_DECL(task_config);
RW_CF_TYPE_EXTERN(task_config_ptr_t);

struct rwdtsperf_component_s {
  CFRuntimeBase _base;
};

RW_TYPE_DECL(rwdtsperf_component);
RW_CF_TYPE_EXTERN(rwdtsperf_component_ptr_t);

struct rwdtsperf_instance_s {
  CFRuntimeBase        _base;
  rwdts_api_t          *dts_h;
  rwtasklet_info_ptr_t rwtasklet_info;
  task_config_t        config;
  rwdtsperf_component_ptr_t component;
  unsigned long        start_tv_sec;
  unsigned long        start_tv_usec;
  unsigned long        end_tv_sec;
  unsigned long        end_tv_usec;
  rwdts_member_reg_handle_t member_handle;
  xact_stats_t         statistics;
  rwdts_api_t*         apih;
};

RW_TYPE_DECL(rwdtsperf_instance);
RW_CF_TYPE_EXTERN(rwdtsperf_instance_ptr_t);

struct rwdtsperf_rsp_flavor_cb_s {
  rwdtsperf_instance_ptr_t instance;
  rsp_flavor_ptr_t rsp_flavor;
  int              rsp_count;
  const rwdts_xact_info_t*  xact_info;
  rwsched_CFRunLoopTimerRef rsp_delay_interval_timer;
};

RW_TYPE_DECL(rwdtsperf_rsp_flavor_cb);
RW_CF_TYPE_EXTERN(rwdtsperf_rsp_flavor_cb_ptr_t);

struct rwdtsperf_xact_detail_cb_s {
  rwdtsperf_instance_ptr_t instance;
  xact_detail_ptr_t xact_detail;
  int               xact_count;
  const rwdts_xact_info_t*  parent_xact;
  rwdtsperf_rsp_flavor_cb_ptr_t rsp_flavor_cb;
  struct timeval     start_time;
  rwsched_CFRunLoopTimerRef xact_delay_interval_timer;
};

RW_TYPE_DECL(rwdtsperf_xact_detail_cb);
RW_CF_TYPE_EXTERN(rwdtsperf_xact_detail_cb_ptr_t);

rwdtsperf_component_ptr_t
rwdtsperf_component_init(void);

void
rwdtsperf_component_deinit(rwdtsperf_component_ptr_t component);

rwdtsperf_instance_ptr_t
rwdtsperf_instance_alloc(rwdtsperf_component_ptr_t component,
			                   struct rwtasklet_info_s * rwtasklet_info,
			                   RwTaskletPlugin_RWExecURL *instance_url);

void 
rwdtsperf_instance_free(rwdtsperf_component_ptr_t component,
                        rwdtsperf_instance_ptr_t instance);

void
rwdtsperf_instance_start(rwdtsperf_component_ptr_t component,
                         rwdtsperf_instance_ptr_t instance);

void 
rwdtsperf_register_dts_callbacks(rwdtsperf_instance_ptr_t instance);

rwsched_CFRunLoopTimerRef
rwdtsperf_start_timer(rwtasklet_info_ptr_t            rwtasklet_info,
                      CFTimeInterval                  timer_interval,
                      rwsched_CFRunLoopTimerCallBack  cbfunc,
                      void                           *cb_arg,
                      int32_t                         periodic);

void                    
rwdtsperf_stop_timer(rwtasklet_info_ptr_t      rwtasklet_info,
                     rwsched_CFRunLoopTimerRef tmr_ref);
rwdts_member_rsp_code_t 
rwdtsperf_handle_xact(const rwdts_xact_info_t* xact_info,
                      RWDtsQueryAction         action,
                      const rw_keyspec_path_t*      key,
                      const ProtobufCMessage*  msg,
                      uint32_t credits,
                      void *getnext_ptr);
void
rwdtsperf_instance_stop(rwdtsperf_component_ptr_t component,
			                  rwdtsperf_instance_ptr_t instance);
__END_DECLS

#endif //__RWDTSPERF_H_
