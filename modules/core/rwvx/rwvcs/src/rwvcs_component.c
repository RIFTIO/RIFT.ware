
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <rw-base.pb-c.h>

#include "rwvcs_component.h"
#include "rwvcs_internal.h"
#include "rwvcs_rwzk.h"
#include "rwvx.h"

/*
 * Link/Unlink a child component to a parent.
 *
 * @param rwvcs           - rwvcs instance
 * @param component_info  - component of which to modify parent
 * @param parent_id       - instance-id of the parent to which the component is a child, if
 *                          NULL, then the child will be unlinked from the parent
 */
static void rwvcs_component_link_parent(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component_info,
    const char * parent_id);

static rw_component_info * rwvcs_component_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    rw_component_type component_type,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * info;
  rw_status_t status;

  info = (rw_component_info *)malloc(sizeof(rw_component_info));
  RW_ASSERT(info);

  rw_component_info__init(info);

  info->has_component_type = true;
  info->component_type = component_type;

  info->component_name = strdup(component_name);
  RW_ASSERT(info->component_name);

  info->has_instance_id = true;
  info->instance_id = instance_id;

  info->instance_name = strdup(instance_name);
  RW_ASSERT(info->instance_name);

  info->has_state = true;
  info->state = RW_BASE_STATE_TYPE_STOPPED;

  if (parent_id) {
    info->rwcomponent_parent = strdup(parent_id);
    RW_ASSERT(info->rwcomponent_parent);
  }


  status = rwvcs_rwzk_node_create(rwvcs, info);
  RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_EXISTS);

  rwvcs_component_link_parent(rwvcs, info, parent_id);
  return info;
}

void rwvcs_component_add_child(
    rw_component_info * parent,
    const char * child_instance)
{
  // Make sure the child is not already there.
  for (int i = 0; i < parent->n_rwcomponent_children; ++i) {
    const char * current_child = parent->rwcomponent_children[i];
    if (!strncmp(current_child, child_instance, strlen(current_child)))
      goto done;
  }

  parent->n_rwcomponent_children++;
  parent->rwcomponent_children = realloc(
      parent->rwcomponent_children,
      parent->n_rwcomponent_children * sizeof(char *));
  RW_ASSERT(parent->rwcomponent_children);

  parent->rwcomponent_children[parent->n_rwcomponent_children-1] = strdup(child_instance);
  RW_ASSERT(parent->rwcomponent_children[parent->n_rwcomponent_children-1]);

done:
  return;
}

void rwvcs_component_delete_child(
    rw_component_info * parent,
    const char * child_instance)
{
  int i;
  char * last_child;

  for (i = 0; i < parent->n_rwcomponent_children; ++i) {
    if (!strncmp(parent->rwcomponent_children[i], child_instance, strlen(child_instance)))
      break;
  }

  if (i == parent->n_rwcomponent_children)
    goto done;

  // Now i is pointing to the child we want to remove, so we can just swap the
  // pointer with the last child in the list.
  last_child = parent->rwcomponent_children[i];
  parent->rwcomponent_children[i] = parent->rwcomponent_children[parent->n_rwcomponent_children - 1];
  parent->n_rwcomponent_children--;

  free(last_child);

done:
  return;
}

void rwvcs_component_delete(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component)
{
  rw_status_t status;

  if (component->has_recovery_action 
      && (component->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_RESTART)) {
    component->state = RW_BASE_STATE_TYPE_TO_RECOVER;
    status = rwvcs_rwzk_node_update(
        rwvcs, 
        component);
  }
  else {
    if (component->rwcomponent_parent) {
      status = rwvcs_rwzk_delete_child(
        rwvcs,
        component->rwcomponent_parent,
        component->component_name,
        component->instance_id);
      RW_ASSERT(status == RW_STATUS_SUCCESS || RW_STATUS_NOTFOUND);
    }

    status = rwvcs_rwzk_delete_node(rwvcs, component->instance_name);
    RW_ASSERT(status == RW_STATUS_SUCCESS || RW_STATUS_NOTFOUND);
  }
}

static void rwvcs_component_link_parent(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component_info,
    const char * parent_id)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  if (component_info->rwcomponent_parent
      && strncmp(component_info->rwcomponent_parent, parent_id, strlen(parent_id))) {
    status = rwvcs_rwzk_delete_child(
        rwvcs,
        component_info->rwcomponent_parent,
        component_info->component_name,
        component_info->instance_id);
    free(component_info->rwcomponent_parent);
    component_info->rwcomponent_parent = NULL;
  }

  if (parent_id) {
    status = rwvcs_rwzk_add_child(
        rwvcs,
        parent_id,
        component_info->instance_name);
    component_info->rwcomponent_parent = strdup(parent_id);
    RW_ASSERT(component_info->rwcomponent_parent)
  }

  RW_ASSERT(status == RW_STATUS_SUCCESS);
}

rw_component_info * rwvcs_rwcollection_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * ret;

  ret = rwvcs_component_alloc(
      rwvcs,
      parent_id,
      RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION,
      component_name,
      instance_id,
      instance_name);

  ret->collection_info = (rw_collection_info *)malloc(sizeof(rw_collection_info));
  if (!ret->collection_info) {
    RW_ASSERT(ret->collection_info);
    protobuf_free(ret);
    return NULL;
  }

  rw_collection_info__init(ret->collection_info);

  return ret;
}

rw_component_info * rwvcs_rwvm_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * vm;

  vm = rwvcs_component_alloc(
      rwvcs,
      parent_id,
      RWVCS_TYPES_COMPONENT_TYPE_RWVM,
      component_name,
      instance_id,
      instance_name);
  vm->vm_info = (rw_vm_info *)malloc(sizeof(rw_vm_info));
  RW_ASSERT(vm->vm_info);
  rw_vm_info__init(vm->vm_info);

  return vm;
}

rw_component_info * rwvcs_rwproc_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * proc;

  proc = rwvcs_component_alloc(
      rwvcs,
      parent_id,
      RWVCS_TYPES_COMPONENT_TYPE_RWPROC,
      component_name,
      instance_id,
      instance_name);
  proc->proc_info = (rw_proc_info *)malloc(sizeof(rw_proc_info));
  RW_ASSERT(proc->proc_info);
  rw_proc_info__init(proc->proc_info);

  return proc;
}

rw_component_info * rwvcs_proc_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * proc;

  proc = rwvcs_component_alloc(
      rwvcs,
      parent_id,
      RWVCS_TYPES_COMPONENT_TYPE_PROC,
      component_name,
      instance_id,
      instance_name);
  proc->proc_info = (rw_proc_info *)malloc(sizeof(rw_proc_info));
  RW_ASSERT(proc->proc_info);
  rw_proc_info__init(proc->proc_info);

  return proc;
}

rw_component_info * rwvcs_rwtasklet_alloc(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_id,
    const char * component_name,
    uint32_t instance_id,
    const char * instance_name)
{
  rw_component_info * component;;

  component = rwvcs_component_alloc(
      rwvcs,
      parent_id,
      RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET,
      component_name,
      instance_id,
      instance_name);

  return component;
}



