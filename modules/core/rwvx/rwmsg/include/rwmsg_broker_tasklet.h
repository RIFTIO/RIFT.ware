
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */


#ifndef __RWMSG_BROKER_TASKLET_H__
#define __RWMSG_BROKER_TASKLET_H__

#include "rwtasklet.h"
#include "rwdts.h"

__BEGIN_DECLS

struct rwmsgbroker_component_s {                                                                                                                                              
  int foo;
};

RW_TYPE_DECL(rwmsgbroker_component);
RW_CF_TYPE_EXTERN(rwmsgbroker_component_ptr_t);

struct rwmsgbroker_instance_s {
  CFRuntimeBase _base;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwmsg_broker_t *broker;
  rwdts_api_t *dts_h;
};

RW_TYPE_DECL(rwmsgbroker_instance);
RW_CF_TYPE_EXTERN(rwmsgbroker_instance_ptr_t);


__END_DECLS

#endif // __RWMSG_BROKER_TASKLET_H__
