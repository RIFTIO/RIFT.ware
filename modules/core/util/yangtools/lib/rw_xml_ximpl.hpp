/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_xml_ximpl.hpp
 * @author Vinod Kamalaraj
 * @date 2014/04/09
 * @brief Implementation of the RW XML library based on Xerces
 *
   Who own nodes?
   - xerces node:
     - owned by the xerces parent, if there is one
     - otherwise unowned and deletable
   - ximpl node
     - owned by the xerces node, if there is one
     - otherwise unowned and deletable

ATTN

 * Be wary of Xerces APIs that adopt the objects they are passed,
 * and/or own the objects they return!  Although the library is
 * generally consistent within a set of APIs (e.g.  the DOM* APIs), it
 * is not consistent between sets of APIs (e.g.  DOM and SAX).  Even
 * when a Xerces API adopts a pointer, the pointer should generally be
 * temporarily held in a unique_ptr<>, unless the object is allocated
 * and then passed to an adopting API the immediately following
 * statement.
 */


#ifndef RW_XML_XIMPL_HPP_
#define RW_XML_XIMPL_HPP_


#include <unistd.h>
#include <string>
#include <iosfwd>
#include <iostream>

#include <xercesc/dom/DOM.hpp>

//#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>

//#include <xercesc/framework/XMLFormatter.hpp>
//#include <xercesc/sax/EntityResolver.hpp>

#if 0
#include <xercesc/dom/DOMLSOutput.hpp>
#include <xercesc/dom/DOMLSInput.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#endif

//#include <xercesc/sax2/DefaultHandler.hpp>
#include "xalanc/PlatformSupport/DOMStringHelper.hpp"
#include "xalanc/XalanDOM/XalanDOMString.hpp"
#include <xalanc/XPath/ElementPrefixResolverProxy.hpp>
XALAN_CPP_NAMESPACE_USE; //ATTN don't do this

#include "rw_xml.h"
#include "rw_xml_mixin_yang.hpp"

namespace rw_yang {

class XImplCharStr;
class XImplXMLStr;

class XImplDOMErrorHandler;
class XImplDOMLSResourceResolver;
class XImplDOMUseerDataHandler;
class XImpleDOMNamespaceResolver;
class XMLDocumentXImpl;
class XMLManagerXImpl;
class XMLNodeXImpl;
class XMLNodeListXImpl;
class XMLAttributeListXImpl;
class XMLAttributeXImpl;



/**
 * Xerces object deleter template for static release() function types.
 * This type is used to define an appropriate unique_ptr<> for
 * automagic memory management in RW code.  The unique_ptr<> defined by
 * this class automatically calls release() when necessary.  The
 * primary target of this template is strings.
 *
 * This template can be used only for Xerces types that use a static
 * release() function.  For objects that have a release() member
 * function, see UniqueXrcsPtrObjectRelease<>.  For objects that don't have
 * a release() function, and that that derive from XMemory, just use
 * raw unique_ptr<>.
 */
template <class pointer_type_t, class release_class_t>
struct UniquePtrStaticRelease
{
  void operator()(pointer_type_t* obj) const noexcept
  {
    release_class_t::release(&obj);
  }
  typedef std::unique_ptr<pointer_type_t,UniquePtrStaticRelease> uptr_t;
};

/**
 *  Xerces object deleter template for types with a release() member function.
 *  This template is designed for use with unique_ptr<> types for
 *  automagic memory management in RW code.  The unique_ptr<> defined by
 *  this class automatically calls release() when necessary.
 */

template <class pointer_type_t>
struct UniquePtrXrcsObjectRelease
{
  void operator()(pointer_type_t* obj) const
  {
    obj->release();
  }
  typedef std::unique_ptr<pointer_type_t,UniquePtrXrcsObjectRelease> uptr_t;
};

typedef UniquePtrStaticRelease<XMLCh,XERCES_CPP_NAMESPACE::XMLString>::uptr_t UPtr_XMLString;
typedef UniquePtrStaticRelease<char,XERCES_CPP_NAMESPACE::XMLString>::uptr_t UPtr_CharString;


// Xerces release() member function types
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::DOMDocument>::uptr_t UPtr_DOMDocument;
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::DOMNode>::uptr_t UPtr_DOMNode;
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::DOMLSSerializer>::uptr_t UPtr_DOMLSSerializer;
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::DOMLSOutput>::uptr_t UPtr_DOMLSOutput;
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::DOMLSParser>::uptr_t UPtr_DOMLSParser;
typedef UniquePtrXrcsObjectRelease<XERCES_CPP_NAMESPACE::Wrapper4InputSource>::uptr_t UPtr_Wrapper4InputSource;


// Plain unique_ptr types (types derived from XERCES_CPP_NAMESPACE:XMemory)
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::InputSource> UPtr_InputSource;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::XMLFormatTarget> UPtr_XMLFormatTarget;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::MemBufFormatTarget> UPtr_MemBufFormatTarget;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::StdOutFormatTarget> UPtr_StdOutFormatTarget;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::LocalFileFormatTarget> UPtr_LocalFileFormatTarget;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::MemBufInputSource> UPtr_MemBufInputSource;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::LocalFileInputSource> UPtr_LocalFileInputSource;
typedef std::unique_ptr<XERCES_CPP_NAMESPACE::DOMNamedNodeMap> UPtr_XMLAttributeList;



/**
 * Utility class that transcodes a char string to a XMLCh string and
 * automatically converts to a const XMLCh*.  Objects of this type are
 * intended for use as temporaries, in places where you start with a
 * char string and need a XMLCh string as a Xerces argument.
 *
 * @code
 *   char* cstr = "string";
 *   xerces_object->api(XImplXMLStr(cstr));
 * @endcode
 */
class XImplXMLStr
{
 public:
  /**
   * Default construct an empty XML string.
   */
  XImplXMLStr()
  : xmlstr_(XERCES_CPP_NAMESPACE::XMLString::transcode(""))
  {}

  /**
   * Construct a temporary XML string.
   */
  explicit XImplXMLStr(
    /// [in] char string to transcode.
    const char* s
  )
  : xmlstr_(s ? XERCES_CPP_NAMESPACE::XMLString::transcode(s) : XERCES_CPP_NAMESPACE::XMLString::transcode(""))
  {}

  // Copy-construct a XML string
  explicit XImplXMLStr(
    /// [in] String to copy.
    const XImplXMLStr& other)
  : xmlstr_(XERCES_CPP_NAMESPACE::XMLString::replicate(other.xmlstr_.get()))
  {}

  // Copy-assign a XML string
  XImplXMLStr& operator = (
    /// [in] String to copy.
    const XImplXMLStr& other)
  {
    xmlstr_.reset(XERCES_CPP_NAMESPACE::XMLString::replicate(other.xmlstr_.get()));
    return *this;
  }

  // Copy-assign a char string
  XImplXMLStr& operator = (
    /// [in] char string to transcode.
    const char* s)
  {
    xmlstr_.reset(s ? XERCES_CPP_NAMESPACE::XMLString::transcode(s) : XERCES_CPP_NAMESPACE::XMLString::transcode(""));
    return *this;
  }

  // Copy-assign a XML string
  XImplXMLStr& operator = (
    /// [in] XML string to replicate.
    const XMLCh* xs)
  {
    xmlstr_.reset(XERCES_CPP_NAMESPACE::XMLString::replicate(xs));
    return *this;
  }

  /// Destroy temporary transcoded string
  ~XImplXMLStr()
  {}

  /// Convert to a const XMLCh*
  operator const XMLCh*() const
  {
    return xmlstr_.get();
  }

 private:
  /// The transcoded string
  UPtr_XMLString xmlstr_;
};



/**
 * Utility class that transcodes a XMLCh string to a char string and
 * automatically converts to std::string and const char*.  Objects of
 * this type are intended for use as temporaries, in places where you
 * start with a XMLCh string and need a char string, such as Xerces
 * return values.
 *
 * @code
 *   std::string XERCES_CPP_NAMESPACE::api()
 *   {
 *     return XImplCharStr(xerces_object->api());
 *   }
 * @endcode
 */
class XImplCharStr
{
 public:
  /**
   * Default construct an empty char string.
   */
  XImplCharStr()
  : charstr_(strdup(""))
  {}

  /**
   * Construct a temporary char string.
   * @param s [in] char string to transcode
   */
  explicit XImplCharStr(
    /// [in] XML string to transcode.
    const XMLCh* xs)
      : charstr_(xs ? XERCES_CPP_NAMESPACE::XMLString::transcode(xs) : new char(0))
  {}

  // Copy-construct a XML string
  explicit XImplCharStr(
    /// [in] String to copy.
    const XImplCharStr& other)
  : charstr_(strdup(other.charstr_.get()))
  {}

  // Copy-assign a char string
  XImplCharStr& operator = (
    /// [in] String to copy.
    const XImplCharStr& other)
  {
    charstr_.reset(strdup(other.charstr_.get()));
    return *this;
  }

  // Copy-assign a XML string
  XImplCharStr& operator = (
    /// [in] char string to transcode.
    const XMLCh* xs)
  {
    charstr_.reset(xs ? XERCES_CPP_NAMESPACE::XMLString::transcode(xs) : strdup(""));
    return *this;
  }

  /// Destroy temporary transcoded string
  ~XImplCharStr()
  {}

  /// Convert to a const char*
  operator const char*() const
  {
    return charstr_.get();
  }

  /// Convert to a std::string
  operator std::string() const
  {
    return charstr_.get();
  }

 private:
  /// The transcoded string
  UPtr_CharString charstr_;
};



/**
 * Callback object for user data set on Xerces objects.  The callbacks
 * are made whenever a node tree modification operation occurs -
 * create, delete, adopt, etc...
 *
 * One of these objects is shared by all nodes in a document.
 *
 * ATTN: The callback made by this class needs to be dispatched to the
 * document, so that the behavior can be customized by the derived
 * document classes!
 */
class XImplDOMUserDataHandler
: public XERCES_CPP_NAMESPACE::DOMUserDataHandler
{
 public:

  /**
   * Construct a callback handler.
   */
  XImplDOMUserDataHandler(
    XMLDocumentXImpl& xi_document ///< The owning document.
  )
  : xi_document_(xi_document)
  {}

  /**
   * Destroy a callback handler.
   */
  virtual ~XImplDOMUserDataHandler()
  {}

  /**
   * Handle a node tree callback.
   */
  void handle(DOMOperationType operation,
              const XMLCh* const key,
              void* v_xi_node,
              const XERCES_CPP_NAMESPACE::DOMNode* xrcs_node,
              XERCES_CPP_NAMESPACE::DOMNode* xrcs_other_node);

 protected:
  /// The owning document.
  XMLDocumentXImpl& xi_document_;
};

/**
 * Callback object for user data set on Xerces attribute object.  The callbacks
 * are made whenever a node tree modification operation occurs -
 * create, delete, adopt, etc...
 *
 * One of these objects is shared by all attribute objects  in a document.
 *
 * ATTN: The callback made by this class needs to be dispatched to the
 * document, so that the behavior can be customized by the derived
 * document classes!
 */
class XImplDOMAttributeUserDataHandler
: public XERCES_CPP_NAMESPACE::DOMUserDataHandler
{
 public:

  /**
   * Construct a callback handler.
   */
  XImplDOMAttributeUserDataHandler(
    XMLDocumentXImpl& xi_document ///< The owning document.
  )
  : xi_document_(xi_document)
  {}

  /**
   * Destroy a callback handler.
   */
  virtual ~XImplDOMAttributeUserDataHandler()
  {}

  /**
   * Handle a node tree callback.
   */
  void handle(DOMOperationType operation,
              const XMLCh* const key,
              void* v_xi_attribute,
              const XERCES_CPP_NAMESPACE::DOMNode* xrcs_node,
              XERCES_CPP_NAMESPACE::DOMNode* xrcs_other_node);

 protected:
  /// The owning document.
  XMLDocumentXImpl& xi_document_;
};

/// XML Attribute for Xerces
/**
 * XML Attribute for the xerces DOM implementation.  This object gets stored
 * in a pointer saved in the owning xerces node; therefore, the xerces library
 * is considered to own these objects.
 */
class XMLAttributeXImpl
: public XMLAttribute
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLAttributeXImpl>::uptr_t uptr_t;

 public:
  /**
   * Constructor.  The xerces node takes ownership of this object.
   */
  XMLAttributeXImpl(
    XMLDocumentXImpl& xi_document ///< The owning document
  );

  /**
   * Bind this attribute to a xerces attribute.
   */
  template <class uptr_t>
  void bind_xrcs_attribute(
    uptr_t&& uptr, ///< [in] unique_ptr<> holding ownership.  Ownership moves to xrcs_attribute
    XERCES_CPP_NAMESPACE::DOMAttr* xrcs_attribute ///< [in] The xerces attribute to bound to
  ) {
    bind_xrcs_attribute(xrcs_attribute);
    uptr.release();
  }

protected:

  /**
   * Bind this attribute to a xerces attribute.
   * This call does not change ownership
   */
  void bind_xrcs_attribute(XERCES_CPP_NAMESPACE::DOMAttr* xrcs_attribue);

public:

  /**
   * Release the attribute.  This release comes from user code, and it
   * should only happen after a attribute has been removed from the
   * document, or if it was never inserted into the document in the
   * first place.
   *
   * @see XMLNode::obj_release()
   *
   * ATTN: This cannot destroy the attribute directly; rather, it should
   * release the xerces attribute, which will make a callback that causes
   * this attribute to be deleted.
   *
   * ATTN: Alternatively, this call could delete the xerces
   * association, delete itself, and then release the xerces attribute.  Or,
   * do the same operations in some other order...
   */
  void obj_release() noexcept override;

  /// @see XMLAttribute::get_text_value()
  std::string get_text_value() override;

  /// @see XMLAttribute::get_text_value()
  bool get_specified () const override;

  /// @see XMLAttribute::get_value()
  std::string get_value () override;

  /// @see XMLAttribute::set_value()
  void set_value (std::string value) override;

  /// @see XMLAttribute::get_prefix()
  std::string get_prefix() override;

  /// @see XMLAttribute::get_local_name()
  std::string get_local_name() override;

  /// @see XMLAttribute::get_name_space()
  std::string get_name_space() override;

  /// @see XMLAttribute::get_document()
  XMLDocument& get_document() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_document() due to class definition
   * order.
   * @return The xerces-based document reference.
   */
  virtual XMLDocumentXImpl& get_xi_document();

  /// @see XMLAttribute::get_manager()
  XMLManager& get_manager() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_manager() due to class definition
   * order.
   * @return The xerces-based document reference.
   */
  virtual XMLManagerXImpl& get_xi_manager();

 protected:
  /**
   * Xerces released the attribute.  This attribute should be deleted.
   */
  virtual void xrcs_release() noexcept;

  /**
   * Destructor.  Protected because this object should only ever be
   * destroyed through the obj_release() function, or through a xerces
   * callback.
   */
  ~XMLAttributeXImpl();

  XMLAttributeXImpl (const XMLAttributeXImpl &) = delete;
  XMLAttributeXImpl& operator = (const XMLAttributeXImpl&) = delete;

 protected:
  /// Document that this node is part of
  XMLDocumentXImpl& xi_document_;

  /// Xerces attribute that belongs to this object
  XERCES_CPP_NAMESPACE::DOMAttr* xrcs_attribute_;

  friend class XImplDOMAttributeUserDataHandler;
  friend class XMLNodeXImpl;
  friend class XMLAttributeListXImpl;
};

/// Xerces XML Attribute List
/**
 * Xerces-specific implementation of RW XML attribute list. The xerces attribute
 * list is owned by the xerces document and never needs to be managed,
 * and there is no API for binding the RW attribute list abstraction to the
 * Xerces attribute list.  Therefore, the users of the RW XML library must
 * manage the RW XML attribute List memory.
 *
 * ATTN: The underlying API allocates an attribute list for each attribute, and
 * the list lives until either the attribute or the document are destroyed!
 * These are bad data structures to create on long-lived DOMs!
 *
 */
class XMLAttributeListXImpl
: public XMLAttributeList
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLAttributeListXImpl>::uptr_t uptr_t;

 public:
  /**
   * Construct a xerces-implementation attribute list.  The creator becomes
   * the owner of the node list.
   */
  XMLAttributeListXImpl(
    XMLDocumentXImpl& xi_document_ ///< The owning document
  );

  /**
   * Initialize the xerces node list.
   */
  void initialize_xrcs_attribute_list(
    XERCES_CPP_NAMESPACE::DOMNamedNodeMap* xrcs_attribute_list_ ///< The xerces attribute list. It remains owned by the xerces document
  );

  /// @see XMLNodeList::obj_release()
  void obj_release() noexcept override;

  /// Destroy a xerces-implementation attribute list.
  ~XMLAttributeListXImpl() {}

 protected:

  // Cannot Copy
  XMLAttributeListXImpl (const XMLAttributeListXImpl &) = delete;
  XMLAttributeListXImpl& operator = (const XMLAttributeListXImpl &) = delete;

 public:
  /// @see XMLAttributeList::length()
  size_t length() override;

  /// @see XMLAttributeList::find()
  XMLAttributeXImpl* find(const char* local_name, const char* ns) override;

  /// @see XMLAttributeList::at()
  XMLAttributeXImpl* at(size_t index) override;

  /// @see XMLAttributeList::get_document()
  XMLDocument& get_document() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_document() due to class definition
   * order.
   *
   * @return The xerces-based document pointer.
   */
  virtual XMLDocumentXImpl& get_xi_document();

  /// @see XMLAttributeList::get_manager()
  XMLManager& get_manager() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_manager() due to class definition
   * order.
   *
   * @return The xerces-based document pointer.
   */
  virtual XMLManagerXImpl& get_xi_manager();

 protected:
  /// The owning document.
  XMLDocumentXImpl& xi_document_;

  /// The xerces node list.  Owned by the xerces document.
  XERCES_CPP_NAMESPACE::DOMNamedNodeMap* xrcs_attribute_list_;

  friend class XImplDocument;
};

/// XML Node for xerces.
/**
 * XML Node for the xerces DOM implementation.  This object gets stored
 * in a pointer saved in the xerces node; therefore, the xerces library
 * is considered to own these objects.
 */
class XMLNodeXImpl
: public XMLNodeMixinYang<XMLNode>
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLNodeXImpl>::uptr_t uptr_t;


 public:
  /**
   * Constructor.  The xerces node takes ownership of this object.
   */
  XMLNodeXImpl(
    XMLDocumentXImpl& xi_document ///< The xerces document that owns this node
  );

  /**
   * ATTN: This is not needed anymore because the diamond is gone?
   * Initialize a new XML node of any XImpl-derived type with a xerces
   * node.
   *
   * @return true if successful.
   */
  bool initialize_xrcs_node(
    const char* local_name, ///< [in] The local name of the new node
    const char* text_val, ///< [in] The text optional value to assign to the node
    const char* ns, ///< [in] The node's namespace
    const char* prefix ///< [in] The optional prefix
  );

  /**
   * ATTN: This is not needed anymore because the diamond is gone?
   * Initialize a new XML node of any XImpl-derived type with a
   * complete copy of another xerces subtree.
   *
   * @return true if successful.
   */
  bool initialize_xrcs_import(
    XMLNodeXImpl* other_xi_node, ///< [in] The node to import
    bool deep ///< [in] Deep import - if true, import all the descendents as well
  );

  /**
   * Bind this node to a xerces node, transferring ownership of this
   * object to the xerces node.
   */
  template <class uptr_t>
  void bind_xrcs_node(
    uptr_t&& uptr, /// [in] unique_ptr<> holding ownership.  Ownership moves to xrcs_node.
    XERCES_CPP_NAMESPACE::DOMNode* xrcs_node /// [in] The xerces node to bind to.
  ) {
    bind_xrcs_node(xrcs_node);
    uptr.release();
  }

  /**
   * Set the namespace prefix to the underlying xerces node.
   */
  void xrcs_set_prefix(
    const char* prefix   /// [in] Prefix to be set on the xerces node
  );

 protected:
  /**
   * Bind this node to a xerces node.
   */
  void bind_xrcs_node(
    XERCES_CPP_NAMESPACE::DOMNode* xrcs_node /// [in] The xerces node to bind to.
  );

 public:
  /**
   * Release the node.  This release comes from user code, and it
   * should only happen after a node has been removed from the
   * document, or if it was never inserted into the document in the
   * first place.
   *
   * @see XMLNode::obj_release()
   *
   * ATTN: This cannot destroy the node directly; rather, it should
   * release the xerces node, which will make a callback that causes
   * this node to be deleted.
   *
   * ATTN: Alternatively, this call could delete the xerces
   * association, delete itself, and then release the xerces node.  Or,
   * do the same operations in some other order...
   */
  void obj_release() noexcept override;

 protected:
  /**
   * Xerces released the node.  This node should be deleted.
   */
  void xrcs_release() noexcept;

  /**
   * Destructor.  Protected because this object should only ever be
   * destroyed through the obj_release() function, or through a xerces
   * callback.
   */
  ~XMLNodeXImpl();

  // Cannot copy
  XMLNodeXImpl (const XMLNodeXImpl &) = delete;
  XMLNodeXImpl& operator = (const XMLNodeXImpl&) = delete;


 public:
  // Node information API implementations

  std::string get_text_value() override;
  rw_status_t set_text_value(const char* value) override;
  std::string get_prefix() override;
  std::string get_local_name() override;
  std::string get_name_space() override;
  XMLDocument& get_document() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_document() due to class definition
   * order.
   *
   * @return The xerces-based document reference.
   */
  XMLDocumentXImpl& get_xi_document();

  XMLManager& get_manager() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_manager() due to class definition
   * order.
   *
   * @return The xerces-based document reference.
   */
  XMLManagerXImpl& get_xi_manager();


 public:
  // Tree traversal interface implementations

  XMLNodeXImpl* get_parent() override;
  XMLNodeXImpl* get_first_child() override;
  XMLNodeXImpl* get_last_child() override;
  XMLNodeXImpl* get_next_sibling() override;
  XMLNodeXImpl* get_previous_sibling() override;
  XMLNodeXImpl* get_first_element() override;
  XMLNodeList::uptr_t get_children() override;

  /**
   * Get the children node list, fully-typed for the xerces
   * implementation.  Useful when you have to iterate over nodes and
   * then access the xerces data structures directly.
   *
   * @return The xerces-based node list.
   */
  UniquePtrXMLObjectRelease<XMLNodeListXImpl>::uptr_t xi_get_children();


 public:
  // Attribute interface implementations

  bool supports_attributes() override;
  XMLAttributeList::uptr_t get_attributes() override;

  /**
   * Get the attribute list for this node, fully-typed for the xerces
   * implementation.  Useful when you have to iterate over attributes
   * for a node and then access the xerces data structures directly.
   *
   * @see XMLNode::get_attributes()
   */
  XMLAttributeListXImpl::uptr_t xi_get_attributes();

  bool has_attributes() override;
  bool has_attribute(
    const char* localname,
    const char* ns = nullptr
  ) override;
  XMLAttribute::uptr_t get_attribute(
    const char* localname,
    const char* ns = nullptr
  ) override;

  std::string get_attribute_value(
    const char* localname,
    const char* ns = nullptr
  ) override;

  void remove_attribute(
    XMLAttribute* attribute
  ) override;
  void set_attribute(
    const char* name,
    const char* value,
    const char* ns = nullptr,
    const char* prefix = nullptr
  ) override;

 public:
  // Comparison interface implementations

  bool is_equal(XMLNode* node) override;

  /**
   * Determine if two nodes are equal, xerces specific implementation.
   * @see XMLNode::is_equal()
   */
  bool is_equal(
    XMLNodeXImpl* xi_node ///< The node to compare against
  );


 public:
  // Search interface implementations

  XMLNodeXImpl* find(const char* local_name, const char* ns) override;
  std::unique_ptr<std::list<XMLNode*>> find_all(const char* local_name, const char* ns) override;
  size_t count(const char* local_name, const char* ns) override;  
  XMLNodeXImpl* find_value(const char* local_name, const char* value, const char* ns) override;

  bool equals(const char* local_name, const char* ns);


 public:
  // Tree modification interface implementations

  /**
   * Append a new node to the children list, xerces specific
   * implementation.
   *
   * @see XMLNode::append_child()
   */
  rw_status_t append_child(
    XMLNodeXImpl::uptr_t* new_xi_node ///< The node to append.
  );

  /**
   * Add a new node to the children list, xerces specific
   * implementation. The difference between add_child and append_child is that,
   * in add_child() the child will be added in yang order for rpc-input/output
   * and the keys will be added to the beginning.
   *
   * @see XMLNode::add_child()
   */
  rw_status_t add_child(XMLNodeXImpl::uptr_t* new_xi_node);

  XMLNodeXImpl* add_child(const char* local_name,
                          const char* text_val = nullptr,
                          const char* ns = nullptr,
                          const char* prefix = nullptr) override;
  XMLNodeXImpl* add_child( YangNode* yang_node, const char* text_val = nullptr) override;

  XMLNodeXImpl* import_child(XMLNode* other_node, YangNode* yang_node, bool deep) override;

  /**
   * Insert a new node, as a sibling to this node, before this node in
   * the parent's children node list, xerces specific implementation.
   *
   * @see XMLNode::insert_before()
   */
  rw_status_t insert_before(
    XMLNodeXImpl::uptr_t* new_xi_node ///< The node to insert
  );


  XMLNodeXImpl* insert_before(const char* local_name,
                              const char* text_val = nullptr,
                              const char* ns = nullptr,
                              const char* prefix = nullptr) override;
  XMLNodeXImpl* insert_before(YangNode* yang_node, const char* text_val = nullptr) override;
  XMLNodeXImpl* import_before(XMLNode* other_node, YangNode *ynode, bool deep) override;

  /**
   * Insert a new node, as a sibling to this node, after this node in
   * the parent's children node list, xerces specific implementation.
   *
   * @see XMLNode::insert_after()
   */
  rw_status_t insert_after(
    XMLNodeXImpl::uptr_t* new_xi_node ///< The node to insert
  );

  XMLNodeXImpl* insert_after(const char* local_name,
                             const char* text_val = nullptr,
                             const char* ns = nullptr,
                             const char* prefix = nullptr) override;
  XMLNodeXImpl* insert_after(YangNode* yang_node, const char* text_val = nullptr) override;
  XMLNodeXImpl* import_after(XMLNode* other_node, YangNode* ynode, bool deep) override;

  /**
   * Remove the specified child of this node, xerces specific
   * implementation.
   *
   * @see XMLNode::remove_child()
   */
  bool remove_child(
    XMLNodeXImpl* child_xi_node ///< The child to remove.
  );

  bool remove_child(XMLNode* child_node) override;


 public:
  // Serialization interface implementations

  std::string to_string() override;
  std::string to_string_pretty() override;
  rw_status_t to_stdout() override;
  rw_status_t to_file(const char* file_name) override;

  /**
   * Output the node via a xerces format target; it could be going to
   * memory, a file, or stdout.
   *
   * ATTN: need to capture errors somehow
   *
   * @return RW_STATUS_SUCCESS on success.
   */
  rw_status_t to_target(
    XERCES_CPP_NAMESPACE::XMLFormatTarget* target, ///< The format target to use
    bool pretty_print ///< Pretty-print the output
  );

 protected:
  /**
   * Internal method used to add key elements to the beginning of the XML.
   *
   * @return RW_STATUS_SUCCESS on success.
   */
  rw_status_t add_key_child(
                YangNode* yang_node, ///< YangNode of this XML node
                XMLNodeXImpl::uptr_t* new_xi_node ///< Node to be added
              ); 


 protected:
  /// Document that this node is part of
  XMLDocumentXImpl& xi_document_;

  /// Xerces node that belongs to this object
  XERCES_CPP_NAMESPACE::DOMNode* xrcs_node_;

  friend class XImplDOMUserDataHandler;
  friend class XMLDocumentXImpl;
  friend class XMLNodeListXImpl;
};



/// Xerces XML Node List
/**
 * Xerces-specific implementation of RW XML node list. The xerces node
 * list is owned by the xerces document and never needs to be managed,
 * and there is no API for binding the RW node list abstraction to the
 * Xerces node list.  Therefore, the users of the RW XML library must
 * manage the RW XML Node List memory.
 *
 * ATTN: The underlying API allocates a node list for each node, and
 * the list lives until either the node or the document are destroyed!
 * These are bad data structures to create on long-lived DOMs!
 *
 * ATTN: The underlying API is horribly inefficient for node-children
 * node lists!  We should avoid this API, or implement our own.
 */
class XMLNodeListXImpl
: public XMLNodeList
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLNodeListXImpl>::uptr_t uptr_t;

 public:
  /**
   * Construct a xerces-implementation node list.  The creator becomes
   * the owner of the node list.
   */
  XMLNodeListXImpl(
    XMLDocumentXImpl& xi_document_ ///< The owning document
  );

  /**
   * Initialize the xerces node list.
   */
  void initialize_xrcs_node_list(
    XERCES_CPP_NAMESPACE::DOMNodeList* xrcs_node_list ///< The xerces node list. It remains owned by the xerces document
  );

  /// @see XMLNodeList::obj_release()
  void obj_release() noexcept override;

 protected:

  /// Destroy a xerces-implementation node list.
  ~XMLNodeListXImpl()
  {}

  // Cannot Copy
  XMLNodeListXImpl (const XMLNodeListXImpl &) = delete;
  XMLNodeListXImpl& operator = (const XMLNodeListXImpl &) = delete;

 public:
  /// @see XMLNodeList::length()
  size_t length() override;

  /// @see XMLNodeList::find_in_ns()
  XMLNodeXImpl* find(const char* node_name, const char* ns) override;

  /// @see XMLNodeList::at()
  XMLNodeXImpl* at(size_t index) override;

  /// @see XMLNodeList::get_document()
  XMLDocument& get_document() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_document() due to class definition
   * order.
   *
   * @return The xerces-based document pointer.
   */
  virtual XMLDocumentXImpl& get_xi_document();

  /// @see XMLNodeList::get_manager()
  XMLManager& get_manager() override;

  /**
   * Get the xerces-implemented document.  Sadly, we can't use
   * covariant return types on get_manager() due to class definition
   * order.
   *
   * @return The xerces-based document pointer.
   */
  virtual XMLManagerXImpl& get_xi_manager();


 protected:
  /// The owning document.
  XMLDocumentXImpl& xi_document_;

  /// The xerces node list.  Owned by the xerces document.
  XERCES_CPP_NAMESPACE::DOMNodeList* xrcs_node_list_;


  friend class XImplNode;
  friend class XImplDocument;
};

/**
 * Error handler.  Subclasses the xerces error handler, capturing the
 * errors in a string for the application to use in any way it sees
 * fit.
 *
 * ATTN: This class needs to make callbacks into the Manager, so that
 * the behavior or errors can be tailored by the derived classes.
 */
class XImplDOMErrorHandler
: public XERCES_CPP_NAMESPACE::DOMErrorHandler, public XMLErrorHandler
{
 public:

  /// Constructor to zero-initialize counters.
  XImplDOMErrorHandler()
  : warnings_(0),
    errors_(0),
    fatal_errors_(0)
  {}

  /// Return the number of fatal errors encountered
  unsigned fatal_errors()
  { return fatal_errors_; }

  /// Return the number of non-fatal errors encountered
  unsigned errors()
  { return errors_; }

  /// Return the number of warnings encountered
  unsigned warnings()
  { return warnings_; }

  /// Were any errors encountered?
  bool saw_errors()
  { return errors_ || fatal_errors_; }

  /// Get a string containing the errors and warnings, newline separated
  std::string error_string() override
  { return oss_.str(); }

  void handle_app_errors(const char* errstr) override;

  void clear() override
  { oss_.str(""); errors_ = warnings_ = fatal_errors_ = 0; }

 protected:
  /// Callback from xerces
  bool handleError(const XERCES_CPP_NAMESPACE::DOMError& domError) override;

#if 0
  // ATTN: These are for SAX parser ErrorHandler:
  /// Callback from xerces
  void error(const XERCES_CPP_NAMESPACE::SAXParseException& e) override;

  /// Callback from xerces
  void fatalError(const XERCES_CPP_NAMESPACE::SAXParseException& e) override;

  /// Callback from xerces
  void warning(const XERCES_CPP_NAMESPACE::SAXParseException& e) override;
#endif

 protected:
  /// The number of warnings
  unsigned warnings_;

  /// The number of non-fatal errors
  unsigned errors_;

  /// The number of fatal errors
  unsigned fatal_errors_;
};


/**
 * Xerces-based XML document.
 */
class XMLDocumentXImpl
: public XMLDocument
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLDocumentXImpl>::uptr_t uptr_t;

 public:
  /**
   * Construct a Xerces-implementation RW XML document.
   */
  XMLDocumentXImpl(
    XMLManagerXImpl& xi_manager ///< The owning manager.
  );

  /// @see XMLDocument::obj_release()
  void obj_release() noexcept override;

 protected:

  /// Destroy a Xerces-implementation RW XML document.
  ~XMLDocumentXImpl();

  // Cannot Copy
  XMLDocumentXImpl (const XMLDocumentXImpl &) = delete;
  XMLDocumentXImpl& operator = (const XMLDocumentXImpl &) = delete;

 public:

  /// @see XMLDocument::import_node()
  XMLNode::uptr_t import_node(XMLNode* node,
                              YangNode* yang_node,
                              bool deep) override;

  /**
   * Import a node, xerces specific implementation.
   * @see XMLDocument::import_node()
   */
  XMLNodeXImpl::uptr_t xi_import_node(
    XMLNodeXImpl* xi_node, ///< [in] The node to import
    YangNode* yang_node, ///< [in] The yang node corresponding to this node
    bool deep ///< [in] Deep import-if true, import all the descendents as well
  );

  /// @see XMLDocument::merge(XMLDocument* other_doc)
  rw_yang_netconf_op_status_t merge(XMLDocument* other_doc) override;

  /// @see XMLDocument::to_pbcm()
  rw_yang_netconf_op_status_t to_pbcm(
    ProtobufCMessage* top_pbcm ///< [out] The protobuf message to fill
    );
 public:
  /// @see XMLDocument::get_root_node()
  XMLNodeXImpl* get_root_node() override;

  /// @see XMLDocument::get_root_child_element(const char* local_name, const char* ns)
  XMLNodeXImpl* get_root_child_element(const char* local_name, const char* ns = nullptr) override;

  /// @see XMLDocument::get_elements(const char* local_name, const char* ns)
  XMLNodeList::uptr_t get_elements(const char* local_name, const char* ns = nullptr) override;

  /**
   * Get the list of elements that match a given tag and in an optional
   * namespace, xerces specific implementation.
   *
   * @return The list of matching nodes.
   */
  virtual XMLNodeListXImpl::uptr_t xi_get_elements(
    const char* local_name, /// [in] The local name of the desired nodes (n/i prefix).
    const char* ns = nullptr /// [in] The desired namespace.  May be nullptr - BUT SHOULD BE AVOIDED.
  );

  /// @see XMLDocument::to_string(const char* ns, const char* tag)
  std::string to_string(const char* local_name = nullptr, const char* ns = nullptr) override;

  /// @see XMLDocument::to_string_pretty(const char* ns, const char* tag)
  std::string to_string_pretty(const char* local_name = nullptr, const char* ns = nullptr) override;

  /// @see XMLDocument::to_stdout(const char* ns, const char* tag)
  rw_status_t to_stdout(const char* local_name = nullptr, const char* ns = nullptr) override;

  /// @see XMLDocument::to_file(const char* ns, const char* tag, const char* file_name)
  rw_status_t to_file(const char *file_name, const char* local_name = nullptr, const char* ns = nullptr) override;

  rw_status_t evaluate_xpath(XMLNode * xml_node,      
                      std::string const xpath);

    /// @see XMLDocument::is_equal(XMLDocument* other_doc)
  bool is_equal(XMLDocument* other_doc) override;

  /**
   * Test if two documents are equal, xerces specific implementation.
   * @see XMLDocument::is_equal(XMLDocument* other_doc)
   */
  virtual bool is_equal(XMLDocumentXImpl* other_xi_document);

  /// @see XMLDocument::get_manager()
  XMLManager& get_manager() override;

  /// @see XMLDocument::get_error_handler()
  XMLErrorHandler& get_error_handler() override { return error_handler_; }

  /**
   * Get the xerces-implemented manager.  Sadly, we can't use
   * covariant return types on get_manager() due to class definition
   * order.
   *
   * @return The xerces-based document pointer.
   */
  virtual XMLManagerXImpl& get_xi_manager();


 public:
  /**
   * Initialize the document to be empty, with a root node of the
   * specified name and namespace.
   *
   * @return true on success.
   */
  bool xi_initialize_empty(
    const char* root_name, /// [in] The local name of the root node (with prefix).  May be nullptr.
    const char* ns, /// [in] The namespace.  May be nullptr - BUT SHOULD BE AVOIDED.
    const char* prefix /// [in] Prefix to be added for the namespace. Can be NULL
  );

  /**
   * Initialize the document from the specified file.
   *
   * @return true on success.
   */
  bool xi_initialize_file(
    const char* file_name, ///< The file name. Cannot be nullptr.
    bool validate ///< If true, validate during parsing
  );

  /**
   * Initialize the document from the specified NUL-terminated string.
   *
   * @return true on success.
   */
  bool xi_initialize_string(
    const char* xml_str, ///< The XML blob to read. Cannot be nullptr.
    bool validate ///< If true, validate during parsing
  );

  /**
   * Initialize the document from the xerces input source.
   *
   * @return true on success.
   */
  bool xi_initialize_input_source(
    UPtr_InputSource input_source, ///< The xerces input source.  Function takes ownership.
    bool validate ///< If true, validate during parsing
  );


 protected:
  /**
   * Map a xerces node to the RW XML node that it contains.  The RW XML
   * node will be created if it did not already exist.  This API can be
   * used on any xerces node.
   *
   * @return The RW XML node. Owned by the xerces node.
   */
  virtual XMLNodeXImpl* map_xrcs_node(
    XERCES_CPP_NAMESPACE::DOMNode* xrcs_node ///< The xerces node
  );

  /**
   * Allocate a XMLNodeXImpl.  The dynamic type may be different.
   *
   * @return The new node.  Caller owns the node.
   */
  virtual XMLNodeXImpl::uptr_t alloc_xi_node();

  /**
   * Allocate a XMLNodeListXImpl.  The dynamic type may be different.
   *
   * @return The new node list.  Caller owns the list.
   */
  virtual XMLNodeListXImpl::uptr_t alloc_xi_node_list();

  /**
   * Map a xerces attribute to the correponding RW XML attribute that it contains.
   * The RW XML attribute will be created if it did not already exist.  This API can be
   * used on any xerces attribute.
   *
   * @return The RW XML attribute. Owned by the calller
   */

  virtual XMLAttributeXImpl* map_xrcs_attribute(
    XERCES_CPP_NAMESPACE::DOMAttr* xrcs_attribute ///< The xerces attribute
  );
  /**
   * Allocate a XMLAttributeXImpl.  The dynamic type may be different.
   *
   * @return The new node.  Caller owns the attribute.
   */
  virtual XMLAttributeXImpl::uptr_t alloc_xi_attribute();

  /**
   * Allocate a XMLAttributeListXImpl.  The dynamic type may be different.
   !*
   * @return The new node.  Caller owns the attribute list.
   */
  virtual XMLAttributeListXImpl::uptr_t alloc_xi_attribute_list();

 protected:
  /// The owning manager.
  XMLManagerXImpl& xi_manager_;

  /// Xerces document that belongs to this object.
  UPtr_DOMDocument xrcs_doc_;

  /// Xerces document parser, when the document was parsed from a file.
  /// When this has a valid pointer, xrcs_doc_ is owned by
  /// xrcs_ls_parser_!
  UPtr_DOMLSParser xrcs_ls_parser_;

  /**
   * Callback used in conjunction with xerces node setUserData(), for
   * all nodes in this document.  This object gets callbacks for node
   * activity.
   */
  XImplDOMUserDataHandler user_data_handler_;

  /**
   * Error handler class. Used to handle parser errors and
   * app errors.
   */
  XImplDOMErrorHandler error_handler_;

  /**
   * Callback used in conjunction with xerces attribute setUserData(), for
   * all node attributes in this document.  This object gets callbacks for attribute
   * activity.
   */
   XImplDOMAttributeUserDataHandler user_attribute_data_handler_;


  friend class XImplDOMUserDataHandler;
  friend class XImplDOMAttributeUserDataHandler;
  friend class XMLNodeXImpl;
  friend class XMLNodeListXImpl;
  friend class XMLManagerXImpl;
  friend class XMLAttributeXImpl;
  friend class XMLAttributeListXImpl;
};



/**
 * Xerces-based XML Manager.
 */
class XMLManagerXImpl
: public XMLManager
{
 public:
  typedef UniquePtrXMLObjectRelease<XMLManagerXImpl>::uptr_t uptr_t;

 public:
  /**
   * Construct a Xerces XML Manager.  Also initializes the Xerces
   * platform.
   */
  XMLManagerXImpl(
      YangModel* model = nullptr, ///< [in] The model to use.  May be nullptr.  Must live at least as long as the manager.
      XERCES_CPP_NAMESPACE::DOMImplementation* impl = nullptr
  );

  /// @see XMLManager::obj_release()
  void obj_release() noexcept override;

 protected:
  /**
   * Destroy a Xerces XML Manager.  Also terminates the Xerces
   * platform, if it was the last Xerces XML manager in existence.
   */
  ~XMLManagerXImpl();

  // Cannot Copy
  XMLManagerXImpl (const XMLManagerXImpl &) = delete;
  XMLManagerXImpl& operator = (const XMLManagerXImpl &) = delete;

 public:
  /// @see XMLManager::get_yang_model()
  YangModel* get_yang_model() override
  {
    return yang_model_;
  }

 public:
  /// @see XMLManager::create_document(const char* root_name = nullptr, const char* ns = nullptr)
  XMLDocument::uptr_t create_document(const char* root_name = nullptr, const char* ns = nullptr) override;

  /// @see XMLManagerYang::create_document(YangNode* yang_node)
  XMLDocument::uptr_t create_document(YangNode* yang_node) override;

  /// @see XMLManager::create_document_from_file(const char* file_name, bool validate = true)
  XMLDocument::uptr_t create_document_from_file(const char* file_name, bool validate = true) override;

  /// @see XMLManager::create_document_from_file(const char* file_name, bool validate = true)
  XMLDocument::uptr_t create_document_from_file(const char* file_name, std::string & error_out, bool validate = true) override;

  /// @see XMLManager::create_document_from_string(const char* xml_str, std::string& error_out, bool validate = true)
  XMLDocument::uptr_t create_document_from_string(const char* xml_str, std::string& error_out, bool validate = true) override;

  /// @see XMLManager::create_document_from_pbcm(ProtobufCMessage *pbcm, rw_yang_netconf_op_status_t& err)
  XMLDocument::uptr_t create_document_from_pbcm(
    ProtobufCMessage *pbcm,
    rw_yang_netconf_op_status_t& err,
    bool rooted,
    const rw_keyspec_path_t *keyspec ) override;

  /// @see XMLManager::xml_to_pb(const char *xml_str, ProtobufCMessage *pbcm)
  rw_status_t xml_to_pb(const char *xml_str, ProtobufCMessage *pbcm, std::string& error_out, rw_keyspec_path_t *keyspec = 0) override;

  /// @see XMLManager::pb_to_xml(ProtobufCMessage *pbcm, std::string& xml_string)
  rw_status_t pb_to_xml(ProtobufCMessage *pbcm, std::string& xml_string,rw_keyspec_path_t *keyspec = 0) override;

  /// @see XMLManager::pb_to_xml_pretty(ProtobufCMessage *pbcm, std::string& xml_string)
  rw_status_t pb_to_xml_pretty(ProtobufCMessage *pbcm, std::string& xml_string,rw_keyspec_path_t *keyspec = 0) override;

  /// @see XMLManager::pb_to_xml_unrooted(ProtobufCMessage *pbcm, std::string& xml_string)
  rw_status_t pb_to_xml_unrooted(ProtobufCMessage *pbcm, std::string& xml_string) override;

  /// @see XMLManager::pb_to_xml_unrooted_pretty(ProtobufCMessage *pbcm, std::string& xml_string)
  rw_status_t pb_to_xml_unrooted_pretty(ProtobufCMessage *pbcm, std::string& xml_string) override;
 public:
  // Xerces implementation helpers.  Non-XImpl objects shouldn't really
  // access these APIs; they are public for convenience (in lieu of
  // friendship for distantly related derived class sets).

  /**
   * Create a Xerces DOMLSSerializer, configured for pretty output and
   * with the XML format declaration.
   *
   * @return The serializer.  Owned by the caller.
   */
  UPtr_DOMLSSerializer create_ls_serializer(
    bool pretty_print ///< Pretty-print the output
  );

  /**
   * Create a Xerces DOMLSOutput to use for serialization.
   *
   * @return The output handle.  Owned by the caller.
   */
  UPtr_DOMLSOutput create_ls_output();


 protected:
  /**
   * Allocate a XMLNodeXImpl.  The dynamic type may be different.
   *
   * @return The new document.  Caller owns the document.
   */
  virtual XMLDocumentXImpl::uptr_t alloc_xi_document();

 private:
  /// The number of managers instanciated so we can track when to initialize and terminate xalan
  static size_t manager_count_;

 protected:
  /// DOM load-save impementation. Owned by Xerces.
  XERCES_CPP_NAMESPACE::DOMImplementation* xrcs_ls_impl_;

  /// The model is owned.
  bool yang_model_owned_;

  /// The yang model
  YangModel* yang_model_;

  XImpleDOMNamespaceResolver * ns_resolver_;

  friend class XMLDocumentXImpl;

};






/**
 * Implement a Xerces resource resolver.  We define our own so that we
 * can intercept .xsd lookups, and redirect them to Rift-specific
 * paths.
 *
 * ATTN: YangDom should probably reimplement this to find and load the
 * yang module.
 */
class XImplDOMLSResourceResolver
: public XERCES_CPP_NAMESPACE::DOMLSResourceResolver
{
 public:
  XERCES_CPP_NAMESPACE::DOMLSInput* resolveResource(
    const XMLCh *const resourceType,
    const XMLCh *const namespaceUri,
    const XMLCh *const publicId,
    const XMLCh *const systemId,
    const XMLCh *const baseURI);
};

/**
 * Implements a Xerces namespace resolver for constructing xpath expressions.
 */ 
class XImpleDOMNamespaceResolver
    : public PrefixResolver
{
 private:
  /// Model used to determing prefix->namespace mappings
  YangModel* model_;

  /// Cache of prefix -> namespace mappings
  mutable std::unordered_map<XalanDOMString, XalanDOMString, DOMStringHashFunction> namespace_map_;

  /// Base URI
  XalanDOMString base_uri_;

  /**
   * Find the namespace in the YangModel associated with the prefix and cache it.
   */
  void cache_prefix(XalanDOMString const & prefix) const;

 public:
  XImpleDOMNamespaceResolver() = default;
  ~XImpleDOMNamespaceResolver();
  XImpleDOMNamespaceResolver(YangModel* model);

  ///    Retrieve a namespace corresponding to a prefix. 
  virtual const XalanDOMString* getNamespaceForPrefix (const XalanDOMString &prefix) const;

  ///    Retrieve the base URI for the resolver. More...
  virtual const XalanDOMString& getURI () const;

};

}

#endif
