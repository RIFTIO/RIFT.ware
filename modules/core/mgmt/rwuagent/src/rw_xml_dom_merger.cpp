/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_xml_dom_merger.cpp
 * @author Balaji Rajappa 
 * @date 2016/03/23
 * @brief Helper to apply config changes to a base DOM 
 */

#include "rw_xml_dom_merger.hpp"

using namespace rw_yang;

static const char* NETCONF_NS = "urn:ietf:params:xml:ns:netconf:base:1.0";

XMLDocMerger::edit_conf_operation_map_t XMLDocMerger::edit_conf_operation_map_ =
{
  // "none" is not a valid operation, only a default_operation
  {"merge", XML_EDIT_OP_MERGE},
  {"replace", XML_EDIT_OP_REPLACE},
  {"create", XML_EDIT_OP_CREATE},
  {"delete", XML_EDIT_OP_DELETE},
  {"remove", XML_EDIT_OP_REMOVE}
};

XMLDocMerger::XMLDocMerger(
                XMLManager* mgr,
                XMLDocument* base_dom,
                const rw_yang_pb_schema_t* ypbc_schema)
  : xml_mgr_(mgr),
    base_dom_(base_dom),
    ypbc_schema_(ypbc_schema)
{
}

XMLDocMerger::~XMLDocMerger()
{
}

XMLDocument::uptr_t XMLDocMerger::copy_and_merge(XMLDocument* delta,
                              XMLEditDefaultOperation default_operation)
{
  current_operation_ = default_operation;
  XMLDocument::uptr_t new_dom(xml_mgr_->create_document());
  XMLDocument::uptr_t update_dom(xml_mgr_->create_document());
  XMLNode* new_root = new_dom->get_root_node();
  XMLNode* base_root = base_dom_->get_root_node();

  if (base_root->has_attribute("serial_number")) {
    std::string value = base_root->get_attribute_value("serial_number");
    new_root->set_attribute("serial_number", value.c_str());
  }

  XMLNode* delta_root = delta->get_root_node();
  XMLNode* update_root = update_dom->get_root_node();

  // Copy from base_dom to new_dom
  for (XMLNodeIter it = base_root->child_begin();
      it != base_root->child_end(); ++it) {
    XMLNode* base_child = &(*it);
    new_root->import_child(base_child, base_child->get_yang_node(), true);
  }

  rw_yang_netconf_op_status_t nc_status;
  bool update_required = false;

  nc_status = do_edit(new_root, delta_root, update_root, &update_required);
  if (nc_status != RW_YANG_NETCONF_OP_STATUS_OK) {
    return nullptr;
  }

  if (update_required && update_root->get_first_child() && on_update_) {
    rw_status_t status = on_update_(update_root);
    if (status != RW_STATUS_SUCCESS) {
      return nullptr;
    }
  }
  
  return new_dom;
}

// ATTN: This function seriously needs re-organization, will be done
// shortly.
rw_yang_netconf_op_status_t XMLDocMerger::do_edit(XMLNode* new_node, 
                XMLNode* delta_node,
                XMLNode* update_node,
                bool* update_required)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;
  YangNode* ynode = new_node->get_descend_yang_node();
  XMLEditDefaultOperation parent_op = current_operation_;

  for(XMLNodeIter it = delta_node->child_begin();
      it != delta_node->child_end(); ++it)
  {
    XMLNode* delta_child = &(*it);
    std::string child_name = delta_child->get_local_name();
    std::string child_ns = delta_child->get_name_space();

    YangNode* child_ynode = ynode->search_child(child_name.c_str(), 
                                                child_ns.c_str());
    if (child_ynode == nullptr) {
      // Incoming node is not in our model and thus is an error
      std::string const err_msg = "Cannot find child ("+child_name+") of node ("+delta_node->get_local_name()+")";
      report_error(delta_node, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, err_msg.c_str());
      return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
    }

    // Set the current node operation (default=merge)
    status = set_current_operation(delta_child);
    if (status !=  RW_YANG_NETCONF_OP_STATUS_OK) {
      return status;
    }

    bool subtree_update = false;

    XMLNode* new_child = new_node->find(child_name.c_str(), child_ns.c_str());
    if (new_child == nullptr) {
      // Node not found in existing config, edit-config on new node
      status = do_edit_new(child_ynode, new_node, delta_child, 
                           update_node, &subtree_update);
    } else {
      status = do_edit_existing(child_ynode, new_node, new_child, delta_child,
                           update_node, &subtree_update);
    }

    *update_required = (*update_required || subtree_update);

    if (status !=  RW_YANG_NETCONF_OP_STATUS_OK) {
      return status;
    }
    current_operation_ = parent_op;
  }

  if (current_operation_ == XML_EDIT_OP_REPLACE) {
    // Iterate thru the config dom node and find the elements not in delta
    // Those are marked for deletion.
    do_delete_missing(new_node, delta_node);
  }

  // Add defaults
  fill_defaults(ynode, new_node, update_node, update_required);

  if (!(*update_required)) {
    // No updates on this subtree. Either it is delete/remove operation or
    // the config is not changed. So remove the update subtree. 
    XMLNode* parent = update_node->get_parent();
    if (parent) {
      parent->remove_child(update_node);
    }
  }

  if (new_node->get_first_child() == nullptr &&
      ynode->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER &&
      !ynode->is_presence()) {
    // No children for a non-presence container, they are only present to
    // maintain hierarchy. Remove it
    XMLNode* parent = new_node->get_parent();
    if (parent) {
      parent->remove_child(new_node);
    }
  }

  return status;
}

rw_yang_netconf_op_status_t XMLDocMerger::do_edit_new(
                YangNode* child_ynode,
                XMLNode* new_node,
                XMLNode* delta_child,
                XMLNode* update_node,
                bool* update_required)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;

  switch(current_operation_) {
    case XML_EDIT_OP_DELETE: {
      // Deleted node should exist, report an error
      std::string const error_msg = "Node to delete ("+new_node->get_local_name()+") doesn't exist.";
      report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_DATA_MISSING, error_msg.c_str());
      return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
    }
    case XML_EDIT_OP_REMOVE: {
      return status;
    }
    default:
      // Fall through for create, merge, replace
      // TODO If the default_operation is none, then the child nodes may contain
      // an operations, hence go down xml tree
      break;
  }

  if (child_ynode->get_case() != nullptr) {
    // TODO check if conflicting case nodes are there in the delta
    // Remove the conflicting case node
    for (XMLNode* xml_node = new_node->get_first_child();
         xml_node; ) {
      YangNode* base_yn = xml_node->get_yang_node();
      if (base_yn == nullptr) {
        xml_node = xml_node->get_next_sibling();
        continue;
      }
      if (child_ynode->is_conflicting_node(base_yn)) {
        XMLNode* next_node = xml_node->get_next_sibling();
        // Conflicting case node found, report the deletion
        report_delete(xml_node);
        new_node->remove_child(xml_node);
        xml_node = next_node;
      } else {
        xml_node = xml_node->get_next_sibling();
      }
    }
  }

  // add it to the new_node
  switch(child_ynode->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_LIST: {
      XMLNode* new_child = new_node->add_child(
                                  delta_child->get_yang_node());
      XMLNode* update_child = update_node->add_child(
                                  delta_child->get_yang_node());
      status = do_edit(new_child, delta_child, update_child, update_required);
      if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
        return status;
      }
      if (child_ynode->is_presence()) {
        *update_required = true;
      }
      break;
    }
    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST: {
      std::string value = delta_child->get_text_value();
      const char* val = nullptr;
      if (child_ynode->get_type()->get_leaf_type() == RW_YANG_LEAF_TYPE_EMPTY) {
        if (!value.empty()) {
          // Emty type cannot have a value
          std::string const error_msg = "Node with empty type ("+delta_child->get_local_name()+") cannot have an associated value.";
          report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        val = nullptr;
      } else {
        // non-empty node, do a parse value check
        if (RW_STATUS_SUCCESS != child_ynode->parse_value(value.c_str())) {
          // Invalid value
          std::string const error_msg = "Node ("
                                        + std::string(child_ynode->get_name())
                                        +") has invalid value ("
                                        + value + ")";
          report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        val = value.c_str();
      }

      XMLNode* new_child = new_node->add_child(delta_child->get_yang_node(), 
                                               val);
      if (new_child == nullptr) {
        std::string const error_msg = "Node ("
                                      + std::string(child_ynode->get_name())
                                      +") has invalid value ("
                                      + val + ")";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
      update_node->add_child(delta_child->get_yang_node(),
                             val);
      *update_required = true;
      break;
    }
    case RW_YANG_STMT_TYPE_ANYXML:
      // ATTN: Support for these is not completed yet, but they must be supported!
      RW_ASSERT_NOT_REACHED();

    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_NA:
    default:
      // merge not supported
      RW_ASSERT_NOT_REACHED();
  }
  return status;
}

rw_yang_netconf_op_status_t XMLDocMerger::do_edit_existing(
                YangNode* child_ynode,
                XMLNode* new_node,
                XMLNode* new_child,
                XMLNode* delta_child,
                XMLNode* update_node,
                bool* update_required)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;
  // ATTN: Is this required?
  new_child->set_yang_node(child_ynode);

  const char* child_name = child_ynode->get_name();
  const char* child_ns   = child_ynode->get_ns();

  switch(current_operation_) {
    case XML_EDIT_OP_DELETE:
    case XML_EDIT_OP_REMOVE: {
      status = do_delete(child_ynode, new_node, new_child, delta_child);
      return status;
    }
    case XML_EDIT_OP_CREATE: {
      XMLNode* found_node = nullptr;
      xml_data_presence_t data_exists = check_if_data_exists(child_ynode,
                                            new_node, delta_child, &found_node);
      if (data_exists != XML_DATA_MISSING) {
        std::string const error_msg =  "Data exists for node ("
                                       + std::string(child_name) + ")";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS;
      }
      // Fall-through, same as merge
      break;
    }
    case XML_EDIT_OP_REPLACE: {
      // Replace the current child with the delta child
      // No special action need here. Based on the current_operation_, 
      // do_edit will perform missing node deletion
      break;
    }
    default:
      // Fall through for merge
      break;
  }

  // operation = merge or create
  switch(child_ynode->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_CONTAINER: {
      XMLNode* update_child = update_node->add_child(
                                 delta_child->get_yang_node());
      status = do_edit(new_child, delta_child, update_child, update_required);
      if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
        return status;
      }
      if (child_ynode->is_presence()) {
        *update_required = true;
      }
      break;
    }
    case RW_YANG_STMT_TYPE_LEAF: {
      std::string value = delta_child->get_text_value();
      if (child_ynode->get_type()->get_leaf_type() == RW_YANG_LEAF_TYPE_EMPTY) {
        if (!value.empty()) {
          std::string const error_msg = "Node with empty type ("
                                        + delta_child->get_local_name()
                                        + ") cannot have an associated value.";
          report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
                       
          return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
        }
        // Already existing empty node. Won't require an update. Just checked
        // if the value is empty. Nothing to do.
        return RW_YANG_NETCONF_OP_STATUS_OK;
      }
      if (RW_STATUS_SUCCESS != child_ynode->parse_value(value.c_str())) {
        // Invalid value
        std::string const error_msg = "Node ("
                                      + std::string(child_ynode->get_name())
                                      + ") has invalid value ("
                                      + value + ")";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
      std::string original_val = new_child->get_text_value();
      if (original_val == value) {
        // Value not changed. Don't have to report an update
        if (child_ynode->is_key()) {
          // For a key node, add it to the updates, but don't mark
          // update_required. It will be marked when other leafs are modified
          update_node->add_child(delta_child->get_yang_node(),
                             value.c_str());
        }
        break;
      }
      if (RW_STATUS_SUCCESS != new_child->set_text_value(value.c_str())) {
        std::string const error_msg = "Node ("
                                      + std::string(child_ynode->get_name())
                                      + ") has invalid value ("
                                      + value + ")";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());

        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
      update_node->add_child(delta_child->get_yang_node(),
                             value.c_str());
      *update_required = true;
      break;
    }
    case RW_YANG_STMT_TYPE_LEAF_LIST: {
      std::string value = delta_child->get_text_value().c_str();
      if (RW_STATUS_SUCCESS != child_ynode->parse_value(value.c_str())) {
        std::string const error_msg =  "Node ("
                                       + std::string(child_ynode->get_name())
                                       + ") has invalid value ("
                                       + value + ")";
            report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }

      XMLNode* duplicate_node = new_node->find_value(child_name,
                                           value.c_str(),
                                           child_ns);
      if (duplicate_node) {
        // Same value already exists. Ignore, as per yang rules.
        break;
      }

      XMLNode* node = new_node->add_child(child_ynode, value.c_str());
      if (!node) {
        std::string const error_msg = "Node ("
                                      + std::string(child_ynode->get_name())
                                      + ") has invalid value ("
                                      + value + ")";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE;
      }
      update_node->add_child(child_ynode, value.c_str());
      *update_required = true;
      break;
    }
    case RW_YANG_STMT_TYPE_LIST: {
      // Find the duplicate list node, if any
      XMLNode* matching_list_node = nullptr;
      rw_yang_netconf_op_status_t status = new_node->find_list_by_key (
                  child_ynode, delta_child, &matching_list_node);

      if (RW_YANG_NETCONF_OP_STATUS_OK == status) {
        new_child = matching_list_node;
      } else {
        // No matching list node, new list node
        new_child = new_node->add_child(
                    delta_child->get_yang_node());
      }

      XMLNode* update_child = update_node->add_child(
                    delta_child->get_yang_node());
      status = do_edit(new_child, delta_child, update_child, update_required);
      if (RW_YANG_NETCONF_OP_STATUS_OK != status) {
        return status;
      }
      break;        
    }
    case RW_YANG_STMT_TYPE_ANYXML:
      // ATTN: Support for these is not completed yet, but they must be supported!
      RW_ASSERT_NOT_REACHED();

    case RW_YANG_STMT_TYPE_ROOT:
      // A child cannot be ROOT
      RW_ASSERT_NOT_REACHED();
    case RW_YANG_STMT_TYPE_RPC:
      // RPC node merge should always be new. 
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_NA:
    default:
      // merge not supported
      RW_ASSERT_NOT_REACHED();
  }
  return status;
}

rw_yang_netconf_op_status_t XMLDocMerger::do_delete(
                YangNode* child_ynode,
                XMLNode* new_node,
                XMLNode* new_child,
                XMLNode* delta_child)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;
  
  // The deletion of last case node should add default-case default-values
  // This is taken care by fill_defaults()

  XMLNode* remove_node = new_child;
  xml_data_presence_t data_presence = check_if_data_exists(child_ynode,
                                              new_node, delta_child, &remove_node);
  switch(data_presence) {
    case XML_DATA_MISSING: {
      if (current_operation_ == XML_EDIT_OP_DELETE) {
        std::string const error_msg = "Node ("
                                      + new_node->get_local_name()
                                      + ") has missing data";
        report_error(delta_child, RW_YANG_NETCONF_OP_STATUS_DATA_MISSING, error_msg.c_str());
        return RW_YANG_NETCONF_OP_STATUS_DATA_MISSING;
      }
      return status;
    }
    case XML_DATA_EXISTS: {
      // If a leaf node has default value and is deleted, then don't report it,
      // just delete the node, the default value will added when fill_defaults
      // is invoked.
      if (child_ynode->get_default_value() == nullptr) {
        // Neither a leaf nor a leaf with default value
        report_delete(remove_node);
      } else {
        // has default value, if it is part of a case and is the last node, and
        // is not part of the default case then report deletion
        YangNode* ycase = child_ynode->get_case();
        YangNode* ychoice = child_ynode->get_choice();
        if (ychoice && ycase != ychoice->get_default_case()) {
          bool found = false;
          // Iterate thru the subtree to find if any node in the existing case
          // exists. If so, don't report
          for (XMLNodeIter it = new_node->child_begin();
               it != new_node->child_end(); ++it) {
            XMLNode* xnode = &(*it);
            if (xnode != remove_node && 
                xnode->get_yang_node()->get_case() == ycase) {
              found = true;
            }
          }
          if (!found) {
            report_delete(remove_node);
          }
        }
      }
      new_node->remove_child(remove_node);
      break;
    }
    case XML_DATA_LIST_EXISTS: {
      // remove all the data with the list name
      for (XMLNode* xchild = new_node->get_first_child(); xchild; ) {
        if (xchild->get_local_name() == child_ynode->get_name() &&
            xchild->get_name_space() == child_ynode->get_ns()) {
          XMLNode* next_node = xchild->get_next_sibling();

          // Multiple keyspecs, report the deletion and remove it from dom
          report_delete(xchild);
          new_node->remove_child(xchild);
          xchild = next_node;
        } else {
          xchild = xchild->get_next_sibling();
        }
      }
      break;
    }
  }

  return status;
}


rw_yang_netconf_op_status_t XMLDocMerger::do_delete_missing(
                                XMLNode* new_node,
                                XMLNode* delta_node)
{
  rw_yang_netconf_op_status_t status = RW_YANG_NETCONF_OP_STATUS_OK;

  for (XMLNode* new_child = new_node->get_first_child();
      new_child; ) {
    XMLNode* tmp_child = delta_node->find(new_child->get_local_name().c_str(),
                                          new_child->get_name_space().c_str());
    if (!tmp_child) {
      // Delete the child from the config dom
      tmp_child = new_child;
      new_child = new_child->get_next_sibling();
      report_delete(tmp_child);
      new_node->remove_child(tmp_child);
    } else {
      XMLNode* found_node = new_child;
      YangNode* child_ynode = new_child->get_yang_node();
      // For list and leaf-list find the missing node by key or value
      
      xml_data_presence_t presence = check_if_data_exists(child_ynode, 
                                        delta_node, new_child, &found_node);
      if (presence == XML_DATA_MISSING) {
        // Node in the base config is not present in the delta, delete it
        tmp_child = new_child;
        new_child = new_child->get_next_sibling();
        report_delete(tmp_child);
        new_node->remove_child(tmp_child);
      } else {
        new_child = new_child->get_next_sibling();
      }
    }
  }

  return status;
}

XMLDocMerger::xml_data_presence_t XMLDocMerger::check_if_data_exists(
                                      YangNode* child_ynode,
                                      XMLNode* new_node,
                                      XMLNode* delta_child,
                                      XMLNode** found_node)
{
  // The YangNode exits, data exists check has to be made for lists, otherwise
  // the data should exist.
  xml_data_presence_t data_exists = XML_DATA_EXISTS;

  switch(child_ynode->get_stmt_type())
  {
    case RW_YANG_STMT_TYPE_LIST: {
      // For a list report an error only if the key already exists
      if (delta_child->get_first_child() == nullptr) {
        // No children but Yang Node present in the base DOM
        data_exists = XML_DATA_LIST_EXISTS;
        break;
      }

      XMLNode* matching_list_node = nullptr;
      rw_yang_netconf_op_status_t status = new_node->find_list_by_key (
                child_ynode, delta_child, &matching_list_node);
      if (status != RW_YANG_NETCONF_OP_STATUS_OK) {
        data_exists = XML_DATA_MISSING;
      } else {
        *found_node = matching_list_node;
      }
      break;
    }
    case RW_YANG_STMT_TYPE_LEAF_LIST: {
      std::string value = delta_child->get_text_value().c_str();
      if (value.empty()) {
        // No data present, but Node is present
        data_exists = XML_DATA_LIST_EXISTS;
        break;
      }

      XMLNode* duplicate_node = new_node->find_value(
                                         child_ynode->get_name(),
                                         value.c_str(),
                                         child_ynode->get_ns());
      if (!duplicate_node) {
        data_exists = XML_DATA_MISSING;
      } else {
        *found_node = duplicate_node;
      }
      break;
    }
    default:
      data_exists = XML_DATA_EXISTS;
  }

  return data_exists;
}

void XMLDocMerger::fill_defaults(
          YangNode* ynode,
          XMLNode*  new_node,
          XMLNode*  update_node,
          bool*     update_required
       )
{
  YangNode* yn = nullptr;
  XMLNode*  xchild = nullptr;

  for (YangNodeIter it = ynode->child_begin();
       it != ynode->child_end(); ++it) {
    yn = &(*it);
    const char* default_val = yn->get_default_value();
    if (!default_val && !yn->has_default()) {
      // Only default values or containers with default leaf descendents
      continue;
    }

    // Default values may be within a choice/case. Check if the choice has
    // another case. If the same case is present
    YangNode* ychoice = yn->get_choice();
    YangNode* ycase = yn->get_case();
    YangNode* other_choice = nullptr;
    YangNode* other_case   = nullptr;
    bool add_default = true;
    bool subtree_update = false;
    if (ychoice && ycase && (ychoice->get_default_case() != ycase)) {
      add_default = false;
    }

    for (XMLNodeIter xit = new_node->child_begin();
         xit != new_node->child_end(); ++xit) {
      xchild = &(*xit);
      if (xchild->get_local_name() == yn->get_name() &&
          xchild->get_name_space() == yn->get_ns()) {
        if (yn->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER) {
          // The container has a default descendant node
          XMLNode* update_child = nullptr;
          bool created = false;
          if ((update_child = update_node->find(
                                yn->get_name(), yn->get_ns())) == nullptr) {
            update_child = update_node->add_child(yn);
            created = true;
          }
          fill_defaults(yn, xchild, update_child, &subtree_update);
          if (!subtree_update && created) {
            update_node->remove_child(update_child);
          }
          *update_required = (*update_required || subtree_update);
        }
        // Default node already present in the new-dom
        add_default = false;
        break;
      }
      
      if (!ychoice) {
        // Not part of a choice
        continue;
      }

      other_choice = xchild->get_yang_node()->get_choice();
      if (!other_choice || (other_choice != ychoice)) {
        // Other node is not a choice, or not the same choice, not conflicting
        continue;
      }

      other_case = xchild->get_yang_node()->get_case();
      if (other_case && (ycase != other_case)) {
        // There is a node with conflicting case. Hence no default
        add_default = false;
        break;
      }

      // Same case, in-case the case is not default, some-other node in the same
      // case is set. Then add this default, unless the same node is found in
      // the new-dom.
      add_default = true;
    }
    if (add_default) {
      XMLNode* xn = new_node->add_child(yn, default_val);
      RW_ASSERT(xn);
      XMLNode* un = update_node->add_child(yn, default_val);
      if (yn->get_stmt_type() == RW_YANG_STMT_TYPE_CONTAINER) {
        // The container has a default descendant node
        fill_defaults(yn, xn, un, &subtree_update);
        if (!subtree_update) {
          new_node->remove_child(xn);
          update_node->remove_child(un);
        }
        *update_required = (*update_required || subtree_update);
      } else {
        *update_required = true;
      }
    }
  }
}

rw_status_t XMLDocMerger::report_delete(XMLNode* node)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  if (on_delete_) {
    rw_keyspec_path_t* key = nullptr;
    rw_yang_netconf_op_status_t ncrs = node->to_keyspec(&key);
    RW_ASSERT(ncrs == RW_YANG_NETCONF_OP_STATUS_OK);

    // ATTN: may be this can be set by on_delete callback
    rw_keyspec_path_set_category(key, nullptr, 
        RW_SCHEMA_CATEGORY_CONFIG);

    status = on_delete_(UniquePtrKeySpecPath::uptr_t(key));
  }
  return status;
}

rw_status_t XMLDocMerger::report_error(
                XMLNode* node,
                rw_yang_netconf_op_status_t nc_status,
                const char* err_msg)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  if (on_error_) {
    // Construct the str_path for the XMLNode
    char* path = nullptr;
    if (ypbc_schema_) {
      rw_keyspec_path_t* key = nullptr;
      rw_yang_netconf_op_status_t ncrs = node->to_keyspec(&key);
      RW_ASSERT(ncrs == RW_YANG_NETCONF_OP_STATUS_OK);

      rw_keyspec_path_get_new_print_buffer(key, nullptr, ypbc_schema_, &path);
      rw_keyspec_path_free(key, nullptr);
    }
    status = on_error_(nc_status, err_msg, path);

    if (path) {
      free(path);
    }
  }
  return status;
}

rw_yang_netconf_op_status_t XMLDocMerger::set_current_operation(XMLNode* delta_node)
{
  std::string operation = delta_node->get_attribute_value("operation", 
                                                           NETCONF_NS);
  if (operation.empty()) {
    // Nothing to do, the current_operation remainst he same as the parent
    // node's operation.
    return RW_YANG_NETCONF_OP_STATUS_OK;
  }

  auto it = edit_conf_operation_map_.find(operation);
  if (it == edit_conf_operation_map_.end()) {
    // operation not found, report error
    std::stringstream err_msg;
    err_msg << "Invalid value for attribute 'operation' - " << operation;
    report_error(delta_node,
             RW_YANG_NETCONF_OP_STATUS_BAD_ATTRIBUTE, 
             err_msg.str().c_str());
    return RW_YANG_NETCONF_OP_STATUS_BAD_ATTRIBUTE;
  }

  current_operation_ = it->second;

  return RW_YANG_NETCONF_OP_STATUS_OK;
}

