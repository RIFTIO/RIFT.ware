/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwsched_source.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 12/04/2013
 * @brief API include for rwsched dispatch source functions
 */

#ifndef __RWSCHED_SOURCE_H__
#define __RWSCHED_SOURCE_H__

__BEGIN_DECLS

#include "rwsched.h"

/*!
 * @typedef rwsched_dispatch_function_t
 *
 * @abstract
 * Callback function to invoke when a dispatch source is processed on a queue
 */

typedef dispatch_function_t rwsched_dispatch_function_t;

/*!
 * @typedef rwsched_dispatch_source_t
 *
 * @abstract
 * Dispatch sources are used to automatically submit event handler blocks to
 * dispatch queues in response to external events.
 */

RWSCHED_DISPATCH_DECL(rwsched_dispatch_source);

/*!
 * @typedef rwsched_dispatch_source_type_t
 *
 * @abstract
 * Constants of this type represent the class of low-level system object that
 * is being monitored by the dispatch source. Constants of this type are
 * passed as a parameter to rwsched_dispatch_source_create() and determine how the
 * handle argument is interpreted (i.e. as a file descriptor, mach port,
 * signal number, process identifer, etc.), and how the mask arugment is
 * interpreted.
 */

#if 1
typedef struct rwsched_dispatch_source_type_s rwsched_dispatch_source_type_s;
typedef const struct rwsched_dispatch_source_type_s *rwsched_dispatch_source_type_t;
#else
typedef dispatch_source_type_t rwsched_dispatch_source_type_t;
#endif

/*!
 * @const RWSCHED_DISPATCH_SOURCE_TYPE_READ
 *
 * @discussion A dispatch source that monitors a file descriptor for pending
 * bytes available to be read.
 * The handle is a file descriptor (int).
 * The mask is unused (pass zero for now).
 */

#define RWSCHED_DISPATCH_SOURCE_TYPE_READ (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_READ

/*!
 * @const RWSCHED_DISPATCH_SOURCE_TYPE_SIGNAL
 *
 * @discussion A dispatch source that monitors the current process for signals.
 * The handle is a signal number (int).
 * The mask is unused (pass zero for now).
 */

#define RWSCHED_DISPATCH_SOURCE_TYPE_SIGNAL (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_SIGNAL

/*!
 * @const RWSCHED_DISPATCH_SOURCE_TYPE_TIMER
 *
 * @discussion A dispatch source that submits the event handler block based
 * on a timer.
 * The handle is unused (pass zero for now).
 * The mask is unused (pass zero for now).
 */

#define RWSCHED_DISPATCH_SOURCE_TYPE_TIMER (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_TIMER

/*!
 * @const RWSCHED_DISPATCH_SOURCE_TYPE_VNODE
 *
 * @discussion A dispatch source that monitors a file descriptor for events
 * defined by dispatch_source_vnode_flags_t.
 * The handle is a file descriptor (int).
 * The mask is a mask of desired events from dispatch_source_vnode_flags_t.
 */

#define RWSCHED_DISPATCH_SOURCE_TYPE_VNODE (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_VNODE

/*!
 * @const RWSCHED_DISPATCH_SOURCE_TYPE_WRITE
 *
 * @discussion A dispatch source that monitors a file descriptor for available
 * buffer space to write bytes.
 * The handle is a file descriptor (int).
 * The mask is unused (pass zero for now).
 */

#define RWSCHED_DISPATCH_SOURCE_TYPE_WRITE (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_WRITE

/*!
 * @const DISPATCH_SOURCE_TYPE_DATA_ADD
 * @discussion A dispatch source that coalesces data obtained via calls to
 * dispatch_source_merge_data(). An ADD is used to coalesce the data.
 * The handle is unused (pass zero for now).
 * The mask is unused (pass zero for now).
 */
#define RWSCHED_DISPATCH_SOURCE_TYPE_DATA_ADD (rwsched_dispatch_source_type_t)DISPATCH_SOURCE_TYPE_DATA_ADD


/*!
 * @function dispatch_source_cancel
 *
 * @abstract
 * Asynchronously cancel the dispatch source, preventing any further invocation
 * of its event handler block.
 *
 * @discussion
 * Cancellation prevents any further invocation of the event handler block for
 * the specified dispatch source, but does not interrupt an event handler
 * block that is already in progress.
 *
 * The cancellation handler is submitted to the source's target queue once the
 * the source's event handler has finished, indicating it is now safe to close
 * the source's handle (i.e. file descriptor or mach port).
 *
 * See dispatch_source_set_cancel_handler() for more information.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param source
 * The dispatch source to be canceled.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_source_cancel(rwsched_tasklet_ptr_t tasklet_info,
                               rwsched_dispatch_source_t source);

/*!
 * @function rwsched_dispatch_source_create
 *
 * @abstract
 * Creates a new dispatch source to monitor low-level system objects and auto-
 * matically submit a handler block to a dispatch queue in response to events.
 *
 * @discussion
 * Dispatch sources are not reentrant. Any events received while the dispatch
 * source is suspended or while the event handler block is currently executing
 * will be coalesced and delivered after the dispatch source is resumed or the
 * event handler block has returned.
 *
 * Dispatch sources are created in a suspended state. After creating the
 * source and setting any desired attributes (i.e. the handler, context, etc.),
 * a call must be made to dispatch_resume() in order to begin event delivery.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param type
 * Declares the type of the dispatch source. Must be one of the defined
 * dispatch_source_type_t constants.
 *
 * @param handle
 * The underlying system handle to monitor. The interpretation of this argument
 * is determined by the constant provided in the type parameter.
 *
 * @param mask
 * A mask of flags specifying which events are desired. The interpretation of
 * this argument is determined by the constant provided in the type parameter.
 *
 * @param queue
 * The dispatch queue to which the event handler block will be submitted.
 * If queue is DISPATCH_TARGET_QUEUE_DEFAULT, the source will submit the event
 * handler block to the default priority global queue.
 */

rwsched_dispatch_source_t
rwsched_dispatch_source_create(rwsched_tasklet_ptr_t tasklet_info,
                               rwsched_dispatch_source_type_t type,
                               uintptr_t handle,
                               unsigned long mask,
                               rwsched_dispatch_queue_t queue);

/*!
 * @function rwsched_dispatch_source_set_event_handler_f
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Sets the event handler function for the given dispatch source.
 *
 * @param source
 * The dispatch source to modify.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param handler
 * The event handler function to submit to the source's target queue.
 * The context parameter passed to the event handler function is the current
 * context of the dispatch source at the time the handler call is made.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_source_set_event_handler_f(rwsched_tasklet_ptr_t tasklet_info,
                                            rwsched_dispatch_source_t source,
                                            rwsched_dispatch_function_t handler);

/*!
 * @function rwsched_dispatch_source_set_cancel_handler_f
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Sets the cancel handler function for the given dispatch source.
 *
 * @param source
 * The dispatch source to modify.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param handler
 * The cancel handler function to submit to the source's target queue.
 * The context parameter passed to the event handler function is the current
 * context of the dispatch source at the time the handler call is made.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_source_set_cancel_handler_f(rwsched_tasklet_ptr_t tasklet_info,
                                             rwsched_dispatch_source_t source,
                                             rwsched_dispatch_function_t handler);


/*!
 * @function rwsched_dispatch_source_set_timer
 *
 * @abstract
 * Sets a start time, interval, and leeway value for a timer source.
 *
 * @discussion
 * Calling this function has no effect if the timer source has already been
 * canceled. Once this function returns, any pending timer data accumulated
 * for the previous timer values has been cleared
 *
 * The start time argument also determines which clock will be used for the
 * timer. If the start time is DISPATCH_TIME_NOW or created with
 * dispatch_time() then the timer is based on mach_absolute_time(). Otherwise,
 * if the start time of the timer is created with dispatch_walltime() then the
 * timer is based on gettimeofday(3).
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param start
 * The start time of the timer. See dispatch_time() and dispatch_walltime()
 * for more information.
 *
 * @param interval
 * The nanosecond interval for the timer.
 *
 * @param leeway
 * A hint given to the system by the application for the amount of leeway, in
 * nanoseconds, that the system may defer the timer in order to align with other
 * system activity for improved system performance or power consumption. (For
 * example, an application might perform a periodic task every 5 minutes, with
 * a leeway of up to 30 seconds.)  Note that some latency is to be expected for
 * all timers even when a leeway value of zero is specified.
 */

void
rwsched_dispatch_source_set_timer(rwsched_tasklet_ptr_t tasklet_info,
                                  rwsched_dispatch_source_t source,
                                  rwsched_dispatch_time_t start,
                                  uint64_t interval,
                                  uint64_t leeway);


/*!
 * @function rwsched_dispatch_source_get_data
 *
 * @abstract
 * Returns pending data for the dispatch source.
 *
 * @discussion
 * This function is intended to be called from within the event handler block.
 * The result of calling this function outside of the event handler callback is
 * undefined.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param source
 * The result of passing NULL in this parameter is undefined.
 *
 * @result
 * The return value should be interpreted according to the type of the dispatch
 * source, and may be one of the following:
 *
 *  DISPATCH_SOURCE_TYPE_DATA_ADD:        application defined data
 *  DISPATCH_SOURCE_TYPE_DATA_OR:         application defined data
 *  DISPATCH_SOURCE_TYPE_PROC:            dispatch_source_proc_flags_t
 *  DISPATCH_SOURCE_TYPE_READ:            estimated bytes available to read
 *  DISPATCH_SOURCE_TYPE_SIGNAL:          number of signals delivered since
 *                                            the last handler invocation
 *  DISPATCH_SOURCE_TYPE_TIMER:           number of times the timer has fired
 *                                            since the last handler invocation
 *  DISPATCH_SOURCE_TYPE_VNODE:           dispatch_source_vnode_flags_t
 *  DISPATCH_SOURCE_TYPE_WRITE:           estimated buffer space available
 */
unsigned long
rwsched_dispatch_source_get_data(rwsched_tasklet_ptr_t tasklet_info,
                                 rwsched_dispatch_source_t source);

/*!
 * @function rwsched_dispatch_source_merge_data
 *
 * @abstract
 * Merges data into a dispatch source of type DISPATCH_SOURCE_TYPE_DATA_ADD or
 * DISPATCH_SOURCE_TYPE_DATA_OR and submits its event handler block to its
 * target queue.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param source
 * The result of passing NULL in this parameter is undefined.
 *
 * @param value
 * The value to coalesce with the pending data using a logical OR or an ADD
 * as specified by the dispatch source type. A value of zero has no effect
 * and will not result in the submission of the event handler block.
 */
void
rwsched_dispatch_source_merge_data(rwsched_tasklet_ptr_t tasklet_info,
                                   rwsched_dispatch_source_t source, 
                                   unsigned long value);
/*!
 * @function dispatch_source_release
 *
 * @abstract
 * Creates a new dispatch source
 *
 * @param source
 * source ptr that was returned during queue create
 *
 *
 * @result
 * None
 */
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
rwsched_dispatch_source_release(rwsched_tasklet_ptr_t tasklet_info,
                                rwsched_dispatch_source_t queue);

__END_DECLS

#endif // __RWSCHED_SOURCE_H__
