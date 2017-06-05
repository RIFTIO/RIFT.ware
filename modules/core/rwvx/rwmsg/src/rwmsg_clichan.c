/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_clichan.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG client channel
 */

#include "rwmsg_int.h"

rwmsg_clichan_t *rwmsg_clichan_create(rwmsg_endpoint_t *ep) {
  RW_ASSERT_TYPE(ep, rwmsg_endpoint_t);
  rwmsg_clichan_t *cc = NULL;
  cc = (rwmsg_clichan_t*)RW_MALLOC0_TYPE(sizeof(*cc), rwmsg_clichan_t);

  int32_t val=0;
  rwmsg_endpoint_get_property_int(ep, "/rwmsg/broker/shunt", &val);
  cc->shunt = !!val;
  cc->nxid = 1;
  
  rwmsg_channel_create(&cc->ch, ep, RWMSG_CHAN_CLI);

  /* Clear outbound queue limits until feedme registration */
  if (cc->ch.ss) {
    rwmsg_queue_set_cap(&cc->ch.ssbufq, 0, 0);
  }

  return cc;
}

void rwmsg_clichan_add_service(rwmsg_clichan_t *cc, ProtobufCService *srv) {
  srv->rw_context = cc;
}

void rwmsg_clichan_destroy(rwmsg_clichan_t *cc) {
  if (!cc->ch.refct) {
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "in rwmsg_clichan_destroy cc=%p\n", cc);

    if (cc->blk.notify.theme) {
      rwmsg_notify_deinit(cc->ch.ep, &cc->blk.notify);
    }

    rwmsg_stream_idx_t *sti = NULL, *tmp = NULL;
    HASH_ITER(hh, cc->streamidx, sti, tmp) {
      HASH_DELETE(hh, cc->streamidx, sti);
      RW_ASSERT(sti->stream->refct >= 1);
      sti->stream->refct--;
      RW_FREE_TYPE(sti, rwmsg_stream_idx_t);
    }

    rwmsg_stream_t *st=NULL, *stmp=NULL;
    HASH_ITER(hh, cc->streamhash, st, stmp) {

      HASH_DELETE(hh, cc->streamhash, st);
      RW_ASSERT(st->refct == 1);
      if (st->dest) {
	rwmsg_destination_release(st->dest);
	st->dest = NULL;
      }
      int p;
      for (p=0; p<RWMSG_PRIORITY_COUNT; p++) {
	if (st->in_socket_defer[p]) {
	  RW_DL_REMOVE(&cc->streamdefer[p], st, socket_defer_elem[p]);
	  st->in_socket_defer[p] = FALSE;
	}
      }
      RW_FREE_TYPE(st, rwmsg_stream_t);
    }

    rwmsg_channel_destroy(&cc->ch);
    rwmsg_request_t *hreq = NULL, *hreq2 = NULL;
    HASH_ITER(hh, cc->outreqs, hreq, hreq2) {
      /* there shouldn't be any pending reqs at this point */
      RW_CRASH();
    }
    memset(cc, 0, sizeof(*cc));
    RW_FREE_TYPE(cc, rwmsg_clichan_t);
  }
}

void rwmsg_clichan_halt(rwmsg_clichan_t *cc) {
  RW_ASSERT(cc->ch.refct >= 1);
  rwmsg_channel_halt(&cc->ch);
  rwmsg_clichan_release(cc);
}

void rwmsg_clichan_release(rwmsg_clichan_t *cc) {
  rwmsg_endpoint_t *ep = cc->ch.ep;

  bool zero=0;
  ck_pr_dec_32_zero(&cc->ch.refct, &zero);
  if (zero) {
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "clichan refct=%d, submitting to GC\n", 0);
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_CLICHAN, cc, NULL, NULL);
  }
}

static void rwmsg_clichan_deferred_feedme_f(void *ctx) {
  rwmsg_stream_t *st = (rwmsg_stream_t *)ctx;
  if (st->cb.feedme) {
    st->cb.feedme(st->cb.ud, st->dest, st->key.streamid);
  }
}

rwmsg_stream_t *rwmsg_clichan_stream_find_destsig(rwmsg_clichan_t *cc,
						  rwmsg_destination_t *dt,
						  uint32_t methno,
						  rwmsg_payfmt_t payt,
						  rwmsg_stream_idx_t **sti_out) {
  struct rwmsg_stream_idx_key_s key;
  memset(&key, 0, sizeof(key));
  key.pathhash = dt->apathhash;
  if (payt == RWMSG_PAYFMT_PBAPI) {
    /* All PBAPI methods for a service end up in one channel/stream.
       So mask out the method part and use the service number part
       only, this way we have one stream instead of many for the
       channel<->channel relationship. */
    key.methno = RWMSG_PBAPI_METHOD_SRVHASHVAL(methno);
  } else {
    key.methno = methno;
  }
  key.payt = payt;
  rwmsg_stream_idx_t *sti = NULL;
  HASH_FIND(hh, cc->streamidx, &key, sizeof(key), sti);
  if (sti && sti_out) {
    *sti_out = sti;
  }
  return sti ? sti->stream : NULL;
}

rwmsg_stream_t *rwmsg_clichan_stream_find_destid(rwmsg_clichan_t *cc,
						 rwmsg_destination_t *dt,
						 uint32_t stid) {
  struct rwmsg_stream_key_s key;
  memset(&key, 0, sizeof(key));
  key.pathhash = dt->apathhash;
  key.streamid = stid;
  rwmsg_stream_t *st = NULL;
  HASH_FIND(hh, cc->streamhash, &key, sizeof(key), st);
  return st;
}

/* Reset the stream cache assiciated with the pathhash on this cc */
void rwmsg_clichan_stream_reset(rwmsg_clichan_t *cc,
                                rwmsg_destination_t *dt) {

  rwmsg_stream_idx_t *sti = NULL, *tmp = NULL;
  HASH_ITER(hh, cc->streamidx, sti, tmp) {
    if (sti->key.pathhash == dt->apathhash) {
      HASH_DELETE(hh, cc->streamidx, sti);
      RW_ASSERT(sti->stream->refct >= 1);
      sti->stream->refct--;
      RW_FREE_TYPE(sti, rwmsg_stream_idx_t);
    }
  }

  rwmsg_stream_t *st=NULL, *stmp=NULL;
  HASH_ITER(hh, cc->streamhash, st, stmp) {
    if (st->key.pathhash == dt->apathhash) {
      HASH_DELETE(hh, cc->streamhash, st);
      RW_ASSERT(st->refct == 1);
      if (st->dest) {
        rwmsg_destination_release(st->dest);
        st->dest = NULL;
      }
      int p;
      for (p=0; p<RWMSG_PRIORITY_COUNT; p++) {
        if (st->in_socket_defer[p]) {
          RW_DL_REMOVE(&cc->streamdefer[p], st, socket_defer_elem[p]);
          st->in_socket_defer[p] = FALSE;
        }
      }
      RW_FREE_TYPE(st, rwmsg_stream_t);
    }
  }
}

/* Update and/or create this req's stream with data from the response / ack. */
void rwmsg_clichan_stream_update(rwmsg_clichan_t *cc,
				 rwmsg_request_t *req) {
  RW_ASSERT(req->st);

  bool wasclosed = (req->st->tx_out >= req->st->tx_win);

  req->st->tx_out--;

  uint32_t stid;
  stid = req->hdr.stid;
  if (stid) {
    if (req->st->key.streamid != stid) {
      rwmsg_stream_t *st_o = NULL;
      rwmsg_stream_t *st;
      if (req->st->key.streamid) {
	/* We actually had some other stream, forget it */
	st_o = rwmsg_clichan_stream_find_destid(cc, req->dest, req->st->key.streamid);
      }

      st = rwmsg_clichan_stream_find_destid(cc, req->dest, stid);
      if (!st) {
	/* Create new stream */
	st = RW_MALLOC0_TYPE(sizeof(*st), rwmsg_stream_t);
	st->key.pathhash = req->dest->apathhash;
	RW_ASSERT(req->hdr.stid == stid);
	st->key.streamid = req->hdr.stid;
	st->tx_win = st_o ? st_o->tx_win : 1;
	st->tx_out = st_o ? st_o->tx_out : 0;
	st->seqno = st_o ? st_o->seqno : 1;
	st->localsc = st_o ? st_o->localsc : NULL;
	st->refct = 1;
	st->dest = req->dest;
	ck_pr_inc_32(&req->dest->refct);   

	RWMSG_TRACE_CHAN(&cc->ch, INFO, "streamhash ref dest=%p new st stid=%u win=%u phash=0x%lx path='%s'", 
			 req->dest, st->key.streamid, req->hdr.stwin, st->key.pathhash, req->dest->apath);
	
	/* Add to ID index */
	rwmsg_stream_t *chkst = NULL;
	HASH_FIND(hh, cc->streamhash, &st->key, sizeof(st->key), chkst);
	RW_ASSERT(!chkst);
	HASH_ADD(hh, cc->streamhash, key, sizeof(st->key), st);
      }

      if (st_o) {
        /* Find the old stream by sig, eject that index entry */
        rwmsg_stream_t *ost;
        rwmsg_stream_idx_t *sti = NULL;
        RW_ASSERT(!st_o->defstream);
        ost = rwmsg_clichan_stream_find_destsig(cc, req->dest, req->hdr.methno, req->hdr.payt, &sti);
        if (ost == st_o && sti) {
          RW_ASSERT(!ost->defstream);
          HASH_DELETE(hh, cc->streamidx, sti);
          RW_FREE_TYPE(sti, rwmsg_stream_idx_t);
          if (ost->refct > 0) {
            ost->refct--;
          }
          if (ost->refct == 0) {

            HASH_DELETE(hh, cc->streamhash, ost);
            if (ost->dest) {
              rwmsg_destination_release(ost->dest);
              ost->dest = NULL;
            }
            int p;
            for (p=0; p<RWMSG_PRIORITY_COUNT; p++) {
              if (ost->in_socket_defer[p]) {
                RW_DL_REMOVE(&cc->streamdefer[p], ost, socket_defer_elem[p]);
                ost->in_socket_defer[p] = FALSE;
              }
            }
            RW_FREE_TYPE(ost, rwmsg_stream_t);
          }
        }
      }
      req->st = st;

      /* Update dest/sig index */
      rwmsg_stream_idx_t *sti = NULL;
      rwmsg_stream_t *nst = rwmsg_clichan_stream_find_destsig(cc, req->dest, req->hdr.methno, req->hdr.payt, &sti);
      if (nst && nst != st) {
	/* Delete the old sig mapping */ 
	RW_ASSERT(sti);
	HASH_DELETE(hh, cc->streamidx, sti);
	RW_FREE_TYPE(sti, rwmsg_stream_idx_t);
	sti = NULL;
	/* Maybe delete the old stream itself */
	if (nst->refct > 0) {
	  nst->refct--;
	}
	if (nst->refct == 0) {

	  if (nst->dest) {
	    rwmsg_destination_release(nst->dest);
	    nst->dest = NULL;
	  }
	  int p;
	  for (p=0; p<RWMSG_PRIORITY_COUNT; p++) {
	    if (nst->in_socket_defer[p]) {
	      RW_DL_REMOVE(&cc->streamdefer[p], nst, socket_defer_elem[p]);
	      nst->in_socket_defer[p] = FALSE;
	    }
	  }
	  HASH_DELETE(hh, cc->streamhash, nst);
	  RW_FREE_TYPE(nst, rwmsg_stream_t);
	}
      }
      if (!sti) {

	RWMSG_TRACE_CHAN(&cc->ch, INFO, "add new mapping in streamidx for st=%p stid=%u <- methno=%u payt=%u phash=0x%lx path='%s'",
			 st, st->key.streamid, req->hdr.methno, req->hdr.payt, req->dest->apathhash, req->dest->apath);

	/* Create new mapping node and add to dest/sig index */
	rwmsg_stream_idx_t *sti = RW_MALLOC0_TYPE(sizeof(*sti), rwmsg_stream_idx_t);
	sti->key.pathhash = req->dest->apathhash;
	if (req->hdr.payt == RWMSG_PAYFMT_PBAPI) {
	  sti->key.methno = RWMSG_PBAPI_METHOD_SRVHASHVAL(req->hdr.methno);
	} else {
	  sti->key.methno = req->hdr.methno;
	}
	sti->key.payt = req->hdr.payt;
	sti->stream = st;
	HASH_ADD(hh, cc->streamidx, key, sizeof(sti->key), sti);
	st->refct++;
      }
    }
  }

  if (cc->bch_id == 0 && !req->local)
    cc->bch_id = (req->hdr.id.broid << 18) | (req->hdr.id.chanid);

  if (!req->st->localsc) {
    req->st->tx_win = req->hdr.stwin;
  } else {
    req->st->tx_win = 100;	/* no win, just rely on dest queue cap if any */
  }
  if (req->hdr.bnc) {
    /* Reset seqno and window on bounce */
    switch (req->hdr.bnc) {
      /* TBD: explicit bounces from the method needn't bother */
    default:
      req->st->seqno = 0;
      req->st->tx_win = req->st->tx_out + 1;
      break;
    }
  }

  bool nowopen = (req->st->tx_out < req->st->tx_win);

  /* If this response opens up the window, tell the user-provided
     callback. */
  if (wasclosed && nowopen) {
    if (req->st->cb.feedme) {
      if (req->hdr.blocking) {
	/* Queue an event to self */
	rwsched_dispatch_queue_t rwq = NULL;
	switch(cc->ch.notify.theme) {
	case RWMSG_NOTIFY_RWSCHED:
	  rwq = cc->ch.notify.rwsched.rwqueue;
	  RW_ASSERT(rwq);
	  break;
	case RWMSG_NOTIFY_CFRUNLOOP:
	  rwq = rwsched_dispatch_get_main_queue(cc->ch.ep->rwsched);
	  RW_ASSERT(rwq);
	  break;
	default:
	  /* Unbound channel, no support */
	  break;
	}
	if (rwq) {
	  rwsched_dispatch_async_f(cc->ch.ep->taskletinfo,
				   rwq,
				   req->st,
				   rwmsg_clichan_deferred_feedme_f);
	}
      } else if (cc->ch.notify.theme) {
	/* Nonblocking request on bound channel, just call back now */
	req->st->cb.feedme(req->st->cb.ud, req->st->dest, req->st->key.streamid);
      } else {
	/* Nonblocking requests on unbound channel, no support */
      }
    }
  }
}

uint64_t max_rtt = 1000;

static rw_status_t rwmsg_clichan_recv_req(rwmsg_clichan_t *cc,
					  rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_recv_req" FMT_MSG_HDR(req->hdr),
                   PRN_MSG_HDR(req->hdr));

  rwmsg_clichan_stream_update(cc, req);

  struct timeval now;
  gettimeofday(&now, NULL);

  timersub(&now, &req->hdr.tracking._[0].tv, &req->rtt);

  static uint64_t max_rtt = 1000000;
  uint64_t rtt;
  if ((max_rtt < (rtt = rwmsg_request_get_response_rtt(req))) || req->hdr.bnc) {
    int i;
    struct timeval delta;
    for (i=0; i<req->hdr.tracking.ct; i++) {
      timersub(&now, &req->hdr.tracking._[i].tv, &delta);
      RWMSG_TRACE_CHAN(&cc->ch, WARN, "MAX req_tracking[%u] delta=%lu, loc:%u",
                       i, (delta.tv_usec + delta.tv_sec * 1000000), req->hdr.tracking._[i].line);
    }
    RWMSG_TRACE_CHAN(&cc->ch, INFO, "MAX rwmsg_request_get_response_rtt = %lu" FMT_MSG_HDR(req->hdr),
                     rtt, PRN_MSG_HDR(req->hdr));
    max_rtt = max_rtt < rtt ? rtt : max_rtt;
  }
  
  switch (req->hdr.payt) {
  case RWMSG_PAYFMT_PBAPI:
    if (req->cb.pbrsp) {
      ProtobufCMessage *arrivingmsg=NULL;
      if (!req->hdr.bnc) {
	rw_status_t rs = rwmsg_request_get_response(req, (const void**)&arrivingmsg);
	if (rs != RW_STATUS_SUCCESS) {
	  arrivingmsg = NULL;
	}
      }
      req->cb.pbrsp(arrivingmsg, req, req->cb.ud);
    }
    break;
  default:
    if (req->cb.response) {
      req->cb.response(req, req->cb.ud);
    }
    break;
  }
  
  /* Stash the request handle to free later if the caller doesn't.
     Also release our reference to the srvchan, if any, right away. */
  if (req->srvchan) {
    _RWMSG_CH_DEBUG_(&req->srvchan->ch, "--");
    rwmsg_srvchan_release(req->srvchan);
    req->srvchan = NULL;
  }
  RWMSG_EP_LOCK(cc->ch.ep);	/* fixme, make cc-specific */
  if (cc->ch.ep->lastreq && cc->ch.ep->lastreq != req) {

    rwmsg_request_t *hreq = NULL;
    HASH_FIND(hh, cc->outreqs, &req->hdr.id.locid, sizeof(req->hdr.id.locid), hreq);
    RW_ASSERT(hreq == NULL);	/* else, this req by locid is in the outreqs hash?! */

    rwmsg_request_release(cc->ch.ep->lastreq);
  }
  cc->ch.ep->lastreq = req;
  RWMSG_EP_UNLOCK(cc->ch.ep);
  req = NULL;
  
  return RW_STATUS_SUCCESS;
}


rw_status_t rwmsg_clichan_recv_buf(rwmsg_clichan_t *cc,
				   rwmsg_buf_t *buf) {
  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_request_t *req;

  rwmsg_header_t hdrtmp;
  rs = rwmsg_buf_unload_header(buf, &hdrtmp);
  if (rs == RW_STATUS_SUCCESS) {
    HASH_FIND(hh, cc->outreqs, &hdrtmp.id.locid, sizeof(hdrtmp.id.locid), req);
    if (req) {
      HASH_DELETE(hh, cc->outreqs, req);
      req->rsp.msg = *buf;
      memcpy(&req->hdr, &hdrtmp, sizeof(req->hdr));
      rs = rwmsg_clichan_recv_req(cc, req);
      goto out;
    } else {
      RWMSG_TRACE_CHAN(&cc->ch, ERROR, "unk req locid=%u in clichan_recv_buf", 
		       hdrtmp.id.locid);
    }
  }
  if (buf->nnbuf) {
    nn_freemsg(buf->nnbuf);
    buf->nnbuf = NULL;
  }
 out:
  return rs;
}

rw_status_t rwmsg_clichan_feedme_callback(rwmsg_clichan_t *cc,
					  rwmsg_destination_t *dest,
					  rwmsg_flowid_t flowid,
					  rwmsg_closure_t *cb) {
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);
  RW_ASSERT(flowid);
  RW_ASSERT(dest);

  RW_ASSERT(cc->ch.notify.theme == RWMSG_NOTIFY_RWSCHED
	    || cc->ch.notify.theme == RWMSG_NOTIFY_CFRUNLOOP);

  rw_status_t retval = RW_STATUS_FAILURE;

  rwmsg_stream_t *st;
  st = rwmsg_clichan_stream_find_destid(cc, dest, flowid);
  if (!st) {
    retval = RW_STATUS_NOTFOUND;
    goto ret;
  }

  rwmsg_destination_t *odest = st->dest;

  if (cb) {
    st->dest = dest;
    ck_pr_inc_32(&dest->refct);
    memcpy(&st->cb, cb, sizeof(st->cb));
    if (cc->ch.ss) {
      rwmsg_queue_set_cap_default(&cc->ch.ssbufq, "cc_ssbufq");
    }
    cc->feedme_count ++;
  } else {
    RW_CRASH();		/* unsupported, once engaged it's clichan-wide */
    st->dest = NULL;
    memset(&st->cb, 0, sizeof(st->cb));
    cc->feedme_count --;
  }
  if (odest) {
    rwmsg_destination_release(odest);
  }

  retval = RW_STATUS_SUCCESS;

 ret:
  return retval;
}

void rwmsg_clichan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_clichan_t *cc = (rwmsg_clichan_t*)ud;
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);

  RW_ASSERT(ss == cc->ch.ss);
  rwmsg_sockset_pollout(ss, pri, FALSE);

  rw_status_t rs = rwmsg_channel_send_buffer_pri(&cc->ch, pri, FALSE, TRUE);
  if (rs != RW_STATUS_SUCCESS) {
    goto out;
  }

  /* Flow control feedme callback to app.  Check stream/flow specific
     one, then dest specific but stream zero one, then clichan-wide
     no-dest no-stream one. */

  /* Stream-specific callback(s) */
  rwmsg_stream_t *st;
  int cap = RW_DL_LENGTH(&cc->streamdefer[pri]);
  while (cap-- && (st = RW_DL_REMOVE_HEAD(&cc->streamdefer[pri], rwmsg_stream_t, socket_defer_elem[pri]))) {
    st->in_socket_defer[pri] = FALSE;
    if (st->cb.feedme) {
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_sockset_event_send calling app feedme.cb(ctx=%p, dest=%p '%s', flowid=%u)",
		       st->cb.ud, st->dest, st->dest->apath, st->key.streamid);
      st->cb.feedme(st->cb.ud, st->dest, st->key.streamid);

      if (rwmsg_sockset_paused_p(ss, pri)) {
	goto out;
      }
    }
  }
  
  // TBD default callbacks...

 out:
  return;
}


uint32_t rwmsg_clichan_recv(rwmsg_clichan_t *cc) {
  uint32_t rval = 0;
  
  int max=1;
  while (max--) {
    rw_status_t rs = RW_STATUS_FAILURE;
    rwmsg_request_t *req;

    req = rwmsg_queue_dequeue(&cc->ch.localq);
    if (req) {
      /* Local */
      RW_ASSERT_TYPE(req, rwmsg_request_t);
      rs = rwmsg_clichan_recv_req(cc, req);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rval++;
    } else { 
      /* Remote */
      rwmsg_buf_t buf = { .nnbuf = NULL, .nnbuf_len = 0 };
      rwmsg_priority_t pri;
      if (cc->ch.ss) {
	rs = rwmsg_sockset_recv(cc->ch.ss, &buf, &pri);
	if (rs == RW_STATUS_SUCCESS) {
	  rs = rwmsg_clichan_recv_buf(cc, &buf);
	  if (rs == RW_STATUS_SUCCESS) {
	    rval++;
	  }
	}
      }
    }
  }

  return rval;
}

rw_status_t rwmsg_clichan_send_blocking(rwmsg_clichan_t *cc,
					rwmsg_destination_t *dest,
					rwmsg_request_t *req) {
  rwsched_CFRunLoopSourceRef cfblksrc = NULL;
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  if (!cc->ch.taskletinfo) {
    rs = RW_STATUS_FAILURE;
    goto ret;
  }

  req->hdr.blocking = 1;
  rs = rwmsg_clichan_send(cc, dest, req);
  if (rs != RW_STATUS_SUCCESS) {
    goto ret;
  }
  if (req->local) {
    // init blk.notify as CF src, if not already
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "send_blocking, local, id=%u dest='%s'\n", 
		     req->hdr.id.locid, dest->apath);
    if (cc->blk.notify.theme != RWMSG_NOTIFY_CFRUNLOOP) {
      cc->blk.notify.theme = RWMSG_NOTIFY_CFRUNLOOP;
      cc->blk.notify.cfrunloop.rwsched = cc->ch.ep->rwsched;
      cc->blk.notify.cfrunloop.taskletinfo = cc->ch.taskletinfo;
      rs = rwmsg_notify_init(cc->ch.ep, &cc->blk.notify, "cc->blk.notify", NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rwmsg_notify_clear(&cc->blk.notify, 1);
    }
    cfblksrc = cc->blk.notify.cfrunloop.cfsource;
    RW_ASSERT(cfblksrc);
  } else {
    /* remote, block on sockset's RWMSG_PRIORITY_BLOCKING socket input events */
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "send_blocking, remote, id=%u dest='%s'\n", 
		     req->hdr.id.locid, dest->apath);
    cfblksrc = rwmsg_sockset_get_blk_cfsrc(cc->ch.ss);
  }
  if (!cfblksrc) {
    rs = RW_STATUS_FAILURE;
    goto retbnc;
  }

  int iter=0;
  do {
    struct timeval start, stop, delta;
    gettimeofday(&start, NULL);

    const int blktime = 5/*s*/;
    rwsched_CFRunLoopSourceRef cfsrc;
    cfsrc = rwsched_tasklet_CFRunLoopRunTaskletBlock(cc->ch.taskletinfo,
					     cfblksrc,
					     blktime);
    gettimeofday(&stop, NULL);
    timersub(&stop, &start, &delta);

    if (cfsrc) {
      RW_ASSERT(cfsrc == cfblksrc);
      if (req->local) {
	rwmsg_notify_clear(&cc->blk.notify, 1);
	if (!req->hdr.isreq) {
	  rs = RW_STATUS_SUCCESS;
	  goto ret;
	}
      } else {
	rwmsg_buf_t buf = { .nnbuf=NULL };
	rs = rwmsg_sockset_recv_pri(cc->ch.ss, RWMSG_PRIORITY_BLOCKING, &buf);
	if (rs == RW_STATUS_SUCCESS) {
	  rwmsg_header_t hdrtmp;
	  rs = rwmsg_buf_unload_header(&buf, &hdrtmp);
	  if (rs == RW_STATUS_SUCCESS) {
	    if (hdrtmp.id.locid == req->hdr.id.locid) {
	      req->rsp.msg = buf;
	      memcpy(&req->hdr, &hdrtmp, sizeof(req->hdr));
	      HASH_DELETE(hh, cc->outreqs, req);
	      goto ret;
	    }
	  }
	  if (buf.nnbuf) {
	    nn_freemsg(buf.nnbuf);
	    buf.nnbuf = NULL;
	  }
	}
      }
    }

    RW_ASSERT(req->hdr.timeo*10 > 1000); /* blocking timeo under 1s is iffy */
    RW_ASSERT(req->hdr.timeo*10 > blktime*1000); /* timeo under our iterating resolution is doomed */

    if (req->hdr.isreq) {
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "blocking_send still waiting for req" FMT_MSG_HDR(req->hdr) "%d/%ds",
                      PRN_MSG_HDR(req->hdr),
                      iter*blktime, req->hdr.timeo*10);
    }

    if (iter++ > req->hdr.timeo*10 / 1000 / blktime) {
      /* Timeout of a local blocking request.  Ick. */
      RWMSG_TRACE_CHAN(&cc->ch, WARN, "blocking_send timeout of req" FMT_MSG_HDR(req->hdr) "%d/%ds", 
                       PRN_MSG_HDR(req->hdr),
		       iter*blktime, req->hdr.timeo*10);
      rs = RW_STATUS_TIMEOUT;
      goto retbnc;
    }
  } while (req->hdr.isreq);

  /* Stash the request handle to free later if the caller doesn't.
     Also release our reference to the srvchan, if any, right away. */
  if (req->srvchan) {
    _RWMSG_CH_DEBUG_(&req->srvchan->ch, "--");
    rwmsg_srvchan_release(req->srvchan);
    req->srvchan = NULL;
  }

 ret:
  if (rs == RW_STATUS_SUCCESS) {
    rwmsg_clichan_stream_update(cc, req);
  }

 retbnc:
  RWMSG_EP_LOCK(cc->ch.ep);	/* fixme, make cc-specific */
  if (cc->ch.ep->lastreq && cc->ch.ep->lastreq != req) {
    rwmsg_request_release(cc->ch.ep->lastreq);
  }
  cc->ch.ep->lastreq = req;
  RWMSG_EP_UNLOCK(cc->ch.ep);

  return rs;
}


static rw_status_t rwmsg_clichan_send_internal(rwmsg_clichan_t *cc,
					       rwmsg_destination_t *dest,
					       rwmsg_request_t *req,
					       bool  dispatched) {
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_send_internal called - req" FMT_MSG_HDR(req->hdr) "dest=%p dest.phash=0x%lx",
                   PRN_MSG_HDR(req->hdr),
                   dest, dest->apathhash);

  // dest pathserial is kosher? else lookup
  req->dest = dest;
  ck_pr_inc_32(&dest->refct);
  //
  if (dispatched) {
    RW_ASSERT(!req->hdr.blocking &&
	      !cc->feedme_count &&
	      !dest->feedme_count &&
	      cc->ch.ss);
  }

  req->hdr.pathhash = dest->apathhash;

  if (!dest->apathserial && !cc->shunt) {
    dest->localep = rwmsg_endpoint_path_localfind_ref(cc->ch.ep, dest->apath);
    if (dest->localep) {
      /* FIXME.  This should note the serial number of the current
	 local set of paths, every time the localfind lookup occurs,
	 (not just on hits as this does).  So any change to what is
	 local will trigger a new localfind, or if there is the same
	 set as last time the dest was looked up we'll skip it. */
      dest->apathserial=1;
    }
  }

  // map from req signature+destination to stream
  req->st = rwmsg_clichan_stream_find_destsig(cc, req->dest, req->hdr.methno, req->hdr.payt, NULL);
  if (!req->st) {
    req->st = &dest->defstream;
  }

  // clistream contains local srvchan reference
  if (dest->localep && !req->st->localsc/* fixme to lookup anew with serial change */) {
    rwmsg_channel_t *ch;
    ch = rwmsg_endpoint_find_channel(cc->ch.ep, RWMSG_METHB_TYPE_SRVCHAN, -1, dest->apathhash, req->hdr.payt, req->hdr.methno);
    if (ch) {
      RW_ASSERT_TYPE(ch, rwmsg_srvchan_t);
      req->st->localsc = (rwmsg_srvchan_t*)ch;
      req->st->tx_win = 100;
    }
  }

  // check stream window state, maybe eagain
  if (!req->hdr.blocking) {
    if (req->st->tx_out >= req->st->tx_win 
	&& req->st->cb.feedme /* aka, user doesn't handle flow control */) {
      RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send window backpressure, st=%u tx_out=%u >= tx_win=%u\n", 
		       req->st->key.streamid,
		       (unsigned)req->st->tx_out, (unsigned)req->st->tx_win);
      
      rs = RW_STATUS_BACKPRESSURE;
      goto ret;
    }
  }
  req->hdr.stid = req->st->key.streamid;
  req->hdr.seqno = req->st->seqno;

  /* Assign ID for response correlation.  Bro 0 / chid / nxid locally;
     broker-mediated requests will have broid/brochid plugged in
     instead of 0/chid */
  req->hdr.id.chanid = cc->ch.chanid;
  req->hdr.id.locid = cc->nxid;
  req->bch_id = cc->bch_id;
  rwmsg_request_message_load_header(&req->req, &req->hdr);
  
  if (req->st->localsc) {
    /* Local, queue on destination srvchan */
    RW_ASSERT_TYPE(req->st->localsc, rwmsg_srvchan_t);
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_send req" FMT_MSG_HDR(req->hdr)  "path='%s' => channel s%u",
                     PRN_MSG_HDR(req->hdr),
		     dest->apath,
		     req->st->localsc->ch.chanid);

    req->local=1;
    RWMSG_REQ_TRACK(req);
    rs = rwmsg_queue_enqueue(&req->st->localsc->ch.localq, req);
    if (rs == RW_STATUS_SUCCESS) {
      if (cc->nxid == ~0U) {
	cc->nxid = 1;
      } else {
	cc->nxid++;
      }
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      if (!req->st->cb.feedme) {
	/* If there is at least one client (us!) of this destination
	   queue with no feedme support, attempt to clear destination
	   queue's cap and enqueue again */
	rwmsg_queue_set_cap(&req->st->localsc->ch.localq, 0, 0);
        RWMSG_REQ_TRACK(req);
	rs = rwmsg_queue_enqueue(&req->st->localsc->ch.localq, req);
	if (rs == RW_STATUS_SUCCESS) {
	  if (cc->nxid == ~0U) {
	    cc->nxid = 1;
	  } else {
	    cc->nxid++;
	  }
	} else {
	  RW_ASSERT(rs != RW_STATUS_BACKPRESSURE); /* we just set the cap to 0! */
	}
      }
    }
  } else if (cc->ch.ss) {
    /* Send on sockset */
    RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_send req id=?/?/%u req" FMT_MSG_HDR(req->hdr)  "path='%s' => broker",
                     req->hdr.id.locid,
                     PRN_MSG_HDR(req->hdr),
                     dest->apath);

    rwmsg_priority_t pri = req->hdr.pri;
    rs = rwmsg_channel_send_buffer_pri(&cc->ch, pri, FALSE, TRUE);
    if (rs != RW_STATUS_SUCCESS) {
      goto queuereq;
    }

    RWMSG_REQ_TRACK(req);
    rs = rwmsg_sockset_send(cc->ch.ss, pri, &req->req.msg);
    if (rs == RW_STATUS_SUCCESS) {
      cc->started = 1;
      if (cc->nxid == ~0U) {
	cc->nxid = 1;
      } else {
	cc->nxid++;
      }
{
rwmsg_request_t *hreq = NULL;
HASH_FIND(hh, cc->outreqs, &req->hdr.id.locid, sizeof(req->hdr.id.locid), hreq);
RW_ASSERT(hreq == NULL);	/* else, this req by locid is in the outreqs hash?! */
}

      HASH_ADD(hh, cc->outreqs, hdr.id.locid, sizeof(req->hdr.id.locid), req);
      rwmsg_request_message_free(&req->req);
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      /* Can we queue? */
      if (!cc->started) {
	RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send() socket backpressure, clichan not started, queueing req=?/?/%u", req->hdr.id.locid);
	goto queuereq;
      }
      if (!req->st->cb.feedme) {
	RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send() socket backpressure, no feedme cb, queueing req=?/?/%u", req->hdr.id.locid);
	goto queuereq;
      } else {
	/* We can't queue and there is a feedme callback, so arrange for a callback */
	RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send() socket backpressure%s, marking stream for defer/feedme", "");
	if (!req->st->in_socket_defer[pri]) {
	  RW_DL_INSERT_TAIL(&cc->streamdefer[pri], req->st, socket_defer_elem[pri]);
	  req->st->in_socket_defer[pri] = TRUE;
	}
	rwmsg_sockset_pollout(cc->ch.ss, pri, TRUE);
      }

    } else if (rs == RW_STATUS_NOTCONNECTED) {
      queuereq:
      /* If bound and not blocking, we can defer.  TODO We'd like to
	 do blocking reqs too, but there is no API yet to fish reqs
	 out of the middle (tail?) of the ssbufq. */
      if (!req->hdr.blocking && !dest->noconndontq) {
	if (cc->ch.notify.theme == RWMSG_NOTIFY_CFRUNLOOP || cc->ch.notify.theme == RWMSG_NOTIFY_RWSCHED) {
	  RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "rwmsg_clichan_send() enqueue of req=?/?/%u", req->hdr.id.locid);
          RWMSG_REQ_TRACK(req);
	  rs = rwmsg_queue_enqueue(&cc->ch.ssbufq, req);
	  if (rs == RW_STATUS_SUCCESS) {
	    if (cc->nxid == ~0U) {
	      cc->nxid = 1;
	    } else {
	      cc->nxid++;
	    }
{
rwmsg_request_t *hreq = NULL;
HASH_FIND(hh, cc->outreqs, &req->hdr.id.locid, sizeof(req->hdr.id.locid), hreq);
RW_ASSERT(hreq == NULL);	/* else, this req by locid is in the outreqs hash?! */
}
	    HASH_ADD(hh, cc->outreqs, hdr.id.locid, sizeof(req->hdr.id.locid), req);
	    // rwmsg_request_message_free req->req on xmit
	    rwmsg_sockset_pollout(cc->ch.ss, pri, TRUE);
	  } else {
	    RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send() enqueue failure rs=%d", rs);	    
	  }
	} else {
	  RWMSG_TRACE_CHAN(&cc->ch, INFO, "rwmsg_clichan_send() unbound clichan, can't queue req=?/?/%u", req->hdr.id.locid);
	}
      } else {
	RWMSG_TRACE_CHAN(&cc->ch, WARN, "rwmsg_clichan_send() blocking request, can't queue req=?/?/%u", req->hdr.id.locid);
      }
    }
  }

  if (rs == RW_STATUS_SUCCESS) {
    req->st->tx_out++;
    req->st->seqno++;
    if (!req->st->seqno) {
      req->st->seqno = 1;
    }
  }

 ret:
  return rs;
}

#define Ticket_3050
#ifdef Ticket_3050
typedef struct rwmsg_clichan_dispatch_ud_s {
  rwmsg_request_t *req;
  rwmsg_clichan_t *cc;
  rwmsg_destination_t *dest;
} rwmsg_clichan_dispatch_ud_t;

static void rwmsg_clichan_send_f(void *ud) {
  rwmsg_clichan_dispatch_ud_t *dispatch_ud = (rwmsg_clichan_dispatch_ud_t*) ud;
  RW_ASSERT(dispatch_ud);
  rwmsg_request_t *req = dispatch_ud->req;
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rwmsg_clichan_t *cc = dispatch_ud->cc;
  RW_ASSERT(cc);
  rwmsg_destination_t *dest = dispatch_ud->dest;
  RW_ASSERT(dest);

  RW_ASSERT(!req->hdr.blocking);
  RW_ASSERT(cc->ch.ss);

  rwmsg_clichan_send_internal(cc, dest, req, TRUE);

  RW_FREE_TYPE(ud, rwmsg_clichan_dispatch_ud_t);
  rwmsg_destination_release(dest);
  rwmsg_request_release(req);
  rwmsg_clichan_release(cc);

  return;
}
#endif

rw_status_t rwmsg_clichan_connection_status(rwmsg_clichan_t *cc) {
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);

  if (cc->ch.ss) {
    return rwmsg_sockset_connection_status(cc->ch.ss);
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t rwmsg_clichan_send(rwmsg_clichan_t *cc,
			       rwmsg_destination_t *dest,
			       rwmsg_request_t *req) {
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RW_ASSERT_TYPE(dest, rwmsg_destination_t);
#ifdef Ticket_3050
  if (!req->hdr.blocking &&
      !cc->feedme_count &&
      !dest->feedme_count &&
      !dest->noconndontq &&
      cc->ch.ss) {
    rwsched_dispatch_queue_t rwq = NULL;
    switch(cc->ch.notify.theme) {
    case RWMSG_NOTIFY_RWSCHED:
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "clichan send async/dispatch path%s", "");
      rwq = cc->ch.notify.rwsched.rwqueue;
      RW_ASSERT(rwq);
      break;
    case RWMSG_NOTIFY_CFRUNLOOP:
      RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "clichan send async/cf path%s", "");
      rwq = rwsched_dispatch_get_main_queue(cc->ch.ep->rwsched);
      RW_ASSERT(rwq);
      break;
    default:
      /* Unbound channel, no support */
      goto non_async;
      break;
    }
    rwmsg_clichan_dispatch_ud_t *ud = RW_MALLOC0_TYPE(sizeof(*ud), rwmsg_clichan_dispatch_ud_t);
    ud->req = req;
    ck_pr_inc_32(&req->refct);
    ud->cc = cc;
    ck_pr_inc_32(&cc->ch.refct);
    ud->dest = dest;
    ck_pr_inc_32(&dest->refct);
    rwsched_dispatch_async_f(cc->ch.ep->taskletinfo,
			     rwq,
			     ud,
			     rwmsg_clichan_send_f);
    return RW_STATUS_SUCCESS;
  }
non_async:
#endif
  RWMSG_TRACE_CHAN(&cc->ch, DEBUG, "clichan send non-async path%s", "");
  return rwmsg_clichan_send_internal(cc, dest, req, FALSE);
}

uint32_t rwmsg_clichan_can_send_howmuch(rwmsg_clichan_t *cc,
                                        rwmsg_destination_t *dest,
                                        uint32_t methno,
                                        rwmsg_payfmt_t payt) {
  rwmsg_stream_t *st = NULL;

  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);
  RW_ASSERT_TYPE(dest, rwmsg_destination_t);

  st = rwmsg_clichan_stream_find_destsig(cc, dest, methno, payt, NULL);
  if (!st) {
    st = &dest->defstream;
  }
  if (st) {
    if (st->cb.feedme) {
      if (st->tx_out >= st->tx_win) {
        return 0;
      } else {
        return st->tx_win - st->tx_out;
      }
    } else {
     return -1;
    }
  } else {
   return 0;
  }
}

rw_status_t rwmsg_clichan_send_protoc_internal(void *rw_context,
					       rwmsg_destination_t *dest,
					       uint32_t flags,
					       ProtobufCService *service,
					       uint32_t methidx,
					       ProtobufCMessage *input,
					       rwmsg_closure_t *closure, 
					       rwmsg_request_t **req_out) {
  RW_ASSERT(rw_context);
  RW_ASSERT(input->descriptor == service->descriptor->methods[methidx].input);

  rw_status_t rs = RW_STATUS_FAILURE;
  rwmsg_clichan_t *cc = (rwmsg_clichan_t*)rw_context;

  rwmsg_request_t *req = rwmsg_request_create(cc);
  req->pbcli = service;
  req->methidx = methidx;

  {
    uint8_t pkbuf[512];
    uint8_t *tmpbuf = NULL;
    uint8_t *buf = pkbuf;
    size_t size = protobuf_c_message_get_packed_size(NULL, input);
    if (size > sizeof (pkbuf))
    {
      buf = tmpbuf = (uint8_t*)malloc(size);
    }
    size_t sz = protobuf_c_message_pack(NULL, input, buf);
    RW_ASSERT(sz == size);
    rwmsg_request_set_payload(req, buf, sz);
    if (tmpbuf)
    {
      free(tmpbuf);
    }
  }

  /* This is dumb, just set the three values already */
  RW_ASSERT(service->descriptor->rw_srvno <= RWMSG_PBAPI_SERVICE_MAX);
  RW_ASSERT(service->descriptor->methods[methidx].rw_methno <= RWMSG_PBAPI_METHOD_MAX);
  uint32_t methno = RWMSG_PBAPI_METHOD(service->descriptor->rw_srvno, service->descriptor->methods[methidx].rw_methno);

  //FIXME - delete the below OLD_CODE
#ifdef OLD_CODE
  rwmsg_signature_t *sig = rwmsg_signature_create(cc->ch.ep,
						  RWMSG_PAYFMT_PBAPI,
						  methno,
						  service->descriptor->methods[methidx].rw_pri);
  rwmsg_request_set_signature(req, sig);
  rwmsg_signature_release(cc->ch.ep, sig);
#else
  rwmsg_priority_t pri = service->descriptor->methods[methidx].rw_pri;
  rwmsg_signature_t sig_s;
  rwmsg_signature_t *sig = &sig_s;
  sig->pri = pri;
  sig->payt = RWMSG_PAYFMT_PBAPI;
  sig->methno = methno;
  sig->timeo =  (RWMSG_BOUNCE_TIMEOUT_VAL/10000000); //centiseconds
  sig->refct = 1;
  rwmsg_request_set_signature(req, sig);
#endif
  rwmsg_request_set_callback(req, closure);

  if (flags&RWMSG_CLICHAN_PROTOC_FLAG_BLOCKING) {
    rs = rwmsg_clichan_send_blocking(cc, dest, req);
  } else {
    rs = rwmsg_clichan_send(cc, dest, req);
  }

  if (req_out) {
    if (rs == RW_STATUS_SUCCESS) {
      *req_out = req;
    }
  } 
  if (rs != RW_STATUS_SUCCESS) {
    /* Blocking case frees req next time through */
    if (!(flags&RWMSG_CLICHAN_PROTOC_FLAG_BLOCKING)) {
      rwmsg_request_release(req);
    }
  }

  return rs;
}


rw_status_t rwmsg_clichan_bind_rwsched_cfrunloop(rwmsg_clichan_t *cc, 
						 rwsched_tasklet_ptr_t tifo) {
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);
  return rwmsg_channel_bind_rwsched_cfrunloop(&cc->ch, tifo);
}


rw_status_t rwmsg_clichan_bind_rwsched_queue(rwmsg_clichan_t *cc, 
					     rwsched_dispatch_queue_t q) {
  RW_ASSERT(cc);
  RW_ASSERT_TYPE(cc, rwmsg_clichan_t);
  return rwmsg_channel_bind_rwsched_queue(&cc->ch, q);
}

rwmsg_endpoint_t *rwmsg_clichan_get_endpoint(rwmsg_clichan_t *cc) {
  rwmsg_endpoint_t *ep = cc->ch.ep;
  if (ep) {
    ck_pr_inc_32(&ep->refct);
  }
  return ep;
}
