
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

/***
 *  rwlogd_common.h
 ***/

#ifndef __RWLOGD_COMMON_H__
#define __RWLOGD_COMMON_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "rwtrace.h"
#include "rwmsg.h"
#include "rwlogd.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****
 *
 * This file is for tasklet related info
 ****/

extern void rwlogd_rx_loop(void *p);
extern void rwlogd_start(rwlogd_instance_ptr_t rwlogd_instance_data,char *rwlog_filename,char *filter_shm_name,const char *schema_name);
extern rw_status_t rwlog_dts_registration (rwlogd_instance_ptr_t instance);
void rwlogd_handle_log(rwlogd_instance_ptr_t instance,uint8_t *buf,size_t size,bool alloc_buf);

rw_status_t
rwlogd_create_server_endpoint(rwlogd_instance_ptr_t instance);

#define RWLOGD_PROC "RW.Logd"
#define RWLOGD_DEFAULT_SINK_NAME "_DEFAULT_RWLOGD_SINK_"
#define RWLOGD_CONSOLE_SINK_NAME "RW.Console"

#ifdef __cplusplus
}
#endif
#endif

