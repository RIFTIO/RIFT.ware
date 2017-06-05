
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef __RWLOGD_RX_H
#define __RWLOGD_RX_H

#include "rwlogd_common.h"
#include <stdio.h>    /* asprintf */
#include <stdlib.h>   /* malloc */
#include <sys/time.h> /* gettimeofday */
#include <errno.h>    /* perror */
#include "rwlog.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "rwsystem_timer.h"
#ifdef __cplusplus
}
#endif

typedef struct rwlogd_callid_info_s {
  rwlogd_tasklet_instance_ptr_t inst;
  uint64_t callid;
}rwlogd_callid_info_t;

extern void rwlogd_notify_log(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data, uint8_t *proto);
extern void rwlogd_create_all_sinks (rwlogd_tasklet_instance_ptr_t rwlogd_instance_data);
//extern void rwlogd_create_sink(SINK_TYPE st, void *data, const char *host,const  int port);
void rwlogd_sink_obj_init(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data,char *filter_shm_name,
                          const char *schema_name);

void rwlogd_sink_load_schema(rwlogd_tasklet_instance_ptr_t rwlogd_instance_data,const char *schema_name);

void rwlogd_handle_file_rotation(rwlogd_tasklet_instance_ptr_t inst_data);
void rwlogd_shm_incr_ticks(rwlogd_tasklet_instance_ptr_t inst_data);
void rwlogd_shm_set_flowctl(rwlogd_tasklet_instance_ptr_t inst_data);
void rwlogd_shm_reset_flowctl(rwlogd_tasklet_instance_ptr_t inst_data);
rw_status_t rwlogd_create_default_sink(rwlogd_instance_ptr_t inst, const char *name);
void rwlogd_add_callid_filter(rwlogd_tasklet_instance_ptr_t inst_data, uint8_t *proto);
void rwlogd_remove_callid_filter(rwlogd_tasklet_instance_ptr_t inst_data, uint64_t callid);
void rwlogd_handle_file_log(rwlogd_tasklet_instance_ptr_t inst_data, char *log_string,char *name);
void rwlogd_remove_log_from_default_sink(rwlogd_instance_ptr_t inst, rwlog_hdr_t *log_hdr);
void rwlogd_clear_log_from_sink(rwlogd_instance_ptr_t instance);
extern void rwlogd_clear_log_from_default_sink(rwlogd_instance_ptr_t inst);



#define RWLOGD_CALL_UNICAST_API_NON_BLOCKING(_mymethod,               \
                                               _inst,                 \
                                               _dst_inst,             \
                                               _input,                \
                                               _rsp_func,             \
                                               _ud)                   \
  do {                                                                \
    char _tpath[256];                                                 \
    rwmsg_destination_t *_dest;                                       \
    rw_status_t rwstatus;                                             \
                                                                      \
    snprintf(_tpath, sizeof(_tpath),                                  \
             "/R/%s/%d",                                           \
             RWLOGD_PROC,                                             \
             _dst_inst);                                              \
                                                                      \
    _dest = rwmsg_destination_create(_inst->rwtasklet_info->rwmsg_endpoint, \
                                     RWMSG_ADDRTYPE_UNICAST,          \
                                     _tpath);                         \
    RW_ASSERT(_dest);                                                 \
    rwmsg_closure_t _clo = { {.pbrsp = NULL} ,.ud = NULL };   \
    rwstatus = rwlogd_peer_api__##_mymethod(&_inst->rwlogd_peer_msg_client, \
                                                    _dest,              \
                                                    _input,             \
                                                    &_clo,              \
                                                    NULL);              \
    /*RW_ASSERT(rwstatus == RW_STATUS_SUCCESS);         */              \
    if(rwstatus != RW_STATUS_SUCCESS) {                                 \
      _inst->rwlogd_info->stats.sending_log_to_peer_failed++;                      \
    }                                                                   \
    rwmsg_destination_release(_dest);                                   \
  } while(0)

#endif
