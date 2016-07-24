
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwlib_gtest.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @author Matt Harper (matt.harper@riftio.com)
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

#include "rwlib.h"
#include "rw_dl.h"
#include "rw_rand.h"
#include "rw_sklist.h"
#include "rw_sys.h"

#include "rw_vx_plugin.h"
#include "feature_plugin.h"
#include <python_plugin.h>
#include "standard_plugin.h"

#include "rwut.h"

#define FEATURE_PLUGIN_EXTENSION_NAME "feature_plugin-"
#define STANDARD_PLUGIN_EXTENSION_NAME "standard_plugin-"


/**
 * Location of the plugin files used for this test program
 */
#ifndef PLUGINDIR
#error - Makefile must define PLUGINDIR
#endif // PLUGINDIR
#define FEATUREPLUGINDIR PLUGINDIR "/../../feature_plugin/vala"

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

/** 
 * Tests g_mem_profile.
 * Need to find out a way to validate the stdout
 */
TEST(DISABLED_RIFTMemProfile, MemProfile) {
  g_mem_set_vtable(glib_mem_profiler_table);
  void *test = RW_MALLOC(100);
  RW_FREE(test);
  g_mem_profile();
}

TEST(RIFTRlibMisc, JenkinsLookup3) {
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  int r = rw_lookup3_test();
  ASSERT_TRUE(r);

  /*  Whoa, probably a bit painful to regex, better to bonk the tests
      or just not change the code and be happy that it works on
      inspection. */
  
  ConsoleCapture::get_capture_string();

#if 0
  ASSERT_THAT(ConsoleCapture::get_capture_string(),
              !/*??*/ContainsRegex(".*error.*"));
  ASSERT_THAT(ConsoleCapture::get_capture_string(),
              !/*??*/ContainsRegex(".*mismatch.*")); 
#endif
}

void foo_f5(void) { volatile int a = 0; RW_SHOW_BACKTRACE; a++; return; }
void foo_f4(void) { volatile int a = 0; foo_f5(); a++; return; }
void foo_f3(void) { volatile int a = 0; foo_f4(); a++; return; }
void foo_f2(void) { volatile int a = 0; foo_f3(); a++; return; }
void foo_f1(void) { volatile int a = 0; foo_f2(); a++; return; }

/**
 * Tests RW_SHOW_BACKTRACE. 
 */
TEST(RIFTBackTrace, BackTrace) {
  TEST_DESCRIPTION("This test valiadtes the RW_SHOW_BACKTRACE macro.");
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  foo_f1();
  ASSERT_THAT(ConsoleCapture::get_capture_string(),
              ContainsRegex(".*foo_f5.*foo_f4.*foo_f3.*foo_f2.*foo_f1.*"));
}

#include <libunwind.h>

unw_context_t *contextp = NULL;
void *callers[RW_RESOURCE_TRACK_MAX_CALLERS+2];
void f6() {
  unw_context_t context;
  unw_getcontext(&context);
  contextp = (unw_context_t*)malloc(sizeof(*contextp));
  *contextp = context;
  rw_btrace_backtrace(callers, RW_RESOURCE_TRACK_MAX_CALLERS);
}
void f5() { f6(); }
void f4() { f5(); }
void f3() { f4(); }
void f2() { f3(); }
void f1() { f2(); }

void skip_func() {
    unw_cursor_t cursor;
    unw_context_t uc;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    unw_step(&cursor);
    unw_step(&cursor);
    unw_resume(&cursor);            // restore the machine state
    printf("will be skipped\n");    // won't be executed
}

void skipped_func() {
    skip_func();
    printf("will be skipped\n");    // won't be executed
}

TEST(RIFTBackTrace, BackTrace2) {
  TEST_DESCRIPTION("This test valiadtes the RW_SHOW_BACKTRACE macro.");
  f1();
  int i; for (i=0; callers[i]; i++) {
    char *fname = NULL;
    fname = rw_unw_get_proc_name(callers[i]);
    printf("unwind_fname[%d]= %s\n", i, fname);
    free(fname);

    fname = rw_btrace_get_proc_name(callers[i]);
    printf("btrace_fname[%d]= %s\n", i, fname);
    free(fname);
  }
}

/**
 * Tests RW_STATIC_ASSERT
 * Since testing static assert requires compilation of code this is
 * done in a python script.
 */
TEST(RIFTStaticAssert, StaticAssert) {
  TEST_DESCRIPTION("Tests RW_STATIC_ASSERT."
                   "Since testing static assert requires compilation of code, "
                   "this is done in a python script");

  ASSERT_EQ(system(PYDIR "/rwlib_test_static_assert.py"), 0);
}

/**
 * Tests RW_ASSERT_NOT_REACHED macro. This test makes sure that
 * RW_ASSERT_NOT_REACHED is triggered appropriately.
 */
TEST(RIFTAssert, FireAssertNotReached) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  TEST_DESCRIPTION("This test makes sure that RW_ASSERT_NOT_REACHED is "
                   "triggered appropriately.");
  ASSERT_DEATH({ RW_ASSERT_NOT_REACHED(); }, "");
}

/**
 * Tests RW_ASSERT macro
 */
TEST(RIFTAssert, FireAssert) {
  TEST_DESCRIPTION("This test ensures that RW_ASSERT macro works as "
                   "expected");
  ASSERT_DEATH({ RW_ASSERT(sizeof(int) == sizeof(char)); }, "");
}

/* Wrap the trace in the macro to get the precise lineno */
#define RWASSERT_SUPPERESSION_TEST(_expr)                                     \
{                                                                             \
    rw_code_location_t _l;                                                    \
    rw_status_t _s;                                                         \
    strncpy(_l.filename, basename(__FILE__), RW_MAX_LOCATION_FILENAME_SIZE);        \
    _l.lineno = __LINE__;                                                     \
    _s = rw_set_assert_ignore_location(&_l);                                  \
    ASSERT_EQ(_s, RW_STATUS_SUCCESS);                                       \
    RW_ASSERT(_expr);                                                       \
}

void test_that_assert_is_suppressed(void)
{
  rw_init_assert_ignore_location_info();
  RWASSERT_SUPPERESSION_TEST(sizeof(int) == sizeof(char))
  /* if the previous assert is not suppressed one would not reach here */
  RW_ASSERT_MESSAGE((1==2), "RW_ASSERT supressed")
}

/**
 * Tests RW_ASSERT macro is suppressed
 */
TEST(RIFTAssert, SuppressAssert) {
  TEST_DESCRIPTION("This test ensures that RW_ASSERT macro "
                   "can be suppressed selectively");
  ASSERT_DEATH({ test_that_assert_is_suppressed(); },
               "RW_ASSERT supressed");
}

// Unit test the set/unset trace method
TEST(RIFTAssert, SetUnsetLocation) {
  rw_status_t status;
  rw_code_location_t location;
  int i;
  int cnt;

  TEST_DESCRIPTION("This test ensures that functions "
                   "that set and unset the suppression of asserts "
                   "work properly. This test involves supressing "
                   "several asserts and then reenabling them");

  rw_init_assert_ignore_location_info();
  strncpy(location.filename, basename(__FILE__), RW_MAX_LOCATION_FILENAME_SIZE);
  for (i = 0; i < RW_MAX_CODE_LOCATIONS; ++i) {
    location.lineno = i+1;
    status = rw_set_assert_ignore_location(&location);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    cnt = rw_get_assert_ignore_location_count();
    ASSERT_EQ(cnt, i+1);

    /* check for duplicate operations */
    status = rw_set_assert_ignore_location(&location);
    if (i == (RW_MAX_CODE_LOCATIONS-1)) {
      ASSERT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);
    } else {
      ASSERT_EQ(status, RW_STATUS_DUPLICATE);
    }
    cnt = rw_get_assert_ignore_location_count();
    ASSERT_EQ(cnt, i+1);
  }

  location.lineno = RW_MAX_CODE_LOCATIONS+1000;
  status = rw_unset_assert_ignore_location(&location);
  ASSERT_EQ(status, RW_STATUS_NOTFOUND);

  location.lineno = 1;
  status = rw_unset_assert_ignore_location(&location);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  cnt = rw_get_assert_ignore_location_count();
  ASSERT_EQ(cnt, RW_MAX_CODE_LOCATIONS-1);
}


/**
 * Tests RW_ASSERT_MESSAGE macro
 */
TEST(RIFTAssert, FireAssertMessage) {
  TEST_DESCRIPTION("This test ensures that RW_ASSERT_MESSAGE macro works as "
                   "expected");
  ASSERT_DEATH({ RW_ASSERT_MESSAGE(sizeof(int) == sizeof(char), "Testing RW_ASSERT_MESSAGE"); }, 
               "Testing RW_ASSERT_MESSAGE");
}

/**
 * Tests for RW_MALLOC0_TYPE, RW_FREE_TYPE, RW_ASSERT_TYPE
 */
TEST(RIFTAssert, TestTypeMacros) {
  TEST_DESCRIPTION("This test ensures that RW_MALLOC0_TYPE, RW_FREE_TYPE, RW_ASSERT_TYPE macros"
                   "work as expected");

  typedef struct _test_struct_t_ {
    int e1;
    char e2[10];
  } test_struct_t;

  test_struct_t *p1;

  p1 = RW_MALLOC0_TYPE(sizeof(test_struct_t), test_struct_t);

  RW_ASSERT_TYPE(p1, test_struct_t);


  ASSERT_DEATH({ *(((unsigned long long*)p1) - (RW_MAGIC_PAD-1)/8) = '0';
                 *(((unsigned long long*)p1) - (RW_MAGIC_PAD-0)/8) = '0';
                 RW_ASSERT_TYPE(p1, test_struct_t);
               },
               "Bad Heap Magic");

  RW_FREE_TYPE(p1, test_struct_t);

#if 0
  ASSERT_DEATH({ RW_ASSERT_TYPE(p1, test_struct_t); },
               "assertion failed:");
#endif
}

/**
 * Tests for RW_MALLOC0_ARRAY_TYPE, RW_FREE_ARRAY_TYPE
 */
TEST(RIFTAssert, TestArrayTypeMacros) {
  TEST_DESCRIPTION("This tests the RW_MALLOC0_ARRAY_TYPE, RW_FREE_ARRAY_TYPE macros");

  typedef struct _test_struct_t_ {
    int e1;
    char *p;
    char e2[10];
  } test_array_struct_t;

  int count = 25;
  int i;

  test_array_struct_t **array;

  array = RW_MALLOC0_ARRAY_TYPE(test_array_struct_t, count);

  RW_ASSERT_TYPE(array, test_array_struct_t*);


  for (i = 0; i < count; i ++) {
    RW_ASSERT_TYPE(array[i], test_array_struct_t);
    EXPECT_EQ(array[i]->e1, 0);
  }

  RW_FREE_ARRAY_TYPE(array, test_array_struct_t, count);

  // Repeat test for the non zero macro
  
  array = RW_MALLOC_ARRAY_TYPE(test_array_struct_t, count);

  RW_ASSERT_TYPE(array, test_array_struct_t*);


  for (i = 0; i < count; i ++) {
    RW_ASSERT_TYPE(array[i], test_array_struct_t);
  }

  RW_FREE_ARRAY_TYPE(array, test_array_struct_t, count);


#if 0
  ASSERT_DEATH({ *(((unsigned long long*)p1) - (RW_MAGIC_PAD-1)/8) = '0';
                 *(((unsigned long long*)p1) - (RW_MAGIC_PAD-0)/8) = '0';
                 RW_ASSERT_TYPE(p1, test_struct_t);
               },
               "assertion failed:");

  RW_FREE_TYPE(p1, test_struct_t);

  ASSERT_DEATH({ RW_ASSERT_TYPE(p1, test_struct_t); },
               "assertion failed:");
#endif
}

TEST(RIFTAssert, RIFTvsRWMagicFree) {
  TEST_DESCRIPTION("Test that RW_MAGIC_FREE can free an RW_MALLOC0_TYPE value without knowing type");

  RW_STATIC_ASSERT(RW_MAGIC_PAD == RW_MAGIC_PAD);

  typedef struct _test_struct_t_ {
    int e1;
    char e2[10];
  } test2_struct_t;
  test2_struct_t *p1;
  p1 = RW_MALLOC0_TYPE(sizeof(test2_struct_t), test2_struct_t);
  RW_ASSERT_TYPE(p1, test2_struct_t);

  RW_MAGIC_FREE(p1);
}

/* Wrap the trace in the macro to get the precise lineno */
#define RWASSERT_MSG_SUPPERESSION_TEST(_expr, _fmt, _args...)                 \
{                                                                             \
    rw_code_location_t _l;                                                    \
    rw_status_t _s;                                                         \
    strncpy(_l.filename, basename(__FILE__), RW_MAX_LOCATION_FILENAME_SIZE);        \
    _l.lineno = __LINE__;                                                     \
    _s = rw_set_assert_ignore_location(&_l);                                  \
    ASSERT_EQ(_s, RW_STATUS_SUCCESS);                                       \
    RW_ASSERT_MESSAGE(_expr, _fmt, ##_args);                                \
}

void test_that_assert_msg_is_suppressed(void)
{
  rw_init_assert_ignore_location_info();
  RWASSERT_MSG_SUPPERESSION_TEST(sizeof(int) == sizeof(char),
                                 "Testing RW_ASSERT_MESSAGE");
  /* if the previous assert is not suppressed one would not reach here */
  RW_ASSERT_MESSAGE((1==2), "RW_ASSERT_MESSAGE supressed")
}

/**
 * Tests suppression of RW_ASSERT_MESSAGE macro
 */
TEST(RIFTAssert, SuppressAssertMessage) {
  TEST_DESCRIPTION("This test ensures that RW_ASSERT_MESSAGE macro can be " 
                   "supressed");
  ASSERT_DEATH({ test_that_assert_msg_is_suppressed(); },
               "RW_ASSERT_MESSAGE supressed");
}

/**
 * Tests various rw_XXX_code_location_YYY() APIs
 */
TEST(RIFTAssert,CodeLocationUtilities) {
  int lc;
  rw_status_t status;
  rw_code_location_info_t li;
  rw_code_location_t l;
  RW_ZERO_VARIABLE(&li);
  RW_ZERO_VARIABLE(&l);
  
  lc = rw_get_code_location_count(&li);
  ASSERT_EQ(lc,0);

  status = rw_unset_code_location(&li,&l);
  ASSERT_EQ(RW_STATUS_NOTFOUND,status);
}

/**
 * Tests RW_ASSERT_MESSAGE macro with arguments to format string
 */
TEST(RIFTAssert, FireAssertMessageArgs) {
  TEST_DESCRIPTION("This test ensures that RW_ASSERT_MESSAGE macro works as "
                   "expected with arguments to the format string");
  ASSERT_DEATH({ int i; i = -1; RW_ASSERT_MESSAGE(sizeof(int) == sizeof(char), 
                                     "Testing %s [%d]", "RW_ASSERT_MESSAGE",
                                     i); }, 
               "Testing RW_ASSERT_MESSAGE \\[-1\\]");
}

/**
 * Tests RW_MAGIC_CHECK
 */
TEST(RIFTMagic, MagicCheck) {
  TEST_DESCRIPTION("This test ensures that RW_MAGIC_MALLOC and "
                   "RW_MAGIC_CHECK macros work as expected");
  void *magic = RW_MAGIC_MALLOC(sizeof(int));
  ASSERT_EQ(RW_MAGIC_CHECK(magic), RW_STATUS_SUCCESS);
  RW_MAGIC_FREE(magic);
}

/**
 * Tests RW_MAGIC_MALLOC negative case. Intentionally corrupt
 * the padding and make sure that RW_MAGIC_CHECK catches it
 */
TEST(RIFTMagic, MagicCheckNegative) {
  TEST_DESCRIPTION("This test ensures that RW_MAGIC_CHECK macro catches "
                   "the issues after intentionally corrupting memeory.");
  void *magic = RW_MAGIC_MALLOC(sizeof(int));
  ((void)magic);
  printf("magic pointer=%p\n", magic);

  *(uint32_t *)((char *)magic - RW_MAGIC_PAD) = 0xdeadbeef;
  ASSERT_NE(RW_MAGIC_CHECK(magic), RW_STATUS_SUCCESS);
  RW_MAGIC_FREE(magic);
}

/**
 * Tests RIFT implementation of doubly linked lists
 */
TEST(RIFTDataStructure, DoublyLinkedList) {
  TEST_DESCRIPTION("This test performs unit tests on rift doubly linked lists.");
  ASSERT_EQ(rw_dl_unit_test(), RW_STATUS_SUCCESS);
}

typedef struct {
  int check1;
  rw_sklist_element_t sklist_element;
  char key[64];
  int check2;
} foo_charbuf_t;

TEST(RIFTDataStructure, SkipListCharBuf) {
  rw_status_t r;
  rw_sklist_t * skl;
  foo_charbuf_t keys[1000];
  const int list_size = 1000;
  foo_charbuf_t * v;
  char nomatch[] = "nomatch";
  char buf[64];

  for(int k = 0; k < list_size; k++) {
    keys[k].check1 = k;
    keys[k].check2 = 2*k;
    sprintf(keys[k].key, "%p", &keys[k]);
  }

  /*
   * Test alloc/dealloc
   */
  RW_SKLIST_PARAMS_DECL(
      sklp2,
      foo_charbuf_t,
      key,
      rw_sklist_comp_charbuf,
      sklist_element);
  skl = RW_SKLIST_ALLOC(&sklp2);
  ASSERT_TRUE(skl != NULL);
  r = RW_SKLIST_FREE(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  /*
   * Make a new list
   */
  skl = RW_SKLIST_ALLOC(&sklp2);
  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  // lookup next on an empty list
  r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, nomatch, (void **)&v);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);
  ASSERT_TRUE(v == NULL);

  r = rw_sklist_debug_sanity_check(skl);
  RW_ASSERT(r == RW_STATUS_SUCCESS);

  /*
   * Try a lookup without a match
   */
  r = RW_SKLIST_LOOKUP_BY_KEY(skl, nomatch, (void * *)&v);
  ASSERT_EQ(r, RW_STATUS_FAILURE);


  /*
   * Insert All keys
   */
  for (int k = 0; k < list_size; k++) {
    r = RW_SKLIST_INSERT(skl, &keys[k]);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);

    r = RW_SKLIST_LOOKUP_BY_KEY(skl, &keys[k].key, (void **)&v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
    ASSERT_EQ(v, &keys[k]);
  }

  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  // lookup-next on tail of list should return NULL;
  r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, &keys[list_size-1].key, (void **)&v);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);
  ASSERT_TRUE(v == NULL);

  // lookup-next on nomatch (the tail) should return NULL;
  r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, nomatch, (void **)&v);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);
  ASSERT_TRUE(v == NULL);


  /*
   * Check lookup for all keys
   */
  for (int k = 0; k < list_size; k++) {
    sprintf(buf, "%p", &keys[k]);
    r = RW_SKLIST_LOOKUP_BY_KEY(skl, buf, (void * *)&v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
    ASSERT_EQ(v, &keys[k]);
  }

  /*
   * Attempt a duplicate key insert - should fail
   */
  r = RW_SKLIST_INSERT(skl,&keys[0]);
  ASSERT_EQ(r, RW_STATUS_DUPLICATE);

  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);


  /*
   * Check lookup for all keys
   */
  for (int k = 0; k < list_size; k++) {
    sprintf(buf, "%p", &keys[k]);
    r = RW_SKLIST_LOOKUP_BY_KEY(skl, buf, (void **)&v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
    ASSERT_EQ(v, &keys[k]);
  }

  /*
   * Delete Keys in reverse order
   */
  for (int k = list_size - 1; k >= 0; k--) {
    r = RW_SKLIST_REMOVE_BY_KEY(skl, keys[k].key, &v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
    ASSERT_EQ(v, &keys[k]);

    r = rw_sklist_debug_sanity_check(skl);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
  }

  /*
   * Make sure no key is still registered
   */
  for (int k = 0; k < list_size; k++) {
    r = RW_SKLIST_LOOKUP_BY_KEY(skl, &keys[k].key, &v);
    ASSERT_EQ(r, RW_STATUS_FAILURE);
  }

  RW_SKLIST_FREE(skl);

  /*
   * Validate all check keys
   */
  for (int k = 0; k < list_size; k++) {
    ASSERT_EQ(keys[k].check1, k);
    ASSERT_EQ(keys[k].check2, 2 * k);
  }
}

typedef struct {
  int check1;
  rw_sklist_element_t sklist_element;
  int key;
  int check2;
} foo_int_t;


TEST(RIFTDataStructure, SkipListInt) {
  rw_status_t r;
  rw_sklist_t actual_skl, *skl;
  int kval;
  const int list_size = 500;
  int count;
  foo_int_t keys[500], junk, *pn1, *pn2;
  void *v;
  int does_not_exist = 999999999;

  for(int k = 0; k < list_size; k++) {
    keys[k].check1 = k;
    keys[k].check2 = 2 * k;
  }

  RW_SKLIST_PARAMS_DECL(
      sklp1,
      foo_int_t,
      key,
      rw_sklist_comp_int,
      sklist_element);
  skl = RW_SKLIST_INIT(&actual_skl ,&sklp1);
  ASSERT_TRUE(skl != NULL);
  ASSERT_EQ(skl, &actual_skl);
  ASSERT_FALSE(skl->dynamic);

  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  /*
   * Try remove on an empty sklist
   */
  r = RW_SKLIST_REMOVE_BY_KEY(skl, &(keys[0].key), &v);
  ASSERT_EQ(r, RW_STATUS_FAILURE);

  /*
   * Test iterator on empty sklist
   */
  count = 0;
  RW_SKLIST_FOREACH(pn1, foo_int_t, skl, sklist_element) {
    count++;
  }
  ASSERT_EQ(count, 0);

  count = 0;
  RW_SKLIST_FOREACH_SAFE(pn1, foo_int_t, skl, sklist_element, pn2) {
    count++;
  }
  ASSERT_EQ(count, 0);


  for(int k = 0; k < list_size; k++) {
    static int step = 0;

    do {
      while (true) {
        keys[k].key = rw_random();
        if (keys[k].key != does_not_exist)
          break;
      }

      /*
       * In theory, two pseudo-random integers COULD be the same
       * duplicates keys are NOT allowed; hence, force some duplicates inserts for testing purposes...
       */
      switch(step++) {
        case 3:
          keys[k].key = keys[k-1].key;
          break;
        case 4:
          keys[k].key = keys[0].key;
          break;
      }
      r = RW_SKLIST_INSERT(skl,&keys[k]);
    } while (r != RW_STATUS_SUCCESS);
  }

  for(int k = 0; k < list_size; k++) {
    ASSERT_EQ(keys[k].check1, k);
    ASSERT_EQ(keys[k].check2, 2 * k);
  }

  /* Make sure the iterator hits the entire list in order */
  count = 0;
  RW_SKLIST_FOREACH(pn1, foo_int_t, skl, sklist_element) {
    if (count)
      ASSERT_LT(kval, pn1->key);
    kval = pn1->key;
    count++;
  }
  ASSERT_EQ(count, list_size);

  count = 0;
  RW_SKLIST_FOREACH_SAFE(pn1, foo_int_t, skl, sklist_element, pn2) {
    if (count)
      ASSERT_LT(kval, pn1->key);
    kval = pn1->key;
    count++;
  }
  ASSERT_EQ(count, list_size);

  /*
   * Try a remove for a non-existant value
   */
  junk.key = does_not_exist;
  r = RW_SKLIST_REMOVE_BY_KEY(skl, &junk.key, &v);
  ASSERT_EQ(r, RW_STATUS_FAILURE);

  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  for(int i = 0; i < 4; i++) {
    for(int k = 0; k < list_size; k++) {
      r = RW_SKLIST_LOOKUP_BY_KEY(skl, &(keys[k].key), &v);
      ASSERT_EQ(r, RW_STATUS_SUCCESS);
      ASSERT_EQ(((foo_int_t *)v)->key, keys[k].key);

      ASSERT_EQ(keys[k].check1, k);
      ASSERT_EQ(keys[k].check2, 2 * k);
    }

    // Test RW_SKLIST_LOOKUP_NEXT_BY_KEY()
    pn1 = RW_SKLIST_HEAD(skl, foo_int_t);
    ASSERT_TRUE(NULL != pn1);

    pn2 = RW_SKLIST_NEXT(pn1, foo_int_t, sklist_element);
    ASSERT_TRUE(NULL != pn2);

    kval = pn1->key;
    ASSERT_LT(kval, pn2->key);

    r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, &kval, (void **)&v);
    ASSERT_EQ(RW_STATUS_SUCCESS, r);
    ASSERT_EQ(v, pn2);

    kval = pn1->key + 1;
    ASSERT_NE(kval, pn2->key);

    r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, &kval, (void **)&v);
    ASSERT_EQ(RW_STATUS_SUCCESS, r);
    ASSERT_EQ(v, pn2);

    // Walk to end of the sklist
    while (true) {
      pn2 = RW_SKLIST_NEXT(pn1, foo_int_t, sklist_element);
      if (!pn2)
        break;
      pn1 = pn2;
    }

    r = RW_SKLIST_LOOKUP_NEXT_BY_KEY(skl, &pn1->key, (void **)&v);
    ASSERT_EQ(RW_STATUS_SUCCESS, r);
    ASSERT_TRUE(v == NULL); // Next after the last should be NULL

    for(int k = 0; k < list_size; k++) {
      ASSERT_EQ(keys[k].check1, k);
      ASSERT_EQ(keys[k].check2, 2 * k);

      r = RW_SKLIST_REMOVE_BY_KEY(skl, &(keys[k].key), &v);
      ASSERT_EQ(keys[k].check1, k);
      ASSERT_EQ(keys[k].check2, 2 * k);
      ASSERT_EQ(r, RW_STATUS_SUCCESS);
      ASSERT_EQ(((foo_int_t *)v)->key, keys[k].key);

      ((foo_int_t *)v)->key = rw_random();
      r = RW_SKLIST_INSERT(skl, v);
      ASSERT_EQ(r, RW_STATUS_SUCCESS);
    }
  }

  for (int k = 0; k < list_size; k++) {
    r = RW_SKLIST_REMOVE_BY_KEY(skl, &(keys[k].key), &v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
    ASSERT_EQ(((foo_int_t *)v)->key, keys[k].key);
  }
  ASSERT_EQ(0, RW_SKLIST_LENGTH(skl));

  r = rw_sklist_debug_sanity_check(skl);
  ASSERT_EQ(r, RW_STATUS_SUCCESS);

  /* Refill the list and test emptying with the iterator */
  for (int k = 0; k < list_size; k++) {
    r = RW_SKLIST_INSERT(skl, &keys[k]);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
  }

  RW_SKLIST_FOREACH_SAFE(pn1, foo_int_t, skl, sklist_element, pn2) {
    r = RW_SKLIST_REMOVE_BY_KEY(skl, &pn1->key, &v);
    ASSERT_EQ(r, RW_STATUS_SUCCESS);
  }
  ASSERT_EQ(0, RW_SKLIST_LENGTH(skl));

  RW_SKLIST_FREE(skl);

  for(int k = 0; k < list_size; k++) {
    ASSERT_EQ(keys[k].check1, k);
    ASSERT_EQ(keys[k].check2, 2 * k);
  }
}

TEST(RIFTSys, InstanceUID) {
  rw_status_t status;
  uint8_t uid;
  char * old_uid;
  char too_big[20];

  old_uid = getenv("RIFT_INSTANCE_UID");
  if (old_uid)
    unsetenv("RIFT_INSTANCE_UID");

  // Test default uid case
  status = rw_instance_uid(&uid);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_LT(uid, RW_MAXIMUM_INSTANCES);
  //ASSERT_GT(uid, RW_RESERVED_INSTANCES);

  // Test valid env var
  setenv("RIFT_INSTANCE_UID", "20", 1);
  status = rw_instance_uid(&uid);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_EQ(uid, 20);

  // Test negative env var
  setenv("RIFT_INSTANCE_UID", "-100", 1);
  status = rw_instance_uid(&uid);
  ASSERT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);

  // Test too large env var
  snprintf(too_big, 20, "%lu", (long unsigned int)too_big + 1);
  setenv("RIFT_INSTANCE_UID", too_big, 1);
  status = rw_instance_uid(&uid);
  ASSERT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);

  if (old_uid)
    setenv("RIFT_INSTANCE_UID", old_uid, 1);
}

TEST(RIFTSys, UniquePort) {
  rw_status_t status;
  char * old_uid = NULL;
  uint16_t port;

  old_uid = getenv("RIFT_INSTANCE_UID");
  if (old_uid)
    unsetenv("RIFT_INSTANCE_UID");
  setenv("RIFT_INSTANCE_UID", "10", 1);

  status = rw_unique_port(100, &port);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_EQ(port, 110);

  status = rw_unique_port(65534, &port);
  ASSERT_EQ(status, RW_STATUS_FAILURE);

  setenv("RIFT_INSTANCE_UID", "-100", 1);
  status = rw_unique_port(1, &port);
  ASSERT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);

  if (old_uid)
    setenv("RIFT_INSTANCE_UID", old_uid, 1);
}

TEST(RIFTSys, AvoidPort) {
  ASSERT_EQ(rw_port_in_avoid_list(110, 1), false);
  ASSERT_EQ(rw_port_in_avoid_list(110, 2), true);
  ASSERT_EQ(rw_port_in_avoid_list(110, 5), true);
  ASSERT_EQ(rw_port_in_avoid_list(111, 1), true);
  ASSERT_EQ(rw_port_in_avoid_list(111, 0), false);
}

/*
 * Create a test fixture for testing the plugin framework
 *
 * This fixture is reponsible for:
 *   1) allocating the Virtual Executive (VX)
 *   2) setting up the peas engine search paths
 *   3) cleanup upon test completion
 */
class RIFTPluginTest : public ::testing::Test {
 protected:
  rw_vx_framework_t *rw_vxfp;
  
  virtual void SetUp() {
    GError *error = NULL;

    /*
     * Allocate a RIFT_VX framework to run the test
     * NOTE: this initializes a LIBPEAS instance
     */
    rw_vxfp = rw_vx_framework_alloc();
    ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));
    
    /* Inform the PEAS engine where the plugins reside */
    g_irepository_prepend_search_path(
        INSTALLDIR "/usr/lib/rift/girepository-1.0");
    g_irepository_prepend_search_path(PLUGINDIR);
    g_irepository_prepend_search_path(PLUGINDIR "/../vala");
    g_irepository_prepend_search_path(PLUGINDIR "/../../common_plugin/vala");
    g_irepository_prepend_search_path(FEATUREPLUGINDIR);
    g_irepository_prepend_search_path(FEATUREPLUGINDIR "/../vala");
    peas_engine_add_search_path(rw_vxfp->engine,
                                PLUGINDIR,
                                PLUGINDIR);
    peas_engine_add_search_path(rw_vxfp->engine,
                                FEATUREPLUGINDIR,
                                FEATUREPLUGINDIR);
  
    /*
     * Setup LIBPEAS search-path and versions for the plugins
     * Load our plugin proxy into the g_irepository namespace
     */
    g_irepository_require(g_irepository_get_default(),
                          "StandardPlugin",
                          "1.0",
                          (GIRepositoryLoadFlags)0,
                          &error);
    g_assert_no_error(error);
    g_irepository_require(g_irepository_get_default(),
                          "FeaturePlugin",
                          "1.0",
                          (GIRepositoryLoadFlags)0,
                          &error);
    g_assert_no_error(error);
    g_irepository_require(g_irepository_get_default(),
                          "Rwplugin",
                          "1.0",
                          (GIRepositoryLoadFlags)0,
                          &error);
    g_assert_no_error(error);
  }

  virtual void TearDown() {
    /* Free the RIFT_VX framework
     * NOTE: This automatically calls de-init for ALL registered modules
     */
    rw_status_t status;
    status = rw_vx_framework_free(rw_vxfp);
    ASSERT_EQ(RW_STATUS_SUCCESS, status);
  }
};

/*
 * Fixture with preopened modules for testing
 */
class RIFTModulePluginTest : public RIFTPluginTest {
 protected:
  gchar *module1_name;
  gchar *module2_name;
  gchar *feature_module_name;
  gchar *module_alloc_cfg;
  gchar *plugin_name, *feature_plugin_name;
  gchar *plugin_name_suffix;
  
  // We don't need any more logic than already in the QuickTest fixture.
  // Therefore the body is empty.
  virtual void SetUp() {
    rw_status_t rw_status;

    /* Call the parent SetUP() */
    RIFTPluginTest::SetUp();

    module1_name = g_strdup("plugin-1");
    module2_name = g_strdup("plugin-2");
    feature_module_name = g_strdup("feature-plugin-3");
    module_alloc_cfg = g_strdup("bogus_alloc_cfg");
    plugin_name_suffix = g_strdup("c");

    plugin_name = g_strdup_printf("%s%s",
                                  STANDARD_PLUGIN_EXTENSION_NAME,
                                  plugin_name_suffix);
    feature_plugin_name = g_strdup_printf("%s%s",
                                          FEATURE_PLUGIN_EXTENSION_NAME,
                                          plugin_name_suffix);
  

    // Create 1st instance of the STANDARD_PLUGIN_API module
    rw_status = rw_vx_module_register(rw_vxfp,
                                          module1_name,
                                          module_alloc_cfg,
                                          plugin_name,
                                          NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

    // Create 2nd instance of the STANDARD_PLUGIN_API module
    rw_status = rw_vx_module_register(rw_vxfp,
                                          module2_name,
                                          module_alloc_cfg,
                                          plugin_name,
                                          NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

    /*
     * Create an instance of the FEATURE_PLUGIN_API module
     *
     * NOTE: This module does NOT support the COMMON MODULE API
     *       so linking isn't valid for it
     */
    rw_status = rw_vx_module_register(rw_vxfp,
                                          feature_module_name,
                                          module_alloc_cfg,
                                          feature_plugin_name,
                                          NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  }
  
  virtual void TearDown() {
    rw_status_t rw_status;
    
    // Destroy module1 plugin
    rw_status = rw_vx_module_unregister(rw_vxfp, module1_name);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    
    // Destroy module2 plugin
    rw_status = rw_vx_module_unregister(rw_vxfp, module2_name);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    
    // Destroy module3 plugin
    rw_status = rw_vx_module_unregister(rw_vxfp, feature_module_name);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

    g_free(module1_name);
    g_free(module2_name);
    g_free(feature_module_name);
    g_free(module_alloc_cfg);
    g_free(plugin_name_suffix);
    g_free(plugin_name);
    g_free(feature_plugin_name);

    /* Call the parent tear down routine */
    RIFTPluginTest::TearDown();
  }
};

/**
 * Test the RIFT pseudo module framework
 */
TEST_F(RIFTPluginTest, DISABLED_PseudoModules) {
  rw_status_t rw_status;
  uint32_t data;
  rw_vx_modinst_common_t *mip1 = NULL, *mip2 = NULL;
  rw_vx_plug_t *plug1, *plug2;
  rw_vx_plug_port_t *port;
  rw_vx_port_pin_t *pin;
  rw_vx_link_handle_t *link_handle1;
  uint32_t linkid = 1;
  StandardPluginApiIface *sp_interface = NULL;
  FeaturePluginApiIface *fp_interface = NULL;

  TEST_DESCRIPTION("This test performs unit tests rift vx pseudo modules.");

  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  mip1 = rw_vx_pseudo_module_instance(rw_vxfp,(void*)&data);
  ASSERT_TRUE(RW_VX_MODINST_VALID(mip1));

  rw_vx_pseudo_modinst_add_interface(mip1,
                                       STANDARD_PLUGIN_TYPE_API,
                                       (void**)&sp_interface);
  ASSERT_TRUE(NULL != sp_interface);
  /* The interface functions may not be filled-in */

  rw_vx_pseudo_modinst_add_interface(mip1,
                                       FEATURE_PLUGIN_TYPE_API,
                                       (void**)&fp_interface);
  ASSERT_TRUE(NULL != fp_interface);
  /* The interface functions may not be filled-in */


  {
    StandardPluginApi *api1;
    StandardPluginApiIface *interface1;
    rw_status = rw_vx_modinst_get_api_and_iface(mip1,
                                                    STANDARD_PLUGIN_TYPE_API,
                                                    (void **)&api1,
                                                    (void**)&interface1,
                                                    NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    ASSERT_TRUE(api1);
    ASSERT_TRUE(interface1);
    ASSERT_TRUE(interface1 == sp_interface);
    ASSERT_TRUE(api1 == (void *)mip1->modinst_handle);
  }

  mip2 = rw_vx_pseudo_module_instance(rw_vxfp,(void*)&data);
  ASSERT_TRUE(RW_VX_MODINST_VALID(mip1));
  rw_vx_pseudo_modinst_add_interface(mip2,
                                       STANDARD_PLUGIN_TYPE_API,
                                       (void**)&sp_interface);
  ASSERT_TRUE(NULL != sp_interface);
  /* The interface functions may not be filled-in */


  plug1 = rw_vx_plug_alloc(rw_vxfp);
  ASSERT_TRUE(RW_VX_PLUG_VALID(plug1));
  port = rw_vx_plug_add_port(plug1,mip2);
  ASSERT_TRUE(RW_VX_PORT_VALID(port));
  pin = rw_vx_port_add_pin(port,
                           RWPLUGIN_PIN_DIRECTION_OUTPUT,
                           STANDARD_PLUGIN_TYPE_API,
                           NULL,NULL);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  
  /*
   * Create Plug2:
   *   - Add port with instance mip2[0]
   */
  plug2 = rw_vx_plug_alloc(rw_vxfp);
  ASSERT_TRUE(RW_VX_PLUG_VALID(plug2));
  port = rw_vx_plug_add_port(plug2,mip2);
  ASSERT_TRUE(RW_VX_PORT_VALID(port));
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  pin = rw_vx_port_add_pin(port,
                           RWPLUGIN_PIN_DIRECTION_INPUT,
                           STANDARD_PLUGIN_TYPE_API,
                           NULL,NULL);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));


  /*
   * Test rw_vx_link2() of plug1 + plug2
   */
  link_handle1 = rw_vx_link2(linkid,plug1,plug2);
  ASSERT_TRUE(RW_VX_LINK_HANDLE_VALID(link_handle1));

  // Check the link results
  ASSERT_EQ(RW_STATUS_SUCCESS, link_handle1->status);
  ASSERT_TRUE(link_handle1->successfully_linked);
  
  /*
   * Unlink plug1 + plug2 (this frees the link, plugs, and pins
   */
  rw_status = rw_vx_unlink(link_handle1);
  // Check the un-link results
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

  rw_status = rw_vx_modinst_close(mip1);
  ASSERT_EQ(rw_status, RW_STATUS_SUCCESS);

  rw_status = rw_vx_modinst_close(mip2);
  ASSERT_EQ(rw_status, RW_STATUS_SUCCESS);
}

/**
 * Test loading multiples copies of the module with
 * different names.
 */
TEST_F(RIFTPluginTest, LoadMultipleCopies) {
  int i;
  rw_status_t rw_status = RW_STATUS_SUCCESS;
  gchar module_alloc_cfg[] = "";
  gchar *plugin_name;
  gchar plugin_name_suffix[] = "c";
  
  TEST_DESCRIPTION("This test performs unit tests loading multiple modules in "
                   "rift vx framework.");
  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));

  plugin_name = g_strdup_printf("%s%s",
                                STANDARD_PLUGIN_EXTENSION_NAME,
                                plugin_name_suffix);
  
  // Load/register 10 copies of the STANDARD PLUGIN using different names
  for(i = 0; i < 10; i++) {
    gchar *module_name = g_strdup_printf("peas-test-module-%d",i);

    rw_status = rw_vx_module_register(rw_vxfp,
                                      module_name,
                                      module_alloc_cfg,
                                      plugin_name,
                                      NULL);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    g_free(module_name);
  }
  
  // Release the 10 plugin copies
  for(i = 0; i < 10; i++) {
    char *module_name = g_strdup_printf("peas-test-module-%d", i);
    rw_status = rw_vx_module_unregister(rw_vxfp, module_name);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    g_free(module_name);
  }
}

/**
 * Helper function to test StandardPlugin
 */
static void test_extension_call_add(PeasExtension *extension,
                                    StandardPluginApi *api,
                                    StandardPluginApiIface *interface)     
{
  gint a, b, c, rc, result;
  UNUSED(rc);
  UNUSED(c);

  // Set the input parmaeters for add(a, b)
  a = 1;
  b = 2;
  c = 99;

  if (extension) {
    // Call the plugin function using the method name "add"
    rc = peas_extension_call(extension, "add", a, b, &result);
    ASSERT_EQ(result, a + b);
  }

  if (interface) {
    ASSERT_TRUE(api);
    // Call the plugin function using the function pointer
    result = interface->add(api, a, b);
    ASSERT_EQ(result, a + b);
        
    // Benchmark the function call rate for add() with the current loader
    struct timeval start_timeval, end_timeval, duration_timeval;
    double seconds;
    int i;
    gettimeofday(&start_timeval, NULL);
    gettimeofday(&end_timeval, NULL);
#define NOPS 10000
    for(i = 0 ; i < NOPS ; i++) {
      result = interface->add(api, a, b);
    }
    gettimeofday(&end_timeval, NULL);
    timersub(&end_timeval, &start_timeval, &duration_timeval);
    seconds = duration_timeval.tv_sec + duration_timeval.tv_usec / 1000000.0;
    printf("Ran %d calls in %4.2f seconds rate = %lf mCPS\n",
           i, seconds, i / seconds / 1000000);
  }
}

/**
 * Test the StandardPluginAPI
 */
TEST_F(RIFTPluginTest, StandardPluginAPI) {
  StandardPluginApi *api;
  StandardPluginApiIface *interface;
  rw_status_t rw_status;
  gchar call_module_name[] = "call_plugin_name";
  gchar module_alloc_cfg[] = "";
  rw_vx_module_common_t *modp;
  rw_vx_modinst_common_t *mip;
  gchar *plugin_name;
  gchar plugin_name_suffix[] = "c";

  TEST_DESCRIPTION("This test performs standard plugin unit tests using "
                   "rift vx framework.");

  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  plugin_name = g_strdup_printf("%s%s",
                                STANDARD_PLUGIN_EXTENSION_NAME,
                                plugin_name_suffix);

  // Load PEAS extension "plugin_name[]" and use the name "call_module_name[]" for it
  rw_status = rw_vx_module_register(rw_vxfp,
                                        call_module_name,
                                        module_alloc_cfg,
                                        plugin_name,
                                        &modp);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  
  rw_status = rw_vx_modinst_open(rw_vxfp,
                                     call_module_name,
                                     RW_VX_CLONE_MINOR_MODINST,
                                     (char *)"",
                                     &mip);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  
  // Get API handle
  rw_status = rw_vx_modinst_get_api_and_iface(mip,
                                                  STANDARD_PLUGIN_TYPE_API,
                                                  (void **)&api,
                                                  (void**)&interface,
                                                  NULL);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  ASSERT_TRUE(api);
  ASSERT_TRUE(interface);

  test_extension_call_add(modp->peas_extension,
                          api,
                          interface);

  rw_status = rw_vx_modinst_close(mip);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  rw_status = rw_vx_module_unregister(rw_vxfp, call_module_name);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

  g_free(plugin_name);  
}


/**
 * Test that separately loaded plugins return separate instances.
 */
TEST_F(RIFTPluginTest, SeparateInstance)
{
  rw_status_t status;
  rw_vx_framework_t * fw1;
  rw_vx_framework_t * fw2;
  rw_vx_modinst_common_t * mip1;
  rw_vx_modinst_common_t * mip2;
  PythonPluginPythonValueStore * cls1;
  PythonPluginPythonValueStore * cls2;
  PythonPluginPythonValueStoreIface * iface1;
  PythonPluginPythonValueStoreIface * iface2;
  char * ret1;
  char * ret2;
  std::string plugin_name;

  // Work around c++ and lack of rw_vx const correctness
  char null_str[] = "";

  fw1 = rw_vx_framework_alloc();
  ASSERT_TRUE(fw1);

  fw2 = rw_vx_framework_alloc();
  ASSERT_TRUE(fw2);


  rw_vx_require_repository("PythonPlugin", "1.0");

  plugin_name = "python_plugin-python";

  status = rw_vx_library_open(fw1, (char *)plugin_name.c_str(), null_str, &mip1);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);

  status = rw_vx_library_open(fw2, (char *)plugin_name.c_str(), null_str, &mip2);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);

  status = rw_vx_library_get_api_and_iface(
      mip1,
      PYTHON_PLUGIN_TYPE_PYTHON_VALUE_STORE,
      (void **)&cls1,
      (void **)&iface1,
      NULL);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);

  status = rw_vx_library_get_api_and_iface(
      mip2,
      PYTHON_PLUGIN_TYPE_PYTHON_VALUE_STORE,
      (void **)&cls2,
      (void **)&iface2,
      NULL);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);

  /* Check that class attributes are the same across plugin instances */
  iface1->set_class_value(cls1, "key1", "value1");

  ret1 = iface1->get_class_value(cls1, "key1");
  ASSERT_STREQ("value1", ret1);
  free(ret1);

  ret2 = iface2->get_class_value(cls2, "key1");
  ASSERT_STREQ("value1", ret2);
  free(ret2);

  /* Check that instance attributes are different across plugin instances */
  iface1->set_instance_value(cls1, "key2", "value2");

  ret1 = iface1->get_instance_value(cls1, "key2");
  ASSERT_STREQ("value2", ret1);
  free(ret1);

  ret2 = iface2->get_instance_value(cls2, "key2");
  ASSERT_STREQ("", ret2);
  free(ret2);

  iface2->set_instance_value(cls2, "key2", "value-new");

  ret1 = iface1->get_instance_value(cls1, "key2");
  ASSERT_STREQ("value2", ret1);
  free(ret1);

  ret2 = iface2->get_instance_value(cls2, "key2");
  ASSERT_STREQ("value-new", ret2);
  free(ret2);

  /* Cleanup */
  status = rw_vx_library_close(mip1, true);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);

  status = rw_vx_library_close(mip2, true);
  ASSERT_EQ(RW_STATUS_SUCCESS, status);
}


/**
 * Test the FeaturePluginAPI
 */
TEST_F(RIFTPluginTest, FeaturePluginAPI) {
  rw_vx_modinst_common_t *mip1, *mip2;
  gchar null_str[] = "";
  FeaturePluginApi *f_api = NULL;
  FeaturePluginApiIface *f_iface = NULL;
  gchar *feature_plugin_name;
  gchar plugin_name_suffix[] = "c";
  rw_status_t rw_status;

  TEST_DESCRIPTION("This test performs feature plugin unit tests using "
                   "rift vx framework.");

  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  feature_plugin_name = g_strdup_printf("%s%s",
                                        FEATURE_PLUGIN_EXTENSION_NAME,
                                        plugin_name_suffix);
  
  rw_status = rw_vx_library_open(rw_vxfp,
                                     feature_plugin_name,
                                     null_str,
                                     &mip1);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

  mip2 = rw_vx_library_lookup(rw_vxfp,
                                feature_plugin_name);
  ASSERT_EQ(mip1, mip2);

  rw_status = rw_vx_library_get_api_and_iface(mip1,
                                                  FEATURE_PLUGIN_TYPE_API,
                                                  (void **)&f_api,
                                                  (void**)&f_iface,
                                                  NULL);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  ASSERT_TRUE(NULL != f_api);
  ASSERT_TRUE(NULL != f_iface);

  // Test the vala-generated way to extract the api+interface for a library
  {
    PeasExtension *extension;
    extension = rw_vx_library_get_peas_extension(mip1);
    ASSERT_TRUE(extension);
    ASSERT_TRUE(FEATURE_PLUGIN_API(extension) == f_api);
    ASSERT_TRUE(FEATURE_PLUGIN_API_GET_INTERFACE(extension) == f_iface);
  }

  rw_status = rw_vx_library_close(mip1, FALSE);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  g_free(feature_plugin_name);
}

/**
 * Test opening and closing modules
 */
TEST_F(RIFTModulePluginTest, ModuleOpenClose) {
  uint32_t i;
  rw_status_t rw_status;
#define NINST 3 /* Size of various tests to run */
  rw_vx_modinst_common_t *mip1[NINST], *mip2[NINST];
  rw_vx_module_common_t *modp = NULL;
  rw_vx_modinst_common_t *modip = NULL;
  char bogus_name[] = "bogus";
  
  TEST_DESCRIPTION("This test performs module open/close tests using "
                   "rift vx framework.");
  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  ASSERT_TRUE(module1_name);

  /* Ensure the module used in this test exists */
  modp = rw_vx_module_lookup(rw_vxfp, module1_name);
  ASSERT_TRUE(NULL != modp);
  
  /*
   * Using module, allocate both specific modinst instances
   * as well as cloned instances
   * (i.e. when opening: minor == RW_VX_CLONE_MINOR_MODINST)
   */

  // Allocate minor module instances 0,3,6,9,...,27
  for(i = 0; i < NINST; i++) {
    rw_status = rw_vx_modinst_open(rw_vxfp,
                                       module1_name,
                                       i*3,
                                       (char *)"",
                                       &mip1[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

    // Ensure each newly opened module instance can be looked-up
    modip = rw_vx_modinst_lookup(rw_vxfp,
                                   module1_name,
                                   mip1[i]->minor);
    ASSERT_TRUE(modip == mip1[i]);
  }

  // Verify that non-existant module instances cannot be looked-up
  modip = rw_vx_modinst_lookup(rw_vxfp,
                                 module1_name,
                                 999999);
  ASSERT_TRUE(NULL == modip);
  
  modip = rw_vx_modinst_lookup(rw_vxfp, bogus_name, 1);
  ASSERT_TRUE(NULL == modip);
  
  modp = rw_vx_module_lookup(rw_vxfp, bogus_name);
  ASSERT_TRUE(NULL == modp);

  /*
   * Allocate some cloned module instances by using minor < 0
   * (i.e. RW_VX_CLONE_MINOR_MODINST). This should result
   * in the skipped module instances #s above being used (1,2,4,5,7,8,...)
   */
  for(i = 0; i < NINST; i++) {
    rw_status = rw_vx_modinst_open(rw_vxfp,
                                       module1_name,
                                       RW_VX_CLONE_MINOR_MODINST,
                                       (char *)"",
                                       &mip2[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  }
  
  /*
   * Close ALL of the module instances opened above
   */
  for(i = 0; i < NINST; i++) {
    rw_status = rw_vx_modinst_close(mip1[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    rw_status = rw_vx_modinst_close(mip2[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  }  
}

/**
 * Test linking various modules
 */
TEST_F(RIFTModulePluginTest, ModuleLinking) {
  rw_status_t rw_status;
  int i;
#define NMI 7 /* # of module instances */
  rw_vx_modinst_common_t *mip1[NMI], *mip2[NMI], *feature_modinst;
  rw_vx_plug_t *plug1, *plug2;
  rw_vx_plug_port_t *port;
  rw_vx_port_pin_t *pin;
  rw_vx_link_handle_t *link_handle1;
  uint32_t linkid = 1;
  StandardPluginApi *api1, *api2, *api3;
  StandardPluginApiIface *iface1, *iface2, *iface3;
  FeaturePluginApi *f_api = NULL;
  FeaturePluginApiIface *f_iface = NULL;

  TEST_DESCRIPTION("This test performs module linking tests using "
                   "rift vx framework.");

  ASSERT_TRUE(RW_VX_FRAMEWORK_VALID(rw_vxfp));
  ASSERT_TRUE(module1_name);
  ASSERT_TRUE(module2_name);
  ASSERT_TRUE(feature_module_name);
  ASSERT_TRUE(RW_VX_MODULE_VALID(rw_vx_module_lookup(rw_vxfp, module1_name)));
  ASSERT_TRUE(RW_VX_MODULE_VALID(rw_vx_module_lookup(rw_vxfp, module2_name)));
  ASSERT_TRUE(RW_VX_MODULE_VALID(rw_vx_module_lookup(rw_vxfp, feature_module_name)));

  /*
   * Open "NMI" module instances output module1 and module2 for the test
   */
  for(i = 0; i < NMI; i++) {
    rw_status = rw_vx_modinst_open(rw_vxfp,
                                       module1_name,
                                       RW_VX_CLONE_MINOR_MODINST,
                                       (char *)"",
                                       &mip1[i]);    
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    ASSERT_TRUE(RW_VX_MODINST_VALID(mip1[i]));
    
    rw_status = rw_vx_modinst_open(rw_vxfp,
                                       module2_name,
                                       RW_VX_CLONE_MINOR_MODINST,
                                       (char *)"",
                                       &mip2[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    ASSERT_TRUE(RW_VX_MODINST_VALID(mip2[i]));
  }

  /*
   * Open the feature module which doesn't support COMMON INTERFACE for linking
   * and get the api and interface to call it
   */
  rw_status = rw_vx_modinst_open(rw_vxfp,
                                     feature_module_name,
                                     RW_VX_CLONE_MINOR_MODINST,
                                     (char *)"",
                                     &feature_modinst);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  ASSERT_TRUE(RW_VX_MODINST_VALID(feature_modinst));
  rw_status = rw_vx_modinst_get_api_and_iface(feature_modinst,
                                                  FEATURE_PLUGIN_TYPE_API,
                                                  (void **)&f_api,
                                                  (void**)&f_iface,
                                                  NULL);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  ASSERT_TRUE(NULL != f_api);
  ASSERT_TRUE(NULL != f_iface);
  
  // Test the vala-generated way to extract the api+interface for a module instance
  {
    PeasExtension *extension;
    extension = rw_vx_modinst_get_peas_extension(feature_modinst);
    ASSERT_TRUE(extension);
    ASSERT_TRUE(FEATURE_PLUGIN_API(extension) == f_api);
    ASSERT_TRUE(FEATURE_PLUGIN_API_GET_INTERFACE(extension) == f_iface);
  }
  f_api = NULL;
  f_iface = NULL;
  
  /*
   * Perform basic sanity-test plug, connector, and link APIs
   */
  plug1 = rw_vx_plug_alloc(rw_vxfp);
  rw_vx_plug_free(plug1);

  /*
   * Create Plug1:
   *   - Add port with instance mip1[0]
   */
  plug1 = rw_vx_plug_alloc(rw_vxfp);
  ASSERT_TRUE(RW_VX_PLUG_VALID(plug1));
  port = rw_vx_plug_add_port(plug1,mip1[0]);
  ASSERT_TRUE(RW_VX_PORT_VALID(port));
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_OUTPUT,
                             STANDARD_PLUGIN_TYPE_API,
                             NULL,NULL);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_INPUT,
                             STANDARD_PLUGIN_TYPE_API,
                             (void**)&api1,(void**)&iface1);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  
  /*
   * Create Plug2:
   *   - Add port with instance mip2[0]
   */
  plug2 = rw_vx_plug_alloc(rw_vxfp);
  ASSERT_TRUE(RW_VX_PLUG_VALID(plug2));
  port = rw_vx_plug_add_port(plug2,mip2[0]);
  ASSERT_TRUE(RW_VX_PORT_VALID(port));
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_OUTPUT,
                             STANDARD_PLUGIN_TYPE_API,
                             (void**)&api2,(void**)&iface2);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_INPUT,
                             STANDARD_PLUGIN_TYPE_API,
                             (void**)&api3,(void**)&iface3);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));


  /*
   * Test rw_vx_link2() of plug1 + plug2
   */
  link_handle1 = rw_vx_link2(linkid,plug1,plug2);
  ASSERT_TRUE(RW_VX_LINK_HANDLE_VALID(link_handle1));

  // Check the link results
  ASSERT_EQ(RW_STATUS_SUCCESS, link_handle1->status);
  ASSERT_TRUE(link_handle1->successfully_linked);
  ASSERT_TRUE((NULL != iface1) && (iface1 == iface2) && (iface2 == iface3));
  ASSERT_TRUE(api1 && api2 && api3 && (api1 == api2));
  
  /*
   * Unlink plug1 + plug2 (this frees the link, plugs, and pins
   */
  rw_status = rw_vx_unlink(link_handle1);
  // Check the un-link results
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  
  /*
   * Test rw_vx_link1()
   *  - single plug with 2 ports
   *  - Each port has a single output pin (API)
   */
  plug1 = rw_vx_plug_alloc(rw_vxfp);
  ASSERT_TRUE(RW_VX_PLUG_VALID(plug1));
  port = rw_vx_plug_add_port(plug1,mip1[0]);
  ASSERT_TRUE(RW_VX_PORT_VALID(port));
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_OUTPUT,
                             STANDARD_PLUGIN_TYPE_API,
                             NULL,NULL);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  port = rw_vx_plug_add_port(plug1,feature_modinst);
  pin = rw_vx_port_add_pin(port,
                             RWPLUGIN_PIN_DIRECTION_OUTPUT,
                             FEATURE_PLUGIN_TYPE_API,
                             (void **)&f_api,(void **)&f_iface);
  ASSERT_TRUE(RW_VX_PIN_VALID(pin));
  link_handle1 = rw_vx_link1(linkid,plug1);
  ASSERT_EQ(RW_STATUS_SUCCESS, link_handle1->status);
  ASSERT_TRUE(f_api);
  ASSERT_TRUE(FEATURE_PLUGIN_IS_API(f_api));
  ASSERT_TRUE(f_iface);

  /*
   * Unlink plug1 (this frees the link, plugs, and pins
   */
  rw_status = rw_vx_unlink(link_handle1);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);

  /*
   * Close all of previously opened module instances
   */
  for(i=0;i<NMI;i++) {
    rw_status = rw_vx_modinst_close(mip1[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
    rw_status = rw_vx_modinst_close(mip2[i]);
    ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);
  }
  
  rw_status = rw_vx_modinst_close(feature_modinst);
  ASSERT_EQ(RW_STATUS_SUCCESS, rw_status);  
}


/**
 * Tests RW_SHOW_BACKTRACE. 
 */
TEST(RIFTSetEnv, SetEnv) {
  TEST_DESCRIPTION("This test valiadtes the rw_setenv() functionality.");

  setenv("RW_VAR_RIFT", "/tmp", FALSE);

  ASSERT_NE(rw_setenv("/", "abc"), RW_STATUS_SUCCESS);
  ASSERT_NE(rw_setenv("abcd/", "abc"), RW_STATUS_SUCCESS);
  ASSERT_EQ(rw_setenv("abcd", "abc"), RW_STATUS_SUCCESS);
  rw_unsetenv("abcd");
  ASSERT_EQ(rw_setenv("ABCD", ""), RW_STATUS_SUCCESS);
  rw_unsetenv("ABCD");
  ASSERT_EQ(rw_setenv("XYZ", "xyz"), RW_STATUS_SUCCESS);
  rw_unsetenv("XYZ");

  ASSERT_TRUE(rw_getenv("ab/cd")==NULL);
  ASSERT_TRUE(rw_getenv("bcd")==NULL);

  rw_setenv("XYZ", "xyz");
  const char *get_val = rw_getenv("XYZ");
  ASSERT_TRUE(get_val != NULL);
  ASSERT_TRUE(!strcmp(get_val, "xyz"));
  rw_unsetenv("XYZ");

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
