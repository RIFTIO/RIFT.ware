/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwdts_gtest_redis.cc
 * @author RIFT.io <info@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwdts
 *
 * @details Google test cases for testing rwdts.
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

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>

#include <rwmsg.h>
#include <rwmsg_int.h>
#include <rwmsg_broker.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_router.h>
#include <dts-test.pb-c.h>
#include <testdts-rw-fpath.pb-c.h>
#include <rwdts_redis.h>
#include <sys/prctl.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rwdts_appconf_api.h>

using ::testing::HasSubstr;

typedef struct memberapi_test_s {
  rwdts_xact_t         *xact;
  rwdts_query_handle_t qhdl;
  rw_keyspec_path_t *key;
} memberapi_test_t;

static void rwdts_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);

static rwdts_member_rsp_code_t
memberapi_test_prepare(const rwdts_xact_info_t* xact_info,
                       RWDtsQueryAction         action,
                       const rw_keyspec_path_t*      key,
                       const ProtobufCMessage*  msg,
                       uint32_t credits,
                       void *getnext_ptr);


static rwdts_member_rsp_code_t
memberapi_test_precommit(const rwdts_xact_info_t*      xact_info,
                         uint32_t                      n_crec,
                         const rwdts_commit_record_t** crec);

static rwdts_member_rsp_code_t
memberapi_test_commit(const rwdts_xact_info_t*      xact_info,
                      uint32_t                      n_crec,
                      const rwdts_commit_record_t** crec);


/* Set env variable V=1 and TESTPRN("foo", ...") will be go to stderr */
#define TSTPRN(args...) {                        \
  if (!venv_g.checked) {                        \
    const char *e = getenv("V");                \
    if (e && atoi(e) == 1) {                        \
      venv_g.val = 1;                                \
    }                                                \
    venv_g.checked = 1;                                \
  }                                                \
  if (venv_g.val) {                                \
    fprintf(stderr, args);                        \
  }                                                \
}
static struct {
  int checked;
  int val;
} venv_g;

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

/* A slight abuse of "unit" testing, to permit DTS testing in a
   simplified multi-tasklet environment. */

typedef enum {
  TASKLET_ROUTER=0,
  TASKLET_BROKER,
  TASKLET_NONMEMBER,
  TASKLET_CLIENT,

#define TASKLET_IS_MEMBER(t) ((t) >= TASKLET_MEMBER_0)

  TASKLET_MEMBER_0,
  TASKLET_MEMBER_1,
  TASKLET_MEMBER_2,
#define TASKLET_MEMBER_CT (3)

  TASKLET_CT
} twhich_t;
struct tenv1 {
  rwsched_instance_ptr_t rwsched;

  //Redis related
  char *rserver_path;
  char *redis_conf;
  rwdts_redis_instance_t *redis_instance;
  rwdts_kv_handle_t *handle;
  rwdts_kv_table_handle_t *tab_handle[3];
  rwdts_kv_table_handle_t *kv_tab_handle;
  int next_serial_number[3];
  bool queried_shard_id[3];
  rwsched_tasklet_ptr_t redis_tasklet;
  pid_t pid;
  bool init;
  uint32_t redis_up;
  bool child_killed;
  int set_count;
  char redis_port[16];

  struct tenv_info {
    twhich_t tasklet_idx;
    int member_idx;
    char rwmsgpath[1024];
    rwsched_tasklet_ptr_t tasklet;
    rwmsg_endpoint_t *ep;
    rwtasklet_info_t rwtasklet_info;

    // each tasklet might have one or more of:
    rwdts_router_t *dts;
    rwdts_api_t *apih;
    rwmsg_broker_t *bro;

    void *ctx;                        // test-specific state ie queryapi_state
  } t[TASKLET_CT];
};
class RWDtsRedisEnsemble : public ::testing::Test {
protected:
  static void SetUpTestCase() {
    int i;
    char *rw_install_dir;
    rw_status_t status;
    tenv.rwsched = rwsched_instance_new();
    ASSERT_TRUE(tenv.rwsched);

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
      status = rw_unique_port(5342, &redisport);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rw_instance_uid(&uid);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      redisport += (105 * uid + getpid());
      if (redisport > 55535) {
        redisport -= 10000; // To manage max redis port range
      } else if (redisport < 1500) {
        redisport += 10000;
      }

      // Avoid the list of known port#s
      while (rw_port_in_avoid_list(redisport-1, 2))
        redisport+=2;
    }

    char tmp[16];
    sprintf(tmp, "%d", redisport-1);
    setenv ("RWMSG_BROKER_PORT", tmp, TRUE);

    sprintf(tenv.redis_port, "%d", redisport);

    signal(SIGCHLD,signalHandler);
    tenv.handle = (rwdts_kv_handle_t *)rwdts_kv_allocate_handle(REDIS_DB);

    tenv.redis_tasklet = rwsched_tasklet_new(tenv.rwsched);
    ASSERT_TRUE(tenv.redis_tasklet);

    tenv.redis_instance = NULL;
    tenv.redis_up = 0;
    tenv.set_count = 0;
    tenv.child_killed = 0;
    rw_install_dir = getenv("RIFT_INSTALL");
    tenv.rserver_path = RW_STRDUP_PRINTF("%s/usr/bin/redis_cluster.py", rw_install_dir);
    tenv.redis_conf = RW_STRDUP_PRINTF("%s/usr/bin/redis.conf", rw_install_dir);

    /* Invoke below Log API to initialise shared memory */
    rwlog_init_bootstrap_filters(NULL);

    for (i = 0; i < 3; i++) {
      tenv.next_serial_number[i] = 0;
      tenv.queried_shard_id[i] = 0;
    }

    tenv.pid = 0;
    if (!getenv("RWDTS_GTEST_NO_REDIS")) {
      tenv.pid = fork();

      if (!tenv.pid) { /*child*/
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        std::string num_nodes = std::to_string(3);
        char const *const argv[] = {
          "python",
          (char *)tenv.rserver_path,
          "-n",
          (char *)(num_nodes.c_str()),
          "-p",
          (char *)tenv.redis_port,
          "-c",
          NULL};
        execvp(argv[0], (char **)argv);
        fprintf(stderr, "execv BIZZARE error(%s)\n", strerror(errno));
        return; /*BIZZARE EXECV ERROR*/
      }
      tenv.init = 1;
      sleep(20); /*ugly, sleep till redis is ready */
      startClient();
    }

    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      memset(ti, 0, sizeof(*ti));
      ti->tasklet_idx = (twhich_t)i;
      ti->member_idx = i - TASKLET_MEMBER_0;
      switch (i) {
      case TASKLET_ROUTER:
        if (!ti->rwtasklet_info.rwlog_instance) {
          ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        sprintf(ti->rwmsgpath, "/R/RW.DTSRouter/%d", RWDTS_HARDCODED_ROUTER_ID);
        break;
      case TASKLET_BROKER:
        if (!ti->rwtasklet_info.rwlog_instance) {
         ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/BROKER/0");
        break;
      case TASKLET_CLIENT:
        if (!ti->rwtasklet_info.rwlog_instance) {
         ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/CLIENT/0");
        break;
      case TASKLET_NONMEMBER:
        if (!ti->rwtasklet_info.rwlog_instance) {
         ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/NONMEMBER/0");
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

      switch (i) {
      case TASKLET_ROUTER:
        ti->rwtasklet_info.rwmsg_endpoint = ti->ep;
        ti->rwtasklet_info.rwsched_instance = tenv.rwsched;
        ti->dts = rwdts_router_init(ti->ep, rwsched_dispatch_get_main_queue(tenv.rwsched), &ti->rwtasklet_info, ti->rwmsgpath, NULL, 1);
        ASSERT_NE(ti->dts, (void*)NULL);
        break;
      case TASKLET_BROKER:
        rwmsg_broker_main(0, 1, 0, tenv.rwsched, ti->tasklet, NULL, TRUE/*mainq*/, NULL, &ti->bro);
        ASSERT_NE(ti->bro, (void*)NULL);
        break;
      case TASKLET_NONMEMBER:
        break;
      default:
        RW_ASSERT(i >= TASKLET_ROUTER); // else router's rwmsgpath won't be filled in
        ti->apih = rwdts_api_init_internal(NULL, NULL, ti->tasklet, tenv.rwsched, ti->ep,  ti->rwmsgpath, RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
        ASSERT_NE(tenv.t[TASKLET_CLIENT].apih, (void*)NULL);
        rwdts_api_set_ypbc_schema(ti->apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  ti->apih->handle = tenv.handle;
        break;
      }
    }

    /* Run a short time to allow broker to accept connections etc */
    rwsched_dispatch_main_until(tenv.t[0].tasklet, 2/*s*/, NULL);
  }

  static void signalHandler(int signal)
  {
    fprintf(stderr, "Caught signal %d with child_killed==%d, pid= %d!\n",signal, tenv.child_killed, tenv.pid);

    int status = 0;

    pid_t pid = wait(&status);

    fprintf(stderr, "Child(pid = %d) exited status = %d, normal[%c - %d] signal[%c - %d] \n", pid, status,
            WIFEXITED(status)?'Y':'N',    WIFEXITED(status)?WIFEXITED(status):0,
            WIFSIGNALED(status)?'Y':'N',  WIFSIGNALED(status) ?WTERMSIG(status):0);

    if (!tenv.child_killed && signal==SIGCHLD) {
      RW_CRASH();
    }
  }

  static void TearDownTestCase() {
    if (tenv.pid) {
      fprintf(stderr, "tearing down and killing child\n");
      tenv.child_killed = 1;
      kill(tenv.pid, SIGINT);
      tenv.init = 0;
      tenv.redis_up = 0;
      sleep(5); /*wait for redis-server to cleanup resources */

      stopClient();
    }


    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];

      if (ti->dts) {
        rwdts_router_deinit(ti->dts);
        ti->dts = NULL;
      }

      if (ti->apih) {
        if (ti->apih->timer) {
          rwsched_dispatch_source_cancel(ti->apih->tasklet, ti->apih->timer);
          ti->apih->timer = NULL;
        }
        rw_status_t rs = rwdts_api_deinit(ti->apih);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        ti->apih = NULL;
      }

      if (ti->bro) {
        int r = rwmsg_broker_halt_sync(ti->bro);
        ASSERT_TRUE(r);
      }

      ASSERT_TRUE(ti->ep);
      int r = rwmsg_endpoint_halt_flush(ti->ep, TRUE);
      ASSERT_TRUE(r);
      ti->ep = NULL;

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
    //sleep(5);
  }

public:
  static struct tenv1 tenv;

  static void startClient(void);
  static void stopClient(void);

  static void rwdts_test_registration_cb(RWDtsRegisterRsp*    rsp,
                                         rwmsg_request_t*     rwreq,
                                          void*                ud);

  static rwdts_kv_light_reply_status_t
  rwdts_kv_light_set_callback(rwdts_kv_light_set_status_t status,
                              int serial_num, void *userdata);

  static void
  rwdts_shard_info_cb(RWDtsGetDbShardInfoRsp*    rsp,
                      rwmsg_request_t*     rwreq,
                      void*                ud);

  static void
  rwdts_kv_light_get_shard_callback(void **val,
                                    int *val_len,
                                    void **key,
                                    int *key_len,
                                    int *serial_num,
                                    int total,
                                    void *userdata,
                                    void *next_block);

};

static void RedisReady(struct tenv1 *tenv, rw_status_t status);

struct abc {
  int a;
  int b;
  int c;
} a;

const char *key_entry[30] = {"name1", "name2", "name3", "name4", "name5", "name6", "name7",
                      "name8", "name9", "name10", "name11", "name12", "name13",
                      "name14", "name15", "name16", "name17", "name18", "name19"};

const char *tab_entry[30] = {"TEST", "Sheldon", "Leonard", "Raj", "Howard", "Bernie",
                       "Amy", "Steve", "Wilma", "Fred", "Charlie", "Penny","What",
                       "When", "Why", "Who", "How", "BBC", "CNN"};

int set_count, shard_delete, set_aborted, set_committed, del_committed, del_aborted, del_count;

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_callback(void *val, int val_len, void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_all_callback(void **key,
                                                                     int *keylen,
                                                                     void **value,
                                                                     int *val_len,
                                                                     int total,
                                                                     void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_set_xact_callback(rwdts_kv_light_set_status_t status, int serial_num, void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_del_xact_callback(rwdts_kv_light_del_status_t status, int serial_num, void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_xact_del_callback(void *val, int val_len, void *userdata)
{
   struct abc *reply = (struct abc*)val;
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   if (del_committed == 1) {
     EXPECT_TRUE( reply == NULL);
     rwdts_kv_light_deregister_table(tab_handle);
   } else {
     EXPECT_TRUE(reply != NULL);
     EXPECT_TRUE((reply->a == a.a) && (reply->b == a.b) && (reply->c == a.c));
     rwdts_kv_light_table_xact_delete(tab_handle, 3, (char *)"FOO", 3, (void *)rwdts_kv_light_del_xact_callback, (void *)tab_handle);
   }
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_del_xact_com_abort_callback(rwdts_kv_light_del_status_t status, void *userdata)
{
    rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
    EXPECT_TRUE(RWDTS_KV_LIGHT_DEL_SUCCESS == status);
    rwdts_kv_light_table_get_by_key(tab_handle, (char *)"FOO", 3, (void *)rwdts_kv_light_get_xact_del_callback, (void *)tab_handle);
    return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_del_xact_callback(rwdts_kv_light_del_status_t status, int serial_num, void *userdata)
{
    rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
    EXPECT_TRUE(RWDTS_KV_LIGHT_DEL_SUCCESS == status);
    EXPECT_TRUE(serial_num == 4);
    if (del_aborted == 1) {
      rwdts_kv_light_table_xact_delete_commit(tab_handle, serial_num, (char *)"FOO", 3, (void *)rwdts_kv_light_del_xact_com_abort_callback, (void *)tab_handle);
      del_committed = 1;
    } else {
      rwdts_kv_light_table_xact_delete_abort(tab_handle, serial_num, (char *)"FOO", 3, (void *)rwdts_kv_light_del_xact_com_abort_callback, (void *)tab_handle);
      del_aborted = 1;
    }
    return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_xact_callback(void *val, int val_len, void *userdata)
{
   struct abc *reply = (struct abc*)val;
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   if (set_committed == 0) {
     EXPECT_TRUE( reply == NULL);
     a.a = 1;
     a.b = 2;
     a.c = 3;
     rwdts_kv_light_table_xact_insert(tab_handle, 1, (char *)"What", (char*)"FOO", 3,
                                      (void*)&a, sizeof(a),
                                      (void *)rwdts_kv_light_set_xact_callback, (void*)tab_handle);
   } else {
     EXPECT_TRUE(reply != NULL);
     EXPECT_TRUE((reply->a == a.a) && (reply->b == a.b) && (reply->c == a.c));
     rwdts_kv_light_table_xact_delete(tab_handle, 3, (char *)"FOO", 3, (void *)rwdts_kv_light_del_xact_callback, (void *)tab_handle);
   }
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_insert_xact_callback(rwdts_kv_light_set_status_t status, int serial_num, void *userdata)
{
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);
   rwdts_kv_light_table_get_by_key(tab_handle, (char *)"FOO", 3, (void *)rwdts_kv_light_get_xact_callback, (void *)tab_handle);
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_set_xact_callback(rwdts_kv_light_set_status_t status, int serial_num, void *userdata)
{
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);
   if (set_aborted == 1) {
     rwdts_kv_light_api_xact_insert_commit(tab_handle, serial_num, (char *)"What", (char *)"FOO", 3, (void *)rwdts_kv_light_insert_xact_callback, (void *)tab_handle);
     set_committed = 1;
   } else {
     rwdts_kv_light_api_xact_insert_abort(tab_handle, serial_num, (char *)"What", (char *)"FOO", 3, (void *)rwdts_kv_light_insert_xact_callback, (void *)tab_handle);
     set_aborted = 1;
   }
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

void RWDtsRedisEnsemble::startClient(void)
{
  char address_port[20] = "127.0.0.1:";
  strcat(address_port, tenv.redis_port);
  rw_status_t status = rwdts_kv_handle_db_connect(tenv.handle, tenv.rwsched, tenv.redis_tasklet,
                                                  address_port,
                                                  "test", NULL,
                                                  (void *)RedisReady,
                                                  (void *)&tenv);
  ASSERT_TRUE(RW_STATUS_SUCCESS == status);
}

void RWDtsRedisEnsemble::stopClient(void)
{
  if (tenv.redis_instance) {
    rwdts_redis_instance_deinit(tenv.redis_instance); //close all the connections
    tenv.handle->kv_conn_instance = NULL;
  }
}

struct tenv1 RWDtsRedisEnsemble::tenv;

TEST_F(RWDtsRedisEnsemble, CheckSetup) {
  ASSERT_TRUE(tenv.rwsched);

  /* This is the tasklet's DTS Router instance, only in router tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].dts);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].dts);

  /* This is the tasklet's RWMsg Broker instance, only in broker tasklet */
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER].bro);
  ASSERT_TRUE(tenv.t[TASKLET_BROKER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].bro);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].bro);

  /* This is the tasklet's RWDts API Handle, in client, member, and router */
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].apih);
  ASSERT_TRUE(tenv.t[TASKLET_CLIENT].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_0].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_1].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_2].apih);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].apih);
}

enum treatment_e {
  TREATMENT_ACK=0,
  TREATMENT_NACK,
  TREATMENT_NA,
  TREATMENT_ERR,
  TREATMENT_ASYNC,
  TREATMENT_BNC,
  TREATMENT_PRECOMMIT_NACK,
  TREATMENT_COMMIT_NACK,
  TREATMENT_ABORT_NACK
};

enum expect_result_e {
  XACT_RESULT_COMMIT=0,
  XACT_RESULT_ABORT,
  XACT_RESULT_ERROR
};


struct qapi_member {
  rwsched_dispatch_queue_t rwq;
  enum treatment_e treatment;
  int32_t test_data_member_api;
};

struct queryapi_state {
#define QAPI_STATE_MAGIC (0x0a0e0402)
  uint32_t magic;
  uint32_t exitnow;
  uint32_t member_startct;
  struct tenv1 *tenv;
  int include_nonmember;
  int ignore;
  uint32_t reg_ready_called;
  struct {
    /* A bunch of this is persistent client state across all ensemble
       tests and should move inside the API and/or into
       tenv->t[TASKLET_CLIENT].mumble */
    rwsched_dispatch_queue_t rwq;
    rwmsg_clichan_t *cc;
    rwmsg_destination_t *dest;
    RWDtsQueryRouter_Client querycli;
    rwmsg_request_t *rwreq;
    int rspct;
    bool noreqs;
    int transactional;
    RWDtsQueryAction action;
    int usebuilderapi;
    enum expect_result_e expect_result;
  } client;
  struct qapi_member member[TASKLET_MEMBER_CT];
  uint32_t expected_notification_count;
  uint32_t notification_count;
  uint32_t expected_advise_rsp_count;
};


static void rwdts_member_fill_response(RWPB_T_MSG(DtsTest_Person) *person, int i) {
  RWPB_F_MSG_INIT(DtsTest_Person,person);
  RW_ASSERT(i >= 0 && i <= 9);
  person->name = strdup("0-RspTestName");
  person->name[0] = '0' + i;
  person->has_id = TRUE;
  person->id = 2000 + i;
  person->email = strdup("0-rsp_test@test.com");
  person->email[0] = '0' + i;
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);
  person->phone[0]->number = strdup("089-123-456");
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_HOME;
  return;
}

static void
memberapi_test_dispatch_response(memberapi_test_t* mbr) {
  int num_responses = 10;
  int i;

  RW_ASSERT(mbr);
  rwdts_xact_t *xact = mbr->xact;
  RW_ASSERT(xact);
  rwdts_query_handle_t qhdl = mbr->qhdl;
  RWDtsQuery *query = (RWDtsQuery*)qhdl;
  RW_ASSERT(query);

  RWPB_T_MSG(DtsTest_Person) *person;
  ProtobufCMessage **array;
  RWPB_T_MSG(DtsTest_Person) *array_body;
  rwdts_member_query_rsp_t rsp;

  RW_ZERO_VARIABLE(&rsp);

  if (query->action != RWDTS_QUERY_READ) {
    num_responses = 1;
  }

  array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*) * num_responses);
  array_body = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)) * num_responses);

  RW_ASSERT(num_responses > 0);

  rsp.ks = mbr->key;

  fprintf(stderr, "%s async responding with %u responses, qidx[%u]\n", xact->apih->client_path, num_responses, query->queryidx);

  rsp.msgs = (ProtobufCMessage**)array;
  for (i = 0; i < num_responses; i++) {

    person = &array_body[i];

    array[i] = &(array_body[i].base);

    rwdts_member_fill_response(person, i);

    rsp.n_msgs++;
    rsp.evtrsp  = ((i+1) == num_responses)? RWDTS_EVTRSP_ACK:RWDTS_EVTRSP_ASYNC;
  }
  rw_status_t rs = rwdts_member_send_response(xact, qhdl, &rsp);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_FREE(array);
  RW_FREE(array_body);
  RW_FREE(mbr);
}

static rwdts_member_rsp_code_t
memberapi_test_prepare(const rwdts_xact_info_t* xact_info,
                       RWDtsQueryAction         action,
                       const rw_keyspec_path_t*      key,
                       const ProtobufCMessage*  msg,
                       uint32_t credits,
                       void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  RWPB_T_MSG(DtsTest_Person) *person;
  static RWPB_T_MSG(DtsTest_Person) person_instance = {};
  static ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;

  person = &person_instance;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member prepare ...\n");
  //array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
  //array[0] = &person->base;

  RW_ZERO_VARIABLE(&rsp);

  rsp.ks = (rw_keyspec_path_t*)key;

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:
  case TREATMENT_COMMIT_NACK:
  case TREATMENT_ABORT_NACK:
  case TREATMENT_PRECOMMIT_NACK:

    array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
    array[0] = &person->base;

    rwdts_member_fill_response(person, 0);

    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)array;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;

    if (s->ignore == 0) {
      //EXPECT_EQ(action, s->client.action);
    }
    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);

    return RWDTS_ACTION_OK;

  case TREATMENT_NA:
    return RWDTS_ACTION_NA;

  case TREATMENT_NACK:
    return RWDTS_ACTION_NOT_OK;

  case TREATMENT_ASYNC: {
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->key  = (rw_keyspec_path_t*)key;

    rwsched_dispatch_after_f(apih->tasklet,
                             10,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_response);
    return RWDTS_ACTION_ASYNC;
  }
  case TREATMENT_BNC:
    rsp.evtrsp  = RWDTS_EVTRSP_ASYNC;
    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
  // ATTN How to free array
}

static rwdts_member_rsp_code_t
memberapi_test_precommit(const rwdts_xact_info_t*      xact_info,
                         uint32_t                      n_crec,
                         const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member pre-commit ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
  case TREATMENT_PRECOMMIT_NACK:
  {
    rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
    HASH_ITER(hh, xact->queries, xquery, qtmp) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
      //rsp.ks = xquery->query->key;
      rwdts_member_send_response((rwdts_xact_t*)xact, xact_info->queryh, &rsp);
    }
    return RWDTS_ACTION_NOT_OK;
    break;
  }
  case TREATMENT_ACK:
  {
    return RWDTS_ACTION_OK;
  }

  default:
    return RWDTS_ACTION_NA;
    break;
  }
}

static rwdts_member_rsp_code_t
memberapi_test_commit(const rwdts_xact_info_t*      xact_info,
                      uint32_t                      n_crec,
                      const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member commit ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
  case TREATMENT_COMMIT_NACK:
  {
    rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
    HASH_ITER(hh, xact->queries, xquery, qtmp) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
      //rsp.ks = xquery->query->key;
      rwdts_member_send_response((rwdts_xact_t*)xact, xact_info->queryh, &rsp);
    }
    return RWDTS_ACTION_NOT_OK;
    break;
  }
  case TREATMENT_ACK:
  {
    return RWDTS_ACTION_OK;
  }

  default:
    return RWDTS_ACTION_NA;
    break;
  }
}

static void rwdts_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)  {
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {

    static RWPB_T_MSG(DtsTest_Person)  *person = NULL;

    switch (s->client.expect_result) {
    case XACT_RESULT_COMMIT:
      if (s->client.action == RWDTS_QUERY_READ) {
        uint32_t cnt;
        uint32_t expected_count = 0;

        for (int r = 0; r < TASKLET_MEMBER_CT; r++) {
          if (s->member[r].treatment == TREATMENT_ACK) {
            // When not Async we respond with 1 record
            expected_count += 1;
          } else if (s->member[r].treatment == TREATMENT_ASYNC)  {
            // When it is Async we respond with 10 records
            expected_count += 10;
          }
        }

        cnt = rwdts_xact_get_available_results_count(xact, NULL,1000);
        // EXPECT_EQ(cnt, expected_count);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,9998);
        // EXPECT_EQ(cnt, expected_count);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,0);
        EXPECT_EQ(cnt, 2*expected_count);
      } else {
        rwdts_query_result_t* qres = rwdts_xact_query_result(xact,0);
        if(s->ignore == 0) {
          ASSERT_TRUE(qres);
          person = ( RWPB_T_MSG(DtsTest_Person) *)qres->message;
          ASSERT_TRUE(person);
          EXPECT_STREQ(person->name, "0-RspTestName");
          EXPECT_TRUE(person->has_id);
          EXPECT_EQ(person->id, 2000);
          EXPECT_STREQ(person->email, "0-rsp_test@test.com");
          EXPECT_TRUE(person->has_employed);
          EXPECT_TRUE(person->employed);
          EXPECT_EQ(person->n_phone, 1);
          ASSERT_TRUE(person->phone);
          EXPECT_STREQ(person->phone[0]->number, "089-123-456");
          EXPECT_TRUE(person->phone[0]->has_type);
          EXPECT_EQ(person->phone[0]->type,DTS_TEST_PHONE_TYPE_HOME);
          protobuf_c_message_free_unpacked(NULL, &((person)->base));
          person = NULL;
        }
      }
      break;
    case XACT_RESULT_ABORT:
      //ASSERT_EQ(xact_status.status, RWDTS_XACT_ABORTED); // this is only so while we have a monolithic-responding router
      /* We don't much care about results from abort although there may be some */
      break;
    case XACT_RESULT_ERROR:
      //??
      ASSERT_TRUE((int)0);
      break;
    default:
      ASSERT_TRUE((int)0);
      break;
    }
  }

  // Call the transaction end
  if (xact_status->status != RWDTS_XACT_RUNNING) {
    s->exitnow = 1;
  }

  return;
}

static void memberapi_client_start_f(void *ctx) {

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  int i;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("MemberAPI test client start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rwdts_xact_t* xact = NULL;
  RWPB_T_MSG(DtsTest_Person) *person;

  uint64_t corrid = 1000;

  for ( i = 0; i < TASKLET_CT; i++)
  {
    if ((s->tenv->t[i].tasklet_idx == TASKLET_MEMBER_0) ||
        (s->tenv->t[i].tasklet_idx == TASKLET_MEMBER_1) ||
        (s->tenv->t[i].tasklet_idx == TASKLET_MEMBER_2))
    {
      rwdts_get_shard_db_info_sched(s->tenv->t[i].apih, (void *)RWDtsRedisEnsemble::rwdts_shard_info_cb);
    }
  }

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  rwdts_api_set_ypbc_schema(clnt_apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));

  RWPB_F_MSG_INIT(DtsTest_Person,person);

  person->name = strdup("TestName");
  person->has_id = TRUE;
  person->id = 1000;
  person->email = strdup("test@test.com");
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);
  person->phone[0]->number = strdup("123-456-789");
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_MOBILE;

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  rw_status_t rs = RW_STATUS_FAILURE;

  if (!s->client.usebuilderapi) {
    flags |= RWDTS_XACT_FLAG_END;
    rs = rwdts_advise_query_proto_int(clnt_apih,
                                      keyspec,
                                      NULL, // xact_parent
                                      &(person->base),
                                      corrid,
                                      NULL, // Debug
                                      RWDTS_QUERY_UPDATE,
                                      &clnt_cb,
                                      flags,
                                      NULL, NULL);
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  } else {
    uint64_t corrid = 999;

    xact = rwdts_api_xact_create(clnt_apih, flags, rwdts_clnt_query_callback, ctx);
    ASSERT_TRUE(xact);
    rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
    ASSERT_TRUE(blk);
    rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
                      flags,corrid,&(person->base));
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    RW_FREE(person->name);
    person->name = NULL;
    person->name = strdup("TestName123");
    RW_FREE(person->email);
    person->email = NULL;
    person->email = strdup("TestName123@test.com");
    uint64_t corrid1 = 9998;

    rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
                                       flags,corrid1,&(person->base));

    ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

    rs = rwdts_xact_commit(xact);

    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }
  protobuf_c_message_free_unpacked(NULL, &((person)->base));

  return;
}

static void rwdts_set_exit_now(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (apih->stats.num_member_advise_rsp <  s->expected_advise_rsp_count) {
    //fprintf(stderr, "Pending responses current[%u], expected[%u]\n",
            // apih->stats.num_member_advise_rsp, s->expected_advise_rsp_count);
    rwsched_dispatch_after_f(apih->tasklet,
                             5 * NSEC_PER_SEC,
                             apih->client.rwq,
                             ctx,
                             rwdts_set_exit_now);
  } else {
    //RW_ASSERT(s->expected_notification_count == s->notification_count);
    fprintf(stderr, "Exiting the  pub sub test --- expected notification count[%u] == received notification count[%u]\n",
            s->expected_notification_count, s->notification_count);
    s->exitnow = 1;
  }
  return;
}
static rwdts_member_rsp_code_t
memberapi_receive_pub_notification(const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,
                                   uint32_t credits,
                                   void *getnext_ptr)
{
  RW_ASSERT(xact_info);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->notification_count++;

  TSTPRN("Received notification from member, count  = %u\n", s->notification_count);

  return RWDTS_ACTION_OK;
}

static void memberapi_pub_sub_db_client_start_f(void *ctx) {
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  //cb.ud = ctx;
  //cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub DB sub notify ...\n");

  rw_keyspec_path_t *keyspec;
  rwdts_shard_info_detail_t *shard_key1;

  char str[15] = "banana";

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (uint8_t *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);
  shard_key1->shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;
  rw_keyspec_path_t *lks = NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);

  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_member_register(NULL, clnt_apih,
                              lks,
                              &cb,
                              RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                              RWDTS_FLAG_PUBLISHER | RWDTS_FLAG_DATASTORE | RWDTS_FLAG_CACHE, shard_key1);
  rw_keyspec_path_free(lks, NULL);

  {
    //Create an element
    RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

    char phone_num1[] = "123-456-789";

    rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
    kpe.has_key00 = 1;
    kpe.key00.number= phone_num1;

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);
    rw_keyspec_path_t *get_keyspec;

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_status_t rs = rwdts_member_reg_handle_create_element(
                                regh,gkpe,&phone->base,NULL);

    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the created element
    //

    rs = rwdts_member_reg_handle_get_element(regh, gkpe, NULL, &get_keyspec, (const ProtobufCMessage**)&tmp);
    ASSERT_EQ(RW_STATUS_SUCCESS, rs);
    ASSERT_TRUE(tmp);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    // Update the list element
    //

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;
    rs = rwdts_member_reg_handle_update_element(regh,
                                               gkpe,
                                               &phone->base,
                                               RWDTS_XACT_FLAG_REPLACE,
                                               NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the updated element and verify the update
    //

    rs = rwdts_member_reg_handle_get_element(regh, gkpe, NULL, &get_keyspec, (const ProtobufCMessage**)&tmp);
    ASSERT_EQ(RW_STATUS_SUCCESS, rs);
    ASSERT_TRUE(tmp);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    //  Get the current list count
    //

    uint32_t count = rwdts_member_data_get_count(NULL, regh);
    EXPECT_EQ(1, count);

    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_reg_handle_delete_element(regh,
                                               gkpe,
                                               &phone->base,
                                               NULL);

    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // DO a get again
    //

    rs = rwdts_member_reg_handle_get_element(regh, gkpe, NULL, &get_keyspec, (const ProtobufCMessage**)&tmp);
    ASSERT_NE(RW_STATUS_SUCCESS, rs);
    EXPECT_FALSE(tmp);
  }

  { // Async  version tests
    //Create an element
    RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

    char phone_num1[] = "978-456-789";
    rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
    kpe.has_key00 = 1;
    kpe.key00.number= phone_num1;

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_status_t rs = rwdts_member_reg_handle_create_element(regh,gkpe,
                                                  &phone->base,NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Update the list element
    //

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;

    rs = rwdts_member_reg_handle_update_element(regh,
                                               gkpe,
                                               &phone->base,
                                               RWDTS_XACT_FLAG_REPLACE,
                                               NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);


    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_reg_handle_delete_element(regh,
                                               gkpe,
                                               &phone->base,
                                               NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  }

  // The test would be done when all the members have received the publish callback
  // Check for completiom
  rwsched_dispatch_async_f(clnt_apih->tasklet,
                           clnt_apih->client.rwq,
                           ctx,
                           rwdts_set_exit_now);
}

rwdts_kv_light_reply_status_t
RWDtsRedisEnsemble::rwdts_kv_light_set_callback(rwdts_kv_light_set_status_t status,
                            int serial_num, void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t*)userdata;

  EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);
  tenv.next_serial_number[tab_handle->db_num] = serial_num;

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

void
RWDtsRedisEnsemble::rwdts_test_registration_cb(RWDtsRegisterRsp*    rsp,
                           rwmsg_request_t*     rwreq,
                           void*                ud)
{
  RW_ASSERT(rsp);
  rwdts_api_t *apih = (rwdts_api_t*)ud;
  uint32_t i, db_number;

  for(i=0; i< rsp->n_responses; i++) {
    if (rsp->responses[i]->has_db_number && rsp->responses[i]->shard_chunk_id) {
      db_number = rsp->responses[i]->db_number;
      if (tenv.redis_up == 1) {
        if (tenv.tab_handle[db_number] == NULL) {
          tenv.tab_handle[db_number] = rwdts_kv_light_register_table(tenv.handle, db_number);
          RW_ASSERT(tenv.tab_handle[db_number]);
          rwdts_kv_light_table_insert(tenv.tab_handle[db_number], tenv.next_serial_number[db_number], (char *)rsp->responses[i]->shard_chunk_id,
                                      (void *)RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone),
                                      sizeof(RWPB_T_PATHSPEC(DtsTest_data_Person_Phone)),
                                      (void *)RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone),
                                      sizeof(RWPB_T_PATHENTRY(DtsTest_data_Person_Phone)),
                                      (void *)RWDtsRedisEnsemble::rwdts_kv_light_set_callback, (void *)tenv.tab_handle[db_number]);
          tenv.set_count++;
            rwdts_get_shard_db_info_sched(apih, (void *)RWDtsRedisEnsemble::rwdts_shard_info_cb);
        }
       }
    }
  }

  fprintf(stderr, "[%s] TEST successfully registered with the router\n", apih->client_path);

  return;
}

void
RWDtsRedisEnsemble::rwdts_kv_light_get_shard_callback(void **val,
                                                 int *val_len,
                                                 void **key,
                                                 int *key_len,
                                                 int *serial_num,
                                                 int total,
                                                 void *userdata,
                                                 void *next_block)
{
  if (total != 0) {
    //#error this key_len stuff won't really work, as it is the size of the keyspec, which is opaque
    // this will be better with minikey support and the key can be non-opaque or at least semi-non-opaque
    EXPECT_TRUE(   ((key_len[0] == 6) && (val_len[0] == 28))
                || ((key_len[0] == 18) && (val_len[0] == 68))
                || ((key_len[0] == 30) && (val_len[0] == 68))
                || ((key_len[0] == 63) && (val_len[0] == 21))
                || ((key_len[0] == 51) && (val_len[0] == 21)));
  }
  fprintf(stderr, "Received key value from Redis for shard %d %d\n", (total?key_len[0]:0), (total?val_len[0]:0));
  return;
}

void
RWDtsRedisEnsemble::rwdts_shard_info_cb(RWDtsGetDbShardInfoRsp*    rsp,
                                   rwmsg_request_t*     rwreq,
                                   void*                ud)
{
  RW_ASSERT(rsp);
  rwdts_api_t *apih = (rwdts_api_t*)ud;
  uint32_t i, db_number;

  if (tenv.redis_up == 1) {
    for (i = 0; i < rsp->n_responses; i++) {
      rwdts_kv_table_handle_t *kv_tab_handle = NULL;
      rw_status_t status;
      db_number = rsp->responses[i]->db_number;
      //shard_chunk_id = rsp->responses[i]->shard_chunk_id;
      status = RW_SKLIST_LOOKUP_BY_KEY(&(apih->kv_table_handle_list), &db_number,
                                       (void *)&kv_tab_handle);
      EXPECT_TRUE(status == RW_STATUS_SUCCESS);
      if (kv_tab_handle != NULL) {
        rwdts_kv_light_get_next_fields(kv_tab_handle,
                                       rsp->responses[i]->shard_chunk_id,
                                       (void *)RWDtsRedisEnsemble::rwdts_kv_light_get_shard_callback,
                                       (void *)apih, NULL);
      }
    }
  }
  return;
}

static void
memberapi_test_regready(rwdts_member_reg_handle_t  regh,
                        rw_status_t                rs,
                        void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_start_f);
  }
  return;
}

static void memberapi_member_start_multi_xact_f(void *ctx) {

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;
  rwdts_shard_info_detail_t *shard_key1;

  char str[15] = "banana";

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = memberapi_test_regready;
  cb.cb.prepare = memberapi_test_prepare;
  cb.cb.precommit = memberapi_test_precommit;
  cb.cb.commit = memberapi_test_commit;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (uint8_t *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);
  shard_key1->shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;

  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_Person),
                        RWDTS_FLAG_DATASTORE | RWDTS_FLAG_SUBSCRIBER, shard_key1);

  TSTPRN("Member start_multi_xact_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_start_multi_xact_nack_f(void *ctx) {

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;
  rwdts_shard_info_detail_t *shard_key1;

  char str[15] = "banana";

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = memberapi_test_regready;
  cb.cb.prepare = memberapi_test_prepare;
  cb.cb.precommit = memberapi_test_precommit;
  cb.cb.commit = memberapi_test_commit;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (uint8_t *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);
  shard_key1->shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;

  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_Person),
                        RWDTS_FLAG_DATASTORE | RWDTS_FLAG_SUBSCRIBER, shard_key1);

  TSTPRN("Member start_multi_xact_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_pub_sub_db_member_start_f(void *ctx) {

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;
  rwdts_shard_info_detail_t *shard_key1;

  char str[15] = "banana";

  shard_key1 = RW_MALLOC0_TYPE(sizeof(rwdts_shard_info_detail_t), rwdts_shard_info_detail_t);
  shard_key1->shard_key_detail.u.byte_key.k.key = (uint8_t *)RW_MALLOC(sizeof(str));
  memcpy((char *)shard_key1->shard_key_detail.u.byte_key.k.key, &str[0], strlen(str));
  shard_key1->shard_key_detail.u.byte_key.k.key_len = strlen(str);
  shard_key1->shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub DB Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;
  rw_keyspec_path_t *lks = NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, lks, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                        RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_DATASTORE, shard_key1);

  rw_keyspec_path_free(lks, NULL);
  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_db_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_db_client_start_f);
  }
  TSTPRN("Member member API pub sub db member start ending , startct=%u\n", s->member_startct);
}

static void RedisReady(struct tenv1 *tenv, rw_status_t status)
{
  int i;
  tenv->redis_instance = (rwdts_redis_instance *)tenv->handle->kv_conn_instance;
  ASSERT_TRUE(tenv->redis_instance);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  tenv->redis_up = 1;
  for ( i = 0; i < TASKLET_CT; i++)
  {
    if ((tenv->t[i].tasklet_idx == TASKLET_MEMBER_0) ||
        (tenv->t[i].tasklet_idx == TASKLET_MEMBER_1) ||
        (tenv->t[i].tasklet_idx == TASKLET_MEMBER_2) ||
        (tenv->t[i].tasklet_idx == TASKLET_CLIENT))
    {
      tenv->t[i].apih->db_up = 1;
    }
  }
}

static void memberapi_test_multi_xact_execute(struct queryapi_state *s) {
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  s->reg_ready_called = 0;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_multi_xact_f);
  }
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, 30, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_multi_xact_nack_execute(struct queryapi_state *s) {
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  s->reg_ready_called = 0;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_multi_xact_nack_f);
  }
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, 30, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_pub_sub_db_notification(struct queryapi_state *s) {
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_db_member_start_f);
  }
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, 30, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_callback(void *val, int val_len, void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_all_callback(void **key,
                                                                     int *keylen,
                                                                     void **value,
                                                                     int *val_len,
                                                                     int total,
                                                                     void *userdata);

static rwdts_kv_light_reply_status_t rwdts_kv_light_set_foo_callback(rwdts_kv_light_set_status_t status, int serial_num, void *userdata)
{
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);
   rwdts_kv_light_table_get_by_key(tab_handle, (char *)"FOO", 3, (void *)rwdts_kv_light_get_callback, (void *)tab_handle);
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_del_callback(rwdts_kv_light_del_status_t status, void *userdata)
{
    rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
    EXPECT_TRUE(RWDTS_KV_LIGHT_DEL_SUCCESS == status);
    rwdts_kv_light_deregister_table(tab_handle);
    return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_tab_exist(int exists,
                                                              void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  EXPECT_TRUE(exists == 1);
  rwdts_kv_light_table_delete_key(tab_handle, 2, (char *)"What", (char *)"FOO", 3, (void *)rwdts_kv_light_del_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_ser_get_callback(void *val, int val_len,
                                                                     int serial_num, void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  EXPECT_TRUE(serial_num == 2);
  rwdts_kv_light_tab_field_exists(tab_handle, (char *)"FOO", 3, (void *)rwdts_kv_light_tab_exist, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_callback(void *val, int val_len, void *userdata)
{
   struct abc *reply = (struct abc*)val;
   rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
   EXPECT_TRUE((reply->a == a.a) && (reply->b == a.b) && (reply->c == a.c));
   rwdts_kv_light_table_get_sernum_by_key(tab_handle, (char *)"FOO", 3, (void *) rwdts_kv_light_ser_get_callback, (void *)tab_handle);
   return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_delete_shard_cbk_fn(rwdts_kv_light_del_status_t status,
                                                                        void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;

  EXPECT_TRUE(RWDTS_KV_LIGHT_DEL_SUCCESS == status);
  if(RWDTS_KV_LIGHT_DEL_SUCCESS == status) {
    //fprintf(stderr, "Successfully deleted the data\n");
  }
  shard_delete = 1;
  rwdts_kv_handle_get_all(tab_handle->handle, tab_handle->db_num, (void *)rwdts_kv_light_get_all_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_final_get_all_callback(void **key,
                                                                           int *keylen,
                                                                           void **value,
                                                                           int *val_len,
                                                                           int total,
                                                                           void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  EXPECT_TRUE(total == 0);
  if (total == 0) {
    //fprintf(stderr, "Successfully deleted the entire table\n");
  }
  rwdts_kv_light_deregister_table(tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_delete_tab_cbk_fn(rwdts_kv_light_del_status_t status,
                                                                        void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  EXPECT_TRUE(RWDTS_KV_LIGHT_DEL_SUCCESS == status);

  if(RWDTS_KV_LIGHT_DEL_SUCCESS == status) {
    //fprintf(stderr, "Successfully deleted the data\n");
  }
  rwdts_kv_handle_get_all(tab_handle->handle, tab_handle->db_num, (void *) rwdts_kv_light_final_get_all_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_all_callback(void **key,
                                                                     int *keylen,
                                                                     void **value,
                                                                     int *val_len,
                                                                     int total,
                                                                     void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  //int i = 0;

  if (shard_delete == 0) {
    EXPECT_TRUE(total == 19);
    //fprintf(stderr, "Total received is %d\n", total);
  } else {
    EXPECT_TRUE(total == 5);
    //fprintf(stderr, "Total received is %d\n", total);
  }

 /*
  for (i = 0; i < total; i++) {
    fprintf(stderr, " Get all %s\n", (char *)value[i]);
    fprintf(stderr, " Get all %s\n", (char *)key[i]);
  }
  */


  if (shard_delete == 0) {
    rwdts_kv_light_delete_shard_entries(tab_handle, (char *)"What",
                                       (void *)rwdts_kv_light_delete_shard_cbk_fn,
                                       (void *)tab_handle);
  } else {
    rwdts_kv_light_del_table(tab_handle, (void *)rwdts_kv_light_delete_tab_cbk_fn, (void *)tab_handle);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_get_next_callback(void **val,
                                                                      int *val_len,
                                                                      void **key,
                                                                      int *key_len,
                                                                      int *serial_num,
                                                                      int total,
                                                                      void *userdata,
                                                                      void *next_block)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  //int i = 0;

  EXPECT_TRUE(total == 14);

/*
  for (i = 0; i < total; i++) {
    fprintf(stderr, " Get next %s\n", (char *)val[i]);
    fprintf(stderr, " Get next %s\n", (char *)key[i]);
  }
*/
  rwdts_kv_handle_get_all(tab_handle->handle, tab_handle->db_num, (void *)rwdts_kv_light_get_all_callback, (void *)tab_handle);
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_shard_set_callback(rwdts_kv_light_set_status_t status,
                                                                      int serial_num, void *userdata)
{
  rwdts_kv_table_handle_t *tab_handle = (rwdts_kv_table_handle_t *)userdata;
  RW_ASSERT(serial_num == 2);
  RW_ASSERT(RWDTS_KV_LIGHT_SET_SUCCESS == status);
  EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);
  EXPECT_TRUE(serial_num == 2);

  if(RWDTS_KV_LIGHT_SET_SUCCESS == status) {
    //fprintf(stderr, "Successfully inserted the data\n");
  }

  set_count++;
  if (set_count == 19) {
    rwdts_kv_light_get_next_fields(tab_handle, (char *)"What", (void *)rwdts_kv_light_get_next_callback,
                                   (void*)tab_handle, NULL);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

TEST_F(RWDtsRedisEnsemble, MemberAPITransactionalMULTIxact) {
  struct queryapi_state s = { 0 };
  s.magic = QAPI_STATE_MAGIC;
  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  s.client.transactional = TRUE;
  s.client.action =  RWDTS_QUERY_UPDATE;
  s.ignore = 1;
  s.member[0].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  memberapi_test_multi_xact_execute(&s);
}

TEST_F(RWDtsRedisEnsemble, MemberAPITransactionalMULTIxactNack) {
  struct queryapi_state s = { 0 };
  s.magic = QAPI_STATE_MAGIC;
  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  s.client.transactional = TRUE;
  s.client.action =  RWDTS_QUERY_UPDATE;
  s.ignore = 1;
  s.member[0].treatment = TREATMENT_NACK;
  s.client.expect_result = XACT_RESULT_ABORT;
  memberapi_test_multi_xact_nack_execute(&s);
}

TEST_F(RWDtsRedisEnsemble, DataMemberAPIPubSubDBNotification) {
  struct queryapi_state s = { 0 };
  s.magic = QAPI_STATE_MAGIC;
  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  s.client.transactional = FALSE;
  s.client.action =  RWDTS_QUERY_CREATE;
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 18; // 1 set sync, 1 set async
  s.expected_advise_rsp_count = 6;
  s.notification_count = 0;
  memberapi_test_pub_sub_db_notification(&s);
}

TEST_F(RWDtsRedisEnsemble, DISABLED_InitRedis)
{
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, &tenv.redis_up);

  ASSERT_TRUE(tenv.redis_up); /*connection should be up at this point */
  a.a = 1;
  a.b = 2;
  a.c = 3;

  tenv.kv_tab_handle = rwdts_kv_light_register_table(tenv.handle, 9);
  rwdts_kv_light_table_insert(tenv.kv_tab_handle, 1, (char *)"What", (char*)"FOO", 3,
                              (void*)&a,
                              sizeof(a),
                              (void *)rwdts_kv_light_set_foo_callback, (void*)tenv.kv_tab_handle);

  rwsched_dispatch_main_until(tenv.redis_tasklet, 2/*s*/, NULL);
}

TEST_F(RWDtsRedisEnsemble, DISABLED_InitRedisShard)
{
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, &tenv.redis_up);
  int i;
  char shard_num[100] = "What";

  ASSERT_TRUE(tenv.redis_up); /*connection should be up at this point */

  set_count = 0;
  shard_delete = 0;

  tenv.kv_tab_handle = rwdts_kv_light_register_table(tenv.handle, 5);

  for (i = 0; i< 19; i++) {
    if (i > 13) {
      strcpy(shard_num, "When");
    }
    rwdts_kv_light_table_insert(tenv.kv_tab_handle, (i+1), shard_num, (void *)key_entry[i],
                                strlen(key_entry[i]), (void *)tab_entry[i], strlen(tab_entry[i]),
                                (void *)rwdts_kv_light_shard_set_callback, (void*)tenv.kv_tab_handle);
  }
  rwsched_dispatch_main_until(tenv.redis_tasklet, 2/*s*/, NULL);
}

TEST_F(RWDtsRedisEnsemble, DISABLED_InitRedisXact)
{
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, &tenv.redis_up);

  ASSERT_TRUE(tenv.redis_up); /*connection should be up at this point */
  a.a = 1;
  a.b = 2;
  a.c = 3;

  tenv.kv_tab_handle = rwdts_kv_light_register_table(tenv.handle, 8);

  set_aborted = 0;
  del_aborted = 0;
  set_committed = 0;
  del_committed = 0;

  rwdts_kv_light_table_xact_insert(tenv.kv_tab_handle, 1, (char *)"What", (char*)"FOO", 3,
                                   (void*)&a, sizeof(a),
                                   (void *)rwdts_kv_light_set_xact_callback, (void*)tenv.kv_tab_handle);

  rwsched_dispatch_main_until(tenv.redis_tasklet, 2/*s*/, NULL);
}

static rwdts_kv_light_reply_status_t rwdts_kv_handle_final_get_all_cb(void **key,
                                                                      int *key_len,
                                                                      void **val,
                                                                      int *val_len,
                                                                      int total,
                                                                      void *userdata)
{
  int i;
  char res_val[50];
  char res_key[50];
  EXPECT_TRUE(total == 14);

  for(i=0; i < total; i++) {
    memcpy(res_val, (char *)val[i], val_len[i]);
    res_val[val_len[i]] = '\0';
    memcpy(res_key, (char *)key[i], key_len[i]);
    res_key[key_len[i]] = '\0';
    //fprintf(stderr, "res_key = %s, res_val = %s\n", res_val, res_key);
    free(key[i]);
    free(val[i]);
  }
  free(key);
  free(val);
  free(key_len);
  free(val_len);

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t
rwdts_kv_handle_del_shash_cb(rwdts_kv_light_del_status_t status,
                             void *userdata)
{
  rwdts_kv_handle_t *handle = (rwdts_kv_handle_t *)userdata;

  RW_ASSERT(RWDTS_KV_LIGHT_DEL_SUCCESS == status);
  del_count++;
  if (del_count == 5) {
    rwdts_kv_handle_get_all(handle, 7, (void *)rwdts_kv_handle_final_get_all_cb,
                                   (void*)handle);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_handle_get_all_shash_cb(void **key,
                                                                      int *key_len,
                                                                      void **val,
                                                                      int *val_len,
                                                                      int total,
                                                                      void *userdata)
{
  rwdts_kv_handle_t *handle = (rwdts_kv_handle_t *)userdata;

  int i;
  char res_val[50];
  char res_key[50];
  rw_status_t res;
  EXPECT_TRUE(total == 19);

  for(i=0; i < total; i++) {
    memcpy(res_val, (char *)val[i], val_len[i]);
    res_val[val_len[i]] = '\0';
    memcpy(res_key, (char *)key[i], key_len[i]);
    res_key[key_len[i]] = '\0';
    //fprintf(stderr, "res_key = %s, res_val = %s\n", res_val, res_key);
    free(key[i]);
    free(val[i]);
  }
  free(key);
  free(val);
  free(key_len);
  free(val_len);

  for (i=0; i < 5; i++) {
    res = rwdts_kv_handle_del_keyval(handle, 7, (char *)key_entry[i],
                                     strlen(key_entry[i]), (void *)rwdts_kv_handle_del_shash_cb, handle);
    EXPECT_EQ(res, RW_STATUS_SUCCESS);
  }

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

static rwdts_kv_light_reply_status_t rwdts_kv_light_shash_set_callback(rwdts_kv_light_set_status_t status,
                                                                       int serial_num, void *userdata)
{
  rwdts_kv_handle_t *handle = (rwdts_kv_handle_t *)userdata;
  EXPECT_TRUE(RWDTS_KV_LIGHT_SET_SUCCESS == status);

  if(RWDTS_KV_LIGHT_SET_SUCCESS == status) {
    //fprintf(stderr, "Successfully inserted the data\n");
  }

  set_count++;
  if (set_count == 19) {
    rwdts_kv_handle_get_all(handle, 7, (void *)rwdts_kv_handle_get_all_shash_cb,
                                   (void*)handle);
  }
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

TEST_F(RWDtsRedisEnsemble, RedisDBTest)
{
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, &tenv.redis_up);
  int i;

  ASSERT_TRUE(tenv.redis_up); /*connection should be up at this point */

  set_count = 0;
  del_count = 0;

  for (i = 0; i< 19; i++) {
    rwdts_kv_handle_add_keyval(tenv.handle, 7, (void *)key_entry[i], strlen(key_entry[i]),
                               (void *)tab_entry[i], strlen(tab_entry[i]),
                               (void *)rwdts_kv_light_shash_set_callback, (void*)tenv.handle);
  }
  rwsched_dispatch_main_until(tenv.redis_tasklet, 5/*s*/, NULL);
}

RWUT_INIT();
