
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

#include "rwsched/cfrunloop.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"

RW_CF_TYPE_DEFINE("RW.Sched CFRunLoopSource Type", rwsched_CFRunLoopSourceRef);
RW_CF_TYPE_DEFINE("RW.Sched CFRunLoopTimer Type", rwsched_CFRunLoopTimerRef);

CF_EXPORT void
rwsched_CFRunLoopInit(void)
{
  // Register the rwsched_CFRunLoop types
  RW_CF_TYPE_REGISTER(rwsched_CFRunLoopSourceRef);
  RW_CF_TYPE_REGISTER(rwsched_CFRunLoopTimerRef);
}

CF_EXPORT rwsched_CFRunLoopRef
rwsched_tasklet_CFRunLoopGetCurrent(rwsched_tasklet_ptr_t sched_tasklet)
{
  rwsched_CFRunLoopRef rwsched_object;
  CFRunLoopRef rl;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoop function
  rl = CFRunLoopGetCurrent();
  rwsched_object = rl;
  
  // Return the rwsched container type
  return rwsched_object;
}

CF_EXPORT CFStringRef
rwsched_tasklet_CFRunLoopCopyCurrentMode(rwsched_tasklet_ptr_t sched_tasklet,
				 rwsched_CFRunLoopRef rl)
{
  CFStringRef mode;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  // RW_CF_TYPE_VALIDATE(rl, rwsched_CFRunLoopRef);

  // Call the native CFRunLoop function
  mode = CFRunLoopCopyCurrentMode(rl);
  
  // Return the native CFRunLoop mode
  return mode;
}

CF_EXPORT void
rwsched_instance_CFRunLoopRun(rwsched_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoop function
  CFRunLoopRun();
}

CF_EXPORT void
rwsched_instance_CFRunLoopStop(rwsched_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoopStop function
  CFRunLoopStop(CFRunLoopGetCurrent());
}

CF_EXPORT SInt32
rwsched_instance_CFRunLoopRunInMode(rwsched_instance_ptr_t instance,
			   CFStringRef mode,
			   CFTimeInterval seconds,
			   Boolean returnAfterSourceHandled)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoop function
  return CFRunLoopRunInMode(mode, seconds, returnAfterSourceHandled);
}

CF_EXPORT CFAllocatorRef
rwsched_instance_CFAllocatorGetDefault(rwsched_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoopStop function
  return CFAllocatorGetDefault();
}

CF_EXPORT rwsched_CFRunLoopSourceRef
rwsched_tasklet_CFRunLoopRunTaskletBlock(rwsched_tasklet_ptr_t sched_tasklet,
				 rwsched_CFRunLoopSourceRef blocking_source,
				 CFTimeInterval secondsToWait)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  SInt32 runloop_status;
  rwsched_CFRunLoopSourceRef return_source = NULL;
  //int i = 0;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Make sure this tasklet is not already blocked
  RW_ASSERT(!sched_tasklet->blocking_mode.blocked);

  // Mark the tasklet as blocked
  RW_ZERO_VARIABLE(&sched_tasklet->blocking_mode);
  sched_tasklet->blocking_mode.blocked = TRUE;
  sched_tasklet->blocking_mode.cfsource = blocking_source;

  // Run the run loop in the default mode for up to the block interval specified
  // The run loop will be stopped if an event is avilable for the wakeupSource
  runloop_status = CFRunLoopRunInMode(instance->main_cfrunloop_mode, secondsToWait, false);

  // If the return value is kCFRunLoopRunTimedOut then the run loop timed out
  // If the return value is kCFRunLoopRunStopped then an event occured on the wakeupSource
  // If any other return value occurs, it is considered to be an error
  if (runloop_status == kCFRunLoopRunTimedOut) {
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    return_source = blocking_source;
    if (CFRunLoopContainsSource(rl, blocking_source->cf_object, instance->main_cfrunloop_mode)) {
      return_source = NULL;
    }
  }
  else if (runloop_status == kCFRunLoopRunStopped || runloop_status == kCFRunLoopRunFinished) {
    return_source = blocking_source;
  }
  else {
    RW_CRASH();
  }

  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  g_tasklet_info = sched_tasklet;

  // relocate timers back to the main-mode
  rwsched_cfrunloop_relocate_timers_to_main_mode(sched_tasklet);

  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  g_tasklet_info = sched_tasklet;

  // relocate sources back to the main-mode
  rwsched_cfrunloop_relocate_sources_to_main_mode(sched_tasklet);

  //runloop_status = rwsched_instance_CFRunLoopRunInMode(instance, instance->deferred_cfrunloop_mode, 0.0, false);
  //RW_ASSERT(runloop_status == kCFRunLoopRunFinished || runloop_status == kCFRunLoopRunTimedOut);

  // Mark the tasklet as unblocked
  RW_ZERO_VARIABLE(&sched_tasklet->blocking_mode);

  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  g_tasklet_info = sched_tasklet;

  //Deliver any events on the dispatch sources
  rwsched_dispatch_unblock_sources(sched_tasklet); 

  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  g_tasklet_info = sched_tasklet;

  // Return the runloop source that wokeup the blocking call, which is NULL if it timed out
  return return_source;
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopAddSource(rwsched_tasklet_ptr_t sched_tasklet,
			   rwsched_CFRunLoopRef rl,
			   rwsched_CFRunLoopSourceRef source,
			   CFStringRef mode)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  // RW_CF_TYPE_VALIDATE(rl, rwsched_CFRunLoopRef);
  RW_CF_TYPE_VALIDATE(source, rwsched_CFRunLoopSourceRef);

  // Call the native CFRunLoop function
  CFRunLoopAddSource(rl, source->cf_object, mode);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopRemoveSource(rwsched_tasklet_ptr_t sched_tasklet,
			      rwsched_CFRunLoopRef rl,
			      rwsched_CFRunLoopSourceRef source,
			      CFStringRef mode)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  // RW_CF_TYPE_VALIDATE(rl, rwsched_CFRunLoopRef);
  RW_CF_TYPE_VALIDATE(source, rwsched_CFRunLoopSourceRef);

  // Call the native CFRunLoop function
  CFRunLoopRemoveSource(rl, source->cf_object, mode);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopSourceInvalidate(rwsched_tasklet_ptr_t sched_tasklet,
				  rwsched_CFRunLoopSourceRef source)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(source, rwsched_CFRunLoopSourceRef);

  // Call the native CFRunLoop function
  CFRunLoopSourceInvalidate(source->cf_object);
}


CF_EXPORT rwsched_CFRunLoopTimerRef
rwsched_tasklet_CFRunLoopTimerCreate(rwsched_tasklet_ptr_t sched_tasklet,
			     CFAllocatorRef allocator,
			     CFAbsoluteTime fireDate,
			     CFTimeInterval interval,
			     CFOptionFlags flags,
			     CFIndex order,
			     rwsched_CFRunLoopTimerCallBack callout,
			     rwsched_CFRunLoopTimerContext *context)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  rwsched_CFRunLoopTimerRef rwsched_timer;
  unsigned int i;

  RW_ASSERT(context->info);

  // Allocate a rwsched container type and track it
  rwsched_timer = RW_CF_TYPE_MALLOC0(sizeof(*rwsched_timer), rwsched_CFRunLoopTimerRef);

  // Look for an unused entry in cftimer_array (start the indexes at 1 for now)
  //RW_GOBJECT_TYPE_VALIDATE(sched_tasklet->cftimer_array, GArray);
  for (i = 1 ; i < sched_tasklet->cftimer_array->len ; i++) {
    if (g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, i) == NULL) {
      g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, i) = rwsched_timer;
      break;
    }
  }
  if (i >= sched_tasklet->cftimer_array->len) {
    // Insert a new element at the end of the array
    g_array_append_val(sched_tasklet->cftimer_array, rwsched_timer);
  }

  // Mark the rwsched_timer as in use
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);
  rwsched_timer->index = i;

  if (interval == 0) {
    rwsched_timer->onetime_timer = 1;
    rwsched_timer->ott.allocator = allocator;
    rwsched_timer->ott.flags = flags;
    rwsched_timer->ott.order = order;
  }

  // Call the native CFRunLoop function
  if (context && context->info == NULL) { context->info = rwsched_timer; }
  rwsched_timer->tmp_context = *context;
  rwsched_timer->tmp_context.info = rwsched_timer;
  rwsched_timer->cf_callout = callout;
  rwsched_tasklet_ref(sched_tasklet);
  rwsched_timer->callback_context.tasklet_info = sched_tasklet;
  rwsched_instance_ref(instance);
  rwsched_timer->callback_context.instance = instance;
  rwsched_timer->cf_object = CFRunLoopTimerCreate(allocator,
                                                   fireDate,
                                                   interval,
                                                   flags,
                                                   order,
                                                   rwsched_cftimer_callout_intercept,
                                                   &rwsched_timer->tmp_context);
  rwsched_timer->cf_context = *context;

  // Return the rwsched container type
  return rwsched_timer;
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerRelease(rwsched_tasklet_ptr_t sched_tasklet,
                              rwsched_CFRunLoopTimerRef rwsched_timer)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  rwsched_timer->release_called = 1;

  if (rwsched_timer->index)
    rwsched_tasklet_CFRunLoopTimerInvalidate(sched_tasklet, rwsched_timer);

  // Call the native CFRunLoop function
  CFRelease(rwsched_timer->cf_object);
  
  RW_CF_TYPE_FREE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  rwsched_tasklet_unref(sched_tasklet);
  rwsched_instance_unref(instance);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerInvalidate(rwsched_tasklet_ptr_t sched_tasklet,
					 rwsched_CFRunLoopTimerRef rwsched_timer)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  rwsched_timer->invalidate_called = 1;
#if 1
  int i; for (i = 1 ; i < sched_tasklet->cftimer_array->len ; i++) {
    if (g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, i) == rwsched_timer) {
      g_array_remove_index (sched_tasklet->cftimer_array, i);
      break;
    }
  }
#else
  RW_ASSERT(rwsched_timer->index != 0);
  RW_ASSERT(sched_tasklet->cftimer_array->len > rwsched_timer->index);
  g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, rwsched_timer->index) = NULL;
  rwsched_timer->index = 0;
#endif
  // Call the native CFRunLoop function
  CFRunLoopTimerInvalidate(rwsched_timer->cf_object);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerGetContext(rwsched_tasklet_ptr_t sched_tasklet,
				 rwsched_CFRunLoopTimerRef rwsched_timer,
				 rwsched_CFRunLoopTimerContext *context)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  // Call the native CFRunLoop function
  CFRunLoopTimerGetContext(rwsched_timer->cf_object, context);

  // Use our internal context structure for now
  context->info = rwsched_timer->cf_context.info;
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopAddTimer(rwsched_tasklet_ptr_t sched_tasklet,
			  rwsched_CFRunLoopRef rl,
			  rwsched_CFRunLoopTimerRef rwsched_timer,
			  CFStringRef mode)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  // Call the native CFRunLoop function
  CFRunLoopAddTimer(rl, rwsched_timer->cf_object, mode);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopRemoveTimer(rwsched_tasklet_ptr_t sched_tasklet,
			     rwsched_CFRunLoopRef rl,
			     rwsched_CFRunLoopTimerRef rwsched_timer,
			     CFStringRef mode)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  // Call the native CFRunLoop function
  CFRunLoopRemoveTimer(rl, rwsched_timer->cf_object, mode);
}

CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerSetNextFireDate(
    rwsched_tasklet_t *sched_tasklet,
    rwsched_CFRunLoopTimerRef rwsched_timer,
    CFAbsoluteTime fireDate)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwsched_timer, rwsched_CFRunLoopTimerRef);

  CFRunLoopTimerSetNextFireDate(rwsched_timer->cf_object, fireDate);
}

CF_EXPORT Boolean
rwsched_tasklet_CFRunLoopTimerIsValid(rwsched_tasklet_ptr_t sched_tasklet,
                              rwsched_CFRunLoopTimerRef rwsched_timer)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Call the native CFRunLoop function
  return CFRunLoopTimerIsValid(rwsched_timer->cf_object);
}
