
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
 * @file rwuagent_sb_req_rpc.cpp
 *
 * Management agent southbound request support for RPC
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;

SbReqRpc::SbReqRpc(
  Instance* instance,
  NbReq* nbreq,
  RequestMode request_mode,
  const char* xml_fragment )
: SbReq(
    instance,
    nbreq,
    RW_MGMTAGT_SB_REQ_TYPE_RPC,
    request_mode,
    "SbReqRpcString",
    xml_fragment )
{
  RW_MA_SBREQ_LOG( this, __FUNCTION__, xml_fragment );

  std::string error_out;
  XMLDocument::uptr_t rpc_dom(
      instance_->xml_mgr()->create_document_from_string(xml_fragment, error_out, false) );

  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM6, "sbreq rpc",
      RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)(this)),
      RWMEMLOG_ARG_STRCPY_MAX(xml_fragment, RWMEMLOG_ARG_SIZE_MAX_BYTES)
      );

  if (!rpc_dom.get()) {
    RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "parse failed",
      RWMEMLOG_ARG_STRCPY_MAX(error_out.c_str(), RWMEMLOG_ARG_SIZE_MAX_BYTES));
    error_out = "Unable to parse XML string: " + error_out;
    add_error().set_error_message( error_out.c_str() );
    return;
  }

  auto root = rpc_dom->get_root_node();
  RW_ASSERT(root); // must have DOM
  auto node = root->get_first_element();
  if (!node) {
    add_error().set_error_message( "Did not find rpc root element" );
    return;
  }

  if (request_mode_ == RequestMode::XML) {
    // Fill defaults for the RPC input type children
    node->fill_rpc_input_with_defaults();
  }

  init_rpc_input( node );
}

SbReqRpc::SbReqRpc(
  Instance* instance,
  NbReq* nbreq,
  RequestMode request_mode,
  XMLDocument* rpc_dom )
: SbReq(
    instance,
    nbreq,
    RW_MGMTAGT_SB_REQ_TYPE_RPC,
    request_mode,
    "SbReqRpcDom" ,
    rpc_dom->to_string().c_str())
{
  auto node = rpc_dom->get_root_node();
  RW_ASSERT(node); // must have DOM
  auto yn = node->get_yang_node();
  if (yn == instance_->yang_model()->get_root_node()) {
    node = node->get_first_element();
  }
  RW_ASSERT(node); // must have RPC

  std::string capture_temporary;
  RW_MA_SBREQ_LOG( this, __FUNCTION__, (capture_temporary=node->to_string()).c_str() );
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM6, "sbreq rpc",
       RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)(this)),
       RWMEMLOG_ARG_STRCPY_MAX(
         (capture_temporary=node->to_string()).c_str(), RWMEMLOG_ARG_SIZE_MAX_BYTES)
       );

  init_rpc_input( node );
}

void SbReqRpc::init_rpc_input(
  XMLNode* node )
{
  ProtobufCMessage* msg = nullptr;
  rw_yang_netconf_op_status_t ncs = node->to_rpc_input( &msg );
  rpc_input_.reset( msg );

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    // ATTN: Need to get XML error string out and back to client.
    add_error().set_error_message( "Could not convert DOM to RPC input message" );
    rpc_input_.reset( nullptr );
    return;
  }

  update_stats (RW_UAGENT_NETCONF_PARSE_REQUEST);
}

SbReqRpc::~SbReqRpc()
{
  if (xact_) {
    rwdts_xact_unref (xact_, __PRETTY_FUNCTION__, __LINE__);
    xact_ = nullptr;
  }
  instance_->update_stats( sbreq_type(), req_.c_str(), &statistics_ );
}

StartStatus SbReqRpc::start_xact_int()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "start rpc");
  if (nc_errors_.length()) {
    return done_with_error();
  }

  if (!rpc_input_.get()) {
    RW_MA_SBREQ_LOG (this, __FUNCTION__, "No message");
    return done_with_error( "No message" );
  }

  std::string ns = protobuf_c_message_descriptor_xml_ns( rpc_input_->descriptor );
  if (ns == uagent_yang_ns) {
    // ATTN: Need some other way rather than checking the element name.
    std::string rpcn = protobuf_c_message_descriptor_xml_element_name( rpc_input_->descriptor );
    if (rpcn == "show-system-info") {
      return start_ssi();
    } else if (rpcn == "show-agent-logs") {
      return show_agent_logs();
    }
  }

  /* Start the transcation on the main queue, both the
     dts xact and the local rpcs.
   */
  rwsched_dispatch_async_f(instance_->rwsched_tasklet(),
                           rwsched_dispatch_get_main_queue(instance_->rwsched()),
                           this,
                           start_xact_async);

  return StartStatus::Async;
}

StartStatus SbReqRpc::start_ns_mgmtagt()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "process local agent rpc");
  RWPB_M_MSG_DECL_SAFE_CAST(RwMgmtagt_input_MgmtAgent, mgmt_agent, rpc_input_.get());

  UniquePtrProtobufCMessage<>::uptr_t dup_msg;
  if (!mgmt_agent) {
    dup_msg.reset(
      protobuf_c_message_duplicate(
        nullptr,
        rpc_input_.get(),
        RWPB_G_MSG_PBCMD(RwMgmtagt_input_MgmtAgent) ) );
    if (!dup_msg.get()) {
      return done_with_error("Cannot coerce RPC to rw-mgmtagt:mgmt-agent");
    }
    mgmt_agent = RWPB_M_MSG_SAFE_CAST(RwMgmtagt_input_MgmtAgent, dup_msg.get());
  }

  if (mgmt_agent->dts_trace) {
    if (dts_) {
      dts_->trace_next();
    }
    return done_with_success();
  } else if (mgmt_agent->has_show_agent_dom) {
    RWPB_M_MSG_DECL_INIT(RwMgmtagt_output_MgmtAgent, rsp);
    UniquePtrProtobufCMessageUseBody<>::uptr_t pb_rsp_cleanup( &rsp.base );

    auto dom_str = instance_->dom()->to_string_pretty();
    rsp.agent_dom = strdup(dom_str.c_str());

    return done_with_success(
        &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_output_MgmtAgent)->rw_keyspec_path_t, &rsp.base );
  }

  return done_with_error( "No choice selected" );
}

StartStatus SbReqRpc::start_ns_mgmtagt_dts()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "process internal rpc dts request");
  RWPB_M_MSG_DECL_SAFE_CAST(RwMgmtagtDts_input_MgmtAgentDts, mgmt_agent, rpc_input_.get());

  UniquePtrProtobufCMessage<>::uptr_t dup_msg;
  if (!mgmt_agent) {
    dup_msg.reset(
      protobuf_c_message_duplicate(
        nullptr,
        rpc_input_.get(),
        RWPB_G_MSG_PBCMD(RwMgmtagtDts_input_MgmtAgentDts) ) );

    if (!dup_msg.get()) {
      return done_with_error("Cannot coerce RPC to rw-mgmtagt-dts:mgmt-agent-dts");
    }

    mgmt_agent = RWPB_M_MSG_SAFE_CAST(RwMgmtagtDts_input_MgmtAgentDts, dup_msg.get());
  }

  if (mgmt_agent->pb_request) {
    return start_pb_request();
  }

  return done_with_error( "No choice selected" );
}

StartStatus SbReqRpc::start_pb_request()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "process pb-request rpc");

  RWPB_M_MSG_DECL_CAST(RwMgmtagtDts_input_MgmtAgentDts, req, rpc_input_.get());
  RW_ASSERT(req->pb_request);

  // Send back into the agent as a new northbound request.
  auto nb_req_internal = new NbReqInternal(
    instance_,
    this,
    req->pb_request );

  if (req->pb_request->has_request_type &&
      req->pb_request->request_type == RW_MGMTAGT_DTS_PB_REQUEST_TYPE_EDIT_CONFIG)
  {
    // We are on main queue
    instance_->enqueue_pb_request(nb_req_internal);
    return StartStatus::Async;
  }

  return nb_req_internal->execute();
}

#define SSI_CONFD_ACTION_TIMEOUT 300 // ATTN:- This has to be changed to periodically push it.

StartStatus SbReqRpc::start_ssi()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "process show-system-information rpc");
  RWPB_M_MSG_DECL_CAST(RwMgmtagt_input_ShowSystemInfo, req, rpc_input_.get());

  // This will take significantly larger time. Increase the time-out on the NB
  // side.
  nbreq_->set_timeout(SSI_CONFD_ACTION_TIMEOUT_SEC);

  // Create a new ShowSysInfo and execute the command.
  ShowSysInfo *ssi = new ShowSysInfo(
      instance_, this, req);

  return ssi->execute();
}

StartStatus SbReqRpc::show_agent_logs()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "process show-agent-logs rpc");
  nbreq_->set_timeout(SHOW_LOGS_ACTION_TIMEOUT_SEC);

  return instance_->mgmt_handler()->handle_rpc(this, rpc_input_.get());
}

void SbReqRpc::start_xact_async(void *ud)
{
  SbReqRpc *rpc = static_cast<SbReqRpc*>(ud);
  RW_ASSERT (rpc);
  RWMEMLOG (rpc->memlog_buf_, RWMEMLOG_MEM2, "rpc start xact async");

  std::string ns = protobuf_c_message_descriptor_xml_ns( rpc->rpc_input()->descriptor );
  if (ns == uagent_dts_yang_ns) {
    rpc->start_ns_mgmtagt_dts();
  } else if (ns == uagent_yang_ns) {
    rpc->start_ns_mgmtagt();
  } else if (ns == uagent_confd_yang_ns) {
    rpc->instance_->mgmt_handler()->handle_rpc(rpc, rpc->rpc_input_.get());
  } else {
    rpc->start_xact_dts();
  }
}

StartStatus SbReqRpc::start_xact_dts()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "rpc start dts xact");

  auto msgdesc = rw_schema_pbcm_get_msg_msgdesc(
      nullptr, rpc_input_.get(), instance_->ypbc_schema() );
  RW_ASSERT(msgdesc);

  auto ks = msgdesc->schema_path_value;
  if (!ks) {
    // ATTN: Need element name
    return done_with_error( "Cannot get keyspec for RPC" );
  }

  xact_ = rwdts_api_query_ks(dts_->api(),
                             ks,
                             RWDTS_QUERY_RPC,
                             dts_flags_,
                             dts_complete_cb,
                             this,
                             rpc_input_.get());

  RW_ASSERT(xact_);
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "starting dts xact",
      RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)this),
      RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIX64, (intptr_t)xact_));

  rwdts_xact_ref (xact_, __PRETTY_FUNCTION__, __LINE__); // Take a ref always.
  RW_MA_SBREQ_LOG (this, __FUNCTION__, "Starting Transaction");
  update_stats (RW_UAGENT_NETCONF_DTS_XACT_START);

  return StartStatus::Async;
}

void SbReqRpc::internal_done()
{
  done_with_success();
}

void SbReqRpc::internal_done(
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  done_with_success( ks, msg );
}

void SbReqRpc::internal_done(
  NetconfErrorList* nc_errors )
{
  done_with_error( nc_errors );
}

void SbReqRpc::dts_cb_async(void* ud)
{
  SbReqRpc *rpc =  static_cast<SbReqRpc*>(ud);
  RW_ASSERT (rpc);
  rpc->done_with_success( rpc->dts_xact() );
}

void SbReqRpc::dts_complete_cb(
  rwdts_xact_t* xact,
  rwdts_xact_status_t* xact_status,
  void* ud)
{
  auto *rpc =  static_cast<SbReqRpc*>(ud);
  RW_ASSERT(rpc);

  RWMEMLOG( rpc->memlog_buf_, RWMEMLOG_MEM2, "dts trx status",
       RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact),
       RWMEMLOG_ARG_PRINTF_INTPTR("status = %" PRIu64, xact_status->status));

  switch (xact_status->status) {
    case RWDTS_XACT_COMMITTED: {
      RWMEMLOG(rpc->memlog_buf_, RWMEMLOG_MEM2, "dts query complete");

      RW_ASSERT (xact == rpc->dts_xact());
      rwsched_dispatch_async_f(rpc->instance()->rwsched_tasklet(),
                               rpc->instance()->get_queue(QueueType::DefaultConcurrent),
                               rpc,
                               dts_cb_async);
      break;
    }

    default:
    case RWDTS_XACT_FAILURE:
    case RWDTS_XACT_ABORTED: {
      RW_MA_SBREQ_LOG (rpc, __FUNCTION__, "RWDTS_XACT_ABORTED");
      rw_status_t rs;
      char *err_str = NULL;
      char *xpath = NULL;

      if (RW_STATUS_SUCCESS ==
          rwdts_xact_get_error_heuristic(xact, 0, &xpath, &rs, &err_str)) {
        RWMEMLOG(rpc->memlog_buf_, RWMEMLOG_MEM6, "Error response",
                 RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)rpc),
                 RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact) );

        NetconfErrorList nc_errors;
        NetconfError& err = nc_errors.add_error();

        if (xpath) {
          err.set_error_path(xpath);
          RW_FREE(xpath);
          xpath = NULL;
        }
        err.set_rw_error_tag(rs);
        if (err_str) {
          err.set_error_message(err_str);
          RW_FREE(err_str);
          err_str = NULL;
        }
        rpc->internal_done(&nc_errors);
        return;
      }
      rpc->done_with_error( "Distributed transaction aborted or failed" );
    }
  }
}
