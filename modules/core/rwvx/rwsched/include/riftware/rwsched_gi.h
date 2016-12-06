
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
 * @file rwsched_gi.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 02/01/2015
 * @brief GObject introspectable top level API include for RW.Sched component
 */

#ifndef __RWSCHED_GI_H__
#define __RWSCHED_GI_H__

#include <glib.h>
#include <glib-object.h>

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFSocket.h>

__BEGIN_DECLS

#define RWSCHED_DISPATCH_DECL(name) typedef struct name##_s *name##_t
#define RWSCHED_TYPE_DECL(name) typedef struct name##_s *name##_ptr_t, name##_t
#define RWSCHED_CF_DECL(name) typedef struct name##_s *name##Ref

typedef struct __CFRunLoop *rwsched_CFRunLoopRef;
typedef struct __CFRunLoopTimerContext rwsched_CFRunLoopTimerContext;

typedef struct rwsched_instance_s rwsched_instance_t;
rwsched_instance_t *rwsched_instance_new(void);
GType rwsched_instance_get_type(void);
rwsched_instance_t *rwsched_instance_ref(rwsched_instance_t * sched);
void rwsched_instance_unref(rwsched_instance_t * sched);


typedef struct rwsched_tasklet_s rwsched_tasklet_t;
rwsched_tasklet_t *rwsched_tasklet_new(rwsched_instance_t *instance);
GType rwsched_tasklet_get_type(void);
rwsched_tasklet_t *rwsched_tasklet_ref(rwsched_tasklet_t * sched_tasklet);
void rwsched_tasklet_unref(rwsched_tasklet_t * sched_tasklet);

/// @cond GI_SCANNER
/**
 * rwsched_sighandler_t:
 * @sched_tasklet:
 * @signum:
 */
typedef void (*rwsched_sighandler_t)(rwsched_tasklet_t *sched_tasklet, int signum);
/**
 * rwsched_tasklet_signal:
 * @sched_tasklet:
 * @signum:
 * @callback: (scope notified)(nullable)(destroy dtor)
 * @user_data:  (nullable)
 * @dtor:(scope async)(nullable):
 **/
int rwsched_tasklet_signal(rwsched_tasklet_t *sched_tasklet, int signum, rwsched_sighandler_t callback, void* user_data, GDestroyNotify dtor);
/// @endcond

RWSCHED_CF_DECL(rwsched_CFRunLoopSource);
RWSCHED_CF_DECL(rwsched_CFRunLoopTimer);
RWSCHED_CF_DECL(rwsched_CFSocket);
__END_DECLS
#endif // __RWSCHED_GI_H__
