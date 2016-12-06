
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
 * @file rwuagent_msg.cpp
 *
 * Management agent RW.Msg integration.
 */

#include <rwvcs.h>
#include "rwuagent.hpp"
#include "rwuagent_msg_client.h"

using namespace rw_uagent;
using namespace rw_yang;


rwmsg_endpoint_t* rwuagent_get_rwmsg_endpoint(
  rwuagent_instance_t instance)
{
  RW_CF_TYPE_VALIDATE(instance, rwuagent_instance_t);
  return instance->rwtasklet_info ? instance->rwtasklet_info->rwmsg_endpoint : NULL;
}

MsgClient::MsgClient(
  Instance* instance )
: instance_(instance),
  memlog_buf_(
    instance->get_memlog_inst(),
    "MsgClient",
    reinterpret_cast<intptr_t>(this) )
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "start msg server");

  endpoint_ = rwuagent_get_rwmsg_endpoint(instance_->rwuai_);
  RW_ASSERT(endpoint_);

  // Create the server channel
  srvchan_ = rwmsg_srvchan_create(endpoint_);
  RW_ASSERT(srvchan_);

  // Bind the channel to the run loop
  rw_status_t rwstatus = rwmsg_srvchan_bind_rwsched_cfrunloop(srvchan_, instance_->rwsched_tasklet());
  RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);

  // Instantiate the uA service
  RW_UAGENT__INITSERVER(&rwua_srv_, reqcb_);
  rwmsg_srvchan_add_service(srvchan_, RWUAGENT_PATH, &rwua_srv_.base, this);

  RW_MA_INST_LOG( instance_, InstanceDebug, "Started Server Channel" );
}

MsgClient::~MsgClient()
{
  // ATTN: close the messaging endpoint
}

void MsgClient::reqcb_netconf_request(
  RwUAgent_Service* srv,
  const NetconfReq* req,
  void* vmsg_client,
  NetconfRsp_Closure rsp_closure,
  void* closure_data)
{
  RW_ASSERT(srv);
  RW_ASSERT(req);
  RW_ASSERT(vmsg_client);
  MsgClient* msg_client = static_cast<MsgClient*>(vmsg_client);
  msg_client->netconf_request( req, rsp_closure, closure_data );
}

void MsgClient::netconf_request(
  const NetconfReq* req,
  NetconfRsp_Closure rsp_closure,
  void* closure_data)
{
  RWMEMLOG_TIME_SCOPE(memlog_buf_, RWMEMLOG_MEM2, "handle msg netconf request");
  auto nbreq = new NbReqMsg( instance_, req, rsp_closure, closure_data );
  nbreq->execute();
}

