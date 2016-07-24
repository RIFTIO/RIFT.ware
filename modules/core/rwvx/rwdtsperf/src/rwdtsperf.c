
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#include "rwdtsperf.h"

rwdtsperf_component_ptr_t rwdtsperf_component_init(void)
{
  rwdtsperf_component_ptr_t component;

  // Allocate a new rwdtsperf_component structure
  component = RW_CF_TYPE_MALLOC0(sizeof(*component), rwdtsperf_component_ptr_t);
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);

  // Return the allocated component
  return component;
}

void
rwdtsperf_component_deinit(rwdtsperf_component_ptr_t component)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
}

static void
rwdtsperf_config_init(rwdtsperf_instance_ptr_t instance)
{
  subscriber_config_t *subsc_cfg;
  xact_config_t       *xact_cfg;

  subsc_cfg = &(instance->config.subsc_cfg);
  RW_SKLIST_PARAMS_DECL(rsp_flavor_list_, rsp_flavor_t, rsp_flavor_name,
                        rw_sklist_comp_charbuf, rsp_flavor_slist_element);
  RW_SKLIST_INIT(&(subsc_cfg->rsp_flavor_list),&rsp_flavor_list_);

  xact_cfg = &(instance->config.xact_cfg);
  RW_SKLIST_PARAMS_DECL(xact_detail_list_, xact_detail_t, xact_name,
                        rw_sklist_comp_charbuf, xact_detail_slist_element);
  RW_SKLIST_INIT(&(xact_cfg->xact_detail_list),&xact_detail_list_);

}

rwdtsperf_instance_ptr_t rwdtsperf_instance_alloc(rwdtsperf_component_ptr_t component,
			                                  struct rwtasklet_info_s * rwtasklet_info,
			                                  RwTaskletPlugin_RWExecURL *instance_url)
{
  rwdtsperf_instance_ptr_t instance;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  RW_ASSERT(instance_url);

  // Allocate a new rwdtsperf_instance structure
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwdtsperf_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  // Save the rwtasklet_info structure
  instance->rwtasklet_info = rwtasklet_info;

  rwdtsperf_config_init(instance);

  // Return the allocated instance
  return instance;
}

void
rwdtsperf_instance_stop(rwdtsperf_component_ptr_t component,
			                  rwdtsperf_instance_ptr_t instance)
{
  xact_config_t *xact_cfg;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  xact_cfg = &(instance->config.xact_cfg);

  xact_cfg->running = false;
  xact_cfg->xact_max_with_outstanding = 0;
  rwdts_api_deinit(instance->dts_h);
}

void
rwdtsperf_instance_free(rwdtsperf_component_ptr_t component,
			                  rwdtsperf_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);
}

void
rwdtsperf_instance_start(rwdtsperf_component_ptr_t component,
			    rwdtsperf_instance_ptr_t instance)
{
  // Validate input parameters
  RW_CF_TYPE_VALIDATE(component, rwdtsperf_component_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwdtsperf_instance_ptr_t);

  rwdtsperf_register_dts_callbacks (instance);
}
