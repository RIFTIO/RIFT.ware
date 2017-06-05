/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_srvchan.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG service channel
 */

#include "rwmsg_int.h"

rwmsg_srvchan_t *rwmsg_srvchan_create(rwmsg_endpoint_t *ep) {
  RW_ASSERT_TYPE(ep, rwmsg_endpoint_t);
  rwmsg_srvchan_t *sc = NULL;
  sc = (rwmsg_srvchan_t*)RW_MALLOC0_TYPE(sizeof(*sc), rwmsg_srvchan_t);
  RW_ASSERT((((uint64_t)sc) & ((1<<4)-1)) == 0); // 16 byte alignment, thank you

  rwmsg_channel_create(&sc->ch, ep, RWMSG_CHAN_SRV);
  return sc;
}

static void rwmsg_srvchan_recv_req(rwmsg_srvchan_t *sc,
				   rwmsg_request_t *req) {
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RW_ASSERT(!req->srvchan);
  req->srvchan = sc;
  _RWMSG_CH_DEBUG_(&sc->ch, "++");
  ck_pr_inc_32(&sc->ch.refct);

  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_recv_req" FMT_MSG_HDR(req->hdr), PRN_MSG_HDR(req->hdr));

  RW_ASSERT(sc->ch.chanid);
  req->hdr.stid = sc->ch.chanid;

  // method lookup and callback, keyed only on payt+methno, or for pb1 just payt+service glue
  rwmsg_method_t *meth = rwmsg_endpoint_find_method(sc->ch.ep,
						    req->hdr.pathhash,
						    req->hdr.payt,
						    req->hdr.methno);
  if (meth) {
    /*
    if (meth && meth->pathidx)
    fprintf(stderr, "sc->->ch.ep->paths[%u].path=%s hashlittle=0x%lu req->hdr.pathhash=0x%lu\n",
            meth->pathidx,
            sc->ch.ep->paths[meth->pathidx].path,
            rw_hashlittle64(sc->ch.ep->paths[meth->pathidx].path, strlen(sc->ch.ep->paths[meth->pathidx].path)),
            req->hdr.pathhash);
    */
    RW_ASSERT(rw_hashlittle64(sc->ch.ep->paths[meth->pathidx].path, strlen(sc->ch.ep->paths[meth->pathidx].path)) == req->hdr.pathhash);
    RW_ASSERT(meth->sig->methno == req->hdr.methno);
    RW_ASSERT(meth->sig->payt == req->hdr.payt);

    switch (sc->ch.notify.theme) {
    case RWMSG_NOTIFY_RWSCHED:
      req->rwq = sc->ch.notify.rwsched.rwqueue;
      break;
    case RWMSG_NOTIFY_CFRUNLOOP:
      req->rwq = rwsched_dispatch_get_main_queue(sc->ch.notify.cfrunloop.rwsched);
      break;
    default:
      req->rwq = NULL;		/* triggers inline response; caller (and responder!) had better be in our bound scheduler context! */
      break;
    }
    
    meth->cb.request(req, meth->cb.ud);
    req = NULL;
    
    rwmsg_method_release(sc->ch.ep, meth);
  } else {
    req->hdr.bnc = RWMSG_BOUNCE_NOMETH;
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "bnc=NOMETH", PRN_MSG_HDR(req->hdr));
#ifndef __RWMSG_BNC_RESPONSE_W_PAYLOAD
    rs = rwmsg_request_send_response(req, NULL, 0);
#else
    //rs = rwmsg_request_send_response(req, NULL, 0);
    rs = rwmsg_request_send_response(req, req->req.msg.nnbuf, req->req.msg.nnbuf_len);
#endif
    if (rs == RW_STATUS_BACKPRESSURE) {
      /* rats */
      rwmsg_request_release(req);
      /* stat */
    }
  }
}

/* Make up a request from a received rwmsg_buf_t */
rw_status_t rwmsg_srvchan_recv_buf(rwmsg_srvchan_t *sc,
				   rwmsg_buf_t *buf) {
  rw_status_t rs = RW_STATUS_FAILURE;

  rwmsg_request_t *req;
  req = rwmsg_request_create(NULL);
  rs = rwmsg_buf_unload_header(buf, &req->hdr);
  if (rs != RW_STATUS_SUCCESS) {
    /* stat dud header */
    rwmsg_request_release(req);
    goto out;
  }
  req->req.msg = *buf;
  req->local = 0;

  rwmsg_srvchan_recv_req(sc, req);

 out:
  return rs;
}

rw_status_t rwmsg_srvchan_pause(rwmsg_srvchan_t *sc) {
  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_pause()%s", "");
  sc->ch.paused_user = TRUE;
  rwmsg_channel_pause_ss(&sc->ch);
  rwmsg_channel_pause_q(&sc->ch);
  return RW_STATUS_SUCCESS;
}
rw_status_t rwmsg_srvchan_resume(rwmsg_srvchan_t *sc) {
  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_resume()%s", "");
  sc->ch.paused_user = FALSE;
  rwmsg_channel_resume_ss(&sc->ch);
  rwmsg_channel_resume_q(&sc->ch);
  return RW_STATUS_SUCCESS;
}  

/* Distressingly, some callers hold the RG lock when calling this.  So it should not block etc */
rw_status_t rwmsg_send_methbinding(rwmsg_channel_t *ch, rwmsg_priority_t pri, struct rwmsg_methbinding_key_s *k, const char *path) {
  struct rwmsg_srvchan_method_s mb_s = { };
  rwmsg_header_t hdr;
  rwmsg_message_t msg;
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(ch);
  RW_ASSERT(k);
  RW_ASSERT(path);

  if (!ch->ss) {
    RWMSG_TRACE_CHAN(ch, ERROR, "rwmsg_send_methbinding() ch->ss=%p", ch->ss);
    return rs;
  }

  mb_s.k = *k;
  strncpy(mb_s.path, path, sizeof(mb_s.path));

  RWMSG_TRACE_CHAN(ch, INFO, "Sending rwmsg_methbinding -pri=%d- phash=0x%lx payt %u methno %u path '%s'",
	      pri, mb_s.k.pathhash, mb_s.k.payt, mb_s.k.methno, mb_s.path);

  RW_ZERO_VARIABLE(&hdr);
  RW_ZERO_VARIABLE(&msg);

  hdr.isreq = TRUE;
  hdr.payt = RWMSG_PAYFMT_MSGCTL;
  hdr.pri = pri;
  hdr.id.broid = -1;
  hdr.id.chanid = ch->chanid;
  hdr.pathhash = 0;
  hdr.methno = RWMSG_MSGCTL_METHNO_METHBIND;
  hdr.paysz = 1;

  /* Gah, do a real payload */
  
  rwmsg_request_message_load(&msg, &mb_s, sizeof(mb_s));
  rwmsg_request_message_load_header(&msg, &hdr);

  rwmsg_request_t *req = rwmsg_request_create(NULL);

  req->hdr = hdr;
  if (ch->chantype == RWMSG_CHAN_SRV) {
    req->hdr.isreq = FALSE;
    req->rsp = msg;
    RWMSG_REQ_TRACK(req);
    rs = rwmsg_queue_enqueue(&ch->ssbufq, req);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rwmsg_sockset_pollout(ch->ss, pri, TRUE);
  } else {
    RW_ASSERT(ch->chantype == RWMSG_CHAN_PEERCLI);
    req->hdr.isreq = TRUE;
    req->req = msg;
    RWMSG_REQ_TRACK(req);
    rs = rwmsg_queue_enqueue(&ch->localq, req);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  rs = RW_STATUS_SUCCESS;
  return rs;
}


static void rwmsg_srvchan_broker_update(rwmsg_srvchan_t *sc) {
  if (!sc->ch.ss) {
    return;
  }
  struct rwmsg_srvchan_method_s *mb, *tmpmb;
  HASH_ITER(hh, sc->meth_hash, mb, tmpmb) {
    if (mb->dirty) {
      rw_status_t rs;
      rs = rwmsg_send_methbinding(&sc->ch, mb->meth->sig->pri, &(mb->k), (const char *)(mb->path));
      if (rs == RW_STATUS_SUCCESS) {
	RW_ASSERT(sc->dirtyct >= 1);
	sc->dirtyct--;
	mb->dirty = FALSE;
      } else {
	rwmsg_sockset_pollout(sc->ch.ss, mb->meth->sig->pri, TRUE);
	return;
      }
    }
  }
}

void rwmsg_srvchan_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_srvchan_t *sc = (rwmsg_srvchan_t*)ud;
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);

  if (sc->dirtyct) {
    rwmsg_srvchan_broker_update(sc);
  }
  rwmsg_channel_send_buffer_pri(&sc->ch, pri, TRUE, FALSE);

  /* flow control feedme callback to app? */

  return;
  ss=ss;
}


rw_status_t rwmsg_srvchan_bind_rwsched_queue(rwmsg_srvchan_t *sc, 
					     rwsched_dispatch_queue_t q) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  return rwmsg_channel_bind_rwsched_queue(&sc->ch, q);
}

rw_status_t rwmsg_srvchan_bind_rwsched_cfrunloop(rwmsg_srvchan_t *sc, 
						 rwsched_tasklet_ptr_t taskletinfo) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  return rwmsg_channel_bind_rwsched_cfrunloop(&sc->ch, taskletinfo);
}

void rwmsg_srvchan_destroy(rwmsg_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  if (!sc->ch.refct) {
    rwmsg_channel_destroy(&sc->ch);
    memset(sc, 0, sizeof(*sc));
    RW_FREE_TYPE(sc, rwmsg_srvchan_t);
  }
}

void rwmsg_srvchan_release(rwmsg_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  rwmsg_endpoint_t *ep = sc->ch.ep;

  bool zero=0;
  ck_pr_dec_32_zero(&sc->ch.refct, &zero);
  if (zero) {
    /* Please call rwmsg_srvchan_halt() first */
    RW_ASSERT(sc->unbound);
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_SRVCHAN, sc, NULL, NULL);
  }
  return;
}

void rwmsg_srvchan_halt(rwmsg_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);

  /* Global bindings in the endpoint */
  rwmsg_endpoint_del_channel_method_bindings(sc->ch.ep, &sc->ch);

  /* Local bindings hash */
  struct rwmsg_srvchan_method_s *mb, *tmpmb;
  HASH_ITER(hh, sc->meth_hash, mb, tmpmb) {
    HASH_DELETE(hh, sc->meth_hash, mb);
    rwmsg_method_release(sc->ch.ep, mb->meth);
    RW_FREE_TYPE(mb, struct rwmsg_srvchan_method_s);
  }

  sc->unbound = TRUE;

  rwmsg_channel_halt(&sc->ch);

  /* Flush buffered outgoing responses; bounce incoming local requests */
  rwmsg_request_t *tmpreq;
  while ((tmpreq = rwmsg_queue_dequeue(&sc->ch.localq))) {
    /* We have no bound methods, so these will all immediately bounce */
    RW_ASSERT_TYPE(tmpreq, rwmsg_request_t);
    rwmsg_srvchan_recv_req(sc, tmpreq);
  }
  if (sc->ch.ss) {
    while ((tmpreq = rwmsg_queue_dequeue(&sc->ch.ssbufq))) {
      RW_ASSERT_TYPE(tmpreq, rwmsg_request_t);
      rwmsg_request_release(tmpreq);
    }
  }

  _RWMSG_CH_DEBUG_(&sc->ch, "--");
  rwmsg_srvchan_release(sc);
  return;
}


rw_status_t rwmsg_srvchan_add_method(rwmsg_srvchan_t *sc, 
				     rwmsg_method_t *method) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);

  // add or update method binding to the endpoint, index by path/payt/methno, value srvchan[8]
  rw_status_t rs = rwmsg_endpoint_add_channel_method_binding(sc->ch.ep, &sc->ch, RWMSG_METHB_TYPE_SRVCHAN, method);

  // the below may need to go onto our serial queue as a workitem

  // local method binding hash
  struct rwmsg_srvchan_method_s *mb, *omb;
  mb = RW_MALLOC0_TYPE(sizeof(*mb), struct rwmsg_srvchan_method_s);
  mb->k.bindtype = RWMSG_METHB_TYPE_SRVCHAN;
  mb->k.pathhash = sc->ch.ep->paths[method->pathidx].pathhash;
  mb->k.payt = method->sig->payt;
  mb->k.methno = method->sig->methno;
  ck_pr_inc_32(&method->refct);
  omb=NULL;
  HASH_FIND(hh, sc->meth_hash, &mb->k, sizeof(mb->k), omb);
  if (!omb) {
    HASH_ADD(hh, sc->meth_hash, k, sizeof(mb->k), mb);
    mb->dirty = TRUE;
    sc->dirtyct++;
  } else {
    /* Already had it, update the method */
    RW_FREE_TYPE(mb, struct rwmsg_srvchan_method_s);
    rwmsg_method_release(sc->ch.ep, omb->meth);
    mb = omb;
    if (!mb->dirty) {
      mb->dirty = TRUE;
      sc->dirtyct++;
    }
  }
  mb->meth = method;
  strncpy(mb->path, sc->ch.ep->paths[method->pathidx].path, sizeof(mb->path)-1);
  mb->path[sizeof(mb->path)-1] = '\0';

  // publish method binding to broker...
  rwmsg_srvchan_broker_update(sc);
  
  return rs;
}

static rw_status_t rwmsg_srvchan_shuttlersp(ProtobufCMessage *rsp, void *ud) {
  rwmsg_request_t *req = (rwmsg_request_t*)ud;
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rw_status_t rs = rwmsg_request_send_response_pbapi(req, rsp);
  return rs;
}

static void rwmsg_srvchan_shuttlereq(rwmsg_request_t *req, void *ud) {
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  ProtobufCService *srv = (ProtobufCService *)ud;
  rwmsg_srvchan_t *sc = srv->rw_context;

  unsigned methno = req->hdr.methno; /* from rwmsg header, as would be
			     	        service number, to be looked
			     	        up among the service(s) bound
			     	        to the rwmsg_srvchannel */
  methno = (((1<<RWMSG_PBAPI_METHOD_BITS)-1) & methno);

  int methidx=-1; /* not methno! */

  /* lookup methidx from methno */
  unsigned int u;
  for (u=0; u<srv->descriptor->n_methods; u++) {
    if (srv->descriptor->methods[u].rw_methno == methno) {
      methidx = u;
      break;
    }
  }
  assert(methidx > -1);

  uint32_t buf_len=0;
  const uint8_t *buf = rwmsg_request_get_request_payload(req, &buf_len);

  /* The unpack calls _init first thing, so defaults do appear, and no
     pre-zeroing is needed.  It can handle working with a protobuf
     descriptor in isolation, although the normal case invokes a
     generated initializer function (pointed to by the protobuf C
     bindings' descriptor structure) rather than the data-driven
     generic initializer. */
  ProtobufCMessage *arrivingmsg;
  arrivingmsg = protobuf_c_message_unpack(&protobuf_c_default_instance, 
					  srv->descriptor->methods[methidx].input, 
					  buf_len, 
					  buf);
  if (arrivingmsg == NULL) {
    rw_status_t rs = RW_STATUS_FAILURE;
    req->hdr.bnc = RWMSG_BOUNCE_BADREQ;
    RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "req" FMT_MSG_HDR(req->hdr) "bnc=BADREQ", PRN_MSG_HDR(req->hdr));
    //rs = rwmsg_request_send_response(req, NULL, 0);
    rs = rwmsg_request_send_response(req, req->req.msg.nnbuf, req->req.msg.nnbuf_len);
    if (rs == RW_STATUS_BACKPRESSURE) {
      /* rats */
      rwmsg_request_release(req);
      /* stat */
    }
    return;
  }
  
  /* Find the service handler and invoke it with our generic closure
     callback.  Probably need a bit of glue or a bodge of some sort to
     provide a pleasing API to users instead of the callback argument
     stuff. Not least to get explicit error returns back. */
  typedef void (*GenericHandler) (void *service,
				  const ProtobufCMessage *input,
				  void *userctx,
				  ProtobufCClosure closure,
				  void *closure_data);
  GenericHandler *handlers = (GenericHandler *) (srv + 1);
  
  req->pbsrv = srv;
  req->methidx = methidx;

  handlers[methidx](srv, arrivingmsg, srv->rw_usercontext, (ProtobufCClosure)rwmsg_srvchan_shuttlersp, req);

  protobuf_c_message_free_unpacked(&protobuf_c_default_instance, arrivingmsg);

  return;
}

rw_status_t rwmsg_srvchan_add_service(rwmsg_srvchan_t *sc,
				      const char *path,
				      ProtobufCService *srv,
				      void *ud) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  rw_status_t rs = RW_STATUS_FAILURE;

  RW_ASSERT(srv);
  RW_ASSERT(srv->descriptor);
  RW_ASSERT(srv->descriptor->name);
  RWMSG_TRACE_CHAN(&sc->ch, INFO, "add pbapi service '%s'", srv->descriptor->name);

  RW_ASSERT(!srv->rw_context);
  srv->rw_context = sc;
  srv->rw_usercontext = ud;

  rwmsg_closure_t clo = { .request = rwmsg_srvchan_shuttlereq, .ud = srv };

  unsigned int midx=0;
  for (midx=0; midx<srv->descriptor->n_methods; midx++) {
    RW_ASSERT(srv->descriptor->rw_srvno <= RWMSG_PBAPI_SERVICE_MAX);
    RW_ASSERT(srv->descriptor->methods[midx].rw_methno <= RWMSG_PBAPI_METHOD_MAX);
    uint32_t methno = RWMSG_PBAPI_METHOD(srv->descriptor->rw_srvno, srv->descriptor->methods[midx].rw_methno);
    rwmsg_signature_t *sig = rwmsg_signature_create(sc->ch.ep,
						    RWMSG_PAYFMT_PBAPI,
						    methno,
						    srv->descriptor->methods[midx].rw_pri);
    rwmsg_method_t *meth = rwmsg_method_create(sc->ch.ep,
					       path,
					       sig,
					       &clo);
    RW_ASSERT(meth);
    RW_ASSERT(meth->pathhash);
    meth->pbmethidx = midx;
    meth->pbsrv = srv;
					      
    rs = rwmsg_srvchan_add_method(sc, meth);
    if (rs != RW_STATUS_SUCCESS) {
      break;
    }

    /* Implicitly added methods and signatures need to go away
       automagically when removed from the ep's index; we keep no
       reference here at present. */
    rwmsg_method_release(sc->ch.ep, meth);
    rwmsg_signature_release(sc->ch.ep, sig);
  }

  return rs;
}

/*
 * Send a response to a request received on a srvchan.  Internal use only (?)
 */
rw_status_t rwmsg_srvchan_send(rwmsg_srvchan_t *sc, 
			       rwmsg_request_t *req) {
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RW_ASSERT(req->srvchan);
  RW_ASSERT(req->srvchan == sc);
  RW_ASSERT(req->rsp.msg.nnbuf); /* payload has been installed */

  RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_send response for req" FMT_MSG_HDR(req->hdr) "to %s", 
                   PRN_MSG_HDR(req->hdr),
                   (req->clichan ? "to local channel" : "to broker"));

  if (req->clichan) {
    /* local */
    rwmsg_srvchan_t *sc = req->srvchan;
    if (req->srvchan) {
      _RWMSG_CH_DEBUG_(&req->srvchan->ch, "--");
      rwmsg_srvchan_release(req->srvchan);
      req->srvchan = NULL;
    }
    if (req->hdr.blocking) {
      rwmsg_notify_raise(&req->clichan->blk.notify);
      rs = RW_STATUS_SUCCESS;
    } else {
      RWMSG_REQ_TRACK(req);
      rs = rwmsg_queue_enqueue(&req->clichan->ch.localq, req);
      if (rs == RW_STATUS_BACKPRESSURE) {
        _RWMSG_CH_DEBUG_(&sc->ch, "++");
	ck_pr_inc_32(&sc->ch.refct);
	req->srvchan = sc;
      }
    }
  } else {
    /* send out any buffered requests first */
    rs = rwmsg_channel_send_buffer_pri(&sc->ch, req->hdr.pri, TRUE, FALSE);
    if (rs != RW_STATUS_SUCCESS) {
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_send buffering req" FMT_MSG_HDR(req->hdr) "n ssbufq due to channel_send_buffer rs=%d",
                       PRN_MSG_HDR(req->hdr), rs);
      goto queuereq;
    }

    /* send out and free this req */
    RWMSG_REQ_TRACK(req);
    rs = rwmsg_sockset_send(sc->ch.ss, req->hdr.pri, &req->rsp.msg);
    if (rs == RW_STATUS_SUCCESS) {
      rwmsg_request_release(req);
      req = NULL;
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      /* pause reads until we get a sendme callback and can send this out */
      rwmsg_sockset_pause_pri(sc->ch.ss, req->hdr.pri);
      RWMSG_TRACE_CHAN(&sc->ch, DEBUG, "rwmsg_srvchan_send buffering req" FMT_MSG_HDR(req->hdr) "in ssbufq due to backpressure", 
                       PRN_MSG_HDR(req->hdr));
    queuereq:
      RWMSG_REQ_TRACK(req);
      rs = rwmsg_queue_enqueue(&sc->ch.ssbufq, req);
      rwmsg_sockset_pollout(sc->ch.ss, req->hdr.pri, TRUE);
      goto out;
    }
  }
 out:
  return rs;
}

uint32_t rwmsg_srvchan_recv(rwmsg_srvchan_t *sc) {
  RW_ASSERT(sc);
  RW_ASSERT_TYPE(sc, rwmsg_srvchan_t);
  uint32_t rval = 0;
  
  int max=1;
  while (max--) {
    rwmsg_request_t *req;
    req = rwmsg_queue_dequeue(&sc->ch.localq);
    if (!req) {
      if (sc->ch.ss) {
	rw_status_t rs = RW_STATUS_FAILURE;
	rwmsg_buf_t buf = { .nnbuf = NULL, .nnbuf_len = 0 };
	rwmsg_priority_t pri;
	rs = rwmsg_sockset_recv(sc->ch.ss, &buf, &pri);
	if (rs == RW_STATUS_SUCCESS) {
	  rwmsg_srvchan_recv_buf(sc, &buf);
	  req++;
	  break;
	}
      }
    } else {
      RW_ASSERT_TYPE(req, rwmsg_request_t);
      rwmsg_srvchan_recv_req(sc, req);
      rval++;
    }
  }

  return rval;
}

