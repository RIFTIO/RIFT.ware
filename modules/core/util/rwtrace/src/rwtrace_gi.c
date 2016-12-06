
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
