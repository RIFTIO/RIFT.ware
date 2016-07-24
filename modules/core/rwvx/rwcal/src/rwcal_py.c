
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#include <libpeas/peas.h>

#include "rwcal-api.h"

rwcal_module_ptr_t rwcal_module_alloc()
{
  rw_status_t status;
  rwcal_module_ptr_t rwcal;

  rwcal = (rwcal_module_ptr_t)malloc(sizeof(struct rwcal_module_s));
  if (!rwcal)
    return NULL;

  bzero(rwcal, sizeof(struct rwcal_module_s));

  rwcal->framework = rw_vx_framework_alloc();
  if (!rwcal->framework)
    goto err;

  rw_vx_require_repository("RwCal", "1.0");

  status = rw_vx_library_open(rwcal->framework, "rwcal_zk", "", &rwcal->mip);
  if (status != RW_STATUS_SUCCESS)
    goto err;

  rwcal->zk = peas_engine_create_extension(
      rwcal->framework->engine,
      rwcal->mip->modp->plugin_info,
      RW_CAL_TYPE_ZOOKEEPER,
      NULL);
  if (!rwcal->zk)
    goto err;

  rwcal->zk_cls = RW_CAL_ZOOKEEPER(rwcal->zk);
  rwcal->zk_iface = RW_CAL_ZOOKEEPER_GET_INTERFACE(rwcal->zk);

  goto done;

err:
  rwcal_module_free(&rwcal);

done:

  return rwcal;
}

void rwcal_module_free(rwcal_module_ptr_t * rwcal)
{
  if ((*rwcal)->zk)
    g_object_unref((*rwcal)->zk);

  if ((*rwcal)->cloud)
    g_object_unref((*rwcal)->cloud);

  if ((*rwcal)->mip)
    rw_vx_modinst_close((*rwcal)->mip);

  if ((*rwcal)->framework)
    rw_vx_framework_free((*rwcal)->framework);

  free(*rwcal);
  *rwcal = NULL;

  return;
}

rwcal_closure_ptr_t rwcal_closure_alloc(
    rwcal_module_ptr_t rwcal,
    rw_status_t (*callback)(rwcal_module_ptr_t, void *, int),
    void * user_data)
{
  rwcal_closure_ptr_t closure = NULL;

  closure = g_object_new(RW_CAL_TYPE_CLOSURE, NULL);
  if (!closure)
    goto done;

  closure->m_rwcal = (void *)rwcal;
  closure->m_callback = (RwCalrwcal_callback)callback;
  closure->m_user_data = user_data;

done:
  return closure;
}

void rwcal_closure_free(rwcal_closure_ptr_t * closure)
{
  g_object_unref(*closure);
  *closure = NULL;

  return;
}

rwcal_zk_server_port_detail_ptr_t rwcal_zk_server_port_detail_alloc(
    char *zk_server_addr,
    int zk_client_port,
    int zk_server_id,
    bool zk_server_start,
    bool zk_client_connect)
{
  rwcal_zk_server_port_detail_ptr_t zk_server_port_detail = NULL;

  zk_server_port_detail = g_object_new(RW_CAL_TYPE_ZK_SERVER_PORT_DETAIL, NULL);
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

void rwcal_zk_server_port_detail_free(rwcal_zk_server_port_detail_ptr_t * zk_server_port_detail)
{
  g_object_unref(*zk_server_port_detail);
  *zk_server_port_detail = NULL;

  return;
}

