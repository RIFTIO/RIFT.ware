/*
/ (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
/*/
/*!
 * @file rwdts_kv_light_api_gi.h
 * @brief Top level RWDTS KV LIGHT API GI header
 * @author Prashanth Bhaskar(Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 */
#ifndef __RWDTS_KV_LIGHT_API_GI_H__
#define __RWDTS_KV_LIGHT_API_GI_H__

#include <glib-object.h>

typedef struct rwdts_kv_handle_s rwdts_kv_handle_t;


__BEGIN_DECLS

GType rwdts_kv_handle_get_type(void);

typedef enum rwdts_avail_db {
  REDIS_DB = 0,
  BK_DB = 1,
  MAX_DB
} rwdts_avail_db_t;

GType rwdts_avail_db_get_type(void);

/*!
 * rwdts_kv_light_db_connect
 * Arguments: rwdts_kv_handle_t *handle, rwdts_kv_light_db_connect
 *            wsched_tasklet_ptr_t tasklet_info, char *uri, const char *file_name,
 *            const char *file_name, const char *program_name,
 *            void *callbkfn, void *callbkdata
 *
 * Returns: rw_status_t
 *
 * API to create and open the file-db. Program_name and error_file_pointer is not used
 * for now and NULL have to supplied.
 */
/// @cond GI_SCANNER
/**
 * rwdts_kv_handle_db_connect:
 * @handle: 
 * @sched: (nullable)
 * @tasklet: (nullable)
 * @uri: (nullable)
 * @file_name: (nullable) 
 * @program_name: (nullable) 
 * @callbkfn: (nullable)
 * @callbkdata: (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_kv_handle_db_connect(rwdts_kv_handle_t *handle, 
                           rwsched_instance_t *sched,
                           rwsched_tasklet_t *tasklet, 
                           char *uri,
                           const char *file_name, 
                           const char *program_name,
                           void *callbkfn, 
                           void *callbkdata);
/*!
 * API rwdts_kv_allocate_handle
 * Arguments: rwdts_avail_db_t db
 *
 * Returns : rwdts_kv_handle_t *
 *
 * API to allocate KV handle needed to connect to a DB type. Accepts DB type as
 * argument and returns KV handle. Currently only REDIS_DB db type is supported.
 * This is the first API to be used by the KV client */

/// @cond GI_SCANNER
/**
 * rwdts_kv_allocate_handle:
 * @db: 
 * Returns: (transfer full)
 */
/// @endcond
rwdts_kv_handle_t* 
rwdts_kv_allocate_handle(rwdts_avail_db_t db);

/*!
 * rwdts_kv_handle_file_close
 * Arguments: rwdts_kv_handle_t *handle
 *
 * Returns: rw_status_t
 *
 * API to close the open file-db. 
 */
/// @cond GI_SCANNER
/**
 * rwdts_kv_handle_file_close:
 * @kv_handle: 
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_kv_handle_file_close(rwdts_kv_handle_t* kv_handle);

/*!
 * rwdts_kv_handle_file_remove
 * Arguments: rwdts_kv_handle_t *handle
 *
 * Returns: rw_status_t
 *
 * API to remove the file-DB.
 */
/// @cond GI_SCANNER
/**
 * rwdts_kv_handle_file_remove:
 * @handle: 
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_kv_handle_file_remove(rwdts_kv_handle_t* handle);

/*!
 * rwdts_kv_light_add_keyval
 * Arguments: rwdts_kv_handle_t *handle
 *            int db_num
 *            char *key, int key_len,
 *            char *val, int val_len 
 *            void *callbkfn, void *callbk_data
 *
 * Returns: rw_status_t
 *
 * API to add Key/Value pair in File-DB.
 */
/// @cond GI_SCANNER
/**
 * rwdts_kv_handle_set_keyval:
 * @handle: 
 * @db_num:
 * @key:
 * @key_len: 
 * @val: 
 * @val_len:
 * @callbkfn: (nullable)
 * @callbk_data: (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_kv_handle_add_keyval(rwdts_kv_handle_t *handle, 
                          int db_num,
                          void *key, 
                          int key_len,
                          void *val,
                          int val_len,
                          void *callbkfn,
                          void *callbk_data);
/*!
 * rwdts_kv_handle_del_keyval
 * Arguments: rwdts_kv_handle_t *handle, int db_num,
 *            char *key, int key_len,
 *            void *callbkfn, void *callbk_data
 *
 * Returns: rw_status_t
 *
 * API to delete Key/Value pair from File-DB.
 */
/// @cond GI_SCANNER
/**
 * rwdts_kv_handle_del_keyval:
 * @handle:
 * @db_num:
 * @key:
 * @key_len: 
 * @callbkfn: (nullable)
 * @callbk_data: (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_kv_handle_del_keyval(rwdts_kv_handle_t *handle, 
                           int db_num,
                           void *key,
                           int key_len, 
                           void *callbkfn, 
                           void *callbk_data);


__END_DECLS


#endif //__RWDTS_KV_LIGHT_API_GI_H__
