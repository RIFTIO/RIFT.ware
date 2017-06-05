/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 6/6/16
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#include <rw_var_root.h>
#include "rwtasklet_var_root.h"

char* rwtasklet_info_get_rift_var_root(rwtasklet_info_t *tasklet_info)
{
  char rift_var_root[1024] = {0};
  char *ret;
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
    ret =  NULL;
  }else{
    ret = strdup(rift_var_root); 
  }

  if (vnf){
    free(vnf);
  }
  
  return ret;
}

rw_status_t rwtasklet_create_rift_var_root(const char* rift_var_root)
{
  return rw_util_create_rift_var_root(rift_var_root);
}

rw_status_t rwtasklet_destory_rift_var_root(const char* rift_var_root)
{
  return rw_util_remove_rift_var_root(rift_var_root);
}
