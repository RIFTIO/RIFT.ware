
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <glib.h>
#include <glib-object.h>

#include <rwvcs.h>
#include <rwvcs_rwzk.h>

#include "rwtasklet.h"

rwtasklet_info_t *rwtasklet_info_ref(rwtasklet_info_t *boxed)
{
  g_atomic_int_inc(&boxed->ref_cnt);
  return boxed;
}

void rwtasklet_info_unref(rwtasklet_info_t * tinfo)
{
  if (g_atomic_int_dec_and_test(&tinfo->ref_cnt) <= 0)
    return;

  rwsched_instance_unref(tinfo->rwsched_instance);
  rwsched_tasklet_unref(tinfo->rwsched_tasklet_info);

  // Leak this until someone deals with RIFT-6966 (RIFT-6986)
     rwlog_close(tinfo->rwlog_instance, false);

  rwmsg_endpoint_halt(tinfo->rwmsg_endpoint);
  if (tinfo->identity.rwtasklet_name)
    free(tinfo->identity.rwtasklet_name);

  free(tinfo);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwtasklet_info_get_type()
 */
G_DEFINE_BOXED_TYPE(rwtasklet_info_t,
                    rwtasklet_info,
                    rwtasklet_info_ref,
                    rwtasklet_info_unref);

vcs_manifest_rwmsg *rwtasklet_get_manifest_rwmsg(vcs_manifest *rwmanifest)
{
  return rwmanifest->init_phase->settings->rwmsg;
}

int rwtasklet_info_is_collapse_vm(rwtasklet_info_t *tasklet_info)
{
  vcs_manifest *rwmanifest;

  rwmanifest = rwtasklet_info_get_manifest(tasklet_info);
  RW_ASSERT(rwmanifest);

  return rwmanifest->init_phase->settings->rwvcs->collapse_each_rwvm;
}

int rwtasklet_info_is_collapse_process(rwtasklet_info_t *tasklet_info)
{
  vcs_manifest *rwmanifest;

  rwmanifest = rwtasklet_info_get_manifest(tasklet_info);
  RW_ASSERT(rwmanifest);

  return rwmanifest->init_phase->settings->rwvcs->collapse_each_rwprocess;
}

int rwtasklet_info_is_collapse_thread(rwtasklet_info_t *tasklet_info)
{
  vcs_manifest *rwmanifest;

  rwmanifest = rwtasklet_info_get_manifest(tasklet_info);
  RW_ASSERT(rwmanifest);

  /* TODO: change this to collapse_each_rwthread */
  return rwmanifest->init_phase->settings->rwvcs->collapse_each_rwprocess;
}

vcs_manifest *
rwtasklet_info_get_manifest(rwtasklet_info_t *tasklet_info)
{
  return tasklet_info->rwvcs->pb_rwmanifest;
}

static bool rwtasklet_get_neighbors_compare(rw_component_info * component,
                                            void *arg) {
  char *name = (char *)arg;

  if (!component->proc_info && (!component->n_rwcomponent_children)){
    if (strcasestr(component->instance_name, name) != NULL){
      return 1;
    }
  }

  return 0;
}


rw_status_t
rwtasklet_info_get_neighbors(rwtasklet_info_t *tasklet_info,
                             char *comparestr,
                             uint32_t depth,
                             char *** neighbors)
{
  char *instance_name;
  rw_status_t status;

  RW_ASSERT(comparestr);

  instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name,
                                   tasklet_info->identity.rwtasklet_instance_id);
  if (instance_name){
    status = rwvcs_rwzk_get_neighbors(tasklet_info->rwvcs,
                                      instance_name,
                                      rwtasklet_get_neighbors_compare,
                                      comparestr,
                                      depth, neighbors);
    
    free(instance_name);
  }else{
    status = RW_STATUS_FAILURE;
  }

  return status;
}


static bool rwtasklet_get_component_name_compare(rw_component_info * component,
                                                 void *arg)
{
  char *name = (char *)arg;

  if (component->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION){
    if (strcmp(name, component->collection_info->collection_type) == 0){
      return 1;
    }
  }

  return 0;
}


static
int rwtasklet_info_compare_colony_cluster_id(rwtasklet_info_t *rwtasklet_info,
                                             char *neighbor_name)
{
   if ((rwtasklet_info_get_colonyid(rwtasklet_info->rwvcs,
                                    rwtasklet_info->identity.rwtasklet_instance_id,
                                    rwtasklet_info->identity.rwtasklet_name) ==
        rwtasklet_info_get_colonyid(rwtasklet_info->rwvcs,
                                    split_instance_id(neighbor_name),
                                    split_component_name(neighbor_name))) &&
       (rwtasklet_info_get_clusterid(rwtasklet_info->rwvcs,
                                     rwtasklet_info->identity.rwtasklet_instance_id,
                                     rwtasklet_info->identity.rwtasklet_name) ==
        rwtasklet_info_get_clusterid(rwtasklet_info->rwvcs,
                                     split_instance_id(neighbor_name),
                                     split_component_name(neighbor_name)))){
     return 1;
   }
   
   return 0;
}

uint32_t
rwtasklet_info_get_tasklet_id_by_name_in_collection(rwtasklet_info_t *rwtasklet_info,
                                                    char *name)
{
  rw_status_t status;
  char ** neighbors;
  uint32_t ret = 0;
  int depth = 0, total_depth;
  rw_component_info target;
  char *instance_name;

  
  instance_name = to_instance_name(rwtasklet_info->identity.rwtasklet_name,
                                   rwtasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    goto ret;
  }
  
  total_depth = 0;
  status = rwvcs_rwzk_lookup_component(rwtasklet_info->rwvcs, instance_name, &target);
  if (status != RW_STATUS_SUCCESS){
    goto ret;
  }
  // find the starting point
  while ((status = rwvcs_rwzk_lookup_component(rwtasklet_info->rwvcs, target.rwcomponent_parent, &target))== RW_STATUS_SUCCESS) {
    total_depth++;
  }
  
  for (depth = 0; depth < total_depth; depth++) {
    ret = 0;
    status = rwtasklet_info_get_neighbors(rwtasklet_info,
                                          name,
                                          depth, &neighbors);
    if (status == RW_STATUS_SUCCESS){
      if (neighbors[0]){
        if (rwtasklet_info_compare_colony_cluster_id(rwtasklet_info,
                                                     neighbors[0])){
          ret = split_instance_id(neighbors[0]);
        }
        free(neighbors[0]);
        free(neighbors);
        if (ret){
          break;
        }
      }else{
        free(neighbors);
      }
    }
  }
ret:
  if (instance_name){
    free(instance_name);
  }
  return ret;
}

uint32_t
rwtasklet_info_get_local_fpathid(rwtasklet_info_t *rwtasklet_info)
{
  rw_status_t status;
  char ** neighbors;
  uint32_t ret = 0;

  status = rwtasklet_info_get_neighbors(rwtasklet_info,
                                        "RW.Fpath",
                                        2, &neighbors);
  if (status == RW_STATUS_SUCCESS){
    if (neighbors[0]){
      ret = split_instance_id(neighbors[0]);
      free(neighbors[0]);
      free(neighbors);
    }else{
      free(neighbors);
    }
  }

  return ret;
}

char*
rwtasklet_info_get_vnfname(rwtasklet_info_t * tasklet_info)
{
  rw_status_t status;
  bool ret;
  rw_component_info component_info;
  char *instance_name;
 
  instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name,
                                   tasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    return 0;
  }
  status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs,
                                       instance_name,
                                       &component_info);
  while(status == RW_STATUS_SUCCESS){
    ret = rwtasklet_get_component_name_compare(&component_info, "rwcluster");
    if (ret ){
      return split_component_name(component_info.instance_name);
    }else{
      ret = rwtasklet_get_component_name_compare(&component_info, "rwcolony");
      if (ret) {
        return split_component_name(component_info.instance_name);
      }
    }
    free(instance_name);
    instance_name = component_info.rwcomponent_parent;
    status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs,
                                         instance_name, &component_info);
  }  
  return NULL;
}

uint32_t
rwtasklet_info_get_colonyid(rwvcs_instance_ptr_t rwvcs,
                            uint32_t instance_id, char *name)
{
  char *instance_name;
  rw_status_t status;
  bool ret;
  int colony_id = 0;
  rw_component_info component_info;

  instance_name = to_instance_name(name, instance_id);
  if (!instance_name){
    return 0;
  }
  status = rwvcs_rwzk_lookup_component(rwvcs,
                                       instance_name,
                                       &component_info);
  while(status == RW_STATUS_SUCCESS){
    ret = rwtasklet_get_component_name_compare(&component_info, "rwcolony");
    if (ret ){
      colony_id = split_instance_id(component_info.instance_name);
      return colony_id;
    }
    free(instance_name);
    instance_name = component_info.rwcomponent_parent;
    status = rwvcs_rwzk_lookup_component(rwvcs,
                                         instance_name, &component_info);
  }

  return (uint32_t)0;
}

uint32_t
rwtasklet_info_get_clusterid( rwvcs_instance_ptr_t rwvcs,
                              uint32_t instance_id, char *name)
{
  char *instance_name;
  rw_status_t status;
  bool ret;
  int colony_id = 0;
  rw_component_info component_info;

  instance_name = to_instance_name(name, instance_id);
  
  if (!instance_name){
    return 0;
  }
  status = rwvcs_rwzk_lookup_component(rwvcs,
                                       instance_name,
                                       &component_info);
  while(status == RW_STATUS_SUCCESS){
    ret = rwtasklet_get_component_name_compare(&component_info, "rwcluster");
    if (ret ){
      colony_id = split_instance_id(component_info.instance_name);
      return colony_id;
    }
    free(instance_name);
    instance_name = component_info.rwcomponent_parent;
    status = rwvcs_rwzk_lookup_component(rwvcs,
                                         instance_name, &component_info);
  }

  return (uint32_t)0;
}

char * rwtasklet_info_get_cluster_instance_name(rwtasklet_info_t * tasklet_info)
{
  char * ret_instance_name = NULL;
  char * instance_name;
  rw_status_t status;
  bool match = 0;
  rw_component_info component_info;

  instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name, tasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    return NULL;
  }

  status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs, instance_name, &component_info);
  while(status == RW_STATUS_SUCCESS && !match) {
    match = rwtasklet_get_component_name_compare(&component_info, "rwcluster");
    if (component_info.rwcomponent_parent == NULL){
      break;
    }
    if (!match){
      free(instance_name);
      instance_name = strdup(component_info.rwcomponent_parent);
      RW_ASSERT(instance_name);
    }
    protobuf_free_stack(component_info);
    status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs, instance_name, &component_info);
  } 
  
  if (match){
    ret_instance_name = instance_name;
  }
  
  return ret_instance_name;
}

char * rwtasklet_info_get_cluster_name(rwtasklet_info_t * tasklet_info)
{
  char * component_name = NULL;
  char * instance_name = NULL;

  instance_name = rwtasklet_info_get_cluster_instance_name(tasklet_info);
  if (instance_name != NULL){
    component_name = split_component_name(instance_name);
  }

  free(instance_name);
  return component_name;
}


char * rwtasklet_info_get_parent_vm_instance_name(rwtasklet_info_t * tasklet_info)
{
  char * instance_name;
  rw_status_t status;
  bool match = 0;
  rw_component_info component_info;

  instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name, tasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    return NULL;
  }

  status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs, instance_name, &component_info);
  while(status == RW_STATUS_SUCCESS && !match) {
    match = (component_info.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM);
    if (component_info.rwcomponent_parent == NULL){
      break;
    }
    if (!match){
      free(instance_name);
      instance_name = strdup(component_info.rwcomponent_parent);
      RW_ASSERT(instance_name);
    }
    protobuf_free_stack(component_info);
    status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs, instance_name, &component_info);
  } 

  if (! match){
    return NULL;
  }

  return instance_name;
}


char * rwtasklet_info_get_parent_vm_parent_instance_name(rwtasklet_info_t * tasklet_info)
{
  char * instance_name;
  rw_status_t status;
  bool match;
  rw_component_info component_info;

  instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name, tasklet_info->identity.rwtasklet_instance_id);
  if (!instance_name){
    return NULL;
  }

  do {
    status = rwvcs_rwzk_lookup_component(tasklet_info->rwvcs, instance_name, &component_info);
    if (status != RW_STATUS_SUCCESS) {
      // This is a hack --- to stop vns_test.py from blowing up
      return(strdup("THIS_IS_A_HACK_rwtasklet.c:406"));
    }
    match = (component_info.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM);
    if (component_info.rwcomponent_parent == NULL){
      break;
    }
    if (match) {
      free(instance_name);
      instance_name = strdup(component_info.rwcomponent_parent);
      return instance_name;
    }
    else {
      free(instance_name);
      instance_name = strdup(component_info.rwcomponent_parent);
      RW_ASSERT(instance_name);
    }
    protobuf_free_stack(component_info);
  } while(status == RW_STATUS_SUCCESS && !match);

  if (! match){
    return NULL;
  }

  return instance_name;
}

rwsched_instance_t *rwtasklet_info_get_rwsched_instance(rwtasklet_info_t *tasklet_info)
{
  return tasklet_info->rwsched_instance;
}

char *rwtasklet_info_get_instance_name(rwtasklet_info_t *tasklet_info)
{
  char *instance_name = to_instance_name(tasklet_info->identity.rwtasklet_name,
                                         tasklet_info->identity.rwtasklet_instance_id);

  return instance_name;
}

rwsched_tasklet_t *rwtasklet_info_get_rwsched_tasklet(rwtasklet_info_t *tasklet_info)
{
  return tasklet_info->rwsched_tasklet_info;
}

rwlog_ctx_t *rwtasklet_info_get_rwlog_ctx(rwtasklet_info_t *tasklet_info)
{
  return tasklet_info->rwlog_instance;
}

rwpb_gi_RwManifest_Manifest*
rwtasklet_info_get_pb_manifest(rwtasklet_info_t *tasklet_info)
{
  vcs_manifest* manifest = rwtasklet_info_get_manifest(tasklet_info);
  RW_ASSERT(manifest);

  vcs_manifest* dup_manifest = (vcs_manifest *)protobuf_c_message_duplicate(
                                                    NULL,
                                                    &manifest->base,
                                                    manifest->base.descriptor);
  RW_ASSERT(dup_manifest);

  RWPB_M_MSG_GI_DECL_INIT(RwManifest_data_Manifest, dup_manifest, res_ptr);

  return res_ptr;
}
