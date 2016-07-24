
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */




/**
 * @file yangdom_test.cpp
 * @author Vinod Kamalaraj
 * @date 2014/04/22
 * @brief test the yangdom
 *
 * Unit tests for rw_xml_ximpl.cpp
 */

#include <limits.h>
#include <cstdlib>
#include <iostream>

#include "gmock/gmock.h"
#include "gtest/rw_gtest.h"
#include "yangmodel.h"
#include "rw_namespace.h"
#include "rw_xml.h"
#include "yangncx.hpp"
#include "rw_json.h"
#include "yangtest_common.hpp"

/* From xml2pb_test initialization */
extern char **g_argv;
extern int g_argc;

#include "bumpy-conversion.pb-c.h"
#include "flat-conversion.pb-c.h"
#include "other-data_rwvcs.pb-c.h"
#include "rift-cli-test.pb-c.h"
#include "test-ydom-top.pb-c.h"

#include "rw-fpath-d.pb-c.h"
#include "rw-fpath-d.ypbc.h"

#include "rw-composite-d.pb-c.h"
#include "yangtest_common.hpp"
#include "company.pb-c.h"
#include "testy2p-top1.pb-c.h"
#include "test-a-composite.pb-c.h"

using namespace rw_yang;

static const char* ydt_top_ns = "http://riftio.com/ns/yangtools/test-ydom-top";
static const char* ydt_top_prefix = "t";
static const char* ydt_aug_ns = "http://riftio.com/ns/yangtools/test-ydom-aug";
static const char* ydt_aug_prefix = "a";

static const char* ydt_xml_header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
static const char* ydt_test_header = "<data><top xmlns=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n";
static const char* ydt_test_trailer = "</top></data>";

static const char* ydt_cli_test_header = "<data><test-choice xmlns=\"http://riftio.com/ns/yangtools/rift-cli-test\">\n";
static const char* ydt_cli_test_trailer = "</test-choice> </data>";




static const char* yd_enum_header =
    "<enum-test xmlns=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n";
static const char* yd_enum_trailer =
    "</enum-test>";

static ProtobufCInstance* pinstance = nullptr;

TEST (RwYangDom, CreateDestroy)
{
  TEST_DESCRIPTION("Creates and destroy a YangDOM");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  XMLDocument::uptr_t dom(mgr->create_document(model->get_root_node()));
  ASSERT_TRUE(dom.get());

  XMLNode *m_root = dom->get_root_node();
  ASSERT_TRUE (m_root);
  YangNode* yn = m_root->get_yang_node();
  EXPECT_TRUE(yn);

}


TEST (RwYangDom, Merge)
{
  TEST_DESCRIPTION("Creates a Yang-based DOM and merge several XML documents into it");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  XMLDocument::uptr_t dom(mgr->create_document(model->get_root_node()));
  ASSERT_TRUE(dom.get());


  XMLManager::uptr_t xml_mgr(xml_manager_create_xerces());
  ASSERT_TRUE(xml_mgr.get());

  XMLDocument::uptr_t merge(xml_mgr->create_document());
  ASSERT_TRUE(merge.get());


  XMLNode *m_root = merge->get_root_node();
  ASSERT_TRUE (m_root);

  XMLNode *top = m_root->add_child( "top", nullptr, ydt_top_ns, ydt_top_prefix );
  ASSERT_TRUE (top);

  XMLNode *top_a = top->add_child( "a", nullptr, ydt_top_ns, ydt_top_prefix );
  ASSERT_TRUE (top_a);

  XMLNode *cont_in_a = top_a->add_child( "cont-in-a", nullptr, ydt_top_ns, ydt_top_prefix );
  ASSERT_TRUE (cont_in_a);

  XMLNode *str_1_cont_in_a = cont_in_a->add_child( "str1", "fo", ydt_top_ns, ydt_top_prefix );
  ASSERT_TRUE (str_1_cont_in_a);

  XMLNode *num_1_cont_in_a = cont_in_a->add_child( "num1", "100", ydt_top_ns, ydt_top_prefix );
  ASSERT_TRUE (num_1_cont_in_a);


  std::string tmp_str = dom->to_string();
  std::cout << "Merge: Original before merge = "<< std::endl << tmp_str << std::endl;

  dom->merge(merge.get());

  tmp_str = merge->to_string();
  std::cout << "Merge: Original after merge = "<< std::endl<<tmp_str << std::endl;

  tmp_str = dom->to_string();
  std::cout << "Merge: DOM after merge = "<< std::endl<<tmp_str << std::endl;


  top = m_root->add_child( "top", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top);

  top_a = top->add_child( "a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top_a);

  cont_in_a = top_a->add_child( "cont-in-a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (cont_in_a);

  str_1_cont_in_a = cont_in_a->add_child( "str1", "bar", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (str_1_cont_in_a);

  dom->merge(merge.get());

  tmp_str = merge->to_string();
  std::cout << "Merge: Original after merge = "<< std::endl<<tmp_str << std::endl;

  tmp_str = dom->to_string();
  std::cout << "Merge: DOM after merge = "<< std::endl<<tmp_str << std::endl;



  merge = std::move(xml_mgr->create_document());
  ASSERT_TRUE (merge.get());

  m_root = merge->get_root_node();
  ASSERT_TRUE (m_root);

  top = m_root->add_child( "top", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top);

  top_a = top->add_child( "a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top_a);

  cont_in_a = top_a->add_child( "cont-in-a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (cont_in_a);

  XMLNode * lst1_1_cont_in_a = cont_in_a->add_child( "lst", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE(lst1_1_cont_in_a);

  XMLNode * lst1_1_str2 = lst1_1_cont_in_a->add_child( "str2", "key1", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst1_1_str2);

  XMLNode * lst1_1_num2 = lst1_1_cont_in_a->add_child( "num2", "10", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst1_1_num2);

  XMLNode * lst1_2_cont_in_a = cont_in_a->add_child( "lst", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst1_2_cont_in_a);

  XMLNode *lst1_2_str2 = lst1_2_cont_in_a->add_child( "str2", "key1", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst1_2_str2);

  XMLNode * lst1_2_num2 = lst1_2_cont_in_a->add_child( "num2", "2", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst1_2_num2);

  XMLNode *lst2_cont_in_a = cont_in_a->add_child( "lst", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst2_cont_in_a);

  XMLNode *lst2_1_str2 = lst2_cont_in_a->add_child( "str2", "key2", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst2_1_str2);

  XMLNode *lst2_1_num2 = lst2_cont_in_a->add_child( "num2", "5", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (lst2_1_num2);

  dom->merge(merge.get());

  tmp_str = merge->to_string();
  std::cout << "Merge: Original after merge = "<< std::endl<<tmp_str << std::endl;

  tmp_str = dom->to_string();
  std::cout << "Merge: DOM after merge = "<< std::endl<<tmp_str << std::endl;

  merge = std::move(xml_mgr->create_document());
  ASSERT_TRUE (merge.get());

  m_root = merge->get_root_node();
  ASSERT_TRUE (m_root);

  top = m_root->add_child( "top", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top);

  top_a = top->add_child( "a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top_a);

  cont_in_a = top_a->add_child( "cont-in-a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (cont_in_a);

  XMLNode * ll1_cont_in_a = cont_in_a->add_child( "ll", "RWCAT_E_A", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE(ll1_cont_in_a);

  ll1_cont_in_a = cont_in_a->add_child( "ll", "RWCAT_E_D", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE(ll1_cont_in_a);

  ll1_cont_in_a = cont_in_a->add_child( "ll", "RWCAT_E_A", ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE(ll1_cont_in_a);

  dom->merge(merge.get());

  tmp_str = merge->to_string();
  std::cout << "Merge: Original after merge = "<< std::endl<<tmp_str << std::endl;

  tmp_str = dom->to_string();
  std::cout << "Merge: DOM after merge = "<< std::endl<<tmp_str << std::endl;



  merge = std::move(xml_mgr->create_document());
  ASSERT_TRUE (merge.get());

  m_root = merge->get_root_node();
  ASSERT_TRUE (m_root);

  top = m_root->add_child( "top", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top);

  top_a = top->add_child( "a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (top_a);

  cont_in_a = top_a->add_child( "cont-in-a", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (cont_in_a);


  // try a bad node name
  XMLNode *junk_a = top->add_child( "argh", nullptr, ydt_top_ns, ydt_top_prefix  );
  ASSERT_TRUE (junk_a);

  dom->merge(merge.get());

  tmp_str = merge->to_string();
  std::cout << "Merge: Original after merge = "<< std::endl<<tmp_str << std::endl;

  std::cout << "Merge: DOM after merge = " << std::endl;
  dom->to_stdout();
}

TEST (RwYangDom, InsertInYangOrder)
{
  TEST_DESCRIPTION("Test the Child Insertion in Yang Order method");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  XMLDocument::uptr_t dom(mgr->create_document(model->get_root_node()));
  ASSERT_TRUE(dom.get());

  XMLNode *root = dom->get_root_node();
  ASSERT_TRUE (root);

  XMLNode* order_test = root->add_child("order_test");
  ASSERT_TRUE(order_test);

  XMLNode* leaf = order_test->add_child("value1", "one");
  EXPECT_TRUE(leaf);

  leaf = order_test->add_child("key1", "kone");
  EXPECT_TRUE(leaf);

  leaf = order_test->add_child("key2", "ktwo");
  EXPECT_TRUE(leaf);

  leaf = order_test->add_child("key3", "kthree");
  EXPECT_TRUE(leaf);

  leaf = order_test->add_child("value2", "two");
  EXPECT_TRUE(leaf);

  leaf = order_test->add_child("value3", "three");
  EXPECT_TRUE(leaf);

  std::string expected_str = 
"\n<data>\n\n"
"  <order_test>\n"
"    <key1>kone</key1>\n"
"    <key2>ktwo</key2>\n"
"    <key3>kthree</key3>\n"
"    <value1>one</value1>\n"
"    <value2>two</value2>\n"
"    <value3>three</value3>\n"
"  </order_test>\n\n"
"</data>";

  std::string dom_str = dom->to_string_pretty();
  EXPECT_EQ(expected_str, dom_str);

}

TEST (RwYangDom, InsertInYangOrderRpc)
{
  TEST_DESCRIPTION("Test the Child Insertion in Yang Order method");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  XMLDocument::uptr_t dom(mgr->create_document(model->get_root_node()));
  ASSERT_TRUE(dom.get());

  XMLNode *root = dom->get_root_node();
  ASSERT_TRUE (root);

  XMLNode* start_test = root->add_child("start-test");
  ASSERT_TRUE(start_test);

  XMLNode* payload = start_test->add_child("payload");
  ASSERT_TRUE(payload);

  XMLNode* leaf = payload->add_child("length", "42");
  EXPECT_TRUE(leaf);

  leaf = payload->add_child("pattern", "xx==yy");
  EXPECT_TRUE(leaf);

  leaf = start_test->add_child("publishers", "2");
  EXPECT_TRUE(leaf);

  leaf = start_test->add_child("subscribers", "1");
  EXPECT_TRUE(leaf);

  leaf = start_test->add_child("tasklet-type", "toy");
  EXPECT_TRUE(leaf);

  leaf = start_test->add_child("test-type", "latency");
  EXPECT_TRUE(leaf);

  leaf = start_test->add_child("xact-operation", "query");
  EXPECT_TRUE(leaf);

  XMLNode* lnode = start_test->add_child("lnode");
  EXPECT_TRUE(lnode);

  leaf = lnode->add_child("value1", "one");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("key1", "kone");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("key2", "ktwo");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("value3", "three");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("key3", "kthree");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("value2", "two");
  EXPECT_TRUE(leaf);

  leaf = lnode->add_child("value4", "four");
  EXPECT_TRUE(leaf);


  std::string expected_xml =
"\n<data>\n\n"
"  <start-test>\n"
"    <test-type>latency</test-type>\n"
"    <publishers>2</publishers>\n"
"    <subscribers>1</subscribers>\n"
"    <xact-operation>query</xact-operation>\n"
"    <tasklet-type>toy</tasklet-type>\n"
"    <payload>\n"
"      <length>42</length>\n"
"      <pattern>xx==yy</pattern>\n"
"    </payload>\n"
"    <lnode>\n"
"      <key1>kone</key1>\n"
"      <key3>kthree</key3>\n"
"      <key2>ktwo</key2>\n"
"      <value1>one</value1>\n"
"      <value2>two</value2>\n"
"      <value3>three</value3>\n"
"      <value4>four</value4>\n"
"    </lnode>\n"
"  </start-test>\n\n"
"</data>";

  std::string dom_str = dom->to_string_pretty();
  EXPECT_EQ(expected_xml, dom_str);
}

TEST (RwYangDom, EmptyLeaf)
{
  TEST_DESCRIPTION("Verify the functioning of empty leaf and boolean");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(ydt_test_header);
  xmlblob.append(
      "<empty_leaf>"
      "<empty1/>"
      "<bool1>true</bool1>"
      "<bool2>false</bool2>"
      "</empty_leaf>" );
  xmlblob += ydt_test_trailer;

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  std::cout << "EmptyLeaf: DOM 1 = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;

  YangNode* yn = nullptr;
  XMLNode* root = nullptr;
  XMLNode* top = nullptr;
  XMLNode* elc = nullptr;
  XMLNode* empty1 = nullptr;
  XMLNode* empty2 = nullptr;
  XMLNode* bool1 = nullptr;
  XMLNode* bool2 = nullptr;
  XMLNode* bool3 = nullptr;
  std::string pb2xml_str;

  // Walk the dom to the empty leaf test container, so that we can test it.
  root = dom->get_root_node();
  ASSERT_TRUE(root);
  yn = root->get_yang_node();
  EXPECT_FALSE(yn);

  top = root->get_first_child();
  ASSERT_TRUE(top);
  yn = top->get_yang_node();
  EXPECT_TRUE(yn);

  elc = top->find("empty_leaf");
  ASSERT_TRUE(elc);

  bool1 = elc->find("bool1"); EXPECT_TRUE(bool1);
  bool2 = elc->find("bool2"); EXPECT_TRUE(bool2);
  bool3 = elc->find("bool3"); EXPECT_FALSE(bool3);
  empty1 = elc->find("empty1"); EXPECT_TRUE(empty1);
  empty2 = elc->find("empty2"); EXPECT_FALSE(empty2);
  /*
   * Make sure that an empty leaf that is set shows up correctly in
   * protobuf.
   */
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtTop,top_pb_new);
  rw_yang_netconf_op_status_t ncrs = top->to_pbcm(&top_pb_new.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ASSERT_TRUE(top_pb_new.empty_leaf);
  EXPECT_TRUE(top_pb_new.empty_leaf->has_bool1);
  EXPECT_TRUE(top_pb_new.empty_leaf->bool1);
  EXPECT_TRUE(top_pb_new.empty_leaf->has_bool2);
  EXPECT_FALSE(top_pb_new.empty_leaf->bool2);
  EXPECT_FALSE(top_pb_new.empty_leaf->has_bool3);
  EXPECT_TRUE(top_pb_new.empty_leaf->has_empty1);
  EXPECT_TRUE(top_pb_new.empty_leaf->empty1);
  EXPECT_FALSE(top_pb_new.empty_leaf->has_empty2);


  protobuf_c_message_free_unpacked_usebody(nullptr, &top_pb_new.base);
}



TEST (RwYangDom, PresenceCont)
{
  TEST_DESCRIPTION("Verify the functioning of presence containers");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(ydt_test_header);
  xmlblob.append(
      "<empty_cont>"
      "<ep02/>"
      "<ep11></ep11>"
      "<ep12><lf12>data12</lf12></ep12>"
      "<ep15></ep15>"
      "<ep16><lf16>data16</lf16></ep16>"
      "<ep21></ep21>"
      "<ep22><lf22>data22</lf22></ep22>"
      "<ep25></ep25>"
      "<ep26><lf26>data26</lf26></ep26>"
      "<ep37><lf37>data37</lf37></ep37>"
      "<ep44></ep44>"
      "<ep55><lf55>data55</lf55></ep55>"
      "</empty_cont>"
      "<empty_cont_flat>"
      "<ep02/>"
      "<ep11></ep11>"
      "<ep12><lf12>data12</lf12></ep12>"
      "<ep15></ep15>"
      "<ep16><lf16>data16</lf16></ep16>"
      "<ep21></ep21>"
      "<ep22><lf22>data22</lf22></ep22>"
      "<ep25></ep25>"
      "<ep26><lf26>data26</lf26></ep26>"
      "<ep37><lf37>data37</lf37></ep37>"
      "<ep44></ep44>"
      "<ep55><lf55>data55</lf55></ep55>"
      "</empty_cont_flat>" );
  xmlblob += ydt_test_trailer;

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  std::cout << "PresenceCont: DOM 1 = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;

  // Walk the dom to the empty container, so that we can test it.
  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);
  YangNode* yn = root->get_yang_node();
  EXPECT_FALSE(yn);

  XMLNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  yn = top->get_yang_node();
  EXPECT_TRUE(yn);

  XMLNode* ecc = top->find("empty_cont");
  ASSERT_TRUE(ecc);

#undef YDT_CONT_CHECK
#define YDT_CONT_CHECK( n_, opt_, truth_ )      \
  XMLNode* ep##n_ = ecc->find("ep"#n_);         \
      truth_(ep##n_);

  YDT_CONT_CHECK(01, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(02, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(10, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(11, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(12, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(14, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(15, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(16, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(20, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(21, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(22, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(24, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(25, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(26, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(36, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(37, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(43, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(44, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(55, MANDATORY, EXPECT_TRUE );

#undef YDT_LEAF_CHECK
#define YDT_LEAF_CHECK( n_, c_opt_, l_opt_, truth_, string_ )           \
  do {                                                                  \
    XMLNode* lf##n_ = nullptr;                                          \
        if (ep##n_) {                                                   \
          lf##n_ = ep##n_->find("lf"#n_);                               \
              if (lf##n_) {                                             \
                EXPECT_STREQ(lf##n_->get_text_value().c_str(), string_#n_); \
              }                                                         \
        }                                                               \
        truth_(lf##n_);                                                 \
  } while(0)

  YDT_LEAF_CHECK(10, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(11, OPTIONAL , OPTIONAL , EXPECT_FALSE, "def" );
  YDT_LEAF_CHECK(12, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(14, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(15, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(16, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(20, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(21, OPTIONAL , OPTIONAL , EXPECT_FALSE, "def" );
  YDT_LEAF_CHECK(22, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(24, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(25, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(26, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(36, OPTIONAL , MANDATORY, EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(37, OPTIONAL , MANDATORY, EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(55, MANDATORY, MANDATORY, EXPECT_TRUE , "data");



  XMLNode* ecf = top->find("empty_cont_flat");
  ASSERT_TRUE(ecf);

#undef YDT_CONT_CHECK
#define YDT_CONT_CHECK( n_, opt_, truth_ )      \
  ep##n_ = ecf->find("ep"#n_);                  \
      truth_(ep##n_);

  YDT_CONT_CHECK(01, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(02, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(10, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(11, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(12, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(14, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(15, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(16, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(20, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(21, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(22, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(24, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(25, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(26, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(36, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(37, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(43, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(44, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(55, MANDATORY, EXPECT_TRUE );


#undef YDT_LEAF_CHECK
#define YDT_LEAF_CHECK( n_, c_opt_, l_opt_, truth_, string_ )           \
  do {                                                                  \
  XMLNode* lf##n_ = nullptr;                                            \
      if (ep##n_) {                                                     \
        lf##n_ = ep##n_->find("lf"#n_);                                 \
            if (lf##n_) {                                               \
              EXPECT_STREQ(lf##n_->get_text_value().c_str(), string_#n_); \
            }                                                           \
      }                                                                 \
      truth_(lf##n_);                                                   \
  } while(0)

  YDT_LEAF_CHECK(10, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(11, OPTIONAL , OPTIONAL , EXPECT_FALSE, "def" );
  YDT_LEAF_CHECK(12, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(14, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(15, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(16, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(20, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(21, OPTIONAL , OPTIONAL , EXPECT_FALSE, "def" );
  YDT_LEAF_CHECK(22, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(24, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(25, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(26, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(36, OPTIONAL , MANDATORY, EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(37, OPTIONAL , MANDATORY, EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(55, MANDATORY, MANDATORY, EXPECT_TRUE , "data");


  /*
   * Convert to protobuf and make sure that everything shows up
   * correctly.
   */
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtTop,top_pb);
  rw_yang_netconf_op_status_t ncrs = top->to_pbcm(&top_pb.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  auto pbecc = top_pb.empty_cont;
  ASSERT_TRUE(pbecc);

#undef YDT_CONT_CHECK
#define YDT_CONT_CHECK( n_, opt_, truth_ )      \
  truth_(pbecc->ep##n_);

  YDT_CONT_CHECK(01, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(02, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(10, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(11, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(12, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(14, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(15, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(16, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(20, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(21, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(22, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(24, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(25, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(26, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(36, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(37, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(43, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(44, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(55, MANDATORY, EXPECT_TRUE );


#undef YDT_LEAF_CHECK
#define YDT_LEAF_CHECK( n_, c_opt_, l_opt_, truth_, string_ )   \
  if (pbecc->ep##n_) {                                          \
    truth_(pbecc->ep##n_->lf##n_);                              \
    if (pbecc->ep##n_->lf##n_) {                                \
      EXPECT_STREQ(pbecc->ep##n_->lf##n_, string_#n_);          \
    }                                                           \
  }

  YDT_LEAF_CHECK(10, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(11, OPTIONAL , OPTIONAL , EXPECT_TRUE , "def" );
  YDT_LEAF_CHECK(12, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(14, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(15, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(16, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(20, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(21, OPTIONAL , OPTIONAL , EXPECT_TRUE , "def" );
  YDT_LEAF_CHECK(22, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(24, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(25, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(26, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(36, OPTIONAL , MANDATORY, EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(37, OPTIONAL , MANDATORY, EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(55, MANDATORY, MANDATORY, EXPECT_TRUE , "data");



  auto pbecf = top_pb.empty_cont_flat;
  ASSERT_TRUE(pbecf);

#undef YDT_MANDATORY
#define YDT_MANDATORY( n_, truth_ )

#undef YDT_OPTIONAL
#define YDT_OPTIONAL( n_, truth_ )              \
  truth_(pbecf->has_ep##n_);

#undef YDT_CONT_CHECK
#define YDT_CONT_CHECK( n_, opt_, truth_ )      \
  YDT_##opt_( n_, truth_ )

  YDT_CONT_CHECK(01, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(02, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(10, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(11, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(12, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(14, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(15, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(16, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(20, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(21, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(22, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(24, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(25, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(26, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(36, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(37, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(43, OPTIONAL , EXPECT_FALSE);
  YDT_CONT_CHECK(44, OPTIONAL , EXPECT_TRUE );
  YDT_CONT_CHECK(55, MANDATORY, EXPECT_TRUE );


#undef YDT_LEAF_MANDATORY
#define YDT_LEAF_MANDATORY( n_, truth_, string_ )       \
  truth_(pbecf->ep##n_.lf##n_[0]);                      \
  EXPECT_STREQ(pbecf->ep##n_.lf##n_, (string_ #n_));

#undef YDT_LEAF_OPTIONAL
#define YDT_LEAF_OPTIONAL( n_, truth_, string_ )        \
  truth_(pbecf->ep##n_.has_lf##n_);                     \
  if (pbecf->ep##n_.has_lf##n_) {                       \
    truth_(pbecf->ep##n_.lf##n_[0]);                    \
    EXPECT_STREQ(pbecf->ep##n_.lf##n_, (string_ #n_));  \
  }

#undef YDT_CONT_MANDATORY
#define YDT_CONT_MANDATORY( n_, l_opt_, truth_, string_ )       \
  YDT_LEAF_##l_opt_( n_, truth_, string_ );

#undef YDT_CONT_OPTIONAL
#define YDT_CONT_OPTIONAL( n_, l_opt_, truth_, string_ )        \
  if (pbecf->has_ep##n_) {                                      \
    YDT_LEAF_##l_opt_( n_, truth_, string_ );                   \
  }

#undef YDT_LEAF_CHECK
#define YDT_LEAF_CHECK( n_, c_opt_, l_opt_, truth_, string_ )   \
  YDT_CONT_##c_opt_( n_, l_opt_, truth_, string_ )

  YDT_LEAF_CHECK(10, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(11, OPTIONAL , OPTIONAL , EXPECT_TRUE , "def" );
  YDT_LEAF_CHECK(12, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(14, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(15, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(16, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(20, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(21, OPTIONAL , OPTIONAL , EXPECT_TRUE , "def" );
  YDT_LEAF_CHECK(22, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(24, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(25, OPTIONAL , OPTIONAL , EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(26, OPTIONAL , OPTIONAL , EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(36, OPTIONAL , MANDATORY, EXPECT_FALSE, ""    );
  YDT_LEAF_CHECK(37, OPTIONAL , MANDATORY, EXPECT_TRUE , "data");
  YDT_LEAF_CHECK(55, MANDATORY, MANDATORY, EXPECT_TRUE , "data");


  protobuf_c_message_free_unpacked_usebody(nullptr, &top_pb.base);
}


TEST (RwYangDom, ApiTest)
{
  TEST_DESCRIPTION("Test the XML*Yang C and C++ APIs");
  XMLManager::uptr_t px_mgr(xml_manager_create_xerces());
  XMLDocument::uptr_t px_doc(px_mgr->create_document());
  ASSERT_TRUE(px_mgr.get());
  ASSERT_TRUE(px_doc.get());


  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  YangModule* ydom_aug = model->load_module("test-ydom-aug");
  EXPECT_TRUE(ydom_top);
  EXPECT_TRUE(ydom_aug);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(ydt_test_header);
  xmlblob.append(
      "<apis_test>"
      " <lint8>100</lint8> \n "
      " <lint32>123987</lint32> \n "   // The extra whitespace is intended!
      " <lint16>2376</lint16> \n "
      "</apis_test>"
      "<garbage>2376</garbage>" );
  xmlblob += ydt_test_trailer;

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  std::cout << "ApiTest: Start DOM = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;

  YangNode* yn = nullptr;
  XMLNode* root = nullptr;
  XMLNode* top = nullptr;
  XMLNode* atc = nullptr;

  // Walk the dom to the one-of-each container, so that we can play with it.
  root = dom->get_root_node();
  ASSERT_TRUE(root);
  yn = root->get_yang_node();
  EXPECT_FALSE(yn);
  EXPECT_FALSE(root->get_parent());

  top = root->get_first_element();
  ASSERT_TRUE(top);
  yn = top->get_yang_node();
  EXPECT_TRUE(yn);
  EXPECT_STREQ(yn->get_name(),"top");
  EXPECT_EQ(root, rw_xml_node_get_parent(top));

  atc = top->get_first_child();
  ASSERT_TRUE(atc);
  YangNode* atc_yn = atc->get_yang_node();
  ASSERT_TRUE(atc_yn);
  EXPECT_STREQ(atc_yn->get_name(),"apis_test");
  EXPECT_EQ(top, atc->get_parent());


  /*
   * Verify the document and manager get interfaces.
   */
  EXPECT_EQ(dom.get(), &atc->get_document());
  EXPECT_EQ(dom.get(), &atc->get_document());
  EXPECT_EQ(&dom->get_manager(), &atc->get_manager());
  EXPECT_EQ(mgr.get(), &atc->get_manager());
  EXPECT_EQ(rw_xml_node_get_document(top), &atc->get_document());
  EXPECT_EQ(rw_xml_node_get_manager(root), &atc->get_manager());


  /*
   * Verify the relationship pointers.
   */
  XMLNode* lint8 = atc->find("lint8");
  XMLNode* lint16 = atc->find("lint16", ydt_top_ns);
  XMLNode* lint32 = atc->find_value("lint32", "123987", ydt_top_ns);
  ASSERT_TRUE(lint8);
  ASSERT_TRUE(lint16);
  ASSERT_TRUE(lint32);

  EXPECT_EQ(atc->get_last_child(), lint16);
  EXPECT_EQ(atc->get_first_child(), lint8);
  EXPECT_EQ(atc->get_first_child(), atc->get_first_element());
  EXPECT_EQ(lint8->get_next_sibling(), lint32);
  EXPECT_EQ(lint8->get_previous_sibling(), nullptr);
  EXPECT_EQ(lint16->get_next_sibling(), nullptr);
  EXPECT_EQ(lint16->get_previous_sibling(), lint32);
  EXPECT_EQ(lint16->get_previous_sibling()->get_next_sibling(), lint16);
  EXPECT_EQ(lint16->get_previous_sibling()->get_previous_sibling()->get_next_sibling()->get_next_sibling(), lint16);
  EXPECT_EQ(lint32->get_last_child(), nullptr);
  EXPECT_EQ(lint32->get_first_child(), nullptr);
  EXPECT_EQ(lint8->get_parent(), lint16->get_parent());
  EXPECT_EQ(atc, lint8->get_parent());
  EXPECT_EQ(lint32->get_next_sibling(), lint16);
  EXPECT_EQ(lint32->get_previous_sibling(), lint8);

  XMLNodeIter i1 = atc->child_begin();
  EXPECT_EQ(&*i1, lint8);
  EXPECT_NE(atc->child_end(), ++i1);
  EXPECT_EQ(i1++, XMLNodeIter(lint32));
  XMLNodeIter i2(i1);
  EXPECT_EQ(i1, i2);
  EXPECT_EQ(i1->get_yang_node(), i2->get_yang_node());
  EXPECT_EQ(i1, i2++);
  EXPECT_NE(i1, i2);
  EXPECT_EQ(atc->child_end(), i2);
  XMLNodeIter i3(lint16);
  EXPECT_EQ(i1, i3);

  EXPECT_EQ(rw_xml_node_get_last_child(atc), lint16);
  EXPECT_EQ(rw_xml_node_get_first_child(atc), lint8);
  EXPECT_EQ(rw_xml_node_get_first_child(atc), rw_xml_node_get_first_element(atc));
  EXPECT_EQ(rw_xml_node_get_next_sibling(lint8), lint32);
  EXPECT_EQ(rw_xml_node_get_previous_sibling(lint8), nullptr);
  EXPECT_EQ(rw_xml_node_get_next_sibling(lint16), nullptr);
  EXPECT_EQ(rw_xml_node_get_previous_sibling(lint16), lint32);
  EXPECT_EQ(rw_xml_node_get_last_child(lint32), nullptr);
  EXPECT_EQ(rw_xml_node_get_first_child(lint32), nullptr);
  EXPECT_EQ(rw_xml_node_get_parent(lint8), rw_xml_node_get_parent(lint16));
  EXPECT_EQ(atc, rw_xml_node_get_parent(lint8));
  EXPECT_EQ(rw_xml_node_get_next_sibling(lint32), lint16);
  EXPECT_EQ(rw_xml_node_get_previous_sibling(lint32), lint8);


  /*
   * Verify children node list and node list APIs.  Beware!  The list
   * contains whitespace text elements!
   */
  XMLNodeList::uptr_t nlist1 = atc->get_children();
  ASSERT_TRUE(nlist1->length() > 3);
  EXPECT_EQ(nlist1->at(1), lint8);
  EXPECT_EQ(nlist1->at(3), lint32);
  EXPECT_EQ(nlist1->at(5), lint16);

  EXPECT_EQ(nlist1->find("lint8", ydt_top_ns), lint8);
  EXPECT_EQ(nlist1->find("lint8", ydt_aug_ns), nullptr);

  EXPECT_EQ(dom.get(), &nlist1->get_document());
  EXPECT_EQ(mgr.get(), &nlist1->get_manager());
  EXPECT_EQ(dom.get(), &nlist1->get_document());
  EXPECT_EQ(mgr.get(), &nlist1->get_manager());


  rw_xml_node_list_t* nlist2 = rw_xml_node_get_children(atc);
  XMLNodeList::uptr_t nlist2_del(static_cast<XMLNodeList*>(nlist2));
  EXPECT_EQ(rw_xml_node_list_length(nlist2), nlist1->length());
  EXPECT_EQ(rw_xml_node_list_at(nlist2, 1), lint8);
  EXPECT_EQ(rw_xml_node_list_at(nlist2, 3), lint32);
  EXPECT_EQ(rw_xml_node_list_at(nlist2, 5), lint16);
  EXPECT_EQ(rw_xml_node_list_at(nlist1.get(), 3), lint32);
  EXPECT_EQ(rw_xml_node_list_at(nlist1.get(), 5), lint16);

  EXPECT_EQ(rw_xml_node_list_find(nlist2, "lint8", ydt_top_ns), lint8);
  EXPECT_EQ(rw_xml_node_list_find(nlist2, "lint8", ydt_aug_ns), nullptr);

  EXPECT_EQ(dom.get(), rw_xml_node_list_get_document(nlist2));
  EXPECT_EQ(mgr.get(), rw_xml_node_list_get_manager(nlist2));


  /*
   * Verify that node lists update in place.
   */
  yn = atc_yn->search_child("lstring", ydt_top_ns);
  ASSERT_TRUE(yn);
  XMLNode* lstring = atc->add_child(yn, "new str value");
  ASSERT_TRUE(lstring);
  EXPECT_EQ(lint16->get_next_sibling(), lstring);

  EXPECT_EQ(nlist1->at(7), lstring);
  EXPECT_EQ(rw_xml_node_list_at(nlist2, 7), lstring);

  nlist1.reset();
  rw_xml_node_list_release(nlist2_del.release());


  /*
    Add and remove nodes...
  */
  XMLNode* nn = atc->add_child("luint8", nullptr, "bogus_ns");
  ASSERT_TRUE(nn);
  yn = nn->get_yang_node();
  ASSERT_FALSE(yn);  // ATTN: Do we want XML-yang to default to using namespaces, and to figure them out automagically?
  ASSERT_EQ(lstring->get_next_sibling(), nn);
  bool rem_status = atc->remove_child(nn);
  EXPECT_TRUE(rem_status);
  EXPECT_EQ(lstring->get_next_sibling(), nullptr);

  nn = atc->add_child("luint8", nullptr, ydt_top_ns);
  ASSERT_TRUE(nn);
  yn = nn->get_yang_node();
  ASSERT_TRUE(yn);
  EXPECT_STREQ(yn->get_name(), nn->get_local_name().c_str());
  ASSERT_EQ(lstring->get_next_sibling(), nn);
  rem_status = atc->remove_child(nn);
  EXPECT_TRUE(rem_status);
  EXPECT_EQ(lstring->get_next_sibling(), nullptr);

  // ATTN: This should fail - library should validate and reject!
  // nn = atc->add_child("luint8", ydt_top_ns, "1024");
  // EXPECT_FALSE(nn);

  XMLNode* luint8 = atc->add_child("luint8", "255", ydt_top_ns);
  ASSERT_TRUE(luint8);
  yn = luint8->get_yang_node();
  ASSERT_TRUE(yn);
  EXPECT_STREQ(yn->get_name(), luint8->get_local_name().c_str());
  ASSERT_EQ(lstring->get_next_sibling(), luint8);

  XMLNode* lint64 = atc->add_child("lint64", "-102410241024", ydt_top_ns, "pt");
  ASSERT_TRUE(lint64);
  yn = lint64->get_yang_node();
  ASSERT_TRUE(yn);
  EXPECT_STREQ(yn->get_name(), lint64->get_local_name().c_str());
  EXPECT_EQ(luint8->get_next_sibling(), lint64);

  XMLNode* aint8 = atc->add_child("aint8", "127", ydt_aug_ns);
  ASSERT_TRUE(aint8);
  EXPECT_EQ(lint64->get_next_sibling(), aint8);

#if 0
  // ATTN use the new import
  //
  XMLNode::uptr_t lboolean_uptr(px_doc->create_node("lboolean", ydt_top_ns, "true"));
  ASSERT_TRUE(lboolean_uptr.get());
  XMLNode* lboolean = atc->import_child(lboolean_uptr.get(), true/*deep*/);
  ASSERT_TRUE(lboolean);
  lboolean_uptr.reset();
  EXPECT_EQ(aint8->get_next_sibling(), lboolean);
#endif

  XMLNode* lboolean = atc->add_child("lboolean", "true", ydt_top_ns);
  ASSERT_TRUE(lboolean);
  EXPECT_EQ(aint8->get_next_sibling(), lboolean);

  rw_xml_node_t* luint64_rw = rw_xml_node_append_child(atc, "luint64", "1024102410241024", ydt_top_ns, "pf");
  XMLNode* luint64 = static_cast<XMLNode*>(luint64_rw);
  ASSERT_TRUE(luint64);
  EXPECT_EQ(lboolean->get_next_sibling(), luint64);

  yn = atc_yn->search_child("lempty", ydt_top_ns);
  ASSERT_TRUE(yn);
  rw_xml_node_t* lempty_rw = rw_xml_node_append_child_yang(atc, yn, nullptr);
  XMLNode* lempty = static_cast<XMLNode*>(lempty_rw);
  ASSERT_TRUE(lempty);
  EXPECT_EQ(luint64->get_next_sibling(), lempty);

  yn = atc_yn->search_child("ldecimal64", ydt_top_ns);
  ASSERT_TRUE(yn);

  rw_xml_node_t* ldecimal64_rw = rw_xml_node_append_child_yang(atc, yn, "12.345678");
  XMLNode* ldecimal64 = static_cast<XMLNode*>(ldecimal64_rw);
  ASSERT_TRUE(ldecimal64_rw);
  EXPECT_EQ(lempty->get_next_sibling(), ldecimal64);

  rw_xml_node_t* lbits_rwx = rw_xml_node_append_child(atc, "lbits", "a b", ydt_top_ns, nullptr);
  XMLNode* lbits = static_cast<XMLNode*>(lbits_rwx);
  ASSERT_TRUE(lbits);
  EXPECT_EQ(ldecimal64->get_next_sibling(), lbits);

  // ATTN: Add to the list and play with find key...

  // Insert before API tests
  XMLNode *newStr = lbits->insert_before("newstring", "inserted", ydt_top_ns, nullptr);
  ASSERT_TRUE(newStr);
  EXPECT_EQ(newStr->get_next_sibling(), lbits);

  // Insert before the root node - failure case
  XMLNode *rootNode = dom->get_root_node()->insert_before("fails");
  EXPECT_FALSE(rootNode);

  // Insert a new node before the  newString
  YangNode *newuint64_yn = atc_yn->search_child("newuint64", ydt_top_ns);
  ASSERT_TRUE(newuint64_yn);
  XMLNode* newuint64 = newStr->insert_before(newuint64_yn, "1234");
  ASSERT_TRUE(newuint64);
  EXPECT_EQ(newuint64->get_next_sibling(), newStr);

  YangNode *newuint32_yn = atc_yn->search_child("newuint32", ydt_top_ns);
  ASSERT_TRUE(newuint32_yn);
  XMLNode* newuint32 = newuint64->insert_before(newuint32_yn, "3456");
  ASSERT_TRUE(newuint32);
  EXPECT_EQ(newuint32->get_next_sibling(), newuint64);

  // Insert before the root node - failure case
  rootNode = dom->get_root_node()->insert_before(newuint32_yn, "1234");
  EXPECT_FALSE(rootNode);

  // Remove the added nodes and repeat the test with C APIs
  //

  rem_status = atc->remove_child(newStr);
  EXPECT_TRUE(rem_status);

  rem_status = atc->remove_child(newuint64);
  EXPECT_TRUE(rem_status);

  rem_status = atc->remove_child(newuint32);
  EXPECT_TRUE(rem_status);

  std::cout << "ApiTest: Before C APIs = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;

  // Add the nodes back using C APIs
  //
  rw_xml_node_t *new_str = rw_xml_node_insert_before(lbits, "newstring", "c-inserted", ydt_top_ns, nullptr);
  ASSERT_TRUE(new_str);
  EXPECT_EQ(rw_xml_node_get_next_sibling(new_str), lbits);

  rw_xml_node_t *new_uint64 = rw_xml_node_insert_before_yang(new_str, newuint64_yn, "12340");
  ASSERT_TRUE(new_uint64);
  EXPECT_EQ(rw_xml_node_get_next_sibling(new_uint64), new_str);

  rw_xml_node_t *new_uint32 = rw_xml_node_insert_before_yang(new_uint64, newuint32_yn, "34210");
  ASSERT_TRUE(new_uint32);
  EXPECT_EQ(rw_xml_node_get_next_sibling(new_uint32), new_uint64);

  rw_xml_node_t* nnode = rw_xml_node_append_child(atc, "ntest", "", ydt_top_ns, nullptr);

  // Add a new node after ntest
  XMLNode *ncnode1 = static_cast<XMLNode*>(nnode)->add_child("str1", "val_str1", ydt_top_ns, nullptr);
  ASSERT_TRUE(ncnode1);
  XMLNode *ncnode2 = ncnode1->insert_after("str2", "val_str1", ydt_top_ns, nullptr);
  ASSERT_TRUE(ncnode2);
  EXPECT_EQ(rw_xml_node_get_next_sibling(ncnode1), ncnode2);
  XMLNode *ncnode3 = ncnode2->insert_after("str3", "val_str1", ydt_top_ns, nullptr);
  ASSERT_TRUE(ncnode3);
  EXPECT_EQ(rw_xml_node_get_next_sibling(ncnode2), ncnode3);

  std::cout << "ApiTest: End DOM = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;
  EXPECT_NE (nullptr, rw_xml_node_insert_after (ncnode1, "str1", "val_str1", ydt_top_ns, 0));

  UNUSED(ydt_aug_prefix);
}


/**
 * Test out some basic rw_fpath like config etc as it would appear in the fpath app
 */

TEST (RwConversions, FPathAPI)
{
  TEST_DESCRIPTION("Test the XML*Yang and DOM C API");
  std::string xmlblob = ydt_xml_header;

  xmlblob.append(ydt_test_header);
  xmlblob.append(
      "<empty_leaf>"
      "<empty1/>"
      "<bool1>true</bool1>"
      "<bool2>false</bool2>"
      "</empty_leaf>" );
  xmlblob += ydt_test_trailer;


  rw_xml_manager_t* mgr = rw_xml_manager_create_xerces();
  rw_yang_model_t *yang_model = rw_xml_manager_get_yang_model(mgr);
  rw_yang_model_load_module (yang_model, "test-ydom-top");
  rw_yang_model_load_module (yang_model, "test-ydom-aug");
  rw_xml_document_t *doc =
      rw_xml_manager_create_document_from_string (mgr, xmlblob.c_str(), 0);

  rw_xml_node_t *root = rw_xml_document_get_root_node(doc);
  rw_xml_node_t *top = rw_xml_node_get_first_child (root);

  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtTop,top_pb);

  rw_yang_netconf_op_status_t ncrs = rw_xml_node_to_pbcm (top,  &top_pb.base);

  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

}


TEST (RwYangDom, EnumTest)
{
  TEST_DESCRIPTION("Creates a Yang-based DOM and then convert it to protobuf-c");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("test-ydom-top");
  EXPECT_TRUE(test_ncx);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(yd_enum_header);
  xmlblob.append(
      "<global>first</global>"
      "<local>first</local>"
      "<not-typed>first</not-typed>");
  xmlblob += yd_enum_trailer;

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  std::cout << "DOM = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl;

  XMLNode* enum_test = dom->get_root_node();
  ASSERT_TRUE(enum_test);
  std::cout << "enum_test: " << enum_test->get_prefix()
            << ":" << enum_test->get_local_name()
            << " " << enum_test->get_name_space() << std::endl;
  YangNode* yn = enum_test->get_yang_node();
  EXPECT_TRUE(yn);


  XMLNode* global = enum_test->get_first_element();
  ASSERT_TRUE(global);
  yn = global->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "global: " << global->get_prefix()
            << ":" << global->get_local_name()
            << " " << global->get_name_space() << std::endl;


  XMLNode* local = global->get_next_sibling();
  ASSERT_TRUE(local);
  yn = local->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "local: " << local->get_prefix()
            << ":" << local->get_local_name()
            << " " << local->get_name_space() << std::endl;



  XMLNode* untyped = local->get_next_sibling();
  ASSERT_TRUE(untyped);
  yn = untyped->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "untyped: " << untyped->get_prefix()
            << ":" << untyped->get_local_name()
            << " " << untyped->get_name_space() << std::endl;

  const char *value = "first";

  ASSERT_TRUE(!strcmp(global->get_text_value().c_str(), value));
  ASSERT_TRUE(!strcmp(local->get_text_value().c_str(), value));
  ASSERT_TRUE(!strcmp(untyped->get_text_value().c_str(), value));

  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtEnum,enum_pb);
  rw_yang_netconf_op_status_t ncrs = enum_test->to_pbcm(&enum_pb.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  EXPECT_TRUE (enum_pb.has_global);
  EXPECT_TRUE (enum_pb.has_local);
  EXPECT_TRUE (enum_pb.has_not_typed);

  EXPECT_EQ (1, enum_pb.global);
  EXPECT_EQ (10, enum_pb.local);
  EXPECT_EQ (100, enum_pb.not_typed);

  // conver the PB back to xml
  rw_yang_netconf_op_status_t status;
  ProtobufCMessage *pbcm = (ProtobufCMessage *) &enum_pb;
  XMLDocument::uptr_t back_to(mgr->create_document_from_pbcm (pbcm, status));
  ASSERT_TRUE(back_to.get());

  enum_test = back_to->get_root_node();
  ASSERT_TRUE(enum_test);
  std::cout << "enum_test: " << enum_test->get_prefix()
            << ":" << enum_test->get_local_name()
            << " " << enum_test->get_name_space() << std::endl;
  yn = enum_test->get_yang_node();
  EXPECT_TRUE(yn);


  global = enum_test->get_first_element();
  ASSERT_TRUE(global);
  yn = global->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "global: " << global->get_prefix()
            << ":" << global->get_local_name()
            << " " << global->get_name_space() << std::endl;


  local = global->get_next_sibling();
  ASSERT_TRUE(local);
  yn = local->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "local: " << local->get_prefix()
            << ":" << local->get_local_name()
            << " " << local->get_name_space() << std::endl;



  untyped = local->get_next_sibling();
  ASSERT_TRUE(untyped);
  yn = untyped->get_yang_node();
  EXPECT_TRUE(yn);
  std::cout << "untyped: " << untyped->get_prefix()
            << ":" << untyped->get_local_name()
            << " " << untyped->get_name_space() << std::endl;

  ASSERT_TRUE(!strcmp(global->get_text_value().c_str(), value));
  ASSERT_TRUE(!strcmp(local->get_text_value().c_str(), value));
  ASSERT_TRUE(!strcmp(untyped->get_text_value().c_str(), value));
  
}


TEST (FlatPbTest, TopLevel)
{
  TEST_DESCRIPTION("Creates a Yang-based DOM and then convert it to protobuf-c");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  // fill the message
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,first);

  first.has_container_1_1 = 1;
  first.container_1_1.has_leaf_1_1_1 = 1;
  sprintf (first.container_1_1.leaf_1_1_1, "%s",
           "leaf_1_1_1 - ONE");

  RWPB_T_MSG(FlatConversion_FirstLevel_Container11) *cont = &first.container_1_1;

  cont->n_list_1_1_2 = 2;
  cont->list_1_1_2[0].int_1_1_2_1 = 11201121;
  cont->list_1_1_2[1].int_1_1_2_1 = 11211121;

  first.n_leaf_list_1_2 = 4;
  sprintf (first.leaf_list_1_2[0], "%s", "Leaf List First");
  sprintf (first.leaf_list_1_2[1], "%s", "Second Leaf in the List");
  sprintf (first.leaf_list_1_2[2], "%s", "?Three Leaf in a List?");
  sprintf (first.leaf_list_1_2[3], "%s", "The leaf broke the branch");


  first.has_empty_1_3 = 1;
  first.empty_1_3 = true;

  first.n_enum_1_4 = 2;
  first.enum_1_4[0] = FLAT_CONVERSION_ENUM14_SECOND;
  first.enum_1_4[1] = FLAT_CONVERSION_ENUM14_SEVENTH;
  first.enum_1_4[2] = FLAT_CONVERSION_ENUM14_FIRST;

  first.has_enum_1_5 = 1;
  first.enum_1_5 = BASE_CONVERSION_CB_ENUM_SEVENTH;

  first.has_bool_1_6 = 1;
  first.bool_1_6 = 1;
  ProtobufCMessage *pbcm = (ProtobufCMessage *) &first;
  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t
      to_xml_1(mgr->create_document_from_pbcm (pbcm, status));

  std::cout << "FIRST DOM = "<<std::endl << to_xml_1->to_string() << std::endl;
  
  // use the dom to generate a PB
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,second);

  rw_yang_netconf_op_status_t ncrs =
      to_xml_1->get_root_node()->to_pbcm(&second.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  EXPECT_EQ (protobuf_c_message_get_packed_size(NULL, &first.base),
             protobuf_c_message_get_packed_size(NULL, &second.base));

  size_t size = protobuf_c_message_get_packed_size(NULL, &first.base);
  uint8_t *buffer_1 = (uint8_t *)calloc (sizeof (uint8_t), size);
  uint8_t *buffer_2 = (uint8_t *)calloc (sizeof (uint8_t), size);

  // ATTN: Is this test valid? this assumes that the position of the elements
  // do not change during conversions, which might not be true, and which might
  // not need to be true.

  protobuf_c_message_pack(NULL, &first.base, buffer_1);
  protobuf_c_message_pack(NULL, &second.base, buffer_2);

  EXPECT_TRUE (!memcmp (buffer_2, buffer_1,size));
}

TEST (FlatPbTest, SecondLevel)
{

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  RWPB_M_MSG_DECL_INIT(FlatConversion_List112,list1);
  list1.int_1_1_2_1 = 11201121;

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &list1;
  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t
      to_xml_1(mgr->create_document_from_pbcm (pbcm, status));

  std::cout << "SECOND DOM = "<<std::endl << to_xml_1->to_string() << std::endl;
}

TEST (FlatPbTest, Unrooted)
{

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  RWPB_M_MSG_DECL_INIT(FlatConversion_Unrooted,first);
  first.has_unroot_int = 1;
  first.unroot_int = 11201121;

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &first;
  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t
      to_xml_1(mgr->create_document_from_pbcm (pbcm, status));

  EXPECT_TRUE(to_xml_1.get());

  XMLDocument::uptr_t
      to_xml_2(mgr->create_document_from_pbcm (pbcm, status,false));

  EXPECT_TRUE(to_xml_2.get());

  RWPB_M_MSG_DECL_INIT(FlatConversion_Unrooted,second);

  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK,
             to_xml_2->get_root_node()->to_pbcm(&second.base));

  size_t size = protobuf_c_message_get_packed_size(NULL, &first.base);
  ASSERT_EQ (size, protobuf_c_message_get_packed_size(NULL, &second.base));

  uint8_t *buffer1 = (uint8_t *) malloc (size);
  uint8_t *buffer2 = (uint8_t *) malloc (size);

  protobuf_c_message_pack(NULL, &first.base, buffer1);
  protobuf_c_message_pack(NULL, &second.base, buffer2);

  EXPECT_TRUE (!memcmp (buffer1, buffer2, size));

  free (buffer1);
  free (buffer2);
}

TEST (FlatPbTest, LoadXml)
{

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/conversion-1.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                          (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());

  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,first);

  rw_yang_netconf_op_status_t ncrs =
      dom->get_root_node()->get_first_child()->to_pbcm(&first.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &first;

  XMLDocument::uptr_t
      to_xml_1(mgr->create_document_from_pbcm (pbcm, ncrs));
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  std::string string_dom1 = to_xml_1->to_string();

  // use the dom to generate a PB
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,second);

  ncrs =
      to_xml_1->get_root_node()->to_pbcm(&second.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  EXPECT_EQ (protobuf_c_message_get_packed_size(NULL, &first.base),
             protobuf_c_message_get_packed_size(NULL, &second.base));

  size_t size = protobuf_c_message_get_packed_size(NULL, &first.base);
  uint8_t *buffer_1 = (uint8_t *)calloc (sizeof (uint8_t), size);
  uint8_t *buffer_2 = (uint8_t *)calloc (sizeof (uint8_t), size);

  // ATTN: Is this test valid? this assumes that the position of the elements
  // do not change during conversions, which might not be true, and which might
  // not need to be true.

  protobuf_c_message_pack(NULL, &first.base, buffer_1);
  protobuf_c_message_pack(NULL, &second.base, buffer_2);

  EXPECT_TRUE (!memcmp (buffer_2, buffer_1,size));

  pbcm = (ProtobufCMessage *) &second;
  XMLDocument::uptr_t
      to_xml_2(mgr->create_document_from_pbcm (pbcm, ncrs));

  EXPECT_EQ (0, strcmp (to_xml_2->to_string().c_str(),
                        to_xml_1->to_string().c_str()));


}

TEST (BumpyPbTest, LoadXml)
{

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("bumpy-conversion");
  ASSERT_TRUE(test_ncx);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/conversion-2.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                          (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());

  RWPB_M_MSG_DECL_INIT(BumpyConversion_BumpyFirstLevel,first);

  rw_yang_netconf_op_status_t ncrs =
      dom->get_root_node()->to_pbcm(&first.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &first;

  XMLDocument::uptr_t
      to_xml_1(mgr->create_document_from_pbcm (pbcm, ncrs));
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  std::string string_dom1 = to_xml_1->to_string();

  // use the dom to generate a PB
  RWPB_M_MSG_DECL_INIT(BumpyConversion_BumpyFirstLevel,second);

  ncrs =
      to_xml_1->get_root_node()->to_pbcm(&second.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  EXPECT_EQ (protobuf_c_message_get_packed_size(NULL, &first.base),
             protobuf_c_message_get_packed_size(NULL, &second.base));

  size_t size = protobuf_c_message_get_packed_size(NULL, &first.base);
  uint8_t *buffer_1 = (uint8_t *)calloc (sizeof (uint8_t), size);
  uint8_t *buffer_2 = (uint8_t *)calloc (sizeof (uint8_t), size);

  // ATTN: Is this test valid? this assumes that the position of the elements
  // do not change during conversions, which might not be true, and which might
  // not need to be true.

  protobuf_c_message_pack(NULL, &first.base, buffer_1);
  protobuf_c_message_pack(NULL, &second.base, buffer_2);

  EXPECT_TRUE (!memcmp (buffer_2, buffer_1,size));

  pbcm = (ProtobufCMessage *) &second;
  XMLDocument::uptr_t
      to_xml_2(mgr->create_document_from_pbcm (pbcm, ncrs));

  EXPECT_EQ (0, strcmp (to_xml_2->to_string().c_str(),
                        to_xml_1->to_string().c_str()));
}


TEST (ConversionAPI, TopLevel)
{
  TEST_DESCRIPTION("Switch from PB to string and back, and see if it all works");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  // fill the message
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,first);

  first.has_container_1_1 = 1;
  first.container_1_1.has_leaf_1_1_1 = 1;
  sprintf (first.container_1_1.leaf_1_1_1, "%s",
           "leaf_1_1_1 - ONE");

  RWPB_T_MSG(FlatConversion_FirstLevel_Container11) *cont = &first.container_1_1;

  cont->n_list_1_1_2 = 2;
  cont->list_1_1_2[0].int_1_1_2_1 = 11201121;
  cont->list_1_1_2[1].int_1_1_2_1 = 11211121;

  first.n_leaf_list_1_2 = 4;
  sprintf (first.leaf_list_1_2[0], "%s", "Leaf List First");
  sprintf (first.leaf_list_1_2[1], "%s", "Second Leaf in the List");
  sprintf (first.leaf_list_1_2[2], "%s", "?Three Leaf in a List?");
  sprintf (first.leaf_list_1_2[3], "%s", "The leaf broke the branch");


  first.has_empty_1_3 = 1;
  first.empty_1_3 = true;

  first.n_enum_1_4 = 2;
  first.enum_1_4[0] = FLAT_CONVERSION_ENUM14_SECOND;
  first.enum_1_4[1] = FLAT_CONVERSION_ENUM14_SEVENTH;
  first.enum_1_4[2] = FLAT_CONVERSION_ENUM14_FIRST;

  first.has_enum_1_5 = 1;
  first.enum_1_5 = BASE_CONVERSION_CB_ENUM_SEVENTH;

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &first;
  std::string xml_str;
  rw_status_t stat = mgr->pb_to_xml (pbcm, xml_str);

  ASSERT_EQ (RW_STATUS_SUCCESS, stat);

  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,second);

  pbcm = (ProtobufCMessage *) &second;
  std::string error_out;
  stat = mgr->xml_to_pb(xml_str.c_str(), pbcm, error_out);

  ASSERT_EQ (RW_STATUS_SUCCESS, stat);

  EXPECT_EQ (protobuf_c_message_get_packed_size(NULL, &first.base),
             protobuf_c_message_get_packed_size(NULL, &second.base));

  size_t size = protobuf_c_message_get_packed_size(NULL, &first.base);
  uint8_t *buffer_1 = (uint8_t *)calloc (sizeof (uint8_t), size);
  uint8_t *buffer_2 = (uint8_t *)calloc (sizeof (uint8_t), size);

  // ATTN: Is this test valid? this assumes that the position of the elements
  // do not change during conversions, which might not be true, and which might
  // not need to be true.

  protobuf_c_message_pack(NULL, &first.base, buffer_1);
  protobuf_c_message_pack(NULL, &second.base, buffer_2);

  EXPECT_TRUE (!memcmp (buffer_2, buffer_1,size));

  std::string xml_str2;
  stat = mgr->pb_to_xml (pbcm, xml_str2);

  ASSERT_EQ (RW_STATUS_SUCCESS, stat);

  ASSERT_EQ (xml_str2, xml_str);
  xml_str.resize(xml_str.length()/2);
  EXPECT_EQ (mgr->xml_to_pb(xml_str.c_str(), pbcm, error_out),RW_STATUS_FAILURE);
  EXPECT_TRUE (error_out.length());

  std::cout << error_out << std::endl;
}


TEST (ConversionAPI, NotRooted)
{
  TEST_DESCRIPTION("Convert non-rooted XML to PB");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* odvcs = model->load_module("other-data_rwvcs");
  ASSERT_TRUE(odvcs);

  const char* rooted1_xml =
      "<data xmlns=\"http://riftio.com/ns/riftware-1.0/other-data_rwvcs\"><data-top><rwvcs><rwcomponent_list><rwcomponent_info>"
      "<component_type>RWCLUSTER</component_type>"
      "<component_name>rwcluster-drone</component_name>"
      "<instance_id>1</instance_id>"
      "<instance_name>rwcluster-drone-1</instance_name>"
      "<rwcomponent_parent>rwcolony-1</rwcomponent_parent>"
      "<rwcluster_info>"
      "</rwcluster_info>"
      "</rwcomponent_info></rwcomponent_list></rwvcs></data-top></data>";

  std::string error_out;
  XMLDocument::uptr_t doc(mgr->create_document_from_string(rooted1_xml, error_out, false/*validate*/));
  ASSERT_TRUE(doc.get());
  XMLNode* root_node = doc->get_root_node();
  ASSERT_TRUE(root_node);

  RWPB_M_MSG_DECL_INIT(OtherDataRwvcs_RwvcsRwcomponentInfo,cinfo);
  rw_status_t rws = mgr->xml_to_pb(rooted1_xml, &cinfo.base, error_out);
  EXPECT_EQ(RW_STATUS_SUCCESS, rws);
  protobuf_c_message_free_unpacked_usebody(nullptr, &cinfo.base);

  const char* rooted2_xml =
      "<data><data-top><rwvcs><rwcomponent_list><rwcomponent_info>"
      "<component_type>RWCLUSTER</component_type>"
      "<component_name>rwcluster-drone</component_name>"
      "<instance_id>1</instance_id>"
      "<instance_name>rwcluster-drone-1</instance_name>"
      "<rwcomponent_parent>rwcolony-1</rwcomponent_parent>"
      "<rwcluster_info>"
      "</rwcluster_info>"
      "</rwcomponent_info></rwcomponent_list></rwvcs></data-top></data>";

  doc = std::move(mgr->create_document_from_string(rooted2_xml, error_out, false/*validate*/));
  ASSERT_TRUE(doc.get());
  root_node = doc->get_root_node();
  ASSERT_TRUE(root_node);

  RWPB_F_MSG_INIT(OtherDataRwvcs_RwvcsRwcomponentInfo,&cinfo);
  rws = mgr->xml_to_pb(rooted2_xml, &cinfo.base, error_out);
  EXPECT_EQ(RW_STATUS_SUCCESS, rws);
  protobuf_c_message_free_unpacked_usebody(nullptr, &cinfo.base);


}


TEST (RwYangDom, ChoiceTest)
{
  TEST_DESCRIPTION("Verify the functioning of choice and case");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(ydt_cli_test_header);
  xmlblob.append(
      "<choice-top-sib-str>Top Sib Str</choice-top-sib-str>"
      "<choice-top-sib-num>231</choice-top-sib-num>"
      "<case-top-1-1>Top-1-1</case-top-1-1>"
      "<case-top-1-2>Top-1-2</case-top-1-2>");

  xmlblob += ydt_cli_test_trailer;
  // std::cout << "Choice: DOM 1 = " << std::endl<< xmlblob<<std::endl;
  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  std::cout << "Choice: DOM 1 = " << std::endl;
  dom->to_stdout();
  std::cout << std::endl << std::endl;

  // Check the DOM before we merge to it
  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);
  YangNode* yn = root->get_yang_node();
  EXPECT_FALSE(yn);

  XMLNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  yn = top->get_yang_node();
  EXPECT_TRUE(yn);

  XMLNode* node = 0;
#undef YDT_CONT_CHECK
#define YDT_CONT_CHECK( n_,  truth_ )           \
  node = 0;                                     \
  node = top->find(n_);                         \
  truth_(node);

  YDT_CONT_CHECK("choice-top-sib-str", ASSERT_TRUE);
  YDT_CONT_CHECK("choice-top-sib-num", ASSERT_TRUE);
  YDT_CONT_CHECK("case-top-1-1", EXPECT_TRUE);
  YDT_CONT_CHECK("case-top-1-2", EXPECT_TRUE);

  xmlblob = ydt_xml_header;
  xmlblob.append(ydt_cli_test_header);
  xmlblob.append(
      "<case-top-2-1>Top-2-1</case-top-2-1>");

  xmlblob += ydt_cli_test_trailer;
  // std::cout << "Choice: DOM 1 = " << std::endl<< xmlblob<<std::endl;
  XMLDocument::uptr_t merge(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(merge.get());

  dom->merge(merge.get());
  

  YDT_CONT_CHECK("choice-top-sib-str", ASSERT_TRUE);
  YDT_CONT_CHECK("choice-top-sib-num", ASSERT_TRUE);
  YDT_CONT_CHECK("case-top-1-1", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-1-2", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-2-1", EXPECT_TRUE);

  xmlblob = ydt_xml_header;
  xmlblob.append(ydt_cli_test_header);
  xmlblob.append(
      "<case-top-3-1>"
      "<case-bot-2-1> some </case-bot-2-1>"
      "</case-top-3-1>");

  xmlblob += ydt_cli_test_trailer;

  XMLDocument::uptr_t merge2(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(merge2.get());

  dom->merge(merge2.get());
  YDT_CONT_CHECK("choice-top-sib-str", ASSERT_TRUE);
  YDT_CONT_CHECK("choice-top-sib-num", ASSERT_TRUE);
  YDT_CONT_CHECK("case-top-2-1", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-3-1", EXPECT_TRUE);

  ASSERT_TRUE (node->find ("case-bot-2-1"));

  //re-merge the first dom
  dom->merge(merge.get());

  YDT_CONT_CHECK("choice-top-sib-str", ASSERT_TRUE);
  YDT_CONT_CHECK("choice-top-sib-num", ASSERT_TRUE);
  YDT_CONT_CHECK("case-top-1-1", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-1-2", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-3-1", EXPECT_FALSE);
  YDT_CONT_CHECK("case-top-2-1", EXPECT_TRUE);
}

TEST(RwYangDom, BinaryTest)
{
  TEST_DESCRIPTION("Verify the functioning of Binary Types");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob = ydt_xml_header;
  xmlblob.append(ydt_test_header);
  xmlblob.append(
      "<apis_test><binary_test>"
      "<binary3>ZMgAWPXC/4A=</binary3>"
      "<binary4>UIIBIc2k/8Y4Yw==</binary4>"
      "<binary_list>MjM0NTY3</binary_list>"
      "<binary_list>yMnKy8zNzs/Q0dLT1NXW19jZ2ts=</binary_list>"
      "</binary_test></apis_test>");
  xmlblob += ydt_test_trailer;

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  ASSERT_TRUE(dom.get());

  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);

  XMLNode* top = root->get_first_child();
  ASSERT_TRUE(top);
  YangNode* yn = top->get_yang_node();
  EXPECT_TRUE(yn);

  XMLNode* node = top->find("apis_test");
  ASSERT_TRUE(node);
  EXPECT_TRUE(node->get_yang_node());

  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtTop_ApisTest,pb_tbin);
  rw_yang_netconf_op_status_t ncrs = node->to_pbcm(&pb_tbin.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  ASSERT_FALSE(pb_tbin.binary_test->has_binary1);
  ASSERT_FALSE(pb_tbin.binary_test->has_binary2);
  ASSERT_TRUE(pb_tbin.binary_test->has_binary3);
  ASSERT_TRUE(pb_tbin.binary_test->has_binary4);
  EXPECT_EQ(pb_tbin.binary_test->binary3.len, 8);
  EXPECT_EQ(pb_tbin.binary_test->binary4.len, 10);
  EXPECT_EQ(pb_tbin.binary_test->n_binary_list, 2);
  ASSERT_TRUE(pb_tbin.binary_test->binary_list);
  EXPECT_EQ(pb_tbin.binary_test->binary_list[0].len, 6);
  EXPECT_EQ(pb_tbin.binary_test->binary_list[1].len, 20);

  unsigned char test[8];
  test[0] = 100; test[1] = 200; test[2] = 0; test[3] = 88; test[4] = 245;
  test[5] = 194; test[6] = 255; test[7] = 128;
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary3.data, test, 8));

  unsigned char test1[10];
  test1[0] = 80; test1[1] = 130; test1[2] = 1; test1[3] = 33; test1[4] = 205;
  test1[5] = 164; test1[6] = 255; test1[7] = 198; test1[8] = 56; test1[9] = 99;
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary4.data, test1, 10));

  // Test default values.
  EXPECT_EQ(pb_tbin.binary_test->binary1.len, 8);
  EXPECT_EQ(pb_tbin.binary_test->binary2.len, 33);

  unsigned char test2[8];
  test2[0] = 10; test2[1] = 20; test2[2] = 30; test2[3] = 40; test2[4] = 209;
  test2[5] = 0; test2[6] = 255; test2[7] = 128;
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary1.data, test2, 8));

  unsigned char test3[33];
  for (unsigned i = 0; i < 25; i++) {
    test3[i] = i+15;
  }

  test3[25] = 0; test3[26] = 255; test3[27] = 254; test3[28] = 253; test3[29] = 200;
  test3[30] = 128; test3[31] = 4; test3[32] = 68;
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary2.data, test3, 33));

  // Test leaf-lists
  unsigned char testl1[6];
  for (unsigned i = 0; i < 6; i++) {
    testl1[i] = i+50;
  }
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary_list[0].data, testl1, 6));

  unsigned char testl2[20];
  for (unsigned i = 0; i < 20; i++) {
    testl2[i] = i+200;
  }
  EXPECT_EQ(0,memcmp(pb_tbin.binary_test->binary_list[1].data, testl2, 20));

  protobuf_c_message_free_unpacked_usebody(NULL, &pb_tbin.base);

  // Test pb to xml conversion.
  RWPB_M_MSG_DECL_INIT(TestYdomTop_TydtTop,pbtop);
  pbtop.apis_test = (RWPB_T_MSG(TestYdomTop_TydtTop_ApisTest) *)malloc(sizeof(RWPB_T_MSG(TestYdomTop_TydtTop_ApisTest)));
  RWPB_F_MSG_INIT(TestYdomTop_TydtTop_ApisTest,pbtop.apis_test);
  pbtop.apis_test->binary_test = (RWPB_T_MSG(TestYdomTop_TydtTop_ApisTest_BinaryTest) *)malloc(sizeof(RWPB_T_MSG(TestYdomTop_TydtTop_ApisTest_BinaryTest)));
  RWPB_F_MSG_INIT(TestYdomTop_TydtTop_ApisTest_BinaryTest,pbtop.apis_test->binary_test);

  // Fill data.
  pbtop.apis_test->binary_test->has_binary1 = true;
  pbtop.apis_test->binary_test->binary1.len = 5;
  pbtop.apis_test->binary_test->binary1.data = (uint8_t *)malloc(5);
  pbtop.apis_test->binary_test->binary1.data[0] = 1;
  pbtop.apis_test->binary_test->binary1.data[1] = 255;
  pbtop.apis_test->binary_test->binary1.data[2] = 5;
  pbtop.apis_test->binary_test->binary1.data[3] = 150;
  pbtop.apis_test->binary_test->binary1.data[4] = 54;

  pbtop.apis_test->binary_test->has_binary2 = true;
  pbtop.apis_test->binary_test->binary2.len = 256;
  pbtop.apis_test->binary_test->binary2.data = (uint8_t *)malloc(256);
  for (unsigned i = 0; i < 256; i++) {
    pbtop.apis_test->binary_test->binary2.data[i] = i;
  }

  // Fill for leaf-list
  pbtop.apis_test->binary_test->n_binary_list = 2;
  pbtop.apis_test->binary_test->binary_list = (ProtobufCBinaryData *)malloc(2*sizeof(ProtobufCBinaryData));
  pbtop.apis_test->binary_test->binary_list[0].len = 6;
  pbtop.apis_test->binary_test->binary_list[0].data = (uint8_t*)malloc(6);
  for (unsigned i = 0; i < 6; i++) {
    pbtop.apis_test->binary_test->binary_list[0].data[i] = i+50;
  }
  pbtop.apis_test->binary_test->binary_list[1].len = 20;
  pbtop.apis_test->binary_test->binary_list[1].data = (uint8_t*)malloc(20);
  for (unsigned i = 0; i < 20; i++) {
    pbtop.apis_test->binary_test->binary_list[1].data[i] = i+200;
  }

  ProtobufCMessage *pbcm = (ProtobufCMessage *) &pbtop;
  XMLDocument::uptr_t dom1(mgr->create_document_from_pbcm (pbcm, ncrs));
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  std::string xml = dom1->to_string();
  ASSERT_TRUE(xml.length());

  top = dom1->get_root_node();
  ASSERT_TRUE(top);

  XMLNode* at = top->find("apis_test");
  ASSERT_TRUE(at);

  XMLNode* bt = at->find("binary_test");
  ASSERT_TRUE(bt);

  XMLNode* b1 = bt->find("binary1");
  EXPECT_STREQ(b1->get_text_value().c_str(), "Af8FljY=");

  XMLNode* b2 = bt->find("binary2");
  ASSERT_TRUE(b2);
  EXPECT_STREQ(b2->get_text_value().c_str(), "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+P0BBQkNERUZHSElKS0xNTk9QUVJTVFVWV1hZWltcXV5fYGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6e3x9fn+AgYKDhIWGh4iJiouMjY6PkJGSk5SVlpeYmZqbnJ2en6ChoqOkpaanqKmqq6ytrq+wsbKztLW2t7i5uru8vb6/wMHCw8TFxsfIycrLzM3Oz9DR0tPU1dbX2Nna29zd3t/g4eLj5OXm5+jp6uvs7e7v8PHy8/T19vf4+fr7/P3+/w==");

  XMLNode* b_l1 = bt->find("binary_list");
  ASSERT_TRUE(b_l1);
  EXPECT_STREQ(b_l1->get_text_value().c_str(), "MjM0NTY3");

  XMLNode* b_l2 = b_l1->get_next_sibling();
  ASSERT_TRUE(b_l2);
  EXPECT_STREQ(b_l2->get_text_value().c_str(), "yMnKy8zNzs/Q0dLT1NXW19jZ2ts=");

  protobuf_c_message_free_unpacked_usebody(NULL, &pbtop.base);
}

TEST(RwYangDom, C_API)
{
  // Test the C API
  rw_xml_manager_t* mgr =rw_xml_manager_create_xerces_model(nullptr);
  rw_yang_model_t* c_model = rw_xml_manager_get_yang_model(mgr);

  YangModel* model = static_cast<YangModel*>(c_model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/conversion-1.xml");

  rw_xml_document_t* doc =
      rw_xml_manager_create_document_from_file (mgr, file_name.c_str(), false);

  ASSERT_TRUE (doc);
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,first);

  XMLDocument *dom = static_cast <XMLDocument *>(doc);

  rw_yang_netconf_op_status_t ncrs =
      dom->get_root_node()->get_first_child()->to_pbcm(&first.base);
  EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  CFMutableStringRef cf = CFStringCreateMutable (NULL, 0);

  rw_status_t status = rw_xml_manager_pb_to_xml_cfstring (mgr, &first, cf);

  ASSERT_EQ (status, RW_STATUS_SUCCESS);

  char str[5000], from_cf[5000];

  status = rw_xml_manager_pb_to_xml (mgr, &first, str, 5000);

  ASSERT_EQ (status, RW_STATUS_SUCCESS);

  CFStringGetCString (cf, from_cf, sizeof (from_cf), kCFStringEncodingUTF8);

  ASSERT_STREQ (from_cf, str);
}

TEST(RwYangDom, ListFindPbcm)
{
  TEST_DESCRIPTION("Verify the functioning of merging of lists");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *gc = get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)gc, ncrs));

  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ncrs = dom->get_root_node()->get_first_element()->merge((ProtobufCMessage *)gc);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  XMLNode *node = dom->get_root_node();
  ASSERT_TRUE (node);

  XMLNode *found = 0;

  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer_GList,key);

  YangNode *yn = node->get_yang_node();
  ASSERT_TRUE (yn);
  yn = yn->search_child("g-list");
  ASSERT_TRUE (yn);

  key.index = 0;
  ncrs = node->find_list_by_key_pb_msg(yn, &key, &found);
  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  EXPECT_TRUE (found);

  key.index = 10;
  ncrs = node->find_list_by_key_pb_msg(yn, &key, &found);
  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  EXPECT_TRUE (found);

  key.index = 1;
  ncrs = node->find_list_by_key_pb_msg(yn, &key, &found);
  EXPECT_NE (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  EXPECT_FALSE (found);

}

TEST(RwYangDom, AnonymousPbcm)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  // Before loading a schema - the conversion to anon should fail
  ProtobufCMessage *anon;
  ncrs = dom->get_root_node()->get_first_element()->to_pbcm(&anon);
  ASSERT_NE(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));
  ASSERT_EQ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
            model->get_ypbc_schema());
  //ASSERT_EQ ( (const rw_yang_pb_module_t*) RWPB_G_MODULE_YPBCMD(RiftCliTest),
  //ydom_top->get_ypbc_schema());

  XMLNode *node = dom->get_root_node();
  ncrs = node->to_pbcm(&anon);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  YangNode *yn = node->get_yang_node();
  ASSERT_TRUE (yn);
  const rw_yang_pb_msgdesc_t *msg = yn->get_ypbc_msgdesc();
  ASSERT_TRUE (msg);
  ASSERT_TRUE( protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)proto, anon));
}

TEST(RwYangDom, RPCOutputList)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* riftcli_mod = model->load_module("rift-cli-test");
  EXPECT_TRUE(riftcli_mod);
  model->register_ypbc_schema((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  TEST_DESCRIPTION ("Test of RPC output lists");
  rw_keyspec_path_t *ks = nullptr;
  ASSERT_EQ (RW_STATUS_SUCCESS,
             rw_keyspec_path_create_dup ((rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(RiftCliTest_output_ListOutput), nullptr , &ks));

  RWPB_M_MSG_DECL_INIT(RiftCliTest_output_ListOutput, output);
  RWPB_M_MSG_DECL_INIT(RiftCliTest_output_ListOutput_Cars, car);

  
  ProtobufCBinaryData key;
  key.data = nullptr;
  key.len = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS,
             rw_keyspec_path_get_binpath(ks, nullptr, (const uint8_t **) &key.data, &key.len));
    

  // The use case is that of an app filling up a RPC output. The keyspec is that
  // of the output, and each list element is filled up once. These should then
  // get serialized and merged into a XML document correclty.

  char make[100];
  char seats[10];

  XMLDocument::uptr_t dom(mgr->create_document());
  XMLNode *root = dom->get_root_node();

  RWPB_T_MSG (RiftCliTest_output_ListOutput_Cars) *array[1];
  output.cars = array;
  array[0] = &car;
  output.n_cars = 1;
  
  for (int i = 0; i < 5; i++) {
    sprintf (make, "Car type %d", i);
    sprintf (seats, "Seats %d", i * 4);

    car.make = make;
    car.seats = seats;

    ProtobufCBinaryData serial;
    uint8_t tmp_buf[2000];
    
    serial.data = tmp_buf;
    serial.len = protobuf_c_message_pack(NULL, (const ProtobufCMessage*)&output, tmp_buf);
    ASSERT_TRUE (serial.len < sizeof (tmp_buf));
    rw_yang_netconf_op_status_t ncs = root->merge (&key, &serial);
    ASSERT_EQ (ncs, RW_YANG_NETCONF_OP_STATUS_OK);
    ASSERT_EQ (root->get_first_element()->get_children()->length(), i+1);
  }

  rw_keyspec_path_free(ks, nullptr);
}

  
TEST(RwYangDom, RPC)
{
  TEST_DESCRIPTION ("Test of RPC functions and elements");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* riftcli_mod = model->load_module("rift-cli-test");
  EXPECT_TRUE(riftcli_mod);
  model->register_ypbc_schema((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  YangNode *rpc = riftcli_mod->search_node("both-args", "http://riftio.com/ns/yangtools/rift-cli-test");
  ASSERT_TRUE(rpc);


  RWPB_M_MSG_DECL_INIT(RiftCliTest_output_BothArgs_Op,op);
  const char *op_string = "goo";
  op.str_arg = (char *) op_string;
  RWPB_M_MSG_DECL_INIT(RiftCliTest_output_BothArgs,output);
  output.op = &op;


  // create DOM from output
  XMLDocument::uptr_t dom(mgr->create_document());

  dom->get_root_node()->add_child(rpc);

  rw_yang_netconf_op_status_t ncrs = dom->get_root_node()->get_first_element()->merge((ProtobufCMessage *)&output);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ProtobufCMessage *anon;
  ncrs = dom->get_root_node()->get_first_element()->to_rpc_output(&anon);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  ASSERT_TRUE( protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&output, anon));

  XMLNode *node = dom->get_root_node()->get_first_element()->find("op");
  ASSERT_TRUE(node);
  ASSERT_TRUE(dom->get_root_node()->get_first_element()->remove_child(node));


  // create DOM from input
  RWPB_M_MSG_DECL_INIT(RiftCliTest_input_BothArgs,input);
  input.has_int_arg = 1;
  input.int_arg = 55;

  dom = std::move(mgr->create_document());
  dom->get_root_node()->add_child(rpc);

  ncrs = dom->get_root_node()->get_first_element()->merge((ProtobufCMessage *)&input);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ncrs = dom->get_root_node()->get_first_element()->to_rpc_input(&anon);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  ASSERT_TRUE( protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&input, anon));


  // Build a binary version of the data to be merged
  XMLDocument::uptr_t dom2(mgr->create_document());
  ProtobufCBinaryData serial;
  uint8_t tmp_buf[2000];
  ASSERT_TRUE (protobuf_c_message_get_packed_size(NULL, (const ProtobufCMessage*)&op) <  sizeof (tmp_buf));
  serial.data = tmp_buf;
  serial.len = protobuf_c_message_pack(NULL, (const ProtobufCMessage*) &op, serial.data);
  ASSERT_TRUE (serial.len);

  // The first call should fail since the RPC node has not been made yet
  XMLNode *root = dom2->get_root_node();
  ASSERT_TRUE (root);
  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_DATA_MISSING, root->merge_rpc(&serial, "output"));
  root->add_child(rpc);
  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_OK,  root->merge_rpc(&serial, "output"));

  rw_keyspec_path_t *ks = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_output_BothArgs), nullptr ,&ks));

  ProtobufCBinaryData key;
  key.data = nullptr;
  key.len = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS,
             rw_keyspec_path_get_binpath(ks, nullptr, (const uint8_t **) &key.data, &key.len));

  auto rc = root->get_first_element()->merge(&serial, &key);
  EXPECT_EQ(rc, RW_YANG_NETCONF_OP_STATUS_DATA_MISSING);

  // Create with binpath will also update the dompath.. so the following
  // function returns even before trying to get the schema desc for the ks.
  //EXPECT_DEATH (root->get_first_element()->merge(&serial, &key), "");
}


TEST(RwYangDom, Keyspec)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  // Get to some list entry (for now)
  XMLNode *node = dom->get_root_node()->get_first_element()->get_next_sibling()->get_next_sibling();
  YangNode *yn = node->get_yang_node();
  ASSERT_TRUE (yn);
  const rw_yang_pb_msgdesc_t* msgdesc = yn->get_ypbc_msgdesc();
  ASSERT_TRUE (msgdesc);

  // Build a keypath for this node
  ASSERT_TRUE(msgdesc->schema_path_value);
  const ProtobufCMessageDescriptor *desc = msgdesc->schema_path_value->base.descriptor;
  ProtobufCMessage *key_spec = 0;

  rw_status_t rs = rw_keyspec_path_create_dup ((const rw_keyspec_path_t *)msgdesc->schema_path_value, nullptr ,
                                          (rw_keyspec_path_t **)&key_spec);

  // Interpreting the keyspec as a generic message now.
  // Shouldnt these be equivalent?
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);
  ASSERT_EQ (key_spec->descriptor,desc);

  std::stack<XMLNode *> node_stack;

  while (node) {
    node_stack.push (node);
    node = node->get_parent();
  }


  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  ASSERT_TRUE (fld);
  ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;
  ASSERT_TRUE (*((char *)key_spec + fld->quantifier_offset));

  char *base = (char *)key_spec + fld->offset;
  char *p_value = 0;

  size_t dom_path_index = 2; // skip the first 2 fields: rooted, category

  while (!node_stack.empty()) {
    node = node_stack.top();
    ASSERT_TRUE (dom_path_index < desc->n_fields);
    fld = &desc->fields[dom_path_index];

    const ProtobufCMessageDescriptor *p_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;
    p_value = base + fld->offset;

    // Move through the message descriptor, setting any needed values
    // index 0 is element id, skip over that.
    ASSERT_STREQ (p_desc->fields[0].name, "element_id");

    // Walk the protobuf descriptors and yang key list at the same time.
    // The keys in the protobuf should be in the same order as those of
    // the yang model

    YangKeyIter yki;
    YangNode *yn =node->get_yang_node();

    ASSERT_TRUE (yn);

    if (yn->is_listy()) {
      yki = yn->key_begin();
    }

    for (size_t k_index = 0; k_index < p_desc->n_fields; k_index++) {

      fld = &p_desc->fields[k_index];
      if (RW_SCHEMA_TAG_ELEMENT_KEY_START > fld->id ||
          RW_SCHEMA_TAG_ELEMENT_KEY_END < fld->id) {
        // not a key.
        continue;
      }

      ASSERT_TRUE (node->get_yang_node()->key_end() != yki);

      // keys should always be optional submessages.


      ASSERT_TRUE (PROTOBUF_C_LABEL_OPTIONAL == fld->label);
      ASSERT_TRUE (PROTOBUF_C_TYPE_MESSAGE == fld->type);


      // set the quantifier flag to true. If this key is not found, the function
      // will fail?
      *(int *)(p_value + fld->quantifier_offset) = 1;

      // Update p_value to point to the start of the key[k_index] message
      p_value += fld->offset;

      const ProtobufCMessageDescriptor *k_desc =
          (const ProtobufCMessageDescriptor *) fld->descriptor;

      YangNode *k_node = yki->get_key_node();

      ASSERT_TRUE (k_node);
      const char *k_name = k_node->get_pbc_field_name();

      ASSERT_TRUE (nullptr != k_name);
      ASSERT_STREQ ("index", k_name);

      fld = protobuf_c_message_descriptor_get_field_by_name (k_desc, k_name);
      ASSERT_TRUE (fld);

      // The value to set is in the xml node.
      XMLNode *k_xml = node->find(k_node->get_name(), k_node->get_ns());
      ASSERT_TRUE (k_xml);

      ASSERT_TRUE (k_xml->get_text_value().c_str());

      protobuf_c_boolean ok =
          protobuf_c_message_set_field_text_value(NULL,
                                                  (ProtobufCMessage*) p_value,
                                                  fld,
                                                  k_xml->get_text_value().c_str(),
                                                  k_xml->get_text_value().length());
      ASSERT_TRUE(ok);

      yki++;
    }
    dom_path_index++;
    node_stack.pop();
  }
  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList) *spec_key =
      (RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)*)key_spec;

  EXPECT_TRUE (spec_key->has_dompath);
  EXPECT_TRUE (spec_key->dompath.rooted);
  EXPECT_TRUE (spec_key->dompath.has_path001);
  EXPECT_TRUE (spec_key->dompath.path001.has_key00);
  EXPECT_EQ (20, spec_key->dompath.path001.key00.index);

  // Try with the API now.
  node = dom->get_root_node()->get_first_element()->get_next_sibling()->get_next_sibling();
  rw_keyspec_path_t *another = 0;
  ncrs = node->to_keyspec(&another);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_TRUE (rw_keyspec_path_is_equal (nullptr, (rw_keyspec_path_t *)key_spec, another));

  ProtobufCMessage *anon;
  //
  ncrs = node->to_pbcm (&anon);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_EQ (anon->descriptor, RWPB_G_MSG_PBCMD(RiftCliTest_data_GeneralContainer_GList));

  // construct a dom from the key spec.
  XMLDocument::uptr_t from_ks(mgr->create_document());

  // ncrs = from_ks->get_root_node()->merge(&spec_key->rw_keyspec_path_t);
  // ASSERT_EQ (ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  const rw_yang_pb_msgdesc_t* top = anon->descriptor->ypbc_mdesc;

  while (top->parent) {
    top = top->parent;
  }

  ASSERT_TRUE (top);

  const rw_yang_pb_msgdesc_t* curr;
  rw_keyspec_path_t *unused = 0;
  p_value = (char *) spec_key;
  rs = rw_keyspec_path_find_msg_desc_schema (&spec_key->rw_keyspec_path_t, nullptr,
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
      &curr, &unused);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_EQ (curr, msgdesc->schema_path_ypbc_desc);

  // for a walk again..
  node = from_ks->get_root_node();
  desc = msgdesc->schema_path_value->base.descriptor;
  yn = nullptr;
  // Get to DOMPATH first.
  fld = protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  ASSERT_TRUE (fld);
  ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);

  p_value += fld->offset;

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;
  ASSERT_TRUE (*((char *)key_spec + fld->quantifier_offset));

  size_t depth = rw_keyspec_path_get_depth(reinterpret_cast<rw_keyspec_path_t *>(spec_key));
  ASSERT_EQ(depth, 2);

  for (size_t path_index = 0; path_index < desc->n_fields; path_index++){

    char *m_value;

    fld = &desc->fields[path_index];
    if (fld->id < RW_SCHEMA_TAG_PATH_ENTRY_START ||
        fld->id > RW_SCHEMA_TAG_PATH_ENTRY_END) {
      // not a path entry..
      continue;
    }

    ASSERT_TRUE(fld->label == PROTOBUF_C_LABEL_OPTIONAL); // All path entries are optional now..

    ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);
    const ProtobufCMessageDescriptor * p_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;

    /* 
     * ATTN: Please revisit this later.
     * Additional path entries are now added in concrete keyspecs.
     * So, skip the path entires if it is not set in the keyspec.
     */
    if (p_desc == &rw_schema_path_entry__descriptor) {
      /* 
       * Generic path entry inside a concrete keyspec. This is currently not
       * used.. Skip it. Come back later when it is used.
       */
      continue;
    }

    m_value = p_value + fld->offset;

    fld = protobuf_c_message_descriptor_get_field_by_name (p_desc, "element_id");
    ASSERT_TRUE (fld);

    RwSchemaElementId *elem_id = (RwSchemaElementId *)(m_value + fld->offset);

    if (!yn) {
      // find the yang node from the top node of the message. This should also
      // be the root of the keyspec.
      YangModule *ym = model->search_module_ns (top->module->ns);
      ASSERT_TRUE (ym);
      ASSERT_EQ (ym, ydom_top);
      yn = ym->search_node(top->yang_node_name, top->module->ns);
    } else {
      // find the child yang node..
      yn = yn->search_child (elem_id->system_ns_id, elem_id->element_tag);
    }

    ASSERT_TRUE (yn);

    // append a node for this element, if one does not exist.
    // further nodes will always be children of
    // this node,

    XMLNode *child = node->find(yn->get_name(), yn->get_ns());
    if (nullptr == child) {
      node = node->add_child (yn);
    } else {
      node = child;
    }

    for (size_t key_index = 0; key_index < p_desc->n_fields; key_index++) {

      fld = &p_desc->fields[key_index];

      if (RW_SCHEMA_TAG_ELEMENT_KEY_START > fld->id ||
          RW_SCHEMA_TAG_ELEMENT_KEY_END < fld->id) {
        // not a key.
        continue;
      }

      ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);
      const ProtobufCMessageDescriptor *k_desc =
          (const ProtobufCMessageDescriptor *) fld->descriptor;

      char *k_value = m_value + fld->offset;

      for (size_t fld_index = 0; fld_index < k_desc->n_fields; fld_index++) {
        // A rather suspicious test. nothing else helps..
        fld = &k_desc->fields[fld_index];
        if (fld->id == 1) {
          continue;
        }

        YangNode *k_node = yn->search_child_by_mangled_name(fld->name);
        ASSERT_TRUE (k_node);

        ProtobufCFieldInfo finfo;
        bool ok = protobuf_c_message_get_field_instance(
          NULL/*instance*/, (ProtobufCMessage*)k_value, fld, 0/*repeated_index*/, &finfo);
        ASSERT_TRUE(ok);

        PROTOBUF_C_CHAR_BUF_DECL_INIT(NULL, cbuf);
        protobuf_c_boolean rc = protobuf_c_field_info_get_xml( &finfo, &cbuf );
        ASSERT_TRUE(rc);

        XMLNode *k_xml = node->find_value(k_node->get_name(), cbuf.buffer, k_node->get_ns());
        if (nullptr == k_xml) {
          ncrs = node->append_node_from_pb_field( &finfo, k_node );
        }
        PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
      }
    }
  }

  // Walk is done!

  node = from_ks->get_root_node()->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "general-container");

  node = node->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "g-list");

  XMLNode *found = node->find_value("index", "20");
  ASSERT_TRUE (found);

  found = node->find_value ("index", "2");
  ASSERT_FALSE (found);


}

TEST(RwXML, LeafListKeySpec)
{
  rw_yang_netconf_op_status_t ncrs;
  RWPB_M_MSG_DECL_INIT(RiftCliTest_RctGenCntnr, g_container);
  int32_t int32_list[] = { 1973, 5, 26, 1010321};

  g_container.int32_list = int32_list;
  g_container.n_int32_list = sizeof (int32_list)/sizeof (int32_t);
      
  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)&g_container, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  XMLNode *node =
      dom->get_root_node()->get_first_child()->get_first_child()->get_next_sibling();

  rw_keyspec_path_t *another = nullptr;
  ncrs = node->to_keyspec(&another);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GContainer_Int32List) *spec_ks =
      (RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GContainer_Int32List) *)another;

  ASSERT_TRUE(spec_ks->has_dompath);
  ASSERT_TRUE(spec_ks->dompath.has_path000);
  ASSERT_TRUE(spec_ks->dompath.has_path001);
  ASSERT_TRUE(spec_ks->dompath.has_path002);
  ASSERT_TRUE(spec_ks->dompath.path002.has_key00);
  ASSERT_EQ(spec_ks->dompath.path002.key00.int32_list, 5);
  

  
  
}
  
TEST(RwYangDom, KeyspecAPI)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  // Get to some list entry (for now)
  XMLNode *node = dom->get_root_node()->get_first_element()->get_next_sibling()->get_next_sibling();
  rw_keyspec_path_t *another = 0;
  ncrs = node->to_keyspec(&another);

  ProtobufCMessage *anon;
  //
  ncrs = node->to_pbcm (&anon);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_EQ (anon->descriptor, RWPB_G_MSG_PBCMD(RiftCliTest_data_GeneralContainer_GList));

  // construct a dom from the key spec.
  XMLDocument::uptr_t from_ks(mgr->create_document());

  node = from_ks->get_root_node();
  ncrs = node->merge(another, anon);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);

  // Walk is done!

  node = from_ks->get_root_node()->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "general-container");

  node = node->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "g-list");

  XMLNode *found = node->find_value("index", "20");
  ASSERT_TRUE (found);

  found = node->find_value ("index", "2");
  ASSERT_FALSE (found);

  node = node->find("gcl-container");
  ASSERT_TRUE (node);

  found = node->find_value("having_a_bool", "true");
  ASSERT_TRUE (found);

  // Build a keyspec from a leaf, ie, found
  ncrs = found->to_keyspec(&another);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
}

TEST(RwYangDom, AnonPbcmKeyspec)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  // Get to some list entry (for now)
  XMLNode *node = dom->get_root_node()->get_first_element()->get_next_sibling();
  rw_keyspec_path_t *keyspec = 0;
  ncrs = node->to_keyspec(&keyspec);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ProtobufCMessage *msg;
  //
  ncrs = node->to_pbcm (&msg);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_EQ (msg->descriptor, RWPB_G_MSG_PBCMD(RiftCliTest_data_GeneralContainer_GList));

  rw_status_t rs = rw_keyspec_path_update_binpath(keyspec, nullptr);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  ProtobufCBinaryData key;
  key.data = nullptr;
  key.len = 0;
  rs = rw_keyspec_path_get_binpath(keyspec, nullptr, (const uint8_t **) &key.data, &key.len);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  rw_keyspec_path_t *gen = rw_keyspec_path_create_with_binpath_binary_data(nullptr, &key);
  ASSERT_TRUE (gen);

  // Serialize the msg
  uint8_t tmp_buf[2000];
  ProtobufCBinaryData serial;
  ASSERT_TRUE (protobuf_c_message_get_packed_size(NULL, msg) < sizeof (tmp_buf));
  serial.data = tmp_buf;
  serial.len = protobuf_c_message_pack(NULL, msg, serial.data);
  ASSERT_TRUE (serial.len);

  protobuf_c_message_free_unpacked (nullptr, msg);
  
  rs = rw_keyspec_path_deserialize_msg_ks_pair (nullptr, (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
                                           &serial, &key,
                                           &msg, &keyspec);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  
  rw_keyspec_path_free (keyspec, nullptr);
  keyspec = 0;

  const rw_yang_pb_msgdesc_t* curr_msgdesc;
  rw_keyspec_path_t *unused = 0;


  rs =
      rw_keyspec_path_find_msg_desc_schema (
          (rw_keyspec_path_t *)gen, 
          nullptr,
          (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
          &curr_msgdesc, &unused);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);


  keyspec = rw_keyspec_path_create_dup_of_type(gen, nullptr, curr_msgdesc->pbc_mdesc);

  ProtobufCMessage *key_spec = (ProtobufCMessage *)keyspec;


  char *p_value = (char *) key_spec;
  XMLDocument::uptr_t from_ks(mgr->create_document());
  // for a walk again..
  node = from_ks->get_root_node();
  const ProtobufCMessageDescriptor*  desc = curr_msgdesc->pbc_mdesc;
  YangNode *yn = nullptr;

  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  ASSERT_TRUE (fld);
  ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);

  p_value += fld->offset;

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;
  ASSERT_TRUE (*((char *)key_spec + fld->quantifier_offset));

  for (size_t path_index = 0; path_index < desc->n_fields; path_index++){

    char *m_value;

    fld = &desc->fields[path_index];
    if (fld->id < RW_SCHEMA_TAG_PATH_ENTRY_START ||
        fld->id > RW_SCHEMA_TAG_PATH_ENTRY_END) {
      // not a path entry..
      continue;
    }

    ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);
    ASSERT_TRUE (fld->label == PROTOBUF_C_LABEL_OPTIONAL);  // path entries are optional now.
    const ProtobufCMessageDescriptor * p_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;

    /* 
     * ATTN: Please revisit this later.
     * Additional path entries are now added in concrete keyspecs.
     * So, skip the path entires if it is not set in the keyspec.
     */
    if (p_desc == &rw_schema_path_entry__descriptor) {
      /* 
       * Generic path entry inside a concrete keyspec. This is currently not
       * used.. Skip it. Come back later when it is used.
       */
      continue;
    }

    m_value = p_value + fld->offset;

    fld = protobuf_c_message_descriptor_get_field_by_name (p_desc, "element_id");
    ASSERT_TRUE (fld);

    RwSchemaElementId *elem_id = (RwSchemaElementId *)(m_value + fld->offset);

    if (!yn) {
      // find the yang node from the top node of the message. This should also
      // be the root of the keyspec.
      NamespaceManager& nm_mgr=NamespaceManager::get_global();
      const char *m_name = nm_mgr.nshash_to_string (elem_id->system_ns_id);
      ASSERT_STREQ (m_name, "http://riftio.com/ns/yangtools/rift-cli-test");

      YangModule *ym = model->search_module_ns (m_name);
      ASSERT_TRUE (ym);
      ASSERT_EQ (ym, ydom_top);

      // Get the schema for the module
      const rw_yang_pb_module_t* ypbc_module = ym->get_ypbc_module();
      ASSERT_TRUE (ypbc_module);
      m_name = 0;

      // get the yang module name from the list of messages
      if (ypbc_module->data_module) {
        for (size_t i = 0; i < ypbc_module->data_module->child_msg_count; i++) {
          if (ypbc_module->data_module->child_msgs[i]->pb_element_tag == elem_id->element_tag) {
            m_name = ypbc_module->data_module->child_msgs[i]->yang_node_name;
            break;
          }
        }
      }

      // get the yang module name from the list of messages
      if (ypbc_module->rpci_module) {
        for (size_t i = 0; i < ypbc_module->rpci_module->child_msg_count; i++) {
          if (ypbc_module->rpci_module->child_msgs[i]->pb_element_tag == elem_id->element_tag) {
            m_name = ypbc_module->rpci_module->child_msgs[i]->yang_node_name;
            break;
          }
        }
      }

      ASSERT_TRUE (m_name);
      ASSERT_STREQ (m_name, "general-container");

      yn = ym->search_node (m_name, ym->get_ns());

    } else {
      // find the child yang node..
      yn = yn->search_child (elem_id->system_ns_id, elem_id->element_tag);
    }

    ASSERT_TRUE (yn);

    // append a node for this element, if one does not exist.
    // further nodes will always be children of
    // this node,

    XMLNode *child = node->find(yn->get_name(), yn->get_ns());
    if (nullptr == child) {
      node = node->add_child (yn);
    } else {
      node = child;
    }

    for (size_t key_index = 0; key_index < p_desc->n_fields; key_index++) {

      fld = &p_desc->fields[key_index];

      if (RW_SCHEMA_TAG_ELEMENT_KEY_START > fld->id ||
          RW_SCHEMA_TAG_ELEMENT_KEY_END < fld->id) {
        // not a key.
        continue;
      }

      ASSERT_TRUE (fld->type == PROTOBUF_C_TYPE_MESSAGE);
      const ProtobufCMessageDescriptor *k_desc =
          (const ProtobufCMessageDescriptor *) fld->descriptor;

      char *k_value = m_value + fld->offset;

      for (size_t fld_index = 0; fld_index < k_desc->n_fields; fld_index++) {
        // A rather suspicious test. nothing else helps..
        fld = &k_desc->fields[fld_index];
        if (fld->id == 1) {
          continue;
        }

        YangNode *k_node = yn->search_child_by_mangled_name(fld->name);
        ASSERT_TRUE (k_node);

        ProtobufCFieldInfo finfo;
        bool ok = protobuf_c_message_get_field_instance(
          NULL/*instance*/, (ProtobufCMessage*)k_value, fld, 0/*repeated_index*/, &finfo);
        ASSERT_TRUE(ok);

        PROTOBUF_C_CHAR_BUF_DECL_INIT(NULL, cbuf);
        protobuf_c_boolean rc = protobuf_c_field_info_get_xml( &finfo, &cbuf );
        ASSERT_TRUE(rc);

        XMLNode *k_xml = node->find_value(k_node->get_name(), cbuf.buffer, k_node->get_ns());
        if (nullptr == k_xml) {
          ncrs = node->append_node_from_pb_field( &finfo, k_node );
        }
        PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
      }
    }
  }

  // yn is also the last non-leafy node. This should thus have the schema for
  // the undecoded protobuf
  curr_msgdesc = yn->get_ypbc_msgdesc();
  ASSERT_TRUE (curr_msgdesc);

  ProtobufCMessage *anon = protobuf_c_message_unpack(nullptr, curr_msgdesc->pbc_mdesc, serial.len, serial.data);
  ASSERT_TRUE (anon);

  ncrs = node->merge(anon);
  ASSERT_EQ (ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  // Walk is done!

  node = from_ks->get_root_node()->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "general-container");

  node = node->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "g-list");

  XMLNode *found = node->find_value("index", "10");
  ASSERT_TRUE (found);

  found = node->find_value ("index", "2");
  ASSERT_FALSE (found);

  found = node->find_value ("index", "0");
  ASSERT_FALSE (found);

  node = node->find("gcl-container");
  ASSERT_TRUE (node);

  found = node->find_value("having_a_bool", "true");
  ASSERT_FALSE (found);

  found = node->find("gclc-empty");
  ASSERT_TRUE (found);

}


TEST(RwYangDom, AnonPbcmKeyspecApi)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  // Get to some list entry (for now)
  XMLNode *node = dom->get_root_node()->get_first_element()->get_next_sibling()->get_next_sibling();
  rw_keyspec_path_t *keyspec = 0;
  ncrs = node->to_keyspec(&keyspec);

  rw_status_t rs = rw_keyspec_path_update_binpath(keyspec, nullptr);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  ProtobufCMessage *msg;
  //
  ncrs = node->to_pbcm (&msg);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);
  ASSERT_EQ (msg->descriptor, RWPB_G_MSG_PBCMD(RiftCliTest_data_GeneralContainer_GList));

  // Serialize the msg
  uint8_t tmp_buf[2000];
  ProtobufCBinaryData serial;
  ASSERT_TRUE (protobuf_c_message_get_packed_size(NULL, msg) < sizeof (tmp_buf));
  serial.data = tmp_buf;
  serial.len = protobuf_c_message_pack(NULL, msg, serial.data);
  ASSERT_TRUE (serial.len);

  ProtobufCBinaryData key;

  XMLDocument::uptr_t from_ks(mgr->create_document());
  node = from_ks->get_root_node();

  rs = rw_keyspec_path_get_binpath(keyspec, nullptr, (const uint8_t **) &key.data, &key.len);

  ncrs = node->merge (&key, &serial);
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);

  node = from_ks->get_root_node()->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "general-container");

  node = node->get_first_element();
  ASSERT_TRUE (node);
  ASSERT_STREQ(node->get_local_name().c_str(), "g-list");
  // ATTN: check its correctness
  node->get_full_prefix_text();
  // ATTN: check its correctness
  node->get_full_suffix_text();
  
  XMLNode *found = node->find_value("index", "20");
  ASSERT_TRUE (found);

  // ATTN: check its correctness
  found->get_full_prefix_text();
  // ATTN: check its correctness
  found->get_full_suffix_text();

  found = node->find_value ("index", "2");
  ASSERT_FALSE (found);

  node = node->find("gcl-container");
  ASSERT_TRUE (node);

  found = node->find_value("having_a_bool", "true");
  ASSERT_TRUE (found);
  ASSERT_EQ (found, rw_xml_node_find_value(node, "having_a_bool", "true", 0));
}

TEST(RwYangDom, XmlOrder)
{
  TEST_DESCRIPTION ("Test of XML parsing field order");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);
  rw_status_t rs = model->load_schema_ypbc(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwCompositeD));
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  // RIFT-3549
  const char* xml = 
      "<rw-base:colony xmlns:rw-base=\"http://riftio.com/ns/riftware-1.0/rw-base\">"
      "<rw-base:name>trafsink</rw-base:name>"
      "<rw-base:network-context>"
      "<rw-base:name>trafgen-lb</rw-base:name>"
      "<rw-fpath:interface xmlns:rw-fpath=\"http://riftio.com/ns/riftware-1.0/rw-fpath\">"
      "<rw-fpath:name>"
      "LOADBAL_TO_TRAFGEN_INTERFACE_1"
      "</rw-fpath:name>"
      "<rw-fpath:ip>"
      "<rw-fpath:address>11.0.1.3/24</rw-fpath:address>"
      "</rw-fpath:ip>"
      "<rw-fpath:bind>"
      "<rw-fpath:port>trafsink/4/1</rw-fpath:port>"
      "</rw-fpath:bind>"
      "</rw-fpath:interface>"
      "</rw-base:network-context>"
      "</rw-base:colony>";

  // Convert to and from XML to verify order.
  RWPB_M_MSG_DECL_INIT(RwCompositeD_ConfigColony, cfgcol);
  ASSERT_TRUE(cfgcol.base.descriptor);

  rs = rw_xml_manager_xml_to_pb(mgr.get(), xml, &cfgcol.base, nullptr);
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  char outxml[4096] = {};
  rs = rw_xml_manager_pb_to_xml(mgr.get(), &cfgcol.base, outxml, sizeof(outxml));
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  rs = rw_xml_manager_pb_to_xml_unrooted(mgr.get(), &cfgcol.base, outxml, sizeof(outxml));
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  EXPECT_STREQ(xml, outxml);

}

TEST(RwYangDom, KeyspecSearch)
{

  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  rw_yang_netconf_op_status_t ncrs;

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  // Get to some list entry (for now)
  XMLNode *node = dom->get_root_node()->get_first_element()->get_next_sibling();
  rw_keyspec_path_t *keyspec = 0;
  ncrs = node->to_keyspec(&keyspec);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  //
  YangNode *yn = node->get_yang_node();
  ASSERT_TRUE (yn);

  RiftCliTest__YangKeyspecPath__RiftCliTest__Data__GeneralContainer__GList *spec_ks =
      (RiftCliTest__YangKeyspecPath__RiftCliTest__Data__GeneralContainer__GList *) keyspec;
  
  XMLNode *found = node->get_parent()->find_list_by_key (yn, (ProtobufCMessage *)&spec_ks->dompath.path001, nullptr);
  ASSERT_TRUE (found);

  std::list<XMLNode*> many;
  ncrs = dom->get_root_node()->find((rw_keyspec_path_t *)spec_ks, many);
  ASSERT_EQ (ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  ASSERT_EQ (1, many.size());

  ASSERT_EQ (*many.begin(), node);
  
}


TEST(RwYangDom, ListKeyValidation)
{
  
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/conversion-1.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                          (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());
  
  XMLNode* root_node = dom->get_root_node();
  ASSERT_TRUE(root_node);
  root_node = root_node->get_first_child();
  ASSERT_TRUE(root_node);

  XMLNode* cont_1 = root_node->find("container_1-1");
  ASSERT_TRUE (cont_1);

  XMLNode* list_1 = cont_1->find("list-1.1_2");
  ASSERT_TRUE (list_1);

  EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, cont_1->validate());
  EXPECT_EQ(RW_YANG_NETCONF_OP_STATUS_OK, rw_xml_node_validate(cont_1));
  EXPECT_EQ(list_1, rw_xml_node_find(cont_1, "list-1.1_2", 0));
  XMLNode* list_2 = list_1->get_next_sibling(list_1->get_local_name(), list_1->get_name_space());
  ASSERT_TRUE (list_2);

  rw_yang_netconf_op_status_t ncrs = list_1->validate();
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);

  ncrs = list_2->validate();
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);

  XMLNode *key_n = list_2->find("int_1.1.2_1");
  ASSERT_TRUE (key_n);

  ASSERT_TRUE (list_2->remove_child(key_n));

  ncrs = list_2->validate();
  ASSERT_NE (RW_YANG_NETCONF_OP_STATUS_OK, ncrs);

  // RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,first);

  // ncrs =
  //     dom->get_root_node()->to_pbcm(&first.base);
  // EXPECT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  // ASSERT_EQ(1, first.container_1_1.n_list_1_1_2);
  
}

TEST(Xml2GI, SelCrit)
{
  TEST_DESCRIPTION ("Test XML to GI");

  static const char *xml =
      "<?xml version=\"1.0\"?>"
      "<data>"
      "<colony xmlns=\"http://riftio.com/ns/riftware-1.0/rw-base\">"
      "<rw-base:name xmlns:rw-base=\"http://riftio.com/ns/riftware-1.0/rw-base\"></rw-base:name>"
      "<rw-base:network-context xmlns:rw-base=\"http://riftio.com/ns/riftware-1.0/rw-base\">"
      "<rw-base:name></rw-base:name>"
      "<rw-fpath:scriptable-lb xmlns:rw-fpath=\"http://riftio.com/ns/riftware-1.0/rw-fpath\">"
      "<rw-fpath:name></rw-fpath:name>"
      "<rw-fpath:server-selection-function>"
      "<rw-fpath:selection-criteria>"
      "<rw-fpath:dns-lb>"
      "<rw-fpath:match-rule>"
      "<rw-fpath:domain>abc.com</rw-fpath:domain>"
      "<rw-fpath:server-group>abc</rw-fpath:server-group>"
      "</rw-fpath:match-rule>"
      "<rw-fpath:match-rule>"
      "<rw-fpath:domain>abc1.com</rw-fpath:domain>"
      "<rw-fpath:server-group>abc</rw-fpath:server-group>"
      "</rw-fpath:match-rule>"
      "</rw-fpath:dns-lb>"
      "</rw-fpath:selection-criteria>"
      "</rw-fpath:server-selection-function>"
      "</rw-fpath:scriptable-lb>"
      "</rw-base:network-context>"
      "</colony>"
      "</data>";

  printf("%s\n", xml);

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);
  rw_status_t rs = model->load_schema_ypbc(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwFpathD));
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  rwpb_gi_RwFpathD_SelectionCriteria *boxed;

  GError *err = nullptr;
  boxed = rw_fpath_d__yang_data__rw_base_d__colony__network_context__scriptable_lb__server_selection_function__selection_criteria__gi_new();
  rw_fpath_d__yang_data__rw_base_d__colony__network_context__scriptable_lb__server_selection_function__selection_criteria__gi_from_xml(
      boxed, model, xml, &err);
}

TEST(RwYangDom, KeylessLists)
{
  // PB to XML first?
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(FlatConversion));

  RWPB_M_MSG_DECL_INIT(FlatConversion_data_Flat, flat);

  RWPB_T_MSG (FlatConversion_data_Flat_Keyless)* kf = &flat.keyless[0];
  flat.n_keyless = 4;
  
  sprintf (kf->str, "%s", "First One in the keyless list");
  kf->has_str = 1;
  kf->has_cont = 1;
  kf->cont.has_bool_ = 1;
  kf->has_number = 1;
  kf->number = 1234;

  kf = &flat.keyless[1];
  sprintf (kf->str, "%s", "More no keys");
  kf->has_str = 1;
  kf->has_enum_ = 1;
  kf->enum_ = FLAT_CONVERSION_ENUM_FOO_BAR;

  kf = &flat.keyless[2];
  kf->has_number = 1;
  kf->number = 12;
  kf->has_enum_ = 1;
  kf->enum_ = FLAT_CONVERSION_ENUM_FOO;

  kf = &flat.keyless[3];
  sprintf (kf->str, "%s", "First One in the keyless list");
  kf->has_str = 1;
  kf->has_cont = 1;
  kf->cont.has_bool_ = 1;
  kf->has_number = 1;
  kf->number = 1234;

  rw_yang_netconf_op_status_t ncrs;
  
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)&flat, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  XMLNode *node = dom->get_root_node(); //@ flat
  ASSERT_EQ (4, node->get_children()->length());

  ProtobufCMessage *msg;
  //
  ncrs = dom->get_root_node()->to_pbcm (&msg);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  ncrs = node->merge (msg);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  // keyless merge - numbers should double
  ASSERT_EQ (8, node->get_children()->length());

  rw_keyspec_path_t *keyspec = 0;
  ncrs = node->get_first_child()->to_keyspec(&keyspec);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  
  ProtobufCBinaryData key;
  key.data = nullptr;
  key.len = 0;
  rw_status_t
      rs = rw_keyspec_path_get_binpath(keyspec, nullptr, (const uint8_t **) &key.data, &key.len);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);


  // Try with a serialized ks and msg..
  uint8_t tmp_buf[2000];
  ProtobufCBinaryData serial;
  ASSERT_TRUE (protobuf_c_message_get_packed_size(NULL, (ProtobufCMessage *)kf) < sizeof (tmp_buf));
  serial.data = tmp_buf;
  serial.len = protobuf_c_message_pack(NULL, (ProtobufCMessage *)kf, serial.data);
  ASSERT_TRUE (serial.len);

  ncrs = node->merge (&key, &serial);
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  // keyless merge - numbers should double
  ASSERT_EQ (9, node->get_children()->length());
}
                                     

TEST(RwYangDom, NullYangNode)
{
  
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  ASSERT_TRUE(test_ncx);

  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/conversion-1.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                          (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());
  
  XMLNode* root_node = dom->get_root_node();
  ASSERT_TRUE(root_node);
  root_node = root_node->get_first_child();
  ASSERT_TRUE(root_node);

  XMLNode* cont_invalid = dom->get_root_child_element("container_1-1");
  ASSERT_FALSE (cont_invalid);

  cont_invalid = root_node->find("container_1-1");
  ASSERT_NE (nullptr, cont_invalid);
  cont_invalid->set_yang_node(nullptr);
  ASSERT_FALSE (cont_invalid->get_yang_node());

  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, cont_invalid->validate());

  XMLDocument::uptr_t dom2(mgr->create_document_from_file
                           (file_name.c_str(), false/*validate*/));


  ASSERT_TRUE(dom2.get());
  
  XMLNode* root_node2 = dom2->get_root_node();
  ASSERT_TRUE(root_node2);
  root_node2 = root_node2->get_first_child();
  ASSERT_TRUE(root_node2);

  XMLNode* cont_valid = root_node2->find("container_1-1");
  ASSERT_TRUE (cont_valid);

  EXPECT_TRUE (cont_valid->is_equal(cont_invalid));
  XMLNode *found = cont_invalid->find_enum ("http://riftio.com/ns/core/util/yangtools/tests/conversion", "test-enum", 0);
  EXPECT_EQ (nullptr, found);

  found = cont_valid->find_enum ("http://riftio.com/ns/core/util/yangtools/tests/conversion", "test-enu", 7);
  EXPECT_EQ (nullptr, found);

  found = cont_valid->find_enum ("http://riftio.com/ns/core/util/yangtools/tests/conversion", "test-enum", 6);
  EXPECT_EQ (nullptr, found);

  found = cont_valid->find_enum ("http://riftio.com/ns/core/util/yangtools/tests/conversin", "test-enum", 7);
  EXPECT_EQ (nullptr, found);

  // Test some keyspec and other conversions without loading a ypbc schema
  rw_keyspec_path_t *in = 0, *out = 0;
  EXPECT_EQ (cont_valid->get_specific_keyspec(in,out), RW_YANG_NETCONF_OP_STATUS_FAILED);
  
  // Load a schema, still some tests fail because of invalid data
  model->register_ypbc_schema ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(FlatConversion));
  EXPECT_DEATH (cont_valid->get_specific_keyspec(in,out), "");

  EXPECT_EQ (RW_YANG_NETCONF_OP_STATUS_DATA_MISSING, cont_invalid->to_keyspec(&in));
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, cont_valid->to_keyspec(&in));
  ASSERT_EQ (RW_YANG_NETCONF_OP_STATUS_OK, cont_valid->get_specific_keyspec (in, out));
  RWPB_T_PATHSPEC(FlatConversion_data_Container1_Container11) *spec_ks =
      (RWPB_T_PATHSPEC(FlatConversion_data_Container1_Container11) *)out;
  ASSERT_EQ (&spec_ks->rw_keyspec_path_t, out);

  std::list<XMLNode*> nodes;

  uint32_t save = spec_ks->dompath.path000.element_id.system_ns_id;
  spec_ks->dompath.path000.element_id.system_ns_id = 152;

  EXPECT_DEATH (root_node2->find(&spec_ks->rw_keyspec_path_t, nodes), " ");
  spec_ks->dompath.path000.element_id.system_ns_id = save;
      
}  


void init_comp_msg(RWPB_T_MSG(Company_data_Company)* msg)
{
  msg->contact_info = (RWPB_T_MSG(Company_data_Company_ContactInfo) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_ContactInfo)));
  RWPB_F_MSG_INIT(Company_data_Company_ContactInfo, msg->contact_info);
  msg->contact_info->name = strdup("abc");
  msg->contact_info->address = strdup("bglr");
  msg->contact_info->phone_number = strdup("1234");

  msg->n_employee = 2;
  msg->employee = (RWPB_T_MSG(Company_data_Company_Employee) **)calloc(2, sizeof(void*));
  msg->employee[0] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg->employee[0]);
  msg->employee[0]->id = 100;
  msg->employee[0]->name = strdup("Emp1");
  msg->employee[0]->title = strdup("Title");
  msg->employee[0]->start_date = strdup("1234");
  msg->employee[1] = (RWPB_T_MSG(Company_data_Company_Employee) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Employee)));
  RWPB_F_MSG_INIT(Company_data_Company_Employee,msg->employee[1]);
  msg->employee[1]->id = 200;
  msg->employee[1]->name = strdup("Emp2");
  msg->employee[1]->title = strdup("Title");
  msg->employee[1]->start_date = strdup("1234");

  msg->n_product = 2;
  msg->product = (RWPB_T_MSG(Company_data_Company_Product) **)calloc(2, sizeof(void*));
  msg->product[0] =  (RWPB_T_MSG(Company_data_Company_Product) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Product)));
  RWPB_F_MSG_INIT(Company_data_Company_Product,msg->product[0]);
  msg->product[0]->id = 1000;
  msg->product[0]->name = strdup("nfv");
  msg->product[0]->msrp = strdup("1000");
  msg->product[1] = (RWPB_T_MSG(Company_data_Company_Product) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Product)));
  RWPB_F_MSG_INIT(Company_data_Company_Product,msg->product[1]);
  msg->product[1]->id = 2000;
  msg->product[1]->name = strdup("cloud");
  msg->product[1]->msrp = strdup("2000");

  msg->n_interests = 3;
  msg->interests = (RWPB_T_MSG(Company_data_Company_Interests) **)calloc(2, sizeof(void*));

  msg->interests[0] = (RWPB_T_MSG(Company_data_Company_Interests) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Interests)));
  RWPB_F_MSG_INIT(Company_data_Company_Interests, msg->interests[0]);
  msg->interests[0]->domain = strdup("Reading");
  msg->interests[0]->name = strdup("Fiction");
  msg->interests[0]->id = 100;

  msg->interests[1] = (RWPB_T_MSG(Company_data_Company_Interests) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Interests)));
  RWPB_F_MSG_INIT(Company_data_Company_Interests, msg->interests[1]);
  msg->interests[1]->domain = strdup("Playing");
  msg->interests[1]->name = strdup("Outdoor");
  msg->interests[1]->id = 100;

  msg->interests[2] = (RWPB_T_MSG(Company_data_Company_Interests) *)calloc(1, sizeof(RWPB_T_MSG(Company_data_Company_Interests)));
  RWPB_F_MSG_INIT(Company_data_Company_Interests, msg->interests[2]);
  msg->interests[2]->domain = strdup("Playing");
  msg->interests[2]->name = strdup("Indoor");
  msg->interests[2]->id = 100;

}
 
TEST(RwYangDom, WildCardNodes)
{ 
  // generate keyspec from xpath
  const rw_yang_pb_schema_t* schema =
          reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(Company));

  char xpath[1024];
  strcpy(xpath, "C,/company:company/company:employee");

  auto *keyspec = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr(keyspec); 

  // Create the dom
  RWPB_T_MSG(Company_data_Company) msg1;
  RWPB_F_MSG_INIT(Company_data_Company, &msg1);
  init_comp_msg(&msg1);

  EXPECT_TRUE(msg1.base.descriptor);  

  UniquePtrProtobufCMessageUseBody<>::uptr_t uptr2(&msg1.base);

  rw_yang_netconf_op_status_t ncrs;
 
  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());
  
  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);
  
  YangModule* ydom_top = model->load_module("company");
  EXPECT_TRUE(ydom_top);
  
  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(std::move(mgr->create_document_from_pbcm((ProtobufCMessage *)&msg1, ncrs)));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  
  model->register_ypbc_schema(
  (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(Company));

  std::list<XMLNode *>found;

  XMLNode *root = dom->get_root_node();
  ncrs = root->find(keyspec, found);

  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
  ASSERT_EQ(found.size(), 2);

  for (auto& v: found) {
    rw_keyspec_path_t*  spec_key = nullptr;
    v->to_keyspec(&spec_key);
    
    char ks_str[256] = "";
    rw_keyspec_path_get_print_buffer(spec_key, NULL, schema, ks_str, sizeof(ks_str));
    std::cout << ks_str << std::endl;
    rw_keyspec_path_free(spec_key, NULL);
  }


  // Test Partial wildcarded nodes

  strcpy(xpath, "C,/company:company/company:interests[company:domain='Playing']");
  auto *keyspec1 = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec1);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr1(keyspec1);

  root = dom->get_root_node();
  found.clear();

  ncrs = root->find(keyspec1, found);
  ASSERT_EQ(found.size(), 2);

  for (auto& v: found) {
    rw_keyspec_path_t*  spec_key = nullptr;
    v->to_keyspec(&spec_key);

    char ks_str[256] = "";
    rw_keyspec_path_get_print_buffer(spec_key, NULL, schema, ks_str, sizeof(ks_str));
    std::cout << ks_str << std::endl;
    rw_keyspec_path_free(spec_key, NULL);
  }

  strcpy(xpath, "C,/company:company/company:interests[company:domain='Reading'][company:name='Fiction']");
  auto *keyspec2 = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec2);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr3(keyspec2);

  found.clear();

  ncrs = root->find(keyspec2, found);
  ASSERT_EQ(found.size(), 1);

  for (auto& v: found) {
    rw_keyspec_path_t*  spec_key = nullptr;
    v->to_keyspec(&spec_key);
    
    char ks_str[256] = "";
    rw_keyspec_path_get_print_buffer(spec_key, NULL, schema, ks_str, sizeof(ks_str));
    std::cout << ks_str << std::endl;
    rw_keyspec_path_free(spec_key, NULL);
  }

  strcpy(xpath, "C,/company:company/company:interests[company:id='100']");
  auto *keyspec3 = rw_keyspec_path_from_xpath(schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec3);

  UniquePtrProtobufCMessage<rw_keyspec_path_t>::uptr_t uptr4(keyspec3);

  found.clear();

  ncrs = root->find(keyspec3, found);
  ASSERT_EQ(found.size(), 3);

  for (auto& v: found) {
    rw_keyspec_path_t*  spec_key = nullptr;
    v->to_keyspec(&spec_key);
    
    char ks_str[256] = "";
    rw_keyspec_path_get_print_buffer(spec_key, NULL, schema, ks_str, sizeof(ks_str));
    std::cout << ks_str << std::endl;
    rw_keyspec_path_free(spec_key, NULL);
  }
}

TEST(RwYangDom, RootPBCM)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* fpathd = model->load_module("rw-fpath-d");
  ASSERT_TRUE(fpathd);
  fpathd->mark_imports_explicit();

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, rwbase);

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony1);
  strcpy(colony1.name, "colony1");
  colony1.n_bundle_ether = 2;

  for (unsigned i = 0; i < 2; ++i) {
    sprintf(colony1.bundle_ether[i].name, "%s%d", "be1", i+1);
    colony1.bundle_ether[i].has_descr_string = true;
    strcpy(colony1.bundle_ether[i].descr_string, "bundle ether1");
    colony1.bundle_ether[i].has_open = true;
    colony1.bundle_ether[i].open = true;
    colony1.bundle_ether[i].has_mtu = true;
    colony1.bundle_ether[i].mtu = 1500;
    colony1.bundle_ether[i].has_shutdown = true;
    colony1.bundle_ether[i].shutdown = true;
    colony1.bundle_ether[i].has_lacp = true;
    colony1.bundle_ether[i].lacp.has_system = true;
    colony1.bundle_ether[i].lacp.system.has_priority = true;
    colony1.bundle_ether[i].lacp.system.priority = 1;
  }

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony2);
  strcpy(colony2.name, "colony2");
  colony2.n_bundle_ether = 2;

  for (unsigned i = 0; i < 2; ++i) {
    sprintf(colony2.bundle_ether[i].name, "%s%d", "be2", i+1);
    colony2.bundle_ether[i].has_descr_string = true;
    strcpy(colony2.bundle_ether[i].descr_string, "bundle ether1");
    colony2.bundle_ether[i].has_open = true;
    colony2.bundle_ether[i].open = true;
    colony2.bundle_ether[i].has_mtu = true;
    colony2.bundle_ether[i].mtu = 1500;
    colony2.bundle_ether[i].has_shutdown = true;
    colony2.bundle_ether[i].shutdown = true;
    colony2.bundle_ether[i].has_bundle = true;
    colony2.bundle_ether[i].bundle.has_minimum_active = true;
    colony2.bundle_ether[i].bundle.minimum_active.has_links = true;
    colony2.bundle_ether[i].bundle.minimum_active.links = 100;
    colony2.bundle_ether[i].n_vlan = 1;
    colony2.bundle_ether[i].vlan[0].id = 1234;
    colony2.bundle_ether[i].vlan[0].has_descr_string = true;
    strcpy(colony2.bundle_ether[i].vlan[0].descr_string, "VLAN1");
    colony2.bundle_ether[i].vlan[0].has_open = true;
    colony2.bundle_ether[i].vlan[0].open = true;
  }

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)* arr[2];
  arr[0] = &colony1;
  arr[1] = &colony2;

  rwbase.n_colony = 2;
  rwbase.colony = &arr[0];

  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm (&rwbase.base, status));
  ASSERT_TRUE(dom.get());

  std::string xml_blob = dom->to_string_pretty();
  std::cout << xml_blob << std::endl;

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, rwbase1);

  std::string error_out;
  rw_status_t rs = mgr->xml_to_pb(xml_blob.c_str(), &rwbase1.base, error_out);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  EXPECT_TRUE(protobuf_c_message_is_equal_deep(NULL, &rwbase.base, &rwbase1.base));

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, rcolony1);
  rs = mgr->xml_to_pb(xml_blob.c_str(), &rcolony1.base, error_out);
  EXPECT_EQ(rs, RW_STATUS_FAILURE);
  std::cout << error_out;

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleEther, be);
  error_out = "";
  rs = mgr->xml_to_pb(xml_blob.c_str(), &be.base, error_out);
  EXPECT_EQ(rs, RW_STATUS_FAILURE);
  std::cout << error_out;

  rwbase.n_colony = 1;
  colony1.n_bundle_ether = 1;

  dom.reset(mgr->create_document_from_pbcm (&rwbase.base, status).release());
  ASSERT_TRUE(dom.get());

  xml_blob = dom->to_string();
  ASSERT_TRUE(xml_blob.length());

  rs = mgr->xml_to_pb(xml_blob.c_str(), &rcolony1.base, error_out);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

  rs = mgr->xml_to_pb(xml_blob.c_str(), &be.base, error_out);
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);
}

TEST(RwYangDom, Decimal64)
{
  TEST_DESCRIPTION("Verify the functioning of decimal64");

  RWPB_M_MSG_DECL_INIT(Testy2pTop1_data_Top1c1, msg);
  msg.has_lu8 = true;
  msg.lu8 = 10;
  msg.has_lbool = true;
  msg.lbool = true;
  msg.has_ldec = true;
  msg.ldec = 9223372.01;
  msg.has_lu32 = true;
  msg.lu32 = 567889;
  msg.ls = strdup("Test String");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  rw_status_t rs = model->load_schema_ypbc(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(Testy2pTop1));
  ASSERT_EQ(rs, RW_STATUS_SUCCESS);

  char outxml[4096] = {};
  rs = rw_xml_manager_pb_to_xml(mgr.get(), &msg.base, outxml, sizeof(outxml));
  EXPECT_EQ(rs, RW_STATUS_SUCCESS);

//  std::cout << outxml << std::endl;

  std::string error;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(outxml, error, false));
  ASSERT_TRUE(dom.get());

  XMLNode *m_root = dom->get_root_node();
  ASSERT_TRUE(m_root);

  auto dec_child = m_root->find("ldec");
  ASSERT_TRUE(dec_child);

  std::string xml_val = dec_child->get_text_value();
  std::size_t pos = xml_val.find('.');
  ASSERT_TRUE(pos != std::string::npos);

  std::string prec = xml_val.substr (pos+1);
  EXPECT_EQ(prec.length(), 12);

  std::string width = xml_val.substr (0, pos);
  EXPECT_EQ(width.length(), 7);
}

TEST(RwYangDom, YangPrefix)
{
  TEST_DESCRIPTION("Tests Yang Prefixes");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE (mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE (model);

  YangModule* base_config = model->load_module("test-a-rw-vnf-base-config");
  ASSERT_TRUE (base_config);
  base_config->mark_imports_explicit();

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(TestARwVnfBaseConfig));

  RWPB_M_MSG_DECL_INIT(TestARwVnfBaseConfig_TunnelParams, tp);
  tp.type = TEST_A_RW_VNF_BASE_TYPES_TUNNEL_TYPE_GRE;
  tp.has_gre = true;
  tp.gre.has_keepalive_time = true;
  tp.gre.keepalive_time = 2000;
  tp.gre.has_hold_time = true;
  tp.gre.hold_time = 1600;
  tp.gre.has_ttl = true;
  tp.gre.ttl = 255;

  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm (&tp.base, status));
  ASSERT_TRUE (dom.get());

  std::cout << " DOM = " << std::endl << dom->to_string() << std::endl;

  XMLNode *xmlnode = dom->get_root_node();
  ASSERT_TRUE (xmlnode);

  std::stack<XMLNode*> stack;
  stack.push (xmlnode);

  while ( !stack.empty() ) {

    xmlnode = stack.top();
    stack.pop();

    auto ynode = xmlnode->get_yang_node();
    ASSERT_TRUE (ynode);

    if (!ynode->is_leafy()) {

      auto ypbc_mdesc = ynode->get_ypbc_msgdesc();
      ASSERT_TRUE (ypbc_mdesc);
      EXPECT_STREQ (ypbc_mdesc->module->prefix, xmlnode->get_prefix().c_str());

    } else {

      auto ypbc_mdesc = xmlnode->get_parent()->get_yang_node()->get_ypbc_msgdesc();
      ASSERT_TRUE (ypbc_mdesc);

      for (unsigned i = 0; i < ypbc_mdesc->num_fields; ++i) {

        auto ypbc_fdesc = &ypbc_mdesc->ypbc_flddescs[i];
        if ( !strcmp(ypbc_fdesc->yang_node_name,
                     xmlnode->get_local_name().c_str() )) {
           EXPECT_STREQ (ypbc_fdesc->module->prefix, xmlnode->get_prefix().c_str());
        }
      }
    }

    for (XMLNodeIter xyni = xmlnode->child_begin(); xyni != xmlnode->child_end(); ++xyni ) {
      XMLNode* child_node = &*xyni;
      stack.push (child_node);
    }
  }
}

TEST (RwYangDom, FillDefaultsAll)
{
  TEST_DESCRIPTION("Test filling default values on a list node with empty content");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob =
"\n<data>\n\n"
"  <t:defaults_test xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:name>Test</t:name>\n"
"  </t:defaults_test>\n\n"
"</data>";

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  if (!dom) {
    std::cout << "RwYangDom.FillAllDefaults Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(dom.get());

  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);

  XMLNode* defaults_test = root->get_first_child();
  ASSERT_TRUE(defaults_test);

  defaults_test->fill_defaults();

  std::string exptected_str =
"\n<data>\n\n"
"  <t:defaults_test xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:name>Test</t:name>\n"
"    <t:d_leaf>default_val</t:d_leaf>\n"
"    <t:d_cont1>\n"
"      <t:d_cont2>\n"
"        <t:d_c2>271</t:d_c2>\n"
"      </t:d_cont2>\n"
"      <t:d_c1>d_c1_val</t:d_c1>\n"
"      <t:d_case_d_leaf>314</t:d_case_d_leaf>\n"
"    </t:d_cont1>\n"
"  </t:defaults_test>\n\n\n"
"</data>";

  EXPECT_EQ(exptected_str, dom->to_string_pretty());
}

TEST (RwYangDom, FillDefaultsWithExisting)
{
  TEST_DESCRIPTION("Test defaults filling with existing content");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob =
"\n<data>\n\n"
"  <t:defaults_test xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:name>Test</t:name>\n"
"    <t:d_cont1>\n"
"      <t:d_cont2>\n"
"        <t:d_c2>42</t:d_c2>\n"
"      </t:d_cont2>\n"
"      <t:nd_case_nd_leaf>161</t:nd_case_nd_leaf>\n"
"    </t:d_cont1>\n"
"  </t:defaults_test>\n\n"
"</data>";

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  if (!dom) {
    std::cout << "RwYangDom.FillDefaultsWithExisting Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(dom.get());

  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);

  XMLNode* defaults_test = root->get_first_child();
  ASSERT_TRUE(defaults_test);

  defaults_test->fill_defaults();

  std::string exptected_str =
"\n<data>\n\n"
"  <t:defaults_test xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:name>Test</t:name>\n"
"    <t:d_cont1>\n"
"      <t:d_cont2>\n"
"        <t:d_c2>42</t:d_c2>\n"
"      </t:d_cont2>\n"
"      <t:nd_case_nd_leaf>161</t:nd_case_nd_leaf>\n"
"      <t:d_c1>d_c1_val</t:d_c1>\n"
"      <t:nd_case_d_leaf>nd_case_leaf_val</t:nd_case_d_leaf>\n"
"    </t:d_cont1>\n"
"    <t:d_leaf>default_val</t:d_leaf>\n"
"  </t:defaults_test>\n\n\n"
"</data>";

  EXPECT_EQ(exptected_str, dom->to_string_pretty());
}

TEST (RwYangDom, FillDefaultsRpcInput)
{
  TEST_DESCRIPTION("Test defaults filling in RPC input");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob =
"\n<data>\n\n"
"  <t:defaults_rpc xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:nd_leaf>Test</t:nd_leaf>\n"
"  </t:defaults_rpc>\n\n"
"</data>";

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  if (!dom) {
    std::cout << "RwYangDom.FillDefaultsRpcInput Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(dom.get());

  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);

  XMLNode* defaults_rpc = root->get_first_element();
  ASSERT_TRUE(defaults_rpc);

  defaults_rpc->fill_rpc_input_with_defaults();

  std::string exptected_str =
"\n<data>\n\n"
"  <t:defaults_rpc xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:nd_leaf>Test</t:nd_leaf>\n"
"    <t:d_leaf>d_leaf_val</t:d_leaf>\n"
"    <t:d_cont>\n"
"      <t:d_cont_d_leaf>314</t:d_cont_d_leaf>\n"
"    </t:d_cont>\n"
"  </t:defaults_rpc>\n\n\n"
"</data>";

  EXPECT_EQ(exptected_str, dom->to_string_pretty());
}

TEST (RwYangDom, FillDefaultsRpcOutput)
{
  TEST_DESCRIPTION("Test defaults filling in RPC output");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("test-ydom-top");
  EXPECT_TRUE(ydom_top);

  std::string xmlblob =
"\n<data>\n\n"
"  <t:defaults_rpc xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:d_list>\n"
"      <t:name>RpcOutput1</t:name>\n"
"    </t:d_list>\n"
"    <t:d_list>\n"
"      <t:name>RpcOutput2</t:name>\n"
"      <t:nd_leaf>42</t:nd_leaf>\n"
"    </t:d_list>\n"
"  </t:defaults_rpc>\n\n"
"</data>";

  std::string error_out;
  XMLDocument::uptr_t dom(mgr->create_document_from_string(xmlblob.c_str(), error_out, false/*validate*/));
  if (!dom) {
    std::cout << "RwYangDom.FillDefaultsRpcOutput Error: " << error_out << std::endl;
  }
  ASSERT_TRUE(dom.get());

  XMLNode* root = dom->get_root_node();
  ASSERT_TRUE(root);

  XMLNode* defaults_rpc = root->get_first_element();
  ASSERT_TRUE(defaults_rpc);

  defaults_rpc->fill_rpc_output_with_defaults();

  std::string exptected_str =
"\n<data>\n\n"
"  <t:defaults_rpc xmlns:t=\"http://riftio.com/ns/yangtools/test-ydom-top\">\n"
"    <t:d_list>\n"
"      <t:name>RpcOutput1</t:name>\n"
"      <t:d_leaf>108</t:d_leaf></t:d_list>\n"
"    <t:d_list>\n"
"      <t:name>RpcOutput2</t:name>\n"
"      <t:nd_leaf>42</t:nd_leaf>\n"
"      <t:d_leaf>108</t:d_leaf></t:d_list>\n"
"  </t:defaults_rpc>\n\n"
"</data>";

  EXPECT_EQ(exptected_str, dom->to_string_pretty());
}
