
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <string.h>

#include "rwcal-api.h"

rw_status_t rwcal_rwzk_create_server_config(
    rwcal_module_ptr_t rwcal,
    int id,
    int client_port,
    bool unique_ports,
    rwcal_zk_server_port_detail_ptr_t *server_details)
{
  rw_status_t status;

  status = rwcal->zk_iface->create_server_config(
      rwcal->zk_cls,
      id,
      client_port,
      unique_ports,
      server_details);

  return status;
}

int rwcal_rwzk_server_start(rwcal_module_ptr_t rwcal, int id)
{

  return rwcal->zk_iface->server_start(rwcal->zk_cls, id);
}

rw_status_t rwcal_rwzk_kazoo_init(
    rwcal_module_ptr_t rwcal,
    rwcal_zk_server_port_detail_ptr_t *server_details)
{
  rw_status_t status;

  status = rwcal->zk_iface->kazoo_init(
      rwcal->zk_cls,
      server_details);

  return status;
}

rw_status_t rwcal_rwzk_kazoo_start(rwcal_module_ptr_t rwcal)
{
  rw_status_t status;

  status = rwcal->zk_iface->kazoo_start(
      rwcal->zk_cls);

  return status;
}

rw_status_t rwcal_rwzk_kazoo_stop(rwcal_module_ptr_t rwcal)
{
  rw_status_t status;

  status = rwcal->zk_iface->kazoo_stop(
      rwcal->zk_cls);

  return status;
}

rw_status_t
rwcal_rwzk_zake_init(rwcal_module_ptr_t rwcal)
{
  rw_status_t status;

  status = rwcal->zk_iface->zake_init(rwcal->zk_cls);

  return status;
}

rw_status_t
rwcal_rwzk_lock(rwcal_module_ptr_t rwcal, const char * path, struct timeval * timeout)
{
  rw_status_t status;

  if (timeout)
    status = rwcal->zk_iface->lock(
        rwcal->zk_cls,
        path,
        timeout->tv_sec + (timeout->tv_usec/1000000.0f));
  else
    status = rwcal->zk_iface->lock(rwcal->zk_cls, path, 0);

  return status;
}

rw_status_t
rwcal_rwzk_unlock(rwcal_module_ptr_t rwcal, const char * path)
{
  rw_status_t status;

  status = rwcal->zk_iface->unlock(rwcal->zk_cls, path);

  return status;
}

bool
rwcal_rwzk_locked(rwcal_module_ptr_t rwcal, const char * path)
{
  bool ret;

  ret = (bool)rwcal->zk_iface->locked(rwcal->zk_cls, path);

  return ret;
}

rw_status_t
rwcal_rwzk_create(rwcal_module_ptr_t rwcal, const char * path,
                  const rwcal_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwcal->zk_iface->create(rwcal->zk_cls, path, closure);

  return status;
}

bool
rwcal_rwzk_exists(rwcal_module_ptr_t rwcal, const char * path)
{
  bool ret;

  ret = (bool)rwcal->zk_iface->exists(rwcal->zk_cls, path);

  return ret;
}

rw_status_t rwcal_rwzk_get(
    rwcal_module_ptr_t rwcal,
    const char * path,
    char ** data,
    const rwcal_closure_ptr_t closure)
{
  int status;

  *data = NULL;

  status = rwcal->zk_iface->get(rwcal->zk_cls, path, (gchar **)data, closure);
  if (status != RW_STATUS_SUCCESS && *data) {
    free(*data);
    *data = NULL;
  }

  return status;
}

rw_status_t rwcal_rwzk_set(
    rwcal_module_ptr_t rwcal,
    const char * path,
    const char * data,
    const rwcal_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwcal->zk_iface->set(rwcal->zk_cls, path, data, closure);

  return status;
}

rw_status_t rwcal_rwzk_get_children(
    rwcal_module_ptr_t rwcal,
    const char * path,
    char *** children,
    const rwcal_closure_ptr_t closure)
{
  rw_status_t status;

  *children = NULL;

  status = rwcal->zk_iface->children(rwcal->zk_cls, path, children, closure);
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

rw_status_t rwcal_rwzk_delete(rwcal_module_ptr_t rwcal, const char * path,
                              const rwcal_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwcal->zk_iface->rm(rwcal->zk_cls, path, closure);

  return status;
}

rw_status_t rwcal_rwzk_register_watcher(
    rwcal_module_ptr_t rwcal,
    const char * path,
    const rwcal_closure_ptr_t closure)
{
  rw_status_t status;

  status = rwcal->zk_iface->register_watcher(rwcal->zk_cls, path, closure);

  return status;
}

rw_status_t rwcal_rwzk_unregister_watcher(
    rwcal_module_ptr_t rwcal,
    const char * path,
    const rwcal_closure_ptr_t closure)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  status = rwcal->zk_iface->unregister_watcher(rwcal->zk_cls, path, closure);

  return status;
}

void *rwcal_get_userdata_idx(void *userdata, int idx)
{
  return ((void *)(((unsigned long *)userdata)[idx]));
}

rwcal_kazoo_client_state_t
rwcal_rwzk_client_state(rwcal_module_ptr_t rwcal)
{
  return rwcal->zk_iface->get_client_state(rwcal->zk_cls);
}

