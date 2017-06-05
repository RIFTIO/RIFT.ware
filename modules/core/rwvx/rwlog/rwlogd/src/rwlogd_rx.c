
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#include "rwlogd_rx.h"
#include <stdio.h>
#include <string.h>
/**********************************************
 * Function :  rwlogd_rx_loop
 * Parameter: None
 * Rx Message from Queue. 
 * Send to the rt_sinks
 * enqueue the msg in local buffer 
 **********************************************/
#if 0
// Throws warnings.. 
static void rwlogd_start_report_cores(rwlogd_instance_ptr_t instance) {
  pid_t pid = fork();
  if (pid > 0) {
    instance->core_monitor_script_child = pid;
  } else if (pid == 0) {
    //const char *argv[] = { "rwlogd_report_cores.py", NULL };
    //execvp("rwlogd_report_cores.py", argv);
    // hrm, seems improper to log from logd, particularly from a forked child thereof
  } else {
    RW_CRASH();
  }
}
static void rwlogd_check_report_cores(rwlogd_instance_ptr_t instance) {
  RW_CRASH();			/* disabled for the moment */
  if (instance->core_monitor_script_child) {
    int status = 0;
    if (instance->core_monitor_script_child == waitpid(instance->core_monitor_script_child, &status, WNOHANG)) {
      // how to log from logger?  ought to be OK for this!
      instance->core_monitor_script_child = 0;
      rwlogd_start_report_cores(instance);
    }
  }
}
#endif


static void rwlogd_start_reader_thread(rwlogd_instance_ptr_t rwlogd_instance_data);

#include "rwshell_mgmt.h"


static void rwlogd_load_base_schema(void *arg) 
{
  rwlogd_instance_ptr_t inst = (rwlogd_instance_ptr_t)arg;

  rwlogd_sink_load_schema(inst->rwlogd_info, "rwlog-mgmt");

  /* Base schema is loaded; change dynamic schema state to ready */
  inst->dynschema_app_state = RW_MGMT_SCHEMA_APPLICATION_STATE_READY;

  return;
}

void rwlogd_start(rwlogd_instance_ptr_t rwlogd_instance,char *rwlog_filename,char *filter_shm_name,
                  const char *schema_name)
{
  RWLOG_DEBUG_PRINT ("Start RWLOGD******\n");

  if(rwlog_filename == NULL) {
    int r;
    if (!getenv("RIFT_VAR_VM")) {
      r = asprintf (&rwlogd_instance->rwlogd_info->rwlog_filename,
                    "%s-%d",
                    RWLOG_FILE,
                    rwlog_get_systemId());
    } else {
      r = asprintf (&rwlogd_instance->rwlogd_info->rwlog_filename,
                    "%s-%s-%d",
                    RWLOG_FILE,
                    getenv("RIFT_VAR_VM"),
                    rwlog_get_systemId());
    }
    RW_ASSERT(r);
    if (!r) { return; }
  }
  else {
    rwlogd_instance->rwlogd_info->rwlog_filename = RW_STRDUP(rwlog_filename);
  }

  gethostname(rwlogd_instance->rwlogd_info->hostname, MAX_HOSTNAME_SZ-1);
  rwlogd_instance->rwlogd_info->hostname[MAX_HOSTNAME_SZ-1] = '\0';


  rwlogd_instance->rwlogd_info->timer = 
      rwtimer_init(&rwlogd_instance->rwlogd_info->stw_system_handle, 
                                        rwlogd_instance->rwtasklet_info,10, 1024);

  rwlogd_sink_obj_init(rwlogd_instance->rwlogd_info, filter_shm_name, schema_name);
  rwlogd_instance->rwlogd_info->num_categories =  rwlogd_get_num_categories(rwlogd_instance);
  rwlogd_start_reader_thread(rwlogd_instance);

  if(!schema_name) {
    rwsched_dispatch_queue_t main_queue = rwsched_dispatch_get_main_queue(rwlogd_instance->rwtasklet_info->rwsched_instance);
    rwsched_dispatch_async_f(rwlogd_instance->rwtasklet_info->rwsched_tasklet_info,
                             main_queue,  
                             (void *)rwlogd_instance,
                             rwlogd_load_base_schema);
  }

  rwlogd_create_default_sink(rwlogd_instance, RWLOGD_DEFAULT_SINK_NAME);
  start_bootstrap_syslog_sink(rwlogd_instance);
  start_rwlogd_console_sink(rwlogd_instance,RWLOGD_CONSOLE_SINK_NAME);

#ifdef RW_BACKGROUND_PROFILING
  g_background_profiler_enabled = (bool) getenv("RW_BACKGROUND_PROFILING");
  if (g_background_profiler_enabled) {
    g_rw_profiler_handle = start_background_profiler(
      rwlogd_instance->rwtasklet_info,
      RW_BACKGROUND_PROFILING_PERIOD,
      0);
    RW_ASSERT(g_rw_profiler_handle);
  }
#endif /* RW_BACKGROUND_PROFILING */
}

static void *circ_buf_malloc(rwlogd_instance_ptr_t instance,
                      int req_size)
{
  rwlog_hdr_t *log_hdr = NULL;
  rwlogd_tasklet_instance_ptr_t rwlogd_data = (rwlogd_tasklet_instance_ptr_t )(instance->rwlogd_info);
  char *end_ptr;
  int avail_size =  rwlogd_data->curr_used_offset - rwlogd_data->curr_offset;

  /* curr_offset -  points to current ptr ie tail of current ring buffer where new logs can point to 
   *  curr_used_offset - points to offset next to curr_offset that has valid log header  
   *  Once we do first wrap if current log uses multiple log entires space, curr_offset points to head available 
   *  to write new logs; and curr_used_offset points next valid log entry ie has valid rwlog_hdr_t data.
   *  If new log needs more space than available in (curr_used_offset - curr_offset) then we need to remove 
   * log entry pointed by curr_used_offset and move curr_used_offset further.
   *  If buffer wraps, then we again start from beginning of buffer and rwlog hdr magic is set to zero to indicate
   *   we have unused entries in end. */
    
  /* The below condition will be TRUE when circular buffer has wrapped and we dont have space for new logs
   * and old log entries has to be freed and removed from ring table */ 
  while (avail_size < req_size) {

    /* Buffer is wrapping; so point both offset to beginning of buffer;
       Since curr_used_offset is also set to zero; we will start from beginning and free up log entry if its used.
       Magic of current offset is set to zero; so that we detect end of buffer case during next wrap  */
    if(rwlogd_data->curr_used_offset == rwlogd_data->log_buffer_size || 
       (rwlogd_data->curr_used_offset + sizeof(rwlog_hdr_t) > rwlogd_data->log_buffer_size)) {
      if(rwlogd_data->curr_offset +  sizeof(rwlog_hdr_t) <= rwlogd_data->log_buffer_size) {
        ((rwlog_hdr_t *)(rwlogd_data->log_buffer + rwlogd_data->curr_offset))->magic = 0;
      }
      rwlogd_data->curr_used_offset = 0;
      rwlogd_data->curr_offset = 0;
      avail_size = 0;
      rwlogd_data->stats.circ_buff_wrap_around++;
    }

    log_hdr = (rwlog_hdr_t *)(rwlogd_data->log_buffer+rwlogd_data->curr_used_offset);
     /* If log header magic is valid; remove log entry and update available size appropriately */
     if(log_hdr->magic == RWLOG_MAGIC) {
       int size = log_hdr->size_of_proto + sizeof(rwlog_hdr_t);
       avail_size += size;
       rwlogd_data->curr_used_offset = (rwlogd_data->curr_used_offset + size);
       rwlogd_remove_log_from_default_sink(instance,log_hdr);
     }
     else {
      /* If magic is invalid; we are near end of log buffer and these are unsused space. So get avail_size and reset curr_used_offset to end of buffer */ 
       RW_DEBUG_ASSERT((avail_size+(rwlogd_data->log_buffer_size - rwlogd_data->curr_used_offset)) == (rwlogd_data->log_buffer_size - rwlogd_data->curr_offset));
       avail_size = (rwlogd_data->log_buffer_size - rwlogd_data->curr_offset);
       rwlogd_data->curr_used_offset = rwlogd_data->log_buffer_size;
     }
   }

  if(rwlogd_data->curr_offset + req_size <= rwlogd_data->log_buffer_size)  {
    end_ptr =  rwlogd_data->log_buffer + rwlogd_data->curr_offset;
    rwlogd_data->curr_offset += req_size;
  }
  else {
    /* This code should not get hit as buffer wrap should be handled before itself. Hence the Debug ASSERT */
    /* We can possibly remove all the elements from ring table and start from beginning */
    RW_DEBUG_ASSERT(0);
#if 0
    printf("Error in doing ciruclar buffer alloc; Re-initialising ring table \n");
#endif

    rwlogd_clear_log_from_default_sink(instance);
    end_ptr =  rwlogd_data->log_buffer;
    rwlogd_data->curr_offset = 0;
    rwlogd_data->curr_used_offset = rwlogd_data->log_buffer_size;
    rwlogd_data->stats.circ_buff_wrap_failure++;
  }
  return (void *)end_ptr;
}

/*!
 * clean up check pointed logs from ring buffer
 * @param inst - rwlogd instance 
 */
void
rwlogd_clear_chkpt_logs(rwlogd_instance_ptr_t inst) 
{
  rwlog_hdr_t *log_hdr;
  char *end_point;
  int size;
  
  log_hdr =  (rwlog_hdr_t *)inst->rwlogd_info->log_buffer_chkpt;
  end_point = inst->rwlogd_info->log_buffer_chkpt + 
        (inst->rwlogd_info->log_buffer_size-sizeof(rwlog_hdr_t));
  while ((char *)log_hdr <= end_point && log_hdr->magic == RWLOG_MAGIC) {
    size = log_hdr->size_of_proto + sizeof(rwlog_hdr_t);
    rwlogd_remove_log_from_default_sink(inst,log_hdr);
    log_hdr = (rwlog_hdr_t *) ((char *)log_hdr+size);
  }
} 

/*!
 * Check point current log and start/swap to new buffer
 * @param inst - rwlogd tasklet instance 
 * @result  - RW_STATUS_SUCCESS after this old logs will be chkpted buffer
 */
rw_status_t rwlogd_chkpt_logs(rwlogd_instance_ptr_t instance)
{
  rwlogd_tasklet_instance_ptr_t inst = instance->rwlogd_info;
  char *bfr;

  if (inst->log_buffer_chkpt) {
    rwlogd_clear_chkpt_logs(instance); /* clear existing checkpointed logs */
    bfr =inst->log_buffer_chkpt;
  }
  else {
    bfr =RW_MALLOC0(inst->log_buffer_size);
    RW_DEBUG_ASSERT(bfr);
    if (!bfr) {
     return RW_STATUS_FAILURE;
    }
  }
  inst->curr_offset = 0;
  inst->curr_used_offset = inst->log_buffer_size;

  inst->log_buffer_chkpt = inst->log_buffer;
  inst->log_buffer = bfr;
  inst->stats.chkpt_stats++;
  return RW_STATUS_SUCCESS;
}


static void callid_filter_release(stw_tmr_t *tmr, void *arg) 
{
  rwlogd_callid_info_t *callid_info = (rwlogd_callid_info_t *)arg;
  RW_FREE(tmr); 
  if (!callid_info) {return;}
  rwlogd_remove_callid_filter(callid_info->inst,callid_info->callid);
  RW_FREE(callid_info);
}


void rwlogd_handle_log(rwlogd_instance_ptr_t instance,uint8_t *rcvd_buf,size_t size,bool alloc_buf) 
{
  rwlog_hdr_t *hdr = (rwlog_hdr_t *)rcvd_buf;
  uint8_t *buf = NULL;

  if(alloc_buf) {
    buf = circ_buf_malloc (instance,size);
    memcpy(buf,rcvd_buf,size);
  }
  else {
    buf = rcvd_buf;
  }


  /* If add_callid_filter flag is set, then add filter */
  if(hdr->cid_filter.cid_attrs.add_callid_filter) {
    rwlogd_add_callid_filter(instance->rwlogd_info,buf);
  }
  else  {
    uint8_t remove_callid_filter = FALSE;
    if (hdr->cid_filter.cid_attrs.invalidate_callid_filter) {
      remove_callid_filter = TRUE;
    }

    rwlogd_notify_log(instance->rwlogd_info, buf);

    if(remove_callid_filter) {
      rwlogd_callid_info_t *callid_info;
      rw_tmr_t *tmr;
      callid_info = RW_MALLOC(sizeof(rwlogd_callid_info_t));
      RW_DEBUG_ASSERT(callid_info);
      tmr = RW_MALLOC(sizeof(rw_tmr_t));
      RW_DEBUG_ASSERT(tmr);
      if (callid_info && tmr) {
        callid_info->inst = instance->rwlogd_info;
        callid_info->callid = ((rwlog_hdr_t *)buf)->cid.callid;
        /* 2 minutes timer */
        rwtimer_start(instance->rwlogd_info->stw_system_handle,
                tmr,2*60*1000,0, (void *)callid_filter_release, callid_info);
      }
      else {
        rwlogd_remove_callid_filter(instance->rwlogd_info,((rwlog_hdr_t *)buf)->cid.callid);
        if (tmr) {
          RW_FREE(tmr);
        }
        if (callid_info) {
          RW_FREE(callid_info);
        }
      }
    }
  }
}


static rw_status_t rwlogd_send_logs_to_peer(rwlogd_instance_ptr_t instance,uint8_t *rcvd_buf,size_t size,int peer_rwlogd_instance) 
{
  rw_status_t status;
  rwlogd_peer_node_entry_t *peer_node = NULL;
  RwlogdSendLogReq req;

  status =  RW_SKLIST_LOOKUP_BY_KEY(&(instance->rwlogd_info->peer_rwlogd_list),&peer_rwlogd_instance, &peer_node);
  RW_ASSERT(status == RW_STATUS_SUCCESS);

  if(status != RW_STATUS_SUCCESS) {
    return status;
  }
  RW_ASSERT(peer_node);
  if (!peer_node) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(peer_node->rwtasklet_instance_id == peer_rwlogd_instance);
  if (peer_node->rwtasklet_instance_id != peer_rwlogd_instance) {
    return RW_STATUS_FAILURE;
  }
  if(peer_node->current_size + size < sizeof(peer_node->log_buffer)) {
    memcpy(&peer_node->log_buffer[peer_node->current_size],rcvd_buf,size);
    peer_node->current_size += size;
  }
  else {
    if(peer_node->current_size) {
      rwlogd_send_log_req__init(&req);
      req.log_msg.data = peer_node->log_buffer;
      req.log_msg.len = peer_node->current_size;

      RWLOGD_CALL_UNICAST_API_NON_BLOCKING(send_log,
                                           instance,
                                           peer_rwlogd_instance,
                                           &req,
                                           NULL,
                                           instance);
      instance->rwlogd_info->stats.peer_send_requests++;
    }
    peer_node->current_size = 0;
    /* Copy new buffer to peer node */
    if(size < sizeof(peer_node->log_buffer)) {
      memcpy(&peer_node->log_buffer[0],rcvd_buf,size);
      peer_node->current_size = size;
    }
    else {
      rwlogd_send_log_req__init(&req);
      req.log_msg.data = rcvd_buf;
      req.log_msg.len = size;

      RWLOGD_CALL_UNICAST_API_NON_BLOCKING(send_log,
                                           instance,
                                           peer_rwlogd_instance,
                                           &req,
                                           NULL,
                                           instance);
      instance->rwlogd_info->stats.peer_send_requests++;
    }
  }

  return status;
}

static rw_status_t rwlogd_flush_logs_to_peer(rwlogd_instance_ptr_t instance) 
{
  rwlogd_peer_node_entry_t *peer_node = NULL;
  RwlogdSendLogReq req;

  for (peer_node = RW_SKLIST_HEAD(&(instance->rwlogd_info->peer_rwlogd_list), rwlogd_peer_node_entry_t);
       peer_node != NULL;
       peer_node = RW_SKLIST_NEXT(peer_node, rwlogd_peer_node_entry_t, element)) {
    /* If current_size is non zero - flush logs */
    if(peer_node->current_size && peer_node->rwtasklet_instance_id) {
      rwlogd_send_log_req__init(&req);
      req.log_msg.data = &peer_node->log_buffer[0];
      req.log_msg.len = peer_node->current_size;

      RWLOGD_CALL_UNICAST_API_NON_BLOCKING(send_log,
                                           instance,
                                           peer_node->rwtasklet_instance_id,
                                           &req,
                                           NULL,
                                           instance);
      instance->rwlogd_info->stats.peer_send_requests++;
      peer_node->current_size = 0;
    }
  }

  return RW_STATUS_SUCCESS;
}

/* Used to debug rwlogd rx loop */
//#define RWLOGD_READ_LOOP_DEBUG 1

void rwlogd_rx_loop(void  *data)
{ 
  int fd = -1;
  uint8_t *buf = 0;
  struct stat file_stats;
  uint64_t no_of_logs_read = 0;
  bool peer_logs_sent = FALSE;
  bool force_rotate = FALSE;

  rwlogd_instance_ptr_t rwlogd_instance = (rwlogd_instance_ptr_t )data;

  rwlogd_tasklet_instance_ptr_t rwlogd_instance_data = 
      (rwlogd_tasklet_instance_ptr_t )(rwlogd_instance->rwlogd_info);
  int size = 0;

  /* Update ticks in shm filter_memory */
  rwlogd_shm_incr_ticks(rwlogd_instance_data);

  if (rwlogd_instance_data->bootstrap_completed == FALSE)
  {
    rwlogd_instance_data->bootstrap_counter++;
    if(rwlogd_instance_data->bootstrap_counter == rwlogd_instance_data->bootstrap_time) {
      rwlogd_instance_data->bootstrap_completed = TRUE;
      rwlogd_shm_reset_bootstrap(rwlogd_instance_data);
      stop_bootstrap_syslog_sink(rwlogd_instance);
      /* Rotate file once bootstrap time is completed; to avoid replay of
       * bootstrap logs in local VM when system starts next time */ 
      if (!rwlogd_instance_data->rotation_in_progress) {
        force_rotate = TRUE;
      }
    }
    else if(rwlogd_instance_data->bootstrap_counter % BOOTSTRAP_ROTATE_TIME == 0 &&
            !rwlogd_instance_data->rotation_in_progress) {
      /* We do rotate every 10 sec during bootstrap time to avoid replay of logs */ 
      force_rotate = TRUE;
    }
  }


#if 0
  /* Core file monitor, test and integrate later */
  rwlogd_check_report_cores(rwlogd_instance_data);
#endif

  if (!rwlogd_instance_data->rotation_in_progress)
  {
    if (stat (rwlogd_instance_data->rwlog_filename, &file_stats) == -1)
    {
      if (rwlogd_instance_data->file_status.fd >= 0) {
        close(rwlogd_instance_data->file_status.fd); 
        /* let's not leak, although it seems like this'll trigger some replay issues */
      }
      rwlogd_instance_data->file_status.fd = -1;
      rwlogd_instance_data->file_status.sd.st_ino = 0;
      return;
    }
    if (file_stats.st_size == 0
        && rwlogd_instance_data->file_status.ticks > 1)
    {
      return;
    }
    if (rwlogd_instance_data->file_status.fd >= 0
        && rwlogd_instance_data->file_status.sd.st_ino != file_stats.st_ino)
    {
      /* How would this happen?  Seems like it could only be a bug in
       * here or some deranged something renaming files on us? */
      RWLOG_DEBUG_PRINT ("ERROR: File mysteriously rotated on us?!\n");
      if (rwlogd_instance_data->file_status.fd > 0)
      {
        goto rotate;
      }
      rwlogd_instance_data->file_status.fd = -1;
      /* Not a rotate, just a reopen, since it wasn't open already */
    }


    if (file_stats.st_size > RWLOGD_MAX_FILE_SIZE
        || (rwlogd_instance_data->file_status.ticks
            && (--rwlogd_instance_data->file_status.ticks) == 0) ||
            force_rotate == TRUE)
    {
      /* Open the file incase its not been opened already */   
      if(rwlogd_instance_data->file_status.fd < 0) {
        rwlogd_instance_data->file_status.fd  = open(rwlogd_instance_data->rwlog_filename, O_RDONLY|O_CLOEXEC, 0);
        if(rwlogd_instance_data->file_status.fd > 0) {
          int r = fstat(rwlogd_instance_data->file_status.fd, &file_stats); /* now we know it'll be *our* file */
          if(r == 0)  {
            rwlogd_instance_data->file_status.sd.st_ino = file_stats.st_ino;
          }
        }
      }
rotate:
      RWLOG_DEBUG_PRINT ("Unlinking file %s sz=%d ticks=%lu\n", 
                         rwlogd_instance_data->rwlog_filename,
                         (int)file_stats.st_size,
                         rwlogd_instance_data->file_status.ticks);
#ifdef RWLOGD_READ_LOOP_DEBUG 
      printf("Unlinking file %s sz=%d ticks=%lu\n",rwlogd_instance_data->rwlog_filename,(int)file_stats.st_size,rwlogd_instance_data->file_status.ticks);
#endif
      int result = unlink(rwlogd_instance_data->rwlog_filename);
      if(result == -1) {
         fprintf(stderr,"Error: Rotating file failed for host: %s tasklet: %s/%d for file: %s error: %s\n",rwlogd_instance_data->hostname,
                 rwlogd_instance->rwtasklet_info->identity.rwtasklet_name,rwlogd_instance->rwtasklet_info->identity.rwtasklet_instance_id,
                 rwlogd_instance_data->rwlog_filename,strerror(errno));

         result = stat(rwlogd_instance_data->rwlog_filename, &file_stats);
         fprintf(stderr, "Error for filename: %s, result: %d error: %s, Size: %ld, uid: %d, gid: %d, mode: %o\n", 
                 rwlogd_instance_data->rwlog_filename,result,strerror(errno),file_stats.st_size,file_stats.st_uid,file_stats.st_gid,file_stats.st_mode);
      }
      rwlogd_handle_file_rotation(rwlogd_instance_data); /* bumps shm counter for clients to notice */
      rwlogd_instance_data->stats.file_rotations++;
      rwlogd_instance_data->rotation_in_progress=2;	 /* continue on the old file for two ticks so we don't miss anything */
    }
  }
  else /* rotation_in_progress */
  {
    if (stat (rwlogd_instance_data->rwlog_filename, &file_stats) != -1)
    { 
      if (file_stats.st_size > RWLOGD_MAX_FILE_SIZE)
      {
        if(rwlogd_instance_data->stats.multiple_file_rotations_atttempted % 600 == 0) {
          fprintf(stderr,"Not Supported Multiple Rotations: File %s Size %ld Attempts: %lu\n",
                  rwlogd_instance_data->rwlog_filename, 
                  file_stats.st_size,
                  rwlogd_instance_data->stats.multiple_file_rotations_atttempted);
        }
        rwlogd_instance_data->stats.multiple_file_rotations_atttempted++;
        rwlogd_shm_set_flowctl(rwlogd_instance_data);
      }
    }
    if (rwlogd_instance_data->file_status.fd < 0) 
    {
      /* Well, rotation may have been in progress, but we've not got a
       * file descriptor, so it's not in progress anymore. */
      rwlogd_instance_data->rotation_in_progress = 0;
    }
  }

  if (rwlogd_instance_data->file_status.fd < 0)
  {
    fd = open(rwlogd_instance_data->rwlog_filename, O_RDONLY|O_CLOEXEC, 0);
    if (fd < 0)
    {
      /* This will happen until someone, anyone writes the first event.  Not an error. */
      return; 
    }
    int r = fstat(fd, &file_stats); /* now we know it'll be *our* file */
    if (r) {
      /* Doh.  Mark inode as zero and it'll be redone next tick when
       * the inode fails to match.  I see no reason not to carry on
       * and read anyway. */
      memset(&file_stats, 0, sizeof(file_stats));
    }
    rwlogd_instance_data->file_status.fd = fd;
    rwlogd_instance_data->file_status.sd.st_ino = file_stats.st_ino;
    rwlogd_instance_data->file_status.ticks = (RWLOGD_RX_ROTATE_TIME_S * 1000 / RWLOGD_RX_PERIOD_MS);
    RWLOG_DEBUG_PRINT ("Opened file %s fd = %d\n", 
            rwlogd_instance_data->rwlog_filename, fd);
  }

  fd = rwlogd_instance_data->file_status.fd;
  if (fd < 0) {
    return;
  }

#ifdef RWLOGD_READ_LOOP_DEBUG
  struct timeval start_t, end_t;
  gettimeofday(&start_t, NULL);
#endif

  while(no_of_logs_read < RWLOGD_MAX_READ_PER_ITER)
  {
    rwlog_hdr_t hdr;
    size = read(fd, &hdr, sizeof(rwlog_hdr_t));
    if (size == 0)
    {
        if(rwlogd_instance_data->rotation_in_progress) 
        {
          /* Countdown from 2 */
          if (!(--rwlogd_instance_data->rotation_in_progress)) {
            RWLOG_DEBUG_PRINT("Read Complete of unlinked file %s fd %d error %s read %d; closing\n", 
                              rwlogd_instance_data->rwlog_filename,
                              fd, strerror(errno), size);
#ifdef RWLOGD_READ_LOOP_DEBUG
            printf("Read Complete of unlinked file %s fd %d error %s read %d; closing\n",
                    rwlogd_instance_data->rwlog_filename,
                    fd, strerror(errno), size);
#endif
            rwlogd_instance_data->rotation_in_progress = 0;

            /* Close old file; it will go away when everyone has closed it */
            close(rwlogd_instance_data->file_status.fd);
            rwlogd_instance_data->file_status.fd = -1;
            rwlogd_instance_data->file_status.sd.st_ino = 0;
            rwlogd_shm_reset_flowctl(rwlogd_instance_data);
          }
        } 
        goto _read_done;
    }
    else if (size == sizeof(rwlog_hdr_t))
    {
      no_of_logs_read++;
      if (hdr.magic != RWLOG_MAGIC)
      {
        rwlogd_instance_data->stats.log_invalid_magic++;
        RWLOG_DEBUG_PRINT("Read Error%s fd %d error %s read %dl Magic%d:rotating\n",
                          rwlogd_instance_data->rwlog_filename, 
                          fd, strerror(errno), size,
                          hdr.magic);
        goto rot;
      }

      if(hdr.log_category != 0) {
        rwlogd_instance_data->stats.log_hdr_validation_failed++;
        RWLOG_DEBUG_PRINT("Log category non-zero; Read Error%s fd %d error %s read %dl Magic%d category:%d Severity:%u; rotating\n",
                          rwlogd_instance_data->rwlog_filename, 
                          fd, strerror(errno), size,
                          hdr.magic,hdr.log_category,hdr.log_severity);
        goto rot;
      }
      
      /* Additional validation of log_hdr to check its proper */
      /* Update category value */
       hdr.log_category = rwlogd_map_category_string_to_index(rwlogd_instance, hdr.log_category_str);
       //RW_ASSERT(hdr.log_category < rwlogd_instance_data->num_categories);
      if(hdr.log_category >= rwlogd_instance_data->num_categories || hdr.log_severity > RW_LOG_LOG_SEVERITY_DEBUG) {
        rwlogd_instance_data->stats.log_hdr_validation_failed++;
        RWLOG_DEBUG_PRINT("Read Error%s fd %d error %s read %dl Magic%d category:%s Severity:%u; rotating\n",
                          rwlogd_instance_data->rwlog_filename, 
                          fd, strerror(errno), size,
                          hdr.magic,hdr.log_category_str,hdr.log_severity);
        goto rot;
     }


      buf = circ_buf_malloc (rwlogd_instance,
                             hdr.size_of_proto+sizeof(rwlog_hdr_t));
      size = read(fd,buf+sizeof(rwlog_hdr_t), hdr.size_of_proto);
      //RWLOG_DEBUG_PRINT(3, "Receving Payload len %d. Read %d\n",hdr.size_of_proto, size);
      memcpy(buf, &hdr,sizeof(rwlog_hdr_t));

      if(size != hdr.size_of_proto) {
        rwlogd_instance_data->stats.log_hdr_validation_failed++;
        RWLOG_DEBUG_PRINT("Read Error%s fd %d error %s read %dl Magic%d category:%u Severity:%u; rotating\n",
                          rwlogd_instance_data->rwlog_filename, 
                          fd, strerror(errno), size,
                          hdr.magic,hdr.log_category,hdr.log_severity);
        goto rot;
      }


      /* If callid is present check RwLogD instance where this log should be
       * sent */
      if(hdr.cid.callid && rwlogd_instance_data->rwlogd_list_ring) {
        int rwlogd_instance_id = 1;
        char *logd_instance = rwlogd_get_node_from_consistent_hash_ring_for_key(rwlogd_instance_data->rwlogd_list_ring,hdr.cid.callid);
        if(logd_instance) {
          RWLOG_DEBUG_PRINT("Callid: %lu, logd instance is : %s len: %lu\n",hdr.cid.callid,logd_instance,size+sizeof(rwlog_hdr_t) );
          char *ptr = strrchr(logd_instance,'/');
          if(ptr) {
            rwlogd_instance_id = atoi(ptr+1);
            RWLOG_DEBUG_PRINT("rwlogd_instance_id is %d\n",rwlogd_instance_id);
          }
          if(rwlogd_instance_id != rwlogd_instance->rwtasklet_info->identity.rwtasklet_instance_id) {
            peer_logs_sent = TRUE;
            rwlogd_send_logs_to_peer(rwlogd_instance,buf,(size+sizeof(rwlog_hdr_t)),rwlogd_instance_id);

            /* Reset back circular buffer cur_ptr to old value */
            rwlogd_instance_data->curr_offset -= (size+sizeof(rwlog_hdr_t));
            rwlogd_instance_data->stats.log_forwarded_to_peer++;
            continue;
          }
        }
      }


      rwlogd_handle_log(rwlogd_instance,buf,(size+sizeof(rwlog_hdr_t)),FALSE);

      if (rwlogd_instance_data->rotation_in_progress) 
      {
        /* Reset countdown until we can get two ticks in a row with no
         * events after unlinking the file */
        rwlogd_instance_data->rotation_in_progress = 2;
      }
      // continue
    }
    else
    {
      /* Partial header?  But it's an atomic file!  Huh? */
      RWLOG_DEBUG_PRINT("Could not read the full header File %s fd %d error %s read %d\n", 
                        rwlogd_instance_data->rwlog_filename, fd, strerror(errno), size);
rot:
      // just rotate now to avoid trouble
      RWLOG_DEBUG_PRINT ("Unlinking file %s sz=%d ticks=%lu\n", 
                         rwlogd_instance_data->rwlog_filename,
                         (int)file_stats.st_size,
                         rwlogd_instance_data->file_status.ticks);
      int result = unlink(rwlogd_instance_data->rwlog_filename);
      if(result == -1) {
         fprintf(stderr,"Error: Rotating file failed in host: %s tasklet: %s/%d for file: %s error: %s\n",rwlogd_instance_data->hostname,
                 rwlogd_instance->rwtasklet_info->identity.rwtasklet_name,rwlogd_instance->rwtasklet_info->identity.rwtasklet_instance_id,
                 rwlogd_instance_data->rwlog_filename,strerror(errno));

         result = stat(rwlogd_instance_data->rwlog_filename, &file_stats);
         fprintf(stderr, "Error in filename: %s, result: %d error: %s, Size: %ld, uid: %d, gid: %d, mode: %o\n", 
                 rwlogd_instance_data->rwlog_filename,result,strerror(errno),file_stats.st_size,file_stats.st_uid,file_stats.st_gid,file_stats.st_mode);
      }
      rwlogd_handle_file_rotation(rwlogd_instance_data); /* bumps shm counter for clients to notice */

      /* This is error case. So we dont wait further. Close old file; it will go away when everyone has closed it */
      close(rwlogd_instance_data->file_status.fd);
      rwlogd_instance_data->file_status.fd = -1;
      rwlogd_instance_data->file_status.sd.st_ino = 0;

      rwlogd_instance_data->rotation_in_progress=0;	 /* continue on the old file for two ticks so we don't miss anything */
      goto _read_done;
    }
  }

_read_done:

  if(peer_logs_sent) {
    rwlogd_flush_logs_to_peer(rwlogd_instance);
  }

#ifdef RWLOGD_READ_LOOP_DEBUG
  gettimeofday(&end_t, NULL);
  uint64_t time_spent = ((end_t.tv_sec - start_t.tv_sec) * 1000) + 
                    ((end_t.tv_usec - start_t.tv_usec)/1000);

  if(no_of_logs_read) {
    printf("No of logs read in this iteration is %lu;time spent: %lu\n",no_of_logs_read,time_spent);
  }
#endif
  return;
}

void rwlogd_start_reader_thread(rwlogd_instance_ptr_t rwlogd_instance_data)
{
  rwsched_dispatch_source_t timer;
  rwtasklet_info_ptr_t rwtasklet_info = rwlogd_instance_data->rwtasklet_info;

  rwsched_dispatch_queue_t main_queue = rwsched_dispatch_get_main_queue(rwtasklet_info->rwsched_instance);

  timer = rwsched_dispatch_source_create(rwtasklet_info->rwsched_tasklet_info,
                                         RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                         0,
                                         0,
                                         main_queue);
  rwsched_dispatch_source_set_timer(rwtasklet_info->rwsched_tasklet_info,
                                    timer,
                                    dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC),
                                    ((uint64_t)RWLOGD_RX_PERIOD_MS) * NSEC_PER_SEC / 1000ull,
                                    0);
  rwsched_dispatch_source_set_event_handler_f(rwtasklet_info->rwsched_tasklet_info,
                                              timer,
                                              rwlogd_rx_loop);
  rwsched_dispatch_set_context(rwtasklet_info->rwsched_tasklet_info,
                               timer,
                               rwlogd_instance_data);
  rwsched_dispatch_resume(rwtasklet_info->rwsched_tasklet_info,
                          timer);
  rwlogd_instance_data->rwlogd_info->connection_queue_timer = timer;
}


void rwlogd_clear_log_from_sink(rwlogd_instance_ptr_t instance) 
{
  rwlogd_clear_log_from_default_sink(instance);
}


static void
rwtimer_wheel_tick(void *info)
{
  stw_t *hdl = (stw_t *) info;
  stw_timer_tick(hdl);
}

rwsched_dispatch_source_t
rwtimer_init(stw_t **hdl, rwtasklet_info_ptr_t rwtasklet, int ms, int buckets)
{
  rwsched_dispatch_source_t timer;
  rwtasklet_info_ptr_t rwtasklet_info = rwtasklet;

  stw_timer_create(buckets, ms, "Rw-Timer-Wheel", hdl);
  rwsched_dispatch_queue_t main_queue = rwsched_dispatch_get_main_queue(rwtasklet_info->rwsched_instance);
  timer = rwsched_dispatch_source_create(rwtasklet_info->rwsched_tasklet_info,
                                         RWSCHED_DISPATCH_SOURCE_TYPE_TIMER,
                                         0, 0, main_queue);
  rwsched_dispatch_source_set_timer(rwtasklet_info->rwsched_tasklet_info,
                       timer, dispatch_time(DISPATCH_TIME_NOW, 0*NSEC_PER_SEC),
                       ((uint64_t)ms) * NSEC_PER_SEC / 1000ull, ((uint64_t)1) * NSEC_PER_SEC / 100ull);
  rwsched_dispatch_source_set_event_handler_f(rwtasklet_info->rwsched_tasklet_info,
                                              timer, rwtimer_wheel_tick);
  rwsched_dispatch_set_context(rwtasklet_info->rwsched_tasklet_info,
                               timer, *hdl);
  rwsched_dispatch_resume(rwtasklet_info->rwsched_tasklet_info, timer);
  return (timer);

}

rw_status_t  
rwtimer_deinit(stw_t *hdl, rwtasklet_info_ptr_t rwtasklet, rwsched_dispatch_source_t id)
{
  RC_STW_t result;
  rwsched_dispatch_source_cancel(rwtasklet->rwsched_tasklet_info,id);
  rwsched_dispatch_source_release(rwtasklet->rwsched_tasklet_info, id);
  result = stw_timer_destroy(hdl);
  if (result == RC_STW_OK){
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}



rw_status_t 
rwtimer_start(stw_t *hdl, rw_tmr_t *tmr, uint32_t initial_delay,
              uint32_t repeat_interval, stw_call_back cb, void *param)
{
  RC_STW_t result;

  stw_timer_prepare(tmr);
  result = stw_timer_start(hdl, tmr,initial_delay,repeat_interval,cb,param);
  if (result == RC_STW_OK) {
    return(RW_STATUS_SUCCESS);
  }
  return(RW_STATUS_FAILURE);
}

rw_status_t
rwtimer_stop(stw_t *hdl, rw_tmr_t *tmr)
{
  if (RC_STW_OK == stw_timer_stop(hdl, tmr)) {
    return(RW_STATUS_SUCCESS);
  }
  return(RW_STATUS_FAILURE);
}
