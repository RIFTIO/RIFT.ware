
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
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

