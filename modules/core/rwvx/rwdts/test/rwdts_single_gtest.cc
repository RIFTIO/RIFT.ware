/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwdts_single_gtest.cc
 * @author RIFT.io <info@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwdts
 *
 * @details Google test cases for testing rwmsg.
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
#include <ctime>
#include<iostream>

#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"

#include <unistd.h>
#include <sys/syscall.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwtasklet.h>

#include <rwmsg.h>
#include <rwmsg_int.h>
#include <rwmsg_broker.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_router.h>
#include <dts-test.pb-c.h>
#include <testdts-rw-fpath.pb-c.h>
#include <testdts-rw-stats.pb-c.h>
#include <rwdts_redis.h>
#include <sys/prctl.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rwdts_appconf_api.h>
#include <rwdts_xpath.h>
#include "rwdts_ensemble.hpp"



struct tenv1 RWDtsEnsemble::tenv;

TEST(RWDtsAPI, Init) {
  TEST_DESCRIPTION("Tests DTS API Init");

  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, ep, "/L/API/TEST/1", RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  EXPECT_EQ((void*)RWPB_G_SCHEMA_YPBCSD(DtsTest), (void*)rwdts_api_get_ypbc_schema(apih));

  /* Ruh-roh, api_init() now fires member registration, which fires off a dispatch_after_f, which we can't easily wait around for */
  rw_status_t rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  rwsched_instance_free(rwsched);
  rwsched = NULL;
}




RWUT_INIT();


TEST_F(RWDtsEnsemble, CheckSetup) {
  ASSERT_TRUE(tenv.rwsched);

  /* This is the tasklet's DTS Router instance, only in router tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].dts);
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER_3].dts);
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER_4].dts);
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_3].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_4].dts);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].dts);

  /* This is the tasklet's RWMsg Broker instance, only in broker tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_BROKER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER_3].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER_4].bro);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_3].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_4].bro);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].bro);

  /* This is the tasklet's RWDts API Handle, in client, member, and router */
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].apih);
  ASSERT_TRUE(tenv.t[TASKLET_CLIENT].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_0].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_1].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_2].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_3].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_4].apih);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].apih);

  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_0].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_1].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_2].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_3].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_4].apih));
}
