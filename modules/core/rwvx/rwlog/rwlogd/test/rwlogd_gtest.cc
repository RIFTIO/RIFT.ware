
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */




/**
 * @file rwlogd_gtest.cc
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
#include "rwdts.h"
#include "rwut.h"
#include "rwlib.h"
#include "rwlog.h"
#include "rwlogd.h"
#include "rwlogd_common.h"
#include "rwlogd_rx.h"
#include "rwtypes.h"
#include "rwlogd_sink_common.hpp"
#include "rw-logtest.pb-c.h"
#include "rwdynschema-logtest.pb-c.h"
#include <sys/types.h>
extern "C" {
#include "rwsystem_timer.h"
#include "rwlogd_plugin_mgr.h"
};

#include <rwmsg.h>
#include <rwmsg_int.h>
#include <rwmsg_broker.h>

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


#define EXPECT_LOG_TRUE(x) if (!(x)) { rwlog_ctxt_dump(ctxt);((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->rwlog_dump_info(5); EXPECT_TRUE(x); }
/***************************************************************
 * Start Test for source side code 
 **************************************************************/
static char *verbose = getenv("VERBOSE");
#define  INCR_ERROR(x) ((!x)?err_cnt++:0)


rwlogd_instance_ptr_t rwlogd_gtest_allocate_instance_internal(int instance_id, bool dts_required, char *rwlog_filename, char *filter_shm_name)
{
  rwlogd_instance_ptr_t inst;
  rwsched_instance_ptr_t rwsched = NULL;

  setenv("__RWLOG_CLI_SINK_GTEST_PRINT_LOG__", "rw_log", 1);
  rwsched = rwsched_instance_new();

  /* For rwlogd gtest we use different filename. So close log instance in
   * rwsched instance and reopen with new filename */
  if(rwsched->rwlog_instance) 
  {
    rwlog_close_internal(rwsched->rwlog_instance,TRUE);
    rwsched->rwlog_instance = rwlog_init_internal("RW.Sched",rwlog_filename,filter_shm_name,NULL);
  }

  inst = (rwlogd_instance_ptr_t)RW_MALLOC0(sizeof(*inst));
  inst->rwtasklet_info =  (rwtasklet_info_t *) RW_MALLOC0(sizeof(rwtasklet_info_t));
  inst->rwtasklet_info->identity.rwtasklet_name = strdup("Gtest");
  inst->rwtasklet_info->identity.rwtasklet_instance_id = instance_id;
  inst->rwtasklet_info->rwsched_tasklet_info = rwsched_tasklet_new(rwsched);
  inst->rwtasklet_info->rwsched_instance= rwsched;
  inst->rwtasklet_info->rwtrace_instance = rwtrace_init();

  rwlog_init_bootstrap_filters(filter_shm_name);

  inst->rwtasklet_info->rwmsg_endpoint = 
      rwmsg_endpoint_create(1, 0, 0, inst->rwtasklet_info->rwsched_instance, 
                            inst->rwtasklet_info->rwsched_tasklet_info , 
                            inst->rwtasklet_info->rwtrace_instance, NULL);
  //ASSERT_TRUE(inst->rwtasklet_info->rwmsg_endpoint != NULL);

  rwlogd_start_tasklet(inst, dts_required,rwlog_filename,filter_shm_name,"rw-logtest");

  RWLOG_DEBUG_PRINT ("**starting test case %p \n", inst->rwlogd_info);

  rwlogd_tasklet_instance_ptr_t rwlogd_instance_data =(rwlogd_tasklet_instance_ptr_t )(inst->rwlogd_info);
  rwlogd_shm_reset_bootstrap(rwlogd_instance_data);
  rwlog_shm_set_dup_events(filter_shm_name, TRUE);
  stop_bootstrap_syslog_sink(inst);
  stop_rwlogd_console_sink(inst,RWLOGD_CONSOLE_SINK_NAME);
  return inst;
}

rwlogd_instance_ptr_t rwlogd_gtest_allocate_sysid_instance(int instance_id, bool dts_required)
{
  char *rwlog_filename;
  char *filter_shm_name;
  rwlogd_instance_ptr_t retval;
 
  // clean slate
  int r = asprintf (&rwlog_filename,
                    "%s-%d",
                    RWLOG_FILE,
                    rwlog_get_systemId());

  RW_ASSERT(r);
 
  r = asprintf (&filter_shm_name,
                      "%s-%d",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId());
  RW_ASSERT(r);

  retval = rwlogd_gtest_allocate_instance_internal(instance_id,dts_required, rwlog_filename,filter_shm_name);
  free(rwlog_filename);
  free(filter_shm_name);
  return(retval);
}

rwlogd_instance_ptr_t rwlogd_gtest_allocate_instance(int instance_id, bool dts_required)
{
  char *rwlog_filename;
  char *filter_shm_name;
  rwlogd_instance_ptr_t retval;
 
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

  retval = rwlogd_gtest_allocate_instance_internal(instance_id,dts_required, rwlog_filename,filter_shm_name);
  free(rwlog_filename);
  free(filter_shm_name);
  return(retval);
}
static rwlog_ctx_t *rwlog_gtest_source_init(const char *taskname)
{
  char *rwlog_filename;
  char *filter_shm_name;
  rwlog_ctx_t *ctx;
  
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

  ctx = rwlog_init_internal(taskname,rwlog_filename,filter_shm_name,NULL);

  free(rwlog_filename);
  free(filter_shm_name);

  return ctx;
}

static rwlog_ctx_t *rwlog_gtest_source_init_with_vnf(const char *taskname,uuid_t vnf_id)
{
  char *rwlog_filename;
  char *filter_shm_name;
  rwlog_ctx_t *ctx;
  
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

  ctx = rwlog_init_internal(taskname,rwlog_filename,filter_shm_name,vnf_id);
  RW_ASSERT(ctx);

  free(rwlog_filename);
  free(filter_shm_name);

  return ctx;
}




void rwlogd_gtest_free_instance(rwlogd_instance_ptr_t inst)
{
  RWLOG_DEBUG_PRINT ("**Stopping test case %p ", inst->rwlogd_info);

  ///Timer  - Enable once we support multiple instance of timer 
  EXPECT_TRUE(RW_STATUS_SUCCESS == rwtimer_deinit(inst->rwlogd_info->stw_system_handle,inst->rwtasklet_info, inst->rwlogd_info->timer));
  rwtasklet_info_ptr_t rwtasklet_info = inst->rwtasklet_info;
  rwsched_dispatch_source_cancel(rwtasklet_info->rwsched_tasklet_info,
                                 inst->rwlogd_info->connection_queue_timer);
  rwsched_dispatch_release(rwtasklet_info->rwsched_tasklet_info,
                          inst->rwlogd_info->connection_queue_timer);
  inst->rwlogd_info->connection_queue_timer = NULL;


  // DTS 
  rw_status_t rs = rwdts_api_deinit((rwdts_api_t *)inst->dts_h);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  rwmsg_srvchan_halt(inst->sc);
   rwmsg_endpoint_halt_flush(inst->rwtasklet_info->rwmsg_endpoint, TRUE);
  //RW_ASSERT(r);
  /// Tasklet
  free(inst->rwtasklet_info->identity.rwtasklet_name);
  rwsched_tasklet_free(inst->rwtasklet_info->rwsched_tasklet_info);
  rwsched_instance_free(inst->rwtasklet_info->rwsched_instance);
  rwtrace_ctx_close(inst->rwtasklet_info->rwtrace_instance);
  RW_FREE(inst->rwtasklet_info);

  // rwlogd specific
  rwlogd_sink_obj_deinit(inst->rwlogd_info);
   

  free(inst->rwlogd_info->rwlog_filename);

  RW_FREE(inst->rwlogd_info->log_buffer);
  if (inst->rwlogd_info->log_buffer_chkpt) {
    RW_FREE(inst->rwlogd_info->log_buffer_chkpt);
  }
  RW_FREE(inst->rwlogd_info);

  // instance
  RW_FREE(inst);

  RWLOG_DEBUG_PRINT ("**Stopped\n");
}

static bool rwlogtest_event(rwlog_ctx_t *ctxt, int size)
{
  int current_size, delta_size = 0;
  int fd= -1;
  struct stat file_stats;


  if (size <= 0 ) 
  {
    return false;
  }
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  char *user_string = (char *)malloc(size+16);
  int i = snprintf(user_string, size+16, "Writing %d : ", size);
  for (int j = 0;j<size; j++,i++)
  {
    user_string[i]= 48+(j%75);
  }
  user_string[i-1] = '\0';
  RW_ASSERT(i>8);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);
  free(user_string);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  if (delta_size < (size+16))
  {
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

static bool rwlogtest_callid(rwlog_ctx_t *ctxt,uint64_t call_id, uint64_t groupid)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  char user_string[] = "abcdef";

  memset(&callidvalue, 0 , sizeof(callidvalue));
  callidvalue.callid=call_id;
  callidvalue.groupid=groupid;
  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;
 
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, user_string);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  if (delta_size<(int)sizeof(user_string))
  {
    close(fd);
    return false;
  }
  close(fd);

  return true;
}
static int log_something(rwlog_ctx_t *ctxt, int loop_count)
{
  int err_cnt = 0;
  INCR_ERROR(rwlogtest_event(ctxt, 1));
  INCR_ERROR(rwlogtest_event(ctxt, 10));
  for (int i = 0; i <loop_count ; i++)
  {
    INCR_ERROR(rwlogtest_event(ctxt, 100));
  }
  INCR_ERROR(rwlogtest_event(ctxt, 1000));
  INCR_ERROR(rwlogtest_event(ctxt, 10000));
  return err_cnt;
}

static void log_packet(rwlog_ctx_t *ctxt, char *fname, int loop_count,uint32_t packet_type,uint32_t packet_direction)
{
  int fd;
  char buf[1024];
  char *rw_install_dir;
  struct stat file_stats;
  char *file_path;
  rw_install_dir = getenv("RIFT_INSTALL");
  file_path = RW_STRDUP_PRINTF("%s/usr/bin/%s", rw_install_dir, fname);
  fd = open(file_path, O_RDONLY, 0);
  if (fd < 0) {
    RWLOG_DEBUG_PRINT ("File not found\n");
    return;
  }
  fstat(fd, &file_stats);
  read(fd, buf, file_stats.st_size -1);
  rw_pkt_info_t pkt_info;
  memset(&pkt_info, 0 , sizeof(rw_pkt_info_t));
  pkt_info.packet_direction = packet_direction;
  pkt_info.packet_type = packet_type;
  pkt_info.packet_data.data = (uint8_t *)buf;
  pkt_info.packet_data.len = file_stats.st_size -1;


  for (int i = 0; i <loop_count ; i++)
  { 
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathPacketTrace, (&pkt_info));
  } 
  free(file_path);
  return;
}

static bool rwlog_validate_log_header(char *log)
{
  char hdr_format[] = "^20[0-9]{2}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]+Z\[([0-9]|[a-z]|-)+\\] version:1     .0 rwlogd_gtest.cc:[0-9]+notification:rw-logtest.";
  int reti;
  regex_t regex;

  reti = regcomp(&regex,hdr_format,REG_EXTENDED);
  if (reti) {
    return(FALSE);
  }
  reti = regexec(&regex, log, 0, NULL, 0);
  regfree(&regex);
  if (!reti) {
    return(TRUE);
  }
  else {
    return(FALSE);
  }
}

int rwlog_gtest_udp_open( uint16_t port)
{
    
int sockfd;
struct sockaddr_in addr;
struct timeval tv;

  sockfd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
  if (sockfd == -1 ) {
    perror("UDP Socket Open in rwlogd Gtest");
    return -1;
  }

  memset(&addr,0,sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr=inet_addr("127.0.0.1");
  addr.sin_port=htons(port);
  if (bind(sockfd, (const sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("UDP socket bind port error in rwlogGtest");
    close(sockfd);
    return -1;
  }
  tv.tv_sec = 5;  /* 1 Secs Timeout */
  tv.tv_usec = 0; 
  if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval)) == -1 ) {
    perror("UDP Setsockopt in rwlogd Gtest");
    close(sockfd);
    return -1;
  }
  return (sockfd);
}


bool rwlog_gtest_udp_read( int sockfd)
{
  char recvline[1000];
  int n;

  n=recvfrom(sockfd,recvline,sizeof(recvline),0,NULL,NULL);
  if ( n == -1 ) {
    perror("recevfom error in rwlogd Gtest");
    return FALSE;
  }
  recvline[n]=0;
  if (verbose) {
    printf("\n%s",recvline);
  }
  return TRUE;
}

TEST(RwLogdGroup1, TaskletStart)
{
  rw_status_t status;
  TEST_DESCRIPTION("1.1: This test verifies rwlogd task start");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.1");
  log_something(ctxt, 10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup1, TaskletStartNoFile)
{
  TEST_DESCRIPTION("1.2: This test verifies rwlogd task start With no file");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  rwlogd_gtest_free_instance(inst);
}



TEST(RwLogdGroup1, TaskletFileRotation)
{
  rw_status_t status;
  TEST_DESCRIPTION("1.3: This test verifies rwlogd task start with file rotation");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  //rwlogd_create_sink(rwlogd_cli_sink, inst, "google-test1.3" , "RW.Cli", 1);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.3");
  filter_memory_header *shmctl = (filter_memory_header *)ctxt->filter_memory;
  int start_ctrl = shmctl->rotation_serial;
  for(int i = 0; i< 10; i++)
  {
    log_something(ctxt, 10000); 
    rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_LOG_TRUE(start_ctrl!=shmctl->rotation_serial);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup1, CliSinkAdd)
{
  rw_status_t status;
  TEST_DESCRIPTION("1.4: This test verifies rwlogd cli sink add");
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.4");

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  log_something(ctxt,10);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("Gtest");
  EXPECT_EQ(sink->matched_log_count(),14);

  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup1, CliSinkMultipeOperations)
{
  rw_status_t status;
  TEST_DESCRIPTION("1.5: This test verifies rwlogd CLI Sink Multiple operation");
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_DUPLICATE);
  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_DUPLICATE);
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_NOTFOUND);

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest1" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest2", "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest2" , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest1", "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

#ifdef RWLOG_FILTER_DEBUG
  //Should crash with debug on
  rwlogd_create_sink(RWLOG_SINK_MAX, inst, "abc", "xyz",1); 
  rwlogd_create_sink(RWLOG_SINK_MAX, inst, "abc", "xyz",1);
  rwlogd_delete_sink(RWLOG_SINK_MAX, inst, "abc", "xyz",1);
#endif

  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup1, SyslogSinkAdd)
{
  rw_status_t status;
  uint16_t port = rand()&0xffff;
  int sockfd;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("1.6: This test verifies rwlogd task syslog sink add");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.6");

  rwlogd_create_sink(rwlogd_syslog_sink, inst, "Syslog", "localhost", port);
  sockfd = rwlog_gtest_udp_open(port);
  EXPECT_LOG_TRUE (sockfd != -1);
  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  
  for (int count = 0; count <14; count++) {
    EXPECT_LOG_TRUE(rwlog_gtest_udp_read(sockfd));
  }
  if (verbose) {
    printf("\n");
  }
  close(sockfd);
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("Syslog");
  EXPECT_EQ(sink->matched_log_count(),14);

  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Syslog", "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup2, SyslogSinkMultipeOperations)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("2.1: This test verifies rwlogd task Syslog sink Multiple operations");
  uint16_t port = rand()&0xffff;
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.7");

  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest" , "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest", "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  if (verbose) {
    printf("Testing Dups SyslogSinkMultipeOperations\n");
  }
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest" , "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest" , "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_DUPLICATE);
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest" , "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_DUPLICATE);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest", "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest", "localhost", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_NOTFOUND);

  if (verbose) {
    printf("Testing multiadds and reverse del SyslogSinkMultipeOperations\n");
  }
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest1" , "1.3.45.1", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest2", "1.3.45.1", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest2" , "1.3.45.1", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest1", "1.3.45.1", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  if (verbose) {
    printf("Testing  and reverse del SyslogSinkMultipeOperations\n");
  }
  status = rwlogd_create_sink(rwlogd_syslog_sink, inst, "Gtest" , "Rw.syslog", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlogd_delete_sink(rwlogd_syslog_sink, inst, "Gtest", "Rw.syslog", port);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_close_internal(ctxt,TRUE);

  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup2, CliPrint)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("2.2: This test verifies CLI print routines");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.8");

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("Gtest");

  EXPECT_EQ(sink->matched_log_count(),0);

  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),14);

  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup2, PacketPrint)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  TEST_DESCRIPTION("2.3: This test verifies rwlogd task start");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.9");

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  char category[] = "rw-logtest";
  status = rwlogd_sink_severity(inst,(char *)"Gtest",category,RW_LOG_LOG_SEVERITY_DEBUG,RW_LOG_ON_OFF_TYPE_ON);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  obj->set_dup_events_flag(FALSE);
  log_packet(ctxt,(char *)"packet.pkt", 100,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_OUTBOUND);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  rwlogd_sink *sink = obj->lookup_sink("Gtest");
  EXPECT_EQ(sink->matched_log_count(),100);

  status = rwlog_close_internal(ctxt,TRUE);
  obj->set_dup_events_flag(TRUE);
  rwlogd_delete_sink(rwlogd_cli_sink, inst, "Gtest", "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup2, CliShowAll)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.10");
  TEST_DESCRIPTION("2.4: Verify show-logs all");
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
  }
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,14);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup2, CliShowPktV3)
{
  rw_status_t status; 
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.11");
  TEST_DESCRIPTION("2.5: Verify show-logs verbosity 3");

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  char category[] = "rw-logtest";
  status = rwlogd_sink_severity(inst,(char *)"Gtest",category,RW_LOG_LOG_SEVERITY_DEBUG,RW_LOG_ON_OFF_TYPE_ON);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
 
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_packet(ctxt,(char *)"inbound_gtp.pkt", 10,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_OUTBOUND); 
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input->has_verbosity = TRUE;
  log_input->verbosity = 3;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,10);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
    if (log_output->logs[i]->pdu_detail) {
      if (verbose) {
        printf("%s\n", log_output->logs[i]->pdu_detail);
      }
    }
  }
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,10);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup3, CliShowPktV5)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.12");
  TEST_DESCRIPTION("3.1: Verify show-logs verbosity 5");

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  char category[] = "rw-logtest";
  status = rwlogd_sink_severity(inst,(char *)"Gtest",category,RW_LOG_LOG_SEVERITY_DEBUG,RW_LOG_ON_OFF_TYPE_ON);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
 

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_packet(ctxt,(char *)"outbound_gtp.pkt", 10,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_OUTBOUND);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input->has_verbosity = TRUE;
  log_input->verbosity = 5;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,10);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
    if (log_output->logs[i]->pdu_detail) {
      if (verbose) {
        printf("%s\n", log_output->logs[i]->pdu_detail);
      }
    }
    if (log_output->logs[i]->pdu_hex) {
      if (verbose) {
        printf("%s\n", log_output->logs[i]->pdu_hex);
      }
    }
  }
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_TRUE(log_output->n_logs==10);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup3, CliShowCallId)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("3.2: Verify show-logs callid");

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;
  sink_filter->has_callid=1;
  sink_filter->has_groupcallid=0;
  sink_filter->callid=112234;
  EXPECT_FALSE(rwlogtest_callid(ctxt,112234,445566));
 
  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter, config_filter);
  char sink_name[] = "Gtest-callid";

  config_filter->has_callid = TRUE;
  config_filter->callid = 112234;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_apply_filter_callgroup(inst, sink_name, config_filter);
  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_filter_callgroup(inst, default_sink_name, config_filter);

  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_LOG_TRUE(rwlogtest_callid(ctxt,112234,445566));

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);
  EXPECT_EQ(sink->matched_log_count(),2);


  EXPECT_FALSE(rwlogtest_callid(ctxt,112235,445566));
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  sink_filter->callid=112235;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;

  EXPECT_FALSE(rwlogtest_callid(ctxt,112235,112234)); 
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->log_filter_matched,2);

  sink_filter->callid= 0;
  sink_filter->has_callid= 0;
  sink_filter->has_groupcallid=1;
  sink_filter->groupcallid=112234;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup3, CliShowCallGroupId)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("3.4: Verify show-logs callid");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp = &category_p;
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.9");
  TEST_DESCRIPTION("3.4: Verify show-logs callid");

  EXPECT_FALSE(rwlogtest_callid(ctxt,112236,445566));
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category, category_p);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter, config_filter);
  char sink_name[] = "Gtest-groupid";

  config_filter->has_groupcallid = TRUE;
  config_filter->groupcallid = 445566;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_apply_filter_callgroup(inst, sink_name, config_filter);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_filter_callgroup(inst, default_sink_name, config_filter);

  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_LOG_TRUE(rwlogtest_callid(ctxt,112234,445566));
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),1);

  log_input->filter = sink_filter;
  sink_filter->category = category_pp;
  sink_filter->has_callid=0;
  sink_filter->has_groupcallid=0;
  sink_filter->groupcallid = 0;

  //category_p->name = RW_LOG_LOG_CATEGORY_RW_LOGTEST;
  strcpy(category_p->name,"rw-logtest");
  category_p->has_severity = 1;
  category_p->severity = RW_LOG_LOG_SEVERITY_ERROR;
  sink_filter->n_category = 1;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 0);

  sink_filter->has_callid=0;
  sink_filter->has_groupcallid=1;
  sink_filter->groupcallid = 445566;
  category_p->has_severity = 0;
  sink_filter->n_category = 0;
  sink_filter->category = NULL; 
  log_output->n_logs =0;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(category_p);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup3, RwLogAttributeFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.22");
  TEST_DESCRIPTION("3.5: Verify show-logs attribute filter");

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category_config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter_Category, category_config_filter);

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute) **attribute_config_pp =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute) *));

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute) *attribute_config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter_Category_Attribute, attribute_config_filter);
  char sink_name[] = "Gtest-attribute-filter";

  // Create Sink
  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  /// Round 1:
  // config >=-> filter category name all attribute name imei value 9234567890.1345
  // Log: imsi:9234567890.1345
  // Should not match 
  RWLOG_FILTER_L2_PRINT("\n\n\nfilter category name all attribute name imei value 9234567890.1345 \n");
  RWLOG_FILTER_L2_PRINT("imsi:9234567890.1345 \n");
  category_config_filter->n_attribute = 1;
  //category_config_filter->name = RW_LOG_LOG_CATEGORY_RW_LOGTEST;
  strcpy(category_config_filter->name,"rw-logtest");
  *attribute_config_pp = attribute_config_filter;
  category_config_filter->attribute = attribute_config_pp;

  strcpy(attribute_config_filter->name,"imei");
  attribute_config_filter->has_value = TRUE;
  strcpy(attribute_config_filter->value,"9234567890.1345");

  status = rwlog_apply_all_categories_filter(inst, sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_all_categories_filter(inst, default_sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  // Call Log
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_CallidImsiTest, "9234567890.1345","Created session for imei");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size); 
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  EXPECT_EQ(sink->matched_log_count(),0);
  // No log as attribute-name does not match
  log_output->n_logs = 0;

  /// Round 2: 
  // config -> filter category name all attribute name imsi value 1234567890.1345
  // Log imsi:123456789912345
  // Should not match 
  RWLOG_FILTER_L2_PRINT("\n\n\nfilter category name all attribute name imsi value 1234567890.1345 \n");
  RWLOG_FILTER_L2_PRINT("imsi:123456789912345 \n");
  strcpy(category_config_filter->name,"rw-logtest");

  strcpy(attribute_config_filter->name,"imsi");
  attribute_config_filter->has_value = TRUE;
  strcpy(attribute_config_filter->value,"1234567890.1345");

  status = rwlog_apply_all_categories_filter(inst, sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_apply_all_categories_filter(inst, default_sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  //Log 
  RWLOG_EVENT(ctxt, RwLogtest_notif_CallidImsiTest, "123456789912345","Created session for imsi");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(!delta_size); 
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  EXPECT_EQ(sink->matched_log_count(),0);
  // No log is written still 
  log_output->n_logs = 0;

  /// Round 3: 
  // config -> filter category name all attribute name imsi value 123456789543210
  // log imsi:123456789543210
  // Should match 
  RWLOG_FILTER_L2_PRINT("\n\n\nfilter category name all attribute name imsi value 123456789543210 \n");
  RWLOG_FILTER_L2_PRINT("imsi:123456789543210 \n");

  //category_config_filter->name = RW_LOG_LOG_CATEGORY_RW_LOGTEST;
  strcpy(category_config_filter->name,"rw-logtest");
  strcpy(attribute_config_filter->name,"imsi");
  attribute_config_filter->has_value = TRUE;
  strcpy(attribute_config_filter->value,"123456789543210");

  status = rwlog_apply_all_categories_filter(inst, sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_apply_all_categories_filter(inst, default_sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  //Log 
  RWLOG_EVENT(ctxt, RwLogtest_notif_CallidImsiTest, "123456789543210","Created session for imsi");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *));

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category, category_filter);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute) **attribute_pp = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute) *));

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute) *attribute_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category_Attribute, attribute_filter);

  /// Round 4 :
  // filter category name all attribute name imei value 9234567890.1345
  // Show log only
  sink_filter->n_category = 1;
  *category_pp = category_filter;
  sink_filter->category = category_pp;

  category_filter->n_attribute = 1;
  *attribute_pp = attribute_filter;
  //category_filter->name = RW_LOG_LOG_CATEGORY_ALL;
  strcpy(category_filter->name,"rw-logtest");
  category_filter->attribute = attribute_pp;

  strcpy(attribute_filter->name,"imei");
  attribute_filter->has_value = TRUE;
  strcpy(attribute_filter->value,"9234567890.1345");

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),1);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_LOG_TRUE(log_output->n_logs==0);
  log_output->n_logs = 0;


  strcpy(attribute_filter->name,"imsi");
  attribute_filter->has_value = TRUE;
  strcpy(attribute_filter->value,"123456789543210");
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_LOG_TRUE(log_output->n_logs==1);
  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->rwlog_dump_info(5);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  log_output->n_logs = 0;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(category_config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup3, CliShowFilterCategoryAllStartTimeEndTime)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("3.6: Verify show-logs callid");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  char start_time[128];
  char end_time[128];

  struct timeval _tv;
  gettimeofday(&_tv, NULL); 
  time_t evtime = _tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(start_time, sizeof(start_time), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(start_time+tmstroff, ".%06uZ", (uint32_t)_tv.tv_usec);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp = &category_p;
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.15");
  TEST_DESCRIPTION("3.6: Verify show-logs start-time end-time filter category all ");

  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal,log_output);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs,log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter,sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category,category_p);

  log_input->filter = sink_filter;
  log_input->has_count = 1;
  log_input->count =3;

  strcpy(category_p->name,"all");
  sink_filter->category = category_pp;
  sink_filter->n_category = 1;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 3);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
    log_output->logs[i] = NULL;
  }

  log_input->has_count = 0;
  log_input->count =0;
  gettimeofday(&_tv, NULL); 
  evtime = _tv.tv_sec;
  tm = gmtime(&evtime);
  tmstroff = strftime(end_time, sizeof(end_time), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(end_time+tmstroff, ".%06uZ", (uint32_t)_tv.tv_usec);

  log_input->filter = sink_filter;
  log_input->start_time = strdup(start_time);
  log_input->end_time = strdup(end_time);
  category_p->has_severity = 0;
  sink_filter->n_category = 0;
  sink_filter->category = NULL;

  log_output->n_logs =0;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,14);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  protobuf_free(category_p); // As category is set to null
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup4, CliShowCallIdCategory)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p =
              (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp = &category_p;

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.19");
  TEST_DESCRIPTION("4.1: Verify show-logs callid category");

  /* Add call id filter directly */
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  char callid_field[] = "callid";
  obj->merge_filter_uint64(FILTER_ADD,(uint32_t)0, callid_field, 112233);

  EXPECT_LOG_TRUE(rwlogtest_callid(ctxt,112233, 234352));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category,category_p);

  log_input->filter = sink_filter;
  sink_filter->has_callid=1;
  sink_filter->callid=112233;

  sink_filter->category = category_pp;
  sink_filter->n_category = 1;
  strcpy(category_p->name,"rw-logtest");

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
  }
  sink_filter->category = NULL;

  protobuf_free(category_p); // As category is set to null
  protobuf_free(log_output);
  protobuf_free(log_input);
  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup4, CliShowFilterCategoryMsgId)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("4.2: Verify show-logs msg-id");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlog_msg_id_key_t  msg_id;
  char start_time[128];

  memset(&msg_id,0,sizeof(msg_id));

  struct timeval _tv;
  gettimeofday(&_tv, NULL); 
  time_t evtime = _tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(start_time, sizeof(start_time), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(start_time+tmstroff, ".%06uZ", (uint32_t)_tv.tv_usec);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp = &category_p;
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.16");
  TEST_DESCRIPTION("4.2: Verify show-logs msg-id <msg-id> ");

  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal,log_output);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs,log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter,sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category,category_p);

  log_input->filter = sink_filter;
  log_input->has_count = TRUE;
  log_input->count = 1;

  strcpy(category_p->name,"all");
  sink_filter->category = category_pp;
  sink_filter->n_category = 1;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 1);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    if (i == 0) {
      msg_id.tv_sec = log_output->logs[i]->tv_sec;
      msg_id.tv_usec = log_output->logs[i]->tv_usec;
      msg_id.pid = log_output->logs[i]->process_id;
      msg_id.tid = log_output->logs[i]->thread_id;
      msg_id.seqno = log_output->logs[i]->seqno;
      strcpy(msg_id.hostname, log_output->logs[i]->hostname);
    }
    protobuf_free(log_output->logs[i]);
    log_output->logs[i] = NULL;
  }
  log_input->has_count = TRUE;
  log_input->count = 1;
  log_input->filter = sink_filter;
  log_input->has_tv_sec = 1;
  log_input->tv_sec = msg_id.tv_sec;
  log_input->has_tv_usec = 1;
  log_input->tv_usec = msg_id.tv_usec;
  log_input->has_thread_id = 1;
  log_input->thread_id = msg_id.tid;
  log_input->has_process_id = 1;
  log_input->process_id = msg_id.pid;
  log_input->has_seqno = 1;
  log_input->seqno = msg_id.seqno;
  log_input->has_hostname = 1;
  strcpy(log_input->hostname , msg_id.hostname);
  category_p->has_severity = 0;
  sink_filter->n_category = 0;
  sink_filter->category = NULL;
  log_output->n_logs =0;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    protobuf_free(log_output->logs[i]);
    log_output->logs[i] = NULL;
  }
  log_input->has_count = TRUE;
  log_input->count = 1;
  log_input->filter = sink_filter;
  memset(&msg_id, 0,sizeof(msg_id));
  log_input->has_tv_sec = 1;
  log_input->tv_sec = msg_id.tv_sec;
  log_input->has_tv_usec = 1;
  log_input->tv_usec = msg_id.tv_usec;
  log_input->has_thread_id = 1;
  log_input->thread_id = msg_id.tid;
  log_input->has_process_id = 1;
  log_input->process_id = msg_id.pid;
  log_input->has_seqno = 1;
  log_input->seqno = msg_id.seqno;
  log_input->has_hostname = 1;
  strcpy(log_input->hostname , msg_id.hostname);
  category_p->has_severity = 0;
  sink_filter->n_category = 0;
  sink_filter->category = NULL;
  log_output->n_logs =0;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    protobuf_free(log_output->logs[i]);
    log_output->logs[i] = NULL;
  }
  log_input->has_count = TRUE;
  log_input->count = 1;
  log_input->filter = sink_filter;
  log_input->has_tv_sec = 1;
  log_input->tv_sec = -1;
  log_input->has_tv_usec = 1;
  log_input->tv_usec = -1;
  log_input->has_process_id = 1;
  log_input->process_id = -1;
  log_input->has_thread_id = 1;
  log_input->thread_id = -1;
  log_input->seqno = -1;
  log_input->has_hostname = 1;
  strcpy(log_input->hostname, "junk");
  category_p->has_severity = 0;
  sink_filter->n_category = 0;
  sink_filter->category = NULL;
  log_output->n_logs =0;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(category_p);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup4, ResetBootStrapShm)
{
  TEST_DESCRIPTION("4.4: This test verifies rwlogd task start With no file");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  inst->rwlogd_info->bootstrap_counter = BOOTSTRAP_TIME-1;
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_TRUE(inst->rwlogd_info->bootstrap_completed);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup4, CircularBufferEnd)
{
  rw_status_t status;
  TEST_DESCRIPTION("4.5: This test verifies rwlogd task start with file rotation");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  int i = 0;

  RW_FREE(inst->rwlogd_info->log_buffer);

  inst->rwlogd_info->log_buffer_size = 100*1024;
  inst->rwlogd_info->curr_used_offset = inst->rwlogd_info->log_buffer_size;
  inst->rwlogd_info->log_buffer = (char *)RW_MALLOC(inst->rwlogd_info->log_buffer_size); 


  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.18");
  for(i=0;i<10;i++) {
    log_something(ctxt, 100);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_LOG_TRUE(inst->rwlogd_info->stats.circ_buff_wrap_around>0);
#if 0
  printf("wrap around counter value is %lu\n",inst->rwlogd_info->stats.circ_buff_wrap_around);
#endif

  for(i=0;i<10;i++) {
    log_something(ctxt, 50);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_LOG_TRUE(inst->rwlogd_info->bootstrap_counter!=0);
  EXPECT_LOG_TRUE(inst->rwlogd_info->stats.circ_buff_wrap_around>0);
#if 0
  printf("wrap around counter value is %lu\n",inst->rwlogd_info->stats.circ_buff_wrap_around);
#endif

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
  ring_buffer_stats_t stats;
  sink->get_log_ring_stats(&stats);

#if 0
  printf("cur_size: %u, max_size: %u\n",stats.cur_size, stats.max_size);
  printf("inserts: %u, removal success: %u removal failure: %u\n",stats.inserts, stats.removal_success,stats.removal_failure);
#endif

  EXPECT_EQ(sink->log_filter_matched,1580);
  EXPECT_EQ(stats.removal_failure,0); 
  EXPECT_EQ(stats.inserts, sink->log_filter_matched);
  EXPECT_EQ(stats.cur_size,stats.inserts- stats.removal_success);
  EXPECT_TRUE(stats.removal_success > 0);

  rwlogd_dump_task_info(inst->rwlogd_info,1,1);

  /* Remove all the entries from ring buffer */
  rwlogd_clear_log_from_sink(inst);
  sink->get_log_ring_stats(&stats);
  EXPECT_EQ(stats.cur_size,0);
  EXPECT_EQ(stats.inserts,stats.removal_success);


  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup4, FetchCircularBufferEnd)
{
  rw_status_t status;
  TEST_DESCRIPTION("4.6: This test verifies Circular Buffer FetchLogs");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  int i = 0;

  EXPECT_FALSE(inst->rwlogd_info->log_buffer == NULL);
  RW_FREE(inst->rwlogd_info->log_buffer);
  inst->rwlogd_info->log_buffer_size = 100*1024;
  inst->rwlogd_info->curr_used_offset = inst->rwlogd_info->log_buffer_size;
  inst->rwlogd_info->log_buffer = (char *)RW_MALLOC(inst->rwlogd_info->log_buffer_size); 

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.19");

  for(i=0;i<5;i++) {
    log_something(ctxt, 100);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_LOG_TRUE(inst->rwlogd_info->stats.circ_buff_wrap_around>0);

#if 0
  printf("wrap around counter value is %lu\n",inst->rwlogd_info->stats.circ_buff_wrap_around);
#endif

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
  ring_buffer_stats_t stats;
  sink->get_log_ring_stats(&stats);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);


  log_input->has_count = TRUE;
  log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(sink->log_filter_matched,520);
  EXPECT_EQ(stats.removal_failure,0); 
  EXPECT_EQ(stats.inserts, sink->log_filter_matched);
  EXPECT_EQ(stats.cur_size,stats.inserts- stats.removal_success);
  EXPECT_TRUE(stats.removal_success > 0);

  EXPECT_EQ(stats.cur_size,log_output->n_logs);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
#if 0
    printf("%s\n", log_output->logs[i]->msg);
#endif
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }


  protobuf_free(log_output);
  protobuf_free(log_input);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup5, CliShowCallIdTime)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.20");
  TEST_DESCRIPTION("5.1: Verify show-logs callid timetaken");

  /* Add call id filter directly */
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  char callid_field[] = "callid";
  obj->merge_filter_uint64(FILTER_ADD,(uint32_t)0, callid_field, 112233);

  for (int i =0; i<21000; i++)
  { 
    EXPECT_LOG_TRUE(rwlogtest_callid(ctxt,112233, 234352));
  }

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;
  sink_filter->has_callid=1;
  sink_filter->has_groupcallid=0;
  sink_filter->callid=112234;
  //unsetenv("__RWLOG_CLI_SINK_GTEST_PRINT_LOG__");
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.5, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  EXPECT_EQ(log_output->log_summary[0]->logs_muted,20*1024);
  EXPECT_LOG_TRUE(log_output->log_summary[0]->time_spent<500);
  if (verbose) {
    printf("Time Spent = %lu ms Found %d Muted %d\n", 
         log_output->log_summary[0]->time_spent,
         (int)log_output->n_logs,
         (int)log_output->log_summary[0]->logs_muted);
  }

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;
  sink_filter->has_callid=1;
  sink_filter->has_groupcallid=0;
  sink_filter->callid=112233;
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,100);
  EXPECT_EQ(log_output->log_summary[0]->logs_muted,0);
  EXPECT_LOG_TRUE(log_output->log_summary[0]->time_spent<500);
  if (verbose) {
    printf("Time Spent = %lu ms Found %d Muted %d\n", 
         log_output->log_summary[0]->time_spent,
         (int)log_output->n_logs,
         (int)log_output->log_summary[0]->logs_muted);
  }
  protobuf_free(log_output);
  protobuf_free(log_input);
  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup5, DISABLED_CliShowCallIdTimeUserFile)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  TEST_DESCRIPTION("5.2: Verify show-logs callid timetaken user file");
  char *rwlog_filename;
  asprintf (&rwlog_filename, "%s-%d", RWLOG_FILE, rwlog_get_systemId());
  int fd_r = open("/tmp/user-file", O_RDONLY, 0);
  if (fd_r<0)
  {
    printf("No File : Test Skipped\n");
    return;
  }
  int fd_w = open(rwlog_filename, O_APPEND|O_CREAT|O_WRONLY, S_IRWXU);
  if (fd_w<0)
  {
    printf("No File : Test Skipped\n");
    return;
  }
  int i= 0;
  int size = 0;
  while (1)
  {
    size = read(fd_r, &i, sizeof(int));
    if (size == 0)
    {
      break;
    }
    write(fd_w,&i,4);
  }

  unsetenv("__RWLOG_CLI_SINK_GTEST_PRINT_LOG__");

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;
  sink_filter->has_callid=1;
  sink_filter->has_groupcallid=0;
  sink_filter->callid=112233;
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_TRUE(log_output->log_summary[0]->time_spent<500);
  if (verbose) {
    printf("Time Spent = %lu ms Found %d Searched %d\n", 
         log_output->log_summary[0]->time_spent,
         (int)log_output->n_logs,
         (int)log_output->log_summary[0]->logs_muted);
  }

  protobuf_free(log_output);
  protobuf_free(log_input);
  close(fd_w);
  close(fd_r);
  free(rwlog_filename);
  rwlogd_gtest_free_instance(inst);
}

static bool rwlogtestprint_event(rwlog_ctx_t *ctxt, int size)
{
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;


  if (size <= 0 ) 
  {
    return false;
  }
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  char *user_string = (char *)malloc(size);
  for (int i=0; i< size; i++)
  {
    user_string[i]= 1+(i%254);
  }
  user_string[size-1] = 0;
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);
  free(user_string);
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;

  if (delta_size<(int)sizeof(user_string))
  {
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

TEST(RwLogdGroup5, CliShowAllUnprintable)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env) {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.10");
  TEST_DESCRIPTION("5.3: Verify show-logs all for unprintable characters");
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 1));
  EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 10));
  for (int i = 0; i <10 ; i++) {
    EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 100));
  }
  EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 1000));
  EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 10000));

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
  }
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,14);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}
TEST(RwLogdGroup5, CliCacheWrapAroundPrintStats)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  TEST_DESCRIPTION("5.4: This test verifies CLI print stats");

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.10");
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->gtest_fake_cache_entries();

  log_something(ctxt,1);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
  }
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,5);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup6, RwLogSessionParamFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "sess-param-fillter";
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");
  TEST_DESCRIPTION("5.6: Verify show-logs session-params filter");

  //status = rwlogd_create_sink(SINK_CLI, inst, sink_name , "RW.Cli", 1);
  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"before-imsi-available");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),0);

  RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams_SessionParams) session_params;
  RWPB_F_MSG_INIT(RwLog_data_EvTemplate_TemplateParams_SessionParams, &session_params);
  session_params.has_imsi = 1;
  strncpy(session_params.imsi,"1234567890.1345",sizeof(session_params.imsi));

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,(&session_params), 
                                         "Created session for imsi before filter configuration");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),0);

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams) *session_param_config_filter = 
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_SessionParams)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter_SessionParams,session_param_config_filter);

  session_param_config_filter->has_imsi = TRUE;
  strcpy(session_param_config_filter->imsi,"1234567890.1345");

  status = rwlog_apply_session_params_filter(inst, sink_name, session_param_config_filter,FALSE,RW_LOG_ON_OFF_TYPE_OFF);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_session_params_filter(inst, default_sink_name,session_param_config_filter,FALSE,RW_LOG_ON_OFF_TYPE_OFF);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=1500;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"before-imsi-available");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),0);

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,(&session_params), "Created session for imsi after filter configuration");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);

  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"after-imsi-available");

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == TRUE);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,3);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),3);

  gettimeofday(&tv, NULL);
  callidvalue.call_arrived_time = tv.tv_sec;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"after-imsi-available-config-available");

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == FALSE);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,4);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),4);

  char tmstr[RWLOG_TIME_STRSZ];
  gettimeofday(&tv, NULL);
  time_t evtime = tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06ldZ", tv.tv_usec);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidInvalidateTest, callid,tmstr,"invalidating callid for session");

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == FALSE);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs(inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,5);
  log_output->n_logs = 0;
  EXPECT_EQ(sink->matched_log_count(),5);

  rwlogd_sink_data *rwlogd_sink_obj = (rwlogd_sink_data *)inst->rwlogd_info->sink_data;
  char field[] = "callid";
  for (auto &pe: *rwlogd_sink_obj->sink_queue_) 
  { 
    pe->remove_generated_callid_filter(callid->callid); 
  }

  rwlogd_sink_obj->merge_filter_uint64(FILTER_DEL, (uint32_t)RW_LOG_LOG_CATEGORY_ALL, field, callid->callid);

  //rwlogd_remove_callid_filter(inst->rwlogd_info,callid->callid);
  callid->serial_no = ((filter_memory_header *)(ctxt)->filter_memory)->log_serial_no;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid," log after invalidating callid");

  //EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  //EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == FALSE);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(session_param_config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup6, RwLogValidateLogFields)
{ 
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env) {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
    (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
    (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal))); 
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.24");
  TEST_DESCRIPTION("6.1: Verify show-logs all for unprintable characters");
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  EXPECT_LOG_TRUE(rwlogtestprint_event(ctxt, 10));

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_LOG_TRUE(log_output->n_logs == 1);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  { 
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_LOG_TRUE(strlen(log_output->logs[i]->msg) > 64);
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }

  char log[512];
  strcpy(log, log_output->logs[0]->msg);
  char *logp = log_output->logs[0]->msg;
  char output[] = "notification:rw-logtest.fastpath-event-log-test severity:error event-id:1007 data:\\001\\002\\003\\004\\005\\006\\007\\010\\t";
  int cnt = 0;
  char *p = strtok(log," ");
  while (p != NULL) {
    if (strncmp(p,"noti",4) == 0)
      break;
    cnt += strlen(p)+1;
    p = strtok(NULL," ");
  }
  p = logp + cnt;
  EXPECT_LOG_TRUE(strncmp(output,p,sizeof(output)) == 0);

  char outputhdr[] = "2015-02-03T05:57:35.258003Z[ganga.blr.riftio.com@google-test1.10-75033-75033] version:1.0 rwlogd_gtest.cc:1442";
  char tmp_str[512];
  strcpy(tmp_str,outputhdr);
  char *date_hr = strtok(tmp_str,":");
  char *minstr = strtok(NULL,":");
  int minu = atoi(minstr);

  char tmstr[128];
  tmstr[0]='\0';
  struct timeval _tv;
  gettimeofday(&_tv, NULL); 
  time_t evtime = _tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);

  EXPECT_LOG_TRUE(strncmp(date_hr,tmstr,strlen(date_hr)));

  EXPECT_LOG_TRUE((tm->tm_min - minu) < 5);

  
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup6, SpeculativeCallEntryCleanup)
{
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  rw_status_t status;
  char sink_name[] = "Gtest-callid";
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.25");
  ctxt->speculative_buffer_window = 2;

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  callidvalue.callid=100;
  callidvalue.groupid=200;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec; 
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-wait-time-expiry-1");
  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 1);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "before-wait-time-expiry-2");
  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == TRUE);

  char user_string[] = "test string";
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == TRUE);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 3, NULL);
  EXPECT_EQ(sink->matched_log_count(),1);

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 2);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == TRUE);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size == 0);


  /* After Callid buffer interval, any other will give oppurtunity to cleanup
   * call log entry */
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size > 0);
  current_size = file_stats.st_size;

  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == FALSE);

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, "after-wait-time-expiry");
  EXPECT_LOG_TRUE(rwlog_gtest_get_no_of_logs_for_call(callidvalue.callid) == 0);
  EXPECT_LOG_TRUE(rwlog_call_hash_is_present_for_callid(callidvalue.callid) == FALSE);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_EQ(sink->matched_log_count(),2);

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size == 0);
  
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup6, SrcDenyEventIdTest)
{
  rw_status_t status;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-RwlogdDenyEventId.1.36");
  TEST_DESCRIPTION("6.2: Verify deny event Id");

  /* Add rwlogd sink filter directly */
  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "filterme");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size > 0);

  current_size = file_stats.st_size;
  //char *event_name = (char *) "1000";
  status =
    rwlogd_shm_filter_operation_uint64(FILTER_DENY_EVID,
                                   inst, (char *) "event-id", (uint64_t) 1000);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestEmergency, "filterme");
  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size == 0);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup6, RwLogNextCallFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "next-call-fillter";
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");
  TEST_DESCRIPTION("6.3 Verify show-logs filter next-call");

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_LogEvent_Filter,config_filter);

  config_filter->has_next_call = TRUE;
  config_filter->next_call = RW_LOG_ON_OFF_TYPE_ON;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  status = rwlog_apply_next_call_filter(inst, sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  //FIXME:  Enabling this code the Test should pass
  //char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  //status = rwlog_apply_next_call_filter(inst, default_sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event1");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;


  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event2");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),2);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  char tmstr[RWLOG_TIME_STRSZ];
  gettimeofday(&tv, NULL);
  time_t evtime = tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06ldZ", tv.tv_usec);

  callidvalue.call_arrived_time = 0;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidInvalidateTest, callid,tmstr,"invalidating callid for session");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),3);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,3);
  log_output->n_logs = 0;

  callidvalue.callid=1500;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"second call after next-call");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size==0);
  current_size = file_stats.st_size;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup7, RwLogNextCallSessionParamFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "next-call-fillter";
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");
  TEST_DESCRIPTION("7.1: Verify show-logs filter next-call session-param imsi");

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_LogEvent_Filter,config_filter);

  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_SessionParams) *session_param_config_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_SessionParams) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter_SessionParams)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_LogEvent_Filter_SessionParams,session_param_config_filter);

  session_param_config_filter->has_imsi = TRUE;
  strcpy(session_param_config_filter->imsi,"987654321054321");

  config_filter->session_params = session_param_config_filter;

  config_filter->has_next_call = TRUE;
  config_filter->next_call = RW_LOG_ON_OFF_TYPE_ON;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  status = rwlog_apply_next_call_filter(inst, sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  // FIXME:
  // char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  // status = rwlog_apply_next_call_filter(inst, default_sink_name, config_filter);
  // EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWPB_T_MSG(RwLog_data_EvTemplate_TemplateParams_SessionParams) session_params;
  RWPB_F_MSG_INIT(RwLog_data_EvTemplate_TemplateParams_SessionParams, &session_params);
  session_params.has_imsi = 1;
  strncpy(session_params.imsi,"987654321054321",sizeof(session_params.imsi));

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,(&session_params), 
                                         "Created session for imsi next call  filter configuration");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;


  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event2");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),2);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  char tmstr[RWLOG_TIME_STRSZ];
  gettimeofday(&tv, NULL);
  time_t evtime = tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06ldZ", tv.tv_usec);

  callidvalue.call_arrived_time = 0;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidInvalidateTest, callid,tmstr,"invalidating callid for session");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),3);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,3);
  log_output->n_logs = 0;

  callidvalue.callid=1500;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,(&session_params), 
                                         "Created second session for imsi next call  filter configuration");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup7, RwLogNextFailedCallFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "next-failed-call-fillter";
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");
  TEST_DESCRIPTION("7.2: Verify show-logs filter next-failed-call");

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *config_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_LogEvent_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_LogEvent_Filter,config_filter);

  config_filter->has_next_failed_call = TRUE;
  config_filter->next_failed_call = RW_LOG_ON_OFF_TYPE_ON;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  status = rwlog_apply_next_call_filter(inst, sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  //FIXME:  Enabling this code the Test should pass
  //char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  //status = rwlog_apply_next_call_filter(inst, default_sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event1");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;


  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event2");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallFailureDebugTest, callid,"session-failed log-event3");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-failed-call session log-event4");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),4);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,4);
  log_output->n_logs = 0;

  char tmstr[RWLOG_TIME_STRSZ];
  gettimeofday(&tv, NULL);
  time_t evtime = tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06ldZ", tv.tv_usec);

  callidvalue.call_arrived_time = 0;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidInvalidateTest, callid,tmstr,"invalidating callid for session");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),5);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,5);
  log_output->n_logs = 0;

  callidvalue.callid=1500;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"second call after next-call");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallFailureDebugTest, callid,"session-failed log-event3");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup7, RwLogFailedCallFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "next-failed-call-fillter";
  rw_call_id_t *callid, callidvalue;
  struct timeval tv;
  int current_size, delta_size;
  int fd= -1;
  struct stat file_stats;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->enable_L1_filter();

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.23");
  TEST_DESCRIPTION("7.3: Verify show-logs filter next-failed-call");

  fd = open(ctxt->rwlog_filename, O_RDONLY, 0);
  fstat(fd, &file_stats);
  current_size = file_stats.st_size;

  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter = 
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter,config_filter);

  config_filter->has_failed_call_recording = TRUE;
  config_filter->failed_call_recording = RW_LOG_ON_OFF_TYPE_ON;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  status = rwlog_apply_failed_call_recording_filter(inst, sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_failed_call_recording_filter(inst, default_sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=500;
  callidvalue.groupid=600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event1");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;


  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-call session log-event2");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallFailureTest, callid,"session-failed log-event3");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"next-failed-call session log-event4");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_EQ(sink->matched_log_count(),4);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,4);
  log_output->n_logs = 0;

  char tmstr[RWLOG_TIME_STRSZ];
  gettimeofday(&tv, NULL);
  time_t evtime = tv.tv_sec;
  struct tm* tm = gmtime(&evtime);
  int tmstroff = strftime(tmstr, sizeof(tmstr), "%Y-%m-%dT%H:%M:%S", tm);
  sprintf(tmstr+tmstroff, ".%06ldZ", tv.tv_usec);

  callidvalue.call_arrived_time = 0;
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidInvalidateTest, callid,tmstr,"invalidating callid for session");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_EQ(sink->matched_log_count(),5);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,5);
  log_output->n_logs = 0;

  callidvalue.callid=1500;
  callidvalue.groupid=1600;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"second call after next-call");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallFailureDebugTest, callid,"session-failed log-event3");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_LOG_TRUE(delta_size);
  current_size = file_stats.st_size;

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_EQ(sink->matched_log_count(),7);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,7);
  log_output->n_logs = 0;

  config_filter->has_failed_call_recording = TRUE;
  config_filter->failed_call_recording = RW_LOG_ON_OFF_TYPE_OFF;

  status = rwlog_apply_failed_call_recording_filter(inst, sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_apply_failed_call_recording_filter(inst, default_sink_name, config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  callidvalue.callid=900;
  callidvalue.groupid=1000;
  gettimeofday(&tv, NULL);

  callidvalue.call_arrived_time = tv.tv_sec;
  callid = &callidvalue;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest2, callid,"third call after failed-call-recording disabled");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);
  current_size = file_stats.st_size;

  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallFailureDebugTest, callid,"session-failed log-event3");

  fstat(fd, &file_stats);
  delta_size = file_stats.st_size - current_size;
  EXPECT_FALSE(delta_size);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup7, L2ExactFilterCallId)
{
  char *env = getenv("YUMA_MODPATH");
  uint32_t cat;
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.24");
  TEST_DESCRIPTION("7.4:  callid l2 Exact filter");

  /* Add call id filter directly */
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  char callid_field[] = "callid";

  for (cat = 0; cat < obj->num_categories_; cat++) {
    obj->merge_filter_uint64(FILTER_ADD,(uint32_t)cat, callid_field, 112233);
    obj->merge_filter_uint64(FILTER_ADD,(uint32_t)cat, callid_field, 112233);
  }
  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->rwlog_dump_info(5);
  for (int i = 0; cat < obj->num_categories_; cat++) {
    EXPECT_LOG_TRUE(obj->l2_exact_uint64_match((uint32_t)i, callid_field, 112233));
  }

  for (cat = 0; cat < obj->num_categories_; cat++) {
    obj->merge_filter_uint64(FILTER_DEL,(uint32_t)cat, callid_field, 112233);
    obj->merge_filter_uint64(FILTER_DEL,(uint32_t)cat, callid_field, 112233);
  }
  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->rwlog_dump_info(5);
  for (cat = 0; cat < obj->num_categories_; cat++) {
    EXPECT_LOG_TRUE(!obj->l2_exact_uint64_match((uint32_t)cat, callid_field, 112233));
  }

  EXPECT_LOG_TRUE(RW_STATUS_SUCCESS == rwlog_close_internal(ctxt,TRUE));
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup7, SeverityFilterCheck)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  rw_call_id_t *callid, callidvalue;
  char user_string[] = "test string";

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *));

  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("7.5: Verify show-logs callid");

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  char category_str[] = "rw-logtest";
  uint32_t category = rwlogd_map_category_string_to_index(inst, category_str);
  obj->set_severity(category,RW_LOG_LOG_SEVERITY_DEBUG);

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category, category_p);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  log_input->filter = sink_filter;
  *category_pp = category_p;
  sink_filter->category = category_pp;

  //category_p->name = RW_LOG_LOG_CATEGORY_RW_LOGTEST;
  strcpy(category_p->name,"rw-logtest");
  category_p->has_severity = 1;
  category_p->severity = RW_LOG_LOG_SEVERITY_ERROR;
  sink_filter->n_category = 1;

  callidvalue.callid=100;
  callidvalue.groupid=200;
  callid = &callidvalue;

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestWarning, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestWarning, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestError, callid, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  category_p->severity = RW_LOG_LOG_SEVERITY_DEBUG;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,8);
  log_output->n_logs = 0;

  category_p->severity = RW_LOG_LOG_SEVERITY_INFO;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,6);
  log_output->n_logs = 0;

  category_p->severity = RW_LOG_LOG_SEVERITY_CRITICAL;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;

  sink_filter->has_callid=1;
  sink_filter->callid=100;

  category_p->severity = RW_LOG_LOG_SEVERITY_INFO;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,3);
  log_output->n_logs = 0;

  category_p->severity = RW_LOG_LOG_SEVERITY_ERROR;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,1);
  log_output->n_logs = 0;


  category_p->severity = RW_LOG_LOG_SEVERITY_DEBUG;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,4);
  log_output->n_logs = 0;

  /* Try severity only filter */
  protobuf_free(log_input);
  log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);

  log_input->has_severity = TRUE;
  log_input->severity = RW_LOG_LOG_SEVERITY_ERROR;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  log_input->severity = RW_LOG_LOG_SEVERITY_DEBUG;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,8);
  log_output->n_logs = 0;

  log_input->severity = RW_LOG_LOG_SEVERITY_INFO;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,6);
  log_output->n_logs = 0;

  log_input->severity = RW_LOG_LOG_SEVERITY_CRITICAL;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs = 0;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup8, ShowLogTrailingTimestampFilter)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  rw_call_id_t *callid, callidvalue;
  char user_string[] = "test string";
  char *start_timestamp = NULL;
  char *end_timestamp = NULL;
  int i = 0;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *));

  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("8.1: Verify show-logs callid");

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  char category_str[] = "rw-logtest";
  uint32_t category = rwlogd_map_category_string_to_index(inst, category_str);
  obj->set_severity(category,RW_LOG_LOG_SEVERITY_DEBUG);

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category, category_p);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  log_input->filter = sink_filter;
  *category_pp = category_p;
  sink_filter->category = category_pp;

  //category_p->name = RW_LOG_LOG_CATEGORY_RW_LOGTEST;
  strcpy(category_p->name,"rw-logtest");
  sink_filter->n_category = 1;

  callidvalue.callid=100;
  callidvalue.groupid=200;
  callidvalue.call_arrived_time = 0;
  callid = &callidvalue;

  for(i=0;i<200;i++) {
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, callid, user_string);
  }

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input->has_count = TRUE;
  log_input->count = 200;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,200);
  log_output->n_logs = 0;

  log_input->has_count = TRUE;
  log_input->count = 50;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,50);
  log_output->n_logs = 0;

  EXPECT_EQ(log_output->n_log_summary,1);
  EXPECT_LOG_TRUE(log_output->log_summary[0]->trailing_timestamp);
  start_timestamp = log_output->log_summary[0]->trailing_timestamp;

  log_input->start_time = start_timestamp;
  log_input->count = 25;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,25);

  EXPECT_EQ(log_output->n_log_summary,1);
  EXPECT_LOG_TRUE(log_output->log_summary[0]->trailing_timestamp);
  log_output->n_logs = 0;

  end_timestamp = log_output->log_summary[0]->trailing_timestamp;

  log_input->count = 300;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,150);
  EXPECT_EQ(log_output->n_log_summary,1);
  EXPECT_LOG_TRUE(log_output->log_summary[0]->trailing_timestamp);
  log_output->n_logs = 0;

  log_input->count =75;
  log_input->end_time = end_timestamp;
  log_input->start_time = NULL;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,75);
  EXPECT_EQ(log_output->n_log_summary,1);

  log_output->n_logs = 0;

  log_input->count =25;
  log_input->start_time = start_timestamp;
  log_input->end_time = end_timestamp;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,25);

  EXPECT_EQ(log_output->n_log_summary,1);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup8, SprintfAttrCheck)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char user_string[] = "test string";
  char imsi[] = "1234567890.1345";
  char sink_name[] = "Category-filter";
  int test32 = 100;
  uint64_t test64 =  50000;

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *category_p = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **category_pp =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) **) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_Category) *));


  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category_config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter_Category, category_config_filter);

  category_config_filter->has_severity  = TRUE;
  category_config_filter->severity = RW_LOG_LOG_SEVERITY_DEBUG;
  strcpy(category_config_filter->name,"rw-logtest");


  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("8.2: Verify show-logs callid");

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_Category, category_p);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  log_input->filter = sink_filter;
  *category_pp = category_p;
  sink_filter->category = category_pp;

  strcpy(category_p->name,"rw-logtest");
  sink_filter->n_category = 1;

  status = rwlog_apply_all_categories_filter(inst, sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, "String constant passed as argument");
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, RWLOG_ATTR_SPRINTF("Sprintf args %s,test32: %d, test64:%lu", user_string, test32,test64));
  RWLOG_EVENT(ctxt, RwLogtest_notif_CallidImsiTest, RWLOG_ATTR_SPRINTF("Sprintf args %s,test32: %d, test64:%lu", user_string, test32,test64),imsi);
  RWLOG_EVENT(ctxt, RwLogtest_notif_CallidImsiTest, imsi, RWLOG_ATTR_SPRINTF("Sprintf args %s,test32: %d, test64:%lu", user_string, test32,test64));

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,5);

  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    printf("%s\n", log_output->logs[i]->msg);
  }

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(category_config_filter);
  status = rwlogd_delete_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup9, RwLogDRxLoopTest)
{
  rw_status_t status;
  TEST_DESCRIPTION("9.1: This test verifies rwlogd task start with file rotation");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  //rwlogd_create_sink(rwlogd_cli_sink, inst, "google-test1.3" , "RW.Cli", 1);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.3");
  filter_memory_header *shmctl = (filter_memory_header *)ctxt->filter_memory;

  //rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 1, NULL);

  char user_string[512];
  int j = 0;
  for (j = 0;j<512; j++)
  {
    user_string[j]= 48+(j%75);
  }
  user_string[j-1] = '\0';

  int start_ctrl = shmctl->rotation_serial;
  for(int i = 0; i< 10; i++)
  {
    for(j=0;j<20000;j++) {
      RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);
    }
  }

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  rwlogd_dump_task_info(inst->rwlogd_info,1,1);

  EXPECT_LOG_TRUE(start_ctrl!=shmctl->rotation_serial);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup9, FileSinkAdd)
{
  rw_status_t status;
  //int sockfd;
  char *env = getenv("YUMA_MODPATH");
  char *log_filename = NULL;
  char *filename = NULL;
  TEST_DESCRIPTION("9.2: This test verifies rwlogd task file sink add");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.6");

  asprintf (&filename,
            "FileSink-%d-%d-%lu",
            rwlog_get_systemId(),
            getpid(),syscall(SYS_gettid));

  status = start_rwlogd_file_sink(inst, "FileSink", filename,0);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  log_something(ctxt,10);
  log_something(ctxt,10);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
 
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("FileSink");
  EXPECT_EQ(sink->matched_log_count(),28);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  asprintf (&log_filename,"/tmp/%s",filename);
  int fd = open(log_filename,O_RDONLY);
  EXPECT_TRUE(fd>0);

  char buf[2048];
  char *ptr = &buf[0];
  char *end_buf = &buf[2048];
  int count = 0;
  bzero(buf,sizeof(buf));
  while(read(fd,ptr,2048) > 0) {
    if (verbose) {
      printf("%s",ptr);
    }
    while((ptr <= end_buf) && (ptr = (strchr(ptr,'\n')))) {
      count++;
      ptr +=1;
    }
    bzero(buf,sizeof(buf));
    ptr = &buf[0];
  }

  EXPECT_EQ(count,28);

  status = stop_rwlogd_file_sink(inst,"FileSink","Filesink-test1");
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  remove(log_filename);
  free(log_filename);
  free(filename);

  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}

static void *rwlogd_gtest_log(void *arg1);
TEST(RwLogdGroup9, RwLogDMultiThreadTest)
{
  rw_status_t status;
  int thread_result;
  pthread_t thread1, thread2, thread3 , thread4, thread5, thread6, thread7, thread8, thread9, thread10;
  TEST_DESCRIPTION("9.3 This test verifies rwlogd task flow control flag functionality");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_sysid_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_init("google-test1.3");
  filter_memory_header *shmctl = (filter_memory_header *)ctxt->filter_memory;
  int start_ctrl = shmctl->rotation_serial;

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);

  EXPECT_EQ(shmctl->rwlogd_flow,0);
  rwlogd_gtest_log( (void *) ctxt);
  rwlogd_gtest_log( (void *) ctxt);
  rwlogd_gtest_log( (void *) ctxt);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  //EXPECT_EQ(start_ctrl+1 , shmctl->rotation_serial);
  EXPECT_TRUE(shmctl->rotation_serial > start_ctrl);

  thread_result = pthread_create(&thread1, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread2, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread3, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread4, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  //EXPECT_EQ(shmctl->rwlogd_flow,1);

  thread_result = pthread_create(&thread5, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread6, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread7, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread8, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread9, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  thread_result = pthread_create(&thread10, NULL, rwlogd_gtest_log, (void *)ctxt);
  EXPECT_EQ(thread_result, 0);

  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
  pthread_join(thread3, NULL);
  pthread_join(thread4, NULL);
  pthread_join(thread5, NULL);
  pthread_join(thread6, NULL);
  pthread_join(thread7, NULL);
  pthread_join(thread8, NULL);
  pthread_join(thread9, NULL);
  pthread_join(thread10, NULL);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  //EXPECT_EQ(shmctl->rwlogd_flow,0);

  EXPECT_EQ(ctxt->stats.log_send_failed,0);
  EXPECT_TRUE(sink->matched_log_count() > 0);

  rwlogd_dump_task_info(inst->rwlogd_info,1,1);

#if 0
  printf("Dropped logs: %lu\n",ctxt->stats.dropped);
  printf("Sent logs: %lu\n",ctxt->stats.sent_events);
  printf("Sink matched log count is %lu\n",sink->matched_log_count());
#endif

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

static void *rwlogd_gtest_log(void *arg1)
{
  //rwlog_ctx_t *ctxt = rwlog_init("google-test1.3");
  rwlog_ctx_t *ctxt = (rwlog_ctx_t *) arg1 ;
  char user_string[128];
  int j = 0; 
  for (j = 0;j<128; j++)
  {
    user_string[j]= 48+(j%75);
  }
  user_string[j-1] = '\0';

  int count = 0;
  while(count < 10000) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTest, user_string);
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, user_string);
    count++;
  }
  return(NULL);
}


TEST(RwLogdGroup9, RwLogStaticShm)
{

  TEST_DESCRIPTION("9.4 This test verifies static shm at source when rwlogd not yet ready");

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.1");
  EXPECT_TRUE(0 == log_something(ctxt, 1));
  EXPECT_TRUE(ctxt->local_shm);
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_TRUE(0 == log_something(ctxt, 1));
  EXPECT_FALSE(ctxt->local_shm);
  EXPECT_EQ(rwlog_close_internal(ctxt,TRUE), RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup9, BootstrapSyslogSink)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  TEST_DESCRIPTION("10.1 This test verifies rwlogd task bootstrap syslog sink");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test");

  status = start_bootstrap_syslog_sink(inst);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(true, rwlogtest_event(ctxt,10));
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
 
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(BOOTSTRAP_SYSLOG_SINK);
  EXPECT_EQ(sink->matched_log_count(),1);
  status = stop_bootstrap_syslog_sink(inst);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup10, LogCheckPoint)
{
  rw_status_t status;
  TEST_DESCRIPTION("10.1 This test verifies Log Checkpoint");
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  int i = 0;
  uint32_t log_count = 0;
  uint32_t inactive_log_count = 0;
  #define LOG_SIZE 430

  inst->rwlogd_info->log_buffer_size = LOG_SIZE * 3;
  inst->rwlogd_info->curr_used_offset = inst->rwlogd_info->log_buffer_size;
  EXPECT_FALSE(inst->rwlogd_info->log_buffer == NULL);
  RW_FREE(inst->rwlogd_info->log_buffer);
  EXPECT_TRUE(inst->rwlogd_info->log_buffer_chkpt==NULL);

  inst->rwlogd_info->log_buffer = (char *)RW_MALLOC(inst->rwlogd_info->log_buffer_size); 
  EXPECT_FALSE(inst->rwlogd_info->log_buffer == NULL);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.19");

  for (i = 0; i < 5; i++) {
    EXPECT_LOG_TRUE(rwlogtest_event(ctxt, 1));
  }

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_LOG_TRUE(inst->rwlogd_info->stats.circ_buff_wrap_around > 0);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(RWLOGD_DEFAULT_SINK_NAME);
  ring_buffer_stats_t stats;
  sink->get_log_ring_stats(&stats);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  /* fetch logs - expect 3 as circular buffer can hold only 3 logs max */
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  log_count = stats.cur_size;

  EXPECT_LOG_TRUE(log_output->n_logs == 2 || log_output->n_logs == 3); 
  EXPECT_EQ(stats.cur_size,log_output->n_logs);
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  /* Fetch inactive logs - expect zero as we have not checkpointed the 3 logs */
  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  log_input->has_inactive = 1;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 0);
  protobuf_free(log_output);
  protobuf_free(log_input);

  /* Checkpoint the 3 logs Fetch inactive and expect to get 3 logs  */

  EXPECT_LOG_TRUE(RW_STATUS_SUCCESS == rwlogd_chkpt_logs(inst));
  inactive_log_count = log_count;
  log_count = 0;
  EXPECT_EQ(inst->rwlogd_info->stats.chkpt_stats,1);
  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  log_input->has_inactive = 1;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, inactive_log_count);
  protobuf_free(log_output);
  protobuf_free(log_input);
  
/* Fetch regular show logs  and expect to get the 3 inactive logs  */

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, inactive_log_count);
  protobuf_free(log_output);
  protobuf_free(log_input);

/* Do a second checkpoint - now there are no active logs so old checkpointed logs will be lost
   show logs inactive will return zero */
 
  EXPECT_LOG_TRUE(RW_STATUS_SUCCESS == rwlogd_chkpt_logs(inst));
  EXPECT_EQ(inst->rwlogd_info->stats.chkpt_stats,2);
  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  log_input->has_inactive = 1;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 0);
  inactive_log_count = 0;
  protobuf_free(log_output);
  protobuf_free(log_input);
 
 /* show logs expected to return 0 as both inactive and active logs are zero */

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs, 0);
  protobuf_free(log_output);
  protobuf_free(log_input);

/* Generate 5 logs and overwrite the current circular buffer
  show log should still show the 3 latest logs */
  
  for (i = 0; i < 5; i++) {
    EXPECT_LOG_TRUE(rwlogtest_event(ctxt, 2));
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_LOG_TRUE(log_output->n_logs == 2 || log_output->n_logs == 3);
  log_count = log_output->n_logs;
  for(uint32_t i=0;i<log_output->n_logs;i++) {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  /* check point the last 3 logs, generate another 5 and show log should show 6
     and show log inactive should show 3 */

  EXPECT_LOG_TRUE(RW_STATUS_SUCCESS == rwlogd_chkpt_logs(inst));
  inactive_log_count = log_count;
  log_count = 0;
  for (i = 0; i < 5; i++) {
    EXPECT_LOG_TRUE(rwlogtest_event(ctxt, 1));
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS); 
  EXPECT_LOG_TRUE(log_output->n_logs == 6 || log_output->n_logs == 5 || log_output->n_logs == 4);
  log_count = log_output->n_logs;
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  log_input->has_inactive = 1;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS); EXPECT_EQ(log_output->n_logs, inactive_log_count);
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  /* Generate another 5 logs , show-logs should give 6 logs and show logs inactive 
     should still say 3 logs
   */
  for (i = 0; i < 5; i++) {
    EXPECT_LOG_TRUE(rwlogtest_event(ctxt, 2));
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS); 
  EXPECT_LOG_TRUE(log_output->n_logs == 6 || log_output->n_logs == 5 || log_output->n_logs == 4);
  log_count = log_output->n_logs;
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->has_count = TRUE; log_input->count = 1000;
  log_input->has_inactive = 1;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS); 
  EXPECT_EQ(log_output->n_logs,inactive_log_count);
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    EXPECT_LOG_TRUE(rwlog_validate_log_header(log_output->logs[i]->msg));
  }
  protobuf_free(log_output);
  protobuf_free(log_input);

  rwlogd_gtest_free_instance(inst);
  status = rwlog_close_internal(ctxt,TRUE);
}

static void rwlogd_gtest_tmr_cb(stw_tmr_t *tmr, void *param)
{
    *((int *)param) +=1;
    return;
}

TEST(RwLogdGroup11, RwLogTmr)
{

  TEST_DESCRIPTION("11.1 Verify timer");
  rw_tmr_t tmr;
  int flag = 0;
  rw_status_t status;
  void (*timer_cb)(stw_tmr_t *tmr, void *parm) = rwlogd_gtest_tmr_cb;

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.1");

  status = rwtimer_start(inst->rwlogd_info->stw_system_handle, &tmr, 100,0,timer_cb,&flag);
  EXPECT_EQ(status, RW_STATUS_SUCCESS); 
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  EXPECT_TRUE(flag == 1);
  EXPECT_TRUE(RW_STATUS_SUCCESS == rwtimer_stop(inst->rwlogd_info->stw_system_handle,&tmr));

  flag = 0;
  status = rwtimer_start(inst->rwlogd_info->stw_system_handle, &tmr, 100,100,timer_cb,&flag);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.3, NULL);
  EXPECT_TRUE(flag >= 2);
  EXPECT_TRUE(RW_STATUS_SUCCESS == rwtimer_stop(inst->rwlogd_info->stw_system_handle,&tmr));

  EXPECT_EQ(rwlog_close_internal(ctxt,TRUE), RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup11, DISABLED_ConsistentHashTest)
{
 char *node;
 void *hash_ring =  rwlogd_create_consistent_hash_ring(RWLOGD_DEFAULT_NODE_REPLICA);

 rwlogd_add_node_to_consistent_hash_ring(hash_ring,(char *)"test-node1");
 rwlogd_add_node_to_consistent_hash_ring(hash_ring,(char *)"test-node2");
 rwlogd_add_node_to_consistent_hash_ring(hash_ring,(char *)"test-node3");

 node = rwlogd_get_node_from_consistent_hash_ring_for_key(hash_ring,123456789);
 EXPECT_TRUE(strcmp(node,"test-node2") == 0);

 node = rwlogd_get_node_from_consistent_hash_ring_for_key(hash_ring,123456788);
 EXPECT_TRUE(strcmp(node,"test-node3") == 0);

 node = rwlogd_get_node_from_consistent_hash_ring_for_key(hash_ring,223456788);
 EXPECT_TRUE(strcmp(node,"test-node1") == 0);

 node = rwlogd_get_node_from_consistent_hash_ring_for_key(hash_ring,323456788);
 EXPECT_TRUE(strcmp(node,"test-node2") == 0);

 rwlogd_display_consistent_hash_ring(hash_ring);

 rwlogd_remove_node_from_consistent_hash_ring(hash_ring,(char *)"test-node3");

 node = rwlogd_get_node_from_consistent_hash_ring_for_key(hash_ring,123456788);
 EXPECT_TRUE(strcmp(node,"test-node1") == 0);


 rwlogd_delete_consistent_hash_ring(hash_ring);
}


TEST(RwLogdGroup11, DISABLED_CallidSharding)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  rw_status_t rs;

  rw_call_id_t *callid, callidvalue;
  char user_string[] = "abcdef";
  memset(&callidvalue, 0 , sizeof(callidvalue));
  callidvalue.callid=112234;
  callidvalue.groupid=445566;
  callid = &callidvalue;
 
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *sink_filter = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1,false);
  rwlogd_instance_ptr_t inst2 = rwlogd_gtest_allocate_instance(2,false);
  rwlogd_instance_ptr_t inst3 = rwlogd_gtest_allocate_instance(3,false);

  rs = rwmsg_clichan_bind_rwsched_cfrunloop(inst->cc,inst->rwtasklet_info->rwsched_tasklet_info);
  EXPECT_TRUE(rs == RW_STATUS_SUCCESS);
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(inst2->cc,inst2->rwtasklet_info->rwsched_tasklet_info);
  EXPECT_TRUE(rs == RW_STATUS_SUCCESS);
  rs = rwmsg_clichan_bind_rwsched_cfrunloop(inst3->cc,inst3->rwtasklet_info->rwsched_tasklet_info);
  EXPECT_TRUE(rs == RW_STATUS_SUCCESS);


 rwlogd_add_node_to_list(inst,1);
 rwlogd_add_node_to_list(inst,2);
 rwlogd_add_node_to_list(inst,3);

 rwlogd_add_node_to_list(inst2,1);
 rwlogd_add_node_to_list(inst2,2);
 rwlogd_add_node_to_list(inst2,3);

 rwlogd_add_node_to_list(inst3,1);
 rwlogd_add_node_to_list(inst3,2);
 rwlogd_add_node_to_list(inst3,3);


  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("3.2: Verify show-logs callid");

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, sink_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);
  log_input->filter = sink_filter;

  log_input->has_count = TRUE;
  log_input->count = 500;

  sink_filter->has_callid=1;
  sink_filter->has_groupcallid=0;
  sink_filter->callid=112234;
 
  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter, config_filter);
  char sink_name[] = "Gtest-callid";

  config_filter->has_callid = TRUE;
  config_filter->callid = 112234;

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_apply_filter_callgroup(inst, sink_name, config_filter);
  char default_sink_name[] = RWLOGD_DEFAULT_SINK_NAME;
  status = rwlog_apply_filter_callgroup(inst, default_sink_name, config_filter);

  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  int i = 0;
  for(i=0;i<20;i++) {
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, user_string);
  }

  rwsched_instance_CFRunLoopRunInMode(inst->rwtasklet_info->rwsched_instance, RWMSG_RUNLOOP_MODE, 0.2, FALSE); 

  EXPECT_EQ((inst->rwlogd_info->stats.peer_send_requests + inst3->rwlogd_info->stats.peer_send_requests), inst2->rwlogd_info->stats.peer_recv_requests);
  EXPECT_EQ(inst->rwlogd_info->stats.log_forwarded_to_peer,20);
  EXPECT_EQ(inst3->rwlogd_info->stats.log_forwarded_to_peer,20);
  EXPECT_EQ(inst2->rwlogd_info->stats.logs_received_from_peer,40);

  EXPECT_TRUE(inst->rwlogd_info->stats.peer_send_requests < inst->rwlogd_info->stats.log_forwarded_to_peer);
  EXPECT_TRUE(inst3->rwlogd_info->stats.peer_send_requests < inst3->rwlogd_info->stats.log_forwarded_to_peer);

  for(i=0;i<20;i++) {
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_CallidTest, callid, user_string);
  }

  rwsched_instance_CFRunLoopRunInMode(inst->rwtasklet_info->rwsched_instance, RWMSG_RUNLOOP_MODE, 0.2, FALSE); 
  
#if 0
  rwsched_instance_CFRunLoopRunInMode(inst2->rwtasklet_info->rwsched_instance, RWMSG_RUNLOOP_MODE, 1, FALSE); 
  rwsched_instance_CFRunLoopRunInMode(inst3->rwtasklet_info->rwsched_instance, RWMSG_RUNLOOP_MODE, 1, FALSE); 
#endif

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  log_output->n_logs = 0;

  status = rwlog_mgmt_fetch_logs (inst2, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  /* Each RwLogD will read the log and so we can expect 3 logs; 2 from peer and
   * one read by this Rwlogd */
  EXPECT_EQ(log_output->n_logs,120);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  log_output->n_logs = 0;


  status = rwlog_mgmt_fetch_logs (inst3, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,0);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  protobuf_free(config_filter);

  EXPECT_EQ((inst->rwlogd_info->stats.peer_send_requests + inst3->rwlogd_info->stats.peer_send_requests), inst2->rwlogd_info->stats.peer_recv_requests);
  EXPECT_EQ(inst->rwlogd_info->stats.log_forwarded_to_peer,40);
  EXPECT_EQ(inst3->rwlogd_info->stats.log_forwarded_to_peer,40);
  EXPECT_EQ(inst2->rwlogd_info->stats.logs_received_from_peer,80);

  EXPECT_TRUE(inst->rwlogd_info->stats.peer_send_requests < inst->rwlogd_info->stats.log_forwarded_to_peer);
  EXPECT_TRUE(inst3->rwlogd_info->stats.peer_send_requests < inst3->rwlogd_info->stats.log_forwarded_to_peer);

  if(verbose) {
    rwlogd_dump_task_info(inst->rwlogd_info,1,1);
    rwlogd_dump_task_info(inst2->rwlogd_info,2,1);
    rwlogd_dump_task_info(inst3->rwlogd_info,3,1);
  }

  rwmsg_clichan_halt(inst->cc);
  rwmsg_clichan_halt(inst2->cc);
  rwmsg_clichan_halt(inst3->cc);

  rwlogd_gtest_free_instance(inst);
  rwlogd_gtest_free_instance(inst3);
  rwlogd_gtest_free_instance(inst2);
}

TEST(RwLogdGroup11, DupEvtSuppress)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.10");
  TEST_DESCRIPTION("2.4: Verify show-logs all");
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  char *filter_shm_name;
  int r;
  r = asprintf (&filter_shm_name,
                      "%s-%d-%d-%lu",
                      RWLOG_FILTER_SHM_PATH,
                      rwlog_get_systemId(),
                      getpid(),syscall(SYS_gettid));
  RW_ASSERT(r);
  rwlog_shm_set_dup_events(filter_shm_name, FALSE);
  free(filter_shm_name);

  char user_string[128];
  int j = 0;
  rw_call_id_t callidvalue, *callid;
  for (j = 0;j<128; j++)
  {
    user_string[j]= 48+(j%75);
  }
  user_string[j-1] = '\0';
  log_something(ctxt,10);
  callidvalue.callid=100;
  callidvalue.groupid=200;
  callidvalue.call_arrived_time = 0;
  callid = &callidvalue;

  for(int i=0;i<200;i++) {
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTest, callid, user_string);
    RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, callid, user_string);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,12);

  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }

  rwlog_ctxt_dump(ctxt);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup12, ProtocolFilter)
{
  rw_status_t status; 
  char *env = getenv("YUMA_MODPATH");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *show_filter =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter)));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_BinaryEvents) *binary_filter =
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_BinaryEvents) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs_Filter_BinaryEvents)));

  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.11");
  TEST_DESCRIPTION("2.5: Verify show-logs verbosity 3");
 
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter, show_filter);
  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs_Filter_BinaryEvents, binary_filter);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  log_input->filter = show_filter;
  show_filter->binary_events = binary_filter;

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  rwlogd_sink_update_protocol_filter(inst,(char *)"Gtest",RW_LOG_PROTOCOL_TYPE_IP, TRUE);

  for(int i=0;i<5;i++) {
    log_packet(ctxt,(char *)"inbound_gtp.pkt", 2,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_INBOUND); 
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "Protocol filter test");
    log_packet(ctxt,(char *)"outbound_gtp.pkt", 2,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_OUTBOUND);
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  log_input->has_verbosity = TRUE;
  log_input->verbosity = 3;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,20);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
    EXPECT_TRUE(strlen(log_output->logs[i]->msg) > 64);
    EXPECT_TRUE(log_output->logs[i]->pdu_detail);
    if (log_output->logs[i]->pdu_detail) {
      if (verbose) {
        printf("%s\n", log_output->logs[i]->pdu_detail);
      }
    }
  }
  log_output->n_logs  = 0;

  binary_filter->has_protocol = TRUE;
  binary_filter->protocol = RW_LOG_PROTOCOL_TYPE_IP;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,20);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    EXPECT_TRUE(log_output->logs[i]->pdu_detail);
  }
  log_output->n_logs  = 0;

  binary_filter->protocol = RW_LOG_PROTOCOL_TYPE_GTP;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,0);
  log_output->n_logs  = 0;

  for(int i=0;i<5;i++) {
    RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, "Protocol filter test");
  }
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  show_filter->binary_events = NULL;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,25);
  log_output->n_logs  = 0;

  show_filter->binary_events = binary_filter;
  binary_filter->protocol = RW_LOG_PROTOCOL_TYPE_IP;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,20);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    EXPECT_TRUE(log_output->logs[i]->pdu_detail);
  }
  log_output->n_logs  = 0;

  show_filter->binary_events = binary_filter;
  binary_filter->protocol = RW_LOG_PROTOCOL_TYPE_IP;
  binary_filter->has_packet_direction = TRUE;
  binary_filter->packet_direction = RW_LOG_DIRECTION_TYPE_INBOUND;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,10);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    EXPECT_TRUE(log_output->logs[i]->pdu_detail);
  }
  log_output->n_logs  = 0;

  show_filter->binary_events = binary_filter;
  binary_filter->protocol = RW_LOG_PROTOCOL_TYPE_IP;
  binary_filter->packet_direction = RW_LOG_DIRECTION_TYPE_OUTBOUND;
  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_EQ(log_output->n_logs,10);
  for (uint32_t i = 0; i < log_output->n_logs; i++)
  {
    EXPECT_TRUE(log_output->logs[i]->pdu_detail);
  }
  log_output->n_logs  = 0;


  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("Gtest");
  EXPECT_EQ(sink->matched_log_count(),25);

  rwlogd_sink_update_protocol_filter(inst, (char *)"Gtest",RW_LOG_PROTOCOL_TYPE_GTP,TRUE);
  rwlogd_sink_update_protocol_filter(inst, (char *)"Gtest",RW_LOG_PROTOCOL_TYPE_UDP,TRUE);
  EXPECT_TRUE(sink->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_GTP));
  EXPECT_TRUE(obj->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_GTP));


  rwlogd_sink_update_protocol_filter(inst, (char *)"Gtest",RW_LOG_PROTOCOL_TYPE_GTP,FALSE);
  EXPECT_FALSE(sink->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_GTP));
  EXPECT_FALSE(obj->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_GTP));
  EXPECT_TRUE(sink->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_UDP));
  EXPECT_TRUE(obj->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_UDP));
  EXPECT_TRUE(sink->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_IP));
  EXPECT_TRUE(obj->is_protocol_filter_set(RW_LOG_PROTOCOL_TYPE_IP));


  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}

int
rwlog_pcap_read( char * filename)
{

  if (!filename) { return -1;}
  pcap_t *handle; 
  struct pcap_pkthdr header;
  int count = 0;
  char errbuf[PCAP_ERRBUF_SIZE];

  handle = pcap_open_offline(filename, errbuf);
  if (!handle) { return -1;} 
  while (pcap_next(handle,&header)) { count++; }
  pcap_close(handle);
  return(count);
}


TEST(RwLogdGroup12, PcapSinkAdd)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char *filename = NULL;
  TEST_DESCRIPTION("12.x: This test verifies rwlogd task pcap sink add");
  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test12.x");

  asprintf (&filename,
            "/tmp/FileSink-%d-%lu",
            getpid(),syscall(SYS_gettid));

  status = start_rwlogd_pcap_sink(inst, "PcapSink", filename,0);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  char category[] = "rw-logtest";
  status = rwlogd_sink_severity(inst,(char *)"PcapSink",category,RW_LOG_LOG_SEVERITY_DEBUG,RW_LOG_ON_OFF_TYPE_ON);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);
  log_packet(ctxt,(char *)"packet.pkt", 2,RW_LOG_PROTOCOL_TYPE_IP,RW_LOG_DIRECTION_TYPE_OUTBOUND);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
 
  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink("PcapSink");
  EXPECT_EQ(sink->matched_log_count(), 2);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);
  free(filename);
  asprintf (&filename,
            "/tmp/FileSink-%d-%lu-%d.pcap",
            getpid(),syscall(SYS_gettid), rwlog_get_systemId());
  EXPECT_EQ(rwlog_pcap_read(filename), 2);
  status = stop_rwlogd_pcap_sink(inst,"PcapSink","Pcapsink-test1");
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  remove(filename);
  free(filename);

  status = rwlog_close_internal(ctxt,TRUE);
  rwlogd_gtest_free_instance(inst);
}

TEST(RwLogdGroup12, DefaultSevCheck)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test12.x");
  char category_str[] = "all";

  rwlog_apply_default_verbosity(inst, RW_LOG_LOG_SEVERITY_INFO,
                                category_str);

  rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);
  char category_str1[] = "rw-logtest";
  status = rwlogd_sink_severity(inst,(char *)"Gtest",category_str1,RW_LOG_LOG_SEVERITY_DEBUG,RW_LOG_ON_OFF_TYPE_ON);
  EXPECT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}

void rwlog_l1_filter_check_test(rwlog_ctx_t *ctxt,rw_call_id_t *callid)
{
  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, "Debug log");
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestDebug, callid, "Debug callid log");

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, "Info log");
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, callid, "Info callid log");


  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestWarning, "Warning log");
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestWarning, callid, "Warning calid log");
}

TEST(RwLogdGroup12, L1FilterCheckTest)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  static int serial_no = 0;
  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test12.x");

  ((rwlogd_sink_data *)inst->rwlogd_info->sink_data)->incr_log_serial_no(serial_no);
  serial_no += 20;

  ctxt->speculative_buffer_window = 5;
  char category_str[] = "rw-logtest";
  rw_call_id_t callidvalue;
  struct timeval tv;           
  gettimeofday(&tv, NULL);  /* Get time directly instead of looking for tv_sec in common_attrs */

  memset(&callidvalue,0,sizeof(callidvalue));
  callidvalue.callid=100;
  callidvalue.groupid=200;
  callidvalue.call_arrived_time = tv.tv_sec-15;

  rwlog_apply_default_verbosity(inst, RW_LOG_LOG_SEVERITY_INFO,
                                category_str);

  //rwlogd_create_sink(rwlogd_cli_sink, inst, "Gtest" , "RW.Cli", 1);

  rwlog_l1_filter_check_test(ctxt,&callidvalue);

  EXPECT_EQ(ctxt->stats.l1_filter_flag_updated,2);
  EXPECT_EQ(ctxt->stats.l1_filter_passed,6);
  EXPECT_EQ(ctxt->stats.sent_events,4);


  rwlog_l1_filter_check_test(ctxt,&callidvalue);

  EXPECT_EQ(ctxt->stats.l1_filter_flag_updated,2);
  EXPECT_EQ(ctxt->stats.l1_filter_passed,10);
  EXPECT_EQ(ctxt->stats.sent_events,8);


  rwlog_apply_default_verbosity(inst, RW_LOG_LOG_SEVERITY_WARNING,category_str);

  rwlog_l1_filter_check_test(ctxt,&callidvalue);

  EXPECT_EQ(ctxt->stats.l1_filter_flag_updated,6);
  EXPECT_EQ(ctxt->stats.l1_filter_passed,16);
  EXPECT_EQ(ctxt->stats.sent_events,10);

  rwlog_l1_filter_check_test(ctxt,&callidvalue);

  EXPECT_EQ(ctxt->stats.l1_filter_flag_updated,6);
  EXPECT_EQ(ctxt->stats.l1_filter_passed,18);
  EXPECT_EQ(ctxt->stats.sent_events,12);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup12, SeverityCritInfoFilterCheck)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  rw_call_id_t *callid, callidvalue;
  char user_string[] = "test string";
  char sink_name[] = "Gtest-sink";

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  memset(&callidvalue, 0 , sizeof(callidvalue));
  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("7.5: Verify critical-info severity filter");

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  callidvalue.callid=100;
  callidvalue.groupid=200;
  callid = &callidvalue;

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogInfoCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogInfoCritinfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogDebugCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogDebugCritinfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogErrorCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogErrorCritinfo, callid, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,6);
  log_output->n_logs = 0;

  EXPECT_EQ(sink->matched_log_count(),6);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestInfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_FastpathEventLogTestError, callid, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,8);
  log_output->n_logs = 0;

  EXPECT_EQ(sink->matched_log_count(),8);


  RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *category_config_filter =
      (RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_data_Logging_Sink_Filter_Category)));
  RWPB_F_MSG_INIT(RwlogMgmt_data_Logging_Sink_Filter_Category, category_config_filter);

  strcpy(category_config_filter->name,"rw-logtest");
  category_config_filter->has_severity =  1;
  category_config_filter->severity =  RW_LOG_LOG_SEVERITY_ERROR;
  category_config_filter->has_critical_info = 1;
  category_config_filter->critical_info =  RW_LOG_ON_OFF_TYPE_OFF;

  status = rwlog_apply_all_categories_filter(inst, sink_name, category_config_filter);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogInfoCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogInfoCritinfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogDebugCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogDebugCritinfo, callid, user_string);

  RWLOG_EVENT(ctxt, RwLogtest_notif_LogErrorCritinfo, user_string);
  RWLOG_EVENT_CALLID(ctxt, RwLogtest_notif_LogErrorCritinfo, callid, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,14);
  log_output->n_logs = 0;

  EXPECT_EQ(sink->matched_log_count(),10);

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}


TEST(RwLogdGroup13, VNFIDLogTest)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char sink_name[] = "Gtest-sink";
  char user_string[] = "test string";

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  uuid_t ctx_vnf_id;
  char ctx_vnf_id_str[36+1];
  uuid_generate(ctx_vnf_id);
  uuid_unparse(ctx_vnf_id,ctx_vnf_id_str);
  rwlog_ctx_t *ctxt = rwlog_gtest_source_init_with_vnf("google-test1.13",ctx_vnf_id);

  status = rwlogd_create_sink(rwlogd_cli_sink, inst, sink_name , "RW.Cli", 1);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  rwlogd_sink_data *obj = rwlogd_get_sink_obj (inst);
  rwlogd_sink *sink = obj->lookup_sink(sink_name);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);

  uuid_t log_vnf_id;
  char log_vnf_id_str[36+1];
  uuid_generate(log_vnf_id);
  uuid_unparse(log_vnf_id,log_vnf_id_str);
  RWLOG_EVENT(ctxt, RwLogtest_notif_LogWithVnf,log_vnf_id_str,user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  for (uint32_t i = 0; i < log_output->n_logs; i++) {
    if(i < 1) {
      EXPECT_LOG_TRUE(strstr(log_output->logs[i]->msg,(char *)ctx_vnf_id_str));
    }
    else {
      EXPECT_LOG_TRUE(strstr(log_output->logs[i]->msg,(char *)log_vnf_id_str));
    }
    if (verbose) {
      printf("%s\n", log_output->logs[i]->msg);
    }
  }
  EXPECT_EQ(log_output->n_logs,2);
  log_output->n_logs = 0;

  EXPECT_EQ(sink->matched_log_count(),2);

  log_input->vnf_id = ctx_vnf_id_str;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(log_output->n_logs,1);
  EXPECT_LOG_TRUE(strstr(log_output->logs[0]->msg,(char *)ctx_vnf_id_str));
  log_output->n_logs = 0;

  if (verbose) {
    printf("%s\n", log_output->logs[0]->msg);
  }

  log_input->vnf_id= log_vnf_id_str;

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(log_output->n_logs,1);
  EXPECT_LOG_TRUE(strstr(log_output->logs[0]->msg,(char *)log_vnf_id_str));
  log_output->n_logs = 0;

  if (verbose) {
    printf("%s\n", log_output->logs[0]->msg);
  }

  log_input->vnf_id = NULL;

  /* Change CLI sink to use  VNF-ID and check CLI sink only filters those logs */
  rwlogd_sink_update_vnf_id(inst, sink_name, ctx_vnf_id_str);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT(ctxt, RwLogtest_notif_LogWithVnf,log_vnf_id_str,user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),3);

  log_input->start_time = strdup(log_output->log_summary[0]->trailing_timestamp);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(log_output->n_logs,2);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }
  //log_output->n_logs = 0;

  /* Change to other VNF-ID and check CLI sink only filters those logs */
  rwlogd_sink_update_vnf_id(inst, sink_name, log_vnf_id_str);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT(ctxt, RwLogtest_notif_LogWithVnf,log_vnf_id_str,user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  EXPECT_EQ(sink->matched_log_count(),4);

  log_input->start_time = strdup(log_output->log_summary[0]->trailing_timestamp);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);

  EXPECT_EQ(log_output->n_logs,2);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }

  log_input->start_time = NULL;

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}



TEST(RwLogdGroup13, DynSchemaTest)
{
  rw_status_t status;
  char *env = getenv("YUMA_MODPATH");
  char user_string[] = "test string";

  if (!env)
  {
    printf("********* Not in riftshell ***********\n");
    return;
  }

  rwlogd_instance_ptr_t inst = rwlogd_gtest_allocate_instance(1, false);

  rwlog_ctx_t *ctxt = rwlog_gtest_source_init("google-test1.13");
  TEST_DESCRIPTION("Dynamic schema test");

  EXPECT_TRUE(inst->rwlogd_info->num_categories == 2);

  RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *log_input = 
      (RWPB_T_MSG(RwlogMgmt_input_ShowLogs) *) RW_MALLOC0(sizeof(RWPB_T_MSG(RwlogMgmt_input_ShowLogs)));
  RWPB_T_MSG(RwLog_output_ShowLogsInternal) *log_output =
      (RWPB_T_MSG(RwLog_output_ShowLogsInternal) *)RW_MALLOC0(sizeof(RWPB_T_MSG(RwLog_output_ShowLogsInternal)));

  RWPB_F_MSG_INIT(RwlogMgmt_input_ShowLogs, log_input);
  RWPB_F_MSG_INIT(RwLog_output_ShowLogsInternal, log_output);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT(ctxt, RwdynschemaLogtest_notif_DynschemaLogError, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,1);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }

  rwdynschema_module_t module = {(char *)"rwdynschema-logtest",
                                 nullptr,
                                 (char *)"libdynschema_log_test_yang_gen.so",
                                 nullptr,
                                 (char *)"rwdynschema-logtest_meta_info.txt",
                                 1};
  printf("load dynamic schema\n");
  rwlogd_handle_dynamic_schema_update(inst,
                                      1,
                                      &module);
  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 1.0, NULL);

  EXPECT_TRUE(inst->rwlogd_info->num_categories == 3);

  category_str_t *category_list = rwlogd_get_category_list(inst);
  EXPECT_TRUE(strcmp(category_list[2],"rwdynschema-logtest") == 0);

  RWLOG_EVENT(ctxt, RwLogtest_notif_FastpathEventLogTestError, user_string);
  RWLOG_EVENT(ctxt, RwdynschemaLogtest_notif_DynschemaLogError, user_string);

  rwsched_dispatch_main_until(inst->rwtasklet_info->rwsched_tasklet_info, 0.2, NULL);

  status = rwlog_mgmt_fetch_logs (inst, log_input, log_output);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  EXPECT_EQ(log_output->n_logs,3);
  if (verbose) {
    for (uint32_t i = 0; i < log_output->n_logs; i++) {
     printf("%s\n", log_output->logs[i]->msg);
    }
  }

  status = rwlog_close_internal(ctxt,TRUE);
  EXPECT_LOG_TRUE(status == RW_STATUS_SUCCESS);
  protobuf_free(log_output);
  protobuf_free(log_input);
  rwlogd_gtest_free_instance(inst);
}
