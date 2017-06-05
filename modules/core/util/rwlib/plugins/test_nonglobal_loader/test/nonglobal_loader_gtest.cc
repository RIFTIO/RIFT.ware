/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file basic_plugin_gtest.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/18/2013
 * @brief Google test cases for testing rwlib
 * 
 * @details Google test cases for testing rwlib. Specifically
 * This file includes test cases for RW_ASSERT framework
 */

/** 
 * Step 1: Include the necessary header files 
 */
#include <string.h>
#include <limits.h>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <glib.h>
#include <libpeas/peas.h>

#include "rwlib.h"
#include "rw_vx_plugin.h"

#include "test_nonglobal_loader.h"
#include "rwut.h"

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;


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

/*
 * Create a test fixture for testing the plugin framework
 *
 * This fixture is reponsible for:
 *   2) setting up the peas engine search paths
 *   3) cleanup upon test completion
 */
class TestNonglobalLoader : public ::testing::Test {
 protected:
  static int fast_add_function_done;
  static int slow_add_function_done;
  static int fast_add_function_start;
  static int slow_add_function_start;

  /**
   * Create BasicPluginApi and BasicPluginApiIface
   * The extension type is either basic_plugin-c or basic_plugin-python
   */
  static void CreateApiInterface(rw_vx_framework_t *rw_vxfp,
                                  char *plugin_name, 
                                  TestNonglobalLoaderApi **klass,
                                  TestNonglobalLoaderApiIface **iface) {
    rw_status_t status;
    rw_vx_modinst_common_t *mip;

    status = rw_vx_library_open(rw_vxfp, plugin_name, (char *)"", &mip);
    RW_ASSERT(status == RW_STATUS_SUCCESS);


    status = rw_vx_library_get_api_and_iface(mip,
                                             TEST_NONGLOBAL_LOADER_TYPE_API,
                                             (void **)klass,
                                             (void **)iface,
                                              NULL);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }

  
  static rw_vx_framework_t *InitPluginFramework() {
    rw_vx_framework_t *rw_vxfp;

    /*
     * Allocate a RIFT_VX framework to run the test
     * NOTE: this initializes a LIBPEAS instance
     */
    // rw_vxfp = rw_vx_framework_alloc();
    rw_vxfp = rw_vx_framework_alloc_nonglobal();
    RW_ASSERT(RW_VX_FRAMEWORK_VALID(rw_vxfp));

    rw_vx_prepend_gi_search_path("/usr/lib64/girepository-1.0");
    rw_vx_prepend_gi_search_path(PLUGINDIR);
    rw_vx_add_peas_search_path(rw_vxfp, PLUGINDIR);
    rw_vx_require_repository("TestNonglobalLoader", "1.0");

    return rw_vxfp;
  }

  virtual void TearDown() {
  }

  virtual void SetUp() {
  }

  static void *SlowAddThread(void *ctx) {
    int secs = 10;
    int a = 1;
    int b = 2;
    rw_vx_framework_t *rw_vxfp;
    TestNonglobalLoaderApi *klass;
    TestNonglobalLoaderApiIface *iface;
    int rc;

    rw_vxfp = InitPluginFramework();
    CreateApiInterface(rw_vxfp, 
                       (char *)"test_nonglobal_loader-lua",
                       &klass, 
                       &iface);

    printf("Starting slow add function...\n");
    fflush(stdout);
    slow_add_function_start = 1;
    rc = iface->add(klass, secs, a, b);
    printf("Slow function returned: %d\n", rc);
    slow_add_function_done = 1;
    fflush(stdout);

    rw_status_t status;
    status = rw_vx_framework_free(rw_vxfp);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    return NULL;
  }

  static void *FastAddThread(void *ctx) {
    int a = 1;
    int b = 2;
    int rc;
    rw_vx_framework_t *rw_vxfp;
    TestNonglobalLoaderApi *klass;
    TestNonglobalLoaderApiIface *iface;

    rw_vxfp = InitPluginFramework();
    CreateApiInterface(rw_vxfp, 
                       (char *)"test_nonglobal_loader-lua",
                       &klass, 
                       &iface);

    fast_add_function_start = 1;
    printf("Starting fast add function...\n");
    fflush(stdout);
    rc = iface->add(klass, 0, a, b);
    fast_add_function_done = 1;
    printf("Fast function returned: %d\n", rc);
    fflush(stdout);

    rw_status_t status;
    status = rw_vx_framework_free(rw_vxfp);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    return NULL;
  }
};

int TestNonglobalLoader::fast_add_function_done;
int TestNonglobalLoader::slow_add_function_done;
int TestNonglobalLoader::fast_add_function_start;
int TestNonglobalLoader::slow_add_function_start;

/**
 * Test the 'add' interface on 'lua' plugin
 */
TEST_F(TestNonglobalLoader, Add) {
  int rc;
  pthread_t thread1, thread2;

  rc = pthread_create(&thread1, 
                      NULL, 
                      TestNonglobalLoader::SlowAddThread, 
                      (void *)"SlowAddThread");
  ASSERT_EQ(rc, 0);

  // block till the slow add function is called
  while (TestNonglobalLoader::slow_add_function_start != 1);

  // slow add function started, now launch another thread
  // to the fast add.
  printf("Starting FastAddThread\n");
  fflush(stdout);
  rc = pthread_create(&thread2, 
                      NULL, 
                      TestNonglobalLoader::FastAddThread, 
                      (void *)"FastAddThread");
  ASSERT_EQ(rc, 0);

  // Sleep arbiraty amount of time
  usleep(100000);

  // At this stage the fast add function should have completed,
  // and slow add function should still be pending.
  ASSERT_EQ(TestNonglobalLoader::fast_add_function_done, 1);
  ASSERT_EQ(TestNonglobalLoader::slow_add_function_done, 0);
  
  // pthread_join(thread1, NULL);
  // pthread_join(thread2, NULL);

  pthread_cancel(thread1);
  pthread_cancel(thread2);
}

// Step 3. Call RUN_ALL_TESTS() in main().
//
// We do this by linking in src/gtest_main.cc file, which consists of
// a main() function which calls RUN_ALL_TESTS() for us.
//
// This runs all the tests you've defined, prints the result, and
// returns 0 if successful, or 1 otherwise.
//
// Did you notice that we didn't register the tests?  The
// RUN_ALL_TESTS() macro magically knows about all the tests we
// defined.  Isn't this convenient?
