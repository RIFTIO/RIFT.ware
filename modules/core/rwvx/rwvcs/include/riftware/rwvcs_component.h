
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
 *
 */

#ifndef __RWVCS_COMPONENT_H__
#define __RWVCS_COMPONENT_H__

#include "rwvcs.h"

__BEGIN_DECLS

/*
 * Add a child to the specified parent.  If the child is already listed
 * then nothing is done.
 *
 * @param parent_info     - parent component
 * @param child_instance  - instance-name of child
 */
void rwvcs_component_add_child(
    rw_component_info * parent,
    const char * child_instance);

/*
 * Remove a child from the specified parent.
 *
 * @param parent          - parent component
 * @param child_instance  - instance name of child to remove
 */
void rwvcs_component_delete_child(
    rw_component_info * parent,
    const char * child_instance);

/*
 * Delete a component.  This includes removing the entry from the zookeeper and
 * updating the parent's children list.
 *
 * @param rwvcs     - rwvcs instance
 * @param component - component to delete
 */
void rwvcs_component_delete(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component);

/*
 * Allocate and store a rwcollection component.  This will create an (empty) node
 * in the data store.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_id       - instance name of the parent component, NULL if none
 * @param component_name  - name of the component as found in the manifest
 * @param instance_id     - instance id
 * @param instance_name   - name of this instance of the component.
 * @return                - pointer to the matching component or NULL if not found.
 */
rw_component_info * rwvcs_rwcollection_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name);

/*
 * Allocate and store a rwvm component.  This will create an (empty) node in
 * the data store.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_id       - instance name of the parent component, NULL if none
 * @param component_name  - name of the component as found in the manifest
 * @param instance_id     - instance id
 * @param instance_name   - name of this instance of the component.
 * @return                - pointer to the matching component or NULL if not found.
 */
rw_component_info * rwvcs_rwvm_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name);

/*
 * Allocate and store a rwproc component.  This will create an (empty) node in
 * the data store.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_id       - instance name of the parent component, NULL if none
 * @param component_name  - name of the component as found in the manifest
 * @param instance_id     - instance id
 * @param instance_name   - name of this instance of the component.
 * @return                - pointer to the matching component or NULL if not found.
 */
rw_component_info * rwvcs_rwproc_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name);

/*
 * Allocate and store a native proc component.  This will create an (empty) node in
 * the data store.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_id       - instance name of the parent component, NULL if none
 * @param component_name  - name of the component as found in the manifest
 * @param instance_id     - instance id
 * @param instance_name   - name of this instance of the component.
 * @return                - pointer to the matching component or NULL if not found.
 */
rw_component_info * rwvcs_proc_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name);

/*
 * Allocate and store a rwtasklet component.  This will create an (empty) node
 * in the data store.
 *
 * @param rwvcs           - rwvcs instance
 * @param parent_id       - instance name of the parent component, NULL if none
 * @param component_name  - name of the component as found in the manifest
 * @param instance_id     - instance id
 * @param instance_name   - name of this instance of the component.
 * @return                - pointer to the matching component or NULL if not found.
 */
rw_component_info * rwvcs_rwtasklet_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name);

__END_DECLS
#endif


