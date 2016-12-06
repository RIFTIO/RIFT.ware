
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


/**
 * @file rwlog_category_list.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 11/07/2013
 * @brief Top level include for RIFT logging categories
 *
 * @details Includes the approriate logging categories. 
 * New categories should be added to this list. This file is shared
 * with rlog library as well as the rlog event parser tool. DONOT add
 * any RIFT specifuc stuff here as parser tool needs to compile standalone.
 */

#ifndef __RWLOG_CATEGORY_LIST_H__
#define __RWLOG_CATEGORY_LIST_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // For strlen() definition

#ifndef __GI_SCANNER__
#include "rw-log.pb-c.h"
#endif /* __GI_SCANNER__ */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GI_SCANNER__
typedef RwLog__YangEnum__LogSeverity__E rwlog_severity_t;
#else
/* ATTN: These are ugly hacks. These require proper enum support in yang-derived GI. */
typedef enum {rwlog_severity_t_bogus} rwlog_severity_t;
#endif /* __GI_SCANNER__ */

#ifdef __cplusplus
}
#endif
#endif // __RWLOG_CATEGORY_LIST_H__
