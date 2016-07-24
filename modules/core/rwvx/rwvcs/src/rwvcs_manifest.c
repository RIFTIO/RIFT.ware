
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <ifaddrs.h>
#include <stdio.h>

#include "rwvcs.h"
#include "rwvcs_manifest.h"
#include "rwcal_vars_api.h"

#define RWTRACE_CATEGORY_RWVCS_MANIFEST RWTRACE_CATEGORY_RWTASKLET

/*
 * Validate the bootstrap phase defined in the specified manifest.  This checks
 * to make sure that all the required values are actually defined and that no
 * conflicting options are selected.
 *
 * @param bootstrap         - manifest bootstrap-phase to validate
 * @return                  - RW_STATUS_SUCCESS or RW_STATUS_FAILURE if validation fails
 */
rw_status_t validate_bootstrap_phase(vcs_manifest_bootstrap * bootstrap)
{
  vcs_manifest_trace *rwtrace;
  vcs_manifest_init_tasklet *rwtasklet;

  // Validate <manifest/bootstrap-phase>
  RW_ASSERT(bootstrap);

  // Validate <manifest/bootstrap-phase/rwtrace>
  rwtrace = bootstrap->rwtrace;
  RW_ASSERT(rwtrace);

  // Validate <manifest/bootstrap-phase/rwtasklet>
  rwtasklet = bootstrap->rwtasklet;
  RW_ASSERT(rwtasklet);

  // Validate <manifest/bootstrap-phase/zookeeper>
  RW_ASSERT(bootstrap->zookeeper);
  RW_ASSERT(bootstrap->zookeeper->master_ip);
  RW_ASSERT(bootstrap->zookeeper->has_zake);
  RW_ASSERT(bootstrap->zookeeper->has_unique_ports);

  // Validate <manifest/bootstrap-phase/rwcal_cloud>
  // TODO:  Eventually this should be required, right now it's unnecessary however.
  if (bootstrap->rwcal_cloud) {
    RW_ASSERT(bootstrap->rwcal_cloud->has_cloud_type);
    RW_ASSERT(bootstrap->rwcal_cloud->auth_key);
    RW_ASSERT(bootstrap->rwcal_cloud->auth_secret);
  }

  // Return the status of the validate operation
  return RW_STATUS_SUCCESS;
}


rw_status_t rwvcs_manifest_load(
    rwvcs_instance_ptr_t rwvcs,
    const char * manifest_path)
{
  rw_status_t status;
  rw_xml_document_t * xml_doc = NULL;
  char * xml_str = NULL;

  xml_doc = rw_xml_manager_create_document_from_file(rwvcs->xml_mgr, manifest_path, true);
  RW_ASSERT(xml_doc);

  rw_xml_document_to_string(xml_doc, NULL, NULL, &xml_str);
  RW_ASSERT(xml_str);

  status = rw_xml_manager_xml_to_pb(rwvcs->xml_mgr, xml_str, &rwvcs->pb_rwmanifest->base, NULL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);


  return status;
}

rw_status_t rwvcs_manifest_component_lookup(
    rwvcs_instance_ptr_t instance,
    const char * name,
    vcs_manifest_component **m_component)
{
  vcs_manifest_inventory * inventory;
  int i;

  // Validate input parameters
  RW_CF_TYPE_VALIDATE(instance, rwvcs_instance_ptr_t);
  RW_ASSERT(name);
  RW_ASSERT(m_component);
  inventory = instance->pb_rwmanifest->inventory;

  if (inventory == NULL) {
    return RW_STATUS_NOTFOUND;
  }
  // Search for a component by name in the selected manifest profile
  *m_component = NULL;
  for (i = 0 ; i < inventory->n_component ; i++) {
    if (!strcmp(inventory->component[i]->component_name, name)) {
      *m_component = inventory->component[i];
      break;
    }
  }

  if (*m_component == NULL) {
    return RW_STATUS_NOTFOUND;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rwvcs_manifest_event_lookup(
    char * name,
    vcs_manifest_event_list * m_eventlist,
    vcs_manifest_event ** m_event)
{
  int i;

  if (!m_eventlist)
    return RW_STATUS_NOTFOUND;

  // Search for an event by name in the selected manifest profile
  *m_event = NULL;
  for (i = 0 ; i < m_eventlist->n_event ; i++) {
    if (!strcmp(m_eventlist->event[i]->name, name)) {
      *m_event = m_eventlist->event[i];
      break;
    }
  }
  if (*m_event == NULL) {
    return RW_STATUS_FAILURE;
  }

  // Return the status of the lookup operation
  return RW_STATUS_SUCCESS;
}

bool rwvcs_manifest_have_component(rwvcs_instance_ptr_t rwvcs, const char * name)
{
  for (int i = 0; i < rwvcs->pb_rwmanifest->inventory->n_component; ++i) {
    char * src = rwvcs->pb_rwmanifest->inventory->component[i]->component_name;
    if (!strcmp(name, src))
      return true;
  }

  return false;
}

int
rwvcs_manifest_config_ready_count(rwvcs_instance_ptr_t rwvcs)
{
  rw_status_t status;
  int count = 0;
  int bootstarp_count = 0;
  vcs_manifest *rw_manifest = rwvcs->pb_rwmanifest;
  
  // count the number of instances in bootstrp VM with config_ready

  if (!rw_manifest) {
    return count;
  }

  int idx;
  vcs_manifest_bootstrap *bootstrap_phase = rw_manifest->bootstrap_phase;
  if (bootstrap_phase->rwvm && bootstrap_phase->rwvm->n_instances) {
    for (idx= 0; idx < bootstrap_phase->rwvm->n_instances; idx++) {
      if (bootstrap_phase->rwvm->instances[idx]->has_config_ready &&
          bootstrap_phase->rwvm->instances[idx]->config_ready) {
        bootstarp_count ++;
      }
    }
  }

  vcs_manifest_inventory *inventory = rw_manifest->inventory;
  if (inventory && inventory->n_component) {
    RwManifest__YangData__RwManifest__Manifest__Inventory__Component **component = inventory->component;
    for (idx = 0; idx < inventory->n_component; idx++) {
      RW_ASSERT(component[idx]->has_component_type);
      switch (component[idx]->component_type) {
        case RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION:{
          RwManifest__YangData__RwManifest__Manifest__Inventory__Component__Rwcollection *rwcollection = component[idx]->rwcollection;
          if (!rwcollection->event_list)
            continue;
          int e_idx;
          for (e_idx = 0;e_idx < rwcollection->event_list->n_event; e_idx++) {
            int a_idx;
            for (a_idx = 0; a_idx < rwcollection->event_list->event[e_idx]->n_action; a_idx++) {
              if (rwcollection->event_list->event[e_idx]->action[a_idx]->start &&
                  rwcollection->event_list->event[e_idx]->action[a_idx]->start->has_config_ready &&
                  rwcollection->event_list->event[e_idx]->action[a_idx]->start->config_ready) {
                count ++;
                vcs_manifest_component * mdef;
                status = rwvcs_manifest_component_lookup(rwvcs, rwcollection->event_list->event[e_idx]->action[a_idx]->start->component_name, &mdef);
                if (status == RW_STATUS_SUCCESS) {
                  if (mdef->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
                    count += bootstarp_count;
                  }
                }
              }
            }
          }
        }
        break;
        case RWVCS_TYPES_COMPONENT_TYPE_RWVM:{
          RwManifest__YangData__RwManifest__Manifest__Inventory__Component__Rwvm *rwvm = component[idx]->rwvm;
          RW_ASSERT(rwvm->event_list);
          int e_idx;
          for (e_idx = 0;e_idx < rwvm->event_list->n_event; e_idx++) {
            int a_idx;
            for (a_idx = 0; a_idx < rwvm->event_list->event[e_idx]->n_action; a_idx++) {
              if (rwvm->event_list->event[e_idx]->action[a_idx]->start &&
                  rwvm->event_list->event[e_idx]->action[a_idx]->start->has_config_ready &&
                  rwvm->event_list->event[e_idx]->action[a_idx]->start->config_ready) {
                count ++;
              }
            }
          }
        }
        break;
        case RWVCS_TYPES_COMPONENT_TYPE_RWPROC:{
          RwManifest__YangData__RwManifest__Manifest__Inventory__Component__Rwproc *rwproc = component[idx]->rwproc;
          int t_idx;
          for (t_idx = 0;t_idx < rwproc->n_tasklet; t_idx++) {
            if (rwproc->tasklet[t_idx]->has_config_ready &&
                rwproc->tasklet[t_idx]->config_ready) {
              count ++;
            }
          }
        } 
        break;
        case RWVCS_TYPES_COMPONENT_TYPE_PROC:
        case RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET:
        default:
        break;
      }
    }
  }

  count ++; //TermIO is not part of manifest

  return count;

}

static int rwvcs_manifest_append_vm_list (rwvcs_instance_ptr_t rwvcs,
                                          char *lead_ip,
                                          vcs_manifest_action_start *start,
                                          rwvcs_manifest_vm_list_t **vm_list)
{
  vcs_manifest_component * mdef;
  rw_status_t status = rwvcs_manifest_component_lookup(
      rwvcs, 
      start->component_name, 
      &mdef);
  if ((status != RW_STATUS_SUCCESS) ||
      (mdef->component_type != RWVCS_TYPES_COMPONENT_TYPE_RWVM)) {
    return 0;
  }
  bool lead_vm = false;
  if (!strcmp(start->ip_address, lead_ip)) {
    lead_vm = true;
  }
  RW_ASSERT(vm_list);
  rwvcs_manifest_vm_list_t *vm_list_entry = NULL;
  if (!(*vm_list)) {
    (*vm_list) = RW_MALLOC0_TYPE(sizeof(rwvcs_manifest_vm_list_t), rwvcs_manifest_vm_list_t);
    vm_list_entry = (*vm_list);
  }
  else {
    vm_list_entry = RW_MALLOC0_TYPE(sizeof(rwvcs_manifest_vm_list_t), rwvcs_manifest_vm_list_t);
    if (lead_vm) {
      vm_list_entry->vm_list_next = (*vm_list);
      (*vm_list) = vm_list_entry;
      fprintf (stderr, "PRE -Appending %p \n", vm_list_entry);
    }
    else {
      rwvcs_manifest_vm_list_t *find_last_entry = (*vm_list);
      while (find_last_entry && find_last_entry->vm_list_next) find_last_entry = find_last_entry->vm_list_next;
      RW_ASSERT(find_last_entry);
      find_last_entry->vm_list_next = vm_list_entry;
    }
  }
  snprintf(vm_list_entry->vm_name, sizeof(vm_list_entry->vm_name), "%s", start->component_name);
  snprintf(vm_list_entry->vm_ip_address, sizeof(vm_list_entry->vm_ip_address), "%s", start->ip_address);
  
  return 1;
}

static int rwvcs_manifest_process_event_list (rwvcs_instance_ptr_t rwvcs,
                                              char *lead_ip,
                                              vcs_manifest_event_list *event_list,
                                              rwvcs_manifest_vm_list_t **vm_list)
{
  int count = 0;
  if (!event_list)
    return count;

  int e_idx;
  for (e_idx = 0;e_idx < event_list->n_event; e_idx++) {
    int a_idx;
    for (a_idx = 0; a_idx < event_list->event[e_idx]->n_action; a_idx++) {
      if (event_list->event[e_idx]->action[a_idx]->start) {
        count += rwvcs_manifest_append_vm_list(
            rwvcs, 
            lead_ip,
            event_list->event[e_idx]->action[a_idx]->start,
            vm_list);
      }
    }
  }
  if (count) {
    rwvcs_manifest_vm_list_t *p_list = (*vm_list);
    while (p_list) {
      fprintf (stderr, "final %p -- %s:%s\n",  p_list, p_list->vm_name, p_list->vm_ip_address);
      p_list = p_list->vm_list_next;
    }
  }
  return count;
}

int  rwvcs_manifest_rwvm_list(rwvcs_instance_ptr_t rwvcs,
                              char                 *lead_ip,
                              rwvcs_manifest_vm_list_t **vm_list)
{
  int count = 0;
  vcs_manifest *rw_manifest = rwvcs->pb_rwmanifest;
  
  if (!rw_manifest) {
    return count;
  }

  int idx;
  vcs_manifest_inventory *inventory = rw_manifest->inventory;
  if (inventory && inventory->n_component) {
    vcs_manifest_component **component = inventory->component;
    for (idx = 0; idx < inventory->n_component; idx++) {
      RW_ASSERT(component[idx]->has_component_type);
      switch (component[idx]->component_type) {
        case RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION: {
          count += rwvcs_manifest_process_event_list (rwvcs,
                                                      lead_ip,
                                                      component[idx]->rwcollection->event_list,
                                                      vm_list);
        }
        break;
        case RWVCS_TYPES_COMPONENT_TYPE_RWVM: {
          count += rwvcs_manifest_process_event_list (rwvcs,
                                                      lead_ip,
                                                      component[idx]->rwvm->event_list,
                                                      vm_list);
        }
        break;
        case RWVCS_TYPES_COMPONENT_TYPE_RWPROC: 
        case RWVCS_TYPES_COMPONENT_TYPE_PROC:
        case RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET:
        default:
        break;
      }
    }
  }

  return count;
}

bool
rwvcs_manifest_is_lss_mode(rwvcs_instance_ptr_t rwvcs)
{
  vcs_manifest_bootstrap * bootstrap = rwvcs->pb_rwmanifest->bootstrap_phase;
  return(bootstrap->n_ip_addrs_list > 1);
}

char *
rwvcs_manifest_get_local_mgmt_addr(vcs_manifest_bootstrap *bootstrap)
{
  struct ifaddrs *ifap;
  int r = getifaddrs(&ifap);
  RW_ASSERT(r == 0);

  char *addrs_found = NULL;
  for (struct ifaddrs * it = ifap; !addrs_found && it; it = it->ifa_next) {
    struct sockaddr_in * addr = (struct sockaddr_in *)it->ifa_addr;
    int indx;
    for(indx = 0; indx < bootstrap->n_ip_addrs_list; indx++) {
      if (!strncmp(bootstrap->ip_addrs_list[indx]->ip_addr, inet_ntoa(addr->sin_addr), 15)) {
        addrs_found = bootstrap->ip_addrs_list[indx]->ip_addr;
        break;
      }
    }
  }
  freeifaddrs(ifap);

  return addrs_found;
}

bool
rwvcs_manifest_is_mgmt_vm (rwvcs_instance_ptr_t rwvcs, char *component_name)
{
  if (!component_name) {
    return true;
  }
  if (!rwvcs->pb_rwmanifest->init_phase->environment->active_component) {
    return false;
  }

  rw_status_t status = rwvcs_variable_list_evaluate(
      rwvcs,
      rwvcs->pb_rwmanifest->init_phase->environment->active_component->n_python_variable,
      rwvcs->pb_rwmanifest->init_phase->environment->active_component->python_variable);

  char c_name[1024];
  status = rwvcs_variable_evaluate_str(
      rwvcs,
      "$rw_component_name",
      c_name,
      sizeof(c_name));
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
  }
  if (!strcmp(component_name, c_name)) {
    return true;
  }
  bzero(&c_name[0], sizeof(c_name));
  if (rwvcs->pb_rwmanifest->bootstrap_phase->n_ip_addrs_list > 1) {
    status = rwvcs_variable_list_evaluate(
        rwvcs,
        rwvcs->pb_rwmanifest->init_phase->environment->standby_component->n_python_variable,
        rwvcs->pb_rwmanifest->init_phase->environment->standby_component->python_variable);

    status = rwvcs_variable_evaluate_str(
        rwvcs,
        "$rw_component_name",
        c_name,
        sizeof(c_name));
    if (status != RW_STATUS_SUCCESS) {
      RW_CRASH();
    }
    if (!strcmp(component_name, c_name)) {
      return true;
    }
  }
  return false;
}

void rwvcs_manifest_setup_mgmt_info (
    rwvcs_instance_ptr_t rwvcs,
    bool mgmt_vm,
    const char           *ip_address)
{
  rwvcs_mgmt_info_t *mgmt_info = &rwvcs->mgmt_info;
  mgmt_info->mgmt_vm = mgmt_vm;
  mgmt_info->unique_ports = true;

  vcs_manifest_bootstrap * bootstrap = rwvcs->pb_rwmanifest->bootstrap_phase;
  if (!bootstrap)
    return;
  bootstrap->ip_addrs_list[bootstrap->n_ip_addrs_list] = NULL;
  int indx, store_indx = 0;
  for (indx = 0; (indx < RWVCS_ZK_SERVER_CLUSTER) && bootstrap->ip_addrs_list[store_indx]; indx++) {
    char *zk_server_addr = NULL;
    int zk_client_port = 0;
    bool zk_server_start = false;
    bool zk_client_connect = false;
    zk_client_port = 2181 + indx;
    RW_ASSERT(store_indx < bootstrap->n_ip_addrs_list);
    zk_server_addr = bootstrap->ip_addrs_list[store_indx]->ip_addr;
    if (mgmt_vm && !strcmp(ip_address, zk_server_addr)) {
      zk_server_start = true;
    }

    char *local_addr = rwvcs_manifest_get_local_mgmt_addr(bootstrap);
    if (local_addr) {
      if (!strcmp(local_addr, zk_server_addr)) {
        zk_client_connect = true;
      }
    }
    else if (!mgmt_vm) {
      zk_client_connect = true;
    }
    mgmt_info->zk_server_port_details[indx] = rwcal_zk_server_port_detail_alloc(
        zk_server_addr,
        zk_client_port,
        indx+1,
        zk_server_start,
        zk_client_connect);
    if (!((bootstrap->n_ip_addrs_list == 2) && !indx)) {
      store_indx++;
    }
  }
  mgmt_info->zk_server_port_details[indx] = NULL;
}
