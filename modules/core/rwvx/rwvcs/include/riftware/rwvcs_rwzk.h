
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWVCS_RWZK_H__
#define __RWVCS_RWZK_H__

#include "rwvcs.h"

__BEGIN_DECLS
/*
 * Return a pointer to a string representing the instance name of the specified
 * component.
 *
 * @param component_name  - name of component
 * @param instance_id     - ID of component instance
 * @return                - allocated string representing the instance name.  It is up to
 *                          the caller to free this string.
 */
char * to_instance_name(const char * component_name, int instance_id);

/*
 * Return a pointer to a string representing the instance name converted to a path
 * suitable for use with rwmsg or dts.  Basically, this converts the last instance
 *
 * @param instance_name - component - identifier combination
 * @return              - a path representing the instance.  The caller owns this string.
 */
char * to_instance_path(const char * instance_name);

/*
 * Return a pointer to a string representing the component name part of the
 * specified instance name (id).
 *
 * @param instance_name - component - identifier combination
 * @return              - Name of the component being referred to, NULL on error.  It
 *                        is up to the caller to free this string.
 */
char * split_component_name(const char * instance_name);

/*
 * Return the instance id portion of the specified instance name
 *
 * @param instance_name - component - identifier combination
 * @return              - instance portion of the instance_name or < 0 on error.
 */
int split_instance_id(const char * instance_name);


/* Lookup a component in the rwzk datastore
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
rwvcs_rwzk_lookup_component(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_info * pb_component);

/* Update a component state  in the rwzk datastore
 *
 * @param rwvcs             - rwvcs instance to use
 * @param instance_name     - name of component and instance id
 * @param state             - New state 
 * @return                  - RW_STATUS_NOTFOUND if the component
 *                            is not in the rwzk datastore.
 *
 *                            RW_STATUS_FAILURE on other failures.
 */
rw_status_t
rwvcs_rwzk_update_state(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_state state);

/*
 * Find all descendents of a given component.  A list of each descendents
 * instance name will be returned including the root component itself.
 *
 * @param rwvcs           - rwvcs instance
 * @param instance_name   - instance name of component at which to start
 * @param descendents     - on success, a null-terminated list of the instance
 *                          names of each descendent of the root node including
 *                          the root node as well.
 * @return                - RW_STATUS_SUCCESS
 *                          RW_STATUS_NOTFOUND if the component is not in the
 *                          rwzk datastore.
 *                          RW_STATUS_FAILURE on any other failure.
 */
rw_status_t rwvcs_rwzk_descendents(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char *** descendents);

/*
 * Get the component info for every descendent of the given component.  The
 * list of returned components will include the root component itself.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - instance name of root component
 * @param components    - on success a null-terminated list of component info
 *                        for each descendent and the root component
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if the component is not in the
 *                        rwzk datastore.
 *                        RW_STATUS_FAILURE on any other failure.
 */
rw_status_t rwvcs_rwzk_descendents_component(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_info *** components);

/*
 * Find all local neighbors that match the given critera.  Neighbors are defined as
 * components that are descendents of the node at the specified height higher in
 * the hierarchy.  The starting point will be included in the results if it passes
 * the test function.
 *
 * @param rwvcs         - rwvcs instance.
 * @param instance_name - instance name of the starting point.
 * @param test          - function called on each neighbor to test if it should be
 *                        included in the results with an argument for the userdata that
 *                        is passed.
 * @param userdata      - userdata pased to the test function above for each neighbor
 * @param height        - height of the starting point relative to instance_name.  For
 *                        instance, a height of 2 starts at the grandparent.
 * @param neighbors     - on success, a null-terminated list of the instances names
 *                        of all neighbors which passed the test function.
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if the lookup fails for any component
 *                        RW_STATUS_OUT_OF_BOUNDS if height goes past the root of the tree
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwvcs_rwzk_get_neighbors(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool (*test)(rw_component_info  *, void *),
    void * userdata,
    uint32_t height,
    char *** neighbors);

/*
 * Starting with height 0, call rwvcs_rwzk_get_neighbors until a match is
 * found.
 *
 * @param rwvcs         - rwvcs instance.
 * @param instance_name - instance name of the starting point.
 * @param test          - function called on each neighbor to test if it should be
 *                        included in the results with an argument for the userdata that
 *                        is passed.
 * @param userdata      - userdata pased to the test function above for each neighbor
 * @param neighbors     - on success, a null-terminated list of the instances names
 *                        of all neighbors which passed the test function.
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if the lookup fails for any component
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwvcs_rwzk_climb_get_neighbors(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool (*test)(rw_component_info *, void *),
    void * userdata,
    char *** neighbors);

/*
 * Helper function for the get_neighbors calls.  Tests to see if the component
 * name matches.
 *
 * @param component - component to test
 * @param userdata  - desired component-name (char *)
 * @return          - true if the component's component-name matches userdata.
 */
bool
rwvcs_rwzk_test_component_name(rw_component_info * component, void * userdata);

/*
 * Helper function for the get_neighbors calls.  Tests to see if the component
 * type matches.
 *
 * @param component - component to test
 * @param userdata  - desired component-name (rw_component_type *)
 * @return          - true if the component's component-type matches userdata.
 */
bool
rwvcs_rwzk_test_component_type(rw_component_info * component, void * userdata);

/*
 * Find the instance name of the nearest leader.  Given any component, the nearest
 * leader is the leader of the first grouping that both contains this component and
 * has a defined leader.
 *
 * This should never fail and if the lookup fails, the function will probably throw
 * an assert.  However, at this time it is still possible to write a valid-looking
 * manifest which does not mark at least one leader.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - instance name of component for which to find the nearest
 *                        leader
 * @param leader_id     - on successful lookup, the instance name of the the
 *                        component's nearest leader, NULL otherwise.
 * @return              - RW_STATUS_SUCCESS,
 *                        RW_STATUS_NOTFOUND if the component does not have a leader.
 */
rw_status_t
rwvcs_rwzk_nearest_leader(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char ** leader_id);

/*
 * Check to see if the specified instance has a child matching the given instance-name.
 *
 * @param rwvcs           - rwvcs instance
 * @param instance_name   - instance-name of parent whose children to check
 * @param child_instance  - instance-name of the child being searched for
 * @return                - true if the parent has a matching child
 */
bool rwvcs_rwzk_has_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * child_instance);

/*
 * Check to see if the specified instance is responsible for the given target.
 * Responsible is defined as follows:
 *  - The target is a child
 *  - The target is being led by the specified instance
 *  - The target is owned by a collection being led by the specified instance.
 *
 * @param rwvcs           - rwvcs instance
 * @param instance_name   - instance-name of the owning/lead component
 * @param target_instance - instance-name of the component being searched against.
 * @return                - true if the specified instance is responsible for the target.
 */
bool rwvcs_rwzk_responsible_for(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * target_instance);

/*
 * Check to see if the specified instance is acting as the leader of the
 * given collection.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - instance-name of possible leader
 * @param target        - instance-name of collection
 * @return              - true if the specified instance is leading the target.
 */
bool rwvcs_rwzk_is_leader_of(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * target);

/*
 * Seed the auto instance assigner.  The available instance will increase
 * monotonically from the start value.
 *
 * @param rwvcvs  - rwvcs instance.
 * @param start   - value at which to begin auto assignment.
 * @param path    - path for the 'instance_path'. If NULL, use g_instance_path
 * @return        - RW_STATUS_SUCCESS.
 */
rw_status_t
rwvcs_rwzk_seed_auto_instance(rwvcs_instance_ptr_t rwvcs, uint32_t start, const char *path);

/*
 * Get the next available auto-assigned instance id and update the data store to
 * mark the returned id as allocated.
 *
 * @param rwvcs       - rwvcs instance
 * @param instance_id - on success, an auto-assigned instance id that can be
 *                      used immediately, otherwise 0.
 * @param path    - path for the 'instance_path'. If NULL, use g_instance_path
 * @return            - RW_STATUS_SUCCESS
 */
rw_status_t
rwvcs_rwzk_next_instance_id(rwvcs_instance_ptr_t rwvcs, uint32_t * instance_id, const char *path);

/*
 * Test to see if given instance exists in the zookeeper
 *
 * @param rwvcs - rwvcs instance
 * @param id    - component instance name
 * @return      - True if the instance is in the zookeeper, false otherwise
 */
bool rwvcs_rwzk_node_exists(rwvcs_instance_ptr_t rwvcs, const char * id);

/*
 * Update a component's parent.  This includes updating rwcomponent_parent in
 * the child, updating rwcomponent_children in the parent and, removing the
 * child from the previous parent.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - name of the component to reparent
 * @param new_parent    - instance-id of the new parent, if NULL parent will be removed
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if any component is not found
 *                        RW_STATUS_FAILURE otherwise
 *                        ASSERT if writing updated components fails.
 */
rw_status_t
rwvcs_rwzk_update_parent(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * new_parent);

/*
 * Lock the zookeeper store for the specified component.
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
rwvcs_rwzk_lock(rwvcs_instance_ptr_t rwvcs, const char * id, struct timeval * timeout);

/*
 * Unlock the zookeeper store for the specified component.
 *
 * @param rwvcs   - rwvcs instance.
 * @param id      - instance name of the component data to lock.
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_NOTFOUND if no component info found
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_unlock(rwvcs_instance_ptr_t rwvcs, const char * id);

/*
 * Update the zookeeper node information with that contained in the component
 * information.
 *
 * @param rwvcs - rwvcs instance
 * @param id    - updated component information
 * @return      - RW_STATUS_SUCCESS on success
 */
rw_status_t
rwvcs_rwzk_node_update(rwvcs_instance_ptr_t rwvcs, 
                       rw_component_info * id);

/*
 * Delete the specified instance from the parent's list of the children.
 * If the instance is not a child of the given parent, it is considered
 * a success.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - instance name of parent
 * @param child_name    - component name of child to remove
 * @param child_id      - instance id of child to remove
 * @return              - rw_status_t
 */
rw_status_t
rwvcs_rwzk_delete_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * child_name,
    int child_id);

/*
 * Add the specified instance to the parent's list of children.  Additions
 * that would cause the parent to have duplicates are successful no-ops.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_instance - instancei name of parent
 * @param child_instance  - instance name of child to add
 * @return                - rw_status_t
 */
rw_status_t
rwvcs_rwzk_add_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_instance,
    const char * child_instance);

/*
 * Update the state of a component.
 *
 * @param rwvcs
 * @param instance_name - instance name of the component to update
 * @param state         - new state of component
 * @return              - rw_status_t
 */
rw_status_t
rwvcs_rwzk_update_state(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_state state);

/*
 * Update a component's config_ready state from action start
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - name of the component to reparent
 * @param config_ready  - boolean value of state
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if any component is not found
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwvcs_rwzk_update_config_ready(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool         config_ready);

/*
 * Update a component's recovery_action state from action start
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - name of the component to reparent
 * @param recovery_action  - component recovery action 
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if any component is not found
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwvcs_rwzk_update_recovery_action(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    vcs_recovery_type recovery_action);

/*
 * starts a watcher - Blocking Call
 *
 * @param rwvcs         - rwvcs instance
 * @param path          - path on which watcher is set
 * @param sched_tasklet_p  - sched tasklet ptr
 * @param rwq           - dispatch serial queue for dispatch callback
 * @param cb            - callback routine
 * @param ud            - userdata for callback
 * @return              - rwcal closure if allocated - NULL otherwise
 */
rwcal_closure_ptr_t
rwvcs_rwzk_watcher_start(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    rwsched_tasklet_ptr_t sched_tasklet_ptr,
    rwsched_dispatch_queue_t rwq,
    void (*cb)(void* ud),
    void *ud);

/*
 * Get a list of the children of the specified node - Blocking Call
 *
 * @param rwvcs     - rwvcs instance
 * @param path      - path to node.
 * @param children  - On success, NULL-terminated list of children nodes.
 * @return          - RW_STATUS_SUCCESS,
 *                    RW_STATUS_NOTFOUND if the node doesn't exist,
 *                    RW_STATUS_FAILURE otherwise.
 */

rw_status_t
rwvcs_rwzk_get_children(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    char ***children);

/*
 * Get data stored at the given zk node - Blocking Call
 *
 * @param rwvcs   - rwvcs instance
 * @param path    - path to node.
 * @param data    - on success, contains a pointer to a buffer containing the node data.
 * @return      - RW_STATUS_SUCCESS,
 *                RW_STATUS_NOTFOUND if the node doesn't exists,
 *                RW_STATUS_FAILURE otherwise.                                                                                                                                                                                                                                   
 */
rw_status_t
rwvcs_rwzk_get(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    char ** data);

/*
 * set data at specified path
 *
 * @param rwvcs   - rwvcs instance.
 * @param path    - path to which data need to be set
 * @param data    - data that needs to be stored
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_set(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path,
    const char * data);

/*
 * Delete a instance data node from the zookeeper
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - name of instance to delete
 * @return              - rw_status_t
 */
rw_status_t
rwvcs_rwzk_delete_node(rwvcs_instance_ptr_t rwvcs, const char * instance_name);

/*
 * create the specified path
 *
 * @param rwvcs   - rwvcs instance.
 * @param path    - path to which data need to be set
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_create(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path);

/*
 * check if specified path exist
 *
 * @param rwvcs   - rwvcs instance.
 * @param path    - path to which data need to be set
 * @return        - True if the instance is in the zookeeper, false otherwise
 */
rw_status_t
rwvcs_rwzk_exists(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path);

/*
 * Lock the zookeeper store for the specified path
 *
 * @param rwvcs   - rwvcs instance.
 * @param path    - path for which lock is to be taken
 * @param timeout - maximum time to wait for the lock.
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_TIMEDOUT if timeout hit
 *                  RW_STATUS_NOTFOUND if not found
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_lock_path(rwvcs_instance_ptr_t rwvcs, const char *path, struct timeval * timeout);

/*
 * Unlock the zookeeper store for the specified path
 *
 * @param rwvcs   - rwvcs instance.
 * @param path    - path to which lock is to be released
 * @return        - RW_STATUS_SUCCESS
 *                  RW_STATUS_NOTFOUND if not found
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t
rwvcs_rwzk_unlock_path(rwvcs_instance_ptr_t rwvcs, const char * path);

/*
 * Delete a path from the zookeeper
 *
 * @param rwvcs         - rwvcs instance
 * @param path          - path to delete
 * @return              - rw_status_t
 */
rw_status_t
rwvcs_rwzk_delete_path(rwvcs_instance_ptr_t rwvcs, const char * path);

/*
 * stops a watcher - Blocking Call
 *
 * @param rwvcs         - rwvcs instance
 * @param path          - path on which watcher is set
 * @param closure       - pointer to the closure returned by watcher_start
 * @return              - RW_STATUS_SUCCESS
 *                        RW_STATUS_NOTFOUND if path is not found
 *                        RW_STATUS_FAILURE otherwise
 */
rw_status_t
rwvcs_rwzk_watcher_stop(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    rwcal_closure_ptr_t *closure);

/*
 * init and start the zookeeper client or zake client
 *
 * @param rwvcvs  - rwvcs instance.
 * @return        - RW_STATUS_SUCCESS.
 */
rw_status_t rwvcs_rwzk_client_init(rwvcs_instance_ptr_t rwvcs);

/*
 * start the zookeeper client
 *
 * @param rwvcvs  - rwvcs instance.
 * @return        - RW_STATUS_SUCCESS.
 */
rw_status_t rwvcs_rwzk_client_start(rwvcs_instance_ptr_t rwvcs);


/*
 * stop the zookeeper client
 *
 * @param rwvcvs  - rwvcs instance.
 * @return        - RW_STATUS_SUCCESS.
 */
rw_status_t rwvcs_rwzk_client_stop(rwvcs_instance_ptr_t rwvcs);

/*
 * zookeeper client state. current version of kazoo does not store client state
 * this shall be supported once kazoo does give the client_state
 *
 * @param rwvcvs  - rwvcs instance.
 * @return        - client state relevant in expanded mode
 */
rwcal_kazoo_client_state_t rwvcs_rwzk_client_state(rwvcs_instance_ptr_t rwvcs);

__END_DECLS

#endif
