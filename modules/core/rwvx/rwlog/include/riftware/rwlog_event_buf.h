
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
 */


/*!
 * @file rwlog_event_buf.h
 * @author Sujithra Periasamy
 * @date 17/12/2014
 * @brief  data structure for storing logs in the source
 * for speculative logging.
 */

#ifndef __RWLOG_EVENT_BUF_H__
#define __RWLOG_EVENT_BUF_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <rwlib.h>
#include <uthash.h>
#include <rw_dl.h>

#define MAX_LOG_EVENTS 1000

typedef RwLog__YangData__RwLog__EvTemplate__TemplateParams__SessionParams session_params_t;

#define MAX_LOG_BUFFER_SIZE 8192
/* 
 * Structure that holds the per call logs. The logs are stored
 * inside a doubly linked list.
 */
 /* IMPORTANT NOTE: Make sure to initialze any new fields added in fn
  * rwlog_allocate_call_logs */
typedef struct {
  uint64_t callid;
  uint32_t log_count;
  uint32_t call_arrived_time;
  uint32_t current_buffer_size;
  uint8_t  config_update_sent;
  void     *ctx;

  //rw_dl_t dll;
  UT_hash_handle hh;

  /* Need to be last member */
  uint8_t log_buffer[MAX_LOG_BUFFER_SIZE];
} rwlog_call_hash_t;

/*
 * Every log event is held in this data structure.
 * The log is the pointer to the complete log message
 * the header + packed proto.
 * The len is the length of the log event (header + proto).
 */
typedef struct {
  uint64_t callid;
  size_t len;
  uint8_t *log;
  gboolean callid_hash_member;
  rw_dl_element_t per_call_elem;
  rw_dl_element_t per_group_elem;
} rwlog_event_t;

/*
 * Main data structure for speculative logging.
 * Contains the callid based hashtable and the
 * all log events ring buffer.
 */
typedef struct {
  rwlog_call_hash_t *call_hash;
} rwlog_event_buf_t;


/*
 * Cleanup all the call hash entries 
 */
void rwlog_call_hash_cleanup(rwlog_event_buf_t *l_buf);
 

/*
 * API for the log source to call for adding a log event 
 * to the buffer.
 */
rw_status_t rwlog_add_event_to_buf(rwlog_event_buf_t *l_buf,
                                   rw_call_id_t  *sessid,
                                   uint8_t *log,
                                   size_t len);

/*
 * API for the log source to call to flush/send out the
 * logs accumulated for a particular callid.
 */
rw_status_t rwlog_flush_events_for_call(rwlog_event_buf_t *l_buf,
                                        uint64_t callid);

/* API to get call_log_entry for a callid */
rwlog_call_hash_t *rwlog_get_call_log_entry_for_callid(rwlog_event_buf_t *l_buf,uint64_t callid);

/* API to check if particular callid is present in call_hash maintained in 
 * source to buffer logs */
uint8_t rwlog_call_hash_is_present_for_callid(uint64_t callid);

/*
 * The following functions are called only from gtest.
 */
rw_status_t rwlog_gtest_get_no_of_logs_for_call(uint64_t callid);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __RWLOG_EVENT_BUF_H__ */
