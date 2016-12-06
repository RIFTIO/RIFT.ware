
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
 * @file ctype_test.cpp
 * @author Tom Seidenberg
 * @date 2014/12/31
 * @brief Test rwpb:field-c-type
 */

#include "rwut.h"
#include "rwlib.h"
#include "ctype-test.pb-c.h"
#include "yangmodel.h"
#include "rw_xml.h"

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

using namespace rw_yang;

static ProtobufCInstance ctype_pbc_instance = {
  .alloc = &ctype_alloc,
  .zalloc = &ctype_zalloc,
  .realloc = &ctype_realloc,
  .free = &ctype_free,
  .error = &ctype_error,
  .data = NULL,
};

/*****************************************************************************/

TEST(RwCType, BasicInit)
{
  TEST_DESCRIPTION("Test basic ctypes allocation on stack");
  ctype_clear_call_counters();

  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList,  cbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList,   cfll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyList,  cbl);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatList,   cfl);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeaf, nbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeaf,  nff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeafList, nbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeafList,  nfll);

  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeBumpyLeaf,  pcbf);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeFlatLeaf,   pcff);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeBumpyLeafList,  pcbll);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeFlatLeafList,   pcfll);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeBumpyList,  pcbl);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_CTypeFlatList,   pcfl);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_NativeBumpyLeaf, pnbf);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_NativeFlatLeaf,  pnff);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_NativeBumpyLeafList, pnbll);
  RWPB_M_MSG_DECL_PTR_ALLOC(CtypeTest_NativeFlatLeafList,  pnfll);

  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcbf->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcff->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcbll->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcfll->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcbl->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pcfl->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pnbf->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pnff->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pnbll->base);
  protobuf_c_message_free_unpacked(&ctype_pbc_instance, &pnfll->base);

  EXPECT_EQ(ctype_calls_to_free_usebody, 0);
}


/*****************************************************************************/

static void test_good_text_field(
  const char* field_name,
  const char* const* from_str_values,
  const char* const* to_str_values,
  size_t n_values)
{
  for (unsigned i = 0; i < n_values; ++i) {
    const char* from_str = from_str_values[i];
    printf("good test: %s %s %s\n", field_name, from_str, to_str_values?to_str_values[i]:"");
    size_t from_len = strlen(from_str);
    ctype_clear_call_counters();

    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList,  cbll);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList,   cfll);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyList,  cbl);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatList,   cfl);
    RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeaf, nbf);
    RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeaf,  nff);
    RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeafList, nbll);
    RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeafList,  nfll);
    ProtobufCMessage* msgs[] = {
      &cbf.base, &cff.base, &cbll.base, &cfll.base, &cbl.base, &cfl.base,
      &nbf.base, &nff.base, &nbll.base, &nfll.base,
    };

    unsigned msgi = 0;
    for (auto& msg: msgs) {
      // auto-clean-up
      UniquePtrProtobufCMessageUseBody<>::uptr_t msg_uptr(msg, &ctype_pbc_instance);

      ProtobufCMessage* outer_msg = msg;
      ProtobufCMessage* inner_msg = msg;
      const ProtobufCFieldDescriptor* ifd
        = protobuf_c_message_descriptor_get_field_by_name(outer_msg->descriptor, field_name);
      ASSERT_TRUE(ifd);
      const ProtobufCFieldDescriptor* ofd = ifd;

      /* If target is listy message, find inner field */
      if (   PROTOBUF_C_LABEL_REPEATED == ofd->label
          && PROTOBUF_C_TYPE_MESSAGE == ofd->type) {
        inner_msg = nullptr;
        bool ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, outer_msg, ofd, &inner_msg);
        EXPECT_TRUE(ok);
        ASSERT_TRUE(inner_msg);
        ifd = protobuf_c_message_descriptor_get_field_by_name(inner_msg->descriptor, field_name);
        ASSERT_TRUE(ifd);
      }

      bool ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, inner_msg, ifd, from_str, from_len);
      EXPECT_TRUE(ok);

      size_t expected_n = 1;
      if (PROTOBUF_C_LABEL_REPEATED == ifd->label) {
        ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, inner_msg, ifd, from_str, from_len);
        EXPECT_TRUE(ok);
        expected_n = 2;
      }

      ok = protobuf_c_message_check(&ctype_pbc_instance, outer_msg);
      EXPECT_TRUE(ok);

      uint8_t buf[4096];
      size_t packed = protobuf_c_message_pack(&ctype_pbc_instance, outer_msg, buf);
      ASSERT_GT(packed, 5);

      UniquePtrProtobufCMessage<>::uptr_t umsg_uptr(
        protobuf_c_message_unpack(&ctype_pbc_instance, outer_msg->descriptor, packed, buf),
        &ctype_pbc_instance);
      ASSERT_TRUE(umsg_uptr.get());
      ok = protobuf_c_message_is_equal_deep(&ctype_pbc_instance, outer_msg, umsg_uptr.get());
      EXPECT_TRUE(ok);


      const ProtobufCFieldDescriptor* newfd = NULL;
      void* value = NULL;
      size_t offset = 0;
      protobuf_c_boolean array_of_ptrs = FALSE;
      size_t count = protobuf_c_message_get_field_desc_count_and_offset(
        umsg_uptr.get(), ofd->id, &newfd, &value, &offset, &array_of_ptrs);

      /* If target is listy message, redo lookup on inner message */
      if (   PROTOBUF_C_LABEL_REPEATED == ofd->label
          && PROTOBUF_C_TYPE_MESSAGE == ofd->type) {
        ASSERT_EQ(count, 1);
        EXPECT_EQ(ofd, newfd);
        ASSERT_NE(value, nullptr);
        if (array_of_ptrs) {
          count = protobuf_c_message_get_field_desc_count_and_offset(
            ((ProtobufCMessage**)value)[0], ifd->id, &newfd, &value, &offset, &array_of_ptrs);
        } else {
          count = protobuf_c_message_get_field_desc_count_and_offset(
            (ProtobufCMessage*)value, ifd->id, &newfd, &value, &offset, &array_of_ptrs);
        }
      }

      EXPECT_EQ(count, expected_n);
      EXPECT_EQ(ifd, newfd);
      EXPECT_NE(value, nullptr);

      char str[4096] = { 0 };
      size_t new_len = sizeof(str);
      if (array_of_ptrs) {
        ok = protobuf_c_field_get_text_value(NULL, ifd, str, &new_len, *(void**)value);
      } else {
        ok = protobuf_c_field_get_text_value(NULL, ifd, str, &new_len, value);
      }
      ASSERT_TRUE(ok);
      EXPECT_LT(new_len, sizeof(str));
      if (to_str_values && msgi < 6) {
        // ctype
        EXPECT_STREQ(to_str_values[i], str);
      } else {
        // native string
        EXPECT_STREQ(from_str, str);
      }

      umsg_uptr.reset();
      msg_uptr.reset();
      EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
      ++msgi;
    }

    EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
    EXPECT_EQ(ctype_calls_to_get_packed_size, 0);
    EXPECT_EQ(ctype_calls_to_pack, 0);
    EXPECT_EQ(ctype_calls_to_unpack, 0);
    EXPECT_EQ(ctype_calls_to_to_string, 0);
    EXPECT_EQ(ctype_calls_to_from_string, 0);
    EXPECT_EQ(ctype_calls_to_init_usebody, 0);
    EXPECT_EQ(ctype_calls_to_free_usebody, 0);
    EXPECT_EQ(ctype_calls_to_check, 0);
    EXPECT_EQ(ctype_calls_to_deep_copy, 0);
    EXPECT_EQ(ctype_calls_to_compare, 0);
  }
}

static void test_bad_text_field(
  const char* field_name,
  const char* const* from_str_values,
  size_t n_values)
{
  for (unsigned i = 0; i < n_values; ++i) {
    const char* from_str = from_str_values[i];
    printf("bad test: %s %s\n", field_name, from_str);
    size_t from_len = strlen(from_str);
    ctype_clear_call_counters();

    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList,  cbll);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList,   cfll);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyList,  cbl);
    RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatList,   cfl);
    ProtobufCMessage* msgs[] = {
      &cbf.base, &cff.base, &cbll.base, &cfll.base, &cbl.base, &cfl.base,
    };

    for (auto& msg: msgs) {
      // auto-clean-up
      UniquePtrProtobufCMessageUseBody<>::uptr_t msg_uptr(msg, &ctype_pbc_instance);

      ProtobufCMessage* outer_msg = msg;
      ProtobufCMessage* inner_msg = msg;
      const ProtobufCFieldDescriptor* ifd
        = protobuf_c_message_descriptor_get_field_by_name(outer_msg->descriptor, field_name);
      ASSERT_TRUE(ifd);
      const ProtobufCFieldDescriptor* ofd = ifd;

      /* If target is listy message, find inner field */
      if (   PROTOBUF_C_LABEL_REPEATED == ofd->label
          && PROTOBUF_C_TYPE_MESSAGE == ofd->type) {
        inner_msg = nullptr;
        bool ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, outer_msg, ofd, &inner_msg);
        EXPECT_TRUE(ok);
        ASSERT_TRUE(inner_msg);
        ifd = protobuf_c_message_descriptor_get_field_by_name(inner_msg->descriptor, field_name);
        ASSERT_TRUE(ifd);
      }

      bool ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, inner_msg, ifd, from_str, from_len);
      EXPECT_FALSE(ok);

      if (PROTOBUF_C_LABEL_REPEATED == ifd->label) {
        ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, inner_msg, ifd, from_str, from_len);
        EXPECT_FALSE(ok);
      }

      ok = protobuf_c_message_check(&ctype_pbc_instance, outer_msg);
      EXPECT_TRUE(ok);

      uint8_t buf[4096] = { 0 };
      size_t packed = protobuf_c_message_pack(&ctype_pbc_instance, outer_msg, buf);

      UniquePtrProtobufCMessage<>::uptr_t umsg_uptr(
        protobuf_c_message_unpack(&ctype_pbc_instance, outer_msg->descriptor, packed, buf),
        &ctype_pbc_instance);
      if (umsg_uptr.get()) {
        ok = protobuf_c_message_is_equal_deep(&ctype_pbc_instance, outer_msg, umsg_uptr.get());
        EXPECT_TRUE(ok);

        const ProtobufCFieldDescriptor* newfd = NULL;
        void* value = NULL;
        size_t offset = 0;
        protobuf_c_boolean array_of_ptrs = FALSE;
        size_t count = protobuf_c_message_get_field_desc_count_and_offset(
          umsg_uptr.get(), ofd->id, &newfd, &value, &offset, &array_of_ptrs);

        /* If target is listy message, redo lookup on inner message */
        if (   PROTOBUF_C_LABEL_REPEATED == ofd->label
            && PROTOBUF_C_TYPE_MESSAGE == ofd->type) {
          ASSERT_EQ(count, 1);
          EXPECT_EQ(ofd, newfd);
          ASSERT_NE(value, nullptr);
          if (array_of_ptrs) {
            count = protobuf_c_message_get_field_desc_count_and_offset(
              ((ProtobufCMessage**)value)[0], ifd->id, &newfd, &value, &offset, &array_of_ptrs);
          } else {
            count = protobuf_c_message_get_field_desc_count_and_offset(
              (ProtobufCMessage*)value, ifd->id, &newfd, &value, &offset, &array_of_ptrs);
          }
          EXPECT_EQ(count, 0);
        } else {
          EXPECT_EQ(count, 0);
        }
      }

      umsg_uptr.reset();
      msg_uptr.reset();
      EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
    }

    EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
    EXPECT_EQ(ctype_calls_to_get_packed_size, 0);
    EXPECT_EQ(ctype_calls_to_pack, 0);
    EXPECT_EQ(ctype_calls_to_unpack, 0);
    EXPECT_EQ(ctype_calls_to_to_string, 0);
    EXPECT_EQ(ctype_calls_to_from_string, 0);
    EXPECT_EQ(ctype_calls_to_init_usebody, 0);
    EXPECT_EQ(ctype_calls_to_free_usebody, 0);
    EXPECT_EQ(ctype_calls_to_check, 0);
    EXPECT_EQ(ctype_calls_to_deep_copy, 0);
    EXPECT_EQ(ctype_calls_to_compare, 0);
  }
}

static const char* const ip4[] = {
  "0.0.0.0",
  "1.2.3.4",
  "010.020.030.040", // 8.16.24.32
  "1.12.123.234",
  "254.254.254.254",
  "255.255.255.255",
  "0111.0123.0234.0345", // 73.83.156.229
  "0x12.0x21.0xab.0xff", // 18.33.171.255
  "0x12.0x21.0xabff", // 18.33.171.255
  "0x12.0x21abff", // 18.33.171.255
  "0x12345678", // 18.52.86.120
  "305419896", // 18.52.86.120
  "02215053170", // 18.52.86.120
};

static const char* const ip4_canonical[] = {
  "0.0.0.0",
  "1.2.3.4",
  "8.16.24.32",
  "1.12.123.234",
  "254.254.254.254",
  "255.255.255.255",
  "73.83.156.229",
  "18.33.171.255",
  "18.33.171.255",
  "18.33.171.255",
  "18.52.86.120",
  "18.52.86.120",
  "18.52.86.120",
};

static const char* const ip4_bad[] = {
  "255255255255",
  "a.b.c.d",
  "1:2:3:4",
  "1,2,3,4",
  "1.2.3.",
  ".1.2.3",
  "256.256.256.256",
  "1.2.3.256",
  "256.1.2.3",
  "25..2.3",
  "bogus",
  "deadbeef",
};


TEST(RwCType, IpV4)
{
  TEST_DESCRIPTION("Test IpV4 ctypes");
  test_good_text_field("ip", ip4, ip4_canonical, sizeof(ip4)/sizeof(ip4[0]));
  test_good_text_field("ip4", ip4, ip4_canonical, sizeof(ip4)/sizeof(ip4[0]));

  test_bad_text_field("ip", ip4_bad, sizeof(ip4_bad)/sizeof(ip4_bad[0]));
  test_bad_text_field("ip4", ip4_bad, sizeof(ip4_bad)/sizeof(ip4_bad[0]));
}


static const char* const ip6[] = {
  "1234:5678:0000:0000:0123:0056:0009:159d",
  "0000:0000:0000:0000:0000:0000:0000:0000",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
  "::",
  "::1",
  "1234:5678:0:0:123:56:9:159d",
  "1234:5678::123:56:9:159d",
  "1::2345",
  "::ffff:10.10.10.10",
  "0000:0000:0000:0000:0000:ffff:255.255.255.255",
};

static const char* const ip6_canonical[] = {
  "1234:5678::123:56:9:159d",
  "::",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
  "::",
  "::1",
  "1234:5678::123:56:9:159d",
  "1234:5678::123:56:9:159d",
  "1::2345",
  "::ffff:10.10.10.10",
  "::ffff:255.255.255.255",
};

static const char* const ip6_bad[] = {
  "1234:5678:0000:0000:0123:0056:0009:159g",
  "1234:5678:0000:0000:0123:0056:0009",
  // ATTN: This gets accepted: "0000:0000:0000:0000:00000:0000:0000:0000",
  "0000:0000:0000:0000:fffff:0000:0000:0000",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
  "ffff::ffff:ffff:ffff::ffff:ffff",
  ":::",
  "i:j:k:l:m:n:o:p",
  "bogus",
  "deadbeef",
};

TEST(RwCType, IpV6)
{
  TEST_DESCRIPTION("Test IpV6 ctypes");
  test_good_text_field("ip", ip6, ip6_canonical, sizeof(ip6)/sizeof(ip6[0]));
  test_bad_text_field("ip", ip6_bad, sizeof(ip6_bad)/sizeof(ip6_bad[0]));
  test_good_text_field("ip6", ip6, ip6_canonical, sizeof(ip6)/sizeof(ip6[0]));
  test_bad_text_field("ip6", ip6_bad, sizeof(ip6_bad)/sizeof(ip6_bad[0]));
}


static const char* const ippfx[] = {
  "0.0.0.0/01",
  "1.2.3.4/32",
  "010.020.030.040/12", // 8.16.24.32
  "1.12.123.234/28",
  "254.254.254.254/1",
  "255.255.255.255/010",
  "0111.0123.0234.0345/0x12", // 73.83.156.229
  "0x12.0x21.0xab.0xff/032", // 18.33.171.255
  "0x12.0x21.0xabff/32", // 18.33.171.255
  "0x12.0x21abff/0", // 18.33.171.255
  "0x12345678/000", // 18.52.86.120
  "305419896/1", // 18.52.86.120
  "02215053170/2", // 18.52.86.120
  "1234:5678:0000:0000:0123:0056:0009:159d/128",
  "0000:0000:0000:0000:0000:0000:0000:0000/127",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/001",
  "::/0",
  "::1/0x80",
  "1234:5678:0:0:123:56:9:159d/82",
  "1234:5678::123:56:9:159d/28",
  "1::2345/18",
  "::ffff:10.10.10.10/033",
  "0000:0000:0000:0000:0000:ffff:255.255.255.255/32",
};

static const char* const ippref_canonical[] = {
  "0.0.0.0/1",
  "1.2.3.4/32",
  "8.16.24.32/12",
  "1.12.123.234/28",
  "254.254.254.254/1",
  "255.255.255.255/8",
  "73.83.156.229/18",
  "18.33.171.255/26",
  "18.33.171.255/32",
  "18.33.171.255/0",
  "18.52.86.120/0",
  "18.52.86.120/1",
  "18.52.86.120/2",
  "1234:5678::123:56:9:159d/128",
  "::/127",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/1",
  "::/0",
  "::1/128",
  "1234:5678::123:56:9:159d/82",
  "1234:5678::123:56:9:159d/28",
  "1::2345/18",
  "::ffff:10.10.10.10/27",
  "::ffff:255.255.255.255/32",
};

static const char* const ippref_bad[] = {
  "255255255255",
  "a.b.c.d",
  "1:2:3:4",
  "1,2,3,4",
  "1.2.3.",
  ".1.2.3",
  "256.256.256.256",
  "255.255.255.255/008",
  "1.2.3.256",
  "256.1.2.3",
  "25..2.3",
  "bogus",
  "deadbeef",
  "1234:5678:0000:0000:0123:0056:0009:159g",
  "1234:5678:0000:0000:0123:0056:0009",
  "0000:0000:0000:0000:fffff:0000:0000:0000",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
  "ffff::ffff:ffff:ffff::ffff:ffff",
  ":::",
  "i:j:k:l:m:n:o:p",
  "bogus",
  "deadbeef",
  // ATTN: accepted!: "18.52.86.120/33",
  // ATTN: accepted!: "18.52.86.120/222",
  // ATTN: accepted!: "1234:5678::123:56:9:159d/129",
};

TEST(RwCType, IpPref)
{
  TEST_DESCRIPTION("Test IpPrefix ctypes");
  test_good_text_field("ippfx", ippfx, ippref_canonical, sizeof(ippfx)/sizeof(ippfx[0]));
  test_bad_text_field("ippfx", ippref_bad, sizeof(ippref_bad)/sizeof(ippref_bad[0]));
}

static const char* const ippfx4[] = {
  "0.0.0.0/01",
  "1.2.3.4/32",
  "010.020.030.040/12", // 8.16.24.32
  "1.12.123.234/28",
  "254.254.254.254/1",
  "255.255.255.255/010",
  "0111.0123.0234.0345/0x12", // 73.83.156.229
  "0x12.0x21.0xab.0xff/032", // 18.33.171.255
  "0x12.0x21.0xabff/32", // 18.33.171.255
  "0x12.0x21abff/0", // 18.33.171.255
  "0x12345678/000", // 18.52.86.120
  "305419896/1", // 18.52.86.120
  "02215053170/2", // 18.52.86.120
};

static const char* const ippfx4_canonical[] = {
  "0.0.0.0/1",
  "1.2.3.4/32",
  "8.16.24.32/12",
  "1.12.123.234/28",
  "254.254.254.254/1",
  "255.255.255.255/8",
  "73.83.156.229/18",
  "18.33.171.255/26",
  "18.33.171.255/32",
  "18.33.171.255/0",
  "18.52.86.120/0",
  "18.52.86.120/1",
  "18.52.86.120/2",
};

static const char* const ippfx4_bad[] = {
  "255255255255",
  "a.b.c.d",
  "1:2:3:4",
  "1,2,3,4",
  "1.2.3.",
  ".1.2.3",
  "256.256.256.256",
  "255.255.255.255/008",
  "1.2.3.256",
  "256.1.2.3",
  "25..2.3",
  "bogus",
  "deadbeef",
  // ATTN: accepted!: "18.52.86.120/33",
  // ATTN: accepted!: "18.52.86.120/222",
  // ATTN: accepted!: "1234:5678::123:56:9:159d/129",
};

TEST(RwCType, Ipv4Pref)
{
  TEST_DESCRIPTION("Test Ipv4Prefix ctypes");
  test_good_text_field("ippfx4", ippfx4, ippfx4_canonical, sizeof(ippfx4)/sizeof(ippfx4[0]));
  test_bad_text_field("ippfx4", ippfx4_bad, sizeof(ippfx4_bad)/sizeof(ippfx4_bad[0]));
}

static const char* const ippfx6[] = {
  "1234:5678:0000:0000:0123:0056:0009:159d/128",
  "0000:0000:0000:0000:0000:0000:0000:0000/127",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/001",
  "::/0",
  "::1/0x80",
  "1234:5678:0:0:123:56:9:159d/82",
  "1234:5678::123:56:9:159d/28",
  "1::2345/18",
  "::ffff:10.10.10.10/033",
  "0000:0000:0000:0000:0000:ffff:255.255.255.255/32",
};

static const char* const ippfx6_canonical[] = {
  "1234:5678::123:56:9:159d/128",
  "::/127",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/1",
  "::/0",
  "::1/128",
  "1234:5678::123:56:9:159d/82",
  "1234:5678::123:56:9:159d/28",
  "1::2345/18",
  "::ffff:10.10.10.10/27",
  "::ffff:255.255.255.255/32",
};

static const char* const ippfx6_bad[] = {
  "1234:5678:0000:0000:0123:0056:0009:159g",
  "1234:5678:0000:0000:0123:0056:0009",
  "0000:0000:0000:0000:fffff:0000:0000:0000",
  "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
  "ffff::ffff:ffff:ffff::ffff:ffff",
  ":::",
  "i:j:k:l:m:n:o:p",
  "bogus",
  "deadbeef",
  // ATTN: accepted!: "18.52.86.120/33",
  // ATTN: accepted!: "18.52.86.120/222",
  // ATTN: accepted!: "1234:5678::123:56:9:159d/129",
};

TEST(RwCType, Ipv6Pref)
{
  TEST_DESCRIPTION("Test Ip6Prefix ctypes");
  test_good_text_field("ippfx6", ippfx6, ippfx6_canonical, sizeof(ippfx6)/sizeof(ippfx6[0]));
  test_bad_text_field("ippfx6", ippfx6_bad, sizeof(ippfx6_bad)/sizeof(ippfx6_bad[0]));
}

static const char* const mac[] = {
  "00:00:00:00:00:00",
  "99:99:99:99:99:99",
  "aa:aa:aa:aa:aa:aa",
  "AA:AA:AA:AA:AA:AA",
  "ff:ff:ff:ff:ff:ff",
  "FF:FF:FF:FF:FF:FF",
  "12:34:45:00:ab:CD",
  "0:0:0:0:0:0",
  "F:F:F:F:F:F",
  "2:4:5:7:b:D",
};

static const char* const mac_canonical[] = {
  "00:00:00:00:00:00",
  "99:99:99:99:99:99",
  "aa:aa:aa:aa:aa:aa",
  "aa:aa:aa:aa:aa:aa",
  "ff:ff:ff:ff:ff:ff",
  "ff:ff:ff:ff:ff:ff",
  "12:34:45:00:ab:cd",
  "00:00:00:00:00:00",
  "0f:0f:0f:0f:0f:0f",
  "02:04:05:07:0b:0d",
};

static const char* const mac_bad[] = {
  "00:00:00:00:00:",
  ":99:99:99:99:99",
  "aaaaaaaaaaaa",
  "AAA:AA:AA:AA:AA:AA",
  "ff:ff:ff:ff:ff:gg",
  "123:34:45:67:ab:CD",
  "0::0:0:0:0",
  "G:F:F:F:F:F",
  // ATTN: Accepted!: "2:4:5:7:b:DDD",
};

TEST(RwCType, Mac)
{
  TEST_DESCRIPTION("Test MAC ctypes");
  test_good_text_field("mac", mac, mac_canonical, sizeof(mac)/sizeof(mac[0]));
  test_bad_text_field("mac", mac_bad, sizeof(mac_bad)/sizeof(mac_bad[0]));
}


/*****************************************************************************/

static void test_binary(
  const char* field_name,
  ProtobufCMessage* msg)
{
  // auto-clean-up
  UniquePtrProtobufCMessageUseBody<>::uptr_t msg_uptr(msg, &ctype_pbc_instance);

  static const uint8_t bin_data1[] = { 1, 0, 255, 254, 0, 40, 92, 92 };
  static const uint8_t bin_data2[] = { 1, 45, 255, 254, 1, 40, 0, 255 };
  const ProtobufCFieldDescriptor* fd
    = protobuf_c_message_descriptor_get_field_by_name(msg->descriptor, field_name);
  ASSERT_TRUE(fd);
  bool ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, msg, fd, (const char*)bin_data1, sizeof(bin_data1));
  EXPECT_TRUE(ok);

  size_t expected_n = 1;
  if (PROTOBUF_C_LABEL_REPEATED == fd->label) {
    ok = protobuf_c_message_set_field_text_value(&ctype_pbc_instance, msg, fd, (const char*)bin_data2, sizeof(bin_data2));
    EXPECT_TRUE(ok);
    expected_n = 2;
  }

  ok = protobuf_c_message_check(&ctype_pbc_instance, msg);
  EXPECT_TRUE(ok);

  uint8_t buf[4096];
  size_t packed = protobuf_c_message_pack(&ctype_pbc_instance, msg, buf);
  ASSERT_GT(packed, 12);

  UniquePtrProtobufCMessage<>::uptr_t umsg_uptr(
    protobuf_c_message_unpack(&ctype_pbc_instance, msg->descriptor, packed, buf),
    &ctype_pbc_instance);
  ASSERT_TRUE(umsg_uptr.get());
  ok = protobuf_c_message_is_equal_deep(&ctype_pbc_instance, msg, umsg_uptr.get());
  EXPECT_TRUE(ok);

  const ProtobufCFieldDescriptor* newfd = NULL;
  void* value = NULL;
  size_t offset = 0;
  protobuf_c_boolean array_of_ptrs = FALSE;
  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
    umsg_uptr.get(), fd->id, &newfd, &value, &offset, &array_of_ptrs);
  EXPECT_EQ(count, expected_n);
  EXPECT_EQ(fd, newfd);
  EXPECT_NE(value, nullptr);

  umsg_uptr.reset();
  msg_uptr.reset();
}

TEST(RwCType, Binary)
{
  TEST_DESCRIPTION("Test binary ctype");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList,  cbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList,   cfll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeaf, nbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeaf,  nff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeafList, nbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeafList,  nfll);
  ProtobufCMessage* msgs[] = {
    &cbf.base, &cff.base, &cbll.base, &cfll.base,
    &nbf.base, &nff.base, &nbll.base, &nfll.base,
  };

  ctype_clear_call_counters();
  for (auto& msg: msgs) {
    test_binary("bin", msg);
    EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
  }

  // factor of 6 is: 2 optional fields, plus 2 arrays of 2 elements
  EXPECT_EQ(ctype_calls_to_get_packed_size, 5*6); // 3x for packs
  EXPECT_EQ(ctype_calls_to_pack, 3*6); // 1x for dup, 2x for is_equal_deep
  EXPECT_EQ(ctype_calls_to_unpack, 2*6); // 1x: from string, 1x: real unpack
  EXPECT_EQ(ctype_calls_to_to_string, 0);
  EXPECT_EQ(ctype_calls_to_from_string, 1*6);
  EXPECT_EQ(ctype_calls_to_init_usebody, 2*6); // from set_field, set_field_text
  EXPECT_EQ(ctype_calls_to_free_usebody, (2*6)+6); // 2 messages, temp ctype inside set_text func
  EXPECT_EQ(ctype_calls_to_check, 6); // 1 req, 1 op, 2 bumpylist, 2 flat list
  EXPECT_EQ(ctype_calls_to_deep_copy, 6); // from set_field
  EXPECT_EQ(ctype_calls_to_compare, 0);
}

TEST(RwCType, Dynalloc)
{
  TEST_DESCRIPTION("Test dynalloc ctype");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList,  cbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList,   cfll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeaf, nbf);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeaf,  nff);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeBumpyLeafList, nbll);
  RWPB_M_MSG_DECL_INIT(CtypeTest_NativeFlatLeafList,  nfll);
  ProtobufCMessage* msgs[] = {
    &cbf.base, &cff.base, &cbll.base, &cfll.base,
    &nbf.base, &nff.base, &nbll.base, &nfll.base,
  };

  ctype_clear_call_counters();
  for (auto& msg: msgs) {
    test_binary("dyn", msg);
    EXPECT_EQ(ctype_calls_to_alloc, ctype_calls_to_free);
  }

  EXPECT_EQ(ctype_calls_to_get_packed_size, 5*6); // 3x for packs
  EXPECT_EQ(ctype_calls_to_pack, 3*6); // 1x for dup, 2x for is_equal_deep
  EXPECT_EQ(ctype_calls_to_unpack, 2*6); // 1x: from string, 1x: real unpack
  EXPECT_EQ(ctype_calls_to_to_string, 0);
  EXPECT_EQ(ctype_calls_to_from_string, 1*6);
  EXPECT_EQ(ctype_calls_to_init_usebody, (1*6)+6); // temp ctype inside set_text
  EXPECT_EQ(ctype_calls_to_free_usebody, (2*6)+6); // 2 messages, temp ctype inside set_text
  EXPECT_EQ(ctype_calls_to_check, 6); // 1 req , 1 opt, 2 flat, 2 bumpy
  EXPECT_EQ(ctype_calls_to_deep_copy, 6); // inside set_field
  EXPECT_EQ(ctype_calls_to_compare, 0);
}

static void
set_ctype_field(ProtobufCMessage *msg,
                    const char *fname,
                    const char* from_str,
                    size_t from_len = 0)
{
  if (!from_len) {
    from_len = strlen(from_str);
  }
  const ProtobufCFieldDescriptor* fd
      = protobuf_c_message_descriptor_get_field_by_name(msg->descriptor, fname);
  ASSERT_TRUE(fd);

  bool ok = protobuf_c_message_set_field_text_value(NULL, msg, fd, from_str, from_len);
  EXPECT_TRUE(ok);
}

static void
set_ctype_field_listy(ProtobufCMessage *msg,
                      const char *fname,
                      const char* const* from_str_values,
                      size_t n_values)
{
  const ProtobufCFieldDescriptor* fd
      = protobuf_c_message_descriptor_get_field_by_name(msg->descriptor, fname);
  ASSERT_TRUE(fd);

  for (unsigned i = 0; i < n_values; ++i) {
    const char* from_str = from_str_values[i];
    size_t from_len = 0;
    if (fd->type == PROTOBUF_C_TYPE_STRING) {
      from_len = strlen(from_str);
    } else {
      from_len = 4; // ATTN: Assumes bytes field is always exactly 4 bytes long
    }
    bool ok = protobuf_c_message_set_field_text_value(NULL, msg, fd, from_str, from_len);
    EXPECT_TRUE(ok);
  }
}

static void
test_merge_ctype(ProtobufCMessage *earlier,
                 ProtobufCMessage *latter,
                 const char *fname,
                 bool reuse_e,
                 bool present_l,
                 const char *fvalue = NULL,
                 size_t flen = 0)
{
  const ProtobufCFieldDescriptor* fd
      = protobuf_c_message_descriptor_get_field_by_name(earlier->descriptor, fname);
  ASSERT_TRUE(fd);

  void* value_e = NULL, *value_l = NULL;
  size_t offset = 0;
  protobuf_c_boolean array_of_ptrs = FALSE;

  size_t count_e = protobuf_c_message_get_field_count_and_offset(earlier,
      fd, &value_e, &offset, &array_of_ptrs);

  size_t count_l = protobuf_c_message_get_field_count_and_offset(latter,
      fd, &value_l, &offset, &array_of_ptrs);

  EXPECT_EQ(count_l, 1);

  if (reuse_e) {
    if (!(fd->rw_flags & RW_PROTOBUF_FOPT_INLINE)) {
      EXPECT_FALSE(count_e);
    }
    char fieldv[1024];
    size_t len = 1024;

    ASSERT_TRUE(protobuf_c_field_get_text_value(NULL, fd, fieldv, &len, value_l));

    if (fd->type == PROTOBUF_C_TYPE_STRING) {
      EXPECT_STREQ(fieldv, fvalue);
    } else if (fd->type == PROTOBUF_C_TYPE_BYTES) {
      EXPECT_EQ(flen, len);
    }

  } else {
    EXPECT_EQ(count_e, 1);
    if (present_l) {
      EXPECT_FALSE(fd->ctype->compare(fd->ctype, fd, value_e, value_l));
    } else {
      EXPECT_TRUE(fd->ctype->compare(fd->ctype, fd, value_e, value_l));
    }
  }
}

static void
test_merge_ctype_listy(ProtobufCMessage *earlier,
                       ProtobufCMessage *latter,
                       const char *fname,
                       bool reuse_e,
                       bool present_l,
                       const char* const* to_str_values,
                       size_t ne_values,
                       size_t nl_values = 0)
{
  const ProtobufCFieldDescriptor* fd
      = protobuf_c_message_descriptor_get_field_by_name(earlier->descriptor, fname);
  ASSERT_TRUE(fd);

  size_t n_values = ne_values + nl_values;

  void* value_e = NULL, *value_l = NULL;
  size_t offset = 0;
  protobuf_c_boolean array_of_ptrs = FALSE;

  size_t count_e = protobuf_c_message_get_field_count_and_offset(earlier,
                                    fd, &value_e, &offset, &array_of_ptrs);

  size_t count_l = protobuf_c_message_get_field_count_and_offset(latter,
                                   fd, &value_l, &offset, &array_of_ptrs);

  EXPECT_EQ(count_l, n_values);
  bool is_inline = !!(fd->rw_flags & RW_PROTOBUF_FOPT_INLINE);

  if (reuse_e) {
    EXPECT_FALSE(count_e);
  } else {
    EXPECT_EQ(count_e, ne_values);
  }

  for (unsigned i = 0; i < ne_values; i++) {

    char fieldv[1024];
    size_t len = 1024;
    void *elem_l = NULL, *elem_e = NULL;

    if (is_inline) {
      elem_l = (uint8_t *)value_l + (i * fd->data_size);
    } else {
      elem_l = *((uint8_t **)value_l + i) ;
    }

    if (reuse_e) {
      ASSERT_TRUE(protobuf_c_field_get_text_value(NULL, fd, fieldv, &len, elem_l));

      if (fd->type == PROTOBUF_C_TYPE_STRING) {
        EXPECT_STREQ(fieldv, to_str_values[i]);
      } else {
        EXPECT_EQ(len, 4);
      }
    } else {

      if (is_inline) {
        elem_e = (uint8_t *)value_e + (i * fd->data_size);
      } else {
        elem_e = *((uint8_t **)value_e + i) ;
      }

      if (present_l) {
        EXPECT_FALSE(fd->ctype->compare(fd->ctype, fd, elem_e, elem_l));
      } else {
        EXPECT_TRUE(fd->ctype->compare(fd->ctype, fd, elem_e, elem_l));
      }
    }
  }

  size_t index = 0;
  for (unsigned i = ne_values; i < count_l; i++, index++) {
    char fieldv[1024];
    size_t len = 1024;
    void *elem_l = NULL;

    if (is_inline) {
      elem_l = (uint8_t *)value_l + (i * fd->data_size);
    } else {
      elem_l = *((uint8_t **)value_l + i) ;
    }

    ASSERT_TRUE(protobuf_c_field_get_text_value(NULL, fd, fieldv, &len, elem_l));

    if (fd->type == PROTOBUF_C_TYPE_STRING) {
      EXPECT_STREQ(fieldv, to_str_values[index]);
    } else {
      EXPECT_EQ(len, 4);
    }
  }
}

static ProtobufCMessage*
pack_merge_and_unpack_msgs(ProtobufCMessage* msg1,
                           ProtobufCMessage* msg2)
{
  size_t size1 = 0;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, msg1, &size1);

  size_t size2 = 0;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, msg2, &size2);

  uint8_t res[size1+size2];
  memcpy(res, data1, size1);
  memcpy(res+size1, data2, size2);

  ProtobufCMessage* out = protobuf_c_message_unpack(NULL, msg1->descriptor, size1+size2, res);
  return out;
}

static void
test_ctypes_merge_non_listy(ProtobufCMessage *msg1,
                            ProtobufCMessage *msg2)
{
  set_ctype_field(msg1, "ip", ip4[1]);
  set_ctype_field(msg1, "ip4", ip4[2]);
  set_ctype_field(msg1, "ippfx", ippfx[3]);
  set_ctype_field(msg1, "mac", mac[0]);
  set_ctype_field(msg1, "cid", "0:12345:0");

  static uint8_t bin_data1[] = { 1, 0, 255, 254, 0, 40, 92, 92 };
  set_ctype_field(msg1, "bin", (char *)bin_data1, sizeof(bin_data1));

  static uint8_t bin_data2[] = { 1, 45, 255, 254, 1, 40, 0, 255 };
  set_ctype_field(msg1, "dyn", (char *)bin_data2, sizeof(bin_data2));

  auto out = pack_merge_and_unpack_msgs(msg1, msg2);
  ASSERT_TRUE(out);

  test_merge_ctype(msg1, out, "ip", false, true, ip4_canonical[1]);
  test_merge_ctype(msg1, out, "ip4", false, true, ip4_canonical[2]);
  test_merge_ctype(msg1, out, "ippfx", false, true, ippref_canonical[3]);
  test_merge_ctype(msg1, out, "mac", false, true, mac_canonical[0]);
  test_merge_ctype(msg1, out, "cid", false, true, "0:12345:0");
  test_merge_ctype(msg1, out, "bin", false, true, (char *)bin_data1, sizeof(bin_data1));
  test_merge_ctype(msg1, out, "dyn", false, true, (char *)bin_data2, sizeof(bin_data2));
}

static void
test_ctypes_merge_new_non_listy(ProtobufCMessage *msg1,
                                ProtobufCMessage *msg2)
{
  set_ctype_field(msg1, "ip", ip4[1]);
  set_ctype_field(msg1, "ip4", ip4[2]);
  set_ctype_field(msg1, "ippfx", ippfx[3]);
  set_ctype_field(msg1, "mac", mac[0]);
  set_ctype_field(msg1, "cid", "0:12345:0");

  static uint8_t bin_data1[] = { 1, 0, 255, 254, 0, 40, 92, 92 };
  set_ctype_field(msg1, "bin", (char *)bin_data1, sizeof(bin_data1));

  static uint8_t bin_data2[] = { 1, 45, 255, 254, 1, 40, 0, 255 };
  set_ctype_field(msg1, "dyn", (char *)bin_data2, sizeof(bin_data2));

  auto ret = protobuf_c_message_merge_new(NULL, msg1, msg2);
  ASSERT_TRUE(ret);

  test_merge_ctype(msg1, msg2, "ip", false, true);
  test_merge_ctype(msg1, msg2, "ip4", false, true);
  test_merge_ctype(msg1, msg2, "ippfx", false, true);
  test_merge_ctype(msg1, msg2, "mac", false, true);
  test_merge_ctype(msg1, msg2, "cid", false, true);
  test_merge_ctype(msg1, msg2, "bin", false, true);
  test_merge_ctype(msg1, msg2, "dyn", false, true);

  protobuf_c_message_free_unpacked_usebody(NULL, msg2);
  protobuf_c_message_init(msg1->descriptor, msg2);

  set_ctype_field(msg2, "ip", ip4[4]);
  set_ctype_field(msg2, "ip4", ip4[3]);
  set_ctype_field(msg2, "ippfx", ippfx[1]);
  set_ctype_field(msg2, "mac", mac[1]);
  set_ctype_field(msg2, "cid", "1:12345:0");

  static uint8_t bin_data3[] = { 1, 45, 255, 253 };
  set_ctype_field(msg2, "bin", (char *)bin_data3, sizeof(bin_data3));

  static uint8_t bin_data4[] = { 1, 45, 14, 31, 8 };
  set_ctype_field(msg2, "dyn", (char *)bin_data4, sizeof(bin_data4));

  ret = protobuf_c_message_merge_new(NULL, msg1, msg2);
  ASSERT_TRUE(ret);

  test_merge_ctype(msg1, msg2, "ip", false, false);
  test_merge_ctype(msg1, msg2, "ip4", false, false);
  test_merge_ctype(msg1, msg2, "ippfx", false, false);
  test_merge_ctype(msg1, msg2, "mac", false, false);
  test_merge_ctype(msg1, msg2, "cid", false, false);
  test_merge_ctype(msg1, msg2, "bin", false, false);
  test_merge_ctype(msg1, msg2, "dyn", false, false);
}

static void
test_ctypes_merge_listy(ProtobufCMessage *msg1,
                        ProtobufCMessage *msg2)
{
  const char* callids[] = { "1:1:0", "1:2:0", "1:3:0"};

  set_ctype_field_listy(msg1, "ip", ip4, 2);
  set_ctype_field_listy(msg1, "ip4", ip4, 2);
  set_ctype_field_listy(msg1, "ippfx", ippfx, 2);
  set_ctype_field_listy(msg1, "mac", mac, 3);
  set_ctype_field_listy(msg1, "cid", callids, 3);

  const uint8_t bd11[] = {0, 1, 2, 3};
  const uint8_t bd12[] = {10, 20, 245, 67};
  const uint8_t *bin_data1[] = { bd11, bd12};
  set_ctype_field_listy(msg1, "bin", (char **)bin_data1, 2);

  const uint8_t bd21[] = {0, 9, 2, 3};
  const uint8_t bd22[] = {160, 20, 245, 67};
  const uint8_t *bin_data2[] = { bd21, bd22 };
  set_ctype_field_listy(msg1, "dyn", (char **)bin_data2, 2);

  set_ctype_field_listy(msg2, "ip", ip4, 3);

  auto out = pack_merge_and_unpack_msgs(msg1, msg2);
  ASSERT_TRUE(out);

  test_merge_ctype_listy(msg1, out, "ip", false, true, ip4_canonical, 2, 3);
  test_merge_ctype_listy(msg1, out, "ip4", false, true, ip4_canonical, 2);
  test_merge_ctype_listy(msg1, out, "ippfx", false, true, ippref_canonical, 2);
  test_merge_ctype_listy(msg1, out, "mac", false, true, mac_canonical, 3);
  test_merge_ctype_listy(msg1, out, "cid", false, true, callids, 3);
  test_merge_ctype_listy(msg1, out, "bin", false, true, (char**)bin_data1, 2);
  test_merge_ctype_listy(msg1, out, "dyn", false, true, (char**)bin_data2, 2);
}

static void
test_ctypes_merge_new_listy(ProtobufCMessage *msg1,
                            ProtobufCMessage *msg2)
{
  const char* callids[] = { "1:1:0", "1:2:0", "1:3:0"};

  set_ctype_field_listy(msg1, "ip", ip4, 2);
  set_ctype_field_listy(msg1, "ip4", ip4, 2);
  set_ctype_field_listy(msg1, "ippfx", ippfx, 2);
  set_ctype_field_listy(msg1, "mac", mac, 3);
  set_ctype_field_listy(msg1, "cid", callids, 3);

  const uint8_t bd11[] = {0, 1, 2, 3};
  const uint8_t bd12[] = {100, 200, 245, 67};
  const uint8_t *bin_data1[] = { bd11, bd12};
  set_ctype_field_listy(msg1, "bin", (char **)bin_data1, 2);

  const uint8_t bd21[] = {7, 1, 2, 3};
  const uint8_t bd22[] = {100, 20, 245, 67};
  const uint8_t *bin_data2[] = { bd21, bd22 };
  set_ctype_field_listy(msg1, "dyn", (char **)bin_data2, 2);

  auto ret = protobuf_c_message_merge_new(NULL, msg1, msg2);
  ASSERT_TRUE(ret);

  test_merge_ctype_listy(msg1, msg2, "ip", false, true, ip4_canonical, 2);
  test_merge_ctype_listy(msg1, msg2, "ip4", false, true, ip4_canonical, 2);
  test_merge_ctype_listy(msg1, msg2, "ippfx", false, true, ippref_canonical, 2);
  test_merge_ctype_listy(msg1, msg2, "mac", false, true, mac_canonical, 3);
  test_merge_ctype_listy(msg1, msg2, "cid", false, true, callids, 3);
  test_merge_ctype_listy(msg1, msg2, "bin", false, true, (char**)bin_data1, 2);
  test_merge_ctype_listy(msg1, msg2, "dyn", false, true, (char**)bin_data2, 2);

  protobuf_c_message_free_unpacked_usebody(NULL, msg2);
  protobuf_c_message_init(msg1->descriptor, msg2);

  set_ctype_field_listy(msg2, "ip", ip4, 3);
  set_ctype_field_listy(msg2, "ippfx", ippfx, 3);

  const uint8_t bd31[] = {17, 11, 21, 31};
  const uint8_t bd32[] = {100, 20, 245, 67};
  const uint8_t *bin_data3[] = { bd31, bd32 };
  set_ctype_field_listy(msg2, "dyn", (char **)bin_data3, 2);

  ret = protobuf_c_message_merge_new(NULL, msg1, msg2);
  ASSERT_TRUE(ret);

  test_merge_ctype_listy(msg1, msg2, "ip", false, true, ip4_canonical, 2, 3);
  test_merge_ctype_listy(msg1, msg2, "ip4", false, true, ip4_canonical, 2);
  test_merge_ctype_listy(msg1, msg2, "ippfx", false, true, ippref_canonical, 2, 3);
  test_merge_ctype_listy(msg1, msg2, "mac", false, true, mac_canonical, 3);
  test_merge_ctype_listy(msg1, msg2, "cid", false, true, callids, 3);
  test_merge_ctype_listy(msg1, msg2, "bin", false, true, (char**)bin_data1, 2);
  test_merge_ctype_listy(msg1, msg2, "dyn", false, true, (char**)bin_data2, 2, 2);
}

TEST(RwCType, MergeBL)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf, cbl1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf, cbl2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cbl1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cbl2.base);

  test_ctypes_merge_non_listy(&cbl1.base, &cbl2.base);
}

TEST(RwCType, MergeBL1)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf, cbl1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf, cbl2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cbl1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cbl2.base);

  test_ctypes_merge_new_non_listy(&cbl1.base, &cbl2.base);
}

TEST(RwCType, MergeFL)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf, cfl1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf, cfl2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cfl1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cfl2.base);

  test_ctypes_merge_non_listy(&cfl1.base, &cfl2.base);
}

TEST(RwCType, MergeFL1)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf, cfl1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf, cfl2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cfl1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cfl2.base);

  test_ctypes_merge_new_non_listy(&cfl1.base, &cfl2.base);
}

TEST(RwCType, MergeBLL)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList, cbll1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList, cbll2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cbll1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cbll2.base);

  test_ctypes_merge_listy(&cbll1.base, &cbll2.base);
}

TEST(RwCType, MergeBLL1)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList, cbll1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList, cbll2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cbll1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cbll2.base);

  test_ctypes_merge_new_listy(&cbll1.base, &cbll2.base);
}

TEST(RwCType, MergeFLL)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList, cfll1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList, cfll2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cfll1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cfll2.base);

  test_ctypes_merge_listy(&cfll1.base, &cfll2.base);
}

TEST(RwCType, MergeFLL1)
{
  TEST_DESCRIPTION("Test Ctype Merge");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList, cfll1);
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList, cfll2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&cfll1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&cfll2.base);

  test_ctypes_merge_new_listy(&cfll1.base, &cfll2.base);

  protobuf_c_message_free_unpacked_usebody(NULL, &cfll2.base);
  RWPB_F_MSG_INIT(CtypeTest_CTypeFlatLeafList, &cfll2);

  set_ctype_field_listy(&cfll2.base, "ip", ip4, 4);
  auto ret = protobuf_c_message_merge_new(NULL, &cfll1.base, &cfll2.base);
  ASSERT_FALSE(ret);
}

TEST(RwCType, ErrorMessages)
{
  TEST_DESCRIPTION("Test Error Messages");
  extern unsigned ctype_calls_to_error;
  unsigned tmp_ctype_calls_to_error = ctype_calls_to_error;
  const char* field_name = "mac";
  bool ok;
  ctype_clear_call_counters();

  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf,  cbf);
  //RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf,   cff);

  ProtobufCMessage *msg = &cbf.base;
  UniquePtrProtobufCMessageUseBody<>::uptr_t msg_uptr(msg, &ctype_pbc_instance);

  ProtobufCMessage* outer_msg = msg;
  ProtobufCMessage* inner_msg = msg;
  const ProtobufCFieldDescriptor* ifd
        = protobuf_c_message_descriptor_get_field_by_name(outer_msg->descriptor, field_name);
  ASSERT_TRUE(ifd);
  const ProtobufCFieldDescriptor* ofd = ifd;

  inner_msg = nullptr;
  ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, outer_msg, ofd, &inner_msg);
  EXPECT_FALSE(ok);
  EXPECT_EQ(ctype_calls_to_error, tmp_ctype_calls_to_error+1);

  inner_msg = nullptr;
  ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, NULL, ofd, &inner_msg);
  EXPECT_FALSE(ok);
  EXPECT_EQ(ctype_calls_to_error, tmp_ctype_calls_to_error+2);

#if 0
  inner_msg = nullptr;
  ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, outer_msg, NULL, &inner_msg);
  EXPECT_FALSE(ok);
  EXPECT_EQ(ctype_calls_to_error, tmp_ctype_calls_to_error+3);

  inner_msg = nullptr;
  ok = protobuf_c_message_set_field_message(&ctype_pbc_instance, NULL, NULL, &inner_msg);
  EXPECT_FALSE(ok);
  EXPECT_EQ(ctype_calls_to_error, tmp_ctype_calls_to_error+4);
#endif
}

static void test_ctypes_init_leafy(
  ProtobufCMessage *msg)
{
  set_ctype_field(msg, "ip", ip4[1]);
  set_ctype_field(msg, "ip4", ip4[2]);
  set_ctype_field(msg, "ip6", ip6[5]);
  set_ctype_field(msg, "ippfx", ippfx[3]);
  set_ctype_field(msg, "ippfx4", ippfx4[4]);
  set_ctype_field(msg, "ippfx6", ippfx6[6]);
  set_ctype_field(msg, "mac", mac[6]);
  set_ctype_field(msg, "cid", "0:12345:6");

  static uint8_t bin_data1[] = { 145, 0, 25, 54, 0, 255 };
  set_ctype_field(msg, "bin", (char *)bin_data1, sizeof(bin_data1));

  static uint8_t bin_data2[] = { 1, 45, 255, 254, 1, 40, 0, 255 };
  set_ctype_field(msg, "dyn", (char *)bin_data2, sizeof(bin_data2));
}

static void test_ctypes_init_leaflisty(
  ProtobufCMessage *msg)
{
  const char* callids[] = { "2:1:0", "1:2:0", "1:34567:0"};

  set_ctype_field_listy(msg, "ip", ip4, 2);
  set_ctype_field_listy(msg, "ip4", ip4, 2);
  set_ctype_field_listy(msg, "ip6", ip6, 2);
  set_ctype_field_listy(msg, "ippfx", ippfx, 2);
  set_ctype_field_listy(msg, "ippfx4", ippfx4, 2);
  set_ctype_field_listy(msg, "ippfx6", ippfx6, 2);
  set_ctype_field_listy(msg, "mac", mac, 2);
  set_ctype_field_listy(msg, "cid", callids, 2);

  const uint8_t bd11[] = {0, 1, 2, 3};
  const uint8_t bd12[] = {10, 20, 245, 67};
  const uint8_t *bin_data1[] = { bd11, bd12};
  set_ctype_field_listy(msg, "bin", (char **)bin_data1, 2);

  const uint8_t bd21[] = {0, 9, 2, 3};
  const uint8_t bd22[] = {160, 20, 245, 67};
  const uint8_t *bin_data2[] = { bd21, bd22 };
  set_ctype_field_listy(msg, "dyn", (char **)bin_data2, 2);
}

static void test_ctypes_xml_round_trip(
  ProtobufCMessage *pb1)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* module = model->load_module("ctype-test");
  ASSERT_TRUE(module);

  // Convert PB to DOM
  rw_yang_netconf_op_status_t ncrs;
  XMLDocument::uptr_t dom1(mgr->create_document_from_pbcm(pb1, ncrs));
  EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_TRUE(dom1.get());

  // Convert DOM to XML
  std::string xml1 = dom1->to_string();
  ASSERT_TRUE(xml1.length());

  // Convert XML back into DOM
  std::string error_out;
  XMLDocument::uptr_t dom2(mgr->create_document_from_string(xml1.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom2.get());

  // Convert the DOM back into PB.
  UniquePtrProtobufCMessage<>::uptr_t pb2(
    protobuf_c_message_create( NULL, pb1->descriptor ));
  ASSERT_TRUE(pb2.get());
  ncrs = dom2->to_pbcm( pb2.get() );
  EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  EXPECT_TRUE(protobuf_c_message_is_equal_deep( NULL, pb1, pb2.get() ));
}

TEST(RwCType, XmlRoundTripBL)
{
  TEST_DESCRIPTION("Test Ctype XML round trip bumpy leaf");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeaf, cbl);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&cbl.base);
  test_ctypes_init_leafy(&cbl.base);
  test_ctypes_xml_round_trip(&cbl.base);
}

TEST(RwCType, XmlRoundTripFL)
{
  TEST_DESCRIPTION("Test Ctype XML round trip flat leaf");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeaf, cfl);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&cfl.base);
  test_ctypes_init_leafy(&cfl.base);
  test_ctypes_xml_round_trip(&cfl.base);
}

TEST(RwCType, XmlRoundTripBLL)
{
  TEST_DESCRIPTION("Test Ctype XML round trip bumpy leaf list");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeBumpyLeafList, cbll);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&cbll.base);
  test_ctypes_init_leaflisty(&cbll.base);
  test_ctypes_xml_round_trip(&cbll.base);
}

TEST(RwCType, XmlRoundTripFLL)
{
  TEST_DESCRIPTION("Test Ctype XML round trip flat leaf list");
  RWPB_M_MSG_DECL_INIT(CtypeTest_CTypeFlatLeafList, cfll);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&cfll.base);
  test_ctypes_init_leaflisty(&cfll.base);
  test_ctypes_xml_round_trip(&cfll.base);
}
