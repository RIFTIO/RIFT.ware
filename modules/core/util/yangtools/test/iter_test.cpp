
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
 * @file iter_test.cpp
 * @author Vinod Kamalaraj
 * @date 03/18/2015
 * @brief Test program for Generic tree iterators
 */

#include "rw_tree_iter.h"
#include "flat-conversion.confd.h"
#include "confd_xml.h"
#include "yangtest_common.hpp"
#include "rw_confd_annotate.hpp"
#include <regex>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

namespace rw_yang {


static ProtobufCInstance* pinstance = nullptr;

/*!
 * This function searchers for all nodes in a haystack for children
 * that match the description of needle. For a listy haystack and a
 * needle with not all keys specified, multiple matches could occur.
 *
 * @param[in] haystack An iterator in which the search has to be conducted
 * in. The children of haystack is searched for a match to needle.
 * @param[in] needle The key to search for. The key could be the values of the
 * key to a list, some values to a list or a message in itself. A tree needle
 * is used when the intersection of two trees is of interest.
 * @param[in] all_leafs When two trees are compared, this flag dictates whether
 * a match implies all the leaf values present in needle are also present in
 * haystack.
 * @param[in] all_keys specifies whether the needle needs to have all the keys
 * for a list for the function to be successful. The intent of this function is
 * not as a standalone function, but for use in finding a node in a tree for a
 * path, or for merging two trees. In the merge case, both the needle and the
 * haystack need to have all keys present for the find to be meaningful.
 * @param[in] found A functor that is called each time a match is found. Having
 * a functor makes it simpler to use this function in both a merge type
 * situation and a deep find situation.
 * @param[in] not_found A functor that is called when there is no needle in the
 * haystack. This function is called only if needle is valid when the all_keys
 * flag is set.
 *
 * @return RW_STATUS_SUCCESS, when there are no errors found in the objects, ie,
 * the objects are valid according to the flags that are passed to this function.
 * This does NOT imply that matches were found - the functors have to handle
 * that responsibility. RW_STATUS_FAILURE is returned when errors or
 * inconsistencies are found in the data objects.
 */
template <class HayStack, class Needle, class FoundAction, class NotFoundAction>
rw_tree_walker_status_t find_child_iter(const HayStack *haystack,
                                        const Needle *needle,
                                        bool all_leafs,
                                        bool all_keys,
                                        FoundAction& found,
                                        NotFoundAction& not_found)
{
  RW_ASSERT_NOT_REACHED();
}

  
template <class FoundAction, class NotFoundAction>
rw_tree_walker_status_t find_child_iter (RwPbcmTreeIterator *parent,
                                         RwSchemaTreeIterator *key_spec,
                                         bool all_leafs,
                                         bool all_keys,
                                         FoundAction& found,
                                         NotFoundAction& not_found)
{
  rw_ylib_data_t value;
  rw_status_t rs = key_spec->get_value(&value);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  if (value.type != RW_YLIB_DATA_TYPE_KS_PATH) {
    RW_CRASH();
  }

  if (all_keys) {
    // Add validation for keys being present
    RW_CRASH();
  }

  RwPbcmTreeIterator child = *parent;
  rs = child.move_to_child(value.path.tag);
  if (RW_STATUS_SUCCESS != rs) {
    return not_found (parent, key_spec, all_leafs, all_keys);
  }

  // Check if we are at a listy point.
  rw_keyspec_entry_t* path;
  
  rs = key_spec->get_value(path);

  if (!rw_keyspec_entry_is_listy (path)) {
    return found (parent, key_spec, all_leafs, all_keys);
  }
  

  bool found_entry = false;

  // Walk all the elements of this list. more than one match is possible
  // esp if all keys are not specified.

  // ATTN: if all keys are specified, maybe this find can be short circuited
  
  for (;rs == RW_STATUS_SUCCESS; rs = child.move_to_next_list_entry()) {
    RwSchemaTreeIterator keys = *key_spec;
    
    bool matches = true;
    rw_status_t ks_st = keys.move_to_first_child();
    for (; RW_STATUS_SUCCESS == ks_st; ks_st = keys.move_to_next_sibling()) {
      
      rw_status_t kv_st = keys.get_value (&value);

      if (RW_STATUS_SUCCESS != kv_st) {
        return RW_TREE_WALKER_FAILURE;
      }
      
      if (RW_YLIB_DATA_TYPE_KS_PATH == value.type) {
        // going deeper to the next node, not yet
        continue;
      }

      // type is key value - check if there is equivalent value in proto
      ProtobufCMessage *child_msg;
      kv_st = child.get_value (child_msg);

      if (RW_STATUS_SUCCESS != kv_st) {
        return RW_TREE_WALKER_FAILURE;
      }

      if (protobuf_c_message_has_field_with_value
          (child_msg, value.rw_pb.fdesc->name, &value.rw_pb)) {
        continue;
      } else {
        matches = false;
        break;
      }
    }

    if (matches) {
      // cannot return yet - could be more matches?
      
      // Do not call the not_found method 
      found_entry = true;

      // Stop if we encounter errors
      if (found (parent, key_spec, all_leafs, all_keys) == RW_TREE_WALKER_FAILURE) {
        return RW_TREE_WALKER_FAILURE;
      }
    }
  }

  if (!found_entry) {
    return (not_found (parent, key_spec, all_leafs, all_keys));
  }

  return RW_TREE_WALKER_SUCCESS;
}

template <class HayStack, class Needle>
class Logger {
 private:

 public:
  rw_tree_walker_status_t operator() (HayStack *haystack,
                                      Needle *needle,
                                      bool all_leafs,
                                      bool all_keys)
  {
    UNUSED (haystack);
    UNUSED (needle);
    UNUSED (all_keys);
    UNUSED (all_leafs);

    return RW_TREE_WALKER_SUCCESS;
  }    
};

template <class HayStack, class Needle>
class Collector
{
 public:
  std::list <HayStack *> matches_;
  Logger<HayStack, Needle> logger_;
  
 public:
  Collector (Logger<HayStack, Needle> logger):
      logger_ (logger) {};
  
  rw_tree_walker_status_t operator() (HayStack *haystack,
                                      Needle *needle,
                                      bool all_leafs,
                                      bool all_keys)
  {
    Needle next = *needle;
    rw_status_t rs = next.move_to_first_child();
    rw_tree_walker_status_t rw = RW_TREE_WALKER_SUCCESS;
    bool non_leaf_children = false;
    
    for (;RW_STATUS_SUCCESS == rs; rs = next.move_to_next_sibling()) {
      if (next.is_leafy()) {
        // This level is handled earlier
        continue;
      }
      non_leaf_children = true;
      rw = find_child_iter (haystack, &next, all_leafs, all_keys, *this, logger_);
      if (rw != RW_TREE_WALKER_SUCCESS) {
        return rw;
      }
      
      return rw;
    }
    if (!non_leaf_children) {
      matches_.push_back (haystack);
    }
    
    return rw;
  }
 
};

  
template <class Haystack,class Needle, class Collector, class Logger>
rw_tree_walker_status_t Find (Haystack *haystack, Needle *needle,
                              Collector& collector,
                              Logger& logger)
{
  
  return find_child_iter (haystack, needle, false, false, collector, logger);
}

}



using namespace rw_yang;
namespace RE = boost;


void rw_test_build_dom_from_file (const char *filename,
                             XMLDocument::uptr_t &dom)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  RW_ASSERT(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  RW_ASSERT(test_ncx);


  if (!strcmp (filename, "confd.xml")) {
    YangModule* ydom_top = model->load_module("base-conversion");
    RW_ASSERT(ydom_top);
    ydom_top = model->load_module("flat-conversion");
    RW_ASSERT(ydom_top);
    model->register_ypbc_schema(
        (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(FlatConversion));

  }
  
  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/") +
      filename;

  dom = std::move (mgr->create_document_from_file
                   (file_name.c_str(), false/*validate*/));
  
}

rw_status_t rw_test_build_pb_flat_from_file (const char *filename,
                                             ProtobufCMessage **msg)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  RW_ASSERT(model);

  YangModule* test_ncx = model->load_module("flat-conversion");
  RW_ASSERT(test_ncx);


  if (!strcmp (filename, "confd.xml")) {
    YangModule* ydom_top = model->load_module("base-conversion");
    RW_ASSERT(ydom_top);
    ydom_top = model->load_module("flat-conversion");
    RW_ASSERT(ydom_top);
    model->register_ypbc_schema(
        (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(FlatConversion));

  }
  
  *msg = nullptr;
  std::ifstream fp;


  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/") +
      filename;

  XMLDocument::uptr_t dom(mgr->create_document_from_file
                          (file_name.c_str(), false/*validate*/));

  rw_yang_netconf_op_status_t ncrs =
      dom->get_root_node()->get_first_element()->to_pbcm(msg);
  if (RW_YANG_NETCONF_OP_STATUS_OK == ncrs) {
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}


rw_status_t rw_test_build_pb_bumpy_from_file (const char *filename,
                                              ProtobufCMessage **msg)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  RW_ASSERT(model);

  *msg = nullptr;
  std::ifstream fp;

  std::string file_name = get_rift_root() +
      std::string("/modules/core/util/yangtools/test/") +
      filename;


  if (!strcmp (filename, "confd.xml")) {
    YangModule* ydom_top = model->load_module("base-conversion");
    RW_ASSERT(ydom_top);
    ydom_top = model->load_module("bumpy-conversion");
    RW_ASSERT(ydom_top);
    model->register_ypbc_schema(
        (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(BumpyConversion));


    // Change the namespace from conversion to bconversion for the xml file
    // so that the bumpy conversion will work.

    std::FILE *fp = std::fopen(file_name.c_str(), "rb");
    if (fp)
    {
      std::string contents;
      std::fseek(fp, 0, SEEK_END);
      contents.resize(std::ftell(fp));
      std::rewind(fp);
      std::fread(&contents[0], 1, contents.size(), fp);
      std::fclose(fp);
      
      // The file has spaces and \n, which do not work well with string based
      // conversions
      RE::regex e(">\\s+<");
      contents = RE::regex_replace (contents, e, std::string("><"));

      
      RE::replace_all(contents, "conversion", "bconversion");
      std::string error_out;
      XMLDocument::uptr_t dom(mgr->create_document_from_string
                              (contents.c_str(), error_out, false/*validate*/));
      rw_yang_netconf_op_status_t ncrs =
          dom->get_root_node()->get_first_element()->to_pbcm(msg);
      if (RW_YANG_NETCONF_OP_STATUS_OK == ncrs) {
        return RW_STATUS_SUCCESS;
      }            
    }
  } 
  return RW_STATUS_FAILURE;
}


static void
build_keypath (confd_hkeypath_t *path)
{
  memset (path, 0, sizeof (confd_hkeypath_t));
  int length = 4;
  path->len = length;
  length --;

  CONFD_SET_XMLTAG (&path->v[length][0], flat_conversion_container_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path->v[length][0], flat_conversion_container_1_1, flat_conversion__ns);
  length --;

  CONFD_SET_XMLTAG (&path->v[length][0],flat_conversion_list_10x2e1_2, flat_conversion__ns);
  length --;

  ASSERT_FALSE (length);
  CONFD_SET_INT32 (&path->v[length][0], 11211121);
  path->v[length][1].type = C_NOEXISTS;
}

TEST(ConfdHkeyIteration, BasicTests)
{
  confd_hkeypath_t path;
  
  build_keypath(&path);
  RwHKeyPathIterator iter(&path);

  ASSERT_EQ (RW_STATUS_FAILURE, iter.move_to_parent());
  ASSERT_EQ (RW_STATUS_FAILURE, iter.move_to_next_sibling());

  RwHKeyPathIterator level = iter;

  // level now at depth 1
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());

  // level now at depth 2
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());
  ASSERT_EQ (RW_STATUS_FAILURE, iter.move_to_next_sibling());
  
  // level now at depth 1
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_parent());
  // @root
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_parent());

  // @level3
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_first_child());

  // leaf doesnt have children
  ASSERT_EQ (RW_STATUS_FAILURE, level.move_to_first_child());
  // and no siblings yet..
  ASSERT_EQ (RW_STATUS_FAILURE, level.move_to_next_sibling());

  // added a sibling now
  CONFD_SET_INT32 (&path.v[0][1], 11211121);
  ASSERT_EQ (RW_STATUS_SUCCESS, level.move_to_next_sibling());

}

rw_status_t rw_test_load_confd_schema (const char *harness_name)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  YangModule* ydom_top = model->load_module("base-conversion");
  ydom_top = model->load_module("flat-conversion");

  ConfdUnittestHarness::uptr_t harness
      = ConfdUnittestHarness::create_harness_autostart(harness_name, model);

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);
  confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa));

  return RW_STATUS_SUCCESS;
}

static rw_status_t rw_test_build_confd_array(const char *harness_name,
                                             confd_tag_value_t **array,
                                             size_t            *count,
                                             struct confd_cs_node **ret_cs_node)
{

  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  YangModel* model = mgr->get_yang_model();
  YangModule* ydom_top = model->load_module("base-conversion");
  RW_ASSERT(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  RW_ASSERT(ydom_top);
  //  YangNode *container_1 = ydom_top->get_first_node();
  
  ConfdUnittestHarness::uptr_t harness
      = ConfdUnittestHarness::create_harness_autostart(harness_name, model);

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

  bool ret = model->app_data_get_token (YANGMODEL_ANNOTATION_KEY,YANGMODEL_ANNOTATION_CONFD_NS ,
                                        &model->adt_confd_ns_);
  RW_ASSERT(ret);
  
  ret =  model->app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_CONFD_NAME,
                                    &model->adt_confd_name_);
  RW_ASSERT(ret);
  
  confd_load_schemas ((struct sockaddr *)&sa, sizeof(sa));
  
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
  
  XMLNode* root_node = dom->get_root_node();
  
  root_node = root_node->get_first_child();
  // associated confd structures
  struct confd_cs_node *cs_node = confd_find_cs_root(flat_conversion__ns);
  
  // Test confd to yang node mapping
  YangNode *yn = root_node->get_yang_node();
  rw_confd_search_child_tags (yn,
                              cs_node->children->ns,
                              cs_node->children->tag);
  
  ConfdTLVBuilder builder(cs_node, true);
  XMLTraverser traverser(&builder,root_node);

  traverser.traverse();

  *count = builder.length();
  *array =  (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * (*count));
  builder.copy_and_destroy_tag_array(*array);
  *ret_cs_node = cs_node;
  return RW_STATUS_SUCCESS;

}
TEST(ConfdUtIteration, BasicTests)
{
  TEST_DESCRIPTION("Basic tests for the confd iterator");
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("base-conversion");
  EXPECT_TRUE(ydom_top);
  ydom_top = 0;
  
  ydom_top = model->load_module("flat-conversion");
  EXPECT_TRUE(ydom_top);

  //  YangNode *container_1 = ydom_top->get_first_node();
  
  ConfdUnittestHarness::uptr_t harness
      = ConfdUnittestHarness::create_harness_autostart("CondfUtIteration", model);
  ASSERT_TRUE(harness.get());
  EXPECT_TRUE(harness->is_running());

  ConfdUnittestHarness::context_t* ctx= harness->get_confd_context();
  UNUSED (ctx);

  ConfdUnittestHarness::sockaddr_t sa;
  harness->make_confd_sockaddr(&sa);

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

  // Test confd to yang node mapping
  YangNode *yn = root_node->get_yang_node();
  ASSERT_TRUE (yn);
  ASSERT_TRUE (cs_node->children);

  ASSERT_TRUE ( rw_confd_search_child_tags (yn,
                                            cs_node->children->ns,
                                            cs_node->children->tag));
  
  // This test depends on the schema to ensure success
  ASSERT_FALSE ( rw_confd_search_child_tags (yn,
                                             cs_node->children->ns + 1, 
                                             cs_node->children->tag));
  // This test depends on the schema to ensure success
  ASSERT_FALSE ( rw_confd_search_child_tags (yn,
                                             cs_node->children->ns,
                                             cs_node->children->tag+1));

  ASSERT_EQ ( rw_confd_search_child_tags (model->get_root_node(),
                                          cs_node->ns, cs_node->tag),
             yn);
      
  // Translate the DOM to confd
  
    ConfdTLVBuilder builder(cs_node, true);
    XMLTraverser traverser(&builder,root_node);

    traverser.traverse();

    ASSERT_TRUE (builder.length());

    size_t length = builder.length();
  
    confd_tag_value_t
        *array =  (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * length);

    builder.copy_and_destroy_tag_array(array);


    // Array is of the following form for the test
  
    /*  1   C_XMLBEGIN  "container_1-1" */
    /*  2       C_BUF "leaf-1_1.1" */
    /*  3       C_XMLBEGIN "list-1.1_2" */
    /*  4   	C_INT32 "int_1.1.2_1" */
    /*  5       C_XMLEND "list-1.1_2" */
    /*  6       C_XMLBEGIN "list-1.1_2" */
    /*  7           C_XMLTAG "grey-goose" */
    /*  8           C_INT32 "int_1.1.2_1" */
    /*  9       C_XMLEND  "list-1.1_2" */
    /* 10   C_XMLEND "container_1-1" */
    /* 11   C_LIST "leaf.list.1.2" */ // HAS 4 ENTRIES !!
    /* 12   C_XMLBEGIN "two-keys" */
    /* 13       C_ENUM_VALUE "prim-enum" */
    /* 14       C_BUF "sec-string" */
    /* 15   C_XMLEND "two-keys" */
    /* 16   C_XMLBEGIN "two-keys" */
    /* 17       C_ENUM_VALUE "prim-enum" */
    /* 18       C_BUF "sec-string" */
    /* 19   C_XMLEND "two-keys" */
    /* 20   C_XMLTAG "empty-1_3" */
    /* 21   C_LIST "enum_1-4" */
    /* 22   C_ENUM_VALUE "enum_1.5" */
    RwTLVAIterator root = RwTLVAIterator(array, length, cs_node);
  
    EXPECT_EQ (root.move_to_next_sibling(),RW_STATUS_FAILURE);
    EXPECT_EQ (root.move_to_parent(), RW_STATUS_FAILURE);

    rw_ylib_data_t data;
    rw_confd_value_t *v = &data.rw_confd;
  
    ASSERT_EQ (RW_STATUS_SUCCESS, root.get_value (&data));
    EXPECT_EQ(v->cs_node, cs_node);
    EXPECT_EQ(v->value, nullptr);
             
    RwTLVAIterator cont = root;
    ASSERT_EQ (RW_STATUS_SUCCESS, cont.move_to_first_child());
    ASSERT_EQ (RW_STATUS_SUCCESS, cont.get_value (&data));
  
    struct xml_tag tag;
    tag.ns = flat_conversion__ns;
    tag.tag = v->cs_node->tag;
  
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));
    const struct confd_cs_node *p_schema = v->cs_node;
  
    EXPECT_EQ(v->value, &array[0].v);
  
    ASSERT_NE (root, cont);
    ASSERT_EQ (cont.position(), 0);
    RwTLVAIterator move_to_parent = cont;
    ASSERT_EQ (RW_STATUS_SUCCESS, move_to_parent.move_to_parent());
    ASSERT_EQ (move_to_parent, root);

    RwTLVAIterator leaf = cont;
    ASSERT_EQ (leaf.move_to_first_child(), RW_STATUS_SUCCESS);
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    tag.tag = v->cs_node->tag;
  
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));
    EXPECT_EQ(v->value, &array[1].v);
  
    move_to_parent = leaf;
    EXPECT_EQ (move_to_parent.move_to_parent(), RW_STATUS_SUCCESS);
    EXPECT_EQ (move_to_parent, cont);
    EXPECT_EQ (leaf.move_to_first_child(), RW_STATUS_FAILURE);

    RwTLVAIterator list = leaf;
    ASSERT_NE (list.move_to_next_sibling(), RW_STATUS_FAILURE);
    EXPECT_EQ (list.position(), 2);
  
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
  
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));
    EXPECT_EQ(v->value, &array[2].v);
  
    leaf = list;
    EXPECT_EQ (leaf.move_to_first_child(),RW_STATUS_SUCCESS);
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    EXPECT_EQ(v->value, &array[3].v);
  
    EXPECT_EQ (leaf.move_to_next_sibling(), RW_STATUS_FAILURE);

    ASSERT_EQ (list.move_to_next_sibling(), RW_STATUS_SUCCESS);
    EXPECT_EQ (list.position(), 5);
    EXPECT_EQ (list.move_to_next_sibling(), RW_STATUS_FAILURE);  

    // The container has more siblings. Lets start there now.
    list = cont;
    struct confd_list *ll = &array[10].v.val.list;
  
    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));  
    EXPECT_EQ(v->value, &ll->ptr[0]);
    EXPECT_STREQ ((char *) v->value->val.c_buf.ptr, "Leaf List First");
    EXPECT_EQ(0, list.list_index());
            
    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));  
    EXPECT_EQ(v->value, &ll->ptr[1]);
    EXPECT_EQ(1, list.list_index());

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));  
    EXPECT_EQ(v->value, &ll->ptr[2]);
    EXPECT_EQ(2, list.list_index());

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));  
    EXPECT_EQ(v->value, &ll->ptr[3]);
    EXPECT_EQ(3, list.list_index());

    // The leaf list is a direct descendent of root. Moving to
    // parent should get us there.

    move_to_parent = list;
    ASSERT_EQ (RW_STATUS_SUCCESS, move_to_parent.move_to_parent());
    EXPECT_EQ (move_to_parent, root);

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    EXPECT_EQ(0, list.list_index());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (cs_node, tag));
    p_schema = v->cs_node;
    EXPECT_EQ(v->value, &array[11].v);

    leaf = list;
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.move_to_first_child());
    EXPECT_EQ(0, leaf.list_index());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    EXPECT_EQ(v->value, &array[12].v);
    EXPECT_EQ(v->value->type, C_ENUM_VALUE);
    EXPECT_EQ(v->value->val.enumvalue, 1);
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));

    ASSERT_EQ (RW_STATUS_FAILURE, leaf.move_to_first_child());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    EXPECT_EQ(v->value, &array[13].v);
    EXPECT_EQ(v->value->type, C_BUF);
    EXPECT_STREQ((char *)v->value->val.c_buf.ptr, "entry");
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));

    ASSERT_EQ (RW_STATUS_FAILURE, leaf.move_to_first_child());
    ASSERT_EQ (RW_STATUS_FAILURE, leaf.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.move_to_parent());
    EXPECT_EQ (leaf, list);

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    leaf = list;
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.move_to_first_child());
    EXPECT_EQ(0, leaf.list_index());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    EXPECT_EQ(v->value, &array[16].v);
    EXPECT_EQ(v->value->type, C_ENUM_VALUE);
    EXPECT_EQ(v->value->val.enumvalue, 7);
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));

    ASSERT_EQ (RW_STATUS_FAILURE, leaf.move_to_first_child());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, leaf.get_value (&data));
    EXPECT_EQ(v->value, &array[17].v);
    EXPECT_EQ(v->value->type, C_BUF);
    EXPECT_STREQ((char *)v->value->val.c_buf.ptr, "entree");
    tag.tag = v->cs_node->tag;
    EXPECT_EQ(v->cs_node, confd_find_cs_node_child (p_schema, tag));

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());
    ASSERT_EQ (RW_STATUS_SUCCESS, list.get_value (&data));
    EXPECT_EQ(v->value, &array[19].v);
    EXPECT_EQ(v->value->type, C_XMLTAG);
    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());  
    ASSERT_EQ(0, list.list_index());
  
    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());  
    ASSERT_EQ(1, list.list_index());

    ASSERT_EQ (RW_STATUS_SUCCESS, list.move_to_next_sibling());  
    ASSERT_EQ(0, list.list_index());

    ASSERT_EQ (RW_STATUS_FAILURE, list.move_to_next_sibling());  
    YangData<RwTLVAIterator, YangNode> yd_root (root, yn);
  
    EXPECT_EQ (yd_root.move_to_next_sibling(),RW_STATUS_FAILURE);
    EXPECT_EQ (yd_root.move_to_parent(), RW_STATUS_FAILURE);
    YangData<RwTLVAIterator, YangNode> yd_cont = yd_root;
    ASSERT_EQ (RW_STATUS_SUCCESS, yd_cont.move_to_first_child());
    //  ASSERT_NE (yd_root, yd_cont);
    ASSERT_EQ (RW_STATUS_SUCCESS, yd_cont.move_to_parent());
    ASSERT_EQ (RW_STATUS_SUCCESS, yd_cont.move_to_first_child());

    YangData<RwTLVAIterator, YangNode> yd_leaf = yd_cont;
    ASSERT_EQ (yd_leaf.move_to_first_child(), RW_STATUS_SUCCESS);
    EXPECT_EQ (yd_leaf.move_to_first_child(), RW_STATUS_FAILURE);

    YangData<RwTLVAIterator, YangNode> yd_list = yd_leaf;
    ASSERT_NE (yd_list.move_to_next_sibling(), RW_STATUS_FAILURE);
    yd_leaf = yd_list;
    EXPECT_EQ (yd_leaf.move_to_first_child(),RW_STATUS_SUCCESS);
    EXPECT_EQ (yd_leaf.move_to_next_sibling(), RW_STATUS_FAILURE);

    ASSERT_EQ (yd_list.move_to_next_sibling(), RW_STATUS_SUCCESS);
    EXPECT_EQ (yd_list.move_to_next_sibling(), RW_STATUS_FAILURE);  

    for (size_t i = 0; i < length; i++) {
      confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
    }
  }
}


TEST (RwPbcmTreeIterator, BasicFunctionsBumpy)
{
  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  UniquePtrProtobufCMessage<RWPB_T_MSG(RiftCliTest_data_GeneralContainer)>::uptr_t
      msg_uptr (proto);
  // This creates a proto with the first child type 
  RwPbcmTreeIterator root((ProtobufCMessage *)proto);

  RwPbcmTreeIterator level1 = root;
  ASSERT_EQ(level1.move_to_first_child(), RW_STATUS_SUCCESS);
  ProtobufCMessage *msg;
  unsigned int fld_id, index;
  std::tie (msg, fld_id, index) = level1.shallowest();

  ASSERT_EQ (msg, (ProtobufCMessage *)proto);
  ASSERT_EQ (fld_id, 1);
  ASSERT_EQ (index, 0);
  

  RwPbcmTreeIterator level2 = level1;
  ASSERT_EQ(level2.move_to_first_child(), RW_STATUS_SUCCESS);

  std::tie (msg, fld_id, index) = level2.shallowest();
  EXPECT_EQ (fld_id, 0);
  EXPECT_EQ (index, 0);

  // level2 is the index part of the list. Add test when the
  // proto API is complete
  EXPECT_EQ (level2.move_to_first_child(), RW_STATUS_FAILURE);
  RwPbcmTreeIterator container = level2;
  EXPECT_EQ(container.move_to_next_sibling(), RW_STATUS_SUCCESS);
  EXPECT_EQ (container.move_to_next_sibling(), RW_STATUS_FAILURE);
  RwPbcmTreeIterator level3 = container;
  ASSERT_EQ (level3.move_to_first_child(), RW_STATUS_SUCCESS);
  std::tie (msg, fld_id, index) = level3.shallowest();
  
  EXPECT_EQ (fld_id, 2);
  EXPECT_EQ (index, 0);

  EXPECT_EQ (level3.move_to_first_child(), RW_STATUS_FAILURE);
  EXPECT_EQ (level3.move_to_next_sibling(), RW_STATUS_FAILURE);

  RwPbcmTreeIterator sibling = level1;
  ASSERT_EQ(sibling.move_to_next_sibling(), RW_STATUS_SUCCESS);
  std::tie (msg, fld_id, index) = sibling.shallowest();

  ASSERT_EQ (msg, (ProtobufCMessage *)proto);
  ASSERT_EQ (fld_id, 1);
  ASSERT_EQ (index, 1);

  ASSERT_EQ (level2.move_to_parent(), RW_STATUS_SUCCESS);
  ASSERT_EQ (level2, level1);

  ASSERT_EQ (level1.move_to_parent(), RW_STATUS_SUCCESS);
  ASSERT_EQ (level1, root);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));
  ASSERT_EQ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
            model->get_ypbc_schema());
  YangNode *yn = model->get_root_node()->get_first_child();
  ASSERT_STREQ (yn->get_name(), "general-container");

  YangData<RwPbcmTreeIterator, YangNode> root_yd(root, yn);
  EXPECT_EQ (RW_STATUS_FAILURE, root_yd.move_to_parent());
  ASSERT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_first_child());
  EXPECT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_parent());
  ASSERT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_first_child());

  ASSERT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_first_child());
  EXPECT_EQ (RW_STATUS_FAILURE, root_yd.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_next_sibling());
  ASSERT_EQ (RW_STATUS_SUCCESS, root_yd.move_to_first_child());
}


TEST(RwXMLTreeIterator, BasicTests)
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

  RwXMLTreeIterator iter = RwXMLTreeIterator(root_node);
  RwXMLTreeIterator root = iter;
  
  ASSERT_EQ(iter.move_to_first_child(), RW_STATUS_SUCCESS);
  ASSERT_EQ(iter.move_to_parent(), RW_STATUS_SUCCESS);
  ASSERT_EQ(iter, root);

  root_node = root_node->get_first_child();
  
  YangData <RwXMLTreeIterator, YangNode> root_yd(RwXMLTreeIterator(root_node),
                                                 root_node->get_yang_node());
  ASSERT_EQ(root_yd.move_to_first_child(), RW_STATUS_SUCCESS);
  ASSERT_EQ(root_yd.move_to_parent(), RW_STATUS_SUCCESS);
  ASSERT_EQ(root_yd.move_to_next_sibling(), RW_STATUS_SUCCESS);
  ASSERT_EQ(root_yd.move_to_first_child(), RW_STATUS_FAILURE);
}

TEST(RwSchemaTreeIterator, BasicTests)
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

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));
    
  // Build something with a list in it
  // and start using the global types..
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm((ProtobufCMessage *)proto, ncrs));
  ASSERT_EQ(ncrs, RW_YANG_NETCONF_OP_STATUS_OK);

  XMLNode *node = dom->get_root_node()->find_first_deepest_node();
  ASSERT_TRUE (node);
  if (node->get_yang_node()->is_leafy()) {
    node = node->get_parent();
  }

  rw_keyspec_path_t* key = nullptr;
  ncrs = node->to_keyspec(&key);

  ASSERT_EQ(ncrs,RW_YANG_NETCONF_OP_STATUS_OK);

  RwSchemaTreeIterator root = RwSchemaTreeIterator(key);
  
  ASSERT_EQ (root.key(), key);
  //ASSERT_EQ (root.length(), 3);

  RwSchemaTreeIterator l1 = root;
  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_first_child());
  ASSERT_EQ (l1.depth(), 1);
  ASSERT_EQ (RW_STATUS_FAILURE, l1.move_to_next_sibling());
  ASSERT_NE (l1, root);
  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_parent());
  ASSERT_EQ (l1, root);

  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_first_child());
  RwSchemaTreeIterator l2 = l1;

  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_next_sibling());

  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_parent());

  rw_status_t rs = rw_keyspec_path_update_binpath(key, nullptr);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  ProtobufCBinaryData bin_key;
  rs = rw_keyspec_path_get_binpath(key, nullptr, (const uint8_t **) &bin_key.data, &bin_key.len);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  rw_keyspec_path_t *gen = rw_keyspec_path_create_with_binpath_binary_data(nullptr, &bin_key);
  ASSERT_TRUE (gen);


  root = RwSchemaTreeIterator(gen);
  
  ASSERT_EQ (root.key(), gen);
  //ASSERT_EQ (root.length(), 3);

  l1 = root;
  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_first_child());
  ASSERT_EQ (l1.depth(), 1);
  ASSERT_EQ (RW_STATUS_FAILURE, l1.move_to_next_sibling());
  ASSERT_NE (l1, root);
  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_parent());
  ASSERT_EQ (l1, root);

  ASSERT_EQ (RW_STATUS_SUCCESS, l1.move_to_first_child());
  l2 = l1;

  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_first_child());
  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_next_sibling());

  ASSERT_EQ (RW_STATUS_SUCCESS, l2.move_to_parent());
}


template <class RwTreeIterator>
class Printer {
 public:
  std::string os_;     // The value of a traversed tree till now
  uint8_t indent_ = 0;    // Intendation for the current line
  
  rw_tree_walker_status_t handle_child (RwTreeIterator *iter) {
    indent_ += 2;
    std::string out;
    rw_status_t rs = get_string_value (iter, out);
    UNUSED (rs);
    
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
    os_ += std::string(indent_,' ');
    os_ += out;
    os_ += '\n';

    return RW_TREE_WALKER_SUCCESS;
  }
                                        
  rw_status_t move_to_parent() {
    indent_ -= 2;

    return RW_STATUS_SUCCESS;
  }
};

/*!
 * A class that will find the message descriptor associated with a path in a tree
 * iterator
 */
template <class RwTreeIterator>
class SchemaFinder {
 public:
  const rw_yang_pb_schema_t* schema_ = nullptr;
  const rw_yang_pb_msgdesc_t *msg_ = nullptr;
  
  SchemaFinder(const rw_yang_pb_schema_t* schema)
      : schema_ (schema) {}
  
  rw_tree_walker_status_t handle_child (RwTreeIterator *iter) {
    const char* name = iter->get_yang_node_name();
    const char* ns = iter->get_yang_ns();

    
    if (nullptr == msg_) {
      msg_ = rw_yang_pb_schema_get_top_level_message (schema_, ns, name);
    } else {
      const rw_yang_pb_msgdesc_t *tmp =
          rw_yang_pb_msg_get_child_message (msg_, ns, name);
      if (tmp) {
        msg_ = tmp;
      } else {
        // get the value and check if it is a leaf
        rw_ylib_data_t child_val;
        rw_status_t rs = iter->get_value(&child_val);


        RW_ASSERT(RW_STATUS_SUCCESS == rs);
        switch (child_val.type){
          case RW_YLIB_DATA_TYPE_CONFD:
            if (child_val.rw_confd.value->type != C_XMLTAG) {
              break;
            }
            // intentional fallthrough
          default:
            msg_ = nullptr;
        }
      }
    }
    
    if (msg_) {
      return RW_TREE_WALKER_SKIP_SIBLINGS;
    }
    return RW_TREE_WALKER_FAILURE;
  }
  
  rw_status_t move_to_parent() {
    return RW_STATUS_SUCCESS;
  }
};



TEST(RwIterWalk, BasicFunc)
{
  confd_tag_value_t *array =  nullptr;
  size_t length;
  struct confd_cs_node *cs_node;
  
  rw_status_t rs = rw_test_build_confd_array ("BasicFunc", &array, &length, &cs_node);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
      
  RwTLVAIterator root = RwTLVAIterator(array, length, cs_node);
  Printer<RwTLVAIterator> print;
  rs = walk_iterator_tree (&root, &print);
  std::cout << print.os_;

  for (size_t i = 0; i < length; i++) {
    confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
  }

  ASSERT_EQ (rs, RW_STATUS_SUCCESS);

  // Walk a protobuf now.
  ProtobufCMessage *bare_msg = nullptr;
  rs = rw_test_build_pb_flat_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);
  
  msg_uptr_t msg = msg_uptr_t (bare_msg);

  RwPbcmTreeIterator pb_root(bare_msg);
  Printer<RwPbcmTreeIterator> print_pbcm;
  rs = walk_iterator_tree (&pb_root, &print_pbcm);
  std::cout << print_pbcm.os_;
  
  rs = rw_test_build_pb_bumpy_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);
  msg = msg_uptr_t (bare_msg);

  RwPbcmTreeIterator pb_root_b(bare_msg);
  Printer<RwPbcmTreeIterator> print_pbcm_b;
  rs = walk_iterator_tree (&pb_root_b, &print_pbcm_b);
  std::cout << print_pbcm_b.os_;

  
}
TEST(RwCopyIters, FlatPbFromDom)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  
  YangModel* model = mgr->get_yang_model();  
  YangModule* ydom_top = model->load_module("base-conversion");
  RW_ASSERT(ydom_top);
  ydom_top = model->load_module("flat-conversion");
  
  char file_name[1023];
  sprintf (file_name, "%s%s", get_rift_root().c_str(),  
                "/modules/core/util/yangtools/test/confd.xml");

  XMLDocument::uptr_t dom = std::move (mgr->create_document_from_file
                                       (file_name, false/*validate*/));

  ASSERT_TRUE (dom.get());
  ASSERT_TRUE (dom->get_root_node());
  RwXMLTreeIterator xml_root = RwXMLTreeIterator(dom->get_root_node()->get_first_element());

  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,copy_from_dom);
  RwPbcmTreeIterator copy_from_dom_iter((ProtobufCMessage *)&copy_from_dom);

  Copier<RwXMLTreeIterator,RwPbcmTreeIterator> from_xml(&copy_from_dom_iter);
  rw_status_t rs = walk_iterator_tree (&xml_root, &from_xml);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);

  YangNode *yn = model->get_root_node();
  yn = yn->search_child(copy_from_dom.base.descriptor->ypbc_mdesc->yang_node_name);
  ASSERT_TRUE (yn);
  XMLDocument::uptr_t from_pbcm_1 = mgr->create_document(yn);

  xml_root = RwXMLTreeIterator(from_pbcm_1->get_root_node());
  Copier<RwPbcmTreeIterator, RwXMLTreeIterator> to_xml(&xml_root);

  rs = walk_iterator_tree (&copy_from_dom_iter, &to_xml);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
}
  

TEST(RwCopyIters, FlatPb)
{
  // Copy a protobuf now.

  ProtobufCMessage *bare_msg = nullptr;
  rw_status_t rs = rw_test_build_pb_flat_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);
  
  msg_uptr_t msg = msg_uptr_t (bare_msg);
  RwPbcmTreeIterator pb_root(bare_msg);

  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,copy_msg);
  RwPbcmTreeIterator copy_iter((ProtobufCMessage *)&copy_msg);
  
  Copier<RwPbcmTreeIterator,RwPbcmTreeIterator> copy_pbcm(&copy_iter);
  rs = walk_iterator_tree (&pb_root, &copy_pbcm);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));

  confd_tag_value_t *array =  nullptr;
  size_t length;
  struct confd_cs_node *cs_node;
  
  rs = rw_test_build_confd_array ("BasicFunc", &array, &length, &cs_node);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
      
  RwTLVAIterator root = RwTLVAIterator(array, length, cs_node);
  RWPB_M_MSG_DECL_INIT(FlatConversion_FirstLevel,copy_from_confd);
  RwPbcmTreeIterator copy_from_confd_iter((ProtobufCMessage *)&copy_from_confd);
  
  Copier<RwTLVAIterator,RwPbcmTreeIterator> from_confd(&copy_from_confd_iter);
  rs = walk_iterator_tree (&root, &from_confd);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, (ProtobufCMessage *)&copy_from_confd));

  

               
}


TEST(RwCopyIters, BumpyPb)
{
  // Copy a protobuf now.
  ProtobufCMessage *bare_msg = nullptr;
  rw_status_t rs = rw_test_build_pb_bumpy_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);
  
  msg_uptr_t msg = msg_uptr_t (bare_msg);
  RwPbcmTreeIterator pb_root(bare_msg);

  RWPB_M_MSG_DECL_INIT(BumpyConversion_BumpyFirstLevel,copy_msg);
  RwPbcmTreeIterator copy_iter((ProtobufCMessage *)&copy_msg);
  
  Copier<RwPbcmTreeIterator,RwPbcmTreeIterator> copy_pbcm(&copy_iter);
  rs = walk_iterator_tree (&pb_root, &copy_pbcm);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));

  confd_tag_value_t *array =  nullptr;
  size_t length;
  struct confd_cs_node *cs_node;
  
  rs = rw_test_build_confd_array ("BasicFunc", &array, &length, &cs_node);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
      
  RwTLVAIterator root = RwTLVAIterator(array, length, cs_node);
  RWPB_M_MSG_DECL_INIT(BumpyConversion_BumpyFirstLevel,copy_from_confd);
  RwPbcmTreeIterator copy_from_confd_iter((ProtobufCMessage *)&copy_from_confd);
  
  Copier<RwTLVAIterator,RwPbcmTreeIterator> from_confd(&copy_from_confd_iter);
  rs = walk_iterator_tree (&root, &from_confd);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, (ProtobufCMessage *)&copy_from_confd));
               
}

TEST(ConfdHkeyToKeySpec, BasicTests)
{
  confd_hkeypath_t path;
  
  build_keypath(&path);
  RwHKeyPathIterator iter(&path);
  rw_test_load_confd_schema("hk2ks");
  const rw_yang_pb_schema_t* schema =
      reinterpret_cast<const rw_yang_pb_schema_t*>(RWPB_G_SCHEMA_YPBCSD(FlatConversion));

  SchemaFinder<RwHKeyPathIterator> schema_finder(schema);
  rw_status_t rs = walk_iterator_tree(&iter, &schema_finder);
  
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);
  ASSERT_TRUE (schema_finder.msg_);
  ASSERT_TRUE (schema_finder.msg_->schema_path_value);
  rw_keyspec_path_t* ks = nullptr;
  
  rs = rw_keyspec_path_create_dup (schema_finder.msg_->schema_path_value, nullptr, &ks);
  ASSERT_EQ (rs, RW_STATUS_SUCCESS);
  RwSchemaTreeIterator root = RwSchemaTreeIterator(ks);
  
  Copier<RwHKeyPathIterator,RwSchemaTreeIterator> from_confd(&root);
  rs = walk_iterator_tree (&iter, &from_confd);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
}

TEST (SearchPbWithKs, BasicTests)
{
  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);

  ASSERT_TRUE (proto);

  XMLManager::uptr_t  mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  YangModule* ydom_top = model->load_module("rift-cli-test");
  EXPECT_TRUE(ydom_top);

  model->register_ypbc_schema(
      (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest));

  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)* ks = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup
             ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_data_GeneralContainer_GList),
              nullptr, (rw_keyspec_path_t **)&ks));
  
  RwSchemaTreeIterator needle = RwSchemaTreeIterator((rw_keyspec_path_t*) ks);
  needle.move_to_first_child();

  RwPbcmTreeIterator haystack((ProtobufCMessage *)proto);
  
  Logger<RwPbcmTreeIterator, RwSchemaTreeIterator> tmp_log;
  Collector <RwPbcmTreeIterator, RwSchemaTreeIterator> collector(tmp_log);

  rw_tree_walker_status_t rw = Find (&haystack, &needle, collector, tmp_log);
  ASSERT_EQ (RW_TREE_WALKER_SUCCESS, rw);
  ASSERT_EQ (collector.matches_.size(),5);

  ks->dompath.path001.has_key00 = 1;
  ks->dompath.path001.key00.index = 20;

  needle = RwSchemaTreeIterator((rw_keyspec_path_t*) ks);
  needle.move_to_first_child();
  
  rw = Find (&haystack, &needle, collector, tmp_log);
  ASSERT_EQ (RW_TREE_WALKER_SUCCESS, rw);
  ASSERT_EQ (collector.matches_.size(),6);
  
}


TEST(RwMergeIters, BumpyPb)
{
  // Copy a protobuf now.
  ProtobufCMessage *bare_msg = nullptr;
  rw_status_t rs = rw_test_build_pb_bumpy_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);
  
  msg_uptr_t msg = msg_uptr_t (bare_msg);
  RwPbcmTreeIterator pb_root(bare_msg);

  RWPB_M_MSG_DECL_INIT(BumpyConversion_BumpyFirstLevel,copy_msg);
  
  RwPbcmTreeIterator merge_iter((ProtobufCMessage *)&copy_msg);
  Merge<RwPbcmTreeIterator,RwPbcmTreeIterator> merge_pbcm(&merge_iter);
  rs = walk_iterator_tree (&pb_root, &merge_pbcm);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));


  // Change a leaf in the original message, check and merge back.
  RWPB_T_MSG(BumpyConversion_BumpyFirstLevel) *spec_msg =
      ( RWPB_T_MSG(BumpyConversion_BumpyFirstLevel) *) bare_msg;

  // ATTN: Leaf lists do not merge correctly - remove the following to test
  // once RIFT-6511 is fixed
  spec_msg->n_leaf_list_1_2 = 0;
  spec_msg->n_enum_1_4 = 0;

  copy_msg.n_leaf_list_1_2 = 0;
  copy_msg.n_enum_1_4 = 0;
  ASSERT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));
  
  spec_msg->enum_1_5 = BASE_CONVERSION_CB_ENUM_FIRST;
  ASSERT_FALSE(protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));
  
  // The iterators should be back where we started, merge again
  rs = walk_iterator_tree (&pb_root, &merge_pbcm);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_TRUE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));

  // Change the key of a list - the merge should not create equal values now.
  spec_msg->two_keys[1]->prim_enum = BASE_CONVERSION_CB_ENUM_FIRST;
  // The iterators should be back where we started, merge again
  rs = walk_iterator_tree (&pb_root, &merge_pbcm);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_FALSE (protobuf_c_message_is_equal_deep(pinstance, (ProtobufCMessage *)&copy_msg, bare_msg));

  EXPECT_EQ (spec_msg->n_two_keys, 2);
  EXPECT_EQ (copy_msg.n_two_keys, 3);
  
}

TEST(RwFindItersWithTraverser, BumpyPbKs)
{
  ProtobufCMessage *bare_msg = nullptr;
  rw_status_t rs = rw_test_build_pb_bumpy_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_NE (nullptr, bare_msg);

  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)* ks = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup
             ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_data_GeneralContainer_GList),
              nullptr, (rw_keyspec_path_t **)&ks));
  
  RwSchemaTreeIterator needle = RwSchemaTreeIterator((rw_keyspec_path_t*) ks);
  RwPbcmTreeIterator find_data((ProtobufCMessage *)bare_msg);
  
  Finder<RwPbcmTreeIterator,RwSchemaTreeIterator> finder(&needle);
  rs = walk_iterator_tree (&find_data, &finder);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_EQ (0, finder.matches_.size());
  
  RWPB_T_MSG(RiftCliTest_data_GeneralContainer) *proto =
      get_general_containers(5);
  
  ASSERT_TRUE (proto);
  RwPbcmTreeIterator haystack((ProtobufCMessage *)proto);
  
  rs = walk_iterator_tree (&haystack, &finder);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_EQ (0, finder.matches_.size());

  // Move the needle down, so that it matches the data in the protobuf
  needle.move_to_first_child();
  Finder<RwPbcmTreeIterator,RwSchemaTreeIterator> new_finder(&needle);
  rs = walk_iterator_tree (&haystack, &new_finder);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_EQ (5, new_finder.matches_.size());

  // add the index, and walk again - one more will match, for a total of 6
  ks->dompath.path001.has_key00 = 1;
  ks->dompath.path001.key00.index = 20;
  rs = walk_iterator_tree (&haystack, &new_finder);
  EXPECT_EQ (RW_STATUS_SUCCESS, rs);
  EXPECT_EQ (6, new_finder.matches_.size());


}

TEST(RwCopyKstoMsg, BumpyPb)
{
  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer, pb_msg);
  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)* ks = 0;
  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup
             ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_data_GeneralContainer_GList), nullptr,
              (rw_keyspec_path_t **)&ks));
  ks->dompath.path001.has_key00 = 1;
  ks->dompath.path001.key00.index = 20;
  
  RwSchemaTreeIterator ks_iter = RwSchemaTreeIterator((rw_keyspec_path_t*) ks);
  RwPbcmTreeIterator pb((ProtobufCMessage *)&pb_msg);

  Copier<RwSchemaTreeIterator,RwPbcmTreeIterator> copy_pbcm (&pb);
  // the ks starts at root - move to first child to get to the right level
  ks_iter.move_to_first_child();
  rw_status_t rs = walk_iterator_tree (&ks_iter, &copy_pbcm);

  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  ASSERT_EQ (pb_msg.n_g_list, 1);
  EXPECT_EQ (pb_msg.g_list[0]->index, 20);
  
}


TEST (RwCopyKsPbcmToDom, BasicTests)
{
  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer_GList, pb_msg);
  RWPB_M_MSG_DECL_INIT(RiftCliTest_data_GeneralContainer_GList_GclContainer, gc);
  RWPB_T_PATHSPEC(RiftCliTest_data_GeneralContainer_GList)* ks = 0;

  gc.has_having_a_bool = 1;
  gc.having_a_bool = 1;
  
  ASSERT_EQ (RW_STATUS_SUCCESS, rw_keyspec_path_create_dup
             ((rw_keyspec_path_t*) RWPB_G_PATHSPEC_VALUE(RiftCliTest_data_GeneralContainer_GList),
              nullptr,
              (rw_keyspec_path_t **)&ks));
  ks->dompath.path001.has_key00 = 1;
  ks->dompath.path001.key00.index = 20;

  pb_msg.index = 20;
  pb_msg.has_index = 1;
  pb_msg.gcl_container = &gc;

  RwKsPbcmIterator ks_pbcm ((rw_keyspec_path_t *)ks, (ProtobufCMessage *) &pb_msg);
  RwPbcmDom dom ((const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RiftCliTest),
                 RW_PBCM_DOM_TYPE_DATA);
  RwPbcmDomIterator dom_iter(&dom);

  Copier<RwKsPbcmIterator,RwPbcmDomIterator> copy_pbcm (&dom_iter);
  rw_status_t rs = walk_iterator_tree (&ks_pbcm, &copy_pbcm);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  ProtobufCMessage *msg = nullptr;
  //should fail, as it already exists
  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_CREATE, msg);
  ASSERT_EQ (RW_STATUS_FAILURE, rs);
  
  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_FIND_OR_CREATE, msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
  RWPB_T_MSG(RiftCliTest_data)* spec_msg = (RWPB_T_MSG(RiftCliTest_data)*) msg;
  ASSERT_NE (spec_msg, nullptr);
  ASSERT_NE (spec_msg->general_container, nullptr);
  ASSERT_EQ (spec_msg->general_container->n_g_list, 1);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->index, 20);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->gcl_container->has_having_a_bool, 1);

  ks->dompath.path001.has_key00 = 1;
  ks->dompath.path001.key00.index = 10;

  pb_msg.index = 10;
  pb_msg.has_index = 1;
  pb_msg.gcl_container = &gc;

  // Test the merge within a message now
  Merge<RwKsPbcmIterator,RwPbcmDomIterator> merge_dom (&dom_iter);
  RwKsPbcmIterator ks_pbcm_merge ((rw_keyspec_path_t *)ks, (ProtobufCMessage *) &pb_msg);
  rs = walk_iterator_tree (&ks_pbcm_merge, &merge_dom);

  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_CREATE, msg);
  ASSERT_EQ (RW_STATUS_FAILURE, rs);
  
  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_FIND_OR_CREATE, msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  spec_msg = (RWPB_T_MSG(RiftCliTest_data)*) msg;
  ASSERT_EQ (spec_msg->general_container->n_g_list, 2);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->index, 20);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->gcl_container->has_having_a_bool, 1);

  rs = walk_iterator_tree (&ks_pbcm_merge, &merge_dom);

  ASSERT_EQ (RW_STATUS_SUCCESS, rs);

  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_CREATE, msg);
  ASSERT_EQ (RW_STATUS_FAILURE, rs);
  
  rs = dom.get_module_tree ("rift-cli-test", RW_PBCM_DOM_FIND_OR_CREATE, msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
  ASSERT_EQ (spec_msg->general_container->n_g_list, 2);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->index, 20);
  EXPECT_EQ (spec_msg->general_container->g_list[0]->gcl_container->has_having_a_bool, 1);


  ProtobufCMessage *bare_msg;
  rs = rw_test_build_pb_flat_from_file ("confd.xml", &bare_msg);
  ASSERT_EQ (RW_STATUS_SUCCESS, rs);
  
  RWPB_T_MSG (FlatConversion_data_Container1) *msg_tk =
      (RWPB_T_MSG (FlatConversion_data_Container1) *)bare_msg;

  RWPB_M_PATHSPEC_DECL_INIT(FlatConversion_data_Container1_TwoKeys,ks_tk);
  ks_tk.dompath.path001.has_key00 = 1;
  ks_tk.dompath.path001.has_key01 = 1;
  
  for (uint32_t i = 0; i < msg_tk->n_two_keys; i++) {
    
    ks_tk.dompath.path001.key00.prim_enum =
        msg_tk->two_keys[i].prim_enum;
    strcpy (ks_tk.dompath.path001.key01.sec_string,
            msg_tk->two_keys[i].sec_string);

    ks_pbcm_merge = RwKsPbcmIterator((rw_keyspec_path_t *)&ks_tk,
                                     (ProtobufCMessage *) &msg_tk->two_keys[i]);
    rs = walk_iterator_tree (&ks_pbcm_merge, &merge_dom);

    ASSERT_EQ (RW_STATUS_SUCCESS, rs);
    
    rs = dom.get_module_tree ("flat-conversion", RW_PBCM_DOM_FIND, msg);
    ASSERT_EQ (RW_STATUS_SUCCESS, rs);

    RWPB_T_MSG(FlatConversion_data)* fc = (RWPB_T_MSG(FlatConversion_data)*) msg;
    ASSERT_EQ (i+1, fc->container_1->n_two_keys);
  }
}
