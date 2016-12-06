
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
 * @file rwmsg_broker_main.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG broker
 */

#include "rwmsg_int.h"
#include "rwmsg_broker.h"

void rwmsg_broker_main(uint32_t sid,
		       uint32_t instid,
		       uint32_t bro_instid,
		       rwsched_instance_ptr_t rws,
		       rwsched_tasklet_ptr_t tinfo,
		       rwvcs_zk_module_ptr_t rwvcs_zk,
		       uint32_t usemainq,
           rwtasklet_info_t *rwtasklet_info,
		       rwmsg_broker_t **bro_out) {
  
  rwmsg_broker_t *bro;
  rwmsg_endpoint_t *ep;

  ep = rwmsg_endpoint_create(sid, instid, bro_instid, rws, tinfo, rwtrace_init(), NULL);

  bro = rwmsg_broker_create(sid, bro_instid, NULL, rws, tinfo, rwvcs_zk, usemainq, ep, rwtasklet_info);
  if (bro_out) {
    *bro_out = bro;
  }

  /* Decrement by one, such that broker_halt will release the endpoint */
  RW_ASSERT(ep->refct >= 2);
  rwmsg_endpoint_release(ep);

  return;
}

void rwmsg_broker_test_main(uint32_t sid,
                            uint32_t instid,
                            uint32_t bro_instid,
                            rwsched_instance_ptr_t rws,
                            rwsched_tasklet_ptr_t tinfo,
                            void *rwvcs_zk,
                            uint32_t usemainq,
                            void **bro_out) {
  rwmsg_broker_main(sid, instid, bro_instid, rws, tinfo, (rwvcs_zk_module_ptr_t) rwvcs_zk, usemainq, NULL, (rwmsg_broker_t **) bro_out);
}
