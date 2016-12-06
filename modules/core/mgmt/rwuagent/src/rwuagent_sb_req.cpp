
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
 * @file rwuagent_sb_req.cpp
 *
 * Management agent southbound request support
 */

#include "rwuagent.hpp"
#include "rwuagent_request_mode.hpp"

using namespace rw_uagent;
using namespace rw_yang;


SbReq::SbReq(
  Instance* instance,
  NbReq* nbreq,
  RwMgmtagt_SbReqType sbreq_type,
  RequestMode request_mode,
  const char* trace_name,
  const char* req)
: sbreq_type_(sbreq_type),
  instance_(instance),
  memlog_buf_(
    instance->get_memlog_inst(),
    trace_name,
    reinterpret_cast<intptr_t>(this)),
  nbreq_(nbreq),
  dts_(instance_->dts()),
  dts_flags_( dts_ ? dts_->get_flags() : RWDTS_XACT_FLAG_NONE ),
  req_(req),
  trace_name_(trace_name),
  request_mode_(request_mode)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "southbound request created",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64,(intptr_t)this),
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_) );

  RWMEMLOG_BARE (memlog_buf_, RWMEMLOG_MEM6,
       RWMEMLOG_ARG_STRNCPY(RWMEMLOG_ARG_SIZE_MAX_BYTES, req));

  if (req_ != ""
      && instance_->mgmt_handler()->is_ready_for_nb_clients()) {
    instance_->log_file_manager()->log_string(this, trace_name_, req_);
  }

  gettimeofday (&last_stat_time_, nullptr);
  RWPB_F_MSG_INIT(RwMgmtagt_SpecificStatistics_ProcessingTimes, &statistics_);
}

SbReq::~SbReq()
{
  RW_ASSERT (responded_);
  // ATTN:- What is the use of this?
  if (xact_) {
    rwsched_instance_ptr_t sched = instance_->rwsched();
    rwsched_dispatch_async_f (instance_->rwsched_tasklet(),
                              rwsched_dispatch_get_main_queue(sched),
                              xact_, SbReq::rwdts_xact_end_wrapper);
  }
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "southbound request destroyed",
    RWMEMLOG_ARG_PRINTF_INTPTR("sbreq=0x%" PRIX64, (intptr_t)this),
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64, (intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );
}

StartStatus SbReq::start_xact()
{
  if (!instance_->dts_ready()) {
    return done_with_error( "Distributed transaction service is not ready" );
  }

  auto ss = start_xact_int();
  if (StartStatus::Async == ss ) {
    was_async_ = true;
  }
  return ss;
}

void SbReq::abort()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "abort southbound request",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  if (xact_) {
    rwdts_xact_abort( xact_, RW_STATUS_FAILURE, "RW.MgmtAgent transaction aborted" );
    xact_ = nullptr;
  }
}

void SbReq::async_delete(void *ud)
{
  SbReq *req = static_cast<SbReq*>(ud);
  RW_ASSERT (req);
  delete req;
}

void SbReq::schedule_async_delete()
{
  if (RW_MGMTAGT_SB_REQ_TYPE_GETNETCONF == sbreq_type_ ||
      RW_MGMTAGT_SB_REQ_TYPE_RPC        == sbreq_type_) {

    /* Has to be in the main queue for two reasons.
     * 1. For calling dts xact unref.
     * 2. For updating the stats in the instance_.
     */
    rwsched_dispatch_async_f (instance_->rwsched_tasklet(),
                              rwsched_dispatch_get_main_queue(instance_->rwsched()),
                              this, async_delete);
    return;
  }

  delete this;
}

StartStatus SbReq::done_with_success()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "done with success" );
  RW_ASSERT(!responded_);
  responded_ = true;
  update_stats(RW_UAGENT_NETCONF_DTS_XACT_DONE);

  RWMEMLOG_TIME_CODE(
    ( nbreq_->respond( this ); ),
    memlog_buf_, RWMEMLOG_MEM2, "respond with self",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  schedule_async_delete();
  return StartStatus::Done;
}

StartStatus SbReq::done_with_success(
  rw_yang::XMLDocument::uptr_t rsp_dom )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "done with success, specific dom" );
  RW_ASSERT(!responded_);
  responded_ = true;
  update_stats(RW_UAGENT_NETCONF_DTS_XACT_DONE);

  RWMEMLOG_TIME_CODE(
    ( nbreq_->respond( this, std::move(rsp_dom) ); ),
    memlog_buf_, RWMEMLOG_MEM2, "respond with dom",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  schedule_async_delete();
  return StartStatus::Done;
}

StartStatus SbReq::done_with_success(
  rwdts_xact_t *xact )
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "done with success, transaction",
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );
  RW_ASSERT(!responded_);
  responded_ = true;
  update_stats(RW_UAGENT_NETCONF_DTS_XACT_DONE);

  RWMEMLOG_TIME_CODE(
    ( nbreq_->respond( this, xact ); ),
    memlog_buf_, RWMEMLOG_MEM2, "respond with transaction",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  schedule_async_delete();
  return StartStatus::Done;
}

StartStatus SbReq::done_with_success(
  const rw_keyspec_path_t* ks,
  const ProtobufCMessage* msg )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "done with success, protobuf" );
  RW_ASSERT(!responded_);
  responded_ = true;

  RWMEMLOG_TIME_CODE(
    ( nbreq_->respond( this, ks, msg ); ),
    memlog_buf_, RWMEMLOG_MEM2, "respond with keyspec and message",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  schedule_async_delete();
  return StartStatus::Done;
}

StartStatus SbReq::done_with_error()
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "done with error, recorded" );
  return done_with_error( &nc_errors_ );
}

StartStatus SbReq::done_with_error(
  const std::string& error_str )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "done with error, string",
    RWMEMLOG_ARG_STRNCPY(80, error_str.c_str()) );
  add_error().set_error_message( error_str.c_str() );
  return done_with_error();
}

StartStatus SbReq::done_with_error(
  NetconfErrorList* nc_errors )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "done with error, list" );
  RW_ASSERT(!responded_);
  responded_ = true;
  update_stats(RW_UAGENT_NETCONF_DTS_XACT_DONE);

  RW_ASSERT(nc_errors->length());
  // ATTN: convert reasons to string?
  // ATTN: Log the reason

  std::string error_xml;
  nc_errors->to_xml(instance_->xml_mgr(),error_xml);
  instance_->log_file_manager()->log_failure(this, trace_name_, error_xml);

  RWMEMLOG_TIME_CODE(
    ( nbreq_->respond( this, nc_errors ); ),
    memlog_buf_, RWMEMLOG_MEM2, "respond with error list",
    RWMEMLOG_ARG_PRINTF_INTPTR("nbreq=0x%" PRIX64,(intptr_t)nbreq_),
    RWMEMLOG_ARG_PRINTF_INTPTR("dts xact=0x%" PRIXPTR, (intptr_t)xact_) );

  schedule_async_delete();
  return StartStatus::Done;
}

NetconfError& SbReq::add_error(
  RwNetconf_ErrorType errType,
  IetfNetconf_ErrorTagType errTag )
{
  RWMEMLOG( memlog_buf_, RWMEMLOG_MEM2, "add error",
    RWMEMLOG_ARG_PRINTF_INTPTR("type=%" PRIuPTR, (intptr_t)errType),
    RWMEMLOG_ARG_PRINTF_INTPTR("tag=%" PRIuPTR, (intptr_t)errTag) );
  return nc_errors_.add_error( errType, errTag );
}

void SbReq::update_stats (dom_stats_state_t state)
{
  struct timeval tv;
  gettimeofday (&tv, nullptr);

  // Find the elapsed time
  struct timeval elapsed;

  RW_ASSERT(timercmp(&tv, &last_stat_time_, >=));
  timersub (&tv, &last_stat_time_, &elapsed);

  uint32_t diff = elapsed.tv_sec * 1000000 + elapsed.tv_usec;
  last_stat_time_ = tv;

  switch (state) {
    case RW_UAGENT_NETCONF_REQ_RVCVD:
      RW_ASSERT_NOT_REACHED();
      break;
    case RW_UAGENT_NETCONF_PARSE_REQUEST:
      statistics_.has_request_parse_time = 1;
      statistics_.request_parse_time = diff;
      break;
    case RW_UAGENT_NETCONF_DTS_XACT_START:
      statistics_.has_transaction_start_time = 1;
      statistics_.transaction_start_time = diff;
      break;
    case RW_UAGENT_NETCONF_DTS_XACT_DONE:
      statistics_.has_dts_response_time = 1;
      statistics_.dts_response_time = diff;
      break;
    case RW_UAGENT_NETCONF_RESPONSE_BUILT:
      statistics_.has_response_parse_time = 1;
      statistics_.response_parse_time = diff;
      break;
  }
}

void SbReq::rwdts_xact_end_wrapper (void *xact)
{
}
