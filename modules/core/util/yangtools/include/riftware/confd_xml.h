
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
 * @file confd_xml.h
 * @author Vinod Kamalaraj
 * @date 2014/27/7
 * @brief A XML to confd data structures interface library
 */

#ifndef CONFD_XML_HPP_
#define CONFD_XML_HPP_

#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#include "rwlib.h"
#include "rw_app_data.hpp"
#include "yangmodel.h"
#include "rw_xml.h"
#include "rw_tree_iter.h"
#include "confd_lib.h"

#if __cplusplus
#include <vector>
#include <unordered_set>
#endif

/* Begining of defintitions from the Yang based XML Node */

/**
 * @defgroup CONFD-XML-Library
 *
 * Riftware needs to interact with Tail-f CONFD as a
 * management agent. Riftware thus needs to convert data structures in CONFD
 * format to various XML abstractions that is used in Riftware.
 * All conversions between Riftware and CONFD will be modelled using
 * the traverser/builder model or the walker/visitor model
 * @{
 */



#if __cplusplus

/**
 * Maintains a set of namespaces which are not used by Riftware.
 * These namespaces tries to leak into uAgent configuration DOM while
 * reloading configuration from CDB, for eg: after a restart.
 * ConfdTLVTraverser while building/decoding the subscription points
 * sent by confd, looks up in this map to check whether the
 * confd_tlv_value_t needs to be considered for building the 
 * DOM.
 */
static std::unordered_set<std::string> ns_rejection_map {
  "urn:ietf:params:xml:ns:yang:ietf-netconf-acm",
  "http://tail-f.com/yang/acm",
  "http://tail-f.com/ns/aaa/1.1",
  "http://tail-f.com/yang/common-monitoring",
  "http://tail-f.com/ns/common/query",
  "http://tail-f.com/yang/common",
  "http://tail-f.com/yang/confd-monitoring",
  "http://tail-f.com/ns/netconf/query",
  "http://tail-f.com/ns/netconf/transactions/1.0",
  "http://tail-f.com/ns/tailf-rest-error",
  "http://tail-f.com/ns/tailf-rest-query",
  "http://tail-f.com/ns/webui",
  // YUMA modules (ATTN: May not be actually needed)
  "http://netconfcentral.org/ns/yuma-nacm",
  "http://netconfcentral.org/ns/yuma-proc",
  "http://netconfcentral.org/ns/yangdump",
  "http://netconfcentral.org/ns/yuma-types",
  "http://netconfcentral.org/ns/yuma-interfaces",
  "http://netconfcentral.org/ns/yangdiff",
  "http://netconfcentral.org/ns/yuma-time-filter",
  "http://netconfcentral.org/ns/yuma-system",
  "http://netconfcentral.org/ns/yuma-ncx",
  "http://netconfcentral.org/ns/yuma-mysession",
  "http://netconfcentral.org/ns/yuma-arp",
};


/**
 * Utility Function that converts a YangNode's name space and tag to a
 * confd xml_tag structure, utilizing tags associated in the yang model
 * @return RW_STATUS_SUCCESS, if both namespace and tag hash values can
 * be found, RW_STATUS_FAILURE otherwise
 */

rw_status_t find_confd_hash_values (
    rw_yang::YangNode *node,       /**< [in] The YangNode */
    struct xml_tag *tag); /**< [out] The confd XML tag */

/**
 * Get the keys of the next entry in a list. This function returns the keys to
 * the current node, in a confd data structure, and also the next element in the
 * list.
 *
 * @return RW_STATUS_SUCCESS always
 */
rw_status_t get_current_key_and_next_node (
    rw_yang::XMLNode *node, /**< [in] the node the next */
    confd_cs_node *parent, /**< [in] The associated confd schema node */
    rw_yang::XMLNode **next_node, /**< [out] The next node in the list */
    confd_value_t *values, /**< [out] Confd values array */
    int *values_count); /**< [out] number of elements in confd array */


/**
 * Given a XML subtree rooted at a node, and a confd keypath, search the subtree
 * for the node at keypath.
 *
 * @return The XML Node at Path 
 */

rw_yang::XMLNode* get_node_by_hkeypath (rw_yang::XMLNode *node,
                                        confd_hkeypath_t *keypath,
                                        uint16_t stop_at = 0);

/**
 * Given a XML sub tree rooted at a node, and a hkeypath that contains choice
 * statments, find the node pointed to by the path. If the node has any of the
 * case children of the choice queried for, return that case statement.
 *
 * @return RW_STATUS_SUCCESS, if the node can be found, RW_STATUS_FAILURE
 * otherwise
 */
rw_status_t get_confd_case (
    rw_yang::XMLNode *node, /**< [in] root of XML subtree */
    confd_hkeypath_t *keypath, /**< [in] Path to node which needs to be checked
                                for the choice */
    confd_value_t *choice, /**< [in] The choice statement that is present */
    confd_value_t *case_val); /**< [out] The case statement value */
    
rw_status_t get_element (rw_yang::XMLNode *node, confd_cs_node *cs_node,
                         confd_value_t *value);


namespace rw_yang {
  
const uint16_t CONFD_MAX_STRING_LENGTH=1024;

typedef std::vector<confd_tag_value_t *> confd_tlv_array_t;
typedef confd_tlv_array_t::iterator cta_iter;


  /**
   * The traverser for CONFD data. CONFD data that is of interest in Riftware is
   * constructed as an array of Tag/Value pointers. Tags are used to indicate
   * descend into children, which maybe YANG container nodes or list nodes. The
   * end of descend is also indicated using specialized tags.
   *
   * The CONFD traverser is constructed using the array. The state of the
   * traverser is the index of the current node
   */

class ConfdTLVTraverser : public Traverser
{
public:
  typedef std::stack <struct confd_cs_node* > confd_schema_stack_t;
 
public:
  
  /**
   * Construct a confd traverser given a builder, an array of values and a count
   *
   */
  ConfdTLVTraverser(Builder           *builder,
                    confd_tag_value_t *values, 
                    int               count);

  /// @see Traverser::build_next()
  virtual rw_xml_next_action_t build_next();

  /// @see Traverser::build_next_sibling()
  virtual rw_xml_next_action_t build_next_sibling();

  /// @see Traverser::stop()
  virtual rw_yang_netconf_op_status_t stop();

  /// @see Traverser::abort_traversal()
  virtual rw_yang_netconf_op_status_t abort_traversal();

  /// Destructor
  virtual ~ConfdTLVTraverser();

public:
  /// Sets the confd schema root node. 
  ///
  /// This is required for RPC to search from the RPC path, since the TLV will
  /// not have the RPC node (only input params)
  void set_confd_schema_root(confd_cs_node *cs_node);

  /// Gets the current confd schema node for the given tag.
  confd_cs_node* get_confd_schema_node(struct xml_tag tag);


public:
  confd_tlv_array_t tlv_tree_;
  cta_iter         current_node_;

  confd_schema_stack_t   schema_stack_;
};


/**
 * A Traverser for Confd Keypath. This traverser walks the keypath as if it
 * were XML.
 */

class ConfdKeypathTraverser : public Traverser
{
  
public:
  
  /**
   * Construct a confd traverser given a builder, an array of values and a count
   *
   */
  ConfdKeypathTraverser(Builder *builder, confd_hkeypath_t *keypath);

  /// @see Traverser::build_next()
  virtual rw_xml_next_action_t build_next();

  /// @see Traverser::build_next_sibling()
  virtual rw_xml_next_action_t build_next_sibling();

  /// @see Traverser::stop()
  virtual rw_yang_netconf_op_status_t stop();

  /// @see Traverser::abort_traversal()
  virtual rw_yang_netconf_op_status_t abort_traversal();

  /// Destructor
  virtual ~ConfdKeypathTraverser();


public:
  confd_hkeypath_t *keypath_;
  int8_t          curr_idx_;
  uint8_t         curr_key_;
};

/**
 * The confd TLV builder object builds a TV array that can be send as a response
 * to a get_object call. This builder needs to take a Node as the argument so
 * that all lists can be skipped over. 
 */


class ConfdTLVBuilder : public Builder
{
public:
  /**
   * The constructor for the ConfdTLV builder takes the confd schema
   * representation
   */
  ConfdTLVBuilder (
      /**
       *  [in] The schema node as per CONFD  interpretation. has only schema
       * information
       */
      confd_cs_node *cfd_schema_node,
  
      /**
       * [in] include_lists By default, lists are not included, as CONFD will
       * request for list items by key. This mechanism is not present for RPC,
       * in which case lists have to be included in the traversal.
       */

      bool include_lists = false);
                                         
                    
  /// Destructor
  virtual ~ConfdTLVBuilder();

  ///@see Builder::append_child(XMLNode *node)
  virtual rw_xml_next_action_t append_child (XMLNode *node);

  
  ///@see Builder::push(XMLNode *node)
  virtual rw_xml_next_action_t push (XMLNode *node);

  ///@see Builder::pop()
  virtual rw_xml_next_action_t pop();

  /**
   * After the data has been build for an object, this function returns the
   * number of confd_tag_value_t structures that are necessary
   *
   * @return length of the list of confd_tag_value_t
   */
  size_t length();

  /**
   * Copy out the DOM to the passed in list. This is a destructive operation.
   * The reason for the destructive ness is that there is no real mechanism
   * in which to send a list to confd and ensure no memory leaks, without either
   * this or a complicated method of converting the confd values back to strings
   * and back again
   *
   * @return length of copied out data
   */

  size_t copy_and_destroy_tag_array (confd_tag_value_t *values);
  
public:

  typedef std::list <confd_tag_value_t> cfd_tag_value_list_t;
  typedef cfd_tag_value_list_t::iterator cfd_tv_iter;

  typedef std::tuple <cfd_tv_iter,std::string> leaf_list_value_t;
  typedef std::unordered_map<YangNode*, leaf_list_value_t> leaf_list_map_t;
  typedef leaf_list_map_t::iterator leaf_list_iter_t;

  /// A list of confd_tag_value_t
  cfd_tag_value_list_t values_;
  
  typedef std::stack <struct confd_cs_node* > cfd_schema_stack_t;

  /// Stack the confd schema nodes
  cfd_schema_stack_t schema_stack_;

  struct confd_cs_node *current_schema_node_;

  leaf_list_map_t leaf_lists_;  
  
  bool include_lists_;
};

/** @} */

/*!
 * @addtogroup TreeIters 
 *
 * @{
 */

/*
 * Functions that are friends, but are free 
 */
class RwTLVAIterator;

rw_status_t get_string_value (const RwTLVAIterator *iter,
                              std::string& str);
template <class RwTreeIterator>
rw_tree_walker_status_t build_iterator_child (RwTLVAIterator *iter,
                                              RwTreeIterator *child);

/*!
 * @page RwTLVAIterator Iterator for a CONFD TLV Array
 * The RwTLVAIterator is an object that iterates over a CONFD TLV Array as a
 * subtree
 * The RWTLVA array is fully qualified by a pointer to the start of the array
 * and the length.
 
 * Each node that is of type C_XMLBEGIN is a complex node, which has children.
 * The next sibling of such a node is found by traversing the array to a array
 * element at its same level - the node after a C_XMLEND which is itself not a
 * C_XMLEND. There are no more siblings for a node with type C_XMLBEGIN when two
 * consecutive C_XMLEND types are encountered in traversal.

 * Each node that is not of type C_XMLBEGIN does not have any children. Its next
 * sibling is the next array index, except if that element has a type of XMLEND,
 * in which case it has no more siblings.
 */

class RwTLVAIterator
{
 private:
  /*!
   * The start elememt of a TLVArray. This is also the first child of the root.
   */
  confd_tag_value_t *root_ = 0;

  /*!
   * The number of nodes in this tree. This is also the length of the TLV array   
   */

  size_t            length_ = 0;

  /*!
   * A type for a path of the confd tree.
   */
  
  typedef std::stack<uint32_t> confd_path;
  confd_path        path_;

  /*!
   * The position witin the array that this iterator points to
   */
  size_t          position_ = 0;

  /*!
   * If the TLV is of type list, the current entry is a leaf list. An index into
   * the list is needed for proper traversal
   */

  size_t         list_index_ = 0;
  
  /*!
   * The confd schema node for this value
   */
  struct confd_cs_node *cs_node_ = 0;
  
 public:
  /*!
   * Accessor functions
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations
   */
  const confd_tag_value_t *root() const {
    return root_;
  }

  const struct confd_cs_node *schmea_node() const {
    return cs_node_;
  }
  
  size_t length() const {
    return length_;
  }

  size_t depth() const {
    return path_.size();
  }

  size_t position() const {
    return position_;
  }

  size_t list_index () const {
    return list_index_;
  }
  
 public:

  /*!
   * Build a default TLV iterator
   */
  RwTLVAIterator(){}
  
  /*!
   * Build a RwTLVAIterator given a root and a length
   */
  RwTLVAIterator(confd_tag_value_t *root, uint32_t length, struct confd_cs_node *csnode)
      :root_(root), length_(length), cs_node_(csnode) {}
  /*!
   * Move the iterator to its first child, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */
  
  rw_status_t move_to_first_child();

  /*!
   * Move the iterator to its next sibling, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_next_sibling();

  /*!
   * Move the iterator up to its current move_to_parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();

  /*!
   * Find the yang node name 
   */
  
  const char *get_yang_node_name () const;

  /*!
   * Find the yang namespace
   */

  const char *get_yang_ns() const;


  /*!
   * Find the SchemaNode associated with the current position when provided with
   * the schema node of the parent
   */

  template <class SchemaNode>
      SchemaNode *get_schema_node (SchemaNode *parent)
  {
    RW_ASSERT(parent);
    RW_ASSERT(position_ < length_);

    return rw_confd_search_child_tags (parent,
                                       root_[position_].tag.ns,
                                       root_[position_].tag.tag);
  }

  /*!
   * The value held in this iterator. 
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;

  /*
   * free friends 
   */
  
  friend rw_status_t get_string_value(const RwTLVAIterator *iter,
                                      std::string& val);
  
  template <class RwTreeIterator>
  friend rw_tree_walker_status_t build_iterator_child (RwTLVAIterator *iter,
                                                       RwTreeIterator *child);

};

inline bool operator == (const RwTLVAIterator& lhs,
                         const RwTLVAIterator& rhs)
{
  if (!lhs.depth()){
    return (!rhs.depth() && (lhs.root() == rhs.root()));
  } else if (!rhs.depth()) {
    return false;
  }

  if (!lhs.root() || !rhs.root()) {
    return false;
  }
  
  return (lhs.root() + lhs.position() == rhs.root() + rhs.position());
}

inline bool operator != (const RwTLVAIterator& lhs,
                         const RwTLVAIterator& rhs)
{
  return !(lhs == rhs);
}

/*!
 * @page RwHKeyPathIterator Iterator for a CONFD Keypath
 * The HKEYPATH object represents an XML path in the confd space. The HKEYPATH
 * is an array of pointer, with each pointer in the array representing a node
 * in the path. Each such node can support an array of leaf values.
 */


class RwHKeyPathIterator
{
 private:
  /*!
   * A pointer to the HKeypath that is being iterated over
   */
  confd_hkeypath_t *h_key_path_ = 0;

  /*!
   * The current depth. This represents the level
   */

  size_t            depth_ = 0;

  /*!
   * The position witin the node that this iterator points to
   */
  size_t          position_ = 0;

  /*!
   * Find a cs node - used by other member functions
   */

  struct confd_cs_node *cs_node() const;
  
 public:
  
  /*!
   *  Accessor methods for private members
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations
   */

  const confd_hkeypath_t *h_key_path() const {
    return h_key_path_;
  }

  size_t depth() const {
    return depth_;
  }

  size_t position() const {
    return position_;
  }
  
 public:
  /*!
   * Build a default RwHKeyPathIterator iterator
   */
    RwHKeyPathIterator();
    
  /*!
   * Build a RwHKeyPathIterator a hkeypath
   */
  RwHKeyPathIterator(confd_hkeypath_t *h_key_path);

  /*!
   * Default destructor
   */
  
  ~RwHKeyPathIterator();

  /*!
   * Move the iterator to its first child, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */
  
  rw_status_t move_to_first_child();
  
  /*!
   * Move the iterator to its next sibling, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_next_sibling();

  /*!
   * Move the iterator up to its current move_to_parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();

  /*!
   * Find the SchemaNode associated with the current position when provided with
   * the schema node of the parent
   */

  template <class SchemaNode>
      SchemaNode *get_schema_node (SchemaNode *parent)
  {
    RW_ASSERT(parent);

    RW_ASSERT(depth_ < h_key_path_->len);
    if (C_XMLTAG != h_key_path_->v[depth_][0].type) {
      return (parent->get_key_at_index(position_ + 1));
    }
    
    xml_tag *tag = &h_key_path_->v[depth_][0].val.xmltag;
    return ( rw_confd_search_child_tags (parent, tag->ns, tag->tag));
  }

  /*!
   * The value held in this iterator. 
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;

  /*!
   * Find the yang node name 
   */
  
  const char *get_yang_node_name () const;

  /*!
   * Find the yang namespace
   */

  const char *get_yang_ns() const;

};


inline bool operator == (const RwHKeyPathIterator& lhs,
                         const RwHKeyPathIterator& rhs)
{
  if (!lhs.h_key_path()) {
    if (!rhs.h_key_path()) {
      return true;
    }
    return false;
  }

  return ((lhs.h_key_path() == rhs.h_key_path()) &&
          (lhs.depth() == rhs.depth()) &&
          (lhs.position() == rhs.position()));
}

inline bool operator != (const RwHKeyPathIterator& lhs,
                         const RwHKeyPathIterator& rhs)
{
  return !(lhs == rhs);
}

/*
 * Functions that are friends, but are free 
 */
class RwTLVListIterator;

rw_tree_walker_status_t build_and_move_iterator_child_value (RwTLVListIterator* parent_iter,
                                                             RwPbcmTreeIterator* child_iter);
rw_status_t get_string_value (const RwTLVAIterator *iter,
                              std::string& str);
template <class RwTreeIterator>
rw_tree_walker_status_t build_iterator_child (RwTLVListIterator *iter,
                                              RwTreeIterator *child);

/*!
 * @page RwTLVListIterator Iterator for a CONFD TLV List
 * The RwTLVListIterator is an object that iterates over a CONFD TLV List as a
 * subtree
 *
 * Each node that is of type C_XMLBEGIN is a complex node, which has children.
 * The next sibling of such a node is found by traversing the array to a array
 * element at its same level - the node after a C_XMLEND which is itself not a
 * C_XMLEND. There are no more siblings for a node with type C_XMLBEGIN when two
 * consecutive C_XMLEND types are encountered in traversal.

 * Each node that is not of type C_XMLBEGIN does not have any children. Its next
 * sibling is the next array index, except if that element has a type of XMLEND,
 * in which case it has no more siblings.
 *
 * Data from confd is always presented to the uagent/subscriber as an Array. But
 * when building a list to send to confd, using an array can be an issue.
 * Frequent mallocs and memmoves would be required to leave a TLV array in a state
 * that is consistent, and is always a tree.
 */

class RwTLVListIterator
{
 public:
  /*!
   * A type for a path of the confd tree.
   */
  typedef std::list <confd_tag_value_t>  tlv_list_t;
  typedef tlv_list_t::iterator           tlv_list_iter_t;
  typedef std::stack<tlv_list_iter_t>    confd_path;

 private:

  /*!
   * The list of TLV. The first element is the nominal root of the tree
   */

  tlv_list_t                             elements_;


  /*!
   * The position witin the list that this iterator points to
   */
  
  tlv_list_iter_t                        position_;

  /*!
   * Path from the root to this element
   */
  
  confd_path                             path_;
  
  /*!
   * If the TLV is of type list, the current entry is a leaf list. An index into
   * the list is needed for proper traversal
   */

  size_t         list_index_ = 0;
  
  /*!
   * The confd schema node for this value
   */
  struct confd_cs_node *cs_node_ = 0;

  /*!
   * A private get method for friends
   */
  rw_status_t get_value(tlv_list_iter_t& value);

  /*!
   * A private move to a particular child method for friends
   */
  rw_status_t move_to_child(tlv_list_iter_t& child);
 public:
  /*!
   * Accessor functions
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations
   */

  const struct confd_cs_node *schema_node() const {
    return cs_node_;
  }
  
  size_t length() const {
    return elements_.size();
  }

  size_t depth() const {
    return path_.size();
  }

  size_t list_index () const {
    return list_index_;
  }
  
 public:

  /*!
   * Build a default TLV iterator
   */
  RwTLVListIterator(){}
  
  /*!
   * Move the iterator to its first child, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */
  
  rw_status_t move_to_first_child();

  /*!
   * Move the iterator to its next sibling, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_next_sibling();

  /*!
   * Move the iterator up to its current move_to_parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();

  /*!
   * Find the yang node name 
   */
  
  const char *get_yang_node_name () const;

  /*!
   * Find the yang namespace
   */

  const char *get_yang_ns() const;

  /*!
   * Add a child of a specified type. The return type is significant.
   * @param [in] child_val Value of the child to be added
   *
   * @return RW_TREE_WALKER_FAILURE  The child could not be added
   *         RW_TREE_WALKER_SKIP_SUBTREE The subtree has been added, all
   *            children of the child have been added as well.
   *         RW_TREE_WALKER_SUCCESS The child has been created, and the iterator
   *            has been positioned at the added child, so further processing
   *            of the children of the child can be made.
   */

  rw_tree_walker_status_t add_child (rw_ylib_data_t *child_val);


  /*!
   * The value held in this iterator. 
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;

  /*
   * free friends 
   */
  
  friend rw_status_t get_string_value(const RwTLVListIterator *iter,
                                      std::string& val);
  
  template <class RwTreeIterator>
  friend rw_tree_walker_status_t build_iterator_child (RwTLVListIterator *iter,
                                                       RwTreeIterator *child);
  friend rw_tree_walker_status_t
      build_and_move_iterator_child_value (RwTLVListIterator* parent_iter,
                                           RwPbcmTreeIterator* child_iter);

};

inline bool operator == (const RwTLVListIterator& lhs,
                         const RwTLVListIterator& rhs)
{
  return false;
}

inline bool operator != (const RwTLVListIterator& lhs,
                         const RwTLVListIterator& rhs)
{
  return !(lhs == rhs);
}

template <class RwTreeIterator>
rw_tree_walker_status_t build_iterator_child (RwTLVListIterator *parent_iter,
                                              RwTreeIterator *child_iter)
{
  rw_ylib_data_t child_val;
  
  rw_status_t rs = child_iter->get_value(child_val);
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
  
  switch (child_val.type) {
    case RW_YLIB_DATA_TYPE_TEXT:
      RW_ASSERT_NOT_REACHED();
      
    case RW_YLIB_DATA_TYPE_PB: 
      rt = add_tlv_list_child (parent_iter, &child_val.rw_pb,
                               child_iter->get_yang_node_name(),
                               child_iter->get_yang_ns(),
                               child);
      break;

    case RW_YLIB_DATA_TYPE_CONFD:
      return RW_TREE_WALKER_FAILURE;

    case RW_YLIB_DATA_TYPE_XML_NODE:
      return RW_TREE_WALKER_FAILURE;
      break;
  }

  if (rt != RW_TREE_WALKER_SUCCESS)  {
    // no need to move to a child
    return rt;
  }

  rs = parent_iter->move_to_child (child);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);
  return rt;
}

rw_status_t get_string_value (rw_confd_value_t *v,
                              std::string& str);


rw_status_t get_string_value (const RwTLVAIterator *iter,
                              std::string& str);

// Copiers for PBCM iterator
rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                             RwTLVAIterator* child_iter);

// Copiers for Schema Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwSchemaTreeIterator* parent_iter,
                                                             RwHKeyPathIterator* child_iter);

// Copiers for Confd TLV List Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwTLVListIterator* parent_iter,
                                                             RwPbcmTreeIterator* child_iter);

}

    


/** @} */
#endif
#endif



