/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file   rwtypes_gi.h
 * @author Rajesh Velandy (rajesh.velandy@riftio.com)
 * @date   01/13/2015
 * @brief  GI supported common types
 * 
 * @details  Contains the types that are exported through Gi
 *
 */
#ifndef __RWTYPES_GI_H__
#define __RWTYPES_GI_H__

#include <sys/cdefs.h>
#include <stdint.h>
#include <glib-object.h>

__BEGIN_DECLS

/**
 * rw_status_t:
 */
typedef enum {
  RW_STATUS_SUCCESS       = 0, /* Used to indicate a successful operation */
  RW_STATUS_FAILURE       = 1, /* Used to indicate a failed operation */
  RW_STATUS_DUPLICATE     = 2, /* Used to indicate a duplicate operation */
  RW_STATUS_NOTFOUND      = 3, /* Used to indicate that data was not found */
  RW_STATUS_OUT_OF_BOUNDS = 4, /* Used to indicate that limits exceeded */
  RW_STATUS_BACKPRESSURE  = 5, /* Used to indicate backpressure, typically EAGAIN */
  RW_STATUS_TIMEOUT       = 6, /* Used to indicate a timeout */
  RW_STATUS_EXISTS        = 7, /* Used to indicate that entry already exists */
  RW_STATUS_NOTEMPTY      = 8, /* Used to indicate that entry is not empty */
  RW_STATUS_NOTCONNECTED  = 9, /* Used to indicate that item is not connected */
  RW_STATUS_PENDING       = 10, /* Used to indicate that operation is not complete */
  RW_STATUS_NO_RESPONSE   = 11, /* Used to indicate that no response for request */
  RW_STATUS_READONLY      = 12, /* Used to indicate that the resource is readonly */
  RW_STATUS_AUDIT_FAILED  = 13,
  RW_STATUS_NOT_IMPLEMENTED = 14,
} rw_status_t;

GType rwtypes_rw_status_get_type(void);

#ifdef __GI_SCANNER__ 
#define bool_t gboolean
#define _Bool gboolean
#endif

__END_DECLS

#endif /* __RWTYPES_GI_H__ */
