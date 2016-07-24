
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file feature_plugin_gtest.cc
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

#include "feature_plugin.h"


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
class FeaturePluginTest : public ::testing::Test {
 protected:
  PeasEngine *engine = NULL;
  PeasExtension *extension;
  PeasPluginInfo *info = NULL;
  FeaturePluginApi *api;
  FeaturePluginApiIface *interface;

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
                          "MathCommon", 
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
      peas_engine_add_search_path(engine, plugin_dirs[i], plugin_dirs[i]);
    }
  }

  /**
   * Create FeaturePluginApi and FeaturePluginApiIface
   * The extension type is either feature_plugin-c or feature_plugin-python
   */
  virtual void CreateApiInterface(const char *extension_type) {
    info = peas_engine_get_plugin_info(engine, extension_type);
    peas_engine_load_plugin(engine, info);

    extension = peas_engine_create_extension(engine,
                                             info,
                                             FEATURE_PLUGIN_TYPE_API,
                                             NULL);
    g_assert(PEAS_IS_EXTENSION(extension));
    g_assert(FEATURE_PLUGIN_IS_API(extension));

    api = FEATURE_PLUGIN_API(extension);
    interface = FEATURE_PLUGIN_API_GET_INTERFACE(extension);
  }
  
  virtual void SetUp() {
    const char *repo_dirs[] = { 
      INSTALLDIR "/usr/lib/girepository-1.0", 
      PLUGINDIR, 
      NULL 
    };
    const char *plugin_dirs [] = { PLUGINDIR, NULL };

    CreatePeasEngine(repo_dirs, plugin_dirs, "FeaturePlugin", "1.0");
    ASSERT_TRUE(engine != NULL);
  }

  virtual void TearDown() {
    g_object_unref(engine);
  }

  /**
   * Example #1a
   *   Method that is invoked with in/out/in-out basic integer types (no return)
   *
   *   Take a number (in/out) then add a number to it (in), 
   *   output the negative of it (out)
   */
  virtual void test_feature_plugin_api_example1a() {
    gint32 number, delta, negative;

    number = 1;
    delta = 2;
    negative = 0;

    interface->example1a(api, &number, delta, &negative);
    ASSERT_EQ(number, 3);
    ASSERT_EQ(negative, -3);
  }

  /**
   *  Example #1b
   *  Method that is invoked with in/out/in-out basic integer types (with return)
   *
   *  Take a number (in/out) then add a number to it (in), 
   *  output the negative of it (out)
   *  Return the sum of all 3 of these
   */
  virtual void test_feature_plugin_api_example1b() {
    gint32 number, delta, negative;
    gint result;

    number = 1;
    delta = 2;
    negative = 0;
    result = interface->example1b(api, &number, delta, &negative);

    ASSERT_EQ(number, 3);
    ASSERT_EQ(negative, -3);
    ASSERT_EQ(result, number + delta + negative);
  }

  /**
   * Example #1c 
   *   Method that is invoked with 3 in and 3 out basic integer types (no return)
   *
   *   Take a number (in/out) then add a number to it (in), 
   *   output the negative of it (out)
   */
  virtual void test_feature_plugin_api_example1c() {
    gint32 number1, number2, number3;
    gint32 negative1, negative2, negative3;

    number1 = 1;
    number2 = 2;
    number3 = 3;
    negative1 = negative2 = negative3 = 0;

    interface->example1c(api, number1, number2, number3, &negative1, &negative2, &negative3);
    ASSERT_EQ(negative1, -1);
    ASSERT_EQ(negative2, -2);
    ASSERT_EQ(negative3, -3);
  }

  /**
   * Example #1d 
   * Method that is invoked with in/out/in-out basic integer types (with return)

   * Take a number (in/out) then add a number to it (in), output the negative of it (out)
   * Return the sum of the square of all 6 of these
   */
  virtual void test_feature_plugin_api_example1d() {
    gint32 number1, number2, number3;
    gint32 negative1, negative2, negative3;
    gint result;

    number1 = 1;
    number2 = 2;
    number3 = 3;
    negative1 = negative2 = negative3 = 0;

    result = interface->example1d(api, number1, number2, number3, &negative1, &negative2, &negative3);
    ASSERT_EQ(negative1, -1);
    ASSERT_EQ(negative2, -2);
    ASSERT_EQ(negative3, -3);
    ASSERT_EQ(result, 1 * 1 + 2 * 2 + 3 * 3 + 1 * 1 + 2 * 2 + 3 * 3);
  }

  /**
   * Example #1e
   * Method that is invoked with in/out/in-out basic integer types (with return)
   *
   * Take a string (in/out) then append a String to it (in), and also output
   * the reverse of it (out)
   * Return the length of the string
   */
  virtual void test_feature_plugin_api_example1e() {
    char *value, *delta, *reverse;
    gint result;

    value = strdup("abc");
    delta = (char *)"def";
    reverse = NULL;
    result = interface->example1e(api, &value, delta, &reverse);
    ASSERT_STREQ(value, "abcdef");
    ASSERT_STREQ(reverse, "fedcba");
    ASSERT_EQ(result, strlen("abcdef"));
  }

  /**
   * Example #1f 
   * Method that is invoked with in/out/in-out basic integer types (no return)
   *
   * Take an vector array (in/out) then add a array to it (in), and also
   * output the origin reflection of it (out)
   * Return the sum of the reflection elements
   */
  virtual void test_feature_plugin_api_example1f() {
    gint *array;
    int array_len;
    gint delta_array[] = { 3, 4, 5 };
    int delta_len = 3;
    int reflection_len = 0;
    gint *reflection_array = NULL;
    int result;

    array = (gint *)g_new(gint, 3);
    array[0] = 0;
    array[1] = 1;
    array[2] = 2;
    array_len = 3;

    result = interface->example1f(api, 
                                  &array, 
                                  &array_len, 
                                  delta_array, 
                                  delta_len, 
                                  &reflection_array, 
                                  &reflection_len);
    ASSERT_EQ(array_len, 3);
    ASSERT_EQ(reflection_len, 3);

    ASSERT_EQ(array[0], 3);
    ASSERT_EQ(array[1], 5);
    ASSERT_EQ(array[2], 7);

    ASSERT_EQ(reflection_array[0], -3);
    ASSERT_EQ(reflection_array[1], -5);
    ASSERT_EQ(reflection_array[2], -7);

    ASSERT_EQ(result, -15);
  }

  /**
   * Example #2a 
   * Method that is invoked with a 'in' structure imported from this header file
   *
   * Compute the modulus of a complex number
   */
  virtual void test_feature_plugin_api_example2a() {
    FeaturePlugin_ComplexNumber *a = NULL;
    gint result;

    a = (FeaturePlugin_ComplexNumber *)g_object_new(FEATURE_PLUGIN_TYPE__COMPLEXNUMBER, 0);
    ASSERT_TRUE(a);
    a->real = 3;
    a->imag = 4;
    result = interface->example2a(api, a);
    ASSERT_EQ(result, 5);
  }

  /**
   * Example #2b 
   * Method that is invoked with a out structure imported from this header file
   *
   * Initialize a complex number to (0, -i)
   */
  virtual void test_feature_plugin_api_example2b() {
    FeaturePlugin_ComplexNumber *a;
    interface->example2b(api, &a);
    ASSERT_EQ(a->real, 0);
    ASSERT_EQ(a->imag, -1);
  }
    
  /**
   * Example #2c 
   * Method that is invoked with a 'in' structure imported from a common header file
   *
   * Compute the modulus of a complex number
   */
  virtual void test_feature_plugin_api_example2c() {
    MathCommon_ComplexNumber *a = NULL;
    gint result;

    a = (MathCommon_ComplexNumber *)g_object_new(MATH_COMMON_TYPE__COMPLEXNUMBER, 0);
    ASSERT_TRUE(a);
    a->real = 3;
    a->imag = 4;
    result = interface->example2c(api, a);
    ASSERT_EQ(result, 5);
  }

  /**
   * Example #2d 
   *   Method that is invoked with a out structure imported from a common header file
   *
   *   Initialize a complex number to (0, -i)
   */
  virtual void test_feature_plugin_api_example2d() {
    MathCommon_ComplexNumber *a;
    interface->example2d(api, &a);
    ASSERT_EQ(a->real, 0);
    ASSERT_EQ(a->imag, -1);
  }

  /**
   * Example #6a 
   *   Method that is invoked with in/out/in-out static GArray types (no-return)
   *
   * Take a vector (in/out) then add a number to it (in), output the negative of it (out)
   */
  virtual void test_feature_plugin_api_example6a() {
  }

  /**
   * Example #6b 
   *   Method that is invoked with in/out/in-out _StaticArrayInt class types 
   *
   * Take a vector (in/out) then add a number to it (in), output the negative of it (out)
   */
  virtual void test_feature_plugin_api_example6b() {
    FeaturePlugin_VectorInt *value, *delta, *reflection;
    gint result;

    value = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(value);
    value->_sarrayint->_storage[0] = 0;
    value->_sarrayint->_storage[1] = 1;
    value->_sarrayint->_storage[2] = 2;

    // Initialize the delta (3, 4, 5) to it
    delta = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(delta);
    delta->_sarrayint->_storage[0] = 3;
    delta->_sarrayint->_storage[1] = 4;
    delta->_sarrayint->_storage[2] = 5;

    // Initialize the reflection to an empty vector
    reflection = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(reflection);

    result = interface->example6b(api, &value, delta, &reflection);

    // After the operation, the value should be (3, 5, 7) 
    // and the reflection should be (-3, -5, -7)
    ASSERT_EQ(result, -15);
    ASSERT_EQ(value->_sarrayint->_len, 3);
    ASSERT_EQ(value->_sarrayint->_storage[0], 3);
    ASSERT_EQ(value->_sarrayint->_storage[1], 5);
    ASSERT_EQ(value->_sarrayint->_storage[2], 7);

    ASSERT_EQ(delta->_sarrayint->_len, 3);
    ASSERT_EQ(delta->_sarrayint->_storage[0], 3);
    ASSERT_EQ(delta->_sarrayint->_storage[1], 4);
    ASSERT_EQ(delta->_sarrayint->_storage[2], 5);

    ASSERT_EQ(reflection->_sarrayint->_len, 3);
    ASSERT_EQ(reflection->_sarrayint->_storage[0], -3);
    ASSERT_EQ(reflection->_sarrayint->_storage[1], -5);
    ASSERT_EQ(reflection->_sarrayint->_storage[2], -7);
  }

  /** 
   * Example #6c 
   *   Method that is invoked with in/out/in-out _StaticArray<int> class types 
   *
   * Take a vector (in/out) then add a number to it (in), output the negative of it (out)
   */
  virtual void test_feature_plugin_api_example6c() {
    FeaturePlugin_VectorInt *value, *delta, *reflection;
    gint result;

    // Initialize the vector to (0, 1, 2)
    value = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(value);
    ((gint *) value->_sarrayint2->_storage)[0] = 0;
    ((gint *) value->_sarrayint2->_storage)[1] = 1;
    ((gint *) value->_sarrayint2->_storage)[2] = 2;

    // Initialize the delta (3, 4, 5) to it
    delta = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(delta);
    ((gint *) delta->_sarrayint2->_storage)[0] = 3;
    ((gint *) delta->_sarrayint2->_storage)[1] = 4;
    ((gint *) delta->_sarrayint2->_storage)[2] = 5;

    // Initialize the reflection to an empty vector
    reflection = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
    ASSERT_TRUE(reflection);

    // Call the plugin function using the function pointer
    result = interface->example6c(api, &value, delta, &reflection);

    // After the operation, the value should be (3, 5, 7) 
    // and the reflection should be (-3, -5, -7)
    ASSERT_EQ(result, -15);

    ASSERT_EQ(value->_sarrayint2->_len, 3);
    ASSERT_EQ(((gint *) value->_sarrayint2->_storage)[0], 3);
    ASSERT_EQ(((gint *) value->_sarrayint2->_storage)[1], 5);
    ASSERT_EQ(((gint *) value->_sarrayint2->_storage)[2], 7);

    ASSERT_EQ(delta->_sarrayint2->_len, 3);
    ASSERT_EQ(((gint *) delta->_sarrayint2->_storage)[0], 3);
    ASSERT_EQ(((gint *) delta->_sarrayint2->_storage)[1], 4);
    ASSERT_EQ(((gint *) delta->_sarrayint2->_storage)[2], 5);

    ASSERT_EQ(reflection->_sarrayint2->_len, 3);
    ASSERT_EQ(((gint *) reflection->_sarrayint2->_storage)[0], -3);
    ASSERT_EQ(((gint *) reflection->_sarrayint2->_storage)[1], -5);
    ASSERT_EQ(((gint *) reflection->_sarrayint2->_storage)[2], -7);
  }

  virtual void test_feature_plugin_api_example7a() {
  }

  /**
   * Example #7b 
   *   Method that is invoked with in/out/in-out _DynamicArrayInt class types 
   *
   *   Take a vector (in/out) then add a number to it (in), output the negative of it (out)
   *   Return the sum of the reflection elements
   */
   virtual void test_feature_plugin_api_example7b() {
     FeaturePlugin_VectorInt *value, *delta, *reflection;
     gint result;

     // Initialize the vector to (0, 1, 2)
     value = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
     ASSERT_TRUE(value);
     feature_plugin__dynamicarrayint_set(value->_darrayint, 0, 0);
     feature_plugin__dynamicarrayint_set(value->_darrayint, 1, 1);
     feature_plugin__dynamicarrayint_set(value->_darrayint, 2, 2);

     // Initialize the delta (3, 4, 5) to it
     delta = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
     ASSERT_TRUE(delta);
     feature_plugin__dynamicarrayint_set(delta->_darrayint, 0, 3);
     feature_plugin__dynamicarrayint_set(delta->_darrayint, 1, 4);
     feature_plugin__dynamicarrayint_set(delta->_darrayint, 2, 5);

     // Initialize the reflection to an empty vector
     reflection = (FeaturePlugin_VectorInt *) g_object_new(FEATURE_PLUGIN_TYPE__VECTORINT, 0);
     ASSERT_TRUE(reflection);

     // Call the plugin function using the function pointer
     result = interface->example7b(api, &value, delta, &reflection);
 
     // After the operation, the value should be (3, 5, 7) 
     // and the reflection should be (-3, -5, -7)
     ASSERT_EQ(result, -15);
 
     ASSERT_EQ(value->_darrayint->_storage->len, 3);
     ASSERT_EQ(g_array_index(value->_darrayint->_storage, int, 0), 3);
     ASSERT_EQ(g_array_index(value->_darrayint->_storage, int, 1), 5);
     ASSERT_EQ(g_array_index(value->_darrayint->_storage, int, 2), 7);
 
     ASSERT_EQ(delta->_darrayint->_storage->len, 3);
     ASSERT_EQ(g_array_index(delta->_darrayint->_storage, int, 0), 3);
     ASSERT_EQ(g_array_index(delta->_darrayint->_storage, int, 1), 4);
     ASSERT_EQ(g_array_index(delta->_darrayint->_storage, int, 2), 5);

     ASSERT_EQ(reflection->_darrayint->_storage->len, 3);
     ASSERT_EQ(g_array_index(reflection->_darrayint->_storage, int, 0), -3);
     ASSERT_EQ(g_array_index(reflection->_darrayint->_storage, int, 1), -5);
     ASSERT_EQ(g_array_index(reflection->_darrayint->_storage, int, 2), -7);
   }

   virtual void test_feature_plugin_api_example7c() {
   }



};

/*
 * Fixture for testing the C plugin
 */
class FeaturePluginCTest : public FeaturePluginTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    FeaturePluginTest::SetUp();
    FeaturePluginTest::CreateApiInterface("feature_plugin-c");
  }
  
  virtual void TearDown() {
    /* Call the parent tear down routine */
    g_object_unref(extension);
    FeaturePluginTest::TearDown();
  }
};

/*
 * Fixture for testing the Lua plugin
 */
class FeaturePluginLuaTest : public FeaturePluginTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    FeaturePluginTest::SetUp();
    peas_engine_enable_loader(engine, "lua5.1");
    FeaturePluginTest::CreateApiInterface("feature_plugin-lua");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    FeaturePluginTest::TearDown();
  }
};

/*
 * Fixture for testing the Python plugin
 */
class FeaturePluginPythonTest : public FeaturePluginTest {
  virtual void SetUp() {
    /* Call the parent SetUP() */
    FeaturePluginTest::SetUp();
    peas_engine_enable_loader(engine, "python3");
    FeaturePluginTest::CreateApiInterface("feature_plugin-python");
  }
  
  virtual void TearDown() {
    g_object_unref(extension);
    /* Call the parent tear down routine */
    FeaturePluginTest::TearDown();
  }
};

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1a) {
  test_feature_plugin_api_example1a();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1a) {
  test_feature_plugin_api_example1a();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1a) {
  test_feature_plugin_api_example1a();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1b) {
  test_feature_plugin_api_example1b();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1b) {
  test_feature_plugin_api_example1b();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1b) {
  test_feature_plugin_api_example1b();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1c) {
  test_feature_plugin_api_example1c();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1c) {
  test_feature_plugin_api_example1c();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1c) {
  test_feature_plugin_api_example1c();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1d) {
  test_feature_plugin_api_example1d();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1d) {
  test_feature_plugin_api_example1d();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1d) {
  test_feature_plugin_api_example1d();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1e) {
  test_feature_plugin_api_example1e();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1e) {
  test_feature_plugin_api_example1e();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1e) {
  test_feature_plugin_api_example1e();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample1f) {
  test_feature_plugin_api_example1f();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample1f) {
  test_feature_plugin_api_example1f();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample1f) {
  test_feature_plugin_api_example1f();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample2a) {
  test_feature_plugin_api_example2a();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample2a) {
  test_feature_plugin_api_example2a();
}

TEST_F(FeaturePluginLuaTest, FeaturePluginLuaApiExample2a) {
  test_feature_plugin_api_example2a();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample2b) {
  test_feature_plugin_api_example2b();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample2b) {
  test_feature_plugin_api_example2b();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample2c) {
  test_feature_plugin_api_example2c();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample2c) {
  test_feature_plugin_api_example2c();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample2d) {
  test_feature_plugin_api_example2d();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample2d) {
  test_feature_plugin_api_example2d();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample6a) {
  test_feature_plugin_api_example6a();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample6a) {
  test_feature_plugin_api_example6a();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample6b) {
  test_feature_plugin_api_example6b();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample6b) {
  test_feature_plugin_api_example6b();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample6c) {
  test_feature_plugin_api_example6c();
}

TEST_F(FeaturePluginPythonTest, DISABLED_FeaturePluginPythonApiExample6c) {
  test_feature_plugin_api_example6c();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample7a) {
  test_feature_plugin_api_example7a();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample7a) {
  test_feature_plugin_api_example7a();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample7b) {
  test_feature_plugin_api_example7b();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample7b) {
  test_feature_plugin_api_example7b();
}

TEST_F(FeaturePluginCTest, FeaturePluginCApiExample7c) {
  test_feature_plugin_api_example7c();
}

TEST_F(FeaturePluginPythonTest, FeaturePluginPythonApiExample7c) {
  test_feature_plugin_api_example7c();
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
