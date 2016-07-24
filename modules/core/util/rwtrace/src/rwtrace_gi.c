
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#include "rwtrace_gi.h"

static const GEnumValue _rwtrace_severity_enum_values[] = {
  { RWTRACE_SEVERITY_DISABLE, "DISABLE", "DISABLE" },
  { RWTRACE_SEVERITY_CRITINFO, "CRITINFO", "CRITINFO" },      
  { RWTRACE_SEVERITY_EMERG, "EMERG", "EMERG" },
  { RWTRACE_SEVERITY_ALERT, "ALERT", "ALERT" },
  { RWTRACE_SEVERITY_CRIT, "CRIT", "CRIT" },
  { RWTRACE_SEVERITY_ERROR, "EMERG", "EMERG" },
  { RWTRACE_SEVERITY_WARN, "WARN", "WARN" },
  { RWTRACE_SEVERITY_NOTICE, "NOTICE", "NOTICE" },
  { RWTRACE_SEVERITY_INFO, "INFO", "INFO" },
  { RWTRACE_SEVERITY_DEBUG, "DEBUG", "DEBUG" },
  { 0, NULL, NULL}
};


/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwtrace_severity_get_type()
 */
GType rwtrace_severity_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type) {
    type = g_enum_register_static("rwtrace_severity_t", 
                                  _rwtrace_severity_enum_values);
  }

  return type;
}

static const GEnumValue _rwtrace_destination_enum_values[] = {
  { RWTRACE_DESTINATION_CONSOLE, "CONSOLE", "CONSOLE" },
  { RWTRACE_DESTINATION_FILE, "FILE", "FILE" },
  { 0, NULL, NULL }
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwtrace_destination_get_type()
 */
GType rwtrace_destination_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type) {
    type = g_enum_register_static("rwtrace_destination_t", 
                                  _rwtrace_destination_enum_values);
  }

  return type;
}

static const GEnumValue _rwtrace_category_enum_values[] = {
  { RWTRACE_CATEGORY_RWTASKLET, "", "" },
  { RWTRACE_CATEGORY_RWCAL, "", "" },
  { RWTRACE_CATEGORY_RWSCHED, "", "" },
  { RWTRACE_CATEGORY_RWZK, "", "" },
  { RWTRACE_CATEGORY_RWMSG, "", "" },
  { RWTRACE_CATEGORY_RWCLI, "", "" },
  { RWTRACE_CATEGORY_RWNCCLNT, "", "" },
  { RWTRACE_CATEGORY_RATEST, "", "" },
  { RWTRACE_CATEGORY_LAST, "", "" },
  { 0, NULL, NULL }
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines rwtrace_category_get_type()
 */
GType rwtrace_category_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type) {
    type = g_enum_register_static("rwtrace_category_t", 
                                  _rwtrace_category_enum_values);
  }

  return type;
}
