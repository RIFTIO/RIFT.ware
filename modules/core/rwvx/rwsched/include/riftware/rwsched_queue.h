
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
 */


/**
 * @file rwsched_queue.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 12/04/2013
 * @brief API include for rwsched dispatch queue functions
 */

#ifndef __RWSCHED_QUEUE_H__
#define __RWSCHED_QUEUE_H__

__BEGIN_DECLS

#include "rwsched.h"

/*!
 * @typedef rwsched_dispatch_queue_t
 *
 * @abstract
 * Dispatch queues invoke blocks submitted to them serially in FIFO order. A
 * queue will only invoke one block at a time, but independent queues may each
 * invoke their blocks concurrently with respect to each other.
 *
 * @discussion
 * Dispatch queues are lightweight objects to which blocks may be submitted.
 * The system manages a pool of threads which process dispatch queues and
 * invoke blocks submitted to them.
 *
 * Conceptually a dispatch queue may have its own thread of execution, and
 * interaction between queues is highly asynchronous.
 *
 * Dispatch queues are reference counted via calls to dispatch_retain() and
 * dispatch_release(). Pending blocks submitted to a queue also hold a
 * reference to the queue until they have finished. Once all references to a
 * queue have been released, the queue will be deallocated by the system.
 */

RWSCHED_DISPATCH_DECL(rwsched_dispatch_queue);

/*!
 * @function dispatch_queue_create
 *
 * @abstract
 * Creates a new dispatch queue to which blocks may be submitted.
 *
 * @discussion
 * Dispatch queues created with the DISPATCH_QUEUE_SERIAL or a NULL attribute
 * invoke blocks serially in FIFO order.
 *
 * Dispatch queues created with the DISPATCH_QUEUE_CONCURRENT attribute may
 * invoke blocks concurrently (similarly to the global concurrent queues, but
 * potentially with more overhead), and support barrier blocks submitted with
 * the dispatch barrier API, which e.g. enables the implementation of efficient
 * reader-writer schemes.
 *
 * When a dispatch queue is no longer needed, it should be released with
 * dispatch_release(). Note that any pending blocks submitted to a queue will
 * hold a reference to that queue. Therefore a queue will not be deallocated
 * until all pending blocks have finished.
 *
 * The target queue of a newly created dispatch queue is the default priority
 * global concurrent queue.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param label
 * A string label to attach to the queue.
 * This parameter is optional and may be NULL.
 *
 * @param attr
 * DISPATCH_QUEUE_SERIAL or DISPATCH_QUEUE_CONCURRENT.
 *
 * @result
 * The newly created dispatch queue.
 */

DISPATCH_EXPORT DISPATCH_MALLOC DISPATCH_RETURNS_RETAINED DISPATCH_WARN_RESULT
DISPATCH_NOTHROW
rwsched_dispatch_queue_t
rwsched_dispatch_queue_create(rwsched_tasklet_ptr_t tasklet_info,
                              const char *label,
                              dispatch_queue_attr_t attr);

/*!
 * @function rwsched_dispatch_set_target_queue
 *
 * @abstract
 * Sets the target queue for the given object.
 *
 * @discussion
 * An object's target queue is responsible for processing the object.
 *
 * A dispatch queue's priority is inherited from its target queue. Use the
 * dispatch_get_global_queue() function to obtain suitable target queue
 * of the desired priority.
 *
 * Blocks submitted to a serial queue whose target queue is another serial
 * queue will not be invoked concurrently with blocks submitted to the target
 * queue or to any other queue with that same target queue.
 *
 * The result of introducing a cycle into the hierarchy of target queues is
 * undefined.
 *
 * A dispatch source's target queue specifies where its event handler and
 * cancellation handler blocks will be submitted.
 *
 * A dispatch I/O channel's target queue specifies where where its I/O
 * operations are executed. If the channel's target queue's priority is set to
 * DISPATCH_QUEUE_PRIORITY_BACKGROUND, then the I/O operations performed by
 * dispatch_io_read() or dispatch_io_write() on that queue will be
 * throttled when there is I/O contention.
 *
 * For all other dispatch object types, the only function of the target queue
 * is to determine where an object's finalizer function is invoked.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The object to modify.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param queue
 * The new target queue for the object. The queue is retained, and the
 * previous target queue, if any, is released.
 * If queue is rwsched_dispatch_get_default_queue() (aka
 * DISPATCH_TARGET_QUEUE_DEFAULT), set the object's target queue
 * to the default target queue for the given object type.
 */

#ifndef  __cplusplus
void
rwsched_dispatch_set_target_queue(rwsched_tasklet_ptr_t tasklet_info,
                                  rwsched_dispatch_object_t object,
                                  rwsched_dispatch_queue_t queue);
#else
void
rwsched_dispatch_set_target_queue(rwsched_tasklet_ptr_t tasklet_info,
                                  void *obj,
                                  rwsched_dispatch_queue_t queue);
#endif

/*!
 * @function rwched_dispatch_get_default_queue
 *
 * @abstract
 * Return a handle to the default (NULL) serial queue, equivalent to
 * DISPATCH_TARGET_QUEUE_DEFAULT.  THIS IS NOT ITSELF A VALID QUEUE,
 * it is a placeholder target value for use with _set_target_queue().
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @result
 * Handle to the default queue.
 */
rwsched_dispatch_queue_t
rwsched_dispatch_get_default_queue(rwsched_tasklet_ptr_t tasklet_info);

/*!
 * @function rwched_dispatch_get_main_queue
 *
 * @abstract 
 * Return a handle to the main queue, equivalent to
 * DISPATCH_TARGET_QUEUE_MAIN.  This queue is serial, and notably it
 * runs in the same execution context as the main app thread aka
 * CFRunloop.
 *
 * @param isntance
 * rwsched instance handle
 *
 * @result
 * Handle to the main queue.
 */
rwsched_dispatch_queue_t
rwsched_dispatch_get_main_queue(rwsched_instance_ptr_t instance);

/*!
 * @function rwched_dispatch_get_global_queue
 *
 * @abstract 
 * Return a handle to one of the global concurrent queues.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param priority
 * Priority value, one of DISPATCH_QUEUE_PRIORITY_HIGH, _DEFAULT, _LOW, _BACKGROUND
 *
 * @result
 * Handle to the main queue.
 */
rwsched_dispatch_queue_t
rwsched_dispatch_get_global_queue(rwsched_tasklet_ptr_t tasklet_info,
				  long priority);


/*!
 * @function rwsched_dispatch_main_until
 *
 * @abstract
 * Run the main queue until (time passes or *exitnow==TRUE) and
 * then return.  Intended for test programs.
 *
 * @param sec
 * Run for this long
 *
 * @param exitflag
 * Pointer to a value which will be set when the main queue should be ended
 *
 */
void
rwsched_dispatch_main_until(rwsched_tasklet_ptr_t tasklet_info,
                            double sec,
                            uint32_t *exitnow);

/*!
 * @function rwsched_dispatch_main_iteration
 *
 * @abstract
 * Run the main queue for a single full iteration.  Will return immediately.  
 * Intended for  test programs
 *
 */
void
rwsched_dispatch_main_iteration(rwsched_tasklet_ptr_t tasklet_info);

/*!
 * @function rwsched_dispatch_async_f
 *
 * @abstract
 * Dispatches the task on the provided queue.
 *
 * @param queue
 * Queue to be dispatched
 *
 */
void
rwsched_dispatch_async_f(rwsched_tasklet_ptr_t tasklet_info,
                         rwsched_dispatch_queue_t queue,
                         void *context,
                         dispatch_function_t handler);

/*!
 * @function rwsched_dispatch_sync_f
 *
 * @abstract
 * Dispatches the task on the provided queue.
 *
 * @param queue
 * Queue to be dispatched
 *
 */
void
rwsched_dispatch_sync_f(rwsched_tasklet_ptr_t tasklet_info,
                        rwsched_dispatch_queue_t queue,
                        void *context,
                        dispatch_function_t handler);

/*!
 * @function rwsched_dispatch_sthread_initialize
 *
 * @abstract
 * Initializes static-thread-queues
 *
 * @param count
 * Number of static-thread-queues possible
 *
 */
void
rwsched_dispatch_sthread_initialize(rwsched_tasklet_ptr_t tasklet_info,
                              unsigned int count);

/*!
 * @function rwsched_dispatch_sthread_queue_create
 *
 * @abstract
 * Creates a statit-thread queue
 *
 * @param label
 * A string label to attach to the queue.
 * This parameter is optional and may be NULL.
 *
 * @param attr
 * Tells whether to create the sthread_queue w/ servicing (a pthread is created
 * and serviced) OR the pthread reation and servicing is done by the caller.
 *
 * @param closure
 * When called with DISPATCH_STHREAD_WO_SERVICING, the closure returns the fn &
 * ud
 *
 * @result
 * On success, returns a rwsched_dispatch_queue corresponding to the static-thread created.
 * Sources can be dispatched om to this static-thread queue.
 *
 */
rwsched_dispatch_queue_t
rwsched_dispatch_sthread_queue_create(rwsched_tasklet_ptr_t tasklet_info,
                              const char *label,
                              dispatch_sthread_queue_attr_t attr,
                              dispatch_sthread_closure_t *closure);
/*!
 * @function rwsched_dispatch_sthread_setaffinity
 *
 * @abstract
 * setaffinity
 *
 * @param cpusetsize
 * The argument cpusetsize is the length (in bytes) of the buffer pointed to by
 * cpuset.
 *
 * @param cpuset
 * The cpuset data structure represents a set of CPUs;
 * See http://linux.die.net/man/7/cpuset
 *
 * @result
 * On success, these functions return 0; on error, they return a nonzero error
 * number. See http://linux.die.net/man/3/pthread_setaffinity_np
 *
 */
int
rwsched_dispatch_sthread_setaffinity(rwsched_tasklet_ptr_t tasklet_info,
                              rwsched_dispatch_queue_t queue,
                              size_t cpusetsize,
                              cpu_set_t *cpuset);
/*!
 * @function rwsched_dispatch_sthread_getaffinity
 *
 * @abstract
 * getaffinity
 *
 * @param cpusetsize
 * The argument cpusetsize is the length (in bytes) of the buffer pointed to by
 * cpuset.
 *
 * @param cpuset
 * The cpuset data structure represents a set of CPUs;
 * See http://linux.die.net/man/7/cpuset
 *
 * @result
 * On success, these functions return 0; on error, they return a nonzero error
 * number. See http://linux.die.net/man/3/pthread_setaffinity_np
 *
 */
int
rwsched_dispatch_sthread_getaffinity(rwsched_tasklet_ptr_t tasklet_info,
                              rwsched_dispatch_queue_t queue,
                              size_t cpusetsize,
                              cpu_set_t *cpuset);
#if 1
/*!
 * @function rwsched_dispatch_after_f
 *
 * @abstract
 * Schedule a function for execution on a given queue at a specified time.
 *
 * @discussion
 * See dispatch_after() for details.
 *
 * @param when
 * A temporal milestone returned by dispatch_time() or dispatch_walltime().
 *
 * @param queue
 * A queue to which the given function will be submitted at the specified time.
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The application-defined context parameter to pass to the function.
 *
 * @param work
 * The application-defined function to invoke on the target queue. The first
 * parameter passed to this function is the context provided to
 * dispatch_after_f().
 * The result of passing NULL in this parameter is undefined.
 */
void
rwsched_dispatch_after_f(rwsched_tasklet_ptr_t tasklet_info,
			 dispatch_time_t when,
			 rwsched_dispatch_queue_t queue,
			 void *context,
			 dispatch_function_t work);
#endif


__END_DECLS

#endif // __RWSCHED_QUEUE_H__
