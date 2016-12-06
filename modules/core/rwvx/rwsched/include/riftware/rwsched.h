
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
 * @file rwsched.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 12/04/2013
 * @brief Top level API include for RW.Sched component
 */

#ifndef __RWSCHED_H__
#define __RWSCHED_H__

#include "rwlib.h"
#include "dispatch/dispatch.h"
#include "dispatch/queue.h"
#include "rw_cf_type_validate.h"

#include "rwsched_gi.h"

__BEGIN_DECLS



typedef struct rwsched_instance_s *rwsched_instance_ptr_t;
typedef struct rwsched_tasklet_s *rwsched_tasklet_ptr_t;

typedef void *rwtrack_object_t;

void rwsched_instance_free(rwsched_instance_t *instance);
void rwsched_tasklet_free(rwsched_tasklet_t *info);

/*!
 * Set or disable the latency reporting threshold for event callbacks.
 *
 * @threshold_ms: Callbacks taking over this many ms to execute will trigger a print.  Set to 0 to disable.
 */
void rwsched_instance_set_latency_check(rwsched_instance_t *instance, int threshold_ms);

/*!
 * Return the currently configured latency reporting threshold for event callbacks.
 *
 * @threshold_ms: Callbacks taking over this many ms to execute will trigger a print.
 */
int rwsched_instance_get_latency_check(rwsched_instance_t *instance);


/*!
 * @typedef rwsched_dispatch_instance_t
 *
 * @abstract
 * rw.sched uses instance handles for all function calls
 * In theory this allows multiple independent schedulers to exist within the same process
 */
RW_CF_TYPE_EXTERN(rwsched_instance_ptr_t);
extern rwsched_instance_ptr_t g_rwsched_instance;

RW_CF_TYPE_EXTERN(rwsched_tasklet_ptr_t);

typedef struct {
  gboolean single_thread;
} rwsched_config_t;

#define RWSCHED_DISPATCH_QUEUE_SERIAL      DISPATCH_QUEUE_SERIAL
#define RWSCHED_DISPATCH_QUEUE_CONCURRENT  DISPATCH_QUEUE_CONCURRENT

struct rwsched_dispatch_struct_header_s {
  dispatch_object_t libdispatch_object;
  void *context;
};

#define rwsched_dispatch_time_t dispatch_time_t

#define RWSCHED_LOG_EVENT(__instance__, __evt__, ...)  \
  RWLOG_EVENT((__instance__)->rwlog_instance, RwSchedLog_notif_##__evt__, __VA_ARGS__)

__END_DECLS

#endif // __RWSCHED_H__
