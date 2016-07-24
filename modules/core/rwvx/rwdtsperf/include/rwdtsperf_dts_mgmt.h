
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#ifndef __RWDTSPERF_DTS_MGMT_H_
#define __RWDTSPERF_DTS_MGMT_H_
#include <rwdts_appconf_api.h>
__BEGIN_DECLS

struct rwdtsperf_config_scratch_s {
  void *config[128];
  int count;
};

RW_TYPE_DECL(rwdtsperf_config_scratch);
RW_CF_TYPE_EXTERN(rwdtsperf_config_scratch_ptr_t);

rw_status_t rwdtsperf_dts_mgmt_register_dtsperf_config (rwdtsperf_instance_ptr_t instance);

__END_DECLS
#endif //__RWDTSPERF_DTS_MGMT_H_
