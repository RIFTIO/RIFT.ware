
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


#include "riftware/rwtasklet.h"

/**
 * @file rwuagent.h
 * @author Tom Seidenberg
 * @date 2014/04/03
 * @brief RW.uAgent tasklet definitions
 */

#ifndef CORE_MGMT_RWUAGENT_H__
#define CORE_MGMT_RWUAGENT_H__

#include <sys/cdefs.h>
#include <rw_tasklet_plugin.h>
#include <rw_xml.h>

#include "rwuagent.pb-c.h"

__BEGIN_DECLS

struct rwuagent_component_s {
  CFRuntimeBase _base;
};

typedef struct rwuagent_component_s* rwuagent_component_t;
RW_CF_TYPE_EXTERN(rwuagent_component_t);

struct rwuagent_instance_s;
typedef struct rwuagent_instance_s* rwuagent_instance_t;
RW_CF_TYPE_EXTERN(rwuagent_instance_t);

rwuagent_component_t rwuagent_component_init(void);

void rwuagent_component_deinit(
  rwuagent_component_t component);

rwuagent_instance_t rwuagent_instance_alloc(
  rwuagent_component_t component,
  struct rwtasklet_info_s * rwtasklet_info,
  RwTaskletPlugin_RWExecURL *instance_url);

void rwuagent_instance_free(
  rwuagent_component_t component,
  rwuagent_instance_t instance);

void rwuagent_instance_start(
  rwuagent_component_t component,
  rwuagent_instance_t instance);

rwtrace_ctx_t* rwuagent_get_rwtrace_instance(
  rwuagent_instance_t instance);

rw_yang_netconf_op_status_t rwuagent_instance_test(
  rwuagent_component_t component,
  rwuagent_instance_t instance,
  const char *xml_str,
  NetconfOperations op,
  NetconfRsp_Closure closure,
  void *closure_data);

__END_DECLS

#endif // CORE_MGMT_RWUAGENT_H__
