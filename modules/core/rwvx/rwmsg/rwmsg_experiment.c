
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */


/* Phase 0 nanomsg + tasklet investigation app

   This demonstrates a) tasklets b) nanomsg (generic) inproc, for the
   purposes of validating nanomsg functionality and experimenting with
   possible topologies.

   Results to date:

     - With 40 tasklets it runs at 25kmps, fine for our skeletal cli
       demo.  The toy scheduler and naive nn topology are equally
       responsible for the slowness.

     - Blocking requests get 100-300us rtt in this laden, 40 tasklet
       scenario.

     - Topology-wise, XPUB <= DEVICE <= XSUB just doesn't work.  It
       would require a thread anyway as the nn_device mechanism is
       blocking.

     - PUB <- trivial "broker" <- NN_PULL does work, broker as a
       tasklet with no special treatment seems to work fine.

     - NN is 100% valgrind clean.

     - Drops, NN_PUB doesn't do flow control/queue/etc.  Probably good
       enough for cli purposes, but awkward in phase 0 since they go
       unobserved and there aren't retransmits.  Cranking the recv
       buffer in the blocking task works OK for this test program.

   TBD:

     - Put together the real rwmsg API header, glue it together with
       toysched, and get on with Phase 0.


*/

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#include <rwlib.h>
#include <rw_rand.h>

#include "rwmsg_toysched.h"

#define GETTID() syscall(SYS_gettid)

int getprocstatusval(const char *what){
  FILE* file = fopen("/proc/self/status", "r");
  int result = -1;
  char line[128];
  int wlen = strlen(what);

  while (fgets(line, 128, file) != NULL){
    if (strncmp(line, what, wlen)==0 && line[wlen]==':') {
      result = atoi(line+wlen+1);
      break;
    }
  }
  fclose(file);
  return result;
}

/******************************************/

#define RUNTIME_SECONDS 90        /* Run for this long */
#define TASKCT 10                 /* Run this many tasklets */
#define MSG_PER_TASK_NOMINAL 10   /* Keep this many requests outstanding per tasklet */
#define DOBLOCKING TRUE           /* Make the first task created send blocking requests */
#define BLOCKER_DESTWEIGHT 10     /* Send only X per thousand (ie 10==1%) as many messages to the blocking task */
#define USE_NN_MSG TRUE           /* Use NN-allocated recv buffers */
#define BROKER_DROP FALSE         /* Drop all messages arriving at broker */
#define NN_WORKER_TASKLET FALSE   /* Move NN worker thread work into the broker tasklet */ 

#if 0
#define BROKER_ADDR_PULL "inproc://testbro"
#define BROKER_ADDR_PUB  "inproc://testpub"
#define MSG_PER_TASK ( MSG_PER_TASK_NOMINAL * 100 / 100 )
#elif 1
#define BROKER_ADDR_PULL "ipc://testbro"
#define BROKER_ADDR_PUB  "ipc://testpub"
#define MSG_PER_TASK ( MSG_PER_TASK_NOMINAL * 20 / 100 )
#else
#define BROKER_ADDR_PULL "tcp://127.0.0.2:9999"
#define BROKER_ADDR_PUB  "tcp://127.0.0.2:9998"
#define MSG_PER_TASK ( MSG_PER_TASK_NOMINAL * 20 / 100 )
#endif

#if 0
#define TRACE(args...) printf(args)
#else
#define TRACE(args...)
#endif

#define PATHLEN 24

struct tasklet {
  int task_idx;
  pid_t task_tid;
  char task_path[PATHLEN];
  char task_bpath[PATHLEN];

  struct {
    int pub;
    int pub_eid;

    int sub;
    int sub_eid;
    int sub_efd;

    int bsub;
    int bsub_eid;
    int bsub_efd;
  } nn;

  rwmsg_toytask_t *toy;

  uint32_t nxserial;

#define MAXBURST (120)

  struct expstat {
    uint64_t timer1_evct;
    uint64_t timer1_sendmore_evct;
    uint64_t sockevent1_evct;
    uint64_t sockevent1_evct_bsub;

    uint64_t req_sent;
    uint64_t req_sent_blocking;
    uint64_t req_send_again;
    uint64_t req_send_err;
    uint64_t req_send_err_lasterrno;
    uint64_t req_sent_pertimev[MAXBURST+1];
    uint64_t msg_recv;
    uint64_t msg_recv_again;
    uint64_t msg_recv_err;
    uint64_t msg_recv_err_lasterrno;
    uint64_t msg_recv_perskev[16];
    uint64_t msg_toss;
    uint64_t req_recv;
    uint64_t req_recv_blocking;
    uint64_t rsp_sent;
    uint64_t rsp_sent_blocking;
    uint64_t rsp_sent_again;
    uint64_t rsp_sent_err;
    uint64_t rsp_sent_err_lasterrno;
    uint64_t rsp_recv;
    uint64_t rsp_recv_blocking;
    uint64_t rsp_recv_blocking_unk;
    uint64_t blocking_timeouts;
    uint64_t blocking_elapsed_sum;
    uint64_t blocking_elapsed_ct;
    uint64_t blocking_elapsed_max;
  } stat;

  uint64_t id_timer1;
  uint64_t id_sockevent1;
  uint64_t id_sockevent1b;
  
  int blocker;
  uint32_t wantblockmsg;
};

static struct {
  rwmsg_toytask_t *toy;

  int xsub;
  int xpub;
  
  int xsub_efd;
  uint64_t id_sockevent1;
  uint64_t id_runtimer;

  struct brostat {
    uint64_t msg_recv;
    uint64_t msg_read_again;
    uint64_t msg_sent;
    uint64_t msg_sent_again[16];
    uint64_t msg_send_err;
    uint64_t msg_send_err_lasterrno;

    uint64_t nn_fdevent;
    uint64_t nn_timer;
    uint64_t nn_timerlen_inf;
    uint64_t nn_timerlen_zero;
    uint64_t nn_timerlen_pos;

  } stat;

  uint64_t runtime_ticks;
  uint64_t ticks;
  int halt;
  int halt_ticks;

  int valgrind;

  struct expstat global_stats;

  int nn_timeout;
  int nn_fd;
  int nn_events;

  uint64_t nn_fdid;
  uint64_t nn_timerid;

  struct timeval tv_start;

} bk;

typedef struct {
  uint32_t rwmsg_ser;
  enum {
    REQ,
    RSP
  } rwmsg_type;
  char rwmsg_ret[PATHLEN];
} rwmsg_header_t;

#define PERIOD_MS 100

static struct {
  // mutex
  char *tasklet[999];
  struct tasklet *self[999];
  int tasklet_ct;
} tasklets;

// fwd
static void timer1(uint64_t id, void *ud);
static void broker_toevent(uint64_t id, void *ud);


static void print_stats(struct expstat *s, struct brostat *b) {
  printf("  Tasklet(s):\n");
  printf("    timer1_evct = %lu\n", s->timer1_evct);
  printf("    timer1_sendmore_evct = %lu\n", s->timer1_sendmore_evct);
  printf("    sockevent1_evct = %lu\n", s->sockevent1_evct);
  printf("    sockevent1_evct_bsub = %lu\n", s->sockevent1_evct_bsub);

  printf("    req_sent = %lu\n", s->req_sent);
  printf("    req_sent_blocking = %lu\n", s->req_sent_blocking);
  printf("    req_send_again = %lu\n", s->req_send_again);
  printf("    req_send_err = %lu\n", s->req_send_err);
  printf("    req_send_err_lasterrno = %lu\n", s->req_send_err_lasterrno);
  //  printf("    req_sent_pertimev[MAXBURST+1] = %lu\n", s->req_sent_pertimev[MAXBURST+1]);
  printf("    msg_recv = %lu\n", s->msg_recv);
  printf("    msg_recv_again = %lu\n", s->msg_recv_again);
  printf("    msg_recv_err = %lu\n", s->msg_recv_err);
  printf("    msg_recv_err_lasterrno = %lu\n", s->msg_recv_err_lasterrno);
  printf("    msg_recv_perskev[0..15] = { %lu", s->msg_recv_perskev[0]);
  int i;
  for (i=1; i<16; i++) {
    printf(", %lu", s->msg_recv_perskev[i]);
  }
  printf(" }\n    msg_toss = %lu\n", s->msg_toss);
  printf("    req_recv = %lu\n", s->req_recv);
  printf("    req_recv_blocking = %lu\n", s->req_recv_blocking);
  printf("    rsp_sent = %lu\n", s->rsp_sent);
  printf("    rsp_sent_blocking = %lu\n", s->rsp_sent_blocking);
  printf("    rsp_sent_again = %lu\n", s->rsp_sent_again);
  printf("    rsp_sent_err = %lu\n", s->rsp_sent_err);
  printf("    rsp_sent_err_lasterrno = %lu\n", s->rsp_sent_err_lasterrno);
  printf("    rsp_recv = %lu\n", s->rsp_recv);
  printf("    rsp_recv_blocking = %lu\n", s->rsp_recv_blocking);
  printf("    rsp_recv_blocking_unk = %lu\n", s->rsp_recv_blocking_unk);
  printf("    blocking_timeouts = %lu\n", s->blocking_timeouts);
  printf("    blocking_elapsed_max = %lu us\n", s->blocking_elapsed_max);
  printf("    blocking_elapsed_avg = %lu us\n", s->blocking_elapsed_ct ? s->blocking_elapsed_sum / s->blocking_elapsed_ct : 0);
  if (b) {
    printf("  Broker:\n");
    printf("    msg_recv = %lu\n", b->msg_recv);
    printf("    msg_read_again = %lu\n", b->msg_read_again);
    printf("    msg_sent = %lu\n", b->msg_sent);
    printf("    msg_sent_again = { %lu", b->msg_sent_again[0]);
    for (i=1; i<16; i++) {
      printf(", %lu", b->msg_sent_again[i]);
    }
    printf(" }\n    msg_send_err = %lu\n", b->msg_send_err);
    printf("    msg_send_err_lasterrno = %lu '%s'\n", b->msg_send_err_lasterrno, strerror((int)b->msg_send_err_lasterrno));
    printf("    nn_fdevent = %lu\n", b->nn_fdevent);
    printf("    nn_timer = %lu\n", b->nn_timer);
    printf("    nn_timerlen_inf = %lu\n", b->nn_timerlen_inf);
    printf("    nn_timerlen_zero = %lu\n", b->nn_timerlen_zero);
    printf("    nn_timerlen_pos = %lu\n", b->nn_timerlen_pos);
  }
}
static void total_stats(struct expstat *tin) {
  RW_ASSERT(tin);
  memset(tin, 0, sizeof(*tin));

  int ct = sizeof(*tin) / sizeof(uint64_t);
  uint64_t *t = (uint64_t *)tin;

  // mutex
  int i;
  for (i=0; i<tasklets.tasklet_ct; i++) {
    uint64_t *t2 = (uint64_t*)&tasklets.self[i]->stat;
    int j;
    for (j=0; j<ct; j++) {
      t[j] += t2[j];
    }
  }
}

static const char *random_tasklet(const char *notme) {
  // mutex
  int idx;
  const char *rval = NULL;
  int nope = FALSE;
  if (tasklets.tasklet_ct > (notme ? 1 : 0)) {
    do {
      nope = FALSE;
      idx = rw_random() % tasklets.tasklet_ct;
      if (tasklets.self[idx]->blocker) {
	/* Send only a 10 percent share of messages to the blocking task */
	nope = (rw_random() % 1000) < (1000 - BLOCKER_DESTWEIGHT);
      }
    } while ((notme && 0==strcmp(notme, tasklets.tasklet[idx])) || nope);
    rval =  tasklets.tasklet[idx];
  }
  return rval;
}

static void push_tasklet(const char *path, struct tasklet *self) {
  // mutex
  tasklets.tasklet[tasklets.tasklet_ct] = strdup(path);
  tasklets.self[tasklets.tasklet_ct] = self;
  tasklets.tasklet_ct++;
}


static void sockevent1(uint64_t id, int fd, int revents, void *ud) {
  struct tasklet *self = (struct tasklet *)ud;

  //TRACE("[%s] sockevent1() id=%lx fd=%d revents=%x\n", self->task_path, id, fd, revents);

  self->stat.sockevent1_evct++;

  int recvct = 0;

  if (revents & POLLIN) {

    int nnsk = -1;
    if (fd == self->nn.sub_efd) {
      nnsk = self->nn.sub;
    } else if (fd == self->nn.bsub_efd) {
      nnsk = self->nn.bsub;
      self->stat.sockevent1_evct_bsub++;
    }

    if (nnsk >= 0) {
      int r;
#if USE_NN_MSG
      char *nnmsg = NULL;
      r = nn_recv(nnsk, &nnmsg, NN_MSG, NN_DONTWAIT);
#else
      char nnmsg[2048];
      r = nn_recv(nnsk, &nnmsg, sizeof(nnmsg), NN_DONTWAIT);
#endif
      if (r < 0) {
	switch (errno) {
	case EAGAIN:
	case EINTR:
	  self->stat.msg_recv_again++;
#if USE_NN_MSG
	  RW_ASSERT(!nnmsg);
#endif
	  break;
	default:
	  self->stat.msg_recv_err++;
	  self->stat.msg_recv_err_lasterrno = errno;
	  printf("[%s] + nn_recv(sk=%d) err %d %s\n", self->task_path, nnsk, errno, strerror(errno));
	}
      } else {
	recvct++;
	self->stat.msg_recv++;
	int i=0;
	while (i < r && nnmsg[i++]) { }
	if ((i + (int)sizeof(rwmsg_header_t)) <= r) {
	  rwmsg_header_t *hdr = (rwmsg_header_t *)&nnmsg[i];
	  char *payload = &nnmsg[i+sizeof(rwmsg_header_t)];
	  int paylen = r - (i + sizeof(rwmsg_header_t));
	  paylen=paylen;
	  payload=payload;
	  int toss = (self->wantblockmsg && hdr->rwmsg_type == RSP && hdr->rwmsg_ser != self->wantblockmsg);
	  TRACE ("[%s] + %s nn_recv(sk=%d/%s) to '%s' ser %u typ %s ret '%s' pay len %d '%s' \n",
		  self->task_path,
		  toss ? "TOSS" : "",
		  nnsk,
		  (nnsk == self->nn.bsub ? "bsub" : nnsk == self->nn.sub ? "sub" : "???"),
		  nnmsg,
		  hdr->rwmsg_ser,
		  hdr->rwmsg_type == REQ ? "REQ" : hdr->rwmsg_type == RSP ? "RSP" : "XXX",
		  hdr->rwmsg_ret,
		  paylen,
		  payload);

	  if (!toss) {
	    switch (hdr->rwmsg_type) {
	    case RSP:
	      self->stat.rsp_recv++;
	      if (nnsk == self->nn.bsub) {
		if (hdr->rwmsg_ser == self->wantblockmsg) {
		  self->stat.rsp_recv_blocking++;
		  self->wantblockmsg = 0;
		  /* CAUTION need to return right away */
		} else {
		  self->stat.rsp_recv_blocking_unk++;
		}
	      } else {
		/* Call timer1 to put some more messages outstanding.  Do not block. */
		int saveblocker = self->blocker;
		self->blocker = 0;
		timer1(0, self);
		self->blocker = saveblocker;
	      }
	    default:
	      break;
	    case REQ:
	      {
		int blocking =  (hdr->rwmsg_ret[3] == 'B');
		self->stat.req_recv++;
		if (blocking) {
		  self->stat.req_recv_blocking++;
		}
		if (0 && blocking) {
		  /* don't answer, to try timeout */
		  goto meh;
		}
		hdr->rwmsg_type = RSP;
		struct nn_iovec iov[3] = {
		  { .iov_base = hdr->rwmsg_ret, .iov_len = strlen(hdr->rwmsg_ret)+1 },
		  { .iov_base = hdr, .iov_len = sizeof(*hdr) },
		  { .iov_base = "This is a response payload.", .iov_len = 28 }
		};
		struct nn_msghdr msghdr = {
		  .msg_iov = iov,
		  .msg_iovlen = 3
		};
		
		TRACE("[%s] + nn_sendmsg sent %s rsp ser %u to %s\n", 
		       self->task_path, 
		       blocking ? "blocking" : "nonblocking",
		       hdr->rwmsg_ser,
		       hdr->rwmsg_ret);

		int r = nn_sendmsg(self->nn.pub, &msghdr, NN_DONTWAIT);
		if (r < 0) {
		  switch (errno) {
		  case EAGAIN:
		  case EINTR:
		    self->stat.rsp_sent_again++;
		    // TBD
		    break;
		  default:
		    self->stat.rsp_sent_err++;
		    self->stat.rsp_sent_err_lasterrno = errno;
		    printf("[%s] + nn_sendmsg errno=%u %s\n", self->task_path, errno, strerror(errno));
		    break;
		  }
		} else {
		  self->stat.rsp_sent++;
		  if (blocking) {
		    self->stat.rsp_sent_blocking++;
		  }
		}
	      }
	      break;
	    }
	  } else {
	    self->stat.msg_toss++;
	  }

	} else {
	  printf ("[%s] + nn_recv funk len?\n", self->task_path);
	}

      meh:
#if USE_NN_MSG
	nn_freemsg(nnmsg);
#endif
	;
      }
    } else {
      printf("[%s] + unk fd value?\n", self->task_path);
      RW_CRASH();
      abort();
    }
  }

  self->stat.msg_recv_perskev[MIN(15,recvct)]++;
  return;

  id=id;
}

static void timer1(uint64_t id, void *ud) {
  struct tasklet *self = (struct tasklet *)ud;
  id=id;

  if (id) {
    self->stat.timer1_evct++;
  } else {
    self->stat.timer1_sendmore_evct++;
  }

  //  TRACE("[%s] timer1() id=%lx\n", self->task_path, id);

  // send msg
  int msgsnt=0;
  int msgct = 1;
  int blocking = self->blocker;
  if (!blocking) {

    //uncomment to do blocking only    goto nope;

    int mx = bk.valgrind ? 3 : MAXBURST;
    int cap = (MSG_PER_TASK / ( bk.valgrind ? 10 : 1)) - (self->stat.req_sent - self->stat.rsp_recv);
    mx = MAX(0, MIN(cap, mx));
    if (mx) {
      msgct = rw_random() % mx;
    } else {
      msgct = 0;
    }

    if (BROKER_DROP) {
      if (id) {
	msgct = 10;
      }
    }

  }
  if (bk.halt) {
    msgct = 0;
  }

  while (msgct--) {
    rwmsg_header_t hdr = {
      .rwmsg_ser = self->nxserial++,
      .rwmsg_type = REQ,
    };
    strncpy(hdr.rwmsg_ret, blocking ? self->task_bpath : self->task_path, sizeof(hdr.rwmsg_ret));
    char *payload = "This is a request payload.";
    const char *path = random_tasklet(blocking ? self->task_path : NULL);
    if (!path) {
      goto nope;
    }
    struct nn_iovec iov[3] = {
      { .iov_base = (void*)path, .iov_len = strlen(path)+1 },
      { .iov_base = &hdr, .iov_len = sizeof(hdr) },
      { .iov_base = payload, .iov_len = strlen(payload)+1 }
    };
    struct nn_msghdr msghdr = {
      .msg_iov = iov,
      .msg_iovlen = 3
    };
    
    int r = nn_sendmsg(self->nn.pub, &msghdr, NN_DONTWAIT);
    if (r < 0) {
      switch (errno) {
      case EAGAIN:
      case EINTR:
	// TBD
	self->stat.req_send_again++;
	break;
      default:
	self->stat.req_send_err++;
	self->stat.req_send_err_lasterrno = errno;
	printf("[%s] + nn_sendmsg errno=%u %s\n", self->task_path, errno, strerror(errno));
	break;
      }
    } else {
      self->stat.req_sent++;
      msgsnt++;
      TRACE("[%s] + nn_sendmsg sent %s req ser %u to %s\n", 
	     self->task_path, 
	     blocking ? "blocking" : "nonblocking",
	     hdr.rwmsg_ser,
	     path);
      
      if (blocking) {
	self->stat.req_sent_blocking++;
	int tval = time(NULL);
	int tval_done = tval + 60;
	int blocking_timeout_ms = ( tval_done - tval ) * 1000;
      again:
	TRACE("[%s] waiting on fd %d sk %d sub %s for blocking response to REQ %u from %s timeout %ums\n", 
	       self->task_path,
	       self->nn.bsub_efd,
	       self->nn.bsub,
	       self->task_bpath,
	       hdr.rwmsg_ser,
	       path,
	       blocking_timeout_ms);
	struct timeval tv1, tv2, delta;
	gettimeofday(&tv1, NULL);
	int revents = rwmsg_toytask_block(self->toy, self->id_sockevent1b, blocking_timeout_ms);
	if (revents) {
	  /* Synthesize the fd event we have in hand */
	  self->wantblockmsg = hdr.rwmsg_ser;
	  sockevent1(self->id_sockevent1b, self->nn.bsub_efd, revents, self);
	  gettimeofday(&tv2, NULL);
	  timersub(&tv2, &tv1, &delta);
	  int elapsed_us = delta.tv_sec * 1000000 + delta.tv_usec;
	  if (self->wantblockmsg) {
	    RW_ASSERT(self->wantblockmsg == hdr.rwmsg_ser);
	    tval = time(NULL);
	    if (tval >= tval_done) {
	      self->wantblockmsg = 0;
	      goto blktimo;
	    } else {
	      blocking_timeout_ms = (tval_done - tval ) * 1000;
	      TRACE("[%s] blocking woke revents=%d noop elapsed=%dus\n", 
		    self->task_path, 
		    revents,
		    elapsed_us);
	      goto again;
	    }
	  }
	  TRACE("[%s] blocking FINISHED revents=%d noop elapsed=%dus\n", 
		self->task_path, 
		revents,
		elapsed_us);
	  self->stat.blocking_elapsed_sum += elapsed_us;
	  self->stat.blocking_elapsed_ct ++;
	  self->stat.blocking_elapsed_max = MAX(self->stat.blocking_elapsed_max, (uint64_t)elapsed_us);
	} else {
	blktimo:
	  self->stat.blocking_timeouts++;
	  printf("[%s] timeout! apparently no RSP %u\n", self->task_path, hdr.rwmsg_ser);
	}
      }
    }
  }

 nope:
  self->stat.req_sent_pertimev[MIN(msgsnt,MAXBURST)]++;
  if (id) {
    /* id=0 => ad-hoc call, not actually a timer expiration, so we don't re-add in that case */
    self->id_timer1 = rwmsg_toytimer_add(self->toy, PERIOD_MS + (rw_random() % (PERIOD_MS / 10) - (PERIOD_MS / 5)), timer1, self);
    RW_ASSERT(self->id_timer1);
  }
}



void broker_runtimer(uint64_t id, void *ud) {
  RW_ASSERT(ud == &bk);
  RW_ASSERT(id == bk.id_runtimer);

  bk.ticks++;

  struct timeval tv_now, tv_elapsed;
  gettimeofday(&tv_now, NULL);
  timersub(&tv_now, &bk.tv_start, &tv_elapsed);

  printf("Broker timer id=%lu ticks=%d elapsed=%u.%06u\n", id, (int)bk.ticks, (unsigned int)tv_elapsed.tv_sec, (unsigned int)tv_elapsed.tv_usec);

  uint64_t msg_recv_previous = bk.global_stats.msg_recv;
  total_stats(&bk.global_stats);
  uint64_t inflight = (bk.global_stats.req_sent + bk.global_stats.rsp_sent) - bk.global_stats.msg_recv;
  if (bk.ticks >= bk.runtime_ticks) {
    if (!bk.halt) {
      printf("Broker setting halt flag\n");
      bk.halt = TRUE;
    } else {
      bk.halt_ticks++;
      printf("Broker halt wait tick %d...\n", bk.halt_ticks);
      printf("  Trailing 5s rate: %lu msg/sec\n  Current inflight msg count: %lu\n  Current VmSize %d KB\n  Total stats:\n",
	     ((bk.global_stats.msg_recv - msg_recv_previous) / 5),
	     inflight,
	     getprocstatusval("VmSize"));
      print_stats(&bk.global_stats, &bk.stat);
      if (tasklets.self[0]->blocker) {
	printf("Blocking tasklet '%s':\n", tasklets.tasklet[0]);
	print_stats(&tasklets.self[0]->stat, NULL);
      }
      if (tasklets.tasklet_ct > 1) {
	printf("Tasklet '%s':\n", tasklets.tasklet[1]);
	print_stats(&tasklets.self[1]->stat, NULL);
      }
      if (!inflight || bk.halt_ticks > 6) {
	if (inflight) {
	  printf("***Inflight=%lu\n", inflight);
	}
	printf("Broker exiting, you have 10s to interrupt in gdb\n");
	sleep(10);
	exit(0);
      }
    }
  } else {
    int ticksleft = bk.runtime_ticks - bk.ticks;
    int sleft = ticksleft * 5000 / 1000;
    if (sleft % 10 == 0) {
      printf("Runtime %lu%% elapsed, %dm%02ds remaining.\n  Trailing 5s rate: %lu msg/sec\n  Current inflight msg count: %lu\n  Current VmSize %d KB\n",
	     bk.ticks * 100 / bk.runtime_ticks,
	     sleft / 60,
	     sleft % 60,
	     ((bk.global_stats.msg_recv - msg_recv_previous) / 5),
	     inflight,
	     getprocstatusval("VmSize"));
      print_stats(&bk.global_stats, &bk.stat);
    }
  }

  bk.id_runtimer = rwmsg_toytimer_add(bk.toy, 5000, broker_runtimer, &bk);
  RW_ASSERT(bk.id_runtimer);
}

void broker_sockevent1(uint64_t id, int fd, int revents, void *ud) {
  RW_ASSERT(ud == &bk);
  RW_ASSERT(id == bk.id_sockevent1);

  int recvcap = 10;

  if (fd == bk.xsub_efd && (revents & POLLIN)) {
    while(recvcap--) {
      int r;
#if USE_NN_MSG
      char *nnmsg = NULL;
      r = nn_recv(bk.xsub, &nnmsg, NN_MSG, NN_DONTWAIT);
#else
      char nnmsg[2048];
      r = nn_recv(bk.xsub, &nnmsg, sizeof(nnmsg), NN_DONTWAIT);
#endif
      if (r < 0) {
	switch (errno) {
	case EAGAIN:
	case EINTR:
	  bk.stat.msg_read_again++;
#if USE_NN_MSG
	  RW_ASSERT(!nnmsg);
#endif
	  goto nxread;
	  break;
	default:
	  printf("[%ld] broker recv err %d '%s'\n", GETTID(), errno, strerror(errno));
	  exit(1);
	  break;
	}
      }
      bk.stat.msg_recv++;
#if ! BROKER_DROP
      TRACE("[%ld] broker forwarding msg to '%s'\n", GETTID(), nnmsg);
      
      int plen = strlen(nnmsg)+1;
      struct nn_iovec iov[] = {
	{ .iov_base = nnmsg, .iov_len = plen },
	{ .iov_base = nnmsg + plen, r - plen }
      };
      struct nn_msghdr msghdr = {
	.msg_iov = iov,
	.msg_iovlen = 2
      };

      int againct=0;
    again:
      r = nn_sendmsg(bk.xpub, &msghdr, NN_DONTWAIT);
      if (r < 0) {
	switch (errno) {
	case EAGAIN:
	case EINTR:
	  sched_yield();
	  againct++;
	  bk.stat.msg_sent_again[MIN(againct,15)]++;
	  goto again;
	  break;
	default:
#if USE_NN_MSG
	  nn_freemsg(nnmsg);
#endif
	  bk.stat.msg_send_err++;
	  bk.stat.msg_send_err_lasterrno = errno;
	  goto nxread;
	  break;
	}
      }
      bk.stat.msg_sent++;
#endif
      
#if USE_NN_MSG
      nn_freemsg(nnmsg);
#endif
    }
  nxread:
    ;
  }

  return;
}

static void broker_fdevent(uint64_t id, int fd, int revents, void *ud) { 
  id=id;
  fd=fd;
  revents=revents;
  ud=ud;

  RW_ASSERT(NN_WORKER_TASKLET);

  bk.stat.nn_fdevent++;

  nn_run_worker();

  nn_worker_getfdto(&bk.nn_fd, &bk.nn_events, &bk.nn_timeout);

  if (bk.nn_fdid) {
    rwmsg_toyfd_del(bk.toy, bk.nn_fdid);
    bk.nn_fdid = 0;
  }
  if (bk.nn_fd > -1) {
    bk.nn_fdid = rwmsg_toyfd_add(bk.toy, bk.nn_fd, bk.nn_events, broker_fdevent, NULL);
  }

  if (bk.nn_timerid) {
    rwmsg_toytimer_del(bk.toy, bk.nn_timerid);
    bk.nn_timerid = 0;
  }
  if (bk.nn_timeout > -1) {
    bk.nn_timerid = rwmsg_toytimer_add(bk.toy, bk.nn_timeout, broker_toevent, NULL);
    RW_ASSERT(bk.nn_timerid);
  }

}
static void broker_toevent(uint64_t id, void *ud) {
  ud=ud;

  RW_ASSERT(NN_WORKER_TASKLET);

  bk.stat.nn_timer++;

  nn_run_worker();

  nn_worker_getfdto(NULL, NULL, &bk.nn_timeout);

  if (bk.nn_timeout < 0) {
    bk.stat.nn_timerlen_inf++;
  } else if (bk.nn_timeout == 0) {
    bk.stat.nn_timerlen_zero++;
  } else if (bk.nn_timeout >= 1) {
    bk.stat.nn_timerlen_pos++;
  }

  if (id) {
    RW_ASSERT(id == bk.nn_timerid);
    rwmsg_toytimer_del(bk.toy, id);
    bk.nn_timerid = 0;
  }

  if (bk.nn_timeout >= 0) {
    bk.nn_timerid = rwmsg_toytimer_add(bk.toy, bk.nn_timeout, broker_toevent, NULL);
    RW_ASSERT(bk.nn_timerid);
  }

}

static int nn_tasklet_yield(int fd, int events, int timeout) {
  uint64_t fdid = 0;
  fd=fd;
  events=events;

  RW_ASSERT(NN_WORKER_TASKLET);

  nn_worker_getfdto(&bk.nn_fd, &bk.nn_events, &bk.nn_timeout);
  if (bk.nn_timeout < 0) {
    bk.stat.nn_timerlen_inf++;
  } else if (bk.nn_timeout == 0) {
    bk.stat.nn_timerlen_zero++;
  } else if (bk.nn_timeout >= 1) {
    bk.stat.nn_timerlen_pos++;
  }

  if (bk.nn_fdid) {
    rwmsg_toyfd_del(bk.toy, bk.nn_fdid);
    bk.nn_fdid = 0;
  }
  if (bk.nn_fd > -1) {
    bk.nn_fdid = rwmsg_toyfd_add(bk.toy, bk.nn_fd, bk.nn_events, broker_fdevent, NULL);
  }

  int revents = rwmsg_toytask_block(bk.toy, fdid, timeout == -1 ? 0 : timeout == 0 ? 1 : timeout);

 spin:
  nn_worker_getfdto(&bk.nn_fd, &bk.nn_events, &bk.nn_timeout);
  if (bk.nn_timeout < 0) {
    bk.stat.nn_timerlen_inf++;
  } else if (bk.nn_timeout == 0) {
    bk.stat.nn_timerlen_zero++;
  } else if (bk.nn_timeout >= 1) {
    bk.stat.nn_timerlen_pos++;
  }

  if (bk.nn_fdid) {
    rwmsg_toyfd_del(bk.toy, bk.nn_fdid);
    bk.nn_fdid = 0;
  }
  if (bk.nn_fd > -1) {
    bk.nn_fdid = rwmsg_toyfd_add(bk.toy, bk.nn_fd, bk.nn_events, broker_fdevent, NULL);
  }

  if (bk.nn_timerid) {
    rwmsg_toytimer_del(bk.toy, bk.nn_timerid);
    bk.nn_timerid = 0;
  }

  if (!bk.nn_timeout) {
    goto spin;
  }

  if (bk.nn_timeout > -1) {
    bk.nn_timerid = rwmsg_toytimer_add(bk.toy, bk.nn_timeout, broker_toevent, NULL);
    RW_ASSERT(bk.nn_timerid);
  }

  return revents;
}

static void broker_init(rwmsg_toysched_t *ts) {
  printf("[%ld] broker_device()\n", GETTID());

  gettimeofday(&bk.tv_start, NULL);

  bk.toy = rwmsg_toytask_create(ts);

  struct nn_riftconfig nncfg  = { 
    .singlethread = NN_WORKER_TASKLET,
    .waitfunc = nn_tasklet_yield
  };

  nn_global_init(&nncfg);

  if (NN_WORKER_TASKLET) {
    nn_worker_getfdto(&bk.nn_fd, &bk.nn_events, &bk.nn_timeout);
    if (bk.nn_timeout < 0) {
      bk.stat.nn_timerlen_inf++;
    } else if (bk.nn_timeout == 0) {
      bk.stat.nn_timerlen_zero++;
    } else if (bk.nn_timeout >= 1) {
      bk.stat.nn_timerlen_pos++;
    }
    if (bk.nn_fdid) {
      rwmsg_toyfd_del(bk.toy, bk.nn_fdid);
      bk.nn_fdid = 0;
    }
    if (bk.nn_fd > -1) {
      bk.nn_fdid = rwmsg_toyfd_add(bk.toy, bk.nn_fd, bk.nn_events, broker_fdevent, NULL);
    }
    if (bk.nn_timerid) {
      rwmsg_toytimer_del(bk.toy, bk.nn_timerid);
      bk.nn_timerid = 0;
    }
    if (bk.nn_timeout > -1) {
      bk.nn_timerid = rwmsg_toytimer_add(bk.toy, bk.nn_timeout, broker_toevent, NULL);
      RW_ASSERT(bk.nn_timerid);
    }
  }

  bk.xsub = nn_socket(AF_SP, NN_PULL);
  nn_bind(bk.xsub, BROKER_ADDR_PULL);

  int r;
  size_t solen = sizeof(bk.xsub_efd);
  r = nn_getsockopt(bk.xsub, NN_SOL_SOCKET, NN_RCVFD, &bk.xsub_efd, &solen);
  if (r) {
    printf("[%ld] + nn_getsockopt err %d %s\n", GETTID(), errno, strerror(errno));
    exit(1);
  }

  bk.xpub = nn_socket(AF_SP, NN_PUB);
  nn_bind(bk.xpub, BROKER_ADDR_PUB);

  printf("[%ld] broker listening at pull='%s' and pub='%s'\n", GETTID(), BROKER_ADDR_PULL, BROKER_ADDR_PUB);

  bk.id_sockevent1 = rwmsg_toyfd_add(bk.toy, bk.xsub_efd, POLLIN, broker_sockevent1, &bk);

  bk.runtime_ticks = ( RUNTIME_SECONDS * 1000 ) / 5000;
  bk.id_runtimer = rwmsg_toytimer_add(bk.toy, 5000, broker_runtimer, &bk);
  RW_ASSERT(bk.id_runtimer);

  if (getenv("VALGRIND")) {
    bk.valgrind = 1;
  }
}


/* Run taskct tasklets, plus a broker if runbroker */
static void runtasks(int taskct, int runbroker) {
  pid_t tid = GETTID(); /* gettid() - integer thread id or pid */

  printf("runtasks() taskct=%d, in tid=%d\n", taskct, tid);

  rwmsg_toysched_t *tasklet_sched;
  tasklet_sched = malloc(sizeof(rwmsg_toysched_t));
  rwmsg_toysched_init(tasklet_sched);

  if (runbroker) {
    broker_init(tasklet_sched);
  }

  static int blocker=0;

  int tidx;
  for (tidx = 0; tidx < taskct; tidx++) {

    struct tasklet *self = malloc(sizeof(struct tasklet));
    memset(self, 0, sizeof(*self));

    if (DOBLOCKING && !blocker) {
      /* This task will be the one blocking task in this process */
      blocker=1;
      self->blocker=1;
    }

    self->nxserial = 1;
    self->task_idx = tidx; 	/* within this sched and thus thread */
    self->task_tid = tid;	/* thread id */
    /* instance number is 64 bits, <thread32>+<tasklet32> */
    uint64_t tnum = (((uint64_t)self->task_tid)<<32) | (((uint64_t)tidx) << 0);
    sprintf(self->task_path, "/X/A/exp[%lx]", tnum);
    sprintf(self->task_bpath, "/X/B/exp[%lx]", tnum);
    RW_ASSERT(strlen(self->task_bpath) < PATHLEN);

    printf ("[%s] + creating task %s\n", self->task_path, self->task_path);
    
    self->toy = rwmsg_toytask_create(tasklet_sched);
    
    self->nn.pub = nn_socket(AF_SP, NN_PUSH);

    self->nn.sub = nn_socket(AF_SP, NN_SUB);
    if (1 || blocker) {
      int soval = 1024000;	/* default was 128KB */
      int sosz = sizeof(soval);
      nn_setsockopt(self->nn.sub, NN_SOL_SOCKET, NN_RCVBUF, &soval, sosz);
    }
    nn_setsockopt(self->nn.sub, NN_SUB, NN_SUB_SUBSCRIBE, self->task_path, strlen(self->task_path));

    self->nn.bsub = nn_socket(AF_SP, NN_SUB);
    nn_setsockopt(self->nn.bsub, NN_SUB, NN_SUB_SUBSCRIBE, self->task_bpath, strlen(self->task_bpath));
    
    self->nn.pub_eid = nn_connect(self->nn.pub, BROKER_ADDR_PULL);
    self->nn.sub_eid = nn_connect(self->nn.sub, BROKER_ADDR_PUB);
    self->nn.bsub_eid = nn_connect(self->nn.bsub, BROKER_ADDR_PUB);
    
    
    int r;
    size_t solen = sizeof(self->nn.sub_efd);
    r = nn_getsockopt(self->nn.sub, NN_SOL_SOCKET, NN_RCVFD, &self->nn.sub_efd, &solen);
    if (r) {
      printf("[%s] + nn_getsockopt err %d %s\n", self->task_path, errno, strerror(errno));
      exit(1);
    }

    solen = sizeof(self->nn.bsub_efd);
    r = nn_getsockopt(self->nn.bsub, NN_SOL_SOCKET, NN_RCVFD, &self->nn.bsub_efd, &solen);
    if (r) {
      printf("[%s] + nn_getsockopt err %d %s\n", self->task_path, errno, strerror(errno));
      exit(1);
    }
    
    self->id_sockevent1 = rwmsg_toyfd_add(self->toy, self->nn.sub_efd, POLLIN, sockevent1, self);
    RW_ASSERT(self->id_sockevent1);
    rwmsg_toyfd_del(self->toy, self->id_sockevent1); /* just to test del */
    self->id_sockevent1 = rwmsg_toyfd_add(self->toy, self->nn.sub_efd, POLLIN, sockevent1, self);
    RW_ASSERT(self->id_sockevent1);

    self->id_sockevent1b = rwmsg_toyfd_add(self->toy, self->nn.bsub_efd, POLLIN, sockevent1, self);
    RW_ASSERT(self->id_sockevent1b);
    
    self->id_timer1 = rwmsg_toytimer_add(self->toy, rw_random() % PERIOD_MS, timer1, self);
    RW_ASSERT(self->id_timer1);

    push_tasklet(self->task_path, self); /* now people will yammer at us! */
  }

  printf("runtasks calling toysched; vmsize=%d\n", getprocstatusval("VmSize"));

  while (1) {
    rwmsg_toysched_run(tasklet_sched, NULL);
  }

  /* There is no cleanup of toytask or tasklet or nanomsg structures */
  RW_CRASH();

  free(tasklet_sched);
  tasklet_sched = NULL;
}

int main(int argc, char **argv, char **envp) {
  TRACE("main()\n");

  setvbuf(stdout, NULL, _IONBF, 0);

  printf("vmsize=%d\n", getprocstatusval("VmSize"));

  printf("Config: TASKCT=%d MSG_PER_TASK=%d RUNTIME=%d DOBLOCKING=%d BLOCKER_DESTWEIGHT=%d/1000 USE_NN_MSG=%d\n"
	 "        BROKER_DROP=%d NN_WORKER_TASKLET=%d BROKER_ADDR_PULL='%s' BROKER_ADDR_PUB='%s'\n",
	 TASKCT,
         MSG_PER_TASK,
	 RUNTIME_SECONDS,
	 DOBLOCKING,
	 BLOCKER_DESTWEIGHT,
	 USE_NN_MSG,
	 BROKER_DROP,
         NN_WORKER_TASKLET,
	 BROKER_ADDR_PULL,
	 BROKER_ADDR_PUB);

  runtasks(TASKCT, TRUE/* broker in this thread */);
  exit(0);

  argv=argv;
  envp=envp;
  argc=argc;
}
