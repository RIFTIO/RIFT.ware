
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
 * @file rwuagent_msg_client.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 05/23/2014
 * @brief uagent client functions
 * @details uagent client functions
 */

#include "rwuagent_msg_client.h"

RW_CF_TYPE_DEFINE("Micro agent client type info", rwuagent_client_info_ptr_t);


rw_status_t rwtasklet_register_uagent_client(rwuagent_client_info_ptr_t rwuagent_info)
{
  rw_status_t status;
  rwtasklet_info_ptr_t rwtasklet_info = rwuagent_info->rwtasklet_info;

  RW_CF_TYPE_VALIDATE(rwuagent_info, rwuagent_client_info_ptr_t);

  // Instantiate a Test service client and bind this to the client channel
  RW_UAGENT__INITCLIENT(&rwuagent_info->rwuacli);

  RW_ASSERT(rwtasklet_info->rwmsg_endpoint);

  // Create a client channel pointing at the Test service's path
  // Instantiate a Test service client and connect this to the client channel
  rwuagent_info->dest = rwmsg_destination_create(rwtasklet_info->rwmsg_endpoint,
                                                 RWMSG_ADDRTYPE_UNICAST,
                                                 RWUAGENT_PATH);
  RW_ASSERT(rwuagent_info->dest);

  // Create a client channel that rwcli uses to recieve messages
  rwuagent_info->cc = rwmsg_clichan_create(rwtasklet_info->rwmsg_endpoint);
  RW_ASSERT(rwuagent_info->cc);

  // Bind a single service for uagent
  rwmsg_clichan_add_service(rwuagent_info->cc, &rwuagent_info->rwuacli.base);

  // Bind this srvchan to rwsched's taskletized cfrunloop
  // This will result in events executing in the cfrunloop context
  // rwsched_CFRunLoopRef runloop = rwsched_tasklet_CFRunLoopGetCurrent(tenv.rwsched);
  status = rwmsg_clichan_bind_rwsched_cfrunloop(rwuagent_info->cc,
                                                rwtasklet_info->rwsched_tasklet_info);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  return status;
}
