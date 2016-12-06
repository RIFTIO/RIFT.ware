/* 
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
 * */

/**
 * @file rw_xml_dom_merger_test.cpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 2016/03/24
 * @brief Tests for RwCLI
 */

#include "rw_xml_dom_merger.hpp"
#include "rw_xml_validate_dom.hpp"
#include "rwlib.h"
#include "rwut.h"
#include "vehicle.pb-c.h"

using namespace rw_yang;

TEST(XMergerMerge, Incremental)
{
  TEST_DESCRIPTION("Test XMLDocMerger with incremental DOM Changes");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("uagent-utest");
  ASSERT_TRUE(test_mod);

  XMLDocument::uptr_t base_dom(mgr->create_document());

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:leaf-1_1.1>TEST</conv:leaf-1_1.1>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());

  ASSERT_TRUE(new_dom.get());

  //std::cout << delta_dom->to_string_pretty() << std::endl;
  //std::cout << new_dom->to_string_pretty() << std::endl;
  EXPECT_EQ(new_dom->to_string_pretty(), delta_xml);

  std::string delta2_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>1</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  XMLDocument::uptr_t delta2_dom(mgr->create_document_from_string(
      delta2_xml.c_str(), error_out, false));
  XMLDocMerger merger2(mgr.get(), new_dom.get());

  merger2.on_update_ = [&delta2_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta2_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom2 = merger2.copy_and_merge(delta2_dom.get());

  ASSERT_TRUE(new_dom2.get());

  std::string expected_str2 =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:leaf-1_1.1>TEST</conv:leaf-1_1.1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>1</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  EXPECT_EQ(new_dom2->to_string_pretty(), expected_str2);

  // Add one more list item
  std::string delta3_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>2</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  XMLDocument::uptr_t delta3_dom(mgr->create_document_from_string(
      delta3_xml.c_str(), error_out, false));
  XMLDocMerger merger3(mgr.get(), new_dom2.get());
  merger3.on_update_ = [&delta3_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta3_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom3 = merger3.copy_and_merge(delta3_dom.get());

  ASSERT_TRUE(new_dom3.get());

  //std::cout << new_dom3->to_string_pretty() << std::endl;
  std::string expected_str3 =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:leaf-1_1.1>TEST</conv:leaf-1_1.1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>1</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>2</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  EXPECT_EQ(new_dom3->to_string_pretty(), expected_str3);

  // Change the leaf value and add another list
  std::string delta4_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:leaf-1_1.1>TEST42</conv:leaf-1_1.1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>3</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  XMLDocument::uptr_t delta4_dom(mgr->create_document_from_string(
      delta4_xml.c_str(), error_out, false));
  XMLDocMerger merger4(mgr.get(), new_dom3.get());
  merger4.on_update_ = [&delta4_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta4_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom4 = merger4.copy_and_merge(delta4_dom.get());

  ASSERT_TRUE(new_dom4.get());

  //std::cout << new_dom4->to_string_pretty() << std::endl;
  std::string expected_str4 =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <conv:container-1 xmlns:conv=\"http://riftio.com/ns/core/mgmt/rwuagent/test/uagent-test\">\n"
      "    <conv:container_1-1>\n"
      "      <conv:leaf-1_1.1>TEST42</conv:leaf-1_1.1>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>1</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>2</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "      <conv:list-1.1_2>\n"
      "        <conv:int_1.1.2_1>3</conv:int_1.1.2_1>\n"
      "      </conv:list-1.1_2>\n"
      "    </conv:container_1-1>\n"
      "  </conv:container-1>\n\n"
      "</data>";
  EXPECT_EQ(new_dom4->to_string_pretty(), expected_str4);
}

TEST(XMergerMerge, Empty)
{
  TEST_DESCRIPTION("Test XMLDocMerger with empty base and empty delta");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocument::uptr_t delta_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  std::string expected_str = "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"/>";
  EXPECT_EQ(expected_str, new_dom->to_string_pretty());
}

TEST(XMergerMerge, ListDelete)
{
  TEST_DESCRIPTION("Test XMLDocMerger for delete");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Test delete of Toyota:Camry
  std::string camry_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\">\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t camry_del_dom(mgr->create_document_from_string(
      camry_del.c_str(), error_out, false));
  if (!camry_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(camry_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  del_merger1.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    std::cout << "Updated XML:" << update_node->to_string_pretty() << std::endl;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    std::string expected = "C,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Camry']";
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(expected, ks_str);
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [](rw_yang_netconf_op_status_t nc_status,
                             const char* err_msg, const char* err_path) {
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(camry_del_dom.get());
  ASSERT_TRUE(new_dom1.get());

  std::string expected_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(new_dom1->to_string_pretty(), expected_xml);
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);

  // Test delete of Honda:*
  std::string honda_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\"/>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t honda_del_dom(mgr->create_document_from_string(
      honda_del.c_str(), error_out, false));
  if (!honda_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(honda_del_dom.get());

  is_updated = false;
  is_deleted = false;
  XMLDocMerger del_merger2(mgr.get(), new_dom1.get(), ypbc_schema);

  del_merger2.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    std::cout << "Updated XML:" << update_node->to_string_pretty() << std::endl;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> expected_ks {
    "C,/vehicle:car[vehicle:brand='Honda']/vehicle:models[vehicle:name='Accord']",
        "C,/vehicle:car[vehicle:brand='Honda']/vehicle:models[vehicle:name='Civic']"
        };
  del_merger2.on_delete_ = [&ypbc_schema, &is_deleted, &expected_ks](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(expected_ks.back(), ks_str);
    expected_ks.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  del_merger2.on_error_ = [](rw_yang_netconf_op_status_t nc_status,
                             const char* err_msg, const char* err_path) {
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom2 = del_merger2.copy_and_merge(honda_del_dom.get());
  ASSERT_TRUE(new_dom2.get());

  std::string expected_xml2 =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(new_dom2->to_string_pretty(), expected_xml2);
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_EQ(0, expected_ks.size());
}

TEST(XMergerDelete, ListDeleteError)
{
  TEST_DESCRIPTION("Test XMLDocMerger for delete non-existing");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Test delete of Toyota:Etios (non-existing)
  std::string camry_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\">\n"
      "      <vehicle:name>Etios</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t camry_del_dom(mgr->create_document_from_string(
      camry_del.c_str(), error_out, false));
  if (!camry_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(camry_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  del_merger1.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    std::cout << "Updated XML:" << update_node->to_string_pretty() << std::endl;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    EXPECT_EQ(nc_status, RW_YANG_NETCONF_OP_STATUS_DATA_MISSING);
    EXPECT_STREQ(err_path, "A,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Etios']");
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(camry_del_dom.get());
  ASSERT_FALSE(new_dom1.get());
  EXPECT_FALSE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_TRUE(is_error);
}

TEST(XMergerDelete, RemoveParentContainer)
{
  TEST_DESCRIPTION("Test XMLDocMerger for a list delete deleting parent container");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:heavy xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:passenger>\n"
      "      <vehicle:bus>\n"
      "        <vehicle:brand>Volvo</vehicle:brand>\n"
      "        <vehicle:model>12345</vehicle:model>\n"
      "        <vehicle:capacity>30</vehicle:capacity>\n"
      "        <vehicle:axles>2</vehicle:axles>\n"
      "      </vehicle:bus>\n"
      "      <vehicle:bus>\n"
      "        <vehicle:brand>Benz</vehicle:brand>\n"
      "        <vehicle:model>54321</vehicle:model>\n"
      "        <vehicle:capacity>35</vehicle:capacity>\n"
      "        <vehicle:axles>2</vehicle:axles>\n"
      "      </vehicle:bus>\n"
      "    </vehicle:passenger>\n"
      "  </vehicle:heavy>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_EQ(delta_xml, new_dom->to_string_pretty());

  // Delete all the bus elements
  std::string bus_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:heavy xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:passenger>\n"
      "      <vehicle:bus xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\"/>\n"
      "    </vehicle:passenger>\n"
      "  </vehicle:heavy>\n\n"
      "</data>";
  XMLDocument::uptr_t bus_del_dom(mgr->create_document_from_string(
      bus_del.c_str(), error_out, false));
  if (!bus_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(bus_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  del_merger1.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> deleted_ks = {
    "C,/vehicle:heavy/vehicle:passenger/vehicle:bus[vehicle:brand='Benz']",
    "C,/vehicle:heavy/vehicle:passenger/vehicle:bus[vehicle:brand='Volvo']"
  };
  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted, &deleted_ks](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(deleted_ks.back(), ks_str);
    deleted_ks.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(bus_del_dom.get());
  ASSERT_TRUE(new_dom1.get());
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(deleted_ks.empty());

  // Empty DOM expected
  std::string expected_dom =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"/>";
  EXPECT_EQ(expected_dom, new_dom1->to_string_pretty());
}

TEST(XMergerDelete, Container)
{
  TEST_DESCRIPTION("Test XMLDocMerger for deleting a container");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:heavy xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:passenger>\n"
      "      <vehicle:bus>\n"
      "        <vehicle:brand>Volvo</vehicle:brand>\n"
      "        <vehicle:model>12345</vehicle:model>\n"
      "        <vehicle:capacity>30</vehicle:capacity>\n"
      "        <vehicle:axles>2</vehicle:axles>\n"
      "      </vehicle:bus>\n"
      "      <vehicle:bus>\n"
      "        <vehicle:brand>Benz</vehicle:brand>\n"
      "        <vehicle:model>54321</vehicle:model>\n"
      "        <vehicle:capacity>35</vehicle:capacity>\n"
      "        <vehicle:axles>2</vehicle:axles>\n"
      "      </vehicle:bus>\n"
      "    </vehicle:passenger>\n"
      "  </vehicle:heavy>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_EQ(delta_xml, new_dom->to_string_pretty());

  // Delete all the bus elements
  std::string bus_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:heavy xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:passenger xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\"/>\n"
      "  </vehicle:heavy>\n\n"
      "</data>";
  XMLDocument::uptr_t bus_del_dom(mgr->create_document_from_string(
      bus_del.c_str(), error_out, false));
  if (!bus_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(bus_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  del_merger1.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> deleted_ks = {
    "C,/vehicle:heavy/vehicle:passenger",
  };
  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted, &deleted_ks](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(deleted_ks.back(), ks_str);
    deleted_ks.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(bus_del_dom.get());
  ASSERT_TRUE(new_dom1.get());
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(deleted_ks.empty());

  // Empty DOM expected
  std::string expected_dom =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"/>";
  EXPECT_EQ(expected_dom, new_dom1->to_string_pretty());
}

TEST(XMergerMerge, ListCreate)
{
  TEST_DESCRIPTION("Test XMLDocMerger for List operation = create");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Test create of Toyota:Etios (new entry)
  std::string camry_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"create\">\n"
      "      <vehicle:name>Etios</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t camry_del_dom(mgr->create_document_from_string(
      camry_del.c_str(), error_out, false));
  if (!camry_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(camry_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  // This will not have the operation=create
  std::string update_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Etios</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  del_merger1.on_update_ = [&is_updated, &update_xml](XMLNode* update_node) {
    is_updated = true;
    EXPECT_EQ(update_xml, update_node->to_string_pretty());
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    std::cout << "Error message: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(camry_del_dom.get());
  ASSERT_TRUE(new_dom1.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);

  std::string expected_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Etios</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(expected_xml, new_dom1->to_string_pretty());
}

TEST(XMergerMerge, ListCreateError)
{
  TEST_DESCRIPTION("Test XMLDocMerger for Creating existing entry");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Test create of existing entry
  std::string camry_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"create\">\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t camry_del_dom(mgr->create_document_from_string(
      camry_del.c_str(), error_out, false));
  if (!camry_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(camry_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger del_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  del_merger1.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    std::cout << "Updated XML: " << update_node->to_string_pretty() << std::endl;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_delete_ = [&ypbc_schema, &is_deleted](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };

  del_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS, nc_status);
    EXPECT_STREQ("A,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Camry']", err_path);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = del_merger1.copy_and_merge(camry_del_dom.get());
  ASSERT_FALSE(new_dom1.get());
  EXPECT_FALSE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_TRUE(is_error);

}

TEST(XMergerMerge, InsertDefault)
{
  TEST_DESCRIPTION("Test XMLDocMerger for inserting a default value when not specified");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);
  std::function<std::string(std::string)> xml_template = [](std::string insert) {
    return std::string(
        "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
        "  <vehicle:heavy xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
        "    <vehicle:passenger>\n"
        "      <vehicle:bus>\n"
        "        <vehicle:brand>Volvo</vehicle:brand>\n"
        "        <vehicle:model>12345</vehicle:model>\n"
        "        <vehicle:capacity>30</vehicle:capacity>\n"
        + insert +
        "      </vehicle:bus>\n"
        "      <vehicle:bus>\n"
        "        <vehicle:brand>Benz</vehicle:brand>\n"
        "        <vehicle:model>54321</vehicle:model>\n"
        "        <vehicle:capacity>35</vehicle:capacity>\n"
        "        <vehicle:ac/>\n"
        + insert +
        "      </vehicle:bus>\n"
        "    </vehicle:passenger>\n"
        "  </vehicle:heavy>\n\n"
        "</data>"
                       );
  };

  std::string error_out;
  std::string delta_xml = xml_template("");
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  std::string expected_xml = xml_template(
      "        <vehicle:axles>1</vehicle:axles>\n");

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error   = false;
  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&is_updated, &expected_xml](XMLNode* update_node) {
    EXPECT_EQ(expected_xml, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_delete_ = [&is_deleted] (UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                 const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_EQ(expected_xml, new_dom->to_string_pretty());
}

TEST(XMergerMerge, InsertDefaultComplex)
{
  TEST_DESCRIPTION("Test XMLDocMerger for inserting a default values on a list");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);
  std::function<std::string(std::string,std::string,std::string)> xml_template = [](
      std::string insert1,
      std::string insert2,
      std::string insert3) {
    return std::string(
        "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
        "  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
        "    <vehicle:name>Toyota Camry SE</vehicle:name>\n"
        + insert1 +
        "  </vehicle:car-config>\n\n"
        "  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
        "    <vehicle:name>Honda Accord S</vehicle:name>\n"
        "    <vehicle:engine>\n"
        "      <vehicle:displacement>2000</vehicle:displacement>\n"
        + insert2 +
        "    </vehicle:engine>\n"
        + insert3 +
        "  </vehicle:car-config>\n\n"
        "</data>"
                       );
  };

  std::string error_out;
  std::string delta_xml = xml_template("","","");
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  std::string expected_xml = xml_template(
      "    <vehicle:air-bags>2</vehicle:air-bags>\n"
      "    <vehicle:engine>\n"
      "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n"
      "    </vehicle:engine>\n"
      "    <vehicle:item4>item4val</vehicle:item4>\n",
      "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n",
      "    <vehicle:air-bags>2</vehicle:air-bags>\n"
      "    <vehicle:item4>item4val</vehicle:item4>\n"
                                          );

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error   = false;
  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&is_updated, &expected_xml](XMLNode* update_node) {
    EXPECT_EQ(expected_xml, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_delete_ = [&is_deleted] (UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                 const char* err_msg, const char* err_path) {
    std::cout << "Error: " << std::endl;
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_EQ(expected_xml, new_dom->to_string_pretty());
}

TEST(XMergerMerge, DISABLED_InsertDefaultDeep)
{
  TEST_DESCRIPTION("Test XMLDocMerger for inserting a default value deep within a container");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("test_default");
  ASSERT_TRUE(test_mod);

  XMLDocument::uptr_t empty_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), empty_dom.get(), nullptr);

  std::string expected_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <td:deep xmlns:td=\"http://riftio.com/ns/core/mgmt/uagent/test/test_default\">\n"
"    <td:has-default>\n"
"      <td:def>is-default</td:def>\n"
"      <td:also-has-default>\n"
"        <td:also-def>also-default</td:also-def>\n"
"      </td:also-has-default>\n"
"    </td:has-default>\n"
"  </td:deep>\n\n"
"</data>";

  bool is_update = false;
  bool is_error  = false;
  bool is_delete = false;
  merger.on_update_ = [&is_update, &expected_xml](XMLNode* update_node) {
    is_update = true;
    EXPECT_EQ(expected_xml, update_node->to_string_pretty());
    return RW_STATUS_SUCCESS;
  };
  merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                  const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_delete_ = [&is_delete](UniquePtrKeySpecPath::uptr_t del_ks) {
    is_delete = true;
    return RW_STATUS_SUCCESS;
  };
  
  XMLDocument::uptr_t default_dom = merger.copy_and_merge(empty_dom.get());
  ASSERT_TRUE(default_dom.get());
  EXPECT_TRUE(is_update);
  EXPECT_FALSE(is_error);
  EXPECT_FALSE(is_delete);

  EXPECT_EQ(expected_xml, default_dom->to_string_pretty());
}

TEST(XMergerMerge, NoMerge)
{
  TEST_DESCRIPTION("Test XMLDocMerger for unchanged edit");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::function<std::string(std::string)> xml_template = [](std::string insert) {
    return
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      + insert +
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  };

  std::string error_out;
  std::string delta_xml = xml_template(
      "      <vehicle:category>Sedan</vehicle:category>\n");

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Update with existing values
  std::string update_xml = 
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t update_dom(mgr->create_document_from_string(
                                 update_xml.c_str(), error_out, false));
  if (!update_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(update_dom.get());


  bool is_update = false;
  bool is_error  = false;
  bool is_delete = false;

  XMLDocMerger new_merger(mgr.get(), new_dom.get(), ypbc_schema);
  new_merger.on_update_ = [&is_update](XMLNode* update_node) {
    is_update = true;
    return RW_STATUS_SUCCESS;
  };
  new_merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };
  new_merger.on_delete_ = [&is_delete](UniquePtrKeySpecPath::uptr_t del_ks) {
    is_delete = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t updated_dom = new_merger.copy_and_merge(update_dom.get()); 
  ASSERT_TRUE(updated_dom.get());
  EXPECT_FALSE(is_update);
  EXPECT_FALSE(is_error);
  EXPECT_FALSE(is_delete);
  EXPECT_EQ(delta_xml, updated_dom->to_string_pretty());

  // Update with one unchnaged and one modified
  std::string update1_xml = 
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Sedan C</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t update1_dom(mgr->create_document_from_string(
                                 update1_xml.c_str(), error_out, false));
  if (!update1_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(update1_dom.get());

  is_update = false;
  is_error  = false;
  is_delete = false;

  XMLDocMerger new_merger1(mgr.get(), new_dom.get(), ypbc_schema);
  new_merger1.on_update_ = [&is_update](XMLNode* update_node) {
    is_update = true;
    std::string expected_update =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
"    <vehicle:brand>Toyota</vehicle:brand>\n"
"    <vehicle:models>\n"
"      <vehicle:name>Corolla</vehicle:name>\n"
"      <vehicle:category>Sedan C</vehicle:category>\n"
"    </vehicle:models>\n"
"  </vehicle:car>\n\n"
"</data>";
    EXPECT_EQ(expected_update, update_node->to_string_pretty());
    return RW_STATUS_SUCCESS;
  };
  new_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };
  new_merger1.on_delete_ = [&is_delete](UniquePtrKeySpecPath::uptr_t del_ks) {
    is_delete = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t updated1_dom = new_merger1.copy_and_merge(update1_dom.get()); 
  ASSERT_TRUE(updated1_dom.get());
  EXPECT_TRUE(is_update);
  EXPECT_FALSE(is_error);
  EXPECT_FALSE(is_delete);

  std::string expected_xml = xml_template(
      "      <vehicle:category>Sedan C</vehicle:category>\n");
    
  EXPECT_EQ(expected_xml, updated1_dom->to_string_pretty());
}

TEST(XMergerReplace, SimpleListReplace)
{
  TEST_DESCRIPTION("Test XMLDocMerger for a simple list entry replace");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Compact-Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Replace an existing entry
  std::string replace_camry_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"replace\">\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>4</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t replace_camry_dom(mgr->create_document_from_string(
      replace_camry_xml.c_str(), error_out, false));
  if (!replace_camry_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(replace_camry_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger replace_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  std::string expected_update =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>4</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  replace_merger1.on_update_ = [&is_updated, &expected_update](XMLNode* update_node) {
    EXPECT_EQ(expected_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_delete_ = [&is_deleted, &ypbc_schema](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    std::cout << "Deleted: " << ks_str << std::endl;
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                          const char* err_msg, const char* err_path) {
    is_error = true;
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t replaced_dom = replace_merger1.copy_and_merge(replace_camry_dom.get());
  ASSERT_TRUE(replaced_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);
  std::string expected_config =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Compact-Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>4</vehicle:capacity>\n"
      "      <vehicle:category>Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(expected_config, replaced_dom->to_string_pretty());
}

TEST(XMergerReplace, ListReplaceDelLeaf)
{
  TEST_DESCRIPTION("Test XMLDocMerger for a list entry replace with a leaf del");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Compact-Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Replace an existing entry
  std::string replace_camry_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"replace\">\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t replace_camry_dom(mgr->create_document_from_string(
      replace_camry_xml.c_str(), error_out, false));
  if (!replace_camry_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(replace_camry_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger replace_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  std::string expected_update =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  replace_merger1.on_update_ = [&is_updated, &expected_update](XMLNode* update_node) {
    EXPECT_EQ(expected_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_delete_ = [&is_deleted, &ypbc_schema](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_STREQ("C,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Corolla']/vehicle:category", ks_str);
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                          const char* err_msg, const char* err_path) {
    is_error = true;
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t replaced_dom = replace_merger1.copy_and_merge(replace_camry_dom.get());
  ASSERT_TRUE(replaced_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  std::string expected_config =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(expected_config, replaced_dom->to_string_pretty());
}

TEST(XMergerReplace, ListReplaceNew)
{
  TEST_DESCRIPTION("Test XMLDocMerger for a non-existing list entry replace");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Compact-Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  // Replace an existing entry
  std::string replace_camry_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"replace\">\n"
      "      <vehicle:name>Sienna</vehicle:name>\n"
      "      <vehicle:capacity>7</vehicle:capacity>\n"
      "      <vehicle:category>Mini-Van</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  XMLDocument::uptr_t replace_camry_dom(mgr->create_document_from_string(
      replace_camry_xml.c_str(), error_out, false));
  if (!replace_camry_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(replace_camry_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger replace_merger1(mgr.get(), new_dom.get(), ypbc_schema);

  std::string expected_update =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Sienna</vehicle:name>\n"
      "      <vehicle:capacity>7</vehicle:capacity>\n"
      "      <vehicle:category>Mini-Van</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  replace_merger1.on_update_ = [&is_updated, &expected_update](XMLNode* update_node) {
    EXPECT_EQ(expected_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_delete_ = [&is_deleted, &ypbc_schema](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    std::cout << "Deleted: " << ks_str << std::endl;
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  // Not expected
  replace_merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                          const char* err_msg, const char* err_path) {
    is_error = true;
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t replaced_dom = replace_merger1.copy_and_merge(replace_camry_dom.get());
  ASSERT_TRUE(replaced_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);
  std::string expected_config =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "      <vehicle:category>Compact-Sedan</vehicle:category>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Sienna</vehicle:name>\n"
      "      <vehicle:capacity>7</vehicle:capacity>\n"
      "      <vehicle:category>Mini-Van</vehicle:category>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";
  EXPECT_EQ(expected_config, replaced_dom->to_string_pretty());
}

TEST(XMergerReplace, ComplexListReplace)
{
  TEST_DESCRIPTION("Test XMLDocMerger for replacing a complex list");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string top_fragment =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n";
  std::string original_items =
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Corolla</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Camry</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n";

  std::string bottom_fragment =
      "  </vehicle:car>\n\n"
      "  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:brand>Honda</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Civic</vehicle:name>\n"
      "      <vehicle:capacity>2</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Accord</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
      "  </vehicle:car>\n\n"
      "</data>";

  std::string replace_items =
      "    <vehicle:brand>Toyota</vehicle:brand>\n"
      "    <vehicle:models>\n"
      "      <vehicle:name>Sienna</vehicle:name>\n"
      "      <vehicle:capacity>3</vehicle:capacity>\n"
      "    </vehicle:models>\n"
"    <vehicle:models>\n"
"      <vehicle:name>Fortuner</vehicle:name>\n"
"      <vehicle:capacity>4</vehicle:capacity>\n"
"    </vehicle:models>\n";

  std::string error_out;
  std::string delta_xml = top_fragment +  original_items + bottom_fragment;

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
                                      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  std::string replace_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\" xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"replace\">\n"
+  replace_items +
"  </vehicle:car>\n\n"
"</data>";
  XMLDocument::uptr_t replace_dom(mgr->create_document_from_string(
                                      replace_xml.c_str(), error_out, false));
  if (!replace_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger replace_merger(mgr.get(), new_dom.get(), ypbc_schema);

  std::string expected_update =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
+  replace_items +
"  </vehicle:car>\n\n"
"</data>";

  replace_merger.on_update_ = [&is_updated, &expected_update](XMLNode* update_node) {
    EXPECT_EQ(expected_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> expected_dels = {
    "C,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Camry']",
    "C,/vehicle:car[vehicle:brand='Toyota']/vehicle:models[vehicle:name='Corolla']"
  };

  replace_merger.on_delete_ = [&is_deleted, &ypbc_schema, &expected_dels](
                        UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(expected_dels.back(), ks_str);
    expected_dels.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  replace_merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                          const char* err_msg, const char* err_path) {
    is_error = true;
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t replaced_dom = replace_merger.copy_and_merge(replace_dom.get());
  ASSERT_TRUE(replaced_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(expected_dels.empty());

  std::string expected_config = top_fragment + replace_items + bottom_fragment;

  EXPECT_EQ(expected_config, replaced_dom->to_string_pretty());
}

TEST(XMergerMerge, ChoiceCase)
{
  TEST_DESCRIPTION("Test XMLDocMerger for Choice Case");
  const rw_yang_pb_schema_t* ypbc_schema =
    reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);
  std::function<std::string(std::string,std::string,std::string)> xml_template = [](
                                              std::string insert1,
                                              std::string insert2,
                                              std::string insert3) {
    return std::string(
    "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
    "  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
    "    <vehicle:name>Toyota Camry SE</vehicle:name>\n"
    + insert1 +
    "  </vehicle:car-config>\n\n"
    "  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
    "    <vehicle:name>Honda Accord S</vehicle:name>\n"
    "    <vehicle:engine>\n"
    "      <vehicle:displacement>2000</vehicle:displacement>\n"
    + insert2 +
    "    </vehicle:engine>\n"
    + insert3 +
    "  </vehicle:car-config>\n\n"
    "</data>"
    );
  };

  std::string error_out;
  std::string delta_xml = xml_template("","","");
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
                                      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  std::string expected_xml = xml_template(
    "    <vehicle:air-bags>2</vehicle:air-bags>\n"
    "    <vehicle:engine>\n"
    "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n"
    "    </vehicle:engine>\n"
    "    <vehicle:item4>item4val</vehicle:item4>\n",
    "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n",
    "    <vehicle:air-bags>2</vehicle:air-bags>\n"
    "    <vehicle:item4>item4val</vehicle:item4>\n"
  );

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error   = false;
  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&is_updated, &expected_xml](XMLNode* update_node) {
    EXPECT_EQ(expected_xml, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_delete_ = [&is_deleted] (UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };
  merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                          const char* err_msg, const char* err_path) {
    std::cout << "Error: " << std::endl;
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_EQ(expected_xml, new_dom->to_string_pretty());

  // Update a choice, check if the old default case is getting removed
  std::function<std::string(std::string)> xml1_template = [](std::string insert1) {
    return std::string(
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
"    <vehicle:name>Toyota Camry SE</vehicle:name>\n"
"    <vehicle:item1>Item1</vehicle:item1>\n"
+ insert1 +
"  </vehicle:car-config>\n\n"
"</data>"
    );
  };

  is_updated = false;
  is_deleted = false;
  is_error   = false;
  std::string delta1_xml = xml1_template("");
  std::string expected1_update = xml1_template(
"    <vehicle:item2>item2val</vehicle:item2>\n"
  );
  std::string expected1_xml = xml_template(
    "    <vehicle:air-bags>2</vehicle:air-bags>\n"
    "    <vehicle:engine>\n"
    "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n"
    "    </vehicle:engine>\n"
    "    <vehicle:item1>Item1</vehicle:item1>\n"
    "    <vehicle:item2>item2val</vehicle:item2>\n",
    "      <vehicle:fuel-type>petrol</vehicle:fuel-type>\n",
    "    <vehicle:air-bags>2</vehicle:air-bags>\n"
    "    <vehicle:item4>item4val</vehicle:item4>\n"
  );

  XMLDocument::uptr_t delta1_dom(mgr->create_document_from_string(
                                      delta1_xml.c_str(), error_out, false));

  XMLDocMerger merger1(mgr.get(), new_dom.get(), ypbc_schema);
  merger1.on_update_ = [&is_updated, &expected1_update](XMLNode* update_node) {
    EXPECT_EQ(expected1_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };
  merger1.on_delete_ = [&is_deleted, &ypbc_schema] (UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    EXPECT_STREQ("C,/vehicle:car-config[vehicle:name='Toyota Camry SE']/vehicle:item4",
                 ks_str);
    is_deleted = true;
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };
  merger1.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                          const char* err_msg, const char* err_path) {
    std::cout << "Error: " << std::endl;
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new1_dom = merger1.copy_and_merge(delta1_dom.get());
  ASSERT_TRUE(new1_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_EQ(expected1_xml, new1_dom->to_string_pretty());

  // Delete the case items, the defaults case default item should get updated
  is_updated = false;
  is_deleted = false;
  is_error   = false;
  std::string delta2_xml =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
"    <vehicle:name>Toyota Camry SE</vehicle:name>\n"
"    <vehicle:item1 xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\"/>\n"
"    <vehicle:item2 xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\"/>\n"
"  </vehicle:car-config>\n\n"
"</data>";
  XMLDocument::uptr_t delta2_dom(mgr->create_document_from_string(
                                      delta2_xml.c_str(), error_out, false));
  if (!delta2_dom) {
    std::cout << "Dom create error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta2_dom.get());

  XMLDocMerger merger2(mgr.get(), new1_dom.get(), ypbc_schema);

  std::string expected2_update =
"\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
"  <vehicle:car-config xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
"    <vehicle:name>Toyota Camry SE</vehicle:name>\n"
"    <vehicle:item4>item4val</vehicle:item4>\n"
"  </vehicle:car-config>\n\n"
"</data>";

  merger2.on_update_ = [&is_updated, &expected2_update](XMLNode* update_node) {
    EXPECT_EQ(expected2_update, update_node->to_string_pretty());
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> expected_ks = {
    "C,/vehicle:car-config[vehicle:name='Toyota Camry SE']/vehicle:item2",
    "C,/vehicle:car-config[vehicle:name='Toyota Camry SE']/vehicle:item1"
  };

  merger2.on_delete_ = [&is_deleted, &ypbc_schema, &expected_ks] (
                                UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    EXPECT_EQ(expected_ks.back(), ks_str);
    expected_ks.pop_back();
    is_deleted = true;
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };
  merger2.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                          const char* err_msg, const char* err_path) {
    std::cout << "Error: "  << std::endl;
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new2_dom = merger2.copy_and_merge(delta2_dom.get());
  ASSERT_TRUE(new2_dom.get());
  EXPECT_TRUE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(expected_ks.empty());
  EXPECT_EQ(expected_xml, new2_dom->to_string_pretty());
}

TEST(XMergerDelete, RemoveUnderTopCont)
{
  TEST_DESCRIPTION("Test XMLDocMerger for a list delete under top-level container");
  const rw_yang_pb_schema_t* ypbc_schema =
      reinterpret_cast<const rw_yang_pb_schema_t *>(RWPB_G_SCHEMA_YPBCSD(Vehicle));

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  YangModule* test_mod = mgr->get_yang_model()->load_module("vehicle");
  mgr->get_yang_model()->load_schema_ypbc(ypbc_schema);
  mgr->get_yang_model()->register_ypbc_schema(ypbc_schema);
  ASSERT_TRUE(test_mod);

  std::string error_out;
  std::string delta_xml =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:mfg-catalog xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:mfg>\n"
      "      <vehicle:name>Volkswagen</vehicle:name>\n"
      "      <vehicle:hq-country>Germany</vehicle:hq-country>\n"
      "    </vehicle:mfg>\n"
      "    <vehicle:mfg>\n"
      "      <vehicle:name>Ford</vehicle:name>\n"
      "      <vehicle:hq-country>United States</vehicle:hq-country>\n"
      "    </vehicle:mfg>\n"
      "  </vehicle:mfg-catalog>\n\n"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(
      delta_xml.c_str(), error_out, false));
  if (!delta_dom) {
    std::cout << "Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t base_dom(mgr->create_document());
  XMLDocMerger merger(mgr.get(), base_dom.get(), ypbc_schema);
  merger.on_update_ = [&delta_xml](XMLNode* update_node) {
    EXPECT_EQ(update_node->to_string_pretty(), delta_xml);
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());
  EXPECT_EQ(delta_xml, new_dom->to_string_pretty());

  // Delete VW
  std::string vw_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:mfg-catalog xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:mfg xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\">\n"
      "      <vehicle:name>Volkswagen</vehicle:name>\n"
      "    </vehicle:mfg>\n"
      "   </vehicle:mfg-catalog>\n\n"
      "</data>";
  XMLDocument::uptr_t vw_del_dom(mgr->create_document_from_string(
      vw_del.c_str(), error_out, false));
  if (!vw_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(vw_del_dom.get());

  bool is_updated = false;
  bool is_deleted = false;
  bool is_error = false;
  XMLDocMerger vw_del_merger(mgr.get(), new_dom.get(), ypbc_schema);

  vw_del_merger.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  std::vector<std::string> deleted_ks = {
    "C,/vehicle:mfg-catalog/vehicle:mfg[vehicle:name='Volkswagen']"
  };
  vw_del_merger.on_delete_ = [&ypbc_schema, &is_deleted, &deleted_ks](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(deleted_ks.back(), ks_str);
    deleted_ks.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  vw_del_merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom1 = vw_del_merger.copy_and_merge(vw_del_dom.get());
  ASSERT_TRUE(new_dom1.get());
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(deleted_ks.empty());

  // Delete Ford
  std::string ford_del =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:mfg-catalog xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\">\n"
      "    <vehicle:mfg xmlns:nc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" nc:operation=\"delete\">\n"
      "      <vehicle:name>Ford</vehicle:name>\n"
      "    </vehicle:mfg>\n"
      "   </vehicle:mfg-catalog>\n\n"
      "</data>";
  XMLDocument::uptr_t ford_del_dom(mgr->create_document_from_string(
      ford_del.c_str(), error_out, false));
  if (!ford_del_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(ford_del_dom.get());

  is_updated = false;
  is_deleted = false;
  is_error = false;
  XMLDocMerger ford_del_merger(mgr.get(), new_dom1.get(), ypbc_schema);

  ford_del_merger.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  deleted_ks = std::vector<std::string> {
    "C,/vehicle:mfg-catalog/vehicle:mfg[vehicle:name='Ford']"
  };
  ford_del_merger.on_delete_ = [&ypbc_schema, &is_deleted, &deleted_ks](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    char* ks_str = nullptr;
    rw_keyspec_path_get_new_print_buffer(del_ks.get(), nullptr, ypbc_schema, &ks_str);
    is_deleted = true;
    EXPECT_EQ(deleted_ks.back(), ks_str);
    deleted_ks.pop_back();
    free(ks_str);
    return RW_STATUS_SUCCESS;
  };

  ford_del_merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom2 = ford_del_merger.copy_and_merge(ford_del_dom.get());
  ASSERT_TRUE(new_dom2.get());
  EXPECT_FALSE(is_updated);
  EXPECT_TRUE(is_deleted);
  EXPECT_FALSE(is_error);
  EXPECT_TRUE(deleted_ks.empty());

  // Empty DOM expected
  std::string expected_dom =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"/>";
  EXPECT_EQ(expected_dom, new_dom2->to_string_pretty());

  // Uselessly create mfg-catalog
  std::string create_cat =
      "\n<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">\n\n"
      "  <vehicle:mfg-catalog xmlns:vehicle=\"http://riftio.com/ns/core/mgmt/uagent/test/vehicle\"/>\n\n"
      "</data>";
  XMLDocument::uptr_t create_dom(mgr->create_document_from_string(
      create_cat.c_str(), error_out, false));
  if (!create_dom) {
    std::cout << error_out << std::endl;
  }
  ASSERT_TRUE(create_dom.get());

  is_updated = false;
  is_deleted = false;
  is_error = false;
  XMLDocMerger create_merger(mgr.get(), new_dom2.get(), ypbc_schema);

  create_merger.on_update_ = [&is_updated](XMLNode* update_node) {
    is_updated = true;
    return RW_STATUS_SUCCESS;
  };

  create_merger.on_delete_ = [&is_deleted](
      UniquePtrKeySpecPath::uptr_t del_ks) {
    is_deleted = true;
    return RW_STATUS_SUCCESS;
  };

  create_merger.on_error_ = [&is_error](rw_yang_netconf_op_status_t nc_status,
                                      const char* err_msg, const char* err_path) {
    is_error = true;
    return RW_STATUS_SUCCESS;
  };

  XMLDocument::uptr_t new_dom3 = create_merger.copy_and_merge(create_dom.get());
  ASSERT_TRUE(new_dom3.get());
  EXPECT_FALSE(is_updated);
  EXPECT_FALSE(is_deleted);
  EXPECT_FALSE(is_error);

  EXPECT_EQ(expected_dom, new_dom3->to_string_pretty());
}


