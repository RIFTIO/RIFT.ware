
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


/*!
 * @file rwdts_router.h
 * @brief Router for RW.DTS
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_ROUTER_TASKLET_H
#define __RWDTS_ROUTER_TASKLET_H

#include <uthash.h>
#include <rw_tasklet_plugin.h>
#include <rwtasklet.h>
#include <rwdts_router.h>
#include <rwvcs_zk_api.h>

__BEGIN_DECLS

// Router tasklet definitions
//
struct rwdtsrouter_component_s {
  CFRuntimeBase _base;
};
RW_TYPE_DECL(rwdtsrouter_component);
RW_CF_TYPE_EXTERN(rwdtsrouter_component_t);

struct rwdtsrouter_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwdtsrouter_component_ptr_t component;
  rwdts_router_t *router;
};

RW_TYPE_DECL(rwdtsrouter_instance);
RW_CF_TYPE_EXTERN(rwdtsrouter_instance_ptr_t);

rwtrace_ctx_t* rwdtsrouter_get_rwtrace_instance(
  rwdtsrouter_instance_ptr_t instance);

rwdts_router_t*
rwdtsrouter_instance_start(rwtasklet_info_ptr_t  rwtasklet_info,
                           int instance_id,
                           rwvcs_zk_module_ptr_t rwvcs_zk);

__END_DECLS


#endif /* __RWDTS_ROUTER_TASKLET_H */
