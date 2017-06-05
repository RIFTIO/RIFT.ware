/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file basic_functor_gtest.cc
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

#include "basic_functor.h"


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
class BasicFunctorTest : public ::testing::Test {
 protected:
  PeasEngine *engine = NULL;
  PeasExtension *extension;
  PeasPluginInfo *info = NULL;
  BasicFunctorApi *api;
  BasicFunctorApiIface *interface;
  static int callback_happened;

  static int callback(void *user_data)
  {
    callback_happened = 1;
    return 0;
  }

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
  }

  /**
   * Create BasicFunctorApi and BasicFunctorApiIface
   * The extension type is either basic_functor-c or basic_functor-python
   */
  virtual void CreateApiInterface(const char *extension_type) {
    info = peas_engine_get_plugin_info(engine, extension_type);
    peas_engine_load_plugin(engine, info);

    extension = peas_engine_create_extension(engine,
                                             info,
                                             BASIC_FUNCTOR_TYPE_API,
                                             NULL);
    g_assert(PEAS_IS_EXTENSION(extension));
    g_assert(BASIC_FUNCTOR_IS_API(extension));

    api = BASIC_FUNCTOR_API(extension);
    interface = BASIC_FUNCTOR_API_GET_INTERFACE(extension);
  }
  
  virtual void SetUp() {
    const char *repo_dirs[] = { 
      "/usr/lib64/girepository-1.0", 
      PLUGINDIR, 
      NULL 
    };
    const char *plugin_dirs [] = { PLUGINDIR, NULL };

    callback_happened = 0;
    CreatePeasEngine(repo_dirs, plugin_dirs, "BasicFunctor", "1.0");
    ASSERT_TRUE(engine != NULL);
  }

  virtual void TearDown() {
    g_object_unref(engine);
  }
};

/*
 * Fixture for testing the C plugin
 */
class BasicCFunctorTest : public BasicFunctorTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    BasicFunctorTest::SetUp();
    BasicFunctorTest::CreateApiInterface("basic_functor-c");
  }
  
  virtual void TearDown() {
    /* Call the parent tear down routine */
    g_object_unref(extension);
    BasicFunctorTest::TearDown();
  }
};

/*
 * Fixture for testing the Lua plugin
 */
class DISABLED_BasicLuaFunctorTest : public BasicFunctorTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    BasicFunctorTest::SetUp();
    peas_engine_enable_loader(engine, "lua5.1");
    BasicFunctorTest::CreateApiInterface("basic_functor-lua");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    BasicFunctorTest::TearDown();
  }
};
/*
 * Fixture for testing the Python plugin
 */
class BasicPythonFunctorTest : public BasicFunctorTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    BasicFunctorTest::SetUp();
    peas_engine_enable_loader(engine, "python3");
    BasicFunctorTest::CreateApiInterface("basic_functor-python");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    BasicFunctorTest::TearDown();
  }
};

int BasicFunctorTest::callback_happened;

/**
 * Test the 'callback_example' interface on 'c' plugin
 */
TEST_F(BasicCFunctorTest, BasicCFunctorApiCallbackExample) {
  /* Create the functor object */
  BasicFunctorFunctor *functor;
  functor = (BasicFunctorFunctor *) g_object_new(BASIC_FUNCTOR_TYPE_FUNCTOR, 0);

  /* Assign the callback function to functor */
  basic_functor_functor_set_function(functor, 
                                     BasicFunctorTest::callback,
                                     NULL,
                                     NULL);
  interface->callback_example(api, (BasicFunctorFunctor *)functor);
  usleep(10 * 1000);
  ASSERT_EQ(callback_happened, 1);
}

/**
 * Test the 'callback_example' interface on 'python' plugin
 */
TEST_F(BasicPythonFunctorTest, BasicPythonFunctorApiAdd) {
  /* Create the functor object */
  BasicFunctorFunctor *functor;
  functor = (BasicFunctorFunctor *) g_object_new(BASIC_FUNCTOR_TYPE_FUNCTOR, 0);

  /* Assign the callback function to functor */
  basic_functor_functor_set_function(functor, 
                                     BasicFunctorTest::callback,
                                     NULL,
                                     NULL);
  interface->callback_example(api, (BasicFunctorFunctor *)functor);
  usleep(10 * 1000);
  ASSERT_EQ(callback_happened, 1);
}

/**
 * Test the 'callback_example' interface on 'lua' plugin
 */
TEST_F(DISABLED_BasicLuaFunctorTest, BasicLuaFunctorApiAdd) {
  /* Create the functor object */
  BasicFunctorFunctor *functor;
  functor = (BasicFunctorFunctor *) g_object_new(BASIC_FUNCTOR_TYPE_FUNCTOR, 0);

  /* Assign the callback function to functor */
  basic_functor_functor_set_function(functor, 
                                     BasicFunctorTest::callback,
                                     NULL,
                                     NULL);
  interface->callback_example(api, (BasicFunctorFunctor *)functor);
  usleep(10 * 1000);
  ASSERT_EQ(callback_happened, 1);
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
