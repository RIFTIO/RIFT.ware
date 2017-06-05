/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_xml_dom_merger.hpp
 * @author Balaji Rajappa 
 * @date 2016/03/23
 * @brief Helper to apply config changes to a base DOM 
 */


#ifndef RW_XML_DOM_MERGER_HPP_
#define RW_XML_DOM_MERGER_HPP_

#include "rw_xml.h"
#include <functional>

namespace rw_yang {

// ATTN: use the IETF_NETCONF_DEFAULT_OPERATION_* from ietf-netconf.pb-c.h
enum XMLEditDefaultOperation
{
  XML_EDIT_OP_NONE,
  XML_EDIT_OP_MERGE,
  XML_EDIT_OP_REPLACE,
  XML_EDIT_OP_CREATE,
  XML_EDIT_OP_DELETE,
  XML_EDIT_OP_REMOVE
};

/**
 * Creates a copy of an existing DOM and merges edit_config changes to it.
 * The delta DOM is Netconf-ish XML.
 */
class XMLDocMerger
{
 public:
  
  /// Map to lookup the edit config operation enum from operation attribute
  typedef std::map<std::string, XMLEditDefaultOperation> edit_conf_operation_map_t;

  enum xml_data_presence_t {
    XML_DATA_MISSING, ///< Data not present 
    XML_DATA_EXISTS,  ///< Data exists but not a list (single data element)
    XML_DATA_LIST_EXISTS ///< Data exists as a list (multiple elements)
  };

  
 public:
  /**
   * Constructor for the XMLDocMerger
   */
  XMLDocMerger(
      /// [in] RW.XMLManager (not owned)
      XMLManager* mgr, 

       /// [in] DOM for which a copy to be created
      XMLDocument* base_dom,

      /// [in] Yang PB schema for the model
      const rw_yang_pb_schema_t* ypbc_schema = nullptr); 

  /**
   * Destructor
   */
  ~XMLDocMerger();

  // Cannot copy
  XMLDocMerger(const XMLDocMerger&) = delete;
  XMLDocMerger& operator=(const XMLDocMerger&) = delete;

 public:
  
  /**
   * Copies the base_dom and then merges the delta changes to it. 
   * During this process it emits events for update, delete and error
   */
  XMLDocument::uptr_t  copy_and_merge(
                /// [in] Delta XML that is to be merged.
                XMLDocument* delta,

                /// [in] Edit-Config Default Operation
                XMLEditDefaultOperation default_operation = XML_EDIT_OP_MERGE 
                          );

 public:
  /**
   * Internal method used for editing the new config using the delta
   * recursively traversing through the new config tree.
   *
   * Updates are added to the update_node tree, deletes are reported as 
   * callback (@see XMLDocMerger::on_delete_), errors are also reported as
   * callbacks (@see XMLDocMerger::on_error_).
   *
   * After this method, the new_node might add/lose nodes based on the
   * operation. The update_node gets more nodes on a merge/create/replace. 
   *
   * @returns Yang Netconf status(OK on success, otherwise appropriate failure)
   */
  rw_yang_netconf_op_status_t do_edit(
                /// [in/out] Node denoting the New Config tree
                XMLNode* new_node, 

                /// [in] Node denoting the Delta tree
                XMLNode* delta_node,

                /// [in/out] Node denoting the Update tree
                XMLNode* update_node,
                
                /// [out] Will be true if an update is required for the subtree
                bool* update_required);

  /**
   * Edits the config with a Delta that is not present in the New Config Tree.
   *
   * @returns Yang Netconf status(OK on success, otherwise appropriate failure)
   */
  rw_yang_netconf_op_status_t do_edit_new(
                /// [in] Yang descriptor for child node that is getting edited
                YangNode* child_ynode,

                /// [in/out] Parent node of the new config
                XMLNode* new_node,

                // [in] Delta child that is not existing in the new-config
                XMLNode* delta_child,

                // [in/out] Node that gets updated with new additions 
                XMLNode* update_node,

                /// [out] Will be true if an update is required for the subtree
                bool* update_required);

  /**
   * Edits the existing config tree with the delta.
   *
   * @returns Yang Netconf status(OK on success, otherwise appropriate failure)
   */
  rw_yang_netconf_op_status_t do_edit_existing(
                /// [in] Yang descriptor for child node that is getting edited
                YangNode* child_ynode,

                /// [in/out] Parent node of the new config node
                XMLNode* new_node,

                /// [in/out] Node in the new-config that exists
                XMLNode* new_child,

                // [in] Delta child that is exists in the new-config
                XMLNode* delta_child,

                // [in/out] Node that gets updated with new additions 
                XMLNode* update_node,

                /// [out] Will be true if an update is required for the subtree
                bool* update_required);

  /**
   * Deletes the specified node from the new config tree
   *
   * @returns Yang Netconf status(OK on success, otherwise appropriate failure)
   */
  rw_yang_netconf_op_status_t do_delete(
                /// [in] Yang descriptor for child node that is getting deleted
                YangNode* child_ynode,

                /// [in/out] Parent node of the node that is getting deleted
                XMLNode* new_node,

                /// [in/out] Node that is up for deletion in the new-config
                XMLNode* new_child,

                /// [in] delta that points to the node that is getting deleted
                XMLNode* delta_child);

  /**
   * Used in replace operation. Delete the nodes that are present in the
   * new-config but not in the delta.
   */
  rw_yang_netconf_op_status_t do_delete_missing(
                /// [in/out] Parent node in the new-config on which missing
                ///          nodes are to be found
                XMLNode* new_node,

                /// [in] Delta from which missing nodes to be identified
                XMLNode* delta_node);

  /**
   * For an existing Yang node, check if the data already exists. This is useful
   * for list type nodes (list and leaf-list) where there can be multiple data
   * nodes for the same Yang node.
   */
  xml_data_presence_t check_if_data_exists(
                /// [in] Yang descriptor 
                YangNode* child_ynode,

                /// [in] Node on which data-existence check perfomed
                XMLNode* new_node,

                /// [in] Data node which is to be checked in the new_node
                XMLNode* delta_child,

                /// [out] If data node found, set to the data node. Not modified
                //        otherwise.
                XMLNode** found_node);

  /**
   * Fills the default nodes for the new_node.
   */
  void fill_defaults(
          YangNode* ynode,
          XMLNode*  new_node,
          XMLNode*  update_node,
          bool* update_required
       ); 
                

  /**
   * Sets the current_operation_ based on the 'operation' attribute on the
   * delta_node. If no such attribute is found, then the current_operation
   * remains unchanged. Also reports an error if the attribute is an invalid
   * value.
   */
  rw_yang_netconf_op_status_t set_current_operation(XMLNode* delta_node);

  /**
   * Invoke the on_delete callback
   */
  rw_status_t report_delete(XMLNode* node);

  /**
   * Invoke the on_error callback
   */
  rw_status_t report_error(
                XMLNode* node,
                rw_yang_netconf_op_status_t status,
                const char* err_msg = nullptr);

 public:
  static edit_conf_operation_map_t edit_conf_operation_map_; 
  
 public:
  /// XML Manager holding the schema 
  XMLManager* xml_mgr_ = nullptr;

  /// Base Dom that is to be copied
  XMLDocument* base_dom_ = nullptr;

  /// Yang PB schema required for converting XML Node to keyspecs
  const rw_yang_pb_schema_t* ypbc_schema_ = nullptr;

  /// Current Edit-Config operation that is underway
  XMLEditDefaultOperation current_operation_ = XML_EDIT_OP_MERGE; 

  /// Callback that will be invoked when a Config Update is encountered during
  /// the merge.
  /// rw_status_t on_update(XMLNode* updated_node)
  std::function<rw_status_t(XMLNode*)>  on_update_;

  // rw_status_t on_delete(UniquePtrKeySpecPath::uptr_t deleted_ks)
  std::function<rw_status_t(UniquePtrKeySpecPath::uptr_t)> on_delete_;
 
  // rw_status_t on_error(rw_yang_netconf_op_status_t, 
  //              const char* error_msg, const char* error_path)
  std::function<rw_status_t(rw_yang_netconf_op_status_t, 
                  const char*, const char*)> on_error_; 
};

} // namespace rw_yang

#endif // RW_XML_DOM_MERGER_HPP_
