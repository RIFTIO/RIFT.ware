/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwsched_object.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 1/1/2014
 * @brief API include for rwsched dispatch object functions
 */

#ifndef __RWSCHED_OBJECT_H__
#define __RWSCHED_OBJECT_H__

__BEGIN_DECLS

#include "rwsched.h"

/*!
 * @typedef rwsched_dispatch_object_t
 *
 * @abstract
 * Abstract base type for all dispatch objects.
 * The details of the type definition are language-specific.
 *
 * @discussion
 * Dispatch objects are reference counted via calls to dispatch_retain() and
 * dispatch_release().
 */

struct rwsched_dispatch_object_s {
  struct rwsched_dispatch_struct_header_s header;
};

typedef union {
  struct rwsched_dispatch_object_s *_object;
  struct rwsched_dispatch_queue_s *_dq;
  struct rwsched_dispatch_source_s *_ds;
#if 0
  struct _os_object_s *_os_obj;
  struct dispatch_object_s *_do;
  struct dispatch_continuation_s *_dc;
  struct dispatch_queue_s *_dq;
  struct dispatch_queue_attr_s *_dqa;
  struct dispatch_group_s *_dg;
  struct dispatch_source_s *_ds;
  struct dispatch_source_attr_s *_dsa;
  struct dispatch_semaphore_s *_dsema;
  struct dispatch_data_s *_ddata;
  struct dispatch_io_s *_dchannel;
  struct dispatch_operation_s *_doperation;
  struct dispatch_disk_s *_ddisk;
#endif
} rwsched_dispatch_object_t __attribute__((__transparent_union__));

#ifndef  __cplusplus
/*!
 * @function rwsched_dispatch_retain
 *
 * @abstract
 * Increment the reference count of a dispatch object.
 *
 * @discussion
 * Calls to dispatch_retain() must be balanced with calls to
 * dispatch_release().
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The object to retain.
 * The result of passing NULL in this parameter is undefined.
 */
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
rwsched_dispatch_retain(rwsched_tasklet_ptr_t tasklet_info,
                        rwsched_dispatch_object_t object);


/*!
 * @function rwsched_dispatch_get_context
 *
 * @abstract
 * Returns the application defined context of the object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 *
 * @result
 * The context of the object; may be NULL.
 */

void *
rwsched_dispatch_get_context(rwsched_tasklet_ptr_t tasklet_info,
                             rwsched_dispatch_object_t object);

/*!
 * @function rwsched_dispatch_set_context
 *
 * @abstract
 * Associates an application defined context with the object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The new client defined context for the object. This may be NULL.
 *
 */

void
rwsched_dispatch_set_context(rwsched_tasklet_ptr_t tasklet_info,
                             rwsched_dispatch_object_t object,
                             void *context);

/*!
 * @function rwsched_dispatch_suspend
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Suspends the invocation of blocks on a dispatch object.
 *
 * @discussion
 * A suspended object will not invoke any blocks associated with it. The
 * suspension of an object will occur after any running block associated with
 * the object completes.
 *
 * Calls to dispatch_suspend() must be balanced with calls
 * to dispatch_resume().
 *
 * @param object
 * The object to be suspended.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_suspend(rwsched_tasklet_ptr_t tasklet_info,
                         rwsched_dispatch_object_t object);

/*!
 * @function rwsched_dispatch_resume
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Resumes the invocation of blocks on a dispatch object.
 *
 * @param object
 * The object to be resumed.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_resume(rwsched_tasklet_ptr_t tasklet_info,
                        rwsched_dispatch_object_t object);

#else
/*!
 * @function rwsched_dispatch_retain
 *
 * @abstract
 * Increment the reference count of a dispatch object.
 *
 * @discussion
 * Calls to dispatch_retain() must be balanced with calls to
 * dispatch_release().
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The object to retain.
 * The result of passing NULL in this parameter is undefined.
 */
DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
rwsched_dispatch_retain(rwsched_tasklet_ptr_t tasklet_info,
                        struct rwsched_dispatch_source_s * object);

/*!
 * @function rwsched_dispatch_release
 *
 * @abstract
 * Decrement the reference count of a dispatch object.
 *
 * @discussion
 * A dispatch object is asynchronously deallocated once all references are
 * released (i.e. the reference count becomes zero). The system does not
 * guarantee that a given client is the last or only reference to a given
 * object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The object to release.
 * The result of passing NULL in this parameter is undefined.
 */

DISPATCH_EXPORT DISPATCH_NONNULL_ALL DISPATCH_NOTHROW
void
rwsched_dispatch_release(rwsched_tasklet_ptr_t tasklet_info,
                         void * object);

/*!
 * @function rwsched_dispatch_get_context
 *
 * @abstract
 * Returns the application defined context of the object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 *
 * @result
 * The context of the object; may be NULL.
 */

void *
rwsched_dispatch_get_context(rwsched_tasklet_ptr_t tasklet_info,
                             struct rwsched_dispatch_source_s * object);

/*!
 * @function rwsched_dispatch_set_context
 *
 * @abstract
 * Associates an application defined context with the object.
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @param object
 * The result of passing NULL in this parameter is undefined.
 *
 * @param context
 * The new client defined context for the object. This may be NULL.
 *
 */

void
rwsched_dispatch_set_context(rwsched_tasklet_ptr_t tasklet_info,
                             struct rwsched_dispatch_source_s * object,
                             void *context);

/*!
 * @function rwsched_dispatch_suspend
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Suspends the invocation of blocks on a dispatch object.
 *
 * @discussion
 * A suspended object will not invoke any blocks associated with it. The
 * suspension of an object will occur after any running block associated with
 * the object completes.
 *
 * Calls to dispatch_suspend() must be balanced with calls
 * to dispatch_resume().
 *
 * @param object
 * The object to be suspended.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_suspend(rwsched_tasklet_ptr_t tasklet_info,
                         struct rwsched_dispatch_source_s * object);

/*!
 * @function rwsched_dispatch_resume
 *
 * @param tasklet_info
 * rwsched tasklet_info handle
 *
 * @abstract
 * Resumes the invocation of blocks on a dispatch object.
 *
 * @param object
 * The object to be resumed.
 * The result of passing NULL in this parameter is undefined.
 */

void
rwsched_dispatch_resume(rwsched_tasklet_ptr_t tasklet_info,
                        struct rwsched_dispatch_source_s * object);
#endif
__END_DECLS

#endif // __RWSCHED_OBJECT_H__
