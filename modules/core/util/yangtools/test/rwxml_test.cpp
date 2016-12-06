 
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
 * @file rwxml_test.cpp
 * @author Vinod Kamalaraj
 * @date 2014/04/11
 * @brief XML wrapper library test program
 *
 * Unit tests for rw_xml_ximpl.cpp
 */

#include <cstdlib>
#include <iostream>
#include <limits.h>
#include <list>
#include <utility>
#include <tuple>
#include <forward_list>

#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"

#include "rwut.h"

#include "company.pb-c.h"
#include "company-augment.pb-c.h"
#include "rw_xml.h"
#include "yangtest_common.hpp"


using namespace rw_yang;
static void compare_xml_trees(XMLNode *old_node, XMLNode *new_node);

int g_argc;
char **g_argv;


class XMLNodeXpathLookup : public ::testing::Test {
 public:
  typedef std::forward_list<std::pair<std::string, size_t>> TestPlan;
  typedef std::forward_list<std::tuple<std::string, ProtobufCMessageDescriptor const *, size_t>> TestPlanNarrow;

 protected:

  XMLManager::uptr_t mgr;
  rw_yang_pb_schema_t const * schema;
  XMLDocument::uptr_t dom;
  XMLNode * root;

  virtual void SetUp() {
    mgr = XMLManager::uptr_t(xml_manager_create_xerces());

    YangModel* model = mgr->get_yang_model();
    ASSERT_TRUE(model);
    YangModule* ydom_top = model->load_module("company");
    ASSERT_TRUE(ydom_top);

    model->register_ypbc_schema(
        (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(Company));
    ASSERT_EQ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(Company),
              model->get_ypbc_schema());

    ydom_top = model->load_module("company-augment");
    ASSERT_TRUE(ydom_top);

    model->register_ypbc_schema(
        (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(CompanyAugment));
    ASSERT_EQ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(CompanyAugment),
              model->get_ypbc_schema());

    schema = mgr->get_yang_model()->get_ypbc_schema();
    ASSERT_TRUE(schema);   

    std::string const delta_xml =
        "<data>"
        "  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
        "    <employee>"
        "      <id>0</id>"
        "      <name>foo</name>"      
        "    </employee>"
        "    <employee>"
        "      <id>1</id>"
        "      <name>bar</name>"      
        "    </employee>"
        "    <wacky-interests>"
        "      <name>foo</name>"      
        "      <id>0</id>"
        "    </wacky-interests>"
        "    <wacky-interests>"
        "      <name>bar</name>"      
        "      <id>1</id>"
        "    </wacky-interests>"
        "  </company>"
        "  <mangle-base xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
        "    <mangle>"
        "      <name>asdf</name>"
        "      <id>13</id>"
        "      <bucket xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company-augment\">"
        "        <contents>water</contents>"
        "      </bucket>"
        "      <bucket-list xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company-augment\">"
        "        <place>victoria falls</place>"
        "      </bucket-list>"
        "      <bucket-list xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company-augment\">"
        "        <place>angkor wat</place>"
        "      </bucket-list>"
        "    </mangle>"
        "  </mangle-base>"
        "</data>";
  
    std::string error_out;
    dom = XMLDocument::uptr_t(mgr->create_document_from_string(delta_xml.c_str(), error_out, false));
    ASSERT_TRUE(dom.get()) << error_out;
    root = dom->get_root_node();
    ASSERT_TRUE(root);
  
  }

  virtual void TearDown() {

  }

  void test_keyspecs(TestPlan & testplan){
    for (auto test : testplan) {
      std::string const & xpath = test.first;
      size_t const matching_element_count = test.second;

      rw_keyspec_path_t const * ks = rw_keyspec_path_from_xpath(schema,
                                                                xpath.c_str(),
                                                                RW_XPATH_KEYSPEC,
                                                                nullptr);
      EXPECT_TRUE(ks) << "keyspec construction from xpath failed: " << xpath;

      if (ks == nullptr) {
        continue;
      }

      std::list<XMLNode*> found;
      rw_yang_netconf_op_status_t ncrs = root->find (ks, found);
      EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, ncrs) << "keyspec lookup failed: " << xpath;

      EXPECT_EQ(found.size(), matching_element_count) << "keyspec lookup didn't find enough elements: " << xpath;
    }
  }

  void test_narrow_keyspecs(TestPlanNarrow & testplan, bool const truncate = false){
    for (auto test : testplan) {
      std::string const & xpath = std::get<0>(test);
      ProtobufCMessageDescriptor const * narrow_ks_type = std::get<1>(test);
      size_t const matching_element_count = std::get<2>(test);

      rw_keyspec_path_t const * ks = rw_keyspec_path_from_xpath(schema,
                                                                xpath.c_str(),
                                                                RW_XPATH_KEYSPEC,
                                                                nullptr);
      EXPECT_TRUE(ks) << "keyspec construction from xpath failed: " << xpath;
      if (ks == nullptr) {
        continue;
      }

      rw_keyspec_path_t * narrow_ks;
      if (truncate) {
        narrow_ks= rw_keyspec_path_create_dup_of_type_trunc(ks, NULL, narrow_ks_type);
        EXPECT_TRUE(narrow_ks) << "keyspec truncate narrowing failed.";
      } else {
        narrow_ks= rw_keyspec_path_create_dup_of_type(ks, NULL, narrow_ks_type);
        EXPECT_TRUE(narrow_ks) << "keyspec narrowing failed.";
      }


      if (narrow_ks == nullptr) {
        continue;
      }

      std::list<XMLNode*> found;
      rw_yang_netconf_op_status_t ncrs = root->find (narrow_ks, found);
      EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, ncrs) << "keyspec lookup failed: " << xpath;

      EXPECT_EQ(found.size(), matching_element_count) << "keyspec lookup didn't find enough for "
                                                      << (truncate ? "truncated" : "non-truncated")
                                                      << " ks elements: "
                                                      << xpath;
    }
  }
};

TEST_F (XMLNodeXpathLookup, KeyspecFind)
{
  TestPlan myplan = {{"C,/company", 1},
                     {"C,/company/employee", 2},
                     {"C,/company/employee[id='0']/name", 1}};

  test_keyspecs(myplan);
}

TEST_F (XMLNodeXpathLookup, KeyspecFindMultiKeyList)
{
  TEST_DESCRIPTION("");

  TestPlan myplan = {{"C,/mangle-base/mangle[name='asdf'][id='13']",1},
                     {"C,/company/wacky-interests",2},
                     {"C,/company/wacky-interests/name",2}};

  test_keyspecs(myplan);
}

TEST_F (XMLNodeXpathLookup, KeyspecFindMultiKeyListNarrowed)
{
  TEST_DESCRIPTION("");
  ProtobufCMessageDescriptor const * narrow_ks_type = RWPB_G_PATHSPEC_PBCMD(Company_data_MangleBase_Mangle);
  auto test = std::make_tuple("C,/mangle-base/mangle[name='asdf'][id='13']/bucket",
                              narrow_ks_type,
                              1);
  TestPlanNarrow myplan = {test};

  test_narrow_keyspecs(myplan);
  test_narrow_keyspecs(myplan, true /*truncate*/);
}

TEST_F (XMLNodeXpathLookup, KeyspecFindMultiKeyListNarrowedList)
{
  ProtobufCMessageDescriptor const * narrow_ks_type = RWPB_G_PATHSPEC_PBCMD(Company_data_MangleBase_Mangle);
  auto test = std::make_tuple("C,/mangle-base/mangle[name='asdf'][id='13']/bucket-list[place='victoria falls']",
                              narrow_ks_type,
                              1);
  TestPlanNarrow myplan = {test};

  test_narrow_keyspecs(myplan);
  test_narrow_keyspecs(myplan, true /*truncate*/);
}

TEST (RwXML, CreateDestroy)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  TEST_DESCRIPTION ("Create and Destroy a DOM");

  XMLDocument::uptr_t doc(mgr->create_document());
  ASSERT_TRUE(doc.get());
  
  XMLNode *root = doc->get_root_node();
  ASSERT_TRUE (root);
  
  XMLNode *child_1 = root->add_child("level1_1");
  ASSERT_TRUE (child_1);

  XMLNode *child_2 = root->add_child("level1_2");
  ASSERT_TRUE (child_2);

  XMLNodeList::uptr_t list_1(doc->get_elements ("level1_1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  XMLNode *dup = list_1->at(0);
  ASSERT_EQ (dup, child_1);

  list_1 = root->get_children();
  ASSERT_EQ (2, list_1->length());

  dup = root->find("level1_1");
  ASSERT_EQ (dup, child_1);

  dup = root->find("level1_2");
  ASSERT_EQ (dup, child_2);

  dup = root->find("level2_2");
  ASSERT_EQ (dup, nullptr);

  XMLNode *ns_child_1 = root->add_child("level1_1", nullptr, "rwtest/NS-1", "NS1");
  ASSERT_TRUE (ns_child_1);

  XMLNode *ns_child_2 = root->add_child("level1_2", nullptr, "rwtest/NS-1", "NS1");
  ASSERT_TRUE (ns_child_2);
  
  list_1 = std::move(doc->get_elements ("level1_1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  dup = list_1->at(0);
  ASSERT_EQ (dup, child_1);

  list_1 = std::move(doc->get_elements ("level1_1", "rwtest/NS-1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  dup = list_1->at(0);
  ASSERT_EQ (dup, ns_child_1);

  std::string tmp_str;
  std::string exp_str = "<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"><level1_1/><level1_2/><NS1:level1_1 xmlns:NS1=\"rwtest/NS-1\"/><NS1:level1_2 xmlns:NS1=\"rwtest/NS-1\"/></data>";

  tmp_str = doc->to_string();
  EXPECT_EQ (tmp_str, exp_str);

  CFMutableStringRef cf = CFStringCreateMutable (NULL, 0);
  rw_xml_document_to_cfstring ((rw_xml_document_t *) doc.get(),
                               cf);

  char from_cf[500];

  CFStringGetCString (cf, from_cf, sizeof (from_cf), kCFStringEncodingUTF8);

  EXPECT_STREQ (tmp_str.c_str(), from_cf);
  std::string error_out;
  XMLDocument::uptr_t dup_doc = mgr->create_document_from_string (tmp_str.c_str(), error_out, false);

  list_1 = std::move(dup_doc->get_elements ("level1_1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  dup = list_1->at(0);

  XMLNode *child_1_1 = child_1->add_child("level1_1_1", "chi_1_1-val0");
  ASSERT_TRUE (child_1_1);

  XMLNode *child_1_2 = child_1->add_child("level1_1_1", "chi_1_1-val2");
  ASSERT_TRUE (child_1_2);
  
  tmp_str = root->to_string();
  std::cout << tmp_str << std::endl;

  rw_xml_node_to_cfstring ((rw_xml_node_t*) root, cf);
  CFStringGetCString (cf, from_cf, sizeof (from_cf), kCFStringEncodingUTF8);

  EXPECT_STREQ (tmp_str.c_str(), from_cf);
  
  bool remove_status = child_1->remove_child(child_1_2);
  ASSERT_TRUE(remove_status);

  tmp_str = root->to_string();
  std::cout << "Original after removal = "<< std::endl<<tmp_str << std::endl;

  root = dup_doc->get_root_node();

  std::cout << "Dup doc " << std::endl;
  tmp_str = root->to_stdout();
  std::cout << std::endl;
  CFRelease(cf);
}

TEST (RwXML, Attributes)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  TEST_DESCRIPTION ("Test Attribute support in RW XML");

  XMLDocument::uptr_t doc(mgr->create_document());
  ASSERT_TRUE(doc.get());
  
  XMLNode *root = doc->get_root_node();
  ASSERT_TRUE (root);
  
  XMLNode *child_1 = root->add_child("level1_1");
  ASSERT_TRUE (child_1);

  XMLNode *child_2 = root->add_child("level1_2");
  ASSERT_TRUE (child_2);

  XMLNodeList::uptr_t list_1(doc->get_elements ("level1_1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  XMLNode *dup = list_1->at(0);
  ASSERT_EQ (dup, child_1);

  list_1 = root->get_children();
  ASSERT_EQ (2, list_1->length());

  XMLNode *ns_child_1 = root->add_child("level1_1", nullptr, "rwtest/NS-1", "NS1");
  ASSERT_TRUE (ns_child_1);

  XMLNode *ns_child_2 = root->add_child("level1_2", nullptr, "rwtest/NS-1", "NS1");
  ASSERT_TRUE (ns_child_2);
  
  list_1 = std::move(doc->get_elements ("level1_1"));
  ASSERT_TRUE (list_1.get());
  ASSERT_EQ (1, list_1->length());

  XMLAttributeList::uptr_t list_a1(child_1->get_attributes());
  ASSERT_TRUE (list_a1.get());
  ASSERT_EQ (0, list_a1->length());

  child_1->set_attribute("attr1", "value1");
  const char *ns = "http://www.riftio.com/namespace";
  child_1->set_attribute("attr2", "value2", ns, "ns");
  ASSERT_TRUE (child_1->has_attributes());
  ASSERT_TRUE (child_1->has_attribute("attr1"));
  //XMLAttribute::uptr_t attr = std::move(child_1->get_attribute("attr1"));
  //ASSERT_NE (nullptr, attr.get());
  list_a1 = std::move(child_1->get_attributes());
  ASSERT_TRUE (list_a1.get());
  ASSERT_EQ (2, list_a1->length());
#if 0
  for (uint32_t i = 0; i <  list_a1->length(); i++) {
    XMLAttribute *attr = list_a1->at(i);
    std::cout <<  attr->get_node_name().c_str() << " : " << attr->get_value().c_str() << std::endl;
  }
#endif

  EXPECT_STREQ(list_a1->at(0)->get_local_name().c_str(), "attr1");
  EXPECT_STREQ(list_a1->at(0)->get_value().c_str(), "value1");
  EXPECT_STREQ(list_a1->at(1)->get_local_name().c_str(), "attr2");
  EXPECT_STREQ(list_a1->at(1)->get_prefix().c_str(), "ns");
  EXPECT_STREQ(list_a1->at(1)->get_text_value().c_str(), "value2");
  EXPECT_STREQ(list_a1->at(1)->get_name_space().c_str(), "http://www.riftio.com/namespace");
  EXPECT_STREQ(list_a1->at(1)->get_value().c_str(), "value2");


  child_2->set_attribute("attr3", "value3");
  child_2->set_attribute("attr4", "value4");
  child_2->set_attribute("attr5", "value5");

  XMLAttributeList::uptr_t list_a2(child_2->get_attributes());
  ASSERT_TRUE (list_a2.get());
  ASSERT_EQ (3, list_a2->length());

  std::string tmp_str;
  std::string exp_str = "<data xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\"><level1_1 attr1=\"value1\" xmlns:ns=\"http://www.riftio.com/namespace\" ns:attr2=\"value2\"/><level1_2 attr3=\"value3\" attr4=\"value4\" attr5=\"value5\"/><NS1:level1_1 xmlns:NS1=\"rwtest/NS-1\"/><NS1:level1_2 xmlns:NS1=\"rwtest/NS-1\"/></data>";
  tmp_str = doc->to_string();
  ASSERT_EQ (tmp_str, exp_str);
}

TEST (RwXML, MappingFunction)
{
  EXPECT_DEATH (rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_NULL),"");
  EXPECT_EQ (RW_STATUS_SUCCESS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_OK));
  EXPECT_EQ (RW_STATUS_EXISTS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_IN_USE));
  EXPECT_EQ (RW_STATUS_EXISTS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS));

  EXPECT_EQ (RW_STATUS_NOTFOUND, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE));
  EXPECT_EQ (RW_STATUS_NOTFOUND, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_MISSING_ATTRIBUTE));
  EXPECT_EQ (RW_STATUS_NOTFOUND, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_MISSING_ELEMENT));
  EXPECT_EQ (RW_STATUS_NOTFOUND, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_DATA_MISSING));

  EXPECT_EQ (RW_STATUS_OUT_OF_BOUNDS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_TOO_BIG));
  EXPECT_EQ (RW_STATUS_OUT_OF_BOUNDS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_BAD_ATTRIBUTE));
  EXPECT_EQ (RW_STATUS_OUT_OF_BOUNDS, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT));

  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ATTRIBUTE));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ELEMENT));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_UNKNOWN_NAMESPACE));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_ACCESS_DENIED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_LOCK_DENIED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_RESOURCE_DENIED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_ROLLBACK_FAILED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_OPERATION_NOT_SUPPORTED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_MALFORMED_MESSAGE));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED));
  EXPECT_EQ (RW_STATUS_FAILURE, rw_yang_netconf_op_status_to_rw_status(RW_YANG_NETCONF_OP_STATUS_FAILED));
}
TEST (RwXML,XMLWalkerTest)
{
  TEST_DESCRIPTION("Verify the functioning of XMLWalker/XMLCopier");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("company");
  EXPECT_TRUE(ydom_top);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/company.xml");

  XMLDocument::uptr_t doc(mgr->create_document_from_file(file_name.c_str(), false));
  ASSERT_TRUE(doc.get());

  XMLNode* root = doc->get_root_node()->get_first_child();

  EXPECT_TRUE(root);

  EXPECT_STREQ(root->get_local_name().c_str(), "company");

  std::string error_out;
  XMLDocument::uptr_t to_doc(mgr->create_document_from_string("<data/>", error_out, false/*validate*/));
  ASSERT_TRUE(to_doc.get());
  
  ASSERT_TRUE(to_doc->get_root_node());

  XMLCopier copier(to_doc->get_root_node());

  XMLWalkerInOrder walker(root);

  walker.walk(&copier);

  to_doc->to_stdout();
  std::cout << std::endl;

  
  // Now walk the copied tree along side the original tree and make sure they match
  //
  //
  root = to_doc->get_root_node();
  EXPECT_STREQ(root->get_local_name().c_str(), "data");

  XMLNode *old_node = doc->get_root_node();
  old_node = old_node->get_first_child();
  
  XMLNode *new_node = root->get_first_child();

  EXPECT_STREQ(old_node->get_local_name().c_str(), new_node->get_local_name().c_str());

  ASSERT_NO_FATAL_FAILURE(compare_xml_trees(old_node, new_node));
}

TEST (RwXML, XMLSubtreeFilter)
{
  TEST_DESCRIPTION ("XML subtree filtering");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);
  YangModule* ydom_top = model->load_module("company");
  EXPECT_TRUE(ydom_top);

  std::string error;

  std::string file_name = get_rift_root() +
    std::string("/modules/core/util/yangtools/test/company.xml");

  XMLDocument::uptr_t doc(mgr->create_document_from_file(file_name.c_str(), false));

  ASSERT_TRUE(doc.get());

  std::string filter = 
    "<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<contact-info></contact-info>"
    "</company>"
    "</data>";

  XMLDocument::uptr_t fdoc(mgr->create_document_from_string(filter.c_str(), error, false));
  rw_status_t res;

  res = doc->subtree_filter(fdoc.get());
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
  std::cout << doc->to_string() << std::endl;

  std::string expected_op =
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n    <contact-info>\n      <name>RIFT.io Inc</name>\n      <address>24, New England State Park, Burlington, MA</address>\n      <phone-number>123-456-7890</phone-number>\n    </contact-info>\n    \n    \n    \n    \n    \n    \n  </company>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //
  error.clear();
  fdoc.release();

  filter = 
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<contact-info>"
    "<name/>"
    "</contact-info>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));

  res = doc->subtree_filter(fdoc.get());
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
  std::cout << doc->to_string() << std::endl;

  auto root = doc->get_root_node();
  ASSERT_TRUE(root);
  auto node = root->find("company",
                    "http://riftio.com/ns/core/util/yangtools/tests/company");
  ASSERT_TRUE(node);

  auto contact_info_node = node->find("contact-info", nullptr);
  EXPECT_TRUE(contact_info_node);

  auto contact_info_name = contact_info_node->find("name", nullptr);
  EXPECT_TRUE(contact_info_name);

  expected_op = 
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n    <contact-info>\n      <name>RIFT.io Inc</name>\n      \n      \n    </contact-info>\n    \n    \n    \n    \n    \n    \n  </company>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //------
  error.clear();
  fdoc.release();


  filter = 
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<employee xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "</employee>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  std::cout << doc->get_root_node()->to_string() << std::endl;
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  root = doc->get_root_node();
  ASSERT_TRUE(root);
  node = root->find("company",
                    "http://riftio.com/ns/core/util/yangtools/tests/company");
  ASSERT_TRUE(node);

  node = node->find("employee", nullptr);
  ASSERT_TRUE(node);

  size_t employee_count = 0;
  while (node) {
    employee_count++;
    node = node->get_next_sibling(node->get_local_name(),
                                  node->get_name_space());
  }

  EXPECT_EQ(employee_count, 6);

  //----
  error.clear();
  fdoc.release();

  
  filter = 
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<employee xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<id/>"
    "<name/>"
    "</employee>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  std::cout << doc->to_string() << std::endl;
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  root = doc->get_root_node();
  node = root->find("company",
                    "http://riftio.com/ns/core/util/yangtools/tests/company");
  ASSERT_TRUE(node);
  
  node = node->find("employee", nullptr);
  ASSERT_TRUE(node);
  
  employee_count = 0;
  while (node) {
    employee_count++;

    auto id = node->find("id", nullptr);
    EXPECT_TRUE(id);
    auto name = node->find("name", nullptr);
    EXPECT_TRUE(name);

    node = node->get_next_sibling(node->get_local_name(),
                                  node->get_name_space());
  }
  
  EXPECT_EQ(employee_count, 6);

  //-----
  error.clear();
  fdoc.release();


  filter =
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<employee xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<id>100</id>"
    "</employee>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  std::cout << doc->to_string() << std::endl;
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  expected_op =
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n    \n    <employee>\n      <id>100</id>\n      <name>name100</name>\n      <title>Engineer</title>\n      <start-date>01/01/2014</start-date>\n    </employee>\n    \n    \n    \n    \n    \n  </company>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //------
  error.clear();
  fdoc.release();

  
  filter =
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<employee xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<start-date/>"
    "</employee>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  std::cout << doc->to_string() << std::endl;
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  expected_op =
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n    \n    <employee>\n      <id>100</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n    <employee>\n      <id>101</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n    <employee>\n      <id>102</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n    <employee>\n      <id>103</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n    <employee>\n      <id>104</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n    <employee>\n      <id>105</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n  </company>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //------
 
  error.clear();
  fdoc.release();

  filter = 
    "<data>"
    "<company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<employee xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">"
    "<id>105</id>"
    "<start-date/>"
    "</employee>"
    "</company>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  std::cout << doc->to_string() << std::endl;
  EXPECT_EQ(res, RW_STATUS_SUCCESS);

  expected_op =
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n  <company xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/company\">\n    \n    \n    \n    \n    \n    \n    <employee>\n      <id>105</id>\n      \n      \n      <start-date>01/01/2014</start-date>\n    </employee>\n  </company>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //------
  
  error.clear();
  fdoc.release();

  model = mgr->get_yang_model();
  ASSERT_TRUE(model);
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);
  
  file_name = get_rift_root() +
              std::string("/modules/core/util/yangtools/test/conversion-1.xml");
  
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  ASSERT_TRUE(doc.get());
  
  filter =
    "<data>"
    "<container-1 xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">"
    "<container_1-1 xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\"/>"
    "</container-1>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  res = doc->subtree_filter(fdoc.get());
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
  std::cout << doc->to_string() << std::endl;

  expected_op = 
"<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">\n  <container-1 xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">\n    <conv:container_1-1 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">\n      <conv:leaf-1_1.1>leaf_1_1_1 - ONE</conv:leaf-1_1.1>\n      <conv:list-1.1_2>\n        <conv:int_1.1.2_1>11201121</conv:int_1.1.2_1>\n      </conv:list-1.1_2>\n      <conv:list-1.1_2>\n        <conv:int_1.1.2_1>11211121</conv:int_1.1.2_1>\n      </conv:list-1.1_2>\n    </conv:container_1-1>\n    \n    \n    \n    \n    \n    \n    \n    \n  </container-1>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  //------
  
  error.clear();
  fdoc.release();

  filter = 
    "<data>"
    "<container-1 xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">"
    "<conv:leaf.list.1.2 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">"
    "</conv:leaf.list.1.2>"
    "</container-1>"
    "</data>";

  fdoc = std::move(mgr->create_document_from_string(filter.c_str(), error, false));
  doc = std::move(mgr->create_document_from_file(file_name.c_str(), false));
  res = doc->subtree_filter(fdoc.get());
  EXPECT_EQ(res, RW_STATUS_SUCCESS);
  std::cout << doc->to_string() << std::endl;

  root = doc->get_root_node();
  ASSERT_TRUE (root);

  node = root->find("container-1",
                    "http://riftio.com/ns/core/util/yangtools/tests/conversion");
  ASSERT_TRUE (node);

  node = node->find("leaf.list.1.2",
                    "http://riftio.com/ns/core/util/yangtools/tests/conversion");
  ASSERT_TRUE (node);

  size_t ll_count = 1; // found one just above
  while ((node = node->get_next_sibling("leaf.list.1.2",
            "http://riftio.com/ns/core/util/yangtools/tests/conversion"))) {
    ll_count++;
  }

  EXPECT_EQ(ll_count, 4);

  expected_op =
    "<data xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">\n  <container-1 xmlns=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">\n    \n    <conv:leaf.list.1.2 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">Leaf List First</conv:leaf.list.1.2>\n    <conv:leaf.list.1.2 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">Second Leaf in the List</conv:leaf.list.1.2>\n    <conv:leaf.list.1.2 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">?Three Leaf in a List?</conv:leaf.list.1.2>\n    <conv:leaf.list.1.2 xmlns:conv=\"http://riftio.com/ns/core/util/yangtools/tests/conversion\">The leaf broke the branch</conv:leaf.list.1.2>\n    \n    \n    \n    \n  </container-1>\n</data>";

  EXPECT_STREQ(doc->to_string().c_str(), expected_op.c_str());

  mgr.release();
}

static void compare_xml_trees(XMLNode *old_node, XMLNode *new_node)
{
  ASSERT_TRUE(old_node);
  ASSERT_TRUE(new_node);
 
  EXPECT_STREQ(old_node->get_local_name().c_str(), new_node->get_local_name().c_str());

  XMLNode *old_ch = old_node->get_first_child();
  XMLNode *new_ch = new_node->get_first_child();

  while (old_ch) {
    EXPECT_NE(old_ch, nullptr);
    EXPECT_NE(new_ch, nullptr);

    EXPECT_STREQ(old_ch->get_local_name().c_str(), new_ch->get_local_name().c_str());
    EXPECT_STREQ(old_ch->get_name_space().c_str(), new_ch->get_name_space().c_str());
    EXPECT_STREQ(old_ch->get_text_value().c_str(), new_ch->get_text_value().c_str());

    compare_xml_trees(old_ch, new_ch);

    old_ch = old_ch->get_next_sibling();
    new_ch = new_ch->get_next_sibling();
  } while (old_ch != nullptr);

}

void mySetup(int argc, char** argv)
{
  //::testing::InitGoogleTest(&argc,argv);

  /* Used by yangdom_test */
  g_argc = argc;
  g_argv = argv;

}

RWUT_INIT(mySetup);
