
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_kv_bkd_interface.c
 * @author Prashanth Bhaskar (Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 * @brief RWDTS KV Light API implementation
 */

#include "db.h"
#include "rwdts_kv_light_common.h"
#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>

rw_status_t 
rwdts_kv_bkd_connect_api(rwdts_kv_handle_t *handle,
                         rwsched_instance_ptr_t sched_instance,
                         rwsched_tasklet_ptr_t tasklet_info,
                         const char *file_name, 
                         const char *program_name,
                         char *hostname, uint16_t port, void *callbkfn, void *callbk_data)
{
  DB *dbp;
  int db_ret;
  /* Create DB */
  if ((db_ret = db_create(&dbp, NULL, 0)) != 0) {
    return RW_STATUS_FAILURE;
  }

  handle->kv_conn_instance = (void *)dbp;

  /* Relate the DB with the given file (location/name) */
  if ((db_ret = dbp->open(dbp, NULL, file_name,  NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
    dbp->err(dbp, db_ret, "file_name");
    handle->kv_conn_instance = NULL;
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

static DB*
rwdts_kv_bkd_get_db_handle(char *file_name)
{
  DB *dbp;
  int db_ret;
  /* Create DB */
  if ((db_ret = db_create(&dbp, NULL, 0)) != 0) {
    return NULL;
  }



  if ((db_ret = dbp->open(dbp, NULL, file_name,  NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
    dbp->err(dbp, db_ret, "file_name");
    return NULL;
  }

  return dbp;
}

static void
rwdts_kv_bkd_close_db(DB *dbp)
{
  RW_ASSERT(dbp);

  dbp->close(dbp, 0);

  return;
}

rw_status_t
rwdts_kv_bkd_set_api(rwdts_kv_handle_t *handle, int db_num, char *file_name, 
                     char *key, int key_len, char *val, int val_len,
                     void *callbkfn, void *callbk_data)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)handle->kv_conn_instance;
  DBT db_key, db_value;

  if (!dbp) {
    dbp = rwdts_kv_bkd_get_db_handle(file_name);
  }

  if (!dbp) {
    return RW_STATUS_FAILURE;
  }

  memset(&db_key, 0, sizeof(DBT));
  memset(&db_value, 0, sizeof(DBT));
  db_key.data = key;
  db_key.size = key_len;
  db_value.data = val;
  db_value.size = val_len;
 
  db_ret = dbp->put(dbp, NULL, &db_key, &db_value, 0);
  switch(db_ret) {
    case 0:
      rwdts_kv_bkd_close_db(dbp);
      return RW_STATUS_SUCCESS;
    default:
      break;
  }
  rwdts_kv_bkd_close_db(dbp);
  return RW_STATUS_FAILURE;
}

rw_status_t
rwdts_kv_bkd_get_api(void *instance, char *file_name, char *key, int key_len, 
                     char **val, int *val_len)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)instance;

  DBT db_key, db_value;

  if (!dbp) {
    dbp = rwdts_kv_bkd_get_db_handle(file_name);
  }

  if (!dbp) {
    return RW_STATUS_FAILURE;
  }

  memset(&db_key, 0, sizeof(DBT));
  memset(&db_value, 0, sizeof(DBT));
  db_key.data = key;
  db_key.size = key_len;

  db_ret = dbp->get(dbp, NULL, &db_key, &db_value, 0);

  switch(db_ret) {
    case 0:
      {
      char *out_data = RW_MALLOC0(db_value.size);
      memcpy(out_data, db_value.data, db_value.size);
      *val = out_data;
      *val_len = db_value.size;
      rwdts_kv_bkd_close_db(dbp);
      return RW_STATUS_SUCCESS;
      }
    default:
      break;
  } 

  rwdts_kv_bkd_close_db(dbp);
  return RW_STATUS_FAILURE;
}

rw_status_t
rwdts_kv_bkd_del_api(rwdts_kv_handle_t *handle, int db_num, char *file_name, char *key, int key_len,
                     void *callbkfn, void *callbk_data)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)handle->kv_conn_instance;

  DBT db_key;

  if (!dbp) {
    dbp = rwdts_kv_bkd_get_db_handle(file_name);
  }
  
  if (!dbp) { 
    return RW_STATUS_FAILURE;
  }

  memset(&db_key, 0, sizeof(DBT));
  db_key.data = key;
  db_key.size = key_len;

  db_ret = dbp->del(dbp, NULL, &db_key, 0);

  switch(db_ret) {
    case 0:
      rwdts_kv_bkd_close_db(dbp);
      return RW_STATUS_SUCCESS;
    default:
      break;
  }
  rwdts_kv_bkd_close_db(dbp);
  return RW_STATUS_FAILURE;
}

void *
rwdts_kv_get_bkd_cursor(void *instance, char *file_name)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)instance;

  DBC *dbcp;

  if (!dbp) {
    dbp = rwdts_kv_bkd_get_db_handle(file_name);
  }
 
  if (!dbp) {
    return NULL;
  }

  if ((db_ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
    dbp->err(dbp, db_ret, "DB->cursor");
    return NULL;
  }

  return (void *)dbcp;
}

rw_status_t
rwdts_kv_get_next_bkd_api(void *instance, void *cursor, char **key, int *key_len,
                          char **val, int *val_len, void **out_cursor)
{
  int db_ret;
  DBT db_key, data;

  if (!cursor) {
    return RW_STATUS_FAILURE;
  }

  DBC *dbcp = (DBC *)cursor;

  memset(&db_key, 0, sizeof(db_key));
  memset(&data, 0, sizeof(data));

  *key = NULL;
  *key_len = 0;
  *val = NULL;
  *val_len = 0;
  if ((db_ret = dbcp->c_get(dbcp, &db_key, &data,
                            DB_FIRST|DB_NEXT)) != 0) {
    if (db_ret == DB_NOTFOUND) {
      *out_cursor = NULL; 
      dbcp->c_close(dbcp); 
      return RW_STATUS_FAILURE;
    }
  }

  *out_cursor = (void *)dbcp;
  *key = db_key.data;
  *key_len = db_key.size;
  *val = data.data;
  *val_len = data.size;

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_kv_remove_bkd(void *instance, const char *file_name)
{
  DB *dbp, *new_dbp;
  int db_ret;
  dbp = (DB *)instance;

  if (dbp) {
    if ((db_ret = dbp->close(dbp, 0)) != 0) {
      return RW_STATUS_FAILURE;
    }
  }

  if ((db_ret = db_create(&new_dbp, NULL, 0)) != 0) {
    return RW_STATUS_FAILURE;
  }

  if ((db_ret = new_dbp->remove(new_dbp, file_name, NULL, 0)) != 0) {
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_kv_close_bkd(void *instance)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)instance;

  if ((db_ret = dbp->close(dbp, 0)) != 0) {
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_kv_bkd_get_all(rwdts_kv_handle_t *handle, int db_num, char *file_name,
                     void *callbkfn, void *callbk_data)
{
  DB *dbp;
  int db_ret;
  dbp = (DB *)handle->kv_conn_instance;

  DBC *dbcp;
  DBT db_key, data;

  RW_ASSERT(callbkfn);
  rwdts_kv_client_getall_cbk_fn getall_callbkfn = (rwdts_kv_client_getall_cbk_fn)callbkfn;

  char **key = NULL, **val = NULL;
  int *key_len = NULL, *val_len = NULL;
  int count = 0;

  if (dbp) {
    if ((db_ret = dbp->close(dbp, 0)) != 0) {
      return RW_STATUS_FAILURE;
    }
    dbp = NULL;
  }

  if (!dbp) {
    dbp = rwdts_kv_bkd_get_db_handle(file_name);
  }

  if (!dbp) {
    getall_callbkfn(NULL, NULL, NULL, NULL, 0, callbk_data);
    return RW_STATUS_FAILURE;
  }

  if ((db_ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
    dbp->err(dbp, db_ret, "DB->cursor");
    rwdts_kv_bkd_close_db(dbp);
    handle->kv_conn_instance = NULL;
    getall_callbkfn(NULL, NULL, NULL, NULL, 0, callbk_data);
    return RW_STATUS_FAILURE;
  }

  memset(&db_key, 0, sizeof(db_key));
  memset(&data, 0, sizeof(data));

  while((db_ret = dbcp->c_get(dbcp, &db_key, &data, DB_NEXT)) == 0) {

    key = realloc(key, ((count+1)*sizeof(key[0])));
    (key)[count] = RW_MALLOC0(db_key.size);
    memcpy((key)[count], db_key.data, db_key.size);
    key_len = realloc(key_len, ((count+1)*sizeof(int)));
    (key_len)[count] = db_key.size;
    val = realloc(val, ((count+1)*sizeof(val[0])));
    (val)[count] = RW_MALLOC0(data.size);
    memcpy((val)[count], data.data, data.size);
    val_len = realloc(val_len, ((count+1)*sizeof(int)));
    (val_len)[count] = data.size;
    count++;
    memset(&db_key, 0, sizeof(db_key));
    memset(&data, 0, sizeof(data));
  }

  rwdts_kv_bkd_close_db(dbp);
  handle->kv_conn_instance = NULL;
  getall_callbkfn((void **)key, key_len, (void **)val, val_len, count, callbk_data);
  return RW_STATUS_SUCCESS;
}

