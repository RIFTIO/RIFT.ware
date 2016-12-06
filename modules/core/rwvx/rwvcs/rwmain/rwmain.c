
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


#include <getopt.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <rwdts.h>
#include <rwmsg_clichan.h>
#include <rwtasklet.h>
#include <rwtrace.h>
#include <rwvcs.h>
#include <rwvcs_manifest.h>
#include <rwvcs_internal.h>
#include <rwvcs_rwzk.h>

#include <google/coredumper.h>

#include "rwdts_int.h"

#include "rwmain.h"

#define CHECK_DTS_MAX_ATTEMPTS 100
#define CHECK_DTS_FREQUENCY 100
#define CHECK_PARENT_CHK_FREQ 100
#define RWMAIN_RESTART_DEFERD 10

struct wait_on_dts_cls {
  struct rwmain_gi * rwmain;
  uint16_t attempts;
};

static void
rwmain_process_pm_inputs(
    rwvcs_instance_ptr_t rwvcs,
    rwmain_pm_struct_t *rwmain_pm);
static rwsched_dispatch_queue_t g_zk_rwq = NULL;
static rw_component_type component_type_str_to_enum(const char * type)
{
  if (!strcmp(type, "rwcollection"))
    return RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION;
  else if (!strcmp(type, "rwvm"))
    return RWVCS_TYPES_COMPONENT_TYPE_RWVM;
  else if (!strcmp(type, "rwproc"))
    return RWVCS_TYPES_COMPONENT_TYPE_RWPROC;
  else if (!strcmp(type, "proc"))
    return RWVCS_TYPES_COMPONENT_TYPE_PROC;
  else if (!strcmp(type, "rwtasklet"))
    return RWVCS_TYPES_COMPONENT_TYPE_RWTASKLET;
  else {
    RW_CRASH();
    return RWVCS_TYPES_COMPONENT_TYPE_RWCOLLECTION;
  }
}


/*
 * Schedule the next rwmain phase.  This will run on the scheduler in the
 * next interation.
 *
 * @param rwmain    - rwmain instance
 * @param next      - next phase function
 * @param frequency - times per second to execute timer, 0 to run only once
 * @param ctx       - if defined, context passed to the next phase, otherwise the
 *                    rwmain instance is passed.
 */
static void schedule_next(
    struct rwmain_gi * rwmain,
    rwsched_CFRunLoopTimerCallBack next,
    uint16_t frequency,
    void * ctx)
{
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context;


  bzero(&cf_context, sizeof(rwsched_CFRunLoopTimerContext));
  cf_context.info = ctx ? ctx : rwmain;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent(),
      frequency ? 1.0 / (double)frequency : 0,
      0,
      0,
      next,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);
}

/*
 * Schedule the next rwmain phase.  This will run on the scheduler in the
 * next interation.
 *
 * @param rwmain    - rwmain instance
 * @param next      - next phase function
 * @param interval  - interval of the timer
 * @param ctx       - if defined, context passed to the next phase, otherwise the
 *                    rwmain instance is passed.
 */
static void schedule_interval(
    struct rwmain_gi * rwmain,
    rwsched_CFRunLoopTimerCallBack next,
    uint16_t interval,
    void * ctx)
{
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context;


  bzero(&cf_context, sizeof(rwsched_CFRunLoopTimerContext));
  cf_context.info = ctx ? ctx : rwmain;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + interval, 
      0,
      0,
      0,
      next,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);
}


static void init_rwtrace(rwvx_instance_ptr_t rwvx, char ** argv)
{
  rw_status_t status;
  char * cmdline;

  status = rwtrace_ctx_category_severity_set(
      rwvx->rwtrace,
      RWTRACE_CATEGORY_RWMAIN,
      RWTRACE_SEVERITY_DEBUG);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  status = rwtrace_ctx_category_destination_set(
      rwvx->rwtrace,
      RWTRACE_CATEGORY_RWMAIN,
      RWTRACE_DESTINATION_CONSOLE);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  cmdline = g_strjoinv(" | ", argv);
  RWTRACE_DEBUG(
      rwvx->rwtrace,
      RWTRACE_CATEGORY_RWMAIN,
      "main() entered, cmdline = \"%s\"",
      cmdline);
  g_free(cmdline);
}

int main_function(int argc, char ** argv, char ** envp)
{
  rwmain_setup(argc, argv, envp);

  return 0;
}

static rwtasklet_info_ptr_t get_rwmain_tasklet_info(
    rwvx_instance_ptr_t rwvx,
    const char * component_name,
    int instance_id,
    uint32_t vm_instance_id)
{
  rwtasklet_info_ptr_t info;
  char * instance_name = NULL;
  int broker_instance_id;

  info = (rwtasklet_info_ptr_t)RW_MALLOC0(sizeof(struct rwtasklet_info_s));
  if (!info) {
    RW_CRASH();
    goto err;
  }

  instance_name = to_instance_name(component_name, instance_id);
  if (!instance_name) {
    RW_CRASH();
    goto err;
  }

  info->rwsched_instance = rwvx->rwsched;
  info->rwsched_tasklet_info = rwsched_tasklet_new(rwvx->rwsched);
  info->rwtrace_instance = rwvx->rwtrace;
  info->rwvx = rwvx;
  info->rwvcs = rwvx->rwvcs;

  info->identity.rwtasklet_instance_id = instance_id;
  info->identity.rwtasklet_name = strdup(component_name);
  char *rift_var_root = rwtasklet_info_get_rift_var_root(info);
  RW_ASSERT(rift_var_root);

  rw_status_t status = rw_setenv("RIFT_VAR_ROOT", rift_var_root);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  setenv("RIFT_VAR_ROOT", rift_var_root, true);

  info->rwlog_instance = rwlog_init(instance_name);

  broker_instance_id = 0;
  if (rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker
      && rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->has_enable
      && rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->enable) {
    broker_instance_id = vm_instance_id ? vm_instance_id : 1;
  }

  info->rwmsg_endpoint = rwmsg_endpoint_create(
    1,
    instance_id,
    broker_instance_id,
    info->rwsched_instance,
    info->rwsched_tasklet_info,
    info->rwtrace_instance,
    rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwmsg);

  rwtasklet_info_ref(info);
  free(instance_name);

  return info;

err:
  if (info)
    free(info);

  if (instance_name)
    free(instance_name);

  return NULL;
}

static void rwmain_dts_handle_state_change(rwdts_api_t*  apih, 
                                           rwdts_state_t state,
                                           void*         ud)
{
}

struct rwmain_gi * rwmain_alloc(
    rwvx_instance_ptr_t rwvx,
    const char * component_name,
    uint32_t instance_id,
    const char * component_type,
    const char * parent_id,
    const char * vm_ip_address,
    uint32_t vm_instance_id)
{
  rw_status_t status;
  int r;
  rwtasklet_info_ptr_t info = NULL;
  rwdts_api_t * dts = NULL;
  struct rwmain_gi * rwmain = NULL;
  char * instance_name = NULL;


  rwmain = (struct rwmain_gi *)malloc(sizeof(struct rwmain_gi));
  if (!rwmain) {
    RW_CRASH();
    goto err;
  }
  bzero(rwmain, sizeof(struct rwmain_gi));

  /* If the component name wasn't specified on the command line, pull it
   * from the manifest init-phase.
   */
  if (!component_name) {
    char cn[1024];

    status = rwvcs_variable_evaluate_str(
        rwvx->rwvcs,
        "$rw_component_name",
        cn,
        sizeof(cn));
    if (status != RW_STATUS_SUCCESS) {
      RW_CRASH();
      goto err;
    }

    rwmain->component_name = strdup(cn);
  } else {
    rwmain->component_name = strdup(component_name);
  }

  if (!rwmain->component_name) {
    RW_CRASH();
    goto err;
  }


  /* If the instance id wasn't specified on the command line pull it from
   * the manifest init-phase if it is there, otherwise autoassign one.
   */
  if (instance_id == 0) {
    int id;

    status = rwvcs_variable_evaluate_int(
        rwvx->rwvcs,
        "$instance_id",
        &id);
    if ((status == RW_STATUS_SUCCESS) && id) {
      rwmain->instance_id = (uint32_t)id;
    } else {
      status = rwvcs_rwzk_next_instance_id(rwvx->rwvcs, &rwmain->instance_id, NULL);
      if (status != RW_STATUS_SUCCESS) {
        RW_CRASH();
        goto err;
      }
    }
  } else {
    rwmain->instance_id = instance_id;
  }

  if (component_type) {
    rwmain->component_type = component_type_str_to_enum(component_type);
  } else {
    char ctype[64];
    status = rwvcs_variable_evaluate_str(
        rwvx->rwvcs,
        "$component_type",
        ctype,
        sizeof(ctype));
    if (status != RW_STATUS_SUCCESS) {
      RW_CRASH();
      goto err;
    }
    rwmain->component_type = component_type_str_to_enum(ctype);
  }

  if (vm_instance_id > 0)
    rwmain->vm_instance_id = vm_instance_id;
  else if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM)
    rwmain->vm_instance_id = rwmain->instance_id;
  else {
    int vm_instance_id;
    status = rwvcs_variable_evaluate_int(
        rwvx->rwvcs,
        "$vm_instance_id",
        &vm_instance_id);
    if (status == RW_STATUS_SUCCESS) {
      rwmain->vm_instance_id = (uint32_t)vm_instance_id;
    }
  }
  RW_ASSERT(rwmain->vm_instance_id);


  // 10 hz with tolerance 600
  // TODO: Must take from YANG. These are currently defined as the defaults in rw-base.yang
  rwmain->rwproc_heartbeat = rwproc_heartbeat_alloc(10, 600);
  if (!rwmain->rwproc_heartbeat) {
    RW_CRASH();
    goto err;
  }

  bzero(&rwmain->sys, sizeof(rwmain->sys));

  if (parent_id) {
    rwmain->parent_id = strdup(parent_id);
    if (!rwmain->parent_id) {
      RW_CRASH();
      goto err;
    }
  }
  else {
    char ctype[64];
    status = rwvcs_variable_evaluate_str(
        rwvx->rwvcs,
        "$parent_id",
        ctype,
        sizeof(ctype));
    if (status == RW_STATUS_SUCCESS) {
      rwmain->parent_id = strdup(ctype);
    }
  }


  if (rwvx->rwvcs->pb_rwmanifest->init_phase->settings->rwvcs->collapse_each_rwvm) {
    r = asprintf(&rwmain->vm_ip_address, "127.%u.%u.1", rwmain->instance_id / 256, rwmain->instance_id % 256);
    if (r == -1) {
      RW_CRASH();
      goto err;
    }
  } else if (vm_ip_address) {
    rwmain->vm_ip_address = strdup(vm_ip_address);
    if (!rwmain->vm_ip_address) {
      RW_CRASH();
      goto err;
    }
    char *variable[0];
    r = asprintf(&variable[0], "vm_ip_address = '%s'", vm_ip_address);
    if (r == -1) {
      RW_CRASH();
      goto err;
    }
    status = rwvcs_variable_list_evaluate(
        rwvx->rwvcs,
        1,
        variable);
    if (status != RW_STATUS_SUCCESS) {
      RW_CRASH();
      goto err;
    }
    free(variable[0]);
  } else {
    char buf[32];

    status = rwvcs_variable_evaluate_str(
        rwvx->rwvcs,
        "$vm_ip_address",
        buf,
        sizeof(buf));
    if (status != RW_STATUS_SUCCESS) {
      RW_CRASH();
      goto err;
    }

    rwmain->vm_ip_address = strdup(buf);
    if (!rwmain->vm_ip_address) {
      RW_CRASH();
      goto err;
    }
  }


  rwvx->rwvcs->identity.vm_ip_address = strdup(rwmain->vm_ip_address);
  if (!rwvx->rwvcs->identity.vm_ip_address) {
    RW_CRASH();
    goto err;
  }
  rwvx->rwvcs->identity.rwvm_instance_id = rwmain->vm_instance_id;

  instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
  RW_ASSERT(instance_name!=NULL);
  if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
    rwvx->rwvcs->identity.rwvm_name = instance_name;
  }
  else if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC) {
    RW_ASSERT(rwmain->parent_id);
    rwvx->rwvcs->identity.rwvm_name = strdup(rwmain->parent_id);
  }

  char rift_var_vm[255];
  if (rwvx->rwvcs->identity.rwvm_name) {
    snprintf(rift_var_vm, 255, "%s%c%s", rwvx->rwvcs->pb_rwmanifest->bootstrap_phase->test_name,
             '-', rwvx->rwvcs->identity.rwvm_name);
  } else {
    snprintf(rift_var_vm, 255, "%s", rwvx->rwvcs->pb_rwmanifest->bootstrap_phase->test_name);
  }

  setenv("RIFT_VAR_VM", rift_var_vm, true);
  info = get_rwmain_tasklet_info(
      rwvx,
      rwmain->component_name,
      rwmain->instance_id,
      rwmain->vm_instance_id);
  if (!info) {
    RW_CRASH();
    goto err;
  }

  if (rwvx->rwsched) {
    if (!rwvx->rwsched->rwlog_instance) {
      rwvx->rwsched->rwlog_instance = rwlog_init("RW.Sched");
    }
  }
  if (!rwvx->rwlog) {
    rwvx->rwlog = rwlog_init("Logging");
  }

  dts = rwdts_api_new(
      info,
      (rw_yang_pb_schema_t *)RWPB_G_SCHEMA_YPBCSD(RwVcs),
      rwmain_dts_handle_state_change,
      NULL,
      NULL);

  if (!dts) {
    RW_CRASH();
    goto err;
  }

  RW_SKLIST_PARAMS_DECL(
      procs_sklist_params,
      struct rwmain_proc,
      instance_name,
      rw_sklist_comp_charptr,
      _sklist);
  RW_SKLIST_INIT(&(rwmain->procs), &procs_sklist_params);

  RW_SKLIST_PARAMS_DECL(
      tasklets_sklist_params,
      struct rwmain_tasklet,
      instance_name,
      rw_sklist_comp_charptr,
      _sklist);
  RW_SKLIST_INIT(&(rwmain->tasklets), &tasklets_sklist_params);

  RW_SKLIST_PARAMS_DECL(
      multivms_sklist_params,
      struct rwmain_multivm,
      key,
      rw_sklist_comp_charbuf,
      _sklist);
  RW_SKLIST_INIT(&(rwmain->multivms), &multivms_sklist_params);


  rwmain->dts = dts;
  rwmain->tasklet_info = info;
  rwmain->rwvx = rwvx;
  r = asprintf(&VCS_GET(rwmain)->vcs_instance_xpath,
               VCS_INSTANCE_XPATH_FMT,
               instance_name);
  if (r == -1) {
    RW_CRASH();
    goto err;
  }
  VCS_GET(rwmain)->instance_name = instance_name;

  goto done;

err:
  if (info) {
    rwsched_tasklet_free(info->rwsched_tasklet_info);
    free(info->identity.rwtasklet_name);
    rwmsg_endpoint_halt(info->rwmsg_endpoint);
    free(info);
  }

  if (dts)
    rwdts_api_deinit(dts);

  if (rwmain->component_name)
    free(rwmain->component_name);

  if (rwmain->parent_id)
    free(rwmain->parent_id);

  if (rwmain)
    free(rwmain);

done:

  return rwmain;
}

rw_status_t process_init_phase(struct rwmain_gi * rwmain)
{
  rw_status_t status;
  rwvcs_instance_ptr_t rwvcs;

  rwvcs = rwmain->rwvx->rwvcs;

  if (rwvcs->pb_rwmanifest->init_phase->settings->rwvcs->no_autostart == false) {
    vcs_manifest_component *m_component;
    char * instance_name = NULL;

    instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
    RW_ASSERT(*instance_name);

    // Lookup the component to start
    status = rwvcs_manifest_component_lookup(rwvcs, rwmain->component_name, &m_component);
    rwmain_trace_info(rwmain, "rwvcs_manifest_component_lookup %s", rwmain->component_name);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    if (m_component->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
      RWVCS_LATENCY_CHK_PRE(rwmain->rwvx->rwsched);
      rwmain_rwvm_init(
          rwmain,
          m_component->rwvm,
          rwmain->component_name,
          rwmain->instance_id,
          instance_name,
          rwmain->parent_id);
      RWVCS_LATENCY_CHK_POST(rwmain->rwvx->rwtrace, RWTRACE_CATEGORY_RWMAIN,
                             rwmain_rwvm_init, "rwmain_rwvm_init:%s", instance_name);
    } else if (m_component->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC) {
      RWVCS_LATENCY_CHK_PRE(rwmain->rwvx->rwsched);
      rwmain_rwproc_init(
          rwmain,
          m_component->rwproc,
          rwmain->component_name,
          rwmain->instance_id,
          instance_name,
          rwmain->parent_id);
      RWVCS_LATENCY_CHK_POST(rwmain->rwvx->rwtrace, RWTRACE_CATEGORY_RWMAIN,
                             rwmain_rwproc_init, "rwmain_rwproc_init:%s", instance_name);
    } else {
      rwmain_trace_crit(
          rwmain,
          "rwmain cannot start a component which is not a vm or process (%s)",
          m_component->component_name);
      RW_CRASH();
    }
  }

  return RW_STATUS_SUCCESS;
}

void init_phase(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  rw_status_t status;
  struct rwmain_gi * rwmain;

  rwmain = (struct rwmain_gi *)ctx;

  rwmain_setup_cputime_monitor(rwmain);

  status = rwmain_setup_dts_registrations(rwmain);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  status = process_init_phase(rwmain);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  status = rwmain_setup_dts_rpcs(rwmain);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  if (timer) {
    rwsched_tasklet_CFRunLoopTimerRelease(
        rwmain->rwvx->rwsched_tasklet,
        timer);
  }
}

/*
 * Called when we get a response from DTS with any additional component
 * definitions added to the inventory via runtime configuration.
 *
 * Adds any new components to the manifest held by rwvcs and then schedules
 * the init_phase.
 */
static void on_op_inventory_update(rwdts_xact_t * xact, rwdts_xact_status_t* xact_status, void * ud)
{
  //rw_status_t status;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;
  vcs_manifest_op_inventory * ret_op_inventory;
  vcs_manifest_inventory * inventory;


  rwmain = (struct rwmain_gi *)ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  if (xact_status->status == RWDTS_XACT_FAILURE || xact_status->status == RWDTS_XACT_ABORTED) {
    rwmain_trace_info(rwmain, "Lookup of component probably failed");
    goto done;
  }

  rwmain_trace_info(rwmain, "Updating operational inventory");

  rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);  
  while (qrslt) {
    ret_op_inventory = (vcs_manifest_op_inventory*)(qrslt->message);
    RW_ASSERT(ret_op_inventory);
    RW_ASSERT(ret_op_inventory->base.descriptor == RWPB_G_MSG_PBCMD(RwManifest_data_Manifest_OperationalInventory));

    inventory = rwvcs->pb_rwmanifest->inventory;
    for (size_t i = 0; i < ret_op_inventory->n_component; ++i) {
      // Any updates to the static manifest are going to be ignored.
      if (rwvcs_manifest_have_component(rwvcs, ret_op_inventory->component[i]->component_name)) {
        continue;
      }

      inventory->component = (vcs_manifest_component **)realloc(
          inventory->component,
          sizeof(vcs_manifest_component *) * (inventory->n_component + 1));
      RW_ASSERT(inventory->component);
      inventory->component[inventory->n_component] = (vcs_manifest_component*)protobuf_c_message_duplicate(
          NULL,
          &ret_op_inventory->component[i]->base,
          ret_op_inventory->component[i]->base.descriptor);
      inventory->n_component++;

      rwmain_trace_info(
          rwmain,
          "Updating operational inventory with %s",
          ret_op_inventory->component[i]->component_name);
    }
    qrslt = rwdts_xact_query_result(xact, 0);
  }

done:
  schedule_next(rwmain, init_phase, 0, NULL);
}

/*
 * Request any new components appended to the inventory via runtime
 * configuration from DTS.  Calls on_op_inventory_update to handle the response
 * after which the init_phase will be scheduled.
 */
void request_op_inventory_update(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct rwmain_gi * rwmain;
  vcs_manifest_component query_msg;
  rw_keyspec_path_t * query_key;
  rwdts_xact_t * query_xact;

  rwmain = (struct rwmain_gi *)ctx;


  // The uagent will only publish the 2nd level right now no mater what we request.
  // RIFT-5261
  query_key = ((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwManifest_data_Manifest_OperationalInventory));

  vcs_manifest_component__init(&query_msg);
  //query_msg.component_name = start_req->component_name;


  query_xact = rwdts_api_query_ks(
      rwmain->dts,
      query_key,
      RWDTS_QUERY_READ,
      0,
      on_op_inventory_update,
      rwmain,
      &query_msg.base);

  RW_ASSERT(query_xact);

  rwmain_trace_info(rwmain, "Requested Operational inventory update.");

  rwsched_tasklet_CFRunLoopTimerRelease(
    rwmain->rwvx->rwsched_tasklet,
    timer);
}

/*
 * Called when we get a response from DTS with any additional component
 * definitions added to the inventory via runtime configuration.
 *
 * Adds any new components to the manifest held by rwvcs and then schedules
 * the init_phase.
 */
static void on_inventory_update(rwdts_xact_t * xact, rwdts_xact_status_t* xact_status, void * ud)
{
  //rw_status_t status;
  struct rwmain_gi * rwmain;
  rwvcs_instance_ptr_t rwvcs;
  vcs_manifest_inventory * ret_inventory;
  vcs_manifest_inventory * inventory;


  rwmain = (struct rwmain_gi *)ud;
  rwvcs = rwmain->rwvx->rwvcs;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  if (xact_status->status == RWDTS_XACT_FAILURE || xact_status->status == RWDTS_XACT_ABORTED) {
    rwmain_trace_info(rwmain, "Lookup of component probably failed");
    goto done;
  }

  rwmain_trace_info(rwmain, "Updating inventory");

  rwdts_query_result_t *qrslt = rwdts_xact_query_result(xact, 0);  
  while (qrslt) {
    ret_inventory = (vcs_manifest_inventory*)(qrslt->message);
    RW_ASSERT(ret_inventory);
    RW_ASSERT(ret_inventory->base.descriptor == RWPB_G_MSG_PBCMD(RwManifest_data_Manifest_Inventory));

    inventory = rwvcs->pb_rwmanifest->inventory;
    for (size_t i = 0; i < ret_inventory->n_component; ++i) {
      // Any updates to the static manifest are going to be ignored.
      if (rwvcs_manifest_have_component(rwvcs, ret_inventory->component[i]->component_name)) {
        continue;
      }

      inventory->component = (vcs_manifest_component **)realloc(
          inventory->component,
          sizeof(vcs_manifest_component *) * (inventory->n_component + 1));
      RW_ASSERT(inventory->component);
      inventory->component[inventory->n_component] = (vcs_manifest_component*)protobuf_c_message_duplicate(
          NULL,
          &ret_inventory->component[i]->base,
          ret_inventory->component[i]->base.descriptor);
      inventory->n_component++;

      rwmain_trace_info(
          rwmain,
          "Updating inventory with %s",
          ret_inventory->component[i]->component_name);
    }
    qrslt = rwdts_xact_query_result(xact, 0);
  }

done:
  schedule_next(rwmain, init_phase, 0, NULL);
}

/*
 * Request any new components appended to the inventory via runtime
 * configuration from DTS.  Calls on_inventory_update to handle the response
 * after which the init_phase will be scheduled.
 */
static void request_inventory_update(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct rwmain_gi * rwmain;
  vcs_manifest_component query_msg;
  rw_keyspec_path_t * query_key;
  rwdts_xact_t * query_xact;

  rwmain = (struct rwmain_gi *)ctx;


  // The uagent will only publish the 2nd level right now no mater what we request.
  // RIFT-5261
  query_key = ((rw_keyspec_path_t *)RWPB_G_PATHSPEC_VALUE(RwManifest_data_Manifest_Inventory));

  vcs_manifest_component__init(&query_msg);
  //query_msg.component_name = start_req->component_name;


  query_xact = rwdts_api_query_ks(
      rwmain->dts,
      query_key,
      RWDTS_QUERY_READ,
      0,
      on_inventory_update,
      rwmain,
      &query_msg.base);

  RW_ASSERT(query_xact);

  rwmain_trace_info(rwmain, "Requested inventory update.");

  rwsched_tasklet_CFRunLoopTimerRelease(
    rwmain->rwvx->rwsched_tasklet,
    timer);
}

/*
 * Timer that loops up to CHECK_DTS_MAX_ATTEMPTS times to see if we have a
 * connection to DTS yet.  If a connection has been made (in collapsed mode
 * this takes roughly 2ms) then an updated component inventory will be
 * requested.  If no connection is made, then the inventory update is skipped
 * and the init_phase is scheduled next.
 */
static void check_dts_connected(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct wait_on_dts_cls * cls;

  cls = (struct wait_on_dts_cls *)ctx;

  if (++cls->attempts >= CHECK_DTS_MAX_ATTEMPTS) {
    rwmain_trace_info(
        cls->rwmain,
        "No connection to DTS after %f seconds, skipping inventory update",
        (double)CHECK_DTS_MAX_ATTEMPTS * (1.0 / (double)CHECK_DTS_FREQUENCY));
    schedule_next(cls->rwmain, init_phase, 0, NULL);
    goto finished;
  }

  if (rwdts_get_router_conn_state(cls->rwmain->dts) == RWDTS_RTR_STATE_UP) {
    rwmain_trace_info(
        cls->rwmain,
        "Connected to DTS after %f seconds, scheduling inventory update",
        (double)cls->attempts * (1.0 / (double)CHECK_DTS_FREQUENCY));
    schedule_next(cls->rwmain, request_inventory_update, 0, NULL);
    goto finished;
  }

  return;

finished:
  rwsched_tasklet_CFRunLoopTimerRelease(cls->rwmain->rwvx->rwsched_tasklet, timer);
  free(cls);
}

static void check_parent_started(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct rwmain_gi * rwmain = (struct rwmain_gi *)ctx;
  rw_component_info c_info;

  rw_status_t status = rwvcs_rwzk_lookup_component(rwmain->rwvx->rwvcs, rwmain->parent_id, &c_info);
  RW_ASSERT(status != RW_STATUS_FAILURE);
  if (status == RW_STATUS_NOTFOUND) {
    return;
  }
  else {
    status = rwmain_bootstrap(rwmain);
    RW_ASSERT(status == RW_STATUS_SUCCESS);

    struct wait_on_dts_cls *cls = (struct wait_on_dts_cls *)malloc(sizeof(struct wait_on_dts_cls));
    RW_ASSERT(cls);
    cls->rwmain = rwmain;
    cls->attempts = 0;
    schedule_next(rwmain, check_dts_connected, CHECK_DTS_FREQUENCY, cls);
  }

  rwsched_tasklet_CFRunLoopTimerRelease(rwmain->rwvx->rwsched_tasklet, timer);
}

static void usage() {
  printf("rwmain [ARGUMENTS]\n");
  printf("\n");
  printf("REQUIRED ARGUMENTS:\n");
  printf("  -m,--manifest [PATH]        Path to the XML manifest definition.\n");
  printf("\n");
  printf("ARGUMENTS:\n");
  printf("  -n,--name [NAME]            Name of component being started.\n");
  printf("  -i,--instance [ID]          Instance ID of component being started.\n");
  printf("  -t,--type [TYPE]            Type of component being started.\n");
  printf("  -p,--parent [NAME]          Parent's instance-name.\n");
  printf("  -a,--ip_address [ADDRESS]   VM IP address.\n");
  printf("  -v,--vm_instance [ID]       VM instance id.\n");
  printf("  -C,--core_test <delay_s>    Coredumper test, segv after delay in sec.\n");
  printf("  -h, --help                  This screen.\n");
  printf("\n");
  printf("ENVIRONMENT VARIABLES\n");
  printf("  RIFT_NO_SUDO_REAPER             Run the reaper as the current user\n");
  printf("  RIFT_PROC_HEARTBEAT_NO_REVERSE  Disable reverse heartbeating\n");
  return;
}

#define RW_SIGCT (64)
static struct rwmain_sigjazz {
  char corefname[1024];
  struct sigaction oact[RW_SIGCT];
  size_t maxcore;
} rwmain_sigjazz = {
  .maxcore = 1024,  /* MB */
  .corefname = "core"
};
  
static void rwmain_sigaction_dump(int sig, siginfo_t *sinfo, void *uctx_in) {
  ucontext_t *uctx = (ucontext_t *)uctx_in;
  uctx = uctx;

  int rval = 0;

  write(STDOUT_FILENO, "\n", 1);
  write(STDOUT_FILENO, "\nWriting ", 9);
  write(STDOUT_FILENO, rwmain_sigjazz.corefname, strlen(rwmain_sigjazz.corefname));
  write(STDOUT_FILENO, "\n", 1);
  write(STDOUT_FILENO, "\n", 1);

  const size_t maxcore = rwmain_sigjazz.maxcore * 1024 * 1024 * 2;

 
  rval = prctl(PR_SET_DUMPABLE, 1, 0, 0, 0);
  if (rval) {
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, "prctl PR_SET_DUMPABLE failed\n", strlen("prctl PR_SET_DUMPABLE failed\n"));
    write(STDOUT_FILENO, "\n", 1);
  }

  rval = prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
  if (rval) {
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, "prctl PR_SET_PTRACER failed\n", strlen("prctl PR_SET_PTRACER failed\n"));
    write(STDOUT_FILENO, "\n", 1);
  }
  rval = WriteCoreDumpLimitedByPriority(rwmain_sigjazz.corefname, maxcore);
  if (rval) {
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, "WriteCoredumpLimited failed\n", strlen("WriteCoredumpLimited failed\n"));
    write(STDOUT_FILENO, "\n", 1);
  }
  if (sig == SIGUSR1) {
    return;
  }
  if (!rval) {
    _exit(1);
  }
  raise(sig);
}

static void rwmain_sigaction_setup(void) {

  /* This needs to be under RIFT_INSTALL or whatever, however the
     existing core file scanner assumes cwd or a global /var/rift
     place */
  sprintf(rwmain_sigjazz.corefname, "core.%d", getpid());
  static int sigs[] = {
    SIGSEGV,
    SIGBUS,
    SIGUSR1,
    SIGFPE,
    SIGABRT,
    SIGILL,
    SIGSYS,
    SIGXCPU,
    SIGXFSZ,
    -1
  };
  int i=0;
  int sig;
  uint64_t seen = 0;
  while ((sig = sigs[i++]) >= 0) {
    assert(sig < 64);
    assert(!(seen & (1<<sig)));
    seen = (seen | (1<<sig));
    struct sigaction sa = {
      .sa_sigaction = rwmain_sigaction_dump,
      .sa_flags = ( ((sig == SIGUSR1 || sig == SIGUSR2) ? 0 : SA_RESETHAND) | SA_RESTART | SA_SIGINFO )
    };
    sigaction(sig, &sa, NULL);
  }
}

static void rwmain_sigaction_test(rwsched_CFRunLoopTimerRef timer, void *ctx) {  
  int *ptr = NULL;
  *ptr = 5;
}

static void rwmain_schedule_segv(struct rwmain_gi * rwmain, 
				 int delay_s) {
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context;

  bzero(&cf_context, sizeof(rwsched_CFRunLoopTimerContext));
  cf_context.info = rwmain;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + (double)delay_s,
      delay_s,
      0,
      0,
      rwmain_sigaction_test,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);
}

struct rwmain_gi * rwmain_setup(int argc, char ** argv, char ** envp)
{
  rw_status_t status;
  rwvx_instance_ptr_t rwvx;
  struct rwmain_gi * rwmain;
  struct wait_on_dts_cls * cls;

  char * manifest_file = NULL;
  char * component_name = NULL;
  char * component_type = NULL;
  char * parent = NULL;
  char * ip_address = NULL;
  uint32_t instance_id = 0;
  uint32_t vm_instance_id = 0;
  char * coretest = NULL;

  rwmain_sigaction_setup();

  // We need to reset optind here and after processing the command line options
  // as successive calls to getopt_long will leave optind=1. So, reset here as
  // rwmain or dpdk may have already parsed arguments and reset after as dpdk
  // may need to do processing of its own.  See getopt(3).
  optind = 0;
  while (true) {
    int c;
    long int lu;

    static struct option long_options[] = {
      {"manifest",      required_argument,  0, 'm'},
      {"name",          required_argument,  0, 'n'},
      {"instance",      required_argument,  0, 'i'},
      {"type",          required_argument,  0, 't'},
      {"parent",        required_argument,  0, 'p'},
      {"ip_address",    required_argument,  0, 'a'},
      {"vm_instance",   required_argument,  0, 'v'},
      {"help",          no_argument,        0, 'h'},
      {"core_test",     required_argument,  0, 'C'},
      {0,               0,                  0,  0},
    };

    c = getopt_long(argc, argv, "m:n:i:t:p:a:v:hC:", long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
      case 'C':
	coretest = strdup(optarg);
	break;

      case 'm':
        manifest_file = strdup(optarg);
        RW_ASSERT(manifest_file);
        break;

      case 'n':
        component_name = strdup(optarg);
        RW_ASSERT(component_name);
        break;

      case 'i':
        errno = 0;
        lu = strtol(optarg, NULL, 10);
        RW_ASSERT(errno == 0);
        RW_ASSERT(lu > 0 && lu < UINT32_MAX);
        instance_id = (uint32_t)lu;
        break;

      case 't':
        component_type = strdup(optarg);
        RW_ASSERT(component_type);
        break;

      case 'p':
        parent = strdup(optarg);
        RW_ASSERT(parent);
        break;

      case 'a':
        ip_address = strdup(optarg);
        RW_ASSERT(ip_address);
        break;

      case 'v':
        errno = 0;
        lu = strtol(optarg, NULL, 10);
        RW_ASSERT(errno == 0);
        RW_ASSERT(lu > 0 && lu < UINT32_MAX);
        vm_instance_id = (uint32_t)lu;
        break;

      case 'h':
        usage();
        return 0;
    }
  }
  optind = 0;

  if (!manifest_file) {
    fprintf(stderr, "ERROR:  No manifest file specified.\n");
    exit(1);
  }

  rwvx = rwvx_instance_alloc();
  RW_ASSERT(rwvx);

  rwvcs_instance_ptr_t rwvcs = rwvx->rwvcs;
  RW_ASSERT(rwvcs);

  init_rwtrace(rwvx, argv);
  rwvcs->envp = envp;
  rwvcs->rwmain_exefile = strdup(argv[0]);
  RW_ASSERT(rwvcs->rwmain_exefile);

  status = rwvcs_process_manifest_file (rwvcs, manifest_file);
  bool mgmt_vm = rwvcs_manifest_is_mgmt_vm(rwvcs, component_name);
  rwmain_pm_struct_t pm = {};
  if (mgmt_vm) {
    start_pacemaker_and_determine_role(rwvx, rwvcs->pb_rwmanifest, &pm);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    usleep (RWVCS_DELAY_START);
    if (ip_address)
      free(ip_address);
    ip_address = rwvcs_manifest_get_local_mgmt_addr(rwvcs->pb_rwmanifest->bootstrap_phase);
    RW_ASSERT(ip_address);
    if (pm.i_am_dc) {
      rwvcs->mgmt_info.state = RWVCS_TYPES_VM_STATE_MGMTACTIVE;
      status = rwvcs_variable_list_evaluate(
          rwvcs,
          rwvcs->pb_rwmanifest->init_phase->environment->active_component->n_python_variable,
          rwvcs->pb_rwmanifest->init_phase->environment->active_component->python_variable);
    }
    else {
      rwvcs->mgmt_info.state = RWVCS_TYPES_VM_STATE_MGMTSTANDBY;
      status = rwvcs_variable_list_evaluate(
          rwvcs,
          rwvcs->pb_rwmanifest->init_phase->environment->standby_component->n_python_variable,
          rwvcs->pb_rwmanifest->init_phase->environment->standby_component->python_variable);
    }
  }
  else {
    NEW_DBG_PRINTS("Attempting starting [[%s]] component\n", component_name);
    //usleep (RWVCS_DELAY_START * 4);
  }
  rwvcs_manifest_setup_mgmt_info (rwvcs, mgmt_vm, ip_address);
  rwmain_process_pm_inputs(rwvcs, &pm);

  char *vm_name = NULL;
  if (!component_name) {
    char cn[1024];
    int id;

    status = rwvcs_variable_evaluate_str(
        rwvcs,
        "$rw_component_name",
        cn,
        sizeof(cn));

    status = rwvcs_variable_evaluate_int(
        rwvcs,
        "$instance_id",
        &id);
    vm_name = to_instance_name(cn, id);
  }
    
  if (g_zk_rwq)
    rwvcs->zk_rwq = g_zk_rwq;
  status = rwvcs_instance_init(rwvcs, 
                               manifest_file, 
                               ip_address, 
                               vm_name,
                               main_function);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (!g_zk_rwq && rwvcs->pb_rwmanifest->init_phase->settings->rwvcs->collapse_each_rwprocess)
    g_zk_rwq = rwvcs->zk_rwq;

  rwmain = rwmain_alloc(rwvx, component_name, instance_id, component_type, parent, ip_address, vm_instance_id);
  RW_ASSERT(rwmain);
  rwvx->rwmain = rwmain;
  {
    rwvcs->apih = rwmain->dts;
    RW_SKLIST_PARAMS_DECL(
        config_ready_entries_,
        rwvcs_config_ready_entry_t,
        instance_name,
        rw_sklist_comp_charptr,
        config_ready_elem);
    RW_SKLIST_INIT(
        &(rwvcs->config_ready_entries), 
        &config_ready_entries_);
    rwvcs->config_ready_fn = rwmain_dts_config_ready_process;
  }

  if (coretest) {
    rwmain_schedule_segv(rwmain, atoi(coretest));
    free(coretest);
    coretest = NULL;
  }

  if (rwmain->vm_ip_address) {
    rwvcs->identity.vm_ip_address = strdup(rwmain->vm_ip_address);
    RW_ASSERT(rwvcs->identity.vm_ip_address);
  }
  rwvcs->identity.rwvm_instance_id = rwmain->vm_instance_id;

  rwvcs_publish_active_mgmt_info(rwvcs);
  rwvcs_register_ha_uagent_state(rwvcs);

#if 1
  {
    char instance_id_str[256];
    snprintf(instance_id_str, 256, "%u", rwmain->vm_instance_id);
    status = rw_setenv("RWVM_INSTANCE_ID", instance_id_str);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
  }
#endif

  rw_component_info c_info;
  if (!mgmt_vm) {
    if (rwmain->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
      struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };
      char * instance_name = NULL;
      instance_name = to_instance_name(component_name, instance_id);
      RW_ASSERT(instance_name!=NULL);
      status = rwvcs_rwzk_lock(rwvcs, instance_name, &timeout);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rwvcs_rwzk_lookup_component(rwvcs, instance_name, &c_info);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      RW_ASSERT(c_info.vm_info!=NULL);
      c_info.vm_info->has_pid = true;
      c_info.vm_info->pid = getpid();
      status = rwvcs_rwzk_node_update(rwvcs, &c_info);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rwvcs_rwzk_unlock(rwvcs, instance_name);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      free(instance_name);
    } 
  }
  else  {
    if (rwvcs->mgmt_info.state == RWVCS_TYPES_VM_STATE_MGMTSTANDBY) {
      status = rwvcs_rwzk_lookup_component(rwvcs, rwmain->parent_id, &c_info);
      RW_ASSERT(status != RW_STATUS_FAILURE);
      if (status == RW_STATUS_NOTFOUND) {
        schedule_next(rwmain, check_parent_started, CHECK_PARENT_CHK_FREQ, rwmain);
        goto done;
      }
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
    
    struct rlimit rlimit;
    uint32_t core_limit;
    uint32_t file_limit;
    RW_ASSERT(getrlimit(RLIMIT_CORE, &rlimit) == 0);
    core_limit = rlimit.rlim_max;
    RW_ASSERT(getrlimit(RLIMIT_FSIZE, &rlimit) == 0);
    file_limit = rlimit.rlim_max;
    rwmain_trace_crit_info(
        rwmain,
        "getrlimit(RLIMIT_CORE)=%u getrlimit(RLIMIT_FSIZE)=%u get_current_dir_name()=%s",
        core_limit, file_limit, get_current_dir_name());
  }

  status = rwmain_bootstrap(rwmain);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  cls = (struct wait_on_dts_cls *)malloc(sizeof(struct wait_on_dts_cls));
  RW_ASSERT(cls);
  cls->rwmain = rwmain;
  cls->attempts = 0;

  schedule_next(rwmain, check_dts_connected, CHECK_DTS_FREQUENCY, cls);

done:
  if (manifest_file)
    free(manifest_file);

  if (component_name)
    free(component_name);

  if (component_type)
    free(component_type);

  if (parent)
    free(parent);

  if (ip_address)
    free(ip_address);

  return rwmain;
}

static void
rwmain_stop_local_zk_server(struct rwmain_gi *rwmain)
{
  rwvcs_instance_ptr_t rwvcs = rwmain->rwvx->rwvcs;
  rwvcs_mgmt_info_t *mgmt_info = &rwvcs->mgmt_info;
  int indx;
  for (indx=0; 
       ((indx < RWVCS_ZK_SERVER_CLUSTER) && mgmt_info->zk_server_port_details[indx]);
       indx++) {
    if (mgmt_info->zk_server_port_details[indx]->zk_server_start) {
      char *zk_server_detail = NULL;
      int r = asprintf(&zk_server_detail, "zookeeper_server:%s:%d:%d:%s",
                       mgmt_info->zk_server_port_details[indx]->zk_server_addr,
                       mgmt_info->zk_server_port_details[indx]->zk_client_port,
                       mgmt_info->zk_server_port_details[indx]->zk_server_pid,
                       (mgmt_info->state == RWVCS_TYPES_VM_STATE_MGMTACTIVE)?"ACTIVE":"STANDBY");
      RW_ASSERT(r > 0);
      send_kill_to_pid (mgmt_info->zk_server_port_details[indx]->zk_server_pid,
                        SIGTERM,
                        zk_server_detail);
      fprintf (stderr, "%s is KILLED by %d\n", zk_server_detail, getpid());
      RW_FREE(zk_server_detail);
    }
  }
}

static void
rwmain_process_pm_inputs(
    rwvcs_instance_ptr_t rwvcs,
    rwmain_pm_struct_t *rwmain_pm)
{
  rwvcs_mgmt_info_t *mgmt_info = &rwvcs->mgmt_info;

  if (!rwvcs->pb_rwmanifest->bootstrap_phase 
      || (rwvcs->pb_rwmanifest->bootstrap_phase->n_ip_addrs_list == 1)
      || (!mgmt_info->mgmt_vm)) {
    return;
  }

  int pm_count = 0;
  char *lead_ip_decision = NULL;

  for (int indx = 0; indx < rwmain_pm->n_ip_entries; indx++)
  {
    switch (rwmain_pm->ip_entry[indx].pm_ip_state) {
    case RWMAIN_PM_IP_STATE_LEADER:
    {
      lead_ip_decision = rwmain_pm->ip_entry[indx].pm_ip_addr;
      // Populate the leader VM details
      int ret = inet_aton(rwmain_pm->ip_entry[indx].pm_ip_addr, &mgmt_info->master_vm_info.vm_addr);
      RW_ASSERT (ret != 0);
      strncpy(mgmt_info->master_vm_info.vm_name, rwmain_pm->pm_dc_name, strlen(rwmain_pm->pm_dc_name) + 1);
    }
    //fallthrough
    case RWMAIN_PM_IP_STATE_ACTIVE:
      pm_count++;
      break;
    default:
      break;
    };
  }

  if (!lead_ip_decision) {
    return;
  }

  int inconfig_count = 0;
  int list_indx;
  int lead_ip_count = 0;
  for (list_indx=0; 
       ((list_indx < RWVCS_ZK_SERVER_CLUSTER) 
        && mgmt_info->zk_server_port_details[list_indx]);
       list_indx++) {
    if (mgmt_info->zk_server_port_details[list_indx]->zk_in_config) {
      inconfig_count++;
      mgmt_info->zk_server_port_details[list_indx]->zk_in_config = false;
    }
    if (!strcmp(mgmt_info->zk_server_port_details[list_indx]->zk_server_addr, lead_ip_decision)) {
      lead_ip_count++;
    }
  }
  if (pm_count < lead_ip_count) {
    pm_count = lead_ip_count;
  }

  int new_config_count = 0;

  for (int indx = 0; indx < rwmain_pm->n_ip_entries; indx++) {
    switch (rwmain_pm->ip_entry[indx].pm_ip_state) {
      case RWMAIN_PM_IP_STATE_ACTIVE:
      case RWMAIN_PM_IP_STATE_LEADER:{
        for (list_indx=0; 
             ((list_indx < RWVCS_ZK_SERVER_CLUSTER) 
              && mgmt_info->zk_server_port_details[list_indx]);
             list_indx++) {
          if (!strcmp(rwmain_pm->ip_entry[indx].pm_ip_addr,
                      mgmt_info->zk_server_port_details[list_indx]->zk_server_addr)) {
            mgmt_info->zk_server_port_details[list_indx]->zk_server_disable = false;
            if (((pm_count == 1) && !new_config_count)|| pm_count > 1) {
              mgmt_info->zk_server_port_details[list_indx]->zk_in_config = true;
              new_config_count++;
            }
          }
        }
      }
      break;
      case RWMAIN_PM_IP_STATE_FAILED:{
        int list_indx;
        for (list_indx=0; 
             ((list_indx < RWVCS_ZK_SERVER_CLUSTER) 
              && mgmt_info->zk_server_port_details[list_indx]);
             list_indx++) {
          if (!strcmp(rwmain_pm->ip_entry[indx].pm_ip_addr,
                      mgmt_info->zk_server_port_details[list_indx]->zk_server_addr)) {
            mgmt_info->zk_server_port_details[list_indx]->zk_server_disable = true;
            if (pm_count > 1) {
              mgmt_info->zk_server_port_details[list_indx]->zk_in_config = true;
              new_config_count++;
            }
          }
        }
      }
      break;
      default:
      break;
    }
  }
  if (inconfig_count != new_config_count) {
    mgmt_info->config_start_zk_pending = true;
  }
  NEW_DBG_PRINTS("[[%s]] %s ending %d prev count %d new count %d pm_count\n", 
                 rwvcs->instance_name,
                 mgmt_info->config_start_zk_pending?"PEND":"NOOP",
                 inconfig_count, new_config_count, pm_count);
}

static void
rwmain_handle_zookeeper_handler(
    struct rwmain_gi *rwmain,
    rwmain_pm_struct_t *rwmain_pm)
{
  rwvcs_instance_ptr_t rwvcs = rwmain->rwvx->rwvcs;
  rwmain_process_pm_inputs(rwvcs, rwmain_pm);
  if (rwvcs->mgmt_info.config_start_zk_pending) {
    rw_status_t status = rwvcs_rwzk_client_stop(rwvcs);
    NEW_DBG_PRINTS("rwvcs_rwzk_client_stop from %s\n", rwvcs->instance_name);
    RW_ASSERT(RW_STATUS_SUCCESS == status);

    rwmain_stop_local_zk_server(rwmain);
    NEW_DBG_PRINTS("rwmain_stop_local_zk_server from %s\n", rwvcs->instance_name);
    char *rift_var_root = getenv("RIFT_VAR_ROOT");
    start_zookeeper_server(rwvcs, rwvcs->pb_rwmanifest->bootstrap_phase, rift_var_root);
    NEW_DBG_PRINTS("start_zookeeper_server from %s\n", rwvcs->instance_name);

    status = rwvcs_rwzk_client_start(rwvcs);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
    NEW_DBG_PRINTS("rwvcs_rwzk_client_start from %s\n", rwvcs->instance_name);
  }
}

rw_status_t
rwmain_pm_handler(struct rwmain_gi *rwmain,
                  rwmain_pm_struct_t *rwmain_pm)
{
  RW_ASSERT (rwmain && rwmain_pm);

  rwvcs_instance_ptr_t rwvcs = rwmain->rwvx->rwvcs;
  rwvcs_mgmt_info_t *mgmt_info = &rwvcs->mgmt_info;

  if (mgmt_info->mgmt_vm) {
    rwmain_handle_zookeeper_handler(rwmain, rwmain_pm);
    vcs_vm_state vm_state = RWVCS_TYPES_VM_STATE_MGMTSTANDBY;

    if (rwmain_pm->i_am_dc) {
      vm_state = RWVCS_TYPES_VM_STATE_MGMTACTIVE;
    }

    switch (mgmt_info->state) {
      case RWVCS_TYPES_VM_STATE_MGMTACTIVE:
      {
        switch(vm_state) {
          case RWVCS_TYPES_VM_STATE_MGMTSTANDBY:{
            // TBD
          }
          break;
          case RWVCS_TYPES_VM_STATE_MGMTACTIVE:
          case RWVCS_TYPES_VM_STATE_APPACTIVE:
          case RWVCS_TYPES_VM_STATE_APPSTANDBY:
          default: {
            // NOP
          }
          break;
        }
      }
      break;
      case RWVCS_TYPES_VM_STATE_MGMTSTANDBY:
      {
        switch(vm_state) {
          case RWVCS_TYPES_VM_STATE_MGMTACTIVE:
          {
            rwmain_notify_transition (rwmain, RWVCS_TYPES_VM_STATE_MGMTACTIVE);
            mgmt_info->state = vm_state;
          }
          break;
          case RWVCS_TYPES_VM_STATE_MGMTSTANDBY:
          case RWVCS_TYPES_VM_STATE_APPACTIVE:
          case RWVCS_TYPES_VM_STATE_APPSTANDBY:
          default:
          {
            // standby -> standby actions also needs to be handled
            rwmain_notify_transition (rwmain, mgmt_info->state);
            mgmt_info->state = vm_state;
          }
          break;
        }
      }
      break;
      case RWVCS_TYPES_VM_STATE_APPACTIVE:{
        switch(vm_state) {
          case RWVCS_TYPES_VM_STATE_MGMTACTIVE:
          case RWVCS_TYPES_VM_STATE_MGMTSTANDBY:
          case RWVCS_TYPES_VM_STATE_APPACTIVE:
          case RWVCS_TYPES_VM_STATE_APPSTANDBY:
          default:
          break;
        }
      }
      break;
      case RWVCS_TYPES_VM_STATE_APPSTANDBY:
      {
        switch(vm_state) {
          case RWVCS_TYPES_VM_STATE_MGMTACTIVE:
          case RWVCS_TYPES_VM_STATE_MGMTSTANDBY:
          case RWVCS_TYPES_VM_STATE_APPACTIVE:
          case RWVCS_TYPES_VM_STATE_APPSTANDBY:
          default:
          break;
        }
      }
      break;
      default:
      break;
    }
  }
  return RW_STATUS_SUCCESS;
}



static void restart_deferred(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  rwmain_restart_instance_t *restart_instance = (rwmain_restart_instance_t *)ctx;
  RW_ASSERT_TYPE(restart_instance, rwmain_restart_instance_t);
  struct rwmain_gi *rwmain = restart_instance->rwmain;
  rwsched_tasklet_CFRunLoopTimerRelease(rwmain->rwvx->rwsched_tasklet, timer);
  ck_pr_store_int(&rwmain->rwvx->rwvcs->restart_inprogress, 0);

  rwmain_restart_instance_t *next = NULL;
  while (restart_instance) {
    RW_ASSERT_TYPE(restart_instance, rwmain_restart_instance_t);
    next = restart_instance->next_restart_instance;

    handle_recovery_action_instance_name(rwmain, restart_instance->instance_name);
    RW_FREE(restart_instance->instance_name);
    RW_FREE_TYPE(restart_instance, rwmain_restart_instance_t);
    restart_instance = next;
  }
}

void
rwmain_restart_deferred(rwmain_restart_instance_t *restart_list)
{
  RW_ASSERT_TYPE(restart_list, rwmain_restart_instance_t);
  struct rwmain_gi *rwmain = restart_list->rwmain;
  RWTRACE_CRIT(
      rwmain->rwvx->rwtrace,
      RWTRACE_CATEGORY_RWVCS,
      "SCHEDULE_INTERVAL %d by %s **************************************",
      RWMAIN_RESTART_DEFERD,
      rwmain->rwvx->rwvcs->instance_name);
  schedule_interval(rwmain, restart_deferred, RWMAIN_RESTART_DEFERD, restart_list);
}
