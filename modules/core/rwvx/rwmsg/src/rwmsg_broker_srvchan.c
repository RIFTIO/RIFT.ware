
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_broker_srvchan.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker server channel
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/syscall.h>

#include "rwmsg_int.h"
#include "rwmsg_broker.h"

/* A word about request references, which are a hassle to get right
   without understanding the governing philosophy.

   Live requests in the broker have two references taken:

   * One reference in brosrvchan, associated with the sc->req_hash.
   * One reference in broclichan, associated with the cc->req_hash's ent->reqref.

   The sc reference is cleared upon receipt of a cancel or ack, NEVER
   at response/bounce time.  This is essential for operation of the
   response cache and future multi-broker network participation.

   The cc reference is cleared upon deleting the ent, normally upon
   the first receipt of a matching msg ID.

   Note that the cc always releases every req it receives on its
   queue, so when sending an actual request back to the cc, we always
   send a "copy" by bumping the req structure's refct as we enqueue.
   Often the cc is fed stub requests, for example cc-declared timeouts
   emit a locally allocated temporary req object that is summarily
   deleted by the release after dequeue and forwarding to the client.

   The queue itself adds and releases a ref to each enqueued req.
   This is invisible under the queue API, but is important to prevent
   headaches around otherwise having no refs on a req in queue.  (Reqs
   with no other refs just "evaporate" invisibly as they come out of
   the queue, and API dequeues again to mask this fact).

 */

static int rwmsg_broker_srvchan_req_return(rwmsg_broker_srvchan_t *sc,
					   rwmsg_request_t *req);
static void rwmsg_broker_srvchan_req_release(rwmsg_broker_srvchan_t *sc,
					     rwmsg_request_t *req);
static void rwmsg_broker_srvchan_ackwheel_tick(void*);
static inline void rwmsg_broker_srvchan_req_dl_remove(rwmsg_broker_srvchan_t *sc,
                                                      rwmsg_request_t *req);

static inline rw_status_t rwmsg_broker_srvchan_process_localq(rwmsg_broker_srvchan_t *sc,
                                                       rwmsg_request_t *req,
                                                       int p,
                                                       bool pre_ready);
static int rwmsg_broker_srvchan_recv_req(rwmsg_broker_srvchan_t *sc,
                                         rwmsg_request_t *req);
static void rwmsg_broker_srvchan_req_reminq(rwmsg_broker_srvchan_t *sc,
                                            rwmsg_request_t *req);


rwmsg_broker_srvchan_t * rwmsg_broker_or_peer_srvchan_create(rwmsg_broker_t *bro,
                                                             struct rwmsg_broker_channel_acceptor_key_s *key,
                                                             enum rwmsg_chantype_e chantype) {
  rwmsg_broker_srvchan_t *sc;

  sc = RW_MALLOC0_TYPE(sizeof(*sc), rwmsg_broker_srvchan_t);
  sc->window = 100;;
  int winval = 0;
  if (RW_STATUS_SUCCESS == rwmsg_endpoint_get_property_int(bro->ep, "/rwmsg/broker/srvchan/window", &winval)) {
    sc->window = winval;
  }

  RW_DL_INIT(&sc->methbindings);

  cbInit(sc, sc->window);

  rwmsg_broker_channel_create(&sc->bch, chantype, bro, key);

  /* Timeo ackwheel timer tick */
  sc->wheel_idx = RWMSG_BROKER_REQWHEEL_SZ-RWMSG_BROKER_REQWHEEL_TICK_DIVISOR;
  gettimeofday(&sc->ackwheel_tv, NULL);


  sc->ackwheel_timer = rwsched_dispatch_source_create(bro->ep->taskletinfo,
                                                      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                      0,
                                                      0,
                                                      sc->bch.rwq);
  rwsched_dispatch_source_set_event_handler_f(bro->ep->taskletinfo,
                                              sc->ackwheel_timer,
                                              rwmsg_broker_srvchan_ackwheel_tick);
  rwsched_dispatch_set_context(bro->ep->taskletinfo,
                               sc->ackwheel_timer,
                               sc);
  rwsched_dispatch_source_set_timer(bro->ep->taskletinfo,
                                    sc->ackwheel_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, RWMSG_BROKER_REQWHEEL_TICK_PERIOD),
                                    RWMSG_BROKER_REQWHEEL_TICK_PERIOD,
                                    (RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD / 2));
  rwsched_dispatch_resume(bro->ep->taskletinfo, sc->ackwheel_timer);

  return sc;
}

rwmsg_broker_srvchan_t *rwmsg_broker_srvchan_create(rwmsg_broker_t *bro,
						    struct rwmsg_broker_channel_acceptor_key_s *key) {
  return rwmsg_broker_or_peer_srvchan_create(bro, key, RWMSG_CHAN_BROSRV);
}

rwmsg_broker_srvchan_t *rwmsg_peer_srvchan_create(rwmsg_broker_t *bro,
						  struct rwmsg_broker_channel_acceptor_key_s *key) {
  rwmsg_broker_srvchan_t *sc =  rwmsg_broker_or_peer_srvchan_create(bro, key, RWMSG_CHAN_PEERSRV);

  return sc;
}

static void rwmsg_broker_srvchan_ackwheel_tick(void*ud) {
  rwmsg_broker_srvchan_t *sc = (rwmsg_broker_srvchan_t*)ud;
  RW_ASSERT_TYPE(sc, rwmsg_broker_srvchan_t);
  unsigned ticks = RWMSG_BROKER_REQWHEEL_TICK_DIVISOR;

  //RW_ASSERT(sc->ch.chantype == RWMSG_CHAN_PEERSRV);

  /* If clocks are reasonable, compute real world tick count */
  struct timeval oldval = sc->ackwheel_tv;
  struct timeval now;
  gettimeofday(&now, NULL);
  struct timeval delta;
  timersub(&now, &oldval, &delta);
  if (delta.tv_sec >= 0 && delta.tv_usec >= 0) {
    uint64_t tickdelta = 0;
    tickdelta = delta.tv_usec / 10000;
    tickdelta += delta.tv_sec * 100;
    // centiseconds, make sure that's still true
    RW_STATIC_ASSERT(RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD == 10000000);
    if (tickdelta < (RWMSG_BROKER_REQWHEEL_SZ / 2)) {
      ticks = tickdelta;
    }
  }

  /* Now set ackwheel_tv to nominal tick time of next tick */
  uint64_t bucket_usec = RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD_USEC * ticks;
  struct timeval nomdelta;
  nomdelta.tv_usec = bucket_usec % 1000000;
  nomdelta.tv_sec = bucket_usec  / 1000000;
  timeradd(&sc->ackwheel_tv, &nomdelta, &sc->ackwheel_tv);
  if (ticks > 2 * RWMSG_BROKER_REQWHEEL_TICK_DIVISOR) {
    RWMSG_TRACE_CHAN(&sc->ch, INFO,
                     "long tick: oldval=%lu.%06lu now=%lu.%06lu act delta=%lu.%06lu bucket_usec=%lu ticks=%u nom delta=%lu.%06lu new ackwheel_tv=%lu.%06lu",
                     oldval.tv_sec, oldval.tv_usec,
                     now.tv_sec, now.tv_usec,
                     delta.tv_sec, delta.tv_usec,
                     bucket_usec,
                     ticks,
                     nomdelta.tv_sec, nomdelta.tv_usec,
                     sc->ackwheel_tv.tv_sec,
                     sc->ackwheel_tv.tv_usec);
  }

  unsigned i;
  for (i=0; i<ticks; i++) {
    int pri;
    for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
      rwmsg_broker_srvchan_ackid_t ackids;
      while (cbPeek(sc, pri, &ackids)) {
        if (ackids.wheel_idx == sc->wheel_idx) {
          rwmsg_broker_clichan_t *ack_cc = ackids.broclichan;
          RW_ASSERT_TYPE(ack_cc, rwmsg_broker_clichan_t);
          rwmsg_request_t *ackreq = rwmsg_request_create((rwmsg_clichan_t*)ack_cc);
          RW_ASSERT(ackreq->broclichan == ack_cc);
          ackreq->hdr = ackids.req_hdr;
          //ackreq->hdr.id.chanid = 0;
          ackreq->hdr.isreq = TRUE;
          ackreq->hdr.nop = TRUE;
          ackreq->hdr.ack = TRUE;
          ackreq->hdr.ackid = ackids.req_hdr.id;
          //ackreq->hdr.seqno = 1;
          ackreq->brosrvchan = sc;
#ifdef PIGGY_ACK_DEBUG
            if (!memcmp(&ackreq->hdr.id, &ackids.req_hdr.id, sizeof(ackids.req_hdr.id)) && ackreq->hdr.bnc) {
              RWMSG_TRACE_CHAN(&sc->bch.ch, DEBUG, "piggy-ack333" FMT_MSG_HDR(ackreq->hdr),
                               PRN_MSG_HDR(ackreq->hdr));
            }
#endif
          rwmsg_request_message_load(&ackreq->req, NULL, 0);

          ack_cc->stat.ack_sent++;

          RWMSG_TRACE_CHAN(&sc->bch.ch, DEBUG, "sending ackreq=%p" FMT_MSG_HDR(ackreq->hdr),
                           ackreq, PRN_MSG_HDR(ackreq->hdr));

          //ck_pr_inc_32(&ackreq->refct);
          RWMSG_REQ_TRACK(ackreq);
          rw_status_t rs = rwmsg_queue_enqueue(&sc->ch.localq, ackreq);
          if (rs != RW_STATUS_SUCCESS) {
            //ck_pr_dec_32(&ackreq->refct);
            RWMSG_TRACE_CHAN(&sc->bch.ch, WARN, "Failed to send  ackreq=%p nop with ackid %u/%u/%u bnc=%u\n",
                           ackreq, ackids.req_hdr.id.broid, ackids.req_hdr.id.chanid, ackreq->hdr.ackid.locid, ackreq->hdr.bnc);
            rwmsg_request_release(ackreq);
          }
    	  rwmsg_broker_clichan_release(ack_cc);
          cbRead(sc, pri, &ackids);
        }
        else {
          break;
        }
        //qent = CK_FIFO_MPMC_FIRST(&sc->ack_fifo[pri]);
      }
    }
    sc->wheel_idx++;
    if (sc->wheel_idx >= RWMSG_BROKER_REQWHEEL_SZ) {
      sc->wheel_idx = 0;
    }
  }
}

static void rwmsg_broker_srvchan_destroy(rwmsg_broker_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_broker_srvchan_t);
  if (!sc->ch.refct) {
    rwmsg_broker_channel_destroy(&sc->bch);
    rwmsg_channel_destroy(&sc->ch);
    memset(sc, 0, sizeof(*sc));
    RW_FREE_TYPE(sc, rwmsg_broker_srvchan_t);
  }
}

void rwmsg_broker_srvchan_release(rwmsg_broker_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_broker_srvchan_t);
  bool zero = 0;
  rwmsg_endpoint_t *ep = sc->ch.ep;
  ck_pr_dec_32_zero(&sc->ch.refct, &zero);
  if (zero) {
    /* generic to avoid linkage from endpoint to broker */
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_GENERIC, sc, (void*)&rwmsg_broker_srvchan_destroy, NULL);
  }
}

void rwmsg_peer_srvchan_release(rwmsg_broker_srvchan_t *sc) {
  rwmsg_broker_srvchan_release(sc);
}

static void rwmsg_broker_srvchan_halt_f(void *ctx) {
  rwmsg_broker_srvchan_t *sc = (rwmsg_broker_srvchan_t *)ctx;

  if (!sc->ackwheel_timer)
    return;

  rwmsg_endpoint_del_channel_method_bindings(sc->ch.ep, &sc->ch);
  rwmsg_channel_halt(&sc->ch);

  // flush requests
  rwmsg_request_t *req = NULL;
  int p;
  int ict=0, clict=0, hashct=0;
  for (p=RWMSG_PRIORITY_COUNT-1; p>=0; p--) {
    // incoming queue
    while ((req = rwmsg_queue_dequeue_pri(&sc->ch.localq, p))) {
      rwmsg_broker_srvchan_req_release(sc, req);
      ict++;
    }

    // request hash
    rwmsg_request_t *nreq = NULL;
    HASH_ITER(hh, sc->req_hash, req, nreq) {
      RW_ASSERT(req->inhash);
      rwmsg_broker_srvchan_req_release(sc, req);
      hashct++;
    }

    // in progress client queues, should now be empty?
    rwmsg_broker_srvchan_cli_t *cli = NULL;
    RW_ASSERT(p >= 0);
    RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
    HASH_ITER(hh, sc->cli_hash, cli, sc->cli_sched[p].cli_next) {
      while ((req = RW_DL_HEAD(&cli->q[p].q, rwmsg_request_t, elem))) {

	rwmsg_broker_srvchan_req_release(sc, req);
	clict++;
      }
    }
  }

  rwmsg_broker_t *bro = sc->bch.bro;
  /* End timer */

  //if (sc->ch.chantype == RWMSG_CHAN_PEERSRV)
  {
    rwsched_dispatch_source_cancel(bro->ep->taskletinfo, sc->ackwheel_timer);
    //NEW-CODE
    rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, sc->ackwheel_timer, bro->ep->rwsched, bro->ep->taskletinfo);
    sc->ackwheel_timer = NULL;
  }

  rwmsg_broker_srvchan_release(sc);

  return;
}

void rwmsg_broker_srvchan_halt_async(rwmsg_broker_srvchan_t *sc) {
  rwmsg_broker_t *bro = sc->bch.bro;
  _RWMSG_CH_DEBUG_(&sc->ch, "++");
  ck_pr_inc_32(&sc->ch.refct);
  rwsched_dispatch_async_f(bro->ep->taskletinfo,
			  rwsched_dispatch_get_main_queue(bro->ep->rwsched),
			  &sc->ch,
			  rwmsg_broker_acceptor_halt_bch_and_remove_from_hash_f);
}

void rwmsg_broker_srvchan_halt(rwmsg_broker_srvchan_t *sc) {
  if (sc->bch.bro->use_mainq) {
    RW_ASSERT(sc->bch.rwq == rwsched_dispatch_get_main_queue(sc->bch.bro->ep->rwsched));
    rwmsg_broker_srvchan_halt_f(sc);
  } else {
    RW_ASSERT(sc->bch.rwq != rwsched_dispatch_get_main_queue(sc->bch.bro->ep->rwsched));
    rwsched_dispatch_sync_f(sc->bch.bro->ep->taskletinfo,
			    sc->bch.rwq,
			    sc,
			    rwmsg_broker_srvchan_halt_f);
  }
}

static void rwmsg_broker_send_methbindings_to_peers_f(void *ud_in) {
  struct rwmsg_srvchan_method_s *ud = (struct rwmsg_srvchan_method_s *)ud_in;
  rwmsg_broker_t *bro = (rwmsg_broker_t*)ud->meth;
  rwmsg_broker_acceptor_t *acc = &bro->acc;
  RW_ASSERT(acc);
  bool zero = 0;
  ck_pr_dec_32_zero(&bro->refct, &zero);
  if (zero) {
    ck_pr_inc_32(&bro->refct);
    rwmsg_broker_destroy(bro);
    return;
  }
  rwmsg_broker_channel_t *bch, *tmp;
  HASH_ITER(hh, acc->bch_hash, bch, tmp) {
    if (bch->ch.chantype == RWMSG_CHAN_PEERCLI) {
      if (!bch->ch.ss) {
        RW_ASSERT(bch->ch.ss);
        continue;
      }

      rw_status_t rs;
      //rs = rwmsg_send_methbinding(&bch->ch, RWMSG_PRIORITY_PLATFORM, (struct rwmsg_methbinding_key_s *)&(ud->k), (const char *)(ud->path));
      rs = rwmsg_send_methbinding(&bch->ch, RWMSG_PRIORITY_LOW, (struct rwmsg_methbinding_key_s *)&(ud->k), (const char *)(ud->path));
      RW_ASSERT(rs==RW_STATUS_SUCCESS);
    }
  }
  RW_FREE_TYPE(ud_in, struct rwmsg_srvchan_method_s);
}

static void rwmsg_broker_srvchan_msgctl(rwmsg_broker_srvchan_t *sc,
					rwmsg_request_t *req) {
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RW_ASSERT(req->hdr.payt == RWMSG_PAYFMT_MSGCTL);

  switch (req->hdr.methno) {
  case RWMSG_MSGCTL_METHNO_METHBIND:
    {
      uint32_t len = 0;
      const struct rwmsg_srvchan_method_s *mb = rwmsg_request_get_request_payload(req, &len);
      //const struct rwmsg_srvchan_method_s *mb = rwmsg_request_get_response_payload(req, &len);
      if (len == sizeof(*mb)) {
	RWMSG_TRACE_CHAN(&sc->bch.ch, INFO, "msgctl add methbinding path='%s' phash=0x%lx methno=%u payt=%d",
			 mb->path,
			 mb->k.pathhash,
			 mb->k.methno,
			 mb->k.payt);

	rwmsg_broker_t *bro = sc->bch.bro;
	rwmsg_channel_t *ch = rwmsg_endpoint_find_channel(sc->ch.ep, RWMSG_METHB_TYPE_BROSRVCHAN, bro->bro_instid, mb->k.pathhash, mb->k.payt, mb->k.methno);
	if (ch) {
	  rwmsg_broker_srvchan_release((rwmsg_broker_srvchan_t*)ch);
	  if (ch == (rwmsg_channel_t*)sc) {
            /* It is us. */
	    goto ret;
	  } else {
            rwmsg_broker_srvchan_t *sc_o = (rwmsg_broker_srvchan_t*)ch;
            if (sc_o->ch.chantype == RWMSG_CHAN_BROSRV) {
              /* Not us?! */
              RWMSG_TRACE_CHAN(&sc->ch, NOTICE, "msgctl add methbinding duplicate found on CHAN_BROSRV, treating as service-recreate; chanid=%u already registered path='%s' phash=0x%lx methno=%u payt=%d",
                               ch->chanid,
                               mb->path,
                               mb->k.pathhash,
                               mb->k.methno,
                               mb->k.payt);
              // delete all the old bindings
              rwmsg_endpoint_del_channel_method_bindings(sc_o->ch.ep, ch);

              int p;
              for (p=RWMSG_PRIORITY_COUNT-2; p>=0; p--) {
                rwmsg_request_t *req_o = NULL;
                while ((req_o = rwmsg_queue_dequeue_pri(&sc_o->ch.localq, p))) {
                  fprintf(stderr, "--------------------------------------sc_o->ch.localq" FMT_MSG_HDR(req_o->hdr), PRN_MSG_HDR(req_o->hdr));
                  RW_ASSERT(!req_o);
                }
                while ((req_o = rwmsg_queue_dequeue_pri(&sc_o->ch.ssbufq, p))) {
                  fprintf(stderr, "--------------------------------------sc_o->ch.ssbufq" FMT_MSG_HDR(req_o->hdr), PRN_MSG_HDR(req_o->hdr));
                  RW_ASSERT(!req_o);
                }
                rwmsg_broker_srvchan_cli_t *cli_o = NULL;
                HASH_ITER(hh, sc_o->cli_hash, cli_o, sc_o->cli_sched[p].cli_next) {
#if 0
                  int sent_count = 0;
                  while ((req_o = RW_DL_HEAD(&cli_o->q[p].q, rwmsg_request_t, elem))) {
                    bool sent = req_o->sent;
                    // take an extra reference
                    ck_pr_inc_32(&req_o->refct);

                    rwmsg_broker_srvchan_req_release(sc_o, req_o);

                    if (!sent) {
                      fprintf(stderr, "++++++++++++++++++++++++++++++++++++++req_o=!sent" FMT_MSG_HDR(req_o->hdr), PRN_MSG_HDR(req_o->hdr));
                      rwmsg_broker_srvchan_process_localq(sc, req_o, p, TRUE);
                      if (sc->cli_sched[p].readyct)
                        rwmsg_sockset_pollout(sc->ch.ss, p, TRUE);
                    } else {
                      fprintf(stderr, "--------------------------------------req_o=sent" FMT_MSG_HDR(req_o->hdr), PRN_MSG_HDR(req_o->hdr));
                      //RW_ASSERT(!req_o);
                      req_o->sent = FALSE;
                      rwmsg_broker_srvchan_process_localq(sc, req_o, p, TRUE);
                      if (sc->cli_sched[p].readyct)
                        rwmsg_sockset_pollout(sc->ch.ss, p, TRUE);
                    }
                    sent_count ++;
                  }
                  rwmsg_broker_srvchan_cli_t *cli = NULL;
                  HASH_FIND(hh, sc->cli_hash, &cli_o->brocli_id, sizeof(cli_o->brocli_id), cli);
                  //RW_ASSERT(cli);
                  if (cli) {
                    RW_ASSERT(cli->brocli_id == cli_o->brocli_id);
                    int qp;
                    for (qp=0; qp<RWMSG_PRIORITY_COUNT; qp++) {
                      if (cli->q[qp].seqno == 1)
                        cli->q[qp].seqno = cli_o->q[qp].seqno;
                        if (qp == p)
                          cli->q[qp].seqno -= sent_count;
                    }
                  }
#else
                  rwmsg_request_t *qreq;
                  while ((qreq = RW_DL_TAIL(&cli_o->q[p].q, rwmsg_request_t, elem))) {
                    //fprintf(stderr, "=======================================qreq.bnc=RWMSG_BOUNCE_SRVRST" FMT_MSG_HDR(qreq->hdr), PRN_MSG_HDR(qreq->hdr));
                    //RW_ASSERT(qreq->hdr.isreq);
                    qreq->hdr.isreq = FALSE;
                    qreq->hdr.bnc = RWMSG_BOUNCE_SRVRST;
                    sc_o->stat.bnc[qreq->hdr.bnc]++;
                    RW_ASSERT(!qreq->hdr.cancel);

                    RW_ASSERT(qreq->broclichan);
                    RW_ASSERT(qreq->refct >= 1);
                    if (!(qreq->hdr.ack && qreq->hdr.nop)) {
                      /* Assert that all items we dequeue and release are in the hash */
                      rwmsg_request_t *dlreq = NULL;
                      HASH_FIND(hh, sc_o->req_hash, &qreq->hdr.id, sizeof(qreq->hdr.id), dlreq);
                      RW_ASSERT(dlreq);
                      RW_ASSERT(dlreq->inhash);
                      RW_ASSERT(dlreq == qreq);
                    }

                    RW_ASSERT_TYPE(qreq->brosrvcli, rwmsg_broker_srvchan_cli_t);
                    RW_ASSERT(qreq->inq);

                    RWMSG_TRACE_CHAN(&sc_o->ch, DEBUG, "qreq RWMSG_BOUNCE_SRVRST" FMT_MSG_HDR(qreq->hdr),
                                     PRN_MSG_HDR(qreq->hdr));

                    /* Remove from cli queue.  Remains in req hash for
                       cancel/retransmit processing etc.  Bounces do get
                       acked so we can free them. */
                    rwmsg_broker_srvchan_req_reminq(sc_o, qreq);
                    RW_ASSERT(qreq != RW_DL_HEAD(&cli_o->q[p].q, rwmsg_request_t, elem));

                    RWMSG_TRACE_CHAN(&sc_o->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_srvchan_recv()" FMT_MSG_HDR(qreq->hdr),
                                     qreq, PRN_MSG_HDR(qreq->hdr));
                    ck_pr_inc_32(&qreq->refct);
                    if (!rwmsg_broker_srvchan_recv_req(sc_o, qreq)) {
                      qreq = NULL;
                    }

                    /* brocli, if it wasn't halted in recv_req/req_return,
                       will send cancel or ack for bounces, at which point
                       we call srvchan_req_release() on our req */
                  }
#endif
                }
              }
            } else {
              RW_ASSERT(sc_o->ch.chantype == RWMSG_CHAN_PEERSRV);
              /* Not us?! */
              RWMSG_TRACE_CHAN(&sc->ch, NOTICE, "msgctl add methbinding duplicate; chanid=%u already registered path='%s' phash=0x%lx methno=%u payt=%d",
                               ch->chanid,
                               mb->path,
                               mb->k.pathhash,
                               mb->k.methno,
                               mb->k.payt);
	      goto ret;
            }
          }
	  // TBD: multiple srvchans per method binding
	}

	RWMSG_TRACE_CHAN(&sc->bch.ch, NOTICE, "Adding methbinding in the broker path='%s' phash=0x%lx methno=%u payt=%d",
			 mb->path,
			 mb->k.pathhash,
			 mb->k.methno,
			 mb->k.payt);


	rwmsg_signature_t *sig = rwmsg_signature_create(sc->ch.ep, mb->k.payt, mb->k.methno, req->hdr.pri);
	rwmsg_method_t *method = rwmsg_method_create(sc->ch.ep, mb->path, sig, NULL);
	rs = rwmsg_endpoint_add_channel_method_binding(sc->ch.ep, &sc->ch, RWMSG_METHB_TYPE_BROSRVCHAN, method);
	RW_ASSERT(rs == RW_STATUS_SUCCESS);

	rwmsg_method_release(sc->ch.ep, method); /* release now, will free when endpoint binding ref lets go */
	rwmsg_signature_release(sc->ch.ep, sig); /* release now, will free when endpoint binding ref lets go of method */

	ck_pr_inc_32(&rwmsg_broker_g.exitnow.method_bindings);

#if 1
	if (sc->ch.chantype != RWMSG_CHAN_BROSRV) {
	  goto ret;
        }

	RWMSG_TRACE_CHAN(&sc->ch, INFO, "Sending the methbinding to peer-brokers path='%s' phash=0x%lx methno=%u payt=%d",
			 mb->path,
			 mb->k.pathhash,
			 mb->k.methno,
			 mb->k.payt);

	{
	  rwmsg_broker_t *bro = sc->bch.bro;
	  RW_ASSERT(bro);
	  rwmsg_endpoint_t *ep = sc->ch.ep;
	  rwmsg_broker_acceptor_t *acc = &bro->acc;
	  rwsched_dispatch_queue_t rwq = NULL;
	  RW_ASSERT(acc->notify.rwsched.rwqueue == rwsched_dispatch_get_main_queue(bro->ep->rwsched));
          RW_ASSERT(ep->rwsched);
	  rwq = rwsched_dispatch_get_main_queue(ep->rwsched);
          RW_ASSERT(rwq);
	  struct rwmsg_srvchan_method_s *ud;
	  ud = RW_MALLOC0_TYPE(sizeof(*ud), struct rwmsg_srvchan_method_s );
	  ud->k = mb->k;
	  ud->meth = (rwmsg_method_t*)bro;
	  ck_pr_inc_32(&bro->refct);
	  memcpy(ud->path, mb->path, sizeof(ud->path));

	  /*
	   * We have to diapatch to acceptor's thread, because we are iterating
	   * on acc.bch_hash which is valid on acceptor's thread.
	   */
	  rwsched_dispatch_async_f(bro->tinfo, rwq, ud, rwmsg_broker_send_methbindings_to_peers_f);
	}
#endif
      }
      else {
	RWMSG_TRACE_CHAN(&sc->bch.ch, WARN, "msgctl len %u incorrect", len);
      }
    }
    break;
  default:
    RWMSG_TRACE_CHAN(&sc->bch.ch, ERROR, "msgctl unknown method %u", req->hdr.methno);
    break;
  }

 ret:
  rwmsg_request_release(req);
}

static inline void rwmsg_broker_srvchan_req_dl_remove(rwmsg_broker_srvchan_t *sc,
                                                      rwmsg_request_t *req) {
  RW_ASSERT(req->brosrvcli);
  rwmsg_broker_srvchan_cli_t *cli = req->brosrvcli;
  RW_ASSERT_TYPE(cli, rwmsg_broker_srvchan_cli_t);

  rwmsg_priority_t pri = req->hdr.pri;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

  if (req->inq) {
    RW_DL_REMOVE(&cli->q[pri].q, req, elem);
    req->inq = FALSE;
    RW_ASSERT(sc->cli_qtot >= 1);
    sc->cli_qtot--;

    if (!req->sent) {
      RW_ASSERT(!req->out);		/* unpossible */
      RW_ASSERT(cli->q[pri].unsent >= 1);
      cli->q[pri].unsent--;
      RW_ASSERT(cli->unsent >= 1);
      cli->unsent--;
      req->sent = TRUE;		/* avoid replay */
    } else if (req->out) {
      RW_ASSERT(req->sent);	/* must have sent it */
      RW_ASSERT(cli->q[pri].outstanding >= 1);
      cli->q[pri].outstanding--;
      RW_ASSERT(cli->outstanding >= 1);
      cli->outstanding--;
      req->out = FALSE;
    }
  }
}


static void rwmsg_broker_srvchan_req_reminq(rwmsg_broker_srvchan_t *sc,
					     rwmsg_request_t *req) {
  RW_ASSERT(req->brosrvcli);
  rwmsg_broker_srvchan_cli_t *cli = req->brosrvcli;
  RW_ASSERT_TYPE(cli, rwmsg_broker_srvchan_cli_t);

  rwmsg_priority_t pri = req->hdr.pri;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

  /* Remove from queue structures and counts if outstanding */
  if (cli->q[pri].last_written == req) {
    cli->q[pri].last_written = RW_DL_PREV(req, rwmsg_request_t, elem);
    if (cli->q[pri].last_written) {
      RW_ASSERT_TYPE(cli->q[pri].last_written, rwmsg_request_t);
    }
  }
  rwmsg_broker_srvchan_req_dl_remove(sc, req);
}

static void rwmsg_broker_srvchan_req_release(rwmsg_broker_srvchan_t *sc,
					     rwmsg_request_t *req) {

  if (req->brosrvcli) {
    rwmsg_broker_srvchan_req_reminq(sc, req);
  }

  RW_ASSERT(!req->inq);

  /* Remove from hash as we will be forgetting this req entirely */
  rwmsg_request_t *hreq = NULL;
  HASH_FIND(hh, sc->req_hash, &req->hdr.id, sizeof(req->hdr.id), hreq);
  if (hreq == req) {
    RW_ASSERT(hreq == req);
    RW_ASSERT(req->inhash);
    HASH_DELETE(hh, sc->req_hash, req);
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "HASH_DELETE req_hash",
                     PRN_MSG_HDR(req->hdr));
    req->inhash = FALSE;
  } else if (!req->hdr.cancel && !req->hdr.nop) {
    RW_ASSERT(!hreq);
  }

  rwmsg_request_release(req);
}

static int rwmsg_broker_srvchan_req_return(rwmsg_broker_srvchan_t *sc,
					   rwmsg_request_t *req) {
  int retval = FALSE;
  rwmsg_priority_t pri = req->hdr.pri;
  int32_t window = 0;

  if (req->brosrvcli) {
    rwmsg_broker_srvchan_cli_t *cli = req->brosrvcli;
    RW_ASSERT_TYPE(cli, rwmsg_broker_srvchan_cli_t);

    /* Remove from queue structures and counts if outstanding */
    if (cli->q[pri].last_written == req) {
      cli->q[pri].last_written = RW_DL_PREV(req, rwmsg_request_t, elem);
      if (cli->q[pri].last_written) {
	RW_ASSERT_TYPE(cli->q[pri].last_written, rwmsg_request_t);
      }
    }

    if (req->inq && !req->sent && sc->ch.chantype != RWMSG_CHAN_PEERSRV) {
	RW_ASSERT(req->hdr.bnc);	/* never sent, must be a bounce */
    }
    rwmsg_broker_srvchan_req_dl_remove(sc, req);

    /* Adjust window to requests outstanding to the srvchan plus the fixed window */
    window = (int32_t)sc->window;
    window -= cli->unsent;
    window += cli->outstanding;
    window = (uint32_t)MAX(1, MIN(RWMSG_WINDOW_MAX, window));
  }

  if (req->broclichan) {
    if (!req->broclichan->ch.halted) {
      /* Send to clichan */
      RW_ASSERT(!req->hdr.cancel);

      /* Send out the current window value */
      req->hdr.stwin = window;

      RW_ASSERT(!req->hdr.isreq);
      RWMSG_REQ_TRACK(req);
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rsp" FMT_MSG_HDR(req->hdr) "=> %s",
                       PRN_MSG_HDR(req->hdr),
                       req->broclichan->ch.rwtpfx);
      rw_status_t rs = rwmsg_queue_enqueue(&req->broclichan->ch.localq, req);
      if (rs != RW_STATUS_SUCCESS) {
	RW_ASSERT(!req->broclichan->ch.localq.qlencap);
	RW_ASSERT(!req->broclichan->ch.localq.qszcap);
	RW_ASSERT(rs != RW_STATUS_BACKPRESSURE); /* not in the broker we don't! */
      } else {
	/* Yes, queued */
	retval = TRUE;
      }
    }
  } else {
    /* Orphaned response from our srv */
    RWMSG_TRACE_CHAN(&sc->ch, INFO,
		     "rsp" FMT_MSG_HDR(req->hdr) "refct=%u has no broclichan\n",
                     PRN_MSG_HDR(req->hdr),
		     req->refct);
    RW_ASSERT(req->refct == 1);
  }

  return retval;
}

/* Ret TRUE if request is sent / consumed.  Ret FALSE otherwise, caller should release req */
static int rwmsg_broker_srvchan_recv_req(rwmsg_broker_srvchan_t *sc,
					 rwmsg_request_t *req) {
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  if (req->hdr.payt == RWMSG_PAYFMT_MSGCTL) {
    rwmsg_broker_srvchan_msgctl(sc, req);
    return TRUE;
  }

  /* Return this req to the broclichan it came from */
  return rwmsg_broker_srvchan_req_return(sc, req);
}

static int rwmsg_broker_srvchan_do_writes_cli(rwmsg_broker_srvchan_t *sc,
					      rwmsg_priority_t pri,
					      rwmsg_broker_srvchan_cli_t *cli,
					      int *sent) {
  RW_ASSERT(sent);

  rw_status_t rs = RW_STATUS_SUCCESS;
  int workperslot = 10;
  rwmsg_request_t *req;
  while (workperslot--
	 && (req = (cli->q[pri].last_written ? RW_DL_NEXT(cli->q[pri].last_written, rwmsg_request_t, elem) : RW_DL_HEAD(&cli->q[pri].q, rwmsg_request_t, elem)))) {
    //RW_ASSERT(req->hdr.isreq); // CORRECT ??
    if (sc->ch.chantype != RWMSG_CHAN_PEERSRV) { // PEERSRV no seqno check
      if (req->hdr.stid && req->hdr.seqno != cli->q[pri].seqno) {
	/* Oops, sequence gap. Clear req and end writing. */
	RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "broker_srvchan_do_writes(pri=%u) req" FMT_MSG_HDR(req->hdr) "seqno=%u != expected sched seqno=%u",
                         pri,
                         PRN_MSG_HDR(req->hdr),
			 req->hdr.seqno, cli->q[pri].seqno);
	req = NULL;
	break;
      }
    }
    // update header in buffer
    if (req->hdr.isreq) {
      RW_ASSERT(req->req.msg.nnbuf_nnmsg);
    }
    rwmsg_request_message_load_header((req->hdr.isreq ? &req->req : &req->rsp), &req->hdr);
    //rs = rwmsg_sockset_send(sc->ch.ss, pri, &req->req.msg);
    RWMSG_REQ_TRACK(req);
    if (req->rwml_buffer) {
      rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                req, 
                                __PRETTY_FUNCTION__, 
                                __LINE__, 
                                "(Req Sockset Send)");
    }

    rs = rwmsg_sockset_send_copy(sc->ch.ss, pri, &req->req.msg);

    if (rs == RW_STATUS_SUCCESS) {
      (*sent)++;
      req->sent = TRUE;
      req->out = TRUE;
      cli->q[pri].last_written = req;
      if (sc->ch.chantype != RWMSG_CHAN_PEERSRV) { // PEERSRV no seqno check
	cli->q[pri].seqno++;
	if (!cli->q[pri].seqno) {
	  cli->q[pri].seqno = 1;
	}
      }

      RW_ASSERT(cli->q[pri].unsent >= 1);
      cli->q[pri].unsent--;
      RW_ASSERT(cli->unsent >= 1);
      cli->unsent--;

      cli->q[pri].outstanding++;
      cli->outstanding++;

      sc->cli_qunsent--;
      RWMSG_TRACE_CHAN(&sc->ch, INFO, "sent req" FMT_MSG_HDR(req->hdr) "into srvchan socket ss=%p (pri=%u)",
                       PRN_MSG_HDR(req->hdr),
                       sc->ch.ss, pri);
      if (sc->ch.chantype == RWMSG_CHAN_PEERSRV && req->hdr.ack && req->hdr.nop) {
        // Done w/ sending standalone-ack
        rwmsg_broker_srvchan_req_release(sc, req);
      }
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      RW_ASSERT(pri >= 0);
      RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);
      sc->cli_sched[pri].cli_next = cli;
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "sockset_send(pri=%u)=%u, pollout, cli->q[pri].unsent=%u, cli->q[pri].outstanding=%u, sent=%u",
		       pri, rs,
		       cli->q[pri].unsent,
		       cli->q[pri].outstanding,
		       *sent);
      goto out;
    } else {
      RWMSG_TRACE_CHAN(&sc->ch, ERROR, "sockset_send(pri=%u)=%u, error - req=%p" FMT_MSG_HDR(req->hdr),
                       pri, rs, req, PRN_MSG_HDR(req->hdr));
      goto out;
    }
  }
  if (!req) {
    cli->q[pri].ready = FALSE;
    RW_ASSERT(pri >= 0);
    RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);
    RW_ASSERT(sc->cli_sched[pri].readyct >= 1);
    sc->cli_sched[pri].readyct--;
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "broker_srvchan_do_writes(pri=%u) end of cli pqueue, cli=%p new .ready=%u, cli_sched[].readyct=%u",
		     pri, cli, cli->q[pri].ready, sc->cli_sched[pri].readyct);
  }

 out:
  return rs;
}

static inline rw_status_t rwmsg_broker_srvchan_process_localq(
    rwmsg_broker_srvchan_t *sc,
    rwmsg_request_t *req,
    int p,
    bool pre_ready) {
  rw_status_t rs = RW_STATUS_SUCCESS;
  /* Req arrives with a ref already added for us by the
     broclichan, so we have to release all reqs exactly once here
     in brosrvchan */

  RW_ASSERT(p != RWMSG_PRIORITY_BLOCKING); /* only exists on client side, so this is unpossible */
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req=%p" FMT_MSG_HDR(req->hdr) "arriving from queue",
                   req, PRN_MSG_HDR(req->hdr));

  /* Process ACK */
  /* TBD: multple ACKs in extension header */
  if (req->hdr.ack) {
    rwmsg_request_t *ackreq = NULL;
    //rwmsg_id_t ackid = req->hdr.id;
    //ackid.locid = req->hdr.ackid;
    rwmsg_id_t ackid = req->hdr.ackid;
    HASH_FIND(hh, sc->req_hash, &ackid, sizeof(ackid), ackreq);
    if (ackreq) {
      RW_ASSERT(ackreq->inhash);

      /* Deref locally */
      rwmsg_broker_srvchan_req_release(sc, ackreq);
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "srvchan_req_release called",
                       PRN_MSG_HDR(req->hdr));
      if (sc->ch.chantype == RWMSG_CHAN_BROSRV) {
        req->hdr.ack = FALSE; /* this brosrv consumed the ACK, the requset might traverse to srvchan */
      }
    }
  }
  if (sc->ch.chantype != RWMSG_CHAN_BROSRV || !req->hdr.nop) {
    // seek rsp in rspcache, else
    rwmsg_request_t *oreq = NULL;
    HASH_FIND(hh, sc->req_hash, &req->hdr.id, sizeof(req->hdr.id), oreq);
    if (oreq) {
      RW_ASSERT(oreq->inhash);
      RW_ASSERT_TYPE(oreq->brosrvcli, rwmsg_broker_srvchan_cli_t); // CORRECT ?? 
      bool reqgoaway = FALSE;

      // replace oreq with this new one, for updated routing, etc
      // we actually keep oreq, since it's in multiple hashes and lists
      // we immediately release req, since it was a retransmit or cancel or somesuch

      RW_ASSERT(req->hdr.pathhash == oreq->hdr.pathhash);
      RW_ASSERT(req->hdr.payt == oreq->hdr.payt);
      RW_ASSERT(req->hdr.methno == oreq->hdr.methno);

      // update the broclichan ptr; will be either a broclichan or bropeerchan in the future
      RW_ASSERT(req->broclichan);
      RW_ASSERT(oreq->broclichan);
      RW_ASSERT(req->brosrvchan == sc);
      if (req->broclichan != oreq->broclichan) {
        rwmsg_broker_clichan_release(oreq->broclichan);
        oreq->broclichan = req->broclichan;
        req->broclichan = NULL;
      }

      if (req->hdr.cancel) {
        /* A cancel; toss */
        reqgoaway = TRUE;
        sc->stat.cancel++;
        RW_ASSERT(req->hdr.bnc);
        rwmsg_broker_srvchan_req_release(sc, oreq); /* our ref */
        oreq = NULL;
        RWMSG_TRACE_CHAN(&sc->ch, WARN, "req cancel" FMT_MSG_HDR(req->hdr),
                         PRN_MSG_HDR(req->hdr));
      } else {
        if (!oreq->hdr.isreq) {
          RW_CRASH();
          /* A cached response, resend, bump ref before doing so, since the brocli will decr */
          RWMSG_TRACE_CHAN(&sc->ch, INFO, "rsp resent" FMT_MSG_HDR(req->hdr),
                           PRN_MSG_HDR(req->hdr));
          RW_ASSERT(oreq->broclichan);
          RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_srvchan_recv()" FMT_MSG_HDR(oreq->hdr),
                           oreq, PRN_MSG_HDR(oreq->hdr));
          ck_pr_inc_32(&oreq->refct); /* this is a second or further copy enqueued, each copy gets released on dequeue in the brocli */
          rwmsg_broker_srvchan_req_return(sc, oreq);
          sc->stat.cached_resend++;
        } else {
          /* A pending request, do nothing */
          RWMSG_TRACE_CHAN(&sc->ch, INFO, "req origin updated, still pending" FMT_MSG_HDR(req->hdr),
                           PRN_MSG_HDR(req->hdr));
          sc->stat.pending_donada++;
        }
      }

      rwmsg_bool_t b = rwmsg_request_release(req);
      if (reqgoaway) {
        RW_ASSERT(b);
      }
      req = NULL;

    } else {
      if (req->hdr.cancel) {
        sc->stat.cancel_unk++;
        goto nop;
      }

      /* If this is an ack traversing thru the bro-srvchan, don't add to the req_hash */
      if (!(req->hdr.ack && req->hdr.nop)) {
        /* We own a ref to this req, this was set up before enqueue in the broclichan */
        HASH_ADD(hh, sc->req_hash, hdr.id, sizeof(req->hdr.id), req);

        RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "HASH_ADD req_hash",
                         PRN_MSG_HDR(req->hdr));
        req->inhash = TRUE;
      }
      else {
        /* If this is an ack traversing thru the bro-srvchan, set the stid to 0 */
        //req->hdr.stid = 0; /* RIFT_6913 */
      }

      /* Find or make the client */
      rwmsg_broker_srvchan_cli_t *cli = NULL;
      uint32_t brocli_id;
      if (req->hdr.stid) {
        brocli_id = RWMSG_ID_TO_BROCLI_ID(&req->hdr.id);
      } else {
        brocli_id = 0;
        /* Note: To ensure ordering, we need to guarantee no
           service to the cli for the actual brocli until this
           brocli has no entries in the stid=0/brocli=0 queue.
           TBD! */
      }
      HASH_FIND(hh, sc->cli_hash, &brocli_id, sizeof(brocli_id), cli);
      if (!cli) {
        /* If stream and no client, make and use a new client. */
        cli = RW_MALLOC0_TYPE(sizeof(*cli), rwmsg_broker_srvchan_cli_t);
        cli->brocli_id = brocli_id;
        int qp;
        for (qp=0; qp<RWMSG_PRIORITY_COUNT; qp++) {
          cli->q[qp].seqno = 1;
        }
        HASH_ADD(hh, sc->cli_hash, brocli_id, sizeof(cli->brocli_id), cli);
        RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "New Client brocli_id=%u added", brocli_id);
      }
      RW_ASSERT(cli);
      req->brosrvcli = cli;

      if (req->hdr.stid) {
        // TBD if outside currently expected window, drop!

        if (sc->ch.chantype == RWMSG_CHAN_PEERSRV) { // PEERSRV no seqno check
          //RW_ASSERT(req->hdr.isreq);
          RW_DL_INSERT_TAIL(&cli->q[p].q, req, elem);
        } else {
          /* RIFT-7414 - plain ack requests are triggering seqno resets on the sever side
           * nops should not trigger a sequence reset
           */
          if (!req->hdr.seqno && !req->hdr.nop) {
            /* Seqno zero is a sequence reset; bounce all extant
               requests in the stream and reset. */
            RWMSG_TRACE_CHAN(&sc->ch, CRIT, "req" FMT_MSG_HDR(req->hdr) "triggered a sequence reset in cli=%p",
                             PRN_MSG_HDR(req->hdr), cli);
            sc->stat.seqzero_recvd++;

            rwmsg_request_t *qreq;
            while ((qreq = RW_DL_HEAD(&cli->q[p].q, rwmsg_request_t, elem))) {
              //RW_ASSERT(qreq->hdr.isreq);
              qreq->hdr.isreq = FALSE;
              qreq->hdr.bnc = RWMSG_BOUNCE_RESET;
              sc->stat.bnc[qreq->hdr.bnc]++;
              RW_ASSERT(!qreq->hdr.cancel);

              RW_ASSERT(qreq->broclichan);
              RW_ASSERT(qreq->refct >= 1);
              if (!(qreq->hdr.ack && qreq->hdr.nop)) {
                /* Assert that all items we dequeue and release are in the hash */
                rwmsg_request_t *dlreq = NULL;
                HASH_FIND(hh, sc->req_hash, &qreq->hdr.id, sizeof(qreq->hdr.id), dlreq);
                RW_ASSERT(dlreq);
                RW_ASSERT(dlreq->inhash);
                RW_ASSERT(dlreq == qreq);
              }

              RW_ASSERT_TYPE(qreq->brosrvcli, rwmsg_broker_srvchan_cli_t);
              RW_ASSERT(qreq->inq);

              RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "qreq RWMSG_BOUNCE_RESET" FMT_MSG_HDR(qreq->hdr),
                               PRN_MSG_HDR(qreq->hdr));

              /* Remove from cli queue.  Remains in req hash for
                 cancel/retransmit processing etc.  Bounces do get
                 acked so we can free them. */
              rwmsg_broker_srvchan_req_reminq(sc, qreq);
              RW_ASSERT(qreq != RW_DL_HEAD(&cli->q[p].q, rwmsg_request_t, elem));

              RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_srvchan_recv()" FMT_MSG_HDR(qreq->hdr),
                               qreq, PRN_MSG_HDR(qreq->hdr));
              ck_pr_inc_32(&qreq->refct);
              if (!rwmsg_broker_srvchan_recv_req(sc, qreq)) {
                qreq = NULL;
              }

              /* brocli, if it wasn't halted in recv_req/req_return,
                 will send cancel or ack for bounces, at which point
                 we call srvchan_req_release() on our req */
            }

            /* This is the new and now only req in the queue */
            //RW_ASSERT(req->hdr.isreq);
            RW_DL_INSERT_HEAD(&cli->q[p].q, req, elem);
            cli->q[p].seqno = 0;

          } else {
            /* queue / order by client */
#define SEQ_LT(a,b) ((int32_t)((uint32_t)(a)-((uint32_t)(b)) < 0))
            RW_ASSERT(sc->ch.chantype == RWMSG_CHAN_BROSRV);
            rwmsg_request_t *qreq = RW_DL_TAIL(&cli->q[p].q, rwmsg_request_t, elem);
            while (qreq && SEQ_LT(req->hdr.seqno, qreq->hdr.seqno)) {
              //RW_ASSERT(qreq->hdr.isreq);
              qreq = RW_DL_PREV(qreq, rwmsg_request_t, elem);
            }
            //RW_ASSERT(req->hdr.isreq);
            if (!qreq) {
              RW_DL_INSERT_HEAD(&cli->q[p].q, req, elem);
            } else {
              qreq = RW_DL_NEXT(qreq, rwmsg_request_t, elem);
              if (qreq) {
                RW_DL_ELEMENT_INSERT_BEFORE(&cli->q[p].q, &req->elem, &qreq->elem);
              } else {
                RW_DL_INSERT_TAIL(&cli->q[p].q, req, elem);
              }
            }
          }
        }

        if (!cli->q[p].ready) {
          if (sc->ch.chantype != RWMSG_CHAN_PEERSRV) { // PEERSRV no seqno check
            rwmsg_request_t *hreq = NULL;
            if (cli->q[p].last_written) {
              hreq = RW_DL_NEXT(cli->q[p].last_written, rwmsg_request_t, elem);
            } else {
              hreq = RW_DL_HEAD(&cli->q[p].q, rwmsg_request_t, elem);
            }
            if ((hreq->hdr.seqno == cli->q[p].seqno)
                || pre_ready) {
              RW_ASSERT(p >= 0);
              RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
              cli->q[p].ready = TRUE;
              if (!sc->cli_sched[p].cli_next || !sc->cli_sched[p].readyct) {
                sc->cli_sched[p].cli_next = cli;
              }
              sc->cli_sched[p].readyct++;
            }
          } else {
            RW_ASSERT(p >= 0);
            RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
            cli->q[p].ready = TRUE;
            if (!sc->cli_sched[p].cli_next || !sc->cli_sched[p].readyct) {
              sc->cli_sched[p].cli_next = cli;
            }
            sc->cli_sched[p].readyct++;
          }
        }

        RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "scheduled in cli=%p q[pri=%u] .seqno=%u, .ready=%u, .cli_next=%p sched[].readyct=%u",
                         PRN_MSG_HDR(req->hdr),
                         cli,
                         p,
                         cli->q[p].seqno,
                         cli->q[p].ready,
                         sc->cli_sched[p].cli_next,
                         sc->cli_sched[p].readyct);
      } else {
        /* Just append to queue on the anon/new client */
        RW_ASSERT(!cli->brocli_id);
        RW_ASSERT(p >= 0);
        RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
        //RW_ASSERT(req->hdr.isreq);
        RW_DL_INSERT_TAIL(&cli->q[p].q, req, elem);
        if (!cli->q[p].ready) {
          cli->q[p].ready = TRUE;
          if (!sc->cli_sched[p].readyct) {
            sc->cli_sched[p].cli_next = cli;
          }
          sc->cli_sched[p].readyct++;
        }

        RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "scheduled in cli=%p (anon/new)",
                         PRN_MSG_HDR(req->hdr),
                         cli);
      }

      req->inq = TRUE;
      sc->cli_qtot++;
      sc->cli_qunsent++;
      cli->unsent++;
      RW_ASSERT(p >= 0);
      RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
      cli->q[p].unsent++;
      req->sent = FALSE;
    }
  } else {
  nop:
    /* nop, release the req */
    if (req->refct > 1) {
      RW_ASSERT(req->refct > 1);
      ck_pr_dec_32(&req->refct);
    }
    rwmsg_request_release(req);
  }
  return rs;
}

/* Execute writes in a vaguely fair round robin sort of way */
static rw_status_t rwmsg_broker_srvchan_do_writes(rwmsg_broker_srvchan_t *sc,
						  rwmsg_priority_t pri) {
  rw_status_t rs = RW_STATUS_SUCCESS;

  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);
  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "broker_srvchan_do_writes pri=%u cli_sched[].readyct=%u", pri, sc->cli_sched[pri].readyct);

  RW_ASSERT(sc->ch.ss->sktype == RWMSG_SOCKSET_SKTYPE_NN);
  int oldpollout = sc->ch.ss->nn.sk[pri].pollout;
  int sent = 0;

  // TBD: weight clis by q depth, or maybe don't even bother

  /* Run cli0 to completion, avoids ordering bug for default window
     reqs which arrived with stid 0.  Need a more subtle method, this
     slightly borks the fairness of the RR scheme.*/
  rwmsg_broker_srvchan_cli_t *cli0 = NULL;
  uint32_t zero = 0;
  HASH_FIND(hh, sc->cli_hash, &zero, sizeof(zero), cli0);
  if (cli0) {
    while (cli0->q[pri].ready) {
      rs = rwmsg_broker_srvchan_do_writes_cli(sc, pri, cli0, &sent);
      if (rs != RW_STATUS_SUCCESS) {
	goto out2;
      }
    }
  }

  rwmsg_broker_srvchan_cli_t *cli = NULL;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);
  HASH_ITER(hh, sc->cli_hash, cli, sc->cli_sched[pri].cli_next) {
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "broker_srvchan_do_writes(pri=%u) considering cli=%p .ready=%u", pri, cli, cli->q[pri].ready);
    if (cli->q[pri].ready) {
      rs = rwmsg_broker_srvchan_do_writes_cli(sc, pri, cli, &sent);
      if (rs != RW_STATUS_SUCCESS) {
	goto out;
      }
    }
    if (!sc->cli_sched[pri].readyct) {
      goto out;
    }
  }
 out2:
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);
  if (sc->cli_sched[pri].readyct) {
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_broker_srvchan_do_writes residual cli_sched[pri=%d].readyct=%d => pollout",
		     pri, sc->cli_sched[pri].readyct);
    rs = RW_STATUS_BACKPRESSURE;
  }

 out:
  if (oldpollout != (rs == RW_STATUS_BACKPRESSURE)) {
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_broker_srvchan sockset_send(pri=%u)=%u, sent=%d, pollout %d->%d",
		     pri, rs,
		     sent,
		     oldpollout,
		     (rs == RW_STATUS_BACKPRESSURE));
  }
  rwmsg_sockset_pollout(sc->ch.ss, pri, (rs == RW_STATUS_BACKPRESSURE));
  return rs;
}

/* Local event on the queue, a request queued from some broclichan or
   bropeerchan */
uint32_t rwmsg_broker_srvchan_recv(rwmsg_broker_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_broker_srvchan_t);
  uint32_t rval = 0;

  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_broker_srvchan_recv() called sc=%p notify=%p .clearct=%u", sc, sc->ch.localq.notify, sc->ch.localq.notify->rwsched.clearct);

  if (1) {
    /* Scheduler sanity checks, just for the heck of it */
    /* This will be valid under static thread pool */
    if (0) {
#define GETTID() syscall(SYS_gettid)
      uint64_t tid;
      static uint64_t otid = 0;
      tid = GETTID();
      if (otid && otid != tid) {
  RWMSG_TRACE_CHAN(&sc->ch, INFO, "broker_srvchan %p tid change %lx -> %lx", sc, otid, tid);
      }
      otid = tid;
    }

    /* This will be valid under all cases */
    if (1) {
      dispatch_queue_t q = dispatch_get_current_queue();
      RW_ASSERT((void*)q == *(void**)sc->bch.rwq);
    }
  }

  rwmsg_request_t *req = NULL;
  int p;
  for (p=RWMSG_PRIORITY_COUNT-1; p>=0; p--) {
    int deqmax = 100;

    while (deqmax--
	   && (req = rwmsg_queue_dequeue_pri(&sc->ch.localq, p))) {
      rwmsg_broker_srvchan_process_localq(sc, req, p, FALSE);
      rval++;
    }


    /* Write out the scheduled requests for this pri */
    RW_ASSERT(p >= 0);
    RW_ASSERT(p < RWMSG_PRIORITY_COUNT);
    if (sc->cli_sched[p].readyct) {

      // PERF FIXME, SET TO 0
#if 1
      /* Test flow control callback path, by always using pollout for every write */
      rwmsg_sockset_pollout(sc->ch.ss, p, TRUE);
#else
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_broker_srvchan_recv() pri=%d calling _do_writes()", p);
      rw_status_t rs = rwmsg_broker_srvchan_do_writes(sc, p);
      switch (rs) {
      case RW_STATUS_SUCCESS:
      case RW_STATUS_BACKPRESSURE:
	break;
      default:
	RWMSG_TRACE_CHAN(&sc->ch, ERROR, "rwmsg_broker_srvchan_do_writes(pri=%d) rs=%d", p, rs);
      }
#endif
    }

    if (deqmax == 99) {
      RW_ASSERT(sc->ch.localq.notify == &sc->ch.notify);
      //RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "no req in localq pri %d localq.detickle=%u .qlen=%u", p, sc->ch.localq.detickle, sc->ch.localq.qlen);
    }

  }

  return rval;
}

uint32_t rwmsg_peer_srvchan_recv(rwmsg_broker_srvchan_t *sc) {
  return rwmsg_broker_srvchan_recv(sc);
}

/* Message read from the sockset */
rw_status_t rwmsg_broker_srvchan_recv_buf(rwmsg_broker_srvchan_t *sc,
					  rwmsg_buf_t *buf) {
  rw_status_t rs = RW_STATUS_FAILURE;

  /* Find request */
  rwmsg_header_t hdr;
  rs = rwmsg_buf_unload_header(buf, &hdr);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  rwmsg_request_t *req = NULL;
  int hashreq = TRUE;
  rwmsg_broker_t *bro = sc->bch.bro;
  HASH_FIND(hh, sc->req_hash, &hdr.id, sizeof(hdr.id), req);
  if (!req) {
    hashreq = FALSE;
    req = rwmsg_request_create(NULL);
    req->brosrvchan = sc;
    _RWMSG_CH_DEBUG_(&sc->ch, "++");
    ck_pr_inc_32(&sc->ch.refct);

    if(bro->rwmemlog) {
      ck_pr_inc_int(&bro->rwmemlog_id);
      req->rwml_buffer = rwmemlog_instance_get_buffer(bro->rwmemlog, "Req", -bro->rwmemlog_id);
      rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                req, 
                                __PRETTY_FUNCTION__, 
                                __LINE__, 
                                "(Req Created)");
    }

  } else {
    RW_ASSERT(req->inhash);
    RW_ASSERT_TYPE(req->brosrvcli, rwmsg_broker_srvchan_cli_t); // CORRECT ??
    if (req->rwml_buffer) {
      rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                req, 
                                __PRETTY_FUNCTION__, 
                                __LINE__, 
                                "(Req in Hash)");
    }
  }
  req->hdr = hdr;
  if (hdr.isreq) {
    // free req.msg?
    req->req.msg = *buf;
  } else {
    // free rsp.msg?
    req->rsp.msg = *buf;
  }
  if (hashreq) {
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_srvchan_recv_buf()" FMT_MSG_HDR(req->hdr),
                     req, PRN_MSG_HDR(req->hdr));
    ck_pr_inc_32(&req->refct);
  }

  if (sc->ch.chantype == RWMSG_CHAN_BROSRV) {
    /* Remove from hash as we will be forgetting this req entirely */
    if (hashreq) {
      RW_ASSERT(req->inhash);
      HASH_DELETE(hh, sc->req_hash, req);
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "HASH_DELETE req_hash",
                       PRN_MSG_HDR(req->hdr));
      req->inhash = FALSE;
      ck_pr_dec_32(&req->refct);
    }
  }
  if (!rwmsg_broker_srvchan_recv_req(sc, req)) {
    /* Some sort of queueing error, or halted brocli */
    /*
    if (hashreq) {
      ck_pr_dec_32(&req->refct);
    }
    */
    rwmsg_broker_srvchan_req_release(sc, req);
  }
  req = NULL;

  return RW_STATUS_SUCCESS;
}

rw_status_t rwmsg_peer_srvchan_recv_buf(rwmsg_broker_srvchan_t *sc,
					  rwmsg_buf_t *buf) {
  return rwmsg_broker_srvchan_recv_buf(sc, buf);
}

/* From sockset, flow control feedme callback */
void rwmsg_broker_srvchan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_broker_srvchan_t *sc = (rwmsg_broker_srvchan_t*)ud;
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_broker_srvchan_t);

  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_broker_srvchan_sockset_event_send(pri=%d)", pri);
  rw_status_t rs = rwmsg_broker_srvchan_do_writes(sc, pri);

  switch (rs) {
  case RW_STATUS_SUCCESS:
  case RW_STATUS_BACKPRESSURE:
    break;
  default:
    RWMSG_TRACE_CHAN(&sc->ch, ERROR, "rwmsg_broker_srvchan_do_writes(pri=%d) rs=%d", pri, rs);
    break;
  }

  return;
}

void rwmsg_peer_srvchan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_broker_srvchan_sockset_event_send(ss, pri, ud);
  rwmsg_broker_srvchan_sockset_event_send(ss, pri, ud);
}

