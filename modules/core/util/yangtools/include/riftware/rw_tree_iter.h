
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_tree_iter.h
 * @author Vinod Kamalaraj
 * @date 2015/17/3
 * @brief Iterator based tree library
 */

#ifndef RW_TREE_ITER_HPP_
#define RW_TREE_ITER_HPP_

#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#include "rwlib.h"
#include "rw_app_data.hpp"
#include "yangmodel.h"
#include "rw_xml.h"
#include "confd_lib.h"

/*!
 * @addtogroup TreeIters Iterator based Tree algoritms
 *
 * @{
 */

/*!
 * @mainpage
 *
 * The tree iterator library provides generic algorithms that work on [yang]
 * schema based trees. Riftware is dependent on YANG, and derived data
 * structures for lot of core operations. The intent of this library is to split
 * the algorithms used by the different representations of trees from the actual
 * data representation.
 *
 * The following algorithms are currently available:
 * - @ref Find
 * - @ref Copy
 * - @ref Merge
 *
 * The following iterators are currently supported
 *
 * - @ref RwPbcmTreeIterator
 * - @ref RwXMLTreeIterator
 * - @ref RwSchemaTreeIterator
 * - @ref RwKsPbcmIterator
 * - @ref RwPbcmDomIterator
 * - @ref RwTLVAIterator
 * - @ref RwHKeyPathIterator
 * - @ref RwTLVListIterator
 */

#if __cplusplus

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

namespace rw_yang {

/*!
 * A structure that encapuslates a confd based field value
 */
  
typedef struct rw_confd_value_s {
  struct confd_cs_node *cs_node;
  confd_value_t              *value;
} rw_confd_value_t;

typedef enum {
  RW_YLIB_DATA_TYPE_INVALID = 0,
  RW_YLIB_DATA_TYPE_TEXT,
  RW_YLIB_DATA_TYPE_PB,
  RW_YLIB_DATA_TYPE_KS_PATH,
  RW_YLIB_DATA_TYPE_KS_KEY,
  RW_YLIB_DATA_TYPE_XML_NODE,
  RW_YLIB_DATA_TYPE_CONFD
} rw_ylib_data_type_e;


typedef enum {
  RW_YLIB_LIST_POSITION_FIRST = 1,
  RW_YLIB_LIST_POSITION_LAST,
  RW_YLIB_LIST_POSITION_SPECIFIED
} rw_ylib_list_position_e;
/*!
 * A generic value for getting a value for a YANG based node
 */

typedef struct rw_ylib_data_s
{
  rw_ylib_data_type_e type;
  union {
    rw_confd_value_t    rw_confd;
    ProtobufCFieldInfo  rw_pb;
    XMLNode             *xml;
    rw_schema_ns_and_tag_t path;
  };
} rw_ylib_data_t;

typedef enum
{
  RW_TREE_WALKER_SUCCESS,
  RW_TREE_WALKER_FAILURE,
  RW_TREE_WALKER_SKIP_SUBTREE,
  RW_TREE_WALKER_SKIP_SIBLINGS,
  RW_TREE_WALKER_STOP
} rw_tree_walker_status_t;


class RwPbcmTreeIterator;
class RwXMLTreeIterator;
class RwSchemaTreeIterator;
class RwKsPbcmIterator;
class RwPbcmDomIterator;

/*!
 * @page Utilities Utility functions 
 */

/*!
 * Find a child that is specified in needle. This is a 1 level find,
 * an immediate child is searched for
 * @param [in] haystack The tree node searched
 * @param [in] needle The key to use for the search
 * @param [out] found The node that is found
 * @returns
 *  - RW_TREE_WALKER_SUCCESS if a child is found
 *  - RW_TREE_WALKER_FAILURE if a child doesnt exist
 */

template <class RwTreeIteratorSrc, class RwTreeIteratorDest>
rw_tree_walker_status_t find_child (RwTreeIteratorSrc *haystack,
                                    RwTreeIteratorDest *needle,
                                    RwTreeIteratorSrc  *found);

  /*!
   * Add a child of a specified type and move to it
   * @param [in] parent Iterator value of the parent node
   * @param [in] child Iterator value of the child to be added
   *
   * @return
   *   - RW_TREE_WALKER_FAILURE  The child could not be added
   *    - RW_TREE_WALKER_SUCCESS The child has been created, and the iterator
   *            has been positioned at the added child, so further processing
   *            of the children of the child can be made.
   */

template <class RwTreeIteratorSrc, class RwTreeIteratorDest>
rw_tree_walker_status_t set_and_move_iterator_child_value (RwTreeIteratorSrc *parent,
                                                           RwTreeIteratorDest *child);

  /*!
   * Add a child of a specified type. The return type is significant.
   * @param [in] parent Iterator value of the parent node
   * @param [in] child Iterator value of the child to be added
   *
   * @return
   *   - RW_TREE_WALKER_FAILURE  The child could not be added
   *   - RW_TREE_WALKER_SKIP_SUBTREE The subtree has been added, all
   *            children of the child have been added as well.
   *    - RW_TREE_WALKER_SUCCESS The child has been created, and the iterator
   *            has been positioned at the added child, so further processing
   *            of the children of the child can be made.
   */


template <class RwTreeIteratorSrc, class RwTreeIteratorDest>
rw_tree_walker_status_t build_and_move_iterator_child_value (RwTreeIteratorSrc *parent,
                                                             RwTreeIteratorDest *child);

/*!
 * @page Copy Copier - A Iterator based copier
 *
 * The copier class enalbes copying from one tree data to a different type. The
 * object is instantiated with the root of the destination type, and the source
 * tree is walked to produce a copy.
 *
 * This algorithm has been tested for the following types:
 *  - RwHKeyPathIterator -> RwSchemaTreeIterator
 *  - RwKsPbcmIterator -> RwPbcmDomIterator
 *  - RwPbcmTreeIterator -> RwPbcmTreeIterator
 *  - RwPbcmTreeIterator ->  RwXMLTreeIterator
 *  - RwSchemaTreeIterator -> RwPbcmTreeIterator
 *  - RwTLVAIterator -> RwPbcmTreeIterator
 *  - RwXMLTreeIterator -> RwPbcmTreeIterator 
 */

template <class RwTreeIteratorSrc, class RwTreeIteratorDest>
class Copier {
 private:
  RwTreeIteratorDest *dest_;

 public:
  Copier (RwTreeIteratorDest *dest)
      :dest_(dest) {}
  
  rw_tree_walker_status_t handle_child (RwTreeIteratorSrc *child) {
    return build_and_move_iterator_child_value (dest_, child);

  }
                                        
  rw_status_t move_to_parent() {

    return dest_->move_to_parent();
  }
};


/*!
 * @page Merge Merge - A class that enables merging
 *
 * The merge class enalbes merging from one tree data to a different type. The
 * object is instantiated with the node of the destination type, and the source
 * node is walked to produce a merge.
 *
 * This algorithm has been tested for the following types:
 * - RwKsPbcmIterator -> RwPbcmDomIterator
 * - RwPbcmTreeIterator -> RwPbcmTreeIterator
 */

template <class RwTreeIteratorSrc, class RwTreeIteratorDest>
class Merge {
 private:
  RwTreeIteratorDest *dest_;

 public:
  Merge (RwTreeIteratorDest *dest)
      :dest_(dest) {}
  
  rw_tree_walker_status_t handle_child (RwTreeIteratorSrc *child) {
    RwTreeIteratorDest dest_child;

    if (child->is_leafy()) {
      return set_and_move_iterator_child_value (dest_, child);
    }
      
    // Not leafy - check if there is an existing value 
    rw_tree_walker_status_t rw  = find_child (dest_, child, &dest_child);
    if (RW_TREE_WALKER_SUCCESS == rw) {
      dest_->move_to_child (&dest_child);
      return RW_TREE_WALKER_SUCCESS;
    }

    rw_ylib_data_t child_val;
    
    rw_status_t rs = child->get_value(&child_val);
    RW_ASSERT(RW_STATUS_SUCCESS == rs);
    
    return build_and_move_iterator_child_value (dest_, child);

  }
  
  rw_status_t move_to_parent() {
    
    return dest_->move_to_parent();
  }
};

/*!
 * @page Find
 * The find class enables finding a node in a tree that is specified by a key.
 *
 * The following types have been tested with the Find algorithms
 *
 * - RwPbcmTreeIterator ->RwSchemaTreeIterator 
 */

template <class RwDataIterator, class RwKeyIterator>
class Finder {
 private:
  RwKeyIterator key_;


 public:
  std::list<RwDataIterator *> matches_;
  Finder (RwKeyIterator *key)
      :key_(*key) {}
  
  rw_tree_walker_status_t handle_child (RwDataIterator *child) {
    RwKeyIterator next_key;

    if (child->is_leafy()) {
      return RW_TREE_WALKER_SKIP_SUBTREE;
    }
      
    // Not leafy - check if there is an existing value 
    rw_tree_walker_status_t rw  = find_child (&key_, child, &next_key);
    if (RW_TREE_WALKER_SUCCESS == rw) {
      
      // If the dest_child does not have any non-leaf children, the *match*
      // has been made.
      if (!next_key.non_leaf_children()) {
        matches_.push_back(child);
        return RW_TREE_WALKER_SKIP_SUBTREE;
      } 
      key_ = next_key;
      return RW_TREE_WALKER_SUCCESS;
    } 

    // Skip this subtree - either not a path matching this node or there are no matches
    return RW_TREE_WALKER_SKIP_SUBTREE;
  }
  
  rw_status_t move_to_parent() {
    
    return key_.move_to_parent();
  }
};



template <class FoundAction, class NotFoundAction>
rw_tree_walker_status_t find_child_iter (RwPbcmTreeIterator *parent,
                                         RwSchemaTreeIterator *key_spec,
                                         bool all_leafs,
                                         bool all_keys,
                                         FoundAction& found,
                                         NotFoundAction& not_found);


/*!
 * Set the value of the current iterator position into a string.
 *
 * @return RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE on failure
 */

rw_status_t get_string_value (
    /*!
     * The iterator from which the string value is to be set
     */
    const RwPbcmTreeIterator *iter,
    
    /*!
     * The string in which the value is to be set
     */

      std::string& val);


/*!
 * Build a child for this iterator, and move to it. The values of the
 * child are in the child node
 *
 * @return The return value is dependent on the value of the child iterator.
 *         If the child is a PBCM iterator, the entire submessage is created,
 *         and a 
 */

template <class RwTreeIterator>
rw_tree_walker_status_t build_iterator_child (
    
    /*! 
     * The PBCM iterator for which a child value/message has to be set
     */
    
    RwPbcmTreeIterator *iter,
    
    /*!
     * The child which has to be added at this position
     */
    
    RwTreeIterator *child);




/*!
 * @page RwPbcmTreeIterator
 * The RwPbcmTreeIterator provides an object that helps iterating over a
 * ProtobufCMessage as if it were a tree. 
 */

class RwPbcmTreeIterator
{
 private:
  
  /*! The root node of this tree. The iterator is constructed using a
   *  protobuf CMessage. The root node is set to that node
   */
  
  ProtobufCMessage*                      root_ = 0;

  /*!
   * A node in the PBCM can be anonymously typed as its parent message,
   * the index of the field within the parent message and for repeated
   * fields, the index of the field itself.
   */
  
  typedef std::tuple<ProtobufCMessage*, unsigned int, unsigned int> node;  
  typedef std::stack<node> node_stack;
  
  /*!
   * A stack of ancestors of this iterator. A get parent creates an iterator.
   * The top of the stack has the state of this iterator
   */
  
  node_stack                           node_state_;



  /*!
   * A private convenience method to find the lowest message descriptor
   * in a PbcmIterator. There is a stack of messages, this method returns the
   * field descriptor of this node, and the message descriptor of this node if
   * the node is a message, or the descriptor of the parent
   */

  void get_lowest_msg_and_field_desc(const ProtobufCMessageDescriptor*& mdesc,
                                     const ProtobufCFieldDescriptor*& fdesc) const;

 public:
  /*!
   * Accessor methods for private members. 
   */

  /*!
   * @return The root node of the PBCM iterator
   */
  
  const ProtobufCMessage *root() const {
    return root_;
  }

  size_t depth() const {
    return node_state_.size();
  }

  node shallowest() const {
    return node_state_.top();
  }
  
 public:

  // ATTN: Move the move_to_child methods as private once the complete set
  // of functions/classes that use these are determined.
  /*!
   * A private method to allow friends to move the iterator to the first child
   * based on a element tag
   */
  rw_status_t move_to_child(uint32_t tag);


  /*!
   * A private method to allow friends to move the iterator to a child
   * based on a ProtobufCFieldInfo
   */
  rw_status_t move_to_child(const RwPbcmTreeIterator *iter);

  
  /*!
   * A private method to allow friends to move the iterator to a child
   * based on a ProtobufCFieldInfo
   */
  rw_status_t move_to_child(const ProtobufCFieldDescriptor *fdesc,
                            rw_ylib_list_position_e position_type = RW_YLIB_LIST_POSITION_FIRST,                            
                            unsigned int position = 0);


  /*!
   * Build a PBCM Tree iterator with no args
   */
  RwPbcmTreeIterator() {}
  
  /*!
   * Build a PBCM Tree iterator given a ProtobufCMessage.
   * @param [in] root The protobuf message that becomes the root of the tree 
   */
  RwPbcmTreeIterator(ProtobufCMessage *root)
      : root_(root) {}
  
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
   * Move the iterator to its next list entry, if the move is possible
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_next_list_entry();

  /*!
   * Move the iterator up to its current parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();

  
  /*!
   * Find the SchemaNode associated with the current position when provided with
   * the schema node of the parent
   */
  
  template <class SchemaNode>
  SchemaNode *get_schema_node ( SchemaNode *parent)
  {
    RW_ASSERT(parent);

    ProtobufCMessage *msg = 0;
    unsigned int field_id = 0, index = 0;
    
    std::tie(msg, field_id, index) = node_state_.top();
    RW_ASSERT(msg->descriptor->n_fields > field_id);

    const ProtobufCFieldDescriptor *fld =
        &msg->descriptor->fields[field_id];
    return parent->search_child_by_mangled_name(fld->name);
  }
 

  /*!
   * Find the yang node name 
   */
  
  const char *get_yang_node_name () const;

  /*!
   * Find the yang namespace
   */

  const char *get_yang_ns() const;

  /*!
   * The value held in this iterator. 
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;

  /*!
   * A  method for getting the protobuf message value
   */
  rw_status_t get_value (ProtobufCMessage*& value) const;

  /*!
   * Is this node leafy?
   */
  bool is_leafy() const;

  template<class RwTreeIteratorSrc, class RwTreeIteratorDest>
  friend class Merge;
  
  friend rw_status_t get_string_value(const RwPbcmTreeIterator *iter,
                                      std::string& val);
  template <class RwTreeIterator>  
  friend rw_tree_walker_status_t build_iterator_child (RwPbcmTreeIterator *iter,
                                                       RwTreeIterator *child);

  template <class FoundAction, class NotFoundAction>
  friend rw_tree_walker_status_t find_child_iter (RwPbcmTreeIterator *parent,
                                                  RwSchemaTreeIterator *key_spec,
                                                  bool all_leafs,
                                                  bool all_keys,
                                                  FoundAction& found,
                                                  NotFoundAction& not_found);

  friend class RwKsPbcmIterator;
  friend class RwPbcmDomIterator;
};

inline bool operator == (const RwPbcmTreeIterator& lhs,
                         const RwPbcmTreeIterator& rhs)
{
  
  if (lhs.depth()) {
    if (!rhs.depth()) {
      return false;
    }

    ProtobufCMessage *m_1, *m_2;
    unsigned int index_1, index_2, feild_1, feild_2;

    std::tie(m_1, feild_1, index_1) = lhs.shallowest();
    std::tie(m_2, feild_2, index_2) = rhs.shallowest();

    return ((m_1 == m_2) && (feild_1 == feild_2) && (index_1 == index_2));
  }

  if (rhs.depth()) {
    return false;
  }
  
  return (lhs.root() == rhs.root());
}

inline bool operator != (const RwPbcmTreeIterator& lhs,
                         const RwPbcmTreeIterator& rhs)
{
  return !(lhs == rhs);
}


/*!
 * @page RwXMLTreeIterator XML Tree Iterator
 * The XML Tree iterator iterates over a RW XML Node
 */


class RwXMLTreeIterator {
 private:

  XMLNode *ptr_;


  /*!
   * A private method for friends to get to the data more directly   
   */
  rw_status_t get_value (XMLNode*& value) const;

  /*!
   * A private method to allow friends to move the iterator to a child
   * based on a ProtobufCFieldInfo
   */
  rw_status_t move_to_child(XMLNode *child);
  
 public:

  /*!
   * Accessor functions
   *
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations
   */

  const XMLNode *ptr() const {
    return ptr_;
  }

  /*!
   * Build a default XML iterator that has no data
   */
  RwXMLTreeIterator() {}
  
  /*!
   * Build a XML tree iterator from an XML Node
   */
  
  RwXMLTreeIterator(XMLNode *node)
      :ptr_ (node) {}

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
   * Move the iterator up to its current parent.
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
    std::string name = ptr_->get_local_name();
    std::string ns = ptr_->get_name_space();
        
    return parent->search_child(name.c_str(), ns.c_str());
  }
 


  /*!
   * value
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;
  
  //template <class RwTreeIterator>  
  friend rw_tree_walker_status_t build_and_move_iterator_child_value (RwXMLTreeIterator *iter,
                                                                      RwPbcmTreeIterator *child);

  friend rw_tree_walker_status_t build_and_move_iterator_child_value (RwXMLTreeIterator *iter,
                                                                      RwXMLTreeIterator *child);

};

inline bool operator == (const RwXMLTreeIterator& lhs,
                         const RwXMLTreeIterator& rhs)
{
  return (lhs.ptr() == rhs.ptr());
}

inline bool operator != (const RwXMLTreeIterator& lhs,
                         const RwXMLTreeIterator& rhs)
{
  return !(lhs == rhs);
}


template <class RwTreeIterator>
rw_tree_walker_status_t build_iterator_child (RwSchemaTreeIterator *parent_iter,
                                              RwTreeIterator *child_iter);

/*!
 * @page RwSchemaTreeIterator RwSchemaTreeIterator
 * The keyspec object models a path in a protobuf tree. Modelling the path
 * itself as a tree helps in making the algorithms more generic
 */
class RwSchemaTreeIterator {
private:

  /*!
   * The keyspec over which the iterator is defined
   */
  rw_keyspec_path_t *key_ = 0;

  /*!
   * Depth of keyspec. cached to avoid recomputation
   */

  size_t      length_ = 0;
  
  /*!
   * Current depth in the iterator
   */
  size_t      depth_ = 0;

  /*!
   * Tag of the key at current depth. This does not correspond
   * directly to the KeyXX value, and no code should assume that
   */
  size_t      index_ = 0;

  /*!
   * The path at current depth. Null at root
   */
  rw_keyspec_entry_t *path_ = 0;

   /*!
   * Private convenience method to get to the y2pb messgae descriptor.
   */
  const rw_yang_pb_msgdesc_t * get_y2pb_mdesc() const;


public:

  /*!
   * Accessor functions
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations

   */

  const rw_keyspec_path_t *key() const {
    return key_;
  }

  size_t depth() const {
    return depth_;
  }

  size_t index() const {
    return index_;
  }

  /*!
   * Build a default Schema tree iterator
   */
  RwSchemaTreeIterator() {} ;
  
  /*!
   * Build a Schema tree iterator from an Schema Node
   */
  
  RwSchemaTreeIterator(rw_keyspec_path_t *key);

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
   * Move the iterator up to its current parent.
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
   * Private method for friends to move to a specfied child
   */
  rw_status_t move_to_child (const ProtobufCFieldDescriptor *fdesc);

  /*!
   * Are there any more paths further down?
   */
  bool non_leaf_children() const {
    return !(depth_ == length_);
  }
  
  /*!
   * Is the iterator at a leafy location?
   */
  bool is_leafy() const {
    return !(index_ == 0);
  }

  
  rw_status_t get_value(rw_keyspec_entry_t*& path_) const;
  

  /*!
   * Find the SchemaNode associated with the current position when provided with
   * the schema node of the parent
   */

  
  template <class SchemaNode>
      SchemaNode *get_schema_node (SchemaNode *parent)
  {
    RW_ASSERT(parent);
    
    if (index_) {
      return parent->get_key_at_index(index_);
    }
    auto path = rw_keyspec_path_get_path_entry (key_, depth_);
    RW_ASSERT(path);
    auto elem_id = rw_keyspec_entry_get_schema_id(path);
    return parent->search_child(elem_id.ns, elem_id.tag);
  }
  
  /*!
   * value
   */
  rw_status_t get_value(rw_ylib_data_t* data) const;

  template <class RwTreeIterator>
  friend rw_tree_walker_status_t build_iterator_child (RwSchemaTreeIterator *parent_iter,
                                                    RwTreeIterator *child_iter);

  template <class FoundAction, class NotFoundAction>
  friend rw_tree_walker_status_t find_child_iter (RwPbcmTreeIterator *parent,
                                                  RwSchemaTreeIterator *key_spec,
                                                  bool all_leafs,
                                                  bool all_keys,
                                                  FoundAction& found,
                                                  NotFoundAction& not_found);

  friend class RwKsPbcmIterator;
};

inline bool operator == (const RwSchemaTreeIterator& lhs,
                         const RwSchemaTreeIterator& rhs)
{
  if (!lhs.key()) {
    return (!rhs.key());
  }

  if (!rhs.key()) {
    return false;
  }
  
  return ((lhs.key() == rhs.key()) &&
          (lhs.depth() == rhs.depth()) &&
          (lhs.index() == rhs.index()));
  
}

inline bool operator != (const RwSchemaTreeIterator& lhs,
                         const RwSchemaTreeIterator& rhs)
{
  return !(lhs == rhs);
}




/*!
 * YangData class defines a combination of schema and a data iterator.
 * 
 */
template <class IterType ,class SchemaType>
class YangData
{

  typedef IterType     DataIterator;
  typedef SchemaType   SchemaNode;
  
 private:
  /*!
   * The iterator.
   */

  DataIterator        iter_;

  /*!
   * Schema nodes are often static. 
   */
  SchemaNode    *schema_ = 0;

 public:

  /*!
   * Build a default YangData Node that will not really match anything
   */
  YangData() {}

  /*!
   *  constructor
   * @param [in] iter The data iterator for this object
   * @param [in] schema Schema node
   */

  YangData(DataIterator iter,
           SchemaNode   *schema) 
      :iter_ (iter), schema_ (schema) {
  }

  
  ~YangData(){}

  /*!
   * Accessor functions
   *
   * Accessor methods should NOT be used  outside of the iterator library and
   * test functions.
   * The types returned, the syntax and semantics of these methods can
   * change without notice, and backwards compatiability considerations
   */
  const DataIterator* iter() const {
    return &iter_;
  }

  SchemaNode* schema() const {
    return schema_;
  }
  
  /*!
   * Move to the first child of this node, if there is one.
   * @return RW_STATUS_SUCCESS if a child node exists,
   *         RW_STATUS_FAILURE otherwise
   */
  
  rw_status_t move_to_first_child()
  {
    
    rw_status_t rs = iter_.move_to_first_child();
    if (RW_STATUS_SUCCESS != rs) {
      return rs;
    }

    SchemaNode *schema = iter_.get_schema_node (schema_);
    RW_ASSERT(schema);
    schema_ = schema;
    return RW_STATUS_SUCCESS;
  }

  /*!
   * Move to a child with the same characteristics as the schema node. The
   * namespace and tag name of the schema node is checked to find the child.
   *
   *  @param[in] in The schema node, a correpsonding child to which is requested.
   *
   *  @return RW_STATUS_SUCCESS, if a child is found,
   *          RW_STATUS_FAILURE otherwise
   */
  rw_status_t move_to_child( SchemaNode *in)
  {
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }

  /*!
   * Move to a child with the same characteristics as the argument.
   * . For a leaf, move to a child with the the same namespace and tag name. If
   *   a value is present for the argument, use that in matching.
   
   * . For a leaf list, move to the first child with the the same namespace and
   *   tag name. If a value is present for the argument, use that in matching.
   *   
   * . For a container, move to a child with the same namespace
   *     and tag name
   *
   * . For a list, move to a child with all keys equal to the ones
   *    present in the argument. The argument might not have all the keys specified,
   *    in which case the first node that matches is where the iterator moves to.
   *
   *  @param[in] in The YangData, a correpsonding node to which is requested.
   *
   *  @return RW_STATUS_SUCCESS, if a child is found,
   *          RW_STATUS_FAILURE otherwise
   */
  rw_status_t move_to_child(const YangData *in)
  {
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }


  /*!
   * Move to the next sibling of this node, if there is one.
   * @return RW_STATUS_SUCCESS if a sibling node exists,
   *         RW_STATUS_FAILURE otherwise
   */

  rw_status_t move_to_next_sibling()
  {
    SchemaNode *sn = schema_->get_parent();
    if (nullptr == sn) {
      return RW_STATUS_FAILURE;
    }

    rw_status_t rs = iter_.move_to_next_sibling();
    if (RW_STATUS_SUCCESS != rs) {
      return rs;
    }

    sn = iter_.get_schema_node(sn);
    RW_ASSERT(sn);

    schema_ = sn;
    return RW_STATUS_SUCCESS;
  }
    

  /*!
   * Move to a sibling with the same characteristics as the argument.
   * @seealso move_to_child (const YangData *in)
   *
   *  @param[in] in The YangData, a correpsonding sibling to which is requested.
   *
   *  @return RW_STATUS_SUCCESS, if a sibling is found,
   *          RW_STATUS_FAILURE otherwise
   */
  rw_status_t move_to_sibling(const YangData *in)
  {
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }


  /*!
   * Move to a sibling with the same characteristics as the schema node. The
   * namespace and tag name of the schema node is checked to find the child.
   * For a listy node, the fist occurance will be returned. This method can be
   * used to iterate over a list, each subsequent call will move to the next
   * list element.
   *
   *  @param[in] in The schema node, a correpsonding child to which is requested.
   *
   *  @return RW_STATUS_SUCCESS, if a child is found,
   *          RW_STATUS_FAILURE otherwise
   */
  rw_status_t move_to_sibling(SchemaNode *in)
  {
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }


  /*!
   * Move to the parent of this node, if there is one.
   * @return RW_STATUS_SUCCESS if this node is not root
   *         RW_STATUS_FAILURE otherwise
   */

  rw_status_t move_to_parent()
  {
    SchemaNode *sn = schema_->get_parent();
    if (nullptr == sn) {
      return RW_STATUS_FAILURE;
    }

    rw_status_t rs = iter_.move_to_parent();
    
    if (RW_STATUS_SUCCESS != rs) {
      return rs;
    }

    schema_ = sn;
    return RW_STATUS_SUCCESS;
  }
};


template <class SourceNodeIterator, class TargetNode>
rw_tree_walker_status_t
    walk_iterator_tree_private (SourceNodeIterator *source, TargetNode *target, bool is_root=false)
{
  rw_tree_walker_status_t rt = RW_TREE_WALKER_SUCCESS;
  if (!is_root) {
    rt = target->handle_child (source);    
  }

  switch (rt) {    
    case RW_TREE_WALKER_SKIP_SUBTREE:
    case RW_TREE_WALKER_FAILURE:
    case RW_TREE_WALKER_STOP:
      return rt;

    case RW_TREE_WALKER_SUCCESS:
    case RW_TREE_WALKER_SKIP_SIBLINGS:
      break;

    default:
      RW_ASSERT_NOT_REACHED();      
  }

  rw_status_t rs = source->move_to_first_child();
  bool at_child = (RW_STATUS_SUCCESS == rs);
  
  while (RW_STATUS_SUCCESS == rs) {
    rt = walk_iterator_tree_private (source, target);
    
    // If failure or stop was issued, it was done by the target handling.
    // Assume it has adjusted its state to what was needed. Nothing much
    // can be done here.
    switch (rt) {
      case RW_TREE_WALKER_SKIP_SUBTREE:
      case RW_TREE_WALKER_SUCCESS:
        break;
      default:
        // Other cases
        return rt;
    }
    
    // Target points to an iterator. The call to handle_child on target would
    // move it to its child, if the recursive call was successful. Move back up
    // in this case
    if (RW_TREE_WALKER_SKIP_SUBTREE != rt) {
      rs = target->move_to_parent();
      RW_ASSERT(RW_STATUS_SUCCESS == rs);
    }

    if (RW_TREE_WALKER_SKIP_SIBLINGS != rt) {
      rt = RW_TREE_WALKER_SUCCESS; 
      rs = source->move_to_next_sibling();
    } else {
      rs = RW_STATUS_FAILURE;
    }
  }

  // If the move to first child was successful, move to the parent now
  if (at_child) {
    rs = source->move_to_parent();

    RW_ASSERT(RW_STATUS_SUCCESS == rs);
  }

  return RW_TREE_WALKER_SUCCESS;
}
    
/*!
 * Algorithm to walk a tree node iterator, breadth first
 *
 * @return RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise
 */
template <class SourceNode, class TargetNode>
rw_status_t walk_iterator_tree (SourceNode *source, TargetNode *target)
{
  rw_tree_walker_status_t rt = walk_iterator_tree_private (source, target, true);

  switch (rt) {
    case RW_TREE_WALKER_SUCCESS:
    case RW_TREE_WALKER_SKIP_SUBTREE:
    case RW_TREE_WALKER_STOP:
      return RW_STATUS_SUCCESS;

    case RW_TREE_WALKER_FAILURE:
      return RW_STATUS_FAILURE;
    default:
      RW_CRASH();
    
  }
  RW_ASSERT_NOT_REACHED();
}

rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                           RwPbcmTreeIterator* child_iter);

rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                           RwSchemaTreeIterator* child_iter);

// Copiers for PBCM iterator

rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                             RwPbcmTreeIterator* child_iter);

rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                             RwXMLTreeIterator* child_iter);

rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmTreeIterator* parent_iter,
                                                             RwSchemaTreeIterator* child_iter);

// Copiers for XML iterator

rw_tree_walker_status_t build_and_move_iterator_child_value (RwXMLTreeIterator* parent_iter,
                                                             RwPbcmTreeIterator* child_iter);

rw_tree_walker_status_t build_and_move_iterator_child_value (RwXMLTreeIterator* parent_iter,
                                                             RwXMLTreeIterator* child_iter);

// Copiers for Schema Iterators
rw_tree_walker_status_t build_and_move_iterator_child_value (RwSchemaTreeIterator* parent_iter,
                                                             RwSchemaTreeIterator* child_iter);

// Copiers for PBCMDom Iterator
rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                               RwKsPbcmIterator* child_iter);

// Find functions 
rw_tree_walker_status_t find_child (RwPbcmTreeIterator* haystack_iter,
                                    RwPbcmTreeIterator* needle,
                                    RwPbcmTreeIterator* found);

rw_tree_walker_status_t find_child (RwSchemaTreeIterator* key_iter,
                                    RwPbcmTreeIterator* data_iter,
                                    RwSchemaTreeIterator* next_key);

rw_tree_walker_status_t find_child (RwPbcmTreeIterator* haystack_iter,
                                    RwSchemaTreeIterator* needle,
                                    RwPbcmTreeIterator* found);

rw_tree_walker_status_t find_child (RwPbcmDomIterator *haystack_iter,
                                    RwKsPbcmIterator *needle,
                                    RwPbcmDomIterator *found);


rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                             RwKsPbcmIterator* child_iter);



typedef enum {
  RW_PBCM_DOM_FIND = 1,
  RW_PBCM_DOM_FIND_OR_CREATE,
  RW_PBCM_DOM_CREATE
} rw_pbcm_dom_get_e;

/*!
 * @page RwKsPbcmIterator Keyspec and Protobuf Combination Iterator
 *
 * Uses cases in Riftware require a tree to be defined as a keyspec from the
 * root to a node, and a protobuf message that defines the node. The RwKsPbcmIterator
 * models this tree.
 */

// An iterator rooted at the schema root, composed of keyspec and messages.
class RwKsPbcmIterator {
 private:


  /*!
   * The keyspec - a path from schema root to PB Message root
   */

  rw_keyspec_path_t* keyspec_;

  /*!
   * The root of the protobuf message. Should correspond to the lowest
   * level of the keyspec
   */
  ProtobufCMessage*  msg_;

  /*!
   * Iterator into the keyspec part of the iterator
   */

  RwSchemaTreeIterator ks_iter_;

  /*!
   * Iterator into the protobuf message part of the composite object
   */

  RwPbcmTreeIterator  pbcm_iter_;


  typedef enum {
    RW_PB_KS_ITER_AT_ROOT = 1,
    RW_PB_KS_ITER_IN_KS,
    RW_PB_KS_ITER_IN_PB
  } rw_pb_ks_iter_state_e;

  /*!
   * Which iterator gives out values?
   */

  rw_pb_ks_iter_state_e state_ = RW_PB_KS_ITER_AT_ROOT ;

 public:

  RwKsPbcmIterator (rw_keyspec_path_t *ks, ProtobufCMessage *msg)
      :keyspec_ (ks), msg_ (msg), ks_iter_ (ks), pbcm_iter_ (msg) {};
  
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
   * Move the iterator up to its current parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();

  /*!
   * Is the iterator at a leafy point?
   */
  bool is_leafy() const;

  /*!
   * get the value of at the current position
   */
  rw_status_t get_value (rw_ylib_data_t *val) const;
  
  /*!
   * Find the module name of this iterator. The module name is that of the
   * position of the iterator. The most common use case seems to be to get the
   * name of the root node of this iterator. 
   */
  const char* get_module_name() const;

  friend rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                               RwKsPbcmIterator* child_iter);

  friend rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                               RwKsPbcmIterator* child_iter);

  friend rw_tree_walker_status_t find_child (RwPbcmDomIterator *haystack_iter,
                                             RwKsPbcmIterator *needle,
                                             RwPbcmDomIterator *found);

};

class RwPbcmDom;

/*!
 * @page RwPbcmDomIterator Multiple Protobuf for a forest
 *
 * Each protobuf in Riftware represents a branch from the schema root. There is
 * no protobuf that defines the schema root. This iterator uses a DOM that is a list
 * of Protobuf messages that represents subtrees from the schema root.
 */

class RwPbcmDomIterator
{
 private:

  /*!
   * The protobuf based DOM that this iterator works over.
   */
  
  RwPbcmDom  *dom_ = nullptr;

  /*!
   * Name of the module where the iterator is currently located
   */

  const char *module_ = nullptr;

  /*!
   * Move to tree root in the module with the specified name. 
   */

  rw_status_t move_to_child (const char *name,
                             rw_pbcm_dom_get_e type =  RW_PBCM_DOM_FIND_OR_CREATE);

  /*!
   * Move to the PBCMIterator provided
   */

  rw_status_t move_to_child (RwPbcmTreeIterator *iter);



  /*!
   * If the iterator is currently located within a message in a DOM, the
   * iterator within that message
   */
  
  RwPbcmTreeIterator  pbcm_iter_;

 public:
  
  RwPbcmDomIterator(RwPbcmDom *dom = nullptr)
      :dom_ (dom) {};

  /*!
   * Move to the Dom Iterator provided
   */
  rw_status_t move_to_child (RwPbcmDomIterator *iter);

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
   * Move the iterator up to its current parent.
   * @return RW_STATUS_SUCCESS if the move is possible, RW_STATUS_FAILURE
   * otherwise
   */

  rw_status_t move_to_parent();
  
  friend rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                               RwKsPbcmIterator* child_iter);

  friend rw_tree_walker_status_t set_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                                      RwKsPbcmIterator* child_iter);

  friend rw_tree_walker_status_t find_child (RwPbcmDomIterator *haystack_iter,
                                             RwKsPbcmIterator *needle,
                                             RwPbcmDomIterator *found);


};

typedef enum {
  RW_PBCM_DOM_TYPE_INVALID, 
  RW_PBCM_DOM_TYPE_DATA  = 0x102030,
  RW_PBCM_DOM_TYPE_RPC_IN,
  RW_PBCM_DOM_TYPE_RPC_OUT,
  RW_PBCM_DOM_TYPE_NOTIFICATION
} rw_pbcm_dom_type_e;

class RwPbcmDom
{
 private:
  
  /*!
   * The ypbc schema that this DOM is based on
   */
  
  const rw_yang_pb_schema_t* schema_ = nullptr;

  /*!
   * The type of messages that comprise this DOM. Only certain branches of a
   * YANG model can co-exist in DOM, and these have to be of the same type.
   */
  rw_pbcm_dom_type_e type_ = RW_PBCM_DOM_TYPE_INVALID;
  
  /*!
   * A forest of protobuf messages, each of which are rooted at a module root.
   * The easiest way to access them is using a map indexed by module name
   */

  // ATTN: The ordering is used so that the next_sibling function will return in
  // some predictable order. Not sure if this is necessary?
  typedef std::map <std::string, ProtobufCMessage *> pb_forest_t;


  pb_forest_t trees_;

 public:
  RwPbcmDom (const rw_yang_pb_schema_t* schema, rw_pbcm_dom_type_e type)
      :schema_ (schema), type_ (type) {}
  
   rw_status_t get_module_tree(const char *name,
                               rw_pbcm_dom_get_e type,
                               ProtobufCMessage *& tree);
  
 private:
  rw_tree_walker_status_t build_and_move_iterator_child_value (RwPbcmDomIterator* parent_iter,
                                                               RwKsPbcmIterator* child_iter);

  friend class RwPbcmDomIterator;
};

}



/** @} */
#endif
#endif
