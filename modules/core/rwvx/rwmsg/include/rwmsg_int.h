
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_int.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Implementation include for RW.Msg component
 */

#ifndef __RWMSG_INT_H__
#define __RWMSG_INT_H__

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#include <ck.h>

#include <uthash.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>

#include <rwvx.h>
#include <rwcal-api.h>

#include "rwmsg.h"
#include "rwmsg_sockset.h"
#include "rwmsg_notify.h"
#include "rwmsg_queue.h"
#include "rwlog.h"
#include <rwmemlog.h>


__BEGIN_DECLS

#define RWMSG_TRACE(ep, lvl, fmt, args...)				\
  RWTRACE_##lvl((ep)->rwtctx, RWTRACE_CATEGORY_RWMSG, "%p:%s " fmt, (ep)->taskletinfo, (ep)->rwtpfx, args);
#define RWMSG_TRACE0(ep, lvl, fmt)					\
  RWTRACE_##lvl((ep)->rwtctx, RWTRACE_CATEGORY_RWMSG, "%p:%s " fmt, (ch)->ep->taskletinfo, (ep)->rwtpfx);
#define RWMSG_TRACE_CHAN(ch, lvl, fmt, args...)				\
  RWTRACE_##lvl((ch)->ep->rwtctx, RWTRACE_CATEGORY_RWMSG, "%p:%s%s " fmt, (ch)->ep->taskletinfo, (ch)->ep->rwtpfx, (ch)->rwtpfx, args);


typedef struct rwmsg_broker_srvchan_s rwmsg_broker_srvchan_t;
typedef struct rwmsg_broker_srvchan_cli_s rwmsg_broker_srvchan_cli_t;
typedef struct rwmsg_broker_clichan_s rwmsg_broker_clichan_t;
typedef struct rwmsg_brosrvchan_s rwmsg_brosrvchan_t;
typedef struct rwmsg_stream_s rwmsg_stream_t;

typedef struct {
  uint32_t broid:18;		/* ID of originating broker 1-262,143 */
  uint32_t chanid:14;		/* ID of originating client within originating broker 1-16,383 */
  uint32_t locid;		/* Client-local message ID */
} rwmsg_id_t;

#define RWMSG_ID_TO_BROCLI_ID(id) ((uint32_t)(((id)->broid << 18) | ((id)->chanid)))

typedef struct {
  uint32_t         isreq:1;
  rwmsg_bounce_t   bnc:4;
  rwmsg_payfmt_t   payt:3;
  rwmsg_priority_t pri:3;
  uint32_t         blocking:1;
  uint32_t         cancel:1;
  uint32_t         nop:1;
  uint32_t         ack:1;
  uint32_t         _pad:1;
  uint32_t timeo:16;		/* centiseconds, max is 65536cs aka 655s */
  rwmsg_id_t id;		/* Globally unique request ID */
  uint64_t pathhash;		/* Destination path */
  uint32_t methno;
  uint32_t paysz;
  uint32_t stid;		/* Stream ID */
  union {
    struct {
      /* (cli -> srv) */
      //uint32_t ackid;		/* Ack number, matches locid */
      rwmsg_id_t ackid; /* piggy-backed or regular ack */
      uint32_t seqno;		/* Sequence number, within stream */
    };
    struct {
      /* (srv -> cli) */
      uint16_t stwin;		/* Stream window value */
      uint16_t _pad2;
      uint32_t _pad3;
    };
  };
  //...
#define RWMSG_REQ_TRACKING_COUNT 10
  struct {
    struct {
      struct timeval tv;
      uint32_t line;
    } _[RWMSG_REQ_TRACKING_COUNT];
    uint32_t ct;
  } tracking;
} rwmsg_header_t;

#define RWMSG_REQ_TRACK(_req_) \
{ \
  if ((_req_)->hdr.tracking.ct<RWMSG_REQ_TRACKING_COUNT) { \
    gettimeofday(&(_req_)->hdr.tracking._[(_req_)->hdr.tracking.ct].tv, NULL); \
    (_req_)->hdr.tracking._[(_req_)->hdr.tracking.ct].line = __LINE__; \
    (_req_)->hdr.tracking.ct++; \
  } \
}

#define RWMSG_HEADER_WIRESIZE (sizeof(rwmsg_header_t))

/* Set to half the sequence number space to provide a blush of disambiguation */
#define RWMSG_WINDOW_MAX (65535)

/* Various parameters applied to our TCP sockets, both nanomsg- and rwmsg-created */
#define RWMSG_SO_SNDBUF (32*1024*1024)
#define RWMSG_SO_RCVBUF (32*1024*1024)
#define RWMSG_LISTEN_Q  (INT_MAX)

enum rwmsg_msgctl_methno_e {
  RWMSG_MSGCTL_METHNO_METHBIND=1,
};

struct rwmsg_buf_s {
  void *nnbuf;
  uint32_t nnbuf_len;
  uint32_t nnbuf_nnmsg:1;
};

struct rwmsg_message_s {
  struct rwmsg_buf_s msg;
};

struct rwmsg_request_s {
  rwmsg_header_t hdr;
#define RWMSG_REQUEST_LEAK_TRACKING
#ifdef RWMSG_REQUEST_LEAK_TRACKING
  void *callers[RW_RESOURCE_TRACK_MAX_CALLERS];
  rw_dl_element_t trackelem;
#endif
  uint32_t refct;
  rwmemlog_buffer_t *rwml_buffer;

  /* Problematic bitfield in broker, all fields owned by brosrvchan so we squeak by for now */
  uint32_t _pad:27;
  uint32_t local:1;		/* not used in broker */
  uint32_t inq:1;		/* brosrvchan */
  uint32_t sent:1;		/* : */
  uint32_t out:1;		/* : */
  uint32_t inhash:1;            /* :  TRUE if in brosrvchan's req_hash */

  uint32_t bch_id;

  uint32_t methidx;
  rwmsg_message_t req;
  rwmsg_message_t rsp;
  rwmsg_closure_t cb;
  rwmsg_destination_t *dest;	/* should just be clistream */
  union {
    rwmsg_clichan_t *clichan;
    rwmsg_broker_clichan_t *broclichan;
  };
  union {
    rwmsg_srvchan_t *srvchan;
    rwmsg_broker_srvchan_t *brosrvchan;
  };
  ProtobufCService *pbsrv;
  ProtobufCService *pbcli;
  union {
    rwmsg_stream_t *st;		/* in clichan */
    rwmsg_broker_srvchan_cli_t *brosrvcli; /* in broker */
  };
  rwsched_dispatch_queue_t rwq;
  UT_hash_handle hh;		/* clichan->outreqs AND brosrvchan->req_hash */
  rw_dl_element_t elem;

  struct timeval rtt;
};

rw_status_t rwmsg_buf_unload_header(rwmsg_buf_t *m, 
				    rwmsg_header_t *hdr_out);
void rwmsg_request_message_load_header(rwmsg_message_t *m,
				       rwmsg_header_t *hdr);
uint32_t rwmsg_request_message_load(rwmsg_message_t *m,
				    const void *ptr, 
				    uint32_t len);
void rwmsg_request_message_free(rwmsg_message_t *m);

enum rwmsg_notify_theme_e {
  RWMSG_NOTIFY_INVALID=0,
  RWMSG_NOTIFY_EVENTFD,
  RWMSG_NOTIFY_RWSCHED,
  RWMSG_NOTIFY_RWSCHED_FD,
  RWMSG_NOTIFY_CFRUNLOOP,
  RWMSG_NOTIFY_CFRUNLOOP_FD,
  RWMSG_NOTIFY_NONE,
};
struct rwmsg_notify_s {
  char name[64];
  rwmsg_endpoint_t *ep;
  enum rwmsg_notify_theme_e theme;
  int fd;
  int order;			/* scheduler priority */
  uint32_t paused:1;
  uint32_t halted:1;
  uint32_t cancelack:1;
  uint32_t wantack:1;
  uint32_t _pad:28;
  union {
    struct {
      uint64_t dbgval;
      //?? writeevfd?
    } eventfd;
    struct {
      rwsched_tasklet_ptr_t taskletinfo;
      rwsched_instance_ptr_t rwsched;
      rwsched_dispatch_source_t rwsource;
      rwsched_dispatch_queue_t rwqueue;
      uint32_t clearct;
    } rwsched;
    struct {
      uint64_t dbgval;
      rwsched_tasklet_ptr_t taskletinfo;
      rwsched_instance_ptr_t rwsched;
      rwsched_CFSocketRef cfsocket;
      rwsched_CFRunLoopSourceRef cfsource;
    } cfrunloop;
  };
  rwmsg_closure_t cb;
};

struct rwmsg_sockset_s {
  enum {
    RWMSG_SOCKSET_SKTYPE_NN=0
  } sktype;
  union {
    struct {
      struct rwmsg_sockset_nnsk_s {
        enum {
	  RWMSG_NNSK_IDLE=0,
	  RWMSG_NNSK_CONNECTING=1,
	  RWMSG_NNSK_CONNECTED=2,
	} state;
	uint32_t paused:1;
	uint32_t pollout:1;
	uint32_t cb_while_paused:1;
	uint32_t _pad:29;
	int sk;

	// eventfd descriptors
	int evfd_in;
	int evfd_out;

	// indices of sched bindings for eventfd descriptors
	rwmsg_notify_t notein;
	rwmsg_notify_t noteout;

	uint8_t pri;
      } sk[RWMSG_PRIORITY_COUNT];
    } nn;
    struct {
      //...
    } unnn;
  };

  rwmsg_bool_t ageout;
  rwsched_dispatch_source_t ageout_timer;

  struct {
    enum rwmsg_notify_theme_e theme;
    rwmsg_sockset_closure_t clo;
    rwsched_instance_ptr_t rwsched;
    rwsched_dispatch_queue_t rwsqueue;
    rwsched_tasklet_ptr_t taskletinfo;
  } sched;

  uint8_t hsval[16];
  size_t hsval_sz;
  rwmsg_endpoint_t *ep;
  rw_dl_element_t trackelem;
  uint32_t refct;
  uint32_t _pad:22;
  uint32_t sk_ct:3;
  uint32_t sk_mask:6;		/* assert in rwmsg_socket_create */
  uint32_t closed:1;
};

struct rwmsg_queue_s {
  /* We would want a ck_ring_t, mpsc flavor, but that doesn't exist! */
  ck_fifo_mpmc_t fifo[RWMSG_PRIORITY_COUNT];

  uint32_t magic;

  uint32_t qlen;
  uint32_t qlencap;

  uint32_t qsz;
  uint32_t qszcap;

  rwmsg_notify_t *notify;	/* what/who do we notify?  */
  rwmsg_notify_t _notify;	/* placeholder standalone notification */
  uint32_t detickle;
  uint32_t full;
  uint32_t paused:1;
  uint32_t paused_hit:1;
  uint32_t _pad:30;

  rwmsg_closure_t feedme;	/* backpressure end cb */

  rwmsg_endpoint_t *ep;
  rw_dl_element_t trackelem;
};

void rwmsg_queue_register_feedme(rwmsg_queue_t *q, rwmsg_closure_t *feedme);

#define RWMSG_QUEUE_DEFLEN (128)
#define RWMSG_QUEUE_DEFSZ  (128*1024)

/* Placeholder until this is demucked */
//#define RWMSG_RUNLOOP_MODE CFSTR("TimerMode")
#define RWMSG_RUNLOOP_MODE kCFRunLoopDefaultMode


/* Base object for channels */
typedef struct rwmsg_channel_s rwmsg_channel_t;
enum rwmsg_chantype_e {
  RWMSG_CHAN_NULL,
  RWMSG_CHAN_CLI,
  RWMSG_CHAN_SRV,
  RWMSG_CHAN_BROSRV,
  RWMSG_CHAN_BROCLI,
  RWMSG_CHAN_PEERSRV,
  RWMSG_CHAN_PEERCLI,
  //RWMSG_CHAN_BROPEER

  RWMSG_CHAN_CT
};


struct rwmsg_channel_funcs {
  enum rwmsg_chantype_e chtype;
  void (*halt)(rwmsg_channel_t *ch);
  void (*release)(rwmsg_channel_t *ch);
  uint32_t (*recv)(rwmsg_channel_t *ch);
  rw_status_t (*recv_buf)(rwmsg_channel_t *cc, rwmsg_buf_t *buf);
  void (*ss_send)(rwmsg_sockset_t *sk, rwmsg_priority_t pri, void *ud);
};
struct rwmsg_channel_s {
  enum rwmsg_chantype_e chantype;
  uint32_t chanid;
  rwmsg_endpoint_t *ep;
  rwsched_tasklet_ptr_t taskletinfo;
  struct rwmsg_channel_funcs *vptr;
  rwmsg_notify_t notify;
  rwmsg_queue_t localq;		/* local delivery (return into clichan pointed in request_t) */
  rwmsg_queue_t ssbufq;		/* buffering of transmits */
  rwmsg_sockset_t *ss;		/* remote delivery and return */
  uint32_t tickle:1;
  uint32_t halted:1;
  uint32_t paused_ss:1;
  uint32_t paused_q:1;
  uint32_t paused_user:1;
  uint32_t _pad:27;
  char rwtpfx[32];
  struct {
    uint64_t msg_in_sock;
  } chanstat;
  rw_dl_element_t trackelem;
  rw_dl_t methbindings;
  uint32_t refct;
};

static bool inline rwmsg_chan_is_brotype(rwmsg_channel_t *ch) {
  return ((ch->chantype) >= RWMSG_CHAN_BROSRV && (ch->chantype) <= RWMSG_CHAN_PEERCLI);
}
void rwmsg_channel_halt(rwmsg_channel_t *ch);
void rwmsg_channel_release(rwmsg_channel_t *ch);
void rwmsg_channel_pause_ss(rwmsg_channel_t *ch);
void rwmsg_channel_pause_q(rwmsg_channel_t *ch);
void rwmsg_channel_resume_ss(rwmsg_channel_t *ch);
void rwmsg_channel_resume_q(rwmsg_channel_t *ch);

rw_status_t rwmsg_channel_bind_rwsched_queue(rwmsg_channel_t *ch,
					     rwsched_dispatch_queue_t q);
rw_status_t rwmsg_channel_bind_rwsched_cfrunloop(rwmsg_channel_t *ch, 
						 rwsched_tasklet_ptr_t taskletinfo);
void rwmsg_channel_destroy(rwmsg_channel_t *ch);
void rwmsg_channel_create(rwmsg_channel_t *ch, rwmsg_endpoint_t *ep, enum rwmsg_chantype_e typ);
void rwmsg_channel_load_funcs(struct rwmsg_channel_funcs *funcs, int ct);
rw_status_t rwmsg_channel_send_buffer_pri(rwmsg_channel_t *ch, rwmsg_priority_t pri, int pauseonxoff, int sendreq);

struct rwmsg_handshake_s {
  uint32_t chanid;
  uint32_t pid;
  rwmsg_priority_t pri:16;
  enum rwmsg_chantype_e chtype:16;
  uint32_t instid;
};


struct rwmsg_stream_s {
  struct rwmsg_stream_key_s {
    uint64_t pathhash;
    uint32_t streamid;		/* assigned by dest / dest broker_srvchan */
  } key;

  uint32_t defstream:1;
  uint32_t _pad:31;

  uint32_t tx_win;		/* assigned by interim broker(s) */
  uint32_t tx_out;
  uint32_t seqno;

  uint32_t refct;

  rwmsg_srvchan_t *localsc;

  rwmsg_closure_t cb;		/* user flow control callback */
  rwmsg_destination_t *dest;	/* to hand back to user */

  //?? tick / age
  UT_hash_handle hh;

  rw_dl_element_t socket_defer_elem[RWMSG_PRIORITY_COUNT]; /* list of streams in which we hit EAGAIN on a socket write EAGAIN.  see also in_socket_defer! */
  uint8_t in_socket_defer[RWMSG_PRIORITY_COUNT];
  // pad TBD

#if 0
  // TBD
  rw_dl_element_t queue_defer_elem[RWMSG_PRIORITY_COUNT]; /* list of streams in which we hit EAGAIN on a local enqueue.  see also in_queue_defer! */
  uint8_t in_queue_defer[RWMSG_PRIORITY_COUNT];
#endif
};

typedef struct {
  struct rwmsg_stream_idx_key_s {
    uint64_t pathhash;
    uint32_t methno;
    rwmsg_payfmt_t payt;
  } key;
  rwmsg_stream_t *stream;
  UT_hash_handle hh;
} rwmsg_stream_idx_t;

struct rwmsg_destination_s {
  char apath[RWMSG_PATH_MAX];
  rwmsg_addrtype_t atype;
  uint64_t apathhash;
  rwmsg_stream_t defstream;

  uint32_t apathserial;
  rwmsg_endpoint_t *localep;

  uint32_t destination_ct;
  uint32_t outstanding_ct;

  uint32_t noconndontq:1;
  uint32_t _pad:31;

  uint32_t refct;
  rwmsg_endpoint_t *ep;
  rw_dl_element_t trackelem;

  uint32_t feedme_count;
};


/* Client channel */
struct rwmsg_clichan_s {
  rwmsg_channel_t ch;
  uint32_t shunt:1;
  uint32_t started:1;
  uint32_t _pad:30;

  uint32_t bch_id;

  uint32_t feedme_count;

  /* This has an N:1 map of signatures -> stream objects for this
     dest.  Streams are created automagically as returned requests
     tell us the chanid values to use (and affiliated window size).
     Until we have learned chanid+window, we use the default stream
     object, which has chanid 0 and some window synthesized by the
     local broker. */
  rwmsg_stream_idx_t *streamidx; /* by path/payt/meth (N:1 ppm:stream) */
  rwmsg_stream_t *streamhash;	 /* by stream id */
  rw_dl_t streamdefer[RWMSG_PRIORITY_COUNT];		 /* list of socket flow controlled streams (ie those which hit EAGAIN) */

  /* Index of all outstanding requests, for correlating responses */
  uint32_t nxid;		/* next ID */
  rwmsg_request_t *outreqs;	/* UTHASH */

  /* Blocking state */
  struct {
    rwmsg_request_t *req;
    rwmsg_notify_t notify;
    //    rwsched_CFSocketRef cfsock;
    //    rwsched_CFRunLoopSourceRef cfsrc;
    // nnsk...
  } blk;
};

rw_status_t rwmsg_clichan_recv_buf(rwmsg_clichan_t *sc, rwmsg_buf_t *buf);
void rwmsg_clichan_sockset_event_send(rwmsg_sockset_t *sk, rwmsg_priority_t pri, void *ud);

enum rwmsg_methb_type_e {
  RWMSG_METHB_TYPE_SRVCHAN,
  RWMSG_METHB_TYPE_BROSRVCHAN,
};
struct rwmsg_methbinding_key_s {
  enum rwmsg_methb_type_e bindtype;
  uint32_t bro_instid;
  uint64_t pathhash;
  uint32_t methno;
  rwmsg_payfmt_t payt;
};

#define RWMSG_PBAPI_METHOD_BITS (10)
#define RWMSG_PBAPI_SERVICE_BITS (32-RWMSG_PBAPI_METHOD_BITS)

#define RWMSG_PBAPI_METHOD(s, m) ((((s)&((1<<RWMSG_PBAPI_SERVICE_BITS)-1))<<RWMSG_PBAPI_METHOD_BITS) \
				  | (((m)&((1<<RWMSG_PBAPI_METHOD_BITS)-1))))
#define RWMSG_PBAPI_METHOD_MAX ((1<<RWMSG_PBAPI_METHOD_BITS)-1)
#define RWMSG_PBAPI_SERVICE_MAX ((1<<RWMSG_PBAPI_SERVICE_BITS)-1)
#define RWMSG_PBAPI_METHOD_SRVHASHVAL(m) ((m)&(~((1<<RWMSG_PBAPI_METHOD_BITS)-1)))
#define RWMSG_PBAPI_SUBVAL_METHOD(m)     ((m)&((1<<RWMSG_PBAPI_METHOD_BITS)-1))
#define RWMSG_PBAPI_SUBVAL_SERVICE(m)    (((m) >> RWMSG_PBAPI_METHOD_BITS) & ((1<<RWMSG_PBAPI_SERVICE_BITS)-1))


struct rwmsg_srvchan_method_s {
  struct rwmsg_methbinding_key_s k;
  UT_hash_handle hh;
  rwmsg_method_t *meth;
  uint32_t dirty:1;
  uint32_t _pad:31;
  char path[RWMSG_PATH_MAX];
};


struct rwmsg_srvchan_s {
  rwmsg_channel_t ch;

  /* Local index/table of methods, copies no less, to provide a thread/numa local
     lookup after socket read */
  struct rwmsg_srvchan_method_s *meth_hash;
  uint32_t dirtyct;

  uint32_t unbound;
};

void rwmsg_srvchan_sockset_event_send(rwmsg_sockset_t *sk, rwmsg_priority_t pri, void *ud);
rw_status_t rwmsg_srvchan_recv_buf(rwmsg_srvchan_t *sc, rwmsg_buf_t *buf);


enum rwmsg_garbage_type_e {
  RWMSG_OBJTYPE_NULL=0,
  RWMSG_OBJTYPE_ENDPOINT=1,
  RWMSG_OBJTYPE_METHOD,
  RWMSG_OBJTYPE_SIGNATURE,
  RWMSG_OBJTYPE_DESTINATION,
  RWMSG_OBJTYPE_SRVCHAN,
  RWMSG_OBJTYPE_CLICHAN,
  RWMSG_OBJTYPE_RWSCHED_OBJREL,
  RWMSG_OBJTYPE_SOCKSET,
  RWMSG_OBJTYPE_GENERIC,
  //...
  /* See also char*rwmsg_objtype[] in rwmsg_endpoint.c */
  RWMSG_OBJTYPE_OBJCT
};

struct rwmsg_garbage_s {
  enum rwmsg_garbage_type_e objtype;
  void *objptr;
  void *ctxptr;
  void *tinfo;
  uint64_t tick;
  rw_dl_element_t elem;
  char *dbgfile;
  int dbgline;
  void *dbgra;
};

typedef struct rwmsg_garbage_truck_s rwmsg_garbage_truck_t;

struct rwmsg_garbage_truck_s {
  rwmsg_endpoint_t *ep;
  uint64_t tick;
  rwsched_dispatch_source_t rws_timer;

  /* Fugly queue of deferred GC objects, struct rwmsg_garbage_s.  For
     now, pending a per-thread quiescent timestamp mechanism, we just
     wait two ticks after the initially observed zero refct, recheck
     refct, and either release or not depending, in the main thread.
     This eliminates races of the form deref-to-zero|ref-to-one, but
     does not permit indefinite holding of an uncounted reference in a
     terminating or acquiring thread. */
  rw_dl_t garbageq;
};

#define rwmsg_garbage(gc, typ, objptr, ctxptr, tinfo) \
  rwmsg_garbage_impl((gc), (typ), (objptr), (ctxptr), (tinfo), __FILE__, __LINE__, __builtin_return_address(0));
void rwmsg_garbage_impl(rwmsg_garbage_truck_t *gc,
			enum rwmsg_garbage_type_e typ,
			void *objptr,
			void *ctxptr,
			void *tinfo,
			const char *dbgfile,
			int dbgline,
			void *dbgra);

#if 0
//junk?
void rwmsg_garbage_barrier(rwmsg_garbage_truck_t *gc,
			   enum rwmsg_garbage_type_e typ,
			   void *objptr,
			   void *ctxptr,
			   void *tinfo,
			   const char *dbgfile,
			   int dbgline);
#endif
void rwmsg_garbage_truck_init(rwmsg_garbage_truck_t *gc, 
			      rwmsg_endpoint_t *ep);
int rwmsg_garbage_truck_tick(rwmsg_garbage_truck_t *gc);
void rwmsg_garbage_truck_flush(rwmsg_garbage_truck_t *gc, 
			       int drain);

void rwmsg_clichan_destroy(rwmsg_clichan_t *cc);
void rwmsg_srvchan_destroy(rwmsg_srvchan_t *sc);
void rwmsg_method_destroy(rwmsg_endpoint_t *ep, rwmsg_method_t *meth);
void rwmsg_signature_destroy(rwmsg_endpoint_t *ep, rwmsg_signature_t *sig);
void rwmsg_destination_destroy(rwmsg_destination_t *dt);


struct rwmsg_endpoint_global_s {
  pthread_mutex_t rgmut;	/* recursive mutex */
  int nn_inited;
  uint32_t chanid_nxt;
  uint32_t epid_nxt;

  rwmsg_global_status_t status;

  /* Index of all local paths */
  ck_ht_t localpathtab;

  /* Index of all local path-payt-methno bindings.
     rwmsg_methbinding_key_s -> rwmsg_methbinding_s */
  ck_ht_t localdesttab;

#ifdef RWMSG_REQUEST_LEAK_TRACKING
  struct {
    rw_dl_t requests;
  } track;
#endif

  /* Totally broken, eventfd fd numbers are from the process fd namespace, unrelated to nn socket count etc */
  rwmsg_closure_t cbs[NN_EVENTFD_DIRECT_OFFSET];
};
extern struct rwmsg_endpoint_global_s rwmsg_global;

void rwmsg_endpoint_clear_nninit(void);


typedef struct rwmsg_path_s {
  char path[RWMSG_PATH_MAX];	/* fully qualified, sid, instance, the whole bit */

  uint64_t pathhash;		/* hash of fully qualified name (number etc) */

  uint8_t ppfxlen;		/* len of prefix only part */
  uint32_t _pad:24;
  uint64_t ppfxhash;		/* hash of prefix part */
  uint32_t instance;		/* is this even useful? */

  rwmsg_endpoint_t *ep;
  rwmsg_method_t *meth;

  uint32_t refct;
} rwmsg_path_t;


struct rwmsg_method_s {
#define RWMSG_METHOD_FLAG_TCP_ACCEPT (1<<0);
  uint32_t flags;

  uint32_t pathidx;
  uint64_t pathhash;
  rwmsg_signature_t *sig;
  rwmsg_closure_t cb;

  rwmsg_closure_t tcp_cb;
  int accept_fd;

  ProtobufCService *pbsrv;
  uint32_t pbmethidx;
  char path[RWMSG_PATH_MAX];

  uint32_t refct;
};

struct rwmsg_signature_s {
  uint32_t methno;	/* user-interpreted types for raw or text; srvno+methno api-serialized protobuf for PBAPI */
  rwmsg_payfmt_t payt;

  rwmsg_priority_t pri;
  uint32_t timeo;

  //...
  uint32_t refct;
};

struct rwmsg_methbinding_s {
  struct rwmsg_methbinding_key_s k;

  uint32_t local_service:1;
  uint32_t _pad:31;

  rwmsg_endpoint_t *ep;
  rwmsg_method_t *meth;
  char path[RWMSG_PATH_MAX];

  uint32_t srvchans_ct;
  struct rwmsg_methbinding_val_s {
    union {
      rwmsg_channel_t *ch;
      rwmsg_srvchan_t *sc;
      rwmsg_broker_srvchan_t *bsc;
    };
#define RWMSG_METHBINDING_SRVCHAN_MAX (1)
  } srvchans[RWMSG_METHBINDING_SRVCHAN_MAX];
  struct {
    uint32_t found_called;
  } stat;
  rw_dl_element_t elem;
};

struct rwmsg_endpoint_s {
  // tasklet context (not just rwsched, what?)
  rwsched_instance_ptr_t rwsched;
  rwsched_tasklet_ptr_t taskletinfo;
  rwtrace_ctx_t *rwtctx;
  rwlog_ctx_t *rwlog_instance;
  char rwtpfx[64];

  uint32_t sysid;
  uint32_t instid;
  uint32_t bro_instid;
  uint32_t epid;

  // single utility conn to broker
  enum rwmsg_broker_state broker_state;
  rwmsg_sockset_t sk;

  /* Stats, state, etc */
  rwmsg_endpoint_status_t stat;

  uint32_t refct;

  struct {
    int fd;
    int events;
    int timeout;
    rwsched_dispatch_source_t rwsource_fd_read;
    rwsched_dispatch_source_t rwsource_fd_write;
    rwsched_dispatch_source_t rwsource_timer;
  } nn;

  /******/

  rwmsg_request_t *lastreq;

  /* Items below here are serialized with a big honking lock */
  pthread_mutex_t epmut;	/* recursive mutex */
  const char * _FILE_;
  int32_t _LINE_;

  /* Unindexed table of local paths, 0 unused */
#define RWMSG_PATHTAB_MAX (64)
  struct rwmsg_path_s paths[RWMSG_PATHTAB_MAX];
  uint32_t paths_nxt;

#define RWMSG_METHBIND_MAX (10240)
  struct rwmsg_methbinding_s methbind[RWMSG_METHBIND_MAX];
  uint32_t methbind_nxt;

  /* Tracking data, in general lists of every interesting data
     structure so we can run around and gather stats etc */
  struct {
    rw_dl_t queues;
    rw_dl_t destinations;
    rw_dl_t srvchans;
    rw_dl_t clichans;
    rw_dl_t socksets;
    rw_dl_t brosrvchans;	/* broker end of srvchan */
    rw_dl_t broclichans;	/* broker end of clichan */
    rw_dl_t peersrvchans;	/* peer-broker end of srvchan */
    rw_dl_t peerclichans;	/* peer-broker end of clichan */
    rw_dl_t bropeerchans;	/* broker to broker channel */
  } track;

  rwmsg_garbage_truck_t gc;

  /* Properties, local mirror of zk-stored (?), broker mirrored, shm
     visible property set.  Mostly for low level knobs within
     riftware. */
  struct rwmsg_endpoint_prop_t {
    char *path;
    enum {
      RWMSG_EP_PROPTYPE_INT32,
      RWMSG_EP_PROPTYPE_STRING
    } valtype;
    union {
      int32_t ival;
      char *sval;
    };
  } *localprops;
  int localprops_ct;
};

#define RWMSG_EP_LOCK(ep) {			\
  int r = pthread_mutex_lock(&(ep)->epmut);	\
  RW_ASSERT(r == 0);				\
  if (r) { abort(); }				\
  (ep)->_FILE_ = __FILE__; \
  (ep)->_LINE_ = __LINE__; \
}
#define RWMSG_EP_UNLOCK(ep) {			\
  int r = pthread_mutex_unlock(&(ep)->epmut);	\
  RW_ASSERT(r == 0);				\
}

#define RWMSG_RG_LOCK() {				\
  int r = pthread_mutex_lock(&rwmsg_global.rgmut);	\
  RW_ASSERT(r == 0);					\
  if (r) { abort(); }					\
}
#define RWMSG_RG_UNLOCK() {				\
  int r = pthread_mutex_unlock(&rwmsg_global.rgmut);	\
  RW_ASSERT(r == 0);					\
}

#define RWMSG_BSC_LOCK(bsc) {                          \
  int r = pthread_mutex_lock(&(bsc)->bscmut);  \
  RW_ASSERT(r == 0);                                   \
  if (r) { abort(); }                                  \
}
#define RWMSG_BSC_UNLOCK(bsc) {                                \
  int r = pthread_mutex_unlock(&(bsc)->bscmut);        \
  RW_ASSERT(r == 0);                                   \
}

rw_status_t rwmsg_endpoint_add_channel_method_binding(rwmsg_endpoint_t *ep, 
						      rwmsg_channel_t *ch, 
						      enum rwmsg_methb_type_e btype,
						      rwmsg_method_t *method);
rwmsg_bool_t rwmsg_endpoint_del_channel_method_bindings(rwmsg_endpoint_t *ep,
							rwmsg_channel_t *ch);
rwmsg_method_t *rwmsg_endpoint_find_method(rwmsg_endpoint_t *ep,
					   uint64_t pathhash,
					   rwmsg_payfmt_t payt,
					   uint32_t methno);
rwmsg_endpoint_t *rwmsg_endpoint_path_localfind_ref(rwmsg_endpoint_t *ep,
						    const char *path);
rwmsg_channel_t *rwmsg_endpoint_find_channel(rwmsg_endpoint_t *ep,
					     enum rwmsg_methb_type_e btype,
					     uint32_t bro_instid,
					     uint64_t pathhash,
					     rwmsg_payfmt_t payt,
					     uint32_t methno);
uint64_t rwmsg_endpoint_path_hash(rwmsg_endpoint_t *ep,
				  const char *path);
uint32_t rwmsg_endpoint_path_add(rwmsg_endpoint_t *ep,
				 const char *path,
				 uint64_t *pathhash_out);
rwmsg_bool_t rwmsg_endpoint_path_del(rwmsg_endpoint_t *ep,
				     uint32_t pathidx);
void rwmsg_queue_set_notify(rwmsg_endpoint_t *ep,
			    rwmsg_queue_t *q,
			    rwmsg_notify_t *notify);

rw_status_t rwmsg_send_methbinding(rwmsg_channel_t *ch,
                                   rwmsg_priority_t pri,
                                   struct rwmsg_methbinding_key_s *k,
                                   const char *path);

void rwmsg_channelspecific_halt(rwmsg_channel_t *ch);
void rwmsg_broker_acceptor_remove_bch_from_hash(void *ctx);


#define RWMSG_SLEEP_MS(x) {						\
  struct timespec ts = { .tv_sec = (x)/1000, .tv_nsec = (x) * 1000000 }; \
  int rv = 0;								\
  do {									\
    rv = nanosleep(&ts, &ts);						\
  } while (rv == -1 && errno == EINTR);					\
}


#define FMT_MSG_HDR(hdr) " id=%u/%u/%u bnc=%u isreq=%u nop=%u pri=%u paysz=%u block=%u ack=%u ackid=%u/%u/%u destination 0x%lx/%u/%u st=%u seqno=%u "
#define PRN_MSG_HDR(hdr) \
                        (hdr).id.broid, (hdr).id.chanid, (hdr).id.locid,\
                        (hdr).bnc,\
                        (hdr).isreq,\
                        (hdr).nop,\
                        (hdr).pri,\
                        (hdr).paysz,\
                        (hdr).blocking,\
                        (hdr).ack,\
                        (hdr).ackid.broid, (hdr).ackid.chanid, (hdr).ackid.locid,\
                        (hdr).pathhash, (hdr).payt, (hdr).methno,\
                        (hdr).stid,\
                        (hdr).seqno

//#define PIGGY_ACK_DEBUG 1

#define __QUOTE(x) #x
#define QUOTE(x) __QUOTE(x)

#define BASE_BROKER_PORT 21234
#define BASE_BROKER_HOST "tcp://127.0.0.1"
#define BASE_BROKER_URL BASE_BROKER_HOST ":" QUOTE(BASE_BROKER_PORT)

#define RWMSG_ENABLE_UID_SEPERATION
#ifdef RWMSG_ENABLE_UID_SEPERATION

#define __RWMSG_CONNECT_ADDR_FMT__ "127.0.%u.1"

#define RWMSG_CONNECT_IP_ADDR  \
    ({ \
       char acceptor_addr_str[999]; \
       uint8_t uid = 0; \
       if (getenv("RIFT_INSTANCE_UID")) { \
         rw_instance_uid(&uid); \
       } \
       sprintf(acceptor_addr_str, __RWMSG_CONNECT_ADDR_FMT__ , uid); \
       ntohl(inet_addr(acceptor_addr_str)); \
     })

#define RWMSG_CONNECT_ADDR_STR  \
    ({ \
       static char acceptor_addr_str[999]; \
       uint8_t uid = 0; \
       if (getenv("RIFT_INSTANCE_UID")) { \
         rw_instance_uid(&uid); \
       } \
       sprintf(acceptor_addr_str, __RWMSG_CONNECT_ADDR_FMT__ , uid); \
       acceptor_addr_str; \
     })
#else
#define RWMSG_CONNECT_IP_ADDR ntohl(inet_addr("127.0.0.1"))
#define RWMSG_CONNECT_ADDR_STR "127.0.0.1"
#endif

#if 0
#define RWMSG_PRINTF(args...) \
  fprintf(stderr, __VA_ARGS__);
#else
#define RWMSG_PRINTF(args...) 
#endif

#define RWMSG_LOG_EVENT(__ep__, __evt__, ...)  \
  RWLOG_EVENT((__ep__)->rwlog_instance, RwMsgLog_notif_##__evt__, __VA_ARGS__)

#if 0
#define _RWMSG_CH_DEBUG_(ch, p_or_m) \
  fprintf(stderr, "%s%s (%u): %u - %s:%u\n", p_or_m, (ch)->rwtpfx, (ch)->ep->instid, (ch)->refct, __FILE__, __LINE__)
#else
#define _RWMSG_CH_DEBUG_(ch, p_or_m)
#endif
__END_DECLS

#endif // __RWMSG_INT_H
