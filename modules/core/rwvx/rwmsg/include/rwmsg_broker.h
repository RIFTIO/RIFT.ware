
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */


#ifndef __RWMSG_BROKER_H__
#define __RWMSG_BROKER_H__

__BEGIN_DECLS

typedef struct rwmsg_broker_s rwmsg_broker_t;
typedef struct rwmsg_broker_acceptor_s rwmsg_broker_acceptor_t;
typedef struct rwmsg_broker_channel_s rwmsg_broker_channel_t;
typedef struct rwmsg_broker_channel_id_s rwmsg_broker_channel_id_t;

struct rwmsg_broker_acceptor_s {
  struct {
    uint32_t ip4;
    uint16_t port;
  } bind;

  int sk;
  rwmsg_notify_t notify;

  rw_dl_t negotiating;

  /* channel handshake -> bro(srv|cli|peer)chan index */
  // UTHASH hs-detail -> brochannel
  rwmsg_broker_channel_t *bch_hash;
};
struct rwmsg_broker_channel_acceptor_key_s {
  uint32_t chanid;
  uint32_t pid;
  uint32_t ipv4;
  uint32_t instid;
};

rw_status_t rwmsg_broker_acceptor_init(rwmsg_broker_acceptor_t *acc,
				       rwmsg_broker_t *bro,
				       uint32_t ipaddr,
				       uint16_t port);
void rwmsg_broker_acceptor_deinit(rwmsg_broker_acceptor_t *acc,
				  rwmsg_broker_t *bro);

void rwmsg_broker_acceptor_halt_bch_and_remove_from_hash_f(void *ctxt);

void rwmsg_broker_dump(rwmsg_broker_t *bro);


/* Global chanid, used for broker routing */
struct rwmsg_broker_channel_id_s {
  //?? colony and/or sector ??
  //?? uint32_t broid;		/* 1-(2**18-1) */
  uint32_t chanid;		/* 1-(2**14-1) */
};

struct rwmsg_broker_channel_s {
  rwmsg_channel_t ch;
  time_t ctime;
  rwmsg_broker_channel_id_t id;
  uint32_t locid;
  rwmsg_broker_t *bro;
  rwsched_dispatch_queue_t rwq;

  /* Acceptor lookup */
  struct rwmsg_broker_channel_acceptor_key_s acc_key;
  UT_hash_handle hh;		/* acceptor's hash elem */
};

void rwmsg_broker_channel_destroy(rwmsg_broker_channel_t *bch);
void rwmsg_broker_channel_create(rwmsg_broker_channel_t *bch,
				 enum rwmsg_chantype_e typ,
				 rwmsg_broker_t *bro,
				 struct rwmsg_broker_channel_acceptor_key_s *key);

struct rwmsg_broker_srvchan_cli_s {
  uint32_t brocli_id;	/* key: broker/client part of request source IDs */

  uint32_t unsent;
  uint32_t outstanding;

  // ordered queue of unanswered reqs, ordered by seqno.
  struct {
    uint32_t ready:1;
    uint32_t _pad:31;
    uint32_t seqno;
    rw_dl_t q;
    rwmsg_request_t *last_written; /* pointer to last req written to the srvchan, or null if none written */
    uint32_t unsent;		   /* count of unwritten reqs */
    uint32_t outstanding;	   /* count of outstanding reqs */
  } q[RWMSG_PRIORITY_COUNT]/* also per coherency-id q needed, TBD */;

  UT_hash_handle hh;
};

struct rwmsg_broker_srvchan_ackid_s {
  rwmsg_header_t req_hdr;
  uint32_t wheel_idx;
  rwmsg_broker_clichan_t *broclichan;
};
typedef struct rwmsg_broker_srvchan_ackid_s rwmsg_broker_srvchan_ackid_t;

/* Circular buffer object */
typedef struct {
    int         size;   /* maximum number of elements           */
    int         start;  /* index of oldest element              */
    int         end;    /* index at which to write new element  */
    rwmsg_broker_srvchan_ackid_t   *elems;  /* vector of elements                   */
} CircularBuffer;

struct rwmsg_broker_srvchan_s {
  union {
    rwmsg_channel_t ch;
    rwmsg_broker_channel_t bch;
  };

  rw_dl_t methbindings;

  rwsched_dispatch_source_t ackwheel_timer;
  uint32_t wheel_idx;
  struct timeval ackwheel_tv;
  pthread_mutex_t bscmut;
  //ck_fifo_mpmc_t ack_fifo[RWMSG_PRIORITY_COUNT];
  CircularBuffer cb[RWMSG_PRIORITY_COUNT];

  /* Advertised method bindings */
  // ARRAY, also in endpoint methbindings


  struct {
    uint64_t bnc[RWMSG_BOUNCE_CT];
    uint64_t cancel;
    uint64_t cancel_unk;
    uint64_t pending_donada;
    uint64_t cached_resend;
    uint64_t seqzero_recvd;
  } stat;

  /* Hash of per-client reordering queue and window/scheduling data */
  rwmsg_broker_srvchan_cli_t *cli_hash;
  uint32_t cli_ct;
  struct {
    uint32_t readyct;		/* how many clients are ready */
    rwmsg_broker_srvchan_cli_t *cli_next; /* pointer to next client in RR scheme */
  } cli_sched[RWMSG_PRIORITY_COUNT];
  uint32_t cli_qtot;		/* total queued request count, input for weighted RR et al */
  uint32_t cli_qunsent;		/* unsent request count, input for window allocation */

  rwmsg_request_t *req_hash;	/* and req->hh */

  uint32_t window;		/* total window */
  unsigned int seed;
  uint32_t peer_chan:1;
  uint32_t _pad:31;
};

static inline void cbInit(rwmsg_broker_srvchan_t *bsc, int size) {
    pthread_mutex_init(&bsc->bscmut, 0);
    RWMSG_BSC_LOCK(bsc);
    int pri;
    for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
      bsc->cb[pri].size  = size + 1; /* include empty elem */
      bsc->cb[pri].start = 0;
      bsc->cb[pri].end   = 0;
      bsc->cb[pri].elems = (rwmsg_broker_srvchan_ackid_t*) calloc(bsc->cb[pri].size, sizeof(rwmsg_broker_srvchan_ackid_t));
    }
    RWMSG_BSC_UNLOCK(bsc);
}

static inline void cbFree(rwmsg_broker_srvchan_t *bsc) {
    RWMSG_BSC_LOCK(bsc);
    int pri;
    for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
      free(bsc->cb[pri].elems); /* OK if null */
    }
    RWMSG_BSC_UNLOCK(bsc);
}

static inline int cbIsFull(rwmsg_broker_srvchan_t *bsc, int pri) {
    return (bsc->cb[pri].end + 1) % bsc->cb[pri].size == bsc->cb[pri].start;
}
static inline int cbIsEmpty(rwmsg_broker_srvchan_t *bsc, int pri) {
    return bsc->cb[pri].end == bsc->cb[pri].start;
}

/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
static inline void cbWrite(rwmsg_broker_srvchan_t *bsc, int pri, rwmsg_broker_srvchan_ackid_t *elem) {
    RWMSG_BSC_LOCK(bsc);
    bsc->cb[pri].elems[bsc->cb[pri].end] = *elem;
    bsc->cb[pri].end = (bsc->cb[pri].end + 1) % bsc->cb[pri].size;
    if (bsc->cb[pri].end == bsc->cb[pri].start)
        bsc->cb[pri].start = (bsc->cb[pri].start + 1) % bsc->cb[pri].size; /* full, overwrite */
    RWMSG_BSC_UNLOCK(bsc);
}

/* Read oldest element. App must ensure !cbIsEmpty() first. */
static inline int cbPeek(rwmsg_broker_srvchan_t *bsc, int pri, rwmsg_broker_srvchan_ackid_t *elem) {
    RWMSG_BSC_LOCK(bsc);
    if (bsc->cb[pri].end == bsc->cb[pri].start) {
      RWMSG_BSC_UNLOCK(bsc);
      return 0;
    }
    *elem = bsc->cb[pri].elems[bsc->cb[pri].start];
    RWMSG_BSC_UNLOCK(bsc);
    return 1;
}

static inline int cbRead(rwmsg_broker_srvchan_t *bsc, int pri, rwmsg_broker_srvchan_ackid_t *elem) {
    RWMSG_BSC_LOCK(bsc);
    if (bsc->cb[pri].end == bsc->cb[pri].start) {
      RWMSG_BSC_UNLOCK(bsc);
      return 0;
    }
    *elem = bsc->cb[pri].elems[bsc->cb[pri].start];
    bsc->cb[pri].start = (bsc->cb[pri].start + 1) % bsc->cb[pri].size;
    RWMSG_BSC_UNLOCK(bsc);
    return 1;
}

rwmsg_broker_srvchan_t *rwmsg_broker_srvchan_create(rwmsg_broker_t *bro,
						    struct rwmsg_broker_channel_acceptor_key_s *key);
void rwmsg_broker_srvchan_halt(rwmsg_broker_srvchan_t *sc);
void rwmsg_broker_srvchan_halt_async(rwmsg_broker_srvchan_t *sc);
rw_status_t rwmsg_broker_srvchan_recv_buf(rwmsg_broker_srvchan_t *sc, rwmsg_buf_t *buf);
uint32_t rwmsg_broker_srvchan_recv(rwmsg_broker_srvchan_t *sc);
void rwmsg_broker_srvchan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud);
void rwmsg_broker_srvchan_release(rwmsg_broker_srvchan_t *sc);

rwmsg_broker_srvchan_t *rwmsg_peer_srvchan_create(rwmsg_broker_t *bro,
						  struct rwmsg_broker_channel_acceptor_key_s *key);
rw_status_t rwmsg_peer_srvchan_recv_buf(rwmsg_broker_srvchan_t *sc, rwmsg_buf_t *buf);
uint32_t rwmsg_peer_srvchan_recv(rwmsg_broker_srvchan_t *sc);
void rwmsg_peer_srvchan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud);
void rwmsg_peer_srvchan_release(rwmsg_broker_srvchan_t *sc);

typedef struct {
  rwmsg_header_t hdr;
  int32_t wheelidx;		/* -1 => not in wheel */
  rwmsg_broker_srvchan_t *brosrvchan;
  rwmsg_request_t *reqref;		/* only needed for broclichan termination? */
  rw_dl_element_t elem;
  UT_hash_handle hh;
} rwmsg_broker_clichan_reqent_t;

struct rwmsg_broker_clichan_s {
  union {
    rwmsg_channel_t ch;
    rwmsg_broker_channel_t bch;
  };

  uint32_t prevlocid;

  /* Combined stream data and cache of destination channels */
  // UTHASH   METHBINDING -> {stream w/credits; ref to dst channel; age}
  //TBD currently no stream control, clichan just respects window as assigned by brosrvchan

  /* Index of outstanding reqs; need same in brosrvchan */
  // UTHASH   ID -> REQ
  //TBD currently just using broker's reqstab
  struct {
    uint64_t recv;
    uint64_t bnc[RWMSG_BOUNCE_CT];
    uint64_t cancel;
    uint64_t ss_pollout;
    uint64_t defer;
    uint64_t to_cansent;
    uint64_t to_canreleased;
    uint64_t ack_sent;
    uint64_t ack_piggy_sent;
    uint64_t ack_miss;
    uint64_t recv_noent;
    uint64_t recv_dup;
  } stat;


  /* Timer wheel of req expirations */
#define RWMSG_BROKER_REQWHEEL_SZ (120/*s*/ * 100/*cs*/)
#define RWMSG_BROKER_REQWHEEL_TIMEO_IDX(bcc, to) ({				\
  uint32_t retidx = (((bcc)->wheel_idx + MIN(RWMSG_BROKER_REQWHEEL_SZ-2, (to)))); \
  if (retidx >= RWMSG_BROKER_REQWHEEL_SZ) {				\
    retidx -= RWMSG_BROKER_REQWHEEL_SZ;					\
  }									\
  retidx;								\
})
#define RWMSG_BROKER_REQWHEEL_TICK_DIVISOR /* do this many wheel buckets per tick */ (4)
#define RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD  /* 1cs in ns */ (10000000ull)
#define RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD_USEC  ( RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD / 1000ull )
#define RWMSG_BROKER_REQWHEEL_TICK_PERIOD  (RWMSG_BROKER_REQWHEEL_TICK_DIVISOR * RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD)
  rw_dl_t reqwheel[RWMSG_BROKER_REQWHEEL_SZ];
  uint32_t wheel_idx;
  struct timeval reqwheel_tv;
  rwmsg_broker_clichan_reqent_t *reqent_hash;
  rwsched_dispatch_source_t reqwheel_timer;
  uint32_t peer_chan:1;
  uint32_t _pad:31;
};

rwmsg_broker_clichan_t *rwmsg_broker_clichan_create(rwmsg_broker_t *bro,
						    struct rwmsg_broker_channel_acceptor_key_s *key);
void rwmsg_broker_clichan_halt_async(rwmsg_broker_clichan_t *cc);
void rwmsg_broker_clichan_halt(rwmsg_broker_clichan_t *cc);
rw_status_t rwmsg_broker_clichan_recv_buf(rwmsg_broker_clichan_t *cc, rwmsg_buf_t *buf);
uint32_t rwmsg_broker_clichan_recv(rwmsg_broker_clichan_t *cc);
void rwmsg_broker_clichan_resume_f(void *ctx);
void rwmsg_broker_clichan_pause_f(void *ctx);
void rwmsg_broker_clichan_pause(rwmsg_broker_clichan_t *cc);
void rwmsg_broker_clichan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud);
void rwmsg_broker_clichan_release(rwmsg_broker_clichan_t *cc);


rwmsg_broker_clichan_t *rwmsg_peer_clichan_create(rwmsg_broker_t *bro,
						  struct rwmsg_broker_channel_acceptor_key_s *key);
rw_status_t rwmsg_peer_clichan_recv_buf(rwmsg_broker_clichan_t *cc, rwmsg_buf_t *buf);
uint32_t rwmsg_peer_clichan_recv(rwmsg_broker_clichan_t *cc);
void rwmsg_peer_clichan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud);
void rwmsg_peer_clichan_release(rwmsg_broker_clichan_t *cc);

typedef struct rwtasklet_info_s rwtasklet_info_t;

struct rwmsg_broker_s {
  /* Base endpoint structure; ep and/or rwmsg_global include index of
     all locally connected clients in localpathtab and an index of all
     local path+payt+methno -> brosrvchan bindings in localdesttab */
  rwmsg_endpoint_t *ep;
  rwsched_tasklet_ptr_t tinfo;
  rwtasklet_info_t *rwtasklet_info;
  rwvcs_zk_module_ptr_t rwvcs_zk;
  uint32_t sysid;
  uint32_t bro_instid;

  /* Socket we listen and accept on */
  rwmsg_broker_acceptor_t acc;
  uint32_t chanid_nxt;

  uint32_t use_mainq:1;
  uint32_t audit_completed:1;
  uint32_t audit_succeeded:1;
  uint32_t halted:1;
  uint32_t _pad:28;

  uint32_t reqct;
  uint32_t thresh_reqct1;	/* xon */
  uint32_t thresh_reqct2;	/* xoff */
  // TBD size tracking and thresholds

  /* rwxk related */
  rwvcs_zk_closure_ptr_t closure;
  uint32_t data_watcher_cbct;	/* callback count */

  struct timeval audit_completed_tv;

  uint32_t refct;
  rwmemlog_instance_t *rwmemlog;
  int   rwmemlog_id;
};

struct rwmsg_broker_global_s {
  /* Assorted flags for test harness */
  struct {
    uint32_t acc_listening;
    uint32_t neg_accepted;
    uint32_t neg_freed;
    uint32_t neg_freed_err;
    uint32_t method_bindings;
    uint32_t bch_count;
  } exitnow;

  rwmsg_broker_t *abroker;
  uint32_t count;
};
extern struct rwmsg_broker_global_s rwmsg_broker_g;

/* Note that this takes a reference on the endpoint */
rwmsg_broker_t *rwmsg_broker_create(uint32_t sysid,
                                    uint32_t bro_instid,
                                    char *ext_ip_address,
                                    rwsched_instance_ptr_t rws,
                                    rwsched_tasklet_ptr_t tinfo,
                                    rwvcs_zk_module_ptr_t rwvcs_zk,
                                    uint32_t usemainq,
                                    rwmsg_endpoint_t *ep,
                                    rwtasklet_info_t *rwtasklet_info);
rwmsg_bool_t rwmsg_broker_halt(rwmsg_broker_t *bro);
rwmsg_bool_t rwmsg_broker_destroy(rwmsg_broker_t *bro);
rwmsg_bool_t rwmsg_broker_halt_sync(rwmsg_broker_t *bro);


void rwmsg_broker_main(uint32_t sid,
		       uint32_t instid,
		       uint32_t bro_instid,
		       rwsched_instance_ptr_t rws,
		       rwsched_tasklet_ptr_t tinfo,
		       rwvcs_zk_module_ptr_t rwvcs_zk,
           uint32_t usemainq,
           rwtasklet_info_t *rwtasklet_info,
		       rwmsg_broker_t **bro_out);

void rwmsg_broker_bcast_pausecli(rwmsg_broker_channel_t *bch);
void rwmsg_broker_bcast_resumecli(rwmsg_broker_channel_t *bch);

__END_DECLS

#endif // __RWMSG_BROKER_H__
