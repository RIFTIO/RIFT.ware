
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

#include <stdio.h>
#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"
#include "../src/rwsched_internal.h"
#include "rwsched_toysched.h"
#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_vx_plugin.h"
#include "async_cb.h"

using ::testing::AtLeast;

#ifdef DEBUG
#define FPRINTF(file, ...) fprintf(file, ##__VA_ARGS__)
#else 
#define FPRINTF(file, ...)
#endif


void
initSuite(int argc, char** argv)
{
  setenv("LIBDISPATCH_DISABLE_KWQ", "1", 1);

  if (argc == 1)
    {
      // Yes this is cheating on argv memory, but oh well...
      argv[1] = (char *) "--gtest_catch_exceptions=0";
      argc++;
    }

  {
    FPRINTF(stderr, "\nMain thread %lu\n",pthread_self());
  }
}
RWUT_INIT(initSuite);

RW_CF_TYPE_DEFINE("RW.Task tasklet type", rwtoytask_tasklet_ptr_t);

TEST(RwSchedUnittest, InstanceAllocAndFree)
{
  // Create an instance of the scheduler
  g_rwsched_tasklet_count = 0;
  g_rwsched_instance_count = 0;
  RWUT_BENCH_ITER(InstanceAllocAndFreeBench, 5, 10000);
    rwsched_instance_ptr_t instance;
    instance = rwsched_instance_new();
    RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

    FPRINTF(stderr, "thread %lu\n", pthread_self());

    // Free the scheduler instance
    rwsched_instance_free(instance);
  RWUT_BENCH_END(InstanceAllocAndFreeBench);

  ASSERT_EQ(g_rwsched_tasklet_count, 0);
  ASSERT_EQ(g_rwsched_instance_count, 0);
}


TEST(RwSchedUnittest, TaskletAllocAndFree)
{
  // Create an instance of the scheduler
  g_rwsched_tasklet_count = 0;
  g_rwsched_instance_count = 0;
  rwsched_instance_ptr_t instance;
  instance = rwsched_instance_new();
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Create a tasklet
  rwsched_tasklet_ptr_t tasklet_info;
  rwtoytask_tasklet_ptr_t task;

  // Register the rwmsg types
  RW_CF_TYPE_REGISTER(rwtoytask_tasklet_ptr_t);

  RWUT_BENCH_ITER(TaskletAllocAndFreeBench, 5, 10000);
    task = (rwtoytask_tasklet_ptr_t) RW_CF_TYPE_MALLOC0(sizeof(*task), rwtoytask_tasklet_ptr_t);
    RW_CF_TYPE_VALIDATE(task, rwtoytask_tasklet_ptr_t);

    tasklet_info = rwsched_tasklet_new(instance);
    RW_CF_TYPE_VALIDATE(tasklet_info, rwsched_tasklet_ptr_t);

    FPRINTF(stderr, "thread %lu\n", pthread_self());

    // Free the tasklet-info
    rwsched_tasklet_free(tasklet_info);
    RW_CF_TYPE_FREE(task, rwtoytask_tasklet_ptr_t);
  RWUT_BENCH_END(TaskletAllocAndFreeBench);

  // Free the scheduler instance
  rwsched_instance_free(instance);

  ASSERT_EQ(g_rwsched_tasklet_count, 0);
  ASSERT_EQ(g_rwsched_instance_count, 0);
}

TEST(RwSchedUnittest, DispatchQueueCreateRelease)
{
  // Create an instance of the scheduler
  g_rwsched_tasklet_count = 0;
  g_rwsched_instance_count = 0;
  rwsched_instance_ptr_t instance;
  instance = rwsched_instance_new();
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  rwtoytask_tasklet_ptr_t task;
  rwsched_tasklet_ptr_t tasklet_info;
  // Register the rwmsg types
  RW_CF_TYPE_REGISTER(rwtoytask_tasklet_ptr_t);
  task = (rwtoytask_tasklet_ptr_t) RW_CF_TYPE_MALLOC0(sizeof(*task), rwtoytask_tasklet_ptr_t);
  RW_CF_TYPE_VALIDATE(task, rwtoytask_tasklet_ptr_t);
  tasklet_info = rwsched_tasklet_new(instance);
  RW_CF_TYPE_VALIDATE(tasklet_info, rwsched_tasklet_ptr_t);

  // Create a dispatch queue
  RWUT_BENCH_ITER(DispatchSourceCreateRelease, 5, 10000);
    rwsched_dispatch_queue_t queue;
    queue = rwsched_dispatch_queue_create(tasklet_info, "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
    EXPECT_TRUE(queue);

    rwsched_dispatch_release(tasklet_info, queue);
  RWUT_BENCH_END(DispatchSourceCreateRelease);

  // Free the tasklet & instance
  rwsched_tasklet_free(tasklet_info);
  rwsched_instance_free(instance);

  ASSERT_EQ(g_rwsched_tasklet_count, 0);
  ASSERT_EQ(g_rwsched_instance_count, 0);
}

TEST(RwSchedUnittest, DispatchSourceCreate1)
{
  // Create an instance of the scheduler
  g_rwsched_tasklet_count = 0;
  g_rwsched_instance_count = 0;
  rwsched_instance_ptr_t instance;
  instance = rwsched_instance_new();
  RW_CF_TYPE_VALIDATE(instance, rwsched_instance_ptr_t);

  // Create an tasklet
  rwtoytask_tasklet_ptr_t task;
  rwsched_tasklet_ptr_t tasklet_info;
  // Register the rwmsg types
  RW_CF_TYPE_REGISTER(rwtoytask_tasklet_ptr_t);
  task = (rwtoytask_tasklet_ptr_t) RW_CF_TYPE_MALLOC0(sizeof(*task), rwtoytask_tasklet_ptr_t);
  RW_CF_TYPE_VALIDATE(task, rwtoytask_tasklet_ptr_t);
  tasklet_info = rwsched_tasklet_new(instance);
  RW_CF_TYPE_VALIDATE(tasklet_info, rwsched_tasklet_ptr_t);

  rwsched_dispatch_queue_t queue;
  queue = rwsched_dispatch_queue_create(tasklet_info, "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
  EXPECT_TRUE(queue);

  // Create a dispatch queue
  RWUT_BENCH_ITER(DispatchSourceCreate1Bench, 5, 10000);
    // Create a dispatch source and bind it to the queue
    rwsched_dispatch_source_t source;
    source = rwsched_dispatch_source_create(tasklet_info,
        RWSCHED_DISPATCH_SOURCE_TYPE_READ,
        fileno(stdin),
        0,
        queue);
    EXPECT_TRUE(source);

    rwsched_dispatch_resume(tasklet_info, source);

    // Cancel the dispatch source
    rwsched_dispatch_source_cancel(tasklet_info, source);
    rwsched_dispatch_release(tasklet_info, source);
  RWUT_BENCH_END(DispatchSourceCreate1Bench);

  // Release queue
  rwsched_dispatch_release(tasklet_info, queue);

  // Free the tasklet & instance
  rwsched_tasklet_free(tasklet_info);
  rwsched_instance_free(instance);

  RW_CF_TYPE_FREE(task, rwtoytask_tasklet_ptr_t);

  ASSERT_EQ(g_rwsched_tasklet_count, 0);
  ASSERT_EQ(g_rwsched_instance_count, 0);
}

rwsched_dispatch_source_t g_source;

int g_pipe_fds[2];

struct disptch_context_s
{
  rwsched_instance_ptr_t instance;
  rwsched_tasklet_ptr_t tasklet_info;
};
typedef struct disptch_context_s *disptch_context_t;

static void
dispatch_io_callback(void *arg)
{
  int rc;
  UNUSED(rc);
  disptch_context_t my_context = (disptch_context_t)arg;
  FPRINTF(stderr, "callback\n");
  FPRINTF(stderr, "thread %lu\n",pthread_self());
#if 1
  // Now read from this pipe
  char buffer[1024];
  FPRINTF(stderr, "callling read\n");

  rc = read(g_pipe_fds[0], buffer, 1);
  FPRINTF(stderr, "rc = %d\n", rc);
#endif
  FPRINTF(stderr, "callling cancel source\n");
  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(my_context->tasklet_info, g_source);
  //dispatch_suspend(g_source->header.libdispatch_object);
}

static void
dispatch_cancel_callback(void *arg)
{
  UNUSED(arg);
  FPRINTF(stderr, "Cancel callback\n");
  FPRINTF(stderr, "thread %lu\n",pthread_self());
  //dispatch_suspend(g_source->header.libdispatch_object);
}

TEST(RwSchedUnittest, DispatchAsync)
{
#if 0
  dispatch_queue_t main_q = dispatch_get_main_queue();
  int i;
  int argc = 1;
  char *argv_mem[2];
  char **argv = argv_mem;
  argv[1] = (char *) "World";

  for (i = 1; i < argc; i++)
    {
      /* Add some work to the main queue. */
      dispatch_async(main_q, ^{ FPRINTF(stderr, "Hello %s!\n", argv[i]); });
    }

  /* Add a last item to the main queue. */
  dispatch_async(main_q, ^{ FPRINTF(stderr, "Goodbye!\n"); });

  /* Start the main queue */
  // dispatch_main();
#endif
}

TEST(RwSchedUnittest, DispatchSourceCreate2)
{
#if 0
  dispatch_queue_t main_q = dispatch_get_main_queue();
  // test_ptr_notnull("main_q", main_q);

  const char *path = "/dev/urandom";

  int fd = open(path, O_RDONLY);

  FPRINTF(stderr, "foo2b()\n");

  dispatch_source_t source = rwsched_dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, fd, 0, main_q);
  // test_ptr_notnull("select source", source);

  dispatch_source_set_event_handler(source, ^{ FPRINTF(stderr, "Hello block!\n"); dispatch_suspend(source); });

  dispatch_resume(source);
#endif
}

/* Test and tasklet environment */
class rwsched_tenv_t {
public:
#define TASKLET_MAX (10)
  rwsched_tenv_t(int tasklet_ct_in=0) {
    g_rwsched_tasklet_count = 0;
    g_rwsched_instance_count = 0;
    assert(tasklet_ct_in <= TASKLET_MAX);
    rwsched = rwsched_instance_new();
    tasklet_ct = tasklet_ct_in;
    memset(&tasklet, 0, sizeof(tasklet));
    for (int i=0; i<tasklet_ct; i++) {
      tasklet[i] = rwsched_tasklet_new(rwsched);
    }
  }
  ~rwsched_tenv_t() {
    if (rwsched) {
      for (int i=0; i<tasklet_ct; i++) {
        rwsched_tasklet_free(tasklet[i]);
        tasklet[i] = NULL;
      }
      rwsched_instance_free(rwsched);
      rwsched = NULL;
    }

    RW_ASSERT(g_rwsched_tasklet_count==0);
    RW_ASSERT(g_rwsched_instance_count==0);
  }

  rwsched_instance_ptr_t rwsched;
  int tasklet_ct;
  rwsched_tasklet_ptr_t tasklet[TASKLET_MAX];
  rwsched_dispatch_queue_t queue;
};

TEST(RwSchedUnittest, DispatchIo)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a pipe to send data through
  int rc;
  rc = pipe(g_pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  rwsched_dispatch_queue_t main_q = rwsched_dispatch_get_main_queue(tenv.rwsched);
  FPRINTF(stderr, "create source for %d\n", g_pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  g_pipe_fds[0],
					  0,
					  main_q);
  EXPECT_TRUE(source);

  g_source = source;

  // Set the context to be passed to the dispatch routines
  disptch_context_t my_context = (disptch_context_t)RW_MALLOC0(sizeof(*my_context));

  my_context->instance = tenv.rwsched;
  my_context->tasklet_info = tenv.tasklet[0];

  rwsched_dispatch_set_context(tenv.tasklet[0], source, (void*)my_context);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback);

  rwsched_dispatch_source_set_cancel_handler_f(tenv.tasklet[0], source, dispatch_cancel_callback);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  // Send data through the pipe
  rc = write(g_pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  FPRINTF(stderr, "calling dispatch_main\n");
  FPRINTF(stderr, "thread %lu\n",pthread_self());

  // dispatch_run_queues();
  // while (1) { dispatch_run_queues(); }
  //  while (1) { dispatch_main(); }
  rwsched_dispatch_main_until(tenv.tasklet[0], 0.002, NULL);

  // Release the dispatch source
  //rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  //rwsched_dispatch_resume(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  // Close any open file handles
  close(g_pipe_fds[0]);
  close(g_pipe_fds[1]);
}

static void
dispatch_signal_callback(void *arg)
{
  int rc;
  UNUSED(rc);
  fprintf(stderr, "callback\n");
  fprintf(stderr, "thread %lu\n",pthread_self());
}

TEST(RwSchedUnittest, DISABLED_SignalSource)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  rwsched_dispatch_queue_t main_q = rwsched_dispatch_get_main_queue(tenv.rwsched);
  signal(SIGHUP, SIG_IGN);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_SIGNAL,
					  SIGHUP,
					  0,
					  main_q);
  EXPECT_TRUE(source);

  g_source = source;

  // Set the context to be passed to the dispatch routines
  //dispatch_set_context(source->header.libdispatch_object,
                       //(void*)instance);

  disptch_context_t my_context = (disptch_context_t)RW_MALLOC0(sizeof(*my_context));

  my_context->instance = tenv.rwsched;
  my_context->tasklet_info = tenv.tasklet[0];

  rwsched_dispatch_set_context(tenv.tasklet[0], source, (void*)my_context);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_signal_callback);
  // dispatch_source_set_event_handler(source, ^{ FPRINTF(stderr, "Hello block (2)!\n"); dispatch_suspend(source); });

  rwsched_dispatch_source_set_cancel_handler_f(tenv.tasklet[0], source, dispatch_cancel_callback);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  // Send SIGHUP
  // int rc;
  //rc = kill(getpid(), SIGHUP);
  //EXPECT_TRUE(rc == 0);

  //  while (1) { dispatch_main(); }
  //rwsched_dispatch_main_until(tenv.tasklet[0], 0.002, NULL);
  rwsched_dispatch_main_until(tenv.tasklet[0], 1, NULL);

  // Cancel the dispatch source
  //rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_resume(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);
}

TEST(RwSchedUnittest, DISABLED_NativeSignalSource)
{

  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  fprintf(stderr, "pid:%u ppid:%u\n", getpid(), getppid());

  // Create a dispatch source and bind it to the queue
  dispatch_source_t source;
  signal(SIGHUP, SIG_IGN);
  source = dispatch_source_create(
      DISPATCH_SOURCE_TYPE_SIGNAL,
      SIGHUP,
      0,
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
  EXPECT_TRUE(source);

  disptch_context_t my_context = (disptch_context_t)RW_MALLOC0(sizeof(*my_context));

  my_context->instance = tenv.rwsched;
  my_context->tasklet_info = tenv.tasklet[0];

  dispatch_set_context(source, (void*)my_context);

  dispatch_source_set_event_handler_f(source, dispatch_signal_callback);

  dispatch_source_set_cancel_handler_f(source, dispatch_cancel_callback);

  dispatch_resume(source);

  // Send SIGHUP
  // int rc;
  //rc = kill(getpid(), SIGHUP);
  //EXPECT_TRUE(rc == 0);

  //  while (1) { dispatch_main(); }
  //rwsched_dispatch_main_until(tenv.tasklet[0], 0.002, NULL);
  //rwsched_dispatch_main_until(tenv.tasklet[0], 20, NULL);
#if 0
  while(1) {
    dispatch_main_queue_drain_np();
    usleep(200);
  }
#endif

  // Cancel the dispatch source
  //rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  //dispatch_resume(source);
  dispatch_release(source);
}
uint32_t g_dispatch_io_callback_happened2 = 0, g_rwsched_io_callback_happened = 0;
uint32_t g_dispatch_io_callback_happened3 = 0;

static void
dispatch_io_callback2(void *arg)
{
  g_dispatch_io_callback_happened2++;
  //dispatch_suspend(g_source->header.libdispatch_object);
}

static void
dispatch_io_callback3(void *arg)
{
  g_dispatch_io_callback_happened3++;
  //dispatch_suspend(g_source->header.libdispatch_object);
}

static void
rwsched_io_callback(rwsched_CFSocketRef s,
                    CFSocketCallBackType type,
                    CFDataRef address,
                    const void *data,
                    void *info)
{
  FPRINTF(stderr, "thread %lu\n",pthread_self());
  g_rwsched_io_callback_happened++;
}

TEST(RwSchedUnittest, DispatchIoWithCFRunLoop)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  rwsched_dispatch_queue_t main_q = rwsched_dispatch_get_main_queue(tenv.rwsched);
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  main_q);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);

  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = 0;
  CFOptionFlags cf_option_flags = 0;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[0]);
  static int info_fd = pipe_fds[0];

  cf_callback_flags |= kCFSocketReadCallBack;
  cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;

  // Create a CFSocket as a runloop source for the toyfd file descriptor
  cf_context.info = (void*)(&info_fd);

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(tenv.tasklet[0],
                                              kCFAllocatorSystemDefault,
                                              pipe_fds[0],
                                              cf_callback_flags,
                                              rwsched_io_callback,
                                              &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(tenv.tasklet[0], cfsocket, cf_option_flags);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(tenv.tasklet[0],
                                                 kCFAllocatorSystemDefault,
                                                 cfsocket,
                                                 0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(tenv.tasklet[0], runloop, cfsource, tenv.rwsched->main_cfrunloop_mode);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 1.000;

  RWUT_BENCH_MS(DispatchIoWithCFRunLoop, 1000);
  for (int i=0; i<100 && !(g_dispatch_io_callback_happened2 && g_rwsched_io_callback_happened); i++)
    rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, tenv.rwsched->main_cfrunloop_mode, timerLimit/20, false);
  RWUT_BENCH_END(DispatchIoWithCFRunLoop);

  if (!RUNNING_ON_VALGRIND) {
    ASSERT_GE(g_dispatch_io_callback_happened2, 1);
    ASSERT_GE(g_rwsched_io_callback_happened, 1);
  }

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  rwsched_tasklet_CFSocketReleaseRunLoopSource(tenv.tasklet[0], cfsource);

  rwsched_tasklet_CFSocketInvalidate(tenv.tasklet[0], cfsocket);

  rwsched_tasklet_CFSocketRelease(tenv.tasklet[0], cfsocket);


  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

TEST(RwSchedUnittest, DispatchIoWithCFRunLoopBlocking)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(2); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  rwsched_dispatch_queue_t main_q = rwsched_dispatch_get_main_queue(tenv.rwsched);
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  main_q);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  //rwsched_dispatch_main_until(tenv.tasklet[0], 0.002, NULL);

  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = 0;
  CFOptionFlags cf_option_flags = 0;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[1]);
  int pipe_fds2[2];
  rc = pipe(pipe_fds2);
  EXPECT_TRUE(rc == 0);
  static int info_fd = pipe_fds2[0];

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source2;
  FPRINTF(stderr, "create source for %d\n", pipe_fds3[0]);
  source2 = rwsched_dispatch_source_create(tenv.tasklet[1],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  main_q);
  EXPECT_TRUE(source2);

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;
  g_dispatch_io_callback_happened3 = 0;
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[1], source2, dispatch_io_callback3);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[1], source2);


  cf_callback_flags |= kCFSocketReadCallBack;
  cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;

  // Create a CFSocket as a runloop source for the toyfd file descriptor
  cf_context.info = (void*)(&info_fd);

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(tenv.tasklet[1],
                                              kCFAllocatorSystemDefault,
                                              pipe_fds2[0],
                                              cf_callback_flags,
                                              rwsched_io_callback,
                                              &cf_context);
  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(tenv.tasklet[1], cfsocket, cf_option_flags);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(tenv.tasklet[1],
                                                 kCFAllocatorSystemDefault,
                                                 cfsocket,
                                                 0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(tenv.tasklet[1], runloop, cfsource, tenv.rwsched->main_cfrunloop_mode);

  double secondsToWait = 5;
  int copy_g_dispatch_io_callback_happened = g_dispatch_io_callback_happened2;
  rwsched_tasklet_CFRunLoopRunTaskletBlock(tenv.tasklet[1],
                                   cfsource,
                                   secondsToWait);
  ASSERT_GT(g_dispatch_io_callback_happened2, copy_g_dispatch_io_callback_happened);
  fprintf(stderr,"g_dispatch_io_callback_happened2=%d, g_dispatch_io_callback_happened3=%d copy_g_dispatch_io_callback_happened=%d\n",
          g_dispatch_io_callback_happened2, g_dispatch_io_callback_happened3, copy_g_dispatch_io_callback_happened);



  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 1.000;

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_source_cancel(tenv.tasklet[1], source2);
  rwsched_dispatch_release(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[1], source2);

  rwsched_tasklet_CFSocketReleaseRunLoopSource(tenv.tasklet[1], cfsource);

  rwsched_tasklet_CFSocketInvalidate(tenv.tasklet[1], cfsocket);

  rwsched_tasklet_CFSocketRelease(tenv.tasklet[1], cfsocket);

#if 1
  //rwsched_tasklet_CFRunLoopRemoveSource(tenv.rwsched, runloop, cfsource, tenv.rwsched->main_cfrunloop_mode);
  int last_dispatch_callback_num = g_dispatch_io_callback_happened2;
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, tenv.rwsched->main_cfrunloop_mode, timerLimit, false);
  ASSERT_EQ(last_dispatch_callback_num, g_dispatch_io_callback_happened2);
#endif

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);
  close(pipe_fds2[0]);
  close(pipe_fds2[1]);
}

TEST(RwSchedUnittest, ExerciseCFRunLoop)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  CFSocketContext cf_context = { 0, NULL, NULL, NULL, NULL };
  CFOptionFlags cf_callback_flags = 0;
  CFOptionFlags cf_option_flags = 0;
  rwsched_CFSocketRef cfsocket;
  rwsched_CFRunLoopSourceRef cfsource;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[0]);
  static int info_fd = pipe_fds[0];

  cf_callback_flags |= kCFSocketReadCallBack;
  cf_option_flags |= kCFSocketAutomaticallyReenableReadCallBack;

  // Create a CFSocket as a runloop source for the toyfd file descriptor
  cf_context.info = (void*)(&info_fd);

  cfsocket = rwsched_tasklet_CFSocketCreateWithNative(tenv.tasklet[0],
                                              kCFAllocatorSystemDefault,
                                              pipe_fds[0],
                                              cf_callback_flags,
                                              rwsched_io_callback,
                                              &cf_context);

  RW_CF_TYPE_VALIDATE(cfsocket, rwsched_CFSocketRef);

  rwsched_tasklet_CFSocketSetSocketFlags(tenv.tasklet[0], cfsocket, cf_option_flags);

  cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(tenv.tasklet[0],
                                                 kCFAllocatorSystemDefault,
                                                 cfsocket,
                                                 0);
  RW_CF_TYPE_VALIDATE(cfsource, rwsched_CFRunLoopSourceRef);

  rwsched_tasklet_CFRunLoopAddSource(tenv.tasklet[0], runloop, cfsource, tenv.rwsched->main_cfrunloop_mode);

  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 1.000;
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, tenv.rwsched->main_cfrunloop_mode, timerLimit, false);

  ASSERT_GT(g_rwsched_io_callback_happened, RUNNING_ON_VALGRIND?0:5);

  //rwsched_tasklet_CFRunLoopRemoveSource(tenv.rwsched, runloop, cfsource, tenv.rwsched->main_cfrunloop_mode);

  rwsched_tasklet_CFSocketReleaseRunLoopSource(tenv.tasklet[0], cfsource);

  // Invalidate the cfsocket
  rwsched_tasklet_CFSocketInvalidate(tenv.tasklet[0], cfsocket);

  // Release the cfsocket
  rwsched_tasklet_CFSocketRelease(tenv.tasklet[0], cfsocket);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);
}

static void
rwsched_timer_callback(rwsched_CFRunLoopTimerRef timer,
                       void *info)
{
  FPRINTF(stderr, "thread %lu\n", pthread_self());
  g_rwsched_io_callback_happened = 1;
}

TEST(RwSchedUnittest, ExerciseCFRunLoopTimer)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_rwsched_io_callback_happened = 0;

  // Create a CF runloop timer
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = 2 /*ms*/ * .001;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[0]);
  rwsched_CFRunLoopTimerRef cftimer;

  // Create a CFRunLoopTimer as a runloop source for the toytimer
  cf_context.info = tenv.rwsched;;
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(tenv.tasklet[0],
                                         kCFAllocatorDefault,
                                         CFAbsoluteTimeGetCurrent() + timer_interval,
                                         timer_interval,
                                         0,
                                         0,
                                         rwsched_timer_callback,
                                         &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(tenv.tasklet[0], runloop, cftimer, tenv.rwsched->main_cfrunloop_mode);

  // Wait a small amount of time for the callbacks to be invoked
  usleep(10 * 1000);

  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 1.000;
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, tenv.rwsched->main_cfrunloop_mode, timerLimit, false);

  ASSERT_EQ(g_rwsched_io_callback_happened, 1);


  // Invalidate the cftimer
  rwsched_tasklet_CFRunLoopTimerInvalidate(tenv.tasklet[0], cftimer);

  // Release the cftimer
  rwsched_tasklet_CFRunLoopTimerRelease(tenv.tasklet[0], cftimer);
}

#define RWSCHEDUNITTEST_NUM_TIMERS 10
rwsched_CFRunLoopTimerRef g_cftimer[RWSCHEDUNITTEST_NUM_TIMERS];

static void
rwsched_timer_callback_two(rwsched_CFRunLoopTimerRef timer, void *info)
{
  fprintf(stderr, "Timer 2 fired\n");
}

static void
rwsched_timer_callback_one(rwsched_CFRunLoopTimerRef timer, void *info)
{
  rwsched_tasklet_ptr_t tasklet = (rwsched_tasklet_ptr_t)info;

  //rwsched_tasklet_CFRunLoopTimerRelease(tasklet, timer);
  for (int i=0; i<RWSCHEDUNITTEST_NUM_TIMERS; i++) {
    rwsched_tasklet_CFRunLoopTimerRelease(tasklet, g_cftimer[i]);
  }
  g_rwsched_io_callback_happened = 1;
}

TEST(RwSchedUnittest, ExerciseCFRunLoopTimerInTimer)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_rwsched_io_callback_happened = 0;

  // Create a CF runloop timer
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  double timer_interval = 5 /*ms*/ * .001;
  rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[0]);
  rwsched_CFRunLoopTimerRef cftimer;

  // Create a CFRunLoopTimer as a runloop source for the toytimer
  cf_context.info = tenv.tasklet[0];
  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(tenv.tasklet[0],
                                         kCFAllocatorDefault,
                                         CFAbsoluteTimeGetCurrent() + 1/*ms*/ * .001,
                                         0,
                                         0,
                                         0,
                                         rwsched_timer_callback_one,
                                         &cf_context);
  RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
  rwsched_tasklet_CFRunLoopAddTimer(tenv.tasklet[0], runloop, cftimer, tenv.rwsched->main_cfrunloop_mode);

  for (int i=0; i<RWSCHEDUNITTEST_NUM_TIMERS; i++) {
    // Create a CFRunLoopTimer as a runloop source for the toytimer
    cf_context.info = tenv.rwsched;;
    g_cftimer[i] = rwsched_tasklet_CFRunLoopTimerCreate(tenv.tasklet[0],
                                           kCFAllocatorDefault,
                                           CFAbsoluteTimeGetCurrent() + timer_interval,
                                           timer_interval,
                                           0,
                                           0,
                                           rwsched_timer_callback_two,
                                           &cf_context);
    RW_CF_TYPE_VALIDATE(cftimer, rwsched_CFRunLoopTimerRef);
    rwsched_tasklet_CFRunLoopAddTimer(tenv.tasklet[0], runloop, cftimer, tenv.rwsched->main_cfrunloop_mode);
  }

  // Wait a small amount of time for the callbacks to be invoked
  usleep(10 * 1000);

  // If not in blocking mode, then run for a very short period of time
  double timerLimit = 1.000;
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, tenv.rwsched->main_cfrunloop_mode, timerLimit, false);

  ASSERT_EQ(g_rwsched_io_callback_happened, 1);


  // Invalidate the cftimer
  rwsched_tasklet_CFRunLoopTimerInvalidate(tenv.tasklet[0], cftimer);

  // Release the cftimer
  rwsched_tasklet_CFRunLoopTimerRelease(tenv.tasklet[0], cftimer);
}

int g_dispatch_timer_callback_count = 0;
static void
dispatch_timer_callback(void *arg)
{
  UNUSED(arg);
  FPRINTF(stderr, "callback\n");
  FPRINTF(stderr, "thread %lu\n",pthread_self());
  g_dispatch_timer_callback_count++;
  dispatch_suspend(g_source->header.libdispatch_object);
}

TEST(RwSchedUnittest, DispatchTimer)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a dispatch source and bind it to the queue
  //struct rwsched_dispatch_source_s * timer;
  rwsched_dispatch_source_t timer;
  rwsched_dispatch_queue_t main_q = rwsched_dispatch_get_main_queue(tenv.rwsched);
  FPRINTF(stderr, "create a timer source\n");
  timer = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
					  0,
					  0,
					  main_q);
  EXPECT_TRUE(timer);

  uint64_t ival = 25/*ms*/*1000;

  rwsched_dispatch_source_set_timer(tenv.tasklet[0], timer, DISPATCH_TIME_NOW, ival, ival/2);

  g_source = timer;
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], timer, dispatch_timer_callback);

  //dispatch_resume(timer->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], timer);

  rwsched_dispatch_main_until(tenv.tasklet[0], 0.2, NULL);

  ASSERT_GE(g_dispatch_timer_callback_count, 1);

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], timer);
  rwsched_dispatch_resume(tenv.tasklet[0], timer);
  rwsched_dispatch_release(tenv.tasklet[0], timer);
}

TEST(RwSchedUnittest, DispatchQueue)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch queue
  rwsched_dispatch_queue_t queue;
  queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
  EXPECT_TRUE(queue);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  queue);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);

  //dispatch_resume(source->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  rwsched_dispatch_async_f(tenv.tasklet[0], queue, NULL, dispatch_io_callback2);

  usleep(10 * 1000);


  ASSERT_GT(g_dispatch_io_callback_happened2, RUNNING_ON_VALGRIND?1:10);

  // Test suspend
  rwsched_dispatch_suspend(tenv.tasklet[0], source);
  usleep(100 * 1000);
  int current_dispatch_cb = g_dispatch_io_callback_happened2;
  usleep(200 * 1000);

  // After suspension, no new events should have fired.
  ASSERT_EQ(g_dispatch_io_callback_happened2, current_dispatch_cb);

  // Cancel the dispatch source
  rwsched_dispatch_resume(tenv.tasklet[0], source);
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  // Release queue
  rwsched_dispatch_release(tenv.tasklet[0], queue);

  //DISABLED RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  //RW_RESOURCE_TRACK_FREE(g_rwresource_track_handle);
}

TEST(RwSchedUnittest, DispatchQueueExample01)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch queue
  rwsched_dispatch_queue_t queue;
  queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
  EXPECT_TRUE(queue);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  queue);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);


  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  rwsched_dispatch_set_target_queue(tenv.tasklet[0],
                                    source,
                                    rwsched_dispatch_get_main_queue(tenv.rwsched));

  //dispatch_resume(timer->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  int i;
  for (i=0; !g_dispatch_io_callback_happened2&&i<50; i++)
    rwsched_dispatch_main_until(tenv.tasklet[0], 0.1, &g_dispatch_io_callback_happened2);

  if (!RUNNING_ON_VALGRIND)
    ASSERT_GE(g_dispatch_io_callback_happened2, 1);

  int num_callbacks = g_dispatch_io_callback_happened2;
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC/10); /* 100ms */
  rwsched_dispatch_after_f(tenv.tasklet[0], when, queue, NULL, dispatch_io_callback2);

  ASSERT_EQ(g_dispatch_io_callback_happened2, num_callbacks);

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  // Release queue
  rwsched_dispatch_release(tenv.tasklet[0], queue);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  usleep(1000000);

}

/**/

TEST(RwSchedUnittest, DispatchQueueExample02)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch queue
  rwsched_dispatch_queue_t queue;
  queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
  EXPECT_TRUE(queue);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  queue);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);

  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  rwsched_dispatch_set_target_queue(tenv.tasklet[0],
                                    source,
                                    rwsched_dispatch_get_main_queue(tenv.rwsched));

  rwsched_dispatch_resume(tenv.tasklet[0], source);

  int i;
  for (i=0; !g_dispatch_io_callback_happened2&&i<50; i++)
    rwsched_dispatch_main_until(tenv.tasklet[0], 0.1, &g_dispatch_io_callback_happened2);

  if (!RUNNING_ON_VALGRIND)
    ASSERT_GE(g_dispatch_io_callback_happened2, 1);

  g_dispatch_io_callback_happened2 = 0;

  rwsched_dispatch_async_f(tenv.tasklet[0], queue, NULL, dispatch_io_callback2);

  if (!RUNNING_ON_VALGRIND)
    ASSERT_GE(g_dispatch_io_callback_happened2, 0);

  rwsched_dispatch_sync_f(tenv.tasklet[0], queue, NULL, dispatch_io_callback2);

  if (!RUNNING_ON_VALGRIND)
    ASSERT_GT(g_dispatch_io_callback_happened2, 0);

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  rwsched_dispatch_release(tenv.tasklet[0], queue);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  usleep(10000);

  //DISABLED RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
}

TEST(RwSchedUnittest, DispatchQueueExample03)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  g_dispatch_io_callback_happened2 = 0;
  g_rwsched_io_callback_happened = 0;

  // Create a pipe to send data through
  int rc, pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);

  // Create a dispatch queue
  rwsched_dispatch_queue_t queue;
  queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", RWSCHED_DISPATCH_QUEUE_SERIAL);
  EXPECT_TRUE(queue);

  // Create a dispatch source and bind it to the queue
  rwsched_dispatch_source_t source;
  FPRINTF(stderr, "create source for %d\n", pipe_fds[0]);
  source = rwsched_dispatch_source_create(tenv.tasklet[0],
					  RWSCHED_DISPATCH_SOURCE_TYPE_READ,
					  pipe_fds[0],
					  0,
					  queue);
  EXPECT_TRUE(source);

  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, dispatch_io_callback2);


  // Send data through the pipe
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  rwsched_dispatch_set_target_queue(tenv.tasklet[0],
                                    source,
                                    rwsched_dispatch_get_main_queue(tenv.rwsched));

  //dispatch_resume(timer->header.libdispatch_object);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  int i;
  for (i=0; !g_dispatch_io_callback_happened2&&i<50; i++)
    rwsched_dispatch_main_until(tenv.tasklet[0], 0.1, &g_dispatch_io_callback_happened2);

  if (!RUNNING_ON_VALGRIND)
    ASSERT_GE(g_dispatch_io_callback_happened2, 1);

  g_dispatch_io_callback_happened2 = 0;
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, NULL, dispatch_io_callback2);
  usleep(10000);

  ASSERT_GE(g_dispatch_io_callback_happened2, 1);
  //ASSERT_EQ(g_rwsched_io_callback_happened, 1);

  // Cancel the dispatch source
  rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  // Release queue
  rwsched_dispatch_release(tenv.tasklet[0], queue);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  usleep(10000);

  //DISABLED RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
}

static void
rw_sthread_dispatch_io_handler(void *ctx)
{
  int rc, fd = *(int*)ctx;
  char buffer[1024];

  rc = read(fd, buffer, 100);
  printf("\nthread %lu rw_sthread_dispatch_io_handler() READING !! rc = %d\n\n", pthread_self(), rc);
}

static void
rw_sthread_dispatch_func(void *ctx)
{
  char *msg = (char*)ctx;
  printf("thread %lu rw_sthread_dispatch_func() Got Dispatched - msg = %s\n", pthread_self(), msg);
}

static void
rw_sthread_dispatch_func_w_sleep1(void *ctx)
{
  char *msg = (char*)ctx;
  printf("thread %lu rw_sthread_dispatch_func_w_sleep1() Got Dispatched - msg = %s\n", pthread_self(), msg);
  usleep(100*1000);
}

static void
rw_sthread_dispatch_func_w_sleep2(void *ctx)
{
  char *msg = (char*)ctx;
  printf("thread %lu rw_sthread_dispatch_func_w_sleep2() Got Dispatched - msg = %s\n", pthread_self(), msg);
  usleep(200*1000);
}

static void
rw_sthread_dispatch_timer_callback(void *ctx)
{
  char *msg = (char*)ctx;
  printf("thread %lu rw_sthread_dispatch_timer_callback() Got Dispatched - msg = %s\n", pthread_self(), msg);
}

TEST(RwSchedUnittest, NativeLibDispatchSThreadTest01)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a pipe to send data through
  int  i;

  dispatch_queue_t queues[10], queue;
#if 1
  for (i=0; i<10; i++) {
    queues[i] = dispatch_sthread_queue_create("TEST",
                                              DISPATCH_STHREAD_W_SERVICING,
                                              NULL);
    EXPECT_TRUE(queues[i]);
    if (i==6) queue = queues[i];
  }
#else
  queue = dispatch_queue_create("test", DISPATCH_QUEUE_SERIAL);
  //queue = dispatch_queue_create("test", DISPATCH_QUEUE_CONCURRENT);
  EXPECT_TRUE(queue);
#endif


  printf("NativeLibDispatchSThreadTest01() MY thread %lu\n",pthread_self());

  dispatch_async_f(queue, (void*)"ASYNC-01", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-02", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-03", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-04", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-05", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---01-05\n", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-06", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-07", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-08", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-09", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-10", rw_sthread_dispatch_func_w_sleep1);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10a", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10b", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10c", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10d\n", rw_sthread_dispatch_func);

  for (i=0; i<10; i++) {
//  dispatch_release(queues[i]);
  }
  usleep(200*1000);
}


TEST(RwSchedUnittest, NativeLibDispatchSThreadTest)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a pipe to send data through
  int rc, i;
  static int  pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  dispatch_queue_t queues[10], queue;
#if 1
  for (i=0; i<10; i++) {
    queues[i] = dispatch_sthread_queue_create("TEST",
                                              DISPATCH_STHREAD_W_SERVICING,
                                              NULL);
    EXPECT_TRUE(queues[i]);
    if (i==6) queue = queues[i];
  }
#else
  queue = dispatch_queue_create("test", DISPATCH_QUEUE_SERIAL);
  //queue = dispatch_queue_create("test", DISPATCH_QUEUE_CONCURRENT);
  EXPECT_TRUE(queue);
#endif


  printf("NativeLibDispatchSThreadTest() MY thread %lu\n",pthread_self());

  dispatch_source_t source;
  source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, pipe_fds[0], 0, queue);
  EXPECT_TRUE(source);
  dispatch_set_context(source, (void*)&pipe_fds[0]);
  dispatch_source_set_event_handler_f(source, rw_sthread_dispatch_io_handler);
  dispatch_resume(source);

  dispatch_async_f(queue, (void*)"ASYNC-01", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-02", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-03", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-04", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-05", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---01-05\n", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-06", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-07", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-08", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-09", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-10", rw_sthread_dispatch_func_w_sleep1);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10\n", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-11", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-12", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-13", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-14", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-15", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---11-15\n", rw_sthread_dispatch_func);

  dispatch_source_t timer;
  timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
  EXPECT_TRUE(timer);
  uint64_t ival = 0.001*NSEC_PER_SEC;
  dispatch_source_set_timer(timer, dispatch_walltime(NULL, ival), ival, ival/2);
  //dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, ival, ival/2);
  printf("\n");
  dispatch_set_context(timer, (void*)"TIMER");
  dispatch_source_set_event_handler_f(timer, rw_sthread_dispatch_timer_callback);
  dispatch_resume(timer);

  usleep(3*1000);

  // Cancel & Release the dispatch sources
  dispatch_source_cancel(timer);
  dispatch_release(timer);
  dispatch_source_cancel(source);
  dispatch_release(source);

  usleep(3*1000);

  for (i=0; i<10; i++) {
//  dispatch_release(queues[i]);
  }
  usleep(200*1000);
}

TEST(RwSchedUnittest, DispatchSThreadCreateFreeTest)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  rwsched_dispatch_queue_t queues[10], queue;
  int i;

  rwsched_dispatch_sthread_initialize(tenv.tasklet[0], 5);
  for (i=0; i<10; i++) {
    queues[i] = rwsched_dispatch_sthread_queue_create(tenv.tasklet[0],
                                                      "STHREAD", DISPATCH_STHREAD_W_SERVICING, NULL);
    EXPECT_TRUE(queues[i]);
    if (i==6) queue = queues[i];
  }

  usleep(5/*ms*/*1000);

  rwsched_dispatch_release(tenv.tasklet[0], queue);
  for (i=0; i<10; i++) {
    if (i!=6)
      rwsched_dispatch_release(tenv.tasklet[0], queues[i]);
  }
  usleep(20*1000);
}

TEST(RwSchedUnittest, DispatchSThreadTest)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a pipe to send data through
  int rc, i;
  static int  pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  rwsched_dispatch_queue_t queues[10], queue;
#if 1
  rwsched_dispatch_sthread_initialize(tenv.tasklet[0], 20);
  for (i=0; i<10; i++) {
    queues[i] = rwsched_dispatch_sthread_queue_create(tenv.tasklet[0],
                                                      "STHREAD-TEST", DISPATCH_STHREAD_W_SERVICING, NULL);
    EXPECT_TRUE(queues[i]);
    if (i==6) queue = queues[i];
  }
#else
  queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", DISPATCH_QUEUE_SERIAL);
  //queue = rwsched_dispatch_queue_create(tenv.tasklet[0], "test", DISPATCH_QUEUE_CONCURRENT);
  EXPECT_TRUE(queue);
#endif


  printf("DispatchSThreadTest() MY thread %lu\n",pthread_self());

  rwsched_dispatch_source_t source;
  source = rwsched_dispatch_source_create(tenv.tasklet[0], RWSCHED_DISPATCH_SOURCE_TYPE_READ, pipe_fds[0], 0, queue);
  EXPECT_TRUE(source);
  rwsched_dispatch_set_context(tenv.tasklet[0], source, (void*)&pipe_fds[0]);
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], source, rw_sthread_dispatch_io_handler);
  rwsched_dispatch_resume(tenv.tasklet[0], source);

  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-01", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-02", rw_sthread_dispatch_func_w_sleep1);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-03", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-04", rw_sthread_dispatch_func_w_sleep2);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-05", rw_sthread_dispatch_func);
  rwsched_dispatch_sync_f(tenv.tasklet[0], queue, (void*)"---SYNC---01-05\n", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-06", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-07", rw_sthread_dispatch_func_w_sleep1);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-08", rw_sthread_dispatch_func_w_sleep2);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-09", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-10", rw_sthread_dispatch_func_w_sleep1);
  rwsched_dispatch_sync_f(tenv.tasklet[0], queue, (void*)"---SYNC---06-10\n", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-11", rw_sthread_dispatch_func_w_sleep1);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-12", rw_sthread_dispatch_func_w_sleep2);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-13", rw_sthread_dispatch_func_w_sleep1);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-14", rw_sthread_dispatch_func);
  rwsched_dispatch_async_f(tenv.tasklet[0], queue, (void*)"ASYNC-15", rw_sthread_dispatch_func);
  rwsched_dispatch_sync_f(tenv.tasklet[0], queue, (void*)"---SYNC---11-15\n", rw_sthread_dispatch_func);

  rwsched_dispatch_source_t timer;
  timer = rwsched_dispatch_source_create(tenv.tasklet[0], RWSCHED_DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
  EXPECT_TRUE(timer);
  uint64_t ival = 0.001*NSEC_PER_SEC;
  rwsched_dispatch_source_set_timer(tenv.tasklet[0], timer, dispatch_walltime(NULL, ival), ival, ival/2);
  //rwsched_dispatch_source_set_timer(tenv.tasklet[0], timer, DISPATCH_TIME_NOW, ival, ival/2);
  printf("\n");
  rwsched_dispatch_set_context(tenv.tasklet[0], timer, (void*)"TIMER");
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[0], timer, rw_sthread_dispatch_timer_callback);
  rwsched_dispatch_resume(tenv.tasklet[0], timer);

  usleep(3*1000);

  // Cancel & Release the dispatch sources
  //rwsched_dispatch_source_cancel(tenv.tasklet[0], timer);
  rwsched_dispatch_release(tenv.tasklet[0], timer);
  //rwsched_dispatch_source_cancel(tenv.tasklet[0], source);
  rwsched_dispatch_release(tenv.tasklet[0], source);

  usleep(3*1000);
  for (i=0; i<10; i++) {
    rwsched_dispatch_release(tenv.tasklet[0], queues[i]);
  }
  usleep(200*1000);
}

TEST(RwSchedUnittest, DISABLED_DispatchSThreadAffinityTest)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up
  int rc;

  rwsched_dispatch_queue_t queue;
  rwsched_dispatch_sthread_initialize(tenv.tasklet[0], 5);
  queue = rwsched_dispatch_sthread_queue_create(tenv.tasklet[0],
                                                "STHREAD", DISPATCH_STHREAD_W_SERVICING, NULL);
  EXPECT_TRUE(queue);

  cpu_set_t cpuset;

#if 0
  CPU_ZERO(&cpuset);
  for (int j = 0; j < 2; j++)
    CPU_SET(j, &cpuset);

  rc = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t), &cpuset);
  EXPECT_TRUE(rc == 0);

  CPU_ZERO(&cpuset);
  rc = pthread_getaffinity_np(pthread_self(),sizeof(cpu_set_t), &cpuset);
  EXPECT_TRUE(rc == 0);
  for (int j = 0; j < 2; j++)
    EXPECT_TRUE(CPU_ISSET(j, &cpuset));
#endif

  CPU_ZERO(&cpuset);
  for (int j = 0; j < 4; j++)
    CPU_SET(j, &cpuset);

  rc = rwsched_dispatch_sthread_setaffinity(tenv.tasklet[0],
                                            queue,
                                            sizeof(cpu_set_t), &cpuset);
  ASSERT_EQ(rc, 0);

  CPU_ZERO(&cpuset);
  rc = rwsched_dispatch_sthread_getaffinity(tenv.tasklet[0],
                                            queue,
                                            sizeof(cpu_set_t), &cpuset);
  ASSERT_EQ(rc, 0);

  for (int j = 0; j < 4; j++)
    EXPECT_TRUE(CPU_ISSET(j, &cpuset));

  usleep(3/*ms*/*1000);
  rwsched_dispatch_release(tenv.tasklet[0], queue);
  usleep(200*1000);
}

#if 1
int _g_dispatch_sthread_my_worker_stop = 0;
static void*
rw_dispatch_sthread_my_worker(void* ud)
{
  dispatch_sthread_closure_t closure = (dispatch_sthread_closure_t)ud;

  RW_ASSERT(ud !=NULL);

  while (!_g_dispatch_sthread_my_worker_stop)
  {
    if (closure->fn) {
      (closure->fn)(closure->ud);
    }
    else {
      break;
    }
    usleep(10);
  }
  return NULL;
}
#else
extern void *
_dispatch_worker_sthread(void *context);
#endif

TEST(RwSchedUnittest, NativeLibDispatchSThreadWOServicingTest)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up

  // Create a pipe to send data through
  int rc;
  static int  pipe_fds[2];
  rc = pipe(pipe_fds);
  EXPECT_TRUE(rc == 0);
  rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(rc == 4);

  dispatch_queue_t queue;
  dispatch_sthread_closure_t closure;

  queue = dispatch_sthread_queue_create("TEST",
                                        DISPATCH_STHREAD_WO_SERVICING,
                                        &closure);
  EXPECT_TRUE(queue);

  _g_dispatch_sthread_my_worker_stop = 0;
  pthread_t pthr;
  rc = pthread_create(&pthr, NULL, rw_dispatch_sthread_my_worker, closure);
  //rc = pthread_detach(pthr);

  printf("NativeLibDispatchSThreadWOServicingTest() MY thread %lu\n",pthread_self());

#if 1
  dispatch_source_t source;
  source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, pipe_fds[0], 0, queue);
  EXPECT_TRUE(source);
  dispatch_set_context(source, (void*)&pipe_fds[0]);
  dispatch_source_set_event_handler_f(source, rw_sthread_dispatch_io_handler);
  dispatch_resume(source);
  usleep(3*1000*100);
  dispatch_source_cancel(source);
  dispatch_release(source);
#endif

#if 1
  dispatch_async_f(queue, (void*)"ASYNC-01", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-02", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-03", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-04", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-05", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---01-05\n", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-06", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-07", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-08", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-09", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-10", rw_sthread_dispatch_func_w_sleep1);
  dispatch_sync_f(queue, (void*)"---SYNC---06-10\n", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-11", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-12", rw_sthread_dispatch_func_w_sleep2);
  dispatch_async_f(queue, (void*)"ASYNC-13", rw_sthread_dispatch_func_w_sleep1);
  dispatch_async_f(queue, (void*)"ASYNC-14", rw_sthread_dispatch_func);
  dispatch_async_f(queue, (void*)"ASYNC-15", rw_sthread_dispatch_func);
  dispatch_sync_f(queue, (void*)"---SYNC---11-15\n", rw_sthread_dispatch_func);
#endif

#if 0
  dispatch_source_t timer;
  timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
  EXPECT_TRUE(timer);
  uint64_t ival = 0.1*NSEC_PER_SEC;
  dispatch_source_set_timer(timer, dispatch_walltime(NULL, ival), ival, ival/2);
  //dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, ival, ival/2);
  printf("\n");
  dispatch_set_context(timer, (void*)"TIMER");
  dispatch_source_set_event_handler_f(timer, rw_sthread_dispatch_timer_callback);
  dispatch_resume(timer);

  usleep(3*1000*100);

  dispatch_source_cancel(timer);
  dispatch_release(timer);
  //dispatch_release(source);
#endif

  closure->fn = NULL;
  _g_dispatch_sthread_my_worker_stop = 1;
  usleep(3*1000*100);
  rc = pthread_cancel(pthr);

  usleep(100);
  dispatch_release(queue);
  usleep(200*1000);
}

/*
 * Create a test fixture for testing async callback framework
 *
 * This fixture is reponsible for:
 *   1) allocating the Virtual Executive (VX)
 *   2) cleanup upon test completion
 */

class RwAsyncCb : public ::testing::Test {
 public:
  static volatile int dispatch_callback_happened;

 protected:
  rw_vx_framework_t *rw_vxfp;
  rw_vx_modinst_common_t *common;
  AsyncCbApi *klass;
  AsyncCbApiIface *interface;
  AsyncCbAsyncFunctorInstance *functor;

  /**
   * This function handles the callback
   */
  static void
  async_callback(AsyncCbAsyncFunctorInstance *functor, void *user_data)
  {
    fprintf(stdout, "Inside async_callback\n");
    dispatch_callback_happened++;

    /* verify that some dummy values are populated */
    ASSERT_EQ(0xdeadbeef, functor->int_variable);
    ASSERT_STREQ("0xbadbeef", functor->string_variable);
    int len;
    gchar **array = async_cb_async_functor_instance_get_string_array(functor, &len);
    ASSERT_EQ(2, len);
    ASSERT_STREQ("0xdeadbeef", array[0]);
    ASSERT_STREQ("0xbadbeef", array[1]);
  }

  /**
   * This is function dispatches the asynchronous callback on
   * to a RWSCHED queue.
   * This is a cookie cutter function and will be standardized in funture
   */
  static void 
  dispatch_callback(AsyncCbAsyncFunctorInstance *functor, void *user_data)
  {
    rwsched_tenv_t *tenv = (rwsched_tenv_t *)functor->context;

    fprintf(stdout, "Inside dispatch_callback\n");
    rwsched_dispatch_async_f(tenv->tasklet[0], 
                             tenv->queue, 
                             functor, 
                             (dispatch_function_t)async_cb_functor_invoke_callback);
  }

  virtual void InitPluginFramework(char *extension) {
    rw_status_t status;

    /* Allocate a vx framework, and setup the repository.
     * This step loads the typelibs */
    rw_vxfp = rw_vx_framework_alloc();
    ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));

    rw_vx_require_repository("RwTypes", "1.0");
    rw_vx_require_repository("AsyncCb", "1.0");

    status = rw_vx_library_open(rw_vxfp, extension, (char *)"", &common);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    /* Allocate functor and get the plugin api and interface */
    functor = (AsyncCbAsyncFunctorInstance *) g_object_new(
                    ASYNC_CB_TYPE_ASYNC_FUNCTOR_INSTANCE, 0);
    status = rw_vx_library_get_api_and_iface(common,
                                             ASYNC_CB_TYPE_API,
                                             (void **) &klass,
                                             (void **) &interface,
                                             NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_TRUE(klass);
    ASSERT_TRUE(interface);

  }

  virtual void TestAsyncRequest() {
    dispatch_callback_happened = 0;

    // Create instances &, tasklets of the scheduler
    rwsched_tenv_t tenv(1); // test env, on stack, dtor cleans up
    tenv.queue = rwsched_dispatch_queue_create(tenv.tasklet[0],
                                               "test",
                                               RWSCHED_DISPATCH_QUEUE_SERIAL);
    EXPECT_TRUE(tenv.queue);

    // setup the functor
    functor->context = &tenv;
    async_cb_async_functor_instance_set__dispatch_method(functor,
                                                         RwAsyncCb::dispatch_callback,
                                                         NULL);
    async_cb_async_functor_instance_set__callback_method(functor,
                                                         RwAsyncCb::async_callback,
                                                         NULL);

    //issue asynchronous request
    interface->async_request(klass, functor);

    // sleep and verify that the function is dispatched
    int i=0;
    for (i=0; i<200 && dispatch_callback_happened == 0; i++) {
      usleep(10 * 1000);
    }
    ASSERT_TRUE(dispatch_callback_happened == 1);
    ASSERT_TRUE(i < 200);		// shouldn't take even remotely 10s of ms, but if we got here it did happen
    rwsched_dispatch_release(tenv.tasklet[0], tenv.queue);
  }

  virtual void TearDown() {
    /* Free the RIFT_VX framework
     * NOTE: This automatically calls de-init for ALL registered modules
     */
    rw_status_t status;
    status = rw_vx_library_close(common, FALSE);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    status = rw_vx_framework_free(rw_vxfp);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
  }
};

class RwAsyncCbCPlugin : public RwAsyncCb {
  virtual void SetUp() {
    InitPluginFramework((char *)"async_cb-c");
  }
};

class RwAsyncCbPythonPlugin : public RwAsyncCb {
  virtual void SetUp() {
    InitPluginFramework((char *)"async_cb-py");
  }
};

volatile int RwAsyncCb::dispatch_callback_happened = 0;

/* The following code illustrates scheduling scheduling 
 * async callbacks from vala interface on RWSCHED.
 * In this case c extension is tested
 */
TEST_F(RwAsyncCbCPlugin, AsyncCallback)
{
  TestAsyncRequest();
}

/* The following code illustrates scheduling scheduling 
 * async callbacks from vala interface on RWSCHED
 * In this case python extension is tested
 */
TEST_F(RwAsyncCbPythonPlugin, AsyncCallback)
{
  TestAsyncRequest();
}


int rwsched_test_SignalHandler_called;
void rwsched_test_SignalHandler(rwsched_tasklet_ptr_t tasklet, int signo)
{
  fprintf(stderr, "rwsched_test_SignalHandler\n");
  ASSERT_EQ(signo, SIGUSR1);
  rwsched_test_SignalHandler_called++;
}

TEST(RwSchedUnittest, SignalHandler)
{
  // Create instances &, tasklets of the scheduler
  rwsched_tenv_t tenv(5); // test env, on stack, dtor cleans up

  rwsched_test_SignalHandler_called = 0;

  rwsched_tasklet_signal(tenv.tasklet[0], SIGUSR1, rwsched_test_SignalHandler,NULL, NULL);
  rwsched_tasklet_signal(tenv.tasklet[4], SIGUSR1, rwsched_test_SignalHandler,NULL, NULL);

  kill(getpid(), SIGUSR1);

  ASSERT_EQ(rwsched_test_SignalHandler_called, 2);


  rwsched_test_SignalHandler_called = 0;

  rwsched_tasklet_signal(tenv.tasklet[0], SIGUSR1, NULL,NULL, NULL);
  rwsched_tasklet_signal(tenv.tasklet[4], SIGUSR1, rwsched_test_SignalHandler,NULL, NULL);

  kill(getpid(), SIGUSR1);

  ASSERT_EQ(rwsched_test_SignalHandler_called, 1);


  rwsched_test_SignalHandler_called = 0;

  rwsched_tasklet_signal(tenv.tasklet[0], SIGUSR1, NULL,NULL, NULL);
  rwsched_tasklet_signal(tenv.tasklet[4], SIGUSR1, NULL,NULL, NULL);

  kill(getpid(), SIGUSR1);

  ASSERT_EQ(rwsched_test_SignalHandler_called, 0);
}
