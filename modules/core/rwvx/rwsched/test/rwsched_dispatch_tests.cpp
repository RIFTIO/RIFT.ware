
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

#include "rwut.h"

using ::testing::AtLeast;

//#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG(...) fprintf(stderr, ##__VA_ARGS__)
#else 
#define DEBUG(...)
#endif

class RwSchedSource : public ::testing::Test { 
  protected:
    virtual void create_source() = 0;
    
    virtual void SetUp() {
      g_rwsched_tasklet_count = 0;
      g_rwsched_instance_count = 0;

      DEBUG("RwSchedIOSourceRead SetUp()");
      this->rwsched = rwsched_instance_new();
      this->tasklet = rwsched_tasklet_new(this->rwsched);
      this->main_q = rwsched_dispatch_get_main_queue(this->rwsched);

      this->source = NULL;
      this->create_source();
    }

    virtual void TearDown() {
      DEBUG("RwSchedIOSourceRead TearDown()");
      this->stop_test();

      if (this->source)
        rwsched_dispatch_source_release(this->tasklet, this->source);
      this->source = NULL;
      rwsched_tasklet_release_all_resource(this->tasklet);
      rwsched_tasklet_free(this->tasklet);
      rwsched_instance_free(this->rwsched);

      ASSERT_EQ(g_rwsched_tasklet_count, 0);
      ASSERT_EQ(g_rwsched_instance_count, 0);
    }

    void stop_test()
    {
      DEBUG("Stopping test.\n");
      this->stopped = true;
      
      // Re-run the event loop to flush all source events out.
      rwsched_dispatch_main_until(this->tasklet, .002, NULL);
    }

    bool stopped = false;
    rwsched_instance_ptr_t rwsched;
    rwsched_tasklet_ptr_t tasklet;
    rwsched_dispatch_source_t source;
    rwsched_dispatch_queue_t main_q;
};

// Class which deals with the read source configuration.
class RwSchedIOSourceRead : public RwSchedSource {

  protected:
    void create_source()
    {
      int rc = pipe(this->pipe_fds);
      ASSERT_TRUE(rc == 0);

      /* New dispatch event source objects returned by dispatch_source_create have a suspension count of 1 and must be resumed before any events are delivered. */
      this->source = rwsched_dispatch_source_create(this->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_READ,
                                                    this->pipe_fds[0],
                                                    0,
                                                    this->main_q);
      ASSERT_TRUE(this->source);
      rwsched_dispatch_resume(this->tasklet, this->source);

      rwsched_dispatch_set_context(this->tasklet, this->source, (void *)this);

      // Send data through the pipe
      rc = write(this->pipe_fds[1], "abcd", 4);
      ASSERT_TRUE(rc == 4);
    }

    void destroy_source()
    {
      if (this->source)
        rwsched_dispatch_source_release(this->tasklet, this->source);
      this->source = NULL;

      // Close any open file handles
      close(this->pipe_fds[0]);
      close(this->pipe_fds[1]);
    }

    static void io_callback(void *arg) {
      RwSchedIOSourceRead  *myObj = static_cast<RwSchedIOSourceRead *>(arg);
      DEBUG("callback\n");
      DEBUG("thread %lu\n",pthread_self());

      ASSERT_EQ(rwsched_dispatch_source_get_data(myObj->tasklet, myObj->source), 4);

      myObj->callbacks_triggered++;
      if (myObj->stopped)
      {
        DEBUG("calling cancel source\n");
        if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
      }
    }

    static void io_cancel_callback(void *arg) {
      RwSchedIOSourceRead  *myObj = static_cast<RwSchedIOSourceRead *>(arg);

      myObj->callbacks_triggered++;
      //myObj->io_callback(arg);

      ASSERT_EQ(rwsched_dispatch_source_get_data(myObj->tasklet, myObj->source), 4);

      // Now read from this pipe
      char buffer[5];
      int rc = read(myObj->pipe_fds[0], buffer, 4);
      ASSERT_EQ(4, rc);

      // End of string marker to use ASSERT_STREQ function
      buffer[4] = '\0';
      ASSERT_STREQ("abcd", buffer);

      DEBUG("calling cancel source\n");
      if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void cancel_callback(void *arg)
    {
      RwSchedIOSourceRead *myObj = static_cast<RwSchedIOSourceRead *>(arg);

      DEBUG("Cancel callback\n");
      DEBUG("thread %lu\n",pthread_self());

      myObj->cancels_triggered++;

      myObj->destroy_source();
    }

    static void cancel_recreate_callback(void *arg)
    {
      RwSchedIOSourceRead  *myObj = (RwSchedIOSourceRead *)arg;
      myObj->cancel_callback(arg);

      if (! myObj->stopped)
      {
        myObj->create_source();
        rwsched_dispatch_source_set_event_handler_f(myObj->tasklet, 
                                                    myObj->source, myObj->io_cancel_callback);

        rwsched_dispatch_source_set_cancel_handler_f(myObj->tasklet, 
                                                     myObj->source, myObj->cancel_recreate_callback);
      }
    }

    // Keep track of the number of times that the IO callback and IO cancel callback are triggered.
    int callbacks_triggered = 0;
    int cancels_triggered = 0;
    int pipe_fds[2];
};

// Ensure that the IO callback and cancel happen at least a single time.
TEST_F(RwSchedIOSourceRead, ReadSingleCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);

  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  this->destroy_source();

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 0);
}

// Ensure that the IO callback and cancel happen at least a single time.
TEST_F(RwSchedIOSourceRead, ReadContinuousCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_callback);

  // Benchmark how many times we can get a read callback & cancel and get a cancel callback.
  RWUT_BENCH_MS(ReadContinuousCallbackBench, 1000);
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(ReadContinuousCallbackBench);
  RWUT_BENCH_RECORD_PROPERTY(ReadContinuousCallbackBench, callbacks_triggered, this->callbacks_triggered);

  
  ASSERT_GT(this->callbacks_triggered, 5);
}


// Ensure that the IO callback and cancel happen at least a single time.
TEST_F(RwSchedIOSourceRead, ReadSingleCallbackCancel)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_callback);

  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 1);
}

// Cancel and Recreate source event handlers performance test.
TEST_F(RwSchedIOSourceRead, ReadContinuousCallbackCancel)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_recreate_callback);

  // Benchmark how many times we can get a read callback & cancel and get a cancel callback.
  RWUT_BENCH_MS(IOCallbackCancelBench, 1000);
    rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(IOCallbackCancelBench);
  RWUT_BENCH_RECORD_PROPERTY(IOCallbackCancelBench, callbacks_triggered, this->callbacks_triggered);
  RWUT_BENCH_RECORD_PROPERTY(IOCallbackCancelBench, cancels_triggered, this->cancels_triggered);

  ASSERT_GT(this->callbacks_triggered, 2);
  ASSERT_GT(this->cancels_triggered, 2);
}


// Class which deals with the read source configuration.
class RwSchedIOSourceWrite : public RwSchedSource {

  protected:
    void create_source()
    {
      int rc = pipe(this->pipe_fds);
      ASSERT_TRUE(rc == 0);

      /* New dispatch event source objects returned by dispatch_source_create have a suspension count of 1 and must be resumed before any events are delivered. */
      this->source = rwsched_dispatch_source_create(this->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_WRITE,
                                                    this->pipe_fds[1],
                                                    0,
                                                    this->main_q);
      ASSERT_NE(this->source, nullptr);
      rwsched_dispatch_resume(this->tasklet, this->source);

      rwsched_dispatch_set_context(this->tasklet, this->source, (void *)this);
    }

    void destroy_source()
    {
      rwsched_dispatch_source_t source = this->source;
      this->source = NULL;
      if (source)
        rwsched_dispatch_source_release(this->tasklet, source);

      // Close any open file handles
      close(this->pipe_fds[0]);
      close(this->pipe_fds[1]);
    }

    static void io_callback(void *arg) {
      RwSchedIOSourceWrite *myObj = static_cast<RwSchedIOSourceWrite *>(arg);

      DEBUG("write callback\n");
      DEBUG("thread %lu\n", pthread_self());

      myObj->callbacks_triggered++;

      if (myObj->stopped)
      {
        DEBUG("calling cancel source\n");
        if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
      }
    }

    static void io_cancel_callback(void *arg) {
      RwSchedIOSourceWrite *myObj = static_cast<RwSchedIOSourceWrite *>(arg);

      myObj->callbacks_triggered++;
      //myObj->io_callback(arg);

      DEBUG("calling cancel source\n");
      if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void io_cancel_callback_get_data(void *arg) {
      RwSchedIOSourceWrite *myObj = static_cast<RwSchedIOSourceWrite *>(arg);

      myObj->callbacks_triggered++;
      //myObj->io_callback(arg);

      int buffer_size = rwsched_dispatch_source_get_data(myObj->tasklet, myObj->source);
      EXPECT_GT(buffer_size, 5);

      DEBUG("calling cancel source\n");
      if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void cancel_callback(void *arg)
    {
      RwSchedIOSourceWrite *myObj = static_cast<RwSchedIOSourceWrite *>(arg);

      myObj->cancels_triggered++;
      DEBUG("Cancel callback\n");
      DEBUG("thread %lu\n", pthread_self());

      myObj->destroy_source();
    }

    static void cancel_create_callback(void *arg)
    {
      RwSchedIOSourceWrite *myObj = static_cast<RwSchedIOSourceWrite *>(arg);

      if (! myObj->stopped)
      {
        myObj->cancel_callback(arg);
        myObj->create_source();
        rwsched_dispatch_source_set_event_handler_f(myObj->tasklet, 
                                                    myObj->source, myObj->io_cancel_callback);

        rwsched_dispatch_source_set_cancel_handler_f(myObj->tasklet, 
                                                     myObj->source, myObj->cancel_create_callback);
      }
    }

    int pipe_fds[2];
    int callbacks_triggered = 0;
    int cancels_triggered = 0;
};


TEST_F(RwSchedIOSourceWrite, WriteSingleCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);

  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 0);
}

// Test failing because rwsched_dispatch_source_get_data() is returning 0 instead of expected buffer size.
TEST_F(RwSchedIOSourceWrite, DISABLED_WriteCallbackGetData)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback_get_data);

  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 0);
}

TEST_F(RwSchedIOSourceWrite, WriteSingleCancelCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_callback);

  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 1);
}

TEST_F(RwSchedIOSourceWrite, WriteContinuousCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_callback);

  RWUT_BENCH_MS(WriteContinuousCallbackBench, 1000);
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(WriteContinuousCallbackBench);
  RWUT_BENCH_RECORD_PROPERTY(WriteContinuousCallbackBench, callbacks_triggered, this->callbacks_triggered);

  ASSERT_GT(this->callbacks_triggered, 5);
  ASSERT_EQ(this->cancels_triggered, 0);
}

TEST_F(RwSchedIOSourceWrite, DISABLED_WriteContinuousCancelCallback)
{
  DEBUG("calling dispatch_main\n");
  DEBUG("thread %lu\n", pthread_self());

  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->io_cancel_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_create_callback);

  RWUT_BENCH_MS(WriteCallbackCancelBench, 1000);
    rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(WriteCallbackCancelBench);
  RWUT_BENCH_RECORD_PROPERTY(WriteCallbackCancelBench, callbacks_triggered, this->callbacks_triggered);
  RWUT_BENCH_RECORD_PROPERTY(WriteCallbackCancelBench, cancels_triggered, this->cancels_triggered);

  ASSERT_GT(this->callbacks_triggered, 2);
  ASSERT_GT(this->cancels_triggered, 2);
}


// Base class which deals with the timer/tasklet configuration.
class RwSchedTimerSource : public RwSchedSource {
  protected:
    void create_source() {
      this->source = rwsched_dispatch_source_create(this->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                    0,
                                                    0,
                                                    this->main_q);
      ASSERT_TRUE(this->source);

      // Set the object context so we can access this instance inside of the static callback
      rwsched_dispatch_set_context(this->tasklet, this->source, (void *)this);
    }

    void destroy_source() {
      if (this->source)
        rwsched_dispatch_source_release(this->tasklet, this->source);
      this->source = NULL;
    }

    static void timer_callback(void* context) {
      auto myObj = static_cast<RwSchedTimerSource *>(context);

      DEBUG("Timer callback triggered\n");
      myObj->callbacks_triggered++;

      if (myObj->stopped)
          if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void single_timer_callback(void* context) {
      auto myObj = static_cast<RwSchedTimerSource *>(context);

      myObj->callbacks_triggered++;
      //myObj->timer_callback(context);

      unsigned long timer_count = rwsched_dispatch_source_get_data(myObj->tasklet, myObj->source);
      EXPECT_GT(timer_count, 1);


      if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void cancel_callback(void* context) {
      auto myObj = static_cast<RwSchedTimerSource *>(context);

      myObj->cancels_triggered++;

      myObj->destroy_source();
    }

    static void cancel_recreate_callback(void* context) {
      auto myObj = static_cast<RwSchedTimerSource *>(context);
      myObj->cancel_callback(context);

      if (! myObj->stopped)
      {
        myObj->create_source();
        rwsched_dispatch_source_set_timer(myObj->tasklet, myObj->source, DISPATCH_TIME_NOW, 100, 10);
        rwsched_dispatch_source_set_event_handler_f(myObj->tasklet, myObj->source, myObj->single_timer_callback);
        rwsched_dispatch_source_set_cancel_handler_f(myObj->tasklet, myObj->source, myObj->cancel_recreate_callback);
        
        // Enable the timer source
        rwsched_dispatch_resume(myObj->tasklet, myObj->source);
      }
    }

    int callbacks_triggered = 0;
    int cancels_triggered = 0;
};

TEST_F(RwSchedTimerSource, TimerSingleFire)
{
  rwsched_dispatch_source_set_timer(this->tasklet, this->source, DISPATCH_TIME_NOW, 100, 10);
  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->single_timer_callback);
  // Enable the timer source
  rwsched_dispatch_resume(this->tasklet, this->source);
  
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
}

TEST_F(RwSchedTimerSource, TimerContinuousFire)
{
  rwsched_dispatch_source_set_timer(this->tasklet, this->source, DISPATCH_TIME_NOW, 100, 10);
  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->timer_callback);
  // Enable the timer source
  rwsched_dispatch_resume(this->tasklet, this->source);
  
  RWUT_BENCH_MS(TimerCallbackBench, 1000);
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(TimerCallbackBench);
  RWUT_BENCH_RECORD_PROPERTY(TimerCallbackBench, callbacks_triggered, this->callbacks_triggered);

  ASSERT_GT(this->callbacks_triggered, 5);
}

TEST_F(RwSchedTimerSource, TimerSingleCancel)
{
  rwsched_dispatch_source_set_timer(this->tasklet, this->source, DISPATCH_TIME_NOW, 100, 10);
  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->single_timer_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_callback);
  // Enable the timer source
  rwsched_dispatch_resume(this->tasklet, this->source);
  
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 1);
}

TEST_F(RwSchedTimerSource, TimerContinuousCancel)
{
  rwsched_dispatch_source_set_timer(this->tasklet, this->source, DISPATCH_TIME_NOW, 100, 10);
  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->single_timer_callback);
  rwsched_dispatch_source_set_cancel_handler_f(this->tasklet, this->source, this->cancel_recreate_callback);
  // Enable the timer source
  rwsched_dispatch_resume(this->tasklet, this->source);

  
  RWUT_BENCH_MS(TimerContinuousCancelBench, 1000);
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);
  RWUT_BENCH_END(TimerContinuousCancelBench);
  RWUT_BENCH_RECORD_PROPERTY(TimerContinuousCancelBench, callbacks_triggered, this->callbacks_triggered);

  ASSERT_GT(this->callbacks_triggered, 2);
  ASSERT_GT(this->cancels_triggered, 2);
}

class RwSchedSignalSource : public RwSchedSource {
  protected:
    void create_source() {
      // Need to disable default signal handler so libdispatch can catch it.
      signal(SIGUSR1, SIG_IGN);
      this->source = rwsched_dispatch_source_create(this->tasklet,
                                                    RWSCHED_DISPATCH_SOURCE_TYPE_SIGNAL,
                                                    SIGUSR1,
                                                    0,
                                                    this->main_q);
      ASSERT_TRUE(this->source);

      this->allocated = true;

      // Enable the signal handler source
      rwsched_dispatch_resume(this->tasklet, this->source);

      // Set the object context so we can access this instance inside of the static callback
      rwsched_dispatch_set_context(this->tasklet, this->source, (void *)this);

      // Set the object context so we can access this instance inside of the static callback
      ASSERT_EQ(rwsched_dispatch_get_context(this->tasklet, this->source), this);
    }

    void destroy_source() {
          rwsched_dispatch_source_cancel(this->tasklet, this->source);

          if (this->source)
            rwsched_dispatch_source_release(this->tasklet, this->source);
          this->source = NULL;
    }

    static void signal_callback(void* context) {
      auto myObj = static_cast<RwSchedSignalSource *>(context);

      DEBUG("Signal callback triggered\n");
      myObj->callbacks_triggered++;

      if (myObj->stopped)
          if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void single_signal_callback(void* context) {
      auto myObj = static_cast<RwSchedSignalSource *>(context);
      myObj->signal_callback(context);

      if (myObj->source) rwsched_dispatch_source_cancel(myObj->tasklet, myObj->source);
    }

    static void cancel_callback(void* context) {
      auto myObj = static_cast<RwSchedSignalSource *>(context);

      myObj->cancels_triggered++;

      if (myObj->source)
        rwsched_dispatch_source_release(myObj->tasklet, myObj->source);
      myObj->source = NULL;
    }

    static void cancel_recreate_callback(void* context) {
      auto myObj = static_cast<RwSchedSignalSource *>(context);
      myObj->cancel_callback(context);

      if (! myObj->stopped)
      {
        rwsched_dispatch_source_set_event_handler_f(myObj->tasklet, myObj->source, myObj->single_signal_callback);
        rwsched_dispatch_source_set_cancel_handler_f(myObj->tasklet, myObj->source, myObj->cancel_recreate_callback);
        myObj->create_source();
      }
    }

    bool allocated = false;
    int callbacks_triggered = 0;
    int cancels_triggered = 0;
};

TEST_F(RwSchedSignalSource, AllocateDeallocateSignal)
{
  this->destroy_source();
  ASSERT_EQ(this->callbacks_triggered, 0);
  ASSERT_EQ(this->cancels_triggered, 0);
  ASSERT_EQ(this->allocated, true);
}

// CALLBACK DOES NOT FIRE.  Disabled.
TEST_F(RwSchedSignalSource, DISABLED_SingleSignalCallback)
{
  rwsched_dispatch_source_set_event_handler_f(this->tasklet, this->source, this->single_signal_callback);

  char cmd_buffer[128];
  sprintf(cmd_buffer, "kill -s USR1 %d", getpid());
  printf("%s",cmd_buffer);
  ASSERT_EQ(0, system(cmd_buffer));

  // Send a signal to this process
  rwsched_dispatch_main_until(this->tasklet, .100, NULL);

  ASSERT_EQ(this->callbacks_triggered, 1);
  ASSERT_EQ(this->cancels_triggered, 0);
}
