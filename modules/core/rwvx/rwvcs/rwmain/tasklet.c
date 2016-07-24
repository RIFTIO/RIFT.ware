
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <strings.h>

#include <rwsched.h>
#include <rwvcs_rwzk.h>

#include "rwmain.h"
#include "rwtrace_gi.h"

// Amount of time we allow for a tasklet to hit the RUNNING state before
// assuming that it's dead.
#define MAX_TASKLET_STARTUP_TIME 300


/*
 * Timeout that runs after MAX_TASKLET_STARTUP_TIME seconds to verify that the
 * tasklet has transisitioned to the RUNNING phase.  If not, the tasklet will
 * be stopped and the state marked as CRASHED.
 */
static void kill_not_running_tasklet(rwsched_CFRunLoopTimerRef timer, void * ctx);


static struct rwtasklet_info_s * create_tasklet_info(
    rwvcs_instance_ptr_t rwvcs,
    rwmain_tasklet_mode_active_t *mode_active,
    const char * tasklet_instance_name,
    uint32_t tasklet_instance_id)
{
  struct rwtasklet_info_s * info;
  int broker_id = 0;

  info = (struct rwtasklet_info_s *)malloc(sizeof(struct rwtasklet_info_s));
  RW_ASSERT(info);
  bzero(info, sizeof(struct rwtasklet_info_s));


  if (rwvcs->pb_rwmanifest->init_phase->settings->rwmsg
      && rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker
      && rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->has_enable
      && rwvcs->pb_rwmanifest->init_phase->settings->rwmsg->multi_broker->enable) {
    broker_id = rwvcs->identity.rwvm_instance_id ? rwvcs->identity.rwvm_instance_id : 1;
  }

  info->rwsched_instance = rwvcs->rwvx->rwsched;
  info->rwsched_tasklet_info = rwsched_tasklet_new(rwvcs->rwvx->rwsched);
  //info->rwtrace_instance = rwvcs->rwvx->rwtrace;
  info->rwtrace_instance = rwtrace_init(); 
  info->rwvx = rwvcs->rwvx;
  info->rwvcs = rwvcs;
  info->identity.rwtasklet_instance_id = tasklet_instance_id;
  info->mode.has_mode_active = mode_active->has_mode_active;
  info->mode.mode_active = mode_active->mode_active;
  info->identity.rwtasklet_name = split_component_name(tasklet_instance_name);
  info->rwlog_instance = rwlog_init(tasklet_instance_name);

  info->rwmsg_endpoint = rwmsg_endpoint_create(
      1,
      tasklet_instance_id,
      broker_id,
      info->rwsched_instance,
      info->rwsched_tasklet_info,
      info->rwtrace_instance,
      rwvcs->pb_rwmanifest->init_phase->settings->rwmsg);

  return info;
}

struct rwmain_tasklet * rwmain_tasklet_alloc(
    const char * instance_name,
    uint32_t instance_id,
    rwmain_tasklet_mode_active_t *mode_active,
    const char * plugin_name,
    const char * plugin_dir,
    rwvcs_instance_ptr_t rwvcs)
{
  rw_status_t status;
  struct rwmain_tasklet * rt;
  rw_vx_modinst_common_t * mip;
  struct rwtasklet_info_s * tinfo = NULL;
  RwTaskletPlugin_RWExecURL * url;

  rt = (struct rwmain_tasklet *)malloc(sizeof(struct rwmain_tasklet));
  if (!rt) {
    RW_CRASH();
    return NULL;
  }

  bzero(rt, sizeof(struct rwmain_tasklet));

  rt->instance_id = instance_id;

  rt->instance_name = strdup(instance_name);
  if (!rt->instance_name) {
    RW_CRASH();
    goto free_tasklet;
  }

  if (plugin_dir)
    rw_vx_add_peas_search_path(rwvcs->rwvx_instance, plugin_dir);

  RWVCS_LATENCY_CHK_PRE(rwvcs->rwvx->rwsched);
  status = rw_vx_library_open(rwvcs->rwvx_instance, (char *)plugin_name, (char *)"", &mip);
  RWVCS_LATENCY_CHK_POST(rwvcs->rwvx->rwtrace, RWTRACE_CATEGORY_RWTASKLET,
                         rw_vx_library_open, "rw_vx_library_open:%s:%s for %s", 
                         plugin_name, plugin_dir, instance_name);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    goto free_tasklet;
  }

  tinfo = create_tasklet_info(rwvcs, mode_active, instance_name, instance_id);
  if (!tinfo) {
    RW_CRASH();
    goto close_library;
  }

  rt->tasklet_info = tinfo;
  rwtasklet_info_ref(tinfo);

  status = rw_vx_library_get_api_and_iface(
      mip,
      RW_TASKLET_PLUGIN_TYPE_COMPONENT,
      (void **)&rt->plugin_klass,
      (void **)&rt->plugin_interface,
      NULL);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    goto close_library;
  }

  url = RW_TASKLET_PLUGIN__RWEXECURL(g_object_new(RW_TASKLET_PLUGIN_TYPE__RWEXECURL, 0));
  if (!RW_TASKLET_PLUGIN_IS__RWEXECURL(url)) {
    RW_CRASH();
    goto close_library;
  }

  rt->h_component = rt->plugin_interface->component_init(rt->plugin_klass);
  RWVCS_LATENCY_CHK_PRE(rwvcs->rwvx->rwsched);
  rt->h_instance = rt->plugin_interface->instance_alloc(
      rt->plugin_klass,
      rt->h_component,
      tinfo,
      url);
  RWVCS_LATENCY_CHK_POST(rwvcs->rwvx->rwtrace, RWTRACE_CATEGORY_RWTASKLET,
                         rt->plugin_interface->instance_alloc, "rt->plugin_interface->instance_alloc:%s:%s for %s", 
                         plugin_name, plugin_dir, instance_name);

  return rt;

close_library:
  rw_vx_library_close(mip, false);

free_tasklet:
  if (tinfo)
    rwtasklet_info_unref(tinfo);

  if (rt->instance_name)
    free(rt->instance_name);

  free(rt);

  return NULL;
}

void rwmain_tasklet_free(struct rwmain_tasklet * rt)
{
  if (rt->plugin_klass)
    rt->plugin_interface->instance_stop(
        rt->plugin_klass,
        rt->h_component,
        rt->h_instance);

  if (rt->kill_if_not_running) {
    RW_ASSERT(rt->rwmain);

    rwsched_tasklet_CFRunLoopTimerRelease(
        rt->rwmain->rwvx->rwsched_tasklet,
        rt->kill_if_not_running);
  }

  if (rt->instance_name)
    free(rt->instance_name);

  if (rt->tasklet_info)
    rwtasklet_info_unref(rt->tasklet_info);

  free(rt);
}

rw_status_t rwmain_tasklet_start(
    struct rwmain_gi * rwmain,
    struct rwmain_tasklet * rt)
{
  rw_status_t status;
  rwsched_CFRunLoopTimerContext cf_context;


  bzero(&cf_context, sizeof(rwsched_CFRunLoopTimerContext));

  status = RW_SKLIST_INSERT(&(rwmain->tasklets), rt);
  if (status != RW_STATUS_SUCCESS) {
    rwmain_trace_crit(
        rwmain,
        "Failed to add tasklet %s to rwmain sklist",
        rt->instance_name);
    RW_CRASH();
    return status;
  }

  RWVCS_LATENCY_CHK_PRE(rwmain->rwvx->rwsched);
  rt->plugin_interface->instance_start(
      rt->plugin_klass,
      rt->h_component,
      rt->h_instance);
  RWVCS_LATENCY_CHK_POST(rwmain->rwvx->rwtrace, RWTRACE_CATEGORY_RWTASKLET,
                         rt->plugin_interface->instance_start, 
                         "rt->plugin_interface->instance_start:%s start", rt->instance_name);

  cf_context.info = rt;
  rt->kill_if_not_running = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + MAX_TASKLET_STARTUP_TIME,
      0,
      0,
      0,
      kill_not_running_tasklet,
      &cf_context);

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      rt->kill_if_not_running,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);

  return RW_STATUS_SUCCESS;
}

static void kill_not_running_tasklet(rwsched_CFRunLoopTimerRef timer, void * ctx) {
  rw_status_t status;
  rw_component_info info;
  struct rwmain_tasklet * rt;

  rt = (struct rwmain_tasklet *)ctx;

  status = rwvcs_rwzk_lookup_component(rt->rwmain->rwvx->rwvcs, rt->instance_name, &info);
  if (status != RW_STATUS_SUCCESS) {
    rwmain_trace_info(
        rt->rwmain,
        "Failed to lookup %s to check state, ignoring",
        rt->instance_name);
    goto cleanup;
  }

  if (info.state != RW_BASE_STATE_TYPE_RUNNING) {
    rwmain_trace_crit(
        rt->rwmain,
        "Tasklet %s still not in state RUNNING.  Killing.  (OR WOULD DO, JUST PRETENDING FOR NOW)",
        rt->instance_name);

#if 0
    /* Caveat Emptor -- most stop functions don't really work */
    status = rwvcs_rwzk_update_state(
        rt->rwmain->rwvx->rwvcs,
        rt->instance_name,
        RW_BASE_STATE_TYPE_CRASHED);
    if (status != RW_STATUS_SUCCESS)
      rwmain_trace_crit(
          rt->rwmain,
          "Failed to mark %s as CRASHED",
          rt->instance_name);

    status = RW_SKLIST_REMOVE_BY_KEY(&(rt->rwmain->tasklets), &rt->instance_name, NULL);
    if (status != RW_STATUS_SUCCESS)
      rwmain_trace_crit(
          rt->rwmain,
          "Failed to remove tasklet %s from rwmain sklist",
          rt->instance_name);

    rwmain_tasklet_free(rt);
#endif
  }

  protobuf_free_stack(info);

cleanup:
  rwsched_tasklet_CFRunLoopTimerRelease(
      rt->rwmain->rwvx->rwsched_tasklet,
      rt->kill_if_not_running);
  rt->kill_if_not_running = NULL;

  return;
}

