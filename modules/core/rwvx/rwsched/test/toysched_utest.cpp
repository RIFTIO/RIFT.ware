
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <stdio.h>
#include "rwsched.h"
#include "rwsched_toysched.h"
#include "rwut.h"
#include <valgrind/valgrind.h>

using ::testing::AtLeast;

#if 0
#undef EXPECT_TRUE
#define EXPECT_TRUE(a) RW_ASSERT(a)
#endif

typedef struct {
  int total_callback_count;
  int read_callback_count;
  int write_callback_count;
  int timer_callback_count;
  int read_byte_count;
  int read_pipe_fd;
  int write_pipe_fd;
} toysched_callback_context_t;

#if 0
void myFunc(int argc, char** argv) {
  printf("\n**** MAIN SETUP!!\n\n");
}
RWUT_INIT(myFunc);
#endif

TEST(ToyschedUnittest, InitAndDeinit)
{
  rwmsg_toysched_t tsched;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, ToytaskCreateAndDestroy)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

static void
toyfd_callback(uint64_t id, int fd, int revents, void *ud)
{
  UNUSED(id);
  toysched_callback_context_t *context = (toysched_callback_context_t *) ud;
  int rc;
  char buffer[1024];

  // Validate input parameters
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Increment the callback counters
  context->total_callback_count++;
  context->read_callback_count += (revents & POLLIN ? 1 : 0);
  context->write_callback_count += (revents & POLLOUT ? 1 : 0);

  // If this is a read callback, then read the data from the file descriptor
  if (fd == context->read_pipe_fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    rc = read(fd, buffer, sizeof(buffer));
    if (rc > 0) {
      context->read_byte_count += rc;
    }
    fcntl(fd, F_SETFL, flags);
  }
}

TEST(ToyschedUnittest, ToyfdAddAndDelete)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  uint64_t toyfd_callback_id;
  uint64_t rc;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Add a toytask file descriptor for callback
  int fd  = 0;
  int pollbits = POLLIN;
  toyfd_callback_id = rwmsg_toyfd_add(task, fd, pollbits, NULL, NULL);
  EXPECT_TRUE(toyfd_callback_id);

  // Delete the toytask file descriptor callback
  rc = rwmsg_toyfd_del(task, toyfd_callback_id);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_callback_id);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}


static void
toytimer_callback(uint64_t id, void *ud)
{
  UNUSED(id);
  toysched_callback_context_t *context = (toysched_callback_context_t *) ud;

  // Validate input parameters
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Increment the callback counters
  context->total_callback_count++;
  context->timer_callback_count++;
}


class ToyFdIoTest : public ::testing::Test {

  protected: 

    void SetUp(){
      // Create an instance of the toy scheduler
      rwmsg_toysched_init(&tsched);

      // Create a toytask
      task = rwmsg_toytask_create(&tsched);
      EXPECT_TRUE(task);

      // Create a pipe to send data through
      pipe_rc = pipe(pipe_fds);
      EXPECT_TRUE(pipe_rc == 0);

      // Allocate a callback context structure
      context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
      RW_ASSERT_TYPE(context, toysched_callback_context_t);
      context->read_pipe_fd = pipe_fds[0];
      context->write_pipe_fd = pipe_fds[1];

      // Add a toytask file descriptor for read and write callback
      callback_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
      EXPECT_TRUE(callback_id[0]);
      callback_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
      EXPECT_TRUE(callback_id[1]);

      pipe_rc = write(this->pipe_fds[1], "abcd", 4);
      EXPECT_TRUE(pipe_rc == 4);
    }

    void TearDown(){
      // Delete the toytask file descriptor read and write callbacks
      uint64_t rc;
      rc = rwmsg_toyfd_del(task, callback_id[0]);
      EXPECT_TRUE(rc);
      EXPECT_EQ(rc, callback_id[0]);
      rc = rwmsg_toyfd_del(task, callback_id[1]);
      EXPECT_TRUE(rc);
      EXPECT_EQ(rc, callback_id[1]);

      // Free the callback context structure
      RW_FREE_TYPE(context, toysched_callback_context_t);

      // Destroy the toytask
      rwmsg_toytask_destroy(task);

      // Destroy the toy scheduler instance
      rwmsg_toysched_deinit(&tsched);
    }

    rwmsg_toysched_t tsched;
    rwmsg_toytask_t *task;
    toysched_callback_context_t *context;
    uint64_t callback_id[2];
    int pipe_rc, pipe_fds[2];

};

TEST_F(ToyFdIoTest, ToyFdIoCallback)
{
    // Wait a small amount of time for the callbacks to be invoked
    // Send data through the pipe to trigger the callback
    usleep(10 * 1000);
    rwmsg_toysched_run(&this->tsched, NULL);

    // Verify the read and write callbacks were invoked
    EXPECT_GE(this->context->total_callback_count, 2);
    EXPECT_GE(this->context->read_callback_count, 1);
    EXPECT_GE(this->context->write_callback_count, 1);
    EXPECT_EQ(this->context->read_byte_count, this->pipe_rc);
}

// Test Hangs in runloop?
//TEST_F(ToyFdIoTest, ToyFdIoCallbackPerfTest)
//{
//    RWUT_BENCH_MS(ToyFdIoCallbackBench, 1000);
//        // Send data through the pipe to trigger the callback
//        int rc = write(this->pipe_fds[1], "abcd", 4);
//        EXPECT_TRUE(rc == 4);
//        rwmsg_toysched_runloop(&this->tsched, NULL, 0, 0);
//    RWUT_BENCH_END(ToyFdIoCallbackBench);
//
//    // Verify the read and write callbacks were invoked
//    EXPECT_GE(this->context->total_callback_count, 2);
//    EXPECT_GE(this->context->read_callback_count, 1);
//    EXPECT_GE(this->context->write_callback_count, 1);
//    //EXPECT_EQ(this->context->read_byte_count, this->pipe_rc);
//}


TEST(ToyschedUnittest, ToytimerAddAndDelete)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  uint64_t callback_id;
  uint64_t rc;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Add a toytask timer callback
  int msec = 0;
  callback_id = rwmsg_toytimer_add(task, msec, toytimer_callback, NULL);
  EXPECT_TRUE(callback_id);

  // Delete the toytask timer callback
  rc = rwmsg_toytimer_del(task, callback_id);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, callback_id);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, SingleToytaskToytimerCallback)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  uint64_t callback_id;
  //uint64_t rc;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add a toytask timer callback
  int msec = 1;
  callback_id = rwmsg_toytimer_add(task, msec, toytimer_callback, context);
  EXPECT_TRUE(callback_id);

  // Wait a small amount of time for the callbacks to be invoked
  usleep(10 * 1000);
  rwmsg_toysched_run(&tsched, NULL);

  // Verify the timer callback was invoked
  EXPECT_EQ(context->total_callback_count, 1);
  EXPECT_EQ(context->timer_callback_count, 1);

#if 0
  // Delete the toytask timer callback
  rc = rwmsg_toytimer_del(task, callback_id);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, callback_id);
#endif

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}


TEST(ToyschedUnittest, MultiToytaskToytimerCallback)
{
  const int ntasks = 16;
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task[ntasks];
  toysched_callback_context_t *context;
  uint64_t callback_id[ntasks];
  //uint64_t rc;
  int i;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create multiple toytasks
  for (i = 0 ; i < ntasks ; i++) {
    task[i] = rwmsg_toytask_create(&tsched);
    EXPECT_TRUE(task[i]);
  }

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add the toytask timer callbacks
  for (i = 0 ; i < ntasks ; i++) {
    int msec = 1;
    callback_id[i] = rwmsg_toytimer_add(task[i], msec, toytimer_callback, context);
    EXPECT_TRUE(callback_id[i]);
  }

  // Wait a small amount of time for the callbacks to be invoked
  usleep(10 * 1000);
  rwmsg_toysched_run(&tsched, NULL);

  // Verify the timer callback was invoked
  EXPECT_EQ(context->total_callback_count, ntasks);
  EXPECT_EQ(context->timer_callback_count, ntasks);

#if 0
  // Delete the toytask timer callbacks
  for (i = 0 ; i < ntasks ; i++) {
    rc = rwmsg_toytimer_del(task[i], callback_id[i]);
    EXPECT_TRUE(rc);
    EXPECT_EQ(rc, callback_id[i]);
  }
#endif

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytasks
  for (i = 0 ; i < ntasks ; i++) {
    rwmsg_toytask_destroy(task[i]);
  }

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, SingleToytaskBlockingModeWithoutData)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  uint64_t toyfd_id[2];
  int revents[2];

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Create a pipe to send data through
  // pipe_fds[0] is the read side of the pipe, pipe_fds[1] is the write side of the pipe
  int pipe_rc, pipe_fds[2];
  pipe_rc = pipe(pipe_fds);
  EXPECT_TRUE(pipe_rc == 0);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add a toytask file descriptor for read and write callback
  toyfd_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[0]);
  toyfd_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[1]);

  // Don't send any data, as we want the timer to expire
  // Call the blocking function with a timeout of 100 msec
  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], RUNNING_ON_VALGRIND?1000:100);
  revents[1] = rwmsg_toytask_block(task, toyfd_id[1], RUNNING_ON_VALGRIND?1000:100);

  // Verify the read and write return values
  EXPECT_EQ(revents[0], 0);
  EXPECT_EQ(revents[1], POLLOUT);

  // Delete the toytask file descriptor read and write callbacks
  uint64_t rc;
  rc = rwmsg_toyfd_del(task, toyfd_id[0]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[0]);
  rc = rwmsg_toyfd_del(task, toyfd_id[1]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[1]);

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, SingleToytaskBlockingModeWithData)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  uint64_t toyfd_id[2];
  int revents[2];

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Create a pipe to send data through
  int pipe_rc, pipe_fds[2];
  pipe_rc = pipe(pipe_fds);
  EXPECT_TRUE(pipe_rc == 0);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);
  context->read_pipe_fd = pipe_fds[0];

  // Add a toytask file descriptor for read and write callback
  toyfd_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[0]);
  toyfd_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[1]);

  // Send data through the pipe to trigger the io callback
  pipe_rc = write(pipe_fds[1], "abcd", 4);
  EXPECT_TRUE(pipe_rc == 4);

  // Call the blocking function with a timeout of 100 msec
  int timeout_ms = RUNNING_ON_VALGRIND?1000:100;
  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], timeout_ms);
  revents[1] = rwmsg_toytask_block(task, toyfd_id[1], timeout_ms);

  // Verify the read and write return values
  EXPECT_EQ(revents[0], POLLIN);
  EXPECT_EQ(revents[1], POLLOUT);

#if 0
  // Verify the read and write callbacks were invoked
  EXPECT_GE(context->total_callback_count, 2);
  EXPECT_GE(context->read_callback_count, 1);
  EXPECT_GE(context->write_callback_count, 1);
  EXPECT_EQ(context->read_byte_count, pipe_rc);
#endif

  // Delete the toytask file descriptor read and write callbacks
  uint64_t rc;
  rc = rwmsg_toyfd_del(task, toyfd_id[0]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[0]);
  rc = rwmsg_toyfd_del(task, toyfd_id[1]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[1]);

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, MultiToytaskBlockingModeWithData)
{
  const int ntasks = 16;
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task[ntasks];
  toysched_callback_context_t *context;
  uint64_t toyfd_id[ntasks][2];
  int revents[2];
  //uint64_t callback_id[ntasks];
  //uint64_t rc;
  int i;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create multiple toytasks
  for (i = 0 ; i < ntasks ; i++) {
    task[i] = rwmsg_toytask_create(&tsched);
    EXPECT_TRUE(task[i]);
  }

  // Create pipes to send data through
  int pipe_rc, pipe_fds[ntasks][2];
  for (i = 0 ; i < ntasks ; i++) {
    pipe_rc = pipe(pipe_fds[i]);
    EXPECT_TRUE(pipe_rc == 0);
  }

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add toytask file descriptor read and write callback(s)
  for (i = 0 ; i < ntasks ; i++) {
    toyfd_id[i][0] = rwmsg_toyfd_add(task[i], pipe_fds[i][0], POLLIN, toyfd_callback, context);
    EXPECT_TRUE(toyfd_id[i][0]);
    toyfd_id[i][1] = rwmsg_toyfd_add(task[i], pipe_fds[i][1], POLLOUT, toyfd_callback, context);
    EXPECT_TRUE(toyfd_id[i][1]);
  }

  // Send data through the pipe to trigger the io callback(s)
  for (i = 0 ; i < ntasks ; i++) {
    pipe_rc = write(pipe_fds[i][1], "abcd", 4);
    EXPECT_TRUE(pipe_rc == 4);
  }

  for (i = 0 ; i < ntasks ; i++) {
    // Update the read_pipe_fd for the current task being invoked
    context->read_pipe_fd = pipe_fds[i][0];

    // Call the blocking function with a timeout of 100 msec
    revents[0] = rwmsg_toytask_block(task[i], toyfd_id[i][0], RUNNING_ON_VALGRIND?1000:100);
    revents[1] = rwmsg_toytask_block(task[i], toyfd_id[i][1], RUNNING_ON_VALGRIND?1000:100);

    // Verify the read and write return values
    EXPECT_EQ(revents[0], POLLIN);
    EXPECT_EQ(revents[1], POLLOUT);
  }

#if 0
  // Verify the read and write callbacks were invoked
  EXPECT_GE(context->total_callback_count, 2);
  EXPECT_GE(context->read_callback_count, 1);
  EXPECT_GE(context->write_callback_count, 1);
  EXPECT_EQ(context->read_byte_count, pipe_rc * ntasks);
#endif

  // Delete the toytask file descriptor read and write callbacks
  for (i = 0 ; i < ntasks ; i++) {
    uint64_t rc;
    rc = rwmsg_toyfd_del(task[i], toyfd_id[i][0]);
    EXPECT_TRUE(rc);
    EXPECT_EQ(rc, toyfd_id[i][0]);
    rc = rwmsg_toyfd_del(task[i], toyfd_id[i][1]);
    EXPECT_TRUE(rc);
    EXPECT_EQ(rc, toyfd_id[i][1]);
  }

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  for (i = 0 ; i < ntasks ; i++) {
    rwmsg_toytask_destroy(task[i]);
  }

  // Close any open file handles
  for (i = 0 ; i < ntasks ; i++) {
    close(pipe_fds[i][0]);
    close(pipe_fds[i][1]);
  }

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

TEST(ToyschedUnittest, SingleToytaskToytimerCallbackWhileBlocking)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  uint64_t callback_id;
  uint64_t rc;
  uint64_t toyfd_id[2];
  int revents[2];
  int msec;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Create a pipe to send data through
  int pipe_rc, pipe_fds[2];
  pipe_rc = pipe(pipe_fds);
  EXPECT_TRUE(pipe_rc == 0);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add a toytask file descriptor for read and write callback
  toyfd_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[0]);
  toyfd_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[1]);

  msec = 4;
  callback_id = rwmsg_toytimer_add(task, msec, toytimer_callback, context);
  EXPECT_TRUE(callback_id);

  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 10);
  EXPECT_EQ(context->timer_callback_count, 0);  // Timer SHALL NOT fire
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 1);  // Timer SHALL fire
  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 10);
  EXPECT_EQ(revents[0], 0); // Verify the read retturns nothing
  usleep(10 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 1);  // Timer SHALL NOT fire; because the timer is already stopped

  msec = 7;
  callback_id = rwmsg_toytimer_add(task, msec, toytimer_callback, context);
  EXPECT_TRUE(callback_id);

  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 10);
  EXPECT_EQ(revents[0], 0); // Verify the read retturns nothing
  usleep(4 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 2); // Timer SHALL fire


  // Delete the toytask file descriptor read and write callbacks
  rc = rwmsg_toyfd_del(task, toyfd_id[0]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[0]);
  rc = rwmsg_toyfd_del(task, toyfd_id[1]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[1]);

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

static void
toytimer_callback_1T(uint64_t id, void *ud)
{
  UNUSED(id);
  toysched_callback_context_t *context = (toysched_callback_context_t *) ud;

  // Validate input parameters
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  //RW_ASSERT(context->total_callback_count == context->timer_callback_count);

  // Increment the callback counters
  context->total_callback_count++;
  context->timer_callback_count++;
}

TEST(ToyschedUnittest, SingleToytaskToy1TtimerCallbackWhileBlocking)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  uint64_t callback_id;
  uint64_t rc;
  uint64_t toyfd_id[2];
  int revents[2];
  UNUSED(revents);
  int msec;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Create a pipe to send data through
  int pipe_rc, pipe_fds[2];
  pipe_rc = pipe(pipe_fds);
  EXPECT_TRUE(pipe_rc == 0);

  // Allocate a callback context structure
  context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add a toytask file descriptor for read and write callback
  toyfd_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[0]);
  toyfd_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[1]);

  // Add a toytask timer callback
  msec = 10;
  callback_id = rwmsg_toy1Ttimer_add(task, msec, toytimer_callback_1T, context);
  EXPECT_TRUE(callback_id);
  usleep(10 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 1); // Timer shall fire

  callback_id = rwmsg_toy1Ttimer_add(task, msec, toytimer_callback_1T, context);
  EXPECT_TRUE(callback_id);
  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 500);
  EXPECT_EQ(context->timer_callback_count, 1); //Timer should not fire
  usleep(1 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 2); // Timer shall fire now

  // Delete the toytask file descriptor read and write callbacks
  rc = rwmsg_toyfd_del(task, toyfd_id[0]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[0]);
  rc = rwmsg_toyfd_del(task, toyfd_id[1]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[1]);

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
  usleep(1 * 1000);
}

static void
toyRtimer_callback(uint64_t id, void *ud)
{
  UNUSED(id);
  toysched_callback_context_t *context = (toysched_callback_context_t *) ud;

  // Validate input parameters
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Increment the callback counters
  context->total_callback_count++;
  context->timer_callback_count++;
}

TEST(ToyschedUnittest, SingleToytaskToyRtimerCallbackWhileBlocking)
{
  rwmsg_toysched_t tsched;
  rwmsg_toytask_t *task;
  toysched_callback_context_t *context;
  //toysched_callback_context_t *context1;
  uint64_t callback_id;
  uint64_t rc;
  uint64_t toyfd_id[2];
  int revents[2];
  UNUSED(revents);
  int msec;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&tsched);

  // Create a toytask
  task = rwmsg_toytask_create(&tsched);
  EXPECT_TRUE(task);

  // Create a pipe to send data through
  int pipe_rc, pipe_fds[2];
  pipe_rc = pipe(pipe_fds);
  EXPECT_TRUE(pipe_rc == 0);

  // Allocate a callback context structure
 context = (toysched_callback_context_t *) RW_MALLOC0_TYPE(sizeof(*context), toysched_callback_context_t);
  RW_ASSERT_TYPE(context, toysched_callback_context_t);

  // Add a toytask file descriptor for read and write callback
  toyfd_id[0] = rwmsg_toyfd_add(task, pipe_fds[0], POLLIN, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[0]);
  toyfd_id[1] = rwmsg_toyfd_add(task, pipe_fds[1], POLLOUT, toyfd_callback, context);
  EXPECT_TRUE(toyfd_id[1]);

  msec = 90;;
  callback_id = rwmsg_toyRtimer_add(task, msec, toyRtimer_callback, context);
  EXPECT_TRUE(callback_id);
  usleep(100 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 1);

  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 10);
  EXPECT_EQ(context->timer_callback_count, 1);

  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 90);
  EXPECT_EQ(context->timer_callback_count, 1);

  usleep(100 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 2); // Timer shall fire, because it is a repeat-timer

  usleep(100 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 3); // Timer shall fire, because it is a repeat-timer

  revents[0] = rwmsg_toytask_block(task, toyfd_id[0], 90);
  usleep(140 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 4); // Timer shall fire, because it is a repeat-timer

  rwmsg_toytimer_del(task, callback_id);
  usleep(100 * 1000);
  rwmsg_toysched_run(&tsched, NULL);
  EXPECT_EQ(context->timer_callback_count, 4); // Now that timer is stopped, it shall not fire any more

  // Delete the toytask file descriptor read and write callbacks
  rc = rwmsg_toyfd_del(task, toyfd_id[0]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[0]);
  rc = rwmsg_toyfd_del(task, toyfd_id[1]);
  EXPECT_TRUE(rc);
  EXPECT_EQ(rc, toyfd_id[1]);

  // Free the callback context structure
  RW_FREE_TYPE(context, toysched_callback_context_t);

  // Destroy the toytask
  rwmsg_toytask_destroy(task);

  // Close any open file handles
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  // Destroy the toy scheduler instance
  rwmsg_toysched_deinit(&tsched);
}

//--------------------------------------------------------------------------------
// Performance test from "toysched_perftest.cpp" (via rwmsg_toysched_runloop)
//--------------------------------------------------------------------------------

typedef enum {
  TOYTIMER_TYPE_ONESHOT,
  TOYTIMER_TYPE_PING,
  TOYTIMER_TYPE_PONG
} toytimer_type_t;

struct toytimer_context_s {
  toytimer_type_t type;
  uint64_t id;
  rwmsg_toytask_t *toytask;
  int timer_count;
  bool stopped;
};

typedef struct toytimer_context_s *toytimer_context_t;

static void
toytimer_oneshot_create(rwmsg_toytask_t *toytask,
			toytimer_context_t toytimer_context);

static void
toytimer_oneshot_callback(uint64_t id, void *ud);


static void
toytimer_oneshot_callback(uint64_t id, void *ud)
{
  toytimer_context_t toytimer_context = (toytimer_context_t) ud;

  // Validate input parameters
  RW_ASSERT_TYPE(toytimer_context, toytimer_context_t);

  // Validate the toytimer context parameters and update them
  EXPECT_EQ(toytimer_context->type, TOYTIMER_TYPE_ONESHOT);
  EXPECT_EQ(toytimer_context->id, id);
  toytimer_context->timer_count++;

  // Create another oneshot toytimer
  if (! toytimer_context->stopped)
    toytimer_oneshot_create(toytimer_context->toytask, toytimer_context);
}

static void
toytimer_oneshot_create(rwmsg_toytask_t *toytask,
                        toytimer_context_t toytimer_context)
{
  uint64_t toytimer_id;

  // Validate input parameters
  RW_ASSERT_TYPE(toytimer_context, toytimer_context_t);

  // Reschedule this timer
  int msec = 0;
  toytimer_id = rwmsg_toytimer_add(toytask, msec, toytimer_oneshot_callback, toytimer_context);
  EXPECT_TRUE(toytimer_id);

  // Update the toytimer context
  toytimer_context->type = TOYTIMER_TYPE_ONESHOT;
  toytimer_context->id = toytimer_id;
  toytimer_context->toytask = toytask;
}


TEST(ToyschedPerftest, Toytask) {
  rwmsg_toysched_t toysched;
  rwmsg_toytask_t *toytask;
  toytimer_context_t toytimer_context;
  //uint64_t timer_id;

  // Create an instance of the toy scheduler
  rwmsg_toysched_init(&toysched);

  // Create a toytask
  toytask = rwmsg_toytask_create(&toysched);
  EXPECT_TRUE(toytask);

  // Allocate a callback context structure
  toytimer_context = (toytimer_context_t) RW_MALLOC0_TYPE(sizeof(*toytimer_context), toytimer_context_t);
  RW_ASSERT_TYPE(toytimer_context, toytimer_context_t);

  // Create the first oneshot toytimer
  toytimer_oneshot_create(toytask, toytimer_context);
  
  int max_events = 0; // max_events is unused in rwmsg_toysched_runloop
  int run_for_secs = 0; //Only make a single loop.

  RWUT_BENCH_MS(timerCountBench, 1000);
  rwmsg_toysched_runloop(&toysched, NULL, max_events, run_for_secs);
  RWUT_BENCH_END(timerCountBench);

  // Mark the test as stopped and flush out the timer events.  Otherwise the oneshot loop will infect other tests.
  toytimer_context->stopped = true;
  rwmsg_toysched_runloop(&toysched, NULL, max_events, .1);

  RWUT_BENCH_RECORD_PROPERTY(timerCountBench, timer_count, toytimer_context->timer_count);

  EXPECT_GT(toytimer_context->timer_count, 0);
}

