#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>

static void timer_did_fire(void *context)
{
  ((void)context);
  printf("main queue timer fired...\n");
}

static void write_completion_handler(dispatch_data_t unwritten_data,
                                     int error,
                                     void *context)
{
  ((void)context);
  if (!unwritten_data && error == 0)
    printf("Dispatch I/O wrote everything to stdout. Hurrah.\n");
}

static void read_completion_handler(dispatch_data_t data,
                                    int error,
                                    void *context)
{
  ((void)error);
  int fd = (intptr_t)context;
  close(fd);

  dispatch_write_f_np(STDOUT_FILENO,
                      data,
                      dispatch_get_main_queue(),
                      NULL,
                      write_completion_handler);
}

static void foo_async_func(void *context)
{
    printf("foo_async_func fired...magic = 0x%x\n", *(int *)context);
}

static void foo_queue_timer_did_fire(void *source)
{
  ((void)source);
  dispatch_queue_t q = dispatch_get_current_queue();
  void *context = dispatch_get_context(q);

  printf("foo_queue timer fired...magic = 0x%x\n", *(int *)context);

  /**
   * lets test an async function here
   */
  dispatch_async_f(q, context, foo_async_func);  
}


int main(int argc, const char *argv[])
{
  dispatch_source_t timer;
  dispatch_queue_t queue;
  
  ((void)argc);
  ((void)argv);
   
  /**
   * dispatch queue creation example
   * Lets create a queue and associate a timer event
   */

  queue = dispatch_queue_create("foo_queue", NULL);
  if (queue) {
    int *magic = (int *)malloc(sizeof(int));
    *magic = 0x12345678;
    dispatch_set_context(queue, magic);
  }
  timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
  dispatch_source_set_event_handler_f(timer, foo_queue_timer_did_fire);
  dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC,
                            0.5 * NSEC_PER_SEC);

  dispatch_resume(timer);

  
  /**
   * Another timer event this time from main queue
   */
  timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                 0,
                                 0,
                                 dispatch_get_main_queue());

  dispatch_source_set_event_handler_f(timer, timer_did_fire);
  dispatch_source_set_timer(timer,
                            DISPATCH_TIME_NOW,
                            1 * NSEC_PER_SEC,
                            0.5 * NSEC_PER_SEC);
  dispatch_resume(timer);

  /**
   * Some examples with file descriptors
   */
  int fd = open("dispatch_test.c", O_RDONLY);
  dispatch_read_f_np(fd,
                     SIZE_MAX,
                     dispatch_get_main_queue(),
                     (void *)(intptr_t)fd,
                     read_completion_handler);

  dispatch_main();
  return 0;
}
