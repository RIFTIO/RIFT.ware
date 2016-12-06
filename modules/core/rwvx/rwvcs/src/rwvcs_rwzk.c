
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
 * @file rwvcs_zk.c
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 07/07/2014
 * @brief RW.VCS zookeeper routines
 */

#include <strings.h>

#include <rwvcs_zk_api.h>

#include "rwvcs.h"
#include "rwvcs_component.h"
#include "rwvcs_internal.h"
#include <ck_pr.h>

static const char g_component_instance_sep = '-';
static const char * g_auto_instance_path = "/rwvcs-instance-id";

char * to_instance_name(const char * component_name, int instance_id)
{
  int r;
  char * ret;

  r = asprintf(&ret, "%s-%d", component_name, instance_id);
  RW_ASSERT(r != -1);

  return ret;
}

char * to_instance_path(const char * instance_name)
{
  char * sep;
  char * ret;

  ret = strdup(instance_name);
  RW_ASSERT(ret);

  sep = rindex(ret, g_component_instance_sep);
  RW_ASSERT(sep);

  *sep = '/';
  return ret;
}

char * split_component_name(const char * instance_name)
{
  char * ret;
  char * p;

  ret = strdup(instance_name);
  RW_ASSERT(ret);

  p = rindex(ret, g_component_instance_sep);
  if (!p)
    return NULL;

  *p = '\0';
  return ret;
}

int split_instance_id(const char * instance_name)
{
  char * p;
  long ret;

  p = rindex(instance_name, g_component_instance_sep);
  if (!p)
    return -1;

  errno = 0;
  ret = strtol(p+1, NULL, 10);
  RW_ASSERT(errno == 0);

  return (int)ret;
}


void rwvcs_rwzk_server_start_sync_f(void *ctx)
{
  rwvcs_rwzk_server_start_sync_t *rwvcs_rwzk_server_start_sync_p = 
      (rwvcs_rwzk_server_start_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_server_start_sync_p->rwvcs;
  uint32_t server_id = rwvcs_rwzk_server_start_sync_p->server_id;
  uint32_t client_port = rwvcs_rwzk_server_start_sync_p->client_port;
  bool unique_ports = rwvcs_rwzk_server_start_sync_p->unique_ports;
  rwvcs_zk_server_port_detail_ptr_t *server_details = rwvcs_rwzk_server_start_sync_p->server_details;
  const char *rift_var_root = rwvcs_rwzk_server_start_sync_p->rift_var_root;

  rw_status_t status = rwvcs_zk_create_server_config(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      server_id,
      client_port,
      unique_ports,
      server_details,
      rift_var_root);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  rwvcs_rwzk_server_start_sync_p->server_pid = rwvcs_zk_server_start(RWVX_GET_ZK_MODULE(rwvcs->rwvx), server_id);
  if (rwvcs_rwzk_server_start_sync_p->server_pid <= 0) {
    fprintf(stderr, "error starting rwvcs_zk_server_start() - server_pid=%d\n", rwvcs_rwzk_server_start_sync_p->server_pid);
  }
  RW_ASSERT(rwvcs_rwzk_server_start_sync_p->server_pid > 0);
}

int rwvcs_rwzk_server_start(
    rwvcs_instance_ptr_t rwvcs,
    uint32_t server_id,
    int client_port,
    bool unique_ports,
    rwvcs_zk_server_port_detail_ptr_t *server_details,
    const char *rift_var_root)
{
  rwvcs_rwzk_server_start_sync_t rwvcs_rwzk_server_start_sync = {
    .rwvcs = rwvcs,
    .server_id = server_id,
    .client_port = client_port,
    .unique_ports = unique_ports,
    .server_details = server_details,
    .rift_var_root = rift_var_root
  };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_server_start_sync,
      rwvcs_rwzk_server_start_sync_f);
  return rwvcs_rwzk_server_start_sync.server_pid;
}

void rwvcs_rwzk_client_init_sync_f(void *ctx)
{
  rwvcs_rwzk_client_init_sync_t *rwvcs_rwzk_client_init_sync_p = 
      (rwvcs_rwzk_client_init_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_client_init_sync_p->rwvcs;

  if (rwvcs->pb_rwmanifest->bootstrap_phase->zookeeper->zake) {
    rwvcs_rwzk_client_init_sync_p->status = rwvcs_zk_zake_init(RWVX_GET_ZK_MODULE(rwvcs->rwvx));
  } else {
    rwvcs_rwzk_client_init_sync_p->status = rwvcs_zk_kazoo_init(
        RWVX_GET_ZK_MODULE(rwvcs->rwvx),
        rwvcs->mgmt_info.zk_server_port_details);
  }
  RW_ASSERT(rwvcs_rwzk_client_init_sync_p->status == RW_STATUS_SUCCESS);
}

rw_status_t rwvcs_rwzk_client_init(rwvcs_instance_ptr_t rwvcs)
{
  rwvcs_rwzk_client_init_sync_t rwvcs_rwzk_client_init_sync = {
    .rwvcs = rwvcs,
  };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_client_init_sync,
      rwvcs_rwzk_client_init_sync_f);
  return rwvcs_rwzk_client_init_sync.status;
}

void rwvcs_rwzk_client_start_sync_f(void *ctx)
{
  rwvcs_rwzk_client_start_sync_t *rwvcs_rwzk_client_start_sync_p = 
      (rwvcs_rwzk_client_start_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_client_start_sync_p->rwvcs;

  RW_ASSERT(!rwvcs->pb_rwmanifest->bootstrap_phase->zookeeper->zake);
  rwvcs_rwzk_client_start_sync_p->status = rwvcs_zk_kazoo_start(RWVX_GET_ZK_MODULE(rwvcs->rwvx));

  RW_ASSERT(rwvcs_rwzk_client_start_sync_p->status == RW_STATUS_SUCCESS);
}

rw_status_t rwvcs_rwzk_client_start(rwvcs_instance_ptr_t rwvcs)
{
  rwvcs_rwzk_client_start_sync_t rwvcs_rwzk_client_start_sync = {
    .rwvcs = rwvcs,
  };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_client_start_sync,
      rwvcs_rwzk_client_start_sync_f);
  return rwvcs_rwzk_client_start_sync.status;
}

void rwvcs_rwzk_client_stop_sync_f(void *ctx)
{
  rwvcs_rwzk_client_stop_sync_t *rwvcs_rwzk_client_stop_sync_p = 
      (rwvcs_rwzk_client_stop_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_client_stop_sync_p->rwvcs;

  RW_ASSERT(!rwvcs->pb_rwmanifest->bootstrap_phase->zookeeper->zake);
  rwvcs_rwzk_client_stop_sync_p->status = rwvcs_zk_kazoo_stop(RWVX_GET_ZK_MODULE(rwvcs->rwvx));

  RW_ASSERT(rwvcs_rwzk_client_stop_sync_p->status == RW_STATUS_SUCCESS);
}

rw_status_t rwvcs_rwzk_client_stop(rwvcs_instance_ptr_t rwvcs)
{
  rwvcs_rwzk_client_stop_sync_t rwvcs_rwzk_client_stop_sync = {
    .rwvcs = rwvcs,
  };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_client_stop_sync,
      rwvcs_rwzk_client_stop_sync_f);
  return rwvcs_rwzk_client_stop_sync.status;
}

void rwvcs_rwzk_seed_auto_instance_sync_f(void *ctx)
{
  rwvcs_rwzk_seed_auto_instance_sync_t *rwvcs_rwzk_seed_auto_instance_sync_p = 
      (rwvcs_rwzk_seed_auto_instance_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_seed_auto_instance_sync_p->rwvcs;
  uint32_t start = rwvcs_rwzk_seed_auto_instance_sync_p->start;
  const char *path = rwvcs_rwzk_seed_auto_instance_sync_p->path;

  bool exists;
  const char *instance_path = path ? path : g_auto_instance_path;

  exists = rwvcs_zk_exists(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path);
  if (!exists) {
    char * data = NULL;
    struct timeval tv = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };
    int r;

    rwvcs_rwzk_seed_auto_instance_sync_p->status = rwvcs_zk_create(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, NULL);
    RW_ASSERT(rwvcs_rwzk_seed_auto_instance_sync_p->status == RW_STATUS_SUCCESS);

    rwvcs_rwzk_seed_auto_instance_sync_p->status = rwvcs_zk_lock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, &tv);
    RW_ASSERT(rwvcs_rwzk_seed_auto_instance_sync_p->status == RW_STATUS_SUCCESS);

    r = asprintf(&data, "%u", start);
    RW_ASSERT(r != -1);

    rwvcs_rwzk_seed_auto_instance_sync_p->status = rwvcs_zk_set(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, data, NULL);
    RW_ASSERT(rwvcs_rwzk_seed_auto_instance_sync_p->status == RW_STATUS_SUCCESS);

    rwvcs_rwzk_seed_auto_instance_sync_p->status = rwvcs_zk_unlock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path);
    RW_ASSERT(rwvcs_rwzk_seed_auto_instance_sync_p->status == RW_STATUS_SUCCESS);
    RW_FREE(data);
  }
}

rw_status_t
rwvcs_rwzk_seed_auto_instance(rwvcs_instance_ptr_t rwvcs, uint32_t start, const char *path)
{
  rwvcs_rwzk_seed_auto_instance_sync_t rwvcs_rwzk_seed_auto_instance_sync = {
    .rwvcs = rwvcs,
    .start = start,
    .path = path};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_seed_auto_instance_sync,
      rwvcs_rwzk_seed_auto_instance_sync_f);
  return rwvcs_rwzk_seed_auto_instance_sync.status;
}

void rwvcs_rwzk_lock_sync_f(void *ctx)
{
  rwvcs_rwzk_lock_sync_t *rwvcs_rwzk_lock_sync_p = 
      (rwvcs_rwzk_lock_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_lock_sync_p->rwvcs;
  const char * id = rwvcs_rwzk_lock_sync_p->id;
  struct timeval * timeout = rwvcs_rwzk_lock_sync_p->timeout;
  int r;
  char *path;

  r = asprintf(&path, "/rwvcs/%s", id);
  RW_ASSERT(r != -1);

  rwvcs_rwzk_lock_sync_p->status = rwvcs_zk_lock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, timeout);
  free(path);

  return;
}

rw_status_t
rwvcs_rwzk_lock(
    rwvcs_instance_ptr_t rwvcs, 
    const char * id, 
    struct timeval * timeout)
{
  rwvcs_rwzk_lock_sync_t rwvcs_rwzk_lock_sync = {
    .rwvcs = rwvcs,
    .id = id,
    .timeout = timeout};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_lock_sync,
      rwvcs_rwzk_lock_sync_f);
  return rwvcs_rwzk_lock_sync.status;
}

rw_status_t
rwvcs_rwzk_lock_internal(
    rwvcs_instance_ptr_t rwvcs, 
    const char * id, 
    struct timeval * timeout)
{
  rwvcs_rwzk_lock_sync_t rwvcs_rwzk_lock_sync = {
    .rwvcs = rwvcs,
    .id = id,
    .timeout = timeout};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      NULL,
      &rwvcs_rwzk_lock_sync,
      rwvcs_rwzk_lock_sync_f);
  return rwvcs_rwzk_lock_sync.status;
}

static void rwvcs_rwzk_unlock_sync_f(void *ctx)
{
  rwvcs_rwzk_unlock_sync_t *rwvcs_rwzk_unlock_sync_p = 
      (rwvcs_rwzk_unlock_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_unlock_sync_p->rwvcs;
  char * id = (char *)rwvcs_rwzk_unlock_sync_p->id;
  int r;
  char *path;

  r = asprintf(&path, "/rwvcs/%s", id);
  RW_ASSERT(r != -1);

  rwvcs_rwzk_unlock_sync_p->status = rwvcs_zk_unlock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path);
  free(path);
  if (rwvcs_rwzk_unlock_sync_p->status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to unlock rwzk node %s",
        path);
  }

  return;
}

rw_status_t
rwvcs_rwzk_unlock(rwvcs_instance_ptr_t rwvcs, const char * id)
{
  rwvcs_rwzk_unlock_sync_t rwvcs_rwzk_unlock_sync = {
    .rwvcs = rwvcs,
    .id = id};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_unlock_sync,
      rwvcs_rwzk_unlock_sync_f);
  return rwvcs_rwzk_unlock_sync.status;
}

rw_status_t
rwvcs_rwzk_unlock_internal(rwvcs_instance_ptr_t rwvcs, const char * id)
{
  rwvcs_rwzk_unlock_sync_t rwvcs_rwzk_unlock_sync = {
    .rwvcs = rwvcs,
    .id = id};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      NULL,
      &rwvcs_rwzk_unlock_sync,
      rwvcs_rwzk_unlock_sync_f);
  return rwvcs_rwzk_unlock_sync.status;
}

void rwvcs_rwzk_node_exists_sync_f(void *ctx)
{
  rwvcs_rwzk_node_exists_sync_t *rwvcs_rwzk_node_exists_sync_p = 
      (rwvcs_rwzk_node_exists_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_node_exists_sync_p->rwvcs;
  const char * id = rwvcs_rwzk_node_exists_sync_p->id;
  int r;
  char * path;

  r = asprintf(&path, "/rwvcs/%s", id);
  RW_ASSERT(r != -1);

  rwvcs_rwzk_node_exists_sync_p->ret = rwvcs_zk_exists(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path);
  free(path);

  return;
}

bool
rwvcs_rwzk_node_exists(
    rwvcs_instance_ptr_t rwvcs,
    const char * id)
{
  rwvcs_rwzk_node_exists_sync_t rwvcs_rwzk_node_exists_sync = {
    .rwvcs = rwvcs,
    .id = id};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_node_exists_sync,
      rwvcs_rwzk_node_exists_sync_f);
  return rwvcs_rwzk_node_exists_sync.ret;
}

static rw_status_t
rwvcs_rwzk_node_create_by_name(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name)
{
  rw_status_t status;
  char * path = NULL;
  int r;

  r = asprintf(&path, "/rwvcs/%s", instance_name);
  RW_ASSERT(r != -1);

  status = rwvcs_zk_create(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, NULL);
  free(path);

  return status;
}

static void rwvcs_rwzk_node_create_sync_f(void *ctx)
{
  rwvcs_rwzk_node_create_sync_t *rwvcs_rwzk_node_create_sync_p = 
      (rwvcs_rwzk_node_create_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_node_create_sync_p->rwvcs;
  const char *instance_name = rwvcs_rwzk_node_create_sync_p->instance_name;
  rwvcs_rwzk_node_create_sync_p->status = rwvcs_rwzk_node_create_by_name(rwvcs, instance_name);
  return;
}

rw_status_t
rwvcs_rwzk_node_create(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * component)
{
  rwvcs_rwzk_node_create_sync_t rwvcs_rwzk_node_create_sync = {
    .rwvcs = rwvcs,
    .instance_name = component->instance_name};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_node_create_sync,
      rwvcs_rwzk_node_create_sync_f);
  return rwvcs_rwzk_node_create_sync.status;
}

/*
 * Query the zookeeper for node data.  If the data is not found, data will be
 * set to NULL and data_len will be 0.
 *
 * @param rwvcs         - rwvcs instance
 * @param instance_name - instance for which to pull data
 * @param data          - on success, filled with data from the zookeeper
 * @param data_len      - on success, lenth of the data buffer
 * @return              - RW_STATUS_SUCCESS assuming nothing fatal happens.  This includes the
 *                        case where there is no node data for the given instance.
 *
 * NOTE:  Once RIFT-2032 is resolved, the return value for the not found case
 * will correctly be RW_STATUS_NOTFOUND.
 */
static rw_status_t
rwvcs_rwzk_get_component_node_data(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char ** data,
    int * data_len)
{
  int r;
  rw_status_t status;
  char * path;

  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  r = asprintf(&path, "/rwvcs/%s", instance_name);
  RW_ASSERT(r != -1);

  status = rwvcs_zk_get(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, data, NULL);
  free(path);

  if (status == RW_STATUS_SUCCESS && *data)
    *data_len = strlen(*data);
  else
    *data_len = 0;

  return status;
}

static void rwvcs_rwzk_node_update_f(void *ud)
{
  rwvcs_instance_ptr_t rwvcs = ((rwvcs_rwzk_node_update_t *)ud)->rwvcs;
  rw_component_info * id = ((rwvcs_rwzk_node_update_t *)ud)->id;
  bool skip_publish = ((rwvcs_rwzk_node_update_t *)ud)->skip_publish;
  RW_FREE_TYPE(((rwvcs_rwzk_node_update_t*)ud), rwvcs_rwzk_node_update_t);

  rw_status_t status;
  rw_status_t unlock_status;
  int r;
  char * path;
  CFMutableStringRef cfstring;
  const char * c_string;
  struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };

  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  cfstring = CFStringCreateMutable(NULL, 0);
  RW_ASSERT(cfstring);

  r = asprintf(&path, "/rwvcs/%s", id->instance_name);
  RW_ASSERT(r != -1);

  rw_xml_manager_pb_to_xml_cfstring (rwvcs->xml_mgr, id, cfstring);
  c_string = CFStringGetCStringPtr(cfstring, CFStringGetSystemEncoding());

  status = rwvcs_zk_lock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, &timeout);
  if (status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to lock rwzk node %s",
        path);
    goto done;
  }

  status = rwvcs_zk_set(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, c_string, NULL);

  unlock_status = rwvcs_zk_unlock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path);
  if (unlock_status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to unlock rwzk node %s",
        path);
  }
  if (rwvcs->config_ready_fn && !skip_publish) {
    rw_status_t config_ready_status = rwvcs->config_ready_fn(rwvcs, id);
    RW_ASSERT(config_ready_status == RW_STATUS_SUCCESS);
  }

done:
    RWTRACE_INFO(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Performing node update for %s:%d from %s :skip_publish %d",
        path, id->state, rwvcs->instance_name, skip_publish);
  CFRelease(cfstring);
  RW_FREE(path);

  protobuf_c_message_free_unpacked(NULL, &id->base);
}

rw_status_t
rwvcs_rwzk_node_update(
    rwvcs_instance_ptr_t rwvcs, 
    rw_component_info * id)
{
  rwvcs_rwzk_node_update_t *rwvcs_rwzk_node_update_p = 
      RW_MALLOC0_TYPE(sizeof(rwvcs_rwzk_node_update_t), rwvcs_rwzk_node_update_t);
  RW_ASSERT_TYPE(rwvcs_rwzk_node_update_p, rwvcs_rwzk_node_update_t);
  rwvcs_rwzk_node_update_p->rwvcs = rwvcs;
  rwvcs_rwzk_node_update_p->id = (rw_component_info*)protobuf_c_message_duplicate(NULL, (const ProtobufCMessage *)id, id->base.descriptor);
  rwvcs_rwzk_node_update_p->skip_publish = ck_pr_load_int(&rwvcs->restart_inprogress);

  RWVCS_RWZK_DISPATCH(
      async, 
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      rwvcs_rwzk_node_update_p, 
      rwvcs_rwzk_node_update_f);
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwvcs_rwzk_node_update_internal(
    rwvcs_instance_ptr_t rwvcs, 
    rw_component_info * id)
{
  rwvcs_rwzk_node_update_t *rwvcs_rwzk_node_update_p = 
      RW_MALLOC0_TYPE(sizeof(rwvcs_rwzk_node_update_t), rwvcs_rwzk_node_update_t);
  RW_ASSERT_TYPE(rwvcs_rwzk_node_update_p, rwvcs_rwzk_node_update_t);
  rwvcs_rwzk_node_update_p->rwvcs = rwvcs;
  rwvcs_rwzk_node_update_p->id = (rw_component_info*)protobuf_c_message_duplicate(NULL, (const ProtobufCMessage *)id, id->base.descriptor);

  RWVCS_RWZK_DISPATCH(
      sync, 
      rwvcs->rwvx->rwsched_tasklet, 
      NULL,
      rwvcs_rwzk_node_update_p, 
      rwvcs_rwzk_node_update_f);
  return RW_STATUS_SUCCESS;
}

static void rwvcs_rwzk_lookup_component_sync_f(void *ctx)
{
  rwvcs_rwzk_lookup_sync_t* rwvcs_rwzk_lookup_sync_p = 
      (rwvcs_rwzk_lookup_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_lookup_sync_p->rwvcs;
  const char * instance_id = rwvcs_rwzk_lookup_sync_p->instance_id;
  rw_component_info * pb_component = rwvcs_rwzk_lookup_sync_p->pb_component;
  char * xml_str;
  int xml_len;

  rwvcs_rwzk_lookup_sync_p->status = rwvcs_rwzk_get_component_node_data(rwvcs, instance_id, &xml_str, &xml_len);
  if (!xml_len) {
    rwvcs_rwzk_lookup_sync_p->status = RW_STATUS_NOTFOUND;
    return;
  }


  {
    // workaround http://jira.riftio.com:8080/browse/RIFT-1962

    while (true) {
      const char * needle  = "vm_ip_address";
      const char * replace = "vm-ip-address";
      char * begin = NULL;

      begin = strstr(xml_str, needle);
      if (!begin)
        break;

      for (int i = 0; i < strlen(replace); i++)
        begin[i] = replace[i];
    }

    while (true) {
      const char * needle  = "RWVCS_TYPES_COMPONENT_TYPE_";
      char * begin = NULL;

      begin = strstr(xml_str, needle);
      if (!begin)
        break;

      while(*(begin + strlen(needle)) != '\0') {
        *begin = *(begin + strlen(needle));
        begin++;
      }
      *begin = '\0';
    }

    while (true) {
      const char * needle  = "RW_BASE_STATE_TYPE_";
      char * begin = NULL;

      begin = strstr(xml_str, needle);
      if (!begin)
        break;

      while(*(begin + strlen(needle)) != '\0') {
        *begin = *(begin + strlen(needle));
        begin++;
      }
      *begin = '\0';
    }

  }

  rw_component_info__init(pb_component);
  rwvcs_rwzk_lookup_sync_p->status = rw_xml_manager_xml_to_pb(rwvcs->xml_mgr, xml_str, &pb_component->base, NULL);
  RW_ASSERT(rwvcs_rwzk_lookup_sync_p->status == RW_STATUS_SUCCESS);
  free(xml_str);

  return;
}

rw_status_t
rwvcs_rwzk_lookup_component(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_id,
    rw_component_info * pb_component)
{
  rwvcs_rwzk_lookup_sync_t rwvcs_rwzk_lookup_sync = {
      .rwvcs = rwvcs, 
      .instance_id = instance_id,
      .pb_component = pb_component};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_lookup_sync,
      rwvcs_rwzk_lookup_component_sync_f);
  return rwvcs_rwzk_lookup_sync.status;
}

rw_status_t
rwvcs_rwzk_lookup_component_internal(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_id,
    rw_component_info * pb_component)
{
 
  rwvcs_rwzk_lookup_sync_t rwvcs_rwzk_lookup_sync = {
      .rwvcs = rwvcs, 
      .instance_id = instance_id,
      .pb_component = pb_component};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      NULL, 
      &rwvcs_rwzk_lookup_sync,
      rwvcs_rwzk_lookup_component_sync_f);
  return rwvcs_rwzk_lookup_sync.status;
}

static void rwvcs_rwzk_delete_node_sync_f(void *ctx)
{
  rwvcs_rwzk_delete_node_sync_t *rwvcs_rwzk_delete_node_sync_p =
      (rwvcs_rwzk_delete_node_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_delete_node_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_delete_node_sync_p->instance_name;
  int r;
  char * path;

  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  r = asprintf(&path, "/rwvcs/%s", instance_name);
  RW_ASSERT(r != -1);

  rwvcs_rwzk_delete_node_sync_p->status = rwvcs_zk_delete(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, NULL);
  free(path);
  return;
}

rw_status_t
rwvcs_rwzk_delete_node(rwvcs_instance_ptr_t rwvcs, const char * instance_name)
{
  rwvcs_rwzk_delete_node_sync_t rwvcs_rwzk_delete_node_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_delete_node_sync,
      rwvcs_rwzk_delete_node_sync_f);
  return rwvcs_rwzk_delete_node_sync.status;
}

static void rwvcs_rwzk_update_parent_sync_f (void *ctx)
{
  rwvcs_rwzk_update_parent_sync_t *rwvcs_rwzk_update_parent_sync_p = 
      (rwvcs_rwzk_update_parent_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_update_parent_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_update_parent_sync_p->instance_name;
  const char * new_parent = rwvcs_rwzk_update_parent_sync_p->new_parent;
  rw_status_t unlock_status;
  rw_component_info info;
  rw_component_info old_parent_info;
  rw_component_info new_parent_info;
  char * old_parent = NULL;
  struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };

  rw_component_info__init(&info);
  rw_component_info__init(&old_parent_info);
  rw_component_info__init(&new_parent_info);

  /*
   * Get the information and lock the child, and the new parent.  If the child
   * previously had a parent, lock it and grab it's information as well.
   */
  rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, instance_name, &timeout);
  if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
    goto done;

  rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &info);
  if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
    goto unlock_info;

  if (info.rwcomponent_parent) {
    old_parent = strdup(info.rwcomponent_parent);
    RW_ASSERT(old_parent);
  }

  if (new_parent) {
    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, new_parent, &timeout);
    if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
      goto unlock_info;

    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, new_parent, &new_parent_info);
    if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
      goto unlock_new_parent;
  }

  if (old_parent) {
    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, info.rwcomponent_parent, &timeout);
    if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
      goto unlock_old_parent;

    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, info.rwcomponent_parent, &old_parent_info);
    if (rwvcs_rwzk_update_parent_sync_p->status != RW_STATUS_SUCCESS)
      goto unlock_old_parent;
  }

  /*
   * We have all the component's information and have locked them.  Time to update.
   * Note that the locks are reentrant so it's safe to call the normal add/remove
   * child functions here.
   */
  if (old_parent) {
    rwvcs_component_delete_child(&old_parent_info, instance_name);
    free(info.rwcomponent_parent);
  }

  if (new_parent) {
    rwvcs_component_add_child(&new_parent_info, instance_name);
    info.rwcomponent_parent = strdup(new_parent);
    RW_ASSERT(info.rwcomponent_parent);
  } else {
    info.rwcomponent_parent = NULL;
  }

  /*
   * Update the zookeeper.  If any of these fail, we're just bailing out.
   */
  rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &info);
  RW_ASSERT(rwvcs_rwzk_update_parent_sync_p->status == RW_STATUS_SUCCESS);

  if (new_parent) {
    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &new_parent_info);
    RW_ASSERT(rwvcs_rwzk_update_parent_sync_p->status == RW_STATUS_SUCCESS);
  }

  if (old_parent) {
    rwvcs_rwzk_update_parent_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &old_parent_info);
    RW_ASSERT(rwvcs_rwzk_update_parent_sync_p->status == RW_STATUS_SUCCESS);
  }

unlock_old_parent:
  if (old_parent) {
    unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, old_parent);
    RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);
  }

unlock_new_parent:
  if (new_parent) {
    unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, new_parent);
    RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);
  }

unlock_info:
  unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, instance_name);
  RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);

done:
  protobuf_free_stack(info);
  protobuf_free_stack(old_parent_info);
  protobuf_free_stack(new_parent_info);

  return;
}

rw_status_t
rwvcs_rwzk_update_parent(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * new_parent)
{
  rwvcs_rwzk_update_parent_sync_t rwvcs_rwzk_update_parent_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .new_parent = new_parent};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_update_parent_sync,
      rwvcs_rwzk_update_parent_sync_f);
  return rwvcs_rwzk_update_parent_sync.status;
}

static void rwvcs_rwzk_delete_child_sync_f(void *ctx)
{
  rwvcs_rwzk_delete_child_sync_t *rwvcs_rwzk_delete_child_sync_p = 
      (rwvcs_rwzk_delete_child_sync_t*)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_delete_child_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_delete_child_sync_p->instance_name;
  const char * child_name = rwvcs_rwzk_delete_child_sync_p->child_name;
  int child_id = rwvcs_rwzk_delete_child_sync_p->child_id;

  rwvcs_rwzk_delete_child_sync_p->status = RW_STATUS_SUCCESS;
  rw_status_t unlock_status;
  char * child_instance = NULL;
  rw_component_info parent;
  struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };

  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);

  rwvcs_rwzk_delete_child_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, instance_name, &timeout);
  if (rwvcs_rwzk_delete_child_sync_p->status != RW_STATUS_SUCCESS)
    return;

  rwvcs_rwzk_delete_child_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &parent);
  if (rwvcs_rwzk_delete_child_sync_p->status != RW_STATUS_SUCCESS)
    goto unlock;

  child_instance = to_instance_name(child_name, child_id);
  rwvcs_component_delete_child(&parent, child_instance);

  rwvcs_rwzk_delete_child_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &parent);

  free(child_instance);
  protobuf_c_message_free_unpacked_usebody(NULL, &parent.base);

unlock:
  unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, instance_name);
  RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);

  if (rwvcs_rwzk_delete_child_sync_p->status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to delete child %s from %s, %d",
        instance_name,
        child_name,
        rwvcs_rwzk_delete_child_sync_p->status);
  }
  return;
}

rw_status_t
rwvcs_rwzk_delete_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * child_name,
    int child_id)
{
  rwvcs_rwzk_delete_child_sync_t rwvcs_rwzk_delete_child_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .child_name = child_name,
    .child_id = child_id};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_delete_child_sync,
      rwvcs_rwzk_delete_child_sync_f);
  return rwvcs_rwzk_delete_child_sync.status;
}

static void rwvcs_rwzk_add_child_sync_f(void *ctx)
{
  rwvcs_rwzk_add_child_sync_t *rwvcs_rwzk_add_child_sync_p = 
      (rwvcs_rwzk_add_child_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_add_child_sync_p->rwvcs;
  const char * parent_instance = rwvcs_rwzk_add_child_sync_p->parent_instance;
  const char * child_instance = rwvcs_rwzk_add_child_sync_p->child_instance;
  rw_status_t unlock_status;
  rw_component_info parent;
  struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };

  rwvcs_rwzk_add_child_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, parent_instance, &timeout);
  if (rwvcs_rwzk_add_child_sync_p->status != RW_STATUS_SUCCESS)
    return;

  rwvcs_rwzk_add_child_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, parent_instance, &parent);
  if (rwvcs_rwzk_add_child_sync_p->status != RW_STATUS_SUCCESS)
    goto unlock;

  rwvcs_component_add_child(&parent, child_instance);

  rwvcs_rwzk_add_child_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &parent);

  protobuf_c_message_free_unpacked_usebody(NULL, &parent.base);

unlock:
  unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, parent_instance);
  RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);

  if (rwvcs_rwzk_add_child_sync_p->status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(
        rwvcs->rwvx->rwtrace,
        RWTRACE_CATEGORY_RWVCS,
        "Failed to add child %s to %s, %d",
        child_instance,
        parent_instance,
        rwvcs_rwzk_add_child_sync_p->status);
  }

  return;
}

rw_status_t
rwvcs_rwzk_add_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * parent_instance,
    const char * child_instance)
{
  rwvcs_rwzk_add_child_sync_t rwvcs_rwzk_add_child_sync = {
    .rwvcs = rwvcs,
    .parent_instance = parent_instance,
    .child_instance = child_instance};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_add_child_sync,
      rwvcs_rwzk_add_child_sync_f);
  return rwvcs_rwzk_add_child_sync.status;
}


void rwvcs_rwzk_update_state_f(void *ud)
{
  rwvcs_instance_ptr_t rwvcs = ((rwvcs_rwzk_update_state_t *)ud)->rwvcs;
  char * instance_name = (char *)((rwvcs_rwzk_update_state_t *)ud)->instance_name;
  rw_component_state state = ((rwvcs_rwzk_update_state_t *)ud)->state;
  RW_FREE_TYPE(((rwvcs_rwzk_update_state_t *)ud), rwvcs_rwzk_update_state_t);

  rw_status_t status;
  rw_component_info component;
  struct timeval timeout = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };

  status = rwvcs_rwzk_lock_internal(rwvcs, instance_name, &timeout);
  if (status != RW_STATUS_SUCCESS){
    RWTRACE_CRIT(rwvcs->rwvx->rwtrace,
                 RWTRACE_CATEGORY_RWVCS,
                 "locking failed %s for setting state %d",
                 instance_name, state);
    RW_FREE(instance_name);
    return;
  }

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &component);
  if (status != RW_STATUS_SUCCESS) {
    RWTRACE_CRIT(rwvcs->rwvx->rwtrace,
                 RWTRACE_CATEGORY_RWVCS,
                 "lookup_component failed %s for setting state %d",
                 instance_name, state);
    goto unlock;
  }

  if (component.state != state) {
    component.state = state;
    status = rwvcs_rwzk_node_update_internal(rwvcs, &component);
  }
    RWTRACE_INFO(rwvcs->rwvx->rwtrace,
                 RWTRACE_CATEGORY_RWVCS,
                 "update_state_f %s for setting state %d",
                 instance_name, state);
  protobuf_free_stack(component);

unlock:
  rwvcs_rwzk_unlock_internal(rwvcs, instance_name);
  RW_FREE(instance_name);
}


rw_status_t rwvcs_rwzk_update_state(rwvcs_instance_ptr_t rwvcs,
                                    const char * instance_name,
                                    rw_component_state state)
{
  rwvcs_rwzk_update_state_t *rwvcs_rwzk_update_state_p =
      RW_MALLOC0_TYPE(sizeof(rwvcs_rwzk_update_state_t), rwvcs_rwzk_update_state_t);
  RW_ASSERT_TYPE(rwvcs_rwzk_update_state_p, rwvcs_rwzk_update_state_t);
  rwvcs_rwzk_update_state_p->rwvcs = rwvcs;
  rwvcs_rwzk_update_state_p->instance_name = RW_STRDUP(instance_name);
  rwvcs_rwzk_update_state_p->state = state;

  RWVCS_RWZK_DISPATCH(
      async, 
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      rwvcs_rwzk_update_state_p, 
      rwvcs_rwzk_update_state_f);
  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwvcs_rwzk_get_leader(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char ** leader_id)
{
  rw_status_t status;
  rw_component_info collection;

  *leader_id = NULL;

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &collection);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS)
    return status;


  for (int i = 0; i < collection.n_rwcomponent_children; ++i) {
    rw_component_info child;

    status = rwvcs_rwzk_lookup_component_internal(rwvcs, collection.rwcomponent_children[i], &child);
    RW_ASSERT(status == RW_STATUS_SUCCESS);
    if (status != RW_STATUS_SUCCESS)
      goto free_collection;

    if (child.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM
        && child.vm_info->has_leader
        && child.vm_info->leader) {
      *leader_id = strdup(child.instance_name);
      RW_ASSERT(*leader_id);

      protobuf_free_stack(child);
      goto free_collection;
    }

    protobuf_free_stack(child);
  }

  status = RW_STATUS_NOTFOUND;


free_collection:
  protobuf_free_stack(collection);

  if (*leader_id)
    status = RW_STATUS_SUCCESS;

  return status;
}

static void rwvcs_rwzk_nearest_leader_sync_f(void *ctx)
{
  rwvcs_rwzk_nearest_leader_sync_t *rwvcs_rwzk_nearest_leader_sync_p = 
      (rwvcs_rwzk_nearest_leader_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_nearest_leader_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_nearest_leader_sync_p->instance_name;
  char ** leader_id = rwvcs_rwzk_nearest_leader_sync_p->leader_id;
  char * current_instance;
  rw_component_info component;

  current_instance = strdup(instance_name);
  RW_ASSERT(current_instance);

  while (true) {
    rwvcs_rwzk_nearest_leader_sync_p->status = 
        rwvcs_rwzk_lookup_component_internal(rwvcs, current_instance, &component);
    RW_ASSERT(rwvcs_rwzk_nearest_leader_sync_p->status == RW_STATUS_SUCCESS || 
              rwvcs_rwzk_nearest_leader_sync_p->status == RW_STATUS_NOTFOUND);

    free(current_instance);
    if (rwvcs_rwzk_nearest_leader_sync_p->status == RW_STATUS_NOTFOUND) {
      break;
    }

    rwvcs_rwzk_nearest_leader_sync_p->status =
        rwvcs_rwzk_get_leader(rwvcs, component.instance_name, leader_id);

    if (rwvcs_rwzk_nearest_leader_sync_p->status == RW_STATUS_SUCCESS) {
      protobuf_c_message_free_unpacked_usebody(NULL, &component.base);
      break;
    }
    RW_ASSERT(component.rwcomponent_parent);
    current_instance = strdup(component.rwcomponent_parent);
    RW_ASSERT(current_instance);
    protobuf_c_message_free_unpacked_usebody(NULL, &component.base);
  }

  return;
}
rw_status_t
rwvcs_rwzk_nearest_leader(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char ** leader_id)
{
  rwvcs_rwzk_nearest_leader_sync_t rwvcs_rwzk_nearest_leader_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .leader_id = leader_id};
  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_nearest_leader_sync,
      rwvcs_rwzk_nearest_leader_sync_f);
  return rwvcs_rwzk_nearest_leader_sync.status;
}


static void rwvcs_rwzk_next_instance_id_sync_f(void *ctx)
{
  rwvcs_rwzk_next_instance_id_sync_t *rwvcs_rwzk_next_instance_id_sync_p = 
      (rwvcs_rwzk_next_instance_id_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_next_instance_id_sync_p->rwvcs;
  uint32_t * instance_id = rwvcs_rwzk_next_instance_id_sync_p->instance_id;
  struct timeval tv = { .tv_sec = RWVCS_RWZK_TIMEOUT_S, .tv_usec = 0 };
  const char *instance_path = rwvcs_rwzk_next_instance_id_sync_p->instance_path;
  char * data = NULL;
  int r;

  rwvcs_rwzk_next_instance_id_sync_p->status = rwvcs_zk_lock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, &tv);
  RW_ASSERT(rwvcs_rwzk_next_instance_id_sync_p->status == RW_STATUS_SUCCESS);

  rwvcs_rwzk_next_instance_id_sync_p->status = rwvcs_zk_get(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, &data, NULL);
  RW_ASSERT(rwvcs_rwzk_next_instance_id_sync_p->status == RW_STATUS_SUCCESS);

  if (!data) {
    *instance_id = 100;
  } else {
    long int ret;

    ret = strtol(data, NULL, 10);
    RW_ASSERT(ret != LONG_MIN && ret != LONG_MAX && ret > 0);

    *instance_id = (uint32_t)ret;
  }

  free(data);
  r = asprintf(&data, "%u", *instance_id + 1);
  RW_ASSERT(r != -1);

  rwvcs_rwzk_next_instance_id_sync_p->status = rwvcs_zk_set(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path, data, NULL);
  RW_ASSERT(rwvcs_rwzk_next_instance_id_sync_p->status == RW_STATUS_SUCCESS);

  rwvcs_rwzk_next_instance_id_sync_p->status = rwvcs_zk_unlock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), instance_path);
  RW_ASSERT(rwvcs_rwzk_next_instance_id_sync_p->status == RW_STATUS_SUCCESS);
  RW_FREE(data);
  return;
}

rw_status_t
rwvcs_rwzk_next_instance_id(
    rwvcs_instance_ptr_t rwvcs,
    uint32_t * instance_id,
    const char* path)
{
  rwvcs_rwzk_next_instance_id_sync_t rwvcs_rwzk_next_instance_id_sync = {
    .rwvcs = rwvcs,
    .instance_id = instance_id,
    .instance_path = path ? path : g_auto_instance_path };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_next_instance_id_sync,
      rwvcs_rwzk_next_instance_id_sync_f);
  return rwvcs_rwzk_next_instance_id_sync.status;
}

/* Find all descendents of a given component.  A list of each descendents
 * instance name will be returned including the root component itself.
 *
 * NOTE:  both descendents and len_descendents should be set appropriately before
 * calling this function.  (I.E. to NULL and 0 on the first call).
 *
 * @param rwvcs           - rwvcs instance
 * @param root            - starting component
 * @param descendents     - on success, a null-terminated list of the instance
 *                          names of each descendent of the root node including
 *                          the root node as well.
 * @param len_descendents - number of descendents found.
 * @return                - RW_STATUS_SUCCESS
 *                          RW_STATUS_NOTFOUND if the component is not in the
 *                          rwzk datastore.
 *                          RW_STATUS_FAILURE on any other failure.
 */
static rw_status_t get_descendents(
    rwvcs_instance_ptr_t rwvcs,
    rw_component_info * root,
    char *** descendents,
    size_t * len_descendents)
{
  rw_status_t status;
  char ** list;

  if (!*len_descendents)
    *descendents = NULL;

  *descendents = (char **)realloc(*descendents, sizeof(char *) * (*len_descendents + 1));
  RW_ASSERT(*descendents);

  list = *descendents;
  list[*len_descendents] = strdup(root->instance_name);
  (*len_descendents)++;

  for (int i = 0; i < root->n_rwcomponent_children; ++i) {
    rw_component_info child;

    status = rwvcs_rwzk_lookup_component_internal(rwvcs, root->rwcomponent_children[i], &child);
    if (status == RW_STATUS_NOTFOUND){
      continue;
    } else if (status != RW_STATUS_SUCCESS) {
      return status;
    }

    status = get_descendents(rwvcs, &child, descendents, len_descendents);
    protobuf_c_message_free_unpacked_usebody(NULL, &child.base);
    if (status != RW_STATUS_SUCCESS)
      return status;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rwvcs_rwzk_descendents(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    char *** descendents)
{
  rw_status_t status;
  rw_component_info root;
  size_t n_descendents = 0;

  *descendents = NULL;

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &root);
  if (status != RW_STATUS_SUCCESS)
    goto err;

  status = get_descendents(rwvcs, &root, descendents, &n_descendents);
  protobuf_c_message_free_unpacked_usebody(NULL, &root.base);
  if (status != RW_STATUS_SUCCESS)
    goto err;

  *descendents = (char **)realloc(*descendents, sizeof(char *) * (n_descendents + 1));
  RW_ASSERT(*descendents);

  (*descendents)[n_descendents] = NULL;
  status = RW_STATUS_SUCCESS;
  goto done;

err:
  *descendents = NULL;

done:
  return status;
}

static void rwvcs_rwzk_descendents_component_sync_f(void *ctx)
{
  rwvcs_rwzk_descendents_sync_t *rwvcs_rwzk_descendants_sync_p = 
      (rwvcs_rwzk_descendents_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_descendants_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_descendants_sync_p->instance_name;
  rw_component_info *** components = rwvcs_rwzk_descendants_sync_p->components;
  rw_component_info * root;
  char ** descendents = NULL;
  size_t n_descendents = 0;
  rw_component_info ** ret = NULL;
  rwvcs_rwzk_descendants_sync_p->status = RW_STATUS_FAILURE;
  *components = NULL;

  root = (rw_component_info *)malloc(sizeof(rw_component_info));
  RW_ASSERT(root);
  if (!root)
    goto err;
  rw_component_info__init(root);

  rwvcs_rwzk_descendants_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, root);
  if (rwvcs_rwzk_descendants_sync_p->status != RW_STATUS_SUCCESS)
    goto err;

  rwvcs_rwzk_descendants_sync_p->status = get_descendents(rwvcs, root, &descendents, &n_descendents);
  if (rwvcs_rwzk_descendants_sync_p->status != RW_STATUS_SUCCESS)
    goto err;

  ret = (rw_component_info **)malloc(sizeof(rw_component_info *) * (n_descendents + 1));
  RW_ASSERT(ret);
  if (!ret)
    goto err;
  bzero(ret, sizeof(rw_component_info *) * (n_descendents + 1));

  for (size_t i = 0; i < n_descendents; ++i) {
    if (!strcmp(descendents[i], instance_name)) {
      ret[i] = root;
      root = NULL;
      continue;
    }

    ret[i] = (rw_component_info *)malloc(sizeof(rw_component_info));
    RW_ASSERT(ret[i]);
    if (!ret[i])
      goto err;
    rw_component_info__init(ret[i]);

    rwvcs_rwzk_descendants_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, descendents[i], ret[i]);
    if (rwvcs_rwzk_descendants_sync_p->status != RW_STATUS_SUCCESS)
      goto err;
  }

  rwvcs_rwzk_descendants_sync_p->status = RW_STATUS_SUCCESS;
  *components = ret;
  goto done;


err:
  if (ret) {
    for (size_t i = 0; ret[i]; ++i)
      protobuf_free(ret[i]);
    free(ret);
  }

  if (root)
    protobuf_free(root);

done:
  return;
}

rw_status_t rwvcs_rwzk_descendents_component(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    rw_component_info *** components)
{
  rwvcs_rwzk_descendents_sync_t rwvcs_rwzk_descendents_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .components = components};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_descendents_sync,
      rwvcs_rwzk_descendents_component_sync_f);
  return rwvcs_rwzk_descendents_sync.status;
}

static void rwvcs_rwzk_get_neighbors_sync_f (void *ctx)
{
  rwvcs_rwzk_get_neighbors_sync_t *rwvcs_rwzk_get_neighbors_sync_p = 
      (rwvcs_rwzk_get_neighbors_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_get_neighbors_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_get_neighbors_sync_p->instance_name;
  bool (*test)(rw_component_info *, void *userdata) = rwvcs_rwzk_get_neighbors_sync_p->test;
  void * userdata = rwvcs_rwzk_get_neighbors_sync_p->userdata;
  uint32_t height = rwvcs_rwzk_get_neighbors_sync_p->height;
  char *** neighbors = rwvcs_rwzk_get_neighbors_sync_p->neighbors;
  rw_component_info target;
  char **candidates;
  size_t len_neighbors = 0;
  size_t current_neighbor = 0;

  *neighbors = NULL;
  RW_ASSERT(test);
  rwvcs_rwzk_get_neighbors_sync_p->status = 
      rwvcs_rwzk_lookup_component_internal(
          rwvcs, instance_name, &target);
  if (rwvcs_rwzk_get_neighbors_sync_p->status != RW_STATUS_SUCCESS)
    return;

  // find the starting point
  for (int i = 0; i < height; ++i) {
    rwvcs_rwzk_get_neighbors_sync_p->status = 
        rwvcs_rwzk_lookup_component_internal(
            rwvcs, target.rwcomponent_parent, &target);
    if (rwvcs_rwzk_get_neighbors_sync_p->status == RW_STATUS_NOTFOUND)
      rwvcs_rwzk_get_neighbors_sync_p->status = RW_STATUS_OUT_OF_BOUNDS;
    if (rwvcs_rwzk_get_neighbors_sync_p->status != RW_STATUS_SUCCESS)
      goto err;
  }

  // Get a list of all the possible components
  rwvcs_rwzk_get_neighbors_sync_p->status = rwvcs_rwzk_descendents(rwvcs, target.instance_name, &candidates);
  if (rwvcs_rwzk_get_neighbors_sync_p->status != RW_STATUS_SUCCESS)
    goto err;

  // Test each component and build up the neighbors list
  for (size_t i = 0; candidates[i]; ++i) {
    rw_component_info child;

    rwvcs_rwzk_get_neighbors_sync_p->status = 
        rwvcs_rwzk_lookup_component_internal(
            rwvcs, candidates[i], &child);
    if (test(&child, userdata)) {
      // Alloctate more memory in the list if needed
      if (current_neighbor == len_neighbors) {
        *neighbors = (char **)realloc(*neighbors, sizeof(char *) * (len_neighbors + 10));
        RW_ASSERT(*neighbors);
        len_neighbors += 10;
        bzero(*neighbors + sizeof(char *) * current_neighbor, sizeof(char *) * 10);
      }

      (*neighbors)[current_neighbor] = strdup(child.instance_name);
      RW_ASSERT((*neighbors)[current_neighbor]);

      current_neighbor++;
    }

    protobuf_c_message_free_unpacked_usebody(NULL, &child.base);
  }

  if (!current_neighbor) {
    rwvcs_rwzk_get_neighbors_sync_p->status = RW_STATUS_NOTFOUND;
    goto err;
  }

  if (current_neighbor == len_neighbors) {
    *neighbors = (char **)realloc(*neighbors, sizeof(char *) * (len_neighbors + 1));
    RW_ASSERT(*neighbors);
    len_neighbors += 1;
  }
  (*neighbors)[current_neighbor] = NULL;

  rwvcs_rwzk_get_neighbors_sync_p->status = RW_STATUS_SUCCESS;
  goto done;

err:
  if (*neighbors) {
    for (size_t i = 0; *neighbors[i]; ++i)
      free((*neighbors)[i]);
    free(*neighbors);
    *neighbors = NULL;
  }

done:
  return;
}

rw_status_t
rwvcs_rwzk_get_neighbors(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool (*test)(rw_component_info *, void *userdata),
    void * userdata,
    uint32_t height,
    char *** neighbors)
{
  rwvcs_rwzk_get_neighbors_sync_t rwvcs_rwzk_get_neighbors_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .test = test,
    .userdata = userdata,
    .height = height,
    .neighbors = neighbors};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_get_neighbors_sync,
      rwvcs_rwzk_get_neighbors_sync_f);
  return rwvcs_rwzk_get_neighbors_sync.status;
}

rw_status_t
rwvcs_rwzk_climb_get_neighbors(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool (*test)(rw_component_info *, void *userdata),
    void * userdata,
    char *** neighbors)
{
  rw_status_t status;
  char ** matches = NULL;

  *neighbors = NULL;

  for (uint32_t height = 0; ; ++height) {
    status = rwvcs_rwzk_get_neighbors(
        rwvcs,
        instance_name,
        test,
        userdata,
        height,
        &matches);

    if (status == RW_STATUS_OUT_OF_BOUNDS) {
      status = RW_STATUS_NOTFOUND;
      break;
    } else if (status == RW_STATUS_SUCCESS && matches) {
      *neighbors = matches;
      status = RW_STATUS_SUCCESS;
      break;
    }
  }

  return status;
}

bool
rwvcs_rwzk_test_component_name(rw_component_info * component, void * userdata) {
  return !strcmp(component->component_name, (char *)userdata);
}

bool
rwvcs_rwzk_test_component_type(rw_component_info * component, void * userdata) {
  return component->component_type == *(rw_component_type *)userdata;
}

bool rwvcs_rwzk_has_child(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * child_instance)
{
  rw_status_t status;
  rw_component_info parent;
  bool found = false;

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &parent);
  if (status != RW_STATUS_SUCCESS)
    return false;

  for (int i = 0; i < parent.n_rwcomponent_children; ++i) {
    if (!strcmp(child_instance, parent.rwcomponent_children[i])) {
      found = true;
      break;
    }
  }

  protobuf_free_stack(parent);

  return found;
}

static void rwvcs_rwzk_responsible_for_sync_f(void *ctx)
{
  rwvcs_rwzk_responsible_for_sync_t *rwvcs_rwzk_responsible_for_sync_p = 
      (rwvcs_rwzk_responsible_for_sync_t*)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_responsible_for_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_responsible_for_sync_p->instance_name;
  const char * target_instance = rwvcs_rwzk_responsible_for_sync_p->target_instance;
  rw_status_t status;
  rw_component_info parent;
  rwvcs_rwzk_responsible_for_sync_p->responsible = false;

  if (rwvcs_rwzk_has_child(rwvcs, instance_name, target_instance)) {
    rwvcs_rwzk_responsible_for_sync_p->responsible = true;
    return;
  }

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &parent);
  if (status != RW_STATUS_SUCCESS) {
    rwvcs_rwzk_responsible_for_sync_p->responsible = false;
    return;
  }

  /*
   * If this is a lead VM for either the targeted instance or if it is leading the
   * collection that owns the target, then this VM is responsible.
   */
  if (parent.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM
      && parent.vm_info->has_leader
      && parent.vm_info->leader) {
    if (!strcmp(target_instance, parent.rwcomponent_parent)
        || rwvcs_rwzk_has_child(rwvcs, parent.rwcomponent_parent, target_instance))
      rwvcs_rwzk_responsible_for_sync_p->responsible = true;
  }

  protobuf_free_stack(parent);

  return;
}
bool rwvcs_rwzk_responsible_for(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * target_instance)
{
  rwvcs_rwzk_responsible_for_sync_t rwvcs_rwzk_responsible_for_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .target_instance = target_instance};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_responsible_for_sync,
      rwvcs_rwzk_responsible_for_sync_f);
  return rwvcs_rwzk_responsible_for_sync.responsible;
}

static void rwvcs_rwzk_is_leader_of_sync_f(void *ctx)
{
  rwvcs_rwzk_is_leader_of_sync_t *rwvcs_rwzk_is_leader_of_sync_p = 
      (rwvcs_rwzk_is_leader_of_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_is_leader_of_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_is_leader_of_sync_p->instance_name;
  const char * target = rwvcs_rwzk_is_leader_of_sync_p->target;
  rw_status_t status;
  rw_component_info ci;
  rwvcs_rwzk_is_leader_of_sync_p->ret = false;

  status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &ci);
  if (status != RW_STATUS_SUCCESS) {
    return;
  }

  rwvcs_rwzk_is_leader_of_sync_p->ret = (ci.component_type == RWVCS_TYPES_COMPONENT_TYPE_RWVM
      && ci.vm_info->has_leader
      && ci.vm_info->leader
      && ci.rwcomponent_parent
      && !strcmp(target, ci.rwcomponent_parent));

  protobuf_free_stack(ci);

  return;
}

bool rwvcs_rwzk_is_leader_of(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    const char * target)
{
  rwvcs_rwzk_is_leader_of_sync_t rwvcs_rwzk_is_leader_of_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .target = target};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_is_leader_of_sync,
      rwvcs_rwzk_is_leader_of_sync_f);
  return rwvcs_rwzk_is_leader_of_sync.ret;
}

static void rwvcs_rwzk_update_config_ready_sync_f (void *ctx)
{
  rwvcs_rwzk_update_config_ready_sync_t *rwvcs_rwzk_update_config_ready_sync_p = 
      (rwvcs_rwzk_update_config_ready_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_update_config_ready_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_update_config_ready_sync_p->instance_name;
  bool config_ready = rwvcs_rwzk_update_config_ready_sync_p->config_ready;
  rw_status_t unlock_status;
  rw_component_info info;
  struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };

  rw_component_info__init(&info);

  rwvcs_rwzk_update_config_ready_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, instance_name, &timeout);
  if (rwvcs_rwzk_update_config_ready_sync_p->status != RW_STATUS_SUCCESS)
    goto done;

  rwvcs_rwzk_update_config_ready_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &info);
  if (rwvcs_rwzk_update_config_ready_sync_p->status != RW_STATUS_SUCCESS)
    goto unlock_info;

  info.has_config_ready = true;
  info.config_ready = config_ready;
  rwvcs_rwzk_update_config_ready_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &info);
  RW_ASSERT(rwvcs_rwzk_update_config_ready_sync_p->status == RW_STATUS_SUCCESS);

unlock_info:
  unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, instance_name);
  RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);

done:
  protobuf_free_stack(info);

  return;
}

rw_status_t
rwvcs_rwzk_update_config_ready(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    bool         config_ready,
    vcs_recovery_type recovery_action)

{
  rwvcs_rwzk_update_config_ready_sync_t rwvcs_rwzk_update_config_ready_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .config_ready = config_ready,
    .recovery_action = recovery_action};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_update_config_ready_sync,
      rwvcs_rwzk_update_config_ready_sync_f);
  return rwvcs_rwzk_update_config_ready_sync.status;
}

static void rwvcs_rwzk_update_recovery_action_sync_f (void *ctx)
{
  rwvcs_rwzk_update_recovery_action_sync_t *rwvcs_rwzk_update_recovery_action_sync_p = 
      (rwvcs_rwzk_update_recovery_action_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_update_recovery_action_sync_p->rwvcs;
  const char * instance_name = rwvcs_rwzk_update_recovery_action_sync_p->instance_name;
  vcs_recovery_type recovery_action = rwvcs_rwzk_update_recovery_action_sync_p->recovery_action;
  rw_status_t unlock_status;
  rw_component_info info;
  struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };

  rw_component_info__init(&info);

  rwvcs_rwzk_update_recovery_action_sync_p->status = rwvcs_rwzk_lock_internal(rwvcs, instance_name, &timeout);
  if (rwvcs_rwzk_update_recovery_action_sync_p->status != RW_STATUS_SUCCESS)
    goto done;

  rwvcs_rwzk_update_recovery_action_sync_p->status = rwvcs_rwzk_lookup_component_internal(rwvcs, instance_name, &info);
  if (rwvcs_rwzk_update_recovery_action_sync_p->status != RW_STATUS_SUCCESS)
    goto unlock_info;

  info.has_recovery_action = true;
  info.recovery_action = recovery_action;
  rwvcs_rwzk_update_recovery_action_sync_p->status = rwvcs_rwzk_node_update_internal(rwvcs, &info);
  RW_ASSERT(rwvcs_rwzk_update_recovery_action_sync_p->status == RW_STATUS_SUCCESS);

unlock_info:
  unlock_status = rwvcs_rwzk_unlock_internal(rwvcs, instance_name);
  RW_ASSERT(unlock_status == RW_STATUS_SUCCESS);

done:
  protobuf_free_stack(info);

  return;
}

rw_status_t
rwvcs_rwzk_update_recovery_action(
    rwvcs_instance_ptr_t rwvcs,
    const char * instance_name,
    vcs_recovery_type recovery_action)

{
  rwvcs_rwzk_update_recovery_action_sync_t rwvcs_rwzk_update_recovery_action_sync = {
    .rwvcs = rwvcs,
    .instance_name = instance_name,
    .recovery_action = recovery_action};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_update_recovery_action_sync,
      rwvcs_rwzk_update_recovery_action_sync_f);
  return rwvcs_rwzk_update_recovery_action_sync.status;
}


typedef struct rwvcs_rwzk_watcher_closure {
  rwsched_tasklet_ptr_t  sched_tasklet_ptr;
  rwsched_dispatch_queue_t rwq;
  void (*cb)(void* ud);
  void *ud;
} rwvcs_rwzk_watcher_closure_t;

#define RWVCS_ZK_RET_UD_IDX(ud, type, idx) ((type *)rwvcs_zk_get_userdata_idx(ud, idx))
static rw_status_t rwvcs_rwzk_watcher_func(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    void *ud, int len)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  RW_ASSERT(len == 1);
  rwvcs_rwzk_watcher_closure_t *ud_closure = RWVCS_ZK_RET_UD_IDX(ud, rwvcs_rwzk_watcher_closure_t, 0);

  rwsched_dispatch_async_f(ud_closure->sched_tasklet_ptr, ud_closure->rwq, ud_closure->ud, ud_closure->cb);

  return status;
}

void rwvcs_rwzk_watcher_start_sync_f(void *ctx)
{
  rwvcs_rwzk_watcher_start_sync_t *rwvcs_rwzk_watcher_start_sync_p = 
      (rwvcs_rwzk_watcher_start_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_watcher_start_sync_p->rwvcs;

  rwvcs_rwzk_watcher_closure_t *ud_closure = malloc(sizeof(*ud_closure));
  ud_closure->sched_tasklet_ptr = rwvcs_rwzk_watcher_start_sync_p->sched_tasklet_ptr;
  ud_closure->rwq = rwvcs_rwzk_watcher_start_sync_p->rwq;
  ud_closure->cb = rwvcs_rwzk_watcher_start_sync_p->cb;
  ud_closure->ud = rwvcs_rwzk_watcher_start_sync_p->ud;

  rwvcs_zk_closure_ptr_t closure = rwvcs_zk_closure_alloc(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      &rwvcs_rwzk_watcher_func,
      (void *)ud_closure);
  RW_ASSERT(closure);

  rw_status_t status = rwvcs_zk_register_watcher(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      rwvcs_rwzk_watcher_start_sync_p->path,
      closure);

  RW_ASSERT(status == RW_STATUS_SUCCESS);
  rwvcs_rwzk_watcher_start_sync_p->closure = closure;

  return;
}

rwvcs_zk_closure_ptr_t
rwvcs_rwzk_watcher_start(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    rwsched_tasklet_ptr_t sched_tasklet_ptr,
    rwsched_dispatch_queue_t rwq,
    void (*cb)(void* ud),
    void *ud)
{
  rwvcs_rwzk_watcher_start_sync_t rwvcs_rwzk_watcher_start_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .sched_tasklet_ptr = sched_tasklet_ptr,
    .rwq = rwq,
    .cb = cb,
    .ud = ud};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_watcher_start_sync,
      rwvcs_rwzk_watcher_start_sync_f);
  return rwvcs_rwzk_watcher_start_sync.closure;
}

void rwvcs_rwzk_get_children_sync_f(void *ctx)
{
  rwvcs_rwzk_get_children_sync_t *rwvcs_rwzk_get_children_sync_p = 
      (rwvcs_rwzk_get_children_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_get_children_sync_p->rwvcs;

  rwvcs_rwzk_get_children_sync_p->status = rwvcs_zk_get_children(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      rwvcs_rwzk_get_children_sync_p->path,
      rwvcs_rwzk_get_children_sync_p->children,
      NULL);

  return;
}

rw_status_t
rwvcs_rwzk_get_children(
    rwvcs_instance_ptr_t rwvcs,
    const char *path,
    char ***children)
{
  rwvcs_rwzk_get_children_sync_t rwvcs_rwzk_get_children_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .children = children};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_get_children_sync,
      rwvcs_rwzk_get_children_sync_f);
  return rwvcs_rwzk_get_children_sync.status;
}

void rwvcs_rwzk_get_sync_f(void *ctx)
{
  rwvcs_rwzk_get_sync_t *rwvcs_rwzk_get_sync_p = 
      (rwvcs_rwzk_get_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_get_sync_p->rwvcs;

  rwvcs_rwzk_get_sync_p->status = rwvcs_zk_get(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      rwvcs_rwzk_get_sync_p->path,
      rwvcs_rwzk_get_sync_p->data,
      NULL);

  return;
}

rw_status_t
rwvcs_rwzk_get(
    rwvcs_instance_ptr_t rwvcs,
    const char *path,
    char ** data)
{
  rwvcs_rwzk_get_sync_t rwvcs_rwzk_get_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .data = data};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_get_sync,
      rwvcs_rwzk_get_sync_f);
  return rwvcs_rwzk_get_sync.status;
}

void rwvcs_rwzk_set_sync_f(void *ctx)
{
  rwvcs_rwzk_set_sync_t *rwvcs_rwzk_set_sync_p = 
      (rwvcs_rwzk_set_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_set_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_set_sync_p->path;
  const char * data = rwvcs_rwzk_set_sync_p->data;

  rwvcs_rwzk_set_sync_p->status = rwvcs_zk_set(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, data, NULL);

  return;
}

rw_status_t
rwvcs_rwzk_set(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path,
    const char * data)
{
  rwvcs_rwzk_set_sync_t rwvcs_rwzk_set_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .data = data};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_set_sync,
      rwvcs_rwzk_set_sync_f);
  return rwvcs_rwzk_set_sync.status;
}

void rwvcs_rwzk_create_sync_f(void *ctx)
{
  rwvcs_rwzk_create_sync_t *rwvcs_rwzk_create_sync_p = 
      (rwvcs_rwzk_create_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_create_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_create_sync_p->path;

  rwvcs_rwzk_create_sync_p->status = rwvcs_zk_create(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, NULL);

  return;
}

rw_status_t
rwvcs_rwzk_create(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path)
{
  rwvcs_rwzk_create_sync_t rwvcs_rwzk_create_sync = {
    .rwvcs = rwvcs,
    .path = path};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_create_sync,
      rwvcs_rwzk_create_sync_f);
  return rwvcs_rwzk_create_sync.status;
}

void rwvcs_rwzk_exists_sync_f(void *ctx)
{
  rwvcs_rwzk_exists_sync_t *rwvcs_rwzk_exists_sync_p = 
      (rwvcs_rwzk_exists_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_exists_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_exists_sync_p->path;

  rwvcs_rwzk_exists_sync_p->exists = rwvcs_zk_exists(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path);

  return;
}

bool
rwvcs_rwzk_exists(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path)
{
  rwvcs_rwzk_exists_sync_t rwvcs_rwzk_exists_sync = {
    .rwvcs = rwvcs,
    .path = path};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_exists_sync,
      rwvcs_rwzk_exists_sync_f);
  return rwvcs_rwzk_exists_sync.exists;
}

void rwvcs_rwzk_lock_path_sync_f(void *ctx)
{
  rwvcs_rwzk_lock_path_sync_t *rwvcs_rwzk_lock_path_sync_p = 
      (rwvcs_rwzk_lock_path_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_lock_path_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_lock_path_sync_p->path;
  struct timeval * timeout = rwvcs_rwzk_lock_path_sync_p->timeout;
  rwvcs_rwzk_lock_path_sync_p->status = rwvcs_zk_lock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, timeout);
  return;
}

rw_status_t
rwvcs_rwzk_lock_path(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path, 
    struct timeval * timeout)
{
  rwvcs_rwzk_lock_path_sync_t rwvcs_rwzk_lock_path_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .timeout = timeout};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_lock_path_sync,
      rwvcs_rwzk_lock_path_sync_f);
  return rwvcs_rwzk_lock_path_sync.status;
}

void rwvcs_rwzk_unlock_path_sync_f(void *ctx)
{
  rwvcs_rwzk_unlock_path_sync_t *rwvcs_rwzk_unlock_path_sync_p = 
      (rwvcs_rwzk_unlock_path_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_unlock_path_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_unlock_path_sync_p->path;
  rwvcs_rwzk_unlock_path_sync_p->status = rwvcs_zk_unlock(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path);
  return;
}

rw_status_t
rwvcs_rwzk_unlock_path(
    rwvcs_instance_ptr_t rwvcs, 
    const char * path)
{
  rwvcs_rwzk_unlock_path_sync_t rwvcs_rwzk_unlock_path_sync = {
    .rwvcs = rwvcs,
    .path = path};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_unlock_path_sync,
      rwvcs_rwzk_unlock_path_sync_f);
  return rwvcs_rwzk_unlock_path_sync.status;
}

static void rwvcs_rwzk_delete_path_sync_f(void *ctx)
{
  rwvcs_rwzk_delete_path_sync_t *rwvcs_rwzk_delete_path_sync_p =
      (rwvcs_rwzk_delete_path_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_delete_path_sync_p->rwvcs;
  const char * path = rwvcs_rwzk_delete_path_sync_p->path;
  RW_CF_TYPE_VALIDATE(rwvcs, rwvcs_instance_ptr_t);
  rwvcs_rwzk_delete_path_sync_p->status = rwvcs_zk_delete(RWVX_GET_ZK_MODULE(rwvcs->rwvx), path, NULL);
  return;
}

rw_status_t
rwvcs_rwzk_delete_path(rwvcs_instance_ptr_t rwvcs, const char * path)
{
  rwvcs_rwzk_delete_path_sync_t  rwvcs_rwzk_delete_path_sync = {
    .rwvcs = rwvcs,
    .path = path};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_delete_path_sync,
      rwvcs_rwzk_delete_path_sync_f);
  return rwvcs_rwzk_delete_path_sync.status;
}

void rwvcs_rwzk_watcher_stop_sync_f(void *ctx)
{
  rwvcs_rwzk_watcher_stop_sync_t *rwvcs_rwzk_watcher_stop_sync_p = 
      (rwvcs_rwzk_watcher_stop_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_watcher_stop_sync_p->rwvcs;
  rwvcs_rwzk_watcher_stop_sync_p->status = rwvcs_zk_unregister_watcher(
      RWVX_GET_ZK_MODULE(rwvcs->rwvx),
      rwvcs_rwzk_watcher_stop_sync_p->path,
      (*rwvcs_rwzk_watcher_stop_sync_p->closure));
  RW_ASSERT(rwvcs_rwzk_watcher_stop_sync_p->status == RW_STATUS_SUCCESS);
  rwvcs_zk_closure_free(rwvcs_rwzk_watcher_stop_sync_p->closure);
  return;
}

rw_status_t
rwvcs_rwzk_watcher_stop(
    rwvcs_instance_ptr_t rwvcs,
    const char * path,
    rwvcs_zk_closure_ptr_t *closure)
{
  rwvcs_rwzk_watcher_stop_sync_t rwvcs_rwzk_watcher_stop_sync = {
    .rwvcs = rwvcs,
    .path = path,
    .closure = closure};

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_watcher_stop_sync,
      rwvcs_rwzk_watcher_stop_sync_f);
  return rwvcs_rwzk_watcher_stop_sync.status;
}

static void rwvcs_rwzk_client_state_sync_f(void *ctx)
{
  rwvcs_rwzk_client_state_sync_t *rwvcs_rwzk_client_state_sync_p = 
      (rwvcs_rwzk_client_state_sync_t *)ctx;
  rwvcs_instance_ptr_t rwvcs = rwvcs_rwzk_client_state_sync_p->rwvcs;

  if (rwvcs->pb_rwmanifest->bootstrap_phase->zookeeper->zake) {
    rwvcs_rwzk_client_state_sync_p->client_state = RW_VCS_ZK_KAZOO_CLIENT_STATE_ENUM_CONNECTED;
  } else {
    rwvcs_rwzk_client_state_sync_p->client_state = rwvcs_zk_client_state(RWVX_GET_ZK_MODULE(rwvcs->rwvx));
  }
}

rwvcs_zk_kazoo_client_state_t rwvcs_rwzk_client_state(rwvcs_instance_ptr_t rwvcs)
{
  rwvcs_rwzk_client_state_sync_t rwvcs_rwzk_client_state_sync = {
    .rwvcs = rwvcs,
  };

  RWVCS_RWZK_DISPATCH(
      sync,
      rwvcs->rwvx->rwsched_tasklet, 
      rwvcs->zk_rwq,
      &rwvcs_rwzk_client_state_sync,
      rwvcs_rwzk_client_state_sync_f);
  return rwvcs_rwzk_client_state_sync.client_state;
}

