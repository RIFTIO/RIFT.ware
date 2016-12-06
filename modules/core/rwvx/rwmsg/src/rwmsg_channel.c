
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */


/**
 * @file rwmsg_channel.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG channel base object
 */

#include "rwmsg_int.h"

static struct rwmsg_channel_funcs functab[RWMSG_CHAN_CT] = {
  { /* NULL */ 0, 0, 0, 0, 0 },
  { RWMSG_CHAN_CLI,
    NULL,
    (void(*)(rwmsg_channel_t*))rwmsg_clichan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_clichan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_clichan_recv_buf,
    rwmsg_clichan_sockset_event_send,
  },
  { RWMSG_CHAN_SRV,
    NULL,
    (void(*)(rwmsg_channel_t*))rwmsg_srvchan_release,
    (uint32_t (*)(rwmsg_channel_t *ch)) rwmsg_srvchan_recv, 
    (rw_status_t (*)(rwmsg_channel_t *cc, rwmsg_buf_t *buf)) rwmsg_srvchan_recv_buf,
    rwmsg_srvchan_sockset_event_send,
  },
  //brosrv
  //brocli
  //bropeer
};

void rwmsg_channel_load_funcs(struct rwmsg_channel_funcs *funcs, int ct) {
  int i;
  for (i=0; i<ct; i++) {
    int idx = funcs[i].chtype;
    RW_ASSERT(idx < (int)(sizeof(functab) / sizeof(functab[0])));
    memcpy(&functab[idx], &funcs[i], sizeof(functab[0]));
  }
}

void rwmsg_channel_release(rwmsg_channel_t *ch) {
  functab[ch->chantype].release(ch);
}

static void rwmsg_channel_sockset_event_send(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_channel_t *ch = (rwmsg_channel_t*)ud;

  if (!ch->halted) {
    ch->vptr->ss_send(ss, pri, ud);
  }
}

static void rwmsg_channel_sockset_event_recv(rwmsg_sockset_t *ss, rwmsg_priority_t pri, void *ud) {
  rwmsg_channel_t *ch = (rwmsg_channel_t*)ud;

  if (!ch->halted) {
    if (ch->paused_ss) {
      rwmsg_sockset_pause_pri(ss, pri);
      goto ret;
    }

    rw_status_t rs = RW_STATUS_FAILURE;
    rwmsg_buf_t buf = { .nnbuf = NULL, .nnbuf_len = 0 };
    rs = rwmsg_sockset_recv_pri(ss, pri, &buf);
    if (rs == RW_STATUS_SUCCESS) {
      if (ch->chantype != RWMSG_CHAN_PEERSRV && pri == RWMSG_PRIORITY_BLOCKING) {
	/* No can do, toss */
  RWMSG_TRACE_CHAN(ch, INFO, "rwmsg_channel_sockset_event_recv: spurious blocking recv %d", ch->chantype);
	nn_freemsg(buf.nnbuf);
	buf.nnbuf = NULL;
      } else {
	ch->chanstat.msg_in_sock++;
	rs = ch->vptr->recv_buf(ch, &buf);
      }
    }
  }

 ret:
  return;
}

void rwmsg_channel_resume_ss(rwmsg_channel_t *ch) {
  if (ch->paused_ss && !ch->halted) {
    int pri;
    ch->paused_ss = FALSE;
    for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
      rwmsg_sockset_resume_pri(ch->ss, pri);
    }
  }
}


void rwmsg_channel_resume_q(rwmsg_channel_t *ch) {
  if (ch->paused_q && !ch->halted) {
    ch->paused_q = FALSE;
    ch->tickle=1;
    rwmsg_queue_resume(ch->ep, &ch->localq);
  }
}

void rwmsg_channel_pause_ss(rwmsg_channel_t *ch) {
  ch->paused_ss = TRUE;
}
void rwmsg_channel_pause_q(rwmsg_channel_t *ch) {
  ch->paused_q = TRUE;
}

static void rwmsg_channel_local_event(rwmsg_channel_t *ch, int bits) {
  uint32_t max = 10, rct=0;
  
  RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_channel_local_event(ch=%p,bits=%u) ch.paused_q=%u .halted=%u .tickle=%u .qlencap=%u", 
		   ch, bits, ch->paused_q, ch->halted, ch->tickle, ch->localq.qlencap);
  
  if (ch->paused_q) {
    rwmsg_queue_pause(ch->ep, &ch->localq);
    goto ret;
  }

  if (!ch->halted) {
    if (!ch->tickle) {
      ch->tickle=1;
      max = ch->localq.qlencap + 1;
    }
    do {
      rct = ch->vptr->recv(ch);
    } while (rct && max-- && !ch->paused_q);
  }
 ret:
  return;
  bits=bits;
}



rw_status_t rwmsg_channel_bind_rwsched_queue(rwmsg_channel_t *ch,
					     rwsched_dispatch_queue_t q) {
  RW_ASSERT(ch);
  RW_ASSERT(ch->notify.theme == RWMSG_NOTIFY_INVALID);

  RWMSG_TRACE_CHAN(ch, DEBUG, "channel_bind_rwsched_queue(q=%p)", q);

  rw_status_t rs = RW_STATUS_FAILURE;

  ch->taskletinfo = NULL;

  if (q) {
    ch->notify.theme = RWMSG_NOTIFY_RWSCHED;
    ch->notify.rwsched.rwsched = ch->ep->rwsched;
    ch->notify.rwsched.rwqueue = q;
    RW_ASSERT(ch->notify.rwsched.rwsource == NULL);
    
    rwmsg_closure_t clo = { .notify = (void(*)(void*, int))rwmsg_channel_local_event, .ud = ch };
    rs = rwmsg_notify_init(ch->ep, &ch->notify, "ch->notify (localq, RWS)", &clo);
    if (rs != RW_STATUS_SUCCESS) {
      memset(&ch->notify, 0, sizeof(ch->notify));
      goto ret;
    }
    rwmsg_queue_set_notify(ch->ep, &ch->localq, &ch->notify);

    if (ch->ss) {
      rwmsg_sockset_closure_t ssclo = {
	.recvme = rwmsg_channel_sockset_event_recv,
	.sendme = rwmsg_channel_sockset_event_send,
	.ud = ch
      };
      rwmsg_sockset_bind_rwsched_queue(ch->ss, q, &ssclo);
    }

  } else {
    rwmsg_queue_set_notify(ch->ep, &ch->localq, NULL);
    rwmsg_notify_deinit(ch->ep, &ch->notify);
    rwmsg_sockset_bind_rwsched_queue(ch->ss, NULL, NULL);
    ch->tickle=0;
  }

 ret:
  return rs;
}

rw_status_t rwmsg_channel_bind_rwsched_cfrunloop(rwmsg_channel_t *ch, 
						 rwsched_tasklet_ptr_t taskletinfo) {
  RW_ASSERT(ch);
  RW_ASSERT(ch->notify.theme == RWMSG_NOTIFY_INVALID);

  RWMSG_TRACE_CHAN(ch, DEBUG, "channel_bind_rwsched_cfrunloop(taskletinfo=%p)", taskletinfo);

  rw_status_t rs = RW_STATUS_FAILURE;

  ch->taskletinfo = taskletinfo;

  if (taskletinfo) {
    ch->notify.theme = RWMSG_NOTIFY_CFRUNLOOP;
    ch->notify.cfrunloop.rwsched = ch->ep->rwsched;
    ch->notify.cfrunloop.taskletinfo = taskletinfo;
    
    rwmsg_closure_t clo = { .notify = (void(*)(void*, int))rwmsg_channel_local_event, .ud = ch };
    rs = rwmsg_notify_init(ch->ep, &ch->notify, "ch->notify (localq, CF)", &clo);
    if (rs != RW_STATUS_SUCCESS) {
      memset(&ch->notify, 0, sizeof(ch->notify));
      goto ret;
    }
    rwmsg_queue_set_notify(ch->ep, &ch->localq, &ch->notify);
    
    if (ch->ss) {
      rwmsg_sockset_closure_t ssclo = {
	.recvme = rwmsg_channel_sockset_event_recv,
	.sendme = rwmsg_channel_sockset_event_send,
	.ud = ch
      };
      rwmsg_sockset_bind_rwsched_cfrunloop(ch->ss, taskletinfo, &ssclo);
    }

  } else {
    rwmsg_queue_set_notify(ch->ep, &ch->localq, NULL);
    rwmsg_notify_deinit(ch->ep, &ch->notify);
    rwmsg_sockset_bind_rwsched_cfrunloop(ch->ss, NULL, NULL);
    ch->tickle=0;
  }

 ret:
  return rs;
}

void rwmsg_channelspecific_halt(rwmsg_channel_t *ch) {
  functab[ch->chantype].halt(ch);
}

void rwmsg_channel_halt(rwmsg_channel_t *ch) {
  if (!ch->halted) {
    //RWMSG_TRACE_CHAN(ch, INFO, "rwmsg_channel_halt(%p)", ch);
    ch->halted=1;
    if (ch->ss) {
      rwmsg_sockset_close(ch->ss);
    }
    rwmsg_notify_resume(ch->ep, &ch->notify);
    rwmsg_notify_deinit(ch->ep, &ch->notify);
    rwmsg_queue_set_notify(ch->ep, &ch->localq, NULL);
  }
}

void rwmsg_channel_destroy(rwmsg_channel_t *ch) {
  if (!ch->refct) {
    RWMSG_TRACE_CHAN(ch, INFO, "rwmsg_channel_destroy()%s", "");

    RW_ASSERT(ch->halted);
    rwmsg_queue_deinit(ch->ep, &ch->localq);
    if (ch->ss) {
      rwmsg_queue_deinit(ch->ep, &ch->ssbufq);
      ch->ss = NULL;
    }

    RWMSG_EP_LOCK(ch->ep);
    switch (ch->chantype) {
    case RWMSG_CHAN_SRV:
      ck_pr_dec_32(&ch->ep->stat.objects.srvchans);
      RW_DL_REMOVE(&ch->ep->track.srvchans, ch, trackelem);
      break;
    case RWMSG_CHAN_CLI:
      ck_pr_dec_32(&ch->ep->stat.objects.clichans);
      RW_DL_REMOVE(&ch->ep->track.clichans, ch, trackelem);
      break;
    case RWMSG_CHAN_BROSRV:
      ck_pr_dec_32(&ch->ep->stat.objects.brosrvchans);
      RW_DL_REMOVE(&ch->ep->track.brosrvchans, ch, trackelem);
      break;
    case RWMSG_CHAN_BROCLI:
      ck_pr_dec_32(&ch->ep->stat.objects.broclichans);
      RW_DL_REMOVE(&ch->ep->track.broclichans, ch, trackelem);
      break;
    case RWMSG_CHAN_PEERSRV:
      ck_pr_dec_32(&ch->ep->stat.objects.peersrvchans);
      RW_DL_REMOVE(&ch->ep->track.peersrvchans, ch, trackelem);
      break;
    case RWMSG_CHAN_PEERCLI:
      ck_pr_dec_32(&ch->ep->stat.objects.peerclichans);
      RW_DL_REMOVE(&ch->ep->track.peerclichans, ch, trackelem);
      break;
    default:
      RW_CRASH();
      break;
    }
    RWMSG_EP_UNLOCK(ch->ep);
  }
}

void rwmsg_channel_create(rwmsg_channel_t *ch, rwmsg_endpoint_t *ep, enum rwmsg_chantype_e typ) {
  ch->chantype = typ;
  ch->vptr = &functab[typ];
  ch->refct = 1;
  ch->ep = ep;
  //...

  char *lqname="";
  char *bufqname="";

  //RW_DL_INIT(&ch->methbindings);

  int pfxletter = '?';
  int broker = FALSE;
  int connect_peer = FALSE;

  RWMSG_RG_LOCK();
  RW_ASSERT(rwmsg_global.chanid_nxt);
  ch->chanid = rwmsg_global.chanid_nxt;
  if (rwmsg_global.chanid_nxt == UINT32_MAX || rwmsg_global.chanid_nxt == 0) {
    rwmsg_global.chanid_nxt = 1;
  } else {
    rwmsg_global.chanid_nxt++;
  }
  RW_STATIC_ASSERT(sizeof(pid_t) == 4);
  ch->chanid |= (((uint64_t)getpid()) << 32);
  RWMSG_RG_UNLOCK();

  RWMSG_EP_LOCK(ep);
  switch (typ) {
  case RWMSG_CHAN_SRV:
    ck_pr_inc_32(&ep->stat.objects.srvchans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.srvchans, ch, trackelem);
    lqname = "srvchan";
    bufqname = "sc_ssbuf";
    pfxletter = 's';
    break;
  case RWMSG_CHAN_CLI:
    ck_pr_inc_32(&ep->stat.objects.clichans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.clichans, ch, trackelem);
    lqname = "clichan";
    bufqname = "cc_ssbuf";
    pfxletter = 'c';
    break;
  case RWMSG_CHAN_BROSRV:
    ck_pr_inc_32(&ep->stat.objects.brosrvchans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.brosrvchans, ch, trackelem);
    lqname = "brosrvchan";
    bufqname = "bsc_ssbuf";
    broker = TRUE;
    pfxletter = 'S';
    break;
  case RWMSG_CHAN_BROCLI:
    ck_pr_inc_32(&ep->stat.objects.broclichans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.broclichans, ch, trackelem);
    lqname = "broclichan";
    bufqname = "bcc_ssbuf";
    broker = TRUE;
    pfxletter = 'C';
    break;
  case RWMSG_CHAN_PEERSRV:
    ck_pr_inc_32(&ep->stat.objects.peersrvchans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.peersrvchans, ch, trackelem);
    lqname = "peersrvchan";
    bufqname = "psc_ssbuf";
    broker = TRUE;
    connect_peer = TRUE;
    pfxletter = 'P';
    break;
  case RWMSG_CHAN_PEERCLI:
    ck_pr_inc_32(&ep->stat.objects.peerclichans);
    RW_DL_ELEMENT_INIT(&ch->trackelem);
    RW_DL_INSERT_HEAD(&ep->track.peerclichans, ch, trackelem);
    lqname = "peerclichan";
    bufqname = "pcc_ssbuf";
    broker = TRUE;
    pfxletter = 'p';
    break;
  default:
    lqname = "invalid";
    bufqname = "invalid";
    RW_CRASH();
    break;
  }
  RWMSG_EP_UNLOCK(ep);

  snprintf(ch->rwtpfx, sizeof(ch->rwtpfx), "[channel %c%u]", pfxletter, ch->chanid);
  ch->rwtpfx[sizeof(ch->rwtpfx)-1] = '\0';

  RWMSG_TRACE_CHAN(ch, INFO, "Created channel %p lqname='%s' bufqname='%s'", ch, lqname, bufqname);

  /* Defer notification config until srvchan is bound to rwsched queue or cfrunloop */
  rw_status_t rs;
  rs = rwmsg_queue_init(ep, 
			&ch->localq, 
			lqname,
			NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(ch->localq.notify == &ch->localq._notify);
  RW_ASSERT(ch->localq.notify->theme == RWMSG_NOTIFY_NONE);

  int usebro = FALSE;
  rs = rwmsg_endpoint_get_property_int(ep, "/rwmsg/broker/enable", &usebro);
  if (rs == RW_STATUS_SUCCESS && usebro) {
    char uribuf[100], *brouri;
    uribuf[0] = '\0';
    rs = rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker/nnuri", uribuf, sizeof(uribuf));
    if (rs == RW_STATUS_SUCCESS) {
      brouri = uribuf;
    } else {
      brouri = NULL;
    }
    
    if (brouri) {
      /* Buffer outgoing request responses waiting on socket level flow control */
      rs = rwmsg_queue_init(ep, 
			    &ch->ssbufq,
			    bufqname,
			    NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

      if (!broker) {
        /* This is the clichan/srvchan code */
	struct rwmsg_handshake_s hs;
	RW_ZERO_VARIABLE(&hs);
	hs.chanid = ch->chanid;
	hs.pid = getpid();
	hs.pri = 0;
	hs.chtype = ch->chantype;
	hs.instid = ep->instid;
	
	ch->ss = rwmsg_sockset_create(ep, (uint8_t*)&hs, sizeof(hs), FALSE);

	rs = rwmsg_sockset_connect(ch->ss, brouri, RWMSG_PRIORITY_COUNT);
	RW_ASSERT(rs == RW_STATUS_SUCCESS);
      } else if (connect_peer) {
	char uribuf[100], *peeruri;
	uribuf[0] = '\0';
	rs = rwmsg_endpoint_get_property_string(ep, "/rwmsg/broker-peer/nnuri", uribuf, sizeof(uribuf));
	if (rs == RW_STATUS_SUCCESS) {
	  peeruri = uribuf;
	} else {
	  peeruri = NULL;
	}
	if (peeruri) {
          // This is the peerclichan/peersrvchan code
	  struct rwmsg_handshake_s hs;
	  RW_ZERO_VARIABLE(&hs);
	  hs.chanid = ch->chanid;
	  hs.pid = getpid();
	  hs.pri = 0;
	  hs.chtype = ch->chantype;

	  ch->ss = rwmsg_sockset_create(ep, (uint8_t*)&hs, sizeof(hs), TRUE);

          // rwzk_set_data has a format similar to :0:tcp://10.0.23.34:21346
          // skip the firts two :'s
	  char *pcount_b = strchr(peeruri, ':');
	  RW_ASSERT(pcount_b);
	  pcount_b++;
	  peeruri = pcount_b;
	  pcount_b = strchr(peeruri, ':');
	  RW_ASSERT(pcount_b);
	  pcount_b++;
	  peeruri = pcount_b;

	  rs = rwmsg_sockset_connect(ch->ss, peeruri, RWMSG_PRIORITY_COUNT);
	  RW_ASSERT(rs == RW_STATUS_SUCCESS);
	}
      }
      else {
	// Broker, no handshake data
        // This is the broclichan/brosrvchan code
	ch->ss = rwmsg_sockset_create(ep, NULL, 0, TRUE);
      }
    }
  }

  if (0) {
    static int once=0;
    if (!once) {
      once=1;
      RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_channel.c: usebro=%d", usebro);
    }
  }
}

/* Attempt to send all buffered messages to go out this pri */
rw_status_t rwmsg_channel_send_buffer_pri(rwmsg_channel_t *ch, rwmsg_priority_t pri, int pauseonxoff, int sendreq) {
  rw_status_t rs = RW_STATUS_SUCCESS;
  int ct=0;
  int wantpollout = FALSE;

  RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_channel_send_buffer_pri(pri=%u,xoxo=%d,sendreq=%d)", pri, pauseonxoff, sendreq);

  rwmsg_request_t *bufreq;
  while ((bufreq = rwmsg_queue_head_pri(&ch->ssbufq, pri))) {
    RW_ASSERT(bufreq->hdr.pri == pri);

    RWMSG_REQ_TRACK(bufreq);
    rs = rwmsg_sockset_send(ch->ss, pri, sendreq ? &bufreq->req.msg : &bufreq->rsp.msg);
    RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_channel_send_buffer_pri sockset_send(req" FMT_MSG_HDR(req->hdr) ") rs=%d", 
                     PRN_MSG_HDR(bufreq->hdr), rs);
    if (rs == RW_STATUS_SUCCESS) {
      rwmsg_request_t *tmpreq = rwmsg_queue_dequeue_pri_spin(&ch->ssbufq, pri);
      RW_ASSERT(tmpreq == bufreq);
      if (sendreq) {
	rwmsg_request_message_free(&bufreq->req);
      } else {
	rwmsg_request_release(bufreq);
      }
      ct++;
    } else if (rs == RW_STATUS_BACKPRESSURE) {
      RWMSG_TRACE_CHAN(ch, DEBUG, "rwmsg_channel_send_buffer_pri sockset_send pri=%d rs=BACKPRESSURE", pri);
      if (pauseonxoff) {
	/* pause reads need sends until we get a sendme callback and can send this out */
	rwmsg_sockset_pause_pri(ch->ss, pri);
      }
      wantpollout = TRUE;
      goto out;
    } else if (rs == RW_STATUS_NOTCONNECTED) {
      /* No go */
      RW_ASSERT(ct == 0);
      wantpollout = TRUE;
      goto out;
    } else {
      // nn reconnects automagically so there should be little else to handle here?
      RW_CRASH();
    }
  }

 out:

  if (pauseonxoff) {
    if (ct && rs == RW_STATUS_SUCCESS) {
      if (!ch->paused_user) {
	rwmsg_sockset_resume_pri(ch->ss, pri);
      }
    }
  }
  rwmsg_sockset_pollout(ch->ss, pri, wantpollout);
  return rs;
}

