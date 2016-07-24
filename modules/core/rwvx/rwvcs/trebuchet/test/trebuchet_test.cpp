
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


#include <rwlib.h>
#include <rwut.h>
#include <rw_sys.h>

#include <trebuchet.h>

using ::testing::HasSubstr;


static pid_t start_trebuchet(const char * trebuchet_path, const char * trebuchet_dir, uint16_t bind_port)
{
  int r;
  pid_t pid;

  pid = fork();
  RW_ASSERT(pid >= 0);
  if (pid == 0) {
    char * port = NULL;
    char * rwmain_path = NULL;

    r = asprintf(&rwmain_path, "%s/test/test_rwmain.sh", trebuchet_dir);
    RW_ASSERT(r != -1);

    r = asprintf(&port, "%u", bind_port);
    RW_ASSERT(r != -1);

    execl("/usr/bin/python", "/usr/bin/python",
            trebuchet_path,
            "--verbose",
            "--port", port,
            "--rwmain", rwmain_path,
            "--no-suid",
            NULL);
    fprintf(stderr, "execl: %s\n", strerror(errno));
    RW_CRASH();
  } else {
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    RW_ASSERT(sock >= 0);

    bzero(&addr, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (int i = 0; i < 1000; ++i) {
      r = connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
      if (!r)
        break;
      usleep(1000);
    }
    RW_ASSERT(r == 0);
  }

  return pid;
}


class TestTrebuchet: public ::testing::Test {
  private:
    struct trebuchet * m_trebuchet;

  protected:
    static pid_t m_pid;
    static uint16_t m_port;
    static char * m_dir;
    static char * m_trebuchet_path;

  protected:
    static void SetUpTestCase() {
      int r;
      rw_status_t status;

      trebuchet_init();

      status = rw_unique_port(8679, &m_port);
      RW_ASSERT(status == RW_STATUS_SUCCESS);

      m_dir = getenv("TREBUCHET_DIR");
      r = asprintf(&m_trebuchet_path, "%s/trebuchet", m_dir);
      RW_ASSERT(r != -1);
    }

    static void TearDownTestCase() {
      free(m_trebuchet_path);
    }

    virtual void SetUp() {
      int r;

      m_trebuchet = trebuchet_alloc();
      r = trebuchet_connect(m_trebuchet, "localhost", (int)m_port);
      ASSERT_EQ(0, r);

      m_pid = start_trebuchet(m_trebuchet_path, m_dir, m_port);
    }

    virtual void TearDown() {
      int r;
      int status;

      trebuchet_free(m_trebuchet);

      kill(m_pid, SIGTERM);
      for (int i = 0; i < 1000; ++i) {
        r = waitpid(m_pid, &status, WNOHANG);
        if (r == m_pid)
          break;
        usleep(1000);
      }

      if (r != m_pid)
        kill(m_pid, SIGKILL);

      waitpid(m_pid, &status, 0);
    }

    virtual void TestPing() {
      int r;
      double ret = 0.0;
      struct timeval tv;

      r = gettimeofday(&tv, NULL);
      ASSERT_EQ(0, r);

      r = trebuchet_ping(m_trebuchet, "localhost", (int)m_port, &ret);
      fprintf(stderr, "TREBUCHET: ping got %f\n", ret);
      ASSERT_EQ(0, r);
      ASSERT_NEAR(ret, tv.tv_sec, 1.0);
    }

    virtual void TestBadManifestPath() {
      int r;
      void * req = NULL;
      char * manifest_path = NULL;

      r = asprintf(
          &manifest_path,
          "%s/test/test_manifest_which_does_not_exist.xml",
          m_dir);
      RW_ASSERT(r != -1);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "component-name",
          111,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req == NULL);

      free(manifest_path);
    }

    virtual void TestLaunch() {
      int r;
      void * req = NULL;
      char * manifest_path = NULL;
      struct trebuchet_launch_resp resp;


      r = asprintf(&manifest_path, "%s/test/test_manifest.xml", m_dir);
      RW_ASSERT(r != -1);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "component-name",
          111,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req != NULL);

      r = trebuchet_launch(m_trebuchet, "localhost", (int)m_port, req, &resp);
      ASSERT_EQ(0, r);
      ASSERT_EQ(0, resp.ret);
      ASSERT_EQ(NULL, resp.errmsg);
      ASSERT_EQ(NULL, resp.log);

      trebuchet_launch_req_free(req);
    }

    virtual void TestLaunchFail() {
      int r;
      void * req = NULL;
      char * manifest_path = NULL;
      struct trebuchet_launch_resp resp;


      r = asprintf(&manifest_path, "%s/test/test_manifest.xml", m_dir);
      RW_ASSERT(r != -1);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "FAIL",
          111,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req != NULL);

      r = trebuchet_launch(m_trebuchet, "localhost", (int)m_port, req, &resp);
      ASSERT_EQ(EIO, r);
      ASSERT_EQ(1, resp.ret);
      ASSERT_STREQ("Rift.VM died after 1s", resp.errmsg);
      ASSERT_STREQ("Failing rwmain exiting with status 1\n", resp.log);

      trebuchet_launch_req_free(req);
    }

    virtual void TestStatus() {
      int r;
      void * req = NULL;
      char * manifest_path = NULL;
      struct trebuchet_launch_resp resp;
      trebuchet_vm_status status;


      r = asprintf(&manifest_path, "%s/test/test_manifest.xml", m_dir);
      RW_ASSERT(r != -1);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "c",
          1,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req != NULL);

      r = trebuchet_launch(m_trebuchet, "localhost", (int)m_port, req, &resp);
      ASSERT_EQ(0, r);

      r = trebuchet_get_status(m_trebuchet, "localhost", (int)m_port, "c", 1, &status);
      ASSERT_EQ(0, r);
      ASSERT_EQ(TREBUCHET_VM_RUNNING, status);

      r = trebuchet_get_status(m_trebuchet, "localhost", (int)m_port, "c", 2, &status);
      ASSERT_EQ(0, r);
      ASSERT_EQ(TREBUCHET_VM_NOTFOUND, status);

      trebuchet_launch_req_free(req);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "FAIL",
          111,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req != NULL);
      r = trebuchet_launch(m_trebuchet, "localhost", (int)m_port, req, &resp);
      ASSERT_EQ(EIO, r);

      r = trebuchet_get_status(m_trebuchet, "localhost", (int)m_port, "FAIL", 111, &status);
      ASSERT_EQ(0, r);
      ASSERT_EQ(TREBUCHET_VM_CRASHED, status);
    }

    virtual void TestTerminate() {
      int r;
      int ret;
      void * req = NULL;
      char * manifest_path = NULL;
      struct trebuchet_launch_resp resp;
      trebuchet_vm_status status;


      r = asprintf(&manifest_path, "%s/test/test_manifest.xml", m_dir);
      RW_ASSERT(r != -1);

      req = trebuchet_launch_req_alloc(
          manifest_path,
          "c",
          1,
          "parent-id",
          "127.0.111.111",
          0);
      ASSERT_TRUE(req != NULL);

      r = trebuchet_launch(m_trebuchet, "localhost", (int)m_port, req, &resp);
      ASSERT_EQ(0, r);

      r = trebuchet_get_status(m_trebuchet, "localhost", (int)m_port, "c", 1, &status);
      ASSERT_EQ(0, r);
      ASSERT_EQ(TREBUCHET_VM_RUNNING, status);

      r = trebuchet_terminate(m_trebuchet, "localhost", (int)m_port, "c", 1, &ret);
      ASSERT_EQ(0, r);
      ASSERT_EQ(0, ret);

      r = trebuchet_get_status(m_trebuchet, "localhost", (int)m_port, "c", 1, &status);
      ASSERT_EQ(0, r);
      ASSERT_EQ(TREBUCHET_VM_NOTFOUND, status);

      trebuchet_launch_req_free(req);
    }

};
pid_t TestTrebuchet::m_pid;
uint16_t TestTrebuchet::m_port;
char * TestTrebuchet::m_dir;
char * TestTrebuchet::m_trebuchet_path;


TEST_F(TestTrebuchet, TestPing) {
  TestPing();
}

TEST_F(TestTrebuchet, TestBadManifestPath) {
  TestBadManifestPath();
}

TEST_F(TestTrebuchet, TestLaunch) {
  TestLaunch();
}

TEST_F(TestTrebuchet, TestLaunchFail) {
  TestLaunchFail();
}

TEST_F(TestTrebuchet, TestStatus) {
  TestStatus();
}

TEST_F(TestTrebuchet, TestTerminate) {
  TestTerminate();
}

RWUT_INIT();

// vim: sw=2
