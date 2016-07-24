
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWVCS_H__
#define __RWVCS_H__

#include <rwlib.h>
#include <rwpython_util.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rw_cf_type_validate.h>

#include "rwvx.h"
#include "rw-base.pb-c.h"

#include "rwvcs_defs.h"

__BEGIN_DECLS

#if 0
#define NEW_DBG_PRINTS(args...) {\
  fprintf(stderr, "NEW_DBG_PRINTS %s:%d[[%s]]", __FILE__, __LINE__, __func__); \
  fprintf(stderr, args); \
}
#else
#define NEW_DBG_PRINTS(args...) do {} while(0);
#endif

#define RWVCS_RWZK_DISPATCH(t_mode, t_sched, t_rwq, t_ptr, t_fn) { \
  if (t_rwq) { \
    rwsched_dispatch_##t_mode##_f (t_sched, t_rwq, (void *)(t_ptr), t_fn); \
  }\
  else { \
    t_fn((void *)(t_ptr));\
  }\
}

#define RWVCS_LATENCY_CHK_PRE(sched_inst) \
{ /* Need to be with RWVCS_LATENCY_CHK_POST to complete { block*/ \
  int repthresh = rwsched_instance_get_latency_check(sched_inst); \
  struct timeval tv_begin, tv_end, tv_delta; \
  /*repthresh = 10;*/ \
  if (repthresh) { \
    gettimeofday(&tv_begin, NULL); \
  } \
  else { \
    tv_begin.tv_sec = 0; \
  }

#define RWVCS_LATENCY_CHK_POST(trace_inst, trace_category, proc_ptr, args_va...) \
  if (tv_begin.tv_sec && repthresh) { \
    gettimeofday(&tv_end, NULL); \
    timersub(&tv_end, &tv_begin, &tv_delta); \
    unsigned int cbms = (tv_delta.tv_sec * 1000 + tv_delta.tv_usec / 1000); \
    if (cbms >= repthresh && trace_inst) { \
      char *name = rw_btrace_get_proc_name(proc_ptr); \
      char out_str[1024] = { 0 }; \
      int len; \
      len = snprintf(out_str, 1024, "callback %s took %ums ", \
              name, cbms); \
      free(name); \
      len = snprintf((out_str + len), 1024 - len, args_va);  \
      RWTRACE_CRIT((trace_inst), trace_category, "%s", out_str); \
    } \
  } \
} /* Terminates { from RWVCS_LATENCY_CHK_PRE */



#define RWTRACE_CATEGORY_RWVCS RWTRACE_CATEGORY_RWTASKLET
#define RWMAIN_EXEFILE_PATHLEN 1024
#define RWVCS_VMPOOL_NAME_LEN 256
#define RWVCS_VM_NAME_LEN 256

struct rwvcs_identity_s {
  // Convenience cache of settings passed on the command line or otherwise discovered
  // by rwmain during initialization.  During normal runtime, the values stored here
  // should be accessed by using the various rwvcs_rwzk_* calls to talk to the permanent
  // and up to date source.  However, that is not always possible during initialization as
  // the zookeeper may not have been written to yet.
  //
  // These values are gathered by rwmain by looking at either the manifest or by inspecting
  // the command line arguments passed to rwmain.  This means that it is completely possible
  // that the value you're interested in may not have been set.  Be sure to check first.
  //
  // NOTE:  These should only be accessed during initialization functions.  During runtime
  // use the zookeeper.

  // Instance id of the current VM.  0 if unknown.
  uint32_t rwvm_instance_id;

  // IP address of the current VM.  NULL if unknown.
  char * vm_ip_address;

  // Instance name of nearest VM (for VM this is the same as RWVM instance_name, RWPROC
  // would have this as its parent VM name
  char * rwvm_name;
};

typedef enum {
  RWCAL_VMPOOL_NONE,
  RWCAL_VMPOOL_STATIC_LIST,
  RWCAL_VMPOOL_STATIC_RANGE,
} rwcal_vmpool_type_t;

typedef struct rwvcs_vmpool_entry {
  rw_sklist_element_t      element; /**skiplist element*/
  uint32_t                 local_flags; /**< These are the local flags*/
  rwcal_vmpool_type_t      pool_type; /** type of vm pool*/
  char                     vmpoolname[RWVCS_VMPOOL_NAME_LEN]; /** VM pool name in the config*/
  union {
    rw_sklist_t              vmpool_ip_slist;/**<vm pool  ip list with name as the key*/
    struct {
      char                   base_name[RWVCS_VM_NAME_LEN];
      uint32_t               low_index;
      uint32_t               high_index;
      uint32_t               curr_index;
      rw_ip_addr_t           low_addr;
      rw_ip_addr_t           high_addr;
    } vm_range;
  } u;
} rwvcs_vmpool_entry_t;

typedef struct rwvcs_vmip_entry{
  rw_sklist_element_t      vm_element; /**skiplist element*/
  uint32_t                 local_flags; /**< These are the local flags*/
  char                     vmname[RWVCS_VM_NAME_LEN]; /** VM name in the config*/
  rw_ip_addr_t             vm_ip_addr;
}rwvcs_vmip_entry_t;

typedef struct rwvcs_config_ready_entry_s{
  rw_sklist_element_t      config_ready_elem; /**skiplist element*/
  char                     *instance_name;
  char                     *childn_xpath;
  char                     *xpath;
  void                     *regh;
} rwvcs_config_ready_entry_t;

#define RWVCS_ZK_SERVER_CLUSTER 3
#define RWVCS_ZK_SERVER_QOUROM_CNT ((RWVCS_ZK_SERVER_CLUSTER/2) + 1)
typedef struct rwvcs_mgmt_info_s {
  rwcal_zk_server_port_detail_ptr_t zk_server_port_details[RWVCS_ZK_SERVER_CLUSTER + 1]; // for NULL terminated list
  uint32_t  unique_ports:1;
  uint32_t  mgmt_vm:1;
  uint32_t  config_start_zk_pending:1;
  vcs_vm_state state;
} rwvcs_mgmt_info_t;

#define RWVCS_MGMT_VM(t_rwvcs) \
    (((t_rwvcs)->mgmt_info.state == RWVCS_TYPES_VM_STATE_MGMTACTIVE) \
     || (t_rwvcs)->mgmt_info.state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY)


struct rwvcs_instance_s {
  CFRuntimeBase _base;
  struct rwvx_instance_s *rwvx;
  int (*rwmain_f)(int argc, char ** argv, char ** envp);
  void (*rwcrash)();
  char *rwmain_exefile;
  char *rwmanifest_xmlfile;
  vcs_manifest *pb_rwmanifest;
  struct rwvcs_identity_s identity;

  rw_vx_framework_t * rwvx_instance;
  rw_vx_modinst_common_t * mip;

  rw_sklist_t    vmpool_slist;/**<vm pool list with name as the key*/
  rw_xml_manager_t * xml_mgr;

  // Environment pointer
  char ** envp;

  struct {
    rwpython_utilApi * cls;
    rwpython_utilApiIface * iface;
  } rwpython_util;

  char * reaper_path;
  int reaper_sock;

  bool heartbeatmon_enabled;
  bool restart_inprogress;

  // If LD_PRELOAD is set (in order to do malloc accounting) then it will
  // be stored here and unset from the environment.  Later execs of rwmain
  // will reintroduce the variable.  This is done so that native processes
  // and anything else being forked does not inherit the setting.  It is
  // only for rwmain (which collectively contain all tasklets).
  char * ld_preload;
  rwsched_dispatch_queue_t zk_rwq;
  struct rwtasklet_info_s * tasklet_info;
  rw_sklist_t config_ready_entries;
  char *vcs_instance_xpath;
  char *instance_name;
  void *vcs_instance_regh;
  void *apih;
  rw_status_t (*config_ready_fn)(rwvcs_instance_ptr_t, rw_component_info*);
  rwvcs_mgmt_info_t mgmt_info;
};

RW_TYPE_DECL(rwvcs_instance);
RW_CF_TYPE_EXTERN(rwvcs_instance_ptr_t)

/*
 * Allocate a rwvcs instance.
 *
 * @return                - rwvcs instance or NULL on failure.
 */
rwvcs_instance_ptr_t rwvcs_instance_alloc();

/*
 * Free a rwvcs instance.
 */
void rwvcs_instance_free(rwvcs_instance_ptr_t rwvcs);

/*
 * Initialize a rwvcs instance
 *
 * @param rwvcs         - instance to initialize
 * @param manifest_file - manifest file with path
 * @param ip_address    - ip address as a string
 * @param rwmain        - function to use for later rwmain instances in collapsed mode
 * @return              - rw_status_t
 */
rw_status_t rwvcs_instance_init(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_file,
    const char * ip_address,
    int (*rwmain)(int argc, char ** argv, char ** envp));

/*
 * Process input manifest file and store in rwvcs instance
 *
 * @param rwvcs         - instance to initialize
 * @param manifest_file - manifest file with path
 * @return              - rw_status_t
 */
rw_status_t rwvcs_process_manifest_file(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_file);

/*
 * Start zoo keeper server from manifest
 *
 * @param rwvcs         - instance to initialize
 * @param bootstrap     - bootstrap portion of manifest
 * @return              - rw_status_t
 */
rw_status_t start_zookeeper_server(
    rwvcs_instance_ptr_t rwvcs,
    vcs_manifest_bootstrap * bootstrap);

  __END_DECLS
#endif // __RWVCS_H__
