
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "test.pb-c.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "gtest/rw_gtest.h"
#include "rwut.h"
#include "rwlib.h"

/*
 * ATTN:- Do not add any new testcases to this file.
 * These are protobuf-c  unittests converted to gtests
 */

TEST(PBCTest, TestGeneratedCode)
{
  Foo__Person person;
  foo__person__init(&person);
  Foo__Person *person2;
  unsigned char simple_pad[8];
  size_t size, size2;
  unsigned char *packed;
  ProtobufCBufferSimple bs = PROTOBUF_C_BUFFER_SIMPLE_INIT (NULL, simple_pad);

  person.name = (char *)"dave b";
  person.id = 42;
  size = foo__person__get_packed_size (NULL, &person);
  packed = (unsigned char*)malloc(size);
  ASSERT_TRUE(packed);
  size2 = foo__person__pack (NULL, &person, packed);
  EXPECT_EQ(size, size2);
  foo__person__pack_to_buffer (&person, &bs.base);
  EXPECT_EQ(bs.len, size);
  EXPECT_FALSE(memcmp (bs.data, packed, size));
  PROTOBUF_C_BUFFER_SIMPLE_CLEAR (&bs);
  person2 = foo__person__unpack (NULL, size, packed);
  ASSERT_TRUE(person2);
  EXPECT_EQ(person2->id, 42);
  EXPECT_STREQ(person2->name, "dave b");

  foo__person__free_unpacked (NULL, person2);
  free (packed);
}
