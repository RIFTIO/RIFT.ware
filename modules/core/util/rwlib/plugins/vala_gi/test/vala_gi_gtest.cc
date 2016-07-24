
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file vala_gi_gtest.cc
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

#include "vala_gi.h"
#include "example_gi.h"


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
class ValaGiTest : public ::testing::Test {
 protected:
  PeasEngine *engine = NULL;
  PeasExtension *extension;
  PeasPluginInfo *info = NULL;
  ValaGiApi *api;
  ValaGiApiIface *interface;

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
                          "ExampleGi", 
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
  }

  /**
   * Create ValaGiApi and ValaGiApiIface
   * The extension type is either vala_gi-c or vala_gi-python
   */
  virtual void CreateApiInterface(const char *extension_type) {
    info = peas_engine_get_plugin_info(engine, extension_type);
    peas_engine_load_plugin(engine, info);

    extension = peas_engine_create_extension(engine,
                                             info,
                                             VALA_GI_TYPE_API,
                                             NULL);
    g_assert(PEAS_IS_EXTENSION(extension));
    g_assert(VALA_GI_IS_API(extension));

    api = VALA_GI_API(extension);
    interface = VALA_GI_API_GET_INTERFACE(extension);
  }
  
  virtual void SetUp() {
    const char *repo_dirs[] = { 
      INSTALLDIR "/usr/lib/girepository-1.0", 
      PLUGINDIR, 
      NULL 
    };
    const char *plugin_dirs [] = { PLUGINDIR, NULL };

    CreatePeasEngine(repo_dirs, plugin_dirs, "ValaGi", "1.0");
    ASSERT_TRUE(engine != NULL);
  }

  virtual void TearDown() {
    g_object_unref(engine);
  }
};

/*
 * Fixture for testing the Python plugin
 */
class ValaGiPythonTest : public ValaGiTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    ValaGiTest::SetUp();
    peas_engine_enable_loader(engine, "python3");
    ValaGiTest::CreateApiInterface("vala_gi-python");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    ValaGiTest::TearDown();
  }
};

/**
 * Test the 'add' interface on 'python' plugin
 */
TEST_F(ValaGiPythonTest, ValaGiPythonApiAdd) {
  int result, len;
  ExampleGiBoxed *boxed = (ExampleGiBoxed *)NULL;

  /**
   * Call get_box function and verify the contents of the box
   */
  result = interface->get_box(api, &boxed);
  ASSERT_TRUE(boxed);
  ASSERT_EQ(example_gi_boxed_get_integer(boxed), 1729);
  ASSERT_EQ(result, 0);
  const char **array1 = example_gi_boxed_get_string_array(boxed);
  ASSERT_STREQ(array1[0], "hello");
  ASSERT_STREQ(array1[1], "world");
  ASSERT_TRUE(array1[2] == NULL);

  /**
   * Call get_my_obj function and verify the contents of the GObject
   */
  ValaGiMyObj *myobj = (ValaGiMyObj *)NULL;
  result = interface->get_my_obj(api, &myobj);
  ASSERT_TRUE(boxed);
  ASSERT_EQ(result, 0);
  ASSERT_STREQ(vala_gi_my_obj_get_name(myobj), "foo");
  gchar **array2 = vala_gi_my_obj_get_string_array(myobj, &len);
  ASSERT_EQ(len, 2);
  ASSERT_STREQ(array2[0], "hello");
  ASSERT_STREQ(array2[1], "world");

  /**
   * Allocate a box struture, pass the box to Python. Have Python
   * populate the contents of box. Verify the contents of the box
   * after the code returns to C.
   */
  boxed = example_gi_boxed_new();
  example_gi_boxed_set_integer(boxed, 1234);
  result = interface->get_data_in_box(api, boxed);
  ASSERT_EQ(example_gi_boxed_get_integer(boxed), 4321);
  const char **array3 = example_gi_boxed_get_string_array(boxed);
  ASSERT_STREQ(array3[0], "hello");
  ASSERT_STREQ(array3[1], "world");
  ASSERT_TRUE(array3[2] == NULL);
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
