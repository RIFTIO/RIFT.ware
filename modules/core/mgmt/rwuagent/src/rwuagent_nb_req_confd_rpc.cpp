
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_nb_req_confd_rpc.cpp
 *
 * Management agent confd northbound handler for rpc
 */

#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NbReqConfdRpcExec::NbReqConfdRpcExec(ConfdDaemon *daemon, confd_user_info *ctxt)
: NbReq(
    daemon->instance_,
    "NbReqConfdRpc",
    RW_MGMTAGT_NB_REQ_TYPE_CONFDACTION ),
  daemon_ (daemon),
  ctxt_ (ctxt)
{
}

NbReqConfdRpcExec::~NbReqConfdRpcExec()
{
}

int NbReqConfdRpcExec::execute (struct xml_tag *name, confd_hkeypath_t *kp,
                                 confd_tag_value_t *params, int nparams)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "NbReqConfdRpcExec::execute",);
  RW_ASSERT (kp == 0);

  if (nullptr == params) {
    RW_MA_NBREQ_LOG (this, ClientDebug, "Received NULL parameter rpc request", "");
    return confd_action_reply_values (ctxt_, nullptr, 0);
  }

  struct confd_cs_node *cs_node = confd_find_cs_root(CONFD_GET_TAG_NS(&params[0]));

  //All the RPC are root nodes - traverse all of them and try to find the named
  // one

  while (cs_node) {
    if ((cs_node->tag == name->tag) &&
        (cs_node->ns == name->ns)) {
      rpc_root_ = cs_node;
      break;
    }
    cs_node = cs_node->next;
  }

  rw_yang::XMLDocument::uptr_t cmd =
      std::move(instance_->xml_mgr()->create_document(instance_->yang_model()->get_root_node()));
  XMLNode *root = cmd->get_root_node();


  char *tag = confd_hash2str (name->tag);
  char *ns = confd_hash2str (name->ns);

  YangNode *yn = root->get_yang_node()->search_child (tag, ns);
  if (!yn) {
    std::string lstr = "Search failed: " + std::string(tag)
        + " : " + std::string(ns);
    RW_MA_NBREQ_LOG (this, ClientError, lstr.c_str(), "");
    return CONFD_OK;
  }

  XMLNode *rpc = root->add_child(yn);

  XMLBuilder builder(rpc);

  ConfdTLVTraverser traverser(&builder, params, nparams);
  traverser.set_confd_schema_root(rpc_root_);
  traverser.traverse();

  std::string capture_temporary;
  RW_MA_NBREQ_LOG (this, ClientDebug, "RPC Operation from confd", (capture_temporary=rpc->to_string()).c_str());

  SbReqRpc *xact = new SbReqRpc(instance_, this, RequestMode::CONFD, cmd.get());
  auto ss = xact->start_xact();
  if (StartStatus::Async == ss) {
    return CONFD_DELAYED_RESPONSE;
  }
  RW_ASSERT(StartStatus::Done == ss);
  return CONFD_OK;
}

StartStatus NbReqConfdRpcExec::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  XMLNode *reply = rsp_dom->get_root_node()->get_first_element();
  size_t length = 0;
  confd_tag_value_t *array = nullptr;

  if (nullptr != reply) {
    std::string capture_temporary;
    RW_MA_NBREQ_LOG (this, ClientDebug, "Rpc response", (capture_temporary=reply->to_string()).c_str());
    ConfdTLVBuilder builder(rpc_root_, true);
    XMLTraverser traverser(&builder, reply);

    traverser.traverse();

    length = builder.length();
    if (length) {
      array =
          (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) * length);

      builder.copy_and_destroy_tag_array(array);
    }
  }
  if (length) {
    RWMEMLOG_TIME_CODE( (
        confd_action_reply_values(ctxt_, array, length);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_action_reply_values" );
    for (size_t i = 0; i < length; i++) {
      confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
    }

  } else {
    RW_MA_NBREQ_LOG (this, ClientDebug, "Rpc response Without data", "");

    if (sbreq && sbreq->was_async()) {
      RWMEMLOG_TIME_CODE( (
          confd_action_delayed_reply_ok(ctxt_);
        ), memlog_buf_, RWMEMLOG_MEM2, "confd_action_delayed_reply_ok" );
    } else {
      RWMEMLOG_TIME_CODE( (
          confd_action_reply_values (ctxt_, nullptr, 0);
        ), memlog_buf_, RWMEMLOG_MEM2, "confd_action_reply_values empty" );
    }
  }

  ctxt_->u_opaque = 0;

  // Done with this object..
  delete this;
  return StartStatus::Done;
}

StartStatus NbReqConfdRpcExec::respond(
  SbReq* sbreq,
  NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with error",
            RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
            RWMEMLOG_ARG_PRINTF_INTPTR("nc_errors=0x%" PRIX64,(intptr_t)nc_errors) );

  RW_ASSERT(nc_errors);

  // Send the first error in the list as response
  for(auto it = nc_errors->get_errors().begin();
      it != nc_errors->get_errors().end(); ++it) {
    std::string err_str = "";
    if (it->get_error_path()) {
      err_str += it->get_error_path();
      err_str += " : ";
    }
    if (it->get_error_message()) {
      err_str += it->get_error_message();
    }
    if (err_str.size()) {
      confd_action_delayed_reply_error(ctxt_, err_str.c_str());
    }

    else {
      confd_action_delayed_reply_error(ctxt_, NULL);
    }
    break;
  }

  // Done with this object
  delete this;
  return StartStatus::Done;
}

int NbReqConfdRpcExec::set_timeout(int timeout_secs)
{
  return confd_action_set_timeout(ctxt_, timeout_secs);
}
