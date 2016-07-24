
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*!
 * @file rwtasklet.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 12/04/2013
 * @brief Top level API include for RW.Tasklet component
 */

#ifndef __RWTASKLET_H__
#define __RWTASKLET_H__

#include <glib.h>
#include <glib-object.h>
#include "rw_vx_plugin.h"
#include "rw_cf_type_validate.h"
#include "rwtrace.h"
#include "rwlog.h"
#include "rwsched.h"
#include "rwsched_main.h"
#include "rwsched_internal.h"
#include "rwmsg.h"
#include "rwcal_vars_api.h"
#include "rwvcs_defs.h"
#include "rw_keyspec.h"
#include "rw-manifest.pb-c.h"

__BEGIN_DECLS

typedef struct rwmain_tasklet_mode_active_s {
  bool has_mode_active;
  bool mode_active;
} rwmain_tasklet_mode_active_t;

#ifndef __GI_SCANNER__
/* This is a less void-y C version of RWTasklet::_RWTaskletInfo over
   in rwtasklet_plugin.vala. */
struct rwtasklet_info_s {
  int ref_cnt;
  rwsched_instance_t *rwsched_instance;
  rwsched_tasklet_t *rwsched_tasklet_info;
  rwtrace_ctx_t *rwtrace_instance;
  struct rwdts_api_s *apih;
  rwlog_ctx_t *rwlog_instance;
  rwmsg_endpoint_t *rwmsg_endpoint;
  struct rwvcs_instance_s *rwvcs;
  struct rwvx_instance_s *rwvx;
  data_store_type data_store;
  char *vm_ip_address;
  struct {
    int rwtasklet_instance_id;
    char *rwtasklet_name;
  } identity;
  rwmain_tasklet_mode_active_t mode;
  

  /* Any changes here should match in rwtasklet_plugin.vala and the 
   * rwtasklet_info_new function below */
};
typedef struct rwtasklet_info_s *rwtasklet_info_ptr_t;
#endif

typedef struct rwtasklet_info_s rwtasklet_info_t;
GType rwtasklet_info_get_type(void);

rwtasklet_info_t *rwtasklet_info_ref(rwtasklet_info_t *boxed);
void rwtasklet_info_unref(rwtasklet_info_t *boxed);

///@cond GI_SCANNER
/**
 * rwtasklet_info_get_rwsched_instance:
 * Returns: (type RwSched.Instance) (transfer none)
 */
///@endcond
rwsched_instance_t *rwtasklet_info_get_rwsched_instance(rwtasklet_info_t *tasklet_info);

///@cond GI_SCANNER
/**
 * rwtasklet_info_get_rwsched_tasklet:
 * Returns: (type RwSched.Tasklet) (transfer none)
 */
///@endcond
//
rwsched_tasklet_t *rwtasklet_info_get_rwsched_tasklet(rwtasklet_info_t *tasklet_info);

///@cond GI_SCANNER
/**
 * rwtasklet_info_get_rwlog_ctx:
 * Returns: (type RwLog.Ctx) (transfer none)
 */
///@endcond
rwlog_ctx_t *rwtasklet_info_get_rwlog_ctx(rwtasklet_info_t *tasklet_info);

///@cond GI_SCANNER
/**
 * rwtasklet_info_get_pb_manifest:
 * Returns: (type RwManifestYang.Manifest) (transfer full)
 */
///@endcond
rwpb_gi_RwManifest_Manifest* rwtasklet_info_get_pb_manifest(rwtasklet_info_t *tasklet_info);

char *rwtasklet_info_get_instance_name(rwtasklet_info_t *tasklet_info);

char *rwtasklet_info_get_parent_vm_instance_name(rwtasklet_info_t *tasklet_info);

char *rwtasklet_info_get_parent_vm_parent_instance_name(rwtasklet_info_t *tasklet_info);

int rwtasklet_info_is_collapse_vm(rwtasklet_info_t *tasklet_info);

int rwtasklet_info_is_collapse_process(rwtasklet_info_t *tasklet_info);

int rwtasklet_info_is_collapse_thread(rwtasklet_info_t *tasklet_info);

vcs_manifest * rwtasklet_info_get_manifest(rwtasklet_info_t *tasklet_info);

vcs_manifest_rwmsg *rwtasklet_get_manifest_rwmsg(vcs_manifest *rwmanifest);

/*!
 * Find all local neighbors that match the given critera.  Neighbors are defined as
 * components that are descendents of the node node at the specified depth higher in
 * the hierarchy.  The starting point will be included in the results if it passes
 * the test function.
 *
 * @param tasklet_info  - tasklet info of the tasklet
 * @param test          - function called on each neighbor to test if it should be
 *                        included in the results.
 * @param depth         - how much higher in the tree to begin from the starting
 *                        component.  For instance, a depth of 2 will search every
 *                        descendent of the grandparent.
 * @param neighbors     - on success, a null-terminated list of the instances names
 *                        of all neighbors which passed the test function.
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if the lookup fails for any component
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwtasklet_info_get_neighbors(rwtasklet_info_t *tasklet_info, 
                             char *comparestr, 
                             uint32_t depth, 
                             char *** neighbors);

/*!
 * Get the vnf-name for a particular tasklet.
 *
 * @param tasklet_info
 *
 * @return NULL if failed
 *         >0 if success
 */
char*
rwtasklet_info_get_vnfname(rwtasklet_info_t * tasklet_info);


/*!
 * Get the colony id for a particular tasklet.
 *
 * @param vcs instance
 * @param instance-id
 * @param instance-name
 *
 * @return 0 if failed
 *         >0 if success
 */
uint32_t
rwtasklet_info_get_clusterid(struct rwvcs_instance_s* rwvcs,
                             uint32_t instance_id, char *name);

/*!
 * Get the cluster name for a particular tasklet.
 *
 * @param tasklet info instance
 *
 * @return 0 if failed
 *         cluster_name if success
 */
char *
rwtasklet_info_get_cluster_name(rwtasklet_info_t *tasklet_info);

/*!
 * Get the cluster instance name for a particular tasklet.
 *
 * @param tasklet info instance
 *
 * @return 0 if failed
 *         cluster_instance_name if success
 */
char *
rwtasklet_info_get_cluster_instance_name(rwtasklet_info_t *tasklet_info);


/*!
 * Get the cluster id for a particular tasklet.
 *
 * @param vcs instance
 * @param instance-id
 * @param instance-name
 *
 * @return 0 if failed
 *         >0 if success
 */
uint32_t
rwtasklet_info_get_colonyid(struct rwvcs_instance_s* rwvcs,
                            uint32_t instance_id, char *name);

/*!
 * Get the local fastpath tasklet id
 *
 * @param tasklet_info  - tasklet info of the tasklet
 *
 * @return 0 if failed
 *         >0 if success
 */
uint32_t
rwtasklet_info_get_local_fpathid(rwtasklet_info_t *tasklet_info);

/*!
 * Get the first tasklet  tasklet id with this name
 *
 * @param tasklet_info  - tasklet info of the tasklet
 *
 * @return 0 if failed
 *         >0 if success
 */
uint32_t
rwtasklet_info_get_tasklet_id_by_name_in_collection(rwtasklet_info_t *tasklet_info,
                                                    char *name);

__END_DECLS


#endif // __RWTASKLET_H__
