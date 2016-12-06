
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


/**
 * @file redis_client.c
 * @author Prashanth Bhaskar
 * @date 06/10/2016
 *
 */

#include "redis_client.h"

typedef rw_status_t (*startup_func_p)();
typedef void* (*handle_func_p)();

void *
rwvcs_get_redish_dynamic(unsigned int db)
{

  void *handle = dlopen ("libkv_light_api.so", RTLD_NOW);

  if (!handle) {
    return NULL;
  }

 handle_func_p  start_funct = (handle_func_p)dlsym(handle, "rwdts_kv_allocate_handle");

  if(!start_funct)
  {
    return NULL;
  }

  return start_funct(db);
}

rw_status_t
rwvcs_connect_redis(void *kv_handle,
                    rwsched_instance_t *sched,
                    rwsched_tasklet_t *tasklet,
                    char *uri, const char *file_name,
                    const char *program_name,
                    void *callbkfn, void *callbkdata)
{

  void *handle = dlopen ("libkv_light_api.so", RTLD_NOW);

  if (!handle) {
    return RW_STATUS_FAILURE;
  }

  startup_func_p  start_funct = (startup_func_p)dlsym(handle, "rwdts_kv_handle_db_connect");

  if (!start_funct) {
    return RW_STATUS_FAILURE;
  }

  return start_funct(kv_handle, sched, tasklet, uri, file_name, program_name, callbkfn, callbkdata);
}

rw_status_t
rwvcs_r_mas_to_sla(void *r_handle,
                   char *hostname, uint16_t port,
                   void *callback,
                   void *userdata)
{
  void *handle = dlopen ("libkv_light_api.so", RTLD_NOW);

  if (!handle) {
    return RW_STATUS_FAILURE;
  }

  startup_func_p  start_funct = (startup_func_p)dlsym(handle, "rwdts_redis_wrapper_master_to_slave");

  if (!start_funct) {
    return RW_STATUS_FAILURE;
  }

  start_funct(r_handle, hostname, port, callback, userdata);
  dlclose(handle);

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwvcs_r_sla_to_mas(void *r_handle,
                   void *callback,
                   void *userdata)
{
  void *handle = dlopen ("libkv_light_api.so", RTLD_NOW);

  if (!handle) {
    return RW_STATUS_FAILURE;
  }

  startup_func_p  start_funct = (startup_func_p)dlsym(handle, "rwdts_redis_wrapper_slave_to_master");

  if (!start_funct) {
    return RW_STATUS_FAILURE;
  }

  start_funct(r_handle, callback, userdata);
  dlclose(handle);

  return RW_STATUS_SUCCESS;
}

uint32_t
rwdts_rwmain_m_to_s(void* handle, void *userdata)
{
  RW_ASSERT(userdata);
  return 1;
}

uint32_t
rwdts_rwmain_s_to_m(void* handle, void *userdata)
{

  RW_ASSERT(userdata);
  return 1;
}

void 
rwmain_redis_notify_transition(struct rwmain_gi *rwmain, vcs_vm_state state)
{
  if (state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY) {
    rwmain_pm_struct_t pm = {};
    rw_status_t status = read_pacemaker_state(&pm);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    int indx;
    char *lead_ip_address = NULL;
    for(indx=0; pm.ip_entry[indx].pm_ip_addr; indx++) {
      if (pm.ip_entry[indx].pm_ip_state == RWMAIN_PM_IP_STATE_LEADER) {
        lead_ip_address = pm.ip_entry[indx].pm_ip_addr;
      }
    }
    RW_ASSERT(lead_ip_address);

    rwvcs_r_mas_to_sla((void *)rwmain->redis_handle,
                       lead_ip_address, 9371, rwdts_rwmain_m_to_s, (void *)rwmain);
  }
  else {
    rwvcs_r_sla_to_mas((void *)rwmain->redis_handle,
                       rwdts_rwmain_s_to_m, (void *)rwmain);
  }
}

static void rwmain_redis_ready(struct rwmain_gi *rwmain,
                               rw_status_t rt_status)
{
  if (rt_status != RW_STATUS_SUCCESS) {
    /* TODO */
    if (rwmain->rwvx->rwvcs->mgmt_info.state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY) {
      /* Fail this standby VM and restart */
    } else {
      /* Fail this VM and switch the standby to active */
    }
    return;
  }
  rwmain_redis_notify_transition(rwmain, rwmain->rwvx->rwvcs->mgmt_info.state);
  return;
}

void
rwmain_start_redis_client(struct rwmain_gi *rwmain)
{
  char address_port[20];
  int len;
  RW_ASSERT(rwmain);
  RW_ASSERT(rwmain->rwvx);
  len = sprintf(address_port, "%s%c%d", rwmain->vm_ip_address, ':', 9371);
  address_port[len] = '\0';
  rwmain->redis_handle = rwvcs_get_redish_dynamic(0);
  rw_status_t status = rwvcs_connect_redis(rwmain->redis_handle, rwmain->rwvx->rwsched,
                                           rwmain->rwvx->rwsched_tasklet, address_port,
                                           "controller", NULL,
                                           (void *)rwmain_redis_ready, (void *)rwmain);
  RW_ASSERT(RW_STATUS_SUCCESS == status);
  return;
}

rw_status_t
rwmain_gen_redis_conf_file(struct rwmain_gi *rwmain,
                           vcs_manifest_component *m_component)
{
  FILE *fp;

  RW_ASSERT(rwmain);
  bool active = FALSE;

  if (rwmain->rwvx && rwmain->rwvx->rwvcs) {
    active = (rwmain->rwvx->rwvcs->mgmt_info.state == RWVCS_TYPES_VM_STATE_MGMTACTIVE);
  } else {
    return RW_STATUS_FAILURE;
  }

  char *installdir = getenv("RIFT_VAR_ROOT");
  RW_ASSERT(installdir);

  char file_name[512];
  char args[512];

  if (active) {
    snprintf(file_name, 512, "%s%c%s", installdir, '/', "active_redis.conf");
    snprintf(args, 512, "%s%s", file_name, " --port 9371");
    m_component->native_proc->args = RW_STRDUP(args);
  } else {
    snprintf(file_name, 512, "%s%c%s", installdir, '/', "standby_redis.conf");
    snprintf(args, 512, "%s%s", file_name, " --port 9371");
  }
  m_component->native_proc->args = RW_STRDUP(args);

  fp = fopen(file_name, "w+");

  if (!fp) {
    return RW_STATUS_FAILURE;
  }

  fprintf(fp, "%s %d\n", "port", 9371);
  fprintf(fp, "%s %s\n", "bind", rwmain->vm_ip_address);
  fprintf(fp, "%s\n", "unixsocket redis.sock");
  fprintf(fp, "%s\n", "unixsocketperm 755");
  fprintf(fp, "%s\n", "appendonly yes");
  fprintf(fp, "%s\n", "save 60 100");
  fprintf(fp, "%s %s", "dir", installdir);


  fclose(fp);

  return RW_STATUS_SUCCESS;
}
