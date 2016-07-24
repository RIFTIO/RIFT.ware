
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
#include <rwcal-api.h>

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
                           rwcal_module_ptr_t rwcal);

__END_DECLS


#endif /* __RWDTS_ROUTER_TASKLET_H */
