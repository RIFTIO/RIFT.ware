
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */


/* 
   Trivial scheduler for testing dethreaded nn and rudimentary tasklet
   messaging and blocking.  None of libev, libevent, etc nest in the
   needed way, nor is rwsched off the ground yet, so here's this
   little thing.

   Toysched does descriptors and timers.  It does multiple tasklets,
   including blocking on fd, timeout, or both.

   It does not specifically do threads, however it has no globals so
   it can be run with a separate scheduler with one or more tasklets
   per thread.

   It's not fast and there are all sorts of arbitrary limits.  But
   then it took a few hours to write and it fits what I need.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>

#include <rwlib.h>
#include <rw_dl.h>
#include <rw_sklist.h>

#include "rwmsg_toysched.h"

/* Initialize the scheduler instance; call once */
void rwmsg_toysched_init(rwmsg_toysched_t *tsched) {
  memset(tsched, 0, sizeof(*tsched));
  tsched->idser = 1;
  tsched->tick = 1;
  gettimeofday(&tsched->tick_tv, NULL);
}

/* Internal */
static void rwmsg_toytask_init(rwmsg_toytask_t *toy, rwmsg_toysched_t *tsched) {
  int i;
  memset(toy, 0, sizeof(*toy));
  toy->tsched = tsched;
  for (i=0; i<RWMSG_TOYTIMER_MAX; i++) {
    toy->toytimer[i] = &toy->toytimerarr[i];
  }
}

/* Create a toytask.  Returns pointer or NULL if no go */
rwmsg_toytask_t *rwmsg_toytask_create(rwmsg_toysched_t *ts) {
  rwmsg_toytask_t *tt = NULL;
  if (ts->task_ct+1 < RWMSG_TOYTASK_MAX) {
    tt = &ts->task[ts->task_ct++];
    rwmsg_toytask_init(tt, ts);
  }
  return tt;
}

/* Register interest in events on an fd.  Save the returned ID, it is
   needed for unregister via rwmsg_toyfd_del() */
uint64_t rwmsg_toyfd_add(rwmsg_toytask_t *toy, int fd, int pollbits, void (*cb)(uint64_t id, int fd, int revents, void *ud), void *ud) {
  uint64_t rval = 0;
  if (toy->toyfd_ct < RWMSG_TOYFD_MAX) {
    struct _toyfd *tf = &toy->toyfd[toy->toyfd_ct];
    toy->toyfd_ct++;

    rval = tf->id = toy->tsched->idser++;
    tf->interest.fd = fd;
    tf->interest.events = pollbits;
    tf->interest.revents = 0;
    tf->cb = cb;
    tf->ud = ud;    
  }
  return rval;
}

/* Internal */
static int rwmsg_toyfd_find(rwmsg_toytask_t *toy, uint64_t id) {
  int i;
  for (i=0; i<toy->toyfd_ct; i++) {
    if (toy->toyfd[i].id == id) {
      return i;
    }
  }
  return -1;
}

/* Internal */
static int rwmsg_toyfd_findfd(rwmsg_toytask_t *toy, int fd) {
  int i;
  for (i=0; i<toy->toyfd_ct; i++) {
    if (toy->toyfd[i].interest.fd == fd) {
      return i;
    }
  }
  return -1;
}

/* Unregister this fd event, by ID returned from _add() */ 
uint64_t rwmsg_toyfd_del(rwmsg_toytask_t *toy, uint64_t id) {
  uint64_t rval = 0;
  int idx = rwmsg_toyfd_find(toy, id);
  if (idx >= 0) {
    rval = toy->toyfd[idx].id;
    if (idx < toy->toyfd_ct-1) {
      /* Copy the last one into this slot */
      memcpy(&toy->toyfd[idx],
	     &toy->toyfd[toy->toyfd_ct-1],
	     sizeof(toy->toyfd[idx]));
    }
    toy->toyfd_ct--;
  }
  return rval;
};

/* Internal */
static int rwmsg_toytimer_cmp(const void *a, const void *b) {
  struct _toytimer *t1, *t2;
  t1 = *(struct _toytimer **)a;
  t2 = *(struct _toytimer **)b;
  if (t1->tick_due < t2->tick_due) {
    return -1;
  } else if (t1->tick_due > t2->tick_due) {
    return 1;
  } else {
    return 0;
  }
}

/* Internal */
static int rwmsg_toytimer_find(rwmsg_toytask_t *toy, uint64_t id) {
  int i;
  for (i=0; i<toy->toytimer_ct; i++) {
    if (toy->toytimer[i]->id == id) {
      return i;
    }
  }
  return -1;
}

/* Add a one-shot timer */
uint64_t rwmsg_toytimer_add(rwmsg_toytask_t *toy, int ms, void (*cb)(uint64_t id, void *ud), void *ud) {
  uint64_t rval = 0;
  if (toy->toytimer_ct < RWMSG_TOYTIMER_MAX) {
    struct _toytimer *tt = toy->toytimer[toy->toytimer_ct];
    toy->toytimer_ct++;

    rval = tt->id = toy->tsched->idser++;
    tt->cb = cb;
    tt->ud = ud;
    tt->tick_due = toy->tsched->tick + (ms / RWMSG_TOYTICK_PERIOD_MS);

    qsort(&toy->toytimer[0], toy->toytimer_ct, sizeof(toy->toytimer[0]), rwmsg_toytimer_cmp);
    if (toy->toytimer_ct >= 2) {
      RW_ASSERT(toy->toytimer[1]->tick_due >= toy->toytimer[0]->tick_due);
    }
  }
  return rval;
}

/* Delete a one-shot timer, by ID as returned from _add() */
uint64_t rwmsg_toytimer_del(rwmsg_toytask_t *toy, uint64_t id) {
  uint64_t rval = 0;
  int idx = rwmsg_toytimer_find(toy, id);
  if (idx >= 0) {
    rval = toy->toytimer[idx]->id;
    struct _toytimer *tt = toy->toytimer[idx];
    if (idx+1 < toy->toytimer_ct) {
      /* Move all the later timers down one slot */
      memmove(&toy->toytimer[idx],
	      &toy->toytimer[idx+1],
	      sizeof(toy->toytimer[idx]) * (toy->toytimer_ct - 1 - idx));
    }
    toy->toytimer_ct--;
    toy->toytimer[toy->toytimer_ct] = tt; /* Don't drop the pointer to this old one */
  }
  return rval;
}

/* Internal */
static int rwmsg_toytask_get_pollset(rwmsg_toytask_t *toy, struct pollfd *pset_out, int pset_max) {
  RW_ASSERT(pset_max >= toy->toyfd_ct);
  int ct = 0;
  if (toy->blocking.blocking) {
    /* Just the one we're waiting for */
    if (toy->blocking.tfd_idx >= 0) {
      int i = toy->blocking.tfd_idx;
      toy->toyfd[i].interest.revents = 0;
      memcpy(&pset_out[0], &toy->toyfd[i].interest, sizeof(pset_out[0]));
      ct = 1;
    }
  } else {
    /* Our whole pollset */
    int i;
    for (i=0; i<toy->toyfd_ct; i++) {
      toy->toyfd[i].interest.revents = 0;
      memcpy(&pset_out[i], &toy->toyfd[i].interest, sizeof(pset_out[0]));
    }
    ct = i;
  }
  return ct;
}

/* Internal */
static uint64_t rwmsg_toytask_get_duetick(rwmsg_toytask_t *toy) {
  uint64_t rval = 0;
  if (toy->blocking.blocking) {
    rval = toy->blocking.tick_due;
  } else {
    if (toy->toytimer_ct) {
      rval = toy->toytimer[0]->tick_due;
    }
  }
  return rval;
}

/* Internal.  Time advanced; run stuff.  Returns TRUE to exit
   scheduler immediately, for blocking */
static int rwmsg_toytask_tick(rwmsg_toytask_t *toy, uint64_t newtick) {
  int rval = FALSE;
  if (toy->blocking.blocking) {
    if (toy->blocking.tick_due <= newtick) {
      toy->blocking.blocking = 0;
      toy->blocking.revents = 0;
      rval = TRUE;
    }
  } else {
    while (toy->toytimer_ct && toy->toytimer[0]->tick_due <= newtick) {
      void (*cb)(uint64_t, void *) = toy->toytimer[0]->cb;
      void *ud = toy->toytimer[0]->ud;
      uint64_t id = toy->toytimer[0]->id;
      rwmsg_toytimer_del(toy, id);
      cb(id, ud);
    }
  }
  return rval;
}

/* Internal.  Descriptor event; run stuff.  Returns TRUE to exit
   scheduler immediately, for blocking */
static int rwmsg_toytask_fdevent(rwmsg_toytask_t *toy, int fd, int revents) {
  int rval = FALSE;
  int idx = rwmsg_toyfd_findfd(toy, fd);
  if (idx >= 0) {
    if (toy->blocking.blocking) {
      if (idx == toy->blocking.tfd_idx) {
	toy->blocking.blocking = 0;
	toy->blocking.revents = revents;
	rval = TRUE;
      }
    } else {
      struct _toyfd *tf = &toy->toyfd[idx];
      uint64_t id = tf->id;
      void (*cb)(uint64_t, int, int, void *) = tf->cb;
      void *ud = tf->ud;
      cb(id, fd, revents, ud);
    }
  }
  return rval;
}

/* Internal. */
int rwmsg_toysched_dotime(rwmsg_toysched_t *ts) {
    struct timeval tv_now, tv_elapsed;
    gettimeofday(&tv_now, NULL);
    timersub(&tv_now, &ts->tick_tv, &tv_elapsed);
    int elapsed_ms = tv_elapsed.tv_sec * 1000 + tv_elapsed.tv_usec / 1000;
    int elapsed_ticks = elapsed_ms / RWMSG_TOYTICK_PERIOD_MS;
    ts->elapsed_ms_subtick = elapsed_ms % RWMSG_TOYTICK_PERIOD_MS;
    if (elapsed_ticks) {
      ts->tick += elapsed_ticks;
      struct timeval tv_round, tv_tmp;
      tv_round.tv_sec = elapsed_ticks / RWMSG_TOYTICK_HZ;
      tv_round.tv_usec = (elapsed_ticks % RWMSG_TOYTICK_HZ) * RWMSG_TOYTICK_PERIOD_MS * 1000;
      timeradd(&ts->tick_tv, &tv_round, &tv_tmp);
      ts->tick_tv = tv_tmp;
    }
    return !!elapsed_ticks;
}

/* Run the scheduler.  Pass NULL for tt_blk */
void rwmsg_toysched_run(rwmsg_toysched_t *ts, rwmsg_toytask_t *tt_blk) {
  rwmsg_toysched_pollset_t *ps;
  int i=0;

  ps = malloc(sizeof(rwmsg_toysched_pollset_t));
  memset(ps, 0, sizeof(*ps));

  while (1) {

    /* Assemble pollset */
    rwmsg_toysched_dotime(ts);
    int poll_timeo = 5000;
    uint64_t min_tick = ~0ULL;
    ps->pollset_ct = 0;
    for (i=0; i<ts->task_ct; i++) {
      rwmsg_toytask_t *tt = &ts->task[i];
      ps->pollset_tasktab[i] = ps->pollset_ct;
      if (!tt->blocking.blocking || tt == tt_blk) {
	uint64_t duetick = rwmsg_toytask_get_duetick(tt);
	if (duetick) {
	  min_tick = MIN(duetick, min_tick);
	}
	ps->pollset_ct += rwmsg_toytask_get_pollset(tt, &ps->pollset[ps->pollset_ct], RWMSG_TOYSCHED_POLL_MAX-ps->pollset_ct);
      }
    }
    if (min_tick != ~0ULL) {
      int slp_ticks = MAX(0, ((int)(min_tick - ts->tick)));
      poll_timeo = slp_ticks * RWMSG_TOYTICK_PERIOD_MS;
      if (slp_ticks > 0) {
	/* The next tick is already partly elapsed, adjust accordingly */
	poll_timeo -= RWMSG_TOYTICK_PERIOD_MS;
	poll_timeo += (RWMSG_TOYTICK_PERIOD_MS - ts->elapsed_ms_subtick);
      }
    }

    /* Get events */
    int r = poll(ps->pollset, ps->pollset_ct, poll_timeo);
    if (r < 0) { 
      switch (errno) {
      case EINTR:
	break;

      default:
	printf("rwmsg_toysched_run poll(2) err %d %s\n", errno, strerror(errno));
	exit(1);
	break;
      }
    } else {
      /* Have events, maybe */
      if (0) {
	int evct=0;
	int tidx=0;
	for (i=0; i<ps->pollset_ct && evct < r; i++) {
	  rwmsg_toytask_t *tt;
	  if (tidx+1 < ts->task_ct && ps->pollset_tasktab[tidx+1] <= i) {
	    tidx++;
	  }
	  if (ps->pollset[i].revents) {
	    evct++;
	    tt = &ts->task[tidx];
	    if (!tt->blocking.blocking || tt == tt_blk) {
	      printf("toypoll fd=%d revents=%u in scheduled %s tt\n", 
		     ps->pollset[i].fd, ps->pollset[i].revents, tt->blocking.blocking ? "BLOCKING" : "nonblocking");
	    } else {
	      printf("toypoll fd=%d revents=%u in unscheduled blocking tt\n", ps->pollset[i].fd, ps->pollset[i].revents);
	    }
	  }
	}
	if (evct) {
	  printf("toypoll end fd events list\n");
	}
      }

      /* Update time */
      rwmsg_toysched_dotime(ts);

      /* Run timers */
      for (i=0; i<ts->task_ct; i++) {
	rwmsg_toytask_t *tt = &ts->task[i];
	if (!tt->blocking.blocking || tt == tt_blk) {
	  if (rwmsg_toytask_tick(tt, ts->tick)) {
	    goto out;
	  }
	}
      }

      /* Run fd events */
      int tidx=0;
      int evct=0;
      for (i=0; i<ps->pollset_ct && evct < r; i++) {
	rwmsg_toytask_t *tt;
	if (tidx+1 < ts->task_ct && ps->pollset_tasktab[tidx+1] <= i) {
	  tidx++;
	}
	if (ps->pollset[i].revents) {
	  evct++;
	  tt = &ts->task[tidx];
	  if (!tt->blocking.blocking || tt == tt_blk) {
	    if (rwmsg_toytask_fdevent(tt, ps->pollset[i].fd, ps->pollset[i].revents)) {
	      goto out;
	    }
	  }
	}
      }
    }
  }

 out:
  free(ps);
}

/* Block waiting for toyfd event id or timeout.  Either can be zero,
   but not both.  Returns revents from the fd, or 0 for timeout. */
int rwmsg_toytask_block(rwmsg_toytask_t *toy, uint64_t id, int timeout_ms) {
  RW_ASSERT(!toy->blocking.blocking);
  RW_ASSERT(id || timeout_ms);
  int revents = 0;
  int idx = rwmsg_toyfd_find(toy, id);
  if (idx >= 0 || id == 0) {
    toy->blocking.blocking = 1;
    toy->blocking.tfd_idx = idx;
    toy->blocking.tick_due = timeout_ms == 0 ? 0 : (toy->tsched->tick + (timeout_ms * 1000 / RWMSG_TOYTICK_HZ));
    toy->blocking.revents = 0;

    rwmsg_toysched_run(toy->tsched, toy);

    if (idx >= 0 && toy->blocking.tfd_idx == idx) {
      revents = toy->blocking.revents;
    }
  }
  return revents;
}

