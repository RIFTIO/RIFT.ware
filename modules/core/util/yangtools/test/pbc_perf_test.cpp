
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
 * @file ks_rrmerge_test.cpp
 * @author Sujithra Periasamy
 * @date 2015/11/04
 * @brief ProtobufC functions perf long running tests.
 */

#include <limits.h>
#include <stdlib.h>

#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_schema.pb-c.h"
#include "rw-fpath-d.pb-c.h"

#include <chrono>
using namespace std::chrono;

static void gen_random_string(char *s, size_t len)
{
  static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  for (size_t i = 0; i < (len-1); ++i) {
     s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  s[len-1] = 0;
}

static void gen_random_bytes(uint8_t *b, size_t len)
{
  for (size_t i = 0; i < len; ++i) {
    b[i] = rand()%sizeof(uint8_t);
  }
}

static void fill_pbcm(ProtobufCMessage* msg, int dyn_count)
{
  auto mdesc = msg->descriptor;
  for (unsigned i = 0; i < mdesc->n_fields; ++i) {

    auto fdesc = &mdesc->fields[i];
    size_t n_elems = dyn_count;
    size_t strsz = 128;

    if (fdesc->label == PROTOBUF_C_LABEL_REPEATED) {
      if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
        n_elems = fdesc->rw_inline_max;
      }
    } else {
      n_elems = 1;
    }

    switch (fdesc->type) {

      case PROTOBUF_C_TYPE_MESSAGE:
        for (unsigned n = 0; n < n_elems; ++n) {
          ProtobufCMessage *inner_msg = nullptr;
          protobuf_c_message_set_field_message(NULL, msg, fdesc, &inner_msg);
          fill_pbcm(inner_msg, dyn_count);
        }
        break;

      case PROTOBUF_C_TYPE_BOOL:
        protobuf_c_message_set_field_text_value(NULL, msg, fdesc, "true", strlen("true"));
        break;

      case PROTOBUF_C_TYPE_ENUM:
        protobuf_c_message_set_field_text_value(NULL, msg, fdesc, fdesc->enum_desc->values_by_name[0].name,
                                                strlen(fdesc->enum_desc->values_by_name[0].name));
        break;

      case PROTOBUF_C_TYPE_STRING: {
        if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
          strsz = fdesc->data_size;
        }

        for (unsigned n = 0; n < n_elems; ++n) {
          char string[strsz];
          gen_random_string(string, strsz);
          protobuf_c_message_set_field_text_value(NULL, msg, fdesc, string, strlen(string));
        }
        break;
      }

      case PROTOBUF_C_TYPE_BYTES: {
        if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
          strsz = fdesc->data_size;
        }

        uint8_t data[strsz];
        gen_random_bytes(data, strsz);
        protobuf_c_message_set_field_text_value(NULL, msg, fdesc, (char *)data, strsz);
        break;
      }

      default:
        for (unsigned n = 0; n < n_elems; ++n) {
          if (fdesc->rw_flags & RW_PROTOBUF_FOPT_KEY) {
            std::string key = std::to_string(rand());
            protobuf_c_message_set_field_text_value(NULL, msg, fdesc, key.c_str(), key.length());
          } else {
            protobuf_c_message_set_field_text_value(NULL, msg, fdesc, "1234567", strlen("1234567"));
          }
        }
    }
  }
}

static void fill_pbcms_for_merge(ProtobufCMessage* msg1, ProtobufCMessage* msg2, int dyn_count)
{
  auto mdesc = msg1->descriptor;

  for (unsigned i = 0; i < mdesc->n_fields; ++i) {

    auto fdesc = &mdesc->fields[i];
    size_t n_elems = dyn_count;
    size_t strsz = 128;

    if (fdesc->label == PROTOBUF_C_LABEL_REPEATED) {
      if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
        n_elems = fdesc->rw_inline_max;
      }
    } else {
      n_elems = 1;
    }

    switch (fdesc->type) {

      case PROTOBUF_C_TYPE_MESSAGE:

        if (!(fdesc->msg_desc->rw_flags & RW_PROTOBUF_MOPT_HAS_KEYS)) {
          if (n_elems > 1) {
            n_elems /= 2;
          }
        }

        for (unsigned n = 0; n < n_elems; ++n) {
          ProtobufCMessage *inner_msg1 = nullptr, *inner_msg2 = nullptr;
          protobuf_c_message_set_field_message(NULL, msg1, fdesc, &inner_msg1);

          protobuf_c_message_set_field_message(NULL, msg2, fdesc, &inner_msg2);
          fill_pbcms_for_merge(inner_msg1, inner_msg2, dyn_count);
        }
        break;

      case PROTOBUF_C_TYPE_BOOL:
        protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, "true", strlen("true"));
        protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, "true", strlen("true"));
        break;

      case PROTOBUF_C_TYPE_ENUM:
        protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, fdesc->enum_desc->values_by_name[0].name,
                                                strlen(fdesc->enum_desc->values_by_name[0].name));

        protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, fdesc->enum_desc->values_by_name[0].name,
                                                strlen(fdesc->enum_desc->values_by_name[0].name));
        break;

      case PROTOBUF_C_TYPE_STRING: {
        if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
          strsz = fdesc->data_size;
        }

        if (n_elems > 1) { n_elems /= 2; }

        for (unsigned n = 0; n < n_elems; ++n) {
          char string[strsz];
          gen_random_string(string, strsz);
          protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, string, strlen(string));
          protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, string, strlen(string));
        }

        break;
      }

      case PROTOBUF_C_TYPE_BYTES: {
        if (fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
          strsz = fdesc->data_size;
        }

        uint8_t data[strsz];
        gen_random_bytes(data, strsz);
        protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, (char *)data, strsz);
        protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, (char *)data, strsz);
        break;
      }

      default:
        if (n_elems > 1) { n_elems /= 2; }
        for (unsigned n = 0; n < n_elems; ++n) {
          if (fdesc->rw_flags & RW_PROTOBUF_FOPT_KEY) {
            std::string key = std::to_string(rand());
            protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, key.c_str(), key.length());
            protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, key.c_str(), key.length());
          } else {
            protobuf_c_message_set_field_text_value(NULL, msg1, fdesc, "1234567", strlen("1234567"));
            protobuf_c_message_set_field_text_value(NULL, msg2, fdesc, "1234567", strlen("1234567"));
          }
        }
    }
  }
}

TEST(ProtobufCPerf, PackedSize)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, msg);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg.base);

  fill_pbcm(&msg.base, 10);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  size_t size = protobuf_c_message_get_packed_size(NULL, &msg.base);
  ASSERT_TRUE(size);

  t2 = high_resolution_clock::now();
  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();

  std::cout << "protobuf_c_message_get_packed_size; size = " << size 
            << " Duration = " << dur << "(ms)" << std::endl;
}

TEST(ProtobufCPerf, Pack)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, msg);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg.base);

  fill_pbcm(&msg.base, 10);

  size_t size = protobuf_c_message_get_packed_size(NULL, &msg.base);
  ASSERT_TRUE(size);

  uint8_t *data = (uint8_t *)protobuf_c_instance_alloc(NULL, size);
  UniquePtrProtobufCFree<>::uptr_t uptr1(data);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  size_t ret_size = protobuf_c_message_pack(NULL, &msg.base, data);

  t2 = high_resolution_clock::now();
  ASSERT_EQ(size, ret_size);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "protobuf_c_message_pack; size = " << ret_size 
            << " Duration = " << dur << "(ms)" << std::endl;
}

TEST(ProtobufCPerf, UnPack)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, msg);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg.base);

  fill_pbcm(&msg.base, 10);

  size_t size = protobuf_c_message_get_packed_size(NULL, &msg.base);
  ASSERT_TRUE(size);

  uint8_t *data = (uint8_t *)protobuf_c_instance_alloc(NULL, size);
  UniquePtrProtobufCFree<>::uptr_t uptr1(data);

  size_t ret_size = protobuf_c_message_pack(NULL, &msg.base, data);
  ASSERT_EQ(ret_size, size);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  auto out = protobuf_c_message_unpack(NULL, msg.base.descriptor, ret_size, data);

  t2 = high_resolution_clock::now();
  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<>::uptr_t uptr2(out);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "protobuf_c_message_unpack; size = " << ret_size 
            << " Duration = " << dur << "(ms)" << std::endl;
}

TEST(ProtobufCPerf, Merge)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, msg1);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, msg2);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg1.base);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg2.base);

  fill_pbcms_for_merge(&msg1.base, &msg2.base, 10);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  protobuf_c_boolean ret = protobuf_c_message_merge(NULL, &msg1.base, &msg2.base);

  t2 = high_resolution_clock::now();
  ASSERT_TRUE(ret);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "protobuf_c_message_merge; Duration = " << dur << "(ms)" << std::endl;
}

RWUT_INIT();
