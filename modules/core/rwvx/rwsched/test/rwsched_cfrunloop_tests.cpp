
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <stdio.h>
#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"
#include "../src/rwsched_internal.h"
#include <valgrind/valgrind.h>

#include "rwut.h"

using ::testing::AtLeast;

//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG(...) fprintf(stderr, ##__VA_ARGS__)
#else 
#define DEBUG(...)
#endif

class CFRunLoopTest : public ::testing::Test {
  protected:
    virtual void SetUp() {
        g_rwsched_tasklet_count = 0;
        g_rwsched_instance_count = 0;
        this->rwsched = rwsched_instance_new();
        this->tasklet = rwsched_tasklet_new(this->rwsched);
        this->runloop =rwsched_tasklet_CFRunLoopGetCurrent(this->tasklet);
    }

    virtual void TearDown() {
        rwsched_tasklet_free(this->tasklet);
        rwsched_instance_free(this->rwsched);

        RW_ASSERT(g_rwsched_tasklet_count==0);
        RW_ASSERT(g_rwsched_instance_count==0);
    }

    void stop_test()
    {
      DEBUG("Stopping test.\n");
      this->stopped = true;
      
      // Re-run the event loop to flush all source events out.
      rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, .100, false);
    }

    bool stopped = false;
    rwsched_instance_ptr_t rwsched;
    rwsched_tasklet_ptr_t tasklet;
    rwsched_CFRunLoopRef runloop;
};


class CFRunLoopTimerTest : public CFRunLoopTest {
  protected:
    void create_timer(rwsched_CFRunLoopTimerCallBack timer_callback, double interval)
    {
      this->interval = interval;
      DEBUG("%s called.\n", __FUNCTION__);
      // Create a CF runloop timer with this object as a context.
      rwsched_CFRunLoopTimerContext cf_context = { 0, this, NULL, NULL, NULL };

      
      this->create_time = CFAbsoluteTimeGetCurrent();
      this->cftimer = rwsched_tasklet_CFRunLoopTimerCreate(this->tasklet,
                                             kCFAllocatorDefault,
                                             this->create_time + this->interval,
                                             this->interval,
                                             0,
                                             0,
                                             timer_callback,
                                             &cf_context);

      RW_CF_TYPE_VALIDATE(this->cftimer, rwsched_CFRunLoopTimerRef);

      rwsched_tasklet_CFRunLoopAddTimer(this->tasklet, this->runloop, this->cftimer, this->rwsched->main_cfrunloop_mode);
      DEBUG("%s finished.\n", __FUNCTION__);
    }

    void destroy_timer(rwsched_CFRunLoopTimerRef timerRef)
    {
      DEBUG("%s called.\n", __FUNCTION__);

      // Release the cftimer
      rwsched_tasklet_CFRunLoopTimerRelease(this->tasklet, timerRef);
      DEBUG("%s finished.\n", __FUNCTION__);
    }

    void set_next_firetime(double new_firetime)
    {
      DEBUG("%s called.\n", __FUNCTION__);

      // SetNextFireDate
      rwsched_tasklet_CFRunLoopTimerSetNextFireDate(this->tasklet, this->cftimer, CFAbsoluteTimeGetCurrent() + new_firetime);

      DEBUG("%s finished.\n", __FUNCTION__);
    }

    static void callback_noaction(rwsched_CFRunLoopTimerRef timerRef, void *info)
    {
      DEBUG("%s called.\n", __FUNCTION__);
      auto myObj = static_cast<CFRunLoopTimerTest *>(info);
      myObj->callbacks_triggered++;

      if (myObj->stopped)
      {
        DEBUG("%s Stopped test.\n", __FUNCTION__);
        myObj->destroy_timer(myObj->cftimer);
      }

      DEBUG("%s finished.\n", __FUNCTION__);
    }

    static void callback_destroy_timer(rwsched_CFRunLoopTimerRef timerRef, void *info)
    {
      DEBUG("%s called.\n", __FUNCTION__);
      auto myObj = static_cast<CFRunLoopTimerTest *>(info);

      myObj->callbacks_triggered++;

      myObj->destroy_timer(myObj->cftimer);
      DEBUG("%s finished.\n", __FUNCTION__);
    }
    
    static void callback_check_interval_destroy_timer(rwsched_CFRunLoopTimerRef timerRef, void *info)
    {
      DEBUG("%s called.\n", __FUNCTION__);
      auto myObj = static_cast<CFRunLoopTimerTest *>(info);

      // Ensure that myObj callback was triggered within 10% of the interval specified
      CFAbsoluteTime curTime = CFAbsoluteTimeGetCurrent();
      CFAbsoluteTime intervalMarginError = .20 * myObj->interval;
      CFAbsoluteTime finishMin = myObj->create_time + myObj->interval - intervalMarginError;
      CFAbsoluteTime finishMax = myObj->create_time + myObj->interval + intervalMarginError;

      DEBUG("curTime: %f\n", curTime);
      DEBUG("finishMin: %f\n", finishMin);
      DEBUG("finishMax: %f\n", finishMax);

      EXPECT_TRUE(curTime <= finishMax);
      EXPECT_TRUE(curTime >= finishMin);

      myObj->callbacks_triggered++;

      myObj->destroy_timer(myObj->cftimer);
      DEBUG("%s finished.\n", __FUNCTION__);
    }

    static void callback_destroy_recreate_timer(rwsched_CFRunLoopTimerRef timerRef, void *info)
    {
      DEBUG("%s called.\n", __FUNCTION__);
      auto myObj = static_cast<CFRunLoopTimerTest *>(info);

      myObj->callbacks_triggered++;
      myObj->destroy_timer(myObj->cftimer);

      if (! myObj->stopped)
      {
        DEBUG("%s Stopped test.\n", __FUNCTION__);
        myObj->create_timer(myObj->callback_destroy_recreate_timer, myObj->interval);
      }

      DEBUG("%s finished.\n", __FUNCTION__);
    }

    double interval = 0;
    int callbacks_triggered = 0;
    rwsched_CFRunLoopTimerRef cftimer = NULL;
    CFAbsoluteTime create_time = 0;
};


TEST_F(CFRunLoopTimerTest, TimerSingleCreateDestroy)
{

  this->create_timer(this->callback_destroy_timer, .01);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

// Double distroy invocation is not yet supported properly
TEST_F(CFRunLoopTimerTest, DISABLED_TimerDoubleDestroy)
{
  this->create_timer(this->callback_destroy_timer, .01);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  //ASSERT_DEATH(this->destroy_timer(this->cftimer), "is not a CF object");
  ASSERT_DEATH(this->destroy_timer(this->cftimer), "");

  ASSERT_EQ(this->callbacks_triggered, 1);
}

TEST_F(CFRunLoopTimerTest, TimerContinuousInterval)
{
  this->create_timer(this->callback_noaction, .00001);
  RWUT_BENCH_MS(TimerContinuousIntervalBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(TimerContinuousIntervalBench);
  RWUT_BENCH_RECORD_PROPERTY(TimerContinuousIntervalBench, callbacks_triggered, this->callbacks_triggered);

  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:1000);
}

TEST_F(CFRunLoopTimerTest, TimerContinuousCreateDestroy)
{
  this->create_timer(this->callback_destroy_recreate_timer, .00001);
  RWUT_BENCH_MS(TimerContinuousCreateDestroyBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(TimerContinuousCreateDestroyBench);
  RWUT_BENCH_RECORD_PROPERTY(TimerContinuousCreateDestroyBench, callbacks_triggered, this->callbacks_triggered);

  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:1000);
}


TEST_F(CFRunLoopTimerTest, TimerDeadlineTest)
{
  this->create_timer(this->callback_check_interval_destroy_timer, .011);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, .200, false);
  ASSERT_GE(this->callbacks_triggered, 1);

  this->create_timer(this->callback_check_interval_destroy_timer, .022);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, .500, false);
  ASSERT_GE(this->callbacks_triggered, 2);

  this->create_timer(this->callback_check_interval_destroy_timer, .055);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, .705, false);
  ASSERT_GE(this->callbacks_triggered, 3);
}

TEST_F(CFRunLoopTimerTest, TimerCreateSetNextFireNdDestroy)
{

  this->create_timer(this->callback_destroy_timer, 1.5);
  this->set_next_firetime(.05);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )



class CFRunLoopDescriptorTest : public CFRunLoopTest {
  protected:
    void create_read_source(rwsched_CFSocketCallBack read_callback)
    {
      // Create a pipe to send data through
      int rc = pipe(this->pipe_fds);
      EXPECT_TRUE(rc == 0);

      // Send data through the pipe
      rc = write(this->pipe_fds[1], "abcd", 4);
      EXPECT_TRUE(rc == 4);

      CFSocketContext cf_context = { 0, this, NULL, NULL, NULL };
      CFOptionFlags cf_callback_flags = kCFSocketReadCallBack;
      CFOptionFlags cf_option_flags = kCFSocketAutomaticallyReenableReadCallBack;

      rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(this->tasklet);
      this->cfsocket = rwsched_tasklet_CFSocketCreateWithNative(this->tasklet,
                                                        kCFAllocatorSystemDefault,
                                                        this->pipe_fds[0],
                                                        cf_callback_flags,
                                                        read_callback,
                                                        &cf_context);

      RW_CF_TYPE_VALIDATE(this->cfsocket, rwsched_CFSocketRef);

      rwsched_tasklet_CFSocketSetSocketFlags(this->tasklet, this->cfsocket, cf_option_flags);

      this->cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(this->tasklet,
                                                           kCFAllocatorSystemDefault,
                                                           this->cfsocket, 0);

      RW_CF_TYPE_VALIDATE(this->cfsource, rwsched_CFRunLoopSourceRef);

      rwsched_tasklet_CFRunLoopAddSource(this->tasklet, runloop, this->cfsource, this->rwsched->main_cfrunloop_mode);
    }

    void create_inotify_source(rwsched_CFSocketCallBack read_callback)
    {
      // Create a pipe to send data through
      this->inotify_fd = inotify_init();
      EXPECT_TRUE(this->inotify_fd >= 0);

      srandom(time(NULL));
      int r = asprintf(&this->inotify_tmp_fname, "/tmp/inotify_%ld", random());
      RW_ASSERT(r!=-1);
      fprintf(stderr, "inotify_tmp_file=%s\n", this->inotify_tmp_fname);

      int fd = creat(this->inotify_tmp_fname, 0777);
      RW_ASSERT(fd>0);
      close(fd);

      int wd = inotify_add_watch(this->inotify_fd,
                                 this->inotify_tmp_fname,
                                 IN_MODIFY | IN_DELETE );
      EXPECT_TRUE(wd >= 0);

      CFSocketContext cf_context = { 0, this, NULL, NULL, NULL };
      CFOptionFlags cf_callback_flags = kCFSocketReadCallBack;
      CFOptionFlags cf_option_flags = kCFSocketAutomaticallyReenableReadCallBack;

      rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(this->tasklet);
      this->cfsocket = rwsched_tasklet_CFSocketCreateWithNative(this->tasklet,
                                                        kCFAllocatorSystemDefault,
                                                        this->inotify_fd,
                                                        cf_callback_flags,
                                                        read_callback,
                                                        &cf_context);

      RW_CF_TYPE_VALIDATE(this->cfsocket, rwsched_CFSocketRef);

      rwsched_tasklet_CFSocketSetSocketFlags(this->tasklet, this->cfsocket, cf_option_flags);

      this->cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(this->tasklet,
                                                           kCFAllocatorSystemDefault,
                                                           this->cfsocket, 0);

      RW_CF_TYPE_VALIDATE(this->cfsource, rwsched_CFRunLoopSourceRef);

      rwsched_tasklet_CFRunLoopAddSource(this->tasklet, runloop, this->cfsource, this->rwsched->main_cfrunloop_mode);
    }

    void create_write_source(rwsched_CFSocketCallBack write_callback)
    {
      // Create a pipe to send data through
      int rc = pipe(this->pipe_fds);
      EXPECT_TRUE(rc == 0);

      CFSocketContext cf_context = { 0, this, NULL, NULL, NULL };
      CFOptionFlags cf_callback_flags = kCFSocketWriteCallBack;
      CFOptionFlags cf_option_flags = kCFSocketAutomaticallyReenableWriteCallBack;

      rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(this->tasklet);
      this->cfsocket = rwsched_tasklet_CFSocketCreateWithNative(this->tasklet,
                                                        kCFAllocatorSystemDefault,
                                                        this->pipe_fds[1],
                                                        cf_callback_flags,
                                                        write_callback,
                                                        &cf_context);

      RW_CF_TYPE_VALIDATE(this->cfsocket, rwsched_CFSocketRef);

      rwsched_tasklet_CFSocketSetSocketFlags(this->tasklet, this->cfsocket, cf_option_flags);

      this->cfsource = rwsched_tasklet_CFSocketCreateRunLoopSource(this->tasklet,
                                                           kCFAllocatorSystemDefault,
                                                           this->cfsocket, 0);

      RW_CF_TYPE_VALIDATE(this->cfsource, rwsched_CFRunLoopSourceRef);

      rwsched_tasklet_CFRunLoopAddSource(this->tasklet, runloop, this->cfsource, this->rwsched->main_cfrunloop_mode);

    }

    void destroy_source()
    {
      rwsched_tasklet_CFSocketReleaseRunLoopSource(this->tasklet, this->cfsource);

      // Invalidate the cfsocket
      rwsched_tasklet_CFSocketInvalidate(this->tasklet, this->cfsocket);

      // Release the cfsocket
      rwsched_tasklet_CFSocketRelease(this->tasklet, this->cfsocket);

      close(this->pipe_fds[0]);
      close(this->pipe_fds[1]);
    }

    static void single_read_callback(rwsched_CFSocketRef s,
                                     CFSocketCallBackType type,
                                     CFDataRef address,
                                     const void *data,
                                     void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;

      uint8_t buffer[5];
      int rc = read(myObj->pipe_fds[0], buffer, 4);
      EXPECT_EQ(rc, 4);

      buffer[4] = '\0';
      EXPECT_STREQ((char *)buffer, "abcd");

      myObj->destroy_source();
    }

    static void single_inotify_callback(rwsched_CFSocketRef s,
                                     CFSocketCallBackType type,
                                     CFDataRef address,
                                     const void *data,
                                     void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;

      uint8_t buffer[BUF_LEN];
      int rc = read(myObj->inotify_fd, buffer, BUF_LEN);
      EXPECT_GT(rc, 0);

      printf("single_inotify_callback CALLED\n");


      myObj->destroy_source();
    }

    static void continuous_callback(rwsched_CFSocketRef s,
                                    CFSocketCallBackType type,
                                    CFDataRef address,
                                    const void *data,
                                    void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;

      if (myObj->stopped)
        myObj->destroy_source();
    }

    static void continuous_read_create_destroy_callback(rwsched_CFSocketRef s,
                                                        CFSocketCallBackType type,
                                                        CFDataRef address,
                                                        const void *data,
                                                        void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;

      myObj->destroy_source();
      if (! myObj->stopped)
        myObj->create_read_source(myObj->continuous_read_create_destroy_callback);
    }

    static void single_write_callback(rwsched_CFSocketRef s,
                                      CFSocketCallBackType type,
                                      CFDataRef address,
                                      const void *data,
                                      void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;
      myObj->destroy_source();
    }

    static void continuous_write_create_destroy_callback(rwsched_CFSocketRef s,
                                                         CFSocketCallBackType type,
                                                         CFDataRef address,
                                                         const void *data,
                                                         void *info)
    {
      auto myObj = static_cast<CFRunLoopDescriptorTest *>(info);
      myObj->callbacks_triggered++;

      myObj->destroy_source();
      if (! myObj->stopped)
        myObj->create_write_source(myObj->continuous_write_create_destroy_callback);
    }
    
    rwsched_CFRunLoopSourceRef cfsource;
    rwsched_CFSocketRef cfsocket;
    int pipe_fds[2];
    int inotify_fd;
    char *inotify_tmp_fname;
    int callbacks_triggered = 0;
};

TEST_F(CFRunLoopDescriptorTest, ReadSingleCallback)
{
  this->create_read_source(this->single_read_callback);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

TEST_F(CFRunLoopDescriptorTest, InotifySingleCallback)
{
  this->create_inotify_source(this->single_inotify_callback);
  unlink(this->inotify_tmp_fname);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

// DISABLED: this test fails some times; looks like callback is dispatched.
TEST_F(CFRunLoopDescriptorTest, DISABLED_ReadSingleCallbackOneLoop)
{
  this->create_read_source(this->single_read_callback);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 0, false);

  // Since we only ran a single iteration, the callback should not have fired yet.
  ASSERT_EQ(this->callbacks_triggered, 0);

  this->stop_test();
}

TEST_F(CFRunLoopDescriptorTest, ReadContinuousCallback)
{
  this->create_read_source(this->continuous_callback);

  RWUT_BENCH_MS(ReadContinuousCallbackBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(ReadContinuousCallbackBench);
  RWUT_BENCH_RECORD_PROPERTY(ReadContinuousCallbackBench, callbacks_triggered, this->callbacks_triggered);

  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:1000);
}

TEST_F(CFRunLoopDescriptorTest, ReadContinuousCreateDestroy)
{
  this->create_read_source(this->continuous_read_create_destroy_callback);

  RWUT_BENCH_MS(ReadContinuousCreateDestroyBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(ReadContinuousCreateDestroyBench);
  RWUT_BENCH_RECORD_PROPERTY(ReadContinuousCreateDestroyBench, callbacks_triggered, this->callbacks_triggered);

  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:500);
}

TEST_F(CFRunLoopDescriptorTest, WriteSingleCallback)
{
  this->create_write_source(this->single_write_callback);
  rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

TEST_F(CFRunLoopDescriptorTest, WriteContinuousCallback)
{
  this->create_write_source(this->continuous_callback);

  RWUT_BENCH_MS(WriteContinuousCallbackBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(WriteContinuousCallbackBench);
  RWUT_BENCH_RECORD_PROPERTY(WriteContinuousCallbackBench, callbacks_triggered, this->callbacks_triggered);
  
  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:1000);
}

TEST_F(CFRunLoopDescriptorTest, WriteContinuousCreateDestroy)
{
  this->create_write_source(this->continuous_write_create_destroy_callback);

  RWUT_BENCH_MS(WriteContinuousCreateDestroyBench,1.000);
    rwsched_instance_CFRunLoopRunInMode(this->rwsched, this->rwsched->main_cfrunloop_mode, 1.00, false);
  RWUT_BENCH_END(WriteContinuousCreateDestroyBench);
  RWUT_BENCH_RECORD_PROPERTY(WriteContinuousCreateDestroyBench, callbacks_triggered, this->callbacks_triggered);

  this->stop_test();

  ASSERT_GT(this->callbacks_triggered, RUNNING_ON_VALGRIND?1:500);
}
