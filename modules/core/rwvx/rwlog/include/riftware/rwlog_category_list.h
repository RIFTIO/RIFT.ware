/* STANDARD_RIFT_IO_COPYRIGHT */
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
