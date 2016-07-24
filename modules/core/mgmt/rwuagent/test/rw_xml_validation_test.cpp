/* 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 * */

/**
 * @file rw_xml_validation_test.cpp
 * @brief Tests for xml dom validation
 */

#include "rw_xml_dom_merger.hpp"
#include "rw_xml_validate_dom.hpp"
#include "rwlib.h"
#include "rwut.h"
#include "validation-other.pb-c.h"

using namespace rw_yang;

std::string get_rift_root()
{
  const char* rift_root = getenv("RIFT_ROOT");
  if (nullptr == rift_root) {
    std::cerr << "Unable to find $RIFT_ROOT" << std::endl;
    throw std::exception();
  }
  return std::string(rift_root);
}

TEST(Validation, xml_validation_negative_simple_leafref)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  XMLDocument::uptr_t base_dom(mgr->create_document());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <ref>asdf</ref>"
      "  </validation>"
      "</data>";
  
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_list_leafref)
{

  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  XMLDocument::uptr_t base_dom(mgr->create_document());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <validation-list>"
      "      <name>a key</name>"
      "      <ref>a leafref</ref>"
      "    </validation-list>"
      "  </validation>"
      "</data>";
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_list_leafref_to_leafref)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  XMLDocument::uptr_t base_dom(mgr->create_document());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <third xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <name>a key</name>"
      "    <ref>a leafref</ref>"
      "  </third>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_cross_module)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  XMLDocument::uptr_t base_dom(mgr->create_document());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation-module  xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation-other\">"
      "    <validation-list xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "        <name>a key</name>"
      "        <ref>a leafref</ref>"
      "    </validation-list>"
      "  </validation-module>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_augment)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  XMLDocument::uptr_t base_dom(mgr->create_document());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <aug xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <cross-ref xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation-other\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "      <name>a key</name>"
      "      <ref>a leafref</ref>"
      "    </cross-ref>"
      "  </aug>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());


  // ATTN: this doesn't keep the augments
  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_list_leafref_to_leafref_missing_target)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const delta_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <validation-list>"
      "      <name>a key</name>"
      "      <ref>original leafref</ref>"
      "    </validation-list>"
      "  </validation>"
      "  <third xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <name>a key</name>"
      "    <ref>a leafref</ref>"
      "  </third>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}

TEST(Validation, xml_validation_negative_list_of_leafrefs_missing_one)
{
  TEST_DESCRIPTION("This test is modeled after the nsd/vnfd constituent-vnfd-id-ref relationship.");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const delta_xml =
      "<data>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>cat</name>"
      "</possible-things>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>gecko</name>"
      "</possible-things>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>bald eagle</name>  "
      "</possible-things>"
      "<thing-collections xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>mammals</name>"
      "  <constituent-things>"
      "    <index>0</index>"
      "    <thing>dog</thing>"
      "  </constituent-things>"
      "  <constituent-things>"
      "    <index>1</index>"
      "    <thing>cat</thing>"
      "  </constituent-things>"
      "</thing-collections>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}

// positive tests

TEST(Validation, xml_validation_positive_list_of_constituent_things)
{
  TEST_DESCRIPTION("This test is modeled after the nsd/vnfd constituent-vnfd-id-ref relationship.");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const delta_xml =
      "<data>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>cat</name>"
      "</possible-things>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>dog</name>"
      "</possible-things>"
      "<possible-things xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>bald eagle</name>  "
      "</possible-things>"
      "<thing-collections xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <name>mammals</name>"
      "  <constituent-things>"
      "    <index>0</index>"
      "    <thing>dog</thing>"
      "  </constituent-things>"
      "  <constituent-things>"
      "    <index>1</index>"
      "    <thing>cat</thing>"
      "  </constituent-things>"
      "</thing-collections>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS) << result.reason();
}

TEST(Validation, xml_validation_positive_simple_leafref)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <str>original leafref</str>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <ref>original leafref</ref>"
      "  </validation>"
      "</data>";
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Validation, xml_validation_positive_list_leafref)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "<other-list>"
      "    <name>a key</name>"
      "    <str>original leafref</str>"
      "</other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <validation-list>"
      "      <name>a key</name>"
      "      <ref>original leafref</ref>"
      "    </validation-list>"
      "  </validation>"
      "</data>";
  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Validation, xml_validation_positive_list_leafref_to_leafref)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const base_xml =
      "<data>"
      "  <validation xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <validation-list>"
      "      <name>a key</name>"
      "      <ref>original leafref</ref>"
      "    </validation-list>"
      "  </validation>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <other-list>"
      "      <name>a key</name>"
      "      <str>original leafref</str>"
      "    </other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <third xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <name>a key</name>"
      "    <ref>original leafref</ref>"
      "  </third>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Validation, xml_validation_positive_cross_module)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <other-list>"
      "      <name>a key</name>"
      "      <str>original leafref</str>"
      "    </other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <validation-module xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation-other\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <validation-list>"
      "      <name>a key</name>"
      "      <ref>original leafref</ref>"
      "    </validation-list>"
      "  </validation-module>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Validation, xml_validation_positive_augment)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;
  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <other-list>"
      "      <name>a key</name>"
      "      <str>original leafref</str>"
      "    </other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "  <aug xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\" xmlns:xc=\"urn:ietf:params:xml:ns:netconf:base:1.0\" xc:operation=\"create\">"
      "    <cross-ref xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation-other\">"
      "      <name>a key</name>"
      "      <ref>original leafref</ref>"
      "    </cross-ref>"
      "  </aug>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Validation, xml_validation_positive_grouping)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<validation-module xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation-other\">"
      "  <top>"
      "    <source>"
      "      <name>a name</name>"
      "      <str>a str</str>"
      "    </source>"
      "    <ref-list>"
      "      <name>a name</name>"
      "      <ref>a str</ref>"
      "    </ref-list>"
      "  </top>"
      "</validation-module>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Mandatory, positive)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<mand>"
      "  <mand-leaf>3</mand-leaf>"
      "  <inner>"
      "    <k>k1</k>"
      "    <inner-leaf>2</inner-leaf>"      
      "    <bottom>"
      "      <bottom-leaf-a>1</bottom-leaf-a>"
      "      <bottom-leaf-b>0</bottom-leaf-b>"      
      "    </bottom>"
      "  </inner>"
      "</mand>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Mandatory, negative)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<mand>"
      "  <inner>"
      "    <inner-leaf>2</inner-leaf>"
      "    <bottom>"
      "      <bottom-leaf-a>1</bottom-leaf-a>"
      "    </bottom>"
      "  </inner>"
      "</mand>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}

TEST(Mandatory, missing_one_list)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<mand>"
      "  <mand-leaf>3</mand-leaf>"
      "  <inner>"
      "    <k>k1</k>"
      "    <inner-leaf>2</inner-leaf>"      
      "    <bottom>"
      "      <bottom-leaf-a>1</bottom-leaf-a>"
      "      <bottom-leaf-b>1</bottom-leaf-b>"      
      "    </bottom>"
      "  </inner>"
      "  <inner>"
      "    <k>k2</k>"
      "    <inner-leaf>2</inner-leaf>"      
      "    <bottom>"
      "      <bottom-leaf-a>1</bottom-leaf-a>"
      "    </bottom>"
      "  </inner>"
      "</mand>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}

TEST(Mandatory, missing_one)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<mand>"
      "  <mand-leaf>3</mand-leaf>"
      "  <inner>"
      "    <k>k1</k>"
      "    <inner-leaf>2</inner-leaf>"      
      "    <bottom>"
      "      <bottom-leaf-a>1</bottom-leaf-a>"
      "    </bottom>"
      "  </inner>"
      "</mand>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}

TEST(Mandatory, rpc_missing_one)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml = "<data><valid:vstart xmlns:valid=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\"><valid:component-name>logd-110</valid:component-name></valid:vstart></data>";
      
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_FAILURE);
}


TEST(Mandatory, positive_rpc_leafref)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <other-list>"
      "      <name>a</name>" 
      "      <str>original leafref</str>"
      "    </other-list>"
      "    <other-list>"
      "      <name>c</name>" 
      "      <str>another leafref</str>"
      "    </other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const delta_xml =
      "<data>"
      "<call xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <stuff-a>"
      "    <name>a</name>"
      "    <str>b</str>"
      "    <absolute>original leafref</absolute>"
      "  </stuff-a>"
      "  <stuff-a>"
      "    <name>c</name>"
      "    <str>b</str>"
      "    <absolute>another leafref</absolute>"
      "  </stuff-a>"
      "  <stuff-b>"
      "    <name>a</name>"
      "    <relative>b</relative>"
      "  </stuff-b>"
      "</call>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Mandatory, negative_rpc_leafref_relative)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const delta_xml =
      "<data>"
      "<call xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <stuff-b>"
      "    <name>c</name>"
      "    <relative>ghjk</relative>"
      "  </stuff-b>"
      "</call>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Mandatory, negative_rpc_leafref_absolute)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const delta_xml =
      "<data>"
      "<call xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "  <stuff-a>"
      "    <name>a</name>"
      "    <str>b</str>"
      "    <absolute>asdf</absolute>"
      "  </stuff-a>"
      "  <stuff-b>"
      "    <name>c</name>"
      "    <relative>ghjk</relative>"
      "  </stuff-b>"
      "</call>"
      "</data>";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  ValidationStatus result = validate_dom(delta_dom.get());
  ASSERT_EQ(result.status(), RW_STATUS_FAILURE);
}

TEST(Mandatory, positive_not_complete)
{
  TEST_DESCRIPTION("");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const xml =
      "<data>"
      "<mand>"
      "  <mand-leaf>3</mand-leaf>"
      "  <inner>"
      "    <k>k1</k>"
      "    <inner-leaf>2</inner-leaf>"      
      "  </inner>"
      "</mand>"
      "</data>";

  XMLDocument::uptr_t dom(mgr->create_document_from_string(xml.c_str(), error_out, false));
  ASSERT_TRUE(dom.get());

  ValidationStatus result = validate_dom(dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}

TEST(Filter, positive_filter)
{
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));

  std::string error_out;

  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "<other-list>"
      "    <name>a key</name>"
      "    <str>original leafref</str>"
      "</other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document_from_string(base_xml.c_str(), error_out, false));
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());

  std::string const filter_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "<other-list>"
      "    <name>a key that doesn't exist</name>"
      "</other-list>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t filter_dom(mgr->create_document_from_string(filter_xml.c_str(), error_out, false));
  ASSERT_TRUE(filter_dom.get());

  base_dom->subtree_filter(filter_dom.get());
  std::cout << base_dom->to_string_pretty() << std::endl;
}

TEST(vnfd, vnfd)
{
  return;
  TEST_DESCRIPTION("");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());

  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("validation-other"));
  ASSERT_TRUE(mgr->get_yang_model()->load_module("rw-vnfd"));


  std::string error_out;

  std::string const base_xml =
      "<data>"
      "  <other xmlns=\"http://riftio.com/ns/core/mgmt/uagent/test/validation\">"
      "    <str>original leafref</str>"
      "  </other>"
      "</data>";

  XMLDocument::uptr_t base_dom(mgr->create_document());
  ASSERT_TRUE(base_dom.get());

  XMLDocMerger merger(mgr.get(), base_dom.get());
  merger.on_error_ = [](rw_yang_netconf_op_status_t nc_status,
                        const char* err_msg, const char* err_path) {
    if(err_msg) {
    std::cout << "Merge Error err_msg: " << err_msg << std::endl;
    }
    if (err_path) {
    std::cout << "Merge Error err_path: " << err_path << std::endl;
    }
    std::cout << "Merge Error: " << std::endl;
    return RW_STATUS_SUCCESS;
  };

  std::string const delta_xml ="vnfd.xml";

  XMLDocument::uptr_t delta_dom(mgr->create_document_from_file(delta_xml.c_str(), error_out, false));
  ASSERT_TRUE(delta_dom.get());

  XMLDocument::uptr_t new_dom = merger.copy_and_merge(delta_dom.get());
  ASSERT_TRUE(new_dom.get());

  ValidationStatus result = validate_dom(new_dom.get());
  ASSERT_EQ(result.status() ,  RW_STATUS_SUCCESS);
}
