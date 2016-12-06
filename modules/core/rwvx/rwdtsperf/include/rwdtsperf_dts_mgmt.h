
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
