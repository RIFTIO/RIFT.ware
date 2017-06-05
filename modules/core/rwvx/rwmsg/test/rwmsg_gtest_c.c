
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <stdio.h>

#include <rwmsg_int.h>

#include "rwmsg_gtest_c.h"

#define ASSERT_TRUE(x) {						\
  if (!(x)) {								\
    retval = FALSE;							\
    fprintf(stderr, "ASSERT fail at %s:%d in %s\n", __FILE__, __LINE__, __FUNCTION__); \
    goto retnow;							\
  }									\
}

int rwmsg_gtest_queue_create_delete(rwmsg_endpoint_t *ep) {

  int retval = TRUE;

  rw_status_t r;
  rwmsg_queue_t q;
  r = rwmsg_queue_init(ep, &q, "test0", NULL);
  ASSERT_TRUE(r == RW_STATUS_SUCCESS);

  ASSERT_TRUE(q.qszcap == RWMSG_QUEUE_DEFSZ);
  ASSERT_TRUE(q.qlencap == RWMSG_QUEUE_DEFLEN);

  rwmsg_endpoint_status_t *epstat=NULL;
  rwmsg_endpoint_get_status_FREEME(ep, &epstat);
  ASSERT_TRUE(epstat->objects.queues == 1);
  rwmsg_endpoint_get_status_free(epstat);
  epstat = NULL;

  {
    rwmsg_queue_t q2;
    rwmsg_endpoint_set_property_int(ep, "/rwmsg/queue/test0/qlen", 50);
    rwmsg_endpoint_set_property_int(ep, "/rwmsg/queue/test0/qsz", 500*1024);
    r = rwmsg_queue_init(ep, &q2, "test0", NULL);
    ASSERT_TRUE(r == RW_STATUS_SUCCESS);
    ASSERT_TRUE(q2.qszcap == 500*1024);
    ASSERT_TRUE(q2.qlencap == 50);

    rwmsg_endpoint_get_status_FREEME(ep, &epstat);
    ASSERT_TRUE(epstat->objects.queues == 2);
    rwmsg_endpoint_get_status_free(epstat);
    epstat = NULL;

    rwmsg_global_status_t gs;
    rwmsg_global_status_get(&gs);
    ASSERT_TRUE(gs.qent == 0);

    rwmsg_queue_deinit(ep, &q2);
    rwmsg_endpoint_get_status_FREEME(ep, &epstat);
    ASSERT_TRUE(epstat->objects.queues == 1);
    rwmsg_endpoint_get_status_free(epstat);
    epstat = NULL;
  }

  rwmsg_queue_deinit(ep, &q);
  rwmsg_endpoint_get_status_FREEME(ep, &epstat);
  ASSERT_TRUE(epstat->objects.queues == 0);
  rwmsg_endpoint_get_status_free(epstat);
  epstat = NULL;

 retnow:
  return retval;
}


int rwmsg_gtest_queue_queue_dequeue(rwmsg_endpoint_t *ep) {
  int retval = TRUE;
  rwmsg_global_status_t gs;

  rwmsg_request_t *r1, *r2, *r3, *r4;
  
  r1 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r2 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r3 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r4 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);

  r1->hdr.isreq=TRUE;
  r2->hdr.isreq=TRUE;
  r3->hdr.isreq=TRUE;
  r4->hdr.isreq=TRUE;
  r1->hdr.paysz=1200;
  r2->hdr.paysz=1200;
  r3->hdr.paysz=1200;
  r4->hdr.paysz=1200;

  /* Mark the priorities backwards from name ordering */
  r1->hdr.pri=2;
  r2->hdr.pri=1;
  r3->hdr.pri=0;
  r4->hdr.pri=0;

  r1->refct=1;
  r2->refct=1;
  r3->refct=1;
  r4->refct=1;

  rwmsg_endpoint_set_property_int(ep, "/rwmsg/queue/test1/qlen", 3);
  rwmsg_endpoint_set_property_int(ep, "/rwmsg/queue/test1/qsz", 3*1024);
  rwmsg_queue_t q;
  rw_status_t s = rwmsg_queue_init(ep, &q, "test1", NULL);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);

  rwmsg_request_t *dq;

  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 0);

  /* Enqueue in the opposite of priority order */
  s = rwmsg_queue_enqueue(&q, r3);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  ASSERT_TRUE(r3->refct == 2);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 1);
  s = rwmsg_queue_enqueue(&q, r2);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  ASSERT_TRUE(r2->refct == 2);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 2);
  s = rwmsg_queue_enqueue(&q, r1); /* this puts us over 3*1024, one request over is OK */
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  ASSERT_TRUE(r1->refct == 2);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 3);

  /* Now dequeue the three, they should come back in priority order,
     the opposite of name and queuing order. */
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r1);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 2);

  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r2);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 1);

  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r3);
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.qent == 0);
  if (gs.qent) {
    fprintf(stderr, "Residual gs.qent=%u, this leak is ticket RW-20\n", gs.qent);
  }

  /* Now try to enqueue more bytes than are allowed (3*1024) */
  r1->hdr.pri=0;
  r2->hdr.pri=0;
  r3->hdr.pri=0;
  r4->hdr.pri=0;
  s = rwmsg_queue_enqueue(&q, r3);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r2);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r4); /* we are already over 3*1024 when we try to add this one, no go */
  ASSERT_TRUE(s == RW_STATUS_BACKPRESSURE);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r3);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r2);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r1);


  /* Now try to enqueue more requests than are allowed (3) */
  r1->hdr.paysz=10;
  r2->hdr.paysz=10;
  r3->hdr.paysz=10;
  r4->hdr.paysz=10;
  s = rwmsg_queue_enqueue(&q, r3);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r2);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  s = rwmsg_queue_enqueue(&q, r4); /* this is the fourth message */
  ASSERT_TRUE(s == RW_STATUS_BACKPRESSURE);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r3);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r2);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r1);

 retnow:
  RW_FREE_TYPE(r1, rwmsg_request_t);
  RW_FREE_TYPE(r2, rwmsg_request_t);
  RW_FREE_TYPE(r3, rwmsg_request_t);
  RW_FREE_TYPE(r4, rwmsg_request_t);
  rwmsg_queue_deinit(ep, &q);
  return retval;
  ep=ep;
}

int rwmsg_gtest_queue_pollable_event(rwmsg_endpoint_t *ep) {
  int retval = TRUE;

  struct pollfd pset[99];
  int psetlen = 0;
  int r;
  int again;
  
  rwmsg_notify_t extnotif = { .theme = RWMSG_NOTIFY_EVENTFD };
  rwmsg_notify_init(ep, &extnotif, "extnotif (rwmsg_gtest_queue_pollable_event)", NULL);

#define WANTEVENTCT(wantct) {					\
  psetlen = rwmsg_notify_getpollset(q.notify, &pset[0], 99);	\
  ASSERT_TRUE(psetlen >= 1);					\
  do {								\
    again = FALSE;						\
    errno = 0;							\
    r = poll(pset, psetlen, 0);					\
    if (r < 0) {						\
      switch (errno) {						\
      case EAGAIN:						\
      case EINTR:						\
	again=TRUE;						\
	break;							\
      default:							\
	ASSERT_TRUE(errno == 0);				\
	break;							\
      }								\
    } else {							\
      ASSERT_TRUE(r == (wantct));				\
      if ((wantct) > 0) {					\
	ASSERT_TRUE((pset[0].revents & POLLIN));		\
      }								\
    }								\
  } while (again);						\
}

  rwmsg_request_t *r1, *r2;
  
  r1 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r1->refct=1;
  r1->hdr.isreq=TRUE;
  r1->hdr.paysz=1200;
  r1->hdr.pri=3;
  r2 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r2->refct=1;
  r2->hdr.isreq=TRUE;
  r2->hdr.paysz=1200;
  r2->hdr.pri=3;

  rwmsg_queue_t q;
  rw_status_t s;
  rwmsg_request_t *dq;

  s = rwmsg_queue_init(ep, &q, "test", &extnotif);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  WANTEVENTCT(0);

  s = rwmsg_queue_enqueue(&q, r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  WANTEVENTCT(1);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r1);
  WANTEVENTCT(0);

  s = rwmsg_queue_enqueue(&q, r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  WANTEVENTCT(1);
  s = rwmsg_queue_enqueue(&q, r2);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  WANTEVENTCT(1);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r1);
  WANTEVENTCT(1);
  dq = rwmsg_queue_dequeue(&q);
  ASSERT_TRUE(dq == r2);
  WANTEVENTCT(0);
  
 retnow:
  RW_FREE_TYPE(r1, rwmsg_request_t);
  RW_FREE_TYPE(r2, rwmsg_request_t);
  rwmsg_queue_deinit(ep, &q);

  rwmsg_notify_deinit(ep, &extnotif);

  return retval;
  ep=ep;
}


int rwmsg_gtest_srvq(rwmsg_srvchan_t *sc) {
  RW_ASSERT(sc->ch.localq.notify == &sc->ch.localq._notify);
  RW_ASSERT(sc->ch.localq.notify->theme == RWMSG_NOTIFY_NONE);

  int rval = 0;
  rwmsg_request_t *r1;
  r1 = RW_MALLOC0_TYPE(sizeof(*r1), rwmsg_request_t);
  r1->refct=1;
  r1->hdr.isreq=TRUE;
  r1->hdr.paysz=1200;
  r1->hdr.pri=0;
  rw_status_t retval = rwmsg_queue_enqueue(&sc->ch.localq, r1);
  if (retval != RW_STATUS_SUCCESS) {
    goto ret;
  }
  rwmsg_request_t *dq = rwmsg_queue_dequeue(&sc->ch.localq);
  rval = (dq == r1);

  RW_FREE_TYPE(r1, rwmsg_request_t);
  r1 = NULL;

 ret:
  return rval;
}
