/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwlog.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/07/2013
 * @brief Process the rift logging files 
 * @details Common logging functions are defined in this file. 
 *
 **/

#include "rwlog.h"
#include <stdarg.h>
#include <stdio.h>
#include "rwlog_filters.h"
#include "rw-log.pb-c.h"

typedef RwLog__YangData__RwLog__EvTemplate__TemplateParams common_params_t;
/* Thread-specific sequence number; notionally in rwlog_ctx, but __thread doesn't work that way */
__thread struct rwlog_ctx_tls_s rwlog_ctx_tls;

static pthread_key_t rwlog_tls_key;
static uint8_t rwlog_tls_key_inited;
void rwlog_ctx_unref(rwlog_ctx_t *boxed);

static bool
rwlog_rate_limit_log(rwlog_ctx_t *ctxt,
                     ProtobufCMessage *proto,
                     common_params_t *common_params,
                     rw_call_id_t *callid);

rw_status_t rwlog_destination_destroy(rwlog_tls_key_t *tls_key)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  if(tls_key != NULL && tls_key->rwlog_file_fd > 0) {
    int r = close(tls_key->rwlog_file_fd);
    tls_key->rwlog_file_fd = -1;
    tls_key->rotation_serial = 0;
    if(r == -1) {
      return RW_STATUS_FAILURE;
    }
  }
  return status;
}

static rwlog_tls_key_t *rwlog_allocate_tls_key(rwlog_ctx_t *ctx) 
{
  rwlog_tls_key_t *tls_key;
  tls_key = RW_MALLOC0(sizeof(rwlog_tls_key_t));
  rwlog_ctx_tls.tls_key = tls_key;
  if(tls_key) {
    RWLOG_DEBUG_PRINT("Called tls key init function for tls_key %lx thread: %lu\n",(uint64_t)tls_key,pthread_self());
    pthread_setspecific(rwlog_tls_key,(void *)tls_key);
  }
  RW_ASSERT(tls_key != NULL);
  return tls_key;
}

static void rwlog_tls_key_free(void *ptr)
{
  rwlog_tls_key_t *tls_key =  (rwlog_tls_key_t *)ptr;
  if(tls_key) {
    rwlog_call_hash_cleanup(&tls_key->l_buf);
    rwlog_destination_destroy(tls_key);
    RW_FREE(tls_key);
  }
}

static void rwlog_tls_key_init()
{

  if(rwlog_tls_key_inited == FALSE) { /* To handle collapsed model case */
    pthread_key_create(&rwlog_tls_key, rwlog_tls_key_free);
    rwlog_tls_key_inited = TRUE;
  }
}

#if 0
static void rwlog_ctx_key_deinit()
{
#if 0
  // May have issue in collapsed mode.. so removing for now */
  pthread_key_delete(event_buf_key);
#endif
}
#endif


/* to make it thread safe protected by mutex */
static void rwlog_destination_setup(rwlog_ctx_t *ctx)
{
  unsigned int permission = S_IRWXU;
  char *rwlog_filename;
  rwlog_tls_key_t *tls_key;

  if(rwlog_ctx_tls.tls_key == NULL) {
    tls_key = rwlog_allocate_tls_key(ctx);
  } 
  else {
    tls_key = rwlog_ctx_tls.tls_key;
  }
  RW_ASSERT(tls_key);
  if (!tls_key) { return; }

  if(tls_key->rwlog_file_fd > 0) {
    /* FD is already created. Return */
    return;
  }

  RW_ASSERT(ctx->rwlog_filename);
  if (!ctx->rwlog_filename) { return; }
  RW_ASSERT(ctx->rwlog_filename[0]); /* There IS a bug here; I saw an empty one once! */
  if (!ctx->rwlog_filename[0]) { return; }
  rwlog_filename = ctx->rwlog_filename;
  if(ctx->filter_memory) {
    rwlog_ctx_tls.tls_key->rotation_serial = ((filter_memory_header *)ctx->filter_memory)->rotation_serial;
  }
 
  /* APPEND is important, single writes will then be atomic.  WRONLY
     is obvious.  CREAT is create, but not CREAT|EXCL, as this is a
     file shared with all other event writers. */
  tls_key->rwlog_file_fd = open(rwlog_filename, O_APPEND|O_CREAT|O_WRONLY, permission);
  if (tls_key->rwlog_file_fd < 0)
  { 
    struct stat file_stats;
    stat(rwlog_filename, &file_stats);
    RWLOG_DEBUG_PRINT ("Error Open %s %s\n", strerror(errno), rwlog_filename);
    fprintf(stderr, "Error opening log file in host: %s app: %s, filename: %s, error: %s, uid: %d, gid: %d, mode: %o at %s:%d\n", 
            ctx->hostname, ctx->appname,rwlog_filename,strerror(errno),file_stats.st_uid,file_stats.st_gid,file_stats.st_mode,
            (char *)__FILE__,__LINE__);
    ctx->error_code |= RWLOG_ERR_OPEN_FILE;
  }
  else 
  {
    RWLOG_DEBUG_PRINT ("Opened %s for ctx=%p fd=%d\n", rwlog_filename, ctx, tls_key->rwlog_file_fd);
     /* RIFT-9022 - Change ownership of file to uid of user running system instead of euid so that 
      * system run in non root mode having process running as both root and
      * non-root works for /tmp/RWLOGD-X file access.  */

    char * env_def = NULL;

    env_def = getenv("RIFT_INSTANCE_REAL_UID");
    if (env_def) {
      struct stat file_stats;
      long int real_uid;
      uid_t uid = getuid();
      uid_t euid = geteuid();
      int r = fstat(tls_key->rwlog_file_fd, &file_stats); 

      real_uid = strtol(env_def, NULL, 10);

      /* IF uid of file differs from real uid and if UID differs change file
       * ownership; check we are running as root as otherwise we may not have 
       * permission to change file ownership */
      if(r == 0 && file_stats.st_uid != real_uid  && real_uid != uid && euid == 0) {
        //fprintf(stderr,"File uid: %d differs from real uid; process uid: %d process euid: %d real uid: %lu; changing file ownership \n",file_stats.st_uid,uid,euid, real_uid);
        int r = fchown(tls_key->rwlog_file_fd,real_uid,-1);
        if (r < 0) {
          RWLOG_DEBUG_PRINT ("Failed to chown %s to uid=%d\n", rwlog_filename, real_uid);
        }
      }
    }
  }

}

/* Below fn is used to intiailise bootstrap filters during VM startup */
void rwlog_init_bootstrap_filters(char *shm_file_name)
{
  int perms = 0600;           /* permissions */
  int oflags = O_CREAT | O_RDWR; // Write for the apps. (WHY!?!?!)
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm = NULL;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl;

  if(!shm_file_name) {
    int r = 0;
    if (!getenv("RIFT_VAR_VM")) {
      r = asprintf (&rwlog_shm,
                    "%s-%d",
                    RWLOG_FILTER_SHM_PATH,
                    rwlog_get_systemId());
    } else {
      r = asprintf (&rwlog_shm,
                    "%s-%s-%d",
                    RWLOG_FILTER_SHM_PATH,
                    getenv("RIFT_VAR_VM"),
                    rwlog_get_systemId());
    }
    RW_ASSERT(r);
    if (!r) {
      return;
    }
  }
  else {
    rwlog_shm = RW_STRDUP(shm_file_name);
  }

  RWLOG_DEBUG_PRINT("INITED bootstrap log filters\n");

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    RW_CRASH_MESSAGE("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    return;
  }

  int r = ftruncate(filter_shm_fd, RWLOG_FILTER_SHM_SIZE);
  if (r < 0) { 
    RWLOG_FILTER_DEBUG_PRINT ("Error ftruncate %s SHM:%s\n", strerror(errno), rwlog_shm);
    RW_CRASH_MESSAGE ("Error ftruncate %s SHM:%s\n", strerror(errno), rwlog_shm);
    return;
  }
  
  rwlogd_shm_ctrl =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == rwlogd_shm_ctrl)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm);
    RW_CRASH_MESSAGE("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm);
    return;
  }

  memset(rwlogd_shm_ctrl, 0, RWLOG_FILTER_SHM_SIZE);

  filter_memory_header *rwlogd_filter_memory = NULL;
  rwlogd_filter_memory = &rwlogd_shm_ctrl->hdr;
  rwlogd_filter_memory->magic = RWLOG_FILTER_MAGIC;
  rwlogd_filter_memory->rwlogd_pid = getpid();
  rwlogd_filter_memory->log_serial_no = 0;

  rwlogd_filter_memory->skip_L1 = TRUE;
  rwlogd_filter_memory->skip_L2 = TRUE;
  rwlogd_filter_memory->bootstrap = TRUE;
  rwlogd_filter_memory->rwlogd_flow = FALSE;
  rwlogd_filter_memory->allow_dup = FALSE;

  rwlogd_shm_ctrl->last_location = sizeof(rwlogd_shm_ctrl_t);

  munmap(rwlogd_shm_ctrl,RWLOG_FILTER_SHM_SIZE);
  free(rwlog_shm);
  close(filter_shm_fd);
}


/*!
 * set allow_duplicate flag to TRUE - for gtest as Gtest repeatedly uses same event id for tesitng
 */
void rwlog_shm_set_dup_events(char *shm_file_name, bool flag)
{
  int perms = 0600;           /* permissions */
  int oflags = O_CREAT | O_RDWR; // Write for the apps. (WHY!?!?!)
  int mprot = PROT_READ|PROT_WRITE;
  int mflags = MAP_FILE|MAP_SHARED;
  char *rwlog_shm = NULL;
  rwlogd_shm_ctrl_t *rwlogd_shm_ctrl;

  if(!shm_file_name) {
    int r = 0;
    if (!getenv("RIFT_VAR_VM")) {
      r = asprintf (&rwlog_shm,
                    "%s-%d",
                    RWLOG_FILTER_SHM_PATH,
                    rwlog_get_systemId());
    } else {
      r = asprintf (&rwlog_shm,
                    "%s-%s-%d",
                    RWLOG_FILTER_SHM_PATH,
                    getenv("RIFT_VAR_VM"),
                    rwlog_get_systemId());
    }
    RW_ASSERT(r);
    if (!r) {
      return;
    }
  }
  else {
    rwlog_shm = RW_STRDUP(shm_file_name);
  }

  int filter_shm_fd =  shm_open(rwlog_shm,oflags,perms);
  if (filter_shm_fd < 0)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    RW_CRASH_MESSAGE("Error Open %s for  SHM:%s\n", strerror(errno), rwlog_shm);
    return;
  }
  rwlogd_shm_ctrl =
      (rwlogd_shm_ctrl_t *) mmap(NULL, RWLOG_FILTER_SHM_SIZE, mprot, mflags, filter_shm_fd, 0);
  if (MAP_FAILED == rwlogd_shm_ctrl)
  {
    RWLOG_FILTER_DEBUG_PRINT ("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm);
    RW_CRASH_MESSAGE("Error Open %s for MAP_FAILED SHM:%s\n", strerror(errno), rwlog_shm);
    return;
  }

  filter_memory_header *rwlogd_filter_memory = NULL;
  rwlogd_filter_memory = &rwlogd_shm_ctrl->hdr;
  rwlogd_filter_memory->allow_dup = flag;
  munmap(rwlogd_shm_ctrl,RWLOG_FILTER_SHM_SIZE);
  close(filter_shm_fd);
  free(rwlog_shm);
}

rwlog_ctx_t *rwlog_init_internal(const char *taskname, char *rwlog_filename,char *rwlog_shm_name,uuid_t vnf_id)
{
  rwlog_ctx_t *ctx;

  ctx = RW_MALLOC0(sizeof(rwlog_ctx_t));
  RW_ASSERT(ctx);
  if (!ctx) {
    return NULL;
  }

  RWLOG_DEBUG_PRINT("Rwlog instance name is %s\n",taskname);

  /* Initialize hostname, pid and application name 
   * These don't change during the life of the process 
   * NOTE: hostname may change, we need to have a way to update 
   * the hostname if it changes 
   */
  ctx->magic = RWLOG_CONTEXT_MAGIC;
  ctx->version = RWLOG_VER;
  g_atomic_int_inc(&ctx->ref_count);
  RW_ASSERT(gethostname(ctx->hostname, MAX_HOSTNAME_SZ-1) == 0);
  ctx->hostname[MAX_HOSTNAME_SZ-1] = '\0';
  ctx->pid = getpid();

  if(vnf_id && !uuid_is_null(vnf_id)) {
    uuid_copy(ctx->vnf_id, vnf_id);
  }
  else {
    uuid_clear(ctx->vnf_id);
  }

  strncpy(ctx->appname, taskname, MAX_TASKNAME_SZ-1);
  ctx->appname[MAX_TASKNAME_SZ-1] = '\0';
  ctx->speculative_buffer_window = RWLOG_CALLID_WINDOW;
  rwlog_ctx_tls.rwlogd_ticks = 0;
  rwlog_ctx_tls.log_count  = RWLOG_PER_TICK_COUNTER;
  rwlog_ctx_tls.repeat_count = 0;
  rwlog_ctx_tls.last_ev = 0;
  rwlog_ctx_tls.last_line_no = 0;
  rwlog_ctx_tls.last_tv_sec = 0;
  rwlog_ctx_tls.last_cid = 0;
  ctx->filter_shm_fd = -1;

  ctx->rwlog_filename = RW_STRDUP(rwlog_filename);
  ctx->rwlog_shm = RW_STRDUP(rwlog_shm_name);

  /*
   * Thread local storage based on key to hold rwlog fd and speculative logging hash *
   */
  rwlog_tls_key_init();

  rwlog_app_filter_setup(ctx);
  rwlog_destination_setup(ctx);
  return ctx;
}

/*
 * rwlog_init
 */
rwlog_ctx_t *rwlog_init(const char *taskname)
{
  char *rwlog_filename = NULL;
  char *rwlog_shm_name = NULL;
  int r;
  rwlog_ctx_t *ctx;

  if (!getenv("RIFT_VAR_VM")) {
    r = asprintf (&rwlog_filename, 
                  "%s-%d", 
                  RWLOG_FILE,
                  rwlog_get_systemId());

    RW_ASSERT(r);
    if (!r) { return NULL; }


    r = asprintf (&rwlog_shm_name,
                  "%s-%d",
                  RWLOG_FILTER_SHM_PATH,
                  rwlog_get_systemId());
  } else {
    r = asprintf (&rwlog_filename,
                  "%s-%s-%d",
                  RWLOG_FILE,
                  getenv("RIFT_VAR_VM"),
                  rwlog_get_systemId());

    RW_ASSERT(r);
    if (!r) { return NULL; }

    r = asprintf (&rwlog_shm_name,
                  "%s-%s-%d",
                  RWLOG_FILTER_SHM_PATH,
                  getenv("RIFT_VAR_VM"),
                  rwlog_get_systemId());
  }

  RW_ASSERT(r);
  if (!r) { 
    free(rwlog_filename);
    return NULL; 
  }

  ctx = rwlog_init_internal(taskname,rwlog_filename,rwlog_shm_name,NULL);
  free(rwlog_filename);
  free(rwlog_shm_name);

  return ctx;
}

rwlog_ctx_t *rwlog_init_with_vnf(const char *taskname, uuid_t vnf_id)
{
  char *rwlog_filename = NULL;
  char *rwlog_shm_name = NULL;
  int r;
  rwlog_ctx_t *ctx;

  if (!getenv("RIFT_VAR_VM")) {
    r = asprintf (&rwlog_filename, 
                  "%s-%d", 
                  RWLOG_FILE,
                  rwlog_get_systemId());

    RW_ASSERT(r);
    if (!r) { return NULL; }

    r = asprintf (&rwlog_shm_name,
                  "%s-%d",
                  RWLOG_FILTER_SHM_PATH,
                  rwlog_get_systemId());
  } else {
    r = asprintf (&rwlog_filename,
                  "%s-%s-%d",
                  RWLOG_FILE,
                  getenv("RIFT_VAR_VM"),
                  rwlog_get_systemId());

    RW_ASSERT(r);
    if (!r) { return NULL; }

    r = asprintf (&rwlog_shm_name,
                  "%s-%s-%d",
                  RWLOG_FILTER_SHM_PATH,
                  getenv("RIFT_VAR_VM"),
                  rwlog_get_systemId());
  }
  RW_ASSERT(r);
  if (!r) {
    free(rwlog_filename);
    return NULL;
  }

  ctx = rwlog_init_internal(taskname,rwlog_filename,rwlog_shm_name,vnf_id);
  free(rwlog_filename);
  free(rwlog_shm_name);

  return ctx;
}


void rwlog_ctxt_dump(rwlog_ctx_t *ctxt)
{
  printf("Ctxt info %p\n",ctxt);
  printf("---------------------\n");
  if(ctxt->error_code)
  { 
    printf("Error Code = %d\n", ctxt->error_code);
  }
  printf("version %d hostname %s appname %s\n",
         ctxt->version,
         ctxt->hostname,
         ctxt->appname);
  printf("File %s Shm %s\n", ctxt->rwlog_filename, ctxt->rwlog_shm);
#if 0
  printf("rotation_serial %d\n",
         ctxt->rotation_serial);
#endif
  printf("filedescriptor %d filter_shm_fd %d pid %d ref_count %d\n",
         rwlog_ctx_tls.tls_key->rwlog_file_fd,  ctxt->filter_shm_fd, ctxt->pid, ctxt->ref_count);
  printf("Ctxt Stats: \n"
         "-----------------\n"
         "attempts         :%lu\t"
         "bootstrap_events :%lu\n"
         "filtered_events  :%lu\t"
         "sent_events      :%lu\n"
         "l2_strcmps       :%lu\t"
         "shm_invalid      :%lu\n"
         "dropped          :%lu\t"
         "dup log drops    :%lu\n"
         "send failed      :%lu\t"
         "L1 flag updated  :%lu\n"
         "L1 filter passed :%lu\n"
         "\n\n",
         ctxt->stats.attempts,
         ctxt->stats.bootstrap_events,
         ctxt->stats.filtered_events,
         ctxt->stats.sent_events,
         ctxt->stats.l2_strcmps,
         ctxt->stats.shm_filter_invalid_cnt,
         ctxt->stats.dropped,
         ctxt->stats.duplicate_suppression_drops,
         ctxt->stats.log_send_failed,
         ctxt->stats.l1_filter_flag_updated,
         ctxt->stats.l1_filter_passed);
  rwlog_app_filter_dump(ctxt->filter_memory);
}

/* 
 * rwlog_update_appname 
 * param: ctxt
 * param: new_name concat "component, Instance"
 */

void rwlog_update_appname(rwlog_ctx_t *ctx, const char *taskname)
{
  RW_ASSERT(ctx);
  if (!ctx) { return;}
  strncpy(ctx->appname, taskname, MAX_TASKNAME_SZ-1);
  ctx->appname[MAX_TASKNAME_SZ-1] = '\0';
}
/*
 * rwlog_close
 * remove_logfile - Always FALSE; except for Gtest when we want to cleanup file 
 */
rw_status_t rwlog_close(rwlog_ctx_t *ctx,bool remove_logfile)
{
  if (ctx) {
    rwlog_ctx_unref(ctx);
  }
  return RW_STATUS_SUCCESS;
}
rw_status_t rwlog_close_internal(rwlog_ctx_t *ctx,bool remove_logfile)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  if (ctx) {
    RW_ASSERT(ctx->magic == RWLOG_CONTEXT_MAGIC);
    status = rwlog_app_filter_destroy(ctx);
   if(remove_logfile) {
     rwlog_destination_destroy(rwlog_ctx_tls.tls_key);
     pthread_setspecific(rwlog_tls_key,NULL);
     rwlog_tls_key_free(rwlog_ctx_tls.tls_key);
     rwlog_ctx_tls.tls_key = NULL;
     remove(ctx->rwlog_filename);
    }
    RW_FREE(ctx->rwlog_filename);
    RW_FREE(ctx->rwlog_shm);
    ctx->rwlog_filename = NULL;
    ctx->rwlog_shm = NULL;
    ctx->magic = 0;
    RW_FREE(ctx);
  }
  return status;
}

rw_status_t rw_pb_get_field_value_str (char *value_str,            
					      size_t *value_str_len,
					      ProtobufCMessage *pbcm,
					      const char *field_name) {
  char *location = 0;
  const ProtobufCFieldDescriptor *fd = 0;
  
  // find the field name within the message
  fd = protobuf_c_message_descriptor_get_field_by_name(pbcm->descriptor,field_name);
  
  if (!fd) {
    return RW_STATUS_FAILURE;
  }
  
  if (fd->type == PROTOBUF_C_TYPE_MESSAGE) {
    // This is used to find key type as of now. fail
    return RW_STATUS_FAILURE;
  }  
  
  switch (fd->label) {
  case PROTOBUF_C_LABEL_REQUIRED:
    location = (char *)pbcm + fd->offset;
    break;
  case PROTOBUF_C_LABEL_OPTIONAL:
    if (*((protobuf_c_boolean *)((char *)pbcm + fd->quantifier_offset))) {
      location = ((char *)pbcm + fd->offset);
      if (!(fd->rw_flags & RW_PROTOBUF_FOPT_INLINE) &&
	  ((fd->type == PROTOBUF_C_TYPE_STRING) ||
	   (fd->type == PROTOBUF_C_TYPE_BYTES))) {
	location = *(char **) location;
      }
    } else {
      return RW_STATUS_FAILURE;        
    }
    break;
  default:
    // REPEATED also shouldnt qualify for now
    return RW_STATUS_FAILURE;
    break;      
  }
  
  RW_ASSERT(location);
  RW_ASSERT(fd);
  if (!fd || !location) {
    return RW_STATUS_FAILURE;
  }
  
  protobuf_c_boolean okay = protobuf_c_field_get_text_value (NULL,fd, value_str, value_str_len, location);
  return okay ? RW_STATUS_SUCCESS : RW_STATUS_FAILURE;
}

#if 0
/* Why on dog's green earth have we not got a standard function for this? */
rw_status_t rw_pb_get_field_value_uint64 (const ProtobufCMessage *pbcm,
						 const char *field_name,
						 uint64_t *out)
{
  char *location = 0;
  const ProtobufCFieldDescriptor *fd = 0;
  
  // find the field name within the message
  fd = protobuf_c_message_descriptor_get_field_by_name(pbcm->descriptor,field_name);
  
  if (!fd) {
    return RW_STATUS_FAILURE;
  }
  
  if (fd->type == PROTOBUF_C_TYPE_MESSAGE) {
    // This is used to find key type as of now. fail
    return RW_STATUS_FAILURE;
  }  
  
  switch (fd->label) {
  case PROTOBUF_C_LABEL_REQUIRED:
    location = (char *)pbcm + fd->offset;
    break;
  case PROTOBUF_C_LABEL_OPTIONAL:
    if (*((protobuf_c_boolean *)((char *)pbcm + fd->quantifier_offset))) {
      location = ((char *)pbcm + fd->offset);
      if (!(fd->rw_flags & RW_PROTOBUF_FOPT_INLINE) &&
	  ((fd->type == PROTOBUF_C_TYPE_STRING) ||
	   (fd->type == PROTOBUF_C_TYPE_BYTES))) {
	location = *(char **) location;
      }
    } else {
      return RW_STATUS_FAILURE;        
    }
    break;
  default:
    // REPEATED also shouldnt qualify for now
    return RW_STATUS_FAILURE;
    break;      
  }
  
  RW_ASSERT(location);
  RW_ASSERT(fd);
  if (!fd || !location) {
    return RW_STATUS_FAILURE;
  }
  
  switch (fd->type) {
  case PROTOBUF_C_TYPE_INT32:
  case PROTOBUF_C_TYPE_SINT32:
    *out = ((uint64_t)(int64_t)(*(int32_t*)location));
    break;    
  case PROTOBUF_C_TYPE_UINT32:
    *out = (uint64_t) (*(uint32_t*)location);
    break;
    
  case PROTOBUF_C_TYPE_INT64:
  case PROTOBUF_C_TYPE_SINT64:
    *out = (uint64_t)(*(int64_t*)location);
    break;
  case PROTOBUF_C_TYPE_UINT64:
    *out = (*(uint64_t*)location);
    break;
    
  case PROTOBUF_C_TYPE_BOOL:
  case PROTOBUF_C_TYPE_ENUM:
    *out = (uint64_t) (*(int*)location);
    break;
    
  default:
    return RW_STATUS_FAILURE;
    break;
    
  }
  
  return RW_STATUS_SUCCESS;
}
#endif

rw_status_t rwlog_check_failed_call_filter(rwlog_ctx_t *ctx, const char *cat_str,
                                           ProtobufCMessage *proto, rw_call_id_t *callid, int *pass)
{
  rw_status_t  status = RW_STATUS_FAILURE;
  rwlog_call_hash_t *call_log_entry = NULL;

  call_log_entry = rwlog_get_call_log_entry_for_callid(&rwlog_ctx_tls.tls_key->l_buf,callid->callid);
  if(!call_log_entry) {
    return RW_STATUS_SUCCESS;
  }

  const ProtobufCFieldDescriptor *fd1 =  protobuf_c_message_descriptor_get_field_by_name(proto->descriptor, "call_failure_cause");
  if(fd1) {
    RWLOG_FILTER_DEBUG_PRINT("Call Failure cause parameter is present %lu\n",callid->callid);

    if(call_log_entry->current_buffer_size) {
      /* Write add_callid_filter in first log buffered */
      rwlog_hdr_t *tmp_hdr =(rwlog_hdr_t *) call_log_entry->log_buffer;
      tmp_hdr->cid_filter.cid_attrs.add_callid_filter = TRUE;
      tmp_hdr->cid_filter.cid_attrs.failed_call_callid = TRUE;

      rwlog_callid_event_write( ctx, call_log_entry->log_buffer, call_log_entry->current_buffer_size); 
      call_log_entry->current_buffer_size = 0;
      call_log_entry->log_count = 0;
      call_log_entry->config_update_sent = TRUE;

      rwlog_proto_send_filtered_logs_to_file(ctx, cat_str, proto, callid,FALSE,FALSE,FALSE); 
      *pass = 1;
      return RW_STATUS_SUCCESS;
    }
  }
 return status;
}

static bool rwlog_l2_exact_string_match(rwlog_ctx_t *ctxt, char *cat_str, uint32_t cat, char *name, char * value)
{
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;
  uint64_t hash_index =  0;
  uint32_t pass = 0;
  char *field_value_combo = NULL;
  bool filter_matched = FALSE;
  UNUSED(pass);

  RW_ASSERT(cat < mem_hdr->num_categories);
  if(cat >= mem_hdr->num_categories) {
    return FALSE;
  }

  int r =  asprintf (&field_value_combo,
                     "%s:%s:%s",
                     name,
                     value,
                     cat_str);

  RW_ASSERT(r);
  if (!r) {
    return FALSE;
  }
  hash_index = BLOOM_IS_SET(ctxt->category_filter[cat].bitmap,field_value_combo, pass);
  filter_array_hdr *fv_entry = &((rwlogd_shm_ctrl_t *)mem_hdr)->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
  if(fv_entry->table_offset)
  {
    int i;
    char *str = NULL;
    RWLOG_FILTER_L2_PRINT(" Find \"%s\" @ hash-index%lu\n", field_value_combo,hash_index);
    for (i=0; i<fv_entry->n_entries;i++)
    {
      str =((char *)mem_hdr + fv_entry->table_offset+i*RWLOG_FILTER_STRING_SIZE);
      ctxt->stats.l2_strcmps++;
      if(strncmp(field_value_combo, str, RWLOG_FILTER_STRING_SIZE) == 0)
      {
        RWLOG_FILTER_L2_PRINT(" Found it  = %s @ %d\n", str,i);
        filter_matched = TRUE;
        break;
      }
    }
    RWLOG_FILTER_L2_PRINT("No match  = %s @ %d\n", str,i);
  }
  if (field_value_combo)
  {
    free(field_value_combo);
  }
  return filter_matched;
}

bool rwlog_l2_exact_uint64_match(rwlog_ctx_t *ctxt, uint32_t cat, char *name, uint64_t value)
{
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;
  uint64_t hash_index =  0;
  uint32_t pass = 0;
  bool filter_matched = FALSE;
  char *field_value_combo = NULL;
  int r =  asprintf (&field_value_combo,
                    "%s:%lu",
                    name,value);
   RW_ASSERT(r);
   if (!r) {
    return FALSE;
   }

  UNUSED(pass);
  hash_index = BLOOM_IS_SET(ctxt->category_filter[cat].bitmap,field_value_combo, pass);
  filter_array_hdr *fv_entry = &((rwlogd_shm_ctrl_t *)mem_hdr)->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
  if(fv_entry->table_offset)
  {
    int i=0;
    for (i=0; i<fv_entry->n_entries; i++)
    {
      char *str =((char *)mem_hdr + fv_entry->table_offset+i*RWLOG_FILTER_STRING_SIZE);
      if(strncmp(field_value_combo, str, RWLOG_FILTER_STRING_SIZE) == 0)
      {
        filter_matched = TRUE;
        break;
      }
    }
  }
  if (field_value_combo)
  {
    free (field_value_combo);
  }
  return filter_matched;
}

/* Automatic Speculative buffer flush if any of session-params attributes
 * like imsi,ip-address, msisdn etc match  or failed-call or next-call match */
/* TODO session-params/callid  need to do L2 based filtering here */
static rw_status_t rwlog_proto_check_session_filter_match(rwlog_ctx_t *ctxt,
                                                  const char *cat_str,
                                                  uint32_t cat,
                                                  ProtobufCMessage *proto,
                                                  rw_call_id_t *callid,
                                                  common_params_t *common_params,
                                                  int *pass)
{
  struct timeval tv;           
  int filter_matched = 0; 
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;
  rwlog_call_hash_t *call_log_entry = NULL;
  rw_status_t status;

  gettimeofday(&tv, NULL);  /* Get time directly instead of looking for tv_sec in common_attrs */

  if(callid->call_arrived_time == 0) {
    callid->call_arrived_time = tv.tv_sec;
  }

  RWLOG_FILTER_CALLID(ctxt, "call-identifier", callid, filter_matched, RW_LOG_LOG_CATEGORY_ALL); 

  if (filter_matched && mem_hdr->skip_L2 == FALSE) {
    filter_matched =  rwlog_l2_exact_uint64_match(ctxt,RW_LOG_LOG_CATEGORY_ALL,"callid",callid->callid);
  }

  if (filter_matched)
  {
    *pass = 1;
    rwlog_callid_events_from_buf(ctxt,callid,FALSE);

    rwlog_proto_send_filtered_logs_to_file(ctxt, cat_str, proto, callid,FALSE,FALSE,FALSE); 
    return RW_STATUS_SUCCESS;
  }


  call_log_entry = rwlog_get_call_log_entry_for_callid(&rwlog_ctx_tls.tls_key->l_buf,callid->callid);
  if(call_log_entry && call_log_entry->config_update_sent == TRUE) {
    rwlog_proto_send_filtered_logs_to_file(ctxt, cat_str, proto, callid,FALSE,FALSE,FALSE); 
    *pass = TRUE;
    return RW_STATUS_SUCCESS;
  }

  if(mem_hdr->failed_call_filter) {
    status = rwlog_check_failed_call_filter(ctxt,cat_str,proto,callid,pass);
    if(status == RW_STATUS_SUCCESS && (*pass == TRUE)) { 
      /* If pass packet is already handled. Return False */
      return status;
    }
  }

  if(common_params && common_params->session_params) {
    session_params_t *session_params =  (session_params_t *)common_params->session_params;

    if (session_params->has_imsi) {
      RWLOG_FILTER_STRING(ctxt, "sess_param:imsi" , session_params->imsi, filter_matched,RW_LOG_LOG_CATEGORY_ALL);

      if(filter_matched && mem_hdr->skip_L2 == FALSE) {
        filter_matched = rwlog_l2_exact_string_match(ctxt,"all",RW_LOG_LOG_CATEGORY_ALL,"sess_param:imsi",session_params->imsi);
      }

      if(filter_matched) {
        *pass = TRUE;

        /* Send this log with add_callid_filter flag to TRUE so that 
         * callid filter is added by rwlodg */
        rwlog_proto_send_filtered_logs_to_file(ctxt, cat_str, proto, callid,TRUE,FALSE,FALSE); 


        if(rwlog_call_hash_is_present_for_callid(callid->callid)) { 
          /* IMSI matched. So write any speculative logs in buffer for this
           * callid */
          /* TODO callid_events_from_buf in below API should ensure callid is added to
           * L1/L2 hash. Wait for some more time for callid to updated */
          rwlog_callid_events_from_buf(ctxt,callid,TRUE);
        }
        else {
          /* If call logs is not already present create one */
          rwlog_call_hash_t *call_logs;
          call_logs = rwlog_allocate_call_logs(ctxt,callid->callid,callid->call_arrived_time);
          call_logs->config_update_sent = TRUE;
        }

        rwlog_proto_send_filtered_logs_to_file(ctxt, cat_str, proto, callid,FALSE,FALSE,FALSE); 

        return RW_STATUS_SUCCESS;
      }
    }
  }

  RWLOG_FILTER_SEVERITY(ctxt, cat , common_params->severity, common_params->crit_info,filter_matched);
  if(filter_matched) {
    *pass = 1;

    rwlog_proto_send_filtered_logs_to_file(ctxt, cat_str, proto, callid,FALSE,FALSE,FALSE); 
    return RW_STATUS_SUCCESS;
  }

  if (callid->call_arrived_time && tv.tv_sec > callid->call_arrived_time + ctxt->speculative_buffer_window) {
    callid->serial_no = mem_hdr->log_serial_no;
    rwlog_flush_events_for_call(&rwlog_ctx_tls.tls_key->l_buf,callid->callid);
    return RW_STATUS_SUCCESS;
  }

  rwlog_proto_buffer( ctxt, cat_str, proto, callid);

  return RW_STATUS_SUCCESS;
}

int rwlog_proto_filter_l2(rwlog_ctx_t *ctxt,
			  const char *cat_str,
        ProtobufCMessage *proto,
        rw_call_id_t *callid,
        common_params_t *common_params,
        uint32_t *local_log_serial_no)
{
  int pass = 0, matched_groupid = 0;
  int matched_fd_position = 0, i = 0;
  const ProtobufCFieldDescriptor *fd = NULL;
  const ProtobufCMessageDescriptor *mdesc = proto->descriptor;
  rw_status_t status;
  uint32_t cat  = 0;

  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;

  if (mem_hdr->magic != RWLOG_FILTER_MAGIC) { /* shaadap Should never happen */
    ctxt->stats.shm_filter_invalid_cnt++;
    RW_DEBUG_ASSERT(1);
    return 1;
  }

  for(cat=0;cat< mem_hdr->num_categories;cat++) {
    if(!strcmp(ctxt->category_filter[cat].name,cat_str)) {
      break;
    }
  }
  if(cat >= mem_hdr->num_categories && ctxt->local_shm) {
    cat = 0;
  }
  if(cat >= mem_hdr->num_categories) {
    //fprintf(stderr,"ERROR: Invalid category %s set for log in %s:%d\n",cat_str,common_params->filename,common_params->linenumber);
    cat = 0;
    if(!mem_hdr->num_categories) {
      fprintf(stderr, "ERROR: Category is not updated to allow filtering of logs. shm updated: %d, shm cat: %d , cat: %d\n",
              ctxt->local_shm, mem_hdr->num_categories, cat);
      return 0;
    }
  }

  
  /* Awkwardness: the EvTemplate type cannot be relied upon to result
     in notification event Pb objects which all have the same layout
     as EvTemplate. Fields must be looked up etc. */
  
  
  /* If common_params not passed, something is wrong. Return failure */
  if(!common_params) {
    return 0;
  }

  if (mem_hdr && mem_hdr->skip_L1) 
  {
    pass = 1;
    goto do_l2_only;
    // L1 are for pythons who dont use macros and speculatives
  }
 
  pass = 0;
  /* Check for session based logging filter match and send any buffer logs to file */
  if(!pass && callid && callid->callid) 
  {
    status = rwlog_proto_check_session_filter_match(ctxt,
                                         cat_str,
                                         cat,
                                         proto,
                                         callid,
                                         common_params,
                                         &pass);
    if(status == RW_STATUS_SUCCESS && pass) { 
      /* If pass log  is already handled. Return False */
      pass = 0;
      goto out;
    }
  }

  RWLOG_FILTER_SEVERITY(ctxt, cat , common_params->severity, common_params->crit_info,pass);
  if (pass)
  {
    goto out;
    // No L1 and L2 . Event passed
  }

  fd = protobuf_c_message_descriptor_get_field_by_name(proto->descriptor,RWLOG_BINARY_FIELD);
  if(fd) {
   RWPB_T_MSG(RwLog_data_BinaryEvent_PktInfo) *pkt_info  = (RWPB_T_MSG(RwLog_data_BinaryEvent_PktInfo) *)((uint64_t)proto + (fd->offset));

    pass = RWLOG_FILTER_IS_PROTOCOL_SET(mem_hdr,pkt_info->packet_type);
   if(pass) {
     //goto out;
     return pass;
   }
   // No L1 and L2. Event passed 
  }


  /* Groupid check is done here; Callid match is done as part of
   * session_filter_match */
  if (callid && callid->groupid)
  {
    RWLOG_FILTER_GROUPID(ctxt, "call-identifier", callid, pass, cat); 
    if (pass)
    {
      matched_groupid = 1;
      goto do_l2;
    }
  }

  {
    int desc_count = 0;
    pass = 0;
    for (;desc_count<mdesc->n_fields;desc_count++)
    {
      fd = mdesc->fields + mdesc->fields_sorted_by_name[desc_count];
      if (fd->type != PROTOBUF_C_TYPE_STRING)
      {
        continue;
      }
      char *str = (char *)(*(uint64_t*)((uint64_t)proto + (fd->offset)));
      if(str) {
        RWLOG_FILTER_STRING(ctxt, fd->name, str, pass, cat);
        if (pass)
        {
          matched_fd_position = desc_count;
          RWLOG_FILTER_L2_PRINT(" matched_fd_position = %d fd_value \"%s:%s\" pass =%d\n", 
                                matched_fd_position, fd->name, str, pass);
          goto do_l2;
        }
      }
    }
  }
  pass = 0;
  // Nothing matches . Drop
  goto out;
do_l2_only:
  pass = 0;
  RWLOG_FILTER_SEVERITY(ctxt, cat, common_params->severity, common_params->crit_info, pass);
  if (pass)
  {
    goto out;
    // No L1 and L2 . Event passed
  }

do_l2:
  if (!pass || 
      (mem_hdr && mem_hdr->skip_L2))
  {
    pass = 1;
    goto out;
  }


  if (callid && callid->groupid) {
    if (matched_groupid)
    {
      pass  =  rwlog_l2_exact_uint64_match(ctxt,cat,"groupcallid",callid->groupid);
      if (pass) {
        //stat
        goto out;
      }
      // No callid matched
      // Fall thru to the attribute filters
      // Stat this
    }
  }

  //Start with  matched fd . Earlier ones are ruled out.
  for(i = matched_fd_position; i < mdesc->n_fields; i++)
  {
    //if(hash(filter_data.name, mdesc->fields_sorted_by_name[i]) == FOUND)
    char *value = NULL;
   // uint64_t hash_index =  0;
    pass = 0;
    fd = mdesc->fields + mdesc->fields_sorted_by_name[i];
    if(fd->type != PROTOBUF_C_TYPE_STRING)
    {
      continue;
    }

    value = (char *)(*(uint64_t*)((uint64_t)proto + (fd->offset)));
    pass = rwlog_l2_exact_string_match(ctxt,(char *)cat_str,cat,(char *)fd->name,value);
    if(pass == TRUE) {
      goto out;
    }
  }
  pass = 0; // reset once L2 code is enabled

out:
  if(local_log_serial_no && ctxt->local_shm == FALSE && mem_hdr->skip_L1 == FALSE && mem_hdr->skip_L2 == FALSE &&
     (common_params->severity > ctxt->category_filter[cat].severity) &&
     common_params->crit_info == FALSE && 
     ctxt->category_filter[cat].bitmap == 0) {
    *local_log_serial_no = mem_hdr->log_serial_no;
    ctxt->stats.l1_filter_flag_updated++;
  }
  RWLOG_FILTER_L2_PRINT("Out Pass = %d\n", pass);
  return pass;
}

void rwlog_proto_send_c(rwlog_ctx_t *ctx, const char *cat_str,
                      ProtobufCMessage *proto, rw_call_id_t *callid, 
                      common_params_t *common_params, int line, 
                      char *file, session_params_t *session_params,
                      bool skip_filtering, bool binary_log,
                      uint32_t *local_log_serial_no) 
{
  RWLOG_PROTO_FILL_CALLID_IN_TEMPLATE_PARAMS(common_params,call_identifier,callid);
  RWLOG_PROTO_FILL_SESSION_PARAMS_IN_TEMPLATE_PARAMS(common_params,session_params);
  RWLOG_PROTO_FILL_COMMON_ATTR(ctx, common_params, 0 /*cat*/, line, file);
  rwlog_proto_send(ctx, cat_str, proto, 0, callid, common_params,skip_filtering, binary_log,local_log_serial_no);
}
void rwlog_proto_send(rwlog_ctx_t *ctx, const char *cat_str,
                      void *proto_p, int filter_matched, 
                      rw_call_id_t *callid, common_params_t *common_params,
                      bool skip_filtering, bool binary_log,
                      uint32_t *local_log_serial_no)
{
  ProtobufCMessage *proto = (ProtobufCMessage *)proto_p;
  filter_memory_header *shmctl = (filter_memory_header *)ctx->filter_memory;

  ctx->stats.l1_filter_passed++;

   /* If rwlog tls key is not yet craeted for thread; create same and also setup rwlog fd for thread */ 
  if(rwlog_ctx_tls.tls_key == NULL) {
    rwlog_allocate_tls_key(ctx);
    rwlog_destination_setup(ctx);
  } 

  /* if the shared memory is not yet open and we are using
     local static filter memory, attempt to open it! */
  if (RW_UNLIKELY(ctx->local_shm)) {
    rwlog_app_filter_setup(ctx);
  }

  if(ctx->local_shm == FALSE && rwlog_rate_limit_log(ctx,proto,common_params,callid) == TRUE) {
    return;
  }

  if(skip_filtering || rwlog_proto_filter_l2(ctx, cat_str, proto,callid,common_params,local_log_serial_no)) {
    rwlog_proto_send_filtered_logs_to_file(ctx, cat_str, proto,callid,FALSE,FALSE,binary_log);
  } 									

  /* ticks updated every 100 msec. So after every CALLID_WINDOW interval; check
   * for any call hash entry beyond interval and clean them up */
    /* Second check present to handle rollover case */
  if((shmctl->rwlogd_ticks > (rwlog_ctx_tls.last_flush_ticks + (RWLOGD_TICKS_PER_SEC*ctx->speculative_buffer_window))) ||
      (rwlog_ctx_tls.last_flush_ticks > shmctl->rwlogd_ticks)) {  
     rwlog_check_and_flush_callid_events(ctx,&rwlog_ctx_tls.tls_key->l_buf);
     rwlog_ctx_tls.last_flush_ticks = shmctl->rwlogd_ticks;
  }
}

void rwlog_proto_buffer(rwlog_ctx_t *ctx, const char *cat_str,
		      ProtobufCMessage *proto, rw_call_id_t *callid) {
  rwlog_proto_buffer_unfiltered(ctx, cat_str, proto, callid);
}


void rwlog_proto_send_gi(rwlog_ctx_t *ctx, 
                         gpointer ptr) 
{
  const ProtobufCFieldDescriptor *fd = NULL;
  rw_call_id_t callid;
  callid.callid = 0;
  callid.groupid = 0;
  callid.call_arrived_time = 0;
  common_params_t *common_params = NULL;
  const char *catstr;

  
  ProtobufCMessage *proto = (ProtobufCMessage *)ptr;
  /* GI objects are missing common attrs */
  /* GI objects are read-only */
  /* So, copy, fill common, then send */
  ProtobufCMessage *ev = protobuf_c_message_duplicate(NULL, proto, proto->descriptor);

  if (ev->descriptor && ev->descriptor->ypbc_mdesc && ev->descriptor->ypbc_mdesc->module) {
    catstr = ev->descriptor->ypbc_mdesc->module->module_name;
  } else {
    fprintf(stderr,"ERROR: Descriptor does not have valid module name.\n");
    return;
  }

  fd = protobuf_c_message_descriptor_get_field_by_name(ev->descriptor, "template_params");
  if(fd) {
    common_params = (common_params_t *)((uint8_t *)proto + (fd->offset));
  }

  /* TBD - Handle callid based on common_params instead of doing temporary copy */
  if(common_params->call_identifier.has_callid) {
    callid.callid = common_params->call_identifier.callid;
  }
  if(common_params->call_identifier.has_groupcallid) {
    callid.groupid = common_params->call_identifier.groupcallid;
  }
  if(common_params->has_call_arrived_time) {
    callid.call_arrived_time = common_params->call_arrived_time;
  }

   /* If rwlog tls key is not yet craeted for thread; create same and also setup rwlog fd for thread */ 
  if(rwlog_ctx_tls.tls_key == NULL) {
    rwlog_allocate_tls_key(ctx);
    rwlog_destination_setup(ctx);
  } 

  /* if the shared memory is not yet open and we are using
     local static filter memory, attempt to open it! */
  if (RW_UNLIKELY(ctx->local_shm)) {
    rwlog_app_filter_setup(ctx);
  }

  if(rwlog_rate_limit_log(ctx,ev,common_params,&callid) == TRUE) {
    return;
  }

  if (rwlog_proto_filter_l2(ctx, catstr, ev,NULL,common_params,NULL)) {	
    rwlog_proto_send_filtered_logs_to_file(ctx, catstr, ev, &callid,FALSE,FALSE,FALSE); 
  }									
  protobuf_c_message_free_unpacked(NULL, ev);
}

static void rwlog_proto_fill_log_hdr(rwlog_ctx_t *ctx, rwlog_hdr_t *hdr,
                         const char * cat_str, ProtobufCMessage *proto,
                         rw_call_id_t *sessid, uint8_t add_callid_filter,
                         uint32_t proto_size, rwlog_severity_t sev,
                         uint32_t common_params_offset, common_params_t *common_params,
                         bool binary_log)
{
  const ProtobufCFieldDescriptor *fd = NULL;
  hdr->magic=RWLOG_MAGIC; 
  hdr->cid_filter.reset = 0;
  if(add_callid_filter) {
    hdr->cid_filter.cid_attrs.add_callid_filter = TRUE;
  }else {
    hdr->cid_filter.cid_attrs.add_callid_filter = FALSE;
  }
  hdr->cid_filter.cid_attrs.invalidate_callid_filter = FALSE;
  hdr->cid_filter.cid_attrs.failed_call_callid = FALSE;
  hdr->cid_filter.cid_attrs.binary_log = binary_log;

  hdr->size_of_proto = proto_size;

  hdr->tv_sec = common_params->sec;
  hdr->tv_usec = common_params->usec;
  hdr->seqno = common_params->sequence;
  hdr->process_id = common_params->processid;
  hdr->thread_id = common_params->threadid;
  strncpy(hdr->hostname,common_params->hostname,sizeof(hdr->hostname));

  hdr->common_params_offset = common_params_offset;
  hdr->log_category = 0;
  strncpy(hdr->log_category_str, cat_str,sizeof(hdr->log_category_str));
  hdr->log_severity = sev;
  hdr->critical_info = common_params->crit_info;

  char vnf_id_str[36+1];
  size_t vnf_id_len=sizeof(vnf_id_str);
  if(rw_pb_get_field_value_str(vnf_id_str, &vnf_id_len,proto,"vnf_id") == RW_STATUS_SUCCESS) {
    uuid_parse(vnf_id_str,hdr->vnf_id);
  }else if(!uuid_is_null(ctx->vnf_id)) {
    uuid_copy(hdr->vnf_id,ctx->vnf_id);
  }
  else {
    uuid_clear(hdr->vnf_id);
  }
  
  if (sessid) {
    hdr->cid = *sessid;
  }
  else {
    memset(&hdr->cid, 0,sizeof(hdr->cid));
  }
  const rw_yang_pb_msgdesc_t *ypbc = (proto)->descriptor->ypbc_mdesc;
  if (ypbc && ypbc->schema_entry_value) {
    hdr->schema_id = rw_keyspec_entry_get_schema_id(ypbc->schema_entry_value);
  } else {
    hdr->schema_id.ns = 0;
    hdr->schema_id.tag = 0;
  } 

  fd = protobuf_c_message_descriptor_get_field_by_name(proto->descriptor, "invalidate_call_time");
  if(fd) {
    RWLOG_FILTER_DEBUG_PRINT("Invalidate callid parameter is present %lu",hdr->cid.callid);
    hdr->cid_filter.cid_attrs.invalidate_callid_filter = TRUE;
  }
}

rw_status_t rwlog_rotate_file(rwlog_ctx_t *ctx) 
{
  filter_memory_header *shmctl = (filter_memory_header *)ctx->filter_memory;
  RWLOG_DEBUG_PRINT("file  rotataed \n");
  rwlog_ctx_tls.tls_key->rotation_serial = shmctl->rotation_serial;
  ctx->last_rotation_tick = shmctl->rwlogd_ticks;
  rwlog_destination_destroy(rwlog_ctx_tls.tls_key);
  rwlog_destination_setup(ctx);
  if(rwlog_ctx_tls.tls_key->rwlog_file_fd < 0) {
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}

/* Simple duplicate event suppression logic 
 * If last event repeated N times in short time drop */
static bool
rwlog_suppress_duplicate_logs(rwlog_ctx_t *ctxt,
                              ProtobufCMessage *proto,
                              common_params_t *common_params,
                              rw_call_id_t *callid)
{
  const ProtobufCFieldDescriptor *fd = NULL;
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;

  if (rwlog_ctx_tls.last_ev == common_params->event_id &&
      rwlog_ctx_tls.last_line_no == common_params->linenumber) {
    if (callid && callid->callid != rwlog_ctx_tls.last_cid) {
      rwlog_ctx_tls.repeat_count = 0;
      rwlog_ctx_tls.last_ev = common_params->event_id;
      rwlog_ctx_tls.last_line_no = common_params->linenumber;
      rwlog_ctx_tls.last_cid = callid->callid;
    }
    else if (rwlog_ctx_tls.last_tv_sec + RWLOG_INTRVL>=common_params->sec) {
      rwlog_ctx_tls.repeat_count++; 
    }
    else { 
      rwlog_ctx_tls.repeat_count = 0; 
    }
  }
  else {
    rwlog_ctx_tls.repeat_count = 0;
    rwlog_ctx_tls.last_ev = common_params->event_id;
    rwlog_ctx_tls.last_line_no = common_params->linenumber;
    if (callid) {
      rwlog_ctx_tls.last_cid = callid->callid;
    }
  }
  rwlog_ctx_tls.last_tv_sec = common_params->sec;

  if (rwlog_ctx_tls.repeat_count > RWLOG_REPEAT_COUNT && !mem_hdr->allow_dup) {
    /* Do not suppress binary logs */
    fd = protobuf_c_message_descriptor_get_field_by_name(proto->descriptor,RWLOG_BINARY_FIELD);
    if (!fd) {
      ctxt->stats.duplicate_suppression_drops++;
      return TRUE;
    }
    rwlog_ctx_tls.repeat_count = 0;
  }
  return FALSE;
}

void rwlog_proto_send_filtered_logs_to_file(rwlog_ctx_t *ctx, const char *cat_str, 
                                 ProtobufCMessage *proto, rw_call_id_t *sessid,
                                 uint8_t add_callid_filter, uint8_t next_call_callid,
                                 bool binary_log) 
{
  RWLOG_FILTER_DEBUG_PRINT("rwlog_proto_send_filtered_logs_to_file(ctx=%p, app=%s, proto=%p cat =%d)\n", 
                    ctx, ctx->appname, proto, cat);

  rwlog_hdr_t *hdr; 
  uint32_t proto_size;
  uint8_t pkbuf[sizeof(*hdr)+RWLOG_PKBUFSZ]; 
  bool file_rotation = FALSE;
  int orig_fd = rwlog_ctx_tls.tls_key->rwlog_file_fd;
  rw_status_t status = RW_STATUS_SUCCESS;

  filter_memory_header *shmctl = (filter_memory_header *)ctx->filter_memory;

  if (shmctl && shmctl->magic == RWLOG_FILTER_MAGIC && rwlog_ctx_tls.tls_key && shmctl->rotation_serial != rwlog_ctx_tls.tls_key->rotation_serial) {
    file_rotation = TRUE;
    rwlog_rotate_file(ctx);
    if(status != RW_STATUS_SUCCESS) {
     fprintf(stderr,"Rotating file failed");
      return;
    }
  }

  const ProtobufCFieldDescriptor *fd = 
      protobuf_c_message_descriptor_get_field_by_name(proto->descriptor, "template_params");

  common_params_t *cp = (common_params_t *)((uint8_t *)proto + (fd->offset));

  /*Check for duplicate logs and suppress them here */
  if(rwlog_suppress_duplicate_logs(ctx,proto,cp,sessid)) {
    return;
  }
  
  RWLOG_PROTO_FILL_COMMON_ATTR_EXCLUDING_FILE_LINENO((ctx),*cp,category);

  ProtobufCBufferSimple sb = PROTOBUF_C_BUFFER_SIMPLE_INIT(NULL, pkbuf);
  sb.base.append = protobuf_c_buffer_simple_append;
  sb.alloced = sizeof(pkbuf);
  sb.len = sizeof(rwlog_hdr_t);
  sb.data = &(pkbuf[0]);
  sb.must_free_data = 0;


  proto_size = protobuf_c_message_pack_to_buffer(proto, &sb.base);
  RW_ASSERT(sb.data);
  RW_ASSERT(proto_size == (sb.len-sizeof(rwlog_hdr_t)));
  if (!sb.data || (proto_size != (sb.len-sizeof(rwlog_hdr_t)))) {
    return;
  }

  /* As part of pack, sb.data might have changed. So reset hdr again */
  hdr = (rwlog_hdr_t*)sb.data;
  rwlog_proto_fill_log_hdr(ctx,hdr,cat_str,proto,sessid,add_callid_filter,
                        proto_size,cp->severity,fd->offset,cp, binary_log);

  hdr->cid_filter.cid_attrs.next_call_callid = next_call_callid;
  hdr->cid_filter.cid_attrs.bootstrap = shmctl->bootstrap;
  
  /* Lotsa juggling above to make this be a single write.
     Probably(tm) writev would have been OK, but why tempt fate. */
  int write_sz = 0;
  if (rwlog_ctx_tls.tls_key->rwlog_file_fd != -1) {
    write_sz = write (rwlog_ctx_tls.tls_key->rwlog_file_fd, sb.data, sb.len); 
  }
  
  ctx->stats.sent_events++;
  /* Well this is a little brusque */
  if(write_sz != sb.len) {
    ctx->stats.log_send_failed++;
    if(!(ctx->error_code & RWLOG_ERR_OPEN_FILE)) {
      fprintf(stderr,"Event log write failed in host %s for fd: %d with error %s; write_size: %d, sb.len: %lu,file rotation: %d, original fd: %d,"
              "current tick: %u, last rotation tick: %u, ctx serial: %d, shm serial: %d\n",
              ctx->hostname,
              rwlog_ctx_tls.tls_key->rwlog_file_fd,strerror(errno),write_sz,sb.len,file_rotation,orig_fd,shmctl->rwlogd_ticks,ctx->last_rotation_tick,
              rwlog_ctx_tls.tls_key->rotation_serial,shmctl->rotation_serial);
    }
    else {
      /* Write failed; try to close and reopen the file */
      rwlog_destination_destroy(rwlog_ctx_tls.tls_key);
      rwlog_destination_setup(ctx);
    }
  }
  //RW_DEBUG_ASSERT(write_sz == sb.len);
  write_sz = write_sz;		/* unused shaddup */

  PROTOBUF_C_BUFFER_SIMPLE_CLEAR(&sb);

  return;  
}


rw_status_t rwlog_proto_buffer_unfiltered(rwlog_ctx_t *ctx, const char *cat_str, 
				 ProtobufCMessage *proto, rw_call_id_t *callid) 
{
  uint32_t proto_size = 0;
  size_t size = 0, tot_len = 0;
  rwlog_call_hash_t *call_log_entry;
  filter_memory_header *shmctl = (filter_memory_header *)ctx->filter_memory;

  const ProtobufCFieldDescriptor *fd = 
      protobuf_c_message_descriptor_get_field_by_name(proto->descriptor, "template_params");

  common_params_t *cp = (common_params_t *)((uint8_t *)proto + (fd->offset));
  RWLOG_PROTO_FILL_COMMON_ATTR_EXCLUDING_FILE_LINENO((ctx),*cp,category);

  size = protobuf_c_message_get_packed_size(NULL, proto);
  RW_ASSERT(size);
  if (!size) { return RW_STATUS_FAILURE;}
  tot_len = sizeof(rwlog_hdr_t) + size;

  call_log_entry = rwlog_get_call_log_entry_for_callid(&rwlog_ctx_tls.tls_key->l_buf,callid->callid);

  if(!call_log_entry) {
   call_log_entry = rwlog_allocate_call_logs(ctx,callid->callid,callid->call_arrived_time);

   /* New callid being seen; check for next-call filter and send the same */
   if(shmctl->next_call_filter) {
     /* Send this log with add_callid_filter flag to TRUE so that 
      * callid filter is added by rwlodg */
     rwlog_proto_send_filtered_logs_to_file(ctx, cat_str, proto, callid,TRUE,TRUE,FALSE); 
     call_log_entry->config_update_sent = TRUE;
     return RW_STATUS_SUCCESS;
   }
  }
  RW_ASSERT(call_log_entry);

  if(!call_log_entry) {
    // Log 
    return RW_STATUS_FAILURE;
  }

  if(call_log_entry->current_buffer_size + tot_len > MAX_LOG_BUFFER_SIZE) {
    // Log
    return RW_STATUS_FAILURE;
  }

  proto_size = protobuf_c_message_pack(NULL, proto, (call_log_entry->log_buffer+call_log_entry->current_buffer_size+sizeof(rwlog_hdr_t)));
  RW_ASSERT(proto_size == size);
  if (proto_size != size) {
    return RW_STATUS_FAILURE;
  }

  rwlog_hdr_t *hdr = (rwlog_hdr_t *)((uint8_t *)call_log_entry->log_buffer+call_log_entry->current_buffer_size);
  rwlog_proto_fill_log_hdr(ctx,hdr,cat_str,proto,callid,FALSE,proto_size,cp->severity,fd->offset,cp, FALSE);

  call_log_entry->current_buffer_size += tot_len;
  call_log_entry->log_count++;

  return RW_STATUS_SUCCESS;
}

void rwlog_callid_event_write(rwlog_ctx_t *ctx, void *logdata, int logsz)
{
  filter_memory_header *shmctl = (filter_memory_header *)ctx->filter_memory;
  
  if (shmctl && shmctl->magic == RWLOG_FILTER_MAGIC && rwlog_ctx_tls.tls_key && shmctl->rotation_serial != rwlog_ctx_tls.tls_key->rotation_serial) {
    RWLOG_DEBUG_PRINT("file  rotataed \n");
    rw_status_t status = rwlog_rotate_file(ctx);
    if(status != RW_STATUS_SUCCESS) {
      fprintf(stderr,"Rotating file failed");
      return;
    }
  }
  int write_sz = 0;
  if (rwlog_ctx_tls.tls_key->rwlog_file_fd != -1) {
    write_sz = write (rwlog_ctx_tls.tls_key->rwlog_file_fd, logdata, logsz); 
  }
  if (write_sz <= 0 ) {
    /* Cats */
    goto out;
  }
  ctx->stats.sent_events++;

 out:
  return;  
}

bool rwlog_l2_exact_denyId_match(rwlog_ctx_t *ctxt, uint64_t event_id)
{
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;
  uint64_t hash_index =  0;
  uint32_t pass = 0;
  char value[256];

  snprintf(value, 256, "event-id:%lu", event_id);
  UNUSED(pass);
  hash_index = BLOOM_IS_SET(ctxt->category_filter[0].bitmap,value, pass);
  RWLOG_FILTER_L2_PRINT("Searching \"%s\" index = %lu\n", value, hash_index);
  filter_array_hdr *fv_entry = &((rwlogd_shm_ctrl_t *)mem_hdr)->fieldvalue_entries[hash_index%RWLOG_FILTER_HASH_SIZE];
  if(fv_entry->table_offset)
  {
    RWLOG_FILTER_L2_PRINT(" Find \"%s\" @ hash-index%lu\n", value,hash_index);
    int i=0;
    for (i=0; i<fv_entry->n_entries; i++)
    {
      char *str =((char *)mem_hdr + fv_entry->table_offset+i*RWLOG_FILTER_STRING_SIZE);
      if(strncmp(value, str, RWLOG_FILTER_STRING_SIZE) == 0)
      {
        RWLOG_FILTER_L2_PRINT(" Found it  = %s @ %d\n", str,i);
        return(TRUE);
      }
      RWLOG_FILTER_L2_PRINT("No match  = %s @ %d\n", str,i);
    }
  }
  return(FALSE);
}

static bool
rwlog_rate_limit_log(rwlog_ctx_t *ctxt,
                     ProtobufCMessage *proto,
                     common_params_t *common_params,
                     rw_call_id_t *callid)
{
  filter_memory_header *mem_hdr = (filter_memory_header *)ctxt->filter_memory;

  /* If common_params not passed, something is wrong. Return failure */
  if(!common_params) {
    return 0;
  }

  /* check rate control for higher level events and drop if needed */
  if (mem_hdr->rwlogd_flow && common_params->severity > RW_LOG_LOG_SEVERITY_ERROR) {
      ctxt->stats.dropped++;      
      return TRUE;
  }
  if (mem_hdr->rwlogd_ticks && rwlog_ctx_tls.rwlogd_ticks == mem_hdr->rwlogd_ticks) {
    if (rwlog_ctx_tls.log_count) {
        rwlog_ctx_tls.log_count--;
    }
    if ((rwlog_ctx_tls.log_count == 0) && 
        (common_params->severity > RW_LOG_LOG_SEVERITY_ERROR)) {
      ctxt->stats.dropped++;      
      return TRUE;
    }
  }
  else {
    rwlog_ctx_tls.rwlogd_ticks = mem_hdr->rwlogd_ticks;
    rwlog_ctx_tls.log_count = RWLOG_PER_TICK_COUNTER;
  }

  return FALSE;
}


#include <glib-object.h>

uint32_t rwlog_get_category_severity_level(rwlog_ctx_t *ctx,const char *category)
{
  filter_memory_header *mem_hdr = (filter_memory_header *)(ctx)->filter_memory;
  uint32_t cat  = 0;

  if(mem_hdr->bootstrap) {
     return RW_LOG_LOG_SEVERITY_DEBUG;
  }

  for(cat=0;cat< mem_hdr->num_categories;cat++) {
    if(!strcmp(ctx->category_filter[cat].name,category)) {
      break;
    }
  }
  if(cat < mem_hdr->num_categories) {
    if(ctx->category_filter[cat].bitmap == 0) {
      return ctx->category_filter[cat].severity;
    }
    else {
     return RW_LOG_LOG_SEVERITY_DEBUG;
    }
  }
  else {
    return RW_LOG_LOG_SEVERITY_ERROR;
  }
}

uint32_t rwlog_get_log_serial_no(rwlog_ctx_t *ctx) {
  filter_memory_header *mem_hdr = (filter_memory_header *)(ctx)->filter_memory;
  return mem_hdr->log_serial_no;
}

char *rwlog_get_shm_filter_name(rwlog_ctx_t *ctx)
{
  return ctx->rwlog_shm;
}

rwlog_ctx_t* rwlog_ctx_ref(rwlog_ctx_t *boxed) {
  RW_ASSERT(boxed->magic == RWLOG_CONTEXT_MAGIC);
  g_atomic_int_inc(&boxed->ref_count);
  return boxed;
}

void rwlog_ctx_unref(rwlog_ctx_t *boxed) {
  RW_ASSERT(boxed->magic == RWLOG_CONTEXT_MAGIC);
  RW_ASSERT(boxed->ref_count);
  if (g_atomic_int_dec_and_test(&boxed->ref_count)) {
    rwlog_close_internal(boxed, FALSE);
    return;
  }
  return;
}

rwlog_ctx_t *rwlog_ctx_new(const char *taskname)
{
  rwlog_ctx_t *ctx = rwlog_init(taskname);
  return ctx;
}

G_DEFINE_BOXED_TYPE(rwlog_ctx_t, rwlog_ctx, rwlog_ctx_ref, rwlog_ctx_unref);


