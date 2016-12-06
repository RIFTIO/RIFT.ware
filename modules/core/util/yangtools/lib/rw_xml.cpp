
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
 * @file rw_xml.cpp
 * @author Vinod Kamalaraj
 * @date 2014/04/11
 * @brief Implementation of generic RW XML library functions
 */

#include "rw_xml.h"
#include <iostream>
#include <inttypes.h>
#include <boost/scoped_array.hpp>

using namespace rw_yang;

const char rw_xml_namespace_none[2] = {'\1'}; // intentionally illegal XML char, and intentially not null string
const char rw_xml_namespace_any[2] = {'\1'}; // intentionally illegal XML char, and intentially not null string
const char rw_xml_namespace_node[2] = {'\1'}; // intentionally illegal XML char, and intentially not null string
const uint16_t RW_XML_TMP_BUFFER_LEN=255;

rw_status_t rw_yang_netconf_op_status_to_rw_status(
  rw_yang_netconf_op_status_t ncs)
{
  switch (ncs) {
    case RW_YANG_NETCONF_OP_STATUS_NULL:
      // ATTN: Prefer to return something else for this one?
      break;

    case RW_YANG_NETCONF_OP_STATUS_OK:
      return RW_STATUS_SUCCESS;

    case RW_YANG_NETCONF_OP_STATUS_IN_USE:
    case RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS:
      return RW_STATUS_EXISTS;

    case RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE:
    case RW_YANG_NETCONF_OP_STATUS_MISSING_ATTRIBUTE:
    case RW_YANG_NETCONF_OP_STATUS_MISSING_ELEMENT:
    case RW_YANG_NETCONF_OP_STATUS_DATA_MISSING:
      return RW_STATUS_NOTFOUND;

    case RW_YANG_NETCONF_OP_STATUS_TOO_BIG:
    case RW_YANG_NETCONF_OP_STATUS_BAD_ATTRIBUTE:
    case RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT:
      return RW_STATUS_OUT_OF_BOUNDS;

    case RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ATTRIBUTE:
    case RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ELEMENT:
    case RW_YANG_NETCONF_OP_STATUS_UNKNOWN_NAMESPACE:
    case RW_YANG_NETCONF_OP_STATUS_ACCESS_DENIED:
    case RW_YANG_NETCONF_OP_STATUS_LOCK_DENIED:
    case RW_YANG_NETCONF_OP_STATUS_RESOURCE_DENIED:
    case RW_YANG_NETCONF_OP_STATUS_ROLLBACK_FAILED:
    case RW_YANG_NETCONF_OP_STATUS_OPERATION_NOT_SUPPORTED:
    case RW_YANG_NETCONF_OP_STATUS_MALFORMED_MESSAGE:
    case RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED:
    case RW_YANG_NETCONF_OP_STATUS_FAILED:
      return RW_STATUS_FAILURE;

    default:
      break;
  }
  RW_ASSERT_NOT_REACHED();
  return RW_STATUS_FAILURE;
}

// ATTN: Why is this not a member function?!?!
static bool rw_pb_is_field_present(
  const ProtobufCFieldDescriptor* parent_fdesc,
  const ProtobufCMessage* pbcm,
  const char* field_name)
{
  if (parent_fdesc->type != PROTOBUF_C_TYPE_MESSAGE) {
    return false;
  }

  const ProtobufCFieldDescriptor *child_fdesc
    = protobuf_c_message_descriptor_get_field_by_name( pbcm->descriptor, field_name );
  if (!child_fdesc) {
    return false;
  }

  if (child_fdesc->type == PROTOBUF_C_TYPE_MESSAGE) {
    // This is used to find key type as of now. fail
    return false;
  }

  // ATTN: This whole API should probably just have a protobuf_c implementation
  ProtobufCFieldInfo finfo;
  return protobuf_c_message_get_field_instance(
    NULL/*instance*/,
    pbcm,
    child_fdesc,
    0/*repeated_index*/,
    &finfo);
}

// ATTN: This should be a member function
static rw_status_t rw_pb_get_field_value_str(
  YangNode *ynode,
  char *value_str,
  size_t *value_str_len,
  const ProtobufCMessage* pbcm,
  const char *field_name)
{
  RW_ASSERT(ynode);
  RW_ASSERT(value_str);
  RW_ASSERT(value_str_len);
  RW_ASSERT(pbcm);
  RW_ASSERT(field_name);

  const ProtobufCFieldDescriptor *fdesc
    = protobuf_c_message_descriptor_get_field_by_name(pbcm->descriptor, field_name);
  if (!fdesc) {
    return RW_STATUS_FAILURE;
  }
  if (   fdesc->type == PROTOBUF_C_TYPE_MESSAGE
      || fdesc->label == PROTOBUF_C_LABEL_REPEATED) {
    // ATTN: Will need to support msg when yang:union maps to msg
    // ATTN: If this was a member function, we could record an error here...
    return RW_STATUS_FAILURE;
  }

  ProtobufCFieldInfo finfo = {};
  bool ok = protobuf_c_message_get_field_instance(
    NULL/*instance*/, pbcm, fdesc, 0/*repeated_index*/, &finfo);
  if (!ok) {
    // ATTN: What about default value? Should this return ok for assumed default value?
    return RW_STATUS_FAILURE;
  }

  //ATTN:- decimal64 types require special handling here.

  PROTOBUF_C_CHAR_BUF_DECL_INIT(NULL, cbuf);
  if (!protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
    return RW_STATUS_FAILURE;
  }

  rw_status_t status = RW_STATUS_SUCCESS;
  int bytes = snprintf( value_str, *value_str_len, "%s", cbuf.buffer );
  if (bytes < 0 || bytes >= (int)*value_str_len) {
    status = RW_STATUS_FAILURE;
  } else {
    *value_str_len = bytes;
  }

  PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  return status;
}

/* ATTN: This should be a member function of something */
/*!
 * Set the field described by pbcfd in message message to the value in the XML Node
 */
static rw_yang_netconf_op_status_t
set_pb_field_from_xml(
  ProtobufCInstance* instance,
  ProtobufCMessage *message,
  const ProtobufCFieldDescriptor *pbcfd,
  XMLNode *node)
{
  YangNode *yn = node->get_yang_node();
  RW_ASSERT(yn);
  RW_ASSERT(yn->is_leafy());

  switch (yn->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_ANYXML:
      RW_XML_NODE_ERROR_STRING (node, "Unsupported type");
      return RW_YANG_NETCONF_OP_STATUS_OPERATION_NOT_SUPPORTED;
    default:
      break;
  }

  YangType* ytype = yn->get_type();
  RW_ASSERT(ytype);
  rw_yang_leaf_type_t leaf_type = ytype->get_leaf_type();
  std::string value = node->get_text_value();
  protobuf_c_boolean ok = true;

  if (!value.length() && leaf_type != RW_YANG_LEAF_TYPE_EMPTY) {
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }

  switch (leaf_type) {
    case RW_YANG_LEAF_TYPE_EMPTY:
      // Special-case empty leaf: set the value to true.
      value = "true";
      break;
    case RW_YANG_LEAF_TYPE_ENUM: {
      // Need to swap the yang value for the protobuf value.
      const char* yval = protobuf_c_message_get_pb_enum (pbcfd, value.c_str());
      if (!yval || yval[0] == '\0') {
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
      value = yval;
      break;
    }

    case RW_YANG_LEAF_TYPE_BINARY: {
      /* For binary fields, the XML string is the base64 encoding of the
         binary data, decode and set it to protobuf. */
      auto bin_len = rw_yang_base64_get_decoded_len(value.c_str(),
                                                    value.length());
      boost::scoped_array<char> bin_data(new char[bin_len+4]);
      // ATTN: Check the return value.
      rw_yang_base64_decode(value.c_str(), value.length(),
                            bin_data.get(), bin_len+4);
      // set the binary value.
      ok = protobuf_c_message_set_field_text_value(instance, message, pbcfd, bin_data.get(), bin_len);
      if (ok) {
        return RW_YANG_NETCONF_OP_STATUS_OK;
      } else {
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
    }

    default:
      // nothing to do by default
      break;
  }
  RW_ASSERT(RW_YANG_LEAF_TYPE_BINARY != leaf_type);
  if (value.length()) {
    // PB2XML seems to have empty leaf specificed for strings generated in automation
    ok = protobuf_c_message_set_field_text_value(instance, message, pbcfd,
                                                 value.c_str(), value.length());
  }
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

/*****************************************************************************/
// Default implementations for node APIs.

XMLNodeIter XMLNode::child_begin()
{
  return XMLNodeIter(get_first_child());
}

XMLNodeIter XMLNode::child_end()
{
  return XMLNodeIter();
}

// ATTN: Define common implementation of is_equal()


rw_yang_netconf_op_status_t XMLNode::validate()
{
  // ATTN: Define common implementation of validate()!
  YangNode* yn = get_descend_yang_node();

  if (nullptr == yn) {
    return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
  }

  switch (yn->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_LIST: {
      // Basic validation for Lists

      for (YangKeyIter yki=yn->key_begin(); yki != yn->key_end(); yki++) {

        RW_ASSERT (nullptr != yki->get_key_node());

        // ATTN: This calculation is loop invariant, per key!
        XMLNode* node = find(yki->get_key_node()->get_name(),yki->get_key_node()->get_ns());

        if (nullptr == node) {
          // the input node should have all key nodes present.
          // The input is invalid
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
      }
      return RW_YANG_NETCONF_OP_STATUS_OK;
    }
    break;

    default:
      return RW_YANG_NETCONF_OP_STATUS_OK;
  }
  RW_ASSERT_NOT_REACHED();
}


bool XMLNode::is_leaf_node()
{
  return child_begin() == child_end();
}


XMLNode* XMLNode::find_enum(
    const char* local_name,
    const char* ns,
    uint32_t value)
{
  YangNode* my_yang_node = get_descend_yang_node();
  std::stringstream log_strm;

  log_strm << "Finding enumeration for "<< local_name << " in name space "<< ns << value;
  std::string log_str = "Finding enumeration for ";

  if (nullptr == my_yang_node) {
    log_strm << " No yang node ";
    RW_XML_NODE_ERROR (this, log_strm);
    return nullptr;
  }

  YangNode *child_yang = my_yang_node->search_child (local_name, ns);
  if (nullptr == child_yang) {
    log_strm << "No child yang found";
    RW_XML_NODE_ERROR (this, log_strm);
    return nullptr;
  }

  const char *text_val = child_yang->get_enum_string(value).c_str();
  return find_value (local_name, text_val, ns);

}

rw_yang_netconf_op_status_t XMLNode::get_specific_keyspec (const rw_keyspec_path_t* in,
                                                           rw_keyspec_path_t*& out)
{

  YangModel *ym = get_manager().get_yang_model();
  RW_ASSERT(ym);

  const rw_yang_pb_schema_t* ypbc_schema = ym->get_ypbc_schema();
  if (!ypbc_schema) {
    RW_XML_NODE_ERROR_STRING (this, "No Message schema available at the schema level");
    return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
  }

  if (rw_keyspec_path_find_spec_ks (in, NULL, ypbc_schema, &out) != RW_STATUS_SUCCESS) {
    RW_XML_NODE_ERROR_STRING (this, "Failed generating specific keyspec");
    return RW_YANG_NETCONF_OP_STATUS_FAILED;
  }

  return RW_YANG_NETCONF_OP_STATUS_OK;
}

rw_yang_netconf_op_status_t XMLNode::find(
  const rw_keyspec_path_t *keyspec,
  std::list<XMLNode*>& nodes)
{

  XMLNode *node = this;
  rw_keyspec_path_t *spec_ks = nullptr;
  char keyspec_debug[RW_XML_TMP_BUFFER_LEN];
  YangModel *ym = get_manager().get_yang_model();

  // to generic and then to specific - the specific
  rw_keyspec_path_t *gen = rw_keyspec_path_create_dup_of_type(keyspec, nullptr ,
                                                              &rw_schema_path_spec__descriptor);

  rw_yang_netconf_op_status_t ncrs = get_specific_keyspec(gen, spec_ks);
  RW_ASSERT(RW_YANG_NETCONF_OP_STATUS_OK == ncrs);
  RW_ASSERT(spec_ks);

  // Manage keyspec resource by unique_ptr
  UniquePtrKeySpecPath::uptr_t gen_uptr(gen);
  UniquePtrKeySpecPath::uptr_t spec_ks_uptr(spec_ks);

  rw_keyspec_path_get_print_buffer (spec_ks, NULL, ym->get_ypbc_schema(), keyspec_debug, sizeof (keyspec_debug));
  std::stringstream log_strm;

  log_strm << " Keyspec "<< keyspec_debug;
  RW_XML_NODE_DEBUG (this, log_strm);


  const ProtobufCMessageDescriptor *desc = ((ProtobufCMessage *)spec_ks)->descriptor;
  char *p_value = (char *)spec_ks;
  YangNode *yn = nullptr;
  size_t offset = 0;


  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  RW_ASSERT(fld);
  RW_ASSERT(fld->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(*(p_value + fld->quantifier_offset))

  p_value += fld->offset;

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;

  for (size_t path_index = 0; path_index < desc->n_fields; path_index++){
      fld = &desc->fields[path_index];
    if (fld->id < RW_SCHEMA_TAG_PATH_ENTRY_START ||
        fld->id > RW_SCHEMA_TAG_PATH_ENTRY_END) {
      // not a path entry..
      offset++;
      continue;
    }

    RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);
    RW_ASSERT (fld->label == PROTOBUF_C_LABEL_OPTIONAL); // path entries now optional
    RwSchemaElementId *elem_id = nullptr;
    const ProtobufCMessageDescriptor * path_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;

    char *path_v = p_value + fld->offset;

    if (path_desc == &rw_schema_path_entry__descriptor) {
      // Currently support only one level of leaf
      RW_ASSERT(desc->n_fields == path_index + 1);
      RW_ASSERT(yn);

      RwSchemaPathEntry **entry = (RwSchemaPathEntry **) path_v;

      if (nullptr == (*entry)) {
        continue;
      }

      elem_id = (*entry)->element_id;

    } else {
      fld = protobuf_c_message_descriptor_get_field_by_name (path_desc, "element_id");
      RW_ASSERT (fld);

      elem_id = (RwSchemaElementId *)(path_v + fld->offset);
    }

    if (!yn) {
      yn = ym->search_node (elem_id->system_ns_id, elem_id->element_tag);
      /* If a fake <data> node has not been added to the dom, the first
         node in the DOM might actually be the same as the child of the
         root of the module, ie, at the same level as this yang node. In
         this case, continue */

      if ((nullptr != yn) && (node->get_local_name() == yn->get_name()) &&
          (node->get_name_space() == yn->get_ns())){
        continue;
      }
    } else {
      yn = yn->search_child (elem_id->system_ns_id, elem_id->element_tag);
    }


    if (nullptr == yn) {
      log_strm << " No yangNode for " << elem_id->system_ns_id << " " <<elem_id->element_tag;
      RW_XML_NODE_ERROR (this, log_strm);
      return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
    }

    if (!yn->is_listy()) {
      XMLNode *child = node->find(yn->get_name(), yn->get_ns());
      if (nullptr == child) {

        RW_XML_NODE_DEBUG (this, log_strm);

        return RW_YANG_NETCONF_OP_STATUS_OK;
      }
      node = child;
    }
    else {
      bool is_wildcard = rw_keyspec_path_has_wildcards_for_depth(spec_ks, path_index - offset + 1);
      XMLNode *child = node->find_list_by_key(yn, (ProtobufCMessage *)path_v, &nodes, is_wildcard);

      if (nullptr == child) {
        return RW_YANG_NETCONF_OP_STATUS_OK;
      }

      if (is_wildcard) {
        return RW_YANG_NETCONF_OP_STATUS_OK;
      }

      node = child;
    }
  }
  nodes.push_back (node);
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

XMLNode* XMLNode::find_list_by_key(YangNode *list_ynode,
                                   ProtobufCMessage *message,
                                   std::list<XMLNode*> *nodes,
                                   bool is_wildcard)
{
  // Process the keys present
  std::stringstream log_strm;

  const ProtobufCMessageDescriptor *p_desc = message->descriptor;
  char* path_v = reinterpret_cast<char  *>(message); // ATTN: wtf?
  XMLNode *candidate = find(list_ynode->get_name(), list_ynode->get_ns());

  bool found = false;

  while ((candidate != nullptr) && !found) {
    found = true;

    for (size_t key_index = 0; key_index < p_desc->n_fields; key_index++) {

      const  ProtobufCFieldDescriptor *fld = &p_desc->fields[key_index];

      if (RW_SCHEMA_TAG_ELEMENT_KEY_START > fld->id ||
          RW_SCHEMA_TAG_ELEMENT_KEY_END < fld->id) {
        // not a key.
        continue;
      }

      RW_ASSERT(fld->label == PROTOBUF_C_LABEL_OPTIONAL);

      const bool *qfier = (const bool*)(const char*)message + fld->quantifier_offset;

      RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);

      const ProtobufCMessageDescriptor *key_desc =
          (const ProtobufCMessageDescriptor *) fld->descriptor;

      char *key_v = path_v + fld->offset;

      for (size_t fld_index = 0; fld_index < key_desc->n_fields; fld_index++) {
        // ATTN: A rather suspicious test. nothing else helps..
        fld = &key_desc->fields[fld_index];
        if (fld->id == 1) {
          continue;
        }

        YangNode *k_node = list_ynode->search_child_by_mangled_name(fld->name);
        RW_ASSERT (k_node);

        char value_str [RW_PB_MAX_STR_SIZE];
        size_t value_str_len = RW_PB_MAX_STR_SIZE;

        if (is_wildcard && !*qfier && nodes) {
          for (auto xyni = candidate->child_begin(); xyni != candidate->child_end(); ++xyni ) {
            auto *cnode = &*xyni;
            if (cnode->get_local_name().compare(fld->name) == 0) {
              auto value = cnode->get_text_value();
              auto len = value.copy(value_str, value.size());
              value_str[len] = '\0';
              value_str_len = value.size();
              break;
            }
          }
        } else {
          rw_status_t rs =
            rw_pb_get_field_value_str (k_node, value_str, &value_str_len, (ProtobufCMessage*)key_v, fld->name);
          RW_ASSERT(RW_STATUS_SUCCESS == rs);
        }

        XMLNode *k_xml = candidate->find_value(k_node->get_name(), value_str, k_node->get_ns());

        if (nullptr == k_xml) {
          found = false;
        }
      }
    }

    if (is_wildcard && found) {
      nodes->push_back(candidate);
      found = false;
    }
    if (!found) {
      candidate = candidate->get_next_sibling(candidate->get_local_name(),
                                              candidate->get_name_space());
    }
  }
  if (candidate) {
    RW_XML_NODE_DEBUG_STRING (this, "Found Node matching protobuf ");
  } else {
    RW_XML_NODE_INFO_STRING(this, "No matching node for protobuf search");
  }
  return candidate;
}


rw_yang_netconf_op_status_t XMLNode::find_list_by_key(
  YangNode* list_ynode,
  XMLNode* lookup_node,
  XMLNode** found_node)
{
  /*
   * ATTN: It is very expensive to search by keys in the xerces
   * implementation - we really need to reimplement this special in
   * xerces, and possibly the other DOM types.
   *
   * ATTN: In order to cache the key mapping, we would also need to
   * expose the dirty indication, and/or bind the RW Node List to the
   * RW Node, so that the key list mapping can be maintained.
   */
  RW_ASSERT(lookup_node);
  RW_ASSERT(list_ynode);
  RW_ASSERT(found_node);
  RW_ASSERT(list_ynode->has_keys()); // ATTN: should this be considered a failure and not a bug?

  RW_XML_NODE_DEBUG_STRING (lookup_node, "Finding lookupnode in ");
  RW_XML_NODE_DEBUG_STRING (this, " ");

  YangNode* yang_node = get_descend_yang_node();
  if (yang_node) {
    if (yang_node->is_rpc()) {
      // In case the XML Node is RPC, the child list node's parent will be
      // input/output node. The RPC node should be validated against the child
      // list node's parent's (input/output) parent.
      RW_ASSERT(list_ynode->get_parent());
      RW_ASSERT(list_ynode->get_parent()->get_parent() == yang_node);
    } else {
      RW_ASSERT(list_ynode->get_parent() == yang_node);
    }
  }

  XMLNodeList::uptr_t children(get_children());
  *found_node = nullptr;

  for (size_t i = 0; i < children->length(); i++) {

    XMLNode* my_child_node = children->at(i);
    if ((my_child_node->get_local_name() == list_ynode->get_name()) &&
        (my_child_node->get_name_space() == list_ynode->get_ns())) {

      // ATTN: Should have key iteration function on the node
      bool found_match = true;
      for (YangKeyIter yki=list_ynode->key_begin();
           yki != list_ynode->key_end();
           yki++) {

        RW_ASSERT (nullptr != yki->get_key_node());

        // ATTN: This calculation is loop invariant, per key!
        XMLNode* val_node_to_merge = lookup_node->find(
          yki->get_key_node()->get_name(),
          yki->get_key_node()->get_ns());

        if (nullptr == val_node_to_merge) {
          // the input node should have all key nodes present.
          // The input is invalid
          std::stringstream log_strm;
          log_strm << " Key Node " << yki->get_key_node()->get_name() << ":" << yki->get_key_node()->get_ns()<< "Not present in DOM Node" << this ;
          RW_XML_NODE_ERROR (this, log_strm);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }

        XMLNode *val_node_in_dom = my_child_node->find(
          yki->get_key_node()->get_name(),
          yki->get_key_node()->get_ns());

        if (!val_node_in_dom) {
          // request DOM might not have the key always
          found_match = false;
          break;
        }

        if (val_node_in_dom->get_text_value() !=
            val_node_to_merge->get_text_value()) {
          // value of atleast this key does not match
          found_match = false;
          break;
        }
      }
      if (found_match) {
        *found_node = my_child_node;
        return RW_YANG_NETCONF_OP_STATUS_OK;
      }
    }
  }

  return RW_YANG_NETCONF_OP_STATUS_FAILED;
}

rw_yang_netconf_op_status_t XMLNode::find_list_by_key_pb_msg(
    YangNode* list_ynode,
    /*ATTN:const ProtobufCMessage*/ void* lookup_value,
    XMLNode** found_node)
{
  /*
   * ATTN ATTN ATTN ATTN
   * ATTN: This entire function is a duplicate of the previous
   * function, but for pb messages.  This is crazy.  This API should
   * not exist.
   */

  RW_ASSERT(list_ynode);
  RW_ASSERT(lookup_value);
  RW_ASSERT(list_ynode->has_keys()); // should this be considered a failure
                                     // and not a bug?

  *found_node = 0;

  YangNode* yang_node = get_descend_yang_node();
  if (yang_node && (RW_YANG_STMT_TYPE_RPC != yang_node->get_stmt_type())) {
    RW_ASSERT(list_ynode->get_parent() == yang_node);
  }

  XMLNodeList::uptr_t children(get_children());
  *found_node = nullptr;

  for (size_t i = 0; i < children->length(); i++) {

    XMLNode* my_child_node = children->at(i);

    if ((my_child_node->get_local_name() == list_ynode->get_name()) &&
        (my_child_node->get_name_space() == list_ynode->get_ns())) {

      bool found_match = true;


      for (YangKeyIter yki=list_ynode->key_begin();
           yki != list_ynode->key_end();
           yki++) {
        char value_str [RW_PB_MAX_STR_SIZE];
        size_t value_str_len = RW_PB_MAX_STR_SIZE;
        YangNode *k_node = yki->get_key_node();
        RW_ASSERT (nullptr != k_node);
        std::string mangled = YangModel::mangle_to_c_identifier(k_node->get_name());
        // ATTN: This calculation is loop invariant, per key!

        // ATTN: changes required when PB supports namespaces correctly

        // ATTN: This is stupid - lookup_value is repeatedly being converted into a string!
        if (RW_STATUS_SUCCESS != rw_pb_get_field_value_str (k_node,
                                                            value_str, &value_str_len,
                                                            (ProtobufCMessage*)lookup_value,
                                                            mangled.c_str())) {
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }

        XMLNode *val_node_in_dom = my_child_node->find(k_node->get_name(), k_node->get_ns());

        if (!val_node_in_dom) {
          return RW_YANG_NETCONF_OP_STATUS_FAILED;
        }

        if (strcmp(val_node_in_dom->get_text_value().c_str(),value_str)) {
          // value of atleast this key does not match
          found_match = false;
          break;
        }
      }
      if (found_match) {
        *found_node = my_child_node;
        return RW_YANG_NETCONF_OP_STATUS_OK;
      }
    }
  }
  return RW_YANG_NETCONF_OP_STATUS_FAILED;
}

XMLNode* XMLNode::find_first_deepest_node()
{
  XMLNodeList::uptr_t my_children = get_children();
  size_t i;

  for ( i = 0; i < my_children->length(); i++) {
    YangNode *yn = my_children->at(i)->get_yang_node();

    if (nullptr == yn) {
      continue;
    }

    if (yn->is_leafy()) {
      continue;
    }

    return my_children->at(i)->find_first_deepest_node();
  }

  // fell off the end - which means this is the node that is searched for
  if (i == 0) {
    // no children - return myself
    return this;
  }

  return my_children->at(0);

}

XMLNode* XMLNode::find_first_deepest_node_in_xml_tree()
{
  XMLNodeList::uptr_t my_children = get_children();
  size_t i;

  for ( i = 0; i < my_children->length(); i++) {
    YangNode *yn = my_children->at(i)->get_yang_node();

    if (nullptr == yn) {
      continue;
    }

    if (yn->is_leafy()) {
      continue;
    }

    return my_children->at(i)->find_first_deepest_node();
  }

  // fell off the end - which means this is the node that is searched for
  if (i == 0) {
    // no children - return myself
    return this;
  }

  return my_children->at(0);

}

rw_yang_netconf_op_status_t XMLNode::merge(XMLNode* merge_node)
{
  YangNode* my_yang_node = get_descend_yang_node();
  std::stringstream log_strm;
  log_strm<< "Merging nodes "<< this << merge_node;
  RW_XML_NODE_DEBUG (this, log_strm);

  log_strm.str("");
  log_strm.clear();

  if (!my_yang_node) {
    // Can't merge without a schema?
    // ATTN: Why not?  Merge it anyway, because we don't really want to lose things
    RW_XML_NODE_ERROR_STRING (this, "No yang node");
    return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
  }

  XMLNodeList::uptr_t my_children = get_children();
  XMLNodeList::uptr_t merge_children = merge_node->get_children();

  for (size_t i = 0; i < merge_children->length(); i++) {

    XMLNode* merge_child_node = merge_children->at(i);
    std::string child_name = merge_child_node->get_local_name();
    std::string child_ns = merge_child_node->get_name_space();
    YangNode* child_ynode = my_yang_node->search_child(child_name.c_str(), child_ns.c_str());

    if (!child_ynode) {
      // The incoming XML has nodes that the model is unaware of. ignore these.
      // ATTN: This is not right - need to either:
      //  - determine a new root
      //  - save them for future validation failure
      continue;
    }

    XMLNode* my_child_node = my_children->find(child_name.c_str(), child_ns.c_str());
    if (!my_child_node) {

      /* Check and remove any children that are to be excluded due to
       * choice and case interactions with this node.
       * This has to be done only if the new child yang node is the
       * immediate child of a case node
       */

      if (nullptr != child_ynode->get_case()) {
        remove_conflicting_nodes(child_ynode);
      }

      XMLNode *new_child_node = import_child(merge_child_node, child_ynode, true);
      RW_ASSERT(new_child_node); // ownership  is with the document
      continue;
    }
    my_child_node->set_yang_node(child_ynode);

    /*
     * Both nodes exist.  Merging depends on the type of node, and
     * possibly other nodes.
     */
    switch (child_ynode->get_stmt_type()) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_CONTAINER: {
        /*
         * current:
         *   <my-child>
         *     <!-- my child's contents -->
         *   </my-child>
         *
         * new:
         *   <my-child>
         *     <!-- merge of my child's contents with merge child's contents -->
         *   </my-child>
         */
        rw_yang_netconf_op_status_t status = my_child_node->merge (merge_child_node);
        if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
          // ATTN: Keep merging?  Would allow more errors to be collected.
          log_strm << "Merge of child" << my_child_node->get_name_space() << " "<< my_child_node->get_local_name() << "Failed";
          RW_XML_NODE_ERROR (this, log_strm);
          return status;
        }
        continue;
      }

      case RW_YANG_STMT_TYPE_LEAF: {
        /*
         * current:
         *   <child-leaf>old-value</child-leaf>
         *
         * new:
         *   <child-leaf>new-value</child-leaf>
         */
        std::string value = merge_child_node->get_text_value().c_str();
        if (RW_STATUS_SUCCESS != child_ynode->parse_value(value.c_str())) {
          log_strm << "Invalid value in Child node "<<merge_child_node->get_name_space()<<":"<<merge_child_node->get_local_name()<<":"<<merge_child_node->get_text_value();
          RW_XML_NODE_ERROR (this, log_strm);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        rw_status_t rs = my_child_node->set_text_value (value.c_str());
        if (RW_STATUS_SUCCESS != rs) {
          log_strm << "Error setting value "<<merge_child_node->get_name_space()<<":"<<merge_child_node->get_local_name()<<":"<<value;
          RW_XML_NODE_ERROR (this, log_strm);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        continue;
      }

      case RW_YANG_STMT_TYPE_LEAF_LIST: {
        /*
         * current:
         *   <child-leaf-list>value1</child-leaf-list>
         *   <child-leaf-list>value2</child-leaf-list>
         *   ...
         *
         * new:
         *   <child-leaf-list>value1</child-leaf-list>
         *   <child-leaf-list>value2</child-leaf-list>
         *   <child-leaf-list>merge-value-if-new</child-leaf-list>
         */
        std::string value = merge_child_node->get_text_value().c_str();
        if (RW_STATUS_SUCCESS != child_ynode->parse_value(value.c_str())) {
          log_strm << "Invalid value in Child node "<<merge_child_node->get_name_space()<<":"<<merge_child_node->get_local_name()<<":"<<merge_child_node->get_text_value();
          RW_XML_NODE_ERROR (this, log_strm);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }

        XMLNode* duplicate_node = find_value(child_name.c_str(),
                                             value.c_str(),
                                             child_ns.c_str());
        if (duplicate_node) {

          // Same value already exists. Ignore, as per yang rules.
          log_strm << "Duplicate value in Child node "<<merge_child_node->get_name_space()<<":"<<merge_child_node->get_local_name()<<":"<<merge_child_node->get_text_value();
          RW_XML_NODE_DEBUG (this, log_strm);
          continue;
        }

        XMLNode* new_node = add_child(child_ynode, value.c_str());
        if (!new_node) {
          log_strm << "Addition of Child node "<<child_ynode->get_ns()<<":"<<child_ynode->get_name()<<":"<<value<<"Failed";
          RW_XML_NODE_ERROR (this, log_strm);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        continue;
      }

      case RW_YANG_STMT_TYPE_LIST: {
        /*
         * current:
         *   <child-list>
         *     <!-- keys -->
         *     <!-- data -->
         *   </child-list>
         *   <!-- more entries... -->
         *
         * new if matching key:
         *   <child-list>
         *     <!-- matching keys -->
         *     <!-- existing nodes merged with merge nodes -->
         *   </child-list>
         *   <!-- more entries... -->
         *
         * new if no matching key:
         *   <child-list>
         *     <!-- keys -->
         *     <!-- data -->
         *   </child-list>
         *   <!-- more entries... -->
         *   <child-list>
         *     <!-- new entry keys -->
         *     <!-- new entry data -->
         *   </child-list>
         */

        // Find the duplicate list node, if any
        XMLNode* matching_list_node = nullptr;
        rw_yang_netconf_op_status_t status = find_list_by_key (child_ynode,
            merge_child_node, &matching_list_node);

        if (RW_YANG_NETCONF_OP_STATUS_OK == status) {
          RW_XML_NODE_DEBUG_STRING (this, "Found list element by key ");
          status = matching_list_node->merge(merge_child_node);
          if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
            return status;
          }
          continue;
        }

        XMLNode *new_child_node = import_child(merge_child_node, child_ynode, true);
        RW_XML_NODE_DEBUG_STRING (this, "Importing child from merge document ");
        RW_ASSERT(new_child_node); // ownership  is with the document
        continue;
      }

      case RW_YANG_STMT_TYPE_ANYXML:
        // ATTN: Support for these is not completed yet, but they must be supported!
        RW_ASSERT_NOT_REACHED();

      case RW_YANG_STMT_TYPE_RPC:
      case RW_YANG_STMT_TYPE_RPCIO:
        // ATTN: Workaround till the uagent can differentiate between edit-config and RPC
        break;
      case RW_YANG_STMT_TYPE_NOTIF:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_NA:
        // merge not supported
        RW_ASSERT_NOT_REACHED();

      default:
        RW_ASSERT_NOT_REACHED();
    }
  }
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

rw_yang_netconf_op_status_t XMLNode::merge(const rw_keyspec_path_t *in_keyspec,
                                           ProtobufCMessage *msg)
{

  XMLNode *node = this;
  char keyspec_debug[RW_XML_TMP_BUFFER_LEN];

  YangNode* yn = get_descend_yang_node();
  if (!yn) {
    // Can't merge without a schema?
    // ATTN: Why not?  Merge it anyway, because we don't really want to
    // lose things
    RW_XML_NODE_ERROR_STRING (this, "YangNode not found ");
    return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
  }
  rw_keyspec_path_t *keyspec = nullptr;
  get_specific_keyspec (in_keyspec, keyspec);
  RW_ASSERT (keyspec);


  YangModel *ym = yn->get_model();
  RW_ASSERT(ym);
  const rw_yang_pb_schema_t* ypbc_schema = ym->get_ypbc_schema();
  if (!ypbc_schema) {
    RW_XML_NODE_ERROR_STRING (this, "PB Schema not found ");
    rw_keyspec_path_free (keyspec, NULL);
    return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
  }

  rw_keyspec_path_get_print_buffer (keyspec, NULL, ypbc_schema, keyspec_debug, sizeof (keyspec_debug));
  std::stringstream log_strm;
  log_strm << " Keyspec "<< keyspec_debug;
  RW_XML_NODE_DEBUG (this, log_strm);

  log_strm.str("");
  log_strm.clear();

  const rw_yang_pb_msgdesc_t* curr_ypbc_msgdesc;
  rw_keyspec_path_t *unused = nullptr;
  rw_status_t rs =
      rw_keyspec_path_find_msg_desc_schema (keyspec, NULL, ypbc_schema, &curr_ypbc_msgdesc, &unused);

  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(curr_ypbc_msgdesc);
  RW_ASSERT(curr_ypbc_msgdesc->pbc_mdesc);
  if (nullptr != unused) {
    rw_keyspec_path_free (unused, NULL);
  }

  // Find the top message in the hierarchy - needed later to get the first child
  // of the root
  const rw_yang_pb_msgdesc_t* top = msg->descriptor->ypbc_mdesc;

  while (top->parent) {
    top = top->parent;
  }

  const ProtobufCMessageDescriptor *desc = curr_ypbc_msgdesc->pbc_mdesc;
  yn = nullptr;
  char* p_value = (char *) keyspec;

  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld = protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  RW_ASSERT(fld);
  RW_ASSERT(fld->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(*(p_value + fld->quantifier_offset))

  p_value += fld->offset;

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;

  for (size_t path_index = 0; path_index < desc->n_fields; path_index++){
      fld = &desc->fields[path_index];
    if (fld->id < RW_SCHEMA_TAG_PATH_ENTRY_START ||
        fld->id > RW_SCHEMA_TAG_PATH_ENTRY_END) {
      // not a path entry..
      continue;
    }

    RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);
    RW_ASSERT (fld->label == PROTOBUF_C_LABEL_OPTIONAL); // Path entries are always optional now.

    const ProtobufCMessageDescriptor * path_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;
    /*
     * ATTN: Please revisit this later.
     * Additional path entries are now added in concrete keyspecs.
     * So, skip the path entires if it is not set in the keyspec.
     */
    if (path_desc == &rw_schema_path_entry__descriptor) {
      /*
       * Generic path entry inside a concrete keyspec. This is currently not
       * used.. Skip it. Come back later when it is used.
       */
      continue;
    }

    char *path_v = p_value + fld->offset;

    fld = protobuf_c_message_descriptor_get_field_by_name (path_desc, "element_id");
    RW_ASSERT (fld);

    RwSchemaElementId *elem_id = (RwSchemaElementId *)(path_v + fld->offset);

    if (!yn) {
      yn = ym->search_node (elem_id->system_ns_id, elem_id->element_tag);
    } else {
      yn = yn->search_child (elem_id->system_ns_id, elem_id->element_tag);
    }
    if (nullptr == yn) {
      log_strm<<"Could not find yang node for "<< elem_id->system_ns_id << " " << elem_id->element_tag;
      rw_keyspec_path_free (keyspec, NULL);
      return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
    }

    if (RW_YANG_STMT_TYPE_RPCIO == yn->get_stmt_type()) {
      // RPC IO nodes are not present in the XML DOM. the children
      // of the RPC IO node become the children of the RPC node. keep
      // th node as is
    }
    else if (!yn->is_listy()) {
      XMLNode *child = node->find(yn->get_name(), yn->get_ns());
      if (nullptr == child) {
        node = node->add_child (yn);
      } else {
        node = child;
      }
    }
    else {
      XMLNode *child = node->find_list_by_key(yn, (ProtobufCMessage *)path_v, nullptr);
      if (nullptr == child) {
        node = node->append_child(yn, (ProtobufCMessage *)path_v);
      }
      else {
        node = child;
      }
    }
    auto cat = rw_keyspec_path_get_category(keyspec);

    if (RW_YANG_STMT_TYPE_RPC == yn->get_stmt_type()) {
      if (RW_SCHEMA_CATEGORY_RPC_OUTPUT == cat) {
        yn = yn->search_child_rpcio_only("output");
      }
    }

  }

  rw_keyspec_path_free (keyspec, NULL);
  return node->merge (msg);
}

rw_yang_netconf_op_status_t XMLNode::merge_rpc(ProtobufCBinaryData *msg_b,
                                               const char *type)
{
  // Should this be for RPC only? it should..

  // Assume called on root..
  XMLNode *node = get_first_child();

  if (!node) {
    RW_XML_NODE_ERROR_STRING (this, "Get first child failed");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  YangNode *yn = node->get_yang_node();

  if (nullptr == yn) {
    RW_XML_NODE_ERROR_STRING (this, "Get yang node failed");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  if (yn->get_stmt_type() != RW_YANG_STMT_TYPE_RPC) {
    RW_XML_NODE_ERROR_STRING (this, "Stmt type is not RPC");
    return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
  }

  yn = yn->search_child_rpcio_only(type);

  if (nullptr == yn) {
    RW_XML_NODE_ERROR_STRING (this, type);
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  const rw_yang_pb_msgdesc_t *ypbc_msgdesc =  yn->get_ypbc_msgdesc();

  if (nullptr == ypbc_msgdesc) {
    RW_XML_NODE_ERROR_STRING (this, "Message schema missing");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  ProtobufCMessage *anon =
      protobuf_c_message_unpack(&get_manager().pb_instance_, ypbc_msgdesc->pbc_mdesc, msg_b->len, msg_b->data);
  RW_ASSERT(anon);

  // Message: merge onto XML node
  rw_yang_netconf_op_status_t ncrs = node->merge(anon);

  return ncrs;
}

rw_yang_netconf_op_status_t XMLNode::merge(ProtobufCBinaryData *keyspec_b,
                                           ProtobufCBinaryData *msg_b)
{

  // Transform the raw keyspec buffer to a specific key spec.

  // Keyspec: From Buffer to a Generic KeySpec
  rw_keyspec_path_t *gen_ks = rw_keyspec_path_create_with_binpath_binary_data(NULL, keyspec_b);
  YangModel *ym = get_manager().get_yang_model();
  if (nullptr == gen_ks) {
    RW_XML_NODE_ERROR_STRING (this, "Failed to create keyspec from binary data");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  rw_keyspec_path_t *keyspec = nullptr;
  get_specific_keyspec (gen_ks, keyspec);
  RW_ASSERT (keyspec);

  char keyspec_debug[RW_XML_TMP_BUFFER_LEN];
  rw_keyspec_path_get_print_buffer (keyspec, NULL, ym->get_ypbc_schema(), keyspec_debug, sizeof (keyspec_debug));
  std::stringstream log_strm;log_strm << " Keyspec "<< keyspec_debug;
  RW_XML_NODE_DEBUG (this, log_strm);

  auto cat = rw_keyspec_path_get_category(keyspec);

  ProtobufCMessage *spec_ks = (ProtobufCMessage *) keyspec;

  // Keyspec: to XML
  XMLNode *node = this;
  YangNode *yn = nullptr;
  char *dompath_v = (char *) spec_ks;

  const ProtobufCMessageDescriptor* desc =
      ((ProtobufCMessage *)spec_ks)->descriptor;

  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  RW_ASSERT (fld);
  RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);

  dompath_v += fld->offset;

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;
  RW_ASSERT (*((char *)spec_ks + fld->quantifier_offset));

  for (size_t path_index = 0; path_index < desc->n_fields; path_index++) {
    fld = &desc->fields[path_index];
    if (fld->id < RW_SCHEMA_TAG_PATH_ENTRY_START ||
        fld->id > RW_SCHEMA_TAG_PATH_ENTRY_END) {
      // not a path entry..
      continue;
    }

    RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);
    RW_ASSERT (fld->label == PROTOBUF_C_LABEL_OPTIONAL); // path entries are optional now.

    const ProtobufCMessageDescriptor * path_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;

    /*
     * ATTN: Please revisit this later.
     * Additional path entries are now added in concrete keyspecs.
     * So, skip the path entires if it is not set in the keyspec.
     */
    if (path_desc == &rw_schema_path_entry__descriptor) {
      /*
       * Generic path entry inside a concrete keyspec. This is currently not
       * used.. Skip it. Come back later when it is used.
       */
      continue;
    }

    char *path_v = dompath_v + fld->offset;

    fld = protobuf_c_message_descriptor_get_field_by_name (path_desc, "element_id");
    RW_ASSERT (fld);

    RwSchemaElementId *elem_id = (RwSchemaElementId *)(path_v + fld->offset);

    if (!yn) {
      yn = ym->search_node (elem_id->system_ns_id, elem_id->element_tag);
    } else {
      yn = yn->search_child (elem_id->system_ns_id, elem_id->element_tag);
    }

    if (nullptr == yn) {
      log_strm << " No yangNode for " << elem_id->system_ns_id << " " <<elem_id->element_tag;
      RW_XML_NODE_ERROR (this, log_strm);
      return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
    }

    if (RW_YANG_STMT_TYPE_RPCIO == yn->get_stmt_type()) {
      // RPC IO nodes are not present in the XML DOM. the children
      // of the RPC IO node become the children of the RPC node. keep
      // th node as is
    }
    else if (!yn->is_listy()) {
      XMLNode *child = node->find(yn->get_name(), yn->get_ns());
      if (nullptr == child) {
        node = node->add_child (yn);
      } else {
        node = child;
      }
    }
    else {
      XMLNode *child = node->find_list_by_key(yn, (ProtobufCMessage *)path_v, nullptr);
      if (nullptr == child) {
        node = node->append_child(yn, (ProtobufCMessage *)path_v);
      }
      else {
        node = child;
      }
    }

    if (RW_YANG_STMT_TYPE_RPC == yn->get_stmt_type()) {
      if (RW_SCHEMA_CATEGORY_RPC_OUTPUT == cat) {
        yn = yn->search_child_rpcio_only("output");
      }
    }
  }

  rw_keyspec_path_free (keyspec, NULL);
  rw_keyspec_path_free (gen_ks, NULL);

  if (nullptr == msg_b) {
    return RW_YANG_NETCONF_OP_STATUS_OK;
  }

  RW_ASSERT(msg_b);

  // Message: decode using type found from keyspec processing
  // yn is also the last non-leafy node. This should thus have the schema for
  // the undecoded protobuf

  const rw_yang_pb_msgdesc_t*  ypbc_msgdesc = yn->get_ypbc_msgdesc();
  RW_ASSERT(ypbc_msgdesc);

  ProtobufCMessage *anon =
      protobuf_c_message_unpack(&get_manager().pb_instance_,
                                ypbc_msgdesc->pbc_mdesc, msg_b->len, msg_b->data);
  if (nullptr == anon) {
    // Should be logged from PB
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  // Message: merge onto XML node
  rw_yang_netconf_op_status_t ncrs = node->merge(anon, false, true);
  protobuf_c_message_free_unpacked (&get_manager().pb_instance_, anon);

  return ncrs;
}


rw_yang_netconf_op_status_t XMLNode::merge(/*ATTN:const*/ProtobufCMessage* msg,
                                           bool is_create,
                                           bool validate_key)
{
  YangNode* my_yang_node = get_descend_yang_node();
  std::stringstream log_strm;
  rw_yang_netconf_op_status_t ncrs = RW_YANG_NETCONF_OP_STATUS_OK;

  if (!my_yang_node) {
    // Can't merge without a schema?
    // ATTN: Why not?  Merge it anyway, because we don't really want to
    // lose things
    RW_XML_NODE_ERROR_STRING (this, "No Yang node");
    return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
  }

  if (RW_YANG_STMT_TYPE_RPC == my_yang_node->get_stmt_type()) {
    // The message can be either input or output.
    // Change the my_yang_node to point to input/output
    if (msg->descriptor->ypbc_mdesc) {
      switch (msg->descriptor->ypbc_mdesc->msg_type) {
        case RW_YANGPBC_MSG_TYPE_RPC_INPUT:
          my_yang_node = my_yang_node->search_child_rpcio_only("input");
          break;
        case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:
          my_yang_node = my_yang_node->search_child_rpcio_only("output");
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
    }
  }
  RW_ASSERT(my_yang_node);

  if (validate_key &&
      my_yang_node->is_listy()   // For lists, validate the keys
      && get_children()->length()) { // but only if the keys have been set

    // The merge can proceed at this node only if the key of the protobuf
    // is the same as the key of this node. If it is not, fail.

    XMLNode *parent = get_parent();
    if (nullptr == parent) {
      RW_XML_NODE_ERROR_STRING (this, "No Parent node");
      return RW_YANG_NETCONF_OP_STATUS_FAILED; // ATTN: different return?
    }

    XMLNode *match = 0;


    ncrs = parent->find_list_by_key_pb_msg(my_yang_node, msg, &match);

    if ((RW_YANG_NETCONF_OP_STATUS_OK != ncrs) ||
        (match != this)) {
      RW_XML_NODE_ERROR_STRING (this, "Mistmatched list nodes");
      return RW_YANG_NETCONF_OP_STATUS_FAILED;
    }
  }

  // The descriptor is always the first element in the message.
  const ProtobufCMessageDescriptor *desc = msg->descriptor;
  const rw_yang_pb_msgdesc_t *ypbc_mdesc = desc->ypbc_mdesc;

  const rw_yang_pb_flddesc_t* ypbc_flddescs = nullptr;
  if (ypbc_mdesc && ypbc_mdesc->num_fields) {
    RW_ASSERT(ypbc_mdesc->num_fields == desc->n_fields);
    ypbc_flddescs = ypbc_mdesc->ypbc_flddescs;
    RW_ASSERT(ypbc_flddescs);
  }

  XMLNodeList::uptr_t my_children = get_children();

  for (size_t i = 0; i < desc->n_fields ; i++) {
    size_t idx = i;

    if (ypbc_flddescs) {
      idx = ypbc_flddescs[i].pbc_order - 1;
      RW_ASSERT(idx < desc->n_fields);
    }
    const ProtobufCFieldDescriptor *fdesc = &desc->fields[idx];

    size_t j = 0;
    ProtobufCFieldInfo finfo;
    protobuf_c_boolean found = protobuf_c_message_get_field_instance(
      nullptr/*instance*/,
      msg,
      fdesc,
      j/*repeated_index*/,
      &finfo);

    if (!found) {
      continue;
    }

    YangNode* child_ynode = my_yang_node->search_child_by_mangled_name(fdesc->name);

    if (!child_ynode) {
      // The incoming message has nodes that the model is unaware of
      // .ignore these.
      // ATTN: This is not right - need to either:
      //  - determine a new root
      //  - save them for future validation failure
      log_strm.str(""); // ATTN: What? 4 lines of code to log some debug? This is not ok.
      log_strm<<" Field name "<< fdesc->name << " does not have associated yang node";
      RW_XML_NODE_INFO (this, log_strm);
      continue;
    }

    // Must be listy, or not, in both worlds.
    RW_ASSERT(   (PROTOBUF_C_LABEL_REPEATED == fdesc->label)
              == child_ynode->is_listy());

    std::string child_name = child_ynode->get_name();
    std::string child_ns = child_ynode->get_ns();

    XMLNode* my_child_node = my_children->find(child_name.c_str(),
                                               child_ns.c_str());

    if (!my_child_node) {
      /*
       * This node has no child that corresponds to the merge child.
       * Import ONLY the first one, and fall through if there are more.
       * The fall through is important, because there can be special
       * handling for duplicates.
       */
      ncrs = append_node_from_pb_field( &finfo, child_ynode );
      RW_ASSERT(RW_YANG_NETCONF_OP_STATUS_OK == ncrs);

      found = protobuf_c_message_get_field_instance( nullptr, msg, fdesc, ++j, &finfo );
      if (!found) {
        continue;
      }

      my_child_node = my_children->find(child_name.c_str(), child_ns.c_str());
      RW_ASSERT(nullptr != my_child_node);
    }

    auto stmt_type = child_ynode->get_stmt_type();
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_CONTAINER:
        /*
         * current:
         *   <my-child>
         *     <!-- my child's contents -->
         *   </my-child>
         *
         * msg: parts of the container with changes
         */
        RW_ASSERT(0 == j);

        ncrs = my_child_node->merge ((ProtobufCMessage*)finfo.element, is_create);
        if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
          // ATTN: Keep merging?  Would allow more errors to be collected.
          RW_ASSERT_NOT_REACHED();
          return ncrs;
        }
        continue;

      case RW_YANG_STMT_TYPE_LEAF:
        if (child_ynode->get_type()->get_leaf_type() == RW_YANG_LEAF_TYPE_EMPTY) {
          continue;
        }
        /**** FALL THROUGH ****/

      case RW_YANG_STMT_TYPE_LEAF_LIST:
        for (/*j already initialized*/;
             found;
             found = protobuf_c_message_get_field_instance(
               nullptr, msg, fdesc, ++j, &finfo) ) {
          if (RW_YANG_STMT_TYPE_LEAF == stmt_type) {
            RW_ASSERT(0 == j);
          }

          // ATTN: De-duplication of leaf-list happens inside append
          ncrs = append_node_from_pb_field( &finfo, child_ynode );
          if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
            log_strm.str("");
            log_strm.clear();
            log_strm <<" Error setting " << fdesc->name << " value";
            RW_XML_NODE_ERROR (this, log_strm);
            return ncrs;
          }
        }
        continue;

      case RW_YANG_STMT_TYPE_LIST:
        /*
         * For each value in the possible list of messages, find the
         * keys, and use them to possilby figure out a matching list
         * entry.  if none exists, create a new list entry
         */
        for (/*j already initialized*/;
             found;
             found = protobuf_c_message_get_field_instance(
               nullptr, msg, fdesc, ++j, &finfo) ) {
          bool handled = false;

          /* ATTN: Shouldn't this be skipping this check based on merge-behavior extension? */
          if (!is_create && child_ynode->has_keys()) {
            XMLNode *matching_list_node = nullptr;
            ncrs = find_list_by_key_pb_msg(child_ynode, (void*)finfo.element, &matching_list_node);

            if (RW_YANG_NETCONF_OP_STATUS_OK == ncrs) {
              ncrs = matching_list_node->merge ((ProtobufCMessage*)finfo.element);
              RW_ASSERT(RW_YANG_NETCONF_OP_STATUS_OK == ncrs);
              handled = true;
            }
          }
          if (!handled){
            ncrs = append_node_from_pb_field( &finfo, child_ynode );
            RW_ASSERT(RW_YANG_NETCONF_OP_STATUS_OK == ncrs);
          }
        }
        continue;

      case RW_YANG_STMT_TYPE_ANYXML:
        // ATTN: Support for these is not completed yet,
        // but they must be supported!
        RW_ASSERT_NOT_REACHED();

      case RW_YANG_STMT_TYPE_RPC:
      case RW_YANG_STMT_TYPE_RPCIO:
        // ATTN: Workaround till the uagent can differentiate between
        // edit-config and RPC
        break;

      case RW_YANG_STMT_TYPE_NOTIF:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_NA:
        // merge not supported
        RW_ASSERT_NOT_REACHED();

      default:
        RW_ASSERT_NOT_REACHED();
    }
  } // for all the fields

  return RW_YANG_NETCONF_OP_STATUS_OK;
}

void XMLNode::remove_conflicting_nodes(YangNode *new_node)
{
  XMLNodeList::uptr_t my_children = get_children();
  size_t i = 0;
  std::stringstream log_strm;

  while (i < my_children->length()) {
    log_strm.str("");
    log_strm.clear();

    XMLNode* child_node = my_children->at(i);
    YangNode* child_ynode = child_node->get_yang_node();

    if (!child_ynode) {
      // No mapping?  skip this node
      RW_XML_NODE_INFO_STRING (child_node,  "No yang node found ");
      i++;
      continue;
    }
    if (new_node->is_conflicting_node (child_ynode)) {
      RW_XML_NODE_DEBUG_STRING (child_node,  "Removing due to conflict ");
      remove_child (child_node);
    }
    else {
      i++;
    }
  }
}

XMLNode* XMLNode::append_child(
  YangNode* yn,
  /*ATTN:const?*/ProtobufCMessage* path_entry)
{
  RW_ASSERT(yn);
  RW_ASSERT(path_entry);
  XMLNode *child = add_child (yn);
  RW_ASSERT(child);

  /*
   * Path entry looks like this in PB:
   *   msg pe {
   *     msg key00 {
   *       optional strkey;
   *       required type keyname;
   *     }
   *     msg key01 {
   *       optional strkey;
   *       required type keyname;
   *     }
   *   }
   * So, this function needs to do some extra work to unpeel the extra
   * layers, and also skip the string key.
   */

  auto pe_mdesc = path_entry->descriptor;
  for (size_t key_index = 0; key_index < pe_mdesc->n_fields; key_index++) {

    auto keyxx_fdesc = &pe_mdesc->fields[key_index];
    if (RW_SCHEMA_TAG_ELEMENT_KEY_START > keyxx_fdesc->id ||
        RW_SCHEMA_TAG_ELEMENT_KEY_END < keyxx_fdesc->id) {
      continue;
    }

    RW_ASSERT (keyxx_fdesc->type == PROTOBUF_C_TYPE_MESSAGE);
    auto keyxx_mdesc = keyxx_fdesc->msg_desc;

    ProtobufCFieldInfo keyxx_finfo;
    bool ok = protobuf_c_message_get_field_instance(
      NULL/*instance*/,
      path_entry,
      keyxx_fdesc,
      0/*repeated_index*/,
      &keyxx_finfo);
    RW_ASSERT(ok);

    for (size_t fld_index = 0; fld_index < keyxx_mdesc->n_fields; fld_index++) {
      auto keyval_fdesc = &keyxx_mdesc->fields[fld_index];

      // ATTN: A rather suspicious test. nothing else helps..
      if (keyval_fdesc->id == 1) {
        continue;
      }

      YangNode *key_yn = yn->search_child_by_mangled_name(keyval_fdesc->name);
      RW_ASSERT(key_yn);

      ProtobufCFieldInfo keyval_finfo;
      ok = protobuf_c_message_get_field_instance(
        NULL/*instance*/,
        (ProtobufCMessage*)keyxx_finfo.element,
        keyval_fdesc,
        0/*repeated_index*/,
        &keyval_finfo);
      RW_ASSERT(ok);
      child->append_node_from_pb_field( &keyval_finfo, key_yn );
    }
  }
  return child;
}

// ATTN: checking for duplicate entries of leaf lists is hidden in the bowels of
// this function. That makes this function ugly, and not really true to its name.
// Clean this up once PB correctly detects Leaf List duplicates.

rw_yang_netconf_op_status_t XMLNode::append_node_from_pb_field(
  const ProtobufCFieldInfo* finfo,
  YangNode *ynode)
{

  PROTOBUF_C_CHAR_BUF_DECL_INIT(NULL, xmlbuf);

  switch (ynode->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_LIST:
      // for lists, check if all the keys are present. once keys are present,
      // the rest of the handling is the same as that for other complex types
      for (YangKeyIter yki=ynode->key_begin(); yki != ynode->key_end(); yki++) {
        std::string mangled = YangModel::mangle_to_c_identifier(
            yki->get_key_node()->get_name());

        if (!rw_pb_is_field_present(
               finfo->fdesc,
               (ProtobufCMessage*)finfo->element,
               mangled.c_str())) {
          std::stringstream err;
          err << "Key field " << finfo->fdesc->name << " not present in protobuf ";
          RW_XML_NODE_ERROR (this, err);
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
      }
      /**** FALL THROUGH ****/

    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_CONTAINER: {
      // create the child from yang node properties
      XMLNode* child = add_child (ynode);
      return child->merge((/*ATTN:const*/ProtobufCMessage*)finfo->element, true);
    }

    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      switch (ynode->get_type()->get_leaf_type()) {
        case RW_YANG_LEAF_TYPE_DECIMAL64: {
          // ATTN:- Needs special handling. Should be done in protobuf-c get_xml func.
          // Get the fraction digits value from the yang node.
          uint8_t precision = ynode->get_type()->get_dec64_fraction_digits();
          RW_ASSERT (precision); // It is mandatory

          std::stringstream xmlstr;
          xmlstr.precision(precision);

          if (   finfo->fdesc->type == PROTOBUF_C_TYPE_DOUBLE
              || finfo->fdesc->type == PROTOBUF_C_TYPE_FLOAT ) {

            if (finfo->fdesc->type == PROTOBUF_C_TYPE_DOUBLE) {
              xmlstr << std::fixed << *(double *)finfo->element;
            } else {
              xmlstr << std::fixed << *(float *)finfo->element;
            }
            strcpy (xmlbuf.buffer, xmlstr.str().c_str());
            break;
          }
          // else fall through.
        }
        case RW_YANG_LEAF_TYPE_INT8:
        case RW_YANG_LEAF_TYPE_INT16:
        case RW_YANG_LEAF_TYPE_INT32:
        case RW_YANG_LEAF_TYPE_INT64:
        case RW_YANG_LEAF_TYPE_UINT8:
        case RW_YANG_LEAF_TYPE_UINT16:
        case RW_YANG_LEAF_TYPE_UINT32:
        case RW_YANG_LEAF_TYPE_UINT64:
        case RW_YANG_LEAF_TYPE_UNION:
        case RW_YANG_LEAF_TYPE_INSTANCE_ID:
        case RW_YANG_LEAF_TYPE_BOOLEAN:
        case RW_YANG_LEAF_TYPE_LEAFREF:
        case RW_YANG_LEAF_TYPE_STRING:
        case RW_YANG_LEAF_TYPE_BINARY:
        case RW_YANG_LEAF_TYPE_IDREF: {
          protobuf_c_boolean rc = protobuf_c_field_info_get_xml( finfo, &xmlbuf );
          RW_ASSERT(rc);
          break;
        }

        case RW_YANG_LEAF_TYPE_ENUM: {
          /*
           * ATTN: This string lookup needs to be moved into
           * protobuf_c_field_info_get_xml using the rw enum desc.  Doing that
           * requires some major surgery to move protobuf-c into core/util, or
           * moving the rw_yang types into ext/yang.  Both are non-trivial
           * tasks...
           */
          int32_t enumval = *(int32_t*)finfo->element;
          std::string strval = ynode->get_enum_string( enumval );
          if (0 == strval.length()) {
            std::stringstream err;
            err << "No enum string for " << finfo->fdesc->name << ": " << enumval;
            RW_XML_NODE_ERROR (this, err);
            return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
          }

          RW_ASSERT(xmlbuf.buf_len > strval.length());
          strcpy (xmlbuf.buffer, strval.c_str());
          break;
        }

        case RW_YANG_LEAF_TYPE_EMPTY: {
          XMLNode* child = add_child (ynode);
          RW_ASSERT(child);
          return RW_YANG_NETCONF_OP_STATUS_OK;
        }

        case RW_YANG_LEAF_TYPE_BITS:
        case RW_YANG_LEAF_TYPE_ANYXML:
          RW_ASSERT_NOT_REACHED();

        default:
          RW_ASSERT_NOT_REACHED();
      }
      break;

    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_ANYXML:
    default:
      RW_ASSERT_NOT_REACHED();
  }

  RW_ASSERT(ynode->is_leafy());

  do {

    if (ynode->is_listy()) {
      XMLNode *duplicate = find_value( ynode->get_name(), xmlbuf.buffer, ynode->get_ns() );
      if (nullptr != duplicate) {
        break; // Ignore duplicates.
      }
    }

    XMLNode* child = add_child( ynode, xmlbuf.buffer );
    RW_ASSERT(child);

  } while(0);

  PROTOBUF_C_CHAR_BUF_CLEAR(&xmlbuf);
  return RW_YANG_NETCONF_OP_STATUS_OK;
}


// ATTN: Define common implementation of to_string()
// ATTN: Define common implementation of to_stdout()
// ATTN: Define common implementation of to_file()

rw_yang_netconf_op_status_t XMLNode::to_pbcm(
  ProtobufCMessage** pbcm)
{
  *pbcm = nullptr;
  YangNode* ynode = get_descend_yang_node(); // ATTN: need this?

  if (nullptr == ynode) {
    RW_XML_NODE_ERROR_STRING (this, "No yang node");
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }

  RW_ASSERT(ynode);


  const ProtobufCMessageDescriptor* desc = nullptr;
  const rw_yang_pb_msgdesc_t *ypbc_msgdesc =  ynode->get_ypbc_msgdesc();

  if (ypbc_msgdesc) {
    desc = ypbc_msgdesc->pbc_mdesc;
  }

  if (nullptr == desc) {
    RW_XML_NODE_ERROR_STRING (this, "No protobuf descriptor");
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }

  RW_ASSERT (desc->sizeof_message);

  *pbcm = (ProtobufCMessage *) malloc (desc->sizeof_message);
  protobuf_c_message_init(desc, *pbcm);

  rw_yang_netconf_op_status_t status = to_pbcm(*pbcm);

  if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
    free (*pbcm);
    *pbcm = nullptr;
  }

  return status;
}

rw_yang_netconf_op_status_t XMLNode::to_rpc_input(ProtobufCMessage** pbcm)
{
  return to_rpc("input", pbcm);
}

rw_yang_netconf_op_status_t XMLNode::to_rpc_output(ProtobufCMessage** pbcm)
{
  return to_rpc("output", pbcm);
}

rw_yang_netconf_op_status_t XMLNode::to_rpc(const char *type,
                                            ProtobufCMessage** pbcm)
{
  *pbcm = nullptr;
  YangNode* yn = get_yang_node();

  RW_ASSERT(yn);

  if (RW_YANG_STMT_TYPE_RPC != yn->get_stmt_type()) {
    RW_XML_NODE_ERROR_STRING (this, "Not an RPC element");
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }


  YangNode *in_out = yn->search_child_rpcio_only(type);
  if (nullptr == in_out) {
    RW_XML_NODE_ERROR_STRING (this, "Not the right type");
    return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
  }

  // Create a Protobuf with the description available at the input yang node,
  // and conver the xml rpc node to that protobuf
  const ProtobufCMessageDescriptor* desc = nullptr;
  const rw_yang_pb_msgdesc_t *ypbc_msgdesc =  in_out->get_ypbc_msgdesc();

  if (ypbc_msgdesc) {
    desc = ypbc_msgdesc->pbc_mdesc;
  }

  RW_ASSERT(desc);

  RW_ASSERT (desc->sizeof_message);

  *pbcm = (ProtobufCMessage *) malloc (desc->sizeof_message);
  protobuf_c_message_init(desc, *pbcm);

  rw_yang_netconf_op_status_t status = to_pbcm(*pbcm);

  if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
    free (*pbcm);
    *pbcm = nullptr;
  }

  return status;
}

rw_yang_netconf_op_status_t XMLNode::to_pbcm(
  ProtobufCMessage* top_pbcm)
{
  RW_ASSERT(top_pbcm);
  YangNode* search_ynode = get_descend_yang_node();
  RW_ASSERT(search_ynode);

  const ProtobufCMessageDescriptor* pbcmd = top_pbcm->descriptor;

  // ATTN: This code does not handle XML attributes!
  // ATTN: This code does not set defaults!

  for (XMLNodeIter xyni = child_begin(); xyni != child_end(); ++xyni ) {
    XMLNode* child_node = &*xyni;

    YangNode* child_ynode = child_node->get_yang_node();
    if (!child_ynode) {
      RW_XML_NODE_ERROR_STRING(child_node, "No yang Node ");
      return RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ELEMENT;
    }

    rw_yang_stmt_type_t stmt_type = child_ynode->get_stmt_type();

    /*
     * Should have a field descriptor.  Might not, though - the model can
     * be more expansive than the protobuf.
     *
     * ATTN: In YangModel, when mapping a YangNode to a PB-Message, it is
     * considered okay to have child YangNodes that have no corresponding
     * PB-Field.  When considered from the translation point of view, that
     * makes sense in the C->XML direction, because extra yang fields do
     * produce any data loss.  However, in the XML->C direction, a PB-Field
     * with no corresponding Yang child could lead to lost data, if those
     * yang fields are actually used in the XML.  At first glance that might
     * seem just as bad, but in the XML->C direction, there is also the
     * possibility that there are random erroneous elements, as well as
     * random erroneous text elements...  So, the question becomes - how
     * faithful do we want the translation to be?  What kind of errors do we
     * need to detect and report?
     *
     * ATTN: One reason to allow the yang to have more fields than the PB
     * is because libncx is currently shared between YangModels; therefore
     * it is possible to augment a model without the application really
     * intending to do so...  This practical consideration may be the best
     * reason for the unequal treatment.
     */
    const ProtobufCFieldDescriptor* pbcfd =
        protobuf_c_message_descriptor_get_field_by_name (pbcmd,child_ynode->get_pbc_field_name());
    if (!pbcfd) {
      // Lossy conversion.
      // ATTN: eout && *eout <<
      // "Error: no field for element " <<
      // child_node->get_local_name() << std::endl;
      RW_XML_NODE_ERROR_STRING (child_node, "No Protobuf field description");
      return RW_YANG_NETCONF_OP_STATUS_FAILED;
    }

    // Leafy?
    if (child_ynode->is_leafy()) {

      set_pb_field_from_xml (&get_manager().pb_instance_, top_pbcm, pbcfd, child_node);
      continue;
    }

    // Must be message-like
    switch (stmt_type) {
      // ATTN: case RW_YANG_STMT_TYPE_GROUPING:
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_LIST:
      case RW_YANG_STMT_TYPE_RPCIO:
      case RW_YANG_STMT_TYPE_NOTIF:
        // THERE IS CODE in fpath/apps etc that depend on the lists
        // presence to figure out what data to return

        // if (child_node->validate() != RW_YANG_NETCONF_OP_STATUS_OK) {
        //   // skip this node, and its descendents
        //   continue;
        // }

        break;

      case RW_YANG_STMT_TYPE_ROOT: /* ATTN: allow this? */
      case RW_YANG_STMT_TYPE_NA:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_CHOICE: /* ATTN: allow this? */
      case RW_YANG_STMT_TYPE_CASE: /* ATTN: allow this? */
      case RW_YANG_STMT_TYPE_RPC:
        RW_ASSERT_NOT_REACHED(); // should be impossible

      default:
        RW_ASSERT_NOT_REACHED(); // should be impossible
    }

    ProtobufCMessage* inner_pbcm = nullptr;
    bool ok = protobuf_c_message_set_field_message(&get_manager().pb_instance_, top_pbcm, pbcfd, &inner_pbcm);
    if (!ok) {
      RW_XML_NODE_ERROR_STRING (child_node, "Failed to set composite message ");
      return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
    }

    // ATTN: Ensure no text elements?

    // Must have found an inner message
    if (!inner_pbcm) {
      RW_XML_NODE_ERROR_STRING (child_node, "Failed to retrieve inner message ");
      return RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED;
    }

    // Recurse into the message...
    rw_yang_netconf_op_status_t rs = child_node->to_pbcm(inner_pbcm);
    if (RW_YANG_NETCONF_OP_STATUS_OK != rs) {
      return rs;
    }
  }

  return RW_YANG_NETCONF_OP_STATUS_OK;
}

XMLNode* XMLNode::apply_subtree_filter_recursive(
  XMLNode *filter)
{
  // ATTN -- This is incomplete now -- Since this is used by ServerMock and since the accuracy of
  // the result is not important --- this is being left as is.
  //  This will need to completed someday ...

  // ATTN Hack --  Skip the root node
  if (filter->get_local_name() == "filter") {
    filter = filter->get_first_child();
  }

  if (filter == nullptr) {
    return this;
  }

  // Check if the filter matches the node
  // If it does and if the filter node is a containement node
  // call apply_subtree_filter_recursive() with the node's children
  // and filkter's children
  // Check if this is leaf node
  if (filter->get_first_child() == nullptr) {
    // ATTN -- Need to macth the siblings --- Do a single simple match for now
    // Leafy node -- is this selection node
    if (filter->get_local_name() == get_local_name() &&
        (filter->get_name_space() == "" ||
         filter->get_name_space() == get_name_space())) {
      if (filter->get_text_value() == "") {
        // Selection node
        return this;
      } else {
        if (filter->get_text_value() == get_text_value()) {
          return this;
        }
        // content match node
      }
    }
  } else {
    // Non leafy node - check the node type
    if (filter->get_local_name() == get_local_name() &&
        (filter->get_name_space() == "" ||
         filter->get_name_space() == get_name_space())) {
      if (filter->get_text_value() == "" && this->get_first_child()) {
        if (this->get_first_child() != nullptr) {
          // ATTN --- This need to iterate through siblings to traverse
          // the entire children list.  For now matches only if first
          // child
          return (this->get_first_child()->apply_subtree_filter_recursive(filter->get_first_child()));
        } else {
          return nullptr;
        }
      } else {
        if (filter->get_text_value() == get_text_value()) {
          if (this->get_first_child() != nullptr) {
            // ATTN --- This need to iterate through siblings to
            // traverse the entire children list.  For now matches only if
            // first child
            return (this->get_first_child()->apply_subtree_filter_recursive(filter->get_first_child()));
          } else {
            return nullptr;
          }
        }
      }
    }
  }
  return (nullptr);
}


/*****************************************************************************/
// C node APIs

void rw_xml_node_release(
  rw_xml_node_t* node)
{
  static_cast<XMLNode*>(node)->obj_release();
}

rw_yang_node_t* rw_xml_node_get_yang_node(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_yang_node();
}

void rw_xml_node_set_yang_node(
  rw_xml_node_t* node,
  rw_yang_node_t* yang_node)
{
  return static_cast<XMLNode*>(node)->set_yang_node(static_cast<YangNode*>(yang_node));
}

rw_yang_node_t* rw_xml_node_get_reroot_yang_node(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_reroot_yang_node();
}

void rw_xml_node_set_reroot_yang_node(
  rw_xml_node_t* node,
  rw_yang_node_t* reroot_yang_node)
{
  return static_cast<XMLNode*>(node)->set_reroot_yang_node(static_cast<YangNode*>(reroot_yang_node));
}

rw_yang_node_t* rw_xml_node_get_descend_yang_node(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_descend_yang_node();
}

void rw_xml_node_get_text_value(
  rw_xml_node_t* node,
  char** value)
{
  std::string retval = static_cast<XMLNode*>(node)->get_text_value();
  RW_ASSERT(value);
  *value = strdup(retval.c_str());
}

rw_status_t rw_xml_node_set_text_value(
  rw_xml_node_t* node,
  const char* value)
{
  return static_cast<XMLNode*>(node)->set_text_value(value);
}

void rw_xml_node_get_prefix(
  rw_xml_node_t* node,
  char** prefix)
{
  std::string retval = static_cast<XMLNode*>(node)->get_prefix();
  RW_ASSERT(prefix);
  *prefix = strdup(retval.c_str());
}

void rw_xml_node_get_local_name(
  rw_xml_node_t* node,
  char** local_name)
{
  std::string retval = static_cast<XMLNode*>(node)->get_local_name();
  RW_ASSERT(local_name);
  *local_name = strdup(retval.c_str());
}

void rw_xml_node_get_name_space(
  rw_xml_node_t* node,
  char** name_space)
{
  std::string retval = static_cast<XMLNode*>(node)->get_name_space();
  RW_ASSERT(name_space);
  *name_space = strdup(retval.c_str());
}

rw_xml_document_t* rw_xml_node_get_document(
  rw_xml_node_t* node)
{
  return &static_cast<XMLNode*>(node)->get_document();
}

rw_xml_manager_t* rw_xml_node_get_manager(
  rw_xml_node_t* node)
{
  return &static_cast<XMLNode*>(node)->get_manager();
}

rw_xml_node_t* rw_xml_node_get_parent(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_parent();
}

rw_xml_node_t* rw_xml_node_get_first_child(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_first_child();
}

rw_xml_node_t* rw_xml_node_get_last_child(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_last_child();
}

rw_xml_node_t* rw_xml_node_get_next_sibling(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_next_sibling();
}

rw_xml_node_t* rw_xml_node_get_previous_sibling(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_previous_sibling();
}

rw_xml_node_t* rw_xml_node_get_first_element(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_first_element();
}

rw_xml_node_list_t* rw_xml_node_get_children(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->get_children().release();
}

bool_t rw_xml_node_is_equal(
  rw_xml_node_t* node,
  rw_xml_node_t* other_node)
{
  return static_cast<XMLNode*>(node)->is_equal(static_cast<XMLNode*>(other_node));
}

rw_yang_netconf_op_status_t rw_xml_node_validate(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->validate();
}

rw_xml_node_t* rw_xml_node_find(
  rw_xml_node_t* node,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLNode*>(node)->find(local_name, ns);
}

rw_xml_node_t* rw_xml_node_find_value(
  rw_xml_node_t* node,
  const char* local_name,
  const char* value,
  const char* ns)
{
  return static_cast<XMLNode*>(node)->find_value(local_name, value, ns);
}

rw_yang_netconf_op_status_t rw_xml_node_find_list_by_key(
  rw_xml_node_t* node,
  rw_yang_node_t* list_ynode,
  rw_xml_node_t* lookup_node,
  rw_xml_node_t** found_node)
{
  XMLNode* my_found = nullptr;
  rw_yang_netconf_op_status_t ncs = static_cast<XMLNode*>(node)->find_list_by_key(
    static_cast<YangNode*>(list_ynode),
    static_cast<XMLNode*>(lookup_node),
    &my_found);
  if (my_found) {
    RW_ASSERT(found_node);
    *found_node = my_found;
  }
  return ncs;
}

rw_xml_node_t* rw_xml_node_append_child(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_value,
  const char* ns,
  const char* prefix)
{
  return static_cast<XMLNode*>(node)->add_child(local_name, text_value, ns, prefix);
}

rw_xml_node_t* rw_xml_node_append_child_yang(
  rw_xml_node_t* node,
  rw_yang_node_t* yang_node,
  const char* text_value)
{
  return static_cast<XMLNode*>(node)->add_child(static_cast<YangNode*>(yang_node), text_value);
}

rw_xml_node_t* rw_xml_node_import_child(
  rw_xml_node_t* node,
  rw_yang_node_t *ynode,
  rw_xml_node_t* import_node,
  bool_t deep)
{
  return static_cast<XMLNode*>(node)->import_child(static_cast<XMLNode*>(node), static_cast<YangNode*>(ynode), deep);
}

rw_xml_node_t* rw_xml_node_insert_before(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_value,
  const char* ns,
  const char* prefix)
{
  return static_cast<XMLNode*>(node)->insert_before(local_name, text_value, ns, prefix);
}

rw_xml_node_t* rw_xml_node_insert_before_yang(
  rw_xml_node_t*  node,
  rw_yang_node_t* ynode,
  const char* text_value)
{
  return static_cast<XMLNode*>(node)->insert_before(static_cast<YangNode*>(ynode), text_value);
}

rw_xml_node_t* rw_xml_node_import_before(
  rw_xml_node_t* node,
  rw_xml_node_t* import_node,
  rw_yang_node_t* ynode,
  bool_t deep)
{
  return static_cast<XMLNode*>(node)->import_before(static_cast<XMLNode*>(import_node), static_cast<YangNode*>(ynode), deep);
}
rw_xml_node_t* rw_xml_node_insert_after(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_value,
  const char* ns,
  const char* prefix)
{
  return static_cast<XMLNode*>(node)->insert_after(local_name, text_value, ns, prefix);
}

rw_xml_node_t* rw_xml_node_insert_after_yang(
  rw_xml_node_t*  node,
  rw_yang_node_t* ynode,
  const char* text_value)
{
  return static_cast<XMLNode*>(node)->insert_after(static_cast<YangNode*>(ynode), text_value);
}

rw_xml_node_t* rw_xml_node_import_after(
  rw_xml_node_t* node,
  rw_xml_node_t* import_node,
  rw_yang_node_t* ynode,
  bool_t deep)
{
  return static_cast<XMLNode*>(node)->import_after(static_cast<XMLNode*>(import_node), static_cast<YangNode*>(ynode), deep);
}

bool rw_xml_node_remove_child(
  rw_xml_node_t* node,
  rw_xml_node_t* child_node)
{
  return static_cast<XMLNode*>(node)->remove_child(static_cast<XMLNode*>(child_node));
}

rw_yang_netconf_op_status_t rw_xml_node_merge(
  rw_xml_node_t* node,
  rw_xml_node_t* other_node)
{
  return static_cast<XMLNode*>(node)->merge(static_cast<XMLNode*>(other_node));
}

void rw_xml_node_to_string(
  rw_xml_node_t* node,
  char** string)
{
  std::string retval = static_cast<XMLNode*>(node)->to_string();
  RW_ASSERT(string);
  *string = strdup(retval.c_str());
}

void rw_xml_node_to_string_pretty(
  rw_xml_node_t* node,
  char** string)
{
  std::string retval = static_cast<XMLNode*>(node)->to_string_pretty();
  RW_ASSERT(string);
  *string = strdup(retval.c_str());
}

void rw_xml_node_to_cfstring(
  rw_xml_node_t* node,
  CFMutableStringRef cfstring)
{
  std::string retval = static_cast<XMLNode*>(node)->to_string();

  CFRange r = CFRangeMake (0, CFStringGetLength(cfstring));
  CFStringDelete (cfstring, r);
  CFStringAppendCString (cfstring, retval.c_str(), kCFStringEncodingUTF8);

}

rw_status_t rw_xml_node_to_stdout(
  rw_xml_node_t* node)
{
  return static_cast<XMLNode*>(node)->to_stdout();
}

rw_status_t rw_xml_node_to_file(
  rw_xml_node_t* node,
  const char* file_name)
{
  return static_cast<XMLNode*>(node)->to_file(file_name);
}

rw_yang_netconf_op_status_t rw_xml_node_to_pbcm(
  rw_xml_node_t* node,
  ProtobufCMessage* top_pbcm)
{
  return static_cast<XMLNode*>(node)->to_pbcm(top_pbcm);
}


/*****************************************************************************/
// C node list APIs

void rw_xml_node_list_release(
  rw_xml_node_list_t* nodelist)
{
  static_cast<XMLNodeList*>(nodelist)->obj_release();
}

size_t rw_xml_node_list_length(
  rw_xml_node_list_t* nodelist)
{
  return static_cast<XMLNodeList*>(nodelist)->length();
}

rw_xml_node_t* rw_xml_node_list_find(
  rw_xml_node_list_t* nodelist,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLNodeList*>(nodelist)->find(local_name, ns);
}

rw_xml_node_t* rw_xml_node_list_at(
  rw_xml_node_list_t* nodelist,
  size_t index)
{
  return static_cast<XMLNodeList*>(nodelist)->at(index);
}

rw_xml_document_t* rw_xml_node_list_get_document(
  rw_xml_node_list_t* nodelist)
{
  return &static_cast<XMLNodeList*>(nodelist)->get_document();
}

rw_xml_manager_t* rw_xml_node_list_get_manager(
  rw_xml_node_list_t* nodelist)
{
  return &static_cast<XMLNodeList*>(nodelist)->get_manager();
}


/*****************************************************************************/
// C document APIs

void rw_xml_document_release(
  rw_xml_document_t* doc)
{
  static_cast<XMLDocument*>(doc)->obj_release();
}

rw_xml_node_t* rw_xml_document_import_node(
  rw_xml_document_t* doc,
  rw_xml_node_t* node,
  bool_t deep)
{
  return static_cast<XMLDocument*>(doc)->import_node(static_cast<XMLNode*>(node), nullptr, deep).release();
}

rw_xml_node_t* rw_xml_document_get_root_node(
  rw_xml_document_t* doc)
{
  return static_cast<XMLDocument*>(doc)->get_root_node();
}

rw_xml_node_t* rw_xml_document_get_root_child_element(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLDocument*>(doc)->get_root_child_element(local_name, ns);
}

rw_xml_node_list_t* rw_xml_document_get_elements(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLDocument*>(doc)->get_elements(local_name, ns).release();
}

void rw_xml_document_to_string(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns,
  char** string)
{
  std::string retval = static_cast<XMLDocument*>(doc)->to_string(local_name, ns);
  RW_ASSERT(string);
  *string = strdup(retval.c_str());
}

void rw_xml_document_to_string_pretty(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns,
  char** string)
{
  std::string retval = static_cast<XMLDocument*>(doc)->to_string_pretty(local_name, ns);
  RW_ASSERT(string);
  *string = strdup(retval.c_str());
}

void rw_xml_document_to_cfstring(
  rw_xml_document_t* doc,
  CFMutableStringRef cfstring)
{
  std::string retval = static_cast<XMLDocument*>(doc)->get_root_node()->to_string();

  CFRange r = CFRangeMake (0, CFStringGetLength(cfstring));
  CFStringDelete (cfstring, r);

  CFStringAppendCString (cfstring, retval.c_str(), kCFStringEncodingUTF8);
}

rw_status_t rw_xml_document_to_stdout(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLDocument*>(doc)->to_stdout(local_name, ns);
}

rw_status_t rw_xml_document_to_file(
  rw_xml_document_t* doc,
  const char* file_name,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLDocument*>(doc)->to_file(file_name, local_name, ns);
}

bool_t rw_xml_document_is_equal(
  rw_xml_document_t* doc,
  rw_xml_document_t* other_doc)
{
  return static_cast<XMLDocument*>(doc)->is_equal(static_cast<XMLDocument*>(other_doc));
}

rw_xml_manager_t* rw_xml_document_get_manager(
  rw_xml_document_t* doc)
{
  return &static_cast<XMLDocument*>(doc)->get_manager();
}

/* C document APIs from XMLDocument <->XMLDocumentYang collapse
 */


rw_xml_node_t* rw_xml_document_import_node_yang(
  rw_xml_document_t* doc,
  rw_xml_node_t* node,
  rw_yang_node_t* ynode,
  bool_t deep)
{
  return static_cast<XMLDocument*>(doc)->import_node(
    static_cast<XMLNode*>(node),
    static_cast<YangNode*>(ynode),
    deep)
  .release();
}

rw_yang_netconf_op_status_t rw_xml_document_merge(
  rw_xml_document_t* doc,
  rw_xml_document_t* other_doc)
{
  return static_cast<XMLDocument*>(doc)->merge(static_cast<XMLDocument*>(other_doc));
}

/*****************************************************************************/
// C manager APIs

void rw_xml_manager_release(
  rw_xml_manager_t* mgr)
{
  static_cast<XMLManager*>(mgr)->obj_release();
}

rw_xml_document_t* rw_xml_manager_create_document(
  rw_xml_manager_t* mgr,
  const char* local_name,
  const char* ns)
{
  return static_cast<XMLManager*>(mgr)->create_document(local_name, ns).release();
}

rw_xml_document_t* rw_xml_manager_create_document_from_file(
  rw_xml_manager_t* mgr,
  const char* file_name,
  bool_t validate)
{
  return static_cast<XMLManager*>(mgr)->create_document_from_file(file_name, validate).release();
}

rw_xml_document_t* rw_xml_manager_create_document_from_string(
  rw_xml_manager_t* mgr,
  const char* xml_str,
  bool_t validate)
{
  std::string error_out;
  return static_cast<XMLManager*>(mgr)->create_document_from_string(xml_str, error_out, validate).release();
}

/* C manager APIs from XMLNode <->XMLNodeYang collapse
 */

rw_yang_model_t* rw_xml_manager_get_yang_model(rw_xml_manager_t* mgr)
{
  return static_cast<XMLManager*>(mgr)->get_yang_model();
}

rw_xml_document_t* rw_xml_manager_create_yang_document(
  rw_xml_manager_t* mgr,
  rw_yang_node_t* ynode)
{
  return static_cast<XMLManager*>(mgr)->create_document(
    static_cast<YangNode*>(ynode)).release();
}


rw_status_t rw_xml_manager_pb_to_xml(
  rw_xml_manager_t* mgr,
  void *pbcm,
  char * xml_string,
  size_t max_length)
{
  std::string xml_str;
  rw_status_t status =
  static_cast<XMLManager*>(mgr)->pb_to_xml((ProtobufCMessage*) pbcm,
                                           xml_str);

  if (RW_STATUS_SUCCESS != status) {
    // ATTN: need to record an error here
    return status;
  }

  if (xml_str.size() >= max_length) {
    // ATTN: need to record an error here
    return RW_STATUS_FAILURE;
  }

  strcpy (xml_string, xml_str.c_str());
  return RW_STATUS_SUCCESS;
}

rw_status_t rw_xml_manager_pb_to_xml_unrooted(
  rw_xml_manager_t* mgr,
  void *pbcm,
  char * xml_string,
  size_t max_length)
{
  std::string xml_str;
  rw_status_t status =
      static_cast<XMLManager*>(mgr)->pb_to_xml_unrooted((ProtobufCMessage*) pbcm,
                                                        xml_str);

  if (RW_STATUS_SUCCESS != status) {
    // ATTN: need to record an error here
    return status;
  }

  if (xml_str.size() >= max_length) {
    // ATTN: need to record an error here
    return RW_STATUS_FAILURE;
  }

  strcpy (xml_string, xml_str.c_str());
  return RW_STATUS_SUCCESS;
}

rw_status_t rw_xml_manager_pb_to_xml_cfstring(
  rw_xml_manager_t* mgr,
  void *pbcm,
  CFMutableStringRef  cfstring)
{
  std::string xml_str;
  rw_status_t status =
  static_cast<XMLManager*>(mgr)->pb_to_xml((ProtobufCMessage*) pbcm,
                                           xml_str);

  if (RW_STATUS_SUCCESS != status) {
    // ATTN: need to record an error here
    return status;
  }

  CFRange r = CFRangeMake (0, CFStringGetLength(cfstring));
  CFStringDelete (cfstring, r);

  CFStringAppendCString (cfstring, xml_str.c_str(), kCFStringEncodingUTF8);
  return RW_STATUS_SUCCESS;
}

char* rw_xml_manager_pb_to_xml_alloc_string(
  rw_xml_manager_t* mgr,
  void *pbcm)
{
  std::string xml_str;
  rw_status_t status =
  static_cast<XMLManager*>(mgr)->pb_to_xml((ProtobufCMessage*) pbcm,
                                           xml_str);

  if (RW_STATUS_SUCCESS != status) {
    // ATTN: need to record an error here
    return strdup("");
  }

  return strdup(xml_str.c_str());
}

char* rw_xml_manager_pb_to_xml_alloc_string_pretty(
  rw_xml_manager_t* mgr,
  void *pbcm,
  int pretty_print)
{
  std::string xml_str;
  rw_status_t status;

  if (!pretty_print) {
    status = static_cast<XMLManager*>(mgr)->pb_to_xml(
                                           (ProtobufCMessage*) pbcm,
                                           xml_str);
  } else {
    status = static_cast<XMLManager*>(mgr)->pb_to_xml_pretty(
                                           (ProtobufCMessage*) pbcm,
                                           xml_str);
  }

  if (RW_STATUS_SUCCESS != status) {
    // ATTN: need to record an error here
    return strdup("");
  }

  return strdup(xml_str.c_str());
}

rw_status_t rw_xml_manager_xml_to_pb(
    rw_xml_manager_t* mgr,
    const char* xml_string,
    ProtobufCMessage *pbcm,
    char **errout)
{
  std::string error_out;
  rw_status_t s = static_cast<XMLManager*>(mgr)->xml_to_pb(xml_string, pbcm, error_out, nullptr);

  if (s != RW_STATUS_SUCCESS && errout && error_out.length()) {
    *errout = strdup(error_out.c_str());
  }

  return s;
}

static void xml_mgr_protobuf_error_handler(ProtobufCInstance* instance,
                                           const char* errmsg)
{
  XMLManager *mgr = static_cast<XMLManager *> (instance->data);
  RW_ASSERT(mgr);

  if (nullptr != mgr->log_cb_) {
    mgr->log_cb_ (mgr->log_cb_user_data_, RW_XML_LOG_LEVEL_ERROR, "", errmsg);
  }
}

XMLManager::XMLManager()
: log_cb_(nullptr),
  log_cb_user_data_(nullptr),
  memlog_inst_("XMLManager", 0), /* DEFAULT retired_count */
  memlog_buf_(memlog_inst_, "XMLMgrInst", reinterpret_cast<intptr_t>(this))
{
  protobuf_c_instance_set_defaults (&pb_instance_);
  pb_instance_.data = this;
  pb_instance_.error = xml_mgr_protobuf_error_handler;
}

/***************************************************************************
 * Subtree Filtering
 */

rw_status_t XMLDocument::subtree_filter(rw_yang::XMLDocument* filter_dom)
{
  RW_ASSERT (filter_dom);
  std::stringstream log_strm;

  XMLNode* filter_root = filter_dom->get_root_node();
  RW_ASSERT (filter_root);

  XMLNode* data_root = this->get_root_node();
  RW_ASSERT (data_root);

  // Check for empty filter
  if (!filter_root->get_first_child()) return RW_STATUS_SUCCESS;

  // initial filtering
  filter_selection_node(filter_root->get_first_child(), data_root);

  // Data node should always be one step above the filter node
  // Its easier to filter that way
  return filter_containment_node(filter_root->get_first_child(), data_root);
}

rw_status_t XMLDocument::filter_containment_node(XMLNode* filter_node, XMLNode* data_node)
{
  RW_ASSERT (filter_node);
  RW_ASSERT (data_node);
  rw_status_t ret = RW_STATUS_SUCCESS;
  std::stringstream log_strm;

  XMLNodeList::uptr_t children(filter_node->get_children());
  size_t clen = children->length();

  if (clen == 0) {
    return filter_selection_node(filter_node, data_node);
  }

  data_node = data_node->find(filter_node->get_local_name().c_str(),
                              filter_node->get_name_space().c_str());
  if (!data_node) {
    // go to next
    log_strm << "Data node not found: " << filter_node->get_local_name();
    RW_XML_DOC_DEBUG (this, log_strm);
    return RW_STATUS_SUCCESS;
  }

  XMLNode* tmp_data_node = data_node;

  while (tmp_data_node) {
    // Check for the presence of content match node under this
    // containment node.
    // RFC-6241 does not specify any ordering for such nodes as in
    // whether those should be first entry or not. So, we have to
    // do a full scan of children under this containment node.
    std::list<XMLNode*> content_match_nodes;
    std::list<XMLNode*> sibling_nodes;
    find_content_match_nodes(filter_node, content_match_nodes, sibling_nodes);

    // Remove all nodes under data node which are not there in sibling_nodes
    std::list<XMLNode*> delete_nodes;
    XMLNode* data_child_node = tmp_data_node->get_first_child();

    while (data_child_node) {
      auto dnode = data_child_node;
      data_child_node = data_child_node->get_next_sibling();

      // Check if the node is from content_match_nodes
      // if yes, ignore it.
      bool content_match = false;
      for (auto cnode : content_match_nodes) {
        if (cnode->get_local_name() == dnode->get_local_name()) {
          content_match = true;
          break;
        }
      }
      if (content_match) continue;

      bool match = false;
      for (auto snode : sibling_nodes) {
        if (dnode->get_local_name() == snode->get_local_name()) {
          match = true;
          break;
        }
      }
      // If only content match nodes are present and
      // no sibling nodes are mentioned in the filter,
      // then all the sibling nodes must be maintained
      // ELSE
      // If sibling_nodes are present, then irrespective of
      // whether content match nodes are present or not,
      // delete the nodes from data node if they are not present
      // in the filter.
      if (sibling_nodes.size()) {
        if (!match && !dnode->get_yang_node()->is_key()) {
          delete_nodes.push_back(dnode);
        }
      }
    }

    for (auto del_node : delete_nodes) {
      tmp_data_node->remove_child(del_node);
    }
    delete_nodes.clear();

    // filter_content_match_nodes deletes the current XML element
    // i.e tmp_data_node if it does not match the filter criteria.
    // Therefor it should not be used in case filter_content_match_nodes
    // returs RW_STATUS_SUCCESS, which means it got removed from the DOM
    auto next_sibling = tmp_data_node->get_next_sibling();

    bool tmp_data_node_dirty = false;

    // Do the filtering for content match nodes
    if (content_match_nodes.size()) {
      ret = filter_content_match_nodes(content_match_nodes, tmp_data_node);
      if (ret == RW_STATUS_SUCCESS) {
        // tmp_data_node is no longer valid
        tmp_data_node_dirty = true;
      }
    }
    // Now recurse on the sibling nodes
    if (!tmp_data_node_dirty) {
      for (auto snode : sibling_nodes) {
        if (!snode->is_leaf_node()) {
          ret = filter_containment_node(snode, tmp_data_node);
        }
      }

      // delete tmp_data_node if it's empty and not a presence container
      YangNode * ynode = tmp_data_node->get_yang_node();
      if (tmp_data_node->get_first_child() == nullptr &&
          ynode->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER &&
          !ynode->is_presence()) {
        // No children for a non-presence container, they are only present to
        // maintain hierarchy. Remove it
        XMLNode* parent = tmp_data_node->get_parent();
        if (parent) {
          parent->remove_child(tmp_data_node);
        }
      }

    }

    tmp_data_node = next_sibling;
  }



  return RW_STATUS_SUCCESS;
}

rw_status_t XMLDocument::filter_selection_node(XMLNode* filter_node, XMLNode* data_node)
{
  RW_ASSERT (filter_node);
  RW_ASSERT (data_node);

  std::list<XMLNode*> del_nodes;
  auto dnode = data_node->get_first_child();

  while (dnode) {
    auto n = dnode;
    dnode = dnode->get_next_sibling();
    if (n->get_local_name() == filter_node->get_local_name()) continue;
    del_nodes.push_back(n);
  }

  for (auto node : del_nodes) data_node->remove_child(node);

  return RW_STATUS_SUCCESS;
}

void XMLDocument::find_content_match_nodes(XMLNode* filter_node,
                                           std::list<XMLNode*>& content_match_nodes,
                                           std::list<XMLNode*>& sibling_nodes)
{
  RW_ASSERT (filter_node);
  XMLNode* cnode = filter_node->get_first_child();

  while (cnode) {
    auto node = cnode;
    cnode = cnode->get_next_sibling();

    if (node->is_leaf_node() && node->get_text_value().length()) {
      content_match_nodes.push_back(node);
    } else {
      sibling_nodes.push_back(node);
    }
  }
}

rw_status_t XMLDocument::filter_content_match_nodes(std::list<XMLNode*>& content_match_nodes,
                                                    XMLNode* data_node)
{
  RW_ASSERT (data_node);
  std::stringstream log_strm;
  bool match = true;

  for (auto cmnode : content_match_nodes) {
    auto mnode = data_node->find(cmnode->get_local_name().c_str(),
                                 cmnode->get_name_space().c_str());
    if (mnode) {
      if (mnode->get_text_value() != cmnode->get_text_value()) {
        match = false;
        break;
      }
    }
  }

  if (!match && content_match_nodes.size()) {
    data_node->get_parent()->remove_child(data_node);
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

/***************************************************************************
 * Name methods in base
 */

std::string XMLNode::get_full_prefix_text()
{
  YangNode *yn = get_yang_node();
  RW_ASSERT (yn);
  const char *prefix = strlen(yn->get_prefix())? yn->get_prefix():nullptr;
  RW_XML_NODE_DEBUG_STRING (this, " ");
  // <rw-base:name xmlns:rw-base=\"http://riftio.com/ns/riftware-1.0/rw-base\">
  std::string xml_string = "<";
  if (prefix) {
    xml_string += prefix;
    xml_string += ":";
  }
  xml_string += yn->get_name();
  if (prefix) {
    xml_string += " xmlns:";
    xml_string += yn->get_prefix();
    xml_string += "\"";
    xml_string += yn->get_ns();
    xml_string += "\">";
  } else {
    xml_string += ">";
  }

  switch (yn->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_LIST:
      for (YangKeyIter yki=yn->key_begin(); yki != yn->key_end(); yki++) {
        XMLNode *key = find(yki->get_key_node()->get_name(),
                            yki->get_key_node()->get_ns());
        if (key) {
          xml_string += key->get_full_prefix_text() + key->get_full_suffix_text();
        }
      }
      break;

    default:
      xml_string += get_text_value();
  }

  return xml_string;
}

std::string XMLNode::get_full_suffix_text()
{
  // </rw-base:name>
  YangNode *yn = get_yang_node();
  RW_ASSERT (yn);
  std::string xml_string = "</";
  const char *prefix = strlen(yn->get_prefix())? yn->get_prefix():nullptr;
  RW_XML_NODE_DEBUG_STRING (this, " ");
  if (prefix) {
    xml_string += yn->get_prefix();
    xml_string += ":";
  }
  xml_string += yn->get_name();
  xml_string += ">";

  return xml_string;
  // <rw-base:name xmlns:rw-base=\"http://riftio.com/ns/riftware-1.0/rw-base\">

}

XMLNode* XMLNode::get_next_sibling (const std::string& localname,
                                    const std::string& ns)
{
  XMLNode *node = get_next_sibling();

  while (node) {
    if ((localname == node->get_local_name().c_str()) &&
        (ns ==  node->get_name_space())) {
      return node;
    }
    node = node->get_next_sibling();
  }
  return node;
}

XMLNode* XMLNode::get_previous_sibling (const std::string& localname,
                                        const std::string& ns)
{
  XMLNode *node = get_previous_sibling();

  while (node) {
    if ((localname == node->get_local_name().c_str()) &&
        (ns ==  node->get_name_space())) {
      return node;
    }
    node = node->get_previous_sibling();
  }
  return node;
}

void XMLNode::fill_defaults()
{
  YangNode* ynode = get_yang_node();
  RW_ASSERT(ynode);
  fill_defaults(ynode);
}

void XMLNode::fill_rpc_input_with_defaults()
{
  YangNode* ynode = get_yang_node();
  RW_ASSERT(ynode);
  RW_ASSERT(ynode->get_stmt_type() == RW_YANG_STMT_TYPE_RPC);

  ynode = ynode->search_child_rpcio_only("input");
  if (ynode) {
    fill_defaults(ynode);
  }
}

void XMLNode::fill_rpc_output_with_defaults()
{
  YangNode* ynode = get_yang_node();
  RW_ASSERT(ynode);
  RW_ASSERT(ynode->get_stmt_type() == RW_YANG_STMT_TYPE_RPC);

  ynode = ynode->search_child_rpcio_only("output");
  if (ynode) {
    fill_defaults(ynode);
  }
}

void XMLNode::fill_defaults(YangNode* ynode)
{
  for (YangNodeIter it = ynode->child_begin();
       it != ynode->child_end(); ++it) {
    YangNode* ychild = &(*it);
    if (ychild->get_stmt_type() == RW_YANG_STMT_TYPE_LIST) {
      fill_defaults_list(ychild);
      continue;
    }
    const char* default_val = ychild->get_default_value();
    if (!default_val && !ychild->has_default()) {
      // Only default values nodes need to checked for existence
      continue;
    }

    // Default values may be within a choice/case. Check if the choice has
    // another case. If the same case is present
    YangNode* ychoice = ychild->get_choice();
    YangNode* ycase = ychild->get_case();
    bool add_default = true;
    if (ychoice && ycase && (ychoice->get_default_case() != ycase)) {
      add_default = false;
    }

    for (XMLNodeIter xit = child_begin(); xit != child_end(); ++xit) {
      XMLNode* xchild = &(*xit);
      if (xchild->get_local_name() == ychild->get_name() &&
          xchild->get_name_space() == ychild->get_ns()) {
        if (ychild->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER) {
          // The container has a default descendant node
          xchild->fill_defaults();
        }
        // Default node already present in the current tree
        add_default = false;
        break;
      }

      if (!ychoice) {
        // Not part of a choice, check the next xml child
        continue;
      }

      YangNode* other_choice = xchild->get_yang_node()->get_choice();
      if (!other_choice || (other_choice != ychoice)) {
        // Other node is not a choice, or not the same choice, not conflicting
        continue;
      }

      YangNode* other_case = xchild->get_yang_node()->get_case();
      if (other_case && (ycase != other_case)) {
        // There is a node with conflicting case. Hence no default
        add_default = false;
        break;
      }

      // Same case, in-case the case is not default, some-other node in the same
      // case is set. Then add this default, unless the same node is found in
      // the current tree.
      add_default = true;
    }

    if (add_default) {
      XMLNode* xn = add_child(ychild, default_val);
      RW_ASSERT(xn);
      if (ychild->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER) {
          // The container has a default descendant node
          xn->fill_defaults();
      }
    }

  }
}

void XMLNode::fill_defaults_list(YangNode* ylist)
{
  // Go through the the current XML and find all the matching list nodes and
  // fill them with defaults
  for (XMLNodeIter it = child_begin();
       it != child_end(); ++it) {
    XMLNode* xchild = &(*it);
    if (xchild->get_local_name() == ylist->get_name() &&
        xchild->get_name_space() == ylist->get_ns()) {
      xchild->fill_defaults(ylist);
    }
  }
}

rw_yang_netconf_op_status_t XMLNode::to_keyspec(rw_keyspec_path_t **keyspec)
{

  XMLNode *node = this;
  YangNode *yn = node->get_yang_node();

  if (!yn) {
    RW_XML_NODE_ERROR_STRING (this, "No Yang node");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  if (yn->is_leafy() && !yn->is_listy()) {
    // For non-listy leafs (normal leafs)
    // Build the parents ks first, and append the yang node of this node
    node = node->get_parent();
    yn = node->get_yang_node();
  }

  const rw_yang_pb_msgdesc_t* ypbc_msgdesc = yn->get_ypbc_msgdesc();

  *keyspec = nullptr;

  if (!ypbc_msgdesc || !ypbc_msgdesc->schema_path_value) {
    // The schema has not been loaded
    RW_XML_NODE_ERROR_STRING (this, "No schema path");
    return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
  }

  const ProtobufCMessageDescriptor *desc = ypbc_msgdesc->schema_path_value->base.descriptor;
  rw_status_t rs = rw_keyspec_path_create_dup ((const rw_keyspec_path_t *) ypbc_msgdesc->schema_path_value,
                                               NULL,
                                               keyspec);

  RW_ASSERT(rs ==  RW_STATUS_SUCCESS);
  ProtobufCMessage *key_spec = ( ProtobufCMessage *) *keyspec;
  RW_ASSERT(key_spec->descriptor == desc);


  std::stack<XMLNode *> node_stack;

  while (node) {
    yn = node->get_yang_node();
    if ((nullptr != yn) && (!yn->is_root())) {
      node_stack.push (node);
      node = node->get_parent();
    } else {
      break;
    }
  }

  // Get to DOMPATH first.
  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field_by_name (desc, "dompath");

  RW_ASSERT (fld);
  RW_ASSERT (fld->type == PROTOBUF_C_TYPE_MESSAGE);

  desc = (const ProtobufCMessageDescriptor *) fld->descriptor;
  RW_ASSERT (*((char *)key_spec + fld->quantifier_offset));

  char *base = (char *)key_spec + fld->offset;
  char *p_value = 0;

  /* ATTN: This code is very fragile!! - must lookup index symbolically!! */
  size_t dom_path_index = 2; // skip the first 2 fields: rooted, category

  while (!node_stack.empty()) {
    node = node_stack.top();
    RW_ASSERT (dom_path_index < desc->n_fields);
    fld = &desc->fields[dom_path_index];

    const ProtobufCMessageDescriptor *p_desc =
        (const ProtobufCMessageDescriptor *) fld->descriptor;

    p_value = base + fld->offset;

    // Walk the protobuf descriptors and yang key list at the same time.
    // The keys in the protobuf should be in the same order as those of
    // the yang model

    YangKeyIter yki;
    yn = node->get_yang_node();

    RW_ASSERT (yn);

    if (yn->has_keys()) {
      yki = yn->key_begin();
    }

    for (size_t k_index = 0; k_index < p_desc->n_fields; k_index++) {

      fld = &p_desc->fields[k_index];
      if (RW_SCHEMA_TAG_ELEMENT_KEY_START > fld->id ||
          RW_SCHEMA_TAG_ELEMENT_KEY_END < fld->id) {
        // not a key.
        continue;
      }

      rw_yang_netconf_op_status_t ncrs;

      // keys should always be optional submessages.
      RW_ASSERT (PROTOBUF_C_LABEL_OPTIONAL == fld->label);
      RW_ASSERT (PROTOBUF_C_TYPE_MESSAGE == fld->type);

      const ProtobufCMessageDescriptor *k_desc =
          (const ProtobufCMessageDescriptor *) fld->descriptor;

      if (yn->is_leafy()) {
        // leaf list
        const ProtobufCFieldDescriptor *l_fld =
            protobuf_c_message_descriptor_get_field_by_name (k_desc, yn->get_pbc_field_name());

        RW_ASSERT(l_fld);

        ncrs = set_pb_field_from_xml (
            nullptr,(ProtobufCMessage *) (p_value + fld->offset), l_fld, node);

        if (RW_YANG_NETCONF_OP_STATUS_OK == ncrs) {
          *(int *)(p_value + fld->quantifier_offset) = 1;
        }
        return ncrs;
      }

      RW_ASSERT (node->get_yang_node()->key_end() != yki);
      YangNode *k_node = yki->get_key_node();

      RW_ASSERT (k_node);
      const char *k_name = k_node->get_pbc_field_name();

      RW_ASSERT (nullptr != k_name);

      /* ATTN: This code is wrong! It should be using the yang node name, not PBC node name! */
      const ProtobufCFieldDescriptor *key_fld =
             protobuf_c_message_descriptor_get_field_by_name (k_desc, k_name);
      RW_ASSERT (key_fld);

      // The value to set is in the xml node.
      XMLNode *k_xml = node->find(k_node->get_name(), k_node->get_ns());
      if (!k_xml) {
        // No key value for this key, try the others
        yki++;
        continue;
      }

      ncrs = set_pb_field_from_xml (
          nullptr,(ProtobufCMessage *) (p_value + fld->offset), key_fld, k_xml);

      if (RW_YANG_NETCONF_OP_STATUS_OK == ncrs) {
        *(int *)(p_value + fld->quantifier_offset) = 1;
      }

      yki++;
    }
    dom_path_index++;
    node_stack.pop();
  }

  yn = get_yang_node();
  if (yn->is_leafy() && !yn->is_key() && !yn->is_listy()) {

    rs = rw_keyspec_path_create_leaf_entry(*keyspec, NULL, ypbc_msgdesc, yn->get_name());
    RW_ASSERT(RW_STATUS_SUCCESS == rs);

  }
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

void XMLWalkerInOrder::walk(XMLVisitor *visitor)
{
  if (curr_node_ == nullptr) {
    return;
  }
  level_ = 0;
  path_[0] =  curr_node_;
  walk_recursive(curr_node_, visitor);

  return;
}

void XMLWalkerInOrder::walk_recursive(XMLNode *node, XMLVisitor *visitor)
{
  XMLNode *next_node = nullptr;

  RW_ASSERT(node);
  RW_ASSERT(visitor);

  rw_xml_next_action_t action = visitor->visit(node, path_, level_);

  switch(action) {
    case RW_XML_ACTION_NEXT:
      // Find the next node in this XML;
      next_node = node->get_first_child();
      if (next_node == nullptr) {
        next_node = node->get_next_sibling();
      } else {
        RW_ASSERT(level_ < (RW_MAX_XML_NODE_DEPTH - 1));
        path_[++level_] = next_node;
      }
      // if next_node is no null means this is the next sibling adjust the path
      if (next_node != nullptr) {
        path_[level_] = next_node;
      }
      // If the first child and next sibling is both null
      // we are done with this subtree. Let us pop a level
      // at a time and start up to the find next sibling
      while (next_node == nullptr && level_ > 0) {
        path_[level_] = nullptr;
        XMLNode* parent = path_[--level_];
        RW_ASSERT(parent);
        next_node = parent->get_next_sibling();
        if (next_node != nullptr) {
          // We found a sibling in the ancestor hierarchy
          // let us add start traversing the sub tree and push it to the path
          path_[level_] = next_node;
          break;
        }
      }
    break;
    case RW_XML_ACTION_FIRST_CHILD:
      next_node = node->get_first_child();
      if (next_node != nullptr) {
        // Found a child and going deeper - push it to the path
        RW_ASSERT(level_ < (RW_MAX_XML_NODE_DEPTH - 1));
        path_[++level_] = node;
      }
    break;
    case RW_XML_ACTION_NEXT_SIBLING:
      next_node = node->get_next_sibling();
      if (next_node != nullptr) {
        // Found a sibling -- update the path to reflect the new sibling
        path_[level_] = node;
      }
      // If a level of siblings has been exhausted, go up and visit
      // unvisited nodes
      while (next_node == nullptr && level_ > 0) {
        path_[level_] = nullptr;
        XMLNode* parent = path_[--level_];
        RW_ASSERT(parent);
        next_node = parent->get_next_sibling();
        if (next_node != nullptr) {
          // We found a sibling in the ancestor hierarchy
          // let us add start traversing the sub tree and push it to the path
          path_[level_] = next_node;
          break;
        }
      }
    break;
    case RW_XML_ACTION_TERMINATE:
    case RW_XML_ACTION_ABORT:
      return;
    default:
      // Hmmm ... Assert for now
      RW_ASSERT(0);
  }
  if (next_node) {
    walk_recursive(next_node, visitor);
  }
  return;
}

XMLVisitor::XMLVisitor()
           :netconf_status(RW_YANG_NETCONF_OP_STATUS_OK),
            curr_node_(nullptr),
            level_(-1)
{
  memset(path_, 0, sizeof(path_));
}

rw_xml_next_action_t XMLCopier::visit(XMLNode* node, XMLNode** path, int32_t level)
{

  RW_ASSERT(level>= 0 && level < RW_MAX_XML_NODE_DEPTH);

  if (node == nullptr) {
    return RW_XML_ACTION_TERMINATE;
  }

  RW_ASSERT(to_node_);

  // This is the root node then this is the first time call
  if (level == 0) {
    RW_ASSERT(level_ == -1);

    curr_node_ = to_node_->add_child(node->get_yang_node(), node->get_text_value().c_str());
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
      curr_node_ = to_parent->add_child(node->get_yang_node(), node->get_text_value().c_str());
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

rw_xml_next_action_t XMLYangValidatedCopier::visit(XMLNode* node, XMLNode** path, int32_t level)
{

  RW_ASSERT(level>= 0 && level < RW_MAX_XML_NODE_DEPTH);

  if (node == nullptr) {
    return RW_XML_ACTION_TERMINATE;
  }

  if (nullptr == node->get_yang_node()) {
    return RW_XML_ACTION_NEXT;
  }

  return XMLCopier::visit (node, path, level);
}

/*******************************************************************************
 * Default implementation
 */
rw_xml_next_action_t Builder::append_child (XMLNode *node)
{
  UNUSED (node);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;
}

rw_xml_next_action_t Builder::append_child (const char *local_name,
                                            const char *ns,
                                            const char *text_val)
{
  UNUSED (local_name);
  UNUSED (ns);
  UNUSED (text_val);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;

}

rw_xml_next_action_t Builder::mark_child_for_deletion(const char *local_name,
                                                      const char *ns)
{
  UNUSED (local_name);
  UNUSED (ns);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;

}

rw_xml_next_action_t Builder::append_child_enum (const char *local_name,
                                                 const char *ns,
                                                 int32_t val)
{
  UNUSED (local_name);
  UNUSED (ns);
  UNUSED (val);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;

}

rw_xml_next_action_t Builder::push (XMLNode *node)
{
  UNUSED (node);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;

}

rw_xml_next_action_t Builder::push (const char *local_name,
                                    const char *ns)
{
  UNUSED (local_name);
  UNUSED (ns);

  RW_ASSERT(0);
  return RW_XML_ACTION_ABORT;

}

/******************************************************************************/
/* XMLBuilder - Builds an XML tree, associating with YANG nodes if they can be
w * found
 */

XMLBuilder::~XMLBuilder()
{
}

XMLBuilder::XMLBuilder (XMLNode *node)
    :current_node_ (node)
{
}

XMLBuilder::XMLBuilder (XMLManager *mgr)
{
  // The first node is usually the root of the document.
  merge_ =
      std::move(mgr->create_document(mgr->get_yang_model()->get_root_node()));

  current_node_ = merge_->get_root_node();
}


rw_xml_next_action_t XMLBuilder::append_child_enum (const char *local_name,
                                                    const char *ns,
                                                    int32_t value)
{
  YangNode* my_yang_node = current_node_->get_descend_yang_node();

  if (my_yang_node) {
    YangNode *child_yang = my_yang_node->search_child (local_name, ns);
    if (nullptr == child_yang) {
      return RW_XML_ACTION_ABORT;
    }

    std::string text_val = child_yang->get_enum_string(value);
    if (!text_val.length()) {
      // error - abort traversal
      return RW_XML_ACTION_ABORT;
    }

    if (current_node_->add_child (child_yang, text_val.c_str()) == nullptr) {
      // error - abort traversal
      return RW_XML_ACTION_ABORT;
    }
    return RW_XML_ACTION_NEXT;
  }
  return RW_XML_ACTION_ABORT;
}



rw_xml_next_action_t XMLBuilder::append_child (const char *local_name,
                                               const char *ns,
                                               const char *text_val)
{
  YangNode* my_yang_node = current_node_->get_descend_yang_node();
  RW_ASSERT (my_yang_node);

  YangNode *child_yang = my_yang_node->search_child (local_name, ns);
  RW_ASSERT(child_yang);

  if (child_yang->is_leafy() &&
      (RW_YANG_LEAF_TYPE_EMPTY != child_yang->get_type()->get_leaf_type())
      && (nullptr == text_val)) {
    // This is an error, and should signal a terminate.
    // but work around a confd bug for now.
    return RW_XML_ACTION_TERMINATE;
  }

  if (current_node_->add_child (child_yang, text_val) == nullptr) {
    // error - abort traversal
    return RW_XML_ACTION_ABORT;
  }
  return RW_XML_ACTION_NEXT;
}

rw_xml_next_action_t XMLBuilder::mark_child_for_deletion (const char *local_name,
                                                          const char *ns)
{
  YangNode* my_yang_node = current_node_->get_descend_yang_node();
  RW_ASSERT (my_yang_node);

  YangNode *child_yang = my_yang_node->search_child (local_name, ns);
  RW_ASSERT(child_yang);


  // Append the child yang
  XMLNode *child = current_node_->add_child (child_yang);
  RW_ASSERT(nullptr != child);

  merge_op_ = false;

  if (child_yang->has_keys()) {
    // This is a list. Need to push this node, get all the keys, and be back
    // to generate a key spec
    stack_.push (current_node_);
    current_node_ = child;
    list_delete_ = true;
    return RW_XML_ACTION_NEXT;
  }
  rw_keyspec_path_t *ks = nullptr;
  rw_yang_netconf_op_status_t ncrs = RW_YANG_NETCONF_OP_STATUS_OK;

  ncrs = child->to_keyspec (&ks);

  if (ncrs != RW_YANG_NETCONF_OP_STATUS_OK) {
    RW_XML_NODE_ERROR_STRING (child, "to_keyspec failed");
    return RW_XML_ACTION_ABORT;
  }

  bool ok;
  ok = current_node_->remove_child(child);
  RW_ASSERT(ok);

  if (nullptr != ks) {
    delete_ks_.push_back (uptr_ks(ks));
    ks = nullptr;
  }

  return RW_XML_ACTION_NEXT;
}

rw_xml_next_action_t XMLBuilder::push (const char *local_name,
                                       const char *ns)
{
  stack_.push (current_node_);

  if (!merge_op_) merge_op_ = true;

  YangNode* my_yang_node = current_node_->get_descend_yang_node();
  RW_ASSERT (my_yang_node);

  YangNode *child_yang = my_yang_node->search_child (local_name, ns);
  RW_ASSERT(child_yang);

  if (child_yang->is_leafy()) {
    // This is an error, and should signal a terminate.
    // but work around a confd bug for now.
    return RW_XML_ACTION_TERMINATE;
  }

  current_node_ = current_node_->add_child (child_yang);
  if ( current_node_ == nullptr) {
    // error - abort traversal
    return RW_XML_ACTION_ABORT;
  }
  return RW_XML_ACTION_NEXT;
}

rw_xml_next_action_t XMLBuilder::pop ()
{
  if (!stack_.size()) {
    return RW_XML_ACTION_ABORT;
  }


  // If this is a listy node that was earlier marked for deletion,
  // generate a keyspec corresponding to it, add it to the list of keyspecs
  // and delete the node from the tree.
  if (list_delete_) {

    list_delete_ = false;
    YangNode *yn = current_node_->get_yang_node();
    RW_ASSERT(yn);
    RW_ASSERT(yn->has_keys());

    rw_keyspec_path_t *ks = nullptr;
    rw_yang_netconf_op_status_t ncrs = current_node_->to_keyspec (&ks);

    if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
      std::string log ("XML node to keyspec conversion failed: ");
      log += current_node_->get_local_name();
      RW_BUILDER_LOG_ERROR (log);
      return RW_XML_ACTION_ABORT;
    }

    RW_ASSERT(current_node_->get_parent());

    bool ok = current_node_->get_parent()->remove_child(current_node_);
    RW_ASSERT(ok);

    delete_ks_.push_back (uptr_ks(ks));
    ks = nullptr;
  } else {
    // There is no need to do merge if this is
    // a strictly delete operation
    if (!merge_op_ && current_node_) {
      current_node_->get_parent()->remove_child(current_node_);
    }
  }

  current_node_ = stack_.top();
  stack_.pop();

  return RW_XML_ACTION_NEXT;
}

Traverser::Traverser(Builder *builder)
    : builder_ (builder)
    , memlog_inst_("Traverser", 0) /* DEFAULT retired_count */
    , memlog_buf_(memlog_inst_, "TraverserInst", reinterpret_cast<intptr_t>(this))
{
}

Traverser::~Traverser()
{
}

rw_yang_netconf_op_status_t Traverser::traverse()
{
  rw_xml_next_action_t action = RW_XML_ACTION_NEXT;

  while (true)
  {
    switch (action) {
      case RW_XML_ACTION_NEXT:
        action = build_next();
        break;

      case RW_XML_ACTION_NEXT_SIBLING:
        action = build_next_sibling();
        break;

      case RW_XML_ACTION_TERMINATE:
        stop();
        return RW_YANG_NETCONF_OP_STATUS_OK;

      case RW_XML_ACTION_ABORT:
        abort_traversal();
        return RW_YANG_NETCONF_OP_STATUS_FAILED;

      default:
        RW_ASSERT(0);
    }
  }

  RW_ASSERT(0);
  return RW_YANG_NETCONF_OP_STATUS_FAILED;
}

XMLTraverser::XMLTraverser(Builder *builder, XMLNode *node)
    :Traverser (builder),
     current_node_ (node)
{
  current_index_ = 0;
}

XMLTraverser::~XMLTraverser()
{
}

rw_xml_next_action_t XMLTraverser::build_next()
{

  while ((nullptr == current_node_ ||
          (current_node_->get_children()->length() <= current_index_))
         && (!stack_.empty())) {

    if (!current_node_) {

      std::tie (current_node_,current_index_) = stack_.top();
      builder_->pop();
      stack_.pop();
    }

    // We are always building the children of the current node.
    RW_ASSERT (!current_node_->get_yang_node()->is_leafy());

    if (current_node_->get_children()->length() <= current_index_) {
      // This level has been exhausted - set current node to nullptr,
      // will causing popping of the stack.
      current_node_ = nullptr;
      continue;
    }

  }

  if ((nullptr == current_node_) ||
      (current_node_->get_children()->length() <= current_index_)) {
    return RW_XML_ACTION_TERMINATE;
  }

  // For a list, do the keys first - this helps with confd interactions
  YangNode *p_yn = current_node_->get_yang_node();

  if (p_yn->has_keys() && !current_index_) {

    rw_xml_next_action_t ret;

    for (YangKeyIter yki = p_yn->key_begin(); yki != p_yn->key_end(); yki ++) {
      YangNode *k_node = yki->get_key_node();
      RW_ASSERT(k_node);

      XMLNode *key_to_build = current_node_->find (k_node->get_name(), k_node->get_ns());
      if (key_to_build) {
        ret = builder_->append_child(key_to_build);
        if (ret != RW_XML_ACTION_NEXT) {
          // some type of error?
          return ret;
        }
      }
    }
  }

  XMLNode *to_build = current_node_->get_children()->at(current_index_);
  current_index_++;

  YangNode *yn = to_build->get_yang_node();

  if ((nullptr == yn) || yn->is_key()) {
    return RW_XML_ACTION_NEXT;
  }

  if (yn->is_leafy()) {
    return builder_->append_child(to_build);
  }

  // push myself before setting up current_node_
  stack_.push (std::make_tuple(current_node_, current_index_));

  current_node_ = to_build;
  current_index_ = 0;

  return builder_->push(to_build);
}

rw_xml_next_action_t XMLTraverser::build_next_sibling()
{
  std::tie (current_node_,current_index_) = stack_.top();
  stack_.pop();
  return build_next();

}

rw_yang_netconf_op_status_t XMLTraverser::stop()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}

rw_yang_netconf_op_status_t XMLTraverser::abort_traversal()
{
  return RW_YANG_NETCONF_OP_STATUS_OK;
}


