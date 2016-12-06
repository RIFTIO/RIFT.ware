
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
 * @file pb2c_test.cpp
 * @brief Google test cases for testing Protoc compiler and PB<->C conversions
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include "rwut.h"
#include "rwlib.h"
#include "test-yang-desc.pb-c.h"
#include "enum.pb-c.h"
#include "pbcopy.pb-c.h"
#include "pbdf.pb-c.h"
#include <string.h>
#include <stdio.h>
#include "company.pb-c.h"
#include "testy2p-top1.pb-c.h"
#include "testy2p-top2.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rwmsg-data-d.pb-c.h"
#include "oneof_test.pb-c.h"
#include "test-tag-generation-base.pb-c.h"
#include "test-tag-generation.pb-c.h"
#include "test-ydom-top.pb-c.h"
#include "is_equal.pb-c.h"
#include "rw-fpath-data-e.pb-c.h"
#include "rw-pbc-stats.pb-c.h"
#include "test-ydom-top.pb-c.h"

static unsigned n_alloc = 0;
static void* pb2c_alloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  ++n_alloc;
  return malloc(size);
}

static void* pb2c_zalloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  ++n_alloc;
  return calloc(size,1);
}

static void* pb2c_realloc(ProtobufCInstance* instance, void* data, size_t size)
{
  (void)instance;
  if (data) {
    --n_alloc;
  }
  data = realloc(data, size);
  if (data) {
    ++n_alloc;
  }
  return data;
}

static void pb2c_free(ProtobufCInstance* instance, void *data)
{
  (void)instance;
  --n_alloc;
  free(data);
}

static void pb2c_error(ProtobufCInstance* instance, const char* errmsg)
{
  (void)instance;
  fprintf(stderr, "%s", errmsg);
}

ProtobufCInstance pb2c_pbc_instance = {
  .alloc = &pb2c_alloc,
  .zalloc = &pb2c_zalloc,
  .realloc = &pb2c_realloc,
  .free = &pb2c_free,
  .error = &pb2c_error,
  .data = NULL,
};

static char pb2c_errorbuf[4096];
int pb2c_errorbuf_n;
static void pb2c_error_to_buf(ProtobufCInstance* instance, const char* errmsg)
{
  (void)instance;
  int s = sizeof(pb2c_errorbuf)-pb2c_errorbuf_n;
  int bytes = snprintf(pb2c_errorbuf+pb2c_errorbuf_n, s, "%s", errmsg);
  if (bytes > s) {
    pb2c_errorbuf_n = sizeof(pb2c_errorbuf);
  } else if (bytes > 0) {
    pb2c_errorbuf_n += bytes;
  }
}

static void pb2c_errorbuf_clear()
{
  memset(pb2c_errorbuf, 0, sizeof(pb2c_errorbuf));
  pb2c_errorbuf_n = 0;
}

ProtobufCInstance pb2c_pbc_instance_errorbuf = {
  .alloc = &pb2c_alloc,
  .zalloc = &pb2c_zalloc,
  .realloc = &pb2c_realloc,
  .free = &pb2c_free,
  .error = &pb2c_error_to_buf,
  .data = NULL,
};

static ProtobufCInstance* pinstance = nullptr;


TEST(RWPb2C, YangDesc)
{
  TEST_DESCRIPTION("This tests the generation of the RW Helper structure in the protoc generated message descriptor");

  RWPB_M_MSG_DECL_INIT(TestYangDesc_Top1,top1);
  ASSERT_NE(top1.base.descriptor, nullptr);
  EXPECT_EQ((const void*)top1.base.descriptor->ypbc_mdesc,
            (const void*)RWPB_G_MSG_YPBCD(TestYangDesc_data_Top1));

  RWPB_M_MSG_DECL_INIT(TestYangDesc_Top2,top2);
  ASSERT_NE(top2.base.descriptor, nullptr);
  EXPECT_EQ((const void*)top2.base.descriptor->ypbc_mdesc,
            (const void*)RWPB_G_MSG_YPBCD(TestYangDesc_data_Top2));
}

TEST(RWPb2C, BasicPBEnumTest)
{
  TEST_DESCRIPTION("Test conversion of Enums at various levels");

  EnumTest__OuterMessage1 msg1;
  EnumTest__OuterMessage2 msg2;

  enum_test__outer_message1__init(&msg1);
  msg1.outer_e = VALUE1;
  msg1.has_outer_e = TRUE;

  msg1.has_inner_e = TRUE;

  msg1.inner_e = ENUM_TEST__OUTER_MESSAGE1__INNER_ENUM1__VALUE3;

  EXPECT_EQ(msg1.has_outer_e, TRUE);
  EXPECT_EQ(msg1.outer_e, VALUE1);

  EXPECT_EQ(msg1.has_inner_e, TRUE);
  EXPECT_EQ(msg1.inner_e, ENUM_TEST__OUTER_MESSAGE1__INNER_ENUM1__VALUE3);

  enum_test__outer_message2__init(&msg2);
  EXPECT_EQ(msg2.outer_e_req, VALUE3);

  EXPECT_EQ(msg2.outer_e_opt, VALUE3);
  EXPECT_EQ(msg2.inner_e_req, ENUM_TEST__OUTER_MESSAGE2__INNER_ENUM2__VALUE3);
  EXPECT_EQ(msg2.inner_e_opt, ENUM_TEST__OUTER_MESSAGE2__INNER_ENUM2__VALUE3);
}

TEST(RWPb2C, BasicCopy)
{
  TEST_DESCRIPTION("Test basic copy operations on tag-compatible protobuf structs");

  Pbcopy__MsgA ma;
  pbcopy__msg_a__init(&ma);
  ma.e1 = A_VALUE2;
  Pbcopy__MsgA* aa[] = { &ma, &ma };
  (void)aa;

  Pbcopy__MsgB mb;
  pbcopy__msg_b__init(&mb);
  mb.e1 = A_VALUE3;
  Pbcopy__MsgB* ab[] = { &mb, &mb, &mb };
  (void)ab;

  Pbcopy__MsgC mc;
  pbcopy__msg_c__init(&mc);
  mc.e1 = B_VALUE7;
  Pbcopy__MsgC* ac[] = { &mc, &mc, &mc, &mc };
  (void)ac;

  Pbcopy__TestPointy tp;
  pbcopy__test_pointy__init(&tp);

  char str16[] = "123456789012345";

  uint8_t bytes15[] = {
    0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0
  };

  tp.oi32 = 12345678;
  tp.has_oi32 = true;

  tp.ou32 = 123456789;
  tp.has_ou32 = true;

  tp.os32 = -1234567;
  tp.has_os32 = true;

  tp.oi64 = 123456789012345;
  tp.has_oi64 = true;

  tp.ou64 = 1234567890123456;
  tp.has_ou64 = true;

  tp.os64 = -1234567890123456;
  tp.has_os64 = true;

  tp.oboo = true;
  tp.has_oboo = true;

  tp.oena = A_VALUE4;
  tp.has_oena = true;

  tp.oenb = B_VALUE8;
  tp.has_oenb = true;

  tp.of64 = 234567890123456;
  tp.has_of64 = true;

  tp.oz64 = -12345678901234567;
  tp.has_oz64 = true;

  volatile double test_double = 1.12345678912345678;
  tp.odbl = test_double;
  tp.has_odbl = true;

  tp.ostr = str16;

  tp.obyt.data = bytes15;
  tp.obyt.len = sizeof(bytes15);
  tp.has_obyt = true;

  tp.omga = &ma;

  tp.omgb = &mb;

  tp.omgc = &mc;

  tp.of32 = 1234567890;
  tp.has_of32 = true;

  tp.oz32 = -123456789;
  tp.has_oz32 = true;

  volatile float test_float = 4.56789; // not exact; putting in temp var makes comparisons work
  tp.oflt = test_float;
  tp.has_oflt = true;


  // Attempt to duplicate the original message into all the others.
  UniquePtrProtobufCMessage<Pbcopy__TestPointy>::uptr_t dup_pointy(
    reinterpret_cast<Pbcopy__TestPointy*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_pointy__descriptor)));
  EXPECT_TRUE(dup_pointy.get());

  UniquePtrProtobufCMessage<Pbcopy__TestCompat>::uptr_t dup_compat(
    reinterpret_cast<Pbcopy__TestCompat*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_compat__descriptor)));
  EXPECT_TRUE(dup_compat.get());

  UniquePtrProtobufCMessage<Pbcopy__TestInline>::uptr_t dup_inline(
    reinterpret_cast<Pbcopy__TestInline*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_inline__descriptor)));
  EXPECT_TRUE(dup_inline.get());

  UniquePtrProtobufCMessage<Pbcopy__TestWireCompat>::uptr_t dup_wire_compat(
    reinterpret_cast<Pbcopy__TestWireCompat*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_wire_compat__descriptor)));
  EXPECT_FALSE(dup_wire_compat.get()); // should be false due to the messages

  UniquePtrProtobufCMessage<Pbcopy__TestWireIncompat>::uptr_t dup_wire_incompat(
    reinterpret_cast<Pbcopy__TestWireIncompat*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_wire_incompat__descriptor)));
  EXPECT_FALSE(dup_wire_incompat.get()); // should be false due to wire incompatibility

  UniquePtrProtobufCMessage<Pbcopy__TestRequired>::uptr_t dup_required(
    reinterpret_cast<Pbcopy__TestRequired*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_required__descriptor)));
  EXPECT_TRUE(dup_required.get());


  // remove the only translation errors that prevent "wire-compatibility"
  tp.obyt.data = nullptr;
  tp.obyt.len = 0;
  tp.has_obyt = false;
  tp.omgb = nullptr;

  dup_wire_compat.reset(
    reinterpret_cast<Pbcopy__TestWireCompat*>(
      protobuf_c_message_duplicate(NULL, &tp.base, &pbcopy__test_wire_compat__descriptor)));
  EXPECT_TRUE(dup_wire_compat.get()); // should work now.


  // Convert back and compare.
  // Attempt to duplicate the original message into all the others.
  dup_pointy.reset(
    reinterpret_cast<Pbcopy__TestPointy*>(
      protobuf_c_message_duplicate(NULL, &dup_compat.get()->base, &pbcopy__test_pointy__descriptor)));
  ASSERT_TRUE(dup_pointy.get());


  EXPECT_EQ(dup_pointy->oi32, 12345678);
  EXPECT_EQ(dup_pointy->ou32, 123456789);
  EXPECT_EQ(dup_pointy->os32, -1234567);
  EXPECT_EQ(dup_pointy->oi64, 123456789012345);
  EXPECT_EQ(dup_pointy->ou64, 1234567890123456);
  EXPECT_EQ(dup_pointy->os64, -1234567890123456);
  EXPECT_EQ(dup_pointy->oboo, true);
  EXPECT_EQ(dup_pointy->oena, A_VALUE4);
  EXPECT_EQ(dup_pointy->oenb, B_VALUE8);
  EXPECT_EQ(dup_pointy->of64, 234567890123456);
  EXPECT_EQ(dup_pointy->oz64, -12345678901234567);
  EXPECT_EQ(dup_pointy->odbl, test_double);
  EXPECT_STREQ(dup_pointy->ostr, str16);
  EXPECT_TRUE(dup_pointy->has_obyt);
  ASSERT_EQ(dup_pointy->obyt.len, sizeof(bytes15));
  EXPECT_TRUE(0 == memcmp(dup_pointy->obyt.data, bytes15, sizeof(bytes15)));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &dup_pointy->omga->base, &ma.base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &dup_pointy->omgb->base, &mb.base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &dup_pointy->omgc->base, &mc.base));
  EXPECT_EQ(dup_pointy->of32, 1234567890);
  EXPECT_EQ(dup_pointy->oz32, -123456789);
  EXPECT_EQ(dup_pointy->oflt, test_float);

  EXPECT_NE(dup_compat->noi32, dup_pointy->oi32);
  EXPECT_EQ(dup_compat->nou32, dup_pointy->ou32);
  EXPECT_NE(dup_compat->nos32, dup_pointy->os32);
  EXPECT_EQ(dup_compat->noi64, dup_pointy->oi64);
  EXPECT_NE(dup_compat->nou64, dup_pointy->ou64);
  EXPECT_EQ(dup_compat->nos64, dup_pointy->os64);
  EXPECT_NE(dup_compat->noboo, dup_pointy->oboo);
  EXPECT_EQ(dup_compat->noena, dup_pointy->oena);
  EXPECT_NE(dup_compat->noenb, dup_pointy->oenb);
  EXPECT_EQ(dup_compat->nof64, dup_pointy->of64);
  EXPECT_NE(dup_compat->noz64, dup_pointy->oz64);
  EXPECT_EQ(dup_compat->nodbl, dup_pointy->odbl);
  EXPECT_STRNE(dup_compat->nostr, dup_pointy->ostr);
  EXPECT_TRUE(dup_compat->has_nobyt);
  ASSERT_EQ(dup_compat->nobyt.len, dup_pointy->obyt.len);
  EXPECT_TRUE(0 == memcmp(dup_compat->nobyt.data, dup_pointy->obyt.data, dup_pointy->obyt.len));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &dup_compat->nomga->base, &dup_pointy->omga->base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &dup_compat->nomgb->base, &dup_pointy->omgb->base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &dup_compat->nomgc->base, &dup_pointy->omgc->base));
  EXPECT_EQ(dup_compat->nof32, dup_pointy->of32);
  EXPECT_NE(dup_compat->noz32, dup_pointy->oz32);
  EXPECT_EQ(dup_compat->noflt, dup_pointy->oflt);
}

TEST(RWPb2C, DeleteField)
{
  TEST_DESCRIPTION("Test delete fields on protobuf structs");

  Pbdf__MsgA ma;
  pbdf__msg_a__init(&ma);

  ma.i1 = 10;
  ma.e1 = AVALUE2;

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &ma.base, 1));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &ma.base, 2));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &ma.base, 3));
  EXPECT_FALSE(protobuf_c_message_delete_field(NULL, &ma.base, 4));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &ma.base, 5));
  EXPECT_FALSE(protobuf_c_message_delete_field(NULL, &ma.base, 6));

  ma.i1 = 10;
  ma.e1 = AVALUE2;

  Pbdf__MsgB mb;
  pbdf__msg_b__init(&mb);

  mb.has_i1 = true;
  mb.i1 = 10;
  mb.s1 = (char *)malloc(20);
  memset(mb.s1, 0, 20);
  strcpy(mb.s1, "hell0");
  mb.has_b1 = true;
  mb.b1.len = 20;
  mb.b1.data = (uint8_t *)malloc(20);
  mb.has_b1 = true;
  mb.has_e1 = true;
  mb.e1 = BVALUE8;
  mb.has_d1 = true;
  mb.d1 = 10.5;
  mb.has_f1 = true;
  mb.f1 = 20.67;
  mb.has_i2 = true;
  mb.i2 = 677777666899;
  mb.has_ui = true;
  mb.ui = 12345;
  mb.has_ui2 = true;
  mb.ui2 = 1243124123412;
  mb.has_si = true;
  mb.si = -1;
  mb.has_si2 = true;
  mb.si2 = -32423432423;
  mb.has_fi = true;
  mb.fi = 76543210;
  mb.has_bo = true;
  mb.bo = true;
  mb.ma = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(NULL, &ma.base,
                                                                     &pbdf__msg_a__descriptor));

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 100));
  EXPECT_EQ(mb.has_i1, false);
  EXPECT_EQ(mb.i1, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 101));
  ASSERT_EQ(mb.s1, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 102));
  EXPECT_EQ(mb.has_b1, false);
  EXPECT_EQ(mb.b1.len, 0);
  ASSERT_EQ(mb.b1.data, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 103));
  EXPECT_EQ(mb.has_e1, false);
  EXPECT_EQ(mb.e1, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 104));
  EXPECT_EQ(mb.has_d1, false);
  EXPECT_EQ(mb.d1, 0.0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 105));
  EXPECT_EQ(mb.has_f1, false);
  EXPECT_EQ(mb.f1, 0.0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 106));
  EXPECT_EQ(mb.has_i2, false);
  EXPECT_EQ(mb.i2, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 107));
  EXPECT_EQ(mb.has_ui, false);
  EXPECT_EQ(mb.ui, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 108));
  EXPECT_EQ(mb.has_ui2, false);
  EXPECT_EQ(mb.ui2, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 109));
  EXPECT_EQ(mb.has_si, false);
  EXPECT_EQ(mb.si, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 110));
  EXPECT_EQ(mb.has_si2, false);
  EXPECT_EQ(mb.si2, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 111));
  EXPECT_EQ(mb.has_fi, false);
  EXPECT_EQ(mb.fi, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 115));
  EXPECT_EQ(mb.has_bo, false);
  EXPECT_EQ(mb.bo, false);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mb.base, 116));
  ASSERT_EQ(mb.ma, nullptr);
  //Test non existent field.
  EXPECT_FALSE(protobuf_c_message_delete_field(NULL, &mb.base, 500));

  Pbdf__MsgC mc;
  pbdf__msg_c__init(&mc);
  mc.has_i1 = true;
  mc.i1 = 54;
  mc.s1 = (char *)malloc(30);
  strcpy(mc.s1, "abcnd");
  char *temp = mc.s1;
  mc.has_b1 = true;
  mc.b1.len = 10;
  mc.b1.data = (uint8_t *)malloc(10);
  mc.has_e1 = true;
  mc.e1 = BVALUE8;
  mc.has_d1 = true;
  mc.d1 = 1.45678901234567;
  mc.has_f1 = true;
  mc.f1 = 445.55;
  mc.has_i2 = true;
  mc.i2 = 1408140814081408;
  mc.has_ui = true;
  mc.ui = 123;
  mc.has_si2 = true;
  mc.si2 = -100;
  mc.has_fi2 = true;
  mc.fi2 = 12134;
  mc.has_sfi2 = true;
  mc.sfi2 = -45;
  mc.has_bo = true;
  mc.bo = true;
  mc.ma = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(NULL, &ma.base,
                                                                     &pbdf__msg_a__descriptor));

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 300));
  EXPECT_EQ(mc.has_i1, false);
  EXPECT_EQ(mc.i1, 8765432);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 301));
  EXPECT_NE(mc.s1, temp);
  EXPECT_FALSE(strcmp(mc.s1, "defstring"));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 302));
  EXPECT_EQ(mc.has_b1, false);
  EXPECT_EQ(mc.b1.len, 0);
  EXPECT_FALSE(strcmp((char*)(mc.b1.data), ""));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 303));
  EXPECT_FALSE(mc.has_e1);
  EXPECT_EQ(mc.e1, BVALUE5);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 304));
  EXPECT_FALSE(mc.has_d1);
  EXPECT_EQ(mc.d1, 3.4567890123456699);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 305));
  EXPECT_FALSE(mc.has_f1);
  EXPECT_EQ(mc.f1, 6.78901f);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 306));
  EXPECT_FALSE(mc.has_i2);
  EXPECT_EQ(mc.i2, 98765432109876);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 307));
  EXPECT_FALSE(mc.has_ui);
  EXPECT_EQ(mc.ui, 9876543);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 310));
  EXPECT_FALSE(mc.has_si2);
  EXPECT_EQ(mc.si2, -987654321098);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 312));
  EXPECT_FALSE(mc.has_fi2);
  EXPECT_EQ(mc.fi2, 765432109876543);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 314));
  EXPECT_FALSE(mc.has_sfi2);
  EXPECT_EQ(mc.sfi2, -6543210987654);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 315));
  EXPECT_FALSE(mc.has_bo);
  EXPECT_EQ(mc.bo, false);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mc.base, 316));
  ASSERT_EQ(mc.ma, nullptr);

  Pbdf__MsgD md;
  pbdf__msg_d__init(&md);
  md.n_i1 = 2;
  md.i1 = (int32_t*)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(int32_t)*2);
  md.i1[0] = 100;
  md.i1[1] = 101;
  md.n_s1 = 2;
  md.s1 = (char **)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(char*)*2);
  md.s1[0] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 10);
  strcpy(md.s1[0], "Hello");
  md.s1[1] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 10);
  strcpy(md.s1[1], "World");
  md.n_b1 = 2;
  md.b1 = (ProtobufCBinaryData *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(ProtobufCBinaryData)*2);
  md.b1[0].len = 0;
  md.b1[0].data = NULL;
  md.b1[1].len = 0;
  md.b1[1].data = NULL;
  md.n_d1 = 2;
  md.d1 = (double*)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(double)*2);
  md.d1[0] = 1.2;
  md.d1[1] = 2.2;
  md.n_ma = 2;
  md.ma = (Pbdf__MsgA **)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(void*)*2);
  md.ma[0] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));
  md.ma[1] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));
  EXPECT_EQ(n_alloc, 13);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &md.base, 400));
  EXPECT_EQ(md.n_i1, 0);
  EXPECT_EQ(md.i1, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &md.base, 401));
  EXPECT_EQ(md.n_s1, 0);
  EXPECT_EQ(md.s1, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &md.base, 402));
  EXPECT_EQ(md.n_b1, 0);
  EXPECT_EQ(md.b1, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &md.base, 404));
  EXPECT_EQ(md.n_d1, 0);
  EXPECT_EQ(md.d1, nullptr);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &md.base, 416));
  EXPECT_EQ(md.n_ma, 0);
  EXPECT_EQ(md.ma, nullptr);
  EXPECT_EQ(n_alloc, 0);

  // test inline fields
  Pbdf__MsgE me;
  pbdf__msg_e__init(&me);

  me.has_i1 = true;
  me.i1 = 123;
  me.has_s1 = true;
  strcpy(me.s1, "Hello World");
  me.b1.len = 10;
  memset(me.b1.data, 1, 10);
  me.has_b1 = true;
  me.has_ma = true;
  protobuf_c_message_copy_usebody(NULL, &ma.base, &me.ma.base);

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &me.base, 600));
  EXPECT_FALSE(me.has_i1);
  EXPECT_EQ(me.i1, 8765432);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &me.base, 601));
  EXPECT_FALSE(me.has_s1);
  EXPECT_FALSE(strcmp(me.s1, ""));
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &me.base, 602));
  EXPECT_FALSE(me.has_b1);
  EXPECT_EQ(me.b1.len, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &me.base, 603));
  EXPECT_FALSE(me.has_ma);
  EXPECT_EQ(me.ma.i1, 0);

  Pbdf__MsgF mf;
  pbdf__msg_f__init(&mf);

  mf.n_i1 = 2;
  mf.i1[0] = 10;
  mf.i1[1] = 11;
  mf.n_s1 = 2;
  strcpy(mf.s1[0], "Hello");
  strcpy(mf.s1[1], "World");
  mf.n_b1 = 1;
  mf.b1[0].len = 10;
  memset(mf.b1[0].data, 1, 10);
  mf.n_ma = 1;
  protobuf_c_message_copy_usebody(NULL, &ma.base, &mf.ma[0].base);

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mf.base, 500));
  EXPECT_EQ(mf.n_i1, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mf.base, 501));
  EXPECT_EQ(mf.n_s1, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mf.base, 502));
  EXPECT_EQ(mf.n_b1, 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &mf.base, 508));
  EXPECT_EQ(mf.n_ma, 0);

  // Flat messages.
  Pbdf__MsgGFlat gf;
  pbdf__msg_gflat__init(&gf);

  gf.i1 = 100;
  gf.has_name = true;
  strcpy(gf.name, "test");
  gf.n_ma = 4;
  strcpy(gf.ma[0].s1, "Msg1");
  gf.ma[0].b1.len = 10;
  gf.ma[0].i1 = 200;
  strcpy(gf.ma[1].s1, "Msg2");
  gf.ma[1].b1.len = 11;
  gf.ma[1].i1 = 201;
  strcpy(gf.ma[2].s1, "Msg3");
  gf.ma[2].b1.len = 12;
  gf.ma[2].i1 = 202;
  strcpy(gf.ma[3].s1, "Msg4");
  gf.ma[3].b1.len = 13;
  gf.ma[3].i1 = 203;

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &gf.base, 700));
  EXPECT_FALSE(protobuf_c_message_delete_field_index(NULL, &gf.base, 701, 5));
  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &gf.base, 701, 0));
  EXPECT_FALSE(gf.has_name);
  EXPECT_EQ(strlen(gf.name), 0);
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &gf.base, 702));
}

TEST(RWPb2C, DeleteFieldIndex)
{
  TEST_DESCRIPTION("Test delete fields index on protobuf structs");
  Pbdf__MsgF mf;
  pbdf__msg_f__init(&mf);

  mf.n_i1 = 1;
  mf.i1[0] = 100;	

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 500, 0));
  EXPECT_EQ(mf.n_i1, 0);

  mf.n_i1 = 3;
  mf.i1[0] = 100;	
  mf.i1[1] = 101;	
  mf.i1[2] = 102;	

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 500, 1));
  ASSERT_EQ(mf.n_i1, 2);
  EXPECT_EQ(mf.i1[0], 100);
  EXPECT_EQ(mf.i1[1], 102);

  mf.n_i1 = 4;
  mf.i1[0] = 100;	
  mf.i1[1] = 101;	
  mf.i1[2] = 102;	
  mf.i1[3] = 103;	

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 500, 1));
  ASSERT_EQ(mf.n_i1, 3);
  EXPECT_EQ(mf.i1[0], 100);
  EXPECT_EQ(mf.i1[1], 102);
  EXPECT_EQ(mf.i1[2], 103);

  mf.n_i1 = 4;
  mf.i1[0] = 100;	
  mf.i1[1] = 101;	
  mf.i1[2] = 102;	
  mf.i1[3] = 103;	

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 500, 3));
  ASSERT_EQ(mf.n_i1, 3);
  EXPECT_EQ(mf.i1[0], 100);
  EXPECT_EQ(mf.i1[1], 101);
  EXPECT_EQ(mf.i1[2], 102);

  EXPECT_FALSE(protobuf_c_message_delete_field_index(NULL, &mf.base, 500, 10));

  mf.n_s1 = 4;
  strcpy(mf.s1[0], "data1");
  strcpy(mf.s1[1], "data2");
  strcpy(mf.s1[2], "data3");
  strcpy(mf.s1[3], "data4");

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 501, 0));
  ASSERT_EQ(mf.n_s1, 3);
  EXPECT_FALSE(strcmp(mf.s1[0], "data2"));
  EXPECT_FALSE(strcmp(mf.s1[1], "data3"));
  EXPECT_FALSE(strcmp(mf.s1[2], "data4"));

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 501, 1));
  ASSERT_EQ(mf.n_s1, 2);
  EXPECT_FALSE(strcmp(mf.s1[0], "data2"));
  EXPECT_FALSE(strcmp(mf.s1[1], "data4"));

  mf.n_ma = 4;
  mf.ma[0].i1 = 1;
  mf.ma[1].i1 = 2;
  mf.ma[2].i1 = 3;
  mf.ma[3].i1 = 4;

  EXPECT_TRUE(protobuf_c_message_delete_field_index(NULL, &mf.base, 508, 1));
  ASSERT_EQ(mf.n_ma, 3);
  EXPECT_EQ(mf.ma[0].i1, 1);
  EXPECT_EQ(mf.ma[1].i1, 3);
  EXPECT_EQ(mf.ma[2].i1, 4);

  Pbdf__MsgD md;
  pbdf__msg_d__init(&md);

  md.n_i1 = 10;
  md.i1 = (int32_t *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(int32_t)*10);
  md.i1[0] = 1; md.i1[1] = 2; md.i1[2] = 3; md.i1[3] = 4; md.i1[4] = 5;
  md.i1[5] = 6; md.i1[6] = 7; md.i1[7] = 8; md.i1[8] = 9; md.i1[9] = 10;

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 400, 0));
  ASSERT_EQ(md.n_i1, 9);
  EXPECT_EQ(md.i1[0], 2);
  EXPECT_EQ(md.i1[1], 3);
  EXPECT_EQ(md.i1[2], 4);
  EXPECT_EQ(md.i1[3], 5);
  EXPECT_EQ(md.i1[4], 6);
  EXPECT_EQ(md.i1[5], 7);
  EXPECT_EQ(md.i1[6], 8);
  EXPECT_EQ(md.i1[7], 9);
  EXPECT_EQ(md.i1[8], 10);
  EXPECT_EQ(n_alloc, 1);

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 400, 4));
  ASSERT_EQ(md.n_i1, 8);
  EXPECT_EQ(md.i1[0], 2);
  EXPECT_EQ(md.i1[1], 3);
  EXPECT_EQ(md.i1[2], 4);
  EXPECT_EQ(md.i1[3], 5);
  EXPECT_EQ(md.i1[4], 7);
  EXPECT_EQ(md.i1[5], 8);
  EXPECT_EQ(md.i1[6], 9);
  EXPECT_EQ(md.i1[7], 10);
  EXPECT_EQ(n_alloc, 1);

  for(int i = 0; i < 8; i++) {
    EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 400, 0));
  }
  EXPECT_EQ(md.n_i1, 0);
  EXPECT_EQ(md.i1, nullptr);
  EXPECT_EQ(n_alloc, 0);

  md.n_s1 = 5;
  md.s1 = (char **)(pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(char*)*5));
  memset(md.s1, 0, sizeof(char*)*5);
  md.s1[0] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 20);
  memset(md.s1[0], 0 , 20);
  strcpy(md.s1[0], "string1");
  md.s1[1] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 20);
  memset(md.s1[1], 0 , 20);
  strcpy(md.s1[1], "string2");
  md.s1[2] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 20);
  memset(md.s1[2], 0 , 20);
  strcpy(md.s1[2], "string3");
  md.s1[3] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 20);
  memset(md.s1[3], 0 , 20);
  strcpy(md.s1[3], "string4");
  md.s1[4] = (char *)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, 20);
  memset(md.s1[4], 0 , 20);
  strcpy(md.s1[4], "string5");

  EXPECT_EQ(n_alloc, 6);
  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 401, 0));
  ASSERT_EQ(md.n_s1, 4);
  EXPECT_EQ(n_alloc, 5);
  EXPECT_FALSE(strcmp(md.s1[0], "string2"));
  EXPECT_FALSE(strcmp(md.s1[1], "string3"));
  EXPECT_FALSE(strcmp(md.s1[2], "string4"));
  EXPECT_FALSE(strcmp(md.s1[3], "string5"));

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 401, 1));
  ASSERT_EQ(md.n_s1, 3);
  EXPECT_EQ(n_alloc, 4);
  EXPECT_FALSE(strcmp(md.s1[0], "string2"));
  EXPECT_FALSE(strcmp(md.s1[1], "string4"));
  EXPECT_FALSE(strcmp(md.s1[2], "string5"));

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 401, 2));
  ASSERT_EQ(md.n_s1, 2);
  EXPECT_EQ(n_alloc, 3);
  EXPECT_FALSE(strcmp(md.s1[0], "string2"));
  EXPECT_FALSE(strcmp(md.s1[1], "string4"));

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 401, 1));
  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 401, 0));
  EXPECT_EQ(md.n_s1, 0);
  EXPECT_EQ(n_alloc, 0);
  EXPECT_EQ(md.s1, nullptr);

  Pbdf__MsgA ma;
  pbdf__msg_a__init(&ma);
  ma.i1 = 1;

  md.n_ma = 4;
  md.ma = (Pbdf__MsgA **)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, sizeof(ProtobufCMessage*)*4);
  md.ma[0] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));
  ma.i1 = 2;
  md.ma[1] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));
  ma.i1 = 3;
  md.ma[2] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));
  ma.i1 = 4;
  md.ma[3] = reinterpret_cast<Pbdf__MsgA*>(protobuf_c_message_duplicate(&pb2c_pbc_instance, &ma.base,
                                                                        &pbdf__msg_a__descriptor));

  EXPECT_EQ(n_alloc, 13);
  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 416, 0));
  EXPECT_EQ(n_alloc, 10);
  ASSERT_EQ(md.n_ma, 3);
  EXPECT_EQ(md.ma[0]->i1, 2);
  EXPECT_EQ(md.ma[1]->i1, 3);
  EXPECT_EQ(md.ma[2]->i1, 4);

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 416, 1));
  EXPECT_EQ(n_alloc, 7);
  ASSERT_EQ(md.n_ma, 2);
  EXPECT_EQ(md.ma[0]->i1, 2);
  EXPECT_EQ(md.ma[1]->i1, 4);

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 416, 1));
  EXPECT_EQ(n_alloc, 4);
  ASSERT_EQ(md.n_ma, 1);
  EXPECT_EQ(md.ma[0]->i1, 2);

  EXPECT_TRUE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 416, 0));
  EXPECT_EQ(n_alloc, 0);
  EXPECT_EQ(md.n_ma, 0);
  EXPECT_EQ(md.ma, nullptr);

  EXPECT_FALSE(protobuf_c_message_delete_field_index(&pb2c_pbc_instance, &md.base, 416, 0));

  // Test unknown fields.
  Pbdf__MsgF mf1;
  pbdf__msg_f__init(&mf1);

  mf1.n_i1 = 3;
  mf1.i1[0] = 100;	
  mf1.i1[1] = 101;	
  mf1.i1[2] = 102;	

  mf1.n_s1 = 4;
  strcpy(mf1.s1[0], "data1");
  strcpy(mf1.s1[1], "data2");
  strcpy(mf1.s1[2], "data3");
  strcpy(mf1.s1[3], "data4");

  mf1.n_d1 = 4;
  mf1.d1[0] = 1.1;
  mf1.d1[1] = 1.2;
  mf1.d1[2] = 1.3;
  mf1.d1[3] = 1.4;

  mf1.has_ui = true;
  mf.ui = 54;

  size_t siz1 = pbdf__msg_f__get_packed_size(&pb2c_pbc_instance, &mf1);
  uint8_t *packed1 = (uint8_t*)pb2c_pbc_instance.alloc(&pb2c_pbc_instance, siz1);
  ASSERT_NE(packed1,nullptr);

  size_t siz2 = pbdf__msg_f__pack (&pb2c_pbc_instance, &mf1, packed1);
  ASSERT_EQ(siz1,siz2);


  Pbdf__MsgFSub fsub;
  pbdf__msg_fsub__init(&fsub);

  Pbdf__MsgFSub *fsubp = pbdf__msg_fsub__unpack_usebody(&pb2c_pbc_instance, siz1, packed1, &fsub);
  ASSERT_NE(fsubp,nullptr);
  ASSERT_EQ(protobuf_c_message_unknown_get_count(&fsubp->base), 5);

  EXPECT_FALSE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &fsubp->base, 507));
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &fsubp->base, 504));

  ASSERT_EQ(protobuf_c_message_unknown_get_count(&fsubp->base), 1);
  EXPECT_TRUE(protobuf_c_message_delete_field(&pb2c_pbc_instance, &fsubp->base, 509));

  ASSERT_EQ(protobuf_c_message_unknown_get_count(&fsubp->base), 0);
  pb2c_pbc_instance.free(&pb2c_pbc_instance, packed1);
  EXPECT_EQ(n_alloc, 0);
}

TEST(RWPb2C, DeleteFieldUnknown)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);
  strcpy(nc.name, "trafgen");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if1);
  strcpy(if1.name, "if1");
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if2);
  strcpy(if2.name, "if2");

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface) *if_list[2];
  if_list[0] = &if1;
  if_list[1] = &if2;

  nc.n_interface = 2;
  nc.interface = if_list;

  nc.n_lb_profile = 2;
  strcpy(nc.lb_profile[0].name, "lbpr1");
  strcpy(nc.lb_profile[1].name, "lbpr2");

  nc.n_scriptable_lb = 1;
  strcpy(nc.scriptable_lb[0].name, "slb");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc1);
  strcpy(nc1.name, "trafgen");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if3);
  strcpy(if3.name, "if3");
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, if4);
  strcpy(if4.name, "if4");

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface) *if_list1[2];
  if_list1[0] = &if3;
  if_list1[1] = &if4;

  nc1.n_interface = 2;
  nc1.interface = if_list1;

  nc1.n_lb_profile = 2;
  strcpy(nc1.lb_profile[0].name, "lbpr3");
  strcpy(nc1.lb_profile[1].name, "lbpr4");

  nc1.n_scriptable_lb = 1;
  strcpy(nc1.scriptable_lb[0].name, "slb1");

  size_t s1;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, &nc.base, &s1);
  ASSERT_TRUE(data1);

  size_t s2;
  uint8_t* data2 = protobuf_c_message_serialize(NULL, &nc1.base, &s2);
  ASSERT_TRUE(data2);

  uint8_t data3[s1+s2];

  memcpy(data3, data1, s1);
  memcpy(data3+s1, data2, s2);

  ProtobufCMessage* out = protobuf_c_message_unpack(NULL, RWPB_G_MSG_PBCMD(RwBaseD_data_Colony_NetworkContext), s1+s2, data3);
  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<>::uptr_t uptm(out);

  EXPECT_EQ(protobuf_c_message_unknown_get_count(out), 10);

  // Get the tag of interface
  unsigned if_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)->pb_element_tag;

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, out, if_tag));
  EXPECT_EQ(protobuf_c_message_unknown_get_count(out), 6);

  // Get the slb tag
  unsigned slb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb)->pb_element_tag;

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, out, slb_tag));
  EXPECT_EQ(protobuf_c_message_unknown_get_count(out), 4);

  // Get the lb profile tag
  unsigned lb_tag = RWPB_G_PATHENTRY_YPBCD(RwFpathD_RwBaseD_data_Colony_NetworkContext_LbProfile)->pb_element_tag;
  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, out, lb_tag));

  EXPECT_EQ(protobuf_c_message_unknown_get_count(out), 0);

  protobuf_c_instance_free(NULL, data1);
  protobuf_c_instance_free(NULL, data2);
}

TEST(RWPb2C, PortDup3871)
{
  TEST_DESCRIPTION("Test for RIFT-3871");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_PortState, port_state);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_PortState_Info, info);

  strcpy(colony.name,"epc");
  colony.port_state = &port_state;

  port_state.info = &info;

  info.n_port = 1;

  auto port = &info.port[0];
  strcpy(port->name, "vfap/1/1");
  port->has_private_name = 1;
  strcpy(port->private_name, "eth_sim:name=fabric1");
  port->has_id = 1;
  port->id = 0;
  port->has_descr_string = 1;
  strcpy(port->descr_string,"");
  port->has_state = 1;
  port->state = RW_FPATH_D_STATE_UP;
  port->has_speed = 1;
  port->speed = 10000;
  port->has_duplex = 1;
  port->duplex = RW_FPATH_D_DUPLEX_FULL_DUPLEX;
  port->has_app_name = 1;
  strcpy(port->app_name,"rw_ipfp");
  port->has_receive_queues = 1;
  port->receive_queues = 2;
  port->has_transmit_queues = 1;
  port->transmit_queues = 40;
  port->has_numa_socket = 1;
  port->numa_socket = 0;
  port->has_mac_address = 1;
  strcpy(port->mac_address,"00:00:00:ab:01:00");
  port->has_fastpath_instance = 1;
  port->fastpath_instance = 1;
  port->has_logical_port_id = 1;
  port->logical_port_id = 81924;
  port->has_flow_control = 1;

  auto flow_control = &port->flow_control;
  flow_control->has_flow_type = 1;
  strcpy(flow_control->flow_type,"Both");
  flow_control->has_high_watermark = 1;
  flow_control->high_watermark = 408;
  flow_control->has_low_watermark = 1;
  flow_control->low_watermark = 306;
  flow_control->has_pause_time = 1;
  flow_control->pause_time = 100;
  flow_control->has_send_xon = 1;
  flow_control->send_xon = RW_FPATH_D_ON_OFF_ON;

  ProtobufCMessage* dup = protobuf_c_message_duplicate(NULL, &colony.base, colony.base.descriptor);
  ASSERT_NE(dup, nullptr);
  protobuf_c_message_free_unpacked(NULL, dup);
}

TEST(RWPb2C, OneofTest)
{
  OneofTest__TestMessOneof msg;
  oneof_test__test_mess_oneof__init(&msg);

  msg.has_opt_int = 1;
  msg.opt_int = 1234;

  msg.test_oneof_case  = ONEOF_TEST__TEST_MESS_ONEOF__TEST_ONEOF_TEST_MESSAGE;

  OneofTest__SubMess sm;
  oneof_test__sub_mess__init(&sm);
  sm.has_val1 = 1;
  sm.val1 = 100;
  sm.has_val2 = 1;
  sm.val2 = 200;
  msg.test_message = &sm;

  OneofTest__SubMess__SubSubMess sm1;
  oneof_test__sub_mess__sub_sub_mess__init(&sm1);
  sm1.has_val1 = 1;
  sm1.val1 = 5000;
  sm1.str1 = strdup("Hello");

  OneofTest__SubMess__SubSubMess sm2;
  oneof_test__sub_mess__sub_sub_mess__init(&sm2);
  sm2.has_val1 = 1;
  sm2.val1 = 5002;
  sm2.str1 = strdup("IHello");

  sm.sub1 = &sm1;
  sm.sub2 = &sm2;

  uint8_t buff[1024];

  size_t s = oneof_test__test_mess_oneof__pack(NULL, &msg, buff);

  OneofTest__TestMessOneof msg1;
  oneof_test__test_mess_oneof__init(&msg1);
  oneof_test__test_mess_oneof__unpack_usebody(NULL, s, buff, &msg1);

  EXPECT_TRUE(msg1.has_opt_int);
  EXPECT_EQ(msg1.opt_int, 1234);

  EXPECT_EQ(msg1.test_oneof_case, ONEOF_TEST__TEST_MESS_ONEOF__TEST_ONEOF_TEST_MESSAGE);
  EXPECT_TRUE(msg1.test_message);
  EXPECT_TRUE(msg1.test_message->has_val1);
  EXPECT_EQ(msg1.test_message->val1, 100);
  EXPECT_TRUE(msg1.test_message->has_val2);
  EXPECT_EQ(msg1.test_message->val2, 200);

  EXPECT_TRUE(msg1.test_message->sub1);
  EXPECT_TRUE(msg1.test_message->sub2);

  EXPECT_TRUE(msg1.test_message->sub1->has_val1);
  EXPECT_EQ(msg1.test_message->sub1->val1, 5000);
  EXPECT_STREQ(msg1.test_message->sub1->str1, "Hello");

  EXPECT_TRUE(msg1.test_message->sub2->has_val1);
  EXPECT_EQ(msg1.test_message->sub2->val1, 5002);
  EXPECT_STREQ(msg1.test_message->sub2->str1, "IHello");

  oneof_test__sub_mess__free_unpacked(NULL, msg1.test_message);
  free(sm1.str1);
  free(sm2.str1);
}

TEST(RWPb2C, OneofTestIn)
{
  OneofTest__TestOneofInline msg;
  oneof_test__test_oneof_inline__init(&msg);

  msg.n_list = 3;
  strcpy(msg.list[0], "Hello");
  strcpy(msg.list[1], "World");
  strcpy(msg.list[2], "Computing");

  msg.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN;
  strcpy(msg.test_string_in, "OneofString");

  uint8_t buff[1024];
  size_t s = oneof_test__test_oneof_inline__pack(NULL, &msg, buff);

  OneofTest__TestOneofInline msg1;
  oneof_test__test_oneof_inline__init(&msg1);
  oneof_test__test_oneof_inline__unpack_usebody(NULL, s, buff, &msg1);

  EXPECT_EQ(msg1.n_list, 3);
  EXPECT_STREQ(msg1.list[0], "Hello");
  EXPECT_STREQ(msg1.list[1], "World");
  EXPECT_STREQ(msg1.list[2], "Computing");

  EXPECT_EQ(msg1.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN);
  EXPECT_STREQ(msg1.test_string_in, "OneofString");

  msg.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES;
  msg.test_bytes.len = strlen("Bytessss");
  memcpy(msg.test_bytes.data, "Bytessss", msg.test_bytes.len);

  s = oneof_test__test_oneof_inline__pack(NULL, &msg, buff);
  OneofTest__TestOneofInline msg2;
  oneof_test__test_oneof_inline__init(&msg2);
  oneof_test__test_oneof_inline__unpack_usebody(NULL, s, buff, &msg2);

  EXPECT_EQ(msg2.n_list, 3);
  EXPECT_STREQ(msg2.list[0], "Hello");
  EXPECT_STREQ(msg2.list[1], "World");
  EXPECT_STREQ(msg2.list[2], "Computing");

  EXPECT_EQ(msg2.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES);
  EXPECT_EQ(msg2.test_bytes.len, 8);
  EXPECT_FALSE(memcmp(msg2.test_bytes.data, "Bytessss", 8));
}

TEST(RWPb2C, OneofRwAPIs)
{
  /* Test deleting oneof field. */
  OneofTest__TestOneofInline msg;
  oneof_test__test_oneof_inline__init(&msg);

  msg.n_list = 3;
  strcpy(msg.list[0], "Hello");
  strcpy(msg.list[1], "World");
  strcpy(msg.list[2], "Computing");

  msg.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN;
  strcpy(msg.test_string_in, "OneofString");

  EXPECT_TRUE(protobuf_c_message_delete_field(NULL, &msg.base, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN));
  EXPECT_FALSE(msg.oneof_inline_case);
  EXPECT_FALSE(strlen(msg.test_string_in));

  /* Test set field. */
  const ProtobufCFieldDescriptor *fd = protobuf_c_message_descriptor_get_field(msg.base.descriptor, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN);
  EXPECT_TRUE(fd);
  EXPECT_TRUE(protobuf_c_message_set_field_text_value(NULL, &msg.base, fd, "OneofString", strlen("OneofString")));
  EXPECT_TRUE(msg.oneof_inline_case);
  EXPECT_EQ(msg.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN);
  EXPECT_STREQ(msg.test_string_in, "OneofString");

  /* Test get field count and offset for oneof field. */
  msg.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES;
  msg.test_bytes.len = strlen("Bytessss");
  memcpy(msg.test_bytes.data, "Bytessss", msg.test_bytes.len);

  const ProtobufCFieldDescriptor *fdesc = NULL;
  size_t off = 0;
  void *msg_ptr;
  protobuf_c_boolean is_dptr = FALSE;
  size_t c = protobuf_c_message_get_field_desc_count_and_offset(&msg.base, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES,
                                                     &fdesc, &msg_ptr, &off, &is_dptr);

  EXPECT_EQ(c, 1);
  EXPECT_EQ(fdesc->type, PROTOBUF_C_TYPE_BYTES);
  EXPECT_TRUE(msg_ptr);
  EXPECT_FALSE(is_dptr);

  ProtobufCFlatBinaryData *bd = (ProtobufCFlatBinaryData *)msg_ptr;
  EXPECT_EQ(bd->len, 8);
  EXPECT_FALSE(memcmp(bd->data, "Bytessss", 8));

  /* Test get field text value. */
  char val_str[64];
  size_t val_str_len = 64;
  EXPECT_TRUE(protobuf_c_field_get_text_value(NULL, fdesc, val_str, &val_str_len, msg_ptr));
  EXPECT_EQ(val_str_len, 8);
  EXPECT_FALSE(memcmp(val_str, "Bytessss", val_str_len));

  /* Test merge messages with oneof fiels. */
  OneofTest__TestOneofInline msg1;
  oneof_test__test_oneof_inline__init(&msg1);
  msg1.n_list = 3;
  strcpy(msg1.list[0], "Cloud");
  strcpy(msg1.list[1], "Rift");
  strcpy(msg1.list[2], "Perf");

  msg1.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN;
  strcpy(msg1.test_string_in, "OneofString2");

  EXPECT_TRUE(protobuf_c_message_merge_new(NULL, &msg.base, &msg1.base));

  EXPECT_EQ(msg1.n_list, 6);
  EXPECT_STREQ(msg1.list[0], "Hello");
  EXPECT_STREQ(msg1.list[1], "World");
  EXPECT_STREQ(msg1.list[2], "Computing");
  EXPECT_STREQ(msg1.list[3], "Cloud");
  EXPECT_STREQ(msg1.list[4], "Rift");
  EXPECT_STREQ(msg1.list[5], "Perf");

  EXPECT_NE(msg1.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES);
  EXPECT_EQ(msg1.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN);
  EXPECT_STREQ(msg1.test_string_in, "OneofString2");

  msg.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE__NOT_SET;
  EXPECT_TRUE(protobuf_c_message_merge_new(NULL, &msg.base, &msg1.base));

  EXPECT_EQ(msg1.n_list, 9);
  EXPECT_STREQ(msg1.list[0], "Hello");
  EXPECT_STREQ(msg1.list[1], "World");
  EXPECT_STREQ(msg1.list[2], "Computing");
  EXPECT_STREQ(msg1.list[3], "Hello");
  EXPECT_STREQ(msg1.list[4], "World");
  EXPECT_STREQ(msg1.list[5], "Computing");
  EXPECT_STREQ(msg1.list[6], "Cloud");
  EXPECT_STREQ(msg1.list[7], "Rift");
  EXPECT_STREQ(msg1.list[8], "Perf");

  EXPECT_NE(msg1.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_BYTES);
  EXPECT_EQ(msg1.oneof_inline_case, ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE_TEST_STRING_IN);
  EXPECT_STREQ(msg1.test_string_in, "OneofString2");

  msg1.n_list = 1;
  msg1.oneof_inline_case = ONEOF_TEST__TEST_ONEOF_INLINE__ONEOF_INLINE__NOT_SET;

  EXPECT_TRUE(protobuf_c_message_merge_new(NULL, &msg.base, &msg1.base));
  EXPECT_EQ(msg1.n_list, 4);
  EXPECT_FALSE(msg1.oneof_inline_case);
}

TEST(RWPb2C, ConcreteDesc)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company_ContactInfo, comp);

  EXPECT_TRUE(comp.base_concrete.descriptor);
  EXPECT_TRUE(comp.base_concrete.descriptor->fields.name);
  EXPECT_TRUE(comp.base_concrete.descriptor->fields.address);
  EXPECT_TRUE(comp.base_concrete.descriptor->fields.phone_number);

  EXPECT_STREQ(comp.base_concrete.descriptor->fields.name->name, "name");
  EXPECT_STREQ(comp.base_concrete.descriptor->fields.address->name, "address");
  EXPECT_STREQ(comp.base_concrete.descriptor->fields.phone_number->name, "phone_number");

  RWPB_M_MSG_DECL_INIT(TestTagGenerationBase_data_TopCont1, msg);

  EXPECT_TRUE(msg.base_concrete.descriptor);
  EXPECT_TRUE(msg.base_concrete.descriptor->fields.top_str1);
  EXPECT_STREQ(msg.base_concrete.descriptor->fields.top_str1->name, "top_str1");
  EXPECT_TRUE(msg.base_concrete.descriptor->fields.top_num1);
  EXPECT_STREQ(msg.base_concrete.descriptor->fields.top_num1->name, "top_num1");
  EXPECT_TRUE(msg.base_concrete.descriptor->fields.cont1);
  EXPECT_STREQ(msg.base_concrete.descriptor->fields.cont1->name, "cont1");
  EXPECT_TRUE(msg.base_concrete.descriptor->fields.str3);
  EXPECT_STREQ(msg.base_concrete.descriptor->fields.str3->name, "str3");
  EXPECT_TRUE(msg.base_concrete.descriptor->fields.num3);
  EXPECT_STREQ(msg.base_concrete.descriptor->fields.num3->name, "num3");

  RWPB_M_MSG_DECL_INIT(TestTagGeneration_TtgbTopCont1, topcont);
  EXPECT_TRUE(topcont.base_concrete.descriptor);
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.top_str1);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.top_str1->name, "top_str1");
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.top_num1);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.top_num1->name, "top_num1");
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.cont1);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.cont1->name, "cont1");
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.str3);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.str3->name, "str3");
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.num3);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.num3->name, "num3");
  EXPECT_TRUE(topcont.base_concrete.descriptor->fields.new_top_str);
  EXPECT_STREQ(topcont.base_concrete.descriptor->fields.new_top_str->name, "new_top_str");
}

class PbcIsEqual
: public ::testing::Test
{
  protected:

    char str_1[15] = "This is a test";
    char str_2[16] = "This is a test?";
    char str_3[11] = "Not a test";

    char* r_astr_1[1] = { str_1 };
    size_t n_astr_1 = sizeof(r_astr_1)/sizeof(r_astr_1[0]);

    char* r_astr_2[3] = { str_1, str_2, str_3 };
    size_t n_astr_2 = sizeof(r_astr_2)/sizeof(r_astr_2[0]);

    char* r_astr_3[3] = { str_1, str_3, str_2 };
    size_t n_astr_3 = sizeof(r_astr_3)/sizeof(r_astr_3[0]);

    char* r_astr_4[2] = { str_3, str_3 };
    size_t n_astr_4 = sizeof(r_astr_4)/sizeof(r_astr_4[0]);


    /******************************************************************/
    uint8_t byt_1_data[10] = { 99, 234, 12, 9, 23, 69, 119, 234, 255, 196 };
    ProtobufCBinaryData byt_1 = {
      .len = sizeof(byt_1_data),
      .data = byt_1_data
    };

    uint8_t byt_2_data[10] = { 99, 234, 12, 9, 23, 69, 119, 234, 254, 196 };
    ProtobufCBinaryData byt_2 = {
      .len = sizeof(byt_2_data),
      .data = byt_2_data
    };

    uint8_t byt_3_data[9] = { 99, 234, 12, 9, 23, 69, 119, 234, 255 };
    ProtobufCBinaryData byt_3 = {
      .len = sizeof(byt_3_data),
      .data = byt_3_data
    };

    ProtobufCBinaryData byt_4 = {
      .len = sizeof(str_1) - 1,
      .data = (uint8_t*)str_1
    };

    ProtobufCBinaryData byt_5 = {
      .len = sizeof(str_2) - 1,
      .data = (uint8_t*)str_2
    };

    ProtobufCBinaryData r_abyt_1[1] = {
      { .len = sizeof(byt_1_data), .data = byt_1_data },
    };
    size_t n_abyt_1 = sizeof(r_abyt_1)/sizeof(r_abyt_1[0]);

    ProtobufCBinaryData r_abyt_2[3] = {
      { .len = sizeof(byt_1_data), .data = byt_1_data },
      { .len = sizeof(byt_2_data), .data = byt_2_data },
      { .len = sizeof(byt_3_data), .data = byt_3_data },
    };
    size_t n_abyt_2 = sizeof(r_abyt_2)/sizeof(r_abyt_2[0]);

    ProtobufCBinaryData r_abyt_3[3] = {
      { .len = sizeof(byt_1_data), .data = byt_1_data },
      { .len = sizeof(byt_3_data), .data = byt_3_data },
      { .len = sizeof(byt_2_data), .data = byt_2_data },
    };
    size_t n_abyt_3 = sizeof(r_abyt_3)/sizeof(r_abyt_3[0]);

    ProtobufCBinaryData r_abyt_4[2] = {
      { .len = sizeof(byt_3_data), .data = byt_3_data },
      { .len = sizeof(byt_3_data), .data = byt_3_data },
    };
    size_t n_abyt_4 = sizeof(r_abyt_4)/sizeof(r_abyt_4[0]);

    ProtobufCBinaryData r_abyt_5[2] = {
      { .len = sizeof(str_1) - 1, .data = (uint8_t*)str_1 },
      { .len = sizeof(str_2) - 1, .data = (uint8_t*)str_2 },
    };
    size_t n_abyt_5 = sizeof(r_abyt_5)/sizeof(r_abyt_5[0]);

    ProtobufCBinaryData r_abyt_6[2] = {
      { .len = sizeof(str_2) - 1, .data = (uint8_t*)str_2 },
      { .len = sizeof(str_1) - 1, .data = (uint8_t*)str_1 },
    };
    size_t n_abyt_6 = sizeof(r_abyt_6)/sizeof(r_abyt_6[0]);


    /******************************************************************/
    IsEqual__MsgA msga_1 = {};
    IsEqual__MsgA msga_2 = {};
    IsEqual__MsgA msga_3 = {};

    IsEqual__MsgA* r_msga_1[1] = { &msga_1 };
    size_t n_msga_1 = sizeof(r_msga_1)/sizeof(r_msga_1[0]);

    IsEqual__MsgA* r_msga_2[3] = { &msga_3, &msga_2, &msga_1 };
    size_t n_msga_2 = sizeof(r_msga_2)/sizeof(r_msga_2[0]);

    IsEqual__MsgA* r_msga_3[3] = { &msga_2, &msga_3, &msga_1 };
    size_t n_msga_3 = sizeof(r_msga_3)/sizeof(r_msga_3[0]);


    /******************************************************************/
    IsEqual__MsgB msgb_1 = {};
    IsEqual__MsgB msgb_2 = {};
    IsEqual__MsgB msgb_3 = {};

    IsEqual__MsgB* r_msgb_1[1] = { &msgb_1 };
    size_t n_msgb_1 = sizeof(r_msgb_1)/sizeof(r_msgb_1[0]);

    IsEqual__MsgB* r_msgb_2[3] = { &msgb_3, &msgb_2, &msgb_1 };
    size_t n_msgb_2 = sizeof(r_msgb_2)/sizeof(r_msgb_2[0]);

    IsEqual__MsgB* r_msgb_3[3] = { &msgb_2, &msgb_3, &msgb_1 };
    size_t n_msgb_3 = sizeof(r_msgb_3)/sizeof(r_msgb_3[0]);


    /******************************************************************/
    IsEqual__MsgC msgc_1 = {};
    IsEqual__MsgC msgc_2 = {};
    IsEqual__MsgC msgc_3 = {};
    IsEqual__MsgC msgc_4 = {};
    IsEqual__MsgC msgc_5 = {};

    IsEqual__MsgC* r_msgc_1[1] = { &msgc_1 };
    size_t n_msgc_1 = sizeof(r_msgc_1)/sizeof(r_msgc_1[0]);

    IsEqual__MsgC* r_msgc_2[3] = { &msgc_3, &msgc_2, &msgc_1 };
    size_t n_msgc_2 = sizeof(r_msgc_2)/sizeof(r_msgc_2[0]);

    IsEqual__MsgC* r_msgc_3[3] = { &msgc_2, &msgc_3, &msgc_1 };
    size_t n_msgc_3 = sizeof(r_msgc_3)/sizeof(r_msgc_3[0]);


    /******************************************************************/
    IsEqual__EnumA r_aena_1[1] = { IE_A_VALUE4 };
    size_t n_aena_1 = sizeof(r_aena_1)/sizeof(r_aena_1[0]);

    IsEqual__EnumA r_aena_2[3] = { IE_A_VALUE4, IE_A_VALUE2, IE_A_VALUE1 };
    size_t n_aena_2 = sizeof(r_aena_2)/sizeof(r_aena_2[0]);

    IsEqual__EnumA r_aena_3[3] = { IE_A_VALUE2, IE_A_VALUE4, IE_A_VALUE1 };
    size_t n_aena_3 = sizeof(r_aena_3)/sizeof(r_aena_3[0]);

    IsEqual__EnumA r_aena_4[3] = { IE_A_VALUE2, IE_A_VALUE3, IE_A_VALUE1 };
    size_t n_aena_4 = sizeof(r_aena_4)/sizeof(r_aena_4[0]);


    /******************************************************************/
    int32_t r_ai32_1[1] = { 1234 };
    size_t n_ai32_1 = sizeof(r_ai32_1)/sizeof(r_ai32_1[0]);

    int32_t r_ai32_2[3] = { 1234, 12, 1 };
    size_t n_ai32_2 = sizeof(r_ai32_2)/sizeof(r_ai32_2[0]);

    int32_t r_ai32_3[3] = { 12, 1234, 1 };
    size_t n_ai32_3 = sizeof(r_ai32_3)/sizeof(r_ai32_3[0]);

    int32_t r_ai32_4[3] = { 12, 123, 1 };
    size_t n_ai32_4 = sizeof(r_ai32_4)/sizeof(r_ai32_4[0]);


    template <class T>
    void set_a(T* p)
    {
      p->oi32 = 1234;
      p->has_oi32 = true;
      p->os32 = -1234;
      p->has_os32 = true;
      p->ou64 = 12345678901234;
      p->has_ou64 = true;
      p->oboo = true;
      p->has_oboo = true;
      p->oenb = IE_B_VALUE7;
      p->has_oenb = true;
      p->oz64 = 9876543210321;
      p->has_oz64 = true;
      p->ostr = str_1;
      p->omga = &msga_1;
      p->omgc = &msgc_1;
      p->oz32 = 5678012;
      p->has_oz32 = true;
      p->ai32 = r_ai32_2;
      p->n_ai32 = n_ai32_2;
      p->astr = r_astr_2;
      p->n_astr = n_astr_2;
      p->amga = r_msga_2;
      p->n_amga = n_msga_2;
      p->amgc = r_msgc_2;
      p->n_amgc = n_msgc_2;
    }

    template <class T>
    void set_b(T* p)
    {
      p->ou32 = 345098;
      p->has_ou32 = true;
      p->oi64 = -567891234509876;
      p->has_oi64 = true;
      p->os64 = -543210987654321;
      p->has_os64 = true;
      p->oena = IE_A_VALUE3;
      p->has_oena = true;
      p->of64 = 456543212345678;
      p->has_of64 = true;
      p->odbl = 6.1234567890123;
      p->has_odbl = true;
      p->obyt = byt_1;
      p->has_obyt = true;
      p->omgb = &msgb_1;
      p->of32 = 456789098;
      p->has_of32 = true;
      p->oflt = 98765.4321;
      p->has_oflt = true;
      p->aena = r_aena_2;
      p->n_aena = n_aena_2;
      p->abyt = r_abyt_2;
      p->n_abyt = n_abyt_2;
      p->amgb = r_msgb_2;
      p->n_amgb = n_msgb_2;
    }

    virtual void SetUp() {
      is_equal__msg_a__init(&msga_1);
      msga_1.s1 = str_1;
      msga_1.b1 = byt_1;
      msga_1.i1 = 12345;
      msga_1.e1 = IE_A_VALUE2;

      is_equal__msg_a__init(&msga_2);
      msga_2.s1 = str_2;
      msga_2.b1 = byt_1;
      msga_2.i1 = 12345;
      msga_2.e1 = IE_A_VALUE2;

      is_equal__msg_a__init(&msga_3);
      msga_3.s1 = str_2;
      msga_3.b1 = byt_3;
      msga_3.i1 = 123456;
      msga_3.e1 = IE_A_VALUE1;

      is_equal__msg_b__init(&msgb_1);
      msgb_1.s1 = str_1;
      msgb_1.b1 = byt_1;
      msgb_1.has_b1 = true;
      msgb_1.i1 = 12345;
      msgb_1.has_i1 = true;
      msgb_1.e1 = IE_A_VALUE2;
      msgb_1.has_e1 = true;

      is_equal__msg_b__init(&msgb_2);
      msgb_2.s1 = str_2;
      msgb_2.b1 = byt_1;
      msgb_2.has_b1 = true;
      msgb_2.i1 = 12345;
      msgb_2.has_i1 = true;
      msgb_2.e1 = IE_A_VALUE2;
      msgb_2.has_e1 = true;

      is_equal__msg_b__init(&msgb_3);
      msgb_3.s1 = str_2;
      msgb_3.b1 = byt_3;
      msgb_3.has_b1 = true;

      is_equal__msg_c__init(&msgc_1);
      msgc_1.b1 = r_abyt_4;
      msgc_1.n_b1 = n_abyt_4;
      msgc_1.b2 = r_abyt_5;
      msgc_1.n_b2 = n_abyt_5;

      is_equal__msg_c__init(&msgc_2);
      msgc_2.b1 = r_abyt_3;
      msgc_2.n_b1 = n_abyt_3;
      msgc_2.b2 = r_abyt_2;
      msgc_2.n_b2 = n_abyt_2;
      msgc_2.i1 = 12345;
      msgc_2.has_i1 = true;
      msgc_2.e1 = IE_B_VALUE7;
      msgc_2.has_e1 = true;

      is_equal__msg_c__init(&msgc_3);
      msgc_2.b1 = r_abyt_2;
      msgc_2.n_b1 = n_abyt_2;
      msgc_2.b2 = r_abyt_3;
      msgc_2.n_b2 = n_abyt_3;
      msgc_2.i1 = 12345;
      msgc_2.has_i1 = true;
      msgc_2.e1 = IE_B_VALUE7;
      msgc_2.has_e1 = true;

      is_equal__msg_c__init(&msgc_4);
      msgc_4.b1 = r_abyt_2;
      msgc_4.n_b1 = n_abyt_2;

      is_equal__msg_c__init(&msgc_5);
      msgc_5.b1 = r_abyt_3;
      msgc_5.n_b1 = n_abyt_3;
    }
};

TEST_F(PbcIsEqual, Strict)
{
  TEST_DESCRIPTION("Strict is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_2 = {};
  is_equal__test__init(&test_2);

  set_a(&test_1);
  set_a(&test_2);

  /* self */
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_1.base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_1.base, &protobuf_c_pack_opts_default));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_1.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_1.base, &protobuf_c_pack_opts_ordered));

  /* between 1 and 2 */
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_2.base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_default));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_ordered));
}

TEST_F(PbcIsEqual, OrderFree)
{
  TEST_DESCRIPTION("Order free is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_2 = {};
  is_equal__test__init(&test_2);

  set_a(&test_1);
  set_a(&test_2);

  /* Use differently ordered, but equivalent enum arrays */
  test_1.aena = r_aena_3;
  test_1.n_aena = n_aena_3;
  test_2.aena = r_aena_2;
  test_2.n_aena = n_aena_2;
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_2.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_default));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_ordered));
}

TEST_F(PbcIsEqual, NotEqual)
{
  TEST_DESCRIPTION("Unequal is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_2 = {};
  is_equal__test__init(&test_2);

  /* Use different enum arrays */
  test_1.aena = r_aena_3;
  test_1.n_aena = n_aena_3;
  test_2.aena = r_aena_4;
  test_2.n_aena = n_aena_4;
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_2.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_default));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_ordered));
}

TEST_F(PbcIsEqual, SubMsg)
{
  TEST_DESCRIPTION("Submsg is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_2 = {};
  is_equal__test__init(&test_2);

  /* Use submessages with differently ordered arrays */
  test_1.omgc = &msgc_4;
  test_2.omgc = &msgc_5;
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_2.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_default));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_ordered));
}

TEST_F(PbcIsEqual, SubMsgDeep)
{
  TEST_DESCRIPTION("Submsg is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_2 = {};
  is_equal__test__init(&test_2);

  /* Use differently ordered submessages */
  test_1.amgc = r_msgc_3;
  test_1.n_amgc = n_msgc_3;
  test_2.amgc = r_msgc_2;
  test_2.n_amgc = n_msgc_2;
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_2.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_default));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_2.base, &protobuf_c_pack_opts_ordered));
}

TEST_F(PbcIsEqual, DiffMsgTypes)
{
  TEST_DESCRIPTION("Different type messages is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__Test test_1a = {};
  is_equal__test__init(&test_1a);

  IsEqual__Test test_1b = {};
  is_equal__test__init(&test_1b);

  IsEqual__TestHalfA test_2a = {};
  is_equal__test_half_a__init(&test_2a);

  IsEqual__TestHalfB test_2b = {};
  is_equal__test_half_b__init(&test_2b);

  set_a(&test_1);
  set_a(&test_1a);
  set_a(&test_2a);

  set_b(&test_1);
  set_b(&test_1b);
  set_b(&test_2b);

  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_1a.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_1a.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1.base, &test_1b.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1.base, &test_1b.base, &protobuf_c_pack_opts_known_ordered));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep(pinstance, &test_1a.base, &test_1b.base));
  EXPECT_FALSE(protobuf_c_message_is_equal_deep_opts(pinstance, &test_1a.base, &test_1b.base, &protobuf_c_pack_opts_known_ordered));

  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &test_1a.base, &test_2a.base));
  EXPECT_TRUE(protobuf_c_message_is_equal_deep(pinstance, &test_1b.base, &test_2b.base));
}

TEST_F(PbcIsEqual, UnknownFields)
{
  TEST_DESCRIPTION("Unknown field is_equal tests");

  IsEqual__Test test_1 = {};
  is_equal__test__init(&test_1);

  IsEqual__TestHalfA test_2a = {};
  is_equal__test_half_a__init(&test_2a);

  IsEqual__TestHalfB test_2b = {};
  is_equal__test_half_b__init(&test_2b);

  set_a(&test_1);
  set_a(&test_2a);
  set_b(&test_1);
  set_b(&test_2b);

  /* default - the serialized data is unordered */
  UniquePtrProtobufCMessage<>::uptr_t test_1ad(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      &test_1.base,
      test_2a.base.descriptor,
      &protobuf_c_pack_opts_default));
  ASSERT_TRUE(test_1ad.get());

  /* the serialized data is ordered */
  UniquePtrProtobufCMessage<>::uptr_t test_1ao(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      &test_1.base,
      test_2a.base.descriptor,
      &protobuf_c_pack_opts_ordered));
  ASSERT_TRUE(test_1ao.get());

  /* the serialized data is ordered, and unknown fields are removed */
  UniquePtrProtobufCMessage<>::uptr_t test_1ak(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      test_1ad.get(),
      test_2a.base.descriptor,
      &protobuf_c_pack_opts_known_ordered));
  ASSERT_TRUE(test_1ak.get());

  /* default - the serialized data is unordered */
  UniquePtrProtobufCMessage<>::uptr_t test_1bd(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      &test_1.base,
      test_2b.base.descriptor,
      &protobuf_c_pack_opts_default));
  ASSERT_TRUE(test_1bd.get());

  /* the serialized data is ordered */
  UniquePtrProtobufCMessage<>::uptr_t test_1bo(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      &test_1.base,
      test_2b.base.descriptor,
      &protobuf_c_pack_opts_ordered));
  ASSERT_TRUE(test_1bo.get());

  /* the serialized data is ordered, and unknown fields are removed */
  UniquePtrProtobufCMessage<>::uptr_t test_1bk(
    protobuf_c_message_duplicate_opts(
      NULL/*instance*/,
      test_1bd.get(),
      test_2b.base.descriptor,
      &protobuf_c_pack_opts_known_ordered));
  ASSERT_TRUE(test_1bk.get());

  /*
   * At this point:
   *   test_1   : fields A and B
   *   test_2a  : fields A, no unknown
   *   test_2b  : fields B, no unknown
   *   test_1ad : fields A and B, B are unknown, and original order
   *   test_1ao : fields A and B, B are unknown, and sorted
   *   test_1ak : fields B, no unknown, and sorted
   *   test_1bd : fields A and B, A are unknown, and original order
   *   test_1bo : fields A and B, A are unknown, and sorted
   *   test_1bk : fields B, no unknown, and sorted
   *
   * Truth Table:
   */
  bool tt[][3] = {
    /* { def, ord, kn_ord } */
    { false, false, false }, //  0    1,  2a
    { false, false, false }, //  1    1,  2b
    { false,  true, false }, //  2    1, 1ad
    { false,  true, false }, //  3    1, 1ao
    { false, false, false }, //  4    1, 1ak
    { false, false, false }, //  5    1, 1bd
    { false,  true, false }, //  6    1, 1bo
    { false, false, false }, //  7    1, 1bk
    { false, false, false }, //  8   2a,  2b
    { false, false,  true }, //  9   2a, 1ad
    { false, false,  true }, // 10   2a, 1ao
    { false,  true,  true }, // 11   2a, 1ak
    { false, false, false }, // 12   2a, 1bd
    { false, false, false }, // 13   2a, 1bo
    { false, false, false }, // 14   2a, 1bk
    { false, false, false }, // 15   2b, 1ad
    { false, false, false }, // 16   2b, 1ao
    { false, false, false }, // 17   2b, 1ak
    { false, false,  true }, // 18   2b, 1bd
    { false, false,  true }, // 19   2b, 1bo
    { false,  true,  true }, // 20   2b, 1bk
    { false,  true,  true }, // 21  1ad, 1ao
    { false, false,  true }, // 22  1ad, 1ak
    { false, false, false }, // 23  1ad, 1bd
    { false,  true, false }, // 24  1ad, 1bo
    { false, false, false }, // 25  1ad, 1bk
    { false, false,  true }, // 26  1ao, 1ak
    { false, false, false }, // 27  1ao, 1bd
    { false,  true, false }, // 28  1ao, 1bo
    { false, false, false }, // 29  1ao, 1bk
    { false, false, false }, // 30  1ak, 1bd
    { false, false, false }, // 31  1ak, 1bo
    { false, false, false }, // 32  1ak, 1bk
    { false, false,  true }, // 33  1bd, 1bo
    { false, false,  true }, // 34  1bd, 1bk
    { false, false,  true }, // 35  1bo, 1bk
  };

  const ProtobufCMessage* m[] = {
    &test_1.base,    // 0
    &test_2a.base,   // 1
    &test_2b.base,   // 2
    test_1ad.get(),  // 3
    test_1ao.get(),  // 4
    test_1ak.get(),  // 5
    test_1bd.get(),  // 6
    test_1bo.get(),  // 7
    test_1bk.get(),  // 8
  };

  unsigned t = 0;
  for (unsigned i = 0; i < sizeof(m)/sizeof(m[0]); ++i) {
    for (unsigned j = i+1; j < sizeof(m)/sizeof(m[0]); ++j) {
      ASSERT_LT(t, sizeof(tt)/sizeof(tt[0]));
      bool b = protobuf_c_message_is_equal_deep_opts(pinstance, m[i], m[j], &protobuf_c_pack_opts_default);
      if (b != tt[t][0]) {
        ADD_FAILURE() << "Unexpected " << b << ": default i=" << i << ", j=" << j << ", t=" << t;
      }
      b = protobuf_c_message_is_equal_deep_opts(pinstance, m[i], m[j], &protobuf_c_pack_opts_ordered);
      if (b != tt[t][1]) {
        ADD_FAILURE() << "Unexpected " << b << ": ordered i=" << i << ", j=" << j << ", t=" << t;
      }
      b = protobuf_c_message_is_equal_deep_opts(pinstance, m[i], m[j], &protobuf_c_pack_opts_known_ordered);
      if (b != tt[t][2]) {
        ADD_FAILURE() << "Unexpected " << b << ": known_ordered i=" << i << ", j=" << j << ", t=" << t;
      }

      ++t;
    }
  }
}


TEST(RWPb2C, DISABLED_UnpackCrash6366)
{
  TEST_DESCRIPTION("Test unpack failure for RIFT-6366");

  /* This packed message was taken straight form the core file */
  static const uint8_t data[] = {
    0xe2, 0x8a, 0x80, 0x84, 0x1, 0xa0, 0x8, 0xe8,
    0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80,
    0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1,
    0x0, 0x80, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x88,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x90, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x98, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2,
    0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80,
    0x84, 0x1, 0x1, 0x80, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x88, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x90,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x98, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1,
    0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8,
    0x8a, 0x80, 0x84, 0x1, 0x2, 0x80, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x88, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x90, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x98,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x3, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8,
    0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80,
    0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1,
    0x4, 0x80, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x88,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x90, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x98, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2,
    0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80,
    0x84, 0x1, 0x5, 0x80, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x88, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x90,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x98, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1,
    0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8,
    0x8a, 0x80, 0x84, 0x1, 0x6, 0x80, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x88, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x90, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x98,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x7, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8,
    0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80,
    0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1,
    0x8, 0x80, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x88,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x90, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x98, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2,
    0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80,
    0x84, 0x1, 0x9, 0x80, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x88, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x90,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x98, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1,
    0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8,
    0x8a, 0x80, 0x84, 0x1, 0xa, 0x80, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x88, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x90, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x98,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xb, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8,
    0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80,
    0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1,
    0xc, 0x80, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x88,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x90, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x98, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0xf2,
    0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8, 0x8a, 0x80,
    0x84, 0x1, 0xd, 0x80, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x88, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x90,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x98, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80, 0x84, 0x1,
    0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1, 0x36, 0xf8,
    0x8a, 0x80, 0x84, 0x1, 0xe, 0x80, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x88, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x90, 0x8b, 0x80, 0x84, 0x1, 0x0, 0x98,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xb8,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x5, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xf, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x0, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x1, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x2, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x3, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x4, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x6, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x7, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x8, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x9, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xa, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xb, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xc, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xd, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xe, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x3, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xf, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x0, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x1, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x2, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x3, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x4, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x5, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x6, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x7, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x8, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0x9, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xa, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xb, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xc, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xd, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xe, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xe2,
    0x8a, 0x80, 0x84, 0x1, 0x42, 0xe8, 0x8a, 0x80,
    0x84, 0x1, 0x4, 0xf2, 0x8a, 0x80, 0x84, 0x1,
    0x36, 0xf8, 0x8a, 0x80, 0x84, 0x1, 0xf, 0x80,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0x88, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0x90, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0x98, 0x8b, 0x80, 0x84, 0x1, 0x0, 0xa0,
    0x8b, 0x80, 0x84, 0x1, 0x0, 0xa8, 0x8b, 0x80,
    0x84, 0x1, 0x0, 0xb0, 0x8b, 0x80, 0x84, 0x1,
    0x0, 0xb8, 0x8b, 0x80, 0x84, 0x1, 0x0,
  };

  int len = sizeof(data);

  pb2c_errorbuf_clear();
  UniquePtrProtobufCMessage<>::uptr_t pbcm(
    protobuf_c_message_unpack(
      &pb2c_pbc_instance_errorbuf,
      RWPB_G_MSG_PBCMD(RwFpathDataE_RwBaseE_data_Colony_LoadBalancer),
      len,
      data ));

  EXPECT_EQ( nullptr, pbcm.get() );
  EXPECT_NE( nullptr, strstr(pb2c_errorbuf, "inline_max reached") );
}

TEST(RWPb2C, ExportEBuff)
{
  TEST_DESCRIPTION("Test export error buffer API");

  // Generate some errors.
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);

  const ProtobufCFieldDescriptor* fd = protobuf_c_message_descriptor_get_field_by_name(
      nc.base.descriptor, "name");
  ASSERT_TRUE(fd);

  ProtobufCMessage *temp = nullptr;
  protobuf_c_boolean ret = protobuf_c_message_set_field_message(NULL, &nc.base, fd, &temp);
  EXPECT_FALSE(ret);

  const char longstr[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  ret = protobuf_c_message_set_field_text_value(NULL, &nc.base, fd, longstr, strlen(longstr));
  EXPECT_FALSE(ret);

  fd = protobuf_c_message_descriptor_get_field_by_name(
      nc.base.descriptor, "scriptable_lb");

  ret = protobuf_c_message_set_field_text_value(NULL, &nc.base, fd, "test", strlen("test"));
  EXPECT_FALSE(ret);

  RWPB_M_MSG_DECL_INIT(RwFpathD_ScriptableLBCfg_ReceiveFunction, rf);

  fd = protobuf_c_message_descriptor_get_field_by_name(
      rf.base.descriptor, "function_type");

  ret = protobuf_c_message_set_field_text_value(NULL, &rf.base, fd, "invalide", strlen("invalide"));
  EXPECT_FALSE(ret);

  // Test the pbc error buffer export API
  RWPB_M_MSG_DECL_INIT(RwPbcStats_data_PbcEbuf_Tasklet, pbm);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&pbm.base);

  int nr = protobuf_c_instance_export_ebuf(NULL, &pbm.base, "error_records", "time_stamp", "error_str");
  ASSERT_TRUE(nr);
  ASSERT_TRUE(pbm.n_error_records);
  ASSERT_TRUE(pbm.error_records);

#if 0
  for (unsigned i = 0; i < pbm.n_error_records; i++) {
    ASSERT_TRUE(pbm.error_records[i]);
    std::cout << pbm.error_records[i]->time_stamp << " "
        << pbm.error_records[i]->error_str << std::endl;
  }
#endif
}

TEST(RWPb2C, ZeroLengthBytes)
{
  TEST_DESCRIPTION("Test zero-length bytes fields");

  uint8_t bumpy_bytes0_data[] = {
    0x00 // need something, in order to make a valid pointer
  };
  ProtobufCBinaryData bumpy_bytes0 = {
    .len = 0,
    .data = bumpy_bytes0_data,
  };

  ProtobufCBinaryData bumpy_arr_b0_2[] = {
    {
      .len = 0,
      .data = bumpy_bytes0_data,
    },
    {
      .len = 0,
      .data = bumpy_bytes0_data,
    },
  };

  ProtobufCBinaryData bumpy_arr_b0_1[] = {
    {
      .len = 0,
      .data = bumpy_bytes0_data,
    },
  };

  /* makes 2 allocs: msg(1), data(1) */
  Pbcopy__MsgB bumpy_mb;
  pbcopy__msg_b__init(&bumpy_mb);
  bumpy_mb.b1 = bumpy_bytes0;
  bumpy_mb.has_b1 = true;
  Pbcopy__MsgB* bumpy_arr_mb_3[] = { &bumpy_mb, &bumpy_mb, &bumpy_mb };

  /* makes 13 allocs: msg(1), 3 MsgB in 1 array (6+1), 3 data in 2 arrays (3+2) */
  Pbcopy__MsgC bumpy_mc;
  pbcopy__msg_c__init(&bumpy_mc);
  bumpy_mc.b1 = bumpy_arr_b0_2;
  bumpy_mc.n_b1 = 2;
  bumpy_mc.b2 = bumpy_arr_b0_1;
  bumpy_mc.n_b2 = 1;
  bumpy_mc.mb = bumpy_arr_mb_3;
  bumpy_mc.n_mb = 3;
  Pbcopy__MsgC* bumpy_arr_mc_4[] = { &bumpy_mc, &bumpy_mc, &bumpy_mc, &bumpy_mc };

  /* makes 80 allocs: msg(1), 3 bytes in 1 array(3+1), 4 MsgB and 1 array (6+1), 5 MsgC and 1 array (65+1) */
  Pbcopy__TestPointy bumpy_test;
  pbcopy__test_pointy__init(&bumpy_test);
  bumpy_test.obyt = bumpy_bytes0;
  bumpy_test.has_obyt = true;
  bumpy_test.omgb = &bumpy_mb;
  bumpy_test.omgc = &bumpy_mc;
  bumpy_test.abyt = bumpy_arr_b0_2;
  bumpy_test.n_abyt = 2;
  bumpy_test.amgb = bumpy_arr_mb_3;
  bumpy_test.n_amgb = 3;
  bumpy_test.amgc = bumpy_arr_mc_4;
  bumpy_test.n_amgc = 4;


  // Copy the message, make sure all the bytes fields are allocated
  pb2c_errorbuf_clear();
  UniquePtrProtobufCMessage<Pbcopy__TestPointy>::uptr_t dup_bumpy(
    reinterpret_cast<Pbcopy__TestPointy*>(
      protobuf_c_message_duplicate(&pb2c_pbc_instance, &bumpy_test.base, &pbcopy__test_pointy__descriptor)),
    &pb2c_pbc_instance);
  ASSERT_TRUE(dup_bumpy.get());
  EXPECT_EQ(n_alloc, 80);


  // Track all the sub-objects
  std::list<Pbcopy__MsgC*> list_c;
  std::list<Pbcopy__MsgB*> list_b;
  std::list<ProtobufCBinaryData*> list_bd;

  // Check the top-level message
  EXPECT_TRUE(dup_bumpy->has_obyt);
  list_bd.emplace_back(&dup_bumpy->obyt);

  EXPECT_EQ(dup_bumpy->n_abyt, 2);
  ASSERT_NE(dup_bumpy->abyt, nullptr);
  for (unsigned i = 0; i < dup_bumpy->n_abyt; ++i) {
    list_bd.emplace_back(&dup_bumpy->abyt[i]);
  }

  ASSERT_NE(dup_bumpy->omgb, nullptr);
  list_b.emplace_back(dup_bumpy->omgb);

  EXPECT_EQ(dup_bumpy->n_amgb, 3);
  ASSERT_NE(dup_bumpy->amgb, nullptr);
  for (unsigned i = 0; i < dup_bumpy->n_amgb; ++i) {
    ASSERT_NE(dup_bumpy->amgb[i], nullptr);
    list_b.emplace_back(dup_bumpy->amgb[i]);
  }

  ASSERT_NE(dup_bumpy->omgc, nullptr);
  list_c.emplace_back(dup_bumpy->omgc);

  EXPECT_EQ(dup_bumpy->n_amgc, 4);
  ASSERT_NE(dup_bumpy->amgc, nullptr);
  for (unsigned i = 0; i < dup_bumpy->n_amgc; ++i) {
    ASSERT_NE(dup_bumpy->amgc[i], nullptr);
    list_c.emplace_back(dup_bumpy->amgc[i]);
  }

  // Check all the MsgC's
  for (auto& p: list_c) {
    EXPECT_EQ(p->n_b1, 2);
    ASSERT_NE(p->b1, nullptr);
    for (unsigned i = 0; i < p->n_b1; ++i) {
      list_bd.emplace_back(&p->b1[i]);
    }

    EXPECT_EQ(p->n_b2, 1);
    ASSERT_NE(p->b2, nullptr);
    for (unsigned i = 0; i < p->n_b2; ++i) {
      list_bd.emplace_back(&p->b2[i]);
    }

    EXPECT_EQ(p->n_mb, 3);
    ASSERT_NE(p->mb, nullptr);
    for (unsigned i = 0; i < p->n_mb; ++i) {
      ASSERT_NE(p->mb[i], nullptr);
      list_b.emplace_back(p->mb[i]);
    }
  }

  // Check all the MsgB's
  for (auto& p: list_b) {
    EXPECT_TRUE(p->has_b1);
    list_bd.emplace_back(&p->b1);
  }

  // Check all the binary data
  for (auto& p: list_bd) {
    EXPECT_EQ(p->len, 0);
    EXPECT_NE(p->data, nullptr);
  }

  // Free the dup, make sure all the binary data gets freed.
  dup_bumpy.reset();
  EXPECT_EQ(n_alloc, 0);


  // Now dup into flat. Make sure there are no extra allocs.
  pb2c_errorbuf_clear();
  UniquePtrProtobufCMessage<Pbcopy__TestFlat>::uptr_t dup_flat(
    reinterpret_cast<Pbcopy__TestFlat*>(
      protobuf_c_message_duplicate(&pb2c_pbc_instance, &bumpy_test.base, &pbcopy__test_flat__descriptor)),
    &pb2c_pbc_instance);
  ASSERT_TRUE(dup_flat.get());
  EXPECT_EQ(n_alloc, 1);

  // Track all the sub-objects
  std::list<Pbcopy__FlatC*> flat_c;
  std::list<Pbcopy__FlatB*> flat_b;
  std::list<ProtobufCFlatBinaryData*> flat_bd;

  // Check the top-level flat message
  EXPECT_TRUE(dup_flat->has_obyt);
  flat_bd.emplace_back(&dup_flat->obyt);

  EXPECT_EQ(dup_flat->n_abyt, 2);
  for (unsigned i = 0; i < dup_flat->n_abyt; ++i) {
    flat_bd.emplace_back(&dup_flat->abyt[i].meh);
  }

  EXPECT_TRUE(dup_flat->has_omgb);
  flat_b.emplace_back(&dup_flat->omgb);

  EXPECT_EQ(dup_flat->n_amgb, 3);
  for (unsigned i = 0; i < dup_flat->n_amgb; ++i) {
    flat_b.emplace_back(&dup_flat->amgb[i]);
  }

  EXPECT_TRUE(dup_flat->has_omgc);
  flat_c.emplace_back(&dup_flat->omgc);

  EXPECT_EQ(dup_flat->n_amgc, 4);
  for (unsigned i = 0; i < dup_flat->n_amgc; ++i) {
    flat_c.emplace_back(&dup_flat->amgc[i]);
  }

  // Check all the FlatC's
  for (auto& p: flat_c) {
    EXPECT_EQ(p->n_b1, 2);
    for (unsigned i = 0; i < p->n_b1; ++i) {
      flat_bd.emplace_back(&p->b1[i].meh);
    }

    EXPECT_EQ(p->n_b2, 1);
    for (unsigned i = 0; i < p->n_b2; ++i) {
      flat_bd.emplace_back(&p->b2[i].meh);
    }

    EXPECT_EQ(p->n_mb, 3);
    for (unsigned i = 0; i < p->n_mb; ++i) {
      flat_b.emplace_back(&p->mb[i]);
    }
  }

  // Check all the FlatB's
  for (auto& p: flat_b) {
    EXPECT_TRUE(p->has_b1);
    flat_bd.emplace_back(&p->b1);
  }

  // Check all the binary data
  for (auto& p: flat_bd) {
    EXPECT_EQ(p->len, 0);
    EXPECT_EQ(p->data[0], 0);
  }

  // Free the dup, make sure all the binary data gets freed.
  dup_flat.reset();
  EXPECT_EQ(n_alloc, 0);
}

TEST(RWPb2C, NegativeEnum)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, slb);
  strcpy(slb.name, "slb");
  slb.has_receive_function = 1;
  slb.receive_function.has_function_type = 1;
  slb.receive_function.function_type =  (RwFpathD__YangEnum__PluginFunctionType__E)-1;

  uint8_t data[512];
  size_t s = protobuf_c_message_get_packed_size(NULL, &slb.base);
  ASSERT_TRUE(s <= 512);

  size_t rs = protobuf_c_message_pack(NULL, &slb.base, data);
  ASSERT_EQ(rs, s);

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_ScriptableLb, ret);

  ProtobufCMessage* out = protobuf_c_message_unpack_usebody(NULL, ret.base.descriptor, s, data, &ret.base);
  ASSERT_TRUE(out);
  EXPECT_EQ(ret.receive_function.function_type, -1);
}

static void check_offset_and_outer(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)* colony)
{
  // Check the ref_state and offset of inline messages.
  ProtobufCReferenceState ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);

  auto bundle_ether = &colony->bundle_ether[0];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(bundle_ether->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  intptr_t outer_offset = bundle_ether->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony), bundle_ether[0]));

  bundle_ether = &colony->bundle_ether[1];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(bundle_ether->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  outer_offset = bundle_ether->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony), bundle_ether[1]));

  bundle_ether = &colony->bundle_ether[30];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(bundle_ether->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  outer_offset = bundle_ether->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony), bundle_ether[30]));

  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony->virtual_fabric->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);

  auto fp = &colony->virtual_fabric->fastpath[0];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(fp->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  outer_offset = fp->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_VirtualFabric), fastpath[0]));

  auto vfap = &fp->vfap[0];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(vfap->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  outer_offset = vfap->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_VirtualFabric_Fastpath), vfap[0]));

  auto ip = &vfap->ip[2];
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(ip->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  outer_offset = ip->base.ref_hdr.outer_offset;
  ASSERT_EQ(outer_offset, offsetof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_VirtualFabric_Fastpath_Vfap), ip[2]));
}

TEST(RWPb2C, RefStateAndOffset)
{
  TEST_DESCRIPTION("Test whether the reference state and the offset are generated properly");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);
  UniquePtrProtobufCMessageUseBody<>::uptr_t muptr(&colony.base);

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_VirtualFabric, temp);
  colony.virtual_fabric = temp;

  check_offset_and_outer(&colony);
}

TEST(RWPb2C, RefAndOffsetOnUnpack)
{
  TEST_DESCRIPTION("Test whether the reference state and the offset are set properly during de-serialization");
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);
  UniquePtrProtobufCMessageUseBody<>::uptr_t muptr(&colony.base);

  strcpy(colony.name, "trafgen");
  colony.n_bundle_ether = 1;
  strcpy(colony.bundle_ether[0].name, "be1");
  colony.bundle_ether[0].has_descr_string = true;
  strcpy(colony.bundle_ether[0].descr_string, "desc1");
  colony.bundle_ether[0].has_mtu = true;
  colony.bundle_ether[0].mtu = 1000;

  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony_VirtualFabric, temp);
  colony.virtual_fabric = temp;
  colony.virtual_fabric->n_fastpath = 1;
  colony.virtual_fabric->fastpath[0].instance = 1;
  colony.virtual_fabric->fastpath[0].n_vfap = 1;
  colony.virtual_fabric->fastpath[0].vfap[0].vfapid = 100;
  colony.virtual_fabric->fastpath[0].vfap[0].n_ip = 1;
  strcpy(colony.virtual_fabric->fastpath[0].vfap[0].ip[0].address, "10.0.1.0");

  size_t sz = 0;
  uint8_t* data = protobuf_c_message_serialize(NULL, &colony.base, &sz);
  ASSERT_TRUE(data);
  UniquePtrProtobufCFree<>::uptr_t upd(data);

  ProtobufCMessage *out = protobuf_c_message_unpack(
      NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony), sz, data);
  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<>::uptr_t uptr(out);

  check_offset_and_outer((RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*)out);
}

TEST(RWPb2C, KsRefStateAndOffset)
{
  TEST_DESCRIPTION("Test whether the reference state and the offsets are generated correctly for ks types");

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_VirtualFabric_Fastpath_Vfap_Ip, ip_ks);
  ASSERT_EQ(rw_keyspec_path_get_depth(&ip_ks.rw_keyspec_path_t), 5);

  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t kuptr(&ip_ks.rw_keyspec_path_t);

  ProtobufCReferenceState ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(ip_ks.base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);

  auto path004 = &ip_ks.dompath.path004;
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(path004->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  auto elemid = &ip_ks.dompath.path004.element_id;
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(elemid->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  auto key00 = &ip_ks.dompath.path004.key00;
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(key00->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_INNER);

  rw_status_t s = rw_keyspec_path_create_leaf_entry(
      &ip_ks.rw_keyspec_path_t, NULL, 
      (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data_Colony_VirtualFabric_Fastpath_Vfap_Ip), "echo-request-sent");

  ASSERT_EQ(s, RW_STATUS_SUCCESS);

  ASSERT_EQ(rw_keyspec_path_get_depth(&ip_ks.rw_keyspec_path_t), 6);
  auto path005 = ip_ks.dompath.path005;
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(path005->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);

  elemid = path005->element_id;
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(elemid->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);
}

TEST(RWPb2C, RefCounting)
{
  TEST_DESCRIPTION("Test the pbcm ref and unref functions");

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);

  ProtobufCReferenceState ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_OWNED);

  uint32_t ref_count = PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count , 0);

  ProtobufCMessage* msg = protobuf_c_message_ref(&colony.base);
  ASSERT_EQ(msg, &colony.base);

  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_STATE);
  ref_count = PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_COUNT);

  ASSERT_EQ(ref_count , 2);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_SHARED);

  protobuf_c_message_unref(&colony.base);
  ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_STATE);
  ref_count = PROTOBUF_C_FLAG_GET(colony.base.ref_hdr.ref_flags, REF_COUNT);

  ASSERT_EQ(ref_count , 1);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_SHARED);

  protobuf_c_message_free_unpacked_usebody(NULL, &colony.base);
}

TEST(RWPb2C, RefCountingGi)
{
  TEST_DESCRIPTION("Test the pbcm ref and unref functions mocking gi calls");

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony) *colony = NULL;
  colony = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony) *)protobuf_c_message_gi_create(
      NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony));

  ASSERT_TRUE(colony);
  ProtobufCReferenceState ref_state = (ProtobufCReferenceState)PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_STATE);
  ASSERT_EQ(ref_state, PROTOBUF_C_FLAG_REF_STATE_COUNTED);

  uint32_t ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 1);

  strcpy(colony->name, "trafgen");
  colony->n_bundle_ether = 2;
  auto be1 = &colony->bundle_ether[0];

  protobuf_c_message_ref(&be1->base);
  ref_count = PROTOBUF_C_FLAG_GET(be1->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 0);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 2);

  auto be2 = &colony->bundle_ether[1];

  protobuf_c_message_ref(&be2->base);
  ref_count = PROTOBUF_C_FLAG_GET(be2->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 0);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 3);

  protobuf_c_message_ref(&be1->vlan[0].base);
  ref_count = PROTOBUF_C_FLAG_GET(be1->vlan[0].base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 0);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 4);

  protobuf_c_message_ref(&be1->vlan[1].base);
  ref_count = PROTOBUF_C_FLAG_GET(be1->vlan[1].base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 0);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 5);

  colony->trafgen_info = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_TrafgenInfo)*)
      protobuf_c_message_gi_create(NULL, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_TrafgenInfo));
  ASSERT_TRUE(colony->trafgen_info);

  ref_count = PROTOBUF_C_FLAG_GET(colony->trafgen_info->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 1);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 5);

  protobuf_c_message_ref(&colony->trafgen_info->base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->trafgen_info->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 2);

  protobuf_c_message_unref_or_free_child(NULL, &colony->trafgen_info->base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->trafgen_info->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 1);

  protobuf_c_message_unref(&colony->trafgen_info->base);
  colony->trafgen_info = NULL;

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 5);

  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 5);

  protobuf_c_message_unref(&be1->vlan[1].base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 4);

  protobuf_c_message_unref(&be1->vlan[0].base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 3);

  protobuf_c_message_unref(&be2->base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 2);

  protobuf_c_message_unref(&be1->base);
  ref_count = PROTOBUF_C_FLAG_GET(colony->base.ref_hdr.ref_flags, REF_COUNT);
  ASSERT_EQ(ref_count, 1);

  protobuf_c_message_unref(&colony->base);
}

TEST(RWPb2C, DynamicDescriptors)
{
  // test-ydom-top.yang apis_test.
  // binary_test 
  ProtobufCMessageDescriptor binary_test = test_ydom_top__yang_data__test_ydom_top__top__apis_test__binary_test__descriptor;
  protobuf_c_message_create_init_value(NULL, &binary_test);
  ASSERT_TRUE(binary_test.init_value);

  TestYdomTop__YangData__TestYdomTop__Top__ApisTest__BinaryTest bt1, bt2;
  test_ydom_top__yang_data__test_ydom_top__top__apis_test__binary_test__init(&bt1);

  protobuf_c_message_init(&binary_test, &bt2.base);
  ASSERT_EQ(bt2.base.descriptor, &binary_test);
  EXPECT_FALSE(protobuf_c_message_memcmp(&bt1.base, &bt2.base));

  // ntest
  ProtobufCMessageDescriptor ntest = test_ydom_top__yang_data__test_ydom_top__top__apis_test__ntest__descriptor;
  protobuf_c_message_create_init_value(NULL, &ntest);
  ASSERT_TRUE(ntest.init_value);

  TestYdomTop__YangData__TestYdomTop__Top__ApisTest__Ntest nt1, nt2;
  test_ydom_top__yang_data__test_ydom_top__top__apis_test__ntest__init(&nt1);

  protobuf_c_message_init(&ntest, &nt2.base);
  ASSERT_EQ(nt2.base.descriptor, &ntest);
  EXPECT_FALSE(protobuf_c_message_memcmp(&nt1.base, &nt2.base));

  //reallist
  ProtobufCMessageDescriptor reallist = test_ydom_top__yang_data__test_ydom_top__top__apis_test__reallist__descriptor;
  protobuf_c_message_create_init_value(NULL, &reallist);
  ASSERT_TRUE(reallist.init_value);

  TestYdomTop__YangData__TestYdomTop__Top__ApisTest__Reallist rl1, rl2;
  test_ydom_top__yang_data__test_ydom_top__top__apis_test__reallist__init(&rl1);

  protobuf_c_message_init(&reallist, &rl2.base);
  ASSERT_EQ(rl2.base.descriptor, &reallist);
  EXPECT_FALSE(protobuf_c_message_memcmp(&rl1.base, &rl2.base));

  //apis_test
  ProtobufCMessageDescriptor apis_test = test_ydom_top__yang_data__test_ydom_top__top__apis_test__descriptor; 
  protobuf_c_message_create_init_value(NULL, &apis_test);
  ASSERT_TRUE(apis_test.init_value);

  TestYdomTop__YangData__TestYdomTop__Top__ApisTest at1, at2;
  test_ydom_top__yang_data__test_ydom_top__top__apis_test__init(&at1);

  protobuf_c_message_init(&apis_test, &at2.base);
  EXPECT_FALSE(protobuf_c_message_memcmp(&at1.base, &at2.base));

  protobuf_c_instance_free(NULL, (void*)binary_test.init_value);
  protobuf_c_instance_free(NULL, (void*)ntest.init_value);
  protobuf_c_instance_free(NULL, (void*)reallist.init_value);
  protobuf_c_instance_free(NULL, (void*)apis_test.init_value);
}

TEST(RWPb2C, DynamicDescriptorsIn)
{
  //testy2p-top1.yang, top1c2
  ProtobufCMessageDescriptor l1 = testy2p_top1__yang_data__testy2p_top1__top1c2__l1__descriptor;
  protobuf_c_message_create_init_value(NULL, &l1);
  ASSERT_TRUE(l1.init_value);

  Testy2pTop1__YangData__Testy2pTop1__Top1c2__L1 ml1, ml2;
  testy2p_top1__yang_data__testy2p_top1__top1c2__l1__init(&ml1);

  protobuf_c_message_init(&l1, &ml2.base);
  ASSERT_EQ(ml2.base.descriptor, &l1);
  EXPECT_FALSE(protobuf_c_message_memcmp(&ml1.base, &ml2.base));

  protobuf_c_instance_free(NULL, (void*)l1.init_value);
}
