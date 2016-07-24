
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */


#ifndef __rwmctasklet_H__
#define __rwmctasklet_H__

#include <rw_tasklet_plugin.h>
#include "rwtasklet.h"
#include "rwdts.h"
#include "rwmc_log.h"

struct rwmctasklet_component_s {
  CFRuntimeBase _base;
  /* ADD ADDITIONAL FIELDS HERE */
};

RW_TYPE_DECL(rwmctasklet_component);
RW_CF_TYPE_EXTERN(rwmctasklet_component_ptr_t);


struct rwmctasklet_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwmctasklet_component_ptr_t component;

  rwdts_member_reg_handle_t dts_member_handle;
  rwdts_api_t *dts_h;
  rwdts_appconf_t *dts_mgmt_handle;



  /* ADD ADDITIONAL FIELDS HERE */
};

struct rwmctasklet_scratchpad_s {
  char                             reason[256];
  struct rwmctasklet_instance_s *instance;
};
RW_TYPE_DECL(rwmctasklet_scratchpad);
RW_CF_TYPE_EXTERN(rwmctasklet_scratchpad_ptr_t);


RW_TYPE_DECL(rwmctasklet_instance);
RW_CF_TYPE_EXTERN(rwmctasklet_instance_ptr_t);

rwmctasklet_component_ptr_t rwmctasklet_component_init(void);

void rwmctasklet_component_deinit(rwmctasklet_component_ptr_t component);

rwmctasklet_instance_ptr_t rwmctasklet_instance_alloc(
        rwmctasklet_component_ptr_t component, 
        struct rwtasklet_info_s * rwtasklet_info,
        RwTaskletPlugin_RWExecURL *instance_url);

void rwmctasklet_instance_free(
        rwmctasklet_component_ptr_t component, 
        rwmctasklet_instance_ptr_t instance);

void rwmctasklet_instance_start(
        rwmctasklet_component_ptr_t component, 
        rwmctasklet_instance_ptr_t instance);

#endif //__rwmctasklet_H__

