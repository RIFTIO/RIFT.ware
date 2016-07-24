
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#if !defined(__RWSCHED_TOYSCHED_H_)
#define __RWSCHED_TOYSCHED_H_

#include <stdint.h>
#include <poll.h>
#include <glib.h>
#include "rwsched/cfrunloop.h"
#include "rwsched/cfsocket.h"

__BEGIN_DECLS

#include "rwsched.h"
#include "rwsched_main.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"
#include "rwsched_source.h"

#define RWMSG_TOYFD_MAX (128)
#define RWMSG_TOYTIMER_MAX (256)

typedef void (*toyfd_cb_t)(uint64_t id, int fd, int revents, void *ud);

typedef void (*toytimer_cb_t)(uint64_t id, void *ud);

struct rwmsg_toysched_s {
  rwsched_instance_ptr_t rwsched_instance;
  GAsyncQueue *runloop_queue;
};

struct rwmsg_event_context_s {
  enum {
    RWMSG_EVENT_CONTEXT_NONE,
    RWMSG_EVENT_CONTEXT_TOYFD,
    RWMSG_EVENT_CONTEXT_TOYTIMER
  } type;
  struct rwtoytask_tasklet_s *toytask;
  union rwmsg_runloop_source_u source;
  union {
    struct {
      struct rwmsg_toyfd_s *toyfd;
    } toyfd;
    struct {
      struct rwmsg_toytimer_s *toytimer;
    } toytimer;
  } u;
};

struct rwmsg_toyfd_context_s {
  struct rwtoytask_tasklet_s *toytask;
  struct rwmsg_toyfd_s *toyfd;
  rwsched_dispatch_source_t source;
};

#if 1

struct rwmsg_toyfd_s {
  CFRuntimeBase _base;
  uint64_t id;
  int fd;
  toyfd_cb_t cb;
  void *ud;
  struct rwtoytask_tasklet_s *toytask;
  struct rwmsg_event_context_s read_context;
  struct rwmsg_event_context_s write_context;
#ifdef _CF_
  rwsched_CFSocketRef cf_ref;
#endif // _CF_
};

RWSCHED_TYPE_DECL(rwmsg_toyfd);
RW_CF_TYPE_EXTERN(rwmsg_toyfd_ptr_t);

#endif

#if 1

struct rwmsg_toytimer_s {
  CFRuntimeBase _base;
  uint64_t id;
  toytimer_cb_t cb;
  void *ud;
  struct rwmsg_event_context_s context;
#ifdef _CF_
  CFRunLoopTimerRef cf_ref;
#endif // _CF_
};

RWSCHED_TYPE_DECL(rwmsg_toytimer);
RW_CF_TYPE_EXTERN(rwmsg_toytimer_ptr_t);

#endif

struct rwtoytask_tasklet_s {
  struct rwmsg_toysched_s *toysched;
  rwsched_dispatch_queue_t main_queue;
  struct {
    struct rwmsg_toyfd_s slot[RWMSG_TOYFD_MAX];
    int index;
  } toyfd;
  struct {
    struct rwmsg_toytimer_s slot[RWMSG_TOYTIMER_MAX];
    int index;
  } toytimer;
  struct {
    gboolean blocked;
    struct rwtoytask_tasklet_s *tt_blk;
    struct rwmsg_toytimer_s *toytimer;
    struct rwmsg_toyfd_s *toyfd;
    union rwmsg_runloop_source_u wakeup_source;
    GAsyncQueue *deferred_queue;
  } blocking_mode;
#ifdef _CF_
  CFStringRef deferred_cfrunloop_mode;
#endif // _CF_
  rwsched_tasklet_ptr_t rwsched_tasklet_info;
};
typedef struct rwmsg_toysched_s rwmsg_toysched_t;

RWSCHED_TYPE_DECL(rwtoytask_tasklet);
RW_CF_TYPE_EXTERN(rwtask_toytask_ptr_t);
typedef struct rwtoytask_tasklet_s rwmsg_toytask_t;

extern void
rwmsg_toysched_init(rwmsg_toysched_t *tsched);

extern void
rwmsg_toysched_deinit(rwmsg_toysched_t *tsched);

extern rwmsg_toytask_t *
rwmsg_toytask_create(rwmsg_toysched_t *ts);

void
rwmsg_toytask_destroy(rwmsg_toytask_t *toy);

extern uint64_t
rwmsg_toyfd_add(rwmsg_toytask_t *toy, int fd, int pollbits, toyfd_cb_t cb, void *ud);

extern uint64_t
rwmsg_toyfd_del(rwmsg_toytask_t *toy, uint64_t id);

extern uint64_t
rwmsg_toytimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud);

extern uint64_t
rwmsg_toytimer_del(rwmsg_toytask_t *toy, uint64_t id);

extern uint64_t
rwmsg_toyRtimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud);

extern uint64_t
rwmsg_toy1Ttimer_add(rwmsg_toytask_t *toy, int ms, toytimer_cb_t cb, void *ud);

extern void
rwmsg_toysched_run(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk);

extern void
rwmsg_toysched_runloop(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk, int max_events, double max_time);

extern int
rwmsg_toytask_block(rwmsg_toytask_t *toy, uint64_t id, int timeout_ms);

__END_DECLS

#endif // !defined(__RWSCHED_TOYSCHED_H_)
