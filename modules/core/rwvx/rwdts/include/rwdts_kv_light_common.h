/*
/ (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
/*/
/*!
 * @file rwdts_kv_light_common.h
 * @brief Top level RWDTS KV LIGHT COMMON header
 * @author Prashanth Bhaskar(Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 */
#ifndef __RWDTS_KV_LIGHT_COMMON_H
#define __RWDTS_KV_LIGHT_COMMON_H

#include <stdint.h> //uint types
#include <rw_sklist.h>

typedef struct rwdts_kv_handle_s rwdts_kv_handle_t;

struct rwdts_kv_handle_s {
  uint32_t db_type;
  void *kv_conn_instance;
  char file_name_shard[255];
  char set_field_sha[41];
  char next_fields_sha[41];
  char del_field_sha[41];
  char get_field_sha[41];
  char del_shard_sha[41];
  char set_pend_sha[41];
  char set_pend_commit_sha[41];
  char set_pend_abort_sha[41];
  char del_pend_sha[41];
  char del_pend_commit_sha[41];
  char del_pend_abort_sha[41];
  char set_shash_sha[41];
  char del_shash_sha[41];
  char get_shash_all_sha[41];
  int  get_index;
};

typedef struct rwdts_kv_table_scan_t {
  uint32_t scan_list[250];
  int num_nodes;
} rwdts_kv_table_scan_t;

typedef struct rwdts_kv_table_handle_t {
  uint32_t db_num;
  rwdts_kv_handle_t *handle;
  rwdts_kv_table_scan_t *scan_block;
  rw_sklist_element_t element;
} rwdts_kv_table_handle_t;

/*
 * rwdts_kv_light_reply_status
 * REPLY status sent by the KV light callback APIs
 */
typedef enum rwdts_kv_light_reply_status {
  RWDTS_KV_LIGHT_REPLY_INVALID,
  RWDTS_KV_LIGHT_REPLY_DONE, /*done, delete the handle */
  RWDTS_KV_LIGHT_REPLY_KEEP, /*user stowed the handle */
  RWDTS_KV_LIGHT_REPLY_MAX = 0xff
}rwdts_kv_light_reply_status_t ;

typedef enum rwdts_kv_light_set_status {
  RWDTS_KV_LIGHT_SET_SUCCESS = 0,
  RWDTS_KV_LIGHT_SET_FAILURE
} rwdts_kv_light_set_status_t;

typedef enum rwdts_kv_light_del_status {
  RWDTS_KV_LIGHT_DEL_SUCCESS = 0,
  RWDTS_KV_LIGHT_DEL_FAILURE
} rwdts_kv_light_del_status_t;

/*
 * rwdts_kv_client_get_callbk_fn
 * Arguments: void *reply
 *            int reply_len
 *            void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API for GET Key-Value function
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_get_callbk_fn)(void *reply,
                                                                       int reply_len,
                                                                       void *callbk_data);

/*
 * rwdts_kv_client_set_callbk_fn
 * Arguments: rwdts_kv_light_set_status_t status
 *            int serial_num
 *            void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API for SET key-Value function
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_set_callbk_fn)(rwdts_kv_light_set_status_t status,
                                                                       int serial_num,
                                                                       void *callbk_data);

/*
 * rwdts_kv_client_del_callbk_fn
 * Arguments: rwdts_kv_light_del_status_t status
 * void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to delete Key-Value function
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_del_callbk_fn)(rwdts_kv_light_del_status_t status,
                                                                       void *callbk_data);

/*
 * rwdts_kv_client_getall_cbk_fn
 * Arguments: void **key, int *key_len
 *            void **value, int *val_len
 *            int total, void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to Get all Key-Value pair API
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_getall_cbk_fn)(void **key,
                                                                       int *key_len,
                                                                       void **value,
                                                                       int *val_len,
                                                                       int total,
                                                                       void *callbk_data);

/*
 * rwdts_kv_client_exists_cbk_fn
 * Arguments: int exists
 *            void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to key exists API
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_exists_cbk_fn)(int exists,
                                                                       void *callbk_data);

/*
 * rwdts_kv_client_get_nxt_cbk_fn
 * Arguments: void **reply, int *reply_len,
 *            void **key, int *key_len,
 *            int *serial_num,
 *            int total,
 *            void *callbk_data, void *next_block
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to Get-next key-value pairs for a particular shard.
 * The reply and key fields blocks should be freed by the application.
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_get_nxt_cbk_fn)(void **reply, //Value list
                                                                        int *reply_len, //Value-len list
                                                                        void **key, //Key-list
                                                                        int *key_len, //Key-len list
                                                                        int *serial_num, //serial num list
                                                                        int total, void *callbk_data,
                                                                        void *next_block);

/*
 * rwdts_kv_client_get_snum_cbk_fn
 * Arguments: void *reply, int reply_len
 *            int serial_num
 *            void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to Get value and serial-num for a key.
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_get_snum_cbk_fn)(void *reply,
                                                                         int reply_len,
                                                                         int serial_num,
                                                                         void *callbk_data);
/*
 * rwdts_kv_client_conn_callbk_fn
 * Arguments: void *callbk_data
 *
 * Returns: Void
 *
 * Callback API to the KV connection function.
 */
typedef void (*rwdts_kv_client_conn_callbk_fn)(void *callbk_data, rw_status_t status);

/*
 * rwdts_kv_client_del_xact_callbk_fn
 * Arguments: rwdts_kv_light_del_status_t status
 *            int serial_num
 *            void *callbk_data
 *
 * Returns: rwdts_kv_light_reply_status_t
 *
 * Callback API to delete KV transaction function
 */
typedef rwdts_kv_light_reply_status_t (*rwdts_kv_client_del_xact_callbk_fn) (rwdts_kv_light_del_status_t status,
                                                                             int serial_num, void *callbk_data);

typedef struct rwdts_kv_db_trans {
  union {
    rwdts_kv_client_set_callbk_fn set_callbkfn;
    rwdts_kv_client_get_callbk_fn get_callbkfn;
    rwdts_kv_client_del_callbk_fn del_callbkfn;
    rwdts_kv_client_getall_cbk_fn getall_callbkfn;
    rwdts_kv_client_conn_callbk_fn conn_callbkfn;
    rwdts_kv_client_exists_cbk_fn exists_callbkfn;
    rwdts_kv_client_get_nxt_cbk_fn get_nxt_callbkfn;
    rwdts_kv_client_get_snum_cbk_fn get_snum_callbkfn;
    rwdts_kv_client_del_xact_callbk_fn del_xact_callbkfn;
  } u;
  int num_nodes;
  rwdts_kv_handle_t *handle;
  unsigned int search_count;
  unsigned int get_count;
  void *callbk_data;
  rwdts_kv_table_handle_t *tab_handle;
  int *value_len;
  int *key_len;
  int *serial_num;
  char **value;
  char **key;
} rwdts_kv_db_trans_t;

#endif /*__RWDTS_KV_LIGHT_COMMON_H */
