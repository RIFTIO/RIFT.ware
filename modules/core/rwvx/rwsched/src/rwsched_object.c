
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
rwsched_dispatch_cancel_handler_intercept(void *ud);

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

    rwsched_tasklet_unref(sched_tasklet);
    // Free the memory associated with the object
    RW_MAGIC_FREE((void *) object._object);

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

    rwsched_tasklet_unref(sched_tasklet);
    // Free the memory associated with the object
    RW_MAGIC_FREE((void *) object._object);

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
    dispatch_resume(object._object->header.libdispatch_object);
    return;
  }

  // Not yet implemented
  RW_CRASH();
}
#endif
