/*
/   Copyright 2016 RIFT.IO Inc
/
/   Licensed under the Apache License, Version 2.0 (the "License");
/   you may not use this file except in compliance with the License.
/   You may obtain a copy of the License at
/
/       http://www.apache.org/licenses/LICENSE-2.0
/
/   Unless required by applicable law or agreed to in writing, software
/   distributed under the License is distributed on an "AS IS" BASIS,
/   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/   See the License for the specific language governing permissions and
/   limitations under the License.
/*/
/*!
 * @file rwdts_kv_redis_interface.h
 * @brief Top level RWDTS KV LIGHT API header
 * @author Prashanth Bhaskar(Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 */
#ifndef __RWDTS_KV_REDIS_INTERFACE_H
#define __RWDTS_KV_REDIS_INTERFACE_H

#include "rwdts_kv_light_common.h"
#include "rwdts_redis.h"

__BEGIN_DECLS

typedef struct {
  void *userdata;
  rwdts_kv_handle_t *handle;
  rwsched_instance_ptr_t sched_instance;
  rwsched_tasklet_ptr_t tasklet_info;
  char * hostname;
  uint16_t port;
  uint16_t num_retries;
  rwsched_dispatch_source_t retry_timer;
} rwdts_kv_db_init_info;

rw_status_t rwdts_kv_redis_connect_api(rwdts_kv_handle_t *handle,
                                       rwsched_instance_ptr_t sched_instance,
                                       rwsched_tasklet_ptr_t tasklet_info,
                                       char * hostname, uint16_t port,
                                       const char *file_name,
                                       const char *program_name,
                                       void *callbk_fn, void * callback_data);

rw_status_t rwdts_kv_redis_set_api(void *instance, int db_num, char *file_name, char *key,
                                   int key_len, char *val, int val_len,
                                   void *callbkfn, void *callbk_data);

void rwdts_kv_redis_get_api(rwdts_kv_handle_t *handle, int db_num, char *key,
                            int key_len, void *callbkfn, void *callbk_data);

rw_status_t 
rwdts_kv_redis_del_api(void *instance, int db_num, char *file_name, char *key,
                       int key_len, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_disc_api(void *instance);

void rwdts_kv_redis_set_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                 char *shard_num, char *key, int key_len,
                                 int serial_num, char *val, int val_len,
                                 void *callbkfn, void *callbk_data);

void rwdts_kv_redis_get_hash_api(rwdts_kv_handle_t *handle, int db_num, char *key,
                                 int key_len, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_del_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                 char *shard_num, char *key, int key_len,
                                 int serial_num, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_update_hash_api(void *instance, int db_num, char *shard_num,
                                    char *key, int key_len, int serial_num,
                                    char *val, int val_len,
                                    void *callbkfn, void *callbk_data);

void rwdts_kv_redis_load_scripts_api(void *instance, const char *filename,
                                     void *callbkfn, void *callbk_data);

void rwdts_kv_redis_hash_exists_api(rwdts_kv_handle_t *handle, int db_num, char *key,
                                    int key_len, void *callbkfn,
                                    void *callbk_data);

void rwdts_kv_redis_get_next_hash_api(rwdts_kv_table_handle_t *tab_handle,
                                      char *shard_num, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_delete_tab_api(void *instance, int db_num,
                                   void *callbkfn, void *callbk_data);

void rwdts_kv_redis_get_hash_sernum_api(rwdts_kv_handle_t *handle, int db_num, char *key,
                                        int key_len, void *callbkfn,
                                        void *callbk_data);

void rwdts_kv_redis_get_all_api(rwdts_kv_handle_t *handle, int db_num, char *file_name,
                                void *callbkfn, void *callbk_data);

void rwdts_kv_redis_del_shard_entries_api(rwdts_kv_handle_t *handle, int db_num,
                                          char *shard_num, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_set_pend_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                      char *shard_num, char *key, int key_len,
                                      int serial_num, char *val, int val_len,
                                      void *callbkfn, void *callbk_data);

void rwdts_kv_redis_set_pend_commit_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                             char *shard_num, char *key, int key_len,
                                             int serial_num, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_set_pend_abort_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                            char *shard_num, char *key, int key_len,
                                            int serial_num, void *callbkfn, void *callbk_data);

void rwdts_kv_redis_del_pend_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                      char *key, int key_len, int serial_num,
                                      void *callbkfn, void *callbk_data);

void rwdts_kv_redis_del_pend_commit_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                             char *key, int key_len, int serial_num,
                                             void *callbkfn, void *callbk_data);

void rwdts_kv_redis_del_pend_abort_hash_api(rwdts_kv_handle_t *handle, int db_num,
                                            char *key, int key_len, int serial_num,
                                            void *callbkfn, void *callbk_data);

rwdts_reply_status rwdts_kv_redis_get_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_set_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_del_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_getall_cbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_update_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_run_script_cbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_get_hash_len_cbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_hash_exists_cbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_load_set_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata);

rwdts_reply_status rwdts_kv_redis_load_get_next_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata);

rwdts_reply_status rwdts_kv_redis_get_next_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                     void *userdata);

rwdts_reply_status rwdts_kv_redis_load_del_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata);

rwdts_reply_status rwdts_kv_redis_load_get_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                            void *userdata);

rwdts_reply_status rwdts_kv_redis_load_del_shard_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                  void *userdata);

rwdts_reply_status rwdts_kv_redis_get_sernum_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                       void *userdata);

rwdts_reply_status rwdts_kv_redis_tab_del_callbk_fn(rwdts_redis_msg_handle *redis_handle, void *userdata);

rwdts_reply_status rwdts_kv_redis_del_shard_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                      void *userdata);

rwdts_reply_status rwdts_kv_redis_load_set_pend_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata);

rwdts_reply_status rwdts_kv_redis_load_set_pend_commit_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                        void *userdata);

rwdts_reply_status rwdts_kv_redis_load_set_pend_abort_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                       void *userdata);

rwdts_reply_status rwdts_kv_redis_load_del_pend_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                 void *userdata);

rwdts_reply_status rwdts_kv_redis_load_del_pend_commit_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                        void *userdata);

rwdts_reply_status rwdts_kv_redis_load_del_pend_abort_script_callbk_fn(rwdts_redis_msg_handle *redis_handle,
                                                                       void *userdata);

void rwdts_kv_redis_init_cback_fn(rwdts_redis_db_init_info *db_info);

rw_status_t rwdts_kv_redis_open_api(rwdts_kv_handle_t *handle,
                                    rwsched_instance_ptr_t sched_instance,
                                    rwsched_tasklet_ptr_t tasklet_info,
                                    const char *file_name,
                                    const char *program_name,
                                    FILE *error_file_pointer);

rw_status_t 
rwdts_kv_redis_set_shash_api(rwdts_kv_handle_t *handle, int db_num,
                             char *shard_num, char *key, int key_len,
                             char *val, int val_len,
                             void *callbkfn, void *callbk_data);

rw_status_t
rwdts_kv_redis_del_shash_api(rwdts_kv_handle_t *handle,
                             int db_num, char *file_name_shard,
                             char *key, int key_len,
                             void *callbkfn, void *callbk_data);

rw_status_t 
rwdts_kv_redis_get_shash_all_api(rwdts_kv_handle_t *handle, int db_num, char *file_name,
                                 void *callbkfn, void *callbk_data);

void rwdts_redis_wrapper_master_to_slave(void *handle, char *hostname,
                                         uint16_t port, void* callback,
                                         void *userdata);

void rwdts_redis_wrapper_slave_to_master(void *handle, void *callback,
                                         void *userdata);
__END_DECLS

#endif /*__RWDTS_KV_REDIS_INTERFACE_H */
