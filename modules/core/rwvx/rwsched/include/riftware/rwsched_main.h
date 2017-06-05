
/*
 * STANDARD_RIFT_IO_COPYRIGHT
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
#ifdef _CF_
  CFRunLoopTimerRef cf_object;
  CFRunLoopTimerContext cf_context;
  struct rwsched_cftimer_callback_context_s callback_context;

  //CFRunLoopTimerCallBack cf_callout;
  rwsched_CFRunLoopTimerCallBack cf_callout;
  CFRunLoopTimerContext tmp_context;
  uint32_t onetime_timer:1;
  uint32_t onetime_timer_fired_while_blocked:1;
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
  unsigned int cf_timers; 
  unsigned int cf_sockets;/*Total number of CF native sockets using rwsched_tasklet_CFSocketCreateWithNative and released using rwsched_tasklet_CFSocketRelease*/
  unsigned int cf_sources;/*Total number of socket sources using rwsched_tasklet_CFSocketCreateRunLoopSource and released using rwsched_tasklet_CFSocketReleaseRunLoopSource*/

  //NEED TO ADD ONE COUNTER FOR EACH TYPE OF SOURCE...
  unsigned int ld_sources; /*Total number of sources of any kind incuding timers/sockets... */
  
  unsigned int ld_queues; /* Total number of queues of any kind including serial...*/
  unsigned int ld_sthreads; /*Total number of sthread queues and not included in the regular queues above*/
  
  unsigned int ld_whats; /* total number of async dispatch handlers. this should be equal to the number in the g_array -1 */
  
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
  pid_t         cfrunloop_tid;
};

void rwsched_tasklet_get_counters(rwsched_tasklet_t *info, rwsched_tasklet_counters_t *counters);

__END_DECLS

#endif // !defined(__RWSCHED_MAIN_H_)
