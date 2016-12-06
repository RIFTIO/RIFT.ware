
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
#include "rwsched_main.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"
#include "rwsched_internal.h"
#include <ck_pr.h>

void
rwsched_dispatch_sthread_initialize(rwsched_tasklet_ptr_t sched_tasklet,
                              unsigned int count)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  instance->sthread_initialized = true;
}

rwsched_dispatch_queue_t
rwsched_dispatch_sthread_queue_create(rwsched_tasklet_ptr_t sched_tasklet,
                              const char *label,
                              dispatch_sthread_queue_attr_t attr,
                              dispatch_sthread_closure_t *closure)
{
  struct rwsched_dispatch_queue_s *queue;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);


  // If libdispatch is enabled for the entire instance, then call the libdispatch routine
  if (instance->use_libdispatch_only &&
      instance->sthread_initialized) {
    // Allocate memory for the dispatch queue
    queue = (rwsched_dispatch_queue_t) RW_MALLOC0_TYPE(sizeof(*queue), rwsched_dispatch_queue_t);
    RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

    queue->header.libdispatch_object._dq = dispatch_sthread_queue_create(label,
                                                                         attr,
                                                                         closure);
    RW_ASSERT(queue->header.libdispatch_object._dq);
    queue->is_sthread = true;

    ck_pr_inc_32(&sched_tasklet->counters.sthread_queues);

    rwsched_tasklet_ref(sched_tasklet);

    return queue;
  }
  // Not yet implemented
  RW_CRASH();
  return NULL;
}

int
rwsched_dispatch_sthread_setaffinity(rwsched_tasklet_ptr_t sched_tasklet,
                              rwsched_dispatch_queue_t queue,
                              size_t cpusetsize,
                              cpu_set_t *cpuset)
{
  int rc;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  if (!queue->is_sthread)
    return -1;

  rc = dispatch_sthread_setaffinity(queue->header.libdispatch_object._dq, cpusetsize, cpuset);

  return rc;
}

int
rwsched_dispatch_sthread_getaffinity(rwsched_tasklet_ptr_t sched_tasklet,
                              rwsched_dispatch_queue_t queue,
                              size_t cpusetsize,
                              cpu_set_t *cpuset)
{
  int rc;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);

  if (!queue->is_sthread)
    return -1;

  rc = dispatch_sthread_getaffinity(queue->header.libdispatch_object._dq, cpusetsize, cpuset);

  return rc;
}
