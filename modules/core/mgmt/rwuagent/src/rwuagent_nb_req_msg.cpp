
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
 * @file rwuagent_nb_req_msg.cpp
 *
 * Management agent northbound request handler for RW.Msg
 */

#include "rwuagent.hpp"
#include "rw_xml_validate_dom.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NbReqMsg::NbReqMsg(
  Instance* instance,
  const NetconfReq* req,
  NetconfRsp_Closure rsp_closure,
  void* closure_data )
: NbReq(
    instance,
    "NbReqMsg",
    RW_MGMTAGT_NB_REQ_TYPE_RWMSG),
  req_(req),
  rsp_closure_(rsp_closure),
  closure_data_(closure_data)
{
}

NbReqMsg::~NbReqMsg()
{
}

rwsched_dispatch_queue_t NbReqMsg::get_execution_q() const
{ 
  return instance_->get_queue(QueueType::DefaultConcurrent);
}

StartStatus NbReqMsg::execute()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "request");

  if (!instance_->mgmt_handler()->is_ready_for_nb_clients()) {
    return NbReq::respond( nullptr, "Agent not ready" );
  }

  if (!req_->has_operation) {
    return NbReq::respond( nullptr, "No operation" );
  }

  if (req_->xml_blob->xml_blob) {
    request_str_ = std::string(req_->xml_blob->xml_blob);
  }
  operation_ = req_->operation;

  RWMEMLOG_BARE (memlog_buf_, RWMEMLOG_MEM2,
       RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, request_str_.c_str()));

  switch (operation_) {
    case nc_edit_config:{
      RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "netconf edit config",);
      RW_ASSERT (req_->has_edit_config_operation);
      RW_MA_NBREQ_LOG (this, ClientDebug, "Edit Config Operation", req_->xml_blob->xml_blob);

      auto edit_xact = new SbReqEditConfig(
          instance_,
          this,
          RequestMode::XML,
          req_->xml_blob->xml_blob);
      return edit_xact->start_xact();
    }
    case nc_default:
    case nc_rpc_exec: {
      RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "netconf rpc",);
      RW_MA_NBREQ_LOG (this, ClientDebug, "RPC Operation from Msg", req_->xml_blob->xml_blob);

      std::string error;
      auto xml_dom(
          instance_->xml_mgr()->create_document_from_string(
            request_str_.c_str(), error, false));
      if (xml_dom) {
        XMLDocMerger merger(instance_->xml_mgr(), instance_->dom());
        XMLDocument::uptr_t tmp_rpc_and_config_dom = merger.copy_and_merge(
                                                        xml_dom.get());
        if (!tmp_rpc_and_config_dom) {
          std::string log_str = "Failed to add defaults to rpc dom.";
          return NbReq::respond(nullptr, log_str.c_str());
        }

        ValidationStatus result = validate_dom(tmp_rpc_and_config_dom.get(), 
                                               instance_);
        if (result.failed()) {
          std::string log_str = "Validation failed: " + result.reason();
          return NbReq::respond(nullptr, log_str.c_str());
                                
        }
      }

      auto rpc_xact = new SbReqRpc(
          instance_,
          this,
          RequestMode::XML,
          req_->xml_blob->xml_blob );
      return rpc_xact->start_xact();
    }
    case nc_get: {
      RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "netconf get",);
      RW_MA_NBREQ_LOG (this, ClientDebug, "Get Operation", req_->xml_blob->xml_blob);

      auto get_xact = new SbReqGet(
          instance_,
          this,
          RequestMode::XML,
          req_->xml_blob->xml_blob );

      return get_xact->start_xact();
    }
    case nc_get_config: {
      RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "netconf config",);
      RW_MA_NBREQ_LOG (this, ClientDebug, "Get Config", req_->xml_blob->xml_blob);
      std::string error;
      auto resp_doc(
          instance_->xml_mgr()->create_document_from_string(
              instance_->dom()->to_string().c_str(), error, false));

      return respond(nullptr, std::move(resp_doc));
    }
    default:
      break;
  }

  return NbReq::respond(nullptr, "Unknown operation");
}

StartStatus NbReqMsg::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "respond with dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  if (instance_->get_request_mode() == RequestMode::CONFD
      && operation_ == nc_edit_config) {
    auto edit_xact = static_cast<SbReqEditConfig*>(sbreq);
    sb_delta_ = edit_xact->move_delta();
    sb_delete_ks_ = edit_xact->move_deletes();
    commit_changes();// sb_delta_, sb_delte_ks_ are hidden parameters to commit_changes
  }

  // Set the response from the dom
  std::string rsp_str;
  if (rsp_dom.get() && rsp_dom->get_root_node()) {
    rsp_str = rsp_dom->get_root_node()->to_string();

    switch (operation_) {
    case nc_get:
      rsp_dom->merge(instance_->dom());
      // FALLTHROUGH
    case nc_get_config:
    {
      rsp_str = std::move(rsp_dom->to_string());

      std::string error;
      auto filter_dom(
          instance_->xml_mgr()->create_document_from_string(
            request_str_.c_str(), error, false));

      rw_status_t ret = rsp_dom->subtree_filter(filter_dom.get());
      if (ret == RW_STATUS_SUCCESS) {
        rsp_str = std::move(rsp_dom->to_string());
      } else {
        RW_MA_NBREQ_LOG (this, ClientError, "Failed in subtree filtering",
            request_str_.c_str());
      }
      break;
    }
    default:
      break;
    };
  }

  return send_response( rsp_str );
}

StartStatus NbReqMsg::respond(
  SbReq* sbreq,
  NetconfErrorList* nc_errors )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "respond with error",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  std::string rsp_str;
  nc_errors->to_xml(instance_->xml_mgr(), rsp_str);
  instance_->last_error_ = rsp_str;

  return send_response( rsp_str );
}

StartStatus NbReqMsg::send_response(
  std::string response )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "send message response");
  XmlBlob blob;
  xml_blob__init( &blob );
  blob.xml_blob = (char*)response.c_str();

  NetconfRsp rsp;
  netconf_rsp__init( &rsp );
  rsp.xml_response = &blob;

  RW_MA_NBREQ_LOG (this, ClientDebug, "msg response", response.c_str());
  rsp_closure_( &rsp, closure_data_ );

  delete this;
  return StartStatus::Done;
}

