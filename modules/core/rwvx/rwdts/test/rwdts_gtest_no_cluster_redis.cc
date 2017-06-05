/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwdts_gtest_no_cluster_redis.cc
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

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

struct tenv1 {
  rwsched_instance_ptr_t rwsched;

  //Redis related
  char *rserver_path;
  char *redis_conf;
  rwdts_redis_instance_t *redis_instance;
  rwdts_redis_instance_t *sec_redis_instance;
  rwdts_kv_handle_t *handle;
  rwdts_kv_handle_t *sec_handle;
  rwsched_tasklet_ptr_t redis_tasklet;
  pid_t pid;
  pid_t sec_pid;
  bool init;
  uint32_t redis_up;
  uint32_t srvr_cnt;
  bool child_killed;
  int set_count;
  char redis_port[16];
  char redis_sec_port[16];
  uint16_t int_port;
  uint16_t int_sec_port;
  uint32_t sec_attempt;
};
class RWDtsRedisNCEnsemble : public ::testing::Test {
protected:
  static void SetUpTestCase() {
    char *rw_install_dir;
    rw_status_t status;
    tenv.rwsched = rwsched_instance_new();
    ASSERT_TRUE(tenv.rwsched);

    if (!tenv.rwsched->rwlog_instance)
      tenv.rwsched->rwlog_instance = rwlog_init("RW.Sched");

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
    tenv.int_port = redisport;
    sprintf(tenv.redis_sec_port, "%d", (redisport+1));
    tenv.int_sec_port = (redisport+1);

    signal(SIGCHLD,signalHandler);
    tenv.handle = (rwdts_kv_handle_t *)rwdts_kv_allocate_handle(REDIS_DB);
    tenv.sec_handle = (rwdts_kv_handle_t *)rwdts_kv_allocate_handle(REDIS_DB);

    tenv.redis_tasklet = rwsched_tasklet_new(tenv.rwsched);
    ASSERT_TRUE(tenv.redis_tasklet);

    tenv.redis_instance = NULL;
    tenv.redis_up = 0;
    tenv.set_count = 0;
    tenv.child_killed = 0;
    rw_install_dir = getenv("RIFT_INSTALL");
    tenv.rserver_path = RW_STRDUP_PRINTF("/usr/bin/redis-server"); 
    tenv.redis_conf = RW_STRDUP_PRINTF("%s/usr/bin/redis.conf", rw_install_dir);

    /* Invoke below Log API to initialise shared memory */
    rwlog_init_bootstrap_filters(NULL);

    tenv.pid = 0;
    if (!getenv("RWDTS_GTEST_NO_REDIS")) {
      tenv.pid = fork();

      if (!tenv.pid) { /*child*/
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        std::string num_nodes = std::to_string(3);
        char const *const argv[] = {
          (char *)tenv.rserver_path,
          tenv.redis_conf,
          "--port ",
          (char *)tenv.redis_port,
          NULL
          };
        execvp(argv[0], (char **)argv);
        fprintf(stderr, "execv BIZZARE error(%s)\n", strerror(errno));
        return; /*BIZZARE EXECV ERROR*/
      }
      tenv.init = 1;
      sleep(10); /*ugly, sleep till redis is ready */
      startClient();
    }

    if (!getenv("RWDTS_GTEST_NO_REDIS")) {
      tenv.sec_pid = fork();

      if (!tenv.sec_pid) { /*child*/
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        std::string num_nodes = std::to_string(3);
        char const *const argv[] = {
          (char *)tenv.rserver_path,
          tenv.redis_conf,
          "--port ",
          (char *)tenv.redis_sec_port,
          NULL
          };
        execvp(argv[0], (char **)argv);
        fprintf(stderr, "execv BIZZARE error(%s)\n", strerror(errno));
        return; /*BIZZARE EXECV ERROR*/
      }
      tenv.init = 1;
      //sleep(10); /*ugly, sleep till redis is ready */
      startSecClient();
    }

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

    if (tenv.sec_pid) {
      fprintf(stderr, "tearing down and killing second child\n");
      tenv.child_killed = 1;
      kill(tenv.pid, SIGINT);
      tenv.init = 0;
      tenv.redis_up = 0;
      sleep(5); /*wait for redis-server to cleanup resources */

      stopSecClient();
    }

    ASSERT_TRUE(tenv.rwsched);
    rwsched_instance_free(tenv.rwsched);
    tenv.rwsched = NULL;
    //sleep(5);
  }

public:
  static struct tenv1 tenv;

  static void startClient(void);
  static void startSecClient(void);
  static void stopClient(void);
  static void stopSecClient(void);

};

struct tenv1 RWDtsRedisNCEnsemble::tenv;

static void RedisReady(struct tenv1 *tenv, rw_status_t status);
static void RedisSecReady(struct tenv1 *tenv, rw_status_t status);

void RWDtsRedisNCEnsemble::startClient(void)
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

void RWDtsRedisNCEnsemble::startSecClient(void)
{
  char address_port[20] = "127.0.0.1:";
  strcat(address_port, tenv.redis_sec_port);
  rw_status_t status = rwdts_kv_handle_db_connect(tenv.sec_handle, tenv.rwsched, tenv.redis_tasklet,
                                                  address_port,
                                                  "test", NULL,
                                                  (void *)RedisSecReady,
                                                  (void *)&tenv);
  ASSERT_TRUE(RW_STATUS_SUCCESS == status);
}

void RWDtsRedisNCEnsemble::stopClient(void)
{
  if (tenv.redis_instance) {
    rwdts_redis_instance_deinit(tenv.redis_instance); //close all the connections
    tenv.handle->kv_conn_instance = NULL;
  }
}

void RWDtsRedisNCEnsemble::stopSecClient(void)
{
  if (tenv.sec_redis_instance) {
    rwdts_redis_instance_deinit(tenv.sec_redis_instance); //close all the connections
    tenv.sec_handle->kv_conn_instance = NULL;
  }
}

static void RedisReady(struct tenv1 *tenv, rw_status_t status)
{
  tenv->redis_instance = (rwdts_redis_instance *)tenv->handle->kv_conn_instance;
  ASSERT_TRUE(tenv->redis_instance);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  tenv->srvr_cnt++;
  if (tenv->srvr_cnt == 2) {
    tenv->redis_up = 1;
  }
}

static void RedisSecReady(struct tenv1 *tenv, rw_status_t status)
{
  tenv->sec_redis_instance = (rwdts_redis_instance *)tenv->sec_handle->kv_conn_instance;
  ASSERT_TRUE(tenv->sec_redis_instance);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  tenv->srvr_cnt++;
  if (tenv->srvr_cnt == 2) {
    tenv->redis_up = 1;
  }
}

char key[20] = "testkey";
char val[20] = "testval";
char del_key[20] = "delkey";
char del_val[20] = "delval";

rwdts_kv_light_reply_status_t
rwdts_redis_get_all_cb(void **key, int *key_len, void **val,
                       int *val_len, int total, void *callbk_data);

static rwdts_kv_light_reply_status_t
rwdts_redis_del_callback(rwdts_kv_light_del_status_t status,
                         void *userdata)
{
  struct tenv1 *tenv = (struct tenv1 *)userdata;

  if(RWDTS_KV_LIGHT_DEL_SUCCESS == status) {
    fprintf(stderr, "Successfully deleted the data\n");
  }

  tenv->sec_attempt = 1;
  rwdts_kv_handle_get_all(tenv->handle, 0, (void *)rwdts_redis_get_all_cb, (void *)tenv);  
  return RWDTS_KV_LIGHT_REPLY_DONE;
}

rwdts_kv_light_reply_status_t
rwdts_redis_get_all_cb(void **key, int *key_len, void **val,
                       int *val_len, int total, void *callbk_data)
{
  struct tenv1 *tenv = (struct tenv1 *)callbk_data;

  int i;
  char res_val[50];
  char res_key[50];
  for(i=0; i < total; i++) {
    memcpy(res_val, (char *)val[i], val_len[i]);
    res_val[val_len[i]] = '\0';
    memcpy(res_key, (char *)key[i], key_len[i]);
    res_key[key_len[i]] = '\0';
    fprintf(stderr, "res_key = %s, res_val = %s\n", res_key, res_val);
    free(key[i]);
    free(val[i]);
  }
  free(key);
  free(val);

  if (!tenv->sec_attempt) {
    EXPECT_EQ(total, 2);
    rwdts_kv_handle_del_keyval(tenv->handle, 0, del_key, strlen(del_key),
                               (void *)rwdts_redis_del_callback, (void *)tenv);
  } else {
    EXPECT_EQ(total, 1);
  }

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

rwdts_reply_status 
rwdts_redis_s_to_m_cb(rwdts_redis_msg_handle*, void *userdata)
{
  struct tenv1 *tenv = (struct tenv1 *)userdata;
  rwdts_kv_handle_get_all(tenv->handle, 0, (void *)rwdts_redis_get_all_cb, (void *)tenv);
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status 
rwdts_redis_m_to_s_cb(rwdts_redis_msg_handle*, void *userdata)
{
  struct tenv1 *tenv = (struct tenv1 *)userdata;

  rwsched_dispatch_main_until(tenv->redis_tasklet, 6/*s*/, NULL);
  rwdts_redis_slave_to_master((rwdts_redis_instance_t *)tenv->handle->kv_conn_instance, 
                              rwdts_redis_s_to_m_cb, (void *)tenv);

  return RWDTS_REDIS_REPLY_DONE;
}

TEST_F(RWDtsRedisNCEnsemble, TestMasterSlave)
{
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, &tenv.redis_up);

  ASSERT_TRUE(tenv.redis_up); /*connection should be up at this point */

  rw_status_t status;
  
  status = rwdts_kv_handle_add_keyval(tenv.sec_handle, 0, key, strlen(key), val, strlen(val), 
                                      NULL, NULL);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwdts_kv_handle_add_keyval(tenv.sec_handle, 0, del_key, strlen(del_key), del_val, strlen(del_val),
                                      NULL, NULL);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  rwdts_redis_master_to_slave((rwdts_redis_instance_t *)tenv.handle->kv_conn_instance,
                              (char *)"127.0.0.1", tenv.int_sec_port, rwdts_redis_m_to_s_cb, (void *)&tenv);
  rwsched_dispatch_main_until(tenv.redis_tasklet, 10/*s*/, NULL);
}

RWUT_INIT();
