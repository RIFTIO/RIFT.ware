
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
 * @file rwvx.h
 * @author Justin Bronder (justin.bronder@riftio.com)
 * @date 09/29/2014
 * @brief Top level API include for rwvcs_zk submodule
 */

#ifndef __RWCAL_API_H__
#define __RWCAL_API_H__

#include <stdbool.h>

#include <libpeas/peas.h>

#include <rwvcs_zk.h>
#include <rwlib.h>
#include <rw-manifest.pb-c.h>
#include <rw_vx_plugin.h>

#include "rwlog.h"

__BEGIN_DECLS

struct rwvcs_zk_module_s {
  rw_vx_framework_t * framework;
  rw_vx_modinst_common_t *mip;
  PeasExtension * zk;
  RwVcsZkZookeeper * zk_cls;
  RwVcsZkZookeeperIface * zk_iface;
  rwlog_ctx_t *rwlog_instance;
};
typedef struct rwvcs_zk_module_s * rwvcs_zk_module_ptr_t;

/* Type generated by vala, see rwvcs_zk.vala RwVcsZk::ZkServerPortDetail */
typedef RwVcsZkServerPortDetail * rwvcs_zk_server_port_detail_ptr_t;

/* Type generated by vala, see rwvcs_zk.vala RwVcsZk::KazooClientStateEnum */
typedef RwVcsZkKazooClientStateEnum rwvcs_zk_kazoo_client_state_t;

/* Type generated by vala, see rwvcs_zk.vala RwVcsZk::Closure */
typedef RwVcsZkClosure * rwvcs_zk_closure_ptr_t;

/*
 * Allocate a rwvcs_zk module.  Once allocated, the clients within
 * the module still need to be initialized.  For rwvcs_zk, see
 * rwvcs_zk_{kazoo,zake}_init().  It is a fatal error to attempt 
 * to use any client before it has been initialized.  However, it is
 * perfectly fine to not initialize a client that will remain
 * unused.  Note that every function contains the client that it
 * will use as part of the name, just after the rwvcs_zk_ prefix.
 *
 * @return - rwvcs_zk module handle or NULL on failure.
 */
rwvcs_zk_module_ptr_t rwvcs_zk_module_alloc();

/*
 * Deallocate a rwvcs_zk module.
 *
 * @param - pointer to the rwvcs_zk module to be deallocated.
 */
void rwvcs_zk_module_free(rwvcs_zk_module_ptr_t * rwvcs_zk);

/*
 * Create a rwvcs_zk closure.  The closure can be passed as an argument to python
 * functions which expect a callback.  Then, when python triggers its callback,
 * the closure will execute the function that was passed in.  If the function
 * returns anything but RW_STATUS_SUCCESS, the corresponding exception will
 * be raised in python (RWError*), where it can be handled normally by python.
 *
 * Callbacks must match the declaration:
 *  rw_status_t callback(rwvcs_zk_module_ptr_t rwvcs_zk, void * user_data);
 * The first parameter will be set to the rwvcs_zk instance that created this
 * closure.  The second is user specified.
 *
 * @param rwvcs_zk  - module handle.
 * @param callback  - callback to execute when python's callback is triggered.
 * @param user_data - passed as the second argument to the callback.
 * @return          - rwvcs_zk closure instance or NULL on error.
 */
rwvcs_zk_closure_ptr_t rwvcs_zk_closure_alloc(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    rw_status_t (*callback)(rwvcs_zk_module_ptr_t, void *, int),
    void * user_data);

/*
 * Deallocate a rwvcs_zk closure.  Note that any user data that was passed in will
 * not be touched.  It is up to the caller to handle that.  On return, closure
 * will be set to NULL.
 *
 * @param closure - pointer to closure to deallocate.
 */
void rwvcs_zk_closure_free(rwvcs_zk_closure_ptr_t * closure);

/*
 * Create a rwvcs_zk zookeeper server port detail.  This can be passed as an 
 * argument to kazoo client init functions
 *
 * @param zk_server_addr - zookeeper server ip address as a string
 * @param zk_client_port - client port associated with the server
 * @param zk_server_id   - id associated with the server
 * @param zk_server_start - indicate if server needs to be started
 * @param zk_client_connect - indicate if client need to connect to this server
 * @return               - rwvcs_zk_zk_server_port_detail instance or NULL on error.
 */
rwvcs_zk_server_port_detail_ptr_t rwvcs_zk_server_port_detail_alloc(
    char               *zk_server_addr,
    int                zk_client_port,
    int                zk_server_id,
    bool               zk_server_start,
    bool               zk_client_connect);

/*
 * Deallocate a rwvcs_zk zookeeper server port detail.  
 * On return the zk_server_port_detail shall be set to NULL
 *
 * @param zk_server_port_detail - pointer to zk_server_port_detail to deallocate.
 */
void rwvcs_zk_server_port_detail_free(rwvcs_zk_server_port_detail_ptr_t * zk_server_port_detail);

/*
 * Create the zookeeper server configuration.
 *
 * @param rwvcs_zk      - module handle.
 * @param id            - identifier of this server in the zookeeper ensemble.
 * @param client_port   - client port for this server instance
 * @param unique_ports  - generate port number based on UID.
 * @param server_details - NULL terminated list of zookeeper server port detail
 * @return              - rift_status.
 */
rw_status_t rwvcs_zk_create_server_config(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    int id,
    int client_port,
    bool unique_ports,
    rwvcs_zk_server_port_detail_ptr_t *server_details,
    const char *rift_var_root);

/*
 * Start the zookeeper server.
 *
 * @param rwvcs_zk         - module handle.
 * @param id            - identifier of this server in the zookeeper ensemble.
 * @return              - pid of started zoo keeper server
 */
int rwvcs_zk_server_start(rwvcs_zk_module_ptr_t rwvcs_zk, int id);

/*
 * Initialize rwvcs_zk to use a real zookeeper server.  This is done
 * by using the python Kazoo module.
 *
 * @param rwvcs_zk         - module handle.
 * @param server_details - NULL terminated list of zookeeper server port detail
 * @return              - rift_status.
 */
rw_status_t rwvcs_zk_kazoo_init(rwvcs_zk_module_ptr_t rwvcs_zk, rwvcs_zk_server_port_detail_ptr_t *server_details);

/*
 * Start kazoo client
 * by using the python Kazoo module.
 *
 * @param rwvcs_zk         - module handle.
 * @return              - rift_status.
 */
rw_status_t rwvcs_zk_kazoo_start(rwvcs_zk_module_ptr_t rwvcs_zk);

/*
 * Stop kazoo client
 * by using the python Kazoo module.
 *
 * @param rwvcs_zk         - module handle.
 * @return              - rift_status.
 */
rw_status_t rwvcs_zk_kazoo_stop(rwvcs_zk_module_ptr_t rwvcs_zk);

/*
 * Initialize rwvcs_zk to use a fake, in-memory, server.  This is suitable
 * for fully collapsed RIFT collectives.
 *
 * @param rwvcs_zk - module handle.
 * @return      - rift status.
 */
rw_status_t rwvcs_zk_zake_init(rwvcs_zk_module_ptr_t rwvcs_zk);

/*
 * Create a zookeeper node
 *
 * @param rwvcs_zk   - module handle.
 * @param path    - path to the node to create.
 * @param closure - callback closure which would result in invoke of async
 *                  flavor of zk operation
 * @return      - RW_STATUS_SUCCESS on creation,
 *                RW_STATUS_EXISTS if the node already exists,
 *                RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_create(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path,
                              const rwvcs_zk_closure_ptr_t closure);

/*
 * Check if a zookeeper node exists
 *
 * @param rwvcs_zk - module handle.
 * @param path  - path to the node.
 * @return      - true if the node exists, false otherwise.
 */
bool rwvcs_zk_exists(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path);

/*
 * Get data stored at the given zookeeper node.
 *
 * @param rwvcs_zk   - module_handle.
 * @param path    - path to node.
 * @param data    - on success, contains a pointer to a buffer containing the node data.
 * @param closure - callback closure which would result in invoke of async
 *                  flavor of zk operation
 * @return      - RW_STATUS_SUCCESS,
 *                RW_STATUS_NOTFOUND if the node doesn't exists,
 *                RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_get(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    char ** data,
    const rwvcs_zk_closure_ptr_t closure);

/*
 * Set data stored at the given zookeeper node.
 *
 * @param rwvcs_zk   - module_handle.
 * @param path    - path to node.
 * @param data    - pointer to data to set in the node.
 * @param closure - callback closure which would result in invoke of async
 *                  flavor of zk operation
 * @return      - RW_STATUS_SUCCESS,
 *                RW_STATUS_NOTFOUND if the node doesn't exists,
 *                RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_set(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const char * data,
    const rwvcs_zk_closure_ptr_t closure);

/*
 * Get a list of the children of the specified node.
 *
 * @param rwvcs_zk     - module handle.
 * @param path      - path to node.
 * @param children  - On success, NULL-terminated list of children nodes.
 * @param closure   - callback closure which would result in invoke of async
 *                    flavor of zk operation
 * @return          - RW_STATUS_SUCCESS,
 *                    RW_STATUS_NOTFOUND if the node doesn't exist,
 *                    RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_get_children(
    rwvcs_zk_module_ptr_t rwcfal,
    const char * path,
    char *** children,
    const rwvcs_zk_closure_ptr_t closure);

/*
 * Delete a zookeeper node.  Note that similar to 'rmdir' the node must
 * not have any children.
 *
 * @param rwvcs_zk   - module handle.
 * @param path    - path to node.
 * @param closure - callback closure which would result in invoke of async
 *                  flavor of zk operation
 * @return      - RW_STATUS_SUCCESS,
 *                RW_STATUS_NOTEMPTY if the node has children,
 *                RW_STATUS_NOTFOUND if the node doesn't exist,
 *                RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_delete(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path,
                              const rwvcs_zk_closure_ptr_t closure);

/*
 * Watch a zookeeper node for any data changes as well as creation/deletion
 * of the node itself.
 *
 * @param rwvcs_zk   - module handle.
 * @param path    - path to node to monitor.
 * @param closure - callback closure which would result in invoke of async
 *                  flavor of zk operation
 * @return        - rw_status_t
 */
rw_status_t rwvcs_zk_register_watcher(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const rwvcs_zk_closure_ptr_t closure);

/*
 * Stop watching a zookeeper node for any changes.
 *
 * @param rwvcs_zk   - module handle.
 * @param path    - path to stop monitoring.
 * @param closure - callback closure to unregister.
 * @return        - rw_status_t
 */
rw_status_t rwvcs_zk_unregister_watcher(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const rwvcs_zk_closure_ptr_t closure);

/*
 * Lock a node for writing.  This call is reentrant.
 *
 * @param rwvcs_zk   - module handle.
 * @param path    - path to lock.
 * @param timeout - if not NULL, maximum amount of time to wait on acquisition
 * @return        - RW_STATUS_SUCCESS,
 *                  RW_STATUS_NOTFOUND if the node does not exist,
 *                  RW_STATUS_NOTCONNECTED if lock acquisition fails
 *                  RW_STATUS_TIMEOUT if acquisition timed out
 *                  RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_lock(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    struct timeval * timeout);

/*
 * Unlock a node for writing.
 *
 * @param rwvcs_zk - module handle.
 * @param path  - path to unlock.
 * @return      - RW_STATUS_SUCCESS,
 *                RW_STATUS_NOTFOUND if the node does not exist,
 *                RW_STATUS_NOTCONNECTED if a lock was not previously acquired,
 *                RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwvcs_zk_unlock(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path);

/*
 * Test if a node is locked or not.
 *
 * @param rwvcs_zk - module handle.
 * @param path  - path to check for locked status.
 * @return      - True if the node is locked, false otherwise.
 */
bool rwvcs_zk_locked(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path);


/*
 * Get different elements from the userdata
 *
 * @param users       - userdata as returned in zk callback functions
 * @param idx         - index to the data to be picked up
 * @return            - pointer to first userdata pointer or NULL
 */
void *rwvcs_zk_get_userdata_idx(void *userdata, int idx);

/*
 * Get kazoo client state. this is pending upgrade to later kazoo
 * version as client_state may not be stored at kazoo client otherwise
 *
 * @param rwvcs_zk - module handle.
 * @return      - client state 
 */
rwvcs_zk_kazoo_client_state_t
rwvcs_zk_client_state(rwvcs_zk_module_ptr_t rwvcs_zk);

__END_DECLS

#endif

