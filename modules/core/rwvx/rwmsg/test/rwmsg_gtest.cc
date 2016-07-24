
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwmsg_gtest.cc
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwmsg
 * 
 * @details Google test cases for testing rwmsg. Specifically
 * this file includes test cases for lotsa stuff(tm).
 */

/** 
 * Step 1: Include the necessary header files 
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <limits.h>
#include <cstdlib>
#include <sys/resource.h>

#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"

#include <unistd.h>
#include <sys/syscall.h>

#include <rwmsg_int.h>
#include "rwmsg_gtest_c.h"

#include "test.pb-c.h"

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

static void nnchk() {
  /* Gratuitous socket open/close; the close will delete any riftcfg
     in the nn library unless there was a leaked nn socket from a
     preceeding test. */
  int sk = nn_socket (AF_SP, NN_PAIR);
  nn_close(sk);

  /* This gets cleared when the last nn sock is closed, has to happen... */
  struct nn_riftconfig rcfg;
  nn_global_get_riftconfig(&rcfg);
  ASSERT_FALSE(rcfg.singlethread);
  ASSERT_TRUE(rcfg.waitfunc == NULL);
}

TEST(RWMsgAPI, PlaceholderNOP) {
  TEST_DESCRIPTION("Tests nothing whatsoever");
}

/* Test and tasklet environment */
class rwmsg_tenv_t {
public:
#define TASKLET_MAX (10)
  rwmsg_tenv_t(int tasklet_ct_in=0) {
    //assert(!(tasklet_ct_in&1));
    assert(tasklet_ct_in <= TASKLET_MAX);
    rwsched = rwsched_instance_new();
    tasklet_ct = tasklet_ct_in;
    memset(&tasklet, 0, sizeof(tasklet));
    for (int i=0; i<tasklet_ct; i++) {
      tasklet[i] = rwsched_tasklet_new(rwsched);
    }

    /* Set broker feature off for now, unless someone already has this
       variable set... */
    setenv("RWMSG_BROKER_ENABLE", "0", FALSE);
  }
  ~rwmsg_tenv_t() {
    if (rwsched) {
      for (int i=0; i<tasklet_ct; i++) {
	rwsched_tasklet_free(tasklet[i]);
	tasklet[i] = NULL;
      }
      rwsched_instance_free(rwsched);
      rwsched = NULL;
    }
  }

  rwsched_instance_ptr_t rwsched;
  int tasklet_ct;
  rwsched_tasklet_ptr_t tasklet[TASKLET_MAX];
};


TEST(RWMsgAPI, EndpointCreateDelete) {
  TEST_DESCRIPTION("Tests rwsched+endpoint creation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

TEST(RWMsgAPI, Properties) {
  TEST_DESCRIPTION("Tests get/set of endpoint property API");

  rwmsg_tenv_t tenv(1);
  ASSERT_TRUE(tenv.rwsched);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rw_status_t s;
  int i=0;
  char buf[100];

  s = rwmsg_endpoint_get_property_int(ep, "/prop/foo", &i);
  ASSERT_EQ(s, RW_STATUS_NOTFOUND);

  s = rwmsg_endpoint_get_property_string(ep, "/prop/bar", buf, sizeof(buf));
  ASSERT_EQ(s, RW_STATUS_NOTFOUND);
  
  rwmsg_endpoint_set_property_int(ep, "/prop/foo", 10);
  rwmsg_endpoint_set_property_string(ep, "/prop/bar", "ten");

  s = rwmsg_endpoint_get_property_int(ep, "/prop/foo", &i);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);
  ASSERT_EQ(i, 10);

  buf[0]='\0';
  s = rwmsg_endpoint_get_property_string(ep, "/prop/bar", buf, sizeof(buf));
  ASSERT_EQ(s,RW_STATUS_SUCCESS);
  ASSERT_EQ(0,strcmp(buf, "ten"));

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

TEST(RWMsgAPI, QueueCreateDeleteSize) {
  TEST_DESCRIPTION("Tests queue creation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  int b = rwmsg_gtest_queue_create_delete(ep);
  ASSERT_TRUE(b);

  rwmsg_bool_t rb = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(rb);
  nnchk();
}

TEST(RWMsgAPI, QueueQueueDequeue) {
  TEST_DESCRIPTION("Tests basic queue operation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  ASSERT_EQ(gs.qent, 0);

  int b = rwmsg_gtest_queue_queue_dequeue(ep);
  ASSERT_TRUE(b);

  rwmsg_global_status_get(&gs);
  ASSERT_EQ(gs.qent, 0);

  rwmsg_bool_t rb = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(rb);

  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();
}

TEST(RWMsgAPI, QueuePollableEvent) {
  TEST_DESCRIPTION("Tests queue synthetic pollable event");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  int b = rwmsg_gtest_queue_pollable_event(ep);
  ASSERT_TRUE(b);

  rwmsg_bool_t rb = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(rb);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();
}

static void reqcb1(rwmsg_request_t *req, void *ud) {
  req=req;
  ud=ud;
}

TEST(RWMsgAPI, SignatureMethodCreation) {
  TEST_DESCRIPTION("Tests sig and method creation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const uint32_t methno = __LINE__;

  rwmsg_signature_t *sig;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/1";

  rwmsg_bool_t r;

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  //...

  rwmsg_method_release(ep, meth);
  rwmsg_signature_release(ep, sig);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}


TEST(RWMsgAPI, SrvchanCreateBind) {
  TEST_DESCRIPTION("Tests sig and method creation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const uint32_t methno = __LINE__;

  rwmsg_signature_t *sig;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/2";

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);

  int ctest = rwmsg_gtest_srvq(sc);
  ASSERT_TRUE(ctest);

  rw_status_t rs;
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Verify that the binding worked by doing a global lookup as though we were a client */
  rwmsg_srvchan_t *clisc;
  uint64_t phash = rw_hashlittle64(taddr, strlen(taddr));
  clisc = (rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, 0, phash, RWMSG_PAYFMT_RAW, methno);
  ASSERT_TRUE(clisc);
  ASSERT_TRUE(clisc == sc);
  if (clisc) {
    rwmsg_srvchan_release(clisc);
  }
  
  rwmsg_bool_t r;
  rwmsg_srvchan_halt(sc);
  rwmsg_method_release(ep, meth);
  rwmsg_signature_release(ep, sig);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

TEST(RWMsgAPI, SrvchanCreateBindTwice) {
  TEST_DESCRIPTION("Tests sig and method creation");

  rwmsg_tenv_t tenv(2);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  rwmsg_endpoint_t *ep2;
  ep2 = rwmsg_endpoint_create(1, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep2 != NULL);

  const uint32_t methno = __LINE__;
  const uint32_t methno2 = __LINE__;

  rwmsg_signature_t *sig;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  rwmsg_signature_t *sig2;
  sig2 = rwmsg_signature_create(ep2, RWMSG_PAYFMT_RAW, methno2, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig2 != NULL);

  // same path in both, should fail
  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/2";

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);
  rwmsg_method_t *meth2 = rwmsg_method_create(ep2, taddr, sig2, &cb);
  ASSERT_TRUE(meth2 == NULL);

  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);

  int ctest = rwmsg_gtest_srvq(sc);
  ASSERT_TRUE(ctest);

  rw_status_t rs;
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Verify that the binding worked by doing a global lookup as though we were a client */
  rwmsg_srvchan_t *clisc;
  uint64_t phash = rw_hashlittle64(taddr, strlen(taddr));
  clisc = (rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, 0, phash, RWMSG_PAYFMT_RAW, methno);
  ASSERT_TRUE(clisc);
  ASSERT_TRUE(clisc == sc);
  if (clisc) {
    rwmsg_srvchan_release(clisc);
  }
  
  rwmsg_bool_t r;
  rwmsg_srvchan_halt(sc);
  rwmsg_method_release(ep, meth);
  rwmsg_signature_release(ep, sig);
  rwmsg_signature_release(ep2, sig2);
  r = rwmsg_endpoint_halt(ep2);
  ASSERT_TRUE(r);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

TEST(RWMsgAPI, ClichanCreate) {
  TEST_DESCRIPTION("Tests dest and clichan creation");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/3";

  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);

  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);


  
  rwmsg_bool_t r;
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

static void reqcb2(rwmsg_request_t *req, void *ud) {
  req=req;
  ud=ud;
}
TEST(RWMsgAPI, RequestCreate) {
  TEST_DESCRIPTION("Tests request creation and raw payload population");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/4";

  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);

  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);

  const uint32_t methno = __LINE__;

  rwmsg_signature_t *sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);

  rwmsg_closure_t cb;
  cb.request=reqcb2;
  cb.ud=NULL;

  rwmsg_request_t *req = rwmsg_request_create(cc);
  ASSERT_TRUE(req != NULL);
  rwmsg_request_set_signature(req, sig);
  rwmsg_request_set_callback(req, &cb);

  const char *payload = "hi there";
  rwmsg_request_set_payload(req, payload, 1+strlen(payload));
  
  uint32_t getlen=0;
  const char *getpay = (char*)rwmsg_request_get_request_payload(req, &getlen);
  ASSERT_TRUE(getlen == 1+strlen(payload));
  ASSERT_TRUE(0 == memcmp(payload, getpay, getlen));

  rwmsg_bool_t r;
  rwmsg_request_release(req);
  rwmsg_signature_release(ep, sig);
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();
}

static void reqcb3(rwmsg_request_t *req, void *ud) {
  /* response callback for RequestSend */
  req=req;
  uint32_t *pval = (uint32_t*)ud;
  RW_ASSERT(pval);
  (*pval)++;

  static uint32_t invoke=0;
  invoke++;

  uint32_t len=0;
  const char *rsp = (const char*)rwmsg_request_get_response_payload(req, &len);
  if (rsp) {
//    fprintf(stderr, "got rsp len=%d '%s' cb3 invoke=%u\n", len, rsp, invoke);
  } else {
    fprintf(stderr, "req had no rsp payload!?\n");
  }
}
static void reqcb4(rwmsg_request_t *req, void *ud) {
  /* Method callback for RequestSend */
  req=req;
  //  fprintf(stderr, "Huzzah, got request %p\n", req);
  uint32_t *pval = (uint32_t*)ud;
  RW_ASSERT(pval);
  (*pval)++;

  static uint32_t invoke=0;
  invoke++;

  char payload[100];
  sprintf(payload, "hi to you, too; cb4 invoke=%u", invoke);

  rwmsg_request_send_response(req, payload, 1+strlen(payload));
}
TEST(RWMsgAPI, RequestLocalRoundtrip) {
  TEST_DESCRIPTION("Tests sending a request to a local srvchan");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/5";

  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);

  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);

  const uint32_t methno = __LINE__;

  rwmsg_signature_t *sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);

  uint32_t reqcb3_invoked=0;
  rwmsg_closure_t cb;
  cb.request=reqcb3;
  cb.ud=&reqcb3_invoked;

  rwmsg_request_t *req = rwmsg_request_create(cc);
  ASSERT_TRUE(req != NULL);
  rwmsg_request_set_signature(req, sig);
  rwmsg_request_set_callback(req, &cb);

  const char *payload = "hi there";
  rwmsg_request_set_payload(req, payload, 1+strlen(payload));
  

  /* Now create srvchan, sort of tests deferred binding... */
  uint32_t reqcb4_invoked=0;
  cb.request=reqcb4;
  cb.ud=&reqcb4_invoked;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);

  rw_status_t rs;
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Now send the request */
  rs = rwmsg_clichan_send(cc, dt, req);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  // should be on sc's queue
  ASSERT_TRUE(reqcb4_invoked == 0);
  uint32_t ct = rwmsg_srvchan_recv(sc);
  ASSERT_TRUE(ct == 1);
  ASSERT_TRUE(reqcb4_invoked == 1);

  // now no new requests
  ASSERT_TRUE(reqcb4_invoked == 1);
  ct = rwmsg_srvchan_recv(sc);
  ASSERT_TRUE(ct == 0);
  ASSERT_TRUE(reqcb4_invoked == 1);
  ct = rwmsg_srvchan_recv(sc);
  ASSERT_TRUE(ct == 0);
  ASSERT_TRUE(reqcb4_invoked == 1);

  //TBD should get an answer via clichan_recv()...
  ASSERT_TRUE(reqcb3_invoked == 0);
  ct = rwmsg_clichan_recv(cc);
  ASSERT_TRUE(ct == 1);
  ASSERT_TRUE(reqcb3_invoked == 1);

  ct = rwmsg_clichan_recv(cc);
  req=NULL;			// end request lifespan
  ASSERT_TRUE(ct == 0);
  ASSERT_TRUE(reqcb3_invoked == 1);


  rwmsg_bool_t r;
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(sc);
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  ASSERT_TRUE(gs.endpoint_ct == 0);
  nnchk();
}

struct qtest1_context {
  rwmsg_queue_t q;
  rwmsg_request_t *r1, *r2;

  uint32_t in;
  uint32_t out;
  uint32_t miss;
};
void qtest_cb1(void *ctx_in, int bits) {
  RW_ASSERT_TYPE(ctx_in, qtest1_context);
  struct qtest1_context *ctx = (struct qtest1_context*)ctx_in;

  /* rwmsg notify callback.  this would normally be notify on a
     srvchan or clichan, in which case the srvchan or clichan specific
     callback would call it's own read or write. */

  if (ctx->in == 1) {
    ck_pr_inc_32(&ctx->in);
    rw_status_t s = rwmsg_queue_enqueue(&ctx->q, ctx->r1);
    ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  }

  rwmsg_request_t *dq;

  dq = rwmsg_queue_dequeue(&ctx->q);
  ASSERT_TRUE(dq == NULL || dq == ctx->r1 || dq == ctx->r2);

  if (dq) {
    ck_pr_inc_32(&ctx->out);
  } else {
    ck_pr_inc_32(&ctx->miss);
  }

  return;
  bits=bits;
}
TEST(RWMsgAPI, QueueRWSchedEvent) {
  TEST_DESCRIPTION("Tests queue rwsched queue event");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  struct qtest1_context *ctx = RW_MALLOC0_TYPE(sizeof(*ctx), qtest1_context);
  
  ctx->r1 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*ctx->r1), rwmsg_request_t);
  ctx->r1->refct = 1;
  ctx->r1->hdr.isreq=TRUE;
  ctx->r1->hdr.paysz=1200;
  ctx->r1->hdr.pri=RWMSG_PRIORITY_HIGH;
  ctx->r2 = (rwmsg_request_t*)RW_MALLOC0_TYPE(sizeof(*ctx->r2), rwmsg_request_t);
  ctx->r2->refct = 1;
  ctx->r2->hdr.isreq=TRUE;
  ctx->r2->hdr.paysz=1200;
  ctx->r2->hdr.pri=RWMSG_PRIORITY_HIGH;

  rw_status_t s;

  rwmsg_notify_t extnotif;
  memset(&extnotif, 0, sizeof(extnotif));
  extnotif.theme = RWMSG_NOTIFY_RWSCHED;
  rwsched_dispatch_queue_t serq = rwsched_dispatch_queue_create(tenv.tasklet[0], "serq", DISPATCH_QUEUE_SERIAL);
  extnotif.rwsched.rwqueue = serq;
  rwmsg_closure_t cb;
  memset(&cb, 0, sizeof(cb));
  cb.notify = qtest_cb1;
  cb.ud = ctx;
  rwmsg_notify_init(ep, &extnotif, "extnotif (RWMsgAPI.QueueRWSchedEvent)", &cb);

  s = rwmsg_queue_init(ep, &ctx->q, "test", &extnotif);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);


  /* Enqueue one thing, run sched, see if it pops out.  Also, the cb1
     will enqueue r2 the first time called, so that should
     happen... */
  ASSERT_TRUE(ctx->in == 0);
  ck_pr_inc_32(&ctx->in);
  s = rwmsg_queue_enqueue(&ctx->q, ctx->r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);

  int iter=0;
  do {
    usleep(10000);
    ck_pr_barrier();
  } while (ctx->out < 2 && iter++ < 500);

  ASSERT_TRUE(ctx->in == 2);
  ASSERT_TRUE(ctx->out == 2);

  /* Enqueue two things, see... */
  ck_pr_inc_32(&ctx->in);
  s = rwmsg_queue_enqueue(&ctx->q, ctx->r1);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  ASSERT_TRUE(ctx->in == 3);
  ck_pr_inc_32(&ctx->in);
  s = rwmsg_queue_enqueue(&ctx->q, ctx->r2);
  ASSERT_TRUE(s == RW_STATUS_SUCCESS);
  ASSERT_TRUE(ctx->in == 4);

  usleep(100000);

  ASSERT_TRUE(ctx->out == 4);

  rwmsg_notify_deinit(ep, &extnotif);

  RW_FREE_TYPE(ctx->r1, rwmsg_request_t);
  RW_FREE_TYPE(ctx->r2, rwmsg_request_t);
  rwmsg_queue_deinit(ep, &ctx->q);
  RW_FREE_TYPE(ctx, qtest1_context);

  rwsched_dispatch_release(tenv.tasklet[0], serq);

  rwmsg_bool_t rb = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(rb);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();
}

struct sctest1_payload {
  uint32_t rno;
  char str[200];
};
struct sctest1_context {
  int verbose;
  int sendmore;
  uint32_t done;
  rwmsg_srvchan_t *sc;
  rwmsg_clichan_t *cc;
  rwmsg_destination_t *dt;
  uint64_t sc_tid;
  uint64_t cc_tid;
  int checktid;
  rwmsg_signature_t *sig;
#define SCTEST1_RMAX (100)
  uint32_t rmax;
  rwmsg_request_t *r[SCTEST1_RMAX];
  uint32_t in;
  uint32_t out;
  uint32_t rspin;
  uint32_t rspout;
  uint32_t bncout;
  uint32_t miss;
};
#define GETTID() syscall(SYS_gettid)
static void sctest1_clicb1(rwmsg_request_t *req, void *ctx_in) {
  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;

  if (ctx->sc_tid) {
    if (!ctx->cc_tid) {
      ctx->cc_tid = GETTID();
      if (ctx->checktid) {
	EXPECT_EQ(ctx->cc_tid, ctx->sc_tid);
      }
    } else if (ctx->checktid) {
      uint64_t curtid = GETTID();
      EXPECT_EQ(ctx->cc_tid, curtid);
      ctx->cc_tid = curtid;
    }
  }

  /* clichan request callback */

  uint32_t rno = ~0;
  uint32_t len=0;
  const struct sctest1_payload *rsp = (const struct sctest1_payload*)rwmsg_request_get_response_payload(req, &len);
  if (rsp) {
    ck_pr_inc_32(&ctx->rspout);
    if (ctx->verbose || rsp->rno >= ctx->rmax) {
      fprintf(stderr, "sctest1_clicb1: got rsp len=%d rno=%u str='%s'\n", len, rsp->rno, rsp->str);
    }
    RW_ASSERT(rsp->rno < ctx->rmax);
    rno = rsp->rno;
  } else {
    ck_pr_inc_32(&ctx->bncout);
    for (rno = 0; rno < ctx->rmax; rno++) {
      if (req == ctx->r[rno]) {
	goto found;
      }
    }
    fprintf(stderr, "sctest1_clicb1: bnc req=%p not found!?\n", req);
    RW_ASSERT(rno < ctx->rmax);
    if (!ctx->sendmore) {
      goto checkend;
    }
    return;

  found:
    ;
  }

  /* req ceases to be valid when we return, so find and clear our handle to it */
  RW_ASSERT(rno < SCTEST1_RMAX);
  ctx->r[rno] = NULL;

  /* If the test has the repeat flag set, reissue another request */
  if (ctx->sendmore) {
    RW_ASSERT(ctx->cc);
    ctx->r[rno] = rwmsg_request_create(ctx->cc);
    ASSERT_TRUE(ctx->r[rno] != NULL);
    rwmsg_request_set_signature(ctx->r[rno], ctx->sig);
    rwmsg_closure_t clicb;
    clicb.request=sctest1_clicb1;
    clicb.ud=ctx;
    rwmsg_request_set_callback(ctx->r[rno], &clicb);
    struct sctest1_payload payload;
    payload.rno = rno;
    sprintf(payload.str, "hi there rno=%d", rno);
    rwmsg_request_set_payload(ctx->r[rno], &payload, sizeof(payload));
    
    /* Send it.  The srvchan should more or less immediately run the
       thing and the methcb will respond. */
    ck_pr_inc_32(&ctx->in);
    rw_status_t rs = rwmsg_clichan_send(ctx->cc, ctx->dt, ctx->r[rno]);
    if (rs != RW_STATUS_SUCCESS) {
      ctx->sendmore = FALSE;
      fprintf(stderr, "sctest1_clicb1: rwmsg_clichan_send() rs=%u\n", rs);
    }
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  } else {
  checkend:
    if ((ctx->rspout + ctx->bncout) == ctx->in) {
      ctx->done = TRUE;
    }
  }

}
static void sctest1_methcb1(rwmsg_request_t *req, void *ctx_in) {

  static int once=0;

  RW_ASSERT(!once);
  once=1;

  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;

  /* srvchan method callback */
  if (!ctx->sc_tid) {
    ctx->sc_tid = GETTID();
  } else if (ctx->checktid) {
    uint64_t curtid = GETTID();
    EXPECT_EQ(ctx->sc_tid, curtid);
    ctx->sc_tid = curtid;
  }

  ck_pr_inc_32(&ctx->out);

  uint32_t paylen=0;
  const struct sctest1_payload *pay = (const struct sctest1_payload*)rwmsg_request_get_request_payload(req, &paylen);
  if (pay) {
    if (ctx->verbose) {
      fprintf(stderr, "sctest1_methcb1 got msg in=%u paylen=%u rno=%u str='%s' ctx->in=%u ctx->out=%u\n", 
	      ctx->in, paylen, pay->rno, pay->str, ctx->in, ctx->out);
    }

    static uint32_t outct = 0;
    struct sctest1_payload payout;
    outct++;
    payout.rno = pay->rno;
    sprintf(payout.str, "response payload number %u to input msg '%s'",
	    outct,
	    pay ? pay->str : "");

    rw_status_t rs = rwmsg_request_send_response(req, &payout, sizeof(payout));
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    ck_pr_inc_32(&ctx->rspin);
  } else {
    rw_status_t rs = rwmsg_request_send_response(req, NULL, 0);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    ck_pr_inc_32(&ctx->rspin);
  }

  ck_pr_barrier();
  RW_ASSERT(once);
  once=0;
}
TEST(RWMsgAPI, SrvchanBindingRWSched) {
  TEST_DESCRIPTION("Tests srvchan binding to rwsched");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  rw_status_t rs;

  struct sctest1_context *ctx = RW_MALLOC0_TYPE(sizeof(*ctx), sctest1_context);
  ctx->verbose=0;
  ctx->rmax = 1;

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/7";
  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method, and bind the srvchan to the rwsched default queue */
  ctx->sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(ctx->sc != NULL);
  rwmsg_signature_t *sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  rwmsg_closure_t methcb;
  methcb.request=sctest1_methcb1;
  methcb.ud=ctx;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(ctx->sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwsched_dispatch_queue_t serq = rwsched_dispatch_queue_create(tenv.tasklet[0], "serq", DISPATCH_QUEUE_SERIAL);

  rs = rwmsg_srvchan_bind_rwsched_queue(ctx->sc, serq);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Create the clichan, add the method's signature */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  ctx->dt = dt;
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);
  //TBD/optional  rwmsg_clichan_add_signature(cc, sig);

  /* Create the actual request */
  int rno=0;
  ctx->r[rno] = rwmsg_request_create(cc);
  ASSERT_TRUE(ctx->r[rno] != NULL);
  rwmsg_request_set_signature(ctx->r[rno], sig);
  rwmsg_closure_t clicb;
  clicb.request=sctest1_clicb1;
  clicb.ud=ctx;
  rwmsg_request_set_callback(ctx->r[rno], &clicb);
  struct sctest1_payload payload;
  payload.rno = rno;
  sprintf(payload.str, "hi there rno=%d", rno);
  rwmsg_request_set_payload(ctx->r[rno], &payload, sizeof(payload));

  /* Send it.  The srvchan should more or less immediately run the
     thing and the methcb will respond. */
  ck_pr_inc_32(&ctx->in);
  rs = rwmsg_clichan_send(cc, ctx->dt, ctx->r[rno]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  usleep(100000);

  ASSERT_TRUE(ctx->out == 1);
  ASSERT_TRUE(ctx->rspin == 1);	// srvchan was bound so a response should have been made from the method callback
  ASSERT_TRUE(ctx->rspout == 0); // clichan is not bound, we'll have to recv

  
  /* Receive on the clichan and see if we got the response */
  uint32_t ct = rwmsg_clichan_recv(cc);
  ASSERT_TRUE(ct == 1);
  ASSERT_TRUE(ctx->rspout == 1);
  ASSERT_TRUE(ctx->miss == 0);
  ASSERT_TRUE(ctx->r[rno] == NULL); // freed by recv, callback should set this to NULL to be tidy

  rwmsg_bool_t r;
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(ctx->sc);
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);

  rwsched_dispatch_release(tenv.tasklet[0], serq);

  RW_FREE_TYPE(ctx, sctest1_context);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();
}



TEST(RWMsgAPI, BothchanBindingRWSched) {
  TEST_DESCRIPTION("Tests clichan and srvchan binding to rwsched");

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  rw_status_t rs;

  struct sctest1_context *ctx = RW_MALLOC0_TYPE(sizeof(*ctx), sctest1_context);
  ctx->rmax = 1;
  
  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/8";
  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method, and bind the srvchan to the rwsched default queue */
  ctx->sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(ctx->sc != NULL);
  rwmsg_signature_t *sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  rwmsg_closure_t methcb;
  methcb.request=sctest1_methcb1;
  methcb.ud=ctx;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(ctx->sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwsched_dispatch_queue_t serq1 = rwsched_dispatch_queue_create(tenv.tasklet[0], "serq1", DISPATCH_QUEUE_SERIAL);
  rwsched_dispatch_queue_t serq2 = rwsched_dispatch_queue_create(tenv.tasklet[0], "serq2", DISPATCH_QUEUE_SERIAL);

  rs = rwmsg_srvchan_bind_rwsched_queue(ctx->sc, serq1);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Create the clichan, add the method's signature */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  ctx->dt = dt;
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);
  //TBD/optional  rwmsg_clichan_add_signature(cc, sig);
  rs = rwmsg_clichan_bind_rwsched_queue(cc, serq2);

  /* Create the actual request */
  int rno=0;
  ctx->r[rno] = rwmsg_request_create(cc);
  ASSERT_TRUE(ctx->r[rno] != NULL);
  rwmsg_request_set_signature(ctx->r[rno], sig);
  rwmsg_closure_t clicb;
  clicb.request=sctest1_clicb1;
  clicb.ud=ctx;
  rwmsg_request_set_callback(ctx->r[rno], &clicb);
  struct sctest1_payload payload;
  payload.rno = rno;
  sprintf(payload.str, "hi there rno=%d", rno);
  rwmsg_request_set_payload(ctx->r[rno], &payload, sizeof(payload));

  /* Send it.  The srvchan should more or less immediately run the
     thing and the methcb will respond. */
  ck_pr_inc_32(&ctx->in);
  rs = rwmsg_clichan_send(cc, ctx->dt, ctx->r[rno]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  int iter=0;
  while (!ctx->done || cc->ch.localq.qlen || ctx->sc->ch.localq.qlen || ctx->rspout < ctx->in) {
    usleep(100000);
    iter++;
    if (iter > 50) {
      fprintf(stderr, "ccqlen=%u scqlen=%u rspout=%u in=%u\n", cc->ch.localq.qlen, ctx->sc->ch.localq.qlen, ctx->rspout, ctx->in);
    }
    ASSERT_LE(iter, 5/*s*/*10);
    ck_pr_barrier();
  }

  ASSERT_TRUE(ctx->out == 1);
  ASSERT_TRUE(ctx->rspin == 1);	// srvchan was bound so a response should have been made from the method callback
  ASSERT_TRUE(ctx->rspout == 1); // clichan is bound, we should have already recv'd
  ASSERT_TRUE(ctx->rspout == 1);
  ASSERT_TRUE(ctx->miss == 0);
  ASSERT_TRUE(ctx->r[rno] == NULL); // freed by recv, callback should set this to NULL to be tidy

  rwmsg_bool_t r;
  rwmsg_srvchan_halt(ctx->sc);
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);

  rwsched_dispatch_release(tenv.tasklet[0], serq1);
  rwsched_dispatch_release(tenv.tasklet[0], serq2);

  RW_FREE_TYPE(ctx, sctest1_context);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();
}



#define VERBOSE() (getenv("V") && getenv("V")[0]=='1')
static struct timeval PRN_start;
#define VERBPRN(args...) do {						\
  if (VERBOSE()) {							\
    if (!PRN_start.tv_sec) {						\
      gettimeofday(&PRN_start, NULL);					\
    }									\
    struct timeval tv;							\
    gettimeofday(&tv, NULL);						\
    struct timeval delta;						\
    timersub(&tv, &PRN_start, &delta);					\
    fprintf(stderr, "%ld.%03ld ", delta.tv_sec, delta.tv_usec/1000);	\
    fprintf(stderr, args);						\
   }									\
  } while(0)



static void Test_increment(Test_Service *,
			   const TestReq *req, 
			   void *ud,
			   TestRsp_Closure clo, 
			   void *rwmsg) {
  TestRsp rsp;
  test_rsp__init(&rsp);

  /* Fill in a response, including a copy of the adjusted request body with the value bumped */
  rsp.errval = 0;
  rsp.has_body = 1;
  rsp.body = req->body;
  rsp.body.value++;

  VERBPRN("    Test_increment(value=%u) returning value=%u\n", req->body.value, rsp.body.value);

  /* Send the response */
  clo(&rsp, rwmsg);

  return;
  ud=ud;
}

static void Test_fail(Test_Service *,
		      const TestReq *req, 
		      void *ud,
		      TestNop_Closure clo, 
		      void *rwmsg) {
  TestNop rsp;
  test_nop__init(&rsp);

  if (req->body.value) {
    VERBPRN("    Test_fail(value=%u) returning errval=1\n", req->body.value);
  }

  rsp.errval = 1;

  clo(&rsp, rwmsg);

  return;
  ud=ud;
}


TEST(RWMsgAPI, ProtobufServiceBind) {
  TEST_DESCRIPTION("Tests protobuf service creation and binding");

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/9";

  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rw_status_t rs = rwmsg_srvchan_add_service(sc, 
					     taddr, 
					     &myapisrv.base,
					     (void*)taddr);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  /* Verify that the binding worked by doing a lookups for methods in
     this service (as done inside local clichans) */
  rwmsg_srvchan_t *clisc;
  uint64_t phash = rw_hashlittle64(taddr, strlen(taddr));
  uint32_t method1 = RWMSG_PBAPI_METHOD(myapisrv.base.descriptor->rw_srvno,
					1);
  uint32_t method2 = RWMSG_PBAPI_METHOD(myapisrv.base.descriptor->rw_srvno,
					2);
  uint32_t method3 = RWMSG_PBAPI_METHOD(myapisrv.base.descriptor->rw_srvno,
					3);
  clisc = (rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, 0, phash, RWMSG_PAYFMT_PBAPI, method2);
  ASSERT_TRUE(clisc);
  ASSERT_TRUE(clisc == sc);
  if (clisc) {
    rwmsg_srvchan_release(clisc);
  }
  clisc = (rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, 0, phash, RWMSG_PAYFMT_PBAPI, method1);
  ASSERT_TRUE(clisc);
  ASSERT_TRUE(clisc == sc);
  if (clisc) {
    rwmsg_srvchan_release(clisc);
  }
  clisc = (rwmsg_srvchan_t*)rwmsg_endpoint_find_channel(ep, RWMSG_METHB_TYPE_SRVCHAN, 0, phash, RWMSG_PAYFMT_PBAPI, method3);
  ASSERT_FALSE(clisc);
  if (clisc) {
    rwmsg_srvchan_release(clisc);
  }

  rwmsg_bool_t r;
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);

  nnchk();
  rs=rs;
}

rwmsg_destination_t *d;

static void PCS_rsp_increment(const TestRsp *msg, rwmsg_request_t *rwreq, void *ud) {
  if (msg) {
    uint32_t *val = (uint32_t *)ud;
    if (!msg->errval) {
      VERBPRN("    Response at PCS_rsp_increment(); old val=%u, new val=%u\n", *val, msg->body.value);
      *val = msg->body.value;
    } else {
      VERBPRN("    Response at PCS_rsp_increment(); errval=%u\n", msg->errval);
    }
    if (*val < 10) {
      VERBPRN("  Sending another request from PCS_rsp_increment(), req.body.value=%u\n", *val);
      TestReq req;
      test_req__init(&req);
      req.body.value = *val;
      rwmsg_closure_t cb;
      cb.pbrsp=(rwmsg_pbapi_cb)PCS_rsp_increment;
      cb.ud=val;
      rw_status_t rs = test__increment((Test_Client*)rwreq->pbcli, d, &req, &cb, NULL);
      rs=rs;
    }
  }
}
static void PCS_rsp_fail(const TestNop *msg, rwmsg_request_t *, void *ud) {
  if (msg) {
    uint32_t *val = (uint32_t *)ud;
    VERBPRN("    Response at PCS_rsp_fail(); rsp->errval=%u\n", msg->errval);
    *val = msg->errval;
  }
}

TEST(RWMsgAPI, ProtobufCliSrv) {
  TEST_DESCRIPTION("Tests protobuf service client and server");

  /*
   * Create a rwsched / tasklet runtime and the one rwmsg endpoint for
   * this tasklet.  (Probably all handled in tasklet infrastructure).
   */

  rwmsg_tenv_t tenv(1);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  /* 
   * Create a server channel 
   */
  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);

  /*
   * Instantiate the 'Test' protobuf service as the set of callbacks
   * named Test_<x>, and bind this Test service to our server channel
   * at the given nameservice location.
   */
  const char *taddr = "/L/GTEST/RWMSG/TESTAPP/10";

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rw_status_t rs = rwmsg_srvchan_add_service(sc, 
					     taddr, 
					     &myapisrv.base,
					     (void*)taddr);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);


  /* 
   * Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */

  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  d = dt;

  rwmsg_clichan_t *cc;
  cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);


  /*
   * Now do stuff... 
   */

  {
    uint32_t val=5;
    
    TestReq req;
    test_req__init(&req);
    req.body.value = val;

    VERBPRN("Phase 1\n  ProtobufCliSrv test sending first request increment(req.body.value=%u)\n", val);
    
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)PCS_rsp_increment;
    cb.ud=&val;
    rs = test__increment(&mycli, d, &req, &cb, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    /* No tasklets / scheduling in this test, just call recv a bunch
       on both ends to move things along */
    int iter=0;
    int smsgct=0, cmsgct=0;
    for (iter=0; iter < 6; iter++) {
      smsgct += rwmsg_srvchan_recv(sc);
      cmsgct += rwmsg_clichan_recv(cc);
    }
    ASSERT_EQ(val, 10);
    ASSERT_EQ(smsgct, 5);
    ASSERT_EQ(cmsgct, 5);

    VERBPRN("  End Phase\n    Srvchan recv ct %u\n    Clichan recv ct %u\n", smsgct, cmsgct);
    VERBPRN("Phase 2\n  ProtobufCliSrv test sending one request fail(req.body.value=%u)\n", val);

    test_req__init(&req);
    req.body.value = val;

    smsgct=0;
    cmsgct=0;
    val=0;
    cb.pbrsp=(rwmsg_pbapi_cb)PCS_rsp_fail;
    cb.ud=&val;
    rs = test__fail(&mycli, d, &req, &cb, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    smsgct += rwmsg_srvchan_recv(sc);
    ASSERT_EQ(smsgct, 1);
    cmsgct += rwmsg_clichan_recv(cc);
    ASSERT_EQ(cmsgct, 1);
    ASSERT_EQ(val, 1);

    VERBPRN("  End Phase\n    Srvchan recv ct %u\n    Clichan recv ct %u\n", smsgct, cmsgct);
  }

  rwmsg_bool_t r;
  rwmsg_srvchan_halt(sc);
  rwmsg_clichan_halt(cc);
  rwmsg_destination_release(dt);
  r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  nnchk();

  rs=rs;
}

struct mycfbctx {
  int doblocking;
  int done;
  uint64_t tid;
  int blockingnow;
  int assertsinglethread;
  uint32_t msgct_end;
  uint32_t burst;
  rwsched_instance_ptr_t rwsched;
  rwsched_CFRunLoopRef loop;
  uint32_t tickct;		// number of timer ticks testwide
  uint32_t timerct;		// number of timers that exist
};
struct mycfbtaskinfo {
  int tnum;
  uint64_t tid;
  rwsched_CFRunLoopTimerRef cftim;
  
  Test_Service myapisrv;
  Test_Client mycli;

  char tpath[128];
  char tpeerpath[128];

  rwmsg_endpoint_t *ep;
  rwmsg_srvchan_t *sc;

  rwmsg_destination_t *dt;
  rwmsg_clichan_t *cc;

  struct mycfbctx *ctx;
  rwsched_tasklet_ptr_t tasklet;

  uint32_t req_sent;
  uint32_t req_eagain;
  uint32_t rsp_recv;
  uint32_t bnc_recv;

  uint32_t tickct;		// number of timer ticks in this tasklet
};

static void mycfb_response(const TestRsp *rsp, rwmsg_request_t *req, void *ud) {
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo *)ud;

  ASSERT_TRUE(rsp != NULL);
  if (rsp) {
    ASSERT_EQ(rsp->errval, 0);
    ASSERT_TRUE(rsp->has_body);
    ASSERT_EQ(rsp->body.hopct+1, rsp->body.value);
    tinfo->rsp_recv++;

    VERBPRN("ProtobufCliSrvCFNonblocking test got rsp, value=%u errval=%u\n", rsp->body.value, rsp->errval);
  } else {
    tinfo->bnc_recv++;
    VERBPRN("ProtobufCliSrvCFNonblocking test got bnc\n");
  }

  return;
  req=req;
}

static void mycfb_timer_cb(rwsched_CFRunLoopTimerRef timer, void *info) {
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo *)info;

  ASSERT_TRUE(tinfo->cftim != NULL);

  if (tinfo->ctx->doblocking && tinfo->ctx->blockingnow) {
    VERBPRN("mycfb_timer_cb tinfo=%p deferring, blockingnow\n", tinfo);
  }

  tinfo->tickct++;
  tinfo->ctx->tickct++;

  /*oops  ASSERT_EQ(tinfo->cftim, timer);*/

  VERBPRN("mycfb_timer_cb, tick %d in tinfo=%p\n", tinfo->tickct, tinfo);

  if (tinfo->ctx->assertsinglethread) {
    uint64_t tid = GETTID();
    if (!tinfo->tid) {
      tinfo->tid = tid;
    } else {
      ASSERT_EQ(tinfo->tid, tid);
    }
    if (!tinfo->ctx->tid) {
      tinfo->ctx->tid = tid;
    } else {
      ASSERT_EQ(tinfo->ctx->tid, tid);
    }
  }

  if ((tinfo->bnc_recv + tinfo->rsp_recv) >= tinfo->ctx->msgct_end) {

    RW_ASSERT(tinfo->req_sent == (tinfo->bnc_recv + tinfo->rsp_recv));
    
    if (tinfo->cftim) {
      
      VERBPRN("Stopping timer after response %u + bnc %u in tasklet '%s'\n", 
	      tinfo->rsp_recv,
	      tinfo->bnc_recv,
	      tinfo->tpath);
      
      rwsched_tasklet_CFRunLoopTimerInvalidate(tinfo->tasklet, tinfo->cftim);
      tinfo->cftim = 0;
      
      tinfo->ctx->timerct--;
      
      if (!tinfo->ctx->timerct) {
	VERBPRN("All tasklet timers gone, ending runloop\n");
	//CFRunLoopStop(tinfo->ctx->loop);
	CFRunLoopStop(rwsched_tasklet_CFRunLoopGetCurrent(tinfo->tasklet));
	tinfo->ctx->done = 1;
      }
    }
    
    return;
  }


  TestReq req;
  test_req__init(&req);
  
  for (uint32_t i=0; i<tinfo->ctx->burst; i++) {
    rwmsg_request_t *rwreq=NULL;

    req.body.value = tinfo->req_sent;
    req.body.hopct = tinfo->req_sent;

    if (tinfo->req_sent >= tinfo->ctx->msgct_end) {
      break;
    }

    if (tinfo->ctx->doblocking) {
      rw_status_t rs;
      rwreq = NULL;
      VERBPRN("ProtobufCliSrvCFBlocking test sending req, value=%u burst i=%u\n", req.body.value, i);

      tinfo->ctx->blockingnow++;
      rs = test__increment_b(&tinfo->mycli, tinfo->dt, &req, &rwreq);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
      tinfo->ctx->blockingnow--;
      
      ASSERT_EQ(RWMSG_PAYFMT_PBAPI, rwmsg_request_get_response_payfmt(rwreq));

      const TestRsp *rsp;
      rs = rwmsg_request_get_response(rwreq, (const void **)&rsp);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
      tinfo->req_sent++;

      VERBPRN("ProtobufCliSrvCFBlocking test got rsp, errval=%u, burst i=%u\n", rsp->errval, i);

      mycfb_response(rsp, rwreq, info);
    } else {
      rwmsg_closure_t cb;
      cb.pbrsp=(rwmsg_pbapi_cb)mycfb_response;
      cb.ud=tinfo;

      VERBPRN("ProtobufCliSrvCFNonblocking test sending req, value=%u burst i=%u\n", req.body.value, i);

      rw_status_t rs;
      rs = test__increment(&tinfo->mycli, tinfo->dt, &req, &cb, &rwreq);
      switch (rs) {
      case RW_STATUS_SUCCESS:
	VERBPRN("ProtobufCliSrvCFNonblocking test sent req, burst i=%u\n", i);
	tinfo->req_sent++;
	break;
      case RW_STATUS_BACKPRESSURE:
	VERBPRN("ProtobufCliSrvCFNonblocking test got backpressure, burst i=%u\n", i);
	tinfo->req_eagain++;
	break;
      default:
	ASSERT_EQ(rs, RW_STATUS_SUCCESS);
	break;
      }
    }
  }

  return;

  timer=timer;
}

/** \if NOPE */
static void pbcscf(int taskct, int doblocking, uint32_t burst, uint32_t endat) {

  struct mycfbctx tctx;
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo*)RW_MALLOC0(sizeof(*tinfo) * taskct);

  memset(&tctx, 0, sizeof(tctx));

  /*
   * Create a rwsched / tasklet runtime and the one rwmsg endpoint for
   * this tasklet.  (Probably all handled in tasklet infrastructure).
   */

  rwmsg_tenv_t tenv(taskct);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  const char *taddrbase = "/L/GTEST/RWMSG/TESTAPP/PBSCF";
  char taddrpfx[128];
  static unsigned testct = 100;
  sprintf(taddrpfx, "%s/%u", taddrbase, testct++);

  tctx.rwsched = tenv.rwsched;
  tctx.doblocking = doblocking;
  tctx.burst = burst;
  tctx.msgct_end = endat;
  tctx.assertsinglethread = FALSE;
  tctx.tid = GETTID();

  for (int t=0; t<tenv.tasklet_ct; t++) {

    tinfo[t].tnum = t;
    tinfo[t].ctx = &tctx;
    tinfo[t].tasklet = tenv.tasklet[t];

    sprintf(tinfo[t].tpath, "%s/%d", taddrpfx, t);
    sprintf(tinfo[t].tpeerpath, "%s/%d", taddrpfx, (t&1) ? t-1 : t+1);

    tinfo[t].ep = rwmsg_endpoint_create(1, t, 0, tenv.rwsched, tenv.tasklet[t], rwtrace_init(), NULL);
    ASSERT_TRUE(tinfo[t].ep != NULL);

    rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.tasklet[t]);

    if (t&1) {
      /* Create a server channel in odd numbered tasklets */
      tinfo[t].sc = rwmsg_srvchan_create(tinfo[t].ep);
      ASSERT_TRUE(tinfo[t].sc != NULL);

      /* Instantiate the 'Test' protobuf service as the set of callbacks
       * named Test_<x>, and bind this Test service to our server channel
       * at the given nameservice location.
       */

      TEST__INITSERVER(&tinfo[t].myapisrv, Test_);
      rw_status_t rs = rwmsg_srvchan_add_service(tinfo[t].sc, 
						 tinfo[t].tpath, 
						 &tinfo[t].myapisrv.base,
						 &tinfo[t]);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);

      rs = rwmsg_srvchan_bind_rwsched_cfrunloop(tinfo[t].sc,
						tenv.tasklet[t]);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
      
    } else {

      /* Create a client channel pointing at the Test service's path.
       * Instantiate a Test service client and connect this to the client
       * channel.
       */
      tinfo[t].dt = rwmsg_destination_create(tinfo[t].ep, RWMSG_ADDRTYPE_UNICAST, tinfo[t].tpeerpath);
      ASSERT_TRUE(tinfo[t].dt != NULL);
      
      tinfo[t].cc = rwmsg_clichan_create(tinfo[t].ep);
      ASSERT_TRUE(tinfo[t].cc != NULL);

      TEST__INITCLIENT(&tinfo[t].mycli);
      rwmsg_clichan_add_service(tinfo[t].cc, &tinfo[t].mycli.base);

      rw_status_t rs = rwmsg_clichan_bind_rwsched_cfrunloop(tinfo[t].cc,
							    tenv.tasklet[t]);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);


      /* Run a timer to execute some requests */
      rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
      double timer_interval = 200/*ms*/ * .001;
      if (RUNNING_ON_VALGRIND) {
	timer_interval = timer_interval * 2.0;
      }

      VERBPRN("timer_interval=%.3f in tinfo[%u]=%p\n", timer_interval, t, &tinfo[t]);

      cf_context.info = &tinfo[t];
      tinfo[t].cftim = rwsched_tasklet_CFRunLoopTimerCreate(tenv.tasklet[t],
						    kCFAllocatorDefault,
						    CFAbsoluteTimeGetCurrent() + timer_interval + 0.1 * t,
						    timer_interval,
						    0,
						    0,
						    mycfb_timer_cb,
						    &cf_context);
      RW_CF_TYPE_VALIDATE(tinfo[t].cftim, rwsched_CFRunLoopTimerRef);
      rwsched_tasklet_CFRunLoopAddTimer(tenv.tasklet[t], runloop, tinfo[t].cftim, RWMSG_RUNLOOP_MODE);
      tctx.timerct++;
    }
  }

  VERBPRN("pbscf invoking CFRunLoop\n");

  //tctx.loop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.rwsched);
  struct timeval start, stop, delta;
  int loopwait_time = 60;
  if (RUNNING_ON_VALGRIND) {
    loopwait_time *= 1.0;
  }
  int loopwait = loopwait_time;
  gettimeofday(&start, NULL);
  do {

    //CFRunLoopRun();
    rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, loopwait, FALSE); 
    gettimeofday(&stop, NULL);
    timersub(&stop, &start, &delta);
    VERBPRN("loop iter %ld.%03ld tctx.done=%d\n", delta.tv_sec, delta.tv_usec / 1000, tctx.done);  
    loopwait = loopwait_time - delta.tv_sec;
  } while (loopwait > 0 && !tctx.done);

  ASSERT_TRUE(tctx.done);

  VERBPRN("loop runtime %lu ms\n", delta.tv_sec * 1000 + delta.tv_usec / 1000);

  for (int t=0; t<tenv.tasklet_ct; t++) {
    if (t&1) {
      rwmsg_srvchan_halt(tinfo[t].sc);
    } else {
      rwmsg_destination_release(tinfo[t].dt);
      rwmsg_clichan_halt(tinfo[t].cc);
      if (tinfo[t].req_eagain) {
	fprintf(stderr, "Tasklet %d req_sent=%u req_eagain=%u\n", t, tinfo[t].req_sent, tinfo[t].req_eagain);
      }
    }
    rwmsg_bool_t r;
    r = rwmsg_endpoint_halt_flush(tinfo[t].ep, TRUE);
    ASSERT_TRUE(r);
    tinfo[t].ctx = NULL;
  }

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  if (tinfo) {
    RW_FREE(tinfo);
  }
}
/** \endif */

TEST(RWMsgAPI, PbCliSrvCFNonblk2) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 2;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, endct);
}

TEST(RWMsgAPI, PbCliSrvCFBlk2) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets");

  int taskct = 2;
  int doblocking = 1;
  uint32_t burst = 2;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, endct);
}

TEST(RWMsgAPI, PbCliSrvCFNonblk10) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 10 tasklets");

  int taskct = 10;
  int doblocking = 0;
  uint32_t burst = 50;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, endct);
}

TEST(RWMsgAPI, PbCliSrvCFBlk10) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 10 tasklets");

  int taskct = 10;
  int doblocking = 1;
  uint32_t burst = 10;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, endct);
}

TEST(RWMsgAPI, SocksetCreateDelete) {
  TEST_DESCRIPTION("Tests sockset+endpoint creation");

  rwmsg_tenv_t tenv(1);
  ASSERT_TRUE(tenv.rwsched);

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwmsg_sockset_t *sk;
  sk = rwmsg_sockset_create(ep, NULL, 0, FALSE);
  ASSERT_TRUE(sk);
  ASSERT_FALSE(sk->closed);

  rwmsg_sockset_close(sk);
  ASSERT_TRUE(sk->closed);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);

  nnchk();
}

static int get_nn_sock_connection_count(int nn, const char* message) {
  struct nn_sock_stats nn_stats;
  size_t olen = sizeof(nn_stats);
  int r = nn_getsockopt(nn, NN_SOL_SOCKET, NN_RWGET_STATS, &nn_stats, &olen);
  if (!r) {
    /*
    fprintf(stderr, "%s\nNN-Socket Stats\n"
                    "Number of currently established connections: %u\n"
                    "Number of connections currently in progress: %u\n"
                    "The currently set priority for sending data: %u\n"
                    "Number of endpoints having last_errno set to non-zero value: %u\n",
                    (message? message:""),
                    nn_stats.current_connections,
                    nn_stats.inprogress_connections,
                    nn_stats.current_snd_priority,
                    nn_stats.current_ep_errors);
                    */
    return nn_stats.current_connections;
  }
  else {
    // fprintf(stderr, "\nNN-Socket Stats FAILED\n");
  }
  return -1;
}

/* This test takes 15s! */
TEST(RWMsgNN, SleepDuringConnect) {
  TEST_DESCRIPTION("Tests NN connection in background thread");

  int r;
  int nnsk[2];

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }

  nnsk[0] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[0], 0);
  nnsk[1] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[1], 0);

#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_socket()"), 0);
#endif

  char abuf[64];

  uint16_t port = 7000 + (getpid() % 1000);

  int eid[2];
  sprintf(abuf, "tcp://127.0.0.1:%d", port);
  //  fprintf(stderr, "bind abuf='%s'\n", abuf);
  eid[0] = nn_bind (nnsk[0], abuf);
  ASSERT_TRUE(eid[0] >= 0);

  sleep(1);
#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_bind()"), 0);
#endif


  sprintf(abuf, "tcp://127.0.0.1:%d", port);
  eid[1] = nn_connect (nnsk[1], abuf);
  ASSERT_TRUE(eid[1] >= 0);

  /* Sleep longer than the 1s nee 10s connect/handshake timeout */
  sleep(12);
#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_connect()"), 1);
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[1], "nnsk[1] - after nn_connect()"), 1);
#endif


  char sndmsg[] = "ABC";
  char rcvmsg[64];

  r = nn_send(nnsk[0], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);

#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_send()"), 1);
#endif

  r = nn_recv(nnsk[1], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  r = nn_send(nnsk[1], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  
  r = nn_recv(nnsk[0], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  nn_close(nnsk[0]);
  nn_close(nnsk[1]);

#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_close()"), -1);
#endif

  nnchk();
}

/* This test takes 15s! */
TEST(RWMsgNN, TryConnectWOSrvr) {
  TEST_DESCRIPTION("Tests NN connection in background thread");

  int r;
  int nnsk[2];

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }

  nnsk[0] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[0], 0);
  nnsk[1] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[1], 0);

  uint16_t port = 7000 + (getpid() % 1000);
  char abuf[64];
  int eid[2];


  sprintf(abuf, "tcp://127.0.0.1:%d", port);
  eid[1] = nn_connect (nnsk[1], abuf);
  ASSERT_TRUE(eid[1] >= 0);

#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_send()"), 0);
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[1], "nnsk[0] - after nn_send()"), 0);
#endif

  char sndmsg[] = "ABC";
  char rcvmsg[64];

  r = nn_send(nnsk[0], sndmsg, strlen(sndmsg)+1, NN_DONTWAIT);
  ASSERT_EQ(r, -1); /* This send fails */

#if 1
#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_socket()"), 0);
#endif

  sprintf(abuf, "tcp://127.0.0.1:%d", port);
  //  fprintf(stderr, "bind abuf='%s'\n", abuf);
  eid[0] = nn_bind (nnsk[0], abuf);
  ASSERT_TRUE(eid[0] >= 0);
#endif

  /* Sleep longer than the 1s nee 10s connect/handshake timeout */
  sleep(12);
#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_connect()"), 1);
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[1], "nnsk[1] - after nn_connect()"), 1);
#endif


  r = nn_send(nnsk[0], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);

  r = nn_recv(nnsk[1], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  r = nn_send(nnsk[1], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  
  r = nn_recv(nnsk[0], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  nn_close(nnsk[0]);
  nn_close(nnsk[1]);

#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_close()"), -1);
#endif

  nnchk();
}

TEST(RWMsgNN, ManySockets) {
  TEST_DESCRIPTION("Tests NN socket creation limit");

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }

  int r;
  int nnsk[NN_MAX_SOCKETS + 1 ];

  struct rlimit orlim = { 0 }, nrlim;
  getrlimit(RLIMIT_NOFILE, &orlim);
  memcpy(&nrlim, &orlim, sizeof(nrlim));
  nrlim.rlim_cur = nrlim.rlim_max;
  setrlimit(RLIMIT_NOFILE, &nrlim);
  getrlimit(RLIMIT_NOFILE, &nrlim);

  /* We just need a lotta sockets, thank you very much */
  ASSERT_LE(2048, nrlim.rlim_cur); // somewhat arbitrary for gtest purposes, real system(s) need tons - 16k 32k or more

  int lastnnsk = 0;
  int i;
  for (i=0; i<NN_MAX_SOCKETS+1; i++) {
    nnsk[i] = nn_socket (AF_SP, NN_PAIR);
    if (nnsk[i] < 0) {
      fprintf(stderr, "nn_socket got to nnsk[i=%d] (nnsk[i-1]=%d, rlim.nofile=%lu): errno=%d/'%s'\n", i, i>0?nnsk[i-1]:-1, nrlim.rlim_cur, errno, strerror(errno));
      break;
    } else {
      lastnnsk = nnsk[i];
    }
#if 1
    ASSERT_EQ(get_nn_sock_connection_count(nnsk[i], "nnsk[i] - after nn_socket()"), 0);
#endif
  }
  /* Should get up to nearly half of rlim.  There are two eventfd's per idle socket. */
  if (lastnnsk+1 < NN_MAX_SOCKETS) {
    ASSERT_GE(i*2, (nrlim.rlim_cur - 100));
  } else {
    /* Forget rlimit, we can open all possible NN sockets! */
  }

  while(--i >= 0) {
    if (nnsk[i] >= 0) {
      r = nn_close(nnsk[i]);
      if (r) {
	fprintf(stderr, "err close(nnsk[i=%d]=%d): '%s'\n", i, nnsk[i], strerror(errno));
      }
      ASSERT_EQ(r, 0);
#if 1
      ASSERT_EQ(get_nn_sock_connection_count(nnsk[i], "nnsk[i] - after nn_close()"), -1);
#endif
    }
  }

  setrlimit(RLIMIT_NOFILE, &orlim);

  nnchk();
}

/* Problem, this only works when all prior nn sockets have been closed properly */
TEST(RWMsgNN, BindToFd) {
  TEST_DESCRIPTION("Tests RiftWare NN extension: nn_connect(sk, 'tcp://:fd123')");

  int sk[2];
  int r;

  r = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sk);
  ASSERT_EQ(r, 0);

  int nnsk[2];

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }


  nnsk[0] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[0], 0);
  nnsk[1] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[1], 0);

  char abuf[64];

  int eid[2];
  sprintf(abuf, "tcp://:fd%d", sk[0]);
  eid[0] = nn_connect (nnsk[0], abuf);
  ASSERT_TRUE(eid[0] >= 0);

  sprintf(abuf, "tcp://:fd%d", sk[1]);
  eid[1] = nn_connect (nnsk[1], abuf);
  ASSERT_TRUE(eid[1] >= 0);

  sleep(1);
#if 1
    ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_connect()"), 1);
    ASSERT_EQ(get_nn_sock_connection_count(nnsk[1], "nnsk[1] - after nn_connect()"), 1);
#endif

  char sndmsg[] = "ABC";
  char rcvmsg[64];

  r = nn_send(nnsk[0], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);

  r = nn_recv(nnsk[1], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  r = nn_send(nnsk[1], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  
  r = nn_recv(nnsk[0], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  //RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);
  
  nn_close(nnsk[0]);
  nn_close(nnsk[1]);

  nnchk();
}

rwmsg_bool_t g_nn_connection_down_cb_called=FALSE;
static void my_nn_connection_down_cb(void *handle, int conns) {
  int *nnp = (int*)(handle);
  fprintf(stderr, "my_nn_connection_down_cb called for fd=%d\n", *nnp);
  if (conns == 0) g_nn_connection_down_cb_called = TRUE;
}

/* Problem, this only works when all prior nn sockets have been closed properly */
TEST(RWMsgNN, TestConnectionDown) {
  TEST_DESCRIPTION("Tests RiftWare NN extension: nn_connect(sk, 'tcp://:fd123')");

  int sk[2];
  int r;

  r = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sk);
  ASSERT_EQ(r, 0);

  static int nnsk[2];

  if (1) {
    /* Gratuitous socket open/close; the close will delete any riftcfg
       in the nn library unless there was a leaked nn socket from a
       preceeding test. */
    int sk = nn_socket (AF_SP, NN_PAIR);
    nn_close(sk);

    struct nn_riftconfig rcfg;
    nn_global_get_riftconfig(&rcfg);
    ASSERT_FALSE(rcfg.singlethread);
    ASSERT_TRUE(rcfg.waitfunc == NULL);
  }

  nnsk[0] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[0], 0);
  nnsk[1] = nn_socket (AF_SP, NN_PAIR);
  ASSERT_GE(nnsk[1], 0);

  char abuf[64];

  int eid[2];
  sprintf(abuf, "tcp://:fd%d", sk[0]);
  eid[0] = nn_connect (nnsk[0], abuf);
  ASSERT_TRUE(eid[0] >= 0);

  sprintf(abuf, "tcp://:fd%d", sk[1]);
  eid[1] = nn_connect (nnsk[1], abuf);
  ASSERT_TRUE(eid[1] >= 0);

  sleep(1);
#if 1
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[0], "nnsk[0] - after nn_connect()"), 1);
  ASSERT_EQ(get_nn_sock_connection_count(nnsk[1], "nnsk[1] - after nn_connect()"), 1);
#endif

  struct nn_riftclosure cl;
  cl.ud = &nnsk[0];
  cl.fn = my_nn_connection_down_cb;
  r = nn_setsockopt(nnsk[0], NN_SOL_SOCKET, NN_RWSET_CONN_IND, &cl, sizeof(cl));

  char sndmsg[] = "ABC";
  char rcvmsg[64];

  r = nn_send(nnsk[0], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);

  r = nn_recv(nnsk[1], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  r = nn_send(nnsk[1], sndmsg, strlen(sndmsg)+1, 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);

  r = nn_recv(nnsk[0], rcvmsg, sizeof(rcvmsg), 0);
  ASSERT_EQ(r, strlen(sndmsg)+1);
  ASSERT_TRUE(0==strcmp(rcvmsg, sndmsg));

  g_nn_connection_down_cb_called = FALSE;
  nn_close(nnsk[1]);
  fprintf(stderr, "Going to sleep\n");
  sleep(2);

  ASSERT_TRUE(g_nn_connection_down_cb_called);

  nn_close(nnsk[0]);
  //nn_close(nnsk[1]);

  nnchk();
}

RWUT_INIT();
