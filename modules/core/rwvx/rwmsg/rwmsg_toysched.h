
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __rwmsg_toysched_h
#define __rwmsg_toysched_h

#define RWMSG_TOYTASK_MAX (64)
#define RWMSG_TOYFD_MAX (128)
#define RWMSG_TOYTIMER_MAX (256)
#define RWMSG_TOYSCHED_POLL_MAX (1024)
#define RWMSG_TOYTICK_HZ (40)
#define RWMSG_TOYTICK_PERIOD_MS (1000 / RWMSG_TOYTICK_HZ)

typedef struct rwmsg_toysched rwmsg_toysched_t;

typedef struct {
  rwmsg_toysched_t *tsched;

  struct {
    int blocking;        // true if waiting on fd and/or timeout
    int tfd_idx;         // can be -1 => no fd!
    uint64_t tick_due;   // can be 0  => forever!
    int revents;
  } blocking;

  struct _toyfd {
    struct pollfd interest;

    uint64_t id;
    void (*cb)(uint64_t id, int fd, int revents, void *ud);
    void *ud;

  } toyfd[RWMSG_TOYFD_MAX];
  int toyfd_ct;

  struct _toytimer {
    uint64_t tick_due;
    
    uint64_t id;
    void (*cb)(uint64_t id, void *ud);
    void *ud;
  } toytimerarr[RWMSG_TOYTIMER_MAX];
  struct _toytimer *toytimer[RWMSG_TOYTIMER_MAX];
  int toytimer_ct;

} rwmsg_toytask_t;

typedef struct {
  struct pollfd pollset[RWMSG_TOYSCHED_POLL_MAX];
  int pollset_ct;
  int pollset_tasktab[RWMSG_TOYTASK_MAX]; /* idx of first pollset[] item for each task */
} rwmsg_toysched_pollset_t;

struct rwmsg_toysched {
  rwmsg_toytask_t task[RWMSG_TOYTASK_MAX];
  int task_ct;

  uint64_t tick;
  struct timeval tick_tv;
  int elapsed_ms_subtick;

  uint64_t idser;

};

extern void rwmsg_toysched_init(rwmsg_toysched_t *tsched);
extern rwmsg_toytask_t *rwmsg_toytask_create(rwmsg_toysched_t *ts);
extern uint64_t rwmsg_toyfd_add(rwmsg_toytask_t *toy, int fd, int pollbits, void (*cb)(uint64_t id, int fd, int revents, void *ud), void *ud);
extern uint64_t rwmsg_toyfd_del(rwmsg_toytask_t *toy, uint64_t id);
extern uint64_t rwmsg_toytimer_add(rwmsg_toytask_t *toy, int ms, void (*cb)(uint64_t id, void *ud), void *ud);
extern uint64_t rwmsg_toytimer_del(rwmsg_toytask_t *toy, uint64_t id);
extern void rwmsg_toysched_run(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk);
extern int rwmsg_toytask_block(rwmsg_toytask_t *toy, uint64_t id, int timeout_ms);

#endif
