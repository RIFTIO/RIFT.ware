
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



/*!
 * @file rwlog_gi.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 01/02/2015
 * @brief Include file for providing GI bindings
 **/

#ifndef __RWLOG_GI_H__
#define __RWLOG_GI_H__

#include "rwtypes_gi.h"
#include "protobuf-c/rift-protobuf-c.h"
#include <glib.h>
#include <glib-object.h>
#include "rwlog_category_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rw_call_id_s rw_call_id_t;
typedef struct rwlog_ctx_s rwlog_ctx_t;
typedef struct RwLog__YangData__RwLog__EvTemplate__TemplateParams common_params_t;

// cruft for gobj
rwlog_ctx_t *rwlog_ctx_new(const char *taskname);
GType rwlog_ctx_get_type(void);

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
 */
/// @cond GI_SCANNER
/**
 * rwlog_proto_send: (method):
 * @ctx
 * @cat
 * @proto
 * @filter_matched
 * @callid
 **/
/// @endcond
void rwlog_proto_send(rwlog_ctx_t *ctx, const char *cat, void *proto, int filter_matched, rw_call_id_t *callid,common_params_t *common_params,bool skip_filtering, bool binary_log,uint32_t *local_log_serial_no);

/// @cond GI_SCANNER
/**
 * rwlog_get_log_serial_no : (method):
 * @ctx
 *
 * Returns: (type uint32_t) (transfer none)
 **/
/// @endcond
uint32_t rwlog_get_log_serial_no(rwlog_ctx_t *ctx);

/// @cond GI_SCANNER
/**
 * rwlog_get_category_severity_level : (method):
 * @ctx
 * @category
 *
 * Returns: (type uint32_t) (transfer none)
 **/
/// @endcond
uint32_t rwlog_get_category_severity_level(rwlog_ctx_t *ctx,const char *category);


/// @cond GI_SCANNER
/**
 * rwlog_get_shm_filter_name : (method):
 * @ctx
 * Returns: (type char*) (transfer none)
 **/
/// @endcond
char *rwlog_get_shm_filter_name(rwlog_ctx_t *ctx);


/// @cond GI_SCANNER
/**
 * rwlog_proto_send_gi: (rename-to rwlog_proto_send) (method):
 * @ctx
 * @proto
 **/
/// @endcond
void rwlog_proto_send_gi(rwlog_ctx_t *ctx, gpointer proto);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __RWLOG_GI_H__
