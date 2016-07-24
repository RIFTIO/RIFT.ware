
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include "rwsched_toysched.h"
#include "rwsched_internal.h"
#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"

RW_CF_TYPE_DEFINE("RW.Task sched_tasklet type", rwtoytask_tasklet_ptr_t);
RW_CF_TYPE_DEFINE("RW.Sched toyfd type", rwmsg_toyfd_ptr_t);
RW_CF_TYPE_DEFINE("RW.Sched toytimer type", rwmsg_toytimer_ptr_t);

static void
rwmsg_toysched_io_callback(rwsched_CFSocketRef s,
			   CFSocketCallBackType type,
			   CFDataRef address,
			   const void *data,
			   void *info);

static void
rwmsg_toysched_timer_callback(rwsched_CFRunLoopTimerRef timer, void *info);


void
rwmsg_toysched_init(rwmsg_toysched_t *tsched)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toysched_t *toysched = tsched;

  // Register the rwmsg types
  RW_CF_TYPE_REGISTER(rwtoytask_tasklet_ptr_t);
  RW_CF_TYPE_REGISTER(rwmsg_toyfd_ptr_t);
  RW_CF_TYPE_REGISTER(rwmsg_toytimer_ptr_t);

  // Zero out the toysched structure
  RW_ZERO_VARIABLE(toysched);

  // Create an instance of the toy scheduler
  instance = rwsched_instance_new();
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Store the rwsched instance into the toysched data structure
  toysched->rwsched_instance = instance;
}

void
rwmsg_toysched_deinit(rwmsg_toysched_t *tsched)
{
  rwmsg_toysched_t *toysched = tsched;
  rwsched_instance_ptr_t instance;

  // Get the rwsched instance from toysched
  instance = toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Free the rwsched instance
  rwsched_instance_free(instance);
}

rwmsg_toytask_t *
rwmsg_toytask_create(rwmsg_toysched_t *ts)
{
  rwmsg_toysched_t *toysched = ts;
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask;
  static int toytask_id;
  
  // Get the rwsched instance from toysched
  instance = toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate memory for the new toytask
  toytask = RW_CF_TYPE_MALLOC0(sizeof(*toytask), rwtoytask_tasklet_ptr_t);
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);
  toytask->rwsched_tasklet_info = rwsched_tasklet_new(instance);

  // Save the toysched instance struture within the toytask
  toytask->toysched = ts;

  // Get the next toytask id for this toytask
  toytask_id++;

  // Return the toytask pointer
  return toytask;
}

void
rwmsg_toytask_destroy(rwmsg_toytask_t *toy)
{
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_ptr_t sched_tasklet;
  rwmsg_toytask_t *toytask = toy;
  
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Get the rwsched sched_tasklet from the toytask
  sched_tasklet = toytask->rwsched_tasklet_info;
  RW_CF_TYPE_VALIDATE(sched_tasklet, rwsched_tasklet_ptr_t);

  // Free the rwsched sched_tasklet
  rwsched_tasklet_free(sched_tasklet);

  // Free the task structure
  RW_CF_TYPE_FREE(toytask, rwtoytask_tasklet_ptr_t);
}

uint64_t
rwmsg_toyfd_add(rwmsg_toytask_t *toy, int fd, int pollbits, toyfd_cb_t cb, void *ud)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  //rwsched_dispatch_source_t source;
  struct rwmsg_toyfd_s *toyfd;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a toyfd container type and track it
  toyfd = RW_CF_TYPE_MALLOC0(sizeof(*toyfd), rwmsg_toyfd_ptr_t);
  RW_CF_TYPE_VALIDATE(toyfd, rwmsg_toyfd_ptr_t);
  
  // Example of using CFSocket to manage file descriptors on a runloop using CFStreamCreatePairWithSocket()
  // http://lists.apple.com/archives/macnetworkprog/2003/Jul/msg00075.html
  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = 0;
  CFOptionFlags cf_option_flags = 0;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(toytask->rwsched_tasklet_info);
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;

  // Use the pollbits to determine which socket callback events to register
  if (pollbits & POLLIN) {
    cf_callback_flags |= kCFSocketReadCallBack;
    cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;
  }
  if (pollbits & POLLOUT) {
    cf_callback_flags |= kCFSocketWriteCallBack;
    cf_option_flags |= kCFSocketAutomaticallyReenableWriteCallBack;
  }

  // Create a CFSocket as a runloop source for the toyfd file descriptor
  cf_context.info = toyfd;
  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(toytask->rwsched_tasklet_info,
					      kCFAllocatorSystemDefault,
					      fd,
					      cf_callback_flags,
					      rwmsg_toysched_io_callback,
					      &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);
  rwsched_tasklet_CFSocketSetSocketFlags(toytask->rwsched_tasklet_info, cfsocket, cf_option_flags);
  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(toytask->rwsched_tasklet_info,
						 kCFAllocatorSystemDefault,
						 cfsocket,
						 0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);
  rwsched_tasklet_CFRunLoopAddSource(toytask->rwsched_tasklet_info, runloop, cfsource, instance->main_cfrunloop_mode);

  // Fill in the toyfd structure
  toyfd->cf_ref = cfsocket;
  toyfd->id = cfsocket->index;
  toyfd->fd = fd;
  toyfd->cb = cb;
  toyfd->ud = ud;
  toyfd->toytask = toytask;

  // Fill in the read context structure
  if (pollbits & POLLIN) {
    toyfd->read_context.source.cfsource = cfsource;
  }

  // Fill in the write context structure
  if (pollbits & POLLOUT) {
    toyfd->write_context.source.cfsource = cfsource;
  }

  // Return the callback id
  return (uint64_t) toyfd->id;
}

uint64_t
rwmsg_toyfd_del(rwmsg_toytask_t *toy, uint64_t id)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  rwsched_CFSocketRef cfsocket;
  struct rwmsg_toyfd_s *toyfd;
  rwsched_CFRunLoopSourceRef cfsource = NULL;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Lookup the cfsocket corresponding to id
  RW_ASSERT(id < toytask->rwsched_tasklet_info->cfsocket_array->len);
  cfsocket = g_array_index(toytask->rwsched_tasklet_info->cfsocket_array, rwsched_CFSocketRef, id);
  RW_ASSERT(cfsocket->index == id);
  rwsched_tasklet_CFSocketGetContext(toytask->rwsched_tasklet_info, cfsocket, &cf_context);

  // Free the cfsocket
  rwsched_tasklet_CFSocketInvalidate(toytask->rwsched_tasklet_info, cfsocket);
  rwsched_tasklet_CFSocketRelease(toytask->rwsched_tasklet_info, cfsocket);

  toyfd = cf_context.info;
  // Free the cfsource
  if ((cfsource = toyfd->read_context.source.cfsource)) {
    RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);
    //rwsched_tasklet_CFRunLoopRemoveSource(toytask->rwsched_tasklet_info, cfsource, instance->main_cfrunloop_mode);
    rwsched_tasklet_CFSocketReleaseRunLoopSource(toytask->rwsched_tasklet_info, cfsource);
    toyfd->read_context.source.cfsource = NULL;
  } else if ((cfsource = toyfd->write_context.source.cfsource)) {
    RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);
    //rwsched_tasklet_CFRunLoopRemoveSource(toytask->rwsched_tasklet_info, cfsource, instance->main_cfrunloop_mode);
    rwsched_tasklet_CFSocketReleaseRunLoopSource(toytask->rwsched_tasklet_info, cfsource);
    toyfd->write_context.source.cfsource = NULL;
  }
  // Free the toyfd
  RW_CF_TYPE_FREE(toyfd, rwmsg_toyfd_ptr_t);

  // Return the callback id
  return id;
}

uint64_t
rwmsg_toytimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  struct rwmsg_toytimer_s *toytimer;
  //uint64_t nsec;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a toytimer container type and track it
  toytimer = RW_CF_TYPE_MALLOC0(sizeof(*toytimer), rwmsg_toytimer_ptr_t);
  RW_CF_TYPE_VALIDATE(toytimer, rwmsg_toytimer_ptr_t);
  
  // Create a CF runloop timer
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = ms * .001;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(toytask->rwsched_tasklet_info);
  rwsched_CFRunLoopTimerRef cftimer;

  // Create a CFRunLoopTimer as a runloop source for the toytimer
  cf_context.info = toytimer;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(toytask->rwsched_tasklet_info,
					 kCFAllocatorDefault,
					 CFAbsoluteTimeGetCurrent() + timer_interval,
					 timer_interval,
					 0,
					 0,
					 rwmsg_toysched_timer_callback,
					 &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(toytask->rwsched_tasklet_info, runloop, cftimer, instance->main_cfrunloop_mode);

  // Fill in the toytimer structure
  toytimer->id = cftimer->index;
  toytimer->cb = cb;
  toytimer->ud = ud;

  // Fill in the toytimer context structure
  toytimer->context.toytask = toy;
  toytimer->context.source.cftimer = cftimer;
  toytimer->context.u.toytimer.toytimer = toytimer;

  // Return the callback id
  return (uint64_t) toytimer->id;
}

uint64_t
rwmsg_toytimer_del(rwmsg_toytask_t *toy, uint64_t id)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  rwsched_CFRunLoopTimerRef cftimer;
  struct rwmsg_toytimer_s *toytimer;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Lookup the cftimer corresponding to id
#if 1
  RW_ASSERT(1 < toytask->rwsched_tasklet_info->cftimer_array->len);
  unsigned int i;
  for (i = 1 ; i < toytask->rwsched_tasklet_info->cftimer_array->len ; i++) {
    cftimer = g_array_index(toytask->rwsched_tasklet_info->cftimer_array, rwsched_CFRunLoopTimerRef, i);
    if (id == cftimer->index) {
      break;
    }
  }
  RW_ASSERT(i < toytask->rwsched_tasklet_info->cftimer_array->len);
#else
  RW_ASSERT(id < toytask->rwsched_tasklet_info->cftimer_array->len);
  cftimer = g_array_index(toytask->rwsched_tasklet_info->cftimer_array, rwsched_CFRunLoopTimerRef, id);
  RW_ASSERT(cftimer->index == id);
#endif
  rwsched_tasklet_CFRunLoopTimerGetContext(toytask->rwsched_tasklet_info, cftimer, &cf_context);

  // Invalidate the cftimer
  //rwsched_tasklet_CFRunLoopTimerInvalidate(toytask->rwsched_tasklet_info, cftimer);
  rwsched_tasklet_CFRunLoopTimerRelease(toytask->rwsched_tasklet_info, cftimer);

  // Free the toytimer
  toytimer = cf_context.info;
  RW_CF_TYPE_FREE(toytimer, rwmsg_toytimer_ptr_t);

  // Return the callback id
  return id;
}

uint64_t
rwmsg_toy1Ttimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  struct rwmsg_toytimer_s *toytimer;
  //uint64_t nsec;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a toytimer container type and track it
  toytimer = RW_CF_TYPE_MALLOC0(sizeof(*toytimer), rwmsg_toytimer_ptr_t);
  RW_CF_TYPE_VALIDATE(toytimer, rwmsg_toytimer_ptr_t);
  
  // Create a CF runloop timer
  CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = ms * .001;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(toytask->rwsched_tasklet_info);
  rwsched_CFRunLoopTimerRef cftimer;

  // Create a CFRunLoopTimer as a runloop source for the toytimer
  cf_context.info = toytimer;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(toytask->rwsched_tasklet_info,
					 kCFAllocatorDefault,
					 CFAbsoluteTimeGetCurrent() + timer_interval,
					 0,
					 0,
					 0,
					 rwmsg_toysched_timer_callback,
					 &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(toytask->rwsched_tasklet_info, runloop, cftimer, instance->main_cfrunloop_mode);

  // Fill in the toytimer structure
  toytimer->id = cftimer->index;
  toytimer->cb = cb;
  toytimer->ud = ud;

  // Fill in the toytimer context structure
  toytimer->context.toytask = toy;
  toytimer->context.source.cftimer = cftimer;
  toytimer->context.u.toytimer.toytimer = toytimer;

  // Return the callback id
  return (uint64_t) toytimer->id;
}

void
rwmsg_toysched_run(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toysched_t *toysched = ts;
  //gpointer *pointer;
  //struct rwmsg_event_context_s *event_context;

  // Get the rwsched instance from the toytask
  instance = toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If in blocking mode, BLOWUP since we have no idea which file descriptor to block on with this API
  RW_ASSERT(!tt_blk);

  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 0.010;
  rwsched_instance_CFRunLoopRunInMode(instance, instance->main_cfrunloop_mode, timerLimit, false);
}

void
rwmsg_toysched_runloop(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk, int max_events, double max_time)
{
  UNUSED(max_events);
  rwsched_instance_ptr_t instance;
  rwmsg_toysched_t *toysched = ts;
  //gpointer *pointer;
  //struct rwmsg_event_context_s *event_context;

  // Get the rwsched instance from the toytask
  instance = toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // If in blocking mode, BLOWUP since we have no idea which file descriptor to block on with this API
  RW_ASSERT(!tt_blk);

  // If not in blocking mode, then run for a very short period of time
  rwsched_instance_CFRunLoopRunInMode(instance, instance->main_cfrunloop_mode, max_time, false);
}

int
rwmsg_toytask_block(rwmsg_toytask_t *toy, uint64_t id, int timeout_ms)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  rwsched_CFSocketRef cfsocket;
  struct rwmsg_toyfd_s *toyfd;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Lookup the cfsocket corresponding to id
  RW_ASSERT(id < toytask->rwsched_tasklet_info->cfsocket_array->len);
  cfsocket = g_array_index(toytask->rwsched_tasklet_info->cfsocket_array, rwsched_CFSocketRef, id);
  RW_ASSERT(cfsocket->index == id);
  rwsched_tasklet_CFSocketGetContext(toytask->rwsched_tasklet_info, cfsocket, &cf_context);
  toyfd = cf_context.info;

  rwsched_CFRunLoopSourceRef wakeup_source;
  double secondsToWait = timeout_ms * .001;
  wakeup_source = rwsched_tasklet_CFRunLoopRunTaskletBlock(toytask->rwsched_tasklet_info,
						   cfsocket->callback_context.cfsource,
						   secondsToWait);

  // Set the return value based on which dispatch source woke up on the semaphore
  int revents = 0;
  if (wakeup_source == NULL) {
    revents = 0;
  }
  else if (wakeup_source == toyfd->read_context.source.source) {
    revents = POLLIN;
  }
  else if (wakeup_source == toyfd->write_context.source.source) {
    revents = POLLOUT;
  }
  else {
    // This should never happen as one of the sources should have signaled the semahpore to wake it up
    RW_CRASH();
  }

  // Return the revents value to the caller
  return revents;
}

static void
rwmsg_toysched_io_callback(rwsched_CFSocketRef s,
			   CFSocketCallBackType type,
			   CFDataRef address,
			   const void *data,
			   void *info)
{
  UNUSED(address); UNUSED(data);
  rwsched_instance_ptr_t instance;
  struct rwmsg_toyfd_s *toyfd;
  rwmsg_toytask_t *toytask;
  //static int index = 1;
  int revents = 0;

  // Validate input parameters
  toyfd = (struct rwmsg_toyfd_s *) info;
  RW_CF_TYPE_VALIDATE(toyfd, rwmsg_toyfd_ptr_t);
  toytask = toyfd->toytask;
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Determine whether this callback represents a read or write toyfd event
  if (type == kCFSocketReadCallBack) {
    revents = POLLIN;
  }
  else if (type == kCFSocketWriteCallBack) {
    revents = POLLOUT;
  }
  else {
    RW_CRASH();
  }

  // Invoke the user specified callback
  RW_ASSERT(toyfd->cb);
  toyfd->cb(toyfd->id, toyfd->fd, revents, toyfd->ud);

  // Inalidate the socket so the callback is not invoked again
  rwsched_tasklet_CFSocketInvalidate(toytask->rwsched_tasklet_info, s);
}

static void
rwmsg_toysched_timer_callback(rwsched_CFRunLoopTimerRef timer, void *info)
{
  UNUSED(timer);
  struct rwmsg_toytimer_s *toytimer = (struct rwmsg_toytimer_s *) info;
  rwmsg_toytask_t *toytask;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytimer, rwmsg_toytimer_ptr_t);
  toytask = toytimer->context.toytask;
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Invoke the user specified callback
  RW_ASSERT(toytimer->cb);
  toytimer->cb(toytimer->id, toytimer->ud);

  // Free the timer slot
  // Note: We have a race condition if the timer was deleted but an event is in the runloop_queue
  rwmsg_toytimer_del(toytask, toytimer->id);
}

static void
rwmsg_toysched_Rtimer_callback(rwsched_CFRunLoopTimerRef timer, void *info)
{
  UNUSED(timer);
  struct rwmsg_toytimer_s *toytimer = (struct rwmsg_toytimer_s *) info;
  rwmsg_toytask_t *toytask;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytimer, rwmsg_toytimer_ptr_t);
  toytask = toytimer->context.toytask;
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Invoke the user specified callback
  RW_ASSERT(toytimer->cb);
  toytimer->cb(toytimer->id, toytimer->ud);
}

uint64_t
rwmsg_toyRtimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud)
{
  rwsched_instance_ptr_t instance;
  rwmsg_toytask_t *toytask = toy;
  struct rwmsg_toytimer_s *toytimer;
  //uint64_t nsec;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(toytask, rwtoytask_tasklet_ptr_t);

  // Get the rwsched instance from the toytask
  instance = toytask->toysched->rwsched_instance;
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Allocate a toytimer container type and track it
  toytimer = RW_CF_TYPE_MALLOC0(sizeof(*toytimer), rwmsg_toytimer_ptr_t);
  RW_CF_TYPE_VALIDATE(toytimer, rwmsg_toytimer_ptr_t);
  
  // Create a CF runloop timer
  CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = ms * .001;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(toytask->rwsched_tasklet_info);
  rwsched_CFRunLoopTimerRef cftimer;

  // Create a CFRunLoopTimer as a runloop source for the toytimer
  cf_context.info = toytimer;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(toytask->rwsched_tasklet_info,
					 kCFAllocatorDefault,
					 CFAbsoluteTimeGetCurrent() + timer_interval,
					 timer_interval,
					 0,
					 0,
					 rwmsg_toysched_Rtimer_callback,
					 &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(toytask->rwsched_tasklet_info, runloop, cftimer, instance->main_cfrunloop_mode);

  // Fill in the toytimer structure
  toytimer->id = cftimer->index;
  toytimer->cb = cb;
  toytimer->ud = ud;

  // Fill in the toytimer context structure
  toytimer->context.toytask = toy;
  toytimer->context.source.cftimer = cftimer;
  toytimer->context.u.toytimer.toytimer = toytimer;

  // Return the callback id
  return (uint64_t) toytimer->id;
}
