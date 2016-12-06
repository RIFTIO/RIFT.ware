
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
 * @file rw_xml_dom_merger.hpp
 * @brief Helper to validate config DOM
 */

#include <algorithm>
#include <iostream>
#include <sstream>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "yangmodel.h"

#include "rw_xml.h"
#include "rwuagent.hpp"
#include "rw_xml_validate_dom.hpp"

using namespace rw_yang;


namespace rw_yang {
namespace {

rw_status_t validate_leafref(XMLDocument* dom,
                             XMLNode * child_xml_node,
                             YangNode * child_yang_node)
{
  RW_ASSERT(child_xml_node);
  RW_ASSERT(child_yang_node);
  RW_ASSERT(child_yang_node->is_leafy());

  // ATTN: what about leaf-lists of leafrefs?
  YangType * child_yang_type = child_yang_node->get_type();
  if (child_yang_type->get_leaf_type() == RW_YANG_LEAF_TYPE_LEAFREF) {
    std::string const xpath = child_yang_node->get_leafref_path_str();

    return dom->evaluate_xpath(child_xml_node, xpath);
  }
  return RW_STATUS_SUCCESS;
}

ValidationStatus validate_mandatory(XMLDocument* dom,
                                    XMLNode * root_xml_node,
                                    YangNode * root_yang_node)
{
  RW_ASSERT(root_xml_node);
  RW_ASSERT(root_yang_node);
  RW_ASSERT(!root_yang_node->is_leafy());


  std::unordered_set<YangNode*> mandatory_yang_leaves;
  std::unordered_set<YangNode*> present_xml_leaves;

  // collect leafy children of the yang node

  YangNodeIter child_itr = root_yang_node->child_begin();
  YangNodeIter child_end = root_yang_node->child_end();

  if (root_yang_node->is_rpc()) {
    // dereference to the input node becuase the input xml tags are not in the request
    child_itr = root_yang_node->get_first_child()->child_begin();
    child_end = root_yang_node->get_first_child()->child_end();
  } else {
    child_itr = root_yang_node->child_begin();
    child_end = root_yang_node->child_end();
  }

  while (child_itr != child_end) {
    if (child_itr->is_leafy() && child_itr->is_mandatory()) {
      mandatory_yang_leaves.insert(child_itr.raw());
    }
    child_itr++;
  }  
  
  // collect leafy children of the xml node
  XMLNode * leaf_xml_runner = root_xml_node->get_first_child();

  while (leaf_xml_runner != nullptr) {
    YangNode * leaf_yang_node = leaf_xml_runner->get_yang_node();
    RW_ASSERT(leaf_yang_node);
    if (leaf_yang_node->is_leafy() && leaf_yang_node->is_mandatory()) {
      present_xml_leaves.insert(leaf_yang_node);
    }

    leaf_xml_runner = leaf_xml_runner->get_next_sibling();
  }

  rw_status_t status = mandatory_yang_leaves == present_xml_leaves ? RW_STATUS_SUCCESS : RW_STATUS_FAILURE;

  std::string reason = "";
  if (status == RW_STATUS_FAILURE) {
    std::stringstream error_message; 
    error_message << root_yang_node->get_ns()
                  << " " << root_yang_node->get_name()
                  << " is missing children : {";

    std::list<YangNode*> difference;
    std::set_difference(mandatory_yang_leaves.begin(),mandatory_yang_leaves.end(),
                        present_xml_leaves.begin(), present_xml_leaves.end(),
                        std::inserter(difference, difference.begin()));

    for (auto n : difference) {
      error_message << n->get_name() << ", ";
    }
    error_message <<  "}";
    reason = error_message.str();
  }
  
  ValidationStatus ret(status, reason);
  return ret;
}

ValidationStatus walk_xml_dom_and_validate(XMLDocument* dom,
                                           XMLNode * xml_node,
                                           rwlog_ctx_t* log)
{

  XMLNode* cnode = xml_node->get_first_child();

  while (cnode) {
    auto child_xml_node = cnode;
    cnode = cnode->get_next_sibling();

    if (child_xml_node->get_local_name() == "#text") {
      continue;
    }



    YangNode* child_yang_node = child_xml_node->get_yang_node();
    if (child_yang_node == nullptr) {
      std::stringstream log_strm; 
      log_strm << "XML node is missing associated yang node: "
               << child_yang_node->get_ns()
               << " " << child_yang_node->get_name();
      RW_LOG (log, DommgrNotice, log_strm.str().c_str());

      std::stringstream error_strm;
      error_strm << "Unexpected element: "
                 << child_yang_node->get_name();

      ValidationStatus ret(RW_STATUS_FAILURE, error_strm.str());
      return ret;
    }

    if (child_yang_node->is_leafy()) {
      if (!child_xml_node->is_leaf_node()) {
        std::stringstream log_strm; 
        log_strm << "XML is_leafy does not match YANG is_leafy: "
                 << child_yang_node->get_ns()
                 << " " << child_yang_node->get_name();
        RW_LOG (log, DommgrNotice, log_strm.str().c_str());
        ValidationStatus ret(RW_STATUS_FAILURE, log_strm.str());
        return ret;
      }

      rw_status_t leafref_result = validate_leafref(dom, child_xml_node, child_yang_node);

      if (leafref_result == RW_STATUS_FAILURE) {
        std::stringstream log_strm;
        log_strm << "Target of leafref does not exist: "
                 << child_yang_node->get_leafref_path_str()
                 << " with value: " << child_xml_node->get_text_value();
        RW_LOG (log, DommgrNotice, log_strm.str().c_str());
        ValidationStatus ret(RW_STATUS_FAILURE, log_strm.str());
        return ret;
      }
    } else {
      ValidationStatus mandatory_result = validate_mandatory(dom, child_xml_node, child_yang_node);

      if (mandatory_result.failed()) {
        RW_LOG (log, DommgrNotice, mandatory_result.reason().c_str());
        return mandatory_result;
      }
      
      ValidationStatus result = walk_xml_dom_and_validate(dom, child_xml_node, log);
      if (result.failed()) {
        return result;
      }
    }
  }

  ValidationStatus ret(RW_STATUS_SUCCESS);
  return ret;
}

}

ValidationStatus::ValidationStatus(rw_status_t const status)
    : status_(status),
      reason_("")
{}

ValidationStatus::ValidationStatus(rw_status_t const status, std::string const & reason)
    : status_(status),
      reason_(reason)
{}

ValidationStatus::ValidationStatus(ValidationStatus const & other)
    : ValidationStatus(other.status_, other.reason_)
{}

ValidationStatus::ValidationStatus(ValidationStatus const && other)
    : ValidationStatus(other.status_, other.reason_)
{}

rw_status_t ValidationStatus::status()
{
  return status_;
}

bool ValidationStatus::failed()
{
  return status_ == RW_STATUS_FAILURE;
}

std::string ValidationStatus::reason()
{
  return reason_;
}

ValidationStatus validate_dom(XMLDocument* dom, rw_uagent::Instance* inst)
{
  RW_ASSERT (dom);
  rwlog_ctx_t* log = nullptr;

  if (!inst) {
    log = rwlog_init("RW.XMLDomValidation");
  } else {
    log = inst->rwlog();
  }

  XMLNode* root_xml_node = dom->get_root_node();
  ValidationStatus result = walk_xml_dom_and_validate(dom, root_xml_node, log);

  if (!inst) {
    rwlog_close(log, false);
  }

  return result;
}

}
