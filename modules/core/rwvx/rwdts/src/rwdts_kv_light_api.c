/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_kv_light_api.c
 * @author Prashanth Bhaskar (Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 * @brief RWDTS KV Light API implementation
 */

#include "rwdts_kv_light_api.h"
#include "rwdts_kv_light_api_gi.h"

rwdts_db_fn_table_t rwdts_fn_table[MAX_DB]  =
{
  { rwdts_kv_redis_connect_api,
    rwdts_kv_redis_set_shash_api,
    rwdts_kv_redis_get_api,
    rwdts_kv_redis_del_shash_api,
    rwdts_kv_redis_disc_api,
    rwdts_kv_redis_set_hash_api,
    rwdts_kv_redis_get_hash_api,
    rwdts_kv_redis_del_hash_api,
    rwdts_kv_redis_hash_exists_api,
    rwdts_kv_redis_get_next_hash_api,
    rwdts_kv_redis_delete_tab_api,
    rwdts_kv_redis_get_hash_sernum_api,
    rwdts_kv_redis_get_shash_all_api,
    rwdts_kv_redis_del_shard_entries_api,
    rwdts_kv_redis_set_pend_hash_api,
    rwdts_kv_redis_set_pend_commit_hash_api,
    rwdts_kv_redis_set_pend_abort_hash_api,
    rwdts_kv_redis_del_pend_hash_api,
    rwdts_kv_redis_del_pend_commit_hash_api,
    rwdts_kv_redis_del_pend_abort_hash_api,
    NULL,
    NULL
  },
  {
    rwdts_kv_bkd_connect_api,
    rwdts_kv_bkd_set_api,
    NULL,
    rwdts_kv_bkd_del_api,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    rwdts_kv_bkd_get_all,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    rwdts_kv_remove_bkd,
    rwdts_kv_close_bkd
  }
};

rwdts_kv_handle_t *
rwdts_kv_allocate_handle(rwdts_avail_db_t db)
{
  rwdts_kv_handle_t *handle;
  handle =  (rwdts_kv_handle_t *)malloc(sizeof(rwdts_kv_handle_t));
  handle->db_type = db;
  return handle;
}

rw_status_t
rwdts_kv_handle_db_connect(rwdts_kv_handle_t *handle,
                          rwsched_instance_t *sched,
                          rwsched_tasklet_t *tasklet,
                          char *uri, const char *file_name,
                          const char *program_name,
                          void *callbkfn, void *callbkdata)
{
  char local_uri[256];
  int port = 0;
  char *ip_address = NULL;

  if (handle->db_type != BK_DB) {
    strncpy(local_uri, uri, 256);
    char *local_uri_ptr = &local_uri[0];
    ip_address = strsep(&local_uri_ptr, ":");
    port = atoi(local_uri_ptr);
  } 

  if (file_name && (file_name != handle->file_name_shard)) {
    strncpy(handle->file_name_shard, file_name, 255);
  }

  if (rwdts_fn_table[handle->db_type].rwdts_db_init_api) {
    return rwdts_fn_table[handle->db_type].rwdts_db_init_api(handle, sched,
                                                             tasklet, ip_address,
                                                             (uint16_t)port, file_name,
                                                             program_name, callbkfn,
                                                             callbkdata);
  } else {
    return RW_STATUS_FAILURE;
  }
}

rw_status_t
rwdts_kv_handle_add_keyval(rwdts_kv_handle_t *handle, int db_num, void *key,
                          int key_len, void *val, int val_len, void *callbkfn,
                          void *callbk_data)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  if (rwdts_fn_table[handle->db_type].rwdts_db_set_api) {
    rs = rwdts_fn_table[handle->db_type].rwdts_db_set_api(handle,
                                                          db_num, handle->file_name_shard, (char *)key, 
                                                          key_len, (char *)val, val_len,
                                                          callbkfn, callbk_data);
    if (handle->db_type == BK_DB) {
      handle->kv_conn_instance = NULL;
    }
  }
  return rs;
}

void
rwdts_kv_light_get_val_from_key(rwdts_kv_handle_t *handle, int db_num, void *key,
                                int key_len, void *callbkfn, void *callbk_data)
{

  if (rwdts_fn_table[handle->db_type].rwdts_db_get_api) {
    rwdts_fn_table[handle->db_type].rwdts_db_get_api(handle,
                                                     db_num, (char *)key, key_len,
                                                     callbkfn, callbk_data);
  }
  return;
}

rw_status_t
rwdts_kv_handle_del_keyval(rwdts_kv_handle_t *handle, int db_num, void *key,
                          int key_len, void *callbkfn, void *callbk_data)
{
  rw_status_t rs = RW_STATUS_SUCCESS;
  if (rwdts_fn_table[handle->db_type].rwdts_db_del_api) {
    rs = rwdts_fn_table[handle->db_type].rwdts_db_del_api(handle,
                                                          db_num, handle->file_name_shard, (char *)key, 
                                                          key_len, callbkfn, callbk_data);
    if (handle->db_type == BK_DB) {
      handle->kv_conn_instance = NULL;
    }
  }
  return rs;
}

rwdts_kv_table_handle_t *rwdts_kv_light_register_table(rwdts_kv_handle_t *handle,
                                                       int db_num)
{
  rwdts_kv_table_handle_t *tab_handle;

  tab_handle = malloc(sizeof(rwdts_kv_table_handle_t));
  tab_handle->db_num = db_num;

  tab_handle->handle = handle;

  tab_handle->scan_block = NULL;

  return tab_handle;
}

void rwdts_kv_light_deregister_table(rwdts_kv_table_handle_t *tab_handle)
{
  if (tab_handle->scan_block) {
    free(tab_handle->scan_block);
  }
  free(tab_handle);
  return;
}

void rwdts_kv_light_table_insert(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                 char *shard_num, void *key, int key_len, void *val,
                                 int val_len, void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_api(tab_handle->handle,
                                                                      tab_handle->db_num,
                                                                      shard_num, key,
                                                                      key_len, serial_num,
                                                                      val, val_len,
                                                                      callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_get_by_key(rwdts_kv_table_handle_t *tab_handle,
                                     void *key, int key_len, void *callbkfn,
                                     void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_get_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_get_api(tab_handle->handle,
                                                                      tab_handle->db_num,
                                                                      key, key_len,
                                                                      callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_delete_key(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                     char *shard_num, void *key, int key_len,
                                     void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_api(tab_handle->handle,
                                                                      tab_handle->db_num, shard_num,
                                                                      key, key_len, serial_num,
                                                                      callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_tab_field_exists(rwdts_kv_table_handle_t *tab_handle,
                                     void *key, int key_len, void *callbkfn,
                                     void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_exists_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_exists_api(tab_handle->handle,
                                                                         tab_handle->db_num,
                                                                         key, key_len,
                                                                         callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_get_next_fields(rwdts_kv_table_handle_t *tab_handle,
                                    char *shard_num, void *callbkfn,
                                    void *callbk_data, void *next_block)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_get_next_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_get_next_api(tab_handle,
                                                                      shard_num, callbkfn,
                                                                      callbk_data);
  }
  return;
}

void rwdts_kv_light_del_table(rwdts_kv_table_handle_t *tab_handle,
                              void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_table_del_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_table_del_api(tab_handle->handle->kv_conn_instance,
                                                                       tab_handle->db_num,
                                                                       callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_get_sernum_by_key(rwdts_kv_table_handle_t *tab_handle,
                                            void *key, int key_len, void *callbkfn,
                                            void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_get_sernum_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_get_sernum_api(tab_handle->handle,
                                                                             tab_handle->db_num,
                                                                             key, key_len,
                                                                             callbkfn, callbk_data);
  }
  return;
}

rw_status_t rwdts_kv_handle_get_all(rwdts_kv_handle_t *handle, int db_num,
                                    void *callbkfn, void *callbk_data)
{
  rw_status_t rs = RW_STATUS_SUCCESS; 
  if (rwdts_fn_table[handle->db_type].rwdts_db_table_get_all_api) {
    rs = rwdts_fn_table[handle->db_type].rwdts_db_table_get_all_api(handle,
                                                                    db_num,
                                                                    handle->file_name_shard,
                                                                    callbkfn, callbk_data);
  }
  return rs;
}

void rwdts_kv_light_delete_shard_entries(rwdts_kv_table_handle_t *tab_handle,
                                         char *shard_num, void *callbkfn,
                                         void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_shard_delete_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_shard_delete_api(tab_handle->handle,
                                                                          tab_handle->db_num,
                                                                          shard_num, callbkfn,
                                                                          callbk_data);
  }
  return;
}

void rwdts_kv_light_table_xact_insert(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                      char *shard_num, void *key, int key_len, void *val,
                                      int val_len, void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_api(tab_handle->handle,
                                                                           tab_handle->db_num,
                                                                           shard_num, key,
                                                                           key_len, serial_num,
                                                                           val, val_len,
                                                                           callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_xact_delete(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                      void *key, int key_len, void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_api(tab_handle->handle,
                                                                           tab_handle->db_num,
                                                                           key, key_len,
                                                                           serial_num,
                                                                           callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_api_xact_insert_commit(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                           char *shard_num, void *key, int key_len,
                                           void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_commit_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_commit_api(tab_handle->handle,
                                                                                  tab_handle->db_num,
                                                                                  shard_num,
                                                                                  key, key_len,
                                                                                  serial_num,
                                                                                  callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_xact_delete_commit(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                             void *key, int key_len, void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_commit_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_commit_api(tab_handle->handle,
                                                                                  tab_handle->db_num,
                                                                                  key, key_len,
                                                                                  serial_num,
                                                                                  callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_api_xact_insert_abort(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                          char *shard_num, void *key, int key_len,
                                          void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_abort_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_set_pend_abort_api(tab_handle->handle,
                                                                                 tab_handle->db_num,
                                                                                 shard_num,
                                                                                 key, key_len,
                                                                                 serial_num,
                                                                                 callbkfn, callbk_data);
  }
  return;
}

void rwdts_kv_light_table_xact_delete_abort(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                            void *key, int key_len, void *callbkfn, void *callbk_data)
{
  if (rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_abort_api) {
    rwdts_fn_table[tab_handle->handle->db_type].rwdts_db_hash_del_pend_abort_api(tab_handle->handle,
                                                                                 tab_handle->db_num,
                                                                                 key, key_len,
                                                                                 serial_num,
                                                                                 callbkfn, callbk_data);
  }
  return;
}

rw_status_t
rwdts_kv_handle_file_remove(rwdts_kv_handle_t *handle)
{
  rw_status_t res = RW_STATUS_FAILURE;
  if (rwdts_fn_table[handle->db_type].rwdts_file_db_remove) {
    res = rwdts_fn_table[handle->db_type].rwdts_file_db_remove(handle->kv_conn_instance,
                                                               handle->file_name_shard);
    handle->kv_conn_instance = NULL;
    return res;
  }
  return res;
}

rw_status_t
rwdts_kv_handle_file_close(rwdts_kv_handle_t *handle)
{
  rw_status_t res = RW_STATUS_FAILURE;
  if (handle->kv_conn_instance && rwdts_fn_table[handle->db_type].rwdts_file_db_close) {
    res = rwdts_fn_table[handle->db_type].rwdts_file_db_close(handle->kv_conn_instance);
    handle->kv_conn_instance = NULL;
  }
  return res;
}

static const GEnumValue _rwdts_avail_db_enum_values[] = {
  { REDIS_DB, "REDIS_DB", "REDIS_DB" },
  { BK_DB,    "BK_DB",    "BK_DB"   },
  { 2, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwdts_avail_db_get_type()
 */
GType rwdts_avail_db_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rwdts_avail_db_t",
                                  _rwdts_avail_db_enum_values);

  return type;
}

static rwdts_kv_handle_t *rwdts_kv_handle_ref(rwdts_kv_handle_t *handle)
{

  rwdts_kv_handle_t* dup_handle = RW_MALLOC0(sizeof(rwdts_kv_handle_t));
  memcpy((char *)dup_handle, (char *)handle, sizeof(rwdts_kv_handle_t));
  return handle;
}

static void rwdts_kv_handle_unref(rwdts_kv_handle_t *handle)
{
  RW_FREE(handle);
  return;
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_kv_handle_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_kv_handle_t,
                    rwdts_kv_handle,
                    rwdts_kv_handle_ref,
                    rwdts_kv_handle_unref);
