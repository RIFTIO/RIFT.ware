
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */


#include <sys/prctl.h>
#include <sys/wait.h>
#include <rwcal-api.h>

#include "rwvcs.h"
#include "rwvcs_component.h"
#include "rwvcs_internal.h"
#include "rwvcs_manifest.h"
#include "rwvcs_rwzk.h"
#include "rwcal_vars_api.h"
#include <dirent.h>


RW_CF_TYPE_DEFINE("RW.VX Instance Type", rwvx_instance_ptr_t);
RW_CF_TYPE_DEFINE("RW.VCS Instance Type", rwvcs_instance_ptr_t);

static void rwmain_crash_task()
{
    RW_CRASH();
}


static void stop_any_local_zookeeper(void)
{
  DIR * proc;
  struct dirent * ent;

  proc = opendir("/proc/");
  for (ent = readdir(proc); ent; ent = readdir(proc)) {
    FILE * fp;
    char path[128];
    char cmdline_start[25];
    size_t nb;

    if (!isdigit(ent->d_name[0]))
      continue;

    #ifdef _DIRENT_HAVE_D_TYPE
    if (ent->d_type != DT_DIR)
      continue;
    #endif

    int r = snprintf(path, 128, "/proc/%s/cmdline", ent->d_name);
    RW_ASSERT(r != -1);

    fp = fopen(path, "r");
    if (!fp)
      continue;

    nb = fread(&cmdline_start, sizeof(char), 25, fp);
    if (nb < 25) {
      fclose(fp);
      continue;
    }

    cmdline_start[24] = '\0';

    fclose(fp);

    if (strncmp(cmdline_start, "java", 4)
        || strncmp(cmdline_start + 5, "-cp", 3)
        || strncmp(cmdline_start + 9, "/etc/zookeeper", 14)) {
      continue;
    }
    //send_kill_to_pid (atoi(ent->d_name), SIGTERM, "zookeeper kill");
  }
  closedir(proc);
}

rw_status_t start_zookeeper_server(
    rwvcs_instance_ptr_t rwvcs,
    vcs_manifest_bootstrap * bootstrap)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwvcs_mgmt_info_t *mgmt_info = &rwvcs->mgmt_info;
  if ((bootstrap->zookeeper->zake) 
      || (!mgmt_info->mgmt_vm))
    goto done;

  mgmt_info->config_start_zk_pending = false;

  struct timeval tv_begin;
  gettimeofday(&tv_begin, NULL);

  int indx;
  for (indx = 0; (indx < RWVCS_ZK_SERVER_CLUSTER) && mgmt_info->zk_server_port_details[indx]; indx++) {
    if (mgmt_info->zk_server_port_details[indx]->zk_server_start) {
    mgmt_info->zk_server_port_details[indx]->zk_server_pid = rwvcs_rwzk_server_start(
          rwvcs,
          mgmt_info->zk_server_port_details[indx]->zk_server_id,
          mgmt_info->zk_server_port_details[indx]->zk_client_port,
          mgmt_info->unique_ports,
          mgmt_info->zk_server_port_details);
    }
  }

done:

  return status;
}


static rw_status_t apply_rwtrace_settings(
    rwtrace_ctx_t * rwtrace,
    vcs_manifest_trace * trace)
{
  rw_status_t status;
  rwtrace_severity_t severity = RWTRACE_SEVERITY_DEBUG;

  if (!trace->has_enable || !trace->enable)
    return RW_STATUS_SUCCESS;

  if (trace->has_level)
    severity = trace->level <= RWTRACE_SEVERITY_DEBUG ? trace->level : RWTRACE_SEVERITY_DEBUG;

  for (int i = 0 ; i < RWTRACE_CATEGORY_LAST ; i++) {
    status = rwtrace_ctx_category_severity_set(rwtrace, i, severity);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    status = rwtrace_ctx_category_destination_set(rwtrace, i, RWTRACE_DESTINATION_CONSOLE);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t apply_rwvcs_settings(
    rwvcs_instance_ptr_t rwvcs,
    vcs_manifest_settings * settings)
{
  if (!settings || !settings->rwvcs)
    return RW_STATUS_SUCCESS;

  if (settings->rwvcs->rwmain_path) {
    if (rwvcs->rwmain_exefile)
      free(rwvcs->rwmain_exefile);

    rwvcs->rwmain_exefile = strdup(settings->rwvcs->rwmain_path);
    RW_ASSERT(rwvcs->rwmain_exefile);
  }

  if (settings->rwvcs->reaper_path) {
    if (rwvcs->reaper_path)
      free(rwvcs->reaper_path);

    rwvcs->reaper_path = strdup(settings->rwvcs->reaper_path);
    RW_ASSERT(rwvcs->reaper_path);
  }

  return RW_STATUS_SUCCESS;
}


static rw_status_t apply_vmpool_settings(
    rwvcs_instance_ptr_t rwvcs,
    vcs_manifest_rwcal * rwcal)
{
  vcs_manifest_vm_pool *irwvmpool;
  vcs_manifest_vm_pool_list *irwvmpoolstatic;
  rwvcs_vmpool_entry_t *vmpool;
  rwvcs_vmip_entry_t *vmip;
  rw_status_t status;

  if (!rwcal)
    return RW_STATUS_SUCCESS;


  for (size_t i = 0 ; i < rwcal->n_rwvm_pool; i ++) {
    irwvmpool = rwcal->rwvm_pool[i];
    RW_ASSERT(irwvmpool);

    //Create skiplist entry for pool
    vmpool = RW_MALLOC0(sizeof(*vmpool));
    RW_ASSERT(vmpool);

    strcpy(vmpool->vmpoolname, irwvmpool->name);

    status = RW_SKLIST_INSERT(&(rwvcs->vmpool_slist), vmpool);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    if (status == RW_STATUS_SUCCESS) {
      RW_SKLIST_PARAMS_DECL(
          vmpool_ip_list_,
          rwvcs_vmip_entry_t,
          vmname,
          rw_sklist_comp_charbuf,
          vm_element);
      RW_SKLIST_INIT(&(vmpool->u.vmpool_ip_slist), &vmpool_ip_list_);

    }


    if (irwvmpool->n_static_vm_list) {
      vmpool->pool_type = RWCAL_VMPOOL_STATIC_LIST;

      for (size_t j = 0; j < irwvmpool->n_static_vm_list; j++ ) {
        irwvmpoolstatic = irwvmpool->static_vm_list[j];

        if (!irwvmpoolstatic)
          continue;

        if (irwvmpoolstatic->vm_name && irwvmpoolstatic->vm_ip_address) {
          //Add VM entry into list vm_name vm_ip_addr
          vmip = RW_MALLOC0(sizeof(*vmip));
          RW_ASSERT(vmip);

          strcpy(vmip->vmname,irwvmpoolstatic->vm_name);

          status = rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(
              &(vmip->vm_ip_addr),
              irwvmpoolstatic->vm_ip_address,
              strlen(irwvmpoolstatic->vm_ip_address));
          RW_ASSERT(status == RW_STATUS_SUCCESS);

          status = RW_SKLIST_INSERT(&(vmpool->u.vmpool_ip_slist), vmip);
          RW_ASSERT(status == RW_STATUS_SUCCESS);
        }
      }
    } else if (irwvmpool->static_vm_range) {
      vcs_manifest_vm_pool_range *staticrange;

      vmpool->pool_type = RWCAL_VMPOOL_STATIC_RANGE;
      staticrange = irwvmpool->static_vm_range;

      if (staticrange->vm_name_range && staticrange->vm_ip_range) {
        vcs_manifest_vm_pool_name_range * vmnamerange;
        vcs_manifest_vm_pool_ip_range * vmiprange;

        vmnamerange = staticrange->vm_name_range;
        vmiprange = staticrange->vm_ip_range;
        RW_ASSERT(vmnamerange->base_name && vmnamerange->has_low_index && vmnamerange->has_high_index);
        RW_ASSERT(vmnamerange->low_index <= vmnamerange->high_index);
        RW_ASSERT(vmiprange->low_addr && vmiprange->high_addr);

        vmpool->u.vm_range.low_index = vmnamerange->low_index;
        vmpool->u.vm_range.high_index = vmnamerange->high_index;
        vmpool->u.vm_range.curr_index = 0;

        RW_ASSERT(strlen(vmnamerange->base_name) < RWVCS_VM_NAME_LEN);
        strcpy(vmpool->u.vm_range.base_name, vmnamerange->base_name);
        status = rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(
            &(vmpool->u.vm_range.low_addr),
            vmiprange->low_addr,
            strlen(vmiprange->low_addr));
        RW_ASSERT(status == RW_STATUS_SUCCESS);

        status = rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(
            &(vmpool->u.vm_range.high_addr),
            vmiprange->high_addr,
            strlen(vmiprange->high_addr));
        RW_ASSERT(status == RW_STATUS_SUCCESS);

        RW_ASSERT(vmpool->u.vm_range.low_addr.ip_v == vmpool->u.vm_range.high_addr.ip_v);
        RW_ASSERT( RW_IS_ADDR_IPV4(&vmpool->u.vm_range.high_addr) == TRUE);
        RW_ASSERT((vmpool->u.vm_range.high_index - vmpool->u.vm_range.low_index) == (vmpool->u.vm_range.high_addr.u.v4.addr - vmpool->u.vm_range.low_addr.u.v4.addr));
      }
    } else {
      RW_CRASH();
    }
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t initialize_zookeeper(rwvcs_instance_ptr_t rwvcs, const char *ip_address)
{
  rw_status_t status;

#if 1
  if (rwvcs->rwvx->rwsched_tasklet &&
      !rwvcs->zk_rwq) {
#if 1
    char zk_rwq_name[256] = {0};
    snprintf(zk_rwq_name, 256, "rwzkq-%d",rwvcs->identity.rwvm_instance_id);
    rwvcs->zk_rwq = rwsched_dispatch_queue_create(rwvcs->rwvx->rwsched_tasklet,
					zk_rwq_name, DISPATCH_QUEUE_SERIAL);
    RW_ASSERT(rwvcs->zk_rwq);
#endif
  }
#endif

  stop_any_local_zookeeper();
  status = start_zookeeper_server(rwvcs, rwvcs->pb_rwmanifest->bootstrap_phase);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS)
    return status;

  // init and start the zookeeper client
  status = rwvcs_rwzk_client_init(rwvcs);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS)
    return status;

  // See zookeeper auto-instance assigner.
  status = rwvcs_rwzk_seed_auto_instance(
      rwvcs,
      rwvcs->pb_rwmanifest->bootstrap_phase->zookeeper->auto_instance_start,
      NULL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  return status;
}

rwvx_instance_t *
rwvx_instance_alloc(void)
{
  rwvx_instance_t *rwvx;
  static rwsched_instance_t * rwsched = NULL;

  // Register the RW.VX types
  RW_CF_TYPE_REGISTER(rwvx_instance_ptr_t);

  // Allocate a RWVX instance
  rwvx = RW_CF_TYPE_MALLOC0(sizeof(*rwvx), rwvx_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(rwvx, rwvx_instance_ptr_t);

  /*
   * Make sure we only create a single scheduler per linux process.  This
   * supports running in a collapsed model where rwmain() is reinvoked each
   * time a process or vm is started without passing any context.
   */
  if (!rwsched)
    rwsched = rwsched_instance_new();

  // Allocate other RiftWare RWVX component instances
  // Store the handles to these components within the RWVX instance
  rwvx->rwtrace = rwtrace_init();
  rwvx->rwlog = rwlog_init("Logging");
  rwvx->rwsched = rwsched;
  rwvx->rwsched_tasklet = rwsched_tasklet_new(rwvx->rwsched);
  rwvx->rwvcs = rwvcs_instance_alloc();

  // Point everything to rwvx
  rwvx->rwvcs->rwvx = rwvx;

  rwvx->rwcal_module = rwcal_module_alloc();
  RW_ASSERT(rwvx->rwcal_module);

  // Return the allocated instance object
  return rwvx;
}

void rwvx_instance_free(rwvx_instance_t * rwvx)
{
  // RIFT-7086, rwsched doesn't have reference counting
  // rwsched_instance_free(rwvx->rwsched);


  rwlog_close(rwvx->rwlog, false);
  rwtrace_ctx_close(rwvx->rwtrace);
  rwsched_tasklet_free(rwvx->rwsched_tasklet);
  rwvcs_instance_free(rwvx->rwvcs);
  rwcal_module_free(&(rwvx->rwcal_module));
}

rwvcs_instance_ptr_t rwvcs_instance_alloc()
{
  rw_status_t status;
  rwvcs_instance_ptr_t instance;
  char * install_dir;
  int r;

  // Register the RW.VCS types
  RW_CF_TYPE_REGISTER(rwvcs_instance_ptr_t);

  // Allocate a rwvcs_instance structure for the instance
  instance = RW_CF_TYPE_MALLOC0(sizeof(*instance), rwvcs_instance_ptr_t);
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);

  instance->rwvx_instance = rw_vx_framework_alloc();

  //Initialize the VM pool list
  RW_SKLIST_PARAMS_DECL(
      vmpool_list_,
      rwvcs_vmpool_entry_t,
      vmpoolname,
      rw_sklist_comp_charbuf,
      element);
  RW_SKLIST_INIT(&(instance->vmpool_slist),&vmpool_list_);

  instance->xml_mgr = rw_xml_manager_create_xerces();
  RW_ASSERT(instance->xml_mgr);

  rw_yang_model_t * yang_model;
  yang_model = rw_xml_manager_get_yang_model(instance->xml_mgr);
  rw_yang_model_load_module(yang_model, "rw-base");
  rw_yang_model_load_module(yang_model, "rw-manifest");

  install_dir = getenv("INSTALLDIR");
  r = asprintf(&instance->reaper_path, "%s/usr/bin/reaperd", install_dir ? install_dir : "");
  RW_ASSERT(r != -1);

  status = rw_vx_library_open(instance->rwvx_instance, "rwpython_util", "", &instance->mip);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  status = rw_vx_library_get_api_and_iface(
      instance->mip,
      RWPYTHON_UTIL_TYPE_API,
      (void **)&instance->rwpython_util.cls,
      (void **)&instance->rwpython_util.iface,
      NULL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  instance->pb_rwmanifest = (vcs_manifest *)malloc(sizeof(vcs_manifest));
  RW_ASSERT(instance->pb_rwmanifest);
  vcs_manifest__init(instance->pb_rwmanifest);

  return instance;
}

void rwvcs_instance_free(rwvcs_instance_ptr_t rwvcs)
{
  rwvcs_vmpool_entry_t * vmp1;
  rwvcs_vmpool_entry_t * vmp2;


  rw_vx_modinst_close(rwvcs->mip);
  rw_vx_framework_free(rwvcs->rwvx_instance);

  RW_SKLIST_FOREACH_SAFE(vmp1, rwvcs_vmpool_entry_t, &(rwvcs->vmpool_slist), element, vmp2)
    free(vmp1);
  rw_sklist_free(&rwvcs->vmpool_slist);

  rw_xml_manager_release(rwvcs->xml_mgr);

  if (rwvcs->reaper_path)
    free(rwvcs->reaper_path);

  if (rwvcs->rwmain_exefile)
    free(rwvcs->rwmain_exefile);

  if (rwvcs->rwmanifest_xmlfile)
    free(rwvcs->rwmanifest_xmlfile);

  if (rwvcs->identity.vm_ip_address)
    free(rwvcs->identity.vm_ip_address);

  if (rwvcs->ld_preload)
    free(rwvcs->ld_preload);

  protobuf_free(rwvcs->pb_rwmanifest);
}

static rw_status_t rwvcs_instance_process_manifest_path(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_path)
{
  rw_status_t status;

  rwvcs->rwmanifest_xmlfile = strdup(manifest_path);
  RW_ASSERT(rwvcs->rwmanifest_xmlfile);

  status = rwvcs_manifest_load(rwvcs, manifest_path);

  return status;
}

rw_status_t rwvcs_process_manifest_file(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_file)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  if (rwvcs->pb_rwmanifest->bootstrap_phase) {
    return status;
  }

  if (manifest_file) {
    status = rwvcs_instance_process_manifest_path(rwvcs, manifest_file);
    if (status != RW_STATUS_SUCCESS)
      return status;
  }

  status = validate_bootstrap_phase(rwvcs->pb_rwmanifest->bootstrap_phase);
  if (status != RW_STATUS_SUCCESS)
    return status;

  status = apply_rwtrace_settings(rwvcs->rwvx->rwtrace, rwvcs->pb_rwmanifest->bootstrap_phase->rwtrace);
  if (status != RW_STATUS_SUCCESS)
    return status;

  status = apply_vmpool_settings(rwvcs, rwvcs->pb_rwmanifest->init_phase->rwcal);
  if (status != RW_STATUS_SUCCESS)
    return status;

  status = apply_rwvcs_settings(rwvcs, rwvcs->pb_rwmanifest->init_phase->settings);
  if (status != RW_STATUS_SUCCESS)
    return status;

  // Evaluate the starting environment variable list
  status = rwvcs_variable_list_evaluate(
      rwvcs,
      rwvcs->pb_rwmanifest->init_phase->environment->n_python_variable,
      rwvcs->pb_rwmanifest->init_phase->environment->python_variable);

  
  return status;
}

rw_status_t rwvcs_instance_init(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_file,
    const char * ip_address,
    int (*rwmain_f)(int argc, char ** argv, char ** envp))
{
  rw_status_t status;
  char * ld_preload;

  rwvcs->rwmain_f = rwmain_f;
  rwvcs->rwcrash = rwmain_crash_task;

  ld_preload = getenv("LD_PRELOAD");
  if (ld_preload) {
    rwvcs->ld_preload = strdup(ld_preload);
    RW_ASSERT(rwvcs->ld_preload);
    unsetenv("LD_PRELOAD");
  }

  rwvcs_process_manifest_file (rwvcs, manifest_file);

  status = initialize_zookeeper(rwvcs, ip_address);
  if (status != RW_STATUS_SUCCESS)
    return status;

  return RW_STATUS_SUCCESS;
}

void send_kill_to_pid(
    int pid, 
    int signal,
    char *name)
{
  fprintf (stderr, " [[[%d]]] '%s' being send %d by [%d] \n", pid, name, signal, getpid());
  int r = kill(pid, signal);
  if (r != -1) {
    for (size_t i = 0; i < 1000; ++i) {
      r = kill(pid, 0);
      if (r == -1)
        break;
      usleep(1000);
    }

    if (r != -1)
      kill(pid, SIGKILL);
  }
  int wait_status;
  r = waitpid(pid, &wait_status, WNOHANG);
  if (r != pid) {
    fprintf(stderr,
        "Failed to wait for pid %u(%s)",
        pid,
        name);
  }
}

