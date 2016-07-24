
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rwtrace.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/07/2013
 * @brief Top level include for RIFT tracing utilities.
 *
 * @details Tracing can be used tasklets to generate low overhead messages.
 * The tracing can be controlled by setting the severity level.
 * The RWTRACE supports seven severity levels (compatible with
 * syslog). Tracing can be completey disabled by setting the
 * severity level to RWTRACE_SEVERITY_DISABLE.
 *
 * Applications should use the RWTRACE macro to generate traces. The
 * following example shows an example of how tracing library can be used.
 *
 * @code
 * include "rwtrace.h"
 *
 * rw_status_t status;
 * rwtrace_ctx_t *ctx = rwtrace_init();
 * status = rwtrace_ctx_category_severity_set(ctx,
 *                                        RWTRACE_CATEGORY_SCHED,
 *                                        RWTRACE_SEVERITY_DEBUG);
 * ASSERT_EQ(status, RW_STATUS_SUCCESS);
 *
 * status = rwtrace_ctx_category_destination_set(ctx,
 *                                           RWTRACE_CATEGORY_SCHED,
 *                                           RWTRACE_DESTINATION_CONSOLE);
 *
 * ASSERT_EQ(status, RW_STATUS_SUCCESS);
 *
 * ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
 * RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_SCHED, "Testing RWTRACE_DEBUG");
 * @endcode
 */

#ifndef __RWTRACE_H__
#define __RWTRACE_H__

/* Specify the includes here */
#include "rwlib.h"
#include <sys/types.h>
#include <unistd.h>
#include "rwtrace_gi.h"

__BEGIN_DECLS

/*!
 * Check the category bounds
 */
#define RWTRACE_IS_VALID_CATEGORY(_c)           \
  (_c < RWTRACE_CATEGORY_LAST)

/*!
 * Check the severity bounds
 */
#define RWTRACE_IS_VALID_SEVERITY(_s)                                   \
  ((_s <= RWTRACE_SEVERITY_DEBUG))

/*!
 * This structure holds the category specific tracing
 * information.
 */
typedef struct {
  rwtrace_severity_t severity;
  rwtrace_destination_t dest;
  char *filename;
  rw_code_location_info_t location_info;
} rwtrace_category_info_t;

#define MAX_TRACE_HOSTNAME_SZ 64
#define MAX_TRACE_TASKNAME_SZ 64

/*!
 * The structure to store the tracing information
 */
struct _rwtrace_ctx {
  rwtrace_category_info_t category[RWTRACE_CATEGORY_LAST];
  char hostname[MAX_TRACE_HOSTNAME_SZ];   /*!< Hostname of the VM/machine */
  char appname[MAX_TRACE_TASKNAME_SZ];    /*!< Application name */
  int pid;
  FILE *fileh;
  int ref_count;
};

#define RWTRACE_ENABLE

#ifdef RWTRACE_ENABLE

/* Strip the file path name for readability */
#define __SHORT_FORM_OF_FILE__                                              \
  (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__ )

/*! 
 * This macro generates common prefix for the trace.
 * The format is as follows
 * yyyy-mm-dd hh:mm:ss.sss <severity> 
 *  (<hostname>:<pid>:<tid>:<file>:<line>) - <category>:<message>
 */
#define RWTRACE_EMIT_PREFIX(_ctx, _severity_str, _category)                 \
{                                                                           \
  struct timeval _tv;                                                       \
  struct tm *_tm;                                                           \
  gettimeofday(&_tv, NULL);                                                 \
  _tm = localtime(&_tv.tv_sec);                                             \
  if ((_ctx)->fileh) fprintf((_ctx)->fileh,                                 \
          "%d-%02d-%02d %02d:%02d:%02d.%d %-6s "                            \
          "(%s-trace@%s:%d:0x%08x:%s:%d) - ",                               \
          _tm->tm_year+1900,                                                \
          _tm->tm_mon+1,                                                    \
          _tm->tm_mday,                                                     \
          _tm->tm_hour,                                                     \
          _tm->tm_min,                                                      \
          _tm->tm_sec,                                                      \
          (int)_tv.tv_usec/1000,                                            \
          _severity_str,                                                    \
          rwtrace_get_category_name(_category),                             \
          (_ctx)->hostname,                                                 \
          (_ctx)->pid,                                                      \
          (uint32_t) pthread_self(),                                        \
          __SHORT_FORM_OF_FILE__,                                           \
          __LINE__                                                          \
          );                                                                \
}

#define RWTRACE_EMIT(_ctx, _category, _severity_str, _fmt, ...)             \
{                                                                           \
  if (_ctx->category[_category].dest & RWTRACE_DESTINATION_CONSOLE) {       \
    RWTRACE_EMIT_PREFIX((_ctx), (_severity_str), (_category));              \
    if ((_ctx)->fileh) {  \
        fprintf((_ctx)->fileh, _fmt "\n", ##__VA_ARGS__);			    \
        fflush((_ctx)->fileh);                                             \
    }                                                                      \
    else {                                                                 \
        printf(_fmt "\n", ##__VA_ARGS__);			                             \
    }                                                                      \
  }                                                                        \
}

/*!
 * Main macro for genarating the traces
 */
#define RWTRACE(_ctx, _category, _serverity, _severity_str, _fmt, ...)      \
{                                                                           \
  RW_ASSERT(_category < RWTRACE_CATEGORY_LAST);                             \
  rw_code_location_info_t *_info = &_ctx->category[_category].location_info;\
  if (_ctx->category[_category].severity >= _serverity) {                   \
    RWTRACE_EMIT(_ctx, _category, _severity_str, _fmt, ##__VA_ARGS__);      \
  } else if (_info->location_hash & (1 << (__LINE__ & 0x1F))) {             \
    uint32_t _i;                                                            \
    for (_i = 0; _i < _info->location_count; ++_i) {                        \
      if ((_info->location[_i].lineno == __LINE__) &&                       \
          (strstr(__FILE__, _info->location[_i].filename))) {               \
        RWTRACE_EMIT(_ctx, _category, _severity_str, _fmt, ##__VA_ARGS__);  \
      }                                                                     \
    }                                                                       \
  }                                                                         \
}

/*
 * Wrappers for RWTRACE macro
 */

#define RWTRACE_DEBUG(_ctx, _category, _fmt, ...)                      \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_DEBUG, "DEBUG",          \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_INFO(_ctx, _category, _fmt, ...)                       \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_INFO, "INFO",            \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_CRITINFO(_ctx, _category, _fmt, ...)                       \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_CRITINFO, "CRIT-INFO",            \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_NOTICE(_ctx, _category, _fmt, ...)                     \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_NOTICE, "NOTICE",        \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_WARN(_ctx, _category, _fmt, ...)                       \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_WARN, "WARN",            \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_ERROR(_ctx, _category, _fmt, ...)                      \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_ERROR, "ERROR",          \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_CRIT(_ctx, _category, _fmt, ...)                       \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_CRIT, "CRIT",            \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_ALERT(_ctx, _category, _fmt, ...)                      \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_ALERT, "ALERT",          \
           _fmt, ##__VA_ARGS__);

#define RWTRACE_EMERG(_ctx, _category, _fmt, ...)                      \
  RWTRACE((_ctx), _category, RWTRACE_SEVERITY_EMERG, "EMERG",          \
           _fmt, ##__VA_ARGS__);

#else

/* All the trace routines are noops */
#define RWTRACE_DEBUG(_category, _fmt, ...)
#define RWTRACE_INFO(_category, _fmt, ...)
#define RWTRACE_CRITINFO(_category, _fmt, ...)
#define RWTRACE_NOTICE(_category, _fmt, ...)
#define RWTRACE_WARN(_category, _fmt, ...)
#define RWTRACE_ERROR(_category, _fmt, ...)
#define RWTRACE_CRIT(_category, _fmt, ...)
#define RWTRACE_ALERT(_category, _fmt, ...)
#define RWTRACE_EMERG(_category, _fmt, ...)

#endif // RWTRACE_ENABLE

//#define RWTRACE_PKT_ENABLE

#ifdef RWTRACE_PKT_ENABLE
#define RWTRACE_PKT_INFO(m, _rw_instance, _category, _fmt, ...)                \
  if (RW_VF_VALID_MDATA_TRACE_ENA(m)) { \
    if (!_rw_instance) {\
      printf(_fmt, ##__VA_ARGS__); \
    } else {\
      RWTRACE((((rwfpath_instance_ptr_t)_rw_instance)->rwtasklet_info->rwtrace_instance), _category, RWTRACE_SEVERITY_INFO, "INFO",            \
             _fmt, ##__VA_ARGS__) \
    }\
 };

#else

#define RWTRACE_PKT_INFO(_category, _fmt, ...)

#endif // RWTRACE_PKT_ENABLE

/*!
 * Function to initialize tracing
 *
 * @return Pointer to the rwtrace_ctx_t
 */
rwtrace_ctx_t *rwtrace_init(void);

/*!
 * Function to close the tracing
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 */
void rwtrace_ctx_close(rwtrace_ctx_t *ctx);

/*!
 * The function returns tracing category name from category type
 *
 * @param[in] category Category type
 *
 * @return Pointer to the caterory name string
 */
char *rwtrace_get_category_name(rwtrace_category_t category);

/*!
 * The function returns tracing destination name
 *
 * @param[in] dest Destination type
 *
 * @return Pointer to the destination string
 */
char *rwtrace_get_destination_name(rwtrace_destination_t dest);

/*!
 * This function unsets the destination for the trace category
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 * @param[in] category Category type
 * @param[in] dest Destination type
 *
 * @return Status of operation
 */
rw_status_t rwtrace_unset_category_destination(rwtrace_ctx_t *ctx,
                                               rwtrace_category_t c,
                                               rwtrace_destination_t dest);
/*!
 * This function sets the trace location that must emit the trace irrespective
 * of severity level
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 * @param[in] category Category type
 * @param[in] location Pointer to the location of the trace (filename and
 * lineno) 
 *
 * @return Status of operation
 */
rw_status_t rwtrace_set_trace_location(rwtrace_ctx_t *ctx,
                                       rwtrace_category_t category,
                                       rw_code_location_t *location);

/*!
 * This function gets the number of trace locations configured
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 * @param[in] category Category type
 *
 * @return Number of trace locations configured
 */
int rwtrace_get_trace_location_count(rwtrace_ctx_t *ctx,
                                     rwtrace_category_t category);

/*!
 * This function unsets the trace location that we set by
 * rwtrace_set_force_enable_location
 *
 * @param[in] ctx The pointer to the rwlog context.
 * @param[in] category Category type
 * @param[in] location Pointer to the location of the trace (filename and
 * lineno) 
 *
 * @return Status of operation
 */
rw_status_t rwtrace_unset_trace_location(rwtrace_ctx_t *ctx,
                                         rwtrace_category_t category,
                                         rw_code_location_t *location);

/*! 
 * This function generates a trace message.
 * NOTE: This is not the most efficent way to invoke tracing.
 * This function is intended to be invoked by the plugin
 *
 * @param[in] ctx The pointer to the rwlog context.
 * @param[in] category Category type
 * @param[in] severity Severity type
 * @param[in] message Trace message
 */
void rwtrace_trace(rwtrace_ctx_t *ctx, 
                   rwtrace_category_t category, 
                   rwtrace_severity_t severity, 
                   char *message);

/*!
 * This function returns a string from the severity type
 *
 * @param[in] severity Severity type
 */
const char *rwtrace_get_severity_string(rwtrace_severity_t severity);
    
__END_DECLS

#endif // __RWTRACE_H__
