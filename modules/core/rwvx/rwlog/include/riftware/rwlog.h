/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwlog.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/07/2013
 * @brief Top level include for RIFT logging/tracing utilities.
 */


/*! \mainpage RWLog

## Overview of adding new Log Event in Riftware Platform
 
###Two steps for adding a new event log to an existing Category:
    
-# Add the new event definition in the log-yang notification file
~~~{.c}
   notification nwepc-log-error {
           rwnotify:log-event-id 00001000;
           description
                "NwEPC Warning Log";
           uses rwlog:severity-error;
           leaf log  {
              type string;
           }
   }
~~~

-# Invoke the LOG_EVENT macro from the code to generate log. Arguments are log context, Yang derived path name, 
   followed by arguments specific to the log notification definition.
   Yang Derived path name is of the form SCHEMA_CATEGORY_PATH where \n
   SCHEMA - proto package file name in Camel case eg rw-appmgr-log.yang - RwAppmgrLog \n
   CATEGORY - notif \n
   PATH - Yang derived path of defined notification in camel case ie nwepc-log-error - NwepcLogError 
                                                                                                        
~~~{.c}
      RWLOG_EVENT(app_handle->rwtasklet_info->rwlog_instance, RwAppmgrLog_notif_NwepcLogError,  logstr);
~~~

### Session Based Logging
For session based logs, uniquely identified by 64 bit callid, if callid is avaialble below macro should be 
used having callid as third agrument followed by arguments specific to log instead of the macro RWLOG_EVENT.

~~~{.c}
      RWLOG_EVENT_CALLID(app_handle->rwtasklet_info->rwlog_instance, RwAppmgrLog_notif_NwepcLogError, callid,  logstr);
~~~

Where callid is of type rw_call_id_t * having below variables. If groupid is applicable the same is updated. \n
call_arrived_time is time application crated the session and expected to be filled by application in all the logs. This helps logging subsystem
in doing black box recording of logs for initial duration.

~~~{.c}
typedef struct rw_call_id_s 
{
  uint64_t callid;
  uint64_t groupid;
  time_t   call_arrived_time;
} rw_call_id_t;
~~~

For Session based logs when any session parameters (as identified in session-grp-params in rw-log.yang) is first available, below event log macro 
should be used where session-param is passed along with callid associated with session-params.\n

~~~{.c}
RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(app_handle->rwtasklet_info->rwlog_instance, RwAppmgrLog_notif_NwepcLogError, callid, session_params, logstr);
~~~

Where session_params  is of type RwLog_notif_EvTemplate_TemplateParams_SessionParams; initialised and updated as below.\n
Auto-genarated file rw-log.pb-c.h has data structure _RwLog__YangNotif__RwLog__EvTemplate__TemplateParams__SessionParams to know fields for reference.

~~~{.c}
RWPB_T_MSG(RwLog_notif_EvTemplate_TemplateParams_SessionParams) session_params;
RWPB_F_MSG_INIT(RwLog_notif_EvTemplate_TemplateParams_SessionParams, &session_params);
session_params.has_imsi = 1;
strncpy(session_params.imsi,"123456789012345",sizeof(session_params.imsi));
~~~

When session is cleared and callid is no longer valid, then log with invalidate-call-params in log-yang notification must be used. 
This will ensure any state regarding callid is cleaned up in logging subsystem.

~~~{.c}
  notification new-callid-invalidate-test {
    rwnotify:log-event-id 00001008;
    description "New callid invalidate test";
    uses rwlog:severity-debug;
    uses rwlog:invalidate-call-params;
    leaf msg {type string; }
  }
~~~

###Steps for adding new category to Riftware Logging Subsystem:

-# Add a yang file rw-XYZ-log.yang into XYZ/plugin/yang directory 
~~~{.c}
            rwappmgr/plugins/yang/rw-appmgr-log.yang
~~~

-# Modify CMakeLists.txt   to include this new file 
~~~{.c}
        set(source_yang_files rw-appmgr.yang rw-appmgr-log.yang)
~~~

-# Change CMakeLists.txt to link librwlog.so library.
~~~{.c}
           rwappmgr/src/CMakeLists.txt
           ${CMAKE_INSTALL_PREFIX}/usr/lib/librwlog.so
~~~

-# Include the common log header files in the source file
~~~{.c}
    rwappmgr/src/rwappmgr_ltesim.c
           #include "rw-appmgr-log.pb-c.h"
           #include "rw-log.pb-c.h"
           #include "rwlog.h"
~~~
-# Include the .so file in modules/automation/systemtest/plugins/yang/CMakeLists.txt
~~~{.c}
         ${CMAKE_INSTALL_PREFIX}/usr/lib/librwgeneric_yang_pb.so
~~~
-# Add the enum in modules/core/rwvx/rwschema/yang/rw-log.yang for the new category. 
~~~{.c}
          type enumeration {
                  enum rw-fpath;
                  enum rw-vcs;
                  enum rw-appmgr;
          }
~~~

-# Register the new yang file in modules/core/util/yangtools/lib/rw_namespace.cpp
     Example:
~~~{.c}
          register_fixed( 122, "http://riftio.com/ns/riftware-1.0/rwappmgr-log);
~~~

 -# Update the cate[] in modules/core/rwvx/rwlog/rwlogd/src/rwlog_mgmt.c for CLI pretty print rouitnes

### Please refer the following example commit which created new category and log events as reference
~~~{.c}
      commit 551a3c95cde018ede93044f3a84c6f4ad82bf7f3
      Author: Suresh Balakrishnan <Suresh.Balakrishnan@riftio.com>
      Date:   Fri Dec 19 11:23:15 2014 +0530
          RIFT-3512 LTESim: Logging support. Added rw-appmgr-log.yang to support logging for RWAppmgr.
~~~

### Speculative call Event log buffering
All log event for session logging using RWLOG_EVENT_CALLID macro;are automatically buffered in a circular buffer if the event is generated within 60 seconds of call arrival time and later application can convert them into actual log events by calling the api rwlog_callid_events_from_buf(ctxt,callid). Please refer the rwlog/test/rwlog_gtest.cc for sample implementation of this feature.

-# RWLOG_BUFFERED_EVENT(rwlog_ctx_t *, rw_call_id_t *) - Fetch the buffered log events for the callid value and write it as event logs

## Adding Sinks 
Refer 
rwlog/rwlogd/include/riftware/rwlogd_plugin_mgr.h

 */


#ifndef __RWLOG_H__
#define __RWLOG_H__

#include "rw-log.pb-c.h"
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // For strlen() definition
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <openssl/md5.h>
#include "rwlib.h"
#include "rw_sys.h"
#include "rwlog_gi.h"
#include "rwlog_category_list.h"
#include "rwlog_common_macros.h"
#include "rwlog_event_buf.h"
#include <uuid/uuid.h>


#define RWLOG_MAJOR_VER 1
#define RWLOG_MINOR_VER 0
#define RWLOG_SHIFT (16)
#define RWLOG_VER ((RWLOG_MAJOR_VER << RWLOG_SHIFT) | RWLOG_MINOR_VER)
#define RWLOG_FILE "/tmp/RWLOGD"   /* gets -### on end, ### is uid/domain/similar */

#define RW_LOG_LOG_CATEGORY_ALL 0

/*!
 * Logging severity levels
 * Compatible with syslog severity levels
 * These are defined in rwlog_event_common.proto, redefining them
 * here using short names.
 */
//#define RWLOG_FILTER_DEBUG 1

#ifdef RWLOG_FILTER_DEBUG
#define RWLOG_DEBUG_PRINT printf
#define RWLOG_FILTER_DEBUG_PRINT printf
#define RWLOG_ERROR_PRINT printf
#define RWLOG_FILTER_L2_PRINT printf
#else
#define RWLOG_DEBUG_PRINT(fmt, args...) do{} while(0)
#define RWLOG_FILTER_DEBUG_PRINT(fmt, args...) do{} while(0)
#define RWLOG_ERROR_PRINT(fmt, args...) do{} while(0)
#define RWLOG_FILTER_L2_PRINT(fmt, args...) do{} while(0)
#endif

#if RWLOG_FILTER_DEBUG
#define RWLOG_ASSERT(x) RW_ASSERT(x)
#else
#define RWLOG_ASSERT(x) if (!(x)) printf("******Rwlog Error @ %s:%d ******\n",__FILE__, __LINE__);
#endif

/*! 30 sec to bootstrap ***/
#define BOOTSTRAP_TIME (30*RWLOGD_TICKS_PER_SEC) 
/*! one minute speculative call log window ***/
#define RWLOG_CALLID_WINDOW 60 

#define BOOTSTRAP_ROTATE_TIME  (10*RWLOGD_TICKS_PER_SEC)

/* We poll in the common sense, not the POSIX sense; poll(2) always
   returns ready to read on files. */
#define RWLOGD_RX_PERIOD_MS (100)

#define RWLOGD_TICKS_PER_SEC (1000/RWLOGD_RX_PERIOD_MS)
bool rwlog_l2_exact_denyId_match(rwlog_ctx_t *ctx, uint64_t value);
typedef enum {
  RWLOG_NO_ERROR          = 0, 
  RWLOG_ERR_FILTER_SHM_FD = 0x00000001,
  RWLOG_ERR_FILTER_MMAP   = 0x00000002,
  RWLOG_ERR_FILTER_NOMEM  = 0x00000004,
  RWLOG_ERR_OPEN_FILE     = 0x00000008,
} rwlog_error_code_t;

#define MAX_HOSTNAME_SZ 64
#define MAX_TASKNAME_SZ 64
#define RWLOG_PKBUFSZ   512
#define RWLOG_PER_TICK_COUNTER 3000

#define rwlog_get_systemId() ({				\
  uint8_t gs_uid_ = 0;					\
  rw_status_t gs_rs_ = rw_instance_uid(&gs_uid_);	\
  RW_ASSERT(gs_rs_ == RW_STATUS_SUCCESS);		\
  gs_uid_;						\
})

typedef struct {
  uint32_t category_enum;
  char category_name[32];
} category_eval_map;

typedef struct {
  uint64_t attempts;
  uint64_t bootstrap_events;
  uint64_t filtered_events;
  uint64_t sent_events;
  uint64_t log_send_failed;
  uint64_t l2_strcmps;
  uint64_t dropped;
  uint64_t duplicate_suppression_drops;
  uint64_t shm_filter_invalid_cnt;
  uint64_t l1_filter_flag_updated;
  uint64_t l1_filter_passed;
} rwlog_stats;

typedef struct {
   rwlog_event_buf_t l_buf;
   int rwlog_file_fd;
   int rotation_serial;
} rwlog_tls_key_t;

/* Thread-specific data, global as opposed to per-context; global is good enough(tm) for the moment */
extern __thread struct rwlog_ctx_tls_s {
  uint64_t sequence;
  uint32_t tid;
  uint32_t last_flush_ticks;    /*last flush tick for buffered logs */
  struct timeval tv_cached;
  rwlog_tls_key_t *tls_key;  /* thread local key storage pointer */
  rwlog_stats stats;
  uint32_t rwlogd_ticks; /* flow control related */
  uint32_t log_count;   /* for flow control */
  uint32_t repeat_count;/* dup suppression  related*/
  uint32_t last_ev; 
  uint32_t last_line_no; 
  uint32_t last_tv_sec; 
  uint64_t last_cid; 
} rwlog_ctx_tls;


typedef char category_str_t[32];
typedef struct {
  category_str_t name;
  rwlog_severity_t severity;
  uint64_t bitmap;
  RwLog__YangEnum__OnOffType__E critical_info;
}rwlog_category_filter_t;

#define RWLOG_CONTEXT_MAGIC   0xfeeddeed
/*!
 * structure for rwlog context
 */
struct rwlog_ctx_s {
  uint32_t magic;
  rwlog_error_code_t error_code;
  gint ref_count;
  uint32_t version;
  char hostname[MAX_HOSTNAME_SZ];   /*! < Hostname of the VM/machine */
  char appname[MAX_TASKNAME_SZ];    /*! < Application name */
  pid_t pid;                        /*! < Process id */
  uuid_t vnf_id;
  int filedescriptor;
  int filter_shm_fd;
  uint32_t speculative_buffer_window;
  uint32_t last_rotation_tick;
  char *rwlog_filename;
  char *rwlog_shm;
  void *filter_memory;              /* Shared Memory Start address */
  rwlog_category_filter_t *category_filter; /* Category filter */
  bool local_shm;                   /* if true using local copy of filter shm */
  rwlog_stats stats;
};

#define RWLOG_MAGIC 0xabcdabcd
#define RWLOG_BLANK_MAGIC 0xabcddcba
typedef struct {
  uint32_t magic;
  uint32_t size_of_proto;
  union {
    struct {
      uint32_t tv_usec;
      uint32_t tv_sec;  /* Do not change this order */
    };
    uint64_t time;
  };
  rw_call_id_t cid;
  uint32_t common_params_offset;
  rw_schema_ns_and_tag_t schema_id;
  uint32_t log_category;
  category_str_t log_category_str;
  uint16_t log_severity;
  uint16_t critical_info;
  union callid_filter_set {
    struct cid_vals {
      uint32_t add_callid_filter:1;
      uint32_t invalidate_callid_filter:1;
      uint32_t next_call_callid:1;
      uint32_t failed_call_callid:1;
      uint32_t bootstrap:1;
      uint32_t binary_log:1;
    } cid_attrs;
    uint32_t reset;
  } cid_filter;
  char hostname[MAX_HOSTNAME_SZ];
  uuid_t vnf_id;
  uint32_t process_id;
  uint32_t thread_id;
  uint64_t seqno;
}  __attribute__((packed)) rwlog_hdr_t;

/* Strip the file path name for readability */
#define __SHORT_FORM_OF_FILE__                                          \
  (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__ )

/*!
 * Prepare common prefix for the logging message
 * The format is as follows
 * yyyy-mm-dd hh:mm:ss.sss <severity> 
 *  (<hostname>:<pid>:<tid>:<file>:<line>) - <category>:<message>
 *
 * NOTE: This is called from RWLOG_PRINTF. This is used for
 * debugging only. Normal operation would call RWLOG_PROTOBUF
 */
#define RWLOG_PREFIX(_ctx, _category_id, _category_name,                  \
                     _evt_id_external, _evt_name,                         \
                     _severity, _severity_str)                            \
{                                                                         \
  struct timeval _tv;                                                     \
  struct tm *_tm;                                                         \
  gettimeofday(&_tv, NULL);                                               \
  _tm = localtime(&_tv.tv_sec);                                           \
  fprintf(stdout,                                                         \
          "%d-%02d-%02d %02d:%02d:%02d.%03d %-6s "                        \
          "(%s@%s:%d:0x%08x:%s:%d) - ",                                   \
          _tm->tm_year+1900,                                              \
          _tm->tm_mon+1,                                                  \
          _tm->tm_mday,                                                   \
          _tm->tm_hour,                                                   \
          _tm->tm_min,                                                    \
          _tm->tm_sec,                                                    \
          (int)_tv.tv_usec/1000,                                          \
          (_severity_str),                                                \
          (_category_name),                                               \
          (_ctx)->hostname,                                               \
          (_ctx)->pid,                                                    \
          (uint32_t) pthread_self(),                                      \
          __SHORT_FORM_OF_FILE__,                                         \
          __LINE__                                                        \
          );                                                              \
}

/*!
 * Print the log message to the stdout
 *
 * NOTE: This is used for
 * debugging only. Normal operation would call RWLOG_PROTOBUF
 */
#define RWLOG_PRINTF(_ctx, _category_id, _category_name,                  \
                     _evt_id_internal, _evt_id_external, _evt_name,       \
                     _severity, _severity_str, _fmt, ...)                 \
{                                                                         \
  RWLOG_PREFIX((_ctx), (_category_id), (_category_name),                  \
               (_evt_id_external), (_evt_name),                           \
               (_severity), (_severity_str));                             \
  fprintf(stdout, (_fmt), ##__VA_ARGS__);                                 \
  fprintf(stdout, "\n");                                                  \
}

#define RWLOG_BUFFERED_EVENT(__ctx__, __rw__call__id__) \
   rwlog_callid_events_from_buf ((__ctx__), (__rw__call__id__),FALSE)

/*!
 * Top level event log macro to be invoked by application.
 *  
 * @param ctx_     - the log context 
 * @param path_    - the path of yang definition (module.category)
 * @params ...     - rest of the variable number of arguments for the macro
 * @return         - Expands through a series of meta data macros into logic for
 *                   doing L1/L2 filtering and calling C-API  to log the event
 */
#define RWLOG_EVENT(ctx_, path_,...) \
  RWLOG_EVENT_INTERNAL(ctx_,path_,((rw_call_id_t *)NULL),NULL,(/* code_before_event_ */),(/* code_after_event_ */),__VA_ARGS__);

#define RWLOG_EVENT_CODE(ctx_, path_, code_before_event_, code_after_event_,...) \
  RWLOG_EVENT_INTERNAL(ctx_,path_,((rw_call_id_t *)NULL),NULL,code_before_event_,code_after_event_,__VA_ARGS__);

/*!
 * Top level event log macro to be invoked by application for logs with callid.
 *  
 * @param ctx_     - the log context 
 * @param path_    - the path of yang definition (module.category)
 * @param callid_, - pointer to callid field of type rw_call_id_t as declared in
 *                   rwtypes.h 
 * @params ...    - rest of the variable number of arguments for the macro
 * @return         - Expands through a series of meta data macros into logic for
 *                   doing L1/L2 filtering and calling C-API  to log the event
 */
#define RWLOG_EVENT_CALLID(ctx_, path_, callid_,...) \
  RWLOG_EVENT_INTERNAL(ctx_,path_,callid_,NULL,(/* code_before_event */),(/* code_after_event */),__VA_ARGS__)

/*!
 * Top level event log macro to be invoked by application for logs with session
 * parameters for session based logging. Event log should be generated atleast
 * once with supported session params using this macro for session based
 * logging.
 *  
 * @param ctx_     - the log context 
 * @param path_    - the path of yang definition (module.category)
 * @param callid_, - pointer to callid field of type rw_call_id_t as declared in
 *                   rwtypes.h 
 * @param session_params_ -  Pointer to Yang derived symbol of type
 *                           RwLog_notif_EvTemplate_TemplateParams_SessionParams
 *                           with session params like imsi, ip-address filed                  
 * @params ...     - rest of the variable number of arguments for the macro
 * @return         - Expands through a series of meta data macros into logic for
 *                   doing L1/L2 filtering and calling C-API  to log the event
 */
#define RWLOG_EVENT_SESSION_PARAMS_WITH_CALLID(ctx_, path_, callid_,session_params_,...) \
    RWLOG_EVENT_INTERNAL(ctx_,path_,callid_,session_params_,(/* code_before_event */),(/* code_after_event */),__VA_ARGS__);

#define RWLOG_EVENT_INTERNAL(ctx_, path_,callid_,session_params_,code_before_event_,code_after_event_,...) \
do { \
  static uint32_t local_log_serial_no = 0; \
  filter_memory_header *mem_hdr = (filter_memory_header *)(ctx_)->filter_memory; \
  if (RW_UNLIKELY((callid_ && callid_->serial_no != mem_hdr->log_serial_no) || (local_log_serial_no != mem_hdr->log_serial_no))) { \
      bool deny_ev_ = FALSE;\
      uint64_t event_id_ = \
        RWPB_i_message_fmd_pbc(RWLog_impl_paste2(path_, TemplateParams), event_id, c_default_value); \
      RWLOG_IS_SET_DENY_ID_HASH(mem_hdr->static_bitmap,event_id_, deny_ev_); \
      if (RW_UNLIKELY(deny_ev_)) { \
        deny_ev_=rwlog_l2_exact_denyId_match(ctx_,event_id_);\
      }\
      if (RW_LIKELY(!deny_ev_)) {\
          RWLog_impl_remove_paren(code_before_event_); \
          const char *_category_str_ =  ((RWPB_G_PATHSPEC_YPBCD(path_))->module)->module_name; \
          RWPB_i_message_mmd_pbc(path_, t_pbcm) proto_; \
          RWPB_i_message_mmd_pbc(path_, f_pbc_init)(&proto_); \
          bool packet_info_present = false; \
          RWLog_impl_assign_all_flds_event1((ctx_), path_, proto_, __VA_ARGS__); \
          (proto_).has_template_params = 1; \
          rwlog_proto_send_c(ctx_, _category_str_,(ProtobufCMessage *)&proto_,callid_,(common_params_t *)(&proto_.template_params), __LINE__, (char *) __FILE__, session_params_,FALSE, packet_info_present, &local_log_serial_no); \
          RWLog_impl_remove_paren(code_after_event_); \
        }\
      } \
}while (0)

/*!
 * Initialize the rwlog context
 *
 * @param[in] taskname Pointer to the taskname string
 *
 * @return Pointer to the rwlog_ctx_t
 *
 */
rwlog_ctx_t *rwlog_init(const char *taskname);

/*!
 * Initialize the rwlog context
 *
 * @param[in] taskname Pointer to the taskname string
 * @param[in] vnf_id   VNF ID as uuid_t
 *
 * @return Pointer to the rwlog_ctx_t
 *
 */
rwlog_ctx_t *rwlog_init_with_vnf(const char *taskname,uuid_t vnf_id);


/*!
 * Internal API to Initialize the rwlog context
 *
 * @param[in] taskname Pointer to the taskname string
 * @param[in] rwlog_filename Pointer to the rwlog file string
 * @param[in] rwlog_shm_name Pointer to the shm name string
 * @param[in] vnf_id  UUID value
 *
 * @return Pointer to the rwlog_ctx_t
 *
 */
rwlog_ctx_t *rwlog_init_internal(const char *taskname,char *rwlog_filename,char *rwlog_shm_name,uuid_t vnf_id);

/* 
 * rwlog_update_appname 
 * @param: Pointer to the rwlog_ctx_t
 * @param: new_name Recommended: concat "component, Instance"
 *
 * @return : void
 */
void rwlog_update_appname(rwlog_ctx_t *ctx, const char *taskname);

/*!
 * This function get category type from the name
 * 
 * @param[in] name Pointer to the category name string
 *
 * @return Category type enum
 */
uint32_t rwlog_get_category(char *name);

/*!
 * Function to get the category name from category type
 *
 * @param[in] category Category type enum
 *
 * @return Pointer to the category name string
 */
char *rwlog_get_category_name(uint32_t category);

/*!
 * Function to set the logging filter based on evt_id
 * 
 * @param[in] ctx Pointer to the rwlog_ctx_t
 * @param[in] category type of the category
 * @param[in] evt_id to set the filter to
 * @param[in] evt_flag_mask can be one of enable, disable, rate-limit et al
 *
 * @retval RW_STATUS_SUCCESS Successfully updated the severity
 * @retval RW_STATUS_FAILURE Invalid evt_id and failed to update the severity

 * Example usage:
 * @code
 * @endcode
 */

/*!
 * Function to initialise shared memory bootstrap filter 
 * */
void rwlog_init_bootstrap_filters(char *shm_file_name);
void rwlog_shm_set_dup_events(char *shm_file_name, bool flag);

rw_status_t rwlog_set_event_filter(rwlog_ctx_t *ctx,
                                   uint32_t category, 
                                   uint32_t evt_id_internal,
                                   int evt_flag_mask);

/*!
 * Function to set the logging filter based on attributes
 * 
 * @param[in] ctx Pointer to the rwlog_ctx_t
 * @param[in] category type of the category
 * @param[in] key string representing attribute key
 * @param[in] value string representing attribute value
 *
 * @retval RW_STATUS_SUCCESS  Successfully updated the attribute filter.
 * @retval RW_STATUS_FAILURE  Max entries for attribute already configured
 * @retval RW_STATUS_NOTFOUND Failed to update attribute filter.

 * Example usage:
 * @code
 * @endcode
 */

rw_status_t rwlog_set_attribute_filter(rwlog_ctx_t *ctx,
                                       uint32_t category_id,
                                       const char *key,
                                       const char *value);

/*!
 * Close the rwlog context
 * @param[in] ctx Pointer to rwlog_ctx_t
 * @params[in] remove_logfile - Set to TRUE to remove log file used only for Gtest
 */
rw_status_t rwlog_close(rwlog_ctx_t *ctx,bool remove_logfile);

rw_status_t rwlog_close_internal(rwlog_ctx_t *ctx,bool remove_logfile);

/*!
 * Execute all source filtering on the given event.
 *
 * @param[in] ctx Pointer to the rwlog_ctx_t logging context handle
 *
 * @param[in] cat RW_LOG_LOG_CATEGORY_mumble category value
 * 
 * @param[in] proto Pointer to an event.  The event must be
 * type compatible with the EvTemplate base event.
 *
 * @return TRUE if the event passes the current filter set and should
 * be logged.
 */
int rwlog_proto_filter_l2(rwlog_ctx_t *ctx, 
			  const char *cat_str,
			  ProtobufCMessage *proto,
        rw_call_id_t *callid,
        common_params_t *common_params,
        uint32_t *local_log_serial_no);


/*!
 * Send the event to the local rwlogd.  Does no filtering; see
 * rwlog_proto_send() for a filter-and-send function.
 *
 * @param[in] ctx Pointer to the rwlog_ctx_t logging context handle
 *
 * @param[in] proto Pointer to an event.  The event must be
 * type compatible with the EvTemplate base event.
 *
 */
void rwlog_proto_send_filtered_logs_to_file(rwlog_ctx_t *ctx, 
                                 const char *cat, 
                                 ProtobufCMessage *proto, 
                                 rw_call_id_t *cid,
                                 uint8_t add_callid_filter,
                                 uint8_t next_call_callid,
                                 bool binary_log);

/*!
 * Buffer the callid event locally.  Does no filtering;
 *
 * @param[in] ctx Pointer to the rwlog_ctx_t logging context handle
 *
 * @param[in] proto Pointer to an event.  The event must be
 * type compatible with the EvTemplate base event.
 *
 */
rw_status_t rwlog_proto_buffer_unfiltered(rwlog_ctx_t *ctx, 
                                   const char *cat, 
                                   ProtobufCMessage *proto, 
                                   rw_call_id_t *callid);

/*!
 * Evaluate the given event against the current filterset and if it
 * passes, log it.  This API is available primarily for language
 * bindings which cannot incorporate the C preprocessor-defined L1
 * filter logic.
 *
 * @param[in] ctx Pointer to the rwlog_ctx_t logging context handle
 *
 * @param[in] cat Category of the event
 *
 * @param[in] proto Pointer to an event.  The event must be
 * type compatible with the EvTemplate base event.
 *
 * @param[in] filter_matched - set to 1, if set to 0 callid parameter should be non NULL
 *
 * @param[in] callid - pointer to call identifier struct for speculative logging
 *
 * @param[in] skip_filtering - skip any L1/L2 filtering in source
 *
 * @param[in] binary_log - true for binary logs 
 */
void rwlog_proto_send(rwlog_ctx_t *ctx, const char *cat_str, 
                      void *proto, int filter_matched, 
                      rw_call_id_t *callid, common_params_t *common_params,
                      bool skip_filtering, bool binary_log,
                      uint32_t *local_log_serial_no);

/*!
 * This API is available primarily for the C macro to call  
 * after initial L1 filtering. This does L2 deep filtering
 *
 * @param[in] ctx Pointer to the rwlog_ctx_t logging context handle
 *
 * @param[in] cat Category of the event
 *
 * @param[in] proto Pointer to an event.  The event must be
 * type compatible with the EvTemplate base event.
 *
 * @param[in] callid - pointer to call identifier struct for speculative logging
 *
 * @param[in] line- line number
 *
 * @param[in] file  - file name
 *
 * @param[in]  - session_params
 *
 * @param[in]  - skip any L1/L2 filtering in source
 */
void rwlog_proto_send_c(rwlog_ctx_t *ctx, 
                      const char *cat_str, 
                      ProtobufCMessage *proto, 
                      rw_call_id_t *callid,
                      common_params_t *common_params,
                      int line, char *file,
                      session_params_t *session_params,
                      bool skip_filtering, bool binary_log,
                      uint32_t *local_log_serial_no);

/**!
 * rwlog_proto_buffer: 
 * @ctx
 * @proto
 * Buffer the log event for speculative call id logging
 **/
void rwlog_proto_buffer(rwlog_ctx_t *ctx, const char  *cat_str, ProtobufCMessage *proto, rw_call_id_t *callid);

rw_status_t rwlog_app_filter_setup(rwlog_ctx_t *ctx);
void  rwlog_app_filter_dump(void *ctx);
void  rwlog_ctxt_dump(rwlog_ctx_t *ctx);
rw_status_t rwlog_app_filter_destroy();

rwlog_call_hash_t* rwlog_allocate_call_logs(rwlog_ctx_t *ctx, uint64_t callid,uint32_t call_arrived_time);
void rwlog_callid_event_write(rwlog_ctx_t *ctx, void *logdata, int logsz);
rw_status_t rwlog_callid_events_from_buf(rwlog_ctx_t *ctx, rw_call_id_t *sessid,uint8_t retain_entry);

/* API to check any speculative call log entry beyond buffer interval and flush
 * the same */
rw_status_t rwlog_check_and_flush_callid_events(rwlog_ctx_t *ctx, rwlog_event_buf_t *l_buf);

gint rw_ypbc_RwLog_LogCategory_gi_from_str(const char *enum_str, GError **error);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RWLOG_H__
