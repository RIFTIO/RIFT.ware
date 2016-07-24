
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"
#include "rw-sched-log.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"

#define THRESH_MS (10)

void
rwsched_cftimer_callout_intercept(CFRunLoopTimerRef timer,
                                  void *info)
{
  rwsched_instance_ptr_t instance;
  rwsched_CFRunLoopTimerRef rwsched_object = (rwsched_CFRunLoopTimerRef) info;
  rwsched_tasklet_ptr_t sched_tasklet;

  // Validate input parameters
  __CFGenericValidateType_(timer, CFRunLoopTimerGetTypeID(), __PRETTY_FUNCTION__);
  RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFRunLoopTimerRef);
  sched_tasklet = rwsched_object->callback_context.tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  instance = rwsched_object->callback_context.instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // #1 - Check if the current tasklet is in blocking mode; if a
  //      timer fired while blocking, then dont call the callback
  // #2 - Otherwise the current tasklet is not blocking and the timer callback
  //      shall be called
  if (sched_tasklet->blocking_mode.blocked) {
    /*
    printf("BLOK timer=%p valid=%d repeat=%d interval=%f next-fire=%f \n",
           timer, CFRunLoopTimerIsValid(timer), CFRunLoopTimerDoesRepeat(timer), CFRunLoopTimerGetInterval(timer),
           (CFRunLoopTimerGetNextFireDate(timer) - CFAbsoluteTimeGetCurrent()));
    */
    CFRunLoopRemoveTimer(rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet), rwsched_object->cf_object, instance->main_cfrunloop_mode);
    //CFRunLoopAddTimer(rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet), rwsched_object->cf_object, instance->deferred_cfrunloop_mode);
    if (rwsched_object->onetime_timer)
      rwsched_object->onetime_timer_fired_while_blocked = 1;
  }
  else {
    /*
    printf("MAIN timer=%p valid=%d repeat=%d interval=%f next-fire=%f \n",
           timer, CFRunLoopTimerIsValid(timer), CFRunLoopTimerDoesRepeat(timer), CFRunLoopTimerGetInterval(timer),
           (CFRunLoopTimerGetNextFireDate(timer) - CFAbsoluteTimeGetCurrent()));
    */
    g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
    g_tasklet_info = sched_tasklet;

    // The current task is not blocking so we can invoke the callback now

    struct timeval tv_begin, tv_end, tv_delta;
    if (sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_begin, NULL);
    } else {
      tv_begin.tv_sec = 0;
    }

    (rwsched_object->cf_callout)(rwsched_object, rwsched_object->cf_context.info);

    if (tv_begin.tv_sec
	&& sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_end, NULL);
      timersub(&tv_end, &tv_begin, &tv_delta);
      unsigned int cbms = (tv_delta.tv_sec * 1000 + tv_delta.tv_usec / 1000);
      if (cbms >= sched_tasklet->instance->latency.check_threshold_ms) {
	char *name = rw_btrace_get_proc_name(rwsched_object->cf_callout);
  RWSCHED_LOG_EVENT(instance, SchedDebug, RWLOG_ATTR_SPRINTF("rwsched[%d] CF timer took %ums ctx %p callback %s",
                                                             getpid(), cbms, rwsched_object->cf_context.info, name));
	free(name);
      }
    }

    g_tasklet_info = 0;
    g_rwresource_track_handle = 0;
  }
}

void
rwsched_cfrunloop_relocate_timers_to_main_mode(rwsched_tasklet_ptr_t sched_tasklet)
{
  //unsigned int i, j;
  unsigned int i;
  rwsched_CFRunLoopTimerRef rwsched_object;
  rwsched_instance_ptr_t instance;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);

  for (i = 1 ; i < sched_tasklet->cftimer_array->len ; i++) {
    if ((rwsched_object = g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, i)) != NULL) {
      RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFRunLoopTimerRef);
      instance = rwsched_object->callback_context.instance;
      RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
      //CFRunLoopRemoveTimer(rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet), rwsched_object->cf_object, instance->deferred_cfrunloop_mode);
      if (rwsched_object->onetime_timer_fired_while_blocked) {
        //(rwsched_object->cf_callout)(rwsched_object, rwsched_object->cf_context.info);
        RW_ASSERT(rwsched_object->onetime_timer);
        rwsched_object->cf_object = CFRunLoopTimerCreate(rwsched_object->ott.allocator,
                                                         CFAbsoluteTimeGetCurrent()+0.000001,
                                                         0,
                                                         rwsched_object->ott.flags,
                                                         rwsched_object->ott.order,
                                                         rwsched_cftimer_callout_intercept,
                                                         &rwsched_object->tmp_context);
        rwsched_object->onetime_timer_fired_while_blocked = 0;
      }
      CFRunLoopAddTimer(rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet), rwsched_object->cf_object, instance->main_cfrunloop_mode);
    }
  }
}

static void
rwsched_cfrunloop_relocate_source(rwsched_tasklet_ptr_t sched_tasklet,
                                  rwsched_CFRunLoopSourceRef source);

void
rwsched_cfsocket_callout_intercept(CFSocketRef s,
				   CFSocketCallBackType type,
				   CFDataRef address,
				   const void *data,
				   void *info)
{
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_ptr_t sched_tasklet;
  rwsched_cfsocket_callback_context_ptr_t callback_context;
  rwsched_CFSocketRef cfsocket = (rwsched_CFSocketRef) info;

  // Validate input parameters
  __CFGenericValidateType_(s, CFSocketGetTypeID(), __PRETTY_FUNCTION__);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);
  callback_context = &cfsocket->callback_context;
  instance = callback_context->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  sched_tasklet = callback_context->tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);

  // #1 - Check to see if the cfsocket matches the current blocking source for the tasklet
  // #2 - Otherwise check if the current tasklet is in blocking mode and then relocate the runloop source
  // #3 - Otherwise the current tasklet is not blocking and the event can be processed now
  if (sched_tasklet->blocking_mode.cfsource == callback_context->cfsource) {
    // Abort the runloop that is currently blocking
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    CFRunLoopStop(rl);
  }
  else if (sched_tasklet->blocking_mode.cfsource) {
    // The task is blocking and this event CANNOT be processed yet, so move it to a different runloop mode
    rwsched_cfrunloop_relocate_source(sched_tasklet, callback_context->cfsource);
  }
  else {
    // The current task is not blocking so we can invoke the callback now
    RW_ASSERT(callback_context->cf_callout);
    g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
    g_tasklet_info = sched_tasklet;


    struct timeval tv_begin, tv_end, tv_delta;
    if (sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_begin, NULL);
    } else {
      tv_begin.tv_sec = 0;
    }

    (*callback_context->cf_callout)(cfsocket, type, address, data, callback_context->cf_context.info);

    if (tv_begin.tv_sec
	&& sched_tasklet->instance->latency.check_threshold_ms) {
      gettimeofday(&tv_end, NULL);
      timersub(&tv_end, &tv_begin, &tv_delta);
      unsigned int cbms = (tv_delta.tv_sec * 1000 + tv_delta.tv_usec / 1000);
      if (cbms >= sched_tasklet->instance->latency.check_threshold_ms) {
	char *name = rw_btrace_get_proc_name(callback_context->cf_callout);
  RWSCHED_LOG_EVENT(instance, SchedDebug, RWLOG_ATTR_SPRINTF("rwsched[%d] CF socket/file took %ums ctx %p callback %s", 
                                                             getpid(), cbms, callback_context->cf_context.info, name));
	free(name);
      }
    }

    g_tasklet_info = 0;
    g_rwresource_track_handle = 0;
  }
}

static void
rwsched_cfrunloop_relocate_source(rwsched_tasklet_ptr_t sched_tasklet,
                                  rwsched_CFRunLoopSourceRef source)
{
  rwsched_CFRunLoopRef cfrunloop;
  //CFStringRef current_mode;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(source, rwsched_CFRunLoopSourceRef);
  //RW_GOBJECT_TYPE_VALIDATE(sched_tasklet->cfsource_array, GArray);

  cfrunloop = rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet);
  //current_mode = rwsched_tasklet_CFRunLoopCopyCurrentMode(sched_tasklet, cfrunloop);

  // Remove the runloop source from the current runloop mode
  rwsched_tasklet_CFRunLoopRemoveSource(sched_tasklet, cfrunloop, source, instance->main_cfrunloop_mode);

  g_array_append_val(sched_tasklet->cfsource_array, source);
}

void
rwsched_cfrunloop_relocate_sources_to_main_mode(rwsched_tasklet_ptr_t sched_tasklet)
{
  rwsched_CFRunLoopSourceRef source;
  rwsched_CFRunLoopRef cfrunloop;
  //CFStringRef current_mode;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  cfrunloop = rwsched_tasklet_CFRunLoopGetCurrent(sched_tasklet);
  //current_mode = rwsched_tasklet_CFRunLoopCopyCurrentMode(sched_tasklet, cfrunloop);

  //for (i = sched_tasklet->cfsource_array->len ; i > 1; i--) {
  while ((source = g_array_index(sched_tasklet->cfsource_array, rwsched_CFRunLoopSourceRef, 1)) != NULL) {;
    RW_CF_TYPE_VALIDATE(source, rwsched_CFRunLoopSourceRef);
    g_array_remove_index (sched_tasklet->cfsource_array, 1);
    rwsched_tasklet_CFRunLoopAddSource(sched_tasklet, cfrunloop, source, instance->main_cfrunloop_mode);
  }
}
