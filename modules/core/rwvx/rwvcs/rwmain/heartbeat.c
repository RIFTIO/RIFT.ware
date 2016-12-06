
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

#include <fcntl.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <reaper_client.h>
#include <rwsched.h>
#include <rwtrace.h>
#include <rwvcs_rwzk.h>
#include <rwvx.h>
#include <rwvcs_manifest.h>
#include <rwvcs_internal.h>

#include "rwmain.h"

// Maximum message size
#define HEARTBEAT_MSG_SIZE 1024

// Statistic history length
#define HEARTBEAT_HISTORY_SIZE 20

// Number of buckes to keep in the missed histogram
#define HEARTBEAT_HISTOGRAM_SIZE 100

// Time duration for each histogram bucket
#define HEARTBEAT_HISTOGRAM_DELTA 10

struct max_timer_cls;

struct stats {
  bool active;
  double recv_time;
  double poll_time;
  double send_time;
  uint32_t missed;
};

struct subscriber_cls {
  rwmain_gi_t * rwmain;
  char * instance_name;
  mqd_t mqd;
  uint32_t missed;
  rwsched_CFRunLoopTimerRef timer;
  struct max_timer_cls * max_timer_cls;

  // Statistics
  double init_duration;
  size_t stat_id;
  struct stats stats[HEARTBEAT_HISTORY_SIZE];

  struct {
    uint32_t histogram[HEARTBEAT_HISTOGRAM_SIZE];
    size_t index;
    int started_at;
  } missed_histogram;
};

struct max_timer_cls {
  struct subscriber_cls * sub_cls;
  rwsched_CFRunLoopTimerRef timer;
};

struct publisher_cls {
  rwmain_gi_t * rwmain;
  mqd_t mqd;
  bool skip_hb;
  rwsched_CFRunLoopTimerRef timer;
  rwsched_CFRunLoopTimerRef subscriber_timeout;

  // Statistics
  size_t stat_id;
  struct stats stats[HEARTBEAT_HISTORY_SIZE];
};

struct rwproc_heartbeat {
  uint16_t frequency;
  uint32_t tolerance;
  bool enabled;
  struct subscriber_cls ** subs;
  struct publisher_cls ** pubs;
};

/*
 * Get the current time
 */
static inline double current_time() {
  int r;
  struct timespec ts;

  r = clock_gettime(CLOCK_REALTIME, &ts);
  if (r)
    return -1;

  return (double)ts.tv_sec + (double)ts.tv_nsec / 1E9;
}


rw_status_t  process_component_death(
    rwmain_gi_t * rwmain,
    char *instance_name,
    rw_component_info *cinfo)
{
  rw_status_t status = rwvcs_rwzk_lookup_component(rwmain->rwvx->rwvcs, instance_name, cinfo);
  RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_NOTFOUND);
  if (status == RW_STATUS_NOTFOUND) {
    return status;
  }

  bool restart = cinfo->has_recovery_action && (cinfo->recovery_action == RWVCS_TYPES_RECOVERY_TYPE_RESTART);
  cinfo->state = restart ? RW_BASE_STATE_TYPE_TO_RECOVER: RW_BASE_STATE_TYPE_CRASHED;
  status = rwvcs_rwzk_node_update(rwmain->rwvx->rwvcs, cinfo);
  if (status != RW_STATUS_SUCCESS && status != RW_STATUS_NOTFOUND) {
    rwmain_trace_crit(
        rwmain,
        "Failed to update %s state to %s",
        instance_name, restart?"TO_RECOVER":"CRASHED");
  } 
  else {
    if (!restart) {
       rwmain_trace_crit(
        rwmain,
        "Updating %s state to CRASHED",
        instance_name);
    }
  }
  if (cinfo->state == RW_BASE_STATE_TYPE_CRASHED) {
    rwmain_trace_crit(
        rwmain,
        "%s CRASHED",
        instance_name);
  }
  int r;
  char *path = NULL;
  char *zklock = NULL;
  char *rwzk_path = NULL;
  rwvcs_instance_ptr_t rwvcs = rwmain->rwvx->rwvcs;
  if (!strcmp(cinfo->component_name, "dtsrouter")) {
    // Find the VM name going up the tree
    int vm_id = split_instance_id(cinfo->rwcomponent_parent);
    r = asprintf(&path, "/R/RW.DTSRouter/%d", vm_id);
    RW_ASSERT(r != -1);
    r = asprintf(&zklock, "%s-lOcK/routerlock", path);
    RW_ASSERT(r != -1);
    r = asprintf(&rwzk_path, "/sys-router%s", path);
    RW_ASSERT(r != -1);
    if (rwvcs_rwzk_exists(rwvcs, rwzk_path)) {
      struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };
      status = rwvcs_rwzk_lock_path(rwvcs, zklock, &tv);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rwvcs_rwzk_delete_path(rwvcs, rwzk_path);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rwvcs_rwzk_unlock_path(rwvcs, zklock);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
      status = rwvcs_rwzk_delete_path(rwvcs, zklock);
      RW_ASSERT(status == RW_STATUS_SUCCESS);
    }
  }
  else {
    r = asprintf(&path, "/R/%s/%lu", cinfo->component_name, cinfo->instance_id);
    RW_ASSERT(r != -1);
    if (!strcmp(cinfo->component_name, "msgbroker")) {
      // Find the VM name going up the tree
      int vm_id = split_instance_id(cinfo->rwcomponent_parent);
      r = asprintf(&zklock, "/sys/rwmsg/broker-lock/lock-1");
      RW_ASSERT(r != -1);

      if (!rwvcs_rwzk_exists(rwvcs, zklock)) {
        status = rwvcs_rwzk_create(rwvcs, zklock);
        RW_ASSERT (status == RW_STATUS_SUCCESS);
      }
      r = asprintf(&rwzk_path, "/sys-1/rwmsg/broker/inst-%d", vm_id);
      RW_ASSERT(r != -1);

      if (rwvcs_rwzk_exists(rwvcs, rwzk_path)) {
        struct timeval tv = { .tv_sec = 120, .tv_usec = 1000 };
        status = rwvcs_rwzk_lock_path(rwvcs, zklock, &tv);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
        status = rwvcs_rwzk_delete_path(rwvcs, rwzk_path);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
        status = rwvcs_rwzk_unlock_path(rwvcs, zklock);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
        status = rwvcs_rwzk_delete_path(rwvcs, zklock);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
      }
    }
  }
  if (zklock) {
    free(zklock);
  }
  if (rwzk_path) {
    free(rwzk_path);
  }

  status = rwdts_member_deregister_path(
      rwmain->dts, path,
      cinfo->has_recovery_action? cinfo->recovery_action: RWVCS_TYPES_RECOVERY_TYPE_FAILCRITICAL);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  free(path);

  int indx;
  rw_component_info chinfo;

  for (indx=0; indx < cinfo->n_rwcomponent_children; indx++) {
    rw_status_t child_status = process_component_death (rwmain, cinfo->rwcomponent_children[indx], &chinfo);
    if (child_status == RW_STATUS_SUCCESS) {
      protobuf_free_stack(chinfo);
    }
  }

  if (cinfo->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC) {
    struct rwmain_proc * rp;
    rw_status_t rs = RW_SKLIST_REMOVE_BY_KEY(&(rwmain->procs), &cinfo->instance_name, &rp);
    if (rs == RW_STATUS_SUCCESS) rwmain_proc_free(rp);
  }

  return status;
}

void kill_component(
    rwmain_gi_t * rwmain,
    char *instance_name,
    rw_component_info *ci)
{
  rw_status_t status;

  status = process_component_death(rwmain, instance_name, ci);
  RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_NOTFOUND);

  if (status != RW_STATUS_SUCCESS)
    return;

  if (ci->proc_info) {
    int pid = ci->proc_info->pid;
    send_kill_to_pid(pid, SIGABRT, instance_name);
  }

  return;
}

static void handle_recovery_action(rwmain_gi_t * rwmain,
                            rw_component_info *ci)
{
  if (ci->has_recovery_action && (ci->recovery_action== RWVCS_TYPES_RECOVERY_TYPE_RESTART)) {
    if ((ci->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC)
        || (ci->component_type == RWVCS_TYPES_COMPONENT_TYPE_PROC)) {
      vcs_manifest_component *m_component = NULL;
      rw_status_t status = rwvcs_manifest_component_lookup(
          rwmain->rwvx->rwvcs,
          ci->component_name,
          &m_component);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
      char *instance_name = NULL;
      rwmain_trace_crit(
          rwmain,
          "[%s]Instance name %s:Component Names %s:Recovery %d:Active %d:Parent %s", 
          rwmain->rwvx->rwvcs->instance_name, 
          ci->instance_name,
          ci->component_name,
          ci->recovery_action,
          ci->mode_active,
          ci->rwcomponent_parent);
      start_component(rwmain,
                      ci->component_name,
                      NULL,
                      RW_BASE_ADMIN_COMMAND_RECOVER,
                      ci->rwcomponent_parent,
                      &instance_name,
                      m_component);
    }
    else if (ci->component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM) {
      vcs_manifest_component *m_component = NULL;
      rw_status_t status = rwvcs_manifest_component_lookup(
          rwmain->rwvx->rwvcs,
          ci->component_name,
          &m_component);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
      char *instance_name = NULL;
      rwmain_trace_crit(
          rwmain,
          "starting recovery of %s",
          ci->component_name);

      start_component(rwmain,
                      ci->component_name,
                      rwmain->vm_ip_address,
                      RW_BASE_ADMIN_COMMAND_RECOVER,
                      ci->rwcomponent_parent,
                      &instance_name,
                      m_component);
    }
  }
}

void handle_recovery_action_instance_name(
    rwmain_gi_t * rwmain,
    char *instance_name)
{
  rw_component_info ci;
  rw_status_t status = rwvcs_rwzk_lookup_component(rwmain->rwvx->rwvcs, instance_name, &ci);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  handle_recovery_action(rwmain, &ci);
  protobuf_free_stack(ci);
}

void restart_process(
    rwmain_gi_t * rwmain,
    char *instance_name)
{
  rw_component_info ci;
  rw_status_t status;

  status = process_component_death(rwmain, instance_name, &ci);
  RW_ASSERT(status == RW_STATUS_SUCCESS || status == RW_STATUS_NOTFOUND);

  if (status != RW_STATUS_SUCCESS)
    return;


  bool found = false;
  struct subscriber_cls * cls = NULL;
  for (size_t i = 0; rwmain->rwproc_heartbeat->subs[i]; ++i) {
    if (!strcmp(rwmain->rwproc_heartbeat->subs[i]->instance_name, instance_name)) {
      found = true;
      cls = rwmain->rwproc_heartbeat->subs[i];
    }

    if (found)
      rwmain->rwproc_heartbeat->subs[i] = rwmain->rwproc_heartbeat->subs[i+1];
  }

  if (cls) {
    if (cls->max_timer_cls != NULL) {
      rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->max_timer_cls->timer);
      free(cls->max_timer_cls);
      cls->max_timer_cls = NULL;
    }
    rwsched_tasklet_CFRunLoopTimerRelease(cls->rwmain->rwvx->rwsched_tasklet, cls->timer);
    mq_close(cls->mqd);
    free(cls->instance_name);
    free(cls);
  }

  handle_recovery_action(rwmain, &ci);
}


/* Fired after a maximum delay waiting for the subscriber to read enough
 * messages from the queue so that the publisher has room to continue to send
 * heartbeats.
 *
 * Timer added by the publisher when the queue is filled and removed by the
 * publisher as soon as there is room on the queue.
 */
static void on_subscriber_timeout(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct publisher_cls * cls;
  char * instance_name;
  rw_component_info self;
  rw_status_t status;


  cls = (struct publisher_cls *)ctx;

  rwmain_trace_crit(
      cls->rwmain,
      "Timeout waiting for subscriver to flush the message queue.  Exiting.");

  rwsched_tasklet_CFRunLoopTimerRelease(cls->rwmain->rwvx->rwsched_tasklet, cls->timer);

  instance_name = to_instance_name(cls->rwmain->component_name, cls->rwmain->instance_id);

  status = rwvcs_rwzk_update_state(cls->rwmain->rwvx->rwvcs, instance_name, RW_BASE_STATE_TYPE_CRASHED);
  if (status != RW_STATUS_SUCCESS) {
    rwmain_trace_crit(
        cls->rwmain,
        "subscriber timeout %s CRASHED: Failed to update component state",
        instance_name);
  }
  else {
    rwmain_trace_crit(cls->rwmain, "Updating  %s state to CRASHED", instance_name);
  }

  status = rwvcs_rwzk_lookup_component(
      cls->rwmain->rwvx->rwvcs,
      instance_name,
      &self);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    exit(1);
  }

  status = rwmain_stop_instance(cls->rwmain, &self);
  if (status != RW_STATUS_SUCCESS) {
    RW_CRASH();
    exit(1);
  }

  free(instance_name);
}

/*
 * Fired after a maximum delay waiting for the publisher to send the first
 * heartbeat.  Removes the subscriber timer and cleans out all allocated
 * heartbeat resources.
 *
 * Note this ideally this will never fire as the first message received by the
 * subscriber will trigger the remove of this timer.
 */
static void on_subscriber_delay_timeout(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  int r;
  struct max_timer_cls * cls;
  char * path;
  bool found;

  cls = (struct max_timer_cls *)ctx;

  rwmain_trace_crit(
      cls->sub_cls->rwmain,
      "Timeout waiting for first heartbeat from %s, assuming dead",
      cls->sub_cls->instance_name);

  rwsched_tasklet_CFRunLoopTimerRelease(
      cls->sub_cls->rwmain->rwvx->rwsched_tasklet,
      cls->sub_cls->timer);
  rw_component_info ci;
  kill_component(cls->sub_cls->rwmain, cls->sub_cls->instance_name, &ci);

  r = asprintf(&path, "/%s", cls->sub_cls->instance_name);
  if (r != -1) {
    mq_unlink(path);
    free(path);
  }

  found = false;
  for (size_t i = 0; cls->sub_cls->rwmain->rwproc_heartbeat->subs[i]; ++i) {
    if (cls->sub_cls->rwmain->rwproc_heartbeat->subs[i] == cls->sub_cls)
      found = true;

    if (found)
      cls->sub_cls->rwmain->rwproc_heartbeat->subs[i] = cls->sub_cls->rwmain->rwproc_heartbeat->subs[i+1];
  }

  free(cls->sub_cls->instance_name);
  mq_close(cls->sub_cls->mqd);
  free(cls->sub_cls);
  free(cls);
}

static void check_mq_heartbeat(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  struct subscriber_cls * cls;
  char buf[HEARTBEAT_MSG_SIZE+1];
  ssize_t read;
  bool got_beat = false;
  size_t sent_id;
  double now = 0.0;

  cls = (struct subscriber_cls *)ctx;

  if (likely(!cls->stats[cls->stat_id].active)) {
    cls->stats[cls->stat_id].active = true;
    cls->stats[cls->stat_id].poll_time = current_time();
  }

  // Make sure to flush the queue each time.
  while (true) {
    read = mq_receive(cls->mqd, buf, HEARTBEAT_MSG_SIZE+1, NULL);
    if (read == -1) {
      RW_ASSERT(errno == EAGAIN);
      break;
    }

    RW_ASSERT(read == sizeof(size_t));
    memcpy(&sent_id, buf, sizeof(size_t));

    if (unlikely(sent_id != cls->stat_id)) {
      RW_CRASH();
      // Not sure how this could happen, but we better check.  This should be
      // clear in the stats as we'll have some set which have a poll time but
      // no recv time and no missed beats.  Then we've skipped to the one with
      // zero for a poll time.

      cls->stats[cls->stat_id].recv_time = 0;
      cls->stats[cls->stat_id].active = false;
      cls->stat_id = sent_id;
      cls->stats[cls->stat_id].poll_time = 0;
    }

    cls->stats[cls->stat_id].recv_time = current_time();
    cls->stats[cls->stat_id].missed = cls->missed;
    cls->stats[cls->stat_id].active = false;

    cls->missed = 0;
    if (++cls->stat_id >= HEARTBEAT_HISTORY_SIZE)
      cls->stat_id = 0;

    cls->stats[cls->stat_id].recv_time = 0;
    cls->stats[cls->stat_id].missed = 0;
    cls->stats[cls->stat_id].poll_time = 0;
    cls->stats[cls->stat_id].active = false;

    got_beat = true;
  }

  now = current_time();

  if (unlikely(got_beat && cls->max_timer_cls != NULL)) {
    cls->init_duration = now - cls->init_duration;

    rwsched_tasklet_CFRunLoopTimerRelease(
        cls->rwmain->rwvx->rwsched_tasklet,
        cls->max_timer_cls->timer);
    free(cls->max_timer_cls);
    cls->max_timer_cls = NULL;
  }

  if (unlikely(now - cls->missed_histogram.started_at > HEARTBEAT_HISTOGRAM_DELTA)) {
    if (++cls->missed_histogram.index >= HEARTBEAT_HISTOGRAM_SIZE - 1)
      cls->missed_histogram.index = 0;
    cls->missed_histogram.started_at = now;
    cls->missed_histogram.histogram[cls->missed_histogram.index] = 0;
  }

  if (unlikely(!got_beat && !cls->max_timer_cls)) {
    cls->missed++;
    cls->missed_histogram.histogram[cls->missed_histogram.index]++;

    if (cls->missed >= cls->rwmain->rwproc_heartbeat->tolerance) {
      bool found;

      rwmain_trace_crit(
          cls->rwmain,
          "%u missed heartbeats from %s, assuming dead",
          cls->missed,
          cls->instance_name);

      raise_heartbeat_failure_notification(cls->rwmain->dts, cls->instance_name);     
      rwsched_tasklet_CFRunLoopTimerRelease(cls->rwmain->rwvx->rwsched_tasklet, timer);
      rw_component_info ci;
      kill_component(cls->rwmain, cls->instance_name, &ci);

      found = false;
      for (size_t i = 0; cls->rwmain->rwproc_heartbeat->subs[i]; ++i) {
        if (cls->rwmain->rwproc_heartbeat->subs[i] == cls)
          found = true;

        if (found)
          cls->rwmain->rwproc_heartbeat->subs[i] = cls->rwmain->rwproc_heartbeat->subs[i+1];
      }

      mq_close(cls->mqd);
      if (ci.has_recovery_action && (ci.recovery_action== RWVCS_TYPES_RECOVERY_TYPE_RESTART)) {
        if (ci.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWPROC){
          vcs_manifest_component *m_component = NULL;
          rw_status_t status = rwvcs_manifest_component_lookup(
              cls->rwmain->rwvx->rwvcs,
              ci.component_name,
              &m_component);
          RW_ASSERT (status == RW_STATUS_SUCCESS);
          char *instance_name = NULL;
          rwmain_trace_crit(
              cls->rwmain,
              "starting recovery of %s",
              cls->instance_name);

          start_component(cls->rwmain,
                          ci.component_name,
                          NULL,
                          RW_BASE_ADMIN_COMMAND_RECOVER,
                          ci.rwcomponent_parent,
                          &instance_name,
                          m_component);
        }
        free(cls->instance_name);
        free(cls);
      }
      else {
        free(cls->instance_name);
        free(cls);
        exit(-2);
      }
    }
  }
}

static void publish_mq_heartbeat(rwsched_CFRunLoopTimerRef timer, void * ctx)
{
  int r;
  struct publisher_cls  * cls;
  struct mq_attr attr;


  cls = (struct publisher_cls *)ctx;

  r = mq_getattr(cls->mqd, &attr);
  if (unlikely(r != 0)) {
    int e = errno;

    rwmain_trace_crit(
        cls->rwmain,
        "Failed to get mq attributes: %s",
        strerror(e));
    RW_CRASH();
  }

  if (unlikely(attr.mq_maxmsg == attr.mq_curmsgs)) {
    if (unlikely(!cls->subscriber_timeout && !getenv("RIFT_PROC_HEARTBEAT_NO_REVERSE"))) {
      rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };

      cf_context.info = cls;

      cls->subscriber_timeout = rwsched_tasklet_CFRunLoopTimerCreate(
          cls->rwmain->rwvx->rwsched_tasklet,
          kCFAllocatorDefault,
          CFAbsoluteTimeGetCurrent() + RWVCS_HEARTBEAT_DELAY,
          0,
          0,
          0,
          on_subscriber_timeout,
          &cf_context);

      rwsched_tasklet_CFRunLoopAddTimer(
          cls->rwmain->rwvx->rwsched_tasklet,
          rwsched_tasklet_CFRunLoopGetCurrent(cls->rwmain->rwvx->rwsched_tasklet),
          cls->subscriber_timeout,
          cls->rwmain->rwvx->rwsched->main_cfrunloop_mode);
    }
    return;
  }

  if (unlikely(cls->subscriber_timeout != NULL)) {
    rwsched_tasklet_CFRunLoopTimerRelease(cls->rwmain->rwvx->rwsched_tasklet, cls->subscriber_timeout);
    cls->subscriber_timeout = NULL;
  }

  if (!cls->skip_hb) {
    r = mq_send(cls->mqd, (char *)&cls->stat_id, sizeof(cls->stat_id), 0);
    if (unlikely(r && errno != EAGAIN)) {
      int e = errno;

      rwmain_trace_crit(
          cls->rwmain,
          "Failed to send heartbeat: %s",
          strerror(e));
      RW_CRASH();
    }

    cls->stats[cls->stat_id].send_time = current_time();
    cls->stat_id++;
    if (cls->stat_id >= HEARTBEAT_HISTORY_SIZE) cls->stat_id = 0;
  }
}

struct rwproc_heartbeat * rwproc_heartbeat_alloc(uint32_t frequency, uint32_t tolerance)
{
  struct rwproc_heartbeat * hb;

  hb = (struct rwproc_heartbeat *)malloc(sizeof(struct rwproc_heartbeat));
  if (!hb) {
    RW_CRASH();
    goto err;
  }

  hb->frequency = frequency;
  hb->tolerance = tolerance;
  hb->enabled = true;

  hb->subs = (struct subscriber_cls **)malloc(sizeof(struct subscriber_cls *));
  if (!hb->subs) {
    RW_CRASH();
    goto err;
  }
  hb->subs[0] = NULL;

  hb->pubs = (struct publisher_cls **)malloc(sizeof(struct publisher_cls *));
  if (!hb->pubs) {
    RW_CRASH();
    goto err;
  }
  hb->pubs[0] = NULL;

  goto done;

err:
  if (hb->subs)
    free(hb->subs);

  if (hb->pubs)
    free(hb->pubs);

  free(hb);
  hb = NULL;

done:
    return hb;

}

void rwproc_heartbeat_free(struct rwproc_heartbeat * rwproc_heartbeat)
{
  int r;
  char * path;

  for (size_t i = 0; rwproc_heartbeat->subs[i]; ++i) {
    struct subscriber_cls * cls = rwproc_heartbeat->subs[i];

    if (cls->max_timer_cls) {
     rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->max_timer_cls->timer);
     free(cls->max_timer_cls);
    }

    if (cls->timer)
      rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->timer);

    r = asprintf(&path, "/%s", cls->instance_name);
    if (r != -1) {
      mq_unlink(path);
      free(path);
    }

    free(cls->instance_name);
    mq_close(cls->mqd);
    free(cls->max_timer_cls);
    free(cls);
  }
  free(rwproc_heartbeat->subs);

  for (size_t i = 0; rwproc_heartbeat->pubs[i]; ++i) {
    struct publisher_cls * cls = rwproc_heartbeat->pubs[i];

    if (cls->timer)
      rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->timer);

    mq_close(cls->mqd);
    free(cls);
  }
  free(rwproc_heartbeat->pubs);

  free(rwproc_heartbeat);
}

rw_status_t rwproc_heartbeat_subscribe(
    rwmain_gi_t * rwmain,
    const char * instance_name)
{
  int r;
  mqd_t mqd = -1;
  struct mq_attr mq_attrs;
  mode_t mask;
  char * path = NULL;
  char * full_path = NULL;
  rw_status_t status;
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  struct subscriber_cls * cls = NULL;
  struct max_timer_cls * max_timer_cls = NULL;
  size_t subs_end;

  mq_attrs.mq_flags = O_NONBLOCK;
  mq_attrs.mq_maxmsg = 10;
  mq_attrs.mq_msgsize = HEARTBEAT_MSG_SIZE;

  r = asprintf(&path, "/%s", instance_name);
  RW_ASSERT(r != -1);

  r = asprintf(&full_path, "/dev/mqueue/%s", instance_name);
  RW_ASSERT(r != -1);

  cls = (struct subscriber_cls *)malloc(sizeof(struct subscriber_cls));
  RW_ASSERT(cls);
  if (!cls) {
    status = RW_STATUS_FAILURE;
    goto err;
  }
  bzero(cls, sizeof(struct subscriber_cls));

  max_timer_cls = (struct max_timer_cls *)malloc(sizeof(struct max_timer_cls));
  RW_ASSERT(max_timer_cls);
  if (!max_timer_cls) {
    status = RW_STATUS_FAILURE;
    goto err;
  }
  bzero(max_timer_cls, sizeof(struct max_timer_cls));

  // The process may not be running as the same user.
  mask = umask(0);
  mqd = mq_open(
      path,
      O_RDONLY|O_CREAT|O_NONBLOCK,
      S_IRWXU|S_IRWXG|S_IRWXO,
      &mq_attrs);
  umask(mask);
  if (mqd == -1) {
    int e = errno;

    rwmain_trace_crit(
        rwmain,
        "Failed to create mq %s: %s",
        path,
        strerror(e));
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto err;
  }

  r = reaper_client_add_path(rwmain->rwvx->rwvcs->reaper_sock, full_path);
  if (r) {
    rwmain_trace_crit(rwmain, "Failed to add %s to reaper", path);
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto err;
  }

  cls->rwmain = rwmain;
  cls->instance_name = strdup(instance_name);
  RW_ASSERT(cls->instance_name);
  cls->mqd = mqd;
  cls->max_timer_cls = max_timer_cls;
  cls->init_duration = current_time();
  cls->stat_id = 0;
  cf_context.info = cls;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + 1.0,
      1.0 / (double)rwmain->rwproc_heartbeat->frequency,
      0,
      0,
      check_mq_heartbeat,
      &cf_context);
  cls->timer = cftimer;

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);


  max_timer_cls->sub_cls = cls;
  cf_context.info = max_timer_cls;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + RWVCS_HEARTBEAT_DELAY,
      0,
      0,
      0,
      on_subscriber_delay_timeout,
      &cf_context);
  max_timer_cls->timer = cftimer;

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);

  for (subs_end = 0; rwmain->rwproc_heartbeat->subs[subs_end]; ++subs_end) {;}

  rwmain->rwproc_heartbeat->subs = (struct subscriber_cls **)realloc(
      rwmain->rwproc_heartbeat->subs,
      (subs_end + 2) * sizeof(struct subscriber_cls *));
  RW_ASSERT(rwmain->rwproc_heartbeat->subs);
  rwmain->rwproc_heartbeat->subs[subs_end] = cls;
  rwmain->rwproc_heartbeat->subs[subs_end + 1] = NULL;

  status = RW_STATUS_SUCCESS;
  goto done;

err:
  if (cls) {
    if (cls->instance_name)
      free(cls->instance_name);
    free(cls);
  }

  if (max_timer_cls)
    free(max_timer_cls);

  if (mqd != -1) {
    mq_close(mqd);
    mq_unlink(path);
  }

done:
  if (path)
    free(path);

  if (full_path)
    free(full_path);

  return status;
}

rw_status_t rwproc_heartbeat_publish(
    rwmain_gi_t * rwmain,
    const char * instance_name)
{
  int r;
  mqd_t mqd;
  char * path = NULL;
  rw_status_t status;
  rwsched_CFRunLoopTimerRef cftimer;
  rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
  struct publisher_cls * cls;
  size_t pubs_end;

  r = asprintf(&path, "/%s", instance_name);
  RW_ASSERT(r != -1);

  cls = (struct publisher_cls *)malloc(sizeof(struct publisher_cls));
  RW_ASSERT(cls);
  if (!cls) {
    status = RW_STATUS_FAILURE;
    goto done;
  }
  bzero(cls, sizeof(struct publisher_cls));

  mqd = mq_open(path, O_WRONLY|O_NONBLOCK, NULL);
  if (mqd == -1) {
    int e = errno;

    rwmain_trace_crit(
        rwmain,
        "Failed to open mq %s: %s",
        path,
        strerror(e));
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto done;
  }

  // There will only ever be two ends to this queue.  It was created by the
  // subscriber and now we have the other end.  We want to make sure the queue
  // does not persist after both ends are closed so we can unlink it now.  This
  // will remove the queue name and once both file descriptors to the queue are
  // closed, will destroy the queue.
  r = mq_unlink(path);
  if (r == -1) {
    int e = errno;
    rwmain_trace_crit(
        rwmain,
        "Failed to unlink mq %s: %s",
        path,
        strerror(e));
    RW_CRASH();
  }

  cls->rwmain = rwmain;
  cls->mqd = mqd;
  cls->stat_id = 0;
  cf_context.info = cls;

  cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
      rwmain->rwvx->rwsched_tasklet,
      kCFAllocatorDefault,
      CFAbsoluteTimeGetCurrent() + 0.1,
      1.0 / (double)rwmain->rwproc_heartbeat->frequency,
      0,
      0,
      publish_mq_heartbeat,
      &cf_context);
  cls->timer = cftimer;

  rwsched_tasklet_CFRunLoopAddTimer(
      rwmain->rwvx->rwsched_tasklet,
      rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
      cftimer,
      rwmain->rwvx->rwsched->main_cfrunloop_mode);

  for (pubs_end = 0; rwmain->rwproc_heartbeat->pubs[pubs_end]; ++pubs_end) {;}

  rwmain->rwproc_heartbeat->pubs = (struct publisher_cls **)realloc(
      rwmain->rwproc_heartbeat->pubs,
      (pubs_end + 2) * sizeof(struct publisher_cls *));
  RW_ASSERT(rwmain->rwproc_heartbeat->pubs);
  rwmain->rwproc_heartbeat->pubs[pubs_end] = cls;
  rwmain->rwproc_heartbeat->pubs[pubs_end + 1] = NULL;

  status = RW_STATUS_SUCCESS;

done:
  if (path)
    free(path);

  if (status != RW_STATUS_SUCCESS && cls)
    free(cls);

  return status;
}

rw_status_t rwproc_heartbeat_reset(
    rwmain_gi_t * rwmain,
    uint16_t frequency,
    uint32_t tolerance,
    bool enabled)
{
  struct rwproc_heartbeat * rwproc_heartbeat;

  rwproc_heartbeat = rwmain->rwproc_heartbeat;

  for (size_t i = 0; rwproc_heartbeat->subs[i]; ++i) {
    rwsched_CFRunLoopTimerRef cftimer;
    rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
    struct subscriber_cls * cls = rwproc_heartbeat->subs[i];

    if (cls->timer) {
      rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->timer);
      cls->timer = NULL;
    }

    if (!enabled) {
      if (cls->max_timer_cls) {
        rwsched_tasklet_CFRunLoopTimerRelease(
            cls->rwmain->rwvx->rwsched_tasklet,
            cls->max_timer_cls->timer);
        cls->max_timer_cls = NULL;
      }
      continue;
    }

    cf_context.info = cls;

    cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
        rwmain->rwvx->rwsched_tasklet,
        kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent(),
        1.0 / (double)frequency,
        0,
        0,
        check_mq_heartbeat,
        &cf_context);
    cls->timer = cftimer;

    rwsched_tasklet_CFRunLoopAddTimer(
        rwmain->rwvx->rwsched_tasklet,
        rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
        cftimer,
        rwmain->rwvx->rwsched->main_cfrunloop_mode);
  }

  for (size_t i = 0; rwproc_heartbeat->pubs[i]; ++i) {
    rwsched_CFRunLoopTimerRef cftimer;
    rwsched_CFRunLoopTimerContext cf_context = { 0, NULL, NULL, NULL, NULL };
    struct publisher_cls * cls = rwproc_heartbeat->pubs[i];

    if (cls->timer) {
      rwsched_tasklet_CFRunLoopTimerRelease(
          cls->rwmain->rwvx->rwsched_tasklet,
          cls->timer);
      cls->timer = NULL;
    }

    if (!enabled) {
      continue;
    }

    cf_context.info = cls;

    cftimer = rwsched_tasklet_CFRunLoopTimerCreate(
        rwmain->rwvx->rwsched_tasklet,
        kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent(),
        1.0 / (double)frequency,
        0,
        0,
        publish_mq_heartbeat,
        &cf_context);
    cls->timer = cftimer;

    rwsched_tasklet_CFRunLoopAddTimer(
        rwmain->rwvx->rwsched_tasklet,
        rwsched_tasklet_CFRunLoopGetCurrent(rwmain->rwvx->rwsched_tasklet),
        cftimer,
        rwmain->rwvx->rwsched->main_cfrunloop_mode);
  }

  rwproc_heartbeat->frequency = frequency;
  rwproc_heartbeat->tolerance = tolerance;
  rwproc_heartbeat->enabled = enabled;

  return RW_STATUS_SUCCESS;
}

void rwproc_heartbeat_settings(
    rwmain_gi_t * rwmain,
    uint16_t * frequency,
    uint32_t * tolerance,
    bool * enabled)
{
  *frequency = rwmain->rwproc_heartbeat->frequency;
  *tolerance = rwmain->rwproc_heartbeat->tolerance;
  *enabled = rwmain->rwproc_heartbeat->enabled;
}

rw_status_t rwproc_heartbeat_stats(
    rwmain_gi_t * rwmain,
    rwproc_heartbeat_stat *** stats,
    size_t * n_stats)
{
  rwproc_heartbeat_stat ** ret;
  struct rwproc_heartbeat * hb;
  size_t max_stats;
  size_t current_stat;


  *n_stats = 0;
  *stats = NULL;
  hb = rwmain->rwproc_heartbeat;

  for (max_stats = 0; hb->subs[max_stats]; ++max_stats) {;}
  for (size_t i = 0; hb->pubs[i]; ++max_stats, ++i) {;}

  if (max_stats == 0)
    return RW_STATUS_SUCCESS;

  ret = (rwproc_heartbeat_stat **)malloc(max_stats * sizeof(void *));
  if (!ret) {
    RW_CRASH();
    goto err;
  }
  bzero(ret, max_stats * sizeof(void *));

  for (size_t i = 0; i < max_stats; ++i) {
    ret[i] = (rwproc_heartbeat_stat *)malloc(sizeof(rwproc_heartbeat_stat));
    if (!ret[i]) {
      RW_CRASH();
      goto free_stats;
    }

    rwproc_heartbeat_stat__init(ret[i]);

    ret[i]->timing = (rwproc_heartbeat_timing **)malloc(HEARTBEAT_HISTORY_SIZE * sizeof(void *));
    if (!ret[i]->timing) {
      RW_CRASH();
      goto free_stats;
    }

    for (size_t j = 0; j < HEARTBEAT_HISTORY_SIZE; ++j) {
      ret[i]->timing[j] = (rwproc_heartbeat_timing *)malloc(sizeof(rwproc_heartbeat_timing));
      if (!ret[i]->timing[j]) {
        RW_CRASH();
        goto free_stats;
      }
      rwproc_heartbeat_timing__init(ret[i]->timing[j]);
    }
    ret[i]->n_timing = HEARTBEAT_HISTORY_SIZE;
  }

  for (current_stat = 0; hb->subs[current_stat]; ++current_stat) {
    struct subscriber_cls * cls = hb->subs[current_stat];
    rwproc_heartbeat_stat * stat = ret[current_stat];
    size_t index;
    size_t array_end;

    if (!cls->max_timer_cls) {
      stat->init_duration = cls->init_duration;
      stat->has_init_duration = true;
    }

    stat->instance_name = strdup(cls->instance_name);
    if (!stat->instance_name) {
      RW_CRASH();
      goto free_stats;
    }

    for (size_t j = 0; j < HEARTBEAT_HISTORY_SIZE; ++j) {
      stat->timing[j]->id = j;
      stat->timing[j]->has_id = 1;
      stat->timing[j]->has_recv_time = true;
      stat->timing[j]->has_missed = true;
      stat->timing[j]->recv_time = cls->stats[j].recv_time;
      stat->timing[j]->missed = cls->stats[j].missed;
    }

    stat->missed_histogram = (rwproc_heartbeat_histogram *)malloc(sizeof(rwproc_heartbeat_histogram));
    if (!stat->missed_histogram) {
      RW_CRASH();
      goto free_stats;
    }
    rwproc_heartbeat_histogram__init(stat->missed_histogram);
    stat->missed_histogram->has_interval_duration = true;
    stat->missed_histogram->interval_duration = HEARTBEAT_HISTOGRAM_DELTA;
    stat->missed_histogram->n_histogram = HEARTBEAT_HISTOGRAM_SIZE;
    stat->missed_histogram->histogram = (uint32_t *)malloc(HEARTBEAT_HISTOGRAM_SIZE  * sizeof(uint32_t));
    if (!stat->missed_histogram->histogram) {
      RW_CRASH();
      goto free_stats;
    }

    index = cls->missed_histogram.index;
    array_end = HEARTBEAT_HISTOGRAM_SIZE - 1;
    for (size_t j = 0; j + index < array_end; ++j)
      stat->missed_histogram->histogram[j] = cls->missed_histogram.histogram[j + index + 1];
    for (size_t j = 0; j <= index; j++)
      stat->missed_histogram->histogram[array_end - index + j] = cls->missed_histogram.histogram[j];
  }

  for (size_t i = 0; hb->pubs[i]; ++i) {
    struct publisher_cls * cls = hb->pubs[i];
    rwproc_heartbeat_stat * stat = ret[current_stat];

    stat->instance_name = to_instance_name(rwmain->component_name, rwmain->instance_id);
    if (!stat->instance_name) {
      RW_CRASH();
      goto free_stats;
    }

    for (size_t j = 0; j < HEARTBEAT_HISTORY_SIZE; ++j) {
      stat->timing[j]->id = j;
      stat->timing[j]->has_id = 1;
      stat->timing[j]->has_send_time = true;
      stat->timing[j]->send_time = cls->stats[j].send_time;
    }

    current_stat++;
  }

  *stats = ret;
  *n_stats = max_stats;

  return RW_STATUS_SUCCESS;


free_stats:
  for (size_t i = 0; ret[i]; ++i) {
    if (ret[i]->timing) {
      for (size_t j = 0; j < HEARTBEAT_HISTORY_SIZE && ret[i]->timing[j]; j++)
        protobuf_free(ret[i]->timing[j]);
    }

    free(ret[i]->timing);
    ret[i]->timing = NULL;
    ret[i]->n_timing = 0;
    protobuf_free(ret[i]);
  }

  free(ret);

err:
    return RW_STATUS_FAILURE;
}

rw_status_t rwproc_heartbeat_skip(
    rwmain_gi_t * rwmain)
{
  struct rwproc_heartbeat * hb = rwmain->rwproc_heartbeat;
   if (hb->pubs[0]) {
      hb->pubs[0]->skip_hb = true;
      return (RW_STATUS_SUCCESS);
  }
  return(RW_STATUS_FAILURE);
}

