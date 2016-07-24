
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


#include <sys/prctl.h>

#include <rwut.h>
#include <rwvcs.h>
#include <rwsched/cfrunloop.h>

#include "serf_client.h"


static serf_context_ptr_t start_serf(rwvcs_instance_ptr_t rwvcs, pid_t * pid)
{
  rw_status_t status;
  int r;
  uint16_t bind_port;
  uint16_t rpc_port;
  char * install_dir;
  char * serf_path;
  char * bind_arg;
  char * rpc_arg;
  serf_context_ptr_t serf;
  pid_t lpid;

  // Fucking c++
  char agent[] = "agent";
  char encrypt[] = "-encrypt=1FzgH8LsTtr0Wopn4934OQ==";
  char log[] = "-log-level=err";
  char node[] = "-node=serf-gtest";
  char localhost[] = "127.0.0.1";


  status = rw_unique_port(8173, &bind_port);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS) {
    return NULL;
  }

  status = rw_unique_port(8174, &rpc_port);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS) {
    return NULL;
  }

  install_dir = getenv("INSTALLDIR");

  r = asprintf(&serf_path, "%s/usr/bin/serf", install_dir ? install_dir : "");
  RW_ASSERT(r != -1);
  if (r==-1) {
    return NULL;
  }

  r = asprintf(&bind_arg, "-bind=127.0.0.1:%d", bind_port);
  if (r==-1) {
    return NULL;
  }
  RW_ASSERT(r != -1);

  r = asprintf(&rpc_arg, "-rpc-addr=127.0.0.1:%d", rpc_port);
  RW_ASSERT(r != -1);
  if (r==-1) {
    return NULL;
  }

  char * argv[] = {
    serf_path,
    agent,
    encrypt,
    log,
    node,
    bind_arg,
    rpc_arg,
    NULL
  };

  lpid = fork();
  if (lpid < 0) {
    return NULL;
  }

  if (lpid == 0) {
    r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    RW_ASSERT(r == 0);
    if (r!=0) {
      return NULL;
  }

    execvp(argv[0], argv);
  }
  *pid = lpid;

  serf = serf_instance_alloc(rwvcs, localhost, rpc_port);

  free(serf_path);
  free(bind_arg);
  free(rpc_arg);

  return serf;
}


void _TestEventCallback(void * ctx, const char * event, const serf_member * member);

class TestSerfRPC: public ::testing::Test {
  public:
    int m_count;

  private:
    rwvx_instance_t * m_rwvx;
    rwvcs_instance_t * m_rwvcs;
    serf_context_ptr_t m_serf;
    pid_t m_serf_pid;

  protected:
    virtual void SetUp() {
      m_count = 0;

      m_rwvx = rwvx_instance_alloc();
      m_rwvcs = m_rwvx->rwvcs;
      m_rwvcs->reaper_sock = 1;

      m_serf = start_serf(m_rwvcs, &m_serf_pid);
    }

    virtual void TearDown() {
      int status;

      kill(m_serf_pid, SIGTERM);
      waitpid(m_serf_pid, &status, 0);

      rwsched_instance_free(m_rwvx->rwsched);
      serf_instance_free(m_serf);
    }

    /*
     * This not only tests startup but is also verifying that message handling
     * is working correctly.  This is due to the serf state only changing as we
     * get responses to the handshake, stream and monitor commands.
     */
    virtual void TestStartup() {
      ASSERT_EQ(m_serf->state, SERF_STATE_CLOSED);

      serf_rpc_connect(m_serf, 0.1, 10);

      for (int i = 0; i < 10000; ++i) {
        rwsched_instance_CFRunLoopRunInMode(
            m_rwvx->rwsched,
            rwsched_instance_CFRunLoopGetMainMode(m_rwvx->rwsched),
            0.01,
            false);
        if (m_serf->state == SERF_STATE_ESTABLISHED)
          break;
      }

      ASSERT_EQ(m_serf->state, SERF_STATE_ESTABLISHED);
    }

    /*
     * Verify that the Event Callback interface is working.
     */
    virtual void TestEventCallback() {
      struct serf_event_callback * ec = serf_register_event_callback(m_serf, _TestEventCallback, this);

      serf_rpc_connect(m_serf, 0.1, 10);

      for (int i = 0; i < 10000; ++i) {
        rwsched_instance_CFRunLoopRunInMode(
            m_rwvx->rwsched,
            rwsched_instance_CFRunLoopGetMainMode(m_rwvx->rwsched),
            0.01,
            false);
        if (m_count)
          break;
      }

      serf_free_event_callback(m_serf, ec);

      ASSERT_EQ(m_count, 1);
    }
};

void _TestEventCallback(void * ctx, const char * event, const serf_member * member)
{
  TestSerfRPC * tester = (TestSerfRPC *) ctx;

  if (tester->m_count)
    return;

  ASSERT_STREQ(event, "member-join");
  ASSERT_STREQ(member->name, "serf-gtest");
  ASSERT_STREQ(member->address, "127.0.0.1");
  ASSERT_STREQ(member->status, "alive");

  tester->m_count++;
}


TEST_F(TestSerfRPC, TestStartup) {
  TestStartup();
}

TEST_F(TestSerfRPC, TestEventCallback) {
  TestEventCallback();
}

RWUT_INIT();
