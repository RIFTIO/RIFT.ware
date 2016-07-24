

/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwvcs_internal.h
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 07/07/2014
 * @brief RW.VCS common include header file
 */

#ifndef __RWVCS_INTERNAL_H__
#define __RWVCS_INTERNAL_H__

#include <rw-base.pb-c.h>

#include "rwvcs.h"
#include "rwvcs_component.h"
#include "rwvx.h"

__BEGIN_DECLS

/*
 * Start the zookeeper server.
 *
 * @param rwvcs         - rwvcs instance
 * @param server_id     - server id to be started
 * @param client_port   - client port of the server
 * @param unique_ports  - generate port number based on UID.
 * @param server_details - NULL terminated list of zookeeper server port details
 * @return              - pid of started zookeeper server process
 */
int
rwvcs_rwzk_server_start(
    rwvcs_instance_ptr_t rwvcs,
    uint32_t server_id,
    int client_port,
    bool unique_ports,
    rwcal_zk_server_port_detail_ptr_t *server_details);

rw_status_t
rwvcs_rwzk_node_create(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component);

typedef struct rwvcs_rwzk_lookup_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_id;
  rw_component_info * pb_component;
  rw_status_t status;
} rwvcs_rwzk_lookup_sync_t;

typedef struct rwvcs_rwzk_get_neighbors_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  bool (*test)(rw_component_info *, void *userdata);
  void * userdata;
  uint32_t height;
  char *** neighbors;
  rw_status_t status;
} rwvcs_rwzk_get_neighbors_sync_t;

typedef struct rwvcs_rwzk_next_instance_id_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  uint32_t *instance_id;
  const char *instance_path;
  rw_status_t status;
} rwvcs_rwzk_next_instance_id_sync_t;

typedef struct rwvcs_rwzk_node_exists_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * id;
  bool ret;
} rwvcs_rwzk_node_exists_sync_t;

typedef struct rwvcs_rwzk_update_state_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  rw_component_state state;
} rwvcs_rwzk_update_state_t;

typedef struct rwvcs_rwzk_node_update_s {
  rwvcs_instance_ptr_t rwvcs;
  rw_component_info    *id;
  bool skip_publish;
} rwvcs_rwzk_node_update_t;

typedef struct rwvcs_rwzk_update_parent_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  const char * new_parent;
  rw_status_t status;
} rwvcs_rwzk_update_parent_sync_t;

typedef struct rwvcs_rwzk_lock_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * id;
  struct timeval * timeout;
  rw_status_t status;
} rwvcs_rwzk_lock_sync_t;

typedef struct rwvcs_rwzk_unlock_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * id;
  rw_status_t status;
} rwvcs_rwzk_unlock_sync_t;

typedef struct rwvcs_rwzk_nearest_leader_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  char ** leader_id;
  rw_status_t status;
} rwvcs_rwzk_nearest_leader_sync_t;

typedef struct rwvcs_rwzk_descendents_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  rw_component_info *** components;
  rw_status_t status;
} rwvcs_rwzk_descendents_sync_t;

typedef struct rwvcs_rwzk_add_child_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * parent_instance;
  const char * child_instance;
  rw_status_t status;
} rwvcs_rwzk_add_child_sync_t;

typedef struct rwvcs_rwzk_is_leader_of_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  const char * target;
  bool ret;
} rwvcs_rwzk_is_leader_of_sync_t;

typedef struct rwvcs_rwzk_responsible_for_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  const char * target_instance;
  bool responsible;
} rwvcs_rwzk_responsible_for_sync_t;

typedef struct rwvcs_rwzk_node_create_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  rw_status_t status;
} rwvcs_rwzk_node_create_sync_t;

typedef struct rwvcs_rwzk_delete_child_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  const char * child_name;
  int child_id;
  rw_status_t status;
} rwvcs_rwzk_delete_child_sync_t;

typedef struct rwvcs_rwzk_delete_node_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  rw_status_t status;
} rwvcs_rwzk_delete_node_sync_t;

typedef struct rwvcs_rwzk_update_config_ready_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  bool         config_ready;
  vcs_recovery_type recovery_action;
  rw_status_t status;
} rwvcs_rwzk_update_config_ready_sync_t;

typedef struct rwvcs_rwzk_watcher_start_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  rwsched_tasklet_ptr_t    sched_tasklet_ptr;
  rwsched_dispatch_queue_t rwq;
  void (*cb)(void* ud);
  void *ud;
  rwcal_closure_ptr_t closure;
} rwvcs_rwzk_watcher_start_sync_t;

typedef struct rwvcs_rwzk_get_children_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  char ***children;
  rw_status_t status;
} rwvcs_rwzk_get_children_sync_t;

typedef struct rwvcs_rwzk_get_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  char ** data;
  rw_status_t status;
} rwvcs_rwzk_get_sync_t;

typedef struct rwvcs_rwzk_set_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  const char * data;
  rw_status_t status;
} rwvcs_rwzk_set_sync_t;

typedef struct rwvcs_rwzk_create_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  rw_status_t status;
} rwvcs_rwzk_create_sync_t;

typedef struct rwvcs_rwzk_exists_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  bool         exists;
} rwvcs_rwzk_exists_sync_t;

typedef struct rwvcs_rwzk_lock_path_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  struct timeval * timeout;
  rw_status_t status;
} rwvcs_rwzk_lock_path_sync_t;

typedef struct rwvcs_rwzk_unlock_path_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  rw_status_t status;
} rwvcs_rwzk_unlock_path_sync_t;

typedef struct rwvcs_rwzk_delete_path_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  rw_status_t status;
} rwvcs_rwzk_delete_path_sync_t;

typedef struct rwvcs_rwzk_watcher_stop_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * path;
  rwcal_closure_ptr_t *closure;
  rw_status_t status;
} rwvcs_rwzk_watcher_stop_sync_t;

typedef struct rwvcs_rwzk_update_recovery_action_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  const char * instance_name;
  vcs_recovery_type recovery_action;
  rw_status_t status;
} rwvcs_rwzk_update_recovery_action_sync_t;

typedef struct rwvcs_rwzk_seed_auto_instance_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  int start;
  const char *path;
  rw_status_t status;
} rwvcs_rwzk_seed_auto_instance_sync_t;

typedef struct rwvcs_rwzk_server_start_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  uint32_t server_id;
  int client_port;
  bool unique_ports;
  rwcal_zk_server_port_detail_ptr_t *server_details;
  int server_pid;
} rwvcs_rwzk_server_start_sync_t;

typedef struct rwvcs_rwzk_client_init_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  rw_status_t status;
} rwvcs_rwzk_client_init_sync_t;

typedef struct rwvcs_rwzk_client_start_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  rw_status_t status;
} rwvcs_rwzk_client_start_sync_t;

typedef struct rwvcs_rwzk_client_stop_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  rw_status_t status;
} rwvcs_rwzk_client_stop_sync_t;

typedef struct rwvcs_rwzk_client_state_sync_s {
  rwvcs_instance_ptr_t rwvcs;
  rwcal_kazoo_client_state_t client_state;
} rwvcs_rwzk_client_state_sync_t;

/* Lookup a component in the rwzk datastore without posting to rwq
 *
 * @param rwvcs             - rwvcs instance to use
 * @param instance_name     - name of component and instance id
 * @param pb_component[out] - on success, the component with the
 *                            specified instance name.
 * @return                  - RW_STATUS_NOTFOUND if the component
 *                            is not in the rwzk datastore.
 *
 *                            RW_STATUS_FAILURE on other failures.
 */
rw_status_t
rwvcs_rwzk_lookup_component_internal(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_info * pb_component);
/*
 * Update the zookeeper node information with that contained in the component
 * information without posting to rwq
 *
 * @param rwvcs - rwvcs instance
 * @param id    - updated component information
 * @return      - RW_STATUS_SUCCESS on success
 */
rw_status_t
rwvcs_rwzk_node_update_internal(
    rwvcs_instance_ptr_t rwvcs, 
    rw_component_info * id);

/*
 * Lock the zookeeper store for the specified component without posting to rwq
 *
 * @param rwvcs   - rwvcs instance.
 * @param id      - instance name of the component data to lock.
 * @param timeout - maximum time to wait for the lock.
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_TIMEDOUT if timeout hit
 *                  RW_STATUS_NOTFOUND if no component info found
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_lock_internal(rwvcs_instance_ptr_t rwvcs, 
                         const char * id, 
                         struct timeval * timeout);
/*
 * Unlock the zookeeper store for the specified component without posting to rwq
 *
 * @param rwvcs   - rwvcs instance.
 * @param id      - instance name of the component data to lock.
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_NOTFOUND if no component info found
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_unlock_internal(rwvcs_instance_ptr_t rwvcs, const char * id);

rw_status_t validate_bootstrap_phase(vcs_manifest_bootstrap * bootstrap);

/*
 * Send signal to pid and wait if it exit. SIGKILL if not succeeding to remove
 *
 * @param pid  - process id of the process
 * @param signal - signal to be sent 
 * @param name   - name of instance if applicable
 * @return       - void
 */
void send_kill_to_pid(
    int pid,
    int signal,
    char *name);

__END_DECLS

#endif // __RWVCS_INTERNAL_H__
