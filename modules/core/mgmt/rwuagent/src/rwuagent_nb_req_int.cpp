
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
 * @file rwuagent_nb_req_int.cpp
 *
 * Management agent internal northbound request handler
 */

#include "rwuagent.hpp"
#include "rw_xml_dom_merger.hpp"
#include "rw_xml_validate_dom.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NbReqInternal::NbReqInternal(
    Instance* instance,
    SbReqRpc* parent_rpc,
    const RwMgmtagtDts_PbRequest* pb_req )
: NbReq(
    instance,
    "NbReqInternal",
    RW_MGMTAGT_NB_REQ_TYPE_INTERNAL ),
  parent_rpc_(parent_rpc),
  pb_req_(pb_req)
{
  RW_ASSERT(parent_rpc_);
  RW_ASSERT(pb_req_);
  // ATTN: Own, copy, or ref pb_req?

  request_mode_ = instance->get_request_mode();
}

NbReqInternal::~NbReqInternal()
{
  // Signal the next edit-cfg request
  async_dequeue_pb_req();
}

StartStatus NbReqInternal::execute()
{
  if (!instance_->mgmt_handler()->is_ready_for_nb_clients()) {
    send_error( 
        RW_YANG_NETCONF_OP_STATUS_RESOURCE_DENIED,
        "Agent not ready" );
    return StartStatus::Done;
  }

  if (!instance_->dts()->is_ready()) {
    send_error(
        RW_YANG_NETCONF_OP_STATUS_FAILED,
        "Northbound interfaces not enabled" );
    return StartStatus::Done;
  }

/*
  ATTN: TIME this
 */
  // Parse the keyspec and message data.
  UniquePtrKeySpecPath::uptr_t ks;
  UniquePtrProtobufCMessage<>::uptr_t msg;

  if (pb_req_->has_keyspec) {
    // Data is defined by a keyspec and message.
    rw_keyspec_path_t* new_ks = nullptr;
    ProtobufCMessage* new_msg = nullptr;

    auto rs = rw_keyspec_path_deserialize_msg_ks_pair(
      nullptr/*instance*/,
      instance_->ypbc_schema(),
      &pb_req_->data,
      &pb_req_->keyspec,
      &new_msg,
      &new_ks );

    ks.reset(new_ks);
    msg.reset(new_msg);

    if (RW_STATUS_SUCCESS != rs) {
      send_error(
        RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
        "Unable to parse keyspec or data" );
      return StartStatus::Done;
    }

  } else if (pb_req_->xpath) {
    // Data is defined by an xpath and message.
    RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "xpath",
      RWMEMLOG_ARG_STRCPY_MAX(pb_req_->xpath, RWMEMLOG_ARG_SIZE_MAX_BYTES));

    ks.reset(rw_keyspec_path_from_xpath(
                         instance_->ypbc_schema(),
                         pb_req_->xpath,
                         RW_XPATH_KEYSPEC,
                         nullptr/*instance*/) 
            );

    if (!ks.get()) {
      send_error(
            RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
            "Bad keyspec");
      return StartStatus::Done;
    }

    const rw_yang_pb_msgdesc_t* msgdesc = 
      rw_schema_pbcm_get_msg_msgdesc(
                    nullptr/*instance*/,
                    (ProtobufCMessage*)ks.get(),
                    instance_->ypbc_schema() );
    RW_ASSERT(msgdesc);

    if (pb_req_->has_data && pb_req_->data.data) {
      msg.reset( protobuf_c_message_unpack(
        nullptr/*instance*/,
        msgdesc->pbc_mdesc,
        pb_req_->data.len,
        pb_req_->data.data ) );

      if (!msg.get()) {
        send_error(
            RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
            "Bad data" );
        return StartStatus::Done;
      }
    }

  } else {
    send_error(
        RW_YANG_NETCONF_OP_STATUS_MISSING_ELEMENT,
        "No key" );
    return StartStatus::Done;
  }

  rw_yang_netconf_op_status_t ncs;
  XMLDocument::uptr_t xml_dom {};

  if (pb_req_->has_data && pb_req_->data.data != nullptr) {
    xml_dom = std::move(
      instance_->xml_mgr()->create_document_from_pbcm(
          msg.get(), ncs, true/*rooted*/, ks.get() ) );

    if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
      send_error( ncs, "Protobuf conversion failed" );
      return StartStatus::Done;
    }
  } else if (pb_req_->blobxml != nullptr) {
    // kill confd hack
    std::string error_out;
    xml_dom = std::move(
      instance_->xml_mgr()->create_document_from_string(
          pb_req_->blobxml, error_out, false));

    RWMEMLOG_BARE (memlog_buf_, RWMEMLOG_MEM2,
        RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, pb_req_->blobxml));
  } 

  SbReq* sbreq = nullptr;
  std::string str;

  switch (pb_req_->request_type) {
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_EDIT_CONFIG: {

      if ((!pb_req_->has_data || pb_req_->data.data == nullptr)
           && (pb_req_->blobxml == nullptr)) {
        sbreq = new SbReqEditConfig( instance_, this, request_mode_, std::move(ks) );
      } else {
        str = xml_dom->to_string(); /*ATTN: stupidly expensive!*/
        sbreq = new SbReqEditConfig( instance_, this, request_mode_, str.c_str());
      }

      RW_MA_NBREQ_LOG( this, ClientDebug, "Internal Request", str.c_str() );

      RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "internal edit",
                RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
                RWMEMLOG_ARG_STRCPY_MAX(str.c_str(), RWMEMLOG_ARG_SIZE_MAX_BYTES) );
      break;
    }
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_RPC: {

      XMLDocMerger merger(instance_->xml_mgr(), instance_->dom());
      auto tmp_rpc_and_config_dom = std::move(merger.copy_and_merge(xml_dom.get()));

      if (tmp_rpc_and_config_dom.get() == nullptr) {
        std::string log_str = std::string("Failed to add defaults to rpc dom: ") + pb_req_->blobxml;
        send_error(
            RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
            log_str.c_str());
        return StartStatus::Done;
      }

      ValidationStatus result = validate_dom(tmp_rpc_and_config_dom.get(), instance_);
      if (result.failed()) {
        std::string log_str = "Validation failed: " + result.reason();
        send_error(
            RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
            log_str.c_str());
        return StartStatus::Done;
      }

      str = xml_dom->to_string(); /*ATTN: stupidly expensive!*/
      RW_MA_NBREQ_LOG( this, ClientDebug, "Internal RPC", str.c_str() );
      sbreq = new SbReqRpc( instance_, this, request_mode_, str.c_str() );

      RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "internal rpc",
        RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
        RWMEMLOG_ARG_STRCPY_MAX(str.c_str(), RWMEMLOG_ARG_SIZE_MAX_BYTES) );
      break;
    }
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET: {
      str = xml_dom->to_string(); /*ATTN: stupidly expensive!*/
      RW_MA_NBREQ_LOG( this, ClientDebug, "Internal GET", str.c_str() );

      XMLNode* root = xml_dom->get_root_node();
      if (!root || !root->get_first_child()) {
        send_error (RW_YANG_NETCONF_OP_STATUS_TOO_BIG,
            "Data too big");
        return StartStatus::Done;
      }

      sbreq = new SbReqGet( instance_, this, request_mode_, str.c_str() );

      RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "internal get",
        RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
        RWMEMLOG_ARG_STRCPY_MAX(str.c_str(), RWMEMLOG_ARG_SIZE_MAX_BYTES) );
      break;
    }

    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET_CONFIG: {
      RW_MA_NBREQ_LOG (this, ClientDebug, "Get Config", xml_dom->to_string().c_str());
      std::string error;
      auto resp_doc(
            instance_->xml_mgr()->create_document_from_string(
               instance_->dom()->to_string().c_str(), error, false));
      return respond(nullptr, std::move(resp_doc));
    }
    default:
      send_error(
        RW_YANG_NETCONF_OP_STATUS_BAD_ELEMENT,
        "Bad request-type" );
      return StartStatus::Done;
  }

  // ATTN: Need to keep sbreq, to propagate aborts?
  return sbreq->start_xact();
}

std::string NbReqInternal::get_response_string(
    rw_yang::XMLDocument::uptr_t rsp_dom)
{
  std::string resp_str;

  switch (pb_req_->request_type)
  {
  case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET:
  {
    // Fill the defaults
    XMLNode* root = rsp_dom->get_root_node();
    RW_ASSERT(root);
    XMLNode* child = root->get_first_element();
    if (child) {
      child->fill_defaults();
    }
    
    rsp_dom->merge(instance_->dom());
  }
    // FALLTHROUGH
  case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET_CONFIG:
  {
    resp_str = std::move(rsp_dom->to_string());

    std::string error;
    auto filter_dom(
        instance_->xml_mgr()->create_document_from_string(
            pb_req_->blobxml, error, false));

    rw_status_t ret = rsp_dom->subtree_filter(filter_dom.get());

    if (ret == RW_STATUS_SUCCESS) {
      resp_str = std::move(rsp_dom->to_string());
    } else {
      RW_MA_NBREQ_LOG( this, ClientError, "Subtree filtering failed", "");
    }

    break;
  }

  case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_RPC: {

    XMLNode* root = rsp_dom->get_root_node();
    RW_ASSERT(root);
    XMLNode* rpc_node = root->get_first_element();
    if (rpc_node) {
      rpc_node->fill_rpc_output_with_defaults();
    }
  }

  default:
    resp_str = rsp_dom->to_string();
    break;
  };

  return resp_str;
}

StartStatus NbReqInternal::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{

  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  switch (pb_req_->request_type) {
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_EDIT_CONFIG: {
      if (pb_req_->blobxml == nullptr) {
        // PBRAW request
        if (instance_->get_request_mode() != RequestMode::XML) {
          auto edit_xact = static_cast<SbReqEditConfig*>(sbreq);
          sb_delta_ = edit_xact->move_delta();
          sb_delete_ks_ = edit_xact->move_deletes();
          commit_changes();// sb_delta_, sb_delte_ks_ are hidden parameters to commit_changes
        }
        send_success( nullptr, nullptr );
        return StartStatus::Done;
      }
      break;
    }
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_RPC: {
      if (pb_req_->blobxml == nullptr) {
        // PBRAW request
        auto root = rsp_dom->get_root_node();
        RW_ASSERT(root);
        auto node = root->get_first_element();
        RW_ASSERT(!node); // does not non-empty handle DOM responses at this time

        send_success( nullptr, nullptr );
        return StartStatus::Done;
      }
      break;
    }
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET:
    case RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET_CONFIG:
    {
      if (pb_req_->blobxml == nullptr) {
        return StartStatus::Done;
      }
      break;
    }
    default:
      RW_ASSERT_NOT_REACHED();
  }

  std::string resp_str = std::move(get_response_string(std::move(rsp_dom)));

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts_PbRequest, pb_rsp);
  pb_rsp.blobxml = (char*)resp_str.c_str();

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts, rsp);
  rsp.pb_request = &pb_rsp;

  parent_rpc_->internal_done(
      &RWPB_G_PATHSPEC_VALUE(RwMgmtagtDts_output_MgmtAgentDts)->rw_keyspec_path_t,
      &rsp.base );

  delete this;
  return StartStatus::Done;
}

StartStatus NbReqInternal::respond(
  SbReq* sbreq,
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with keyspec and message",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  if (pb_req_->blobxml == nullptr) {
    // PBRAW request
    return StartStatus::Done;
  }

  std::string xml_string;
  instance_->xml_mgr()->pb_to_xml((ProtobufCMessage*)msg, xml_string);

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts_PbRequest, pb_rsp);

  XMLDocument::uptr_t rsp_dom {};

  if(pb_req_->request_type == RW_MGMTAGT_DTS_PB_REQUEST_TYPE_GET) {
    rsp_dom = std::move( instance_->xml_mgr()->create_document(
        instance_->yang_model()->get_root_node()) );

    rsp_dom->get_root_node()->merge( ks, (ProtobufCMessage*) msg );
    xml_string = std::move(get_response_string(std::move(rsp_dom)));
  }

  pb_rsp.blobxml = (char*)xml_string.c_str();

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts, rsp);
  rsp.pb_request = &pb_rsp;

  parent_rpc_->internal_done(
      &RWPB_G_PATHSPEC_VALUE(RwMgmtagtDts_output_MgmtAgentDts)->rw_keyspec_path_t,
      &rsp.base );

  delete this;
  return StartStatus::Done;
}

StartStatus NbReqInternal::respond(
  SbReq* sbreq,
  NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with error",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );
  send_error( nc_errors );
  return StartStatus::Done;
}

void NbReqInternal::send_success(
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "send success",
    RWMEMLOG_ARG_PRINTF_INTPTR("parent rpc sbreq=0x%" PRIX64,(intptr_t)parent_rpc_) );

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts_PbRequest, pb_rsp);
  UniquePtrProtobufCMessageUseBody<>::uptr_t pb_rsp_cleanup( &pb_rsp.base );

  if (ks) {
    auto rs = rw_keyspec_path_serialize_dompath( ks, nullptr, &pb_rsp.keyspec );
    if (RW_STATUS_SUCCESS == rs) {
      pb_rsp.has_keyspec = true;
      pb_rsp.xpath = rw_keyspec_path_to_xpath(
          ks, instance_->ypbc_schema(), nullptr );
    }
  }

  if (msg) {
    pb_rsp.data.data = protobuf_c_message_serialize(
        nullptr, msg, &pb_rsp.data.len );
    if (pb_rsp.data.data) {
      pb_rsp.has_data = true;
    }
  }

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts, rsp);
  rsp.pb_request = &pb_rsp;

  parent_rpc_->internal_done(
    &RWPB_G_PATHSPEC_VALUE(RwMgmtagtDts_output_MgmtAgentDts)->rw_keyspec_path_t,
    &rsp.base );

  delete this;
}

void NbReqInternal::send_error(
  rw_yang_netconf_op_status_t ncs,
  const std::string& err_str )
{
  NetconfErrorList nc_errors;
  nc_errors.add_error()
    .set_error_message( err_str.c_str() )
    .set_rw_error_tag( ncs );
  send_error( &nc_errors );
}

void NbReqInternal::send_error(
  NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "send error",
    RWMEMLOG_ARG_PRINTF_INTPTR("parent rpc sbreq=0x%" PRIX64,(intptr_t)parent_rpc_) );
  RW_ASSERT(parent_rpc_);

  std::string rsp_str;
  nc_errors->to_xml(instance_->xml_mgr(), rsp_str);

  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts, rsp);
  RWPB_M_MSG_DECL_INIT(RwMgmtagtDts_output_MgmtAgentDts_PbRequest, pb_rsp);
  rsp.pb_request = &pb_rsp;
  pb_rsp.error = const_cast<char*>(rsp_str.c_str());

  parent_rpc_->internal_done(
    &RWPB_G_PATHSPEC_VALUE(RwMgmtagtDts_output_MgmtAgentDts)->rw_keyspec_path_t,
    &rsp.base );

  delete this;
}

void NbReqInternal::async_dequeue_pb_req()
{
  if (pb_req_->request_type != RW_MGMTAGT_DTS_PB_REQUEST_TYPE_EDIT_CONFIG) {
    return;
  }

  rwsched_dispatch_async_f(instance_->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(instance_->rwsched()),
                           instance_,
                           dequeue_pb_req_cb);
}

void NbReqInternal::dequeue_pb_req_cb(void* ud)
{
  RW_ASSERT (ud);
  auto inst = static_cast<Instance*>(ud);
  inst->dequeue_pb_request();
}

