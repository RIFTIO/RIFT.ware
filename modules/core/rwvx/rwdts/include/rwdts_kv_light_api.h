/*
/ (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
/*/
/*!
 * @file rwdts_kv_light_api.h
 * @brief Top level RWDTS KV LIGHT API header
 * @author Prashanth Bhaskar(Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 */
#ifndef __RWDTS_KV_LIGHT_H
#define __RWDTS_KV_LIGHT_H

#include "rwdts_kv_light_common.h"
#include "rwdts_kv_redis_interface.h"
#include "rwdts_kv_bkd_interface.h"

__BEGIN_DECLS

typedef rw_status_t (*rwdts_db_connect_api_t)(rwdts_kv_handle_t *kv_handle,
                                              rwsched_instance_ptr_t sched_instance,
                                              rwsched_tasklet_ptr_t tasklet_info,
                                              char * hostname, uint16_t port,
                                              const char *file_name,
                                              const char *program_name,
                                              void *callbk_fn, void * userdata);

typedef rw_status_t (*rwdts_db_set_api_t)(rwdts_kv_handle_t *kv_handle, int db_num, char *file_name_shard, char *key,
                                   int key_len, char *val, int val_len,
                                   void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_get_api_t)(void *instance, int db_num, char *key,
                                   int key_len, void *callbkfn,
                                   void *callbk_data);

typedef rw_status_t (*rwdts_db_del_api_t)(rwdts_kv_handle_t *kv_handle, int db_num, char *file_name_shard, char *key,
                                          int key_len, void *callbkfn,
                                          void *callbk_data);

typedef void (*rwdts_db_hash_set_api_t)(rwdts_kv_handle_t *kv_handle, int db_num,
                                        char *shard_num, char *key, int key_len,
                                        int serial_num, char *val, int val_len,
                                        void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_get_api_t)(void *instance, int db_num,
                                        char *key, int key_len,
                                        void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_get_sernum_api_t)(void *instance, int db_num,
                                               char *key, int key_len,
                                               void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_del_api_t)(rwdts_kv_handle_t *kv_handle, int db_num,
                                        char *shard_num, char *key, int key_len,
                                        int serial_num, void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_get_all_api_t)(void *instance, int db_num,
                                            char *hash, void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_get_len_api_t)(void *instance, int db_num, char *hash,
                                            void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_load_scripts_api_t)(void *instance, const char *filename,
                                            void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_run_script_api_t)(void *instance, const char *sha_script,
                                          char *key, int key_len, int index,
                                          int count, void *callbkfn,
                                          void *callbk_data);

typedef void (*rwdts_db_get_next_api_t)(rwdts_kv_table_handle_t *tab_handle,
                                        char *shard_num, void *callbkfn,
                                        void *callbk_data);

typedef void (*rwdts_db_hash_exists_api_t)(void *instance, int db_num, char *key,
                                           int key_len, void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_table_del_api_t)(void *instance, int db_num,
                                        void *callbkfn, void *callbk_data);

typedef rw_status_t (*rwdts_db_table_get_all_api_t)(rwdts_kv_handle_t *kv_handle, int db_num, char *file_name_shard,
                                             void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_shard_delete_api_t)(rwdts_kv_handle_t *kv_handle, int db_num,
                                            char *shard_num,  void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_set_pend_api_t)(rwdts_kv_handle_t *kv_handle, int db_num,
                                             char *shard_num, char *key, int key_len,
                                             int serial_num, void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_hash_del_pend_api_t)(rwdts_kv_handle_t *kv_handle, int db_num,
                                             char *key, int key_len, int serial_num,
                                             void *callbkfn, void *callbk_data);

typedef void (*rwdts_db_disc_api_t) (void *instance);

typedef rw_status_t (*rwdts_file_db_remove_t) (void *instance, const char *file_name);

typedef rw_status_t (*rwdts_file_db_close_t) (void *instance);

typedef struct rwdts_db_fn_table {
  rwdts_db_connect_api_t         rwdts_db_init_api;
  rwdts_db_set_api_t             rwdts_db_set_api;
  rwdts_db_get_api_t             rwdts_db_get_api;
  rwdts_db_del_api_t             rwdts_db_del_api;
  rwdts_db_disc_api_t            rwdts_db_disc_api;
  rwdts_db_hash_set_api_t        rwdts_db_hash_set_api;
  rwdts_db_hash_get_api_t        rwdts_db_hash_get_api;
  rwdts_db_hash_del_api_t        rwdts_db_hash_del_api;
  rwdts_db_hash_exists_api_t     rwdts_db_hash_exists_api;
  rwdts_db_get_next_api_t        rwdts_db_get_next_api;
  rwdts_db_table_del_api_t       rwdts_db_table_del_api;
  rwdts_db_hash_get_sernum_api_t rwdts_db_hash_get_sernum_api;
  rwdts_db_table_get_all_api_t   rwdts_db_table_get_all_api;
  rwdts_db_shard_delete_api_t    rwdts_db_shard_delete_api;
  rwdts_db_hash_set_api_t        rwdts_db_hash_set_pend_api;
  rwdts_db_hash_set_pend_api_t   rwdts_db_hash_set_pend_commit_api;
  rwdts_db_hash_set_pend_api_t   rwdts_db_hash_set_pend_abort_api;
  rwdts_db_hash_del_pend_api_t   rwdts_db_hash_del_pend_api;
  rwdts_db_hash_del_pend_api_t   rwdts_db_hash_del_pend_commit_api;
  rwdts_db_hash_del_pend_api_t   rwdts_db_hash_del_pend_abort_api;
  rwdts_file_db_remove_t         rwdts_file_db_remove;
  rwdts_file_db_close_t          rwdts_file_db_close;
} rwdts_db_fn_table_t;


/*
 * rwdts_kv_light_get_val_from_key
 * Arguments: rwdts_kv_handle_t *handle
 *            int db_num
 *            void *key, int key_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to get Value for Key from the DB. This API does noy support sharding and
 * also does not validate the ownership of the data
 */
void rwdts_kv_light_get_val_from_key(rwdts_kv_handle_t *handle, int db_num,
                                     void *key, int key_len, void *callbkfn,
                                     void *callbk_data);

/*
 * rwdts_kv_light_register_table
 * Arguments: rwdts_kv_handle_t *handle
 *            int db_num
 *
 * Returns: rwdts_kv_table_handle_t*
 *
 * API to get a table handle for a DB number. Table handle should be used for
 * any further operations on that table (or DB identified by DB number)
 */
rwdts_kv_table_handle_t* rwdts_kv_light_register_table(rwdts_kv_handle_t *handle,
                                                       int db_num);

/*
 * rwdts_kv_light_deregister_table
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *
 * Returns: Void
 *
 * API to deregister the table handle. This API should be called when no further
 * operation on the table is planned by the application.
 */
void rwdts_kv_light_deregister_table(rwdts_kv_table_handle_t *tab_handle);

/*
 * rwdts_kv_light_table_insert
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num, char* shard_num
 *            void *key, int key_len
 *            void *val, int val_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to Insert a key-value pair in the table (identified by DB number). This
 * API supports sharding. The serial number received as argument is validated
 * before updating the data.
 */
void rwdts_kv_light_table_insert(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                 char* shard_num, void *key, int key_len, void *val,
                                 int val_len, void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_table_delete_key
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num, char *shard_num
 *            void *key, int key_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to delete the key-value pair in the table (identified by DB number). This
 * API supports sharding. The serial number received as argument is validated
 * before deleting the data.
 */
void rwdts_kv_light_table_delete_key(rwdts_kv_table_handle_t *tab_handle,
                                     int serial_num, char *shard_num, void *key,
                                     int key_len, void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_table_get_by_key
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            void *key, int key_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to Get the value for a key in the table (identified by DB number).
 */
void rwdts_kv_light_table_get_by_key(rwdts_kv_table_handle_t *tab_handle,
                                     void *key, int key_len, void *callbkfn,
                                     void *callbk_data);

/*
 * rwdts_kv_light_table_get_sernum_by_key
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            void *key, int key_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to provide value and valid serial-number for a key in a DB (identified by
 * DB number).
 */
void rwdts_kv_light_table_get_sernum_by_key(rwdts_kv_table_handle_t *tab_handle,
                                            void *key, int key_len, void *callbkfn,
                                            void *callbk_data);

/*
 * rwdts_kv_light_tab_field_exists
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            void *key, int key_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to validate if a particular Key exists in a DB (identified by DB number)
 */
void rwdts_kv_light_tab_field_exists(rwdts_kv_table_handle_t *tab_handle,
                                     void *key, int key_len,
                                     void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_get_next_fields
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            char *shard_num
 *            void *callbkfn, void *callbk_data
 *            void *next_block
 *
 * Returns: Void
 *
 * API to get the next fields for a particular shard number. The next_block
 * should be sent as NULL at the begining of the fetch operation. In the
 * response if a non-NULL next_block address is received, it has to be sent as
 * arguments for the next fetch operation to get the next set of key-value pairs
 * belonging to a particular shard.
 */
void rwdts_kv_light_get_next_fields(rwdts_kv_table_handle_t *tab_handle,
                                    char *shard_num, void *callbkfn,
                                    void *callbk_data, void *next_block);

/*
 * rwdts_kv_light_del_table
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            void *callbkfn
 *            void *callbk_data
 *
 * Returns: Void
 *
 * API to flush the entire DB (identified by DB number). This API should be
 * called only when in need. Currently no safety (or ownership check) is in
 * place while performing this operation.
 */
void rwdts_kv_light_del_table(rwdts_kv_table_handle_t *tab_handle,
                              void *callbkfn,
                              void *callbk_data);

/*
 * rwdts_kv_light_tab_get_all
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            void *callbkfn, void *callbk_data
 *
 * Returns: rw_status_t
 *
 * API to get all the key-value pairs from a DB (identified by DB number).
 */
rw_status_t rwdts_kv_handle_get_all(rwdts_kv_handle_t *handle, int db_num,
                                    void *callbkfn,
                                    void *callbk_data);

/*
 * rwdts_kv_light_delete_shard_entries
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            char *shard_num
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to delete all the key-value pairs related to a shard number.
 */
void rwdts_kv_light_delete_shard_entries(rwdts_kv_table_handle_t *tab_handle,
                                         char *shard_num, void *callbkfn,
                                         void *callbk_data);

/*
 * rwdts_kv_light_table_xact_insert
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num, char *shard_num,
 *            void *key, int key_len,
 *            void *val, int val_len
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to insert a KV pair to the DB as part of DTS transaction.
 */
void rwdts_kv_light_table_xact_insert(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                      char *shard_num, void *key, int key_len, void *val,
                                      int val_len, void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_table_xact_delete
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num,
 *            void *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to delete a KV pair to the DB as part of DTS transaction.
 */

void rwdts_kv_light_table_xact_delete(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                      void *key, int key_len, void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_api_xact_insert_commit
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num, char *shard_num,
 *            void *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to commit the previously pending insert transaction as part of DTS transaction.
 */
void rwdts_kv_light_api_xact_insert_commit(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                           char *shard_num, void *key, int key_len,
                                           void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_table_xact_delete_commit
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num,
 *            void *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to commit the previously pending delete transaction as part of DTS transaction.
 */
void rwdts_kv_light_table_xact_delete_commit(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                             void *key, int key_len, void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_api_xact_insert_abort
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num, char *shard_num,
 *            void *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to abort the previously pending insert transaction as part of DTS transaction.
 */
void rwdts_kv_light_api_xact_insert_abort(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                          char *shard_num, void *key, int key_len,
                                          void *callbkfn, void *callbk_data);

/*
 * rwdts_kv_light_table_xact_delete_abort
 * Arguments: rwdts_kv_table_handle_t *tab_handle
 *            int serial_num,
 *            void *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: Void
 *
 * API to abort the previously pending delete transaction as part of DTS transaction.
 */
void rwdts_kv_light_table_xact_delete_abort(rwdts_kv_table_handle_t *tab_handle, int serial_num,
                                            void *key, int key_len, void *callbkfn, void *callbk_data);


__END_DECLS

#endif /*__RWDTS_KV_LIGHT_H */
