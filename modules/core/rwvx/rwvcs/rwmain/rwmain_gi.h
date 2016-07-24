
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __rwmain_gi_h__
#define __rwmain_gi_h__

#include <rw_sklist.h>
#include <rwvcs_defs.h>
#include <rwvx.h>
#include <rwtasklet.h>

__BEGIN_DECLS

#ifndef __GI_SCANNER__
struct cpu_usage {
  struct {
    double user;
    double sys;
    double idle;
  } current;

  struct {
    unsigned long user;
    unsigned long sys;
    unsigned long idle;
    unsigned long total;
  } previous;
};

struct rwmain_gi {
  int _refcnt;

  char * component_name;
  rw_component_type component_type;
  uint32_t instance_id;
  char * parent_id;
  char * vm_ip_address;
  uint32_t vm_instance_id;

  rwdts_api_t * dts;
  uint32_t vstart_corr_id;

  rwtasklet_info_ptr_t tasklet_info;
  rwvx_instance_ptr_t rwvx;

  time_t start_sec;

  struct {
    double last_proc_systime;
    double last_proc_cputime;
    double proc_cpu_usage;

    struct cpu_usage all_cpus;
    struct cpu_usage ** each_cpu;
  } sys;

  struct serf_context * serf;
  struct serf_event_callback * serf_handler;

  struct rwproc_heartbeat * rwproc_heartbeat;

  rwdts_kv_handle_t *redis_handle;

  // List of native processes launched by this vcs instance.  Used
  // to track the processes via a CFSource.
  rw_sklist_t procs;

  // List of tasklets launched by this vcs instance.
  rw_sklist_t tasklets;

  // List of muti_vms launched by this vcs instance.
  rw_sklist_t multivms;

};
#endif
typedef struct rwmain_gi rwmain_gi_t;

GType rwmain_gi_get_type(void);

///@cond GI_SCANNER
/**
 * rwmain_gi_new:
 * @manifest_box: (type RwManifestYang.Manifest) (transfer none)
 */
///@endcond
rwmain_gi_t * rwmain_gi_new(rwpb_gi_RwManifest_Manifest * manifest_box);

///@cond GI_SCANNER
/*
 * rwmain_gi_add_tasklet:
 * @rwmain_gi:
 * @plugin_dir:
 * @plugin_name:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
///@endcond
rw_status_t rwmain_gi_add_tasklet(
  rwmain_gi_t * rwmain_gi,
  const char * plugin_dir,
  const char * plugin_name);

///@cond GI_SCANNER
/**
 * rwmain_gi_get_tasklet:
 * returns: (type RwTasklet.Info) (transfer none)
 */
///@endcond
rwtasklet_info_t * rwmain_gi_get_tasklet_info(rwmain_gi_t * rwmain_gi);

///@cond GI_SCANNER
/**
 * rwmain_gi_find_taskletinfo_by_name:
 * returns: (type RwTasklet.Info) (transfer none)
 */
///@endcond
rwtasklet_info_t * rwmain_gi_find_taskletinfo_by_name(rwmain_gi_t* rwmain,
                                                  const char * tasklet_instance);
///@cond GI_SCANNER
/**
 * rwmain_gi_new_tasklet:
 * returns: (type RwTasklet.Info) (transfer none)
 */
///@endcond
rwtasklet_info_t * rwmain_gi_new_tasklet_info(
    rwmain_gi_t * rwmain_gi,
    const char * name,
    uint32_t id);



rw_status_t update_zk(struct rwmain_gi * rwmain);
rw_status_t reaper_start(rwvcs_instance_ptr_t instance, const char * instance_name);
rw_status_t rwmain_notify_transition (struct rwmain_gi * rwmain, vcs_vm_state state);

__END_DECLS
#endif
