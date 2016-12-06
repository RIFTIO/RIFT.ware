
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
 * @file rw_xml.hpp
 * @author Vinod Kamalaraj
 * @date 2014/04/09
 * @brief A XML library to use with Riftware
 */

#ifndef RW_XML_HPP_
#define RW_XML_HPP_

#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include <rwmemlog.h>
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

#if __cplusplus
#include "rw_app_data.hpp"
#include "util.hpp"
#include <string>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <stack>
#include <memory>
#endif


// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

__BEGIN_DECLS

typedef enum {
  RW_XML_LOG_LEVEL_DEBUG = 31012015,
  RW_XML_LOG_LEVEL_INFO,
  RW_XML_LOG_LEVEL_ERROR
} rw_xml_log_level_e;

typedef rw_xml_log_level_e rw_traverser_log_level_e;

typedef void (*rw_xml_mgr_log_cb)(void *user_data, rw_xml_log_level_e level, const char *fn, const char *log_msg);
typedef rw_xml_mgr_log_cb rw_log_cb;

#define RW_XML_MGR_LOG(xml_mgr, level, fn, log_msg) \
  if ((xml_mgr).log_cb_) {                          \
    ((xml_mgr).log_cb_)(((xml_mgr).log_cb_user_data_), level, fn, log_msg.str().c_str()); \
  }

#define RW_TRAVERSER_LOG(traverser, level, fn, log_msg) \
    if ((traverser).log_cb_) {                          \
      ((traverser).log_cb_)(((traverser).log_cb_user_data_), level, fn, log_msg.c_str()); \
    }

#define RW_XML_NODE_LOG_STRING(node, level, function, log_str) \
  {                                                            \
    std::stringstream ss;                                               \
    ss<<"Node - ns("<<node->get_name_space()<<"), tag("<<node->get_local_name()<<") " << log_str; \
    if (level == RW_XML_LOG_LEVEL_ERROR) { \
      RWMEMLOG ((node)->get_document().get_memlog_ptr(), RWMEMLOG_ALWAYS, "XMLNode Error",\
                RWMEMLOG_ARG_STRNCPY(80, ss.str().c_str()) ); \
      (node)->get_document().get_error_handler().handle_app_errors(ss.str().c_str()); \
    } \
    RW_XML_MGR_LOG (((node)->get_document().get_manager()), level, function, ss); \
  }


#define RW_XML_NODE_DEBUG(node, log_msg)                                \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_NODE_INFO(node, log_msg)                                 \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_INFO, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_NODE_ERROR(node, log_msg)                                \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_MGR_LOG_STRING(xml_mgr, level, fn, log_msg) \
  if ((xml_mgr).log_cb_) {                          \
    ((xml_mgr).log_cb_)(((xml_mgr).log_cb_user_data_),level, fn, log_msg); \
  }

#define RW_XML_NODE_DEBUG_STRING(node, log_msg)                                \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg)

#define RW_XML_NODE_INFO_STRING(node, log_msg)                                 \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_INFO, __FUNCTION__, log_msg)

#define RW_XML_NODE_ERROR_STRING(node, log_msg)                                \
  RW_XML_NODE_LOG_STRING(node, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg)


#define RW_XML_DOC_DEBUG(doc, log_msg)                                \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_DOC_INFO(doc, log_msg)                                 \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_INFO, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_DOC_ERROR(doc, log_msg)                                \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg.str().c_str())

#define RW_XML_DOC_DEBUG_STRING(doc, log_msg)                                \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg)

#define RW_XML_DOC_INFO_STRING(doc, log_msg)                                 \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_INFO, __FUNCTION__, log_msg)

#define RW_XML_DOC_ERROR_STRING(doc, log_msg)                                \
  RW_XML_DOC_LOG_STRING(doc, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg)


#define RW_XML_DOC_LOG_STRING(doc, level, function, log_str) \
  {                                                            \
    if (level == RW_XML_LOG_LEVEL_ERROR) { \
      RWMEMLOG ((doc)->get_memlog_ptr(), RWMEMLOG_ALWAYS, "XMLDOM Error",\
                RWMEMLOG_ARG_STRNCPY(80, log_str) ); \
      (doc)->get_error_handler().handle_app_errors(log_str); \
    }\
    RW_XML_MGR_LOG_STRING (((doc)->get_manager()), level, function, log_str); \
  }

#define RW_XML_MGR_LOG_DEBUG(log_msg) \
  RW_XML_MGR_LOG(*this,RW_XML_LOG_LEVEL_DEBUG,__FUNCTION__, log_msg)

#define RW_XML_MGR_LOG_ERROR(log_msg) \
  RWMEMLOG (memlog_buf_, RWMEMLOG_ALWAYS, "XMLMgr Error",\
            RWMEMLOG_ARG_STRNCPY(80, log_msg.str().c_str()) ); \
  RW_XML_MGR_LOG(*this,RW_XML_LOG_LEVEL_ERROR,__FUNCTION__, log_msg)

#define RW_XML_MGR_LOG_INFO(log_msg) \
  RW_XML_MGR_LOG(*this,RW_XML_LOG_LEVEL_INFO,__FUNCTION__, log_msg)

#define RW_TRAVERSER_LOG_DEBUG(log_msg) \
  RW_TRAVERSER_LOG(*this, RW_XML_LOG_LEVEL_DEBUG, __FUNCTION__, log_msg)

#define RW_TRAVERSER_LOG_INFO(log_msg) \
  RW_TRAVERSER_LOG(*this, RW_XML_LOG_LEVEL_INFO, __FUNCTION__, log_msg)

#define RW_TRAVERSER_LOG_ERROR(log_msg) \
  RWMEMLOG (memlog_buf_, RWMEMLOG_ALWAYS, "Traverser Error", \
            RWMEMLOG_ARG_STRNCPY(80, log_msg.c_str()) ); \
  RW_TRAVERSER_LOG(*this, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg)

#define RW_BUILDER_LOG_DEBUG(log_msg) \
  RW_TRAVERSER_LOG_DEBUG(log_msg)

#define RW_BUILDER_LOG_INFO(log_msg) \
  RW_TRAVERSER_LOG_INFO(log_msg)

#define RW_BUILDER_LOG_ERROR(log_msg) \
  RWMEMLOG (memlog_buf_, RWMEMLOG_ALWAYS, "Builder Error", \
            RWMEMLOG_ARG_STRNCPY(80, log_msg.c_str()) ); \
  RW_TRAVERSER_LOG(*this, RW_XML_LOG_LEVEL_ERROR, __FUNCTION__, log_msg)

/* Begining of defintitions from the Yang based XML Node */

/**
 * @defgroup RW-XML-LIBRARY Yang and PB aware XML Library
 * The Riftware XML library provides a XML library that is YANG aware,
 * and protobuf aware. The RW-XML library is used extensively at run-time,
 * for conversion between protobuf structures and XML, and to create XML
 * for CLI nodes from strings.
 * @{
 */

/**
 * The status of NETCONF operations on YANG DOM nodes.
 */
typedef enum rw_netconf_op_status_e {
  RW_YANG_NETCONF_OP_STATUS_NULL = 0,
  RW_YANG_NETCONF_OP_STATUS_base = 0x15081947,
  RW_YANG_NETCONF_OP_STATUS_OK,
  RW_YANG_NETCONF_OP_STATUS_IN_USE,
  RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE,
  RW_YANG_NETCONF_OP_STATUS_TOO_BIG,
  RW_YANG_NETCONF_OP_STATUS_MISSING_ATTRIBUTE,
  RW_YANG_NETCONF_OP_STATUS_BAD_ATTRIBUTE,
  RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ATTRIBUTE,
  RW_YANG_NETCONF_OP_STATUS_MISSING_ELEMENT,
  RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
  RW_YANG_NETCONF_OP_STATUS_UNKNOWN_ELEMENT,
  RW_YANG_NETCONF_OP_STATUS_UNKNOWN_NAMESPACE,
  RW_YANG_NETCONF_OP_STATUS_ACCESS_DENIED,
  RW_YANG_NETCONF_OP_STATUS_LOCK_DENIED,
  RW_YANG_NETCONF_OP_STATUS_RESOURCE_DENIED,
  RW_YANG_NETCONF_OP_STATUS_ROLLBACK_FAILED,
  RW_YANG_NETCONF_OP_STATUS_DATA_EXISTS,
  RW_YANG_NETCONF_OP_STATUS_DATA_MISSING,
  RW_YANG_NETCONF_OP_STATUS_OPERATION_NOT_SUPPORTED,
  RW_YANG_NETCONF_OP_STATUS_OPERATION_FAILED,
  RW_YANG_NETCONF_OP_STATUS_MALFORMED_MESSAGE,
  RW_YANG_NETCONF_OP_STATUS_FAILED,
  RW_YANG_NETCONF_OP_STATUS_RESPONSE_DELAYED,
  RW_YANG_NETCONF_OP_STATUS_end,
  RW_YANG_NETCONF_OP_STATUS_FIRST = RW_YANG_NETCONF_OP_STATUS_base+1,
  RW_YANG_NETCONF_OP_STATUS_LAST = RW_YANG_NETCONF_OP_STATUS_end-1,
  RW_YANG_NETCONF_OP_STATUS_COUNT = RW_YANG_NETCONF_OP_STATUS_end-RW_YANG_NETCONF_OP_STATUS_base-1,
} rw_yang_netconf_op_status_t;

static inline bool_t rw_yang_netconf_op_status_is_good(rw_yang_netconf_op_status_t v)
{
  return    (int)v >= (int)RW_YANG_NETCONF_OP_STATUS_FIRST
      && (int)v <= (int)RW_YANG_NETCONF_OP_STATUS_LAST;
}

/**
 *  * Convert a RW netconf status to a plain RW status.
 *   * @return The corresponding rw_status_t.
 *    */
rw_status_t rw_yang_netconf_op_status_to_rw_status(
  rw_yang_netconf_op_status_t ncs ///< [in] The status to convert
);

/**
 *  The NETCONF operations on YANG DOM nodes.
 */
typedef enum rw_netconf_operation_e {
  RW_YANG_NETCONF_OPERATION_NULL = 0,
  RW_YANG_NETCONF_OPERATION_base = 0x26011950,
  RW_YANG_NETCONF_OPERATION_MERGE,
  RW_YANG_NETCONF_OPERATION_REPLACE,
  RW_YANG_NETCONF_OPERATION_DELETE,
  RW_YANG_NETCONF_OPERATION_REMOVE,
  RW_YANG_NETCONF_OPERATION_CREATE,
  RW_YANG_NETCONF_OPERATION_GET_CONFIG,
  RW_YANG_NETCONF_OPERATION_GET,
  RW_YANG_NETCONF_OPERATION_EXECUTE,
  RW_YANG_NETCONF_OPERATION_end,

  RW_YANG_NETCONF_OPERATION_FIRST = RW_YANG_NETCONF_OPERATION_base+1,
  RW_YANG_NETCONF_OPERATION_LAST = RW_YANG_NETCONF_OPERATION_end-1,
  RW_YANG_NETCONF_OPERATION_COUNT = RW_YANG_NETCONF_OPERATION_end-RW_YANG_NETCONF_OPERATION_base-1,

} rw_yang_netconf_operation_t;

static inline bool_t rw_yang_netconf_operation_is_good(rw_yang_netconf_operation_t v)
{
  return    (int)v >= (int)RW_YANG_NETCONF_OPERATION_FIRST
      && (int)v <= (int)RW_YANG_NETCONF_OPERATION_LAST;
}


typedef enum rw_yang_qaulifier_type_e {
  RW_YANG_QUALIFIER_TYPE_base = 0x12032607,
  RW_YANG_QUALIFIER_TYPE_LIST,
  RW_YANG_QUALIFIER_TYPE_LEAF_LIST,
  RW_YANG_QUALIFIER_TYPE_end,

  RW_YANG_QUALIFIER_TYPE_FIRST = RW_YANG_QUALIFIER_TYPE_base+1,
  RW_YANG_QUALIFIER_TYPE_LAST = RW_YANG_QUALIFIER_TYPE_end-1

} rw_yang_qualifier_type_t;

typedef enum rw_netconf_subtree_filter_node_type_e {
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_NULL = 0,
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_base = 0x30245678,
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_CONTAINMENT,
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_SELECTION,
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_ATTRIB_MATCH,
  RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_end,

  RW_YANG_NETCONF_SUBTREE_FILTER_NODE_TYPE_FIRST = RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_base+1,
  RW_YANG_NETCONF_SUBTREE_FILTER_NODE_TYPE_LAST = RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_end - 1,
  RW_YANG_NETCONF_SUBTREE_FILTER_NODE_TYPE_COUNT = RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_end - RW_NETCONF_SUBTREE_FILTER_NODE_TYPE_base - 1

} rw_netconf_subtree_filter_node_type_t;

static inline bool_t rw_yang_qualifier_type_is_good (rw_yang_qualifier_type_t v)
{
  return ((int)v >= (int)RW_YANG_QUALIFIER_TYPE_FIRST) &&
      (int) v <= RW_YANG_QUALIFIER_TYPE_LAST;
}

/* End of defintitions from the Yang based XML Node */

#ifdef __GI_SCANNER__
/// C adapter struct for C++ class XMLNode
typedef struct rw_xml_node_s rw_xml_node_t;

/// C adapter struct for C++ class XMLNodeList
typedef struct rw_xml_node_list_s rw_xml_node_list_t;

/// C adapter struct for C++ class XMLDocument
typedef struct rw_xml_document_s rw_xml_document_t;

/// C adapter struct for C++ class XMLManager
typedef struct rw_xml_manager_s rw_xml_manager_t;

/// C adapter struct for C++ class XMLAttribute
typedef struct rw_xml_attribute_s rw_xml_attribute_t;

/// C adapter struct for C++ class XMLAttributeList
typedef struct rw_xml_attribute_list_s rw_xml_attribute_list_t;
#else /* __GI_SCANNER__ */
/// C adapter struct for C++ class XMLNode
typedef struct rw_xml_node_s {} rw_xml_node_t;

/// C adapter struct for C++ class XMLNodeList
typedef struct rw_xml_node_list_s {} rw_xml_node_list_t;

/// C adapter struct for C++ class XMLDocument
typedef struct rw_xml_document_s {} rw_xml_document_t;

/// C adapter struct for C++ class XMLManager
typedef struct rw_xml_manager_s {} rw_xml_manager_t;

/// C adapter struct for C++ class XMLAttribute
typedef struct rw_xml_attribute_s {} rw_xml_attribute_t;

/// C adapter struct for C++ class XMLAttributeList
typedef struct rw_xml_attribute_list_s {} rw_xml_attribute_list_t;
#endif /* __GI_SCANNER__ */

/// XMLNode pair data structure used by the walker to maintain relation
//  between the tree being walked and the tree being maninpulated by visitor
typedef struct rw_xml_node_pair_s {
  void *first;
  void *second;
} rw_xml_node_pair_t;

__END_DECLS

#if __cplusplus

namespace rw_yang {

class XMLManager;
class XMLDocument;
class XMLNodeList;
class XMLNode;
class XMLAttribute;
class XMLAttributeList;
class XMLNodeIter;
class XMLVisitor;
class XMLErrorHandler;

// Maximum nesting depth supported for an XML document
const int RW_MAX_XML_NODE_DEPTH = 32;

/**
 * Object deleter template for types with a obj_release() member function.
 * This template is designed for use with unique_ptr<> types for
 * automagic memory management in RW code.  The unique_ptr<> defined by
 * this class automatically calls obj_release() when necessary.
 */
template <class pointer_type_t>
struct UniquePtrXMLObjectRelease
{
  void operator()(pointer_type_t* obj) const
  {
    obj->obj_release();
  }
  typedef std::unique_ptr<pointer_type_t,UniquePtrXMLObjectRelease> uptr_t;
};

/// XML Attribute
/**
 * Refers to a single Attribute in a XMLNode.  Supports DOM functions that
 * are required for Riftware.
 *
 * Abstract base class.
 *
 * ATTN: Attributes may be defined by yang extensions in a few rare
 * cases.  There should be APIs here for mapping to those extensions.
 */
class XMLAttribute
: public rw_xml_attribute_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLAttribute>::uptr_t uptr_t;

 public:
  XMLAttribute()
  {}

  /**
   * Release the attribute.  Use this API to destroy the attr when you are
   * done with it, and it is no longer in the tree.  Do not call this
   * if the attr is in the tree - it must be removed first!
   */
  virtual void obj_release() noexcept = 0;

 protected:
  /// Trivial protected destructor - see XMLAttribute::obj_release().
  virtual ~XMLAttribute()
  {}

  // Cannot copy
  XMLAttribute (const XMLAttribute &) = delete;
  XMLAttribute& operator = (const XMLAttribute&) = delete;

 public:
  /**
   * Get a copy of the attributes value, if any.
   * @return The text value, may be empty string
   */
  virtual std::string get_text_value () = 0;

  /**
   * Returns true if the attribute received its value explicitly in the XML document,
   * or if a value was assigned programatically with the setvalue function.
   * @return The value of the attribute, may be empty string
   */
  virtual bool get_specified () const  = 0;

  /**
   * Get the value of the attribute
   * @return The value of the attribute, may be empty string
   */
  virtual std::string get_value () = 0;

  /**
   * Set the value of the attribute
   * @return Returns void
   */
  virtual void set_value (std::string value) = 0;

  /**
   * Get a copy of the namespace prefix, if any.
   * @return The prefix, may be empty string
   */
  virtual std::string get_prefix() = 0;

  /**
   * Get a copy of the attribute node's value.
   * Does not include prefix.
   * @return The text value, may be empty string
   */
  virtual std::string get_local_name() = 0;

  /**
   * Get a copy of the attribute node's namespace URI.
   * @return The text value, may be empty string
   */
  virtual std::string get_name_space() = 0;

  /**
   * Get a pointer to the owning document.
   * @return The owning document
   */
  virtual XMLDocument& get_document() = 0;

  /**
   * Get a pointer to the document's manager.
   * @return The owning manager
   */
  virtual XMLManager& get_manager() = 0;
};

/**
 * RW XML attribute list, which is the list of attributes in a single  node.
 *
 * ATTN: Need to decide if this is true: The user of a attribute list should
 * not assume that it is either live or stale - depending on the XML
 * implementation, the list of attributes may or may not reflect active
 * changes in the document.
 *
 * The RW XML attribute list is owned by the user.
 *
 * Abstract base class.
 */
class XMLAttributeList
: public rw_xml_attribute_list_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLAttributeList>::uptr_t uptr_t;

 public:
  /// Construct an attribute list.
  XMLAttributeList() {}

  /**
   *  Release the attribute.  Use this API to destroy the attribute
   *  when you aredone with it, and it is no longer in the tree.
   *  Do not call this if the attribute is in the tree -
   *  it must be removed first!  */
  virtual void obj_release() noexcept = 0;

 protected:
  /// Trivial protected destructor
  virtual ~XMLAttributeList() {}

  // Cannot Copy
  XMLAttributeList (const XMLAttributeList &) = delete;
  XMLAttributeList& operator = (const XMLAttributeList &) = delete;

 public:
  /**
   * Get the number of elements in the list.
   * @return The number of nodes.
   */
  virtual size_t length() = 0;

  /**
   * Find the first attribute in the list with a specific local name and
   * namespace.  The local_name does not include the namespace prefix,
   * regardless of whether one existed in the original XML.  This API
   * is ambiguous - multiple attributes may match, in which case only the
   * first matching attribute is returned.
   *
   * @return The first matching attribute, or nullptr if no matches.
   */
  virtual XMLAttribute* find(
    const char* local_name, ///< The local name to search for.
    const char* ns ///< The namespace to search in.
  ) = 0;

  /**
   * Get the attribute at a specific position within the list.
   * @return The attribute is valid position, otherwise nullptr.
   */
  virtual XMLAttribute* at(
    size_t index ///< The node's position
  ) = 0;

  /**
   * Get a pointer to the owning document.
   * @return The owning document
   */
  virtual XMLDocument& get_document() = 0;

  /**
   * Get a pointer to the document's manager.
   * @return The owning manager
   */
  virtual XMLManager& get_manager() = 0;
};


/// XML Node.
/**
 * Refers to a single node in a document.  Supports DOM functions that
 * are required for Riftware.
 *
 * ATTN: Should provide the DOM APIs:
 *   get_attributes()
 *   set_attributes()
 *   insert_before()
 *   replace_child()
 *   normalize()
 *
 * Abstract base class.
 */
class XMLNode
: public rw_xml_node_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLNode>::uptr_t uptr_t;

  typedef std::tuple<YangNode*,std::string> key_value_t;
  typedef std::list <key_value_t> key_list_t;

 public:
  /**
   * Default constructor.
   */
  XMLNode()
  {}

  /**
   * Release the node.  Use this API to destroy the node when you are
   * done with it, and it is no longer in the tree.  Do not call this
   * if the node is in the tree - it must be removed first!
   */
  virtual void obj_release() noexcept = 0;


 protected:

  /// Trivial protected destructor - see XMLNode::obj_release().
  virtual ~XMLNode()
  {}

  // Cannot copy
  XMLNode (const XMLNode &) = delete;
  XMLNode& operator = (const XMLNode&) = delete;


 public:
  // Schema information

  /**
   * Determine if the yang schema node for this XML node is already
   * known.  This is not equivalent to whether or not there is one -
   * the schema node can be both known and nullptr (for example, if the
   * schema node was looked up, but not found).
   *
   * @return true if already known, false if not yet known.
   */
  virtual bool known_yang_node() = 0;

  /**
   * Get the yang meta-data descriptor for this RW XML node, if the RW
   * XML node maps to the yang schema.
   *
   * @return The yang descriptor.  Owned by the yang model.
   */
  virtual YangNode* get_yang_node() = 0;

  /**
   * Set the yang meta-data descriptor for this RW XML node.  The XML
   * node should not already have a yang descriptor, or it should be
   * the same one that is set.
   */
  virtual void set_yang_node(
    /// The yang descriptor.
    YangNode* yang_node
  ) = 0;

 /**
   * Get the yang meta-data re-root descriptor for this RW XML node.
   *
   * @return The yang descriptor.  Owned by the yang model.
   */
  virtual YangNode* get_reroot_yang_node() = 0;


  /**
   * Set the yang meta-data re-root descriptor for this RW XML node.
   * The node should not already have a re-root descriptor, or the
   * re-root descriptor should be the same one that is set.  A re-root
   * descriptor is never determined automatically - it must be set
   * explicitly.
   *
   * The re-root descriptor changes how the immediate descendents of
   * this node will be mapped to the yang schema.  Changing the re-root
   * descriptor has no effect on nodes that have already been mapped to
   * the schema.  There are several use cases for re-rooting:
   *  - anyxml nodes, which are leaf nodes by the parent's node
   *    hiearchy, but which may be their own root nodes, depending on
   *    the contents of the XML.
   *  - netconf protocol, which has an outer wrapper to get to the RPC
   *    command, and the inner contents consisting of the RPC command
   *    data.
   */
  virtual void set_reroot_yang_node(
    YangNode* reroot_yang_node ///< The yang descriptor.
  ) = 0;

  /**
   * Get the yang meta-data descriptor that should be used for further
   * descents in the schema.  Prefers the reroot node, and if one is
   * not set, returns the regular yang node.
   *
   * @return The yang descriptor.  Owned by the yang model.
   */
  virtual YangNode* get_descend_yang_node() = 0;



 public:
  // Node information APIs

  /**
   * Get a copy of the node's value, if any.
   * @return The text value, may be empty string
   */
  virtual std::string get_text_value () = 0;

  /**
   * Set the text value of this node.
   *
   * ATTN: Should have a flavor of this that can set a text value with
   * a namespace, which will automatically prepend the namespace prefix
   * and colon to the string?
   */
  virtual rw_status_t set_text_value(
    const char* text_value ///< The new value
  ) = 0;

  /**
   * Get a copy of the namespace prefix, if any.
   * @return The prefix, may be empty string
   */
  virtual std::string get_prefix() = 0;

  /**
   * Get a copy of the node's element name.  Does not include prefix.
   * @return The text value, may be empty string
   */
  virtual std::string get_local_name() = 0;

  /**
   * Get a copy of the node's namespace URI.
   * @return The text value, may be empty string
   */
  virtual std::string get_name_space() = 0;


  /**
   * Get a prefix string representation of just this node. For lists, this will
   * include the children which are keys, if they are present. This is required
   * for correctly splitting and splicing paths.
   * @return The prefix of the full node
   */
  virtual std::string get_full_prefix_text();

  /**
   * Get a suffix string representation of just this node. For lists, this will
   * include the children which are keys, if they are present. This is required
   * for correctly splitting and splicing paths.
   * @return The suffix of the full tag
   */

  virtual std::string get_full_suffix_text();


  /**
   * Get a pointer to the owning document.
   * @return The owning document
   */
  virtual XMLDocument& get_document() = 0;

  /**
   * Get a pointer to the document's manager.
   * @return The owning manager
   */
  virtual XMLManager& get_manager() = 0;



 public:
  // Tree traversal APIs

  /**
   * Get the parent node, if any.
   *
   * @return The parent node.
   */
  virtual XMLNode* get_parent() = 0;

  /**
   * Get the first child element, if any.
   *
   * @return The first child element.
   */
  virtual XMLNode* get_first_child() = 0;

  /**
   * Get the last child element, if any.
   *
   * @return The last child element.
   */
  virtual XMLNode* get_last_child() = 0;

  /**
   * Get the next sibling element, if any.
   *
   * @return The next sibling element.
   */
  virtual XMLNode* get_next_sibling() = 0;

  /**
   * Get the next sibling - of the same name. When RWXML interacts with CONFD,
   * CONFD calls into rw_xml to find a node with a given key, and its next
   * sibling. In a list of lists, the next sibling could be any child of the
   * parent while what is intended is a sibling of the same local name and
   * namespace. The API is defined generically to suit other possible use cases
   */

  virtual XMLNode* get_next_sibling(
      const std::string& local_name, /**< [in] The name of the sibling node */
      const std::string& ns);        /**< [in] namespace of the sibling node */

  /**
   * Get the previous sibling element, if any.
   *
   * @return The previous sibling element.
   */
  virtual XMLNode* get_previous_sibling() = 0;

  /**
   * Get the previous sibling element of the same name, if any.
   *
   * @return The previous sibling element of the same name.
   */
  virtual XMLNode* get_previous_sibling(
      const std::string& local_name, /**< [in] The name of the sibling node */
      const std::string& ns);        /**< [in] namespace of the sibling node */

  /**
   * Get the first non-text element under this node.
   *
   * @return The element.
   */
  virtual XMLNode* get_first_element() = 0;

  /**
   * Get a node list containing all the children of the node.  The node
   * list is live - changes to nodes in the list will appear in the
   * document, and vice-versa.
   *
   * @return The node list. Caller takes ownership.
   */
  virtual rw_yang::UniquePtrXMLObjectRelease<XMLNodeList>::uptr_t get_children() = 0;

  /**
   * Get an iterator to the first child element.
   *
   * @return The node iterator.
   */
  virtual XMLNodeIter child_begin();

  /**
   * Get an iterator to the end of the child elements.
   *
   * @return The node iterator.
   */
  virtual XMLNodeIter child_end();

 public:
  // XML attribute APIs

  /**
   * Determine if a node supports attributes.
   *
   * @return true if the node supports attributes.
   */
  virtual bool supports_attributes() = 0;

  /**
   * Get a attribute list containing all the attributes of the node.
   * The attribute list is live - changes to attributes in the list
   * will appear in the document, and vice-versa.
   *
   * @return The attribute list.  Caller takes ownership.  Will be null
   *   if the node does not support attributes.
   */
  virtual XMLAttributeList::uptr_t get_attributes() = 0;

  /**
   * Determine if the node has any attributes.
   *
   * @return true if the node has any attributes.  false will be
   *   returned if the node does not have attributes or if the node
   *   does not even support attributes.
   */
  virtual bool has_attributes() = 0;

  /**
   * Determine if the node has an attribute with the given name (and
   * namespace).
   *
   * @return
   * - true if the node has the attribute.
   * - false if the node does not have the attribute, or if the node
   *   does not support attributes.
   */
  virtual bool has_attribute(
    /**
     * [in] The attribute name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* attr_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the attribute must
     * not be associated with any namespace.  May also pass
     * rw_xml_namespace_none to indicate that the attribute must not be
     * associated with any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Retrieve an attribute by name (and namespace).
   *
   * ATTN: ATTN: This shouldn't be returning a unique pointer. Results in
   * premature obj_release as the attribute will be owned by its Element and
   * Element will still exist.
   *
   * @return
   * - A valid pointer if the attribute exists.
   * - nullptr if the node does not have the attribute, or if the node
   *   does not support attributes.
   */
  virtual XMLAttribute::uptr_t get_attribute(
    /**
     * [in] The attribute name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* attr_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the attribute must
     * not be associated with any namespace.  May also pass
     * rw_xml_namespace_none to indicate that the attribute must not be
     * associated with any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Retrieve an attribute by name (and namespace).
   *
   * @return
   * - A valid string if the attribute exists.
   * - empty string if the node does not have the attribute, or if the node
   *   does not support attributes.
   */
  virtual std::string get_attribute_value(
    /**
     * [in] The attribute name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* attr_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the attribute must
     * not be associated with any namespace.  May also pass
     * rw_xml_namespace_none to indicate that the attribute must not be
     * associated with any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Remove an attribute.  Will crash if the attribute is not owned by
   * the node.
   * ATTN: Create an API that does not crash?
   * ATTN: Create an API that just uses strings?
   */
  virtual void remove_attribute(
    /// The attribute to remove.
    XMLAttribute *attribute
  ) = 0;

  /**
   * Set an attribute for the node.  If the attribute already exists in
   * the node, the attribute's value will be replaced by the new value.
   *
   * This function may crash if called on a node type that does not
   * support attributes, or if called with an attribute that is not
   * supported.
   *
   * ATTN: Create a non-virtual no_crash version that checks if
   * attributes are supported first?
   */
  virtual void set_attribute(
    /**
     * [in] The attribute name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* attr_name,

    /**
     * [in] The attribute value, in UTF-8.  The string is raw - any XML
     * "predefined entities" will not be interpretted as the characters
     * they represent, but rather the literal entity string.
     */
    const char* attr_value,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the attribute
     * will not be associated with any namespace.  May also pass
     * rw_xml_namespace_none to indicate that the attribute should not
     * be associated with any namespace.
     */
    const char* ns = nullptr,

    /**
     * [in] The namespace prefix, in UTF-8.  If nullptr, then the
     * attribute will use the specified namespace's default prefix (if
     * known).  May also pass rw_xml_prefix_namespace to use the
     * namespace's default prefix.
     */
    const char* prefix = nullptr
  ) = 0;

 public:
  // Comparison APIs

  // ATTN: is_identical: the same object

  /**
   * Determine if two nodes are equal.  If they are equal, they have
   * the same attributes, and equal children.
   *
   * @return true if the nodes are equal.
   */
  virtual bool is_equal(
    XMLNode* node ///< The node to compare against
  ) = 0;

  /**
   * Validate the node and its descendantagainst the yang schema.
   *
   * @return The status of the validate.  Various errors.
   *   ATTN: it makes most sense to save a list of errors somewhere.
   */
  virtual rw_yang_netconf_op_status_t validate();


  /**
   * Check if the node has any children under it.
   *
   * @return Returns a bool. If 'true', that means
   * the node under observation has no children.
   * If 'false', the node under observation has child node(s)
   * under it.
   */
  virtual bool is_leaf_node();

 public:
  // Search APIs

  /**
   * Find a child with a specific name (and namespace).  This API is
   * ambiguous - multiple nodes may match, in which case only the first
   * matching node is returned.
   *
   * ATTN: Should also have a next function, to continue a search for
   * additional matches.
   *
   * ATTN: Should (also) have a yang-based API for this.
   *
   * @return The first matching node, or nullptr if no matches
   */
  virtual XMLNode* find(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace, or
     * rw_xml_namespace_node to match only the namespace of this node.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Return the count of children matching the given name and namespace.
   *
   * @return the count of the matches
   */
  virtual size_t count(

    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace, or
     * rw_xml_namespace_node to match only the namespace of this node.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Return the collection of children matching the given name and namespace,
   * or return nullptr
   *
   * @return the list of nodes, or nullptr if no match
   */
  virtual std::unique_ptr<std::list<XMLNode*>> find_all(

    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace, or
     * rw_xml_namespace_node to match only the namespace of this node.
     */
    const char* ns = nullptr
  ) = 0;


  /**
   * Find a list of nodes at the path specified by a key spec. The nodes will be
   * at the depth of keyspec level. The path would be as specified in the keyspec,
   * and any key values specified in the keyspec will have matched in the path of
   * the nodes included
   *
   * @return status of the search, RW_YANG_NETCONF_OP_STATUS_OK on success. Success
   * does not imply finding the right node, just that the key spec and the yang model
   * on which the DOM is based are equivalent
   */
  virtual rw_yang_netconf_op_status_t find(
    /**
     * [in] The key spec path of the node to be found
     */
    const rw_keyspec_path_t *keyspec,

    /**
     * [out] The node that was found
     * ATTN: DO NOT PASS OUT parameters by ref.
     */
    std::list<XMLNode*>& node);

  /**
   * Find a child with a specific name (and namespace), holding a
   * specific text value.  This API is ambiguous - multiple nodes may
   * match, in which case only the first matching node is returned.
   *
   * ATTN: Should also have a next function, to continue a search for
   * additional matches.
   *
   * ATTN: Should (also) have a yang-based API for this.
   *
   * @return The first matching node, or nullptr if no matches
   */
  virtual XMLNode* find_value(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The text value, in UTF-8.  The string is raw - any XML
     * "predefined entities" will not be interpretted as the single
     * character the entity represents, but rather the literal
     * characters of the predefined entity string.  In the yang-XML
     * mapping, text values may be preceded by a namespace prefix in
     * order to identify the proper identifier (for enums and
     * identities); this API does not take that into account - the
     * comparison will include the entire string, prefix and all.
     */
    const char* text_value,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace, or
     * rw_xml_namespace_node to match only the namespace of this node.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Find a child with a specific name (and namespace), holding a
   * specific integer value.  The integer value represents an enumeration.
   * The nodes actual text value, if found, would be the text associated
   * with the enumeration
   *
   *
   * @return The first matching node, or nullptr if no matches
   */
  virtual XMLNode* find_enum(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace, or
     * rw_xml_namespace_node to match only the namespace of this node.
     */
    const char* ns,

    /**
     * [in] The integer representation of the enumeration that is to be
     * found.
     */
    uint32_t enum_val

  );

  /**
   * Find a child node in this node, assumed to be a list, that
   * corresponds to the given XML node.  This API is used to lookup a
   * specific list entry, either for modification or to satisfy a
   * search.
   *
   * ATTN: Should have a XMLNode lookup_node overload?
   *
   * @return The status of the search.
   *   - RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE: bad lookup_node
   *   - RW_YANG_NETCONF_OP_STATUS_OK: match found
   *   - RW_YANG_NETCONF_OP_STATUS_FAILED: no match found
   */
  virtual rw_yang_netconf_op_status_t find_list_by_key(
    YangNode* list_ynode, ///< [in] The yang node that defines the list being searched.
    XMLNode* lookup_node, ///< [in] The node to search for.  May contain more than just the keys.
    XMLNode** found_node ///< [out] The node that was found, if any.  Owned by the document.
  );

  /**
   * Find a child node in this node, assumed to be a list, that
   * corresponds to a given path spec.  This API is used to lookup a
   * specific list entry, either for modification or to satisfy a
   * search, for functions that manipulate keyspec
   *
   * ATTN: Should have a XMLNode lookup_node overload?
   *
   * @return The node found
   */
  virtual XMLNode* find_list_by_key(
    YangNode* list_ynode, ///< [in] The yang node that defines the list being searched.
    ProtobufCMessage *path_entry, ///< [in] The path entry for the key
    std::list<XMLNode*> *nodes,   ///< [in-out] adds XMLNodes to this list id is_wildcard = true
    bool is_wildcard = false      ///> [in] true if wilcarded node else false
  );

  /**
   * Find a child node in this node, assumed to be a list, that
   * corresponds to the given Protobuf message.  This API is used to lookup a
   * specific list entry, either for modification or to satisfy a
   * search.
   *
   * ATTN: Should have a XMLNode lookup_node overload?
   *
   * @return The status of the search.
   *   - RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE: bad lookup_node
   *   - RW_YANG_NETCONF_OP_STATUS_OK: match found
   *   - RW_YANG_NETCONF_OP_STATUS_FAILED: no match found
   */
  virtual rw_yang_netconf_op_status_t find_list_by_key_pb_msg(
      YangNode* list_ynode, ///< [in] The yang node that defines the list being searched.
      void* lookup_value, ///< [in] The node to search for.  May contain more than just the keys.
      XMLNode** found_node  ///< [out] The node that was found, if any.  Owned by the document.
    );
  /**
   * Find the first deepest node in a Yang defined XML tree. This might
   * ***** NOT BE THE LONGEST ****** path in the tree. At each node, the first
   * non-leaf child node is chosen. The node returned does NOT have any non-leaf
   * children
   *
   * @return The node found. This function never fails - the node itself is
   * a valid return, if the node does not have any non-leaf children
   */

  virtual XMLNode* find_first_deepest_node();

  /**
   * Find the first deepest node in the XML tree. This might
   * ***** NOT BE THE LONGEST ****** path in the tree. At each node, the first
   * non-leaf child node is chosen. The node returned does NOT have any non-leaf
   * children
   *
   * @return The node found. This function never fails - the node itself is
   * a valid return, if the node does not have any non-leaf children
   */

  virtual XMLNode* find_first_deepest_node_in_xml_tree();

 public:
  // Tree modification

  /**
   * Add a new node to the children list, with the given attributes, as per
   * the Yang RFC XML ordering specification.
   *    List keys should be added at the beginning, ordered as per 'key' stmt
   *    RPC input/output nodes should be added as per Yang ordering
   * If no Yang node is associated with the parent XMLNode, then this method
   * appends the child element.
   *
   * @return The created node.  Document owns the node.
   */
  virtual XMLNode* add_child(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The text value, in UTF-8.  The string is raw - any XML
     * "predefined entities" will not be interpretted as the characters
     * they represent, but rather the literal entity string.
     */
    const char* text_value = nullptr,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the element will
     * use the current node's namespace.  May also pass
     * rw_xml_namespace_node to use the current node's namespace.
     */
    const char* ns = nullptr,

    /**
     * [in] The namespace prefix, in UTF-8.  If nullptr, then the
     * element will use the specified namespace's default prefix (if
     * known).  May also pass rw_xml_prefix_namespace to use the
     * namespace's default prefix.
     */
    const char* prefix = nullptr
  ) = 0;


  /**
   * Add a new node to the children list, with the attributes
   * derived from the specified yang node.  Upon success, the document
   * takes ownership of the node.  Yang-specific overload. Adds the child as per
   * Yang RFC XML node ordering specification.
   *
   * @return The new node.  Node owned by the document.
   */
  virtual XMLNode* add_child(
    YangNode* yang_node, /// [in] The yang schema node
    const char* text_val = nullptr /// [in] The text optional value to assign to the node. May be nullptr.
  ) = 0;

  /**
   * Import a node, possibly from another document, into this document,
   * and append it as a child.  The imported node will be copied - the
   * original will remain undisturbed.
   *
   * ATTN: Currently, the implementations of the nodes must be the
   * same.  The API does not require this, but it is just easier to
   * implement it that way.  Therefore, this document and the imported
   * node are required to share the same XML manager.
   *
   * @return The imported node, owned by the document.
   */
  virtual XMLNode* import_child(
    XMLNode* node, ///< [in] The node to insert
    // ATTN: why is the yang node an argument here?  Shouldn't it be taken from node?
    YangNode* yang_node, /// [in] The yang schema node
    bool deep ///< [in] Deep import - if true, import all the descendents as well
  ) = 0;

  /**
   * Insert a new node as a sibling before the current node
   * Upon success, the document takes ownership of the node.
   *
   * @return The created node.  Document owns the node.
   */
  virtual XMLNode* insert_before(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The text value, in UTF-8.  The string is raw - any XML
     * "predefined entities" will not be interpretted as the characters
     * they represent, but rather the literal entity string.
     */
    const char* text_value = nullptr,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the element will
     * use the current node's namespace.  May also pass
     * rw_xml_namespace_node to use the current node's namespace.
     */
    const char* ns = nullptr,

    /**
     * [in] The namespace prefix, in UTF-8.  If nullptr, then the
     * element will use the specified namespace's default prefix (if
     * known).  May also pass rw_xml_prefix_namespace to use the
     * namespace's default prefix.
     */
    const char* prefix = nullptr
  ) = 0;


  /**
   * Insert a new node as a sibling before the currenr node with the attributes
   * derived from the specified yang node.  Upon success, the document
   * takes ownership of the node.  Yang-specific overload.
   *
   * @return The new node.  Node owned by the document.
   */
  virtual XMLNode* insert_before(
    YangNode* yang_node, /// [in] The yang schema node
    const char* text_val = nullptr /// [in] The text optional value to assign to the node. May be nullptr.
  ) = 0;

  /**
   * Import a node, possibly from another document, into this document,
   * and insert it before as a sibling.  The imported node will be copied - the
   * original will remain undisturbed.
   *
   * ATTN: Currently, the implementations of the nodes must be the
   * same.  The API does not require this, but it is just easier to
   * implement it that way.  Therefore, this document and the imported
   * node are required to share the same XML manager.
   *
   * @return The imported node, owned by the document.
   */
  virtual XMLNode* import_before(
    XMLNode* node, ///< [in] The node to insert
    YangNode* yang_node, /// [in] The yang schema node
    bool deep ///< [in] Deep import - if true, import all the descendents as well
  ) = 0;

  /**
   * Remove the specified child of this node.  The child_node must be a
   * child of this node.  If successful, the child node is removed and freed
   *
   * @return true if the node was successfully removed, false otherwise
   */

  /**
   * Insert a new node as a sibling after the current node
   * Upon success, the document takes ownership of the node.
   *
   * @return The created node.  Document owns the node.
   */
  virtual XMLNode* insert_after(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The text value, in UTF-8.  The string is raw - any XML
     * "predefined entities" will not be interpretted as the characters
     * they represent, but rather the literal entity string.
     */
    const char* text_value = nullptr,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the element will
     * use the current node's namespace.  May also pass
     * rw_xml_namespace_node to use the current node's namespace.
     */
    const char* ns = nullptr,

    /**
     * [in] The namespace prefix, in UTF-8.  If nullptr, then the
     * element will use the specified namespace's default prefix (if
     * known).  May also pass rw_xml_prefix_namespace to use the
     * namespace's default prefix.
     */
    const char* prefix = nullptr
  ) = 0;

  /**
   * Insert a new node as a sibling after the currenr node with the attributes
   * derived from the specified yang node.  Upon success, the document
   * takes ownership of the node.  Yang-specific overload.
   *
   * @return The new node.  Node owned by the document.
   */
  virtual XMLNode* insert_after(
    YangNode* yang_node, /// [in] The yang schema node
    const char* text_val = nullptr /// [in] The text optional value to assign to the node. May be nullptr.
  ) = 0;

  /**
   * Import a node, possibly from another document, into this document,
   * and insert it after as a sibling.  The imported node will be copied - the
   * original will remain undisturbed.
   *
   * ATTN: Currently, the implementations of the nodes must be the
   * same.  The API does not require this, but it is just easier to
   * implement it that way.  Therefore, this document and the imported
   * node are required to share the same XML manager.
   *
   * @return The imported node, owned by the document.
   */
  virtual XMLNode* import_after(
    XMLNode* node, ///< [in] The node to insert
    YangNode* ynode, /// [in] The yang schema node
    bool deep ///< [in] Deep import - if true, import all the descendents as well
  ) = 0;

  /**
   * Remove the specified child of this node.  The child_node must be a
   * child of this node.  If successful, the child node is removed and freed
   *
   * @return true if the node was successfully removed, false otherwise
   */
  virtual bool remove_child(
    XMLNode* child_node ///< The child to remove.
  ) = 0;

  /**
   * Merge a node (and, its descendant nodes) into this node.  This API
   * follows the rules for netconf merge for all the contained nodes.
   *
   * ATTN: How to handle extra (non-YangModel) nodes?
   *   Can that be handled by validate?
   * ATTN: Should have a XMLNode node overload?
   *
   * @return The status of the merge.  Various errors.
   */
  virtual rw_yang_netconf_op_status_t merge(
    XMLNode* node ///< The node to merge.
  );

  /**
   * Merge a Protobuf C message into this node.  This API
   * follows the rules for netconf merge for all the contained nodes.
   *
   * The descriptors for each field and contained sub messages
   * should be the mangled names derived from the YANG node names.
   *
   * ATTN: How to handle extra (non-YangModel) nodes?
   *   Can that be handled by validate?
   *
   * ATTN: how to handle "assigned" names for messages/submessages?
   *
   * ATTN: A void* API is just bad programming practice.  We shouldn't
   * have this.
   *
   * @return The status of the merge.  Various errors.
   */
  virtual rw_yang_netconf_op_status_t merge(
      /**
       * [in] The message that needs to be merged to the DOM.
       */
      ProtobufCMessage* msg,

      /*!
       * [in] a flag indicating whether a node is being created.
       * ATTN: this doesnt implement the NETCONF MERGE with create flag
       */
      bool           is_create = false,

      /*!
       * [in] A flag to indicate whether for a listy node, the keys at a node
       * have to be validated against the incoming message. This check is
       * necessary only at a keyspec merge boundary with a message.
       */
      bool          validate_key = false
  );

  /**
   * Merge a serialized rpc protobuf to a DOM. The root node of the DOM is known
   * [it can be the root node of the dom]. The only child of the root is assumed
   * to be the RPC.
   *
   * ATTN: This is just a temperory hack - when keyspec can find RPC input and
   * output from a schema, this should be removed.
   */
  virtual rw_yang_netconf_op_status_t merge_rpc(
      /**
       * [in] The message that needs to be to merged
       */
      ProtobufCBinaryData* msg,

      /**
       * [in] A string that is either "input" or "output".
       */
      const char *type
  );

  /**
   * Merge a keyspec onto this node. The keyspec is a Protobuf C Message, which
   * is a qualified path in the YANG model. Each listy node in the model is
   * specialized with a key.
   *
   * The protobuf message passed in is a message that is rooted at the node where
   * the key spec ends.
   *
   * @return The status of the merge.  Various errors.
   */
  virtual rw_yang_netconf_op_status_t merge(

    /**
     * [in] The keyspec that needs to be merged to the DOM.
     */

      const rw_keyspec_path_t *keyspec,

      /**
       * [in] The message that needs to be to the tail of the keyspec
       */

      ProtobufCMessage* msg

  );

  /**
   * In DTS, the standard message passing mechanism constist of using a keyspec
   * and a protobuf message. At the sender, both of these can be derived from a
   * XML node using member functions. On the receiver, the keyspec and message
   * are present in a serialized buffer. Both the keyspec and the protbuf
   * message have to be de_serialized and converted to XML, adding nodes where
   * needed
   * @return The status of the merge.  Various errors.
   */
  virtual rw_yang_netconf_op_status_t merge(

    /**
     * [in] The keyspec that needs to be merged to the DOM.
     */

      ProtobufCBinaryData* keyspec,

      /**
       * [in] The message that needs to be to the tail of the keyspec. A null
       * message can be used to merge just a keyspec in to the DOM
       */

      ProtobufCBinaryData* msg = nullptr

  );

  /**
   * Add a child to the current node. The added child is a list entry, and the
   * function adds the keys to the list as well, as present in the path.
   *
   * @return The created child, ie, the list entry. The returned node could
   *         have children
   */

  virtual XMLNode* append_child
      (YangNode *yn, ///< [in] The yang node which specifies the listy node
       ProtobufCMessage *path); ///< [in] The key children of this node

  /**
   * When a XML Node associated with a YANG case node is merged,
   * other children of the current node might need to get deleted
   * so that the DOM has only the right case nodes.
   */
  void remove_conflicting_nodes(
    /// [in] The YANG node of the XML node being merged
    YangNode* new_node
  );

  /**
   * Given a Protobuf field and its corresponding YANG node, create a new
   * XMLNode, and append it to the current node in the tree.
   * The field can inturn be a message with more fields, so this could
   * end up creating a new sub tree in the DOM.
   */
  virtual rw_yang_netconf_op_status_t append_node_from_pb_field(
    /*! [in] The field to be appended. */
    const ProtobufCFieldInfo* finfo,

    /*! [in] The yang node associated with the value */
    YangNode *ynode
  );

  /**
   * Fill the node with default values. If the node has non-presence containers,
   * it goes deep into them and if any default descendants are found, fills them
   * too.
   */
  void fill_defaults();

  /**
   * Fill the RPC XMLNode's inputs with defaults.
   */
  void fill_rpc_input_with_defaults();

  /**
   * Fill the RPC XMLNode's output with defaults.
   */
  void fill_rpc_output_with_defaults();

  /**
   * Fills defaults for the children as provided in the ynode. Useful for
   * filling RPC inputs and outputs.
   */
  void fill_defaults(YangNode* ynode);

  /**
   * For given the list Yang node, fill all the list nodes that match with
   * default values.
   */
  void fill_defaults_list(YangNode* ylist);


 public:
  // Serialization APIs

  /**
   * Create a string representation of the node.  UTF-8 formatted.
   *
   * ATTN: need to capture errors somehow
   *
   * @return string representation of the node.  Empty on error.
   */
  virtual std::string to_string() = 0;

  /**
   * Create a string representation of the node in pretty format.
   * UTF-8 formatted.
   *
   * ATTN: need to capture errors somehow
   *
   * @return string representation of the node.  Empty on error.
   */
  virtual std::string to_string_pretty() = 0;

  /**
   * Output the node to stdout.  UTF-8 formatted.
   *
   * ATTN: need to capture errors somehow
   *
   * @return RW_STATUS_SUCCESS on success.
   */
  virtual rw_status_t to_stdout() = 0;

  /**
   * Output the node to a file.  UTF-8 formatted.
   *
   * ATTN: need to capture errors somehow
   *
   * @return RW_STATUS_SUCCESS on success.
   */
  virtual rw_status_t to_file(
    const char* file_name ///< [in] The file to serialize to.
  ) = 0;

  /**
   * Create a protobuf representation of the node and its children.
   *
   * ATTN: need a better way to capture errors.
   *
   * ATTN: What should we do about XML nodes that are unknown to the
   * schema and/or the message descriptor?  Different users will want
   * different behavior: some will want to ignore the unknowns, and
   * some will want to fail the export.  Could yangpbc automatically
   * generate a common field for such nodes - basically an automatic
   * anyxml field, where all the unknowns go as a malloc()ed string.
   *
   * ATTN: What should we do about XML atributes?
   *
   * @return A netconf status:
   *   - RW_YANG_NETCONF_OP_STATUS_OK on success
   */
  virtual rw_yang_netconf_op_status_t to_pbcm(
    /// [out] The protobuf message to fill
    ProtobufCMessage* top_pbcm
  );

  /**
   * The management agent has a global schema assigned. In this global schema,
   * the exact type of protobuf is not known, but the protobuf descriptor is.
   * Allocate memory for the protobuf message from the descriptors size,
   * initialize it, and make a message from the contents of this XML Node
   */
  virtual rw_yang_netconf_op_status_t to_pbcm(
    /// [out] The protobuf filled in
    ProtobufCMessage** pbcm
  );

  /**
   * Conversion from and to nodes that are derived from the children of YANG
   * RPC nodes require directional knowledge. NETCONF clients and servers know
   * whether what they are receiving are RPC requests or responses. The only way
   * to convert from children nodes of RPC correctly is to use such nodes.
   * This function converts a node that is associated with a YANG RPC assuming
   * that it represents RPC input.
   */

  virtual rw_yang_netconf_op_status_t to_rpc_input(
      ///[out] the protobuf message generated
      ProtobufCMessage **pbcm);

  /**
   * Conversion from and to nodes that are derived from the children of YANG
   * RPC nodes require directional knowledge. NETCONF clients and servers know
   * whether what they are receiving are RPC requests or responses. The only way
   * to convert from children nodes of RPC correctly is to use such nodes.
   * This function converts a node that is associated with a YANG RPC assuming
   * that it represents RPC output.
   */

  virtual rw_yang_netconf_op_status_t to_rpc_output(
      ///[out] the protobuf message generated
      ProtobufCMessage **pbcm);


  virtual rw_yang_netconf_op_status_t to_rpc (
      ///[in] The name of the child node of rpc, either input or output.
      const char *type,
      /// [out] the message to be generated
      ProtobufCMessage **pbcm);
  /**
   * Apply the passed netconfish dom  to this XML and return the result
   * as a dom.
   */
  virtual XMLNode* apply_subtree_filter_recursive(
    XMLNode* filter/// [in] Netconfish filter as a dom.
  );


  /* End of Yang XML related methods */


  /**
   * Find the key spec that defines this xml node. Works only if the right yang
   * model is loaded.
   */

  rw_yang_netconf_op_status_t to_keyspec (
      rw_keyspec_path_t **keyspec); /**<  Keyspec - alloced by library - owned by caller */

    /**
   * Get a specific keyspec from a generic one. This is required in various
   * find and merge functions. The implemenation of this function might change.
   */

  rw_yang_netconf_op_status_t get_specific_keyspec (
    /**
     * [in] The generic keyspec that has boe converted to a specific
     * keyspec
     */
    const rw_keyspec_path_t *gen_ks,
    /**
     * [out] The generated output keyspec
     */
    rw_keyspec_path_t*& spec_ks);
};



/**
 * RW XML node list, which is the list of child nodes in another node.
 *
 * ATTN: Need to decide if this is true: The user of a node list should
 * not assume that it is either live or stale - depending on the XML
 * implementation, the list of nodes may or may not reflect active
 * changes in the document.
 *
 * The RW XML node list is owned by the user.
 *
 * Abstract base class.
 */
class XMLNodeList
: public rw_xml_node_list_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLNodeList>::uptr_t uptr_t;

 public:
  /// Construct a node list.
  XMLNodeList()
  {}

  /**
   * Release the node.  Use this API to destroy the node when you are
   * done with it, and it is no longer in the tree.  Do not call this
   * if the node is in the tree - it must be removed first!
   */
  virtual void obj_release() noexcept = 0;

 protected:
  /// Trivial protected destructor - see XMLNodeList::obj_release().
  virtual ~XMLNodeList()
  {}

  // Cannot Copy
  XMLNodeList (const XMLNodeList &) = delete;
  XMLNodeList& operator = (const XMLNodeList &) = delete;


 public:
  /**
   * Get the number of elements in the list.
   * @return The number of nodes.
   */
  virtual size_t length() = 0;

  /**
   * Find the first node in the list with a specific name (and
   * namespace).  This API is ambiguous - multiple nodes may match, in
   * which case only the first matching node is returned.
   *
   * ATTN: Should also have a next function, to continue a search for
   * additional matches.
   *
   * ATTN: Should (also) have a yang-based API for this.
   *
   * @return The first matching node, or nullptr if no matches.
   */
  virtual XMLNode* find(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name
     * matches, regardless of namespace, will be accepted.  May also
     * pass rw_xml_namespace_any to match any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Get the node at a specific position within the list.
   * @return The node is valid position, otherwise nullptr.
   */
  virtual XMLNode* at(
    size_t index ///< The node's position
  ) = 0;

  /**
   * Get a reference to the owning document.
   * @return The owning document.
   */
  virtual XMLDocument& get_document() = 0;

  /**
   * Get a reference to the document's manager.
   * @return The owning manager.
   */
  virtual XMLManager& get_manager() = 0;
};



/**
 * XMLDocument.  Contains a complete DOM.  Most management functions
 * will maintain multiple DOMs depending on the databases in use.  DOMs
 * may be created during processing of single CLI commands, and
 * destroyed after the CLI is processed, or could persist for the life
 * of a process, like the DOM associated with a running configuration.
 *
 * ATTN: Should also support the XMLNode interface?
 *
 * ATTN: Should also define a default namespace?  Or is that up to
 * individual implementations?
 *
 * Abstract base class.
 */
class XMLDocument
: public rw_xml_document_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLDocument>::uptr_t uptr_t;

 public:
  /// Construct a document
  XMLDocument(rwmemlog_instance_t* memlog_inst) : memlog_buf_(memlog_inst, "XMLDom", reinterpret_cast<intptr_t>(this))
  {}

  /**
   * Release the document.  Use this API to destroy the document when
   * you are done with it.  This destroys all underlying data
   * structures!  The caller should not refer to any child resource
   * obtained from the document, after this call.  Prior to this call,
   * the caller should ensure that there are no caller-owned resources
   * still active, as freeing them may become in invaid operation after
   * this call.
   */
  virtual void obj_release() noexcept = 0;

 protected:
  /// Trivial protected destructor - see XMLDocument::obj_release().
  virtual ~XMLDocument()
  {}

  // Cannot Copy
  XMLDocument (const XMLDocument &) = delete;
  XMLDocument& operator = (const XMLDocument &) = delete;

 public:
  /**
   * Get a reference to the owning manager
   *
   * @return The owning manager
   */
  virtual XMLManager& get_manager() = 0;

  /**
   * Get the reference to the error handler class.
   *
   * @return The error handler object reference
   */
  virtual XMLErrorHandler& get_error_handler() = 0;

  /**
   * Find the root node.
   * @return The root node.  Owned by the document.  Cannot be nullptr.
   */
  virtual XMLNode* get_root_node() = 0;

  /**
   * Find the node which has the specified name (and namespace).  This
   * API is ambiguous - multiple nodes may match, in which case only
   * the first matching node is returned.
   *
   * ATTN: Should also have a next function, to continue a search for
   * additional matches.
   *
   * ATTN: Should (also) have a yang-based API for this.
   *
   * @return The node if found, nullptr if not.
   */
  virtual XMLNode* get_root_child_element(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Get the list of elements that match a given name (and namespace).
   * This API can return elements at any level in the DOM, not
   * necessarily just the children of the root node.  This API is not
   * necessarily supported by all DOM implementations.
   *
   * @return The list of matching nodes.
   */
  virtual XMLNodeList::uptr_t get_elements(
    /**
     * [in] The element name, in UTF-8.  Must not include namespace
     * prefix.
     */
    const char* local_name,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then any name matches,
     * regardless of namespace, will be accepted.  May also pass
     * rw_xml_namespace_any to match any namespace.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Test if two documents are equal.
   *
   * ATTN: Need to implement visitor pattern for this to work
   * correctly, across different DOM implementations?
   *
   * @return true if the documents are equal
   */
  virtual bool is_equal(
    /**
     * [in] The document to compare to.
     */
    XMLDocument* other_doc
  ) = 0;

  /**
   * Import a node, possibly from another document, into this document.
   * The imported node will be copied - the original will remain
   * undisturbed.  Optionally, the node's descendants are also imported.
   *
   * ATTN: Currently, the implementations of the nodes must be the
   * same.  The API does not require this, but it is just easier to
   * implement it that way.  Therefore, this document and the imported
   * node are required to share the same XML manager.
   *
   * @return The imported node, which belongs to this document.  Caller
   *   takes ownership, because it is not yet inserted into the
   *   document.
   */
  virtual XMLNode::uptr_t import_node(
    XMLNode* node, ///< [in] The node to import
    YangNode* yang_node, /// < [in] The corresponding yang node
    bool deep ///< [in] Deep import - if true, import all the descendents as well
  ) = 0;

  /**
   * ATTN: bad documentation
   *
   * @return RW_YANG_NETCONF_OP_STATUS_OK on success
   */
  virtual rw_yang_netconf_op_status_t merge(
    XMLDocument* other_doc ///< [in] The document to merge
  ) = 0;

  /**
   * Walk from the root node the path in the PBCM and then export the
   * result node to the protoc-c data message data structure.
   *
   * ATTN: This function cannot traverse list nodes, because it doesn't
   * know the keys!  Therefore, it will always return the first list
   * entry.
   *
   * ATTN: Create an iterator for list entries?
   *
   * @return RW_YANG_NETCONF_OP_STATUS_OK on success
   */
  virtual rw_yang_netconf_op_status_t to_pbcm(
    /**
     * [in,out] The message to export to.  This buffer must have
     * already been initialized by the appropriate protoc-c
     * initialization function!  Cannot be nullptr.
     */
    ProtobufCMessage* base
  ) = 0;

  /**
   * Create a string representation of the DOM at a requested tag.
   *
   * @return string representation of the DOM.
   */
  virtual std::string to_string(
    /**
     * [in] Optional root child element name, for exporting just a subtree,
     * in UTF-8.  Must not include namespace prefix.  If nullptr, the
     * entire document will be serialized.
     */
    const char* local_name = nullptr,

    /**
     * [in] Optional root child element namespace, for exporting just a
     * subtree, in UTF-8The namespace, in UTF-8.  If nullptr, then any
     * namespace will be accepted.  May also pass rw_xml_namespace_any
     * to match any namespace.  Must be nullptr if name is nullptr.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Create a pretty string representation of the DOM at a requested tag.
   *
   * @return string representation of the DOM.
   */
  virtual std::string to_string_pretty(
    /**
     * [in] Optional root child element name, for exporting just a subtree,
     * in UTF-8.  Must not include namespace prefix.  If nullptr, the
     * entire document will be serialized.
     */
    const char* local_name = nullptr,

    /**
     * [in] Optional root child element namespace, for exporting just a
     * subtree, in UTF-8The namespace, in UTF-8.  If nullptr, then any
     * namespace will be accepted.  May also pass rw_xml_namespace_any
     * to match any namespace.  Must be nullptr if name is nullptr.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Output the node at requested tag and namespace to stdout.
   *
   * @return RW_STATUS_SUCCESS on success
   */
  virtual rw_status_t to_stdout(
    /**
     * [in] Optional root child element name, for exporting just a subtree,
     * in UTF-8.  Must not include namespace prefix.  If nullptr, the
     * entire document will be serialized.
     */
    const char* local_name = nullptr,

    /**
     * [in] Optional root child element namespace, for exporting just a
     * subtree, in UTF-8The namespace, in UTF-8.  If nullptr, then any
     * namespace will be accepted.  May also pass rw_xml_namespace_any
     * to match any namespace.  Must be nullptr if name is nullptr.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Output the node at requested tag and namespace to a file.
   *
   * @return RW_STATUS_SUCCESS on success
   */
  virtual rw_status_t to_file(
    /**
     * [in] The file to output to.
     */
    const char* file_name,

    /**
     * [in] Optional root child element name, for exporting just a subtree,
     * in UTF-8.  Must not include namespace prefix.  If nullptr, the
     * entire document will be serialized.
     */
    const char* local_name = nullptr,

    /**
     * [in] Optional root child element namespace, for exporting just a
     * subtree, in UTF-8The namespace, in UTF-8.  If nullptr, then any
     * namespace will be accepted.  May also pass rw_xml_namespace_any
     * to match any namespace.  Must be nullptr if name is nullptr.
     */
    const char* ns = nullptr
  ) = 0;

  /*!
   * Get the memlog buffer for XMLDocument.
   */
  rwmemlog_buffer_t** get_memlog_ptr()
  {
    return &memlog_buf_;
  }

  virtual rw_status_t evaluate_xpath(XMLNode * xml_node,
                                     std::string const xpath) = 0;


  /*!
   * API for doing subtree filtering as per RFC-6241
   * The filtering is done on the current DOM thereby modifying its
   * content.
   * @filter_dom : XML DOM of the filter which needs to be applied
   * on this DOM.
   */
  virtual rw_status_t subtree_filter(rw_yang::XMLDocument* filter_dom);

private:
  /*!
   * Filtering for the containment node.
   * Containment node usually hold the other types of node
   * which are: 1) Selection node 2) Content match node
   *
   * Based on the child filter under the containment node it recurses.
   */
  virtual rw_status_t filter_containment_node(XMLNode* filter_node,
                                              XMLNode* data_node);

  /*!
   * Filtering for selection node. <node/>
   */
  virtual rw_status_t filter_selection_node(XMLNode* filter_node, XMLNode* data_node);

  /*!
   * This function iterates through the child nodes of the containment
   * filter node.
   * If the child node is leaf AND has value it will be pushed to content_match_nodes.
   * For any other node, it will be pushed to sibling_nodes.
   * This is later used to perform filtering as per
   * https://tools.ietf.org/html/rfc6241#section-6.4.6
   */
  virtual void find_content_match_nodes(XMLNode* filter_node,
                                        std::list<XMLNode*>& content_match_nodes,
                                        std::list<XMLNode*>& sibling_nodes);

  /*!
   * Filtering for content match nodes.
   */
  virtual rw_status_t filter_content_match_nodes(std::list<XMLNode*>& content_match_nodes,
                                                 XMLNode* data_node);


protected:

  /*!
   * The memory logger for this DOM
   */
  RwMemlogBuffer memlog_buf_;
};



/**
 * @brief XML manager
 *
 * An XML manager contains methods that help in abstracting out the
 * implementation from applications that use the library.  The manager
 * initializes the implementation.  The manager contains factory
 * methods to create new XML documents.
 *
 * Abstract base class.
 */
class XMLManager
: public rw_xml_manager_s
{
 public:
  typedef rw_yang::UniquePtrXMLObjectRelease<XMLManager>::uptr_t uptr_t;

 public:
  XMLManager();

  /**
   * Release the manager.  Use this API to destroy the manager, and all
   * releated resources, when you are done with them.  This destroys
   * all underlying data structures, including the documents!  The
   * caller should not refer to any resource obtained through the
   * manager after this call.  Prior to this call, the caller should
   * have released all the documents.
   */
  virtual void obj_release() noexcept = 0;


 protected:
  /// Trivial protected destructor - see XMLManager::obj_release().
  virtual ~XMLManager()
  {}

  // Cannot copy
  XMLManager (const XMLManager &) = delete;
  XMLManager& operator = (const XMLManager&) = delete;

 public:
  /**
   * Get the yang model descriptor for this manager.
   * @return The yang model descriptor.
   */
  virtual YangModel* get_yang_model() = 0;

  /**
   * Create an empty document, with a root node of the specified name
   * and namespace.
   *
   * ATTN: Can fail?
   *
   * @return The new document.  Caller owns the document.
   */
  virtual XMLDocument::uptr_t create_document(
    /**
     * [in] The root element name, in UTF-8.  Must not include
     * namespace prefix.  If nullptr, then the root node name will be
     * set to a default value depending on the DOM implementation.
     */
    const char* local_name = nullptr,

    /**
     * [in] The namespace, in UTF-8.  If nullptr, then the namespace
     * will be set to a default value depending on the DOM
     * implementation.
     */
    const char* ns = nullptr
  ) = 0;

  /**
   * Create a document based on the passed yangnode.
   *
   * ATTN: Can fail?
   *
   * @return The new document.  Caller owns the document.
   */
  virtual XMLDocument::uptr_t create_document(
    YangNode* yang_node /// [in] The  YangNode based on which this document needs to be created
  ) = 0;

  /**
   * Create a new document from a file.
   *
   * ATTN: How to pass errors back?
   * ATTN: Validation should be pluggable - might want yang-dom
   *   validation even if xsd is possible
   *
   * @return The new document.  nullptr on error.  Caller owns the
   *   document.
   */
  virtual XMLDocument::uptr_t create_document_from_file(
    const char* file_name, ///< The file name. Cannot be nullptr.
    std::string& error_out, ///< The error_out string to capture errors.
    /// ATTN: error_out should be a pointer, nullable, and default to nullptr
    bool validate = true ///< If true, validate during parsing
  ) = 0;

  /**
   * Create a new document from a file.
   *
   * ATTN: How to pass errors back?
   * ATTN: Validation should be pluggable - might want yang-dom
   *   validation even if xsd is possible
   *
   * @return The new document.  nullptr on error.  Caller owns the
   *   document.
   */
  virtual XMLDocument::uptr_t create_document_from_file(
    const char* file_name, ///< The file name. Cannot be nullptr.
    bool validate = true ///< If true, validate during parsing
  ) = 0;

  /**
   * Create a new document from a string.
   *
   * ATTN: How to pass errors back?
   *
   * @return The new document.  nullptr on error.  Caller owns the
   *   document.
   */
  virtual XMLDocument::uptr_t create_document_from_string(
    const char* xml_str, ///< The XML blob to read. Cannot be nullptr.
    std::string& error_out, ///< The error_out string to capture errors.
    /// ATTN: error_out should be a pointer, nullable, and default to nullptr
    bool validate = true ///< If true, validate during parsing
  ) = 0;

  /**
   * Create a newyang-based document from a proto-c structure
   *
   * New API because you can't have covariant smart pointer return
   * types.
   *
   * If a rooted document is requested, there should either be no
   * lists in the path from the root of the YangModel to the root of
   * the message, or a key spec that disambuguates the path needs to
   * be provided
   *
   * @return The new document.  nullptr on error.  Caller owns the
   *   document
   */
  virtual XMLDocument::uptr_t create_document_from_pbcm(
    /**
     * [in] PBCM from which document is to be created.
     ATTN: Should be const
     */
    ProtobufCMessage *pbcm,

    /**
     * [out] Error detected.
     * ATTN: DO NOT PASS OUT PARAMS BY REF
     */
    rw_yang_netconf_op_status_t& err,

    /**
     * [in] Specifies whether the generated document is rooted at the
     * root of the YANG schema, or at the start of the PBCM node. "false"
     * indicates an unrooted XML snippet
     */
    bool rooted = true,

    /// [in] key spec of the path to pbcm.
    const rw_keyspec_path_t *keyspec = nullptr

  ) = 0;

  /**
   * Convert a Protobuf structure to a XML string. The protobuf structure
   * is first used to build a YANG document. The root node of the constructed
   * DOM is used to construct an XML string
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE if the generated string is too large
   * @returns RW_STATUS_FAILURE if the protobuf path or data does not correspond
   *          to the YANG DOM
   */
  virtual rw_status_t pb_to_xml(
    /// [in] The protobuf structure to be converted to XML
    ProtobufCMessage *pbcm,

    /// [out] String to be constructed
    std::string& xml_string,

    /// [in] key spec of the path to pbcm.
    rw_keyspec_path_t *keyspec = 0
  ) = 0;

  /**
   * Convert a Protobuf structure to a XML string. The protobuf structure
   * is first used to build a YANG document. The root node of the constructed
   * DOM is used to construct an XML string
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE if the generated string is too large
   * @returns RW_STATUS_FAILURE if the protobuf path or data does not correspond
   *          to the YANG DOM
   */
  virtual rw_status_t pb_to_xml_pretty(
    /// [in] The protobuf structure to be converted to XML
    ProtobufCMessage *pbcm,

    /// [out] String to be constructed
    std::string& xml_string,

    /// [in] key spec of the path to pbcm.
    rw_keyspec_path_t *keyspec = 0
  ) = 0;

  /**
   * Translations from Protobuf to XML and vice-versa is used commonly in
   * Riftware. In some cases, the protobuf to be converted cannot be a full
   * XML document as per the YANG. The reason for this is that the subtree
   * that is encoded in the protobuf is below a list node, the keys of which are
   * not known in the Translation function.
   * The unrooted translation creates a XML fragment for the subtree. The
   * subtree has nodes that are fully YANG qualified.
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE if the generated string is too large
   * @returns RW_STATUS_FAILURE if the protobuf path or data does not correspond
   *          to the YANG DOM
   */
  virtual rw_status_t pb_to_xml_unrooted(
    /// [in] The protobuf structure to be converted to XML
    ProtobufCMessage *pbcm,

    /// [out] String to be constructed
    std::string& xml_string
  ) = 0;

  /**
   * Translations from Protobuf to XML and vice-versa is used commonly in
   * Riftware. In some cases, the protobuf to be converted cannot be a full
   * XML document as per the YANG. The reason for this is that the subtree
   * that is encoded in the protobuf is below a list node, the keys of which are
   * not known in the Translation function.
   * The unrooted translation creates a XML fragment for the subtree. The
   * subtree has nodes that are fully YANG qualified.
   * @returns RW_STATUS_SUCCESS on success
   * @returns RW_STATUS_FAILURE if the generated string is too large
   * @returns RW_STATUS_FAILURE if the protobuf path or data does not correspond
   *          to the YANG DOM
   */
  virtual rw_status_t pb_to_xml_unrooted_pretty(
    /// [in] The protobuf structure to be converted to XML
    ProtobufCMessage *pbcm,

    /// [out] String to be constructed
    std::string& xml_string
  ) = 0;

  /**
   * Convert a XML string to a protobuf. The XML has to be rooted at the
   * top of the YANG model. The protbuf has an embedded path in it, and
   * the DOM is walked by that path to reach the node from where the PB
   * data is generated
   */
  virtual rw_status_t xml_to_pb(
    /**
     * [in] XML string that has to be converted to a protobuf C
     * structure
     */
    const char *xml_str,

    /**
     * [out] An initialized protobuf message structure that will be filled
     * with the data in the XML
     */
    ProtobufCMessage *pbcm,

    /**
     * [out] String to capture any errors during parsing and conversion.
     */
    std::string& error_out,

    /**
     * [in] a keyspec that specifies the path from the start of the model to this
     * protobuf.
     */
    rw_keyspec_path_t *keyspec = nullptr
  ) = 0;

  /**
   * Set the log callback for the xml manager
   */

  void set_log_cb (rw_xml_mgr_log_cb cb, void *user_data)
  { log_cb_ = cb; log_cb_user_data_ = user_data;}

  /**
   * Get the rwmemlog instance pointer.
   */
  rwmemlog_instance_t* get_memlog_inst()
  {
    return memlog_inst_;
  }

  /**
   * Get the memlog buffer for XMLMgr
   */
  rwmemlog_buffer_t** get_memlog_ptr()
  {
    return &memlog_buf_;
  }

public:
  rw_xml_mgr_log_cb log_cb_;
  void *log_cb_user_data_;

  /*!
   * The protobuf instance used for PB operations
   */

  ProtobufCInstance pb_instance_;

protected:

  /*!
   * The rwmemlog instance. The owner of the xmlmgr, if
   * required has to register it with dts for config or get
   */
  RwMemlogInstance memlog_inst_;

  /*!
   * The rwmemlog buffer used by this mgr instance for tracing.
   */
  RwMemlogBuffer memlog_buf_;
};

/**
 * XMLNode iterator type.  Iterates over siblings.  Node iterators
 * may be safely compared ONLY when created from the same parent node.
 */
class XMLNodeIter
{
private:
  /// The underlying pointer
  XMLNode* p_;

public:
  /// Create an node iterator pointing at nothing.
  XMLNodeIter()
  : p_(nullptr)
  {}

  /// Create a node iterator located at the given node.
  explicit XMLNodeIter(XMLNode* node)
  : p_(node)
  {}

  /// Copy a node iterator.
  XMLNodeIter(const XMLNodeIter& other)
  : p_(other.p_)
  {}

  /// Assign a node iterator
  XMLNodeIter& operator=(const XMLNodeIter& other)
  {
    p_ = other.p_;
    return *this;
  }

  /// Compare two node iterators.
  bool operator==(const XMLNodeIter& other) const
  {
    return p_ == other.p_;
  }

  /// Compare two node iterators.
  bool operator!=(const XMLNodeIter& other) const
  {
    return !(p_ == other.p_);
  }

  /// Pre-increment node iterator
  XMLNodeIter& operator++()
  {
    RW_ASSERT(p_);
    p_ = p_->get_next_sibling();
    return *this;
  }

  /// Post-increment node iterator
  XMLNodeIter operator++(int)
  {
    RW_ASSERT(p_);
    XMLNodeIter old(*this);
    p_ = p_->get_next_sibling();
    return old;
  }

  /// Dereference iterator.
  XMLNode& operator*()
  {
    return *p_;
  }

  /// Dereference iterator.
  XMLNode* operator->()
  {
    return p_;
  }

  typedef XMLNode value_type;
  typedef void difference_type;
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef XMLNode* pointer;
  typedef XMLNode& reference;
};

/**
 * Create a Xerces-based XML Manager.
 *
 * @return The manager.  Crashes or throws on failure.
 */
XMLManager::uptr_t xml_manager_create_xerces(
  YangModel* model = nullptr ///< [in] The model to use.  May be nullptr.  Must live at least as long as the manager.
);

/**
 * XMLWalker object. An XMLWalker object allows walking over an  XML subtree.
 *
 * Unless used in a threadsafe context, this class is not threadsafe.
 *
 */
class XMLWalker
{

protected:
  /// The XMLNode where this walker is currently on
  XMLNode* curr_node_;

  /// The current XML Node in the new tree
  int32_t level_;

  /// The current XML Node in the new tree
  XMLNode *path_[RW_MAX_XML_NODE_DEPTH];

public:
  /// Create an Empty XMLWalker object
  XMLWalker()
  : curr_node_(nullptr)
  {}

  /// Create an XMLWalker located at the given node.
  explicit XMLWalker(XMLNode* node)
  : curr_node_(node)
  {}

  /// Copy a XMLWalker
  XMLWalker(const XMLWalker& other)
  : curr_node_(other.curr_node_)
  {}

  /// Assign a XML Walker
  XMLWalker& operator=(const XMLWalker& other)
  {
    curr_node_ = other.curr_node_;
    return *this;
  }

  /// Compare two XML Walkers
  bool operator==(const XMLWalker& other) const
  {
    return curr_node_ == other.curr_node_;
  }

  /// Compare two XML Walkers
  bool operator!=(const XMLWalker& other) const
  {
    return !(curr_node_ == other.curr_node_);
  }

  /// Dereference XMLWalker.
  XMLNode& operator*()
  {
    return *curr_node_;
  }

  /// Dereference XMLWalker
  XMLNode* operator->()
  {
    return curr_node_;
  }

  /**
   * The XMLWalker's walk() function. This function walks the passed XML sub-tree, executes the visit() method from
   * the passed visitor.
   *
   * The order in which the XML tree is walked will be based on the concrete implementation. When visitors are
   * chained, the visit function executes each of the visitor's visit() method as far as the visit method is
   * is returning XML_NODE_WALK_NOOP
   */

  virtual void walk(XMLVisitor *visitor) = 0;

};

/**
 * XMLWalkerInOrder is a concrete implementation of the XMLWalker object that walks the XML subtree in
 * inorder manner. The walker starts walking the obect at root and based on the return value from
 * visit() in the XMLVisitor object, the next node to walk is determined.
 *
 * Unless used in a threadsafe context, this class is not threadsafe.
 *
 */
class XMLWalkerInOrder
  :public XMLWalker
{
  public:
    /// trivial constructor
    XMLWalkerInOrder(XMLNode *node):XMLWalker(node) {}

    /// trivial  destructor
    virtual ~XMLWalkerInOrder() {}

     // Cannot copy
     XMLWalkerInOrder(const XMLWalkerInOrder&) = delete;
     XMLWalkerInOrder& operator=(const XMLWalkerInOrder&) = delete;
  public:
    void walk(XMLVisitor *visitor) override;

  private:
    virtual void walk_recursive(XMLNode* node, XMLVisitor *visitor);
};

/**
 * The action for XMLWalker from the Visitor object
 * The visit() method in the Visitor object returns this
 * enum and instructs the walker what to do next.
 *
 * See XMLNode::visit(XMLNode* node)
 *
 */
typedef enum rw_xml_next_action_e {
  RW_XML_ACTION_NULL = 0,
  RW_XML_ACTION_base = 0x15081947,
  RW_XML_ACTION_NOOP,
  RW_XML_ACTION_NEXT,
  RW_XML_ACTION_FIRST_CHILD,
  RW_XML_ACTION_NEXT_SIBLING,
  RW_XML_ACTION_TERMINATE,
  RW_XML_ACTION_ABORT,
  RW_XML_ACTION_end,
  RW_XML_ACTION_FIRST = RW_XML_ACTION_base+1,
  RW_XML_ACTION_LAST = RW_XML_ACTION_end-1,
  RW_XML_ACTION_COUNT = RW_XML_ACTION_end-RW_XML_ACTION_base-1,
} rw_xml_next_action_t;

static inline bool_t rw_xml_next_action_is_good(rw_xml_next_action_t v)
{
  return    (int)v >= (int)RW_XML_ACTION_FIRST
      && (int)v <= (int)RW_XML_ACTION_LAST;
}

/**
 * XMLVisitor is used by the XMLWalker object to walk the XMLNode's subtree.
 * The object receives the node in the visit method and inidcates to XMLWalker
 * in the response how to walk the tree -- first child, silbling, parent ...
 *
 * Unless used in a threadsafe context, this class is not threadsafe.
 *
 * ATTN: Revisit thread safe considerations
 * ATTN: Need to consider chaining -- Ability to get the walker run through a chain of visitors
 *
 */
class XMLVisitor
{
  protected:

    rw_yang_netconf_op_status_t netconf_status;

    /// The current XML Node in the new tree
    XMLNode* curr_node_;

    /// The current XML Node in the new tree
    int32_t level_;

    /// The current XML Node in the new tree
    rw_xml_node_pair_t path_[RW_MAX_XML_NODE_DEPTH];


  public:
    /// trivial constructor
    XMLVisitor();

    /// trivial  destructor
    virtual ~XMLVisitor() {}

     // Cannot copy
     XMLVisitor(const XMLVisitor&) = delete;
     XMLVisitor& operator=(const XMLVisitor&) = delete;
  public:
    /**
     * This method is called by the XMLWalker object with the XMLNode it is currently
     * walking.
     *
     * @return the indication to walker - Used by XMLWalker to determine what to do next
     *
     *   RW_XML_ACTION_NOOP         - Invoke visit() in the next visitor in the visitor chain
     *   RW_XML_ACTION_FIRST_CHILD  - Advance to the first child of the current node
     *   RW_XML_ACTION_NEXT_SIBLING - Advance to the next sibling of the current node
     */

    virtual rw_xml_next_action_t visit(XMLNode* node, XMLNode** path, int32_t level) = 0;

    /**
     * Get the netconf status associated with this object when the visit() method fails with RW_XML_ACTION_ABORT
     */
    rw_yang_netconf_op_status_t get_netconf_status();
};

/**
 * XMLCopier is a XMLVisitor object that copies the passed XMLNode subtree into the passed document.
 *
 * The object receives the node in the visit method and indicates to XMLWalker
 * in the response how to walk the tree -- first child, silbling, parent ...
 *
 * Unless used in a thread safe context, this class by iteself is not threadsafe.
 *
 * ATTN: Revisit thread safety considerations
 * ATTN: Need to consider chaining -- Ability to get the walker run through a chain of visitors
 *
 */
class XMLCopier
  :public XMLVisitor
{
  protected:
    /// The XML subtree to which to copy -- The callee owns the Node
    XMLNode* to_node_;



  public:
    /// constructor - node is owned by the caller and expected to be around while this object is alive
    XMLCopier(XMLNode *node):to_node_(node) {}

    /// trivial  destructor
    virtual ~XMLCopier() {}

     // Cannot copy
     XMLCopier(const XMLCopier&) = delete;
     XMLCopier& operator=(const XMLCopier&) = delete;

  public:

    /**
     * This method is called by the XMLWalker object with the XMLNode it is currently
     * walking.  The method, copies the passed  XMLNode into the object's holding node.
     *
     * @return the indication to walker - Used by XMLWalker to determine what to do next
     *
     *   RW_XML_ACTION_NOOP         - Invoke visit() in the next visitor in the visitor chain
     *   RW_XML_ACTION_NEXT         - Advance to the next XMLNode of the current node
     *   RW_XML_ACTION_FIRST_CHILD  - Advance to the first child of the current node
     *   RW_XML_ACTION_NEXT_SIBLING - Advance to the next sibling of the current node
     */

     virtual rw_xml_next_action_t visit(XMLNode* node, XMLNode** path, int32_t level);
};

/**
 * XMLYangValidatedCopier is a XMLVisitor object that copies the passed XMLNode
 * subtree into the passed document, only if the source tree has an associated
 * YANG Node. The primary use of this is to create a copy of a tree which does
 * not have white spaces.
 *
 *
 * ATTN: Should yANG validation be added?
 *
 */
class XMLYangValidatedCopier
  :public XMLCopier
{

  public:
    /// constructor - node is owned by the caller and expected to be around while this object is alive
    XMLYangValidatedCopier(XMLNode *node):XMLCopier(node) {}

    /// trivial  destructor
    virtual ~XMLYangValidatedCopier() {}

     // Cannot copy
     XMLYangValidatedCopier(const XMLCopier&) = delete;
     XMLYangValidatedCopier& operator=(const XMLCopier&) = delete;

  public:

    /**
     * This method is called by the XMLWalker object with the XMLNode it is currently
     * walking.  The method, copies the passed  XMLNode into the object's holding node.
     *
     * @return the indication to walker - Used by XMLWalker to determine what to do next
     *
     *   RW_XML_ACTION_NOOP         - Invoke visit() in the next visitor in the visitor chain
     *   RW_XML_ACTION_NEXT         - Advance to the next XMLNode of the current node
     *   RW_XML_ACTION_FIRST_CHILD  - Advance to the first child of the current node
     *   RW_XML_ACTION_NEXT_SIBLING - Advance to the next sibling of the current node
     */

     virtual rw_xml_next_action_t visit(XMLNode* node, XMLNode** path, int32_t level);
};

/**
 * The Builder object supports building of a XML-ish subtree in multiple steps.
 * The Builder is instantiated with a start node. Children can be added to
 * the current node, and the created node can be set to become the new current
 * node. When the current node changes to be a created child, the old current
 * node is preserved in a stack. The builder then supports a "pop" operation to
 * move up from its current node.
 *
 * The return values on all methods are the same - the next action to be taken.
 * The base class is pure abstract.
 *
 * The append functions have default implementations, so that a specialized
 * builder needs to implement only one type of appends - either with strings
 * or with XML Nodes. The default functions if called will cause the traversal
 * be aborted.
 */

class Builder
{
public:
  /**
   * Constructor - Initialize a builder.
   */
  Builder(): memlog_inst_("Builder", 0) /* DEFAULT retired_count */
           , memlog_buf_(memlog_inst_, "BuilderInst", reinterpret_cast<intptr_t>(this))
  {} ;

  /**
   * Add a child to the current node. The "current" node does not change
   * return value is an indication to the traverser on which node to visit next:
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t append_child (
      const char* local_name, /// [in] The local name of the new node (n/i prefix).
      const char* ns = nullptr, /// [in] The node's namespace. May be nullptr - BUT SHOULD BE AVOIDED.
      const char* text_val = nullptr); /// [in] The text optional value to assign to the node. May be nullp

  /**
   * Add a child to the current node. The "current" node does not change
   * return value is an indication to the traverser on which node to visit next:
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t append_child (
      XMLNode *node /// [in] The XML node that needs to be appended
  );

  /**
   * Add a child to the current node of an enumeration type.
   * The "current" node does not change
   * return value is an indication to the traverser on which node to visit next:
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t append_child_enum (const char *local_name,
                                                  const char *ns,
                                                  int32_t val);

  /**
   * Mark a child of the current node as "to be deleted". This is required mainly
   * in interactions with NETCONF. During an EDIT CONFIG operation, some nodes
   * will have operation set to "delete" or "remove", but the rest of tree could
   * have any operation.
   */

  virtual rw_xml_next_action_t mark_child_for_deletion (
      const char* local_name, /// [in] The local name of the new node (n/i prefix).
      const char* ns); /// [in] The node's namespace. May be nullptr - BUT SHOULD BE AVOIDED.


  /**
   * Delete a child to the current node. The "current" node does not change
   * The text value of the child is set to the text representing the enumeration
   */

  /**
   * Add a child to the current node. Set the current node to the created child
   * node
   *
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t push (
      const char* local_name, /// [in] The local name of the new node (n/i prefix).
      const char* ns = nullptr); /// [in] The node's namespace. May be nullptr - BUT SHOULD BE AVOIDED.

  /**
   * Add a child to the current node. Set the current node to the created child
   * node
   *
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t push (
      XMLNode *node); /// Node which needs to be built out

  /**
   * Set the current node to a previous current node. This pops the node stack.
   * pushes and pops have to be "inorder" to have expected resulting XML nodes.
   *
   *
   * @return RW_XML_ACTION_NEXT traverse the next node
   * @return RW_XML_ACTION_NEXT_SIBLING skip children of current node, and move
   *         to next node
   * @return RW_XML_ACTION_ABORT The builder encountered an error, and the
   *         traversal has to be aborted
   * @return RW_XML_ACTION_TERMINATE The builder has done building.
   */

  virtual rw_xml_next_action_t pop() = 0;

  /**
   * Set the log callback for the xml manager
   */

  virtual void set_log_cb (rw_log_cb cb, void *user_data)
  { log_cb_ = cb; log_cb_user_data_ = user_data;}

  /**
   * Get the rwmemlog instance pointer.
   */
  rwmemlog_instance_t* get_memlog_inst()
  {
    return memlog_inst_;
  }

  /**
   * Get the memlog buffer for XMLMgr
   */
  rwmemlog_buffer_t** get_memlog_ptr()
  {
    return &memlog_buf_;
  }

  /**
   * The destructor
   */
  virtual ~Builder(){};

public:
  rw_log_cb log_cb_ = nullptr;
  void *log_cb_user_data_ = nullptr;

protected:
  /*!
   * The rwmemlog instance. The owner of the xmlmgr, if
   * required has to register it with dts for config or get
   */
  RwMemlogInstance memlog_inst_;

  /*!
   * The rwmemlog buffer used by this mgr instance for tracing.
   */
  RwMemlogBuffer memlog_buf_;
};

/** The XMLBuilder object implements a simple builder that instructs the traverser to
 * move through all the nodes it can, detecting errors in creation of nodes and
 * aborting traversal in these cases.
 */


class XMLBuilder : public Builder
{
public:
  /**
   * Constructor - Initialize a builder.
   */
  XMLBuilder (XMLManager *mgr); /**< [in] The  manager for this builder */
  XMLBuilder (XMLNode *node); /** <[in] Current node for this builder */
  ///@see Builder::append_child(const char * local_name, const char *ns, const char *text_val)
  virtual rw_xml_next_action_t append_child (
      const char* local_name,
      const char* ns = nullptr,
      const char* text_val = nullptr);

  ///@see Builder::append_child_enum
  virtual rw_xml_next_action_t append_child_enum (const char *local_name,
                                                  const char *ns,
                                                  int32_t value);

  ///@see Builder::mark_child_for_deletion
  virtual rw_xml_next_action_t mark_child_for_deletion (
      const char* local_name, /// [in] The local name of the new node (n/i prefix).
      const char* ns); /// [in] The node's namespace. May be nullptr - BUT SHOULD BE AVOIDED.

  ///@see Builder::push(const char * local_name, const char *ns)
  virtual rw_xml_next_action_t push (
      const char* local_name,
      const char* ns = nullptr);

  ///@see Builder::pop()
  virtual rw_xml_next_action_t pop();

  virtual ~XMLBuilder();

public:
  typedef std::stack <XMLNode *> node_stack_t;

  node_stack_t                   stack_; /**< stack of nodes */

  XMLNode*                       current_node_ = nullptr; /**< The current node */

  XMLDocument::uptr_t            merge_;

  typedef UniquePtrKeySpecPath::uptr_t uptr_ks;
  std::list<uptr_ks>             delete_ks_;
  bool                           list_delete_ = false;
  // In case of delete, merge operation is not performed
  bool                           merge_op_ = true;
};

/**
 * The XML Traverser object enables building of an XML Node based on arbitary
 * data. The Traverser object works in conjunction with a builder object. The
 * Traverser access the builders methods to build a subtree.
 * Depending on the type of node the traverser encounters, it requests the
 * builder to move to a new child node, to append a child or to move a level up.
 * On any of these operations, the return value from the builder could make the
 * traverser move one or more nodes.
 */

class Traverser {
public:

  /**
   * Constructor - The Traverser is alway constructed with an associated Builder
   */

  Traverser (Builder *builder);

  /**
   * Start traversing a Traverser. The builder is invoked with the first XML
   * traversed. The traversal can be generic and is implemented in the base class
   *
   * @return RW_YANG_NETCONF_OP_STATUS_OK on success
   * @return RW_YANG_NETCONF_OP_STATUS_INVALID_VALUE on builder returning errors
   */
  virtual rw_yang_netconf_op_status_t
      traverse();

  /**
   * Process the next operation on the traverser. This is a pure virtual
   * and depending on the state of the concrete traverser, appropriate builder
   * functions are called
   */

  virtual rw_xml_next_action_t
      build_next() = 0;

  /**
   * Skip the childern of the current node, and then process the next operation on
   * the traverser. This is a pure virtual function.
   * Depending on the state of the concrete traverser, appropriate builder
   * functions are called
   */

  virtual rw_xml_next_action_t
      build_next_sibling() = 0;

  /**
   * Stop the traversal, if the builder or calls to other build routines so instructs.
   */

  virtual rw_yang_netconf_op_status_t
      stop() = 0;

  /**
   * Abort the traversal, if the builder so instructs.
   */

  virtual rw_yang_netconf_op_status_t
      abort_traversal() = 0;

  /**
   * Set the log callback for the xml manager
   */

  virtual void set_log_cb (rw_log_cb cb, void *user_data)
  { log_cb_ = cb; log_cb_user_data_ = user_data;}

  /**
   * Get the rwmemlog instance pointer.
   */
  rwmemlog_instance_t* get_memlog_inst()
  {
    return memlog_inst_;
  }

  /**
   * Get the memlog buffer for XMLMgr
   */
  rwmemlog_buffer_t** get_memlog_ptr()
  {
    return &memlog_buf_;
  }

  /**
   * Destructor -
   */
  virtual ~Traverser();

public:
  Builder *builder_;
  rw_log_cb log_cb_ = nullptr;
  void *log_cb_user_data_ = nullptr;

protected:
  /*!
   * The rwmemlog instance. The owner of the xmlmgr, if
   * required has to register it with dts for config or get
   */
  RwMemlogInstance memlog_inst_;

  /*!
   * The rwmemlog buffer used by this mgr instance for tracing.
   */
  RwMemlogBuffer memlog_buf_;

};

/**
 * A Traverser for Riftware XML.
 */

class XMLTraverser : public Traverser
{

public:

  /**
   * Construct a confd traverser given a builder, and an XML node
   *
   */
  XMLTraverser(Builder *builder, XMLNode *node);

  /// @see Traverser::build_next()
  virtual rw_xml_next_action_t build_next();

  /// @see Traverser::build_next_sibling()
  virtual rw_xml_next_action_t build_next_sibling();

  /// @see Traverser::stop()
  virtual rw_yang_netconf_op_status_t stop();

  /// @see Traverser::abort_traversal()
  virtual rw_yang_netconf_op_status_t abort_traversal();

  /// Destructor
  virtual ~XMLTraverser();


public:
  XMLNode                        *current_node_;
  size_t                         current_index_;
  typedef std::tuple<XMLNode *,size_t> node_state_t;

  typedef std::stack <node_state_t> node_state_stack_t;

  node_state_stack_t                stack_; /**< stack of nodes */
};

/**
 * An error handler class to capture app errors.
 * XML implementation should subclass it.
 */
class XMLErrorHandler
{
 public:
  XMLErrorHandler() { }

  /// Get a string containing the errors and warnings, newline separated
  virtual std::string error_string() = 0;

  /// Clear the contents of the error string or last error
  virtual void clear() = 0;

  /// Handle rw_xml generated error messages
  virtual void handle_app_errors(const char* errstr) = 0;

 protected:
  /// Trivial protected destructor
  virtual ~XMLErrorHandler() { }

  /// String containing the list of errors
  std::ostringstream oss_;
};


} // namespace rw_yang


#endif /* __cplusplus */


__BEGIN_DECLS

int xml2cstruct_set_attr_by_index(ProtobufCMessage *pbcm,
				  int idx,
				  const char *val);

int xml2cstruct_set_attr_by_name(ProtobufCMessage *pbcm,
				 const char *field_name,
				 const char *val);

ProtobufCMessage *xml2cstruct_submsg_by_index(ProtobufCMessage *pbcm,
					      int idx);

ProtobufCMessage *xml2cstruct_submsg_by_name(ProtobufCMessage *pbcm,
					     const char *field_name);

/*****************************************************************************/
// C node APIs

extern const char rw_xml_namespace_none[];
extern const char rw_xml_namespace_any[];
extern const char rw_xml_namespace_node[];


/// C adaptor function: @see XMLNode::obj_release()
void rw_xml_node_release(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::known_yang_node()
bool rw_xml_node_known_yang_node(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_yang_node()
rw_yang_node_t* rw_xml_node_get_yang_node(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::set_yang_node(YangNode* yang_node)
void rw_xml_node_set_yang_node(
  rw_xml_node_t* node,
  rw_yang_node_t* yang_node);

/// C adaptor function: @see XMLNode::get_reroot_yang_node()
rw_yang_node_t* rw_xml_node_get_reroot_yang_node(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::set_reroot_yang_node(YangNode* reroot_yang_node)
void rw_xml_node_set_reroot_yang_node(
  rw_xml_node_t* node,
  rw_yang_node_t* reroot_yang_node);

/// C adaptor function: @see XMLNode::get_descend_yang_node()
rw_yang_node_t* rw_xml_node_get_descend_yang_node(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_text_value()
void rw_xml_node_get_text_value(
  rw_xml_node_t* node,
  char** text_value);

/// C adaptor function: @see XMLNode::set_text_value(const char* text_value)
rw_status_t rw_xml_node_set_text_value(
  rw_xml_node_t* node,
  const char* text_value);

/// C adaptor function: @see XMLNode::get_prefix()
void rw_xml_node_get_prefix(
  rw_xml_node_t* node,
  char** prefix);

/// C adaptor function: @see XMLNode::get_local_name()
void rw_xml_node_get_local_name(
  rw_xml_node_t* node,
  char** local_name);

/// C adaptor function: @see XMLNode::get_name_space()
void rw_xml_node_get_name_space(
  rw_xml_node_t* node,
  char** name_space);

/// C adaptor function: @see XMLNode::get_document()
rw_xml_document_t* rw_xml_node_get_document(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_manager()
rw_xml_manager_t* rw_xml_node_get_manager(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_parent()
rw_xml_node_t* rw_xml_node_get_parent(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_first_child()
rw_xml_node_t* rw_xml_node_get_first_child(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_last_child()
rw_xml_node_t* rw_xml_node_get_last_child(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_next_sibling()
rw_xml_node_t* rw_xml_node_get_next_sibling(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_previous_sibling()
rw_xml_node_t* rw_xml_node_get_previous_sibling(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_first_element()
rw_xml_node_t* rw_xml_node_get_first_element(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::get_children()
rw_xml_node_list_t* rw_xml_node_get_children(
  rw_xml_node_t* node);

// ATTN: C child_begin can not be trivially implemented

// ATTN: C child_end can not be trivially implemented


// ATTN: The C attribute APIs are missing!


/// C adaptor function: @see XMLNode::is_equal(rw_xml_node_t* node)
bool_t rw_xml_node_is_equal(
  rw_xml_node_t* node,
  rw_xml_node_t* other_node);

/// C adaptor function: @see XMLNode::validate()
rw_yang_netconf_op_status_t rw_xml_node_validate(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::find(const char* local_name)
rw_xml_node_t* rw_xml_node_find(
  rw_xml_node_t* node,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLNode::find_value_in_ns(const char* local_name, const char* ns, const char* text_value)
rw_xml_node_t* rw_xml_node_find_value(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_value,
  const char* ns);

/// C adaptor function: @see XMLNode::find_list_by_key(YangNode* list_ynode, XMLNode* lookup_node, XMLNode** found_node)
rw_yang_netconf_op_status_t rw_xml_node_find_list_by_key(
  rw_xml_node_t* node,
  rw_yang_node_t* list_ynode,
  rw_xml_node_t* lookup_node,
  rw_xml_node_t** found_node);


// ATTN: C find_list_by_key_pb_msg is missing

// ATTN: C find_node_by_path is missing


/// C adaptor function: @see XMLNode::append_child(const char* local_name, const char* text_value, const char* ns, const char* prefix)
rw_xml_node_t* rw_xml_node_append_child(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix);

/// C adaptor function: @see XMLNode::append_child(YangNode* yang_node, const char* text_val)
rw_xml_node_t* rw_xml_node_append_child_yang(
  rw_xml_node_t* node,
  rw_yang_node_t* yang_node,
  const char* text_val);

/// C adaptor function: @see XMLNode::import_child(rw_xml_node_t* node, bool_t deep)
rw_xml_node_t* rw_xml_node_import_child(
  rw_xml_node_t* node,
  rw_xml_node_t* import_node,
  rw_yang_node_t *ynode,
  bool_t deep);

/// C adaptor function: @see XMLNode::insert_before(const char* local_name, const char* text_value, const char* ns, const char* prefix)
rw_xml_node_t* rw_xml_node_insert_before(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix);

/// C adaptor function: @see XMLNode::insert_before(YangNode *ynode, const char* text_val)
rw_xml_node_t* rw_xml_node_insert_before_yang(
  rw_xml_node_t*  node,
  rw_yang_node_t* ynode,
  const char* text_val);

/// C adaptor function: @see XMLNode::insert_before(XMLNode *xml_node, bool deep)
rw_xml_node_t* rw_xml_node_import_before(
  rw_xml_node_t* node,
  rw_xml_node_t* import_node,
  rw_yang_node_t* ynode,
  bool_t deep);

/// C adaptor function: @see XMLNode::insert_after(const char* local_name, const char* text_value, const char* ns, const char* prefix)
rw_xml_node_t* rw_xml_node_insert_after(
  rw_xml_node_t* node,
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix);

/// C adaptor function: @see XMLNode::insert_after(YangNode *ynode, const char* text_val)
rw_xml_node_t* rw_xml_node_insert_after_yang(
  rw_xml_node_t*  node,
  rw_yang_node_t* ynode,
  const char* text_val);

/// C adaptor function: @see XMLNode::insert_after(XMLNode *xml_node, bool deep)
rw_xml_node_t* rw_xml_node_import_after(
  rw_xml_node_t* node,
  rw_xml_node_t* import_node,
  bool_t deep);

/// C adaptor function: @see XMLNode::remove_child(rw_xml_node_t*child_node)
bool rw_xml_node_remove_child(
  rw_xml_node_t* node,
  rw_xml_node_t* child_node);

/// C adaptor function: @see XMLNode::merge(XMLNode* node)
rw_yang_netconf_op_status_t rw_xml_node_merge(
  rw_xml_node_t* node,
  rw_xml_node_t* other_node);


// ATTN: C merge pb is missing

// ATTN: C create_node_by_path is missing

// ATTN: C append_node_from_pb_field is missing


/// C adaptor function: @see XMLNode::to_string()
void rw_xml_node_to_string(
  rw_xml_node_t* node,
  char** string);

/// C adaptor function: @see XMLNode::to_string_pretty()
void rw_xml_node_to_string_pretty(
  rw_xml_node_t* node,
  char** string);

/// C adaptor function to CF string: @see XMLNode::to_string()
void rw_xml_node_to_cfstring(
    rw_xml_node_t* node,
    CFMutableStringRef cfstring);

/// C adaptor function: @see XMLNode::to_stdout()
rw_status_t rw_xml_node_to_stdout(
  rw_xml_node_t* node);

/// C adaptor function: @see XMLNode::to_file(const char* file_name)
rw_status_t rw_xml_node_to_file(
  rw_xml_node_t* node,
  const char* file_name);

/// C adaptor function: @see XMLNode::to_pbcm(ProtobufCMessage* top_pbcm)
rw_yang_netconf_op_status_t rw_xml_node_to_pbcm(
  rw_xml_node_t* node,
  ProtobufCMessage* top_pbcm);

/*****************************************************************************/
// C node list APIs

/// C adaptor function: @see XMLNodeList::obj_release()
void rw_xml_node_list_release(
  rw_xml_node_list_t* nodelist);

/// C adaptor function: @see XMLNodeList::length()
size_t rw_xml_node_list_length(
  rw_xml_node_list_t* nodelist);

/// C adaptor function: @see XMLNodeList::find_in_ns(const char* local_name, const char* ns)
rw_xml_node_t* rw_xml_node_list_find(
  rw_xml_node_list_t* nodelist,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLNodeList::at(size_t index)
rw_xml_node_t* rw_xml_node_list_at(
  rw_xml_node_list_t* nodelist,
  size_t index);


/// C adaptor function: @see XMLNodeList::get_document()
rw_xml_document_t* rw_xml_node_list_get_document(
  rw_xml_node_list_t* nodelist);

/// C adaptor function: @see XMLNodeList::get_manager()
rw_xml_manager_t* rw_xml_node_list_get_manager(
  rw_xml_node_list_t* nodelist);


/*****************************************************************************/
// C document APIs

/// C adaptor function: @see XMLDocument::obj_release()
void rw_xml_document_release(
  rw_xml_document_t* doc);

/// C adaptor function: @see XMLDocument::get_manager()
rw_xml_manager_t* rw_xml_document_get_manager(
  rw_xml_document_t* doc);

/// C adaptor function: @see XMLDocument::get_root_node()
rw_xml_node_t* rw_xml_document_get_root_node(
  rw_xml_document_t* doc);

/// C adaptor function: @see XMLDocument::get_root_child_element(const char* local_name, const char* ns)
rw_xml_node_t* rw_xml_document_get_root_child_element(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLDocument::get_elements(const char* local_name, const char* ns)
rw_xml_node_list_t* rw_xml_document_get_elements(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLDocument::is_equal(rw_xml_document_t* other_doc)
bool_t rw_xml_document_is_equal(
  rw_xml_document_t* doc,
  rw_xml_document_t* other_doc);

/// C adaptor function: @see XMLDocument::import_node(rw_xml_node_t* node, bool_t deep)
rw_xml_node_t* rw_xml_document_import_node(
  rw_xml_document_t* doc,
  rw_xml_node_t* node,
  bool_t deep);

/// C adaptor function: @see XMLDocument::import_node(XMLNode* node, YangNode* yang_node, bool_t deep)
rw_xml_node_t* rw_xml_yang_document_import_node_yang(
  rw_xml_document_t* doc,
  rw_xml_node_t* node,
  rw_yang_node_t* ynode,
  bool_t deep);

/// C adaptor function: @see XMLDocument::merge(XMLDocument* other_doc)
rw_yang_netconf_op_status_t rw_xml_document_merge(
  rw_xml_document_t* doc,
  rw_xml_document_t* other_doc);

// ATTN: to_pbcm() is missing!

/// C adaptor function: @see XMLDocument::to_string(const char* local_name, const char* ns)
void rw_xml_document_to_string(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns,
  char** string);

/// C adaptor function: @see XMLDocument::to_string_pretty(const char* local_name, const char* ns)
void rw_xml_document_to_string_pretty(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns,
  char** string);

/// C adaptor function: @see XMLDocument::to_string(const char* local_name, const char* ns)
void rw_xml_document_to_cfstring(
  rw_xml_document_t* doc,
  CFMutableStringRef  cfstring);

/// C adaptor function: @see XMLDocument::to_stdout(const char* local_name, const char* ns)
rw_status_t rw_xml_document_to_stdout(
  rw_xml_document_t* doc,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLDocument::to_file(const char* file_name, const char* local_name, const char* ns)
rw_status_t rw_xml_document_to_file(
  rw_xml_document_t* doc,
  const char* file_name,
  const char* local_name,
  const char* ns);



/*****************************************************************************/
// C manager APIs

/**
 * Create an XML manager based on xerces, with a specified yang model.
 *
 * @return The manager pointer.  The caller takes ownership.
 */
rw_xml_manager_t* rw_xml_manager_create_xerces_model(
  /**
   * The yang model.  May be nullptr.
   */
  rw_yang_model_t* yang_model
);

/**
 * Create an XML manager based on xerces.  The manager will create its
 * own yang model.
 *
 * @return The manager pointer.  The caller takes ownership.
 */
rw_xml_manager_t* rw_xml_manager_create_xerces(void);

/// C adaptor function: @see XMLManager::obj_release()
void rw_xml_manager_release(
  rw_xml_manager_t* mgr);

/// C adaptor function: @see XMLManager::get_yang_model()
rw_yang_model_t* rw_xml_manager_get_yang_model(
  rw_xml_manager_t* mgr);

/// C adaptor function: @see XMLManager::create_document(const char* root_name, const char* ns)
rw_xml_document_t* rw_xml_manager_create_document(
  rw_xml_manager_t* mgr,
  const char* local_name,
  const char* ns);

/// C adaptor function: @see XMLManager::create_document(YangNode* yang_node)
rw_xml_document_t* rw_xml_manager_create_document_yang(
  rw_xml_manager_t* mgr,
  rw_yang_node_t* yang_node);

/// C adaptor function: @see XMLManager::create_document_from_file(const char* file_name, bool_t validate)
rw_xml_document_t* rw_xml_manager_create_document_from_file(
  rw_xml_manager_t* mgr,
  const char* file_name,
  bool_t validate);

/// C adaptor function: @see XMLManager::create_document_from_string(const char* xml_str, bool_t validate)
rw_xml_document_t* rw_xml_manager_create_document_from_string(
  rw_xml_manager_t* mgr,
  const char* xml_str,
  bool_t validate);

/// C adaptor fucntion: @see XMLManager::pb_to_xml(ProtobufCMessage *pbcm, char * xml_string, size_t max_length)
rw_status_t rw_xml_manager_pb_to_xml_cfstring(
  rw_xml_manager_t* mgr,
  void *pbcm,
  CFMutableStringRef  cfstring);

/// C adaptor fucntion: @see XMLManager::pb_to_xml(ProtobufCMessage *pbcm, char * xml_string, size_t max_length)
rw_status_t rw_xml_manager_pb_to_xml(
  rw_xml_manager_t* mgr,
  void *pbcm,
  char * xml_string,
  size_t max_length);

/// C adaptor fucntion: @see XMLManager::pb_to_xml_unrooted(ProtobufCMessage *pbcm, char * xml_string, size_t max_length)
rw_status_t rw_xml_manager_pb_to_xml_unrooted(
  rw_xml_manager_t* mgr,
  void *pbcm,
  char * xml_string,
  size_t max_length);

/// C adaptor fucntion: @see XMLManager::pb_to_xml(ProtobufCMessage *pbcm, char * xml_string, size_t max_length)
/// This function returns the XML string and the callee owns the returned string.
char* rw_xml_manager_pb_to_xml_alloc_string(
  rw_xml_manager_t* mgr,
  void *pbcm);

/// C adaptor fucntion: @see XMLManager::pb_to_xml_pretty(ProtobufCMessage *pbcm, char * xml_string, size_t max_length)
/// This function returns the XML string and the callee owns the returned string.
/// Returned string is pretty printed only when the param pretty_print is set to
/// 1.
char* rw_xml_manager_pb_to_xml_alloc_string_pretty(
  rw_xml_manager_t* mgr,
  void *pbcm,
  int pretty_print);

/// C adaptor function: @see XMLManager::xml_to_pb( char * xml_string,ProtobufCMessage *pbcm)
rw_status_t rw_xml_manager_xml_to_pb(
  rw_xml_manager_t* yd_mgr,
  const char * xml_string,
  ProtobufCMessage *pbcm,
  char **errout);
__END_DECLS

/** @} */

#endif
