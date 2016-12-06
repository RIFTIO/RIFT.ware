
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
 * @file rwuagent_nb_req_confd_dp.cpp
 *
 * Management agent confd northbound handler for data
 */
#include <thread>
#include "rwuagent.hpp"

using namespace rw_uagent;
using namespace rw_yang;


NbReqConfdDataProvider::NbReqConfdDataProvider(
  ConfdDaemon *daemon,
  struct confd_trans_ctx *ctxt,
  rwsched_dispatch_queue_t serial_q,
  uint32_t cli_dom_refresh_period_ms,
  uint32_t nc_rest_dom_refresh_period_ms )
: NbReq(
    daemon->instance_,
    "NbReqConfdDP",
    RW_MGMTAGT_NB_REQ_TYPE_CONFDGET ),
  daemon_(daemon),
  current_dispatch_q_(serial_q),
  cli_dom_refresh_period_msec_(cli_dom_refresh_period_ms),
  nc_rest_dom_refresh_period_msec_(nc_rest_dom_refresh_period_ms)
{
  RW_ASSERT(ctxt->uinfo);
  if (strcmp (ctxt->uinfo->context, "cli") == 0) {
    type_ = CLI;
  } else if (strcmp(ctxt->uinfo->context, "netconf") == 0) {
    type_ = NETCONF;
  } else if (strcmp(ctxt->uinfo->context, "rest") == 0) {
    type_ = RESTCONF;
  } else if (strcmp(ctxt->uinfo->context, "system") == 0) {
    type_ = MAAPI_UPGRADE;
  } else {
    RW_ASSERT_NOT_REACHED();
  }

  if (type_ != MAAPI_UPGRADE) {
    start_dom_reclamation_timer();
  }
}

NbReqConfdDataProvider::~NbReqConfdDataProvider()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "NbReqConfdDataProvider destroy",
       RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)this) );

  if (nullptr != keypath_) {
    confd_free_hkeypath(keypath_);
  }
  if (type_ == MAAPI_UPGRADE) {
    return;
  }

  stop_dom_reclamation_timer();
}

void NbReqConfdDataProvider::log_state(
  RwMgmtagt_Confd_CallbackType type,
  const char *hkey_path,
  const char *dts_ks)
{
  if (type == RW_MGMTAGT_CONFD_CALLBACK_TYPE_CREATE) {
    // called during create of operational dom. Create a new entry
    RW_ASSERT(hkey_path);
    dom_stats_.emplace_front(new ConfdDomStats(hkey_path, dts_ks));
    return;
  }

  auto it = dom_stats_.begin();
  if (it != dom_stats_.end()) {
    (*it)->log_state(type);
  }
}

int NbReqConfdDataProvider::confd_get_next(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath,
  long next)
{
  RW_ASSERT(keypath);
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd_get_next",
    RWMEMLOG_ARG_PRINTF_INTPTR("next=%" PRIi64, next) );

  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);

  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  confd_value_t keys[RW_MAX_CONFD_KEYS];
  XMLNode *next_node = nullptr;
  int key_count = 0;
  rw_status_t status = RW_STATUS_FAILURE;

  if (!instance_->dts_ready()) {
    RWMEMLOG_TIME_CODE( (
        auto rv = confd_data_reply_next_key (ctxt_, NULL, -1, -1);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key no dts yet" );
    return rv;
  }

  RW_MA_NBREQ_LOG(this, ClientDebug, "Confd get_next for ", confd_hkeypath_buffer);

  if ((next == -1) && !is_dom_sufficient(keypath)) {
    state_ = GET_FIRST_KEY;
    RW_MA_NBREQ_LOG (this, ClientDebug, "Confd get_next requires fetch ",confd_hkeypath_buffer);
    return retrieve_operational_data(ctxt, keypath);
  }

  std::string prntstr;
  RW_MA_NBREQ_LOG (this, ClientDebug, (prntstr=op_dom_.debug_print()).c_str(), "");

  XMLNode *node = nullptr;
  auto op_node = op_dom_.node(dom_ptr_);
  RW_ASSERT(op_node);

  // check if we still have cached data
  if (next != -1) {
    // Check the current dom_ptr_
    if (!op_node.get().is_keypath_present(keypath))
    {
      //Check if any other dom in cache has seen this keypath
      auto tdom_ptr = op_dom_.dom_with_keypath(confd_hkeypath_buffer);

      // Check if the DOM got evicted.
      // ATTN: Currently we just stop
      // streaming output once we do not
      // find entry
      if (!tdom_ptr && !op_dom_.dom(keypath)) {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_key(ctxt_, NULL, -1, -1);
          ), memlog_buf_, RWMEMLOG_MEM2|RWMEMLOG_KEEP, "confd_data_reply_next_key evicted!" );
        RW_MA_NBREQ_LOG(this, ClientDebug, "Cached data evicted", confd_hkeypath_buffer);
        return CONFD_OK;
      }
      if (tdom_ptr) {
        dom_ptr_ = tdom_ptr;
        op_node = op_dom_.node(dom_ptr_);
        RW_ASSERT(op_node);
      }
    }
  }

  // The data can be retrieved from the DOM. This is either directly from the
  // node pointer in next, or if that is 0, by traversing the DOM using the
  // hkeypath in reverse.
  log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_NEXT, confd_hkeypath_buffer);
  struct confd_cs_node *cs_node = confd_find_cs_node(keypath, keypath->len);
  RW_ASSERT (cs_node);

  // Track the keypath
  op_node.get().add_keypath(keypath);

  if (next != -1) {
    // return the key from XML node identified by the key
    node = op_node.get().get_node(next);
  } else if (dom_ptr_) {
    // Find the node by hkeypath, and return its key
    node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath);
  }

  if (nullptr != node) {
    status = get_current_key_and_next_node(node, cs_node, &next_node, keys, &key_count);
  }

  if (RW_STATUS_SUCCESS == status) {
    if (nullptr != next_node) {
      RW_ASSERT(next_node->get_yang_node());
    }
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_next_key(ctxt_, keys, key_count,
                                  op_node.get().add_pointer(next_node));
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key" );

    RW_MA_NBREQ_LOG (this, ClientDebug, "Confd get_next found  ", confd_hkeypath_buffer);
    return CONFD_OK;
  }
  else {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_next_key (ctxt_, NULL, -1, -1);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key not found" );
    RW_MA_NBREQ_LOG (this, ClientDebug, "Confd get_next not found  ", confd_hkeypath_buffer );
    return CONFD_OK;
  }
  return CONFD_OK;
}

int NbReqConfdDataProvider::confd_get_object(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd_get_object");
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  if (!instance_->dts_ready()) {
    RWMEMLOG_TIME_CODE( (
        auto rv = confd_data_reply_not_found(ctxt_);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
    return rv;
  }

  XMLNode *node = nullptr;
  if (is_dom_sufficient(keypath)) {
    if (dom_ptr_) {
      node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath);
    }
  }

  if (nullptr != node) {
    RW_MA_NBREQ_LOG (this, ClientDebug, "confd_get_object found ", confd_hkeypath_buffer );
    send_node_to_confd(node, keypath);
    log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_OBJECT, confd_hkeypath_buffer);
    return CONFD_OK;
  }

  RW_MA_NBREQ_LOG(this, ClientDebug, "Retrieving data", confd_hkeypath_buffer );

  state_ = GET_OBJECT;
  return retrieve_operational_data(ctxt, keypath);
}

int NbReqConfdDataProvider::confd_get_next_object(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath,
  long next)
{
  RW_ASSERT(keypath);
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd_get_next_object",
    RWMEMLOG_ARG_PRINTF_INTPTR("next=%" PRIi64, next) );

  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES,confd_hkeypath_buffer) );

  if (!instance_->dts_ready()) {
    RWMEMLOG_TIME_CODE( (
        auto rv = confd_data_reply_next_object_tag_value_arrays (ctxt, nullptr, 0, 0);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_object_tag_value_arrays no dts" );
    return rv;
  }

  if ((next == -1) && !is_dom_sufficient(keypath)) {
    state_ = GET_FIRST_OBJECT;
    RW_MA_NBREQ_LOG (this, ClientDebug, "Confd get_next_object requires fetch ",confd_hkeypath_buffer);
    return retrieve_operational_data(ctxt, keypath);
  }

  auto op_node = op_dom_.node(dom_ptr_);
  RW_ASSERT(op_node);

  // check if we still have cached data
  if (next != -1) {
    // Check the current dom_ptr_
    if (!op_node.get().is_keypath_present(keypath))
    {
      //Check if any other dom in cache has seen this
      //keypath
      auto tdom_ptr = op_dom_.dom_with_keypath(confd_hkeypath_buffer);

      // Check if the DOM got evicted.
      // ATTN: Currently we just stop
      // streaming output once we do not
      // find entry
      if (!tdom_ptr && !op_dom_.dom(keypath)) {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_key(ctxt_, NULL, -1, -1);
          ), memlog_buf_, RWMEMLOG_MEM2|RWMEMLOG_KEEP, "confd_data_reply_next_key expired" );
        RW_MA_NBREQ_LOG(this, ClientDebug, "Cached data evicted", confd_hkeypath_buffer);
        return CONFD_OK;
      }
      if (tdom_ptr) {
        dom_ptr_ = tdom_ptr;
        op_node = op_dom_.node(dom_ptr_);
        RW_ASSERT(op_node);
      }
    }
  }

  log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_NEXT_OBJECT, confd_hkeypath_buffer);

  // Track the keypath
  op_node.get().add_keypath(keypath);

  XMLNode *node = nullptr;
  if (-1 != next) {
    if (op_node) {
      node = op_node.get().get_node(next);
    }
  } else if (dom_ptr_){
    node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath);
  }

  if (nullptr == node) {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_next_object_tag_value_arrays (ctxt, nullptr, 0, 0);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_object_tag_value_arrays no data" );
    return CONFD_OK;
  }

  return send_multiple_nodes_to_confd(node, keypath);
}

int NbReqConfdDataProvider::confd_get_case(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath,
  confd_value_t *choice)
{
  RW_ASSERT(keypath);
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd_get_case");

  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  if (!instance_->dts_ready()) {
    RW_ASSERT_NOT_REACHED();
  }

  log_state(RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_CASE, confd_hkeypath_buffer);
  RW_MA_NBREQ_LOG(this, ClientDebug, "Getting case", confd_hkeypath_buffer );

  auto op_node = op_dom_.node(dom_ptr_);

  if (op_node) {
    op_node.get().add_keypath(keypath);
  } else {
    //ATTN: Ideally we should not be here, but there is a possibility
    //if confd overlaps two requests
    std::string log;
    RW_MA_NBREQ_LOG (this, ClientNotice, "Cached node not found for ", confd_hkeypath_buffer);
    RW_MA_NBREQ_LOG (this, ClientDebug, (log=op_dom_.debug_print()).c_str(), "");
  }

  confd_value_t case_val = {};
  rw_status_t status = RW_STATUS_FAILURE;

  if (dom_ptr_ && dom_ptr_->get_root_node()) {
    status = get_confd_case(dom_ptr_->get_root_node(), keypath, choice, &case_val);
  } else {
    std::string log;
    RW_MA_NBREQ_LOG (this, ClientError, "Dom not found for ", confd_hkeypath_buffer);
    RW_MA_NBREQ_LOG (this, ClientError, (log=op_dom_.debug_print()).c_str(), "");
  }

  int rv;
  if (status == RW_STATUS_SUCCESS) {
    RWMEMLOG_TIME_CODE( (
        rv = confd_data_reply_value(ctxt, &case_val);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_value" );
  } else {
    RWMEMLOG_TIME_CODE( (
      rv = confd_data_reply_not_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found no case" );
  }
  return rv;
}


int NbReqConfdDataProvider::confd_get_elem(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "confd_get_elem");

  RW_ASSERT(keypath);
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  if (!instance_->dts_ready()) {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_not_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
    return CONFD_OK;
  }
  RW_MA_NBREQ_LOG (this, ClientDebug, "Getting element", confd_hkeypath_buffer);

  XMLNode *node = nullptr;
  if (dom_ptr_ && (DOM_READY == state_)) {
    node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath);
  }

  if (nullptr != node) {
    struct confd_cs_node *cs_node = confd_find_cs_node(keypath, keypath->len);
    RW_ASSERT (cs_node);

    auto op_node = op_dom_.node(dom_ptr_);

    if (op_node) {
      op_node.get().add_keypath(keypath);
    } else {
      //ATTN: Ideally we should not be here, but there is a possibility
      //if confd overlaps two requests
      std::string log;
      RW_MA_NBREQ_LOG (this, ClientNotice, "Cached node not found for ", confd_hkeypath_buffer);
      RW_MA_NBREQ_LOG (this, ClientDebug, (log=op_dom_.debug_print()).c_str(), "");
    }

    confd_value_t value;
    rw_status_t status = get_element (node, cs_node, &value);
    if (status == RW_STATUS_SUCCESS) {
      RWMEMLOG_TIME_CODE( (
          confd_data_reply_value (ctxt, &value);
        ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_value" );
      return CONFD_OK;
    }

    RWMEMLOG_TIME_CODE( (
        confd_data_reply_not_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
    log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_ELEM, confd_hkeypath_buffer);
    return CONFD_OK;
  }

  // Get Element is only called for a leaf - check if the parent exists.
  // If the parent exists, assume the leaf doesnt - for now.

  // ATTN: This should go away once DTS gets leaves
  if (dom_ptr_ && (DOM_READY == state_)) {
    node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath, 1);
  }

  if (nullptr != node) {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_not_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
    log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_ELEM, confd_hkeypath_buffer);
    return CONFD_OK;
  }

  state_ = GET_ELEMENT;
  // refresh the operational DOM.
  return retrieve_operational_data(ctxt, keypath);
}


int NbReqConfdDataProvider::confd_get_exists(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath)
{
  RWMEMLOG_TIME_SCOPE( memlog_buf_, RWMEMLOG_MEM2, "confd_get_exists" );

  RW_ASSERT(keypath);
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  if (!instance_->dts_ready()) {
    RWMEMLOG_TIME_CODE( (
        auto rv = confd_data_reply_not_found (ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found no dts yet" );
    return rv;
  }

  log_state(RW_MGMTAGT_CONFD_CALLBACK_TYPE_EXISTS_OPTIONAL, confd_hkeypath_buffer);
  RW_MA_NBREQ_LOG(this, ClientDebug, "Checking for existance", confd_hkeypath_buffer );

  XMLNode *node = nullptr;
  if (is_dom_sufficient(keypath, false)) {
    if (dom_ptr_) {
      node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath);
    }
  }

  if (nullptr != node) {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_found" );
  } else {
    RWMEMLOG_TIME_CODE( (
        confd_data_reply_not_found(ctxt);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
  }
  return CONFD_OK;
}

int NbReqConfdDataProvider::retrieve_operational_data(
  struct confd_trans_ctx *ctxt,
  confd_hkeypath_t *keypath)
{
  // ATTN: Is this pp_kpath redundant?
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath);

  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "retrieve_operational_data",
    RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );
  RW_MA_NBREQ_LOG (this, ClientDebug, "Retrieving operational data", confd_hkeypath_buffer );

  // free the resources that were allocated earlier.
  if (nullptr != keypath_) {
    confd_free_hkeypath(keypath_);
  }

  keypath_ = confd_hkeypath_dup(keypath);
  ctxt_ = ctxt;

  rw_yang::XMLDocument::uptr_t req =
      std::move(instance_->xml_mgr()->create_document(instance_->yang_model()->get_root_node()));
  XMLNode *root = req->get_root_node();
  XMLBuilder builder(root);

  ConfdKeypathTraverser traverser(&builder, keypath_);
  traverser.traverse();

  std::string capture_temporary;
  RW_MA_NBREQ_LOG (this, ClientDebug, "XML path ", (capture_temporary=root->to_string()).c_str());

  SbReqGet* get = new SbReqGet (instance_, this, RequestMode::CONFD, std::move(req));
  log_state( RW_MGMTAGT_CONFD_CALLBACK_TYPE_CREATE, confd_hkeypath_buffer, get->req().c_str() );

  auto ss = get->start_xact();
  if (StartStatus::Async == ss) {
    return CONFD_DELAYED_RESPONSE;
  }
  RW_ASSERT(StartStatus::Done == ss);
  return CONFD_OK;
}

StartStatus NbReqConfdDataProvider::respond(
  SbReq* sbreq,
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond have dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq) );

  RW_ASSERT(rsp_dom.get());

  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof (confd_hkeypath_buffer) - 1, keypath_);
  RWMEMLOG_BARE(memlog_buf_, RWMEMLOG_MEM2,
        RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, confd_hkeypath_buffer) );

  op_dom_.add_dom(std::move(rsp_dom), confd_hkeypath_buffer);

  dom_ptr_ = op_dom_.dom(confd_hkeypath_buffer);
  RW_ASSERT(dom_ptr_);

  auto op_node = op_dom_.node(dom_ptr_);
  RW_ASSERT(op_node);

  struct confd_cs_node *cs_node = confd_find_cs_node(keypath_, keypath_->len);
  RW_ASSERT (cs_node);

  XMLNode *node = nullptr;

  switch (state_) {
    case GET_FIRST_KEY: {
      log_state(RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_NEXT, confd_hkeypath_buffer, nullptr);

      // confd could be waiting for a reply?
      node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath_);

      if (!node) {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_key(ctxt_, NULL, -1, -1);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key not found" );
        RW_MA_NBREQ_LOG(this, ClientDebug, "No XML Found ", confd_hkeypath_buffer );
        break;
      }

      std::string capture_temporary;
      RW_MA_NBREQ_LOG(this, ClientDebug, "XML Found ", (capture_temporary=node->to_string()).c_str());
      confd_value_t keys[RW_MAX_CONFD_KEYS];
      XMLNode *next_node = nullptr;
      int key_count = 0;
      rw_status_t status = RW_STATUS_FAILURE;

      status = get_current_key_and_next_node(node, cs_node, &next_node, keys, &key_count);

      if (status == RW_STATUS_SUCCESS) {

        if (nullptr != next_node) {
          RW_ASSERT(next_node->get_yang_node());
        }

        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_key(ctxt_, keys, key_count,
                                      op_node.get().add_pointer(next_node));
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key" );

        for (size_t i = 0; i < (size_t)key_count; i++) {
          confd_free_value(&keys[i]);
        }
      } else {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_key(ctxt_, NULL, -1, -1);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_key not found" );
      }
      break;
    }

    case GET_ELEMENT: {
      confd_value_t value;
      log_state (RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_ELEM, confd_hkeypath_buffer,nullptr);
      node = get_node_by_hkeypath (dom_ptr_->get_root_node(), keypath_);
      if (!node) {
        RW_MA_NBREQ_LOG (this, ClientDebug, "No XML Found ", confd_hkeypath_buffer );
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_not_found(ctxt_);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found not found" );
        break;
      }

      std::string capture_temporary;
      RW_MA_NBREQ_LOG (this, ClientDebug, "XML Found ", (capture_temporary=node->to_string()).c_str());
      rw_status_t status =
          get_element ( node, cs_node, &value);
      if (status == RW_STATUS_SUCCESS) {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_value (ctxt_, &value);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_value" );
      } else {
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_not_found(ctxt_);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found not found" );
      }
      break;
    }

    case GET_OBJECT: {
      log_state(RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_OBJECT, confd_hkeypath_buffer, nullptr);
      node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath_);

      if (!node) {
        RW_MA_NBREQ_LOG(this, ClientDebug, "No XML Found ", confd_hkeypath_buffer );
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_not_found(ctxt_);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found not found" );
      } else {
        std::string capture_temporary;
        RW_MA_NBREQ_LOG(this, ClientDebug, "XML Found ", (capture_temporary=node->to_string()).c_str());
        send_node_to_confd(node, keypath_);
      }
      break;
    }

    case GET_FIRST_OBJECT: {
      log_state(RW_MGMTAGT_CONFD_CALLBACK_TYPE_GET_NEXT_OBJECT, confd_hkeypath_buffer, nullptr);
      node = get_node_by_hkeypath(dom_ptr_->get_root_node(), keypath_);

      if (nullptr == node) {
        RW_MA_NBREQ_LOG(this, ClientDebug, "No XML Found ", confd_hkeypath_buffer );
        RWMEMLOG_TIME_CODE( (
            confd_data_reply_next_object_tag_value_arrays(ctxt_, nullptr, 0, 0);
          ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_object_tag_value_arrays" );
      }else {
        std::string capture_temporary;
        RW_MA_NBREQ_LOG(this, ClientDebug, "XML Found ", (capture_temporary=node->to_string()).c_str());
        send_multiple_nodes_to_confd(node, keypath_);
      }
      break;
    }

    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (node != nullptr) {
    state_ = DOM_READY;
  } else {
    if (nullptr != keypath_) {
      confd_free_hkeypath(keypath_);
      keypath_ = nullptr;
    }
  }
  return StartStatus::Done;
}

StartStatus NbReqConfdDataProvider::respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "respond with error",
              RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)sbreq),
              RWMEMLOG_ARG_PRINTF_INTPTR("nc_errors=0x%" PRIX64,(intptr_t)nc_errors) );

  RW_ASSERT(nc_errors); 

  // Send the first error in the list
  for(const NetconfError& err : nc_errors->get_errors()) {
    std::string err_str = "";
    if (err.get_error_path()) {
      err_str += err.get_error_path();
      err_str += " : ";
    }
    if (err.get_error_message()) {
      err_str += err.get_error_message();
    }
    if (err_str.size()) {
      confd_trans_seterr_extended(ctxt_,
                                  CONFD_ERRCODE_APPLICATION, // confd_errcode
                                  0, // apptag_ns
                                  0, // apptag_tag
                                  "%s", err_str.c_str());
    }
    else {
      confd_trans_seterr_extended(ctxt_,
                                  CONFD_ERRCODE_APPLICATION, // confd_errcode
                                  0, // apptag_ns
                                  0, // apptag_tag
                                  "%s", "");
    }
    confd_delayed_reply_error(ctxt_, NULL);
    break;
  }

  return StartStatus::Done;
}

bool NbReqConfdDataProvider::is_dom_sufficient(
  confd_hkeypath_t* &keypath,
  bool do_cleanup)
{
  if ((nullptr == keypath_) || (state_ != DOM_READY)) {
    return false;
  }

  rw_confd_hkeypath_cmp_t cmp = hkeypath_cmp(keypath_, keypath);

  switch (type_) {
    case CLI: {
      switch (cmp) {
        case HKEYPATH_IS_PARENT:
        case HKEYPATH_EQUAL: {
          do_dom_cleanup(do_cleanup);
          if (op_dom_.dom(keypath_)) return true;
          // Flushed away by timer
          return false;
        }
        case HKEYPATH_IS_CHILD:
        case HKEYPATH_DIVERGES: {
          do_dom_cleanup(do_cleanup);
          auto *doc = op_dom_.dom(keypath);
          if (doc) {
            dom_ptr_ = doc;
            confd_free_hkeypath(keypath_);
            keypath_ = confd_hkeypath_dup(keypath);
            return true;
          }
          return false;
        }
      };
      RW_ASSERT_NOT_REACHED();
    }
    case NETCONF:
    case RESTCONF: {
      switch (cmp) {
        case HKEYPATH_IS_PARENT:
          return true;
        case HKEYPATH_EQUAL:
        case HKEYPATH_IS_CHILD:
        case HKEYPATH_DIVERGES: {
          do_dom_cleanup(do_cleanup);
          auto *doc = op_dom_.dom(keypath);
          if (doc) {
            dom_ptr_ = doc;
            return true;
          }
          return false;
        }
      };
      RW_ASSERT_NOT_REACHED();
    }
    case MAAPI_UPGRADE:
      return true;
    default:
      break;
  };
  RW_ASSERT_NOT_REACHED();
}

rw_confd_hkeypath_cmp_t NbReqConfdDataProvider::hkeypath_cmp(
  const confd_hkeypath_t *hk1,
  const confd_hkeypath_t *hk2)
{
  const confd_hkeypath_t *small = nullptr, *large = nullptr;
  if (hk1->len < hk2->len) {
    small = hk1;
    large = hk2;
  } else {
    small = hk2;
    large = hk1;
  }

  for (int i = small->len, j = large->len; i >= 0; i--, j--) {
    for (int k = 0; small->v[i][k].type != C_NOEXISTS &&  large->v[i][k].type != C_NOEXISTS; k++) {
      if (!confd_val_eq (&small->v[i][k], &large->v[j][k])) {
        return HKEYPATH_DIVERGES;
      }
    }
  }

  if (hk1->len < hk2->len) {
    return HKEYPATH_IS_PARENT;
  } else if (hk1->len == hk2->len) {
    return HKEYPATH_EQUAL;
  } else {
    return HKEYPATH_IS_CHILD;
  }
}

int NbReqConfdDataProvider::send_multiple_nodes_to_confd(
  XMLNode *node,
  confd_hkeypath_t *keypath)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "send_multiple_nodes_to_confd");

  struct confd_cs_node *cs_node = confd_find_cs_node(keypath, keypath->len);

  // Sent around RWUAGENT_CONFD_MULTI_GET_MAX_TLV tlv elements but not more
  // than RWUAGENT_CONFD_MULTI_GET_MAX_NODES nodes

  size_t n_nodes = 0, n_tlv = 0;
  struct confd_tag_next_object objs[RWUAGENT_CONFD_MULTI_GET_MAX_NODES];
  memset (&objs, 0, sizeof (objs));

  auto op_node = op_dom_.node(dom_ptr_);
  RW_ASSERT(op_node);

  while ( (nullptr != node)  && (n_nodes < RWUAGENT_CONFD_MULTI_GET_MAX_NODES) &&
          (n_tlv < RWUAGENT_CONFD_MULTI_GET_MAX_TLV) && op_node) {

    struct confd_tag_next_object *obj_ptr = &objs[n_nodes];
    obj_ptr->n = convert_node_to_confd (node, cs_node, &obj_ptr->tv, true);
    RW_ASSERT(obj_ptr->n);

    node = node->get_next_sibling(node->get_local_name(), node->get_name_space());
    obj_ptr->next = op_node.get().add_pointer(node);

    n_tlv += obj_ptr->n;
    n_nodes++;
  }

  if (node) {
    node_ = node;
  } else {
    node_ = nullptr;
  }

  RWMEMLOG_TIME_CODE( (
      int ret = confd_data_reply_next_object_tag_value_arrays (ctxt_, objs, n_nodes, 0);
    ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_next_object_tag_value_arrays" );

  // Free allocated data
  for (size_t i = 0; i < n_nodes; i++) {
    struct confd_tag_next_object *obj_ptr = &objs[i];
    for (size_t j = 0; j < (size_t) obj_ptr->n; j++) {
      confd_free_value(CONFD_GET_TAG_VALUE(&obj_ptr->tv[j]));
    }
    RW_FREE (obj_ptr->tv);
    obj_ptr->tv = nullptr;
  }

  return ret;
}

int NbReqConfdDataProvider::send_node_to_confd(
  XMLNode *node,
  confd_hkeypath_t *keypath)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "send_node_to_confd");

  struct confd_cs_node *cs_node = confd_find_cs_node(keypath, keypath->len);
  RW_ASSERT (cs_node);

  std::string capture_temporary;
  RW_MA_NBREQ_LOG (this, ClientDebug, "Sending node to confd ", (capture_temporary=node->to_string()).c_str());

  confd_tag_value_t *array = nullptr;
  size_t length = 0;

  length = convert_node_to_confd (node, cs_node, &array, false);

  if (length) {
    RWMEMLOG_TIME_CODE( (
        int ret = confd_data_reply_tag_value_array(ctxt_, array, length);
      ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_tag_value_array" );

    for (size_t i = 0; i < length; i++) {
      confd_free_value(CONFD_GET_TAG_VALUE(&array[i]));
    }
    if (nullptr != array) {
      RW_FREE (array);
      array = nullptr;
    }

    if (CONFD_OK != ret) {
      RW_MA_NBREQ_LOG (this, ClientError, "Sending node to confd failed", (capture_temporary=node->to_string()).c_str());
    }
    return CONFD_OK;
  }

  RWMEMLOG_TIME_CODE( (
      confd_data_reply_not_found(ctxt_);
    ), memlog_buf_, RWMEMLOG_MEM2, "confd_data_reply_not_found" );
  return CONFD_OK;
}

/*
 * Build a confd TLV array corresponding to a XML Node.
 *
 * @return length - a zero indicates failure
 *
 * @param [in] node XML node to convert to confd
 * @param [in] cs_node The confd schema node associated with this XML node
 * @param [in out] array Allocated and filled in by this function. Memory owned
 * by caller
 */
int NbReqConfdDataProvider::convert_node_to_confd(
  XMLNode *node,
  struct confd_cs_node *cs_node,
  confd_tag_value_t **array,
  bool multiple)
{
  RW_ASSERT(node);
  RW_ASSERT(cs_node);
  RW_ASSERT(array);

  ConfdTLVBuilder builder(cs_node);
  XMLTraverser traverser(&builder, node);

  traverser.traverse();

  size_t length = builder.length();

  if (!length) {
    return length;
  }

  YangNode *yn = node->get_yang_node();
  bool add_bogus_key = false;
  add_bogus_key =  multiple && yn->is_listy() && !yn->has_keys();

  if (add_bogus_key) {
    length += 1;
  }

  *array =
      (confd_tag_value_t *) RW_MALLOC (sizeof (confd_tag_value_t) *  length);

  if (add_bogus_key) {
    builder.copy_and_destroy_tag_array(&((*array)[1]));
    (*array)[0].tag.tag = 0;
    (*array)[0].tag.ns = (*array)[1].tag.ns;
    CONFD_SET_INT64 (&((*array)[0].v), (int64_t) node);
  }
  else {
    builder.copy_and_destroy_tag_array(*array);
  }

  return length;
}

void NbReqConfdDataProvider::start_dom_reclamation_timer()
{
  double timer_interval = 0;

  switch (type_) {
  case CLI:
    timer_interval = cli_dom_refresh_period_msec_ / 1000.0;
    break;
  case NETCONF:
  case RESTCONF:
    timer_interval = nc_rest_dom_refresh_period_msec_ / 1000.0;
    break;
  default:
    RW_ASSERT_NOT_REACHED();
  };

  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();

  RW_MA_NBREQ_LOG(this, ClientDebug, "Starting dom reclaim timer", "");

  dom_timer_ = rwsched_dispatch_source_create(tasklet,
                                              RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                              0,
                                              0,
                                              rwsched_dispatch_get_main_queue(instance_->rwsched()));

  rwsched_dispatch_source_set_event_handler_f(tasklet,
                                              dom_timer_,
                                              cleanup_expired_dom_cb);
  rwsched_dispatch_set_context(tasklet,
                               dom_timer_,
                               this);

  rwsched_dispatch_source_set_timer(tasklet,
                                    dom_timer_,
                                    dispatch_time(DISPATCH_TIME_NOW, timer_interval*NSEC_PER_SEC),
                                    0,
                                    0);

  rwsched_dispatch_resume(tasklet, dom_timer_);
}

void NbReqConfdDataProvider::cleanup_expired_dom_cb(
  void *user_context)
{
  auto *provider = static_cast<NbReqConfdDataProvider*>(user_context);
  provider->cleanup_indicator_ = true;
  provider->stop_dom_reclamation_timer();
}

void NbReqConfdDataProvider::stop_dom_reclamation_timer()
{
  rwsched_tasklet_ptr_t tasklet = instance_->rwsched_tasklet();

  RW_MA_NBREQ_LOG(this, ClientDebug, "Stopping dom reclaim timer", "");

  if (dom_timer_) {
    rwsched_dispatch_source_cancel(tasklet, dom_timer_);
    rwsched_dispatch_release(tasklet, dom_timer_);
  }
  dom_timer_ = NULL;
}

