
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwmsg_gtest.cc
 * @author RIFT.io <info@riftio.com>
 * @date 11/18/2013
 * @brief Google test cases for testing rwdts
 *
 * @details Google test cases for testing rwdts.
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

#include <valgrind/valgrind.h>

#include "rwut.h"
#include "rwlib.h"
#include "rw_dl.h"
#include "rw_sklist.h"

#include <unistd.h>
#include <sys/syscall.h>

#include <testdts-rw-fpath.pb-c.h>
#include <rwdts_int.h>
#include <rwdts_router.h>


static void
add_pubobj_registration(rwdts_router_member_t *memb, uint32_t flags, uint32_t reg_id)
{
  rw_keyspec_path_t* keyspec = NULL;
  rwdts_memb_router_registration_t *reg = NULL;
  rw_status_t rs;

  reg = (rwdts_memb_router_registration_t *)RW_MALLOC0(sizeof(rwdts_memb_router_registration_t));
  RW_ASSERT(reg);
  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface), NULL ,
                                  (rw_keyspec_path_t**)&keyspec, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  reg->keyspec = keyspec;
  reg->flags = flags;

  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path000.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path000.key00.name, "colony541");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path001.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path001.key00.name, "nwc541");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path002.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path002.key00.name, "interface541");

  rs = RW_KEYSPEC_PATH_GET_BINPATH(reg->keyspec, NULL , (const uint8_t **)&reg->ks_binpath, &reg->ks_binpath_len, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  reg->rtr_reg_id = reg_id;

  if (reg->flags & RWDTS_FLAG_PUBLISHER) {
    HASH_ADD_KEYPTR(hh_reg, memb->registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
  } else {
    HASH_ADD_KEYPTR(hh_reg, memb->sub_registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
  }
  return;
}

static void
add_subobj_registration(rwdts_router_member_t *memb, uint32_t flags, uint32_t reg_id)
{
  rw_keyspec_path_t* keyspec = NULL;
  rwdts_memb_router_registration_t *reg = NULL;
  rw_status_t rs;

  reg = (rwdts_memb_router_registration_t *)RW_MALLOC0(sizeof(rwdts_memb_router_registration_t));
  RW_ASSERT(reg);
  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface), NULL ,
                                  (rw_keyspec_path_t**)&keyspec, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  reg->keyspec = keyspec;
  reg->flags = flags;

  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path000.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path000.key00.name, "colony145");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path001.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path001.key00.name, "nwc145");

  if (reg->flags & RWDTS_FLAG_PUBLISHER) {
    ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path002.has_key00 = 1;
    strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)reg->keyspec)->dompath.path002.key00.name, "interface145");
  }

  rs = RW_KEYSPEC_PATH_GET_BINPATH(reg->keyspec, NULL , (const uint8_t **)&reg->ks_binpath, &reg->ks_binpath_len, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  reg->rtr_reg_id = reg_id;

  if (reg->flags & RWDTS_FLAG_PUBLISHER) {
    HASH_ADD_KEYPTR(hh_reg, memb->registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
  } else {
    HASH_ADD_KEYPTR(hh_reg, memb->sub_registrations, reg->ks_binpath, reg->ks_binpath_len, reg);
  }
  return;
}

static void
del_reg_full(rwdts_memb_router_registration_t* reg)
{
  if (reg->keyspec) {
    rw_keyspec_path_free(reg->keyspec, NULL);
    reg->keyspec = NULL;
  }
  RW_FREE(reg);
  return;
}

static void
del_member_full(rwdts_router_member_t *memb)
{
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  HASH_ITER(hh_reg, memb->registrations, reg, tmp_reg) {
    HASH_DELETE(hh_reg, memb->registrations, reg);
    del_reg_full(reg);
  }
  HASH_ITER(hh_reg, memb->sub_registrations, reg, tmp_reg) {
    HASH_DELETE(hh_reg, memb->sub_registrations, reg);
    del_reg_full(reg);
  }
  RW_FREE(memb->msgpath);
  memb->msgpath = NULL;
  RW_FREE(memb);
  return;
}

TEST(RwDtsObjCheck, CheckPubObj)
{
  rwdts_router_member_t *memb1 = NULL, *memb2 = NULL;
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  uint32_t flags = 0;
  uint32_t reg_id = 100;
  rw_keyspec_path_t* ks = NULL;
  bool ksmatch = false;
  char matchdesc[128] = "??";
  int pubsubmatch = true;
  size_t missat= 0;
  rw_status_t rs;


  memb1 = (rwdts_router_member_t *)RW_MALLOC0(sizeof(rwdts_router_member_t));
  RW_ASSERT(memb1);
  memb1->msgpath = RW_STRDUP("Member1");
  memb2 = (rwdts_router_member_t *)RW_MALLOC0(sizeof(rwdts_router_member_t));
  RW_ASSERT(memb2);
  memb2->msgpath = RW_STRDUP("Member2");


  flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED;
  add_pubobj_registration(memb1, flags, reg_id);
  reg_id++;
  flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED|RWDTS_FLAG_SUBOBJECT;
  add_pubobj_registration(memb2, flags, reg_id);

  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext), NULL ,
                                  (rw_keyspec_path_t**)&ks, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext *)ks)->dompath.path000.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext *)ks)->dompath.path000.key00.name, "colony541");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext *)ks)->dompath.path001.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext *)ks)->dompath.path001.key00.name, "nwc541");

  HASH_ITER(hh_reg, memb1->registrations, reg, tmp_reg) {
    bool comp = rw_keyspec_path_is_match_detail(NULL, reg->keyspec, ks, &missat, NULL);
    rwdts_router_pub_obj(reg, ks, &pubsubmatch, &ksmatch, &comp, &matchdesc[0], &missat);
    EXPECT_EQ(pubsubmatch, false);
    EXPECT_EQ(ksmatch, false);
  }

  HASH_ITER(hh_reg, memb2->registrations, reg, tmp_reg) {
    bool comp = rw_keyspec_path_is_match_detail(NULL, reg->keyspec, ks, &missat, NULL);
    rwdts_router_pub_obj(reg, ks, &pubsubmatch, &ksmatch, &comp, &matchdesc[0], &missat);
    EXPECT_EQ(pubsubmatch, 0);
    EXPECT_EQ(ksmatch, true);
    EXPECT_STREQ(matchdesc, "object match");
  }
  del_member_full(memb1);
  del_member_full(memb2);
  rw_keyspec_path_free(ks, NULL);
}

TEST(RwDtsObjCheck, CheckSubObj)
{
  rwdts_router_member_t *memb1 = NULL, *memb2 = NULL, *memb3 = NULL;
  rwdts_memb_router_registration_t *reg = NULL, *tmp_reg = NULL;
  uint32_t flags = 0;
  uint32_t reg_id = 200;
  uint32_t n_obj_list = 0;
  rwdts_router_pub_obj_t **pub_obj_list = NULL;
  rw_keyspec_path_t* ks = NULL;
  bool ksmatch = false;
  char matchdesc[128] = "??";
  int pubsubmatch = true;
  size_t missat= 0;
  rw_status_t rs;


  memb1 = (rwdts_router_member_t *)RW_MALLOC0(sizeof(rwdts_router_member_t));
  RW_ASSERT(memb1);
  memb1->msgpath = RW_STRDUP("Member1");
  memb2 = (rwdts_router_member_t *)RW_MALLOC0(sizeof(rwdts_router_member_t));
  RW_ASSERT(memb2);
  memb2->msgpath = RW_STRDUP("Member2");
  memb3 = (rwdts_router_member_t *)RW_MALLOC0(sizeof(rwdts_router_member_t));
  RW_ASSERT(memb3);
  memb3->msgpath = RW_STRDUP("Member3");

  flags = RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE;
  add_subobj_registration(memb1, flags, reg_id);
  reg_id++;
  flags = RWDTS_FLAG_SUBSCRIBER|RWDTS_FLAG_CACHE|RWDTS_FLAG_DEPTH_OBJECT;
  add_subobj_registration(memb2, flags, reg_id);
  reg_id++;
  flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED|RWDTS_FLAG_SUBOBJECT;
  add_subobj_registration(memb3, flags, reg_id);

  rs = RW_KEYSPEC_PATH_CREATE_DUP((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(TestdtsRwFpath_TestdtsRwBase_data_Colony_NetworkContext_Interface), NULL ,
                                  (rw_keyspec_path_t**)&ks, NULL);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path000.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path000.key00.name, "colony145");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path001.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path001.key00.name, "nwc145");
  ((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path002.has_key00 = 1;
  strcpy(((TestdtsRwFpath__YangKeyspecPath__TestdtsRwBase__Data__Colony__NetworkContext__Interface *)ks)->dompath.path002.key00.name, "interface145");

  rwdts_router_update_pub_obj(memb1, &n_obj_list, &pub_obj_list, ks);
  rwdts_router_update_pub_obj(memb2, &n_obj_list, &pub_obj_list, ks);

  HASH_ITER(hh_reg, memb1->registrations, reg, tmp_reg) {
    bool comp = rw_keyspec_path_is_match_detail(NULL, reg->keyspec, ks, &missat, NULL);
    rwdts_router_sub_obj(reg, n_obj_list, pub_obj_list, &pubsubmatch, &ksmatch, &comp, &matchdesc[0]);
    EXPECT_EQ(pubsubmatch, true);
    EXPECT_EQ(ksmatch, false);
  }

  HASH_ITER(hh_reg, memb2->registrations, reg, tmp_reg) {
    bool comp = rw_keyspec_path_is_match_detail(NULL, reg->keyspec, ks, &missat, NULL);
    rwdts_router_sub_obj(reg, n_obj_list, pub_obj_list, &pubsubmatch, &ksmatch, &comp, &matchdesc[0]);
    EXPECT_EQ(pubsubmatch, false);
    EXPECT_EQ(ksmatch, true);
    EXPECT_STREQ(matchdesc, "object match");
  }

  del_member_full(memb1);
  del_member_full(memb2);
  del_member_full(memb3);
  rw_keyspec_path_free(ks, NULL);
}

const char *key_entry[30] = {"name1", "name2", "name3", "name4", "name5", "name6", "name7",
                      "name8", "name9", "name10", "name11", "name12", "name13",
                      "name14", "name15", "name16", "name17", "name18", "name19"};

const char *tab_entry[30] = {"TEST", "Sheldon", "Leonard", "Raj", "Howard", "Bernie",
                       "Amy", "Steve", "Wilma", "Fred", "Charlie", "Penny","What",
                       "When", "Why", "Who", "How", "BBC", "CNN"};

rwdts_kv_light_reply_status_t
rwdts_kv_handle_get_all_cb(void **key, int *key_len, void **val,
                           int *val_len, int total, void *callbk_data)
{
  int *cb_data = (int *)callbk_data;
  RW_ASSERT(cb_data);

  EXPECT_EQ(total, *cb_data);

  int i;
  char res_val[50];
  char res_key[50];
  for(i=0; i < total; i++) {
    memcpy(res_val, (char *)val[i], val_len[i]);
    res_val[val_len[i]] = '\0';
    memcpy(res_key, (char *)key[i], key_len[i]);
    res_key[key_len[i]] = '\0';
    fprintf(stderr, "res_key = %s, res_val = %s\n", res_val, res_key);
    free(key[i]);
    free(val[i]);
  }
  free(key);
  free(val);

  return RWDTS_KV_LIGHT_REPLY_DONE;
}

TEST(RwDtsFileDB, FileBkDB)
{
  rw_status_t res;
  rwdts_kv_handle_t* handle = (rwdts_kv_handle_t*)rwdts_kv_allocate_handle(BK_DB);
  RW_ASSERT(handle);
  int* cb_data = (int *)RW_MALLOC0(sizeof(int));
  int i;

  res = rwdts_kv_handle_db_connect(handle, NULL, NULL, NULL, "test_client.db", NULL, NULL, NULL);
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  res = rwdts_kv_handle_add_keyval(handle, 0, (char *)key_entry[0],
                                   strlen(key_entry[0]), (char *)tab_entry[0], strlen(tab_entry[0]), NULL, NULL);
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
  for (i = 0; i< 19; i++) {
    res = rwdts_kv_handle_add_keyval(handle, 0, (char *)key_entry[i],
                                     strlen(key_entry[i]), (char *)tab_entry[i], strlen(tab_entry[i]), NULL, NULL);
    EXPECT_EQ(res, RW_STATUS_SUCCESS);
  }

  *cb_data = 19;
  rwdts_kv_handle_get_all(handle, 0, (void *)rwdts_kv_handle_get_all_cb, cb_data);

  for (i=0; i < 5; i++) {
    res = rwdts_kv_handle_del_keyval(handle, 0, (char *)key_entry[i],
                                     strlen(key_entry[i]), NULL, NULL);
    EXPECT_EQ(res, RW_STATUS_SUCCESS);
  }

  *cb_data = 14;
  rwdts_kv_handle_get_all(handle, 0, (void *)rwdts_kv_handle_get_all_cb, cb_data);

  res = rwdts_kv_handle_file_remove(handle);
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
}  

#if 0
TEST(RwDtsCredits, CreditsDispenserAccuracy)
{
  rwdts_credit_obj_t credits;
  memset(&credits, 0, sizeof(rwdts_credit_obj_t));
  credits.credits_total  = 1000;
  credits.credits_remaining  = 1000;
  uint32_t mycredits = 0;

  // Go to HWM
  for (int i = 0; i<100;i++)
  {
    EXPECT_EQ(RWDTS_GET_CREDITS(credits, 1), 20);
  }
}

TEST(RwDtsMember, Registration)
{
}
TEST(RwDtsMember, Prepare)
{
}
TEST(RwDtsMember, PrepareCbNonXact)
{
}
TEST(RwDtsMember, PreCommit)
{
}
TEST(RwDtsMember, Commit)
{
}
TEST(RwDtsMember, Abort)
{
}
#endif
