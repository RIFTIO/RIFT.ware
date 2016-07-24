
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwdts_kv_redis_interface.c
 * @author Prashanth Bhaskar (Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 * @brief RWDTS KV Light API Redis interface
 */

#include "rwtasklet.h"
#include "rwdts_kv_redis_interface.h"

#define MAX_DB_RETRIES 5
#define REDIS_RETRY_TIME 2

static void
rwdts_kv_redis_start_conn_timer(rwdts_kv_db_init_info *init_info);

static void
rwdts_kv_redis_init_retry(void* ud);

rw_status_t rwdts_kv_redis_connect_api(rwdts_kv_handle_t *handle,
                                       rwsched_instance_ptr_t sched_instance,
                                       rwsched_tasklet_ptr_t tasklet_info,
                                       char * hostname, uint16_t port,
                                       const char *file_name,
                                       const char *program_name,
                                       void *callbkfn, void * callbk_data)
{
  rw_status_t status = RW_STATUS_FAILURE;
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.conn_callbkfn = (rwdts_kv_client_conn_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;
  trans_block->handle = handle;

  rwdts_kv_db_init_info *init_info = RW_MALLOC0(sizeof(rwdts_kv_db_init_info));
  RW_ASSERT(init_info);
  init_info->handle = handle;
  init_info->userdata = trans_block;
  init_info->sched_instance = sched_instance;
  init_info->tasklet_info = tasklet_info;
  init_info->hostname = RW_STRDUP(hostname);
  init_info->port = port;

  status = rwdts_redis_instance_init(sched_instance, tasklet_info, hostname,
                                     (uint16_t)port, rwdts_kv_redis_init_cback_fn,
                                     (void *)init_info);

  if (status != RW_STATUS_SUCCESS) {
    rwdts_kv_redis_start_conn_timer(init_info);
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_kv_redis_set_api(void *instance, int db_num, char *file_name, 
                       char *key, int key_len,
                       char *val, int val_len, void *callbkfn,
                       void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_set_key_value((rwdts_redis_instance_t *)instance,
                            db_num, key, key_len, val, val_len,
                            rwdts_kv_redis_set_callbk_fn, (void *)trans_block);
  return RW_STATUS_SUCCESS;
}

void
rwdts_kv_redis_get_api(void *instance, int db_num, char *key, int key_len,
                       void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.get_callbkfn = (rwdts_kv_client_get_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_get_key_value((rwdts_redis_instance_t *)instance, db_num, key,
                            key_len, rwdts_kv_redis_get_callbk_fn,
                            (void *)trans_block);
  return;
}

rw_status_t
rwdts_kv_redis_del_api(void *instance, int db_num, char *file_name, char *key, 
                       int key_len, void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_del_key_value((rwdts_redis_instance_t *)instance, db_num, key,
                            key_len, rwdts_kv_redis_del_callbk_fn,
                            (void *)trans_block);
  return RW_STATUS_SUCCESS;
}

void
rwdts_kv_redis_disc_api(void *instance)
{
  rwdts_redis_instance_deinit((rwdts_redis_instance_t *)instance);
  return;
}

void rwdts_kv_redis_set_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                 char *shard_num, char *key, int key_len,
                                 int serial_num, char *val, int val_len,
                                 void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_set_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                             handle->set_field_sha, key, key_len, db_num, serial_num,
                             shard_num, val, val_len, rwdts_kv_redis_set_callbk_fn,
                             (void *)trans_block);

  return;
}


void rwdts_kv_redis_get_next_hash_api(rwdts_kv_table_handle_t *tab_handle,
                                      char *shard_num, void *callbkfn,
                                      void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.get_nxt_callbkfn = (rwdts_kv_client_get_nxt_cbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;
  trans_block->search_count = 0;
  trans_block->tab_handle = tab_handle;

  if (!tab_handle->scan_block) {
    trans_block->num_nodes = rwdts_redis_run_get_next_script((rwdts_redis_instance_t *)tab_handle->handle->kv_conn_instance,
                                                             tab_handle->handle->next_fields_sha,
                                                             tab_handle->db_num, NULL,
                                                             shard_num, rwdts_kv_redis_get_next_callbk_fn,
                                                             (void *)trans_block);
  } else {
    tab_handle->scan_block->num_nodes = 0;
    trans_block->num_nodes = rwdts_redis_run_get_next_script((rwdts_redis_instance_t *)tab_handle->handle->kv_conn_instance,
                                                             tab_handle->handle->next_fields_sha,
                                                             tab_handle->db_num, tab_handle->scan_block->scan_list,
                                                             shard_num, rwdts_kv_redis_get_next_callbk_fn,
                                                             (void *)trans_block);
  }

  if (!trans_block->num_nodes) {
    if (trans_block->u.get_nxt_callbkfn) {
      trans_block->u.get_nxt_callbkfn(NULL, NULL, NULL, NULL, NULL, 0, callbk_data, NULL);
    }
    RW_FREE(trans_block);
  }

  return;
}

void rwdts_kv_redis_del_shard_entries_api(rwdts_kv_handle_t *handle,
                                          int db_num, char *shard_num,
                                          void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  trans_block->num_nodes = rwdts_redis_run_del_shard_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                            handle->del_shard_sha, db_num, shard_num,
                                                            rwdts_kv_redis_del_shard_callbk_fn, (void *)trans_block);

  if (!trans_block->num_nodes) {
    if (trans_block->u.del_callbkfn) {
      trans_block->u.del_callbkfn(RWDTS_KV_LIGHT_DEL_FAILURE, callbk_data);
    }
    RW_FREE(trans_block);
  }
  return;
}

void rwdts_kv_redis_get_hash_api(void *instance, int db_num, char *key,
                                 int key_len, void *callbkfn,
                                 void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.get_callbkfn = (rwdts_kv_client_get_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_get_hash_key_value((rwdts_redis_instance_t *)instance, db_num,
                                 key, key_len,
                                 rwdts_kv_redis_get_callbk_fn,
                                 (void *)trans_block);

  return;
}

void rwdts_kv_redis_get_hash_sernum_api(void *instance, int db_num, char *key,
                                        int key_len, void *callbkfn,
                                        void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.get_snum_callbkfn = (rwdts_kv_client_get_snum_cbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_get_hash_key_sernum_value((rwdts_redis_instance_t *)instance, db_num,
                                        key, key_len,
                                        rwdts_kv_redis_get_sernum_callbk_fn,
                                        (void *)trans_block);

  return;
}


void rwdts_kv_redis_del_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                 char *shard_num, char *key, int key_len,
                                 int serial_num, void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_del_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                             handle->del_field_sha, key, key_len,
                             db_num, serial_num, rwdts_kv_redis_del_callbk_fn,
                             (void *)trans_block);

  return;
}

void rwdts_kv_redis_get_all_api(rwdts_kv_handle_t *handle, int db_num, char *file_name,
                                void *callbkfn, void *callbk_data)
{
   rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
   trans_block->u.getall_callbkfn = (rwdts_kv_client_getall_cbk_fn)callbkfn;
   trans_block->callbk_data = callbk_data;
   trans_block->get_count = 0;

   trans_block->num_nodes = rwdts_redis_run_get_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                       handle->get_field_sha, db_num,
                                                       rwdts_kv_redis_getall_cbk_fn, (void *)trans_block);
   return;
}

void rwdts_kv_redis_load_get_next_script(rwdts_kv_handle_t *handle, const char *filename,
                                         void *callbkfn, void*callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));

  trans_block->handle = handle;
  trans_block->u.conn_callbkfn = (rwdts_kv_client_conn_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  trans_block->num_nodes = rwdts_redis_load_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                   filename,
                                                   rwdts_kv_redis_load_get_next_script_callbk_fn,
                                                   (void *)trans_block);

  if (!trans_block->num_nodes) {
    RW_FREE(trans_block);
  }
  return;
}

void rwdts_kv_redis_load_script(rwdts_kv_handle_t *handle, const char *filename, 
                                rwdts_trans_callback callback)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));

  trans_block->handle = handle;

  trans_block->num_nodes = rwdts_redis_load_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                   filename,
                                                   callback,
                                                   (void *)trans_block);

  if (!trans_block->num_nodes) {
    RW_FREE(trans_block);
  }
  return;
}

void rwdts_kv_redis_hash_exists_api(void *instance, int db_num, char *key,
                                    int key_len, void *callbkfn,
                                    void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.exists_callbkfn = (rwdts_kv_client_exists_cbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_hash_exist_key_value((rwdts_redis_instance_t *)instance, db_num,
                                   key, key_len, rwdts_kv_redis_hash_exists_cbk_fn,
                                   (void *)trans_block);
  return;
}

void rwdts_kv_redis_delete_tab_api(void *instance, int db_num,
                                   void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  trans_block->num_nodes = rwdts_redis_del_table((rwdts_redis_instance_t *)instance, db_num,
                                                 rwdts_kv_redis_tab_del_callbk_fn, (void *)trans_block);

  if (!trans_block->num_nodes) {
    if (trans_block->u.del_callbkfn) {
      trans_block->u.del_callbkfn(RWDTS_KV_LIGHT_DEL_FAILURE, callbk_data);
    }
    RW_FREE(trans_block);
  }
  return;
}

rwdts_reply_status rwdts_kv_redis_load_set_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->set_field_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->set_field_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_field_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->del_field_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_get_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->get_field_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->get_field_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_shard_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                  void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_shard_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->del_shard_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_get_next_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->next_fields_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->next_fields_sha[len] = '\0';
      }
    }
    if (trans_block->u.conn_callbkfn) {
      trans_block->u.conn_callbkfn(trans_block->callbk_data, RW_STATUS_SUCCESS);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_set_pend_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->set_pend_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->set_pend_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_set_pend_commit_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                        void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->set_pend_commit_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->set_pend_commit_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_set_pend_abort_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                       void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->set_pend_abort_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->set_pend_abort_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_pend_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_pend_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->del_pend_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_pend_commit_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                        void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_pend_commit_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->del_pend_commit_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_pend_abort_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                       void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;

  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_pend_abort_sha[0],
               redis_handle->reply->str,
               len);
        trans_block->handle->del_pend_abort_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_set_shash_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                           void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->set_shash_sha[0],
               redis_handle->reply->str, len);
        trans_block->handle->set_shash_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_del_shash_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                           void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->del_shash_sha[0],
               redis_handle->reply->str, len);
        trans_block->handle->del_shash_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_load_get_shash_all_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                               void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  trans_block->num_nodes --;
  if (!trans_block->num_nodes) {
    if (NULL != RWDTS_VIEW_REPLY(redis_handle)) {
      int len = RWDTS_VIEW_REPLY_LEN(redis_handle);
      if (len) {
        memcpy((char *)&trans_block->handle->get_shash_all_sha[0],
               redis_handle->reply->str, len);
        trans_block->handle->get_shash_all_sha[len] = '\0';
      }
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_get_next_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                     void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  int node = 0, next_scan = 0;
  int i;
  size_t rsz;

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    if (redis_handle->reply->type == REDIS_REPLY_ARRAY) {
      if (redis_handle->reply->element[0] != NULL) {
        node = redis_handle->reply->element[0]->integer;
      }
      if (redis_handle->reply->element[4] != NULL) {
        next_scan = redis_handle->reply->element[4]->integer;
      }
      if ((redis_handle->reply->element[1] != NULL) &&
          (redis_handle->reply->element[2] != NULL) &&
          (redis_handle->reply->element[3] != NULL)) {
        for (i = 0; i < redis_handle->reply->element[1]->elements; i++) {
          if ((redis_handle->reply->element[1]->element[i] != NULL) &&
              (redis_handle->reply->element[2]->element[i] != NULL) &&
              (redis_handle->reply->element[3]->element[i] != NULL)) {
            trans_block->search_count++;
            rsz = (trans_block->search_count * sizeof(trans_block->value[0]));
            trans_block->value = realloc(trans_block->value, rsz);
            trans_block->value[trans_block->search_count-1] = RW_STRDUP(redis_handle->reply->element[1]->element[i]->str);
            rsz = (trans_block->search_count * sizeof(trans_block->key[0]));
            trans_block->key = realloc(trans_block->key, rsz);
            trans_block->key[trans_block->search_count-1] = RW_STRDUP(redis_handle->reply->element[2]->element[i]->str);
            rsz = (trans_block->search_count * sizeof(trans_block->key_len[0]));
            trans_block->key_len = realloc(trans_block->key_len, rsz);
            trans_block->key_len[trans_block->search_count-1] = redis_handle->reply->element[2]->element[i]->len;
            rsz = (trans_block->search_count * sizeof(trans_block->value_len[0]));
            trans_block->value_len = realloc(trans_block->value_len, rsz);
            trans_block->value_len[trans_block->search_count-1] = redis_handle->reply->element[1]->element[i]->len;
            rsz = (trans_block->search_count * sizeof(trans_block->serial_num[0]));
            trans_block->serial_num = realloc(trans_block->serial_num, rsz);
            trans_block->serial_num[trans_block->search_count-1] = redis_handle->reply->element[3]->element[i]->integer;
          }
        }
      }
    }
  }

  if (next_scan != 0) {
    if (!trans_block->tab_handle->scan_block) {
      trans_block->tab_handle->scan_block = RW_MALLOC0(sizeof(rwdts_kv_table_scan_t));
    }
    trans_block->tab_handle->scan_block->scan_list[node] = next_scan;
    trans_block->tab_handle->scan_block->num_nodes++;
  }

  if (!trans_block->num_nodes) {

    if (trans_block->u.get_nxt_callbkfn) {
      if ((trans_block->tab_handle->scan_block) &&
          (!trans_block->tab_handle->scan_block->num_nodes)) {
        RW_FREE(trans_block->tab_handle->scan_block);
        trans_block->tab_handle->scan_block = NULL;
      }
      trans_block->u.get_nxt_callbkfn((void **)trans_block->value, trans_block->value_len, (void **)trans_block->key, trans_block->key_len,
                                      trans_block->serial_num, trans_block->search_count, trans_block->callbk_data, (void *)trans_block->tab_handle->scan_block);
    }
    for (i = 0; i < trans_block->search_count; i++) {
      RW_FREE(trans_block->key[i]);
      RW_FREE(trans_block->value[i]);
    }
    if (trans_block->key) {
      RW_FREE(trans_block->key);
    }
    if (trans_block->value) {
       RW_FREE(trans_block->value);
    }
    if (trans_block->key_len) {
      RW_FREE(trans_block->key_len);
    }
    if (trans_block->value_len) {
      RW_FREE(trans_block->value_len);
    }
    if (trans_block->serial_num) {
      RW_FREE(trans_block->serial_num);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_del_shard_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                      void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_del_status_t kv_status = RWDTS_KV_LIGHT_DEL_FAILURE;


  trans_block->num_nodes--;

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    kv_status = RWDTS_KV_LIGHT_DEL_SUCCESS;
  }

  if (!trans_block->num_nodes) {
    if (trans_block->u.del_callbkfn) {
      trans_block->u.del_callbkfn(kv_status, trans_block->callbk_data);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_get_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                void *userdata)
{
  rwdts_kv_light_reply_status_t status;
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  char *val = NULL;
  int len = 0;

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    if (redis_handle->reply->type == REDIS_REPLY_ARRAY) {
      if (redis_handle->reply->element && redis_handle->reply->element[0] != NULL) {
        if (redis_handle->reply->element[0]->type == REDIS_REPLY_STRING) {
          val = redis_handle->reply->element[0]->str;
          len = redis_handle->reply->element[0]->len;
        }
      }
    }
  }

  if (trans_block->u.get_callbkfn) {
    status = trans_block->u.get_callbkfn(val,
                                         len,
                                         trans_block->callbk_data);
    RW_FREE(trans_block);
    if (status == RWDTS_KV_LIGHT_REPLY_KEEP) {
      return RWDTS_REDIS_REPLY_KEEP;
    } else if (status == RWDTS_KV_LIGHT_REPLY_DONE) {
      return RWDTS_REDIS_REPLY_DONE;
    }
  } else {
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_get_sernum_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                       void *userdata)
{
  rwdts_kv_light_reply_status_t status;
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  int serial_num = 0, val_len = 0;
  char *val = NULL;
  char *serial_shard, *serial;
  char serial_shard_str[20];

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    if (redis_handle->reply->type == REDIS_REPLY_ARRAY) {
      if (redis_handle->reply->elements == 2) {
        val = redis_handle->reply->element[1]->str;
        val_len = redis_handle->reply->element[1]->len;
        strcpy(&serial_shard_str[0], redis_handle->reply->element[0]->str);
        serial_shard = &serial_shard_str[0];
        serial = strsep(&serial_shard, ":");
        serial_num = atoi(serial);
      }
    }
  }

  if (trans_block->u.get_snum_callbkfn) {
    status = trans_block->u.get_snum_callbkfn(val, val_len, (serial_num + 1),
                                              trans_block->callbk_data);
    RW_FREE(trans_block);
    if (status == RWDTS_KV_LIGHT_REPLY_KEEP) {
      return RWDTS_REDIS_REPLY_KEEP;
    } else if (status == RWDTS_KV_LIGHT_REPLY_DONE) {
      return RWDTS_REDIS_REPLY_DONE;
    }
    return RWDTS_REDIS_REPLY_DONE;
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_set_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_set_status_t kv_status = RWDTS_KV_LIGHT_SET_FAILURE;
  int serial_num = 0;

  if (NULL != redis_handle->reply) {
    if (redis_handle->reply->type == 2) {
      if (redis_handle->reply->elements == 2) {
        if (redis_handle->reply->element[0]->integer == 1) {
          kv_status = RWDTS_KV_LIGHT_SET_SUCCESS;
        }
        serial_num = redis_handle->reply->element[1]->integer;
      }
    }
  }

  if (trans_block->u.set_callbkfn) {
    trans_block->u.set_callbkfn(kv_status, serial_num,
                                trans_block->callbk_data);
  }
  RW_FREE(trans_block);
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_del_xact_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                     void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_set_status_t kv_status = RWDTS_KV_LIGHT_SET_FAILURE;
  int serial_num = 0;

  if (NULL != redis_handle->reply) {
    if (redis_handle->reply->type == 2) {
      if (redis_handle->reply->elements == 2) {
        if (redis_handle->reply->element && redis_handle->reply->element[0]) {
          if ((redis_handle->reply->element[0]->type == REDIS_REPLY_INTEGER) &&
              (redis_handle->reply->element[0]->integer == 1)) {
            kv_status = RWDTS_KV_LIGHT_DEL_SUCCESS;
          }
          if (redis_handle->reply->element && redis_handle->reply->element[1]) {
            serial_num = redis_handle->reply->element[1]->integer;
          }
        }
      }
    }
  }

  if (trans_block->u.del_callbkfn) {
    trans_block->u.del_xact_callbkfn(kv_status, serial_num,
                                     trans_block->callbk_data);
  }
  RW_FREE(trans_block);
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_del_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_del_status_t kv_status = RWDTS_KV_LIGHT_DEL_FAILURE;

  if (trans_block->u.del_callbkfn) {
    if (RWDTS_VIEW_REPLY_STATUS(redis_handle) == RW_HANDLE_STATE_REPLY) {
      if ((redis_handle->reply->type == REDIS_REPLY_INTEGER) &&
          (redis_handle->reply->integer)) {
        kv_status = RWDTS_KV_LIGHT_DEL_SUCCESS;
      }
    }
    trans_block->u.del_callbkfn(kv_status, trans_block->callbk_data);
  }
  RW_FREE(trans_block);
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_getall_cbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  int  i;
  size_t rsz;

  trans_block->num_nodes--;

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    if ((redis_handle->reply->element[0] != NULL) &&
        (redis_handle->reply->element[1] != NULL)) {
      for (i=0; i < redis_handle->reply->element[0]->elements; i++) {
        if ((redis_handle->reply->element[0]->element[i] != NULL) &&
            (redis_handle->reply->element[1]->element[i] != NULL)) {

          trans_block->get_count++;
          rsz = (trans_block->get_count * sizeof(trans_block->value[0]));
          trans_block->value = realloc(trans_block->value, rsz);
          trans_block->value[trans_block->get_count-1] = RW_STRDUP(redis_handle->reply->element[1]->element[i]->str);
          rsz = (trans_block->get_count * sizeof(trans_block->key[0]));
          trans_block->key = realloc(trans_block->key, rsz);
          trans_block->key[trans_block->get_count-1] = RW_STRDUP(redis_handle->reply->element[0]->element[i]->str);
          rsz = (trans_block->get_count * sizeof(trans_block->key_len[0]));
          trans_block->key_len = realloc(trans_block->key_len, rsz);
          trans_block->key_len[trans_block->get_count-1] = redis_handle->reply->element[0]->element[i]->len;;
          rsz = (trans_block->get_count * sizeof(trans_block->value_len[0]));
          trans_block->value_len = realloc(trans_block->value_len, rsz);
          trans_block->value_len[trans_block->get_count-1] = redis_handle->reply->element[1]->element[i]->len;;
        }
      }
    }
  }

  if (!trans_block->num_nodes) {
    if (trans_block->u.getall_callbkfn) {
      trans_block->u.getall_callbkfn((void **)trans_block->key, trans_block->key_len,
                                     (void **)trans_block->value, trans_block->value_len,
                                     trans_block->get_count,
                                     trans_block->callbk_data);
    }
    for (i = 0; i < trans_block->search_count; i++) {
      RW_FREE(trans_block->key[i]);
      RW_FREE(trans_block->value[i]);
    }
    if (trans_block->key) {
      RW_FREE(trans_block->key);
    }
    if (trans_block->value) {
       RW_FREE(trans_block->value);
    }
    if (trans_block->key_len) {
      RW_FREE(trans_block->key_len);
    }
    if (trans_block->value_len) {
      RW_FREE(trans_block->value_len);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

static void
rwdts_kv_redis_start_conn_timer(rwdts_kv_db_init_info *init_info)
{
  RW_ASSERT(init_info);
  init_info->retry_timer = rwsched_dispatch_source_create(init_info->tasklet_info,
                                                          RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                                          0, 0, ((struct rwsched_instance_s *)init_info->sched_instance)->main_rwqueue);

  rwsched_dispatch_source_set_timer(init_info->tasklet_info,
                                    init_info->retry_timer,
                                    dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * REDIS_RETRY_TIME),
                                    0, 0);

  rwsched_dispatch_source_set_event_handler_f(init_info->tasklet_info,
                                              init_info->retry_timer,
                                              rwdts_kv_redis_init_retry);

  rwsched_dispatch_set_context(init_info->tasklet_info,
                               init_info->retry_timer,
                               init_info);

  rwsched_dispatch_resume(init_info->tasklet_info,
                          init_info->retry_timer);

  return;
}

static void 
rwdts_kv_redis_init_retry(void* ud)
{
  RW_ASSERT(ud);
  rwdts_kv_db_init_info *init_info = (rwdts_kv_db_init_info *)ud;
  rw_status_t status;

  if (init_info->retry_timer &&
      (rwsched_dispatch_get_context(init_info->tasklet_info, init_info->retry_timer) == init_info)) {
    rwsched_dispatch_source_cancel(init_info->tasklet_info, init_info->retry_timer);
    rwsched_dispatch_release(init_info->tasklet_info, init_info->retry_timer);
    init_info->retry_timer = NULL;
  }

  init_info->num_retries++;
  status = rwdts_redis_instance_init(init_info->sched_instance, init_info->tasklet_info,
                                     init_info->hostname, (uint16_t)init_info->port,
                                     rwdts_kv_redis_init_cback_fn, (void *)init_info);

  if (status != RW_STATUS_SUCCESS) {
    if (init_info->num_retries < MAX_DB_RETRIES) {
      rwdts_kv_redis_start_conn_timer(init_info);
    } else {
      rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)init_info->userdata;
      RW_ASSERT(trans_block);

      if (trans_block->u.conn_callbkfn) {
        trans_block->u.conn_callbkfn(trans_block->callbk_data, RW_STATUS_FAILURE);
      }
      RW_FREE(trans_block);
      if (init_info->hostname) {
        RW_FREE(init_info->hostname);
      }
      RW_FREE(init_info);
    }
  } 

  return;
}

void rwdts_kv_redis_init_cback_fn(rwdts_redis_db_init_info *db_info)
{
  rwdts_kv_db_init_info *init_info = db_info->userdata;
  RW_ASSERT(init_info);
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)init_info->userdata;

  if (RWDTS_REDIS_DB_DOWN == RWDTS_REDIS_DB_STATUS(db_info)) {
    if (init_info->num_retries < MAX_DB_RETRIES) {
      rwdts_kv_redis_start_conn_timer(init_info); 
      return;
    } else {
      if (trans_block->u.conn_callbkfn) {
        trans_block->u.conn_callbkfn(trans_block->callbk_data, RW_STATUS_FAILURE);
      }
      RW_FREE(trans_block);
    }
  } else {
    /*connection is up, stow in connection instance*/
   char *rw_install_dir, *set_file_path, *next_file_path, *del_file_path;
   char *get_file_path, *del_shard_file_path, *set_pend_file_path, *set_pend_cfile_path;
   char *set_pend_afile_path, *del_pend_file_path, *del_pend_cfile_path, *del_pend_afile_path;
   char *set_shash_path, *del_shash_path, *get_shash_all_path;

   rw_install_dir = getenv("RIFT_INSTALL");
   set_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                    rw_install_dir, "set_next.lua");

   next_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                     rw_install_dir, "get_next.lua");

   del_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                    rw_install_dir, "del_scr.lua");

   get_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                    rw_install_dir, "get_all.lua");

   del_shard_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                          rw_install_dir, "del_shard.lua");

   set_pend_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                         rw_install_dir, "set_pend.lua");

   set_pend_cfile_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                          rw_install_dir, "set_pend_commit.lua");

   set_pend_afile_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                          rw_install_dir, "set_pend_abort.lua");

   del_pend_file_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                         rw_install_dir, "del_pend.lua");

   del_pend_cfile_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                          rw_install_dir, "del_pend_commit.lua");

   del_pend_afile_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                          rw_install_dir, "del_pend_abort.lua");

   set_shash_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                     rw_install_dir, "set_shash.lua");

   del_shash_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                     rw_install_dir, "del_shash.lua");

   get_shash_all_path = RW_STRDUP_PRINTF("%s/usr/bin/lua-scripts/%s",
                                         rw_install_dir, "get_shash_all.lua");

   init_info->handle->kv_conn_instance = db_info->instance;
   rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)init_info->userdata;
   rwdts_kv_redis_load_script(trans_block->handle, set_file_path,
                              rwdts_kv_redis_load_set_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_file_path,
                              rwdts_kv_redis_load_del_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, get_file_path,
                              rwdts_kv_redis_load_get_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_shard_file_path,
                              rwdts_kv_redis_load_del_shard_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, set_pend_file_path,
                              rwdts_kv_redis_load_set_pend_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, set_pend_cfile_path,
                              rwdts_kv_redis_load_set_pend_commit_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, set_pend_afile_path,
                              rwdts_kv_redis_load_set_pend_abort_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_pend_file_path,
                              rwdts_kv_redis_load_del_pend_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_pend_cfile_path,
                              rwdts_kv_redis_load_del_pend_commit_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_pend_afile_path,
                              rwdts_kv_redis_load_del_pend_abort_script_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, set_shash_path,
                              rwdts_kv_redis_load_set_shash_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, del_shash_path,
                              rwdts_kv_redis_load_del_shash_callbk_fn);
   rwdts_kv_redis_load_script(trans_block->handle, get_shash_all_path,
                              rwdts_kv_redis_load_get_shash_all_callbk_fn);

   rwdts_kv_redis_load_get_next_script(trans_block->handle, next_file_path,
                                       (void *)trans_block->u.conn_callbkfn,
                                       trans_block->callbk_data);

   RW_FREE(del_file_path);
   RW_FREE(next_file_path);
   RW_FREE(set_file_path);
   RW_FREE(get_file_path);
   RW_FREE(del_shard_file_path);
   RW_FREE(set_pend_file_path);
   RW_FREE(set_pend_cfile_path);
   RW_FREE(set_pend_afile_path);
   RW_FREE(del_pend_file_path);
   RW_FREE(del_pend_cfile_path);
   RW_FREE(del_pend_afile_path);
   RW_FREE(set_shash_path);
   RW_FREE(del_shash_path);
   RW_FREE(get_shash_all_path);
   RW_FREE(trans_block);
  }
  if (init_info->hostname) {
    RW_FREE(init_info->hostname);
  }
  RW_FREE(init_info);
}

rwdts_reply_status rwdts_kv_redis_hash_exists_cbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  int exists = 0;

  if (RWDTS_VIEW_REPLY_STATUS(redis_handle) == RW_HANDLE_STATE_REPLY) {
    if (REDIS_REPLY_INTEGER == redis_handle->reply->type) {
      exists = redis_handle->reply->integer;
    }
  }

  if (trans_block->u.exists_callbkfn) {
    trans_block->u.exists_callbkfn(exists, trans_block->callbk_data);
  }
  RW_FREE(trans_block);
  return RWDTS_REDIS_REPLY_DONE;
}

rwdts_reply_status rwdts_kv_redis_tab_del_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_del_status_t kv_status = RWDTS_KV_LIGHT_DEL_FAILURE;

  trans_block->num_nodes--;
  if (!trans_block->num_nodes) {
    if (trans_block->u.del_callbkfn) {
      if (RWDTS_VIEW_REPLY_STATUS(redis_handle) == RW_HANDLE_STATE_REPLY) {
        if ((redis_handle->reply->type == REDIS_REPLY_STATUS) &&
            (!strcmp(redis_handle->reply->str, "OK"))) {
          kv_status = RWDTS_KV_LIGHT_DEL_SUCCESS;
        }
      }
      trans_block->u.del_callbkfn(kv_status, trans_block->callbk_data);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

void rwdts_kv_redis_set_pend_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                      char *shard_num, char *key, int key_len,
                                      int serial_num, char *val, int val_len,
                                      void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_set_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                  handle->set_pend_sha, key, key_len, db_num, serial_num,
                                  shard_num, val, val_len, rwdts_kv_redis_set_callbk_fn,
                                  (void *)trans_block);


  return;
}

void rwdts_kv_redis_set_pend_commit_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                             char *shard_num, char *key, int key_len,
                                             int serial_num, void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_set_pend_commit_abort_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                               handle->set_pend_commit_sha, key, key_len, db_num,
                                               serial_num, shard_num, rwdts_kv_redis_set_callbk_fn,
                                               (void *)trans_block);

  return;
}

void rwdts_kv_redis_set_pend_abort_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                            char *shard_num, char *key, int key_len,
                                            int serial_num, void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_set_pend_commit_abort_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                               handle->set_pend_abort_sha, key, key_len, db_num,
                                               serial_num, shard_num, rwdts_kv_redis_set_callbk_fn,
                                               (void *)trans_block);

  return;
}

void rwdts_kv_redis_del_pend_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                      char *key, int key_len, int serial_num,
                                      void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_xact_callbkfn = (rwdts_kv_client_del_xact_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_del_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                             handle->del_pend_sha, key, key_len, db_num, serial_num,
                             rwdts_kv_redis_del_xact_callbk_fn, (void *)trans_block);

  return;
}

void rwdts_kv_redis_del_pend_commit_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                             char *key, int key_len, int serial_num,
                                             void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_del_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                             handle->del_pend_commit_sha, key, key_len, db_num, serial_num,
                             rwdts_kv_redis_del_callbk_fn, (void *)trans_block);

  return;
}

void rwdts_kv_redis_del_pend_abort_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                            char *key, int key_len, int serial_num,
                                            void *callbkfn, void *callbk_data)
{
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  rwdts_redis_run_del_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                             handle->del_pend_abort_sha, key, key_len, db_num, serial_num,
                             rwdts_kv_redis_del_callbk_fn, (void *)trans_block);

  return;
}

rwdts_reply_status rwdts_kv_redis_set_shash_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                      void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  rwdts_kv_light_del_status_t kv_status = RWDTS_KV_LIGHT_SET_FAILURE;

  if (trans_block->u.set_callbkfn) {
    if (RWDTS_VIEW_REPLY_STATUS(redis_handle) == RW_HANDLE_STATE_REPLY) {
      if ((redis_handle->reply->type == REDIS_REPLY_INTEGER) &&
          (redis_handle->reply->integer)) {
        kv_status = RWDTS_KV_LIGHT_SET_SUCCESS;
      }
    }
    trans_block->u.set_callbkfn(kv_status, 0, trans_block->callbk_data);
  }
  RW_FREE(trans_block);
  return RWDTS_REDIS_REPLY_DONE;
}

rw_status_t 
rwdts_kv_redis_set_shash_api(rwdts_kv_handle_t *handle, int db_num,
                             char *shard_num, char *key, int key_len,
                             char *val, int val_len,
                             void *callbkfn, void *callbk_data)
{
  rw_status_t status = RW_STATUS_FAILURE;
  uint32_t retry = 0;
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.set_callbkfn = (rwdts_kv_client_set_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  if (!handle->kv_conn_instance) {
    return status;
  }

  while(status == RW_STATUS_FAILURE) {
    status = rwdts_redis_run_set_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                        handle->set_shash_sha, key, key_len, db_num, 0,
                                        shard_num, val, val_len, rwdts_kv_redis_set_shash_callbk_fn,
                                        (void *)trans_block);
    retry++;
    if (retry == MAX_DB_RETRIES) {
      break;
    }
  }

  return status;
}

rw_status_t 
rwdts_kv_redis_del_shash_api(rwdts_kv_handle_t *handle,
                             int db_num, char *file_name_shard,
                             char *key, int key_len,
                             void *callbkfn, void *callbk_data)
{
  uint32_t retry = 0;
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.del_callbkfn = (rwdts_kv_client_del_callbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;

  if (!handle->kv_conn_instance) {
    return RW_STATUS_FAILURE;
  }

  while(!trans_block->num_nodes) {
    trans_block->num_nodes = rwdts_redis_run_del_shash_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                              handle->del_shash_sha, db_num, file_name_shard,
                                                              key, key_len, rwdts_kv_redis_del_shard_callbk_fn,
                                                              (void *)trans_block);
    retry++;
    if (!trans_block->num_nodes && (retry == MAX_DB_RETRIES)) {
      if (trans_block->u.del_callbkfn) {
        trans_block->u.del_callbkfn(RWDTS_KV_LIGHT_DEL_FAILURE, callbk_data);
      }
      RW_FREE(trans_block);
      return RW_STATUS_FAILURE;
    }
  }
  return RW_STATUS_SUCCESS;
}

rwdts_reply_status rwdts_kv_redis_get_shash_all_cbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                       void *userdata)
{
  rwdts_kv_db_trans_t *trans_block = (rwdts_kv_db_trans_t *)userdata;
  int  i;
  size_t rsz;

  trans_block->num_nodes--;

  if (RW_HANDLE_STATE_REPLY == redis_handle->status) {
    if ((redis_handle->reply->element[0] != NULL) &&
        (redis_handle->reply->element[1] != NULL)) {
      for (i=0; i < redis_handle->reply->element[0]->elements; i++) {
        if ((redis_handle->reply->element[0]->element[i] != NULL) &&
            (redis_handle->reply->element[1]->element[i] != NULL)) {

          trans_block->get_count++;
          rsz = (trans_block->get_count * sizeof(trans_block->value[0]));
          trans_block->value = realloc(trans_block->value, rsz);
          trans_block->value[trans_block->get_count-1] = RW_MALLOC0(redis_handle->reply->element[1]->element[i]->len);
          memcpy(trans_block->value[trans_block->get_count-1], redis_handle->reply->element[1]->element[i]->str,
                 redis_handle->reply->element[1]->element[i]->len);

          
          rsz = (trans_block->get_count * sizeof(trans_block->key[0]));
          trans_block->key = realloc(trans_block->key, rsz);
          trans_block->key[trans_block->get_count-1] = RW_MALLOC0(redis_handle->reply->element[0]->element[i]->len);
          memcpy(trans_block->key[trans_block->get_count-1], redis_handle->reply->element[0]->element[i]->str,
                 redis_handle->reply->element[0]->element[i]->len);

          rsz = (trans_block->get_count * sizeof(trans_block->key_len[0]));
          trans_block->key_len = realloc(trans_block->key_len, rsz);
          trans_block->key_len[trans_block->get_count-1] = redis_handle->reply->element[0]->element[i]->len;;
          rsz = (trans_block->get_count * sizeof(trans_block->value_len[0]));
          trans_block->value_len = realloc(trans_block->value_len, rsz);
          trans_block->value_len[trans_block->get_count-1] = redis_handle->reply->element[1]->element[i]->len;;
        }
      }
    }
  }

  if (!trans_block->num_nodes) {
    if (trans_block->u.getall_callbkfn) {
      trans_block->u.getall_callbkfn((void **)trans_block->key, trans_block->key_len,
                                     (void **)trans_block->value, trans_block->value_len,
                                     trans_block->get_count,
                                     trans_block->callbk_data);
    }
    RW_FREE(trans_block);
  }
  return RWDTS_REDIS_REPLY_DONE;
}

rw_status_t rwdts_kv_redis_get_shash_all_api(rwdts_kv_handle_t *handle, int db_num, char *file_name_shard,
                                             void *callbkfn, void *callbk_data)
{
  uint32_t retry = 0;
  rwdts_kv_db_trans_t *trans_block = RW_MALLOC0(sizeof(rwdts_kv_db_trans_t));
  trans_block->u.getall_callbkfn = (rwdts_kv_client_getall_cbk_fn)callbkfn;
  trans_block->callbk_data = callbk_data;
  trans_block->get_count = 0;

  if (!handle->kv_conn_instance) {
    return RW_STATUS_FAILURE;
  }

  while(!trans_block->num_nodes) {
    trans_block->num_nodes = rwdts_redis_run_get_all_shash_script((rwdts_redis_instance_t *)handle->kv_conn_instance,
                                                                  handle->get_shash_all_sha, db_num,
                                                                  file_name_shard, rwdts_kv_redis_get_shash_all_cbk_fn,
                                                                  (void *)trans_block);
    retry++;
    if (!trans_block->num_nodes && (retry == MAX_DB_RETRIES)) {
      if (trans_block->u.getall_callbkfn) {
        trans_block->u.getall_callbkfn(NULL, 0, NULL, 0, 0, trans_block->callbk_data);
      }
      RW_FREE(trans_block);
      return RW_STATUS_FAILURE;
    }
  }
  return RW_STATUS_SUCCESS;
}
