

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
 * @file rw_keyspec_test.cpp
 * @author Sujithra Periasamy
 * @date 2014/09/22
 * @brief Google test cases for rw_schema APIs.
 */

#include <limits.h>
#include <stdlib.h>

#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_schema.pb-c.h"
#include "bumpy-conversion.pb-c.h"
#include "testy2p-top2.pb-c.h"
#include "test-rwapis.pb-c.h"
#include "company.pb-c.h"
#include "document.pb-c.h"
#include "document.ypbc.h"
#include "flat-conversion.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rwdts-data-d.pb-c.h"
#include "test-ydom-top.pb-c.h"
#include "test-tag-generation-base.pb-c.h"
#include "test-tag-generation.pb-c.h"
#include "rw_namespace.h"
#include "rw-appmgr-d.pb-c.h"
#include "rw-ncmgr-data-e.pb-c.h"
#include "rw-keyspec-stats.pb-c.h"
#include "testy2p-search-node1.pb-c.h"
#include "rift-cli-test.pb-c.h"

#define TY2PT2_NS(v) (((TY2PT2_NSID & 0xFFF)<<17)+(v))
#define TY2PT2_NSID rw_namespace_string_to_hash("http://riftio.com/ns/core/util/yangtools/tests/testy2p-top2")
#define TRWAPI_NS(v) (((TRWAPI_NSID & 0xFFF)<<17)+(v))
#define TRWAPI_NSID rw_namespace_string_to_hash("http://riftio.com/ns/core/util/yangtools/tests/test-rwapis")
#define TDOC_NS(v) (((TDOC_NSID & 0xFFF) << 17)+(v))
#define TDOC_NSID rw_namespace_string_to_hash("http://riftio.com/ns/core/util/yangtools/tests/document")
#define TCORP_NS(v) (((TCORP_NSID & 0xFFF)<<17)+(v))
#define TCORP_NSID rw_namespace_string_to_hash("http://riftio.com/ns/core/util/yangtools/tests/company")

// Not public APIs, declared  here for testing..
void keyspec_default_error_cb(rw_keyspec_instance_t* inst, const char* errmsg);
void keyspec_dump_error_records();
void keyspec_clear_error_records();

#define INIT_BASIC_KEY_SPEC1() \
  RwSchemaPathSpec key_spec; \
  rw_schema_path_spec__init(&key_spec);\
  RwSchemaPathDom dom;\
  rw_schema_path_dom__init(&dom);\
  key_spec.dompath = &dom;\
  \
  dom.path000 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path000); \
  dom.path000->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path000->element_id); \
  dom.path000->element_id->system_ns_id = 1; \
  dom.path000->element_id->element_tag = 1; \
  \
  dom.path001 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path001); \
  dom.path001->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path001->element_id); \
  dom.path001->element_id->system_ns_id = 1; \
  dom.path001->element_id->element_tag = 2; \
  \
  dom.path002 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path002); \
  dom.path002->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path002->element_id); \
  dom.path002->element_id->system_ns_id = 1; \
  dom.path002->element_id->element_tag = 3; \
  dom.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1; \
  dom.path002->key00 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));\
  rw_schema_key__init(dom.path002->key00);\
  dom.path002->key00->string_key = strdup("Colony123");\
  \
  dom.path003 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path003); \
  dom.path003->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path003->element_id); \
  dom.path003->element_id->system_ns_id = 1; \
  dom.path003->element_id->element_tag = 4; \
  dom.path003->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_2; \
  dom.path003->key00 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));\
  rw_schema_key__init(dom.path003->key00); \
  dom.path003->key00->string_key = strdup("Colony1");\
  dom.path003->key01 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey)); \
  rw_schema_key__init(dom.path003->key01);\
  dom.path003->key01->string_key = strdup("ClusterC");\
  \
  dom.path004 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path004); \
  dom.path004->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path004->element_id); \
  dom.path004->element_id->system_ns_id = 1; \
  dom.path004->element_id->element_tag = 5;

#define FREE_KEY_SPEC1() \
  rw_schema_path_dom__free_unpacked_usebody(NULL, &dom); \

#define INIT_BASIC_KEY_SPEC2(val) \
  RwSchemaPathSpec key_spec1; \
  rw_schema_path_spec__init(&key_spec1);\
  RwSchemaPathDom dom1;\
  rw_schema_path_dom__init(&dom1);\
  key_spec1.dompath = &dom1;\
  \
  dom1.path000 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom1.path000); \
  dom1.path000->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom1.path000->element_id); \
  dom1.path000->element_id->system_ns_id = 1; \
  dom1.path000->element_id->element_tag = 1+val; \
  \
  dom1.path001 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom1.path001); \
  dom1.path001->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom1.path001->element_id); \
  dom1.path001->element_id->system_ns_id = 1; \
  dom1.path001->element_id->element_tag = 2+val; \
  \
  dom1.path002 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom1.path002); \
  dom1.path002->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom1.path002->element_id); \
  dom1.path002->element_id->system_ns_id = 1; \
  dom1.path002->element_id->element_tag = 3+val; \
  dom1.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1; \
  \
  dom1.path003 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom1.path003); \
  dom1.path003->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom1.path003->element_id); \
  dom1.path003->element_id->system_ns_id = 1; \
  dom1.path003->element_id->element_tag = 4+val; \
  dom1.path003->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_2; \
  \
  dom1.path004 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom1.path004); \
  dom1.path004->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom1.path004->element_id); \
  dom1.path004->element_id->system_ns_id = 1; \
  dom1.path004->element_id->element_tag = 5+val; \

#define FREE_KEY_SPEC2() \
  rw_schema_path_dom__free_unpacked_usebody(NULL, &dom1); \

#define ADD_PATH(dom, index)  \
  dom.path00##index = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom.path00##index); \
  dom.path00##index->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom.path00##index->element_id); \
  dom.path00##index->element_id->system_ns_id = 1; \
  dom.path00##index->element_id->element_tag = index+1; \

#define ADD_PATH_SPEC(dom, index, ns, tag, et)  \
  dom->path0##index = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom->path0##index); \
  dom->path0##index->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom->path0##index->element_id); \
  dom->path0##index->element_id->system_ns_id = ns; \
  dom->path0##index->element_id->element_tag = tag; \
  dom->path0##index->element_id->element_type = et;

#define CREATE_SUB_PATH() \
  RwSchemaPathSpec key_spec_sub; \
  rw_schema_path_spec__init(&key_spec_sub);\
  RwSchemaPathDom dom_sub;\
  rw_schema_path_dom__init(&dom_sub);\
  key_spec_sub.dompath = &dom_sub;\
  \
  dom_sub.path000 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom_sub.path000); \
  dom_sub.path000->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom_sub.path000->element_id); \
  dom_sub.path000->element_id->system_ns_id = 1; \
  dom_sub.path000->element_id->element_tag = 2; \
  \
  dom_sub.path001 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));\
  rw_schema_path_entry__init(dom_sub.path001); \
  dom_sub.path001->element_id =\
  (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId)); \
  rw_schema_element_id__init(dom_sub.path001->element_id); \
  dom_sub.path001->element_id->system_ns_id = 1; \
  dom_sub.path001->element_id->element_tag = 3; \
  dom_sub.path001->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1;

#define FREE_SUB_PATH() \
  rw_schema_path_dom__free_unpacked_usebody(NULL, &dom_sub); \

TEST(RwKeyspec, API1)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  INIT_BASIC_KEY_SPEC1()

  const char *str_path = nullptr;
  rw_status_t status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, "A,1:1/1:2/1:3[Key1=\'Colony123\']/1:4[Key1=\'Colony1\',Key2=\'ClusterC\']/1:5");

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t *>(&key_spec)), 5);
  const void *entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), 0);
  const RwSchemaPathEntry *e = (const RwSchemaPathEntry *)entry;
  EXPECT_EQ(e->element_id->system_ns_id, 1);
  EXPECT_EQ(e->element_id->element_tag, 1);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), 1);
  e = (const RwSchemaPathEntry *)entry;
  EXPECT_EQ(e->element_id->system_ns_id, 1);
  EXPECT_EQ(e->element_id->element_tag, 2);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), 2);
  e = (const RwSchemaPathEntry *)entry;
  EXPECT_EQ(e->element_id->system_ns_id, 1);
  EXPECT_EQ(e->element_id->element_tag, 3);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), 3);
  e = (const RwSchemaPathEntry *)entry;
  EXPECT_EQ(e->element_id->system_ns_id, 1);
  EXPECT_EQ(e->element_id->element_tag, 4);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), 4);
  e = (const RwSchemaPathEntry *)entry;
  EXPECT_EQ(e->element_id->system_ns_id, 1);
  EXPECT_EQ(e->element_id->element_tag, 5);

  RwSchemaPathEntry temp;
  rw_schema_path_entry__init(&temp);

  RwSchemaElementId tempeid;
  rw_schema_element_id__init(&tempeid);
  tempeid.system_ns_id = 1;
  tempeid.element_tag = 2;
  temp.element_id = &tempeid;


  size_t ele_index = 0;
  int key_index = 0;
  EXPECT_DEATH(rw_keyspec_path_is_entry_at_tip(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr,
                                               reinterpret_cast<rw_keyspec_entry_t*>(&temp),
                                               &ele_index, &key_index), "");
  // EXPECT_EQ(ele_index, 1);
  // EXPECT_EQ(key_index, -1);

  // RwSchemaKey key;
  // rw_schema_key__init(&key);
  // key.string_key = (char *)malloc(20);
  // strcpy(key.string_key, "Colony1");
  // temp.key00 = &key;
  // tempeid.element_tag = 3;
  // tempeid.element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1;

  // EXPECT_TRUE(rw_keyspec_path_has_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr,
  //                                  reinterpret_cast<rw_keyspec_entry_t*>(&temp),
  //                                  &ele_index, &key_index));
  // EXPECT_EQ(ele_index, 2);
  // EXPECT_EQ(key_index, 0);

  // tempeid.element_tag = 4;

  // RwSchemaKey key01;
  // rw_schema_key__init(&key01);
  // key01.string_key = (char *)malloc(20);
  // strcpy(key01.string_key, "ClusterA");
  // temp.key01 = &key01;
  // tempeid.element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_2;

  // EXPECT_TRUE(rw_keyspec_path_has_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr,
  //                                  reinterpret_cast<rw_keyspec_entry_t*>(&temp),
  //                                  &ele_index, &key_index));
  // EXPECT_EQ(ele_index, 3);
  // EXPECT_EQ(key_index, 1);


  // tempeid.element_tag = 5;
  // tempeid.element_type = RW_SCHEMA_ELEMENT_TYPE_CONTAINER;
  // temp.key00 = NULL;
  // temp.key01 = NULL;
  // EXPECT_TRUE(rw_keyspec_path_has_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr,
  //                                  reinterpret_cast<rw_keyspec_entry_t*>(&temp),
  //                                  &ele_index, &key_index));
  // EXPECT_EQ(ele_index, 4);
  // EXPECT_EQ(key_index, -1);

  // tempeid.element_tag = 30;
  // EXPECT_FALSE(rw_keyspec_path_has_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr,
  //                                  reinterpret_cast<rw_keyspec_entry_t*>(&temp),
  //                                  &ele_index, &key_index));

  RwSchemaElementId element;
  rw_schema_element_id__init(&element);

  element.system_ns_id = 1;
  element.element_tag = 5;

  EXPECT_TRUE(rw_keyspec_path_has_element(reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                          nullptr,
                                     &element, &ele_index));

  EXPECT_EQ(ele_index, 4);

  element.system_ns_id = 1;
  element.element_tag = 1;

  EXPECT_TRUE(rw_keyspec_path_has_element(reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                          nullptr,
                                     &element, &ele_index));

  EXPECT_EQ(ele_index, 0);

  element.system_ns_id = 4;
  element.element_tag = 1;

  EXPECT_FALSE(rw_keyspec_path_has_element(reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                           nullptr,
                                     &element, &ele_index));

  size_t s = protobuf_c_message_get_packed_size(NULL, &dom.base);
  uint8_t stackd[1024] = {};

  size_t ss = protobuf_c_message_pack(NULL, &dom.base, stackd);
  EXPECT_EQ(s, ss);

  ProtobufCBinaryData data;
  data.len = 0;
  data.data = nullptr;

  RwSchemaPathSpec *newspec = (RwSchemaPathSpec *)rw_keyspec_path_create_with_binpath_buf(nullptr, ss, stackd);
  ASSERT_NE(newspec, nullptr);
  EXPECT_EQ(newspec->has_binpath, true);
  EXPECT_EQ(newspec->binpath.len, ss);
  EXPECT_FALSE(memcmp(newspec->binpath.data, stackd, ss));
  EXPECT_EQ(rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(newspec), nullptr), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_serialize_dompath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , &data), RW_STATUS_SUCCESS);
  EXPECT_EQ(data.len, ss);
  ASSERT_NE(data.data, nullptr);
  EXPECT_FALSE(memcmp(data.data, stackd, ss));
  EXPECT_FALSE(key_spec.has_binpath);

  RwSchemaPathSpec *newspec1 = (RwSchemaPathSpec *)rw_keyspec_path_create_with_binpath_binary_data(nullptr, &data);
  ASSERT_NE(newspec1, nullptr);
  EXPECT_EQ(newspec1->has_binpath, true);
  EXPECT_EQ(newspec1->binpath.len, ss);
  EXPECT_FALSE(memcmp(newspec1->binpath.data, stackd, ss));
  EXPECT_EQ(rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(newspec1), nullptr), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_update_binpath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), NULL), RW_STATUS_SUCCESS);

  EXPECT_TRUE(key_spec.has_binpath);
  EXPECT_EQ(key_spec.binpath.len, ss);
  EXPECT_FALSE(memcmp(key_spec.binpath.data, stackd, ss));

  const uint8_t *bin = nullptr;
  size_t len = 0;

  EXPECT_EQ(rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , &bin, &len), RW_STATUS_SUCCESS);
  EXPECT_EQ(len , ss);
  ASSERT_NE(bin, nullptr);
  EXPECT_FALSE(memcmp(bin, stackd, len));

  data.len = 0;
  free(data.data);
  data.data = nullptr;
  EXPECT_EQ(rw_keyspec_path_serialize_dompath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , &data),  RW_STATUS_SUCCESS);
  EXPECT_EQ(data.len, ss);
  ASSERT_NE(data.data, nullptr);
  EXPECT_FALSE(memcmp(data.data, stackd, ss));
  free(data.data);
  data.data = nullptr;


  RwSchemaPathSpec key_spec1;
  rw_schema_path_spec__init(&key_spec1);

  key_spec1.has_binpath = true;
  key_spec1.binpath.len = ss;
  key_spec1.binpath.data = stackd;

  EXPECT_EQ(rw_keyspec_path_update_dompath(reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), nullptr), RW_STATUS_SUCCESS);
  ASSERT_NE(key_spec1.dompath, nullptr);
  EXPECT_EQ(key_spec1.dompath->path000->element_id->system_ns_id, 1);
  EXPECT_EQ(key_spec1.dompath->path000->element_id->element_tag, 1);
  EXPECT_EQ(key_spec1.dompath->path001->element_id->system_ns_id, 1);
  EXPECT_EQ(key_spec1.dompath->path001->element_id->element_tag, 2);
  EXPECT_EQ(key_spec1.dompath->path002->element_id->system_ns_id, 1);
  EXPECT_EQ(key_spec1.dompath->path002->element_id->element_tag, 3);
  EXPECT_STREQ(key_spec1.dompath->path002->key00->string_key, "Colony123");
  EXPECT_EQ(key_spec1.dompath->path003->element_id->system_ns_id, 1);
  EXPECT_EQ(key_spec1.dompath->path003->element_id->element_tag, 4);
  EXPECT_STREQ(key_spec1.dompath->path003->key00->string_key, "Colony1");
  EXPECT_STREQ(key_spec1.dompath->path003->key01->string_key, "ClusterC");
  EXPECT_EQ(key_spec1.dompath->path004->element_id->system_ns_id, 1);
  EXPECT_EQ(key_spec1.dompath->path004->element_id->element_tag, 5);

  EXPECT_TRUE(rw_keyspec_path_is_equal(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1)));

  EXPECT_TRUE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  key_spec1.dompath->path004->element_id->element_tag = 30;
  ele_index = 0; key_index = 0;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));
  EXPECT_EQ(ele_index, 4);
  EXPECT_EQ(key_index, -1);

  ele_index = 0; key_index = 0;
  strcpy(key_spec1.dompath->path003->key01->string_key, "ClusterZ");
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));
  EXPECT_EQ(ele_index, 3);
  EXPECT_EQ(key_index, 1);

  key_spec1.dompath->path000->element_id->system_ns_id = 5000;
  ele_index = 1000; key_index = 0;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));
  EXPECT_EQ(ele_index, 0);
  EXPECT_EQ(key_index, -1);

  free(key_spec1.dompath->path002->key00->string_key);
  key_spec1.dompath->path002->key00->string_key = NULL;
  free(key_spec1.dompath->path002->key00);
  key_spec1.dompath->path002->key00 = NULL;

  key_spec1.dompath->path000->element_id->system_ns_id = 1;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));
  EXPECT_EQ(ele_index, 2);
  EXPECT_EQ(key_index, 0);

  RwSchemaPathSpec key_spec2;
  rw_schema_path_spec__init(&key_spec2);

  key_spec2.has_binpath = true;
  key_spec2.binpath.len = ss;
  key_spec2.binpath.data = stackd;

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t *>(&key_spec2)), 5);

  key_spec2.has_binpath = false;
  key_spec2.binpath.len = 0;
  key_spec2.binpath.data = nullptr;

  protobuf_c_message_free_unpacked_usebody(NULL, &key_spec2.base);

  key_spec1.has_binpath = false;
  key_spec1.binpath.len = 0;
  key_spec1.binpath.data = nullptr;

  protobuf_c_message_free_unpacked_usebody(NULL, &key_spec1.base);

  RwSchemaPathSpec key_spec3;
  rw_schema_path_spec__init(&key_spec3);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t *>(&key_spec3)), 0);
  EXPECT_EQ(rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t *>(&key_spec3), 0), nullptr);


  if (key_spec.has_binpath) {
    free(key_spec.binpath.data);
    key_spec.binpath.data = nullptr;
    key_spec.has_binpath = false;
    key_spec.binpath.len = 0;
  }

  if (key_spec.strpath) {
    free(key_spec.strpath);
  }
  FREE_KEY_SPEC1()
  // protobuf_c_message_free_unpacked_usebody(NULL, &key.base);
  // protobuf_c_message_free_unpacked_usebody(NULL, &key01.base);
}

TEST(RwKeyspec, API2)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  INIT_BASIC_KEY_SPEC1()
  INIT_BASIC_KEY_SPEC2(0)

  size_t ele_index = 0;
  int key_index = 0;

  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                         reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));
  EXPECT_EQ(ele_index, 2);
  EXPECT_EQ(key_index, 0);

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  dom1.path002->key00 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(dom1.path002->key00);
  dom1.path002->key00->string_key = strdup("Colony123");

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  strcpy(dom1.path002->key00->string_key, "Colony124");

  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  EXPECT_EQ(ele_index, 2);
  EXPECT_EQ(key_index, 0);

  strcpy(dom1.path002->key00->string_key, "Colony123");

  dom1.path003->key00 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(dom1.path003->key00);
  dom1.path003->key00->string_key = strdup("Colony1");

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
              reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  dom1.path003->key01 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(dom1.path003->key01);
  dom1.path003->key01->string_key = strdup("ClusterC");

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  strcpy(dom1.path003->key01->string_key, "ClusterB");
  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  EXPECT_EQ(ele_index, 3);
  EXPECT_EQ(key_index, 1);

  strcpy(dom1.path003->key01->string_key, "ClusterC");

  ADD_PATH(dom1, 5);
  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(&key_spec),
                  reinterpret_cast<rw_keyspec_path_t *>(&key_spec1), &ele_index, &key_index));

  EXPECT_EQ(ele_index, 5);
  EXPECT_EQ(key_index, -1);

  //TODO: Add testcases for unknown keys when there are concert keyspec
  //available.
  FREE_KEY_SPEC1()
  FREE_KEY_SPEC2()
}

TEST(RwKeyspec, API3)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  INIT_BASIC_KEY_SPEC1()
  INIT_BASIC_KEY_SPEC2(0)

  size_t ele_index = 0;
  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec),
                                             reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec1), &ele_index));

  key_spec1.dompath->path001->element_id->system_ns_id = 100;
  EXPECT_FALSE(rw_keyspec_path_is_same_path_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec),
                                             reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec1), &ele_index));
  EXPECT_EQ(ele_index, 1);
  key_spec1.dompath->path001->element_id->system_ns_id = 1;

  ADD_PATH(dom1, 5)

  EXPECT_FALSE(rw_keyspec_path_is_same_path_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec),
                                             reinterpret_cast<rw_keyspec_path_t
                                             *>(&key_spec1), &ele_index));
  EXPECT_EQ(ele_index, 5);

  size_t start_index = 0, len = 0;
  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec1), true, &start_index, &len));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(len, 5);

  CREATE_SUB_PATH()

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 2);

  dom_sub.path001->element_id->system_ns_id = 100;
  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 1);

  dom_sub.path000->element_id->system_ns_id = 100;
  EXPECT_FALSE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec1), false, &start_index, &len));
  EXPECT_EQ(start_index, 0);
  EXPECT_EQ(len, 2);

  dom_sub.path000->element_id->system_ns_id = 1;
  dom_sub.path001->element_id->system_ns_id = 1;

  ADD_PATH(dom_sub, 2);
  dom_sub.path002->element_id->element_tag = 4;
  dom_sub.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_2;

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 3);
  dom_sub.path001->element_id->element_tag = 77;

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 1);

  dom_sub.path001->element_id->element_tag = 3;
  ADD_PATH(dom_sub, 3);
  dom_sub.path003->element_id->element_tag = 5;
  ADD_PATH(dom_sub, 4);
  dom_sub.path004->element_id->element_tag = 6;
  ADD_PATH(dom_sub, 5);
  dom_sub.path005->element_id->element_tag = 7;

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 4);

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), false, &start_index, &len));
  EXPECT_EQ(start_index, 1);
  EXPECT_EQ(len, 1);

  dom_sub.path000->element_id->element_tag = 90;
  EXPECT_FALSE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));

  dom_sub.path001->element_id->element_tag = 91;
  dom_sub.path002->element_id->element_tag = 92;
  dom_sub.path003->element_id->element_tag = 93;
  dom_sub.path004->element_id->element_tag = 94;
  dom_sub.path005->element_id->element_tag = 95;

  EXPECT_FALSE(rw_keyspec_path_is_branch_detail(nullptr, reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec),
                                          reinterpret_cast<rw_keyspec_path_t
                                          *>(&key_spec_sub), true, &start_index, &len));
  FREE_KEY_SPEC1()
  FREE_KEY_SPEC2()
  FREE_SUB_PATH()
}

TEST(RwKeyspec, API4)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  INIT_BASIC_KEY_SPEC1()

  RwSchemaPathEntry entry1;
  rw_schema_path_entry__init(&entry1);
  entry1.element_id = (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId));
  rw_schema_element_id__init(entry1.element_id);
  entry1.element_id->system_ns_id = 1;
  entry1.element_id->element_tag = 6;

  EXPECT_EQ(rw_keyspec_path_append_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                          reinterpret_cast<rw_keyspec_entry_t *> (&entry1)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 6);
  const void *entry = nullptr;
  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), 5);

  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, 6);

  // Test empty keyspec.
  RwSchemaPathSpec emp;
  rw_schema_path_spec__init(&emp);

  EXPECT_EQ(rw_keyspec_path_append_entry(reinterpret_cast<rw_keyspec_path_t*> (&emp), nullptr ,
                          reinterpret_cast<rw_keyspec_entry_t *> (&entry1)), RW_STATUS_FAILURE);

  free(entry1.element_id);

  RwSchemaPathEntry *list[30];
  for (unsigned i = 0; i < 30; i++) {
    list[i] = nullptr;
  }

  for (unsigned i = 0; i < 3; i++) {
    list[i] = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));
    rw_schema_path_entry__init(list[i]);
    list[i]->element_id = (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId));
    rw_schema_element_id__init(list[i]->element_id);
    list[i]->element_id->system_ns_id = 1;
    list[i]->element_id->element_tag = 7+i;
  }

  EXPECT_EQ(rw_keyspec_path_append_entries(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                              (rw_keyspec_entry_t **)list, 3), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 9);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), 6);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, 7);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), 7);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, 8);

  entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), 8);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
  EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, 9);

  // Test for maximum.
  for (unsigned i = 3; i < 25; i++) {
    list[i] = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));
    rw_schema_path_entry__init(list[i]);
    list[i]->element_id = (RwSchemaElementId*)malloc(sizeof(RwSchemaElementId));
    rw_schema_element_id__init(list[i]->element_id);
    list[i]->element_id->system_ns_id = 1;
    list[i]->element_id->element_tag = 7+i;
  }

  EXPECT_EQ(rw_keyspec_path_append_entries(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                              (rw_keyspec_entry_t **)(list+3), 21), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 30);
  for (unsigned i = 9; i < 30; i++) {
    entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), i);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, i+1);
  }

  EXPECT_EQ(rw_keyspec_path_append_entries(reinterpret_cast<rw_keyspec_path_t*>(&key_spec), nullptr ,
                                      (rw_keyspec_entry_t **)(list+24), 1),  RW_STATUS_FAILURE);

  EXPECT_EQ(rw_keyspec_path_append_entries(reinterpret_cast<rw_keyspec_path_t*>(&key_spec), nullptr ,
                                      (rw_keyspec_entry_t **)(list+24), 0),  RW_STATUS_SUCCESS);
  for (unsigned i = 0; i < 30; i++) {
    if (list[i]) {
      free(list[i]->element_id);
      free(list[i]);
    }
  }

  RwSchemaPathSpec empty;
  rw_schema_path_spec__init(&empty);
  empty.dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  rw_schema_path_dom__init(empty.dompath);

  EXPECT_EQ(rw_keyspec_path_append_keyspec(reinterpret_cast<rw_keyspec_path_t*> (&empty), nullptr ,
                            reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&empty)), 30);

  INIT_BASIC_KEY_SPEC2(30)

  RwSchemaPathSpec empty1;
  rw_schema_path_spec__init(&empty1);
  empty1.dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  rw_schema_path_dom__init(empty1.dompath);

  EXPECT_EQ(rw_keyspec_path_append_keyspec(reinterpret_cast<rw_keyspec_path_t*> (&empty1), nullptr ,
                            reinterpret_cast<rw_keyspec_path_t*> (&key_spec1)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&empty1)), 5);

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), 0, 0, 5), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 30);

  for (unsigned i = 0; i < 5; i++) {
    entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), i);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, i+30+1);
  }

  for (unsigned i = 5; i < 30; i++) {
    entry = rw_keyspec_path_get_path_entry(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), i);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->system_ns_id, 1);
    EXPECT_EQ(((RwSchemaPathEntry *)entry)->element_id->element_tag, i+1);
  }

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), 31, 0, 5), RW_STATUS_OUT_OF_BOUNDS);

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), -5, 0, 5), RW_STATUS_OUT_OF_BOUNDS);

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), 0, 5, 5), RW_STATUS_OUT_OF_BOUNDS);

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), 0, 0, 10), RW_STATUS_OUT_OF_BOUNDS);

  EXPECT_EQ(rw_keyspec_path_trunc_suffix_n(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr , 5), RW_STATUS_SUCCESS);
  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 5);

  EXPECT_EQ(rw_keyspec_path_trunc_suffix_n(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr , 6), RW_STATUS_OUT_OF_BOUNDS);

  EXPECT_EQ(rw_keyspec_path_append_replace(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                                      reinterpret_cast<rw_keyspec_path_t*> (&empty1), -1, 0, 5), RW_STATUS_SUCCESS);
  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*> (&key_spec)), 10);

  FREE_KEY_SPEC1()
  FREE_KEY_SPEC2()

  protobuf_c_message_free_unpacked_usebody(NULL, &empty.base);
  protobuf_c_message_free_unpacked_usebody(NULL, &empty1.base);
}

TEST(RwKeyspec, API5)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  INIT_BASIC_KEY_SPEC1()

  RwSchemaPathSpec *temp = nullptr;

  EXPECT_EQ(rw_keyspec_path_create_dup(reinterpret_cast<rw_keyspec_path_t*> (&key_spec), nullptr ,
                       reinterpret_cast<rw_keyspec_path_t**> (&temp)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*>(temp)), 5);

  RwSchemaPathSpec output;
  rw_schema_path_spec__init(&output);
  EXPECT_EQ(rw_keyspec_path_copy(reinterpret_cast<rw_keyspec_path_t*>(temp), nullptr,
            reinterpret_cast<rw_keyspec_path_t*>(&output)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*>(&output)), 5);

  ADD_PATH(dom, 5);
  ADD_PATH(dom, 6);
  ADD_PATH(dom, 7);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*>(&key_spec)), 8);
  EXPECT_EQ(rw_keyspec_path_swap(nullptr, reinterpret_cast<rw_keyspec_path_t*>(&dom),
                            reinterpret_cast<rw_keyspec_path_t*>(output.dompath)), RW_STATUS_SUCCESS);

  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*>(&output)), 8);
  EXPECT_EQ(rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t*>(&key_spec)), 5);

  FREE_KEY_SPEC1();
  rw_schema_path_spec__free_unpacked_usebody(NULL, &output);
  EXPECT_EQ(rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t*>(temp), nullptr), RW_STATUS_SUCCESS);

  RwSchemaPathEntry pe;
  rw_schema_path_entry__init(&pe);


  const ProtobufCFieldDescriptor *fd = nullptr;
  void *msg;
  size_t offset = 0;
  protobuf_c_boolean dptr = false;

  size_t c = protobuf_c_message_get_field_desc_count_and_offset(&pe.base,
                          1, &fd, &msg, &offset, &dptr);

  EXPECT_EQ(c, 0);
  EXPECT_EQ(msg, nullptr);

  pe.element_id = (RwSchemaElementId *)malloc(sizeof(RwSchemaElementId));

  c = protobuf_c_message_get_field_desc_count_and_offset(&pe.base,
                          1, &fd, &msg, &offset, &dptr);

  EXPECT_EQ(c, 1);
  EXPECT_NE(msg, nullptr);

  free(pe.element_id);
}

TEST(RwKeyspec, CompileTimeKeySpecs)
{

  TEST_DESCRIPTION("Tests the rw_schema APIs");

  EXPECT_TRUE(rw_keyspec_entry_is_listy(reinterpret_cast<const rw_keyspec_entry_t*>
      (RWPB_G_PATHENTRY_VALUE(BumpyConversion_data_Container1_Container11_List112))));

  EXPECT_FALSE(rw_keyspec_entry_is_listy(reinterpret_cast<const rw_keyspec_entry_t*>
      (RWPB_G_PATHENTRY_VALUE(BumpyConversion_data_Container1))));

  rw_keyspec_path_t *ks = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1_Container11_List112)), 
                             nullptr ,
                &ks);
  EXPECT_TRUE(rw_keyspec_path_is_listy(ks));
  rw_keyspec_path_free(ks, nullptr);

  ks = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1_TwoKeys)), nullptr ,
            &ks);

  EXPECT_TRUE(rw_keyspec_path_is_listy(ks));

  // Path entry testing
  size_t depth = 0;
  
  depth = rw_keyspec_path_get_depth (ks);
  EXPECT_EQ (depth, 2);

  for (size_t index = 0; index < depth; index++) {
    const rw_keyspec_entry_t *pe = rw_keyspec_path_get_path_entry (ks, index);
    ASSERT_NE (pe, nullptr);
    bool at_tip = rw_keyspec_path_is_entry_at_tip (ks, nullptr, pe, nullptr, nullptr);
    ASSERT_EQ (index == depth -1, at_tip);    
  }
  
  rw_keyspec_path_free(ks, nullptr);

  ks = nullptr;
  rw_keyspec_path_create_dup(
      reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1_Container11_List112_Wine)), nullptr ,
            &ks);

  EXPECT_FALSE(rw_keyspec_path_is_listy(ks));
    depth = rw_keyspec_path_get_depth (ks);
  EXPECT_EQ (depth, 4);

  for (size_t index = 0; index < depth; index++) {
    const rw_keyspec_entry_t *pe = rw_keyspec_path_get_path_entry (ks, index);
    ASSERT_NE (pe, nullptr);
    bool at_tip = rw_keyspec_path_is_entry_at_tip (ks, nullptr, pe, nullptr, nullptr);
    ASSERT_EQ (index == depth -1, at_tip);    
  }

  rw_keyspec_path_free(ks, nullptr);

  INIT_BASIC_KEY_SPEC1()


  dom.path000->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_CONTAINER;
  dom.path001->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_CONTAINER;
  dom.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1;
  dom.path003->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_2;
  dom.path004->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_0;

  EXPECT_FALSE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(&key_spec)));

  RWPB_M_PATHSPEC_DECL_INIT(BumpyConversion_data_Container1_Container11_List112,temp);

  temp.has_dompath = true;
  temp.dompath.rooted = true;
  temp.dompath.path000.element_id.system_ns_id = 1;
  temp.dompath.path000.element_id.element_tag = 5001;
  temp.dompath.path000.element_id.element_type = RW_SCHEMA_ELEMENT_TYPE_CONTAINER;

  temp.dompath.path001.element_id.system_ns_id = 1;
  temp.dompath.path001.element_id.element_tag = 5002;
  temp.dompath.path001.element_id.element_type = RW_SCHEMA_ELEMENT_TYPE_CONTAINER;

  temp.dompath.path002.element_id.system_ns_id = 1;
  temp.dompath.path002.element_id.element_tag = 5003;
  temp.dompath.path002.element_id.element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1;
  temp.dompath.path002.has_key00 = true;
  temp.dompath.path002.key00.int_1_1_2_1 = 100;

  EXPECT_FALSE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(&temp)));
  EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(&temp)));
  EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(&temp), 3));

  temp.dompath.path002.has_key00 = false;
  EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(&temp)));
  EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(&temp)));
  EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(&temp), 3));

  temp.dompath.path002.has_key00 = true;
  char print_buf[512];
  int ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , NULL,
          print_buf, sizeof(print_buf));
  EXPECT_NE(ret, -1);

  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(&temp), nullptr , NULL,
          print_buf, sizeof(print_buf));
  EXPECT_EQ(ret, 35);
  EXPECT_EQ(strlen(print_buf), 35);

  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(&temp), nullptr , NULL,
          print_buf, 10);
  EXPECT_EQ(ret, 9);
  EXPECT_EQ(strlen(print_buf), 9);

  free(temp.dompath.path002.key00.string_key);
  FREE_KEY_SPEC1()

 // Try printing RPC and Notifications.
  const rw_keyspec_path_t *keys = reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_input_RpcInOut_C));
  ret =  rw_keyspec_path_get_print_buffer(const_cast<rw_keyspec_path_t *>(keys), nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2)), print_buf, 512);
  EXPECT_EQ(ret, 25);
  EXPECT_STREQ(print_buf, "I,/tyt2:rpc-in-out/tyt2:c");

  ret =  rw_keyspec_path_get_print_buffer(const_cast<rw_keyspec_path_t *>(keys), nullptr , NULL, print_buf, 512);
  EXPECT_EQ(ret, 25);
  EXPECT_STREQ(print_buf, "I,/tyt2:rpc-in-out/tyt2:c");
}

TEST(RwKeyspec, SchemaWalk)
{
  RwSchemaPathSpec gen_ks;
  rw_schema_path_spec__init(&gen_ks);

  gen_ks.dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  rw_schema_path_dom__init(gen_ks.dompath);

  gen_ks.dompath->rooted = true;
  ADD_PATH_SPEC(gen_ks.dompath, 00, TY2PT2_NSID, TY2PT2_NS(56), RW_SCHEMA_ELEMENT_TYPE_LISTY_1)
  ADD_PATH_SPEC(gen_ks.dompath, 01, TY2PT2_NSID, TY2PT2_NS(14), RW_SCHEMA_ELEMENT_TYPE_LISTY_3)
  ADD_PATH_SPEC(gen_ks.dompath, 02, TY2PT2_NSID, TY2PT2_NS(18), RW_SCHEMA_ELEMENT_TYPE_LISTY_2)

  const rw_yang_pb_msgdesc_t *msg_out = nullptr;
  rw_keyspec_path_t *rem = nullptr;
  rw_status_t status = rw_keyspec_path_find_msg_desc_schema(reinterpret_cast<rw_keyspec_path_t *>(&gen_ks), nullptr , 
        reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2)),
        &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(msg_out == (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(Testy2pTop2_data_Topl_La_Lb));
  EXPECT_EQ(rem, nullptr);

  INIT_BASIC_KEY_SPEC1()
  key_spec.dompath->rooted = true;
  status = rw_keyspec_path_find_msg_desc_schema(reinterpret_cast<rw_keyspec_path_t *>(&key_spec), nullptr , 
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2)),
      &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_FAILURE);
  EXPECT_EQ(msg_out, nullptr);
  EXPECT_EQ(rem, nullptr);

  rw_schema_path_spec__free_unpacked_usebody(NULL, &gen_ks);
  rw_schema_path_spec__init(&gen_ks);

  gen_ks.dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  rw_schema_path_dom__init(gen_ks.dompath);
  gen_ks.dompath->rooted = true;

  ADD_PATH_SPEC(gen_ks.dompath, 00, TRWAPI_NSID, TRWAPI_NS(97), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 01, TRWAPI_NSID, TRWAPI_NS(98), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 02, TRWAPI_NSID, TRWAPI_NS(100), RW_SCHEMA_ELEMENT_TYPE_LISTY_1)
  ADD_PATH_SPEC(gen_ks.dompath, 03, TRWAPI_NSID, TRWAPI_NS(63), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 04, TRWAPI_NSID, TRWAPI_NS(79), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 05, TRWAPI_NSID, TRWAPI_NS(35), RW_SCHEMA_ELEMENT_TYPE_LISTY_1)
  ADD_PATH_SPEC(gen_ks.dompath, 06, TRWAPI_NSID, TRWAPI_NS(29), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 07, TRWAPI_NSID, TRWAPI_NS(19), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)
  ADD_PATH_SPEC(gen_ks.dompath, 08, TRWAPI_NSID, TRWAPI_NS(20), RW_SCHEMA_ELEMENT_TYPE_LISTY_1)
  ADD_PATH_SPEC(gen_ks.dompath, 09, TRWAPI_NSID, TRWAPI_NS(4), RW_SCHEMA_ELEMENT_TYPE_LISTY_1)
  ADD_PATH_SPEC(gen_ks.dompath, 10, TRWAPI_NSID, TRWAPI_NS(8), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)

  status = rw_keyspec_path_find_msg_desc_schema(reinterpret_cast<rw_keyspec_path_t *>(&gen_ks), nullptr , 
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(TestRwapis)),
      &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE((msg_out == (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Annex)));
  EXPECT_EQ(rem, nullptr);

  gen_ks.dompath->path010->element_id->element_tag = TRWAPI_NS(9);
  status = rw_keyspec_path_find_msg_desc_schema(
      reinterpret_cast<rw_keyspec_path_t *>(&gen_ks), 
      nullptr,
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(TestRwapis)),
      &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_OUT_OF_BOUNDS);
  EXPECT_TRUE((msg_out == (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action)));
  ASSERT_NE(rem, nullptr);

  EXPECT_EQ(rw_keyspec_path_get_depth(rem), 1);
  const rw_keyspec_entry_t *pe = rw_keyspec_path_get_path_entry(rem, 0);
  ASSERT_NE(pe, nullptr);
  EXPECT_EQ(((RwSchemaPathEntry *)pe)->element_id->system_ns_id, TRWAPI_NSID);
  EXPECT_EQ(((RwSchemaPathEntry *)pe)->element_id->element_tag, TRWAPI_NS(9));
  rw_schema_path_spec__free_unpacked(NULL, reinterpret_cast<RwSchemaPathSpec *>(rem));
  rem = nullptr;

  gen_ks.dompath->path010->element_id->element_tag = TRWAPI_NS(8);
  status = rw_keyspec_path_find_msg_desc_msg(
      reinterpret_cast<rw_keyspec_path_t *>(&gen_ks), 
      nullptr,
      (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(TestRwapis_data_Config_Manifest_Profile_InitPhase),
      3, &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE((msg_out == (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(TestRwapis_data_Config_Manifest_Profile_InitPhase_Rwvcs_Component_Rwcolony_EventList_Event_Action_Annex)));
  EXPECT_EQ(rem, nullptr);
  rw_schema_path_spec__free_unpacked_usebody(NULL, &gen_ks);

  rw_schema_path_spec__init(&gen_ks);
  gen_ks.dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  rw_schema_path_dom__init(gen_ks.dompath);
  gen_ks.dompath->rooted = true;
  ADD_PATH_SPEC(gen_ks.dompath, 00, TRWAPI_NSID, TRWAPI_NS(97), RW_SCHEMA_ELEMENT_TYPE_CONTAINER)

  status = rw_keyspec_path_find_msg_desc_msg(
      reinterpret_cast<rw_keyspec_path_t *>(&gen_ks), 
      nullptr,
      (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(TestRwapis_data_Config),
      0, &msg_out, &rem);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE((msg_out == (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(TestRwapis_data_Config)));
  EXPECT_EQ(rem, nullptr);
  rw_schema_path_spec__free_unpacked_usebody(NULL, &gen_ks);

  FREE_KEY_SPEC1()

  rw_keyspec_path_t *ks1 = rw_keyspec_path_create_dup_of_type(
    reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_output_RpcOutOnly_C_Cc)),
    nullptr,
    &rw_schema_path_spec__descriptor);
  ASSERT_NE(nullptr, ks1);


  rw_status_t rs = rw_keyspec_path_find_msg_desc_schema(ks1, nullptr ,
    reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2)),
    &msg_out, &rem);

  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  EXPECT_EQ( msg_out,
    (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(Testy2pTop2_output_RpcOutOnly_C_Cc));
  EXPECT_EQ(rem, nullptr);

  rs = rw_keyspec_path_find_msg_desc_schema(ks1, nullptr , NULL, &msg_out, &rem);
  EXPECT_EQ(rs, RW_STATUS_FAILURE);

  rw_keyspec_path_t *cks2 = nullptr;
  rw_keyspec_path_create_dup(
    reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_output_RpcOutOnly_C_Cc)),
    nullptr,
    &cks2);
  ASSERT_NE(nullptr, cks2);
  rs = rw_keyspec_path_find_msg_desc_schema(cks2, nullptr , NULL, &msg_out, &rem);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg_out, (const rw_yang_pb_msgdesc_t*)RWPB_G_PATHSPEC_YPBCD(Testy2pTop2_output_RpcOutOnly_C_Cc));
  EXPECT_EQ(rem, nullptr);

  rw_keyspec_path_free(ks1, nullptr);
  rw_keyspec_path_free(cks2, nullptr);
}

TEST(RwKeyspec, Create)
{
  {
    RWPB_T_PATHSPEC(Document_data_MainBook_Chapters) *key_spec = nullptr;
    const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Document_data_MainBook_Chapters));
    rw_keyspec_path_create_dup(const_keyspec, nullptr , reinterpret_cast<rw_keyspec_path_t**>(&key_spec));

    key_spec->dompath.has_path001 = true;
    key_spec->dompath.path001.has_key00 = true;
    key_spec->dompath.path001.key00.number = 10;

    rw_status_t s = rw_keyspec_path_update_binpath(reinterpret_cast<rw_keyspec_path_t *>(key_spec), NULL);
    EXPECT_EQ(s, RW_STATUS_SUCCESS);

    rw_keyspec_path_t *new_ks = rw_keyspec_path_create_concrete_with_dompath_binary_data(nullptr, &key_spec->binpath, key_spec->base.descriptor);
    EXPECT_TRUE(new_ks);

    auto *new1 = RWPB_F_PATHSPEC_CAST_OR_CRASH(Document_data_MainBook_Chapters, new_ks);

    EXPECT_TRUE(new1->has_dompath);
    EXPECT_FALSE(new1->has_binpath);
    EXPECT_TRUE(new1->dompath.path001.has_key00);
    EXPECT_EQ(new1->dompath.path001.key00.number, 10);

    rw_keyspec_path_free(new_ks, nullptr);
    new_ks = rw_keyspec_path_create_with_dompath_binary_data(nullptr, &key_spec->binpath);
    EXPECT_TRUE(new_ks);

    RwSchemaPathSpec *new2 = reinterpret_cast<RwSchemaPathSpec *>(new_ks);
    EXPECT_TRUE(new2->dompath);
    EXPECT_FALSE(new2->has_binpath);
    EXPECT_TRUE(new2->dompath->path000);
    EXPECT_TRUE(new2->dompath->path001);
    EXPECT_FALSE(new2->dompath->path002);
    EXPECT_TRUE(new2->dompath->path001->key00);
    EXPECT_FALSE(new2->dompath->path001->key00->string_key);

    unsigned unk_count = protobuf_c_message_unknown_get_count(&new2->dompath->path001->key00->base);
    ASSERT_TRUE(unk_count);

    const ProtobufCMessageUnknownField* unk_fld = nullptr;
    for (unsigned i = 0; i < unk_count; i++) {
      auto tmp = protobuf_c_message_unknown_get_index(&new2->dompath->path001->key00->base, i);
      if (tmp->base_.tag == TDOC_NS(11)) {
        unk_fld = tmp;
        break;
      }
    }

    ASSERT_TRUE(unk_fld);
    EXPECT_EQ(unk_fld->serialized.len, 1);
    EXPECT_EQ(*(unsigned char *)unk_fld->serialized.data, 10);

    rw_keyspec_path_free(new_ks, nullptr);
    rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t*>(key_spec), nullptr);
  }
  {
    RWPB_M_MSG_DECL_INIT(Testy2pTop2_data_Topl_La,msg);
    msg.k1 = (char *)malloc(20);
    strcpy(msg.k1, "Key1");
    msg.k2 = 54;
    msg.has_k2 = 1;
    msg.k3 = (char *)malloc(20);
    strcpy(msg.k3, "Key2");
    msg.n_lb = 1;
    msg.lb = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)**)malloc(sizeof(void*));
    msg.lb[0] = (RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb) *)malloc(sizeof(RWPB_T_MSG(Testy2pTop2_data_Topl_La_Lb)));
    RWPB_F_MSG_INIT(Testy2pTop2_data_Topl_La_Lb,msg.lb[0]);
    msg.lb[0]->k4 = (char *)malloc(20);
    strcpy(msg.lb[0]->k4, "Key4");
    msg.lb[0]->k5 = 3114;
    msg.lb[0]->has_k5 = 1;

    rw_keyspec_entry_t *path_entry = rw_keyspec_entry_create_from_proto(nullptr, &msg.base);
    EXPECT_TRUE(path_entry);
    RWPB_T_PATHENTRY(Testy2pTop2_data_Topl_La) *msg_pe = nullptr;
    msg_pe = (RWPB_T_PATHENTRY(Testy2pTop2_data_Topl_La) *)path_entry;
    EXPECT_EQ(msg_pe->base.descriptor, RWPB_G_PATHENTRY_PBCMD(Testy2pTop2_data_Topl_La));

    EXPECT_EQ(msg_pe->element_id.element_tag, TY2PT2_NS(14));
    EXPECT_EQ(msg_pe->element_id.system_ns_id, TY2PT2_NSID);
    EXPECT_EQ(msg_pe->element_id.element_type, RW_SCHEMA_ELEMENT_TYPE_LISTY_3);
    EXPECT_TRUE(msg_pe->has_key00);
    EXPECT_TRUE(msg_pe->has_key01);
    EXPECT_TRUE(msg_pe->has_key02);

    EXPECT_STREQ(msg_pe->key00.k1, "Key1");
    EXPECT_EQ(msg_pe->key01.k2, 54);
    EXPECT_STREQ(msg_pe->key02.k3, "Key2");

    protobuf_c_message_free_unpacked(NULL, &msg_pe->base);
    protobuf_c_message_free_unpacked_usebody(NULL, &msg.base);
  }
  {
    RWPB_T_PATHSPEC(TestRwapis_data_Config_Manifest_Profile_InitPhase_Environment_Action_Annex) *ks = nullptr;
    rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(TestRwapis_data_Config_Manifest_Profile_InitPhase_Environment_Action_Annex)), nullptr ,
                          reinterpret_cast<rw_keyspec_path_t **>(&ks));

    EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 1));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 2));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 3));

    ks->dompath.has_path002 = true;
    ks->dompath.path002.has_key00 = true;
    ks->dompath.path002.key00.name = strdup("Name1");

    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 3));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 4));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 5));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 6));

    ks->dompath.path005.has_key00 = true;
    //free(ks->dompath.path002.key00.name);
    ks->dompath.path005.key00.name = strdup("Action1");

    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 6));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 7));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(ks)));

    rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks), nullptr);
  }
  {
    RWPB_T_PATHSPEC(Testy2pTop2_data_Topl_La_Lb) *ks = nullptr;
    rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pTop2_data_Topl_La_Lb)), nullptr ,
                          reinterpret_cast<rw_keyspec_path_t **>(&ks));

    EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 1));

    ks->dompath.has_path000 = true;
    ks->dompath.path000.has_key00 = true;
    ks->dompath.path000.key00.k0 = strdup("Key0");

    EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 1));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 2));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(ks)));

    ks->dompath.has_path001= true;
    ks->dompath.path001.has_key00 = true;
    ks->dompath.path001.key00.k1 = strdup("Key1");

    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 2));

    ks->dompath.path001.has_key01 = true;
    ks->dompath.path001.key01.k2 = 2;

    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 2));

    ks->dompath.path001.has_key02 = true;
    ks->dompath.path001.key02.k3 = strdup("Key3");

    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 2));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(ks)));

    ks->dompath.has_path002 = true;
    ks->dompath.path002.has_key00 = true;
    ks->dompath.path002.key00.k4 = strdup("Key4");

    EXPECT_TRUE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_TRUE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 3));

    ks->dompath.path002.has_key01 = true;
    ks->dompath.path002.key01.k5 = 5;

    EXPECT_FALSE(rw_keyspec_path_has_wildcards(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_last_entry(reinterpret_cast<rw_keyspec_path_t *>(ks)));
    EXPECT_FALSE(rw_keyspec_path_has_wildcards_for_depth(reinterpret_cast<rw_keyspec_path_t *>(ks), 3));

    rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks), nullptr);
  }
}

TEST(RwKeyspec, DomPathCategory)
{
  rw_keyspec_path_t *ks1 = nullptr;
  rw_keyspec_path_create_dup(
    reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Document_data_MainBook_Chapters)),
    nullptr,
    &ks1);
  ASSERT_NE(nullptr, ks1);

  rw_keyspec_path_t *ks2 = nullptr;
  rw_keyspec_path_create_dup(ks1, nullptr , &ks2);
  ASSERT_NE(nullptr, ks2);

  EXPECT_EQ(rw_keyspec_path_get_category(ks1), RW_SCHEMA_CATEGORY_ANY);
  EXPECT_EQ(rw_keyspec_path_get_category(ks2), RW_SCHEMA_CATEGORY_ANY);

  size_t entry_index = 0;
  int key_index = 0;
  size_t start_index = 0;
  size_t len = 0;
  EXPECT_TRUE(rw_keyspec_path_is_equal(nullptr, ks1, ks2));
  EXPECT_TRUE(rw_keyspec_path_is_equal_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, ks1, ks2, &entry_index));
  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, ks1, ks2, false/*ignorekeys*/, &start_index, &len));


  rw_keyspec_path_set_category(ks2, nullptr, RW_SCHEMA_CATEGORY_DATA);
  EXPECT_EQ(rw_keyspec_path_get_category(ks2), RW_SCHEMA_CATEGORY_DATA);

  EXPECT_FALSE(rw_keyspec_path_is_equal(nullptr, ks1, ks2));

  char buff[512];
  int ret = rw_keyspec_path_get_print_buffer(ks2, nullptr ,
              reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Document)), buff, 512);
  EXPECT_EQ(ret, 29);
  EXPECT_STREQ(buff, "D,/doc:main-book/doc:chapters");

  ret = rw_keyspec_path_get_print_buffer(ks2, nullptr , NULL, buff, 512);
  EXPECT_EQ(ret, 29);
  EXPECT_STREQ(buff, "D,/doc:main-book/doc:chapters");

  entry_index = 0;
  key_index = 0;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  entry_index = 0;
  key_index = 0;
  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, ks1, ks2, &entry_index));

  EXPECT_TRUE(rw_keyspec_path_is_branch_detail(nullptr, ks1, ks2, false/*ignorekeys*/, &start_index, &len));


  rw_keyspec_path_set_category(ks1, nullptr, RW_SCHEMA_CATEGORY_CONFIG);
  EXPECT_EQ(rw_keyspec_path_get_category(ks1), RW_SCHEMA_CATEGORY_CONFIG);

  EXPECT_FALSE(rw_keyspec_path_is_equal(nullptr, ks1, ks2));

  entry_index = 0;
  key_index = 0;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  entry_index = 0;
  key_index = 0;
  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, ks1, ks2, &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  EXPECT_FALSE(rw_keyspec_path_is_same_path_detail(nullptr, ks1, ks2, &entry_index));

  EXPECT_FALSE(rw_keyspec_path_is_branch_detail(nullptr, ks1, ks2, false/*ignorekeys*/, &start_index, &len));


  rw_keyspec_path_free(ks1, nullptr);
  rw_keyspec_path_free(ks2, nullptr);

  EXPECT_EQ(
    RWPB_G_PATHSPEC_VALUE(Testy2pTop2_input_RpcInOnly_C_Cc)->dompath.category,
    RW_SCHEMA_CATEGORY_RPC_INPUT);
  EXPECT_EQ(
    RWPB_G_PATHSPEC_VALUE(Document_data_MainBook_Chapters)->dompath.category,
    RW_SCHEMA_CATEGORY_ANY);
}

TEST(RwKeyspec, Match)
{
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t**>(&ks1));
  EXPECT_TRUE(ks1);

  ks1->dompath.has_path001 = true;
  ks1->dompath.path001.has_key00 = true;
  ks1->dompath.path001.key00.id = 100;

  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks2 = nullptr;
  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t**>(&ks2));
  EXPECT_TRUE(ks2);

  ks2->dompath.has_path001 = true;
  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 100;

  size_t entry_index = 0;
  int key_index = -1;
  EXPECT_TRUE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                         reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  ks2->dompath.path001.has_key00 = false;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                         reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                         reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  ks2->dompath.path001.has_key00 = true;
  ks2->dompath.path001.key00.id = 200;

  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                         reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                         reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, reinterpret_cast<rw_keyspec_path_t *>(ks1),
                                             reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index));
  EXPECT_EQ(entry_index, 0);

  ks2->dompath.path001.key00.id = 100;

  rw_status_t s = rw_keyspec_path_update_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks1), NULL);
  EXPECT_EQ(RW_STATUS_SUCCESS, s);

  ProtobufCBinaryData data;
  data.data = NULL;
  data.len = 0;
  rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t *>(ks1), nullptr , const_cast<const uint8_t **>(&data.data), &data.len);
  EXPECT_TRUE(data.data);

  rw_keyspec_path_t *g_ks = rw_keyspec_path_create_with_dompath_binary_data(nullptr, &data);
  EXPECT_TRUE(g_ks);

  EXPECT_TRUE(rw_keyspec_path_is_equal_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, g_ks,
                                             reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index));
  EXPECT_EQ(entry_index, 0);

  ks2->dompath.path001.key00.id = 200;
  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_FALSE(rw_keyspec_path_is_match_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, g_ks,
                                             reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index));
  EXPECT_EQ(entry_index, 0);

  rw_keyspec_path_free(g_ks, nullptr);

  g_ks = rw_keyspec_path_create_dup_of_type(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks);

  EXPECT_FALSE(rw_keyspec_path_is_equal_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 1);
  EXPECT_EQ(key_index, 0);

  EXPECT_TRUE(rw_keyspec_path_is_match_detail(nullptr, g_ks, reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index, &key_index));
  EXPECT_EQ(entry_index, 0);
  EXPECT_EQ(key_index, -1);

  EXPECT_TRUE(rw_keyspec_path_is_same_path_detail(nullptr, g_ks,
                                             reinterpret_cast<rw_keyspec_path_t *>(ks2), &entry_index));
  EXPECT_EQ(entry_index, 0);

  rw_keyspec_path_free(g_ks, nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks2), nullptr);
}

TEST(RwKeyspec, CopyKeysTest)
{
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *ks1 = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t**>(&ks1));

  RWPB_M_PATHENTRY_DECL_INIT(Company_data_Company_Employee,pe);

  pe.has_key00 = true;
  pe.key00.id = 100;


  rw_status_t status;
  status = rw_keyspec_path_add_keys_to_entry(&ks1->rw_keyspec_path_t,
                                              nullptr,
                                              &pe.rw_keyspec_entry_t,
                                              1);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(ks1->dompath.path001.has_key00);
  EXPECT_EQ(ks1->dompath.path001.key00.id, 100);

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks1), nullptr);

  RWPB_T_PATHSPEC(Testy2pSearchNode1_data_Tysn1T1_Lvl2l) *ks = nullptr;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t *>(RWPB_G_PATHSPEC_VALUE(Testy2pSearchNode1_data_Tysn1T1_Lvl2l)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t**>(&ks));

  RWPB_M_PATHENTRY_DECL_INIT(Testy2pSearchNode1_data_Tysn1T1_Lvl2l,pe1);

  pe1.has_key00 = true;
  pe1.key00.lfk1 = 140809;

  pe1.has_key01 = true;
  pe1.key01.lfk2 = 1122335;

  status = rw_keyspec_path_add_keys_to_entry(&ks->rw_keyspec_path_t,
                                              nullptr,
                                              &pe1.rw_keyspec_entry_t,
                                              1);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(ks->dompath.path001.has_key00);
  EXPECT_EQ(ks->dompath.path001.key00.lfk1, 140809);

  EXPECT_TRUE(ks->dompath.path001.has_key01);
  EXPECT_EQ(ks->dompath.path001.key01.lfk2, 1122335);

  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks), nullptr);
}

TEST(RwKeyspec, PrintPathEntry)
{
  const rw_keyspec_entry_t *pathe = reinterpret_cast<const rw_keyspec_entry_t *>(RWPB_G_PATHENTRY_VALUE(Company_data_Company_Product));

  char print_buff[512];
  int ret = rw_keyspec_entry_get_print_buffer(pathe, nullptr, print_buff, 512);
  EXPECT_EQ(ret, 15);
  EXPECT_STREQ(print_buff, "company:product");

  RWPB_M_PATHENTRY_DECL_INIT(Company_data_Company_Product,path_e);

  path_e.has_key00 = true;
  path_e.key00.id = 1431;

  ret = rw_keyspec_entry_get_print_buffer(
        reinterpret_cast<rw_keyspec_entry_t *>(&path_e), nullptr, print_buff, 512);

  EXPECT_EQ(ret, 34);
  EXPECT_STREQ(print_buff, "company:product[company:id='1431']");

  RwSchemaPathEntry generic;
  rw_schema_path_entry__init(&generic);

  generic.element_id = (RwSchemaElementId *)malloc(sizeof(RwSchemaElementId));
  rw_schema_element_id__init(generic.element_id);

  generic.element_id->system_ns_id = 100;
  generic.element_id->element_tag = 56789;
  generic.element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_5;

  generic.key00 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(generic.key00);
  generic.key00->string_key = strdup("Hello");

  generic.key01 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(generic.key01);
  generic.key01->string_key = strdup("World");

  generic.key02 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(generic.key02);
  generic.key02->string_key = strdup("Cloud");

  generic.key03 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(generic.key03);
  generic.key03->string_key = strdup("Computing");

  generic.key04 = (RwSchemaKey *)malloc(sizeof(RwSchemaKey));
  rw_schema_key__init(generic.key04);
  generic.key04->string_key = strdup("Rift");

  ret = rw_keyspec_entry_get_print_buffer(
       reinterpret_cast<rw_keyspec_entry_t *>(&generic), nullptr, print_buff, 512);
  EXPECT_EQ(ret, 78);
  EXPECT_STREQ(print_buff, "100:56789[Key1=\'Hello\',Key2=\'World\',Key3=\'Cloud\',Key4=\'Computing\',Key5=\'Rift\']");

  rw_schema_path_entry__free_unpacked_usebody(NULL, &generic);

  RWPB_M_PATHENTRY_DECL_INIT(Testy2pTop2_data_Topl_La,new_pe);

  new_pe.has_key00 = true;
  new_pe.key00.k1 = strdup("Happy");
  new_pe.has_key01 = true;
  new_pe.key01.k2 = 1408;
  new_pe.has_key02 = true;
  new_pe.key02.k3 = strdup("Computing");

  ret = rw_keyspec_entry_get_print_buffer(reinterpret_cast<rw_keyspec_entry_t *>(&new_pe), nullptr,
                                  print_buff, 512);
  EXPECT_EQ(ret, 69);
  EXPECT_STREQ(print_buff, "testy2p-top2:la[tyt2:k1='Happy'][tyt2:k2='1408'][tyt2:k3='Computing']");
  free(new_pe.key00.k1);
  free(new_pe.key02.k3);

  RWPB_T_PATHSPEC(Company_data_Company_Employee) *emp_ks = NULL;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t **>(&emp_ks));
  EXPECT_TRUE(emp_ks);

  emp_ks->dompath.has_path001 = true;
  emp_ks->dompath.path001.has_key00 = true;
  emp_ks->dompath.path001.key00.id = 100;

  char print_buf[512];
  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr ,
                                        reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
                                        print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");

  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, print_buf, 512);
  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");


  ProtobufCBinaryData bin;
  bin.data = nullptr;
  bin.len = 0;

  rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t*>(emp_ks), nullptr , const_cast<const uint8_t **>(&bin.data), &bin.len);

  rw_keyspec_path_t* g_ks = rw_keyspec_path_create_with_dompath_binary_data(nullptr, &bin);

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
                                    print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , NULL, print_buf, 512);

  EXPECT_FALSE(strncmp(print_buf, "A,/132267980:530055169/132267980:530055174[Unkey1=", strlen("A,/132267980:530055169/132267980:530055174[Unkey1=")));

  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, print_buf, 512);

  rw_status_t status = rw_keyspec_path_update_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr,
                                reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)));

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  const char *str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, print_buf);

  rw_keyspec_path_free(g_ks, nullptr);
  g_ks = rw_keyspec_path_create_dup_of_type(reinterpret_cast<rw_keyspec_path_t*>(emp_ks), nullptr , &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks);

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
                                    print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , NULL, print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");

  emp_ks->dompath.has_path001 = true;
  emp_ks->dompath.path001.key00.id = 200;

  str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, print_buf);

  str_path = nullptr;
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr, RW_SCHEMA_CATEGORY_CONFIG);
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, "C,/company:company/company:employee[company:id='200']");

  emp_ks->dompath.path001.key00.id = 300;
  status = rw_keyspec_path_update_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), NULL, NULL);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, "C,/company:company/company:employee[company:id='300']");

  rw_keyspec_path_free(g_ks, nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr);

  RWPB_T_PATHSPEC(FlatConversion_data_Container1_TwoKeys_UnrootedPb) *ks_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1_TwoKeys_UnrootedPb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr , reinterpret_cast<rw_keyspec_path_t**>(&ks_in));
  EXPECT_TRUE(ks_in);

  ks_in->dompath.has_path001 = true;
  ks_in->dompath.path001.has_key00 = true;
  ks_in->dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_SEVENTH;
  ks_in->dompath.path001.has_key01 = true;
  strcpy(ks_in->dompath.path001.key01.sec_string, "Second_key");

  ret = rw_keyspec_path_get_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , NULL, print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");

  g_ks = rw_keyspec_path_create_dup_of_type(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks);

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(FlatConversion)), print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");

  ret = rw_keyspec_path_get_print_buffer(g_ks, nullptr , NULL, print_buf, 512);

  std::string buf(print_buf);

  EXPECT_NE(buf.find("Unkey1"), std::string::npos);
  EXPECT_NE(buf.find("Unkey2"), std::string::npos);

  rw_keyspec_path_free(g_ks, nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks_in), nullptr);

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, fks);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr(&fks.rw_keyspec_path_t);
  fks.dompath.path000.has_key00 = true;
  strcpy(fks.dompath.path000.key00.name, "trafgen");
  fks.dompath.path001.has_key00 = true;
  strcpy(fks.dompath.path001.key00.name, "bundle1");

  ret = rw_keyspec_path_get_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");

  // Test keyspec and schema mis-match after some point.
  // Set an invalid leaf..
  fks.dompath.path002 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));
  rw_schema_path_entry__init(fks.dompath.path002);
  fks.dompath.path002->element_id = (RwSchemaElementId *)malloc(sizeof(RwSchemaElementId));
  rw_schema_element_id__init(fks.dompath.path002->element_id);
  fks.dompath.path002->element_id->system_ns_id = 1;
  fks.dompath.path002->element_id->element_tag = 66666;
  fks.dompath.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LEAF;

  ret = rw_keyspec_path_get_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']/1:66666");

  // Try a mis-matching container/list
  fks.dompath.path001.element_id.system_ns_id = 1;
  fks.dompath.path001.element_id.element_tag = 77777;
  ret = rw_keyspec_path_get_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         print_buf, 512);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/1:77777[Key1=\'bundle1\']/1:66666");
}

TEST(RwKeyspec, PrintPathEntryNew)
{
  int ret = -1;
  RWPB_T_PATHSPEC(Company_data_Company_Employee) *emp_ks = NULL;

  rw_keyspec_path_create_dup(reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee)), nullptr ,
                        reinterpret_cast<rw_keyspec_path_t **>(&emp_ks));
  EXPECT_TRUE(emp_ks);

  emp_ks->dompath.has_path001 = true;
  emp_ks->dompath.path001.has_key00 = true;
  emp_ks->dompath.path001.key00.id = 100;

  char *print_buf;
  ret = rw_keyspec_path_get_new_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), 
            nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
            &print_buf);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");
  free(print_buf);

  ret = rw_keyspec_path_get_new_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &print_buf);
  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");
  free(print_buf);


  ProtobufCBinaryData bin;
  bin.data = nullptr;
  bin.len = 0;

  rw_keyspec_path_get_binpath(reinterpret_cast<rw_keyspec_path_t*>(emp_ks), nullptr , const_cast<const uint8_t **>(&bin.data), &bin.len);

  rw_keyspec_path_t* g_ks = rw_keyspec_path_create_with_dompath_binary_data(nullptr, &bin);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
                                    &print_buf);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");
  free(print_buf);

  char print_buf_lt[32];
  const char* expected_str = "A,/...mployee[company:id='100']";
  ret = rw_keyspec_path_get_print_buffer_ltruncate(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)), print_buf_lt, 32);
  EXPECT_EQ(ret, strlen(expected_str));
  EXPECT_STREQ(print_buf_lt, expected_str);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , NULL, &print_buf);

  EXPECT_FALSE(strncmp(print_buf, "A,/132267980:530055169/132267980:530055174", strlen("A,/132267980:530055169/132267980:530055174")));
  free(print_buf);

  ret = rw_keyspec_path_get_new_print_buffer(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &print_buf);

  rw_status_t status = rw_keyspec_path_update_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr,
                                reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)));

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  const char *str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, print_buf);
  free(print_buf);

  rw_keyspec_path_free(g_ks, nullptr);
  g_ks = rw_keyspec_path_create_dup_of_type(reinterpret_cast<rw_keyspec_path_t*>(emp_ks), nullptr , &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Company)),
                                    &print_buf);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");
  free(print_buf);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , NULL, &print_buf);

  EXPECT_STREQ(print_buf, "A,/company:company/company:employee[company:id='100']");

  emp_ks->dompath.has_path001 = true;
  emp_ks->dompath.path001.key00.id = 200;

  str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, print_buf);
  free(print_buf);

  str_path = nullptr;
  rw_keyspec_path_set_category(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr, RW_SCHEMA_CATEGORY_CONFIG);
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, "C,/company:company/company:employee[company:id='200']");

  emp_ks->dompath.path001.key00.id = 300;
  status = rw_keyspec_path_update_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), NULL, NULL);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  str_path = nullptr;
  status = rw_keyspec_path_get_strpath(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr , NULL, &str_path);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(str_path);
  EXPECT_STREQ(str_path, "C,/company:company/company:employee[company:id='300']");

  rw_keyspec_path_free(g_ks, nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(emp_ks), nullptr);

  RWPB_T_PATHSPEC(FlatConversion_data_Container1_TwoKeys_UnrootedPb) *ks_in = nullptr;
  const rw_keyspec_path_t *const_keyspec = reinterpret_cast<const rw_keyspec_path_t*>(RWPB_G_PATHSPEC_VALUE(FlatConversion_data_Container1_TwoKeys_UnrootedPb));
  rw_keyspec_path_create_dup(const_keyspec, nullptr , reinterpret_cast<rw_keyspec_path_t**>(&ks_in));
  EXPECT_TRUE(ks_in);

  ks_in->dompath.has_path001 = true;
  ks_in->dompath.path001.has_key00 = true;
  ks_in->dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_SEVENTH;
  ks_in->dompath.path001.has_key01 = true;
  strcpy(ks_in->dompath.path001.key01.sec_string, "Second_key");

  ret = rw_keyspec_path_get_new_print_buffer(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , NULL, &print_buf);

  EXPECT_STREQ(print_buf, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");
  free(print_buf);

  g_ks = rw_keyspec_path_create_dup_of_type(reinterpret_cast<rw_keyspec_path_t*>(ks_in), nullptr , &rw_schema_path_spec__descriptor);
  EXPECT_TRUE(g_ks);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(FlatConversion)), &print_buf);

  EXPECT_STREQ(print_buf, "A,/conv:container-1/conv:two-keys[conv:prim-enum='seventh'][conv:sec-string='Second_key']/conv:unrooted-pb");
  free(print_buf);

  ret = rw_keyspec_path_get_new_print_buffer(g_ks, nullptr , NULL, &print_buf);

  std::string buf(print_buf);
  free(print_buf);

  EXPECT_NE(buf.find("Unkey1"), std::string::npos);
  EXPECT_NE(buf.find("Unkey2"), std::string::npos);

  rw_keyspec_path_free(g_ks, nullptr);
  rw_keyspec_path_free(reinterpret_cast<rw_keyspec_path_t *>(ks_in), nullptr);

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, fks);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t up(&fks.rw_keyspec_path_t);

  fks.dompath.path000.has_key00 = true;
  strcpy(fks.dompath.path000.key00.name, "trafgen");
  fks.dompath.path001.has_key00 = true;
  strcpy(fks.dompath.path001.key00.name, "bundle1");

  ret = rw_keyspec_path_get_new_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         &print_buf);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");
  free(print_buf);

  // Test keyspec and schema mis-match after some point.
  // Set an invalid leaf..
  fks.dompath.path002 = (RwSchemaPathEntry *)malloc(sizeof(RwSchemaPathEntry));
  rw_schema_path_entry__init(fks.dompath.path002);
  fks.dompath.path002->element_id = (RwSchemaElementId *)malloc(sizeof(RwSchemaElementId));
  rw_schema_element_id__init(fks.dompath.path002->element_id);
  fks.dompath.path002->element_id->system_ns_id = 1;
  fks.dompath.path002->element_id->element_tag = 66666;
  fks.dompath.path002->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LEAF;

  ret = rw_keyspec_path_get_new_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         &print_buf);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']/1:66666");
  free(print_buf);

  // Try a mis-matching container/list
  fks.dompath.path001.element_id.system_ns_id = 1;
  fks.dompath.path001.element_id.element_tag = 77777;
  ret = rw_keyspec_path_get_new_print_buffer(&fks.rw_keyspec_path_t, nullptr , (const rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwFpathD),
                                         &print_buf);

  EXPECT_STREQ(print_buf, "A,/rw-base:colony[rw-base:name='trafgen']/1:77777[Key1=\'bundle1\']/1:66666");
  free(print_buf);
}

TEST(RwKeyspec, RWPBMacros)
{
  RWPB_M_MSG_DECL_INIT(Company_data_Company,comp);

  comp.n_employee = 3;
  comp.employee = (RWPB_T_MSG(Company_data_Company_Employee) **)malloc(sizeof(void*)*comp.n_employee);
  for (unsigned i = 0; i < 3; i++) {
    comp.employee[i] = (RWPB_T_MSG(Company_data_Company_Employee) *)malloc(sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
    RWPB_F_MSG_INIT(Company_data_Company_Employee,comp.employee[i]);
    comp.employee[i]->id = 100+i;
    comp.employee[i]->has_id = 1;
    comp.employee[i]->name = (char *)malloc(50);
    strcpy(comp.employee[i]->name, "ABCD");
    comp.employee[i]->title = (char *)malloc(50);
    strcpy(comp.employee[i]->title, "Software Eng");
    comp.employee[i]->start_date = (char *)malloc(50);
    strcpy(comp.employee[i]->start_date, "12/12/2012");
  }

  size_t s = protobuf_c_message_get_packed_size(NULL, &comp.base);
  uint8_t buff[1024];
  size_t packed_size = protobuf_c_message_pack(NULL, &comp.base, buff);
  EXPECT_EQ(s, packed_size);

  RWPB_T_MSG(Company_data_Company) *msg = RWPB_F_MSG_UNPACK(Company_data_Company, NULL, s, buff);
  EXPECT_TRUE(msg);
  RWPB_F_MSG_FREE_AND_BODY(Company_data_Company, msg, NULL);

  RWPB_T_MSG(Company_data_Company) comp1;
  RWPB_F_MSG_INIT(Company_data_Company, &comp1);
  RWPB_T_MSG(Company_data_Company) *temp = RWPB_F_MSG_UNPACK_USE_BODY(Company_data_Company, NULL, s, buff, &comp1.base);
  EXPECT_EQ(temp, &comp1);
  RWPB_F_MSG_FREE_KEEP_BODY(Company_data_Company, temp, NULL);

  ProtobufCMessage *test = &comp.base;
  auto comp2 = RWPB_M_MSG_CAST(Company_data_Company, test);
  EXPECT_TRUE(comp2);

  auto mismat = RWPB_M_MSG_SAFE_CAST(Company_data_Company_Employee, test);
  EXPECT_FALSE(mismat);

  RWPB_T_MSG(Company_data_Company) *dup = RWPB_F_MSG_DUPLICATE(Company_data_Company, NULL, test);
  EXPECT_TRUE(dup);
  RWPB_F_MSG_FREE_AND_BODY(Company_data_Company, dup, NULL);

  RWPB_F_MSG_FREE_KEEP_BODY(Company_data_Company, &comp, NULL);

  // Tag numbers
  unsigned tag = RWPB_C_MSG_FIELD_TAG(Company_data_Company, employee);
  EXPECT_EQ(tag, RWPB_G_MSG_YPBCD(Company_data_Company_Employee)->ypbc.pb_element_tag);
}

TEST(RwKeyspec, PathEntryAPIs)
{
  Testy2pTop2__YangKeyspecEntry__Testy2pTop2__Data__Topl__La__Lb pe = *RWPB_G_PATHENTRY_VALUE(Testy2pTop2_Testy2pTop2_data_Topl_La_Lb);
  pe.has_key00 = true;
  pe.key00.k4 = strdup("Hello");

  pe.has_key01 = true;
  pe.key01.k5 = 100;

  rw_keyspec_entry_t *path_entry = nullptr;
  rw_status_t status = rw_keyspec_entry_create_dup(reinterpret_cast<rw_keyspec_entry_t *>(&pe),
                                                   nullptr,
                                                        &path_entry);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_TRUE(path_entry);

  auto *pe1 = RWPB_F_PATHENTRY_CAST_OR_CRASH(Testy2pTop2_Testy2pTop2_data_Topl_La_Lb, path_entry);
  EXPECT_TRUE(pe1->has_key00);
  EXPECT_STREQ(pe1->key00.k4, "Hello");

  EXPECT_TRUE(pe1->has_key01);
  EXPECT_EQ(pe1->key01.k5, 100);

  rw_keyspec_entry_free(path_entry, nullptr);

  path_entry = rw_keyspec_entry_create_dup_of_type(reinterpret_cast<rw_keyspec_entry_t *>(&pe),
                                                   nullptr,
                                                        &rw_schema_path_entry__descriptor);
  EXPECT_TRUE(path_entry);

  RwSchemaPathEntry *g_pe = reinterpret_cast<RwSchemaPathEntry *>(path_entry);
  EXPECT_TRUE(g_pe->key00);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&g_pe->key00->base), 1);
  EXPECT_TRUE(g_pe->key01);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&g_pe->key01->base), 1);

  rw_keyspec_entry_free(path_entry, nullptr);

  uint8_t buf[512];
  size_t len = rw_keyspec_entry_pack(reinterpret_cast<rw_keyspec_entry_t *>(&pe), nullptr, buf, 512);
  EXPECT_TRUE(len > 0);

  path_entry = rw_keyspec_entry_unpack(nullptr, RWPB_G_PATHENTRY_PBCMD(Testy2pTop2_Testy2pTop2_data_Topl_La_Lb),
                                            buf, len);
  EXPECT_TRUE(path_entry);

  auto *pe2 = RWPB_F_PATHENTRY_CAST_OR_CRASH(Testy2pTop2_Testy2pTop2_data_Topl_La_Lb, path_entry);
  EXPECT_TRUE(pe2->has_key00);
  EXPECT_STREQ(pe2->key00.k4, "Hello");

  EXPECT_TRUE(pe2->has_key01);
  EXPECT_EQ(pe2->key01.k5, 100);

  rw_keyspec_entry_free(path_entry, nullptr);

  path_entry = rw_keyspec_entry_unpack(nullptr, &rw_schema_path_entry__descriptor,
                                            buf, len);
  EXPECT_TRUE(path_entry);
  g_pe = reinterpret_cast<RwSchemaPathEntry *>(path_entry);
  EXPECT_TRUE(g_pe->key00);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&g_pe->key00->base), 1);
  EXPECT_TRUE(g_pe->key01);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&g_pe->key01->base), 1);

  rw_keyspec_entry_free(path_entry, nullptr);
  free(pe.key00.k4);
}

TEST(RwKeyspec, XpathToKS)
{
  // generate keyspec from xpath
  const rw_yang_pb_schema_t* schema =
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));
  rw_keyspec_path_t *keyspec = NULL;
  char xpath[1024];
  strcpy(xpath, "/company:company/company:employee[company:id='100']");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr(keyspec);

#define out_type RWPB_T_PATHSPEC(Company_data_Company_Employee)
#define KEYSPEC_OUT ((out_type *)keyspec)

  EXPECT_EQ(KEYSPEC_OUT->base.descriptor,
            RWPB_G_PATHSPEC_PBCMD(Company_data_Company_Employee));
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.rooted);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.key00.id, 100);

  char *xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2));
  strcpy(xpath, "/tyt2:topl[tyt2:k0='test0']/tyt2:la[tyt2:k1='test1'][tyt2:k2='20'][tyt2:k3='test3']");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Testy2pTop2_Testy2pTop2_data_Topl_La)

  EXPECT_EQ(KEYSPEC_OUT->base.descriptor,
            RWPB_G_PATHSPEC_PBCMD(Testy2pTop2_Testy2pTop2_data_Topl_La));
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.rooted);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.k0,"test0");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.k1,"test1");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key01);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.key01.k2,20);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key02);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key02.k3,"test3");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // The order of the keys should not matter
  strcpy(xpath, "/tyt2:topl[tyt2:k0='test0']/tyt2:la[tyt2:k3='test3'][tyt2:k1='test1'][tyt2:k2='20']");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Testy2pTop2_Testy2pTop2_data_Topl_La)

  EXPECT_EQ(KEYSPEC_OUT->base.descriptor,
            RWPB_G_PATHSPEC_PBCMD(Testy2pTop2_Testy2pTop2_data_Topl_La));
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.rooted);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.k0,"test0");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.k1,"test1");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key01);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.key01.k2,20);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key02);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key02.k3,"test3");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, "/tyt2:topl[tyt2:k0='test0']/tyt2:la[tyt2:k1='test1'][tyt2:k2='20'][tyt2:k3='test3']");
  free(xpathstr);

  // Test double quotes for predicates
  strcpy(xpath, "/tyt2:topl[tyt2:k0=\"test0\"]/tyt2:la[tyt2:k1=\"test1\"][tyt2:k2=\"20\"][tyt2:k3='test3']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

  EXPECT_EQ(KEYSPEC_OUT->base.descriptor,
            RWPB_G_PATHSPEC_PBCMD(Testy2pTop2_Testy2pTop2_data_Topl_La));
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.rooted);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.k0,"test0");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.k1,"test1");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key01);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.key01.k2, 20);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key02);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key02.k3,"test3");

  // generate keyspec for leaf nodes from xpath
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));
  strcpy(xpath, "/company:company/company:employee[company:id='100']/company:name");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);

  uptr.reset(keyspec);

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 3);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Company_data_Company_Employee)

  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.key00.id, 100);
  ASSERT_TRUE(KEYSPEC_OUT->dompath.path002); // leaf..
  ASSERT_TRUE(KEYSPEC_OUT->dompath.path002->element_id);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002->element_id->system_ns_id, 
            KEYSPEC_OUT->dompath.path000.element_id.system_ns_id);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002->element_id->element_type, RW_SCHEMA_ELEMENT_TYPE_LEAF);

  auto ypb_desc = RWPB_G_MSG_YPBCD(Company_data_Company_Employee);
  auto ypb_fdescs = ypb_desc->ypbc.ypbc_flddescs;
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002->element_id->element_tag, ypb_fdescs[1].pbc_fdesc->id);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // Wildcard xpath
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));
  strcpy(xpath, "/company:company/company:employee");
  keyspec = NULL;

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);

  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Company_data_Company_Employee)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 2);
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_FALSE(KEYSPEC_OUT->dompath.path001.has_key00);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // If instance identifier type, the api should fail.
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_INSTANCEID, nullptr);
  ASSERT_FALSE(keyspec);

  // Multiple keys
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2));
  strcpy(xpath, "/tyt2:topl[tyt2:k0='test0']/tyt2:la[tyt2:k3='test3']");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Testy2pTop2_Testy2pTop2_data_Topl_La)

  EXPECT_EQ(KEYSPEC_OUT->base.descriptor,
            RWPB_G_PATHSPEC_PBCMD(Testy2pTop2_Testy2pTop2_data_Topl_La));
  EXPECT_TRUE(KEYSPEC_OUT->has_dompath);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.rooted);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.k0,"test0");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_FALSE(KEYSPEC_OUT->dompath.path001.has_key00);

  EXPECT_FALSE(KEYSPEC_OUT->dompath.path001.has_key01);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key02);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key02.k3,"test3");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // If instance identifier type, the api should fail.
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_INSTANCEID, nullptr);
  ASSERT_FALSE(keyspec);

  // augments.
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(RwFpathD));

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_BundleEther)

  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.name, "trafgen");
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.name, "bundle1");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  strcpy(xpath, "D,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

  RwSchemaCategory category = rw_keyspec_path_get_category(keyspec);
  EXPECT_EQ(category, RW_SCHEMA_CATEGORY_DATA);

#undef out_type
#define out_type RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_BundleEther)

  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.name, "trafgen");
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.name, "bundle1");

  char buff[1024];
  rw_keyspec_path_get_print_buffer(keyspec, NULL, NULL, buff, sizeof(buff));
  EXPECT_STREQ(buff, xpath);

  // Error case.
  strcpy(xpath, "P,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // leaf-list
  strcpy(xpath, "/t:top/t:apis_test/t:leaflist[.='hello']");
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(TestYdomTop));

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(TestYdomTop_data_Top_ApisTest_Leaflist)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 3);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path002);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.path002.has_key00);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002.element_id.element_type, RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path002.key00.leaflist, "hello");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // wild-cards for leaf-list
  strcpy(xpath, "/t:top/t:apis_test/t:leaflist");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(TestYdomTop_data_Top_ApisTest_Leaflist)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 3);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path002);

  EXPECT_FALSE(KEYSPEC_OUT->dompath.path002.has_key00);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002.element_id.element_type, RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  strcpy(xpath, "/rw-base:colony/rw-base:network-context/rw-fpath:ip/rw-fpath:route[rw-fpath:prefix='13.0.1.23/32']");
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(RwFpathD));

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext_Ip_Route)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 4);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path002);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path003);

  EXPECT_FALSE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_FALSE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path003.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path003.key00.prefix, "13.0.1.23/32");

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // prefix mis-match
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(RwNcmgrDataE));
  strcpy(xpath, "A,/rw-base:colony[rw-base:name='iot_army']/rw-ncmgr:network-context-state[rw-ncmgr:name='iot_army']/rw-ncmgr:vrf[rw-ncmgr:name='default']/rw-fpath:ipv4/rw-fpath:route");

  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  strcpy(xpath, "/rw-base:colony[rw-base:name='iot_army']/rw-ncmgr:network-context-state[rw-ncmgr:name='iot_army']/rw-ncmgr:vrf[rw-ncmgr:name='default']/rw-ncmgr:ipv4/rw-ncmgr:route");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(RwNcmgrDataE_RwBaseE_data_Colony_NetworkContextState_Vrf_Ipv4_Route)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 5);

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path000.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path000.key00.name, "iot_army");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path001);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path001.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path001.key00.name, "iot_army");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path002);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.path002.has_key00);
  EXPECT_STREQ(KEYSPEC_OUT->dompath.path002.key00.name, "default");

  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path003);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path004);
  EXPECT_FALSE(KEYSPEC_OUT->dompath.path004.has_key00);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath);
  free(xpathstr);

  // Xpath for notifications.
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Testy2pTop2));
  strcpy(xpath, "N,/tyt2:n3");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(Testy2pTop2_notif_N3)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 1);
  EXPECT_EQ(rw_keyspec_path_get_category(keyspec), RW_SCHEMA_CATEGORY_NOTIFICATION);
  EXPECT_TRUE(KEYSPEC_OUT->dompath.has_path000);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, xpath+2);
  free(xpathstr);

  // omitting prefixes
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));
  strcpy(xpath, "/company/employee[id='100']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

  xpathstr = rw_keyspec_path_to_xpath(keyspec, schema, nullptr);
  ASSERT_TRUE(xpathstr);
  EXPECT_STREQ(xpathstr, "/company:company/company:employee[company:id='100']");
  free(xpathstr);

  // Test positional index for keyless list. Though the keyspec doesnt support
  // it, the parsing should not fail. We just ignore it while creating the
  // keyspec
  schema = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(FlatConversion));
  strcpy(xpath, "D,/conv:flat/conv:keyless[2]/conv:number");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);

#undef out_type
#define out_type RWPB_T_PATHSPEC(FlatConversion_data_Flat_Keyless)

  EXPECT_EQ(rw_keyspec_path_get_depth(keyspec), 3);
  EXPECT_EQ(rw_keyspec_path_get_category(keyspec), RW_SCHEMA_CATEGORY_DATA);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path001.element_id.element_type, RW_SCHEMA_ELEMENT_TYPE_LISTY_0);
  EXPECT_EQ(KEYSPEC_OUT->dompath.path002->element_id->element_type, RW_SCHEMA_ELEMENT_TYPE_LEAF);
}

TEST(RwKeyspec, XpathErrors)
{
  const rw_yang_pb_schema_t* schema =
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));
  rw_keyspec_path_t *keyspec = NULL;

  // non-rooted
  char xpath[1024];
  strcpy(xpath, "company:company/company:employee[company:id='100']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // Make sure the usage if '*' fails
  strcpy(xpath, "/company:company/company:employee[company:id='*']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // parse error
  strcpy(xpath, "/company:company/company:employee[]");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // some more parse error
  strcpy(xpath, "/company:company/company:");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // some more...
  strcpy(xpath, "/company:company/company:employee[company:id='100'");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  strcpy(xpath, "/company:company/company:employee[company:id");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // List index for a keyed list
  strcpy(xpath, "/company:company/company:employee[32]");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // Non-existing leaf node
  strcpy(xpath, "/company:company/company:employee/company:location");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // Multiple leafs
  strcpy(xpath, "/company:company/company:employee/company:name/company:address");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // Same key multiple times
  strcpy(xpath, "/company:company/company:employee[company:id='100'][company:id='100']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // non-existant key
  strcpy(xpath, "/company:company/company:employee[company:cosmic='100']");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);

  // Keys for a container
  strcpy(xpath, "/company:company[zombie='dead']/company:employee");
  keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);
}

TEST(RwKeyspec, DupTrunc)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony,ks_col);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState,ks_nc);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_PacketCounters,ks_ctrs);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContextState_PacketCounters_Fastpath,ks_fp);

  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony)* ks_col1
    = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony)*)rw_keyspec_path_create_dup_of_type_trunc(
        &ks_fp.rw_keyspec_path_t,
        nullptr,
        RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony));
  EXPECT_TRUE(rw_keyspec_path_is_equal(nullptr, &ks_col.rw_keyspec_path_t, &ks_col1->rw_keyspec_path_t));
  ASSERT_TRUE(ks_col1->has_dompath);
  EXPECT_TRUE(ks_col1->dompath.has_path000);
  EXPECT_FALSE(ks_col1->dompath.path001);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&ks_col1->dompath.base), 0);

  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony)* ks_col2
    = (RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony)*)rw_keyspec_path_create_dup_of_type_trunc(
        &ks_nc.rw_keyspec_path_t,
        nullptr,
        RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony));
  EXPECT_TRUE(rw_keyspec_path_is_equal(nullptr, &ks_col1->rw_keyspec_path_t, &ks_col2->rw_keyspec_path_t));
  ASSERT_TRUE(ks_col2->has_dompath);
  EXPECT_TRUE(ks_col2->dompath.has_path000);
  EXPECT_FALSE(ks_col2->dompath.path001);
  EXPECT_EQ(protobuf_c_message_unknown_get_count(&ks_col2->dompath.base), 0);

  rw_keyspec_path_free(&ks_col1->rw_keyspec_path_t, nullptr);
  rw_keyspec_path_free(&ks_col2->rw_keyspec_path_t, nullptr);
  protobuf_c_message_free_unpacked_usebody(NULL, &ks_col.base);
}

TEST(RwKeyspec, GetGiEntryFromKs)
{
  RWPB_M_PATHSPEC_DECL_INIT(Document_TableEntry, ks);
  const char* title = "Chapter 42";
  ks.dompath.path001.has_key00 = true;
  ks.dompath.path001.key00.title = (char*)title;

  const RwProtobufCMessageDescriptor_Document__YangData__Document__TableOfContents__Entry* pbcmd
    = document__yang_data__document__table_of_contents__entry__gi_schema();
  ASSERT_NE(pbcmd, nullptr);
  ASSERT_NE(pbcmd->base.ypbc_mdesc, nullptr);
  const rw_yang_pb_msgdesc_t* ks_ypb_in_box = rw_yang_pb_msgdesc_get_path_msgdesc(pbcmd->base.ypbc_mdesc);
  ASSERT_NE(ks_ypb_in_box, nullptr);
  const rw_yang_pb_msgdesc_t* ks_ypb_expected = ks.base.descriptor->ypbc_mdesc;
  ASSERT_NE(ks.base_concrete.descriptor, nullptr);
  EXPECT_EQ(ks_ypb_in_box, ks_ypb_expected);

  rwpb_gi_Document_YangKeyspecEntry_Document_Data_TableOfContents_Entry* pe
    = document__pbcmd__yang_data__document__table_of_contents__entry__gi_keyspec_to_entry(pbcmd, &ks.rw_keyspec_path_t);
  // ATTN: Need unique_ptr for freeing a box!
  ASSERT_NE(pe, nullptr);
  ASSERT_NE(pe->s.message, nullptr);
  const ProtobufCMessageDescriptor* pe_pbcmd_expected = RWPB_G_PATHENTRY_PBCMD(Document_TableEntry);
  const ProtobufCMessageDescriptor* pe_pbcmd_found = pe->s.message->base.descriptor;
  EXPECT_EQ(pe_pbcmd_found, pe_pbcmd_expected);

  EXPECT_TRUE(pe->s.message->has_key00);
  ASSERT_NE(pe->s.message->key00.title, nullptr);
  EXPECT_STREQ(pe->s.message->key00.title, title);

  document__yang_keyspec_entry__document__data__table_of_contents__entry__gi_unref(pe);
}

TEST(RwKeyspec, KSLeaf)
{
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, ks);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr(&ks.rw_keyspec_path_t);

  rw_status_t s = rw_keyspec_path_create_leaf_entry(&ks.rw_keyspec_path_t, nullptr, 
                                                    (rw_yang_pb_msgdesc_t *)RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data_Colony_BundleEther), "mtu");
  ASSERT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(rw_keyspec_path_get_depth(&ks.rw_keyspec_path_t), 3);

  auto entry = rw_keyspec_path_get_path_entry(&ks.rw_keyspec_path_t, 2);
  ASSERT_TRUE(entry);
  ASSERT_EQ(entry->base.descriptor, &rw_schema_path_entry__descriptor);

  auto spe = (RwSchemaPathEntry *)entry;
  auto fd = protobuf_c_message_descriptor_get_field_by_name(RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_BundleEther), 
                                                            "mtu");
  ASSERT_TRUE(fd);
  EXPECT_EQ(spe->element_id->element_type, RW_SCHEMA_ELEMENT_TYPE_LEAF);
  EXPECT_EQ(spe->element_id->element_tag, fd->id);
  EXPECT_EQ(spe->element_id->system_ns_id, RWPB_G_PATHENTRY_VALUE(RwFpathD_RwBaseD_data_Colony_BundleEther)->element_id.system_ns_id);
}

TEST(RwKeyspec, DeleteTest)
{
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, msg);

  UniquePtrProtobufCMessageUseBody<>::uptr_t muptr(&msg.base);

  strcpy(msg.name, "trafgen");
  msg.n_network_context = 1;
  msg.network_context = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext) **)malloc(sizeof(void*)*1);
  msg.network_context[0] = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)));
  RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, msg.network_context[0]);
  strcpy(msg.network_context[0]->name, "trafgen");
  msg.network_context[0]->has_name = 1;
  msg.network_context[0]->n_interface = 2;
  msg.network_context[0]->interface = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface) **)malloc(sizeof(void*)*2);

  for (unsigned i = 0; i < 2; i++) {
    msg.network_context[0]->interface[i] = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface) *)malloc(sizeof(RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)));
    RWPB_F_MSG_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, msg.network_context[0]->interface[i]);
    sprintf(msg.network_context[0]->interface[i]->name, "interface%d", i+1);
    msg.network_context[0]->interface[i]->has_name = 1;
    msg.network_context[0]->interface[i]->has_interface_type = true;
    msg.network_context[0]->interface[i]->interface_type.has_loopback = true;
    msg.network_context[0]->interface[i]->interface_type.loopback = true;
    msg.network_context[0]->interface[i]->n_ip = 4;
    strcpy(msg.network_context[0]->interface[i]->ip[0].address, "10.0.0.0");
    strcpy(msg.network_context[0]->interface[i]->ip[1].address, "10.0.0.1");
    strcpy(msg.network_context[0]->interface[i]->ip[2].address, "10.0.0.2");
    strcpy(msg.network_context[0]->interface[i]->ip[3].address, "10.0.0.3");
    msg.network_context[0]->interface[i]->has_bind = true;
    msg.network_context[0]->interface[i]->bind.has_port = true;
    strcpy(msg.network_context[0]->interface[i]->bind.port, "port1");
    msg.network_context[0]->interface[i]->bind.has_vlan = true;
    msg.network_context[0]->interface[i]->bind.vlan = 1;
  }

  msg.network_context[0]->n_lb_profile = 2;
  for (unsigned i = 0; i < 2; i++) {
    snprintf(msg.network_context[0]->lb_profile[i].name, 
             sizeof(msg.network_context[0]->lb_profile[i].name)-1, "lbprofile%d", i+1);
    msg.network_context[0]->lb_profile[i].has_name = 1;
    msg.network_context[0]->lb_profile[i].has_descr_string = true;
    snprintf(msg.network_context[0]->lb_profile[i].descr_string, sizeof(msg.network_context[0]->lb_profile[i].descr_string)-1,
             "Desc string");
    msg.network_context[0]->lb_profile[i].n_destination_nat = 2;
    strcpy(msg.network_context[0]->lb_profile[i].destination_nat[0].real_ip, "192.168.0.1");
    msg.network_context[0]->lb_profile[i].destination_nat[0].has_tcp_port = true;
    msg.network_context[0]->lb_profile[i].destination_nat[0].tcp_port = 5060;

    strcpy(msg.network_context[0]->lb_profile[i].destination_nat[1].real_ip, "192.168.0.2");
    msg.network_context[0]->lb_profile[i].destination_nat[1].has_udp_port = true;
    msg.network_context[0]->lb_profile[i].destination_nat[1].udp_port = 5060;
  }

  char xpath[1024];
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']");

  const rw_yang_pb_schema_t* schema =
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(RwFpathD));

  rw_keyspec_path_t *ks = NULL;
  ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(ks);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr(ks);

  // Delete a leaf
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                /rw-fpath:interface[rw-fpath:name='interface1']/rw-fpath:bind/rw-fpath:vlan");
  rw_keyspec_path_t *del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(del_ks);

  rw_status_t s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);

  EXPECT_FALSE(msg.network_context[0]->interface[0]->bind.has_vlan);

  // Delete a string leaf.
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                  /rw-fpath:lb-profile[rw-fpath:name='lbprofile1']/rw-fpath:descr-string");

  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);

  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_FALSE(msg.network_context[0]->lb_profile[0].has_descr_string);

  // Delete a container
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                  /rw-fpath:interface[rw-fpath:name='interface1']/rw-fpath:interface-type");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_FALSE(msg.network_context[0]->interface[0]->has_interface_type);

  // Delete a non-existent list entry
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='xyz']\
                  /rw-fpath:lb-profile[rw-fpath:name='lbprofile1']");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_FAILURE);

  // Delete a complete list
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                  /rw-fpath:interface[rw-fpath:name='interface2']/rw-fpath:ip");

  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);

  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.network_context[0]->interface[1]->n_ip, 0);

  // Delete a particular list element.
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                 /rw-fpath:lb-profile[rw-fpath:name='lbprofile1']/rw-fpath:destination-nat[rw-fpath:real-ip='192.168.0.1']");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);

  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.network_context[0]->lb_profile[0].n_destination_nat, 1);
  EXPECT_STREQ(msg.network_context[0]->lb_profile[0].destination_nat[0].real_ip, "192.168.0.2");

  // Delete a list element, pointy
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context[rw-base:name='trafgen']\
                   /rw-fpath:interface[rw-fpath:name='interface2']");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.network_context[0]->n_interface, 1);
  EXPECT_STREQ(msg.network_context[0]->interface[0]->name, "interface1");

  // Delete complte list, pointy
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']/rw-base:network-context");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_network_context, 0);
  EXPECT_FALSE(msg.network_context);

  // Test error case. Delete non-existent field.
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_FAILURE);

  // Same level delete.
  strcpy(xpath, "/rw-base:colony[rw-base:name='trafgen']");
  del_ks = NULL;
  del_ks = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(del_ks);

  uptr1.reset(del_ks);
  s = rw_keyspec_path_delete_proto_field(nullptr, ks, del_ks, &msg.base);
  EXPECT_EQ(s, RW_STATUS_FAILURE);
}

TEST(RwKeyspec, DelLeafListTest)
{
  RWPB_M_MSG_DECL_INIT(TestYdomTop_data_Top_ApisTest, msg);
  msg.n_leaflist = 4;
  msg.leaflist = (char **)malloc(4*sizeof(void*));
  msg.leaflist[0] = strdup("OpenStack");
  msg.leaflist[1] = strdup("OpenvSwitch");
  msg.leaflist[2] = strdup("OpenFlow");
  msg.leaflist[3] = strdup("OpenDayLight");

  msg.n_primlist = 5;
  msg.primlist = (int32_t *)malloc(sizeof(int32_t)*5);
  for (unsigned i = 0; i < 5; i++) {
    msg.primlist[i] = i+1;
  }

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&msg.base);

  // Create a del keyspec for leaf-list
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_data_Top_ApisTest_Leaflist, del_ks);
  del_ks.dompath.path002.has_key00 = true;
  del_ks.dompath.path002.key00.leaflist = strdup("OpenFlow");

  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr1(&del_ks.rw_keyspec_path_t);

  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_data_Top_ApisTest, msg_ks);
  
  rw_status_t status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                                          &del_ks.rw_keyspec_path_t,
                                                          &msg.base);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_leaflist, 3);
  EXPECT_STREQ(msg.leaflist[0], "OpenStack");
  EXPECT_STREQ(msg.leaflist[1], "OpenvSwitch");
  EXPECT_STREQ(msg.leaflist[2], "OpenDayLight");

  // Generic ks for del
  free(del_ks.dompath.path002.key00.leaflist);
  del_ks.dompath.path002.key00.leaflist = strdup("OpenvSwitch");
  rw_keyspec_path_t *ks = rw_keyspec_path_create_dup_of_type(&del_ks.rw_keyspec_path_t, nullptr , 
                                                             &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(ks);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr2(ks);

  status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                              ks,
                                              &msg.base);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_leaflist, 2);
  EXPECT_STREQ(msg.leaflist[0], "OpenStack");
  EXPECT_STREQ(msg.leaflist[1], "OpenDayLight");

  // Non-existent key
  free(del_ks.dompath.path002.key00.leaflist);
  del_ks.dompath.path002.key00.leaflist = strdup("SDN");
  status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                              &del_ks.rw_keyspec_path_t,
                                              &msg.base);
  EXPECT_EQ(status, RW_STATUS_FAILURE);

  // del keyspec for prim leaflist
  RWPB_M_PATHSPEC_DECL_INIT(TestYdomTop_data_Top_ApisTest_Primlist, del_ks1);
  del_ks1.dompath.path002.has_key00 = true;
  del_ks1.dompath.path002.key00.primlist = 3;

  status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                              &del_ks1.rw_keyspec_path_t,
                                              &msg.base);

  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_primlist, 4);
  EXPECT_EQ(msg.primlist[0], 1);
  EXPECT_EQ(msg.primlist[1], 2);
  EXPECT_EQ(msg.primlist[2], 4);
  EXPECT_EQ(msg.primlist[3], 5);

  // del all.
  del_ks.dompath.path002.has_key00 = false;
  status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                              &del_ks.rw_keyspec_path_t,
                                              &msg.base);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_leaflist, 0);

  del_ks1.dompath.path002.has_key00 = false;
  status = rw_keyspec_path_delete_proto_field(nullptr, &msg_ks.rw_keyspec_path_t,
                                              &del_ks1.rw_keyspec_path_t,
                                              &msg.base);
  EXPECT_EQ(status, RW_STATUS_SUCCESS);
  EXPECT_EQ(msg.n_primlist, 0);

  del_ks.dompath.path002.has_key00 = true;
  del_ks1.dompath.path002.has_key00 = true;
}

TEST(RwKeyspec, MessageAndKey)
{
  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer_GList, pb_msg);
  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer_GList_GclContainer, gc);
  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)* ks_sp = 0;


  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup
             ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_data_GeneralContainer_GList), nullptr, (rw_keyspec_path_t **)&ks_sp));

  rw_keyspec_path_t *ks = (rw_keyspec_path_t *)ks_sp;
  ASSERT_TRUE (rw_keyspec_path_matches_message (ks, nullptr, (ProtobufCMessage *) &pb_msg, true));
  ASSERT_FALSE (rw_keyspec_path_matches_message (ks, nullptr, (ProtobufCMessage *) &gc, true));
  ks_sp->dompath.path001.has_key00 = 1;
  ks_sp->dompath.path001.key00.index = 20;
  ASSERT_FALSE (rw_keyspec_path_matches_message (ks, nullptr, (ProtobufCMessage *) &pb_msg, true));
  pb_msg.index = 20;
  pb_msg.has_index = 1;
  pb_msg.gcl_container = &gc;
  ASSERT_TRUE (rw_keyspec_path_matches_message (ks, nullptr, (ProtobufCMessage *) &pb_msg, true));
  rw_keyspec_path_free(ks, nullptr);

  RWPB_M_PATHSPEC_DECL_INIT(RwAppmgrD_data_Ltesim_TestScript, kst);
  RWPB_M_MSG_DECL_INIT(RwAppmgrD_data_Ltesim_TestScript, msgt);

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptrm(&msgt.base);
  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptrk(&kst.rw_keyspec_path_t);

  kst.dompath.path001.has_key00 = true;
  kst.dompath.path001.key00.id = strdup("ping_error");

  msgt.id = strdup("ping_error");
  ASSERT_TRUE (rw_keyspec_path_matches_message (&kst.rw_keyspec_path_t, nullptr, &msgt.base, true));

  free(kst.dompath.path001.key00.id);
  kst.dompath.path001.key00.id = strdup("icmp_error");

  ASSERT_FALSE(rw_keyspec_path_matches_message (&kst.rw_keyspec_path_t, nullptr, &msgt.base, true));

  // Flat message, bumpy ks string
  RWPB_M_PATHSPEC_DECL_INIT(FlatConversion_data_Container1_TwoKeys,ks_tk);
  RWPB_M_MSG_DECL_INIT(FlatConversion_data_Container1_TwoKeys, tk_msg);

  ks_tk.dompath.path001.has_key01 = 1;
  ks_tk.dompath.path001.has_key00 = 1;
  
  strcpy (ks_tk.dompath.path001.key01.sec_string, "foo");
  ks_tk.dompath.path001.key00.prim_enum = BASE_CONVERSION_CB_ENUM_FIRST;
      
  strcpy (tk_msg.sec_string, "foo");
  tk_msg.has_sec_string = 1;

  tk_msg.prim_enum = BASE_CONVERSION_CB_ENUM_FIRST;
  tk_msg.has_prim_enum = 1;
  ASSERT_TRUE (rw_keyspec_path_matches_message (&ks_tk.rw_keyspec_path_t, nullptr, &tk_msg.base, true));

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, ks1);
  RWPB_M_MSG_DECL_INIT(RwBaseD_data_Colony_NetworkContext, msg1);

  ks1.dompath.path001.has_key00 = 1;
  strcpy (ks1.dompath.path001.key00.name, "networkctx1");
  strcpy (msg1.name, "networkctx1");
  msg1.has_name = 1;

  ASSERT_FALSE (rw_keyspec_path_matches_message (&ks1.rw_keyspec_path_t, nullptr, &msg1.base, true));
  ASSERT_TRUE  (rw_keyspec_path_matches_message (&ks1.rw_keyspec_path_t, nullptr, &msg1.base, false));

  strcpy (msg1.name, "networkctx2");
  msg1.has_name = 1;
  ASSERT_FALSE  (rw_keyspec_path_matches_message (&ks1.rw_keyspec_path_t, nullptr, &msg1.base, false));

  ASSERT_FALSE  (rw_keyspec_path_matches_message (&ks1.rw_keyspec_path_t, nullptr, &tk_msg.base, true));
  ASSERT_FALSE  (rw_keyspec_path_matches_message (&ks1.rw_keyspec_path_t, nullptr, &tk_msg.base, false));
}
  
TEST(RwKeyspec, ConstCorrectness)
{
  // Make sure that we can use the global const keyspecs safely in
  // the keyspec APIs.
  auto cks1 = RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee);
  auto cks2 = RWPB_G_PATHSPEC_VALUE(Company_data_Company_Employee);

  EXPECT_TRUE(rw_keyspec_path_is_equal(nullptr, &cks1->rw_keyspec_path_t, &cks2->rw_keyspec_path_t));
  EXPECT_FALSE(cks1->has_binpath);

  ProtobufCBinaryData bd;
  bd.data = nullptr;
  bd.len = 0;

  rw_keyspec_path_serialize_dompath(&cks1->rw_keyspec_path_t, nullptr , &bd);
  EXPECT_TRUE(bd.data);
  EXPECT_FALSE(cks1->has_binpath);

  RWPB_T_PATHSPEC(Company_data_Company_Employee) ks1;
  company__yang_keyspec_path__company__data__company__employee__init(&ks1);

  EXPECT_FALSE(ks1.has_dompath);
  EXPECT_FALSE(ks1.has_binpath);

  ks1.has_binpath = true;
  ks1.binpath.data = bd.data;
  ks1.binpath.len = bd.len;

  UniquePtrProtobufCMessageUseBody<rw_keyspec_path_t>::uptr_t uptr(&ks1.rw_keyspec_path_t);

  EXPECT_EQ(rw_keyspec_path_get_depth(&ks1.rw_keyspec_path_t), 2);
  EXPECT_FALSE(ks1.has_dompath);

  EXPECT_TRUE(rw_keyspec_path_is_listy(&ks1.rw_keyspec_path_t));
  EXPECT_TRUE(rw_keyspec_path_has_wildcards(&ks1.rw_keyspec_path_t));
  EXPECT_FALSE(ks1.has_dompath);
}

TEST(RwKeyspec, CircularErrorBuf)
{
  TEST_DESCRIPTION("Test keyspec circular error buffer");

  rw_keyspec_instance_t* inst = rw_keyspec_instance_default();
  ASSERT_TRUE(inst);

  keyspec_dump_error_records();
  keyspec_clear_error_records();

  for (unsigned i = 0; i < 70; i++) {
    char errormsg[512];
    snprintf(errormsg, sizeof(errormsg), "Keyspec Error Record %d", i+1);
    keyspec_default_error_cb(inst, errormsg);
  }

  // Test the keyspec error buffer export API
  RWPB_M_MSG_DECL_INIT(RwKeyspecStats_data_KeyspecEbuf_Tasklet, pbm);
  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr(&pbm.base);

  int nr = rw_keyspec_export_error_records(NULL, &pbm.base, "error_records", "time_stamp", "error_str");
  ASSERT_EQ(nr, 64);
  ASSERT_EQ(pbm.n_error_records, nr);
  ASSERT_TRUE(pbm.error_records);

  for (unsigned i = 0; i < pbm.n_error_records; i++) {
    ASSERT_TRUE(pbm.error_records[i]);
    char errormsg[512];
    snprintf(errormsg, sizeof(errormsg), "Keyspec Error Record %d", 70-i);
    EXPECT_STREQ(pbm.error_records[i]->error_str, errormsg);
  }
}

TEST(RwKeyspec, KeyspecErrors)
{
  TEST_DESCRIPTION("Test keyspec error handling");

  rw_keyspec_instance_t* inst = rw_keyspec_instance_default();
  ASSERT_TRUE(inst);

  keyspec_clear_error_records();

  // Create some real errors.
  RWPB_M_PATHSPEC_DECL_INIT(Company_data_Company_Employee, emp);
  RWPB_M_MSG_DECL_INIT(Company_data_Company_Employee, msg);
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);

  emp.dompath.path001.has_key00 = true;
  emp.dompath.path001.key00.id = 100;

  EXPECT_FALSE(rw_keyspec_path_reroot(&emp.rw_keyspec_path_t, NULL, &msg.base, &nc.rw_keyspec_path_t));

  RWPB_M_PATHSPEC_DECL_INIT(Company_data_Company, comp);
  RWPB_M_MSG_DECL_INIT(Company_data_Company, cmsg);

  RWPB_T_MSG(Company_data_Company_Employee)* emp_ar[1];
  msg.id = 500;
  msg.has_id = 1;
  emp_ar[0] = &msg;
  cmsg.n_employee = 1;
  cmsg.employee = &emp_ar[0];

  EXPECT_EQ(rw_keyspec_path_delete_proto_field(nullptr, &emp.rw_keyspec_path_t, &comp.rw_keyspec_path_t, &msg.base), RW_STATUS_FAILURE);

  EXPECT_FALSE(rw_keyspec_path_reroot(&comp.rw_keyspec_path_t, inst, &cmsg.base, &emp.rw_keyspec_path_t));

  RwSchemaPathSpec* pathsp = (RwSchemaPathSpec *)malloc(sizeof(RwSchemaPathSpec));
  rw_schema_path_spec__init(pathsp);

  UniquePtrKeySpecPath::uptr_t uptr(&pathsp->rw_keyspec_path_t);

  rw_keyspec_path_serialize_dompath(&emp.rw_keyspec_path_t, inst, &pathsp->binpath);

  pathsp->has_binpath = true;
  ASSERT_TRUE(pathsp->binpath.data);
  ASSERT_TRUE(pathsp->binpath.len);

  EXPECT_EQ(rw_keyspec_path_update_binpath(&pathsp->rw_keyspec_path_t, inst), RW_STATUS_FAILURE);
  keyspec_dump_error_records();
}

TEST(RwKeyspec, XpathRpc)
{
  const rw_yang_pb_schema_t* schema
    = reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(RwFpathD));
  rw_keyspec_path_t *keyspec = nullptr;
  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr;

  keyspec = rw_keyspec_path_from_xpath(schema, "/rw-fpath:start", RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);
  EXPECT_EQ(((ProtobufCMessage*)keyspec)->descriptor, RWPB_G_PATHSPEC_PBCMD(RwFpathD_input_Start));
  EXPECT_EQ(RW_SCHEMA_CATEGORY_RPC_INPUT, rw_keyspec_path_get_category(keyspec));
  keyspec = nullptr;

  keyspec = rw_keyspec_path_from_xpath(schema, "A,/rw-fpath:start", RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);
  EXPECT_EQ(((ProtobufCMessage*)keyspec)->descriptor, RWPB_G_PATHSPEC_PBCMD(RwFpathD_input_Start));
  EXPECT_EQ(RW_SCHEMA_CATEGORY_ANY, rw_keyspec_path_get_category(keyspec));
  keyspec = nullptr;

  keyspec = rw_keyspec_path_from_xpath(schema, "I,/rw-fpath:start", RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);
  EXPECT_EQ(((ProtobufCMessage*)keyspec)->descriptor, RWPB_G_PATHSPEC_PBCMD(RwFpathD_input_Start));
  EXPECT_EQ(RW_SCHEMA_CATEGORY_RPC_INPUT, rw_keyspec_path_get_category(keyspec));
  keyspec = nullptr;

  keyspec = rw_keyspec_path_from_xpath(schema, "O,/rw-fpath:start", RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset(keyspec);
  EXPECT_EQ(((ProtobufCMessage*)keyspec)->descriptor, RWPB_G_PATHSPEC_PBCMD(RwFpathD_output_Start));
  EXPECT_EQ(RW_SCHEMA_CATEGORY_RPC_OUTPUT, rw_keyspec_path_get_category(keyspec));
  keyspec = nullptr;

  keyspec = rw_keyspec_path_from_xpath(schema, "D,/rw-fpath:start", RW_XPATH_KEYSPEC, nullptr);
  ASSERT_FALSE(keyspec);
}

TEST(RwKeyspec, CreateRPCOut)
{
  RWPB_T_PATHSPEC(RwFpathE_input_Start) ip_start = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_input_Start));

  rw_keyspec_path_t* op_start = rw_keyspec_path_create_output_from_input(&ip_start.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_start);

  UniquePtrKeySpecPath::uptr_t uptr(op_start);

#define TEST_OUTPUT(output_, desc_) \
  EXPECT_EQ(output_->base.descriptor, desc_); \
  EXPECT_EQ(rw_keyspec_path_get_category(output_), RW_SCHEMA_CATEGORY_RPC_OUTPUT); \
  EXPECT_EQ(rw_keyspec_path_get_depth(output_), 1);

  TEST_OUTPUT(op_start, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_Start));

  RWPB_T_PATHSPEC(RwFpathE_input_Start_Colony) ip_start_colony = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_input_Start_Colony));
  op_start = rw_keyspec_path_create_output_from_input(&ip_start_colony.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_start);

  uptr.reset(op_start);
  TEST_OUTPUT(op_start, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_Start));

  RWPB_T_PATHSPEC(RwFpathE_input_Start_Colony_Traffic) ip_start_colony_traffic = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_input_Start_Colony_Traffic));
  op_start = rw_keyspec_path_create_output_from_input(&ip_start_colony_traffic.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_start);

  uptr.reset(op_start);
  TEST_OUTPUT(op_start, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_Start));

  RWPB_T_PATHSPEC(RwFpathE_output_Start) op_start1 = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_output_Start));
  op_start = rw_keyspec_path_create_output_from_input(&op_start1.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_start);
  ASSERT_NE(op_start, &op_start1.rw_keyspec_path_t);

  uptr.reset(op_start);
  TEST_OUTPUT(op_start, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_Start));

  RWPB_T_PATHSPEC(RwFpathE_input_FpathDebug) ip_fdb =  *(RWPB_G_PATHSPEC_VALUE(RwFpathE_input_FpathDebug));
  rw_keyspec_path_t* op_debug = rw_keyspec_path_create_output_from_input(&ip_fdb.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_debug);

  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));

  RWPB_T_PATHSPEC(RwFpathE_input_FpathDebug_Node_Show) ip_fdb_n_sh = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_input_FpathDebug_Node_Show));
  op_debug = rw_keyspec_path_create_output_from_input(&ip_fdb_n_sh.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_debug);

  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));

  RWPB_T_PATHSPEC(RwFpathE_output_FpathDebug_WorkInfo) op_fdb_wi = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_output_FpathDebug_WorkInfo));
  op_debug = rw_keyspec_path_create_output_from_input(&op_fdb_wi.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_debug);

  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));

  RWPB_T_PATHSPEC(RwFpathE_output_FpathDebug_WorkInfo_Work) op_fdb_wi_w = *(RWPB_G_PATHSPEC_VALUE(RwFpathE_output_FpathDebug_WorkInfo_Work));
  op_debug = rw_keyspec_path_create_output_from_input(&op_fdb_wi_w.rw_keyspec_path_t, NULL, NULL);
  ASSERT_TRUE(op_debug);

  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));

  // Error case.
  RWPB_T_PATHSPEC(RwFpathD_RwBaseD_data_Colony_NetworkContext) nc = *(RWPB_G_PATHSPEC_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext));
  op_debug = rw_keyspec_path_create_output_from_input(&nc.rw_keyspec_path_t, NULL, NULL);
  ASSERT_FALSE(op_debug);

  // Test Generic Keyspec
  rw_keyspec_path_t* ip_gks = rw_keyspec_path_create_dup_of_type(&ip_fdb_n_sh.rw_keyspec_path_t, NULL, &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(ip_gks);

  UniquePtrKeySpecPath::uptr_t uptr1(ip_gks);

  op_debug = rw_keyspec_path_create_output_from_input(ip_gks, NULL, NULL);
  ASSERT_FALSE(op_debug);

  op_debug = rw_keyspec_path_create_output_from_input(ip_gks, NULL, (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwFpathE));
  ASSERT_TRUE(op_debug);
  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));

  ip_gks = rw_keyspec_path_create_dup_of_type(&ip_fdb.rw_keyspec_path_t, NULL, &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(ip_gks);
  uptr1.reset(ip_gks);

  op_debug = rw_keyspec_path_create_output_from_input(ip_gks, NULL, (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwFpathE));
  ASSERT_TRUE(op_debug);
  uptr.reset(op_debug);
  TEST_OUTPUT(op_debug, RWPB_G_PATHSPEC_PBCMD(RwFpathE_output_FpathDebug));
}

TEST(RwKeyspec, ModuleRootKS)
{
  TEST_DESCRIPTION("Test all the keyspec methods for root keyspecs");

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data, rks);

  rw_keyspec_path_t *gks = rw_keyspec_path_create_dup_of_type(&rks.rw_keyspec_path_t, NULL, &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(gks);
  UniquePtrKeySpecPath::uptr_t uptr(gks);

  const rw_yang_pb_msgdesc_t* result = nullptr;
  const rw_yang_pb_schema_t* schema = (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  auto status = rw_keyspec_path_find_msg_desc_schema(gks, NULL, schema, &result, NULL);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);
  ASSERT_TRUE(result);
  ASSERT_EQ(result, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data));

  result = nullptr;
  schema = (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwBaseD);
  status = rw_keyspec_path_find_msg_desc_schema(gks, NULL, schema, &result, NULL);
  ASSERT_EQ(status, RW_STATUS_SUCCESS);

  ASSERT_TRUE(result);
  ASSERT_EQ(result, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwBaseD_data));

  // Test print methods
  char print_buf[512];
  int ret = rw_keyspec_path_get_print_buffer(&rks.rw_keyspec_path_t, nullptr , NULL,
          print_buf, sizeof(print_buf));
  EXPECT_NE(ret, -1);
  EXPECT_STREQ(print_buf, "A,/");

  rw_keyspec_path_t *rootks = rw_keyspec_path_from_xpath(schema, "/", RW_XPATH_KEYSPEC, NULL);
  ASSERT_TRUE(rootks);
  uptr.reset(rootks);

  EXPECT_TRUE(rw_keyspec_path_is_root(rootks));
}

TEST(RwKeyspec, TruncDelUnknown)
{
  TEST_DESCRIPTION("Test the removal of unknown path entries by trunc API");

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface_Bind_BundleEther, ks);

  rw_keyspec_path_t *s_ks = rw_keyspec_path_create_dup_of_type (&ks.rw_keyspec_path_t,  
                                                                NULL, 
                                                                RWPB_G_PATHSPEC_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext));
  ASSERT_TRUE (s_ks);
  UniquePtrKeySpecPath::uptr_t ks_uptr(s_ks);

  // Bind and BundleEther should be in the unknowns
  auto dom_fd = protobuf_c_message_descriptor_get_field (s_ks->base.descriptor, RW_SCHEMA_TAG_KEYSPEC_DOMPATH);
  ASSERT_TRUE (dom_fd);

  ProtobufCFieldInfo fi = {};
  ASSERT_TRUE (protobuf_c_message_get_field_instance (NULL, &s_ks->base, dom_fd, 0, &fi));
  ASSERT_TRUE (fi.element);

  EXPECT_EQ (protobuf_c_message_unknown_get_count ((ProtobufCMessage *)fi.element), 2);

  rw_status_t s = rw_keyspec_path_trunc_suffix_n (s_ks, NULL, 2);
  EXPECT_EQ (s, RW_STATUS_SUCCESS);

  EXPECT_EQ (protobuf_c_message_unknown_get_count ((ProtobufCMessage *)fi.element), 0);
  EXPECT_EQ (rw_keyspec_path_get_depth (s_ks), 2);
}
