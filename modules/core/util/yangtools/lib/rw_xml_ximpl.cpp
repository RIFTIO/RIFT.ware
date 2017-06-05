/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_xml_ximpl.cpp
 * @author Vinod Kamalaraj
 * @date 2014/04/09
 * @brief Implementation of the RW XML library based on Xerces
 */

#include <sstream>

#include "rw_xml_ximpl.hpp"
#include <yangncx.hpp>
#include <rw_yang_mutex.hpp>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLURL.hpp>
#include <xalanc/XalanTransformer/XercesDOMWrapperParsedSource.hpp>

#include <xalanc/XSLT/XSLTProcessorEnvSupportDefault.hpp>
#include "xalanc/PlatformSupport/DOMStringHelper.hpp"
#include <xalanc/XalanTransformer/XercesDOMWrapperParsedSource.hpp>
#include <xalanc/Include/PlatformDefinitions.hpp>
#include <xalanc/XalanTransformer/XalanTransformer.hpp>
#include <xalanc/XalanSourceTree/XalanSourceTreeDOMSupport.hpp>
#include <xalanc/XalanSourceTree/XalanSourceTreeParserLiaison.hpp>
#include <xalanc/XPath/XObjectFactoryDefault.hpp>
#include <xalanc/XPath/XPath.hpp>
#include <xalanc/XPath/XPathConstructionContextDefault.hpp>
#include <xalanc/XPath/XPathEnvSupportDefault.hpp>
#include <xalanc/XPath/XPathExecutionContextDefault.hpp>
#include <xalanc/XPath/XPathInit.hpp>
#include <xalanc/XPath/XPathProcessorImpl.hpp>
#include <xalanc/XPath/XPathFactoryDefault.hpp>
#include <xalanc/XPath/ElementPrefixResolverProxy.hpp>
#include "xalanc/XercesParserLiaison/XercesDocumentWrapper.hpp"
using namespace rw_yang;

namespace X = XERCES_CPP_NAMESPACE;

XALAN_CPP_NAMESPACE_USE; //ATTN don't do this 
typedef XPathConstructionContext::GetAndReleaseCachedString     GetAndReleaseCachedString;

static const XMLCh rw_xml_ximpl_bkptr_key[] =
  {'R','W','_','X','M','L','_','X','I','M','P','L','_','B','K','P','T','R','_','K','E','Y'};



/*****************************************************************************/
size_t XMLManagerXImpl::manager_count_ = 0;

XMLManager::uptr_t rw_yang::xml_manager_create_xerces(YangModel* model)
{
  return XMLManager::uptr_t(new XMLManagerXImpl(model));
}

rw_xml_manager_t* rw_xml_manager_create_xerces_model(rw_yang_model_t* yang_model)
{
  return static_cast<XMLManager*>(new XMLManagerXImpl(static_cast<YangModel*>(yang_model)));
}

rw_xml_manager_t* rw_xml_manager_create_xerces()
{
  return static_cast<XMLManager*>(new XMLManagerXImpl());
}

XMLManagerXImpl::XMLManagerXImpl(YangModel *model, X::DOMImplementation* impl)
: xrcs_ls_impl_(impl),
  yang_model_owned_(model == nullptr),
  yang_model_(model)
{
  // Initialize the platform.  Really only happens once.
  {
    GlobalMutex::guard_t guard(GlobalMutex::g_mutex);
    X::XMLPlatformUtils::Initialize();
    if (manager_count_ == 0) {
      XalanTransformer::initialize();
    }
    manager_count_++;

  }

  // ATTN: Create our own memory manager, that uses the special RW
  //   memory APIs to give xerces-objects some kind of identity for
  //   debugging.

  static const XMLCh ls[] = { 'L', 'S', 0 };
  if (!impl) {
    xrcs_ls_impl_ = static_cast<X::DOMImplementation*>(X::DOMImplementationRegistry::getDOMImplementation(ls));
  } 

  if (!yang_model_) {
    yang_model_owned_ = true;
    yang_model_ = YangModelNcx::create_model();
  }
  ns_resolver_ = new XImpleDOMNamespaceResolver(yang_model_);  
}

void XMLManagerXImpl::obj_release() noexcept
{
  delete ns_resolver_;
  delete this;
}

XMLManagerXImpl::~XMLManagerXImpl()
{
  if (yang_model_ && yang_model_owned_) {
    delete yang_model_;
    yang_model_ = nullptr;
  }
  // Terminate the platform.  Really only happens when all XImpls are gone.
  GlobalMutex::guard_t guard(GlobalMutex::g_mutex);

  manager_count_--;
  if (manager_count_ == 0) {
    // Terminate Xalan...
    XalanTransformer::terminate();
  }

  X::XMLPlatformUtils::Terminate();
}

XMLDocument::uptr_t XMLManagerXImpl::create_document(const char* local_name, const char* ns)
{
  XMLDocumentXImpl::uptr_t xi_doc = alloc_xi_document();
  std::stringstream log_strm;

  log_strm << "Creating document " << ns <<":"<<local_name;

  if (! xi_doc->xi_initialize_empty(local_name, ns, nullptr)) {
    // ATTN: Failed to parse!  Where did the errors go?
    // Reset the smart pointer to delete the object.
    xi_doc.reset();
    RW_XML_MGR_LOG_ERROR (log_strm);

  } else {
    // provoke yang lookup
    XMLNodeXImpl* root_node = xi_doc->get_root_node();
    if (nullptr == root_node->get_yang_node()) {
      YangNode *top_node = yang_model_->get_root_node()->search_child (root_node->get_local_name().c_str());
      if (nullptr != top_node) {
        log_strm << " setting top_node to "<< root_node->get_local_name();
        root_node->set_yang_node(top_node);
        root_node->xrcs_set_prefix(top_node->get_prefix());
      } else {
        root_node->set_reroot_yang_node(yang_model_->get_root_node());
      }
    }
    RW_XML_MGR_LOG_DEBUG (log_strm);
  }
  return static_cast_move<XMLDocument::uptr_t>(xi_doc);
}

XMLDocument::uptr_t XMLManagerXImpl::create_document(YangNode* yang_node)
{
  const char* root_name = nullptr;
  const char* ns = nullptr;
  const char* prefix = nullptr;
  if (yang_node) {
    root_name = yang_node->get_name();
    ns = yang_node->get_ns();
    prefix = yang_node->get_prefix();
  }
  std::stringstream log_strm;

  log_strm << "Creating document " << ns <<":"<< root_name;

  XMLDocumentXImpl::uptr_t doc(new XMLDocumentXImpl(*this));
  if (! doc->xi_initialize_empty(root_name, ns, prefix)) {
    // Reset the smart pointer to delete the object, due to the failed init.
    doc.reset();
  } else {
    // Need to bind the input yang_node to the xerces root node, so
    // that tree walks can work correctly.
    XMLNodeXImpl* root_node = doc->get_root_node();
    if (yang_node) {
      log_strm << " setting top_node to "<< root_node->get_local_name();
      root_node->set_yang_node(yang_node);
    } else {
      root_node->set_reroot_yang_node(yang_model_->get_root_node());
    }
    RW_XML_MGR_LOG_DEBUG (log_strm);
  }

  return static_cast_move<XMLDocument::uptr_t>(doc);
}

XMLDocument::uptr_t XMLManagerXImpl::create_document_from_file(const char* file_name, std::string& error_out, bool validate)
{
  RW_ASSERT(file_name);

  std::stringstream log_strm;

  log_strm << "Creating document " << file_name << ":" << validate;

  XMLDocumentXImpl::uptr_t xi_doc = alloc_xi_document();
  if (! xi_doc->xi_initialize_file(file_name, validate)) {
    // ATTN: Failed to parse!  Where did the errors go?
    // Reset the smart pointer to delete the object.
    log_strm << "Failed to parse XML:" << xi_doc->get_error_handler().error_string() << std::endl;
    xi_doc.reset();
    error_out = log_strm.str();
    RW_XML_MGR_LOG_ERROR (log_strm);
  } else {
    // provoke yang lookup
    XMLNodeXImpl* root_node = xi_doc->get_root_node();
    if (nullptr == root_node->get_yang_node()) {
      YangNode *top_node = yang_model_->get_root_node()->search_child (root_node->get_local_name().c_str());
      if (nullptr != top_node) {
        root_node->set_yang_node(top_node);
      } else {
        root_node->set_reroot_yang_node(yang_model_->get_root_node());
      }
    }
    RW_XML_MGR_LOG_DEBUG (log_strm);
  }
  return static_cast_move<XMLDocument::uptr_t>(xi_doc);
}
XMLDocument::uptr_t XMLManagerXImpl::create_document_from_file(const char* file_name, bool validate)
{
  RW_ASSERT(file_name);
  std::string error_out;
  return create_document_from_file(file_name, error_out, validate);
}

XMLDocument::uptr_t XMLManagerXImpl::create_document_from_string(const char* xml_str, std::string& error_out, bool validate)
{
  RW_ASSERT(xml_str);

  std::stringstream log_strm;
  log_strm<<"Create from string ";

  XMLDocumentXImpl::uptr_t xi_doc = alloc_xi_document();
  if (! xi_doc->xi_initialize_string(xml_str, validate)) {
    // ATTN: Failed to parse!  Where did the errors go?
    // Reset the smart pointer to delete the object.
    log_strm << "Failed to parse XML:" << xi_doc->get_error_handler().error_string() << std::endl;
    xi_doc.reset();
    error_out = log_strm.str();
    RW_XML_MGR_LOG_ERROR (log_strm);
  } else {
    // provoke yang lookup
    XMLNodeXImpl* root_node = xi_doc->get_root_node();
    if (nullptr == root_node->get_yang_node()) {
      YangNode *top_node = yang_model_->get_root_node()->search_child (root_node->get_local_name().c_str());
      if (nullptr != top_node) {
        root_node->set_yang_node(top_node);
      } else {
        root_node->set_reroot_yang_node(yang_model_->get_root_node());
      }
    }
  }
  return static_cast_move<XMLDocument::uptr_t>(xi_doc);
}


XMLDocument::uptr_t XMLManagerXImpl::create_document_from_pbcm(
  ProtobufCMessage *pbcm,
  rw_yang_netconf_op_status_t& err,
  bool rooted,
  const rw_keyspec_path_t *keyspec )
{
  if (keyspec) {
    RW_ASSERT(rooted);
    auto doc = create_document(yang_model_->get_root_node());
    err = doc->get_root_node()->merge( keyspec, pbcm );
    return static_cast_move<XMLDocument::uptr_t>(doc);
  }

  // Find the path to the root of the schema.
  XMLDocumentXImpl::uptr_t doc(new XMLDocumentXImpl(*this));
  std::stringstream log_strm;
  RW_ASSERT (pbcm->descriptor);
  log_strm << "Creating document " <<pbcm->descriptor->name;

  const rw_yang_pb_msgdesc_t *y2pb = pbcm->descriptor->ypbc_mdesc;
  RW_ASSERT(y2pb);

  typedef std::tuple<const char*, const char *> elt_t;
  typedef std::stack<elt_t> elt_stack_t;
  elt_stack_t stack;
  const rw_yang_pb_module_t *y_mod = nullptr;
  const char *name = nullptr;

  if (!rooted) {
    y_mod = y2pb->module;
    name = y2pb->yang_node_name;
  }

  while (nullptr != y2pb) {
    stack.push(std::make_tuple (y2pb->module->ns, y2pb->yang_node_name));
    if (rooted && (nullptr == y2pb->parent)) {
      // The module is that of the highest level nodes.
      y_mod = y2pb->module;
      name = y2pb->yang_node_name;
    }
    y2pb = y2pb->parent;
  }

  RW_ASSERT(stack.size());
  RW_ASSERT(y_mod);

  if (!doc->xi_initialize_empty(name, y_mod->ns, nullptr)) {
    // Reset the smart pointer to delete the object, due to the failed init.
    doc.reset();
    log_strm << "Failed to initialized document with "<< y_mod->module_name
             <<":" << y_mod->ns;
    RW_XML_MGR_LOG_ERROR (log_strm);
    return static_cast_move<XMLDocument::uptr_t>(doc);
  }

  XMLNodeXImpl *node = doc->get_root_node();
  YangNode *yn = yang_model_->get_root_node();

  if (rooted) {
    if (strcmp(name, "data")) {
      yn = yn->search_child (name, y_mod->ns);
      RW_ASSERT(yn);
    }
    node->set_yang_node(yn);
    node->xrcs_set_prefix(yn->get_prefix());
    stack.pop();
  }

  while (!stack.empty()) {
    const char *ns = nullptr, *name = nullptr;
    std::tie (ns, name) = stack.top();

    yn = yn->search_child (name, ns);
    RW_ASSERT(yn);
    if (rooted) {
      node = node->add_child(yn);
      RW_ASSERT (node);
    }

    stack.pop();
  }

  if (!rooted) {
    node->set_yang_node(yn);
    node->xrcs_set_prefix(yn->get_prefix());
  }

  // The protobuf has to be merged to the node
  err = node->merge(pbcm);
  if (RW_YANG_NETCONF_OP_STATUS_OK != err) {
    doc.reset();
  }
  return static_cast_move<XMLDocument::uptr_t>(doc);
}


rw_status_t XMLManagerXImpl::pb_to_xml(ProtobufCMessage *pbcm,
                                       std::string& xml_string,
                                       rw_keyspec_path_t *keyspec)
{
  rw_yang_netconf_op_status_t ncrs;
  XMLDocument::uptr_t dom(create_document_from_pbcm (pbcm, ncrs, true/*rooted*/, keyspec));

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
    return RW_STATUS_FAILURE;
  }
  xml_string = dom->to_string();
  return RW_STATUS_SUCCESS;
}

rw_status_t XMLManagerXImpl::pb_to_xml_pretty(ProtobufCMessage *pbcm,
                                       std::string& xml_string,
                                       rw_keyspec_path_t *keyspec)
{
  rw_yang_netconf_op_status_t ncrs;
  XMLDocument::uptr_t dom(create_document_from_pbcm (pbcm, ncrs, true/*rooted*/, keyspec));

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
    return RW_STATUS_FAILURE;
  }
  xml_string = dom->to_string_pretty();
  return RW_STATUS_SUCCESS;
}

rw_status_t XMLManagerXImpl::pb_to_xml_unrooted(ProtobufCMessage *pbcm,
                                       std::string& xml_string)
{
  rw_yang_netconf_op_status_t ncrs;

  XMLDocument::uptr_t dom(create_document_from_pbcm (pbcm, ncrs, false, nullptr));

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
    return RW_STATUS_FAILURE;
  }
  xml_string = dom->to_string();
  return RW_STATUS_SUCCESS;
}

rw_status_t XMLManagerXImpl::pb_to_xml_unrooted_pretty(ProtobufCMessage *pbcm,
                                       std::string& xml_string)
{
  rw_yang_netconf_op_status_t ncrs;

  XMLDocument::uptr_t dom(create_document_from_pbcm (pbcm, ncrs, false, nullptr));

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
    return RW_STATUS_FAILURE;
  }
  xml_string = dom->to_string_pretty();
  return RW_STATUS_SUCCESS;
}

rw_status_t XMLManagerXImpl::xml_to_pb(const char * xml_string,
                                       ProtobufCMessage *pbcm,
                                       std::string& error_out,
                                       rw_keyspec_path_t *keyspec)
{
  // Convert the pbcm to a YANG dom
  // ATTN: Shouldn't really pass false here for validate.  It would be nice to validate

  XMLDocument::uptr_t dom(create_document_from_string(xml_string, error_out, false));

  if (nullptr == dom.get()) {
    return RW_STATUS_FAILURE;
  }
  rw_yang_netconf_op_status_t ncrs = dom->to_pbcm (pbcm);

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncrs) {
    error_out = "Failed XML->PB conversion:" + dom->get_error_handler().error_string();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


UPtr_DOMLSSerializer XMLManagerXImpl::create_ls_serializer(bool pretty_print)
{
  UPtr_DOMLSSerializer serializer(xrcs_ls_impl_->createLSSerializer());
  X::DOMConfiguration *config = serializer->getDomConfig();
  config->setParameter(X::XMLUni::fgDOMWRTFormatPrettyPrint, pretty_print);
  config->setParameter(X::XMLUni::fgDOMXMLDeclaration, true);
  return serializer;
}

UPtr_DOMLSOutput XMLManagerXImpl::create_ls_output()
{
  UPtr_DOMLSOutput output(xrcs_ls_impl_->createLSOutput());
  RW_ASSERT(output.get());

  //static const XMLCh utf8[] = { 'U', 'T', 'F', '-', '8', 0 };
  //output->setEncoding(utf8);
  return output;
}

XMLDocumentXImpl::uptr_t XMLManagerXImpl::alloc_xi_document()
{
  return XMLDocumentXImpl::uptr_t(new XMLDocumentXImpl(*this));
}



/*****************************************************************************/
XMLDocumentXImpl::XMLDocumentXImpl(XMLManagerXImpl& xi_manager)
: XMLDocument(xi_manager.get_memlog_inst()),
  xi_manager_(xi_manager),
  user_data_handler_(*this),
  user_attribute_data_handler_(*this)
{
}

void XMLDocumentXImpl::obj_release() noexcept
{
  if (xrcs_ls_parser_) {
    // If there is a parser, it already owns the xerces doc
    xrcs_doc_.release();
  }
  xrcs_ls_parser_.reset();
  xrcs_doc_.reset();
  delete this;
}

XMLDocumentXImpl::~XMLDocumentXImpl()
{
  RW_ASSERT(!xrcs_ls_parser_);
  RW_ASSERT(!xrcs_doc_);
}

bool XMLDocumentXImpl::xi_initialize_empty(const char* local_name,
                                           const char* ns,
                                           const char* prefix)
{
  RW_ASSERT(!xrcs_ls_parser_);
  RW_ASSERT(!xrcs_doc_);

  XImplXMLStr xml_local_name;
  if (nullptr == local_name) {
    xml_local_name = "data";
  } else {
    xml_local_name = local_name;
  }

  XImplXMLStr xml_ns;
  if (nullptr == ns) {
    xml_ns = "http://riftio.com/ns/riftware-1.0/rw-base";
  } else {
    xml_ns = ns;
  }

  xrcs_doc_.reset(xi_manager_.xrcs_ls_impl_->createDocument(xml_ns, xml_local_name, 0));
  RW_ASSERT(xrcs_doc_);
  xrcs_doc_->setXmlVersion(X::XMLUni::fgVersion1_1);
  xrcs_doc_->setXmlStandalone(true);

  if (prefix && prefix[0] != '\0') {
    X::DOMElement* xrcs_node =  xrcs_doc_->getDocumentElement();
    xrcs_node->setPrefix(XImplXMLStr(prefix));
  }

  return true;
}

bool XMLDocumentXImpl::xi_initialize_file(const char* file_name, bool validate)
{
  RW_ASSERT(file_name);

  // make an InputSource and then pass off to a common function
  return xi_initialize_input_source(
    UPtr_InputSource(new X::LocalFileInputSource(XImplXMLStr(file_name))),
    validate);
}

bool XMLDocumentXImpl::xi_initialize_string(const char* xml_str, bool validate)
{
  RW_ASSERT(xml_str);

  // make an InputSource and then pass off to a common function
  X::MemBufInputSource* membuf = new X::MemBufInputSource((const XMLByte*)xml_str, strlen(xml_str), "/");
  membuf->setCopyBufToStream(false);
  return xi_initialize_input_source(UPtr_InputSource(membuf), validate);
}

bool XMLDocumentXImpl::xi_initialize_input_source(UPtr_InputSource input_source, bool validate)
{
  RW_ASSERT(input_source);
  // Only allowed when there is no document already
  RW_ASSERT(!xrcs_ls_parser_);
  RW_ASSERT(!xrcs_doc_);

  xrcs_ls_parser_.reset(xi_manager_.xrcs_ls_impl_->createLSParser(X::DOMImplementationLS::MODE_SYNCHRONOUS, 0));
  X::DOMConfiguration* config = xrcs_ls_parser_->getDomConfig();

  XImplDOMLSResourceResolver resolver;
  config->setParameter(X::XMLUni::fgDOMResourceResolver, &resolver);

  //  XImplDOMErrorHandler handler;
  error_handler_.clear();
  config->setParameter(X::XMLUni::fgDOMErrorHandler, &error_handler_);

  config->setParameter(X::XMLUni::fgDOMNamespaces, true);
  config->setParameter(X::XMLUni::fgDOMElementContentWhitespace, false);

   // ATTN: validation may become magic - implemented by YangModel
   //   That could require a SAX parser, plus callbacks for each element

  if (validate) {
    // Enable the parser's schema support
    config->setParameter(X::XMLUni::fgXercesSchema, true);
    config->setParameter(X::XMLUni::fgXercesSchemaFullChecking, true);
    config->setParameter(X::XMLUni::fgDOMValidate, true);
  } else {
    // Dont enable the parser's schema support
    config->setParameter(X::XMLUni::fgXercesSchema, false);
    config->setParameter(X::XMLUni::fgXercesSchemaFullChecking, false);
    config->setParameter(X::XMLUni::fgDOMValidate, false);
  }

  // need to wrap the InputSource to get a LSInput.  Adopts input source.
  X::Wrapper4InputSource lsinput(input_source.release());

  // parse the document
  xrcs_doc_.reset(xrcs_ls_parser_->parse(&lsinput));

  // ATTN: proper error handling
  if (error_handler_.saw_errors() || !xrcs_doc_) {
    return false;
  }

  return true;
}

XMLNode::uptr_t XMLDocumentXImpl::import_node(XMLNode* node, YangNode* yang_node, bool deep)
{
  RW_ASSERT(node);
  XMLNodeXImpl* xi_node = dynamic_cast<XMLNodeXImpl*>(node);
  RW_ASSERT(xi_node);

  // ATTN: Should implement a least-common-denomenator API that can
  //   import even if the XML implementations are different

  // Dispatch to common function.  Upcast the smart pointer for return.
  // Caller now owns it until inserted into the document.
  return static_cast_move<XMLNode::uptr_t>(xi_import_node(xi_node, yang_node, deep));
}

XMLNodeXImpl::uptr_t XMLDocumentXImpl::xi_import_node(XMLNodeXImpl* other_xi_node,
                                                      YangNode* yang_node,
                                                      bool deep)
{
  RW_ASSERT(other_xi_node);
  XMLNodeXImpl::uptr_t new_xi_node(alloc_xi_node());

  // Dispatch the xerces node import
  if (! new_xi_node->initialize_xrcs_import(other_xi_node, deep)) {
    return XMLNodeXImpl::uptr_t();
  }

  new_xi_node->ynode_ = yang_node;

  return new_xi_node;
}

rw_yang_netconf_op_status_t XMLDocumentXImpl::merge(XMLDocument* other_doc)
{
  // document merge is handled directly by the root node.
  XMLNode* node = get_root_node();
  RW_ASSERT(node);
  XMLNode* other_node = other_doc->get_root_node();
  RW_ASSERT(other_node);
  return node->merge(other_node);
}


rw_yang_netconf_op_status_t XMLDocumentXImpl::to_pbcm (
    ProtobufCMessage *pbcm)
{
  XMLNode* root = get_root_node();
  XMLNode* result_node = root;

  bool is_leaf_node = result_node->is_leaf_node();

  rw_yang_netconf_op_status_t err;
  RW_ASSERT(root);
  RW_ASSERT(pbcm);
  err = RW_YANG_NETCONF_OP_STATUS_OK;

  RW_ASSERT (pbcm->descriptor);
  RW_ASSERT (pbcm->descriptor->ypbc_mdesc);

  std::stringstream log_strm;
  log_strm << "Converting "<< pbcm->descriptor->name;
  typedef std::tuple<const char *, const char *> tmp_path_desc_t;
  std::stack<tmp_path_desc_t> path;

  const rw_yang_pb_msgdesc_t *ypb = pbcm->descriptor->ypbc_mdesc;
  while (nullptr != ypb) {


    // This function is sometimes called with a fictitious root, and sometimes
    // without. If we reach a node that is the same namespace as root, and its
    // parent is null, don't push it
    if ((nullptr == ypb->parent) &&
        (root->get_local_name() == ypb->yang_node_name) &&
        ((!root->get_name_space().length()) ||
         (root->get_name_space() == ypb->module->ns))) {
      break;
    }

    path.push(std::make_tuple (ypb->module->ns, ypb->yang_node_name));
    ypb = ypb->parent;
  }


  while (path.size() && (nullptr != result_node) && !is_leaf_node) {
    auto elem = path.top();
    const char *ns = nullptr, *name = nullptr;
    std::tie (ns, name) = path.top();
    result_node = result_node->find (name, ns);

    if (nullptr == result_node) {
      log_strm << ":Cannot find xml node " << name << "," << ns << std::endl;
      RW_XML_DOC_ERROR (this, log_strm);
      return RW_YANG_NETCONF_OP_STATUS_FAILED;
    }

    path.pop();
    YangNode *yn = result_node->get_yang_node();
    if (nullptr == yn || yn->is_leafy()) {
      log_strm << " : leaf node or null yang encountered,"
               << result_node->get_local_name() << std::endl;
      RW_XML_DOC_ERROR (this, log_strm);

      return RW_YANG_NETCONF_OP_STATUS_FAILED;
    }

    if (yn->is_listy() && result_node->get_next_sibling(name, ns)) {
      // There are multiple list elements. This is ambigious, we cannot
      // determine which one to choose.
      log_strm << " : Multiple list elements for " << name << std::endl;
      RW_XML_DOC_ERROR (this, log_strm);

      return RW_YANG_NETCONF_OP_STATUS_FAILED;
    }

  }

  if (nullptr == result_node) {
    log_strm << " : No result node";
    RW_XML_DOC_ERROR (this, log_strm);
    return RW_YANG_NETCONF_OP_STATUS_FAILED;
  }

  RW_ASSERT (nullptr != result_node);
  return (result_node->to_pbcm(pbcm));
}

XMLNodeXImpl* XMLDocumentXImpl::get_root_node()
{
  X::DOMNode* xrcs_node =  xrcs_doc_->getDocumentElement();
  if (nullptr == xrcs_node) {
    return nullptr;
  }

  return map_xrcs_node(xrcs_node);
}

XMLNodeXImpl* XMLDocumentXImpl::get_root_child_element(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_doc_);
  RW_ASSERT(local_name);

  std::stringstream log_strm;

  XImplXMLStr xml_ns;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    ns = rw_xml_namespace_any;
  } else if (ns == rw_xml_namespace_node) {
    xml_ns = xrcs_doc_->getNamespaceURI();
  } else {
    RW_ASSERT(rw_xml_namespace_none != ns);
    xml_ns = ns;
  }

  log_strm << "namespace:"<<ns<<"node_name"<<local_name;

  X::DOMNodeList* xrcs_list = nullptr;
  if (rw_xml_namespace_any == ns) {
    xrcs_list = xrcs_doc_->getElementsByTagName(XImplXMLStr(local_name));
  } else {
    xrcs_list = xrcs_doc_->getElementsByTagNameNS(XImplXMLStr(ns), XImplXMLStr(local_name));
  }

  X::DOMNode* xmlroot = xrcs_doc_->getDocumentElement();

  log_strm << " Found "<<  xrcs_list->getLength();

  for (size_t i = 0 ; i < xrcs_list->getLength() ; i++) {
    X::DOMNode* xrcs_node = xrcs_list->item(i);
    if (xrcs_node->getParentNode() == xmlroot) {
      RW_XML_DOC_DEBUG (this, log_strm);
      return map_xrcs_node(xrcs_node);
    }
  }
  RW_XML_DOC_ERROR (this, log_strm);
  return nullptr;
}

XMLNodeList::uptr_t XMLDocumentXImpl::get_elements(const char* local_name, const char* ns)
{
  return static_cast_move<XMLNodeList::uptr_t>(xi_get_elements(local_name, ns));
}

XMLNodeListXImpl::uptr_t XMLDocumentXImpl::xi_get_elements(const char* local_name, const char* ns)
{
  if (!local_name) {
    return XMLNodeListXImpl::uptr_t(get_root_node()->xi_get_children());
  }

  XMLNodeListXImpl::uptr_t xi_node_list = alloc_xi_node_list();
  X::DOMNodeList* xrcs_node_list = nullptr;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    xrcs_node_list = xrcs_doc_->getElementsByTagName(XImplXMLStr(local_name));
  } else {
    xrcs_node_list = xrcs_doc_->getElementsByTagNameNS(XImplXMLStr(ns), XImplXMLStr(local_name));
  }

  std::stringstream log_strm;


  RW_ASSERT(xrcs_node_list);
  log_strm << "Got "<< xrcs_node_list->getLength()<< "elements for "<< ns <<":"<<local_name;
  xi_node_list->initialize_xrcs_node_list(xrcs_node_list);

  RW_XML_DOC_DEBUG (this, log_strm);

  return xi_node_list;
}

std::string XMLDocumentXImpl::to_string(const char* local_name, const char* ns)
{
  if (local_name) {
    XMLNodeXImpl* xi_node = get_root_child_element(local_name, ns);
    if (!xi_node) {
      return "";
    }
    return xi_node->to_string();
  }
  return get_root_node()->to_string();
}

std::string XMLDocumentXImpl::to_string_pretty(const char* local_name, const char* ns)
{
  if (local_name) {
    XMLNodeXImpl* xi_node = get_root_child_element(local_name, ns);
    if (!xi_node) {
      return "";
    }
    return xi_node->to_string_pretty();
  }
  return get_root_node()->to_string_pretty();
}

rw_status_t XMLDocumentXImpl::to_stdout(const char* local_name, const char* ns)
{
  if (local_name) {
    XMLNodeXImpl* xi_node = get_root_child_element(local_name, ns);
    if (!xi_node) {
      return RW_STATUS_FAILURE;
    }
    return xi_node->to_stdout();
  }
  return get_root_node()->to_stdout();
}

rw_status_t XMLDocumentXImpl::to_file(const char* file_name, const char* local_name, const char* ns)
{
  if (local_name) {
    XMLNodeXImpl* xi_node = get_root_child_element(local_name, ns);
    if (!xi_node) {
      return RW_STATUS_FAILURE;
    }
    return xi_node->to_file(file_name);
  }
  return get_root_node()->to_file(file_name);
}



rw_status_t XMLDocumentXImpl::evaluate_xpath(XMLNode * xml_node,                                      
                                      std::string const xpath)
{

  // rooted xpaths must refer to the /root element
  std::string const rooted("/");
  std::string const any("//");

  bool const is_rooted = 0 == xpath.compare(0, rooted.size(), rooted);
  bool const is_any = 0 == xpath.compare(0, any.size(), any);
  
  std::string real_xpath(xpath);
  if(is_rooted && !is_any) {
    // ATNN: hack, rooted xpaths don't work so make them select any (i.e. start with //)
    real_xpath = rooted + real_xpath;
  }

  // xalan boiler plate
  XalanSourceTreeDOMSupport       theDOMSupport;
  XalanSourceTreeParserLiaison    theLiaison(theDOMSupport);
  XPathEnvSupportDefault          theEnvSupport;
  theDOMSupport.setParserLiaison(&theLiaison);  

  XalanTransformer theXalanTransformer;

  // map the xerces dom to xalan
  XercesDocumentWrapper xalan_dom(theXalanTransformer.getMemoryManager(),
      xrcs_doc_.get(),
      false, //thread safe, can't have both thread safety and xerces<->xalan mapping
      true, // build wrappper
      true //build maps
      );

  auto ximpl_node = static_cast<XMLNodeXImpl *>(xml_node);
  XalanNode*   context_node = xalan_dom.mapNode(ximpl_node->xrcs_node_);
  RW_ASSERT(context_node);
  XalanElement * root_element = xalan_dom.getDocumentElement();
  RW_ASSERT(root_element);

  // xpath boiler plate
  XObjectFactoryDefault           theXObjectFactory;
  XPathExecutionContextDefault    theExecutionContext(theEnvSupport, theDOMSupport, theXObjectFactory);
  XPathConstructionContextDefault theXPathConstructionContext;
  XPathFactoryDefault             theXPathFactory;
  XPathProcessorImpl              theXPathProcessor;

  // create xpath
  const GetAndReleaseCachedString     theGuard(theXPathConstructionContext);
  XalanDOMString&     theString = theGuard.get();

  XPath* const    xalan_xpath = theXPathFactory.create();

  theString = real_xpath.c_str();

  theXPathProcessor.initXPath(
      *xalan_xpath,
      theXPathConstructionContext,
      theString,
      *xi_manager_.ns_resolver_);

  XObjectPtr xObj = xalan_xpath->execute(
      context_node,
      *xi_manager_.ns_resolver_,
      theExecutionContext);

  switch (xObj->getType())
  {
    case XObject::eTypeNodeSet:
      {
        const NodeRefListBase& nodeset = xObj->nodeset();
        size_t len = nodeset.getLength();

        if (len == 0) {
          return RW_STATUS_FAILURE;
        }
        for (size_t i=0; i<len; i++)
        {
          XalanNode const * const node = nodeset.item(i);
          XalanDOMString found_value;
          int const theType = node->getNodeType();

          if (theType == XalanNode::ELEMENT_NODE) {
            // the text value of the target of the leafref will be a child of the node
            XalanNode const * const value_node = node->getFirstChild();
            if (value_node != nullptr) {
              // we have a #text element to examine
              found_value = value_node->getNodeValue();
            } else {
              continue;
            }
          } else {
            continue;
          }

          XalanDOMString::CharVectorType value_cvec;
          bool const add_null_terminator = true;

          TranscodeToLocalCodePage(found_value, value_cvec, add_null_terminator);

          std::string converted_value = c_str(value_cvec);

          if (xml_node->get_text_value() == converted_value) {
            return RW_STATUS_SUCCESS;            
          } 
        }

        break;
      }

    default:
      {
        break;
      }
  }
  return RW_STATUS_FAILURE;
}

bool XMLDocumentXImpl::is_equal(XMLDocument* other_doc)
{
  RW_ASSERT(other_doc);
  XMLDocumentXImpl* other_xi_doc = dynamic_cast<XMLDocumentXImpl*>(other_doc);
  if (other_xi_doc) {
    return is_equal(other_xi_doc);
  }
  // ATTN: The hard way...
  return false;
}

bool XMLDocumentXImpl::is_equal(XMLDocumentXImpl* other_xi_document)
{
  RW_ASSERT(other_xi_document);
  RW_ASSERT(xrcs_doc_);
  if (other_xi_document) {
    return xrcs_doc_->isEqualNode(other_xi_document->xrcs_doc_.get());
  }
  return false;
}

XMLManager& XMLDocumentXImpl::get_manager()
{
  return xi_manager_;
}

XMLManagerXImpl& XMLDocumentXImpl::get_xi_manager()
{
  return xi_manager_;
}

XMLNodeXImpl* XMLDocumentXImpl::map_xrcs_node(X::DOMNode* xrcs_node)
{
  XMLNodeXImpl* xi_node = (XMLNodeXImpl*)xrcs_node->getUserData(rw_xml_ximpl_bkptr_key);
  if (xi_node) {
    return xi_node;
  }

  XMLNodeXImpl::uptr_t new_xi_node = alloc_xi_node();
  xi_node = new_xi_node.get();
  xi_node->bind_xrcs_node(std::move(new_xi_node), xrcs_node);

  return xi_node;
}

XMLAttributeXImpl* XMLDocumentXImpl::map_xrcs_attribute(X::DOMAttr* xrcs_attribute)

{
  XMLAttributeXImpl* xi_attribute = (XMLAttributeXImpl*)xrcs_attribute->getUserData(rw_xml_ximpl_bkptr_key);
  if (xi_attribute) {
    return xi_attribute;
  }

  XMLAttributeXImpl::uptr_t new_xi_attribute = alloc_xi_attribute();
  xi_attribute = new_xi_attribute.get();
  xi_attribute->bind_xrcs_attribute(std::move(new_xi_attribute), xrcs_attribute);

  return xi_attribute;
}

XMLNodeXImpl::uptr_t XMLDocumentXImpl::alloc_xi_node()
{
  return XMLNodeXImpl::uptr_t(new XMLNodeXImpl(*this));
}

XMLNodeListXImpl::uptr_t XMLDocumentXImpl::alloc_xi_node_list()
{
  return XMLNodeListXImpl::uptr_t(new XMLNodeListXImpl(*this));
}

XMLAttributeXImpl::uptr_t XMLDocumentXImpl::alloc_xi_attribute()
{
  return XMLAttributeXImpl::uptr_t(new XMLAttributeXImpl(*this));
}

XMLAttributeListXImpl::uptr_t XMLDocumentXImpl::alloc_xi_attribute_list()
{
  return XMLAttributeListXImpl::uptr_t(new XMLAttributeListXImpl(*this));
}



/*****************************************************************************/

XMLNodeListXImpl::XMLNodeListXImpl(XMLDocumentXImpl& xi_document)
: xi_document_(xi_document),
  xrcs_node_list_(nullptr)
{
}

void XMLNodeListXImpl::initialize_xrcs_node_list(X::DOMNodeList* xrcs_node_list)
{
  RW_ASSERT(!xrcs_node_list_);
  xrcs_node_list_ = xrcs_node_list;
}

void XMLNodeListXImpl::obj_release() noexcept
{
  delete this;
}

size_t XMLNodeListXImpl::length()
{
  RW_ASSERT(xrcs_node_list_);
  return xrcs_node_list_->getLength();
}

XMLNodeXImpl* XMLNodeListXImpl::find(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_node_list_);
  RW_ASSERT(local_name);
  XImplXMLStr xml_name(local_name);

  XImplXMLStr xml_ns;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    ns = rw_xml_namespace_any;
  } else {
    RW_ASSERT(rw_xml_namespace_none != ns);
    RW_ASSERT(rw_xml_namespace_node != ns);
    xml_ns = ns;
  }

  for (size_t i = 0; i < xrcs_node_list_->getLength(); i++) {
    X::DOMNode* xrcs_node = xrcs_node_list_->item(i);
    if (X::XMLString::equals(xrcs_node->getLocalName(), xml_name)) {
      if (ns != rw_xml_namespace_any) {
        const XMLCh* node_ns = xrcs_node->getNamespaceURI();
        if (node_ns && node_ns[0] != 0 && !X::XMLString::equals(node_ns, xml_ns)) {
          continue;
        }
      }
      return xi_document_.map_xrcs_node(xrcs_node);
    }
  }

  return nullptr;
}

XMLNodeXImpl* XMLNodeListXImpl::at(size_t index)
{
  RW_ASSERT(xrcs_node_list_);
  X::DOMNode* xrcs_node = xrcs_node_list_->item(index);
  if (nullptr == xrcs_node) {
    return nullptr;
  }
  return xi_document_.map_xrcs_node(xrcs_node);
}

XMLDocument& XMLNodeListXImpl::get_document()
{
  return xi_document_;
}

XMLDocumentXImpl& XMLNodeListXImpl::get_xi_document()
{
  return xi_document_;
}

XMLManager& XMLNodeListXImpl::get_manager()
{
  return xi_document_.get_xi_manager();
}

XMLManagerXImpl& XMLNodeListXImpl::get_xi_manager()
{
  return xi_document_.get_xi_manager();
}



/*****************************************************************************/

XMLNodeXImpl::XMLNodeXImpl(XMLDocumentXImpl& xi_document)
: xi_document_(xi_document),
  xrcs_node_(nullptr)
{
}

bool XMLNodeXImpl::initialize_xrcs_node(
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(local_name);
  RW_ASSERT(!xrcs_node_);

  UPtr_DOMNode new_xrcs_node;

  // ATTN: Need default ns

  // Node constructor has a different API, depending on whether there is a namespace
  if (ns) {
    new_xrcs_node.reset(xi_document_.xrcs_doc_->createElementNS(XImplXMLStr(ns), XImplXMLStr(local_name)));
    if (!new_xrcs_node) {
      // ATTN: Where did the error go?
      return false;
    }
    if (prefix) {
      new_xrcs_node->setPrefix(XImplXMLStr(prefix));
    }
  } else {
    RW_ASSERT(!prefix); // No namespace, no prefix
    new_xrcs_node.reset(xi_document_.xrcs_doc_->createElement(XImplXMLStr(local_name)));
    if (!new_xrcs_node) {
      // ATTN: Where did the error go?
      return false;
    }
  }

  if (text_val) {
    new_xrcs_node->setTextContent(XImplXMLStr(text_val));
  }

  /*
   * Register this XML node with the xerces node.  If this call is
   * successful, then the xerces node becomes the owner of this object.
   * However, presumably, the caller already has an owning reference to
   * the RW XML node, so no unique_ptr is wanted here.
   */
  bind_xrcs_node(std::move(new_xrcs_node), new_xrcs_node.get());
  return true;
}

bool XMLNodeXImpl::initialize_xrcs_import(
  XMLNodeXImpl* other_xi_node,
  bool deep)
{
  RW_ASSERT(other_xi_node);
  RW_ASSERT(other_xi_node->xrcs_node_);
  RW_ASSERT(!xrcs_node_);

  /*
   * Import copies a tree of nodes for use in this document, but it
   * doesn't actually place them into the tree - that happens as a
   * following step.  So this function must own the nodes temporarily.
   * However, the very nature of this API assumes that the caller has
   * created a new RW XML node, which they maintain sole ownership of.
   * This function will own the xerces nodes until the RW XML node is
   * bound, at which point the caller is assumed to have acquired
   * ownership.
   */
  // Import the foreign nodes into this document
  UPtr_DOMNode new_xrcs_node(xi_document_.xrcs_doc_->importNode(other_xi_node->xrcs_node_, deep));

  // Bind this and xerces nodes together; release ownership of the xerces node
  bind_xrcs_node(std::move(new_xrcs_node), new_xrcs_node.get());

  return true;
}

void XMLNodeXImpl::xrcs_set_prefix(const char* prefix)
{
  RW_ASSERT(xrcs_node_);

  if (prefix && prefix[0] != '\0') {
    xrcs_node_->setPrefix(XImplXMLStr(prefix));
  }
}

void XMLNodeXImpl::bind_xrcs_node(X::DOMNode* xrcs_node)
{
  RW_ASSERT(xrcs_node);
  RW_ASSERT(!xrcs_node_);
  void* old_xi_node = xrcs_node->setUserData(rw_xml_ximpl_bkptr_key, this, &xi_document_.user_data_handler_);
  RW_ASSERT(!old_xi_node);
  xrcs_node_ = xrcs_node;
}

void XMLNodeXImpl::obj_release() noexcept
{
  if (xrcs_node_) {
    // Release the xerces node.  This will cause a callback to xrcs_release().
    xrcs_node_->release();
  } else {
    // Delete directly - didn't get bound to a xerces node
    delete this;
  }
}

void XMLNodeXImpl::xrcs_release() noexcept
{
  RW_ASSERT(xrcs_node_);
  xrcs_node_ = nullptr;
  delete this;
}

XMLNodeXImpl::~XMLNodeXImpl()
{
  /*
   * Should never get here when still bound to a xerces node!  That
   * means that someone called delete on the node directly, instead of
   * releasing the xerces node.
   */
  RW_ASSERT(nullptr == xrcs_node_);
}

std::string XMLNodeXImpl::get_text_value()
{
  RW_ASSERT(xrcs_node_);
  if (xrcs_node_->getNodeType() == X::DOMNode::ELEMENT_NODE) {
    // ATTN: Need to iterate over possibly multiple text-ish children?
    X::DOMNode* child = xrcs_node_->getFirstChild();
    if (child && child->getNodeType() == X::DOMNode::TEXT_NODE) {
      return XImplCharStr(child->getTextContent());
    }
  }
  return "";
}

rw_status_t XMLNodeXImpl::set_text_value(const char* value)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(value);
  xrcs_node_->setTextContent(XImplXMLStr(value));
  return RW_STATUS_SUCCESS;
}

std::string XMLNodeXImpl::get_prefix()
{
  RW_ASSERT(xrcs_node_);
  return XImplCharStr(xrcs_node_->getPrefix());
}

std::string XMLNodeXImpl::get_local_name()
{
  RW_ASSERT(xrcs_node_);
  const XMLCh* xml_name = xrcs_node_->getLocalName();
  if (nullptr == xml_name) {
    xml_name = xrcs_node_->getNodeName();
  }
  return XImplCharStr(xml_name);
}

std::string XMLNodeXImpl::get_name_space()
{
  RW_ASSERT(xrcs_node_);
  return XImplCharStr(xrcs_node_->getNamespaceURI());
}

XMLDocument& XMLNodeXImpl::get_document()
{
  return xi_document_;
}

XMLDocumentXImpl& XMLNodeXImpl::get_xi_document()
{
  return xi_document_;
}

XMLManager& XMLNodeXImpl::get_manager()
{
  return xi_document_.get_manager();
}

XMLManagerXImpl& XMLNodeXImpl::get_xi_manager()
{
  return xi_document_.get_xi_manager();
}

XMLNodeXImpl* XMLNodeXImpl::get_parent()
{
  RW_ASSERT(xrcs_node_);
  X::DOMNode* xrcs_parent_node = xrcs_node_->getParentNode();
  if (!xrcs_parent_node) {
    return nullptr;
  }

  if (xi_document_.xrcs_doc_.get() == xrcs_parent_node) {
    // Don't return the document node as a parent node.
    return nullptr;
  }

  return xi_document_.map_xrcs_node(xrcs_parent_node);
}

XMLNodeXImpl* XMLNodeXImpl::get_first_child()
{
  RW_ASSERT(xrcs_node_);
  X::DOMNode* xrcs_element = xrcs_node_->getFirstChild();
  while (xrcs_element && xrcs_element->getNodeType() != X::DOMNode::ELEMENT_NODE) {
    xrcs_element = xrcs_element->getNextSibling();
  }
  if (!xrcs_element) {
    return nullptr;
  }
  return xi_document_.map_xrcs_node(xrcs_element);
}

XMLNodeXImpl* XMLNodeXImpl::get_last_child()
{
  RW_ASSERT(xrcs_node_);
  X::DOMNode* xrcs_element = xrcs_node_->getLastChild();
  while (xrcs_element && xrcs_element->getNodeType() != X::DOMNode::ELEMENT_NODE) {
    xrcs_element = xrcs_element->getPreviousSibling();
  }
  if (!xrcs_element) {
    return nullptr;
  }
  return xi_document_.map_xrcs_node(xrcs_element);
}

XMLNodeXImpl* XMLNodeXImpl::get_next_sibling()
{
  RW_ASSERT(xrcs_node_);
  X::DOMNode* xrcs_element = xrcs_node_->getNextSibling();
  while (xrcs_element && xrcs_element->getNodeType() != X::DOMNode::ELEMENT_NODE) {
    xrcs_element = xrcs_element->getNextSibling();
  }
  if (!xrcs_element) {
    return nullptr;
  }
  return xi_document_.map_xrcs_node(xrcs_element);
}

XMLNodeXImpl* XMLNodeXImpl::get_previous_sibling()
{
  RW_ASSERT(xrcs_node_);
  X::DOMNode* xrcs_element = xrcs_node_->getPreviousSibling();
  while (xrcs_element && xrcs_element->getNodeType() != X::DOMNode::ELEMENT_NODE) {
    xrcs_element = xrcs_element->getPreviousSibling();
  }
  if (!xrcs_element) {
    return nullptr;
  }
  return xi_document_.map_xrcs_node(xrcs_element);
}

XMLNodeXImpl* XMLNodeXImpl::get_first_element()
{
  X::DOMNodeList* xrcs_list = xrcs_node_->getChildNodes();
  for (size_t i = 0; i < xrcs_list->getLength(); i++) {
    X::DOMNode* xrcs_node = xrcs_list->item(i);
    if (xrcs_node->getNodeType() == X::DOMNode::ELEMENT_NODE) {
      return xi_document_.map_xrcs_node(xrcs_node);
    }
  }
  return nullptr;
}

XMLNodeList::uptr_t XMLNodeXImpl::get_children()
{
  // Implemented in terms of the xerces-specific API, but must convert unique_ptr
  return static_cast_move<XMLNodeList::uptr_t>(xi_get_children());
}

XMLNodeListXImpl::uptr_t XMLNodeXImpl::xi_get_children()
{
  X::DOMNodeList* xrcs_node_list = xrcs_node_->getChildNodes();
  RW_ASSERT(xrcs_node_list);

  XMLNodeListXImpl::uptr_t xi_node_list = get_xi_document().alloc_xi_node_list();
  xi_node_list->initialize_xrcs_node_list(xrcs_node_list);
  return xi_node_list;
}

bool XMLNodeXImpl::supports_attributes()
{
  return true;
}

XMLAttributeList::uptr_t XMLNodeXImpl::get_attributes()
{
  return static_cast_move<XMLAttributeList::uptr_t>(xi_get_attributes());
}

XMLAttributeListXImpl::uptr_t XMLNodeXImpl::xi_get_attributes()
{
  XMLAttributeListXImpl::uptr_t xi_attribute_list = xi_document_.alloc_xi_attribute_list();
  RW_ASSERT(xrcs_node_);
  X::DOMNamedNodeMap* xrcs_attribute_list = xrcs_node_->getAttributes();
  xi_attribute_list->initialize_xrcs_attribute_list(xrcs_attribute_list);
  return xi_attribute_list;
}

bool XMLNodeXImpl::has_attributes()
{
  return xrcs_node_->hasAttributes();
}

bool XMLNodeXImpl::has_attribute(const char* attr_name, const char *ns)
{
  RW_ASSERT(attr_name);
  if (ns && ns != rw_xml_namespace_none) {
    return static_cast<X::DOMElement*>(xrcs_node_)->hasAttributeNS(
      XImplXMLStr(ns),
      XImplXMLStr(attr_name));
  }
  return static_cast<X::DOMElement*>(xrcs_node_)->hasAttribute(XImplXMLStr(attr_name));
}

XMLAttribute::uptr_t XMLNodeXImpl::get_attribute(const char* attr_name, const char *ns)
{
  RW_ASSERT(attr_name);

  if (ns && ns != rw_xml_namespace_none) {
    X::DOMAttr* xrcs_attribute
      = static_cast<X::DOMElement*>(xrcs_node_)->getAttributeNodeNS(
          XImplXMLStr(ns),
          XImplXMLStr(attr_name));
    return static_cast<XMLAttribute::uptr_t>(xi_document_.map_xrcs_attribute(xrcs_attribute));
  }

  X::DOMAttr* xrcs_attribute = static_cast<X::DOMElement*>(xrcs_node_)->getAttributeNode(XImplXMLStr(attr_name));
  return static_cast<XMLAttribute::uptr_t>(xi_document_.map_xrcs_attribute(xrcs_attribute));
}

std::string XMLNodeXImpl::get_attribute_value(const char* attr_name, const char *ns)
{
  RW_ASSERT(attr_name);

  if (ns && ns != rw_xml_namespace_none) {
    const XMLCh* attr_val = static_cast<X::DOMElement*>(xrcs_node_)->getAttributeNS(
          XImplXMLStr(ns),
          XImplXMLStr(attr_name));
    return XImplCharStr(attr_val);
  }
  const XMLCh* attr_val = static_cast<X::DOMElement*>(xrcs_node_)->getAttribute(
          XImplXMLStr(attr_name));
  return XImplCharStr(attr_val);

}

void XMLNodeXImpl::remove_attribute(XMLAttribute *attribute)
{
  RW_ASSERT(0) ; // Implement this
}

void XMLNodeXImpl::set_attribute(
  const char* attr_name,
  const char* attr_value,
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(attr_name);
  RW_ASSERT(attr_value);

  const char* colon = strchr(attr_name, ':');
  RW_ASSERT(!(colon && prefix && strncmp(attr_name, prefix, colon-attr_name))); // cannot specify a prefix both ways
  RW_ASSERT(ns || (!colon && !prefix)); // you must provide the namespace if you provide a prefix

  if (nullptr == ns) {
     ns = rw_xml_namespace_none;
  }

  if (ns != rw_xml_namespace_none) {
    XImplXMLStr xml_ns(ns);

    // Need to lookup the prefix?
    XImplCharStr xml_prefix;
    if (!colon && !prefix) {
      xml_prefix = xrcs_node_->lookupPrefix(xml_ns);
      prefix = xml_prefix;
    }

    XImplXMLStr xml_qual_name;
    if (prefix && !colon) {
      // Create the qualified name from scratch.
      std::string temp_qual = prefix;
      temp_qual.append(":");
      temp_qual.append(attr_name);
      xml_qual_name = temp_qual.c_str();
    } else {
      xml_qual_name = attr_name;
    }

    static_cast<X::DOMElement*>(xrcs_node_)->setAttributeNS(
      xml_ns,
      xml_qual_name,
      XImplXMLStr(attr_value));
    return;
  }

  static_cast<X::DOMElement*>(xrcs_node_)->setAttribute(XImplXMLStr(attr_name), XImplXMLStr(attr_value));
}

bool XMLNodeXImpl::is_equal(XMLNode* other_node)
{
  RW_ASSERT(other_node);
  XMLNodeXImpl* other_xi_node = dynamic_cast<XMLNodeXImpl*>(other_node);
  RW_ASSERT(other_xi_node);
  return is_equal(other_xi_node);
  // ATTN: Use default implementation
}

bool XMLNodeXImpl::is_equal(XMLNodeXImpl* xi_node)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(xi_node->xrcs_node_);
  return xrcs_node_->isEqualNode(xi_node->xrcs_node_);
}

XMLNodeXImpl* XMLNodeXImpl::find(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XImplXMLStr xml_name(local_name);

  XImplXMLStr xml_ns;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    ns = rw_xml_namespace_any;
  } else if (ns == rw_xml_namespace_node) {
    xml_ns = xrcs_node_->getNamespaceURI();
  } else {
    RW_ASSERT(rw_xml_namespace_none != ns);
    xml_ns = ns;
  }

  X::DOMNodeList* xrcs_children = xrcs_node_->getChildNodes();
  for (size_t i = 0; i < xrcs_children->getLength(); i++) {
    X::DOMNode* xrcs_node = xrcs_children->item(i);
    if (X::XMLString::equals(xrcs_node->getLocalName(), xml_name)) {
      if (ns != rw_xml_namespace_any) {
        const XMLCh* node_ns = xrcs_node->getNamespaceURI();
        if (node_ns && node_ns[0] != 0 && !X::XMLString::equals(node_ns, xml_ns)) {
          continue;
        }
      }
      return get_xi_document().map_xrcs_node(xrcs_node);
    }
  }

  return nullptr;
}

std::unique_ptr<std::list<XMLNode*>> XMLNodeXImpl::find_all(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XImplXMLStr xml_name(local_name);

  std::unique_ptr<std::list<XMLNode*>> found_nodes(new std::list<XMLNode*>);

  XImplXMLStr xml_ns;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    ns = rw_xml_namespace_any;
  } else if (ns == rw_xml_namespace_node) {
    xml_ns = xrcs_node_->getNamespaceURI();
  } else {
    RW_ASSERT(rw_xml_namespace_none != ns);
    xml_ns = ns;
  }

  X::DOMNodeList* xrcs_children = xrcs_node_->getChildNodes();
  for (size_t i = 0; i < xrcs_children->getLength(); i++) {
    X::DOMNode* xrcs_node = xrcs_children->item(i);
    if (X::XMLString::equals(xrcs_node->getLocalName(), xml_name)) {
      if (ns != rw_xml_namespace_any) {
        const XMLCh* node_ns = xrcs_node->getNamespaceURI();
        if (node_ns && node_ns[0] != 0 && !X::XMLString::equals(node_ns, xml_ns)) {
          continue;
        }
      }
      found_nodes->push_back(get_xi_document().map_xrcs_node(xrcs_node));
    }
  }

  if (found_nodes->size() > 0) {
    return std::move(found_nodes);
  }
  return nullptr;
}

size_t XMLNodeXImpl::count(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);

  std::unique_ptr<std::list<XMLNode*>> found_nodes = find_all(local_name, ns);

  if (found_nodes == 0) {
    return 0;
  } else {
    return found_nodes->size();
  }
}

XMLNodeXImpl* XMLNodeXImpl::find_value(const char* local_name, const char* text_value, const char* ns)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XImplXMLStr xml_name(local_name);
  RW_ASSERT(text_value);
  XImplXMLStr xml_value(text_value);

  XImplXMLStr xml_ns;
  if (nullptr == ns || ns[0] == '\0' || rw_xml_namespace_any == ns) {
    ns = rw_xml_namespace_any;
  } else if (ns == rw_xml_namespace_node) {
    xml_ns = xrcs_node_->getNamespaceURI();
  } else {
    RW_ASSERT(rw_xml_namespace_none != ns);
    xml_ns = ns;
  }

  X::DOMNodeList* xrcs_children = xrcs_node_->getChildNodes();
  for (size_t i = 0; i < xrcs_children->getLength(); i++) {
    X::DOMNode* xrcs_node = xrcs_children->item(i);
    if (X::XMLString::equals(xrcs_node->getLocalName(), xml_name)) {
      if (ns != rw_xml_namespace_any) {
        const XMLCh* node_ns = xrcs_node->getNamespaceURI();
        if (node_ns && node_ns[0] != 0 && !X::XMLString::equals(node_ns, xml_ns)) {
          continue;
        }
      }
      const XMLCh* node_text_value = xrcs_node->getTextContent();
      if (node_text_value && X::XMLString::equals(node_text_value, xml_value)) {
        return get_xi_document().map_xrcs_node(xrcs_node);
      }
    }
  }


  return nullptr;
}

bool XMLNodeXImpl::equals(const char* local_name, const char* ns)
{
  return ((get_local_name() == local_name) &&
          (get_name_space().empty() || get_name_space() == ns));
}

rw_status_t XMLNodeXImpl::append_child(XMLNodeXImpl::uptr_t* new_xi_node)
{
  RW_ASSERT(new_xi_node);
  RW_ASSERT((*new_xi_node)->xrcs_node_);
  if (&(*new_xi_node)->xi_document_ != &xi_document_) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(xrcs_node_);

  try {
    X::DOMNode* xrcs_added = xrcs_node_->appendChild((*new_xi_node)->xrcs_node_);
    RW_ASSERT(xrcs_added == (*new_xi_node)->xrcs_node_);
    new_xi_node->release();
  } catch(X::DOMException& e) {
    // ATTN: better status?
    // ATTN: Or just assert?
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t XMLNodeXImpl::add_key_child(
                  YangNode* yang_node,
                  XMLNodeXImpl::uptr_t* new_xi_node)
{      
  // Add the key to the beginning as per the key order
  XMLNodeXImpl* xi_child = nullptr;
  XMLNodeXImpl* first_xi_child = get_first_child();
  XMLNodeXImpl* last_key_xnode = nullptr;

  RW_ASSERT(first_xi_child);

  for (auto key_iter = yang_node->key_begin();
      key_iter != yang_node->key_end(); key_iter++) {
    const char* kname = key_iter->get_key_node()->get_name();
    const char* kns   = key_iter->get_key_node()->get_ns();

    if ((*new_xi_node)->equals(kname, kns)) {
      // Key matches with the new xml node
      // Insert at the beginning if there were no key xml nodes found
      // Else insert after the last found key node
      if (last_key_xnode == nullptr) {
        first_xi_child->insert_before(new_xi_node);
      } else {
        last_key_xnode->insert_after(new_xi_node);
      }
      return RW_STATUS_SUCCESS;
    } 

    // Get the next key xml node in the XML
    xi_child = first_xi_child;
    while (xi_child) {
      if (xi_child->equals(kname, kns)) {
        last_key_xnode = xi_child;
        break;
      }
      xi_child = xi_child->get_next_sibling(); 
    }
  }

  // Key not inserted! Key not found in the 'key' statement!
  RW_ASSERT_NOT_REACHED();
  return RW_STATUS_FAILURE;
}

rw_status_t XMLNodeXImpl::add_child(XMLNodeXImpl::uptr_t* new_xi_node)
{
  XMLNodeXImpl* xi_child = get_first_child();
  if (xi_child == nullptr) {
    // No child xml nodes, treat it like a normal append
    return append_child(new_xi_node);
  }

  // Yang ordering is required only in the following cases
  //   - RPC input nodes
  //   - RPC output nodes
  //   - Key nodes of a list
  YangNode* yang_node = get_yang_node();
  if (yang_node == nullptr ||
      !(yang_node->is_rpc() || 
        yang_node->is_rpc_input() || 
        yang_node->is_rpc_output() || 
        yang_node->has_keys())
     ) {
    return append_child(new_xi_node);
  }

  // Iterate the Child Yang nodes in order
  bool is_rpc = false;
  YangNode* yn_child = yang_node->get_first_child();

  if (yang_node->is_rpc()) {
    // If the node is rpc then skip the 'input', 'output' and 'error' nodes
    // Go to the children of these nodes instead
    is_rpc = true;
    yang_node = yn_child;
    if (yang_node) {
      yn_child = yang_node->get_first_child();
    }
  }

  while (yn_child) {
    const char* cname = yn_child->get_name();
    const char* cns = yn_child->get_ns();
    if (yn_child->is_key() && (*new_xi_node)->equals(cname, cns)) {
      // A key node to be added as per 'key' statement order
      return add_key_child(yang_node, new_xi_node);
    }

    while (xi_child && (xi_child->get_yang_node()->is_key() ||
                        xi_child->get_yang_node() == yn_child)) {
      // Found the YangNode for the current child xmlnode, advance the child
      // node. This child expected to match next. In case of leaf list, there
      // may be multiple xml nodes with same yang node
      xi_child = xi_child->get_next_sibling();
    }

    if ((*new_xi_node)->equals(cname, cns)) {
      if (xi_child == nullptr) {
        // No more children, treat it like a normal append
        return append_child(new_xi_node);
      }
      if (yn_child->is_rpc_input() || yn_child->is_rpc_output()) {
        // Found the yang node for the child node to be added
        // Add it before the XMLNode child, that is expected next
        return xi_child->insert_before(new_xi_node);
      } else {
        return append_child(new_xi_node);
      }
    }

    // Go to the next yang child
    yn_child = yn_child->get_next_sibling();
    if (yn_child == nullptr && is_rpc) {
      // In case of rpc, the child is the children of next rpc node
      yang_node = yang_node->get_next_sibling();
      if (yang_node) {
        yn_child = yang_node->get_first_child();
      }
    }
  }

  // Neither the child yang node is found,
  // nor reached the end of XMLNode children
  return RW_STATUS_FAILURE;
}

XMLNodeXImpl* XMLNodeXImpl::add_child(
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XMLNodeXImpl::uptr_t new_xi_node(xi_document_.alloc_xi_node());
  std::string my_ns = get_name_space();

  if (nullptr == ns || rw_xml_namespace_node == ns) {
    ns = my_ns.c_str();
  }

  // Dispatch the xerces node creation
  if (! new_xi_node->initialize_xrcs_node(local_name, text_val, ns, prefix)) {
    return nullptr;
  }

  // add_child will transfer ownership upon success, so keep a copy of the pointer
  XMLNodeXImpl* retval = new_xi_node.get();
  rw_status_t rs = XMLNodeXImpl::add_child(&new_xi_node);

  switch (rs) {
    case RW_STATUS_SUCCESS:
      RW_ASSERT(!new_xi_node);
      return retval;
    default:
      break;
  }

  // Let the new_xi_node get released automatically
  return nullptr;
}

XMLNodeXImpl* XMLNodeXImpl::add_child(YangNode* yang_node,
                                      const char* text_val)
{
  RW_ASSERT(yang_node);

  // Use the yang data to create the node
  XMLNodeXImpl* new_xml_node = add_child(
    yang_node->get_name(),
    text_val,
    yang_node->get_ns(),
    yang_node->get_prefix());
  new_xml_node->set_yang_node(yang_node);
  return (new_xml_node);
}

XMLNodeXImpl* XMLNodeXImpl::import_child(XMLNode* other_node, YangNode* ynode, bool deep)
{
  RW_ASSERT(other_node);
  XMLNodeXImpl::uptr_t new_xi_copy(
    dynamic_cast_move<XMLNodeXImpl::uptr_t>(xi_document_.import_node(other_node, ynode, deep)));

  XMLNodeXImpl* retval = new_xi_copy.get();
  rw_status_t rs = add_child(&new_xi_copy);
  if (RW_STATUS_SUCCESS != rs) {
    return nullptr;
  }

  return retval;
}

rw_status_t XMLNodeXImpl::insert_before(XMLNodeXImpl::uptr_t* new_xi_node)
{
  RW_ASSERT(new_xi_node);
  RW_ASSERT((*new_xi_node)->xrcs_node_);
  if (&(*new_xi_node)->xi_document_ != &xi_document_) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(xrcs_node_);

  // If this is root node return error
  if (this == get_document().get_root_node()) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(get_parent());
  RW_ASSERT(get_parent()->xrcs_node_);

  try {
    X::DOMNode* xrcs_added = get_parent()->xrcs_node_->insertBefore((*new_xi_node)->xrcs_node_, xrcs_node_);
    RW_ASSERT(xrcs_added == (*new_xi_node)->xrcs_node_);
    new_xi_node->release();
  } catch(X::DOMException& e) {
    // ATTN: better status?
    // ATTN: Or just assert?
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}

XMLNodeXImpl* XMLNodeXImpl::insert_before(
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XMLNodeXImpl::uptr_t new_xi_node(xi_document_.alloc_xi_node());

  if (nullptr == ns || rw_xml_namespace_node == ns) {
    ns = get_name_space().c_str();
  }

  // Dispatch the xerces node creation
  if (! new_xi_node->initialize_xrcs_node(local_name, text_val, ns, prefix)) {
    return nullptr;
  }

  // insert_before will transfer ownership upon success, so keep a copy of the pointer
  XMLNodeXImpl* retval = new_xi_node.get();
  rw_status_t rs = XMLNodeXImpl::insert_before(&new_xi_node);

  switch (rs) {
    case RW_STATUS_SUCCESS:
      RW_ASSERT(!new_xi_node);
      return retval;
    default:
      break;
  }

  // Let the new_xi_node get released automatically
  return nullptr;
}

XMLNodeXImpl* XMLNodeXImpl::insert_before(YangNode* yang_node,
                                          const char* text_val)
{
  RW_ASSERT(yang_node);

  // Use the yang data to create the node
  XMLNodeXImpl* new_xml_node = insert_before(
    yang_node->get_name(),
    text_val,
    yang_node->get_ns(),
    yang_node->get_prefix());
  if (new_xml_node) {
    new_xml_node->set_yang_node(yang_node);
  }
  return (new_xml_node);
}

XMLNodeXImpl* XMLNodeXImpl::import_before(XMLNode* other_node, YangNode *ynode, bool deep)
{
  RW_ASSERT(other_node);
  XMLNodeXImpl::uptr_t new_xi_copy(
    dynamic_cast_move<XMLNodeXImpl::uptr_t>(xi_document_.import_node(other_node, ynode, deep)));

  XMLNodeXImpl* retval = new_xi_copy.get();
  rw_status_t rs = insert_before(&new_xi_copy);
  if (RW_STATUS_SUCCESS != rs) {
    return nullptr;
  }

  return retval;
}

rw_status_t XMLNodeXImpl::insert_after(XMLNodeXImpl::uptr_t* new_xi_node)
{
  RW_ASSERT(new_xi_node);
  RW_ASSERT((*new_xi_node)->xrcs_node_);
  if (&(*new_xi_node)->xi_document_ != &xi_document_) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(xrcs_node_);

  // If this is root node return error
  if (this == get_document().get_root_node()) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(get_parent());
  RW_ASSERT(get_parent()->xrcs_node_);

  try {
    X::DOMNode* next_silbling = nullptr;
    // End of the list -- Cannot insert
    if (get_next_sibling() != nullptr) {
      next_silbling = get_next_sibling()->xrcs_node_;
    }
    X::DOMNode* xrcs_added = get_parent()->xrcs_node_->insertBefore((*new_xi_node)->xrcs_node_, next_silbling);
    RW_ASSERT(xrcs_added == (*new_xi_node)->xrcs_node_);
    new_xi_node->release();
  } catch(X::DOMException& e) {
    // ATTN: better status?
    // ATTN: Or just assert?
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}

XMLNodeXImpl* XMLNodeXImpl::insert_after(
  const char* local_name,
  const char* text_val,
  const char* ns,
  const char* prefix)
{
  RW_ASSERT(xrcs_node_);
  RW_ASSERT(local_name);
  XMLNodeXImpl::uptr_t new_xi_node(xi_document_.alloc_xi_node());

  if (nullptr == ns || rw_xml_namespace_node == ns) {
    ns = get_name_space().c_str();
  }

  // Dispatch the xerces node creation
  if (! new_xi_node->initialize_xrcs_node(local_name, text_val, ns, prefix)) {
    return nullptr;
  }

  // insert_after will transfer ownership upon success, so keep a copy of the pointer
  XMLNodeXImpl* retval = new_xi_node.get();
  rw_status_t rs = XMLNodeXImpl::insert_after(&new_xi_node);

  switch (rs) {
    case RW_STATUS_SUCCESS:
      RW_ASSERT(!new_xi_node);
      return retval;
    default:
      break;
  }

  // Let the new_xi_node get released automatically
  return nullptr;
}

XMLNodeXImpl* XMLNodeXImpl::insert_after(YangNode* yang_node,
                                         const char* text_val)
{
  RW_ASSERT(yang_node);

  // Use the yang data to create the node
  XMLNodeXImpl* new_xml_node = insert_after(
    yang_node->get_name(),
    text_val,
    yang_node->get_ns(),
    yang_node->get_prefix());
  if (new_xml_node) {
    new_xml_node->set_yang_node(yang_node);
  }
  return (new_xml_node);
}

XMLNodeXImpl* XMLNodeXImpl::import_after(XMLNode* other_node, YangNode *ynode, bool deep)
{
  RW_ASSERT(other_node);
  XMLNodeXImpl::uptr_t new_xi_copy(
    dynamic_cast_move<XMLNodeXImpl::uptr_t>(xi_document_.import_node(other_node, ynode, deep)));

  XMLNodeXImpl* retval = new_xi_copy.get();
  rw_status_t rs = insert_after(&new_xi_copy);
  if (RW_STATUS_SUCCESS != rs) {
    return nullptr;
  }
  return retval;
}

bool XMLNodeXImpl::remove_child(XMLNodeXImpl* child_xi_node)
{
  RW_ASSERT(child_xi_node);
  RW_ASSERT(xrcs_node_);

  X::DOMNode* xrcs_removed = nullptr;
  try {
    xrcs_removed = xrcs_node_->removeChild(child_xi_node->xrcs_node_);
  } catch(X::DOMException& e) {
    switch (e.code) {
      case X::DOMException::NOT_FOUND_ERR:
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
    return false;
  }

  RW_ASSERT(xrcs_removed == child_xi_node->xrcs_node_);

  child_xi_node->obj_release();

  return true;
}

bool XMLNodeXImpl::remove_child(XMLNode* child_node)
{
  RW_ASSERT(child_node);
  XMLNodeXImpl* child_xi_node = dynamic_cast<XMLNodeXImpl*>(child_node);
  RW_ASSERT(child_xi_node);
  return (remove_child(child_xi_node));
}

std::string XMLNodeXImpl::to_string()
{
  // ATTN: Create a subclass of X::XMLFormatTarget that goes straight to std::string
  X::MemBufFormatTarget fmt_tgt;
  rw_status_t rs = to_target(&fmt_tgt, false/*pretty_print*/);
  if (RW_STATUS_SUCCESS != rs) {
    return std::string();
  }

  return std::string((const char*)fmt_tgt.getRawBuffer(), fmt_tgt.getLen());
}

std::string XMLNodeXImpl::to_string_pretty()
{
  // ATTN: Create a subclass of X::XMLFormatTarget that goes straight to std::string
  X::MemBufFormatTarget fmt_tgt;
  rw_status_t rs = to_target(&fmt_tgt, true/*pretty_print*/);
  if (RW_STATUS_SUCCESS != rs) {
    return std::string();
  }

  return std::string((const char*)fmt_tgt.getRawBuffer(), fmt_tgt.getLen());
}

rw_status_t XMLNodeXImpl::to_stdout()
{
  X::StdOutFormatTarget fmt_tgt;
  return to_target(&fmt_tgt, true/*pretty_print*/);
}

rw_status_t XMLNodeXImpl::to_file(const char* file_name)
{
  X::LocalFileFormatTarget fmt_tgt(file_name);
  return to_target(&fmt_tgt, true/*pretty_print*/);
}

rw_status_t XMLNodeXImpl::to_target(X::XMLFormatTarget* target, bool pretty_print)
{
  RW_ASSERT(target);
  UPtr_DOMLSOutput output(get_xi_manager().create_ls_output());
  RW_ASSERT(output);
  output->setByteStream(target);

  UPtr_DOMLSSerializer serializer(get_xi_manager().create_ls_serializer(pretty_print));
  RW_ASSERT(serializer);

  // ATTN: There should be an error handler here?
  RW_ASSERT(xrcs_node_);

  X::DOMConfiguration* config = serializer->getDomConfig();
  RW_ASSERT(config);

  get_document().get_error_handler().clear();
  config->setParameter(X::XMLUni::fgDOMErrorHandler, &get_document().get_error_handler());

  bool success = serializer->write(xrcs_node_, output.get());
  // ATTN: something should be in the error handler if there was an error...
  return success ? RW_STATUS_SUCCESS : RW_STATUS_FAILURE;
}


/*****************************************************************************/

void XImplDOMUserDataHandler::handle(DOMOperationType operation,
                                     const XMLCh* const key,
                                     void* v_xi_node,
                                     const X::DOMNode* xrcs_node,
                                     X::DOMNode* xrcs_other_node)
{
  UNUSED(key); // presumably rw_xml_ximpl_bkptr_key
  UNUSED(xrcs_other_node); // only for CLONED, ADOPTED

  switch (operation) {
    case NODE_DELETED: {
      XMLNodeXImpl* xi_node = static_cast<XMLNodeXImpl*>(v_xi_node);
      // xrcs_node is stupidly null, so we can't verify it here
      xi_node->xrcs_release();
      break;
    }
    case NODE_CLONED:
    case NODE_IMPORTED:
      // These happen, but we don't care right now.
      break;
    case NODE_RENAMED:
    case NODE_ADOPTED:
      // These should not happen in RW XML
      RW_ASSERT_NOT_REACHED();
      break;
    default:
      RW_ASSERT_NOT_REACHED();
      break;
  }
}

/*****************************************************************************/

void XImplDOMAttributeUserDataHandler::handle(DOMOperationType operation,
                                     const XMLCh* const key,
                                     void* v_xi_attribute,
                                     const X::DOMNode* xrcs_node,
                                     X::DOMNode* xrcs_other_attribute)
{
  UNUSED(key); // presumably rw_xml_ximpl_bkptr_key
  UNUSED(xrcs_other_attribute); // only for CLONED, ADOPTED

  switch (operation) {
    case NODE_DELETED: {
      XMLAttributeXImpl* xi_attribute = static_cast<XMLAttributeXImpl*>(v_xi_attribute);
      // xrcs_node is stupidly null, so we can't verify it here
      xi_attribute->xrcs_release();
      break;
    }
    case NODE_CLONED:
    case NODE_IMPORTED:
      // These happen, but we don't care right now.
      break;
    case NODE_RENAMED:
    case NODE_ADOPTED:
      // These should not happen in RW XML
      RW_ASSERT_NOT_REACHED();
      break;
    default:
      RW_ASSERT_NOT_REACHED();
      break;
  }
}

/*****************************************************************************/

bool XImplDOMErrorHandler::handleError(const X::DOMError& domError)
{
  bool continueParsing = true;
  oss_ << "XML ";

  if (domError.getSeverity() == X::DOMError::DOM_SEVERITY_WARNING) {
    oss_ << "warning";
    ++warnings_;
  } else if (domError.getSeverity() == X::DOMError::DOM_SEVERITY_ERROR) {
    oss_ << "error";
    ++errors_;
  } else {
    oss_ << "fatal error";
    continueParsing = false;
    ++fatal_errors_;
  }

  oss_
    << ": URI " << XImplCharStr(domError.getLocation()->getURI())
    << ", at " << domError.getLocation()->getLineNumber()
    << ":" << domError.getLocation()->getColumnNumber()
    << ": " << XImplCharStr(domError.getMessage())
    << std::endl;

  return continueParsing;
}

void XImplDOMErrorHandler::handle_app_errors(const char* errstr)
{
  oss_ << errstr;
}

#if 0
ATTN: These are for SAX ErrorHandler, which is not a base of XImplDOMErrorHandler
void XImplDOMErrorHandler::fatalError(const X::SAXParseException& e)
{
  ++fatal_errors_;
  oss_
    << "XML fatal error: ";
    << "URI " << XImplCharStr(e.getSystemId())
    << ", at " << e.getLineNumber()
    << ":" << e.getColumnNumber()
    << ": " << XImplCharStr(e.getMessage())
    << std::endl;
}

void XImplDOMErrorHandler::error(const X::SAXParseException& e)
{
  ++errors_;
  oss_
    << "XML error: ";
    << "URI " << XImplCharStr(e.getSystemId())
    << ", at " << e.getLineNumber()
    << ":" << e.getColumnNumber()
    << ": " << XImplCharStr(e.getMessage())
    << std::endl;
}

void XImplDOMErrorHandler::warning(const X::SAXParseException& e)
{
  ++warnings_;
  oss_
    << "XML warning: ";
    << "URI " << XImplCharStr(e.getSystemId())
    << ", at " << e.getLineNumber()
    << ":" << e.getColumnNumber()
    << ": " << XImplCharStr(e.getMessage())
    << std::endl;
}
#endif



/*****************************************************************************/

X::DOMLSInput* XImplDOMLSResourceResolver::resolveResource(
  const XMLCh* const resourceType,
  const XMLCh* const namespaceUri,
  const XMLCh* const publicId,
  const XMLCh* const systemId,
  const XMLCh* const baseURI)
{
  static const char xsd_ext[] = ".xsd";

  (void)resourceType; /* ATTN */
  (void)namespaceUri; /* ATTN */
  (void)publicId; /* ATTN */
  (void)baseURI; /* ATTN */
  // don't search for absolute paths.  N.B., the build shouldn't generate absolute paths!
  X::XMLURL url(systemId);
  if (!url.isRelative()) {
    return nullptr;
  }

  // transcode
  std::string path = XImplCharStr(url.getPath());
  if (path.length() == 0) {
    return nullptr;
  }

  // check absolute again - XMLURL.isRelative() considers URLs with no protocol "relative".
  if (path[0] == '/') {
    return nullptr;
  }

  // Only know XSD files for now.
  const size_t xsd_ext_len = sizeof(xsd_ext)-1;
  if (path.length() < xsd_ext_len ||
      0 != path.compare(path.length()-xsd_ext_len, std::string::npos, xsd_ext)) {
    return nullptr;
  }

  // Find the RIFT XSD lookup paths // ATTN: This is a prime candidate for CmdArgs!
  const char* env = getenv("RIFT_XSD_PATH");
  std::string rw_xsd_path;
  if (env) {
    rw_xsd_path = env;
  } else {
    rw_xsd_path = ":/usr/data/xsd"; // ATTN: This should be a #def somewhere!
  }
  std::vector<std::string> xsd_paths;
  split_line( rw_xsd_path, ':', xsd_paths );

  // don't search if there is nowhere to look
  if (xsd_paths.empty()) {
    return nullptr;
  }

  // try all the paths.
  for (std::vector<std::string>::iterator iter=xsd_paths.begin(); iter != xsd_paths.end(); ++iter ) {
    std::string xsd_filename = *iter;
    xsd_filename.append("/").append(path);
    char xsd_file_realpath[PATH_MAX];
    if (NULL == realpath(xsd_filename.c_str(),xsd_file_realpath)) { 
      continue;
    }

    struct stat stat_buf;
    if (0 == stat(xsd_file_realpath,&stat_buf)) {
      // Wrapper takes ownership of input (it is configurable, but default is to adopt).
      X::LocalFileInputSource* input = new X::LocalFileInputSource(XImplXMLStr(xsd_file_realpath));
      return new X::Wrapper4InputSource(input);
    }
  }

  return nullptr;
}

/*****************************************************************************/
/* XImpleDOMNamespaceResolver methods */

XImpleDOMNamespaceResolver::~XImpleDOMNamespaceResolver()
{}

XImpleDOMNamespaceResolver::XImpleDOMNamespaceResolver(YangModel* model)
    : model_(model)
{
  XalanDOMString root_prefix;
  XalanDOMString root_ns;

  root_prefix = "rw-base";
  root_ns = "\"http://riftio.com/ns/riftware-1.0/rw-base\"";
  namespace_map_[root_prefix] = root_ns;
}

void XImpleDOMNamespaceResolver::cache_prefix(XalanDOMString const & prefix) const
{
  auto module_end = model_->module_end();

  for (auto module_iter = model_->module_begin();
       module_iter != module_end;
       ++module_iter) {

      XalanDOMString module_prefix;
      module_prefix = module_iter->get_prefix();

    if (module_prefix == prefix) {
      XalanDOMString module_ns;
      module_ns = module_iter->get_ns();

      namespace_map_[prefix] = module_ns;
      return;
    }
  }         

  RW_ASSERT_NOT_REACHED();
}

const XalanDOMString* XImpleDOMNamespaceResolver::getNamespaceForPrefix (const XalanDOMString &prefix) const
{

  if (namespace_map_.count(prefix) == 0) {
    cache_prefix(prefix);
  } 

  return &namespace_map_[prefix];
}

const XalanDOMString& XImpleDOMNamespaceResolver::getURI() const
{

  return base_uri_;
}

/*****************************************************************************/
/* XMLAttributeXImpl functions */

XMLAttributeXImpl::XMLAttributeXImpl(XMLDocumentXImpl& xi_document)
: xi_document_(xi_document),
  xrcs_attribute_(nullptr)
{
}

void XMLAttributeXImpl::bind_xrcs_attribute(X::DOMAttr* xrcs_attribute)
{
  RW_ASSERT(xrcs_attribute);
  RW_ASSERT(!xrcs_attribute_);
  void* old_xi_attribute = xrcs_attribute->setUserData(rw_xml_ximpl_bkptr_key, this, &xi_document_.user_attribute_data_handler_);
  RW_ASSERT(!old_xi_attribute);
  xrcs_attribute_ = xrcs_attribute;
}

std::string XMLAttributeXImpl::get_text_value()
{
  RW_ASSERT(xrcs_attribute_);
  return XImplCharStr(xrcs_attribute_->getTextContent());
}

bool XMLAttributeXImpl::get_specified() const
{
  RW_ASSERT(xrcs_attribute_);
  return xrcs_attribute_->getSpecified();
}

std::string XMLAttributeXImpl::get_value()
{
  RW_ASSERT(xrcs_attribute_);
  return XImplCharStr(xrcs_attribute_->getValue());
}

void XMLAttributeXImpl::set_value(std::string value)
{
  RW_ASSERT(xrcs_attribute_);
  xrcs_attribute_->setValue(XImplXMLStr(value.c_str()));
}

std::string XMLAttributeXImpl::get_prefix()
{
  RW_ASSERT(xrcs_attribute_);
  return XImplCharStr(xrcs_attribute_->getPrefix());
}

std::string XMLAttributeXImpl::get_local_name()
{
  RW_ASSERT(xrcs_attribute_);
  const XMLCh* xml_name = xrcs_attribute_->getLocalName();
  if (nullptr == xml_name) {
    xml_name = xrcs_attribute_->getNodeName();
  }
  return XImplCharStr(xml_name);
}

std::string XMLAttributeXImpl::get_name_space()
{
  RW_ASSERT(xrcs_attribute_);
  return XImplCharStr(xrcs_attribute_->getNamespaceURI());
}

XMLDocument& XMLAttributeXImpl::get_document()
{
  return xi_document_;
}

XMLDocumentXImpl& XMLAttributeXImpl::get_xi_document()
{
  return xi_document_;
}

XMLManager& XMLAttributeXImpl::get_manager()
{
  return xi_document_.get_manager();
}

XMLManagerXImpl& XMLAttributeXImpl::get_xi_manager()
{
  return xi_document_.get_xi_manager();
}

void XMLAttributeXImpl::obj_release() noexcept
{
  if (xrcs_attribute_) {
    // Release the xerces node.  This will cause a callback to xrcs_release().
    xrcs_attribute_->release();
  } else {
    // Delete directly - didn't get bound to a xerces node
    delete this;
  }
}

void XMLAttributeXImpl::xrcs_release() noexcept
{
  RW_ASSERT(xrcs_attribute_);
  xrcs_attribute_ = nullptr;
  delete this;
}

XMLAttributeXImpl::~XMLAttributeXImpl()
{
  /*
   * Should never get here when still bound to a xerces attribute!  That
   * means that someone called delete on the attribute directly, instead of
   * releasing the xerces attribute.
   */
  RW_ASSERT(nullptr == xrcs_attribute_);
}



/*****************************************************************************/
/* XMLAttributeListXImpl functions */

XMLAttributeListXImpl::XMLAttributeListXImpl(XMLDocumentXImpl& xi_document)
 :xi_document_(xi_document),
  xrcs_attribute_list_(nullptr)
{
}

void XMLAttributeListXImpl::initialize_xrcs_attribute_list(X::DOMNamedNodeMap* xrcs_attribute_list)
{
  RW_ASSERT(!xrcs_attribute_list_);
  xrcs_attribute_list_ = xrcs_attribute_list;
}

size_t XMLAttributeListXImpl::length()
{
  RW_ASSERT(xrcs_attribute_list_);
  return xrcs_attribute_list_->getLength();
}

XMLAttributeXImpl* XMLAttributeListXImpl::find(const char* local_name, const char* ns)
{
  RW_ASSERT(xrcs_attribute_list_);
  RW_ASSERT(local_name);
  XImplXMLStr xml_name(local_name);

  XImplXMLStr xml_ns;
  if (nullptr != ns && rw_xml_namespace_none != ns) {
    RW_ASSERT(rw_xml_namespace_any != ns);
    RW_ASSERT(rw_xml_namespace_node != ns);
    xml_ns = ns;
  }

  for (size_t i = 0; i < xrcs_attribute_list_->getLength(); i++) {
    X::DOMAttr* xrcs_attribute = dynamic_cast<X::DOMAttr*>(xrcs_attribute_list_->item(i));
    if (    X::XMLString::equals(xrcs_attribute->getLocalName(), xml_name)
         && X::XMLString::equals(xrcs_attribute->getNamespaceURI(), xml_ns)) {
      return xi_document_.map_xrcs_attribute(xrcs_attribute);
    }
  }
  return nullptr;
}

XMLAttributeXImpl* XMLAttributeListXImpl::at(size_t index)
{
  RW_ASSERT(xrcs_attribute_list_);
  X::DOMAttr* xrcs_attribute = dynamic_cast<X::DOMAttr*>(xrcs_attribute_list_->item(index));
  if (nullptr == xrcs_attribute) {
    return nullptr;
  }
  return xi_document_.map_xrcs_attribute(xrcs_attribute);
}

XMLDocument& XMLAttributeListXImpl::get_document()
{
  return xi_document_;
}

XMLDocumentXImpl& XMLAttributeListXImpl::get_xi_document()
{
  return xi_document_;
}

XMLManager& XMLAttributeListXImpl::get_manager()
{
  return xi_document_.get_xi_manager();
}

XMLManagerXImpl& XMLAttributeListXImpl::get_xi_manager()
{
  return xi_document_.get_xi_manager();
}

void XMLAttributeListXImpl::obj_release() noexcept
{
  delete this;
}
