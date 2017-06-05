
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __CFRUNLOOP_H__
#define __CFRUNLOOP_H__

#ifdef __GI_SCANNER__
#include <glib.h>
#include <glib-object.h>
#else
#include "rwsched.h"
#endif
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFRunLoop.h>

__BEGIN_DECLS

struct rwsched_CFRunLoopSource_s {
  CFRunLoopSourceRef cf_object;
};

RW_CF_TYPE_EXTERN(rwsched_CFRunLoopRef);
RW_CF_TYPE_EXTERN(rwsched_CFRunLoopSourceRef);
RW_CF_TYPE_EXTERN(rwsched_CFRunLoopTimerRef);

/*!
 * @function rwsched_CFRunLoopInit
 *
 * @abstract
 * TBD
 *
 * @abstract
 * TBD
 *
 */
CF_EXPORT void
rwsched_CFRunLoopInit(void);

/*!
 * @function rwsched_instance_CFRunLoopRun
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param instance
 * rwsched instance handle
 */
CF_EXPORT void
rwsched_instance_CFRunLoopRun(rwsched_instance_t *instance);

/*!
 * @function rwsched_instance_CFRunLoopStop
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param instance
 * rwsched instance handle
 */
CF_EXPORT void
rwsched_instance_CFRunLoopStop(rwsched_instance_t *instance);

/// @cond GI_SCANNER
/**
 * rwsched_CFRunLoopTimerCallBack:
 * @timer:
 * @user_data: (closure)
 */
/// @endcond
typedef void (*rwsched_CFRunLoopTimerCallBack)(rwsched_CFRunLoopTimerRef timer,
                                               void *user_data);

/// @cond GI_SCANNER
/**
 * rwsched_instance_CFRunLoopGetMainMode:
 * @instance:
 * Returns: (type CF.String) (transfer none)
 */
/// @endcond
CFStringRef rwsched_instance_CFRunLoopGetMainMode(rwsched_instance_t *instance);

/// @cond GI_SCANNER
/**
 * rwsched_instance_CFRunLoopSetMainMode:
 * @instance:
 * @mode: (type CF.String)
 */
/// @endcond
void rwsched_instance_CFRunLoopSetMainMode(rwsched_instance_t *instance, 
                                           CFStringRef mode);

/// @cond GI_SCANNER
/**
 * rwsched_instance_CFRunLoopGetDeferredMode:
 * @instance:
 * Returns: (type CF.String) (transfer none)
 */
/// @endcond
CFStringRef rwsched_instance_CFRunLoopGetDeferredMode(rwsched_instance_t *instance);

/// @cond GI_SCANNER
/**
 * rwsched_instance_CFRunLoopSetDeferredMode:
 * @instance:
 * @mode: (type CF.String)
 */
/// @endcond
void rwsched_instance_CFRunLoopSetDeferredMode(rwsched_instance_t *instance, 
                                               CFStringRef mode);

/*!
 * @function rwsched_tasklet_CFRunLoopGetCurrent
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 *
 * @result
 * TBD
 */
/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopGetCurrent:
 * @tasklet:
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT rwsched_CFRunLoopRef
rwsched_tasklet_CFRunLoopGetCurrent(rwsched_tasklet_t *tasklet);

/*!
 * @function rwsched_tasklet_CFRunLoopCopyCurrentMode
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param rl
 * TBD
 *
 * @result
 * TBD
 */
/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopCopyCurrentMode:
 * @tasklet:
 * @rl:
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT CFStringRef
rwsched_tasklet_CFRunLoopCopyCurrentMode(rwsched_tasklet_t *tasklet,
                                         rwsched_CFRunLoopRef rl);

/*!
 * @function rwsched_instance_CFRunLoopRunInMode
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param instance
 * rwsched instance handle
 * @param mode
 * TBD
 * @param seconds
 * TBD
 * @param returnAfterSourceHandled
 * TBD
 *
 * @result
 * TBD
 */
/// @cond GI_SCANNER
/**
 * rwsched_instance_CFRunLoopRunInMode:
 * @instance:
 * @mode: (type CF.String)
 * @seconds: (type CF.TimeInterval)
 * @returnAfterSourceHandled: (type CF.Boolean)
 *
 * Returns: (type CF.SInt32) (transfer none)
 **/
/// @endcond
CF_EXPORT SInt32
rwsched_instance_CFRunLoopRunInMode(rwsched_instance_t *instance,
                                    CFStringRef mode,
                                    CFTimeInterval seconds,
                                    Boolean returnAfterSourceHandled);

/// @cond GI_SCANNER
/**
 * rwsched_instance_CFAllocatorGetDefault:
 * @instance:
 *
 * Returns: (type CF.Allocator) (transfer none)
 **/
/// @endcond
CF_EXPORT CFAllocatorRef
rwsched_instance_CFAllocatorGetDefault(rwsched_instance_t *instance);

/*!
 * @function rwsched_tasklet_CFRunLoopRunTaskletBlock
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param blocking_source
 * CFRunLoop source that will wakeup blocking mode
 * @seconds_to_wait
 * Time to wait for a source callback before aborting blocking mode
 *
 * @result
 * TBD
 */

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopRunTaskletBlock:
 * @tasklet:
 * @blocking_source:
 * @seconds_to_wait: (type CF.TimeInterval)
 *
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT rwsched_CFRunLoopSourceRef
rwsched_tasklet_CFRunLoopRunTaskletBlock(rwsched_tasklet_t *tasklet,
                                         rwsched_CFRunLoopSourceRef blocking_source,
                                         CFTimeInterval seconds_to_wait);

/*!
 * @function rwsched_tasklet_CFRunLoopAddSource
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param rl
 * TBD
 * @param source
 * TBD
 * @param mode
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopAddSource(rwsched_tasklet_t *tasklet,
                                   rwsched_CFRunLoopRef rl,
                                   rwsched_CFRunLoopSourceRef source,
                                   CFStringRef mode);

/*!
 * @function rwsched_tasklet_CFRunLoopRemoveSource
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param rl
 * TBD
 * @param source
 * TBD
 * @param mode
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopRemoveSource(rwsched_tasklet_t *tasklet,
                                      rwsched_CFRunLoopRef rl,
                                      rwsched_CFRunLoopSourceRef source,
                                      CFStringRef mode);

/*!
 * @function rwsched_tasklet_CFRunLoopSourceInvalidate
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param source
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopSourceInvalidate(rwsched_tasklet_t *tasklet,
                                          rwsched_CFRunLoopSourceRef source);


/*!
 * @function rwsched_tasklet_CFRunLoopTimerCreate
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param allocator
 * TBD
 * @param fireDate
 * TBD
 * @param interval
 * TBD
 * @param flags
 * TBD
 * @param order
 * TBD
 * @param callout
 * TBD
 * @param context
 * TBD
 * 
 * @result
 * TBD
 */

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopTimerCreate:
 * @tasklet:
 * @allocator: (nullable)
 * @fireDate:
 * @interval:
 * @flags:
 * @order:
 * @callout: (scope async)
 * @context:
 *
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT rwsched_CFRunLoopTimerRef
rwsched_tasklet_CFRunLoopTimerCreate(rwsched_tasklet_t *tasklet,
                                     CFAllocatorRef allocator,
                                     CFAbsoluteTime fireDate,
                                     CFTimeInterval interval,
                                     CFOptionFlags flags,
                                     CFIndex order,
                                     rwsched_CFRunLoopTimerCallBack callout,
                                     rwsched_CFRunLoopTimerContext *context);

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopTimer:
 * @tasklet:
 * @fireDate:
 * @interval:
 * @callback: (scope notified)
 * @user_data:
 *
 * Returns: (transfer none)
 **/
/// @endcond
rwsched_CFRunLoopTimerRef 
rwsched_tasklet_CFRunLoopTimer(rwsched_tasklet_t *tasklet,
                               CFAbsoluteTime fireDate,
                               CFTimeInterval interval,
                               rwsched_CFRunLoopTimerCallBack callback,
                               gpointer user_data);

/*!
 * @function rwsched_tasklet_CFRunLoopTimerGetContext
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param timer
 * TBD
 * @param context
 * TBD
 */

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopTimerGetContext:
 * @tasklet:
 * @timer:
 * @context: (out)
 **/
/// @endcond
CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerGetContext(rwsched_tasklet_t *tasklet,
                                         rwsched_CFRunLoopTimerRef timer,
                                         rwsched_CFRunLoopTimerContext *context);

/*!
 * @function rwsched_tasklet_CFRunLoopAddTimer
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param rl
 * TBD
 * @param timer
 * TBD
 * @param mode
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopAddTimer(rwsched_tasklet_t *tasklet,
                                  rwsched_CFRunLoopRef rl,
                                  rwsched_CFRunLoopTimerRef timer,
                                  CFStringRef mode);

/*!
 * @function rwsched_tasklet_CFRunLoopRemoveTimer
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched tasklet handle
 * @param rl              
 * TBD
 * @param timer
 * TBD
 * @param mode
 * TBD
 *
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopRemoveTimer(rwsched_tasklet_t *tasklet,
                                     rwsched_CFRunLoopRef rl,
                                     rwsched_CFRunLoopTimerRef timer,
                                     CFStringRef mode);

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopTimerSetNextFireDate:
 * @tasklet:
 * @timer:
 * @fireDate
 **/
/// @endcond
CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerSetNextFireDate(
    rwsched_tasklet_t *tasklet,
    rwsched_CFRunLoopTimerRef timer,
    CFAbsoluteTime fireDate);

/*!
 * @function rwsched_tasklet_CFRunLoopTimerIsValid
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * tasklet info pointer
 * @param timer
 *
 * @returns
 */
/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFRunLoopTimerIsValid:
 * @tasklet:
 * @timer:
 * Returns: (type CF.Boolean) (transfer none)
 **/
/// @endcond
CF_EXPORT Boolean
rwsched_tasklet_CFRunLoopTimerIsValid(rwsched_tasklet_t *tasklet,
                                      rwsched_CFRunLoopTimerRef timer);

/*!
 * @function rwsched_tasklet_CFRunLoopTimerInvalidate
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * rwsched instance handle
 * @param timer
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerInvalidate(rwsched_tasklet_t *tasklet,
                                         rwsched_CFRunLoopTimerRef timer);
/*!
 * @function rwsched_tasklet_CFRunLoopTimerRelease
 *
 * @abstract
 * TBD
 *
 * @discussion
 * TBD
 *
 * @param tasklet
 * tasklet info pointer
 * @param timer
 * TBD
 */
CF_EXPORT void
rwsched_tasklet_CFRunLoopTimerRelease(rwsched_tasklet_t *tasklet,
                                      rwsched_CFRunLoopTimerRef timer);
#ifndef __GI_SCANNER__
#endif
__END_DECLS

#endif //__CFRUNLOOP_H__
