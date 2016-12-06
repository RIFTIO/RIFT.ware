
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
 * @file rwmsg_gtest_broker.cc
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwmsg_broker
 * 
 * @details Google test cases for testing rwmsg_broker.
 */

/** 
 * Step 1: Include the necessary header files 
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <limits.h>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <valgrind/valgrind.h>

#include "rwut.h"

#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"
#include "rw_sys.h"
#include "rwtasklet.h"

#include <unistd.h>
#include <sys/syscall.h>

#include <rwmsg_int.h>
#include <rwmsg_broker.h>
#include "rwmsg_gtest_c.h"

#include "test.pb-c.h"
#include <ck.h>

#undef RWMSG_NOTHREAD

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

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

#define MAX_RTT 999999999999


/*
 * Step 2: Use the TEST macro to define your tests. The following
 * is the notes from Google sample test
 *
 * TEST has two parameters: the test case name and the test name.
 * After using the macro, you should define your test logic between a
 * pair of braces.  You can use a bunch of macros to indicate the
 * success or failure of a test.  EXPECT_TRUE and EXPECT_EQ are
 * examples of such macros.  For a complete list, see gtest.h.
 *
 * In Google Test, tests are grouped into test cases.  This is how we
 * keep test code organized.  You should put logically related tests
 * into the same test case.
 *
 * The test case name and the test name should both be valid C++
 * identifiers.  And you should not use underscore (_) in the names.
 *
 * Google Test guarantees that each test you define is run exactly
 * once, but it makes no guarantee on the order the tests are
 * executed.  Therefore, you should write your tests in such a way
 * that their results don't depend on their order.
 */


/* Test and tasklet environment */
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

static uint16_t broport_g=0;
#define GTEST_BASE_BROKER_PORT 31234

uint16_t rwmsg_broport_g(int tasklet_ct_in) {
  rw_status_t status;
  static uint16_t previous_tasklet_ct_in = 0;
  /* Plug in a new broker port for each test to avoid confusion.
     Each test uses the next port */
  if (!broport_g) {
    const char *envport = getenv("RWMSG_BROKER_PORT");
    if (envport) {
      long int long_port;
      long_port = strtol(envport, NULL, 10);
      if (long_port < 65535 && long_port > 0)
        broport_g = (uint16_t)long_port;
      else
        RW_ASSERT(long_port < 65535 && long_port > 0);
    } else {
      uint8_t uid;
      status = rw_unique_port(GTEST_BASE_BROKER_PORT, &broport_g);
      RW_ASSERT(status == RW_STATUS_SUCCESS);

      status = rw_instance_uid(&uid);
      RW_ASSERT(status == RW_STATUS_SUCCESS);

      // The unit test needs a range of unique ports.
      // coverage-tests taking almost 500 ports
      broport_g += 500 * uid;
    }
  } else {
    broport_g += previous_tasklet_ct_in;
  }

  // Avoid the list of known port#s
  while (rw_port_in_avoid_list(broport_g, tasklet_ct_in))
    broport_g += tasklet_ct_in;

  previous_tasklet_ct_in = tasklet_ct_in;

  return broport_g;
}

class rwmsg_btenv_t {
public:
#define TASKLET_MAX (10)
  rwmsg_btenv_t(int tasklet_ct_in=0) {

    broport = rwmsg_broport_g(tasklet_ct_in);

    char tmp[16];
    sprintf(tmp, "%d", broport);
    fprintf(stderr, "+++++++++++++++++++++++++++ %d\n", broport);
    setenv ("RWMSG_BROKER_PORT", tmp, TRUE);

    setenv("RWMSG_BROKER_ENABLE", "1", TRUE);

    setenv("RWMSG_CHANNEL_AGEOUT", "1", TRUE);

    memset(&rwmsg_broker_g, 0, sizeof(rwmsg_broker_g));

    assert(tasklet_ct_in <= TASKLET_MAX);
    rwsched = rwsched_instance_new();
    tasklet_ct = tasklet_ct_in;
    memset(&tasklet, 0, sizeof(tasklet));
    for (int i=0; i<tasklet_ct; i++) {
      tasklet[i] = rwsched_tasklet_new(rwsched);
    }
    
    ti = &ti_s;
    ti->rwvx = rwvx_instance_alloc();
    ti->rwsched_instance = rwsched;
    ti->rwvcs = ti->rwvx->rwvcs;
    rwvcs_zk = RWVX_GET_ZK_MODULE(ti->rwvx);
    RW_ASSERT(rwvcs_zk);

    rw_status_t status = rwvcs_zk_zake_init(rwvcs_zk);
    RW_ASSERT(RW_STATUS_SUCCESS == status);

  }
  ~rwmsg_btenv_t() {
    setenv("RWMSG_BROKER_PORT", "0" , TRUE);
    setenv("RWMSG_BROKER_ENABLE", "0" , TRUE);
    setenv("RWMSG_CHANNEL_AGEOUT", "0" , TRUE);
    if (rwsched) {
      for (int i=0; i<tasklet_ct; i++) {
	rwsched_tasklet_free(tasklet[i]);
	tasklet[i] = NULL;
      }
      rwsched_instance_free(rwsched);
      rwsched = NULL;
    }
    if (rwvcs_zk) {
      char rwzk_LOCK[999];
      char rwzk_path[999];
      char ** children = NULL;
      rw_status_t rs;

      sprintf(rwzk_path, "/sys/rwmsg/broker-lock");
      rs = rwvcs_zk_get_children(rwvcs_zk, rwzk_path, &children, NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS || rs == RW_STATUS_NOTFOUND);
      if (children) {
        int i=0;
        while (children[i] != NULL) {
          sprintf(rwzk_LOCK, "/sys/rwmsg/broker-lock/%s", children[i]);
          rs = rwvcs_zk_delete(rwvcs_zk, rwzk_LOCK, NULL);
          free(children[i]);
          i++;
        }
        free(children);
      }
      children = NULL;
      rs = rwvcs_zk_get_children(rwvcs_zk, rwzk_path, &children, NULL);
      if (rs == RW_STATUS_SUCCESS) {
        RW_ASSERT(!children || !children[0]);
      }
      else {
        RW_ASSERT(rs == RW_STATUS_NOTFOUND);
      }

      rwvcs_zk = NULL;
    }
  }

  int broport;
  rwsched_instance_ptr_t rwsched;
  rwtasklet_info_t ti_s;
  rwtasklet_info_t *ti;
  int tasklet_ct;
  rwsched_tasklet_ptr_t tasklet[TASKLET_MAX];
  rwvcs_zk_module_ptr_t rwvcs_zk;
};



TEST(RWMsgBroker, PlaceholderNOP) {
  TEST_DESCRIPTION("Tests nothing whatsoever");
  if (0) {
    VERBPRN(" ");
  }
}

TEST(RWMsgTestEnv, ThouShaltCoreDump) {
  TEST_DESCRIPTION("Tests rlimit coredump setting");

  struct rlimit rlim = { 0 };
  getrlimit(RLIMIT_CORE, &rlim);

  VERBPRN("Core rlimit .rlim_cur=%lu .rlim_max=%lu\n", rlim.rlim_cur, rlim.rlim_max);
  ASSERT_GT(rlim.rlim_cur, 0);
}

TEST(RWMsgBroker, CreateBindListen) {
  TEST_DESCRIPTION("Tests creation of scheduler, broker, binding, and listening");
  rwmsg_btenv_t tenv(1);
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  /* Should now accept and ultimately timeout in the broker's acceptor */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 0);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwmsg_bool_t rb = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(rb);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, CreateBindListen2Bros) {
  TEST_DESCRIPTION("Tests creation of scheduler, broker, binding, and listening");
  rwmsg_btenv_t tenv(2);
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);
  usleep(1000);
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);
  usleep(1000);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<100)
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwmsg_bool_t rb;
  rb = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(rb);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, CreateBindListen3Bros) {
  TEST_DESCRIPTION("Tests creation of scheduler, broker, binding, and listening");
  rwmsg_btenv_t tenv(3);
  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*3*(3-1); //==30

  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);
  rwmsg_broker_t *bro3=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 2, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro3);

  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<100)
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwmsg_bool_t rb;
  rb = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro3);
  ASSERT_TRUE(rb);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, CreateBindListen4Bros) {
  TEST_DESCRIPTION("Tests creation of scheduler, broker, binding, and listening");
  rwmsg_btenv_t tenv(4);
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);
  rwmsg_broker_t *bro3=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 2, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro3);
  rwmsg_broker_t *bro4=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[3];
  rwmsg_broker_main(broport_g, 3, 3, tenv.rwsched, tenv.tasklet[3], tenv.rwvcs_zk, TRUE, tenv.ti, &bro4);

  usleep(10*1000);
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //int loopwait = 15;
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*4*(4-1); //==5*2*n*(n-1)/2 60
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(50*1000); // 50ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  usleep(50*1000);

  rwmsg_bool_t rb;
  rb = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro3);
  ASSERT_TRUE(rb);
  rb = rwmsg_broker_halt_sync(bro4);
  ASSERT_TRUE(rb);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptTimeout) {
  TEST_DESCRIPTION("Tests timeout of accepted connection in broker");
  rwmsg_btenv_t tenv(1);
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0 || (r == -1 && errno == EINPROGRESS));

  /* Should now accept and ultimately timeout in the broker's acceptor */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed_err);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 0);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 1);

  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptTimeout2Bros) {
  TEST_DESCRIPTION("Tests timeout of accepted connection in broker");
  rwmsg_btenv_t tenv(2);

  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  {
  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0 || (r == -1 && errno == EINPROGRESS));

  /* Should now accept and ultimately timeout in the broker's acceptor */
  int loopwait = 10;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed_err);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 10);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 1);
  }

  int r;
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptHandshakeFail) {
  TEST_DESCRIPTION("Tests rejection of failed handshake in broker");
  rwmsg_btenv_t tenv(1);
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  //fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0);

  uint64_t hsbuf[2] = { 0x0102030405060708ull, 0x0102030405060708ull };
  r = write(sk, &hsbuf, 7);	// not 16!
  ASSERT_EQ(r, 7);

  /* Should now accept, read partial hs, and jettison from the broker's acceptor */
  int loopwait = 3;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed_err);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 0);

  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptHandshakeFail2Bros) {
  TEST_DESCRIPTION("Tests rejection of failed handshake in broker");
  rwmsg_btenv_t tenv(2);

  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  {
  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  //fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0);

  uint64_t hsbuf[2] = { 0x0102030405060708ull, 0x0102030405060708ull };
  r = write(sk, &hsbuf, 7);	// not 16!
  ASSERT_EQ(r, 7);

  /* Should now accept, read partial hs, and jettison from the broker's acceptor */
  int loopwait = 3;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed_err);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 10);
  }

  {
  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  //fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport+1);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0);

  uint64_t hsbuf[2] = { 0x0102030405060708ull, 0x0102030405060708ull };
  r = write(sk, &hsbuf, 7);	// not 16!
  ASSERT_EQ(r, 7);

  /* Should now accept, read partial hs, and jettison from the broker's acceptor */
  int loopwait = 3;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed_err);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 10);
  }

  int r;
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptHandshakeOK) {
  TEST_DESCRIPTION("Tests accepting a handshake in broker");
  rwmsg_btenv_t tenv(1);
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  //fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0);

  struct rwmsg_handshake_s handshake;
  memset(&handshake, 0, sizeof(handshake));
  handshake.chanid = 1;
  handshake.pid = getpid();
  handshake.chtype = RWMSG_CHAN_SRV;
  handshake.pri = RWMSG_PRIORITY_HIGH;

  r = write(sk, &handshake, sizeof(handshake));
  ASSERT_EQ(r, sizeof(handshake));

  /* Should now accept, read partial hs, and jettison from the broker's acceptor */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptHandshakeOK2Bros) {
  TEST_DESCRIPTION("Tests accepting a handshake in broker");
  rwmsg_btenv_t tenv(2);

  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 1, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  {
  int sk = socket(AF_INET, SOCK_STREAM, 0);
  int fl = fcntl(sk, F_GETFL);
  ASSERT_TRUE(fl != -1);
  //fl |= O_NONBLOCK;
  int r = fcntl(sk, F_SETFL, fl);
  ASSERT_TRUE(r == 0);

  struct sockaddr_in ad;
  ad.sin_family = AF_INET;
  ad.sin_port = htons(tenv.broport);
  ad.sin_addr.s_addr = htonl(RWMSG_CONNECT_IP_ADDR);

  r = connect(sk, (struct sockaddr*)&ad, sizeof(ad));
  ASSERT_TRUE(r == 0);

  struct rwmsg_handshake_s handshake;
  memset(&handshake, 0, sizeof(handshake));
  handshake.chanid = 1;
  handshake.pid = getpid();
  handshake.chtype = RWMSG_CHAN_SRV;
  handshake.pri = RWMSG_PRIORITY_HIGH;

  r = write(sk, &handshake, sizeof(handshake));
  ASSERT_EQ(r, sizeof(handshake));

  /* Should now accept, read partial hs, and jettison from the broker's acceptor */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  usleep(1000*100);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);
  }

  int r;
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptClichan) {
  TEST_DESCRIPTION("Tests accepting from a clichan");
  rwmsg_btenv_t tenv(2);


  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);
  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_bool_t r;
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, ClichanConnectionStatus) {
  TEST_DESCRIPTION("Tests accepting from a clichan");

  rw_status_t rs;
  rwmsg_endpoint_t *ep;
  rwmsg_clichan_t *cc;
  rwmsg_bool_t r;

  rwsched_tasklet_ptr_t tasklet;
  rwsched_instance_ptr_t rwsched;
  /* create a tasklet w/o setting RWMSG_BROKER_PORT that way the endpoint won't
   * have broker enabled and the clichan would only have local-only channels */
  rwsched = rwsched_instance_new();
  tasklet = rwsched_tasklet_new(rwsched);

  ep = rwmsg_endpoint_create(0, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);

  /* Returns SUCCESS if the cc is local-only */
  rs = rwmsg_clichan_connection_status(cc);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* End clichan */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  rwsched_tasklet_free(tasklet);
  rwsched_instance_free(rwsched);

  /* create the usual style tasklets w/ RWMSG_BROKER_PORT enabled 
   * so that we can test the clichan w/ remote channels which connects to the broker*/
  rwmsg_btenv_t tenv(2);

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_connection_status(cc);
  ASSERT_TRUE(rs == RW_STATUS_NOTCONNECTED);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Returns NOTCONNECTED if the cc's socket's state is not NN_CONNECTED */
  rs = rwmsg_clichan_connection_status(cc);
  ASSERT_TRUE(rs == RW_STATUS_NOTCONNECTED);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  /* Returns SUCCESS if the cc's socket's state is NN_CONNECTED */
  rs = rwmsg_clichan_connection_status(cc);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptClichan2Bros) {
  TEST_DESCRIPTION("Tests accepting from a clichan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;

  /* Broker-1 is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker-2 is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);
  
  {
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);
  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  {
  /* Clichan is tasklet 3, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);
  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptClichanAndAgeout_LONG) {
  TEST_DESCRIPTION("Tests accepting from a clichan");
  rwmsg_btenv_t tenv(2);


  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  for(int i=0; i<3; i++) {
    rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
    ASSERT_TRUE(cc != NULL);
    rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    rwmsg_broker_g.exitnow.neg_accepted = 0;
    rwmsg_broker_g.exitnow.neg_freed = 0;
    rwmsg_broker_g.exitnow.neg_freed_err = 0;

    /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
    int loopwait = 10;
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

    ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
    ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
    ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

    /* End clichan tasklet 1 */
    rwmsg_clichan_halt(cc);
  }
  rwmsg_bool_t r;
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  //rwmsg_broker_dump(bro);
  //sleep(2);
  int loopwait = 2; // wait for more than 1sec
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  rwmsg_broker_dump(bro);
  ASSERT_EQ(rwmsg_broker_g.exitnow.bch_count, 0);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndAgeout_LONG) {
  TEST_DESCRIPTION("Tests aging-out bro-srvchan");
  rwmsg_btenv_t tenv(2);


  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  for(int i=0; i<3; i++) {
    rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep);
    ASSERT_TRUE(sc != NULL);
    rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    rwmsg_broker_g.exitnow.neg_accepted = 0;
    rwmsg_broker_g.exitnow.neg_freed = 0;
    rwmsg_broker_g.exitnow.neg_freed_err = 0;

    /* Should now accept, read hs, create brosrvchan, attach incoming accepted sk */
    int loopwait = 10;
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

    ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
    ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
    ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

    /* End srvchan tasklet 1 */
    rwmsg_srvchan_halt(sc);
  }
  rwmsg_bool_t r;
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  //rwmsg_broker_dump(bro);
  //sleep(2);
  int loopwait = 2; // wait for more than 1sec
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  rwmsg_broker_dump(bro);
  ASSERT_EQ(rwmsg_broker_g.exitnow.bch_count, 0);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

void broker_sleep_test_timer(void *ud) {
  if (0) {
    fprintf(stderr, "broker_sleep_test_timer start sleep\n");
    sleep(2);
    fprintf(stderr, "broker_sleep_test_timer end sleep\n");
  }
}

TEST(RWMsgBroker, AcceptClichanSleep) {
  TEST_DESCRIPTION("Tests accepting from a clichan");
  rwmsg_btenv_t tenv(2);


  /* Broker is tasklet 0, not on main queue, although acceptor always is */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, FALSE, tenv.ti, &bro);
  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);


  /* Timer on main queue to sleep and thereby sputter broker acceptor event processing */
  rwsched_dispatch_source_t tim = rwsched_dispatch_source_create(tenv.tasklet[1],
								 RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
								 0,
								 0,
								 rwsched_dispatch_get_main_queue(tenv.rwsched));
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[1],
					      tim,
					      broker_sleep_test_timer);
  rwsched_dispatch_set_context(tenv.tasklet[1],
			       tim,
			       NULL);
  rwsched_dispatch_source_set_timer(tenv.tasklet[1],
				    tim,
				    dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC),
				    100ull * NSEC_PER_SEC / 1000000ull,
				    0);
  rwsched_dispatch_resume(tenv.tasklet[1], tim);

  /* Make a serial queue for clichan */
  rwsched_dispatch_queue_t q = rwsched_dispatch_queue_create(tenv.tasklet[0], "q", DISPATCH_QUEUE_SERIAL);

  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, q);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 15;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 1\n");

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");


  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwsched_dispatch_source_cancel(tenv.tasklet[1], tim);
  rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, tim, tenv.rwsched, tenv.tasklet[1]);


  /* End clichan tasklet 1 */
  rwmsg_bool_t r;
  rwmsg_clichan_halt(cc);
  rwsched_dispatch_release(tenv.tasklet[0], q);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptClichanSleep2Bros) {
  TEST_DESCRIPTION("Tests accepting from a clichan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;


  /* Broker-1 is tasklet 0, not on main queue, although acceptor always is */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, FALSE, tenv.ti, &bro1);
  
  /* Broker-2 is tasklet 2, not on main queue, although acceptor always is */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, FALSE, tenv.ti, &bro2);
  
  {
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);


  /* Timer on main queue to sleep and thereby sputter broker acceptor event processing */
  rwsched_dispatch_source_t tim = rwsched_dispatch_source_create(tenv.tasklet[1],
								 RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
								 0,
								 0,
								 rwsched_dispatch_get_main_queue(tenv.rwsched));
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[1],
					      tim,
					      broker_sleep_test_timer);
  rwsched_dispatch_set_context(tenv.tasklet[1],
			       tim,
			       NULL);
  rwsched_dispatch_source_set_timer(tenv.tasklet[1],
				    tim,
				    dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC),
				    100ull * NSEC_PER_SEC / 1000000ull,
				    0);
  rwsched_dispatch_resume(tenv.tasklet[1], tim);

  /* Make a serial queue for clichan */
  rwsched_dispatch_queue_t q = rwsched_dispatch_queue_create(tenv.tasklet[0], "q", DISPATCH_QUEUE_SERIAL);

  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, q);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 1\n");

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwsched_dispatch_source_cancel(tenv.tasklet[1], tim);
  rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, tim, tenv.rwsched, tenv.tasklet[1]);


  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  rwsched_dispatch_release(tenv.tasklet[0], q);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

#if 0
  {
  /* Clichan is tasklet 3, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ASSERT_TRUE(cc != NULL);


  /* Timer on main queue to sleep and thereby sputter broker acceptor event processing */
  rwsched_dispatch_source_t tim = rwsched_dispatch_source_create(tenv.tasklet[3],
								 RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
								 0,
								 0,
								 rwsched_dispatch_get_main_queue(tenv.rwsched));
  rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[3],
					      tim,
					      broker_sleep_test_timer);
  rwsched_dispatch_set_context(tenv.tasklet[3],
			       tim,
			       NULL);
  rwsched_dispatch_source_set_timer(tenv.tasklet[3],
				    tim,
				    dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC),
				    100ull * NSEC_PER_SEC / 1000000ull,
				    0);
  rwsched_dispatch_resume(tenv.tasklet[3], tim);

  /* Make a serial queue for clichan */
  rwsched_dispatch_queue_t q = rwsched_dispatch_queue_create(tenv.tasklet[3], "q", DISPATCH_QUEUE_SERIAL);

  rw_status_t rs = rwmsg_clichan_bind_rwsched_queue(cc, q);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 1\n");

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");


  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  rwsched_dispatch_source_cancel(tenv.tasklet[1], tim);
  rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, tim, tenv.rwsched, tenv.tasklet[1]);


  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  rwsched_dispatch_release(tenv.tasklet[1], q);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }
#endif

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchan) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(2);


  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);

  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_srvchan_t *cc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(cc != NULL);
  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_bool_t r;
  rwmsg_srvchan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchan2Bros) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;


  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  {
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_srvchan_t *cc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(cc != NULL);
  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_srvchan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  {
  /* Clichan is tasklet 3, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  rwmsg_srvchan_t *cc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(cc != NULL);

  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_srvchan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

static void Test_increment(Test_Service *,
			   const TestReq *req, 
			   void *ud,
			   TestRsp_Closure clo, 
			   void *rwmsg);
static void Test_fail(Test_Service *,
		      const TestReq *req, 
		      void *ud,
		      TestNop_Closure clo, 
		      void *rwmsg);
void multibrocfb_response(const TestRsp *rsp, rwmsg_request_t *req, void *ud) {
  //struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo *)ud;
  //fprintf(stderr, "req->hdr.bnc=%d\n",req->hdr.bnc);
  //ASSERT_EQ(req->hdr.bnc, 0);
  //ASSERT_TRUE(rsp != NULL);
  if (rsp) {
    ASSERT_EQ(rsp->errval, 0);
    ASSERT_EQ(rsp->body.hopct+1, rsp->body.value);
    //tinfo->rsp_recv++;
    VERBPRN("ProtobufCliSrvCFNonblocking test got rsp, value=%u errval=%u\n", rsp->body.value, rsp->errval);
  } else {
    //tinfo->bnc_recv++;
    VERBPRN("ProtobufCliSrvCFNonblocking test got bnc=%d\n", (int)req->hdr.bnc);
  }

  return;
  rsp=rsp;
  req=req;
  ud=ud;
}


TEST(RWMsgBroker, AcceptSrvchanAndCliChanW1Bro_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  //rs = rwmsg_srvchan_add_service(sc, tpath, &myapisrv.base, &tenv.tasklet[3]);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  TestReq req;
  rwmsg_request_t *rwreq=NULL;
  test_req__init(&req);
  // fprintf(stderr, "test__increment - 01\n");
#if 1
  rwmsg_closure_t cb;
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
#else
  rs = test__increment_b(&mycli, dt, &req, &rwreq);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
#endif

  //  fprintf(stderr, "end dispatch_main 1\n");

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

const char *bnc_test_msg = "This Bounces";

static void bnc_test_meth_cb(rwmsg_request_t *req, void *ctx_in) {
  fprintf(stderr, "inside bnc_test_meth_cb()\n");
  return;
  req = req;
  ctx_in = ctx_in;
}

static void bnc_test_rsp_cb(rwmsg_request_t *req, void *ctx_in) {
  ASSERT_TRUE(req->hdr.bnc);
  fprintf(stderr, "inside bnc_test_rsp_cb()\n");
  rwmsg_flowid_t fid = rwmsg_request_get_response_flowid(req);
#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
  uint32_t len=0;
  char *rsp = (char*)rwmsg_request_get_bnc_response_payload(req, &len);
  rsp = rsp;
  ASSERT_EQ(len, strlen(bnc_test_msg));
#endif
  return;
  req = req;
  fid = fid;
  ctx_in = ctx_in;
}

typedef struct bnc_test_ud {
  rwmsg_clichan_t *cc;
  rwmsg_destination_t *dt;
  uint32_t methno;
} bnc_test_ud_t;

#if 0
static void bnc_test_feedme(void *ctx_in, rwmsg_destination_t *dest, rwmsg_flowid_t flowid) {
}
#endif

static void bnc_test_rsp_cb_and_set_feedme(rwmsg_request_t *req, void *ctx_in) {
  ASSERT_TRUE(req->hdr.bnc);
  fprintf(stderr, "inside bnc_test_rsp_cb()\n");
  rwmsg_flowid_t fid = rwmsg_request_get_response_flowid(req);
#if 0
    bnc_test_ud_t *ud = (bnc_test_ud_t*)ctx_in;
    rwmsg_closure_t cb = { };
    cb.ud = NULL;
    cb.feedme = bnc_test_feedme;
    rw_status_t rs = rwmsg_clichan_feedme_callback(ud->cc, ud->dt, fid, &cb);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
#endif

#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
  uint32_t len=0;
  char *rsp = (char*)rwmsg_request_get_bnc_response_payload(req, &len);
  rsp = rsp;
  ASSERT_EQ(len, strlen(bnc_test_msg));
#endif
  return;
  req = req;
  fid = fid;
  ctx_in = ctx_in;
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanTOBnc) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method */
  rwmsg_signature_t *sig = rwmsg_signature_create(ep_s, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  int timeo = 50;		// ms
  if (RUNNING_ON_VALGRIND) {
    timeo = timeo * 5;
  }
  rwmsg_signature_set_timeout(sig, timeo);
  rwmsg_closure_t methcb;
  methcb.request=bnc_test_meth_cb;
  methcb.ud=ep_s;
  rwmsg_method_t *meth = rwmsg_method_create(ep_s, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  /* Wait on binding the srvchan until after we've sent the initial
     flurry of requests.  Otherwise, our ++ and the request handler's
     ++ of some of the request count variables ABA-stomp each
     other. */

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Create the clichan, add the method's signature */

  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

    rwmsg_request_t *req = rwmsg_request_create(cc);
    rwmsg_request_set_signature(req, sig);
    rwmsg_closure_t clicb;
    clicb.request=bnc_test_rsp_cb;
    clicb.ud=cc;
    rwmsg_request_set_callback(req, &clicb);

    rwmsg_request_set_payload(req, bnc_test_msg, strlen(bnc_test_msg));

    /* Send it.  The srvchan should more or less immediately run the
       thing and the methcb will respond. */
    rs = rwmsg_clichan_send(cc, dt, req);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_signature_release(ep_s, sig);
  rwmsg_method_release(ep_s, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanNoPeerBnc) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method */
  rwmsg_signature_t *sig = rwmsg_signature_create(ep_s, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  int timeo = 1000;		// ms
  if (RUNNING_ON_VALGRIND) {
    timeo = timeo * 5;
  }
  rwmsg_signature_set_timeout(sig, timeo);
  rwmsg_closure_t methcb;
  methcb.request=bnc_test_meth_cb;
  methcb.ud=ep_s;
  rwmsg_method_t *meth = rwmsg_method_create(ep_s, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  /* Wait on binding the srvchan until after we've sent the initial
     flurry of requests.  Otherwise, our ++ and the request handler's
     ++ of some of the request count variables ABA-stomp each
     other. */

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Create the clichan, add the method's signature */

  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

    rwmsg_signature_t *sig_bad = rwmsg_signature_create(ep_c, RWMSG_PAYFMT_RAW, methno+666, RWMSG_PRIORITY_DEFAULT);
    ASSERT_TRUE(sig_bad);
    rwmsg_request_t *req = rwmsg_request_create(cc);
    rwmsg_request_set_signature(req, sig_bad);
    rwmsg_closure_t clicb;
    clicb.request=bnc_test_rsp_cb;
    clicb.ud=cc;
    rwmsg_request_set_callback(req, &clicb);

    rwmsg_request_set_payload(req, bnc_test_msg, strlen(bnc_test_msg));

    /* Send it.  The srvchan should more or less immediately run the
       thing and the methcb will respond. */
    rs = rwmsg_clichan_send(cc, dt, req);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_signature_release(ep_c, sig_bad);
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_signature_release(ep_s, sig);
  rwmsg_method_release(ep_s, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanTestFlowCtrlHint) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method */
  rwmsg_signature_t *sig = rwmsg_signature_create(ep_s, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  int timeo = 50;		// ms
  if (RUNNING_ON_VALGRIND) {
    timeo = timeo * 5;
  }
  rwmsg_signature_set_timeout(sig, timeo);
  rwmsg_closure_t methcb;
  methcb.request=bnc_test_meth_cb;
  methcb.ud=ep_s;
  rwmsg_method_t *meth = rwmsg_method_create(ep_s, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  /* Wait on binding the srvchan until after we've sent the initial
     flurry of requests.  Otherwise, our ++ and the request handler's
     ++ of some of the request count variables ABA-stomp each
     other. */

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Create the clichan, add the method's signature */

  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

    rwmsg_request_t *req = rwmsg_request_create(cc);
    rwmsg_request_set_signature(req, sig);
    rwmsg_closure_t clicb;
    clicb.request=bnc_test_rsp_cb_and_set_feedme;
    bnc_test_ud_t *ud = (bnc_test_ud_t *) malloc(sizeof(*ud));
    ud->cc = cc;
    ud->dt = dt;
    ud->methno = methno;

    clicb.ud=ud;
    rwmsg_request_set_callback(req, &clicb);

    rwmsg_request_set_payload(req, bnc_test_msg, strlen(bnc_test_msg));

    ASSERT_NE(rwmsg_clichan_can_send_howmuch(cc, dt, methno, RWMSG_PAYFMT_RAW), 0);

    /* Send it.  The srvchan should more or less immediately run the
       thing and the methcb will respond. */
    rs = rwmsg_clichan_send(cc, dt, req);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_signature_release(ep_s, sig);
  rwmsg_method_release(ep_s, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, CloseSrvChanW1Bro) {
  uint32_t start_reqct = rwmsg_global.status.request_ct;
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  //rs = rwmsg_srvchan_add_service(sc, tpath, &myapisrv.base, &tenv.tasklet[3]);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  TestReq req;
  rwmsg_request_t *rwreq=NULL;
  test_req__init(&req);
  // fprintf(stderr, "test__increment - 01\n");
  rwmsg_closure_t cb;
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  for (int i=0; i<5; i++) {
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }

  //  fprintf(stderr, "end dispatch_main 1\n");

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif

  int sent_N_after_srvr_close = 10;
  for (int i=0; i<sent_N_after_srvr_close; i++) {
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_LE(start_reqct+sent_N_after_srvr_close, rwmsg_global.status.request_ct);
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanAndSndBlkW1Bro) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  //rs = rwmsg_srvchan_add_service(sc, tpath, &myapisrv.base, &tenv.tasklet[3]);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_srvchan_bind_rwsched_cfrunloop(sc, tenv.tasklet[1]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  //rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(cc, tenv.tasklet[2]);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  TestReq req;
  rwmsg_request_t *rwreq=NULL;
  test_req__init(&req);
  // fprintf(stderr, "test__increment - 01\n");
  rs = test__increment_b(&mycli, dt, &req, &rwreq);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //  fprintf(stderr, "end dispatch_main 1\n");

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanAndSndNonConnDontQW1Bro_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";


  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);

  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  //rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(cc, tenv.tasklet[2]);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    //dt->noconndontq = 1;
    rwmsg_destination_set_noconndontq(dt);
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_NOTCONNECTED);

    //dt->noconndontq = 0;
    rwmsg_destination_unset_noconndontq(dt);
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }

  //  fprintf(stderr, "end dispatch_main 1\n");

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  //rs = rwmsg_srvchan_add_service(sc, tpath, &myapisrv.base, &tenv.tasklet[3]);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_srvchan_bind_rwsched_cfrunloop(sc, tenv.tasklet[1]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }

  //  fprintf(stderr, "end dispatch_main 2\n");

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanOnSame2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(50*1000); // 50ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);



#if 1
  // fprintf(stderr, "\ntest__increment - clican-to-bro-1\n");
  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;
  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c_b0;
  ep_c_b0 = rwmsg_endpoint_create(0, 1, 1, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c_b0 != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c_b0, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc0 = rwmsg_clichan_create(ep_c_b0);
  ASSERT_TRUE(cc0 != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc0, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc0, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  int i;
  for (i=0; i<100; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);

    loopwait = 0.01;
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }
#endif

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc0);
  r = rwmsg_endpoint_halt_flush(ep_c_b0, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanAndSndBlkOnSame2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(50*1000); // 50ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_srvchan_bind_rwsched_cfrunloop(sc, tenv.tasklet[3]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 1, 1, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  //rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(cc, tenv.tasklet[1]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  for (int i=0; i<100; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
#if 0
    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);
#else
    req.body.value = i+5;
    req.body.hopct = i+5;

    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    
    ASSERT_EQ(RWMSG_PAYFMT_PBAPI, rwmsg_request_get_response_payfmt(rwreq));

    const TestRsp *rsp;
    rs = rwmsg_request_get_response(rwreq, (const void **)&rsp);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    // fprintf(stderr, "ProtobufCliSrvCFBlocking test got rsp, errval=%u, burst i=%u\n", rsp->errval, i);

    multibrocfb_response(rsp, rwreq, tenv.tasklet[1]);
#endif
  }

  for (int i=0; i<10; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);

    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);
  }

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanOnSeperate2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(5*1000); // 5ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


#if 1
  // fprintf(stderr, "\ntest__increment - clican-to-bro-0\n");
  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;
  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c_b0;
  ep_c_b0 = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c_b0 != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c_b0, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc0 = rwmsg_clichan_create(ep_c_b0);
  ASSERT_TRUE(cc0 != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc0, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc0, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = .2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  sleep(1);

  rwmsg_global.status.request_ct = 0;

#define AcceptSrvchanAndCliChanOnSeperate2Bros_COUNT 5
  int i;
  rwmsg_request_t *rwreq=NULL;
  for (i=0; i<AcceptSrvchanAndCliChanOnSeperate2Bros_COUNT; i++) {
    TestReq req;
    test_req__init(&req);
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);


    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);


    //loopwait = 0.001;
    //rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);
#endif

  //usleep(500*1000);
  loopwait = 2.0;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  rwmsg_request_release(rwreq);

  ASSERT_LE(rwmsg_global.status.request_ct, 0);

#if 0
  fprintf(stderr, "rwmsg_broker_dump = bro1\n");
  rwmsg_broker_dump((rwmsg_broker_t *)bro1);

  fprintf(stderr, "\nrwmsg_broker_dump = bro2\n");
  rwmsg_broker_dump((rwmsg_broker_t *)bro2);
#endif

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc0);
  r = rwmsg_endpoint_halt_flush(ep_c_b0, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanAndSndBlkOnSeperate2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(50*1000); // 50ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_srvchan_bind_rwsched_cfrunloop(sc, tenv.tasklet[3]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  //rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(cc, tenv.tasklet[1]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  for (int i=0; i<100; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
#if 0
    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);
#else
    req.body.value = i+5;
    req.body.hopct = i+5;

    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    
    ASSERT_EQ(RWMSG_PAYFMT_PBAPI, rwmsg_request_get_response_payfmt(rwreq));

    const TestRsp *rsp;
    rs = rwmsg_request_get_response(rwreq, (const void **)&rsp);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    // fprintf(stderr, "ProtobufCliSrvCFBlocking test got rsp, errval=%u, burst i=%u\n", rsp->errval, i);

    multibrocfb_response(rsp, rwreq, tenv.tasklet[1]);
#endif
  }

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanAndCFSndBlkOnSeperate2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_srvchan_bind_rwsched_cfrunloop(sc, tenv.tasklet[3]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  //rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(cc, tenv.tasklet[1]);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  for (int i=0; i<100; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
#if 0
    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);
#else
    req.body.value = i+5;
    req.body.hopct = i+5;

    rs = test__increment_b(&mycli, dt, &req, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    
    ASSERT_EQ(RWMSG_PAYFMT_PBAPI, rwmsg_request_get_response_payfmt(rwreq));

    const TestRsp *rsp;
    rs = rwmsg_request_get_response(rwreq, (const void **)&rsp);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    // fprintf(stderr, "ProtobufCliSrvCFBlocking test got rsp, errval=%u, burst i=%u\n", rsp->errval, i);

    multibrocfb_response(rsp, rwreq, tenv.tasklet[1]);
#endif
  }

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 2, FALSE); 

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

// This gtest is covering multibroker channel stall fixed in RIFT-9058
TEST(RWMsgBroker, CheckStuckMessagesOnSeperate2Bros_LONG) {
  rwmsg_global.status.endpoint_ct = 0;
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;


  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(5*1000); // 5ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


#if 1
  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;
  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c_b0;
  ep_c_b0 = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c_b0 != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c_b0, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);

  rwmsg_clichan_t *cc0 = rwmsg_clichan_create(ep_c_b0);
  ASSERT_TRUE(cc0 != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc0, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc0, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = .2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  sleep(1);

  rwmsg_global.status.request_ct = 0;

#define CheckStuckMessagesOnSeperate2Bros_COUNT 15
  int i;
  rwmsg_request_t *rwreq=NULL;
  for (i=0; i<CheckStuckMessagesOnSeperate2Bros_COUNT; i++) {
    TestReq req;
    test_req__init(&req);
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_TRUE(rs==RW_STATUS_SUCCESS);
  }

  loopwait = 2.0;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);


  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);
#endif

  //rwmsg_request_release(rwreq);

  ASSERT_LE(rwmsg_global.status.request_ct, 1);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc0);
  r = rwmsg_endpoint_halt_flush(ep_c_b0, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, AcceptSrvchanAndCliChanW3Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(5);
  rwmsg_bool_t r;
  rw_status_t rs;


  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  /* Broker is tasklet 4 */
  rwmsg_broker_t *bro3=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[4];
  rwmsg_broker_main(broport_g, 4, 2, tenv.rwsched, tenv.tasklet[4], tenv.rwvcs_zk, TRUE, tenv.ti, &bro3);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*3*(3-1); //==5*2*n*(n-1)/2 == 30
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(50*1000); // 50ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 2
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 2, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  //rs = rwmsg_srvchan_add_service(sc, tpath, &myapisrv.base, &tenv.tasklet[3]);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);




  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 1, 1, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  int i;
  for (i=0; i<10; i++) {
    TestReq req;
    rwmsg_request_t *rwreq=NULL;
    test_req__init(&req);
    // fprintf(stderr, "test__increment - 01\n");
    rwmsg_closure_t cb;
    cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
    cb.ud=tenv.tasklet[1];

    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  //  fprintf(stderr, "end dispatch_main 2\n");

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif


  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro3);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, ReCreateSrvChanW1Bro_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(3);
  rwmsg_bool_t r;
  rw_status_t rs;

  setenv("RWMSG_CHANNEL_AGEOUT", "60", TRUE);

  //const char *taddr = "/L/GTEST/RWBRO/TESTAPP/PBSCF/100/3";
  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  double loopwait = 0.1;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 5;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


  /* Clichan is tasklet 2, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(0, 2, 0, tenv.rwsched, tenv.tasklet[2], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ASSERT_TRUE(cc != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  TestReq req;
  rwmsg_request_t *rwreq=NULL;
  test_req__init(&req);
  rwmsg_closure_t cb;
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  int send_some = 5;
  for (int i=0; i<send_some; i++) {
    req.body.value = i;
    req.body.hopct = i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End srvchan tasklet 2 */
  VERBPRN("\n\n\n\n-------------- End srvchan\n");
  rwmsg_srvchan_halt(sc);
  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);

  int send_some_more = 2;
#if 1
  VERBPRN("\n\n\n\n-------------- send_some_more %u\n", send_some_more);
  for (int i=0; i<send_some_more; i++) {
    req.body.value = 50+i;
    req.body.hopct = 50+i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }
#endif


#if 1
  VERBPRN("\n\n\n\n-------------- Create a Server Channel\n");
  sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);
#endif


  send_some_more = 3;
  VERBPRN("\n\n\n\n-------------- send_some_more %u\n", send_some_more);
  for (int i=0; i<send_some_more; i++) {
    req.body.value = 100+i;
    req.body.hopct = 100+i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  loopwait = 2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);

  VERBPRN("\n\n\n\n-------------- End srvchan & clichan\n");
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc);
  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, ReCreateSrvChanW2Bros_LONG) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests accepting from a srvchan");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;
  rw_status_t rs;

  setenv("RWMSG_CHANNEL_AGEOUT", "60", TRUE);

  const char *taddr = "/L/GTEST/RWBRO/1";

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);

  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);

  double loopwait = 0.1;
  int loopcount = 0;
  uint32_t meshcount = 5*2*(2-1); //==5*2*n*(n-1)/2 == 10
  /* Should now accept and ultimately timeout in the broker's acceptor */
  //rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.acc_listening);
  while (rwmsg_broker_g.exitnow.neg_accepted<meshcount && loopcount++<200) {
    usleep(5*1000); // 5ms
    rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);
  }

  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_accepted, meshcount);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* Srvchan is tasklet 3, attached to broker 0
   * also bound to main rws queue to avoid CF/dispatch headachess
   * This endpoint has bro_instid=0 */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);
  // fprintf(stderr, "\nSrvchan-to-bro-1\n");
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  rwmsg_srvchan_t *sc;
  sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  Test_Service myapisrv;
  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[3], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);


#if 1
  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;
  /* Clichan is tasklet 1, attached to broker 1
   * also bound to main rws queue to avoid CF/dispatch headaches
   * This endpoint has bro_instid=1 */
  rwmsg_endpoint_t *ep_c_b0;
  ep_c_b0 = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c_b0 != NULL);
  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c_b0, "/rwmsg/broker/shunt", 1);

  /* Create a client channel pointing at the Test service's path.
   * Instantiate a Test service client and connect this to the client
   * channel.
   */
  rwmsg_destination_t *dt;
  //dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, tpeerpath);
  dt = rwmsg_destination_create(ep_c_b0, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  
  rwmsg_clichan_t *cc0 = rwmsg_clichan_create(ep_c_b0);
  ASSERT_TRUE(cc0 != NULL);

  rs = rwmsg_clichan_bind_rwsched_queue(cc0, rwsched_dispatch_get_main_queue(tenv.rwsched));

  Test_Client mycli;
  TEST__INITCLIENT(&mycli);
  rwmsg_clichan_add_service(cc0, &mycli.base);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  loopwait = .2;
  rwsched_dispatch_main_until(tenv.tasklet[1], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  sleep(1);

  rwmsg_global.status.request_ct = 0;

  TestReq req;
  rwmsg_request_t *rwreq=NULL;
  test_req__init(&req);
  rwmsg_closure_t cb;
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  int send_some = 5;
  for (int i=0; i<send_some; i++) {
    req.body.value = i;
    req.body.hopct = i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);
#endif

  VERBPRN("\n\n\n\n-------------- End srvchan, send_some_more, recreate & send_some_more\n");
#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  test_req__init(&req);
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  int send_some_more = 5;
  for (int i=0; i<send_some_more; i++) {
    req.body.value = 50+i;
    req.body.hopct = 50+i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(sc != NULL);

  TEST__INITSERVER(&myapisrv, Test_);
  rs = rwmsg_srvchan_add_service(sc, taddr, &myapisrv.base, &tenv.tasklet[3]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  loopwait = 1;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  test_req__init(&req);
  cb.pbrsp=(rwmsg_pbapi_cb)multibrocfb_response;
  cb.ud=tenv.tasklet[2];

  send_some_more = 5;
  for (int i=0; i<send_some_more; i++) {
    req.body.value = 100+i;
    req.body.hopct = 100+i;
    rs = test__increment(&mycli, dt, &req, &cb, &rwreq);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    loopwait = 0.001;
    rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);
  }
#endif

  //usleep(500*1000);
  loopwait = 1.0;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, NULL);//&rwmsg_broker_g.exitnow.neg_freed);

  rwmsg_request_release(rwreq);

  //ASSERT_LE(rwmsg_global.status.request_ct, 0);

#if 0
  fprintf(stderr, "rwmsg_broker_dump = bro1\n");
  rwmsg_broker_dump((rwmsg_broker_t *)bro1);

  fprintf(stderr, "\nrwmsg_broker_dump = bro2\n");
  rwmsg_broker_dump((rwmsg_broker_t *)bro2);
#endif

#if 1
  /* End clichan tasklet 1 */
  rwmsg_clichan_halt(cc0);
  r = rwmsg_endpoint_halt_flush(ep_c_b0, TRUE);
  ASSERT_TRUE(r);
#endif

#if 1
  /* End srvchan tasklet 2 */
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);
#endif

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}


static void reqcb1(rwmsg_request_t *req, void *ud) {
  req=req;
  ud=ud;
}

TEST(RWMsgBroker, BindSrvchan) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests method binds a srvchan to a broker");
  rwmsg_btenv_t tenv(2);

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro);
  
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 1, 0, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  const char *taddr = "/L/GTEST/RWBRO/TESTAPP/1";
  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);
  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  rwmsg_signature_t *sig;
  const int methno = 5;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_EQ(rwmsg_broker_g.exitnow.method_bindings, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_bool_t r;
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  /* End broker */
  r = rwmsg_broker_halt_sync(bro);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}

TEST(RWMsgBroker, BindSrvchan2Bros) {
  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  TEST_DESCRIPTION("Tests method binds a srvchan to a broker");
  rwmsg_btenv_t tenv(4);
  rwmsg_bool_t r;

  /* Broker is tasklet 0 */
  rwmsg_broker_t *bro1=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[0];
  rwmsg_broker_main(broport_g, 0, 0, tenv.rwsched, tenv.tasklet[0], tenv.rwvcs_zk, TRUE, tenv.ti, &bro1);
  
  /* Broker is tasklet 2 */
  rwmsg_broker_t *bro2=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[2];
  rwmsg_broker_main(broport_g, 2, 1, tenv.rwsched, tenv.tasklet[2], tenv.rwvcs_zk, TRUE, tenv.ti, &bro2);
  
  {
  /* Clichan is tasklet 1, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  const char *taddr = "/L/GTEST/RWBRO/TESTAPP/2";
  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);
  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  rwmsg_signature_t *sig;
  const int methno = 5;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[0], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  //ASSERT_EQ(rwmsg_broker_g.exitnow.method_bindings, 2);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_accepted, 1);
  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  {
  /* Clichan is tasklet 3, also bound to main rws queue to avoid CF/dispatch headaches */
  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(0, 3, 1, tenv.rwsched, tenv.tasklet[3], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);
  const char *taddr = "/L/GTEST/RWBRO/TESTAPP/3";
  rwmsg_srvchan_t *sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(sc != NULL);
  rw_status_t rs = rwmsg_srvchan_bind_rwsched_queue(sc, rwsched_dispatch_get_main_queue(tenv.rwsched));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  rwmsg_signature_t *sig;
  const int methno = 5;
  sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig != NULL);

  rwmsg_closure_t cb;
  cb.request=reqcb1;
  cb.ud=NULL;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &cb);
  ASSERT_TRUE(meth != NULL);

  rs = rwmsg_srvchan_add_method(sc, meth);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  rwmsg_broker_g.exitnow.method_bindings = 0;
  rwmsg_broker_g.exitnow.neg_accepted = 0;
  rwmsg_broker_g.exitnow.neg_freed = 0;
  rwmsg_broker_g.exitnow.neg_freed_err = 0;

  /* Should now accept, read hs, create broclichan, attach incoming accepted sk */
  int loopwait = 10;
  rwsched_dispatch_main_until(tenv.tasklet[2], loopwait, &rwmsg_broker_g.exitnow.neg_freed);

  ASSERT_GE(rwmsg_broker_g.exitnow.neg_freed, 1);
  ASSERT_EQ(rwmsg_broker_g.exitnow.neg_freed_err, 0);

  /* End clichan tasklet 1 */
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(sc);
  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);
  }

  /* End broker */
  r = rwmsg_broker_halt_sync(bro1);
  ASSERT_TRUE(r);
  r = rwmsg_broker_halt_sync(bro2);
  ASSERT_TRUE(r);

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();
}


struct mycfbctx {
  int doblocking;
  int done;
  uint64_t tid;
  int assertsinglethread;
  uint32_t msgct_end;
  uint32_t burst;
  rwsched_instance_ptr_t rwsched;
  rwsched_CFRunLoopRef loop;
  uint32_t tickct;		// number of timer ticks testwide
  uint32_t timerct;		// number of timers that exist
  rwmsg_broker_t *bro;
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

static void brodump_f(void *val) {
  fprintf(stderr, "Gratuitous call to broker dump for smoketest/coverage reasons:\n");
  rwmsg_broker_dump((rwmsg_broker_t *)val);
}

static void mycfb_response(const TestRsp *rsp, rwmsg_request_t *req, void *ud) {
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo *)ud;

  VERBPRN("req->hdr.bnc=%d\n",req->hdr.bnc);
  // fprintf(stderr,"req->hdr.bnc=%d\n",req->hdr.bnc);

  {
    static int once=0;
    if (tinfo->ctx->bro && !once) {
      once=1;
      rwsched_dispatch_async_f(tinfo->ctx->bro->tinfo,
			       rwsched_dispatch_get_main_queue(tinfo->ctx->rwsched),
			       tinfo->ctx->bro,
			       brodump_f);
    }
  }

  //  ASSERT_TRUE(rsp != NULL);
  if (rsp) {
    ASSERT_EQ(rsp->errval, 0);
    ASSERT_EQ(rsp->body.hopct+1, rsp->body.value);
    tinfo->rsp_recv++;
    VERBPRN("ProtobufCliSrvCFNonblocking test got rsp, value=%u errval=%u\n", rsp->body.value, rsp->errval);
  } else {
    tinfo->bnc_recv++;
    VERBPRN("ProtobufCliSrvCFNonblocking test got bnc=%d\n", (int)req->hdr.bnc);
  }

  return;
  req=req;
}

#define GETTID() syscall(SYS_gettid)

static void mycfb_timer_cb(rwsched_CFRunLoopTimerRef timer, void *info) {
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo *)info;

  ASSERT_TRUE(tinfo->cftim != NULL);

  /*oops  ASSERT_EQ(tinfo->cftim, timer);*/

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

  if (tinfo->req_sent >= tinfo->ctx->msgct_end) {

    if (tinfo->req_sent <= (tinfo->bnc_recv + tinfo->rsp_recv)) {

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

    }

    return;
  }


  TestReq req;
  test_req__init(&req);
  
  for (uint32_t i=0; i<tinfo->ctx->burst; i++) {
    rwmsg_request_t *rwreq=NULL;

    req.body.value = tinfo->req_sent;
    req.body.hopct = tinfo->req_sent;

    if (tinfo->ctx->doblocking) {
      rw_status_t rs;
      rwreq = NULL;

      VERBPRN("ProtobufCliSrvCFBlocking test sending req, value=%u burst i=%u\n", req.body.value, i);

      rs = test__increment_b(&tinfo->mycli, tinfo->dt, &req, &rwreq);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
      
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

  tinfo->tickct++;
  tinfo->ctx->tickct++;

  return;

  timer=timer;
}

static void Test_increment(Test_Service *,
			   const TestReq *req, 
			   void *ud,
			   TestRsp_Closure clo, 
			   void *rwmsg) {
  TestRsp rsp;
  test_rsp__init(&rsp);

  /* Fill in a response, including a copy of the adjusted request body with the value bumped */
  rsp.errval = 0;
  rsp.has_body = TRUE;
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

/** \if NOPE */
static void pbcscf(int taskct, int doblocking, uint32_t burst, uint32_t startburst, uint32_t endat, int broker) {

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  VERBPRN("ProtobufCliSrvCFNonblocking test begin, taskct=%d doblocking=%d burst=%u startburst=%u endat=%u broker=%d\n", 
	  taskct,
	  doblocking,
	  burst,
	  startburst,
	  endat,
	  broker);

  struct mycfbctx tctx;
  struct mycfbtaskinfo *tinfo = (struct mycfbtaskinfo*)RW_MALLOC0(sizeof(*tinfo) * taskct);

  memset(&tctx, 0, sizeof(tctx));

  /*
   * Create a rwsched / tasklet runtime and the one rwmsg endpoint for
   * this tasklet.  (Probably all handled in tasklet infrastructure).
   * broker = -1 ==> no shunt
   * broker = 0 ==> no broker
   * broker > 0 ==> number of  brokers
   */

  unsigned broker_c = (broker<0?1:broker);
  rwmsg_btenv_t tenv(taskct+(broker_c?broker_c:1));		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_broker_t *bros[broker_c];
  memset(bros, 0, sizeof(bros));
  if (broker_c) {
    for (unsigned i=0; i<broker_c; i++) {
      //fprintf(stderr, "tasklet.broker[%u]=%p\n",i, tenv.tasklet[tenv.tasklet_ct-(broker_c-i)]);
      tenv.ti->rwsched_tasklet_info = tenv.tasklet[tenv.tasklet_ct-(broker_c-i)];
      rwmsg_broker_main(broport_g, tenv.tasklet_ct-(broker_c-i), i, tenv.rwsched, tenv.tasklet[tenv.tasklet_ct-(broker_c-i)], tenv.rwvcs_zk, TRUE, tenv.ti, &bros[i]);
      //tctx.bro = bro;
    }
  }

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  const char *taddrbase = "/L/GTEST/RWBRO/TESTAPP/PBSCF";
  char taddrpfx[128];
  static unsigned testct = 100;
  sprintf(taddrpfx, "%s/%u", taddrbase, testct++);

  tctx.rwsched = tenv.rwsched;
  tctx.doblocking = doblocking;
  tctx.burst = burst;
  tctx.msgct_end = endat;
  tctx.assertsinglethread = FALSE;
  tctx.tid = GETTID();

  for (unsigned t=0; t<tenv.tasklet_ct-(broker_c?broker_c:1)/*brokers are last ones*/; t++) {

    //fprintf(stderr, "tasklet[%u]=%p\n",t, tenv.tasklet[t]);

    tinfo[t].tnum = t;
    tinfo[t].ctx = &tctx;
    tinfo[t].tasklet = tenv.tasklet[t];

    sprintf(tinfo[t].tpath, "%s/%d", taddrpfx, t);
    sprintf(tinfo[t].tpeerpath, "%s/%d", taddrpfx, (t&1) ? t-1 : t+1);

    tinfo[t].ep = rwmsg_endpoint_create(1, t, (broker_c?(t%broker_c):0), tenv.rwsched, tenv.tasklet[t], rwtrace_init(), NULL);
    ASSERT_TRUE(tinfo[t].ep != NULL);

    /* Direct everything through the broker */
    rwmsg_endpoint_set_property_int(tinfo[t].ep, "/rwmsg/broker/shunt", (broker > 0)); // no shunt when broker==-1

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

      /* Send some requests right away, test pre-connection request
	 buffering in ssbufq.  They may happen still to bounce should
	 the server not have connected yet to the broker by the time
	 the req gets there. */
      if (startburst) {
	RW_ASSERT(!tctx.doblocking); // no can do

	rwmsg_closure_t cb;
	cb.pbrsp=(rwmsg_pbapi_cb)mycfb_response;
	cb.ud=tinfo;

	uint32_t u=0;
	rw_status_t rs = RW_STATUS_SUCCESS;

	for (u=0; rs == RW_STATUS_SUCCESS && u<startburst; u++) {
	  TestReq req;
	  test_req__init(&req);
	  rwmsg_request_t *rwreq=NULL;

	  req.body.value = tinfo[t].req_sent;
	  req.body.hopct = tinfo[t].req_sent;

	  VERBPRN("ProtobufCliSrvCFNonblocking test sending req, value=%u burst u=%u\n", req.body.value, u);

	  rs = test__increment(&tinfo[t].mycli, tinfo[t].dt, &req, &cb, &rwreq);
	  switch (rs) {
	  case RW_STATUS_SUCCESS:
	    VERBPRN("ProtobufCliSrvCFNonblocking test sent req, burst u=%u\n", u);
	    tinfo[t].req_sent++;
	    break;
	  case RW_STATUS_BACKPRESSURE:
	    VERBPRN("ProtobufCliSrvCFNonblocking test got backpressure, burst u=%u\n", u);
	    tinfo[t].req_eagain++;
	    break;
	  case RW_STATUS_NOTCONNECTED:
	    VERBPRN("ProtobufCliSrvCFNonblocking test got notconected, burst u=%u\n", u);
	    tinfo[t].req_eagain++;
	    break;
	  default:
	    VERBPRN("ProtobufCliSrvCFNonblocking test got rs=%d, burst u=%u\n", rs, u);
	    break;
	  }
	}

	/* Figure out the ssbufq size and check that we sent at most that many */
	int premax = 9999999;
	rs = rwmsg_endpoint_get_property_int(tinfo[t].ep, "/rwmsg/queue/cc_ssbuf/qlen", &premax);
	RW_ASSERT(rs == RW_STATUS_SUCCESS);

	/* max is nominally 1 per destination due to flow window
	   logic, although we skate on the edge and crank it a bit.
	   ought to support flow/seqno application on the other end of
	   the queue, but that's a nightmare */
	int defwin = 1;
	rwmsg_endpoint_get_property_int(tinfo[t].ep, "/rwmsg/destination/defwin", &defwin);
	//too gimpy a check, consider feedme vs nofeedme etc:	ASSERT_EQ(tinfo[t].req_sent, MIN(defwin, MIN(premax, (int)startburst)));
      }


      /* Run a timer to execute some requests */
      rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
      double timer_interval = 200/*ms*/ * .001;
      timer_interval += (t * 20/*ms*/ * .001);    // extra smidge for each new tasklet to avoid stampede

      double valgfactor = 1.0;
      if (RUNNING_ON_VALGRIND) {
	valgfactor = 2.5;
      }

      cf_context.info = &tinfo[t];
      tinfo[t].cftim = rwsched_tasklet_CFRunLoopTimerCreate(tenv.tasklet[t],
						    kCFAllocatorDefault,
						    CFAbsoluteTimeGetCurrent() + (valgfactor * 10.0 * timer_interval), // allow a while for connections to come up
						    timer_interval,
						    0,
						    0,
						    mycfb_timer_cb,
						    &cf_context);
      RW_CF_TYPE_VALIDATE(tinfo[t].cftim, rwsched_CFRunLoopTimerRef);
      rwsched_tasklet_CFRunLoopAddTimer(tenv.tasklet[t], runloop, tinfo[t].cftim, RWMSG_RUNLOOP_MODE);//kCFRunLoopCommonModes);
      tctx.timerct++;

    }
  }

  //tctx.loop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.rwsched);
  struct timeval start, stop, delta;
  int looptot = 60;
  if (RUNNING_ON_VALGRIND) {
    looptot = looptot * 5 / 2;
  }
  int loopwait = looptot;
  VERBPRN("loop iter begin\n");
  gettimeofday(&start, NULL);
  do {
    rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, loopwait, FALSE); 
    gettimeofday(&stop, NULL);
    timersub(&stop, &start, &delta);
    VERBPRN("loop iter %ld.%03ld tctx.done=%d\n", delta.tv_sec, delta.tv_usec / 1000, tctx.done);  
    loopwait = looptot - delta.tv_sec;
  } while (loopwait > 0 && !tctx.done);

  ASSERT_TRUE(tctx.done);

  //sleep(2);

  VERBPRN("loop runtime %lu ms\n", delta.tv_sec * 1000 + delta.tv_usec / 1000);

  //RW_RESOURCE_TRACK_DUMP(g_rwresource_track_handle);

  for (unsigned t=0; t<tenv.tasklet_ct-(broker_c?broker_c:1)/*brokers are last ones*/; t++) {
    if (t&1) {
      rwmsg_srvchan_halt(tinfo[t].sc);
    } else {
      rwmsg_destination_release(tinfo[t].dt);
      rwmsg_clichan_halt(tinfo[t].cc);
      if (tinfo[t].req_eagain) {
	fprintf(stderr, "Tasklet %d req_sent=%u req_eagain=%u\n", t, tinfo[t].req_sent, tinfo[t].req_eagain);
      }
      if (tinfo[t].bnc_recv) {
	fprintf(stderr, "Tasklet %d req_sent=%u bnc_recv=%u\n", t, tinfo[t].req_sent, tinfo[t].bnc_recv);
      }
      ASSERT_LE(tinfo[t].bnc_recv, MAX(burst,startburst));
    }
    rwmsg_bool_t r;
    VERBPRN("rwmsg_endpoint_halt_flush(ep=%p tasklet %d)\n", tinfo[t].ep, t);
    r = rwmsg_endpoint_halt_flush(tinfo[t].ep, TRUE);
    ASSERT_TRUE(r);
  }

  if (broker_c) {
    rwmsg_bool_t r;
    for (unsigned i=0; i<broker_c; i++) {
      if (bros[i]) {
	VERBPRN("broker_halt_sync()\n");
	r = rwmsg_broker_halt_sync(bros[i]);
	ASSERT_TRUE(r);
      }
    }
  }

  ASSERT_EQ(rwmsg_global.status.endpoint_ct, 0);
  nnchk();

  if (tinfo) {
    RW_FREE(tinfo);
  }

  VERBPRN("ProtobufCliSrvCFNonblocking test end\n");
}
/** \endif */

TEST(RWMsgBroker, PbCliSrvCFNonblk2Bro) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 2;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFNonblk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, 2 broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 2;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFBrstNonblk2Bro) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFBrstNonblk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, 2 broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst1Nonblk2Bro) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 1, broker");

  // send one request straightaway to exercise the ssbufq buffer used to hold initial requests sent before broker connection comes up

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 1;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst1Nonblk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 1, 2 broker");

  // send one request straightaway to exercise the ssbufq buffer used to hold initial requests sent before broker connection comes up

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 1;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst16Nonblk2Bro) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 16, broker");

  // this is the startup xmit buffer size

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 16;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst16Nonblk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 16, 2 broker");

  // this is the startup xmit buffer size

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 16;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst24Nonblk2Bro_LONG) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 24, broker");

  // this is supposed to exceed the startup buffer and demonstrate 1+ initial requests experiencing backpressure 

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 24;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFBrstStartburst24Nonblk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, bursty, startburst 24, 2 broker");

  // this is supposed to exceed the startup buffer and demonstrate 1+ initial requests experiencing backpressure 

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 20;
  uint32_t startburst = 24;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFBlk2Bro) {
  TEST_DESCRIPTION("Tests protobuf service client and server, blocking under taskletized CFRunloop, 2 tasklets, broker");

  int taskct = 2;
  int doblocking = 1;
  uint32_t burst = 2;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 1);
}

TEST(RWMsgBroker, PbCliSrvCFBlk2Bro2Bros) {
  TEST_DESCRIPTION("Tests protobuf service client and server, blocking under taskletized CFRunloop, 2 tasklets, 2 broker");

  int taskct = 2;
  int doblocking = 1;
  uint32_t burst = 1;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 2);
}

TEST(RWMsgBroker, PbCliSrvCFNonblk2BroIdle) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, no shunt to broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t burst = 2;
  uint32_t startburst = 0;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, -1);
}

TEST(RWMsgBrokerNot, PbCliSrvCFNonblk2) {
  TEST_DESCRIPTION("Tests protobuf service client and server, nonblocking under taskletized CFRunloop, 2 tasklets, no broker");

  int taskct = 2;
  int doblocking = 0;
  uint32_t startburst = 0;
  uint32_t burst = 2;
  uint32_t endct = 2 * burst;

  pbcscf(taskct, doblocking, burst, startburst, endct, 0);
}







struct lottaopts {
  int bigbrosrvwin;
  int bigdefwin;
  int reqtimeo;
  int bncall;
  int srvrspfromanyq;
  uint32_t paysz;
};
struct lottarawvals {
  uint64_t sec;
  uint64_t flushtime;
  uint64_t rmax;
  uint64_t reqsent;
  uint64_t firstburst;
  uint64_t reqans;
  uint64_t reqdone;
  uint64_t reqbnc;
  uint64_t reqbnc_final;
  uint64_t reqhz;
  uint64_t min_rtt;
  uint64_t max_rtt;
};
struct sctest1_payload {
  uint32_t rno;
  char str[200];
};
struct sctest1_context {
  int verbose;
  int window;
  int tokenbucket;
  uint32_t tokenfill_ct;
  int flowexercise;
  struct lottaopts *opts;
  struct sctest1_payload *paybuf;
  rwmsg_flowid_t flowid;
  int unpout;
  int usebro;
  int ccwaiting;
  uint32_t rcvburst;
  int sendmore;
  int sendone;
  uint32_t sendmsg_firstburst_done;
  uint32_t sendmsg_preflow;
  uint32_t kickct;
  uint32_t done;
  rwmsg_srvchan_t *sc;
  rwmsg_clichan_t *cc;
  rwmsg_destination_t *dt;
  uint64_t sc_tid;
  uint64_t cc_tid;
  int checktid;
  rwmsg_signature_t *sig;
#define SCTEST1_RMAX (10000)
  uint32_t rmax;

  uint32_t misorderct;

  /* Keep a queue of the request transmit order, to compare with response arrival order */
  uint32_t expectedreqno[SCTEST1_RMAX+50];
  uint32_t expectedreqno_head;
  uint32_t expectedreqno_tail;
#define SCTEST1_RNO_ENQ(ctx, rno) {				\
  ((ctx)->expectedreqno[(ctx)->expectedreqno_tail] = (rno));	\
  if ((++(ctx)->expectedreqno_tail) >= ((ctx)->rmax + 50)) {	\
    (ctx)->expectedreqno_tail = 0;				\
  }								\
}
#define SCTEST1_RNO_DEQ(ctx)   ({					\
  ASSERT_NE((ctx)->expectedreqno_head , (ctx)->expectedreqno_tail);	\
  int rno=((ctx)->expectedreqno[(ctx)->expectedreqno_head]);		\
  if ((++(ctx)->expectedreqno_head) >= ((ctx)->rmax + 50)) {		\
    (ctx)->expectedreqno_head = 0;					\
  }									\
  rno;									\
})

  uint32_t paysz;
  rwmsg_request_t *r[SCTEST1_RMAX];
  uint32_t in;
  uint32_t out;
  uint32_t rspin;
  uint32_t rspout;
  uint32_t bncout;
  uint32_t miss;
  uint64_t min_rtt;
  uint64_t max_rtt;
  rwsched_dispatch_queue_t rwq[2];
  rwsched_dispatch_source_t sc_timer;
  rwmsg_btenv_t *tenv;
};
#define GETTID() syscall(SYS_gettid)

static void lottakickoff(void *ctx_in);

static void sctest1_clicb_feedme(void *ctx_in, rwmsg_destination_t *dest, rwmsg_flowid_t flowid) {
  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  if (ctx->verbose) {
    fprintf(stderr, "sctest1_clicb_feedme: dest='%s',flowid=%u\n", dest->apath, flowid);
  }
  lottakickoff(ctx_in);
}

static void sctest1_clicb1(rwmsg_request_t *req, void *ctx_in) {
  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  int expected = 0;

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


  if (1 || ctx->flowexercise) {
    /* Register for the flow control callback the first time, or rather when the flowid changes */
    rwmsg_flowid_t fid = rwmsg_request_get_response_flowid(req);
    if (fid && fid != ctx->flowid) {
      ctx->flowid = fid;
      rwmsg_closure_t cb = { };
      cb.ud = ctx;
      cb.feedme = sctest1_clicb_feedme;
      rw_status_t rs = rwmsg_clichan_feedme_callback(ctx->cc, ctx->dt, ctx->flowid, &cb);
      EXPECT_EQ(rs, RW_STATUS_SUCCESS);

      //ASSERT_GT(rwmsg_clichan_can_send_howmuch(ctx->cc, ctx->dt, ctx->sig->methno, ctx->sig->payt), 0);
    }
  }

  uint32_t len=0;
  const struct sctest1_payload *rsp = (const struct sctest1_payload*)rwmsg_request_get_response_payload(req, &len);
  uint32_t rno;

  uint64_t rtt = rwmsg_request_get_response_rtt(req);
  if (rtt > 0) {
    if (rtt < ctx->min_rtt) {
      ctx->min_rtt = rtt;
    }
    if (rtt > ctx->max_rtt) {
      ctx->max_rtt = rtt;
    }
  }

  if (rsp) {
    ck_pr_inc_32(&ctx->rspout);
    if (ctx->verbose) {
      fprintf(stderr, "sctest1_clicb1: got rsp len=%d rno=%u str='%s'\n", len, rsp->rno, rsp->str);
    }
    int ticks = (RUNNING_ON_VALGRIND ? 100 : 10000);
    if (ctx->rspout < 1000) {
      ticks = 100;
    }
    if (ctx->rspout % ticks == 0) {
      fprintf(stderr, "_");
    }
    RW_ASSERT(rsp->rno < ctx->rmax);
    rno = rsp->rno;
  } else {
    ck_pr_inc_32(&ctx->bncout);
    rwmsg_bounce_t bcode = rwmsg_request_get_response_bounce(req);
    int bcodeint = (int)bcode;
    const char *bncdesc[] = {
      "OK",
      "NODEST",
      "NOMETH",
      "NOPEER",
      "BROKERR",
      "TIMEOUT",
      "RESET",
      "TERM",
      NULL
    };
    RW_ASSERT(bcodeint < RWMSG_BOUNCE_CT);
    if (ctx->bncout < 10) {
      fprintf(stderr, "b(%s)", bncdesc[bcodeint]);
    }
    if (ctx->bncout % 1000 == 0) {
      fprintf(stderr, "B(%s)", bncdesc[bcodeint]);
    }
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
    //    fprintf(stderr, "sctest1_clicb1: req rno=%d had no rsp payload!?\n", rno);
  }

  /* Verify responses are in order, up until -- or shortly before! -- we get a bounce, at which point all bets are off */
  expected = SCTEST1_RNO_DEQ(ctx);
  if (!ctx->bncout) {
    if ((int)rno != expected) {
      ctx->misorderct++;
    }
  }

  /* req ceases to be valid when we return, so find and clear our handle to it */
  RW_ASSERT(rno < ctx->rmax);
  RW_ASSERT(ctx->r[rno]);
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

    memset(ctx->paybuf, 0, sizeof(struct sctest1_payload));
    ctx->paybuf->rno = rno;
    sprintf(ctx->paybuf->str, "hi there rno=%d", rno);
    rwmsg_request_set_payload(ctx->r[rno], ctx->paybuf, MAX(ctx->paysz, sizeof(struct sctest1_payload)));

    /* Send it.  The srvchan should more or less immediately run the
       thing and the methcb will respond. */
    ck_pr_inc_32(&ctx->in);
    rw_status_t rs = rwmsg_clichan_send(ctx->cc, ctx->dt, ctx->r[rno]);
    if (rs == RW_STATUS_BACKPRESSURE) { // && ctx->flowexercise)
      if (ctx->verbose) {
	fprintf(stderr, "sctest1_clicb1 _send rno=%u backpressure\n", rno);
      }
      ck_pr_dec_32(&ctx->in);
      rwmsg_request_release(ctx->r[rno]);
      ctx->r[rno] = NULL;
      ctx->ccwaiting = TRUE;
      /* backpressure will trigger feedme callback */
    } else if (rs != RW_STATUS_SUCCESS) {
      ck_pr_dec_32(&ctx->in);
      rwmsg_request_release(ctx->r[rno]);
      ctx->r[rno] = NULL;
      ctx->sendmore = FALSE;
      fprintf(stderr, "sctest1_clicb1: rwmsg_clichan_send() rs=%u\n", rs);
    } else {
      /* We should not have two out with the same ID */
      if (ctx->verbose) {
	fprintf(stderr, "sctest1_clicb1 _send rno=%u sent, ccwaiting now 0\n", rno);
      }
      SCTEST1_RNO_ENQ(ctx, rno);
      ctx->ccwaiting = FALSE;
      uint32_t rr;
      for (rr = 0; rr < ctx->rmax; rr++) {
	if (ctx->r[rr] && rr != rno) {
	  if (ctx->usebro) {
	    RW_ASSERT(ctx->r[rr]->hdr.id.locid != ctx->r[rno]->hdr.id.locid);
	  }
	}
      }
    }

    if (1 || ctx->flowexercise) {
      lottakickoff(ctx);
    }
  } else {
  checkend:
    if ((ctx->bncout + ctx->rspout) == ctx->in) {
      ctx->done = TRUE;
    }
  }
}

static void lottakickoff(void *ctx_in) {
  /* Send the actual request(s).  The rsp callback will repeat them as they are answered (ctx->repeat) */
  RW_ASSERT_TYPE(ctx_in, sctest1_context);

  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  int rno=0;
  RW_ASSERT(ctx->window <= SCTEST1_RMAX);
  ctx->rmax = ctx->window;
  int sent = 0, slots = 0, backp = 0, snderr = 0;
  for (rno=0; rno < (int)ctx->rmax && (ctx->sendmore || (!ctx->sendmore && ctx->sendone)) && rwmsg_clichan_can_send_howmuch(ctx->cc, ctx->dt, ctx->sig->methno, ctx->sig->payt); rno++) {
    RW_ASSERT(rno <= SCTEST1_RMAX);
    if (!ctx->r[rno]) {
      slots++;
      ctx->r[rno] = rwmsg_request_create(ctx->cc);
      ASSERT_TRUE(ctx->r[rno] != NULL);
      rwmsg_request_set_signature(ctx->r[rno], ctx->sig);
      rwmsg_closure_t clicb;
      clicb.request=sctest1_clicb1;
      clicb.ud=ctx;
      rwmsg_request_set_callback(ctx->r[rno], &clicb);

      memset(ctx->paybuf, 0, sizeof(struct sctest1_payload));
      ctx->paybuf->rno = rno;
      sprintf(ctx->paybuf->str, "hi there rno=%d", rno);
      rwmsg_request_set_payload(ctx->r[rno], ctx->paybuf, MAX(ctx->paysz, sizeof(struct sctest1_payload)));

      /* Send it. */
      ck_pr_inc_32(&ctx->in);
      rw_status_t rs = rwmsg_clichan_send(ctx->cc, ctx->dt, ctx->r[rno]);
      if (rs == RW_STATUS_SUCCESS) {
	if (!ctx->sendmsg_firstburst_done) {
	  //RW_ASSERT(!ctx->flowid);
	  ctx->sendmsg_preflow++;
	}
	SCTEST1_RNO_ENQ(ctx, rno);
	sent++;
      } else {
	ctx->sendmsg_firstburst_done = TRUE;
	if (rs == RW_STATUS_BACKPRESSURE) {
	  backp++;
	  if (ctx->verbose) {
	    fprintf(stderr, "lottakickoff _send rno=%u backpressure\n", rno);
	  }
          ck_pr_dec_32(&ctx->in);
	  rwmsg_request_release(ctx->r[rno]);
	  ctx->r[rno] = NULL;
	  if (!ctx->ccwaiting) {
	    ctx->ccwaiting = TRUE;
	    if (ctx->verbose) {
	      fprintf(stderr, "lottakickoff backpressure ccwaiting=%d\n", ctx->ccwaiting);
	    }
	  }
	  goto out;
	} else if (rs != RW_STATUS_SUCCESS) {
	  snderr++;
	  ck_pr_dec_32(&ctx->in);
	  rwmsg_request_release(ctx->r[rno]);
	  ctx->r[rno] = NULL;
	  ctx->sendmore = FALSE;
	  fprintf(stderr, "sctest1_clicb1: rwmsg_clichan_send() rs=%u\n", rs);
	} else {
	  if (ctx->verbose) {
	    fprintf(stderr, "sctest1_clicb1 _send rno=%u sent, ccwaiting now 0\n", rno);
	  }
	  ctx->ccwaiting = FALSE;

	  if (0 && ctx->in >= 5) {
	    fprintf(stderr, "lottakickoff sendmore=FALSE hack\n");
	    ctx->sendmore = FALSE;
	  }

	}

      }
    }
  }
 out:
  if (ctx->verbose) {
    fprintf(stderr, "lottakickoff slots=%d sent=%d backp=%d snderr=%d sendmore=%d\n", slots, sent, backp, snderr, ctx->sendmore);
  }

  ctx->kickct++;
}


static void sctest1_clicb02(rwmsg_request_t *req, void *ctx_in);

static void sctest1_reqsend(void *ctx_in) {
  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  rwmsg_request_t *req;

#if 0
  static unsigned long long tids[100] = {0};
  static int l_tid=0;
  int i;
  unsigned long long tid = GETTID();
  for (i=0; i<l_tid;i++) {
    if (tids[i] == tid) break;
  }
  if (i>=l_tid) {
    // fprintf(stderr, "sctest1_clicb02: Thread 0x%llx cc=%p\n", tid, ctx->cc);
    tids[l_tid] = tid;
    l_tid++;
  }
#endif

  req = rwmsg_request_create(ctx->cc);
  ASSERT_TRUE(req != NULL);
  rwmsg_request_set_signature(req, ctx->sig);
  rwmsg_closure_t clicb;
  clicb.request=sctest1_clicb02;
  clicb.ud=ctx;
  rwmsg_request_set_callback(req, &clicb);

  memset(ctx->paybuf, 0, sizeof(struct sctest1_payload));
  ctx->paybuf->rno = 0;
  sprintf(ctx->paybuf->str, "hi there rno=%d", 0);
  rwmsg_request_set_payload(req, ctx->paybuf, MAX(ctx->paysz, sizeof(struct sctest1_payload)));

  /* Send it.  The srvchan should more or less immediately run the
     thing and the methcb will respond. */
  ck_pr_inc_32(&ctx->in);
  rw_status_t rs = rwmsg_clichan_send(ctx->cc, ctx->dt, req);
  if (rs == RW_STATUS_BACKPRESSURE) { // && ctx->flowexercise)
    if (ctx->verbose) {
      fprintf(stderr, "sctest1_clicb02 _send rno=%u backpressure\n", 0);
    }
    ck_pr_dec_32(&ctx->in);
    rwmsg_request_release(req);
    req = NULL;
    ctx->ccwaiting = TRUE;
    /* backpressure will trigger feedme callback */
  } else if (rs != RW_STATUS_SUCCESS) {
    ck_pr_dec_32(&ctx->in);
    rwmsg_request_release(req);
    req = NULL;
    ctx->sendmore = FALSE;
    fprintf(stderr, "sctest1_clicb02: rwmsg_clichan_send() rs=%u\n", rs);
  }
}

static void sctest1_clicb02(rwmsg_request_t *req, void *ctx_in) {
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
  uint32_t len=0;
  const struct sctest1_payload *rsp = (const struct sctest1_payload*)rwmsg_request_get_response_payload(req, &len);

  uint64_t rtt = rwmsg_request_get_response_rtt(req);
  if (rtt > 0) {
    if (rtt < ctx->min_rtt) {
      ctx->min_rtt = rtt;
    }
    if (rtt > ctx->max_rtt) {
      ctx->max_rtt = rtt;
    }
  }

  if (rsp) {
    ck_pr_inc_32(&ctx->rspout);
    if (ctx->verbose) {
      fprintf(stderr, "sctest1_clicb02: got rsp len=%d rno=%u str='%s'\n", len, rsp->rno, rsp->str);
    }
    int ticks = (RUNNING_ON_VALGRIND ? 100 : 10000);
    if (ctx->rspout < 1000) {
      ticks = 100;
    }
    if (ctx->rspout % ticks == 0) {
      fprintf(stderr, "_");
    }
    RW_ASSERT(rsp->rno < ctx->rmax);
    //rno = rsp->rno;
  } else {
    ck_pr_inc_32(&ctx->bncout);
    if (!ctx->sendmore) {
      goto checkend;
    }
    return;
  }

  /* If the test has the repeat flag set, reissue another request */
  if (ctx->sendmore) {
    RW_ASSERT(ctx->cc);
    sctest1_reqsend(ctx_in);
  } else {
  checkend:
    if ((ctx->bncout + ctx->rspout) == ctx->in) {
      ctx->done = TRUE;
    }
  }
}

static void lottakickoff02(void *ctx_in) {
  /* Send the actual request(s).  The rsp callback will repeat them as they are answered (ctx->repeat) */
  RW_ASSERT_TYPE(ctx_in, sctest1_context);

  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  int rno=0;
  RW_ASSERT(ctx->window <= SCTEST1_RMAX);
  ctx->rmax = ctx->window;
  int sent = 0, slots = 0, backp = 0, snderr = 0;
  for (rno=0; rno < (int)ctx->rmax && (ctx->sendmore || (!ctx->sendmore && ctx->sendone)); rno++) {
    RW_ASSERT(rno <= SCTEST1_RMAX);
    sctest1_reqsend(ctx_in);
  }
  if (ctx->verbose) {
    fprintf(stderr, "lottakickoff02 slots=%d sent=%d backp=%d snderr=%d sendmore=%d\n", slots, sent, backp, snderr, ctx->sendmore);
  }

  ctx->kickct++;
}

static void sctest1_refill_bucket(struct sctest1_context *ctx) {

#define TOKVAL (VERBOSE() ? 4 : (RUNNING_ON_VALGRIND ? 25 : 100) )

  ctx->tokenbucket = TOKVAL;
  ck_pr_inc_32(&ctx->tokenfill_ct);
}
static void sctest1_timer(void *ctx_in) {
  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;
  sctest1_refill_bucket(ctx);
  rwmsg_srvchan_resume(ctx->sc);
}

struct sctest1_worker_handle {
  rwmsg_request_t *req;
  struct sctest1_context *ctx;
};

static void sctest1_methcb1_worker_f(void *ud) {
  struct sctest1_worker_handle *han = (struct sctest1_worker_handle *)ud;
  rwmsg_request_t *req = han->req;
  struct sctest1_context *ctx = han->ctx;

  if (!ctx->opts->bncall) {
    uint32_t paylen=0;
    const struct sctest1_payload *pay = (const struct sctest1_payload*)rwmsg_request_get_request_payload(req, &paylen);
    if (pay) {
      if (ctx->verbose) {
	fprintf(stderr, "sctest1_methcb1 got msg in=%u paylen=%u rno=%u str='%s' ctx->in=%u ctx->out=%u\n", 
		ctx->in, paylen, pay->rno, pay->str, ctx->in, ctx->out);
      }
      
      static uint32_t outct = 0;
      struct sctest1_payload payout;
      memset(&payout, 0, sizeof(payout));
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
    int ticks = (RUNNING_ON_VALGRIND ? 100 : 10000);
    if (ctx->rspin < 1000) {
      ticks = 100;
    }
    if (ctx->rspin % ticks == 0) {
      fprintf(stderr, "+");
    }
    
    if (ctx->flowexercise) {
      ASSERT_TRUE(ctx->tokenbucket > 0);
      if (--ctx->tokenbucket <= 0) {
	rwmsg_srvchan_pause(ctx->sc);
	if (ctx->verbose) {
	  fprintf(stderr, "sctest1_methcb1 drained bucket, invoking srvchan_pause\n");
	}
      }
    }
  }

  free(han);
}

static void sctest1_methcb1(rwmsg_request_t *req, void *ctx_in) {
  static int once=0;

  RW_ASSERT(!once);
  once=1;

  RW_ASSERT_TYPE(ctx_in, sctest1_context);
  struct sctest1_context *ctx = (struct sctest1_context*)ctx_in;

  if (ctx->flowexercise) {
    if (ctx->tokenbucket == 0) {
      ASSERT_TRUE(ctx->sc->ch.paused_user);
    }
    ASSERT_TRUE(ctx->tokenbucket > 0);
  }

  /* srvchan method callback */
  if (!ctx->sc_tid) {
    ctx->sc_tid = GETTID();
  } else if (ctx->checktid) {
    uint64_t curtid = GETTID();
    EXPECT_EQ(ctx->sc_tid, curtid);
    ctx->sc_tid = curtid;
  }

  ck_pr_inc_32(&ctx->out);

  struct sctest1_worker_handle *han = (struct sctest1_worker_handle*)malloc(sizeof(*han));
  han->req = req;
  han->ctx = ctx;
  if (ctx->opts->srvrspfromanyq) {
    rwsched_dispatch_queue_t q = rwsched_dispatch_get_global_queue(ctx->tenv->tasklet[0], DISPATCH_QUEUE_PRIORITY_DEFAULT);
    RW_ASSERT(q);
    RW_ASSERT_TYPE(q, rwsched_dispatch_queue_t);
    rwsched_dispatch_async_f(ctx->tenv->tasklet[0],
			     q,
			     han,
			     sctest1_methcb1_worker_f);
  } else {
    sctest1_methcb1_worker_f(han);
  }


  ck_pr_barrier();
  RW_ASSERT(once);
  once=0;
}


static void lottaraw(int squat, int mainq, int fixedrateserver, int window, int usebro, int broflo, struct lottaopts *opts, struct lottarawvals *valsout) {
  uint32_t start_reqct = rwmsg_global.status.request_ct;
  VERBPRN("begin rwmsg_global.status.request_ct=%u\n", rwmsg_global.status.request_ct);

  rwmsg_btenv_t tenv(2);		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  if (valsout) {
    memset(valsout, 0, sizeof(*valsout));
  }

  if (RUNNING_ON_VALGRIND) {
    window /= 10;
    window = MAX(1, window);
  }

  rwmsg_broker_t *bro=NULL;
  tenv.ti->rwsched_tasklet_info = tenv.tasklet[1];
  rwmsg_broker_main(broport_g, 1, 0, tenv.rwsched, tenv.tasklet[1], tenv.rwvcs_zk, mainq, tenv.ti, &bro);

  if (broflo) {
    bro->thresh_reqct2 = 10;
    bro->thresh_reqct1 = 5;
  }
  if (opts->bigbrosrvwin) {
    /* set the broker's srvchan window value really high, to cause overrun of writes to srvchan socket */
    rwmsg_endpoint_set_property_int(bro->ep, "/rwmsg/broker/srvchan/window", (RUNNING_ON_VALGRIND ? 1000 : 1000000));
  }

  rwmsg_endpoint_t *ep;
  ep = rwmsg_endpoint_create(1, 1, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep != NULL);

  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep, "/rwmsg/broker/shunt", usebro);

  if (opts->bigdefwin) {
    rwmsg_endpoint_set_property_int(ep, "/rwmsg/destination/defwin", opts->bigdefwin);
  }

  rw_status_t rs;

  struct sctest1_context *ctx = RW_MALLOC0_TYPE(sizeof(*ctx), sctest1_context);
  ctx->usebro = usebro;
  ctx->window = window;
  ctx->tenv = &tenv;
  ctx->paysz = sizeof(sctest1_payload);
  if (opts) {
    ctx->paysz = opts->paysz;
  }
  ctx->paybuf = RW_MALLOC0_TYPE(MAX(sizeof(struct sctest1_payload), ctx->paysz), sctest1_payload);
  ctx->verbose = VERBOSE();
  RW_ASSERT(opts);
  ctx->opts = opts;

  ctx->min_rtt = MAX_RTT;

  ctx->checktid=0;		// check thread id is same every time; adds a syscall per msg on each side
  if (ctx->checktid) {
    fprintf(stderr, "warning: checktid enabled\n");
  }
  
  const char *taddrpfx = "/L/GTEST/RWBRO/TESTAPP/6";
  char taddr[999];
  sprintf(taddr, "%s/%u", taddrpfx, rand());
  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method */
  ctx->sc = rwmsg_srvchan_create(ep);
  ASSERT_TRUE(ctx->sc != NULL);
  rwmsg_signature_t *sig = rwmsg_signature_create(ep, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  int timeo = 1000;		// ms
  if (opts->reqtimeo) {
    timeo = opts->reqtimeo;
  }
  if (RUNNING_ON_VALGRIND) {
    timeo = timeo * 5;
  }
  rwmsg_signature_set_timeout(sig, timeo);
  ctx->sig = sig;
  rwmsg_closure_t methcb;
  methcb.request=sctest1_methcb1;
  methcb.ud=ctx;
  rwmsg_method_t *meth = rwmsg_method_create(ep, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(ctx->sc, meth);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  /* Wait on binding the srvchan until after we've sent the initial
     flurry of requests.  Otherwise, our ++ and the request handler's
     ++ of some of the request count variables ABA-stomp each
     other. */

  /* Create the clichan, add the method's signature */
  rwmsg_destination_t *dt = rwmsg_destination_create(ep, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  ctx->dt = dt;

  if (squat!=2 ||
      ctx->flowexercise) {
    rwmsg_closure_t cb = { };
    cb.ud = ctx;
    cb.feedme = sctest1_clicb_feedme;
    rwmsg_destination_feedme_callback(dt, &cb);
  }
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep);
  ctx->cc = cc;
  ASSERT_TRUE(cc != NULL);
  //TBD/optional  rwmsg_clichan_add_signature(cc, sig);

  /* Bind to a serial queue */
  if (mainq) {
    ctx->rwq[0] = rwsched_dispatch_get_main_queue(tenv.rwsched);
    ctx->rwq[1] = rwsched_dispatch_get_main_queue(tenv.rwsched);
  } else {
    ctx->rwq[0] = rwsched_dispatch_queue_create(tenv.tasklet[0], "ctx->rwq[0] (cc)", DISPATCH_QUEUE_SERIAL);
    ctx->rwq[1] = rwsched_dispatch_queue_create(tenv.tasklet[1], "ctx->rwq[1] (sc)", DISPATCH_QUEUE_SERIAL);
  }

  rs = rwmsg_clichan_bind_rwsched_queue(cc, ctx->rwq[0]);
  if (window == 1) {
    ctx->sendmore = FALSE;
    ctx->sendone = TRUE;
  } else {
    ctx->sendmore = TRUE;
  }

  /* Now bind the srvchan; at this point the srvchan will start
     chewing in another thread. */
  rs = rwmsg_srvchan_bind_rwsched_queue(ctx->sc, ctx->rwq[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  /* Setup  the srvchan to run at a fixed rate of 4 or 100 req/s, to exercise backpressure etc. */
  ctx->flowexercise = fixedrateserver;

  /* Fake pump the main queue for 1s to get connected, registered, etc */
  int setupsec = 1;
  if (RUNNING_ON_VALGRIND) {
    setupsec = setupsec * 5 / 2;
  }
  rwsched_dispatch_main_until(tenv.tasklet[0], setupsec, NULL);

  if (!squat) {

    if (ctx->flowexercise) {
      sctest1_refill_bucket(ctx);
      ctx->sc_timer = rwsched_dispatch_source_create(tenv.tasklet[1],
						     RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
						     0,
						     0,
						     ctx->rwq[1]);
      rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[1],
						  ctx->sc_timer,
						  sctest1_timer);
      rwsched_dispatch_set_context(tenv.tasklet[1],
				   ctx->sc_timer,
				   ctx);
      uint64_t ms1000 = 1*NSEC_PER_SEC/1;
      rwsched_dispatch_source_set_timer(tenv.tasklet[1],
					ctx->sc_timer,
					dispatch_time(DISPATCH_TIME_NOW, ms1000),
					ms1000,
					0);
      rwsched_dispatch_resume(tenv.tasklet[1], ctx->sc_timer);
    }

    if (RUNNING_ON_VALGRIND) {
      /* Pump the main queue briefly to get everyone up and running before sending */
      rwsched_dispatch_main_until(tenv.tasklet[0], 1, NULL);
    }

    //lottakickoff(ctx);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff);

    int sec_start = time(NULL);
    double  sec = 5;
    if (RUNNING_ON_VALGRIND) {
      sec = sec * 5 / 2 ;
    }
    
    /* Run the main queue for 5s or longer under valgrind */
#if 1
    for(int i=0; !ctx->done && i<100; i++) {
        rwsched_dispatch_main_until(tenv.tasklet[0], sec/100, NULL);
    }
#else
    rwsched_dispatch_main_until(tenv.tasklet[0], sec, NULL);
#endif

    VERBPRN("\nln=%d ctx->done=%d ctx->rspout=%u\n", __LINE__,  ctx->done, ctx->rspout);
    
    ctx->sendmore = FALSE;
    ck_pr_barrier();
    
    uint32_t endct_in = ctx->in;
    uint32_t endct_rspin = ctx->rspin;
    uint32_t endct_rspout = ctx->rspout;

    uint32_t endct_bncout = ctx->bncout;

    
    int iter=0;
    VERBPRN("\n\nMain test run done/timeout; pumping to flush events and/or finish...\n\nln=%d iter=%d ctx->done=%d ctx->rspout=%u bncout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);
    do {
      /* Fake pump the main queue until done */
      rwsched_dispatch_main_until(tenv.tasklet[0], 10, &ctx->done);
      iter++;
      VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->in=%u ctx->rspout=%u ctx->bncout=%u\n", __LINE__, iter, ctx->done, ctx->in, ctx->rspout, ctx->bncout);
      if (iter > 6) {
	fprintf(stderr, "waiting for done, iter=%d rspout=%u in=%u bnc=%u\n", iter, ctx->rspout, ctx->in, ctx->bncout);
      }
      if (!RUNNING_ON_VALGRIND) {
	ASSERT_LE(iter, 6);
      }
      ck_pr_barrier();
    } while (!ctx->done);

    ck_pr_barrier();
    
    VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->rspout=%u ctx->bcnout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);

    if (!ctx->bncout) {
      ASSERT_EQ(ctx->rspout, ctx->in);
      ASSERT_EQ(ctx->out, ctx->in);
      ASSERT_EQ(ctx->rspout, ctx->rspin);
      ASSERT_EQ(ctx->out, ctx->rspout);
      if (!ctx->opts->srvrspfromanyq) {
	ASSERT_FALSE(ctx->misorderct);
      }
    } else {
      /* Note that we can get bounces plus some might have been
	 answered even as they bounced on our end. */
      ASSERT_EQ(ctx->rspout + ctx->bncout, ctx->in); // rsp+bnc at the client == requests sent by client
      ASSERT_GE(ctx->out + ctx->bncout, ctx->in);
      ASSERT_LE(ctx->rspout, ctx->rspin);
      ASSERT_LE(ctx->out, ctx->bncout + ctx->rspout);
    }
    
    int rno;
    for (rno=0; rno < SCTEST1_RMAX; rno++) {
      ASSERT_TRUE(ctx->r[rno] == NULL);
    }

    valsout->sec = time(NULL) - sec_start; //sec;
    valsout->flushtime = iter*100;
    valsout->rmax = ctx->rmax;
    valsout->reqsent = endct_in;
    valsout->reqans = endct_rspin;
    valsout->reqdone = endct_rspout;
    valsout->reqbnc = endct_bncout;
    valsout->reqbnc_final = ctx->bncout;
    valsout->reqhz = (endct_rspout / sec / 10);
    valsout->reqhz *= 10;
    valsout->min_rtt = ctx->min_rtt;
    valsout->max_rtt = ctx->max_rtt;
    valsout->firstburst = ctx->sendmsg_preflow;

    /* Ought to work under val, but there is a cock-up with timers that makes the token refresh et al go wrong */
    if (ctx->flowexercise && !ctx->bncout && !RUNNING_ON_VALGRIND) {
      ASSERT_GE(valsout->reqhz, TOKVAL);
    }

    if (ctx->flowexercise && ctx->sc_timer) {
      /* Fake pump the main queue for 1s, allow lingering async_f and
	 timer calls to flush out */
      rwsched_dispatch_source_cancel(tenv.tasklet[0], ctx->sc_timer);
      RW_ASSERT(bro);
      rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, ctx->sc_timer, tenv.rwsched, tenv.tasklet[0]);
      ctx->sc_timer = NULL;
      //      rwsched_dispatch_main_until(tenv.tasklet[0], 5, NULL);
    }
  } else if (squat == 2) { /* For *ConcurQ* tests */
    rwsched_dispatch_queue_t cuncurQ = rwsched_dispatch_get_global_queue(tenv.tasklet[0], DISPATCH_QUEUE_PRIORITY_DEFAULT);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);

    int sec_start = time(NULL);
    double  sec = 5;
    if (RUNNING_ON_VALGRIND) {
      sec = sec * 5 / 2 ;
    }
    
    /* Run the main queue for 5s or longer under valgrind */
#if 1
    for(int i=0; !ctx->done && i<100; i++) {
        rwsched_dispatch_main_until(tenv.tasklet[0], sec/100, NULL);
    }
#else
    rwsched_dispatch_main_until(tenv.tasklet[0], sec, NULL);
#endif

    VERBPRN("\nln=%d ctx->done=%d ctx->rspout=%u\n", __LINE__,  ctx->done, ctx->rspout);
    
    ctx->sendmore = FALSE;
    ck_pr_barrier();
    
    uint32_t endct_in = ctx->in;
    uint32_t endct_rspin = ctx->rspin;
    uint32_t endct_rspout = ctx->rspout;

    uint32_t endct_bncout = ctx->bncout;

    
    int iter=0;
    VERBPRN("\n\nMain test run done/timeout; pumping to flush events and/or finish...\n\nln=%d iter=%d ctx->done=%d ctx->rspout=%u bncout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);
    do {
      /* Fake pump the main queue until done */
      rwsched_dispatch_main_until(tenv.tasklet[0], 10, &ctx->done);
      iter++;
      VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->in=%u ctx->rspout=%u ctx->bncout=%u\n", __LINE__, iter, ctx->done, ctx->in, ctx->rspout, ctx->bncout);
      if (iter > 6) {
	fprintf(stderr, "waiting for done, iter=%d rspout=%u in=%u bnc=%u\n", iter, ctx->rspout, ctx->in, ctx->bncout);
      }
      if (!RUNNING_ON_VALGRIND) {
	ASSERT_LE(iter, 6);
      }
      ck_pr_barrier();
    } while (!ctx->done);

    ck_pr_barrier();
    
    VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->rspout=%u ctx->bcnout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);

    if (!ctx->bncout) {
      ASSERT_EQ(ctx->rspout, ctx->in);
      ASSERT_EQ(ctx->out, ctx->in);
      ASSERT_EQ(ctx->rspout, ctx->rspin);
      ASSERT_EQ(ctx->out, ctx->rspout);
      if (!ctx->opts->srvrspfromanyq) {
	ASSERT_FALSE(ctx->misorderct);
      }
    } else {
      /* Note that we can get bounces plus some might have been
	 answered even as they bounced on our end. */
      ASSERT_EQ(ctx->rspout + ctx->bncout, ctx->in); // rsp+bnc at the client == requests sent by client
      ASSERT_GE(ctx->out + ctx->bncout, ctx->in);
      ASSERT_LE(ctx->rspout, ctx->rspin);
      ASSERT_LE(ctx->out, ctx->bncout + ctx->rspout);
    }
    
    int rno;
    for (rno=0; rno < SCTEST1_RMAX; rno++) {
      ASSERT_TRUE(ctx->r[rno] == NULL);
    }

    valsout->sec = time(NULL) - sec_start; //sec;
    valsout->flushtime = iter*100;
    valsout->rmax = ctx->rmax;
    valsout->reqsent = endct_in;
    valsout->reqans = endct_rspin;
    valsout->reqdone = endct_rspout;
    valsout->reqbnc = endct_bncout;
    valsout->reqbnc_final = ctx->bncout;
    valsout->reqhz = (endct_rspout / sec / 10);
    valsout->reqhz *= 10;
    valsout->firstburst = ctx->sendmsg_preflow;
    valsout->min_rtt = ctx->min_rtt;
    valsout->max_rtt = ctx->max_rtt;

    /* Ought to work under val, but there is a cock-up with timers that makes the token refresh et al go wrong */
    if (ctx->flowexercise && !ctx->bncout && !RUNNING_ON_VALGRIND) {
      ASSERT_EQ(valsout->reqhz, TOKVAL);
    }

    if (ctx->flowexercise && ctx->sc_timer) {
      /* Fake pump the main queue for 1s, allow lingering async_f and
	 timer calls to flush out */
      rwsched_dispatch_source_cancel(tenv.tasklet[0], ctx->sc_timer);
      RW_ASSERT(bro);
      rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, ctx->sc_timer, tenv.rwsched, tenv.tasklet[0]);
      ctx->sc_timer = NULL;
      //      rwsched_dispatch_main_until(tenv.tasklet[0], 5, NULL);
    }
  } else {
    VERBPRN("\n***Squat!***\n");
  }

  //sleep(2);

  rwmsg_bool_t r;
  rwmsg_signature_release(ep, sig);
  rwmsg_method_release(ep, meth);
  rwmsg_srvchan_halt(ctx->sc);
  rwmsg_clichan_halt(ctx->cc);
  rwmsg_destination_release(dt);

  if (!mainq) {
    rwsched_dispatch_release(tenv.tasklet[0], ctx->rwq[0]);
    rwsched_dispatch_release(tenv.tasklet[1], ctx->rwq[1]);
  }

  r = rwmsg_endpoint_halt_flush(ep, TRUE);
  ASSERT_TRUE(r);

  if (bro) {
    r = rwmsg_broker_halt_sync(bro);
    ASSERT_TRUE(r);
  }

  RW_FREE_TYPE(ctx->paybuf, sctest1_payload);
  RW_FREE_TYPE(ctx, sctest1_context);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();

  if (rwmsg_global.status.request_ct != start_reqct) {
    fprintf(stderr, "***start_reqct=%u ending rwmsg_global.status.request_ct=%u\n", start_reqct, rwmsg_global.status.request_ct);
  }

  VERBPRN("end rwmsg_global.status.request_ct=%u\n", rwmsg_global.status.request_ct);
  VERBPRN("end rwmsg_global.status.qent=%u\n", rwmsg_global.status.qent);
}

#define PRINT_REPORT(txt) \
  fprintf(stderr, \
	  "\n" txt " at %lds sent=%lu answered=%lu done=%lu bnc=%lu bnc_final=%lu => %lu req/s\n" \
          "                  min_rtt=%lu max_rtt=%lu usec\n",\
	  vals.sec,\
	  vals.reqsent,\
	  vals.reqans,\
	  vals.reqdone,\
	  vals.reqbnc,\
          vals.reqbnc_final, \
	  vals.reqhz, \
          (vals.min_rtt==MAX_RTT?0:vals.min_rtt), \
          vals.max_rtt);

TEST(RWMsgBroker, LottaRawRWSConcurQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(LottaRawRWSConcurQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = TRUE;
  int broflo = FALSE;


  

  lottaraw(2, false, false, burst, broker, broflo, &opts, &vals); /* For *ConcurQ* tests */

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaRawRWSConcurQ");
  nnchk();
  RWUT_BENCH_END(LottaRawRWSConcurQBench);
}

static void lottaraw(int squat, int mainq, int fixedrateserver, int window, int usebro, int broflo, struct lottaopts *opts, struct lottarawvals *valsout);
static void lottaraw2bros(int squat, int mainq, int fixedrateserver, int window, int broflo, struct lottaopts *opts, struct lottarawvals *valsout);

TEST(RWMsgBroker, LottaRawRWMeasureRTT) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(LottaRawRWMeasureRTT, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  

  lottaraw(false, true, false, 2, FALSE, FALSE, &opts, &vals); /* No Broker tests MainQ*/
  PRINT_REPORT("RWMsgBroker.MeasureRTT.NoBroker.MainQ");
  ASSERT_LT(vals.max_rtt, 300*1000);

  lottaraw(false, false, false, 2, FALSE, FALSE, &opts, &vals); /* No Broker tests */
  PRINT_REPORT("RWMsgBroker.MeasureRTT.NoBroker");
  ASSERT_LT(vals.max_rtt, 300*1000);
  fprintf(stderr, "\n");

  lottaraw(false, true, false, 2, TRUE, FALSE, &opts, &vals); /* With Broker tests MainQ */
  PRINT_REPORT("RWMsgBroker.MeasureRTT.WithBroker.MainQ");
  ASSERT_LT(vals.max_rtt, 500*1000);

  lottaraw(false, false, false, 2, TRUE, FALSE, &opts, &vals); /* With Broker tests */
  PRINT_REPORT("RWMsgBroker.MeasureRTT.WithBroker.ConcurQ");
  ASSERT_LT(vals.max_rtt, 500*1000);
  fprintf(stderr, "\n");

  lottaraw2bros(false, true, false, 2, FALSE, &opts, &vals); /* For *MainQ* tests */
  PRINT_REPORT("RWMsgBroker.MeasureRTT.With2Brokers.MainQ");
  ASSERT_LT(vals.max_rtt, 500*1000);
  fprintf(stderr, "\n");

  lottaraw2bros(false, false, false, 2, FALSE, &opts, &vals); /* For *ConcurQ* tests */
  PRINT_REPORT("RWMsgBroker.MeasureRTT.With2Brokers.ConcurQ");
  ASSERT_LT(vals.max_rtt, 500*1000);
  fprintf(stderr, "\n");

  nnchk();
  RWUT_BENCH_END(LottaRawRWMeasureRTT);
}

TEST(RWMsgBroker, LottaRawRWSSerQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(LottaRawRWSSerQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = TRUE;
  int broflo = FALSE;


  

  lottaraw(false, false, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaRawRWSSerQ");

  nnchk();
  RWUT_BENCH_END(LottaRawRWSSerQBench);
}

TEST(RWMsgBroker, LottaRawRWSSerQRspManyQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues, srvchan answering from all default rwqueues");
  RWUT_BENCH_ITER(LottaRawRWSSerQRspManyQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.srvrspfromanyq = TRUE;
  int burst = 100;
  int broker = TRUE;
  int broflo = FALSE;

  lottaraw(false, false, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaRawRWSSerQRspManyQ");

  nnchk();
  RWUT_BENCH_END(LottaRawRWSSerQRspManyQBench);
}


#if 0
TEST(RWMsgBroker, LottaBrofloRawRWSSerQ) {
  TEST_DESCRIPTION("Tests in-broker flow control under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(LottaBrofloRawRWSSerQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = TRUE;
  int broflo = TRUE;

  lottaraw(false, false, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  fprintf(stderr, 
	  "\nRWMsgBroker.LottabrofloRawRWSSerQ at %lds sent=%lu answered=%lu done=%lu bnc=%lu => %lu req/s\n",
	  vals.sec,
	  vals.reqsent,
	  vals.reqans,
	  vals.reqdone,
	  vals.reqbnc,
	  vals.reqhz);
  nnchk();
  RWUT_BENCH_END(LottaBrofloRawRWSSerQBench);
}
#endif


TEST(RWMsgBrokerNot, LottaRawRWSConcurQ) {
  TEST_DESCRIPTION("Tests inprocess under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(NobroLottaRawRWSConcurQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = FALSE;
  int broflo = FALSE;

  lottaraw(2, false, false, burst, broker, broflo, &opts, &vals); /* For *ConcurQ* tests */

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBrokerNot.LottaRawRWSConcurQ");

  nnchk();
  RWUT_BENCH_END(NobroLottaRawRWSConcurQBench)
}

TEST(RWMsgBrokerNot, LottaRawRWSSerQ) {
  TEST_DESCRIPTION("Tests inprocess under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(NobroLottaRawRWSSerQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = FALSE;
  int broflo = FALSE;

  lottaraw(false, false, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBrokerNot.LottaRawRWSSerQ");

  nnchk();
  RWUT_BENCH_END(NobroLottaRawRWSSerQBench)
}

TEST(RWMsgBrokerNot, LottaRawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(NobroLottaRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = FALSE;
  int broflo = FALSE;
  int squat = FALSE;

  lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);


#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBrokerNot.LottaRawRWSMainQ");

  nnchk();

  RWUT_BENCH_END(NobroLottaRawRWSMainQBench);
}

TEST(RWMsgBroker, LottaRawRWSBigdefwinMainQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaRawRWSBigdefwinMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.bigdefwin = 100;
      int burst = 100;
      int broker = TRUE;
      int broflo = FALSE;
      int squat = FALSE;

      lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawRWSBigdefwinMainQ");

      nnchk();
  RWUT_BENCH_END(LottaRawRWSBigdefwinMainQBench);
}

TEST(RWMsgBroker, LottaRawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaRawRWSMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
      int burst = 100;
      int broker = TRUE;
      int broflo = FALSE;
      int squat = FALSE;

      lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawRWSMainQ");

      nnchk();
  RWUT_BENCH_END(LottaRawRWSMainQBench);
}
TEST(RWMsgBroker, LottaRawBncRWSMainQ) {
  TEST_DESCRIPTION("Exercise broker timeout bounce code path.");

  RWUT_BENCH_ITER(LottaRawBncRWSMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
      struct lottaopts opts;
      memset(&opts, 0, sizeof(opts));
      int burst = 1;
      int broker = TRUE;
      int broflo = FALSE;
      int squat = FALSE;
      opts.reqtimeo = 50;/*ms*/
      opts.bncall = TRUE;

      lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawBncRWSMainQ");

      nnchk();
  RWUT_BENCH_END(LottaRawBncRWSMainQBench);
}



TEST(RWMsgBroker, Big0512RawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker with big 512KB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big0512RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 512000;

  int burst = 10;
  int broker = TRUE;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBroker.Big0512RawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(Big0512RawRWSMainQBench);
}

TEST(RWMsgBroker, Big5120RawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker with big 5MB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big5120RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 5120000;

  int burst = 10;
  int broker = TRUE;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBroker.Big5120RawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(Big5120RawRWSMainQBench);
}



TEST(RWMsgBrokerNot, Big0512RawRWSMainQ) {
  TEST_DESCRIPTION("Tests with big 512KB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big0512RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 512000;

  int burst = 10;
  int broker = FALSE;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBrokerNot.Big0512RawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(Big0512RawRWSMainQBench);
}

TEST(RWMsgBrokerNot, Big5120RawRWSMainQ) {
  TEST_DESCRIPTION("Tests with big 5MB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big5120RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 5120000;

  int burst = 10;
  int broker = FALSE;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw(squat, true, false, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBrokerNot.Big5120RawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(Big5120RawRWSMainQBench);
}



TEST(RWMsgBroker, LottaPauseRawRWSSerQ) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Serial Queue");

  RWUT_BENCH_ITER(LottaPauseRawRWSSerQBench, 5, 5);
  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  burst=burst;
  int broker = TRUE;
  int broflo = FALSE;

  lottaraw(false, false/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseRawRWSSerQ");

  nnchk();
  RWUT_BENCH_END(LottaPauseRawRWSSerQBench);
}

TEST(RWMsgBroker, LottaPauseBigwinRawRWSSerQ) {
  TEST_DESCRIPTION("Tests broker pause under SCTEST1_RMAX requests outstanding, RWSched Serial Queue");

  RWUT_BENCH_ITER(LottaPauseBigwinRawRWSSerQBench, 5, 5);
  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = SCTEST1_RMAX;
  burst=burst;
  int broker = TRUE;
  int broflo = FALSE;
  opts.bigbrosrvwin = TRUE;

  lottaraw(false, false/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);


#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseBigwinRawRWSSerQ");

  nnchk();
  RWUT_BENCH_END(LottaPauseBigwinRawRWSSerQBench);
}

TEST(RWMsgBrokerNot, LottaPauseRawRWSSerQ) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Serial Queue");
  RWUT_BENCH_ITER(NobroLottaPauseRawRWSSerQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  burst=burst;
  int broker = FALSE;
  int broflo = FALSE;
  lottaraw(false, false/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBrokerNot.LottaPauseRawRWSSerQ");

  nnchk();
  RWUT_BENCH_END(NobroLottaPauseRawRWSSerQBench);
}
TEST(RWMsgBroker, LottaPauseRawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaPauseRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = TRUE;
  int broflo = FALSE;

  lottaraw(false, true/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseRawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(LottaPauseRawRWSMainQBench);
}
TEST(RWMsgBroker, LottaPauseBigwinRawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker pause under SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaPauseBigwinRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = SCTEST1_RMAX;
  int broker = TRUE;
  int broflo = FALSE;
  opts.bigbrosrvwin = TRUE;

  lottaraw(false, true/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseBigwinRawRWSMainQ");

  nnchk();
  RWUT_BENCH_END(LottaPauseBigwinRawRWSMainQBench);
}
TEST(RWMsgBrokerNot, LottaPauseRawRWSMainQ) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(NobroLottaPauseRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broker = FALSE;
  int broflo = FALSE;

  lottaraw(false, true/*mainq*/, true/*noread*/, burst, broker, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBrokerNot.LottaPauseRawRWSMainQ");

  nnchk();

  RWUT_BENCH_END(NobroLottaPauseRawRWSMainQBench);
}

static void lottaraw2bros(int squat, int mainq, int fixedrateserver, int window, int broflo, struct lottaopts *opts, struct lottarawvals *valsout) {
  uint32_t start_reqct = rwmsg_global.status.request_ct;
  VERBPRN("begin rwmsg_global.status.request_ct=%u\n", rwmsg_global.status.request_ct);

  if (valsout) {
    memset(valsout, 0, sizeof(*valsout));
  }

  if (RUNNING_ON_VALGRIND) {
    window /= 10;
    window = MAX(1, window);
  }

  const unsigned broker_c = 2;
  rwmsg_btenv_t tenv(2+(broker_c?broker_c:1));		// test env, on stack, dtor cleans up
  ASSERT_TRUE(tenv.rwsched != NULL);

  rwmsg_broker_t *bros[broker_c];
  rwmsg_broker_t *bro=NULL;
  memset(bros, 0, sizeof(bros));
  if (broker_c) {
    for (unsigned i=0; i<broker_c; i++) {
      tenv.ti->rwsched_tasklet_info = tenv.tasklet[2-(broker_c-i)];
      rwmsg_broker_main(broport_g, 2-(broker_c-i), i, tenv.rwsched, tenv.tasklet[2-(broker_c-i)], tenv.rwvcs_zk, mainq, tenv.ti, &bros[i]);
      bro = bros[i];
    }
  }

  rwsched_instance_CFRunLoopRunInMode(tenv.rwsched, RWMSG_RUNLOOP_MODE, 1, FALSE); 

  if (broflo) {
    bro->thresh_reqct2 = 10;
    bro->thresh_reqct1 = 5;
  }
  if (opts->bigbrosrvwin) {
    /* set the broker's srvchan window value really high, to cause overrun of writes to srvchan socket */
    rwmsg_endpoint_set_property_int(bro->ep, "/rwmsg/broker/srvchan/window", (RUNNING_ON_VALGRIND ? 1000 : 1000000));
  }

  rw_status_t rs;
  struct sctest1_context *ctx = RW_MALLOC0_TYPE(sizeof(*ctx), sctest1_context);
  ctx->usebro = TRUE;
  ctx->window = window;
  ctx->tenv = &tenv;
  ctx->paysz = sizeof(sctest1_payload);
  if (opts) {
    ctx->paysz = opts->paysz;
  }
  ctx->paybuf = RW_MALLOC0_TYPE(MAX(sizeof(struct sctest1_payload), ctx->paysz), sctest1_payload);
  ctx->verbose = VERBOSE();
  RW_ASSERT(opts);
  ctx->opts = opts;

  ctx->min_rtt = MAX_RTT;

  ctx->checktid=0;		// check thread id is same every time; adds a syscall per msg on each side
  if (ctx->checktid) {
    fprintf(stderr, "warning: checktid enabled\n");
  }
  
  const char *taddrpfx = "/L/GTEST/RWBRO/TESTAPP/6";
  char taddr[999];
  sprintf(taddr, "%s/%u", taddrpfx, rand());
  const uint32_t methno = __LINE__;

  /* Create the srvchan, bind a method */
  rwmsg_endpoint_t *ep_s;
  ep_s = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, tenv.tasklet[0], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_s != NULL);

  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/broker/shunt", 1);

  if (opts->bigdefwin) {
    rwmsg_endpoint_set_property_int(ep_s, "/rwmsg/destination/defwin", opts->bigdefwin);
  }

  ctx->sc = rwmsg_srvchan_create(ep_s);
  ASSERT_TRUE(ctx->sc != NULL);
  rwmsg_signature_t *sig = rwmsg_signature_create(ep_s, RWMSG_PAYFMT_RAW, methno, RWMSG_PRIORITY_DEFAULT);
  ASSERT_TRUE(sig);
  int timeo = 1000;		// ms
  if (opts->reqtimeo) {
    timeo = opts->reqtimeo;
  }
  if (RUNNING_ON_VALGRIND) {
    timeo = timeo * 5;
  }
  rwmsg_signature_set_timeout(sig, timeo);
  ctx->sig = sig;
  rwmsg_closure_t methcb;
  methcb.request=sctest1_methcb1;
  methcb.ud=ctx;
  rwmsg_method_t *meth = rwmsg_method_create(ep_s, taddr, sig, &methcb);
  ASSERT_TRUE(meth != NULL);
  rs = rwmsg_srvchan_add_method(ctx->sc, meth);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  /* Wait on binding the srvchan until after we've sent the initial
     flurry of requests.  Otherwise, our ++ and the request handler's
     ++ of some of the request count variables ABA-stomp each
     other. */

  /* Create the clichan, add the method's signature */
  rwmsg_endpoint_t *ep_c;
  ep_c = rwmsg_endpoint_create(1, 1, 1, tenv.rwsched, tenv.tasklet[1], rwtrace_init(), NULL);
  ASSERT_TRUE(ep_c != NULL);

  /* Direct everything through the broker */
  rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/broker/shunt", 1);

  if (opts->bigdefwin) {
    rwmsg_endpoint_set_property_int(ep_c, "/rwmsg/destination/defwin", opts->bigdefwin);
  }

  rwmsg_destination_t *dt = rwmsg_destination_create(ep_c, RWMSG_ADDRTYPE_UNICAST, taddr);
  ASSERT_TRUE(dt != NULL);
  ctx->dt = dt;

  if (squat!=2 ||
      ctx->flowexercise) {
    rwmsg_closure_t cb = { };
    cb.ud = ctx;
    cb.feedme = sctest1_clicb_feedme;
    rwmsg_destination_feedme_callback(dt, &cb);
  }
  rwmsg_clichan_t *cc = rwmsg_clichan_create(ep_c);
  ctx->cc = cc;
  ASSERT_TRUE(cc != NULL);
  //TBD/optional  rwmsg_clichan_add_signature(cc, sig);

  /* Bind to a serial queue */
  if (mainq) {
    ctx->rwq[0] = rwsched_dispatch_get_main_queue(tenv.rwsched);
    ctx->rwq[1] = rwsched_dispatch_get_main_queue(tenv.rwsched);
  } else {
    ctx->rwq[0] = rwsched_dispatch_queue_create(tenv.tasklet[0], "ctx->rwq[0] (cc)", DISPATCH_QUEUE_SERIAL);
    ctx->rwq[1] = rwsched_dispatch_queue_create(tenv.tasklet[1], "ctx->rwq[1] (sc)", DISPATCH_QUEUE_SERIAL);
  }

  rs = rwmsg_clichan_bind_rwsched_queue(cc, ctx->rwq[0]);

  if (window == 1) {
    ctx->sendmore = FALSE;
    ctx->sendone = TRUE;
  } else {
    ctx->sendmore = TRUE;
  }

  /* Now bind the srvchan; at this point the srvchan will start
     chewing in another thread. */
  rs = rwmsg_srvchan_bind_rwsched_queue(ctx->sc, ctx->rwq[1]);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  /* Setup  the srvchan to run at a fixed rate of 4 or 100 req/s, to exercise backpressure etc. */
  ctx->flowexercise = fixedrateserver;

  /* Fake pump the main queue for 1s to get connected, registered, etc */
  int setupsec = 1;
  if (RUNNING_ON_VALGRIND) {
    setupsec = setupsec * 5 / 2;
  }
  rwsched_dispatch_main_until(tenv.tasklet[0], setupsec, NULL);

  if (!squat) {

    if (ctx->flowexercise) {
      sctest1_refill_bucket(ctx);
      ctx->sc_timer = rwsched_dispatch_source_create(tenv.tasklet[1],
						     RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
						     0,
						     0,
						     ctx->rwq[1]);
      rwsched_dispatch_source_set_event_handler_f(tenv.tasklet[1],
						  ctx->sc_timer,
						  sctest1_timer);
      rwsched_dispatch_set_context(tenv.tasklet[1],
				   ctx->sc_timer,
				   ctx);
      uint64_t ms1000 = 1*NSEC_PER_SEC/1;
      rwsched_dispatch_source_set_timer(tenv.tasklet[1],
					ctx->sc_timer,
					dispatch_time(DISPATCH_TIME_NOW, ms1000),
					ms1000,
					0);
      rwsched_dispatch_resume(tenv.tasklet[1], ctx->sc_timer);
    }

    if (RUNNING_ON_VALGRIND) {
      /* Pump the main queue briefly to get everyone up and running before sending */
      rwsched_dispatch_main_until(tenv.tasklet[0], 1, NULL);
    }

    //lottakickoff(ctx);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff);

    int sec_start = time(NULL);
    double  sec = 5;
    if (RUNNING_ON_VALGRIND) {
      sec = sec * 5 / 2 ;
    }
    
    /* Run the main queue for 5s or longer under valgrind */
#if 1
    for(int i=0; !ctx->done && i<100; i++) {
        rwsched_dispatch_main_until(tenv.tasklet[0], sec/100, NULL);
    }
#else
    rwsched_dispatch_main_until(tenv.tasklet[0], sec, NULL);
#endif

    VERBPRN("\nln=%d ctx->done=%d ctx->rspout=%u\n", __LINE__,  ctx->done, ctx->rspout);
    
    ctx->sendmore = FALSE;
    ck_pr_barrier();
    
    uint32_t endct_in = ctx->in;
    uint32_t endct_rspin = ctx->rspin;
    uint32_t endct_rspout = ctx->rspout;

    uint32_t endct_bncout = ctx->bncout;

    
    int iter=0;
    VERBPRN("\n\nMain test run done/timeout; pumping to flush events and/or finish...\n\nln=%d iter=%d ctx->done=%d ctx->rspout=%u bncout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);
    do {
      /* Fake pump the main queue until done */
      rwsched_dispatch_main_until(tenv.tasklet[0], 10, &ctx->done);
      iter++;
      VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->in=%u ctx->rspout=%u ctx->bncout=%u\n", __LINE__, iter, ctx->done, ctx->in, ctx->rspout, ctx->bncout);
      if (iter > 6) {
	fprintf(stderr, "waiting for done, iter=%d rspout=%u in=%u bnc=%u\n", iter, ctx->rspout, ctx->in, ctx->bncout);
      }
      if (!RUNNING_ON_VALGRIND) {
	ASSERT_LE(iter, 6);
      }
      ck_pr_barrier();
    } while (!ctx->done);

    ck_pr_barrier();
    
    VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->rspout=%u ctx->bcnout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);

    if (!ctx->bncout) {
      ASSERT_EQ(ctx->rspout, ctx->in);
      ASSERT_EQ(ctx->out, ctx->in);
      ASSERT_EQ(ctx->rspout, ctx->rspin);
      ASSERT_EQ(ctx->out, ctx->rspout);
      if (!ctx->opts->srvrspfromanyq) {
	ASSERT_FALSE(ctx->misorderct);
      }
    } else {
      /* Note that we can get bounces plus some might have been
	 answered even as they bounced on our end. */
      ASSERT_EQ(ctx->rspout + ctx->bncout, ctx->in); // rsp+bnc at the client == requests sent by client
      ASSERT_GE(ctx->out + ctx->bncout, ctx->in);
      ASSERT_LE(ctx->rspout, ctx->rspin);
      ASSERT_LE(ctx->out, ctx->bncout + ctx->rspout);
    }
    
    int rno;
    for (rno=0; rno < SCTEST1_RMAX; rno++) {
      ASSERT_TRUE(ctx->r[rno] == NULL);
    }

    valsout->sec = time(NULL) - sec_start; //sec;
    valsout->flushtime = iter*100;
    valsout->rmax = ctx->rmax;
    valsout->reqsent = endct_in;
    valsout->reqans = endct_rspin;
    valsout->reqdone = endct_rspout;
    valsout->reqbnc = endct_bncout;
    valsout->reqbnc_final = ctx->bncout;
    valsout->reqhz = (endct_rspout / (ctx->tokenfill_ct-1));
    valsout->firstburst = ctx->sendmsg_preflow;
    valsout->min_rtt = ctx->min_rtt;
    valsout->max_rtt = ctx->max_rtt;

    /* Ought to work under val, but there is a cock-up with timers that makes the token refresh et al go wrong */
    if (ctx->flowexercise && !ctx->bncout && !RUNNING_ON_VALGRIND) {
      ASSERT_LE(valsout->reqhz, TOKVAL);
    }

    if (ctx->flowexercise && ctx->sc_timer) {
      /* Fake pump the main queue for 1s, allow lingering async_f and
	 timer calls to flush out */
      rwsched_dispatch_source_cancel(tenv.tasklet[0], ctx->sc_timer);
      RW_ASSERT(bro);
      rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, ctx->sc_timer, tenv.rwsched, tenv.tasklet[0]);
      ctx->sc_timer = NULL;
      //      rwsched_dispatch_main_until(tenv.tasklet[0], 5, NULL);
    }
  } else if (squat == 2) { /* For *ConcurQ* tests */
    rwsched_dispatch_queue_t cuncurQ = rwsched_dispatch_get_global_queue(tenv.tasklet[0], DISPATCH_QUEUE_PRIORITY_DEFAULT);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], cuncurQ, ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);
    rwsched_dispatch_async_f(tenv.tasklet[0], ctx->rwq[0], ctx, lottakickoff02);

    int sec_start = time(NULL);
    double  sec = 5;
    if (RUNNING_ON_VALGRIND) {
      sec = sec * 5 / 2 ;
    }
    
    /* Run the main queue for 5s or longer under valgrind */
#if 1
    for(int i=0; !ctx->done && i<100; i++) {
        rwsched_dispatch_main_until(tenv.tasklet[0], sec/100, NULL);
    }
#else
    rwsched_dispatch_main_until(tenv.tasklet[0], sec, NULL);
#endif

    VERBPRN("\nln=%d ctx->done=%d ctx->rspout=%u\n", __LINE__,  ctx->done, ctx->rspout);
    
    ctx->sendmore = FALSE;
    ck_pr_barrier();
    
    uint32_t endct_in = ctx->in;
    uint32_t endct_rspin = ctx->rspin;
    uint32_t endct_rspout = ctx->rspout;

    uint32_t endct_bncout = ctx->bncout;

    
    int iter=0;
    VERBPRN("\n\nMain test run done/timeout; pumping to flush events and/or finish...\n\nln=%d iter=%d ctx->done=%d ctx->rspout=%u bncout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);
    do {
      /* Fake pump the main queue until done */
      rwsched_dispatch_main_until(tenv.tasklet[0], 10, &ctx->done);
      iter++;
      VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->in=%u ctx->rspout=%u ctx->bncout=%u\n", __LINE__, iter, ctx->done, ctx->in, ctx->rspout, ctx->bncout);
      if (iter > 6) {
	fprintf(stderr, "waiting for done, iter=%d rspout=%u in=%u bnc=%u\n", iter, ctx->rspout, ctx->in, ctx->bncout);
      }
      if (!RUNNING_ON_VALGRIND) {
	ASSERT_LE(iter, 6);
      }
      ck_pr_barrier();
    } while (!ctx->done);

    ck_pr_barrier();
    
    VERBPRN("\nln=%d iter=%d ctx->done=%d ctx->rspout=%u ctx->bcnout=%u\n", __LINE__, iter, ctx->done, ctx->rspout, ctx->bncout);

    if (!ctx->bncout) {
      ASSERT_EQ(ctx->rspout, ctx->in);
      ASSERT_EQ(ctx->out, ctx->in);
      ASSERT_EQ(ctx->rspout, ctx->rspin);
      ASSERT_EQ(ctx->out, ctx->rspout);
      if (!ctx->opts->srvrspfromanyq) {
	ASSERT_FALSE(ctx->misorderct);
      }
    } else {
      /* Note that we can get bounces plus some might have been
	 answered even as they bounced on our end. */
      ASSERT_EQ(ctx->rspout + ctx->bncout, ctx->in); // rsp+bnc at the client == requests sent by client
      ASSERT_GE(ctx->out + ctx->bncout, ctx->in);
      ASSERT_LE(ctx->rspout, ctx->rspin);
      ASSERT_LE(ctx->out, ctx->bncout + ctx->rspout);
    }
    
    int rno;
    for (rno=0; rno < SCTEST1_RMAX; rno++) {
      ASSERT_TRUE(ctx->r[rno] == NULL);
    }

    valsout->sec = time(NULL) - sec_start; //sec;
    valsout->flushtime = iter*100;
    valsout->rmax = ctx->rmax;
    valsout->reqsent = endct_in;
    valsout->reqans = endct_rspin;
    valsout->reqdone = endct_rspout;
    valsout->reqbnc = endct_bncout;
    valsout->reqbnc_final = ctx->bncout;
    valsout->reqhz = (endct_rspout / (ctx->tokenfill_ct-1));
    valsout->firstburst = ctx->sendmsg_preflow;
    valsout->min_rtt = ctx->min_rtt;
    valsout->max_rtt = ctx->max_rtt;

    /* Ought to work under val, but there is a cock-up with timers that makes the token refresh et al go wrong */
    if (ctx->flowexercise && !ctx->bncout && !RUNNING_ON_VALGRIND) {
      ASSERT_EQ(valsout->reqhz, TOKVAL);
    }

    if (ctx->flowexercise && ctx->sc_timer) {
      /* Fake pump the main queue for 1s, allow lingering async_f and
	 timer calls to flush out */
      rwsched_dispatch_source_cancel(tenv.tasklet[0], ctx->sc_timer);
      RW_ASSERT(bro);
      rwmsg_garbage(&bro->ep->gc, RWMSG_OBJTYPE_RWSCHED_OBJREL, ctx->sc_timer, tenv.rwsched, tenv.tasklet[0]);
      ctx->sc_timer = NULL;
      //      rwsched_dispatch_main_until(tenv.tasklet[0], 5, NULL);
    }
  } else {
    VERBPRN("\n***Squat!***\n");
  }

  sleep(2);

  rwmsg_bool_t r;
  rwmsg_signature_release(ep_s, sig);
  rwmsg_method_release(ep_s, meth);
  rwmsg_srvchan_halt(ctx->sc);
  rwmsg_clichan_halt(ctx->cc);
  rwmsg_destination_release(dt);

  if (!mainq) {
    rwsched_dispatch_release(tenv.tasklet[0], ctx->rwq[0]);
    rwsched_dispatch_release(tenv.tasklet[0], ctx->rwq[1]);
  }

  r = rwmsg_endpoint_halt_flush(ep_c, TRUE);
  ASSERT_TRUE(r);

  r = rwmsg_endpoint_halt_flush(ep_s, TRUE);
  ASSERT_TRUE(r);

  if (broker_c) {
    for (unsigned i=0; i<broker_c; i++) {
      r = rwmsg_broker_halt_sync(bros[i]);
      ASSERT_TRUE(r);
    }
  }

  RW_FREE_TYPE(ctx->paybuf, sctest1_payload);
  RW_FREE_TYPE(ctx, sctest1_context);

  rwmsg_global_status_t gs;
  rwmsg_global_status_get(&gs);
  if (!gs.endpoint_ct) {
    ASSERT_EQ(gs.qent, 0);
  }
  nnchk();

  if (rwmsg_global.status.request_ct != start_reqct) {
    fprintf(stderr, "***start_reqct=%u ending rwmsg_global.status.request_ct=%u\n", start_reqct, rwmsg_global.status.request_ct);
  }

  VERBPRN("end rwmsg_global.status.request_ct=%u\n", rwmsg_global.status.request_ct);
  VERBPRN("end rwmsg_global.status.qent=%u\n", rwmsg_global.status.qent);
}

TEST(RWMsgBroker, LottaRawRWSConcurQ2Bros) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched per-channel Serial Queues");
  RWUT_BENCH_ITER(LottaRawRWSConcurQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broflo = FALSE;


  

  lottaraw2bros(2, false, false, burst, broflo, &opts, &vals); /* For *ConcurQ* tests */

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaRawRWSConcurQ2Bros");

  nnchk();
  RWUT_BENCH_END(LottaRawRWSConcurQBench);
}

TEST(RWMsgBroker, LottaRawRWSBigdefwinMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaRawRWSBigdefwinMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.bigdefwin = 100;
      int burst = 100;
      int broflo = FALSE;
      int squat = FALSE;

      lottaraw2bros(squat, true, false, burst, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawRWSBigdefwinMainQ2Bros");

      nnchk();
  RWUT_BENCH_END(LottaRawRWSBigdefwinMainQBench);
}

TEST(RWMsgBroker, LottaRawRWSMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaRawRWSMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
      int burst = 100;
      int broflo = FALSE;
      int squat = FALSE;

      lottaraw2bros(squat, true, false, burst, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawRWSMainQ2Bros");

      nnchk();
  RWUT_BENCH_END(LottaRawRWSMainQBench);
}
TEST(RWMsgBroker, LottaRawBncRWSMainQ2Bros) {
  TEST_DESCRIPTION("Exercise broker timeout bounce code path.");

  RWUT_BENCH_ITER(LottaRawBncRWSMainQBench, 5, 5);

      struct lottarawvals vals;
      memset(&vals, 0, sizeof(vals));
      struct lottaopts opts;
      memset(&opts, 0, sizeof(opts));
      int burst = 1;
      int broflo = FALSE;
      int squat = FALSE;
      opts.reqtimeo = 50;/*ms*/
      opts.bncall = TRUE;

      lottaraw2bros(squat, true, false, burst, broflo, &opts, &vals);

#if 0
      RecordProperty("FlushTime", vals.flushtime);
      RecordProperty("rmax", vals.rmax);
      RecordProperty("ReqSent", vals.reqsent);
      RecordProperty("ReqAnswered", vals.reqans);
      RecordProperty("ReqDone", vals.reqdone);
      RecordProperty("ReqHz", vals.reqhz);
#endif

      PRINT_REPORT("RWMsgBroker.LottaRawBncRWSMainQ2Bros");

      nnchk();
  RWUT_BENCH_END(LottaRawBncRWSMainQBench);
}



TEST(RWMsgBroker, Big0512RawRWSMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker with big 512KB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big0512RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 512000;

  int burst = 10;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw2bros(squat, true, false, burst, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBroker.Big0512RawRWSMainQ2Bros");

  nnchk();
  RWUT_BENCH_END(Big0512RawRWSMainQBench);
}

TEST(RWMsgBroker, Big5120RawRWSMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker with big 5MB requests, RWSched Main Queue");

  RWUT_BENCH_ITER(Big5120RawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  opts.paysz = 5120000;

  int burst = 10;
  int broflo = FALSE;
  int squat = FALSE;
  
  lottaraw2bros(squat, true, false, burst, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif
  
  PRINT_REPORT("RWMsgBroker.Big5120RawRWSMainQ2Bros");

  nnchk();
  RWUT_BENCH_END(Big5120RawRWSMainQBench);
}



TEST(RWMsgBroker, LottaPauseRawRWSSerQ2Bros) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Serial Queue");

  RWUT_BENCH_ITER(LottaPauseRawRWSSerQBench, 5, 5);
  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  burst=burst;
  int broflo = FALSE;

  lottaraw2bros(false, false/*mainq*/, true/*noread*/, burst, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseRawRWSSerQ2Bros");

  nnchk();
  RWUT_BENCH_END(LottaPauseRawRWSSerQBench);
}

TEST(RWMsgBroker, LottaPauseBigwinRawRWSSerQ2Bros) {
  TEST_DESCRIPTION("Tests broker pause under SCTEST1_RMAX requests outstanding, RWSched Serial Queue");

  RWUT_BENCH_ITER(LottaPauseBigwinRawRWSSerQBench, 5, 5);
  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = SCTEST1_RMAX;
  burst=burst;
  int broflo = FALSE;
  opts.bigbrosrvwin = TRUE;

  lottaraw2bros(false, false/*mainq*/, true/*noread*/, burst, broflo, &opts, &vals);


#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseBigwinRawRWSSerQ2Bros");

  nnchk();
  RWUT_BENCH_END(LottaPauseBigwinRawRWSSerQBench);
}


TEST(RWMsgBroker, LottaPauseRawRWSMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker pause under potloads of up to SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaPauseRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = 100;
  int broflo = FALSE;

  lottaraw2bros(false, true/*mainq*/, true/*noread*/, burst, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseRawRWSMainQ2Bros");

  nnchk();
  RWUT_BENCH_END(LottaPauseRawRWSMainQBench);
}

TEST(RWMsgBroker, LottaPauseBigwinRawRWSMainQ2Bros) {
  TEST_DESCRIPTION("Tests broker pause under SCTEST1_RMAX requests outstanding, RWSched Main Queue");

  RWUT_BENCH_ITER(LottaPauseBigwinRawRWSMainQBench, 5, 5);

  struct lottarawvals vals;
  memset(&vals, 0, sizeof(vals));
  struct lottaopts opts;
  memset(&opts, 0, sizeof(opts));
  int burst = SCTEST1_RMAX;
  int broflo = FALSE;
  opts.bigbrosrvwin = TRUE;

  lottaraw2bros(false, true/*mainq*/, true/*noread*/, burst, broflo, &opts, &vals);

#if 0
  RecordProperty("FlushTime", vals.flushtime);
  RecordProperty("rmax", vals.rmax);
  RecordProperty("ReqSent", vals.reqsent);
  RecordProperty("ReqAnswered", vals.reqans);
  RecordProperty("ReqDone", vals.reqdone);
  RecordProperty("ReqHz", vals.reqhz);
#endif

  PRINT_REPORT("RWMsgBroker.LottaPauseBigwinRawRWSMainQ2Bros");

  nnchk();
  RWUT_BENCH_END(LottaPauseBigwinRawRWSMainQBench);
}

TEST(RWMsgPerftool, CollapsedNofeedme) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "45",
    "--nofeedme",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "1000000",
    NULL
  };

#define RUNPERF()				\
  pid_t c = fork();				\
  if (!c) {					\
    int i=0; \
    while (argv[i]) fprintf(stderr,"%s ", argv[i++]); \
    fprintf(stderr,"\n"); \
    execv(filename, (char* const*) argv);	\
    abort();					\
  }						\
  RW_ASSERT(c > 0);				\
  int status = 0;				\
  pid_t pid = waitpid(c, &status, 0);		\
  ASSERT_GE(pid, 1);				\
  if (pid > 0) {				\
    ASSERT_EQ(WEXITSTATUS(status), 0);		\
  }

  RUNPERF();
}

TEST(RWMsgPerftool, LargeNofeedme) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "360",
    "--nofeedme",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "1000000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeNofeedme2Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "50",
    "--nofeedme",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeNofeedme1000) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "300",
    "--nofeedme",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "1000000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeNofeedme10002Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "300",
    "--nofeedme",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, DISABLED_CollapsedFeedmeBigwin) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "./rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "30",
    "--size",
    "64",
    "--window",
    "999999",
    "--count",
    "1000000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, DISABLED_LargeFeedmeBigwin) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "30",
    "--size",
    "64",
    "--window",
    "99999",
    "--count",
    "100000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, DISABLED_LargeFeedmeBigwin2Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "50",
    "--size",
    "64",
    "--window",
    "99999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, DISABLED_LargeFeedmeBigwin1000) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "30",
    "--size",
    "64",
    "--window",
    "99999",
    "--count",
    "100000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, DISABLED_LargeFeedmeBigwin10002Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--window",
    "1000",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "50",
    "--size",
    "64",
    "--window",
    "99999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, CollapsedFeedmeSmwin) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "30",
    "--size",
    "64",
    "--window",
    "999",
    "--count",
    "100000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeFeedmeSmwin) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "60",
    "--size",
    "64",
    "--window",
    "999",
    "--count",
    "100000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeFeedmeSmwin2Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "50",
    "--size",
    "64",
    "--window",
    "999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeFeedmeSmwin1000) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--runtime",
    "30",
    "--size",
    "64",
    "--window",
    "999",
    "--count",
    "100000",
    NULL
  };

  RUNPERF();
}

TEST(RWMsgPerftool, LargeFeedmeSmwin10002Bros) {
  int broport = rwmsg_broport_g(4);
  char nnuri[128];
  sprintf(nnuri, "tcp://%s:%d", RWMSG_CONNECT_ADDR_STR, broport);
  const char *filename = "rwmsg_perftool";
  const char *argv[] = {
    "rwmsg_perftool",
    "--nnuri",
    nnuri,
    "--shunt",
    "--tasklet",
    "broker",
    "--instance",
    "1",
    "--window",
    "1000",
    "--tasklet",
    "broker",
    "--instance",
    "2",
    "--window",
    "1000",
    "--tasklet",
    "server",
    "--instance",
    "1",
    "--broinst",
    "1",
    "--size",
    "64",
    "--tasklet",
    "client",
    "--instance",
    "1",
    "--broinst",
    "2",
    "--runtime",
    "50",
    "--size",
    "64",
    "--window",
    "999",
    "--count",
    "50000",
    NULL
  };

  RUNPERF();
}
