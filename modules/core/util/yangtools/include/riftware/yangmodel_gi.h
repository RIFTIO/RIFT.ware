
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
 * @file yangmodel_gi.h
 *
 * GI support for yangmodel
 */

#ifndef RW_YANGTOOLS_YANGMODEL_GI_H_
#define RW_YANGTOOLS_YANGMODEL_GI_H_


#include <sys/cdefs.h>
#include <glib-object.h>
#include "yangpb_gi.h"
#include "rwtypes_gi.h"

#ifndef __GI_SCANNER__
#include "rwlib.h"
#include "rwtypes.h"
#endif

#ifdef __GI_SCANNER__
#define ProtobufCFieldDescriptor gpointer
#define ProtobufCMessageDescriptor gpointer
#endif

__BEGIN_DECLS

#undef _RW_YANG_ENUM_MAGIC_
#define _RW_YANG_ENUM_MAGIC_(WHAT) RW_YANG_##WHAT

#define RW_YANG_STMT_TYPE_VALUES \
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_NULL),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_FIRST),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_ROOT),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_NA),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_ANYXML),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_CONTAINER),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_LEAF),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_LEAF_LIST),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_LIST),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_CHOICE),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_CASE),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_RPC),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_RPCIO),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_NOTIF),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_GROUPING),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_LAST),\
	_RW_YANG_ENUM_MAGIC_(STMT_TYPE_END)


#define  RW_YANG_LEAF_TYPE_VALUES \
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_NULL),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_FIRST),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_INT8),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_INT16),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_INT32),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_INT64),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_UINT8),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_UINT16),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_UINT32),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_UINT64),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_DECIMAL64),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_EMPTY),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_BOOLEAN),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_ENUM),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_STRING),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_LEAFREF),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_IDREF),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_INSTANCE_ID),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_BITS),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_BINARY),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_ANYXML),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_UNION),\
	_RW_YANG_ENUM_MAGIC_(LEAF_TYPE_LAST)

#define RW_YANG_LOG_LEVEL_VALUES  \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_NONE), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_ERROR), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_WARN), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_INFO), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_DEBUG), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_DEBUG2), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_DEBUG3), \
	_RW_YANG_ENUM_MAGIC_(LOG_LEVEL_DEBUG4)

#undef _RW_YANG_ENUM_MAGIC2_
#define _RW_YANG_ENUM_MAGIC2_(what, WHAT) \
typedef enum  { \
  RW_YANG_##WHAT##_VALUES \
} rw_yang_##what##_t; \
\
GType rw_yang_##what##_get_type(void);

_RW_YANG_ENUM_MAGIC2_(stmt_type, STMT_TYPE)
_RW_YANG_ENUM_MAGIC2_(leaf_type, LEAF_TYPE)
_RW_YANG_ENUM_MAGIC2_(log_level, LOG_LEVEL)

//! C adapter struct for C++ class YangModel.
GType rw_yang_model_get_type(void);
typedef struct rw_yang_model_s rw_yang_model_t;

//! C adapter struct for C++ class YangModule.
GType rw_yang_module_get_type(void);
typedef struct rw_yang_module_s rw_yang_module_t;

//! C adapter struct for C++ class YangNode.
GType rw_yang_node_get_type(void);
typedef struct rw_yang_node_s rw_yang_node_t;

//! C adapter struct for C++ class YangType.
GType rw_yang_type_get_type(void);
typedef struct rw_yang_type_s rw_yang_type_t;

//! C adapter struct for C++ class YangKey.
GType rw_yang_key_get_type(void);
typedef struct rw_yang_key_s rw_yang_key_t;

//! C adapter struct for C++ class YangAugment.
GType rw_yang_augment_get_type(void);
typedef struct rw_yang_augment_s rw_yang_augment_t;

//! C adapter struct for C++ class YangValue.
GType rw_yang_value_get_type(void);
typedef struct rw_yang_value_s rw_yang_value_t;

//! C adapter struct for C++ class YangBits.
GType rw_yang_bits_get_type(void);
typedef struct rw_yang_bits_s rw_yang_bits_t;

//! C adapter struct for C++ class YangExtension.
GType rw_yang_extension_get_type(void);
typedef struct rw_yang_extension_s rw_yang_extension_t;

//! C adapter struct for C++ class YangTypedEnum.
GType rw_yang_typed_enum_get_type(void);
typedef struct rw_yang_typed_enum_s rw_yang_typed_enum_t;

//! C adapter struct for C++ class YangGrouping.
GType rw_yang_grouping_get_type(void);
typedef struct rw_yang_grouping_s rw_yang_grouping_t;

//! C adapter struct for C++ class YangUses.
GType rw_yang_uses_get_type(void);
typedef struct rw_yang_uses_s rw_yang_uses_t;


/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
void rw_yang_model_dump(rw_yang_model_t* model, unsigned indent, unsigned verbosity);

void rw_yang_value_dump(rw_yang_value_t* value, unsigned indent, unsigned verbosity);

void rw_yang_node_dump(rw_yang_node_t* node, unsigned indent, unsigned verbosity);
void rw_yang_node_dump_tree(rw_yang_node_t* node, unsigned indent, unsigned verbosity);
rw_yang_node_t* rw_yang_find_node_in_model(rw_yang_model_t* model, const char *name);
#define RW_YANG_SHORT_DESC_BUFLEN 105
size_t rw_yang_make_help_short(const char* s, char buf[RW_YANG_SHORT_DESC_BUFLEN]);

char* rw_yang_alloc_help_short(const char* s);

/* Base64 Helper routines. */
unsigned rw_yang_base64_get_encoded_len(unsigned binary_data_len);

/// @cond GI_SCANNER
/**
 * rw_yang_base64_encode:
 * @input:
 * @inp_len:
 * @output:
 * @out_len:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_base64_encode(const char *input, unsigned inp_len, char *output, unsigned out_len);
unsigned rw_yang_base64_get_decoded_len(const char *input, unsigned inp_len);
/// @cond GI_SCANNER
/**
 * rw_yang_base64_decode:
 * @input:
 * @inp_len:
 * @output:
 * @out_len:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_base64_decode(const char *input, unsigned inp_len, char *output, unsigned out_len);
bool_t rw_yang_is_valid_base64_string(const char *input);


/*****************************************************************************/
// YangModel C APIs
rw_yang_model_t* rw_yang_model_create_libncx();
//rw_yang_model_t* rw_yang_model_new();

// YangModel C APIs
rw_yang_model_t* rw_yang_model_create_libncx_with_trace(void *trace_ctx);

/*! @sa YangModel::~YangModel */
void rw_delete_yang_model (rw_yang_model_t *model);

#if 0 // C function defnitions missing
/*! @sa YangModel::filename_to_module */
void rw_yang_model_filename_to_module(const char* filename, char** str /*!< Allocated string, owned by the caller! */ );
#endif

/*! @sa YangModel::load_module */
rw_yang_module_t* rw_yang_model_load_module(rw_yang_model_t* model, const char* module_name);

/*! @sa YangModel::load_module */
/// @cond GI_SCANNER
/**
 * rw_yang_model_load_schema_ypbc:
 * @model:
 * @schema:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_model_load_schema_ypbc(rw_yang_model_t* model, const rw_yang_pb_schema_t* schema);

/*! @sa YangModel::get_root_node */
rw_yang_node_t* rw_yang_model_get_root_node(rw_yang_model_t* model);

/*! @sa YangModel::search_module_ns */
rw_yang_module_t* rw_yang_model_search_module_ns(rw_yang_model_t* model, const char* ns);

/*! @sa YangModel::search_module_name_rev */
/// @cond GI_SCANNER
/**
 * rw_yang_model_search_module_name_rev:
 * @model:
 * @name:
 * @revision: (nullable)
 * Returns:
 */
/// @endcond GI_SCANNER
rw_yang_module_t* rw_yang_model_search_module_name_rev(rw_yang_model_t* model, const char* name, const char* revision);

/*! @sa YangModel::search_module_ypbc */
rw_yang_module_t* rw_yang_model_search_module_ypbc(rw_yang_model_t* model, const rw_yang_pb_module_t* ypbc_mod);

/*! @sa YangModel::get_first_module */
rw_yang_module_t* rw_yang_model_get_first_module(rw_yang_model_t* model);

/*! @sa YangModel::rw_load_schema*/
const rw_yang_pb_schema_t* rw_yang_model_load_and_get_schema(const char* schema_name);
const rw_yang_pb_schema_t* rw_yang_model_load_and_merge_schema(const rw_yang_pb_schema_t* schema, const char* so_filename, const char* yang_module_name);
const rw_yang_pb_schema_t* rw_yang_model_load_schema(const char* so_filename, const char* yang_module_name);

#ifndef __GI_SCANNER__
/*! @sa YangModel::set_log_level */
void rw_yang_model_set_log_level(rw_yang_model_t* model, rw_yang_log_level_t level);
#endif

/*! @sa YangModel::log_level */
rw_yang_log_level_t rw_yang_model_log_level(rw_yang_model_t* model);

/*! @sa YangModel::get_ypbc_schema */
const rw_yang_pb_schema_t* rw_yang_model_get_ypbc_schema(rw_yang_model_t* model);

/*!
 * Load the schema for the schema name provided as argument
 */
const rw_yang_pb_schema_t* rw_load_and_get_schema(const char* schema_name);

/*!
 * Load the schema with the given name from the given .so and merge it with the given schema.
 */
const rw_yang_pb_schema_t* rw_load_and_merge_schema(const rw_yang_pb_schema_t* schema, const char* so_filename, const char* yang_module_name);

/*!
 * Load the schema with the given name from the given .so.
 */
const rw_yang_pb_schema_t* rw_load_schema(const char* so_filename, const char* yang_module_name);

/// @cond GI_SCANNER
/**
* rw_yang_validate_schema:
* @error_str: (out) (nullable) (transfer full)
*/
/// @endcond GI_SCANNER
void rw_yang_validate_schema(const char* yang_module_name, char** error_str);

/*****************************************************************************/
// YangModule C APIs

/// @cond GI_SCANNER
/**
 * rw_yang_module_get_location:
 * @ymod:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_module_get_location(rw_yang_module_t* ymod, char** str /*!< Allocated string, owned by the caller! */);
const char* rw_yang_module_get_description(rw_yang_module_t* ymod);
const char* rw_yang_module_get_name(rw_yang_module_t* ymod);
const char* rw_yang_module_get_prefix(rw_yang_module_t* ymod);
const char* rw_yang_module_get_ns(rw_yang_module_t* ymod);
rw_yang_module_t* rw_yang_module_get_next_module(rw_yang_module_t* ymod);
bool_t rw_yang_module_was_loaded_explicitly(rw_yang_module_t* ymod);
rw_yang_node_t* rw_yang_module_get_first_node(rw_yang_module_t* ymod);
#if 0 // C function defnitions missing
rw_yang_node_t* rw_yang_module_node_begin(rw_yang_module_t* ymod);
#endif
rw_yang_node_t* rw_yang_module_node_end(rw_yang_module_t* ymod);
rw_yang_extension_t* rw_yang_module_get_first_extension(rw_yang_module_t* ymod);
#if 0 // C function defnitions missing
rw_yang_extension_t* rw_yang_module_extension_begin(rw_yang_module_t* ymod);
rw_yang_extension_t* rw_yang_module_extension_end(rw_yang_module_t* ymod);
#endif
rw_yang_augment_t* rw_yang_module_get_first_augment(rw_yang_module_t* ymod);
#if 0 // C function defnitions missing
rw_yang_augment_t* rw_yang_module_augment_begin(rw_yang_module_t* ymod);
rw_yang_augment_t* rw_yang_module_augment_end(rw_yang_module_t* ymod);
#endif

/*****************************************************************************/
// YangNode C APIs

//! C adaptor function. @sa YangNode::get_location()

/// @cond GI_SCANNER
/**
 * rw_yang_node_get_location:
 * @ynode:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_node_get_location(rw_yang_node_t* ynode, char** str /*!< Allocated string, owned by the caller! */);

//! C adaptor function. @sa YangNode::get_description()
const char* rw_yang_node_get_description(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_help_short()
const char* rw_yang_node_get_help_short(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_help_full()
const char* rw_yang_node_get_help_full(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_name()
const char* rw_yang_node_get_name(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_prefix()
const char* rw_yang_node_get_prefix(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_ns()
const char* rw_yang_node_get_ns(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_default_value()
const char* rw_yang_node_get_default_value(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_max_elements()
uint32_t rw_yang_node_get_max_elements(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_stmt_type()
rw_yang_stmt_type_t rw_yang_node_get_stmt_type(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_leafref_ref()
rw_yang_node_t * rw_yang_node_get_leafref_ref(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_config()
bool_t rw_yang_node_is_config(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_leafy()
bool_t rw_yang_node_is_leafy(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_listy()
bool_t rw_yang_node_is_listy(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_key()
bool_t rw_yang_node_is_key(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::has_keys()
bool_t rw_yang_node_has_keys(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_presence()
bool_t rw_yang_node_is_presence(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_mandatory()
bool_t rw_yang_node_is_mandatory(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_parent()
rw_yang_node_t* rw_yang_node_get_parent(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_first_child()
rw_yang_node_t* rw_yang_node_get_first_child(rw_yang_node_t* ynode);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::child_begin()
rw_yang_node_t* rw_yang_node_child_begin(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::child_end()
rw_yang_node_t* rw_yang_node_child_end(rw_yang_node_t* ynode);
#endif

//! C adaptor function. @sa YangNode::get_next_sibling()
rw_yang_node_t* rw_yang_node_get_next_sibling(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::search_child(const char* name, const char *ns)
/// @cond GI_SCANNER
/**
 * rw_yang_node_search_child
 * @ynode :
 * @ns : (nullable):
 */
/// @endcond GI_SCANNER
rw_yang_node_t* rw_yang_node_search_child(rw_yang_node_t* ynode, const char* name, const char *ns);

//! C adaptor function. @sa YangNode::search_child_with_prefix(const char * name, const char * prefix)
/// @cond GI_SCANNER
/**
 * rw_yang_node_search_child_with_prefix
 * @ynode :
 * @prefix : (nullable):
 */
/// @endcond GI_SCANNER
rw_yang_node_t* rw_yang_node_search_child_with_prefix(rw_yang_node_t * ynode, const char * name, const char * prefix);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::search_descendant_path(rw_yang_node_t* ynode, const rw_yang_path_element_t* const* path)
rw_yang_node_t* rw_yang_node_search_descendant_path(rw_yang_node_t* ynode, const rw_yang_path_element_t* const* path);
#endif

//! C adaptor function. @sa YangNode::node_type()
rw_yang_type_t* rw_yang_node_node_type(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_first_value()
rw_yang_value_t* rw_yang_node_get_first_value(rw_yang_node_t* ynode);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::value_begin()
rw_yang_value_t* rw_yang_node_value_begin(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::value_end()
rw_yang_value_t* rw_yang_node_value_end(rw_yang_node_t* ynode);
#endif

//! C adaptor function. @sa YangNode::get_first_key()
rw_yang_key_t* rw_yang_node_get_first_key(rw_yang_node_t* ynode);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::key_begin()
rw_yang_key_t* rw_yang_node_key_begin(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::key_end()
rw_yang_key_t* rw_yang_node_key_end(rw_yang_node_t* ynode);
#endif

//! C adaptor function. @sa YangNode::get_first_extension()
rw_yang_extension_t* rw_yang_node_get_first_extension(rw_yang_node_t* ynode);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::extension_begin()
rw_yang_extension_t* rw_yang_node_extension_begin(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::extension_end()
rw_yang_extension_t* rw_yang_node_extension_end(rw_yang_node_t* ynode);
#endif

//! C adaptor function. @sa YangNode::search_extension(const char* ns, const char* ext)
rw_yang_extension_t* rw_yang_node_search_extension(rw_yang_node_t* ynode, const char* ns, const char* ext);

//! C adaptor function. @sa YangNode::get_module()
rw_yang_module_t* rw_yang_node_get_module(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_module_orig()
rw_yang_module_t* rw_yang_node_get_module_orig(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::get_model()
rw_yang_model_t* rw_yang_node_get_model(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::matches_prefix(const char* prefix_string)
bool_t rw_yang_node_matches_prefix(rw_yang_node_t* ynode, const char* prefix_string);

//! C adaptor function. @sa YangNode::parse_value(const char* value_string, rw_yang_value_t** matched)
/// @cond GI_SCANNER
/**
 * rw_yang_node_parse_value:
 * @ynode:
 * @value_string:
 * @matched: (out)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_node_parse_value(rw_yang_node_t* ynode, const char* value_string, rw_yang_value_t** matched);

#if 0 // C function defnitions missing
//! C adaptor function. @sa YangNode::get_pbc_message_desc()
const ProtobufCMessageDescriptor* rw_yang_node_get_pbc_message_desc();

//! C adaptor function. @sa YangNode::set_pbc_message_desc(const ProtobufCMessageDescriptor* pbc_message_desc)
/// @cond GI_SCANNER
/**
 * rw_yang_node_set_pbc_message_desc:
 * @pbc_message_desc:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_node_set_pbc_message_desc(const ProtobufCMessageDescriptor* pbc_message_desc);

//! C adaptor function. @sa YangNode::get_pbc_field_name()
const char* rw_yang_node_get_pbc_field_name();

//! C adaptor function. @sa YangNode::get_pbc_field_desc()
const ProtobufCFieldDescriptor* rw_yang_node_get_pbc_field_desc();

//! C adaptor function. @sa YangNode::set_pbc_field_desc(const ProtobufCFieldDescriptor* pbc_field_desc)
/// @cond GI_SCANNER
/**
 * rw_yang_node_set_pbc_field_desc:
 * @pbc_field_desc:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_node_set_pbc_field_desc(const ProtobufCFieldDescriptor* pbc_field_desc);

//! C adaptor function. @sa YangNode::set_mode_path(rw_yang_node_t* ynode)
bool_t rw_yang_node_set_mode_path(rw_yang_node_t* ynode);
#endif

//! C adaptor function. @sa YangNode::is_mode_path(rw_yang_node_t* ynode)
bool_t rw_yang_node_is_mode_path(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_rpc(rw_yang_node_t* ynode)
bool_t rw_yang_node_is_rpc(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_rpc_input(rw_yang_node_t* ynode)
bool_t rw_yang_node_is_rpc_input(rw_yang_node_t* ynode);

//! C adaptor function. @sa YangNode::is_rpc_output(rw_yang_node_t* ynode)
bool_t rw_yang_node_is_rpc_output(rw_yang_node_t* ynode);

/// @cond GI_SCANNER
/**
* rw_yang_node_to_json_schema:
* @ymod:
* @ostr: (out):
*/
/// @endcond GI_SCANNER
void rw_yang_node_to_json_schema(rw_yang_node_t* ynode, char** ostr, bool_t pretty_print);


/*****************************************************************************/
// YangKey C APIs
rw_yang_node_t* rw_yang_key_get_list_node(rw_yang_key_t* ykey);
rw_yang_node_t* rw_yang_key_get_key_node(rw_yang_key_t* ykey);
rw_yang_key_t* rw_yang_key_get_next_key(rw_yang_key_t* ykey);


#if 0 // C function defnitions missing
/*****************************************************************************/
// YangAugment C APIs

/// @cond GI_SCANNER
/**
 * rw_yang_augment_get_location:
 * @yaug:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_augment_get_location(rw_yang_augment_t* yaug, char** str /*!< Allocated string, owned by the caller! */);
rw_yang_node_t* rw_yang_augment_get_node(rw_yang_augment_t* yaug);
rw_yang_module_t* rw_yang_augment_get_module(rw_yang_augment_t* yaug);
rw_yang_augment_t* rw_yang_augment_get_next_augment(rw_yang_augment_t* yaug);
rw_yang_extension_t* rw_yang_augment_get_first_extension(rw_yang_augment_t* yaug);
rw_yang_extension_t* rw_yang_augment_extension_begin(rw_yang_augment_t* yaug);
rw_yang_extension_t* rw_yang_augment_extension_end(rw_yang_augment_t* yaug);
#endif


/*****************************************************************************/
// YangType C APIs

/// @cond GI_SCANNER
/**
 * rw_yang_type_get_location:
 * @ytype:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_type_get_location(rw_yang_type_t* ytype, char** str /*!< Allocated string, owned by the caller! */);
const char* rw_yang_type_get_description(rw_yang_type_t* ytype);
const char* rw_yang_type_get_help_short(rw_yang_type_t* ytype);
const char* rw_yang_type_get_help_full(rw_yang_type_t* ytype);
const char* rw_yang_type_get_prefix(rw_yang_type_t* ytype);
const char* rw_yang_type_get_ns(rw_yang_type_t* ytype);
rw_yang_leaf_type_t rw_yang_type_get_leaf_type(rw_yang_type_t* ytype);
rw_yang_value_t* rw_yang_type_get_first_value(rw_yang_type_t* ytype);
#if 0 // C function defnitions missing
rw_yang_value_t* rw_yang_type_value_begin(rw_yang_type_t* ytype);
rw_yang_value_t* rw_yang_type_value_end(rw_yang_type_t* ytype);
#endif
/// @cond GI_SCANNER
/**
 * rw_yang_type_parse_value:
 * @ytype:
 * @value_string:
 * @matched: (out)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_type_parse_value(rw_yang_type_t* ytype, const char* value_string, rw_yang_value_t** matched);
unsigned rw_yang_type_count_matches(rw_yang_type_t* ytype, const char* value_string);
unsigned rw_yang_type_count_partials(rw_yang_type_t* ytype, const char* value_string);
rw_yang_extension_t* rw_yang_type_get_first_extension(rw_yang_type_t* ytype);
#if 0 // C function defnitions missing
rw_yang_extension_t* rw_yang_type_extension_begin(rw_yang_type_t* ytype);
rw_yang_extension_t* rw_yang_type_extension_end(rw_yang_type_t* ytype);
#endif


/*****************************************************************************/
// YangValue C APIs
const char* rw_yang_value_get_name(rw_yang_value_t* yvalue);

/// @cond GI_SCANNER
/**
 * rw_yang_value_get_location:
 * @yvalue:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_value_get_location(rw_yang_value_t* yvalue, char** str /*!< Allocated string, owned by the caller! */);
const char* rw_yang_value_get_description(rw_yang_value_t* yvalue);
const char* rw_yang_value_get_help_short(rw_yang_value_t* yvalue);
const char* rw_yang_value_get_help_full(rw_yang_value_t* yvalue);
const char* rw_yang_value_get_prefix(rw_yang_value_t* yvalue);
const char* rw_yang_value_get_ns(rw_yang_value_t* yvalue);
rw_yang_leaf_type_t rw_yang_value_get_leaf_type(rw_yang_value_t* yvalue);
uint64_t rw_yang_value_get_max_length(rw_yang_value_t* yvalue);
rw_yang_value_t* rw_yang_value_get_next_value(rw_yang_value_t* yvalue);
/// @cond GI_SCANNER
/**
 * rw_yang_value_parse_value:
 * @yvalue:
 * @value_string:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_value_parse_value(rw_yang_value_t* yvalue, const char* value_string);
/// @cond GI_SCANNER
/**
 * rw_yang_value_parse_partial:
 * @yvalue:
 * @value_string:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond GI_SCANNER
rw_status_t rw_yang_value_parse_partial(rw_yang_value_t* yvalue, const char* value_string);
bool_t rw_yang_value_is_keyword(rw_yang_value_t* yvalue);
rw_yang_extension_t* rw_yang_value_get_first_extension(rw_yang_value_t* yvalue);
#if 0 // C function defnitions missing
rw_yang_extension_t* rw_yang_value_extension_begin(rw_yang_value_t* yvalue);
rw_yang_extension_t* rw_yang_value_extension_end(rw_yang_value_t* yvalue);
#endif


/*****************************************************************************/
// YangExtension C APIs

/// @cond GI_SCANNER
/**
 * rw_yang_extension_get_location:
 * @yext:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_extension_get_location(rw_yang_extension_t* yext, char** str /*!< Allocated string, owned by the caller! */);

/// @cond GI_SCANNER
/**
 * rw_yang_extension_get_location_ext:
 * @yext:
 * @str: (out):
 */
/// @endcond GI_SCANNER
void rw_yang_extension_get_location_ext(rw_yang_extension_t* yext, char** str /*!< Allocated string, owned by the caller! */);
const char* rw_yang_extension_get_value(rw_yang_extension_t* yext);
const char* rw_yang_extension_get_description_ext(rw_yang_extension_t* yext);
const char* rw_yang_extension_get_name(rw_yang_extension_t* yext);
const char* rw_yang_extension_get_module_ext(rw_yang_extension_t* yext);
const char* rw_yang_extension_get_prefix(rw_yang_extension_t* yext);
const char* rw_yang_extension_get_ns(rw_yang_extension_t* yext);
rw_yang_extension_t* rw_yang_extension_get_next_extension(rw_yang_extension_t* yext);
rw_yang_extension_t* rw_yang_extension_search(rw_yang_extension_t* yext, const char* module, const char* ext);
bool_t rw_yang_extension_is_match(rw_yang_extension_t* yext, const char* module, const char* ext);

__END_DECLS

#endif // RW_YANGTOOLS_YANGMODEL_GI_H_
