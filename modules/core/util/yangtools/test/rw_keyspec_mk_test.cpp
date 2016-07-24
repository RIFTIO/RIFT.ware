
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rw_keyspec_mk_test.cpp
 * @author Sujithra Periasamy
 * @date 2014/26/11
 * @brief Google test cases for testing schema minikeys.
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include "rwut.h"
#include "rwlib.h"
#include "rw_schema.pb-c.h"
#include "rw_keyspec_mk.h"
#include "minikeys.pb-c.h"
#include "company.pb-c.h"
#include "rw-fpath-d.pb-c.h"

#include <string.h>
#include <stdio.h>

TEST(RwSchemaMKs, BasicMiniKeys)
{
  TEST_DESCRIPTION("Tests basic Minikeys creation");

  RWPB_MINIKEY_DEF_INIT_INT32(mk_int32, 54)
  RWPB_MINIKEY_DEF_INIT_INT64(mk_int64, -89898989898)
  RWPB_MINIKEY_DEF_INIT_UINT32(mk_uint32, 90)
  RWPB_MINIKEY_DEF_INIT_UINT64(mk_uint64, 9999999999)
  RWPB_MINIKEY_DEF_INIT_FLOAT(mk_float, 67.99)
  RWPB_MINIKEY_DEF_INIT_DOUBLE(mk_double, 889.0001)
  RWPB_MINIKEY_DEF_INIT_STRING(mk_string, "HelloWorld")
  RWPB_MINIKEY_DEF_INIT_BOOL(mk_bool, true)
  RWPB_MINIKEY_DEF_INIT_BYTES(mk_bytes, "AZk.", 4)

  char k3[20];
  strcpy(k3, "Hello");

  RWPB_M_MK_DECL_SET(Minikeys_data_MultiKeys, mk_ml, 54, k3, 1000);
}

TEST(RwSchemaMKs, MiniKeyCreate)
{
  rw_schema_minikey_opaque_t mk;

  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee,emp);

  emp.id = 100;
  emp.name = strdup("Alice");
  emp.title = strdup("SE");
  emp.start_date = strdup("14/11");

  rw_status_t s = rw_schema_mk_get_from_message(&emp.base, &mk);

  EXPECT_EQ(s, RW_STATUS_SUCCESS);

  rw_minikey_basic_int32_t *mk_i = (rw_minikey_basic_int32_t *)mk;
  EXPECT_EQ(mk_i->k.desc, &rw_var_minikey_basic_int32_desc);
  EXPECT_EQ(mk_i->k.key, 100);

  char buff[512];
  int ret = rw_schema_mk_get_print_buffer(&mk, buff, 512);
  EXPECT_NE(ret, -1);
  EXPECT_FALSE(strcmp(buff, "100"));

  RWPB_M_PATHENTRY_DECL_INIT(Company_data_Company_Employee,pe);

  pe.has_key00 = true;
  pe.key00.id = 100;

  rw_schema_minikey_opaque_t mk1;

  s = rw_schema_mk_get_from_path_entry(
      reinterpret_cast<rw_keyspec_entry_t *>(&pe), &mk1);

  mk_i = (rw_minikey_basic_int32_t *)mk1;
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(mk_i->k.desc, &rw_var_minikey_basic_int32_desc);
  EXPECT_EQ(mk_i->k.key, 100);

  protobuf_c_message_free_unpacked_usebody(NULL, &emp.base);

  RWPB_M_PATHENTRY_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, be);
  be.has_key00 = true;
  strcpy(be.key00.name, "bundle1");

  rw_schema_minikey_opaque_t mk2;

  s = rw_schema_mk_get_from_path_entry(
      reinterpret_cast<rw_keyspec_entry_t *>(&be), &mk2);

  rw_minikey_basic_string_t *mk_s = (rw_minikey_basic_string_t *)mk2;
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(mk_s->k.desc, &rw_var_minikey_basic_string_desc);
  EXPECT_STREQ(mk_s->k.key, "bundle1");

  rw_keyspec_entry_t *peg = rw_keyspec_entry_create_dup_of_type(
      &pe.rw_keyspec_entry_t, NULL, &rw_schema_path_entry__descriptor);
  ASSERT_TRUE(peg);
  UniquePtrKeySpecEntry::uptr_t uptr(peg);

  memset(mk1, 0, sizeof(mk1));
  s = rw_schema_mk_get_from_path_entry(peg, &mk1);
  mk_i = (rw_minikey_basic_int32_t *)mk1;
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(mk_i->k.desc, &rw_var_minikey_basic_int32_desc);
  EXPECT_EQ(mk_i->k.key, 100);

  peg =  rw_keyspec_entry_create_dup_of_type(
      &be.rw_keyspec_entry_t, NULL, &rw_schema_path_entry__descriptor);
  ASSERT_TRUE(peg);
  uptr.reset(peg);

  memset(mk2, 0, sizeof(mk2));
  s = rw_schema_mk_get_from_path_entry(peg, &mk2);
  mk_s = (rw_minikey_basic_string_t *)mk2;
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(mk_s->k.desc, &rw_var_minikey_basic_string_desc);
  EXPECT_STREQ(mk_s->k.key, "bundle1");
}

TEST(RwSchemaMKs, MiniKeyCopy)
{
  RWPB_M_MK_DECL_SET(Company_data_Company_Employee, mk, 100);
  RWPB_M_PATHENTRY_DECL_INIT(Company_data_Company_Employee, path_entry);

  rw_status_t s = rw_schema_mk_copy_to_path_entry(&mk.opaque, 
                              reinterpret_cast<rw_keyspec_entry_t *>(&path_entry));

  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(path_entry.has_key00);
  EXPECT_EQ(path_entry.key00.id, 100);

  RWPB_M_MK_DECL_SET(Minikeys_data_Company_MkStrinl, mk1, "StringKey");
  RWPB_M_PATHENTRY_DECL_INIT(Minikeys_data_Company_MkStrinl, path_entry1);

  s = rw_schema_mk_copy_to_path_entry(&mk1.opaque,
                         reinterpret_cast<rw_keyspec_entry_t *>(&path_entry1));

  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(path_entry1.has_key00);
  EXPECT_STREQ(path_entry1.key00.s1, "StringKey");

  RWPB_M_PATHSPEC_DECL_INIT(Company_data_Company_Employee, ks);
  s = rw_schema_mk_copy_to_keyspec(reinterpret_cast<rw_keyspec_path_t *>(&ks),
                                   &mk.opaque, 1);

  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(ks.dompath.path001.has_key00);
  EXPECT_EQ(ks.dompath.path001.key00.id, 100);

  RWPB_M_PATHSPEC_DECL_INIT(Minikeys_data_Company_MkStrinl, ks1);
  s = rw_schema_mk_copy_to_keyspec(reinterpret_cast<rw_keyspec_path_t *>(&ks1),
                                   &mk1.opaque, 1);

  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(ks1.dompath.path001.has_key00);
  EXPECT_STREQ(ks1.dompath.path001.key00.s1, "StringKey");

  RWPB_M_MK_DECL_INIT(Minikeys_data_Company_MkString, mk2);
  mk2.k.key = strdup("PointyStringKey");

  RWPB_M_PATHENTRY_DECL_INIT(Minikeys_data_Company_MkString, path_entry2);

  s = rw_schema_mk_copy_to_path_entry(&mk2.opaque,
                reinterpret_cast<rw_keyspec_entry_t *>(&path_entry2));

  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(path_entry2.has_key00);
  EXPECT_STREQ(path_entry2.key00.name, "PointyStringKey");

  RWPB_M_PATHSPEC_DECL_INIT(Minikeys_data_Company_MkString, ks2);
  s = rw_schema_mk_copy_to_keyspec(reinterpret_cast<rw_keyspec_path_t *>(&ks2),
                                   &mk2.opaque, 1);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_TRUE(ks2.dompath.path001.has_key00);
  EXPECT_STREQ(ks2.dompath.path001.key00.name, "PointyStringKey");

  RWPB_F_MSG_FREE_KEEP_BODY(Minikeys_data_Company_MkString, &path_entry2, NULL);
  RWPB_F_MSG_FREE_KEEP_BODY(Minikeys_data_Company_MkString, &ks2, NULL);

  free(mk2.k.key);
}
