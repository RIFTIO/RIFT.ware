
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
#include "rwsched_main.h"
#include "rwsched_internal.h"

rwsched_dispatch_queue_t
rwsched_dispatch_queue_create(rwsched_tasklet_ptr_t sched_tasklet,
                              const char *label,
                              dispatch_queue_attr_t attr)
{
  struct rwsched_dispatch_queue_s *queue;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate memory for the dispatch queue
  queue = (rwsched_dispatch_queue_t) RW_MALLOC0_TYPE(sizeof(*queue), rwsched_dispatch_queue_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    queue->header.libdispatch_object._dq = dispatch_queue_create(label, attr);
    RW_ASSERT(queue->header.libdispatch_object._dq);

    rwsched_tasklet_ref(sched_tasklet);
    ck_pr_inc_32(&sched_tasklet->counters.queues);

    return queue;
  }

  // Not yet implemented
  RW_CRASH();
  return NULL;
}

#ifndef  __cplusplus
void
rwsched_dispatch_set_target_queue(rwsched_tasklet_ptr_t sched_tasklet,
                                  rwsched_dispatch_object_t object,
                                  rwsched_dispatch_queue_t queue)
{
#else
void
rwsched_dispatch_set_target_queue(rwsched_tasklet_ptr_t sched_tasklet,
                                  void *obj,
                                  rwsched_dispatch_queue_t queue)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) obj;
#endif
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_set_target_queue(object._object->header.libdispatch_object._do,
			      queue->header.libdispatch_object._dq);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

rwsched_dispatch_queue_t
rwsched_dispatch_get_default_queue(rwsched_tasklet_ptr_t sched_tasklet)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  RW_ASSERT_TYPE(instance->default_rwqueue, rwsched_dispatch_queue_t);
  return instance->default_rwqueue;
}

rwsched_dispatch_queue_t
rwsched_dispatch_get_global_queue(rwsched_tasklet_ptr_t sched_tasklet,
				  long priority) {
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  RW_ASSERT(priority >= DISPATCH_QUEUE_PRIORITY_BACKGROUND);
  RW_ASSERT(priority <= DISPATCH_QUEUE_PRIORITY_HIGH);
  int i=0;
  for (i=0; i<RWSCHED_DISPATCH_QUEUE_GLOBAL_CT; i++) {
    if (instance->global_rwqueue[i].pri == priority) {
      RW_ASSERT_TYPE(instance->global_rwqueue[i].rwq, rwsched_dispatch_queue_t);
      return instance->global_rwqueue[i].rwq;
    }
  }
  RW_CRASH();
  return NULL;
}

rwsched_dispatch_queue_t
rwsched_dispatch_get_main_queue(rwsched_instance_ptr_t instance)
{
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(instance->main_rwqueue, rwsched_dispatch_queue_t);
  return instance->main_rwqueue;
}

static void
rwsched_dispatch_intercept(void *ud)
{
  rwsched_dispatch_what_ptr_t what = ud;

  // Validate input paraemters
  RW_ASSERT_TYPE(what, rwsched_dispatch_what_ptr_t);
  RW_ASSERT(what->closure.handler);
  rwsched_tasklet_ptr_t sched_tasklet = what->tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);

  g_rwresource_track_handle = sched_tasklet->rwresource_track_handle;
  g_tasklet_info = sched_tasklet;

  (what->closure.handler)(what->closure.context);

  RW_FREE_TYPE(what, rwsched_dispatch_what_ptr_t);
  rwsched_tasklet_unref(sched_tasklet);

  g_tasklet_info = 0;
  g_rwresource_track_handle = 0;
}


void
rwsched_dispatch_async_f(rwsched_tasklet_ptr_t sched_tasklet,
                         rwsched_dispatch_queue_t queue,
                         void *context,
                         dispatch_function_t handler)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    RW_ASSERT(queue->header.libdispatch_object._dq);
    if (queue == instance->main_rwqueue &&
        sched_tasklet->blocking_mode.blocked) {
      //RW_CRASH();
      rwsched_dispatch_what_ptr_t what = (rwsched_dispatch_what_ptr_t) RW_MALLOC0_TYPE(sizeof(*what), rwsched_dispatch_what_ptr_t); /* always leaked! */
      what->type = RWSCHED_DISPATCH_ASYNC;
      what->closure.handler = handler;
      what->closure.context = context;
      what->queue = queue;
      g_array_append_val(sched_tasklet->dispatch_what_array, what);
    }
    else {
      rwsched_dispatch_what_ptr_t what = (rwsched_dispatch_what_ptr_t) RW_MALLOC0_TYPE(sizeof(*what), rwsched_dispatch_what_ptr_t);
      what->type = RWSCHED_DISPATCH_ASYNC;
      what->closure.handler = handler;
      what->closure.context = context;
      what->queue = queue;
      rwsched_tasklet_ref(sched_tasklet);
      what->tasklet_info = sched_tasklet;
      dispatch_async_f(queue->header.libdispatch_object._dq, (void*)what, rwsched_dispatch_intercept);
    }
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_after_f(rwsched_tasklet_ptr_t sched_tasklet,
                         dispatch_time_t when,
                         rwsched_dispatch_queue_t queue,
                         void *context,
                         dispatch_function_t handler)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    RW_ASSERT(queue->header.libdispatch_object._dq);
    if (queue == instance->main_rwqueue &&
        sched_tasklet->blocking_mode.blocked) {
      //RW_CRASH();
      rwsched_dispatch_what_ptr_t what = (rwsched_dispatch_what_ptr_t) RW_MALLOC0_TYPE(sizeof(*what), rwsched_dispatch_what_ptr_t); /* always leaked! */
      what->type = RWSCHED_DISPATCH_ASYNC;
      what->closure.handler = handler;
      what->closure.context = context;
      what->queue = queue;
      g_array_append_val(sched_tasklet->dispatch_what_array, what);
    }
    else {
      rwsched_dispatch_what_ptr_t what = (rwsched_dispatch_what_ptr_t) RW_MALLOC0_TYPE(sizeof(*what), rwsched_dispatch_what_ptr_t);
      what->type = RWSCHED_DISPATCH_ASYNC;
      what->closure.handler = handler;
      what->closure.context = context;
      what->queue = queue;
      rwsched_tasklet_ref(sched_tasklet);
      what->tasklet_info = sched_tasklet;
      dispatch_after_f(when, queue->header.libdispatch_object._dq, (void*)what, rwsched_dispatch_intercept);
    }
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_sync_f(rwsched_tasklet_ptr_t sched_tasklet,
                        rwsched_dispatch_queue_t queue,
                        void *context,
                        dispatch_function_t handler)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_sync_f(queue->header.libdispatch_object._dq, context, handler);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}
