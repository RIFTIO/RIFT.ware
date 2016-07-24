
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <stdio.h>
#include <cstdlib>
#include "rwsched.h"
#include "rwsched_toysched.h"
#include <libev/ev.h>

#include "gtest/gtest.h"

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
};

typedef struct toytimer_context_s *toytimer_context_t;

static void
toytimer_oneshot_create(struct ev_loop *loop,
			struct ev_timer *timeout_watcher);

static void
toytimer_oneshot_callback(EV_P_ ev_timer *w, int revents);

toytimer_context_t ud;
struct ev_loop *loop = EV_DEFAULT;
ev_timer g_timeout_watcher;

int
main(int argc, char *argv[])
{
  UNUSED(argc); UNUSED(argv);
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
  ud = toytimer_context;

  // Create the first oneshot toytimer
  toytimer_oneshot_create(loop, &g_timeout_watcher);
  
  // Run the toysched runloop until a specific number of timers are processed
  // rwmsg_toysched_runloop(&toysched, toytask, 1);
  int num_timers = 1 * 1000 * 100;
  while (1) {
    ev_run(loop, 0);
  }

  EXPECT_EQ(toytimer_context->timer_count, num_timers);
  printf("timer_count = %d\n", toytimer_context->timer_count);
}

static void
toytimer_oneshot_create(struct ev_loop *loop,
			struct ev_timer *timeout_watcher)
{
  ev_timer_init(timeout_watcher, toytimer_oneshot_callback, 0.0, 0.0);
  ev_timer_start(loop, timeout_watcher);
}

static void
toytimer_oneshot_callback(EV_P_ ev_timer *w, int revents)
{
  UNUSED(w); UNUSED(revents);
  toytimer_context_t toytimer_context = (toytimer_context_t) ud;

  printf("timer %d\n", toytimer_context->timer_count);

  // Validate input parameters
  RW_ASSERT_TYPE(toytimer_context, toytimer_context_t);

  // Validate the toytimer context parameters and update them
  EXPECT_EQ(toytimer_context->type, TOYTIMER_TYPE_ONESHOT);
  toytimer_context->timer_count++;

  // Stop this timer from running
  // This causes the inntermost ev_run to stop iterating
  ev_break(EV_A_ EVBREAK_ONE);

  // Create another oneshot toytimer
  toytimer_oneshot_create(loop, &g_timeout_watcher);
}
