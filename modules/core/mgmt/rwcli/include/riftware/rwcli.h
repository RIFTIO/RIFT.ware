
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWCLI_H__
#define __RWCLI_H__

#include <rw_tasklet_plugin.h>
#include "rwuagent_msg_client.h"
#include "rwlogd.pb-c.h"

__BEGIN_DECLS

struct rwcli_component_s {
  CFRuntimeBase _base;
};

RW_TYPE_DECL(rwcli_component);
RW_CF_TYPE_EXTERN(rwcli_component_ptr_t);

struct rwcli_s {};

struct rwcli_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwcli_component_ptr_t component;
  struct rwuagent_client_info_s uagent;
  struct rwcli_s* rwcli;
  Rwlogd_Service rwlogd_srv;
  rwmsg_srvchan_t *sc;
};

RW_TYPE_DECL(rwcli_instance);
RW_CF_TYPE_EXTERN(rwcli_instance_ptr_t);

rwcli_component_ptr_t
rwcli_component_init(void);

rwcli_instance_ptr_t
rwcli_instance_alloc(rwcli_component_ptr_t component,
		                 struct rwtasklet_info_s * rwtasklet_info,
		                 RwTaskletPlugin_RWExecURL *instance_url);

void
rwcli_instance_free(rwcli_component_ptr_t component,
		                rwcli_instance_ptr_t instance);

void
rwcli_instance_start(rwcli_component_ptr_t component,
		                 rwcli_instance_ptr_t instance);

__END_DECLS

#endif //__RWCLI_H__
