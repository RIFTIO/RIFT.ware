
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"
#include <ck_pr.h>

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

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a rwsched container type and track it
  rwsched_object = RW_CF_TYPE_MALLOC0(sizeof(*rwsched_object), rwsched_CFSocketRef);
  RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFSocketRef);

  g_array_append_val(sched_tasklet->cfsocket_array, rwsched_object);
  
  // Mark the rwsched_object as in use
  RW_CF_TYPE_VALIDATE(rwsched_object, rwsched_CFSocketRef);
  
  // Fill in the callback context structure
  rwsched_instance_ref(instance);
  rwsched_object->callback_context.instance = instance;
  rwsched_tasklet_ref(sched_tasklet);
  ck_pr_inc_32(&sched_tasklet->counters.cf_sockets);

  
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

  rwsched_tasklet_CFSocketInvalidate(sched_tasklet, s);

  CFRelease(s->cf_object);
  RW_CF_TYPE_FREE(s, rwsched_CFSocketRef);

  ck_pr_dec_32(&sched_tasklet->counters.cf_sockets);
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
  RW_CF_TYPE_VALIDATE(s, rwsched_CFSocketRef);
  int i;

  for (i = 1 ; i < sched_tasklet->cfsocket_array->len ; i++) {
    if (g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, i) == s) {
      g_array_remove_index_fast(sched_tasklet->cfsocket_array, i);
      break;
    }
  }

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
