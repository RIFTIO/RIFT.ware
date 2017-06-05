
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <stdio.h>
#include "rwsched.h"
#include "rwsched_toysched.h"
#include "rwut.h"

typedef struct {
  int total_callback_count;
  int read_callback_count;
  int write_callback_count;
  int timer_callback_count;
  int read_byte_count;
  int read_pipe_fd;
  int write_pipe_fd;
} toysched_callback_context_t;

static void
toyfd_callback(struct rwmsg_toyfd_s * id, int fd, int revents, void *ud)
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
      printf ("read: %d\n", rc);
    }
    fcntl(fd, F_SETFL, flags);
  }
}

TEST(ToyFdIoTest, ToyFdIoCallbackPerfTest)
{
    rwmsg_toysched_t tsched;
    rwmsg_toytask_t *task;
    toysched_callback_context_t *context;
    struct rwmsg_toyfd_s *callback_id[2];
    int pipe_rc, pipe_fds[2];

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

    // Send data through the pipe to trigger the callback
    pipe_rc = write(pipe_fds[1], "abcd", 4);
    EXPECT_TRUE(pipe_rc == 4);

    // HANGS?         
    RWUT_BENCH_MS(bench, 1000);
        printf ("run");
        rwmsg_toysched_runloop(&tsched, NULL, 0, 0.0);
    RWUT_BENCH_END(bench);

    // Verify the read and write callbacks were invoked
    EXPECT_GE(context->total_callback_count, 2);
    EXPECT_GE(context->read_callback_count, 1);
    EXPECT_GE(context->write_callback_count, 1);
    EXPECT_EQ(context->read_byte_count, pipe_rc);
    //
    // Delete the toytask file descriptor read and write callbacks
    rwmsg_toyfd_del(task, callback_id[0]);
    
    rwmsg_toyfd_del(task, callback_id[1]);
    

    // Free the callback context structure
    RW_FREE_TYPE(context, toysched_callback_context_t);

    // Destroy the toytask
    rwmsg_toytask_destroy(task);

    // Destroy the toy scheduler instance
    rwmsg_toysched_deinit(&tsched);
}
