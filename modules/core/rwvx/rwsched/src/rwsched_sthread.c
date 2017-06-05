
/*
 * STANDARD_RIFT_IO_COPYRIGHT
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

    ck_pr_inc_32(&sched_tasklet->counters.ld_sthreads);

    queue->header.sched_tasklet = rwsched_tasklet_ref(sched_tasklet);
    
    return queue;
  }
  // Not yet implemented
  RW_CRASH();
  return NULL;
}



void
rwsched_dispatch_sthread_queue_release(rwsched_tasklet_ptr_t sched_tasklet,
                                       rwsched_dispatch_queue_t queue)
{
  RW_ASSERT(sched_tasklet != NULL);
  RW_ASSERT(queue != NULL);
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT_TYPE(queue, rwsched_dispatch_queue_t);
  
  RW_ASSERT(queue->header.sched_tasklet == sched_tasklet);

  if (instance->use_libdispatch_only) {
    ck_pr_dec_32(&sched_tasklet->counters.ld_sthreads);
    rwsched_dispatch_release(queue->header.sched_tasklet,
                             (rwsched_dispatch_object_t)queue);
    rwsched_tasklet_unref(queue->header.sched_tasklet);
    RW_MAGIC_FREE(queue);
    return;
  }
  RW_ASSERT(0);//not implemented
  return;
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
