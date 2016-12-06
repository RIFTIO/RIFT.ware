
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


/*!
 * @file rwsched/cfsocket.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 2/24/2013
 * @brief RW.Sched CFSocket API
 */

#ifndef __RWSCHED_CFSOCKET_H__
#define __RWSCHED_CFSOCKET_H__

#include <glib.h>
#include <glib-object.h>
#include "rwsched.h"
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFSocket.h>

__BEGIN_DECLS

typedef CFSocketCallBackType rwsched_CFSocketCallBackType;
GType rwsched_CFSocketCallBackType_get_type(void);


RWSCHED_CF_DECL(rwsched_CFSocket);
RW_CF_TYPE_EXTERN(rwsched_CFSocketRef);

/// @cond GI_SCANNER
/**
 * rwsched_CFSocketC0allBack:
 * @s:
 * @type:
 * @address:
 * @data:
 * @user_data: (closure)
 */
/// @endcond
typedef void (*rwsched_CFSocketCallBack)(rwsched_CFSocketRef s,
                                         CFSocketCallBackType type,
                                         CFDataRef address,
                                         const void *data,
                                         void *user_data);

CF_EXPORT void rwsched_CFSocketInit(void);

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFSocketGetContext:
 * @tasklet:
 * @s:
 * @context: (out)
 **/
/// @endcond
CF_EXPORT void
rwsched_tasklet_CFSocketGetContext(rwsched_tasklet_t *tasklet,
                                   rwsched_CFSocketRef s,
                                   CFSocketContext *context);

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFSocketCreateWithNative:
 * @tasklet:
 * @allocator: (nullable)
 * @sock:
 * @callBackTypes:
 * @callout: (scope async)
 * @context:
 *
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT rwsched_CFSocketRef
rwsched_tasklet_CFSocketCreateWithNative(rwsched_tasklet_t *tasklet,
                                         CFAllocatorRef allocator,
                                         CFSocketNativeHandle sock,
                                         CFOptionFlags callBackTypes,
                                         rwsched_CFSocketCallBack callout,
                                         const CFSocketContext *context);
/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFSocketBindToNative:
 * @tasklet:
 * @sock:
 * @callBackTypes:
 * @callback: (scope notified)
 * @user_data:
 *
 * Returns: (transfer none)
 **/
/// @endcond
rwsched_CFSocketRef
rwsched_tasklet_CFSocketBindToNative(rwsched_tasklet_t *tasklet,
                                     CFSocketNativeHandle sock,
                                     CFOptionFlags callBackTypes,
                                     rwsched_CFSocketCallBack callback,
                                     gpointer user_data);

CF_EXPORT void
rwsched_tasklet_CFSocketRelease(rwsched_tasklet_t *tasklet,
                        rwsched_CFSocketRef s);

/// @cond GI_SCANNER
/**
 * rwsched_tasklet_CFSocketCreateRunLoopSource:
 * @tasklet:
 * @allocator: (nullable)
 * @s:
 * @order:
 *
 * Returns: (transfer none)
 **/
/// @endcond
CF_EXPORT rwsched_CFRunLoopSourceRef
rwsched_tasklet_CFSocketCreateRunLoopSource(rwsched_tasklet_t *tasklet,
                                            CFAllocatorRef allocator,
                                            rwsched_CFSocketRef s,
                                            CFIndex order);

CF_EXPORT void
rwsched_tasklet_CFSocketReleaseRunLoopSource(rwsched_tasklet_t *tasklet,
                                             rwsched_CFRunLoopSourceRef rl);


CF_EXPORT void
rwsched_tasklet_CFSocketSetSocketFlags(rwsched_tasklet_t *tasklet,
                                       rwsched_CFSocketRef s,
                                       CFOptionFlags flags);

CF_EXPORT void
rwsched_tasklet_CFSocketInvalidate(rwsched_tasklet_t *tasklet,
                                   rwsched_CFSocketRef s);

CF_EXPORT void
rwsched_tasklet_CFSocketEnableCallBacks(rwsched_tasklet_t *tasklet,
                                        rwsched_CFSocketRef s,
                                        CFOptionFlags callBackTypes);

CF_EXPORT void
rwsched_tasklet_CFSocketDisableCallBacks(rwsched_tasklet_t *tasklet,
                                         rwsched_CFSocketRef s,
                                         CFOptionFlags callBackTypes);

__END_DECLS

#endif // __RWSCHED_CFSOCKET_H__
