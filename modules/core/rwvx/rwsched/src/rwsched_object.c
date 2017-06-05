
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"

#ifndef  __cplusplus
void
rwsched_dispatch_retain(rwsched_tasklet_ptr_t sched_tasklet,
                        rwsched_dispatch_object_t object)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_retain(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}


void
rwsched_dispatch_release(rwsched_tasklet_ptr_t sched_tasklet,
                         rwsched_dispatch_object_t object)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_release(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void *
rwsched_dispatch_get_context(rwsched_tasklet_ptr_t sched_tasklet,
                             rwsched_dispatch_object_t object)
{
  void *context;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    context = object._ds->user_context;
    //context = dispatch_get_context(object._object->header.libdispatch_object);
    return context;
  }

  // Not yet implemented
  RW_CRASH();
  return NULL;
}

void
rwsched_dispatch_set_context(rwsched_tasklet_ptr_t sched_tasklet,
                             rwsched_dispatch_object_t object,
                             void *context)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    object._ds->user_context = context;
    if (dispatch_get_context(object._object->header.libdispatch_object) == NULL) {
      dispatch_set_context(object._object->header.libdispatch_object, context);
    }
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_suspend(rwsched_tasklet_ptr_t sched_tasklet,
                         rwsched_dispatch_object_t object)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    g_atomic_int_inc(&object._object->header.suspended);
    dispatch_suspend(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_resume(rwsched_tasklet_ptr_t sched_tasklet,
                        rwsched_dispatch_object_t object)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    //libdispath does not have an api to check if an object is suspended or resumed..
    if (g_atomic_int_dec_and_test(&object._object->header.suspended) != 0){
      RW_ASSERT(0);
    }
    dispatch_resume(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}
#else
void
rwsched_dispatch_retain(rwsched_tasklet_ptr_t sched_tasklet,
                        void * object)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_retain(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_release(rwsched_tasklet_ptr_t sched_tasklet,
                         void * object)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_release(object._object->header.libdispatch_object);
    
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void *
rwsched_dispatch_get_context(rwsched_tasklet_ptr_t sched_tasklet,
                             void * object)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    context = dispatch_get_context(object._object->header.libdispatch_object);
    //context = source->context;
    //context = dispatch_get_context(object._object->header.libdispatch_object);
    return context;
  }

  // Not yet implemented
  RW_CRASH();
  return NULL;
}

void
rwsched_dispatch_set_context(rwsched_tasklet_ptr_t sched_tasklet,
                             void *object,
                             void *context)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    dispatch_set_context(object._object->header.libdispatch_object, context);
    //source->context = context;
    //dispatch_set_context(object._object->header.libdispatch_object, source);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_suspend(rwsched_tasklet_ptr_t sched_tasklet,
                         void * object)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    g_atomic_int_inc(&object._object->header.suspended);
    dispatch_suspend(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}

void
rwsched_dispatch_resume(rwsched_tasklet_ptr_t sched_tasklet,
                        void * object)
{
  rwsched_dispatch_object_t object = (rwsched_dispatch_object_t) object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only) {
    if (g_atomic_int_dec_and_test(&object._object->header.suspended) != 0){
      RW_ASSERT(0);
    }
    dispatch_resume(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}
#endif
