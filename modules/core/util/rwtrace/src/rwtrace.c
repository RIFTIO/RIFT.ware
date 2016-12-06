
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



#include "rwtrace.h"

static const GEnumValue _rwtrace_severity_enum_values[] = {
  { RWTRACE_SEVERITY_DISABLE, "DISABLE", "DISABLE" },
  { RWTRACE_SEVERITY_CRITINFO, "CRITINFO", "CRITINFO" },      
  { RWTRACE_SEVERITY_EMERG, "EMERG", "EMERG" },
  { RWTRACE_SEVERITY_ALERT, "ALERT", "ALERT" },
  { RWTRACE_SEVERITY_CRIT, "CRIT", "CRIT" },
  { RWTRACE_SEVERITY_ERROR, "ERROR", "ERROR" },
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
  { RWTRACE_CATEGORY_RWTASKLET, "RWTASKLET", "RWTASKLET" },
  { RWTRACE_CATEGORY_RWCAL, "RWCAL", "RWCAL" },
  { RWTRACE_CATEGORY_RWSCHED, "RWSCHED", "RWSCHED" },
  { RWTRACE_CATEGORY_RWZK, "RWZK", "RWZK" },
  { RWTRACE_CATEGORY_RWMSG, "RWMSG", "RWMSG" },
  { RWTRACE_CATEGORY_RWCLI, "RWCLI", "RWCLI" },
  { RWTRACE_CATEGORY_RWNCCLNT, "RWNCCLNT", "RWNCCLNT" },
  { RWTRACE_CATEGORY_RATEST, "RATEST", "RATEST" },
  { RWTRACE_CATEGORY_LAST, "LAST", "LAST" },
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

static rwtrace_ctx_t* rwtrace_ctx_ref(rwtrace_ctx_t *boxed) {
  g_atomic_int_inc(&boxed->ref_count);
  return boxed;
}

static void rwtrace_ctx_unref(rwtrace_ctx_t *boxed) {
  if (!g_atomic_int_dec_and_test(&boxed->ref_count)) {
    return;
  }
  return;
}

rwtrace_ctx_t *rwtrace_ctx_new(void)
{
  rwtrace_ctx_t *ctx = rwtrace_init();
  ctx->ref_count = 1;
  return ctx;
}

G_DEFINE_BOXED_TYPE(rwtrace_ctx_t, rwtrace_ctx, rwtrace_ctx_ref, rwtrace_ctx_unref);

/*
 * This function returns tracing category name from category type
 */
char *rwtrace_get_category_name(rwtrace_category_t category)
{
  switch(category) {
  case RWTRACE_CATEGORY_RWTASKLET:
    return "rwtasklet";
  case RWTRACE_CATEGORY_RWCAL:
    return "rwcal";
  case RWTRACE_CATEGORY_RWSCHED:
    return "rwsched";
  case RWTRACE_CATEGORY_RWZK:
    return "rwzk";
  case RWTRACE_CATEGORY_RWMSG:
    return "rwmsg";
  case RWTRACE_CATEGORY_RWCLI:
    return "rwcli";
  case RWTRACE_CATEGORY_RWNCCLNT:
    return "rwncclnt";
  case RWTRACE_CATEGORY_RATEST:
    return "ratest";
  default: 
    return "unknown";
  } 
}

/*
 * This function returns tracing destination name
 */
char *rwtrace_get_destination_name(rwtrace_destination_t dest)
{
  switch(dest) {
  case RWTRACE_DESTINATION_CONSOLE: 
    return "console";
  case RWTRACE_DESTINATION_FILE:
    return "file";
  default: 
    return "unknown";
  } 
}

/*
 * This function gets the trace location count
 */
int rwtrace_get_trace_location_count(rwtrace_ctx_t *ctx,
                                     rwtrace_category_t category)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return -1;
  }

  RW_ASSERT(ctx);

  return rw_get_code_location_count(&ctx->category[category].location_info);
}

/*
 * This function sets the trace location that must emit the trace irrespective
 * of trace level
 */
rw_status_t rwtrace_set_trace_location(rwtrace_ctx_t *ctx,
                                       rwtrace_category_t category,
                                       rw_code_location_t *location)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return RW_STATUS_FAILURE;
  }

  RW_ASSERT(ctx);

  return rw_set_code_location(&ctx->category[category].location_info,
                              location);
}

/*
 * This function unsets the trace location that we set by
 * rwtrace_set_force_enable_location
 */
rw_status_t rwtrace_unset_trace_location(rwtrace_ctx_t *ctx,
                                         rwtrace_category_t category,
                                         rw_code_location_t *location)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return RW_STATUS_FAILURE;
  }
  
  RW_ASSERT(ctx);

  return rw_unset_code_location(&ctx->category[category].location_info,
                                location);
}

/*
 * This function sets the severity for the trace category
 */
rw_status_t rwtrace_ctx_category_severity_set(rwtrace_ctx_t *ctx,
                                              rwtrace_category_t category,
                                              rwtrace_severity_t severity)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return RW_STATUS_FAILURE;
  }

  if (!RWTRACE_IS_VALID_SEVERITY(severity)) {
    return RW_STATUS_FAILURE;
  }

  if (ctx == NULL) {
    return RW_STATUS_FAILURE;
  }

  ctx->category[category].severity = severity;

  return RW_STATUS_SUCCESS;
}

/*
 * This function sets the destination for the trace category
 */
rw_status_t rwtrace_ctx_category_destination_set(rwtrace_ctx_t *ctx,
                                                 rwtrace_category_t category,
                                                 rwtrace_destination_t dest)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return RW_STATUS_FAILURE;
  }

  if (ctx == NULL) {
    return RW_STATUS_FAILURE;
  }

  if (dest == RWTRACE_DESTINATION_CONSOLE) {
    ctx->category[category].dest |= dest;
  } else {
    /* LOG */
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * This function unsets the destination for the trace category
 */
rw_status_t rwtrace_unset_category_destination(rwtrace_ctx_t *ctx,
                                               rwtrace_category_t category,
                                               rwtrace_destination_t dest)
{
  if (!RWTRACE_IS_VALID_CATEGORY(category)) {
    return RW_STATUS_FAILURE;
  }

  if (ctx == NULL) {
    return RW_STATUS_FAILURE;
  }

  if (dest == RWTRACE_DESTINATION_CONSOLE) {
    ctx->category[category].dest &= (~dest);
  } else {
    /* LOG */
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * This function initializes tracing. Every process
 * using the tracing functionalty is expected to call this once.
 */
rwtrace_ctx_t *rwtrace_init(void)
{
  rwtrace_ctx_t *ctx;
  ctx = RW_MALLOC0(sizeof(rwtrace_ctx_t));
  RW_ASSERT(ctx);

  /* Initialize hostname, pid and application name 
   * These don't change during the life of the process 
   * NOTE: hostname may change, we need to have a way to update 
   * the hostname if it changes 
   */
  RW_ASSERT(gethostname(ctx->hostname, MAX_TRACE_HOSTNAME_SZ-1) == 0);
  ctx->hostname[MAX_TRACE_HOSTNAME_SZ-1] = '\0';
  ctx->pid = getpid();

  /* Make up our own FILE* for stdout, so that we get our own buffer
     and control over buffering mode.  Having a handle per thread then
     gives us much higher liklihood of individual trace lines not
     being interspersed with those from other threads, since they'll
     now be written with one call to write(2).  However at present
     there is only one ctx and handle. */
  ctx->fileh = fdopen(STDOUT_FILENO, "a");
  setvbuf(ctx->fileh, NULL, _IOFBF, 2048);

  return ctx;
}

void rwtrace_ctx_close(rwtrace_ctx_t *ctx)
{
  if (ctx) {
    // don't close, that closes stdout, too! fclose(ctx->fileh);
    // can't dup(stdout) since that would break redirection to > file
    ctx->fileh = NULL;
    RW_FREE(ctx);
  }
  return;
}

const char *rwtrace_get_severity_string(rwtrace_severity_t severity)
{
  switch (severity) {
  case RWTRACE_SEVERITY_EMERG:
    return "EMERG";
  case RWTRACE_SEVERITY_ALERT:
    return "ALERT";
  case RWTRACE_SEVERITY_CRIT:
    return "CRIT";
  case RWTRACE_SEVERITY_ERROR:
    return "ERROR";
  case RWTRACE_SEVERITY_WARN:
    return "WARN";
  case RWTRACE_SEVERITY_NOTICE:
    return "NOTICE";
  case RWTRACE_SEVERITY_CRITINFO:
    return "CRIT-INFO";
  case RWTRACE_SEVERITY_INFO:
    return "INFO";
  case RWTRACE_SEVERITY_DEBUG:
    return "DEBUG";
  default:
    return "UNKNOWN";
  }
}

/* This is not the most efficent way to invoke tracing.
 * This function is intended to be invoked by the plugin
 */
void rwtrace_ctx_trace(rwtrace_ctx_t *ctx, 
                       rwtrace_category_t category, 
                       rwtrace_severity_t severity, 
                       char *message)
{
  const char *severity_str = rwtrace_get_severity_string(severity);
  RWTRACE(ctx, category, severity, severity_str, "%s", message)
}


