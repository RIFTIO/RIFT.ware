
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWVCS_MANIFEST_H__
#define __RWVCS_MANIFEST_H__

#include "rwvcs.h"

__BEGIN_DECLS

/*
 * Load the specified manifest.
 *
 * @param rwvcs         - rwvcs instace
 * @param manifest_path - path to the manifest file
 * @return              - rw_status
 */
rw_status_t rwvcs_manifest_load(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_path);

/*
 * Lookup a component definition in the manifest.
 *
 * @param rwvcs     - rwvcs instance
 * @param name      - name of the component
 * @param component - on success a pointer to the component definition.  This pointer
 *                    is owned by rwvcs.
 * @return          - RW_STATUS_SUCCESS
 *                    RW_STATUS_NOTFOUND if the name cannot be found
 */
rw_status_t rwvcs_manifest_component_lookup(
    rwvcs_instance_ptr_t rwvcs,
    const char * name,
    vcs_manifest_component ** m_component);

/*
 * Lookup an event in the specified event list.
 *
 * @param event_name  - name of the event to find
 * @param list        - event list to search
 * @param event       - on success a pointer to the event definition.  This pointer
 *                      is owned by by the event list.
 * @return            - RW_STATUS_SUCCESS
 *                      RW_STATUS_NOTFOUND if the name cannot be found
 */
rw_status_t rwvcs_manifest_event_lookup(
    char * event_name,
    vcs_manifest_event_list * list,
    vcs_manifest_event ** event);

/*
 * Check if a given component exists in the manifest.
 *
 * @param rwvcs - rwvcs instance
 * @param name  - name of the component to lookup.
 * @return      - true if the manifest contains that component, false otherwise.
 */
bool rwvcs_manifest_have_component(rwvcs_instance_ptr_t rwvcs, const char * name);

/*
 * get config_ready count from manifest 
 *
 * @param rwvcs         - instance 
 * @return              - count of config_ready enabled elements
 */
int rwvcs_manifest_config_ready_count(rwvcs_instance_ptr_t rwvcs);

typedef struct rwvcs_manifest_vm_list_s {
  char vm_name[128];
  char vm_ip_address[32];
  struct rwvcs_manifest_vm_list_s *vm_list_next;
} rwvcs_manifest_vm_list_t;

/*
 * get rwvm list and associated ip addr from manifest 
 *
 * @param rwvcs - instance
 * @param lead_ip - ip address of the lead vm
 * @return      - number of vms in the list
 */
int rwvcs_manifest_rwvm_list(rwvcs_instance_ptr_t rwvcs,
                             char                 *lead_ip,
                             rwvcs_manifest_vm_list_t **vm_list);

/*
 * returns true if management VM is in lss/ls mode
 *
 * @param rwvcs - instance
 * @return      - true if in lss mode
 */
bool
rwvcs_manifest_is_lss_mode(rwvcs_instance_ptr_t rwvcs);

/*
 * get the local ip addrs of management VM
 *
 * @param bootstrap - protobuf of bootstrap portion of manifest
 * @return      - local ip address of management VM
 */
char *
rwvcs_manifest_get_local_mgmt_addr(vcs_manifest_bootstrap *bootstrap);

/*
 * returns if the component_name is a management VM component
 *
 * @param rwvcs - instance
 * @param component_name - component_name
 * @return      - TRUE if component_name is a management VM, FALSE otherwise
 */
bool
rwvcs_manifest_is_mgmt_vm (rwvcs_instance_ptr_t rwvcs, char *component_name);

/*
 * returns if the component_name is a management VM component
 *
 * @param rwvcs - instance
 * @param mgmt_vm - 
 * @param ip_address - 
 */
void 
rwvcs_manifest_setup_mgmt_info (
    rwvcs_instance_ptr_t rwvcs,
    bool                 mgmt_vm,
    const char           *ip_address);

__END_DECLS
#endif
