
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <stdlib.h>
#include <stdio.h>

#include "rwut.h"
#include "rwlib.h"
#include "test-full.pb-c.h"
#include "test-full-cxx-output.inc"

/*
 * ATTN:- Do not add any test cases to this file.
 * These are protobuf-c unit tests converted to gtests.
 */

#define TEST_ENUM_SMALL_TYPE_NAME   Foo__TestEnumSmall
#define TEST_ENUM_SMALL(shortname)   FOO__TEST_ENUM_SMALL__##shortname
#define TEST_ENUM_TYPE_NAME   Foo__TestEnum
#include "common_test_arrays.h"
#define N_ELEMENTS(arr)   (sizeof(arr)/sizeof((arr)[0]))

/* ==== helper functions ==== */
static protobuf_c_boolean
are_bytes_equal (unsigned actual_len,
                 const uint8_t *actual_data,
                 unsigned expected_len,
                 const uint8_t *expected_data)
{
  if (actual_len != expected_len)
    return 0;
  return memcmp (actual_data, expected_data, actual_len) == 0;
}

static void
dump_bytes_stderr (size_t len, const uint8_t *data)
{
  size_t i;
  for (i = 0; i < len; i++)
    fprintf(stderr," %02x", data[i]);
  fprintf(stderr,"\n");
}
static void
test_versus_static_array(unsigned actual_len,
                         const uint8_t *actual_data,
                         unsigned expected_len,
                         const uint8_t *expected_data,
                         const char *static_buf_name,
                         const char *filename,
                         unsigned lineno)
{
  if (are_bytes_equal (actual_len, actual_data, expected_len, expected_data))
    return;

  fprintf (stderr, "test_versus_static_array failed:\n"
                   "actual: [length=%u]\n", actual_len);
  dump_bytes_stderr (actual_len, actual_data);
  fprintf (stderr, "expected: [length=%u] [buffer %s]\n",
           expected_len, static_buf_name);
  dump_bytes_stderr (expected_len, expected_data);
  fprintf (stderr, "at line %u of %s.\n", lineno, filename);
  abort();
}

#define TEST_VERSUS_STATIC_ARRAY(actual_len, actual_data, buf) \
  test_versus_static_array(actual_len,actual_data, \
                           sizeof(buf), buf, #buf, __FILE__, __LINE__)

/* rv is unpacked message */
static void *
test_compare_pack_methods (ProtobufCMessage *message,
                           size_t *packed_len_out,
                           uint8_t **packed_out)
{
  unsigned char scratch[16];
  ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT (NULL, scratch);
  size_t siz1 = protobuf_c_message_get_packed_size (NULL, message);
  size_t siz2;
  size_t siz3 = protobuf_c_message_pack_to_buffer (message, &bs.base);
  uint8_t *packed1 = (uint8_t *)malloc (siz1);
  //ASSERT_TRUE(packed1);
  EXPECT_EQ(siz1, siz3);
  siz2 = protobuf_c_message_pack (NULL, message, packed1);
  EXPECT_EQ(siz1 , siz2);
  EXPECT_EQ(bs.len , siz1);
  EXPECT_FALSE (memcmp (bs.data, packed1, siz1));
  auto rv = protobuf_c_message_unpack (NULL, message->descriptor, siz1, packed1);
  //ASSERT_TRUE(rv);
  PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&bs);
  *packed_len_out = siz1;
  *packed_out = packed1;
  return rv;
}

#define GENERIC_ASSIGN(dst,src)    ((dst) = (src))

#define NUMERIC_EQUALS(a,b)   ((a) == (b))
#define STRING_EQUALS(a,b)    (strcmp((a),(b))==0)

#define CHECK_NONE(a)
#define CHECK_NOT_NULL(a)    ASSERT_TRUE( (a) )

static protobuf_c_boolean
binary_data_equals (ProtobufCBinaryData a, ProtobufCBinaryData b)
{
  if (a.len != b.len)
    return 0;
  return memcmp (a.data, b.data, a.len) == 0;
}

static protobuf_c_boolean
submesses_equals (Foo__SubMess *a, Foo__SubMess *b)
{
  EXPECT_EQ(a->base.descriptor, &foo__sub_mess__descriptor);
  EXPECT_EQ(b->base.descriptor, &foo__sub_mess__descriptor);
  return a->test == b->test;
}

/* === the actual tests === */


/* === misc fencepost tests === */

static void test_enum_small (void)
{

#define DO_TEST(UC_VALUE) \
  do{ \
    Foo__TestMessRequiredEnumSmall small; \
    foo__test_mess_required_enum_small__init(&small); \
    size_t len; \
    uint8_t *data; \
    Foo__TestMessRequiredEnumSmall *unpacked; \
    small.test = UC_VALUE; \
    unpacked = (Foo__TestMessRequiredEnumSmall *)test_compare_pack_methods ((ProtobufCMessage*)&small, &len, &data); \
    EXPECT_EQ(unpacked->test, UC_VALUE); \
    foo__test_mess_required_enum_small__free_unpacked (NULL, unpacked); \
    TEST_VERSUS_STATIC_ARRAY (len, data, test_enum_small_##UC_VALUE); \
    free (data); \
  }while(0)

  EXPECT_EQ(sizeof(Foo__TestEnumSmall), 4);

  DO_TEST(VALUE);
  DO_TEST(OTHER_VALUE);

#undef DO_TEST
}

TEST(PBCTest, TestEnumSmall)
{
  test_enum_small();
}

TEST(PBCTest, TestEnumBig)
{
  Foo__TestMessRequiredEnum big;
  foo__test_mess_required_enum__init(&big);
  size_t len;
  uint8_t *data;
  Foo__TestMessRequiredEnum *unpacked;

  EXPECT_EQ(sizeof (Foo__TestEnum) , 4);

#define DO_ONE_TEST(shortname, numeric_value, encoded_len) \
  do{ \
    big.test = shortname; \
    unpacked = (Foo__TestMessRequiredEnum*)test_compare_pack_methods ((ProtobufCMessage*)&big, &len, &data); \
    EXPECT_EQ(unpacked->test , shortname); \
    foo__test_mess_required_enum__free_unpacked (NULL, unpacked); \
    TEST_VERSUS_STATIC_ARRAY (len, data, test_enum_big_##shortname); \
    EXPECT_EQ(encoded_len + 1 , len); \
    EXPECT_EQ(big.test , numeric_value); \
    free (data); \
  }while(0)

  DO_ONE_TEST(VALUE0, 0, 1);
  DO_ONE_TEST(VALUE127, 127, 1);
  DO_ONE_TEST(VALUE128, 128, 2);
  DO_ONE_TEST(VALUE16383, 16383, 2);
  DO_ONE_TEST(VALUE16384, 16384, 3);
  DO_ONE_TEST(VALUE2097151, 2097151, 3);
  DO_ONE_TEST(VALUE2097152, 2097152, 4);
  DO_ONE_TEST(VALUE268435455, 268435455, 4);
  DO_ONE_TEST(VALUE268435456, 268435456, 5);

#undef DO_ONE_TEST
}

TEST(PBCTest, TestFieldNumbers)
{
  size_t len;
  uint8_t *data;

#define DO_ONE_TEST(num, exp_len) \
  { \
    Foo__TestFieldNo##num t;\
    foo__test_field_no##num##__init(&t); \
    Foo__TestFieldNo##num *t2; \
    t.test = (char *)"tst"; \
    t2 = (Foo__TestFieldNo##num *)test_compare_pack_methods ((ProtobufCMessage*)(&t), &len, &data); \
    EXPECT_STREQ (t2->test, "tst"); \
    TEST_VERSUS_STATIC_ARRAY (len, data, test_field_number_##num); \
    EXPECT_EQ(len , exp_len); \
    free (data); \
    foo__test_field_no##num##__free_unpacked (NULL, t2); \
  }
  DO_ONE_TEST (15, 1 + 1 + 3);
  DO_ONE_TEST (16, 2 + 1 + 3);
  DO_ONE_TEST (2047, 2 + 1 + 3);
  DO_ONE_TEST (2048, 3 + 1 + 3);
  DO_ONE_TEST (262143, 3 + 1 + 3);
  DO_ONE_TEST (262144, 4 + 1 + 3);
  DO_ONE_TEST (33554431, 4 + 1 + 3);
  DO_ONE_TEST (33554432, 5 + 1 + 3);
#undef DO_ONE_TEST
}

/* === Required type fields === */

#define DO_TEST_REQUIRED(Type, TYPE, type, value, example_packed_data, equal_func) \
  do{ \
  Foo__TestMessRequired##Type opt; \
  foo__test_mess_required_##type##__init(&opt); \
  Foo__TestMessRequired##Type *mess; \
  size_t len; uint8_t *data; \
  opt.test = value; \
  mess = (Foo__TestMessRequired##Type *)test_compare_pack_methods (&opt.base, &len, &data); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  EXPECT_TRUE (equal_func (mess->test, value)); \
  foo__test_mess_required_##type##__free_unpacked (NULL, mess); \
  free (data); \
  }while(0)

TEST(PBCTest, TestRequiredint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Int32, INT32, int32, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT32_MIN, test_required_int32_min);
  DO_TEST(-1000, test_required_int32_m1000);
  DO_TEST(0, test_required_int32_0);
  DO_TEST(INT32_MAX, test_required_int32_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredsint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(SInt32, SINT32, sint32, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT32_MIN, test_required_sint32_min);
  DO_TEST(-1000, test_required_sint32_m1000);
  DO_TEST(0, test_required_sint32_0);
  DO_TEST(INT32_MAX, test_required_sint32_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredsfixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(SFixed32, SFIXED32, sfixed32, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT32_MIN, test_required_sfixed32_min);
  DO_TEST(-1000, test_required_sfixed32_m1000);
  DO_TEST(0, test_required_sfixed32_0);
  DO_TEST(INT32_MAX, test_required_sfixed32_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequireduint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(UInt32, UINT32, uint32, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(0, test_required_uint32_0);
  DO_TEST(MILLION, test_required_uint32_million);
  DO_TEST(UINT32_MAX, test_required_uint32_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredfixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Fixed32, FIXED32, fixed32, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(0, test_required_fixed32_0);
  DO_TEST(MILLION, test_required_fixed32_million);
  DO_TEST(UINT32_MAX, test_required_fixed32_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Int64, INT64, int64, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT64_MIN, test_required_int64_min);
  DO_TEST(-TRILLION, test_required_int64_mtril);
  DO_TEST(0, test_required_int64_0);
  DO_TEST(QUADRILLION, test_required_int64_quad);
  DO_TEST(INT64_MAX, test_required_int64_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredsint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(SInt64, SINT64, sint64, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT64_MIN, test_required_sint64_min);
  DO_TEST(-TRILLION, test_required_sint64_mtril);
  DO_TEST(0, test_required_sint64_0);
  DO_TEST(QUADRILLION, test_required_sint64_quad);
  DO_TEST(INT64_MAX, test_required_sint64_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredsfixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(SFixed64, SFIXED64, sfixed64, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(INT64_MIN, test_required_sfixed64_min);
  DO_TEST(-TRILLION, test_required_sfixed64_mtril);
  DO_TEST(0, test_required_sfixed64_0);
  DO_TEST(QUADRILLION, test_required_sfixed64_quad);
  DO_TEST(INT64_MAX, test_required_sfixed64_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequireduint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(UInt64, UINT64, uint64, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(0, test_required_uint64_0);
  DO_TEST(THOUSAND, test_required_uint64_thou);
  DO_TEST(MILLION, test_required_uint64_mill);
  DO_TEST(BILLION, test_required_uint64_bill);
  DO_TEST(TRILLION, test_required_uint64_tril);
  DO_TEST(QUADRILLION, test_required_uint64_quad);
  DO_TEST(QUINTILLION, test_required_uint64_quint);
  DO_TEST(UINT64_MAX, test_required_uint64_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredFixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Fixed64, FIXED64, fixed64, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(0, test_required_fixed64_0);
  DO_TEST(THOUSAND, test_required_fixed64_thou);
  DO_TEST(MILLION, test_required_fixed64_mill);
  DO_TEST(BILLION, test_required_fixed64_bill);
  DO_TEST(TRILLION, test_required_fixed64_tril);
  DO_TEST(QUADRILLION, test_required_fixed64_quad);
  DO_TEST(QUINTILLION, test_required_fixed64_quint);
  DO_TEST(UINT64_MAX, test_required_fixed64_max);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredFloat)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Float, FLOAT, float, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(-THOUSAND, test_required_float_mthou);
  DO_TEST(0, test_required_float_0);
  DO_TEST(420, test_required_float_420);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredDouble)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Double, DOUBLE, double, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(-THOUSAND, test_required_double_mthou);
  DO_TEST(0, test_required_double_0);
  DO_TEST(420, test_required_double_420);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredBool)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Bool, BOOL, bool, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(0, test_required_bool_0);
  DO_TEST(1, test_required_bool_1);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredTestEnumSmall)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(EnumSmall, ENUM_SMALL, enum_small, value, example_packed_data, NUMERIC_EQUALS)
  DO_TEST(VALUE, test_required_enum_small_VALUE);
  DO_TEST(OTHER_VALUE, test_required_enum_small_OTHER_VALUE);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredTestEnum)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(Enum, ENUM, enum, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (VALUE0, test_required_enum_0);
  DO_TEST (VALUE1, test_required_enum_1);
  DO_TEST (VALUE127, test_required_enum_127);
  DO_TEST (VALUE128, test_required_enum_128);
  DO_TEST (VALUE16383, test_required_enum_16383);
  DO_TEST (VALUE16384, test_required_enum_16384);
  DO_TEST (VALUE2097151, test_required_enum_2097151);
  DO_TEST (VALUE2097152, test_required_enum_2097152);
  DO_TEST (VALUE268435455, test_required_enum_268435455);
  DO_TEST (VALUE268435456, test_required_enum_268435456);

#undef DO_TEST
}

TEST(PBCTest, TestRequiredString)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED(String, STRING, string, value, example_packed_data, STRING_EQUALS)

  DO_TEST((char *)"", test_required_string_empty);
  DO_TEST((char *)"hello", test_required_string_hello);
  DO_TEST((char *)"two hundred xs follow: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", test_required_string_long);

#undef DO_TEST
}

TEST(PBCTest, TestRequiredBytes)
{
  static ProtobufCBinaryData bd_empty = { 0, (uint8_t*)"" };
  static ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
  static ProtobufCBinaryData bd_random = { 5, (uint8_t*)"\1\0\375\2\4" };
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED (Bytes, BYTES, bytes, value, example_packed_data, binary_data_equals)
  DO_TEST (bd_empty, test_required_bytes_empty);
  DO_TEST (bd_hello, test_required_bytes_hello);
  DO_TEST (bd_random, test_required_bytes_random);
#undef DO_TEST
}

TEST(PBCTest, TestRequiredSubMess)
{
  Foo__SubMess submess;
  foo__sub_mess__init(&submess);
#define DO_TEST(value, example_packed_data) \
  DO_TEST_REQUIRED (Message, MESSAGE, message, value, example_packed_data, submesses_equals)
  submess.test = 0;
  DO_TEST (&submess, test_required_submess_0);
  submess.test = 42;
  DO_TEST (&submess, test_required_submess_42);
#undef DO_TEST
}

/* === Optional type fields === */
TEST(PBCTest, TestEmptyOptional)
{
  Foo__TestMessOptional mess;
  foo__test_mess_optional__init(&mess);
  size_t len;
  uint8_t *data;
  Foo__TestMessOptional *mess2 = (Foo__TestMessOptional *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(len , 0);
  free (data);
  foo__test_mess_optional__free_unpacked (NULL, mess2);
}


#define DO_TEST_OPTIONAL(base_member, value, example_packed_data, equal_func) \
  do{ \
  Foo__TestMessOptional opt;\
  foo__test_mess_optional__init(&opt); \
  Foo__TestMessOptional *mess; \
  size_t len; uint8_t *data; \
  opt.has_##base_member = 1; \
  opt.base_member = value; \
  mess = (Foo__TestMessOptional *)test_compare_pack_methods (&opt.base, &len, &data); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  EXPECT_TRUE (mess->has_##base_member); \
  EXPECT_TRUE (equal_func (mess->base_member, value)); \
  foo__test_mess_optional__free_unpacked (NULL, mess); \
  free (data); \
  }while(0)

TEST(PBCTest, TestOptionalint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_int32, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_int32_min);
  DO_TEST (-1, test_optional_int32_m1);
  DO_TEST (0, test_optional_int32_0);
  DO_TEST (666, test_optional_int32_666);
  DO_TEST (INT32_MAX, test_optional_int32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalsint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_sint32, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_sint32_min);
  DO_TEST (-1, test_optional_sint32_m1);
  DO_TEST (0, test_optional_sint32_0);
  DO_TEST (666, test_optional_sint32_666);
  DO_TEST (INT32_MAX, test_optional_sint32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalsfixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_sfixed32, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_sfixed32_min);
  DO_TEST (-1, test_optional_sfixed32_m1);
  DO_TEST (0, test_optional_sfixed32_0);
  DO_TEST (666, test_optional_sfixed32_666);
  DO_TEST (INT32_MAX, test_optional_sfixed32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_int64, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_int64_min);
  DO_TEST (-1111111111LL, test_optional_int64_m1111111111LL);
  DO_TEST (0, test_optional_int64_0);
  DO_TEST (QUINTILLION, test_optional_int64_quintillion);
  DO_TEST (INT64_MAX, test_optional_int64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalsint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_sint64, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_sint64_min);
  DO_TEST (-1111111111LL, test_optional_sint64_m1111111111LL);
  DO_TEST (0, test_optional_sint64_0);
  DO_TEST (QUINTILLION, test_optional_sint64_quintillion);
  DO_TEST (INT64_MAX, test_optional_sint64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalsfixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_sfixed64, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_sfixed64_min);
  DO_TEST (-1111111111LL, test_optional_sfixed64_m1111111111LL);
  DO_TEST (0, test_optional_sfixed64_0);
  DO_TEST (QUINTILLION, test_optional_sfixed64_quintillion);
  DO_TEST (INT64_MAX, test_optional_sfixed64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionaluint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_uint32, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_uint32_0);
  DO_TEST (669, test_optional_uint32_669);
  DO_TEST (UINT32_MAX, test_optional_uint32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalfixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_fixed32, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_fixed32_0);
  DO_TEST (669, test_optional_fixed32_669);
  DO_TEST (UINT32_MAX, test_optional_fixed32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionaluint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_uint64, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_uint64_0);
  DO_TEST (669669669669669ULL, test_optional_uint64_669669669669669);
  DO_TEST (UINT64_MAX, test_optional_uint64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalfixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_fixed64, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_fixed64_0);
  DO_TEST (669669669669669ULL, test_optional_fixed64_669669669669669);
  DO_TEST (UINT64_MAX, test_optional_fixed64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalFloat)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_float, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (-100, test_optional_float_m100);
  DO_TEST (0, test_optional_float_0);
  DO_TEST (141243, test_optional_float_141243);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalDouble)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_double, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (-100, test_optional_double_m100);
  DO_TEST (0, test_optional_double_0);
  DO_TEST (141243, test_optional_double_141243);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalbool)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_boolean, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_bool_0);
  DO_TEST (1, test_optional_bool_1);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalTestEnumSmall)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_enum_small, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (VALUE, test_optional_enum_small_0);
  DO_TEST (OTHER_VALUE, test_optional_enum_small_1);

#undef DO_TEST
}

TEST(PBCTest, TestOptionalTestEnum)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL(test_enum, value, example_packed_data, NUMERIC_EQUALS)

  DO_TEST (VALUE0, test_optional_enum_0);
  DO_TEST (VALUE1, test_optional_enum_1);
  DO_TEST (VALUE127, test_optional_enum_127);
  DO_TEST (VALUE128, test_optional_enum_128);
  DO_TEST (VALUE16383, test_optional_enum_16383);
  DO_TEST (VALUE16384, test_optional_enum_16384);
  DO_TEST (VALUE2097151, test_optional_enum_2097151);
  DO_TEST (VALUE2097152, test_optional_enum_2097152);
  DO_TEST (VALUE268435455, test_optional_enum_268435455);
  DO_TEST (VALUE268435456, test_optional_enum_268435456);

#undef DO_TEST
}

#define DO_TEST_OPTIONAL__NO_HAS(base_member, value, example_packed_data, equal_func) \
  do{ \
  Foo__TestMessOptional opt;\
  foo__test_mess_optional__init(&opt); \
  Foo__TestMessOptional *mess; \
  size_t len; uint8_t *data; \
  opt.base_member = value; \
  mess = (Foo__TestMessOptional *)test_compare_pack_methods (&opt.base, &len, &data); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  ASSERT_TRUE (mess->base_member); \
  EXPECT_TRUE (equal_func (mess->base_member, value)); \
  foo__test_mess_optional__free_unpacked (NULL, mess); \
  free (data); \
  }while(0)

TEST(PBCTest, TestOptionalString)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL__NO_HAS (test_string, value, example_packed_data, STRING_EQUALS)
  DO_TEST ((char *)"", test_optional_string_empty);
  DO_TEST ((char *)"hello", test_optional_string_hello);
#undef DO_TEST
}

TEST(PBCTest, TestOptionalBytes)
{
  static ProtobufCBinaryData bd_empty = { 0, (uint8_t*)"" };
  static ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
  static ProtobufCBinaryData bd_random = { 5, (uint8_t*)"\1\0\375\2\4" };
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL (test_bytes, value, example_packed_data, binary_data_equals)
  DO_TEST (bd_empty, test_optional_bytes_empty);
  DO_TEST (bd_hello, test_optional_bytes_hello);
  DO_TEST (bd_random, test_optional_bytes_random);
#undef DO_TEST
}

TEST(PBCTest, TestOptionalSubMess)
{
  Foo__SubMess submess;
  foo__sub_mess__init(&submess);
#define DO_TEST(value, example_packed_data) \
  DO_TEST_OPTIONAL__NO_HAS (test_message, value, example_packed_data, submesses_equals)
  submess.test = 0;
  DO_TEST (&submess, test_optional_submess_0);
  submess.test = 42;
  DO_TEST (&submess, test_optional_submess_42);
#undef DO_TEST
}

/* === Oneof type fields === */
TEST(PBCTest, TestEmptyoneof)
{
  Foo__TestMessOneof mess;
  foo__test_mess_oneof__init(&mess);
  size_t len;
  uint8_t *data;
  Foo__TestMessOneof *mess2 = (Foo__TestMessOneof *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(len , 0);
  free (data);
  foo__test_mess_oneof__free_unpacked (NULL, mess2);
}

#define DO_TEST_GENERIC_ONEOF(type, init, free_unpacked, case_member, case_enum, member, value, example_packed_data, assign, equal_func, result_check) \
  do{ \
  type opt;\
  init(&opt); \
  type *mess; \
  size_t len; uint8_t *data; \
  opt.case_member = case_enum; \
  assign(opt.member, value); \
  mess = (type *)test_compare_pack_methods (&opt.base, &len, &data); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  EXPECT_EQ(mess->case_member , case_enum); \
  result_check(mess->member); \
  EXPECT_TRUE (equal_func (mess->member, value)); \
  free_unpacked (NULL, mess); \
  free (data); \
  }while(0)

#define DO_TEST_ONEOF(member, MEMBER, value, example_packed_data, assign, equal_func) \
  DO_TEST_GENERIC_ONEOF(Foo__TestMessOneof, \
                foo__test_mess_oneof__init, \
                foo__test_mess_oneof__free_unpacked, \
                test_oneof_case, \
                FOO__TEST_MESS_ONEOF__TEST_ONEOF_##MEMBER, \
                member, \
                value, example_packed_data, assign, equal_func, CHECK_NONE)

#define DO_TEST_ONEOF_REF_VAL(member, MEMBER, value, example_packed_data, assign, equal_func) \
  DO_TEST_GENERIC_ONEOF(Foo__TestMessOneof, \
                foo__test_mess_oneof__init, \
                foo__test_mess_oneof__free_unpacked, \
                test_oneof_case, \
                FOO__TEST_MESS_ONEOF__TEST_ONEOF_##MEMBER, \
                member, \
                value, example_packed_data, assign, equal_func, CHECK_NOT_NULL)

TEST(PBCTest, TestOneofint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_int32, TEST_INT32, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_int32_min);
  DO_TEST (-1, test_optional_int32_m1);
  DO_TEST (0, test_optional_int32_0);
  DO_TEST (666, test_optional_int32_666);
  DO_TEST (INT32_MAX, test_optional_int32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofsint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_sint32, TEST_SINT32, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_sint32_min);
  DO_TEST (-1, test_optional_sint32_m1);
  DO_TEST (0, test_optional_sint32_0);
  DO_TEST (666, test_optional_sint32_666);
  DO_TEST (INT32_MAX, test_optional_sint32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofsfixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_sfixed32, TEST_SFIXED32, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT32_MIN, test_optional_sfixed32_min);
  DO_TEST (-1, test_optional_sfixed32_m1);
  DO_TEST (0, test_optional_sfixed32_0);
  DO_TEST (666, test_optional_sfixed32_666);
  DO_TEST (INT32_MAX, test_optional_sfixed32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_int64, TEST_INT64, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_int64_min);
  DO_TEST (-1111111111LL, test_optional_int64_m1111111111LL);
  DO_TEST (0, test_optional_int64_0);
  DO_TEST (QUINTILLION, test_optional_int64_quintillion);
  DO_TEST (INT64_MAX, test_optional_int64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofsint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_sint64, TEST_SINT64, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_sint64_min);
  DO_TEST (-1111111111LL, test_optional_sint64_m1111111111LL);
  DO_TEST (0, test_optional_sint64_0);
  DO_TEST (QUINTILLION, test_optional_sint64_quintillion);
  DO_TEST (INT64_MAX, test_optional_sint64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofsfixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_sfixed64, TEST_SFIXED64, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (INT64_MIN, test_optional_sfixed64_min);
  DO_TEST (-1111111111LL, test_optional_sfixed64_m1111111111LL);
  DO_TEST (0, test_optional_sfixed64_0);
  DO_TEST (QUINTILLION, test_optional_sfixed64_quintillion);
  DO_TEST (INT64_MAX, test_optional_sfixed64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofuint32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_uint32, TEST_UINT32, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_uint32_0);
  DO_TEST (669, test_optional_uint32_669);
  DO_TEST (UINT32_MAX, test_optional_uint32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneoffixed32)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_fixed32, TEST_FIXED32, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_fixed32_0);
  DO_TEST (669, test_optional_fixed32_669);
  DO_TEST (UINT32_MAX, test_optional_fixed32_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneofuint64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_uint64, TEST_UINT64, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_uint64_0);
  DO_TEST (669669669669669ULL, test_optional_uint64_669669669669669);
  DO_TEST (UINT64_MAX, test_optional_uint64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneoffixed64)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_fixed64, TEST_FIXED64, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_fixed64_0);
  DO_TEST (669669669669669ULL, test_optional_fixed64_669669669669669);
  DO_TEST (UINT64_MAX, test_optional_fixed64_max);

#undef DO_TEST
}

TEST(PBCTest, TestOneoffloat)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_float, TEST_FLOAT, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (-100, test_optional_float_m100);
  DO_TEST (0, test_optional_float_0);
  DO_TEST (141243, test_optional_float_141243);

#undef DO_TEST
}

TEST(PBCTest, TestOneofdouble)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_double, TEST_DOUBLE, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (-100, test_optional_double_m100);
  DO_TEST (0, test_optional_double_0);
  DO_TEST (141243, test_optional_double_141243);

#undef DO_TEST
}

TEST(PBCTest, TestOneofbool)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_boolean, TEST_BOOLEAN, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (0, test_optional_bool_0);
  DO_TEST (1, test_optional_bool_1);

#undef DO_TEST
}

TEST(PBCTest, TestOneofTestEnumSmall)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_enum_small, TEST_ENUM_SMALL, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (VALUE, test_optional_enum_small_0);
  DO_TEST (OTHER_VALUE, test_optional_enum_small_1);

#undef DO_TEST
}

TEST(PBCTest, TestOneofTestEnum)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF(test_enum, TEST_ENUM, value, example_packed_data, GENERIC_ASSIGN, NUMERIC_EQUALS)

  DO_TEST (VALUE0, test_optional_enum_0);
  DO_TEST (VALUE1, test_optional_enum_1);
  DO_TEST (VALUE127, test_optional_enum_127);
  DO_TEST (VALUE128, test_optional_enum_128);
  DO_TEST (VALUE16383, test_optional_enum_16383);
  DO_TEST (VALUE16384, test_optional_enum_16384);
  DO_TEST (VALUE2097151, test_optional_enum_2097151);
  DO_TEST (VALUE2097152, test_optional_enum_2097152);
  DO_TEST (VALUE268435455, test_optional_enum_268435455);
  DO_TEST (VALUE268435456, test_optional_enum_268435456);

#undef DO_TEST
}

TEST(PBCTest, TestOneofString)
{
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF_REF_VAL (test_string, TEST_STRING, value, example_packed_data, GENERIC_ASSIGN, STRING_EQUALS)
  DO_TEST ((char *)"", test_optional_string_empty);
  DO_TEST ((char *)"hello", test_optional_string_hello);
#undef DO_TEST
}

TEST(PBCTest, TestOneofBytes)
{
  static ProtobufCBinaryData bd_empty = { 0, (uint8_t*)"" };
  static ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
  static ProtobufCBinaryData bd_random = { 5, (uint8_t*)"\1\0\375\2\4" };
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF (test_bytes, TEST_BYTES, value, example_packed_data, GENERIC_ASSIGN, binary_data_equals)
  DO_TEST (bd_empty, test_optional_bytes_empty);
  DO_TEST (bd_hello, test_optional_bytes_hello);
  DO_TEST (bd_random, test_optional_bytes_random);
#undef DO_TEST
}

TEST(PBCTest, TestOneofSubMess)
{
  Foo__SubMess submess;
  foo__sub_mess__init(&submess);
#define DO_TEST(value, example_packed_data) \
  DO_TEST_ONEOF_REF_VAL (test_message, TEST_MESSAGE, value, example_packed_data, GENERIC_ASSIGN, submesses_equals)
  submess.test = 0;
  DO_TEST (&submess, test_optional_submess_0);
  submess.test = 42;
  DO_TEST (&submess, test_optional_submess_42);
#undef DO_TEST
}

TEST(PBCTest, TestOneofMerge)
{
  Foo__TestMessOneof *msg;
#define DO_TEST(value, member, MEMBER, equals_func, example_packed_data) \
  msg = foo__test_mess_oneof__unpack (NULL, sizeof (example_packed_data), example_packed_data); \
  ASSERT_TRUE (msg); \
  EXPECT_EQ(msg->test_oneof_case , FOO__TEST_MESS_ONEOF__TEST_ONEOF_##MEMBER); \
  EXPECT_TRUE (equals_func (msg->member, value)); \
  foo__test_mess_oneof__free_unpacked (NULL, msg);

  DO_TEST (444455555, test_double, TEST_DOUBLE, NUMERIC_EQUALS, test_oneof_merge_double);
  DO_TEST (333, test_float, TEST_FLOAT, NUMERIC_EQUALS, test_oneof_merge_float);
  DO_TEST (666, test_int32, TEST_INT32, NUMERIC_EQUALS, test_oneof_merge_int32);
  DO_TEST ("", test_string, TEST_STRING, STRING_EQUALS, test_oneof_merge_string);

  Foo__SubMess submess;
  foo__sub_mess__init(&submess);
  submess.test = 42;
  DO_TEST (&submess, test_message, TEST_MESSAGE, submesses_equals, test_oneof_merge_submess);

  ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
  DO_TEST(bd_hello, test_bytes, TEST_BYTES, binary_data_equals, test_oneof_merge_bytes);
#undef DO_TEST
}

/* === repeated type fields === */
#define DO_TEST_REPEATED(lc_member_name, cast, \
                         static_array, example_packed_data, \
                         equals_macro) \
  do{ \
  Foo__TestMess mess; \
  foo__test_mess__init(&mess); \
  Foo__TestMess *mess2; \
  size_t len; \
  uint8_t *data; \
  unsigned i; \
  mess.n_##lc_member_name = N_ELEMENTS (static_array); \
  mess.lc_member_name = cast static_array; \
  mess2 = (Foo__TestMess *)test_compare_pack_methods ((ProtobufCMessage*)(&mess), &len, &data); \
  EXPECT_EQ(mess2->n_##lc_member_name , N_ELEMENTS (static_array)); \
  for (i = 0; i < N_ELEMENTS (static_array); i++) \
    EXPECT_TRUE(equals_macro(mess2->lc_member_name[i], static_array[i])); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  free (data); \
  foo__test_mess__free_unpacked (NULL, mess2); \
  }while(0)

TEST(PBCTest, TestEmptyRepeated)
{
  Foo__TestMess mess;
  foo__test_mess__init(&mess);
  size_t len;
  uint8_t *data;
  Foo__TestMess *mess2 = (Foo__TestMess *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(len , 0);
  free (data);
  foo__test_mess__free_unpacked (NULL, mess2);
}

TEST(PBCTest, TestRepeatedint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_int32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_repeated_int32_arr0);
  DO_TEST (int32_arr1, test_repeated_int32_arr1);
  DO_TEST (int32_arr_min_max, test_repeated_int32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedsint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_sint32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_repeated_sint32_arr0);
  DO_TEST (int32_arr1, test_repeated_sint32_arr1);
  DO_TEST (int32_arr_min_max, test_repeated_sint32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedsfixed32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_sfixed32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_repeated_sfixed32_arr0);
  DO_TEST (int32_arr1, test_repeated_sfixed32_arr1);
  DO_TEST (int32_arr_min_max, test_repeated_sfixed32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeateduint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_uint32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (uint32_roundnumbers, test_repeated_uint32_roundnumbers);
  DO_TEST (uint32_0_max, test_repeated_uint32_0_max);

#undef DO_TEST
}



TEST(PBCTest, TestRepeatedfixed32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_fixed32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (uint32_roundnumbers, test_repeated_fixed32_roundnumbers);
  DO_TEST (uint32_0_max, test_repeated_fixed32_0_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedint64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_int64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_repeated_int64_roundnumbers);
  DO_TEST (int64_min_max, test_repeated_int64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedsint64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_sint64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_repeated_sint64_roundnumbers);
  DO_TEST (int64_min_max, test_repeated_sint64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedsfixed64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_sfixed64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_repeated_sfixed64_roundnumbers);
  DO_TEST (int64_min_max, test_repeated_sfixed64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedUint64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_uint64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(uint64_roundnumbers, test_repeated_uint64_roundnumbers);
  DO_TEST(uint64_0_1_max, test_repeated_uint64_0_1_max);
  DO_TEST(uint64_random, test_repeated_uint64_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedfixed64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_fixed64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(uint64_roundnumbers, test_repeated_fixed64_roundnumbers);
  DO_TEST(uint64_0_1_max, test_repeated_fixed64_0_1_max);
  DO_TEST(uint64_random, test_repeated_fixed64_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedFloat)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_float, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(float_random, test_repeated_float_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedDouble)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_double, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(double_random, test_repeated_double_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedBoolean)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_boolean, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(boolean_0, test_repeated_boolean_0);
  DO_TEST(boolean_1, test_repeated_boolean_1);
  DO_TEST(boolean_random, test_repeated_boolean_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedTestEnumSmall)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_enum_small, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(enum_small_0, test_repeated_enum_small_0);
  DO_TEST(enum_small_1, test_repeated_enum_small_1);
  DO_TEST(enum_small_random, test_repeated_enum_small_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedTestEnum)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_enum, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(enum_0, test_repeated_enum_0);
  DO_TEST(enum_1, test_repeated_enum_1);
  DO_TEST(enum_random, test_repeated_enum_random);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedString)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_string, (char **), \
                   static_array, example_packed_data, \
                   STRING_EQUALS)

  DO_TEST(repeated_strings_0, test_repeated_strings_0);
  DO_TEST(repeated_strings_1, test_repeated_strings_1);
  DO_TEST(repeated_strings_2, test_repeated_strings_2);
  DO_TEST(repeated_strings_3, test_repeated_strings_3);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedBytes)
{
  static ProtobufCBinaryData test_binary_data_0[] = {
    { 4, (uint8_t *) "text" },
    { 9, (uint8_t *) "str\1\2\3\4\5\0" },
    { 10, (uint8_t *) "gobble\0foo" }
  };
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_bytes, , \
                   static_array, example_packed_data, \
                   binary_data_equals)

  DO_TEST (test_binary_data_0, test_repeated_bytes_0);

#undef DO_TEST
}

TEST(PBCTest, TestRepeatedSubMess)
{
  static Foo__SubMess submess0;
  foo__sub_mess__init(&submess0);
  static Foo__SubMess submess1;
  foo__sub_mess__init(&submess1);
  static Foo__SubMess submess2;
  foo__sub_mess__init(&submess2);
  static Foo__SubMess *submesses[3] = { &submess0, &submess1, &submess2 };

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_REPEATED(test_message, , \
                   static_array, example_packed_data, \
                   submesses_equals)

  DO_TEST (submesses, test_repeated_submess_0);
  submess0.test = 42;
  submess1.test = -10000;
  submess2.test = 667;
  DO_TEST (submesses, test_repeated_submess_1);

#undef DO_TEST
}

#define DO_TEST_PACKED_REPEATED(lc_member_name, cast, \
                         static_array, example_packed_data, \
                         equals_macro) \
  do{ \
  Foo__TestMessPacked mess;\
  foo__test_mess_packed__init(&mess); \
  Foo__TestMessPacked *mess2; \
  size_t len; \
  uint8_t *data; \
  unsigned i; \
  mess.n_##lc_member_name = N_ELEMENTS (static_array); \
  mess.lc_member_name = cast static_array; \
  mess2 = (Foo__TestMessPacked *)test_compare_pack_methods ((ProtobufCMessage*)(&mess), &len, &data); \
  TEST_VERSUS_STATIC_ARRAY (len, data, example_packed_data); \
  EXPECT_EQ(mess2->n_##lc_member_name, N_ELEMENTS (static_array)); \
  for (i = 0; i < N_ELEMENTS (static_array); i++) \
    EXPECT_TRUE(equals_macro(mess2->lc_member_name[i], static_array[i])); \
  free (data); \
  foo__test_mess_packed__free_unpacked (NULL, mess2); \
  }while(0)

TEST(PBCTest, TestPackedRepeatedint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_int32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_packed_repeated_int32_arr0);
  DO_TEST (int32_arr1, test_packed_repeated_int32_arr1);
  DO_TEST (int32_arr_min_max, test_packed_repeated_int32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedsint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_sint32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_packed_repeated_sint32_arr0);
  DO_TEST (int32_arr1, test_packed_repeated_sint32_arr1);
  DO_TEST (int32_arr_min_max, test_packed_repeated_sint32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedsfixed32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_sfixed32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int32_arr0, test_packed_repeated_sfixed32_arr0);
  DO_TEST (int32_arr1, test_packed_repeated_sfixed32_arr1);
  DO_TEST (int32_arr_min_max, test_packed_repeated_sfixed32_arr_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeateduint32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_uint32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (uint32_roundnumbers, test_packed_repeated_uint32_roundnumbers);
  DO_TEST (uint32_0_max, test_packed_repeated_uint32_0_max);

#undef DO_TEST
}



TEST(PBCTest, TestPackedRepeatedFixed32)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_fixed32, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (uint32_roundnumbers, test_packed_repeated_fixed32_roundnumbers);
  DO_TEST (uint32_0_max, test_packed_repeated_fixed32_0_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedInt64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_int64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_packed_repeated_int64_roundnumbers);
  DO_TEST (int64_min_max, test_packed_repeated_int64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedSint64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_sint64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_packed_repeated_sint64_roundnumbers);
  DO_TEST (int64_min_max, test_packed_repeated_sint64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedSfixed64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_sfixed64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST (int64_roundnumbers, test_packed_repeated_sfixed64_roundnumbers);
  DO_TEST (int64_min_max, test_packed_repeated_sfixed64_min_max);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedUint64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_uint64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(uint64_roundnumbers, test_packed_repeated_uint64_roundnumbers);
  DO_TEST(uint64_0_1_max, test_packed_repeated_uint64_0_1_max);
  DO_TEST(uint64_random, test_packed_repeated_uint64_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedFixed64)
{
#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_fixed64, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(uint64_roundnumbers, test_packed_repeated_fixed64_roundnumbers);
  DO_TEST(uint64_0_1_max, test_packed_repeated_fixed64_0_1_max);
  DO_TEST(uint64_random, test_packed_repeated_fixed64_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedFloat)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_float, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(float_random, test_packed_repeated_float_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedDouble)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_double, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(double_random, test_packed_repeated_double_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedBoolean)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_boolean, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(boolean_0, test_packed_repeated_boolean_0);
  DO_TEST(boolean_1, test_packed_repeated_boolean_1);
  DO_TEST(boolean_random, test_packed_repeated_boolean_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedTestEnumSmall)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_enum_small, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(enum_small_0, test_packed_repeated_enum_small_0);
  DO_TEST(enum_small_1, test_packed_repeated_enum_small_1);
  DO_TEST(enum_small_random, test_packed_repeated_enum_small_random);

#undef DO_TEST
}

TEST(PBCTest, TestPackedRepeatedTestEnum)
{

#define DO_TEST(static_array, example_packed_data) \
  DO_TEST_PACKED_REPEATED(test_enum, , \
                   static_array, example_packed_data, \
                   NUMERIC_EQUALS)

  DO_TEST(enum_0, test_packed_repeated_enum_0);
  DO_TEST(enum_1, test_packed_repeated_enum_1);
  DO_TEST(enum_random, test_packed_repeated_enum_random);

#undef DO_TEST
}


TEST(PBCTest, TestUnknownFields)
{
  static Foo__EmptyMess mess;
  foo__empty_mess__init(&mess);
  static Foo__EmptyMess *mess2;
  ProtobufCMessageUnknownFields unk_buf;
  ProtobufCMessageUnknownField fields[2];
  size_t len; uint8_t *data;

  unk_buf.n_unknown_fields = 2;
  unk_buf.unknown_fields = fields;
  mess.base.unknown_buffer = &unk_buf;

  fields[0].base_.tag = 5454;
  fields[0].serialized.wire_type = PROTOBUF_C_WIRE_TYPE_VARINT;
  fields[0].serialized.len = 2;
  fields[0].serialized.data = (uint8_t*)"\377\1";
  PROTOBUF_C_FLAG_SET(fields[0].base_.unknown_flags, UNKNOWN_UNION_TYPE, PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

  fields[1].base_.tag = 5555;
  fields[1].serialized.wire_type = PROTOBUF_C_WIRE_TYPE_32BIT;
  fields[1].serialized.len = 4;
  fields[1].serialized.data = (uint8_t*)"\4\1\0\0";
  PROTOBUF_C_FLAG_SET(fields[1].base_.unknown_flags, UNKNOWN_UNION_TYPE, PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

  mess2 = (Foo__EmptyMess *)test_compare_pack_methods (&mess.base, &len, &data);
  ASSERT_TRUE(mess2->base.unknown_buffer);
  EXPECT_EQ(mess2->base.unknown_buffer->n_unknown_fields , 2);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].base_.tag , 5454);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].serialized.wire_type , PROTOBUF_C_WIRE_TYPE_VARINT);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].serialized.len , 2);
  EXPECT_FALSE (memcmp (mess2->base.unknown_buffer->unknown_fields[0].serialized.data, fields[0].serialized.data, 2));

  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].base_.tag , 5555);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].serialized.wire_type , PROTOBUF_C_WIRE_TYPE_32BIT);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].serialized.len , 4);
  EXPECT_FALSE (memcmp (mess2->base.unknown_buffer->unknown_fields[1].serialized.data, fields[1].serialized.data, 4));
  TEST_VERSUS_STATIC_ARRAY (len, data, test_unknown_fields_0);
  free (data);
  foo__empty_mess__free_unpacked (NULL, mess2);

  fields[0].base_.tag = 6666;
  fields[0].serialized.wire_type = PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;
  fields[0].serialized.len = 9;
  fields[0].serialized.data = (uint8_t*)"\10xxxxxxxx";
  PROTOBUF_C_FLAG_SET(fields[0].base_.unknown_flags, UNKNOWN_UNION_TYPE, PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

  fields[1].base_.tag = 7777;
  fields[1].serialized.wire_type = PROTOBUF_C_WIRE_TYPE_64BIT;
  fields[1].serialized.len = 8;
  fields[1].serialized.data = (uint8_t*)"\1\1\1\0\0\0\0\0";
  PROTOBUF_C_FLAG_SET(fields[1].base_.unknown_flags, UNKNOWN_UNION_TYPE, PROTOBUF_C_FLAG_UNKNOWN_UNION_TYPE_SERIALIZED);

  mess2 = (Foo__EmptyMess *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(mess2->base.unknown_buffer->n_unknown_fields , 2);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].base_.tag , 6666);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].serialized.wire_type , PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[0].serialized.len , 9);
  EXPECT_FALSE (memcmp (mess2->base.unknown_buffer->unknown_fields[0].serialized.data, fields[0].serialized.data, 9));
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].base_.tag , 7777);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].serialized.wire_type , PROTOBUF_C_WIRE_TYPE_64BIT);
  EXPECT_EQ(mess2->base.unknown_buffer->unknown_fields[1].serialized.len , 8);
  EXPECT_FALSE (memcmp (mess2->base.unknown_buffer->unknown_fields[1].serialized.data, fields[1].serialized.data, 8));
  TEST_VERSUS_STATIC_ARRAY (len, data, test_unknown_fields_1);
  free (data);
  foo__empty_mess__free_unpacked (NULL, mess2);
}

static void
test_enum_descriptor (const ProtobufCEnumDescriptor *desc)
{
  unsigned i;
  for (i = 0; i < desc->n_values; i++)
    {
      const ProtobufCEnumValue *sv = desc->values + i;
      const ProtobufCEnumValue *vv;
      const ProtobufCEnumValue *vn;
      vv = protobuf_c_enum_descriptor_get_value (desc, sv->value);
      vn = protobuf_c_enum_descriptor_get_value_by_name (desc, sv->name);
      EXPECT_EQ(sv, vv);
      EXPECT_EQ(sv , vn);
    }
  for (i = 0; i < desc->n_value_names; i++)
    {
      const char *name = desc->values_by_name[i].name;
      const ProtobufCEnumValue *v;
      v = protobuf_c_enum_descriptor_get_value_by_name (desc, name);
      ASSERT_TRUE (v);
    }
}
static void
test_enum_by_name (const ProtobufCEnumDescriptor *desc,
                   const char *name,
                   int expected_value)
{
  const ProtobufCEnumValue *v;
  v = protobuf_c_enum_descriptor_get_value_by_name (desc, name);
  ASSERT_TRUE (v);
  EXPECT_EQ(v->value, expected_value);
}

TEST(PBCTest, TestEnumLookups)
{
  test_enum_descriptor (&foo__test_enum__descriptor);
  test_enum_descriptor (&foo__test_enum_small__descriptor);
  test_enum_descriptor (&foo__test_enum_dup_values__descriptor);
#define TEST_ENUM_DUP_VALUES(str, shortname) \
  test_enum_by_name (&foo__test_enum_dup_values__descriptor,  \
                     str, shortname)
  TEST_ENUM_DUP_VALUES ("VALUE_A", VALUE_A);
  TEST_ENUM_DUP_VALUES ("VALUE_B", VALUE_B);
  TEST_ENUM_DUP_VALUES ("VALUE_C", VALUE_C);
  TEST_ENUM_DUP_VALUES ("VALUE_D", VALUE_D);
  TEST_ENUM_DUP_VALUES ("VALUE_E", VALUE_E);
  TEST_ENUM_DUP_VALUES ("VALUE_F", VALUE_F);
  TEST_ENUM_DUP_VALUES ("VALUE_AA", VALUE_AA);
  TEST_ENUM_DUP_VALUES ("VALUE_BB", VALUE_BB);
#undef TEST_ENUM_DUP_VALUES
}

static void
test_message_descriptor (const ProtobufCMessageDescriptor *desc)
{
  unsigned i;
  for (i = 0; i < desc->n_fields; i++)
    {
      const ProtobufCFieldDescriptor *f = desc->fields + i;
      const ProtobufCFieldDescriptor *fv;
      const ProtobufCFieldDescriptor *fn;
      fv = protobuf_c_message_descriptor_get_field (desc, f->id);
      fn = protobuf_c_message_descriptor_get_field_by_name (desc, f->name);
      EXPECT_EQ(f, fv);
      EXPECT_EQ(f, fn);
    }
}

TEST(PBCTest, TestMessageLookups)
{
  test_message_descriptor (&foo__test_mess__descriptor);
  test_message_descriptor (&foo__test_mess_optional__descriptor);
  test_message_descriptor (&foo__test_mess_required_enum__descriptor);
}

static void
assert_required_default_values_are_default (Foo__DefaultRequiredValues *mess)
{
  EXPECT_EQ(mess->v_int32, -42);
  EXPECT_EQ(mess->v_uint32, 666);
  EXPECT_EQ(mess->v_int64, 100000);
  EXPECT_EQ(mess->v_uint64, 100001);
  EXPECT_EQ(mess->v_float, 2.5);
  EXPECT_EQ(mess->v_double, 4.5);
  EXPECT_STREQ (mess->v_string, "hi mom\n");
  EXPECT_EQ(mess->v_bytes.len, /* a */ 1
                               + /* space */ 1
                               + /* NUL */ 1
                               + /* space */ 1
                               + /* "character" */ 9);
  EXPECT_FALSE (memcmp (mess->v_bytes.data, "a \0 character", 13));
}


TEST(PBCTest, TestRequiredDefaultValues)
{
  Foo__DefaultRequiredValues mess;
  foo__default_required_values__init(&mess);
  Foo__DefaultRequiredValues *mess2;
  size_t len; uint8_t *data;
  assert_required_default_values_are_default (&mess);
  mess2 = (Foo__DefaultRequiredValues *)test_compare_pack_methods (&mess.base, &len, &data);
  free (data);
  assert_required_default_values_are_default (mess2);
  foo__default_required_values__free_unpacked (NULL, mess2);
}

static void
assert_optional_default_values_are_default (Foo__DefaultOptionalValues *mess)
{
  EXPECT_FALSE (mess->has_v_int32);
  EXPECT_EQ(mess->v_int32, -42);
  EXPECT_FALSE (mess->has_v_uint32);
  EXPECT_EQ(mess->v_uint32, 666);
  EXPECT_FALSE (mess->has_v_int64);
  EXPECT_EQ(mess->v_int64, 100000);
  EXPECT_FALSE (mess->has_v_uint64);
  EXPECT_EQ(mess->v_uint64, 100001);
  EXPECT_FALSE (mess->has_v_float);
  EXPECT_EQ(mess->v_float, 2.5);
  EXPECT_FALSE (mess->has_v_double);
  EXPECT_EQ(mess->v_double, 4.5);
  EXPECT_STREQ (mess->v_string, "hi mom\n");
  EXPECT_FALSE (mess->has_v_bytes);
  EXPECT_EQ(mess->v_bytes.len, /* a */ 1
                               + /* space */ 1
                               + /* NUL */ 1
                               + /* space */ 1
                               + /* "character" */ 9);
  EXPECT_FALSE(memcmp (mess->v_bytes.data, "a \0 character", 13));
}

TEST(PBCTest, TestOptionalDefaultValues)
{
  Foo__DefaultOptionalValues mess;
  foo__default_optional_values__init(&mess);
  Foo__DefaultOptionalValues *mess2;
  size_t len; uint8_t *data;
  assert_optional_default_values_are_default (&mess);
  mess2 = (Foo__DefaultOptionalValues *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(len, 0);            /* no non-default values */
  free (data);
  assert_optional_default_values_are_default (mess2);
  foo__default_optional_values__free_unpacked (NULL, mess2);
}

static void
assert_optional_lowercase_enum_default_value_is_default (Foo__LowerCase *mess)
{
  EXPECT_FALSE(mess->has_value);
  EXPECT_EQ(mess->value, 2);
}

TEST(PBCTest, TestOptionalLowercaseEnumDefaultValue)
{
  Foo__LowerCase mess;
  foo__lower_case__init(&mess);
  Foo__LowerCase *mess2;
  size_t len; uint8_t *data;
  assert_optional_lowercase_enum_default_value_is_default (&mess);
  mess2 = (Foo__LowerCase *)test_compare_pack_methods (&mess.base, &len, &data);
  EXPECT_EQ(len, 0);            /* no non-default values */
  free (data);
  assert_optional_lowercase_enum_default_value_is_default (mess2);
  foo__lower_case__free_unpacked (NULL, mess2);
}

TEST(PBCTest, TestFieldMerge)
{
  Foo__TestMessOptional msg1;
  foo__test_mess_optional__init(&msg1);
  Foo__SubMess sub1;
  foo__sub_mess__init(&sub1);
  Foo__SubMess__SubSubMess subsub1;
  foo__sub_mess__sub_sub_mess__init(&subsub1);

  msg1.has_test_int32 = 1;
  msg1.test_int32 = 12345;
  msg1.has_test_sint32 = 1;
  msg1.test_sint32 = -12345;
  msg1.has_test_int64 = 1;
  msg1.test_int64 = 232;
  msg1.test_string = (char *)"123";
  msg1.test_message = &sub1;
  sub1.has_val1 = 1;
  sub1.val1 = 4;
  sub1.has_val2 = 1;
  sub1.val2 = 5;
  int32_t arr1[] = {0, 1};
  sub1.n_rep = 2;
  sub1.rep = arr1;
  sub1.sub1 = &subsub1;
  sub1.sub2 = &subsub1;
  subsub1.n_rep = 2;
  subsub1.rep = arr1;
  subsub1.str1 = (char *)"test";

  size_t msg_size = foo__test_mess_optional__get_packed_size (NULL, &msg1);

  Foo__TestMessOptional msg2;
  foo__test_mess_optional__init(&msg2);
  Foo__SubMess sub2;
  foo__sub_mess__init(&sub2);
  Foo__SubMess__SubSubMess subsub2;
  foo__sub_mess__sub_sub_mess__init(&subsub2);

  msg2.has_test_int64 = 1;
  msg2.test_int64 = 2;
  msg2.has_test_enum = 1;
  msg2.test_enum = VALUE128;
  msg2.test_string = (char *)"456";
  msg2.test_message = &sub2;
  sub2.has_val2 = 1;
  sub2.val2 = 666;
  int32_t arr2[] = {2, 3, 4};
  sub2.n_rep = 3;
  sub2.rep = arr2;
  sub2.sub1 = &subsub2;
  subsub2.has_val1 = 1;
  subsub2.val1 = 1;

  msg_size += foo__test_mess_optional__get_packed_size (NULL, &msg2);

  uint8_t *packed_buffer = (uint8_t *)malloc (msg_size);

  size_t packed_size = foo__test_mess_optional__pack (NULL, &msg1, packed_buffer);
  packed_size += foo__test_mess_optional__pack (NULL, &msg2, packed_buffer + packed_size);

  EXPECT_EQ(packed_size, msg_size);

  Foo__TestMessOptional *merged = foo__test_mess_optional__unpack
      (NULL, msg_size, packed_buffer);

  EXPECT_TRUE(merged->has_test_int32);
  EXPECT_EQ(merged->test_int32, msg1.test_int32);
  EXPECT_TRUE(merged->has_test_sint32);
  EXPECT_EQ(merged->test_sint32, msg1.test_sint32);
  EXPECT_TRUE(merged->has_test_int64);
  EXPECT_EQ(merged->test_int64, msg2.test_int64);
  EXPECT_TRUE(merged->has_test_enum);
  EXPECT_EQ(merged->test_enum, msg2.test_enum);
  EXPECT_STREQ(merged->test_string, msg2.test_string);
  EXPECT_TRUE(merged->test_message->has_val1);
  EXPECT_EQ(merged->test_message->val1, sub1.val1);
  EXPECT_TRUE(merged->test_message->has_val2);
  EXPECT_EQ(merged->test_message->val2, sub2.val2);
  /* Repeated fields should get concatenated */
  int32_t merged_arr[] = {0, 1, 2, 3, 4};
  EXPECT_EQ(merged->test_message->n_rep, 5);
  EXPECT_FALSE(memcmp(merged->test_message->rep, merged_arr, sizeof(merged_arr)));

  EXPECT_EQ(merged->test_message->sub1->val1, subsub2.val1);
  EXPECT_FALSE(memcmp(merged->test_message->sub1->bytes1.data,
                 subsub2.bytes1.data, subsub2.bytes1.len));
  EXPECT_STREQ(merged->test_message->sub1->str1, subsub1.str1);
  EXPECT_EQ(merged->test_message->sub1->n_rep, subsub1.n_rep);
  EXPECT_FALSE(memcmp(merged->test_message->sub1->rep, arr1, sizeof(arr1)));

  EXPECT_FALSE (merged->test_message->sub2->has_val1);
  EXPECT_EQ(merged->test_message->sub2->val1, subsub1.val1);

  free (packed_buffer);
  foo__test_mess_optional__free_unpacked (NULL, merged);
}

TEST(PBCTest, TestSubmessageMerge)
{
  Foo__TestMessSubMess *merged;
  size_t size;
  uint8_t *packed;

  merged = foo__test_mess_sub_mess__unpack
      (NULL, sizeof (test_submess_unmerged1), test_submess_unmerged1);

  size = foo__test_mess_sub_mess__get_packed_size(NULL, merged);
  packed = (uint8_t *)malloc (size);
  foo__test_mess_sub_mess__pack (NULL, merged, packed);

  EXPECT_EQ(size, sizeof (test_submess_merged1));
  EXPECT_FALSE(memcmp (packed, test_submess_merged1, size));

  foo__test_mess_sub_mess__free_unpacked (NULL, merged);
  free (packed);

  merged = foo__test_mess_sub_mess__unpack
      (NULL, sizeof (test_submess_unmerged2), test_submess_unmerged2);

  size = foo__test_mess_sub_mess__get_packed_size(NULL, merged);
  packed = (uint8_t *)malloc (size);
  foo__test_mess_sub_mess__pack (NULL, merged, packed);

  EXPECT_EQ(size, sizeof (test_submess_merged2));
  EXPECT_FALSE (memcmp (packed, test_submess_merged2, size));

  foo__test_mess_sub_mess__free_unpacked (NULL, merged);
  free (packed);
}

static struct alloc_data {
  uint32_t alloc_count;
  int32_t allocs_left;
} test_allocator_data;

static void *test_alloc(ProtobufCInstance *allocator_data, size_t size)
{
  struct alloc_data *ad = (struct alloc_data *)allocator_data->data;
  void *rv = NULL;
  if (ad->allocs_left-- > 0)
      rv = malloc (size);
  /* fprintf (stderr, "alloc %d = %p\n", size, rv); */
  if (rv)
    ad->alloc_count++;
  return rv;
}

static void test_free (ProtobufCInstance *allocator_data, void *data)
{
  struct alloc_data *ad = (struct alloc_data *)allocator_data->data;
  /* fprintf (stderr, "free %p\n", data); */
  free (data);
  if (data)
    ad->alloc_count--;
}

static ProtobufCInstance test_allocator = {
	.alloc = test_alloc,
	.zalloc = NULL,
	.realloc = NULL,
	.free = test_free,
	.error = NULL,
	.data = &test_allocator_data,
};

#define SETUP_TEST_ALLOC_BUFFER(pbuf, len)					\
  uint8_t bytes[] = "some bytes", *pbuf;						\
  size_t len, _len2;                                        \
  Foo__DefaultRequiredValues _req; \
  foo__default_required_values__init(&_req);		\
  Foo__AllocValues _mess; \
  foo__alloc_values__init(&_mess);				\
  _mess.a_string = (char *)"some string";						\
  _mess.r_string = repeated_strings_2;						\
  _mess.n_r_string = sizeof(repeated_strings_2) / sizeof(*repeated_strings_2);	\
  _mess.a_bytes.len = sizeof(bytes);						\
  _mess.a_bytes.data = bytes;							\
  _mess.a_mess = &_req;								\
  len = foo__alloc_values__get_packed_size (NULL, &_mess);			\
  pbuf = (uint8_t *)malloc (len);							\
  ASSERT_TRUE (pbuf);								\
  _len2 = foo__alloc_values__pack (NULL, &_mess, pbuf);			\
  EXPECT_EQ(len, _len2);

static void
test_alloc_graceful_cleanup (uint8_t *packed, size_t len, int good_allocs)
{
  Foo__AllocValues *mess;
  test_allocator_data.alloc_count = 0;
  test_allocator_data.allocs_left = good_allocs;
  mess = foo__alloc_values__unpack (&test_allocator, len, packed);
  EXPECT_TRUE (test_allocator_data.allocs_left < 0 ? !mess : !!mess);
  if (mess)
    foo__alloc_values__free_unpacked (&test_allocator, mess);
  EXPECT_EQ(test_allocator_data.alloc_count, 0);
}

TEST(PBCTest, TestAllocFreeAll)
{
  SETUP_TEST_ALLOC_BUFFER (packed, len);
  test_alloc_graceful_cleanup (packed, len, INT32_MAX);
  free (packed);
}

/* TODO: test alloc failure for slab, unknown fields */
TEST(PBCTest, TestAllocFail)
{
  int i = 0;
  SETUP_TEST_ALLOC_BUFFER (packed, len);
  do ASSERT_DEATH(test_alloc_graceful_cleanup (packed, len, i++), "");
  while (test_allocator_data.allocs_left < 0);
  free (packed);
}

/* This test checks that protobuf decoder is capable of detecting special
   cases of incomplete messages. The message should have at least two required
   fields field1 and field129 with positions pos1 and pos2 (no matter what the
   field numbers are), such as (pos1 % 128) , (pos2 % 128). The decoder must
   return NULL instead of incomplete message with field129 missing. */
TEST(PBCTest, TestRequiredFieldsBitmap)
{
  const uint8_t source[] = {
    (1 << 3) | PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED,
    sizeof("hello") - 1,
    'h', 'e', 'l', 'l', 'o'
  };
  Foo__TestRequiredFieldsBitmap *msg;
  msg = foo__test_required_fields_bitmap__unpack(NULL, sizeof(source), source);
  EXPECT_FALSE (msg);
}

TEST(PBCTest, TestFieldFlags)
{
  const ProtobufCFieldDescriptor *f;

  f = protobuf_c_message_descriptor_get_field_by_name(
    &foo__test_field_flags__descriptor, "no_flags1");
  ASSERT_TRUE(f);

  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_PACKED));
  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_DEPRECATED));

  f = protobuf_c_message_descriptor_get_field_by_name(
    &foo__test_field_flags__descriptor, "no_flags2");
  ASSERT_TRUE(f);

  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_PACKED));
  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_DEPRECATED));

  f = protobuf_c_message_descriptor_get_field_by_name(
    &foo__test_field_flags__descriptor, "no_flags3");
  ASSERT_TRUE(f);

  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_PACKED));
  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_DEPRECATED));

  f = protobuf_c_message_descriptor_get_field_by_name(
    &foo__test_field_flags__descriptor, "packed");
  ASSERT_TRUE(f);

  EXPECT_TRUE((f->flags & PROTOBUF_C_FIELD_FLAG_PACKED));
  EXPECT_FALSE((f->flags & PROTOBUF_C_FIELD_FLAG_DEPRECATED));

  f = protobuf_c_message_descriptor_get_field_by_name(
    &foo__test_field_flags__descriptor, "packed_deprecated");
  ASSERT_TRUE(f);

  EXPECT_TRUE((f->flags & PROTOBUF_C_FIELD_FLAG_PACKED));
  EXPECT_TRUE((f->flags & PROTOBUF_C_FIELD_FLAG_DEPRECATED));
}

TEST(PBCTest, TestMessageCheck)
{
  Foo__TestMessageCheck__SubMessage sm;
  foo__test_message_check__sub_message__init(&sm);
  Foo__TestMessageCheck__SubMessage sm2;
  foo__test_message_check__sub_message__init(&sm2);
  Foo__TestMessageCheck m;
  foo__test_message_check__init(&m);
  char *null = NULL;
  char *str = (char *)"";
  Foo__TestMessageCheck__SubMessage *sm_p;
  ProtobufCBinaryData bd;

  /* test incomplete submessage */
  EXPECT_FALSE(protobuf_c_message_check(NULL, &sm.base));

  /* test complete submessage */
  sm.str = str;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &sm.base));

  /* test just initialized message */
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with required_string not set */
  m.required_string = NULL;
  m.required_msg = &sm;
  m.required_bytes.data = (uint8_t *)str; m.required_bytes.len = 1;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with required_msg not set */
  m.required_string = str;
  m.required_msg = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with required_bytes set incorrectly */
  m.required_msg = &sm;
  m.required_bytes.data = NULL; m.required_bytes.len = 1;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with all required fields set */
  m.required_bytes.data = (uint8_t *)str; m.required_bytes.len = 1;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with incomplete required submessage */
  sm.str = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with incomplete optional submessage */
  sm.str = str;
  m.optional_msg = &sm2;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with complete optional submessage */
  sm2.str = str;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with correct optional bytes */
  m.has_optional_bytes = 1;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with incorrect optional bytes */
  m.optional_bytes.data = NULL; m.optional_bytes.len = 1;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with correct optional bytes */
  m.optional_bytes.data = (uint8_t *)str; m.optional_bytes.len = 1;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated strings set incorrectly */
  m.n_repeated_string = 1;
  m.repeated_string = &null;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated strings set correctly */
  m.repeated_string = (char **)(&str);
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated submessage set incorrectly */
  sm_p = NULL;
  m.n_repeated_msg = 1;
  m.repeated_msg = &sm_p;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated incomplete submessage */
  sm_p = &sm;
  sm.str = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated complete submessage */
  sm.str = str;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated bytes set incorrectly */
  bd.len = 1;
  bd.data = NULL;
  m.n_repeated_bytes = 1;
  m.repeated_bytes = &bd;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with repeated bytes set correctly */
  bd.data = (uint8_t *)str;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));

  /* test with empty repeated string vector */
  m.repeated_string = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with empty repeated bytes vector */
  m.repeated_string = (char **)&str;
  m.repeated_bytes = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test with empty repeated submessage vector */
  m.repeated_bytes = &bd;
  m.repeated_msg = NULL;
  EXPECT_FALSE(protobuf_c_message_check(NULL, &m.base));

  /* test all vectors set again */
  m.repeated_msg = &sm_p;
  EXPECT_TRUE(protobuf_c_message_check(NULL, &m.base));
}
