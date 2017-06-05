
/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwdts_gtest.cc
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

static void rwdts_test_api_state_changed(rwdts_api_t *apih, rwdts_state_t state, void *ud)
{
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  //EXPECT_EQ(state, RW_DTS_STATE_INIT);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info*) ud;

  ti->api_state_change_called = 1;

  return;
}

void RWDtsEnsemble::SetUpTestCase() {
    int i;
    /* Invoke below Log API to initialise shared memory */
    rwlog_init_bootstrap_filters(NULL);

    tenv.rwsched = rwsched_instance_new();
    ASSERT_TRUE(tenv.rwsched);

    tenv.rwvx = rwvx_instance_alloc();
    ASSERT_TRUE(tenv.rwsched);
    rw_status_t status = rwvcs_zk_zake_init(RWVX_GET_ZK_MODULE(tenv.rwvx));
    RW_ASSERT(RW_STATUS_SUCCESS == status);

    uint16_t redisport;
    const char *envport = getenv("RWDTS_REDIS_PORT");

    if (envport) {
      long int long_port;
      long_port = strtol(envport, NULL, 10);
      if (long_port < 65535 && long_port > 0)
        redisport = (uint16_t)long_port;
      else
        RW_ASSERT(long_port < 65535 && long_port > 0);
    } else {
      uint8_t uid;
      rw_status_t status;
      status = rw_unique_port(5342, &redisport);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rw_instance_uid(&uid);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      redisport += (105 * uid + getpid());
      if (redisport > 55530) {
        redisport -= 10000; // To manage max redis port range
      } else if (redisport < 1505) {
        redisport += 10000;
      }

      // Avoid the list of known port#s
      while (rw_port_in_avoid_list(redisport-1, 2) ||
             rw_port_in_avoid_list(redisport-1 + REDIS_CLUSTER_PORT_INCR, 1)) {
        redisport+=2;
      }
    }

    char tmp[16];
    sprintf(tmp, "%d", redisport-1);
    setenv ("RWMSG_BROKER_PORT", tmp, TRUE);

    //sprintf(tenv.redis_port, "%d", redisport);

    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      memset(ti, 0, sizeof(*ti));
      ti->tasklet_idx = (twhich_t)i;
      ti->member_idx = i - TASKLET_MEMBER_0;
      switch (i) {
        case TASKLET_ROUTER:
          sprintf(ti->rwmsgpath, "/R/RW.DTSRouter/%d", RWDTS_HARDCODED_ROUTER_ID);
          break;
        case TASKLET_BROKER:
          strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/BROKER/1");
          break;
        case TASKLET_CLIENT:
          strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/CLIENT/0");
          break;
        case TASKLET_NONMEMBER:
          strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/NONMEMBER/0");
          break;
        case TASKLET_ROUTER_3:
          strcat(ti->rwmsgpath, "/R/RW.DTSRouter/3");
          break;
        case TASKLET_MEMBER_3:
          strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/3");
          break;
        case TASKLET_ROUTER_4:
          strcat(ti->rwmsgpath, "/R/RW.DTSRouter/4");
          break;
        case TASKLET_MEMBER_4:
          strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/4");
          break;
        default:
          sprintf(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/%d", i-TASKLET_MEMBER_0);
          break;
      }

      ti->tasklet = rwsched_tasklet_new(tenv.rwsched);
      ASSERT_TRUE(ti->tasklet);

      ti->ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, ti->tasklet, rwtrace_init(), NULL);
      ASSERT_TRUE(ti->ep);

      const int usebro = TRUE;

      if (usebro) {
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/enable", TRUE);

        // We'd like not to set this for router, but the broker's forwarding lookup doesn't support local tasklets
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/shunt", TRUE);
      }

      ti->rwtasklet_info.rwsched_tasklet_info = ti->tasklet;
      ti->rwtasklet_info.ref_cnt = 1;

      uint64_t rout_id = RWDTS_HARDCODED_ROUTER_ID;
      switch (i) {
        case TASKLET_ROUTER:
        case TASKLET_ROUTER_3:
        case TASKLET_ROUTER_4:
          if (!ti->rwtasklet_info.rwlog_instance) {
            ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
          }
          ti->rwtasklet_info.rwmsg_endpoint = ti->ep;
          ti->rwtasklet_info.rwsched_instance = tenv.rwsched;
          ti->rwtasklet_info.rwvx = tenv.rwvx;
          ti->rwtasklet_info.rwvcs = tenv.rwvx->rwvcs;
          if (i== TASKLET_ROUTER_3) rout_id = 3;
          if (i== TASKLET_ROUTER_4) rout_id = 4;
          ti->dts = rwdts_router_init(ti->ep, rwsched_dispatch_get_main_queue(tenv.rwsched), 
                                      &ti->rwtasklet_info, ti->rwmsgpath, NULL, rout_id);

          ASSERT_NE(ti->dts, (void*)NULL);
          break;
        case TASKLET_BROKER:
          if (!ti->rwtasklet_info.rwlog_instance) {
            ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
          }
          rwmsg_broker_main(0, 1, 0, tenv.rwsched, ti->tasklet, NULL, TRUE/*mainq*/, NULL, &ti->bro);
          ASSERT_NE(ti->bro, (void*)NULL);
          break;
        case TASKLET_NONMEMBER:
          break;
        default:
          sleep(1);
          RW_ASSERT(i >= TASKLET_ROUTER); // else router's rwmsgpath won't be filled in
          rwdts_state_change_cb_t state_chg = {.cb = rwdts_test_api_state_changed, .ud = ti};
          uint64_t id = RWDTS_HARDCODED_ROUTER_ID;
          if (i== TASKLET_MEMBER_3) id = 3;
          if (i== TASKLET_MEMBER_4) id = 4;
          ti->apih = rwdts_api_init_internal(NULL, NULL, ti->tasklet, tenv.rwsched, ti->ep, ti->rwmsgpath, id, 0, &state_chg);
          ASSERT_NE(ti->apih, (void*)NULL);
          rwdts_api_set_ypbc_schema(ti->apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
          break;
      }
    }
    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      switch (i) {
        case TASKLET_ROUTER:
          rwdts_router_test_register_single_peer_router(ti->dts, 3);
          rwdts_router_test_register_single_peer_router(ti->dts, 4);
          break;
        case TASKLET_ROUTER_3:
          rwdts_router_test_register_single_peer_router(ti->dts, 1);
          rwdts_router_test_register_single_peer_router(ti->dts, 4);
          break;
        case TASKLET_ROUTER_4:
          rwdts_router_test_register_single_peer_router(ti->dts, 3);
          rwdts_router_test_register_single_peer_router(ti->dts, 1);
          break;
        default:
          break;
      }
    }

    /* Run a short time to allow broker to accept connections etc */
    rwsched_dispatch_main_until(tenv.t[0].tasklet, 4/*s*/, NULL);
  }

int RWDtsEnsemble::rwdts_test_get_running_xact_count() {
    int cnt = 0;
    int i;
    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      if (TASKLET_IS_MEMBER(ti->tasklet_idx) && ti->apih) {
        cnt += HASH_CNT(hh, ti->apih->xacts);
      }
    }
    return cnt;
  }

void RWDtsEnsemble::TearDownTestCase() {
    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];

      if (ti->dts) {
        int xct = HASH_COUNT(ti->dts->xacts);
        if (xct) {
          fprintf(stderr, "*** WARNING router still has %d xacts in flight\n", xct);
          rwdts_router_dump_xact_info(ti->dts->xacts, "All gtests are over");
        }
        EXPECT_EQ(xct, 0);   // should have no xacts floating around in the router
        rwdts_router_deinit(ti->dts);
        ti->dts = NULL;
      }

      if (ti->apih) {
        EXPECT_EQ(ti->api_state_change_called, 1);
        if (ti->apih->reg_timer) {
          rwsched_dispatch_source_cancel(ti->apih->tasklet, ti->apih->reg_timer);
          ti->apih->reg_timer = NULL;
        }

        ASSERT_EQ(0, HASH_COUNT(ti->apih->xacts));

        rw_status_t rs = rwdts_api_deinit(ti->apih);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        ti->apih = NULL;
      }

      if (ti->bro) {
        int r = rwmsg_broker_halt_sync(ti->bro);
        ASSERT_TRUE(r);
      }
      ASSERT_TRUE(ti->ep);

      /* Note trace ctx; it gets traced into during endpoint shutdown, so we have to close it later */
      rwtrace_ctx_t *tctx = ti->ep->rwtctx;

      int r = rwmsg_endpoint_halt_flush(ti->ep, TRUE);
      ASSERT_TRUE(r);
      ti->ep = NULL;

      if (tctx) {
        rwtrace_ctx_close(tctx);
      }
      if (ti->rwtasklet_info.rwlog_instance) {
        rwlog_close(ti->rwtasklet_info.rwlog_instance,TRUE);
        ti->rwtasklet_info.rwlog_instance = NULL;
      }

      ASSERT_TRUE(ti->tasklet);
      rwsched_tasklet_free(ti->tasklet);
      ti->tasklet = NULL;
    }

    ASSERT_TRUE(tenv.rwsched);
    rwsched_instance_free(tenv.rwsched);
    tenv.rwsched = NULL;
  }
  
void RWDtsEnsemble::SetUp() {
  ASSERT_NE(tenv.t[TASKLET_CLIENT].apih, (void*)NULL);
  ASSERT_NE(tenv.t[TASKLET_MEMBER_0].apih, (void*)NULL);
  ASSERT_NE(tenv.t[TASKLET_MEMBER_1].apih, (void*)NULL);
  ASSERT_NE(tenv.t[TASKLET_MEMBER_2].apih, (void*)NULL);
  ASSERT_NE(tenv.t[TASKLET_MEMBER_3].apih, (void*)NULL);
  ASSERT_NE(tenv.t[TASKLET_MEMBER_4].apih, (void*)NULL);

  tenv.testno++;


  for (int i=TASKLET_CT-1; i>=0; i--) {
    struct tenv1::tenv_info *ti = &tenv.t[i];

    if (i==TASKLET_CT-1) {
      if (getenv("RWDTS_GTEST_SEGREGATE")) {
        fprintf(stderr, "SetUp(), tenv.testno=%d, RWDTS_GTEST_SEGREGATE=1, waiting for straggler events etc\n", tenv.testno);
        rwsched_dispatch_main_until(ti->tasklet, 70, NULL);
      }
    }

    if (ti->apih) {
      /* Blocking scheduler invokation until this tasklet's DTS API is ready */
      int maxtries=500;
      while (!ti->api_state_change_called && maxtries--) {
        rwsched_dispatch_main_until(ti->tasklet, 30, &ti->api_state_change_called);
        ASSERT_EQ(ti->api_state_change_called, 1);
      }
    }
  }
}

void RWDtsEnsemble::TearDown() {

  /* See if there are any xacts still floating around.  Give them 2s to clear out, then go boom. */
  int xct = 0;
  for (int i=TASKLET_CT-1; i>=0; i--) {
    struct tenv1::tenv_info *ti = &tenv.t[i];
    RW_ASSERT(ti);
    if (ti->apih) {
      xct += HASH_COUNT(ti->apih->xacts);
    }
    if (ti->dts) {
      int xct = HASH_COUNT(ti->dts->xacts);
      if (xct) {
        double seconds = (RUNNING_ON_VALGRIND)?30:2;
        fprintf(stderr, "Router has  %d xacts outstanding, waiting %Gs to see if they go away\n", xct, seconds);
        rwsched_dispatch_main_until(tenv.t[TASKLET_CLIENT].tasklet, seconds, NULL);
      }
      xct = HASH_COUNT(ti->dts->xacts);
      if (xct) {
        fprintf(stderr, "*** WARNING router still has %d xacts in flight\n", xct);
        rwdts_router_dump_xact_info(ti->dts->xacts, "Gtest is over");
      }
      EXPECT_EQ(xct, 0);   // should have no xacts floating around in the router
    }
  }
  if (xct) {
    double seconds = (RUNNING_ON_VALGRIND)?30:2;
    fprintf(stderr, "Test left %d xacts outstanding, waiting %Gs to see if they go away\n", xct, seconds);
    rwsched_dispatch_main_until(tenv.t[TASKLET_CLIENT].tasklet, seconds, NULL);
  }

  for (int i=TASKLET_CT-1; i>=0; i--) {
    struct tenv1::tenv_info *ti = &tenv.t[i];
    RW_ASSERT(ti);
    if (ti->apih) {
      xct = HASH_COUNT(ti->apih->xacts);
      if (xct > 0) {
        char tmp_log_xact_id_str[128] = "";
        rwdts_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
        HASH_ITER(hh, ti->apih->xacts, xact_entry, tmp_xact_entry) {
          fprintf(stderr,
                  "*** LEAKED XACT apih=%p client=%s xact=%p xact[%s] ref_cnt=%d, _query_count=%d\n",
                  ti->apih,
                  ti->apih->client_path,
                  xact_entry,
                  (char*)rwdts_xact_id_str(&(ti->apih->xacts)->id, tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)),
                  xact_entry->ref_cnt,
                  HASH_COUNT(xact_entry->queries));

          rwdts_xact_query_t *query=NULL, *qtmp=NULL;
          HASH_ITER(hh, xact_entry->queries, query, qtmp) {
            ASSERT_NE(query->evtrsp, RWDTS_EVTRSP_ASYNC);
          }
        }
      }
      ASSERT_EQ(0, xct);
    }
    if (ti->dts) {
      EXPECT_EQ(HASH_COUNT(ti->dts->xacts), 0);   // should have no xacts floating around in the router
    }


    rwdts_api_t *apih = ti->apih;
    if (apih && RW_SKLIST_LENGTH(&(apih->reg_list))) {
      rwdts_api_reset_stats(apih);
      rwdts_member_registration_t *entry = NULL, *next_entry = NULL;

      entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
      while (entry) {
        next_entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
        if (((rwdts_member_registration_t *)apih->init_regidh != entry) && ((rwdts_member_registration_t *)apih->init_regkeyh != entry)) {
          if(entry->stats.out_of_order_queries)
          {
            //EXPECT_EQ(entry->stats.out_of_order_queries, 2);
          }
          if (!entry->dts_internal) {
            rw_status_t rs = rwdts_member_registration_deinit(entry);
            RW_ASSERT(rs == RW_STATUS_SUCCESS);
          }
        }
        entry = next_entry;
      }
    }

    rwdts_router_t *dts = ti->dts;
    if (dts && HASH_CNT(hh, dts->members)) {
      rwdts_shard_del(&dts->rootshard);
      RW_ASSERT(dts->rootshard->children);
      rwdts_router_member_t *memb=NULL, *membnext=NULL;
      HASH_ITER(hh, dts->members, memb, membnext) {
        if (strstr(memb->msgpath, "DTSRouter")) {
          continue;
        }
        rwdts_router_remove_all_regs_for_gtest(dts, memb);
      }
    }
  }
  usleep(100);
}



rw_status_t
rwdts_member_deregister_all(rwdts_api_t* apih)
{

  rwdts_member_registration_t *entry = NULL, *next_entry = NULL;
  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (entry) {
    next_entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
    if (!entry->dts_internal) {
      rwdts_member_deregister((rwdts_member_reg_handle_t)entry);
    }
    entry = next_entry;
  }
  return RW_STATUS_SUCCESS;
}
