
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"
#include "rwsched_toysched.h"
#include "rwsched_internal.h"
#include "rw-sched-log.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"

void
rwsched_dispatch_cancel_handler_intercept(void *ud);

void
rwsched_dispatch_source_cancel(rwsched_tasklet_ptr_t sched_tasklet,
                               rwsched_dispatch_source_t source)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_source_t ds = source->header.libdispatch_object._ds;
#if 1
    rwsched_dispatch_cancel_handler_intercept(source);
    //source->is_source = FALSE;
#endif

    dispatch_source_cancel(ds);

    ck_pr_dec_32(&sched_tasklet->counters.sources);

    return;
  }

  // Not yet implemented
  RW_CRASH();
}

long
rwsched_dispatch_source_testcancel(rwsched_tasklet_ptr_t sched_tasklet,
				   rwsched_dispatch_source_t source)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  long rs = 0;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    if ((rs = dispatch_source_testcancel(source->header.libdispatch_object._ds))) {
      ck_pr_dec_32(&sched_tasklet->counters.sources);
    }
    return rs;
  }

  // Not yet implemented
  RW_CRASH();
  return 0;
}

unsigned long
rwsched_dispatch_source_get_data(rwsched_tasklet_ptr_t sched_tasklet,
                                 rwsched_dispatch_source_t source)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    return dispatch_source_get_data(source->header.libdispatch_object._ds);
  }

  // Not yet implemented
  RW_CRASH();
  return 0;
}

void
rwsched_dispatch_source_merge_data(rwsched_tasklet_ptr_t sched_tasklet,
                                   rwsched_dispatch_source_t source, 
                                   unsigned long data)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_source_merge_data(source->header.libdispatch_object._ds, data);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

static void
rwsched_dispatch_event_handler_intercept(void *ud)
{
  rwsched_dispatch_source_t source = (rwsched_dispatch_source_t)ud;
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);
  rwsched_tasklet_ptr_t sched_tasklet = source->tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  if (sched_tasklet->blocking_mode.blocked) {
    rwsched_dispatch_what_ptr_t what = (rwsched_dispatch_what_ptr_t) RW_MALLOC0_TYPE(sizeof(*what), rwsched_dispatch_what_ptr_t);
    what->type = RWSCHED_SOURCE_EVENT;
    what->source = source;
    g_array_append_val(sched_tasklet->dispatch_what_array, what);
  } else {
    g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
    g_tasklet_info = sched_tasklet;

    struct timeval tv_begin, tv_end, tv_delta;
    if (sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_begin, NULL);
    } else {
      tv_begin.tv_sec = 0;
    }

    (source->event_handler)(source->user_context);

    if (tv_begin.tv_sec
	&& sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_end, NULL);
      timersub(&tv_end, &tv_begin, &tv_delta);
      unsigned int cbms = (tv_delta.tv_sec * 1000 + tv_delta.tv_usec / 1000);
      if (cbms >= sched_tasklet->instance->latency.check_threshold_ms) {
	char *name = rw_btrace_get_proc_name(source->event_handler);
  RWSCHED_LOG_EVENT(instance, SchedDebug, RWLOG_ATTR_SPRINTF("rwsched[%d] dispatch event took %ums ctx %p callback %s",
                                                             getpid(), cbms, source->user_context, name));
	free(name);
      }
    }

    g_tasklet_info = 0;
  }
}

void
rwsched_dispatch_cancel_handler_intercept(void *ud)
{
  rwsched_dispatch_source_t source = (rwsched_dispatch_source_t)ud;
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);
  rwsched_tasklet_ptr_t sched_tasklet = source->tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  dispatch_source_set_cancel_handler_f(source->header.libdispatch_object._ds, NULL);
  if (sched_tasklet->blocking_mode.blocked) {
    RW_CRASH(); // Not implemented
  } else {
    if (source->cancel_handler) {
      g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
      g_tasklet_info = sched_tasklet;
      (source->cancel_handler)(source->user_context);
      g_tasklet_info = 0;
    }
  }
}

rwsched_dispatch_source_t
rwsched_dispatch_source_create(rwsched_tasklet_ptr_t sched_tasklet,
                               rwsched_dispatch_source_type_t type,
                               uintptr_t handle,
                               unsigned long mask,
                               rwsched_dispatch_queue_t queue)
{
  struct rwsched_dispatch_source_s *source;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);


  // Allocate memory for the dispatch source
  source = (rwsched_dispatch_source_t) RW_MALLOC0_TYPE(sizeof(*source), rwsched_dispatch_source_t); /* always leaked! */
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);
  source->tasklet_info = sched_tasklet;

  source->is_source = TRUE;
  source->tasklet_info = sched_tasklet;

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    source->header.libdispatch_object._ds = dispatch_source_create((dispatch_source_type_t)type,
                   handle,
                   mask,
                   queue->header.libdispatch_object._dq);
    RW_ASSERT(source->header.libdispatch_object._ds);
    dispatch_set_context(source->header.libdispatch_object, source);
    dispatch_source_set_cancel_handler_f(source->header.libdispatch_object._ds, rwsched_dispatch_cancel_handler_intercept);

    ck_pr_inc_32(&sched_tasklet->counters.sources);

    rwsched_tasklet_ref(sched_tasklet);
    return source;
  }

  // Not yet implemented
  RW_CRASH();
  return NULL;
}

void
rwsched_dispatch_source_set_event_handler_f(rwsched_tasklet_ptr_t sched_tasklet,
                                            rwsched_dispatch_source_t source,
                                            dispatch_function_t handler)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);
  RW_ASSERT(handler);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    source->event_handler = handler;
    dispatch_source_set_event_handler_f(source->header.libdispatch_object._ds, rwsched_dispatch_event_handler_intercept);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_source_set_cancel_handler_f(rwsched_tasklet_ptr_t sched_tasklet,
                                             rwsched_dispatch_source_t source,
                                             dispatch_function_t handler)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);
  RW_ASSERT(handler);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    source->cancel_handler = handler;
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_source_set_timer(rwsched_tasklet_ptr_t sched_tasklet,
                                  rwsched_dispatch_source_t source,
                                  rwsched_dispatch_time_t start,
                                  uint64_t interval,
                                  uint64_t leeway)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(source, rwsched_dispatch_source_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_source_set_timer(source->header.libdispatch_object._ds, start, interval, leeway);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_unblock_sources(rwsched_tasklet_ptr_t sched_tasklet)
{
  rwsched_dispatch_what_ptr_t what;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  g_tasklet_info = sched_tasklet;
  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  while ((what = g_array_index(sched_tasklet->dispatch_what_array, rwsched_dispatch_what_ptr_t, 1)) != NULL) {
    RW_ASSERT_TYPE(what, rwsched_dispatch_what_ptr_t);
    g_array_remove_index (sched_tasklet->dispatch_what_array, 1);
    switch(what->type) {
      case RWSCHED_SOURCE_EVENT:
        rwsched_dispatch_event_handler_intercept(what->source);
        break;
      case RWSCHED_SOURCE_CANCEL:
        rwsched_dispatch_cancel_handler_intercept(what->source);
        break;
      case RWSCHED_DISPATCH_ASYNC:
        (what->closure.handler)(what->closure.context);
        /*
        RW_ASSERT_TYPE(what->queue, rwsched_dispatch_queue_t);
        dispatch_async_f(what->queue->header.libdispatch_object._dq,
                         what->closure.context,
                         what->closure.handler);
        */
        break;
      default:
        RW_CRASH();
        break;
    }
    g_tasklet_info = 0;
    g_rwresource_track_handle = 0;
    RW_FREE_TYPE(what, rwsched_dispatch_what_ptr_t);
  }
}
