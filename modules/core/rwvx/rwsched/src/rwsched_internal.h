
/*
 * STANDARD_RIFT_IO_COPYRIGHT
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
  gboolean is_sthread;
};

struct rwsched_dispatch_source_s {
  struct rwsched_dispatch_struct_header_s header;
  void *user_context;
  dispatch_function_t event_handler;
  dispatch_function_t cancel_handler;
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
    uint64_t clock_per_ms;
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

/*!
 * @function rwsched_dispatch_release
 *
 * @abstract
 * Decrement the reference count of a dispatch object.
 *
 * @discussion
 * A dispatch object is asynchronously deallocated once all references are
 * released (i.e. the reference count becomes zero). The system does not
 * guarantee that a given client is the last or only reference to a given
 * object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The object to release.
 * The result of passing NULL in this parameter is undefined.
 */
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
rwsched_dispatch_release(rwsched_tasklet_ptr_t tasklet_info,
                         rwsched_dispatch_object_t object);

#define RWSCHED_FPRINTF(file, ...)

#endif // !defined(__RWSCHED_INTERNAL_H_)
