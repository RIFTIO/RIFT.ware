
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

RW_CF_TYPE_DEFINE("RW.Sched CFSocket Type", rwsched_CFSocketRef);

GType
rwsched_CFSocketCallBackType_get_type (void)
{
    static GType etype = 0;
    if (G_UNLIKELY(etype == 0)) {
        static const GEnumValue values[] = {
            { kCFSocketNoCallBack, "kCFSocketNoCallBack", "kCFSocketNoCallBack" },
            { kCFSocketReadCallBack, "kCFSocketReadCallBack", "kCFSocketReadCallBack" },
            { kCFSocketAcceptCallBack, "kCFSocketAcceptCallBack", "kCFSocketAcceptCallBack" },
            { kCFSocketDataCallBack, "kCFSocketDataCallBack", "kCFSocketDataCallBack" },
            { kCFSocketConnectCallBack, "kCFSocketConnectCallBack", "kCFSocketConnectCallBack" },
            { kCFSocketWriteCallBack, "kCFSocketWriteCallBack", "kCFSocketWriteCallBack" },
            { kCFSocketAutomaticallyReenableReadCallBack, "kCFSocketAutomaticallyReenableReadCallBack", "kCFSocketAutomaticallyReenableReadCallBack"},
            { kCFSocketAutomaticallyReenableAcceptCallBack, "kCFSocketAutomaticallyReenableAcceptCallBack", "kCFSocketAutomaticallyReenableAcceptCallBack" },
            { kCFSocketAutomaticallyReenableDataCallBack, "kCFSocketAutomaticallyReenableDataCallBack", "kCFSocketAutomaticallyReenableDataCallBack" },
            { kCFSocketAutomaticallyReenableWriteCallBack, "kCFSocketAutomaticallyReenableWriteCallBack", "kCFSocketAutomaticallyReenableWriteCallBack" },
            { kCFSocketLeaveErrors, "kCFSocketLeaveErrors", "kCFSocketLeaveErrors" },
            { kCFSocketCloseOnInvalidate, "kCFSocketCloseOnInvalidate", "kCFSocketCloseOnInvalidate" },
            { 0, NULL, NULL }
        };
        etype = g_enum_register_static(g_intern_static_string ("rwsched_CFSocketCallBackType"), values);
    }
    return etype;
}

CF_EXPORT void
rwsched_CFSocketInit(void)
{
  // Register the rwsched_CFSocket types
  RW_CF_TYPE_REGISTER(rwsched_CFSocketRef);
}

CF_EXPORT void
rwsched_tasklet_CFSocketGetContext(rwsched_tasklet_t *sched_tasklet,
			   rwsched_CFSocketRef s,
			   CFSocketContext *context)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

  // Call the native CFRunLoop function
  CFSocketGetContext(s->cf_object, context);

  // Use our internal context structure for now
  *context = s->callback_context.cf_context;
}

CF_EXPORT rwsched_CFSocketRef
rwsched_tasklet_CFSocketCreateWithNative(rwsched_tasklet_t *sched_tasklet,
				 CFAllocatorRef allocator,
				 CFSocketNativeHandle sock,
				 CFOptionFlags callBackTypes,
				 rwsched_CFSocketCallBack callout,
				 const CFSocketContext *context)
{
  rwsched_CFSocketRef rwsched_object;
  CFSocketCallBack native_callout;
  CFSocketContext native_context;
  unsigned int i;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a rwsched container type and track it
  rwsched_object = RW_CF_TYPE_MALLOC0(sizeof(*rwsched_object), rwsched_CFSocketRef);
  RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFSocketRef);

  // Look for an unused entry in cfsocket_array (start the indexes at 1 for now)
  //RW_GOBJECT_TYPE_VALIDATE(sched_tasklet->cfsocket_array, GArray);
  for (i = 1 ; i < sched_tasklet->cfsocket_array->len ; i++) {
    if (g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, i) == NULL) {
      g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, i) = rwsched_object;
      break;
    }
  }
  if (i >= sched_tasklet->cfsocket_array->len) {
    // Insert a new element at the end of the array
    g_array_append_val(sched_tasklet->cfsocket_array, rwsched_object);
  }

  // Mark the rwsched_object as in use
  RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFSocketRef);
  rwsched_object->index = i;

  // Fill in the callback context structure
  rwsched_instance_ref(instance);
  rwsched_object->callback_context.instance = instance;
  rwsched_tasklet_ref(sched_tasklet);
  rwsched_object->callback_context.tasklet_info = sched_tasklet;
  rwsched_object->callback_context.cfsource = NULL;
  rwsched_object->callback_context.cf_callout = callout;
  rwsched_object->callback_context.cf_context = *context;

  // Override the callout and context with an rwsched intercept method
  native_callout = rwsched_cfsocket_callout_intercept;
  native_context = *context;
  native_context.info = rwsched_object;
  
  // Call the native CFSocket function
  rwsched_object->cf_object = CFSocketCreateWithNative(allocator,
						       sock,
						       callBackTypes,
						       native_callout,
						       &native_context);
  RW_ASSERT(rwsched_object->cf_object);

  // Return the rwsched container type

  ck_pr_inc_32(&sched_tasklet->counters.sockets);

  return rwsched_object;
}

CF_EXPORT void
rwsched_tasklet_CFSocketRelease(rwsched_tasklet_t *sched_tasklet,
                        rwsched_CFSocketRef s)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

#if 1
  int i; for (i = 1 ; i < sched_tasklet->cfsocket_array->len ; i++) {
    if (g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, i) == s) {
      //g_array_remove_index (sched_tasklet->cfsocket_array, i);
      g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, i) = NULL;
      break;
    }
  }
#endif

  rwsched_tasklet_CFSocketInvalidate(sched_tasklet, s);

  CFRelease(s->cf_object);
  RW_CF_TYPE_FREE(s, rwsched_CFSocketRef);

  ck_pr_dec_32(&sched_tasklet->counters.sockets);

  rwsched_tasklet_unref(sched_tasklet);
  rwsched_instance_unref(instance);

}

CF_EXPORT rwsched_CFRunLoopSourceRef
rwsched_tasklet_CFSocketCreateRunLoopSource(rwsched_tasklet_t *sched_tasklet,
				    CFAllocatorRef allocator,
				    rwsched_CFSocketRef s,
				    CFIndex order)
{
  UNUSED(sched_tasklet);
  rwsched_CFRunLoopSourceRef rwsched_object;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a rwsched container type and track it
  rwsched_object = RW_CF_TYPE_MALLOC0(sizeof(*rwsched_object), rwsched_CFRunLoopSourceRef);

  // Update the callback context structure
  s->callback_context.cfsource = rwsched_object;

  // Create a CFSocketRunloopSource
  rwsched_object->cf_object = CFSocketCreateRunLoopSource(allocator, s->cf_object, order);
  RW_ASSERT(rwsched_object->cf_object);

  ck_pr_inc_32(&sched_tasklet->counters.socket_sources);

  rwsched_tasklet_ref(sched_tasklet);
  rwsched_instance_ref(instance);

  // Return the rwsched container type
  return rwsched_object;
}

CF_EXPORT void
rwsched_tasklet_CFSocketReleaseRunLoopSource(rwsched_tasklet_t *sched_tasklet,
                                     rwsched_CFRunLoopSourceRef rl)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  CFRelease(rl->cf_object);
  RW_CF_TYPE_FREE(rl, rwsched_CFRunLoopSourceRef);

  ck_pr_dec_32(&sched_tasklet->counters.socket_sources);

  rwsched_tasklet_unref(sched_tasklet);
  rwsched_instance_unref(instance);
}


CF_EXPORT void
rwsched_tasklet_CFSocketSetSocketFlags(rwsched_tasklet_t *sched_tasklet,
			       rwsched_CFSocketRef s,
			       CFOptionFlags flags)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

  // Call the native CFSocket function
  CFSocketSetSocketFlags(s->cf_object, flags);
}

CF_EXPORT void
rwsched_tasklet_CFSocketInvalidate(rwsched_tasklet_t *sched_tasklet,
			   rwsched_CFSocketRef s)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

  // Call the native CFSocket function
  CFSocketInvalidate(s->cf_object);
}

CF_EXPORT void
rwsched_tasklet_CFSocketEnableCallBacks(rwsched_tasklet_t *sched_tasklet,
                                rwsched_CFSocketRef s,
                                CFOptionFlags callBackTypes)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

  // Call the native CFSocket function
  CFSocketEnableCallBacks(s->cf_object, callBackTypes);
}

CF_EXPORT void
rwsched_tasklet_CFSocketDisableCallBacks(rwsched_tasklet_t *sched_tasklet,
                                 rwsched_CFSocketRef s,
                                 CFOptionFlags callBackTypes)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);

  // Call the native CFSocket function
  CFSocketDisableCallBacks(s->cf_object, callBackTypes);
}
