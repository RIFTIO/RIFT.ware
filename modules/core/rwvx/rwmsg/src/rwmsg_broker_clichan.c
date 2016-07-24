
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_broker_clichan.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker client channel
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "rwmsg_int.h"
#include "rwmsg_broker.h"

static void rwmsg_broker_clichan_reqwheel_tick(void*);

rwmsg_broker_clichan_t *rwmsg_broker_clichan_create(rwmsg_broker_t *bro,
						    struct rwmsg_broker_channel_acceptor_key_s *key) {
  rwmsg_broker_clichan_t *cc;

  cc = RW_MALLOC0_TYPE(sizeof(*cc), rwmsg_broker_clichan_t);
  rwmsg_broker_channel_create(&cc->bch, RWMSG_CHAN_BROCLI, bro, key);

  /* Timeo reqwheel timer tick */
  cc->wheel_idx = RWMSG_BROKER_REQWHEEL_SZ-RWMSG_BROKER_REQWHEEL_TICK_DIVISOR;
  gettimeofday(&cc->reqwheel_tv, NULL);

  cc->reqwheel_timer = rwsched_dispatch_source_create(bro->ep->taskletinfo,
						      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
						      0,
						      0,
						      cc->bch.rwq);
  rwsched_dispatch_source_set_event_handler_f(bro->ep->taskletinfo,
					      cc->reqwheel_timer,
					      rwmsg_broker_clichan_reqwheel_tick);
  rwsched_dispatch_set_context(bro->ep->taskletinfo,
			       cc->reqwheel_timer,
			       cc);
  rwsched_dispatch_source_set_timer(bro->ep->taskletinfo,
				    cc->reqwheel_timer,
				    dispatch_time(DISPATCH_TIME_NOW, RWMSG_BROKER_REQWHEEL_TICK_PERIOD),
				    RWMSG_BROKER_REQWHEEL_TICK_PERIOD,
				    (RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD / 2));
  rwsched_dispatch_resume(bro->ep->taskletinfo, cc->reqwheel_timer);

  return cc;
}

rwmsg_broker_clichan_t *rwmsg_peer_clichan_create(rwmsg_broker_t *bro,
						  struct rwmsg_broker_channel_acceptor_key_s *key) {
  rwmsg_broker_clichan_t *cc;

  cc = RW_MALLOC0_TYPE(sizeof(*cc), rwmsg_broker_clichan_t);
  cc->peer_chan = TRUE;
  rwmsg_broker_channel_create(&cc->bch, RWMSG_CHAN_PEERCLI, bro, key);

  /* Timeo reqwheel timer tick */
  cc->wheel_idx = RWMSG_BROKER_REQWHEEL_SZ-RWMSG_BROKER_REQWHEEL_TICK_DIVISOR;
  gettimeofday(&cc->reqwheel_tv, NULL);

  cc->reqwheel_timer = rwsched_dispatch_source_create(bro->ep->taskletinfo,
						      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
						      0,
						      0,
						      cc->bch.rwq);
  rwsched_dispatch_source_set_event_handler_f(bro->ep->taskletinfo,
					      cc->reqwheel_timer,
					      rwmsg_broker_clichan_reqwheel_tick);
  rwsched_dispatch_set_context(bro->ep->taskletinfo,
			       cc->reqwheel_timer,
			       cc);
  rwsched_dispatch_source_set_timer(bro->ep->taskletinfo,
				    cc->reqwheel_timer,
				    dispatch_time(DISPATCH_TIME_NOW, RWMSG_BROKER_REQWHEEL_TICK_PERIOD),
				    RWMSG_BROKER_REQWHEEL_TICK_PERIOD,
				    (RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD / 2));
  rwsched_dispatch_resume(bro->ep->taskletinfo, cc->reqwheel_timer);

  return cc;
}

static void rwmsg_broker_clichan_reqwheel_tick(void*ud) {
  rwmsg_broker_clichan_t *cc = (rwmsg_broker_clichan_t *)ud;
  unsigned ticks = RWMSG_BROKER_REQWHEEL_TICK_DIVISOR;
  /* If clocks are reasonable, compute real world tick count */
  struct timeval oldval = cc->reqwheel_tv;
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

  /* Now set reqwheel_tv to nominal tick time of next tick */
  uint64_t bucket_usec = RWMSG_BROKER_REQWHEEL_BUCKET_PERIOD_USEC * ticks;
  struct timeval nomdelta;
  nomdelta.tv_usec = bucket_usec % 1000000;
  nomdelta.tv_sec = bucket_usec  / 1000000;
  timeradd(&cc->reqwheel_tv, &nomdelta, &cc->reqwheel_tv);
  if (ticks > 2 * RWMSG_BROKER_REQWHEEL_TICK_DIVISOR) {
    RWMSG_TRACE_CHAN(&cc->ch, INFO,
		     "long tick: oldval=%lu.%06lu now=%lu.%06lu act delta=%lu.%06lu bucket_usec=%lu ticks=%u nom delta=%lu.%06lu new reqwheel_tv=%lu.%06lu",
		     oldval.tv_sec, oldval.tv_usec,
		     now.tv_sec, now.tv_usec,
		     delta.tv_sec, delta.tv_usec,
		     bucket_usec,
		     ticks,
		     nomdelta.tv_sec, nomdelta.tv_usec,
		     cc->reqwheel_tv.tv_sec,
		     cc->reqwheel_tv.tv_usec);
  }


  unsigned i;
  for (i=0; i<ticks; i++) {
    rwmsg_broker_clichan_reqent_t *ent;
    while ((ent = RW_DL_POP(&cc->reqwheel[cc->wheel_idx], rwmsg_broker_clichan_reqent_t, elem))) {
      /* Timeout.  Synthesize a bounce.  */
      RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_POP reqwheel" FMT_MSG_HDR(ent->hdr),
                       PRN_MSG_HDR(ent->hdr));
      ent->wheelidx = -1;	/* no longer in wheel! still in reqent_hash though */
      RW_ASSERT_TYPE(ent->reqref, rwmsg_request_t);

      rwmsg_request_t *req = rwmsg_request_create((rwmsg_clichan_t*)cc);
      RW_ASSERT(req->broclichan == cc);
      req->hdr = ent->hdr;
      req->hdr.isreq = FALSE;
      req->hdr.bnc = RWMSG_BOUNCE_TIMEOUT;

#ifndef __RWMSG_BNC_RESPONSE_W_PAYLOAD
      rwmsg_request_message_load(&req->rsp, NULL, 0);
#else
      //uint32_t len = 0;
      //const void *req_payload = rwmsg_request_get_request_payload(ent->reqref, &len);
      //rwmsg_request_message_load(&req->req, req_payload, len);
      rwmsg_request_message_load(&req->rsp, ent->reqref->req.msg.nnbuf, ent->reqref->req.msg.nnbuf_len);
#endif

      rwmsg_broker_t *bro = cc->bch.bro;

      RWMSG_TRACE_CHAN(&cc->ch, WARN, "req timeout wheelidx=%u ticks=%u delta=%lu.%06lu" FMT_MSG_HDR(req->hdr),
                       cc->wheel_idx,
                       ticks,
                       delta.tv_sec,
                       delta.tv_usec,
                       PRN_MSG_HDR(req->hdr));

      /* Queue for self.  Will release req upon dequeue */
      RW_ASSERT(!req->hdr.isreq);
      RWMSG_REQ_TRACK(req);
      rw_status_t rs = rwmsg_queue_enqueue(&cc->ch.localq, req);
      if (rs != RW_STATUS_SUCCESS) {
	RW_CRASH();
      }

      /* Also send a cancel request in the general direction the request went */
      rwmsg_broker_srvchan_t *bsc = (rwmsg_broker_srvchan_t *)rwmsg_endpoint_find_channel(cc->ch.ep,
											  RWMSG_METHB_TYPE_BROSRVCHAN,
											  bro->bro_instid,
											  req->hdr.pathhash,
											  req->hdr.payt,
											  req->hdr.methno);
      if (bsc) {
	req = rwmsg_request_create((rwmsg_clichan_t*)cc);
	RW_ASSERT(req->broclichan == cc);
	req->hdr = ent->hdr;
	req->hdr.isreq = TRUE;
	req->hdr.bnc = RWMSG_BOUNCE_TIMEOUT;
	req->hdr.cancel = TRUE;
	req->hdr.nop = FALSE;
	req->brosrvchan = bsc;
        rwmsg_request_message_load(&req->rsp, NULL, 0);
	rs = rwmsg_queue_enqueue(&bsc->ch.localq, req);
	if (rs != RW_STATUS_SUCCESS) {
	  cc->stat.to_canreleased++;
	  rwmsg_request_release(req);
	} else {
	  cc->stat.to_cansent++;
	}
	/* brosrvchan will release the cancel req and the original req*/
      }
    }

    cc->wheel_idx++;
    if (cc->wheel_idx >= RWMSG_BROKER_REQWHEEL_SZ) {
      cc->wheel_idx = 0;
    }
  }
}

static void rwmsg_broker_clichan_destroy(rwmsg_broker_clichan_t *cc) {
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_broker_clichan_t);
  if (!cc->ch.refct) {
    rwmsg_broker_channel_destroy(&cc->bch);
    rwmsg_channel_destroy(&cc->ch);
    memset(cc, 0, sizeof(*cc));
    RW_FREE_TYPE(cc, rwmsg_broker_clichan_t);
  }
}

void rwmsg_broker_clichan_release(rwmsg_broker_clichan_t *cc) {
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_broker_clichan_t);
  bool zero = 0;
  rwmsg_endpoint_t *ep = cc->ch.ep;
  ck_pr_dec_32_zero(&cc->ch.refct, &zero);
  if (zero) {
    /* generic to avoid linkage from endpoint to broker */
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_GENERIC, cc, (void*)&rwmsg_broker_clichan_destroy, NULL);
  }
}

void rwmsg_peer_clichan_release(rwmsg_broker_clichan_t *cc) {
  rwmsg_broker_clichan_release(cc);
}

static void rwmsg_broker_clichan_halt_f(void *ud) {
  rwmsg_broker_clichan_t *cc = (rwmsg_broker_clichan_t *)ud;
  
  rwmsg_channel_halt(&cc->ch);

  int qct=0;
  rwmsg_request_t *req;
  while ((req = rwmsg_queue_dequeue(&cc->ch.localq))) {
    rwmsg_broker_clichan_reqent_t *ent = NULL;
    HASH_FIND(hh, cc->reqent_hash, &req->hdr.id, sizeof(req->hdr.id), ent);
    if (ent) {
      RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      if (ent->wheelidx >= 0) {
	RW_ASSERT(ent->wheelidx >= 0);
	RW_ASSERT(ent->wheelidx < RWMSG_BROKER_REQWHEEL_SZ);
        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_REMOVE reqwheel" FMT_MSG_HDR(ent->hdr),
                       PRN_MSG_HDR(ent->hdr));
	RW_DL_REMOVE(&cc->reqwheel[ent->wheelidx], ent, elem);
      }
      HASH_DELETE(hh, cc->reqent_hash, ent);
      if (ent->reqref) {
	if (ent->reqref == req) {
	  RW_ASSERT(ent->reqref->refct >= 2);
	}
	rwmsg_request_release(ent->reqref);
	ent->reqref = NULL;
      }
      RW_FREE_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      ent = NULL;
    }
    rwmsg_request_release(req);
    qct++;
  }
  RW_ASSERT(!cc->ch.localq.qlen);

  /* Delete remaining ents, send cancels if we can */ 
  int canct=0, canct_rel=0, hashct=0, hash_freect=0;
  rwmsg_broker_clichan_reqent_t *ent, *ent2;
  HASH_ITER(hh, cc->reqent_hash, ent, ent2) {
    hashct++;
    RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
    if (ent->wheelidx >= 0) {
      RW_ASSERT(ent->wheelidx >= 0);
      RW_ASSERT(ent->wheelidx < RWMSG_BROKER_REQWHEEL_SZ);
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_REMOVE reqwheel" FMT_MSG_HDR(ent->hdr),
                       PRN_MSG_HDR(ent->hdr));
      RW_DL_REMOVE(&cc->reqwheel[ent->wheelidx], ent, elem);
    }
    HASH_DELETE(hh, cc->reqent_hash, ent);

    rwmsg_broker_t *bro = cc->bch.bro;
    /* Try to send cancel */
    rwmsg_broker_srvchan_t *bsc = (rwmsg_broker_srvchan_t *)rwmsg_endpoint_find_channel(cc->ch.ep,
											RWMSG_METHB_TYPE_BROSRVCHAN,
											bro->bro_instid,
											ent->hdr.pathhash,
											ent->hdr.payt,
											ent->hdr.methno);
    if (bsc && !bsc->ch.halted) {
      rwmsg_request_t *req = rwmsg_request_create((rwmsg_clichan_t*)cc);
      RW_ASSERT(req->broclichan == cc);
      req->hdr = ent->hdr;
      req->hdr.isreq = TRUE;
      req->hdr.bnc = RWMSG_BOUNCE_NOPEER;
      req->hdr.cancel = TRUE;
      req->hdr.nop = FALSE;
      req->brosrvchan = bsc;

      RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      RW_ASSERT_TYPE(ent->reqref, rwmsg_request_t);

#ifndef __RWMSG_BNC_RESPONSE_W_PAYLOAD
      rwmsg_request_message_load(&req->rsp, NULL, 0);
#else
      //uint32_t len = 0;
      //const void *req_payload = rwmsg_request_get_request_payload(ent->reqref, &len);
      //rwmsg_request_message_load(&req->req, req_payload, len);
      rwmsg_request_message_load(&req->rsp, ent->reqref->req.msg.nnbuf, ent->reqref->req.msg.nnbuf_len);
#endif

      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "req peer halt cancel" FMT_MSG_HDR(req->hdr),
                       PRN_MSG_HDR(req->hdr));

      rw_status_t rs = rwmsg_queue_enqueue(&bsc->ch.localq, req);
      if (rs != RW_STATUS_SUCCESS) {
	canct_rel++;
	rwmsg_request_release(req);
      } else {
	canct++;
      }
      /* brosrvchan will release the one reference on the cancel
	 req */
    }

    if (ent->reqref) {
      RW_ASSERT(ent->reqref->refct >= 1);
      rwmsg_request_release(ent->reqref);
      ent->reqref = NULL;
      hash_freect++;
    }
    RW_FREE_TYPE(ent, rwmsg_broker_clichan_reqent_t);
    ent = NULL;
  }

  rwmsg_broker_clichan_release(cc);
  /* Reqs hold a reference to us in req->broclichan; we'll only
     finally go away when all reqs are gone */
}

void rwmsg_broker_clichan_halt_async(rwmsg_broker_clichan_t *cc) {
  rwmsg_broker_t *bro = cc->bch.bro;
  ck_pr_inc_32(&cc->ch.refct);
  rwsched_dispatch_async_f(bro->ep->taskletinfo,
			  rwsched_dispatch_get_main_queue(bro->ep->rwsched),
			  &cc->ch,
			  rwmsg_broker_acceptor_halt_bch_and_remove_from_hash_f);
}

void rwmsg_broker_clichan_halt(rwmsg_broker_clichan_t *cc) {
  rwmsg_broker_t *bro = cc->bch.bro;

  /* End timer */
  if (cc->reqwheel_timer) {
    rwsched_dispatch_source_cancel(bro->ep->taskletinfo, cc->reqwheel_timer);
    //NEW-CODE
    rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, cc->reqwheel_timer, bro->ep->rwsched, bro->ep->taskletinfo);
    cc->reqwheel_timer = NULL;

    if (bro->use_mainq) {
      rwmsg_broker_clichan_halt_f(cc);
    } else {
      rwsched_dispatch_sync_f(bro->ep->taskletinfo,
            cc->bch.rwq,
            cc,
            rwmsg_broker_clichan_halt_f);
    }
  }

  return;
}


/* Flow control feedme callback; send on sockset at pri */
void rwmsg_broker_clichan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  int pollout = FALSE;
  uint32_t sndct = 0;
  rwmsg_broker_clichan_t *cc = (rwmsg_broker_clichan_t*)ud;
  rwmsg_request_t *req;


  if (cc->bch.ch.halted) {
    return;
  }

  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_broker_clichan_t);
  RW_ASSERT(cc->ch.ss == ss);

  /* Chew on the incoming localq */
  while ((req = rwmsg_queue_head_pri(&cc->ch.localq, pri))) {
    bool ack = FALSE;

    /* Find our local hash record for this request */
    rwmsg_broker_clichan_reqent_t *ent = NULL;
    if (RWMSG_PAYFMT_MSGCTL == req->hdr.payt && RWMSG_MSGCTL_METHNO_METHBIND == req->hdr.methno) {
      RW_ASSERT(!req->hdr.cancel);
      RW_ASSERT(req->hdr.bnc < RWMSG_BOUNCE_CT);
      RW_ASSERT(req->hdr.bnc >= 0);

      int sndpri = pri;
      if (req->hdr.blocking) {
	sndpri = RWMSG_PRIORITY_BLOCKING;
      }

      RWMSG_REQ_TRACK(req);
      if (req->rwml_buffer) {
        rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                  req, 
                                  __PRETTY_FUNCTION__, 
                                  __LINE__, 
                                  "(Req Sockset Send)");
      }

      rw_status_t rs = rwmsg_sockset_send(ss, sndpri, &req->req.msg);
      if (rs != RW_STATUS_SUCCESS) {
	pollout = TRUE;
	cc->stat.ss_pollout++;
	goto out;
      }

      sndct++;
      ack = FALSE;
      //cc->stat.bnc[req->hdr.bnc]++;

      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "sent RWMSG_MSGCTL_METHNO_METHBIND" FMT_MSG_HDR(req->hdr) "into socket",
                       PRN_MSG_HDR(req->hdr));
    } else {
      HASH_FIND(hh, cc->reqent_hash, &req->hdr.id, sizeof(req->hdr.id), ent);
      if (cc->ch.chantype == RWMSG_CHAN_PEERCLI || ent) {
        RW_ASSERT(!req->hdr.cancel);
        RW_ASSERT(req->hdr.bnc < RWMSG_BOUNCE_CT);
        RW_ASSERT(req->hdr.bnc >= 0);

        int sndpri = pri;
        if (req->hdr.blocking) {
          sndpri = RWMSG_PRIORITY_BLOCKING;
        }

        RW_ASSERT(!req->hdr.isreq);
        if (!req->rsp.msg.nnbuf) {
          rwmsg_request_message_load(&req->rsp, NULL, 0);
        }
        rwmsg_request_message_load_header((req->hdr.isreq ? &req->req : &req->rsp), &req->hdr);

        RWMSG_REQ_TRACK(req);
        if (req->rwml_buffer) {
          rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                    req, 
                                    __PRETTY_FUNCTION__, 
                                    __LINE__, 
                                    "(Req Sockset Send Copy)");
        }

        rw_status_t rs = rwmsg_sockset_send_copy(ss, sndpri, &req->rsp.msg);
        if (rs != RW_STATUS_SUCCESS) {
          pollout = TRUE;
          cc->stat.ss_pollout++;
          goto out;
        }

        sndct++;
        ack = TRUE;
        cc->stat.bnc[req->hdr.bnc]++;

        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "sent rsp" FMT_MSG_HDR(req->hdr) "into socket",
                         PRN_MSG_HDR(req->hdr));
      } else {
        /* Missing: spurious or duplicate; we have already generated and sent a reply */
        cc->stat.recv_dup++;
        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "Missing: spurious or duplicate" FMT_MSG_HDR(req->hdr),
                         PRN_MSG_HDR(req->hdr));
      }
    }

    rwmsg_request_t *qreq;
    qreq = rwmsg_queue_dequeue_pri_spin(&cc->ch.localq, pri);
    RW_ASSERT(qreq == req);

    /* Send an ack in the general direction the request came from.
       Includes ack of bounces, they're no different, really... */
    if (ack && cc->ch.chantype == RWMSG_CHAN_BROCLI) {
      rwmsg_broker_t *bro = cc->bch.bro;
      /* piggyback ACK if possible */
      rwmsg_broker_srvchan_t *bsc = req->brosrvchan;
      if (!bsc) {
        bsc = (rwmsg_broker_srvchan_t *)rwmsg_endpoint_find_channel(cc->ch.ep,
                                                                    RWMSG_METHB_TYPE_BROSRVCHAN,
                                                                    bro->bro_instid,
                                                                    req->hdr.pathhash,
                                                                    req->hdr.payt,
                                                                    req->hdr.methno);
      }
      else {
        // bump up the refct
        ck_pr_inc_32(&((rwmsg_channel_t*)bsc)->refct);
      }

      if (bsc) {
        rwmsg_broker_srvchan_ackid_t ackids;
        if (cbIsFull(bsc, pri)) {
          int non_piggy_ack_sent=0;
          while (cbPeek(bsc, pri, &ackids) && non_piggy_ack_sent++<20) {
            rwmsg_broker_clichan_t *ack_cc = ackids.broclichan;
            RW_ASSERT_TYPE(ack_cc, rwmsg_broker_clichan_t); 
            rwmsg_request_t *ackreq = rwmsg_request_create((rwmsg_clichan_t*)ack_cc);
            RW_ASSERT(ackreq->broclichan == ack_cc);
            ackreq->hdr = ackids.req_hdr;
            ackreq->hdr.isreq = TRUE;
            ackreq->hdr.nop = TRUE;
            ackreq->hdr.ack = TRUE;
            ackreq->hdr.ackid = ackids.req_hdr.id;
            ackreq->brosrvchan = bsc;
            rwmsg_request_message_load(&ackreq->req, NULL, 0);
#ifdef PIGGY_ACK_DEBUG
            if (!memcmp(&ackreq->hdr.id, &ackids.req_hdr.id, sizeof(ackids.req_hdr.id)) && ackreq->hdr.bnc) {
              RWMSG_TRACE_CHAN(&ack_cc->ch, DEBUG, "piggy-ack222" FMT_MSG_HDR(ackreq->hdr),
                               PRN_MSG_HDR(ackreq->hdr));
            }
#endif
            ack_cc->stat.ack_sent++;

            RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "sending ackreq=%p nop with ackid=%u/%u/%u\n",
                             ackreq, ackids.req_hdr.id.broid, ackids.req_hdr.id.chanid, ackreq->hdr.ackid.locid);

            RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_clichan_sockset_event_send()" FMT_MSG_HDR(ackreq->hdr),
                                 ackreq, PRN_MSG_HDR(ackreq->hdr));
            ck_pr_inc_32(&ackreq->refct);
            RWMSG_REQ_TRACK(ackreq);
            rw_status_t rs = rwmsg_queue_enqueue(&bsc->ch.localq, ackreq);
            if (rs != RW_STATUS_SUCCESS) {
              ck_pr_dec_32(&ackreq->refct);
              rwmsg_request_release(ackreq);
            }
            rwmsg_broker_clichan_release(ack_cc);
            cbRead(bsc, pri, &ackids);
          }
        }
        RW_ASSERT(!cbIsFull(bsc, pri));

        // bump up the refct - we are reffering inside the cBuffer
        ck_pr_inc_32(&cc->ch.refct);
        ackids.req_hdr = req->hdr;
        ackids.wheel_idx = RWMSG_BROKER_REQWHEEL_TIMEO_IDX(bsc, 100); /* 1sec = 100cs */
        ackids.broclichan = cc;

        cbWrite(bsc, pri, &ackids);
        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "deferring ack for rsp" FMT_MSG_HDR(req->hdr),
                         PRN_MSG_HDR(req->hdr));
      } else {
        cc->stat.ack_miss++;
      }
    }

    if (ent) {
      RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      if (ent->wheelidx >= 0) {
        RW_ASSERT(ent->wheelidx >= 0);
        RW_ASSERT(ent->wheelidx < RWMSG_BROKER_REQWHEEL_SZ);
        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_REMOVE reqwheel" FMT_MSG_HDR(ent->hdr),
                       PRN_MSG_HDR(ent->hdr));
        RW_DL_REMOVE(&cc->reqwheel[ent->wheelidx], ent, elem);
      }
      HASH_DELETE(hh, cc->reqent_hash, ent);
      if (ent->reqref) {
        if (req == ent->reqref) {
          RW_ASSERT(req->refct >= 2);
        }
        rwmsg_request_release(ent->reqref);
        ent->reqref = NULL;
      }
      RW_FREE_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      ent = NULL;
    } else {
      if (cc->ch.chantype == RWMSG_CHAN_BROCLI) {
        cc->stat.recv_noent++;
      }
      else {
        if (req->refct > 1) {
          RW_ASSERT(req->refct > 1);
          ck_pr_dec_32(&req->refct);
        }
      }
    }

    rwmsg_request_release(req);
    req = NULL;
  }

 out:


  /* Resume at broker-wide request threshold.  FIXME the lock:
     allocate blocks of credits to the cc's on demand, pause on credit
     alloc fail.  Also, this will need to move to where we expire
     cached responses once we do that. */
  if (0) {
    int dopause = FALSE;
    RWMSG_EP_LOCK(cc->bch.bro->ep);
    if (cc->bch.bro->reqct > cc->bch.bro->thresh_reqct1
	&& cc->bch.bro->reqct - sndct <= cc->bch.bro->thresh_reqct1) {
      dopause = TRUE;
    }
    cc->bch.bro->reqct -= sndct;
    RWMSG_EP_UNLOCK(cc->bch.bro->ep);
    if (dopause) {
      RWMSG_TRACE_CHAN(&cc->ch, WARN, "broker req ct fell to resume thresh_reqct1=%u", cc->bch.bro->thresh_reqct1);
      rwmsg_broker_bcast_resumecli(&cc->bch);
    }
  }


  /* Establish or clear pollout registration */
  rwmsg_sockset_pollout(ss, pri, pollout);
  return;
}

void rwmsg_peer_clichan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_broker_clichan_sockset_event_send(ss, pri, ud);
}

/* Stop reading */
void rwmsg_broker_clichan_pause(rwmsg_broker_clichan_t *cc) {
  rwmsg_channel_pause_ss(&cc->ch);
}
void rwmsg_broker_clichan_resume(rwmsg_broker_clichan_t *cc) {
  rwmsg_channel_resume_ss(&cc->ch);
}

/* Dispatch async_f function(s) */
void rwmsg_broker_clichan_resume_f(void *ctx) {
  rwmsg_broker_clichan_resume((rwmsg_broker_clichan_t *)ctx);
}
void rwmsg_broker_clichan_pause_f(void *ctx) {
  rwmsg_broker_clichan_pause((rwmsg_broker_clichan_t *)ctx);
}



/* Arriving work in the local queue.  Responses / bounces. */
uint32_t rwmsg_broker_clichan_recv(rwmsg_broker_clichan_t *cc) {
  uint32_t ct=0;
  int p;

  /* Copy from localq to sockset */
  for (p=RWMSG_PRIORITY_COUNT-1; p>=0; p--) {
    rwmsg_broker_clichan_sockset_event_send(cc->ch.ss, p, cc);
  }

  return ct;
}

uint32_t rwmsg_peer_clichan_recv(rwmsg_broker_clichan_t *cc) {
  return rwmsg_broker_clichan_recv(cc);
}

/* Message read from the socket */
rw_status_t rwmsg_broker_clichan_recv_buf(rwmsg_broker_clichan_t *cc,
					  rwmsg_buf_t *buf) {
  rw_status_t rs;
  rwmsg_request_t *req;
  rwmsg_broker_t *bro = cc->bch.bro;

  cc->stat.recv++;
  req = rwmsg_request_create((rwmsg_clichan_t*)cc);
  RW_ASSERT(req->broclichan == cc);
  rs = rwmsg_buf_unload_header(buf, &req->hdr);
  if (rs != RW_STATUS_SUCCESS) {
    /* stat dud header */
    RWMSG_TRACE_CHAN(&cc->ch, CRIT, "rwmsg_broker_clichan_recv_buf rwmsg_buf_t contains dud header%s", "");
    rwmsg_request_release(req);
    goto out;
  }
  req->req.msg = *buf;

  RW_ASSERT(cc->ch.chantype == RWMSG_CHAN_BROCLI || cc->ch.chantype == RWMSG_CHAN_PEERCLI);
  if (cc->ch.chantype == RWMSG_CHAN_BROCLI) {
    /* Fill broker-assigned stuff in */
    req->hdr.id.chanid = cc->bch.id.chanid;
    req->hdr.id.broid = bro->bro_instid;
  }

  if (bro->rwmemlog) {
    ck_pr_inc_int(&bro->rwmemlog_id);
    req->rwml_buffer = rwmemlog_instance_get_buffer(bro->rwmemlog, "Req", -bro->rwmemlog_id);
    rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                              req, 
                              __PRETTY_FUNCTION__, 
                              __LINE__, 
                              "(Req Created)");
  }

  RW_ASSERT (req->hdr.payt != RWMSG_PAYFMT_MSGCTL);

  if (cc->prevlocid == req->hdr.id.locid) {
    RWMSG_TRACE_CHAN(&cc->ch, WARN, "req id=?/?/%u dup ID from client; rejecting bnc=BROKERR", cc->prevlocid);
    req->hdr.bnc = RWMSG_BOUNCE_BROKERR;
    goto rej;
  }

  /* Lookup destination */
  rwmsg_broker_srvchan_t *bsc = (rwmsg_broker_srvchan_t *)rwmsg_endpoint_find_channel(cc->ch.ep,
										      RWMSG_METHB_TYPE_BROSRVCHAN,
										      bro->bro_instid,
										      req->hdr.pathhash,
										      req->hdr.payt,
										      req->hdr.methno);
  if (!bsc) {
    RWMSG_TRACE_CHAN(&cc->ch, NOTICE, "req" FMT_MSG_HDR(req->hdr) "not found; rejecting bnc=NOPEER",
                     PRN_MSG_HDR(req->hdr));
    req->hdr.bnc = RWMSG_BOUNCE_NOPEER;
    goto rej;
  }
  req->brosrvchan = bsc;

  RWMSG_TRACE_CHAN(&cc->ch, NOTICE, "req=%p" FMT_MSG_HDR(req->hdr) "=> %s",
                   req, PRN_MSG_HDR(req->hdr),
		   bsc->ch.rwtpfx);

  if (!req->hdr.ack ||
      //(req->hdr.ack && !(req->hdr.id.broid == req->hdr.ackid.broid && req->hdr.id.locid == req->hdr.ackid.locid))) {
      (req->hdr.ack && memcmp(&req->hdr.id, &req->hdr.ackid, sizeof(req->hdr.id)))) {
    rwmsg_broker_clichan_reqent_t *ent = NULL;
    HASH_FIND(hh, cc->reqent_hash, &req->hdr.id, sizeof(req->hdr.id), ent);
    if (ent) {
      /* Already existed, huh? */

      RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
      RW_ASSERT_TYPE(ent->reqref, rwmsg_request_t);

      RWMSG_TRACE_CHAN(&cc->ch, ERROR, "req" FMT_MSG_HDR(req->hdr) "hash collision; rejecting bnc=BROKERR",
                       PRN_MSG_HDR(req->hdr));
      req->hdr.bnc = RWMSG_BOUNCE_BROKERR;

    rej:

      RWMSG_REQ_TRACK(req);

      {
	rwmsg_broker_clichan_reqent_t *ent = NULL;
	HASH_FIND(hh, cc->reqent_hash, &req->hdr.id, sizeof(req->hdr.id), ent);
	if (ent) {
	  RW_ASSERT_TYPE(ent, rwmsg_broker_clichan_reqent_t);
	  if (ent->wheelidx >= 0) {
	    RW_ASSERT(ent->wheelidx >= 0);
	    RW_ASSERT(ent->wheelidx < RWMSG_BROKER_REQWHEEL_SZ);
            RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_REMOVE reqwheel" FMT_MSG_HDR(ent->hdr),
                             PRN_MSG_HDR(ent->hdr));
	    RW_DL_REMOVE(&cc->reqwheel[ent->wheelidx], ent, elem);
	  }
	  HASH_DELETE(hh, cc->reqent_hash, ent);
	  RW_FREE_TYPE(ent, rwmsg_broker_clichan_reqent_t);
	  ent = NULL;
	}
      }

      req->hdr.isreq = FALSE;

#ifndef __RWMSG_BNC_RESPONSE_W_PAYLOAD
      rwmsg_request_message_load(&req->rsp, NULL, 0);
      rwmsg_request_message_load_header(&req->rsp, &req->hdr);
#else
      //rwmsg_request_message_load(&req->rsp, NULL, 0);
      rwmsg_request_message_load(&req->rsp, req->req.msg.nnbuf, req->req.msg.nnbuf_len);
      rwmsg_request_message_load_header(&req->rsp, &req->hdr);
#endif

      if (req->rwml_buffer) {
        rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                                  req, 
                                  __PRETTY_FUNCTION__, 
                                  __LINE__, 
                                  "(Req Sockset Send Rej)");
      }

      cc->stat.bnc[req->hdr.bnc]++;
      /* Attempt ss write */
      rs = rwmsg_sockset_send(cc->ch.ss, req->hdr.pri, &req->rsp.msg);
      if (rs == RW_STATUS_SUCCESS) {
	RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "sent rsp" FMT_MSG_HDR(req->hdr)  "into socket",
                   PRN_MSG_HDR(req->hdr));
	rwmsg_request_release(req);
	goto out;
      }

      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rsp" FMT_MSG_HDR(req->hdr)  "sockset_send() rs=%u; deferring in localq",
                       PRN_MSG_HDR(req->hdr), rs);
      goto defer;
    }

    /* Add to timeo wheel.  The extra node is awkward, would be nicer as
       part of req, but req is usually freed in another thread */
    if (cc->ch.chantype == RWMSG_CHAN_BROCLI) {
      rwmsg_broker_clichan_reqent_t *ent = RW_MALLOC0_TYPE(sizeof(*ent), rwmsg_broker_clichan_reqent_t);
      ent->hdr = req->hdr;
      ent->wheelidx = RWMSG_BROKER_REQWHEEL_TIMEO_IDX(cc, ent->hdr.timeo);
      RW_ASSERT(ent->wheelidx >= 0);
      RW_ASSERT(ent->wheelidx < RWMSG_BROKER_REQWHEEL_SZ);
      ent->brosrvchan = req->brosrvchan;
      ent->reqref = req;
      RW_DL_PUSH(&cc->reqwheel[ent->wheelidx], ent, elem);
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "DL_PUSH reqwheel" FMT_MSG_HDR(ent->hdr),
                       PRN_MSG_HDR(ent->hdr));
      HASH_ADD(hh, cc->reqent_hash, hdr.id, sizeof(ent->hdr.id), ent);
    }
  }

  /* Piggyback ACK */
  if (cc->ch.chantype == RWMSG_CHAN_BROCLI) {
    RW_ASSERT(!req->hdr.ack);
    rwmsg_broker_srvchan_ackid_t ackids;
    if (cbRead(bsc, req->hdr.pri, &ackids)) {
      rwmsg_broker_clichan_t *ack_cc = ackids.broclichan;
      RW_ASSERT_TYPE(ack_cc, rwmsg_broker_clichan_t); 
      //RW_ASSERT(bsc->ch.chantype == RWMSG_CHAN_BROSRV);
#ifdef PIGGY_ACK_DEBUG
      if (!memcmp(&req->hdr.id, &ackids.req_hdr.id, sizeof(ackids.req_hdr.id))) {
        RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "piggy-ack111" FMT_MSG_HDR(req->hdr), PRN_MSG_HDR(req->hdr));
      }
#endif 
      req->hdr.ackid = ackids.req_hdr.id;
      req->hdr.ack = TRUE;
      ack_cc->stat.ack_piggy_sent++;
      RWMSG_TRACE_CHAN(&cc->ch, NOTICE, "req" FMT_MSG_HDR(req->hdr) "=> %s",
                       PRN_MSG_HDR(req->hdr), bsc->ch.rwtpfx);

    }
  }

  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "inc_refct(req=%p) - rwmsg_broker_clichan_recv_buf()" FMT_MSG_HDR(req->hdr),
                   req, PRN_MSG_HDR(req->hdr));
  ck_pr_inc_32(&req->refct);
  RW_ASSERT(req->refct == 2);
  RWMSG_REQ_TRACK(req);
  rs = rwmsg_queue_enqueue(&bsc->ch.localq, req);
  if (rs != RW_STATUS_SUCCESS) {
    ck_pr_dec_32(&req->refct);
    RW_ASSERT(!bsc->ch.localq.qlencap);
    RW_ASSERT(!bsc->ch.localq.qszcap);
    RW_ASSERT(rs != RW_STATUS_BACKPRESSURE); /* not in the broker we don't! */
    RWMSG_TRACE_CHAN(&cc->ch, CRIT, "req" FMT_MSG_HDR(req->hdr) "unable to enqueue to destination channel chanid=%u; rejecting bnc=BROKERR",
                     PRN_MSG_HDR(req->hdr), bsc->ch.chanid);
    req->hdr.bnc = RWMSG_BOUNCE_BROKERR;
    goto rej;
  }

  /* Pause at broker-wide request cap.  FIXME the lock: allocate
     blocks of credits to the cc's on demand, pause on credit alloc
     fail. */
  if (0) {
    int dopause = FALSE;
    RWMSG_EP_LOCK(cc->bch.bro->ep);
    cc->bch.bro->reqct++;
    if (cc->bch.bro->reqct == cc->bch.bro->thresh_reqct2) {
      dopause = TRUE;
    }
    RWMSG_EP_UNLOCK(cc->bch.bro->ep);
    if (dopause) {
      RWMSG_TRACE_CHAN(&cc->ch, WARN, "broker hit request cap thresh_reqct2=%u", cc->bch.bro->thresh_reqct2);
      rwmsg_broker_bcast_pausecli(&cc->bch);
    }
  }


 out:
  return rs;

 defer:  
  cc->stat.defer++;
  if (req->hdr.bnc) {
    RW_ASSERT(req->broclichan == cc);
    RW_ASSERT(!req->hdr.isreq);
    RWMSG_REQ_TRACK(req);
    rwmsg_queue_enqueue(&cc->ch.localq, req);
    rwmsg_sockset_pollout(cc->ch.ss, req->hdr.pri, TRUE);
  } else {
    RW_CRASH();
  }
  rs = RW_STATUS_BACKPRESSURE;
  goto out;
}

rw_status_t rwmsg_peer_clichan_recv_buf(rwmsg_broker_clichan_t *cc,
					rwmsg_buf_t *buf) {
  rwmsg_header_t hdr;
  rw_status_t rs;
  rs = rwmsg_buf_unload_header(buf, &hdr);
  RW_ASSERT(rs==RW_STATUS_SUCCESS);
  RWMSG_TRACE_CHAN(&cc->ch, NOTICE, "PC-PC rwmsg_peer_clichan_recv_buf(%p, %p) req" FMT_MSG_HDR(hdr), cc, buf, PRN_MSG_HDR(hdr));
  return rwmsg_broker_clichan_recv_buf(cc, buf);
}
