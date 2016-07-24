
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*!
 * @file yangpb_gi.h
 * @author Tom Seidenberg
 * @date 2015/04/08
 * @brief GI support for yang_pb data structures
 */

#ifndef RW_YANGTOOLS_YANGPB_GI_H_
#define RW_YANGTOOLS_YANGPB_GI_H_


#include <sys/cdefs.h>
#include <glib-object.h>

#ifndef __GI_SCANNER__ 
#include "rwlib.h"
#include "rwtypes.h"
#endif


__BEGIN_DECLS

typedef struct rw_yang_path_element_t rw_yang_path_element_t;
//GType rw_yang_path_element_get_type(void);

typedef struct rw_yang_pb_group_root_t rw_yang_pb_group_root_t;
//GType rw_yang_pb_group_root_get_type(void);

typedef struct rw_yang_pb_module_t rw_yang_pb_module_t;
//GType rw_yang_pb_module_get_type(void);

typedef struct rw_yang_pb_rpcdef_t rw_yang_pb_rpcdef_t;
//GType rw_yang_pb_rpcdef_get_type(void);

typedef union rw_yang_pb_msgdesc_union_t rw_yang_pb_msgdesc_union_t;
//GType rw_yang_pb_msgdesc_union_get_type(void);

typedef struct rw_yang_pb_msgdesc_t rw_yang_pb_msgdesc_t;
//GType rw_yang_pb_msgdesc_get_type(void);

typedef struct rw_yang_pb_enumdesc_t rw_yang_pb_enumdesc_t;
//GType rw_yang_pb_enumdesc_get_type(void);

typedef struct rw_yang_pb_flddesc_t rw_yang_pb_flddesc_t;
//GType rw_yang_pb_flddesc_get_type(void);

typedef struct rw_yang_pb_schema_t rw_yang_pb_schema_t;
//GType rw_yang_pb_schema_get_type(void);

const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_msg_msgdesc(
  const rw_yang_pb_msgdesc_t* in_msgdesc);


__END_DECLS


#endif // RW_YANGTOOLS_YANGPB_GI_H_
