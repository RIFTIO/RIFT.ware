/*STANDARD_RIFT_IO_COPYRIGHT*/
/*!
 * @file rwdts_kv_bkd_interface.h
 * @brief Top level RWDTS KV LIGHT API header
 * @author Prashanth Bhaskar(Prashanth.Bhaskar@riftio.com)
 * @date 2014/10/02
 */
#ifndef __RWDTS_KV_BKD_INTERFACE_H
#define __RWDTS_KV_BKD_INTERFACE_H

#include "rwdts_kv_light_common.h"
#include "rwdts_redis.h"

__BEGIN_DECLS

rw_status_t 
rwdts_kv_bkd_connect_api(rwdts_kv_handle_t *handle,
                         rwsched_instance_ptr_t sched_instance,
                         rwsched_tasklet_ptr_t tasklet_info,
                         char * hostname, uint16_t port,
                         const char *file_name,
                         const char *program_name,
                         void *callbkfn, void *callbk_data);

/*
rw_status_t
rwdts_kv_bkd_set_api(void *instance, char *file_name, char *key, int key_len,
                     char *val, int val_len);
                     */

rw_status_t 
rwdts_kv_bkd_set_api(rwdts_kv_handle_t *handle, int db_num, char *file_name, char *key,
                     int key_len, char *val, int val_len,
                     void *callbkfn, void *callbk_data);

rw_status_t
rwdts_kv_bkd_get_api(void *instance, char *file_name, char *key, int key_len,
                     char **val, int *val_len);

rw_status_t
rwdts_kv_bkd_del_api(rwdts_kv_handle_t *handle, int db_num, char *file_name, char *key, int key_len,
                     void *callbkfn, void *callbk_data);

void *
rwdts_kv_get_bkd_cursor(void *instance, char *file_name);

rw_status_t
rwdts_kv_get_next_bkd_api(void *instance,void *cursor, char **key, int *key_len,
                          char **val, int *val_len, void **out_cursor);

rw_status_t
rwdts_kv_remove_bkd(void *instance, const char *file_name);

rw_status_t
rwdts_kv_close_bkd(void *instance);

rw_status_t
rwdts_kv_bkd_get_all(rwdts_kv_handle_t *handle, int db_num, char *file_name,
                     void *callbkfn, void *callbk_data);
__END_DECLS

#endif /*__RWDTS_KV_BKD_INTERFACE_H */
