
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
 *
 */

#include "rw_confd_annotate.hpp"
#include "confd_xml.h"


using namespace rw_yang;

rw_status_t find_confd_hash_values (YangNode *node,struct xml_tag *tag)
{

  YangModel *model = node->get_model();
  intptr_t tmp = (intptr_t)0;

  bool status = false;

  status = node->app_data_check_and_get (&model->adt_confd_ns_, &tmp);
  RW_ASSERT(status);
  tag->ns = tmp;

  status = node->app_data_check_and_get (&model->adt_confd_name_, &tmp);
  RW_ASSERT(status);

  tag->tag = tmp;

  return RW_STATUS_SUCCESS;
}

rw_status_t get_current_key_and_next_node (
    XMLNode *node,
    confd_cs_node *parent,
    XMLNode **next_node,
    confd_value_t *values,
    int *values_count)
{
  *values_count = 0;
  YangNode* yang_node = node->get_descend_yang_node();
  rw_status_t status = RW_STATUS_SUCCESS;

  *next_node = nullptr;

  RW_ASSERT (yang_node);
  RW_ASSERT (yang_node->get_stmt_type() == RW_YANG_STMT_TYPE_LIST);

  *next_node = node->get_next_sibling(node->get_local_name(), node->get_name_space());

  if (!yang_node->has_keys()) {
    values[0].type = C_INT64;
    values[0].val.i64 = (int64_t )node;
    *values_count = 1;
    return RW_STATUS_SUCCESS;
  }
  
  YangKeyIter yki = yang_node->key_begin();
  
  for (yki=yang_node->key_begin(); yki != yang_node->key_end(); yki++) {
    
    YangNode *key_yang_node = yki->get_key_node();
    RW_ASSERT (key_yang_node->get_stmt_type() == RW_YANG_STMT_TYPE_LEAF);
    XMLNode *key_node = node->find (key_yang_node->get_name(),yang_node->get_ns());
    RW_ASSERT (key_node);

    
    struct xml_tag tag = {0};
    status = find_confd_hash_values(key_yang_node, &tag);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
    
    // find the node corresponding to this type from confd land
    struct confd_cs_node *cs_node = confd_find_cs_node_child(parent, tag);
    RW_ASSERT (cs_node);
    RW_ASSERT (cs_node->info.type);
    std::string value_string = key_node->get_text_value();
    if (!value_string.length()) {
      RW_CRASH();
      break;
    }
    
    int ret = confd_str2val (cs_node->info.type, value_string.c_str(),
                             &values[*values_count]);
    RW_ASSERT(CONFD_OK == ret);
    (*values_count)++;
  }
 
  return RW_STATUS_SUCCESS;

}

XMLNode* get_node_by_hkeypath (XMLNode *node,
                               confd_hkeypath_t *keypath,
                               uint16_t stop_at)

{
  const char *ns = 0;

  int i = keypath->len;

  while (i > stop_at) {

    confd_value_t *val = &keypath->v[i-1][0];
    const char *tag = 0;
    char value[CONFD_MAX_STRING_LENGTH];
    struct confd_cs_node *cs_node = confd_find_cs_node (keypath, keypath->len - (i - 1));


    RW_ASSERT(cs_node);
    RW_ASSERT(C_XMLTAG == val->type);

    tag = confd_hash2str (val->val.xmltag.tag);
    ns = confd_hash2str (val->val.xmltag.ns);

    node = node->find (tag, ns);

    if (nullptr == node) {
      return nullptr;
    }

    i--;

    if (i == 0) {
      return node;
    }

    YangNode *yn = node->get_yang_node();
    
    if (!yn->is_listy()) {
      continue;
    }

    if (!yn->has_keys()) {
      // Keyless list - there should be only a single "key-value", which should
      // be a ptr to a sibling value. There is no list node parent in XML.
      RW_ASSERT(keypath->v[i-1][0].type == C_INT64);
      RW_ASSERT(keypath->v[i-1][1].type == C_NOEXISTS);
      XMLNode *sibling = (XMLNode *) (keypath->v[i-1][0].val.i64);
      if (nullptr == sibling) {
        return nullptr;
      }

      
      RW_ASSERT (sibling->get_parent() == node->get_parent());

      // There could be multiple keyless lists
      node = sibling;
      i --;
      continue;
    }
    
    bool matched = true;

    // The node has moved to the *first* list entry that it could find.
    // go through all its siblings with matching names to see if any match
    // all the keys.

    while (node) {

      matched = true;

      for (int j = 0; val->type != C_NOEXISTS; j++) {
        
        val = &keypath->v[i-1][j];

        RW_ASSERT(cs_node->info.keys[j]);
        tag = confd_hash2str(cs_node->info.keys[j]);

        switch (val->type) {
          case C_NOEXISTS:
          case C_XMLEND:
          case C_BIT32:
          case C_BIT64:
          case C_XMLTAG:
            RW_CRASH();
            break;
          case C_ENUM_VALUE:
            if (nullptr == node->find_enum (tag, ns, CONFD_GET_ENUM_VALUE (val))) {
              matched = false;
            }
            break;
          default:
            size_t len =  confd_pp_value (value, sizeof (value), val);
            RW_ASSERT (len < sizeof (value));
            
            if (!len || nullptr == node->find_value (tag, value, ns)) {
              matched = false;
            }
            break;
        }
        if (!matched) {
          break;
        }
        val = &keypath->v[i-1][j+1];
      }

      if (matched) {
        break;
      }
      node = node->get_next_sibling(node->get_local_name(), node->get_name_space());
    }

    if (!matched) {
      return nullptr;
    }
    // Account for the key
    i--;
  }
  return node;
}


rw_status_t get_confd_case ( rw_yang::XMLNode *root,
                             confd_hkeypath_t *keypath,
                             confd_value_t *choice,
                             confd_value_t *case_val)
{
  RW_ASSERT (case_val);
  rw_yang::XMLNode *node = get_node_by_hkeypath (root, keypath, 0);

  if (nullptr == node) {
    return RW_STATUS_FAILURE;
  }

  XMLNodeList::uptr_t children(node->get_children());

  for (size_t i = 0; i < children->length(); i++) {
    XMLNode* child = children->at(i);
    YangNode *yn = child->get_descend_yang_node();

    if (nullptr == yn) {
      continue;
    }

    YangNode *yn_case = yn->get_case();
    if (nullptr == yn_case) {
      continue;
    }

    // The comparision starts with the choice..
    YangNode *yn_choice = yn_case->get_choice();
    RW_ASSERT (yn_choice);

    while (choice->type != C_NOEXISTS) {

      RW_ASSERT (choice->type == C_XMLTAG);

      const char * tag = confd_hash2str (choice->val.xmltag.tag);
      int n_s = choice->val.xmltag.ns;
      const char *ns = confd_hash2str (n_s);

      if (strcmp (tag, yn_choice->get_name()) ||
          strcmp (ns, yn_choice->get_ns())) {
        break;
      }

      // The case matches - check if choice exists
      choice++;

      if (C_NOEXISTS == choice->type) {
        case_val->type = C_XMLTAG;
        case_val->val.xmltag.ns = n_s;
        case_val->val.xmltag.tag = confd_str2hash (yn_case->get_name());
        return RW_STATUS_SUCCESS;
      }

      RW_ASSERT (choice->type == C_XMLTAG);

      yn_case = yn_choice->get_case();

      // Can this be null??
      RW_ASSERT (yn_case);

      tag = confd_hash2str (choice->val.xmltag.tag);
      ns = confd_hash2str (choice->val.xmltag.ns);

      if (strcmp (tag, yn_case->get_name()) ||
          strcmp (ns, yn_case->get_ns())) {
        break;
      }
    }
  }

  return RW_STATUS_FAILURE;
}

rw_status_t get_element (XMLNode *node, confd_cs_node *cs_node,
                         confd_value_t *value)
{
  YangNode *yang_node = node->get_yang_node();
  RW_ASSERT (yang_node);

  if (RW_YANG_LEAF_TYPE_EMPTY == yang_node->get_type()->get_leaf_type()) {

    value->type = C_XMLTAG;

  } else if (RW_YANG_STMT_TYPE_LEAF_LIST == yang_node->get_stmt_type()) {

    std::string y_name = node->get_local_name();
    std::string ns = node->get_name_space();

    XMLNode *sibling = node->get_next_sibling(y_name, ns);
    if (yang_node->get_type()->get_leaf_type() != RW_YANG_LEAF_TYPE_STRING) { 

      std::string all_values = node->get_text_value();
      while (nullptr != sibling) {
        all_values += " ";
        all_values += sibling->get_text_value();
        sibling = sibling->get_next_sibling(y_name, ns);
      }

      int ret = confd_str2val (cs_node->info.type, all_values.c_str(), value);
      if (CONFD_OK != ret) {
        return RW_STATUS_FAILURE;
      }

    } else {

       std::vector<std::string> values;
       values.push_back(node->get_text_value());

       while (nullptr != sibling) {
         values.push_back (sibling->get_text_value());
         sibling = sibling->get_next_sibling (y_name, ns);
       }

       value->type = C_LIST;
       value->val.list.ptr =(confd_value_t *) malloc (sizeof (confd_value_t) *  values.size());
       value->val.list.size = values.size();

       size_t i = 0;
       for (auto iter:values) {
         CONFD_SET_CBUF(&value->val.list.ptr[i], strdup(iter.c_str()), iter.length());
         i++;
       }

       RW_ASSERT(i == values.size());
    }

  } else {

    std::string value_string = node->get_text_value();
    int ret = confd_str2val (cs_node->info.type, value_string.c_str(), value);
    if (CONFD_OK != ret) {
      return RW_STATUS_FAILURE;
    }
  }

  return RW_STATUS_SUCCESS;
}


ConfdTLVTraverser::ConfdTLVTraverser (Builder *builder, 
                                      confd_tag_value_t *values, 
                                      int count)
    :Traverser (builder)
{
  for (int i = 0; i < count; i++) {
    if (ns_rejection_map.find(
            confd_hash2str((&values[i])->tag.ns)) != ns_rejection_map.end()) {
      continue;
    }
    tlv_tree_.push_back(&values[i]);
  }

  current_node_ = tlv_tree_.begin();
}

ConfdTLVTraverser::~ConfdTLVTraverser()
{
}

static inline std::string get_confd_pp_value(const confd_value_t *v)
{
  RW_ASSERT (v);

  char stack_buff [4096];
  stack_buff[0] = 0;
  int buf_sz = sizeof (stack_buff);

  char* work_buf = &(stack_buff[0]);
  std::unique_ptr<char[]> uptr;

  do {
    int len =  confd_pp_value (work_buf, buf_sz, v);
    RW_ASSERT (len >= 0);
    if (len < buf_sz) {
      break;
    }

    buf_sz *= 2;
    uptr.reset(new char[buf_sz]);
    work_buf = uptr.get();
    work_buf[0] = 0;

  } while(1);

  return std::string(work_buf);
}

static inline std::string get_confd_val2str(struct confd_type *type, const confd_value_t *val)
{
  RW_ASSERT (type);
  RW_ASSERT (val);

  char stack_buff [4096];
  stack_buff[0] = 0;
  int buf_sz = sizeof (stack_buff);

  char* work_buf = &(stack_buff[0]);
  std::unique_ptr<char[]> uptr;

  do {

    int len = confd_val2str(type, val, work_buf, buf_sz);
    RW_ASSERT (len != CONFD_ERR);
    RW_ASSERT (len >= 0);

    if (len < buf_sz) {
      break;
    }

    buf_sz *= 2;
    uptr.reset(new char[buf_sz]);
    work_buf = uptr.get();
    work_buf[0] = 0;

  } while(1);

  return std::string(work_buf);
}

rw_xml_next_action_t ConfdTLVTraverser::build_next()
{

  if (tlv_tree_.end() == current_node_) {
    // The last node has been traversed, indicate a stop
    return RW_XML_ACTION_TERMINATE;
  }

  std::string value;
  std::string log_str;

  confd_tag_value_t *val = *current_node_;
  const char * tag = confd_hash2str (val->tag.tag);
  const char *ns = confd_hash2str (val->tag.ns);
  confd_cs_node* cs_node = nullptr;
  RW_TRAVERSER_LOG_DEBUG ((log_str = std::string(tag) + " " + std::string(ns)));

  // move to the next node
  current_node_++;

  switch (val->v.type) {
    case C_XMLBEGIN:
      // ATTN: Needs re-work for presence containers
      cs_node = get_confd_schema_node(val->tag);
      if (!cs_node) {
        return RW_XML_ACTION_TERMINATE;
      }
      schema_stack_.push(cs_node);
      if (current_node_ == tlv_tree_.end()) {
        return RW_XML_ACTION_TERMINATE;
      }

    return builder_->push(tag, ns);
    case C_XMLEND:
      // The stack is not supposed to be empty, but just as a precaution, with
      // malformed TLV's (atleast there is one unittest case for this)
      if (!schema_stack_.empty()) {
        schema_stack_.pop();
      }
      return builder_->pop();
    case C_XMLTAG:
      return builder_->append_child (tag, ns, 0);
    case C_BIT32:
    case C_BIT64: {
      std::string log("C_BIT32 / C_BIT64 Types not handled");
      RW_TRAVERSER_LOG_ERROR (log);
      return RW_XML_ACTION_ABORT;
    }
    case C_ENUM_VALUE:
      return builder_->append_child_enum (tag, ns, CONFD_GET_ENUM_VALUE(&val->v));

    case C_LIST:
      {
        confd_value *ptr = val->v.val.list.ptr;
        rw_xml_next_action_t ret = RW_XML_ACTION_NEXT;
        for (size_t i = 0; i < val->v.val.list.size; i++) {

          switch (ptr->type) {
            case C_XMLBEGIN:
            case C_XMLEND:
            case C_BIT32:
            case C_BIT64:
            case C_LIST:
            case C_XMLTAG: {
              std::string log("Type not handled: ");
              log += std::to_string(ptr->type);
              RW_TRAVERSER_LOG_ERROR (log);
              return RW_XML_ACTION_ABORT;
            }
            case C_ENUM_VALUE:
              ret = builder_->append_child_enum (tag, ns, CONFD_GET_ENUM_VALUE(&ptr[i]));
              break;
            case C_BINARY:
              cs_node = get_confd_schema_node(val->tag);
              if (!cs_node) {
                // Confd schema node not found, return error
                return RW_XML_ACTION_TERMINATE;
              }
              value = get_confd_val2str (cs_node->info.type, &ptr[i]);
              ret = builder_->append_child (tag, ns, value.c_str());
              break;
            default:
              value = get_confd_pp_value (&ptr[i]);
              ret = builder_->append_child (tag, ns, value.c_str());
              break;
          }
        }
        return ret;
      }
    case C_NOEXISTS:
    case C_XMLBEGINDEL:
      if (val->v.type == C_XMLBEGINDEL) {
        cs_node = get_confd_schema_node(val->tag);
        if (!cs_node) {
          return RW_XML_ACTION_TERMINATE;
        }
        schema_stack_.push(cs_node);
      }
      return builder_->mark_child_for_deletion(tag, ns);
    case C_BINARY:
      // ATTN: This conversion should be the default instead of confd_pp_value
      cs_node = get_confd_schema_node(val->tag);
      if (!cs_node) {
        // Confd schema node not found, return error
        return RW_XML_ACTION_TERMINATE;
      }
      value = get_confd_val2str (cs_node->info.type, &val->v);
      return builder_->append_child (tag, ns, value.c_str());

    default:
      value = get_confd_pp_value (&val->v);
      return builder_->append_child (tag, ns, value.c_str());

  }
  std::string log("Unexpected code path hit");
  RW_TRAVERSER_LOG_ERROR (log);
  return RW_XML_ACTION_TERMINATE;
}

rw_xml_next_action_t ConfdTLVTraverser::build_next_sibling()
{
  if (tlv_tree_.end() == current_node_) {
    // The last node has been traversed, indicate a stop
    return RW_XML_ACTION_TERMINATE;
  }

  cta_iter prev = current_node_ - 1;
  if ( (*prev)->v.type == C_XMLBEGIN) {
    while (( (*current_node_)->v.type != C_XMLEND) &&
           (current_node_ != tlv_tree_.end())) {
      current_node_++;
    }

    if (current_node_ != tlv_tree_.end()) {
      current_node_++;
    }
  }

  // an exit of 1 level has been made if the current node was a container or list

  return build_next();
}

rw_yang_netconf_op_status_t ConfdTLVTraverser::stop()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

rw_yang_netconf_op_status_t ConfdTLVTraverser::abort_traversal()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

void ConfdTLVTraverser::set_confd_schema_root(confd_cs_node* cs_node)
{
  RW_ASSERT(schema_stack_.empty());
  schema_stack_.push(cs_node);
}

confd_cs_node* ConfdTLVTraverser::get_confd_schema_node(struct xml_tag tag)
{
  confd_cs_node* cs_node = nullptr;
  if (schema_stack_.empty()) {
    // Request from confd are assumed to be rooted, the first container/leaf
    // is assumed to be from /
    confd_cs_node* root_cs = confd_find_cs_root(tag.ns);
    for (cs_node = root_cs;
        cs_node; cs_node = cs_node->next) {
      if (tag.tag == cs_node->tag &&
          tag.ns == cs_node->ns) {
        break;
      }
    }
    if (!cs_node) {
      // Some unittests doesn't start from the root since the
      // ConfdTLVBuilder doesn't have it. Check the children
      cs_node = confd_find_cs_node_child(root_cs, tag);
    }
  } else {
    cs_node = confd_find_cs_node_child(schema_stack_.top(), tag);
  }

  if (!cs_node) {
    std::string log("Confd node not found ");
    log += std::to_string(tag.tag) + " " + std::to_string(tag.ns);
    RW_TRAVERSER_LOG_ERROR (log);
  }

  return cs_node;
}


/******************* ConfdKeypathTraverser *************************/

ConfdKeypathTraverser::ConfdKeypathTraverser(Builder *builder,
                                             confd_hkeypath_t *keypath)
    :Traverser (builder),
     keypath_ (keypath)
{
  curr_idx_ = keypath_->len - 1;
  curr_key_ = 0;
}


ConfdKeypathTraverser::~ConfdKeypathTraverser()
{
}


rw_xml_next_action_t ConfdKeypathTraverser::build_next()
{
  if (curr_idx_ < 0 ) {
    // The last node has been traversed, indicate a stop
    return RW_XML_ACTION_TERMINATE;
  }

  if (C_NOEXISTS == keypath_->v[curr_idx_][curr_key_].type) {
    // No more keys, go to next node.
    curr_key_ = 0;
    curr_idx_ --;
    return RW_XML_ACTION_NEXT;
  }

  char value[CONFD_MAX_STRING_LENGTH];
  const char *tag = 0, *ns = 0;

  confd_value_t *val = &keypath_->v[curr_idx_][curr_key_];

  if (C_XMLTAG == val->type) {

    tag = confd_hash2str (val->val.xmltag.tag);
    ns = confd_hash2str (val->val.xmltag.ns);
    curr_idx_ --;

  } else {
    // Key values

    if (curr_idx_ == keypath_->len - 1) {  // Cannot be the top node.
      std::string log("Parent does not exist : ");
      log += std::to_string(curr_idx_);
      RW_TRAVERSER_LOG_ERROR (log);
      return RW_XML_ACTION_ABORT;
    }
    struct confd_cs_node *node = confd_find_cs_node (keypath_,
                                                     keypath_->len - curr_idx_ - 1);
    RW_ASSERT(node);
    RW_ASSERT(node->info.keys);
    RW_ASSERT(node->info.keys[curr_key_]);
    tag = confd_hash2str (node->info.keys[curr_key_]);

    curr_key_++;
  }
  switch (val->type) {
    case C_NOEXISTS:
    case C_XMLEND:
      return RW_XML_ACTION_TERMINATE; 
    case C_BIT32:
    case C_BIT64:
      return RW_XML_ACTION_TERMINATE;
    case C_XMLTAG:
      {
        return builder_->push(tag, ns);
      }

    case C_ENUM_VALUE:
      return builder_->append_child_enum (tag, ns,CONFD_GET_ENUM_VALUE(val));
    default:
      {
        size_t len =  confd_pp_value (value, sizeof (value), val);
        RW_ASSERT (len < sizeof (value));

        return builder_->append_child (tag, ns, value);
      }
  }

  RW_ASSERT_NOT_REACHED ();
}



rw_xml_next_action_t ConfdKeypathTraverser::build_next_sibling()
{
  // skip a level - doesnt make sense for this traverser
  return RW_XML_ACTION_ABORT;
}

rw_yang_netconf_op_status_t ConfdKeypathTraverser::stop()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

rw_yang_netconf_op_status_t ConfdKeypathTraverser::abort_traversal()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

/*****************************************************************************
 * ConfdTLVBuilder
 */

ConfdTLVBuilder::ConfdTLVBuilder(confd_cs_node *cfd_schema_node,
                                 bool include_lists): current_schema_node_(cfd_schema_node)
                                                    , include_lists_(include_lists)
{
}

ConfdTLVBuilder::~ConfdTLVBuilder()
{
}

const size_t CONFD_TLV_BUILDER_CLIST_REALLOC_SIZE = 10;

rw_xml_next_action_t ConfdTLVBuilder::append_child (XMLNode *node)
{

  YangNode *yang_node = node->get_yang_node();
  RW_ASSERT (yang_node);
  confd_tag_value_t val;

  RW_ASSERT(yang_node->is_leafy());
  // Set the tag
  rw_status_t status = RW_STATUS_FAILURE;

  status = find_confd_hash_values(yang_node, &val.tag);
  if (RW_STATUS_SUCCESS != status) {
    std::string log("Node not found:");
    log += yang_node->get_name();
    RW_BUILDER_LOG_ERROR (log);
    return RW_XML_ACTION_ABORT;
  }

  // Build the leaf-list values only for the first node.

  if ( RW_YANG_STMT_TYPE_LEAF_LIST == yang_node->get_stmt_type() ) {

    std::string y_name = node->get_local_name();
    std::string ns = node->get_name_space();

    if ( nullptr != node->get_previous_sibling(y_name, ns)) {
      // The First leaf-list node already visited, just return
      return RW_XML_ACTION_NEXT;
    }
  }

  // Find the childs confd schema node
  struct confd_cs_node *cs_node =
      confd_find_cs_node_child(current_schema_node_, val.tag);
  RW_ASSERT (cs_node);

  status = get_element (node, cs_node, &val.v);

  if (status != RW_STATUS_SUCCESS) {
    std::string log("Failed xml to confd conversion.");
    log += node->get_text_value();
    RW_BUILDER_LOG_ERROR (log);
    return RW_XML_ACTION_ABORT;
  }
  values_.push_back (val);
  return RW_XML_ACTION_NEXT;
}



rw_xml_next_action_t ConfdTLVBuilder::push (XMLNode *node)
{
  RW_ASSERT (!node->get_yang_node()->is_leafy());
  if ((!values_.empty() && node->get_yang_node()->is_listy()) &&
      ( !include_lists_)){
    // Confd doesnt like embedded lists - will ask for it later
    // Except if this is the first element - that is..
    return RW_XML_ACTION_NEXT_SIBLING;
  }

  confd_tag_value_t val;

  schema_stack_.push (current_schema_node_);

  rw_status_t ret = find_confd_hash_values(node->get_yang_node(),&val.tag);
  RW_ASSERT(RW_STATUS_SUCCESS == ret);

  struct confd_cs_node *cs_node =
      confd_find_cs_node_child(current_schema_node_, val.tag);
  RW_ASSERT (cs_node);

  val.v.type = C_XMLBEGIN;
  values_.push_back (val);

  current_schema_node_ = cs_node;

  return RW_XML_ACTION_NEXT;
}


rw_xml_next_action_t ConfdTLVBuilder::pop()
{
  confd_tag_value_t val;

  val.v.type = C_XMLEND;

  val.tag.tag = current_schema_node_->tag;
  val.tag.ns = current_schema_node_->ns;

  values_.push_back (val);

  current_schema_node_ = schema_stack_.top();
  schema_stack_.pop();

  return RW_XML_ACTION_NEXT;
}

size_t ConfdTLVBuilder::length()
{
  return values_.size();
}

size_t ConfdTLVBuilder::copy_and_destroy_tag_array (confd_tag_value_t *values)
{
  int i = 0;

  while (!values_.empty()) {
    values[i] = values_.front();
    values_.pop_front();
    i++;
  }

  return i;
}

rw_status_t
RwTLVAIterator::move_to_first_child()
{
  confd_tag_value_t *node = &root_[position_];
  RwTLVAIterator iter = *this;
  
  if (path_.size() == 0)
  {
    /* The "tree" is an array of children, in the constructor. The schema node
     * is set to the right level, ie to the schema node of the root. 
     */
    
    RW_ASSERT(position_ == 0);
    path_.push (0);
    cs_node_ = confd_find_cs_node_child(cs_node_, root_[position_].tag);
    RW_ASSERT(cs_node_);
    return RW_STATUS_SUCCESS;
  }
  
  if ((position_ == length_ - 1) || (node->v.type != C_XMLBEGIN)) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(position_ < length_);
  
  path_.push (position_);
  position_++;
  
  RW_ASSERT(cs_node_);
  cs_node_ = confd_find_cs_node_child(cs_node_, root_[position_].tag);
  RW_ASSERT(cs_node_);

  return RW_STATUS_SUCCESS;
}

rw_status_t
RwTLVAIterator::move_to_next_sibling()
{
  if (path_.size() == 0) {
    // The root has no siblings
    return RW_STATUS_FAILURE;
  }
    
  confd_tag_value_t *node = &root_[position_];
  struct confd_value *val = &node->v;
  if ((val->type == C_LIST) && (val->val.list.size > list_index_ + 1)) {
    list_index_++;
    RW_ASSERT(list_index_ < val->val.list.size);
    return RW_STATUS_SUCCESS;
  }
    
  size_t new_pos = position_ + 1;
  
  if (new_pos == length_) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(new_pos < length_);
  
  if (node->v.type != C_XMLBEGIN) {
    if (root_[new_pos].v.type == C_XMLEND) {
      return RW_STATUS_FAILURE;
    }
  } else  {
    uint8_t xml_begin_count = 1, xml_end_count = 0;
    
    for (; (new_pos < length_ - 1) && (xml_begin_count > xml_end_count);
         new_pos++) {
      
      switch (root_[new_pos].v.type) { 
        case C_XMLEND:
          xml_end_count++;
          if (xml_end_count < xml_begin_count) {
            continue;
          }

          RW_ASSERT(xml_end_count == xml_begin_count);
          if  (C_XMLEND == root_[new_pos + 1].v.type) {
            return RW_STATUS_FAILURE;
          }
          // new_pos is now at END, but the loop end increments
          break;

        case C_XMLBEGIN:
          xml_begin_count++;
          break;

        default:
          break;
      }
    }

    if (xml_begin_count != xml_end_count) {
      return RW_STATUS_FAILURE;
    }
  }
  
  if (new_pos == length_) {
    return RW_STATUS_FAILURE;
  }

  position_ = new_pos;
  list_index_ = 0;
  
  RW_ASSERT(cs_node_);
  RW_ASSERT(cs_node_->parent);
  cs_node_ = confd_find_cs_node_child(cs_node_->parent, root_[position_].tag);
  RW_ASSERT(cs_node_);
  
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwTLVAIterator::move_to_parent()
{
  if (!path_.size()) {
    return RW_STATUS_FAILURE;
  }

  position_ = path_.top();
  path_.pop();
  RW_ASSERT(cs_node_);
  RW_ASSERT(cs_node_->parent);
  cs_node_ = cs_node_->parent;

  return RW_STATUS_SUCCESS;
}

rw_status_t
RwTLVAIterator::get_value(rw_ylib_data_t* data) const
{
  data->type = RW_YLIB_DATA_TYPE_CONFD;
  rw_confd_value_t *v = &data->rw_confd;
  v->cs_node = cs_node_;
  v->value = nullptr;

  if (!path_.size()) {
    return RW_STATUS_SUCCESS;
  }
  
  struct confd_value *val = &root_[position_].v;

  if (val->type != C_LIST) {
    v->value = val;
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT(list_index_ < val->val.list.size);
  v->value = &val->val.list.ptr[list_index_];
  return RW_STATUS_SUCCESS;
}

const char*
RwTLVAIterator::get_yang_node_name() const
{
  return confd_hash2str (cs_node_->tag);
}

const char*
RwTLVAIterator::get_yang_ns() const
{
  return confd_hash2str (cs_node_->ns);
}

RwHKeyPathIterator::RwHKeyPathIterator()
{
}

RwHKeyPathIterator::RwHKeyPathIterator (confd_hkeypath_t *key)
    :h_key_path_(key)
{
  if (key) {
    depth_ = key->len;
  }
}

RwHKeyPathIterator::~RwHKeyPathIterator()
{
}

rw_status_t
RwHKeyPathIterator::move_to_first_child()
{
  if (!h_key_path_ || (0 == depth_)
      ||((depth_ != (size_t) h_key_path_->len) &&
         (C_XMLTAG != h_key_path_->v[depth_][0].type))) {
    return RW_STATUS_FAILURE;
  }

  if (position_) {
    // at a leaf, cannot have children
    return RW_STATUS_FAILURE;
  }
  // go down
  depth_--;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwHKeyPathIterator::move_to_next_sibling()
{
  if ((!h_key_path_) || (depth_ == (size_t) h_key_path_->len)) {
    return RW_STATUS_FAILURE;
  }

  if (C_XMLTAG == h_key_path_->v[depth_][0].type ) {
    return RW_STATUS_FAILURE;
  }

  if (C_NOEXISTS != h_key_path_->v[depth_][position_+1].type ) {
    position_++;
    return RW_STATUS_SUCCESS;
  }

  if (!depth_) {
    return RW_STATUS_FAILURE;
  }

  depth_ --;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwHKeyPathIterator::move_to_parent()
{
  RW_ASSERT(h_key_path_->len >= 0);
  if (!h_key_path_ || ((size_t) h_key_path_->len == depth_ )) {
    return RW_STATUS_FAILURE;
  }
  uint8_t i = depth_ + 1;
  while ((i < h_key_path_->len) &&
         h_key_path_->v[i][0].type != C_XMLTAG) {
    i++;
  }
  

  if (h_key_path_->len < i) {
    return RW_STATUS_FAILURE;    
  }
  
  depth_ = i;
  position_ = 0;
  return RW_STATUS_SUCCESS;
}

struct confd_cs_node *RwHKeyPathIterator::cs_node() const
{
  struct confd_cs_node *cs_node =
      confd_find_cs_node (h_key_path_, h_key_path_->len -  depth_);
  if (h_key_path_->v[depth_][0].type != C_XMLTAG) {
    RW_ASSERT(cs_node);
    RW_ASSERT(cs_node->info.keys);

    struct xml_tag tag = {0};
    tag.ns = cs_node->ns;
    tag.tag = cs_node->info.keys[position_];
    cs_node = confd_find_cs_node_child (cs_node, tag);
    RW_ASSERT(cs_node);    
  }

  return cs_node;
    
}
rw_status_t
RwHKeyPathIterator::get_value(rw_ylib_data_t* data) const
{
  data->type = RW_YLIB_DATA_TYPE_CONFD;
  rw_confd_value_t *v = &data->rw_confd;
  v->cs_node = cs_node();
  RW_ASSERT(v->cs_node);
  v->value = &h_key_path_->v[depth_][position_];
  
  return RW_STATUS_SUCCESS;
}

const char*
RwHKeyPathIterator::get_yang_node_name() const
{
  return confd_hash2str (cs_node()->tag);
}

const char*
RwHKeyPathIterator::get_yang_ns() const
{
  return confd_hash2str (cs_node()->ns);
}

rw_status_t
RwTLVListIterator::move_to_first_child()
{
  if (!elements_.size()) {
    return RW_STATUS_FAILURE;
  }
  
  if (position_->v.type != C_XMLBEGIN) {
    return RW_STATUS_FAILURE;
  }

  path_.push (position_);
  position_++;

  RW_ASSERT(elements_.end() != position_);
  RW_ASSERT(position_->v.type != C_XMLEND);

  return RW_STATUS_SUCCESS;
}


rw_status_t
RwTLVListIterator::move_to_next_sibling()
{
  tlv_list_iter_t new_pos = position_;

  struct confd_value *val = &new_pos->v;

  if ((val->type == C_LIST) && (val->val.list.size > list_index_ + 1)) {
    list_index_++;
    RW_ASSERT(list_index_ < val->val.list.size);
    return RW_STATUS_SUCCESS;
  }

  new_pos++;
  if (C_XMLBEGIN != position_->v.type) {
    if (C_XMLEND == new_pos->v.type) {
    return RW_STATUS_FAILURE;
  } else {
      // The old position was an XMLBEGIN type, which is a container or list.
      // move over any children - and subchildren
      uint8_t xml_begin_count = 1, xml_end_count = 0;
      for (;elements_.end() != new_pos; new_pos++) {
        switch (new_pos->v.type) {
          case C_XMLEND:
            xml_end_count++;
            if (xml_end_count < xml_begin_count) {
              continue;
            }
            
            RW_ASSERT(xml_end_count == xml_begin_count);
            break;
            
          case C_XMLBEGIN:
            xml_begin_count++;
            break;
            
          default:
            break;
        }
      }
      
      if (xml_begin_count != xml_end_count) {
        return RW_STATUS_FAILURE;
      }
    }
  }

  if ((elements_.end() == new_pos) || (C_XMLEND == new_pos->v.type)) {
    return RW_STATUS_FAILURE;
  }

  position_ = new_pos;
  list_index_ = 0;
  
  RW_ASSERT(cs_node_);
  RW_ASSERT(cs_node_->parent);

  cs_node_ = confd_find_cs_node_child(cs_node_->parent, position_->tag);
  RW_ASSERT(cs_node_);
  
  return RW_STATUS_SUCCESS;
  
}

rw_status_t
RwTLVListIterator::get_value(tlv_list_iter_t& value)
{
  if (position_ == elements_.end()) {
    return RW_STATUS_FAILURE;
  }
  value = position_;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwTLVListIterator::move_to_child(tlv_list_iter_t& child)
{
  
  if (elements_.end() == position_) {
    RW_ASSERT(!elements_.size());
  }
  
  path_.push(position_);
  position_ = child;
  
  return RW_STATUS_SUCCESS;
}


rw_status_t
RwTLVListIterator::move_to_parent()
{
  if (!path_.size()) {
    return RW_STATUS_FAILURE;
  }

  position_ = path_.top();
  path_.pop();
  RW_ASSERT(cs_node_);
  RW_ASSERT(cs_node_->parent);
  cs_node_ = cs_node_->parent;

  return RW_STATUS_SUCCESS;

}


const char *
RwTLVListIterator::get_yang_ns() const
{
  return confd_hash2str (cs_node_->ns);
}

const char *
RwTLVListIterator::get_yang_node_name() const
{
  return confd_hash2str (cs_node_->tag);
}


static
rw_tree_walker_status_t add_tlv_list_child (RwTLVListIterator *parent,
                                            ProtobufCFieldInfo *child_pb,
                                            const char *yang_node_name,
                                            const char *yang_ns,
                                            RwTLVListIterator::tlv_list_iter_t& child)
{
  // Find the child cs node
  const struct confd_cs_node *cs_node = parent->schema_node();
  RW_ASSERT(cs_node);
  rw_status_t rs; 
  struct xml_tag tag = {0};
  tag.tag = confd_str2hash (yang_node_name);
  tag.ns = confd_str2hash (yang_ns);

  cs_node = confd_find_cs_node_child (cs_node, tag);
  RW_ASSERT(cs_node);
  
  confd_tag_value_t child_tag_val = {0};
  child_tag_val.tag = tag;
  confd_value_t *child_val = &child_tag_val.v;
  
  const void *elt = child_pb->element;
  
  switch (cs_node->info.shallow_type) {
    case C_NOEXISTS:
    case C_STR:
    case C_SYMBOL:
    case C_QNAME:      
    case C_DATETIME:
    case C_DATE:
    case C_TIME:
    case C_DURATION:
    case C_BIT32:
    case C_BIT64:
    case C_OBJECTREF: 
    case C_UNION: 
    case C_PTR: 
    case C_CDBBEGIN: 
    case C_OID:
    case C_DEFAULT:
    case C_IDENTITYREF:
    case C_XMLBEGINDEL:
    case C_DQUAD:
    case C_XMLEND:
    case C_MAXTYPE:
      RW_CRASH();
      break;
      
    case C_XMLTAG:      
      break;
    case C_BUF: {
      std::string str;
      rs = get_string_value(child_pb, str);
      if (RW_STATUS_SUCCESS != rs) {
        CONFD_SET_BUF(child_val, (unsigned char *) strdup (str.c_str()), str.length());
      }
    }
      break;
    case C_INT8:
      CONFD_SET_INT8(child_val, (int8_t) (*(int32_t *)elt));
      break;
    case C_INT16:
      CONFD_SET_INT16(child_val, (int16_t) (*(int32_t *)elt));
      break;
    case C_INT32:
      CONFD_SET_INT32(child_val, (*(int32_t *)elt));
      break;
    case C_INT64:
      CONFD_SET_INT64(child_val, (*(int64_t *)elt));
      break;
    case C_UINT8:
      CONFD_SET_UINT8(child_val, (uint8_t) (*(uint32_t *)elt));
      break;      
    case C_UINT16:
      CONFD_SET_UINT16(child_val, (uint16_t) (*(uint32_t *)elt));
      break;
    case C_UINT32:
      CONFD_SET_UINT32(child_val, (*(uint32_t *)elt));
      break;
    case C_UINT64:
      CONFD_SET_UINT64(child_val, (*(uint64_t *)elt));
      break;
    case C_DOUBLE:
    case C_IPV4:
    case C_IPV6:
    case C_IPV4PREFIX:
    case C_IPV6PREFIX:
    case C_DECIMAL64:
    case C_IPV4_AND_PLEN:
    case C_IPV6_AND_PLEN:
    case C_BINARY:
    case C_HEXSTR:
      break;
    case C_BOOL:
      CONFD_SET_BOOL (child_val, *(protobuf_c_boolean *)elt);
      break;
    case C_ENUM_VALUE:
      CONFD_SET_ENUM_VALUE(child_val,(*(uint32_t *)elt));
      break;
    case C_LIST: {
      struct confd_type *ct = confd_get_leaf_list_type ((confd_cs_node*) cs_node);
      RW_ASSERT(ct);      
    }
      
      break;
    case C_XMLBEGIN:
      break;
  }
  return RW_TREE_WALKER_SUCCESS;
  
}

rw_status_t RwTLVListIterator::get_value (rw_ylib_data_t* data) const
{
  data->type = RW_YLIB_DATA_TYPE_CONFD;
  rw_confd_value_t *v = &data->rw_confd;
  v->cs_node = cs_node_;
  v->value = nullptr;
  
  struct confd_value *val = &position_->v;

  if (val->type != C_LIST) {
    v->value = val;
    return RW_STATUS_SUCCESS;
  }

  RW_ASSERT(list_index_ < val->val.list.size);
  v->value = &val->val.list.ptr[list_index_];
  return RW_STATUS_SUCCESS;

}

namespace rw_yang {

rw_status_t get_string_value (const RwTLVListIterator *iter,
                              std::string& str)
{
  rw_ylib_data_t val;
  
  rw_status_t rs = iter->get_value(&val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  return get_string_value(&val.rw_confd, str);
}


rw_tree_walker_status_t add_xml_child (XMLNode *parent,
                                       rw_confd_value_t *confd,
                                       XMLNode*& child)
{
  confd_cs_node *cs_node = confd->cs_node;
  confd_value_t *child_src = confd->value;
  const char *name = confd_hash2str (confd->cs_node->tag);
  const char *ns = confd_hash2str (confd->cs_node->ns);

  YangNode *child_yn = nullptr;
  child = nullptr;

  YangNode *yn = parent->get_yang_node();
  if (!yn) {
    return RW_TREE_WALKER_FAILURE;
  }

  child_yn = yn->search_child (name, ns);
  if (!child_yn) {
    return RW_TREE_WALKER_FAILURE;
  }

  if (!child_yn->is_leafy()) {
    child = parent->add_child(child_yn);
    if (child) {
      return RW_TREE_WALKER_SUCCESS;
    }

    return RW_TREE_WALKER_FAILURE;
  }

  char value[1024];
  char *val_p = nullptr;
  
  switch (child_src->type) {
    default:
    case C_QNAME: 
    case C_DATETIME: 
    case C_DATE: 
    case C_TIME: 
    case C_DURATION:
    case C_NOEXISTS:
    case C_STR:
    case C_SYMBOL:
    case C_BIT32: 
    case C_BIT64:
    case C_XMLBEGIN:  
    case C_XMLEND: 
    case C_OBJECTREF: 
    case C_UNION: 
    case C_PTR: 
    case C_CDBBEGIN: 
    case C_OID: 
    case C_DEFAULT: 
    case C_IDENTITYREF: 
    case C_XMLBEGINDEL: 
    case C_DQUAD: 
    case C_HEXSTR: 

      RW_CRASH();
    break;
    
    case C_XMLTAG:
      break;

    case C_BUF: 
    case C_INT8: 
    case C_INT16: 
    case C_INT32: 
    case C_INT64: 
    case C_UINT8: 
    case C_UINT16: 
    case C_UINT32: 
    case C_UINT64: 
    case C_DOUBLE: 
    case C_BOOL: 
    case C_ENUM_VALUE:
    case C_BINARY: 
    case C_DECIMAL64: 
    case C_IPV4: 
    case C_IPV6: 
    case C_IPV4PREFIX:
    case C_IPV4_AND_PLEN: 
    case C_IPV6_AND_PLEN: 
    case C_IPV6PREFIX: {

      struct confd_type *ct = cs_node->info.type;
      
      int len = confd_val2str (ct, child_src, value, sizeof (value));
      if (len < 0) {
        return RW_TREE_WALKER_FAILURE;
      }
      val_p = &value[0];
    }
      
      break;
    case C_LIST: {
      
      // ATTN: ATTN:
      // This code is inefficient. TAIL-F has been asked what the best method
      // for doing this is. Once TAILF responds, change this code.

      // ATTN: ATTN:
      // This also needs to change when protobuf representation of a leaf list
      // transitions to becoming a list of single elements
      

      struct confd_type *ct = confd_get_leaf_list_type (cs_node);
      RW_ASSERT(ct);
      
      int len = confd_val2str (ct, child_src, value, sizeof (value));
      if (len < 0) {
        return RW_TREE_WALKER_FAILURE;
      }
      val_p = &value[0];
    }
      break;
  }
  child = parent->add_child (child_yn, val_p);
  if (child) {
    return RW_TREE_WALKER_SUCCESS;
  }

  return RW_TREE_WALKER_FAILURE;
}


rw_tree_walker_status_t add_pbcm_child (ProtobufCMessage *parent,
                                        rw_confd_value_t *confd,
                                        ProtobufCFieldInfo *child_tgt,
                                        bool find_field)
{

  if (find_field) {
    memset (child_tgt, 0, sizeof (ProtobufCFieldInfo));
    const char *name = confd_hash2str (confd->cs_node->tag);
    const char *ns = confd_hash2str (confd->cs_node->ns);
    
    child_tgt->fdesc = protobuf_c_message_descriptor_get_field_by_yang (
        parent->descriptor, name, ns);
  }

  child_tgt->element = nullptr;
  child_tgt->len = 0;
  
  RW_ASSERT(child_tgt->fdesc);


  confd_cs_node *cs_node = confd->cs_node;
  confd_value_t *child_src = confd->value;

  uint32_t v_uint32 = 0;
  int32_t v_int32 = 0;
  uint64_t v_uint64 = 0;
  int64_t v_int64 = 0;
  double v_double = 0.0;
  protobuf_c_boolean v_bool = 0;

  switch (child_src->type) {
    default:
    case C_NOEXISTS:
    case C_STR:
    case C_SYMBOL: 
      RW_CRASH();
    break;
    
    case C_XMLTAG: 
      // Empty in YANG, bool in pbcm
      v_bool = 1;
      child_tgt->element = &v_bool;      
      break;

    case C_BUF: 
      child_tgt->element = CONFD_GET_BUFPTR(child_src);
      child_tgt->len = CONFD_GET_BUFSIZE(child_src);
      break;
      
    case C_INT8: 
      v_int32 = (int32_t)CONFD_GET_INT8 (child_src);
      child_tgt->element = &v_int32;
      break;
      
    case C_INT16: 
      v_int32 = (int32_t)CONFD_GET_INT16 (child_src);
      child_tgt->element = &v_int32;
      break;

    case C_INT32: 
      v_int32 = CONFD_GET_INT32 (child_src);
      child_tgt->element = &v_int32;
      break;
      
    case C_INT64: 
      v_int64 = CONFD_GET_INT64 (child_src);
      child_tgt->element = &v_int64;
      break;
      
    case C_UINT8: 
      v_uint32 = (uint32_t)CONFD_GET_UINT8 (child_src);
      child_tgt->element = &v_uint32;
      break;
 
    case C_UINT16: 
      v_uint32 = (uint32_t)CONFD_GET_UINT16 (child_src);
      child_tgt->element = &v_uint32;
      break;
      
    case C_UINT32: 
      v_uint32 = CONFD_GET_UINT32 (child_src);
      child_tgt->element = &v_uint32;
      break;
      
    case C_UINT64: 
      v_uint64 = CONFD_GET_UINT64 (child_src);
      child_tgt->element = &v_uint64;
      break;
      
    case C_DOUBLE: 
      v_double = CONFD_GET_DOUBLE (child_src);
      child_tgt->element = &v_double;
      break;
      
    case C_BOOL: 
      v_bool = CONFD_GET_BOOL(child_src);
      child_tgt->element = &v_bool;
      break;
      
    case C_QNAME: 
      RW_CRASH();
      break;
      
    case C_DATETIME: 
      RW_CRASH();
      break;
      
    case C_DATE: 
      RW_CRASH();
      break;
      
    case C_TIME: 
      RW_CRASH();
      break;
 
    case C_DURATION:
      RW_CRASH();
      break;
    case C_ENUM_VALUE: 
      v_uint32 = (uint32_t) CONFD_GET_ENUM_VALUE (child_src);
      child_tgt->element = &v_uint32;
      break;
      
    case C_BIT32: 
      RW_CRASH();
      break;
    case C_BIT64: 
      RW_CRASH();
      break;
      
    case C_LIST: {
      
      // ATTN: ATTN:
      // This code is inefficient. TAIL-F has been asked what the best method
      // for doing this is. Once TAILF responds, change this code.

      // ATTN: ATTN:
      // This also needs to change when protobuf representation of a leaf list
      // transitions to becoming a list of single elements
      
      char value[1024];
      struct confd_type *ct = confd_get_leaf_list_type (cs_node);
      RW_ASSERT(ct);
      
      int len = confd_val2str (ct, child_src, value, sizeof (value));
      if (len < 0) {
        return RW_TREE_WALKER_FAILURE;
      }
      
      bool ok = protobuf_c_message_set_field_text_value (
          nullptr, parent, child_tgt->fdesc, value, len);
      if (ok) {
        return RW_TREE_WALKER_SUCCESS;
      } else {
        return RW_TREE_WALKER_FAILURE;
      }
    }
      break;
    case C_XMLBEGIN:  {
      // Add a message 
      bool ok = protobuf_c_message_set_field_message (
          nullptr, parent, child_tgt->fdesc, (ProtobufCMessage **) &child_tgt->element);
      if (ok) {
        return RW_TREE_WALKER_SUCCESS;
      } else {
        return RW_TREE_WALKER_FAILURE;
      }
    }          
      break;
      
    case C_XMLEND: 
      RW_CRASH();
      break;
    case C_OBJECTREF: 
      RW_CRASH();
      break;
    case C_UNION: 
      RW_CRASH();
      break;
    case C_PTR: 
      RW_CRASH();
      break;
 
    case C_CDBBEGIN: 
      RW_CRASH();
      break;
    case C_OID: 
      RW_CRASH();
      break;
    case C_BINARY: 
      child_tgt->element = CONFD_GET_BINARY_PTR(child_src);
      child_tgt->len = CONFD_GET_BINARY_SIZE(child_src);
      break;
      
    case C_DECIMAL64: 
    case C_IPV4: 
    case C_IPV6: 
    case C_IPV4PREFIX:
    case C_IPV4_AND_PLEN: 
    case C_IPV6_AND_PLEN: 
    case C_IPV6PREFIX: {
      
      char value[1024];
      struct confd_type *ct = cs_node->info.type;

      int len = confd_val2str (ct, child_src, value, sizeof (value));
      if (len < 0) {
        return RW_TREE_WALKER_FAILURE;
      }
      
      bool ok = protobuf_c_message_set_field_text_value (
          nullptr, parent, child_tgt->fdesc, value, len);
      if (ok) {
        return RW_TREE_WALKER_SUCCESS;
      } else {
        return RW_TREE_WALKER_FAILURE;
      }
    }
      break;
    case C_DEFAULT: 
      RW_CRASH();
      break;
      
    case C_IDENTITYREF: 
      RW_CRASH();
      break;
    case C_XMLBEGINDEL: 
      RW_CRASH();
      break;
 
    case C_DQUAD: 
      RW_CRASH();
      break;
    case C_HEXSTR: 
      RW_CRASH();
      break;
  }

  bool ok = protobuf_c_message_set_field (nullptr, parent, child_tgt);
  if (ok) {
    return RW_TREE_WALKER_SUCCESS;
  }

  return RW_TREE_WALKER_FAILURE;

  
}



rw_status_t get_string_value (rw_confd_value_t *v,
                              std::string& str)
{
  str.clear();
  if (v->value) {
    if (v->cs_node->info.type) {
      
      char value[1024];
      struct confd_type *ct = v->cs_node->info.type;

      if (C_LIST == v->cs_node->info.shallow_type) {
        ct = confd_get_leaf_list_type (v->cs_node);
        RW_ASSERT(ct);
      }
      int len = confd_val2str (ct, v->value,value, sizeof (value));
      if (len < 0) {
        return RW_STATUS_FAILURE;
      }
      RW_ASSERT ((size_t) len < sizeof (value));
      str = value;
    } else {
      str = confd_hash2str(v->cs_node->tag);
    }
  }
  return RW_STATUS_SUCCESS;
}


rw_status_t get_string_value (const RwTLVAIterator *iter,
                              std::string& str)
{
  rw_ylib_data_t val;
  
  rw_status_t rs = iter->get_value(&val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  return get_string_value(&val.rw_confd, str);
}


// Copiers for PBCM iterator
rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                             RwTLVAIterator* child_iter)
{
  ProtobufCMessage *parent;
  rw_status_t rs = parent_iter->get_value (parent);  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (nullptr == parent) {
    return RW_TREE_WALKER_FAILURE;
  }

  ProtobufCFieldInfo child;
  memset (&child, 0, sizeof (child));
  
  rw_ylib_data_t tlv_child;
  rs = child_iter->get_value(&tlv_child);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(RW_YLIB_DATA_TYPE_CONFD == tlv_child.type);

  rw_tree_walker_status_t rt =
      add_pbcm_child (parent, &tlv_child.rw_confd, &child, true);


    if (rt != RW_TREE_WALKER_SUCCESS)  {
    // no need to move to a child
    return rt;
  }
  
  // Move to the last child of this type for now? this wont work when
  // things are sorted properly for leaf lists, and lists?
  
  rs = parent_iter->move_to_child (child.fdesc,RW_YLIB_LIST_POSITION_LAST);
  if (RW_STATUS_SUCCESS != rs) {
    return RW_TREE_WALKER_FAILURE;
  }
  return rt;
}

// Copiers for Schema Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwSchemaTreeIterator* parent_iter,
                                                             RwHKeyPathIterator* child_iter)
{

  rw_ylib_data_t tlv_child;
  
  rw_status_t rs = child_iter->get_value(&tlv_child);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  
  rw_confd_value_t *v = &tlv_child.rw_confd;

  const char *name = confd_hash2str(v->cs_node->tag);
  const char *ns = confd_hash2str (v->cs_node->ns);

  rs = RW_STATUS_FAILURE;
  
  if (v->value->type == C_XMLTAG) {
    
    rs = parent_iter->move_to_first_child();
    if ((RW_STATUS_SUCCESS != rs) ||
        strcmp(parent_iter->get_yang_node_name(), name) ||
        strcmp(parent_iter->get_yang_ns(), ns)) {
      return RW_TREE_WALKER_FAILURE;
    }
    
    return RW_TREE_WALKER_SUCCESS;
  }

  // Not an XML tag - have to add a value
  rw_keyspec_entry_t* parent = nullptr;
  rs = parent_iter->get_value(parent);

  ProtobufCFieldInfo child;
  memset (&child, 0, sizeof (child));
  
  const ProtobufCFieldDescriptor *k_desc = nullptr;
  rs = rw_keyspec_path_key_descriptors_by_yang_name (parent, name, ns,
                                                     &k_desc, &child.fdesc);
  RW_ASSERT(RW_STATUS_SUCCESS == rs) ;
  RW_ASSERT(k_desc);
  RW_ASSERT(k_desc->quantifier_offset);
  RW_ASSERT(child.fdesc);
  
  rw_tree_walker_status_t rt = add_pbcm_child (
      STRUCT_MEMBER_PTR (ProtobufCMessage, parent,k_desc->offset),
      v, &child,false);
  
  if (rt == RW_TREE_WALKER_SUCCESS) {
    // Mark the key as present in the path -
    // ATTN - Maybe all this needs to be consolidated as a single call into
    // rw_keyspec
    *(STRUCT_MEMBER_PTR (uint32_t, parent, k_desc->quantifier_offset)) = 1;
    rs = parent_iter->move_to_child(k_desc);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
  }
  return rt;
}



// Copiers for Confd TLV List Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwTLVListIterator* parent_iter,
                                                               RwPbcmTreeIterator* child_iter)
{
  rw_ylib_data_t child_val;
  
  rw_status_t rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  RwTLVListIterator::tlv_list_iter_t parent,child;
  rs = parent_iter->get_value(parent);

  if (rs != RW_STATUS_SUCCESS) {
    return RW_TREE_WALKER_FAILURE;
  }

  // Adding a child needs zero (leaf list with existing confd TLV), one
  // (leaf with no existing confd TLV) or two (containers and lists) confd
  // TLV value

  rw_tree_walker_status_t rt = RW_TREE_WALKER_FAILURE;
RW_ASSERT(RW_YLIB_DATA_TYPE_PB == child_val.type);
 rt  = add_tlv_list_child (parent_iter, &child_val.rw_pb,
                               child_iter->get_yang_node_name(),
                               child_iter->get_yang_ns(),
                               child);
  if (rt != RW_TREE_WALKER_SUCCESS)  {
    // no need to move to a child
    return rt;
  }

  rs = parent_iter->move_to_child (child);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  return rt;

}

}


