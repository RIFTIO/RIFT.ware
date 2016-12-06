
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
 * @file confd_test.cpp
 * @author Tom Seidenberg
 * @date 08/19/2014
 * @brief Test program for CONFD-related yang support
 */

#include "rw_ut_confd.hpp"
#include "rwut.h"
#include "yangncx.hpp"
#include "rw_confd_annotate.hpp"

#include <boost/scope_exit.hpp>

#include <confd_lib.h>
#include <confd_cdb.h>
#include <confd_dp.h>

#include <iostream>
#include "rw_tree_iter.h"
#include "rw_xml.h"
#include "confd_xml.h"
#include "flat-conversion.confd.h"
#include "flat-conversion.pb-c.h"
#include "yangtest_common.hpp"

static const int CONFD_TEST_CONNECT_MAX_TRIES = 3;
using namespace rw_yang;

class XMLNoListCopier
  :public XMLCopier
{

  public:
    /// constructor - node is owned by the caller and expected to be around while this object is alive
    XMLNoListCopier(XMLNode *node):XMLCopier(node) {}

    /// trivial  destructor
    virtual ~XMLNoListCopier() {}

     // Cannot copy
     XMLNoListCopier(const XMLCopier&) = delete;
     XMLNoListCopier& operator=(const XMLCopier&) = delete;

  public:
  virtual rw_xml_next_action_t visit(XMLNode* node, XMLNode** path, int32_t level)
  {
    if  (node == nullptr) {
      return RW_XML_ACTION_TERMINATE;
    }

    YangNode *yn = node->get_yang_node();
    
    if (nullptr == yn) {
      return RW_XML_ACTION_NEXT;
    }

    if (yn->get_stmt_type() == RW_YANG_STMT_TYPE_LIST) {
      return RW_XML_ACTION_NEXT_SIBLING;      
    }

    RW_ASSERT(to_node_);
    

    
    // This is the root node then this is the first time call
    if (level == 0) {
      RW_ASSERT(level_ == -1);

      std::string text = node->get_text_value();

      if (node->get_yang_node()->is_leafy() &&
          node->get_yang_node()->get_type()->get_leaf_type() != RW_YANG_LEAF_TYPE_EMPTY && 
          text.find_first_not_of("\t\n\v\f\r ") != std::string::npos) {
        if (text.find_first_of ("http")) {
          RW_CRASH();
        }
        
        curr_node_ = to_node_->add_child(node->get_yang_node(), node->get_text_value().c_str());
      } else {
        curr_node_ = to_node_->add_child(node->get_yang_node());
      }
      path_[++level_].first = node;
      path_[level_].second = curr_node_;
      
      return RW_XML_ACTION_NEXT;
    }
    
    XMLNode *from_parent = path[level-1];
    
    RW_ASSERT(from_parent);
    
    // Walk up our path and find the corresponding parent in teh vistor's tree
    
    while(level_ >= 0)  {
      if (path_[level_].first == from_parent) {
        // Found a common parent - break  and add the new node
        XMLNode *to_parent = static_cast<XMLNode*>(path_[level_].second);

        std::string text = node->get_text_value();

        if (text.find_first_not_of("\t\n\v\f\r ") != std::string::npos) {
          // A bit of a hack to ensure that white spaces are not inserted to the
          // XML. This is a problem for UT that read from files 
          
          curr_node_ = to_parent->add_child(node->get_yang_node(), node->get_text_value().c_str());
        } else {
          curr_node_ = to_parent->add_child(node->get_yang_node());
        }
        
        
        // Replace the node in the current path with the new node.
        path_[++level_].first = node;
        path_[level_].second  = curr_node_;
        break;
      }
      level_--;
    }
    RW_ASSERT(level_ > 0);
    
    return RW_XML_ACTION_NEXT;
  }
};


TEST(ConfdUtHarness, CreateDestroy)
{
  TEST_DESCRIPTION("Creates and destroy a ConfdUnittestHarness");
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_manual("harness_test1");
  ASSERT_TRUE(harness.get());
}

TEST(ConfdUtHarness, AutoStart)
{
  TEST_DESCRIPTION("Create and auto-start a ConfdUnittestHarness");
  YangModel::ptr_t ymodel(YangModelNcx::create_model());
  ASSERT_TRUE(ymodel.get());

  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("harness_test2", ymodel.get());
  ASSERT_TRUE(harness.get());

  EXPECT_TRUE(harness->is_running());
  harness->stop();
  EXPECT_FALSE(harness->is_running());
}

TEST(ConfdUtTranslation, DomToConfd)
{
  TEST_DESCRIPTION("Translate a RWXML DOM to Confd and back");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("base-conversion");
  EXPECT_TRUE(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);

  YangNode *container_1 = ydom_top->get_first_node();
  
  std::ifstream fp;
  
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("ConfdDOM", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {

    bool ret = model->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                          &model->adt_confd_ns_);
    RW_ASSERT(ret);
    
    ret =  model->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                      &model->adt_confd_name_);
    RW_ASSERT(ret);

    ASSERT_EQ (CONFD_OK, confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa)) );

    namespace_map_t ns_map;
    struct confd_nsinfo *listp;
    
    uint32_t ns_count = confd_get_nslist(&listp);
    
    RW_ASSERT (ns_count); // for now
    
    for (uint32_t i = 0; i < ns_count; i++) {
      ns_map[listp[i].uri] = listp[i].hash;
    }
    
    rw_confd_annotate_ynodes (model, ns_map, confd_str2hash,
                              YANGMODEL_ANNOTATION_CONFD_NS,
                              YANGMODEL_ANNOTATION_CONFD_NAME );


  // Create a DOM
  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/confd.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                              (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());

  XMLNode* root_node = dom->get_root_node();
  ASSERT_TRUE (root_node);

  root_node = root_node->get_first_child();

  // The DOMs should be similar - EXCEPT that the new one will not have
  // the list children.. And only YANG nodes assocaited nodes will be present

  rw_yang::XMLDocument::uptr_t filtered = 
      std::move(mgr->create_document(model->get_root_node()));

  XMLNode* filter_root = filtered->get_root_node();

  XMLNoListCopier copier (filter_root);
  XMLWalkerInOrder walker (root_node);

  walker.walk(&copier);

  // associated confd structures
  struct confd_cs_node *cs_node = confd_find_cs_root(flat_conversion__ns);

  // Translate the DOM to confd
  
  ConfdTLVBuilder builder(cs_node);
  XMLTraverser traverser(&builder,root_node);

  traverser.traverse();

  ASSERT_TRUE (builder.length());

  size_t length = builder.length();
  
  confd_tag_value_t
      *array =  (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * length);

  builder.copy_and_destroy_tag_array(array);

  // Translate the confd to DOM
  rw_yang::XMLDocument::uptr_t converted = 
      std::move(mgr->create_document(model->get_root_node()));

  XMLNode *root = converted->get_root_node();



  XMLNode *build_node = root->add_child (container_1);
  XMLBuilder xml_builder(build_node);
  
  ConfdTLVTraverser tlv_traverser(&xml_builder, array, length );
  tlv_traverser.traverse();

  for (size_t i = 0; i < length; i++) {
    confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
  }
  RW_FREE (array);
    
  std::string orig = filter_root->to_string();
  std::string copied = root->to_string();

  ASSERT_STREQ (orig.c_str(), copied.c_str());
  }

  harness->stop();
}


TEST(ConfdUtTranslation, FindHkeyPath)
{
  TEST_DESCRIPTION("Translate a RWXML DOM to Confd and back");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("base-conversion");
  EXPECT_TRUE(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);

  std::ifstream fp;
  
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("harness_test4", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {

    bool ret = model->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                          &model->adt_confd_ns_);
    RW_ASSERT(ret);
    
    ret =  model->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                      &model->adt_confd_name_);
    RW_ASSERT(ret);

    ASSERT_EQ (CONFD_OK, confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa)) );

    namespace_map_t ns_map;
    struct confd_nsinfo *listp;
    
    uint32_t ns_count = confd_get_nslist(&listp);
    
    RW_ASSERT (ns_count); // for now
    
    for (uint32_t i = 0; i < ns_count; i++) {
      ns_map[listp[i].uri] = listp[i].hash;
    }

    rw_confd_annotate_ynodes (model, ns_map, confd_str2hash,
                              YANGMODEL_ANNOTATION_CONFD_NS,
                              YANGMODEL_ANNOTATION_CONFD_NAME );
    
    
    // Create a DOM
  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/confd.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                              (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());

  XMLNode* root_node = dom->get_root_node();
  ASSERT_TRUE (root_node);

  // Build a confd hkeypath
  size_t length = 4;
  
  confd_hkeypath_t path;

  memset (&path, 0, sizeof (confd_hkeypath_t));
  
  path.len = length;
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0],flat_conversion_container_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_container_1_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0],flat_conversion_list_10x2e1_2, flat_conversion__ns);
  length --;

  ASSERT_FALSE (length);
  CONFD_SET_INT32 (&path.v[length][0], 11211121);
  path.v[length][1].type = C_NOEXISTS;

  XMLNode *node = get_node_by_hkeypath (root_node, &path, 0);

  ASSERT_TRUE (node);

  // Look for vodka - path of 0
  memset (&path, 0, sizeof (confd_hkeypath_t));

  confd_value_t choice[10], drink;
  memset (choice, 0, sizeof (confd_value_t) * 10);
  memset (&drink, 0, sizeof (confd_value_t));
  
  CONFD_SET_XMLTAG (&choice[0], flat_conversion_drink, flat_conversion__ns);
  choice[1].type = C_NOEXISTS;

  rw_status_t status = get_confd_case (node, &path, choice, &drink);

  ASSERT_EQ (RW_STATUS_SUCCESS, status);
  ASSERT_EQ (CONFD_GET_XMLTAG(&drink), flat_conversion_vodka);
  
  memset (&path, 0, sizeof (confd_hkeypath_t));

  length = 2;
  path.len = length;
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_container_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0],flat_conversion_empty_1_3, flat_conversion__ns);

  node = get_node_by_hkeypath (root_node, &path, 0);

  ASSERT_TRUE (node);

  // Multi key list - success
    // Build a confd hkeypath
  length = 3;
  
  memset (&path, 0, sizeof (confd_hkeypath_t));
  
  path.len = length;
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_container_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_two_keys, flat_conversion__ns);
  length --;

  ASSERT_FALSE (length);
  
  const char *entry = "entry";
  const char *entree = "entree";

  CONFD_SET_ENUM_VALUE (&path.v[length][0], 1);
  CONFD_SET_CBUF (&path.v[length][1], entry, strlen(entry));

  path.v[length][2].type = C_NOEXISTS;

  node = get_node_by_hkeypath (root_node, &path, 0);
  ASSERT_TRUE (node);

  struct confd_cs_node *cs_node = confd_find_cs_node(&path, 2);
  ASSERT_TRUE (cs_node);
  // Test getting keys and next entry
  confd_value_t keys[5];
  int key_count = 0;
  XMLNode *next_node;
  
  ASSERT_EQ (get_current_key_and_next_node (node, cs_node, &next_node, keys, &key_count), RW_STATUS_SUCCESS);
  EXPECT_EQ (2, key_count);
  ASSERT_NE (next_node, nullptr);
  ASSERT_EQ (get_current_key_and_next_node (next_node, cs_node, &next_node, keys, &key_count), RW_STATUS_SUCCESS);
  EXPECT_EQ (2, key_count);
  EXPECT_EQ (next_node, nullptr);
  
  
  
  CONFD_SET_CBUF (&path.v[length][1], entree, strlen(entree));
  node = get_node_by_hkeypath (root_node, &path, 0);
  ASSERT_FALSE (node);

  CONFD_SET_ENUM_VALUE (&path.v[length][0], 7);
  node = get_node_by_hkeypath (root_node, &path, 0);
  ASSERT_TRUE (node);

  // Traverse the largest path
  rw_yang::XMLDocument::uptr_t req =
      std::move(mgr->create_document(mgr->get_yang_model()->get_root_node()));
  XMLNode *root = req->get_root_node();
  XMLBuilder build(root);

  ConfdKeypathTraverser traver(&build, &path);
  traver.traverse();

  
  CONFD_SET_CBUF (&path.v[length][1], entry, strlen(entry));
  node = get_node_by_hkeypath (root_node, &path, 0);
  ASSERT_FALSE (node);

  // associated confd structures
  cs_node = confd_find_cs_root(flat_conversion__ns);

  // Translate the DOM to confd
  
  ConfdTLVBuilder builder(cs_node);
  XMLTraverser traverser(&builder,root_node->get_first_element());

  traverser.traverse();

  ASSERT_TRUE (builder.length());

  length = builder.length();
  
  confd_tag_value_t
      *array =  (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * length);
  ASSERT_NE (nullptr, array);
  }

  
  harness->stop();
}


TEST(ConfdUtHarness, Connect)
{
  TEST_DESCRIPTION("Create and auto-start a ConfdUnittestHarness");
  YangModel::ptr_t ymodel(YangModelNcx::create_model());
  ASSERT_TRUE(ymodel.get());

  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("harness_test3", ymodel.get());
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {
    int cdb_fd = harness->alloc_confd_socket();
    
    ASSERT_GT(cdb_fd, -1);

    BOOST_SCOPE_EXIT(&cdb_fd) {
      if (-1 != cdb_fd) {
        cdb_close(cdb_fd);
      }
    } BOOST_SCOPE_EXIT_END

    int st;
    for (int i = 0; i < CONFD_TEST_CONNECT_MAX_TRIES; i++) {
      st = cdb_connect(cdb_fd, CDB_DATA_SOCKET, (struct sockaddr *)&sa, sizeof(sa));
      if (st == CONFD_OK) {
        break;
      }
      sleep (1);
    }
    
    EXPECT_EQ(CONFD_OK, st);

    st = cdb_wait_start(cdb_fd);
    EXPECT_EQ(CONFD_OK, st);
  }

  {
    int dp_fd = harness->alloc_confd_socket();
    ASSERT_GT(dp_fd, -1);

    BOOST_SCOPE_EXIT(&dp_fd) {
      if (-1 != dp_fd) {
        close(dp_fd);
      }
    } BOOST_SCOPE_EXIT_END

    int st = confd_connect(ctx, dp_fd, CONTROL_SOCKET, (struct sockaddr *)&sa, sizeof(sa));
    EXPECT_EQ(CONFD_OK,st);
  }

  harness->stop();
}


TEST(ConfdUtTranslation, ConfdNewBuilder)
{
  TEST_DESCRIPTION("Translate a RWXML DOM to Confd and back without list filter and new builder");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("base-conversion");
  EXPECT_TRUE(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(FlatConversion));

  std::ifstream fp;
  
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("ConfdDOM", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {

    bool ret = model->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                          &model->adt_confd_ns_);
    RW_ASSERT(ret);
    
    ret =  model->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                      &model->adt_confd_name_);
    RW_ASSERT(ret);

    ASSERT_EQ (CONFD_OK, confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa)) );

    namespace_map_t ns_map;
    struct confd_nsinfo *listp;
    
    uint32_t ns_count = confd_get_nslist(&listp);
    
    RW_ASSERT (ns_count); // for now
    
    for (uint32_t i = 0; i < ns_count; i++) {
      ns_map[listp[i].uri] = listp[i].hash;
    }
    
    rw_confd_annotate_ynodes (model, ns_map, confd_str2hash,
                              YANGMODEL_ANNOTATION_CONFD_NS,
                              YANGMODEL_ANNOTATION_CONFD_NAME );


  // Create a DOM
  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/confd.xml");

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                              (file_name.c_str(), false/*validate*/));

  ASSERT_TRUE(dom.get());

  XMLNode* root_node = dom->get_root_node();
  ASSERT_TRUE (root_node);

  root_node = root_node->get_first_child();
  // associated confd structures
  struct confd_cs_node *cs_node = confd_find_cs_root(flat_conversion__ns);

  // Translate the DOM to confd
  
  ConfdTLVBuilder builder(cs_node,true);
  XMLTraverser traverser(&builder,root_node);

   traverser.traverse();


  ASSERT_TRUE (builder.length());

  size_t length = builder.length();

  // Add 3 more  - to add a delete keyspec
  confd_tag_value_t
      *array =  (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * (length + 3));

  builder.copy_and_destroy_tag_array(array);
  

  // Array is of the following form for the test
  // The nodes with C++ comments added to test deletion
  /*  1      C_XMLBEGIN  "container_1-1"         */
  /*  2          C_BUF "leaf-1_1.1"              */
  /*  3          C_XMLBEGIN "list-1.1_2"         */
  /*  4      	C_INT32 "int_1.1.2_1"              */
  /*  5          C_XMLEND "list-1.1_2"           */
  /*  6          C_XMLBEGIN "list-1.1_2"         */
  /*  7              C_XMLTAG "grey-goose"       */
  /*  8              C_INT32 "int_1.1.2_1"       */
  /*  9          C_XMLEND  "list-1.1_2"          */
  // 10          C_XMLBEGINDEL "list-1.1_2"         */
  // 11              C_INT32 "int_1.1.2_1"       */
  // 12          C_XMLEND  "list-1.1_2"          */
  /* 13      C_XMLEND "container_1-1"            */
  /* 14      C_LIST "leaf.list.1.2"              */
  /* 15      C_XMLBEGIN "two-keys"               */
  /* 16          C_ENUM_VALUE "prim-enum"        */
  /* 17          C_BUF "sec-string"              */
  /* 18      C_XMLEND "two-keys"                 */
  /* 19      C_XMLBEGIN "two-keys"               */
  /* 20          C_ENUM_VALUE "prim-enum"        */
  /* 21          C_BUF "sec-string"              */
  /* 22      C_XMLEND "two-keys"                 */
  /* 23      C_XMLTAG "empty-1_3"                */
  /* 24      C_LIST "enum_1-4"                   */
  /* 25      C_ENUM_VALUE "enum_1.5"             */

  // add 3 lines in the middle
  memmove (&array[12], &array[9], sizeof (confd_tag_value_t) * 13);
  memcpy (&array[9], &array[2],  sizeof (confd_tag_value_t) * 3);
  array[9].v.type = C_XMLBEGINDEL;
  array[10].v.val.i32 = 0xdeadbeef;
  
  length += 3;
    // Translate the confd to DOM
  rw_yang::XMLDocument::uptr_t converted = 
      std::move(mgr->create_document(model->get_root_node()));

  XMLNode *root = converted->get_root_node();

  YangNode *container_1 = ydom_top->get_first_node();
  XMLNode *build_node = root->add_child (container_1);
  XMLBuilder xml_builder(build_node);
  
  ConfdTLVTraverser tlv_traverser(&xml_builder, array, length );

  
  tlv_traverser.traverse();
  std::string orig = dom->get_root_node()->get_first_element()->to_string();
  std::string copy = xml_builder.current_node_->to_string();

  ASSERT_EQ (1, xml_builder.delete_ks_.size());

  for (size_t i = 0; i < length; i++) {
    confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
  }

  RW_FREE (array);

  }
}

TEST(ConfdUtTranslation, KeylessLists)
{
  TEST_DESCRIPTION("Translate a RWXML DOM with keyless lists to confd");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("base-conversion");
  EXPECT_TRUE(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);
  
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("keyless-lists", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {

    bool ret = model->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                          &model->adt_confd_ns_);
    RW_ASSERT(ret);
    
    ret =  model->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                      &model->adt_confd_name_);
    RW_ASSERT(ret);

    ASSERT_EQ (CONFD_OK, confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa)) );

    namespace_map_t ns_map;
    struct confd_nsinfo *listp;
    
    uint32_t ns_count = confd_get_nslist(&listp);
    
    RW_ASSERT (ns_count); // for now
    
    for (uint32_t i = 0; i < ns_count; i++) {
      ns_map[listp[i].uri] = listp[i].hash;
    }

    rw_confd_annotate_ynodes (model, ns_map, confd_str2hash,
                              YANGMODEL_ANNOTATION_CONFD_NS,
                              YANGMODEL_ANNOTATION_CONFD_NAME );
    
    
    // Create a DOM
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
      XMLDocument::uptr_t dom(mgr->create_document());
      ASSERT_TRUE(dom.get());

      XMLNode* root_node = dom->get_root_node();
      ASSERT_TRUE (root_node);

      XMLNode *first_elt = root_node->add_child("flat", 0, "http://riftio.com/ns/core/util/yangtools/tests/conversion");
      ASSERT_TRUE (first_elt);
      ncrs = first_elt->merge ((ProtobufCMessage *)&flat);
      ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);
      ASSERT_EQ(4, first_elt->get_children()->length());
      
      confd_hkeypath_t path;
      memset (&path, 0, sizeof (confd_hkeypath_t));
      
      // Build a confd hkeypath
      size_t length = 3;
      path.len = length;
      length --;

      CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_flat, flat_conversion__ns);
      length --;

      CONFD_SET_XMLTAG (&path.v[length][0], flat_conversion_keyless, flat_conversion__ns);      
      length--;

      first_elt = first_elt->get_first_child();
      
      ASSERT_FALSE (length);
      CONFD_SET_INT64 (&path.v[length][0] , (int64_t) first_elt);
      path.v[length][1].type = C_NOEXISTS;
      
      XMLNode *node = get_node_by_hkeypath (root_node, &path, 0);
      ASSERT_TRUE (node);
      ASSERT_EQ (first_elt, node);
      
      struct confd_cs_node *cs_node = confd_find_cs_node(&path, 2);
      ASSERT_TRUE (cs_node);

      confd_value_t keys[5];
      int key_count = 0;
      XMLNode *next_node;
      
      ASSERT_EQ (get_current_key_and_next_node (node, cs_node, &next_node, keys, &key_count), RW_STATUS_SUCCESS);
      EXPECT_EQ (1, key_count);
      EXPECT_EQ ((int64_t) first_elt, keys[0].val.i64);
      // Get object with the key set to the pointer
      
      
      ASSERT_NE (next_node, nullptr);
      ASSERT_EQ (get_current_key_and_next_node (next_node, cs_node, &next_node, keys, &key_count), RW_STATUS_SUCCESS);
      EXPECT_EQ (1, key_count);
  }
}

extern void create_dynamic_composite_d(const rw_yang_pb_schema_t** dynamic);

TEST(ConfdUtTranslation, DynamicSchema)
{
  TEST_DESCRIPTION("Verify Dynamic Schema loading..");

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* comp = model->load_module("rw-composite-d");
  EXPECT_TRUE(comp);
  comp->mark_imports_explicit();
  
  ConfdUnittestHarness::uptr_t harness
    = ConfdUnittestHarness::create_harness_autostart("Dynamic", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  EXPECT_TRUE(harness->wait_till_phase2(&sa));

  {
    ASSERT_EQ (CONFD_OK, confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa)) );

    namespace_map_t ns_map;
    struct confd_nsinfo *listp;
    
    uint32_t ns_count = confd_get_nslist(&listp);
    
    RW_ASSERT (ns_count); // for now

    for (uint32_t i = 0; i < ns_count; i++) {
      ns_map[listp[i].uri] = listp[i].hash;
    }
    
    const rw_yang_pb_schema_t *dynamic = nullptr;
    create_dynamic_composite_d(&dynamic);

    ASSERT_TRUE(dynamic);

    YangModelNcx* model1 = YangModelNcx::create_model();
    YangModel::ptr_t p(model1);
    ASSERT_TRUE(model1);

    bool ret = model1->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                           &model1->adt_confd_ns_);
    RW_ASSERT(ret);
    
    ret =  model1->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                       &model1->adt_confd_name_);
    RW_ASSERT(ret);

    rw_status_t s = model1->load_schema_ypbc(dynamic);
    EXPECT_EQ(s, RW_STATUS_SUCCESS);

    s = model1->register_ypbc_schema(dynamic);
    EXPECT_EQ(s, RW_STATUS_SUCCESS);

    s = rw_confd_annotate_ynodes (model1, ns_map, confd_str2hash,
                                  YANGMODEL_ANNOTATION_CONFD_NS,
                                  YANGMODEL_ANNOTATION_CONFD_NAME );

    EXPECT_EQ(s, RW_STATUS_SUCCESS);
  }

  harness->stop();
}

