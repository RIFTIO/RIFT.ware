/* STANDARD_RIFT_IO_COPYRIGHT */
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
#include "test-ydom-top.pb-c.h"

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

TEST(KeyspecPerf, ReRoot)
{
  // In KS and MSG
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TestRwapis_data_Config, msg);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg.base);

  fill_pbcm(&msg.base, 6);
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_TestRwapis_data_Config, ks);

  // Out KS and MSG
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwvm_EventList_Event_Action_Start_Dl1_Dc1_Dc2_Dl2_Dc3_Dl3, oks);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  rw_keyspec_path_reroot_iter_state_t state;
  rw_keyspec_path_reroot_iter_init(&ks.rw_keyspec_path_t, nullptr , &state, &msg.base, &oks.rw_keyspec_path_t);
  unsigned i = 0;
  while(rw_keyspec_path_reroot_iter_next(&state)) {
    i++;
  }
  rw_keyspec_path_reroot_iter_done(&state);
  t2 = high_resolution_clock::now();

  ASSERT_TRUE(i);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_reroot_iter: msg count = " << i
            << " Duration = " << dur << "(ms)" << std::endl;
}

#define LONG_PATH TestYdomTop_TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwvm_EventList_Event_Action_Start_Dl1_Dc1_Dc2_Dl2_Dc3_Dl3

static void fill_keys_to_ks(RWPB_T_PATHSPEC(LONG_PATH)* ks)
{
  ks->dompath.path002.has_key00 = true;
  ks->dompath.path002.key00.name = strdup("profile");
  ks->dompath.path005.has_key00 = true;
  ks->dompath.path005.key00.component_name = strdup("component");
  ks->dompath.path008.has_key00 = true;
  ks->dompath.path008.key00.name = strdup("event");
  ks->dompath.path009.has_key00 = true;
  ks->dompath.path009.key00.name = strdup("action");
  ks->dompath.path011.has_key00 = true;
  strcpy(ks->dompath.path011.key00.mykey1, "key1");
  ks->dompath.path011.has_key01 = true;
  ks->dompath.path011.key01.mykey2 = 100;
  ks->dompath.path011.has_key02 = true;
  ks->dompath.path011.key02.mykey3 = 200;
  ks->dompath.path011.has_key03 = true;
  strcpy(ks->dompath.path011.key03.mykey4, "key4");
  ks->dompath.path011.has_key04 = true;
  strcpy(ks->dompath.path011.key04.mykey5, "key5");
  ks->dompath.path011.has_key05 = true;
  strcpy(ks->dompath.path011.key05.mykey6, "key6");
  ks->dompath.path011.has_key06 = true;
  ks->dompath.path011.key06.mykey7 = 500;
  ks->dompath.path014.has_key00 = true;
  ks->dompath.path014.key00.keyl2 = strdup("abc");
}

TEST(KeyspecPerf, ReRootMerge)
{
  // In KS and MSG
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TestRwapis_data_Config, msg1);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg1.base);

  fill_pbcm(&msg1.base, 10);
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_TestRwapis_data_Config, ks1);

  // Out KS and MSG
  RWPB_M_MSG_DECL_INIT(LONG_PATH, msg2);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg2.base);
  fill_pbcm(&msg2.base, 10);

  RWPB_M_PATHSPEC_DECL_INIT(LONG_PATH, ks2);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr2(&ks2.rw_keyspec_path_t);
  fill_keys_to_ks(&ks2);

  rw_keyspec_path_t *out_ks = nullptr;

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  auto out = rw_keyspec_path_reroot_and_merge(nullptr, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t, &msg1.base, &msg2.base, &out_ks);

  t2 = high_resolution_clock::now();

  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<>::uptr_t uptr3(out);
  ASSERT_TRUE(out_ks);
  UniquePtrKeySpecPath::uptr_t uptr4(out_ks);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_reroot_and_merge: Duration = " 
            << dur << "(ms)" << std::endl;
}

TEST(KeyspecPerf, ReRootMergeOpaque)
{
  // In KS and MSG
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TestRwapis_data_Config, msg1);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg1.base);

  fill_pbcm(&msg1.base, 10);
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_TestRwapis_data_Config, ks1);

  // Out KS and MSG
  RWPB_M_MSG_DECL_INIT(LONG_PATH, msg2);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr1(&msg2.base);
  fill_pbcm(&msg2.base, 10);

  RWPB_M_PATHSPEC_DECL_INIT(LONG_PATH, ks2);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr2(&ks2.rw_keyspec_path_t);
  fill_keys_to_ks(&ks2);

  ProtobufCBinaryData dmsg1 = {}, dmsg2 = {};
  dmsg1.data = protobuf_c_message_serialize(NULL, &msg1.base, &dmsg1.len);
  ASSERT_TRUE(dmsg1.data);
  UniquePtrProtobufCFree<>::uptr_t uptr3(dmsg1.data);

  dmsg2.data = protobuf_c_message_serialize(NULL, &msg2.base, &dmsg2.len);
  ASSERT_TRUE(dmsg2.data);
  UniquePtrProtobufCFree<>::uptr_t uptr4(dmsg2.data);

  rw_keyspec_path_t *out_ks = nullptr;
  ProtobufCBinaryData outm = {};

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  auto s = rw_keyspec_path_reroot_and_merge_opaque(NULL,  &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t, &dmsg1, &dmsg2, &out_ks, &outm);

  t2 = high_resolution_clock::now();

  ASSERT_EQ(s, RW_STATUS_SUCCESS);
  ASSERT_TRUE(out_ks);
  UniquePtrKeySpecPath::uptr_t uptr5(out_ks);
  ASSERT_TRUE(outm.data);
  UniquePtrProtobufCFree<>::uptr_t uptr6(outm.data);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_reroot_and_merge_opaque: Duration = " 
            << dur << "(ms)" << std::endl;
}

TEST(KeyspecPerf, Xpath)
{
  const char *xpath = "/test-rwapis:config/test-rwapis:manifest/test-rwapis:profile[test-rwapis:name='profile1']/test-rwapis:init-phase/test-rwapis:rwvcs/test-rwapis:component[test-rwapis:component-name='component1']/test-rwapis:rwvm/test-rwapis:event-list/test-rwapis:event[test-rwapis:name='event1']/test-rwapis:action[test-rwapis:name='action1']/test-rwapis:start/t:dl1[t:mykey1='key1'][t:mykey2='100'][t:mykey3='200'][t:mykey4='key4'][t:mykey5='key5'][t:mykey6='key6'][t:mykey7='500']/t:dc1/t:dc2/t:dl2[t:keyl2='abc']/t:dc3/t:dl3";

  auto schema = (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(TestYdomTop);

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  rw_keyspec_path_t *ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_INSTANCEID, NULL);
  t2 = high_resolution_clock::now();

  ASSERT_TRUE(ks);
  UniquePtrKeySpecPath::uptr_t uptr(ks);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_from_xpath: Duration = "
            << dur << "(ms)" << std::endl;

  t1 = high_resolution_clock::now();
  char *xpathr = rw_keyspec_path_to_xpath(ks, schema, NULL);
  t2 = high_resolution_clock::now();

  ASSERT_TRUE(xpathr);
  EXPECT_STREQ(xpathr, xpath);

  dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_to_xpath: Duration = "
            << dur << "(ms)" << std::endl;
  free(xpathr);
}

TEST(KeyspecPerf, Match)
{
  RWPB_M_PATHSPEC_DECL_INIT(LONG_PATH, ks1);
  fill_keys_to_ks(&ks1);

  RWPB_M_PATHSPEC_DECL_INIT(LONG_PATH, ks2);
  fill_keys_to_ks(&ks2);

  size_t ei = 0;
  int ki = 0;

  high_resolution_clock::time_point t1,t2;
  t1 = high_resolution_clock::now();

  bool rc = rw_keyspec_path_is_match_detail(NULL, &ks1.rw_keyspec_path_t, &ks2.rw_keyspec_path_t, &ei, &ki);

  t2 = high_resolution_clock::now();
  ASSERT_TRUE(rc);

  auto dur = duration_cast<milliseconds>( t2 - t1 ).count();
  std::cout << "rw_keyspec_path_is_match_detail: Duration = "
            << dur << "(ms)" << std::endl;
}
