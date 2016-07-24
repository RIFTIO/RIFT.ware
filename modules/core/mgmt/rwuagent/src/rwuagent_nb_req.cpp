
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_nb_req.cpp
 *
 * Management agent northbound request base.
 */

#include "rwuagent.hpp"
#include "rwuagent_request_mode.hpp"

using namespace rw_uagent;
using namespace rw_yang;

NbReq::NbReq(
  Instance* instance,
  const char* type,
  RwMgmtagt_NbReqType nbreq_type )
: instance_(instance),
  memlog_buf_(
    instance->get_memlog_inst(),
    type,
    reinterpret_cast<intptr_t>(this)),
  nbreq_type_(nbreq_type)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "northbound request created",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)this) );
  RW_ASSERT(instance_);

  if (instance_->mgmt_handler()->is_ready_for_nb_clients()) {
    instance_->log_file_manager()->log_string(this, type, "");
  }
}

NbReq::~NbReq()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "northbound request destroyed",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)this) );
}

rwsched_dispatch_queue_t NbReq::get_execution_q() const
{
  // Return main queue if client does not have its own
  // preference
  return rwsched_dispatch_get_main_queue(instance_->rwsched());
}

StartStatus NbReq::respond(
  SbReq* sbreq )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond generic empty dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  // ATTN: Create empty dom and return it
  auto rsp_dom = instance_->xml_mgr()->create_document(
      instance_->yang_model()->get_root_node() );
  return respond( sbreq, std::move(rsp_dom) );
}

StartStatus NbReq::respond(
  SbReq* sbreq,
  rwdts_xact_t* xact )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond transaction",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact) );

  /*
   * ATTN: This needs to be refactored once DTS moves to ref-counted
   * results.  This code would like to take the first result and then
   * see if there are more results.  If only one result, we should
   * prefer to make the ks+msg callback.  If more than one result, the
   * DOM callback will have to do (for now).  However, there is no DTS
   * API to ask how many results there are, and this code cannot keep
   * the ks+msg after making another call to rwdts_xact_query_result().
   * So, this code now makes a dup of the first response before trying
   * to build a DOM out of multiple responses.
   * ATTN: So, to sum up, this code is dumb.  Let's fix it someday.
   */

  rwdts_xact_status_t status;
  auto rs = rwdts_xact_get_status (xact, &status);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  switch (status.status) {
    case RWDTS_XACT_COMMITTED: {
      XMLDocument::uptr_t rsp_dom;
      UniquePtrProtobufCMessage<>::uptr_t rsp_msg;
      UniquePtrKeySpecPath::uptr_t rsp_ks;

      bool resp = false; // Checck if we got any result
      bool first = true;
      size_t count = 0;
      while (rwdts_query_result_t * result = rwdts_xact_query_result(xact, 0)) {
        RWMEMLOG(memlog_buf_, RWMEMLOG_MEM6, "merge one result",
                 RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
                 RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact) );

        resp = true;
        count++;
        if (first) {
          rw_keyspec_path_t* dup_ks = nullptr;
          auto rs = rw_keyspec_path_create_dup( result->keyspec, nullptr, &dup_ks );
          RW_ASSERT(RW_STATUS_SUCCESS == rs);
          rsp_ks.reset( dup_ks );

          rsp_msg.reset( protobuf_c_message_duplicate(
              nullptr, result->message, result->message->descriptor ) );
          first = false;

        } else {
          if (!rsp_dom.get()) {
            rsp_dom = std::move( instance_->xml_mgr()->create_document(
                instance_->yang_model()->get_root_node()) );

            auto ncs = rsp_dom->get_root_node()->merge( rsp_ks.get(), rsp_msg.get() );
            if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
              return respond(sbreq, "Merge operation failed");
            }
          }

          auto ncs = rsp_dom->get_root_node()->merge( result->keyspec, result->message );
          if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
            return respond(sbreq, "Merge operation failed");
          }
        }
      }
      sbreq->update_stats(RW_UAGENT_NETCONF_RESPONSE_BUILT);

      if (resp) {
        if (first) { // not possible?
          return respond( sbreq );
        }
        if (rsp_dom.get()) {
          return respond( sbreq, std::move(rsp_dom) );
        }
        return respond( sbreq, rsp_ks.get(), rsp_msg.get() );
      }
      break;
    }

    default:
    case RWDTS_XACT_FAILURE:
    case RWDTS_XACT_ABORTED: {
      rw_status_t rs;
      char *err_str = NULL;
      char *xpath = NULL;
      sbreq->update_stats(RW_UAGENT_NETCONF_RESPONSE_BUILT);
      if (RW_STATUS_SUCCESS ==
          rwdts_xact_get_error_heuristic(xact, 0, &xpath, &rs, &err_str)) {
        RWMEMLOG(memlog_buf_, RWMEMLOG_MEM6, "Error response",
                 RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
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
        return respond( sbreq, &nc_errors );
      }
      return respond(sbreq, "Transaction not successful, but no errors provided");
    }
  }
  return respond( sbreq );
}

StartStatus NbReq::respond(
  SbReq* sbreq,
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond generic keyspec and message to dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  rw_yang_netconf_op_status_t ncs = RW_YANG_NETCONF_OP_STATUS_FAILED;
  auto rsp_dom = std::move( instance_->xml_mgr()->create_document_from_pbcm(
      /*ATTN: stupid cast!*/const_cast<ProtobufCMessage*>(msg),
      ncs, true/*rooted*/, ks ) );
  sbreq->update_stats(RW_UAGENT_NETCONF_RESPONSE_BUILT);

  if (RW_YANG_NETCONF_OP_STATUS_OK != ncs) {
    return respond( sbreq, "Protobuf conversion failed" );
  }
  return respond( sbreq, std::move(rsp_dom) );
}

StartStatus NbReq::respond(
  SbReq* sbreq,
  const std::string& error_str )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond generic error string",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  NetconfErrorList nc_errors;
  nc_errors.add_error().set_error_message( error_str.c_str() );
  return respond( sbreq, &nc_errors );
}

void NbReq::commit_changes()
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd subscription");
  RW_MA_NBREQ_LOG (this, ClientDebug, __FUNCTION__, "Commiting changes");

  RW_ASSERT (nbreq_type_ == RW_MGMTAGT_NB_REQ_TYPE_CONFDCONFIG
          || nbreq_type_ == RW_MGMTAGT_NB_REQ_TYPE_INTERNAL
          || nbreq_type_ == RW_MGMTAGT_NB_REQ_TYPE_RWMSG);

  XMLNode *root = instance_->dom()->get_root_node();

  for (auto& ks : sb_delete_ks_) {
    std::list<XMLNode *>found;
    rw_yang_netconf_op_status_t ncrs = root->find(ks.get(), found);

    if (ncrs != RW_YANG_NETCONF_OP_STATUS_OK ||
        !found.size()) {
      RW_MA_NBREQ_LOG (this, ClientError, __FUNCTION__, "Error - config entry not found in DOM");
      break;
    }
    for (auto node : found) {
      XMLNode* parent = node->get_parent();
      RW_ASSERT (parent);
      parent->remove_child(node);
    }
  }

  // Merge delta with configuration DOM
  instance_->dom()->merge(sb_delta_.get());
}
