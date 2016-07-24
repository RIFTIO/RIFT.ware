
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_queue.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG message queue
 */

#include "rwmsg_int.h"

rw_status_t rwmsg_queue_init(rwmsg_endpoint_t *ep,
			     rwmsg_queue_t *q,
			     const char *flavor,
			     rwmsg_notify_t *notify) {
  memset(q, 0, sizeof(*q));
  q->magic = RW_HASH_TYPE(rwmsg_queue_t);
  q->ep = ep;
  q->qlen = 0;
  q->qsz = 0;

  rwmsg_queue_set_cap_default(q, flavor);

  int pri=0;
  for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
    struct ck_fifo_mpmc_entry *head = RW_MALLOC(sizeof(*head));
    ck_fifo_mpmc_init(&q->fifo[pri], head);
  }

  if (notify) {
    q->notify = notify;
  } else {
    q->_notify.theme = RWMSG_NOTIFY_NONE;
    if (RW_STATUS_SUCCESS != rwmsg_notify_init(ep, &q->_notify, "queue (NONE)", NULL)) {
      goto lockret;
    }
    q->notify = &q->_notify;
  }

  RWMSG_EP_LOCK(ep);
  RW_DL_INSERT_HEAD(&ep->track.queues, q, trackelem);
  RWMSG_EP_UNLOCK(ep);

  ck_pr_inc_32(&ep->stat.objects.queues);

  return RW_STATUS_SUCCESS;

 lockret:
  return RW_STATUS_FAILURE;
}

void rwmsg_queue_set_cap(rwmsg_queue_t *q,
			 int len,
			 int size) {
  RW_ASSERT(q->magic == RW_HASH_TYPE(rwmsg_queue_t));
  q->qszcap = size;
  q->qlencap = len;
}

void rwmsg_queue_set_cap_default(rwmsg_queue_t *q,
				 const char *flavor) {
  RW_ASSERT(strlen(flavor) < 128);
  int32_t deflen = RWMSG_QUEUE_DEFLEN;
  char ppath[512];
  sprintf(ppath, "/rwmsg/queue/%s/qlen", flavor);
  rwmsg_endpoint_get_property_int(q->ep, ppath, &deflen);

  q->qlencap = deflen;

  RW_ASSERT(strlen(flavor) < 128);
  deflen = RWMSG_QUEUE_DEFSZ;
  sprintf(ppath, "/rwmsg/queue/%s/qsz", flavor);
  rwmsg_endpoint_get_property_int(q->ep, ppath, &deflen);

  q->qszcap = deflen;
}

void rwmsg_queue_deinit(rwmsg_endpoint_t *ep,
			rwmsg_queue_t *q) {

  RW_ASSERT(q->magic == RW_HASH_TYPE(rwmsg_queue_t));
  q->magic = 0;

  //?? RW_ASSERT(q->qsz == 0);
  RW_ASSERT(q->qlen == 0);

  if (q->notify == &q->_notify) {
    rwmsg_notify_deinit(ep, &q->_notify);
  } else {
    /* External notifier, so we don't deinit */
    q->notify = NULL;
  }

  int pri;
  for (pri=0; pri<RWMSG_PRIORITY_COUNT; pri++) {
    RW_ASSERT(CK_FIFO_MPMC_ISEMPTY(&q->fifo[pri]));
    struct ck_fifo_mpmc_entry *junk=NULL;
    ck_fifo_mpmc_deinit(&q->fifo[pri], &junk);
    RW_ASSERT(junk);
    RW_FREE(junk);
  }

  q->full = 0;

  RWMSG_EP_LOCK(ep);
  RW_DL_REMOVE(&ep->track.queues, q, trackelem);
  RWMSG_EP_UNLOCK(ep);

  ck_pr_dec_32(&ep->stat.objects.queues);

  return;
}

void rwmsg_queue_pause(rwmsg_endpoint_t *ep,
		       rwmsg_queue_t *q) {
  if (!q->paused) {
    q->paused = TRUE;
    if (q->notify) {
      rwmsg_notify_pause(ep, q->notify);
    }
  }
}

void rwmsg_queue_resume(rwmsg_endpoint_t *ep,
			rwmsg_queue_t *q) {
  if (q->paused) {
    if (q->notify) {
      rwmsg_notify_resume(ep, q->notify);
    }
    q->paused = FALSE;
  }
}

void rwmsg_queue_set_notify(rwmsg_endpoint_t *ep,
			    rwmsg_queue_t *q,
			    rwmsg_notify_t *notify) {
  if (q->notify == &q->_notify) {
    /* Internally-provided null notify */
    rwmsg_notify_deinit(ep, q->notify);
  }
  if (notify) {
    q->notify = notify;
    q->detickle = 1;
    rwmsg_notify_raise(q->notify); /* tickle! */
  } else {
    q->_notify.theme = RWMSG_NOTIFY_NONE;
    if (RW_STATUS_SUCCESS != rwmsg_notify_init(ep, &q->_notify, "queue (NONE)", NULL)) {
      RW_CRASH();
    }
    q->notify = &q->_notify;
  }
}

void rwmsg_queue_register_feedme(rwmsg_queue_t *q,
				 rwmsg_closure_t *cb) {
  if (cb) {
    memcpy(&q->feedme, cb, sizeof(q->feedme));
  } else {
    memset(&q->feedme, 0, sizeof(q->feedme));
  }
}

rw_status_t rwmsg_queue_enqueue(rwmsg_queue_t *q, 
				rwmsg_request_t *req) {
  RW_ASSERT(q->magic == RW_HASH_TYPE(rwmsg_queue_t));
  RW_ASSERT(req->hdr.pri < RWMSG_PRIORITY_COUNT);

  //... len/sz check, requests only
  //if (req->hdr.isreq && !req->hdr.peerchan) {
  if (req->hdr.isreq) {
    if ((q->qszcap && q->qsz >= q->qszcap)
	|| (q->qlencap && q->qlen >= q->qlencap)) {
      ck_pr_inc_32(&q->full);
      return RW_STATUS_BACKPRESSURE;
    }
  }

  //... len/sz/stats
  uint32_t mlen = req->hdr.paysz;
  ck_pr_inc_32(&q->qlen);
  ck_pr_add_32(&q->qsz, mlen);
  ck_pr_fence_store();

  /* Well this is unsatisfactory.  Apparently the qent can't live
     inside the rwmsg_request_t, since the returned one isn't the one
     that went in with this request.  All a hassle, need to switch to
     a ring. */
  ck_fifo_mpmc_entry_t *qent = RW_MALLOC(sizeof(*qent)); 
  ck_pr_inc_32(&rwmsg_global.status.qent);

  // ...raise event
  if (q->notify) {
    rwmsg_notify_raise(q->notify);
  }

  ck_pr_inc_32(&req->refct);
  ck_fifo_mpmc_enqueue(&q->fifo[req->hdr.pri], qent, req);

  return RW_STATUS_SUCCESS;
}

rwmsg_request_t *rwmsg_queue_dequeue(rwmsg_queue_t *q) {
  uint32_t magic = RW_HASH_TYPE(rwmsg_queue_t);
  RW_ASSERT(q->magic == magic);
  // some ffs nonsense would be nicer, also there is lockless timing
  // awkwardness between the ck_fifo op, qlen, and notify clear/raise
  if (q->detickle) {
    if (q->notify) {
      rwmsg_notify_clear(q->notify, 1);
    }
    q->detickle = 0;
  }
  int p;
  rwmsg_request_t *retval;
  for (p=RWMSG_PRIORITY_COUNT-1; p>=0; p--) {
    retval = rwmsg_queue_dequeue_pri(q, p);
    if (retval) {
      goto ret;
    }
  }
 ret:
  return retval;
}

static rwmsg_request_t *rwmsg_queue_dequeue_pri_int(rwmsg_queue_t *q, 
						    rwmsg_priority_t pri,
						    uint32_t spin,
						    bool noevap) {
  uint32_t magic = RW_HASH_TYPE(rwmsg_queue_t);
  RW_ASSERT(q->magic == magic);
  bool rb = FALSE;
  rwmsg_request_t *req = NULL;
  struct ck_fifo_mpmc_entry *garbage=NULL;

  if (q->detickle) {
    if (q->notify) {
      rwmsg_notify_clear(q->notify, 1);
    }
    q->detickle = 0;
  }
 evaporated:
  req=NULL;
  garbage=NULL;
  if (spin) {
    rb = ck_fifo_mpmc_dequeue(&q->fifo[pri], &req, &garbage);
  } else {
    rb = ck_fifo_mpmc_trydequeue(&q->fifo[pri], &req, &garbage);
  }
  if (rb) {
    RW_ASSERT(req);
    ck_pr_dec_32(&rwmsg_global.status.qent);
    RW_ASSERT(garbage);
    RW_FREE(garbage);

    uint32_t mlen = req->hdr.paysz;
    ck_pr_dec_32(&q->qlen);
    RW_ASSERT(q->qsz >= mlen);
    ck_pr_add_32(&q->qsz, -mlen);
    ck_pr_fence_store();

    if (q->notify) {
      rwmsg_notify_clear(q->notify, 1);
    }

    if (q->full) {
      ck_pr_dec_32(&q->full);
      if (q->feedme.qfeed) {
	q->feedme.qfeed(q->feedme.ud);
      }
    }

    if (rwmsg_request_release(req)) {
      /* Request can be freed on dequeue! */
      req = NULL;
      if (!noevap) {
	goto evaporated;
      }
    }

  } else {
    RW_ASSERT(!req);
  }
  return req;
}
rwmsg_request_t *rwmsg_queue_dequeue_pri(rwmsg_queue_t *q, 
					 rwmsg_priority_t pri) {
  return rwmsg_queue_dequeue_pri_int(q, pri, FALSE, FALSE);
}
rwmsg_request_t *rwmsg_queue_dequeue_pri_spin(rwmsg_queue_t *q, 
					      rwmsg_priority_t pri) {
  return rwmsg_queue_dequeue_pri_int(q, pri, TRUE, FALSE);
}

rwmsg_request_t *rwmsg_queue_head_pri(rwmsg_queue_t *q, 
				      rwmsg_priority_t pri) {
  RW_ASSERT(q->magic == RW_HASH_TYPE(rwmsg_queue_t));
  if (q->detickle) {
    if (q->notify) {
      rwmsg_notify_clear(q->notify, 1);
    }
    q->detickle = 0;
  }
  rwmsg_request_t *req = NULL;
  struct ck_fifo_mpmc_entry *qent = NULL;

 evaporated:
  qent = CK_FIFO_MPMC_FIRST(&q->fifo[pri]);
  if (qent) {
    req = (rwmsg_request_t*) qent->value;
  }
  if (req && req->refct == 1) {
    /* The head request has a ref count of 1, so will evaporate when
       we pop it.  Pop it now to get rid of it and try again. */
    req = rwmsg_queue_dequeue_pri_int(q, pri, TRUE, TRUE);
    RW_ASSERT(!req);
    goto evaporated;
  }
  return req;
}
