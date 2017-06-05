/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_nb_req_dts.cpp
 *
 * Management agent northbound request handler for RW.DTS
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NbReqDts::NbReqDts(
  Instance* instance,
  const rwdts_xact_info_t* xact_info,
  RWDtsQueryAction action,
  const rw_keyspec_path_t* ks_in,
  const ProtobufCMessage* msg )
: NbReq(
    instance,
    "NbReqDts",
    RW_MGMTAGT_NB_REQ_TYPE_RWDTS ),
  xact_info_(xact_info),
  action_(action),
  ks_in_(ks_in),
  msg_(msg)
{
  // ATTN: turn ks_in into a string and trace it?

  // Take a ref on the dts xact
  rwdts_xact_ref (xact_info_->xact, __PRETTY_FUNCTION__, __LINE__);
}

NbReqDts::~NbReqDts()
{
  if (xact_info_->xact) {
    rwdts_xact_unref (xact_info_->xact, __PRETTY_FUNCTION__, __LINE__);
  }
}

StartStatus NbReqDts::execute()
{
  if (!instance_->mgmt_handler()->is_ready_for_nb_clients()) {
    RW_MA_NBREQ_LOG (this, ClientNotice, "Agent not yet ready to accept request", "");
    return send_error( RW_YANG_NETCONF_OP_STATUS_RESOURCE_DENIED );
  }

  auto xml_mgr = instance_->xml_mgr();
  RW_ASSERT(xml_mgr);

  rw_yang_netconf_op_status_t ncs = RW_YANG_NETCONF_OP_STATUS_FAILED;
  auto xml_rpc(
    instance_->xml_mgr()->create_document_from_pbcm(
      /*ATTN:stupid-cast, shouldn't be needed*/const_cast<ProtobufCMessage*>(msg_), ncs ) );
  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    return send_error( ncs );
  }

  std::string capture_temporary;
  RW_MA_NBREQ_LOG( this, ClientDebug, "RPC Operation from DTS",
    (capture_temporary=xml_rpc->to_string()).c_str() );

  SbReqRpc* rpc_xact = new SbReqRpc( instance_, this, RequestMode::CONFD, xml_rpc.get() );

  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "New sb rpc trx",
      RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)rpc_xact));
  RWMEMLOG_BARE (memlog_buf_, RWMEMLOG_MEM2,
      RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, 
        (capture_temporary=xml_rpc->to_string()).c_str()));

  return rpc_xact->start_xact();
}

StartStatus NbReqDts::respond(
  SbReq* sbreq,
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "respond with keyspec and message",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq));

  auto rs = rwdts_xact_info_respond_keyspec(
    xact_info_,
    RWDTS_XACT_RSP_CODE_ACK,
    ks,
    msg );

  if (RW_STATUS_SUCCESS != rs) {
    RW_MA_NBREQ_LOG( this, ClientError, "DTS RPC failed, unable to respond", "" );
  }

  delete this;
  return StartStatus::Done;
}

StartStatus NbReqDts::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "respond with dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq));
  std::string capture_temporary;

  // ATTN: Just hand the PB back...  Oh, don't have the PB anymore.
  // Bummer.  Need to convert DOM back into PB.  This is dumb - it
  // was already in PB format just a moment ago...
  auto root = rsp_dom->get_root_node();
  RW_ASSERT(root);

  auto node = root->get_first_element();
  if (!node) {
    RW_MA_NBREQ_LOG( this, ClientDebug, "DTS RPC failed, no data", "");
    return send_error( RW_YANG_NETCONF_OP_STATUS_FAILED );
  }

  // Convert to PB
  ProtobufCMessage* msg = nullptr;
  auto ncs = node->to_pbcm( &msg );
  UniquePtrProtobufCMessageUseBody<>::uptr_t msg_uptr( msg );

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    RW_MA_NBREQ_LOG( this, ClientError, "DTS RPC failed, cannot convert XML",
      (capture_temporary=node->to_string()).c_str() );
    return send_error( ncs );
  }

  const rw_yang_pb_msgdesc_t* msgdesc = rw_schema_pbcm_get_msg_msgdesc(nullptr, msg, nullptr);

  return respond(
    sbreq,
    msgdesc->schema_path_value,
    msg );
}

StartStatus NbReqDts::respond(
  SbReq* sbreq,
  NetconfErrorList* nc_errors )
{

  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with error",
            RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
            RWMEMLOG_ARG_PRINTF_INTPTR("nc_errors=0x%" PRIX64,(intptr_t)nc_errors) );

  RW_ASSERT(nc_errors);

  for (auto it = nc_errors->get_errors().begin();
       it != nc_errors->get_errors().end(); ++it) {
    rw_status_t rs;
    if (it->get_error_path()){
      rs = rwdts_xact_info_send_error_xpath(
          xact_info_,
          rw_yang_netconf_op_status_to_rw_status(it->get_rw_nc_error_tag()),
          it->get_error_path(),
          it->get_error_message());
    }
    else {
      rs = rwdts_xact_info_send_error_keyspec(
          xact_info_,
          rw_yang_netconf_op_status_to_rw_status(it->get_rw_nc_error_tag()),
          &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_output_MgmtAgent)->rw_keyspec_path_t,
          it->get_error_message());
    }
    if (RW_STATUS_SUCCESS != rs) {
      RW_MA_NBREQ_LOG( this, ClientError,
                       "DTS RPC failed, unable to respond with error",
                       it->get_error_message() );
      break;
    }
  }
  delete this;
  return StartStatus::Done;

}

StartStatus NbReqDts::send_error(
    rw_yang_netconf_op_status_t ncs )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "send error",
    RWMEMLOG_ARG_PRINTF_INTPTR("ncs=%" PRIXPTR, (intptr_t)ncs) );

  // ATTN: Should have some kind of message passed into here.
  std::string error = "operation failed";

  auto rs = rwdts_xact_info_send_error_keyspec(
    xact_info_,
    rw_yang_netconf_op_status_to_rw_status( ncs ),
    &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_output_MgmtAgent)->rw_keyspec_path_t,
    error.c_str() );

  if (RW_STATUS_SUCCESS != rs) {
    RW_MA_NBREQ_LOG( this, ClientError, "DTS RPC failed, unable to respond with error", 
        error.c_str() );
  }

  delete this;
  return StartStatus::Done;
}

