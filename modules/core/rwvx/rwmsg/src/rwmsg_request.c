
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_request.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG request object
 */

#include "rwmsg_int.h"

static const char*
rwmsg_request_bounce_to_str(rwmsg_bounce_t bnc)
{
  switch(bnc) {
    case RWMSG_BOUNCE_OK:
      return "RWMSG_BOUNCE_OK";
    case RWMSG_BOUNCE_NODEST:
      return "RWMSG_BOUNCE_NODEST";
    case RWMSG_BOUNCE_NOMETH:
      return "RWMSG_BOUNCE_NOMETH";
    case RWMSG_BOUNCE_NOPEER:
      return "RWMSG_BOUNCE_NOPEER";
    case RWMSG_BOUNCE_BROKERR:
      return "RWMSG_BOUNCE_BROKERR";
    case RWMSG_BOUNCE_TIMEOUT:
      return "RWMSG_BOUNCE_TIMEOUT";
    case RWMSG_BOUNCE_RESET:
      return "RWMSG_BOUNCE_RESET";
    case RWMSG_BOUNCE_SRVRST:
      return "RWMSG_BOUNCE_SRVRST";
    case RWMSG_BOUNCE_TERM:
      return "RWMSG_BOUNCE_TERM";
    case RWMSG_BOUNCE_BADREQ:
      return "RWMSG_BOUNCE_BADREQ";
    default:
      return "??UNK_BOUNCE_??";
  }
}

rwmsg_request_t *rwmsg_request_create(rwmsg_clichan_t *cc) {
  rwmsg_channel_t *ch = (rwmsg_channel_t*)cc;

  rwmsg_request_t *req = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*req), rwmsg_request_t);
  req->refct = 1;

  req->hdr.isreq = TRUE;

#ifdef RWMSG_REQUEST_LEAK_TRACKING
  rw_btrace_backtrace(req->callers, RW_RESOURCE_TRACK_MAX_CALLERS);
  req->callers[0] = cc;
  RWMSG_RG_LOCK();
  RW_DL_INSERT_TAIL(&rwmsg_global.track.requests, req, trackelem);
  RWMSG_RG_UNLOCK();
#endif

  ck_pr_inc_32(&rwmsg_global.status.request_ct);

  /* This and the sc reference are awkward and should be eliminated in
     the broker case. */
  req->clichan = cc;
  if (ch) {
    RW_ASSERT(ch->chantype == RWMSG_CHAN_CLI || ch->chantype == RWMSG_CHAN_BROCLI || ch->chantype == RWMSG_CHAN_PEERCLI);
    _RWMSG_CH_DEBUG_(ch, "++");
    ck_pr_inc_32(&ch->refct);
  }
  RWMSG_REQ_TRACK(req);

  return req;
}

void rwmsg_request_set_signature(rwmsg_request_t *req,
				 rwmsg_signature_t *sig) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  req->hdr.pri = sig->pri;
  req->hdr.payt = sig->payt;
  req->hdr.methno = sig->methno;
  req->hdr.timeo = sig->timeo;
}

void rwmsg_request_set_callback(rwmsg_request_t *req,
				rwmsg_closure_t *cb) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  if (cb) {
    req->cb = *cb;
  }
}


uint32_t rwmsg_request_message_load(rwmsg_message_t *m,
				    const void *ptr, 
				    uint32_t len) {
  uint32_t sz = RWMSG_HEADER_WIRESIZE + len;
  RW_ASSERT(!m->msg.nnbuf);
  m->msg.nnbuf = nn_allocmsg(sz, 0);
  m->msg.nnbuf_len = sz;
  m->msg.nnbuf_nnmsg = TRUE;
  if (len) {
    memcpy(m->msg.nnbuf+RWMSG_HEADER_WIRESIZE, ptr, len);
  }
  return sz;
}

void rwmsg_request_message_load_header(rwmsg_message_t *m,
				       rwmsg_header_t *hdr) {
  RW_ASSERT(m->msg.nnbuf);
#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
  if (hdr->bnc)
    hdr->paysz = 0;
#else
  hdr->paysz = m->msg.nnbuf_len - RWMSG_HEADER_WIRESIZE;
#endif
  memcpy(m->msg.nnbuf, hdr, sizeof(*hdr));
}

rw_status_t rwmsg_buf_unload_header(rwmsg_buf_t *m, 
				    rwmsg_header_t *hdr_out) {
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(m->nnbuf);
  if (m->nnbuf_len < RWMSG_HEADER_WIRESIZE) {
    goto out;
  }
  RW_STATIC_ASSERT(RWMSG_HEADER_WIRESIZE == sizeof(*hdr_out));
  memcpy(hdr_out, m->nnbuf, sizeof(*hdr_out));
#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
  if (hdr_out->bnc)
    hdr_out->paysz = 0;
#endif
  rs = RW_STATUS_SUCCESS;
 out:
  return rs;
}


void rwmsg_request_message_free(rwmsg_message_t *m) {
  if (m->msg.nnbuf) {
    nn_freemsg(m->msg.nnbuf);
    m->msg.nnbuf = NULL;
    m->msg.nnbuf_len = 0;
  }
}

static void rwmsg_request_send_response_f(void *ud) {
  rwmsg_request_t *req = (rwmsg_request_t *)ud;

  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(req->srvchan);

  if (req->srvchan->ch.halted) {
    goto toss;
  }

  req->hdr.isreq = FALSE;
  rwmsg_request_message_load_header(&req->rsp, &req->hdr);
  rs = rwmsg_srvchan_send(req->srvchan, req);
  if (rs != RW_STATUS_SUCCESS) {
    /* Doh.  We are no longer the caller, so we've got to eat this response somehow... */
    req->hdr.isreq = TRUE;
    if (rs == RW_STATUS_BACKPRESSURE
	|| rs == RW_STATUS_NOTCONNECTED) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 100000ull);   // tenth of a ms
      rwsched_dispatch_after_f(req->srvchan->ch.ep->taskletinfo,
			       when,
			       req->rwq,
			       req,
			       rwmsg_request_send_response_f);
    } else {
      /* ?? Final rs is from rwmsg_queue_enqueue, which succeeds or backpressures. */
      goto toss;
    }
  }
  return;

 toss:
  rwmsg_request_release(req);
  req = NULL;
  return;
}
rw_status_t rwmsg_request_send_response(rwmsg_request_t *req,
					const void *payload,
					uint32_t payload_len) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rw_status_t rs = RW_STATUS_FAILURE;
  RW_ASSERT(req->hdr.isreq);
  RW_ASSERT(req->srvchan);

  rwmsg_endpoint_t *ep = req->srvchan->ch.ep;

  bool isbroker = rwmsg_chan_is_brotype(&req->srvchan->ch);

  rwmsg_request_message_load(&req->rsp, payload, payload_len);

  if (!isbroker /* FIXME */
      && req->rwq) {
    if (req->hdr.blocking && req->clichan) {
      /* Local blocking request, ensuring that we execute the response
	 on the mainq/CF context is awkward due to our tasklet
	 blocking implementation.  It shouldn't be, one tasklet should
	 be blocking while the other's mainq/CF runs unfettered.  The
	 mainq is not taskletized, it blocks when any tasklet is
	 blocking, so our answer computation won't make progress
	 unless it's on the CFRunLoop */
      rwsched_dispatch_queue_t q = rwsched_dispatch_get_main_queue(ep->rwsched);
      RW_ASSERT(req->rwq == q);
      if (req->rwq == q) {
	/* FIXME: mainq is not taskletized, just answer directly from
	   this thread, Dog help us if anyone answers from a different
	   thread/queue!  Maybe use a PerformBlock or 0s timer to run
	   in the right context?  */
	goto imm;
      }
    }

    /* Possibly we are in a different rwq than the srvchan runs in.
       This can't help performance, we really need some API change up
       at binding time to make this better, by arranging not to set
       req->rwq when the responses will all be issued from the bound
       rwsched context. */
    rwsched_dispatch_async_f(ep->taskletinfo,
			     req->rwq,
			     req,
			     rwmsg_request_send_response_f);
    rs = RW_STATUS_SUCCESS;
  } else {
  imm:
    /* Channel not bound to a queue */
    req->hdr.isreq = FALSE;
    rwmsg_request_message_load_header(&req->rsp, &req->hdr);
    rs = rwmsg_srvchan_send(req->srvchan, req);
    if (rs != RW_STATUS_SUCCESS) {
      /* TBD: If it's EAGAIN we are supposed to buffer this and stall
	 the inbound request queue until this buffered req gets out */
      
      /* Oops, undo so caller can try again later. */
      req->hdr.isreq=TRUE;
      rwmsg_request_message_free(&req->rsp);
      
    }
  }

  return rs;
}


rw_status_t rwmsg_request_send_response_pbapi(rwmsg_request_t *req,
					      const ProtobufCMessage *rsp) {
  uint8_t pkbuf[512];
  uint8_t *tmpbuf = NULL;
  uint8_t *buf = pkbuf;
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  size_t size = protobuf_c_message_get_packed_size(NULL, rsp);
  if (size > sizeof(pkbuf)) {
    buf = tmpbuf = (uint8_t*)malloc(size);
  }
  RW_ASSERT(rsp->descriptor == req->pbsrv->descriptor->methods[req->methidx].output);
  size_t sz = protobuf_c_message_pack(NULL, rsp, buf);
  RW_ASSERT(sz == size);
  rw_status_t rs = rwmsg_request_send_response(req, buf, size);
  if (tmpbuf) {
    free(tmpbuf);
  }
  return rs;
}


rw_status_t rwmsg_request_set_payload(rwmsg_request_t *req,
				      const void *payload,
				      uint32_t payload_len) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rwmsg_request_message_load(&req->req, payload, payload_len);
  return RW_STATUS_SUCCESS;
}

struct rwmsg_transient_val_s {
  uint8_t *buf;
  uint32_t buf_len;

  rwmsg_payfmt_t fmt;		/* union discriminator */
  union {
    struct {
      ProtobufCMessage *msg;
#define TRANSIENT_SMBUF_SZ (2048)
      char smbuf[TRANSIENT_SMBUF_SZ];
      uint32_t heremsg:1;
    } pbapi;
  };
  // do we need any other context?
};
static __thread struct {
  struct rwmsg_transient_val_s req;
  struct rwmsg_transient_val_s rsp;
} thrval;

rw_status_t rwmsg_request_get_response(rwmsg_request_t *req, const void **bufout) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  rw_status_t rs = RW_STATUS_FAILURE;

  uint32_t len;
  const uint8_t *buf;
  buf = rwmsg_request_get_response_payload(req, &len);
  
  if (buf) {
    /* Free the previous rsp */
    switch (thrval.rsp.fmt) {
    case RWMSG_PAYFMT_PBAPI:
      if (thrval.rsp.pbapi.msg) {
	if (thrval.rsp.pbapi.heremsg) {
	  if (!(thrval.rsp.pbapi.msg->descriptor->rw_flags & RW_PROTOBUF_MOPT_FLAT)) {
	    protobuf_c_message_free_unpacked_usebody(&protobuf_c_default_instance, thrval.rsp.pbapi.msg);
	  }
	} else {
	  protobuf_c_message_free_unpacked(&protobuf_c_default_instance, thrval.rsp.pbapi.msg);
	}
	thrval.rsp.pbapi.msg = NULL;
      }
      break;
    default:
      break;
    }

    /* Figure out the response */
    switch (req->hdr.payt) {
    default:
      *bufout = buf;
      rs = RW_STATUS_SUCCESS;
      break;
      
    case RWMSG_PAYFMT_PBAPI:
      thrval.rsp.fmt = RWMSG_PAYFMT_PBAPI;
      if (req->pbcli->descriptor->methods[req->methidx].output->sizeof_message > TRANSIENT_SMBUF_SZ) {
	thrval.rsp.pbapi.heremsg = FALSE;
	thrval.rsp.pbapi.msg = protobuf_c_message_unpack(&protobuf_c_default_instance, 
							 req->pbcli->descriptor->methods[req->methidx].output, 
							 len, 
							 buf);
      } else {
	thrval.rsp.pbapi.heremsg = TRUE;
  protobuf_c_message_init(req->pbcli->descriptor->methods[req->methidx].output, (ProtobufCMessage*)&thrval.rsp.pbapi.smbuf);
	thrval.rsp.pbapi.msg = protobuf_c_message_unpack_usebody(&protobuf_c_default_instance, 
								 req->pbcli->descriptor->methods[req->methidx].output, 
								 len, 
								 buf,
								 (ProtobufCMessage*)&thrval.rsp.pbapi.smbuf);
      }
      if (thrval.rsp.pbapi.msg) {
	*bufout = thrval.rsp.pbapi.msg;
	rs = RW_STATUS_SUCCESS;
      }
      break;
    }
  }

  return rs;
}


const void *rwmsg_request_get_request_payload(rwmsg_request_t *req, uint32_t *len_out) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  if (!req->req.msg.nnbuf) {
    return NULL;
  }

  if (len_out) {
    *len_out = (req->req.msg.nnbuf_len - RWMSG_HEADER_WIRESIZE);
  }
  return (void*)(req->req.msg.nnbuf + RWMSG_HEADER_WIRESIZE);

  thrval.req.fmt=thrval.req.fmt;
}

const void *rwmsg_request_get_response_payload(rwmsg_request_t *req, uint32_t *len_out) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  if (!req->rsp.msg.nnbuf || req->hdr.bnc) {
    return NULL;
  }
  if (len_out) {
    *len_out = (req->rsp.msg.nnbuf_len - RWMSG_HEADER_WIRESIZE);
  }
  return (void*)(req->rsp.msg.nnbuf + RWMSG_HEADER_WIRESIZE);
}

#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
const void *rwmsg_request_get_bnc_response_payload(rwmsg_request_t *req, uint32_t *len_out) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  if (!req->rsp.msg.nnbuf) {
    return NULL;
  }
  if (len_out) {
    *len_out = (req->rsp.msg.nnbuf_len - RWMSG_HEADER_WIRESIZE*2);
  }
  return (void*)(req->rsp.msg.nnbuf + RWMSG_HEADER_WIRESIZE*2);
}
#endif

rwmsg_payfmt_t rwmsg_request_get_response_payfmt(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  return req->hdr.payt;
}

rw_status_t rwmsg_request_get_request_client_id(rwmsg_request_t *req, rwmsg_request_client_id_t *cliid) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  RW_ASSERT(cliid);

  memset(cliid, 0, sizeof(*cliid));
  if (req->bch_id) {
    cliid->_something_else = req->bch_id;
  } else if (req->local) {
    RW_ASSERT(req->clichan);
    cliid->_something_local = (uint64_t)req->clichan;
  } else {
    //!! not nonzero?    RW_ASSERT(req->hdr.id.broid);  /* nonzero? */
    RW_ASSERT(req->hdr.id.chanid); /* nonzero? */
    uint32_t id = 0;
    id = (req->hdr.id.broid << 18) | (req->hdr.id.chanid);
    cliid->_something_else = id;
  }
  cliid->_something_magic = RWMSG_REQUEST_CLIENT_ID_MAGIC;
  return RW_STATUS_SUCCESS;
}

rwmsg_flowid_t rwmsg_request_get_response_flowid(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  return req->hdr.stid;
}

rwmsg_bounce_t rwmsg_request_get_response_bounce(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  return req->hdr.bnc;
}

uint64_t rwmsg_request_get_response_rtt(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);
  return req->rtt.tv_usec + req->rtt.tv_sec * 1000000;
}

rw_status_t rwmsg_request_cancel(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  RW_ASSERT(req->clichan);
  RW_ASSERT_TYPE(req->clichan, rwmsg_clichan_t);

  memset(&req->cb, 0, sizeof(req->cb));

  return RW_STATUS_SUCCESS;
}

rwmsg_bool_t rwmsg_request_release(rwmsg_request_t *req) {
  RW_ASSERT(req);
  RW_ASSERT_TYPE(req, rwmsg_request_t);

  RW_ASSERT(req->refct >= 1);

  bool zero=0;
  ck_pr_dec_32_zero(&req->refct, &zero);
  if (!zero) {
    /* We do not mess around here with a deferred destructor to avoid
       all possible races.  All refct++ lives in the client thread at
       creation time. */
    return FALSE;
  }

  RW_ASSERT(!req->inhash);

  rwmsg_endpoint_t *ep = NULL;
  if (req->clichan) {
    ep = req->clichan->ch.ep;
  } else if (req->srvchan) {
    ep = req->srvchan->ch.ep;
  }

  if (ep) {
    RWMSG_EP_LOCK(ep);
    if (ep->lastreq == req) {
      ep->lastreq = NULL;
    }
    RWMSG_EP_UNLOCK(ep);
  }

  rwmsg_request_message_free(&req->req);
  rwmsg_request_message_free(&req->rsp);
  if (req->rwml_buffer) {
    rwmsg_request_memlog_hdr (&req->rwml_buffer, 
                              req, 
                              __PRETTY_FUNCTION__, 
                              __LINE__, 
                              "(Req Released)");
    rwmemlog_buffer_return_all(req->rwml_buffer);
    req->rwml_buffer = NULL;
  }


  /* It would be nice not to hold these references in the request object */
  if (req->clichan) {
    rwmsg_channel_release(&req->clichan->ch);
    req->clichan = NULL;
  }
  if (req->srvchan) {
    rwmsg_channel_release(&req->srvchan->ch);
    req->srvchan = NULL;
  }
  if (req->dest) {
    rwmsg_destination_release(req->dest);
    req->dest = NULL;
  }


#ifdef RWMSG_REQUEST_LEAK_TRACKING
  RWMSG_RG_LOCK();
  RW_DL_REMOVE(&rwmsg_global.track.requests, req, trackelem);
  RWMSG_RG_UNLOCK();
#endif

#ifndef NDEBUG
  memset(req, 0x0c, sizeof(*req));
#endif
  RW_FREE_TYPE(req, rwmsg_request_t);

  ck_pr_dec_32(&rwmsg_global.status.request_ct);

  return TRUE;
}

#ifdef RWMSG_REQUEST_LEAK_TRACKING
void rwmsg_request_dump() {
  rwmsg_request_t *req = NULL;
  while ((req = req ?
          RW_DL_NEXT(req, rwmsg_request_t, trackelem) :
          RW_DL_HEAD(&rwmsg_global.track.requests, rwmsg_request_t, trackelem))) {
    RWMSG_PRINTF("%u %p\n", ++i, req);
  }
}
#endif

void rwmsg_request_memlog_hdr(rwmemlog_buffer_t **rwml_buffer,
                              rwmsg_request_t    *req,
                              const char         *func_name,
                              const int          line,
                              const char         *string)
{
  RW_ASSERT(rwml_buffer);
  if (req) {
    RWMEMLOG(rwml_buffer,
	     RWMEMLOG_MEM0,
       "RWMsg HDR",
       RWMEMLOG_ARG_STRNCPY(50, func_name),
       RWMEMLOG_ARG_PRINTF_INTPTR("%"PRIdPTR, line),
       RWMEMLOG_ARG_PRINTF_INTPTR("broid %"PRIdPTR, req->hdr.id.broid),
       RWMEMLOG_ARG_PRINTF_INTPTR("chanid %"PRIdPTR, req->hdr.id.chanid),
       RWMEMLOG_ARG_PRINTF_INTPTR("locid %"PRIdPTR, req->hdr.id.locid),
       RWMEMLOG_ARG_ENUM_FUNC(rwmsg_request_bounce_to_str, "", req->hdr.bnc)); 
  }
}
