
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

/** 
 * Step 1: Include the necessary header files 
 */
#include <limits.h>
#include <cstdlib>
#include <iostream> 
#include "rwut.h"
#include "rwlib.h"
#include "rwlog.h"
#include "rwlog_filters.h"
#include "rw-logtest.pb-c.h"
#include "rwtypes.h"
#include <sys/mman.h> 
#include <sys/time.h>

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

#define RW_LOG_GTEST_CATEGORY  1

static void rwlog_gtest_reset_bootstrap_filters(char *shm_file_name)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm = NULL;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl;

  if(!shm_file_name) {
    int r = asprintf (&rwlog_shm,
                      "%s-%d",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId());
    RW_ASSERT(r);
  }
  else {
    rwlog_shm = RW_STRDUP(shm_file_name);
  }

  RWLOG_DEBUG_PRINT("INITED bootstrap log filters\n");

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    RWLOG_ASSERT(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  rwlogd_shm_ctrl =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == rwlogd_shm_ctrl)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm);
    RWLOG_ASSERT(0);
    return;
  }

  filter_memory_header *rwlogd_filter_memory = NULL;
  rwlogd_filter_memory = &rwlogd_shm_ctrl->hdr;

  rwlogd_filter_memory->skip_L1 = FALSE;
  rwlogd_filter_memory->skip_L2 = FALSE;
  rwlogd_filter_memory->allow_dup = TRUE;
  rwlogd_filter_memory->rwlogd_flow = FALSE;
  rwlogd_filter_memory->log_serial_no = 1;

  rwlogd_filter_memory->num_categories = 2;
  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t));
  strcpy(category_filter->name,"all");
  category_filter->severity = RW_LOG_LOG_SEVERITY_ERROR;
  category_filter->bitmap = 0;

  category_filter++;
  strcpy(category_filter->name,"rw-logtest");
  category_filter->severity = RW_LOG_LOG_SEVERITY_ERROR;
  category_filter->bitmap = 0;

  rwlogd_shm_ctrl->last_location = sizeof(rwlogd_shm_ctrl_t)+ sizeof(rwlog_category_filter_t)*2;


  munmap(rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  free(rwlog_shm);
  close(filter_shm_fd);
}

static rwlog_ctx_t *rwlog_gtest_source_init(const char *taskname)
{
  char *rwlog_filename;
  char *filter_shm_name;
  rwlog_ctx_t *ctx;
  //int i = 0;
  
  // clean slate
  int r = asprintf (&rwlog_filename,
                    "%s-%d-%d-%lu",
                    RWLOG_FILE,
                    rwlog_get_systemId(),
                    getpid(),syscall(SYS_gettid));

  RW_ASSERT(r);

  r = asprintf (&filter_shm_name,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  RW_ASSERT(r);

  rwlog_init_bootstrap_filters(filter_shm_name);
  rwlog_gtest_reset_bootstrap_filters(filter_shm_name);

  ctx = rwlog_init_internal(taskname,rwlog_filename,filter_shm_name,NULL);
  RW_ASSERT(ctx);

  free(rwlog_filename);
  free(filter_shm_name);

  return ctx;
}

static rw_status_t rwlog_gtest_source_close(rwlog_ctx_t *ctx)
{
  char *rwlog_shm = strdup(ctx->rwlog_shm);
  rw_status_t status = rwlog_close(ctx,TRUE);
  shm_unlink(rwlog_shm);
  free(rwlog_shm);
  return status;
}


/***************************************************************
 * Start Test for source side code 
 **************************************************************/
TEST(RWLogSource, LogGtestInitSimple) 
{
  rw_status_t status;
  TEST_DESCRIPTION("1.1: This test verifies log-ctxt initialization");

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  EXPECT_TRUE(ctxt->error_code == 0);
  rwlog_update_appname(ctxt, "riftio-logtest");
  rwlog_ctxt_dump(ctxt);
  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}

TEST(RWLogSource, LogInitSimple) 
{
  rw_status_t status;
  TEST_DESCRIPTION("1.1: This test verifies log-ctxt initialization");

  rwlog_ctx_t *ctxt = rwlog_init("google-test1");
  EXPECT_TRUE(ctxt->error_code == 0);

  status = rwlog_close(ctxt,TRUE);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  char *rwlog_shm_name = NULL;
  asprintf (&rwlog_shm_name,
                    "%s-%d",
                    RWLOG_FILTER_SHM_PATH,
                    rwlog_get_systemId());
  shm_unlink(rwlog_shm_name);
  free(rwlog_shm_name);
}

TEST(RWLogSource, LogInitFileExists) 
{
  rw_status_t status;
  TEST_DESCRIPTION("1.2: This test verifies log-ctxt initialization when SHM and file exist");
  rwlog_ctx_t *ctxt1 = rwlog_gtest_source_init("google-test_init_");

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  EXPECT_TRUE(ctxt->error_code == 0);
  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_gtest_source_close(ctxt1);
  //Limitation. 
  //EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}


TEST(RWLogSource, LogInitInvalidFilePermission) 
{
  rw_status_t status;
  char file[256];
  TEST_DESCRIPTION("1.3: This test verifies log-ctxt file permissions are invalid");
  rwlog_ctx_t *ctxt1 = rwlog_gtest_source_init("google-test_init_");

  // Change File Permission
  strcpy(file, ctxt1->rwlog_filename);
  chmod(file,S_IRUSR|S_IRGRP|S_IROTH);
  status = rwlog_gtest_source_close(ctxt1);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  status = rwlog_gtest_source_close(ctxt);


  // Change directory permission
  // TBD ...
  ctxt = rwlog_gtest_source_init("google-test1");
  status = rwlog_gtest_source_close(ctxt);

  chmod(file,S_IRWXU);
  ctxt = rwlog_gtest_source_init("google-test1");
  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}

TEST(RWLogSource, LogBasicLogs)
{
  rw_status_t status;
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  TEST_DESCRIPTION("1.4: This test verifies various calls to rwlog_event");
  EXPECT_TRUE(ctxt->error_code == 0);
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;

  memset(&callidvalue, 0, sizeof(callidvalue));
  callidvalue.callid=1;
  callidvalue.groupid=2;
  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abc", "XYZ", "33331");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abcef", "XYZ", "33331");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "abcef");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "filter");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "forward");
  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}
// EVENT logs .
// Refer Yang file

TEST(RWLogSource, LogEventCallTestFileWrite)
{
  TEST_DESCRIPTION("1.5: This test verifies File write with calls to rwlog_event");
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;


  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  EXPECT_TRUE(ctxt->error_code == 0);

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  ASSERT_TRUE(fd>0);

  fstat(fd, &file_stats);
  current_size = file_stats.st_size; 

  //Event 1:
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abc", "XYZ", "33331");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size);
  current_size = file_stats.st_size; 

  //Repeat
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abc", "XYZ", "33331");
  fstat(fd, &file_stats);
  EXPECT_EQ((file_stats.st_size-current_size), delta_size);
  current_size = file_stats.st_size; 

  //Event 2:
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abcef", "XYZ", "33331");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size);
  current_size = file_stats.st_size; 

  //Repeat
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEvPortStateChange, "abcef", "XYZ", "33331");
  fstat(fd, &file_stats);
  EXPECT_EQ((file_stats.st_size-current_size), delta_size);
  current_size = file_stats.st_size; 

  //Event 3:
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "abcef");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "filter");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "forward");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size);
  current_size = file_stats.st_size; 

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "abcef");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "filter");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "forward");
  fstat(fd, &file_stats);
  EXPECT_EQ((file_stats.st_size-current_size), delta_size);

  close(fd);
  rwlog_gtest_source_close(ctxt);
}

static bool rwlogtest_event(int size )
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  int i;
  struct stat file_stats;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  char user_string[size];
  memset(user_string, 0, sizeof(user_string));
  for (i=0; i< (int)sizeof(user_string)-1; i++)
  {
     user_string[i]='a';
  }
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  if (delta_size<(int)sizeof(user_string))
  {
    return false;
  }

  status = rwlog_gtest_source_close(ctxt);
  close(fd);
  return (status == RW_STATUS_SUCCESS);
}
TEST(RWLogSource, LogBigEvents)
{
  TEST_DESCRIPTION("1.5: This test verifies event of various sizes");
  EXPECT_TRUE(rwlogtest_event(1));
  EXPECT_TRUE(rwlogtest_event(10));
  EXPECT_TRUE(rwlogtest_event(100));
  EXPECT_TRUE(rwlogtest_event(1000));
  EXPECT_TRUE(rwlogtest_event(10000));
}

/***************************************************************
 * FILTERS AT SOURCE 
 **************************************************************/
//Copied from the file rwlogd_sink_common.cpp
//
static void rwlog_test_filter_set(uint32_t cat,
                                  rwlog_severity_t sev, 
                                  int level)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm;

  int r = asprintf (&rwlog_shm,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  ASSERT_TRUE(r);

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  filter_memory_header *_rwlogd_filter_memory =
      (filter_memory_header *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == _rwlogd_filter_memory)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }

  //Header
  _rwlogd_filter_memory->magic = RWLOG_FILTER_MAGIC;
  _rwlogd_filter_memory->rwlogd_pid = getpid();
  _rwlogd_filter_memory->rotation_serial = 0;
  _rwlogd_filter_memory->rwlogd_ticks = 1; 
  _rwlogd_filter_memory->rwlogd_flow = FALSE;
  _rwlogd_filter_memory->allow_dup = TRUE;

  _rwlogd_filter_memory->skip_L1 = FALSE;
  _rwlogd_filter_memory->skip_L2 = TRUE;

  for(uint32_t i =0 ; i<_rwlogd_filter_memory->num_categories; i++)
  {
    rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)_rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t) + i * sizeof(rwlog_category_filter_t));
    category_filter->severity = RW_LOG_LOG_SEVERITY_ERROR;
  }
  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)_rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t) + cat * sizeof(rwlog_category_filter_t));
  category_filter->severity = sev;

  /* Configure to pass all L1 and L2 filters for the moment.  A very
     temporary situation.  We actually want a "hidden" command of
     sorts to enable either or both of these at whim for testing etc.
     Disabling L1 in particular tests marshalling of all events. */
  munmap(_rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  close(filter_shm_fd);
  free(rwlog_shm);
}

static void rwlog_gtest_serialno_set(rw_call_id_t *callid)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm;

  int r = asprintf (&rwlog_shm,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  ASSERT_TRUE(r);

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  filter_memory_header *_rwlogd_filter_memory =
      (filter_memory_header *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == _rwlogd_filter_memory)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }

  callid->serial_no = _rwlogd_filter_memory->log_serial_no;
  munmap(_rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  free(rwlog_shm); 
  close(filter_shm_fd);
}


static void rwlog_test_callid_filter(uint64_t value )
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm;

  int r = asprintf (&rwlog_shm,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  ASSERT_TRUE(r);

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  filter_memory_header *_rwlogd_filter_memory =
      (filter_memory_header *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == _rwlogd_filter_memory)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }

  //Header
  _rwlogd_filter_memory->magic = RWLOG_FILTER_MAGIC;
  _rwlogd_filter_memory->rwlogd_pid = getpid();
  _rwlogd_filter_memory->rotation_serial = 0;

  for(uint32_t i =0 ; i<_rwlogd_filter_memory->num_categories; i++)
  {
    rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)_rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t) + i * sizeof(rwlog_category_filter_t));
    category_filter->severity = RW_LOG_LOG_SEVERITY_ERROR;
    category_filter->bitmap = 0;
    BLOOM_SET_UINT64(category_filter->bitmap,&value);
  }

  _rwlogd_filter_memory->skip_L1 = FALSE;
  _rwlogd_filter_memory->skip_L2 = TRUE;
  _rwlogd_filter_memory->rwlogd_flow = FALSE;

  munmap(_rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  close(filter_shm_fd);
  free(rwlog_shm);
}
static void rwlog_test_callid_filter_reset(uint64_t value )
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm;

  int r = asprintf (&rwlog_shm,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  ASSERT_TRUE(r);

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  filter_memory_header *_rwlogd_filter_memory =
      (filter_memory_header *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == _rwlogd_filter_memory)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }

  for(uint32_t i =0 ; i<_rwlogd_filter_memory->num_categories; i++)
  {
    rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)_rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t) + i * sizeof(rwlog_category_filter_t));
    BLOOM_RESET_UINT64(category_filter->bitmap,&value);
  }


  munmap(_rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  close(filter_shm_fd);
  free(rwlog_shm);
}

TEST(RWLogSrcFilter, FilterSeverity)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  for (int type = 0; type <5;type++)
  {
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size>0); //Default error pass
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_EMERGENCY, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size==0); // Drop event
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_DEBUG, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // Pass event
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // Pass event . Sev matched
  current_size = file_stats.st_size;
  
  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  }
}
TEST(RWLogSrcFilter, FilterSeverityEmergency)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  for (int type = 0; type <5;type++)
  { 
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size>0); //Default error send
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_EMERGENCY, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // pass
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_DEBUG, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // Pass event
  current_size = file_stats.st_size;

  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  }
}
TEST(RWLogSrcFilter, FilterSeverityDebug)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  for (int type = 0; type <1;type++)
  {

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size==0); //Default error drop
  current_size = file_stats.st_size;


  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_EMERGENCY, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size==0); // Emergency . Drop
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_DEBUG, type);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size); // Pass event
  current_size = file_stats.st_size;

  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  }
}
TEST(RWLogSrcFilter, FilterString)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size==0); //Default error drop

  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}
TEST(RWLogSrcFilter, FilterMemoryCorruption)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  //rwlog_test_filter_set(RW_LOG, RW_LOG_LOG_SEVERITY_ERROR);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size==0); //Default error drop

  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}

TEST(RWLogSrcFilter, FilterBootStrapLog)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");

  int filter_shm_fd =  shm_open(ctxt->rwlog_shm,oflags,perms);
  EXPECT_FALSE(filter_shm_fd < 0);
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  filter_memory_header *_rwlogd_filter_memory =
      (filter_memory_header *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);

  EXPECT_FALSE(MAP_FAILED == _rwlogd_filter_memory);

  //Header
  _rwlogd_filter_memory->magic = RWLOG_FILTER_MAGIC;
  _rwlogd_filter_memory->rwlogd_pid = getpid();
  _rwlogd_filter_memory->rotation_serial = 0;

  for(uint32_t i=0;i< _rwlogd_filter_memory->num_categories;i++) {
    rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)_rwlogd_filter_memory + sizeof(rwlogd_shm_ctrl_t) + i * sizeof(rwlog_category_filter_t));
    category_filter->severity = RW_LOG_LOG_SEVERITY_DEBUG;
  }

  _rwlogd_filter_memory->skip_L1 = FALSE;
  _rwlogd_filter_memory->skip_L2 = FALSE;
  _rwlogd_filter_memory->rwlogd_flow = FALSE;
  _rwlogd_filter_memory->allow_dup = TRUE;

  munmap(_rwlogd_filter_memory,RWLOG_FILTER_SHM_SIZE);
  close(filter_shm_fd);

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size; 
  EXPECT_TRUE(delta_size>0); //pass
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // pass
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "FILTER-THIS");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size>0); // Pass event
  current_size = file_stats.st_size;

  status = rwlog_gtest_source_close(ctxt);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
}
/*******************************************************************
 * Older tests
 ******************************************************************/
TEST(RWLog, LogRawCApi) 
{

  TEST_DESCRIPTION("This test verifies logging using plain C helper APIs");
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  ASSERT_TRUE(NULL != ctxt);
  int file_name_start = strlen(__FILE__);

  // init and fill in general event data
#if 1
  RwLog__YangData__RwLog__EvTemplate ev;
  rw_log__yang_data__rw_log__ev_template__init(&ev);
#else
  rw_fpath__rw_global_schema__rw_fpath__notification__fastpath_ev_port_state_change ev;
  rw_fpath__rw_global_schema__rw_fpath__notification__fastpath_ev_port_state_change__init(&ev);
#endif
  ev.template_params.has_linenumber = TRUE;
  ev.template_params.linenumber = __LINE__;
  if(file_name_start < 0) {
    file_name_start = 0;
  } 
  strncpy((ev).template_params.filename, 
          (char *)(__FILE__+ file_name_start),
          sizeof((ev).template_params.filename)); 
  (ev).template_params.filename[sizeof((ev).template_params.filename)-1] = '\0'; 

  // BUG category is not in the event, how do?

#if 0
  // event-specific data: for port state event
  strcat(ev.portnumber, "1");
  ev.operstate = "operstate!";
  ev.serialnumber = "serialno!";
#endif

  // send the thing, if it passes filters
  rwlog_proto_send(ctxt, "rw-logtest", &ev.base, 1, NULL,NULL,FALSE, FALSE,NULL);

  /* And how now do we verify that it went somewhere?  Perhaps: */
  //?? assert(ctx->filedescriptor_bytes > what_it_was_before)
  //free(ev.template_params.appname);
  //free(ev.template_params.hostname);

  rwlog_gtest_source_close(ctxt);
}

static bool rwlogtest_callevent()
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;

  memset(&callidvalue, 0, sizeof(callidvalue));
  callidvalue.callid=1;
  callidvalue.groupid=2;
  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;


  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");


  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_DEBUG, 1); 
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size);

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1); 
  current_size = file_stats.st_size;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_EQ(delta_size,0);

  rwlog_test_callid_filter((uint64_t)1);
  current_size = file_stats.st_size;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  EXPECT_TRUE(delta_size);

  rwlog_test_callid_filter_reset((uint64_t)1);
  rwlog_gtest_source_close(ctxt);
  close(fd);
  return (true);
}

static bool rwlogtest_calleventbuffering()
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  rw_call_id_t *callid, callidvalue;

  memset(&callidvalue, 0, sizeof(callidvalue));
  callidvalue.callid=1;
  callidvalue.groupid=2;
  callidvalue.call_arrived_time=0;
  callid = &callidvalue;
  struct timeval tv;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");


  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1); 
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  EXPECT_FALSE(delta_size);
  EXPECT_EQ (rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid),1);
  
  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;
  current_size = file_stats.st_size;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest ,callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  EXPECT_FALSE(delta_size);
  EXPECT_EQ(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid),2);

  current_size = file_stats.st_size;
  rwlog_callid_events_from_buf(ctxt,callid,FALSE);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size >=4);
  EXPECT_EQ(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid),0);

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1); 
  current_size = file_stats.st_size;

  //callid->suppress_log = TRUE;
  rwlog_gtest_serialno_set(callid);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "abcef");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);

  rwlog_gtest_source_close(ctxt);
  close(fd);
  return(true);
}

TEST(RWLogSource, LogCallEvent)
{
  TEST_DESCRIPTION(" This test verifies callid event test");
  EXPECT_TRUE(rwlogtest_callevent());
}

TEST(RWLogSource, LogCallEventBuffering)
{
  TEST_DESCRIPTION(" This test verifies callid event test");
  EXPECT_TRUE(rwlogtest_calleventbuffering());
}

TEST(RWLogSource, LogSpeculativeTimeExpiry)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;

  memset(&callidvalue, 0, sizeof(callidvalue));
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  ctxt->speculative_buffer_window = 10;

  callidvalue.callid=100;
  callidvalue.groupid=200;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec-7;  /* Fake call arrived time to reduce gtest execution time */
  callid = &callidvalue;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1); 

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-wait-time-expiry-1");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size == 0);

  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-wait-time-expiry-2");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);

  sleep(5);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "after-wait-time-expiry");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "after-wait-time-expiry-2");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  
  current_size = file_stats.st_size;
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size == 0);

  rwlog_gtest_source_close(ctxt);
  close(fd);
}

static void rwlog_test_get_shm_filter(filter_memory_header **filter_memory)
{
  int perms = 0600;           /* permissions */
  int  oflags  = O_CREAT | O_RDWR; // Write for the apps.
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl;

  int r = asprintf (&rwlog_shm,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  ASSERT_TRUE(r);

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }
  ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  rwlogd_shm_ctrl =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == rwlogd_shm_ctrl)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM%s\n", strerror(errno), rwlog_shm);
    ASSERT_TRUE(0);
    return;
  }

  *filter_memory = &rwlogd_shm_ctrl->hdr;

  close(filter_shm_fd);
  free(rwlog_shm);
}


TEST(RWLogSource, LogSpeculativeSuccessCase)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  filter_memory_header  *filter_memory = NULL;


  memset(&callidvalue, 0, sizeof(callidvalue));
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");

  rwlog_test_get_shm_filter(&filter_memory);
  filter_memory->skip_L2 = TRUE;

  const char *imsi = "123456789012346";
  const char *imsi_field = "123456789012346";
  /* Set bloom filter for imsi; sess-param filter are set for category ALL */

  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)filter_memory + sizeof(rwlogd_shm_ctrl_t));
  BLOOM_SET(category_filter[0].bitmap,imsi_field);
  EXPECT_TRUE(category_filter[0].bitmap);

  memset(&callidvalue,0,sizeof(callidvalue));
  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

 /* Case where initial logs generated dont have configured key and key is 
   * available after few logs */
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-imsi-available");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size == 0);


  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-imsi-available-2");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size == 0);


  RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams_SessionParams) session_params;
  RWPB_F_MSG_INIT(RwLog_data_EvTemplate_TemplateParams_SessionParams, &session_params);
  session_params.has_imsi = 1;
  strncpy(session_params.imsi,imsi,sizeof(session_params.imsi));

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt,RwLogtest_notif_CallidTest2, callid,(&session_params), "after-imsi-available-1");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_TRUE( rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "after-imsi-available-before-callid-configured");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_TRUE( rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);

  /* We now expect callid filter to be added. So add the same for gtest */
  BLOOM_SET_UINT64(category_filter[0].bitmap,&callidvalue.callid);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "after-imsi-available-after-callid-configured");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_FALSE(rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  callidvalue.callid=1510;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  /* Case where first log generated itself has configured key */
  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt,RwLogtest_notif_CallidTest2, callid,(&session_params), "first-log-imsi");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "first-log-imsi-before-callid-configured");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  BLOOM_SET_UINT64(category_filter[0].bitmap,&callidvalue.callid);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "first-log-imsi-after-callid-configured");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_FALSE(rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  BLOOM_RESET(category_filter[0].bitmap,imsi_field);

  munmap(filter_memory,RWLOG_FILTER_SHM_SIZE);
  rwlog_gtest_source_close(ctxt);
  close(fd);
}

TEST(RWLogSource, LogCheckNewMacro)
{
  int fd= -1;
  struct stat file_stats;
  int current_size, delta_size;
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_ERROR, 1); 

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "NEW_MACRO_TEST");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);
  current_size = file_stats.st_size;


  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, "NEW_MACRO_SEV_FAIL_TEST");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size == 0);
  current_size = file_stats.st_size;

  rwlog_gtest_source_close(ctxt);
  close(fd);

}


static void* rwlog_thread_test_routine(void *arg) 
{
  rw_call_id_t *callid, callidvalue;
  rwlog_ctx_t *ctxt = (rwlog_ctx_t *)arg;
  int i;
  struct timeval tv;
  const char *imsi = "123456789012346";

  memset(&callidvalue, 0, sizeof(callidvalue));


  callidvalue.callid=700;
  callidvalue.groupid=800;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid,"before-imsi-available");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, RWLOG_ATTR_SPRINTF("%s",imsi));
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);

  RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams_SessionParams) session_params;
  RWPB_F_MSG_INIT(RwLog_data_EvTemplate_TemplateParams_SessionParams, &session_params);
  session_params.has_imsi = 1;
  strncpy(session_params.imsi,imsi,sizeof(session_params.imsi));

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt,RwLogtest_notif_CallidTest2, callid,(&session_params), "after-imsi-available-1");
  EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_TRUE( rwlog_call_hash_is_present_for_callid(callidvalue.callid));

  for(i=1;i<10;i++) {
    callidvalue.callid=i;
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid,"loop-test");
    EXPECT_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);
  }

  return (NULL);
}


TEST(RWLogSource, RwLogMultiThreadTest)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  filter_memory_header  *filter_memory = NULL;


  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");

  rwlog_test_get_shm_filter(&filter_memory);

  const char *imsi_field = "123456789012346";
  /* Set bloom filter for imsi */
  filter_memory->skip_L2 = TRUE;

  rwlog_category_filter_t *category_filter = (rwlog_category_filter_t *)((char *)filter_memory + sizeof(rwlogd_shm_ctrl_t));
  BLOOM_SET(category_filter[0].bitmap,imsi_field);

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  int thread_result;
  pthread_t thread;

  thread_result = pthread_create(&thread, NULL,
                          rwlog_thread_test_routine, ctxt);

  if (0 == thread_result) {
    pthread_join(thread, NULL);
  }

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size > 0);

  munmap(filter_memory,RWLOG_FILTER_SHM_SIZE);
  rwlog_gtest_source_close(ctxt);
  close(fd);
}

TEST(RWLogSource, RwLogNoArgument)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test 1.25");
  
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest1Emergency);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  EXPECT_TRUE(delta_size>0);

  rwlog_gtest_source_close(ctxt);
  close(fd);
}

/* This test essentially tries to log some message when SHM is not yet initialised. 
   We need to check  we dont have any crash when test runs */
TEST(RWLogSource, RwLogInvalidShmFilter)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  char *rwlog_filename;
  char *filter_shm_name;
  rwlog_ctx_t *ctx;
  rw_call_id_t *callid, callidvalue;
  char data[16] = "TestData";
  UNUSED(delta_size);

  memset(&callidvalue, 0, sizeof(callidvalue));

  callidvalue.callid=700;
  callidvalue.groupid=800;
  callid = &callidvalue;

  // clean slate
  int r = asprintf (&rwlog_filename,
                    "%s-%d-%d-%lu",
                    RWLOG_FILE,
                    rwlog_get_systemId(),
                    getpid(),syscall(SYS_gettid));
  RW_ASSERT(r);

  r = asprintf (&filter_shm_name,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  RW_ASSERT(r);

  ctx = rwlog_init_internal("Gtest-Invalid-Shm",rwlog_filename,filter_shm_name,NULL);
  EXPECT_TRUE(ctx != NULL);
  RW_ASSERT(ctx);

  fd = open(ctx->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  rwlog_ctxt_dump(ctx);

  RWLOG_EVENT(ctx, RwLogtest_notif_FastpathEventLogTestEmergency, "NEW_MACRO_TEST");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  current_size = file_stats.st_size;


  RWLOG_EVENT_CALLID(ctx, RwLogtest_notif_CallidTest, callid,"before-imsi-available");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams_SessionParams) session_params;
  RWPB_F_MSG_INIT(RwLog_data_EvTemplate_TemplateParams_SessionParams, &session_params);
  session_params.has_imsi = 1;
  strncpy(session_params.imsi,"123456789012345",sizeof(session_params.imsi));

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctx,RwLogtest_notif_CallidTest2, callid,(&session_params), "after-imsi-available");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  current_size = file_stats.st_size;


  RWPB_T_MSG(RwLogtest_notif_CallidTest2) proto;
  RWPB_F_MSG_INIT(RwLogtest_notif_CallidTest2,&proto);
  proto.data = data;
  proto.has_template_params = 1;

  /* Call proto_send directly and check we dont have any issue */
  char cat_str[] = "rw-logtest";
  rwlog_proto_send_c(ctx, cat_str,(ProtobufCMessage *)&proto,callid,(common_params_t *)(&proto.template_params), __LINE__, (char *) __FILE__, &session_params,FALSE, FALSE,NULL); \
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  current_size = file_stats.st_size;

  rwlog_ctxt_dump(ctx);

  rwlog_gtest_source_close(ctx);
  close(fd);

  free(rwlog_filename);
  free(filter_shm_name);
}

TEST(RWLogSource, LogEventRateControl)
{
  TEST_DESCRIPTION("1.25: This test verifies rate control drop");
  int current_size, delta_size;
  int fd= -1, i;
  struct stat file_stats;
  uint64_t dropped;

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1");
  EXPECT_TRUE(ctxt->error_code == 0);

  rwlog_test_filter_set(RW_LOG_GTEST_CATEGORY, RW_LOG_LOG_SEVERITY_DEBUG, 1);
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  ASSERT_TRUE(fd>0);

  fstat(fd, &file_stats);
  current_size = file_stats.st_size; 

  //Event 1:
  //  fastpath-event-log-test-debug
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "abc");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_TRUE(delta_size);
  current_size = file_stats.st_size; 

  //Repeat 
  for (i = 0; i < RWLOG_PER_TICK_COUNTER-1; i++) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "abc");
    fstat(fd, &file_stats);
    delta_size = file_stats.st_size - current_size;
    EXPECT_TRUE(delta_size);
    current_size = file_stats.st_size; 
  }
  dropped = ctxt->stats.dropped;
  for (i = 0; i < RWLOG_PER_TICK_COUNTER-1; i++) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "abc");
    EXPECT_TRUE(ctxt->stats.dropped == dropped+i+1);
  }

  fstat(fd, &file_stats);
  current_size = file_stats.st_size; 
  for (i = 0; i < 100; i++) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "abc");
    fstat(fd, &file_stats);
    delta_size = file_stats.st_size - current_size;
    EXPECT_FALSE(delta_size);
    current_size = file_stats.st_size; 
  }

  for (i = 0; i < 100; i++) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest1Emergency);
    fstat(fd, &file_stats);
    delta_size = file_stats.st_size - current_size;
    EXPECT_TRUE(delta_size);
    current_size = file_stats.st_size; 
  }
  rwlog_gtest_source_close(ctxt);
  close (fd);
}
