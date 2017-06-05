/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file lua_coroutine_gtest.cc
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
#include <iostream>
#include <exception>

#include <glib.h>
#include <libpeas/peas.h>

#include "lua_coroutine.h"


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
class LuaCoroutineTest : public ::testing::Test {
 protected:
  PeasEngine *engine = NULL;
  PeasExtension *extension;
  PeasPluginInfo *info = NULL;
  LuaCoroutineApi *api;
  LuaCoroutineApiIface *interface;

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
   * Create LuaCoroutineApi and LuaCoroutineApiIface
   * The extension type is either lua_coroutine-c or lua_coroutine-python
   */
  virtual void CreateApiInterface(const char *extension_type) {
    info = peas_engine_get_plugin_info(engine, extension_type);
    peas_engine_load_plugin(engine, info);

    extension = peas_engine_create_extension(engine,
                                             info,
                                             LUA_COROUTINE_TYPE_API,
                                             NULL);
    g_assert(PEAS_IS_EXTENSION(extension));
    g_assert(LUA_COROUTINE_IS_API(extension));

    api = LUA_COROUTINE_API(extension);
    interface = LUA_COROUTINE_API_GET_INTERFACE(extension);
  }
  
  virtual void SetUp() {
    const char *repo_dirs[] = { 
      "/usr/lib64/girepository-1.0", 
      PLUGINDIR, 
      NULL 
    };
    const char *plugin_dirs [] = { PLUGINDIR, NULL };

    CreatePeasEngine(repo_dirs, plugin_dirs, "LuaCoroutine", "1.0");
    ASSERT_TRUE(engine != NULL);
  }

  virtual void TearDown() {
    g_object_unref(engine);
  }
};

/*
 * Fixture for testing the Lua plugin
 */
class LuaCoroutineLuaTest : public LuaCoroutineTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    LuaCoroutineTest::SetUp();
    peas_engine_enable_loader(engine, "lua5.1");
    LuaCoroutineTest::CreateApiInterface("lua_coroutine-lua");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    LuaCoroutineTest::TearDown();
  }
};

/**
 * Test the 'add' interface on 'lua' plugin
 */
TEST_F(LuaCoroutineLuaTest, LuaCoroutineLuaApiAddCoroutineCreate) {
  int a = 1;
  int b = 2;
  int result;
  LuaCoroutineFunctor *functor = (LuaCoroutineFunctor *)NULL;

  try {
    // this call should yield and return the closure
    result = interface->add_coroutine_create(api, a, b, &functor);
  } catch (int e) {
    std::cout << "exception: " << e << '\n';
  }
  ASSERT_EQ(result, a);
  ASSERT_TRUE(functor);

  // This call completes the function
  result = lua_coroutine_functor_callback(functor);
  ASSERT_EQ(result, a+b);
}

/**
 * Test the 'add' interface on 'lua' plugin
 */
TEST_F(LuaCoroutineLuaTest, LuaCoroutineLuaApiAddCoroutineWrap) {
  int a = 1;
  int b = 2;
  int result;
  LuaCoroutineFunctor *functor = (LuaCoroutineFunctor *)NULL;

  // this call should yield and return the closure
  result = interface->add_coroutine_wrap(api, a, b, &functor);
  ASSERT_EQ(result, a);
  ASSERT_TRUE(functor);

  // This call completes the function
  result = lua_coroutine_functor_callback(functor);
  ASSERT_EQ(result, a+b);
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
