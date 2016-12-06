
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


/**
 * @file rwlog_event_buf.c
 * @author Sujithra Periasamy
 * @date 2014/12/17
 * @brief rwlog source log event buffer APIs for speculative logging.
 *
 */

#include <pthread.h>
#include "rwlog.h"
#include <rwlog_event_buf.h>

void rwlog_call_hash_cleanup(rwlog_event_buf_t *l_buf)
{
  RWLOG_DEBUG_PRINT("Called call hash de-init function for l_buf %lx thread: %lu\n",(uint64_t)l_buf,pthread_self());

  if (l_buf->call_hash) {
    rwlog_call_hash_t *entry = NULL, *tmp = NULL;
    HASH_ITER(hh, l_buf->call_hash, entry, tmp) {
      HASH_DELETE(hh, l_buf->call_hash, entry);
      RW_FREE(entry);
    }
  }
}

#if 0
void rwlog_cleanup_call_hash_for_ctx(void *ctx, rwlog_event_buf_t *l_buf)
{
  if (l_buf && l_buf->call_hash) {
    rwlog_call_hash_t *entry = NULL, *tmp = NULL;
    HASH_ITER(hh, l_buf->call_hash, entry, tmp) {
      if(entry->ctx == ctx) {
        HASH_DELETE(hh, l_buf->call_hash, entry);
        RW_FREE(entry);
      }
    }
  }
}
#endif


rwlog_call_hash_t* rwlog_allocate_call_logs(rwlog_ctx_t *ctx,uint64_t callid,uint32_t call_arrived_time)
{
  rwlog_tls_key_t *tls_key;

  tls_key =  rwlog_ctx_tls.tls_key;

  if(!tls_key) {
    return NULL;
  }

  rwlog_call_hash_t *call_logs = NULL;
  call_logs = RW_MALLOC0(sizeof(rwlog_call_hash_t));
  call_logs->callid = callid;
  call_logs->call_arrived_time = call_arrived_time;
  call_logs->current_buffer_size = 0;
  call_logs->config_update_sent = 0;
  call_logs->log_count = 0;
  call_logs->ctx = (void *)ctx;

  HASH_ADD(hh, tls_key->l_buf.call_hash, callid, sizeof(callid), call_logs);
  return call_logs;
}

rw_status_t rwlog_check_and_flush_callid_events(rwlog_ctx_t *ctx,rwlog_event_buf_t *l_buf)
{
  rwlog_call_hash_t *entry = NULL, *tmp = NULL;
  if(!l_buf) {
    return RW_STATUS_SUCCESS;
  }

  if (l_buf->call_hash) {
    HASH_ITER(hh, l_buf->call_hash, entry, tmp) {
      if(rwlog_ctx_tls.tv_cached.tv_sec > entry->call_arrived_time+ctx->speculative_buffer_window) {
        /* Callid entry has past speculative buffering time; so flush the entry*/
        HASH_DELETE(hh, l_buf->call_hash, entry);
        RW_FREE(entry);
      }
    }
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t rwlog_callid_events_from_buf(rwlog_ctx_t *ctx,
                                         rw_call_id_t *sessid,
                                         uint8_t retain_entry)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwlog_tls_key_t *tls_key = rwlog_ctx_tls.tls_key;
  rwlog_event_buf_t *l_buf = NULL;
  rwlog_call_hash_t *call_logs = NULL;

  if(!tls_key) {
    return RW_STATUS_FAILURE;
  }
  l_buf = &tls_key->l_buf;

  HASH_FIND(hh, l_buf->call_hash, &(sessid->callid), sizeof(sessid->callid), call_logs);

  if (call_logs) {
    call_logs->config_update_sent = TRUE;
    rwlog_callid_event_write( ctx, call_logs->log_buffer, call_logs->current_buffer_size); 
    call_logs->current_buffer_size = 0;
    call_logs->log_count = 0;

    if(retain_entry == FALSE) {
      HASH_DELETE(hh, l_buf->call_hash, call_logs);
      RW_FREE(call_logs);
    }
  }

  return status;
}

rw_status_t rwlog_flush_events_for_call(rwlog_event_buf_t *l_buf,
                                        uint64_t callid)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  rwlog_call_hash_t *call_logs = NULL;
  
  if(!l_buf) {
    return RW_STATUS_FAILURE;
  }
  HASH_FIND(hh, l_buf->call_hash, &callid, sizeof(callid), call_logs);

  if (call_logs) {
    HASH_DELETE(hh, l_buf->call_hash, call_logs);
    RW_FREE(call_logs);
  }

  return status;
}

rwlog_call_hash_t *
rwlog_get_call_log_entry_for_callid(rwlog_event_buf_t *l_buf,uint64_t callid)
{
  rwlog_call_hash_t *call_logs = NULL;
  if(!l_buf) {
    return NULL;
  }
  HASH_FIND(hh, l_buf->call_hash, &callid, sizeof(callid), call_logs);

  return call_logs;
}


/*
 * The following functions are called only from
 * gtest.
 */
uint32_t rwlog_gtest_get_no_of_logs_for_call(uint64_t callid)
{
  rwlog_tls_key_t *tls_key = rwlog_ctx_tls.tls_key;
  rwlog_event_buf_t *l_buf = NULL;
  rwlog_call_hash_t *call_logs;
  uint32_t count = 0;

  if(!tls_key) {
    return RW_STATUS_FAILURE;
  }
  l_buf = &tls_key->l_buf;

  HASH_FIND(hh, l_buf->call_hash, &callid, sizeof(callid), call_logs);

  if (call_logs) {
    count = call_logs->log_count;
  }

  return count;
}

uint8_t rwlog_call_hash_is_present_for_callid(uint64_t callid) 
{
  rwlog_call_hash_t *call_logs;
  rwlog_tls_key_t *tls_key = rwlog_ctx_tls.tls_key;
  rwlog_event_buf_t *l_buf = NULL;

  if(!tls_key) {
    return RW_STATUS_FAILURE;
  }
  l_buf = &tls_key->l_buf;

  HASH_FIND(hh, l_buf->call_hash, &callid, sizeof(callid), call_logs);
  if(call_logs) {
    return TRUE;
  }

  return FALSE;
}
