
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
 * @file rwshell_pt_gtest.cpp
 * @author Rajesh Ramankutty
 * @date 2015/05/08
 * @brief Google test cases for testing rwshell
 * 
 * @details Google test cases for testing rwshell
 */

/* 
 * Step 1: Include the necessary header files 
 */
#include <limits.h>
#include <cstdlib>
#include "rwut.h"
#include "rwlib.h"
#include "rw_vx_plugin.h"
#include "rwtrace.h"
#include "rwsched.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"

#include "rwshell-api.h"

using ::testing::HasSubstr;

struct int_status {
  int x;
  rw_status_t status;
};


/*
 * Create a test fixture for testing the plugin framework
 *
 * This fixture is reponsible for:
 *   1) allocating the Virtual Executive (VX)
 *   2) cleanup upon test completion
 */
class RwShellPt : public ::testing::Test {
 public:

 protected:
  rwshell_module_ptr_t m_mod;

  virtual void InitPluginFramework() {

    m_mod = rwshell_module_alloc();
    ASSERT_TRUE(m_mod);

  }

  virtual void TearDown() {
    rwshell_module_free(&m_mod);
  }


  virtual void do_something4() {
    for (int i=0; i<10000; i++) {
      usleep(10);
    }
  }

  virtual void do_something3() {
    do_something4();
  }

  virtual void do_something2() {
    do_something3();
  }

  virtual void do_something1() {
    do_something2();
  }

  virtual void do_something0() {
    do_something1();
  }

  // TESTS
  virtual void TestProfiler() {
    rw_status_t status;

    char **pids = (char**)malloc(4*sizeof(char*));
    char *command;
    int newpid=0;

    static char pids_storage[999];
    sprintf(&pids_storage[0], "%u", getpid());
    pids[0] = &pids_storage[0];
#if 0
    sprintf(&pids_storage[strlen(pids[0])+1], "%u", getppid());
    pids[1] = &pids_storage[strlen(pids[0])+1];
    pids[2] = NULL;
#else
    pids[1] = NULL;
#endif

    //pids[0] = "10393";
    //pids[1] = "10398";
    //pids[2] = "10403";
    //pids[3] = NULL;

    status = rwshell_perf_start(m_mod, (const char**)pids, 200, "/tmp/rwshell.gtest.data", &command, &newpid);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    ASSERT_GT(newpid, 0);
    printf("COMMAND=%s\n", command);

    do_something0();
    sleep(2);
    do_something0();

    status = rwshell_perf_stop(m_mod, newpid, &command);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    printf("COMMAND=%s\n", command);

    sleep(2);

    char *report;
    status = rwshell_perf_report(m_mod, 2.0, "/tmp/rwshell.gtest.data", &command, &report);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
    printf("COMMAND=%s\n", command);
    printf("%s", report);
  }

  virtual void TestCrash() {
    rw_status_t status;

    popen("touch reported-ABC-core.0001.txt", "r");
    popen("touch reported-XYX-core.0002.txt", "r");
    popen("touch reported-123-core.0003.txt", "r");

    sleep(1);

    char **crashes;
    status = rwshell_crash_report(m_mod, "ABC", &crashes);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);

    int i=0;
    while (crashes[i]) {
      printf("Backtrace-%u\n%s", i+1, crashes[i]);
      i++;
    }
    ASSERT_EQ(1, i);

    popen("rm reported-*-core.0001.txt", "r");
  }

};

class RwShellPtPythonPlugin : public RwShellPt {
  virtual void SetUp() {
    InitPluginFramework();
  }
};

TEST_F(RwShellPtPythonPlugin, DISABLED_Profiler) {
  TestProfiler();
}

TEST_F(RwShellPtPythonPlugin, Crash) {
  TestCrash();
}
