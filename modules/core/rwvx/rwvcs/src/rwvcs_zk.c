
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include <string.h>
#include "rwvcs_zk_api.h"
#include <libpeas/peas.h>


rw_status_t rwvcs_zk_create_server_config(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    int id,
    int client_port,
    bool unique_ports,
    rwvcs_zk_server_port_detail_ptr_t *server_details,
    const char *rift_var_root)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->create_server_config(
      rwvcs_zk->zk_cls,
      id,
      client_port,
      unique_ports,
      server_details,
      rift_var_root);

  return status;
}

int rwvcs_zk_server_start(rwvcs_zk_module_ptr_t rwvcs_zk, int id)
{

  return rwvcs_zk->zk_iface->server_start(rwvcs_zk->zk_cls, id);
}

rw_status_t rwvcs_zk_kazoo_init(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    rwvcs_zk_server_port_detail_ptr_t *server_details)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->kazoo_init(
      rwvcs_zk->zk_cls,
      server_details);

  return status;
}

rw_status_t rwvcs_zk_kazoo_start(rwvcs_zk_module_ptr_t rwvcs_zk)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->kazoo_start(
      rwvcs_zk->zk_cls);

  return status;
}

rw_status_t rwvcs_zk_kazoo_stop(rwvcs_zk_module_ptr_t rwvcs_zk)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->kazoo_stop(
      rwvcs_zk->zk_cls);

  return status;
}

rw_status_t
rwvcs_zk_zake_init(rwvcs_zk_module_ptr_t rwvcs_zk)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->zake_init(rwvcs_zk->zk_cls);

  return status;
}

rw_status_t
rwvcs_zk_lock(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path, struct timeval * timeout)
{
  rw_status_t status;

  if (timeout)
    status = rwvcs_zk->zk_iface->lock(
        rwvcs_zk->zk_cls,
        path,
        timeout->tv_sec + (timeout->tv_usec/1000000.0f));
  else
    status = rwvcs_zk->zk_iface->lock(rwvcs_zk->zk_cls, path, 0);

  return status;
}

rw_status_t
rwvcs_zk_unlock(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->unlock(rwvcs_zk->zk_cls, path);

  return status;
}

bool
rwvcs_zk_locked(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path)
{
  bool ret;

  ret = (bool)rwvcs_zk->zk_iface->locked(rwvcs_zk->zk_cls, path);

  return ret;
}

rw_status_t
rwvcs_zk_create(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path,
                  const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->create(rwvcs_zk->zk_cls, path, closure);

  return status;
}

bool
rwvcs_zk_exists(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path)
{
  bool ret;

  ret = (bool)rwvcs_zk->zk_iface->exists(rwvcs_zk->zk_cls, path);

  return ret;
}

rw_status_t rwvcs_zk_get(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    char ** data,
    const rwvcs_zk_closure_ptr_t closure)
{
  int status;

  *data = NULL;

  status = rwvcs_zk->zk_iface->get(rwvcs_zk->zk_cls, path, (gchar **)data, closure);
  if (status != RW_STATUS_SUCCESS && *data) {
    free(*data);
    *data = NULL;
  }

  return status;
}

rw_status_t rwvcs_zk_set(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const char * data,
    const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->set(rwvcs_zk->zk_cls, path, data, closure);

  return status;
}

rw_status_t rwvcs_zk_get_children(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    char *** children,
    const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status;

  *children = NULL;

  status = rwvcs_zk->zk_iface->children(rwvcs_zk->zk_cls, path, children, closure);
  if (status != RW_STATUS_SUCCESS) {
    if (*children) {
      for (int i = 0; (*children)[i]; ++i)
        free((*children)[i]);
      free(*children);
    }

    *children = NULL;
  }

  return status;
}

rw_status_t rwvcs_zk_delete(rwvcs_zk_module_ptr_t rwvcs_zk, const char * path,
                              const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->rm(rwvcs_zk->zk_cls, path, closure);

  return status;
}

rw_status_t rwvcs_zk_register_watcher(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwvcs_zk->zk_iface->register_watcher(rwvcs_zk->zk_cls, path, closure);

  return status;
}

rw_status_t rwvcs_zk_unregister_watcher(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    const char * path,
    const rwvcs_zk_closure_ptr_t closure)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  status = rwvcs_zk->zk_iface->unregister_watcher(rwvcs_zk->zk_cls, path, closure);

  return status;
}

void *rwvcs_zk_get_userdata_idx(void *userdata, int idx)
{
  return ((void *)(((unsigned long *)userdata)[idx]));
}

rwvcs_zk_kazoo_client_state_t
rwvcs_zk_client_state(rwvcs_zk_module_ptr_t rwvcs_zk)
{
  return rwvcs_zk->zk_iface->get_client_state(rwvcs_zk->zk_cls);
}

rwvcs_zk_module_ptr_t rwvcs_zk_module_alloc()
{
  rw_status_t status;
  rwvcs_zk_module_ptr_t rwvcs_zk;

  rwvcs_zk = (rwvcs_zk_module_ptr_t)malloc(sizeof(struct rwvcs_zk_module_s));
  if (!rwvcs_zk)
    return NULL;

  bzero(rwvcs_zk, sizeof(struct rwvcs_zk_module_s));

  rwvcs_zk->framework = rw_vx_framework_alloc();
  if (!rwvcs_zk->framework)
    goto err;

  rw_vx_require_repository("RwVcsZk", "1.0");

  status = rw_vx_library_open(rwvcs_zk->framework, "rwvcs_zk", "", &rwvcs_zk->mip);
  if (status != RW_STATUS_SUCCESS)
    goto err;

  rwvcs_zk->zk = peas_engine_create_extension(
      rwvcs_zk->framework->engine,
      rwvcs_zk->mip->modp->plugin_info,
      RW_VCS_ZK_TYPE_ZOOKEEPER,
      NULL);
  if (!rwvcs_zk->zk)
    goto err;

  rwvcs_zk->zk_cls = RW_VCS_ZK_ZOOKEEPER(rwvcs_zk->zk);
  rwvcs_zk->zk_iface = RW_VCS_ZK_ZOOKEEPER_GET_INTERFACE(rwvcs_zk->zk);

  goto done;

err:
  rwvcs_zk_module_free(&rwvcs_zk);

done:

  return rwvcs_zk;
}

void rwvcs_zk_module_free(rwvcs_zk_module_ptr_t * rwvcs_zk)
{
  if ((*rwvcs_zk)->zk)
    g_object_unref((*rwvcs_zk)->zk);

  if ((*rwvcs_zk)->mip)
    rw_vx_modinst_close((*rwvcs_zk)->mip);

  if ((*rwvcs_zk)->framework)
    rw_vx_framework_free((*rwvcs_zk)->framework);

  free(*rwvcs_zk);
  *rwvcs_zk = NULL;

  return;
}

rwvcs_zk_closure_ptr_t rwvcs_zk_closure_alloc(
    rwvcs_zk_module_ptr_t rwvcs_zk,
    rw_status_t (*callback)(rwvcs_zk_module_ptr_t, void *, int),
    void * user_data)
{
  rwvcs_zk_closure_ptr_t closure = NULL;

  closure = g_object_new(RW_VCS_ZK_TYPE_CLOSURE, NULL);
  if (!closure)
    goto done;

  closure->m_rwvcs_zk = (void *)rwvcs_zk;
  closure->m_callback = (RwVcsZkrwvcs_zk_callback)callback;
  closure->m_user_data = user_data;

done:
  return closure;
}

void rwvcs_zk_closure_free(rwvcs_zk_closure_ptr_t * closure)
{
  g_object_unref(*closure);
  *closure = NULL;

  return;
}

rwvcs_zk_server_port_detail_ptr_t rwvcs_zk_server_port_detail_alloc(
    char *zk_server_addr,
    int zk_client_port,
    int zk_server_id,
    bool zk_server_start,
    bool zk_client_connect)
{
  rwvcs_zk_server_port_detail_ptr_t zk_server_port_detail = NULL;

  zk_server_port_detail = g_object_new(RW_VCS_ZK_TYPE_SERVER_PORT_DETAIL, NULL);
  if (!zk_server_port_detail)
    goto done;

  zk_server_port_detail->zk_server_addr = strdup(zk_server_addr);
  zk_server_port_detail->zk_client_port = zk_client_port;
  zk_server_port_detail->zk_server_id = zk_server_id;
  zk_server_port_detail->zk_server_start = zk_server_start;
  zk_server_port_detail->zk_client_connect = zk_client_connect;

done:
  return zk_server_port_detail;
}

void rwvcs_zk_server_port_detail_free(rwvcs_zk_server_port_detail_ptr_t * zk_server_port_detail)
{
  g_object_unref(*zk_server_port_detail);
  *zk_server_port_detail = NULL;

  return;
}

