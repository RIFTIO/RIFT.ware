
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */


#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#include "rwtasklet.h"

#include <rwmsg_int.h>
#include <rwmsg_broker.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>

#define TEST_PATH "/R/RWMSG/TEST/PERF"

typedef struct rwmsg_perf_task_s rwmsg_perf_task_t;

typedef struct {
  void (*init)(rwmsg_perf_task_t *);
  void (*start)(void *ud);
  void (*deinit)(rwmsg_perf_task_t *);
  void (*timeout)(void *ud);
} rwmsg_perf_test_vector_t;

struct rwmsg_perf_task_s {
  struct {
    enum {
      RWMSG_PERF_FUNC_BROKER,
      RWMSG_PERF_FUNC_SERVER,
      RWMSG_PERF_FUNC_CLIENT
    } function;
    int instance;
    int bro_inst;
    int blocking;
    int outstanding;
    int count;
    int runtime;
    int delay;
    int size;
    int mainq;
    int cliqlen;
    int defwin;
    int nofeedme;
  } cfg;

  int ditto;

  int num;
  char *str_function;

  rwmsg_perf_test_vector_t *v;

  rwsched_tasklet_ptr_t tasklet;

  char path[64];
  rwmsg_endpoint_t *ep;
  rwsched_dispatch_queue_t rws_q;

  rwmsg_srvchan_t *sc;
  rwmsg_method_t *meth;

  rwmsg_signature_t *sig;

  rwmsg_clichan_t *cc;
  rwmsg_destination_t *dest;

  rwmsg_broker_t *bro;

  struct msg *m;
  int halt;
  int feedme_defreg;
  int feedme_reg;
  
  struct stats {
    uint64_t sent;
    uint64_t sent_bytes;
    uint64_t recv;
    uint64_t recv_bytes;
    uint64_t outstanding;
    uint64_t max_outstanding;
    uint64_t e_backpressure;
    uint64_t e_notconnected;
    uint64_t e_other;
    uint64_t last_sent;
    uint64_t last_recv;
    uint64_t in_mainq;
    uint64_t task_timeout;
    uint64_t feedme_flow;
    uint64_t feedme_noflow;
    uint64_t bnc;
    uint64_t bnctype[RWMSG_BOUNCE_CT];
    struct timeval tv_first;
    struct timeval tv_last;
    struct rtt {
#define RTT_SAMPLE (128)  /* 1/N messages are RTT-measured, must be power of two */
      uint32_t rtt_2ms;
      uint32_t rtt_4ms;
      uint32_t rtt_8ms;
      uint32_t rtt_16ms;
      uint32_t rtt_32ms;
      uint32_t rtt_64ms;
      uint32_t rtt_128ms;
      uint32_t rtt_256ms;
      uint32_t rtt_512ms;
      uint32_t rtt_1024ms;
      uint32_t rtt_2048ms;
      uint32_t rtt_4096ms;
      uint32_t rtt_8192ms;
      uint32_t rtt_16384ms;
      uint32_t rtt_32768ms;
      uint32_t rtt_huge;
    } rtt;
  } stats;

};

static void print_stats(struct stats *s) {
  const char *desc[] = {
    "sent",
    "sent_bytes",
    "recv",
    "recv_bytes",
    "outstanding",
    "max_outstanding",
    "e_backpressure",
    "e_notconnected",
    "e_other",
    "last_sent",
    "last_recv",
    "in_mainq",
    "task_timeout",
    "feedme_flow",
    "feedme_noflow",
    "bnc",
    NULL
  };
  const char *bncdesc[] = {
    "OK",
    "NODEST",
    "NOMETH",
    "NOPEER",
    "BROKERR",
    "TIMEOUT",
    "RESET",
    "TERM",
    NULL
  };
  int i=0;
  uint64_t *val;
  for (i=0,val=&(s->sent); desc[i]; i++) {
    printf("  %-16s: %lu\n", desc[i], val[i]);
  }

  for (i=0,val=&(s->bnctype[0]); bncdesc[i]; i++) {
    printf("  bnctype %-8s: %lu\n", bncdesc[i], val[i]);
  }

  if (s->tv_first.tv_sec && s->tv_last.tv_sec) {
    struct timeval delta;
    timersub(&s->tv_last, &s->tv_first, &delta);
    uint64_t ms = delta.tv_sec;
    ms *= 1000;
    ms += (delta.tv_usec / 1000);
    printf("  %-16s: %lu\n",
	   "elapsed ms",
	   ms);
    uint64_t rate = 0;
    if (ms) {
      rate = 1000 * (s->recv + s->bnc) / ms;
    }
    printf("  %-16s: %lu\n",
	   "rate msg/s",
	   rate);
  }
}
static void print_rtt(struct rtt *r, uint64_t bnc, uint64_t lost) {
  int i=0;
  uint32_t *val = &(r->rtt_2ms);
  uint64_t tot = 0;
  for (i=0; i<15; i++) {
    uint32_t ms = (1<<(i+1));
    printf("  rtt <%7ums  : %u\n", ms, val[i]);
    tot += val[i];
  }
  assert(&(val[i]) == &(r->rtt_huge));
  printf(  "  rtt >= 32768ms  : %u\n", r->rtt_huge);
  tot += r->rtt_huge;
  printf(  "  rtt bnc         : %lu\n", bnc / RTT_SAMPLE);
  tot += (bnc/RTT_SAMPLE);
  printf(  "  rtt never       : %lu\n", lost / RTT_SAMPLE);
  tot += (lost/RTT_SAMPLE);
  printf(  "  rtt total samps : %lu\n", tot);
}

#define LOG2(val) ({				\
  int bit=0;					\
  int tmp=(val);				\
  while (tmp >>= 1) {				\
    bit++;					\
  }						\
  bit;						\
})

static inline void rtt_bump(struct rtt *r, int ms) {
  int bit = LOG2(ms);
  if (ms < 32768) {
    assert(bit < 15);
    (&r->rtt_2ms)[bit]++;
  } else {
    r->rtt_huge++;
  }
}

static struct {
  /* Config */
  int shunt;
  char *nnuri;
  int usebro;
  int zookeep;

  rwmsg_perf_task_t *tasks;
  int tasks_ct;

  /* Runtime */
  rwsched_instance_ptr_t rwsched;
  rwcal_module_ptr_t rwcal;
  rwtrace_ctx_t *rwtrace;

#define MAX_BROS 5
  int bros_ct;

  uint32_t done_ct; 
  int max_runtime;
  uint32_t killable_tasks;
  uint32_t killed_tasks;

  uint32_t done;

  rwtasklet_info_t ti_s;
  rwtasklet_info_t *ti;
} perf = {
  .nnuri = "tcp://127.0.0.1:1234",
  //.nnuri = "tcp://" RWMSG_CONNECT_ADDR_STR ":1234",
  .max_runtime = 1  
};


struct msg {
  struct timeval tv_sent;
  uint8_t last;
  char _buf[0];
};

/* Forwards */
static void start_client(void *ctx);
static void feedme_client(void *ctx, rwmsg_destination_t *dest, rwmsg_flowid_t flowid);


static void init_broker(rwmsg_perf_task_t *t) {
  if (perf.bros_ct>MAX_BROS) {
    fprintf(stderr, "Error: Maximum broker tasklets already running, can only run maximum %d!\n", MAX_BROS);
    exit(1);
  }
  char tmp[999];
  strcpy(tmp,perf.nnuri);
  char *pnum = strrchr(tmp, ':');
  RW_ASSERT(pnum);
  pnum++;
  RW_ASSERT(pnum);
  uint16_t port = atoi(pnum);
  *pnum = '\0';
  sprintf(pnum,"%u", port+t->cfg.instance);
  printf("Task %d broker, nnuri '%s' starting...\n", t->num, tmp);
  RW_ASSERT(!t->bro);

  *(pnum-1) = '\0'; // to extract host-ip from  "tcp://<host-ip>:<port>
  pnum = strrchr(tmp, ':');
  pnum++; // skip "://" to get to the starting of host-ip
  pnum++;
  pnum++;

  //t->bro = rwmsg_broker_create(0, t->cfg.instance, "127.0.0.1", perf.rwsched, t->tasklet, perf.rwcal, t->cfg.mainq, t->ep);
  t->bro = rwmsg_broker_create(0, t->cfg.instance, pnum, perf.rwsched, t->tasklet, perf.rwcal, t->cfg.mainq, t->ep, perf.ti);
  perf.bros_ct++;
}
static void deinit_broker(rwmsg_perf_task_t *t) {
  rwmsg_broker_halt(t->bro);
  t->bro = NULL;
  perf.bros_ct--;
}

static void server_method_1(rwmsg_request_t *req, void *ctx) {
  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_perf_task_t *t = (rwmsg_perf_task_t *)ctx;
  t->stats.recv++;
  uint32_t paylen=0;
  const struct msg *pay = (const struct msg*)rwmsg_request_get_request_payload(req, &paylen);
  if (pay) {
    t->stats.recv_bytes += paylen;
    int pay_is_last = FALSE;
    if (paylen >= sizeof(struct msg) && pay->last) {
      t->stats.last_recv++;
      pay_is_last = TRUE;
    }
    if (t->cfg.size <= paylen) {
      rs = rwmsg_request_send_response(req, pay, t->cfg.size);
    } else {
      memcpy(t->m, pay, paylen);
      rs = rwmsg_request_send_response(req, t->m, t->cfg.size);
    }
    if (rs != RW_STATUS_SUCCESS) {
      printf("server_method_1 send_response() error, rs=%u\n", rs);
      switch(rs) {
      default:
	t->stats.e_other++;
	break;
      case RW_STATUS_NOTCONNECTED:
	t->stats.e_notconnected++;
	break;
      case RW_STATUS_BACKPRESSURE:
	t->stats.e_backpressure++;
	break;
      }
    } else {
      t->stats.sent++;
      t->stats.sent_bytes += t->cfg.size;
      if (pay_is_last && t->cfg.size >= sizeof(struct msg)) {
	t->stats.last_sent++;
      }
    }
  } else {
    rs = rwmsg_request_send_response(req, NULL, 0);
    if (rs != RW_STATUS_SUCCESS) {
      printf("server_method_1 send_response() error, rs=%u\n", rs);
    } else {
      t->stats.sent++;
    }
  }
  if (rs == RW_STATUS_SUCCESS && t->stats.sent == 1) {
    gettimeofday(&t->stats.tv_first, NULL);
  }
}
static void init_server(rwmsg_perf_task_t *t) {
  rw_status_t rs = RW_STATUS_FAILURE;
  UNUSED(rs);

  printf("Task %d server, path '%s' starting...\n", t->num, t->path);

  t->m = calloc(t->cfg.size, 1);

  t->sc = rwmsg_srvchan_create(t->ep);
  assert(t->sc);

  t->sig = rwmsg_signature_create(t->ep, RWMSG_PAYFMT_RAW, 1, RWMSG_PRIORITY_DEFAULT);
  assert(t->sig);

  rwmsg_closure_t cb;
  cb.request = server_method_1;
  cb.ud = t;
  t->meth = rwmsg_method_create(t->ep, t->path, t->sig, &cb);
  assert(t->meth);

  rs = rwmsg_srvchan_add_method(t->sc, t->meth);

  rs = rwmsg_srvchan_bind_rwsched_queue(t->sc, t->rws_q);
  assert(rs == RW_STATUS_SUCCESS);
}
static void deinit_server(rwmsg_perf_task_t *t) {
  rwmsg_signature_release(t->ep, t->sig);
  rwmsg_method_release(t->ep, t->meth);
  rwmsg_srvchan_halt(t->sc);
}

static void deinit_tasklet(rwmsg_perf_task_t *t) {
  t->v->deinit(t);
  
  if (!t->cfg.mainq) {
    rwsched_dispatch_release(t->tasklet, t->rws_q);
  }
  t->rws_q = NULL;

  //if (!perf.bro || t->ep != perf.bro->ep) {
  if (!t->bro) {
    rwmsg_endpoint_halt_flush(t->ep, TRUE);
    t->ep = NULL;
  }

  perf.killed_tasks++;
}

static void main_kill_tasklet(void *ctx) {
  //  rwmsg_perf_task_t *t = (rwmsg_perf_task_t *)ctx;

  perf.done_ct++;
  if (perf.done_ct == perf.killable_tasks) {
    perf.done = TRUE;
  }

}

static void init_client(rwmsg_perf_task_t *t) {
  printf("Task %d client starting, sending to path '%s'...\n", t->num, t->path);
  t->cc = rwmsg_clichan_create(t->ep);
  rwmsg_clichan_bind_rwsched_queue(t->cc, t->rws_q);
  t->sig = rwmsg_signature_create(t->ep, RWMSG_PAYFMT_RAW, 1, RWMSG_PRIORITY_DEFAULT);
  t->dest = rwmsg_destination_create(t->ep, RWMSG_ADDRTYPE_UNICAST, t->path);
  t->m = calloc(t->cfg.size, 1);
  RW_ASSERT(!t->feedme_defreg);
  if (!t->feedme_defreg && !t->cfg.nofeedme) {
    rwmsg_closure_t cb = { 
      .feedme = feedme_client,
      .ud = t
    };
    rwmsg_destination_feedme_callback(t->dest, &cb);
    t->feedme_defreg = TRUE;
  }
}

static void client_response_1(rwmsg_request_t *req, void *ctx) {
  rwmsg_perf_task_t *t = (rwmsg_perf_task_t*)ctx;

  if (req) {
    uint32_t rsplen = 0;
    const struct msg *rsp = (const struct msg*)rwmsg_request_get_response_payload(req, &rsplen);
    if (rsp) {
      if (rsplen >= sizeof(struct msg) && rsp->tv_sent.tv_sec) {
	struct timeval now, delta;
	gettimeofday(&now, NULL);
	timersub(&now, &rsp->tv_sent, &delta);
	uint64_t delta_ms = delta.tv_usec / 1000 + delta.tv_sec * 1000;
	rtt_bump(&t->stats.rtt, delta_ms);
      }

      t->stats.recv++;
      t->stats.recv_bytes += rsplen;

      if (!t->feedme_reg && !t->cfg.nofeedme) {
	rwmsg_closure_t cb = { 
	  .feedme = feedme_client,
	  .ud = t
	};
	rwmsg_clichan_feedme_callback(t->cc, t->dest, rwmsg_request_get_response_flowid(req), &cb);
	t->feedme_reg = TRUE;
      }

      if (rsplen >= sizeof(struct msg) && rsp->last) {
	t->stats.last_recv++;
	rwsched_dispatch_async_f(t->tasklet, rwsched_dispatch_get_main_queue(perf.rwsched), t, main_kill_tasklet);
      }

    } else {
      t->stats.bnc++;
      rwmsg_bounce_t bcode = rwmsg_request_get_response_bounce(req);
      RW_ASSERT(bcode < RWMSG_BOUNCE_CT);
      t->stats.bnctype[bcode]++;
    }

    if (t->stats.bnc + t->stats.recv == t->cfg.count) {
      gettimeofday(&t->stats.tv_last, NULL);
    }

    t->stats.outstanding--;
  }

  rwmsg_closure_t cb;
  cb.request = client_response_1;
  cb.ud = t;

  req = NULL;			/* guarantee req is a client request */
  while (t->stats.outstanding < t->cfg.outstanding && t->stats.sent < t->cfg.count && !t->halt) {
    req = rwmsg_request_create(t->cc);
    rwmsg_request_set_signature(req, t->sig);
    rwmsg_request_set_callback(req, &cb);
    
    if (t->cfg.size) {
      t->m->last = !!(t->stats.sent+1 == t->cfg.count);
      if (t->m->last) {
	t->stats.last_sent++;
      }
      if (!(t->stats.sent & (RTT_SAMPLE-1))) {
	/* Occasionally send a timestamp to sample rtt */
	gettimeofday(&t->m->tv_sent, NULL);
      } else {
	t->m->tv_sent.tv_sec = 0;
      }
    }
    
    rwmsg_request_set_payload(req, t->m, t->cfg.size);
    
    rw_status_t rs = rwmsg_clichan_send(t->cc, t->dest, req);
    if (rs == RW_STATUS_SUCCESS) {
      req = NULL;
      t->stats.outstanding++;
      if (t->stats.outstanding > t->stats.max_outstanding) {
	t->stats.max_outstanding = t->stats.outstanding;
      }
      t->stats.sent++;
      t->stats.sent_bytes += t->cfg.size;
      if (t->stats.sent == 1) {
	gettimeofday(&t->stats.tv_first, NULL);
      }
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      t->stats.e_backpressure++;
      if (t->cfg.nofeedme) {
	fprintf(stderr, "Error, nofeedme client got rs=BACKPRESSURE on send.  t->stats.outstanding=%lu t->cfg.outstanding=%d t->stats.sent=%lu\n",
		t->stats.outstanding,
		t->cfg.outstanding,
		t->stats.sent);
	exit(1);
      }
      if (!t->stats.outstanding) {
	/* Mostly we rely on the feedme callback, however we also do
	   this nonsense, mostly for small model. */
	rwsched_dispatch_async_f(t->tasklet, t->rws_q, t, start_client);
      }
      break;
    } else if (rs == RW_STATUS_NOTCONNECTED) {
      t->stats.e_notconnected++;
      if (t->cfg.nofeedme) {
	fprintf(stderr, "Error, nofeedme client got rs=NOTCONNECTED on send.  t->stats.outstanding=%lu t->cfg.outstanding=%d t->stats.sent=%lu\n",
		t->stats.outstanding,
		t->cfg.outstanding,
		t->stats.sent);
	exit(1);
      }
      if (!t->stats.outstanding) {
	/* Mostly we rely on the feedme callback, however we also do
	   this nonsense, mostly for small model. */
	rwsched_dispatch_async_f(t->tasklet, t->rws_q, t, start_client);
      }
      break;
    } else {
      t->stats.e_other++;
      break;
    }
  }
  if (req) {
    /* If non-NULL, we failed to send and should free the req */ 
    rwmsg_request_release(req);
  }
}
static void feedme_client(void *ctx, rwmsg_destination_t *dest, rwmsg_flowid_t flowid) {
  rwmsg_perf_task_t *t = (rwmsg_perf_task_t *)ctx;
  if (flowid) {
    t->stats.feedme_flow++;
  } else {
    t->stats.feedme_noflow++;
  }
  client_response_1(NULL, t);
}
static void start_client(void *ctx) {
  rwmsg_perf_task_t *t = (rwmsg_perf_task_t *)ctx;
  client_response_1(NULL, t);
}
static void timeout_client(void *ctx) {
  rwmsg_perf_task_t *t = (rwmsg_perf_task_t *)ctx;
  t->halt = TRUE;
  t->stats.task_timeout++;
  rwsched_dispatch_async_f(t->tasklet, rwsched_dispatch_get_main_queue(perf.rwsched), t, main_kill_tasklet);
}
static void deinit_client(rwmsg_perf_task_t *t) {
  rwmsg_signature_release(t->ep, t->sig);
  rwmsg_destination_release(t->dest);
  rwmsg_clichan_halt(t->cc);
}


static rwmsg_perf_test_vector_t vec[] = {
  [RWMSG_PERF_FUNC_BROKER] = {
    .init = init_broker,
    .deinit = deinit_broker
  },
  [RWMSG_PERF_FUNC_CLIENT] = {
    .init = init_client,
    .start = start_client,
    .deinit = deinit_client,
    .timeout = timeout_client
  },
  [RWMSG_PERF_FUNC_SERVER] = {
    .init = init_server,
    .deinit = deinit_server
  }
};

#include <dirent.h>
#include <ifaddrs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>

void start_zookeeper() {
  int r;
  rw_status_t status = RW_STATUS_SUCCESS;
  DIR * proc;
  struct dirent * ent;

  // Scan /proc/[0-9]?*/cmdline for any process who's first 3 arguments start
  // with 'java', '-cp', '/etc/zookeeper'.  While crappy, this is enough to make
  // sure we don't attempt to start the zookeeper more than once on the same machine
  // and leave zombies all over the place.
  proc = opendir("/proc/");
  for (ent = readdir(proc); ent; ent = readdir(proc)) {
    FILE * fp;
    char path[128];
    char cmdline_start[25];
    size_t nb;

    if (!isdigit(ent->d_name[0]))
      continue;

    #ifdef _DIRENT_HAVE_D_TYPE
    if (ent->d_type != DT_DIR)
      continue;
    #endif

    r = snprintf(path, 128, "/proc/%s/cmdline", ent->d_name);
    RW_ASSERT(r != -1);

    fp = fopen(path, "r");
    if (!fp)
      continue;

    nb = fread(&cmdline_start, sizeof(char), 25, fp);
    if (nb < 25) {
      fclose(fp);
      continue;
    }

    cmdline_start[24] = '\0';

    fclose(fp);

    if (strncmp(cmdline_start, "java", 4)
        || strncmp(cmdline_start + 5, "-cp", 3)
        || strncmp(cmdline_start + 9, "/etc/zookeeper", 14)) {
      continue;
    }

    closedir(proc);
    goto done;
  }                                                                                                                                                                                                                 
  closedir(proc);

  struct ifaddrs * ifap;
  const char * master_ip;
  /*
   * Check each ip address associated with this machine.  If it matches the
   * one specified for the zookeepr master, start if it is not already running.
   *
   * Note that the zkServer.sh script provided by zookeeper upstream keeps a
   * pid file and considers it a successful no-op if the zookeeper is already
   * running.
   */

  r = getifaddrs(&ifap);
  RW_ASSERT(r == 0);

  //master_ip = "127.0.0.1";
  master_ip = RWMSG_CONNECT_ADDR_STR;

  struct ifaddrs  * it;
  for (it = ifap; it != NULL; it = it->ifa_next) {
    struct sockaddr_in * addr = (struct sockaddr_in *)it->ifa_addr;

    if (!strncmp(master_ip, inet_ntoa(addr->sin_addr), 15)) {
      char server_name[100];
      sprintf(server_name, "%s", inet_ntoa(addr->sin_addr));


      rwcal_zk_server_port_detail_ptr_t zk_server_port_detail = rwcal_zk_server_port_detail_alloc(
          server_name,
          2181,
          1,
          true,
          true);

      rwcal_zk_server_port_detail_ptr_t zk_server_port_details[] = {
        zk_server_port_detail,
        NULL
      };
      status = rwcal_rwzk_create_server_config(perf.rwcal, getpid(), 2181, 0, zk_server_port_details);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      int ret_pid = rwcal_rwzk_server_start(perf.rwcal,getpid());
      RW_ASSERT(ret_pid > 0);

      break;
    }
  }
  freeifaddrs(ifap);


done:
  return;
}

int main(int argc, char * const *argv, const char *envp[]) {

  int retval = 1;
  rw_status_t status;

  //  g_rwresource_track_handle = RW_RESOURCE_TRACK_CREATE_CONTEXT("The Master Context");

  assert(LOG2(0) == 0);
  assert(LOG2(1) == 0);
  assert(LOG2(2) == 1);
  assert(LOG2(3) == 1);
  assert(LOG2(4) == 2);
  assert(LOG2(7) == 2);
  assert(LOG2(8) == 3);
  assert(LOG2(32767) == 14);
  assert(LOG2(32768) == 15);

  char *exe;
  exe = strrchr(argv[0], '/');
  if (exe) {
    exe++;
  } else {
    exe=argv[0];
  }

  int curtask = 0;

  while (1) {
    int option_index = 0;
    enum {
      M_SHUNT=1,
      M_MAINQ,
      M_DITTO,
      M_CLIQLEN,
      M_DEFWIN,
      M_NOFEEDME,
      M_RUNTIME='r',
      M_DELAY='d',
      M_WINDOW='w',
      M_INSTANCE='i',
      M_BROINST='j',
      M_BLOCKING='b',
      M_COUNT='c',
      M_NNURI='u',
      M_SIZE='s',
      M_TASK='t',
      M_ZOOKEEP='z',
    };
    static struct option long_options[] = {
      {"nnuri",   required_argument, 0,  M_NNURI },
      {"shunt",   0,                 0,  M_SHUNT },

      {"tasklet", required_argument, 0,  M_TASK },
      {"ditto",   0,                 0,  M_DITTO },
      {"size",    required_argument, 0,  M_SIZE },
      {"mainq",   0,                 0,  M_MAINQ },
      {"nofeedme",0,                 0,  M_NOFEEDME },
      {"count",   required_argument, 0,  M_COUNT },
      {"runtime", required_argument, 0,  M_RUNTIME },
      {"delay",   required_argument, 0,  M_DELAY },
      {"window",  required_argument, 0,  M_WINDOW },
      {"blocking",0,                 0,  M_BLOCKING },
      {"instance",required_argument, 0,  M_INSTANCE },
      {"broinst", required_argument, 0,  M_BROINST },
      {"cliqlen", required_argument, 0,  M_CLIQLEN },
      {"defwin",  required_argument, 0,  M_DEFWIN },
      {"zookeep", 0,                 0,  M_ZOOKEEP },

      {"help",    0,                 0,  'h'},

      {0,         0,                 0,  0 }
    };
    
    int c = getopt_long(argc, argv, "r:w:i:j:bc:u:s:t:?h", long_options, &option_index);
    if (c == -1)
      break;
    
    switch (c) {
    case M_NOFEEDME:
      if (curtask < perf.tasks_ct) {
	rwmsg_perf_task_t *t = &perf.tasks[curtask];
	if (t->cfg.function != RWMSG_PERF_FUNC_CLIENT) {
	  fprintf(stderr, "Error: --nofeedme is for client only\n");
	  goto help;
	}
	t->cfg.nofeedme = TRUE;
      }
      break;

    case M_DEFWIN:
      if (curtask < perf.tasks_ct) {
	rwmsg_perf_task_t *t = &perf.tasks[curtask];
	if (t->cfg.function != RWMSG_PERF_FUNC_CLIENT) {
	  fprintf(stderr, "Error: --defwin is for client only\n");
	  goto help;
	}
	t->cfg.defwin = atoi(optarg);
      }
      break;

    case M_CLIQLEN:
      if (curtask < perf.tasks_ct) {
	rwmsg_perf_task_t *t = &perf.tasks[curtask];
	if (t->cfg.function != RWMSG_PERF_FUNC_CLIENT) {
	  fprintf(stderr, "Error: --cliqlen is for client only\n");
	  goto help;
	}
	t->cfg.cliqlen = atoi(optarg);
      }
      break;

    case M_NNURI:
      perf.nnuri = strdup(optarg);
      perf.usebro = TRUE;
      break;

    case M_SHUNT:
      perf.shunt = TRUE;	/* shunt */
      perf.usebro = TRUE;
      break;

    case M_ZOOKEEP:
      perf.zookeep = TRUE;
      break;

    case M_SIZE:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.size = MAX(atoi(optarg), sizeof(struct msg));
      }
      break;

    case M_MAINQ:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.mainq = TRUE;
      }
      break;

    case M_COUNT:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.count = atoi(optarg);
      }
      break;

    case M_RUNTIME:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.runtime = atoi(optarg);
	perf.max_runtime = MAX(perf.tasks[curtask].cfg.runtime, perf.max_runtime);
      }
      break;

    case M_DELAY:
      if (curtask < perf.tasks_ct) {
	if (vec[perf.tasks[curtask].cfg.function].start) {
	  perf.tasks[curtask].cfg.delay = atoi(optarg);
	} else {
	  fprintf(stderr, "Error: Tasklet type does not support startup delay.\n");
	  goto help;
	}
      }
      break;

    case M_WINDOW:
      if (curtask < perf.tasks_ct) {
	int val = atoi(optarg);
	if (val <= 0) {
	  fprintf(stderr, "Warning: silly window, setting to 100\n");
	  val = 100;
	}
	perf.tasks[curtask].cfg.outstanding = val;
      }
      break;

    case M_BLOCKING:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.blocking = TRUE;
      }
      break;

    case M_INSTANCE:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.instance = atoi(optarg);
      }
      break;

    case M_BROINST:
      if (curtask < perf.tasks_ct) {
	perf.tasks[curtask].cfg.bro_inst = atoi(optarg);
      }
      break;

    case M_DITTO:
    case M_TASK:
      if (curtask < perf.tasks_ct) {
	rwmsg_perf_task_t *t = &perf.tasks[curtask];
	switch (t->cfg.function) {
	case RWMSG_PERF_FUNC_CLIENT:
	  if (!(t->cfg.count || t->cfg.runtime)) {
	    fprintf(stderr, "Error: Client needs at least one of -r <runtime> or -c <count>\n");
	    goto help;
	  }
	  if (!t->cfg.instance) {
	    fprintf(stderr, "Error: Client needs a destination instance -i <instance>\n");
	    goto help;
	  }
	  break;
	case RWMSG_PERF_FUNC_SERVER:
	  if (!t->cfg.instance) {
	    fprintf(stderr, "Error: Server needs an instance -i <instance>\n");
	    goto help;
	  }
	default:
	  break;
	}
      }
      curtask = perf.tasks_ct;
      perf.tasks_ct++;
      perf.tasks = realloc(perf.tasks, perf.tasks_ct * sizeof(rwmsg_perf_task_t));
      memset(&perf.tasks[curtask], 0, sizeof(perf.tasks[curtask]));
      if (c == M_DITTO) {
	if (curtask < 1) {
	  fprintf(stderr, "Error: cannot invoke --ditto until after at least one task is defined\n");
	  goto help;
	}
	if (perf.tasks[curtask-1].cfg.function == RWMSG_PERF_FUNC_CLIENT) {
	  perf.killable_tasks++;
	}
	memcpy(&perf.tasks[curtask], &perf.tasks[curtask-1], sizeof(perf.tasks[curtask]));
	perf.tasks[curtask].ditto = TRUE;
      } else {
	perf.tasks[curtask].cfg.outstanding = 1;
	perf.tasks[curtask].num = curtask;
	switch (tolower(optarg[0])) {
	case 'b':
	  perf.tasks[curtask].cfg.function = RWMSG_PERF_FUNC_BROKER;
	  perf.tasks[curtask].str_function = "broker";
	  perf.tasks[curtask].cfg.outstanding = 100;
	  break;
	case 'c':
	  perf.tasks[curtask].cfg.function = RWMSG_PERF_FUNC_CLIENT;
	  perf.tasks[curtask].str_function = "client";
	  perf.tasks[curtask].cfg.delay = 100;
	  perf.killable_tasks++;
	  break;
	case 's':
	  perf.tasks[curtask].cfg.function = RWMSG_PERF_FUNC_SERVER;
	  perf.tasks[curtask].str_function = "server";
	  break;
	default:
	  goto help;
	  break;
	}
      }
      break;

    default:
    case '?':
    case 'h':
      if (1) {
	help:
	fprintf(stderr, 
		"Usage:\n  %s --tasklet <broker|server|client> ... [ --tasklet <b|s|c> ... ] ...\n",
		exe);
	fprintf(stderr,
		"\nOptions:\n"
                "  --nnuri <nnuri>           Broker address, ie tcp://127.0.0.1:1234 [-u]\n"
		"  --shunt                   Shu(n)t to broker, always\n"
		"  --tasklet <bro|cli|srv>   Run this tasklet, following args apply to it [-t]\n"
		"     --size <size>             Message size to send (client req or server rsp) [-s]\n"
		"     --mainq                   Use dispatch mainq, otherwise dedicated serial queue\n"
		"     --count <count>           Message count to send (client) [-c]\n"
		"     --runtime <seconds>       Run tasklet for this long\n"
		"     --delay <milliseconds>    Start tasklet after this delay (client)\n"
		"     --window <out>            Window size, (client|broker) [-w]\n"
		"     --defwin <out>            Initial per-destination window size (client)\n"
		"     --cliqlen <out>           Client side transmit queue length (client)\n"
		"     --nofeedme                Disable xon callback, AND fail upon xoff event (client)\n"
		"     --instance <instance>     Tasklet instance (server), or destination instance(client) [-i]\n"
		"     --broinst <broker-inst>   Tasklet's broker-instance [-i]\n"
		"  --ditto                   Run a duplicate of the preceeding tasklet\n"
		//		"     --blocking                Send blocking requests (client) [-b]\n"

		);

	fprintf(stderr,
		"\nCanned configs:\n"
		"\n  # Small model client server, 100 64 byte requests, window size 10\n"
		"  %s --tasklet server --instance 1 --size 64 \\\n"
		"  %*s --tasklet client --instance 1 --count 100 --size 64 --window 10\n",
		exe,
		(int)strlen(exe), "");

	fprintf(stderr,
		"\n  # Small model client server and broker, shunted, also 100 64b reqs win 10\n"
		"  %s --nnuri tcp://127.0.0.1:1234 --shunt \\\n"
		"  %*s --tasklet server --instance 1 --size 64 \\\n"
		"  %*s --tasklet client --instance 1 --count 100 --runtime 10 --size 64 --window 10 \\\n"
		"  %*s --tasklet broker\n",
		exe,
		(int)strlen(exe), "",
		(int)strlen(exe), "",
		(int)strlen(exe), "");

	fprintf(stderr,
		"\n  # Large model client server and broker\n"
		"  %s --nnuri tcp://127.0.0.1:1234 --tasklet broker --runtime 120\n"
		"  %s --nnuri tcp://127.0.0.1:1234 \\\n"
		"  %*s --tasklet server --size 64 --runtime 120 --instance 1 \n"
		"  %s --nnuri tcp://127.0.0.1:1234 \\\n"
		"  %*s --tasklet client --size 64 --runtime 60 --window 100 --count 500000 --instance 1\n",
		exe,
		exe,
		(int)strlen(exe), "",
		exe,
		(int)strlen(exe), "");

	exit(1);
      }
      break;      
    }
  }
  
  if (!perf.tasks_ct) {
    fprintf(stderr, "Error: need to request at least one tasklet\n");
    goto help;
  }

  int i;

  perf.rwtrace = rwtrace_init();
  perf.rwsched = rwsched_instance_new();

  perf.ti = &perf.ti_s;
  perf.ti->rwvx = rwvx_instance_alloc();
  perf.ti->rwsched_instance = perf.rwsched;
  perf.ti->rwvcs = perf.ti->rwvx->rwvcs;
  perf.rwcal = perf.ti->rwvx->rwcal_module;
  RW_ASSERT(perf.rwcal);

#if 1 // ZOOKEEPER
  if (perf.zookeep) {
    start_zookeeper();
    RwCalZkServerPortDetail server_detail;
    server_detail.zk_client_port = 2181;
    server_detail.zk_server_addr = "127.0.0.1";
    server_detail.zk_server_id = 1;
    rwcal_zk_server_port_detail_ptr_t server_details[2] = { &server_detail, NULL };
    status = rwcal_rwzk_kazoo_init(perf.rwcal, server_details);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
  } else {
    status = rwcal_rwzk_zake_init(perf.rwcal);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
  }
#endif

  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];

    t->tasklet = rwsched_tasklet_new(perf.rwsched);

    char tmp[999];
    strcpy(tmp,perf.nnuri);
    char *pnum = strrchr(tmp, ':');
    RW_ASSERT(pnum);
    pnum++;
    RW_ASSERT(pnum);
    uint16_t port = atoi(pnum);
    *pnum = '\0';
#if 0
    if (t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
      t->ep = rwmsg_endpoint_create(0, i, t->cfg.bro_inst, perf.rwsched, t->tasklet, perf.rwtrace, NULL);
      sprintf(pnum,"%u", port+t->cfg.bro_inst);
    } else {
      t->ep = rwmsg_endpoint_create(0, i, t->cfg.instance, perf.rwsched, t->tasklet, perf.rwtrace, NULL);
      sprintf(pnum,"%u", port+t->cfg.instance);
    }
#else
    if (t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
      t->ep = rwmsg_endpoint_create(0, i, t->cfg.bro_inst, perf.rwsched, t->tasklet, perf.rwtrace, NULL);
      sprintf(pnum,"%u", port+t->cfg.bro_inst);
    } else {
      t->ep = rwmsg_endpoint_create(0, i, t->cfg.instance, perf.rwsched, t->tasklet, perf.rwtrace, NULL);
      sprintf(pnum,"%u", port+t->cfg.instance);
    }
#endif

    rwmsg_endpoint_set_property_string(t->ep, "/rwmsg/broker/nnuri", tmp);
#if 1
    rwmsg_endpoint_get_property_string(t->ep, "/rwmsg/broker/nnuri",tmp, sizeof(tmp));
    fprintf(stderr, "Tasklet %s instance %d",  t->str_function, t->cfg.instance);
    fprintf(stderr, ": ep=%p /rwmsg/broker/nnuri == %s\n", t->ep, tmp);
    //rwmsg_endpoint_set_property_string(t->ep, "/rwmsg/broker/nnuri",perf.nnuri);
#endif

    if (perf.shunt) {
      rwmsg_endpoint_set_property_int(t->ep, "/rwmsg/broker/shunt", 1);
    }
    if (perf.usebro) {
      rwmsg_endpoint_set_property_int(t->ep, "/rwmsg/broker/enable", 1);
    }
    if (t->cfg.cliqlen) {
      rwmsg_endpoint_set_property_int(t->ep, "/rwmsg/queue/cc_ssbuf/qlen", t->cfg.cliqlen);
    }
    if (t->cfg.defwin) {
      rwmsg_endpoint_set_property_int(t->ep, "/rwmsg/destination/defwin", t->cfg.defwin);
    }

    if (t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
      sprintf(t->path, "%s/%u", TEST_PATH, t->cfg.instance);
    } else {
      /* broker */
      if (t->cfg.outstanding) {
	rwmsg_endpoint_set_property_int(t->ep, "/rwmsg/broker/srvchan/window", t->cfg.outstanding);
      }
    }

    if (t->cfg.mainq) {
      t->stats.in_mainq=1;
      t->rws_q = rwsched_dispatch_get_main_queue(perf.rwsched);
    } else {
      t->stats.in_mainq=0;
      t->rws_q = rwsched_dispatch_queue_create(t->tasklet, t->path, DISPATCH_QUEUE_SERIAL);
    }

    t->v = &(vec[t->cfg.function]);
    t->v->init(t);
    
    if (t->cfg.runtime && t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
      /* Dispatch timer on t->rws_q */

      // unneeded, we just rely on dispatch_main_until...
    }
  }

  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];
    if (t->v->start) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1000000ull * t->cfg.delay);
      rwsched_dispatch_after_f(t->tasklet, when, t->rws_q, t, t->v->start);
    }
  }  
  
  printf("Test command, given:\n ");
  for (i=0; i<argc; i++) {
    printf(" %s", argv[i]);
  }
  printf("\n");

  printf("Test command, normalized:\n");
  printf("  %s --nnuri '%s'", exe, perf.nnuri);
  if (perf.shunt) {
    printf(" --shunt");
  }
  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];
    if (i >= 1 && 0==memcmp(&(t[0].cfg), &(t[-1].cfg), sizeof(t[i].cfg))) {
      printf(" \\\n  %*s --ditto", (int)strlen(exe), "");
    } else {
      printf(" \\\n  %*s --tasklet %s --instance %d", (int)strlen(exe), "", t->str_function, t->cfg.instance);
      if (t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
	printf(" --size %d", t->cfg.size);
      }
      if (t->cfg.function != RWMSG_PERF_FUNC_BROKER) {
	printf(" --broinst %d", t->cfg.bro_inst);
      }
      if (t->cfg.mainq) {
	printf(" --mainq");
      }
      if (t->cfg.nofeedme) {
	printf(" --nofeedme");
      }
      if (t->cfg.defwin) {
	printf(" --defwin %d", t->cfg.defwin);
      }
      if (t->cfg.cliqlen) {
	printf(" --cliqlen %d", t->cfg.cliqlen);
      }
      if (t->cfg.runtime) {
	printf(" --runtime %d", t->cfg.runtime);
      }
      if (t->cfg.delay && vec[t->cfg.function].start) {
	printf(" --delay %d", t->cfg.delay);
      }
      switch (t->cfg.function) {
      case RWMSG_PERF_FUNC_CLIENT:
	if (t->cfg.blocking) {
	  printf(" --blocking");
	}
	printf(" --count %d --window %d", t->cfg.count, t->cfg.outstanding);
	break;
      case RWMSG_PERF_FUNC_SERVER:
	break;
      case RWMSG_PERF_FUNC_BROKER:
	if (t->cfg.outstanding) {
	  printf(" --window %d", t->cfg.outstanding);
	}
	break;
      }
    }
  }
  printf("\n");
  
  rwsched_dispatch_main_until(perf.tasks[0].tasklet, perf.max_runtime, &perf.done);
  
  if (perf.done) {
    printf("Test completed per count, in %u of %u client tasks\n", perf.done_ct, perf.killable_tasks);
    retval = 0;
  } else {
    printf("Test timeout per runtime, %u of %u client tasks were done\n", perf.done_ct, perf.killable_tasks);
  }

  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];
    deinit_tasklet(t);
  }

  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];
    if (!t->bro)
      continue;
    rwmsg_endpoint_t *broep = t->bro->ep;
    rwmsg_bool_t halted = rwmsg_broker_destroy(t->bro);
    RW_ASSERT(!halted);
    halted = rwmsg_endpoint_halt_flush(broep, TRUE);
    //!!    RW_ASSERT(halted);
    broep = NULL;
  }

  //  fprintf(stderr, "resource dump:\n");
  //  RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
  for (i=0; i<perf.tasks_ct; i++) {
    rwmsg_perf_task_t *t = &perf.tasks[i];
    
    switch (t->cfg.function) {
    case RWMSG_PERF_FUNC_CLIENT:
      printf("Tasklet #%d, client of instance %u attached to broker-instance %u:\n", 
	     i, t->cfg.instance, t->cfg.bro_inst);
      print_stats(&t->stats);
      print_rtt(&t->stats.rtt, t->stats.bnc, t->stats.sent - (t->stats.bnc+t->stats.recv));
      break;
    case RWMSG_PERF_FUNC_SERVER:
      printf("Tasklet #%d, server instance %u attached to broker-instance %u:\n", 
	     i, t->cfg.instance, t->cfg.bro_inst);
      print_stats(&t->stats);
      break;
    case RWMSG_PERF_FUNC_BROKER:
      break;
    }
  }  

  return retval;
}
