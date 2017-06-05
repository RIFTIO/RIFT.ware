/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file   rwtypes_gi.c
 * @author Rajesh Velandy (rajesh.velandy@riftio.com)
 * @date   01/13/2015
 * @brief  GI supported common types
 * 
 * @details  Contains the types that are exported through Gi
 *
 */
#include <rwtypes_gi.h>

static const GEnumValue _rw_status_enum_values[] = {
  { RW_STATUS_SUCCESS, "SUCCESS", "SUCCESS" },
  { RW_STATUS_FAILURE, "FAILURE", "FAILURE" },
  { RW_STATUS_DUPLICATE, "DUPLICATE", "DUPLICATE" },
  { RW_STATUS_NOTFOUND, "NOTFOUND", "NOTFOUND" },
  { RW_STATUS_OUT_OF_BOUNDS, "OUT_OF_BOUNDS", "OUT_OF_BOUNDS" },
  { RW_STATUS_BACKPRESSURE, "BACKPRESSURE", "BACKPRESSURE" },
  { RW_STATUS_TIMEOUT, "TIMEOUT", "TIMEOUT" },
  { RW_STATUS_EXISTS, "EXISTS", "EXISTS" },
  { RW_STATUS_NOTEMPTY, "NOTEMPTY", "NOTEMPTY" },
  { RW_STATUS_NOTCONNECTED, "NOTCONNECTED", "NOTCONNECTED" },
  { RW_STATUS_PENDING, "PENDING", "PENDING" },
  { RW_STATUS_NO_RESPONSE, "NO_RESPONSE", "NO_RESPONSE" },
  { RW_STATUS_READONLY, "READONLY", "READONLY" },
  { RW_STATUS_AUDIT_FAILED, "AUDIT_FAILED", "AUDIT_FAILED" },
  { RW_STATUS_READONLY, "NOT_IMPLEMENTED", "NOT_IMPLEMENTED" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwtypes_rw_status_get_type()
 */
GType rwtypes_rw_status_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rw_status_t", _rw_status_enum_values);

  return type;
}

