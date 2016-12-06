
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
 *
 */

#if !defined(__RWSCHED_INTERNAL_H_)
#define __RWSCHED_INTERNAL_H_

#include <glib.h>
#include "rwlog.h"
#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"

extern __thread void *g_tasklet_info;

struct rwsched_dispatch_queue_s {
  struct rwsched_dispatch_struct_header_s header;
  void *user_context;
  gboolean is_source;
  gboolean is_sthread;
};

struct rwsched_dispatch_source_s {
  struct rwsched_dispatch_struct_header_s header;
  void *user_context;
  gboolean is_source;
  dispatch_function_t event_handler;
  dispatch_function_t cancel_handler;
  rwsched_tasklet_ptr_t tasklet_info;
};

struct rwsched_instance_s {
  CFRuntimeBase _base;
  gint ref_cnt;
  rwsched_config_t config;
  gboolean use_libdispatch_only;
  gboolean sthread_initialized;
  struct rwsched_dispatch_queue_s *default_rwqueue;
  struct rwsched_dispatch_queue_s *main_rwqueue;
#define RWSCHED_DISPATCH_QUEUE_GLOBAL_CT (4)
  struct {
    long pri;
    struct rwsched_dispatch_queue_s *rwq;
  } global_rwqueue[RWSCHED_DISPATCH_QUEUE_GLOBAL_CT];
  CFStringRef main_cfrunloop_mode;
  CFStringRef deferred_cfrunloop_mode;
  GArray *tasklet_array;
  unsigned int signal_required;
  rwlog_ctx_t *rwlog_instance;
  struct {
    int check_threshold_ms;
  } latency;
  struct {
    unsigned int total_cb_registered;
    unsigned int total_cb_invoked;
    unsigned int total_latency_ts;
  } stats;
};

void
rwsched_cftimer_callout_intercept(CFRunLoopTimerRef timer,
				                          void *info);
void
rwsched_cfrunloop_relocate_timers_to_main_mode(rwsched_tasklet_ptr_t tasklet_info);

void
rwsched_cfrunloop_relocate_sources_to_main_mode(rwsched_tasklet_ptr_t tasklet_info);
void
rwsched_cfsocket_callout_intercept(CFSocketRef s,
				   CFSocketCallBackType type,
				   CFDataRef address,
				   const void *data,
				   void *info);

void
rwsched_dispatch_unblock_sources(rwsched_tasklet_ptr_t tasklet_info);

extern unsigned int g_rwsched_instance_count;
extern unsigned int g_rwsched_tasklet_count;
#define RWSCHED_FPRINTF(file, ...)

#endif // !defined(__RWSCHED_INTERNAL_H_)
