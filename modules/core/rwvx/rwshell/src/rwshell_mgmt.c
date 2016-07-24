
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwlog_mgmt.c
 * @date 11/09/2014
 * @brief RW.Log Tasklet Management Implementation
 *
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <stdarg.h>
#include <pcap/pcap.h>

#include "rwvcs_rwzk.h"

#include "rwshell-api.h"
#include "rwshell-mgmt.pb-c.h"                                                                                                                                                                                                                                                
#include "rwvx.h"
#include "rwvcs.h"
#include "rwvcs_rwzk.h"
#include "rwshell_mgmt.h"

rwshell_module_ptr_t m_mod = NULL;

rw_status_t get_vmname_and_procids(
    rwtasklet_info_ptr_t tasklet,
    char **vm_name_p,
    char ***pids_p)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwvcs_instance_ptr_t rwvcs = tasklet->rwvcs;
  char *instance_name = NULL;
  RWPB_T_MSG(RwBase_ComponentInfo) self;

  RW_ASSERT(tasklet);
  RW_ASSERT(vm_name_p);

  *vm_name_p = NULL;

  instance_name = to_instance_name(tasklet->identity.rwtasklet_name, tasklet->identity.rwtasklet_instance_id);
  status = rwvcs_rwzk_lookup_component(rwvcs, instance_name, &self);
  RW_ASSERT(RW_STATUS_SUCCESS == status);
  RW_ASSERT(RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET == self.component_type);

  char *vm = self.rwcomponent_parent;
  rw_component_info vm_info;
  while (RW_STATUS_SUCCESS == rwvcs_rwzk_lookup_component(rwvcs, vm, &vm_info) &&
         RWVCS_TYPES_COMPONENT_TYPE_RWVM != vm_info.component_type &&
         ((vm = vm_info.rwcomponent_parent) != NULL))
    protobuf_free_stack(vm_info);
  RW_ASSERT(RWVCS_TYPES_COMPONENT_TYPE_RWVM == vm_info.component_type);

  *vm_name_p = strdup(vm_info.instance_name);

  if (!pids_p)
    goto done;

  *pids_p = NULL;
  int i, j, r, n_pids = 0;
  if (vm_info.vm_info) {
    (*pids_p) = realloc((*pids_p), (n_pids+1)*sizeof(char**));
    n_pids++;
    r = asprintf(&(*pids_p)[0], "%u", vm_info.vm_info->pid);
    RW_ASSERT(r != -1);
  }
  for (i = 0; i < vm_info.n_rwcomponent_children; ++i) {
    rw_component_info child_info;
    char * child = vm_info.rwcomponent_children[i];

    status = rwvcs_rwzk_lookup_component(rwvcs, child, &child_info);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    if (//child_info.component_type != RWVCS_TYPES_COMPONENT_TYPE_PROC &&
        child_info.component_type != RWVCS_TYPES_COMPONENT_TYPE_RWPROC) {
      goto cont;
    }

    RW_ASSERT(child_info.proc_info);

    for (j=0; j<n_pids; j++)
      if (atoi((*pids_p)[j]) == child_info.proc_info->pid)
        break;
    if (j>=n_pids) {
      (*pids_p) = realloc((*pids_p), (n_pids+1)*sizeof(char**));
      n_pids++;
      r = asprintf(&(*pids_p)[j], "%u", child_info.proc_info->pid);
      RW_ASSERT(r != -1);
    }
cont:
    protobuf_free_stack(child_info);
  }

  if (n_pids > 0) {
    (*pids_p) = realloc((*pids_p), (n_pids+1)*sizeof(char**));
    (*pids_p)[n_pids] = NULL;
  }

done:
  protobuf_free_stack(self);
  protobuf_free_stack(vm_info);
  if (instance_name)
    free(instance_name);

  return status;
}

/* Related to background profiler */

void *g_rw_profiler_handle = NULL;
bool g_background_profiler_enabled = 0;

static int newpid=0;
static int *newpids;
static int pids_count;
static char **pids = NULL;
static bool g_perf_started = false;
static bool g_perf_stopped = false;
static bool g_perf_reported = false;

#ifdef RW_BACKGROUND_PROFILING
static void rw_profiler_tick(void *info)
{
  rw_profiler_t *ph = (rw_profiler_t *) info;
  rw_status_t status;
  char *command = NULL;

  RW_ASSERT_TYPE(ph, rw_profiler_t);


  if (!m_mod)
    m_mod = rwshell_module_alloc();
  RW_ASSERT(m_mod);

  if (newpid) {
    status = rwshell_perf_stop(
        m_mod,
        newpid,
        &command);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    free(command);
  }

  char *instance_name= NULL;
  char **pids = NULL;
  RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(
          ph->tasklet_info,
          &instance_name, &pids));

  char perf_data_fname[999];
  sprintf(perf_data_fname, "/tmp/rwprofiler.%s.data", instance_name);

#if 0
  fprintf(stderr, "rw_profiler_tick(%s) %p\n", instance_name, pids);
  char *command;
  char *report;
  status = rwshell_perf_report(m_mod, 1.0, perf_data_fname, &command, &report);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  fprintf(stderr, "command\n%s", command);
  fprintf(stderr, "profiler-output\n%s", report);
#endif

  if (pids) {
    status = rwshell_perf_start(
        m_mod,
        (const char**)pids,
        100,
        perf_data_fname,
        &command,
        &newpid);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    free(command);

    //free pids
    int i;
    for (i=0; pids[i]; i++)
      free(pids[i]);
    free(pids);
  }
  if (instance_name)
    free(instance_name);
}

void* start_background_profiler(
    rwtasklet_info_ptr_t rwtasklet_info,
    uint32_t duration_secs,
    bool use_mainq)
{
  RW_ASSERT(rwtasklet_info!=NULL);

  rw_profiler_t *ph;
  ph = (rw_profiler_t*)RW_MALLOC0_TYPE(sizeof(*ph), rw_profiler_t);
  RW_ASSERT_TYPE(ph, rw_profiler_t);

  ph->tasklet_info = rwtasklet_info;
  ph->duration_secs = duration_secs;
  ph->use_mainq = use_mainq;

  if (use_mainq) {
    ph->rwq = rwsched_dispatch_get_main_queue(
        rwtasklet_info->rwsched_instance);
  } else {
    ph->rwq = rwsched_dispatch_queue_create(
        rwtasklet_info->rwsched_tasklet_info,
        "profiler-serq",
        RWSCHED_DISPATCH_QUEUE_SERIAL);
  }
  RW_ASSERT_TYPE(ph->rwq, rwsched_dispatch_queue_t);

  ph->timer = rwsched_dispatch_source_create(
      rwtasklet_info->rwsched_tasklet_info,
      RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
      0,
      0,
      ph->rwq);
  rwsched_dispatch_source_set_timer(
      rwtasklet_info->rwsched_tasklet_info,
      ph->timer,
      dispatch_time(DISPATCH_TIME_NOW, ((uint64_t)1) * NSEC_PER_SEC / 100ull),
      ((uint64_t)duration_secs) * NSEC_PER_SEC,
      ((uint64_t)1) * NSEC_PER_SEC / 100ull);
  rwsched_dispatch_source_set_event_handler_f(
      rwtasklet_info->rwsched_tasklet_info,
      ph->timer,
      rw_profiler_tick);
  rwsched_dispatch_set_context(
      rwtasklet_info->rwsched_tasklet_info,
      ph->timer,
      ph);
  rwsched_dispatch_resume(
      rwtasklet_info->rwsched_tasklet_info,
      ph->timer);

  return (void*)ph;
}


rw_status_t stop_background_profiler(void* info)
{
  rw_profiler_t *ph = (rw_profiler_t *) info;
  rw_status_t status = RW_STATUS_SUCCESS;
  rwtasklet_info_ptr_t rwtasklet = ph->tasklet_info;

  RW_ASSERT_TYPE(ph, rw_profiler_t);

  rwsched_dispatch_source_cancel(
      rwtasklet->rwsched_tasklet_info,
      ph->timer);
  rwsched_dispatch_release(
      rwtasklet->rwsched_tasklet_info,
      ph->timer);
  ph->timer = NULL;

  if (!ph->use_mainq) {
    rwsched_dispatch_release(
        rwtasklet->rwsched_tasklet_info,
        ph->rwq);
    ph->rwq = NULL;
  }

  if (newpid) {
    if (!m_mod)
      m_mod = rwshell_module_alloc();
    RW_ASSERT(m_mod);

    char *command;
    status = rwshell_perf_stop(
        m_mod,
        newpid,
        &command);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    free(command);
  }

  RW_FREE_TYPE(ph, rw_profiler_t);

  return status;
}
#endif /* RW_BACKGROUND_PROFILING */


static rwdts_member_rsp_code_t on_show_crash_list(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg_unused,
    uint32_t credits,
    void *getnext_ptr)
{
  rw_status_t status;
  rwdts_member_rsp_code_t dts_ret;
  const RWPB_T_PATHSPEC(RwshellMgmt_data_Crash_List) *key = RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_Crash_List);
  RWPB_T_MSG(RwshellMgmt_data_Crash_List) * crash_list = NULL;
  RWPB_T_MSG(RwshellMgmt_data_Crash_List_Vm) *vm = NULL;
  RWPB_T_MSG(RwshellMgmt_data_Crash_List_Vm)** vm_list = NULL;

  RW_ASSERT(xact_info);

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;
  char *instance_name = NULL;

  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_OK;

  crash_list = (RWPB_T_MSG(RwshellMgmt_data_Crash_List) *)malloc(
      sizeof(RWPB_T_MSG(RwshellMgmt_data_Crash_List)));
  RW_ASSERT(crash_list);
  RWPB_F_MSG_INIT(RwshellMgmt_data_Crash_List,crash_list);

  if (!m_mod)
    m_mod = rwshell_module_alloc();
  RW_ASSERT(m_mod);

  int i, crash_count=0;
  char **crashes = NULL;
  if (m_mod) {
    RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

    status = rwshell_crash_report(m_mod, instance_name, &crashes);

    if (status != RW_STATUS_SUCCESS || !crashes) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

    while (crashes[crash_count]) {
      crash_count++;
    }

    if (!crash_count) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

     vm_list = malloc(
        sizeof(RWPB_T_MSG(RwshellMgmt_data_Crash_List_Vm)**));

    vm = malloc(
        sizeof(RWPB_T_MSG(RwshellMgmt_data_Crash_List_Vm)));
    RW_ASSERT(vm);
    RWPB_F_MSG_INIT(RwshellMgmt_data_Crash_List_Vm, vm);

    vm->name = instance_name;

    crash_list->n_vm = 1;
    crash_list->vm = vm_list;
    vm_list[0] = vm;

    if (crash_count) {
      RWPB_T_MSG (RwshellMgmt_data_Crash_List_Vm_Backtrace)** backtrace_list_p = malloc(crash_count*sizeof(*backtrace_list_p));
      for (i=0; i<crash_count; i++) {
          RWPB_T_MSG (RwshellMgmt_data_Crash_List_Vm_Backtrace)* backtrace_p = malloc(crash_count*sizeof(*backtrace_p));
          RWPB_F_MSG_INIT(RwshellMgmt_data_Crash_List_Vm_Backtrace, backtrace_p);
          backtrace_list_p[i] = backtrace_p;
          backtrace_p->id = i;
          backtrace_p->detail = crashes[i];
          crashes[i] = 0;
      }
      vm->n_backtrace = crash_count;
      vm->backtrace = backtrace_list_p;
    }
  }
  else {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  // Finally, send the result.
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage * msgs[1];
  memset (&rsp, 0, sizeof (rsp));
  rsp.msgs = msgs;
  rsp.msgs[0] = &crash_list->base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)key;
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  dts_ret = RWDTS_ACTION_OK;

  //vm->name = NULL;

  if (crashes)
    free(crashes);

done:
  if (crash_list)
    protobuf_c_message_free_unpacked(NULL, &crash_list->base);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_rpc_profiler_start(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret;
  rw_status_t status;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerStart));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;
  char *instance_name= NULL;

  char * specific_instance = ((RWPB_T_MSG(RwshellMgmt_input_ProfilerStart)*)msg)->vm_name;
  unsigned int frequency = ((RWPB_T_MSG(RwshellMgmt_input_ProfilerStart)*)msg)->has_frequency ?
      ((RWPB_T_MSG(RwshellMgmt_input_ProfilerStart)*)msg)->frequency < 1001 ?
      ((RWPB_T_MSG(RwshellMgmt_input_ProfilerStart)*)msg)->frequency :
      100 :
      100; 

  g_perf_stopped = false;
  g_perf_reported = false;

  if (action != RWDTS_QUERY_RPC)
    return RWDTS_ACTION_NA;

  if (g_perf_started)
    return RWDTS_ACTION_NA;

  RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, &pids));
  if (pids) {
    char **commands = NULL;

    if (!(!specific_instance ||
        !strcmp(specific_instance, "all") ||
        !strcmp(specific_instance, instance_name))) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

#ifdef RW_BACKGROUND_PROFILING
      if (g_rw_profiler_handle != NULL) {
        RW_ASSERT(RW_STATUS_SUCCESS == stop_background_profiler(g_rw_profiler_handle));
        g_rw_profiler_handle = NULL;
      }
#endif /* RW_BACKGROUND_PROFILING */

    if (!m_mod)
      m_mod = rwshell_module_alloc();
    RW_ASSERT(m_mod);

    RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerStart, start_op);
    RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerStart_Vm, vm);

    int idx = 0;
    pids_count = 0;
    newpids = NULL;
    while (pids[idx] && pids[idx][0]) {
      char perf_data_fname[999];
      char *command;
      char **dummy = RW_MALLOC0(sizeof(dummy[0]) * 2);
      dummy[0] = pids[idx];
      sprintf(perf_data_fname, "/tmp/rwshell.%s.%s.data", instance_name, pids[idx]);

      status = rwshell_perf_start(m_mod, (const char**)dummy, frequency, perf_data_fname, &command, &newpid);
      pids_count++;
      newpids = realloc(newpids, sizeof(newpids[0]) * pids_count);
      newpids[pids_count - 1] = newpid;
      commands = realloc(commands, sizeof(commands[0]) * pids_count);
      int r = asprintf(&(commands[pids_count - 1]), "pid:%u %s", newpid, command);
      RW_ASSERT(r);
      idx++;
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      g_perf_started = true;
      RW_FREE(command);

    }
    vm.name = instance_name;
    vm.n_command = pids_count;
    vm.command = commands;

    RWPB_T_MSG (RwshellMgmt_output_ProfilerStart_Vm)* vm_list[1];
    start_op.n_vm = 1;
    start_op.vm = vm_list;
    vm_list[0] = &vm;

    // Finally, send the result.
    rwdts_member_query_rsp_t rsp;
    ProtobufCMessage * msgs[1];
    memset (&rsp, 0, sizeof (rsp));
    rsp.msgs = msgs;
    rsp.msgs[0] = &start_op.base;
    rsp.n_msgs = 1;
    rsp.ks = (rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(RwshellMgmt_output_ProfilerStart);
    rsp.evtrsp = RWDTS_EVTRSP_ACK;

    status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    dts_ret = RWDTS_ACTION_OK;

    vm.name = NULL;
    start_op.vm = NULL;
    start_op.n_vm = 0;

    protobuf_free_stack(vm);
    protobuf_free_stack(start_op);

  } else {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

done:
  if (instance_name)
    free(instance_name);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_rpc_profiler_stop(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret;
  rw_status_t status;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerStop));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;
  char *instance_name = NULL;

  //RWPB_T_MSG(RwshellMgmt_input_ProfilerStop) * req = (RWPB_T_MSG(RwshellMgmt_input_ProfilerStop) *)msg;
  char * specific_instance = ((RWPB_T_MSG(RwshellMgmt_input_ProfilerStop)*)msg)->vm_name;

  g_perf_started = false;
  if (action != RWDTS_QUERY_RPC)
    return RWDTS_ACTION_NA;

  if (g_perf_stopped)
    return RWDTS_ACTION_NA;


  RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerStop, stop_op);
  RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerStop_Vm, vm);

  if (m_mod && newpid) {
    char **commands = RW_MALLOC0(sizeof(commands[0]) * pids_count);

    int idx;
    for (idx = 0; idx < pids_count; idx++) {
      status = rwshell_perf_stop(m_mod, newpids[idx], &commands[idx]);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }

    RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

    if (!(!specific_instance ||
        !strcmp(specific_instance, "all") ||
        !strcmp(specific_instance, instance_name))) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

    vm.name = instance_name;
    vm.n_command = pids_count;
    vm.command = commands;

    RWPB_T_MSG (RwshellMgmt_output_ProfilerStop_Vm)* vm_list[1];
    stop_op.n_vm = 1;
    stop_op.vm = vm_list;
    vm_list[0] = &vm;

    g_perf_stopped = true;

#ifdef RW_BACKGROUND_PROFILING
    if (g_background_profiler_enabled) {
      g_rw_profiler_handle =  start_background_profiler(
          tasklet,
          RW_BACKGROUND_PROFILING_PERIOD,
          false);
      RW_ASSERT(g_rw_profiler_handle);
    }
#endif /* RW_BACKGROUND_PROFILING */
  } else {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  // Finally, send the result.
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage * msgs[1];
  memset (&rsp, 0, sizeof (rsp));
  rsp.msgs = msgs;
  rsp.msgs[0] = &stop_op.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(RwshellMgmt_output_ProfilerStop);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  dts_ret = RWDTS_ACTION_OK;

  vm.name = NULL;
  stop_op.vm = NULL;
  stop_op.n_vm = 0;

  protobuf_free_stack(vm);
  protobuf_free_stack(stop_op);

done:
  if (instance_name)
    free(instance_name);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_rpc_profiler_report(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret;
  rw_status_t status;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerReport));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;
  char *instance_name = NULL;
  char **out_datas = RW_MALLOC0(sizeof(out_datas[0]) * pids_count);
  char **commands = RW_MALLOC0(sizeof(commands[0]) * pids_count);

  if (action != RWDTS_QUERY_RPC)
    return RWDTS_ACTION_NA;

  char * specific_instance = ((RWPB_T_MSG(RwshellMgmt_input_ProfilerReport)*)msg)->vm_name;
  double percent_limit = ((RWPB_T_MSG(RwshellMgmt_input_ProfilerReport)*)msg)->has_percent_limit ?
      ((RWPB_T_MSG(RwshellMgmt_input_ProfilerReport)*)msg)->percent_limit :
      1.0;

  RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerReport, report_op);
  RWPB_M_MSG_DECL_INIT(RwshellMgmt_output_ProfilerReport_Vm, vm);

  if (m_mod) {

    RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

    if (!(!specific_instance ||
        !strcmp(specific_instance, "all") ||
        !strcmp(specific_instance, instance_name))) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

    int idx;
    for (idx = 0; idx < pids_count; idx++) {
      char perf_data_fname[999];
      sprintf(perf_data_fname, "/tmp/rwshell.%s.%s.data", instance_name, pids[idx]);
      status = rwshell_perf_report(m_mod, percent_limit, perf_data_fname, &commands[idx], &out_datas[idx]);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }

    vm.name = instance_name;
    vm.n_command = pids_count;
    vm.command = commands;
    vm.n_out_data = pids_count;
    vm.out_data = out_datas;

    RWPB_T_MSG (RwshellMgmt_output_ProfilerReport_Vm)* vm_list[1];
    report_op.n_vm = 1;
    report_op.vm = vm_list;
    vm_list[0] = &vm;

  }
  else {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  // Finally, send the result.
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage * msgs[1];
  memset (&rsp, 0, sizeof (rsp));
  rsp.msgs = msgs;
  rsp.msgs[0] = &report_op.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(RwshellMgmt_output_ProfilerReport);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  dts_ret = RWDTS_ACTION_OK;

  vm.name = NULL;
  vm.command = NULL;
  vm.out_data = NULL;

  report_op.vm = NULL;
  report_op.n_vm = 0;

  protobuf_free_stack(vm);
  protobuf_free_stack(report_op);


done:
  if (instance_name)
    free(instance_name);

  int idx;
  if (pids) {
    for (idx=0; pids[idx]; idx++) {
      free(pids[idx]);
    }
    free(pids);
    pids = NULL;
  }
  return dts_ret;
}

static rwdts_member_rsp_code_t on_show_profiler_report(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret;
  rw_status_t status;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_data_Profiler_Report));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;
  char *instance_name = NULL;
  char *command = NULL;
  char *out_data = NULL;

  if (action != RWDTS_QUERY_READ)
    return RWDTS_ACTION_NA;

  char * specific_instance = msg &&
                        ((RWPB_T_MSG(RwshellMgmt_data_Profiler_Report)*)msg)->n_vm &&
                        ((RWPB_T_MSG(RwshellMgmt_data_Profiler_Report)*)msg)->vm[0] ?
                        ((RWPB_T_MSG(RwshellMgmt_data_Profiler_Report)*)msg)->vm[0]->name
                        : NULL;

  RWPB_M_MSG_DECL_INIT(RwshellMgmt_data_Profiler_Report, report_op);
  RWPB_M_MSG_DECL_INIT(RwshellMgmt_data_Profiler_Report_Vm, vm);

  if (m_mod) {

    RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

    if (!(!specific_instance ||
        !strcmp(specific_instance, "all") ||
        !strcmp(specific_instance, instance_name))) {
      dts_ret = RWDTS_ACTION_NA;
      goto done;
    }

    char perf_data_fname[999];
    sprintf(perf_data_fname, "/tmp/rwprofiler.%s.data.old", instance_name);
    status = rwshell_perf_report(m_mod, 1.0, perf_data_fname, &command, &out_data);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    vm.name = instance_name;
    vm.command = command;
    vm.out_data = out_data;

    RWPB_T_MSG (RwshellMgmt_data_Profiler_Report_Vm)* vm_list[1];
    report_op.n_vm = 1;
    report_op.vm = vm_list;
    vm_list[0] = &vm;

  }
  else {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  // Finally, send the result.
  rwdts_member_query_rsp_t rsp;
  ProtobufCMessage * msgs[1];
  memset (&rsp, 0, sizeof (rsp));
  rsp.msgs = msgs;
  rsp.msgs[0] = &report_op.base;
  rsp.n_msgs = 1;
  rsp.ks = (rw_keyspec_path_t*)RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_Profiler_Report);
  rsp.evtrsp = RWDTS_EVTRSP_ACK;

  status = rwdts_member_send_response(xact_info->xact, xact_info->queryh, &rsp);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  dts_ret = RWDTS_ACTION_OK;

  vm.name = NULL;
  vm.command = NULL;
  vm.out_data = NULL;

  report_op.vm = NULL;
  report_op.n_vm = 0;

  protobuf_free_stack(vm);
  protobuf_free_stack(report_op);


done:
  if (instance_name)
    free(instance_name);
  if (command)
    free(command);
  if (out_data)
    free(out_data);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_background_profiler_enable(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret = RWDTS_ACTION_OK;

  char *instance_name = NULL;
  if (action != RWDTS_QUERY_UPDATE)
    return RWDTS_ACTION_NA;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_data_BackgroundProfiler_Enable));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;

  char * specific_instance = ((RWPB_T_MSG(RwshellMgmt_data_BackgroundProfiler_Enable)*)msg)->vm_name;
  RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

  fprintf(stderr, "specific_instance=%s background_profiler_enable(%s)\n", specific_instance, instance_name);

  if (!(!specific_instance ||
      !strcmp(specific_instance, "all") ||
      !strcmp(specific_instance, instance_name))) {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  g_background_profiler_enabled = 1;

#ifdef RW_BACKGROUND_PROFILING
    if (g_rw_profiler_handle != NULL) {
      RW_ASSERT(RW_STATUS_SUCCESS == stop_background_profiler(g_rw_profiler_handle));
      g_rw_profiler_handle = NULL;
    }

    g_rw_profiler_handle = start_background_profiler(
      tasklet,
      RW_BACKGROUND_PROFILING_PERIOD,
      false);
    RW_ASSERT(g_rw_profiler_handle);
#endif /* RW_BACKGROUND_PROFILING */

  dts_ret = RWDTS_ACTION_OK;

done:
  if (instance_name)
    free(instance_name);

  return dts_ret;
}

static rwdts_member_rsp_code_t on_background_profiler_disable(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* key_in,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void *getnext_ptr)
{
  rwdts_member_rsp_code_t dts_ret = RWDTS_ACTION_OK;

  if (action != RWDTS_QUERY_UPDATE)
    return RWDTS_ACTION_NA;

  RW_ASSERT(xact_info);
  RW_ASSERT(msg->descriptor == RWPB_G_MSG_PBCMD(RwshellMgmt_data_BackgroundProfiler_Disable));

  rwtasklet_info_ptr_t tasklet = (rwtasklet_info_ptr_t) xact_info->ud;

  char *instance_name = NULL;
  char * specific_instance = ((RWPB_T_MSG(RwshellMgmt_data_BackgroundProfiler_Enable)*)msg)->vm_name;
  RW_ASSERT(RW_STATUS_SUCCESS == get_vmname_and_procids(tasklet, &instance_name, NULL));

  fprintf(stderr, "specific_instance=%s background_profiler_disable(%s)\n", specific_instance, instance_name);

  if (!(!specific_instance ||
      !strcmp(specific_instance, "all") ||
      !strcmp(specific_instance, instance_name))) {
    dts_ret = RWDTS_ACTION_NA;
    goto done;
  }

  g_background_profiler_enabled = 0;

#ifdef RW_BACKGROUND_PROFILING
    if (g_rw_profiler_handle != NULL) {
      RW_ASSERT(RW_STATUS_SUCCESS == stop_background_profiler(g_rw_profiler_handle));
      g_rw_profiler_handle = NULL;
    }
#endif /* RW_BACKGROUND_PROFILING */

  dts_ret = RWDTS_ACTION_OK;

done:
  if (instance_name)
    free(instance_name);

  return dts_ret;
}

#define DTS_TBL(_a,_b,_c,_d)  \
    {.keyspec=(rw_keyspec_path_t *)&(_a),.desc=&(_b),.callback={.cb.prepare = (_c),},.flags=(_d)}                                   
rw_status_t
rwlog_sysmgmt_dts_registration (rwtasklet_info_ptr_t tasklet, rwdts_api_t *dts_h)
{
  int i;

  RW_ASSERT(tasklet);
  RW_ASSERT(dts_h);

  /* SHOW Commands */
  static rwdts_registration_t data_regns[] = {

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_Crash_List)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_data_Crash_List)),
            on_show_crash_list, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* Show Crash */

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_Profiler_Report)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_data_Profiler_Report)),
            on_show_profiler_report, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* Show Profiler */
  };

  /* RPC Commands */
  static rwdts_registration_t rpc_regns[] = {

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_input_ProfilerStart)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerStart)),
            on_rpc_profiler_start, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* profiler-start */

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_input_ProfilerStop)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerStop)),
            on_rpc_profiler_stop, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* profiler-stop */

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_input_ProfilerReport)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_input_ProfilerReport)),
            on_rpc_profiler_report, RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_SHARED), /* profiler-report */
  };

  /* CONFIG Commands */
  static rwdts_registration_t config_regns[] = {
    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_BackgroundProfiler_Enable)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_data_BackgroundProfiler_Enable)),
            on_background_profiler_enable, RWDTS_FLAG_SUBSCRIBER), /* background-profiler enable */

    DTS_TBL((*RWPB_G_PATHSPEC_VALUE(RwshellMgmt_data_BackgroundProfiler_Disable)),
            (*RWPB_G_MSG_PBCMD(RwshellMgmt_data_BackgroundProfiler_Disable)),
            on_background_profiler_disable, RWDTS_FLAG_SUBSCRIBER), /* background-profiler disable */
  };


  for (i = 0; i < sizeof (data_regns) / sizeof (rwdts_registration_t); i++) {
    data_regns[i].callback.ud = tasklet;
    rw_keyspec_path_t* lks = NULL;
    rw_keyspec_path_create_dup(data_regns[i].keyspec, NULL, &lks);
    rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_DATA);
    rwdts_member_register(NULL, dts_h, lks, &data_regns[i].callback,
                          data_regns[i].desc, data_regns[i].flags, NULL);
    rw_keyspec_path_free(lks, NULL);
  }

  for (i = 0; i < sizeof (rpc_regns) / sizeof (rwdts_registration_t); i++) {
    rpc_regns[i].callback.ud = tasklet;
    rw_keyspec_path_t* lks = NULL;
    rw_keyspec_path_create_dup(rpc_regns[i].keyspec, NULL, &lks);
    rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_RPC_INPUT);
    rwdts_member_register(NULL, dts_h, lks, &rpc_regns[i].callback,
                          rpc_regns[i].desc, rpc_regns[i].flags, NULL);
    rw_keyspec_path_free(lks, NULL);
  }

  for (i = 0; i < sizeof (config_regns) / sizeof (rwdts_registration_t); i++) {
    config_regns[i].callback.ud = tasklet;
    rw_keyspec_path_t* lks = NULL;
    rw_keyspec_path_create_dup(config_regns[i].keyspec, NULL, &lks);
    rw_keyspec_path_set_category(lks, NULL, RW_SCHEMA_CATEGORY_CONFIG);
    rwdts_member_register(NULL, dts_h, lks, &config_regns[i].callback,
                          config_regns[i].desc, config_regns[i].flags, NULL);
    rw_keyspec_path_free(lks, NULL);
  }

  return RW_STATUS_SUCCESS;
}

