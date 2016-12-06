
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

#if !defined(__RWSCHED_MAIN_H_)
#define __RWSCHED_MAIN_H_

#include <stdint.h>
#include <poll.h>
#include <glib.h>
#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"

__BEGIN_DECLS

#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"

#ifdef _CF_
union rwmsg_runloop_source_u {
  void *source;
  rwsched_CFRunLoopSourceRef cfsource;
  rwsched_CFRunLoopTimerRef cftimer;
};
#else
union rwmsg_runloop_source_u {
  void *source;
  rwsched_dispatch_source_t dispatch_source;
};
#endif // #ifdef _CF_

struct rwsched_cfsocket_callback_context_s {
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_ptr_t tasklet_info;
  rwsched_CFRunLoopSourceRef cfsource;
  rwsched_CFSocketCallBack cf_callout;
  CFSocketContext cf_context;
};

RWSCHED_TYPE_DECL(rwsched_cfsocket_callback_context);

struct rwsched_cftimer_callback_context_s {
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_ptr_t tasklet_info;
  rwsched_CFRunLoopSourceRef cfsource;
};

RWSCHED_TYPE_DECL(rwsched_cftimer_callback_context);

struct rwsched_CFSocket_s {
  CFRuntimeBase _base;
  uint64_t index;
#ifdef _CF_
  CFSocketRef cf_object;
  rwsched_CFSocketCallBack cf_callout;
  CFSocketContext cf_context;
  struct rwsched_cfsocket_callback_context_s callback_context;
#endif // _CF_

  uint64_t id;
  int fd;
  void *ud;
#ifdef _CF_
  rwsched_CFSocketRef cf_ref;
#endif // _CF_
};


struct rwsched_CFRunLoopTimer_s {
  CFRuntimeBase _base;
  uint64_t index;
#ifdef _CF_
  CFRunLoopTimerRef cf_object;
  CFRunLoopTimerContext cf_context;
  struct rwsched_cftimer_callback_context_s callback_context;

  //CFRunLoopTimerCallBack cf_callout;
  rwsched_CFRunLoopTimerCallBack cf_callout;
  CFRunLoopTimerContext tmp_context;
  uint32_t onetime_timer:1;
  uint32_t onetime_timer_fired_while_blocked:1;
  uint32_t release_called:1;
  uint32_t invalidate_called:1;
  uint32_t _pad:28;
  struct {
    CFAllocatorRef allocator;
    CFOptionFlags flags;
    CFIndex order;
  } ott;
#endif // _CF_

  uint64_t id;
  void *ud;
#ifdef _CF_
  CFRunLoopTimerRef cf_ref;
#endif // _CF_
};

struct rwsched_dispatch_closure_s {
  void *context;
  void (*handler)(void *ud);
};
typedef struct rwsched_dispatch_closure_s rwsched_dispatch_closure_t;

struct rwsched_dispatch_what_s {
 enum {
    RWSCHED_EVENT_NONE,
    RWSCHED_SOURCE_EVENT,
    RWSCHED_SOURCE_CANCEL,
    RWSCHED_DISPATCH_ASYNC
  } type;
  rwsched_dispatch_closure_t closure;
  rwsched_dispatch_source_t source;
  rwsched_dispatch_queue_t queue;
  rwsched_tasklet_ptr_t tasklet_info;
};
typedef struct rwsched_dispatch_what_s rwsched_dispatch_what_t;
typedef struct rwsched_dispatch_what_s *rwsched_dispatch_what_ptr_t;

struct rwsched_dispatch_context_s {
  void *user_context;
  rwsched_tasklet_ptr_t tasklet_info;
  rwsched_dispatch_source_t source;
  dispatch_function_t cancel_handler;
};
typedef struct rwsched_dispatch_context_s rwsched_dispatch_context_t;
typedef struct rwsched_dispatch_context_s *rwsched_dispatch_context_ptr_t;

struct rwsched_tasklet_counters_s {
    unsigned int sources;

    unsigned int queues;
    unsigned int sthread_queues;

    unsigned int sockets;
    unsigned int socket_sources;

    unsigned long  memory_allocated;
    unsigned long  memory_chunks;
};
typedef struct rwsched_tasklet_counters_s rwsched_tasklet_counters_t;

#define RWSCHED_MAX_SIGNALS 32
typedef void (*rwsched_sighandler_t)(rwsched_tasklet_t *, int);

struct rwsched_tasklet_s {
  CFRuntimeBase _base;
  gint ref_cnt;
  GArray *cftimer_array;
  GArray *cfsocket_array;
  GArray *cfsource_array;
  GArray *dispatch_what_array;
  struct {
    gboolean blocked;
    rwsched_CFRunLoopSourceRef cfsource;
  } blocking_mode;
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_counters_t counters;
  void *rwresource_track_handle;
  unsigned int signal_required;
  rwsched_sighandler_t signal_handler[RWSCHED_MAX_SIGNALS];
  void *signal_dtor_ud[RWSCHED_MAX_SIGNALS];
  GDestroyNotify signal_dtor[RWSCHED_MAX_SIGNALS];
};

void rwsched_tasklet_get_counters(rwsched_tasklet_t *info, rwsched_tasklet_counters_t *counters);

__END_DECLS

#endif // !defined(__RWSCHED_MAIN_H_)
