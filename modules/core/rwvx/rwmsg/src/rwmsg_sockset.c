/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwmsg_sockset.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG sockset object
 */

#include <sys/resource.h>
#include <nanomsg/pair.h>
#include <nanomsg/tcp.h>
#include "rwmsg_int.h"


rwmsg_sockset_t *rwmsg_sockset_create(rwmsg_endpoint_t *ep, uint8_t *hsval, size_t hsval_sz, rwmsg_bool_t ageout) {
  RW_STATIC_ASSERT(RWMSG_PRIORITY_COUNT <= 6); /* bit width of sk_mask */
  rwmsg_sockset_t *ss = NULL;

  ss = (rwmsg_sockset_t *)RW_MALLOC0_TYPE(sizeof(*ss), rwmsg_sockset_t);
  ss->refct = 1;
  ss->sktype = RWMSG_SOCKSET_SKTYPE_NN;
  ss->ageout = ageout;

  RW_ASSERT(hsval_sz <= sizeof(ss->hsval));
  if (hsval && hsval_sz) {
    memcpy(&ss->hsval, hsval, hsval_sz);
  }
  ss->hsval_sz = hsval_sz;
  ss->ep = ep;
  ck_pr_inc_32(&ep->refct);

  ss->sched.theme = RWMSG_NOTIFY_NONE;
  //...

  ck_pr_inc_32(&ep->stat.objects.socksets);
  RWMSG_EP_LOCK(ep);
  RW_DL_INSERT_HEAD(&ep->track.socksets, ss, trackelem);
  RWMSG_EP_UNLOCK(ep);

  return ss;
}

static void rwmsg_sockset_close_pri(rwmsg_sockset_t *ss, rwmsg_priority_t pri) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);

  struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
  if (ss->sk_mask & (1<<pri)) {
    RW_ASSERT(sk->sk >= 0);
    sk->state = RWMSG_NNSK_IDLE;
    
    rwmsg_notify_pause(ss->ep, &sk->noteout);
    rwmsg_notify_pause(ss->ep, &sk->notein);
    sk->noteout.wantack = TRUE;
    sk->notein.wantack = TRUE;
    rwmsg_notify_deinit(ss->ep, &sk->noteout);
    rwmsg_notify_deinit(ss->ep, &sk->notein);
    int nn = sk->sk;
    sk->sk = -1;
    //evfds go away on nn_close
    //if we were connected, ie in sk_mask, they should exist
    RW_ASSERT(ss->nn.sk[pri].evfd_in >= 0);
    ck_pr_dec_32(&ss->ep->stat.objects.nn_eventfds);
    RW_ASSERT(ss->nn.sk[pri].evfd_out >= 0);
    ck_pr_dec_32(&ss->ep->stat.objects.nn_eventfds);
    nn_close(nn);
    //sk->sk = -1;
    ss->sk_mask &= ~(1<<pri);
  }
}

static void rwmsg_sockset_closeall(rwmsg_sockset_t *ss) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);

  int pri;
  RW_ASSERT(ss->sk_ct <= RWMSG_PRIORITY_COUNT);
  for (pri=0; pri<ss->sk_ct; pri++) {
    if (ss->sk_mask & (1<<pri)) {
      rwmsg_sockset_close_pri(ss, pri);
    }
  }
}

void rwmsg_sockset_close(rwmsg_sockset_t *ss) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(!ss->closed);
  ss->closed = 1;

  // flush any queues

  if (ss->ageout_timer) {
    rwsched_dispatch_source_cancel(ss->ep->taskletinfo, ss->ageout_timer);
    rwsched_dispatch_source_release(ss->ep->taskletinfo, ss->ageout_timer);
    ss->ageout_timer = NULL;
    rwmsg_sockset_release(ss);
  }

  rwmsg_sockset_closeall(ss);
  rwmsg_sockset_release(ss);
}

void rwmsg_sockset_release(rwmsg_sockset_t *ss) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  rwmsg_endpoint_t *ep = ss->ep;

  bool zero=0;
  ck_pr_dec_32_zero(&ss->refct, &zero);
  if (zero) {
    /* Please call rwmsg_sockset_close() first */
    RW_ASSERT(ss->closed);
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_SOCKSET, ss, NULL, NULL);
  }
  return;
}

void rwmsg_sockset_destroy(rwmsg_sockset_t *ss) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  if (!ss->refct) {
    RW_ASSERT(!ss->refct);

    int p;
    for (p=0; p<RWMSG_PRIORITY_COUNT; p++) {
      RW_ASSERT(ss->sktype == RWMSG_SOCKSET_SKTYPE_NN);
      if ( (ss->nn.sk[p].notein.halted && ss->nn.sk[p].notein.wantack && !ss->nn.sk[p].notein.cancelack)
	   || (ss->nn.sk[p].noteout.halted && ss->nn.sk[p].noteout.wantack && !ss->nn.sk[p].noteout.cancelack) ) {

	if (1) {
    RWMSG_TRACE(ss->ep, DEBUG, "ss=%p sk[%d] notein=%p wantack=%d canack=%d rwsrc=%p noteout=%p wantack=%d canack=%d rwsrc=%p",
                ss,
                p,
                &ss->nn.sk[p].notein,
                ss->nn.sk[p].notein.wantack,
                ss->nn.sk[p].notein.cancelack,
                ss->nn.sk[p].notein.rwsched.rwsource,
                &ss->nn.sk[p].noteout,
                ss->nn.sk[p].noteout.wantack,
                ss->nn.sk[p].noteout.cancelack,
                ss->nn.sk[p].noteout.rwsched.rwsource);

	}

	// requeue self
	rwmsg_garbage(&ss->ep->gc, RWMSG_OBJTYPE_SOCKSET, ss, NULL, NULL);
	return;
      }
    }

    RWMSG_EP_LOCK(ss->ep);
    RW_DL_REMOVE(&ss->ep->track.socksets, ss, trackelem);
    RWMSG_EP_UNLOCK(ss->ep);
    ck_pr_dec_32(&ss->ep->refct);
    ck_pr_dec_32(&ss->ep->stat.objects.socksets);
    memset(ss, 0, sizeof(*ss));
    RW_FREE_TYPE(ss, rwmsg_sockset_t);
  }
  return;
}

/* Send message.  May or may not invalidate msg->msg.nnbuf, depending
   on if it was an nnbuf and the send succeeded. */
static rw_status_t rwmsg_sockset_send_int(rwmsg_sockset_t *ss,
					  rwmsg_priority_t pri, 
					  rwmsg_buf_t *buf,
					  int keepnnmsg) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(ss->sktype == RWMSG_SOCKSET_SKTYPE_NN);
  RW_ASSERT(buf->nnbuf_nnmsg);
  RW_ASSERT(buf->nnbuf);

  rw_status_t rs = RW_STATUS_FAILURE;

  if ((1<<pri) & ss->sk_mask) {
    if (ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTING ||
        ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTED) {

      int nnmsg_keepval = buf->nnbuf_nnmsg;
      if (keepnnmsg) {
	buf->nnbuf_nnmsg = 0;
      }

      rs = RW_STATUS_FAILURE;
      struct nn_iovec iov;
      iov.iov_base = buf->nnbuf_nnmsg ? &buf->nnbuf : buf->nnbuf;
      RW_ASSERT(buf->nnbuf_len >= sizeof(RWMSG_HEADER_WIRESIZE));
      iov.iov_len = buf->nnbuf_nnmsg ? NN_MSG : buf->nnbuf_len;
      struct nn_msghdr hdr = {
	.msg_iov = &iov,
	.msg_iovlen = 1,
	.msg_control = NULL,
	.msg_controllen = 0
      };

      int nnbuf_len = buf->nnbuf_len;
      int r = nn_sendmsg(ss->nn.sk[pri].sk, &hdr, NN_DONTWAIT);
      if (r > 0) {
	RW_ASSERT(hdr.msg_iovlen == 1);
	RW_ASSERT((uint32_t)r == nnbuf_len);
	if (!buf->nnbuf_nnmsg) {
	  RW_ASSERT(hdr.msg_iov[0].iov_len == buf->nnbuf_len);
	  RW_ASSERT((uint32_t)r == hdr.msg_iov[0].iov_len);
	} else {
	  //...
	}
	rs = RW_STATUS_SUCCESS;
	RWMSG_TRACE(ss->ep, DEBUG, "sockset_send_int(ss=%p,pri=%d) nnsk=%d SUCCESS", ss, pri, ss->nn.sk[pri].sk);
	if (keepnnmsg) {
	  buf->nnbuf_nnmsg = nnmsg_keepval;
	} else {
	  if (buf->nnbuf_nnmsg) {
	    buf->nnbuf = NULL;
	    buf->nnbuf_len = 0;
	    buf->nnbuf_nnmsg = FALSE;
	  }
	}
      } else {
	int nnerrno = errno;
	if (keepnnmsg) {
	  buf->nnbuf_nnmsg = nnmsg_keepval;
	}
	RWMSG_TRACE(ss->ep, DEBUG, "sockset_send_int(ss=%p,pri=%d) nnsk=%d errno=%d '%s'", ss, pri, ss->nn.sk[pri].sk, nnerrno, strerror(nnerrno));
	switch (nnerrno) {
	case EAGAIN:
	case EINTR:
          if (ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTING) {
	    rs = RW_STATUS_NOTCONNECTED;
          } else {
	    rs = RW_STATUS_BACKPRESSURE;
          }
	  break;
	case EFAULT:
	  RW_CRASH();
	  break;
	case EMSGSIZE:
	  RW_CRASH();
	  break;
	case EINVAL:
	  RW_CRASH();
	  break;
	case EBADF:
	  RW_CRASH();
	  break;
	case ENOTSUP:
	  RW_CRASH();
	  break;
	case EFSM:
	  RW_CRASH();
	  break;
	case ETERM:
	  RW_CRASH();
	  break;
	default:
	  RW_CRASH();
	  break;
	}
      }
    }
    else {
      rs = RW_STATUS_NOTCONNECTED;
    }
  }

  return rs;
}

rw_status_t rwmsg_sockset_send(rwmsg_sockset_t *ss,
			       rwmsg_priority_t pri, 
			       rwmsg_buf_t *buf) {
  return rwmsg_sockset_send_int(ss, pri, buf, FALSE);
}
rw_status_t rwmsg_sockset_send_copy(rwmsg_sockset_t *ss,
				    rwmsg_priority_t pri, 
				    rwmsg_buf_t *buf) {
  RW_ASSERT(buf->nnbuf_nnmsg);
  return rwmsg_sockset_send_int(ss, pri, buf, TRUE);
}

rw_status_t rwmsg_sockset_recv(rwmsg_sockset_t *ss, 
			       rwmsg_buf_t *buf,
			       rwmsg_priority_t *pri_out) {
  rw_status_t rs;
  int pri=0;
  for (pri = RWMSG_PRIORITY_COUNT-1; pri >= 0; pri--) {
    rs = rwmsg_sockset_recv_pri(ss, pri, buf);
    if (rs == RW_STATUS_SUCCESS) {
      if (pri_out) {
	*pri_out = pri;
      }
      return rs;
    }
  }
  return RW_STATUS_BACKPRESSURE;
}

rwsched_CFRunLoopSourceRef rwmsg_sockset_get_blk_cfsrc(rwmsg_sockset_t *ss) {
  rwsched_CFRunLoopSourceRef cfsrc = NULL;
  if (ss->sktype == RWMSG_SOCKSET_SKTYPE_NN
      && ss->sched.theme == RWMSG_NOTIFY_CFRUNLOOP_FD
      && ( ss->nn.sk[RWMSG_PRIORITY_BLOCKING].state == RWMSG_NNSK_CONNECTING
         ||ss->nn.sk[RWMSG_PRIORITY_BLOCKING].state == RWMSG_NNSK_CONNECTED)
      && ss->nn.sk[RWMSG_PRIORITY_BLOCKING].notein.theme == RWMSG_NOTIFY_CFRUNLOOP_FD) {
    cfsrc = ss->nn.sk[RWMSG_PRIORITY_BLOCKING].notein.cfrunloop.cfsource;
  }
  return cfsrc;
}

rw_status_t rwmsg_sockset_recv_pri(rwmsg_sockset_t *ss, 
				   rwmsg_priority_t pri, 
				   rwmsg_buf_t *buf) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(ss->sktype == RWMSG_SOCKSET_SKTYPE_NN);

  rw_status_t rs = RW_STATUS_FAILURE;

  if ((1<<pri) & ss->sk_mask) {
    if ((ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTING ||
         ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTED) && !ss->nn.sk[pri].paused) {
      struct nn_iovec iov;
      void *nnbuf = NULL;
      iov.iov_base = &nnbuf;
      iov.iov_len = NN_MSG;
      struct nn_msghdr hdr = {
	.msg_iov = &iov,
	.msg_iovlen = 1,
	.msg_control = NULL,
	.msg_controllen = 0
      };
      
      int r = nn_recvmsg(ss->nn.sk[pri].sk, &hdr, NN_DONTWAIT);
      if (r > 0) {
	buf->nnbuf = nnbuf;
	buf->nnbuf_len = r;
	buf->nnbuf_nnmsg = TRUE;
	rs = RW_STATUS_SUCCESS;
      } else {
	rs = RW_STATUS_FAILURE;
	switch (errno) {
	case EAGAIN:
	case EINTR:
	  rs = RW_STATUS_BACKPRESSURE;
	  break;
	case EFAULT:
	  RW_CRASH();
	  break;
	case EMSGSIZE:
	  RW_CRASH();
	  break;
	case EINVAL:
	  RW_CRASH();
	  break;
	case EBADF:
	  RW_CRASH();
	  break;
	case ENOTSUP:
	  RW_CRASH();
	  break;
	case EFSM:
	  RW_CRASH();
	  break;
	case ETERM:
	  RW_CRASH();
	  break;
	default:
    RWMSG_TRACE(ss->ep, DEBUG, "rwmsg_sockset_recv_pri r=%d errno=%d '%s'", r, errno, strerror(errno));
	  RW_CRASH();
	  break;
	}
      }
    }
  }

  return rs;
}


static void rwmsg_sockset_nnsk_event_in(void *ud, int bits) {
  struct rwmsg_sockset_nnsk_s *sk = (struct rwmsg_sockset_nnsk_s *)ud;

  if (sk->state == RWMSG_NNSK_IDLE) {
    return;
  }

  int pri = sk->pri;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

  uint8_t *sk0ptr = (uint8_t*)&sk[-pri];
  rwmsg_sockset_t *ss = (rwmsg_sockset_t *)(sk0ptr - offsetof(rwmsg_sockset_t, nn.sk[0]));
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(pri < ss->sk_ct);

  if (!sk->paused && ((1<<pri)&ss->sk_mask)) {
    ss->sched.clo.recvme(ss, pri, ss->sched.clo.ud);
  }

  return;
  bits=bits;
}

static void rwmsg_sockset_nnsk_event_out(void *ud, int bits) {
  struct rwmsg_sockset_nnsk_s *sk = (struct rwmsg_sockset_nnsk_s *)ud;

  if (sk->state == RWMSG_NNSK_IDLE) {
    return;
  }

  int pri = sk->pri;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

  uint8_t *sk0ptr = (uint8_t*)&sk[-pri];
  rwmsg_sockset_t *ss = (rwmsg_sockset_t *)(sk0ptr - offsetof(rwmsg_sockset_t, nn.sk[0]));
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(pri < ss->sk_ct);

  if (((1<<pri)&ss->sk_mask)) {
    ss->sched.clo.sendme(ss, pri, ss->sched.clo.ud);
  } else {
    rwmsg_sockset_pollout(ss, pri, FALSE);
  }

  return;
  bits=bits;
}

//#define NN_DIRECT_CB

static void rwmsg_nn_direct_dispatch_f(void *ctx) {
  rwmsg_closure_t *cb = (rwmsg_closure_t*)ctx;
  struct rwmsg_sockset_nnsk_s *sk = (struct rwmsg_sockset_nnsk_s *)(cb->ud);
  int pri = sk->pri;
  int count = 0;
  static int winval = 0;
  RW_ASSERT(pri >= 0);
  RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

  uint8_t *sk0ptr = (uint8_t*)&sk[-pri];
  rwmsg_sockset_t *ss = (rwmsg_sockset_t *)(sk0ptr - offsetof(rwmsg_sockset_t, nn.sk[0]));
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(pri < ss->sk_ct);

  winval = 50;

  if (!sk->paused && ((1<<pri)&ss->sk_mask)) {
    rwmsg_channel_t *ch = (rwmsg_channel_t*)ss->sched.clo.ud;

    while (count++ < winval) {
      rw_status_t rs = RW_STATUS_FAILURE;
      rwmsg_buf_t buf = { .nnbuf = NULL, .nnbuf_len = 0 };

      if (!ch->halted) {
        if (ch->paused_ss) {
          RWMSG_TRACE_CHAN(ch, DEBUG, "CB -rwmsg_channel_sockset_event_recv - paused_ss %u", 1);
          rwmsg_sockset_pause_pri(ss, pri);
          sk->cb_while_paused = 1;
          break;
        }

        rs = rwmsg_sockset_recv_pri(ss, pri, &buf);
        if (rs == RW_STATUS_SUCCESS) {
            ch->chanstat.msg_in_sock++;
            rs = ch->vptr->recv_buf(ch, &buf);
        }
        else
          break;
      }
    }
    if (count >= winval) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / winval * 50 * 0.001);
      rwsched_dispatch_after_f(ss->ep->taskletinfo, when, ss->sched.rwsqueue, ctx, rwmsg_nn_direct_dispatch_f);
    }
  }
  else {
    sk->cb_while_paused = 1;
  }
  ck_pr_barrier();
}

int rwmsg_nn_direct_cb(int fd) {

  if (fd>NN_EVENTFD_DIRECT_OFFSET) {
    RW_STATIC_ASSERT(sizeof(rwmsg_global.cbs)/sizeof(rwmsg_global.cbs[0]) == NN_EVENTFD_DIRECT_OFFSET);
    RW_ASSERT(fd-NN_EVENTFD_DIRECT_OFFSET < NN_EVENTFD_DIRECT_OFFSET);
    if (rwmsg_global.cbs[fd-NN_EVENTFD_DIRECT_OFFSET].notify)
      return 1;
    else
      return 0;
  }
  if (rwmsg_global.cbs[fd].notify) {
    struct rwmsg_sockset_nnsk_s *sk = (struct rwmsg_sockset_nnsk_s *)(rwmsg_global.cbs[fd].ud);
    int pri = sk->pri;
    RW_ASSERT(pri >= 0);
    RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

    uint8_t *sk0ptr = (uint8_t*)&sk[-pri];
    rwmsg_sockset_t *ss = (rwmsg_sockset_t *)(sk0ptr - offsetof(rwmsg_sockset_t, nn.sk[0]));

    RW_ASSERT(ss->sched.rwsched);
    RW_ASSERT(ss->ep->taskletinfo);
    if (ss->sched.rwsqueue) {
      rwsched_dispatch_async_f(ss->ep->taskletinfo, ss->sched.rwsqueue, &rwmsg_global.cbs[fd], rwmsg_nn_direct_dispatch_f);
    }
    else {
      rwsched_dispatch_async_f(ss->ep->taskletinfo, rwsched_dispatch_get_main_queue(ss->sched.rwsched), &rwmsg_global.cbs[fd], rwmsg_nn_direct_dispatch_f);
    }
    return 1;
  }
  return 0;
}

static rw_status_t rwmsg_sockset_bind_nnsk(rwmsg_sockset_t *ss,
					   struct rwmsg_sockset_nnsk_s *sk,
					   int pri) {
  RW_ASSERT(ss->sched.theme == RWMSG_NOTIFY_CFRUNLOOP_FD
	    || ss->sched.theme == RWMSG_NOTIFY_RWSCHED_FD
	    || ss->sched.theme == RWMSG_NOTIFY_NONE);

  rw_status_t rs = RW_STATUS_FAILURE;

  RWMSG_TRACE(ss->ep, DEBUG, "sockset_bind_nnsk(ss=%p, sk=%p pri=%d)", ss, sk, pri);

  if (ss->sched.theme == RWMSG_NOTIFY_NONE) {
    RWMSG_TRACE(ss->ep, DEBUG, "sockset_bind_nnsk(ss=%p, sk=%p pri=%d) theme NONE", ss, sk, pri);
    rs = RW_STATUS_SUCCESS;
  } else {
    sk->notein.theme = ss->sched.theme;
    sk->notein.fd = sk->evfd_in;
    sk->notein.order = RWMSG_PRIORITY_COUNT - pri - 1;
    if (ss->sched.theme == RWMSG_NOTIFY_RWSCHED_FD) {
      sk->notein.rwsched.rwqueue = ss->sched.rwsqueue;
      RWMSG_TRACE(ss->ep, DEBUG, "sockset_bind_nnsk(ss=%p, sk=%p pri=%d) theme RWSCHED_FD evfd_in=%d evfd_out=%d", ss, sk, pri, sk->evfd_in, sk->evfd_out);
    } else {
      RWMSG_TRACE(ss->ep, DEBUG, "sockset_bind_nnsk(ss=%p, sk=%p pri=%d) theme CFRUNLOOP_FD evfd_in=%d evfd_out=%d", ss, sk, pri, sk->evfd_in, sk->evfd_out);
    }
    rwmsg_closure_t cbin = { .notify = rwmsg_sockset_nnsk_event_in, sk };
#ifdef NN_DIRECT_CB
    RW_ASSERT(sk->evfd_in<NN_EVENTFD_DIRECT_OFFSET);
    rwmsg_global.cbs[sk->evfd_in] = cbin;
#endif
    rs = rwmsg_notify_init(ss->ep, &sk->notein, "sk->notein", &cbin);
    if (rs != RW_STATUS_SUCCESS) {
      RWMSG_TRACE(ss->ep, DEBUG, "rwmsg_notify_init(evfd_in) rs=%d", rs);
      goto bail;
    }
    sk->paused = FALSE;
    
    sk->noteout.theme = ss->sched.theme;
    sk->noteout.fd = sk->evfd_out;
    sk->noteout.order = RWMSG_PRIORITY_COUNT - pri - 1;
    if (ss->sched.theme == RWMSG_NOTIFY_RWSCHED_FD) {
      sk->noteout.rwsched.rwqueue = ss->sched.rwsqueue;
    }
    rwmsg_closure_t cbout = { .notify = rwmsg_sockset_nnsk_event_out, sk };
    rs = rwmsg_notify_init(ss->ep, &sk->noteout, "sk->noteout", &cbout);
    if (rs != RW_STATUS_SUCCESS) {
      RWMSG_TRACE(ss->ep, DEBUG, "rwmsg_notify_init(evfd_in) rs=%d", rs);
      goto bail;
    }
    sk->pollout = TRUE;

    rs = RW_STATUS_SUCCESS;
  }

 bail:
  return rs;
}

rw_status_t rwmsg_sockset_bind_rwsched_cfrunloop(rwmsg_sockset_t *ss, 
						 rwsched_tasklet_ptr_t tinfo, 
						 rwmsg_sockset_closure_t *cb) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(ss->sched.theme == RWMSG_NOTIFY_NONE);

  RWMSG_TRACE(ss->ep, DEBUG, "sockset_bind_rwsched_cfrunloop(ss=%p, taskletinfo=%p)", ss, tinfo);

  rw_status_t rs = RW_STATUS_FAILURE;

  ss->sched.theme = RWMSG_NOTIFY_CFRUNLOOP_FD;
  ss->sched.clo = *cb;
  ss->sched.rwsched = ss->ep->rwsched;
  ss->sched.taskletinfo = tinfo;

  int pri;
  for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
    switch (ss->sktype) {
    case RWMSG_SOCKSET_SKTYPE_NN:
      {
	struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
	if (sk->state == RWMSG_NNSK_CONNECTING ||
	    sk->state == RWMSG_NNSK_CONNECTED) {
	  sk->notein.cfrunloop.taskletinfo = tinfo;
	  sk->noteout.cfrunloop.taskletinfo = tinfo;
	  rs = rwmsg_sockset_bind_nnsk(ss, sk, pri);
	}
	if (rs != RW_STATUS_SUCCESS) {
	  break;
	}
      }
      break;
    default:
      break;
    }
  }
  return rs;
}



rw_status_t rwmsg_sockset_bind_rwsched_queue(rwmsg_sockset_t *ss, 
					     rwsched_dispatch_queue_t q, 
					     rwmsg_sockset_closure_t *cb) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(ss->sched.theme == RWMSG_NOTIFY_NONE);

  rw_status_t rs = RW_STATUS_FAILURE;

  ss->sched.theme = RWMSG_NOTIFY_RWSCHED_FD;
  ss->sched.clo = *cb;
  ss->sched.rwsched = ss->ep->rwsched;
  ss->sched.rwsqueue = q;

  int pri;
  for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
    switch (ss->sktype) {
    case RWMSG_SOCKSET_SKTYPE_NN:
      {
	struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
	if (sk->state == RWMSG_NNSK_CONNECTING ||
	    sk->state == RWMSG_NNSK_CONNECTED) {
	  sk->notein.rwsched.rwqueue = q;
	  sk->noteout.rwsched.rwqueue = q;
	  rs = rwmsg_sockset_bind_nnsk(ss, sk, pri);
	}
      }
      break;
    default:
      break;
    }
  }

  return rs;
}

rw_status_t rwmsg_sockset_attach_fd(rwmsg_sockset_t *ss, int fd, rwmsg_priority_t pri) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(fd >= 0);

  rw_status_t rs = RW_STATUS_FAILURE;

  switch (ss->sktype) {
  case RWMSG_SOCKSET_SKTYPE_NN:
    rwmsg_sockset_close_pri(ss, pri);
    char nnuri[64];
    sprintf(nnuri, "tcp://:fd%d", fd);

    rs = rwmsg_sockset_connect_pri(ss, nnuri, pri);
    if (rs == RW_STATUS_SUCCESS) {
      ss->sk_ct = MAX(ss->sk_ct, pri+1);

      struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];

      RW_ASSERT(!sk->paused);
      RW_ASSERT(sk->notein.theme == RWMSG_NOTIFY_RWSCHED_FD);
      RW_ASSERT(ss->sched.theme != RWMSG_NOTIFY_NONE); /* arguably optional, but attach is only done in broker where this is true */
      if (sk->notein.paused) {
        RWMSG_TRACE(ss->ep, DEBUG, "***** sk->notein.paused %d !!", sk->notein.paused);
      }
      if (sk->notein.halted) {
        RWMSG_TRACE(ss->ep, DEBUG, "***** sk->notein.halted %d !!", sk->notein.halted);
      }
      sk->paused = 0;
      rwmsg_notify_resume(ss->ep, &sk->notein);
      RW_ASSERT(!sk->notein.paused);
      RW_ASSERT(!sk->notein.halted);
    }
    break;
  default:
    RW_CRASH();
    break;
  }

  return rs;
}

void rwmsg_ageout_timer_f(void *ctx) {
  rwmsg_sockset_t *ss = (rwmsg_sockset_t*)ctx;
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);

  if (ss->ageout_timer) {
    rwsched_dispatch_source_cancel(ss->ep->taskletinfo, ss->ageout_timer);
    rwsched_dispatch_source_release(ss->ep->taskletinfo, ss->ageout_timer);
    ss->ageout_timer = NULL;
  }

  rwmsg_channel_t *ch = (rwmsg_channel_t*)ss->sched.clo.ud;
  int pri;
  RW_ASSERT(ss->sk_ct <= RWMSG_PRIORITY_COUNT);
  for (pri=0; pri<ss->sk_ct; pri++) {
    struct nn_sock_stats nn_stats;
    size_t olen = sizeof(nn_stats);
    nn_getsockopt(ss->nn.sk[pri].sk, NN_SOL_SOCKET, NN_RWGET_STATS, &nn_stats, &olen);

    if (nn_stats.current_connections) {
      // Still connection UP? don't ageout this chennel
      goto _ret;
    }
  }

  for (pri=0; pri<ss->sk_ct; pri++) {
    if (ss->sk_mask & (1<<pri)) {
      //struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
      rwmsg_sockset_close_pri(ss, pri);
    }
  }
  rwmsg_sockset_release(ss);
  rwmsg_channelspecific_halt(ch);
  return;

_ret:
  rwmsg_sockset_release(ss);
  return;
}

static rw_status_t rwmsg_sockset_connection_status_pri(rwmsg_sockset_t *ss,
                                                       int pri) {
  if (ss->nn.sk[pri].state != RWMSG_NNSK_CONNECTED) {
    return RW_STATUS_NOTCONNECTED;
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t rwmsg_sockset_connection_status(rwmsg_sockset_t *ss) {
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  int pri;
  rw_status_t rs;
  for (pri=0; pri < RWMSG_PRIORITY_COUNT; pri++) {
    if ((rs = rwmsg_sockset_connection_status_pri(ss, pri)) != RW_STATUS_SUCCESS) {
      return rs;
    }
  }
  return RW_STATUS_SUCCESS;
}

static void rwmsg_nn_connection_indicationn_cb(void *handle, int conns) {
  struct rwmsg_sockset_nnsk_s *sk = (struct rwmsg_sockset_nnsk_s *)handle;
  ck_pr_barrier();
  if (sk) {
    int pri = sk->pri;
    RW_ASSERT(pri >= 0);
    RW_ASSERT(pri < RWMSG_PRIORITY_COUNT);

    uint8_t *sk0ptr = (uint8_t*)&sk[-pri];
    rwmsg_sockset_t *ss = (rwmsg_sockset_t *)(sk0ptr - offsetof(rwmsg_sockset_t, nn.sk[0]));
    RW_ASSERT_TYPE(ss, rwmsg_sockset_t);

    if (sk->sk>=0) {
      if (ss->ageout_timer) {
        rwsched_dispatch_source_cancel(ss->ep->taskletinfo, ss->ageout_timer);
	rwsched_dispatch_source_release(ss->ep->taskletinfo, ss->ageout_timer);
	ss->ageout_timer = NULL;
	rwmsg_sockset_release(ss);
      }

      if (conns == 0) {
	rwsched_dispatch_queue_t rwsqueue;

        if (ss->ageout) {
          int ageout_time;
          rwmsg_endpoint_get_property_int(ss->ep, "/rwmsg/channel/ageout_time", &ageout_time);
          if (ageout_time > 0) {
            ck_pr_inc_32(&ss->refct);

            if (ss->sched.rwsqueue) {
              rwsqueue = ss->sched.rwsqueue;
            } else {
              rwsqueue = rwsched_dispatch_get_main_queue(ss->sched.rwsched);
            }
            ss->ageout_timer = rwsched_dispatch_source_create(ss->ep->taskletinfo,
                                                              RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                              0,
                                                              0,
                                                              rwsqueue);
            RW_ASSERT(ss->ageout_timer);
            rwsched_dispatch_source_set_timer(ss->ep->taskletinfo, ss->ageout_timer,
                                              dispatch_time(DISPATCH_TIME_NOW, ageout_time * NSEC_PER_SEC), 0, 0); // 1sec
            rwsched_dispatch_source_set_event_handler_f(ss->ep->taskletinfo, ss->ageout_timer, rwmsg_ageout_timer_f);
            rwsched_dispatch_set_context(ss->ep->taskletinfo, ss->ageout_timer, ss);
            rwsched_dispatch_resume(ss->ep->taskletinfo, ss->ageout_timer);
          }
        }
      } else {
        if (ss->nn.sk[pri].state == RWMSG_NNSK_CONNECTING) {
          RWMSG_TRACE(ss->ep, DEBUG, "RWMSG_NNSK_CONNECTED(ss=%p,pri=%d) SUCCESS", ss, pri);
          ss->nn.sk[pri].state = RWMSG_NNSK_CONNECTED;
        }
      }
    }
  }
}

/* Connect this one uri in the given pri slot */
rw_status_t rwmsg_sockset_connect_pri(rwmsg_sockset_t *ss, const char *uri, rwmsg_priority_t pri) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);

  rw_status_t rs = RW_STATUS_FAILURE;

  switch (ss->sktype) {
  case RWMSG_SOCKSET_SKTYPE_NN:
    {
      int r;

      struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
      sk->evfd_out = -1;
      sk->evfd_in = -1;

      int nn = nn_socket(AF_SP, NN_PAIR);
      if (nn < 0) {
	int saverr = errno;
	struct rlimit orlim = { 0 };
	getrlimit(RLIMIT_NOFILE, &orlim);
	RWMSG_TRACE(ss->ep, ERROR, "rwmsg_socket_connect/nn_socket(AF_SP, NN_PAIR) err='%s' rlim.cur=%u/.max=%u\n", strerror(saverr), (unsigned int)orlim.rlim_cur, (unsigned int)orlim.rlim_max);
	goto bail;
      }

      int opt = 1;
      r = nn_setsockopt (nn, NN_TCP, NN_TCP_NODELAY, &opt, sizeof (opt));
      RW_ASSERT(r == 0);

      opt = RWMSG_SO_SNDBUF;
      r = nn_setsockopt (nn, NN_SOL_SOCKET, NN_SNDBUF, &opt, sizeof (opt));
      RW_ASSERT(r == 0);

      opt = RWMSG_SO_RCVBUF;
      r = nn_setsockopt (nn, NN_SOL_SOCKET, NN_RCVBUF, &opt, sizeof (opt));
      RW_ASSERT(r == 0);


      /* Gah! */
      struct rwmsg_handshake_s *hs = (struct rwmsg_handshake_s *)&ss->hsval;
      hs->pri = pri;

      if (ss->hsval_sz) {
	r = nn_setsockopt(nn, NN_SOL_SOCKET, NN_RWHANDSHAKE, &ss->hsval, ss->hsval_sz);
	if (r < 0) {
	  int saverr = errno;
	  struct rlimit orlim = { 0 };
	  getrlimit(RLIMIT_NOFILE, &orlim);
	  RWMSG_TRACE(ss->ep, ERROR, "rwmsg_socket_connect/nn_setsockopt(NN_RWHANDSHAKE) err='%s' rlim.cur=%u/.max=%u\n", strerror(saverr), (unsigned int)orlim.rlim_cur, (unsigned int)orlim.rlim_max);
	  goto bail;
	}
      }
      int eid = nn_connect(nn, uri);
      if (eid < 0) {
	int saverr = errno;
	if (errno == EMFILE) {
	  struct rlimit orlim = { 0 };
	  getrlimit(RLIMIT_NOFILE, &orlim);
	  RWMSG_TRACE(ss->ep, ERROR, "rwmsg_socket_connect/nn_connect err='%s' rlim.cur=%u/.max=%u\n", strerror(saverr), (unsigned int)orlim.rlim_cur, (unsigned int)orlim.rlim_max);
	}
	goto bail;
      }
      int evsk = 0;
      size_t olen = sizeof(evsk);
      r = nn_getsockopt(nn, NN_SOL_SOCKET, NN_SNDFD, &evsk, &olen);
      if (r) {
	int saverr = errno;
	struct rlimit orlim = { 0 };
	getrlimit(RLIMIT_NOFILE, &orlim);
	RWMSG_TRACE(ss->ep, ERROR, "rwmsg_socket_connect/nn_getsockopt(NN_SNDFD) err='%s' rlim.cur=%u/.max=%u\n", strerror(saverr), (unsigned int)orlim.rlim_cur, (unsigned int)orlim.rlim_max);
	goto bail;
      }
      RW_ASSERT(evsk >= 0);
      sk->evfd_out = evsk;
      ck_pr_inc_32(&ss->ep->stat.objects.nn_eventfds);
      
      evsk = 0;
      olen = sizeof(evsk);
      r = nn_getsockopt(nn, NN_SOL_SOCKET, NN_RCVFD, &evsk, &olen);
      if (r) {
	int saverr = errno;
	struct rlimit orlim = { 0 };
	getrlimit(RLIMIT_NOFILE, &orlim);
	RWMSG_TRACE(ss->ep, ERROR, "rwmsg_socket_connect/nn_getsockopt(NN_RCVFD) err='%s' rlim.cur=%u/.max=%u\n", strerror(saverr), (unsigned int)orlim.rlim_cur, (unsigned int)orlim.rlim_max);
	goto bail;
      }
      RW_ASSERT(evsk >= 0);
      sk->evfd_in = evsk;
      ck_pr_inc_32(&ss->ep->stat.objects.nn_eventfds);
      
      RWMSG_TRACE(ss->ep, DEBUG, "RWMSG_NNSK_CONNECTING(ss=%p,pri=%d) nnsk=%d SUCCESS", ss, pri, nn);

      sk->sk = nn;
      sk->state = RWMSG_NNSK_CONNECTING;
      sk->pri = pri;

      struct nn_riftclosure nn_cl;
      nn_cl.ud = sk;
      nn_cl.fn = rwmsg_nn_connection_indicationn_cb;
      r = nn_setsockopt(nn, NN_SOL_SOCKET, NN_RWSET_CONN_IND, &nn_cl, sizeof(nn_cl));

      rs = rwmsg_sockset_bind_nnsk(ss, sk, pri);
    }
    rs = RW_STATUS_SUCCESS;
    break;

  default:
    rs = RW_STATUS_FAILURE;
    goto out;
    break;
  }
  
 out:
  if (rs == RW_STATUS_SUCCESS) {
    ss->sk_mask |= (1<<pri);
  }
  return rs;
  
 bail:
  rs = RW_STATUS_FAILURE;
  return rs;
}


rwmsg_bool_t rwmsg_sockset_paused_p(rwmsg_sockset_t *ss, rwmsg_priority_t pri) {
  struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
  return !! sk->paused;
}

void rwmsg_sockset_pause_pri(rwmsg_sockset_t *ss, rwmsg_priority_t pri) {
  struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
  if (!sk->paused) {
    sk->paused = 1;
    rwmsg_notify_pause(ss->ep, &sk->notein);
  }
}

void rwmsg_sockset_resume_pri(rwmsg_sockset_t *ss, rwmsg_priority_t pri) {
  struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
  if (sk->paused) {
    sk->paused = 0;
    rwmsg_notify_resume(ss->ep, &sk->notein);
#ifdef NN_DIRECT_CB
    if (sk->cb_while_paused) {
      sk->cb_while_paused = 0;
      if (rwmsg_global.cbs[sk->evfd_in].notify) {
        if (ss->sched.rwsqueue) {
          rwsched_dispatch_async_f(ss->ep->taskletinfo, ss->sched.rwsqueue, &rwmsg_global.cbs[sk->evfd_in], rwmsg_nn_direct_dispatch_f);
        }
        else {
          rwsched_dispatch_async_f(ss->ep->taskletinfo, rwsched_dispatch_get_main_queue(ss->sched.rwsched), &rwmsg_global.cbs[sk->evfd_in], rwmsg_nn_direct_dispatch_f);
        }
      }
    }
#endif
  }
}

void rwmsg_sockset_pollout(rwmsg_sockset_t *ss, rwmsg_priority_t pri, rwmsg_bool_t onoff) {
  struct rwmsg_sockset_nnsk_s *sk = &ss->nn.sk[pri];
  RWMSG_TRACE(ss->ep, DEBUG, "sockset_pollout(ss=%p, pri=%d, onoff=%d) current sk->pollout=%d notify=%p notify->paused=%d notify->halted=%d evfd=%d", 
	      ss, pri, onoff, sk->pollout, &sk->noteout, sk->noteout.paused, sk->noteout.halted, sk->evfd_out);
  if (onoff) {
    sk->pollout = TRUE;
    rwmsg_notify_resume(ss->ep, &sk->noteout);
  } else {
    sk->pollout = FALSE;
    rwmsg_notify_pause(ss->ep, &sk->noteout);
  }
  return;
}


rw_status_t rwmsg_sockset_connect(rwmsg_sockset_t *ss, const char *nnuri, int ct) {
  RW_ASSERT(ss);
  RW_ASSERT_TYPE(ss, rwmsg_sockset_t);
  RW_ASSERT(ct <= RWMSG_PRIORITY_COUNT);
  RW_ASSERT(ss->sched.theme == RWMSG_NOTIFY_CFRUNLOOP_FD
	    || ss->sched.theme == RWMSG_NOTIFY_RWSCHED_FD
	    || ss->sched.theme == RWMSG_NOTIFY_NONE);
  
  rw_status_t rs = RW_STATUS_FAILURE;
  int pri;

  ss->sk_ct=ct;
  for (pri=0; pri < ct; pri++) {
    rs = rwmsg_sockset_connect_pri(ss, nnuri, pri);
    if (rs != RW_STATUS_SUCCESS) {
      goto bail;
    }
  }
  
  return rs;
  
 bail:
  rwmsg_sockset_closeall(ss);
  rs = RW_STATUS_FAILURE;
  return rs;
}

