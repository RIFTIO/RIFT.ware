
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwtrace_gtest.cc
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/18/2013
 * @brief Google test cases for testing RIFT tracing
 * 
 * @details Google test cases for testing RIFT tracing
 */

/* 
 * Step 1: Include the necessary header files 
 */
#include <limits.h>
#include <cstdlib>
#include "rwut.h"
#include "rwlib.h"
#include "rwtrace.h"

using ::testing::HasSubstr;

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

class RWTrace : public ::testing::Test {
 public:
  rwtrace_ctx_t *ctx;
  virtual void SetUp() {
    ctx = rwtrace_init();
    ASSERT_TRUE(ctx != NULL);
  }

  virtual void TearDown() {
    rwtrace_ctx_close(ctx);
  }
};

TEST_F(RWTrace, EnableAllTraces) {
  TEST_DESCRIPTION("This test sets the trace severity to debug and make sure "
                   "that traces at all other severity are logged");
  rw_status_t status;

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_DEBUG);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_RWSCHED,
                                            RWTRACE_DESTINATION_CONSOLE);
  
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_DEBUG");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_DEBUG"));


  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_INFO");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_INFO"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_CRITINFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRITINFO");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_CRITINFO"));


  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_NOTICE(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_NOTICE");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_NOTICE"));


  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_WARN");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_WARN"));


  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ERROR");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_ERROR"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_CRIT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRIT");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_CRIT"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_ALERT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ALERT");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_ALERT"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_EMERG");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_EMERG"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, 
                "Testing RWTRACE_EMERG with one int param [%d]", 
                10);
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_EMERG with one int param [10]"));

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, 
                "Testing RWTRACE_EMERG with two int param [%d, %d]", 
                10, 10);
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_EMERG with two int param [10, 10]"));
}

// Set the severity of a category to RWTRACE_SEVERITY_EMERG
// and make sure that only EMERG traces are produced.
TEST_F(RWTrace, EnableOnlyEmerg) {
  rw_status_t status;

  TEST_DESCRIPTION("This test sets the trace severity to emerg and makes sure "
                   "that only emerg traces are logged");

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_EMERG);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_RWSCHED,
                                            RWTRACE_DESTINATION_CONSOLE);
  
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_DEBUG");
  RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_INFO");
  RWTRACE_NOTICE(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_NOTICE");
  RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_WARN");
  RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ERROR");
  RWTRACE_CRIT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRIT");
  RWTRACE_ALERT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ALERT");
  /* nothing should be captured */
  ASSERT_EQ(ConsoleCapture::get_capture_string().size(), (unsigned int)0); 

  /* Only emergency traces show up on console */
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_EMERG");
  ASSERT_THAT(ConsoleCapture::get_capture_string(), 
              HasSubstr("Testing RWTRACE_EMERG"));
}

// Set the severity of a category to RWTRACE_SEVERITY_DISABLE
// and make sure that no traces are produced.
TEST_F(RWTrace, DisableAll) {
  rw_status_t status;

  TEST_DESCRIPTION("This test sets the trace severity to disable and makes sure "
                   "that NO traces are logged");

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_DISABLE);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_RWSCHED,
                                            RWTRACE_DESTINATION_CONSOLE);
  
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  /* category is disabled nothing shows up on console */
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_DEBUG");
  RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_INFO");
  RWTRACE_CRITINFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRITINFO");
  RWTRACE_NOTICE(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_NOTICE");
  RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_WARN");
  RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ERROR");
  RWTRACE_CRIT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRIT");
  RWTRACE_ALERT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ALERT");
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_EMERG");
  /* nothing should be captured */
  ASSERT_EQ(ConsoleCapture::get_capture_string().size(), (unsigned int)0); 
}

/* Wrap the trace in the macro to get the precise lineno */
#define RWTRACE_LOCATION_TEST(_category, _fmt, ...)                         \
{                                                                           \
  rw_code_location_t _l;                                                    \
  rw_status_t _s;                                                           \
  strncpy(_l.filename, "rwtrace_gtest.cc", RW_MAX_LOCATION_FILENAME_SIZE);     \
  _l.lineno = __LINE__;                                                     \
  _s = rwtrace_set_trace_location(ctx, RWTRACE_CATEGORY_RWSCHED, &_l);        \
  ASSERT_EQ(_s, RW_STATUS_SUCCESS);                                         \
  RWTRACE_EMERG(ctx, _category, _fmt, ##__VA_ARGS__);                       \
}

// Set the severity of a category to RWTRACE_SEVERITY_DISABLE
// and make sure that no traces are produced.
TEST_F(RWTrace, EnableTraceLoc) {
  rw_status_t status;

  TEST_DESCRIPTION("This test sets the trace severity to disable and uses "
                   "rwtrace_set_trace_location to enable a specific trace");
  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_DISABLE);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_RWSCHED,
                                            RWTRACE_DESTINATION_CONSOLE);
  
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  /* category is disabled nothing shows up on console */
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_DEBUG");
  RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_INFO");
  RWTRACE_CRITINFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRITINFO");
  RWTRACE_NOTICE(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_NOTICE");
  RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_WARN");
  RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ERROR");
  RWTRACE_CRIT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRIT");
  RWTRACE_ALERT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ALERT");
  RWTRACE_EMERG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_EMERG");
  /* nothing should be captured */
  ASSERT_EQ(ConsoleCapture::get_capture_string().size(), (unsigned int)0); 

  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_LOCATION_TEST(RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_EMERG");
  ASSERT_THAT(ConsoleCapture::get_capture_string(),
              HasSubstr("Testing RWTRACE_EMERG"));

}

// Unit test the set/unset trace method
TEST_F(RWTrace, SetUnsetTraceLocation) {
  rw_status_t status;  
  rw_code_location_t location;
  int i;
  int cnt;

  TEST_DESCRIPTION("This tests the setting and unsetting of the trace locations.");
  
  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_DISABLE);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  strncpy(location.filename, "rwtrace_gtest.cc", RW_MAX_LOCATION_FILENAME_SIZE);
  for (i = 0; i < RW_MAX_CODE_LOCATIONS; ++i) {
    location.lineno = i+1;
    status = rwtrace_set_trace_location(ctx, RWTRACE_CATEGORY_RWSCHED, &location);
    ASSERT_EQ(status, RW_STATUS_SUCCESS);
    cnt = rwtrace_get_trace_location_count(ctx, RWTRACE_CATEGORY_RWSCHED);   
    ASSERT_EQ(cnt, i+1);

    /* check for duplicate operations */
    status = rwtrace_set_trace_location(ctx, RWTRACE_CATEGORY_RWSCHED, &location);
    if (i == (RW_MAX_CODE_LOCATIONS-1)) {
      ASSERT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);
    } else {
      ASSERT_EQ(status, RW_STATUS_DUPLICATE);
    }
    cnt = rwtrace_get_trace_location_count(ctx, RWTRACE_CATEGORY_RWSCHED);   
    ASSERT_EQ(cnt, i+1);
  }

  location.lineno = RW_MAX_CODE_LOCATIONS+1000;
  status = rwtrace_unset_trace_location(ctx, RWTRACE_CATEGORY_RWSCHED, &location);
  ASSERT_EQ(status, RW_STATUS_NOTFOUND);

  location.lineno = 1;
  status = rwtrace_unset_trace_location(ctx, RWTRACE_CATEGORY_RWSCHED, &location);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  cnt = rwtrace_get_trace_location_count(ctx, RWTRACE_CATEGORY_RWSCHED);
  ASSERT_EQ(cnt, RW_MAX_CODE_LOCATIONS-1);
}

// Unset the category destination and make sure that no traces
// are generated to console.
TEST_F(RWTrace, DisableConsole) {
  rw_status_t status;

  TEST_DESCRIPTION("This test disables the output to console and make sure that "
                   "no traces are seen on the console.");
  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED,
                                         RWTRACE_SEVERITY_DISABLE);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  /* No destination is set, so nothing shows up on console */
  ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
  RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_DEBUG");
  RWTRACE_INFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_INFO");
  RWTRACE_CRITINFO(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRITINFO");
  RWTRACE_NOTICE(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_NOTICE");
  RWTRACE_WARN(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_WARN");
  RWTRACE_ERROR(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_ERROR");
  RWTRACE_CRIT(ctx, RWTRACE_CATEGORY_RWSCHED, "Testing RWTRACE_CRIT");
  /* nothing should be captured */
  ASSERT_EQ(ConsoleCapture::get_capture_string().size(), (unsigned int)0); 
}

// Try to set the severity of invalid category
TEST_F(RWTrace, SetSeverityOfInvalidCategory) {
  rw_status_t status;

  TEST_DESCRIPTION("This test tries to set the severity of invalid category "
                   "and makes sure that operation fails in a non-fatal fashion.");

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         (rwtrace_category_t)-1, 
                                         RWTRACE_SEVERITY_DEBUG);
  ASSERT_EQ(status, RW_STATUS_FAILURE);

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_LAST, 
                                         RWTRACE_SEVERITY_DEBUG);
  ASSERT_EQ(status, RW_STATUS_FAILURE);
}

// Try to set invalid severity
TEST_F(RWTrace, SetInvalidSeverity) {
  rw_status_t status;

  TEST_DESCRIPTION("This test tries to set invalid severity "
                   "and makes sure that operation fails in a non-fatal fashion.");

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED, 
                                         (rwtrace_severity_t)(RWTRACE_SEVERITY_DEBUG+1));
  ASSERT_EQ(status, RW_STATUS_FAILURE);

  status = rwtrace_ctx_category_severity_set(ctx, 
                                         RWTRACE_CATEGORY_RWSCHED, 
                                         (rwtrace_severity_t)-1);
  ASSERT_EQ(status, RW_STATUS_FAILURE);
}

// Try to set the destination of invalid category
TEST_F(RWTrace, SetDestnationofInvalidCategory) {
  rw_status_t status;

  TEST_DESCRIPTION("This test tries to set the destination of invalid category "
                   "and makes sure that operation fails in a non-fatal fashion.");

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            (rwtrace_category_t)-1, 
                                            RWTRACE_DESTINATION_CONSOLE);
  ASSERT_EQ(status, RW_STATUS_FAILURE);

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_LAST, 
                                            RWTRACE_DESTINATION_CONSOLE);
  ASSERT_EQ(status, RW_STATUS_FAILURE);
} 

// Try to set invalid trace destination
TEST_F(RWTrace, SetInvalidDestnation) {
  rw_status_t status;

  TEST_DESCRIPTION("This test tries to set invalid destination "
                   "and makes sure that operation fails in a non-fatal fashion.");

  status = rwtrace_ctx_category_destination_set(ctx, 
                                            RWTRACE_CATEGORY_RWSCHED, 
                                            (rwtrace_destination_t)(RWTRACE_DESTINATION_CONSOLE+1));
  ASSERT_EQ(status, RW_STATUS_FAILURE);
} 
