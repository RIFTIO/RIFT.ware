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
 * Creation Date: 6/6/16
 * 
 */

#include <rw_var_root.h>
#include "rwtasklet_var_root.h"

char* rwtasklet_info_get_rift_var_name(rwtasklet_info_t *tasklet_info)
{
  char rift_var_name[1024] = {0};

  char* vnf = rwtasklet_info_get_vnfname(tasklet_info);
  char* vm = getenv("RIFT_VAR_VM");
  if (!vm) {
    vm = rwtasklet_info_get_parent_vm_instance_name(tasklet_info);
  }

  rw_status_t s = rw_util_get_rift_var_name(NULL,
                                            vnf,
                                            vm,
                                            rift_var_name,
                                            1024);

  if (s != RW_STATUS_SUCCESS) {
    return NULL;
  }

  return strdup(rift_var_name);
}

char* rwtasklet_info_get_rift_var_root(rwtasklet_info_t *tasklet_info)
{
  char rift_var_root[1024] = {0};

  char* vnf = rwtasklet_info_get_vnfname(tasklet_info);
  char* vm = getenv("RIFT_VAR_VM");
  if (!vm) {
    vm = rwtasklet_info_get_parent_vm_instance_name(tasklet_info);
  }

  rw_status_t s = rw_util_get_rift_var_root(NULL,
                                            vnf,
                                            vm,
                                            rift_var_root,
                                            1024);

  if (s != RW_STATUS_SUCCESS) {
    return NULL;
  }

  return strdup(rift_var_root);
}

rw_status_t rwtasklet_create_rift_var_root(const char* rift_var_root)
{
  return rw_util_create_rift_var_root(rift_var_root);
}

rw_status_t rwtasklet_destory_rift_var_root(const char* rift_var_root)
{
  return rw_util_remove_rift_var_root(rift_var_root);
}
