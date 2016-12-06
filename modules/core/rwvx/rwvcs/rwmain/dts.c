
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

#include <sys/statvfs.h>

#include <riftware-version.h>
#include <rw-base.pb-c.h>
#include <rw-ha.pb-c.h>
#include <rw-debug.pb-c.h>
#include <rwdts.h>
#include <rwvcs.h>
#include <rwvcs_component.h>
#include <rwvcs_rwzk.h>
#include <rwdts_int.h>
#include <rwvcs_internal.h>

#include "rwmain.h"

// Typedef so I can fit a malloc on less than 3 lines.
typedef RWPB_T_MSG(RwBase_data_Vcs_Info_Components_ComponentInfo) tasklet_component_info;

static rwdts_member_rsp_code_t on_tasklet_info(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;
  rw_component_info * component;
  rw_component_info * parent = NULL;
  char * this_instance;
  size_t max_components;
  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;
  RWPB_T_MSG(RwBase_data_Vcs_Info) info;
  const RWPB_T_PATHSPEC(RwBase_data_Vcs_Info) *key =
      RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_Info);
  RWPB_T_MSG(RwBase_data_Vcs_Info_Components) components;

  RW_ASSERT(xact_info);

  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  rwvcs = rwmain->rwvx->rwvcs;

  // Prepare the output containers
  RWPB_F_MSG_INIT(RwBase_data_Vcs_Info, &info);
  RWPB_F_MSG_INIT(RwBase_data_Vcs_Info_Components, &components);

  // Lookup the current instance
  component = (rw_component_info *)malloc(sizeof(rw_component_info ));
  RW_ASSERT(component);

  this_instance = to_instance_name(rwmain->component_name, rwmain->instance_id);
  status = rwvcs_rwzk_lookup_component(rwvcs, this_instance, component);
  //RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS) {
    free(component);
    component = NULL;
    goto err;
  }

  if (!component->vm_info || !component->vm_info->leader) {
    free(this_instance);
    protobuf_free(component);
    return RWDTS_ACTION_OK;
  }

  parent = (rw_component_info *)malloc(sizeof(rw_component_info));
  RW_ASSERT(parent);

  status = rwvcs_rwzk_lookup_component(rwvcs, component->rwcomponent_parent, parent);
  //RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS)
    goto err;


  info.components = &components;

  components.component_info = (tasklet_component_info**)malloc(
      sizeof(tasklet_component_info *) * 10);
  RW_ASSERT(components.component_info);
  max_components = 10;


  /*
   * Run through all of the rhildren of the parent of the leader.  If the child is
   * not a collection type (cluster/colony) then this VM is responsible for reporting
   * all of the information about that component and any of its children.
   */
  for (int i = 0; i < parent->n_rwcomponent_children; ++i) {
    rw_component_info child_info;
    rw_component_info ** descendents;
    char * child = parent->rwcomponent_children[i];
    size_t new_components;

    status = rwvcs_rwzk_lookup_component(rwvcs, child, &child_info);
    if (status != RW_STATUS_SUCCESS)
      goto err;

    if (child_info.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION) {
      protobuf_free_stack(child_info);
      continue;
    }

    // The descendents list will contain this, appropriately malloced.
    protobuf_free_stack(child_info);

    status = rwvcs_rwzk_descendents_component(rwvcs, child, &descendents);
    if (status != RW_STATUS_SUCCESS) {
      goto err;
    }

    for (new_components = 0; descendents[new_components]; ++new_components) {;}

    // Increase buffer if necessary
    if ((components.n_component_info + new_components + 1) >= max_components) {
      max_components += new_components;

      components.component_info = (tasklet_component_info **)realloc(
          components.component_info,
          sizeof(tasklet_component_info **) * max_components);
      RW_ASSERT(components.component_info)
      if (!components.component_info) {
        for (size_t i = 0; descendents[i]; ++i) {
          protobuf_free(descendents[i]);
          free(descendents);
          status = RW_STATUS_FAILURE;
          goto err;
        }
      }
    }

    for (size_t i = 0; descendents[i]; ++i) {
      components.component_info[components.n_component_info] = descendents[i];
      components.n_component_info++;
    }

    free(descendents);
  }

  // Appending these two here makes deallocating everything from the error handler easier.
  if ((components.n_component_info + 3) >= max_components) {
    max_components += 3;

    components.component_info = (tasklet_component_info **)realloc(
        components.component_info,
        sizeof(tasklet_component_info **) * max_components);
    RW_ASSERT(components.component_info);
    if (!components.component_info) {
      status = RW_STATUS_FAILURE;
      goto err;
    }
  }

  components.component_info[components.n_component_info] = component;
  components.n_component_info++;
  component = NULL;

  components.component_info[components.n_component_info] = parent;
  components.n_component_info++;
  parent = NULL;

  memset (&rsp, 0, sizeof (rsp));
  // Finally, send the result.
  rsp.msgs = msgs;
  rsp.msgs[0] = &info.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)key;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  return RWDTS_ACTION_OK;


err:
  if (component)
    protobuf_free(component);

  if (parent)
    free(parent);

  info.components = NULL;
  protobuf_free_stack(info);
  protobuf_free_stack(components);

  return RWDTS_ACTION_NOT_OK;
}

static rwdts_member_rsp_code_t on_tasklet_resource(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  rwdts_member_rsp_code_t dts_ret;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;

  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  // Lookup the current instance
  char * self_instance;
  char * this_instance;
  self_instance = to_instance_name(rwmain->component_name, rwmain->instance_id);

  char * specific_instance =  msg &&
                         ((RWPB_T_MSG(RwDebug_data_RwDebug_Resource)*)msg)->n_tasklet  &&
                         ((RWPB_T_MSG(RwDebug_data_RwDebug_Resource)*)msg)->tasklet[0] ?
                         ((RWPB_T_MSG(RwDebug_data_RwDebug_Resource)*)msg)->tasklet[0]->name
                         : NULL;

  rw_component_info self;
  rw_component_info component;
  status = rwvcs_rwzk_lookup_component(rwvcs, self_instance, &self);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  // Prepare the output containers
  RWPB_T_MSG(RwDebug_data_RwDebug_Resource) resource;
  const RWPB_T_PATHSPEC(RwDebug_data_RwDebug_Resource) *key
      = RWPB_G_PATHSPEC_VALUE (RwDebug_data_RwDebug_Resource);

  RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource, &resource);

  RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet) **tasklet_ptr_ptr = NULL;
  RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet) *tasklet_ptr = NULL;

  size_t num_components;
  rwsched_tasklet_ptr_t rwsched_tasklet_info = NULL;

  if (self.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET) {
    this_instance = self_instance;
    num_components = self.n_rwcomponent_children;
    rwsched_tasklet_info = rwmain->tasklet_info->rwsched_tasklet_info;
    goto _fill_info;

  }
  else {
    free(self_instance);
  }
  self_instance = NULL;

  for (num_components = 0; num_components<self.n_rwcomponent_children; ++num_components) {
    struct rwmain_tasklet * rt = NULL;

    status = rwvcs_rwzk_lookup_component(rwvcs, self.rwcomponent_children[num_components], &component);
    if (component.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET) {
      continue;
    }
    protobuf_c_message_free_unpacked_usebody(NULL, &component.base);

    status = RW_SKLIST_LOOKUP_BY_KEY(&(rwmain->tasklets), &self.rwcomponent_children[num_components], &rt);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    if (!rt->tasklet_info)
      continue;

    rwsched_tasklet_info = rt->tasklet_info->rwsched_tasklet_info;
    this_instance = strdup(self.rwcomponent_children[num_components]);

_fill_info:
    if (!(!specific_instance ||
          !strcmp(specific_instance, "") ||
          !strcmp(specific_instance, this_instance))) {
      continue;
    }

    tasklet_ptr_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet)**)realloc(tasklet_ptr_ptr, (resource.n_tasklet+1)*sizeof(*tasklet_ptr_ptr));
    tasklet_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet)*)malloc(sizeof(*tasklet_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource_Tasklet, tasklet_ptr);
    tasklet_ptr_ptr[resource.n_tasklet] = tasklet_ptr;
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource_Tasklet, tasklet_ptr);
    resource.tasklet = tasklet_ptr_ptr;
    resource.n_tasklet++;
    tasklet_ptr->name = this_instance;
    this_instance = NULL;

    RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Memory) *memory_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Memory)*)malloc(sizeof(*memory_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource_Tasklet_Memory, memory_ptr);
    tasklet_ptr->memory = memory_ptr;
    memory_ptr->has_allocated = TRUE;
    memory_ptr->has_chunks = TRUE;
    if (rwsched_tasklet_info) {
      memory_ptr->allocated = rwsched_tasklet_info->counters.memory_allocated;
      memory_ptr->chunks = rwsched_tasklet_info->counters.memory_chunks;
    }
    else memory_ptr->allocated = -1;

    RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler) *scheduler_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler)*)malloc(sizeof(*scheduler_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler, scheduler_ptr);
    tasklet_ptr->scheduler = scheduler_ptr;

    RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler_Counters) *counters_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler_Counters)*)malloc(sizeof(*counters_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Resource_Tasklet_Scheduler_Counters, counters_ptr);
    scheduler_ptr->counters = counters_ptr;

    rwsched_tasklet_counters_t rwsched_counters;
    rwsched_tasklet_get_counters(rwmain->tasklet_info->rwsched_tasklet_info, &rwsched_counters);

    counters_ptr->has_sources = TRUE;
    counters_ptr->sources = rwsched_counters.sources;
    counters_ptr->has_queues = TRUE;
    counters_ptr->queues = rwsched_counters.queues;
    counters_ptr->has_sthread_queues = TRUE;
    counters_ptr->sthread_queues = rwsched_counters.sthread_queues;
    counters_ptr->has_sockets = TRUE;
    counters_ptr->sockets = rwsched_counters.sockets;
    counters_ptr->has_socket_sources = TRUE;
    counters_ptr->socket_sources = rwsched_counters.socket_sources;
  }
  protobuf_c_message_free_unpacked_usebody(NULL, &self.base);

  memset (&rsp, 0, sizeof (rsp));

  dts_ret = RWDTS_ACTION_OK;
  if (tasklet_ptr_ptr ) {
    // Finally, send the result.
    rsp.msgs = msgs;
    rsp.msgs[0] = &resource.base;
    rsp.n_msgs = 1;
    rsp.ks = (rw_keyspec_path_t*)key;
    rsp.evtrsp = RWDTS_EVTRSP_ACK;

    status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    protobuf_free_stack(resource);
  }
  return dts_ret;
}

//This is for debugging RIFT-6763 - Investigate slowness of heap retrieval
//#define  DIRECT_PRINT 1
static rwdts_member_rsp_code_t on_tasklet_heap(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  rwdts_member_rsp_code_t dts_ret;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;

  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  // Lookup the current instance
  char * self_instance;
  char * this_instance;
  self_instance = to_instance_name(rwmain->component_name, rwmain->instance_id);

  char * specific_instance = msg &&
                        ((RWPB_T_MSG(RwDebug_data_RwDebug_Heap)*)msg)->n_tasklet &&
                        ((RWPB_T_MSG(RwDebug_data_RwDebug_Heap)*)msg)->tasklet[0] ?
                        ((RWPB_T_MSG(RwDebug_data_RwDebug_Heap)*)msg)->tasklet[0]->name
                        : NULL;

  rw_component_info self;
  rw_component_info component;
  status = rwvcs_rwzk_lookup_component(rwvcs, self_instance, &self);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

#ifndef DIRECT_PRINT
  // Prepare the output containers
  ProtobufCMessage * msgs[1];
  RWPB_T_MSG(RwDebug_data_RwDebug_Heap) heap;
  const RWPB_T_PATHSPEC(RwDebug_data_RwDebug_Heap) *key
      = RWPB_G_PATHSPEC_VALUE(RwDebug_data_RwDebug_Heap);
  RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap, &heap);

  RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet) **tasklet_ptr_ptr = NULL;
  RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet) *tasklet_ptr = NULL;
#endif

  size_t num_components;
  rwsched_tasklet_ptr_t rwsched_tasklet_info = NULL;

  if (self.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET) {
    this_instance = self_instance;
    num_components = self.n_rwcomponent_children; // to exit from the for loop
    rwsched_tasklet_info = rwmain->tasklet_info->rwsched_tasklet_info;
    goto _fill_info;
  } else {
    free(self_instance);
  }
  self_instance = NULL;

  for (num_components = 0; num_components<self.n_rwcomponent_children; ++num_components) {
    struct rwmain_tasklet * rt = NULL;

    status = rwvcs_rwzk_lookup_component(rwvcs, self.rwcomponent_children[num_components], &component);
    if (component.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET) {
      continue;
    }
    protobuf_c_message_free_unpacked_usebody(NULL, &component.base);

    status = RW_SKLIST_LOOKUP_BY_KEY(&(rwmain->tasklets), &self.rwcomponent_children[num_components], &rt);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    if (!rt->tasklet_info)
      continue;

    rwsched_tasklet_info = rt->tasklet_info->rwsched_tasklet_info;
    this_instance = strdup(self.rwcomponent_children[num_components]);

_fill_info:
    if (!(!specific_instance ||
          !strcmp(specific_instance, "") ||
          !strcmp(specific_instance, this_instance))) {
      continue;
    }
    if (!rwsched_tasklet_info) {
      continue;
    }

#ifndef DIRECT_PRINT
    tasklet_ptr_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet)**)realloc(tasklet_ptr_ptr, (heap.n_tasklet+1)*sizeof(*tasklet_ptr_ptr));
    tasklet_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet)*)malloc(sizeof(*tasklet_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet, tasklet_ptr);
    tasklet_ptr_ptr[heap.n_tasklet] = tasklet_ptr;
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet, tasklet_ptr);
    heap.tasklet = tasklet_ptr_ptr;
    heap.n_tasklet++;
    tasklet_ptr->name = this_instance;
    this_instance = NULL;
#else
    fprintf(stderr, "Tasklet-name: %s\n", this_instance);
#endif

#define MAX_PER_TASKLET 200
#define MAX_PER_SINGLE_TASKLET MAX_PER_TASKLET*50
    unsigned int how_many = (!g_malloc_intercepted ? MAX_PER_SINGLE_TASKLET : !specific_instance ? MAX_PER_TASKLET : MAX_PER_SINGLE_TASKLET);
    RW_RESOURCE_TRACK_HANDLE rwresource_track_handle =  g_rwresource_track_handle;
    //g_rwresource_track_handle = 0;
    RW_TA_RESOURCE_TRACK_DUMP_SINK s = (RW_TA_RESOURCE_TRACK_DUMP_SINK)RW_MALLOC0(sizeof(*s)*how_many);
    RW_RESOURCE_TRACK_DUMP_STRUCT(rwsched_tasklet_info->rwresource_track_handle, s, how_many);
    RW_ASSERT(s[0].sink_used <= how_many);

#ifndef DIRECT_PRINT
    RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation) **allocation_ptr_ptr = NULL;
    RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation) *allocation_ptr = NULL;

    RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Memory) *memory_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Memory)*)malloc(sizeof(*memory_ptr));
    RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet_Memory, memory_ptr);
    tasklet_ptr->memory = memory_ptr;
#endif

    //fprintf(stderr, "%s RW_RESOURCE_TRACK_%d\n", tasklet_ptr->name, s[0].sink_used);

    unsigned int i=0;
    unsigned long memory_allocated;
    memory_allocated = 0;
    for (i=0; i<s[0].sink_used; i++) {
      if (s[i].obj_type == RW_RAW_OBJECT || s[i].obj_type == RW_CF_OBJECT || s[i].obj_type == RW_MALLOC_OBJECT) {
        memory_allocated += s[i].size;
        if (!g_malloc_intercepted && i>MAX_PER_TASKLET) 
          continue;
#ifndef DIRECT_PRINT
        tasklet_ptr->allocation = \
        allocation_ptr_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation)**)realloc(allocation_ptr_ptr, sizeof(*allocation_ptr_ptr)*(tasklet_ptr->n_allocation+1));
        RW_ASSERT(allocation_ptr_ptr != NULL);
        allocation_ptr_ptr[tasklet_ptr->n_allocation] = \
        allocation_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation)*)malloc(sizeof(*allocation_ptr));
        RW_ASSERT(allocation_ptr != NULL);
        tasklet_ptr->n_allocation++;
        RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet_Allocation, allocation_ptr);
        allocation_ptr->has_address = TRUE;
        allocation_ptr->address = (uint64_t)s[i].sink;
        //allocation_ptr->type = (char*)s[i].name;
        allocation_ptr->type = strdup(s[i].name);
        allocation_ptr->has_size = TRUE;
        allocation_ptr->size = s[i].size;
#else
        fprintf(stderr, "  allocation:\n");
        fprintf(stderr, "    size: %-10u type: %s\n", s[i].size, s[i].name);
#endif

        if (s[i].obj_type == RW_MALLOC_OBJECT) {
          RW_ASSERT(s[i].obj_type == RW_MALLOC_OBJECT);
#ifndef DIRECT_PRINT
          RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller) **caller_ptr_ptr = NULL;
          RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller) *caller_ptr = NULL;
#endif
          int j; for (j=0; j<RW_RESOURCE_TRACK_MAX_CALLERS && s[i].callers[j]; j++) {
#ifndef DIRECT_PRINT
            allocation_ptr->caller = \
            caller_ptr_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller)**)realloc(caller_ptr_ptr, sizeof(*caller_ptr_ptr)*(allocation_ptr->n_caller+1));
            RW_ASSERT(caller_ptr_ptr != NULL);
            caller_ptr_ptr[allocation_ptr->n_caller] = \
            caller_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller)*)malloc(sizeof(*caller_ptr));
            RW_ASSERT(caller_ptr != NULL);
            RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller, caller_ptr);
            if (g_heap_decode_using == RW_DEBUG_DECODE_TYPE_BTRACE) {
              caller_ptr->info = rw_btrace_get_proc_name(s[i].callers[j]);
            } else if (g_heap_decode_using == RW_DEBUG_DECODE_TYPE_UNWIND) {
              caller_ptr->info = rw_unw_get_proc_name(s[i].callers[j]);
            } else {
              RW_CRASH();
            }
            allocation_ptr->n_caller++;
#else
            char *str;
            if (g_heap_decode_using == RW_DEBUG_DECODE_TYPE_BTRACE) {
              str = rw_btrace_get_proc_name(s[i].callers[j]);
            } else if (g_heap_decode_using == RW_DEBUG_DECODE_TYPE_UNWIND) {
              str = rw_unw_get_proc_name(s[i].callers[j]);
            } else {
              RW_CRASH();
            }
            fprintf(stderr, "    caller%u: %s\n", j, str);
            free(str);
#endif
          }
        } else if (s[i].loc && (s[i].obj_type == RW_RAW_OBJECT || s[i].obj_type == RW_CF_OBJECT)) {
          RW_ASSERT(s[i].obj_type == RW_RAW_OBJECT ||
                    s[i].obj_type == RW_CF_OBJECT);
#ifndef DIRECT_PRINT
          RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller) **caller_ptr_ptr = NULL;
          RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller) *caller_ptr = NULL;
          allocation_ptr->caller = \
          caller_ptr_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller)**)realloc(caller_ptr_ptr, sizeof(*caller_ptr_ptr)*(allocation_ptr->n_caller+1));
          RW_ASSERT(caller_ptr_ptr != NULL);
          caller_ptr_ptr[allocation_ptr->n_caller] = \
          caller_ptr = (RWPB_T_MSG(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller)*)malloc(sizeof(*caller_ptr));
          RW_ASSERT(caller_ptr != NULL);
          RWPB_F_MSG_INIT(RwDebug_data_RwDebug_Heap_Tasklet_Allocation_Caller, caller_ptr);
          caller_ptr->info = strdup(s[i].loc);
          allocation_ptr->n_caller++;
#else
          fprintf(stderr, "    caller0: %s\n", s[i].loc);
#endif
        }
      }
    }

#ifndef DIRECT_PRINT
    memory_ptr->has_allocated = TRUE;
    memory_ptr->allocated = (!g_malloc_intercepted ? memory_allocated : rwsched_tasklet_info->counters.memory_allocated);
    memory_ptr->has_chunks = TRUE;
    memory_ptr->chunks = (!g_malloc_intercepted ? s[0].sink_used : rwsched_tasklet_info->counters.memory_chunks);

#endif
    if (s) RW_FREE(s);
    g_rwresource_track_handle = rwresource_track_handle;
  }
  protobuf_c_message_free_unpacked_usebody(NULL, &self.base);

  dts_ret = RWDTS_ACTION_OK;

  memset (&rsp, 0, sizeof (rsp));

#ifndef DIRECT_PRINT
  if (tasklet_ptr_ptr) {
    // Finally, send the result.
    rsp.msgs = msgs;
    rsp.msgs[0] = &heap.base;
    rsp.n_msgs = 1;
    rsp.ks = (rw_keyspec_path_t*)key;
    rsp.evtrsp = RWDTS_EVTRSP_ACK;
    status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    protobuf_free_stack(heap);
  }
#else
  dts_ret = RWDTS_ACTION_OK;
#endif
  return dts_ret;
}

static rwdts_member_rsp_code_t on_config_tasklet_heap(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_UPDATE)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  g_callstack_depth = msg && ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->has_depth ? 
                      ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->depth :
                      g_callstack_depth;

  g_heap_track_nth = msg && ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->has_track_nth ?
                      ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->track_nth :
                      g_heap_track_nth;

  g_heap_track_bigger_than = msg && ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->has_track_bigger_than ?
                      ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->track_bigger_than :
                      g_heap_track_bigger_than;

  g_heap_decode_using = msg && ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->has_decode_using ?
                      ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Heap)*)msg)->decode_using :
                      g_heap_decode_using;

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t on_config_tasklet_scheduler(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_UPDATE)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  if (msg && ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Scheduler)*)msg)->has_latency_threshold) {
    uint32_t thresh = ((RWPB_T_MSG(RwDebug_data_RwDebug_Settings_Scheduler)*)msg)->latency_threshold;
    rwsched_instance_set_latency_check(rwmain->tasklet_info->rwsched_instance, thresh);
  }

  return RWDTS_ACTION_OK;
}


/*
 * Allocate and fill a RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu) object which will then
 * be added to the vm_info object.
 *
 * @param vm_info - VCS Resource object to add cpu information to.
 */
static void fill_vcs_cpu(struct rwmain_gi * rwmain, RWPB_T_MSG(RwBase_VcsResource_Vm) * vm_info)
{
  size_t n_cpus;

  vm_info->cpu = (RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu) *)malloc(sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu)));
  RW_ASSERT(vm_info->cpu);
  RWPB_F_MSG_INIT(RwBase_VcsResource_Vm_Cpu,vm_info->cpu);

  vm_info->cpu->aggregate = (RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Aggregate) *)malloc(sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Aggregate)));
  RW_ASSERT(vm_info->cpu->aggregate);
  RWPB_F_MSG_INIT(RwBase_VcsResource_Vm_Cpu_Aggregate,vm_info->cpu->aggregate);

  vm_info->cpu->aggregate->has_user = true;
  vm_info->cpu->aggregate->has_system = true;
  vm_info->cpu->aggregate->has_idle = true;

  vm_info->cpu->aggregate->user = rwmain->sys.all_cpus.current.user;
  vm_info->cpu->aggregate->system = rwmain->sys.all_cpus.current.sys;
  vm_info->cpu->aggregate->idle= rwmain->sys.all_cpus.current.idle;

  for (n_cpus = 0; rwmain->sys.each_cpu[n_cpus]; ++n_cpus) {;}

  vm_info->cpu->individual = (RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Individual)**)malloc(
      sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Individual)*) * n_cpus);
  RW_ASSERT(vm_info->cpu->individual);
  vm_info->cpu->n_individual = n_cpus;

  for (int i = 0; rwmain->sys.each_cpu[i]; ++i) {
    vm_info->cpu->individual[i] = (RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Individual)*)malloc(
      sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Cpu_Individual)));
    RW_ASSERT(vm_info->cpu->individual[i]);
    RWPB_F_MSG_INIT(RwBase_VcsResource_Vm_Cpu_Individual, vm_info->cpu->individual[i]);

    vm_info->cpu->individual[i]->has_user = true;
    vm_info->cpu->individual[i]->has_system = true;
    vm_info->cpu->individual[i]->has_idle = true;

    vm_info->cpu->individual[i]->id = i;
    vm_info->cpu->individual[i]->has_id = 1;
    vm_info->cpu->individual[i]->user = rwmain->sys.each_cpu[i]->current.user;
    vm_info->cpu->individual[i]->system = rwmain->sys.each_cpu[i]->current.sys;
    vm_info->cpu->individual[i]->idle = rwmain->sys.each_cpu[i]->current.idle;
  }

  return;
}

/*
 * Parse /proc/meminfo and pull out memory statistics.  A RWPB_T_MSG(RwBase_VcsResource_Vm_Memory)
 * object is then allocated and the relevant information is added.  This will
 * all be added to the vm_info object.
 *
 * @param vm_info - VCS Resource object to add memory information to.
 */
static void fill_vcs_mem(RWPB_T_MSG(RwBase_VcsResource_Vm) * vm_info)
{
  FILE * fp;
  char * line = NULL;
  ssize_t line_len = 0;
  size_t cline_len;

  vm_info->memory = (RWPB_T_MSG(RwBase_VcsResource_Vm_Memory)*)malloc(sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Memory)));
  RW_ASSERT(vm_info->memory);
  RWPB_F_MSG_INIT(RwBase_VcsResource_Vm_Memory,vm_info->memory);

  fp = fopen("/proc/meminfo", "r");
  RW_ASSERT(fp);

  while (true) {
    line_len = getline(&line, &cline_len, fp);
    RW_ASSERT(line_len > 0);

    if (!strncmp(line, "MemTotal:", 9)) {
      char * p = &line[9];
      long int total;

      while (isblank(*++p)) {;}

      total = strtol(p, NULL, 10);
      RW_ASSERT(total != LONG_MIN && total != LONG_MAX);

      vm_info->memory->has_total = true;
      vm_info->memory->total = (uint32_t)total;
    } else if (!strncmp(line, "MemFree:", 8)) {
      char * p = &line[8];
      long int curr_free;

      RW_ASSERT(vm_info->memory->has_total);

      while (isblank(*++p)) {;}

      curr_free = strtol(p, NULL, 10);
      RW_ASSERT(curr_free != LONG_MIN && curr_free != LONG_MAX);

      vm_info->memory->has_curr_usage = true;
      vm_info->memory->curr_usage = vm_info->memory->total - (uint32_t)curr_free;
      break;
    }
    free(line);
    line = NULL;
  }

  if (line)
    free(line);

  fclose(fp);

  return;
}

/*
 * Fill a RWPB_T_MSG(RwBase_VcsResource_Vm_Storage) object with information about the
 * root filesystem.  On completion, it will be added to the vm_info object.
 *
 * @param vm_info - VCS Resource object to add storage information to.
 */
static void fill_vcs_storage(RWPB_T_MSG(RwBase_VcsResource_Vm) * vm_info)
{
  int r;
  struct statvfs vfs;

  vm_info->storage = (RWPB_T_MSG(RwBase_VcsResource_Vm_Storage)*)malloc(sizeof(RWPB_T_MSG(RwBase_VcsResource_Vm_Storage)));
  RW_ASSERT(vm_info->storage);
  RWPB_F_MSG_INIT(RwBase_VcsResource_Vm_Storage,vm_info->storage);

  r = statvfs("/", &vfs);
  RW_ASSERT(r != -1);

  vm_info->storage->has_curr_usage = true;
  vm_info->storage->has_total = true;

  vm_info->storage->total = vfs.f_blocks * (vfs.f_frsize / 1024);
  vm_info->storage->curr_usage = vm_info->storage->total - (vfs.f_bavail * (vfs.f_bsize / 1024));

  return;
}

/*
 * Fill a Process info protobuf with the number of file descriptors the
 * specified process has open.
 *
 * @param process - protobuf to fill
 * @param pid     - pid of the process to inspect
 */
static void fill_process_file_descriptors(RWPB_T_MSG(RwBase_VcsProcess) * process, pid_t pid)
{
  int r;
  char * dirpath;
  DIR * dirp;
  struct dirent * ent;

  r = asprintf(&dirpath, "/proc/%d/fd/", pid);
  RW_ASSERT(r != -1);

  dirp = opendir(dirpath);
  RW_ASSERT(dirp);

  process->has_file_descriptors = true;
  process->file_descriptors = 0;

  for (ent = readdir(dirp); ent; ent = readdir(dirp))
    process->file_descriptors++;

  closedir(dirp);
  free(dirpath);
}

/*
 * Fill a process info protobuf with the amount of memory in kB that
 * a process currently has in resident memory.
 *
 * @param process - protobuf to fill
 * @param pid     - pid of the process to inspect
 */
static void fill_process_memory_nd_threadcount(RWPB_T_MSG(RwBase_VcsProcess) * process, pid_t pid)
{
  int r;
  char * path = NULL;
  char * line = NULL;
  char * start = NULL;
  char * rss_start = NULL;
  char * threadcount_start = NULL;
  FILE * fp = NULL;
  size_t line_len;
  long rss;
  long thread_count;

  r = asprintf(&path, "/proc/%d/stat", pid);
  RW_ASSERT(r != -1);

  fp = fopen(path, "r");
  RW_ASSERT(fp);

  line_len = getline(&line, &line_len, fp);
  if (line_len < 0)
    perror("getline:");
  RW_ASSERT(line_len > 0);

  start = line;
  // RSS is the 24th field, so we need the 23rd space.
  // And the thread-count is the 19th field.
  for (int i = 0; i < 23; ++i) {
    if (i == 19) threadcount_start = start;
    start++;
    while (*start != ' ')
      start++;
  }

  rss_start = start;
  r = sscanf(rss_start, "%ld ", &rss);
  RW_ASSERT(r == 1);

  rss *= sysconf(_SC_PAGESIZE);
  rss /= 1024;

  process->has_memory = true;
  process->memory = (uint32_t)rss;

  r = sscanf(threadcount_start, "%ld ", &thread_count);
  RW_ASSERT(r == 1);

  process->has_thread_count = true;
  process->thread_count = (uint32_t)thread_count;

  free(line);
  fclose(fp);
  free(path);
}

static rwdts_member_rsp_code_t on_vcs_resources(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;
  rw_component_info self;
  char * instance_name = NULL;
  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;
  rwdts_member_rsp_code_t dts_ret;
  RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization) *req = NULL;
  const RWPB_T_PATHSPEC(RwBase_data_Vcs_Resource_Utilization) *key = RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_Resource_Utilization);
  RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization) * resources = NULL;

  if (action != RWDTS_QUERY_READ) {
    return RWDTS_ACTION_OK;
  }

  RW_ASSERT(xact_info);

  if (msg) {
    RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwBase_data_Vcs_Resource_Utilization));
    req = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization) *)msg;
  }

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  rwvcs = rwmain->rwvx->rwvcs;

  instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
  status = rwvcs_rwzk_lookup_component(rwvcs, instance_name, &self);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  if (self.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWVM
      && self.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWPROC
      && self.component_type != RWVCS_TYPES_COMPONENT_TYPE_PROC
      && self.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET) {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  if (req && req->n_vm && req->vm[0]->id) {
    if ((self.vm_info
          && req->vm[0]->id != rwvcs->identity.rwvm_instance_id)
        || (self.proc_info
          && req->vm[0]->id != split_instance_id(self.rwcomponent_parent))) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }
  }

  resources = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization) *)malloc(
      sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization)));
  RW_ASSERT(resources);
  RWPB_F_MSG_INIT(RwBase_data_Vcs_Resource_Utilization,resources);

  resources->n_vm = 1;
  resources->vm = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm) **)malloc(
      sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm) *));
  RW_ASSERT(resources->vm);

  resources->vm[0] = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm) *)malloc(
      sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm)));
  RW_ASSERT(resources->vm[0]);
  RWPB_F_MSG_INIT(RwBase_data_Vcs_Resource_Utilization_Vm,resources->vm[0]);

  resources->vm[0]->id = rwvcs->identity.rwvm_instance_id;
  resources->vm[0]->has_id = 1;


  if (self.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
    fill_vcs_cpu(rwmain, (RWPB_T_MSG(RwBase_VcsResource_Vm) *)resources->vm[0]);
    fill_vcs_mem((RWPB_T_MSG(RwBase_VcsResource_Vm) *)resources->vm[0]);
    fill_vcs_storage((RWPB_T_MSG(RwBase_VcsResource_Vm) *)resources->vm[0]);
  } else if (self.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC
      || self.component_type == RWVCS_TYPES_COMPONENT_TYPE_PROC) {
    RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes) ** processes;

    processes = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes) **)malloc(
        sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes) *));
    RW_ASSERT(processes);
    processes[0] = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes) *)malloc(
        sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes)));
    RW_ASSERT(processes[0]);
    RWPB_F_MSG_INIT(RwBase_data_Vcs_Resource_Utilization_Vm_Processes,processes[0]);

    resources->vm[0]->id = (uint32_t)split_instance_id(self.rwcomponent_parent);
    resources->vm[0]->has_id = 1;
    resources->vm[0]->processes = processes;
    resources->vm[0]->n_processes = 1;

    processes[0]->instance_name = strdup(instance_name);
    RW_ASSERT(processes[0]->instance_name);
    processes[0]->has_cpu = true;
    processes[0]->cpu = rwmain->sys.proc_cpu_usage;
    processes[0]->stats = (RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes_Stats) *)
                       malloc(sizeof(RWPB_T_MSG(RwBase_data_Vcs_Resource_Utilization_Vm_Processes_Stats)));
    RW_ASSERT(processes[0]->stats);
    RWPB_F_MSG_INIT(RwBase_data_Vcs_Resource_Utilization_Vm_Processes_Stats,processes[0]->stats);
    if (processes[0]->stats == NULL) {
      dts_ret = RWDTS_ACTION_NOT_OK;
      goto done;
    }
    processes[0]->stats->has_scheduler_cb = 1;
    processes[0]->stats->scheduler_cb = rwmain->rwvx->rwsched->stats.total_cb_invoked;
    processes[0]->stats->has_scheduler_latency_ts = 1;
    processes[0]->stats->scheduler_latency_ts =rwmain-> rwvx->rwsched->stats.total_latency_ts;

    RW_ASSERT(self.proc_info->has_pid);
    fill_process_file_descriptors((RWPB_T_MSG(RwBase_VcsProcess) *)processes[0], self.proc_info->pid);
    fill_process_memory_nd_threadcount((RWPB_T_MSG(RwBase_VcsProcess) *)processes[0], self.proc_info->pid);
  } else {
    RW_ASSERT(self.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET);
  }

  // Finally, send the result.
  memset (&rsp, 0, sizeof (rsp));
  rsp.msgs = msgs;
  rsp.msgs[0] = &resources->base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)key;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  dts_ret = RWDTS_ACTION_OK;

done:
  if (resources)
    protobuf_c_message_free_unpacked(NULL, &resources->base);

  if (instance_name)
    free(instance_name);

  protobuf_c_message_free_unpacked_usebody(NULL, &self.base);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_vcs_proc_heartbeat_pub(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  rwdts_member_rsp_code_t dts_ret;

  if (action != RWDTS_QUERY_READ) {
    return RWDTS_ACTION_OK;
  }

  RW_ASSERT(xact_info);

  if (msg) {
    RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwBase_data_Vcs_ProcHeartbeat));
  }

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  dts_ret = RWDTS_ACTION_OK;

  RWPB_T_MSG(RwBase_data_Vcs_ProcHeartbeat) resp;
  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;


  RWPB_F_MSG_INIT(RwBase_data_Vcs_ProcHeartbeat, &resp);

  rwproc_heartbeat_settings(
      rwmain,
      (uint16_t *)&resp.frequency,
      &resp.tolerance,
      (bool *)&resp.enabled);

  resp.has_frequency = true;
  resp.has_tolerance = true;
  resp.has_enabled = true;

  status = rwproc_heartbeat_stats(rwmain, &resp.stats, &resp.n_stats);
  if (status != RW_STATUS_SUCCESS) {
    rwmain_trace_crit(rwmain, "Failed to fill heartbeat stats");
  }

  bzero(&rsp, sizeof(rsp));
  msgs[0] = &resp.base;
  rsp.msgs = msgs;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_ProcHeartbeat);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  protobuf_free_stack(resp);
  return dts_ret;
}

static rwdts_member_rsp_code_t on_vcs_proc_heartbeat_sub(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  rwdts_member_rsp_code_t dts_ret;
  RWPB_T_MSG(RwBase_data_Vcs_ProcHeartbeat) * req;
  uint16_t frequency;
  uint32_t tolerance;
  bool enabled;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwBase_data_Vcs_ProcHeartbeat));

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  dts_ret = RWDTS_ACTION_OK;

  if (action != RWDTS_QUERY_UPDATE) {
    return dts_ret;
  }

  rwproc_heartbeat_settings(rwmain, &frequency, &tolerance, &enabled);

  req = (RWPB_T_MSG(RwBase_data_Vcs_ProcHeartbeat) *)msg;

  status = rwproc_heartbeat_reset(
      rwmain,
      req->has_frequency ? req->frequency : frequency,
      req->has_tolerance ? req->tolerance : tolerance,
      req->has_enabled ? req->enabled : enabled);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    dts_ret = RWDTS_ACTION_NOT_OK;
  }

  if (req->has_enabled) {
    RW_ASSERT(rwmain->rwvx->rwvcs);
    rwmain->rwvx->rwvcs->heartbeatmon_enabled = req->enabled;
  }

  return dts_ret;
}

static rwdts_member_rsp_code_t on_version(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  RWPB_T_MSG(RwBase_data_Version) info;

  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_OK;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  RWPB_F_MSG_INIT(RwBase_data_Version, &info);
  const RWPB_T_PATHSPEC(RwBase_data_Version) *key =
      RWPB_G_PATHSPEC_VALUE(RwBase_data_Version);

  info.version = RIFTWARE_VERSION;
  info.build_sha = RIFTWARE_GIT;
  info.build_date = RIFTWARE_DATE;

  memset (&rsp, 0, sizeof (rsp));

  rsp.msgs = msgs;
  rsp.msgs[0] = &info.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)key;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  if (status != RW_STATUS_SUCCESS)
    rwmain_trace_error(rwmain, "Failed to send response to version query");

  return RWDTS_ACTION_OK;
}

static rwdts_member_rsp_code_t on_uptime(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;
  RWPB_T_MSG(RwBase_data_Uptime) info;

  ProtobufCMessage * msgs[1];
  rwdts_member_query_rsp_t rsp;

  RW_ASSERT(xact_info);
  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_NA;

  rwmain = (struct rwmain_gi *)xact_info->ud;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  RWPB_F_MSG_INIT(RwBase_data_Uptime, &info);
  const RWPB_T_PATHSPEC(RwBase_data_Uptime) *key =
      RWPB_G_PATHSPEC_VALUE(RwBase_data_Uptime);

  if (!rwmain->start_sec)
    return RWDTS_ACTION_NA;

  time_t uptime;
  struct timespec tp;
  int r;

  r = clock_gettime(CLOCK_MONOTONIC, &tp);
  RW_ASSERT(r == 0);

  uptime = tp.tv_sec - rwmain->start_sec;
  if (uptime > 60)
    uptime += 30;

  info.uptime = uptime;
  info.has_uptime = true;
  info.has_days = info.days = uptime / 86400;
  uptime %= 86400;
  info.has_hours = info.hours = uptime / 3600;
  uptime %= 3600;
  info.has_minutes = info.minutes = uptime / 60;
  info.has_seconds = true;
  info.seconds = uptime % 60;

  memset (&rsp, 0, sizeof (rsp));

  rsp.msgs = msgs;
  rsp.msgs[0] = &info.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)key;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  if (status != RW_STATUS_SUCCESS)
    rwmain_trace_error(rwmain, "Failed to send response to uptime query");

  return RWDTS_ACTION_OK;
}

char * rwvcs_get_publish_xpath(
    bool internal,
    rwvcs_instance_ptr_t rwvcs,
    char *child_name,
    const char *parent_name,
    RwvcsTypes__YangEnum__ComponentType__E component_type)
{
  rw_status_t rs;
  char *xpath = NULL;
  RW_ASSERT(child_name);

  rw_component_info component;
  switch (component_type) {
    case RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION: {
      break;
      if (!parent_name) {
        return NULL;
      }
      if (internal) {
        rs = rwvcs_rwzk_lookup_component_internal(
            rwvcs,
            parent_name, 
            &component);
      }
      else {
        rs = rwvcs_rwzk_lookup_component(
            rwvcs,
            parent_name, 
            &component);
      }
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      if (component.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION) {
        int r = asprintf(&xpath,
                 VCS_INSTANCE_XPATH_FMT,
                 parent_name);
        RW_ASSERT(r != -1);
      }
    }
    break;
    case RWVCS_TYPES_COMPONENT_TYPE_RWVM: {
      if (strcmp(rwvcs->instance_name, child_name) ||
          !parent_name) {
        return NULL;
      }
      if (internal) {
        rs = rwvcs_rwzk_lookup_component_internal(
            rwvcs,
            parent_name, 
            &component);
      }
      else {
        rs = rwvcs_rwzk_lookup_component(
            rwvcs,
            parent_name, 
            &component);
      }
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      if (component.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
        int r = asprintf(&xpath,
                 VCS_INSTANCE_XPATH_FMT,
                 parent_name);
        RW_ASSERT(r != -1);
      }
    }
    break;
    case RWVCS_TYPES_COMPONENT_TYPE_RWPROC:{
      if (strcmp(rwvcs->instance_name, child_name)) {
        return NULL;
      }
      RW_ASSERT(parent_name);
      int r = asprintf(&xpath,
               VCS_INSTANCE_XPATH_FMT,
               parent_name);
      RW_ASSERT(r != -1);
    }
    break;
    case RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET:
      RW_ASSERT(parent_name);
      if (strcmp(rwvcs->instance_name, parent_name)) {
        return NULL;
      }
    case RWVCS_TYPES_COMPONENT_TYPE_PROC:
      RW_ASSERT(parent_name);
      if (strcmp(rwvcs->instance_name, parent_name)) {
        return NULL;
      }
    default:
    break;
  }
  if (!xpath) {
    xpath = RW_STRDUP(rwvcs->vcs_instance_xpath);
  }
  return xpath;
}

rw_status_t rwvcs_instance_update_child_state(
    struct rwmain_gi *rwmain,
    char *child_name,
    const char *parent_name,
    RwvcsTypes__YangEnum__ComponentType__E component_type,
    RwBase__YangEnum__AdminCommand__E admin_command)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  RW_ASSERT(child_name);
  if (ck_pr_load_int(&rwmain->rwvx->rwvcs->restart_inprogress)) {
    return rs;
  }

  char *xpath = rwvcs_get_publish_xpath(
      false,
      VCS_GET(rwmain),
      child_name,
      parent_name,
      component_type);
  if (!xpath) {
    goto skip_to_done;
  }
  if (VCS_GET(rwmain)->vcs_instance_regh) {
    RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN) *child_n;
    child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)*)RW_MALLOC0(sizeof(*child_n));
    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN, child_n);
    child_n->instance_name = strdup(child_name);
    if (component_type < RWVCS_TYPES_COMPONENT_TYPE_MAX_VALUE) {
      child_n->has_component_type = TRUE;
      child_n->component_type = component_type;
    }
    if (admin_command < RW_BASE_ADMIN_COMMAND_MAX_VALUE) {
      child_n->has_admin_command = TRUE;
      child_n->admin_command = admin_command;
    }
    RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst;
    inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)RW_MALLOC0(sizeof(*inst));
    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance, inst);
    inst->instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
    RW_ASSERT(inst->instance_name);
    inst->child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)**)RW_MALLOC0(sizeof(*inst->child_n));
    inst->child_n[0] = child_n;
    inst->n_child_n = 1;

    rwmain_trace_info(rwmain, "UPDATING instance_name=%s child=%s to xpath=%s\n",
                      inst->instance_name,
                      child_name,
                      xpath);
    rs = rwdts_member_reg_handle_update_element_xpath(
        VCS_GET(rwmain)->vcs_instance_regh,
        xpath,
        &inst->base,
        0, //RWDTS_FLAG_TRACE
        NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);

    // Free the protobuf
    if (inst) {
      protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inst);
    }
  }
skip_to_done:
  if (xpath) {
    RW_FREE(xpath);
  }
  return rs;
}

rw_status_t rwvcs_instance_delete_child(
    struct rwmain_gi *rwmain,
    char *child_name,
    const char *parent_name,
    RwvcsTypes__YangEnum__ComponentType__E component_type)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  RW_ASSERT(child_name);

  if (VCS_GET(rwmain)->apih) {
    rwvcs_config_ready_entry_t *config_ready_entry = NULL;
    RW_SKLIST_LOOKUP_BY_KEY(
        &VCS_GET(rwmain)->config_ready_entries,
        &child_name,
        &config_ready_entry);
    if (config_ready_entry) {
      rs = rwdts_member_reg_handle_delete_element_xpath(
          config_ready_entry->regh,
          config_ready_entry->xpath,
          NULL,
          NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
      rs = rwdts_member_reg_handle_delete_element_xpath(
          VCS_GET(rwmain)->vcs_instance_regh,
          config_ready_entry->childn_xpath,
          NULL,
          NULL);
      RW_ASSERT(rs == RW_STATUS_SUCCESS);
    }
  }
  return rs;
}

rw_status_t rwvcs_instance_delete_instance(struct rwmain_gi *rwmain)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  if (VCS_GET(rwmain)->vcs_instance_regh) {
    rs = rwdts_member_reg_handle_delete_element_xpath(
        VCS_GET(rwmain)->vcs_instance_regh,
        VCS_GET(rwmain)->vcs_instance_xpath,
        NULL,
        NULL);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  return rs;
}

static void send2dts_start_req_done(
    rwdts_xact_t * xact,
    rwdts_xact_status_t* xact_status,
    void * ud)
{
  rwvcs_instance_ptr_t rwvcs;
  struct dts_start_stop_closure * cls;


  if (!xact_status->xact_done) {
    return;
  }

  cls = (struct dts_start_stop_closure *)ud;
  rwvcs = cls->rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst = NULL;

  rwdts_query_result_t* qres = rwdts_xact_query_result(xact,0);
  if (qres) {
    inst = ( RWPB_T_MSG(RwBase_VcsInstance_Instance) *)qres->message;
    send_start_request_response(
      cls->xact,
      cls->query,
      RW_STATUS_SUCCESS,
      inst->child_n[0]->instance_name);
  }
  else {
    send_start_request_response(
      cls->xact,
      cls->query,
      RW_STATUS_FAILURE,
      NULL);
  }

  free(cls->instance_name);
  free(cls->rpc_query);
  if (cls->xact) {
    rwdts_xact_unref(cls->xact, __PRETTY_FUNCTION__, __LINE__);
    cls->xact = NULL;
  }
  free(cls);
}

static void send2dts_stop_req_done(
    rwdts_xact_t * xact,
    rwdts_xact_status_t* xact_status,
    void * ud)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwvcs_instance_ptr_t rwvcs;
  struct dts_start_stop_closure * cls;

  if (xact_status->status == RWDTS_XACT_ABORTED ||
       xact_status->status == RWDTS_XACT_FAILURE) {
      rwdts_xact_unref(xact, __PRETTY_FUNCTION__, __LINE__);
    return;
  }

  if (!xact_status->xact_done) {
    return;
  }

  cls = (struct dts_start_stop_closure *)ud;
  rwvcs = cls->rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  /*
   * No children left?  We can directly stop now then.
   */
  {
    char * instance_name;
    rw_component_info self;
    instance_name = to_instance_name(cls->rwmain->component_name, cls->rwmain->instance_id);
    status = rwvcs_rwzk_lookup_component(
        cls->rwmain->rwvx->rwvcs,
        instance_name,
        &self);
    if (status == RW_STATUS_SUCCESS) {

      status = rwmain_stop_instance(cls->rwmain, &self);
      RW_ASSERT(status == RW_STATUS_SUCCESS);

      RWMAIN_DEREG_PATH_DTS(cls->rwmain->dts, &self,
                            self.has_recovery_action? self.recovery_action: RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL);


      status = rwvcs_instance_delete_instance(cls->rwmain);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
  }

  free(cls->instance_name);
  free(cls->rpc_query);
  if (cls->xact) {
    rwdts_xact_unref(cls->xact, __PRETTY_FUNCTION__, __LINE__);
    cls->xact = NULL;
  }
  free(cls);
}

rw_status_t send2dts_start_req(
  struct rwmain_gi * rwmain,
  const rwdts_xact_info_t * xact_info,
  vcs_rpc_start_input *start_req)
{
  RW_ASSERT(start_req->component_name);
  rwvcs_instance_ptr_t rwvcs;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN) *child_n;
  child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)*)RW_MALLOC0(sizeof(*child_n));
  RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN, child_n);
  child_n->instance_name = strdup("START-REQ");
  child_n->component_name = strdup(start_req->component_name);
  if (start_req->has_recover && start_req->recover) {
    child_n->has_admin_command = true;
    child_n->admin_command = RW_BASE_ADMIN_COMMAND_RECOVER;
  }

  RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst;
  inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)RW_MALLOC0(sizeof(*inst));
  RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance, inst);
  inst->instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
  RW_ASSERT(inst->instance_name);
  inst->child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)**)RW_MALLOC0(sizeof(*inst->child_n));
  inst->child_n[0] = child_n;
  inst->n_child_n = 1;

  struct dts_start_stop_closure * cls = NULL;
  if (xact_info) {
    cls = (struct dts_start_stop_closure *)RW_MALLOC0(sizeof(struct dts_start_stop_closure));
    RW_ASSERT(cls);

    cls->rwmain = rwmain;
    cls->xact = xact_info->xact;
    rwdts_xact_ref(cls->xact, __PRETTY_FUNCTION__, __LINE__);
    cls->query = xact_info->queryh;
    cls->vstart_corr_id = rwmain->vstart_corr_id;

    cls->instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
    RW_ASSERT(cls->instance_name);
    cls->rpc_query = strdup(start_req->component_name);
  }

  rwmain_trace_info(rwmain, "SENDING START-REQ to xpath: %s", VCS_GET(rwmain)->vcs_instance_xpath);

  rwmain->vstart_corr_id++;
  rwdts_xact_t* xact = rwdts_api_query(
      rwmain->dts,
      VCS_GET(rwmain)->vcs_instance_xpath,
      RWDTS_QUERY_CREATE,
      0, //RWDTS_XACT_FLAG_TRACE,
      (cls ? send2dts_start_req_done : NULL),
      cls,
      &(inst->base));
  RW_ASSERT(xact);

  // Free the protobuf
  if (inst) {
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inst);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t send2dts_stop_req(
  struct rwmain_gi * rwmain,
  const rwdts_xact_info_t * xact_info,
  char *instance_name,
  char *child_name)
{
  RW_ASSERT(instance_name || child_name);
  rwvcs_instance_ptr_t rwvcs;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);
  char xpath[999];
  RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst = NULL;
  RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN) *child_n = NULL;
  const ProtobufCMessage* msg = NULL;

  rwmain_trace_info(rwmain, "send2dts_stop_req instance_name=%s child_name=%s\n",
          instance_name, child_name);

  if (instance_name) {
    inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)RW_MALLOC0(sizeof(*inst));
    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance, inst);
    inst->instance_name = strdup(instance_name);
    inst->has_admin_stop = TRUE;
    inst->admin_stop = TRUE;
    RW_ASSERT(inst->instance_name);
    sprintf(xpath, VCS_INSTANCE_XPATH_FMT,
            instance_name);
    msg = &(inst->base);
  }

  if (child_name) {
    child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)*)RW_MALLOC0(sizeof(*child_n));
    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN, child_n);
    child_n->instance_name = strdup(child_name);
    child_n->has_admin_command = TRUE;
    child_n->admin_command = RW_BASE_ADMIN_COMMAND_STOP;
    sprintf(xpath, "%s/child-n[instance-name='%s']",
          VCS_GET(rwmain)->vcs_instance_xpath,
          child_name);
    msg = &(child_n->base);
  }

  if (inst && child_n) {
    inst->child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)**)RW_MALLOC0(sizeof(*inst->child_n));
    inst->child_n[0] = child_n;
    inst->n_child_n = 1;
    sprintf(xpath, VCS_INSTANCE_XPATH_FMT,
            instance_name);
    msg = &(inst->base);
    child_n = NULL;
  }

  struct dts_start_stop_closure * cls = NULL;
  if (xact_info) {
    cls = (struct dts_start_stop_closure *)RW_MALLOC0(sizeof(struct dts_start_stop_closure));
    RW_ASSERT(cls);

    cls->rwmain = rwmain;
    cls->xact = xact_info->xact;
    rwdts_xact_ref(cls->xact, __PRETTY_FUNCTION__, __LINE__);
    cls->query = xact_info->queryh;
    cls->vstart_corr_id = rwmain->vstart_corr_id;

    cls->instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
    RW_ASSERT(cls->instance_name);
    //cls->rpc_query = strdup(component_name);
    cls->rpc_query = NULL;
  }

  rwdts_xact_t* xact = rwdts_api_query(
      rwmain->dts,
      xpath,
      RWDTS_QUERY_UPDATE,
      0, //RWDTS_FLAG_TRACE,
      (cls ? send2dts_stop_req_done : NULL),
      cls,
      msg);
  RW_ASSERT(xact);

  // Free the protobuf
  if (inst) {
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inst);
  }
  if (child_n) {
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)child_n);
  }
  
  return RW_STATUS_SUCCESS;
}

static void rwmain_dts_vcs_instance_reg_ready(
    rwdts_member_reg_handle_t  regh,
    rw_status_t                rs,
    void*                      user_data)
{
  struct rwmain_gi *rwmain = (struct rwmain_gi *)user_data;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);

  RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst;
  inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)RW_MALLOC0(sizeof(*inst));
  RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance, inst);

  char * self_instance;
  rwvcs_instance_ptr_t rwvcs;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);
  rw_status_t status;
  self_instance = to_instance_name(rwmain->component_name, rwmain->instance_id);
  inst->instance_name = self_instance;
  rw_component_info self;
  rw_component_info component;
  status = rwvcs_rwzk_lookup_component(rwvcs, self_instance, &self);
  if (status != RW_STATUS_SUCCESS) {
    return;
  }
  for (size_t num_components=0; num_components<self.n_rwcomponent_children; ++num_components) {
    status = rwvcs_rwzk_lookup_component(rwvcs, self.rwcomponent_children[num_components], &component);
    RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN) *child_n;
    child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)*)RW_MALLOC0(sizeof(*child_n));
    RWPB_F_MSG_INIT(RwBase_VcsInstance_Instance_ChildN, child_n);
    child_n->has_component_type = TRUE;
    child_n->component_type = component.component_type;
    protobuf_c_message_free_unpacked_usebody(NULL, &component.base);
    child_n->instance_name = strdup(self.rwcomponent_children[num_components]);
    inst->child_n = (RWPB_T_MSG(RwBase_VcsInstance_Instance_ChildN)**)realloc(inst->child_n, sizeof(*inst->child_n)*(inst->n_child_n+1));
    inst->child_n[inst->n_child_n] = child_n;
    inst->n_child_n++;
    rwmain_trace_info(rwmain, "Instance %s number of children %d Name %s\n",
            self_instance,
            (int)inst->n_child_n,
            self.rwcomponent_children[num_components]);
  }
  rs = rwdts_member_reg_handle_update_element_xpath(
      VCS_GET(rwmain)->vcs_instance_regh,
      VCS_GET(rwmain)->vcs_instance_xpath,
      &inst->base,
      0,
      NULL);
  RW_ASSERT(rs==RW_STATUS_SUCCESS);

  // Free the protobuf
  if (inst) {
    protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inst);
  }
  
  return;
}

static rwdts_member_rsp_code_t rwmain_dts_vcs_instance_prepare(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction         action,
    const rw_keyspec_path_t* keyspec,
    const ProtobufCMessage*  msg,
    void*                    user_data)
{
  struct rwmain_gi *rwmain = (struct rwmain_gi *)user_data;
  RW_CF_TYPE_VALIDATE(rwmain->rwvx, rwvx_instance_ptr_t);
  rwdts_member_rsp_code_t dts_ret = RWDTS_ACTION_OK;

  rwdts_api_t *api = xact_info->apih; 
  rwdts_xact_t *xact = xact_info->xact; 
  rwdts_member_reg_handle_t regh = xact_info->regh;
  rw_status_t rs;

  RW_ASSERT(api != NULL);
  RW_ASSERT(xact != NULL);

  if (action == RWDTS_QUERY_CREATE) {  /* VSTART come as CREATE */
    RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)msg;;
    if (strcmp(inst->child_n[0]->instance_name, "START-REQ")) {
      // Don't update for "START-REQ"
      rs = rwdts_member_reg_handle_update_element_xpath(
          regh,
          VCS_GET(rwmain)->vcs_instance_xpath,
          &inst->base,
          0,
          NULL);
      RW_ASSERT(rs==RW_STATUS_SUCCESS);
      RW_ASSERT(inst->n_child_n==1); // Now assume only one create at a times; event thou we have forloop below.
    }
    if (!strcmp(inst->child_n[0]->instance_name, "START-REQ")) {
      char * instance_name = NULL;
      char * child_instance = NULL;
      instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);

      RW_ASSERT(inst->child_n[0]->component_name);

      dts_ret = do_vstart(
          xact_info,
          rwmain,
          inst->child_n[0],
          instance_name,
          &child_instance);
      if (dts_ret == RWDTS_ACTION_OK) {
        inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)protobuf_c_message_duplicate(NULL, msg, msg->descriptor);
        RW_ASSERT(child_instance);
        inst->child_n[0]->instance_name = child_instance;
        child_instance = NULL;

        rwdts_member_query_rsp_t rsp;
        rw_status_t status;
        ProtobufCMessage * msgs[1];
        memset (&rsp, 0, sizeof (rsp));
        rsp.msgs = msgs;
        rsp.msgs[0] = &inst->base;
        rsp.n_msgs = 1;
        rsp.ks = (rw_keyspec_path_t*)keyspec;
        rsp.evtrsp = RWDTS_EVTRSP_ACK;
        status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
        RW_ASSERT(status == RW_STATUS_SUCCESS);

        // Free the protobuf
        if (inst) {
          protobuf_c_message_free_unpacked_usebody(NULL, (ProtobufCMessage*)inst);
        }
      }
    }
  } else if (action == RWDTS_QUERY_UPDATE) {  /* VSTOP come as UPDATE */
    RWPB_T_MSG(RwBase_VcsInstance_Instance) *inst = (RWPB_T_MSG(RwBase_VcsInstance_Instance)*)msg;;
    rs = rwdts_member_reg_handle_update_element_xpath(
        regh,
        VCS_GET(rwmain)->vcs_instance_xpath,
        &inst->base,
        0,
        NULL);
    RW_ASSERT(rs==RW_STATUS_SUCCESS);
    for (int i=0; i<inst->n_child_n; i++) {
      if (inst->child_n[i]->admin_command == RW_BASE_ADMIN_COMMAND_STOP) {
        do_vstop(
            xact_info, //NULL,
            rwmain,
            inst->child_n[i]->instance_name);
      }
    }
    if (inst->has_admin_stop && inst->admin_stop) {
      do_vstop(
          xact_info, //NULL,
          rwmain,
          inst->instance_name);
    }
  } else {
  }

  return dts_ret;
}

rw_status_t rwmain_setup_dts_registrations(struct rwmain_gi * rwmain)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  rwdts_registration_t dts_registrations[] = {
    // Show tasklet info
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_Info),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Vcs_Info),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_tasklet_info,
      }
    },

    // Show tasklet resource
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDebug_data_RwDebug_Resource),
      .desc = RWPB_G_MSG_PBCMD(RwDebug_data_RwDebug_Resource),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_tasklet_resource,
      }
    },

    // Show tasklet heap
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDebug_data_RwDebug_Heap),
      .desc = RWPB_G_MSG_PBCMD(RwDebug_data_RwDebug_Heap),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_tasklet_heap,
      }
    },

    // Show vcs resources
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_Resource_Utilization),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Vcs_Resource_Utilization),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_vcs_resources,
      }
    },

    // Show vcs proc_heartbeat publisher
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_ProcHeartbeat),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Vcs_ProcHeartbeat),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_vcs_proc_heartbeat_pub,
      }
    },

    // Show version info
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Version),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Version),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_version,
      }
    },

    // Show uptime info
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Uptime),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Uptime),
      .flags = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED,
      .callback = {
        .cb.prepare = &on_uptime,
      }
    },

    {
      .keyspec = NULL,
    },
  };

  for (int i = 0; i< sizeof(dts_registrations)/sizeof(dts_registrations[0]); i++) {
    if (dts_registrations[i].keyspec) {
      dts_registrations[i].callback.ud = (void *)rwmain;
      rw_keyspec_path_t* lks = NULL;
      rw_keyspec_path_create_dup(dts_registrations[i].keyspec, NULL, &lks);
      rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);
      rwdts_member_reg_handle_t reg_handle = rwdts_member_register(
          NULL,
          rwmain->dts,
          lks,
          &dts_registrations[i].callback,
          dts_registrations[i].desc,
          dts_registrations[i].flags,
          NULL);
      RW_ASSERT(reg_handle);
      rw_keyspec_path_free(lks, NULL);
    }
  }

  rwdts_registration_t config_dts_registrations[] = {
    // config tasklet-heap depth
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDebug_data_RwDebug_Settings_Heap),
      .desc = RWPB_G_MSG_PBCMD(RwDebug_data_RwDebug_Settings_Heap),
      .flags = RWDTS_FLAG_SUBSCRIBER,
      .callback = {
        .cb.prepare = &on_config_tasklet_heap,
      }
    },

    // config vcs proc_heartbeat
    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwBase_data_Vcs_ProcHeartbeat),
      .desc = RWPB_G_MSG_PBCMD(RwBase_data_Vcs_ProcHeartbeat),
      .flags = RWDTS_FLAG_SUBSCRIBER,
      .callback = {
        .cb.prepare = &on_vcs_proc_heartbeat_sub,
      }
    },

    {
      .keyspec = (rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwDebug_data_RwDebug_Settings_Scheduler),
      .desc = RWPB_G_MSG_PBCMD(RwDebug_data_RwDebug_Settings_Scheduler),
      .flags = RWDTS_FLAG_SUBSCRIBER,
      .callback = {
        .cb.prepare = &on_config_tasklet_scheduler,
      }
    },

    {
      .keyspec = NULL,
    },
  };

  for (int i = 0; i< sizeof(config_dts_registrations)/sizeof(config_dts_registrations[0]); i++) {
    if (config_dts_registrations[i].keyspec) {
      config_dts_registrations[i].callback.ud = (void *)rwmain;
      rw_keyspec_path_t* lks = NULL;
      rw_keyspec_path_create_dup(config_dts_registrations[i].keyspec, NULL, &lks);
      rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
      rwdts_member_reg_handle_t reg_handle = rwdts_member_register(
          NULL,
          rwmain->dts,
          lks,
          &config_dts_registrations[i].callback,
          config_dts_registrations[i].desc,
          config_dts_registrations[i].flags,
          NULL);
      RW_ASSERT(reg_handle);
      rw_keyspec_path_free(lks, NULL);
    }
  }

  status = rwmain_dts_register_vcs_instance(rwmain, NULL);

  return status;
}


rw_status_t rwmain_dts_register_vcs_instance(struct rwmain_gi * rwmain, const char* instance_name)
{
  char xpath[999];
  rwdts_member_reg_handle_t regh;
  rw_status_t rs;
  if (instance_name) {
    sprintf(xpath, VCS_INSTANCE_XPATH_FMT, instance_name);
  } else {
    sprintf(xpath, "%s", VCS_GET(rwmain)->vcs_instance_xpath);
  }
  rwmain_trace_info(rwmain, "REGISTERING xpath: %s", xpath);
  rs  = rwdts_api_member_register_xpath(
      rwmain->dts,
      xpath,
      NULL,
      RWDTS_FLAG_PUBLISHER | RWDTS_FLAG_NO_PREP_READ | /*RWDTS_FLAG_TRACE |*/ RWDTS_FLAG_CACHE,
      RW_DTS_SHARD_FLAVOR_NULL, NULL, 0,-1, 0, 0,
      rwmain_dts_vcs_instance_reg_ready, //reg_ready
      rwmain_dts_vcs_instance_prepare, //prepare
      NULL, //precommit
      NULL, //commit
      NULL, //abort
      rwmain, //ctx
      &regh);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  RW_ASSERT(regh);
  if (!instance_name) VCS_GET(rwmain)->vcs_instance_regh = regh;
  return rs;
}

rw_status_t rwmain_dts_config_ready_process(rwvcs_instance_ptr_t rwvcs,
                                            rw_component_info * id)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  if (ck_pr_load_int(&rwvcs->restart_inprogress)) {
    return status;
  }
  char *instance_xpath = rwvcs_get_publish_xpath(
      true, /* called from within zk_rwq serial thread*/
      rwvcs,
      id->instance_name,
      id->rwcomponent_parent,
      id->component_type);
  if (!instance_xpath) {
    return status;
  }
  char *childn_xpath = NULL;
  asprintf (&childn_xpath, "%s/child-n[instance-name='%s']", instance_xpath, id->instance_name);
  RW_ASSERT(childn_xpath);
  RW_FREE(instance_xpath);

  char *xpath = NULL;
  asprintf (&xpath, "%s/publish-state", childn_xpath);
  RW_ASSERT(xpath);

  if (rwvcs->apih) {
    rwvcs_config_ready_entry_t *config_ready_entry;
    RW_SKLIST_LOOKUP_BY_KEY(
        &rwvcs->config_ready_entries,
        &id->instance_name,
        &config_ready_entry);
    if (!config_ready_entry) {
      config_ready_entry = RW_MALLOC0_TYPE (sizeof(*config_ready_entry),
                                            typeof(*config_ready_entry));
      RW_ASSERT_TYPE(config_ready_entry, typeof(*config_ready_entry));
      config_ready_entry->instance_name = RW_STRDUP(id->instance_name);
      config_ready_entry->childn_xpath = childn_xpath;
      config_ready_entry->xpath = xpath;
      childn_xpath = xpath = NULL;
      status = RW_SKLIST_INSERT(
          &(rwvcs->config_ready_entries), 
          config_ready_entry);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }

    rwdts_api_config_ready_data_t config_ready_data = {
      .apih = rwvcs->apih,
      .regh_p = &config_ready_entry->regh,
      .xpath = config_ready_entry->xpath,
      .config_ready = id->config_ready,
      .state = id->state};

    rwdts_config_ready_publish(&config_ready_data);
  }
  if (childn_xpath) {
    RW_FREE(childn_xpath);
  }
  if (xpath) {
    RW_FREE(xpath);
  }
  return status;
}

static void
rwvcs_publish_active_mgmt_info_data(rwvcs_instance_ptr_t      rwvcs,
                                    rwdts_member_reg_handle_t regh,
                                    rw_keyspec_path_t         *keyspec)
{
  RWPB_T_MSG(RwHa_data_ActiveMgmtInfo) active_mgmt_info_msg, *active_mgmt_info_msgp;
  active_mgmt_info_msgp = &active_mgmt_info_msg;
  RWPB_F_MSG_INIT(RwHa_data_ActiveMgmtInfo, active_mgmt_info_msgp);
  if (active_mgmt_info_msgp->mgmt_ip_address) {
    free (active_mgmt_info_msgp->mgmt_ip_address);
  }
  active_mgmt_info_msgp->mgmt_ip_address = strdup(inet_ntoa(rwvcs->mgmt_info.master_vm_info.vm_addr));
  active_mgmt_info_msgp->mgmt_vm_instance_name = rwvcs->mgmt_info.master_vm_info.vm_name;

  rw_status_t create_rs = rwdts_member_reg_handle_create_element_keyspec(regh, 
                                                                         keyspec,
                                                                         &active_mgmt_info_msgp->base,
                                                                         NULL);
  RW_ASSERT(create_rs == RW_STATUS_SUCCESS);
}

static void
rwvcs_publish_active_mgmt_info_ready(rwdts_member_reg_handle_t regh,
                                     rw_status_t               rs,
                                     void*                     ctx)
{
  rwvcs_instance_ptr_t rwvcs = (rwvcs_instance_ptr_t) ctx;
    
  rw_keyspec_path_t *active_mgmt_info_ks = NULL;
  rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_ActiveMgmtInfo)->rw_keyspec_path_t,
                             NULL,
                             &active_mgmt_info_ks);

  RW_ASSERT_MESSAGE(active_mgmt_info_ks, "keyspec duplication failed");
  rw_keyspec_path_set_category (active_mgmt_info_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

  rwvcs_publish_active_mgmt_info_data(rwvcs, regh, (rw_keyspec_path_t *)active_mgmt_info_ks);
  rw_keyspec_path_free(active_mgmt_info_ks, NULL);

  return;
}

rw_status_t
rwvcs_publish_active_mgmt_info(rwvcs_instance_ptr_t rwvcs)
{
  if (rwvcs->mgmt_info.mgmt_vm) {
    rw_keyspec_path_t *active_mgmt_info_ks = NULL;
    rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_ActiveMgmtInfo)->rw_keyspec_path_t,
                               NULL,
                               &active_mgmt_info_ks);
    RW_ASSERT_MESSAGE(active_mgmt_info_ks, "keyspec duplication failed");
    rw_keyspec_path_set_category (active_mgmt_info_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

    if (!rwvcs->mgmt_info.regh) {
      rwdts_registration_t regn = {
        .keyspec = (rw_keyspec_path_t*)active_mgmt_info_ks,
        .desc    = RWPB_G_MSG_PBCMD(RwHa_data_ActiveMgmtInfo),
        .flags   = RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_CACHE|RWDTS_FLAG_NO_PREP_READ,
        .callback = {
          .cb.reg_ready = rwvcs_publish_active_mgmt_info_ready,
          .ud = rwvcs
        }
      };
      rwvcs->mgmt_info.regh = rwdts_member_register(NULL, rwvcs->apih,
                                                    regn.keyspec,
                                                    &regn.callback,
                                                    regn.desc,
                                                    regn.flags,
                                                    NULL);

    }
    else {
      rwvcs_publish_active_mgmt_info_data(rwvcs, rwvcs->mgmt_info.regh, (rw_keyspec_path_t *)active_mgmt_info_ks);
    }
    rw_keyspec_path_free(active_mgmt_info_ks, NULL);
  }
  return RW_STATUS_SUCCESS;
}


static rwdts_member_rsp_code_t rwvcs_ha_uagent_state_prepare (
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{

  RW_ASSERT(xact_info);

  rwvcs_instance_ptr_t rwvcs = (rwvcs_instance_ptr_t)xact_info->ud;
  RWPB_T_MSG(RwHa_data_HaUagentState) *ha_uagent_state_msg =
      (RWPB_T_MSG(RwHa_data_HaUagentState) *)msg;

  if (ha_uagent_state_msg
      && ha_uagent_state_msg->ha_config_state
      && ha_uagent_state_msg->ha_config_state->has_ready 
      && ha_uagent_state_msg->ha_config_state->ready) {
    if (ck_pr_load_int(&rwvcs->restart_inprogress)) {
        rwmain_restart_deferred (rwvcs->mgmt_info.restart_list);
    }
  }

  return RWDTS_ACTION_OK;
}

rw_status_t
rwvcs_register_ha_uagent_state(rwvcs_instance_ptr_t rwvcs)
{
  if (rwvcs->mgmt_info.mgmt_vm) {
    rw_keyspec_path_t *ha_uagent_state_ks = NULL;
    rw_keyspec_path_create_dup(&RWPB_G_PATHSPEC_VALUE(RwHa_data_HaUagentState)->rw_keyspec_path_t,
                               NULL,
                               &ha_uagent_state_ks);
    RW_ASSERT_MESSAGE(ha_uagent_state_ks, "keyspec duplication failed");
    rw_keyspec_path_set_category (ha_uagent_state_ks, NULL, RW_SCHEMA_CATEGORY_DATA);

    rwdts_registration_t regn = {
      .keyspec = (rw_keyspec_path_t*)ha_uagent_state_ks,
      .desc    = RWPB_G_MSG_PBCMD(RwHa_data_HaUagentState),
      .flags   = RWDTS_FLAG_SUBSCRIBER,
      .callback = {
        .cb.prepare = rwvcs_ha_uagent_state_prepare,
        .ud = rwvcs
      }
    };
    rwdts_member_register(NULL, rwvcs->apih,
                          regn.keyspec,
                          &regn.callback,
                          regn.desc,
                          regn.flags,
                          NULL);

    rw_keyspec_path_free(ha_uagent_state_ks, NULL);
  }
  return RW_STATUS_SUCCESS;
}

void raise_heartbeat_failure_notification(
      rwdts_api_t* apih,
      const char* taskletname)
{
  RWPB_M_MSG_DECL_PTR_ALLOC(RwVcs_notif_HeartbeatAlarm, alarm);
  alarm->name = strdup(taskletname);
  rwdts_api_send_notification(apih, &(alarm->base));
  protobuf_c_message_free_unpacked(NULL, &(alarm->base));
}
