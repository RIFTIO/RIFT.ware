/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rwpython_util_gtest.cc
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

#include <glib.h>
#include <libpeas/peas.h>

#include "rwpython_util.h"


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
class RwpythonUtilTest : public ::testing::Test {
 protected:
  PeasEngine *engine = NULL;
  PeasExtension *extension;
  PeasPluginInfo *info = NULL;
  rwpython_utilApi *api;
  rwpython_utilApiIface *interface;

  /**
   * This function creates the peas engine
   */
  virtual void CreatePeasEngine(const char *repo_dirs[],
                                const char *plugin_dirs[],
                                const char *plugin_name,
                                const char *plugin_version) {
    GError *error = NULL;
    int i;

    // setup repository search path, this is where the typelib files should
    // exist for python 
    for (i = 0; repo_dirs[i]; ++i) {
      printf("Repository Directory = %s\n", repo_dirs[i]);
      g_irepository_prepend_search_path(repo_dirs[i]);
    }

    // Load the repositories
    g_irepository_require(g_irepository_get_default(), 
                          "Peas", 
                          "1.0", 
                          (GIRepositoryLoadFlags) 0, 
                          &error);
    g_assert_no_error(error);

    g_irepository_require(g_irepository_get_default(),
                          plugin_name, 
                          plugin_version, 
                          (GIRepositoryLoadFlags) 0, 
                          &error);
    g_assert_no_error(error);

    // Initialize a PEAS engine
    engine = peas_engine_get_default();
    g_object_add_weak_pointer(G_OBJECT(engine), (gpointer *) &engine);

    // Inform the PEAS engine where the plugins reside 
    for (i = 0; plugin_dirs[i]; ++i) {
      printf("Plugin Directory = %s\n", plugin_dirs[i]);
      peas_engine_add_search_path(engine, plugin_dirs[i], plugin_dirs[i]);
    }

    // Enable python loader
    peas_engine_enable_loader(engine, "python3");
  }

  /**
   * Create RwpythonUtilApi and RwpythonUtilApiIface
   * The extension type is either rwpython_util-c or rwpython_util-python
   */
  virtual void CreateApiInterface(const char *extension_type) {
    info = peas_engine_get_plugin_info(engine, extension_type);
    g_assert(info);

    peas_engine_load_plugin(engine, info);

    extension = peas_engine_create_extension(engine,
                                             info,
                                             RWPYTHON_UTIL_TYPE_API,
                                             NULL);
    g_assert(PEAS_IS_EXTENSION(extension));
    g_assert(RWPYTHON_UTIL_IS_API(extension));

    api = RWPYTHON_UTIL_API(extension);
    interface = RWPYTHON_UTIL_API_GET_INTERFACE(extension);
  }
  
  virtual void SetUp() {
    const char *repo_dirs[] = { 
      "/usr/lib64/girepository-1.0", 
      PLUGINDIR, 
      NULL 
    };
    const char *plugin_dirs [] = { PLUGINDIR, NULL };

    CreatePeasEngine(repo_dirs, plugin_dirs, "rwpython_util", "1.0");
    ASSERT_TRUE(engine != NULL);
    CreateApiInterface("rwpython_util");
  }

  virtual void TearDown() {
    g_object_unref(extension);
    g_object_unref(engine);
  }
};

/**
 * Test the 'eval integer' interface on 'python' plugin
 */
TEST_F(RwpythonUtilTest, RwpythonUtilApiEvalInteger) {
  rw_status_t status = RW_STATUS_FAILURE;
  int result;

  status = interface->python_run_string(api, "rwvm_instance_id = 1");
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = interface->python_eval_integer(api, "int(rwvm_instance_id)", &result);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_EQ(result, 1);

  status = interface->python_eval_integer(api, "int(dne)", &result);
  ASSERT_EQ(status, RW_STATUS_FAILURE);
}

/**
 * Test the 'eval integer' interface on 'python' plugin
 */
TEST_F(RwpythonUtilTest, RwpythonUtilApiEvalString) {
  rw_status_t status = RW_STATUS_FAILURE;
  char * result = NULL;

  status = interface->python_run_string(api, "rw_component_name = 'RW_VM_CLI'");
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = interface->python_eval_string(api, "str(rw_component_name)", &result);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_STREQ("RW_VM_CLI", result);
  free(result);

  status = interface->python_eval_string(api, "str(dne)", &result);
  ASSERT_EQ(status, RW_STATUS_FAILURE);
  ASSERT_STREQ(result, "");
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
