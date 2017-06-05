/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file rwtrace_gi.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 02/15/2015
 * @brief GObject bindings for trace
 */

#ifndef __RWTRACE_GI_H__
#define __RWTRACE_GI_H__

#include <sys/cdefs.h>
#include <stdint.h>
#include <glib-object.h>
#include "rwtypes.h"

__BEGIN_DECLS

/**
 * rwtrace_severity_t:
 */
typedef enum {
  RWTRACE_SEVERITY_DISABLE,
  RWTRACE_SEVERITY_CRITINFO,      /* Critical information messages */
  RWTRACE_SEVERITY_EMERG,         /* System is unusable */
  RWTRACE_SEVERITY_ALERT,         /* Action must be taken immediately */
  RWTRACE_SEVERITY_CRIT,          /* Critical conditions */
  RWTRACE_SEVERITY_ERROR,         /* Error condition */
  RWTRACE_SEVERITY_WARN,          /* Warning condition */
  RWTRACE_SEVERITY_NOTICE,        /* Normal but significant condition */
  RWTRACE_SEVERITY_INFO,          /* Informational messages */
  RWTRACE_SEVERITY_DEBUG,         /* Debug messages */
} rwtrace_severity_t;

GType rwtrace_severity_get_type(void);

/**
 * rwtrace_destination_t:
 */
typedef enum {
  RWTRACE_DESTINATION_CONSOLE = 1,   /* Send the output to console */
  RWTRACE_DESTINATION_FILE = 2       /* Send the output to file */
} rwtrace_destination_t;

GType rwtrace_destination_get_type(void);

/**
 * rwtrace_category_t:
 */
typedef enum {
  RWTRACE_CATEGORY_RWTASKLET,
  RWTRACE_CATEGORY_RWCAL,
  RWTRACE_CATEGORY_RWSCHED,
  RWTRACE_CATEGORY_RWZK,
  RWTRACE_CATEGORY_RWMSG,
  RWTRACE_CATEGORY_RWCLI,
  RWTRACE_CATEGORY_RWNCCLNT,
  RWTRACE_CATEGORY_RATEST,
  RWTRACE_CATEGORY_LAST
} rwtrace_category_t;

GType rwtrace_category_get_type(void);

/**
 * rwtrace_ctx_t:
 */
typedef struct _rwtrace_ctx rwtrace_ctx_t;
rwtrace_ctx_t *rwtrace_ctx_new(void);
GType rwtrace_ctx_get_type(void);

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
void rwtrace_ctx_trace(rwtrace_ctx_t *ctx,
                       rwtrace_category_t category,
                       rwtrace_severity_t severity,
                       char *message);

/*!
 * Function to close the tracing
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 */
void rwtrace_ctx_close(rwtrace_ctx_t *ctx);

/*!
 * This function sets the severity for the tracing category
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 * @param[in] category Category type
 * @param[in] severity Severity type
 *
 * @return Status of operation
 */
/// @cond GI_SCANNER
/**
 * rwtrace_ctx_category_severity_set:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @cond
rw_status_t rwtrace_ctx_category_severity_set(rwtrace_ctx_t *ctx,
                                              rwtrace_category_t category,
                                              rwtrace_severity_t severity);

/*!
 * This function sets the destination for the trace category
 *
 * @param[in] ctx Pointer to the rwtrace_ctx_t
 * @param[in] category Category type
 * @param[in] dest Destination type
 *
 * @return Status of operation
 */
/// @cond GI_SCANNER
/**
 * rwtrace_ctx_category_destination_set:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @cond
rw_status_t rwtrace_ctx_category_destination_set(rwtrace_ctx_t *ctx,
                                                 rwtrace_category_t category,
                                                 rwtrace_destination_t dest);



__END_DECLS

#endif // __RWTRACE_H__
