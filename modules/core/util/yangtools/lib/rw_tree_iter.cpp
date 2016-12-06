
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

#include "rw_tree_iter.h"
#include "confd_xml.h"

using namespace rw_yang;

rw_status_t
RwXMLTreeIterator::move_to_child (XMLNode *node)
{
  RW_ASSERT(node);
  ptr_ = node;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwXMLTreeIterator::move_to_first_child()
{
  if (!ptr_) {
    return RW_STATUS_FAILURE;
  }
  
  auto child = ptr_->get_first_child();
  
  if (!child) {
    return RW_STATUS_FAILURE;
  }
  
  ptr_ = child;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwXMLTreeIterator::move_to_next_sibling()
{
  if (!ptr_) {
    return RW_STATUS_FAILURE;
  }
  
  auto sibling = ptr_->get_next_sibling();
  if (!sibling) {
    return RW_STATUS_FAILURE;
  }

  ptr_ = sibling;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwXMLTreeIterator::move_to_parent()
{
  if (!ptr_) {
    return RW_STATUS_FAILURE;
  }
  auto parent = ptr_->get_parent();

  if (!parent) {
    return RW_STATUS_FAILURE;
  }
  
  ptr_= parent;
  return RW_STATUS_SUCCESS;
}


rw_status_t
RwXMLTreeIterator::get_value(XMLNode*& data) const
{
  RW_ASSERT(ptr_);
  data = ptr_;
  return RW_STATUS_SUCCESS;
}

rw_status_t
RwXMLTreeIterator::get_value(rw_ylib_data_t* data) const
{
  RW_ASSERT(data);
  RW_ASSERT(ptr_->get_yang_node());

  data->type = RW_YLIB_DATA_TYPE_XML_NODE;
  data->xml = ptr_;
  
  return RW_STATUS_SUCCESS;
}


const char *
RwXMLTreeIterator::get_yang_node_name() const
{
  YangNode *yn = ptr_->get_yang_node();
  RW_ASSERT(yn);

  return yn->get_name();
}

const char *
RwXMLTreeIterator::get_yang_ns() const
{
  YangNode *yn = ptr_->get_yang_node();
  RW_ASSERT(yn);

  return yn->get_ns();
}

static
rw_tree_walker_status_t add_xml_child (XMLNode *parent,
                                       ProtobufCFieldInfo *child_pb,
                                       const char *yang_node_name,
                                       const char *yang_ns,
                                       XMLNode*& child);
static
rw_tree_walker_status_t add_xml_child (XMLNode *parent,
                                       XMLNode *child_src,
                                       XMLNode*& child_tgt);

rw_status_t
RwPbcmTreeIterator::move_to_child(const RwPbcmTreeIterator *iter)
{
  // The iterators should be the same till the top
  if (root_ != iter->root_) {
    return RW_STATUS_FAILURE;
  }

  if (node_state_.size() != iter->node_state_.size() - 1) {
    return RW_STATUS_FAILURE;
  }

  // ATTN: The std::stack does not allow this - if this check needs to be added, switch
  // to a list
  
  // if (!node_state_.empty()) {
  //   for (auto parent = node_state_.cbegin(), auto child = iter->node_state_.c_begin();
  //        parent != node_state_.end(); parent ++; child++)
  //   {
  //     if (*parent != *child) {
  //       return RW_STATUS_FAILURE;
  //     }
  //   }
  // }

  node_state_.push(iter->node_state_.top());
  return RW_STATUS_SUCCESS;
  
}

// Private functions called by other methods

rw_tree_walker_status_t add_xml_child (XMLNode *parent,
                                       ProtobufCFieldInfo *child_pb,
                                       const char *yang_node_name,
                                       const char *yang_ns,
                                       XMLNode*& child)
{
  YangNode *yn = parent->get_yang_node();
  if (!yn) {
    return RW_TREE_WALKER_FAILURE;
  }

  const ProtobufCFieldDescriptor *fdesc = child_pb->fdesc;
  
  if (nullptr == fdesc) {
    return RW_TREE_WALKER_FAILURE;
  }
    

  YangNode *child_yn = yn->search_child (yang_node_name, yang_ns);

  if (nullptr == child_yn) {
    return RW_TREE_WALKER_FAILURE;
  }
  
  
  if (fdesc->type == PROTOBUF_C_TYPE_MESSAGE) {

    child = parent->add_child (child_yn);
    if (nullptr == child) {
      return RW_TREE_WALKER_FAILURE;
    }

    return RW_TREE_WALKER_SUCCESS;
  }
  
  RW_ASSERT(child_yn->get_type());
  rw_yang_leaf_type_t leaf_type = child_yn->get_type()->get_leaf_type();
  
  char value_str[1024];
  size_t len = 1024;

  value_str[0] = 0;
  const char *val_p = nullptr;

  switch (leaf_type) {
    case RW_YANG_LEAF_TYPE_EMPTY:
      break;
    case RW_YANG_LEAF_TYPE_ENUM:
      val_p = protobuf_c_message_get_yang_enum (child_pb);
      if (!val_p) {
        return RW_TREE_WALKER_FAILURE;
      }
      break;
    default:
      bool ok = protobuf_c_field_get_text_value (nullptr, fdesc, value_str, &len, (void *) child_pb->element);
      if (!ok) {
        return RW_TREE_WALKER_FAILURE;
      }
      val_p = value_str;
  }

  child = parent->add_child (child_yn, val_p);
  if (nullptr == child) {
    return RW_TREE_WALKER_FAILURE;
  }

  return RW_TREE_WALKER_SUCCESS;
}
      
static
rw_tree_walker_status_t add_xml_child (XMLNode *parent,
                                        XMLNode *child_src,
                                        XMLNode*& child_tgt)
{
  child_tgt = nullptr;
  std::string name = child_src->get_local_name();
  std::string ns = child_src->get_name_space();
  std::string value = child_src->get_text_value();

  if (value.length()) {
    child_tgt = parent->add_child (name.c_str(), value.c_str(), ns.c_str());
  } else {
    child_tgt = parent->add_child (name.c_str(), nullptr, ns.c_str());
  }

  if (nullptr != child_tgt) {
    return RW_TREE_WALKER_SUCCESS;
  }

  return RW_TREE_WALKER_FAILURE;
}


rw_status_t
RwPbcmTreeIterator::move_to_child(uint32_t tag)
{
  ProtobufCMessage *msg;
  rw_status_t rs = get_value(msg);
  
  if (RW_STATUS_SUCCESS != rs) {
    return rs;
  }

  const ProtobufCFieldDescriptor *fd =
      protobuf_c_message_descriptor_get_field (msg->descriptor, tag);
  if (nullptr == fd) {
    return rs;
  }
  
  ProtobufCFieldInfo info;
  
  bool ok = protobuf_c_message_get_field_instance (nullptr, msg, fd,
                                                   0, &info);
  if (!ok) {
    return RW_STATUS_FAILURE;
  }

  // Find the index of the field - Should this be a function in protoland?
  const ProtobufCMessageDescriptor *desc = msg->descriptor;
  RW_ASSERT(desc);
  unsigned int field_id;
  for (field_id = 0; field_id < desc->n_fields; field_id++) {
    if (desc->fields[field_id].id == tag) {
    
      node_state_.push (std::make_tuple (msg, field_id, 0));
      return RW_STATUS_SUCCESS;
    }
  }
  
  RW_ASSERT_NOT_REACHED();
}


rw_status_t
RwPbcmTreeIterator::move_to_child(const ProtobufCFieldDescriptor *fdesc,
                                  rw_ylib_list_position_e position_type,
                                  unsigned int position)
{
  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;

  if (!node_state_.size()) {
    msg = root_;
  } else {
    std::tie(msg, field_id, index) = node_state_.top();
    msg = protobuf_c_message_get_sub_message(msg, field_id, index);
  }

  if (nullptr == msg) {
    return RW_STATUS_FAILURE;
  }

  for ( field_id= 0; field_id < msg->descriptor->n_fields; field_id++) {
    if (msg->descriptor->fields[field_id].id == fdesc->id) {
      break;
    }
  }
  
  if (field_id ==  msg->descriptor->n_fields) {
    return RW_STATUS_FAILURE;
  }

  if (!protobuf_c_message_is_field_present (msg, field_id)) {
    return RW_STATUS_FAILURE;
  }
  
  if (PROTOBUF_C_LABEL_REPEATED == fdesc->label) {
    size_t count = protobuf_c_message_get_field_count (msg, fdesc);

    if (!count) {
      return RW_STATUS_FAILURE;
    }

    switch (position_type) {
      case RW_YLIB_LIST_POSITION_FIRST:
        index = 0;
        break;

      case RW_YLIB_LIST_POSITION_LAST:
        index = count - 1;
        break;

      case RW_YLIB_LIST_POSITION_SPECIFIED:
        if (position <= count) {
          return RW_STATUS_FAILURE;
        }
        index = position;
    }
  }

  node_state_.push (std::make_tuple (msg, field_id, index));
  return RW_STATUS_SUCCESS;
}


rw_status_t
RwPbcmTreeIterator::move_to_first_child()
{
  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;

  if (!node_state_.size()) {
    msg = root_;
  } else {
    std::tie(msg, field_id, index) = node_state_.top();
    msg = protobuf_c_message_get_sub_message(msg, field_id, index);
  }

  if (nullptr == msg) {
    return RW_STATUS_FAILURE;
  }
  
  size_t i;
  for ( i= 0; i < msg->descriptor->n_fields; i++) {
    if (protobuf_c_message_is_field_present (msg, i)) {
      break;
    }
  }

  if (i ==  msg->descriptor->n_fields) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(i < msg->descriptor->n_fields);

  node_state_.push (std::make_tuple (msg, i, 0));

  return RW_STATUS_SUCCESS;
  
}

rw_status_t
RwPbcmTreeIterator::move_to_next_list_entry()
{
    if (!node_state_.size()) {
    return RW_STATUS_FAILURE;
  }
  
  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;

  std::tie(msg, field_id, index) = node_state_.top();
  
  // If the current message field is listy, and has more,
  // return that
  RW_ASSERT(field_id < msg->descriptor->n_fields);
  
  const ProtobufCFieldDescriptor *fld = &msg->descriptor->fields[field_id];
  if ((fld->label == PROTOBUF_C_LABEL_REPEATED) && 
      (*(unsigned int *)((char *)msg + fld->quantifier_offset) > index + 1)) {
    index++;
    node_state_.pop();
    node_state_.push(std::make_tuple (msg,field_id, index));
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

rw_status_t
RwPbcmTreeIterator::move_to_next_sibling()
{
  if (!node_state_.size()) {
    return RW_STATUS_FAILURE;
  }
  
  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;

  std::tie(msg, field_id, index) = node_state_.top();
  unsigned int i;
  
  // If the current message field is listy, and has more,
  // return that
  RW_ASSERT(field_id < msg->descriptor->n_fields);
  
  const ProtobufCFieldDescriptor *fld = &msg->descriptor->fields[field_id];
  if ((fld->label == PROTOBUF_C_LABEL_REPEATED) && 
      (*(unsigned int *)((char *)msg + fld->quantifier_offset) > index + 1)) {
    i = field_id;
    index++;
  } else {
    index = 0;
    for ( i = field_id + 1; i < msg->descriptor->n_fields; i++)  {
      if (protobuf_c_message_is_field_present (msg, i)) {
        break;
      }
    }    
    if (i ==  msg->descriptor->n_fields) {
      return RW_STATUS_FAILURE;
    }    
  }

  RW_ASSERT(i < msg->descriptor->n_fields);

  node_state_.pop();
  node_state_.push (std::make_tuple (msg, i, index));

  return RW_STATUS_SUCCESS;
}

rw_status_t
RwPbcmTreeIterator::move_to_parent()
{
  if (!node_state_.size()) {
    return RW_STATUS_FAILURE;
  }
  
  node_state_.pop();

  return RW_STATUS_SUCCESS;
}


rw_status_t
RwPbcmTreeIterator::get_value(ProtobufCMessage*& msg) const
{
  if (!node_state_.size()) {    
    msg = root_;    
    return RW_STATUS_SUCCESS;
  }

  ProtobufCMessage *parent = nullptr;
  unsigned int field_id = 0, index = 0;
  
  std::tie(parent, field_id, index) = node_state_.top();

  msg = protobuf_c_message_get_sub_message (parent, field_id, index);
  if (nullptr != msg) {
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

rw_status_t
RwPbcmTreeIterator::get_value(rw_ylib_data_t* data) const
{
  data->type = RW_YLIB_DATA_TYPE_PB;
  ProtobufCFieldInfo *v = &data->rw_pb;

  if (!node_state_.size()) {
    
    v->fdesc = nullptr;
    v->element = root_;
    
    return RW_STATUS_SUCCESS;
  }

  const ProtobufCFieldDescriptor *fdesc;
  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;
  
  std::tie(msg, field_id, index) = node_state_.top();
  fdesc = &msg->descriptor->fields[field_id];
  bool ok = protobuf_c_message_get_field_instance (nullptr, msg, fdesc,
                                                   index, v);
  if (ok) {
    return RW_STATUS_SUCCESS;
  }
  return RW_STATUS_FAILURE;
}

void
RwPbcmTreeIterator::get_lowest_msg_and_field_desc(const ProtobufCMessageDescriptor*& mdesc,
                                                  const ProtobufCFieldDescriptor*& fdesc) const
{
  mdesc = 0;
  fdesc = 0;

  if (!node_state_.size()) {
    mdesc = root_->descriptor;
    return;
  }

  ProtobufCMessage *msg = nullptr;
  unsigned int field_id = 0, index = 0;
  
  std::tie(msg, field_id, index) = node_state_.top();
  mdesc = msg->descriptor;
  fdesc = &mdesc->fields[field_id];

  if (PROTOBUF_C_TYPE_MESSAGE == fdesc->type) {
    mdesc = (const ProtobufCMessageDescriptor *)fdesc->descriptor;
  }
  
  return;
}
const char *
RwPbcmTreeIterator::get_yang_node_name() const
{
  const ProtobufCMessageDescriptor *mdesc;
  const ProtobufCFieldDescriptor *fdesc;

  get_lowest_msg_and_field_desc (mdesc, fdesc);
  if ((nullptr == fdesc) || (PROTOBUF_C_TYPE_MESSAGE == fdesc->type)) {
    if (nullptr == mdesc->ypbc_mdesc) {
      return nullptr;
    }

    return mdesc->ypbc_mdesc->yang_node_name;
  }
  
  return protobuf_c_message_get_yang_node_name(mdesc, fdesc);  
}

bool
RwPbcmTreeIterator::is_leafy() const
{
  const ProtobufCMessageDescriptor *mdesc;
  const ProtobufCFieldDescriptor *fdesc;
  
  get_lowest_msg_and_field_desc (mdesc, fdesc);
  
  return (PROTOBUF_C_TYPE_MESSAGE != fdesc->type);
}

const char *
RwPbcmTreeIterator::get_yang_ns() const
{
  const ProtobufCMessageDescriptor *mdesc;
  const ProtobufCFieldDescriptor *fdesc;

  get_lowest_msg_and_field_desc (mdesc, fdesc);
  RW_ASSERT(mdesc);
  
  const rw_yang_pb_msgdesc_t *ypbc =  mdesc->ypbc_mdesc;
  if ((nullptr == ypbc) || (nullptr == ypbc->module)) {
    return nullptr;
  }
  
  return ypbc->module->ns;
}

static
rw_tree_walker_status_t add_pbcm_child (ProtobufCMessage *parent,
                                        ProtobufCFieldInfo *child);
static
rw_tree_walker_status_t add_pbcm_child (ProtobufCMessage *parent,
                                        XMLNode *node,
                                        ProtobufCFieldInfo *child);




// Free functions that deal with RwPbcmTreeIterator. namespaced.
namespace rw_yang {
rw_status_t get_string_value (const RwPbcmTreeIterator *iter,
                              std::string& str)
{
  rw_ylib_data_t val;  
  rw_status_t rs = iter->get_value(&val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  str.clear();

  return get_string_value (&val.rw_pb, str);
}


// Copiers for PBCM iterator
rw_tree_walker_status_t
build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                     RwPbcmTreeIterator* child_iter)
{
  ProtobufCMessage *parent;
  rw_status_t rs = parent_iter->get_value (parent);  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (nullptr == parent) {
    return RW_TREE_WALKER_FAILURE;
  }
  
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  
  return add_pbcm_child (parent, &child_val.rw_pb);

}

rw_tree_walker_status_t
build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                     RwXMLTreeIterator* child_iter)
{
  ProtobufCMessage *parent;
  rw_status_t rs = parent_iter->get_value (parent);  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (nullptr == parent) {
    return RW_TREE_WALKER_FAILURE;
  }

  ProtobufCFieldInfo child;
  memset (&child, 0, sizeof (child));
  
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  
  rw_tree_walker_status_t rt = add_pbcm_child (parent, child_val.xml,
                                               &child);

  if (rt != RW_TREE_WALKER_SUCCESS)  {
    // no need to move to a child
    return rt;
  }
  
  // Move to the last child of this type for now? this wont work when
  // things are sorted properly for leaf lists, and lists?
  
  rs = parent_iter->move_to_child (child.fdesc,RW_YLIB_LIST_POSITION_LAST);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  return rt;
}

rw_tree_walker_status_t
build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                     RwSchemaTreeIterator* child_iter)
{
  ProtobufCMessage *parent;
  rw_status_t rs = parent_iter->get_value (parent);  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (nullptr == parent) {
    return RW_TREE_WALKER_FAILURE;
  }

  ProtobufCFieldInfo child;
  memset (&child, 0, sizeof (child));
  
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  const ProtobufCMessageDescriptor *desc = parent->descriptor;
  RW_ASSERT(nullptr != desc);

  bool ok = false;
  
  // Not sure if there is a need to make this a more generic function.
  switch (child_val.type) {
    case RW_YLIB_DATA_TYPE_KS_PATH:
      child.fdesc = protobuf_c_message_descriptor_get_field (desc, child_val.path.tag);
      if (nullptr == child.fdesc) {
        return RW_TREE_WALKER_FAILURE;
      }
      ok = protobuf_c_message_set_field_message (nullptr, parent, child.fdesc, (ProtobufCMessage **) &child.element);
      break;
    case RW_YLIB_DATA_TYPE_KS_KEY:
      child.fdesc = protobuf_c_message_descriptor_get_field (desc, child_val.rw_pb.fdesc->id);
      if (nullptr == child.fdesc) {
        return RW_TREE_WALKER_FAILURE;
      }
      child.element = child_val.rw_pb.element;
      
      ok = protobuf_c_message_set_field (nullptr, parent, &child);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  if (!ok) {
    return RW_TREE_WALKER_FAILURE;
  }

  // Move to the last child of this type for now? this wont work when
  // things are sorted properly for leaf lists, and lists?
  
  rs = parent_iter->move_to_child (child.fdesc,RW_YLIB_LIST_POSITION_LAST);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  return RW_TREE_WALKER_SUCCESS;
  
}

// Copiers for XML iterator

rw_tree_walker_status_t
build_and_move_iterator_child_value (RwXMLTreeIterator* parent_iter,
                                     RwPbcmTreeIterator* child_iter)
{
  XMLNode *parent;
  rw_status_t rs = parent_iter->get_value (parent);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  XMLNode *child = nullptr;
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  rw_tree_walker_status_t rt = add_xml_child (parent, &child_val.rw_pb,
                                              child_iter->get_yang_node_name(),
                                              child_iter->get_yang_ns(),
                                              child);

  if (RW_TREE_WALKER_SUCCESS == rt) {
    rs = parent_iter->move_to_child (child);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
  }

  return rt;
    
}

rw_tree_walker_status_t build_and_move_iterator_child_value (RwXMLTreeIterator* parent_iter,
                                                             RwXMLTreeIterator* child_iter)
{
  XMLNode *parent;
  rw_status_t rs = parent_iter->get_value (parent);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  XMLNode *child = nullptr;
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  RW_ASSERT(RW_YLIB_DATA_TYPE_XML_NODE ==  child_val.type);

  rw_tree_walker_status_t rt = add_xml_child (parent, child_val.xml, child);
  
  if (rt != RW_TREE_WALKER_SUCCESS)  {
    // no need to move to a child
    return rt;
  }

  RW_ASSERT(nullptr != child);
  parent_iter->move_to_child (child);

  return rt;

}

// Copiers for Schema Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwSchemaTreeIterator* parent_iter,
                                                             RwSchemaTreeIterator* child_iter)
{
  RW_ASSERT_NOT_REACHED();
}


rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                           RwPbcmTreeIterator* child_iter)
{
  ProtobufCMessage *parent = nullptr;
  
  rw_ylib_data_t child_val;
  rw_status_t rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(RW_YLIB_DATA_TYPE_PB == child_val.type);
  
  rs = parent_iter->get_value (parent);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  
  bool ok = protobuf_c_message_set_field (nullptr, parent, &child_val.rw_pb);

  if (!ok) {
    return RW_TREE_WALKER_FAILURE;
  }

  rs = parent_iter->move_to_child (child_val.rw_pb.fdesc);
  if (RW_STATUS_SUCCESS == rs) {
    return RW_TREE_WALKER_SUCCESS;
  }
  
  return RW_TREE_WALKER_FAILURE;  
}

rw_tree_walker_status_t
set_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                     RwSchemaTreeIterator* child_iter)
{
  ProtobufCMessage *parent;
  rw_status_t rs = parent_iter->get_value (parent);  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  if (nullptr == parent) {
    return RW_TREE_WALKER_FAILURE;
  }

  ProtobufCFieldInfo child;
  memset (&child, 0, sizeof (child));
  
  rw_ylib_data_t child_val;
  rs = child_iter->get_value(&child_val);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  const ProtobufCMessageDescriptor *desc = parent->descriptor;
  RW_ASSERT(nullptr != desc);

  bool ok = false;
  
  // Not sure if there is a need to make this a more generic function.
  switch (child_val.type) {
    case RW_YLIB_DATA_TYPE_KS_KEY:
      child.fdesc = protobuf_c_message_descriptor_get_field (desc, child_val.rw_pb.fdesc->id);
      if (nullptr == child.fdesc) {
        return RW_TREE_WALKER_FAILURE;
      }
      child.element = child_val.rw_pb.element;
      
      ok = protobuf_c_message_set_field (nullptr, parent, &child);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  if (!ok) {
    return RW_TREE_WALKER_FAILURE;
  }

  // Move to the last child of this type for now? this wont work when
  // things are sorted properly for leaf lists, and lists?
  
  rs = parent_iter->move_to_child (child.fdesc,RW_YLIB_LIST_POSITION_LAST);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  return RW_TREE_WALKER_SUCCESS;
  
}

rw_tree_walker_status_t find_child (RwPbcmTreeIterator* haystack_iter,
                                    RwPbcmTreeIterator* needle,
                                    RwPbcmTreeIterator* found)
{
  ProtobufCMessage *hs;
  rw_ylib_data_t   n_val;

  rw_status_t rs = needle->get_value (&n_val);
  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(RW_YLIB_DATA_TYPE_PB == n_val.type);
  const ProtobufCFieldDescriptor *fdesc = n_val.rw_pb.fdesc;
  RW_ASSERT(fdesc->type == PROTOBUF_C_TYPE_MESSAGE);
  
  rs = haystack_iter->get_value (hs);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  
  // If not listy - a submessage find should be sufficient
  RwPbcmTreeIterator candidate = *haystack_iter;
  
  rs = candidate.move_to_child(fdesc,RW_YLIB_LIST_POSITION_FIRST );

  if (RW_STATUS_SUCCESS != rs)   {
    return RW_TREE_WALKER_FAILURE;
  }

  if (PROTOBUF_C_LABEL_REPEATED != fdesc->label) {
    *found = candidate;
    return RW_TREE_WALKER_SUCCESS;
  }

  ProtobufCMessage *key_val = (ProtobufCMessage *)n_val.rw_pb.element;
  
  // Listy - check all the keys of all the list entries
  while (rs == RW_STATUS_SUCCESS) {

    ProtobufCMessage *candidate_val = nullptr;
    rs = candidate.get_value(candidate_val);
    if (rs != RW_STATUS_SUCCESS) {
      return RW_TREE_WALKER_FAILURE;
    }

    bool match = protobuf_c_message_compare_keys(candidate_val, key_val, fdesc->msg_desc);

    if (match) {
      *found = candidate;
      return RW_TREE_WALKER_SUCCESS;
    }

    rs = candidate.move_to_next_list_entry();
  }

  return RW_TREE_WALKER_FAILURE;
}

rw_tree_walker_status_t find_child (RwPbcmTreeIterator* haystack_iter,
                                    RwSchemaTreeIterator* needle,
                                    RwPbcmTreeIterator* found)
{
  ProtobufCMessage *hs;
  rw_ylib_data_t   n_val;

  rw_status_t rs = needle->get_value (&n_val);
  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(RW_YLIB_DATA_TYPE_KS_PATH == n_val.type);
  
  rs = haystack_iter->get_value (hs);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  
  const ProtobufCFieldDescriptor *fdesc =
      protobuf_c_message_descriptor_get_field(hs->descriptor, n_val.path.tag);

  if (nullptr == fdesc) {
    return RW_TREE_WALKER_FAILURE;
  }
  
  RW_ASSERT(fdesc->type == PROTOBUF_C_TYPE_MESSAGE);
 
  RwPbcmTreeIterator candidate = *haystack_iter; 
  rs = candidate.move_to_child(fdesc,RW_YLIB_LIST_POSITION_FIRST );

  if (RW_STATUS_SUCCESS != rs)   {
    return RW_TREE_WALKER_FAILURE;
  }

  if (PROTOBUF_C_LABEL_REPEATED != fdesc->label) {
    // If not listy - a submessage find should be sufficient
    *found = candidate;
    return RW_TREE_WALKER_SUCCESS;
  }


  for (;RW_STATUS_SUCCESS == rs; rs = candidate.move_to_next_list_entry()) {
    
    RwSchemaTreeIterator keys = *needle;    
    rw_ylib_data_t  candidate_value;
    bool match = true;

    rs = candidate.get_value (&candidate_value);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
    
    for (rw_status_t key_rs = keys.move_to_first_child();
         RW_STATUS_SUCCESS == key_rs;
         key_rs = keys.move_to_next_sibling()) {
      
      rw_status_t temp_rs = keys.get_value (&n_val);    
      RW_ASSERT(RW_STATUS_SUCCESS == temp_rs);
      
      if (RW_YLIB_DATA_TYPE_KS_PATH == n_val.type) {
        continue;
      }
      
      RW_ASSERT(RW_YLIB_DATA_TYPE_KS_KEY == n_val.type);
      if (!protobuf_c_message_has_field_with_value ((const ProtobufCMessage *)candidate_value.rw_pb.element,
                                                    n_val.rw_pb.fdesc->name, &n_val.rw_pb)) {
        match = false;
        break;
      }
    }
    if (match) {
      // ATTN: THIS WILL FIND ONLY THE FIRST MATCH.
      // The iterated element should be the key to collect multiple nodes
      *found = candidate;
      return RW_TREE_WALKER_SUCCESS;
    }
  }
  return RW_TREE_WALKER_FAILURE;
}


rw_tree_walker_status_t find_child (RwSchemaTreeIterator* key_iter,
                                    RwPbcmTreeIterator* data_iter,
                                    RwSchemaTreeIterator* next_key)
{
  rw_ylib_data_t key, data;
  
  rw_status_t rs = data_iter->get_value (&data);
  
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  RW_ASSERT(RW_YLIB_DATA_TYPE_PB == data.type);
  
  const ProtobufCFieldDescriptor *fdesc = data.rw_pb.fdesc;
  RW_ASSERT(fdesc->type == PROTOBUF_C_TYPE_MESSAGE);

  // The intend is to find a child that matches. Get the first
  // child of the key, and check whether it matches with the data being inspected.
  *next_key= *key_iter;
  rs = next_key->move_to_first_child();

  // The case for calling this function at the leaf of the key implies that this
  // node has already been matched.

  if (RW_STATUS_FAILURE == rs) {
    return RW_TREE_WALKER_SUCCESS;
  }
  
  // The move to first child will go to a path if it exists. If this is a leaf,
  // the calling of this function does not make sense for the current use cases
  RW_ASSERT(!next_key->is_leafy());

  rs = next_key->get_value(&key);  
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  if (key.type != RW_YLIB_DATA_TYPE_KS_PATH) {
    RW_CRASH();
  }

  // The tag of the key has to match the tag of the sub message. Getting to the
  // messages pb tag is throught the yang data structure.
  const ProtobufCMessageDescriptor *desc = data.rw_pb.fdesc->msg_desc;
  RW_ASSERT(nullptr != desc);

  if (!desc->ypbc_mdesc ||
      (desc->ypbc_mdesc->pb_element_tag != key.path.tag)) {
    return RW_TREE_WALKER_FAILURE;
  }

  // If not listy - a submessage find should be sufficient
  if (PROTOBUF_C_LABEL_REPEATED != fdesc->label) {
    return RW_TREE_WALKER_SUCCESS;
  }

  // Listy - check whether all the keys match. Do not iterate the list though -
  // the walker will do that for us.
  RwSchemaTreeIterator key_elt = *next_key;
  rs = key_elt.move_to_first_child();

  for (; RW_STATUS_SUCCESS == rs; rs = key_elt.move_to_next_sibling()) {
    rs = key_elt.get_value(&key);
    
    if (RW_STATUS_FAILURE == rs) {
      return RW_TREE_WALKER_FAILURE;
    }

    if (RW_YLIB_DATA_TYPE_KS_PATH == key.type) {
      continue;
    }

    RW_ASSERT(RW_YLIB_DATA_TYPE_KS_KEY == key.type);
    
    if (protobuf_c_message_has_field_with_value
        ((ProtobufCMessage *) data.rw_pb.element, key.rw_pb.fdesc->name, &key.rw_pb)) {
      continue;
    }
    else {
      return RW_TREE_WALKER_FAILURE;
    }
  }
  
  return RW_TREE_WALKER_SUCCESS;
}

}

// Private functions used by other functions
static
rw_tree_walker_status_t add_pbcm_child (ProtobufCMessage *parent,
                                        ProtobufCFieldInfo *child)
{
  if (nullptr == child->fdesc) {
    ProtobufCMessage *c_msg = (ProtobufCMessage *)child->element;
    if ((nullptr == c_msg->descriptor->ypbc_mdesc) ||
        (nullptr == c_msg->descriptor->ypbc_mdesc->module)){
      return RW_TREE_WALKER_FAILURE;
    }

    const rw_yang_pb_msgdesc_t *y_desc = c_msg->descriptor->ypbc_mdesc;
      
    child->fdesc = protobuf_c_message_descriptor_get_field_by_yang(
        parent->descriptor,
        y_desc->yang_node_name,
        y_desc->module->ns);

    if (!child->fdesc) {
      return RW_TREE_WALKER_FAILURE;
    }                                                            
  }
  bool ok = protobuf_c_message_set_field (nullptr, parent, child);
  if (!ok) {
    return RW_TREE_WALKER_FAILURE;
        
  }
  return RW_TREE_WALKER_SKIP_SUBTREE;
}

static
rw_tree_walker_status_t add_pbcm_child (ProtobufCMessage *parent,
                                        XMLNode *node,
                                        ProtobufCFieldInfo *child)
{

  YangNode *yn = node->get_yang_node();

  if (yn == nullptr) {
    return RW_TREE_WALKER_FAILURE;
  }

  child->fdesc = protobuf_c_message_descriptor_get_field_by_yang(
      parent->descriptor,
      yn->get_name(),
      yn->get_ns());
  
  if (!child->fdesc) {
    return RW_TREE_WALKER_FAILURE;
  }                                                            

  if (!yn->is_leafy()) {
    bool ok = protobuf_c_message_set_field_message (
        nullptr, parent, child->fdesc, (ProtobufCMessage **) &child->element);
    if (ok) {
      return RW_TREE_WALKER_SUCCESS;
    } else {
      return RW_TREE_WALKER_FAILURE;
    }                                                            
  }
  // Leafy, non-empty elements need to have a string value
  std::string value = node->get_text_value();

  YangType* ytype = yn->get_type();
  RW_ASSERT(ytype);

  rw_yang_leaf_type_t leaf_type = ytype->get_leaf_type();


  if (leaf_type == RW_YANG_LEAF_TYPE_ENUM) {
    value = protobuf_c_message_get_pb_enum (child->fdesc, value.c_str());
  }

  if ((leaf_type != RW_YANG_LEAF_TYPE_EMPTY) && !value.length()) {
    return RW_TREE_WALKER_FAILURE;
  }

  bool ok = protobuf_c_message_set_field_text_value (
      nullptr, parent, child->fdesc, value.c_str(), value.length());
  if (ok) {
    return RW_TREE_WALKER_SUCCESS;
  }
  
  return RW_TREE_WALKER_SKIP_SUBTREE;
}


RwSchemaTreeIterator::RwSchemaTreeIterator(rw_keyspec_path_t *key)
    :key_ (key)
{
  if (key_) {
    length_ = rw_keyspec_path_get_depth (key_);
    RW_ASSERT(length_);
  }
}

rw_status_t
RwSchemaTreeIterator::move_to_child(const ProtobufCFieldDescriptor *fdesc)
{
  RW_ASSERT(path_);
  
  const ProtobufCMessageDescriptor *desc =
      ((ProtobufCMessage *) path_)->descriptor;
  
  // Set the index based on tag. While it would be good to remember just the tag,
  // iterating to the next sibling based on that is iffy

  for (size_t i = 0; i < desc->n_fields; i++) {
    if (fdesc == &desc->fields[i]) {
      index_ = i;
      return RW_STATUS_SUCCESS;
    }
  }
  return RW_STATUS_FAILURE;
}

rw_status_t RwSchemaTreeIterator::move_to_first_child()
{
 
  if (index_ || // at a leaf, cannot go down
      depth_ > length_) { // too deep
    return RW_STATUS_FAILURE;
  }
  
  if (depth_ == length_)
  {
    size_t index = index_;
    rw_status_t rs = rw_keyspec_entry_get_key_at_or_after (path_, &index);
    if (RW_STATUS_SUCCESS != rs) {
      return RW_STATUS_FAILURE;
    }
    
    index_ = index + 1;
    return RW_STATUS_SUCCESS;
  }
  
  index_ = 0;
  
  // ATTN: Tom - should the signature of the base function e changed?
  path_ = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry (key_, depth_);
  depth_ ++;
  
  RW_ASSERT(depth_ <= length_);
  return RW_STATUS_SUCCESS;
}

rw_status_t RwSchemaTreeIterator::move_to_next_sibling()
{
  size_t index = 0;
  
  if (!index_) {
    // We are at pathXX, and not at keyXX. go up, and find the first present key
    
    if (depth_ < 2) { // The root node and PATH00 has no siblings
      return RW_STATUS_FAILURE;
    }
    
    const rw_keyspec_entry_t *parent_path =
        rw_keyspec_path_get_path_entry (key_, depth_ - 2);
    
    rw_status_t rs = rw_keyspec_entry_get_key_at_or_after (parent_path, &index);
    if (RW_STATUS_SUCCESS == rs) {
      path_ = (rw_keyspec_entry_t *)parent_path;
      depth_ -= 1;
      index_ = index + 1;
    }

    return rs;
    
  }
  // If this fails, the index is not updated
  index = index_ + 1;
  if (RW_STATUS_SUCCESS == rw_keyspec_entry_get_key_at_or_after (path_, &index)) {
    index_ = index;
  }

  return RW_STATUS_FAILURE;
}

rw_status_t RwSchemaTreeIterator::move_to_parent()
{
  if (!depth_) {
    return RW_STATUS_FAILURE;
  }

  if (index_) {    
    index_ = 0;
    return RW_STATUS_SUCCESS;
  }
  
  depth_--;
  
  if (depth_) {
    path_ =(rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry (key_, depth_-1);
  }
  
  return RW_STATUS_SUCCESS;
}


rw_status_t
RwSchemaTreeIterator::get_value(rw_ylib_data_t* data) const
{
  //ATTN: index_ is post incremented after a call to rw_path_spec_has_key.
  if (!path_) {
    return RW_STATUS_FAILURE;
  }
  
  if (!index_) {
    data->type = RW_YLIB_DATA_TYPE_KS_PATH;
    data->path = rw_keyspec_entry_get_schema_id (path_);
    return RW_STATUS_SUCCESS;
  }

  
  data->type = RW_YLIB_DATA_TYPE_KS_KEY;
  memset (&data->rw_pb, 0, sizeof (data->rw_pb));
  rw_status_t rs = rw_keyspec_entry_get_key_value (path_, index_ - 1, &data->rw_pb);
  return rs;
}

rw_status_t
RwSchemaTreeIterator::get_value(rw_keyspec_entry_t*& path) const
{
  // For root - return null
  // For 
  path = path_;
  return RW_STATUS_SUCCESS;
}

const rw_yang_pb_msgdesc_t *
RwSchemaTreeIterator::get_y2pb_mdesc() const
{

  if (path_) {
    const ProtobufCMessageDescriptor *desc =
        ((ProtobufCMessage *)path_)->descriptor;
    return desc->ypbc_mdesc;
  }
  
  const rw_yang_pb_msgdesc_t *y2pb =
      ((ProtobufCMessage *) key_)->descriptor->ypbc_mdesc;

  while (y2pb->parent) {
    y2pb = y2pb->parent;
  }

  return y2pb;
}

const char *
RwSchemaTreeIterator::get_yang_ns() const
{
  const rw_yang_pb_msgdesc_t *y2pb = get_y2pb_mdesc();
  if (nullptr != y2pb) {
    return y2pb->module->ns;
  }

  return nullptr;
}

const char *
RwSchemaTreeIterator::get_yang_node_name() const
{
  const rw_yang_pb_msgdesc_t *y2pb = get_y2pb_mdesc();
  if (nullptr != y2pb) {
    return y2pb->yang_node_name;
  }

  return nullptr;

}


rw_status_t 
RwKsPbcmIterator::move_to_first_child()
{
  switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
      if (nullptr != keyspec_) {
        state_ = RW_PB_KS_ITER_IN_KS;
      } else {
      state_ = RW_PB_KS_ITER_IN_PB;
      }
      return RW_STATUS_SUCCESS;

    case RW_PB_KS_ITER_IN_KS:
      if ((nullptr != keyspec_) && (ks_iter_.depth_ < ks_iter_.length_)) {
        return ks_iter_.move_to_first_child();
      }
      // intentional fall through
    case RW_PB_KS_ITER_IN_PB:
      if (nullptr != msg_) {
        state_ = RW_PB_KS_ITER_IN_PB;
        return pbcm_iter_.move_to_first_child();
      }
        return RW_STATUS_FAILURE;        
    default:
        RW_ASSERT_NOT_REACHED();
  }
  RW_ASSERT_NOT_REACHED();
}

rw_status_t 
RwKsPbcmIterator::move_to_next_sibling()
{
  switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
      return RW_STATUS_FAILURE;
    case RW_PB_KS_ITER_IN_KS:
      return ks_iter_.move_to_next_sibling();
    case RW_PB_KS_ITER_IN_PB:
      if (pbcm_iter_.node_state_.size()) {
        return pbcm_iter_.move_to_next_sibling();
      }
      RW_ASSERT(ks_iter_.length_ == ks_iter_.depth_);
      state_ = RW_PB_KS_ITER_IN_KS;
      return ks_iter_.move_to_next_sibling();
    default:
      RW_ASSERT_NOT_REACHED();
  }
  RW_ASSERT_NOT_REACHED();
}

rw_status_t 
RwKsPbcmIterator::move_to_parent()
{
  switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
      return RW_STATUS_FAILURE;
    case RW_PB_KS_ITER_IN_PB:
      if (pbcm_iter_.node_state_.size())  {
        return pbcm_iter_.move_to_parent();
      }
      state_ = RW_PB_KS_ITER_IN_KS;
      //intentional fallthrough
    case RW_PB_KS_ITER_IN_KS:
      if (0 == ks_iter_.depth_) {
        state_ = RW_PB_KS_ITER_AT_ROOT;
        return RW_STATUS_SUCCESS;
      }
      return ks_iter_.move_to_parent();
      
    default:
      RW_ASSERT_NOT_REACHED();
  }
  RW_ASSERT_NOT_REACHED();
}

const char *
RwKsPbcmIterator::get_module_name() const
{

  ProtobufCMessage *msg = nullptr;
  switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
    case RW_PB_KS_ITER_IN_KS:
      RW_ASSERT(nullptr != keyspec_);
      msg = (ProtobufCMessage *) keyspec_;
      break;
    case RW_PB_KS_ITER_IN_PB: {      
      rw_status_t rs = pbcm_iter_.get_value(msg);
      RW_ASSERT(RW_STATUS_SUCCESS == rs);
    }
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  RW_ASSERT(nullptr != msg);
  RW_ASSERT(nullptr != msg->descriptor);
  if (nullptr == msg->descriptor->ypbc_mdesc) {
    // can happen for generic keyspec or protos not from yang?
    return nullptr;
  }
  RW_ASSERT(msg->descriptor->ypbc_mdesc->module);
  return (msg->descriptor->ypbc_mdesc->module->module_name);
}


bool
RwKsPbcmIterator::is_leafy() const
{
  switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
      return false;
    case RW_PB_KS_ITER_IN_PB:
      return pbcm_iter_.is_leafy();
    case RW_PB_KS_ITER_IN_KS:
      return ks_iter_.is_leafy();
    default:
      RW_ASSERT_NOT_REACHED();
  }
  RW_ASSERT_NOT_REACHED();
}

rw_status_t
RwKsPbcmIterator::get_value(rw_ylib_data_t *val) const
{
    switch (state_) {
    case RW_PB_KS_ITER_AT_ROOT:
      return RW_STATUS_FAILURE;
    case RW_PB_KS_ITER_IN_PB:
      return pbcm_iter_.get_value(val);
    case RW_PB_KS_ITER_IN_KS:
      return ks_iter_.get_value(val);
    default:
      RW_ASSERT_NOT_REACHED();
  }
  RW_ASSERT_NOT_REACHED();
}


rw_status_t
    RwPbcmDomIterator::move_to_first_child()
 {
   if (dom_->trees_.empty()) {
     return RW_STATUS_FAILURE;
   }

   if (nullptr == module_) {
     auto it = dom_->trees_.begin();
     module_ = it->first.c_str();
     RW_ASSERT(module_);
     RW_ASSERT(it->second);
     pbcm_iter_ = RwPbcmTreeIterator(it->second);
     return RW_STATUS_SUCCESS;
   }

   return pbcm_iter_.move_to_first_child();
 }

rw_status_t
RwPbcmDomIterator::move_to_child (RwPbcmDomIterator *iter)
{
  //ATTN: add some validation here.
  module_ = iter->module_;
  pbcm_iter_ = iter->pbcm_iter_;

  return RW_STATUS_SUCCESS;
}

rw_status_t
RwPbcmDomIterator::move_to_child (const char *name,
                                  rw_pbcm_dom_get_e type)
{
  // Either in a module, or no modules
  if ((nullptr != module_) || (nullptr == dom_)) {
    return RW_STATUS_FAILURE;    
  }

  ProtobufCMessage *msg = nullptr;
  rw_status_t rs = dom_->get_module_tree (name, type, msg);

  if (rs != RW_STATUS_SUCCESS) {
    return rs;
  }

  module_ = name;
  pbcm_iter_ = RwPbcmTreeIterator (msg);
  return rs;
}



  
 rw_status_t
 RwPbcmDomIterator::move_to_next_sibling()
 {
   
   if (dom_->trees_.empty()) {
     return RW_STATUS_FAILURE;
   }

   if (nullptr == module_) {
     // The schema root has no siblings
     return RW_STATUS_FAILURE;
   }
   
   if (pbcm_iter_.node_state_.size()) {
     // Iterator is inside a message - return from that message
     return pbcm_iter_.move_to_next_sibling();
   }

   auto it = dom_->trees_.find(module_);
   RW_ASSERT(dom_->trees_.end() != it);

   it ++;
   if (dom_->trees_.end() == it) {
     return RW_STATUS_FAILURE;
   }

   module_ = it->first.c_str();
   RW_ASSERT(module_);
   RW_ASSERT(it->second);
   pbcm_iter_ = RwPbcmTreeIterator(it->second);
   return RW_STATUS_SUCCESS;
 }

 rw_status_t
 RwPbcmDomIterator::move_to_parent()
 {
      if (dom_->trees_.empty()) {
     return RW_STATUS_FAILURE;
   }

   if (nullptr == module_) {
     // The schema root has no parent
     return RW_STATUS_FAILURE;
   }
   
   if (pbcm_iter_.node_state_.size()) {
     // Iterator is inside a message - return from that message
     return pbcm_iter_.move_to_parent();
   }

   // The message iterator is at its root - move to the schema root.
   module_ = nullptr;
   return RW_STATUS_SUCCESS;
 }

rw_status_t
RwPbcmDom::get_module_tree (const char *name,
                            rw_pbcm_dom_get_e type,
                            ProtobufCMessage*& tree)
{
  auto it = trees_.find (name);
  tree = nullptr;
  
  switch (type) {
    case RW_PBCM_DOM_FIND:
      if (trees_.end() != it ) {
        tree = it->second;
        return RW_STATUS_SUCCESS;
      }
      return RW_STATUS_FAILURE;

    case RW_PBCM_DOM_FIND_OR_CREATE:
      if (trees_.end() != it ) {
        tree = it->second;
        return RW_STATUS_SUCCESS;
      }
      break;
    case RW_PBCM_DOM_CREATE:
      if (trees_.end() != it ) {
        return RW_STATUS_FAILURE;
      }
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  // Find the module for this message from the schema
  const rw_yang_pb_module_t  *module = nullptr;
  bool found_match = false;
  
  for (size_t i = 0; i < schema_->module_count; i++) {
    module = schema_->modules[i];
    RW_ASSERT(nullptr != module);
    if (0 == strcmp (module->module_name, name)) {
      found_match = true;
      break;
    }
  }

  if (!found_match) {
    return RW_STATUS_FAILURE;
  }
  
  // Create the default message for the module
  RW_ASSERT(nullptr != module);
  
  const rw_yang_pb_msgdesc_t *ypb_msg = nullptr;
  switch (type_) {
    case RW_PBCM_DOM_TYPE_DATA:
      ypb_msg = module->data_module;
      break;
    case RW_PBCM_DOM_TYPE_RPC_IN:
      ypb_msg = module->rpci_module;
      break;
    case RW_PBCM_DOM_TYPE_RPC_OUT:
      ypb_msg = module->rpco_module;
      break;
    case RW_PBCM_DOM_TYPE_NOTIFICATION:
      ypb_msg = module->notif_module;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  if (nullptr == ypb_msg) {
    return RW_STATUS_FAILURE;
  }
  
  tree = protobuf_c_message_create (nullptr, ypb_msg->pbc_mdesc);
  if (nullptr == tree) {
    return RW_STATUS_FAILURE;
  }
  // Add it to the map
  trees_[module->module_name] = tree;
  return RW_STATUS_SUCCESS;
}

namespace rw_yang {

rw_tree_walker_status_t find_child (RwPbcmDomIterator *haystack_iter,
                                    RwKsPbcmIterator *needle,
                                    RwPbcmDomIterator *found)
{

  rw_status_t rs = RW_STATUS_FAILURE;
  
  if (nullptr == haystack_iter->dom_) {
    return RW_TREE_WALKER_FAILURE;
  }
  // The found iterator will be a child of haystack_iter, so initialize to haystack_iter
  // and move appropriately.
  *found = *haystack_iter;
  // If the iterator is at the root of the schema, find the right module from
  // the map
  if (nullptr == found->module_) {
    rs = found->move_to_child(needle->get_module_name());
    
    if (RW_STATUS_SUCCESS == rs) {
      return RW_TREE_WALKER_SUCCESS;
    }

    return RW_TREE_WALKER_FAILURE;
  }

  //The PBCM iterator has to be iterated with the other iterator.
  //
  // ATTN: This code is too intimate with the internals of both iterators
  rw_tree_walker_status_t rt = RW_TREE_WALKER_FAILURE;
  RwPbcmTreeIterator iter;
  
  switch (needle->state_) {
    case RwKsPbcmIterator::RW_PB_KS_ITER_AT_ROOT:
      // This should nt be the case.
      return RW_TREE_WALKER_FAILURE;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_KS:
      rt = find_child ( &found->pbcm_iter_, &needle->ks_iter_, &iter);
      break;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_PB:
      rt = find_child ( &found->pbcm_iter_, &needle->pbcm_iter_, &iter);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (RW_TREE_WALKER_SUCCESS != rt) {
    return rt;
  }
  rs = found->pbcm_iter_.move_to_child (&iter);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  return rt;
}
rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                           RwKsPbcmIterator* child_iter)
{    
  rw_tree_walker_status_t rt = RW_TREE_WALKER_FAILURE;
  
  switch (child_iter->state_) {
    case  RwKsPbcmIterator::RW_PB_KS_ITER_AT_ROOT:
      // This should nt be the case.
      return RW_TREE_WALKER_FAILURE;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_KS:
      rt = set_and_move_iterator_child_value ( &parent_iter->pbcm_iter_, &child_iter->ks_iter_);
      break;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_PB:
      rt = set_and_move_iterator_child_value( &parent_iter->pbcm_iter_, &child_iter->pbcm_iter_);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  
  return rt;
}

rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                             RwKsPbcmIterator* child_iter)
{
  rw_status_t rs = RW_STATUS_FAILURE;
  
  if (nullptr == parent_iter->module_) {
    rs = parent_iter->move_to_child(child_iter->get_module_name(), RW_PBCM_DOM_CREATE);
    if (rs == RW_STATUS_SUCCESS) {
      return RW_TREE_WALKER_SUCCESS;
    }
    return RW_TREE_WALKER_FAILURE;
  }

  rw_tree_walker_status_t rt = RW_TREE_WALKER_FAILURE;
  
  switch (child_iter->state_) {
    case  RwKsPbcmIterator::RW_PB_KS_ITER_AT_ROOT:
      // This should nt be the case.
      return RW_TREE_WALKER_FAILURE;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_KS:
      rt = build_and_move_iterator_child_value ( &parent_iter->pbcm_iter_, &child_iter->ks_iter_);
      break;
    case  RwKsPbcmIterator::RW_PB_KS_ITER_IN_PB:
      rt = build_and_move_iterator_child_value( &parent_iter->pbcm_iter_, &child_iter->pbcm_iter_);
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  
  
  return rt;
}
}  

