
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



/**
 * @file rwdts_gtest.cc
 * @author RIFT.io <info@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwdts
 *
 * @details Google test cases for testing rwmsg.
 */

/**
 * Step 1: Include the necessary header files
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#include <limits.h>
#include <cstdlib>
#include <sys/resource.h>
#include <ctime>
#include<iostream>

#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"

#include <unistd.h>
#include <sys/syscall.h>

#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwtasklet.h>

#include <rwmsg.h>
#include <rwmsg_int.h>
#include <rwmsg_broker.h>

#include <rwdts.h>
#include <rwdts_int.h>
#include <rwdts_router.h>
#include <dts-test.pb-c.h>
#include <testdts-rw-fpath.pb-c.h>
#include <testdts-rw-stats.pb-c.h>
#include <rwdts_redis.h>
#include <sys/prctl.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rwdts_appconf_api.h>
#include <rwdts_xpath.h>

#define TEST_MAX_RECOVER_AUDIT 8
#define REDIS_CLUSTER_PORT_INCR 10000

const rw_yang_pb_schema_t* rw_ypbc_gi__dts_test_get_schema(void);

/* Set env variable V=1 and TSTPRN("foo", ...") will be go to stderr */

using ::testing::HasSubstr;

typedef struct memberapi_test_s {
  rwdts_xact_t         *xact;
  rwdts_query_handle_t qhdl;
  rw_keyspec_path_t *key;
  uint32_t results_pending;
  bool send_nack;
  uint32_t inst;
  uint32_t results_sent;
  void *getnext_ptr;
} memberapi_test_t;

struct query_params {
 void *ctx;
 rwdts_api_t *clnt_apih;
 rw_keyspec_path_t *keyspec;
 uint32_t flags;
 uint64_t corrid;
 uint64_t corrid1;
};
static void rwdts_sub_app_set_exit_now(void *ctx);
//static void rwdts_clnt_getnext_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);
static void
rwdts_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);
static void
rwdts_clnt_query_callback_multiple(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);
static void
rwdts_ident_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud);
static void
memberapi_client_sub_app_start_f(void *ctx);

static void memberapi_dumpmember(void *ctx, rwdts_xact_t *xact);
static void memberapi_client_start_f(void *ctx);

static rwdts_member_rsp_code_t
memberapi_test_prepare(const rwdts_xact_info_t* xact_info,
                       RWDtsQueryAction         action,
                       const rw_keyspec_path_t* key,
                       const ProtobufCMessage*  msg,
                       uint32_t                 credits,
                       void*                    getnext_ptr);

static rwdts_member_rsp_code_t memberapi_test_prepare_new_version(const rwdts_xact_info_t* xact_info,
                                                                  RWDtsQueryAction         action,
                                                                  const rw_keyspec_path_t*      key,
                                                                  const ProtobufCMessage*  msg,
                                                                  void*                    ud);
static rwdts_member_rsp_code_t
memberapi_test_get_next(const rwdts_xact_info_t* xact_info,
                        RWDtsQueryAction         action,
                        const rw_keyspec_path_t* key,
                        const ProtobufCMessage*  msg,
                        void*                    user_data);

static rwdts_member_rsp_code_t
memberapi_test_advice_prepare(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg,
                              uint32_t credits,
                              void *getnext_ptr);

static rwdts_member_rsp_code_t
memberapi_receive_pub_notification(const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,uint32_t credits,
                                   void *getnext_ptr);

static rwdts_member_rsp_code_t
memberapi_receive_dereg_notification(const rwdts_xact_info_t* xact_info,
                                     RWDtsQueryAction         action,
                                     const rw_keyspec_path_t*      key,
                                     const ProtobufCMessage*  msg,
                                     uint32_t credits,
                                     void *getnext_ptr);

static void memberapi_pub_sub_client_start_f(void *ctx);

static void memberapi_pub_sub_cache_client_start_f(void *ctx);

static void memberapi_pub_sub_non_listy_client_start_f(void *ctx);

static void memberapi_pub_sub_cache_xact_member_start_f(void *ctx);

static void rwdts_set_exit_now(void *ctx);

static void rwdts_pub_set_exit_now(void *ctx);
static void set_exitnow(void *ud);

static int memberapi_get_objct(void *ctx);

static rwdts_member_rsp_code_t 
memberapi_test_ident_prepare(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg, 
                              uint32_t credits,
                              void *getnext_ptr);
static void
memberapi_ident_member_reg_ready(rwdts_member_reg_handle_t  regh,
                             rw_status_t                rs,
                             void*                      user_data);
static void
memberapi_test_data_member_cursor_query_cb(rwdts_xact_t* xact,
                                           rwdts_xact_status_t* xact_status,
                                           void*         ud);
static void
memberapi_test_reroot_list_query_cb(rwdts_xact_t* xact,
                                    rwdts_xact_status_t* xact_status,
                                    void*         ud);

static rwdts_member_rsp_code_t
memberapi_test_data_member_cursor_precommit_cb(const rwdts_xact_info_t*     xact_info,
                                              unsigned int                  size,
                                              const rwdts_commit_record_t** crec);
static void
memberapi_test_data_member_cursor_query_cb(rwdts_xact_t* xact,
                                           rwdts_xact_status_t* xact_status,
                                           void*         ud);

static void memberapi_test_data_member_cursor_client_start_f(void *ctx);
static void memberapi_reroot_list_client_start_f(void *ctx);
static void memberapi_client_async_start_f(void *ctx);

static void
member_reg_appdata_ready_queue_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx);

static void
member_reg_appdata_ready_safe_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx);

static void
member_reg_appdata_ready_unsafe_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx);

static void memberapi_sub_before_pub_client_start_f(void *ctx);

#define TSTPRN(args...) {                        \
  if (!venv_g.checked) {                        \
    const char *e = getenv("V");                \
    if (e && atoi(e) == 1) {                        \
      venv_g.val = 1;                                \
    }                                                \
    venv_g.checked = 1;                                \
  }                                                \
  if (venv_g.val) {                                \
    fprintf(stderr, args);                        \
  }                                                \
}
static struct {
  int checked;
  int val;
} venv_g;

using ::testing::MatchesRegex;
using ::testing::ContainsRegex;

TEST(RWDtsAPI, Init) {
  TEST_DESCRIPTION("Tests DTS API Init");

  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, ep, "/L/API/TEST/1", RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  EXPECT_EQ((void*)RWPB_G_SCHEMA_YPBCSD(DtsTest), (void*)rwdts_api_get_ypbc_schema(apih));

  /* Ruh-roh, api_init() now fires member registration, which fires off a dispatch_after_f, which we can't easily wait around for */
  rw_status_t rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  rwsched_instance_free(rwsched);
  rwsched = NULL;
}

TEST(RWDtsAPI, DynamicSchemas)
{

  TEST_DESCRIPTION("Tests DTS API add_schema");

  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, ep, "/L/API/TEST/2", RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
  ASSERT_NE(apih, (void*)NULL);

  auto dtstest = (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(DtsTest);
  rw_status_t s = rwdts_api_add_ypbc_schema (apih, dtstest);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  // DtsTest includes all test-schemas. Cannot test mulitple schema merges.
  // Ok.. atleast test whether rw-dts schema is added implicitly.
  auto unified = rwdts_api_get_ypbc_schema (apih);
  ASSERT_TRUE(unified);

  // Since dtstest contains dts schema it should not change
  ASSERT_EQ(unified, dtstest);

  // Do some xpath->keyspec conversion to verify the dynamic schema.
  // Some testcase better than this??
  rw_keyspec_path_t *ks = nullptr;

  char xpath [1024];
  strcpy (xpath, "/ps:person/ps:phone[ps:number='1234']");
  s = rwdts_api_keyspec_from_xpath(apih, xpath, &ks);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  UniquePtrKeySpecPath::uptr_t uptr(ks);

  ks = nullptr;
  strcpy (xpath, "/rwdts:dts/rwdts:member[rwdts:name='abc']/rwdts:state");
  s = rwdts_api_keyspec_from_xpath(apih, xpath, &ks);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  uptr.reset(ks);
  ks = nullptr;
  strcpy (xpath, "/tdtsbase:colony/tdtsfp:bundle-ether/tdtsfp:lacp");
  s = rwdts_api_keyspec_from_xpath(apih, xpath, &ks);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  /* Ruh-roh, api_init() now fires member registration, which fires off a dispatch_after_f, which we can't easily wait around for */
  rw_status_t rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;
  rw_keyspec_path_free(ks, NULL);
  rwsched_instance_free(rwsched);
  rwsched = NULL;
}

static rwdts_member_reg_handle_t
rwdts_memberapi_register(rwdts_xact_t *xact,
                         rwdts_api_t*          apih,
                         const rw_keyspec_path_t*               keyspec,
                         const rwdts_member_event_cb_t*    cb,
                         const ProtobufCMessageDescriptor* desc,
                         uint32_t                          flags,
                         const rwdts_shard_info_detail_t*  shard_detail)
{

    rw_keyspec_path_t *lks = NULL;
    rw_keyspec_path_create_dup(keyspec, NULL, &lks);
    rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);

    rwdts_member_reg_handle_t regh;

    /* Establish a registration */
    regh = rwdts_member_register(xact, apih,
                                 lks, cb, desc, flags, shard_detail);

    rw_keyspec_path_free(lks, NULL);

    return regh;
}

static rwdts_member_rsp_code_t myapp_xact_event(rwdts_api_t *apih,
                                                rwdts_group_t *grp,
                                                rwdts_xact_t *xact,
                                                rwdts_member_event_t xact_event,
                                                void *ctx,
                                                void *scratch_in) {
  return RWDTS_ACTION_OK;
}

TEST(RWDtsShardAPI, ShardMatchSimple) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *matched_shard;
  rwdts_shard_chunk_info_t *chunk;
  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard , RW_DTS_SHARD_FLAVOR_NULL ,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN);

  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  chunk = rwdts_shard_add_chunk(shard1, NULL);
  ASSERT_TRUE(chunk != NULL);

  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec, NULL, NULL, NULL);
  ASSERT_TRUE(shard1 == matched_shard);
 
  // deinint shard1
  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

}

TEST(RWDtsShardAPI, ShardMatchRange) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *matched_shard;
  rwdts_shard_chunk_info_t *chunk;
  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;
  union rwdts_shard_flavor_params_u params;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard , RW_DTS_SHARD_FLAVOR_RANGE ,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN);

  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);
  
  params.range.start_range = 100;
  params.range.end_range = 200;

  chunk = rwdts_shard_add_chunk(shard1, &params);
  ASSERT_TRUE(chunk != NULL);

  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec, &params, NULL, NULL);
  ASSERT_TRUE(shard1 == matched_shard);
  
  
  // deinint shard1
  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

}

TEST(RWDtsShardAPI, ShardInit) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *shard2, *matched_shard;
  rw_keyspec_path_t *keyspec;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t chunk_id;
  int depth;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard , RW_DTS_SHARD_FLAVOR_IDENT,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN);

  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard1
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);
  union rwdts_shard_flavor_params_u params1;
  size_t keylen;
  uint8_t *keyptr;
  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  memcpy(params1.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params1.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard1, &params1);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // add shard2 with different key value
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  shard2 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard , RW_DTS_SHARD_FLAVOR_IDENT,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL, RWDTS_SHARD_DEMUX_ROUND_ROBIN);

  ASSERT_TRUE(shard2 != NULL);

  // match shard2
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec, NULL, &chunk, &chunk_id);
  ASSERT_TRUE(shard2 == matched_shard);
  
  // add chunk for shard2
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);
  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  union rwdts_shard_flavor_params_u params2;
  memcpy(&params2.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params2.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard2, &params2);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // match shard1
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec, &params1, &chunk, &chunk_id);
  ASSERT_TRUE(shard1 == matched_shard);
  ASSERT_TRUE(chunk != NULL);
 
  // delete chunk from shard1
  status = rwdts_shard_delete_chunk(shard1, &params1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  
  // deinint shard1
  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // deinit shard2 + chunk in shard2
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  status = rwdts_shard_deref(shard2);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

TEST(RWDtsShardAPI, NullShardFlavor) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *shard2, *matched_shard;
  rw_keyspec_path_t *keyspec;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t chunk_id;
  int depth;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard1
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  union rwdts_shard_flavor_params_u params1;
  size_t keylen;
  uint8_t *keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  memcpy(&params1.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params1.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard1, &params1);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  params1.nullparams.chunk_id = chunk->chunk_id;
  // add shard2 with different key value
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  shard2 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard2 != NULL);

  
  // add chunk for shard2
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);

  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);
  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  union rwdts_shard_flavor_params_u params2;
  memcpy(&params2.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params2.identparams.keylen = keylen;

  chunk = rwdts_shard_add_chunk(shard2, &params2);
  ASSERT_TRUE(chunk != NULL);

  // match shard2
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(shard2 == matched_shard);

  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  // match shard1
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec,
                          &params1, &chunk, &chunk_id);
  ASSERT_TRUE(shard1 == matched_shard);
  ASSERT_TRUE(chunk != NULL);
 
  // delete chunk from shard1
  status = rwdts_shard_delete_chunk(shard1, &params1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  
  // deinint shard1
  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // deinit shard2 + chunk in shard2
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  status = rwdts_shard_deref(shard2);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

TEST(RWDtsShardAPI, IDENTShardFlavor) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *shard2, *matched_shard;
  rw_keyspec_path_t *keyspec;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t chunk_id;
  int depth;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_IDENT,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard1
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  union rwdts_shard_flavor_params_u params1;
  size_t keylen;
  uint8_t *keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  memcpy(&params1.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params1.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard1, &params1);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // add shard2 with different key value
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  shard2 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_IDENT,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard2 != NULL);

  
  // add chunk for shard2
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  union rwdts_shard_flavor_params_u params2;
  memcpy(&params2.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params2.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard2, &params2);
  ASSERT_TRUE(chunk != NULL);

  // match shard2
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(shard2 == matched_shard);

  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  // match shard1
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec,
                          &params1, &chunk, &chunk_id);
  ASSERT_TRUE(shard1 == matched_shard);
  ASSERT_TRUE(chunk != NULL);
 
  // delete chunk from shard1
  status = rwdts_shard_delete_chunk(shard1, &params1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  
  // deinint shard1
  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // deinit shard2 + chunk in shard2
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  status = rwdts_shard_deref(shard2);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

TEST(RWDtsShardAPI, NullShardMultipleInit) 
{

  TEST_DESCRIPTION("Tests DTS Shard APIs");
  rwdts_shard_t *rootshard = NULL, *shard1, *shard2, *shard1dup;
  rw_keyspec_path_t *keyspec;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_chunk_info_t *chunk;
  //rwdts_chunk_id_t chunk_id;
  int depth;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard1
  shard1 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard1
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  union rwdts_shard_flavor_params_u params1;
  size_t keylen;
  uint8_t *keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  memcpy(&params1.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params1.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard1, &params1);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  // add shard2 with different key value
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  shard2 = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard2 != NULL);

  
  // add chunk for shard2
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  union rwdts_shard_flavor_params_u params2;
  memcpy(&params2.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params2.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard2, &params2);
  ASSERT_TRUE(chunk != NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

 // add keyspec with key = 2 again 
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  shard1dup = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard1dup != NULL);
  ASSERT_TRUE(NULL != rootshard);
  ASSERT_TRUE(shard1dup == shard1);

  // add chunk for shard1
  depth = rw_keyspec_path_get_depth(keyspec);
  ASSERT_TRUE(depth);
  pe = rw_keyspec_path_get_path_entry(keyspec, depth-1);
  ASSERT_TRUE(pe!= NULL);

  keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr != NULL);
  union rwdts_shard_flavor_params_u params3;
  memcpy(&params3.identparams.keyin,keyptr, keylen);
  RW_FREE(keyptr);
  keyptr = NULL;
  params3.identparams.keylen = keylen;
  chunk = rwdts_shard_add_chunk(shard1, &params3);
  ASSERT_TRUE(chunk != NULL);

  status = rwdts_shard_deref(shard1);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwdts_shard_deref(shard1dup);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);

  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("3");
  status = rwdts_shard_deref(shard2);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}




TEST(RWDtsShardAPI, NullShardChunkIter) 
{

  TEST_DESCRIPTION("Tests DTS Shard Iter");
  rwdts_shard_t *rootshard = NULL, *shard;
  rw_keyspec_path_t *keyspec;
  rwdts_shard_chunk_info_t *chunk;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_status_t status;
  rwdts_chunk_rtr_info_t rtr_info, *rtr_info_out;
  rwdts_shard_chunk_iter_t *iter = NULL;
  int i, n_records;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  // add shard
  shard = rwdts_shard_init_keyspec(keyspec,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard
  chunk = rwdts_shard_add_chunk(shard, NULL);
  ASSERT_TRUE(chunk != NULL);
 // call create element api.

  // add 10000 elements which automatically adds required chunks to hold them.
  for (i = 0; i< 1 ; i++) {
    uint64_t chunk_id;
    uint32_t membid;
    char membername[64];
    sprintf(membername, "%u", i);
    status = rwdts_rts_shard_create_element(shard, chunk, &rtr_info, true, &chunk_id, &membid, membername);
    ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  }

  // Iterate through the 10000 elements
  i = 0;
  iter = rwdts_shard_chunk_iter_create_keyspec(rootshard, keyspec, NULL);
  ASSERT_TRUE(iter != NULL);
  while (rwdts_rtr_shard_chunk_iter_next(iter, &rtr_info_out, &n_records)) {
    i += n_records;
  } 
  i += n_records;
  ASSERT_EQ(i, 1);
  status = rwdts_shard_deref(shard);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
}


TEST(RWDtsShardAPI, Shardwildcard) 
{

  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rwdts_shard_t *shard2;
  rwdts_shard_t *shard3;
  rwdts_shard_t *rootshard = NULL;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_shard_t *matched_shard;
  uint64_t chunk_id;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry1  = 
                (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry2 = 
                (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry3 = 
                (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  // init keyspec1
  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec_entry1.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path000.key00.name, "mergecolony", sizeof(keyspec_entry1.dompath.path000.key00.name) - 1);
  keyspec_entry1.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path001.key00.name, "mergenccontext", sizeof(keyspec_entry1.dompath.path001.key00.name) - 1);
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  // init keyspec2 (wildcard)
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entry2;
  keyspec_entry2.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path000.key00.name, "mergecolony", sizeof(keyspec_entry2.dompath.path000.key00.name) - 1);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);

  // init keyspec3 (wildcard )
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entry3;
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);

  shard3 = rwdts_shard_init_keyspec(keyspec3,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard3 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard3
  chunk = rwdts_shard_add_chunk(shard3, NULL);
  ASSERT_TRUE(chunk != NULL);
  // match 
  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(NULL != matched_shard);
  
  // add and match a more specific keyspec 
  shard2 = rwdts_shard_init_keyspec(keyspec2,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard2 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  // add chunk for shard2
  chunk = rwdts_shard_add_chunk(shard2, NULL);
  ASSERT_TRUE(chunk != NULL);

  
  // match a more specific keyspec 
  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(shard2 == matched_shard);

  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec2,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(shard2 == matched_shard);
  rwdts_shard_deref(shard3);
  rwdts_shard_deref(shard2);

}

TEST(RWDtsShardAPI, ShardwildcardMatching) 
{
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;
  rwdts_shard_t *rootshard = NULL;
  rwdts_shard_t *shard3, *shard4, *shard5, *shard6;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_shard_t *matched_shard;
  uint64_t chunk_id;
#if 0
  rwdts_shard_t *shard2;
#endif

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 1;
  keyspec_entry1.dompath.path003.key00.key_four = 4;

  keyspec_entrywd1.dompath.path000.has_key00 = 1;
  keyspec_entrywd1.dompath.path000.key00.key_one = 1;
  keyspec_entrywd1.dompath.path001.has_key00 = 1;
  keyspec_entrywd1.dompath.path001.key00.key_two = 2;
  keyspec_entrywd1.dompath.path002.has_key00 = 1;
  keyspec_entrywd1.dompath.path002.key00.key_three = 3;

  keyspec_entrywd2.dompath.path000.has_key00 = 1;
  keyspec_entrywd2.dompath.path000.key00.key_one = 1;
  keyspec_entrywd2.dompath.path001.has_key00 = 1;
  keyspec_entrywd2.dompath.path001.key00.key_two = 2;

  keyspec_entrywd3.dompath.path000.has_key00 = 1;
  keyspec_entrywd3.dompath.path000.key00.key_one = 1;


  shard3 = rwdts_shard_init_keyspec(keyspec4,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard3 != NULL);
  ASSERT_TRUE(NULL != rootshard);

  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(NULL != matched_shard);


  shard4 = rwdts_shard_init_keyspec(keyspec3,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard4 != NULL);

  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(NULL != matched_shard);

  shard5 = rwdts_shard_init_keyspec(keyspec2,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard5 != NULL);

  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(NULL != matched_shard);

  shard6 = rwdts_shard_init_keyspec(keyspec1,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard6 != NULL);

  chunk = NULL;
  matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec1,
                          NULL, &chunk, &chunk_id);
  ASSERT_TRUE(NULL != matched_shard);

  rw_status_t status = rwdts_shard_deref(shard3);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(rootshard->ref_cnt == 1);

  status = rwdts_shard_deref(shard4);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(rootshard->ref_cnt == 1);

  status = rwdts_shard_deref(shard5);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(rootshard->ref_cnt == 1);

  status = rwdts_shard_deref(shard6);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

}


TEST(RWDtsShardAPI, ShardPerf_LONG) 
{
  int i;
#define LOOP1 1000
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;
  rwdts_shard_t *rootshard = NULL;
  rwdts_shard_t *shard3;
  rw_status_t status;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_shard_t *matched_shard;
  uint64_t chunk_id;

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);

  for(i = 0; i < LOOP1; i++) {
    keyspec_entry1.dompath.path000.has_key00 = 1;
    keyspec_entry1.dompath.path000.key00.key_one = i;
    keyspec_entry1.dompath.path001.has_key00 = 1;
    keyspec_entry1.dompath.path001.key00.key_two = i;
    keyspec_entry1.dompath.path002.has_key00 = 1;
    keyspec_entry1.dompath.path002.key00.key_three = i;
    keyspec_entry1.dompath.path003.has_key00 = 1;
    keyspec_entry1.dompath.path003.key00.key_four = i;

    shard3 = rwdts_shard_init_keyspec(keyspec1,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
    ASSERT_TRUE(shard3 != NULL);
    ASSERT_TRUE(NULL != rootshard);
  } 

  for (i = LOOP1; i < 2*LOOP1; i++) {
    keyspec_entrywd1.dompath.path000.has_key00 = 1;
    keyspec_entrywd1.dompath.path000.key00.key_one = i;
    keyspec_entrywd1.dompath.path001.has_key00 = 1;
    keyspec_entrywd1.dompath.path001.key00.key_two = i;
    keyspec_entrywd1.dompath.path002.has_key00 = 1;
    keyspec_entrywd1.dompath.path002.key00.key_three = i;

    shard3 = rwdts_shard_init_keyspec(keyspec2,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
    ASSERT_TRUE(shard3 != NULL);
    ASSERT_TRUE(NULL != rootshard);
  }

  
  for (i = 2*LOOP1; i < 3*LOOP1; i++) {
    keyspec_entrywd2.dompath.path000.has_key00 = 1;
    keyspec_entrywd2.dompath.path000.key00.key_one = i;
    keyspec_entrywd2.dompath.path001.has_key00 = 1;
    keyspec_entrywd2.dompath.path001.key00.key_two = i;
    shard3 = rwdts_shard_init_keyspec(keyspec3,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
    ASSERT_TRUE(shard3 != NULL);
    ASSERT_TRUE(NULL != rootshard);
  }

  for (i = 3*LOOP1; i < 4*LOOP1; i++) {
    keyspec_entrywd3.dompath.path000.has_key00 = 1;
    keyspec_entrywd3.dompath.path000.key00.key_one = 1;

    shard3 = rwdts_shard_init_keyspec(keyspec4,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
    ASSERT_TRUE(shard3 != NULL);
    ASSERT_TRUE(NULL != rootshard);
  }

  for (i=0; i< LOOP1; i++) {
    chunk = NULL;
    matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec2,
                          NULL, &chunk, &chunk_id);
    ASSERT_TRUE(NULL != matched_shard);

    chunk = NULL;
    matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec2,
                          NULL, &chunk, &chunk_id);
    ASSERT_TRUE(NULL != matched_shard);

    chunk = NULL;
    matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec3,
                          NULL, &chunk, &chunk_id);
    ASSERT_TRUE(NULL != matched_shard);

    chunk = NULL;
    matched_shard = rwdts_shard_match_keyspec(rootshard, keyspec4,
                          NULL, &chunk, &chunk_id);
    ASSERT_TRUE(NULL != matched_shard);

  }
  status = rwdts_shard_deinit_keyspec(keyspec3, &rootshard);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec4, &rootshard);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec1, &rootshard);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec2, &rootshard);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

}


TEST(RWDtsShardAPI, NullShardIter) 
{
  TEST_DESCRIPTION("Tests DTS Shard Iter");
  rwdts_shard_t *rootshard = NULL, *shard1, *shard2, *shard3, *shard4, *head;
  rwdts_shard_t **shards;
  uint32_t n_shards;
  size_t depth, i, total_shards;
  bool match;
  uint64_t chunk_id;
  uint32_t membid;
  const rw_keyspec_entry_t *pe;
  rw_status_t status;
  rwdts_chunk_rtr_info_t rtr_info; //, *rtr_info_out;
  rwdts_shard_chunk_info_t *chunk;
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;


  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 1;
  keyspec_entry1.dompath.path003.key00.key_four = 4;

  keyspec_entrywd1.dompath.path000.has_key00 = 1;
  keyspec_entrywd1.dompath.path000.key00.key_one = 1;
  keyspec_entrywd1.dompath.path001.has_key00 = 1;
  keyspec_entrywd1.dompath.path001.key00.key_two = 2;
  keyspec_entrywd1.dompath.path002.has_key00 = 1;
  keyspec_entrywd1.dompath.path002.key00.key_three = 3;

  keyspec_entrywd2.dompath.path000.has_key00 = 1;
  keyspec_entrywd2.dompath.path000.key00.key_one = 1;
  keyspec_entrywd2.dompath.path001.has_key00 = 1;
  keyspec_entrywd2.dompath.path001.key00.key_two = 2;

  keyspec_entrywd3.dompath.path000.has_key00 = 1;
  keyspec_entrywd3.dompath.path000.key00.key_one = 1;

  // add shard for keyspec1
  shard1 = rwdts_shard_init_keyspec(keyspec1,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard1 != NULL);
  ASSERT_TRUE(NULL != rootshard);
  // add chunk for shard
  TSTPRN("keyspec1:adding chunk to shard=%p\n", shard1);
  chunk = rwdts_shard_add_chunk(shard1, NULL);
  ASSERT_TRUE(chunk != NULL);

  for (i = 0; i< 30 ; i++) {
    char membername[64];
    sprintf(membername, "%zu", i);
    status = rwdts_rts_shard_create_element(shard1, chunk, &rtr_info, true, &chunk_id, &membid, membername);
    ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  }

  // add shard for keyspec2
  shard2 = rwdts_shard_init_keyspec(keyspec2,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard2 != NULL);
  ASSERT_TRUE(NULL != rootshard);
  // add chunk for shard
  TSTPRN("keyspec2:adding chunk to shard=%p\n", shard2);
  chunk = rwdts_shard_add_chunk(shard2, NULL);
  ASSERT_TRUE(chunk != NULL);

  for (i = 0; i< 30 ; i++) {
    char membername[64];
    sprintf(membername, "%zu", i);
    status = rwdts_rts_shard_create_element(shard2, chunk, &rtr_info, true, &chunk_id, &membid, membername);
    ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  }


  // add shard for keyspec3
  shard3 = rwdts_shard_init_keyspec(keyspec3,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard3 != NULL);
  ASSERT_TRUE(NULL != rootshard);
  // add chunk for shard
  TSTPRN("keyspec3:adding chunk to shard3=%p\n", shard3);
  chunk = rwdts_shard_add_chunk(shard3, NULL);
  ASSERT_TRUE(chunk != NULL);

  for (i = 0; i< 30 ; i++) {
    char membername[64];
    sprintf(membername, "%zu", i);
    status = rwdts_rts_shard_create_element(shard3, chunk, &rtr_info, true, &chunk_id, &membid, membername);
    ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  }


  // add shard for keyspec4
  shard4 = rwdts_shard_init_keyspec(keyspec4,-1, &rootshard ,
           RW_DTS_SHARD_FLAVOR_NULL,NULL, RWDTS_SHARD_KEYFUNC_NOP, NULL,
           RWDTS_SHARD_DEMUX_ROUND_ROBIN);
  ASSERT_TRUE(shard4 != NULL);
  ASSERT_TRUE(NULL != rootshard);
  // add chunk for shard
  TSTPRN("keyspec4:adding chunk to shard=%p\n", shard4);
  chunk = rwdts_shard_add_chunk(shard4, NULL);
  ASSERT_TRUE(chunk != NULL);

  for (i = 0; i< 30 ; i++) {
    char membername[64];
    sprintf(membername, "%zu", i);
    status = rwdts_rts_shard_create_element(shard4, chunk, &rtr_info, true, &chunk_id, &membid, membername);
    ASSERT_TRUE(status == RW_STATUS_SUCCESS);
  }


   head = rootshard;
   total_shards = 0;
   depth = rw_keyspec_path_get_depth(keyspec1);
   for (i=0; i<depth; i++) {
       shards = NULL;
       pe = rw_keyspec_path_get_path_entry(keyspec1, i);
       ASSERT_TRUE(pe != NULL);
       match = rwdts_shard_match_pathelem(&head,pe, &shards, &n_shards);
       total_shards+= n_shards;
       size_t k;
       for (k=0; k<n_shards; k++) {
         TSTPRN("matched shard = %p\n", shards[k]);
       }
       if (shards) {
         free(shards);
         shards = NULL;
       }
       if (match == false) {
         break;
       }
   }

  ASSERT_TRUE(total_shards == 4);

  status = rwdts_shard_deinit_keyspec(keyspec1, &rootshard);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec2, &rootshard);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec3, &rootshard);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);

  status = rwdts_shard_deinit_keyspec(keyspec4, &rootshard);
  ASSERT_TRUE(status == RW_STATUS_SUCCESS);
}


TEST(RWDtsAPI, GroupCreate) {
  TEST_DESCRIPTION("Tests DTS group create");
  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwtasklet_info_t rwtasklet_info = { 0 };
  rwtasklet_info.rwsched_tasklet_info = tasklet;
  rwtasklet_info.rwmsg_endpoint = ep;
  rwtasklet_info.rwsched_instance = rwsched;
  rwtasklet_info_ref(&rwtasklet_info);
  rwtasklet_info_ref(&rwtasklet_info);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_new (&rwtasklet_info, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest) , NULL, NULL, NULL);

  ASSERT_NE(apih, (void*)NULL);
  ASSERT_EQ(rwdts_get_router_conn_state(apih), RWDTS_RTR_STATE_DOWN);

  rwdts_group_t *grp = NULL;
  rw_status_t rs =
  //  rwdts_api_group_create(apih, NULL, NULL, myapp_xact_event, NULL, &grp);
      rwdts_api_group_create_gi(apih, NULL, NULL, myapp_xact_event, NULL,
                                &grp, NULL, NULL, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  ASSERT_TRUE(grp);
  ASSERT_EQ(apih->group_len, 3); // group[0] is DTS apih's own appconf group
  ASSERT_TRUE(apih->group);
  ASSERT_EQ(apih->group[2], grp);
  ASSERT_TRUE(apih->group[2]->cb.xact_event == myapp_xact_event);

  /* Make a second group */
  rwdts_group_t *grp2 = NULL;
  rs= rwdts_api_group_create(apih, NULL, NULL, NULL, NULL, &grp2);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  ASSERT_TRUE(grp2);
  ASSERT_EQ(grp2->id, 3);
  ASSERT_EQ(apih->group_len, 4);
  ASSERT_TRUE(apih->group);
  ASSERT_EQ(apih->group[3], grp2);
  ASSERT_TRUE(apih->group[3]->cb.xact_event == NULL);

  /* Add a registration */
  if (0) {
    rwdts_member_reg_handle_t reg;
    rwdts_group_register_keyspec(grp, NULL, NULL, NULL, NULL, NULL,NULL, 0, 0, &reg);
    reg=reg;
  }

  /* Destroy first group */
  rwdts_group_destroy(grp);

  rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  rwsched_instance_free(rwsched);
  rwsched = NULL;
}


TEST(RWDtsAPI, XactCreateAndDestroy) {
  TEST_DESCRIPTION("Tests DTS Xact create and destroy");
  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, ep, "/L/API/TEST/3", RWDTS_HARDCODED_ROUTER_ID, 0, NULL);

  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  
  EXPECT_EQ((void*)RWPB_G_SCHEMA_YPBCSD(DtsTest), (void*)rwdts_api_get_ypbc_schema(apih));

  rwdts_xact_t *xact = rwdts_api_xact_create(apih, RWDTS_FLAG_NONE, NULL, 0);
  ASSERT_TRUE(xact);

  xact = NULL;

  rw_status_t rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  rwsched_instance_free(rwsched);
  rwsched = NULL;
}

TEST(RWDtsAPI, RWDtsLogHndl) {
  TEST_DESCRIPTION("Tests DTS log handler");

  srandom((unsigned int)time(NULL));
  char fname[256];
  int fd;
  const char *msg = "RWDTS log handler test message";
  struct stat stats;
  uint64_t  param;

  sprintf(fname, "/tmp/%ld", random());
  fd = open(fname, O_CREAT|O_RDWR);
  ASSERT_TRUE(fd != -1);

  param= fd;
  rwdts_log_handler(0,G_LOG_LEVEL_MESSAGE, msg, (gpointer)param);
  close(fd);
  memset(&stats, 0, sizeof(stats));
  stat(fname,&stats);
  ASSERT_TRUE(stats.st_size > 0);
  unlink(fname);

rwdts_member_xact_state_t st[] = {
    RWDTS_MEMB_XACT_ST_INIT,
    RWDTS_MEMB_XACT_ST_PREPARE,
    RWDTS_MEMB_XACT_ST_PRECOMMIT,
    RWDTS_MEMB_XACT_ST_COMMIT,
    RWDTS_MEMB_XACT_ST_ABORT,
    RWDTS_MEMB_XACT_ST_END
  };
const char *str[] =
    { "INIT", "PREPARE", "PRECOMMIT",
      "COMMIT", "ABORT", "END"};
int i;
  for (i = 0; i<6; i++) {
    char *state_string = rwdts_get_xact_state_string(st[i]);
    EXPECT_STREQ(str[i],state_string);
  }

}

/* A slight abuse of "unit" testing, to permit DTS testing in a
   simplified multi-tasklet environment. */

typedef enum {
  TASKLET_ROUTER=0,
  TASKLET_ROUTER_3,
  TASKLET_ROUTER_4,
  TASKLET_BROKER,
  TASKLET_NONMEMBER,
  TASKLET_CLIENT,

#define TASKLET_IS_MEMBER_OLD(t) (((t) >= TASKLET_MEMBER_0) && ((t) <= TASKLET_MEMBER_2))
#define TASKLET_IS_MEMBER(t) (((t) >= TASKLET_MEMBER_0))

  TASKLET_MEMBER_0,
  TASKLET_MEMBER_1,
  TASKLET_MEMBER_2,
#define TASKLET_MEMBER_CT (3)
  TASKLET_MEMBER_3,
  TASKLET_MEMBER_4,
#define TASKLET_MEMBER_CT_NEW (5)

  TASKLET_CT
} twhich_t;

typedef struct cb_track_s {
  int reg_ready;
  int reg_ready_old;
  int prepare;
  int precommit;
  int commit;
  int abort;
  int xact_init;
  int xact_deinit;
  int validate;
  int apply;
} cb_track_t;

struct tenv1 {
  rwsched_instance_ptr_t rwsched;
  rwvx_instance_t *rwvx;
  int testno;

  struct tenv_info {
    twhich_t tasklet_idx;
    int member_idx;
    char rwmsgpath[1024];
    rwsched_tasklet_ptr_t tasklet;
    //    rwsched_dispatch_queue_t rwq;
    rwmsg_endpoint_t *ep;
    rwtasklet_info_t rwtasklet_info;
    rwdts_xact_info_t *saved_xact_info;

    // each tasklet might have one or more of:
    rwdts_router_t *dts;
    rwdts_api_t *apih;
    rwmsg_broker_t *bro;
    uint32_t  member_responses;
    struct {
      const rwdts_xact_info_t* xact_info;
      RWDtsQueryAction action;
      rw_keyspec_path_t *key;
      ProtobufCMessage *msg;
      rwdts_xact_block_t *block;

      int rspct;
    } async_rsp_state;

    void *ctx;                        // test-specific state ie queryapi_state

    uint32_t api_state_change_called;
    union {
      int prepare_cb_cnt;
      uint32_t miscval;
    };
    cb_track_t cb;

  } t[TASKLET_CT];
};

typedef struct appconf_scratch_s {
  rwdts_appconf_t *ac;
  const rwdts_xact_info_t *xact_info;
} appconf_scratch_t;

static void rwdts_test_api_state_changed(rwdts_api_t *apih, rwdts_state_t state, void *ud)
{
  TSTPRN("Invoking rwdts_test_api_state_changed() member %s state %u\n",
          apih->client_path,
          apih->dts_state);
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  //EXPECT_EQ(state, RW_DTS_STATE_INIT);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info*) ud;

  ti->api_state_change_called = 1;

  return;
}

static int memberapi_dumpmember_nc(struct tenv1::tenv_info *ti,
				   rwdts_xact_t *xact,
				   const char *ctxname);



class RWDtsEnsemble : public ::testing::Test {
protected:
  static void SetUpTestCase() {
    int i;
    /* Invoke below Log API to initialise shared memory */
    rwlog_init_bootstrap_filters(NULL);

    tenv.rwsched = rwsched_instance_new();
    ASSERT_TRUE(tenv.rwsched);

    tenv.rwvx = rwvx_instance_alloc();
    ASSERT_TRUE(tenv.rwsched);
    rw_status_t status = rwvcs_zk_zake_init(RWVX_GET_ZK_MODULE(tenv.rwvx));
    RW_ASSERT(RW_STATUS_SUCCESS == status);

    uint16_t redisport;
    const char *envport = getenv("RWDTS_REDIS_PORT");

    if (envport) {
      long int long_port;
      long_port = strtol(envport, NULL, 10);
      if (long_port < 65535 && long_port > 0)
        redisport = (uint16_t)long_port;
      else
        RW_ASSERT(long_port < 65535 && long_port > 0);
    } else {
      uint8_t uid;
      rw_status_t status;
      status = rw_unique_port(5342, &redisport);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rw_instance_uid(&uid);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      redisport += (105 * uid + getpid());
      if (redisport > 55530) {
        redisport -= 10000; // To manage max redis port range
      } else if (redisport < 1505) {
        redisport += 10000;
      }

      // Avoid the list of known port#s
      while (rw_port_in_avoid_list(redisport-1, 2) ||
             rw_port_in_avoid_list(redisport-1 + REDIS_CLUSTER_PORT_INCR, 1)) {
        redisport+=2;
      }
    }

    char tmp[16];
    sprintf(tmp, "%d", redisport-1);
    setenv ("RWMSG_BROKER_PORT", tmp, TRUE);

    //sprintf(tenv.redis_port, "%d", redisport);

    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      memset(ti, 0, sizeof(*ti));
      ti->tasklet_idx = (twhich_t)i;
      ti->member_idx = i - TASKLET_MEMBER_0;
      switch (i) {
      case TASKLET_ROUTER:
        sprintf(ti->rwmsgpath, "/R/RW.DTSRouter/%d", RWDTS_HARDCODED_ROUTER_ID);
        break;
      case TASKLET_BROKER:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/BROKER/1");
        break;
      case TASKLET_CLIENT:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/CLIENT/0");
        break;
      case TASKLET_NONMEMBER:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/NONMEMBER/0");
        break;
      case TASKLET_ROUTER_3:
        strcat(ti->rwmsgpath, "/R/RW.DTSRouter/3");
        break;
      case TASKLET_MEMBER_3:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/3");
        break;
      case TASKLET_ROUTER_4:
        strcat(ti->rwmsgpath, "/R/RW.DTSRouter/4");
        break;
      case TASKLET_MEMBER_4:
        strcat(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/4");
        break;
      default:
        sprintf(ti->rwmsgpath, "/L/RWDTS_GTEST/MEMBER/%d", i-TASKLET_MEMBER_0);
        break;
      }

      ti->tasklet = rwsched_tasklet_new(tenv.rwsched);
      ASSERT_TRUE(ti->tasklet);

      ti->ep = rwmsg_endpoint_create(1, 0, 0, tenv.rwsched, ti->tasklet, rwtrace_init(), NULL);
      ASSERT_TRUE(ti->ep);

      const int usebro = TRUE;

      if (usebro) {
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/enable", TRUE);

        // We'd like not to set this for router, but the broker's forwarding lookup doesn't support local tasklets
        rwmsg_endpoint_set_property_int(ti->ep, "/rwmsg/broker/shunt", TRUE);
      }

      ti->rwtasklet_info.rwsched_tasklet_info = ti->tasklet;
      ti->rwtasklet_info.ref_cnt = 1;

      uint64_t rout_id = RWDTS_HARDCODED_ROUTER_ID;
      switch (i) {
      case TASKLET_ROUTER:
      case TASKLET_ROUTER_3:
      case TASKLET_ROUTER_4:
        if (!ti->rwtasklet_info.rwlog_instance) {
         ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        ti->rwtasklet_info.rwmsg_endpoint = ti->ep;
        ti->rwtasklet_info.rwsched_instance = tenv.rwsched;
        ti->rwtasklet_info.rwvx = tenv.rwvx;
        ti->rwtasklet_info.rwvcs = tenv.rwvx->rwvcs;
        if (i== TASKLET_ROUTER_3) rout_id = 3;
        if (i== TASKLET_ROUTER_4) rout_id = 4;
        ti->dts = rwdts_router_init(ti->ep, rwsched_dispatch_get_main_queue(tenv.rwsched), 
                                    &ti->rwtasklet_info, ti->rwmsgpath, NULL, rout_id);

        ASSERT_NE(ti->dts, (void*)NULL);
        break;
      case TASKLET_BROKER:
        if (!ti->rwtasklet_info.rwlog_instance) {
         ti->rwtasklet_info.rwlog_instance =  rwlog_init("RW.DtsTests");
        }
        rwmsg_broker_main(0, 1, 0, tenv.rwsched, ti->tasklet, NULL, TRUE/*mainq*/, NULL, &ti->bro);
        ASSERT_NE(ti->bro, (void*)NULL);
        break;
      case TASKLET_NONMEMBER:
        break;
      default:
        sleep(1);
        RW_ASSERT(i >= TASKLET_ROUTER); // else router's rwmsgpath won't be filled in
        rwdts_state_change_cb_t state_chg = {.cb = rwdts_test_api_state_changed, .ud = ti};
        uint64_t id = RWDTS_HARDCODED_ROUTER_ID;
        if (i== TASKLET_MEMBER_3) id = 3;
        if (i== TASKLET_MEMBER_4) id = 4;
        ti->apih = rwdts_api_init_internal(NULL, NULL, ti->tasklet, tenv.rwsched, ti->ep, ti->rwmsgpath, id, 0, &state_chg);
        ASSERT_NE(ti->apih, (void*)NULL);
        rwdts_api_set_ypbc_schema(ti->apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
        break;
      }
    }
    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      switch (i) {
      case TASKLET_ROUTER:
        rwdts_router_test_register_single_peer_router(ti->dts, 3);
        rwdts_router_test_register_single_peer_router(ti->dts, 4);
        break;
      case TASKLET_ROUTER_3:
        rwdts_router_test_register_single_peer_router(ti->dts, 1);
        rwdts_router_test_register_single_peer_router(ti->dts, 4);
        break;
      case TASKLET_ROUTER_4:
        rwdts_router_test_register_single_peer_router(ti->dts, 3);
        rwdts_router_test_register_single_peer_router(ti->dts, 1);
        break;
      default:
        break;
      }
    }

    /* Run a short time to allow broker to accept connections etc */
    rwsched_dispatch_main_until(tenv.t[0].tasklet, 4/*s*/, NULL);
  }

  static void TearDownTestCase() {

    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];

      if (ti->dts) {
        int xct = HASH_COUNT(ti->dts->xacts);
        if (xct) {
          fprintf(stderr, "*** WARNING router still has %d xacts in flight\n", xct);
          rwdts_router_dump_xact_info(ti->dts->xacts, "All gtests are over");
        }
        EXPECT_EQ(xct, 0);   // should have no xacts floating around in the router
        rwdts_router_deinit(ti->dts);
        ti->dts = NULL;
      }

      if (ti->apih) {
        EXPECT_EQ(ti->api_state_change_called, 1);
        if (ti->apih->timer) {
          rwsched_dispatch_source_cancel(ti->apih->tasklet, ti->apih->timer);
          ti->apih->timer = NULL;
        }

        ASSERT_EQ(0, HASH_COUNT(ti->apih->xacts));

        rw_status_t rs = rwdts_api_deinit(ti->apih);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        ti->apih = NULL;
      }

      if (ti->bro) {
        int r = rwmsg_broker_halt_sync(ti->bro);
        ASSERT_TRUE(r);
      }
      ASSERT_TRUE(ti->ep);

      /* Note trace ctx; it gets traced into during endpoint shutdown, so we have to close it later */
      rwtrace_ctx_t *tctx = ti->ep->rwtctx;

      int r = rwmsg_endpoint_halt_flush(ti->ep, TRUE);
      ASSERT_TRUE(r);
      ti->ep = NULL;

      if (tctx) {
        rwtrace_ctx_close(tctx);
      }
      if (ti->rwtasklet_info.rwlog_instance) {
        rwlog_close(ti->rwtasklet_info.rwlog_instance,TRUE);
        ti->rwtasklet_info.rwlog_instance = NULL;
      }

      ASSERT_TRUE(ti->tasklet);
      rwsched_tasklet_free(ti->tasklet);
      ti->tasklet = NULL;
    }

    ASSERT_TRUE(tenv.rwsched);
    rwsched_instance_free(tenv.rwsched);
    tenv.rwsched = NULL;
  }
  static int rwdts_test_get_running_xact_count() {
    int cnt = 0;
    int i;
    for (i=0; i<TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      if (TASKLET_IS_MEMBER(ti->tasklet_idx) && ti->apih) {
        cnt += HASH_CNT(hh, ti->apih->xacts);
      }
    }
    return cnt;
  }

  void SetUp() {
    ASSERT_NE(tenv.t[TASKLET_CLIENT].apih, (void*)NULL);
    ASSERT_NE(tenv.t[TASKLET_MEMBER_0].apih, (void*)NULL);
    ASSERT_NE(tenv.t[TASKLET_MEMBER_1].apih, (void*)NULL);
    ASSERT_NE(tenv.t[TASKLET_MEMBER_2].apih, (void*)NULL);
    ASSERT_NE(tenv.t[TASKLET_MEMBER_3].apih, (void*)NULL);
    ASSERT_NE(tenv.t[TASKLET_MEMBER_4].apih, (void*)NULL);

    tenv.testno++;


    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];

      if (i==TASKLET_CT-1) {
        if (getenv("RWDTS_GTEST_SEGREGATE")) {
          fprintf(stderr, "SetUp(), tenv.testno=%d, RWDTS_GTEST_SEGREGATE=1, waiting for straggler events etc\n", tenv.testno);
          rwsched_dispatch_main_until(ti->tasklet, 70, NULL);
        } else {
	  TSTPRN("SetUp(), tenv.testno=%d\n", tenv.testno);
        }
      }

      TSTPRN( "  + ti=%p\n", ti);
      if (ti->apih) {
        TSTPRN("  + ti->apih=%p api_state_change_called=%d\n", ti->apih, ti->api_state_change_called);
        /* Blocking scheduler invokation until this tasklet's DTS API is ready */
        int maxtries=500;
        while (!ti->api_state_change_called && maxtries--) {
          rwsched_dispatch_main_until(ti->tasklet, 30, &ti->api_state_change_called);
          ASSERT_EQ(ti->api_state_change_called, 1);
        }
      }
    }
  }

  void TearDown() {

    /* See if there are any xacts still floating around.  Give them 2s to clear out, then go boom. */
    int xct = 0;
    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      RW_ASSERT(ti);
      if (ti->apih) {
        xct += HASH_COUNT(ti->apih->xacts);
      }
      if (ti->dts) {
        int xct = HASH_COUNT(ti->dts->xacts);
        if (xct) {
          double seconds = (RUNNING_ON_VALGRIND)?30:2;
          fprintf(stderr, "Router has  %d xacts outstanding, waiting %Gs to see if they go away\n", xct, seconds);
          rwsched_dispatch_main_until(tenv.t[TASKLET_CLIENT].tasklet, seconds, NULL);
        }
        xct = HASH_COUNT(ti->dts->xacts);
        if (xct) {
          fprintf(stderr, "*** WARNING router still has %d xacts in flight\n", xct);
          rwdts_router_dump_xact_info(ti->dts->xacts, "Gtest is over");
        }
        EXPECT_EQ(xct, 0);   // should have no xacts floating around in the router
      }
    }
    if (xct) {
      double seconds = (RUNNING_ON_VALGRIND)?30:2;
      fprintf(stderr, "Test left %d xacts outstanding, waiting %Gs to see if they go away\n", xct, seconds);
      rwsched_dispatch_main_until(tenv.t[TASKLET_CLIENT].tasklet, seconds, NULL);
    }

    for (int i=TASKLET_CT-1; i>=0; i--) {
      struct tenv1::tenv_info *ti = &tenv.t[i];
      RW_ASSERT(ti);
      if (ti->apih) {
        xct = HASH_COUNT(ti->apih->xacts);
        if (xct > 0) {
          char tmp_log_xact_id_str[128] = "";
          rwdts_xact_t *xact_entry = NULL, *tmp_xact_entry = NULL;
          HASH_ITER(hh, ti->apih->xacts, xact_entry, tmp_xact_entry) {
            fprintf(stderr,
                    "*** LEAKED XACT apih=%p client=%s xact=%p xact[%s] ref_cnt=%d, _query_count=%d\n",
                    ti->apih,
                    ti->apih->client_path,
                    xact_entry,
                    (char*)rwdts_xact_id_str(&(ti->apih->xacts)->id, tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)),
                    xact_entry->ref_cnt,
                    HASH_COUNT(xact_entry->queries));

            rwdts_xact_query_t *query=NULL, *qtmp=NULL;
            HASH_ITER(hh, xact_entry->queries, query, qtmp) {
              ASSERT_NE(query->evtrsp, RWDTS_EVTRSP_ASYNC);
            }
          }
        }
        ASSERT_EQ(0, xct);

#if 0
        if (ti->apih->timer) {
          rwsched_dispatch_source_cancel(ti->apih->tasklet, ti->apih->timer);
          ti->apih->timer = NULL;
        }
        rw_status_t rs = rwdts_api_deinit(ti->apih);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        ti->apih = NULL;
#endif

      }
      if (ti->dts) {
        EXPECT_EQ(HASH_COUNT(ti->dts->xacts), 0);   // should have no xacts floating around in the router
      }


      rwdts_api_t *apih = ti->apih;
      if (apih && RW_SKLIST_LENGTH(&(apih->reg_list))) {
        rwdts_api_reset_stats(apih);
        rwdts_member_registration_t *entry = NULL, *next_entry = NULL;

        entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
        while (entry) {
          next_entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
          if (((rwdts_member_registration_t *)apih->init_regidh != entry) && ((rwdts_member_registration_t *)apih->init_regkeyh != entry)) {
            if(entry->stats.out_of_order_queries)
            {
              //EXPECT_EQ(entry->stats.out_of_order_queries, 2);
            }
            if (!entry->dts_internal) {
              rw_status_t rs = rwdts_member_registration_deinit(entry);
              RW_ASSERT(rs == RW_STATUS_SUCCESS);
            }
          }
          entry = next_entry;
        }
      }

      rwdts_router_t *dts = ti->dts;
      if (dts && HASH_CNT(hh, dts->members)) {
        rwdts_shard_del(&dts->rootshard);
        RW_ASSERT(dts->rootshard->children);
        rwdts_router_member_t *memb=NULL, *membnext=NULL;
        HASH_ITER(hh, dts->members, memb, membnext) {
          if (strstr(memb->msgpath, "DTSRouter")) {
            continue;
          }
          rwdts_router_remove_all_regs_for_gtest(dts, memb);
        }
      }
    }
    usleep(100);
  }

public:
  static struct tenv1 tenv;


  static void rwdts_test_registration_cb(RWDtsRegisterRsp*    rsp,
                                         rwmsg_request_t*     rwreq,
                                          void*                ud);

  static void
  rwdts_shard_info_cb(RWDtsGetDbShardInfoRsp*    rsp,
                      rwmsg_request_t*     rwreq,
                      void*                ud);

};

struct tenv1 RWDtsEnsemble::tenv;
enum treatment_e {
  TREATMENT_ACK=100,
  TREATMENT_NACK,
  TREATMENT_NA,
  TREATMENT_ERR,
  TREATMENT_ASYNC,
  TREATMENT_ASYNC_NACK,
  TREATMENT_BNC,
  TREATMENT_PRECOMMIT_NACK,
  TREATMENT_PRECOMMIT_NULL,
  TREATMENT_COMMIT_NACK,
  TREATMENT_ABORT_NACK,
  TREATMENT_PREPARE_DELAY
};

enum expect_result_e {
  XACT_RESULT_COMMIT=0,
  XACT_RESULT_ABORT,
  XACT_RESULT_ERROR
};

enum reroot_test_e {
   REROOT_FIRST=0,
   REROOT_SECOND,
   REROOT_THIRD
};

typedef struct {
  struct queryapi_state *s;
  struct tenv1::tenv_info *ti;
  struct qapi_member *member;
  struct {
    rwdts_appconf_t *ac;
    rwdts_member_reg_handle_t reg_nmap;
    rwdts_member_reg_handle_t reg_nmap_test;
    rwdts_member_reg_handle_t reg_nmap_test_na;
  } conf1;
  int conf_apply_count;
} myappconf_t;

struct qapi_member {
  rwsched_dispatch_queue_t rwq;
  enum treatment_e treatment;
  myappconf_t appconf;
  myappconf_t sec_appconf;
  bool call_getnext_api;
  uint32_t    cb_count;
};

typedef enum rwdts_audit_test_type_e {
  RW_DTS_AUDIT_TEST_RECONCILE_SUCCESS = 1,
  RW_DTS_AUDIT_TEST_REPORT = 2,
  RW_DTS_AUDIT_TEST_RECOVERY = 3,
  RW_DTS_AUDIT_TEST_FAIL_CONVERGE = 4
} rwdts_audit_test_type_t;


typedef struct memberdata_api_corr_s {
  int correlate_sent;
  struct queryapi_state *s;
} memberdata_api_corr_t;

typedef enum del_tc_num {
  DEL_WC_1 = 1,
  DEL_WC_2 = 2
} del_tc_num_t;

typedef struct shard_safe_data_s {
  rwdts_shard_t *shard;
  uint32_t safe_id;
} shard_safe_data_t;

typedef enum appdata_type {
  APPDATA_KS = 1,
  APPDATA_PE = 2,
  APPDATA_MK = 3
} appdata_type_t;

typedef enum appdata_test_type {
  APPDATA_SAFE = 1,
  APPDATA_UNSAFE = 2,
  APPDATA_QUEUE = 3
} appdata_test_type_t;

struct queryapi_state {
#define QAPI_STATE_MAGIC (0x0a0e0402)
  uint32_t magic;
  int testno;
  uint32_t exitnow;
  uint32_t exit_soon;                // flag, should go on to set exitnow in xact_deinit
  uint32_t exit_without_checks;
  uint32_t member_startct;
  uint32_t install;
  char test_name[128];
  char test_case[128];
  struct tenv1 *tenv;
  int include_nonmember;
  int ignore;
  int keep_alive;
  int after;
  int exit_on_member_xact_finished;
  int append_done;
  int stream_results;
  int use_reg;
  int committed;
  int abort;
  int delete_test;
  int total_queries;
  uint32_t reg_ready_called;
  struct {
    /* A bunch of this is persistent client state across all ensemble
       tests and should move inside the API and/or into
       tenv->t[TASKLET_CLIENT].mumble */
    rwsched_dispatch_queue_t rwq;
    rwmsg_clichan_t *cc;
    rwmsg_destination_t *dest;
    RWDtsQueryRouter_Client querycli;
    rwmsg_request_t *rwreq;
    int rspct;
    bool noreqs;
    int transactional;
    int wildcard;
    int getnext;
    int client_abort;
    RWDtsQueryAction action;
    int usebuilderapi;
    int usexpathapi;
    bool userednapi;
    bool userednblk;
    int testperf_numqueries;
    int testperf_itercount;
    void *perfdata;
    uint32_t tv_usec;
    uint32_t tv_sec;
    enum expect_result_e expect_result;
    enum reroot_test_e reroot_test;
    char *xpath; // Send different XPath queries
    bool no_xml; // XML result not available
    bool expect_value; // Expect a value from query result
    ProtobufCType val_type;
  } client;
  struct qapi_member member[TASKLET_MEMBER_CT_NEW];
  uint32_t expected_notification_count;
  uint32_t notification_count;
  rwdts_audit_action_t audit_action;
  rwdts_audit_test_type_t audit_test;
  uint32_t expected_advise_rsp_count;
  int num_responses;
  uint32_t subscribed_data_attempt;
  uint32_t pub_data_check_success;
  uint32_t expected_pub_data_check_success;
  bool client_dreg_done;
  bool multi_router;
  int  memberapi_dummy_cb_correlation_rcvd;
  rwdts_member_reg_handle_t regh[TASKLET_CT];
  uint32_t  query_cb_rcvd;
  uint32_t  prepare_cb_called;
  uint32_t  query_cb_called;
  uint32_t  prepare_list_cb_called;
  uint32_t  publisher_ready:8;
  uint32_t  subs_ready:8;
  uint32_t  subs_created:8;
  uint32_t  pubs_created:8;
  rwdts_member_reg_handle_t client_regh;
  rwdts_member_reg_handle_t ip_nat_regh[TASKLET_CT];
  rwdts_member_reg_handle_t nc_regh[TASKLET_CT];
  rwdts_shard_handle_t* shard[TASKLET_CT];
  int recover_audit_attempts;
  del_tc_num_t del_tc;
  appdata_type_t appdata;
  appdata_test_type_t appdata_test;
  ProtobufCMessage *msg[TASKLET_CT];
  ProtobufCMessage *getnext_msg[5];
  rw_keyspec_path_t *getnext_ks[5];
  rw_keyspec_entry_t *getnext_pe[5];
  char* getnext_mk[5];
  uint32_t position;
  int32 regct;
  int Dlevel;
  int Dleftover;
  bool started;
  bool MultipleAdvise;
  uint32_t expected_notif_rsp_count;
  bool sec_reg_done; 
};

static void init_query_api_state(struct queryapi_state* s,
                                 RWDtsQueryAction       action,
                                 bool                   transactional,
                                 bool                   usexapthapi)
{
  int i;
  s->magic = QAPI_STATE_MAGIC;
  s->tenv = &RWDtsEnsemble::tenv;
  s->testno = s->tenv->testno;
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);

  const ::testing::TestInfo* const test_info =
  ::testing::UnitTest::GetInstance()->current_test_info();
  strncpy(s->test_name,  test_info->name(), sizeof(s->test_name) - 1);
  strncpy(s->test_case,  test_info->test_case_name(), sizeof(s->test_case) - 1);

  s->client.transactional = transactional;
  s->client.usexpathapi   = usexapthapi;
  s->client.action        = action;
  s->member[0].treatment  = TREATMENT_ACK;
  s->member[1].treatment  = TREATMENT_ACK;
  s->member[2].treatment  = TREATMENT_ACK;
  s->member[3].treatment  = TREATMENT_ACK;
  s->member[4].treatment  = TREATMENT_ACK;
  s->client.expect_result = XACT_RESULT_COMMIT;
  s->prepare_cb_called = 0;
  s->query_cb_called = 0;
  s->append_done = 0;
  s->use_reg = 0;
  s->after = 0;
  s->reg_ready_called = 0;
  s->expected_notif_rsp_count = 0;

  for (i=0; i < TASKLET_CT; i++) {
    s->regh[i] = NULL;
  }
}



TEST_F(RWDtsEnsemble, CheckSetup) {
  ASSERT_TRUE(tenv.rwsched);

  /* This is the tasklet's DTS Router instance, only in router tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].dts);
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER_3].dts);
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER_4].dts);
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].dts);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_3].dts);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_4].dts);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].dts);

  /* This is the tasklet's RWMsg Broker instance, only in broker tasklet */
  ASSERT_TRUE(tenv.t[TASKLET_BROKER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER_3].bro);
  ASSERT_FALSE(tenv.t[TASKLET_ROUTER_4].bro);
  ASSERT_FALSE(tenv.t[TASKLET_CLIENT].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_0].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_1].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_2].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_3].bro);
  ASSERT_FALSE(tenv.t[TASKLET_MEMBER_4].bro);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].bro);

  /* This is the tasklet's RWDts API Handle, in client, member, and router */
  ASSERT_FALSE(tenv.t[TASKLET_BROKER].apih);
  ASSERT_TRUE(tenv.t[TASKLET_CLIENT].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_0].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_1].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_2].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_3].apih);
  ASSERT_TRUE(tenv.t[TASKLET_MEMBER_4].apih);
  ASSERT_FALSE(tenv.t[TASKLET_NONMEMBER].apih);

  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_0].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_1].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_2].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_3].apih));
  ASSERT_TRUE(rwdts_cache_reg_ok(tenv.t[TASKLET_MEMBER_4].apih));
}

// TEST_F(RWDtsEnsemble, Foo) { }
// ... runs in a fixed environment with a client, router, broker, and some members, all tasklets etc
// ... dts apis are instantiated in those which needed it
// ... to setup: rwsched_dispatch_async_f to trigger a callback in a tasklet, typically on main queue (== CFRunLoop thread)
// ... to run: "rwsched_dispatch_main_until(tenv.t[0], 60/*s*/, &exitnow);"
// ... to end: set exitnow from app / test logic
// ... WARNING: there is no fixed ordering between tests

struct demo_state {
  uint32_t exitnow;
};

static void demo_member_setup_f(void *ctx) {
  struct demo_state *d = (struct demo_state *)ctx;
  TSTPRN("Starting ensemble demo test...\n");
  d=d;
}

static void demo_member_stop_f(void *ctx) {
  struct demo_state *d = (struct demo_state *)ctx;
  TSTPRN("Stopping ensemble demo test...\n");
  d->exitnow = 1;
}

TEST_F(RWDtsEnsemble, Demo) {
  struct demo_state d = { 0 };
  rwsched_dispatch_async_f(tenv.t[TASKLET_MEMBER_0].tasklet, rwsched_dispatch_get_main_queue(tenv.rwsched), &d, demo_member_setup_f);
  rwsched_dispatch_async_f(tenv.t[TASKLET_MEMBER_0].tasklet, rwsched_dispatch_get_main_queue(tenv.rwsched), &d, demo_member_stop_f);
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &d.exitnow);
  ASSERT_TRUE(d.exitnow);
}

struct plumbing1_state {
  int testno;
  uint32_t exitnow;
  uint32_t gotrsp;
  struct tenv1 *tenv;
  struct {
    /* A bunch of this is persistent client state across all ensemble
       tests and should move inside the API and/or into
       tenv->t[TASKLET_CLIENT].mumble */
    rwsched_dispatch_queue_t rwq;
    rwmsg_clichan_t *cc;
    rwmsg_destination_t *dest;
//    RWDtsQueryRouter_Client querycli;
    rwmsg_request_t *rwreq;
  } client;
};

static void plumbing1_client_regcb(RWDtsRegisterRsp *rsp,
                                   rwmsg_request_t *rwreq,
                                   void *ud) {
  struct plumbing1_state *s = (struct plumbing1_state *)ud;
  RW_ASSERT(s->testno == s->tenv->testno);

  if (rsp) {
    TSTPRN("Plumbing1 got regist rsp\n");
  } else {
    ASSERT_TRUE(rwreq);
    TSTPRN("Plumbing1 didn't get regist rsp, bnc code=%d\n",
           rwmsg_request_get_response_bounce(rwreq));
  }
  
  s->exitnow = TRUE;

  ASSERT_TRUE(rsp);
}

static rw_status_t
rwdts_member_deregister_all(rwdts_api_t* apih)
{

  rwdts_member_registration_t *entry = NULL, *next_entry = NULL;
  entry = RW_SKLIST_HEAD(&(apih->reg_list), rwdts_member_registration_t);
  while (entry) {
    next_entry = RW_SKLIST_NEXT(entry, rwdts_member_registration_t, element);
    if (!entry->dts_internal) {
      rwdts_member_deregister((rwdts_member_reg_handle_t)entry);
    }
    entry = next_entry;
  }
  return RW_STATUS_SUCCESS;
}

void
member_add_multi_regs(rwdts_api_t* apih)
{

  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  int i;

#undef LOOP1
#define LOOP1 (1000 / 6)  //??  1000 => 12000  or /6=> ~2000
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);

  for(i = 0; i < LOOP1; i++) {
    keyspec_entry1.dompath.path000.has_key00 = 1;
    keyspec_entry1.dompath.path000.key00.key_one = i;
    keyspec_entry1.dompath.path001.has_key00 = 1;
    keyspec_entry1.dompath.path001.key00.key_two = i;
    keyspec_entry1.dompath.path002.has_key00 = 1;
    keyspec_entry1.dompath.path002.key00.key_three = i;
    keyspec_entry1.dompath.path003.has_key00 = 1;
    keyspec_entry1.dompath.path003.key00.key_four = i;

   rwdts_memberapi_register(NULL, apih, keyspec1, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = LOOP1; i < 2*LOOP1; i++) {
    keyspec_entrywd1.dompath.path000.has_key00 = 1;
    keyspec_entrywd1.dompath.path000.key00.key_one = i;
    keyspec_entrywd1.dompath.path001.has_key00 = 1;
    keyspec_entrywd1.dompath.path001.key00.key_two = i;
    keyspec_entrywd1.dompath.path002.has_key00 = 1;
    keyspec_entrywd1.dompath.path002.key00.key_three = i;

   rwdts_memberapi_register(NULL, apih, keyspec2, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = 2*LOOP1; i < 3*LOOP1; i++) {
    keyspec_entrywd2.dompath.path000.has_key00 = 1;
    keyspec_entrywd2.dompath.path000.key00.key_one = i;
    keyspec_entrywd2.dompath.path001.has_key00 = 1;
    keyspec_entrywd2.dompath.path001.key00.key_two = i;

   rwdts_memberapi_register(NULL, apih, keyspec3, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = 3*LOOP1; i < 4*LOOP1; i++) {
    keyspec_entrywd3.dompath.path000.has_key00 = 1;
    keyspec_entrywd3.dompath.path000.key00.key_one = i;

   rwdts_memberapi_register(NULL, apih, keyspec4, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  }

  return;
}

static void plumbing1_client_start_f(void *ctx) {
  rw_status_t rs = RW_STATUS_FAILURE;
  // static uint64_t serial_no = 0;
  // struct timeval time_val;

  struct plumbing1_state *s = (struct plumbing1_state *)ctx;
  TSTPRN("Plumbing1 client setup...\n");
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;

  RW_ASSERT(s->tenv->t[TASKLET_CLIENT].api_state_change_called);

  RW_ASSERT(apih);
  TSTPRN("Plumbing1 sending regist req...\n");
  
  RWDtsRegisterReq req;
  // RWDtsXactID id;
  rwdts_register_req__init(&req);
  rwmsg_closure_t clo = { };
  
  clo.pbrsp = (rwmsg_pbapi_cb)plumbing1_client_regcb;
  clo.ud=s;

  rs = rwdts_query_router__regist(&apih->client.service, apih->client.dest, &req, &clo, &apih->client.rwreq);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
}

TEST_F(RWDtsEnsemble, Plumbing1) {
  struct plumbing1_state s = { 0 };
  s.testno = tenv.testno;
  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  RW_ASSERT(s.tenv->t[TASKLET_CLIENT].api_state_change_called); // done in SetUp()
  rwsched_dispatch_async_f(tenv.t[TASKLET_CLIENT].tasklet, s.client.rwq, &s, plumbing1_client_start_f);
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 60, &s.exitnow);
  ASSERT_TRUE(s.exitnow);
  memset(&s, 0xaa, sizeof(s));
}


static void routermisc(void*ctx) {
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *) ctx;

  TSTPRN("Running router misc test function\n");
  rwdts_router_test_misc(ti->dts);
  ti->miscval = 1; // exitnow
}
TEST_F(RWDtsEnsemble, RouterMisc) {
  /* Run rwdts_router_test_misc internal test routine, exercises management code and other esoterica */
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].dts);
  tenv.t[TASKLET_ROUTER].miscval = 0;
  rwsched_dispatch_async_f(tenv.t[TASKLET_ROUTER].tasklet, rwsched_dispatch_get_main_queue(tenv.rwsched), &tenv.t[TASKLET_ROUTER], routermisc);
  rwsched_dispatch_main_until(tenv.t[0].tasklet, 10, &tenv.t[TASKLET_ROUTER].miscval);
  ASSERT_TRUE(tenv.t[TASKLET_ROUTER].miscval);
}
  int no_object;
  int trigger_create;


#if 0
static void queryapi_event_cb(rwdts_xact_t *xact, rwdts_event_t evt, void *ud)
{

  struct queryapi_state *s = (struct queryapi_state *)ud;
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_status_t status;

  TSTPRN("queryapi_event_cb got dts evt=%d\n", evt);

  s->client.rspct++;
  s->exitnow = 1;

  EXPECT_EQ(evt, RWDTS_EVENT_STATUS_CHANGE);

  rwdts_xact_result_t *result = NULL;

  status = rwdts_xact_get_result_pbraw(xact, 0, &result);

  ASSERT_EQ(status, RW_STATUS_SUCCESS);

}
#endif

static void 
is_test_done( struct queryapi_state *s)
{
  const ::testing::TestInfo* const test_info =
    ::testing::UnitTest::GetInstance()->current_test_info();
  bool exitnow = false;
  int num_cb = 0;

  if (!strcmp(test_info->test_case_name(), "RWDtsEnsemble")) {
    int i;
    if (!strcmp(test_info->name(), "AppConf_Success_LONG") ||
        !strcmp(test_info->name(), "AppConf_Delay_LONG")   ||
        !strcmp(test_info->name(), "AppConf_Cache_Update_Delay_LONG")   ||
        !strcmp(test_info->name(), "AppConf_PrepNotOk_LONG")   ||
        !strcmp(test_info->name(), "AppConf_Cache_Success_LONG")) {
      // for appconf success test, every member need to receive the following callbacks
      for (i = 0; i < TASKLET_CT; i++) {
        struct tenv1::tenv_info *ti = &RWDtsEnsemble::tenv.t[i];
        if (TASKLET_IS_MEMBER_OLD(ti->tasklet_idx)) {
          if (ti->cb.xact_deinit == 1) {
            num_cb++;
          }
        }
      }
      TSTPRN("is_test_done() called num_deinit = %d, exit_soon=%d\n", num_cb, s->exit_soon);
      if (/*num_cb == 3 &&*/ s->exit_soon == 3) {
        exitnow = true;
      }
    } else if (!strcmp(test_info->name(), "RegReady")) {
      for (i = 0; i < TASKLET_CT; i++) {
        struct tenv1::tenv_info *ti = &RWDtsEnsemble::tenv.t[i];
        if (TASKLET_IS_MEMBER_OLD(ti->tasklet_idx)) {
          if (ti->cb.reg_ready == 2) { // One pub and one sub
            num_cb++;
          }
          if (ti->cb.reg_ready_old == 1) {
            num_cb++;
          }
        }
      }
      if (num_cb == 6) {
        exitnow = true;
      }
    }
  }

  if (exitnow) {
    int i;
    for (i = 0; i < TASKLET_CT; i++) {
      struct tenv1::tenv_info *ti = &RWDtsEnsemble::tenv.t[i];
      if (TASKLET_IS_MEMBER_OLD(ti->tasklet_idx)) {
        if (!strcmp(test_info->name(), "AppConf_Success_LONG") ||
            !strcmp(test_info->name(), "AppConf_Delay_LONG")) {
          EXPECT_EQ(ti->cb.xact_init, 1);
          EXPECT_TRUE(ti->cb.validate< 2);
          EXPECT_TRUE(ti->cb.apply <3);
          EXPECT_EQ(ti->cb.xact_deinit, 1);
          if (!strcmp(test_info->name(), "AppConf_Success_LONG")) {
            EXPECT_EQ(ti->cb.prepare, 2);
          } else {
            EXPECT_EQ(ti->cb.prepare, 1);
          }
        } else if (!strcmp(test_info->name(), "RegReady")) {
          EXPECT_EQ(ti->cb.reg_ready, 2); // One pub and sub
          EXPECT_EQ(ti->cb.reg_ready_old, 1); //  Only called for pub since sub cache fecth  brings nothing
        }
      }
      // Reset the cb struct
      memset(&ti->cb, 0, sizeof(ti->cb));
    }
    if (!s->exitnow) {
      TSTPRN("is_test_done() starting 1/3s timer for set_exitnow(ud=%p)\n", &s->exitnow);
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, when,
                             s->member[0].rwq, &s->exitnow, set_exitnow);
    }
  }
  return;
}

static void queryapi_client_start_f(void *ctx) {

  struct queryapi_state*   s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  TSTPRN("Query API client setup...\n");

  // Initilize the API
  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_NE(apih, (void*)NULL);
  TSTPRN("Query API Initialized API Handle...\n");
  s->exitnow = 1;
}

TEST_F(RWDtsEnsemble, QueryApiTransactional) {
  struct queryapi_state s = {};
  s.testno = tenv.testno;
  s.magic = QAPI_STATE_MAGIC;
  const ::testing::TestInfo* const test_info =
  ::testing::UnitTest::GetInstance()->current_test_info();

  strncpy(s.test_name,  test_info->name(), sizeof(s.test_name) - 1);
  strncpy(s.test_case,  test_info->test_case_name(), sizeof(s.test_case) - 1);

  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  s.client.transactional = TRUE;
  rwsched_dispatch_async_f(tenv.t[TASKLET_CLIENT].tasklet, s.client.rwq, &s, queryapi_client_start_f);
  double seconds = (RUNNING_ON_VALGRIND)?180:60;
  rwsched_dispatch_main_until(tenv.t[0].tasklet, seconds, &s.exitnow);
  ASSERT_TRUE(s.exitnow);
}


TEST_F(RWDtsEnsemble, QueryApiNontransactional) {
  struct queryapi_state s = {};
  s.testno = tenv.testno;
  s.magic = QAPI_STATE_MAGIC;

  const ::testing::TestInfo* const test_info =
  ::testing::UnitTest::GetInstance()->current_test_info();

  strncpy(s.test_name,  test_info->name(), sizeof(s.test_name) - 1);
  strncpy(s.test_case,  test_info->test_case_name(), sizeof(s.test_case) - 1);
  s.tenv = &tenv;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  rwsched_dispatch_async_f(tenv.t[TASKLET_CLIENT].tasklet, s.client.rwq, &s, queryapi_client_start_f);
  double seconds = (RUNNING_ON_VALGRIND)?180:60;
  rwsched_dispatch_main_until(tenv.t[0].tasklet, seconds, &s.exitnow);
  ASSERT_TRUE(s.exitnow);
}


/* Seems to be no way to send an empty request through the APIs */
TEST_F(RWDtsEnsemble, QueryApiNontransactionalNoreqs) {
  struct queryapi_state s = {};
  s.testno = tenv.testno;
  s.magic = QAPI_STATE_MAGIC;

  const ::testing::TestInfo* const test_info =
  ::testing::UnitTest::GetInstance()->current_test_info();

  strncpy(s.test_name,  test_info->name(), sizeof(s.test_name) - 1);
  strncpy(s.test_case,  test_info->test_case_name(), sizeof(s.test_case) - 1);
  s.tenv = &tenv;
  s.client.noreqs = TRUE;
  s.client.rwq = rwsched_dispatch_get_main_queue(tenv.rwsched);
  rwsched_dispatch_async_f(tenv.t[TASKLET_CLIENT].tasklet, s.client.rwq, &s, queryapi_client_start_f);
  double seconds = (RUNNING_ON_VALGRIND)?180:60;
  rwsched_dispatch_main_until(tenv.t[0].tasklet, seconds, &s.exitnow);
  ASSERT_TRUE(s.exitnow);
}


static void rwdts_member_fill_response(RWPB_T_MSG(DtsTest_Person) *person, int i, bool getnext=false) {
  RWPB_F_MSG_INIT(DtsTest_Person,person);
  RW_ASSERT(i >= 0 && i <= 29);
  person->name = strdup("0-RspTestName");
  person->name[0] = '0' + i;
  person->has_id = TRUE;
  person->id = 2000 + i;
  person->email = strdup("0-rsp_test@test.com");
  person->email[0] = '0' + i;
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);

  if (getnext)
  {
    asprintf(&person->phone[0]->number, "%10d", rand());
  }
  else
  {
    person->phone[0]->number = strdup("089-123-456");
  }
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_HOME;
  return;
}

static rwdts_member_rsp_code_t
memberapi_test_dispatch_get_next_response(memberapi_test_t* mbr, uint32_t credits, uint32_t num_responses)
{
  uint32_t i;
  rwdts_member_rsp_code_t rsp_code;

  RW_ASSERT(mbr);
  rwdts_xact_t *xact = mbr->xact;
  RW_ASSERT(xact);
  rwdts_query_handle_t qhdl = mbr->qhdl;
   
  RWPB_T_MSG(DtsTest_Person) *person;
  ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;

  RW_ZERO_VARIABLE(&rsp);

 
  array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*) * num_responses);
  RW_ASSERT(num_responses > 0);

  rsp.ks = mbr->key;

  rsp.msgs = (ProtobufCMessage**)array;
  for (i = 0; i < num_responses; i++) 
  {
    person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));
    array[i] = &(person->base);
    rwdts_member_fill_response(person, i, true);
    rsp.n_msgs++;
  }
  mbr->getnext_ptr = (void *)mbr;
  if (num_responses < mbr->results_pending) //more results pending
  {
    mbr->results_pending-=i;
    rsp.evtrsp = RWDTS_EVTRSP_ASYNC;
    rsp.getnext_ptr = mbr->getnext_ptr;
    printf ("APP>>>MORE:mbr[%u]->Sending[%u]results_pending %d query->credits %d num_responses %d getnext_ptr %p \n", 
            mbr->inst,
            rsp.n_msgs,
            mbr->results_pending, credits, num_responses, rsp.getnext_ptr);
    rw_status_t rs = rwdts_member_send_response_more(xact, qhdl, &rsp, rsp.getnext_ptr);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    rsp_code = RWDTS_ACTION_ASYNC;
  }
  else
  {
    mbr->results_pending=0;
    rsp_code = RWDTS_ACTION_OK;
    rsp.evtrsp = RWDTS_EVTRSP_ACK;
    printf ("APP>>>DONE:mbr[%u]->Sending[%u]results_pending %d query->credits %d num_responses %d member_responses %d \n",
            mbr->inst,
            rsp.n_msgs,
            mbr->results_pending, credits, num_responses, num_responses);
    rw_status_t rs = rwdts_member_send_response(xact, qhdl, &rsp);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
    RW_FREE(mbr);
  }
  for (i = 0; i < num_responses; i++) {
    protobuf_c_message_free_unpacked(NULL, array[i]);
    array[i] = NULL;
  }
  RW_FREE(array);
  return rsp_code;
}

static void
memberapi_test_dispatch_response(memberapi_test_t* mbr) 
{
  int num_responses = 10;
  int i;

  RW_ASSERT(mbr);
  rwdts_xact_t *xact = mbr->xact;
  RW_ASSERT(xact);
  rwdts_query_handle_t qhdl = mbr->qhdl;
  rwdts_match_info_t *match = (rwdts_match_info_t*)qhdl;
  RW_ASSERT(match);
  RWDtsQuery *query = match->query;
  RW_ASSERT(query);
   
  RWPB_T_MSG(DtsTest_Person) *person;
  ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;

  RW_ZERO_VARIABLE(&rsp);

  if (query->action != RWDTS_QUERY_READ) {
    num_responses = 1;
  }
 
  array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*) * num_responses);

  RW_ASSERT(num_responses > 0);

  rsp.ks = mbr->key;


  rsp.msgs = (ProtobufCMessage**)array;
  for (i = 0; i < num_responses; i++) 
  {

    person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));
    
    array[i] = &(person->base);

    rwdts_member_fill_response(person, i);

    rsp.n_msgs++;
    if (mbr->send_nack) 
    {
      rsp.evtrsp  = ((i+1) == num_responses)? RWDTS_EVTRSP_NACK:RWDTS_EVTRSP_ASYNC;
      if (mbr->inst == 1)
      {
        rwdts_xact_info_send_error_xpath(&match->xact_info,
                                RW_STATUS_NOTFOUND, 
                                "/ps:person",
                                "ERRORERRORERRORERRORERRORERRORERRORERRORERROR");
      }
    } 
    else 
    {
      rsp.evtrsp  = ((i+1) == num_responses)? RWDTS_EVTRSP_ACK:RWDTS_EVTRSP_ASYNC;
    }
  }
  TSTPRN("APP->%s member %d responding with %u responses, qidx[%u] with %s\n", 
          xact->apih->client_path, 
	 mbr->inst,
          num_responses, 
          query->queryidx, rwdts_evtrsp_to_str( rsp.evtrsp));
  rw_status_t rs = rwdts_member_send_response_int(xact, qhdl, &rsp);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  for (i = 0; i < num_responses; i++) {
    protobuf_c_message_free_unpacked(NULL, array[i]);
    array[i] = NULL;
  }
  RW_FREE(array);
  RW_FREE(mbr);

}

static void
memberapi_test_dispatch_delay_response(memberapi_test_t* mbr)
{
  RW_ASSERT(mbr);
  rwdts_xact_t *xact = mbr->xact;
  RW_ASSERT(xact);
  rwdts_query_handle_t qhdl = mbr->qhdl;

  RWPB_T_MSG(DtsTest_Person) *person;
  static RWPB_T_MSG(DtsTest_Person) person_instance = {};
  ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;

  person = &person_instance;
  RW_ZERO_VARIABLE(&rsp);

  array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
  array[0] = &person->base;

  rwdts_member_fill_response(person, 0);

  rsp.n_msgs = 1;
  rsp.msgs = (ProtobufCMessage**)array;
  rsp.ks = mbr->key;
  rsp.evtrsp  = RWDTS_EVTRSP_ACK;

  printf("Done..... \n");
  TSTPRN("APP->%s\n", xact->apih->client_path);
  rw_status_t rs = rwdts_member_send_response(xact, qhdl, &rsp);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  protobuf_c_message_free_unpacked_usebody(NULL, &person->base);
  RW_FREE(array);
  RW_FREE(mbr);
}

static rwdts_member_rsp_code_t memberapi_test_abort(const rwdts_xact_info_t* xact_info,
                                                    void *ud)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member abort  ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) 
  {
  case TREATMENT_ABORT_NACK:
  {
    rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
    HASH_ITER(hh, xact->queries, xquery, qtmp)   
    {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
      //rsp.ks = xquery->query->key;
      rwdts_member_send_response((rwdts_xact_t*)xact, xact_info->queryh, &rsp);
    }
    TSTPRN("**********RWDTS_ACTION_NOT_OK for ABORT *******\n");
    return RWDTS_ACTION_NOT_OK;
    break;
  }
  default:
    return RWDTS_ACTION_OK;
    break;
  }
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t memberapi_test_precommit(const rwdts_xact_info_t* xact_info,
                                                        void* ud)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member pre-commit ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
  case TREATMENT_PRECOMMIT_NULL:
    RW_CRASH();
    return RWDTS_ACTION_NOT_OK;
    break;
  case TREATMENT_PRECOMMIT_NACK:
  {
    rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
    HASH_ITER(hh, xact->queries, xquery, qtmp) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
      //rsp.ks = xquery->query->key;
      rwdts_member_send_response((rwdts_xact_t*)xact, xact_info->queryh, &rsp);
    }
    return RWDTS_ACTION_NOT_OK;
    break;
  }
  case TREATMENT_ACK:
  {
    return RWDTS_ACTION_OK;
  }

  default:
    return RWDTS_ACTION_OK;
    break;
  }
}
static rwdts_member_rsp_code_t memberapi_test_commit(const rwdts_xact_info_t* xact_info,
                                                     void* ud)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member commit ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
  case TREATMENT_COMMIT_NACK:
  {
    rwdts_xact_query_t *xquery=NULL, *qtmp=NULL;
    HASH_ITER(hh, xact->queries, xquery, qtmp) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
      //rsp.ks = xquery->query->key;
      rwdts_member_send_response((rwdts_xact_t*)xact, xact_info->queryh, &rsp);
    }
    return RWDTS_ACTION_NOT_OK;
    break;
  }
  case TREATMENT_ACK:
  {
    return RWDTS_ACTION_OK;
  }
  default:
    return RWDTS_ACTION_OK;
    break;
  }
}
static rwdts_member_rsp_code_t 
memberapi_test_get_next(const rwdts_xact_info_t* xact_info,
                        RWDtsQueryAction         action,
                        const rw_keyspec_path_t* key,
                        const ProtobufCMessage*  msg,
                        void *user_data)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT (action == RWDTS_QUERY_READ);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;
  memberapi_test_t *mbr = NULL;

  void * get_next_key = rwdts_xact_info_get_next_key(xact_info);
  uint32_t credits = rwdts_xact_info_get_credits(xact_info);

  RW_ASSERT(apih);
  RW_ASSERT(s);
  RW_ASSERT(credits);

  TSTPRN ("APP->Calling DTS Member-%d getnext ... Parameters %d %p %s\n", 
          ti->member_idx,
         credits, 
         get_next_key, 
         rwdts_get_xact_state_string(xact->mbr_state));


  if(!get_next_key)
  {
    mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));
    mbr->inst = ti->member_idx;
    mbr->results_pending = credits;
    // Credits is 20 and  member_responses is 5 10 and 25
    // This meands that for three members
    // one will have 5More + 5M +5M + 5Done
    // One will have 25(20More + 5 PendingRsp)
    // One will have 20D FIXME
  }
  else 
  {
    sleep(1);
    mbr = (memberapi_test_t *)get_next_key;
  }

  mbr->xact = xact;
  mbr->qhdl = xact_info->queryh;
  mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

  return memberapi_test_dispatch_get_next_response(mbr, credits, ti->member_responses);
}
static rwdts_member_rsp_code_t memberapi_test_prepare_new_version(const rwdts_xact_info_t* xact_info,
                                                                  RWDtsQueryAction         action,
                                                                  const rw_keyspec_path_t* key,
                                                                  const ProtobufCMessage*  msg, 
                                                                  void*                    ud)
{
  void* get_next_key = rwdts_xact_info_get_next_key(xact_info);
  uint32_t credits = rwdts_xact_info_get_credits(xact_info);

  return (memberapi_test_prepare(xact_info, action, key, msg,credits,get_next_key));
}

static rwdts_member_rsp_code_t memberapi_test_prepare(const rwdts_xact_info_t* xact_info,
                                                      RWDtsQueryAction         action,
                                                      const rw_keyspec_path_t* key,
                                                      const ProtobufCMessage*  msg,
                                                      uint32_t                 credits,
                                                      void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih;

  apih = xact->apih;
  RW_ASSERT(apih);

  RWPB_T_MSG(DtsTest_Person) *person;
  static RWPB_T_MSG(DtsTest_Person) person_instance = {};
  static ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;
  person = &person_instance;
 
  RW_ASSERT(s);

  TSTPRN("Calling DTS Member prepare ...%d %d\n", ti->member_idx, s->member[ti->member_idx].treatment);

  bool rtr_connected = (rwdts_get_router_conn_state(apih) == RWDTS_RTR_STATE_UP)?true:false;
  RW_ASSERT(rtr_connected);

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK: 
  case TREATMENT_NACK:
  case TREATMENT_COMMIT_NACK: 
  case TREATMENT_ABORT_NACK: 
  case TREATMENT_PRECOMMIT_NACK: 
  case TREATMENT_PRECOMMIT_NULL:

    rsp.ks = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp.ks));

    array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
    array[0] = &person->base;

    rwdts_member_fill_response(person, 0);

    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)array;
    if (s->member[ti->member_idx].treatment == TREATMENT_NACK) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
    }
    else {
      rsp.evtrsp  = RWDTS_EVTRSP_ACK;
    }

    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);
//  rwdts_xact_info_respond_xpath(xact_info,dts_rsp_code,xpath,array[0]);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    protobuf_c_message_free_unpacked_usebody(NULL, &person->base);
   
    RW_FREE(array);
    return RWDTS_ACTION_OK;

  case TREATMENT_NA:  
    return RWDTS_ACTION_NA;

  case TREATMENT_ASYNC:
  case TREATMENT_ASYNC_NACK: {
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    if (s->member[ti->member_idx].treatment == TREATMENT_ASYNC_NACK) {
      mbr->send_nack = TRUE;
    }
    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->inst = ti->member_idx;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2);
    rwsched_dispatch_after_f(apih->tasklet,
			     when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_response);
    TSTPRN("Member %d returning ASYNC from prepare\n", ti->member_idx);
    return RWDTS_ACTION_ASYNC;
  }
  case TREATMENT_BNC:
    rsp.evtrsp  = RWDTS_EVTRSP_ASYNC;
    return RWDTS_ACTION_OK;

  case TREATMENT_PREPARE_DELAY: {
    uint32_t delay_sec;
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    // Dispatch can be called from main q alone???
    if (s->client.client_abort) {
      delay_sec = 5;
    } else {
      delay_sec = 10 * (1 + ti->member_idx);
    }


    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, delay_sec * NSEC_PER_SEC);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_delay_response);
    return RWDTS_ACTION_ASYNC;
  }
  default: 
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
  // ATTN How to free array
}

static rwdts_member_rsp_code_t memberapi_test_prepare_multiple(const rwdts_xact_info_t* xact_info,
                                                      RWDtsQueryAction         action,
                                                      const rw_keyspec_path_t* key,
                                                      const ProtobufCMessage*  msg,
                                                      void*                    ud)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  rwdts_xact_t *new_xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_member_query_rsp_t rsp;
  RWPB_M_MSG_DECL_INIT(DtsTest_PhoneNumber, phone);
  static ProtobufCMessage **array;
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih;
  uint32_t flags = 0;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  TSTPRN("tasklet[%d] member[%d] in memberapi_test_prepare_multiple \n", 
	 ti->tasklet_idx, ti->member_idx);

  apih = xact->apih;
  RW_ASSERT(apih);

  RW_ASSERT(s);

  bool rtr_connected = (rwdts_get_router_conn_state(apih) == RWDTS_RTR_STATE_UP)?true:false;
  RW_ASSERT(rtr_connected);

  switch (ti->tasklet_idx) {
    case TASKLET_MEMBER_0:
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP("1");
    break;
    
    case TASKLET_MEMBER_1:
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
    break;

    case TASKLET_MEMBER_2:
    default:
      keyspec_entry.dompath.path001.has_key00 = 1;
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP("2");
      keyspec = (rw_keyspec_path_t*) &keyspec_entry;
      rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
      rsp.ks = keyspec;
      RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp.ks));
      array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
      array[0] = &phone.base;
      phone.number = RW_STRDUP("2");
      rsp.n_msgs = 1;
      rsp.msgs = (ProtobufCMessage**)array;
      rsp.evtrsp  = RWDTS_EVTRSP_ACK;
      TSTPRN("tasklet[%d] member[%d] in memberapi_test_prepare_multiple sending response key00.number='%s'\n", 
	     ti->tasklet_idx, ti->member_idx, keyspec_entry.dompath.path001.key00.number);
      rwdts_member_send_response(xact, xact_info->queryh, &rsp);
      protobuf_c_message_free_unpacked_usebody(NULL, &phone.base);
      RW_FREE(keyspec_entry.dompath.path001.key00.number);
      RW_FREE(array);
      return RWDTS_ACTION_OK;
    break;
  }

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  TSTPRN("tasklet[%d] member[%d] in memberapi_test_prepare_multiple sending subquery key00.number='%s', %stransactional\n", 
	 ti->tasklet_idx, ti->member_idx, keyspec_entry.dompath.path001.key00.number,
	 ((flags&RWDTS_XACT_FLAG_NOTRAN)?"NON":""));

  new_xact = rwdts_api_query_ks(clnt_apih, keyspec, RWDTS_QUERY_READ,
                            flags, rwdts_clnt_query_callback_multiple, ti,
                            &(phone.base));
  RW_ASSERT(new_xact);
  ti->saved_xact_info = (rwdts_xact_info_t *)xact_info;
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  return RWDTS_ACTION_ASYNC;
}

static rwdts_member_rsp_code_t memberapi_test_anycast_prepare(const rwdts_xact_info_t* xact_info,
                                                              RWDtsQueryAction         action,
                                                              const rw_keyspec_path_t*      key,
                                                              const ProtobufCMessage*  msg, 
                                                              uint32_t credits,
                                                              void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;
  rwdts_member_reg_handle_t regh = xact_info->regh;

  RW_ASSERT(apih);
  RW_ASSERT(regh);

  RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  static RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) cpu_instance = {}; 
  rwdts_member_query_rsp_t rsp;

  cpu = &cpu_instance; 
  testdts_rw_base__yang_data__testdts_rw_base__vcs__resources__vm__cpu__init(cpu); 

  RW_ASSERT(s);

  s->prepare_cb_called++;

  TSTPRN("Calling DTS Member prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  rwdts_member_cursor_t *cursor1 = rwdts_member_data_get_cursor(xact,
                                                       regh);

  RW_ASSERT(cursor1);

  rwdts_member_cursor_t *cursor2 = rwdts_member_data_get_cursor(xact,
                                                      s->regh[ti->tasklet_idx]);
  RW_ASSERT(cursor2);

  ProtobufCMessage *msg2 = NULL;
  ProtobufCMessage *msg1 = NULL;
  rw_keyspec_path_t *out_ks = NULL;
  while((msg2 =
               (ProtobufCMessage*)rwdts_member_reg_handle_get_next_element(
                                                  s->regh[ti->tasklet_idx],
                                                  cursor2, xact,
                                                  &out_ks)) != NULL) {
        RW_ASSERT(msg2);
       
        msg1 = (ProtobufCMessage*)rwdts_member_reg_handle_get_next_element(
                                                  regh,
                                                  cursor1, xact, &out_ks);

        RW_ASSERT(!msg1);
  } 

  rwdts_member_data_delete_cursors(xact, regh);
  rwdts_member_data_delete_cursors(xact, s->regh[ti->tasklet_idx]);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:

    rsp.ks = (rw_keyspec_path_t*)key;
  
    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)&cpu;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;
    rwdts_member_send_error(xact,
                            key,
                            ((rwdts_match_info_t*)xact_info->queryh)->query,
                            NULL,
                            &rsp,
                            RW_STATUS_NOTFOUND,
                            "ERRORERRORERRORERRORERRORERRORERRORERRORERROR ");

    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);

    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
}

static rwdts_member_rsp_code_t
memberapi_test_merg_prep(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t*      key,
                         const ProtobufCMessage*  msg,
                         uint32_t credits,
                         void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));

  if (ti->member_idx != 0) {
    return RWDTS_ACTION_OK;
  }
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rw_keyspec_path_t* keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "mergecolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "mergenccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc;
  nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, nc);

  strncpy(nc->name,"mergenccontext",sizeof(nc->name));
  nc->has_name = 1;
  nc->n_interface = 1;
  nc->interface = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface**)RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*));
  nc->interface[0] = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*)RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface, nc->interface[0]);

  strcpy(nc->name, "mergenccontext");
  nc->has_name = 1;
  strcpy(nc->interface[0]->name, "Interface1");
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.has_loopback = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->n_ip = 1;
  strcpy(&nc->interface[0]->ip[0].address[0], "10.10.10.10");
  nc->interface[0]->ip[0].has_address = 1;

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs =(ProtobufCMessage**)&nc;
  rsp.evtrsp = RWDTS_EVTRSP_ASYNC;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);

  rsp.evtrsp = RWDTS_EVTRSP_ACK;
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t
memberapi_test_publish_prep(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t*      key,
                         const ProtobufCMessage*  msg,
                         uint32_t credits,
                         void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  EXPECT_NE(action,RWDTS_QUERY_READ);

  return RWDTS_ACTION_OK;
}
static rwdts_member_rsp_code_t
memberapi_test_audit_prep(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t*      key,
                         const ProtobufCMessage*  msg,
                         uint32_t credits,
                         void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));

  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  int num_records = 10;
  int mismatch = 0;
  int additional = 0;
  int change_every_iter = 0;
  int i;

  if (((rwdts_member_registration_t*)s->regh[TASKLET_CLIENT])->audit.audit_serial > 1) {
    switch(s->audit_test) {
      case RW_DTS_AUDIT_TEST_RECONCILE_SUCCESS:
        num_records = 5;
        mismatch = 1;
        additional = 1;
        break;
      case RW_DTS_AUDIT_TEST_RECOVERY:
        num_records = 1;
        mismatch = 1;
        additional = 1;
        break;
      case RW_DTS_AUDIT_TEST_REPORT:
        break;
      case RW_DTS_AUDIT_TEST_FAIL_CONVERGE:
        change_every_iter = 1;
        break;
      default:
        RW_CRASH();
    }
  }

  ti->prepare_cb_cnt++;

  rw_keyspec_path_t* keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  snprintf(keyspec_entry.dompath.path000.key00.name, sizeof(keyspec_entry.dompath.path000.key00.name),
           "mycolony%d",  ti->member_idx+ 1);
  
  keyspec_entry.dompath.path001.has_key00 = 1;
  snprintf(keyspec_entry.dompath.path001.key00.name, sizeof(keyspec_entry.dompath.path001.key00.name),
           "mycontext%d", ti->member_idx+ (change_every_iter?ti->prepare_cb_cnt*2+1:1));

  TSTPRN("prep_cb_cnt[%d], num_records = %d, mismatch[%c], additional[%c]\n",
         ti->prepare_cb_cnt, num_records, mismatch?'Y':'N', additional?'Y':'N');

  for (i = 0; i < num_records; i++) {
    RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc;
    nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)
           RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
    RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, nc);

    snprintf(nc->name, sizeof(nc->name), "mycontext%d", ti->member_idx+(change_every_iter? ti->prepare_cb_cnt*2+1:1));
    nc->has_name = 1;

    nc->n_interface = 1;
    nc->interface = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface**)
                     RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*));
    nc->interface[0] = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*)
                        RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface));
    RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface, nc->interface[0]);

    if (ti->member_idx == 0) {
      sprintf(nc->interface[0]->name, "Interface1%d", i);
      nc->interface[0]->has_name = 1;
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      if (mismatch && (i+1) == num_records) {
        strcpy(&nc->interface[0]->ip[0].address[0], "10.10.10.100");
      } else {
        strcpy(&nc->interface[0]->ip[0].address[0], "10.10.10.10");
      }
      nc->interface[0]->ip[0].has_address = 1;
    }

    if (ti->member_idx == 1) {
      sprintf(nc->interface[0]->name, "Interface1%d", i);
      nc->interface[0]->has_name = 1;
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      if (mismatch && (i+1) == num_records) {
        strcpy(&nc->interface[0]->ip[0].address[0], "20.20.20.100");
      } else {
        strcpy(&nc->interface[0]->ip[0].address[0], "20.20.20.20");
      }
      nc->interface[0]->ip[0].has_address = 1;
    }

    if (ti->member_idx == 2) {
      sprintf(nc->interface[0]->name, "Interface1%d", i);
      nc->interface[0]->has_name = 1;
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      if (mismatch && (i+1) == num_records) {
        strcpy(&nc->interface[0]->ip[0].address[0], "30.30.30.100");
      } else {
        strcpy(&nc->interface[0]->ip[0].address[0], "30.30.30.30");
      }
      nc->interface[0]->ip[0].has_address = 1;
    }

    char *ks_str = NULL;
    rw_keyspec_path_get_new_print_buffer(keyspec, NULL, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(TestdtsRwFpath), &ks_str);
    TSTPRN("Line:%d/Member:%d The keyspec string is %s\n", __LINE__, ti->member_idx, ks_str ? ks_str : "");
    free(ks_str);

    rsp.n_msgs = 1;
    rsp.ks = keyspec;
    rsp.msgs = (ProtobufCMessage**)&nc;
    rsp.evtrsp = (i+1 == num_records && !additional) ? RWDTS_EVTRSP_ACK : RWDTS_EVTRSP_ASYNC;

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    protobuf_c_message_free_unpacked(NULL, &nc->base);
  }

  if (additional) {
    keyspec_entry.dompath.path000.has_key00 = 1;
    snprintf(keyspec_entry.dompath.path000.key00.name, sizeof(keyspec_entry.dompath.path000.key00.name),
             "mycolony%d",  ti->member_idx+1);
    keyspec_entry.dompath.path001.has_key00 = 1;
    snprintf(keyspec_entry.dompath.path001.key00.name, sizeof(keyspec_entry.dompath.path001.key00.name),
             "mycontext1%d", ti->member_idx+1);

    RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc;
    nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)
           RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
    RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, nc);

    nc->n_interface = 1;
    snprintf(nc->name, sizeof(nc->name), "mycontext1%d", ti->member_idx+1);
    nc->has_name = 1;
    nc->interface = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface**)
                     RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*));
    nc->interface[0] = (TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface*)
                        RW_MALLOC0(sizeof(TestdtsRwFpath__YangData__TestdtsRwBase__Colony__NetworkContext__Interface));
    RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface, nc->interface[0]);

    char *ks_str = NULL;
    rw_keyspec_path_get_new_print_buffer(keyspec, NULL, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(TestdtsRwFpath), &ks_str);
    TSTPRN("Line:%d/Member:%d The keyspec string is %s\n", __LINE__, ti->member_idx, ks_str ? ks_str : "");
    free(ks_str);

    if (ti->member_idx == 0) {
      strcpy(nc->interface[0]->name, "Interface100");
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      strcpy(&nc->interface[0]->ip[0].address[0], "100.100.100.100");

    }

    if (ti->member_idx == 1) {
      strcpy(nc->interface[0]->name, "Interface200");
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      strcpy(&nc->interface[0]->ip[0].address[0], "200.200.200.200");
    }

    if (ti->member_idx == 2) {
      strcpy(nc->interface[0]->name, "Interface300");
      nc->interface[0]->has_interface_type = 1;
      nc->interface[0]->interface_type.has_loopback = 1;
      nc->interface[0]->interface_type.loopback = 1;
      nc->interface[0]->n_ip = 1;
      strcpy(&nc->interface[0]->ip[0].address[0], "300.300.300.300");
    }

    nc->interface[0]->has_name = 1;
    nc->interface[0]->ip[0].has_address = 1;

    rsp.n_msgs = 1;
    rsp.ks = keyspec;
    rsp.msgs = (ProtobufCMessage**)&nc;
    rsp.evtrsp = RWDTS_EVTRSP_ACK;

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    protobuf_c_message_free_unpacked(NULL, &nc->base); 
  }

  return RWDTS_ACTION_ASYNC;
}

static void rwdts_clnt_try_append_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  char tmp_log_xact_id_str[128] = ""; 


  RW_ASSERT(s);

  TSTPRN("xact[%s] %s In Client rwdts_clnt_try_append_callback \n",
         (char*)rwdts_xact_id_str(&(xact)->id, tmp_log_xact_id_str, sizeof(tmp_log_xact_id_str)),
         xact->apih->client_path);
  TSTPRN("Calling rwdts_clnt_try_append_callback with status=%d in tasklet=%d status->xact_done=%d status->responses_done=%d xact_status->block=%p\n", 
        xact_status->status,
        ti->tasklet_idx,
        xact_status->xact_done,
        xact_status->responses_done,
        xact_status->block);

  rwdts_query_result_t *res = NULL;
  int resct = 0;
  if (xact_status->block) {
    for (res = rwdts_xact_block_query_result(xact_status->block, 1002);
         res;
         res = rwdts_xact_block_query_result(xact_status->block, 1002)) {
      resct++;
      if (s->multi_router) {
        const RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *phone_m = NULL;
        phone_m = (const RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *)rwdts_query_result_get_protobuf(res);
        ASSERT_TRUE(phone_m);
        ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_data_MultiPerson_MultiPhone), phone_m->base.descriptor);
        EXPECT_STREQ(phone_m->number, "5678");
        EXPECT_EQ(phone_m->type, DTS_TEST_PHONE_TYPE_HOME);
      }
      else {
        const RWPB_T_MSG(DtsTest_data_Person_Phone) *phone = NULL;
        phone = (const RWPB_T_MSG(DtsTest_data_Person_Phone) *)rwdts_query_result_get_protobuf(res);
        ASSERT_TRUE(phone);
        ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone), phone->base.descriptor);
        EXPECT_STREQ(phone->number, "5678");
        EXPECT_EQ(phone->type, DTS_TEST_PHONE_TYPE_HOME);
      }
    }
    ti->async_rsp_state.rspct += resct;
    TSTPRN("Tasklet=%d got %d results to query 1002, total now %d via block=%p\n\n\n", ti->tasklet_idx, resct, ti->async_rsp_state.rspct, xact_status->block);
  } else {
    // bug path, there is supposed to be distinct xact vs block callbacks
    for (res = rwdts_xact_query_result(xact, 1002);
	 res;
	 res = rwdts_xact_query_result(xact, 1002)) {
      resct++;
      const RWPB_T_MSG(DtsTest_data_Person_Phone) *phone = NULL;
      phone = (const RWPB_T_MSG(DtsTest_data_Person_Phone) *)rwdts_query_result_get_protobuf(res);
      ASSERT_TRUE(phone);
      ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone), phone->base.descriptor);
      EXPECT_STREQ(phone->number, "5678");   // or is it 567812340 ???
	//      EXPECT_STREQ(phone->number, "567812340"); // the toplevel response from the prepare
      EXPECT_EQ(phone->type, DTS_TEST_PHONE_TYPE_HOME);
    }
    ti->async_rsp_state.rspct += resct;
    TSTPRN("Tasklet=%d got %d results to query 1002, total now %d via xact handle, no block?\n\n\n", ti->tasklet_idx, ti->async_rsp_state.rspct, resct);
  }

  // The transaction may be done now.  However, we don't especially care, we leave that
  // to the xact-wide callback.

  rwdts_xact_block_t *block = xact_status->block;
  EXPECT_TRUE(block);
  EXPECT_EQ(block, ti->async_rsp_state.block);
  EXPECT_TRUE(xact_status->block);
  EXPECT_EQ(block, xact_status->block);

  if (xact_status->responses_done && ti->async_rsp_state.key) {

//    EXPECT_EQ(ti->async_rsp_state.rspct, 3);

    switch (s->member[ti->member_idx].treatment) {

    case TREATMENT_ACK:
      {
        uint32_t flags;
        rwdts_member_query_rsp_t rsp;

        RW_ASSERT(ti->async_rsp_state.key);
        rsp.ks = (rw_keyspec_path_t*)ti->async_rsp_state.key;

        RWPB_T_MSG(DtsTest_data_Person_Phone) *phone = NULL;
        static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
        phone = &phone_instance;

        RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) *phonekey_ps = (RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) *)ti->async_rsp_state.key;
        const char *keynum = (phonekey_ps->dompath.path001.has_key00 ?
                              phonekey_ps->dompath.path001.key00.number : NULL);

        dts_test__yang_data__dts_test__person__phone__init(phone);
        phone->number = RW_STRDUP(keynum);
        phone->has_type = 1;
        phone->type = DTS_TEST_PHONE_TYPE_HOME;

        TSTPRN("Tasklet=%d append callback responding to original request for/with phone=%s\n", ti->tasklet_idx, keynum);

        rsp.n_msgs = 1;
        rsp.msgs = (ProtobufCMessage**)&phone;
        rsp.evtrsp  = RWDTS_EVTRSP_ACK;

        RW_ASSERT(ti->async_rsp_state.xact_info);
        flags = rwdts_member_get_query_flags(ti->async_rsp_state.xact_info->queryh);
        EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

        if (!s->multi_router)
        rwdts_member_send_response(xact, ti->async_rsp_state.xact_info->queryh, &rsp);
        ti->async_rsp_state.xact_info = NULL;
        RW_FREE(phone->number);
        if (ti->async_rsp_state.key) {
          rw_keyspec_path_free(ti->async_rsp_state.key, NULL);
          ti->async_rsp_state.key = NULL;
        }
        if (ti->async_rsp_state.msg) {
          protobuf_c_message_free_unpacked(NULL, ti->async_rsp_state.msg);
          ti->async_rsp_state.msg = NULL;
        }

      }
      break;

    default:
      // Answered from original prepare
      break;
    }

  } else {
    TSTPRN("Tasklet=%d append callback, responses not done / more expected...\n", ti->tasklet_idx);
  }

  return;
}

static rwdts_member_rsp_code_t memberapi_test_append_prepare(const rwdts_xact_info_t* xact_info,
                                                             RWDtsQueryAction         action,
                                                             const rw_keyspec_path_t*      key,
                                                             const ProtobufCMessage*  msg, 
                                                             uint32_t credits,
                                                             void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  uint32_t flags = 1;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;
  rw_status_t rs;

  RW_ASSERT(apih);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) *phonekey_ps = (RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) *)key;
  RWPB_T_PATHSPEC(DtsTest_data_MultiPerson_MultiPhone) *phonekey_m_ps = (RWPB_T_PATHSPEC(DtsTest_data_MultiPerson_MultiPhone) *)key;
  char *keynum = NULL;
  if (s->multi_router)  {
    keynum = (phonekey_m_ps->dompath.path001.has_key00 ?
			phonekey_m_ps->dompath.path001.key00.number : NULL);
  }
  else {
    keynum = (phonekey_ps->dompath.path001.has_key00 ?
			phonekey_ps->dompath.path001.key00.number : NULL);
  }

  int in_subquery = (0==strcmp(keynum, "5678"));

  TSTPRN("In DTS Member prepare (query key phone=%s) ... tasklet=%d in_subquery=%d\n", 
	 (keynum ? : "(no key00 / phone)"),
	 ti->tasklet_idx,
	 in_subquery);

#if 0
  dts_test__yang_data__dts_test__person__phone__init(phone);
  phone->number = RW_STRDUP("567812340");
  phone->has_type = 1;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;
#endif

  RW_ASSERT(s);
  flags |= RWDTS_XACT_FLAG_TRACE;	/* Individual responses! */
  s->prepare_cb_called++;

  if (!in_subquery) {
    rw_keyspec_path_t *keyspec;

    RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
    RWPB_T_PATHSPEC(DtsTest_data_MultiPerson_MultiPhone) keyspec_m_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_MultiPerson_MultiPhone));

    if (s->multi_router) {
      keyspec = (rw_keyspec_path_t*)&keyspec_m_entry;
    }
    else {
      keyspec = (rw_keyspec_path_t*)&keyspec_entry;
    }

    keyspec_entry.dompath.path001.has_key00 = 1;
    keyspec_entry.dompath.path001.key00.number = RW_STRDUP("5678");

    keyspec_m_entry.dompath.path001.has_key00 = 1;
    keyspec_m_entry.dompath.path001.key00.number = RW_STRDUP("5678");

    rwdts_xact_block_t* block2 = rwdts_xact_block_create(xact);

    rs = rwdts_xact_block_add_query_ks(block2,
				       keyspec,
				       RWDTS_QUERY_READ,
				       flags,
				       1002,
				       NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    TSTPRN("Calling DTS Member prepare ...doing append in member tasklet=%d outgoing query key=%s\n", 
	   ti->tasklet_idx, keyspec_entry.dompath.path001.key00.number);

    rwdts_event_cb_t clnt_cb = { };
    clnt_cb.cb=rwdts_clnt_try_append_callback;
    clnt_cb.ud=ti;

    if (TREATMENT_ACK == s->member[ti->member_idx].treatment) {
      // we respond into this once this block's response is available, save the needed state for that
      ti->async_rsp_state.xact_info = xact_info;	
      ti->async_rsp_state.action = action;
      ti->async_rsp_state.key = NULL;
      rs = rw_keyspec_path_create_dup(key, NULL, &ti->async_rsp_state.key);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      RW_ASSERT(ti->async_rsp_state.key);
      ti->async_rsp_state.msg = msg ? protobuf_c_message_duplicate(NULL, msg, msg->descriptor) : NULL;
      ti->async_rsp_state.block = block2;
    }

    if (s->after) {
      rwdts_xact_block_execute(block2, flags, clnt_cb.cb, ti, NULL);
    } else {
      rwdts_xact_block_execute_immediate(block2, flags, clnt_cb.cb, ti);
    }

    RW_FREE(keyspec_entry.dompath.path001.key00.number);
    RW_FREE(keyspec_m_entry.dompath.path001.key00.number);

    return RWDTS_ACTION_OK;
  }

  TSTPRN("%lu %s In DTS Member prepare ... tasklet=%d\n",
         xact->id.serialno,
         xact->apih->client_path,
         ti->tasklet_idx);

  switch (s->member[ti->member_idx].treatment) {

  case TREATMENT_ACK:
    if (in_subquery) {
      // second-level subquery, answer now, response to this will hit append_callback, above, and trigger the next response up

      TSTPRN("Tasklet=%d prepare callback responding to subquery request for/with phone=%s\n", ti->tasklet_idx, keynum);

      rwdts_member_query_rsp_t rsp;
      RW_ZERO_VARIABLE(&rsp);

      int kct=0;
      for (int i=0; i<TASKLET_MEMBER_CT; i++) {
        if (s->tenv->t[TASKLET_MEMBER_0 + i].async_rsp_state.key) {
          kct++;
        }
      }
      //RW_ASSERT(kct > 0);        // at least one member has sent a subquery, as indicated by it stowing that key in async_rsp_state.key
      rsp.ks = (rw_keyspec_path_t*)key;
      
      rsp.n_msgs = 1;

      RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
      static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
      phone = &phone_instance;
      dts_test__yang_data__dts_test__person__phone__init(phone);
      phone->number = RW_STRDUP(keynum);
      phone->has_type = 1;
      phone->type = DTS_TEST_PHONE_TYPE_HOME;

      RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *phone_m;
      static RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) phone_m_instance = {};
      phone_m = &phone_m_instance;
      dts_test__yang_data__dts_test__multi_person__multi_phone__init(phone_m);
      phone_m->number = RW_STRDUP(keynum);
      phone_m->has_type = 1;
      phone_m->type = DTS_TEST_PHONE_TYPE_HOME;

      if (s->multi_router) {
        rsp.msgs = (ProtobufCMessage**)&phone_m;
      }
      else {
        rsp.msgs = (ProtobufCMessage**)&phone;
      }

      rsp.evtrsp  = RWDTS_EVTRSP_ACK;
      
      rwdts_member_send_response(xact, xact_info->queryh, &rsp);
      protobuf_c_message_free_unpacked_usebody(NULL, &phone_m->base);
      protobuf_c_message_free_unpacked_usebody(NULL, &phone->base);

      return RWDTS_ACTION_OK;

      
    } else {
      // out of block rsp in rwdts_clnt_try_append_callback
    }
    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
}

static void
rwdts_rereg_end(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*       ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
 }

static void rwdts_mem_clnt_transreg_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_mem_clnt_transreg_callback with status = %d\n", xact_status->status);


  if (xact_status->status == RWDTS_XACT_COMMITTED) {
    if (!s->exitnow) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/2);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, when,
                               s->member[0].rwq, &s->exitnow, set_exitnow);
    }
  }
  return;
}

static rwdts_member_rsp_code_t memberapi_second_transreg_prepare(const rwdts_xact_info_t* xact_info,
                                                                 RWDtsQueryAction         action,
                                                                 const rw_keyspec_path_t*      key,
                                                                 const ProtobufCMessage*  msg,
                                                                 uint32_t credits,
                                                                 void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t memberapi_test_transreg_prepare(const rwdts_xact_info_t* xact_info,
                                                             RWDtsQueryAction         action,
                                                             const rw_keyspec_path_t*      key,
                                                             const ProtobufCMessage*  msg,
                                                             uint32_t credits,
                                                             void *getnext_ptr)

{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  uint32_t flags = RWDTS_XACT_FLAG_NOTRAN;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;
  rw_status_t rs;

  RW_ASSERT(apih);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
  rwdts_member_query_rsp_t rsp;

  phone = &phone_instance;

  dts_test__yang_data__dts_test__person__phone__init(phone);
  phone->has_type = 1;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;

  RW_ASSERT(s);

  s->prepare_cb_called++;

  if (!s->use_reg) {
      rw_keyspec_path_t *keyspec;
      keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

      RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

      keyspec = (rw_keyspec_path_t*)&keyspec_entry;

      keyspec_entry.dompath.path001.has_key00 = 1;
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP("5689311890");
      phone->number = RW_STRDUP("5689311890");

      rwdts_xact_block_t* block = rwdts_xact_block_create(xact);

      rs = rwdts_xact_block_add_query_ks(block,
                                    (rw_keyspec_path_t *)keyspec,
                                    RWDTS_QUERY_CREATE,
                                    flags,
                                    1009,
                                    &(phone->base));
      RW_ASSERT(rs == RW_STATUS_SUCCESS);

      rwdts_event_cb_t clnt_cb = { };
      clnt_cb.cb=rwdts_mem_clnt_transreg_callback;
      clnt_cb.ud=ti;

//    rwdts_xact_block_execute(block, flags, clnt_cb.cb, ti, NULL);
      rwdts_xact_block_execute_gi(block, flags, clnt_cb.cb, ti, NULL,NULL);
      s->use_reg = TRUE;
      RW_FREE(keyspec_entry.dompath.path001.key00.number);
  } else {
    if (!s->append_done) {
      rwdts_member_event_cb_t reg_cb;
      RW_ZERO_VARIABLE(&reg_cb);
      reg_cb.ud = xact_info->ud;
      reg_cb.cb.reg_ready_old = rwdts_rereg_end;
      reg_cb.cb.prepare = memberapi_second_transreg_prepare;

      rw_keyspec_path_t* keyspec = NULL;

      RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
      keyspec = (rw_keyspec_path_t*)&keyspec_entry;
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP("5689311890");
      keyspec_entry.dompath.path001.has_key00 = 1;
      rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

      /* Establish a registration */

      rwdts_member_register(xact, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                            RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
      s->append_done = TRUE;
      RW_FREE(keyspec_entry.dompath.path001.key00.number);
    }
  }

  TSTPRN("Calling DTS Member prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:

    rsp.ks = (rw_keyspec_path_t*)key;
    phone->number = RW_STRDUP("9876543210");
    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)&phone;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;

    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    RW_FREE(phone->number);
    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
}

static rwdts_member_rsp_code_t memberapi_test_shared_prepare(const rwdts_xact_info_t* xact_info,
                                                             RWDtsQueryAction         action,
                                                             const rw_keyspec_path_t*      key,
                                                             const ProtobufCMessage*  msg, 
                                                             uint32_t credits,
                                                             void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  RWPB_T_MSG(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category) *filter;
  static RWPB_T_MSG(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category) filter_instance = {};
  rwdts_member_query_rsp_t rsp;

  filter = &filter_instance;
 
  testdts_rw_base__yang_data__testdts_rw_base__logging__filter__category__init(filter);
  filter->name = TESTDTS_RW_BASE_CATEGORY_TYPE_FASTPATH;
  filter->has_name = 1;

  RW_ASSERT(s);

  s->prepare_cb_called++;

fprintf(stderr, "SHARED prepare in member %d\n", ti->member_idx);

  TSTPRN("Calling DTS Member prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:

    rsp.ks = (rw_keyspec_path_t*)key;

    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)&filter;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;

    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);

    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
}

static rwdts_member_rsp_code_t 
memberapi_test_prepare_reroot(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg,
                              uint32_t credits,
                              void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = xact->apih;
  rwdts_match_info_t *match = (rwdts_match_info_t *)xact_info->queryh;
  RW_ASSERT(match);

  RW_ASSERT(apih);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool) *nat_pool;
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *ncontext; 

  uint32_t index = 0;


  RW_ASSERT(s);

  TSTPRN("Calling DTS Member reroot prepare ...\n");

  if (!strcmp(xact->apih->client_path, "/L/RWDTS_GTEST/MEMBER/0")) {
    index = 0;
  } else if (!strcmp(xact->apih->client_path, "/L/RWDTS_GTEST/MEMBER/1")) {
    index = 1;
  } else if (!strcmp(xact->apih->client_path, "/L/RWDTS_GTEST/MEMBER/2")) {
    index = 2;
  }

  if (s->client.reroot_test == REROOT_FIRST) {
    nat_pool = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool) *)msg;
    EXPECT_TRUE(strcmp(nat_pool->name, "NAT-POOL-1") == 0);
  } else if (s->client.reroot_test == REROOT_SECOND) {
    ncontext = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *)msg;
    EXPECT_TRUE(ncontext->interface[0]->interface_type.loopback == 1);
  } else if (s->client.reroot_test == REROOT_THIRD) {
    rwdts_member_registration_t *ip_nat_reg = (rwdts_member_registration_t *)s->ip_nat_regh[index];
    rwdts_member_registration_t *nc_regh = (rwdts_member_registration_t *)s->nc_regh[index];
    if (match->reg->reg_id == ip_nat_reg->reg_id) {
      nat_pool = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool) *)msg;
      EXPECT_TRUE(strcmp(nat_pool->name, "NAT-POOL-1") == 0);
    } else if (match->reg->reg_id == nc_regh->reg_id) {
      ncontext = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *)msg;
      EXPECT_TRUE(strcmp(ncontext->name, "testnccontext") == 0);
    }
  }

  switch (s->member[ti->member_idx].treatment) {
  case TREATMENT_ACK:
  case TREATMENT_NACK:
  case TREATMENT_COMMIT_NACK:
  case TREATMENT_ABORT_NACK:
  case TREATMENT_PRECOMMIT_NACK:

    return RWDTS_ACTION_OK;

  case TREATMENT_NA:
    return RWDTS_ACTION_NA;

  case TREATMENT_ASYNC: {
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_response);
    return RWDTS_ACTION_ASYNC;
  }
  case TREATMENT_BNC:
    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
  // ATTN How to free array
}

#if 0
static rwdts_member_rsp_code_t
publisher_data_prepare(const rwdts_xact_info_t* xact_info,
                       RWDtsQueryAction         action,
                       const rw_keyspec_path_t*      key,
                       const ProtobufCMessage*  msg,
                       uint32_t credits,
                       void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member advice prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  rsp.ks = keyspec;
  EXPECT_EQ(action,RWDTS_QUERY_READ);
  return RWDTS_ACTION_OK; 
}
#endif
static rwdts_member_rsp_code_t 
memberapi_test_advice_prepare(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg, 
                              uint32_t credits,
                              void *getnext_ptr) 
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);

  TSTPRN("Calling DTS Member advice prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rsp.ks = keyspec;

  if (action != RWDTS_QUERY_READ && action != RWDTS_QUERY_DELETE) {

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
    phone = (RWPB_T_MSG(DtsTest_data_Person_Phone) *)msg;
    RW_ASSERT(phone);
    EXPECT_TRUE((strcmp(phone->number, "123-456-789") == 0) ||
              (strcmp(phone->number, "987-654-321") == 0) ||
              (strcmp(phone->number, "978-456-789") == 0) ||
              (strcmp(phone->number, "603-456-789") == 0) ||
              (strcmp(phone->number, "089-123-456") == 0));
  }

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:

    rsp.n_msgs = 0;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);
    s->num_responses++;

    return RWDTS_ACTION_OK;

  case TREATMENT_NA: 
    rsp.evtrsp  = RWDTS_EVTRSP_NA;
    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    s->num_responses++;
    return RWDTS_ACTION_NA;

  case TREATMENT_NACK:
    rsp.evtrsp  = RWDTS_EVTRSP_NACK;
    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    s->num_responses++;
    return RWDTS_ACTION_OK;

  case TREATMENT_ASYNC: {
    rsp.evtrsp  = RWDTS_EVTRSP_ASYNC;
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 10);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_response);
    return RWDTS_ACTION_OK;
  }
  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
  //RW_FREE(array);
}

static void rwdts_set_exit_now(void *ctx)
{
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (s->exit_without_checks) {
    TSTPRN("%d: apih[%p], s[%p], Exiting without checks notif count[%d], exp notif count[%u]\n",
            __LINE__, apih, s, s->notification_count, s->expected_notification_count);
    s->exitnow = 1;
  }

  TSTPRN("%s:%d: apih->stats.num_member_advise_rsp=%d s->expected_advise_rsp_count=%d\n",
	 __PRETTY_FUNCTION__,
	 __LINE__,
	 (int)apih->stats.num_member_advise_rsp,
	 (int)s->expected_advise_rsp_count);

  if (apih->stats.num_member_advise_rsp <  s->expected_advise_rsp_count) {
    rwsched_dispatch_after_f(apih->tasklet,
                             10 * NSEC_PER_SEC,
                             apih->client.rwq,
                             s,
                             rwdts_set_exit_now);
  } else {
    // RW_ASSERT(s->expected_notification_count == s->notification_count);
    TSTPRN("%d: apih[%p], s[%p], Exiting the  pub sub test --"
            " advise rsp[%lu], exp advise rsp[%u],notif count[%d], exp notif count[%u]\n",
            __LINE__, apih, s, 
            apih->stats.num_member_advise_rsp, s->expected_advise_rsp_count,
            s->notification_count, s->expected_notification_count);
    s->exitnow = 1;
  }
  return;
}

static void rwdts_set_member_exit_now(void *ctx)
{
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = s->tenv->t[TASKLET_MEMBER_0].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  if (s->exit_without_checks) {
    TSTPRN("%d: apih[%p], s[%p], Exiting without checks notif count[%d], exp notif count[%u]\n",
            __LINE__, apih, s, s->notification_count, s->expected_notification_count);
    s->exitnow = 1;
  }

  if (apih->stats.num_member_advise_rsp <  s->expected_advise_rsp_count) {
    rwsched_dispatch_after_f(apih->tasklet,
                             10 * NSEC_PER_SEC,
                             s->member[0].rwq,
                             s,
                             rwdts_set_member_exit_now);
  } else {
    // RW_ASSERT(s->expected_notification_count == s->notification_count);
    TSTPRN("%d: apih[%p], s[%p], Exiting the  pub sub test --"
            " advise rsp[%lu], exp advise rsp[%u],notif count[%d], exp notif count[%u]\n",
            __LINE__, apih, s,
            apih->stats.num_member_advise_rsp, s->expected_advise_rsp_count,
            s->notification_count, s->expected_notification_count);

    if (!s->exitnow) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
      rwsched_dispatch_after_f(apih->tasklet, when,
			     s->member[0].rwq, &s->exitnow, set_exitnow);
    }
  }
  return;
}

static void rwdts_pub_set_exit_now(void *ctx)
{
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)s->regh[TASKLET_CLIENT];

  if (reg && (reg->outstanding_requests > 0)) {
    rwsched_dispatch_after_f(apih->tasklet,
                             10 * NSEC_PER_SEC,
                             apih->client.rwq,
                             s,
                             rwdts_pub_set_exit_now);
  } else {
    s->exitnow = 1;
  }

  return;
}

static void rwdts_check_subscribed_data(void *ctx)
{

#define DTS_CHECK_SUBSCRIPTION_MAX_ATTEMPT 15
  unsigned int dts_check_subscription_max_attempt = (RUNNING_ON_VALGRIND)?
      5*DTS_CHECK_SUBSCRIPTION_MAX_ATTEMPT: DTS_CHECK_SUBSCRIPTION_MAX_ATTEMPT;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_status_t rs;

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  s->subscribed_data_attempt ++;

  struct timeval tv;
  gettimeofday(&tv, NULL);

  TSTPRN("%d.%06d taskidx[%d]: subscribed_data_attempt now=%d s->regh[tidx]=%p\n",
	 (int)tv.tv_sec,
	 (int)tv.tv_usec,
	 ti->tasklet_idx,
	 s->subscribed_data_attempt,
	 s->regh[ti->tasklet_idx]);

  ASSERT_LT(s->subscribed_data_attempt, dts_check_subscription_max_attempt);

  RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));
  const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp = NULL;
  char phone_num[] = "123-985-789";

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  kpe.key00.number= phone_num;
  if (s->regh[ti->tasklet_idx]) {
    rs = rwdts_member_reg_handle_get_element(s->regh[ti->tasklet_idx],
                                             gkpe,
                                             NULL,
                                             NULL,
                                             (const ProtobufCMessage**)&tmp);
  }
  if (tmp) {
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    s->pub_data_check_success++;
    TSTPRN("taskidx[%d]: Received the subscribed data of already published content, count[%d], expected_count[%d]\n", 
            ti->tasklet_idx, s->pub_data_check_success, s->expected_pub_data_check_success);
    ASSERT_TRUE(tmp);
    EXPECT_STREQ("123-985-789", tmp->number);
//    rw_status_t rs = rwdts_member_deregister(s->regh[ti->tasklet_idx]);
//   EXPECT_EQ(rs, RW_STATUS_SUCCESS);
//    s->regh[ti->tasklet_idx] = NULL; // Got the expected result from this member
  } else {
    if  (s->subscribed_data_attempt > dts_check_subscription_max_attempt) {
      TSTPRN("Failed maximum attempts [%d] to check subscribed data, expcted - %d: actual -  %d", 
              s->expected_notification_count, s->notification_count, dts_check_subscription_max_attempt);

      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1/10);
      rwsched_dispatch_after_f(apih->tasklet,
                               when,
                               apih->client.rwq,
                               s,
                               rwdts_pub_set_exit_now);

    } else {

      /* Seems to be taking a bit longer than ~1s in total? */
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1);
      rwsched_dispatch_after_f(apih->tasklet,
                               when,
                               apih->client.rwq,
                               ctx,
                               rwdts_check_subscribed_data);
    }
  }

  if (s->expected_pub_data_check_success <= s->pub_data_check_success) {
    TSTPRN("Received expected number[%d] of published content\n", s->expected_pub_data_check_success);
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1/10);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             s,
                             rwdts_pub_set_exit_now);
  } 
  return;
}

static rwdts_member_rsp_code_t 
memberapi_receive_pub_notification(const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,uint32_t credits,
                                   void *getnext_ptr)
{
  RW_ASSERT(xact_info);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  s->notification_count++;
  
  TSTPRN("Received notification from member, count  = %u\n", s->notification_count);
  

  return RWDTS_ACTION_OK;
}

static gboolean
clnt_reg_dereg_check_xpath(rwdts_query_result_t *qresult, char *xpath)
{
 // RWPB_T_MSG(RwDts_Dts_Router)* dts = (RWPB_T_MSG(RwDts_Dts_Router)*)qresult->message;
  RwDts__YangData__RwDts__Dts__Routers *dts = (RwDts__YangData__RwDts__Dts__Routers *)qresult->message;
  size_t j, k;
  gboolean result = FALSE;

  for (j = 0; j < dts->n_member_info; j++) {
    for (k = 0; k < dts->member_info[j]->n_registration; k++) {
      if (dts->member_info[j]->registration[k]->has_keyspec) {
      }
      if (dts->member_info[j]->registration[k]->has_keyspec && 
          !strcmp(dts->member_info[j]->registration[k]->keyspec, xpath)) {
           TSTPRN("%s: member %s keyspec: %s\n", dts->name, dts->member_info[j]->name, dts->member_info[j]->registration[k]->keyspec);
           result = TRUE; 
      }
    }
  }
  return result;
}

static void clnt_reg_dereg_query2_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  char xpath[64];

  RW_ASSERT(s);

  TSTPRN("Calling clnt_reg_dereg_query2_callback  with status = %d\n", xact_status->status);
  if (!(xact_status->xact_done)) {
    TSTPRN("clnt_reg_dereg_query2_callback:xact_done=0\n");
    return;
  }
  ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
  rwdts_query_result_t *qresult =  rwdts_xact_query_result(xact,0);
  ASSERT_TRUE(qresult);
  strcpy(xpath, "D,/ps:person/ps:phone[ps:number='074']");
  ASSERT_FALSE(clnt_reg_dereg_check_xpath(qresult, xpath));
  s->exitnow = 1;
}

static void memberapi_dereg_client_stop_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  //TSTPRN(" Inside memberapi_dereg_client_stop_f again... \n");
  if (s->reg_ready_called == 1) {
    rw_status_t rs = rwdts_member_deregister(s->regh[TASKLET_CLIENT]);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
    s->reg_ready_called = 0;
    goto disp;
  }

  if (clnt_apih->dereg_end == 1) {
    rwdts_xact_t *xact = rwdts_api_query_gi(clnt_apih,
// ATTN : change this to /R/RW.DTSRouter/3 once we implement delete reg for peer router implemented
//                                        "D,/rwdts:dts/rwdts:routers[name='/R/RW.DTSRouter/3']",
                                          "D,/rwdts:dts/rwdts:routers[name='/R/RW.DTSRouter/1']",
                                          RWDTS_QUERY_READ,
                                          0,
                                          clnt_reg_dereg_query2_callback,
                                          ctx,
                                          NULL,
                                          NULL);

    ASSERT_TRUE(xact);
    // python world api. let us explictly un-ref simulating python app
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return;
  }

disp:
  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq, &s->tenv->t[TASKLET_CLIENT],
                           memberapi_dereg_client_stop_f);
  return;
}
  
static rwdts_member_rsp_code_t
memberapi_receive_dereg_notification(const rwdts_xact_info_t* xact_info,
                                     RWDtsQueryAction         action,
                                     const rw_keyspec_path_t*      key,
                                     const ProtobufCMessage*  msg, 
                                     uint32_t credits,
                                     void *getnext_ptr)
{
  RW_ASSERT(xact_info);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  s->notification_count++;
 
  TSTPRN("Received notification from member, count  = %d\n", s->notification_count);

  if (s->expected_notification_count == s->notification_count) {
    TSTPRN("Exiting the  Reg Dereg test ---"
            " expected notification count[%d] == received notification count[%d]\n",
            s->expected_notification_count, s->notification_count);
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_dereg_client_stop_f);    
  }
  return RWDTS_ACTION_OK;
}


void rwdts_gtest_send_query(void *ud)
{
  query_params *params = (query_params *)ud;
  void *ctx = params->ctx;
  rwdts_api_t *clnt_apih = params->clnt_apih;
  rw_keyspec_path_t *keyspec = params->keyspec;
  uint32_t flags = params->flags;
  uint64_t corrid = params->corrid;
  uint64_t corrid1 = params->corrid1;

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_status_t rs;
  rwdts_xact_t* xact = NULL;

  RWPB_T_MSG(DtsTest_Person) *person;
  person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));

  RWPB_F_MSG_INIT(DtsTest_Person,person);

  person->name = strdup("TestName");
  person->has_id = TRUE;
  person->id = 1000;
  person->email = strdup("test@test.com");
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);
  person->phone[0]->number = strdup("123-456-789");
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_MOBILE;

  int num_queries = 1;
  if (s->client.testperf_numqueries)
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    s->client.tv_sec = tv.tv_sec;
    s->client.tv_usec = tv.tv_usec;
    num_queries = s->client.testperf_numqueries;
    TSTPRN(" Start new test for %d queries\n", num_queries);
  }
  for (int i = 0; i < num_queries; i++)
  {
    corrid = corrid+i;

    if (i == 900)
    {
      // Test out of sequence queries
      clnt_apih->tx_serialnum--;
    }
    xact = rwdts_api_xact_create(clnt_apih, flags, rwdts_clnt_query_callback, ctx);
    ASSERT_TRUE(xact);

    rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
    ASSERT_TRUE(blk);
    RW_FREE(person->name);
    person->name = NULL;
    person->name = strdup("TestName123");
    RW_FREE(person->email);
    person->email = NULL;
    person->email = strdup("TestName123@test.com");
    corrid1 = corrid1+i;

    rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
				       flags,corrid1,&(person->base));
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    /* What, why is this true? */
    ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

    rwdts_xact_block_execute(blk, RWDTS_XACT_FLAG_END, 
                             NULL, NULL, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  } 
  protobuf_c_message_free_unpacked(NULL, &person->base);
}
#if 0
static void rwdts_clnt_getnext_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)  
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("CLIENT->Calling %s with status = %d responses_done %d\n",
         __FUNCTION__,
         xact_status->status,
         xact_status->responses_done);

  if (s->client.action == RWDTS_QUERY_READ) 
    {
      rwdts_query_result_t       *qresult; 
      while((qresult = rwdts_xact_query_result(xact,0))) {
	RWPB_T_MSG(DtsTest_Person) *person;
	ASSERT_TRUE(qresult);
	person = (RWPB_T_MSG(DtsTest_Person)*)qresult->message; 
	for (uint32_t i = 0; i < person->n_phone; i++) {
	  TSTPRN(" %u Phone %s \n", 
		 i,
		 person->phone[i]->number);
	}
	// free person?
      }
    }
  else 
    {
      printf("ASSERT not a read\n");
      RW_CRASH();
    }
  if (xact_status->xact_done == FALSE)
    {
      TSTPRN("Transaction not done ...... Continue\n");
      return;
    }
  s->exitnow = 1;
}
#endif

static void rwdts_clnt_query_error (rwdts_xact_t *xact, bool expect_err_report)
{
  char *xpath = NULL;
  char *errstr = NULL;
  rw_status_t cause;
  if (rwdts_xact_get_error_heuristic (xact, 0, &xpath, &cause, &errstr)
      == RW_STATUS_SUCCESS) {
    TSTPRN ("Hueristic error for %s: (%d) %s\n", xpath, cause, errstr);
    RW_FREE(errstr);
  }
  if (xpath) RW_FREE(xpath);
  uint32_t count = rwdts_xact_get_error_count (xact, 0);
  TSTPRN ("Number of errors: %u\n", count);
  /* Print the error report */
  uint32_t err_count = 0;
  rwdts_query_error_t * err = NULL;
  while ((err = rwdts_xact_query_error(xact, 0))) {
    err_count++;
    gchar *xml = rwdts_query_error_get_xml(err);
    if (xml) {
      TSTPRN("Error XML:\n%s\n", xml);
      free(xml);
    }
    RWDtsErrorEntry *ent =
      ( RWDtsErrorEntry *)rwdts_query_error_get_protobuf(err);
    rw_status_t cause = rwdts_xact_get_error_cause(err);
    const char* errstr = rwdts_xact_get_error_errstr(err);
    const char* xpath = rwdts_xact_get_error_xpath(err);
    const char* keystr = rwdts_xact_get_error_keystr(err);
    TSTPRN("Query Error for %s: client %s, xpath %s, keystr %s, cause %d, corrid %lu, queryidx %u, client_idx %u, tasklet %s, msg %s\n",
	   (err->apih->client_path) ? err->apih->client_path : "~NA~",
	   (ent->client_path) ? ent->client_path : "~NA~",
           xpath ? xpath : "~NA~",
           keystr ? keystr : "~NA~",
	   cause,
	   ent->has_corrid ? ent->corrid : 0L,
	   ent->has_queryidx ? ent->queryidx: 0,
	   ent->has_client_idx ? ent->has_client_idx : 0,
	   ent->tasklet_name ? ent->tasklet_name : "~NA~",
	   errstr ? errstr : "~NA~");
    rwdts_query_error_destroy(err);
    err = NULL;
  }

  if (!err_count) {
    if (expect_err_report) {
      TSTPRN("ERROR: Did not find any reports for error, some expected!!\n");
      ASSERT_GT(err_count, 0);
    }
    else {
      TSTPRN("WARNING: Did not find any reports for error\n");
    }
  }
}

static void rwdts_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_query_result_t *qresult = NULL;
 
  RW_ASSERT(s);

  TSTPRN("CLIENT->Calling rwdts_clnt_query_callback with status = %d resp  %d \n", xact_status->status, s->client.testperf_numqueries);
  if (!(xact_status->xact_done)) {
    TSTPRN("CLIENT->xact_done=0\n");
    return;
  }
  if (s->client.testperf_numqueries>1)
  {
    s->client.testperf_numqueries--;
    return;
  }

  /* Got last outstanding query from the previous block(s), now send another burst */
  if (s->client.testperf_itercount)
  {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    TSTPRN("CLIENT->Calling rwdts_clnt_query_callback with status = %d response in %lums at iter = %d\n", xact_status->status, 
           (tv.tv_usec-s->client.tv_usec)/1000+(tv.tv_sec-s->client.tv_sec)*1000,
           s->client.testperf_itercount);
    s->client.testperf_itercount++;
    if (s->client.testperf_itercount<4)
    {
      bool quick = (RUNNING_ON_VALGRIND);
      switch(s->client.testperf_itercount)
      {
        case 1:
          s->client.testperf_numqueries = 500;
          break;
        case 2:
          s->client.testperf_numqueries = quick ? 40 : 1000;
          break;
        case 3:
          s->client.testperf_numqueries = quick ? 20 : 2500;
          break;
        default:
          break;
      };
      RW_ASSERT(s->client.usebuilderapi);
      RW_ASSERT(s->client.perfdata);
      rwdts_gtest_send_query(s->client.perfdata);
      return;
    }
  }

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    TSTPRN("CLIENT->Calling rwdts_clnt_query_callback responses_done is %d\n", xact_status->responses_done);
    if (!xact_status->responses_done) {

      s->keep_alive++;

      TSTPRN("keep_alive = %d\n", s->keep_alive);
      int cnt = rwdts_xact_get_available_results_count(xact, NULL,1000);
      TSTPRN("CLIENT->count = %d KA timer\n", cnt);
      return;
    }
    if (!xact_status->xact_done) {
      TSTPRN("CLIENT->responses_done, but xact_done=0\n");
      return;
    }
    static RWPB_T_MSG(DtsTest_Person)  *person = NULL;

    RW_ASSERT(xact_status->responses_done);
    RW_ASSERT(xact_status->xact_done);
    
    switch (s->client.expect_result) {
    case XACT_RESULT_COMMIT:
      if (s->client.action == RWDTS_QUERY_READ) {
        ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
        uint32_t cnt;
        uint32_t expected_count = 0;
        
        for (int r = 0; r < TASKLET_MEMBER_CT; r++) {
          if (s->member[r].treatment == TREATMENT_ACK) {
            // When not Async we respond with 1 record
            expected_count += 1;
          } else if (s->member[r].treatment == TREATMENT_ASYNC)  {
            // When it is Async we respond with 10 records
            expected_count += 10;
          }
        }

        cnt = rwdts_xact_get_available_results_count(xact, NULL,1000);
        // EXPECT_EQ(cnt, expected_count);
        TSTPRN("CLIENT->count = %d\n", cnt);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,9998);
        // EXPECT_EQ(cnt, expected_count);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,0);
        // EXPECT_EQ(cnt, 2*expected_count);
        UNUSED(cnt);
      } else {
        if (s->client.transactional) {
          ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
        } else {
          if (s->member[1].treatment == TREATMENT_ASYNC_NACK) {
            ASSERT_EQ(xact_status->status, RWDTS_XACT_FAILURE);
          }
          else {
            ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
          }
        }
        qresult =  rwdts_xact_query_result(xact,0);
        if(s->ignore == 0) {
          ASSERT_TRUE(qresult);
          if (qresult) {
            person = (RWPB_T_MSG(DtsTest_Person) *)qresult->message; 
            ASSERT_TRUE(person);
            if (person) {
              EXPECT_STREQ(person->name, "0-RspTestName");
              EXPECT_TRUE(person->has_id);
              EXPECT_EQ(person->id, 2000);
              EXPECT_STREQ(person->email, "0-rsp_test@test.com");
              EXPECT_TRUE(person->has_employed);
              EXPECT_TRUE(person->employed);
              //EXPECT_EQ(person->n_phone, 3);
              ASSERT_TRUE(person->phone);
              EXPECT_STREQ(person->phone[0]->number, "089-123-456");
              EXPECT_TRUE(person->phone[0]->has_type);
              EXPECT_EQ(person->phone[0]->type,DTS_TEST_PHONE_TYPE_HOME);
            }
          }
        }
      }
      break;
    case XACT_RESULT_ABORT:
      ASSERT_EQ(xact_status->status, RWDTS_XACT_ABORTED);
      /* We don't much care about results from abort although there may be some */
      rwdts_clnt_query_error(xact, false);
      break;
    case XACT_RESULT_ERROR:
      ASSERT_EQ(xact_status->status, RWDTS_XACT_FAILURE);
      rwdts_clnt_query_error(xact, false);
      break;
    default:
      ASSERT_TRUE((int)0);
      break;
    }
  }

  if (s->client.perfdata) {
    RW_FREE(s->client.perfdata);
    s->client.perfdata = NULL;
  }
  s->exitnow = 1;
  
  return;
}

static void rwdts_clnt_query_callback_multiple(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)  
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage**  array; 
  RWPB_M_MSG_DECL_INIT(DtsTest_PhoneNumber, phone_instance);
  char ph[32];
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec;

  RW_ASSERT(s);

  TSTPRN("tasklet[%d] in rwdts_clnt_query_callback_multiple with status = %d resp  %d \n", 
	 ti->tasklet_idx, xact_status->status, s->client.testperf_numqueries);
  
  RWPB_T_MSG(DtsTest_PhoneNumber)  *phone = NULL;
  rwdts_query_result_t* qresult = rwdts_xact_query_result(xact,0);
  if(s->ignore == 0) {
    ASSERT_TRUE(qresult);
    if (qresult) {
      phone = (RWPB_T_MSG(DtsTest_PhoneNumber) *) qresult->message;
      ASSERT_TRUE(phone);
      if (phone) {
        switch (ti->tasklet_idx) {
	case TASKLET_MEMBER_0:
	  EXPECT_STREQ(phone->number, "1");
          break;
	case TASKLET_MEMBER_1:
	  EXPECT_STREQ(phone->number, "2");
          break;
	case TASKLET_CLIENT:
	  EXPECT_STREQ(phone->number, "0");
	  break;
	case TASKLET_MEMBER_2:
	default:
	  ASSERT_TRUE(0);
        }
      }

      if (phone) {
	TSTPRN("tasklet[%d] in rwdts_clnt_query_callback_multiple result phone->number='%s'\n", 
	       ti->tasklet_idx, phone->number);
	phone= NULL;
      }
      qresult = rwdts_xact_query_result(xact,0);
      ASSERT_TRUE(qresult == NULL);
    }
    
    if ((ti->member_idx == 0) ||(ti->member_idx == 1)) {
      memset(ph,0,sizeof(ph));
      sprintf(ph,"%d",ti->member_idx);
      
      TSTPRN("tasklet[%d] in rwdts_clnt_query_callback_multiple responding upstream, phone key00='%s'\n", ti->tasklet_idx, ph);
      
      memset(&rsp,0,sizeof(rsp)); 
      keyspec_entry.dompath.path001.has_key00 = 1;
      keyspec_entry.dompath.path001.key00.number = RW_STRDUP(ph);
      keyspec = (rw_keyspec_path_t*) &keyspec_entry;
      rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
      rsp.ks = keyspec;
      RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp.ks));
      array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
      
      phone = &phone_instance;
      phone->number = RW_STRDUP(ph);
      array[0] = &phone->base;
      rsp.n_msgs = 1;
      rsp.msgs = (ProtobufCMessage**)array;
      rsp.evtrsp  = RWDTS_EVTRSP_ACK;
      rwdts_member_send_response(ti->saved_xact_info->xact, ti->saved_xact_info->queryh, &rsp);
      RW_FREE(keyspec_entry.dompath.path001.key00.number);
      RW_FREE(phone->number);
      RW_FREE(array);
    }
    
    if (ti->tasklet_idx == TASKLET_CLIENT ) {
      TSTPRN("tasklet[%d] in rwdts_clnt_query_callback_multiple, setting s->exitnow\n", ti->tasklet_idx);
      s->exitnow = 1;
    }
  }    
  TSTPRN("tasklet[%d] end rwdts_clnt_query_callback_multiple\n", ti->tasklet_idx);
  return;
}

static void rwdts_clnt_query_anycast_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_anycast_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    ASSERT_EQ(s->prepare_cb_called, 1);
  }

  ASSERT_TRUE(xact->xres && xact->xres->dbg && xact->xres->dbg->tr && xact->xres->dbg->tr->n_ent);

  ASSERT_TRUE(xact->xres && xact->xres->dbg && xact->xres->dbg->err_report);
  rwdts_clnt_query_error(xact, true);

  // Call the transaction end
  s->exitnow = 1;

  return;
}

static void rwdts_clnt_query_merge_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc = NULL;

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_merge_callback with status = %d\n", xact_status->status);

  rwdts_query_result_t* qresult = rwdts_xact_query_result(xact,0);

  ASSERT_TRUE(qresult != NULL);
  if (qresult) {
    if (!s->stream_results) {
      nc = ( RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *) qresult->message;
      ASSERT_EQ(nc->n_interface, 1);
      qresult = rwdts_xact_query_result(xact,0);
      ASSERT_TRUE(qresult == NULL);
    }
  }

  if (xact_status->xact_done)
  {
    if (s->stream_results) {
      ASSERT_EQ(3, s->stream_results);
    }
  
    // Call the transaction end
    s->exitnow = 1;
  }
  else {
    ASSERT_TRUE(s->stream_results>0);
  }
  return;
}

static void rwdts_clnt_query_publish_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_publish_callback with status = %d\n", xact_status->status);
 
  rwdts_query_result_t* qresult = rwdts_xact_query_result(xact, 0);
  
  ASSERT_TRUE(qresult != NULL);
  if (qresult) {
    phone = (RWPB_T_MSG(DtsTest_data_Person_Phone) *)qresult->message;
    ASSERT_TRUE(phone != NULL);
    if (phone) {
      EXPECT_TRUE(!strcmp(phone->number, "4523178609"));
      EXPECT_TRUE(phone->has_type);
      EXPECT_EQ(phone->type, DTS_TEST_PHONE_TYPE_MOBILE);
    }
  }
  qresult = rwdts_xact_query_result(xact, 0);
  ASSERT_TRUE(qresult == NULL);
  
  // Call the transaction end

  s->exitnow = 1;

  return;
}

static void rwdts_clnt_query_append_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_append_callback with status=%d tasklet=%d xact_done=%d rsp_done=%d\n",
	 xact_status->status, ti->tasklet_idx, xact_status->xact_done, xact_status->responses_done);

  rwdts_query_result_t *res = NULL;
  int resct = 0;

  while ((res = rwdts_xact_query_result(xact, 999))) {
    resct++;
    if (s->multi_router) {
      const RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *phone_m = NULL;
      phone_m = (const RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *)rwdts_query_result_get_protobuf(res);
      ASSERT_TRUE(phone_m);
      ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_data_MultiPerson_MultiPhone), phone_m->base.descriptor);
      EXPECT_STREQ(phone_m->number, "1234567890");
      EXPECT_EQ(phone_m->type, DTS_TEST_PHONE_TYPE_HOME);
    }
    else {
      const RWPB_T_MSG(DtsTest_data_Person_Phone) *phone = NULL;
      phone = (const RWPB_T_MSG(DtsTest_data_Person_Phone) *)rwdts_query_result_get_protobuf(res);
      ASSERT_TRUE(phone);
      ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone), phone->base.descriptor);
      EXPECT_STREQ(phone->number, "1234567890");
      EXPECT_EQ(phone->type, DTS_TEST_PHONE_TYPE_HOME);
    }
  }
  TSTPRN("Tasklet=%d got %d results to toplevel query 999 via xact handle?\n", ti->tasklet_idx, resct);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  if (xact_status->xact_done) {
    int participants = (s->multi_router)?5:3;
    if (s->client.wildcard) {
      ASSERT_EQ(s->prepare_cb_called,  participants/* orig query */ + (participants*participants) /* for each subquery*/);
    }
    else {
      ASSERT_EQ(s->prepare_cb_called,3);
    }
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, when,
			     s->member[0].rwq, &s->exitnow, set_exitnow);
  }

  return;
}

static void rwdts_clnt_query_shared_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_shared_callback with status = %d\n", xact_status->status);

  if (xact_status->xact_done) {
    ASSERT_TRUE(xact_status->responses_done == TRUE);
    ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
    ASSERT_EQ(s->prepare_cb_called, 2);

    ASSERT_EQ(s->exitnow, 0);
    s->exitnow = TRUE;
  }

  return;
}

static void rwdts_clnt_xpath_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_xpath_query_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    //static RWPB_T_MSG(DtsTest_Person)  *person = NULL;
    //uint32_t n_results = 0;
    
    switch (s->client.expect_result) {
    case XACT_RESULT_COMMIT:
      if (s->client.action == RWDTS_QUERY_READ) {
        uint32_t cnt;
        uint32_t expected_count = 0;
        
        UNUSED(cnt);
        for (int r = 0; r < TASKLET_MEMBER_CT; r++) {
          if (s->member[r].treatment == TREATMENT_ACK) {
            // When not Async we respond with 1 record
            expected_count += 1;
          } else if (s->member[r].treatment == TREATMENT_ASYNC)  {
            // When it is Async we respond with 10 records
            expected_count += 10;
          }
        }

        cnt = rwdts_xact_get_available_results_count(xact, NULL,1000);
        // EXPECT_EQ(cnt, expected_count);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,9998);
        // EXPECT_EQ(cnt, expected_count);
        cnt = rwdts_xact_get_available_results_count(xact, NULL,0);
//        EXPECT_EQ(cnt, 2*expected_count);
        rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);

        while (qrslt) {
          const RWPB_T_MSG(DtsTest_Person)  *person = NULL;
          person = (const RWPB_T_MSG(DtsTest_Person) *)rwdts_query_result_get_protobuf(qrslt);
          ASSERT_TRUE(person);
          ASSERT_EQ(RWPB_G_MSG_PBCMD(DtsTest_Person), person->base.descriptor);
          EXPECT_STREQ(person->name, "0-RspTestName");
          EXPECT_EQ(person->id, 2000);
          EXPECT_STREQ(person->email, "0-rsp_test@test.com");
          const rw_keyspec_path_t *ks = rwdts_query_result_get_keyspec(qrslt);
          ASSERT_TRUE(ks);
          const char* xpath = rwdts_query_result_get_xpath(qrslt);
          ASSERT_TRUE(xpath);
          EXPECT_STREQ(xpath, "/ps:person");
          qrslt = rwdts_xact_query_result(xact, 0);
        }
      }
      break;
    case XACT_RESULT_ABORT:
      ASSERT_EQ(xact_status->status, RWDTS_XACT_ABORTED); // this is only so while we have a monolithic-responding router
      /* We don't much care about results from abort although there may be some */
      break;
    case XACT_RESULT_ERROR:
      ASSERT_EQ(xact_status->status, RWDTS_XACT_FAILURE);
      /* We don't much care about results from a failure although there may be some */
      break;
    default:
      ASSERT_TRUE((int)0);
      break;
    }
  }

  // Call the transaction end
  s->exitnow = 1;
  
  return;
}

static void rwdts_ident_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)  
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("CLIENT->Calling rwdts_ident_clnt_query_callback with status = %d \n", xact_status->status);
  if (!(xact_status->xact_done)) {
    TSTPRN("CLIENT->xact_done=0\n");
    return;
  }
  EXPECT_TRUE(RWDTS_XACT_COMMITTED == xact_status->status);
  s->exitnow = 1;
  
  return;
}
static void rwdts_gtest_xact_abort( void *ctx)
{
  rwdts_xact_abort((rwdts_xact_t *)ctx, RW_STATUS_FAILURE, "ERROR Aborted");
}

static void memberapi_ident_client_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 1;
  keyspec_entry1.dompath.path003.key00.key_four = 0;

  RWPB_T_MSG(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) *msg;

  uint64_t corrid = 1000;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  msg = (RWPB_T_MSG(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour)));

  RWPB_F_MSG_INIT(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour,msg);
  msg->has_key_four = 1;
  msg->key_four = 0;

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_ident_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;
  if (getenv("RWDTS_XACT_FLAG_TRACE")) {
    flags |= RWDTS_XACT_FLAG_TRACE;
  }
  flags |= RWDTS_XACT_FLAG_END;

  rw_status_t rs = rwdts_advise_query_proto_int(clnt_apih,
                                      keyspec1,
                                      NULL, // xact_parent
                                      &(msg->base),
                                      corrid,
                                      NULL, // Debug
                                      RWDTS_QUERY_UPDATE,
                                      &clnt_cb,
                                      flags,
                                      NULL, NULL);

  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  protobuf_c_message_free_unpacked(NULL, &((msg)->base));

  return;
}

static void memberapi_client_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("MemberAPI test client start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rwdts_xact_t* xact = NULL;
  RWPB_T_MSG(DtsTest_Person) *person;
  
  uint64_t corrid = 1000;
 
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  
  ASSERT_TRUE(clnt_apih);
  
  person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));
  
  RWPB_F_MSG_INIT(DtsTest_Person,person);
  
  person->name = strdup("TestName");
  person->has_id = TRUE;
  person->id = 1000;
  person->email = strdup("test@test.com");
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);
  person->phone[0]->number = strdup("123-456-789");
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_MOBILE;
  
  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  if (getenv("RWDTS_XACT_FLAG_TRACE")) {
    flags |= RWDTS_XACT_FLAG_TRACE;
  }

  rw_status_t rs = RW_STATUS_FAILURE;

  if (s->client.usexpathapi) {
    clnt_cb.cb = rwdts_clnt_xpath_query_callback;
    clnt_cb.ud = ctx;
    
    ASSERT_EQ(rwdts_api_get_ypbc_schema(clnt_apih), (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));

    char path[] = "/ps:person";
    xact = rwdts_api_query_gi(clnt_apih, path, RWDTS_QUERY_UPDATE, 
                              flags, clnt_cb.cb, clnt_cb.ud,NULL, 
                              &(person->base));

    ASSERT_TRUE(xact);
    // python world api. let us explictly un-ref simulating python app
    rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    ASSERT_EQ(xact->ref_cnt,2);
    char *xact_id_str;
    xact_id_str = rwdts_xact_get_id(xact);
    ASSERT_TRUE(xact_id_str);
    RW_FREE(xact_id_str);
  } else if (s->client.userednapi) {
    clnt_cb.cb = rwdts_clnt_xpath_query_callback;
    clnt_cb.ud = ctx;

    ASSERT_EQ(rwdts_api_get_ypbc_schema(clnt_apih), (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));

    // char path[] = "/ps:person/ps:phone[ps:number = '123-456-789']/ps:type";
    char path[] = "/ps:person";
    xact = rwdts_api_query_reduction(clnt_apih, path, RWDTS_QUERY_UPDATE,
                                     flags, clnt_cb.cb, clnt_cb.ud, &(person->base));

    ASSERT_TRUE(xact);
    char *xact_id_str;
    xact_id_str = rwdts_xact_get_id(xact);
    ASSERT_TRUE(xact_id_str);
    RW_FREE(xact_id_str);
  } else  if (s->client.usebuilderapi) {
    uint64_t corrid1 = 9998;
    uint64_t corrid = 999;
    query_params *params = (query_params *)malloc(sizeof(query_params));
    params->ctx = ctx;
    params->clnt_apih= clnt_apih;
    params->keyspec = keyspec;
    params->flags = flags;
    params->corrid = corrid;
    params->corrid1 = corrid1;
    s->client.perfdata = params;
    RW_ASSERT(s->client.perfdata);
    rwdts_gtest_send_query(s->client.perfdata);
  } else {
    if (clnt_cb.cb == rwdts_clnt_query_callback) {
      RW_ASSERT(!s->client.testperf_itercount); // else will also expects s->client.perfdata
      RW_ASSERT(!s->client.perfdata);
    }
    flags |= RWDTS_XACT_FLAG_END;// xact_parent = NULL
    flags |= RWDTS_XACT_FLAG_ADVISE;

    xact = rwdts_api_xact_create(clnt_apih, flags,
                                clnt_cb.cb, clnt_cb.ud);

    ASSERT_TRUE(xact);
    rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
    ASSERT_TRUE(blk);
    rs = rwdts_xact_block_add_query_ks(blk, keyspec,
                                       s->client.action,
                                       flags, corrid,
                                       &(person->base));
    rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
    ASSERT_EQ (rs, RW_STATUS_SUCCESS);
  }
  protobuf_c_message_free_unpacked(NULL, &((person)->base));
  if (s->client.client_abort)
  {
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 2*NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             rwsched_dispatch_get_global_queue(s->tenv->t[TASKLET_CLIENT].tasklet, DISPATCH_QUEUE_PRIORITY_DEFAULT),
                             xact,
                             rwdts_gtest_xact_abort);
  }

  return;
}

static void memberapi_client_start_multiple_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("MemberAPI test client start...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t *)&keyspec_entry;
  RWPB_M_MSG_DECL_INIT(DtsTest_PhoneNumber, phone_instance);

  rwdts_xact_t* xact = NULL;
  
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  
  ASSERT_TRUE(clnt_apih);
  
  
  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback_multiple;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("0");
  keyspec_entry.dompath.path001.has_key00 = 1;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  TSTPRN("tasklet[%d] in memberapi_client_start_multiple_f sending READ(key00.number='%s'), %s\n", 
	 ti->tasklet_idx, keyspec_entry.dompath.path001.key00.number,
	 ((flags&RWDTS_XACT_FLAG_NOTRAN) ? "NONtransactional" : "transactional"));

  xact = rwdts_api_query_ks(clnt_apih,
                            keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            clnt_cb.cb,
                            (gpointer)clnt_cb.ud,
                            &(phone_instance.base));
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  keyspec_entry.dompath.path001.key00.number = NULL;
  keyspec_entry.dompath.path001.has_key00 = 0;
  ASSERT_TRUE(xact);
  if (s->client.client_abort)
  {
    TSTPRN("tasklet[%d] in memberapi_client_start_multiple_f arranging abort in 2s\n",
	   ti->tasklet_idx);
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 2*NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             rwsched_dispatch_get_global_queue(s->tenv->t[TASKLET_CLIENT].tasklet, DISPATCH_QUEUE_PRIORITY_DEFAULT),
                             xact,
                             rwdts_gtest_xact_abort);
  }

  return;
}

static void memberapi_client_anycast_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client anycast start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu)); 

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu)); 

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path002.has_key00 = 1;
  keyspec_entry.dompath.path002.key00.id = 1234;

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  cpu = (RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)));
  RWPB_F_MSG_INIT(TestdtsRwBase_VcsResource_Vm_Cpu, cpu);

  uint32_t flags = 0;
  flags |= RWDTS_XACT_FLAG_TRACE;

  fprintf(stderr, "[ Expect trace output here: ]\n");
  xact = rwdts_api_query_ks(clnt_apih,
                            keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            rwdts_clnt_query_anycast_callback,
                            ctx,
                            &cpu->base);
  ASSERT_TRUE(xact);
  ASSERT_TRUE(xact->trace);

  RW_FREE(cpu);
  return;
}

static void memberapi_client_merge_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  TSTPRN("MemberAPI test client merge start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc;
  nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, nc);

  uint32_t flags = 0;
  if (s->stream_results) {
    flags |= RWDTS_XACT_FLAG_STREAM;
  } else {
    flags |= RWDTS_XACT_FLAG_BLOCK_MERGE;
  }

  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_READ,
                          flags,
                          rwdts_clnt_query_merge_callback,
                          ctx,
                          NULL);
  ASSERT_TRUE(xact!=NULL);

  protobuf_c_message_free_unpacked(NULL, &nc->base);
  return;
}

static rwdts_member_rsp_code_t
memberapi_test_empty_prep(const rwdts_xact_info_t* xact_info,
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t*      key,
                           const ProtobufCMessage*  msg,
                           uint32_t credits,
                           void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));

  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rw_keyspec_path_t* keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwStats_Routerstats));

  RWPB_T_MSG(TestdtsRwStats_Routerstats) *stats;
  stats = (RWPB_T_MSG(TestdtsRwStats_Routerstats)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwStats_Routerstats)));
  RWPB_F_MSG_INIT(TestdtsRwStats_Routerstats, stats);

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs =(ProtobufCMessage**)&stats;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  protobuf_c_message_free_unpacked(NULL, &(stats->base));
  return RWDTS_ACTION_OK;
}

static void rwdts_clnt_query_refcnt_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (xact_status->xact_done)
  {
    s->exitnow = 1;
  }
}

static void rwdts_clnt_query_empty_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RWPB_T_MSG(TestdtsRwStats_Routerstats) *stats = NULL;
  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_empty_callback with status = %d\n", xact_status->status);


  if (xact_status->xact_done)
  {

    rwdts_query_result_t* qresult = rwdts_xact_query_result(xact,0);
    ASSERT_TRUE(qresult != NULL);
    if (qresult) {
      stats = ( RWPB_T_MSG(TestdtsRwStats_Routerstats) *) qresult->message;
      EXPECT_TRUE(stats != NULL);
      if (stats) {
        EXPECT_EQ(stats->has_topx_begin, 0);
        EXPECT_EQ(stats->has_topx_count_5s, 0);
        EXPECT_EQ(stats->has_topx_latency_5s, 0);
      }
      qresult = rwdts_xact_query_result(xact,0);
      ASSERT_TRUE(qresult == NULL);
      s->exitnow = 1;
    }
  }
  return;
}

static void memberapi_client_empty_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client empty start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwStats_Routerstats));

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  uint32_t flags = 0;
  if (s->stream_results == TRUE) {
    flags |= RWDTS_XACT_FLAG_BLOCK_MERGE;
    flags |= RWDTS_XACT_FLAG_STREAM;
  }

  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_READ,
                          flags,
                          rwdts_clnt_query_empty_callback,
                          ctx,
                          NULL);
  ASSERT_TRUE(xact!=NULL);
  return;
}

static void
memberapi_client_refcnt_regready_f(rwdts_member_reg_handle_t regh,
                              const rw_keyspec_path_t*  ks,
                              const ProtobufCMessage*   msg,
                              void*                     ctx);

static void memberapi_client_refcnt_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t reg_cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client refcnt start...\n");

  rw_keyspec_path_t *keyspec;
  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_empty_prep;

  
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "refcntcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "refcntcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);


  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  reg_cb.cb.reg_ready_old = memberapi_client_refcnt_regready_f;
  rwdts_memberapi_register(NULL, apih, keyspec, &reg_cb, 
            RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_SHARED, NULL);


}

static void
gtest_rwdts_member_registration_delete_async( void *regh)
{
  rwdts_member_registration_delete((rwdts_member_registration_t *) regh);
}

static void
memberapi_client_refcnt_regready_f(rwdts_member_reg_handle_t regh,
                              const rw_keyspec_path_t*  ks,
                              const ProtobufCMessage*   msg,
                              void*                     ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rw_status_t rs;
   rwdts_xact_block_t *blk;
  TSTPRN("MemberAPI test client expand start...\n");

  rw_keyspec_path_t *keyspec;
  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "refcntcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "refcntcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  uint32_t flags = 0;
  if (getenv("RWDTS_XACT_FLAG_TRACE")) {
    flags |= RWDTS_XACT_FLAG_TRACE;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE ;

  xact = rwdts_api_xact_create(clnt_apih,
             flags,
             rwdts_clnt_query_refcnt_callback,
             ctx);
  ASSERT_TRUE(xact != NULL);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;
  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "refcntcontext", sizeof(nc->name)-1);
  nc->has_name = 1;
  nc->n_interface = 1;
  nc->interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)**)
   RW_MALLOC0(nc->n_interface*sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*));
  nc->interface[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[0]);
  strncpy(nc->interface[0]->name, "Interface2", sizeof(nc->interface[0]->name)-1);
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->interface_type.has_loopback = 1;

  for (int j=0; j<2; j++) {
    blk = rwdts_xact_block_create(xact);
    ASSERT_TRUE(blk!= NULL);
    sprintf(nc->interface[0]->name, "Interface:%d", j);
    nc->interface[0]->has_name = 1;
    rs = rwdts_xact_block_add_query_ks(blk,
                                keyspec,
                                j?RWDTS_QUERY_UPDATE:RWDTS_QUERY_CREATE,
                                RWDTS_XACT_FLAG_ADVISE,
                                1000+j,
                                &nc->base);
   ASSERT_EQ(rs, RW_STATUS_SUCCESS);
   } 

  rs =rwdts_xact_block_execute(blk, 
                RWDTS_XACT_FLAG_END|RWDTS_XACT_FLAG_ADVISE,NULL, NULL, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs = rwdts_xact_commit(xact);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  protobuf_c_message_free_unpacked_usebody(NULL, &nc->base);
  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq,
                           regh,
                           gtest_rwdts_member_registration_delete_async);

  return;
}


void 
rwdts_audit_done(rwdts_api_t*                apih,
                 rwdts_member_reg_handle_t   regh, 
                 const rwdts_audit_result_t* audit_result, 
                 const void*                 ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  ASSERT_TRUE(reg);
  rwdts_audit_t *audit = &reg->audit;
  ASSERT_TRUE(reg);
  
  
  TSTPRN("rwdts_audit_done() called\n");

  // Peek into the audit structure to make sure things happened as 
  // we expected 

  switch (s->audit_test) {
    case RW_DTS_AUDIT_TEST_RECONCILE_SUCCESS:
    case RW_DTS_AUDIT_TEST_RECOVERY:
      EXPECT_EQ(audit->audit_state, RW_DTS_AUDIT_STATE_COMPLETED);
      EXPECT_EQ(audit->audit_status, RW_DTS_AUDIT_STATUS_SUCCESS);
#if RWDTS_USE_POPULATE_CACHE
      EXPECT_EQ(audit->audit_stats.num_audits_started, 1);
      EXPECT_EQ(audit->audit_stats.num_audits_succeeded, 1);
      EXPECT_EQ(audit->fetch_attempts, 2);
      EXPECT_EQ(audit->objs_missing_in_dts, 0);
      EXPECT_EQ(audit->objs_missing_in_cache, 0);
      EXPECT_EQ(audit->objs_matched, 6);
#else
      EXPECT_EQ(audit->audit_stats.num_audits_started, 2);
      EXPECT_EQ(audit->audit_stats.num_audits_succeeded, 2);
      EXPECT_EQ(audit->fetch_attempts, 2);
      EXPECT_EQ(audit->objs_missing_in_dts, 0);
      EXPECT_EQ(audit->objs_missing_in_cache, 3);
      EXPECT_EQ(audit->objs_matched, 9);
#endif /* RWDTS_USE_POPULATE_CACHE */
      break;
    case RW_DTS_AUDIT_TEST_REPORT:
      EXPECT_EQ(audit->audit_state, RW_DTS_AUDIT_STATE_COMPLETED);
      EXPECT_EQ(audit->audit_status, RW_DTS_AUDIT_STATUS_SUCCESS);
#if RWDTS_USE_POPULATE_CACHE
      EXPECT_EQ(audit->audit_stats.num_audits_started, 1);
      EXPECT_EQ(audit->audit_stats.num_audits_succeeded, 1);
      EXPECT_EQ(audit->fetch_attempts, 1);
      EXPECT_EQ(audit->objs_missing_in_dts, 0);
      EXPECT_EQ(audit->objs_missing_in_cache, 0);
      EXPECT_EQ(audit->objs_matched, 3);
#else
      EXPECT_EQ(audit->audit_stats.num_audits_started, 2);
      EXPECT_EQ(audit->audit_stats.num_audits_succeeded, 2);
      EXPECT_EQ(audit->fetch_attempts, 1);
      EXPECT_EQ(audit->objs_missing_in_dts, 0);
      EXPECT_EQ(audit->objs_missing_in_cache, 0);
      EXPECT_EQ(audit->objs_matched, 3);
#endif /* RWDTS_USE_POPULATE_CACHE */
      break;

    case RW_DTS_AUDIT_TEST_FAIL_CONVERGE:
#if RWDTS_USE_POPULATE_CACHE
      EXPECT_EQ(audit->audit_state, RW_DTS_AUDIT_STATE_COMPLETED);
      EXPECT_EQ(audit->audit_status, RW_DTS_AUDIT_STATUS_SUCCESS);
#else
      EXPECT_EQ(audit->audit_state, RW_DTS_AUDIT_STATE_FAILED);
      EXPECT_EQ(audit->audit_status, RW_DTS_AUDIT_STATUS_FAILURE);
      EXPECT_EQ(audit->fetch_attempts, 16);
      EXPECT_EQ(audit->objs_missing_in_dts, 3);
      EXPECT_EQ(audit->objs_missing_in_cache, 48);
      EXPECT_EQ(audit->objs_matched, 0);
#endif /* RWDTS_USE_POPULATE_CACHE */
      break;

    default:
      RW_CRASH();
      break;
  }

  s->exitnow  = 1;
  return;
}

static void memberapi_client_publish_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("MemberAPI test client expand start...\n");
  rw_keyspec_path_t *keyspec;

  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  rwdts_xact_t* xact = NULL;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("4523178609");
  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  phone = (RWPB_T_MSG(DtsTest_data_Person_Phone)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_data_Person_Phone)));
  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone, phone);

  uint32_t flags = 0;
  if (s->stream_results == TRUE) {
    flags |= RWDTS_XACT_FLAG_STREAM;
  } else {
    flags |= RWDTS_XACT_FLAG_BLOCK_MERGE;
  }
  xact = rwdts_api_query_ks(clnt_apih,
                            keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            rwdts_clnt_query_publish_callback,
                            ctx,
                            NULL); 
  ASSERT_TRUE(xact);
  char path[] = "/ps:person/ps:phone[ps:number='4523178609']";
  rw_status_t rs = rwdts_api_trace(clnt_apih,path);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  RW_FREE(phone); 
  return;
}


typedef struct {
  int _what;
} myapp_audit_scratch_t;

static void*
memberapi_client_audit_xact_init(rwdts_appconf_t* ac,
                                 rwdts_xact_t*    xact,
                                 void*            ud)
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp);
  RW_ASSERT(myapp->s);
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  struct tenv1::tenv_info *ti = myapp->ti;
  RW_ASSERT(ti);
  ti->cb.xact_init++;

  is_test_done(myapp->s);
  TSTPRN("myapp_conf1_xact_init[%d] called, xact_init[%d]\n", 
          myapp->ti->member_idx, ti->cb.xact_init);
  

  return RW_MALLOC0_TYPE(sizeof(myapp_audit_scratch_t), myapp_audit_scratch_t);
}

static void 
memberapi_client_audit_xact_deinit(rwdts_appconf_t* ac,
                                   rwdts_xact_t*    xact,
                                   void*            ud,
                                   void*            scratch_in)
{
  myappconf_t *myapp = (myappconf_t *)ud;

  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.xact_deinit++;

  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  TSTPRN("memberapi_client_audit_xact_deinit[%d] called, xact_deinit[%d]\n", 
          myapp->ti->member_idx, ti->cb.xact_deinit);
  RW_FREE_TYPE(scratch, myapp_audit_scratch_t);
  
  return;
}

static void
memberapi_client_audit_config_validate(rwdts_api_t*     apih,
                                       rwdts_appconf_t* ac,
                                       rwdts_xact_t*    xact,
                                       void*            ctx,
                                       void*            scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t*)ctx;

  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.validate++;

  TSTPRN("memberapi_client_audit_config_validate() called, validate[%d]\n", 
          ti->cb.validate);
  return;
}

static void
memberapi_client_audit_config_apply(rwdts_api_t*           apih,
                                    rwdts_appconf_t*       ac,
                                    rwdts_xact_t*          xact,
                                    rwdts_appconf_action_t action,
                                    void*                  ctx,
                                    void*                  scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t *)ctx;
  //myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;

  ASSERT_TRUE(apih);
  ASSERT_TRUE(ac);
  ASSERT_TRUE(myapp);

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.apply++;

  TSTPRN("memberapi_client_audit_config_apply() called apply[%d]\n",
          ti->cb.apply);
 
  if (action == RWDTS_APPCONF_ACTION_RECONCILE) {
    TSTPRN("memberapi_client_audit_config_apply() called with action RWDTS_APPCONF_ACTION_RECONCILE\n");
  }

  return;
}


static void
client_begin_audit_test(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s  = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("client_begin_audit_test called\n");
  // Check if the registrations audit is done

  if (RWDTS_AUDIT_IS_IN_PROGRESS(&((rwdts_member_registration_t*)s->regh[ti->tasklet_idx])->audit)) {
    s->recover_audit_attempts++;
    if (s->recover_audit_attempts < TEST_MAX_RECOVER_AUDIT) {
      TSTPRN("Recovery Audit still in progress -- retrying - count  %d stil under limit %d",
              s->recover_audit_attempts, TEST_MAX_RECOVER_AUDIT);
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC);
      rwdts_api_t *apih = ((rwdts_member_registration_t*)s->regh[ti->tasklet_idx])->apih;
      RW_ASSERT_TYPE(apih, rwdts_api_t);
      rwsched_dispatch_after_f(apih->tasklet,
                               when,
                               apih->client.rwq,
                               ctx,
                               client_begin_audit_test);
    } else {
      EXPECT_FALSE(0);
      s->exitnow = 1;
    }
  }
  rw_status_t rs = rwdts_member_reg_handle_audit(s->regh[ti->tasklet_idx], 
                                                 s->audit_action, 
                                                 rwdts_audit_done, 
                                                 ctx);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
}

static void
delayed_rwdts_appconf_phase_complete(void *param)
{
  rwdts_appconf_t *appcnf = (rwdts_appconf_t *) param;
  rwdts_appconf_phase_complete(appcnf, RWDTS_APPCONF_PHASE_REGISTER);
}

static void 
memberapi_client_audit_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s  = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("client  () memberapi_test_audit_member_read_client_audit_start_f() called\n");
  TSTPRN("MemberAPI test client audit start ...\n");

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry = 
    (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

#if 0
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc;
  nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, nc);
#endif

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = memberapi_client_audit_xact_init;
  cbset.xact_deinit = memberapi_client_audit_xact_deinit;
  cbset.config_validate = memberapi_client_audit_config_validate;
  cbset.config_apply = memberapi_client_audit_config_apply;
  cbset.ctx = myapp;
  rw_status_t status;


  status = rwdts_api_appconf_group_create(ti->apih, NULL,
                                        cbset.xact_init,cbset.xact_deinit,
                                        cbset.config_validate,
                                        cbset.config_apply,
                                        cbset.ctx,
                                        &myapp->conf1.ac);

  ASSERT_EQ(status,RW_STATUS_SUCCESS);


  char xpath[] = "/tdtsbase:colony/tdtsbase:network-context";
  status = rwdts_appconf_register_xpath_gi(myapp->conf1.ac,
               xpath,0,NULL,NULL,NULL, &myapp->conf1.reg_nmap);

  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(&myapp->conf1.reg_nmap);
  s->regh[ti->tasklet_idx] = myapp->conf1.reg_nmap;

  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC);
  rwdts_api_t *apih = ((rwdts_member_registration_t*)s->regh[ti->tasklet_idx])->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           when,
                           apih->client.rwq,
                           ctx,
                           client_begin_audit_test);

  /* Say we're finished after a small delay for this test case */
  when = dispatch_time(DISPATCH_TIME_NOW, .1 * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           when,
                           apih->client.rwq,
                           myapp->conf1.ac,
                           delayed_rwdts_appconf_phase_complete);

//rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);

  return;
}

static void memberapi_client_append_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  TSTPRN("MemberAPI test client append start...\n");

  rw_keyspec_path_t *keyspec;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  RWPB_T_PATHSPEC(DtsTest_data_MultiPerson_MultiPhone) keyspec_m_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_MultiPerson_MultiPhone));
  if (s->multi_router) {
    keyspec = (rw_keyspec_path_t*)&keyspec_m_entry;
  }
  else {
    keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  }
  keyspec_m_entry.dompath.path001.has_key00 = 1;
  keyspec_m_entry.dompath.path001.key00.number = RW_STRDUP("1234567890");
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("1234567890");


  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }


  RW_ASSERT(venv_g.checked);
  if (venv_g.val) {
    flags |= RWDTS_XACT_FLAG_TRACE;
  }


  uint64_t corrid = 999;

  xact = rwdts_api_xact_create(clnt_apih,
			       flags|RWDTS_XACT_FLAG_TRACE,
			       rwdts_clnt_query_append_callback,
			       ctx);
  RW_ASSERT(xact);
  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  RW_ASSERT(blk);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  phone = (RWPB_T_MSG(DtsTest_data_Person_Phone)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_data_Person_Phone)));
  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone, phone);

  RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone) *phone_m;
  phone_m = (RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_data_MultiPerson_MultiPhone)));
  RWPB_F_MSG_INIT(DtsTest_data_MultiPerson_MultiPhone, phone_m);

  if (s->multi_router) {
    rwdts_xact_block_add_query_ks(blk,
                                  keyspec,
                                  RWDTS_QUERY_READ,
                                  RWDTS_XACT_FLAG_TRACE,
                                  corrid,
                                  &phone_m->base);
  }
  else {
    rwdts_xact_block_add_query_ks(blk,
                                  keyspec,
                                  RWDTS_QUERY_READ,
                                  RWDTS_XACT_FLAG_TRACE,
                                  corrid,
                                  &phone->base);
  }
  rwdts_xact_block_execute(blk,
			   RWDTS_XACT_FLAG_END,
			   NULL,
			   NULL,
			   NULL);

  ASSERT_TRUE(xact);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  RW_FREE(keyspec_m_entry.dompath.path001.key00.number);
  RW_FREE(phone);
  RW_FREE(phone_m);
  return;
}

static void rwdts_clnt_query_transreg_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_transreg_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
  }

  return;
}

static void memberapi_client_transreg_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  TSTPRN("MemberAPI test client expand start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("9876543210");

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  phone = (RWPB_T_MSG(DtsTest_data_Person_Phone)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_data_Person_Phone)));
  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone, phone);
  phone->number = RW_STRDUP("9876543210");

  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }

  xact = rwdts_api_query_ks(clnt_apih,keyspec,RWDTS_QUERY_UPDATE,flags,
                rwdts_clnt_query_transreg_callback,ctx,&phone->base);
  ASSERT_TRUE(xact);

  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  RW_FREE(phone->number);
  RW_FREE(phone);
  return;
}

static void memberapi_member_shared_reg_ready(rwdts_member_reg_handle_t regh,
					      rw_status_t               rs,
					      void*                     ctx)
{
  /* This is a registration ready, wait until all are ready then query */

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;

  if (rs == RW_STATUS_SUCCESS) {
    s->regct++;
  }

  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct < TASKLET_MEMBER_CT) {
    return;
  }
  /* All regs get reg ready. this need to be changed to allow remove of failed one */
  TSTPRN("s->regct == %d\n", s->regct);
  RW_ASSERT(s->regct == 3);

  /* Technically we are now executing client code in some random
     member's tasklet context here.  This is immaterial in these
     gtests so long as we issue the query using the correct dts
     apih. */

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  TSTPRN("MemberAPI test client shared start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category));

  RWPB_T_PATHSPEC(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path002.has_key00 = 1;
  keyspec_entry.dompath.path002.key00.name = TESTDTS_RW_BASE_CATEGORY_TYPE_FASTPATH;

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category) *filter;
  filter = (RWPB_T_MSG(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category)));
  RWPB_F_MSG_INIT(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category, filter);

  uint32_t flags = 0;

  xact = rwdts_api_query_ks(clnt_apih,keyspec,RWDTS_QUERY_READ,flags,
                rwdts_clnt_query_shared_callback,ctx,&filter->base);
  ASSERT_TRUE(xact);
  return;
}

static void memberapi_client_start_reroot_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  uint64_t corrid = 1000;

  TSTPRN("MemberAPI test client reroot start...\n");

  rw_keyspec_path_t *keyspec;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip) keyspec_entry;
  keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip) *ip;
  ip = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip) *)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip,ip);
  ip->n_nat_pool = 1;
  strcpy(&ip->nat_pool[0].name[0], "NAT-POOL-1");
  ip->nat_pool[0].has_name = 1;
  ip->nat_pool[0].has_prefix = 1;
  strcpy(&ip->nat_pool[0].prefix[0], "10.10.10.10");

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  rw_status_t rs = RW_STATUS_FAILURE;

  if (!s->client.usebuilderapi) {
    flags |= RWDTS_XACT_FLAG_END;// xact_parent = NULL
    rs = rwdts_advise_query_proto_int(clnt_apih,
                                      keyspec,
                                      NULL, // xact_parent
                                      &(ip->base),
                                      corrid,
                                      NULL, // Debug
                                      RWDTS_QUERY_UPDATE,
                                      &clnt_cb,
                                      flags,
                                      NULL, NULL);
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  } else {
    uint64_t corrid = 999;

    xact = rwdts_api_xact_create(clnt_apih, flags, rwdts_clnt_query_callback, ctx);
    ASSERT_TRUE(xact);
    rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
    ASSERT_TRUE(blk);
    rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
				       flags,corrid,&(ip->base));
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


    strcpy(&ip->nat_pool[0].name[0], "NAT-POOL-2");
    strcpy(&ip->nat_pool[0].prefix[0], "11.11.11.11");
    uint64_t corrid1 = 9998;

    rs = rwdts_xact_block_add_query_ks(blk, keyspec, s->client.action, flags, corrid1, &(ip->base));
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

    rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    rs = rwdts_xact_commit(xact);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }
  RW_FREE(ip);

  return;
}

static void memberapi_client_start_expand_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  uint64_t corrid = 1000;

  TSTPRN("MemberAPI test client expand start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "testncinterface", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *interface;
  interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,interface);

  strcpy(interface->name, "testncinterface");
  interface->has_name = 1;
  interface->n_ip = 1;
  interface->has_interface_type = 1;
  interface->interface_type.has_loopback = 1;
  interface->interface_type.loopback = 1;
  strcpy(&interface->ip[0].address[0], "10.10.10.10");
  interface->ip[0].has_address = 1;

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  rw_status_t rs = RW_STATUS_FAILURE;

  if (!s->client.usebuilderapi) {
    flags |= RWDTS_XACT_FLAG_END; //xact_parent = NULL
    rs = rwdts_advise_query_proto_int(clnt_apih,
                                      keyspec,
                                      NULL, // xact_parent
                                      &(interface->base),
                                      corrid,
                                      NULL, // Debug
                                      RWDTS_QUERY_UPDATE,
                                      &clnt_cb,
                                      flags,
                                      NULL, NULL);
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  } else {
    uint64_t corrid = 999;

    xact = rwdts_api_xact_create(clnt_apih, flags, rwdts_clnt_query_callback, ctx);
   ASSERT_TRUE(xact);
   rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
   ASSERT_TRUE(blk);
   rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
                      flags,corrid,&(interface->base));
   ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    uint64_t corrid1 = 9998;

    rs = rwdts_xact_block_add_query_ks(blk, keyspec, s->client.action, flags, corrid1, &(interface->base));
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

    rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    rs = rwdts_xact_commit(xact);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }
  //protobuf_c_message_free_unpacked(NULL, &((person)->base));

  return;
}

static void clnt_reg_dereg_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  char xpath[64];
  RW_ASSERT(s);

  TSTPRN("Calling clnt_reg_dereg_query_callback  with status = %d\n", xact_status->status);
  if (!(xact_status->xact_done)) {
    TSTPRN("clnt_reg_dereg_query_callback:xact_done=0\n");
    return;
  }
  ASSERT_EQ(xact_status->status, RWDTS_XACT_COMMITTED);
  rwdts_query_result_t *qresult =  rwdts_xact_query_result(xact,0);
  ASSERT_TRUE(qresult);
  strcpy(xpath, "D,/ps:person/ps:phone[ps:number='074']");
  ASSERT_TRUE(clnt_reg_dereg_check_xpath(qresult, xpath));
  s->reg_ready_called = 1;
}

static void
memberapi_reg_dreg_ready(rwdts_member_reg_handle_t regh,
                         rw_status_t               rs,
                         void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  char phone_num1[] = "074";
  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  kpe.key00.number= phone_num1;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num1;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  rw_status_t status = 
       rwdts_member_reg_handle_create_element(regh,
                                              gkpe,
                                              &phone->base,
                                              NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, status);
  rwdts_xact_t *xact = rwdts_api_query_gi(clnt_apih, 
// TBD: Attn change this once dereg implemented properly for peer rtrs 
//                                        "D,/rwdts:dts/rwdts:routers[rwdts:name='/R/RW.DTSRouter/3']",
                                          "D,/rwdts:dts/rwdts:routers[rwdts:name='/R/RW.DTSRouter/1']",
                                          RWDTS_QUERY_READ, 
                                          0, 
                                          clnt_reg_dereg_query_callback, 
                                          ctx, 
                                          NULL, 
                                          NULL); 

  ASSERT_TRUE(xact);
  // python world api. let us explictly un-ref simulating python app
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);

  return;
}

static void memberapi_reg_dereg_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = memberapi_reg_dreg_ready;

  TSTPRN("starting client for Member Reg Dereg notify ...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec_entry.dompath.path001.has_key00 = 1;
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("074");

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  rwdts_member_deregister_all(clnt_apih);

  regh = s->regh[TASKLET_CLIENT] = rwdts_memberapi_register(NULL, clnt_apih,
                                  keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  ASSERT_TRUE(regh);
  clnt_apih->dereg_end = 0;
  return;
}

static void memberapi_pub_sub_cache_kn_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  // cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub sub Cache KN notify ...\n");

  rwdts_api_set_ypbc_schema(ti->apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  rwdts_member_deregister_all(clnt_apih);

  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_memberapi_register(NULL, clnt_apih,
                                  keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);

  RW_ASSERT(regh != NULL);
  {

    char phone_num1[] = "123-456-789";
    char phone_num2[] = "987-654-321";

    RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) *k_keyspec = 0;
    rw_status_t rs = rw_keyspec_path_create_dup((rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone)) , NULL, (rw_keyspec_path_t **)&k_keyspec);
    rw_keyspec_path_set_category((rw_keyspec_path_t*)k_keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
    EXPECT_EQ (rs, RW_STATUS_SUCCESS);

    k_keyspec->dompath.path001.has_key00 = 1;
    #pragma GCC diagnostic ignored "-Wformat-security"
    sprintf (k_keyspec->dompath.path001.key00.number, phone_num1);
    #pragma GCC diagnostic error "-Wformat-security"

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;
    rw_keyspec_path_t *get_keyspec = NULL;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    TSTPRN("Creating list element in  Member pub sub Cache notify ...\n");
    rs = rwdts_member_reg_handle_create_element_keyspec(
                                                     regh,
                                                     (rw_keyspec_path_t *)k_keyspec,
                                                     &phone->base,
                                                     NULL);
 
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
    //
    // Get the created element
    //

    rs = rwdts_member_reg_handle_get_element_keyspec(regh,
                                                     (rw_keyspec_path_t *)k_keyspec,
                                                     NULL,
                                                     &get_keyspec,
                                                     (const ProtobufCMessage**)&tmp);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    ASSERT_TRUE(tmp);
    ASSERT_TRUE(get_keyspec != NULL);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    // Update the list element
    //

    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_keyspec_path_delete_binpath((rw_keyspec_path_t *)k_keyspec, NULL);

    k_keyspec->dompath.path001.has_key00 = 1;
    #pragma GCC diagnostic ignored "-Wformat-security"
    sprintf (k_keyspec->dompath.path001.key00.number, phone_num2);
    #pragma GCC diagnostic error "-Wformat-security"

    TSTPRN("Updating list element in  Member pub sub Cache notify ...\n");
    rw_yang_pb_schema_t* sch = (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest);
    ASSERT_TRUE(sch != NULL);
    char *xp = rw_keyspec_path_to_xpath((const rw_keyspec_path_t *)k_keyspec, sch,NULL);
    ASSERT_TRUE(xp != NULL);
    rs = rwdts_member_reg_handle_update_element_xpath(regh,
                                                xp,
                                                &phone->base,
                                                0,
                                                NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
    RW_FREE(xp);
    xp = NULL;

    //
    // Get the updated element and verify the update
    //

    rs = rwdts_member_reg_handle_get_element_keyspec(regh,
                                                     (rw_keyspec_path_t *)k_keyspec,
                                                     NULL,
                                                     &get_keyspec,
                                                     (const ProtobufCMessage**)&tmp);

    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    ASSERT_TRUE(tmp);
    ASSERT_TRUE(get_keyspec != NULL);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //

    uint32_t count = rwdts_member_data_get_count(NULL, regh);
    EXPECT_EQ(2, count);

// Delete not supported in cache mode for now
    TSTPRN("Deleting list element in  Member pub sub Cache notify ...\n");
    //
    // Delete the updated element and verify it is gone
    //

    xp = rw_keyspec_path_to_xpath((const rw_keyspec_path_t *)k_keyspec, sch,NULL);
    ASSERT_TRUE(xp != NULL);

    rs = rwdts_member_reg_handle_delete_element_xpath(regh,
                                                    xp,
                                                    &phone->base,
                                                    NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
    RW_FREE(xp);
    xp = NULL;
 
    //
    // DO a get again
    //

    rs = rwdts_member_reg_handle_get_element_keyspec(regh,
                                                     (rw_keyspec_path_t *)k_keyspec,
                                                     NULL,
                                                     &get_keyspec,
                                                     (const ProtobufCMessage**)&tmp);
    EXPECT_NE(rs, RW_STATUS_FAILURE);
    EXPECT_FALSE(tmp);
    ASSERT_TRUE(get_keyspec == NULL);

    k_keyspec->dompath.path001.has_key00 = 1;
    #pragma GCC diagnostic ignored "-Wformat-security"
    sprintf (k_keyspec->dompath.path001.key00.number, phone_num2);
    #pragma GCC diagnostic error "-Wformat-security"

    phone = &phone_instance;

    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    TSTPRN("Creating list element in  Member pub sub Cache KN notify ...\n");

    rs = rwdts_member_reg_handle_create_element_keyspec(
                 regh,
                 (rw_keyspec_path_t *)k_keyspec,
                 &phone->base,
                 NULL);

    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Update the list element
    //

    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_WORK;

    TSTPRN("Updating list element in  Member pub sub Cache KN notify ...\n");
    rs = rwdts_member_reg_handle_update_element_keyspec(regh,
                                                        (rw_keyspec_path_t *)k_keyspec,
                                                        &phone->base,
                                                        0,
                                                        NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

// Delete not supported in cache mode for now
    TSTPRN("Deleting list element in  Member pub sub Cache KN notify ...\n");
    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_reg_handle_delete_element_keyspec(regh,
                                                     (rw_keyspec_path_t *)k_keyspec,
                                                      &phone->base,
                                                      NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

   rw_keyspec_path_free((rw_keyspec_path_t*)k_keyspec, NULL);
  }
  // The test would be done when all the members have received the publish callback
  // Check for completiom
  rwsched_dispatch_async_f(clnt_apih->tasklet,
                           clnt_apih->client.rwq,
                           s,
                           rwdts_set_exit_now);

}

static void memberapi_update_merge_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  rw_status_t rs;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  const RWPB_T_MSG(TestdtsRwFpath_Fastpath) *tmp_fpath;
  rw_keyspec_path_t *get_keyspec = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Update merge clent ...\n");

  clnt_apih->stats.num_member_advise_rsp = 0;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath) *lks = NULL;
  rs = rw_keyspec_path_create_dup((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath), NULL,
                             (rw_keyspec_path_t**)&lks);

  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  lks->has_dompath = TRUE;
  lks->dompath.path000.has_key00 = TRUE;
  strcpy(lks->dompath.path000.key00.name, "Colony123");

  lks->dompath.path001.has_key00 = TRUE;
  strcpy(lks->dompath.path001.key00.name, "NetworkCont123");

  rwdts_member_deregister_all(clnt_apih);

  rwdts_member_reg_handle_t list_reg;
  rw_keyspec_path_set_category((rw_keyspec_path_t*)lks, NULL, RW_SCHEMA_CATEGORY_DATA);


  /* Establish a  list registration */
  list_reg = rwdts_member_register(NULL, clnt_apih, (rw_keyspec_path_t*)lks, &cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_Fastpath),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);


  RWPB_M_PATHENTRY_DECL_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath,kpe);

  RWPB_T_MSG(TestdtsRwFpath_Fastpath) fpath;

  RWPB_F_MSG_INIT(TestdtsRwFpath_Fastpath,&fpath);

  fpath.has_rx = 1;
  fpath.has_tx = 1;

  fpath.rx.has_packet_counters_direction = 1;
  fpath.rx.packet_counters_direction.has_total_wire_packets = 1;
  fpath.rx.packet_counters_direction.total_wire_packets = 1000;
  fpath.has_instance = 1;

  fpath.tx.has_packet_counters_direction = 1;

  kpe.has_key00 = 1;
  kpe.key00.instance = fpath.instance = 1;

  rs = rwdts_member_reg_handle_create_element(
        list_reg,(rw_keyspec_entry_t*)&kpe,&fpath.base,NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_get_element(list_reg,
                                           (rw_keyspec_entry_t*)&kpe,
                                           NULL,
                                           &get_keyspec,
                                           (const ProtobufCMessage**)&tmp_fpath);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_TRUE(tmp_fpath);
  ASSERT_TRUE(get_keyspec != NULL);

  ASSERT_EQ(sizeof(fpath), fpath.base.descriptor->sizeof_message);

  uint32_t diff = protobuf_c_message_is_equal_deep(NULL, &tmp_fpath->base, &fpath.base);
  EXPECT_EQ(1, diff);

  // Change the keys  and do a get
  kpe.has_key00 = 1;
  kpe.key00.instance =  11;

  get_keyspec = NULL;
  rs = rwdts_member_reg_handle_get_element(list_reg,
                                          (rw_keyspec_entry_t*)&kpe,
                                          NULL,
                                          &get_keyspec,
                                          (const ProtobufCMessage**)&tmp_fpath);
  EXPECT_EQ(rs, RW_STATUS_FAILURE);
  ASSERT_FALSE(tmp_fpath);
  ASSERT_TRUE(get_keyspec == NULL);

  uint32_t count = rwdts_member_data_get_count(NULL, list_reg);

  EXPECT_EQ(count, 1);

  /*
   * Now update and delete
   */


  fpath.rx.packet_counters_direction.total_wire_packets = 10000;
  fpath.rx.packet_counters_direction.has_total_wire_packets = 1;


  kpe.key00.instance = fpath.instance = 2;

  rs = rwdts_member_reg_handle_update_element(list_reg,
                                             (rw_keyspec_entry_t*)&kpe,
                                             &fpath.base,
                                             RWDTS_XACT_FLAG_REPLACE,
                                             NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_get_element(list_reg,
                                          (rw_keyspec_entry_t*)&kpe,
                                          NULL,
                                          &get_keyspec,
                                          (const ProtobufCMessage**)&tmp_fpath);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  ASSERT_TRUE(tmp_fpath);
  ASSERT_TRUE(get_keyspec != NULL);
  ASSERT_EQ(sizeof(fpath), fpath.base.descriptor->sizeof_message);
  diff = protobuf_c_message_is_equal_deep(NULL, &tmp_fpath->base, &fpath.base);
  EXPECT_EQ(1, diff);

  fpath.rx.packet_counters_direction.total_vf_packets =  500;
  fpath.rx.packet_counters_direction.has_total_vf_packets = TRUE;
  fpath.rx.packet_counters_direction.has_total_128_packets = TRUE;
  fpath.rx.packet_counters_direction.total_128_packets = 100;

  rs = rwdts_member_reg_handle_update_element(list_reg,
                                             (rw_keyspec_entry_t*)&kpe,
                                             &fpath.base,
                                             0,
                                             NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_get_element(list_reg,
                                          (rw_keyspec_entry_t*)&kpe,
                                          NULL,
                                          &get_keyspec,
                                          (const ProtobufCMessage**)&tmp_fpath);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_TRUE(tmp_fpath);
  ASSERT_TRUE(get_keyspec != NULL);
  ASSERT_EQ(sizeof(fpath), fpath.base.descriptor->sizeof_message);
 
  EXPECT_EQ(tmp_fpath->rx.packet_counters_direction.total_wire_packets, 10000);
  EXPECT_EQ(tmp_fpath->rx.packet_counters_direction.total_vf_packets, 500);
  EXPECT_EQ(tmp_fpath->rx.packet_counters_direction.total_128_packets, 100);
  //diff = protobuf_c_message_is_equal_deep(&tmp_fpath->base, &fpath.base);
  //EXPECT_EQ(1, diff);

  rs = rwdts_member_reg_handle_delete_element(list_reg,
                                             (rw_keyspec_entry_t*)&kpe,
                                             &fpath.base,
                                             NULL);

  count = rwdts_member_data_get_count(NULL, list_reg);

  EXPECT_EQ(count, 1);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  get_keyspec = NULL;
  rs = rwdts_member_reg_handle_get_element(list_reg,
                                          (rw_keyspec_entry_t*)&kpe,
                                          NULL,
                                          &get_keyspec,
                                          (const ProtobufCMessage**)&tmp_fpath);
  EXPECT_EQ(rs, RW_STATUS_FAILURE);
  ASSERT_FALSE(tmp_fpath);
  ASSERT_TRUE(get_keyspec == NULL);

    rwsched_dispatch_async_f(clnt_apih->tasklet,
                             clnt_apih->client.rwq,
                             s,
                             rwdts_set_exit_now);

 rw_keyspec_path_free((rw_keyspec_path_t *)lks, NULL);
}

typedef struct {
  uint32_t init:1, pre:1, com:1, abrt:1, deinit:1;
} memberapi_test_grp_scratch_t;
static void *memberapi_test_grp_xinit(rwdts_group_t *grp,
				      rwdts_xact_t *xact,
				      void *ctx) 
{
  memberapi_test_grp_scratch_t *s = (memberapi_test_grp_scratch_t *)calloc(1, sizeof(memberapi_test_grp_scratch_t));
  s->init = TRUE;
  return s;
}
static void memberapi_test_grp_xdeinit(rwdts_group_t *grp,
					rwdts_xact_t *xact,
					void *ctx,
					void *scratch_in) {
  memberapi_test_grp_scratch_t *s = (memberapi_test_grp_scratch_t *)scratch_in;
  ASSERT_TRUE(s->init);
  if (s->com || s->abrt) {
    //ASSERT_TRUE(s->pre); abrt not just in Precommit state
    ASSERT_FALSE(s->com && s->abrt);
  }

  s->deinit = TRUE;
  memset(s, 0, sizeof(*s));
  free(s);
  return;
}
static rwdts_member_rsp_code_t memberapi_test_grp_event(rwdts_api_t *apih,
							rwdts_group_t *grp,
							rwdts_xact_t *xact,
							rwdts_member_event_t evt,
							void *ctx,
							void *scratch_in) {
  memberapi_test_grp_scratch_t *s = (memberapi_test_grp_scratch_t *)scratch_in;
  if (!s) {
    assert(!xact);
    return RWDTS_ACTION_OK;
  }
  assert(s->init);
  assert(!s->deinit);
  switch (evt) {
  case RWDTS_MEMBER_EVENT_PRECOMMIT:
    assert(!s->pre);
    assert(!s->com);
    s->pre=TRUE;
    break;
  case RWDTS_MEMBER_EVENT_COMMIT:
    assert(s->pre);
    assert(!s->abrt);
    assert(!s->com);
    s->com=TRUE;
    break;
  case RWDTS_MEMBER_EVENT_ABORT:
    assert(!s->com); //??
    assert(!s->abrt);
    s->abrt=TRUE;
    break;
  default:
    assert(0);
    break;
  }
  return RWDTS_ACTION_OK;
}

static void 
memberapi_ident_update_reg_ready(rwdts_member_reg_handle_t regh,
                                 rw_status_t rs, void *user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_event_cb_t reg_cb;
  rwdts_api_t *apih = ti->apih;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = user_data;
  reg_cb.cb.prepare = memberapi_test_ident_prepare;
  reg_cb.cb.reg_ready = memberapi_ident_member_reg_ready;

  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  rwdts_shard_info_detail_t shard_detail;

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 1;
  keyspec_entry1.dompath.path003.key00.key_four = ti->member_idx;

  memset(&shard_detail, 0, sizeof(shard_detail));
  int depth = rw_keyspec_path_get_depth(keyspec1);
  size_t keylen;
  ASSERT_TRUE(depth);
  const rw_keyspec_entry_t *pe = rw_keyspec_path_get_path_entry(keyspec1, depth-1);
  ASSERT_TRUE(pe != NULL);
  uint8_t *keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr!=NULL);
  TSTPRN("key value in %s: %x, %x, %x %x %x %x\n", __PRETTY_FUNCTION__, keyptr[0], keyptr[1], keyptr[2], keyptr[3], keyptr[4], keyptr[5]);
  shard_detail.shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;
  memcpy(&shard_detail.params.identparams.keyin[0], keyptr, keylen);
  shard_detail.params.identparams.keylen = keylen;

  keyspec_entry1.dompath.path003.has_key00 = 0;

  rw_status_t ret_stat = rwdts_member_api_shard_key_update(keyspec1, apih, regh,
                              RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                              0, RW_DTS_SHARD_FLAVOR_IDENT,
                              &shard_detail.params, RWDTS_SHARD_KEYFUNC_NOP, NULL, 
                              RWDTS_SHARD_DEMUX_ROUND_ROBIN,&reg_cb );

  ASSERT_TRUE(ret_stat == RW_STATUS_SUCCESS);
  RW_FREE(keyptr);
}

static void
memberapi_ident_member_reg_ready(rwdts_member_reg_handle_t  regh,
                             rw_status_t                rs,
                             void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_ident_client_start_f);
  }
  return;
}

static void
memberapi_range_member_reg_ready(rwdts_member_reg_handle_t  regh,
                             rw_status_t                rs,
                             void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_ident_client_start_f);
  }
  return;
}

static void
memberapi_member_reg_ready(rwdts_member_reg_handle_t  regh,
                             rw_status_t                rs,
                             void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  //  fprintf(stderr, "memberapi_member_reg_ready called; dts router reg count now %u\n", (unsigned) s->tenv->t[TASKLET_ROUTER].dts->rtr_reg_id);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  TSTPRN("Registered %s\n", reg->keystr);

  TSTPRN("memberapi_member_reg_ready() called\n");
  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_start_f);
  }
  return;
}

static void
memberapi_member_perf_reg_ready(rwdts_member_reg_handle_t  regh,
                                rw_status_t                rs,
                                void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  //  fprintf(stderr, "memberapi_member_perf_reg_ready called; dts router reg count now %u\n", (unsigned) s->tenv->t[TASKLET_ROUTER].dts->rtr_reg_id);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  TSTPRN("Registered %s\n", reg->keystr);

  TSTPRN("memberapi_member_perf_reg_ready() called\n");
  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == (TASKLET_MEMBER_CT*400)) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f %d\n", (int)s->reg_ready_called);
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_start_f);
  }
  return;
}

static rwdts_member_rsp_code_t 
memberapi_test_ident_prepare(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg, 
                              uint32_t credits,
                              void *getnext_ptr) 
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(s);


  RW_ZERO_VARIABLE(&rsp);

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  rsp.ks = keyspec;
  rsp.n_msgs = 0;
  rsp.evtrsp  = RWDTS_EVTRSP_ACK;
  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  s->num_responses++;
  return RWDTS_ACTION_OK;
}

static void memberapi_ident_member_start_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  //rwdts_member_reg_handle_t reg_handle;

  rwdts_member_event_cb_t reg_cb;
  rwdts_api_t *apih = ti->apih;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  
  reg_cb.cb.prepare = memberapi_test_ident_prepare;

  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);

  rwdts_member_deregister_all(apih);
 
  member_add_multi_regs(apih);

  reg_cb.cb.reg_ready = memberapi_ident_member_reg_ready;


  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  rwdts_shard_info_detail_t shard_detail;

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 1;
  keyspec_entry1.dompath.path003.key00.key_four = 4 + s->member_startct;

  memset(&shard_detail, 0, sizeof(shard_detail));
  int depth = rw_keyspec_path_get_depth(keyspec1);
  size_t keylen;
  ASSERT_TRUE(depth);
  const rw_keyspec_entry_t *pe = rw_keyspec_path_get_path_entry(keyspec1, depth-1);
  ASSERT_TRUE(pe != NULL);
  uint8_t *keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 0, &keylen);
  ASSERT_TRUE(keyptr!=NULL);

  shard_detail.shard_flavor = RW_DTS_SHARD_FLAVOR_IDENT;
  memcpy(&shard_detail.params.identparams.keyin[0], keyptr, keylen);
  shard_detail.params.identparams.keylen = keylen;

  keyspec_entry1.dompath.path003.has_key00 = 0;

  ret_stat = rwdts_member_api_shard_key_init(keyspec1, apih, 
                              RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                              RWDTS_FLAG_SUBSCRIBER, 0, RW_DTS_SHARD_FLAVOR_IDENT,
                              &shard_detail.params, RWDTS_SHARD_KEYFUNC_NOP, NULL, 
                              RWDTS_SHARD_DEMUX_ROUND_ROBIN,&reg_cb );

  ASSERT_TRUE(ret_stat == RW_STATUS_SUCCESS);
 
  ck_pr_inc_32(&s->member_startct);
  RW_FREE(keyptr);
  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_ident_update_member_start_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_event_cb_t reg_cb;
  rwdts_api_t *apih = ti->apih;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  
  reg_cb.cb.prepare = memberapi_test_ident_prepare;

  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);

  rwdts_member_deregister_all(apih);
 
  member_add_multi_regs(apih);

  reg_cb.cb.reg_ready = memberapi_ident_update_reg_ready;


  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 0;

  rwdts_member_reg_handle_t reg_handle = 
        rwdts_member_register(NULL, apih, keyspec1, &reg_cb,
                RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                RWDTS_FLAG_SUBSCRIBER| RWDTS_FLAG_SHARED, NULL);

  ASSERT_TRUE(reg_handle != NULL);
 
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_range_member_start_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_event_cb_t reg_cb;
  rwdts_api_t *apih = ti->apih;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  
  reg_cb.cb.prepare = memberapi_test_ident_prepare;

  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);

  rwdts_member_deregister_all(apih);
 
  member_add_multi_regs(apih);

  reg_cb.cb.reg_ready = memberapi_range_member_reg_ready;


  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  rwdts_shard_info_detail_t shard_detail;

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry1.dompath.path000.has_key00 = 1;
  keyspec_entry1.dompath.path000.key00.key_one = 1;
  keyspec_entry1.dompath.path001.has_key00 = 1;
  keyspec_entry1.dompath.path001.key00.key_two = 2;
  keyspec_entry1.dompath.path002.has_key00 = 1;
  keyspec_entry1.dompath.path002.key00.key_three = 3;
  keyspec_entry1.dompath.path003.has_key00 = 0;

  memset(&shard_detail, 0, sizeof(shard_detail));

  shard_detail.shard_flavor = RW_DTS_SHARD_FLAVOR_RANGE;
  shard_detail.params.range.start_range = s->member_startct * 10;
  shard_detail.params.range.end_range = shard_detail.params.range.start_range+10;

  ret_stat = rwdts_member_api_shard_key_init(keyspec1, apih, 
                              RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                              RWDTS_FLAG_SUBSCRIBER, 0, RW_DTS_SHARD_FLAVOR_RANGE,
                              &shard_detail.params, RWDTS_SHARD_KEYFUNC_NOP, NULL, 
                              RWDTS_SHARD_DEMUX_ROUND_ROBIN,&reg_cb );

  ASSERT_TRUE(ret_stat == RW_STATUS_SUCCESS);
 
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_start_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_reg_t reg_cb;
  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.user_data = ctx;
  
  if(s->member[ti->member_idx].call_getnext_api)
  {
    reg_cb.prepare = memberapi_test_get_next;
  }
  else
  {
    reg_cb.prepare = memberapi_test_prepare_new_version;
  }
  if (s->member[ti->member_idx].treatment != TREATMENT_PRECOMMIT_NULL) {
    reg_cb.precommit = memberapi_test_precommit;
  }
  reg_cb.commit = memberapi_test_commit;
  reg_cb.abort = memberapi_test_abort;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  apih->print_error_logs = 1;
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);
  apih->print_error_logs = 0;

  rwdts_group_t *group = NULL;
  rw_status_t rs = rwdts_api_group_create(apih, 
                                          memberapi_test_grp_xinit, 
                                          memberapi_test_grp_xdeinit, 
                                          memberapi_test_grp_event, 
                                          ctx,
                                          &group);
  RW_ASSERT(rs == RW_STATUS_SUCCESS)
  ASSERT_TRUE(!!group);

  rw_keyspec_path_t* lks= NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
 
  //  fprintf(stderr, "memberapi_member_start_f called; starting from dts router reg count regid %u\n", (unsigned) s->tenv->t[TASKLET_ROUTER].dts->rtr_reg_id);
  member_add_multi_regs(apih);

  rwdts_member_reg_handle_t regh;
#if 1
  char xpath[] = "/ps:person";
  rwdts_group_register_xpath(group, xpath, 
                       reg_cb.reg_ready, reg_cb.prepare,
                       reg_cb.precommit, reg_cb.commit,
                       reg_cb.abort,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
#else
  rwdts_group_register_keyspec(group, lks, 
                       reg_cb.reg_ready, reg_cb.prepare,
                       reg_cb.precommit, reg_cb.commit,
                       reg_cb.abort,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
#endif

  reg_cb.reg_ready = memberapi_member_reg_ready;
  rwdts_group_register_keyspec(group, lks,
                               reg_cb.reg_ready, reg_cb.prepare,
                               reg_cb.precommit, reg_cb.commit,
                               reg_cb.abort, RWDTS_FLAG_SUBSCRIBER,
                               reg_cb.user_data,&regh);

  rw_keyspec_path_free(lks, NULL);
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_start_perf_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_status_t status;

  rw_keyspec_path_t *keyspec;
  rwdts_member_reg_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.user_data = ctx;

  if(s->member[ti->member_idx].call_getnext_api)
  {
    reg_cb.prepare = memberapi_test_get_next;
  }
  else
  {
    reg_cb.prepare = memberapi_test_prepare_new_version;
  }
  if (s->member[ti->member_idx].treatment != TREATMENT_PRECOMMIT_NULL) {
    reg_cb.precommit = memberapi_test_precommit;
  }
  reg_cb.commit = memberapi_test_commit;
  reg_cb.abort = memberapi_test_abort;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  int i;
#undef LOOP1
#define LOOP1 100
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  = 
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);


  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  apih->print_error_logs = 1;
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);
  apih->print_error_logs = 0;

  rwdts_group_t *group = NULL;
  rw_status_t rs = rwdts_api_group_create(apih, 
                                          memberapi_test_grp_xinit, 
                                          memberapi_test_grp_xdeinit, 
                                          memberapi_test_grp_event, 
                                          ctx,
                                          &group);
  RW_ASSERT(rs == RW_STATUS_SUCCESS)
  ASSERT_TRUE(!!group);

  rw_keyspec_path_t* lks= NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;
#if 1
  char xpath[] = "/ps:person";
  rwdts_group_register_xpath(group, xpath, 
                       reg_cb.reg_ready, reg_cb.prepare,
                       reg_cb.precommit, reg_cb.commit,
                       reg_cb.abort,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
#else
  rwdts_group_register_keyspec(group, lks, 
                       reg_cb.reg_ready, reg_cb.prepare,
                       reg_cb.precommit, reg_cb.commit,
                       reg_cb.abort,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
#endif

//  reg_cb.reg_ready = memberapi_member_reg_ready;
  rwdts_group_register_keyspec(group, lks,
                               reg_cb.reg_ready, reg_cb.prepare,
                               reg_cb.precommit, reg_cb.commit,
                               reg_cb.abort, RWDTS_FLAG_SUBSCRIBER,
                               reg_cb.user_data,&regh);

  rw_keyspec_path_free(lks, NULL);

  for(i = 0; i < LOOP1; i++) {
    keyspec_entry1.dompath.path000.has_key00 = 1;
    keyspec_entry1.dompath.path000.key00.key_one = i;
    keyspec_entry1.dompath.path001.has_key00 = 1;
    keyspec_entry1.dompath.path001.key00.key_two = i;
    keyspec_entry1.dompath.path002.has_key00 = 1;
    keyspec_entry1.dompath.path002.key00.key_three = i;
    keyspec_entry1.dompath.path003.has_key00 = 1;
    keyspec_entry1.dompath.path003.key00.key_four = i;

    status = rwdts_group_register_keyspec(group, keyspec1, 
                       memberapi_member_perf_reg_ready, NULL, NULL, NULL, NULL,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);

    RW_ASSERT(status == RW_STATUS_SUCCESS); 

  } 

  for (i = LOOP1; i < 2*LOOP1; i++) {
    keyspec_entrywd1.dompath.path000.has_key00 = 1;
    keyspec_entrywd1.dompath.path000.key00.key_one = i;
    keyspec_entrywd1.dompath.path001.has_key00 = 1;
    keyspec_entrywd1.dompath.path001.key00.key_two = i;
    keyspec_entrywd1.dompath.path002.has_key00 = 1;
    keyspec_entrywd1.dompath.path002.key00.key_three = i;

    status = rwdts_group_register_keyspec(group, keyspec2, 
                       memberapi_member_perf_reg_ready, NULL, NULL, NULL, NULL,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
    RW_ASSERT(status == RW_STATUS_SUCCESS); 

  }

  for (i = 2*LOOP1; i < 3*LOOP1; i++) {
    keyspec_entrywd2.dompath.path000.has_key00 = 1;
    keyspec_entrywd2.dompath.path000.key00.key_one = i;
    keyspec_entrywd2.dompath.path001.has_key00 = 1;
    keyspec_entrywd2.dompath.path001.key00.key_two = i;

    status = rwdts_group_register_keyspec(group, keyspec3, 
                       memberapi_member_perf_reg_ready, NULL, NULL, NULL, NULL,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
    RW_ASSERT(status == RW_STATUS_SUCCESS); 
  }
  
  for (i = 3*LOOP1; i < 4*LOOP1; i++) {
    keyspec_entrywd3.dompath.path000.has_key00 = 1;
    keyspec_entrywd3.dompath.path000.key00.key_one = 1;

    status = rwdts_group_register_keyspec(group, keyspec4, 
                       memberapi_member_perf_reg_ready, NULL, NULL, NULL, NULL,
                       RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, 
                       reg_cb.user_data,&regh);
    RW_ASSERT(status == RW_STATUS_SUCCESS); 
  }

  ck_pr_inc_32(&s->member_startct);


  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}


static void
memberapi_member_start_multiple_reg_ready(rwdts_member_reg_handle_t  regh,
                                          rw_status_t                rs,
                                          void*                      ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("tasklet[%d] member[%d] memberapi_member_start_multiple_reg_ready() called startct=%u\n",
	 ti->tasklet_idx, ti->member_idx, s->member_startct);
  
  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("tasklet[%d] member[%d] memberapi_member_start_multiple_reg_ready invoking client start_f\n",
	   ti->tasklet_idx, ti->member_idx);
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_start_multiple_f);
  }
  return;
}
static void memberapi_member_start_multiple_f(void *ctx) 
{
  
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  char ph[128];

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rwdts_member_reg_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("tasklet[%d] member[%d] memberapi_member_start_multiple_f ...\n", ti->tasklet_idx, ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  keyspec = (rw_keyspec_path_t *)&keyspec_entry;
  reg_cb.user_data = ctx;

  reg_cb.reg_ready = memberapi_member_start_multiple_reg_ready;
  reg_cb.prepare = memberapi_test_prepare_multiple;
  if (s->member[ti->member_idx].treatment != TREATMENT_PRECOMMIT_NULL) {
    reg_cb.precommit = memberapi_test_precommit;
  }
  reg_cb.commit = memberapi_test_commit;
  reg_cb.abort = memberapi_test_abort;


  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  apih->print_error_logs = 1;
  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);
  apih->print_error_logs = 0;


  sprintf(ph,"%d",ti->member_idx);
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP(ph);
  keyspec_entry.dompath.path001.has_key00 = 1;

  TSTPRN("tasklet[%d] member[%d] memberapi_member_start_multuple_f registering for key00.number='%s' and both pub and sub\n", ti->tasklet_idx, ti->member_idx, ph);

  rwdts_group_t *group = NULL;
  rw_status_t rs = rwdts_api_group_create(apih, 
                              memberapi_test_grp_xinit, 
                              memberapi_test_grp_xdeinit, 
                              memberapi_test_grp_event, 
                              ctx, &group);
  RW_ASSERT(rs == RW_STATUS_SUCCESS)
  ASSERT_TRUE(!!group);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;
  rwdts_group_register_keyspec(group, keyspec, 
                       reg_cb.reg_ready, reg_cb.prepare,
                       reg_cb.precommit, reg_cb.commit,
                       reg_cb.abort,
                       RWDTS_FLAG_PUBLISHER, 
                       reg_cb.user_data,&regh);

  rwdts_group_register_keyspec(group, keyspec,
                               NULL, reg_cb.prepare,
                               reg_cb.precommit, reg_cb.commit,
                               reg_cb.abort, RWDTS_FLAG_SUBSCRIBER,
                               reg_cb.user_data,&regh);

 RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

static void
member_gi_api_test_reg_ready(rwdts_member_reg_handle_t  regh,
                             rw_status_t                rs,
                             void*                      user_data)
{
  TSTPRN("member_gi_api_test_reg_ready() called\n");
  return;
}
static rwdts_member_rsp_code_t member_gi_api_test_prepare(const rwdts_xact_info_t* xact_info,
                                       RWDtsQueryAction         action,
                                       const rw_keyspec_path_t* keyspec,
                                       const ProtobufCMessage*  msg,
                                       void*                    user_data)
{
  rw_keyspec_path_t*      ks = NULL;
  rw_status_t             rs;
  const char *xpath = "/ps:person/ps:name";
  rwdts_api_t *api = rwdts_xact_info_get_api(xact_info);
  rwdts_xact_t *xact = rwdts_xact_info_get_xact(xact_info);

  RW_ASSERT(api != NULL);
  RW_ASSERT(xact != NULL);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  xact = NULL; // for code coverage we called rwdts_xact_info_get api

  TSTPRN("member_gi_api_test_prepare() called\n");

#if 0
  // too noisy eventhough this was helping to increase unit test coverage report
  rwdts_xact_print(xact_info->xact);
  rwdts_api_print(xact_info->xact->apih);
  rwdts_reg_print((rwdts_member_registration_t*)xact_info->regh);
#endif
  rs = rwdts_api_keyspec_from_xpath(xact_info->xact->apih, xpath, &ks);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  return RWDTS_ACTION_OK;
}
static rwdts_member_rsp_code_t member_gi_api_test_precommit(const rwdts_xact_info_t* xact_info,
                                         void*                    user_data)
{
  TSTPRN("member_gi_api_test_precommit() called\n");
  return RWDTS_ACTION_OK;
}
static rwdts_member_rsp_code_t member_gi_api_test_commit(const rwdts_xact_info_t* xact_info,
                                      void*                    user_data)
{
  TSTPRN("member_gi_api_test_commit() called\n");
  return RWDTS_ACTION_OK;
}
static rwdts_member_rsp_code_t member_gi_api_test_abort(const rwdts_xact_info_t* xact_info,
                                     void*                    user_data)
{
  TSTPRN("member_gi_api_test_abort() called\n");
  return RWDTS_ACTION_OK;
}

static void member_gi_api_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member GI ...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person) *person_ks = NULL;
  rw_status_t rs = rw_keyspec_path_create_dup((rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person)) , NULL, (rw_keyspec_path_t **)&person_ks);

  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
#if 0
  person_ks->dompath.path000.has_key00 = 1;
  person_ks->dompath.path000.key00.name = (char*)"TestName";
#endif

  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)person_ks;

    /* Establish a registration */

  rwdts_member_deregister_all(clnt_apih);

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  RWPB_T_MSG(DtsTest_Person) person;
  
  RWPB_F_MSG_INIT(DtsTest_Person,&person);
  
  person.name = (char*)"TestName";
  person.has_id = TRUE;
  person.id = 1000;
  person.email = (char*)"test@test.com";
  person.has_employed = TRUE;
  person.employed = TRUE;
  person.n_phone = 1;
  RWPB_T_MSG(DtsTest_PhoneNumber) phone1, phone2, *phones[2];
  
  person.phone = phones;
  phones[0] = &phone1;
  phones[1] = &phone2;
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person.phone[0]);

  person.phone[0]->number = (char*)"123-456-789";
  person.phone[0]->has_type = TRUE;
  person.phone[0]->type = DTS_TEST_PHONE_TYPE_MOBILE;

  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person.phone[1]);
  person.phone[1]->number = (char*)"987-654-321";
  person.phone[1]->has_type = TRUE;
  person.phone[1]->type = DTS_TEST_PHONE_TYPE_MOBILE;

  /* WTF are we testing that we're having to do it with synthetic advise incantations? */

  flags |= RWDTS_XACT_FLAG_END; //xact_parent = NULL
  rs = rwdts_advise_query_proto_int(clnt_apih,
                                    keyspec,
                                    NULL, // xact_parent
                                    &(person.base),
                                    123,
                                    NULL, // Debug
                                    RWDTS_QUERY_UPDATE,
                                    &clnt_cb,
                                    flags,
                                    NULL, NULL);
  rw_keyspec_path_free(keyspec, NULL);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
}

static void
member_reg_ready_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ti->cb.reg_ready_old++;
  is_test_done(s);
}

static void
member_reg_ready(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ti->cb.reg_ready++;
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  is_test_done(s);
}

static void member_reg_ready_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Reg Ready %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person)); 
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  ti->cb.reg_ready = 0;
  ti->cb.reg_ready_old = 0;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = member_reg_ready_old;
  reg_cb.cb.reg_ready     = member_reg_ready;

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  EXPECT_TRUE(pub_regh);

  /* Establish a  subscriber registration */
  rwdts_member_reg_handle_t sub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  sub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                   RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  EXPECT_TRUE(sub_regh);
}

static void rwdts_notif_exit_now(void *ctx)
{
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  TSTPRN("%s:%d: apih->stats.num_notif_rsp_count=%d s->expected_notif_rsp_count=%d\n",
   __PRETTY_FUNCTION__,
   __LINE__,
   (int)apih->stats.num_notif_rsp_count,
   (int)s->expected_notif_rsp_count);

  if (apih->stats.num_notif_rsp_count <  s->expected_notif_rsp_count) {
    rwsched_dispatch_after_f(apih->tasklet,
                             10 * NSEC_PER_SEC,
                             apih->client.rwq,
                             s,
                             rwdts_notif_exit_now);
  } else {
    ASSERT_EQ(s->prepare_cb_called, 3); 
    s->exitnow = 1;
  }
  return;
}

static void memberapi_client_notif_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("MemberAPI test notif client start...\n");

  rwdts_xact_t* xact = NULL;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  clnt_apih->stats.num_notif_rsp_count = 0;

  RWPB_M_MSG_DECL_PTR_ALLOC(DtsTest_notif_EventRouteAdded, route);

  route->name = strdup("TestName");
  route->id = 3;

  xact = rwdts_api_send_notification(clnt_apih, &(route->base));
  ASSERT_TRUE(xact);

  rwdts_notif_exit_now(s);
  protobuf_c_message_free_unpacked(NULL, &(route->base));
  return;
}

static rwdts_member_rsp_code_t
member_reg_notif_prepare(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t*      key,
                         const ProtobufCMessage*  msg,
                         uint32_t credits,
                         void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

 s->prepare_cb_called++;

  return RWDTS_ACTION_OK;
}

static void
member_reg_notif_ready(rwdts_member_reg_handle_t regh,
                       rw_status_t               rs,
                       void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  TSTPRN("member_reg_notif_ready() called\n");
  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_notif_start_f);
  }
  return;
}

static void member_notif_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Notif %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_notif_EventRouteAdded) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_notif_EventRouteAdded));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready = member_reg_notif_ready;
  reg_cb.cb.prepare   = member_reg_notif_prepare;

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t sub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_NOTIFICATION);
  sub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_notif_EventRouteAdded),
                                   RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_SHARED, NULL);

  EXPECT_TRUE(sub_regh);

}

rw_status_t member_appdata_cb_safe_ks_rwref_take(rwdts_shard_handle_t *shard,
                                                 rw_keyspec_path_t *ks,
                                                 ProtobufCMessage **ref_out,
                                                 void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);

  *ref_out = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

static
void appdata_stored_msg_free(void *ud)
{
  struct queryapi_state *s = (struct queryapi_state *)ud;
  int i;

  for (i = 0; i < TASKLET_MEMBER_CT; i++) {
    if (s->msg[i]) {
      protobuf_c_message_free_unpacked(NULL, s->msg[i]);
      s->msg[i] = NULL;
    }
  }

  return;
}

static
void appdata_stored_getnext_free(void *ud)
{
  struct queryapi_state *s = (struct queryapi_state *)ud;
  int i;

  for (i = 0; i < 3; i++) {
    if (s->getnext_msg[i]) {
      protobuf_c_message_free_unpacked(NULL, s->getnext_msg[i]);
      s->getnext_msg[i] = NULL;
    }
    if (s->getnext_ks[i]) {
      rw_keyspec_path_free(s->getnext_ks[i], NULL);
      s->getnext_ks[i] = NULL;
    }
    if (s->getnext_pe[i]) {
      rw_keyspec_entry_free(s->getnext_pe[i], NULL);
      s->getnext_pe[i] = NULL;
    }
    if (s->getnext_mk[i]) {
      RW_FREE(s->getnext_mk[i]);
      s->getnext_mk[i] = NULL;
    }
  }

  return;
}

rw_status_t member_appdata_cb_safe_pe_rwref_take(rwdts_shard_handle_t *shard,
                                                 rw_keyspec_entry_t *pe,
                                                 ProtobufCMessage **ref_out,
                                                 void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *ref_out = s->msg[ti->member_idx];
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_safe_mk_rwref_take(rwdts_shard_handle_t *shard,
                                                 rw_schema_minikey_t *mk,
                                                 ProtobufCMessage **ref_out,
                                                 void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *ref_out = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

static void
appdata_subscriber_ready(rwdts_member_reg_handle_t regh,
                        rw_status_t status,
                        void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_reg_handle_t pub_regh = s->regh[0]; /* Read the data from first member */
  rw_status_t rs;
  ProtobufCMessage* msg;

  rw_keyspec_path_t* get_keyspec = NULL;
  rw_keyspec_path_t* keyspec = ((rwdts_member_registration_t *)pub_regh)->keyspec;
  rwdts_shard_t* shard = ((rwdts_member_registration_t *)pub_regh)->shard;

  rs = rwdts_member_reg_handle_get_element_keyspec(pub_regh, keyspec, NULL, &get_keyspec, (const ProtobufCMessage**)&msg);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  if (shard && shard->appdata) {
    rw_keyspec_path_free(get_keyspec, NULL);
    get_keyspec = NULL;
  }
  
  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone = (RWPB_T_MSG(DtsTest_data_Person_Phone) *)msg;
  if (phone) {
    rwdts_shard_handle_appdata_safe_put_exec(shard, shard->appdata->scratch_safe_id); 
  }
  EXPECT_EQ(phone->type, DTS_TEST_PHONE_TYPE_HOME);
  EXPECT_STREQ(phone->number, "4321876590");

  rs = rwdts_member_reg_handle_update_element_keyspec(pub_regh, keyspec, msg, 0, NULL);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  if (shard->appdata->appdata_type != RWDTS_APPDATA_TYPE_QUEUE_KEYSPEC) {
    rw_status_t rs = rwdts_member_reg_handle_delete_element_keyspec(pub_regh, keyspec, NULL, NULL);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  }

  appdata_stored_msg_free(s);
  if (shard->appdata) {
      protobuf_c_message_free_unpacked(NULL, msg);
  }
  s->exitnow = 1;

  return;
}

static void memberapi_appdata_client_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;

  switch(s->appdata) {
    case (APPDATA_KS):
      {
        cb.cb.reg_ready = appdata_subscriber_ready;
        break;
      }
    case (APPDATA_PE):
      {
        cb.cb.reg_ready = appdata_subscriber_ready;
        break;
      }
    case (APPDATA_MK):
      {
        cb.cb.reg_ready = appdata_subscriber_ready;
        break;
      }
  }

  TSTPRN("starting pub before sub client start ...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  if (!s->client_dreg_done) {
    rwdts_member_deregister_all(clnt_apih);
    s->client_dreg_done = 1;
  }


  clnt_apih->stats.num_member_advise_rsp = 0;
  rwdts_memberapi_register(NULL, clnt_apih,
                           keyspec,
                           &cb,
                           RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                           RWDTS_FLAG_CACHE|RWDTS_FLAG_SUBSCRIBER, NULL);
}


rw_status_t member_appdata_cb_ks_create(rwdts_shard_handle_t *shard,
                                        rw_keyspec_path_t *ks,
                                        ProtobufCMessage *newmsg,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, newmsg, newmsg->descriptor);

  rwdts_shard_handle_appdata_register_keyspec_pbref_install(shard, ks, s->msg[ti->member_idx], ud);
  TSTPRN("member_appdata_cb_ks_create Data inserted for member-idx %u \n", ti->member_idx);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_ks_delete(rwdts_shard_handle_t *shard,
                                        rw_keyspec_path_t *ks,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_handle_appdata_register_keyspec_pbref_uninstall(shard, ks, ud);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_unsafe_ks_rwref_get(rwdts_shard_handle_t *shard,
                                                  rw_keyspec_path_t *ks,
                                                  ProtobufCMessage **ref_out,
                                                  void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *ref_out = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_pe_create(rwdts_shard_handle_t *shard,
                                        rw_keyspec_entry_t *pe,
                                        ProtobufCMessage *newmsg,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, newmsg, newmsg->descriptor);

  rwdts_shard_handle_appdata_register_pathentry_pbref_install(shard, pe, s->msg[ti->member_idx], ud);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_pe_delete(rwdts_shard_handle_t *shard,
                                        rw_keyspec_entry_t *pe,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_handle_appdata_register_pathentry_pbref_uninstall(shard, pe, ud);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_unsafe_pe_rwref_get(rwdts_shard_handle_t *shard,
                                                  rw_keyspec_entry_t *pe,
                                                  ProtobufCMessage **ref_out,
                                                  void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *ref_out = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_mk_create(rwdts_shard_handle_t *shard,
                                        rw_schema_minikey_t *mk,
                                        ProtobufCMessage *newmsg,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, newmsg, newmsg->descriptor);

  rwdts_shard_handle_appdata_register_minikey_pbref_install(shard, mk, s->msg[ti->member_idx], ud);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_mk_delete(rwdts_shard_handle_t *shard,
                                        rw_schema_minikey_t *mk,
                                        void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_handle_appdata_register_minikey_pbref_uninstall(shard, mk, ud);
  return RW_STATUS_SUCCESS;
}

rw_status_t member_appdata_cb_unsafe_mk_rwref_get(rwdts_shard_handle_t *shard,
                                                  rw_schema_minikey_t *mk,
                                                  ProtobufCMessage **ref_out,
                                                  void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *ref_out = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

static void member_reg_appdata_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Reg Appdata %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("4321876590");

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = member_reg_appdata_ready_unsafe_old;

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  EXPECT_TRUE(pub_regh);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

static void member_reg_appdata_safe_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Reg Ready %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("4321876590");

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = member_reg_appdata_ready_safe_old;

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  EXPECT_TRUE(pub_regh);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

rw_status_t
rwdts_appdata_cb_ks_pbdelta_1(rwdts_shard_handle_t *shard,
                              rw_keyspec_path_t *ks,
                              ProtobufCMessage *pbdelta,
                              void *ud)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (pbdelta) {
    s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, pbdelta, pbdelta->descriptor);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_appdata_cb_pe_pbdelta_1(rwdts_shard_handle_t *shard,
                              rw_keyspec_entry_t *pe,
                              ProtobufCMessage *pbdelta,
                              void *ud)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (pbdelta) {
    s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, pbdelta, pbdelta->descriptor);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_appdata_cb_mk_pbdelta_1(rwdts_shard_handle_t *shard,
                              rw_schema_minikey_t *mk,
                              ProtobufCMessage *pbdelta,
                              void *ud)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (pbdelta) {
    s->msg[ti->member_idx] = protobuf_c_message_duplicate(NULL, pbdelta, pbdelta->descriptor);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
member_appdata_cb_ks_copy(rwdts_shard_handle_t *shard,
                          rw_keyspec_path_t *ks,
                          ProtobufCMessage **copy_output,
                          void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *copy_output = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

rw_status_t
member_appdata_cb_pe_copy(rwdts_shard_handle_t *shard,
                          rw_keyspec_entry_t *pe,
                          ProtobufCMessage **copy_output,
                          void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *copy_output = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

rw_status_t
member_appdata_cb_mk_copy(rwdts_shard_handle_t *shard,
                          rw_schema_minikey_t *mk,
                          ProtobufCMessage **copy_output,
                          void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(s->msg[ti->member_idx]);
  *copy_output = s->msg[ti->member_idx];

  return RW_STATUS_SUCCESS;
}

static void
member_reg_appdata_ready_queue_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;
 
  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_queue_keyspec(shard, NULL, member_appdata_cb_ks_copy, rwdts_appdata_cb_ks_pbdelta_1, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_queue_pe(shard, NULL, member_appdata_cb_pe_copy, rwdts_appdata_cb_pe_pbdelta_1, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_queue_minikey(shard, NULL, member_appdata_cb_mk_copy, rwdts_appdata_cb_mk_pbdelta_1, ctx, NULL);
        break;
      }
  }

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
  phone = &phone_instance;

  dts_test__yang_data__dts_test__person__phone__init(phone);
  phone->number = RW_STRDUP("4321876590");
  phone->has_type = 1;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;
  rwdts_member_reg_handle_create_element_keyspec(regh, ((rwdts_member_registration_t *)regh)->keyspec, &(phone->base), NULL);
  RW_FREE(phone->number);
  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_appdata_client_start_f);
  }

  return;
}

static void
member_reg_appdata_ready_safe_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;
  
  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_safe_keyspec(shard, NULL, member_appdata_cb_safe_ks_rwref_take, NULL,
                                            member_appdata_cb_ks_create, member_appdata_cb_ks_delete, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_safe_pe(shard, NULL, member_appdata_cb_safe_pe_rwref_take, NULL,
                                       member_appdata_cb_pe_create, member_appdata_cb_pe_delete, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_safe_minikey(shard, NULL, member_appdata_cb_safe_mk_rwref_take, NULL,
                                            member_appdata_cb_mk_create, member_appdata_cb_mk_delete, ctx, NULL);
        break;
      }
  }

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
  phone = &phone_instance;

  dts_test__yang_data__dts_test__person__phone__init(phone);
  phone->number = RW_STRDUP("4321876590");
  phone->has_type = 1;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;
  rwdts_member_reg_handle_create_element_keyspec(regh, ((rwdts_member_registration_t *)regh)->keyspec, &(phone->base), NULL);

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_appdata_client_start_f);
  }
  RW_FREE(phone->number);
  return;
}

static void
member_reg_appdata_ready_unsafe_old(rwdts_member_reg_handle_t regh,
                     const rw_keyspec_path_t*  ks,
                     const ProtobufCMessage*   msg,
                     void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;

  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_unsafe_keyspec(shard, NULL, member_appdata_cb_unsafe_ks_rwref_get, member_appdata_cb_ks_create, member_appdata_cb_ks_delete, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_unsafe_pe(shard, NULL, member_appdata_cb_unsafe_pe_rwref_get, member_appdata_cb_pe_create, member_appdata_cb_pe_delete, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_unsafe_minikey(shard, NULL, member_appdata_cb_unsafe_mk_rwref_get, member_appdata_cb_mk_create, member_appdata_cb_mk_delete, ctx, NULL);
        break;
      }
  }

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance = {};
  phone = &phone_instance;

  dts_test__yang_data__dts_test__person__phone__init(phone);
  phone->number = RW_STRDUP("4321876590");
  phone->has_type = 1;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;
  rwdts_member_reg_handle_create_element_keyspec(regh, ((rwdts_member_registration_t *)regh)->keyspec, &(phone->base), NULL);
  RW_FREE(phone->number);
  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_appdata_client_start_f);
  }

  return;
}

static void member_reg_appdata_queue_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Reg Ready %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("4321876590");

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = member_reg_appdata_ready_queue_old;

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  EXPECT_TRUE(pub_regh);

}

rw_status_t
appdata_cb_ks_getnext(const rw_keyspec_path_t* ks,
                           rw_keyspec_path_t** next_ks,
                           void *ctx)
{

  RW_ASSERT(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(next_ks);

  *next_ks = s->getnext_ks[s->position];

  if (*next_ks) {
    return RW_STATUS_SUCCESS;
  } else {
    return RW_STATUS_FAILURE;
  }
}

rw_status_t
appdata_cb_safe_ks_take(rwdts_shard_handle_t *shard,
                        rw_keyspec_path_t* ks,
                        ProtobufCMessage **out_ref,
                        void* ud)
{

  RW_ASSERT(ud);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(out_ref);

  //ASSERT_EQ(ks, s->getnext_ks[s->position]);

  *out_ref = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t
appdata_cb_pe_getnext(rw_keyspec_entry_t* pe,
                           rw_keyspec_entry_t** next_pe,
                           void *ctx)
{

  RW_ASSERT(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(next_pe);

  *next_pe = s->getnext_pe[s->position];

  if (*next_pe) {
    return RW_STATUS_SUCCESS;
  } else {
    return RW_STATUS_FAILURE;
  }
}

rw_status_t
appdata_cb_safe_pe_take(rwdts_shard_handle_t *shard,
                        rw_keyspec_entry_t* pe,
                        ProtobufCMessage **out_ref,
                        void* ud)
{

  RW_ASSERT(ud);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(out_ref);

  //ASSERT_EQ(ks, s->getnext_ks[s->position]);

  *out_ref = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t
appdata_cb_mk_getnext(rw_schema_minikey_t* mk,
                           rw_schema_minikey_t** next_mk,
                           void *ctx)
{

  RW_ASSERT(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(next_mk);

  *next_mk = (rw_schema_minikey_t *)s->getnext_mk[s->position];

  if (*next_mk) {
    return RW_STATUS_SUCCESS;
  } else {
    return RW_STATUS_FAILURE;
  }
}

rw_status_t
appdata_cb_safe_mk_take(rwdts_shard_handle_t *shard,
                        rw_schema_minikey_t* mk,
                        ProtobufCMessage **out_ref,
                        void* ud)
{

  RW_ASSERT(ud);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(out_ref);

  //ASSERT_EQ(ks, s->getnext_ks[s->position]);

  *out_ref = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t 
appdata_cb_unsafe_ks_get(rwdts_shard_handle_t *shard,
                         rw_keyspec_path_t *ks,
                         ProtobufCMessage **ref_out,
                         void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  *ref_out = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t
appdata_cb_unsafe_pe_get(rwdts_shard_handle_t *shard,
                         rw_keyspec_entry_t *pe,
                         ProtobufCMessage **ref_out,
                         void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  
  *ref_out = s->getnext_msg[s->position];
  
  return RW_STATUS_SUCCESS;
} 

rw_status_t
appdata_cb_unsafe_mk_get(rwdts_shard_handle_t *shard,
                         rw_schema_minikey_t *mk,
                         ProtobufCMessage **ref_out,
                         void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  
  *ref_out = s->getnext_msg[s->position];
  
  return RW_STATUS_SUCCESS;
} 

rw_status_t
appdata_cb_ks_copy(rwdts_shard_handle_t *shard,
                   rw_keyspec_path_t *ks,
                   ProtobufCMessage **copy_output,
                   void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  *copy_output = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t
appdata_cb_pe_copy(rwdts_shard_handle_t *shard,
                   rw_keyspec_entry_t *pe,
                   ProtobufCMessage **copy_output,
                   void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  *copy_output = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

rw_status_t
appdata_cb_mk_copy(rwdts_shard_handle_t *shard,
                   rw_schema_minikey_t *mk,
                   ProtobufCMessage **copy_output,
                   void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  *copy_output = s->getnext_msg[s->position];

  return RW_STATUS_SUCCESS;
}

static void
appdata_test_getnext_api(void *ctx)
{
  RW_ASSERT(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  //rwdts_shard_handle_t* shard = s->shard[0];
  rwdts_member_registration_t* reg = (rwdts_member_registration_t *)s->regh[0];
  rwdts_shard_handle_t* shard = reg->shard;

  rwdts_appdata_cursor_t *cursor = rwdts_shard_handle_appdata_get_current_cursor(shard);

  ProtobufCMessage* msg = NULL;
  rw_keyspec_path_t* keyspec = NULL;
  rw_keyspec_entry_t *pe = NULL;
  rw_schema_minikey_t *mk = NULL;

  if (s->appdata == APPDATA_KS) {
    while((msg = (ProtobufCMessage*) rwdts_shard_handle_appdata_getnext_list_element_keyspec(shard,
                                                 cursor, &keyspec)) != NULL) {
      rwdts_shard_handle_appdata_safe_put_exec(shard, shard->appdata->scratch_safe_id);
      rw_keyspec_path_free(keyspec, NULL);
      keyspec = NULL;
      s->position++;
    }
  }

  if (s->appdata == APPDATA_PE) {
    while((msg = (ProtobufCMessage*) rwdts_shard_handle_appdata_getnext_list_element_pathentry(shard,
                                                 cursor, &pe)) != NULL) {
      rwdts_shard_handle_appdata_safe_put_exec(shard, shard->appdata->scratch_safe_id);
      if (pe)  {
        rw_keyspec_entry_free(pe, NULL);
        pe = NULL;
      }
      s->position++;
    }
  }    

  if (s->appdata == APPDATA_MK) {
    while((msg = (ProtobufCMessage*) rwdts_shard_handle_appdata_getnext_list_element_minikey(shard,
                                                 cursor, &mk)) != NULL) {
      rwdts_shard_handle_appdata_safe_put_exec(shard, shard->appdata->scratch_safe_id);
      RW_FREE(mk);
      mk = NULL;
      s->position++;
    }
  }   

  EXPECT_EQ(3, s->position); 
  appdata_stored_getnext_free((void *)s);
  s->exitnow = 1;
  return;
}

static void
appdata_create_keyspec_msg_db(void *ctx)
{
  RW_ASSERT(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rw_status_t rs;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  char xpath1[] = "D,/ps:person/ps:phone[ps:number='1234']";
  char xpath2[] = "D,/ps:person/ps:phone[ps:number='4321']";
  char xpath3[] = "D,/ps:person/ps:phone[ps:number='6543']";
  rw_keyspec_path_t *ks1=NULL, *ks2=NULL, *ks3=NULL;

  rs = rwdts_api_keyspec_from_xpath(apih, xpath1, &ks1);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone1;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance1 = {};
  phone1 = &phone_instance1;

  dts_test__yang_data__dts_test__person__phone__init(phone1);
  phone1->number = RW_STRDUP("987654012");
  phone1->has_type = 1;
  phone1->type = DTS_TEST_PHONE_TYPE_HOME;

  s->getnext_msg[0] = protobuf_c_message_duplicate(NULL, &phone1->base, phone1->base.descriptor);

  rs = rwdts_api_keyspec_from_xpath(apih, xpath2, &ks2);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone2;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance2 = {};
  phone2 = &phone_instance2;

  dts_test__yang_data__dts_test__person__phone__init(phone2);
  RW_FREE(phone1->number);
//phone1->number = RW_STRDUP("987654013");
//phone1->has_type = 1;
//phone1->type = DTS_TEST_PHONE_TYPE_WORK;

  s->getnext_msg[1] = protobuf_c_message_duplicate(NULL, &phone2->base, phone2->base.descriptor); 

  rs = rwdts_api_keyspec_from_xpath(apih, xpath3, &ks3);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone3;
  static RWPB_T_MSG(DtsTest_data_Person_Phone) phone_instance3 = {};
  phone3 = &phone_instance3;

  dts_test__yang_data__dts_test__person__phone__init(phone3);
  phone3->number = RW_STRDUP("987654014");
  phone3->has_type = 1;
  phone3->type = DTS_TEST_PHONE_TYPE_HOME;

  s->getnext_msg[2] = protobuf_c_message_duplicate(NULL, &phone3->base, phone3->base.descriptor);
  RW_FREE(phone3->number);
  switch (s->appdata) {
    case (APPDATA_KS): {
      s->getnext_ks[0] = ks1;
      s->getnext_ks[1] = ks2;
      s->getnext_ks[2] = ks3;
      break;
    }

    case (APPDATA_PE): {
      size_t depth = rw_keyspec_path_get_depth(ks1);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks1, depth-1);
      rw_keyspec_entry_t* dup_pe = NULL;
      rs = rw_keyspec_entry_create_dup(pe, NULL, &dup_pe);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_pe[0] = dup_pe;

      dup_pe = NULL;
      depth = rw_keyspec_path_get_depth(ks2);
      pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks2, depth-1);
      rs = rw_keyspec_entry_create_dup(pe, NULL, &dup_pe);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_pe[1] = dup_pe;

      dup_pe = NULL;
      depth = rw_keyspec_path_get_depth(ks3);
      pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks3, depth-1);
      rs = rw_keyspec_entry_create_dup(pe, NULL, &dup_pe);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_pe[2] = dup_pe;
      break;
    }
       
    case (APPDATA_MK): {
      rw_schema_minikey_t* minikey = NULL;

      size_t depth = rw_keyspec_path_get_depth(ks1);
      rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks1, depth-1);
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_mk[0] = (char *)minikey;

      minikey = NULL;
      depth = rw_keyspec_path_get_depth(ks2);
      pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks2, depth-1);
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_mk[1] = (char *)minikey;

      minikey = NULL;
      depth = rw_keyspec_path_get_depth(ks3);
      pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks3, depth-1);
      rs = rw_schema_minikey_get_from_path_entry(pe, &minikey);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      s->getnext_mk[2] = (char *)minikey;
      break;
    }
  }

  if (s->appdata != APPDATA_KS) {
    rw_keyspec_path_free(ks1, NULL);
    rw_keyspec_path_free(ks2, NULL);
    rw_keyspec_path_free(ks3, NULL);
  }
  return;
}

static void
appdata_ready_safe_getnext(rwdts_member_reg_handle_t regh,
                           const rw_keyspec_path_t*  ks,
                           const ProtobufCMessage*   msg,
                           void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;

  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_safe_keyspec(shard, appdata_cb_ks_getnext, appdata_cb_safe_ks_take, NULL,
                                                         NULL, NULL, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_safe_pe(shard, appdata_cb_pe_getnext, appdata_cb_safe_pe_take, NULL,
                                                    NULL, NULL, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_safe_minikey(shard, appdata_cb_mk_getnext, appdata_cb_safe_mk_take, NULL,
                                                         NULL, NULL, ctx, NULL);
        break;
      }
    default:
        break;
  }

  if (s->reg_ready_called == 0) {
    appdata_create_keyspec_msg_db(ctx);
  }

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    appdata_test_getnext_api(ctx);
  }

  return;
}

static void
appdata_ready_unsafe_getnext(rwdts_member_reg_handle_t regh,
                             const rw_keyspec_path_t*  ks,
                             const ProtobufCMessage*   msg,
                             void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;

  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_unsafe_keyspec(shard, appdata_cb_ks_getnext, appdata_cb_unsafe_ks_get, 
                                                           NULL, NULL, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_unsafe_pe(shard, appdata_cb_pe_getnext, appdata_cb_unsafe_pe_get,
                                                      NULL, NULL, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_unsafe_minikey(shard, appdata_cb_mk_getnext, appdata_cb_unsafe_mk_get,
                                                           NULL, NULL, ctx, NULL);
        break;
      }
    default:
        break;
  }

  if (s->reg_ready_called == 0) {
    appdata_create_keyspec_msg_db(ctx);
  }

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    appdata_test_getnext_api(ctx);
  }

  return;
}

static void
appdata_ready_queue_getnext(rwdts_member_reg_handle_t regh,
                            const rw_keyspec_path_t*  ks,
                            const ProtobufCMessage*   msg,
                            void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_shard_t* shard = ((rwdts_member_registration_t *)regh)->shard;

  RW_ASSERT(shard);
  s->shard[ti->member_idx] = shard;
  s->regh[ti->member_idx] = regh;

  switch(s->appdata) {
    case (APPDATA_KS):
      {
        rwdts_shard_handle_appdata_register_queue_keyspec(shard, appdata_cb_ks_getnext, appdata_cb_ks_copy,
                                                          NULL, ctx, NULL);
        break;
      }
    case (APPDATA_PE):
      {
        rwdts_shard_handle_appdata_register_queue_pe(shard, appdata_cb_pe_getnext, appdata_cb_pe_copy,
                                                     NULL, ctx, NULL);
        break;
      }
    case (APPDATA_MK):
      {
        rwdts_shard_handle_appdata_register_queue_minikey(shard, appdata_cb_mk_getnext, appdata_cb_mk_copy,
                                                          NULL, ctx, NULL);
        break;
      }
    default:
        break;
  }

  if (s->reg_ready_called == 0) {
    appdata_create_keyspec_msg_db(ctx);
  }

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    appdata_test_getnext_api(ctx);
  }

  return;
}

static void appdata_getnext_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member Reg Ready %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.has_key00 = 1;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("9012345678");

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;

  switch (s->appdata_test) {
    case (APPDATA_SAFE): {
      reg_cb.cb.reg_ready_old = appdata_ready_safe_getnext;
      break;
    }
    case (APPDATA_UNSAFE): {
      reg_cb.cb.reg_ready_old = appdata_ready_unsafe_getnext;
      break;
    }
    case (APPDATA_QUEUE): {
      reg_cb.cb.reg_ready_old = appdata_ready_queue_getnext;
      break;
    }
  }

  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  EXPECT_TRUE(pub_regh);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

static void rwdts_clnt_query_noobj_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_obj_callback with status = %d\n", xact_status->status);

  ASSERT_EQ(s->prepare_cb_called, 3);

  // Call the transaction end
  s->exitnow = 1;
  return;
}
 
static void rwdts_clnt_query_obj_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_obj_callback with status = %d\n", xact_status->status);

  ASSERT_EQ(s->prepare_cb_called, 1);

  s->prepare_cb_called = 0;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony98801", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rwdts_xact_t* new_xact = NULL;

  uint32_t flags = 0;

  new_xact = rwdts_api_query_ks(clnt_apih,
                                keyspec,
                                RWDTS_QUERY_READ,
                                flags,
                                rwdts_clnt_query_noobj_callback,
                                ud,
                                NULL);
  ASSERT_TRUE(new_xact!=NULL);

  // Call the transaction end
  //s->exitnow = 1;

  return;
}

static rwdts_member_rsp_code_t
memberapi_test_obj_prepare(const rwdts_xact_info_t* xact_info,
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t*      key,
                           const ProtobufCMessage*  msg,
                           uint32_t credits,
                           void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->prepare_cb_called++;

  return RWDTS_ACTION_OK;
}

static void memberapi_obj_client_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony98801", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rwdts_xact_t* xact = NULL;

  uint32_t flags = RWDTS_XACT_FLAG_DEPTH_OBJECT;

  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_READ,
                          flags,
                          rwdts_clnt_query_obj_callback,
                          ctx,
                          NULL);
  ASSERT_TRUE(xact!=NULL);
  return;

}

static void
memberapi_test_obj_regready(rwdts_member_reg_handle_t regh,
                            const rw_keyspec_path_t*  ks,
                            const ProtobufCMessage*   msg,
                            void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_obj_client_start_f);
  }

  return;
}

static void memberapi_member_object_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_obj_regready;
  reg_cb.cb.prepare = memberapi_test_obj_prepare;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony98801", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    /* Establish a registration */
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED|RWDTS_FLAG_SUBOBJECT, NULL);
  } else {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  }

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void rwdts_clnt_query_subobj_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_obj_callback with status = %d\n", xact_status->status);

  ASSERT_EQ(s->prepare_cb_called, 2);

  // Call the transaction end
  s->exitnow = 1;

  return;
}

static void memberapi_subobj_client_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony6179", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1; 
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon6179", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_xact_t* xact = NULL;

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;
  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon6179", sizeof(nc->name)-1);
  nc->has_name = 1;


  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_CREATE,
                          RWDTS_XACT_FLAG_ADVISE,
                          rwdts_clnt_query_subobj_callback,
                          ctx,
                          &nc->base);
  ASSERT_TRUE(xact!=NULL);
  return;

}

static void
memberapi_test_subobj_regready(rwdts_member_reg_handle_t regh,
                               const rw_keyspec_path_t*  ks,
                               const ProtobufCMessage*   msg,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq,
                           &s->tenv->t[TASKLET_CLIENT],
                           memberapi_subobj_client_start_f);

  return;
}

static void memberapi_member_subobject_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_obj_prepare;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony6179", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    reg_cb.cb.reg_ready_old = memberapi_test_subobj_regready;
    /* Establish a registration */
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED|RWDTS_FLAG_SUBOBJECT, NULL);
  } else if (ti->member_idx == 1){
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_SUBSCRIBER, NULL);
  } else {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_OBJECT, NULL);
  }

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void rwdts_clnt_query_nosubobj_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_obj_callback with status = %d\n", xact_status->status);

  ASSERT_EQ(s->prepare_cb_called, 1);

  // Call the transaction end
  s->exitnow = 1;

  return;
}

static void memberapi_nosubobj_client_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path003.has_key00 = 1;
  keyspec_entry.dompath.path003.key00.instance_name = RW_STRDUP("instance1234");

  rwdts_xact_t* xact = NULL;

  RWPB_T_MSG(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo) *nc, nc_instance;
  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo, nc);

  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_CREATE,
                          RWDTS_XACT_FLAG_ADVISE,
                          rwdts_clnt_query_nosubobj_callback,
                          ctx,
                          &nc->base);
  RW_FREE(keyspec_entry.dompath.path003.key00.instance_name);
  ASSERT_TRUE(xact!=NULL);
  return;

}

static void
memberapi_test_nosubobj_regready(rwdts_member_reg_handle_t regh,
                                 const rw_keyspec_path_t*  ks,
                                 const ProtobufCMessage*   msg,
                                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq,
                           &s->tenv->t[TASKLET_CLIENT],
                           memberapi_nosubobj_client_start_f);

  return;
}

static void memberapi_member_nosubobject_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_obj_prepare;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo));

  RWPB_T_PATHSPEC(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path003.has_key00 = 1;
  keyspec_entry.dompath.path003.key00.instance_name = RW_STRDUP("instance1234");

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    reg_cb.cb.reg_ready_old = memberapi_test_nosubobj_regready;
    /* Establish a registration */
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  } else if (ti->member_idx == 1){
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo),
                          RWDTS_FLAG_SUBSCRIBER, NULL);
  } else {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_data_Tasklet_Info_RwcolonyList_ComponentInfo_TaskletInfo),
                          RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_OBJECT, NULL);
  }

  ck_pr_inc_32(&s->member_startct);

  RW_FREE(keyspec_entry.dompath.path003.key00.instance_name);
  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
member_advise_solicit_validate_test_results_and_exit(void *ctx)
{
  ASSERT_TRUE(ctx);
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  EXPECT_EQ(3, s->prepare_cb_called);
  EXPECT_EQ(1, s->query_cb_called);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)s->regh[ti->member_idx];

  TSTPRN("%s: member_advise_solicit_validate_test_results_and_exit (reg = 0x%p)\n",
          reg->apih->client_path, reg);

  RW_ASSERT_TYPE(reg, rwdts_member_registration_t);

  // EXPECT_EQ(HASH_COUNT(reg->pub_serials), 3);
  
  rwdts_pub_serial_t *ps, *tmp;
  HASH_ITER(hh, reg->pub_serials, ps, tmp) {
    RW_ASSERT_TYPE(ps, rwdts_pub_serial_t);
    EXPECT_EQ(reg, ps->reg);
    EXPECT_EQ(1, ps->serial);
  //  EXPECT_EQ(0, ps->id.member_id); // ATTN - Fix this populate member_id
    EXPECT_EQ(1, ps->id.router_id); // ATTN - Fix this populate router_id
//    EXPECT_EQ(0, ps->id.pub_id); // ATTN - Fix this populate pub_id
  }
  s->exitnow = 1;
  return;
}
static void
member_advise_solicit_rpc_cb(rwdts_xact_t*           xact,
                             rwdts_xact_status_t*    xact_status,
                             void*                   user_data)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t*             apih = xact->apih;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  s->query_cb_called++;
  RWPB_T_MSG(RwDts_output_SolicitAdvise)* rpc_out;

  TSTPRN("Member Advise solicit client callback ...\n");

  rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);
  ASSERT_TRUE(qrslt);
  rpc_out = (RWPB_T_MSG(RwDts_output_SolicitAdvise)*) (qrslt->message); 
  ASSERT_TRUE(rpc_out);
  
  if (s->prepare_cb_called == 3 && s->query_cb_called == 1) {
    member_advise_solicit_validate_test_results_and_exit(user_data);
  }
  return;
}

static rwdts_member_rsp_code_t
member_advise_solicit_client_prepare(const rwdts_xact_info_t* xact_info,
                                     RWDtsQueryAction         action,
                                     const rw_keyspec_path_t* key,
                                     const ProtobufCMessage*  msg,
                                     uint32_t                 credits,
                                     void*                    getnext_ptr)
{
  RW_ASSERT(xact_info);
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t*             apih = xact_info->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);

  rwdts_match_info_t *match  = (rwdts_match_info_t*)(xact_info->queryh);
  RW_ASSERT_TYPE(match, rwdts_match_info_t);

  RWDtsQuery *query = match->query;
  RW_ASSERT(query);

  TSTPRN(" Xact trigger is %d\n", query->flags&RWDTS_XACT_FLAG_SOLICIT_RSP);
  char xact_id_str[128];
  ++s->prepare_cb_called;
  TSTPRN("member_advise_solicit_client_prepare(xact = %p , xact_id = %s) %u\n",
          xact_info->xact, 
          rwdts_xact_id_str(&xact_info->xact->id,xact_id_str,128), 
          s->prepare_cb_called);

  TSTPRN("Query  serial rcvd = %lu:%lu:%lu:%lu\n", 
          query->serial->id.member_id, query->serial->id.router_id,
          query->serial->id.pub_id,query->serial->serial);

  if (s->prepare_cb_called == 3 && s->query_cb_called == 1) {
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1/10);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             ti,
                             member_advise_solicit_validate_test_results_and_exit);
  }

  return RWDTS_ACTION_OK;
}

static void
member_advise_solicit_client_reg_ready(rwdts_member_reg_handle_t  regh,
                                      rw_status_t                rs,
                                      void*                      user_data)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = s->tenv->t[TASKLET_CLIENT].apih;
  rwdts_xact_t*            xact;

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));

  TSTPRN("Member Advise solicit client regready ...\n");

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RWPB_T_PATHSPEC(RwDts_input_SolicitAdvise) solicit_advise_ks = *(RWPB_G_PATHSPEC_VALUE(RwDts_input_SolicitAdvise));
  RWPB_M_MSG_DECL_INIT(RwDts_input_SolicitAdvise, solicit_advise_msg);

  RWPB_T_MSG(RwDts_input_SolicitAdvise_Paths)* paths[1];

  RWPB_M_MSG_DECL_INIT(RwDts_input_SolicitAdvise_Paths, path1);

  solicit_advise_msg.n_paths = 1;
  solicit_advise_msg.paths = (RWPB_T_MSG(RwDts_input_SolicitAdvise_Paths)**)paths;
  solicit_advise_msg.paths[0] = &path1;
  path1.keyspec_str = (char*)"/ps:person";

  xact = rwdts_api_query_ks(apih,
                            (rw_keyspec_path_t*)&solicit_advise_ks,
                            RWDTS_QUERY_RPC,
                            RWDTS_XACT_FLAG_BLOCK_MERGE|RWDTS_XACT_FLAG_END,
                            member_advise_solicit_rpc_cb,
                            user_data,
                            &solicit_advise_msg.base);
  RW_ASSERT(xact);

  return;
}

static void
member_advise_solicit_client_start_f(void *ctx)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = s->tenv->t[TASKLET_CLIENT].apih;
  rw_keyspec_path_t*       keyspec;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  TSTPRN("Member Advise solicit client start...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready = member_advise_solicit_client_reg_ready;
  reg_cb.cb.prepare = member_advise_solicit_client_prepare;

  /* Establish a  subscriber registration */

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  s->regh[ti->member_idx] = rwdts_member_register(NULL,
                                   apih,
                                   keyspec,
                                   &reg_cb,
                                   RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                   RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE,
                                   NULL);
  EXPECT_TRUE( s->regh[ti->member_idx]);
  return;
}

static void
member_advise_solicit_reg_ready(rwdts_member_reg_handle_t  regh,
                                rw_status_t                rs,
                                void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("member_advise_solicit_reg_ready() called\n");

  rw_status_t rw_status;
  RWPB_T_PATHSPEC(DtsTest_Person) ks = (*RWPB_G_PATHSPEC_VALUE(DtsTest_Person));
  RWPB_T_MSG(DtsTest_Person) *person;
  person = (RWPB_T_MSG(DtsTest_Person)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Person)));

  RWPB_F_MSG_INIT(DtsTest_Person,person);

  person->name = strdup("TestName");
  person->has_id = TRUE;
  person->id = 1000;
  person->email = strdup("test@test.com");
  person->has_employed = TRUE;
  person->employed = TRUE;
  person->n_phone = 1;
  person->phone = (RWPB_T_MSG(DtsTest_PhoneNumber)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)*) * person->n_phone);
  person->phone[0] = (RWPB_T_MSG(DtsTest_PhoneNumber)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_PhoneNumber)));
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber,person->phone[0]);
  person->phone[0]->number = strdup("123-456-789");
  person->phone[0]->has_type = TRUE;
  person->phone[0]->type = DTS_TEST_PHONE_TYPE_MOBILE;

  rw_status = rwdts_member_reg_handle_create_element_keyspec(regh,(rw_keyspec_path_t*)&ks,
                                                             &person->base,NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rw_status);
  protobuf_c_message_free_unpacked(NULL, &person->base);

  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member advise solicit reg ready \n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             member_advise_solicit_client_start_f);
  }
  return;
}

static void member_advise_solicit_test_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member solicit advise test %d API start ...\n", ti->member_idx);

  RWPB_T_PATHSPEC(DtsTest_data_Person) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready = member_advise_solicit_reg_ready;

//  rwdts_member_deregister_all(apih);

  /* Establish a  publisher registration */
  rwdts_member_reg_handle_t pub_regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);
  pub_regh = rwdts_member_register(NULL,
                                   apih,
                                   keyspec,
                                   &reg_cb,
                                   RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
                                   NULL);

  EXPECT_TRUE(pub_regh);

}

static void member_gi_api_member_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member API GI  %d API start ...\n", ti->member_idx);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_reg_handle_t regh = NULL;

  rw_status_t rs;


  rs  = rwdts_api_member_register_xpath(apih,
					  "/ps:person",
					  NULL,
					  RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_SHARED,
            RW_DTS_SHARD_FLAVOR_NULL, NULL, 0,-1,0,0,
					  member_gi_api_test_reg_ready,
					  member_gi_api_test_prepare,
					  member_gi_api_test_precommit,
					  member_gi_api_test_commit,
					  member_gi_api_test_abort,
					  ctx,
					  &regh); 

  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_TRUE(regh);

  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member API GI - Starting client\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             member_gi_api_client_start_f);
  }

  TSTPRN("Member API GI startct =%u\n", s->member_startct);
}

static void
memberapi_test_anycast_regready(rwdts_member_reg_handle_t regh,
                                rw_status_t               rs,
                                void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == (TASKLET_MEMBER_CT * 2)) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_anycast_start_f);
  }

  return;
}

static void
memberapi_test_anycast_sec_regready(rwdts_member_reg_handle_t regh,
                                    rw_status_t               rs,
                                    void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) cpu_ks = *(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));
  keyspec1 = (rw_keyspec_path_t*)&cpu_ks;
  cpu_ks.dompath.path002.key00.id = 4567;
  cpu_ks.dompath.path002.has_key00 = 1;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu, cpu_instance;

  cpu = &cpu_instance;

  RWPB_F_MSG_INIT(TestdtsRwBase_VcsResource_Vm_Cpu,cpu);

  rs = rwdts_member_reg_handle_create_element_keyspec(regh, keyspec1, &cpu->base, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == (TASKLET_MEMBER_CT * 2)) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_anycast_start_f);
  }  
  return;
}

static void memberapi_member_anycast_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_anycast_prepare;
  reg_cb.cb.reg_ready = memberapi_test_anycast_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu)); 

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu)); 

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path002.key00.id = 1234;
  keyspec_entry.dompath.path002.has_key00 = 1;
  
  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_VcsResource_Vm_Cpu),
                        RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready = memberapi_test_anycast_sec_regready;
  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry2  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));
  keyspec = (rw_keyspec_path_t*)&keyspec_entry2;
  keyspec_entry2.dompath.path002.key00.id = 4567;
  keyspec_entry2.dompath.path002.has_key00 = 1;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_VcsResource_Vm_Cpu),
                                  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  ASSERT_TRUE(s->regh);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static rwdts_member_rsp_code_t memberapi_test_subread_prepare(const rwdts_xact_info_t* xact_info,
                                                              RWDtsQueryAction         action,
                                                              const rw_keyspec_path_t*      key,
                                                              const ProtobufCMessage*  msg,
                                                              uint32_t credits,
                                                              void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  static RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) cpu_instance = {};
  rwdts_member_query_rsp_t rsp;

  cpu = &cpu_instance;
  testdts_rw_base__yang_data__testdts_rw_base__vcs__resources__vm__cpu__init(cpu);

  RW_ASSERT(s);

  s->prepare_cb_called++;

  TSTPRN("Calling DTS Member prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;

  case TREATMENT_ACK:

    rsp.ks = (rw_keyspec_path_t*)key;

    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)&cpu;
    rsp.evtrsp  = RWDTS_EVTRSP_ACK;
    rwdts_member_send_error(xact,
                            key,
                            ((rwdts_match_info_t*)xact_info->queryh)->query,
                            NULL,
                            &rsp,
                            RW_STATUS_NOTFOUND,
                            "ERRORERRORERRORERRORERRORERRORERRORERRORERROR ");

    flags = rwdts_member_get_query_flags(xact_info->queryh);
    EXPECT_TRUE(flags|RWDTS_XACT_FLAG_ADVISE);

    rwdts_member_send_response(xact, xact_info->queryh, &rsp);

    return RWDTS_ACTION_OK;

  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
}

static void rwdts_clnt_query_subquery_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_subquery_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    ASSERT_EQ(s->prepare_cb_called, 3);
  }

  ASSERT_TRUE(xact->xres && xact->xres->dbg && xact->xres->dbg->tr && xact->xres->dbg->tr->n_ent);

  ASSERT_TRUE(xact->xres && xact->xres->dbg && xact->xres->dbg->err_report);
  rwdts_clnt_query_error(xact, true);

  // Call the transaction end
  s->exitnow = 1;

  return;
}

static void memberapi_client_subread_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client anycast start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path002.has_key00 = 1;
  keyspec_entry.dompath.path002.key00.id = 9854;

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  //RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  //cpu = (RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)));
  //RWPB_F_MSG_INIT(TestdtsRwBase_VcsResource_Vm_Cpu, cpu);

  uint32_t flags = RWDTS_XACT_FLAG_SUB_READ;
  flags |= RWDTS_XACT_FLAG_TRACE;

  fprintf(stderr, "[ Expect trace output here: ]\n");
  xact = rwdts_api_query_ks(clnt_apih,
                            keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            rwdts_clnt_query_subquery_callback,
                            ctx,
                            NULL);
                            //&cpu->base);
  ASSERT_TRUE(xact);
  ASSERT_TRUE(xact->trace);

  //RW_FREE(cpu);
  return;
}

static void
memberapi_test_subready_regready(rwdts_member_reg_handle_t regh,
                                 rw_status_t               rs,
                                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_subread_start_f);
  }

  return;
}

static void memberapi_member_subread_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_subread_prepare;
  reg_cb.cb.reg_ready = memberapi_test_subready_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path002.key00.id = 9854;
  keyspec_entry.dompath.path002.has_key00 = 1;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_VcsResource_Vm_Cpu),
                        RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}


static rwdts_member_rsp_code_t
memberapi_test_promote_sub_prepare(const rwdts_xact_info_t* xact_info,
                                   RWDtsQueryAction         action,
                                   const rw_keyspec_path_t*      key,
                                   const ProtobufCMessage*  msg,
                                   uint32_t credits,
                                   void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = xact->apih;

  RW_ASSERT(apih);

  RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  static RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) cpu_instance = {};
  rwdts_member_query_rsp_t rsp;

  cpu = &cpu_instance;
  testdts_rw_base__yang_data__testdts_rw_base__vcs__resources__vm__cpu__init(cpu);

  RW_ASSERT(s);

  s->prepare_cb_called++;

  TSTPRN("Calling DTS Member prepare ...\n");

  RW_ZERO_VARIABLE(&rsp);

  rsp.ks = (rw_keyspec_path_t*)key;

  rsp.n_msgs = 1;
  rsp.msgs = (ProtobufCMessage**)&cpu;
  rsp.evtrsp  = RWDTS_EVTRSP_ACK;
  rwdts_member_send_response(xact,
                             xact_info->queryh, &rsp);


  return RWDTS_ACTION_OK;
}

static void rwdts_clnt_query_promote_sub_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_anycast_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    ASSERT_EQ(s->prepare_cb_called, 3);
  }

  // Call the transaction end
  s->exitnow = 1;

  return;
}

static void memberapi_client_promote_sub_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client promote sub start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t *)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path002.has_key00 = 1;
  keyspec_entry.dompath.path002.key00.id = 4567;

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  //RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu) *cpu;
  //cpu = (RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)*)RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwBase_VcsResource_Vm_Cpu)));
  //RWPB_F_MSG_INIT(TestdtsRwBase_VcsResource_Vm_Cpu, cpu);

  uint32_t flags = 0;
  flags |= RWDTS_XACT_FLAG_TRACE;

  fprintf(stderr, "[ Expect trace output here: ]\n");
  xact = rwdts_api_query_ks(clnt_apih,
                            keyspec,
                            RWDTS_QUERY_READ,
                            flags,
                            rwdts_clnt_query_promote_sub_callback,
                            ctx,
                            NULL);
                            //&cpu->base);
  ASSERT_TRUE(xact);
  ASSERT_TRUE(xact->trace);

  //RW_FREE(cpu);
  return;
}

static void
memberapi_test_promote_sub_regready(rwdts_member_reg_handle_t regh,
                                    rw_status_t               rs,
                                    void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;

  ck_pr_inc_32(&s->member_startct);

  rwdts_member_shard_promote_to_publisher(reg->shard, reg->apih->client_path);
  rwdts_send_sub_promotion_to_router((void *)regh);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 8);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_promote_sub_start_f);
  }

  return;
}

static void memberapi_promote_sub_test_execute_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_promote_sub_prepare;
  reg_cb.cb.reg_ready = memberapi_test_promote_sub_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  RWPB_T_PATHSPEC(TestdtsRwBase_VcsResource_Vm_Cpu) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_VcsResource_Vm_Cpu));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path002.key00.id = 4567;
  keyspec_entry.dompath.path002.has_key00 = 1;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_VcsResource_Vm_Cpu),
                        RWDTS_FLAG_SUBSCRIBER, NULL);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_member_merge_reg_ready(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    TSTPRN("Member start_f invoking memberapi_client_merge_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_merge_start_f);
  }
  return;
}

static void
memberapi_member_test_empty_reg_ready(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member reg ready invoking client start_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1/3 * NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_empty_start_f);
  }

  return;
}

static void memberapi_member_merge_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_merg_prep;
  reg_cb.cb.reg_ready = memberapi_member_merge_reg_ready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "mergecolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "mergenccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);


  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_empty_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_empty_prep;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwStats_Routerstats));

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    reg_cb.cb.reg_ready = memberapi_member_test_empty_reg_ready;
  }
  rwdts_memberapi_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwStats_Routerstats),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);


  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}



static void
memberapi_test_refcnt_regready(rwdts_member_reg_handle_t regh,
                              const rw_keyspec_path_t*  ks,
                              const ProtobufCMessage*   msg,
                              void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq,
                           &s->tenv->t[TASKLET_CLIENT],
                           memberapi_client_refcnt_start_f);

  return;
}

static void memberapi_member_refcnt_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_empty_prep;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "refcntcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "refcntcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);


  /* Establish a registration */
  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    reg_cb.cb.reg_ready_old = memberapi_test_refcnt_regready;
  }
  rwdts_memberapi_register(NULL, apih, keyspec, &reg_cb, 
                           RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                           RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

}

static void memberapi_memb_rsp3(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_xact_info_t* xact_info = ti->saved_xact_info;
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));
  rw_keyspec_path_t* keyspec;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-async", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-async", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-async", sizeof(nc->name)-1);
  nc->has_name = 1;
  nc->n_interface = 1;
  nc->interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)**)
                   RW_MALLOC0(nc->n_interface * sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*));
  nc->interface[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[0]);
  strncpy(nc->interface[0]->name, "Interface3", sizeof(nc->interface[0]->name)-1);
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->interface_type.has_loopback = 1;

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs =(ProtobufCMessage**)&nc;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  RW_FREE(nc->interface[0]);
  RW_FREE(nc->interface);
  return;
}

static void memberapi_memb_rsp2(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_xact_info_t* xact_info = ti->saved_xact_info;
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));
  rw_keyspec_path_t* keyspec;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-async", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-async", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-async", sizeof(nc->name)-1);
  nc->has_name = 1;
  nc->n_interface = 1;
  nc->interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)**)
                   RW_MALLOC0(nc->n_interface * sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*));
  nc->interface[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[0]);
  strncpy(nc->interface[0]->name, "Interface2", sizeof(nc->interface[0]->name)-1);
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->interface_type.has_loopback = 1;

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs =(ProtobufCMessage**)&nc;
  rsp.evtrsp = RWDTS_EVTRSP_ASYNC;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);
  protobuf_c_message_free_unpacked_usebody(NULL, &nc->base);
  return;
}

static void memberapi_memb_rsp1(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_xact_info_t* xact_info = ti->saved_xact_info;
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));
  rw_keyspec_path_t* keyspec;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-async", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-async", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-async", sizeof(nc->name)-1);
  nc->has_name = 1;
  nc->n_interface = 1;
  nc->interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)**)
                   RW_MALLOC0(nc->n_interface * sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*));
  nc->interface[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[0]); 
  strncpy(nc->interface[0]->name, "Interface1", sizeof(nc->interface[0]->name)-1);
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->interface_type.has_loopback = 1;

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs =(ProtobufCMessage**)&nc;
  rsp.evtrsp = RWDTS_EVTRSP_ASYNC;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);

  return;
}

static rwdts_member_rsp_code_t
memberapi_test_async_prep(const rwdts_xact_info_t* xact_info,
                          RWDtsQueryAction         action,
                          const rw_keyspec_path_t*      key,
                          const ProtobufCMessage*  msg,
                          uint32_t credits,
                          void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ti->saved_xact_info = (rwdts_xact_info_t*) xact_info;

  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet,
                           when,
                           s->member[0].rwq,
                           ti,
                           memberapi_memb_rsp1);

  when = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet,
                           when,
                           s->member[0].rwq,
                           ti,
                           memberapi_memb_rsp2);

  when = dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet,
                           when,
                           s->member[0].rwq,
                           ti,
                           memberapi_memb_rsp3);

  return RWDTS_ACTION_ASYNC;
}

static void rwdts_clnt_query_async_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc = NULL;

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_query_empty_callback with status = %d\n", xact_status->status);

  rwdts_query_result_t* qres = rwdts_xact_query_result(xact, 0);
  
  s->total_queries++;

  ASSERT_TRUE(qres);
  if (xact_status->xact_done)
  {
     nc = ( RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *) qres->message;
    // Call the transaction end
    
    EXPECT_EQ(nc->n_interface, 3);
    EXPECT_STREQ(nc->interface[0]->name, "Interface1");
    EXPECT_STREQ(nc->interface[1]->name, "Interface2");
    EXPECT_STREQ(nc->interface[2]->name, "Interface3");

    TSTPRN("total_queries is %u\n", s->total_queries);
    if (s->total_queries < 1) {
      memberapi_client_async_start_f(ud);
    } else {
      s->exitnow = 1;
    }
  }
  return;
}

static void memberapi_client_async_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("MemberAPI test client expand start...\n");

  rw_keyspec_path_t *keyspec;
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-async", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-async", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_xact_t* xact = NULL;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  uint32_t flags = 0;

  xact = rwdts_api_query_ks(clnt_apih,
                          keyspec,
                          RWDTS_QUERY_READ,
                          flags,
                          rwdts_clnt_query_async_callback,
                          ctx,
                          NULL);
  ASSERT_TRUE(xact!=NULL);
  return;
}

static void
memberapi_test_async_regready(rwdts_member_reg_handle_t regh,
                              const rw_keyspec_path_t*  ks,
                              const ProtobufCMessage*   msg,
                              void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           s->client.rwq,
                           &s->tenv->t[TASKLET_CLIENT],
                           memberapi_client_async_start_f);
  return;
}



static void memberapi_member_async_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;
  RW_ZERO_VARIABLE(&reg_cb);
  int i;
  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  rwdts_member_deregister_all(apih);
#undef LOOP1
#define LOOP1 1000
  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t *keyspec2;
  rw_keyspec_path_t *keyspec3;
  rw_keyspec_path_t *keyspec4;
  rw_keyspec_path_t *keyspec5;

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entry1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd1  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd2  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd3  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  RWPB_T_PATHSPEC(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour) keyspec_entrywd4  =
                   (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec2 = (rw_keyspec_path_t*)&keyspec_entrywd1;
  keyspec3 = (rw_keyspec_path_t*)&keyspec_entrywd2;
  keyspec4 = (rw_keyspec_path_t*)&keyspec_entrywd3;
  keyspec5 = (rw_keyspec_path_t*)&keyspec_entrywd4;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec3, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec4, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec5, NULL, RW_SCHEMA_CATEGORY_DATA);

  for(i = 0; i < LOOP1; i++) {
    keyspec_entry1.dompath.path000.has_key00 = 1;
    keyspec_entry1.dompath.path000.key00.key_one = i;
    keyspec_entry1.dompath.path001.has_key00 = 1;
    keyspec_entry1.dompath.path001.key00.key_two = i;
    keyspec_entry1.dompath.path002.has_key00 = 1;
    keyspec_entry1.dompath.path002.key00.key_three = i;
    keyspec_entry1.dompath.path003.has_key00 = 1;
    keyspec_entry1.dompath.path003.key00.key_four = i;

   rwdts_memberapi_register(NULL, apih, keyspec1, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = LOOP1; i < 2*LOOP1; i++) {
    keyspec_entrywd1.dompath.path000.has_key00 = 1;
    keyspec_entrywd1.dompath.path000.key00.key_one = i;
    keyspec_entrywd1.dompath.path001.has_key00 = 1;
    keyspec_entrywd1.dompath.path001.key00.key_two = i;
    keyspec_entrywd1.dompath.path002.has_key00 = 1;
    keyspec_entrywd1.dompath.path002.key00.key_three = i;

   rwdts_memberapi_register(NULL, apih, keyspec2, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = 2*LOOP1; i < 3*LOOP1; i++) {
    keyspec_entrywd2.dompath.path000.has_key00 = 1;
    keyspec_entrywd2.dompath.path000.key00.key_one = i;
    keyspec_entrywd2.dompath.path001.has_key00 = 1;
    keyspec_entrywd2.dompath.path001.key00.key_two = i;

   rwdts_memberapi_register(NULL, apih, keyspec3, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  }

  for (i = 3*LOOP1; i < 4*LOOP1; i++) {
    keyspec_entrywd3.dompath.path000.has_key00 = 1;
    keyspec_entrywd3.dompath.path000.key00.key_one = i;

   rwdts_memberapi_register(NULL, apih, keyspec4, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_LevelOne_LevelTwo_LevelThree_LevelFour),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  }

  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_async_prep;
  reg_cb.cb.reg_ready_old = memberapi_test_async_regready;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-async", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-async", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_memberapi_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void 
memberapi_test_audit_member_ready(rwdts_member_reg_handle_t  regh,
                                  const rw_keyspec_path_t*   keyspec,
                                  const ProtobufCMessage*    msg, 
                                  void*                      ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s = (struct queryapi_state *)ti->ctx;

  ck_pr_inc_32(&s->member_startct);
  TSTPRN("member %d: memberapi_test_audit_member_ready() is called\n", s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member audit start_f invoking client start_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_audit_start_f);
  }
  return;
}
static void
publish_reg_ready(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*  ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s = (struct queryapi_state *)ti->ctx;

  //Create an element
  RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

  char phone_num[] = "4523178609";

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  kpe.key00.number= phone_num;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;
  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  rw_status_t rs = rwdts_member_reg_handle_create_element(
                    regh,gkpe,&phone->base,NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // Get the created element
  //
  rs = rwdts_member_reg_handle_get_element(regh,
                                          gkpe,
                                          NULL,
                                          NULL,
                                          (const ProtobufCMessage**)&tmp);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_TRUE(tmp);
  EXPECT_STREQ(phone->number, tmp->number);
  EXPECT_EQ(phone->type, tmp->type);
  EXPECT_EQ(phone->has_type, tmp->has_type);

  ck_pr_inc_32(&s->reg_ready_called);

  if (s->reg_ready_called == 3) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1/3 * NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_publish_start_f);
  }
}


static void memberapi_test_audit_tests_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s = (struct queryapi_state *)ti->ctx;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API Audit tests start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_audit_member_ready;
  reg_cb.cb.prepare = memberapi_test_audit_prep;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry = 
    (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  snprintf(keyspec_entry.dompath.path000.key00.name, sizeof(keyspec_entry.dompath.path000.key00.name),
           "mycolony%d",ti->member_idx+1);
//  keyspec_entry.dompath.path001.has_key00 = 1;
//  snprintf(keyspec_entry.dompath.path001.key00.name, sizeof(keyspec_entry.dompath.path001.key00.name)
//          "mycontext%d", ti->member_idx+1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_reg_handle_t pub_regh;
  pub_regh = rwdts_member_register(NULL,
                                   apih,
                                   keyspec,
                                   &reg_cb,
                                   RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  ASSERT_TRUE(pub_regh);
  s->regh[ti->tasklet_idx] = pub_regh;




  TSTPRN("Member audit start_f ending, startct=%u\n", s->member_startct);
}

static void*
memberapi_sub_app_II_xact_init(rwdts_appconf_t* ac,
                            rwdts_xact_t*    xact,
                            void*            ud)
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  TSTPRN("memberapi_sub_app_II_xact_init[%d] called\n", myapp->ti->member_idx);

  return RW_MALLOC0_TYPE(sizeof(myapp_audit_scratch_t), myapp_audit_scratch_t);
} 

static void
memberapi_sub_app_II_xact_deinit(rwdts_appconf_t* ac,
                                   rwdts_xact_t*    xact,
                                   void*            ud,
                                   void*            scratch_in)
{     
  myappconf_t *myapp = (myappconf_t *)ud;
  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);

  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  TSTPRN("memberapi_sub_app_II_xact_deinit[%d] called\n", myapp->ti->member_idx);
  RW_FREE_TYPE(scratch, myapp_audit_scratch_t);

  return;
}

static void
memberapi_sub_app_II_config_validate(rwdts_api_t*     apih,
                                       rwdts_appconf_t* ac,
                                       rwdts_xact_t*    xact,
                                       void*            ctx,
                                       void*            scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t*)ctx;
  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);

  TSTPRN("memberapi_sub_app_II_config_validate() called\n");
  return;
}

static void rwdts_sub_app_set_exit_now(void *ctx)
{

  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  s->exitnow = 1;

  return;
}

static void
memberapi_sub_app_II_config_apply(rwdts_api_t*           apih,
                                    rwdts_appconf_t*       ac,
                                    rwdts_xact_t*          xact,
                                    rwdts_appconf_action_t action,
                                    void*                  ctx,
                                    void*                  scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t *)ctx;
  //myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;

  ASSERT_TRUE(apih);
  ASSERT_TRUE(ac);
  ASSERT_TRUE(myapp);
  struct queryapi_state *s = (struct queryapi_state *)myapp->s;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("memberapi_sub_app_II_config_apply() called\n");
  
  if (action == RWDTS_APPCONF_ACTION_RECONCILE) {
    TSTPRN("memberapi_sub_app_II_config_apply() called with action RWDTS_APPCONF_ACTION_RECONCILE\n");
  }

  myapp->s->prepare_cb_called++;

  if (myapp->s->prepare_cb_called == 1) {
    ASSERT_EQ(s->committed, 24);
    if (!s->abort) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC);
      rwsched_dispatch_after_f(apih->tasklet,
                               when,
                               myapp->s->member[myapp->ti->member_idx].rwq,
                               myapp->s,
                               rwdts_sub_app_set_exit_now);    
    }
  }

  return;
}

static void rwdts_sub_app_prep_II_cb(rwdts_api_t *apih,
          rwdts_appconf_t *ac,
          rwdts_xact_t *xact,
          const rwdts_xact_info_t *xact_info,
          rw_keyspec_path_t *keyspec,
          const ProtobufCMessage *pbmsg,
          void *ctx,
          void *scratch_in)
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  TSTPRN("rwdts_sub_app_prep_II_cb[%d] called, reg=%p\n", myapp->ti->member_idx, xact_info->regh);

  rwdts_appconf_prepare_complete_ok(ac, xact_info);
  return;
}

static void*
memberapi_sub_app_xact_init(rwdts_appconf_t* ac,
                            rwdts_xact_t*    xact,
                            void*            ud)
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  if ((myapp->ti->member_idx == 0) && (!myapp->s->sec_reg_done)) {
    rwdts_api_t *apih = myapp->ti->apih;
    RW_ASSERT(apih);

    TSTPRN("memberapi_sub_app_xact_init[%d] called\n", myapp->ti->member_idx);

    rw_keyspec_path_t* keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

    RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

    keyspec = (rw_keyspec_path_t*)&keyspec_entry;
    keyspec_entry.dompath.path000.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
    keyspec_entry.dompath.path001.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path001.key00.name, "appcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

    myappconf_t *myapp1 = &myapp->s->member[myapp->ti->member_idx].sec_appconf;
    memset(myapp1, 0, sizeof(*myapp1));
    myapp1->s = myapp->s;
    myapp1->ti = myapp->ti;
    myapp1->member = &myapp->s->member[myapp->ti->member_idx];

    /* Create appconf group */
    rwdts_appconf_cbset_t cbset = { };
    cbset.xact_init = memberapi_sub_app_II_xact_init;
    cbset.xact_deinit = memberapi_sub_app_II_xact_deinit;
    cbset.config_validate = memberapi_sub_app_II_config_validate;
    cbset.config_apply = memberapi_sub_app_II_config_apply;
    cbset.ctx = myapp1;

    rw_status_t rs; 
    rs = rwdts_api_appconf_group_create_gi(myapp->ti->apih, xact,
                                    (appconf_xact_init_gi)cbset.xact_init,
                                    (appconf_xact_deinit_gi)cbset.xact_deinit,
                                    (appconf_config_validate_gi)cbset.config_validate,
                                    (appconf_config_apply_gi)cbset.config_apply,
                                    cbset.ctx,
                                    &myapp1->conf1.ac,
                                    NULL,NULL,NULL,NULL);
                                    
    RW_ASSERT(RW_STATUS_SUCCESS==rs);

    rw_keyspec_path_t* lks = NULL;
    rw_keyspec_path_create_dup(keyspec, NULL, &lks);
    RW_ASSERT(lks);
    rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
    rwdts_appconf_register(myapp1->conf1.ac,
                           lks,
                           NULL,
                           RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                           RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, // flags
                           rwdts_sub_app_prep_II_cb);

    rw_keyspec_path_free(lks, NULL);

    myapp->s->sec_reg_done = true;

    /* Say we're finished */
    rwdts_appconf_phase_complete(myapp1->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);
  }

  return RW_MALLOC0_TYPE(sizeof(myapp_audit_scratch_t), myapp_audit_scratch_t);
}

static void 
memberapi_sub_app_xact_deinit(rwdts_appconf_t* ac,
                                   rwdts_xact_t*    xact,
                                   void*            ud,
                                   void*            scratch_in)
{      
  myappconf_t *myapp = (myappconf_t *)ud;
  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);
   
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  RW_FREE_TYPE(scratch, myapp_audit_scratch_t);

  return;
}

static void
memberapi_sub_app_config_validate(rwdts_api_t*     apih,
                                       rwdts_appconf_t* ac,
                                       rwdts_xact_t*    xact,
                                       void*            ctx,
                                       void*            scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t*)ctx;
  myapp_audit_scratch_t* scratch = (myapp_audit_scratch_t*)scratch_in;
  struct queryapi_state *s = (struct queryapi_state *)myapp->s;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  
  ASSERT_TRUE(myapp);
  ASSERT_TRUE(scratch);

  s->committed++;
  TSTPRN("memberapi_sub_app_config_validate() called commited = %u\n", s->committed);
  return;
}

static void
memberapi_sub_app_config_apply(rwdts_api_t*           apih,
                                    rwdts_appconf_t*       ac,
                                    rwdts_xact_t*          xact,
                                    rwdts_appconf_action_t action,
                                    void*                  ctx,
                                    void*                  scratch_in)
{
  myappconf_t*           myapp   = (myappconf_t *)ctx;

  ASSERT_TRUE(apih);
  ASSERT_TRUE(ac);
  ASSERT_TRUE(myapp);
  struct queryapi_state *s = (struct queryapi_state *)myapp->s;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);


  if (action == RWDTS_APPCONF_ACTION_RECONCILE) {
    TSTPRN("memberapi_sub_app_config_apply() called with action RWDTS_APPCONF_ACTION_RECONCILE\n");
  }

  if (action == RWDTS_APPCONF_ACTION_INSTALL) {
    ck_pr_inc_32(&s->member_startct);

    if (s->member_startct == TASKLET_MEMBER_CT) {
      /* Start client after all members are going */
      TSTPRN("Member start_f invoking client start_f\n");
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet, 
                               s->client.rwq,
                               &s->tenv->t[TASKLET_CLIENT],
                               memberapi_client_sub_app_start_f);
    }
  }



  /* Was it a container delete?  Some checks for the object being gone as viewed in the xact... */
  if (myapp->s->delete_test) {
    if (s->started) {
      RW_ASSERT(xact);
      int expected_contexts = myapp->s->Dleftover;

      TSTPRN("sub_app_config_apply() in member_idx=%d state dump IN XACT:\n", myapp->ti->member_idx);
      memberapi_dumpmember(myapp->ti, xact);
      ASSERT_EQ(expected_contexts, memberapi_dumpmember_nc(myapp->ti, xact, NULL));
      TSTPRN("sub_app_config_apply() in member_idx=%d state dump COMMITTED/NULL xact:\n", myapp->ti->member_idx);
      memberapi_dumpmember(myapp->ti, NULL);
      ASSERT_EQ((myapp->s->MultipleAdvise ? 1 : 2), memberapi_dumpmember_nc(myapp->ti, NULL, NULL));
    }
  }


  if (s->abort) {
    rwdts_appconf_xact_add_issue(ac, xact, RW_STATUS_FAILURE, "Trying out the error case");
  }
  return;
}

static void rwdts_sub_app_prep_cb(rwdts_api_t *apih,
          rwdts_appconf_t *ac,
          rwdts_xact_t *xact,
          const rwdts_xact_info_t *xact_info,
          rw_keyspec_path_t *keyspec,
          const ProtobufCMessage *pbmsg,
          void *ctx,
          void *scratch_in)
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  TSTPRN("rwdts_sub_app_prep_cb[%d] called, reg=%p\n", myapp->ti->member_idx, xact_info->regh);

  rwdts_appconf_prepare_complete_ok(ac, xact_info);

  return;
}

static rwdts_member_rsp_code_t
memberapi_client_sub_app_prep(const rwdts_xact_info_t* xact_info,
                         RWDtsQueryAction         action,
                         const rw_keyspec_path_t*      key,
                         const ProtobufCMessage*  msg,
                         uint32_t credits,
                         void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  TSTPRN("Inside memberapi_client_sub_app_prep ..............\n");
  rwdts_member_query_rsp_t rsp;
  memset(&rsp, 0, sizeof(rwdts_member_query_rsp_t));
#if 0
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element

  TSTPRN("Started publishing the data from memberapi_pub_app_regready \n");

  RWPB_T_PATHENTRY(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) kpe = *(RWPB_G_PATHENTRY_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
#endif

  rw_keyspec_path_t *keyspec;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "appcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "Interface1", sizeof(keyspec_entry.dompath.path002.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *interface, interface_instance;

  interface = &interface_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,interface);

  strncpy(interface->name, "Interface1", sizeof(interface->name)-1);
  interface->has_name = 1;
  interface->has_interface_type = 1;
  interface->interface_type.loopback = 1;
  interface->interface_type.has_loopback = 1;

  rsp.n_msgs = 1;
  rsp.ks = keyspec;
  rsp.msgs = (ProtobufCMessage**)&interface;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  rwdts_member_send_response(xact, xact_info->queryh, &rsp);

  return RWDTS_ACTION_OK;

} 

static void
memberapi_pub_app_cb(rwdts_xact_t* xact,
                     rwdts_xact_status_t* status,
                     void*         ud)
{
  struct queryapi_state *s = (struct queryapi_state *)ud;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  if (s->abort) {
    // Check for errors and report
    rwdts_clnt_query_error(xact, true);
    if (!s->exitnow) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, when,
	                            s->member[0].rwq, &s->exitnow, set_exitnow);
    }
  }
   
  TSTPRN("Inside memberapi_pub_app_cb ............\n");
  return;
}

static void
memberapi_pub_app_del_fin_cb(rwdts_xact_t* xact, 
                             rwdts_xact_status_t* status,
                             void*         ud)   
{
  struct queryapi_state *s = (struct queryapi_state *)ud; 
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->exit_soon--;
  if (!s->exit_soon) {
    s->exitnow = 1;
  }

  TSTPRN("Inside memberapi_pub_app_del_fin_cb ............\n");
  return;
}

static void
memberapi_pub_app_del_cb(rwdts_xact_t* xact,
                         rwdts_xact_status_t* status,
                         void*         ud)
{
  TSTPRN("memberapi_pub_app_del_cb block=%p responses_done=%d xact_done=%d\n",
	  status->block,
	  status->responses_done,
	  status->xact_done);
  
  if (!status->xact_done) {
    return;
  }

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->started=TRUE;

  memberapi_dumpmember(&s->tenv->t[TASKLET_MEMBER_0], NULL);
  memberapi_dumpmember(&s->tenv->t[TASKLET_MEMBER_1], NULL);
  memberapi_dumpmember(&s->tenv->t[TASKLET_MEMBER_2], NULL);

  if (s->MultipleAdvise) {
    int member_objct;
    member_objct = memberapi_get_objct(&s->tenv->t[TASKLET_MEMBER_0]);
    ASSERT_EQ(1, member_objct);
    member_objct = memberapi_get_objct(&s->tenv->t[TASKLET_MEMBER_1]);
    ASSERT_EQ(1, member_objct);
    member_objct = memberapi_get_objct(&s->tenv->t[TASKLET_MEMBER_2]);
    ASSERT_EQ(1, member_objct);
  }

  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);

  TSTPRN("Delete the data from memberapi_pub_app_del_cb \n");

  rw_keyspec_path_t *keyspec;

  int Dlevel = s->Dlevel;

  RW_ASSERT(Dlevel >= -4);
  RW_ASSERT(Dlevel <= 4);

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry_if  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry_nc  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony) keyspec_entry_ctx  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony));

  const char *colony = "appdelcolony";
  if (Dlevel < 0) {
    Dlevel = -Dlevel;
    colony = "appdelcolony-ixnay";
  }
  const char *ctxname = "appdelcontext1";
  if (s->MultipleAdvise) {
    ctxname = "appdelcontext";
  }

  if (Dlevel >= 3) {
    keyspec = (rw_keyspec_path_t*)&keyspec_entry_if;
    keyspec_entry_if.dompath.path000.has_key00 = 1;
    strncpy(keyspec_entry_if.dompath.path000.key00.name, colony, sizeof(keyspec_entry_if.dompath.path000.key00.name) - 1);
    keyspec_entry_if.dompath.path001.has_key00 = 1;

    strncpy(keyspec_entry_if.dompath.path001.key00.name, ctxname, sizeof(keyspec_entry_if.dompath.path001.key00.name) - 1);

    if (Dlevel >= 4) {
      char ifname[100];
      sprintf(ifname, "Interface%d", 2);
      keyspec_entry_if.dompath.path002.has_key00 = 1;
      strncpy(keyspec_entry_if.dompath.path002.key00.name, ifname, sizeof(keyspec_entry_if.dompath.path002.key00.name) - 1);
    } else {
      // interface[name=*]
    }

  } else if (Dlevel >= 1) {

    keyspec = (rw_keyspec_path_t*)&keyspec_entry_nc;

    // colony[whatever]
    keyspec_entry_nc.dompath.path000.has_key00 = 1;
    strncpy(keyspec_entry_nc.dompath.path000.key00.name, colony, sizeof(keyspec_entry_nc.dompath.path000.key00.name) - 1);

    if (Dlevel >= 2) {
      // context[appdelcontext]
      keyspec_entry_nc.dompath.path001.has_key00 = 1;
      strncpy(keyspec_entry_nc.dompath.path001.key00.name, ctxname, sizeof(keyspec_entry_nc.dompath.path001.key00.name) - 1);
    } else {
      // context[*]
    }

  } else { 
    keyspec = (rw_keyspec_path_t*)&keyspec_entry_ctx;

    keyspec_entry_ctx.dompath.path000.has_key00 = 1;
    strncpy(keyspec_entry_ctx.dompath.path000.key00.name, colony, sizeof(keyspec_entry_nc.dompath.path000.key00.name) - 1);
  }

  uint32_t flags = 0;

  rwdts_event_cb_t advice_cb = {};
  advice_cb.cb = memberapi_pub_app_del_fin_cb;
  advice_cb.ud = s;

  flags |= RWDTS_XACT_FLAG_END; //xact_parent = NULL

  if (getenv("RWDTS_XACT_FLAG_TRACE")) {
    flags |= RWDTS_XACT_FLAG_TRACE;
  }

  if (1) {
    char *ks_str = NULL;
    rw_keyspec_path_get_new_print_buffer(keyspec, NULL, rwdts_api_get_ypbc_schema(ti->apih), &ks_str);
    RW_ASSERT(ks_str);
    TSTPRN("Sending Dlevel=%d DELETE, path '%s'\n", Dlevel, ks_str);
    free(ks_str);
  }

  /* This is a new xact.  The previous one has completed. */
  rwdts_advise_query_proto_int(apih,
                               keyspec,
                               NULL,
                               NULL,
                               1080,
                               NULL,
                               RWDTS_QUERY_DELETE,
                               &advice_cb,
                               flags,
                               NULL, NULL);

  return;
}

static void
memberapi_pub_app_regready(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*  ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element

  TSTPRN("Started publishing the data from memberapi_pub_app_regready tasklet_idx=%d\n", ti->tasklet_idx);

  rw_keyspec_path_t *keyspec;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  keyspec_entry.dompath.path001.has_key00 = 1;
  const char *ctxpfx;
  if (s->delete_test) {
    ctxpfx = "appdelcontext";
    strncpy(keyspec_entry.dompath.path000.key00.name, "appdelcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  } else {
    ctxpfx = "appcontext";
    strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  }

  int j;
  for (j=1; j<=2; j++) {
    char ctxstr[99]; 
    (s->delete_test)?sprintf(ctxstr, "%s%d", ctxpfx, j):
                     sprintf(ctxstr, "%s", ctxpfx);
    strncpy(keyspec_entry.dompath.path001.key00.name, ctxstr, sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

    int i;
    for (i=1; i<=4; i++) {
      
      char ifname[100];
      sprintf(ifname, "Interface%d", i);//ti->tasklet_idx);
      
      keyspec_entry.dompath.path002.has_key00 = 1;
      strncpy(keyspec_entry.dompath.path002.key00.name, ifname, sizeof(keyspec_entry.dompath.path002.key00.name) - 1);
      
      
      RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *interface, interface_instance;
      
      interface = &interface_instance;
      
      RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,interface);
      
      strncpy(interface->name, ifname, sizeof(interface->name)-1);
      interface->has_name = 1;
      interface->has_interface_type = 1;
      interface->interface_type.loopback = 1;
      interface->interface_type.has_loopback = 1;
      
     uint32_t flags = 0;
 
     rwdts_event_cb_t advice_cb = {};
     if (i==4 && j == 2) {
       if (s->delete_test) {
        advice_cb.cb = memberapi_pub_app_del_cb;
        advice_cb.ud = ctx;
       } else {
        advice_cb.cb = memberapi_pub_app_cb;
        advice_cb.ud = s;
       }
     }
 
     flags |= RWDTS_XACT_FLAG_END; //xact_parent = NULL
 
    rwdts_advise_query_proto_int(apih,
                                 keyspec,
                                 NULL,
                                 &interface->base,
                                 1080,
                                 NULL,
                                 RWDTS_QUERY_UPDATE,
                                 &advice_cb,
                                 flags,
                                 NULL, NULL);



      
    }
  }

  return;
}


static void
memberapi_pub_app_advise_regready(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*  ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element

  TSTPRN("Started publishing the data from memberapi_pub_app_advise_regready tasklet_idx=%d\n", ti->tasklet_idx);

  rw_keyspec_path_t *keyspec;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  keyspec_entry.dompath.path001.has_key00 = 1;
  const char *ctxpfx;
  if (s->delete_test) {
    ctxpfx = "appdelcontext";
    strncpy(keyspec_entry.dompath.path000.key00.name, "appdelcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  } else {
    ctxpfx = "appcontext";
    strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  }

  int j;
  for (j=1; j<=2; j++) {
    char ctxstr[99]; 
//  (s->delete_test)?sprintf(ctxstr, "%s%d", ctxpfx, j):
                     sprintf(ctxstr, "%s", ctxpfx);
    strncpy(keyspec_entry.dompath.path001.key00.name, ctxstr, sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

    int i;
    for (i=1; i<=4; i++) {
      
      char ifname[100];
      sprintf(ifname, "Interface%d", i);//ti->tasklet_idx);
      
      keyspec_entry.dompath.path002.has_key00 = 1;
      strncpy(keyspec_entry.dompath.path002.key00.name, ifname, sizeof(keyspec_entry.dompath.path002.key00.name) - 1);
      
      
      RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *interface, interface_instance;
      
      interface = &interface_instance;
      
      RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,interface);
      
      strncpy(interface->name, ifname, sizeof(interface->name)-1);
      interface->has_name = 1;
      interface->has_interface_type = 1;
      interface->interface_type.loopback = 1;
      interface->interface_type.has_loopback = 1;
      
     uint32_t flags = 0;
 
     rwdts_event_cb_t advice_cb = {};
     if (i==4 && j == 2) {
       if (s->delete_test) {
        advice_cb.cb = memberapi_pub_app_del_cb;
        advice_cb.ud = ctx;
       } else {
        advice_cb.cb = memberapi_pub_app_cb;
        advice_cb.ud = s;
       }
     }
 
     flags |= RWDTS_XACT_FLAG_END; //xact_parent = NULL
 
     rwdts_advise_query_proto_int(apih,
                                 keyspec,
                                 NULL,
                                 &interface->base,
                                 1080,
                                 NULL,
                                 RWDTS_QUERY_CREATE,
                                 &advice_cb,
                                 flags,
                                 NULL, NULL);



      
    }
  }

  return;
}

static void
memberapi_client_sub_app_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state   *s  = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("client  () memberapi_client_sub_app_start_f() called\n");
  TSTPRN("MemberAPI test client aub-app start ...\n");

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  if (s->MultipleAdvise) {
    reg_cb.cb.reg_ready_old = memberapi_pub_app_advise_regready;
  }
  else {
    reg_cb.cb.reg_ready_old = memberapi_pub_app_regready;
  }
  reg_cb.cb.prepare = memberapi_client_sub_app_prep;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  keyspec_entry.dompath.path001.has_key00 = 1;
  if (s->delete_test) {
    strncpy(keyspec_entry.dompath.path000.key00.name, "appdelcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
    strncpy(keyspec_entry.dompath.path001.key00.name, "appdelcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  } else {
    strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
    strncpy(keyspec_entry.dompath.path001.key00.name, "appcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  }

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, clnt_apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
						   RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_CACHE|RWDTS_FLAG_SHARED, NULL);

  return;
}

static void memberapi_sub_appconf_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "appcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "appcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = memberapi_sub_app_xact_init;
  cbset.xact_deinit = memberapi_sub_app_xact_deinit;
  cbset.config_validate = memberapi_sub_app_config_validate;
  cbset.config_apply = memberapi_sub_app_config_apply;
  cbset.ctx = myapp;

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  rw_keyspec_path_t* lks = NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  myapp->conf1.reg_nmap = rwdts_appconf_register(myapp->conf1.ac,
						 lks,
						 NULL,
						 RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
						 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, // flags
						 rwdts_sub_app_prep_cb);
  
  rw_keyspec_path_free(lks, NULL);

  ASSERT_TRUE(myapp->conf1.reg_nmap);
  s->regh[ti->tasklet_idx] = myapp->conf1.reg_nmap;

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);


  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

  

static void rwdts_app_del_prep_cb(rwdts_api_t *apih,
                                  rwdts_appconf_t *ac,
                                  rwdts_xact_t *xact,
                                  const rwdts_xact_info_t *xact_info,
                                  rw_keyspec_path_t *keyspec,
                                  const ProtobufCMessage *pbmsg,
                                  void *ctx,
                                  void *scratch_in)
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  TSTPRN("rwdts_app_del_prep_cb[%d] called, reg=%p\n", myapp->ti->member_idx, xact_info->regh);

  ASSERT_TRUE(pbmsg);

  /* Figure out if the whole object is deleted */
  ProtobufCFieldReference *fref = protobuf_c_field_ref_alloc(NULL);
  protobuf_c_field_ref_goto_whole_message(fref, (/*cheat*/ProtobufCMessage*)pbmsg);
  gboolean obj_del = protobuf_c_field_ref_is_field_deleted(fref);
  if (obj_del) {
    /* Level 4 is interfac(es) inside the object */
    ASSERT_GE(3, myapp->s->Dlevel);
  }
  protobuf_c_field_ref_unref(fref);
  fref = NULL;

  int nc_ct_xact = memberapi_dumpmember_nc(myapp->ti, xact, NULL);
  int nc_ct_comm = memberapi_dumpmember_nc(myapp->ti, NULL, NULL);

  TSTPRN("rwdts_app_del_prep_cb[member_idx=%d] called, Dlevel=%d, per fref whole msg obj_del=%d, nc ct, xact=%d comm=%d\n", myapp->ti->member_idx, myapp->s->Dlevel, obj_del, nc_ct_xact, nc_ct_comm);


  rwdts_appconf_prepare_complete_ok(ac, xact_info);
  return;
}

static void*
memberapi_app_del_xact_init(rwdts_appconf_t* ac,
                            rwdts_xact_t*    xact,
                            void*            ud)
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(myapp->s->testno == myapp->s->tenv->testno);

  return RW_MALLOC0_TYPE(sizeof(myapp_audit_scratch_t), myapp_audit_scratch_t);
}





static void memberapi_appconf_delete_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "appdelcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  if (s->MultipleAdvise) {
    keyspec_entry.dompath.path001.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path001.key00.name, "appdelcontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  }

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = memberapi_app_del_xact_init;
  cbset.xact_deinit = memberapi_sub_app_xact_deinit;
  cbset.config_validate = memberapi_sub_app_config_validate;
  cbset.config_apply = memberapi_sub_app_config_apply;
  cbset.ctx = myapp;

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  rw_keyspec_path_t* lks = NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  s->regh[ti->tasklet_idx] = rwdts_appconf_register(myapp->conf1.ac,
						    lks,
						    NULL,
						    RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
						    RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DELTA_READY, // flags
						    rwdts_app_del_prep_cb);
  RW_ASSERT(s->regh[ti->tasklet_idx]);

  rw_keyspec_path_free(lks, NULL);

  //??!!  ASSERT_TRUE(&myapp->conf1.reg_nmap);
  //??!!  s->regh[ti->tasklet_idx] = myapp->conf1.reg_nmap;

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);


  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_publish_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_publish_prep;
  reg_cb.cb.reg_ready_old = publish_reg_ready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("4523178609");
  keyspec_entry.dompath.path001.has_key00 = 1;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_memberapi_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                           RWDTS_FLAG_NO_PREP_READ|RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_test_append_regready(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->reg_ready_called++;
  if (s->reg_ready_called == (s->multi_router?TASKLET_MEMBER_CT_NEW: TASKLET_MEMBER_CT)) {
    TSTPRN("Member start_f invoking memberapi_client_append_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_append_start_f);
  }
  return;
}

static void memberapi_member_append_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  if(!s->multi_router && (ti->member_idx >= TASKLET_MEMBER_CT)) {
    TSTPRN("Multi Router members disabled for non multi_router case\n");
    return;
  }
  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  if (!ti->api_state_change_called) {
    TSTPRN("API not readt, bailing\n");
    return;
  }

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_append_prepare;
  reg_cb.cb.reg_ready = memberapi_test_append_regready;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  RWPB_T_PATHSPEC(DtsTest_data_MultiPerson_MultiPhone) keyspec_m_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_MultiPerson_MultiPhone));

                      
  const ProtobufCMessageDescriptor* desc;
  if (s->multi_router) {
    keyspec = (rw_keyspec_path_t*)&keyspec_m_entry;
    desc = RWPB_G_MSG_PBCMD(DtsTest_data_MultiPerson_MultiPhone);
  }
  else {
    keyspec = (rw_keyspec_path_t*)&keyspec_entry;
    desc = RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone);
  }

  if (s->client.wildcard) {
    // wildcard reg, we expect both the primary and appended block to go somewhere useful
  } else {
    // specific reg, we expect the appended child block to go nowhere and return right away...
    keyspec_entry.dompath.path001.key00.number = RW_STRDUP("1234567890");
    keyspec_entry.dompath.path001.has_key00 = 1;
  }

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration -- GAH pub/sub reg doesn't work, does it? */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, desc,
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  ck_pr_inc_32(&s->member_startct);

  if (keyspec_entry.dompath.path001.has_key00 ) {
    RW_FREE(keyspec_entry.dompath.path001.key00.number);
  }
  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_test_transreg_regready(rwdts_member_reg_handle_t regh,
                                 rw_status_t               rs,
                                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->reg_ready_called++;
  if (s->reg_ready_called == 3) {
    TSTPRN("Member start_f invoking client start_f\n");

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 2*NSEC_PER_SEC/1);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_transreg_start_f);
  } 
  return;
}

static void memberapi_member_transreg_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_transreg_prepare;
  reg_cb.cb.reg_ready = memberapi_test_transreg_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("9876543210");
  keyspec_entry.dompath.path001.has_key00 = 1;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  ck_pr_inc_32(&s->member_startct);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_shared_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rw_keyspec_path_t *keyspec1;
  rwdts_member_event_cb_t reg_cb;
  rwdts_member_event_cb_t reg_cb1;
  uint32_t flags = RWDTS_FLAG_PUBLISHER;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  RW_ZERO_VARIABLE(&reg_cb1);
  reg_cb.ud = ctx;
  reg_cb.cb.prepare = memberapi_test_shared_prepare;
  reg_cb.cb.reg_ready = memberapi_member_shared_reg_ready;
  reg_cb1.ud = ctx;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category));
  keyspec1 = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));

  RWPB_T_PATHSPEC(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;

  keyspec_entry.dompath.path002.key00.name = TESTDTS_RW_BASE_CATEGORY_TYPE_FASTPATH;
  keyspec_entry.dompath.path002.has_key00 = 1;

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  if (ti->member_idx != 1) {
    flags |= RWDTS_FLAG_SHARED;
  }

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_memberapi_register(NULL, apih,keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwBase_TestdtsRwBase_data_Logging_Filter_Category),
                           flags, NULL);

  rwdts_memberapi_register(NULL, apih, keyspec1, &reg_cb1,
                           RWPB_G_MSG_PBCMD(DtsTest_Person),
                           RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

}


static void memberapi_pub_sub_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
//   cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub sub notify ...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  rwdts_member_deregister_all(clnt_apih);

  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);

  {
    //Create an element
    RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

    char phone_num2[] = "987-654-321";

    rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
    kpe.has_key00 = 1;
    kpe.key00.number= phone_num2;

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;
    rw_keyspec_path_t *get_keyspec = NULL;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    uint32_t count = rwdts_member_data_get_count(NULL, regh);
    EXPECT_EQ(0, count);

    /* The below code (create_list..) is commented to check if the update_list
     * with replace flag creates the an element. The
     * rwdts_member_reg_handle_create_element APi is called implicitly from
     * update_list...
     * API */
    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_status_t rs = rwdts_member_reg_handle_create_element(
                    regh,gkpe,&phone->base,NULL);

    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the created element
    //

    rs = rwdts_member_reg_handle_get_element(regh,
                                             gkpe,
                                             NULL,
                                             NULL,
                                             (const ProtobufCMessage**)&tmp);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    ASSERT_TRUE(tmp);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);
    //
    // Update the list element
    //
   
    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;

    rs = rwdts_member_reg_handle_update_element(regh,
                                                gkpe,
                                                &phone->base,
                                                RWDTS_XACT_FLAG_REPLACE,
                                                NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the updated element and verify the update
    //

    rs = rwdts_member_reg_handle_get_element(regh,
                                             gkpe,
                                             NULL,
                                             &get_keyspec,
                                             (const ProtobufCMessage**)&tmp);
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    ASSERT_TRUE(tmp);
    ASSERT_TRUE(get_keyspec != NULL);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    //  Get the current list count
    //

    count = rwdts_member_data_get_count(NULL, regh);
    EXPECT_EQ(1, count);
   // ATTN deletes need work
    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_reg_handle_delete_element(regh,
                                               gkpe,
                                               &phone->base,
                                               NULL);


    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
   
    //
    // DO a get again
    //

    get_keyspec = NULL;
    rs = rwdts_member_reg_handle_get_element(regh,
                                             gkpe,
                                             NULL,
                                             &get_keyspec,
                                             (const ProtobufCMessage**)&tmp);
    EXPECT_EQ(rs, RW_STATUS_FAILURE);
    EXPECT_FALSE(tmp);
    ASSERT_TRUE(get_keyspec == NULL);
  }

  { // Async  version tests
    //Create an element
    RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

    char phone_num1[] = "978-456-789";
    rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
    kpe.has_key00 = 1;
    kpe.key00.number= phone_num1;

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_status_t rs = rwdts_member_reg_handle_create_element(
                      regh,gkpe,&phone->base,NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Update the list element
    //
   
    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;

    rs = rwdts_member_reg_handle_update_element(regh,
                                               gkpe,
                                               &phone->base,
                                               RWDTS_XACT_FLAG_REPLACE,
                                               NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);


    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_reg_handle_delete_element(regh,
                                               gkpe,
                                               &phone->base,
                                               NULL);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  }

  // The test would be done when all the members have received the publish callback
  // Check for completiom

  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
  rwsched_dispatch_after_f(clnt_apih->tasklet,
                           when,
                           clnt_apih->client.rwq,
                           s,
                           rwdts_set_exit_now);
}

static void 
subscriber_ready(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  s->subs_ready++;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  TSTPRN("--> %s:subscriber_ready %s\n",
         ((rwdts_member_registration_t *)regh)->apih->client_path,
         ((rwdts_member_registration_t *)regh)->keystr);

}

static void
subscriber_ready_sub_b_pub(rwdts_member_reg_handle_t regh,
                           rw_status_t               rs,
                           void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  s->subs_ready++;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_sub_before_pub_client_start_f);
  }

  TSTPRN("--> %s:subscriber_ready_sub_b_pub %s\n",
         ((rwdts_member_registration_t *)regh)->apih->client_path,
         ((rwdts_member_registration_t *)regh)->keystr);

}

static void 
memberapi_cache_updated(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*  ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("--> %s:memberapi_cache_updated %s\n", 
         ((rwdts_member_registration_t *)regh)->apih->client_path,
         ((rwdts_member_registration_t *)regh)->keystr);

  ASSERT_TRUE(regh);
  ASSERT_TRUE(ks);
  ASSERT_TRUE(msg);
  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone;
  phone = (RWPB_T_MSG(DtsTest_data_Person_Phone) *)msg;
  ASSERT_TRUE(phone);
  EXPECT_TRUE(phone->has_type);
  EXPECT_EQ(DTS_TEST_PHONE_TYPE_MOBILE, phone->type);
  EXPECT_STREQ("123-985-789", phone->number);
}

static void
publisher_ready(rwdts_member_reg_handle_t regh,
                        const rw_keyspec_path_t*  ks,
                        const ProtobufCMessage*   msg,
                        void*                     ctx)
{
  //Create an element
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));
  s->publisher_ready++;

  char phone_num[] = "123-985-789";

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  kpe.key00.number= phone_num;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;
  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  TSTPRN("Publisher is Ready and creating data SUBS = %d\n", s->subs_ready);
  rw_status_t rs = rwdts_member_reg_handle_create_element(
                    regh,gkpe,&phone->base,NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // Get the created element
  //

  rs = rwdts_member_reg_handle_get_element(regh,
                                           gkpe,
                                           NULL,
                                           NULL,
                                           (const ProtobufCMessage**)&tmp);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  ASSERT_TRUE(tmp);
  EXPECT_STREQ(phone->number, tmp->number);
  EXPECT_EQ(phone->type, tmp->type);
  EXPECT_EQ(phone->has_type, tmp->has_type);
}

static void memberapi_sub_after_pub_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  //cb.cb.prepare = publisher_data_prepare;
  cb.cb.reg_ready_old = publisher_ready;

  TSTPRN("starting pub before sub client start ...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  if (!s->client_dreg_done) {
    rwdts_member_deregister_all(clnt_apih);
    s->client_dreg_done = 1;
  }


  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_memberapi_register(NULL, clnt_apih, 
                              keyspec, 
                              &cb, 
                              RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                              RWDTS_FLAG_CACHE|RWDTS_FLAG_PUBLISHER, NULL);
  s->regh[ti->tasklet_idx] = regh;
  //printf("%d CLIENT   LIST = %p\n",ti->tasklet_idx, s->regh[ti->tasklet_idx]);

}


static void memberapi_sub_before_pub_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  TSTPRN("All Subs ready(%d/%d) . !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",  s->subs_ready, s->subs_created);
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  //cb.cb.prepare = publisher_data_prepare;
  cb.cb.reg_ready_old = publisher_ready;

  TSTPRN("starting pub before sub client start ...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  if (!s->client_dreg_done) {
    rwdts_member_deregister_all(clnt_apih);
    s->client_dreg_done = 1;
  }

  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
  s->regh[ti->tasklet_idx] = regh;
  //printf("%d CLIENT   LIST = %p\n",ti->tasklet_idx, s->regh[ti->tasklet_idx]);

}
static void memberapi_pub_sub_minikey_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub sub notify ...\n");

  rw_keyspec_path_t *keyspec1;
  rw_keyspec_path_t * keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;
  keyspec1=NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &keyspec1);

  rwdts_member_deregister_all(clnt_apih);

  clnt_apih->stats.num_member_advise_rsp = 0;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  RwSchemaCategory cat = rw_keyspec_path_get_category((rw_keyspec_path_t*)keyspec1); 
  RW_ASSERT(cat == RW_SCHEMA_CATEGORY_DATA);

  regh = rwdts_member_register(NULL, clnt_apih,
                              keyspec1,
                              &cb,
                              RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                              RWDTS_FLAG_PUBLISHER, NULL);

  {
    //Create an element
    char phone_num1[] = "123-456-789";

    RWPB_M_MK_DECL_INIT(DtsTest_data_Person_Phone, mk);
    mk.k.key = RW_STRDUP("123-456-789");
    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    rw_status_t rs = rwdts_member_data_create_list_element_mk(NULL,
                                                              regh,
                                                              (rw_schema_minikey_opaque_t *)&mk,
                                                              &phone->base);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the created element
    //

    tmp = (const RWPB_T_MSG(DtsTest_data_Person_Phone)*)rwdts_member_data_get_list_element_mk(NULL, regh, (rw_schema_minikey_opaque_t *)&mk);
    ASSERT_TRUE(tmp);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    // Update the list element
    //

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;

    rs = rwdts_member_data_update_list_element_mk(NULL,
                                                  regh,
                                                  (rw_schema_minikey_opaque_t *)&mk,
                                                  &phone->base,
                                                  RWDTS_XACT_FLAG_REPLACE);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Get the updated element and verify the update
    //

    tmp = (const RWPB_T_MSG(DtsTest_data_Person_Phone)*)rwdts_member_data_get_list_element_mk(NULL, regh, (rw_schema_minikey_opaque_t *)&mk);

    ASSERT_TRUE(tmp);
    EXPECT_STREQ(phone->number, tmp->number);
    EXPECT_EQ(phone->type, tmp->type);
    EXPECT_EQ(phone->has_type, tmp->has_type);

    //
    //  Get the current list count
    //

    uint32_t count = rwdts_member_data_get_count(NULL, regh);
    EXPECT_EQ(1, count);

    //
    // Delete the updated element and verify it is gone
    //

    rs = rwdts_member_data_delete_list_element_mk(NULL,
                                                  regh,
                                                  (rw_schema_minikey_opaque_t *)&mk,
                                                  &phone->base);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // DO a get again
    //

    tmp = (const RWPB_T_MSG(DtsTest_data_Person_Phone)*)rwdts_member_data_get_list_element_mk(NULL, regh, (rw_schema_minikey_opaque_t *)&mk);
    EXPECT_FALSE(tmp);
    RW_FREE(mk.k.key);
  }
  { // Async  version tests
    //Create an element

    char phone_num1[] = "978-456-789";

    RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

    phone = &phone_instance;

    RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

    DtsTest__YangData__DtsTest__Person__Phone__MiniKey *mk = (DtsTest__YangData__DtsTest__Person__Phone__MiniKey *)RW_MALLOC0(sizeof(DtsTest__YangData__DtsTest__Person__Phone__MiniKey));
    dts_test__yang_data__dts_test__person__phone__mini_key__init(mk);
    mk->k.key = RW_STRDUP("978-456-789");
    rw_status_t rs = rwdts_member_data_create_list_element_mk(NULL,
                                                             regh,
                                                             (rw_schema_minikey_opaque_t *)mk,
                                                             &phone->base);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);

    //
    // Update the list element
    //

    phone->number = phone_num1;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_HOME;

    rs = rwdts_member_data_update_list_element_mk(NULL,
                                                  regh,
                                                  (rw_schema_minikey_opaque_t *)mk,
                                                  &phone->base,
                                                  RWDTS_XACT_FLAG_REPLACE);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);


    //
    // Delete the updated element and verify it is gone
    //
    rs = rwdts_member_data_delete_list_element_mk(NULL,
                                                  regh,
                                                  (rw_schema_minikey_opaque_t *)mk,
                                                  &phone->base);
    RW_FREE(mk->k.key);
    RW_FREE(mk);
    EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  }

  rw_keyspec_path_free(keyspec1, NULL);
  // The test would be done when all the members have received the publish callback
  // Check for completiom
  clnt_apih->stats.num_member_advise_rsp = 0;
  rwsched_dispatch_async_f(clnt_apih->tasklet,
                           clnt_apih->client.rwq,
                           s,
                           rwdts_set_exit_now);
}

static void memberapi_dummy_cb_fn(rwdts_xact_s*        xact,
                                  rwdts_xact_status_t* xact_status,
                                  void*                user_data)
{
  RW_ASSERT_TYPE(xact, rwdts_xact_t);
  RW_ASSERT_TYPE(xact->apih, rwdts_api_t);
  memberdata_api_corr_t rcvd = *((memberdata_api_corr_t *)user_data);
  struct queryapi_state *s = rcvd.s;
  s->memberapi_dummy_cb_correlation_rcvd++;
  EXPECT_EQ(s->memberapi_dummy_cb_correlation_rcvd, rcvd.correlate_sent);
}

static void
memberapi_pub_sub_regready_part2(rwdts_member_reg_handle_t regh,
                                 rw_status_t               rs,
                                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_Person) person_ks = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec1 = (rw_keyspec_path_t*)&person_ks;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);

  char person_name[] = "Parimal Tripathi";

  RWPB_T_MSG(DtsTest_data_Person) *person, person_instance;

  person = &person_instance;


  RWPB_F_MSG_INIT(DtsTest_data_Person,person);

  person->name = person_name;

  rs = rwdts_member_reg_handle_create_element_keyspec(regh, keyspec1, &person->base, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_delete_element_keyspec(regh, keyspec1, NULL, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  char *person_name1 = RW_STRDUP("Sukumar Sinha");

  RWPB_F_MSG_INIT(DtsTest_data_Person,person);

  person->name = person_name1;

  rs = rwdts_member_reg_handle_create_element_keyspec(regh, keyspec1, &person->base, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  person->has_id = TRUE;
  person->id = 123456;
  rs = rwdts_member_reg_handle_update_element_keyspec(regh, keyspec1, &person->base, 0, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_delete_element_keyspec(regh, keyspec1, NULL, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  RW_FREE(person_name1);

  person_name1 = RW_STRDUP("Raj Kumar");
  RWPB_F_MSG_INIT(DtsTest_data_Person,person);
  person->name = person_name1;

  static struct memberdata_api_corr_s correlate_sent1 = {
    .correlate_sent = 1,
    .s = s
  };

  rwdts_event_cb_t advise_cb = {
    .cb = memberapi_dummy_cb_fn,
    .ud = &correlate_sent1
  };
  rs = rwdts_member_data_block_advise_w_cb (regh, RWDTS_QUERY_CREATE,
                                            keyspec1, &person->base,
                                            NULL, &advise_cb);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  person->has_id = TRUE;
  person->id = 654321;
  static struct memberdata_api_corr_s correlate_sent2 = {
    .correlate_sent = 2,
    .s = s
  };

  advise_cb.ud = &correlate_sent2;
  rs = rwdts_member_data_block_advise_w_cb (regh, RWDTS_QUERY_UPDATE,
                                            keyspec1, &person->base,
                                            NULL, &advise_cb);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  static struct memberdata_api_corr_s correlate_sent3 = {
    .correlate_sent = 3,
    .s = s
  };

  advise_cb.ud = &correlate_sent3;
  rs = rwdts_member_data_block_advise_w_cb (regh, RWDTS_QUERY_DELETE,
                                            keyspec1, &person->base,
                                            NULL, &advise_cb);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  RW_FREE(person_name1);

  // The test would be done when all the members have received the publish callback
  // Check for completiom
  clnt_apih->stats.num_member_advise_rsp = 0;

  TSTPRN("%s:%d s->exitnow=%d\n", __PRETTY_FUNCTION__, __LINE__, s->exitnow);
  if (!s->exitnow) {
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, when,
       s->member[0].rwq, s, rwdts_set_exit_now);
  }
  TSTPRN("%s:%d s->exitnow=%d\n", __PRETTY_FUNCTION__, __LINE__, s->exitnow);
  return;
}

static void
memberapi_pub_sub_regready(rwdts_member_reg_handle_t regh,
                           rw_status_t               rs_status,
                           void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_member_event_cb_t cb;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = memberapi_pub_sub_regready_part2;

  //Create an element
  RWPB_T_PATHENTRY(DtsTest_data_Person_Phone) kpe = *(RWPB_G_PATHENTRY_VALUE(DtsTest_data_Person_Phone));

  char phone_num1[] = "123-456-789";
  char phone_num2[] = "987-654-321";

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  kpe.key00.number= phone_num1;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  const RWPB_T_MSG(DtsTest_data_Person_Phone) *tmp;
  rw_keyspec_path_t *get_keyspec = NULL;

  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num1;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  TSTPRN("Creating list element in  Member pub sub Cache notify ...\n");
  rw_status_t rs = rwdts_member_reg_handle_create_element(
                    regh,gkpe,&phone->base,NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // Get the created element
  //

  rs = rwdts_member_reg_handle_get_element(regh,
                                           gkpe,
                                           NULL,
                                           &get_keyspec,
                                           (const ProtobufCMessage**)&tmp);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  ASSERT_TRUE(tmp);
  ASSERT_TRUE(get_keyspec != NULL);
  EXPECT_STREQ(phone->number, tmp->number);
  EXPECT_EQ(phone->type, tmp->type);
  EXPECT_EQ(phone->has_type, tmp->has_type);

  //
  // Update the list element
  //

  phone->number = phone_num2;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  kpe.key00.number= phone_num2;

  TSTPRN("Updating list element in  Member pub sub Cache notify ...\n");
  rs = rwdts_member_reg_handle_update_element(regh,
                                             gkpe,
                                             &phone->base,
                                             0,
                                             NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // Get the updated element and verify the update
  //

  rs = rwdts_member_reg_handle_get_element(regh,
                                           gkpe,
                                           NULL,
                                           &get_keyspec,
                                           (const ProtobufCMessage**)&tmp);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  ASSERT_TRUE(tmp);
  ASSERT_TRUE(get_keyspec != NULL);
  EXPECT_STREQ(phone->number, tmp->number);
  EXPECT_EQ(phone->type, tmp->type);
  EXPECT_EQ(phone->has_type, tmp->has_type);

  //
  //  Get the current list count
  //

  uint32_t count = rwdts_member_data_get_count(NULL, regh);
  EXPECT_EQ(2, count);

  phone->number = phone_num2;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_HOME;

  TSTPRN("Updating list element in  Member pub sub Cache notify ...\n");
  rs = rwdts_member_reg_handle_update_element(regh,
                                             gkpe,
                                             &phone->base,
                                             0,
                                             NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // Get the updated element and verify the update
  //

  rs = rwdts_member_reg_handle_get_element(regh,
                                           gkpe,
                                           NULL,
                                           &get_keyspec,
                                           (const ProtobufCMessage**)&tmp);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  ASSERT_TRUE(tmp);
  ASSERT_TRUE(get_keyspec != NULL);
  EXPECT_STREQ(phone->number, tmp->number);
  EXPECT_EQ(phone->type, tmp->type);
  EXPECT_EQ(phone->has_type, tmp->has_type);

  //
  //  Get the current list count
  //

  count = rwdts_member_data_get_count(NULL, regh);
  EXPECT_EQ(2, count);

  // Delete not supported in cache mode for now
  TSTPRN("Deleting list element in  Member pub sub Cache notify ...\n");
  //
  // Delete the updated element and verify it is gone
  //
  rs = rwdts_member_reg_handle_delete_element(regh,
                                             gkpe,
                                             &phone->base,
                                             NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  //
  // DO a get again
  //

  get_keyspec = NULL;
  rs = rwdts_member_reg_handle_get_element(regh,
                                           gkpe,
                                           NULL,
                                           &get_keyspec,
                                           (const ProtobufCMessage**)&tmp);
  EXPECT_EQ(RW_STATUS_FAILURE, rs);
  EXPECT_FALSE(tmp);
  ASSERT_TRUE(get_keyspec == NULL);

  rw_keyspec_path_t *keyspec1;
  RWPB_T_PATHSPEC(DtsTest_data_Person) person_ks = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec1 = (rw_keyspec_path_t*)&person_ks;
  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);


  /* Establish a registration */

  rwdts_memberapi_register(NULL, clnt_apih,
                           keyspec1,
                           &cb,
                           RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                           RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  return;
}

static void memberapi_pub_sub_cache_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
//  cb.cb.prepare = memberapi_test_advice_prepare;
  cb.cb.reg_ready = memberapi_pub_sub_regready;

  TSTPRN("starting client for Member pub sub Cache notify ...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  rwdts_member_deregister_all(clnt_apih);

  clnt_apih->stats.num_member_advise_rsp = 0;
  regh = rwdts_memberapi_register(NULL, clnt_apih,
                                  keyspec,
                                  &cb,
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
  RW_ASSERT(regh != NULL);

  return;
}

static void memberapi_pub_sub_non_listy_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_status_t rs;

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  //cb.ud = ctx;
  //cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub sub Cache notify ...\n");

  rw_keyspec_path_t *keyspec; 
  RWPB_T_PATHSPEC(DtsTest_data_Person) person_ks = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec = (rw_keyspec_path_t*)&person_ks;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  clnt_apih->stats.num_member_advise_rsp = 0;

  rwdts_member_deregister_all(clnt_apih);

  regh = rwdts_memberapi_register(NULL, clnt_apih,
                                  keyspec,
                                  &cb,
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);

  char person_name[] = "Parimal Tripathi";

  RWPB_T_MSG(DtsTest_data_Person) *person, person_instance;

  person = &person_instance;


  RWPB_F_MSG_INIT(DtsTest_data_Person,person);

  person->name = person_name;

  rs = rwdts_member_reg_handle_create_element_keyspec(regh, keyspec, &person->base, NULL);
  

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  person->has_id = TRUE;
  person->id = 123456;
  rs = rwdts_member_reg_handle_update_element_keyspec(regh, keyspec, &person->base, 0, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rs = rwdts_member_reg_handle_delete_element_keyspec(regh, keyspec, NULL, NULL);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  // The test would be done when all the members have received the publish callback
  // Check for completiom
  clnt_apih->stats.num_member_advise_rsp = 0;
  ASSERT_EQ(s->exitnow, false);
  rwsched_dispatch_async_f(clnt_apih->tasklet,
                           clnt_apih->client.rwq,
                           s,
                           rwdts_set_exit_now);
}

static void memberapi_pub_sub_cache_xact_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  rwdts_member_event_cb_t cb;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  // cb.cb.prepare = memberapi_test_advice_prepare;

  TSTPRN("starting client for Member pub sub Cache notify ...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone)  keyspec_entry  = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*)&keyspec_entry;

    /* Establish a registration */
  rwdts_member_reg_handle_t regh;

  rwdts_member_deregister_all(clnt_apih);

  regh = rwdts_memberapi_register(NULL, clnt_apih,
                                  keyspec,
                                  &cb,
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
  RW_ASSERT(regh != NULL);
  char phone_num1[] = "123-456-789";
  char phone_num2[] = "987-654-321";

  keyspec_entry.dompath.path001.has_key00 = TRUE;
  keyspec_entry.dompath.path001.key00.number = phone_num1;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num1;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  rwdts_xact_t* xact = NULL;
  uint64_t corrid = 1000;
  rw_status_t rs;
  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;
  if (!s->client.transactional) {
    flags |= RWDTS_XACT_FLAG_NOTRAN ;
  }
  flags |= RWDTS_XACT_FLAG_ADVISE;

  if (!s->client.usebuilderapi) {
    flags |= RWDTS_XACT_FLAG_END;// xact_parent = NULL
    rs = rwdts_advise_query_proto_int(clnt_apih,
                                      keyspec,
                                      NULL, // xact_parent
                                      &(phone->base),
                                      corrid,
                                      NULL, // Debug
                                      RWDTS_QUERY_CREATE,
                                      &clnt_cb,
                                      flags,
                                      NULL, NULL);
    ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  } else {
    uint64_t corrid = 999;

    xact = rwdts_api_xact_create(clnt_apih, flags, rwdts_clnt_query_callback, ctx);
   ASSERT_TRUE(xact);
   rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
   ASSERT_TRUE(blk);
   rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
                      flags,corrid,&(phone->base));
   ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

    uint64_t corrid1 = 9998;
    phone->number = phone_num2;
    phone->has_type = TRUE;
    phone->type = DTS_TEST_PHONE_TYPE_MOBILE;
    keyspec_entry.dompath.path001.key00.number = phone_num2;

    rs = rwdts_xact_block_add_query_ks(blk, keyspec, s->client.action, flags, corrid1, &(phone->base));
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

    rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);

    rs = rwdts_xact_commit(xact);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }
}

static void
memberapi_test_regready_reroot(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                            s->client.rwq,
                            &s->tenv->t[TASKLET_CLIENT],
                            memberapi_client_start_reroot_f);
  }
  return;
}

static void memberapi_member_start_multi_xact_reroot_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member xact reroot %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_test_prepare_reroot;
  cb.cb.reg_ready = memberapi_test_regready_reroot;


  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &cb, 
        RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool),
                        RWDTS_FLAG_SUBSCRIBER, NULL);

  TSTPRN("Member tart_multi_xact_reroot_f ending, startct=%u\n", s->member_startct);
}

static void memberapi_member_start_xact_iter_reroot_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec1, *keyspec2;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member xact iter reroot %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_test_prepare_reroot;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool) keyspec_entry2  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry1 = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec_entry1.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry1.dompath.path000.key00.name) - 1);
  keyspec_entry1.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry1.dompath.path001.key00.name) - 1);

  keyspec2 = (rw_keyspec_path_t*)&keyspec_entry2;
  keyspec_entry2.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry2.dompath.path000.key00.name) - 1);
  keyspec_entry2.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry2.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
  rw_keyspec_path_set_category(keyspec2, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  s->nc_regh[ti->member_idx] = rwdts_member_register(NULL, apih, keyspec1, &cb, 
                     RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                     RWDTS_FLAG_SUBSCRIBER, NULL);
  cb.cb.reg_ready = memberapi_test_regready_reroot;
  s->ip_nat_regh[ti->member_idx] = rwdts_member_register(NULL, apih, keyspec2, &cb, 
                RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Ip_NatPool),
                RWDTS_FLAG_SUBSCRIBER, NULL);

  TSTPRN("Member start_multi_xact_reroot_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_multi_xact_reg_ready(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->reg_ready_called++;
  TSTPRN("Member %d memberapi_multi_xact_reg_ready invoked...\n", ti->member_idx);
  if (s->reg_ready_called == 3) {
    TSTPRN("Member start_f invoking memberapi_client_start_expand_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_client_start_expand_f);
  }
  return;
}

static void memberapi_member_start_multi_xact_expand_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d expand API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = memberapi_multi_xact_reg_ready;
  cb.cb.prepare = memberapi_test_prepare_reroot;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "testcolony", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "testnccontext", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);
  /* Establish a registration */
  rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                        RWDTS_FLAG_SUBSCRIBER, NULL);

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_multi_xact_expand_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_receive_dereg_regready(rwdts_member_reg_handle_t regh,
                                 rw_status_t               rs,
                                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);
  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_reg_dereg_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_reg_dereg_client_start_f);
  }

  return;
}

static void memberapi_reg_dereg_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Reg Dereg  Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_dereg_notification;
  cb.cb.reg_ready = memberapi_receive_dereg_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  rwdts_memberapi_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                           RWDTS_FLAG_SUBSCRIBER, NULL);

  apih->dereg_end = 0;

  TSTPRN("Member member API Reg DeReg member start ending , startct=%u\n", s->member_startct);
}

static void memberapi_pub_sub_cache_kn_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub  Cache KN Notify start ...\n", ti->member_idx);
  
  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  rwdts_member_deregister_all(apih);
  /* Establish a registration */

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  s->regh[ti->tasklet_idx] = rwdts_memberapi_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                                      RWDTS_FLAG_SUBSCRIBER, NULL);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_cache_kn_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_cache_kn_client_start_f);
  }
  TSTPRN("Member member API pub sub cache KN member start ending , startct=%u\n", s->member_startct);
}

static void memberapi_update_merge_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Update Merge start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  //rwdts_member_deregister(apih, keyspec);

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_Fastpath),
                        RWDTS_FLAG_SUBSCRIBER , NULL);
  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_client_start_f\n");
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_update_merge_client_start_f);
  }
  TSTPRN("Member member API pub sub member start ending , startct=%u\n", s->member_startct);
}

static void
memberapi_test_create2_client_cb(rwdts_xact_t* xact,
                                 rwdts_xact_status_t* xact_status,
                                 void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];

  const RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *tmp;
  rw_keyspec_path_t *elem_ks = NULL;
  uint32_t count = 0;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  while ((tmp = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*) rwdts_member_reg_handle_get_next_element(
                                                                                                regh,
                                                                                                cur,
                                                                                                NULL,
                                                                                                &elem_ks))) {
    EXPECT_STREQ(tmp->name, "nc-create");
    EXPECT_EQ(tmp->n_interface, 2);
    EXPECT_STREQ(tmp->interface[0]->name, "interface1");
    EXPECT_STREQ(tmp->interface[1]->name, "interface2");
    count++;
  }
  rwdts_member_data_delete_cursors(NULL, regh);

  EXPECT_EQ(count, 1);

  s->exitnow = 1;

  return;
}

static void
memberapi_test_create1_client_cb(rwdts_xact_t* xact,
                                 rwdts_xact_status_t* xact_status,
                                 void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rw_keyspec_path_t* keyspec = NULL;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-create", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nc-create", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "interface2", sizeof(keyspec_entry.dompath.path002.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *inter, inter_instance;

  inter = &inter_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,inter);

  strncpy(inter->name, "interface2", sizeof(inter->name)-1);
  inter->has_name = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_create2_client_cb,
                                              ud, &inter->base);
  ASSERT_TRUE(new_xact);

  return;
}


static void
memberapi_test_create_regready(rwdts_member_reg_handle_t regh,
                               const rw_keyspec_path_t*  ks,
                               const ProtobufCMessage*   msg,
                               void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  rw_keyspec_path_t *keyspec;


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-create", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nc-create", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "interface1", sizeof(keyspec_entry.dompath.path002.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *inter, inter_instance;

  inter = &inter_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,inter);

  strncpy(inter->name, "interface1", sizeof(inter->name)-1);
  inter->has_name = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_create1_client_cb,
                                              ctx, &inter->base);
  ASSERT_TRUE(new_xact);

  return;
}

static void memberapi_member_multiple_create_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_create_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-create", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nc-create", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    rw_keyspec_path_t* keyspec1 = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

    RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry1  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

    keyspec1 = (rw_keyspec_path_t*)&keyspec_entry1;
    keyspec_entry.dompath.path000.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path000.key00.name, "colony-create", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
    keyspec_entry.dompath.path001.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path001.key00.name, "nc-create", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
    rw_keyspec_path_set_category(keyspec1, NULL, RW_SCHEMA_CATEGORY_DATA);
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, keyspec1, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                                 RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);
  } else {
    reg_cb.cb.reg_ready_old = NULL;
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  }

  s->regh[ti->member_idx] = regh;

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_test_del_again_client_cb(rwdts_xact_t* xact,
                                   rwdts_xact_status_t* xact_status,
                                   void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];

  uint32_t count = rwdts_member_data_get_count(NULL, regh);
  EXPECT_EQ(count, 0);

  s->exitnow = 1;
  return;
}

static void
memberapi_test_del_client_cb(rwdts_xact_t* xact,
                             rwdts_xact_status_t* xact_status,
                             void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];
  rw_keyspec_path_t* keyspec = NULL;

  const RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *tmp;
  rw_keyspec_path_t *elem_ks = NULL;
  uint32_t count = 0;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  while ((tmp = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*) 
             rwdts_member_reg_handle_get_next_element(regh,
                                                     cur,
                                                     NULL,
                                                     &elem_ks))) {
    TSTPRN("Name is %s\n", tmp->name);
    //EXPECT_STREQ(tmp->name, "Interface1");
    EXPECT_EQ(tmp->has_interface_type, 1);
    count++;
  }
  rwdts_member_data_delete_cursors(NULL, regh);

  EXPECT_EQ(count, 2);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_DELETE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_again_client_cb,
                                              ud, NULL);
  ASSERT_TRUE(new_xact);


  return;
}

static void
memberapi_test_del_regready(rwdts_member_reg_handle_t regh,
                            const rw_keyspec_path_t*  ks,
                            const ProtobufCMessage*   msg,
                            void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  rw_keyspec_path_t *keyspec;


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-del", sizeof(nc->name)-1);
  nc->has_name = 1;
  nc->n_interface = 2;
  nc->interface = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)**)
                   RW_MALLOC0(nc->n_interface * sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*));
  nc->interface[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  nc->interface[1] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)));
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[0]);
  testdts_rw_fpath__yang_data__testdts_rw_base__colony__network_context__interface__init(nc->interface[1]);
  
  strncpy(nc->interface[0]->name, "Interface1", sizeof(nc->interface[0]->name)-1);
  nc->interface[0]->has_name = 1;
  nc->interface[0]->has_interface_type = 1;
  nc->interface[0]->interface_type.loopback = 1;
  nc->interface[0]->interface_type.has_loopback = 1;
  strncpy(nc->interface[1]->name, "Interface2", sizeof(nc->interface[1]->name)-1);
  nc->interface[1]->has_name = 1;
  nc->interface[1]->has_interface_type = 1;
  nc->interface[1]->interface_type.loopback = 1;
  nc->interface[1]->interface_type.has_loopback = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_client_cb,
                                              ctx, &nc->base);
  ASSERT_TRUE(new_xact);

  protobuf_c_message_free_unpacked_usebody(NULL, &(nc->base));
  return;
}

static void memberapi_member_del_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_del_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    rw_keyspec_path_t* ks1 = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
    RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) ks_entry = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
    ks1 = (rw_keyspec_path_t*)&ks_entry;
    ks_entry.dompath.path000.has_key00 = 1;
    ks_entry.dompath.path001.has_key00 = 1;
    strncpy(ks_entry.dompath.path000.key00.name, "colony-del", sizeof(ks_entry.dompath.path000.key00.name)-1);
    strncpy(ks_entry.dompath.path001.key00.name, "nwcon-del", sizeof(ks_entry.dompath.path001.key00.name)-1);
    rw_keyspec_path_set_category(ks1, NULL, RW_SCHEMA_CATEGORY_DATA);
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, ks1, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);
  } else {
    reg_cb.cb.reg_ready_old = NULL;
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                                 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  }

  s->regh[ti->member_idx] = regh;

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_test_del_deep_client_cb(rwdts_xact_t* xact,
                                  rwdts_xact_status_t* xact_status,
                                  void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];

  const RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *tmp;
  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  uint32_t count = 0;
  while ((tmp = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*) 
               rwdts_member_reg_handle_get_next_element(regh,
                                                        cur,
                                                        NULL,
                                                        &elem_ks))) {
    TSTPRN("Name is %s\n", tmp->name);
    EXPECT_STREQ(tmp->name, "nwcon-del-deep");
    EXPECT_EQ(tmp->n_interface, 1);
    EXPECT_STREQ(tmp->interface[0]->name, "Inter-del-deep");
    EXPECT_EQ(tmp->interface[0]->has_interface_type, 0);
    count++;
  }
  rwdts_member_data_delete_cursors(NULL, regh);

  EXPECT_EQ(count, 1);

  s->exitnow = 1;
  return;
}

static void
memberapi_test_deeper_del_client_cb(rwdts_xact_t* xact,
                                    rwdts_xact_status_t* xact_status,
                                    void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];
  rw_keyspec_path_t* keyspec = NULL;

  const RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *tmp;
  rw_keyspec_path_t *elem_ks = NULL;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  uint32_t count = 0;
  while ((tmp = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*) 
                rwdts_member_reg_handle_get_next_element(regh,
                                                         cur,
                                                         NULL,
                                                         &elem_ks))) {
    TSTPRN("Name is %s\n", tmp->name);
    EXPECT_STREQ(tmp->name, "nwcon-del-deep");
    EXPECT_EQ(tmp->n_interface, 1);
    EXPECT_STREQ(tmp->interface[0]->name, "Inter-del-deep");
    EXPECT_EQ(tmp->interface[0]->has_interface_type, 1);
    count++;
  }
  rwdts_member_data_delete_cursors(NULL, regh);

  EXPECT_EQ(count, 1);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface_InterfaceType));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface_InterfaceType) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface_InterfaceType));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del-deep", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del-deep", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "Inter-del-deep", sizeof(keyspec_entry.dompath.path002.key00.name) - 1);


  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_DELETE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_deep_client_cb,
                                              ud, NULL);
  ASSERT_TRUE(new_xact);


  return;
}

static void
memberapi_test_del_deep_regready(rwdts_member_reg_handle_t regh,
                                 const rw_keyspec_path_t*  ks,
                                 const ProtobufCMessage*   msg,
                                 void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  rw_keyspec_path_t *keyspec;


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del-deep", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del-deep", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  keyspec_entry.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path002.key00.name, "Inter-del-deep", sizeof(keyspec_entry.dompath.path002.key00.name) -1);

  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *inter, inter_instance;

  inter = &inter_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,inter);

  strncpy(inter->name, "Inter-del-deep", sizeof(inter->name)-1);
  inter->has_name = 1;
  inter->has_interface_type = 1;
  inter->interface_type.loopback = 1;
  inter->interface_type.has_loopback = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_deeper_del_client_cb,
                                              ctx, &inter->base);
  ASSERT_TRUE(new_xact);

  return;
}

static void memberapi_member_del_deep_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_del_deep_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-del-deep", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-del-deep", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                                 RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);
  } else {
    reg_cb.cb.reg_ready_old = NULL;
    rw_keyspec_path_t* ks1 = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
    RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) ks_entry = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
    ks1 = (rw_keyspec_path_t*)&ks_entry;
    ks_entry.dompath.path000.has_key00 = 1;
    ks_entry.dompath.path001.has_key00 = 1;
    strncpy(ks_entry.dompath.path000.key00.name, "colony-del-deep", sizeof(ks_entry.dompath.path000.key00.name)-1);
    strncpy(ks_entry.dompath.path001.key00.name, "nwcon-del-deep", sizeof(ks_entry.dompath.path001.key00.name)-1);
    rw_keyspec_path_set_category(ks1, NULL, RW_SCHEMA_CATEGORY_DATA);
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, ks1, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  }

  s->regh[ti->member_idx] = regh;

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void
memberapi_test_del_wc_client_cb(rwdts_xact_t* xact,
                                 rwdts_xact_status_t* xact_status,
                                 void*         ud)
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_member_reg_handle_t regh = s->regh[0];
  rw_keyspec_path_t* keyspec = NULL;

  const RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *tmp;
  rw_keyspec_path_t *elem_ks = NULL;
  uint32_t count = 0;
  rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
  while ((tmp = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*) rwdts_member_reg_handle_get_next_element(
                                                                                                regh,
                                                                                                cur,
                                                                                                NULL,
                                                                                                &elem_ks))) {
    TSTPRN("Name is %s\n", tmp->name);
    if (s->del_tc == DEL_WC_1) {
      EXPECT_STREQ(tmp->name, "nwcon-wc1-del");
    } else {
      EXPECT_STREQ(tmp->name, "nwcon-wc2-del");
    }
    count++;
  }

  rwdts_member_data_delete_cursors(NULL, regh);
  EXPECT_EQ(count, 1);


  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  if (s->del_tc == DEL_WC_1) {
    strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc1-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
    keyspec_entry.dompath.path001.has_key00 = 1;
    strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-wc1-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);
  } else {
    strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc2-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  }

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_DELETE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_again_client_cb,
                                              ud, NULL);
  ASSERT_TRUE(new_xact);

  return;
}

static void
memberapi_test_del_wc1_regready(rwdts_member_reg_handle_t regh,
                                const rw_keyspec_path_t*  ks,
                                const ProtobufCMessage*   msg,
                                void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  rw_keyspec_path_t *keyspec;


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc1-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-wc1-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-wc1-del", sizeof(nc->name)-1);
  nc->has_name = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_wc_client_cb,
                                              ctx, &nc->base);
  ASSERT_TRUE(new_xact);

  return;
}

static void memberapi_member_del_wc1_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_del_wc1_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc1-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);
  } else {
    reg_cb.cb.reg_ready_old = NULL;
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  }

  s->regh[ti->member_idx] = regh;

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}


static void
memberapi_test_del_wc2_regready(rwdts_member_reg_handle_t regh,
                                const rw_keyspec_path_t*  ks,
                                const ProtobufCMessage*   msg,
                                void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  rw_keyspec_path_t *keyspec;


  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc2-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-wc2-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);


  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc, nc_instance;

  nc = &nc_instance;

  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nc);

  strncpy(nc->name, "nwcon-wc2-del", sizeof(nc->name)-1);
  nc->has_name = 1;

  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec, RWDTS_QUERY_CREATE,
                                              RWDTS_XACT_FLAG_ADVISE, memberapi_test_del_wc_client_cb,
                                              ctx, &nc->base);
  ASSERT_TRUE(new_xact);

  return;
}

static void memberapi_member_del_wc2_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_test_del_wc2_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony-wc2-del", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);
  keyspec_entry.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path001.key00.name, "nwcon-wc2-del", sizeof(keyspec_entry.dompath.path001.key00.name) - 1);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);
  rwdts_member_reg_handle_t regh;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 2) {
    /* Establish a registration */
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_PUBLISHER|RWDTS_XACT_FLAG_ANYCAST|RWDTS_FLAG_SHARED, NULL);
  } else {
    reg_cb.cb.reg_ready_old = NULL;
    regh = rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                                 RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE, NULL);
  }

  s->regh[ti->member_idx] = regh;

  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void rwdts_clnt_p_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);
  rwdts_query_result_t *qrslt = NULL;

  qrslt = rwdts_xact_query_result(xact, 9998); 
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony)* col = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony)*)qrslt->message;
  ASSERT_TRUE(col);
  EXPECT_STREQ(col->name, "colony999");
  EXPECT_EQ(col->port[0]->has_mtu, 1);
  EXPECT_EQ(col->n_network_context, 1);
  EXPECT_STREQ(col->network_context[0]->name, "nwcontext999");
  EXPECT_EQ(col->network_context[0]->n_load_balancer, 0);
  
  qrslt = rwdts_xact_query_result(xact, 9997);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony)* col1 = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony)*)qrslt->message;
  EXPECT_STREQ(col1->name, "colony999");
  EXPECT_EQ(col1->port[0]->has_mtu, true);
  EXPECT_EQ(col1->n_network_context, 1);
  EXPECT_STREQ(col1->network_context[0]->name, "nwcontext999");
  EXPECT_EQ(col1->network_context[0]->n_load_balancer, 1);
  EXPECT_STREQ(col1->network_context[0]->load_balancer[0].name, "lb999");

  qrslt = rwdts_xact_query_result(xact, 9996);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)* nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)qrslt->message;
  EXPECT_STREQ(nc->name, "nwcontext999");
  EXPECT_EQ(nc->n_load_balancer, 1);
  EXPECT_STREQ(nc->load_balancer[0].name, "lb999");
  EXPECT_EQ(nc->n_interface, 0);

  qrslt = rwdts_xact_query_result(xact, 9995);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)* nc1 = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)qrslt->message;
  EXPECT_STREQ(nc1->name, "nwcontext999");
  //EXPECT_EQ(nc1->n_interface, 1);
  if (nc1->interface && nc1->interface[0]) {
    //EXPECT_STREQ(nc1->interface[0]->name, "interface999");Z
  }

}

static void rwdts_second_clnt_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);
  rwdts_query_result_t *qrslt = NULL;

  qrslt = rwdts_xact_query_result(xact, 9994);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*inter = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)qrslt->message;
  EXPECT_STREQ(inter->name, "interface999");
  EXPECT_EQ(inter->has_interface_type, 1);
  EXPECT_EQ(inter->interface_type.loopback, 1);
  EXPECT_EQ(inter->n_ip, 1);
  EXPECT_STREQ(inter->ip[0].address, "2.2.2.2"); 

  qrslt = rwdts_xact_query_result(xact, 9993);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*inter1 = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface)*)qrslt->message;
  EXPECT_STREQ(inter1->name, "interface999");
  EXPECT_EQ(inter1->has_interface_type, 1);
  EXPECT_EQ(inter1->interface_type.loopback, 1);
  EXPECT_EQ(inter1->n_ip, 1);
  EXPECT_STREQ(inter1->ip[0].address, "2.2.2.2");

  EXPECT_EQ(s->prepare_cb_called, 3);
  EXPECT_EQ(s->prepare_list_cb_called, 4);

  s->exitnow = 1;
}
static void memberapi_pruning_client_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_xact_t* xact = NULL;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  rw_status_t rs;

  TSTPRN("MemberAPI test client pruning start...\n");

  rw_keyspec_path_t *keyspec = NULL;
  uint32_t flags = 0;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony) keyspec_entry;
  keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  flags |= RWDTS_XACT_FLAG_DEPTH_ONE;
  xact = rwdts_api_xact_create(clnt_apih, flags,rwdts_clnt_p_query_callback, ctx);
  ASSERT_TRUE(xact);
  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  ASSERT_TRUE(blk);
  rs = rwdts_xact_block_add_query_ks(blk,keyspec,RWDTS_QUERY_READ,
                      flags,9998,NULL);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);


  flags = 0;
  flags |= RWDTS_XACT_FLAG_DEPTH_LISTS;

  rs = rwdts_xact_block_add_query_ks(blk, keyspec, RWDTS_QUERY_READ, flags, 9997, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);


  flags = 0;
  flags |= RWDTS_XACT_FLAG_DEPTH_ONE;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry1;
  keyspec_entry1  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec_entry1.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry1.dompath.path000.key00.name) - 1);
  keyspec_entry1.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path001.key00.name, "nwcontext999", sizeof(keyspec_entry1.dompath.path001.key00.name) - 1);

  rs = rwdts_xact_block_add_query_ks(blk, keyspec, RWDTS_QUERY_READ, flags, 9996, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

  flags = 0;
  flags |= RWDTS_XACT_FLAG_DEPTH_LISTS;

  rs = rwdts_xact_block_add_query_ks(blk, keyspec, RWDTS_QUERY_READ, flags, 9995, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

  rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);



  rwdts_xact_t* lxact = NULL;

  flags = 0;
  flags |= RWDTS_XACT_FLAG_DEPTH_ONE;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry2;
  keyspec_entry2 = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry2;
  keyspec_entry2.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry2.dompath.path000.key00.name) - 1);
  keyspec_entry2.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path001.key00.name, "nwcontext999", sizeof(keyspec_entry2.dompath.path001.key00.name) - 1);
  keyspec_entry2.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path002.key00.name, "interface999", sizeof(keyspec_entry2.dompath.path002.key00.name) - 1);

  lxact = rwdts_api_xact_create(clnt_apih, flags,rwdts_second_clnt_query_callback, ctx);
  ASSERT_TRUE(lxact);
  blk = rwdts_xact_block_create(lxact);
  ASSERT_TRUE(blk);
  rs = rwdts_xact_block_add_query_ks(blk,keyspec,RWDTS_QUERY_READ,
                     flags,9994,NULL);
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);

  flags = 0;
  flags |= RWDTS_XACT_FLAG_DEPTH_LISTS;
  flags |= RWDTS_XACT_FLAG_END;;

  rs = rwdts_xact_block_add_query_ks(blk, keyspec, RWDTS_QUERY_READ, flags, 9993, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

  rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);


  rs = rwdts_xact_commit(xact);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
}

static void
memberapi_interface_regready(rwdts_member_reg_handle_t regh,
                             const rw_keyspec_path_t*  ks,
                             const ProtobufCMessage*   msg,
                             void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  RWPB_T_PATHENTRY(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) kpe = *(RWPB_G_PATHENTRY_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  strncpy(kpe.key00.name, "interface999", sizeof(kpe.key00.name)-1);


  RWPB_M_MSG_DECL_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface,inter_instance);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) *inter = &inter_instance;

  strncpy(inter->name, "interface999", sizeof(inter->name)-1);
  inter->has_name = 1;
  inter->has_interface_type = true;
  inter->interface_type.has_loopback = true;
  inter->interface_type.loopback = true;
  inter->n_ip = 1;
  strcpy(inter->ip[0].address, "2.2.2.2");
  inter->ip[0].has_address = 1;

  rw_status_t rs = rwdts_member_reg_handle_create_element(
                              regh,gkpe,&inter->base,NULL);

  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inter);


  TSTPRN("Member start_f invoking  memberapi_pruning_client_start_f\n");
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           when,
                           s->client.rwq,
                           &s->tenv->t[TASKLET_CLIENT],
                           memberapi_pruning_client_start_f);

}

static void
memberapi_nwcontext_regready(rwdts_member_reg_handle_t regh,
                             const rw_keyspec_path_t*  ks,
                             const ProtobufCMessage*   msg,
                             void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  RWPB_T_PATHENTRY(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) kpe = *(RWPB_G_PATHENTRY_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  strncpy(kpe.key00.name, "nwcontext999", sizeof(kpe.key00.name)-1);


  RWPB_M_MSG_DECL_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext,nwc_instance);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nwc = &nwc_instance;

  strncpy(nwc->name, "nwcontext999", sizeof(nwc->name)-1);
  nwc->has_name = 1;
  nwc->n_load_balancer = 1;
  strcpy(nwc->load_balancer[0].name, "lb999");
  nwc->load_balancer[0].has_name = 1;

  rw_status_t rs = rwdts_member_reg_handle_create_element(regh,
                                                          gkpe,
                                                          &nwc->base,
                                                          NULL);

  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  //protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)nwc);
}

static void
memberapi_colony_regready(rwdts_member_reg_handle_t regh,
                          const rw_keyspec_path_t*  ks,
                          const ProtobufCMessage*   msg,
                          void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *apih = ti->apih;
  RW_ASSERT(apih);
  //Create an element
  RWPB_T_PATHENTRY(TestdtsRwFpath_TestdtsRwBase_data_Colony) kpe = *(RWPB_G_PATHENTRY_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony));

  rw_keyspec_entry_t *gkpe = (rw_keyspec_entry_t*)&kpe;
  kpe.has_key00 = 1;
  strncpy(kpe.key00.name, "colony999", sizeof(kpe.key00.name)-1);

  RWPB_M_MSG_DECL_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony,colony_instance);
  RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony) *colony = &colony_instance;

  strncpy(colony->name, "colony999", sizeof(colony->name)-1);
  colony->has_name = 1;
  colony->n_port = 1;
  colony->port = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_Port)**)
                            RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_Port)*));
  colony->port[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_Port)*)
                     RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_Port)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_Port, colony->port[0]);
  strcpy(colony->port[0]->name, "Port999");
  colony->port[0]->has_name = 1;
  colony->port[0]->has_name = 1;
  colony->port[0]->has_mtu = true;
  colony->port[0]->mtu = 1500;
  colony->port[0]->has_receive_q_length = true;
  colony->port[0]->receive_q_length = 100;
  colony->n_network_context = 1;
  colony->network_context = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)**)
                             RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*));
  colony->network_context[0] = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)*)
                               RW_MALLOC0(sizeof(RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext)));
  RWPB_F_MSG_INIT(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext, colony->network_context[0]);
  strcpy(colony->network_context[0]->name, "nwcontext999");
  colony->network_context[0]->has_name = 1;

  rw_status_t rs = rwdts_member_reg_handle_create_element(
                           regh,gkpe,&colony->base,NULL);

  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)colony);
}

static rwdts_member_rsp_code_t member_api_depth_one_prepare(const rwdts_xact_info_t* xact_info,
                                                       RWDtsQueryAction         action,
                                                       const rw_keyspec_path_t*      key,
                                                       const ProtobufCMessage*  msg,
                                                       uint32_t credits,
                                                       void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  s->prepare_cb_called++;
  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t member_api_depth_list_prepare(const rwdts_xact_info_t* xact_info,
                                                       RWDtsQueryAction         action,
                                                       const rw_keyspec_path_t*      key,
                                                       const ProtobufCMessage*  msg,
                                                       uint32_t credits,
                                                       void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  s->prepare_list_cb_called++;
  return RWDTS_ACTION_OK;
}

static void memberapi_pruning_start_f(void *ctx);

static void
member_api_depth_regready(rwdts_member_reg_handle_t regh,
                          rw_status_t               rs,
                          void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_MEMBER_0],
                             memberapi_pruning_start_f);
  }

  return;
}

static void memberapi_pruning_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t reg_cb;
  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_colony_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry;
  keyspec_entry.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry.dompath.path000.key00.name) - 1);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 0) {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  } else {
    RW_ZERO_VARIABLE(&reg_cb);
    reg_cb.ud = ctx;
    if (ti->member_idx == 1) {
      reg_cb.cb.prepare = member_api_depth_one_prepare;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_ONE, NULL);
    } else {
      reg_cb.cb.prepare = member_api_depth_list_prepare;
      reg_cb.cb.reg_ready = member_api_depth_regready;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_LISTS, NULL);
    }
  }

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_nwcontext_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) keyspec_entry1  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry1;
  keyspec_entry1.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry1.dompath.path000.key00.name) - 1);
  keyspec_entry1.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry1.dompath.path001.key00.name, "nwcontext999", sizeof(keyspec_entry1.dompath.path001.key00.name) - 1);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 0) {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  } else {
    RW_ZERO_VARIABLE(&reg_cb);
    reg_cb.ud = ctx;
    if (ti->member_idx == 1) {
      reg_cb.cb.prepare = member_api_depth_one_prepare;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_ONE, NULL);
    } else {
      reg_cb.cb.prepare = member_api_depth_list_prepare;
      reg_cb.cb.reg_ready = member_api_depth_regready;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_LISTS, NULL);
    }
  }


  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.ud = ctx;
  reg_cb.cb.reg_ready_old = memberapi_interface_regready;

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));
  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface) keyspec_entry2  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface));

  keyspec = (rw_keyspec_path_t*)&keyspec_entry2;
  keyspec_entry2.dompath.path000.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path000.key00.name, "colony999", sizeof(keyspec_entry2.dompath.path000.key00.name) - 1);
  keyspec_entry2.dompath.path001.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path001.key00.name, "nwcontext999", sizeof(keyspec_entry2.dompath.path001.key00.name) - 1);
  keyspec_entry2.dompath.path002.has_key00 = 1;
  strncpy(keyspec_entry2.dompath.path002.key00.name, "interface999", sizeof(keyspec_entry2.dompath.path002.key00.name) - 1);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  if (ti->member_idx == 0) {
    rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                          RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED, NULL);
  } else {
    RW_ZERO_VARIABLE(&reg_cb);
    reg_cb.ud = ctx;
    if (ti->member_idx == 1) {
      reg_cb.cb.prepare = member_api_depth_one_prepare;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_ONE, NULL);
    } else {
      reg_cb.cb.prepare = member_api_depth_list_prepare;
      reg_cb.cb.reg_ready = member_api_depth_regready;
      rwdts_member_register(NULL, apih, keyspec, &reg_cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface),
                            RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_DEPTH_LISTS, NULL);
    }
  }

  TSTPRN("Member start_f ending, startct=%u\n", s->member_startct);
}

static void multireg_stop_f(void *ctx) {

  struct queryapi_state*   s = (struct queryapi_state *)ctx;

  TSTPRN("Multi Reg stop function...\n");
  s->exitnow = 1;
}

static void multireg_dereg_path_f(void *ctx) {
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rwdts_api_t *apih = s->tenv->t[TASKLET_CLIENT].apih;

  rwdts_member_deregister_path(apih,
                               (char *)"/L/RWDTS_GTEST/MEMBER/4",
                               RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL);

  double seconds = (RUNNING_ON_VALGRIND)?60:5;
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_2].tasklet,
                           when,
                           s->client.rwq,
                           s,
                           multireg_stop_f);

  return;
}

static void multireg_dereg_f(void* arg)
{
  struct queryapi_state *s = (struct queryapi_state *)arg;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_member_reg_handle_t regh = s->regh[1];
 
  rw_status_t rw_status = rwdts_member_deregister(regh);
  EXPECT_EQ(RW_STATUS_SUCCESS, rw_status);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)s->regh[2];
  rwdts_member_deregister_keyspec(reg->apih, reg->keyspec); 

  double seconds = (RUNNING_ON_VALGRIND)?10:1;
  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC);
  rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                           when,
                           s->client.rwq,
                           s,
                           multireg_dereg_path_f);

  return;
}

static void
memberapi_multi_reg_ready(rwdts_member_reg_handle_t  regh,
                          rw_status_t                rs,
                          void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("memberapi_multi_reg_ready() called\n");

  s->reg_ready_called++;

  if (s->reg_ready_called == 6) {
    double seconds = (RUNNING_ON_VALGRIND)?5:1;
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             s,
                             multireg_dereg_f);
  }
  return;
}

static void memberapi_test_multiple_registrations_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Multiple registration start...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContextState_PacketCounters_Fastpath);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  /* Establish a registration */
  cb.cb.reg_ready = memberapi_multi_reg_ready;
  rwdts_member_reg_handle_t regh1 = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_Fastpath),
                                                  RWDTS_FLAG_SUBSCRIBER , NULL);
  s->regh[ti->member_idx] = regh1;

  ASSERT_TRUE(regh1);

  // Addd the same registration again
  rwdts_member_reg_handle_t regh2 = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(TestdtsRwFpath_Fastpath),
                                                  RWDTS_FLAG_PUBLISHER , NULL);
  ASSERT_TRUE(regh2);

  EXPECT_NE(regh1, regh2);

  rwdts_member_registration_t *reg1 = (rwdts_member_registration_t*)regh1;
  rwdts_member_registration_t *reg2 = (rwdts_member_registration_t*)regh2;

  size_t index = 0;
  int key_index = 0;

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(NULL, reg1->keyspec, reg2->keyspec, &index, &key_index));

  usleep(100);

}

static void memberapi_pub_sub_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub  Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                        RWDTS_FLAG_SUBSCRIBER , NULL);
  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet, 
                             s->client.rwq, 
                             &s->tenv->t[TASKLET_CLIENT], 
                             memberapi_pub_sub_client_start_f);
  }
  TSTPRN("Member member API pub sub member start ending , startct=%u\n", s->member_startct);
}

static void memberapi_sub_before_pub_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d sub Before pub ...\n", ti->member_idx);

  {
  
    s->subs_created++;
    RW_ZERO_VARIABLE(&cb);
    cb.ud = ctx;
    cb.cb.prepare = memberapi_receive_pub_notification;
    cb.cb.reg_ready_old = memberapi_cache_updated;
    cb.cb.reg_ready = subscriber_ready_sub_b_pub;

    RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
    keyspec = (rw_keyspec_path_t*)&keyspec_instance;
    rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

    rwdts_api_t *apih = ti->apih;
    ASSERT_NE(apih, (void*)NULL);

    rwdts_member_deregister_all(apih);

    apih->stats.num_member_advise_rsp = 0;
    /* Establish a registration */
    s->regh[ti->tasklet_idx] = rwdts_memberapi_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                          RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE , NULL);

    RW_ASSERT_TYPE(apih, rwdts_api_t);

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1/10*3);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             ctx,
                             rwdts_check_subscribed_data);

    TSTPRN("Member member API sub before pub ending , startct=%u\n", s->member_startct);
  }
}

static void memberapi_pub_sub_minikey_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub  Minikey Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;
  rwdts_member_deregister_all(apih);

  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_SUBSCRIBER , NULL);
         

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_minikey_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_minikey_client_start_f);
  }
  TSTPRN("Member member API pub sub minikey member start ending , startct=%u\n", s->member_startct);
}
static void memberapi_sub_after_pub_f(void *ctx) 
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member start_f invoking  memberapi_pub_sub_client_start_f\n");

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  if (clnt_apih->stats.num_member_advise_rsp < 3)
  {  
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             ctx,
                             memberapi_sub_after_pub_f);
    return;
  }
  TSTPRN("Subscriber Starting %d +===========================\n", s->publisher_ready);
  
  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;
  cb.cb.reg_ready_old = memberapi_cache_updated;
  cb.cb.reg_ready = subscriber_ready;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);


  apih->stats.num_member_advise_rsp = 0;
  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_memberapi_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                        RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE , NULL);

  ck_pr_inc_32(&s->member_startct);

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 1/10*3);
  rwsched_dispatch_after_f(apih->tasklet,
                           when,
                           apih->client.rwq,
                           ctx,
                           rwdts_check_subscribed_data);

  TSTPRN("Member member API sub before pub ending , startct=%u\n", s->member_startct);
}

static void
memberapi_receive_pub_regready(rwdts_member_reg_handle_t regh,
                               rw_status_t               rs,
                               void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_cache_client_start_f);
  }

  return;
}

static void memberapi_pub_sub_cache_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub  Cache Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;
  cb.cb.reg_ready = memberapi_receive_pub_regready;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
  keyspec = (rw_keyspec_path_t*)&keyspec_instance;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_SUBSCRIBER , NULL);

  TSTPRN("Member member API pub sub cache member start ending , startct=%u\n", s->member_startct);
}

static void
memberapi_pub_sub_non_listy_regready(rwdts_member_reg_handle_t regh,
                                     rw_status_t               rs,
                                     void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_non_listy_client_start_f);
  }

  return;
}

static void memberapi_pub_sub_non_listy_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub Non listy Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;
  cb.cb.reg_ready = memberapi_pub_sub_non_listy_regready;

  RWPB_T_PATHSPEC(DtsTest_data_Person) person_ks = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  keyspec = (rw_keyspec_path_t*)&person_ks;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);


  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  apih->stats.num_member_advise_rsp = 0;

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person),
                                  RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_SHARED , NULL);

  TSTPRN("Member member API pub sub non listy member start ending , startct=%u\n", s->member_startct);
}

static void
memberapi_sub_pub_cache_xact_regready (rwdts_member_reg_handle_t regh,
                            rw_status_t               rs,
                            void*                     ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  ck_pr_inc_32(&s->member_startct);

  if (s->member_startct == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking  memberapi_pub_sub_cache_client_start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_pub_sub_cache_xact_client_start_f);
  }
}

static void memberapi_pub_sub_cache_xact_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rw_keyspec_path_t *keyspec;
  rwdts_member_event_cb_t cb;

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Pub Sub  Cache Notify start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.prepare = memberapi_receive_pub_notification;
  cb.cb.reg_ready = memberapi_sub_pub_cache_xact_regready;

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) person_ks = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec = (rw_keyspec_path_t*)&person_ks;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  rwdts_member_deregister_all(apih);

  /* Establish a registration */
  s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, keyspec, &cb, RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_SUBSCRIBER | RWDTS_FLAG_CACHE, NULL);

  TSTPRN("Member member API pub sub cache member start ending , startct=%u\n", s->member_startct);
}

static void member_reg_ready_test_execute(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             member_reg_ready_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
  
}

static void member_test_notif_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             member_notif_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
 
}

static void member_advise_solicit_test(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             member_advise_solicit_test_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

}

static void member_reg_appdata_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             member_reg_appdata_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

}

static void member_reg_appdata_safe_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             member_reg_appdata_safe_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

}

static void member_reg_appdata_queue_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             member_reg_appdata_queue_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

}

static void appdata_test_getnext_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             appdata_getnext_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

}

static void memberapi_obj_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_object_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_subobj_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_subobject_start_f);
  }

  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_nosubobj_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_nosubobject_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void member_gi_api_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], member_gi_api_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_execute(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_execute_ident(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_ident_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_execute_ident_update(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             memberapi_ident_update_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_execute_range(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_range_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}


static void memberapi_test_execute_perf(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    //s->member[i].rwq = s->tenv->t[TASKLET_MEMBER_0+i].rwq;
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_perf_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_execute_multiple(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_multiple_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:90;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_anycast_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_anycast_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_subread_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_subread_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_merge_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_merge_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_empty_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_empty_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_refcnt_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_refcnt_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}


static void memberapi_async_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  s->member[0].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  s->tenv->t[TASKLET_MEMBER_0].ctx = s;
  rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0].tasklet, s->member[0].rwq, &s->tenv->t[TASKLET_MEMBER_0], memberapi_member_async_start_f);
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_promote_sub_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_promote_sub_test_execute_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_publish_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_publish_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}
static void memberapi_test_audit_tests(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_test_audit_tests_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_sub_appconf_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_sub_appconf_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

  return;
}

static int memberapi_get_objct(void *ctx) {
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_reg_handle_t regh = s->regh[ti->tasklet_idx];
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;

  int ct=0;
  if (reg) {
    
    rw_keyspec_path_t *ks = NULL;
    const ProtobufCMessage *msg;

    rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
    
    while ((msg = rwdts_member_data_get_next_list_element(NULL, regh, cur, &ks))) {
      ks=NULL;
      ct++;
    }
    rwdts_member_data_delete_cursors(NULL, regh);
  }
  return ct;
}

static int memberapi_dumpmember_nc_iface(struct tenv1::tenv_info *ti,
					 const char *ctxname,
					 const char *ifname) {
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_reg_handle_t regh = s->regh[ti->tasklet_idx];
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;


  int ct = 0;
  if (reg) {
    
    rw_keyspec_path_t *ks = NULL;
    const ProtobufCMessage *msg;

    ks=NULL;
    rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(NULL, regh);
    while ((msg = rwdts_member_data_get_next_list_element(NULL, regh, cur, &ks))) {

      RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *)msg;

      if (ctxname && 0!=strcmp(ctxname, nc->name)) {
	continue;
      }

      for (int i=0; i<(int)nc->n_interface; i++) {
	if (0==strcmp(ifname, nc->interface[i]->name)) {
	  ct++;
	}
      }
    }
    rwdts_member_data_delete_cursors(NULL, regh);
  }

  return ct;
}

static int memberapi_dumpmember_nc(struct tenv1::tenv_info *ti,
				   rwdts_xact_t *xact,
				   const char *ctxname) {
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_reg_handle_t regh = s->regh[ti->tasklet_idx];
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;


  int ct = 0;
  if (reg) {
    
    rw_keyspec_path_t *ks = NULL;
    const ProtobufCMessage *msg;

    ks=NULL;
    rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(xact, regh);
    while ((msg = rwdts_member_data_get_next_list_element(xact, regh, cur, &ks))) {
      RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *nc = (RWPB_T_MSG(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext) *)msg;

      if (ctxname && 0!=strcmp(ctxname, nc->name)) {
	continue;
      }

      ct++;
    }
    rwdts_member_data_delete_cursors(xact, regh);
  }

  return ct;

  memberapi_dumpmember_nc(ti,xact,ctxname); // !unused
}

static void memberapi_dumpmember(void *ctx, rwdts_xact_t *xact) {

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_reg_handle_t regh = s->regh[ti->tasklet_idx];
  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;

  int ct=0;
  if (reg) {
    
    rw_keyspec_path_t *ks = NULL;
    const ProtobufCMessage *msg;

    rwdts_member_cursor_t *cur = rwdts_member_data_get_cursor(xact, regh);
    
    while ((msg = rwdts_member_data_get_next_list_element(xact, regh, cur, &ks))) {
      ks=NULL;
      ct++;
    }
    rwdts_member_data_delete_cursors(xact, regh);

    TSTPRN("Member %d dump, obj count %d%s\n", 
	    ti->member_idx,
	    ct,
	    ct ? ":" : ".");

    TSTPRN("  Reg is %s, keystr '%s'\n", 
	    (reg->flags & RWDTS_FLAG_SUBSCRIBER ) ? "SUBSCRIBER" : "PUBLISHER",
	    (reg && reg->keystr) ? reg->keystr : "NULL");

    ks=NULL;
    cur = rwdts_member_data_get_cursor(xact, regh);
    while ((msg = rwdts_member_data_get_next_list_element(xact, regh, cur, &ks))) {
      
      char *str = NULL;
      rw_json_pbcm_to_json(msg, msg->descriptor, &str);
      RW_ASSERT(str);
      
      char *ks_str = NULL;
      rw_keyspec_path_get_new_print_buffer(ks, NULL, rwdts_api_get_ypbc_schema(ti->apih), &ks_str);
      RW_ASSERT(ks_str);
      
      TSTPRN("  Key '%s'\n      %s\n", ks_str, str);
      
      free(str);
      free(ks_str);
      
      ks=NULL;
    }
    rwdts_member_data_delete_cursors(xact, regh);

  }
}


static void memberapi_appconf_delete(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  s->exitnow = 0;
  s->exit_soon = 1;//TASKLET_MEMBER_CT;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_appconf_delete_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:60;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);

  TSTPRN("Dumping all member data in all tasklets...\n");
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    memberapi_dumpmember(&s->tenv->t[TASKLET_MEMBER_0+i], NULL);
  }

  /* Validate the correct number of objects leftover in memoer 0's
     registration.  This is too minimalist a check, but better than it
     was. */
  int member0_objct = memberapi_get_objct(&s->tenv->t[TASKLET_MEMBER_0]);
  ASSERT_EQ(s->Dleftover, member0_objct);

  return;
  memberapi_appconf_delete(s);	// lint / unused warning
}

static void memberapi_append_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT_NEW; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_append_start_f);
  }

  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_transreg_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_transreg_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_shared_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_shared_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_pub_sub_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             memberapi_pub_sub_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_sub_before_pub(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             memberapi_sub_before_pub_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:20;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_sub_after_pub(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_NE(clnt_apih, (void*)NULL);
  clnt_apih->stats.num_member_advise_rsp = 0;

  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                            s->member[i].rwq,
                            &s->tenv->t[TASKLET_MEMBER_0 + i],
                            memberapi_sub_after_pub_f);
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                            &s->tenv->t[TASKLET_CLIENT],
                            memberapi_sub_after_pub_client_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}


static void memberapi_test_pub_sub_minikey_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_minikey_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_pub_sub_cache_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  ASSERT_FALSE(s->exitnow);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_cache_member_start_f);
  }
  ASSERT_FALSE(s->exitnow);
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_pub_sub_non_listy_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_non_listy_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?60:20;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_pub_sub_cache_xact_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_cache_xact_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_multi_xact_reroot_execute(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_multi_xact_reroot_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_xact_iter_reroot_execute(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_xact_iter_reroot_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_multi_xact_expand_execute(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_member_start_multi_xact_expand_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_reg_dereg_notification(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_reg_dereg_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow != 0);
}

static void memberapi_test_pub_sub_cache_key_note(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_pub_sub_cache_kn_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?45:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_create_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i == TASKLET_MEMBER_CT-1) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 3*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               when,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_multiple_create_start_f);
    } else {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_multiple_create_start_f);
    }
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_update_merge(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_update_merge_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:15;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_del_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i == TASKLET_MEMBER_CT-1) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 3*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               when,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_start_f);
    } else {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_start_f);
    }
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_del_deep_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i == TASKLET_MEMBER_CT-1) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 3*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               when,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_deep_start_f);
    } else {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_deep_start_f);
    }
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_del_wc1_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i == TASKLET_MEMBER_CT-1) {
    double after = (RUNNING_ON_VALGRIND)?9:3;
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, after*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               when,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_wc1_start_f);
    } else {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_wc1_start_f);
    }
  }
  double seconds = (RUNNING_ON_VALGRIND)?120:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_del_wc2_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i == TASKLET_MEMBER_CT-1) {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 3*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               when,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_wc2_start_f);
    } else {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_member_del_wc2_start_f);
    }
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_pruning_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=TASKLET_MEMBER_CT-1; i >= 0; i--) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    if (i!=0) {
      rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0+i].tasklet,
                               s->member[i].rwq,
                               &s->tenv->t[TASKLET_MEMBER_0+i],
                               memberapi_pruning_start_f);
    } 
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:90;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void memberapi_test_multiple_registrations(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet,
                             s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i],
                             memberapi_test_multiple_registrations_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?160:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

#if 0
static rwdts_member_rsp_code_t
memberapi_test_data_member_cursor_prepare_cb(rwdts_xact_t*                 xact,
                                             rwdts_query_handle_t          qhdl,
                                             rwdts_member_event_t          evt,
                                             rw_keyspec_path_t*                 key,
                                             const ProtobufCMessage*       msg,
                                             void*                         ud)
#endif
static rwdts_member_rsp_code_t
memberapi_test_data_member_cursor_precommit_cb(const rwdts_xact_info_t*    xact_info,
                                             unsigned int                  size,
                                             const rwdts_commit_record_t** crec)
{
  RW_ASSERT(xact_info);
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
   
  int idx = 0;
  const char* phones[] = {"111-456-789", "222-456-789", "333-456-789", "444-456-789"};
  RWPB_T_MSG(DtsTest_data_Person_Phone)* phone   = NULL;

  rwdts_member_cursor_t* cursor = rwdts_member_data_get_cursor((rwdts_xact_t*)xact_info->xact, 
                                                               s->regh[ti->tasklet_idx]);

  s->member[ti->member_idx].cb_count++;
  EXPECT_GE(2, s->member[ti->member_idx].cb_count);

  RW_ASSERT(cursor);
  phone = (RWPB_T_MSG(DtsTest_data_Person_Phone)*)rwdts_member_reg_handle_get_next_element(
                                                                s->regh[ti->tasklet_idx], 
                                                                cursor,
                                                                (rwdts_xact_t*)xact_info->xact,
                                                                NULL);
  RW_ASSERT(phone);
  while (phone) {
    EXPECT_GT((sizeof(phones)/sizeof(char*)), idx);
    EXPECT_STREQ(phone->number, phones[idx]);
    idx++;
    phone = (RWPB_T_MSG(DtsTest_data_Person_Phone)*)rwdts_member_reg_handle_get_next_element(
                                                     s->regh[ti->tasklet_idx], 
                                                     cursor,
                                                     (rwdts_xact_t*)xact_info->xact,
                                                     NULL);
  }
  rwdts_member_data_delete_cursors((rwdts_xact_t*)xact_info->xact, 
                                   s->regh[ti->tasklet_idx]);
  if (s->member[ti->member_idx].cb_count == 1) {
    EXPECT_EQ(3, idx);
  } else {
    EXPECT_EQ(4, idx);
  }

  return RWDTS_ACTION_OK;
}

static void 
memberapi_test_data_member_cursor_query_second_cb(rwdts_xact_t* xact, 
                                                 rwdts_xact_status_t* xact_status,
                                                 void*         ud) 
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
   

  s->query_cb_rcvd++;
  TSTPRN("Received response for second transaction from client\n");
  ASSERT_EQ(2, s->query_cb_rcvd);
  s->exitnow = 1;

  return;
}
static void 
memberapi_test_data_member_cursor_query_cb(rwdts_xact_t* xact, 
                                           rwdts_xact_status_t* xact_status,
                                           void*         ud) 
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
   
  /* start one more transaction */


  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone)  keyspec_entry  = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec                                      = (rw_keyspec_path_t*)&keyspec_entry;

  char phone_num4[] = "444-456-789";
  keyspec_entry.dompath.path001.has_key00 = TRUE;
  keyspec_entry.dompath.path001.key00.number = phone_num4;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num4;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  
  s->query_cb_rcvd++;
  RW_ASSERT(s->query_cb_rcvd  == 1);
  rwdts_xact_t *new_xact = rwdts_api_query_ks(apih,keyspec,
                RWDTS_QUERY_CREATE,RWDTS_XACT_FLAG_ADVISE,
                memberapi_test_data_member_cursor_query_second_cb,
                ud,&phone->base);
  ASSERT_TRUE(new_xact);
  TSTPRN("Invoking second transaction [%p] from client\n", xact);

  return;
}

static void
memberapi_test_data_member_cursor_reg_ready (rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);
  s->reg_ready_called++;
 if (s->reg_ready_called == 3) {
   /* Start client after all members are going */
   TSTPRN("invoking client -- memberapi_test_data_member_cursor_f\n");
   dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC/10);
   rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                            when,
                            s->client.rwq,
                            &s->tenv->t[TASKLET_CLIENT],
                            memberapi_test_data_member_cursor_client_start_f);
  }
}

static void 
memberapi_test_data_member_cursor_f(void *ctx) 
{

  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_keyspec_path_t*           keyspec;
  rwdts_member_event_cb_t cb;
  rwdts_api_t*            apih = ti->apih;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Data member cursor...\n", ti->member_idx);

  if (s->member_startct != TASKLET_MEMBER_CT) { 
    RW_ZERO_VARIABLE(&cb);
    cb.ud = ctx;
    cb.cb.reg_ready = memberapi_test_data_member_cursor_reg_ready;
    cb.cb.precommit = memberapi_test_data_member_cursor_precommit_cb;

    RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
    keyspec = (rw_keyspec_path_t*)&keyspec_instance;

    rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

    rwdts_member_deregister_all(apih);

    /* Establish a registration */
    s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, 
                                                     keyspec, 
                                                     &cb, 
                                                     RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                                     RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE , 
                                                     NULL);

    ck_pr_inc_32(&s->member_startct);

    TSTPRN("invoking member -- memberapi_test_data_member_cursor_f , startct=%u\n", s->member_startct);
 }
}
static void memberapi_test_data_member_cursor(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             memberapi_test_data_member_cursor_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void 
memberapi_test_data_member_cursor_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s  = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_member_event_cb_t cb;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rwdts_xact_t*    xact   = NULL;
  uint64_t         corrid = 1000;
  rw_status_t      rs;
  uint32_t         flags = 0;

  RW_ZERO_VARIABLE(&cb);

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone)  keyspec_entry  = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  rw_keyspec_path_t *keyspec                                      = (rw_keyspec_path_t*)&keyspec_entry;

    /* Establish a registration */
  char phone_num1[] = "111-456-789";
  char phone_num2[] = "222-456-789";
  char phone_num3[] = "333-456-789";

  keyspec_entry.dompath.path001.has_key00 = TRUE;
  keyspec_entry.dompath.path001.key00.number = phone_num1;

  RWPB_T_MSG(DtsTest_data_Person_Phone) *phone, phone_instance;

  phone = &phone_instance;

  RWPB_F_MSG_INIT(DtsTest_data_Person_Phone,phone);

  phone->number = phone_num1;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;

  flags |= RWDTS_XACT_FLAG_ADVISE;

  xact = rwdts_api_xact_create(clnt_apih, flags, memberapi_test_data_member_cursor_query_cb, ctx);
  ASSERT_TRUE(xact);
  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  ASSERT_TRUE(blk);
  
  corrid = 111;
  rs = rwdts_xact_block_add_query_ks(blk,keyspec,s->client.action,
				     flags,corrid,&(phone->base));
  ASSERT_TRUE(rs == RW_STATUS_SUCCESS);
  
  corrid = 222;
  phone->number = phone_num2;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;
  keyspec_entry.dompath.path001.key00.number = phone_num2;
  rs = rwdts_xact_block_add_query_ks(blk,keyspec,RWDTS_QUERY_CREATE,flags,corrid,&(phone->base));
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);




  corrid = 333;
  phone->number = phone_num3;
  phone->has_type = TRUE;
  phone->type = DTS_TEST_PHONE_TYPE_MOBILE;
  keyspec_entry.dompath.path001.key00.number = phone_num3;
  rs = rwdts_xact_block_add_query_ks(blk,keyspec,RWDTS_QUERY_CREATE,flags,corrid,&(phone->base));
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  ASSERT_EQ(rwdts_xact_get_block_count(xact), 1);

  rs = rwdts_xact_block_execute(blk, flags, NULL, NULL, NULL);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rwdts_xact_commit(xact);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  return;
}

/*
 *  reroot list tests
 */
static void 
memberapi_test_reroot_list_query_cb(rwdts_xact_t* xact, 
                                    rwdts_xact_status_t* xact_status,
                                    void*         ud) 
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;
  rwdts_api_t*             apih = ti->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
   s->exitnow = 1;
  return;
}

static rwdts_member_rsp_code_t 
memberapi_reoot_list_prepare(const rwdts_xact_info_t* xact_info,
                             RWDtsQueryAction         action,
                             const rw_keyspec_path_t*      key,
                             const ProtobufCMessage*  msg, 
                             uint32_t credits,
                             void *getnext_ptr)
{
  RWPB_T_MSG(DtsTest_PhoneNumber)  *phone  = (RWPB_T_MSG(DtsTest_PhoneNumber)*)msg;

  if (phone) {
    TSTPRN("memberapi_reoot_list_prepare called  number = %s\n", phone->number);
  } else {
    TSTPRN("memberapi_reoot_list_prepare called msg = NULL\n");
  }
  return RWDTS_ACTION_OK;
}
static void
member_reroot_reg_ready(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  ck_pr_inc_32(&s->reg_ready_called);
  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("invoking client -- memberapi_test_reroot_list_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_reroot_list_client_start_f);
  }
}

static void 
memberapi_reroot_list_f(void *ctx) 
{

  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s = (struct queryapi_state *)ti->ctx;

  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rw_keyspec_path_t*           keyspec;
  rwdts_member_event_cb_t cb;
  rwdts_api_t*            apih = ti->apih;

  RW_ASSERT_TYPE(apih, rwdts_api_t);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d Reroot  list ...\n", ti->member_idx);

  if (s->member_startct != TASKLET_MEMBER_CT) { 
    RW_ZERO_VARIABLE(&cb);
    cb.ud = ctx;
    cb.cb.prepare = memberapi_reoot_list_prepare;
    cb.cb.reg_ready = member_reroot_reg_ready;

    RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_instance = *RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone);
    keyspec = (rw_keyspec_path_t*)&keyspec_instance;
    rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

    rwdts_member_deregister_all(apih);

    /* Establish a registration */
    s->regh[ti->tasklet_idx] = rwdts_member_register(NULL, apih, 
                                                     keyspec, 
                                                     &cb, 
                                                     RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                                     RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE , 
                                                     NULL);

    ck_pr_inc_32(&s->member_startct);

    TSTPRN("invoking member -- memberapi_test_reroot_list_f , startct=%u\n", s->member_startct);
 }
}

static void memberapi_test_reroot_list(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                             s->member[i].rwq, 
                             &s->tenv->t[TASKLET_MEMBER_0 + i], 
                             memberapi_reroot_list_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void 
memberapi_reroot_list_client_start_f(void *ctx) 
{
  struct tenv1::tenv_info* ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state*   s  = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rwdts_xact_t*    xact   = NULL;
  uint32_t         flags = 0;

  char person_name[] = "Jon Snow";
  char person_email[] = "Jonsnow@gmail.com";

  RWPB_T_PATHSPEC(DtsTest_data_Person)  keyspec_entry  = *(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
  rw_keyspec_path_t *keyspec                                = (rw_keyspec_path_t*)&keyspec_entry;

    /* Establish a registration */
  char phone_num1[] = "111-456-789";
  char phone_num2[] = "222-456-789";
  char phone_num3[] = "333-456-789";


  RWPB_T_MSG(DtsTest_PhoneNumber)  *phone1, phone1_instace;
  phone1 = &phone1_instace;
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber, phone1);
  phone1->number   = phone_num1;
  phone1->has_type = TRUE;
  phone1->type     = DTS_TEST_PHONE_TYPE_HOME;

  RWPB_T_MSG(DtsTest_PhoneNumber)  *phone2, phone2_instace;
  phone2 = &phone2_instace;
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber, phone2);
  phone2->number   = phone_num2;
  phone2->has_type = TRUE;
  phone2->type     = DTS_TEST_PHONE_TYPE_WORK;

  RWPB_T_MSG(DtsTest_PhoneNumber)  *phone3, phone3_instace;
  phone3 = &phone3_instace;
  RWPB_F_MSG_INIT(DtsTest_PhoneNumber, phone3);
  phone3->number   = phone_num3;
  phone3->has_type = TRUE;
  phone3->type     = DTS_TEST_PHONE_TYPE_MOBILE;
  
  RWPB_T_MSG(DtsTest_PhoneNumber) *phones[] = { phone1, phone2, phone3 };

  RWPB_T_MSG(DtsTest_data_Person) *person, person_instance;
  person = &person_instance;
  RWPB_F_MSG_INIT(DtsTest_data_Person,person);

  person->name = person_name;
  person->has_id = TRUE; 
  person->id = 2000;
  person->email = person_email;
  person->has_employed = person->employed = TRUE;
  person->n_phone = 3; 
  person->phone = phones;
  xact = rwdts_api_query_ks(clnt_apih,keyspec,
                RWDTS_QUERY_CREATE,flags|RWDTS_XACT_FLAG_ADVISE,
                memberapi_test_reroot_list_query_cb,
                ctx,&person->base);
  ASSERT_TRUE(xact);

  return;
}

static void
member_chain_reg_ready4(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  if (++s->num_responses == 4) {
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             when,
                             s->client.rwq,
                             &s->exitnow,
                             set_exitnow);
  }
  TSTPRN("### member_chain_reg_ready4 num_responses is now %d\n", s->num_responses);
}

static void
member_chain_reg_ready_nop(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  if (++s->num_responses == 4) {
    s->exitnow = true;
  }
  TSTPRN("### member_chain_reg_ready_nop num_responses is now %d\n", s->num_responses);
}

static void
member_chain_reg_ready1(rwdts_member_reg_handle_t regh,
                 rw_status_t               rs,
                 void*                     ctx)
{
  ASSERT_TRUE(ctx);

  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rwdts_member_event_cb_t cb;
  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = member_chain_reg_ready4;
  TSTPRN("starting 4th registration to allow pushing 3 as well...\n"); 

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec_entry.dompath.path001.has_key00 = 1;
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);

  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("976");

  s->num_responses++;
  ASSERT_TRUE (s->num_responses < 4);
  TSTPRN("### member_chain_reg_ready1 num_responses is now %d\n", s->num_responses);
    
  rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
 RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

static void memberapi_client_chain_reg_advise(void *ctx) 
{
  struct queryapi_state *s = (struct queryapi_state *)ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  s->num_responses = 0;		// this will go to 4, one per reg-ready callback

  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);

  rwdts_member_event_cb_t cb;
  RW_ZERO_VARIABLE(&cb);
  cb.cb.reg_ready = member_chain_reg_ready_nop;
  cb.ud = ctx;
  TSTPRN("starting 1st registration in client chain reg...\n");

  RWPB_T_PATHSPEC(DtsTest_data_Person_Phone) keyspec_entry  = (*RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));
  keyspec_entry.dompath.path001.has_key00 = 1;
  rw_keyspec_path_t *keyspec = (rw_keyspec_path_t*) &keyspec_entry;
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_DATA);


  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("973");
  rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready = member_chain_reg_ready1;
  TSTPRN("starting 2nd registration to pend...\n");
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("974");
  rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);

  RW_ZERO_VARIABLE(&cb);
  cb.cb.reg_ready = member_chain_reg_ready_nop;
  cb.ud = ctx;
  TSTPRN("starting 3rd registration to pend ...\n");

  RW_FREE(keyspec_entry.dompath.path001.key00.number);
  keyspec_entry.dompath.path001.key00.number = RW_STRDUP("975");
  rwdts_memberapi_register(NULL, clnt_apih, 
                                  keyspec, 
                                  &cb, 
                                  RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone),
                                  RWDTS_FLAG_PUBLISHER, NULL);
  RW_FREE(keyspec_entry.dompath.path001.key00.number);
}

// FIXME
TEST_F(RWDtsEnsemble, RegisterOnRegReady) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_client_chain_reg_advise(s);
  double seconds = (RUNNING_ON_VALGRIND)?180:45;
  rwsched_dispatch_main_until(s->tenv->t[TASKLET_CLIENT].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
  free(s);
}


/* 
 *  The following are all the non transactional Query tests
 */

/*
 * Single read query - reponded by all the members
 */

TEST_F(RWDtsEnsemble, MemberAPINonTransactional_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_READ, FALSE, FALSE);
  s->client.usebuilderapi = TRUE;
  memberapi_test_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalMultiple_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_READ, FALSE, FALSE);
  s->client.usebuilderapi = TRUE;
  memberapi_test_execute_multiple(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAPITransactionalMultiple_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_READ, FALSE, FALSE);
  s->client.usebuilderapi = TRUE;
  s->client.transactional = TRUE;
  memberapi_test_execute_multiple(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

/*
 * Use the builder API to do a non transactional  read
 */

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalUseBuilder_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s->client.usebuilderapi = TRUE;
  memberapi_test_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

/*
 * Members use ANYCAST flag for registration
 */

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalAnyCast_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_anycast_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Member use SUB_READ flag forqueryregistration
 */

TEST_F(RWDtsEnsemble, MemberAPISubRead_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_subread_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalAppend_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.wildcard = TRUE;
  memberapi_append_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalMultiRAppend)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.wildcard = TRUE;
  s.multi_router = TRUE;
  memberapi_append_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalAppendMatchless_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);

  s.client.wildcard = FALSE;
  memberapi_append_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalAppendAfter)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.after = 1;
  memberapi_append_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalReg)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  memberapi_transreg_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalShared)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_shared_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Let all the members respond with an ASYNC
 */

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalAsyncResponse_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s.member[i].treatment = TREATMENT_ASYNC;
  }
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Let all the members respond with an ASYNC followed by NACK
 */
TEST_F(RWDtsEnsemble, MemberAPINonTransAsyncNackResponse_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.ignore = 1;
  s.client.expect_result = XACT_RESULT_ERROR;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s.member[i].treatment = TREATMENT_ASYNC_NACK;
  }
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Let all the members respond with
 * an async responseo
 */
TEST_F(RWDtsEnsemble, PerfMemberAPIGetQueryAsync_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.usebuilderapi = TRUE;
  s.client.testperf_numqueries = 1000;
  s.client.testperf_itercount = 1;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s.member[i].treatment = TREATMENT_ASYNC;
  }
  s.tenv->t[TASKLET_CLIENT].apih->tx_serialnum = -200;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}
/*
 * Simple update
 */

TEST_F(RWDtsEnsemble, SimpleUpdate_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, IDENTShard_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  memberapi_test_execute_ident(&s);
  memset(&s, 0xaa, sizeof(s));
}
TEST_F(RWDtsEnsemble, IDENTShardUpdate_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  memberapi_test_execute_ident_update(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RangeShard_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  memberapi_test_execute_range(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Performance test for large keyspec registrations
 */
TEST_F(RWDtsEnsemble, PerfTest_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  memberapi_test_execute_perf(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Let all the members respond with
 * an async responseo
 */
TEST_F(RWDtsEnsemble, PerfMemberAPIUpdateQueryAck_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.usebuilderapi = TRUE;
  s.client.testperf_numqueries = 10;
  s.client.testperf_itercount = 1;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/*
 * Let all the members respond with
 * an async responseo
 */

TEST_F(RWDtsEnsemble, PerfMemberAPIGetQueryAck_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.usebuilderapi = TRUE;
  s.client.testperf_numqueries = 150;
  s.client.testperf_itercount = 1;
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {                         
    s.member[i].treatment = TREATMENT_ACK;
  }
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}



/*
 * Let all the members respond with
 * an async responseo
 */
TEST_F(RWDtsEnsemble, MemberAPIGetQuery_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.usebuilderapi = TRUE;
  for (int i=0; i < TASKLET_MEMBER_CT; i++)
  {
    s.member[i].treatment = TREATMENT_ASYNC;
  }
  s.client.expect_result = XACT_RESULT_COMMIT;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}


/*
 * Transactional test cases
 */


/*
 * A transactional query where all the members 
 * Ack
 */

TEST_F(RWDtsEnsemble, MemberAPITransactional_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/* 
 *  Transactional query where one member Nacks
 */

TEST_F(RWDtsEnsemble, MemberAPITransactionalNACK_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.expect_result = XACT_RESULT_ABORT;
  s.member[0].treatment = TREATMENT_NACK;
  memberapi_test_execute(&s);
}

/* 
 *  Transactional query where one member Nacks
 */


TEST_F(RWDtsEnsemble, MemberAPITransactionalPreCommitNACK)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.expect_result = XACT_RESULT_ABORT;
  s.member[0].treatment = TREATMENT_PRECOMMIT_NACK;
  memberapi_test_execute(&s);
  // int memb_xact_cnt = rwdts_test_get_running_xact_count();
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalPreCommitNull_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.member[0].treatment = TREATMENT_PRECOMMIT_NULL;
  memberapi_test_execute(&s);
  // int memb_xact_cnt = rwdts_test_get_running_xact_count();
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalAbortNACK_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.expect_result = XACT_RESULT_ABORT; //??
  int tasknum = rand()%3;
  s.member[tasknum].treatment = TREATMENT_PRECOMMIT_NACK;
  s.member[(tasknum+1)%3].treatment = TREATMENT_ABORT_NACK;
  memberapi_test_execute(&s);
  // int memb_xact_cnt = rwdts_test_get_running_xact_count();
  memset(&s, 0xaa, sizeof(s));
}

/* 
 *  Transactional query where one member Nacks
 */

TEST_F(RWDtsEnsemble, MemberAPITransactionalMultipleNACK_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.expect_result = XACT_RESULT_ABORT;
  s.member[0].treatment = TREATMENT_NACK;
  s.member[1].treatment = TREATMENT_NACK;
  memberapi_test_execute(&s);
  // int memb_xact_cnt = rwdts_test_get_running_xact_count();
  memset(&s, 0xaa, sizeof(s));
}

/* 
 *  Transactional query where one member sends NA
 */

TEST_F(RWDtsEnsemble, MemberAPITransactionalNA) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.exit_on_member_xact_finished = TRUE;
  s.member[0].treatment = TREATMENT_NA;
  s.member[1].treatment = TREATMENT_NA;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/* 
 *  Transactional query where one member sends NA (bounces, huh?)
 */

TEST_F(RWDtsEnsemble, MemberAPITransactionalBNC_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.include_nonmember = TRUE;
  s.member[0].treatment = TREATMENT_BNC;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

/* 
 *  Transactional query where one member sends NA
 */

TEST_F(RWDtsEnsemble, MemberAPITransactionalDelayPrepare_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.include_nonmember = TRUE;
  s.member[0].treatment = TREATMENT_PREPARE_DELAY;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, ClientAbort_MemberDelay_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.client_abort =  TRUE;
  s.client.expect_result = XACT_RESULT_ABORT;
  s.include_nonmember = TRUE;
  s.member[0].treatment = TREATMENT_PREPARE_DELAY;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalDelayMultiple_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.include_nonmember = TRUE;
  s.member[0].treatment = TREATMENT_PREPARE_DELAY;
  s.member[1].treatment = TREATMENT_PREPARE_DELAY;
  s.member[2].treatment = TREATMENT_PREPARE_DELAY;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

// Need test cases for each of precommit and commit-time member nack, error, async, and/or bounce returns


TEST_F(RWDtsEnsemble, DataMemberAPIPubSubNotification_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 18; // 1 set sync, 1 set async
  s.expected_advise_rsp_count = 6; // Will increment this as we go along add tests
  memberapi_test_pub_sub_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, DataMemberAPIPubSubminikeyNotification_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.expected_notification_count = 9 * 2; // 1 set sync, 1 set async
  s.expected_advise_rsp_count = 6;
  memberapi_test_pub_sub_minikey_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, DataMemberAPIPubSubCacheNotification_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 24;
  s.expected_advise_rsp_count = 8;
  s.ignore = 1;
  memberapi_test_pub_sub_cache_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, TrackDataMemberAPIPubSubCacheXactNotification_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  //s.expected_notification_count = 9;
  //s.expected_advise_rsp_count = 3;
  s.notification_count = 0;
  s.ignore = 1;
  memberapi_test_pub_sub_cache_xact_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, DataMemberAPIPubSubnonlistyNotification_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 9;
  s.expected_advise_rsp_count = 3;
  s.ignore = 1;
  memberapi_test_pub_sub_non_listy_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalMULTIxactreroot_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.reroot_test = REROOT_FIRST;
  s.ignore = 1;
  memberapi_test_multi_xact_reroot_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}


TEST_F(RWDtsEnsemble, MemberAPITransactionalMULTIxactexpand) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.reroot_test = REROOT_SECOND;
  s.ignore = 1;
  memberapi_test_multi_xact_expand_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPITransactionalXactIterReroot_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, TRUE, FALSE);
  s.client.reroot_test = REROOT_THIRD;
  s.ignore = 1;
  memberapi_test_xact_iter_reroot_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIRegDeReg_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 3;
  memberapi_test_reg_dereg_notification(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, DataMemberAPIPubSubCacheKeyNote_LONG) 
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 18;
  s.expected_advise_rsp_count = 6;
  s.ignore = 1;
  memberapi_test_pub_sub_cache_key_note(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalMerge) {
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_merge_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalPublish)
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_publish_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINonTransactionalStream_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.stream_results = 3;
  memberapi_merge_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIEmptyResp_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_empty_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIOrderAsyncResp_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_READ, FALSE, FALSE);
  s->total_queries = 0;
  memberapi_async_test_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberAPIPromoteSub_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_promote_sub_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberSubAppConfCommit_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  memberapi_sub_appconf_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberSubAppConfAbort_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->abort = 1;
  memberapi_sub_appconf_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberAppConfDelete_MultiAdvice)
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.delete_test = 1;		// deletes the colony (container above the network contexts at the registration point)
  s.Dlevel = 4;
  s.Dleftover = 2;
  memberapi_appconf_delete(&s);
}

TEST_F(RWDtsEnsemble, MemberAppConfDelete_0)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;		// deletes the colony (container above the network contexts at the registration point)
  s->Dlevel = 0;
  s->Dleftover = 0;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDelete_1)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;		// deletes  context[*] (objects at the registration point)
  s->Dlevel = 1;
  s->Dleftover = 0;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDelete_2)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = 2;		// deletes one context[foo] (at the registraiton point)
  s->Dleftover = 1;
  memberapi_appconf_delete(s);

  /* Validate the correct Interfaces in member 0's appdelcontext1 object */
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface1"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface2"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface3"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface4"));

  /* Validate the correct number of Interfaces in member 0's appdelcontext2 object */
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface1"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface2"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface3"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface4"));
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberAppConfDelete_3)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = 3;		// deletes all interfaces interface[*] inside the context (beneath the registraiton point)
  s->Dleftover = 2;

  memberapi_appconf_delete(s);

  /* Validate the correct Interfaces in member 0's appdelcontext1 object */
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface1"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface2"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface3"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface4"));

  /* Validate the correct number of Interfaces in member 0's appdelcontext2 object */
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface1"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface2"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface3"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface4"));
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

/* Multiple creates with same keyspec followed by Delete. */
 
TEST_F(RWDtsEnsemble, MemberAppConfAdviseDelete_3)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = 3;		// deletes all interfaces interface[*] inside the (one) context (beneath the registraiton point)
  s->Dleftover = 1;
  s->MultipleAdvise = TRUE;	// only one context name, not appdelcontext%d

  memberapi_appconf_delete(s);

  /* Validate the correct Interfaces in member 0's appdelcontext object */
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext",
					     "Interface1"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext",
					     "Interface2"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext",
					     "Interface3"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext",
					     "Interface4"));
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDelete_4)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = 4;		// deletes one interface inside the context (beneath the registraiton point)
  s->Dleftover = 2;      // two contexts, just an interface inside

  memberapi_appconf_delete(s);

  /* Validate the correct Interfaces in member 0's appdelcontext1 object */
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface1"));
  ASSERT_EQ(0, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface2"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface3"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext1",
					     "Interface4"));

  /* Validate the correct number of Interfaces in member 0's appdelcontext2 object */
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface1"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface2"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface3"));
  ASSERT_EQ(1, memberapi_dumpmember_nc_iface(&s->tenv->t[TASKLET_MEMBER_0],
					     "appdelcontext2",
					     "Interface4"));

  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDeleteMiss_1)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = -1;                // deletes using a nonexisting colony (container above the registration) name
  s->Dleftover = 2;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDeleteMiss_2)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = -2;       
  s->Dleftover = 2;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDeleteMiss_3)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = -3;       
  s->Dleftover = 2;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}
TEST_F(RWDtsEnsemble, MemberAppConfDeleteMiss_4)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->delete_test = 1;
  s->Dlevel = -4;       
  s->Dleftover = 2;
  memberapi_appconf_delete(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}



/*
 * DTS Audit related tests 
 */
TEST_F(RWDtsEnsemble, DTSAuditReconcileSuccess_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  s->magic = QAPI_STATE_MAGIC;
  s->testno = tenv.testno;
  s->tenv = &tenv;
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->audit_action = RWDTS_AUDIT_ACTION_RECONCILE;
  s->audit_test = RW_DTS_AUDIT_TEST_RECONCILE_SUCCESS;
  memberapi_test_audit_tests(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DTSAuditReport_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  s->audit_action = RWDTS_AUDIT_ACTION_REPORT_ONLY;
  s->audit_test = RW_DTS_AUDIT_TEST_REPORT;
  memberapi_test_audit_tests(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DTSAuditRecovery_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  s->audit_action = RWDTS_AUDIT_ACTION_RECOVER;
  s->audit_test = RW_DTS_AUDIT_TEST_RECOVERY;
  memberapi_test_audit_tests(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DTSAuditFailConverge_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->audit_action = RWDTS_AUDIT_ACTION_RECOVER;
  s->audit_test = RW_DTS_AUDIT_TEST_FAIL_CONVERGE;
  memberapi_test_audit_tests(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

static void 
myapp_cache_upd_api_cb(rwdts_member_reg_handle_t regh,
          const rw_keyspec_path_t*        keyspec,
          const ProtobufCMessage*    msg,
          void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  rw_status_t status;

  TSTPRN("myapp_cache_upd_api_cb\n");
  
  RWPB_M_MSG_DECL_INIT(TestdtsRwFpath_ConfigNumaMap,nmap);
  nmap.n_port = 1;
  const char macin[] = { 0xd, 0xe, 0xa, 0xd, 0xf, 0xf };
  memcpy(nmap.port[0].mac_address, macin, 6);
  nmap.port[0].has_mac_address = 1;
  nmap.port[0].has_numa_socket = TRUE;
  nmap.port[0].numa_socket = 4;

  rw_keyspec_path_t* key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap));

  RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap) kpe  = (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap));

  key = (rw_keyspec_path_t*)&kpe;
  kpe.dompath.path000.has_key00 = TRUE;
  kpe.dompath.path000.key00.id = 1;

  rw_keyspec_path_set_category(key, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  status = rwdts_member_reg_handle_create_element_keyspec(s->client_regh,
                                                 (rw_keyspec_path_t*)key, 
                                                 &nmap.base,
                                                 NULL);

  RW_ASSERT(status == RW_STATUS_SUCCESS);

  return;
}

static void appconf_client_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  char *strks = NULL;
  rwdts_member_event_cb_t cb;


  TSTPRN("AppConf test client start...\n");
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;
  ASSERT_TRUE(clnt_apih);
  
  rw_keyspec_path_t *keyspec_gen = NULL;
  const rw_keyspec_path_t *constks;
  constks  = (rw_keyspec_path_t*)(&(*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_create_dup(constks, NULL, &keyspec_gen);

  // how to fill in keys along the path?
  auto *keyspec = (RWPB_T_PATHSPEC(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap) *)keyspec_gen;

  /* Ruh-roh, why does filling in a key for this object make it not match at wildcard registrants? */
  if (1) {
    keyspec->dompath.path000.has_key00 = TRUE;
    keyspec->dompath.path000.key00.id = 1;
  }
  rw_keyspec_path_get_new_print_buffer(keyspec_gen, NULL, NULL, &strks);
  TSTPRN("AppConf client query keyspec='%s'\n", strks ? strks : "");
  free(strks);

  rw_keyspec_path_set_category((rw_keyspec_path_t*)keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);

  RW_ZERO_VARIABLE(&cb);
  cb.ud = ctx;
  cb.cb.reg_ready_old = myapp_cache_upd_api_cb;
  clnt_apih->stats.num_member_advise_rsp = 0;
  s->client_regh = rwdts_member_register(NULL, clnt_apih,
                        (rw_keyspec_path_t*)keyspec,
                        &cb,
                        RWPB_G_MSG_PBCMD(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap),
                        RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_CACHE|RWDTS_FLAG_SHARED,
                        NULL);
  rw_keyspec_path_free(keyspec_gen, NULL);
}

typedef struct {
  int _what;
} myapp_conf1_xact_scratch_t;

static void* myapp_conf1_xact_init(rwdts_appconf_t *ac,
                                   rwdts_xact_t *xact,
                                   void *ud) 
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);

  struct tenv1::tenv_info *ti = myapp->ti;
  RW_ASSERT(ti);
  ti->cb.xact_init++;

  TSTPRN("myapp_conf1_xact_nit[memb=%d] xact_init_cb_cnt = %d\n",
          myapp->ti->member_idx, ti->cb.xact_init);

  return RW_MALLOC0_TYPE(sizeof(myapp_conf1_xact_scratch_t), myapp_conf1_xact_scratch_t);
}

static void set_exitnow(void *ud) {
  TSTPRN("set_exitnow() set exitnow to 1, ud=%p\n", ud);
  uint32_t *exitnow = (uint32_t *)ud;
  *exitnow = 1;
}

static void myapp_conf1_xact_deinit(rwdts_appconf_t *ac,
				    rwdts_xact_t *xact, 
				    void *ud,
				    void *scratch_in) 
{
  myappconf_t *myapp = (myappconf_t *)ud;
  RW_ASSERT(myapp->s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(myapp);
  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);

  ti->cb.xact_deinit++;

  TSTPRN("myapp_conf1_xact_deinit[memb=%d] xact_init_cb_cnt = %d\n",
          myapp->ti->member_idx, ti->cb.xact_deinit);
  char str[128];
  TSTPRN("myapp_conf1_xact_deinit = %p, %s\n", xact, rwdts_xact_id_str(&xact->id, str, sizeof(str)));
  RW_ASSERT(myapp->s->exit_soon >= 0);
  myapp->s->exit_soon++;

  TSTPRN("myapp_conf1_xact_deinit = %p, %s / now exit_soon=%d\n", xact, rwdts_xact_id_str(&xact->id, str, sizeof(str)), myapp->s->exit_soon);

  RW_ASSERT(myapp->s->exit_soon <= 9);
  if (myapp->s->exit_soon == 9) {
    // myapp->s->exitnow = TRUE;
    myapp->s->exit_soon = -999;
    TSTPRN("myapp_conf1_xact_deinit = %p, %s / dispatching set_exitnow in 1/3 second\n", xact, rwdts_xact_id_str(&xact->id, str, sizeof(str)));
    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
    rwsched_dispatch_after_f(myapp->s->tenv->t[TASKLET_MEMBER_0].tasklet, 
			     when,
			     myapp->s->member[0].rwq, 
			     &myapp->s->exitnow,
			     set_exitnow);
  }

  RW_ASSERT_TYPE(scratch_in, myapp_conf1_xact_scratch_t);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;

  RW_ASSERT(scratch);
  RW_FREE_TYPE(scratch, myapp_conf1_xact_scratch_t);
  is_test_done(myapp->s);
}

static void myapp_conf1_prepare_numamap(rwdts_api_t *apih,
					rwdts_appconf_t *ac, 
					rwdts_xact_t *xact,
					const rwdts_xact_info_t *xact_info,
					rw_keyspec_path_t *keyspec, 
					const ProtobufCMessage *pbmsg, 
					void *ctx, 
					void *scratch_in) 
{
  const char macin[] = { 0xd, 0xe, 0xa, 0xd, 0xf, 0xf };
  myappconf_t *myapp = (myappconf_t *)ctx;

  ASSERT_TRUE(myapp);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.prepare++;

  TSTPRN("myapp_conf1_prepare_numamap[memb=%d] regh= %p, scratch=%p ,prep_cb = %d\n",
          myapp->ti->member_idx, xact_info->regh, scratch, ti->cb.prepare);

  RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *)pbmsg;
  RW_ASSERT(RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap) == pbmsg->descriptor);

  EXPECT_EQ(nmap->n_port, 1); 
  EXPECT_EQ(nmap->port[0].numa_socket, 4); 
  EXPECT_FALSE(memcmp(&nmap->port[0].mac_address, &macin,6));

  /* These can be called later, the prepare operation can go off and
     do subtransactions, messaging, whatever, before it completes */
  if (0) {
    rwdts_appconf_prepare_complete_fail(ac, xact_info, RW_STATUS_OUT_OF_BOUNDS, "That's a crazy value");
  } else {
    rwdts_appconf_prepare_complete_ok(ac, xact_info);
  }

  nmap=nmap;
  is_test_done(myapp->s);
}

static void
appconf_test_delay_prepare_complete_ok (appconf_scratch_t *tmp) 
{
  rwdts_appconf_prepare_complete_ok(tmp->ac, tmp->xact_info);
  RW_FREE(tmp);
}

static void myapp_conf1_prepare_numamap_delay(rwdts_api_t *apih,
					rwdts_appconf_t *ac, 
					rwdts_xact_t *xact,
					const rwdts_xact_info_t *xact_info,
					rw_keyspec_path_t *keyspec, 
					const ProtobufCMessage *pbmsg, 
					void *ctx, 
					void *scratch_in) 
{
  const char macin[] = { 0xd, 0xe, 0xa, 0xd, 0xf, 0xf };
  myappconf_t *myapp = (myappconf_t *)ctx;
  uint32_t delay_sec = 2;
  appconf_scratch_t *appconf_tmp;

  RW_ASSERT(myapp);
  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.prepare++;

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;
  TSTPRN("myapp_conf1_prepare_numamap_delay[%d] called, reg=%p, scratch=%p\n", myapp->ti->member_idx, xact_info->regh, scratch);

  RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *)pbmsg;
  RW_ASSERT(RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap) == pbmsg->descriptor);

  EXPECT_EQ(nmap->n_port, 1); 
  EXPECT_EQ(nmap->port[0].numa_socket, 4); 
  EXPECT_FALSE(memcmp(&nmap->port[0].mac_address, &macin,6));

  /* These can be called later, the prepare operation can go off and
     do subtransactions, messaging, whatever, before it completes */
    
    appconf_tmp = (appconf_scratch_t*) RW_MALLOC0(sizeof(appconf_scratch_t));
    appconf_tmp->ac = ac;
    appconf_tmp->xact_info = xact_info;

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, delay_sec * NSEC_PER_SEC);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             appconf_tmp,
                             (dispatch_function_t)appconf_test_delay_prepare_complete_ok);

  nmap=nmap;
}

static void myapp_conf1_prepare_numamap_nok(rwdts_api_t *apih,
					rwdts_appconf_t *ac, 
					rwdts_xact_t *xact,
					const rwdts_xact_info_t *xact_info,
					rw_keyspec_path_t *keyspec, 
					const ProtobufCMessage *pbmsg, 
					void *ctx, 
					void *scratch_in) 
{
  const char macin[] = { 0xd, 0xe, 0xa, 0xd, 0xf, 0xf };
  myappconf_t *myapp = (myappconf_t *)ctx;

  RW_ASSERT(myapp);
  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.prepare++;


  RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *)pbmsg;
  RW_ASSERT(RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap) == pbmsg->descriptor);

  EXPECT_EQ(nmap->n_port, 1); 
  EXPECT_EQ(nmap->port[0].numa_socket, 4); 
  EXPECT_FALSE(memcmp(&nmap->port[0].mac_address, &macin,6));

  rwdts_appconf_prepare_complete_fail(ac, xact_info, RW_STATUS_OUT_OF_BOUNDS, "That's a crazy value");
  nmap=nmap;
  is_test_done(myapp->s);
}

static void myapp_conf1_prepare_numamap_na(rwdts_api_t *apih,
					   rwdts_appconf_t *ac, 
					   rwdts_xact_t *xact,
					   const rwdts_xact_info_t *xact_info,
					   rw_keyspec_path_t *keyspec, 
					   const ProtobufCMessage *pbmsg, 
					   void *ctx, 
					   void *scratch_in) 
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  ASSERT_TRUE(myapp);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  ti->cb.prepare++;

  TSTPRN("myapp_conf1_prepare_numamap_na[memb=%d] regh= %p, scratch=%p ,prep_cb = %d\n",
          myapp->ti->member_idx, xact_info->regh, scratch, ti->cb.prepare);

  RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *)pbmsg;
  RW_ASSERT(RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap) == pbmsg->descriptor);

  //...

  rwdts_appconf_prepare_complete_na(ac, xact_info);

  nmap=nmap;
  is_test_done(myapp->s);
}

void myapp_conf1_validate(rwdts_api_t *apih,
			  rwdts_appconf_t *ac,
			  rwdts_xact_t *xact, 
			  void *ctx, 
			  void *scratch_in) 
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  ASSERT_TRUE(myapp);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;
//  rw_keyspec_path_t *get_keyspec = NULL;

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);

  ti->cb.validate++;
  TSTPRN("myapp_conf1_validate[memb=%d] validate = %d\n",
          myapp->ti->member_idx, ti->cb.validate);

  if(TREATMENT_ABORT_NACK == myapp->member->treatment)
  {
    TSTPRN("Delaying \n");
    sleep(35);
  }

  if (0) {
    /* Pattern for keyed/listy items, get a cursor and iterate */
    rwdts_member_cursor_t *cur = NULL;  
    //..


    cur=cur;
  }

#if 0
  RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = NULL;
  nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap)*)rwdts_member_reg_handle_get_element_keyspec(myapp->conf1.reg_nmap, NULL, xact, NULL, &get_keyspec);
  if (nmap) {
    unsigned int p;
    for (p=0; p<nmap->n_port; p++) {
      TSTPRN("  + port[%u] mac=%02x:%02x:%02x:%02x:%02x:%02x has_numa_socket=%d numa_socket=%d\n",
	      p,
	      nmap->port[p].mac_address[0], 
	      nmap->port[p].mac_address[1], 
	      nmap->port[p].mac_address[2], 
	      nmap->port[p].mac_address[3], 
	      nmap->port[p].mac_address[4], 
	      nmap->port[p].mac_address[5], 
	      nmap->port[p].has_numa_socket,
	      nmap->port[p].numa_socket);
    }
  } else {
    TSTPRN("myapp_conf1_validate[%u] found no nmap(s)\n", myapp->ti->member_idx);
  }
#endif


  scratch=scratch;
  is_test_done(myapp->s);
}
			  
void myapp_conf1_apply(rwdts_api_t *apih,
		       rwdts_appconf_t *ac,
		       rwdts_xact_t *xact,
		       rwdts_appconf_action_t action,
		       void *ctx, 
		       void *scratch_in) 
{

  myappconf_t *myapp = (myappconf_t *)ctx;
  ASSERT_TRUE(myapp);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;
  rw_keyspec_path_t *get_keyspec = NULL;

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);
  ti->cb.apply++;
  
  if (myapp->conf_apply_count) {
    if (!strcmp(myapp->s->test_name,"AppConf_Delay_LONG") ) {
      ASSERT_TRUE(myapp->conf_apply_count == 1);
      myapp->conf_apply_count = 0;
    }
    if (!strcmp(myapp->s->test_name,"AppConf_PrepNotOk_LONG") ) {
      ASSERT_TRUE(myapp->conf_apply_count == 1);
      myapp->conf_apply_count = 0;
    }
    if (!strcmp(myapp->s->test_name,"AppConf_Cache_Success_LONG") ) {
      ASSERT_TRUE(myapp->conf_apply_count == 1);
      myapp->conf_apply_count = 0;
    }
    if (!strcmp(myapp->s->test_name,"AppConf_Cache_Update_Delay_LONG") ) {
      ASSERT_TRUE(myapp->conf_apply_count == 1);
      myapp->conf_apply_count = 0;
    }
  }
  
  ASSERT_TRUE(myapp->conf_apply_count == 0);
  myapp->conf_apply_count++;
  TSTPRN("myapp_conf1_apply[%d] called with action=%s xact=%p scratch=%p num_apply[%d]\n", 
	  myapp->ti->member_idx,
	  ( action == RWDTS_APPCONF_ACTION_INSTALL ? "INSTALL"
	    : action == RWDTS_APPCONF_ACTION_RECONCILE ? "RECONCILE"
	    : "?????"),
	  xact,
	  scratch_in,
    ti->cb.apply);

  if (xact) {
    /* Scratch exists while the xact exists */
    RW_ASSERT_TYPE(scratch_in, myapp_conf1_xact_scratch_t);
  }

  switch (action) {

  case RWDTS_APPCONF_ACTION_INSTALL:
  {
    /* The given configuration *must* now be taken, or the task will
       probably be shot in the head. */

    /* Scratch may or may not exist!  Needs improvement, maybe we
       should synthesize a walk of the appconf registration set and
       calls to prepare with an INSTALL flag. */

    struct queryapi_state *s = myapp->s;
    ASSERT_TRUE(s);

    ck_pr_inc_32(&s->member_startct);
    if (s->member_startct == TASKLET_MEMBER_CT) {
      /* Start client after all members are going */
      TSTPRN("Member start_f invoking client start_f\n");

      // meh.  should use registration ready callback, sort this with bootstrap/dts/member/registration state machine features
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
             when,
             s->client.rwq,
             &s->tenv->t[TASKLET_CLIENT],
             appconf_client_start_f);
    }
  }

  break;

  case RWDTS_APPCONF_ACTION_RECONCILE:

    /* The given configuration should be reconciled with the app
       state.  If it cannot be done, returning failure will give a new
       event with the rollback config, not just in this task but
       across the universe. */

    if (0) {
      /* The actual proposed config in question lives in the xact.  Note
         that xact may be NULL, when we're rolling back; not a
         problem. */
      rw_status_t rs;
      RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = NULL;
      rs = rwdts_member_reg_handle_get_element_keyspec(myapp->conf1.reg_nmap,
                                                       NULL,
                                                       xact,
                                                       &get_keyspec,
                                                       (const ProtobufCMessage**)&nmap);
      EXPECT_EQ(RW_STATUS_SUCCESS, rs);
      if (nmap) {
        TSTPRN("myapp_conf1_apply[%d] found nmap with %lu ports\n", myapp->ti->member_idx, nmap->n_port);
        unsigned int p;
        for (p=0; p<nmap->n_port; p++) {
          TSTPRN("  + port[%d] mac=%02x:%02x:%02x:%02x:%02x:%02x has_numa_socket=%d numa_socket=%d\n",
                  p,
                  nmap->port[p].mac_address[0],
                  nmap->port[p].mac_address[1],
                  nmap->port[p].mac_address[2],
                  nmap->port[p].mac_address[3],
                  nmap->port[p].mac_address[4],
                  nmap->port[p].mac_address[5],
                  nmap->port[p].has_numa_socket,
                  nmap->port[p].numa_socket);

          /* Do something with this numa port */
          if (0) {
            // rwdts_member_xact_add_issue_ks()...?
            rwdts_appconf_xact_add_issue(ac, xact,
                                         RW_STATUS_FAILURE, "Unable to deal with this port for some reason.");
          }
        }

        /* Pretend we are happy; seems to be no test here for unhappy result? */
        //        myapp->s->exit_soon++;

      } else {
	TSTPRN("myapp_conf1_apply[%d] found no nmap(s)\n", myapp->ti->member_idx);
      }
      /* ...and so on for every damned thing in the myapp->conf1.ac appconf grouping */
    }

    break;
    case RWDTS_APPCONF_ACTION_AUDIT:
    break;
    case RWDTS_APPCONF_ACTION_RECOVER:
    break;
  }


  scratch=scratch;
  is_test_done(myapp->s);
}

void myapp_conf1_appconf_success_apply(rwdts_api_t *apih,
		       rwdts_appconf_t *ac,
		       rwdts_xact_t *xact,
		       rwdts_appconf_action_t action,
		       void *ctx, 
		       void *scratch_in) 
{

  myappconf_t *myapp = (myappconf_t *)ctx;
  ASSERT_TRUE(myapp);

  myapp_conf1_xact_scratch_t *scratch = (myapp_conf1_xact_scratch_t *)scratch_in;
  rw_keyspec_path_t *get_keyspec = NULL;

  struct tenv1::tenv_info *ti = myapp->ti;
  ASSERT_TRUE(ti);
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  ASSERT_TRUE(s);
  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);
  ti->cb.apply++;

  if (myapp->conf_apply_count) {
    if (!strcmp(myapp->s->test_name,"AppConf_Success_LONG") ) {
      ASSERT_TRUE(myapp->conf_apply_count == 1);
      myapp->conf_apply_count = 0;
    }
  }
  ASSERT_TRUE(myapp->conf_apply_count == 0);
  myapp->conf_apply_count++;
  TSTPRN("myapp_conf1_appconf_success_apply[%d] called with action=%s xact=%p scratch=%p num_apply[%d]\n", 
	  myapp->ti->member_idx,
	  ( action == RWDTS_APPCONF_ACTION_INSTALL ? "INSTALL"
	    : action == RWDTS_APPCONF_ACTION_RECONCILE ? "RECONCILE"
	    : "?????"),
	  xact,
	  scratch_in,
    ti->cb.apply);

  if (xact) {
    /* Scratch exists while the xact exists */
    RW_ASSERT_TYPE(scratch_in, myapp_conf1_xact_scratch_t);
  }

  switch (action) {

  case RWDTS_APPCONF_ACTION_INSTALL:

    /* The given configuration *must* now be taken, or the task will
       probably be shot in the head. */

    /* Scratch may or may not exist!  Needs improvement, maybe we
       should synthesize a walk of the appconf registration set and
       calls to prepare with an INSTALL flag. */
     ck_pr_inc_32(&s->install);
     if (s->install == 3) {
       /* Start client after all members are going */
       TSTPRN("Member start_f invoking client start_f\n");
       dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC *1/6);
       rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
           when,
           s->client.rwq,
           &s->tenv->t[TASKLET_CLIENT],
           appconf_client_start_f);
      }
    break;

  case RWDTS_APPCONF_ACTION_RECONCILE:

    /* The given configuration should be reconciled with the app
       state.  If it cannot be done, returning failure will give a new
       event with the rollback config, not just in this task but
       across the universe. */

    if (0) {
      /* The actual proposed config in question lives in the xact.  Note
         that xact may be NULL, when we're rolling back; not a
         problem. */
      rw_status_t rs;
      RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = NULL;
      rs = rwdts_member_reg_handle_get_element_keyspec(myapp->conf1.reg_nmap,
                                                       NULL,
                                                       xact,
                                                       &get_keyspec,
                                                       (const ProtobufCMessage**)&nmap);
      EXPECT_EQ(RW_STATUS_SUCCESS, rs);
      if (nmap) {
        TSTPRN("myapp_conf1_appconf_success_apply[%d] found nmap with %lu ports\n", myapp->ti->member_idx, nmap->n_port);
        unsigned int p;
        for (p=0; p<nmap->n_port; p++) {
          TSTPRN("  + port[%d] mac=%02x:%02x:%02x:%02x:%02x:%02x has_numa_socket=%d numa_socket=%d\n",
                  p,
                  nmap->port[p].mac_address[0],
                  nmap->port[p].mac_address[1],
                  nmap->port[p].mac_address[2],
                  nmap->port[p].mac_address[3],
                  nmap->port[p].mac_address[4],
                  nmap->port[p].mac_address[5],
                  nmap->port[p].has_numa_socket,
                  nmap->port[p].numa_socket);

          /* Do something with this numa port */
          if (0) {
            // rwdts_member_xact_add_issue_ks()...?
            rwdts_appconf_xact_add_issue(ac, xact,
                                         RW_STATUS_FAILURE, "Unable to deal with this port for some reason.");
          }
        }

        /* Pretend we are happy; seems to be no test here for unhappy result? */
        //        myapp->s->exit_soon++;

      } else {
	TSTPRN("myapp_conf1_appconf_success_apply[%d] found no nmap(s)\n", myapp->ti->member_idx);
      }
      /* ...and so on for every damned thing in the myapp->conf1.ac appconf grouping */
    }

    break;
    case RWDTS_APPCONF_ACTION_AUDIT:
    break;
    case RWDTS_APPCONF_ACTION_RECOVER:
    break;
  }


  scratch=scratch;
  is_test_done(myapp->s);
}
void myapp_conf2_apply(rwdts_api_t *apih, rwdts_appconf_t *ac,
		       rwdts_xact_t *xact, rwdts_appconf_action_t action,
		       void *ctx, void *scratch_in) 
{
  myappconf_t *myapp = (myappconf_t *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)myapp->s;
//  rw_keyspec_path_t *get_keyspec = NULL;

  s->prepare_cb_called++;
  ASSERT_TRUE(s->prepare_cb_called <= 3);

  if (action == RWDTS_APPCONF_ACTION_INSTALL) {
    if (s->prepare_cb_called == 3) {
       myapp->s->exit_soon = -999;
       dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 3/10);
       rwsched_dispatch_after_f(myapp->s->tenv->t[TASKLET_MEMBER_0].tasklet,
                                when, myapp->s->member[0].rwq,
                                &myapp->s->exitnow, set_exitnow);
      }
    }
    else if (action == RWDTS_APPCONF_ACTION_RECONCILE) {
#if 0
      RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap) *nmap = NULL;
      nmap = (RWPB_T_MSG(TestdtsRwFpath_ConfigNumaMap)*)
        rwdts_member_reg_handle_get_element_keyspec(myapp->conf1.reg_nmap, NULL,xact, NULL, &get_keyspec);
      if (nmap) {
              TSTPRN("myapp_conf22apply[%d] found nmap with %lu ports\n",
                myapp->ti->member_idx, nmap->n_port);
              unsigned int p;
              for (p=0; p<nmap->n_port; p++) {
                 TSTPRN("  + port[%d] mac=%02x:%02x:%02x:%02x:%02x:%02x has_numa_socket=%d numa_socket=%d", p,
                      nmap->port[p].mac_address[0],
                      nmap->port[p].mac_address[1],
                      nmap->port[p].mac_address[2],
                      nmap->port[p].mac_address[3],
                      nmap->port[p].mac_address[4],
                      nmap->port[p].mac_address[5],
                      nmap->port[p].has_numa_socket,
                      nmap->port[p].numa_socket);
              }
      }
      else {
	      TSTPRN("myapp_conf2_apply[%d] found no nmap(s)\n", myapp->ti->member_idx);
      }
#endif
    }
}

static void appconf_member_cache_upd(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  TSTPRN("appconf_member_cache_upd called in member %d\n", myapp->ti->member_idx);

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = myapp_conf1_xact_init;
  cbset.xact_deinit = myapp_conf1_xact_deinit;
  cbset.config_validate = myapp_conf1_validate;
  cbset.config_apply = myapp_conf1_apply;
  cbset.ctx = myapp;

  /* Throwaway, just to exercise the code that handles more than one appconf group in the apih */
  rwdts_appconf_t *junk = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(junk);

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  /* Register config keyspec(s) */
  rw_keyspec_path_t *keyspec = NULL;

  rw_keyspec_path_t *lks = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_create_dup(lks, NULL, &keyspec);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rw_status_t status = rwdts_appconf_register_keyspec(myapp->conf1.ac,
						      keyspec,
						      0, // flags
						      myapp_conf1_prepare_numamap,
						      &myapp->conf1.reg_nmap);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);

  rwdts_appconf_unref(junk);
  rw_keyspec_path_free(keyspec, NULL);
  TSTPRN("Member start_appconf_f ending, startct=%u\n", s->member_startct);
}

static void appconf_member_start_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  ti->cb.apply = 0;
  ti->cb.xact_init = 0;
  ti->cb.validate = 0;
  ti->cb.xact_deinit = 0;
  s->install = 0;
  myapp->member = &s->member[ti->member_idx];

  TSTPRN("appconf_member_start_f called in member %d\n", myapp->ti->member_idx);
  rwdts_member_deregister_all(ti->apih);

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = myapp_conf1_xact_init;
  cbset.xact_deinit = myapp_conf1_xact_deinit;
  cbset.config_validate = myapp_conf1_validate;
  cbset.config_apply = myapp_conf1_appconf_success_apply;
  cbset.ctx = myapp;

  /* Throwaway, just to exercise the code that handles more than one appconf group in the apih */
  rwdts_appconf_t *junk = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(junk);

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  /* Register config keyspec(s) */
  rw_keyspec_path_t *keyspec = NULL;
  rw_keyspec_path_t *keyspec_test = NULL;

  rw_keyspec_path_t *lks = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_t *lks_test = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMapTest)));
  rw_keyspec_path_create_dup(lks, NULL, &keyspec);
  rw_keyspec_path_create_dup(lks_test, NULL, &keyspec_test);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(keyspec_test, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  uint32_t flags = ( RWDTS_FLAG_SUBSCRIBER | 0 );
  myapp->conf1.reg_nmap = rwdts_appconf_register(myapp->conf1.ac, 
                                                 keyspec,
                                                 NULL, /*shard*/
                                                 RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
                                                 flags,
                                                 myapp_conf1_prepare_numamap);

  /* Secondary registration for nmap_test, this one returns NA */
  myapp->conf1.reg_nmap_test_na = rwdts_appconf_register(myapp->conf1.ac, 
                                                         keyspec,
                                                         NULL, /*shard*/
                                                         RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
                                                         flags,
                                                         myapp_conf1_prepare_numamap_na);

  myapp->conf1.reg_nmap_test = rwdts_appconf_register(myapp->conf1.ac, 
                                                      keyspec_test,
                                                      NULL, /*shard*/
                                                      RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMapTest),
                                                      flags,
                                                      myapp_conf1_prepare_numamap);

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);

  rwdts_appconf_unref(junk);
  rw_keyspec_path_free(keyspec, NULL);
  rw_keyspec_path_free(keyspec_test, NULL);

  /* Progress the ensemble stuff for dts gtest */
  ck_pr_inc_32(&s->member_startct);
  TSTPRN("Member start_appconf_f ending, startct=%u\n", s->member_startct);
}


static void appconf_member_start_after_client_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  TSTPRN("appconf_member_start_after_client_f called in member %d\n", myapp->ti->member_idx);

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = myapp_conf1_xact_init;
  cbset.xact_deinit = myapp_conf1_xact_deinit;
  cbset.config_validate = myapp_conf1_validate;
  cbset.config_apply = myapp_conf2_apply;
  cbset.ctx = myapp;

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  /* Register config keyspec(s) */
  rw_keyspec_path_t *keyspec = NULL;
  rw_keyspec_path_t *keyspec_test = NULL;

  rw_keyspec_path_t *lks = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_t *lks_test = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMapTest)));
  rw_keyspec_path_create_dup(lks, NULL, &keyspec);
  rw_keyspec_path_create_dup(lks_test, NULL, &keyspec_test);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(keyspec_test, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  uint32_t flags = ( RWDTS_FLAG_SUBSCRIBER | 0 );
  myapp->conf1.reg_nmap = rwdts_appconf_register(myapp->conf1.ac, 
						 keyspec,
						 NULL, /*shard*/
						 RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
						 flags,
						 myapp_conf1_prepare_numamap);

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);

   rw_keyspec_path_free(keyspec, NULL);
   rw_keyspec_path_free(keyspec_test, NULL);

  /* Progress the ensemble stuff for dts gtest */
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Member appconf_member_start_after_client_f ending, startct=%u\n", s->member_startct);
}

static void appconf_member_start_delay_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  ti->cb.apply = 0;
  ti->cb.xact_init= 0;
  ti->cb.validate = 0;
  ti->cb.xact_deinit = 0;
  ti->cb.prepare = 0;
  myapp->member = &s->member[ti->member_idx];

  TSTPRN("appconf_member_start_f called in member %d\n", myapp->ti->member_idx);

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = myapp_conf1_xact_init;
  cbset.xact_deinit = myapp_conf1_xact_deinit;
  cbset.config_validate = myapp_conf1_validate;
  cbset.config_apply = myapp_conf1_apply;
  cbset.ctx = myapp;

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  /* Register config keyspec(s) */
  rw_keyspec_path_t *keyspec = NULL;
  rw_keyspec_path_t *keyspec_test = NULL;

  rw_keyspec_path_t *lks = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_t *lks_test = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMapTest)));
  rw_keyspec_path_create_dup(lks, NULL, &keyspec);
  rw_keyspec_path_create_dup(lks_test, NULL, &keyspec_test);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(keyspec_test, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  uint32_t flags = ( RWDTS_FLAG_SUBSCRIBER | 0 );
  myapp->conf1.reg_nmap = rwdts_appconf_register(myapp->conf1.ac, 
						 keyspec,
						 NULL, /*shard*/
						 RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
						 flags,
						 myapp_conf1_prepare_numamap_delay);

  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);

  rw_keyspec_path_free(keyspec, NULL);
  rw_keyspec_path_free(keyspec_test, NULL);
  TSTPRN("Member start_appconf_f ending, startct=%u\n", s->member_startct);
}

static void appconf_member_prepare_nok_f(void *ctx) 
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  RW_ASSERT(ti->tasklet_idx >= TASKLET_MEMBER_0);
  RW_ASSERT(ti->tasklet_idx <= TASKLET_MEMBER_2);

  myappconf_t *myapp = &s->member[ti->member_idx].appconf;
  memset(myapp, 0, sizeof(*myapp));
  myapp->s = s;
  myapp->ti = ti;
  myapp->member = &s->member[ti->member_idx];

  /* Create appconf group */
  rwdts_appconf_cbset_t cbset = { };
  cbset.xact_init = myapp_conf1_xact_init;
  cbset.xact_deinit = myapp_conf1_xact_deinit;
  cbset.config_validate = myapp_conf1_validate;
  cbset.config_apply = myapp_conf1_apply;
  cbset.ctx = myapp;

  myapp->conf1.ac = rwdts_appconf_group_create(ti->apih, NULL, &cbset);
  RW_ASSERT(myapp->conf1.ac);

  /* Register config keyspec(s) */
  rw_keyspec_path_t *keyspec = NULL;
  rw_keyspec_path_t *keyspec_test = NULL;

  rw_keyspec_path_t *lks = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMap)));
  rw_keyspec_path_t *lks_test = (rw_keyspec_path_t*)(& (*RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Node_NumaMapTest)));
  rw_keyspec_path_create_dup(lks, NULL, &keyspec);
  rw_keyspec_path_create_dup(lks_test, NULL, &keyspec_test);
  rw_keyspec_path_set_category(keyspec, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  rw_keyspec_path_set_category(keyspec_test, NULL, RW_SCHEMA_CATEGORY_CONFIG);
  uint32_t flags = ( RWDTS_FLAG_SUBSCRIBER | 0 );
  myapp->conf1.reg_nmap = rwdts_appconf_register(myapp->conf1.ac, 
						 keyspec,
						 NULL, /*shard*/
						 RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
						 flags,
						 myapp_conf1_prepare_numamap_nok);

  /* Secondary registration for nmap_test, this one returns NA */
  myapp->conf1.reg_nmap_test_na = rwdts_appconf_register(myapp->conf1.ac,
                                                         keyspec,
                                                         NULL, /*shard*/
                                                         RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMap),
                                                         flags,
                                                         myapp_conf1_prepare_numamap_na);

  myapp->conf1.reg_nmap_test = rwdts_appconf_register(myapp->conf1.ac,
                                                      keyspec_test,
                                                      NULL, /*shard*/
                                                      RWPB_G_MSG_PBCMD(TestdtsRwFpath_ConfigNumaMapTest),
                                                      flags,
                                                      myapp_conf1_prepare_numamap);
  /* Say we're finished */
  rwdts_appconf_phase_complete(myapp->conf1.ac, RWDTS_APPCONF_PHASE_REGISTER);


  rw_keyspec_path_free(keyspec, NULL);
  rw_keyspec_path_free(keyspec_test, NULL);
  TSTPRN("Member start_appconf_f ending, startct=%u\n", s->member_startct);
}
static void appconf_test_execute_f(struct queryapi_state *s, dispatch_function_t f) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq, &s->tenv->t[TASKLET_MEMBER_0 + i], f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

static void appconf_test_execute_publish_f(struct queryapi_state *s) 
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
 
  rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,s->client.rwq,&s->tenv->t[TASKLET_CLIENT],appconf_client_start_f); 


  dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_after_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, 
                        when,
                        s->member[i].rwq, 
                        &s->tenv->t[TASKLET_MEMBER_0 + i], 
                        appconf_member_start_after_client_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?90:30;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}


/*
 * AppConf testcases
 */

// FIXME
TEST_F(RWDtsEnsemble, AppConf_Success_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  s->exit_soon = 6;
  appconf_test_execute_f(s, appconf_member_start_f);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, AppConf_SubAfterPublish_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  appconf_test_execute_publish_f(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

/* Seems to have an unexpected prepare coutner at the end.  Some of
   those checks are using the non-LONG name for some *other* tests, so
   I don't know how the heck all this can be correct. */
TEST_F(RWDtsEnsemble, AppConf_Delay_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  appconf_test_execute_f(s, appconf_member_start_delay_f);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

// FIXME
TEST_F(RWDtsEnsemble, AppConf_PrepNotOk_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  appconf_test_execute_f(s, appconf_member_prepare_nok_f);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

/* Xacts never finish?  Test exits, the 2s timer passes, and s is invalid.  ??? */
TEST_F(RWDtsEnsemble, AppConf_Cache_Success_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  appconf_test_execute_f(s, appconf_member_cache_upd);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DISABLED_AppConf_Cache_Update_Delay_LONG) 
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->member[0].treatment = TREATMENT_ABORT_NACK;
  appconf_test_execute_f(s, appconf_member_cache_upd);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DataMemberAPISubBeforePub)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->member[0].treatment = TREATMENT_ACK;
  s->member[1].treatment = TREATMENT_ACK;
  s->member[2].treatment = TREATMENT_ACK;
  s->pub_data_check_success = 0; 
  s->expected_pub_data_check_success = 3; 
  s->expected_notification_count = 3;
  s->notification_count = 0;
  s->client_dreg_done = 0;
  memberapi_test_sub_before_pub(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DataMemberAPISubAfterPub)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->member[0].treatment = TREATMENT_ACK;
  s->member[1].treatment = TREATMENT_ACK;
  s->member[2].treatment = TREATMENT_ACK;
  s->client.expect_result = XACT_RESULT_COMMIT;
  s->pub_data_check_success = 0;
  s->expected_pub_data_check_success = 3;
  s->expected_notification_count = 3;
  s->notification_count = 0;
  s->client_dreg_done = 0;
  memberapi_test_sub_after_pub(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, DataMemberAPIUpdateMerge_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s->member[0].treatment = TREATMENT_ACK;
  s->member[1].treatment = TREATMENT_ACK;
  s->member[2].treatment = TREATMENT_ACK;
  s->expected_notification_count = 12;
  s->expected_advise_rsp_count = 4;
  memberapi_test_update_merge(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

TEST_F(RWDtsEnsemble, MemberAPICreateData_LONG)
{
  struct queryapi_state *s = (struct queryapi_state *)calloc(1, sizeof(struct queryapi_state));
  init_query_api_state(s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  memberapi_create_test_execute(s);
  memset(s, 0xaa, sizeof(*s));
  free(s);
}

// FIXME
TEST_F(RWDtsEnsemble, MemberAPIDelData_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.expected_notification_count = s.expected_advise_rsp_count = 6;
  memberapi_del_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIDelDeepData_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.expected_notification_count = s.expected_advise_rsp_count = 6;
  memberapi_del_deep_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIDelwildcard1Data_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.del_tc = DEL_WC_1;
  memberapi_del_wc1_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIDelwildcard2Data_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.del_tc = DEL_WC_2;
  memberapi_del_wc2_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPIPruning_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  s.client.expect_result = XACT_RESULT_COMMIT;
  memberapi_pruning_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MultipleRegistrations_LONG)
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_CREATE, FALSE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  s.expected_notification_count = 12;
  s.expected_advise_rsp_count = 4;
  s.notification_count = 0;
  memberapi_test_multiple_registrations(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, DataMemberCursor_LONG)
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  memberapi_test_data_member_cursor(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RerootList)
{
  struct queryapi_state s = { 0 };
  init_query_api_state(&s, RWDTS_QUERY_CREATE, TRUE, FALSE);
  s.member[0].treatment = TREATMENT_ACK;
  s.member[1].treatment = TREATMENT_ACK;
  s.member[2].treatment = TREATMENT_ACK;
  s.client.expect_result = XACT_RESULT_COMMIT;
  memberapi_test_reroot_list(&s);
  memset(&s, 0xaa, sizeof(s));
}

// FIXME
TEST_F(RWDtsEnsemble, MemberAPIXpathQuery_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.client.expect_result = XACT_RESULT_COMMIT;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberGIAPI_LONG) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.client.expect_result = XACT_RESULT_COMMIT;
  member_gi_api_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RegReady) 
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  member_reg_ready_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, TestNotif)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.expected_notif_rsp_count = 1;
  member_test_notif_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

// FIXME

TEST_F(RWDtsEnsemble, AdviseSolicitTest_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  member_advise_solicit_test(&s);
  memset(&s, 0xaa, sizeof(s));
}


TEST_F(RWDtsEnsemble, AppDataUnsafekeyspec_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  member_reg_appdata_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataUnsafepathentry_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  member_reg_appdata_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataUnsafeminikey_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  member_reg_appdata_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafekeyspec_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  member_reg_appdata_safe_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafepathentry_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  member_reg_appdata_safe_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafeminikey_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  member_reg_appdata_safe_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueuekeyspec_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  member_reg_appdata_queue_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueuepathentry_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  member_reg_appdata_queue_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueueminikey_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  member_reg_appdata_queue_test_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafeKeygetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  s.appdata_test = APPDATA_SAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafePegetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  s.appdata_test = APPDATA_SAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDatasafeMKgetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  s.appdata_test = APPDATA_SAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataUnsafeKeygetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  s.appdata_test = APPDATA_UNSAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataUnsafePegetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  s.appdata_test = APPDATA_UNSAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataUnsafeMKgetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  s.appdata_test = APPDATA_UNSAFE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueueKeygetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_KS;
  s.appdata_test = APPDATA_QUEUE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueuePegetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_PE;
  s.appdata_test = APPDATA_QUEUE;
  appdata_test_getnext_execute(&s);
}

TEST_F(RWDtsEnsemble, AppDataQueuegetnext_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, TRUE);
  s.appdata = APPDATA_MK;
  s.appdata_test = APPDATA_QUEUE;
  appdata_test_getnext_execute(&s);
}

/*
 * Members use SUBOBJ flag for registration
 */

TEST_F(RWDtsEnsemble, MemberAPIObj)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_obj_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPISubObj)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_subobj_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, MemberAPINoSubObj)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_UPDATE, FALSE, FALSE);
  memberapi_nosubobj_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

static rw_status_t
    rwdts_xp_run_testcase(rwdts_api_t *apih, const char* xpath, bool fail, bool eval)
{
  RW_ASSERT(xpath);
  TSTPRN("XPath: %s\n\n", xpath);
  rwdts_xpath_pcb_t *rwpcb = NULL;
  rw_status_t rs = rwdts_xpath_parse(xpath, RW_XPATH_KEYSPEC, NULL,
                                     apih, &rwpcb);
  if (fail) {
    EXPECT_EQ(rs, RW_STATUS_FAILURE);
    EXPECT_TRUE(rwpcb == NULL);
  }
  else {
    EXPECT_EQ(rs, RW_STATUS_SUCCESS);
    if (rs == RW_STATUS_SUCCESS) {
      EXPECT_FALSE(rwpcb == NULL);
      EXPECT_FALSE(rwpcb->lib_inst == NULL);
      EXPECT_FALSE(rwpcb->pcb == NULL);
      EXPECT_FALSE(rwpcb->expr == NULL);
      char *tk = rwdts_xpath_dump_tokens(rwpcb);
      TSTPRN("Tokens:\n%s\n", tk);
      RW_FREE(tk);
      tk = NULL;
    }
  }
  if (eval && (RW_STATUS_SUCCESS == rs)) {
    rs = rwdts_xpath_eval(rwpcb);
    if (!fail) {
      EXPECT_NE(rs, RW_STATUS_FAILURE);
      EXPECT_FALSE(rwpcb->results == NULL);
      char *res = rwdts_xpath_dump_results(rwpcb);
      TSTPRN("Results:\n%s\n", res);
      RW_FREE(res);
      res = NULL;
    }
    if (rwpcb->errmsg[0]) {
      TSTPRN("\nERROR in eval: %s\n", rwpcb->errmsg);
    }
  }

  rwdts_xpath_pcb_free(rwpcb);
  rwpcb = NULL;

  rw_keyspec_path_t *keyspec = NULL;
  keyspec = rw_keyspec_path_from_xpath(rwdts_api_get_ypbc_schema(apih),
                                       (char*)xpath,
                                       RW_XPATH_KEYSPEC,
                                       &apih->ksi);
  if (keyspec) {
    TSTPRN("\nKS from xpath using XPathParserII: %s\n", xpath);
    rw_keyspec_path_free(keyspec, NULL);
  }
  return rs;
}

TEST(RWDtsRedn, Parsing)
{
  TEST_DESCRIPTION("Tests DTS XPath Parsing");

  rwsched_instance_ptr_t rwsched = NULL;
  rwsched = rwsched_instance_new();
  ASSERT_TRUE(rwsched);

  rwsched_tasklet_ptr_t tasklet = NULL;
  tasklet = rwsched_tasklet_new(rwsched);
  ASSERT_TRUE(tasklet);

  rwmsg_endpoint_t *ep = NULL;
  ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  ASSERT_TRUE(ep);

  rwdts_api_t *apih = NULL;
  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, ep, "/L/API/XPATH/2", RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
  ASSERT_NE(apih, (void*)NULL);

  rwdts_api_set_ypbc_schema(apih, (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));
  EXPECT_EQ((void*)RWPB_G_SCHEMA_YPBCSD(DtsTest), (void*)rwdts_api_get_ypbc_schema(apih));

  rw_status_t rs = RW_STATUS_SUCCESS;
  rs = rwdts_xp_run_testcase(apih, "1+2", false, true);
  rs = rwdts_xp_run_testcase(apih, "/ps:company/ps:persons/ps:phones", false, true);
  rs = rwdts_xp_run_testcase(apih, "/ps:company/ps:persons[ps:id = '999']", false, true);
  rs = rwdts_xp_run_testcase(apih, "/ps:company/ps:persons[ps:id = '999']/ps:phones[ps:number = '123-456-789']/ps:type", false, true);
  rs = rwdts_xp_run_testcase(apih, "sum(/ps:company/ps:persons/ps:phones/ps:calls)", false, true);
  rs = rwdts_xp_run_testcase(apih, "rift-min(/ps:company/ps:persons/ps:id)", false, true);
  rs = rwdts_xp_run_testcase(apih, "rift-max(/ps:company/ps:persons/ps:phones/ps:calls)", false, true);
  rs = rwdts_xp_run_testcase(apih, "rift-avg(/ps:company/ps:persons/ps:phones/ps:calls)", false, true);
  rs = rwdts_xp_run_testcase(apih, "count(/ps:company/ps:persons/ps:phones/ps:type)", false, true);
  // ATTN: Evaluation of the below expressionsnot yet supported
  rs = rwdts_xp_run_testcase(apih, "/ps:company/ps:persons[ps:id = rift-max(/ps:company/ps:persons/ps:id)]", false, true);
  rs = rwdts_xp_run_testcase(apih, "sum(/ps:company/ps:persons[ps:id = '1111']/ps:phones/ps:calls)"
                             "+ /ps:company/ps:persons[ps:id = '1111']/ps:emergency/ps:calls", false, true);
  rs = rwdts_xp_run_testcase(apih, "sum(/ps:company/ps:persons[ps:id = '2222']/ps:phones/ps:calls "
                             "| /ps:company/ps:persons[ps:id = '2222']/ps:emergency/ps:calls)", false, false);

  // Invalid XPath
  fprintf(stderr, "\nError messages expected here:\n");
  rs = rwdts_xp_run_testcase(apih, "/ps:company/ps:persons/[ps:name = 'Test1']", true, false);
  fprintf(stderr,"End of error messages.\n");

  // TODO: Add more parsing expressions

#if 0
  // Check timing for parsing
  int32_t i = 0;
  int32_t max_count = 100;
  const char *xpath = (char*)"/ps:company/ps:persons[ps:id = '1111']/ps:phones[ps:number = '123-456-789']";
  clock_t start_time = clock();
  for (; i<max_count; i++) {
    rwdts_xpath_pcb_t *rwpcb = NULL;
    rs = rwdts_xpath_parse(xpath, RW_XPATH_KEYSPEC, NULL,
                           apih, &rwpcb);
    if (rs == RW_STATUS_SUCCESS) {
      rs = rwdts_xpath_eval(rwpcb);
      if (RW_STATUS_PENDING == rs) {
        rw_keyspec_path_t *ks = rwdts_xpath_get_single_query_ks(rwpcb);
        if (ks)
          rw_keyspec_path_free(ks, NULL);
      }
    }
    rwdts_xpath_pcb_free(rwpcb);
  }
  clock_t end_time = clock();
  std::cout << "NCX time:" << (end_time - start_time) <<
      " (" << ((float)(end_time - start_time))/CLOCKS_PER_SEC << "s)" << std::endl;

  start_time = clock();
  for (i=0; i<max_count; i++) {
    rw_keyspec_path_t *keyspec = NULL;
    keyspec = rw_keyspec_path_from_xpath(rwdts_api_get_ypbc_schema(apih),
                                         (char*)xpath,
                                         RW_XPATH_KEYSPEC,
                                         &apih->ksi);
    if (keyspec)
      rw_keyspec_path_free(keyspec, NULL);

  }
  end_time = clock();
  std::cout << "Custom time:" << (end_time - start_time) <<
      " (" << ((float)(end_time - start_time))/CLOCKS_PER_SEC << "s)" << std::endl;
#endif

  /* Ruh-roh, api_init() now fires member registration, which fires off a dispatch_after_f, which we can't easily wait */
  rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  rwsched_instance_free(rwsched);
  rwsched = NULL;
}

static void rwdts_redn_member_fill_response(RWPB_T_MSG(DtsTest_Company) *company, int count, bool getnext=false) {
  RW_ASSERT(company);
  RWPB_F_MSG_INIT(DtsTest_Company, company);
  RW_ASSERT(count > 0 && count <= 30);
  company->persons = (RWPB_T_MSG(DtsTest_Persons)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Persons)*) * count);
  RW_ASSERT(company->persons);
  int i = 0;
  for(; i<count; i++ ) {
    RWPB_T_MSG(DtsTest_Persons)* persons = (RWPB_T_MSG(DtsTest_Persons)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Persons)));
    RWPB_F_MSG_INIT(DtsTest_Persons, persons);
    persons->name = strdup("0-RspTestName");
    persons->name[0] = '0' + i;
    persons->id = 2000 + i;
    persons->has_id = 1;
    persons->email = strdup("0-rsp_test@test.com");
    persons->email[0] = '0' + i;
    persons->has_employed = TRUE;
    persons->employed = TRUE;
    persons->n_phones = 4;
    persons->phones = (RWPB_T_MSG(DtsTest_Phones)**)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Phones)*) * persons->n_phones);
    uint j=0;
    for(; j<persons->n_phones; j++) {
      persons->phones[j] = (RWPB_T_MSG(DtsTest_Phones)*)RW_MALLOC0(sizeof(RWPB_T_MSG(DtsTest_Phones)));
      RWPB_F_MSG_INIT(DtsTest_Phones,persons->phones[j]);
      if ((i==1) && (j==0)) {
        asprintf(&persons->phones[j]->number, "%s", "1234567890");
      }
      else {
        asprintf(&persons->phones[j]->number, "%10d", rand());
      }
      persons->phones[j]->has_type = TRUE;
      persons->phones[j]->type = (DtsTest__YangEnum__PhoneType__E)(j%3);
      // Do not fill the calls for the last phone
      if (j != (persons->n_phones-1)) {
        persons->phones[j]->calls = rand();
        persons->phones[j]->has_calls = TRUE;
      }
    }
    company->persons[company->n_persons++] = persons;
  }
  return;
}

static void rwdts_clnt_redn_query_callback(rwdts_xact_t *xact, rwdts_xact_status_t* xact_status, void *ud)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  RW_ASSERT(s);

  TSTPRN("Calling rwdts_clnt_redn_query_callback with status = %d\n", xact_status->status);

  /* It's actually rather awkward to get hold of the actual execution status of the xact? */
  {
    switch (s->client.expect_result) {
    case XACT_RESULT_COMMIT:
      if (s->client.action == RWDTS_QUERY_READ) {
        uint32_t cnt;
        uint32_t expected_count = 0;

        UNUSED(cnt);
        for (int r = 0; r < TASKLET_MEMBER_CT; r++) {
          if (s->member[r].treatment == TREATMENT_ACK) {
            // When not Async we respond with 1 record
            expected_count += 1;
          } else if (s->member[r].treatment == TREATMENT_ASYNC)  {
            // When it is Async we respond with 10 records
            expected_count += 10;
          }
        }

        cnt = rwdts_xact_get_available_results_count(xact, NULL,0);
        if (s->client.expect_value) {
          EXPECT_NE(cnt, 0);
        }

        cnt = rwdts_xact_get_available_results_count(xact, NULL,0);
        if (s->client.expect_value) {
          EXPECT_NE(cnt, 0);
        }

        rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);
        while (qrslt) {
          const rw_keyspec_path_t *ks = rwdts_query_result_get_keyspec(qrslt);
          ASSERT_TRUE(ks);
          const char* xpath = rwdts_query_result_get_xpath(qrslt);
          EXPECT_TRUE(xpath);
          if (s->client.expect_value) {
            ProtobufCType type;
            void *value = NULL;
            rw_status_t rs = rwdts_query_result_get_value(qrslt, &type, &value);
            EXPECT_EQ(rs, RW_STATUS_SUCCESS);
            if (RW_STATUS_SUCCESS == rs) {
              RW_ASSERT(value);
              EXPECT_EQ(type, s->client.val_type);
              switch (type) {
                case PROTOBUF_C_TYPE_INT32:
                case PROTOBUF_C_TYPE_SINT32:
                case PROTOBUF_C_TYPE_SFIXED32:
                case PROTOBUF_C_TYPE_INT64:
                case PROTOBUF_C_TYPE_SINT64:
                case PROTOBUF_C_TYPE_SFIXED64:
                  TSTPRN("Value(int): %li\n", *(int64_t*)value);
                  break;
                case PROTOBUF_C_TYPE_UINT32:
                case PROTOBUF_C_TYPE_FIXED32:
                case PROTOBUF_C_TYPE_UINT64:
                case PROTOBUF_C_TYPE_FIXED64:
                  TSTPRN("Value(uint): %lu\n", *(uint64_t*)value);
                  break;
                case PROTOBUF_C_TYPE_FLOAT:
                case PROTOBUF_C_TYPE_DOUBLE:
                  TSTPRN("Value(double): %lf\n", *(double*)value);
                  break;
                default:
                  TSTPRN("Value: ~UNKNOWN~\n");
              }
              RW_FREE(value);
            }
          }
          qrslt = rwdts_xact_query_result(xact, 0);
        }
      }
      break;
    case XACT_RESULT_ABORT:
      // ASSERT_EQ(xact_status->status, RWDTS_XACT_ABORTED); // this is only so while we have a monolithic-responding router
      /* We don't much care about results from abort although there may be some */
      break;
    case XACT_RESULT_ERROR:
      // ASSERT_EQ(xact_status->status, RWDTS_XACT_FAILURE);
      /* We don't much care about results from a failure although there may be some */
      break;
    default:
      ASSERT_TRUE((int)0);
      break;
    }
  }

  // Call the transaction end
  s->exitnow = 1;

  return;
}


static void memberapi_redn_client_start_f(void *ctx)
{

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  TSTPRN("MemberAPI reduction test client start...\n");

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Company_Persons));
  UNUSED(keyspec);

  rwdts_xact_t* xact = NULL;
  rwdts_api_t *clnt_apih = s->tenv->t[TASKLET_CLIENT].apih;

  ASSERT_TRUE(clnt_apih);

  RWDtsClientset csreq;
  rwdts_clientset__init(&csreq);
  const char *set[4] = { "/L/RWDTS_GTEST/MEMBER/0", "/L/RWDTS_GTEST/MEMBER/1", "/L/RWDTS_GTEST/MEMBER/2", "/L/RWDTS_GTEST/NONMEMBER/0" };
  csreq.n_member = 3;
  if (s->include_nonmember) {
    csreq.n_member = 4;		// include the one on the end that will bounce
  }
  csreq.member = (char **)&set;

  rwdts_event_cb_t clnt_cb = {};
  clnt_cb.cb = rwdts_clnt_redn_query_callback;
  clnt_cb.ud = ctx;
  uint32_t flags = 0;

  ASSERT_EQ(rwdts_api_get_ypbc_schema(clnt_apih), (rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(DtsTest));

  ASSERT_TRUE(s->client.xpath);
  xact = rwdts_api_query(clnt_apih, s->client.xpath, RWDTS_QUERY_READ,
                         flags|RWDTS_XACT_FLAG_TRACE, clnt_cb.cb, clnt_cb.ud, NULL);

  if (s->client.noreqs) {
    ASSERT_FALSE(xact);
    s->exitnow = 1;
  }
  else {
    ASSERT_TRUE(xact);
    char *xact_id_str;
    xact_id_str = rwdts_xact_get_id(xact);
    ASSERT_TRUE(xact_id_str);
    RW_FREE(xact_id_str);

    if (s->client.client_abort)
    {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, 2*NSEC_PER_SEC);
      rwsched_dispatch_after_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                               when,
                               rwsched_dispatch_get_global_queue(s->tenv->t[TASKLET_CLIENT].tasklet, DISPATCH_QUEUE_PRIORITY_DEFAULT),
                               xact,
                               rwdts_gtest_xact_abort);
    }
  }
  return;
}


static void
memberapi_redn_member_reg_ready(rwdts_member_reg_handle_t  regh,
                                rw_status_t                rs,
                                void*                      user_data)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)user_data;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  TSTPRN("memberapi_redn_member_reg_ready() called\n");
  ck_pr_inc_32(&s->reg_ready_called);

  EXPECT_EQ(RW_STATUS_SUCCESS, rs);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t*)regh;
  TSTPRN("Registered %s\n", reg->keystr);

  if (s->reg_ready_called == TASKLET_MEMBER_CT) {
    /* Start client after all members are going */
    TSTPRN("Member start_f invoking client start_f\n");
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_CLIENT].tasklet,
                             s->client.rwq,
                             &s->tenv->t[TASKLET_CLIENT],
                             memberapi_redn_client_start_f);
  }
  return;
}

void
memberapi_redn_test_dispatch_delay_response(memberapi_test_t* mbr)
{
  RW_ASSERT(mbr);
  rwdts_xact_t *xact = mbr->xact;
  RW_ASSERT(xact);
  rwdts_query_handle_t qhdl = mbr->qhdl;

  RWPB_T_MSG(DtsTest_Company) *company;
  static RWPB_T_MSG(DtsTest_Company) company_instance = {};
  static ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;
  company = &company_instance;

  TSTPRN("Calling DTS redn Member delay response ...\n");

  RW_ZERO_VARIABLE(&rsp);

  rsp.ks = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Company));
  RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp.ks));

  array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
  array[0] = &company->base;

  rwdts_redn_member_fill_response(company, 2);

  rsp.n_msgs = 1;
  rsp.msgs = (ProtobufCMessage**)array;
  rsp.evtrsp  = RWDTS_EVTRSP_ACK;

  rw_status_t rs = rwdts_member_send_response(xact, qhdl, &rsp);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  RW_FREE(array);
  RW_FREE(mbr);
}


static rwdts_member_rsp_code_t
memberapi_redn_member_prepare(const rwdts_xact_info_t* xact_info,
                              RWDtsQueryAction         action,
                              const rw_keyspec_path_t*      key,
                              const ProtobufCMessage*  msg,
                              void *getnext_ptr)
{
  RW_ASSERT(xact_info);
  rwdts_xact_t *xact = xact_info->xact;
  RW_ASSERT_TYPE(xact, rwdts_xact_t);

  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)xact_info->ud;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s);
  RW_ASSERT(s->testno == s->tenv->testno);
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);

  rwdts_api_t *apih;
  apih = xact->apih;
  RW_ASSERT(apih);

  RWPB_T_MSG(DtsTest_Company) *company;
  static RWPB_T_MSG(DtsTest_Company) company_instance = {};
  static ProtobufCMessage **array;
  rwdts_member_query_rsp_t rsp;
  company = &company_instance;

  TSTPRN("Calling DTS redn Member prepare ...%d %d\n", ti->member_idx, s->member[ti->member_idx].treatment);
  char *str = (char*)RW_MALLOC0(1024);
  if (str) {
    rw_keyspec_path_get_print_buffer(key, &xact->ksi,
                                     rwdts_api_get_ypbc_schema(apih), str, 1023);
    TSTPRN("Key for prepare is %s\n", str);
    RW_FREE(str);
  }

  bool rtr_connected = (rwdts_get_router_conn_state(apih) == RWDTS_RTR_STATE_UP)?true:false;
  RW_ASSERT(rtr_connected);

  RW_ZERO_VARIABLE(&rsp);

  switch (s->member[ti->member_idx].treatment) {
    uint32_t flags;
    UNUSED(flags);

  case TREATMENT_ACK:
  case TREATMENT_NACK:
  case TREATMENT_COMMIT_NACK:
  case TREATMENT_ABORT_NACK:
  case TREATMENT_PRECOMMIT_NACK:
  case TREATMENT_PRECOMMIT_NULL:

    rsp.ks = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Company));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(rsp.ks));

    array = (ProtobufCMessage**)RW_MALLOC0(sizeof(ProtobufCMessage*));
    array[0] = &company->base;

    rwdts_redn_member_fill_response(company, 2);

    rsp.n_msgs = 1;
    rsp.msgs = (ProtobufCMessage**)array;
    if (s->member[ti->member_idx].treatment == TREATMENT_NACK) {
      rsp.evtrsp  = RWDTS_EVTRSP_NACK;
    }
    else {
      rsp.evtrsp  = RWDTS_EVTRSP_ACK;
    }

    if (s->ignore == 0) {
      // EXPECT_EQ(action, s->client.action);
    }
    flags = rwdts_member_get_query_flags(xact_info->queryh);
    //  rwdts_xact_info_respond_xpath(xact_info,dts_rsp_code,xpath,array[0]);

    s->num_responses++;
    rwdts_member_send_response(xact, xact_info->queryh, &rsp);
    protobuf_c_message_free_unpacked_usebody(NULL, &company->base);
    RW_FREE(array);
    return RWDTS_ACTION_OK;

  case TREATMENT_NA:
    return RWDTS_ACTION_NA;

  case TREATMENT_ASYNC:
  case TREATMENT_ASYNC_NACK: {
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    if (s->member[ti->member_idx].treatment == TREATMENT_ASYNC_NACK) {
      mbr->send_nack = TRUE;
    }
    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->inst = ti->member_idx;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_test_dispatch_response);
    TSTPRN("Member %d returning ASYNC from prepare\n", ti->member_idx);
    return RWDTS_ACTION_ASYNC;
  }
  case TREATMENT_BNC:
    rsp.evtrsp  = RWDTS_EVTRSP_ASYNC;
    return RWDTS_ACTION_OK;

  case TREATMENT_PREPARE_DELAY: {
    uint32_t delay_sec;
    memberapi_test_t *mbr = (memberapi_test_t*)RW_MALLOC0(sizeof(*mbr));

    mbr->xact = xact;
    mbr->qhdl = xact_info->queryh;
    mbr->key = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Company));
    RW_ASSERT(!rw_keyspec_path_has_wildcards(mbr->key));

    // Dispatch can be called from main q alone???
    if (s->client.client_abort) {
      delay_sec = 5;
    } else {
      delay_sec = 10 * (1 + ti->member_idx);
    }

    TSTPRN("Calling DTS redn Member prepare Delaying for %u secs...\n", delay_sec);

    dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, delay_sec * NSEC_PER_SEC);
    rwsched_dispatch_after_f(apih->tasklet,
                             when,
                             apih->client.rwq,
                             mbr,
                             (dispatch_function_t)memberapi_redn_test_dispatch_delay_response);
    return RWDTS_ACTION_ASYNC;
  }
  default:
    RW_CRASH();
    return RWDTS_ACTION_NA;
    break;
  }
  return RWDTS_ACTION_OK;
}

static void memberapi_redn_member_start_f(void *ctx)
{
  struct tenv1::tenv_info *ti = (struct tenv1::tenv_info *)ctx;
  struct queryapi_state *s = (struct queryapi_state *)ti->ctx;
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);

  rwdts_member_reg_t reg_cb;
  rwdts_api_t *apih = ti->apih;
  ASSERT_NE(apih, (void*)NULL);

  ASSERT_TRUE(TASKLET_IS_MEMBER(ti->tasklet_idx));
  TSTPRN("Member %d API start ...\n", ti->member_idx);

  RW_ZERO_VARIABLE(&reg_cb);
  reg_cb.user_data = ctx;

  if(s->member[ti->member_idx].call_getnext_api)
  {
    reg_cb.prepare = memberapi_test_get_next;
  }
  else
  {
    reg_cb.prepare = memberapi_redn_member_prepare;
  }
  if (s->member[ti->member_idx].treatment != TREATMENT_PRECOMMIT_NULL) {
    reg_cb.precommit = memberapi_test_precommit;
  }
  reg_cb.commit = memberapi_test_commit;
  reg_cb.abort = memberapi_test_abort;
  reg_cb.reg_ready = memberapi_redn_member_reg_ready;

  ASSERT_TRUE(rwdts_api_get_state(apih)>RW_DTS_STATE_NULL);
  apih->print_error_logs = 1;
  rw_status_t ret_stat = rwdts_api_set_state(apih, rwdts_api_get_state(apih));
  ASSERT_TRUE(ret_stat==RW_STATUS_SUCCESS);
  apih->print_error_logs = 0;

  rwdts_group_t *group = NULL;
  rw_status_t rs = rwdts_api_group_create(apih,
                                          memberapi_test_grp_xinit,
                                          memberapi_test_grp_xdeinit,
                                          memberapi_test_grp_event,
                                          ctx,
                                          &group);
  RW_ASSERT(rs == RW_STATUS_SUCCESS)
  ASSERT_TRUE(!!group);

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Company));
  rw_keyspec_path_t* lks= NULL;
  rw_keyspec_path_create_dup(keyspec, NULL, &lks);
  rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwdts_member_deregister_all(apih);

  //  fprintf(stderr, "memberapi_member_start_f called; starting from dts router reg count regid %u\n", (unsigned) s->tenv->t[TASKLET_ROUTER].dts->rtr_reg_id);
  member_add_multi_regs(apih);

  rwdts_member_reg_handle_t regh;
  char xpath[] = "D,/ps:company";
  rs = rwdts_api_member_register_xpath(apih, xpath, NULL,
                                       RWDTS_FLAG_PUBLISHER,
                                       RW_DTS_SHARD_FLAVOR_NULL, NULL,0,-1,0,0,
                                       memberapi_redn_member_reg_ready,
                                       memberapi_redn_member_prepare,
                                       NULL, NULL, NULL,
                                       ctx,
                                       &regh);
  ASSERT_TRUE(regh);

  rw_keyspec_path_free(lks, NULL);
  ck_pr_inc_32(&s->member_startct);

  TSTPRN("Redn Member start_f ending, startct=%u\n", s->member_startct);

}

static void memberapi_redn_test_execute(struct queryapi_state *s)
{
  RW_ASSERT(s->magic == QAPI_STATE_MAGIC);
  RW_ASSERT(s->testno == s->tenv->testno);
  s->tenv->t[TASKLET_CLIENT].ctx = s;
  ASSERT_NE(s->tenv->t[TASKLET_CLIENT].apih, (void*)NULL);
  s->client.rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
  for (int i=0; i < TASKLET_MEMBER_CT; i++) {
    s->member[i].rwq = rwsched_dispatch_get_main_queue(s->tenv->rwsched);
    s->tenv->t[TASKLET_MEMBER_0+i].ctx = s;
    rwsched_dispatch_async_f(s->tenv->t[TASKLET_MEMBER_0 + i].tasklet, s->member[i].rwq,
                             &s->tenv->t[TASKLET_MEMBER_0 + i], memberapi_redn_member_start_f);
  }
  double seconds = (RUNNING_ON_VALGRIND)?240:120;
  rwsched_dispatch_main_until(s->tenv->t[0].tasklet, seconds, &s->exitnow);
  ASSERT_TRUE(s->exitnow);
}

/*
 *  The following are all using Redn queries
 */

/*
 * Single read query - reponded by all the members
 */

// Test if the basic query works
TEST_F(RWDtsEnsemble, RednMemberAPINonTransExisting_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  memberapi_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransConst_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"1+2";
  s.client.noreqs = TRUE;
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimple_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleList_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company/ps:persons/ps:phones";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSinglePred_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company/ps:persons[ps:id = '2000']";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransMultiplePred_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company/ps:persons[ps:id = '2001']/ps:phones[ps:number = '1234567890']";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleLeaf_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company/ps:persons/ps:phones/ps:calls";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransMultiplePredLeaf_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.xpath = (char*)"/ps:company/ps:persons[ps:id = '2001']/ps:phones[ps:number = '1234567890']/ps:calls";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleSum_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_UINT64;
  s.client.xpath = (char*)"sum(/ps:company/ps:persons/ps:phones/ps:calls)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransPredSum_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_UINT64;
  s.client.xpath = (char*)"sum(/ps:company/ps:persons[ps:id = '2001']/ps:phones/ps:calls)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleMin_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_UINT64;
  s.client.xpath = (char*)"rift-min(/ps:company/ps:persons/ps:phones/ps:calls)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleMax_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_SINT32;
  s.client.xpath = (char*)"rift-max(/ps:company/ps:persons/ps:id)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransSimpleAvg_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.no_xml = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_DOUBLE;
  s.client.xpath = (char*)"rift-avg(/ps:company/ps:persons/ps:phones/ps:calls)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransCountNodes_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.no_xml = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_UINT64;
  s.client.xpath = (char*)"count(/ps:company/ps:persons/ps:phones)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednMemberAPINonTransCountLeafs_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.userednapi = TRUE;
  s.client.no_xml = TRUE;
  s.client.expect_value = TRUE;
  s.client.val_type = PROTOBUF_C_TYPE_UINT64;
  s.client.xpath = (char*)"count(/ps:company/ps:persons/ps:phones/ps:calls)";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

TEST_F(RWDtsEnsemble, RednClientAbort_MemberDelay_LONG)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  s.client.client_abort =  TRUE;
  s.client.expect_result = XACT_RESULT_ABORT;
  s.include_nonmember = TRUE;
  s.member[0].treatment = TREATMENT_PREPARE_DELAY;
  s.client.userednapi = TRUE;
  s.client.no_xml = TRUE;
  s.client.xpath = (char*)"/ps:company";
  memberapi_redn_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}


#define RWDTS_TEST_MAX_SERIALS 50

void
rwdts_test_compare_serials(rw_sklist_t *list,
                           uint32_t    n_serials,
                           uint64_t    *serials)
{
  RW_ASSERT(list);
  RW_ASSERT(serials);
  RW_ASSERT(n_serials > 0);

  int i = 0;;
  rwdts_pub_serial_t *ps;

  EXPECT_EQ(RW_SKLIST_LENGTH(list), n_serials);
  RW_SKLIST_FOREACH(ps, rwdts_pub_serial_t, list, element) {
    EXPECT_EQ(serials[i], ps->serial);
    i++;
    ASSERT_LE(i, n_serials);
  }
  return;
}
static void
rwdts_test_commit_serial_list(void) 
{
  rw_sklist_t list;
  rwdts_pub_serial_t serial[RWDTS_TEST_MAX_SERIALS+4];
  rwdts_pub_serial_t s;
  uint64_t high_commit_serial = 0;
  int i;

  uint64_t result1[] = {3,5,6,7,9,10,11,13,14,15,17,18,19,21,22,23,25,26,27,29,30,31,33,34,35,37,38,39,41,42,43,45,46,47,49,50};
  uint64_t result2[] = {3,5,6,7,9,10,11,13,14,15,17,18,19,21,22,23,25,26,27,29,30,31,33,34,35,37,38,39,41,42,43,45,46,47,49,50,52,53,54,55};
  uint64_t result3[]  = {3,5,6,7,8,9,10,11,13,14,15,16,17,18,19,21,22,23,24,25,26,27,29,30,31,32,33,34,35,37,38,39,40,41,42,43,45,46,47,48,49,50,52,53,54,55};
  uint64_t result4[] = {50,52,53,54,55};

  uint32_t n_result1, n_result2, n_result3, n_result4;

  rwdts_init_commit_serial_list(&list);

  for (i = 0; i < RWDTS_TEST_MAX_SERIALS; i++) {
    memset(&serial[i], 0, sizeof(serial[i]));
    serial[i].id.member_id = 1;
    serial[i].id.router_id = 2;
    serial[i].id.pub_id    = 3;
    serial[i].serial = i+1;
    
    if ((i +1) % 4 == 0 ) {
      continue;
    }
    rwdts_add_entry_to_commit_serial_list(&list,
                                          &serial[i],
                                          &high_commit_serial);
  }
  n_result1 = sizeof(result1)/sizeof(result1[0]);
  n_result2 = sizeof(result2)/sizeof(result2[0]);
  n_result3 = sizeof(result3)/sizeof(result3[0]);
  n_result4 = sizeof(result4)/sizeof(result4[0]);

  rwdts_test_compare_serials(&list, n_result1, result1);
  
  for (i = RWDTS_TEST_MAX_SERIALS; i < RWDTS_TEST_MAX_SERIALS+4; i++) {
    memset(&serial[i], 0, sizeof(serial[i]));
    serial[i].id.member_id = 1;
    serial[i].id.router_id = 2;
    serial[i].id.pub_id    = 3;
    serial[i].serial = i+2;
    
    rwdts_add_entry_to_commit_serial_list(&list,
                                          &serial[i],
                                          &high_commit_serial);
  }
  dump_commit_serial_list(&list);
  rwdts_test_compare_serials(&list, n_result2, result2);

  for (i = 0; i < RWDTS_TEST_MAX_SERIALS; i++) {
    memset(&s, 0, sizeof(s));
    s.id.member_id = 1;
    s.id.router_id = 2;
    s.id.pub_id    = 3;
    s.serial = i+1;
    
    if ((i+1) % 8 == 0 ) {
      rwdts_add_entry_to_commit_serial_list(&list,
                                            &serial[i],
                                            &high_commit_serial);
    }
  }
  dump_commit_serial_list(&list);
  rwdts_test_compare_serials(&list, n_result3, result3);

  for (i = 0; i < RWDTS_TEST_MAX_SERIALS; i++) {
    memset(&s, 0, sizeof(s));
    s.id.member_id = 1;
    s.id.router_id = 2;
    s.id.pub_id    = 3;
    s.serial = i+1;
    
    if ((i+1) % 8 != 0  &&  (i+1) % 4 == 0) {
      rwdts_add_entry_to_commit_serial_list(&list,
                                            &serial[i],
                                            &high_commit_serial);
    }
  }
  dump_commit_serial_list(&list);
  rwdts_test_compare_serials(&list, n_result4, result4);

  rwdts_deinit_commit_serial_list(&list);

  EXPECT_EQ(RW_SKLIST_LENGTH(&list), 0);

  return;
}


TEST(RWDtsLiveRecovery, CommitSerials)
{
  rwdts_test_commit_serial_list();
}


RWDtsQuery* 
rwdts_gtest_journal_create_query(rwdts_member_registration_t *reg,
                                 uint64_t   queryidx)
{
  rwdts_member_reg_handle_inc_serial(reg);

  RWDtsQuery* query = rwdts_xact_test_init_empty_query(reg);
  query->key = rwdts_init_key_from_keyspec_int(reg->keyspec);
  query->action = RWDTS_QUERY_CREATE;
  query->flags = RWDTS_XACT_FLAG_DEPTH_ONE;
  query->has_queryidx = 1;
  query->queryidx = queryidx;
  query->serial->has_id = 1;
  query->serial->id.member_id = 888;
  query->serial->id.router_id = 999;
  query->serial->id.pub_id = 777;
  return query;
}

static rwdts_api_t*
rwdts_test_creat_api_handle(const char* path)
{
  rwdts_api_t *apih;

  rwsched_instance_ptr_t rwsched = rwsched_instance_new();
  RW_ASSERT(rwsched);

  rwsched_tasklet_ptr_t tasklet = rwsched_tasklet_new(rwsched);
  RW_ASSERT(tasklet);

  rwmsg_endpoint_t *ep = rwmsg_endpoint_create(1, 0, 0, rwsched, tasklet, rwtrace_init(), NULL);
  RW_ASSERT(ep);

  apih = rwdts_api_init_internal(NULL, NULL, tasklet, rwsched, 
                                 ep, path,
                                 RWDTS_HARDCODED_ROUTER_ID, 0, NULL);
  return (apih);
}

static void 
rwdts_test_cleanup(rwdts_api_t *apih)
{
  RW_ASSERT(apih);

  rwsched_tasklet_ptr_t tasklet = apih->tasklet;
  rwsched_instance_ptr_t rwsched = apih->sched;
  rwmsg_endpoint_t *ep = apih->client.ep;

  rw_status_t rs = rwdts_api_deinit(apih);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int r = rwmsg_endpoint_halt(ep);
  ASSERT_TRUE(r);
  ep = NULL;

  ASSERT_TRUE(tasklet);
  rwsched_tasklet_free(tasklet);
  tasklet = NULL;

  ASSERT_TRUE(rwsched);
  rwsched_instance_free(rwsched);
  rwsched = NULL;

  return;
}


TEST(RWDtsJournal, JournalApiTest) 
{
  TEST_DESCRIPTION("Tests DTS Journals APIs");


  rwdts_api_t* apih = rwdts_test_creat_api_handle("/L/API/JOURNALTEST/3");

  ASSERT_TRUE(apih);

  rw_keyspec_path_t *keyspec;
  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  rwdts_member_event_cb_t cb = { .cb = { NULL, NULL, NULL, NULL, NULL }, .ud = NULL };

  rwdts_member_registration_t *reg = rwdts_member_registration_init_local(apih, keyspec, 
                                                                          &cb, RWDTS_FLAG_SUBSCRIBER, 
                                                                          RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone));

  rw_status_t rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_DISABLED);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs= rwdts_journal_set_mode(reg, RWDTS_JOURNAL_LIVE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_DISABLED);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rs = rwdts_journal_destroy(reg);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  int indx, query_idx = 0;
  rwdts_xact_t *xact = rwdts_api_xact_create(apih, RWDTS_FLAG_NONE, NULL, 0);
  ASSERT_TRUE(xact);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  for (indx = 0; indx < (RWDTS_JOURNAL_IN_USE_QLEN + 3); indx++) {
    RWDtsQuery* query = rwdts_gtest_journal_create_query(reg, ++query_idx);
    ASSERT_TRUE(query);
    rwdts_xact_query_t *xquery;
    rwdts_xact_add_query_internal(xact, query, &xquery);
    rwdts_query__free_unpacked(NULL, query);
    query = NULL;
    ASSERT_TRUE(xquery);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_PREPARE);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_PRECOMMIT);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_QUERY_RSP);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_COMMIT_RSP);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_ABORT_RSP);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    if(indx % 2) {
      rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_COMMIT);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    }
    else {
      rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_ABORT);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    }
  }
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
  rs = rwdts_journal_destroy(reg);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  xact = rwdts_api_xact_create(apih, RWDTS_FLAG_NONE, NULL, 0);
  ASSERT_TRUE(xact);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  for (indx = 0; indx < (RWDTS_JOURNAL_IN_USE_QLEN + 3); indx++) {
    RWDtsQuery* query = rwdts_gtest_journal_create_query(reg, ++query_idx);
    ASSERT_TRUE(query);
    rwdts_xact_query_t *xquery;
    rwdts_xact_add_query_internal(xact, query, &xquery);
    ASSERT_TRUE(xquery);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_PREPARE);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_END);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  }
  rs = rwdts_journal_destroy(reg);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);

  xact = rwdts_api_xact_create(apih, RWDTS_FLAG_NONE, NULL, 0);
  ASSERT_TRUE(xact);
  rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  for (indx = 0; indx < (RWDTS_JOURNAL_IN_USE_QLEN * 30); indx++) {
    RWDtsQuery* query = rwdts_gtest_journal_create_query(reg, ++query_idx);
    ASSERT_TRUE(query);
    rwdts_xact_query_t *xquery;
    rwdts_xact_add_query_internal(xact, query, &xquery);
    ASSERT_TRUE(xquery);
    rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_PREPARE);
    ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    switch(indx % 3) {
      case 0:
        rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_COMMIT);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        break;
      case 1:
        rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_ABORT);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        break;
      case 2:
        rs = rwdts_journal_update(reg, xquery, RWDTS_MEMB_XACT_EVT_END);
        ASSERT_EQ(rs, RW_STATUS_SUCCESS);
        break;
    }
    if (indx && (!(indx % (RWDTS_JOURNAL_IN_USE_QLEN * 10)))) {
      rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_LIVE);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    }
    else if (indx && (!(indx % (RWDTS_JOURNAL_IN_USE_QLEN * 20)))) {
      rs = rwdts_journal_set_mode(reg, RWDTS_JOURNAL_IN_USE);
      ASSERT_EQ(rs, RW_STATUS_SUCCESS);
    }
    rwdts_query__free_unpacked(NULL, query);
  }
  rs = rwdts_journal_destroy(reg);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);
  rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);

  rwdts_test_cleanup(apih);
}
TEST(RWDtsLiveRecovery, RegistrationFsmTest) 
{
  TEST_DESCRIPTION("Tests The registration fsm");
  rw_keyspec_path_t *keyspec;
  rwdts_member_registration_t *reg;

  rwdts_api_t* apih = rwdts_test_creat_api_handle("/L/API/REGFSMTEST/1");
  ASSERT_TRUE(apih);

  keyspec = (rw_keyspec_path_t*)(RWPB_G_PATHSPEC_VALUE(DtsTest_data_Person_Phone));

  rwdts_member_event_cb_t cb = 
                              { .cb = { NULL, NULL, NULL, NULL, NULL }, 
                                .ud = NULL };

  reg  = rwdts_member_registration_init_local(apih, keyspec, 
                                              &cb, RWDTS_FLAG_SUBSCRIBER, 
                                              RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone));

  ASSERT_TRUE(reg);
  EXPECT_EQ(RW_DTS_REGISTRATION_STATE_INIT, reg->fsm.state);

  // Post a RW_DTS_REGISTRATION_EVT_BEGIN_REG event
  rwdts_registration_args_t args = {};
  args.keyspec = keyspec;
  args.desc = RWPB_G_MSG_PBCMD(DtsTest_data_Person_Phone);
  args.flags = RWDTS_FLAG_SUBSCRIBER;
  args.cb = &cb;

  rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_BEGIN_REG, &args);

  // Verify the the new state
  EXPECT_EQ(RW_DTS_REGISTRATION_STATE_REGISTERING, reg->fsm.state);

  // Post a RW_DTS_REGISTRATION_EVT_REGISTERED event
  rwdts_member_reg_run(reg, RW_DTS_REGISTRATION_EVT_REGISTERED, NULL);

  // Verify the the new state
  EXPECT_EQ(RW_DTS_REGISTRATION_STATE_REG_READY, reg->fsm.state);

  rwdts_test_cleanup(apih);
}

//
//FIXME: RIFT-9642 - This test will crash due to ref cnt issue 
//
TEST_F(RWDtsEnsemble, TestXactRefCnt)
{
  struct queryapi_state s = {};
  init_query_api_state(&s, RWDTS_QUERY_READ, FALSE, FALSE);
  memberapi_refcnt_test_execute(&s);
  memset(&s, 0xaa, sizeof(s));
}

RWUT_INIT();
