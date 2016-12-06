
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

#include "rwlib.h"
#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_internal.h"
#ifdef _CF_
#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"
#endif // _CF_
#include "rwsched_main.h"
#include "rw-sched-log.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"
#include <ck_pr.h>

__thread void *g_tasklet_info = NULL;

RW_CF_TYPE_DEFINE("RW.Sched Instance Type", rwsched_instance_ptr_t);
RW_CF_TYPE_DEFINE("RW.Sched Tasklet Info Type", rwsched_tasklet_ptr_t);

G_DEFINE_POINTER_TYPE(rwsched_CFRunLoopRef, rwsched_CFRunLoop);
G_DEFINE_POINTER_TYPE(rwsched_CFRunLoopSourceRef, rwsched_CFRunLoopSource);
G_DEFINE_POINTER_TYPE(rwsched_CFRunLoopTimerRef, rwsched_CFRunLoopTimer);
G_DEFINE_POINTER_TYPE(rwsched_CFSocketRef, rwsched_CFSocket);

static void rwsched_instance_free_int(rwsched_instance_t *instance);
static void rwsched_tasklet_free_int(rwsched_tasklet_t *sched_tasklet);
unsigned int g_rwsched_instance_count = 0;
unsigned int g_rwsched_tasklet_count = 0;
rwsched_instance_ptr_t g_rwsched_instance = NULL;

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwsched_instance_get_type()
 */
G_DEFINE_BOXED_TYPE(rwsched_instance_t,
                    rwsched_instance,
                    rwsched_instance_ref,
                    rwsched_instance_unref);

rwsched_instance_t
*rwsched_instance_ref(rwsched_instance_t *instance)
{
  g_atomic_int_inc(&instance->ref_cnt);
  return instance;
}

void
rwsched_instance_unref(rwsched_instance_t *instance)
{
  rwsched_instance_free_int(instance);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwsched_tasklet_get_type()
 */
G_DEFINE_BOXED_TYPE(rwsched_tasklet_t,
                    rwsched_tasklet,
                    rwsched_tasklet_ref,
                    rwsched_tasklet_unref);

rwsched_tasklet_t
*rwsched_tasklet_ref(rwsched_tasklet_t *sched_tasklet)
{
  g_atomic_int_inc(&sched_tasklet->ref_cnt);
  return sched_tasklet;
}

void
rwsched_tasklet_unref(rwsched_tasklet_t *sched_tasklet)
{
  rwsched_tasklet_free_int(sched_tasklet);
}

void
rwsched_instance_set_latency_check(rwsched_instance_t *instance, int threshold_ms) 
{
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT(threshold_ms >= 0);

  instance->latency.check_threshold_ms = threshold_ms;
}

int
rwsched_instance_get_latency_check(rwsched_instance_t *instance) 
{
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  return instance->latency.check_threshold_ms;
}

CFStringRef rwsched_instance_CFRunLoopGetMainMode(rwsched_instance_t *instance)
{
  return instance->main_cfrunloop_mode;
}

void rwsched_instance_CFRunLoopSetMainMode(rwsched_instance_t *instance, 
                                           CFStringRef mode)
{
  return;
}

CFStringRef rwsched_instance_CFRunLoopGetDeferredMode(rwsched_instance_t *instance)
{
  return instance->deferred_cfrunloop_mode;
}

void rwsched_instance_CFRunLoopSetDeferredMode(rwsched_instance_t *instance, 
                                               CFStringRef mode)
{
  return;
}

rwsched_instance_t *
rwsched_instance_new(void)
{
  struct rwsched_instance_s *instance;

  // Allocate memory for the new instance

  // Register the rwsched instance types
  RW_CF_TYPE_REGISTER(rwsched_instance_ptr_t);
  RW_CF_TYPE_REGISTER(rwsched_tasklet_ptr_t);
  rwsched_CFRunLoopInit();
  rwsched_CFSocketInit();

  // Allocate the Master Resource-Tracking Handle
  g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  // Allocate a rwsched instance type and track it
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwsched_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Set the instance configuration
  instance->config.single_thread = TRUE;

  // For now use libdispatch only
  instance->use_libdispatch_only = TRUE;
  // libdispatch_init();

  // Fake up a rwqueue placeholder object to use as the (NULL) DISPATCH_TARGET_QUEUE_DEFAULT
  RW_ASSERT(instance->use_libdispatch_only);
  instance->default_rwqueue = (rwsched_dispatch_queue_t) RW_MALLOC0_TYPE(sizeof(*instance->default_rwqueue), rwsched_dispatch_queue_t);
  RW_ASSERT_TYPE(instance->default_rwqueue, rwsched_dispatch_queue_t);
  instance->default_rwqueue->header.libdispatch_object._dq = DISPATCH_TARGET_QUEUE_DEFAULT;

  // Fake up a rwqueue placeholder object to use as DISPATCH_TARGET_QUEUE_MAIN
  RW_ASSERT(instance->use_libdispatch_only);
  instance->main_rwqueue = (rwsched_dispatch_queue_t) RW_MALLOC0_TYPE(sizeof(*instance->main_rwqueue), rwsched_dispatch_queue_t);
  RW_ASSERT_TYPE(instance->main_rwqueue, rwsched_dispatch_queue_t);
  instance->main_rwqueue->header.libdispatch_object._dq = dispatch_get_main_queue();

  // Fake up rwqueue placeholder objects for the usual four global
  // queues.  The pri values are not 0,1,2,3 or similar, they are
  // -MAX, -2, 0, 2.  We do not support arbitrary pri values, although
  // I think the dispatch API is intended to.
  RW_ASSERT(instance->use_libdispatch_only);
  RW_STATIC_ASSERT(RWSCHED_DISPATCH_QUEUE_GLOBAL_CT == 4);
  static long pris[RWSCHED_DISPATCH_QUEUE_GLOBAL_CT] = {
    DISPATCH_QUEUE_PRIORITY_HIGH,
    DISPATCH_QUEUE_PRIORITY_DEFAULT,
    DISPATCH_QUEUE_PRIORITY_LOW,
    DISPATCH_QUEUE_PRIORITY_BACKGROUND
  };
  int  i; for (i=0; i<RWSCHED_DISPATCH_QUEUE_GLOBAL_CT; i++) {
    instance->global_rwqueue[i].pri = pris[i];
    instance->global_rwqueue[i].rwq = (rwsched_dispatch_queue_t) RW_MALLOC0_TYPE(sizeof(*instance->global_rwqueue[i].rwq), rwsched_dispatch_queue_t);
    RW_ASSERT_TYPE(instance->global_rwqueue[i].rwq, rwsched_dispatch_queue_t);
    instance->global_rwqueue[i].rwq->header.libdispatch_object._dq = dispatch_get_global_queue(pris[i], 0);
    RW_ASSERT(instance->global_rwqueue[i].rwq->header.libdispatch_object._dq);
  }

  instance->main_cfrunloop_mode = kCFRunLoopDefaultMode;
  //instance->main_cfrunloop_mode = CFSTR("TimerMode");
  //instance->deferred_cfrunloop_mode = CFSTR("Deferred Mode");

  // Allocate an array of tasklet pointers and track it
  rwsched_tasklet_t *tasklet = NULL;
  instance->tasklet_array = g_array_sized_new(TRUE, TRUE, sizeof(void *), 256);
  g_array_append_val(instance->tasklet_array, tasklet);

  // Set default latency check = 2 seconds 
  rwsched_instance_set_latency_check(instance, 2000);

  rwsched_instance_ref(instance);
  ck_pr_inc_32(&g_rwsched_instance_count);
  //RW_ASSERT(g_rwsched_instance_count <= 2);
  g_rwsched_instance = instance;


  // Return the instance pointer
  return instance;
}

void
rwsched_instance_free(rwsched_instance_t *instance)
{
  rwsched_instance_free_int(instance);
}

static void
rwsched_instance_free_int(rwsched_instance_t *instance)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  //FIXME
  if (!g_atomic_int_dec_and_test(&instance->ref_cnt)) {
    return;
  }
#if 1
  RW_FREE_TYPE(instance->default_rwqueue, rwsched_dispatch_queue_t);
  RW_FREE_TYPE(instance->main_rwqueue, rwsched_dispatch_queue_t);
  long i;
  for (i=0; i<RWSCHED_DISPATCH_QUEUE_GLOBAL_CT; i++) {
    instance->global_rwqueue[i].rwq->header.libdispatch_object._dq = NULL;
    RW_FREE_TYPE(instance->global_rwqueue[i].rwq, rwsched_dispatch_queue_t);
    instance->global_rwqueue[i].rwq = NULL;
  }

  ck_pr_dec_32(&g_rwsched_instance_count);
  if (instance->rwlog_instance) {
    rwlog_close(instance->rwlog_instance, FALSE);
  }
  //NO-FREE
  RW_CF_TYPE_FREE(instance, rwsched_instance_ptr_t);
#endif
}

rwsched_tasklet_t *
rwsched_tasklet_new(rwsched_instance_t *instance)
{
  rwsched_tasklet_ptr_t sched_tasklet;
  //int i;

  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate memory for the new sched_tasklet sched_tasklet structure and track it
  sched_tasklet = RW_CF_TYPE_MALLOC0(sizeof(*sched_tasklet), rwsched_tasklet_ptr_t);
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);

  rwsched_instance_ref(instance);
  sched_tasklet->instance = instance;
  sched_tasklet->rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("Tasklet Context");

  // Look for an unused entry in tasklet_array (start the indexes at 1 for now)
  int i; for (i = 1 ; i < instance->tasklet_array->len ; i++) {
    if (g_array_index(instance->tasklet_array, rwsched_tasklet_ptr_t, i) == NULL) {
      g_array_index(instance->tasklet_array, rwsched_tasklet_ptr_t, i) = sched_tasklet;
      break;
    }
  }
  if (i >= instance->tasklet_array->len) {
    // Insert a new element at the end of the array
    g_array_append_val(instance->tasklet_array, sched_tasklet);
  }


#ifdef _CF_
  // Allocate an array of cftimer pointers and track it
  rwsched_CFRunLoopTimerRef cftimer = NULL;
  sched_tasklet->cftimer_array = g_array_sized_new(TRUE, TRUE, sizeof(void *), 256);
  g_array_append_val(sched_tasklet->cftimer_array, cftimer);

  // Allocate an array of cfsocket pointers and track it
  rwsched_CFSocketRef cfsocket = NULL;
  sched_tasklet->cfsocket_array = g_array_sized_new(TRUE, TRUE, sizeof(void *), 256);
  g_array_append_val(sched_tasklet->cfsocket_array, cfsocket);

  // Allocate an array of cfsocket pointers and track it
  rwsched_CFRunLoopSourceRef cfsource = NULL;
  sched_tasklet->cfsource_array = g_array_sized_new(TRUE, TRUE, sizeof(void *), 256);
  g_array_append_val(sched_tasklet->cfsource_array, cfsource);

  // Allocate an array of dispatch_what pointers and track it
  rwsched_dispatch_what_ptr_t dispatch_what = NULL;
  sched_tasklet->dispatch_what_array = g_array_sized_new(TRUE, TRUE, sizeof(void *), 256);
  g_array_append_val(sched_tasklet->dispatch_what_array, dispatch_what);
#endif

  rwsched_tasklet_ref(sched_tasklet);
  ck_pr_inc_32(&g_rwsched_tasklet_count);

  // Return the allocated sched_tasklet sched_tasklet structure
  return sched_tasklet;
}

void
rwsched_tasklet_free(rwsched_tasklet_t *sched_tasklet)
{
  rwsched_tasklet_free_int(sched_tasklet);
}

static void
rwsched_tasklet_free_int(rwsched_tasklet_t *sched_tasklet)
{
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  if (!g_atomic_int_dec_and_test(&sched_tasklet->ref_cnt)) {
    return;
  }

  int i;
  for (i = 0 ; i < RWSCHED_MAX_SIGNALS; i++) {
    if (sched_tasklet->signal_dtor[i]) {
      sched_tasklet->signal_dtor[i](sched_tasklet->signal_dtor_ud[i]);
    }
    sched_tasklet->signal_dtor[i]= NULL;
    sched_tasklet->signal_dtor_ud[i]= NULL;
  }

  for (i = 1 ; i < instance->tasklet_array->len ; i++) {
    if (g_array_index(instance->tasklet_array, rwsched_tasklet_ptr_t, i) == sched_tasklet) {
      g_array_remove_index (instance->tasklet_array, i);
      break;
    }
  }

  rwsched_CFRunLoopTimerRef rw_timer;
  while ((rw_timer = g_array_index(sched_tasklet->cftimer_array, rwsched_CFRunLoopTimerRef, 1)) != NULL) {
    RW_CF_TYPE_VALIDATE(rw_timer, rwsched_CFRunLoopTimerRef);
    rwsched_tasklet_CFRunLoopTimerInvalidate(sched_tasklet, rw_timer);
    g_array_remove_index (sched_tasklet->cftimer_array, 1);
  }
  g_array_free(sched_tasklet->cftimer_array, TRUE);

  rwsched_CFSocketRef rw_socket;
  while ((rw_socket = g_array_index(sched_tasklet->cfsocket_array, rwsched_CFSocketRef, 1)) != NULL) {
    RW_CF_TYPE_VALIDATE(rw_socket, rwsched_CFSocketRef);
    rwsched_tasklet_CFSocketRelease(sched_tasklet, rw_socket);
    //g_array_remove_index (sched_tasklet->cfsocket_array, 1);
  }
  g_array_free(sched_tasklet->cfsocket_array, TRUE);

  rwsched_CFRunLoopSourceRef rw_source;
  while ((rw_source = g_array_index(sched_tasklet->cfsource_array, rwsched_CFRunLoopSourceRef, 1)) != NULL) {
    RW_CF_TYPE_VALIDATE(rw_source, rwsched_CFRunLoopSourceRef);
    rwsched_tasklet_CFSocketReleaseRunLoopSource(sched_tasklet, rw_source);
    g_array_remove_index (sched_tasklet->cfsource_array, 1);
  }
  g_array_free(sched_tasklet->cfsource_array, TRUE);

  rwsched_dispatch_what_ptr_t what;
  while ((what = g_array_index(sched_tasklet->dispatch_what_array, rwsched_dispatch_what_ptr_t, 1)) != NULL) {
    RW_FREE_TYPE(what, rwsched_dispatch_what_ptr_t);
    g_array_remove_index (sched_tasklet->dispatch_what_array, 1);
  }
  g_array_free(sched_tasklet->dispatch_what_array, TRUE);

  ck_pr_dec_32(&g_rwsched_tasklet_count);
  //NO-FREE
  RW_CF_TYPE_FREE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_unref(instance);
}

#include <valgrind/valgrind.h> //for checking RUNNING_ON_VALGRIND
void
rwsched_dispatch_main_until(rwsched_tasklet_ptr_t sched_tasklet,
                            double sec,
                            uint32_t *exitnow) {
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  struct timeval start_tv;
  gettimeofday(&start_tv, NULL);
  struct timeval now_tv = start_tv;

  int evfd = dispatch_get_main_queue_eventfd_np();
  RW_ASSERT(evfd >= 0);

  int flags = fcntl(evfd, F_GETFL);
  flags |= O_NONBLOCK;
  fcntl(evfd, F_SETFL, flags);

  while (seconds_elapsed(&start_tv, &now_tv) < sec) {
      if (exitnow && *exitnow) 
        break;

      int secleft = MAX(0, (sec - seconds_elapsed(&start_tv, &now_tv)));
      struct pollfd pfd = { .fd = evfd, .events = POLLIN, .revents = 0 };
      if (0 < poll(&pfd, 1, secleft * 1000)) {
	uint64_t evct = 0;
	int rval = 0;
	rval = read(evfd, &evct, sizeof(evct));
	if (rval == 8) {
	  dispatch_main_queue_drain_np();
	}
      }
      
      gettimeofday(&now_tv, NULL);
  }
}

void
rwsched_dispatch_main_iteration(rwsched_tasklet_ptr_t sched_tasklet) {
 
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_t *instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  dispatch_main_queue_drain_np();
}


rwsched_CFRunLoopTimerRef 
rwsched_tasklet_CFRunLoopTimer(rwsched_tasklet_t *sched_tasklet,
                               CFAbsoluteTime fireDate,
                               CFTimeInterval interval,
                               rwsched_CFRunLoopTimerCallBack callback,
                               gpointer user_data)
{
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  cf_context.info = user_data;
  return rwsched_tasklet_CFRunLoopTimerCreate(sched_tasklet,
                                      kCFAllocatorDefault,
                                      fireDate,
                                      interval,
                                      0,
                                      0,
                                      callback,
                                      &cf_context);

}

rwsched_CFSocketRef
rwsched_tasklet_CFSocketBindToNative(rwsched_tasklet_t *sched_tasklet,
                                     CFSocketNativeHandle sock,
                                     CFOptionFlags callBackTypes,
                                     rwsched_CFSocketCallBack callback,
                                     gpointer user_data)
{
  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  cf_context.info = user_data;
  return rwsched_tasklet_CFSocketCreateWithNative(sched_tasklet,
                                                  kCFAllocatorDefault,
                                                  sock,
                                                  callBackTypes,
                                                  callback,
                                                  &cf_context);
}

void
rwsched_tasklet_get_counters(rwsched_tasklet_t *sched_tasklet,
                             rwsched_tasklet_counters_t *counters)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  RW_ASSERT(counters);
  *counters = sched_tasklet->counters;
}

#if 1
#define _GNU_SOURCE
#include <signal.h>
#include <string.h>

extern rwsched_instance_ptr_t g_rwsched_instance;

static void
rwsched_handler(int signum)
{
  if (signum < 1 || signum > RWSCHED_MAX_SIGNALS-1) {
    return;
  }
  rwsched_instance_ptr_t instance = g_rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);
  RW_ASSERT(instance->signal_required & (1<<signum));
  rwsched_tasklet_t *sched_tasklet = NULL;

  int i; for (i = 1 ; i < instance->tasklet_array->len ; i++) {
    sched_tasklet = g_array_index(instance->tasklet_array, rwsched_tasklet_ptr_t, i);
    RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
    if (sched_tasklet->signal_required & (1<<signum)) {
      RW_ASSERT(sched_tasklet->signal_handler[signum] != NULL);
      (sched_tasklet->signal_handler[signum])(sched_tasklet, signum);
    }
  }
}

int
rwsched_tasklet_signal(
    rwsched_tasklet_t *sched_tasklet,
    int signum,
    rwsched_sighandler_t sighandler,
    void* user_data,
    GDestroyNotify dtor)
{
  // Validate input paraemters
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);
  rwsched_instance_ptr_t instance = sched_tasklet->instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  if (signum < 1 || signum > RWSCHED_MAX_SIGNALS-1) {
    return -1;
  }

  if (!sighandler) {
    sched_tasklet->signal_required &= ~(1<<signum);
    sched_tasklet->signal_handler[signum] = NULL;
    goto done;
  }

  if (!(instance->signal_required & (1<<signum))) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = rwsched_handler;
    if (sigaction(signum, &sa, NULL) == -1) {
      RWSCHED_LOG_EVENT(instance, SchedError, RWLOG_ATTR_SPRINTF("ERROR: rwsched_tasklet_signal(%p signum=%u) FAILED ON sigaction\n", sched_tasklet, signum));
      return -1;
    }
    instance->signal_required |= (1<<signum);
  }
  sched_tasklet->signal_required |= (1<<signum);
  sched_tasklet->signal_handler[signum] = sighandler;
done:
  if (sched_tasklet->signal_dtor[signum]) {
    sched_tasklet->signal_dtor[signum](sched_tasklet->signal_dtor_ud[signum]);
  }
  sched_tasklet->signal_dtor[signum]= dtor;
  sched_tasklet->signal_dtor_ud[signum]= user_data;
  return 0;
}
#endif
