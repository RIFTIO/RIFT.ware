
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file yangmodel.cpp
 *
 * Rift yang model, common routines.
 */

#include <algorithm>
#include <ctype.h>
#include <dlfcn.h>

#include "b64.h"

#include "rw_pb_schema.h"
#include "yangmodel.h"
#include "yangncx.hpp"
#include "yangpbc.hpp"
#include "rwyangutil.h"

using namespace rw_yang;


// Protobuf extensions
const char* RW_YANG_PB_MODULE = "http://riftio.com/ns/riftware-1.0/rw-pb-ext";

const char* RW_YANG_PB_EXT_MOD_PACKAGE = "mod-package";
const char* RW_YANG_PB_EXT_MOD_GLOBAL = "mod-global";

const char* RW_YANG_PB_EXT_MSG_NEW = "msg-new";
const char* RW_YANG_PB_EXT_MSG_NAME = "msg-name";
const char* RW_YANG_PB_EXT_MSG_FLAT = "msg-flat";
const char* RW_YANG_PB_EXT_MSG_PROTO_MAX_SIZE = "msg-proto-max-size";
const char* RW_YANG_PB_EXT_MSG_TAG_BASE = "msg-tag-base";
const char* RW_YANG_PB_EXT_MSG_TAG_RESERVE = "msg-tag-reserve";

const char* RW_YANG_PB_EXT_FLD_NAME = "field-name";
const char* RW_YANG_PB_EXT_FLD_INLINE = "field-inline";
const char* RW_YANG_PB_EXT_FLD_INLINE_MAX = "field-inline-max";
const char* RW_YANG_PB_EXT_FLD_STRING_MAX = "field-string-max";
const char* RW_YANG_PB_EXT_FLD_TAG = "field-tag";
const char* RW_YANG_PB_EXT_FLD_TYPE = "field-type";
const char* RW_YANG_PB_EXT_FLD_C_TYPE = "field-c-type";

const char* RW_YANG_PB_EXT_PACKAGE_NAME = "package-name";

const char* RW_YANG_PB_EXT_ENUM_NAME = "enum-name";
const char* RW_YANG_PB_EXT_ENUM_TYPE = "enum-type";
const char* RW_YANG_PB_EXT_FILE_PBC_INCLUDE = "file-pbc-include";
const char* RW_YANG_PB_EXT_FIELD_MERGE_BEHAVIOR = "field-merge-behavior";


// RW.CLI extensions
const char* RW_YANG_CLI_MODULE = "http://riftio.com/ns/riftware-1.0/rw-cli-ext";

const char* RW_YANG_CLI_EXT_NEW_MODE = "new-mode";
const char* RW_YANG_CLI_EXT_PLUGIN_API = "plugin-api";
const char* RW_YANG_CLI_EXT_CLI_PRINT_HOOK = "cli-print-hook";
const char* RW_YANG_CLI_EXT_SHOW_KEY_KW = "show-key-keyword";


const char *RW_ANNOTATIONS_NAMESPACE = "Riftio_base";

const char* RW_YANG_NOTIFY_MODULE = "http://riftio.com/ns/riftware-1.0/rw-notify-ext";
const char* RW_YANG_NOTIFY_EXT_LOG_COMMON = "log-common";
const char* RW_YANG_NOTIFY_EXT_LOG_EVENT_ID = "log-event-id";

/*!
 * Determine the short description string length.  The idea is to break
 * the short description on a word, sentence, or line boundary.  The
 * whole string is used, if short enough.
 */
static size_t rw_yang_description_short_len(const char* s)
{
  static const size_t RW_YANG_SHORT_DESC_END = 60;
  static const size_t RW_YANG_SHORT_DESC_MAX = 100;
  RW_ASSERT(s);
  size_t i;
  for(i = 0; i < RW_YANG_SHORT_DESC_END; ++i) {
    if (s[i] == '\0' || s[i] == '.' || s[i] == '\n') {
      return i;
    }
  }
  for(; i < RW_YANG_SHORT_DESC_MAX; ++i) {
    if (s[i] == '\0' || s[i] == '-' || s[i] == ';' || s[i] == '!' || s[i] == ',' || s[i] == '\n' || s[i] == ' ') {
      return i;
    }
  }

  return i;
}

/*!
 * Take a description string and copy a short version of it to a
 * buffer.  If the buffer is too short, elipsis will be added.
 */
size_t rw_yang_make_help_short(const char* s, char buf[RW_YANG_SHORT_DESC_BUFLEN])
{
  size_t len = rw_yang_description_short_len(s);
  strncpy(buf,s,len);
  if (s) {
    if (s[len]) {
      if (s[len+1]) {
        strncpy(&buf[len],"...",3);
        len += 3;
      } else {
        buf[len] = s[len];
        ++len;
      }
    }
  }
  RW_ASSERT(len<RW_YANG_SHORT_DESC_BUFLEN);
  buf[len]='\0';
  return len;
}

/*!
 * Take a description string, and create a short version of it in a
 * malloc'ed buffer.  If the description is too long, elipsis will be
 * added.
 */
char* rw_yang_alloc_help_short(const char* s)
{
  size_t len = rw_yang_description_short_len(s);
  size_t buf_len = len;
  if (s) {
    if (s[len]) {
      if (s[len+1]) {
        buf_len += 3;
      } else {
        ++buf_len;
      }
    }
  }
  RW_ASSERT(buf_len<RW_YANG_SHORT_DESC_BUFLEN);
  char* buf = (char*)malloc(buf_len+1);
  strncpy(buf,s,len);
  if (s) {
    if (s[len]) {
      if (s[len+1]) {
        strncpy(&buf[len],"...",3);
        len += 3;
      } else {
        buf[len] = s[len];
      }
    }
  }
  RW_ASSERT(buf_len == len);
  buf[len]='\0';
  return buf;
}


/*
 * Base64 helper functions.
 */
unsigned rw_yang_base64_get_encoded_len(unsigned binary_data_len)
{
  return (b64_get_encoded_str_len(binary_data_len, 0));
}

rw_status_t rw_yang_base64_encode(const char *input, unsigned inp_len, char *output, unsigned out_len)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  unsigned ret_len = 0;

  RW_ASSERT(input);
  RW_ASSERT(output);

  unsigned encode_len = b64_get_encoded_str_len(inp_len, 0);
  if (out_len < encode_len) {
    status = RW_STATUS_OUT_OF_BOUNDS;
  }

  // The output buffer is null terminated.
  if (b64_encode((const uint8_t*)input, inp_len, (uint8_t*)output, out_len, 0, &ret_len) != NO_ERR) {
    status = RW_STATUS_FAILURE;
  }

  return status;
}

unsigned rw_yang_base64_get_decoded_len(const char *input, unsigned inp_len)
{
  return (b64_get_decoded_str_len((const uint8_t *)input, inp_len));
}

rw_status_t rw_yang_base64_decode(const char *input, unsigned inp_len, char *output, unsigned out_len)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  unsigned ret_len = 0;

  RW_ASSERT(input);
  RW_ASSERT(output);

  unsigned decode_len = b64_get_decoded_str_len((const uint8_t *)input, inp_len);
  if (out_len < decode_len) {
    status = RW_STATUS_OUT_OF_BOUNDS;
  }

  // The output buffer is not null terminated.
  if (b64_decode((const uint8_t*)input, inp_len, (uint8_t*)output, out_len, &ret_len) != NO_ERR) {
    status = RW_STATUS_FAILURE;
  }

  return status;
}

bool_t rw_yang_is_valid_base64_string(const char *input)
{
  for (unsigned i = 0; i < strlen(input); i++) {
    if (!((isalnum(input[i])) || (input[i] == '+') || (input[i] == '/') || (input[i] == '='))) {
      return false;
    }
  }
  return true;
}

/*!
 * Get a description string for statement type.
 */
const char* rw_yang_stmt_type_string(rw_yang_stmt_type_t v)
{
  switch (v) {
    case RW_YANG_STMT_TYPE_NULL:      return "NULL";
    case RW_YANG_STMT_TYPE_ROOT:      return "ROOT";
    case RW_YANG_STMT_TYPE_NA:        return "NA";
    case RW_YANG_STMT_TYPE_ANYXML:    return "ANYXML";
    case RW_YANG_STMT_TYPE_CONTAINER: return "CONTAINER";
    case RW_YANG_STMT_TYPE_GROUPING:  return "GROUPING";
    case RW_YANG_STMT_TYPE_LEAF:      return "LEAF";
    case RW_YANG_STMT_TYPE_LEAF_LIST: return "LEAF_LIST";
    case RW_YANG_STMT_TYPE_LIST:      return "LIST";
    case RW_YANG_STMT_TYPE_CHOICE:    return "CHOICE";
    case RW_YANG_STMT_TYPE_CASE:      return "CASE";
    case RW_YANG_STMT_TYPE_RPC:       return "RPC";
    case RW_YANG_STMT_TYPE_RPCIO:     return "RPCIO";
    case RW_YANG_STMT_TYPE_NOTIF:     return "NOTIF";
    default: return "**UNKNOWN**";
  }
}

/*!
 * Get a description string for statement type.
 */
const char* rw_yang_leaf_type_string(rw_yang_leaf_type_t v)
{
  switch (v) {
    case RW_YANG_LEAF_TYPE_NULL:        return "NULL";
    case RW_YANG_LEAF_TYPE_INT8:        return "INT8";
    case RW_YANG_LEAF_TYPE_INT16:       return "INT16";
    case RW_YANG_LEAF_TYPE_INT32:       return "INT32";
    case RW_YANG_LEAF_TYPE_INT64:       return "INT64";
    case RW_YANG_LEAF_TYPE_UINT8:       return "UINT8";
    case RW_YANG_LEAF_TYPE_UINT16:      return "UINT16";
    case RW_YANG_LEAF_TYPE_UINT32:      return "UINT32";
    case RW_YANG_LEAF_TYPE_UINT64:      return "UINT64";
    case RW_YANG_LEAF_TYPE_DECIMAL64:   return "DECIMAL64";
    case RW_YANG_LEAF_TYPE_EMPTY:       return "EMPTY";
    case RW_YANG_LEAF_TYPE_BOOLEAN:     return "BOOLEAN";
    case RW_YANG_LEAF_TYPE_ENUM:        return "ENUM";
    case RW_YANG_LEAF_TYPE_STRING:      return "STRING";
    case RW_YANG_LEAF_TYPE_LEAFREF:     return "LEAFREF";
    case RW_YANG_LEAF_TYPE_IDREF:       return "IDREF";
    case RW_YANG_LEAF_TYPE_INSTANCE_ID: return "INSTANCE_ID";
    case RW_YANG_LEAF_TYPE_BITS:        return "BITS";
    case RW_YANG_LEAF_TYPE_BINARY:      return "BINARY";
    case RW_YANG_LEAF_TYPE_ANYXML:      return "ANYXML";
    case RW_YANG_LEAF_TYPE_UNION:       return "UNION";
    default: return "**UNKNOWN**";
  }
}

/*!
 * Get a description string for the ypbc message type.
 */
const char* rw_yang_pb_msg_type_string(rw_yang_pb_msg_type_t v)
{
  switch (v) {
    case RW_YANGPBC_MSG_TYPE_NULL:           return "NULL";
    case RW_YANGPBC_MSG_TYPE_ROOT:           return "ROOT";
    case RW_YANGPBC_MSG_TYPE_MODULE_ROOT:    return "MODULE_ROOT";
    case RW_YANGPBC_MSG_TYPE_CONTAINER:      return "CONTAINER";
    case RW_YANGPBC_MSG_TYPE_LIST:           return "LIST";
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST:      return "LEAF_LIST";
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT:      return "RPC_INPUT";
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT:     return "RPC_OUTPUT";
    case RW_YANGPBC_MSG_TYPE_GROUPING:       return "GROUPING";
    case RW_YANGPBC_MSG_TYPE_NOTIFICATION:   return "NOTIFICATION";
    case RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY:   return "SCHEMA_ENTRY";
    case RW_YANGPBC_MSG_TYPE_SCHEMA_PATH:    return "SCHEMA_PATH";
    default: return "**UNKNOWN**";
  }
}


void rw_yang_model_dump(rw_yang_model_t* model, unsigned indent, unsigned verbosity)
{
  RW_ASSERT(model);
  rw_yang_node_dump_tree(static_cast<YangModel*>(model)->get_root_node(), indent, verbosity);
}

void rw_yang_value_dump(rw_yang_value_t* value, unsigned indent, unsigned verbosity)
{
  RW_ASSERT(value);
  YangValue* yvalue = static_cast<YangValue*>(value);

  unsigned adjust = 0;
  if (indent<30) {
    adjust = 30 - indent;
  }
  printf( "%*s%-*.*s type=%-10.10s pfx=%-10.10s",
    indent, "",
    adjust, adjust, yvalue->get_name(),
    rw_yang_leaf_type_string(yvalue->get_leaf_type()),
    yvalue->get_prefix() );

  if (verbosity > 2) {
    printf( "\n%s\n", yvalue->get_description() );
  } else if (verbosity) {
    char buf[RW_YANG_SHORT_DESC_BUFLEN];
    rw_yang_make_help_short(yvalue->get_description(),buf);
    printf( " %s\n", buf );
  } else {
    printf( "\n" );
  }
}


void rw_yang_node_dump(rw_yang_node_t* node, unsigned indent, unsigned verbosity)
{
  RW_ASSERT(node);
  YangNode* ynode = static_cast<YangNode*>(node);

  unsigned adjust = 0;
  if (indent<30) {
    adjust = 30 - indent;
  }
  printf( "%*s%-*.*s obtp=%-10.10s pfx=%-10.10s",
    indent, "",
    adjust, adjust, ynode->get_name(),
    rw_yang_stmt_type_string(ynode->get_stmt_type()),
    ynode->get_prefix() );

  if (verbosity > 2) {
    printf( "\n%s\n", ynode->get_description() );
  } else if (verbosity) {
    char buf[RW_YANG_SHORT_DESC_BUFLEN];
    rw_yang_make_help_short(ynode->get_description(),buf);
    printf( " %s\n", buf );
  } else {
    printf( "\n" );
  }

  if (ynode->is_leafy()) {
    for (YangValueIter yvi(ynode); yvi != YangValueIter(); ++yvi ) {
      rw_yang_value_dump(&*yvi, indent+2, verbosity);
    }
  }
}

void rw_yang_node_dump_tree(rw_yang_node_t* node, unsigned indent, unsigned verbosity)
{
  RW_ASSERT(node);
  YangNode* ynode = static_cast<YangNode*>(node);

  rw_yang_node_dump(ynode,indent,verbosity);
  for (ynode = ynode->get_first_child(); NULL != ynode; ynode = ynode->get_next_sibling() ) {
    rw_yang_node_dump_tree(ynode, indent+2, verbosity);
  }
}

rw_yang_node_t* rw_yang_find_node_in_model(rw_yang_model_t* model, const char *name)
{
  RW_ASSERT(model);
  YangNode*ynode = static_cast<YangModel*>(model)->get_root_node();
  for (ynode = ynode->get_first_child(); NULL != ynode; ynode = ynode->get_next_sibling() ) {
    if (ynode->matches_prefix(name))
      return ynode;
  }
  return NULL;
}


// YangModel C APIs
void rw_delete_yang_model (rw_yang_model_t *model)
{
  YangModel *model_impl = static_cast<YangModel*>(model);

  delete model_impl;
}

rw_yang_module_t* rw_yang_model_load_module(rw_yang_model_t* model, const char* module_name)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangModel*>(model)->load_module(module_name));
}

rw_status_t rw_yang_model_load_schema_ypbc(rw_yang_model_t* model, const rw_yang_pb_schema_t* schema)
{
  return static_cast<YangModel*>(model)->load_schema_ypbc(schema);
}

rw_yang_node_t* rw_yang_model_get_root_node(rw_yang_model_t* model)
{
  RW_ASSERT(model);
  YangNode*ynode = static_cast<YangModel*>(model)->get_root_node();
  return ynode;
}

rw_yang_module_t* rw_yang_model_search_module_ns(rw_yang_model_t* model, const char* ns)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangModel*>(model)->search_module_ns(ns));
}

rw_yang_module_t* rw_yang_model_search_module_name_rev(rw_yang_model_t* model, const char* name, const char* revision)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangModel*>(model)->search_module_name_rev(name, revision));
}

rw_yang_module_t* rw_yang_model_search_module_ypbc(rw_yang_model_t* model, const rw_yang_pb_module_t* ypbc_mod)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangModel*>(model)->search_module_name_rev(ypbc_mod->module_name, ypbc_mod->revision));
}

rw_yang_module_t* rw_yang_model_get_first_module(rw_yang_model_t* model)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangModel*>(model)->get_first_module());
}

const rw_yang_pb_schema_t* rw_yang_model_load_and_get_schema(const char* schema_name)
{
  return rw_load_and_get_schema(schema_name);
}

const rw_yang_pb_schema_t* rw_yang_model_load_and_merge_schema(const rw_yang_pb_schema_t* schema, const char* so_filename, const char* yang_module_name)
{
  return rw_load_and_merge_schema(schema, so_filename, yang_module_name);
}

const rw_yang_pb_schema_t* rw_yang_model_load_schema(const char* so_filename, const char* yang_module_name)
{
  return rw_load_schema(so_filename, yang_module_name);
}

void rw_yang_model_set_log_level(rw_yang_model_t* model, rw_yang_log_level_t level)
{
  static_cast<YangModel*>(model)->set_log_level(level);
}

rw_yang_log_level_t rw_yang_model_log_level(rw_yang_model_t* model)
{
  return static_cast<rw_yang_log_level_t>(static_cast<YangModel*>(model)->log_level());
}

const rw_yang_pb_schema_t* rw_load_and_get_schema(const char* schema_name)
{
  RW_ASSERT(schema_name);

  std::string sname(schema_name);

  auto sym_name = "rw_ypbc_" + YangModel::mangle_to_camel_case(sname.c_str())
    + "_g_schema";

  sname.erase(std::remove_if(sname.begin(), sname.end(), [](char c) {
                                                      if (c == '-' || c == '_') {
                                                        return true;
                                                      }
                                                      return false;
                                                   }),
              sname.end());

  auto lib_name = "lib" + sname + "_yang_parse_gen.so";
  void *handle = dlopen (lib_name.c_str(), RTLD_LAZY|RTLD_NODELETE);

  // ATTN: Deprecate when the list of messages is available.
  RW_ASSERT(handle);

  return
      ((const rw_yang_pb_schema_t*)dlsym (handle, sym_name.c_str()));

}

const rw_yang_pb_schema_t* rw_load_and_merge_schema(const rw_yang_pb_schema_t* schema, const char* so_filename, const char* yang_module_name)
{
  RW_ASSERT(so_filename);
  RW_ASSERT(yang_module_name);

  const rw_yang_pb_schema_t* new_schema =  rw_load_schema(so_filename, yang_module_name);
  RW_ASSERT(new_schema);

  const rw_yang_pb_schema_t * merged_schema = rw_schema_merge(NULL, schema, new_schema);
  RW_ASSERT(merged_schema);

  return merged_schema;
}

const rw_yang_pb_schema_t* rw_load_schema(const char* so_filename, const char* yang_module_name)
{
  RW_ASSERT(so_filename);
  RW_ASSERT(yang_module_name);

  auto sym_name = "rw_ypbc_" + YangModel::mangle_to_camel_case(yang_module_name)
    + "_g_schema";

  void *handle = dlopen (so_filename, RTLD_LAZY|RTLD_NODELETE);
  RW_ASSERT(handle);

  const rw_yang_pb_schema_t* new_schema =  (const rw_yang_pb_schema_t*)dlsym (handle, sym_name.c_str());
  RW_ASSERT(new_schema);

  return new_schema;
}

void rw_yang_validate_schema(const char* yang_module_name,
                             char** error_str)
{
  RW_ASSERT(yang_module_name);

  std::string err_str;
  std::string mangled_name = YangModel::mangle_to_camel_case(yang_module_name);
  std::string upper_to_lower = YangModel::mangle_camel_case_to_underscore(mangled_name.c_str());

  bool ret = rwyangutil::validate_module_consistency(
                      yang_module_name,
                      mangled_name,
                      upper_to_lower,
                      err_str);

  if (!ret && err_str.length()) {
    *error_str = RW_STRDUP (err_str.c_str());
  }
}

/*****************************************************************************/
// Default implementations for YangNode virtual functions.

const char* YangNode::get_help_short()
{
  // ATTN: implement a common function that builds this from get_description()?
  return "";
}

const char* YangNode::get_help_full()
{
  // ATTN: implement a common function that builds this from other get()s?
  return "";
}

const char* YangNode::get_default_value()
{
  // By default, there is no default value.  Empty string means
  // "default is empty string", so don't return empty string here.
  return nullptr;
}

std::string YangNode::get_xml_element_name()
{
  return std::string(get_module()->get_prefix()) + ":" + get_name();
}

std::string YangNode::get_xml_element_start_with_ns(
  bool require_xmlns )
{
  std::string retval = get_xml_element_name();

  auto parent = get_parent();
  auto module = get_module();
  if (   require_xmlns
      || parent->is_root()
      || parent->get_module() != module) {
    retval += std::string(" xmlns:") + get_module()->get_prefix() + "=\"" + get_ns() + "\"";
  }
  return retval;
}

std::string YangNode::get_xpath_element_name()
{
  switch (get_stmt_type()) {
    case RW_YANG_STMT_TYPE_RPCIO: {
      // The XML tags come from the parent node.
      auto parent = get_parent();
      RW_ASSERT(parent);
      return parent->get_xml_element_name();
    }
    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    case RW_YANG_STMT_TYPE_RPC:
      // Don't consider these to have an xpath element.
      break;

    case RW_YANG_STMT_TYPE_ANYXML:
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_NOTIF:
      return get_xml_element_name();

    default:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_NULL:
      RW_ASSERT_NOT_REACHED();
  }

  return "";
}

std::string YangNode::get_rw_xpath_element_with_sample_predicate()
{
  std::string retval = get_xpath_element_name();
  if (!has_keys()) {
    return retval;
  }

  for (YangKeyIter yki = key_begin(); yki != key_end(); ++yki) {
    YangNode* keynode = yki->get_key_node();
    RW_ASSERT(keynode);
    retval += "[" + keynode->get_xml_element_name() + "='"
           + YangModel::utf8_to_escaped_libncx_xpath_string(
               keynode->get_xml_sample_leaf_value())
           + "']";
  }
  return retval;
}

std::string YangNode::get_xml_sample_leaf_value()
{
  if (!is_leafy()) {
    return "";
  }

  const char* def_val = get_default_value();
  if (def_val && def_val[0] != '\0') {
    return def_val;
  }

  YangType* type = get_type();
  RW_ASSERT(type);
  switch (type->get_leaf_type()) {
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
      return "0";

    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
      return "0";

    case RW_YANG_LEAF_TYPE_DECIMAL64:
      // ATTN: Handle fraction-digits
      return "0.0";

    case RW_YANG_LEAF_TYPE_EMPTY:
      // Not quite; empty is really special and this is technically wrong
      return "";

    case RW_YANG_LEAF_TYPE_BOOLEAN:
      return "true";

    case RW_YANG_LEAF_TYPE_ENUM: {
      auto value = get_first_value();
      if (!value) {
        return "ENUM_VALUE";
      }
      return value->get_name();
    }
    case RW_YANG_LEAF_TYPE_LEAFREF: {
      // Follow the chain to the ultimate target.
      auto ref_node = get_leafref_ref();
      for (auto next_ref = ref_node; next_ref; next_ref = next_ref->get_leafref_ref()) {
        ref_node = next_ref;
      }
      if (ref_node) {
        return ref_node->get_xml_sample_leaf_value();
      }
      return "LEAFREF_VALUE";
    }
    case RW_YANG_LEAF_TYPE_STRING:
      // ATTN: Pattern or length
      return "STRING";

    case RW_YANG_LEAF_TYPE_IDREF: {
      auto value = get_first_value();
      if (!value) {
        return "IDENTITY_VALUE";
      }
      return value->get_name();
    }
    case RW_YANG_LEAF_TYPE_INSTANCE_ID:
      return "/ex:top/ex:list[ex:key='value']/...";

    case RW_YANG_LEAF_TYPE_BITS: {
      std::string sample;
      auto value = get_first_value();
      if (!value) {
        return "BITS";
      }
      sample = value->get_name();
      value = value->get_next_value();
      if (!value) {
        return sample;
      }
      return sample + " " +  value->get_name();
    }
    case RW_YANG_LEAF_TYPE_BINARY:
      return "BASE64+Encoded==";

    case RW_YANG_LEAF_TYPE_ANYXML:
      return "<ex:anyxml/>";

    case RW_YANG_LEAF_TYPE_UNION:
      // ATTN: Get first type
      return "UNION_VALUE";

    default:
      RW_ASSERT_NOT_REACHED();
  }

  return "";
}

std::string YangNode::get_xml_sample_element(
  unsigned indent,
  unsigned to_depth,
  bool config_only,
  bool require_xmlns )
{
  if (config_only && !is_config()) {
    return "";
  }

  std::string sample;
  auto stmt_type = get_stmt_type();
  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_ANYXML:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
    case RW_YANG_STMT_TYPE_LEAF:
      sample = get_xml_sample_leaf_value();
      if (0 == sample.length()) {
        sample = "<" + get_xml_element_start_with_ns(require_xmlns) + "/>";
        break;
      }

      if (RW_YANG_STMT_TYPE_ANYXML == stmt_type) {
        sample =
            std::string("<") + get_xml_element_start_with_ns(require_xmlns) + ">\n"
          + std::string(indent+2,' ') + sample + "\n"
          + std::string(indent,' ') + "</" + get_xml_element_name() + ">";
      } else {
        sample =
            std::string("<") + get_xml_element_start_with_ns(require_xmlns) + ">"
          + sample
          + "</" + get_xml_element_name() + ">";
      }

      if (RW_YANG_STMT_TYPE_LEAF_LIST == stmt_type) {
        // Add a second leaf-list entry, fake data
        sample
          += "\n" + std::string(indent,' ')
          +  "<" + get_xml_element_start_with_ns(require_xmlns) + ">...</"
          +  get_xml_element_name() + ">";
      }
      break;

    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF: {
      /*
       * ATTN: Technically, the rpc output element has different open
       * and closing tags than this produces.  There are no situations
       * in NETCONF where the output XML actually contains the RPC name
       * or "output".
       */
      /*
       * ATTN: For output, if the message is empty, then the contents
       * is actually just <ok/>.  Should that be in the example?
       */
      // ATTN: What to do if input/output/notif && config-only?
      bool is_empty = (nullptr == get_first_child());
      if (!is_empty && config_only) {
        is_empty = true;
        for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
          if (yni->is_config()) {
            is_empty = false;
            break;
          }
        }
      }
      if (is_empty) {
        // No children, so make it empty.
        sample = "<" + get_xml_element_start_with_ns(require_xmlns) + "/>";
        break;
      }

      sample = "<" + get_xml_element_start_with_ns(require_xmlns) + ">";

      if (to_depth > 0) {
        bool keys_first = (has_keys() && !is_rpc_input());
        std::string nl = "\n" + std::string(indent+2,' ');

        if (keys_first) {
          for (YangKeyIter yki = key_begin(); yki != key_end(); ++yki) {
            YangNode* keynode = yki->get_key_node();
            RW_ASSERT(keynode);
            sample += nl + keynode->get_xml_sample_element(
              indent+2, to_depth-1, config_only, false/*require_xmlns*/ );
          }
        }

        for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
          if (keys_first && yni->is_key()) {
            continue;
          }
          if (config_only && !yni->is_config()) {
            continue;
          }
          sample += nl + yni->get_xml_sample_element(
            indent+2, to_depth-1, config_only, false/*require_xmlns*/ );
        }

        sample += "\n" + std::string(indent,' ');
      } else {
        // ATTN: Insert XML comment with reference to deeper element?
        sample += "...";
      }
      sample += "</" + get_xml_element_name() + ">";
      break;
    }

    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_RPC:
      // ATTN: Should assert?
      break;

    case RW_YANG_STMT_TYPE_NULL:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    default:
      RW_ASSERT_NOT_REACHED();
  }
  return sample;
}

std::string YangNode::get_rest_element_name()
{
  std::string retval = get_name();
  auto parent = get_parent();
  auto module = get_module();
  if (   parent->is_root()
      || parent->get_module() != module) {
    retval = std::string(module->get_name()) + ":" + retval;
  }
  return retval;
}

std::string YangNode::get_rwrest_path_element()
{
  std::string retval = get_rest_element_name();
  if (has_keys()) {
    bool first = true;
    for (YangKeyIter yki = key_begin(); yki != key_end(); ++yki) {
      YangNode* keynode = yki->get_key_node();
      RW_ASSERT(keynode);
      retval = retval
             + (first?"/":",")
             + YangModel::utf8_to_escaped_uri_key_string(
                 keynode->get_xml_sample_leaf_value());
      first = false;
    }
  }
  return retval;
}

std::string YangNode::get_rwrest_json_sample_leaf_value()
{
  if (!is_leafy()) {
    return "";
  }
  std::string sample = get_xml_sample_leaf_value();

  YangType* type = get_type();
  RW_ASSERT(type);
  switch (type->get_leaf_type()) {
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
    case RW_YANG_LEAF_TYPE_DECIMAL64:
    case RW_YANG_LEAF_TYPE_BOOLEAN:
      break;

    case RW_YANG_LEAF_TYPE_EMPTY:
      // As per RESTCONF.  ATTN: What does confd do?
      return "[null]";

    case RW_YANG_LEAF_TYPE_LEAFREF: {
      auto ref_node = get_leafref_ref();
      if (ref_node) {
        return ref_node->get_rwrest_json_sample_leaf_value();
      }
      return "\"LEAFREF_VALUE\"";
    }
    case RW_YANG_LEAF_TYPE_ENUM:
    case RW_YANG_LEAF_TYPE_STRING:
    case RW_YANG_LEAF_TYPE_IDREF:
    case RW_YANG_LEAF_TYPE_BITS:
    case RW_YANG_LEAF_TYPE_BINARY:
    case RW_YANG_LEAF_TYPE_UNION:
      return "\"" + YangModel::utf8_to_escaped_json_string(sample) + "\"";

    case RW_YANG_LEAF_TYPE_INSTANCE_ID:
      return "\"/example:top/example:list[example:key='value']/...\"";

    case RW_YANG_LEAF_TYPE_ANYXML:
      return "null";

    default:
      RW_ASSERT_NOT_REACHED();
  }
  return sample;
}

std::string YangNode::get_rwrest_json_sample_element(
  unsigned indent,
  unsigned to_depth,
  bool config_only )
{
  if (config_only && !is_config()) {
    return "";
  }

  std::string sample;
  auto stmt_type = get_stmt_type();

  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_ANYXML:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
    case RW_YANG_STMT_TYPE_LEAF:
      sample = get_rwrest_json_sample_leaf_value();
      if (0 == sample.length()) {
        sample = "null";
      }

      if (RW_YANG_STMT_TYPE_LEAF_LIST == stmt_type) {
        /*
         * "leaf_list": [ a, b ]
         */
        sample = "[ " + sample + ", ... ]";
      } else {
        /*
         * "leaf": "val"
         */
        ; // No extra output at this time.
      }
      return sample;

    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_NOTIF:
      // Complex multi-line samples, output below.
      break;

    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_NULL:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (0 == to_depth) {
    // Depth of zero, just fully truncate entire contents.
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_RPCIO:
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_NOTIF:
        return "{ ... }";

      case RW_YANG_STMT_TYPE_LIST:
        return "[ { ... }, { ... } ]";

      default:
        RW_ASSERT_NOT_REACHED();
    }
  }

  std::string nl_end;
  std::string nl_member;
  auto new_indent = indent + 2;

  if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
    /*
     * "list": [
     *   {
     *     "member": ...
     *   }, {
     *     "member": ...
     *   }
     * ]
     */
    std::string nl_list = "\n" + std::string(indent+2,' ');
    nl_member = "\n" + std::string(indent+4,' ');
    nl_end = nl_list + "}, {" + nl_member + "..."
           + nl_list + "}\n" + std::string(indent,' ') + "]";
    sample += "[" + nl_list + "{";
    new_indent += 2;
  } else {
    /*
     * "container": {
     *   "member": ...
     * }
     */
    RW_ASSERT(!is_leafy());
    nl_end = "\n" + std::string(indent,' ') + "}";
    nl_member = "\n" + std::string(indent+2,' ');
    sample += "{";
  }

  std::string comma = "";

  for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
    if (config_only && !yni->is_config()) {
      continue;
    }
    sample += comma + nl_member
            + "\"" + yni->get_rest_element_name() + "\": "
            + yni->get_rwrest_json_sample_element(
                new_indent, to_depth-1, config_only );
    comma = ",";
  }

  return sample + nl_end;
}

uint32_t YangNode::get_max_elements()
{
  // If not listy, the max is 1.  ATTN: Maybe 0?
  return 1;
}

bool YangNode::is_config()
{
  // Assume not config.
  return false;
}

bool YangNode::is_leafy()
{
  switch (get_stmt_type()) {
    case RW_YANG_STMT_TYPE_ANYXML:
    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      return true;

    default:
      return false;
  }
}

bool YangNode::is_listy()
{
  // ATTN: And bits leaf?
  switch (get_stmt_type()) {
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      return true;

    default:
      return false;
  }
}

bool YangNode::is_key()
{
  // Assume not a key.
  return false;
}

bool YangNode::has_keys()
{
  if (get_stmt_type() != RW_YANG_STMT_TYPE_LIST) {
    return false;
  }
  return (nullptr != get_first_key());
}

bool YangNode::is_presence()
{
  // Assume not a presence container.
  return false;
}

bool YangNode::is_mandatory()
{
  // Assume not mandatory.
  return false;
}

bool YangNode::has_default()
{
  // Assume no default descendant
  return false;
}

YangNode* YangNode::get_parent()
{
  // Assume there is no parent.
  return nullptr;
}

YangNodeIter YangNode::child_begin()
{
  // Create an iterator on the first child, if any.
  return YangNodeIter(get_first_child());
}

YangNodeIter YangNode::child_end()
{
  // Create a null iterator.
  return YangNodeIter();
}

YangNode* YangNode::search_child(const char* name, const char *ns)
{
  RW_ASSERT(name);
  RW_ASSERT(name[0]);

  // Iterate over all the children, looking for a match.
  for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
    const char* cname = yni->get_name();
    const char* cns = yni->get_ns();
    RW_ASSERT(cname);
    if (((nullptr == ns) || (0 == strcmp(ns, cns))) && (0 == strcmp(cname, name))) {
      return &*yni;
    }
  }

  if (get_stmt_type() == RW_YANG_STMT_TYPE_RPC) {
    // the names are those of the child RPC nodes
    YangNode *yn = nullptr;
    for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
      yn = yni->search_child (name, ns);
      if (yn != nullptr) {
        return yn;
      }
    }
  }
  return nullptr;
}

YangNode* YangNode::search_child_with_prefix(const char * name, const char * prefix)
{
  RW_ASSERT(name);
  RW_ASSERT(name[0]);

  // Iterate over all the children, looking for a match.
  for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
    const char* cname = yni->get_name();
    const char* cprefix = yni->get_prefix();
    RW_ASSERT(cname);
    if (((nullptr == prefix) || (0 == strcmp(prefix, cprefix))) && (0 == strcmp(cname, name))) {
      return &*yni;
    }
  }

  if (get_stmt_type() == RW_YANG_STMT_TYPE_RPC) {
    // the names are those of the child RPC nodes
    YangNode *yn = nullptr;
    for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
      yn = yni->search_child_with_prefix (name, prefix);
      if (yn != nullptr) {
        return yn;
      }
    }
  }
  return nullptr;
}

YangNode* YangNode::search_child(uint32_t system_ns_id, uint32_t element_tag)
{
  UNUSED (system_ns_id);
  const rw_yang_pb_msgdesc_t* ypbc_msgdesc = get_ypbc_msgdesc();

  if (get_stmt_type() == RW_YANG_STMT_TYPE_RPC) {
    for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
      ypbc_msgdesc = yni->get_ypbc_msgdesc();
      if (nullptr == ypbc_msgdesc) {
        return nullptr;
      }
      if (ypbc_msgdesc->pb_element_tag == element_tag) {
        return &(*yni);
      }
    }
    return nullptr;
  }

  if (nullptr == ypbc_msgdesc) {
    YangModel *ym = get_model();
    return ym->search_top_yang_node(system_ns_id, element_tag);
  }

  const ProtobufCFieldDescriptor *fld =
      protobuf_c_message_descriptor_get_field(ypbc_msgdesc->pbc_mdesc, element_tag);

  RW_ASSERT (fld);

  // ATTN: use the ns_id either as a parameter to the next call, or to find the ns string
  return search_child_by_mangled_name(fld->name);
}

YangNode* YangNode::search_child_by_mangled_name(const char* name, const char *ns)
{
  RW_ASSERT(name);

  // Iterate over all the children, looking for a match.
  for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
    std::string cname = (YangModel::mangle_to_c_identifier(yni->get_name()));
    const char* cns = yni->get_ns();

    if (((nullptr == ns) || (0 == strcmp (ns, cns))) &&
        (0 == strcmp(cname.c_str(), name))) {
      return &*yni;
    }
  }

  if (get_stmt_type() == RW_YANG_STMT_TYPE_RPC) {
    // the names are those of the child RPC nodes
    YangNode *yn = nullptr;
    for (YangNodeIter yni = child_begin(); yni != child_end(); ++yni) {
      yn = yni->search_child_by_mangled_name (name, ns);
      if (yn != nullptr) {
        return yn;
      }
    }
  }

  return NULL;
}



YangType* YangNode::get_type()
{
  // Assume not leafy.
  return nullptr;
}

YangValue* YangNode::get_first_value()
{
  // Assume not leafy.
  return nullptr;
}

YangValueIter YangNode::value_begin()
{
  // Create an iterator on the first value, if any.
  return YangValueIter(get_first_value());
}

YangValueIter YangNode::value_end()
{
  // Create a null iterator.
  return YangValueIter();
}

YangKey* YangNode::get_first_key()
{
  // Assume not a list with keys.
  return nullptr;
}

YangKeyIter YangNode::key_begin()
{
  // Create an iterator on the first key, if any.
  return YangKeyIter(get_first_key());
}

YangKeyIter YangNode::key_end()
{
  // Create a null iterator.
  return YangKeyIter();
}

YangNode* YangNode::get_key_at_index(uint8_t position)
{
  int i = 0;
  for (YangKeyIter yki=key_begin(); yki != key_end(); yki++) {
    i++;
    if (i == position) {
      return yki->get_key_node();
    }
  }
  return nullptr;
}

YangExtension* YangNode::get_first_extension()
{
  // Assume no extensions.
  return nullptr;
}

YangExtensionIter YangNode::extension_begin()
{
  // Create an iterator on the first extension, if any.
  return YangExtensionIter(get_first_extension());
}

YangExtensionIter YangNode::extension_end()
{
  // Create a null iterator.
  return YangExtensionIter();
}

YangExtension* YangNode::search_extension(const char* ns, const char* ext)
{
  // Search all the extensions.
  // ATTN: This may need to change when uses and grouping become first-class objects.
  YangExtensionIter yei = extension_begin();
  if (yei != extension_end()) {
    return yei->search(ns,ext);
  }
  return NULL;
}

YangModule* YangNode::get_module_orig()
{
  // Assume not a uses or augment.
  // ATTN: Should the implementer return get_module() if this would otherwise be nullptr?
  // ATTN: Should force reimplementation?
  return get_module();
}

YangAugment* YangNode::get_augment()
{
  // Assume not an augment.
  return nullptr;
}

YangUses* YangNode::get_uses()
{
  // Assume no uses
  return nullptr;
}

YangNode* YangNode::get_next_grouping()
{
  // Assume no grouping
  return nullptr;
}

bool YangNode::matches_prefix(const char* value_string)
{
  size_t i;
  const char* name = get_name();
  for (i = 0; name[i] != '\0' && value_string[i] != '\0' && name[i] == value_string[i]; ++i) {
    ; // null loop
  }

  if (value_string[i] == '\0') {
    // exact match is also considered good
    return true;
  }

  return false;
}

rw_status_t YangNode::parse_value(const char* value, YangValue** matched)
{
  RW_ASSERT(value);
  YangValue* found = nullptr;
  rw_status_t retval = RW_STATUS_FAILURE;

  // Just forward to the type, if any.
  YangType* ytype = get_type();
  if (ytype) {
    retval = ytype->parse_value(value, &found);
  }

  if (matched) {
    if (RW_STATUS_SUCCESS == retval) {
      *matched = found;
    } else {
      *matched = nullptr;
    }
  }

  return retval;
}

const char* YangNode::get_pbc_field_name()
{
  // Assume no field name.
  return nullptr;
}


void YangNode::set_mode_path()
{
  // Unsupported by default.
  // ATTN: Should have a way to return an error?
}

bool YangNode::is_mode_path()
{
  // Assume the node is not on a mode path
  return false;
}

bool YangNode::is_rpc()
{
  // Assume the node is not a RPC
  return false;
}

bool YangNode::is_rpc_input()
{
  // Assume the node is not a RPC input
  return false;
}

bool YangNode::is_rpc_output()
{
  // Assume the node is not a RPC output
  return false;
}

bool YangNode::is_notification()
{
  // Assume the node is not a notification
  return false;
}

bool YangNode::is_root()
{
  // Assume the node is not the root
  return false;
}

std::string YangNode::get_enum_string(uint32_t value)
{
  // Wrong logic in code. Shouldnt be called?
  return std::string("");
}

YangNode* YangNode::get_leafref_ref()
{
  // Assume the node is not a leaf of type leafref.
  return nullptr;
}

std::string YangNode::get_leafref_path_str()
{
  return std::string();
}

YangNode* YangNode::get_reusable_grouping()
{
  return nullptr;
}

YangModule* YangNode::get_reusable_grouping_module()
{
  YangNode *grp = get_reusable_grouping();

  if (grp) {
    return (grp->get_module());
  }
  return nullptr;
}

bool YangNode::app_data_is_cached(const AppDataTokenBase* token) const noexcept
{
  // Default implementation doesn't support app data
  UNUSED(token);
  return false;
}

bool YangNode::app_data_is_looked_up(const AppDataTokenBase* token) const noexcept
{
  // Default implementation doesn't support app data; pretend it has already been looked up
  UNUSED(token);
  return true;
}

bool YangNode::app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  // Default implementation doesn't support app data
  UNUSED(token);
  UNUSED(data);
  return false;
}

bool YangNode::app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data)
{
  // Default implementation doesn't support app data
  UNUSED(token);
  UNUSED(data);
  return false;
}

void YangNode::app_data_set_looked_up(const AppDataTokenBase* token)
{
  UNUSED(token);
}

intptr_t YangNode::app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  // Default implementation doesn't support app data; just give the pointer back.
  UNUSED(token);
  return data;
}

intptr_t YangNode::app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  // Default implementation doesn't support app data; just ignore
  UNUSED(token);
  UNUSED(data);
  return 0;
}

YangNode* YangNode::get_case()
{
  return nullptr;
}

YangNode* YangNode::get_choice()
{
  return nullptr;
}

YangNode* YangNode::get_default_case()
{
  return nullptr;
}

bool YangNode::is_conflicting_node(YangNode *other)
{
  // Assume no conflict
  return false;
}

rw_status_t YangNode::register_ypbc_msgdesc(const rw_yang_pb_msgdesc_t* ypbc_msgdesc)
{
  // set the annotation
  YangModel *model = get_model();
  app_data_set_and_keep_ownership(&model->adt_schema_node_, ypbc_msgdesc);

  for (size_t i = 0; i < ypbc_msgdesc->child_msg_count; i++) {
    const rw_yang_pb_msgdesc_t *cm = ypbc_msgdesc->child_msgs[i];
    YangNode *yn = search_child(cm->yang_node_name, cm->module->ns);

    if (nullptr == yn) {
      // ATTN: When a module uses a grouping defined in a different
      // module, the module name is set to the "using" module.
      // Augments works the other way - the parent and child will be
      // in different namespaces, as defined by YANG.

      // This node could come from a uses from a different module -
      // try finding it with this nodes name space.
      yn = search_child(cm->yang_node_name, get_ns());

      if (!yn) {
        // ATTN: Quick hack fix for RIFT-9576. Proper yangpbc fix still required.
        yn = search_child(cm->yang_node_name, nullptr);
      }
    }
    RW_ASSERT (yn);
    rw_status_t rs = yn->register_ypbc_msgdesc(cm);
    RW_ASSERT (RW_STATUS_SUCCESS == rs);
  }

  return RW_STATUS_SUCCESS;
}

const rw_yang_pb_msgdesc_t* YangNode::get_ypbc_msgdesc()
{
  YangModel *model = get_model();
  const rw_yang_pb_msgdesc_t *ret = nullptr;

  app_data_check_and_get (&model->adt_schema_node_, &ret);

  return ret;
}



/*****************************************************************************/
// YangNode C APIs

void rw_yang_node_get_location(rw_yang_node_t* ynode, char** str)
{
  std::string loc = static_cast<YangNode*>(ynode)->get_location();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

const char* rw_yang_node_get_description(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_description());
}

const char* rw_yang_node_get_help_short(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_help_short());
}

const char* rw_yang_node_get_help_full(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_help_full());
}


const char* rw_yang_node_get_name(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_name());
}

const char* rw_yang_node_get_prefix(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_prefix());
}

const char* rw_yang_node_get_ns(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_ns());
}

const char* rw_yang_node_get_default_value(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_default_value());
}

uint32_t rw_yang_node_get_max_elements(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_max_elements());
}

rw_yang_stmt_type_t rw_yang_node_get_stmt_type(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_stmt_type());
}

rw_yang_node_t * rw_yang_node_get_leafref_ref(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->get_leafref_ref());
}


bool_t rw_yang_node_is_config(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_config());
}

bool_t rw_yang_node_is_leafy(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_leafy());
}

bool_t rw_yang_node_is_listy(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_listy());
}

bool_t rw_yang_node_is_key(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_key());
}

bool_t rw_yang_node_has_keys(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->has_keys());
}

bool_t rw_yang_node_is_presence(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_presence());
}

bool_t rw_yang_node_is_mandatory(rw_yang_node_t* ynode)
{
  return (static_cast<YangNode*>(ynode)->is_mandatory());
}

rw_yang_node_t* rw_yang_node_get_parent(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->get_parent());
}

rw_yang_node_t* rw_yang_node_get_first_child(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->get_first_child());
}

// ATTN rw_yang_node_t* rw_yang_node_child_begin(rw_yang_node_t* ynode);
/*
rw_yang_node_t* rw_yang_node_child_begin(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->child_begin());
}
*/

// ATTN rw_yang_node_t* rw_yang_node_child_end(rw_yang_node_t* ynode);
/*
rw_yang_node_t* rw_yang_node_child_end(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->child_end());
}
*/


rw_yang_node_t* rw_yang_node_get_next_sibling(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->get_next_sibling());
}

rw_yang_node_t* rw_yang_node_search_child(rw_yang_node_t* ynode, const char* name, const char *ns)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->search_child(name, ns));
}

rw_yang_node_t* rw_yang_node_search_child_with_prefix(rw_yang_node_t * ynode, const char * name, const char * prefix)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->search_child_with_prefix(name, prefix));
}

// ATTN: rw_yang_node_t* rw_yang_node_search_descendant_path(rw_yang_node_t* ynode, const rw_yang_path_element_t* const* path);
/*
rw_yang_node_t* rw_yang_node_search_descendant_path(rw_yang_node_t* ynode, const rw_yang_path_element_t* const* path)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangNode*>(ynode)->search_descendant_path(path));
}
*/


rw_yang_type_t* rw_yang_node_node_type(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_type_t*>(static_cast<YangNode*>(ynode)->get_type());
}

rw_yang_value_t* rw_yang_node_get_first_value(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_value_t*>(static_cast<YangNode*>(ynode)->get_first_value());
}

// ATTN: rw_yang_value_t* rw_yang_node_value_begin(rw_yang_node_t* ynode);
/*
rw_yang_value_t* rw_yang_node_value_begin(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_value_t*>(static_cast<YangNode*>(ynode)->value_begin());
}
*/

// ATTN: rw_yang_value_t* rw_yang_node_value_end(rw_yang_node_t* ynode);
/*
rw_yang_value_t* rw_yang_node_value_end(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_value_t*>(static_cast<YangNode*>(ynode)->value_end());
}
*/


rw_yang_key_t* rw_yang_node_get_first_key(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_key_t*>(static_cast<YangNode*>(ynode)->get_first_key());
}

// ATTN: rw_yang_key_t* rw_yang_node_key_begin(rw_yang_node_t* ynode);
/*
rw_yang_key_t* rw_yang_node_key_begin(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_key_t*>(static_cast<YangNode*>(ynode)->key_begin());
}
*/


// ATTN: rw_yang_extension_t* rw_yang_node_key_end(rw_yang_node_t* ynode);
rw_yang_extension_t* rw_yang_node_get_first_extension(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangNode*>(ynode)->get_first_extension());
}

// ATTN: rw_yang_extension_t* rw_yang_node_extension_begin(rw_yang_node_t* ynode);
/*
rw_yang_extension_t* rw_yang_node_extension_begin(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangNode*>(ynode)->extension_begin());
}
*/

// ATTN: rw_yang_extension_t* rw_yang_node_extension_end(rw_yang_node_t* ynode);
/*
rw_yang_extension_t* rw_yang_node_extension_end(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangNode*>(ynode)->extension_end());
}
*/


rw_yang_extension_t* rw_yang_node_search_extension(rw_yang_node_t* ynode, const char* ns, const char* ext)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangNode*>(ynode)->search_extension(ns, ext));
}

rw_yang_module_t* rw_yang_node_get_module(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangNode*>(ynode)->get_module());
}

rw_yang_module_t* rw_yang_node_get_module_orig(rw_yang_node_t* ynode)
{
  return static_cast<rw_yang_module_t*>(static_cast<YangNode*>(ynode)->get_module_orig());
}

// ATTN: bool_t rw_yang_node_matches_prefix(rw_yang_node_t* ynode, const char* prefix_string);
bool_t rw_yang_node_matches_prefix(rw_yang_node_t* ynode, const char* prefix_string)
{
  return static_cast<bool_t>(static_cast<YangNode*>(ynode)->matches_prefix(prefix_string));
}


// ATTN: rw_status_t rw_yang_node_parse_value(rw_yang_node_t* ynode, const char* value_string, rw_yang_value_t** matched);
rw_status_t rw_yang_node_parse_value(rw_yang_node_t* ynode, const char* value_string, rw_yang_value_t** matched)
{
  YangValue* found = nullptr;
  rw_status_t retval = RW_STATUS_FAILURE;
  retval = static_cast<YangNode*>(ynode)->parse_value(value_string, &found);

  if (matched) {
    if (RW_STATUS_SUCCESS == retval) {
      *matched = found;
    } else {
      *matched = nullptr;
    }
  }
  return retval;
}


// ATTN: const ProtobufCMessageDescriptor* rw_yang_node_get_pbc_message_desc();

// ATTN: rw_status_t rw_yang_node_set_pbc_message_desc(const ProtobufCMessageDescriptor* pbc_message_desc);

// ATTN: const char* rw_yang_node_get_pbc_field_name();

// ATTN: const ProtobufCFieldDescriptor* rw_yang_node_get_pbc_field_desc();

// ATTN: rw_status_t rw_yang_node_set_pbc_field_desc(const ProtobufCFieldDescriptor* pbc_field_desc);

// ATTN: bool_t rw_yang_node_set_mode_path(rw_yang_node_t* ynode);
/*
bool_t rw_yang_node_set_mode_path(rw_yang_node_t* ynode)
{
  static_cast<YangNode*>(ynode)->set_mode_path();
  return;
}
*/

// ATTN: bool_t rw_yang_node_is_mode_path(rw_yang_node_t* ynode);
bool_t rw_yang_node_is_mode_path(rw_yang_node_t* ynode)
{
  return static_cast<YangNode*>(ynode)->is_mode_path();
}

// ATTN: bool_t rw_yang_node_is_rpc(rw_yang_node_t* ynode);
bool_t rw_yang_node_is_rpc(rw_yang_node_t* ynode)
{
  return static_cast<YangNode*>(ynode)->is_rpc();
}

// ATTN: bool_t rw_yang_node_is_rpc_input(rw_yang_node_t* ynode);
bool_t rw_yang_node_is_rpc_input(rw_yang_node_t* ynode)
{
  return static_cast<YangNode*>(ynode)->is_rpc_input();
}

// ATTN: bool_t rw_yang_node_is_rpc_output(rw_yang_node_t* ynode);
bool_t rw_yang_node_is_rpc_output(rw_yang_node_t* ynode)
{
  return static_cast<YangNode*>(ynode)->is_rpc_output();
}



/*****************************************************************************/
// YangKey C APIs

rw_yang_node_t* rw_yang_key_get_list_node(rw_yang_key_t* ykey)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangKey*>(ykey)->get_list_node());
}

rw_yang_node_t* rw_yang_key_get_key_node(rw_yang_key_t* ykey)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangKey*>(ykey)->get_key_node());
}

rw_yang_key_t* rw_yang_key_get_next_key(rw_yang_key_t* ykey)
{
  return static_cast<rw_yang_key_t*>(static_cast<YangKey*>(ykey)->get_next_key());
}

/*****************************************************************************/
/*!
 * Compare a string against the type, returnning the first value that
 * accepts the input.  The list skips unions - they get expanded
 * inline.  The returned value descriptor is owned by the model.  Will
 * be NULL if no match is found or there are no value descriptors.
 * Caller may use value descriptor until model is destroyed.
 */
rw_status_t YangType::parse_value(const char* value_string, YangValue** matched)
{
  RW_ASSERT(value_string);
  RW_ASSERT(matched);
  *matched = NULL;

  for (YangValueIter vi(this); vi != YangValueIter(); ++vi) {
    rw_status_t rs = vi->parse_value(value_string);
    if (RW_STATUS_SUCCESS == rs) {
      *matched = &*vi;
      return rs;
    }
  }

  return RW_STATUS_FAILURE;
}

/*!
 * Count the number of values that match a value string.
 */
unsigned YangType::count_matches(const char* value_string)
{
  RW_ASSERT(value_string);
  unsigned count = 0;

  for (YangValueIter vi(this); vi != YangValueIter(); ++vi) {
    rw_status_t rs = vi->parse_value(value_string);
    if (RW_STATUS_SUCCESS == rs) {
      ++count;
    }
  }

  return count;
}

/*!
 * Count the number of values that match a partial value string.
 */
unsigned YangType::count_partials(const char* value_string)
{
  RW_ASSERT(value_string);
  unsigned count = 0;

  for (YangValueIter vi(this); vi != YangValueIter(); ++vi) {
    rw_status_t rs = vi->parse_partial(value_string);
    if (RW_STATUS_SUCCESS == rs) {
      ++count;
    }
  }

  return count;
}

/*!
 * Return an iterator pointing at the first value descriptor of a type.
 */
YangValueIter YangType::value_begin()
{
  return YangValueIter(get_first_value());
}

/*!
 * Return an iterator pointing to no value descriptor.
 */
YangValueIter YangType::value_end()
{
  return YangValueIter();
}

/*!
 * Return an iterator pointing at the first extension descriptor.
 */
YangExtensionIter YangType::extension_begin()
{
  return YangExtensionIter(get_first_extension());
}

/*!
 * Return an iterator pointing to the end of the extension list.
 */
YangExtensionIter YangType::extension_end()
{
  return YangExtensionIter();
}


// YangType C APIs
void rw_yang_type_get_location(rw_yang_type_t* ytype, char** str /*!< Allocated string, owned by the caller! */)
{
  std::string loc = static_cast<YangType*>(ytype)->get_location();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

const char* rw_yang_type_get_description(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_description());
}

const char* rw_yang_type_get_help_short(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_help_short());
}

const char* rw_yang_type_get_help_full(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_help_full());
}

const char* rw_yang_type_get_prefix(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_prefix());
}

const char* rw_yang_type_get_ns(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_ns());
}

/*
rw_yang_value_t* rw_yang_type_value_begin(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->value_begin());
}

rw_yang_value_t* rw_yang_type_value_end(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->value_end());
}
*/

rw_yang_leaf_type_t rw_yang_type_get_leaf_type(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_leaf_type());
}

rw_yang_value_t* rw_yang_type_get_first_value(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_first_value());
}

rw_status_t rw_yang_type_parse_value(rw_yang_type_t* ytype, const char* value_string, rw_yang_value_t** matched)
{
  YangValue* local_matched = nullptr;
  rw_status_t rs = static_cast<YangType*>(ytype)->parse_value(value_string, &local_matched);
  if (rs == RW_STATUS_SUCCESS)
    *matched = local_matched;
  return rs;
}

unsigned rw_yang_type_count_matches(rw_yang_type_t* ytype, const char* value_string)
{
  return (static_cast<YangType*>(ytype)->count_matches(value_string));
}

unsigned rw_yang_type_count_partials(rw_yang_type_t* ytype, const char* value_string)
{
  return (static_cast<YangType*>(ytype)->count_partials(value_string));
}

rw_yang_extension_t* rw_yang_type_get_first_extension(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->get_first_extension());
}

/*
rw_yang_extension_t* rw_yang_type_extension_begin(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->extension_begin());
}

rw_yang_extension_t* rw_yang_type_extension_end(rw_yang_type_t* ytype)
{
  return (static_cast<YangType*>(ytype)->extension_end());
}
*/




/*****************************************************************************/
/*!
 * Return an iterator pointing at the first extension descriptor.
 */
YangExtensionIter YangValue::extension_begin()
{
  return YangExtensionIter(get_first_extension());
}

/*!
 * Return an iterator pointing to the end of the extension list.
 */
YangExtensionIter YangValue::extension_end()
{
  return YangExtensionIter();
}


// YangValue C APIs
const char* rw_yang_value_get_name(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_name());
}

void rw_yang_value_get_location(rw_yang_value_t* yvalue, char** str /*!< Allocated string, owned by the caller! */)
{
  std::string loc = static_cast<YangValue*>(yvalue)->get_location();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

const char* rw_yang_value_get_description(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_description());
}

const char* rw_yang_value_get_help_short(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_help_short());
}

const char* rw_yang_value_get_help_full(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_help_full());
}

const char* rw_yang_value_get_prefix(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_prefix());
}

const char* rw_yang_value_get_ns(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_ns());
}

rw_yang_leaf_type_t rw_yang_value_get_leaf_type(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_leaf_type());
}

uint64_t rw_yang_value_get_max_length(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_max_length());
}

rw_yang_value_t* rw_yang_value_get_next_value(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->get_next_value());
}

rw_status_t rw_yang_value_parse_value(rw_yang_value_t* yvalue, const char* value_string)
{
  return (static_cast<YangValue*>(yvalue)->parse_value(value_string));
}

rw_status_t rw_yang_value_parse_partial(rw_yang_value_t* yvalue, const char* value_string)
{
  return (static_cast<YangValue*>(yvalue)->parse_partial(value_string));
}

bool_t rw_yang_value_is_keyword(rw_yang_value_t* yvalue)
{
  return (static_cast<YangValue*>(yvalue)->is_keyword());
}

rw_yang_extension_t* rw_yang_value_get_first_extension(rw_yang_value_t* yvalue)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangValue*>(yvalue)->get_first_extension());
}

/*
rw_yang_extension_t* rw_yang_value_extension_begin(rw_yang_value_t* yvalue)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangValue*>(yvalue)->extension_begin());
}


rw_yang_extension_t* rw_yang_value_extension_end(rw_yang_value_t* yvalue)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangValue*>(yvalue)->extension_end());
}
*/



/*****************************************************************************/

/*!
 * Search a chain of extensions for a specific one.
 */
YangExtension* YangExtension::search( const char* ns, const char* ext )
{
  if (is_match(ns,ext)) {
    return this;
  }

  YangExtension* next = get_next_extension();
  if (next) {
    return next->search(ns,ext);
  }

  return NULL;
}

bool YangExtension::is_match(const char* ns, const char* ext)
{
  return 0 == strcmp(ns,get_ns()) && 0 == strcmp(ext,get_name());
}


// YangExtension C APIs
void rw_yang_extension_get_location(rw_yang_extension_t* yext, char** str /*!< Allocated string, owned by the caller! */)
{
  std::string loc = static_cast<YangExtension*>(yext)->get_location();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

void rw_yang_extension_get_location_ext(rw_yang_extension_t* yext, char** str /*!< Allocated string, owned by the caller! */)
{
  std::string loc = static_cast<YangExtension*>(yext)->get_location_ext();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

const char* rw_yang_extension_get_value(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_value());
}

const char* rw_yang_extension_get_description_ext(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_description_ext());
}

const char* rw_yang_extension_get_name(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_name());
}

const char* rw_yang_extension_get_module_ext(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_module_ext());
}

const char* rw_yang_extension_get_prefix(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_prefix());
}

const char* rw_yang_extension_get_ns(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_ns());
}

rw_yang_extension_t* rw_yang_extension_get_next_extension(rw_yang_extension_t* yext)
{
  return (static_cast<YangExtension*>(yext)->get_next_extension());
}

rw_yang_extension_t* rw_yang_extension_search(rw_yang_extension_t* yext, const char* module, const char* ext)
{
  return (static_cast<YangExtension*>(yext)->search(module, ext));
}

bool_t rw_yang_extension_is_match(rw_yang_extension_t* yext, const char* module, const char* ext)
{
  return (static_cast<YangExtension*>(yext)->is_match(module, ext));
}



/*****************************************************************************/
/*!
 * Return an iterator pointing at the first node in the module.
 */
YangNodeIter YangModule::node_begin()
{
  return YangNodeIter(get_first_node());
}

/*!
 * Return an iterator pointing past the last node in the module.
 */
YangNodeIter YangModule::node_end()
{
  return YangNodeIter();
}

/*!
 * Return an iterator pointing at the first extension descriptor.
 */
YangExtensionIter YangModule::extension_begin()
{
  return YangExtensionIter(get_first_extension());
}

/*!
 * Return an iterator pointing to the end of the extension list.
 */
YangExtensionIter YangModule::extension_end()
{
  return YangExtensionIter();
}

/*!
 * Return an iterator pointing at the first augment descriptor.
 */
YangAugmentIter YangModule::augment_begin()
{
  return YangAugmentIter(get_first_augment());
}

/*!
 * Return an iterator pointing to the end of the augment list.
 */
YangAugmentIter YangModule::augment_end()
{
  return YangAugmentIter();
}
/*!
 * Return an iterator to the start of module level typedef-ed enumerations
 */

YangTypedEnumIter YangModule::typed_enum_begin()
{
  return YangTypedEnumIter(get_first_typed_enum());
}

/*!
 * Return an iterator to the end of module level typedef-ed enumerations
 */

YangTypedEnumIter YangModule::typed_enum_end()
{
  return YangTypedEnumIter();
}

/*!
 * Return an iterator to the start of module level imports
 */

YangImportIter  YangModule::import_begin()
{
  return YangImportIter(get_first_import());
}

/*!
 * Return an iterator to the end of module level imports
 */
YangImportIter  YangModule::import_end()
{
  return YangImportIter();
}

/*!
 * Return an iterator to the start of module level groupings
 */

YangGroupingIter  YangModule::grouping_begin()
{
  return YangGroupingIter(get_first_grouping());
}

/*!
 * Return an iterator to the end of module level groupings
 */

YangGroupingIter  YangModule::grouping_end()
{
  return YangGroupingIter();
}


/*****************************************************************************/
// YangModule C APIs
rw_yang_node_t* rw_yang_module_get_first_node(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_node_t*>(static_cast<YangModule*>(ymod)->get_first_node());
}

// ATTN rw_yang_node_t* rw_yang_module_node_begin(rw_yang_module_t* ymod);
/*
rw_yang_node_t* rw_yang_module_node_begin(rw_yang_module_t* ymod);
{
  return static_cast<rw_yang_node_t*>(static_cast<YangModule*>(ymod)-node_begin());
}
*/

rw_yang_node_t* rw_yang_module_node_end(rw_yang_module_t* ymod)
{
  YangNodeIter i = static_cast<YangModule*>(ymod)->node_end();
  if (i == YangNodeIter()) {
    return nullptr;
  }
  return static_cast<rw_yang_node_t*>(&*i);
}

void rw_yang_module_get_location(rw_yang_module_t* ymod, char** str /*!< Allocated string, owned by the caller! */)
{
  std::string loc = static_cast<YangModule*>(ymod)->get_location();
  char* newstr = strdup(loc.c_str());
  *str = newstr;
  return;
}

const char* rw_yang_module_get_description(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->get_description();
}

const char* rw_yang_module_get_name(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->get_name();
}

const char* rw_yang_module_get_prefix(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->get_prefix();
}

const char* rw_yang_module_get_ns(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->get_ns();
}

rw_yang_module_t* rw_yang_module_get_next_module(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->get_next_module();
}

bool_t rw_yang_module_was_loaded_explicitly(rw_yang_module_t* ymod)
{
  return static_cast<YangModule*>(ymod)->was_loaded_explicitly();
}

rw_yang_extension_t* rw_yang_module_get_first_extension(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangModule*>(ymod)->get_first_extension());
}

/*
rw_yang_extension_t* rw_yang_module_extension_begin(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangModule*>(ymod)->extension_begin());
}

rw_yang_extension_t* rw_yang_module_extension_end(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_extension_t*>(static_cast<YangModule*>(ymod)->extension_end());
}
*/

rw_yang_augment_t* rw_yang_module_get_first_augment(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_augment_t*>(static_cast<YangModule*>(ymod)->get_first_augment());
}

/*
rw_yang_augment_t* rw_yang_module_augment_begin(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_augment_t*>(static_cast<YangModule*>(ymod)->augment_begin());
}

rw_yang_augment_t* rw_yang_module_augment_end(rw_yang_module_t* ymod)
{
  return static_cast<rw_yang_augment_t*>(static_cast<YangModule*>(ymod)->augment_end());
}
*/

#if 0
bool_t  rw_yang_module_app_data_is_cached(rw_yang_module_t* ymod, const rw_app_data_token_base_t *token)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_is_cached(static_cast<AppDataTokenBase*>(token)));
}

bool_t  rw_yang_module_app_data_is_looked_up(rw_yang_module_t* ymod, const rw_app_data_token_base_t *token)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_is_looked_up(static_cast<AppDataTokenBase*>(token)));
}

bool_t  rw_yang_module_app_data_check_and_get(rw_yang_module_t* ymod,
                                              const rw_app_data_token_base_t *token,
                                              intptr_t* data)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_check_and_get(static_cast<AppDataTokenBase*>(token), data));
}

bool_t  rw_yang_module_app_data_check_lookup_and_get(rw_yang_module_t* ymod,
                                                     const rw_app_data_token_base_t *token,
                                                     intptr_t* data)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_check_lookup_and_get(static_cast<AppDataTokenBase*>(token), data));
}

bool_t  rw_yang_module_app_data_set_looked_up(rw_yang_module_t* ymod,
                                              const rw_app_data_token_base_t *token)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_set_looked_up(static_cast<AppDataTokenBase*>(token)));
}

intptr_t rw_yang_module_app_data_set_and_give_ownership(rw_yang_module_t* ymod,
                                                        const rw_app_data_token_base_t *token,
                                                        intptr_t data)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_set_and_give_ownership(static_cast<AppDataDataTokenBase*>(token), data));
}
intptr_t rw_yang_module_app_data_set_and_keep_ownership(rw_yang_module_t* ymod,
                                                        const rw_app_data_token_base_t *token,
                                                        intptr_t data)
{
  return static_cast<bool_t>(static_cast<YangModule*>(ymod)->app_data_set_and_keep_ownership(static_cast<AppDataDataTokenBase*>(token), data));
}
#endif

YangExtension* YangModule::search_extension(const char* ns, const char* ext)
{
  // Search all the extensions.
  // ATTN: This may need to change when uses and grouping become first-class objects.
  YangExtensionIter yei = extension_begin();
  if (yei != extension_end()) {
    return yei->search(ns,ext);
  }
  return NULL;
}

bool YangModule::app_data_is_cached(const AppDataTokenBase* token) const noexcept
{
  // Default implementation doesn't support app data
  UNUSED(token);
  return false;
}

bool YangModule::app_data_is_looked_up(const AppDataTokenBase* token) const noexcept
{
  // Default implementation doesn't support app data; pretend it has already been looked up
  UNUSED(token);
  return true;
}

bool YangModule::app_data_check_and_get(const AppDataTokenBase* token, intptr_t* data) const noexcept
{
  // Default implementation doesn't support app data
  UNUSED(token);
  UNUSED(data);
  return false;
}

bool YangModule::app_data_check_lookup_and_get(const AppDataTokenBase* token, intptr_t* data)
{
  // Default implementation doesn't support app data
  UNUSED(token);
  UNUSED(data);
  return false;
}

void YangModule::app_data_set_looked_up(const AppDataTokenBase* token)
{
  UNUSED(token);
}

intptr_t YangModule::app_data_set_and_give_ownership(const AppDataTokenBase* token, intptr_t data)
{
  // Default implementation doesn't support app data; just give the pointer back.
  UNUSED(token);
  return data;
}

intptr_t YangModule::app_data_set_and_keep_ownership(const AppDataTokenBase* token, intptr_t data)
{
  // Default implementation doesn't support app data; just ignore
  UNUSED(token);
  UNUSED(data);
  return 0;
}


rw_status_t YangModule::register_ypbc_module(
  const rw_yang_pb_module_t *ypbc_module)
{
  // Set the modules app data to its ypbc_module
  rw_status_t status = RW_STATUS_FAILURE;

  app_data_set_and_keep_ownership(&get_model()->adt_schema_module_, ypbc_module);

  if (ypbc_module->data_module) {
    for (size_t i = 0; i < ypbc_module->data_module->child_msg_count; i++) {
      const rw_yang_pb_msgdesc_t* child_msg = ypbc_module->data_module->child_msgs[i];
      YangNode *yn = search_node(child_msg->yang_node_name, child_msg->module->ns);
      RW_ASSERT(yn);
      status = yn->register_ypbc_msgdesc(child_msg);
      RW_ASSERT(RW_STATUS_SUCCESS == status);
    }
  }

  if (ypbc_module->rpci_module) {
    for (size_t i = 0; i < ypbc_module->rpci_module->child_msg_count; i++) {
      const rw_yang_pb_msgdesc_t* rpci_msg = ypbc_module->rpci_module->child_msgs[i];
      YangNode *yn = search_node(rpci_msg->yang_node_name, rpci_msg->module->ns);
      RW_ASSERT(yn);
      YangNode *rpc_input = yn->search_child("input");
      RW_ASSERT(rpc_input);
      status = rpc_input->register_ypbc_msgdesc(rpci_msg);
      RW_ASSERT(RW_STATUS_SUCCESS == status);
    }
  }

  if (ypbc_module->rpco_module) {
    for (size_t i = 0; i < ypbc_module->rpco_module->child_msg_count; i++) {
      const rw_yang_pb_msgdesc_t* rpco_msg = ypbc_module->rpco_module->child_msgs[i];
      YangNode *yn = search_node(rpco_msg->yang_node_name, rpco_msg->module->ns);
      RW_ASSERT(yn);
      YangNode *rpc_output = yn->search_child("output");
      RW_ASSERT(rpc_output);
      status = rpc_output->register_ypbc_msgdesc(rpco_msg);
      RW_ASSERT(RW_STATUS_SUCCESS == status);
    }
  }

  if (ypbc_module->notif_module) {
    for (size_t i = 0; i < ypbc_module->notif_module->child_msg_count; i++) {
      const rw_yang_pb_msgdesc_t* child_msg = ypbc_module->notif_module->child_msgs[i];
      YangNode *yn = search_node(child_msg->yang_node_name, child_msg->module->ns);
      RW_ASSERT(yn);
      status = yn->register_ypbc_msgdesc(child_msg);
      RW_ASSERT(RW_STATUS_SUCCESS == status);
    }
  }

  // ATTN: if (ypbc_module->group_root) { ... }

  return RW_STATUS_SUCCESS;
}

const rw_yang_pb_module_t* YangModule::get_ypbc_module()
{
  const rw_yang_pb_module_t *ret = 0;
  app_data_check_and_get(&get_model()->adt_schema_module_, &ret);
  return ret;
}

YangNode* YangModule::search_node(
  const char *name,
  const char *ns)
{
  RW_ASSERT(name);
  RW_ASSERT(name[0]);

  for (auto it = node_begin(); it != node_end(); it++) {
    if (   0 == strcmp(it->get_name(), name)
        && (   ns == nullptr
            || 0 == strcmp(it->get_ns(), ns))) {
      return &(*it);
    }
  }
  return nullptr;
}



/*****************************************************************************/
YangExtension* YangAugment::get_first_extension()
{
  // default implementation: no extension support
  return nullptr;
}

YangExtensionIter YangAugment::extension_begin()
{
  return YangExtensionIter(get_first_extension());
}

YangExtensionIter YangAugment::extension_end()
{
  return YangExtensionIter();
}



/*****************************************************************************/
/*!
 * Return an iterator pointing at the first value descriptor of a type.
 */
YangValueIter YangTypedEnum::enum_value_begin()
{
  return YangValueIter(get_first_enum_value());
}

/*!
 * Return an iterator pointing to no value descriptor.
 */
YangValueIter YangTypedEnum::enum_value_end()
{
  return YangValueIter();
}



/*****************************************************************************/

const char* rw_yang::YANGMODEL_ANNOTATION_KEY = "YANGMODEL";
const char* rw_yang::YANGMODEL_ANNOTATION_PBNODE = "pb_node";
const char* rw_yang::YANGMODEL_ANNOTATION_SCHEMA_NODE = "protobuf_schema_node";
const char* rw_yang::YANGMODEL_ANNOTATION_SCHEMA_MODULE = "protobuf_schema_module";

YangModel::YangModel()
{
}

std::string YangModel::filename_to_module(const char* filename)
{
  RW_ASSERT(filename);
  RW_ASSERT(filename[0] != '\0');

  // Determine the base module name.
  std::string retval(filename);
  size_t eop = retval.rfind('/');
  if (eop != std::string::npos) {
    retval.erase(0,eop+1);
  }

  static const char dot_yang[] = ".yang";
  size_t boe = retval.rfind(dot_yang);
  if (boe != std::string::npos) {
    retval.erase(boe);
  }

  return retval;
}

std::string YangModel::mangle_to_c_identifier(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  std::string mangled_id;

  bool last_was_underscore = false;
  for (const char* p = id; *p; ++p) {
    if (0 == mangled_id.length()) {
      const char* good = strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", *p);
      if (!good) {
        good = strchr("0123456789", *p);
        if (good) {
          // Convert leading integer to word
          static const char* numbers[10] = {
            "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };
          unsigned i = (unsigned char)*good - (unsigned char)'0';
          RW_ASSERT(i < 10);
          mangled_id.append(numbers[i]);
        }

        // Skip non-C identifer characters and underscores
        continue;
      }

      mangled_id.push_back(*p);
      continue;
    }

    const char* good = strchr("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", *p);
    if (good) {
      mangled_id.push_back(*p);
      last_was_underscore = false;
      continue;
    }

    if (last_was_underscore) {
      // Don't add more underscore for bad character
      continue;
    }

    // convert to underscore
    last_was_underscore = true;
    mangled_id.push_back('_');
  }

  // Eat trailing underscore
  if ('_' == mangled_id.back()) {
     mangled_id.pop_back();
  }

  if (0 == mangled_id.length()) {
    // Huh?  No identifier?
    mangled_id.push_back('x');
  }

  return mangled_id;
}

std::string YangModel::mangle_camel_case_to_underscore(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  bool last_was_underscore = false;
  bool last_was_upper = false;
  std::string mangled_id;
  for (const char* p = id; *p; ++p) {
    const char* good = strchr("0123456789abcdefghijklmnopqrstuvwxyz_", *p);
    if (good) {
      mangled_id.push_back(*p);
      if (*p == '_') {
        last_was_underscore = true;
      } else {
        last_was_underscore = false;
      }
      last_was_upper = false;
      continue;
    } else if (isupper(*p)) {
      // convert to underscore
      if (mangled_id.length() && !last_was_underscore && !last_was_upper && mangled_id.length()) {
        mangled_id.push_back('_');
      }
      mangled_id.push_back(tolower(*p));
      last_was_upper = true;
      continue;
    } else {
      RW_ASSERT_NOT_REACHED(); // invalid char
    }
  }
  // Eat trailing underscore
  if ('_' == mangled_id.back()) {
     mangled_id.pop_back();
  }
  if (0 == mangled_id.length()) {
    // Huh?  No identifier?
    RW_ASSERT_NOT_REACHED(); // empty conversion
  }
  return mangled_id;
}

std::string YangModel::mangle_to_camel_case(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  std::string mangled_id = mangle_to_c_identifier(id);

  for (size_t pos = 0;
       pos < mangled_id.length() && pos != std::string::npos;
       pos = mangled_id.find('_',pos)) {
    // Delete the underscores...
    while (pos < mangled_id.length() && mangled_id[pos] == '_') {
      mangled_id.replace(pos,1,"");
    }

    if (pos < mangled_id.length()) {
      // CamelCase the current char, if any
      // Assumes ASCII, which is correct for C identifiers.
      mangled_id[pos] = toupper(mangled_id[pos]);
    }
  }
  return mangled_id;
}

bool YangModel::mangle_is_c_identifier(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  if (!islower(id[0]) && !isupper(id[0])) {
    return false;
  }
  size_t span = strspn(id, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_");
  if (span != strlen(id)) {
    return false;
  }

  const char* num = strchr("0123456789", *id);
  return nullptr == num;
}

bool YangModel::mangle_is_camel_case(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  if (!isascii(id[0]) && !isupper(id[0])) {
    return false;
  }
  size_t span = strspn(id, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  if (span != strlen(id)) {
    return false;
  }
  return true;
}

bool YangModel::mangle_is_lower_case(const char* id)
{
  RW_ASSERT(id);
  RW_ASSERT(id[0] != '\0');
  if (!isascii(id[0]) && !islower(id[0])) {
    return false;
  }
  size_t span = strspn(id, "0123456789abcdefghijklmnopqrstuvwxyz_");
  if (span != strlen(id)) {
    return false;
  }
  return nullptr == strstr(id, "__");
}

std::string YangModel::utf8_to_escaped_json_string(
  const std::string& string )
{
  /*
   * RFC 7159, 7:
   *   The representation of strings is similar to conventions used in the C
   *   family of programming languages.  A string begins and ends with
   *   quotation marks.  All Unicode characters may be placed within the
   *   quotation marks, except for the characters that must be escaped:
   *   quotation mark, reverse solidus, and the control characters (U+0000
   *   through U+001F).
   */
  std::string json;
  json.reserve(string.length());

  for (const char c: string) {
    if (c == '\\' || c == '\"') {
      json.push_back('\\');
      json.push_back(c);
    } else if (c >= '\x00' && c <= '\x1F') {
      char pct_encoded[8];
      auto bytes = snprintf(pct_encoded, sizeof(pct_encoded), "\\u%04X", (unsigned int)c);
      RW_ASSERT(6 == bytes);
      json.append(pct_encoded);
    } else {
      json.push_back(c);
    }
  }
  return json;
}

std::string YangModel::utf8_to_escaped_uri_key_string(
  const std::string& string )
{
  /*
   * RFC 3986, 2.2:
   *   sub-delims  = "!" / "$" / "&" / "'" / "(" / ")"
   *                     / "*" / "+" / "," / ";" / "="
   * RFC 3986, 2.3:
   *   unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
   * RFC 3986, 3.3:
   *   path          = path-abempty    ; begins with "/" or is empty
   *                 / path-absolute   ; begins with "/" but not "//"
   *                 / path-noscheme   ; begins with a non-colon segment
   *                 / path-rootless   ; begins with a segment
   *                 / path-empty      ; zero characters
   *   path-abempty  = *( "/" segment )
   *   path-absolute = "/" [ segment-nz *( "/" segment ) ]
   *   path-noscheme = segment-nz-nc *( "/" segment )
   *   path-rootless = segment-nz *( "/" segment )
   *   path-empty    = 0<pchar>
   *   segment       = *pchar
   *   segment-nz    = 1*pchar
   *   segment-nz-nc = 1*( unreserved / pct-encoded / sub-delims / "@" )
   *                 ; non-zero-length segment without any colon ":"
   *   pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
   *
   * RESTCONF draft 08, 3.5.1:
   *
   *   - If there are multiple key leaf values, the value of each leaf
   *     identified in the "key" statement is encoded in the order
   *     specified in the YANG "key" statement, with a comma separating
   *     them.
   *
   *   - The key value is specified as a string, using the canonical
   *     representation for the YANG data type.  Any reserved characters
   *     MUST be percent-encoded, according to [RFC3986], section 2.1.
   *
   *   - All the components in the "key" statement MUST be encoded.
   *     Partial instance identifiers are not supported.
   *
   *   - Quoted strings are not allowed in the key leaf values.  A missing
   *     key value is interpreted a zero-length string.  (example:
   *     list=foo,,baz).
   *
   * Rift:
   *   The RESTCONF clauses overrides RFC 3986.  Specifically, in a key
   *   value, the comma will be percent-encoded, even though it is
   *   allowed per RFC 3986.
   */
  std::string uri;
  uri.reserve(string.length());

  for (const char c: string) {
    if (   // ALPHA
           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           // DIGIT
        || (c >= '0' && c <= '9')
           // rest of unreserved
        || c == '-' || c == '.' || c == '_' || c == '~'
           // sub-delims, BUT NOT COMMA
        || c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' || c == ')'
        || c == '*' || c == '+' || c == ';' || c == '='
           // rest of pchar
        || c == ':' || c == '@' ) {
      uri.push_back(c);
    } else {
      char pct_encoded[8];
      auto bytes = snprintf(pct_encoded, sizeof(pct_encoded), "%%%2X", (unsigned int)c);
      RW_ASSERT(3 == bytes);
      uri.append(pct_encoded);
    }
  }
  return uri;
}

std::string YangModel::utf8_to_escaped_libncx_xpath_string(
  const std::string& string )
{
  /*
   * Non-normative!
   * XPath 2.0+:
   *   Quotes are doubled inside strings to be escaped; the rest of the
   *   chars are left alone.
   * RFC 6020 (v1.0) is silent on escaping.
   * libncx uses backslash escaping. So, that's what we'll do.
   *
   * ATTN: THIS IS TERRIBLE.  libncx is hardly the authoritative
   * answer - the parser seems incompatible with the XML standards.
   * But this will work, anyway.
   */
  std::string xpath_string;
  xpath_string.reserve(string.length());

  for (const char c: string) {
    if (c == '\\') {
      xpath_string.append("\\\\");
    } else {
      xpath_string.push_back(c);
    }
  }
  return xpath_string;
}

std::string YangModel::utf8_to_escaped_xml_data(
  const std::string& string )
{
  /*
   * XML1.1, 2.4:
   *   The ampersand character (&) and the left angle bracket (<) MUST
   *   NOT appear in their literal form, except when used as markup
   *   delimiters, or within a comment, a processing instruction, or a
   *   CDATA section.  If they are needed elsewhere, they MUST be
   *   escaped using either numeric character references or the strings
   *   "&amp;" and "&lt;" respectively.  The right angle bracket (>)
   *   may be represented using the string "&gt;", and MUST, for
   *   compatibility, be escaped using either "&gt;" or a character
   *   reference when it appears in the string "]]>" in content, when
   *   that string is not marking the end of a CDATA section.
   *
   *   In the content of elements, character data is any string of
   *   characters which does not contain the start-delimiter of any
   *   markup or the CDATA-section-close delimiter, "]]>".  In a CDATA
   *   section, character data is any string of characters not
   *   including the CDATA-section-close delimiter.
   *
   *   To allow attribute values to contain both single and double
   *   quotes, the apostrophe or single-quote character (') may be
   *   represented as "&apos;", and the double-quote character (") as
   *   "&quot;".
   *
   *   [14] CharData ::= [^<&]* - ([^<&]* ']]>' [^<&]*)
   *
   * XML1.1, 4.1:
   *   [66] CharRef ::= '&#' [0-9]+ ';'
   *                   | '&#x' [0-9a-fA-F]+ ';'
   *
   *   Characters referred to using character references MUST match the
   *   production for Char.
   *
   *   If the character reference begins with "&#x", the digits and
   *   letters up to the terminating ; provide a hexadecimal
   *   representation of the character's code point in ISO/IEC 10646.
   *   If it begins just with "&#", the digits up to the terminating ;
   *   provide a decimal representation of the character's code point.
   *
   * Rift:
   *   For the purposes of this function, it is assumed that the quotes
   *   do not need escaping.  That will not be true, of course, if the
   *   string ends up as an attribute value, but that is the caller's
   *   problem.  This function will escape control chars with the
   *   &#xXX; syntax, and also '<', '>', and '&' with the entity
   *   syntax.
   */
  std::string xml_data;
  xml_data.reserve(string.length());

  for (const char c: string) {
    if (c == '&') {
      xml_data.append("&amp;");
    } else if (c == '<') {
      xml_data.append("&lt;");
    } else if (c == '>') {
      xml_data.append("&gt;");
    } else if (c >= '\x01' && c <= '\x1F') {
      char amp_encoded[8];
      auto bytes = snprintf(amp_encoded, sizeof(amp_encoded), "&#x%02X;", (unsigned int)c);
      RW_ASSERT(6 == bytes);
      xml_data.append(amp_encoded);
    } else {
      xml_data.push_back(c);
    }
  }
  return xml_data;
}

std::string YangModel::adjust_indent_yang(
  unsigned indent,
  const std::string& data )
{
  if (0 == data.length()) {
    return "";
  }

  /*
   * Determine the padding to subtract from the data, by inferring the
   * padding it already has.  Because of the way libncx saves text
   * blocks, this code searches the entire text for the minimum
   * padding, excluding the beginning of the string.
   */
  size_t min_pad = std::string::npos;
  auto nl = data.find('\n');
  while (std::string::npos != nl) {
    auto nonspace = data.find_first_not_of(' ', nl+1);
    if (nonspace != std::string::npos) {
      auto pad = nonspace - nl - 1;
      if (pad && pad < min_pad) {
        min_pad = pad;
      }
    }
    nl = data.find('\n', nl+1);
  }

  if (std::string::npos == min_pad) {
    min_pad = 0;
  }

  return adjust_indent( indent, min_pad, data );
}

std::string YangModel::adjust_indent_normal(
  unsigned indent,
  const std::string& data )
{
  if (0 == data.length()) {
    return "";
  }

  size_t sub_pad = 0;
  if (' ' == data.front()) {
    sub_pad = data.find_first_not_of(' ');
  }

  return adjust_indent( indent, sub_pad, data );
}

std::string YangModel::adjust_indent(
  unsigned indent,
  size_t subtract_pad,
  const std::string& data )
{
  std::string output;
  size_t sol = 0;
  while (sol < data.length()) {
    size_t eol = data.find('\n', sol);
    size_t nsp = data.find_first_not_of(' ', sol);

    /*
     * Cases:
     *   "":     Impossible by loop condition
     *   " "     eol=NPOS  nsp=NPOS   eol==nsp  emit blank line, no padding
     *   " \n"   eol=1     nsp=1      eol==nsp  emit blank line, no padding
     *   "\n"    eol=0     nsp=0      eol==nsp  emit blank line, no padding
     *   " x"    eol=NPOS  nsp=1      eol>nsp   subtract pad of 1
     *   " x\n"  eol=2     nsp=1      eol>nsp   subtract pad of 1
     *   "x"     eol=NPOS  nsp=0      eol>nsp   no pad to subtract
     *   "x\n"   eol=1     nsp=0      eol>nsp   no pad to subtract
     */
    if (nsp == eol) {
      // just spaces (or nothing) on this line...
    } else {
      RW_ASSERT(nsp < eol);
      auto pad = nsp - sol;
      if (pad < subtract_pad) {
        // Whoops, this line has less pad than the subtract.
        sol = nsp;
      } else {
        sol += subtract_pad;
      }
      auto len = eol - sol;
      if (std::string::npos == eol) {
        len = data.length() - sol;
      }
      output.append(indent,' ');
      output.append(data, sol, len);
    }

    output.push_back('\n');
    if (std::string::npos == eol) {
      break;
    }
    sol = eol + 1;
  }

  while (output.length() && '\n' == output.back()) {
    output.pop_back();
  }

  return output;
}

YangModule* YangModel::load_module_ypbc(
  const rw_yang_pb_module_t* ypbc_mod)
{
  // ATTN: This can be reimplemented by Yang NCX more efficiently, by
  //  hashing the ypbc_mod.
  RW_ASSERT(ypbc_mod);
  RW_ASSERT(ypbc_mod->module_name);

  YangModule* ymod = search_module_name_rev(ypbc_mod->module_name, ypbc_mod->revision);
  if (ymod && ymod->was_loaded_explicitly()) {
    // Need to load the module if it hasn't been loaded "explicitly" yet.
    return ymod;
  }

  // ATTN: Need to support revision.
  return load_module(ypbc_mod->module_name);
}

rw_status_t YangModel::load_schema_ypbc(const rw_yang_pb_schema_t* schema)
{
  NamespaceManager& nsmgr = NamespaceManager::get_global();

  RW_ASSERT(schema->modules);
  rw_status_t retval = RW_STATUS_SUCCESS;

  for (size_t i = 0; i < schema->module_count; ++i) {
    const char* m_ns = schema->modules[i]->ns;
    if (!nsmgr.ns_is_dynamic(m_ns)) { // ignore dynamic modules!!
      YangModule* mod = load_module_ypbc(schema->modules[i]);
      if (!mod) {
        retval = RW_STATUS_FAILURE;
      }
    }
  }

  return retval;
}

YangModule* YangModel::search_module_ns(
  const char* ns)
{
  RW_ASSERT(ns);
  for (YangModuleIter ymi = module_begin(); ymi != module_end(); ++ymi) {
    const char* mns = ymi->get_ns();
    RW_ASSERT(mns);
    if (0 == strcmp(ns, mns)) {
      return &*ymi;
    }
  }
  return NULL;
}

YangNode* YangModel::search_top_yang_node(
  const uint32_t system_ns_id,
  uint32_t element_tag)
{
  for (YangModuleIter ymi = module_begin(); ymi != module_end(); ++ymi) {
    const rw_yang_pb_module_t* ypbc_module = ymi->get_ypbc_module();
    if (!ypbc_module) {
      continue;
    }

    if (ypbc_module->data_module) {
      for (size_t i = 0; i < ypbc_module->data_module->child_msg_count; i++) {
        if (ypbc_module->data_module->child_msgs[i]->pb_element_tag == element_tag) {
          return ymi->search_node(ypbc_module->data_module->child_msgs[i]->yang_node_name, ymi->get_ns());
        }
      }
    }

    if (ypbc_module->rpci_module) {
      for (size_t i = 0; i < ypbc_module->rpci_module->child_msg_count; i++) {
        if (ypbc_module->rpci_module->child_msgs[i]->pb_element_tag == element_tag) {
          return ymi->search_node(ypbc_module->rpci_module->child_msgs[i]->yang_node_name, ymi->get_ns());
        }
      }
    }

    if (ypbc_module->notif_module) {
      for (size_t i = 0; i < ypbc_module->notif_module->child_msg_count; i++) {
        if (ypbc_module->notif_module->child_msgs[i]->pb_element_tag == element_tag) {
          return ymi->search_node(ypbc_module->notif_module->child_msgs[i]->yang_node_name, ymi->get_ns());
        }
      }
    }
  }
  return NULL;
}

YangModule* YangModel::search_module_name_rev(
  const char* name,
  const char* revision)
{
  RW_ASSERT(name);
  RW_ASSERT(name[0] != '\0');
  /* ATTN: Revision support:  RW_ASSERT(revision == nullptr|| revision[0] != '\0'); */
  for (YangModuleIter ymi = module_begin(); ymi != module_end(); ++ymi) {
    const char* mname = ymi->get_name();
    RW_ASSERT(mname);
    RW_ASSERT(mname[0] != '\0');
    if (0 == strcmp(mname, name)) {
      return &*ymi;
    }
  }
  return NULL;
}

YangModuleIter YangModel::module_begin()
{
  return YangModuleIter(get_first_module());
}

YangModuleIter YangModel::module_end()
{
  return YangModuleIter();
}

YangNode* YangModel::search_node (const uint32_t system_ns_id,
                                  const uint32_t element_id)
{

  NamespaceManager& nm_mgr=NamespaceManager::get_global();
  const char *m_name = nm_mgr.nshash_to_string (system_ns_id);

  if(nullptr == m_name) {
    return nullptr;
  }

  YangModule *ym = search_module_ns (m_name);
  if (nullptr == ym) {
    return nullptr;
  }
  return ym->search_node (element_id);
}

YangNode* YangModule::search_node (const uint32_t element_id)
{
  const rw_yang_pb_module_t* ypbc_module = get_ypbc_module();
  if (nullptr == ypbc_module) {
    return nullptr;
  }

  const char *n_name = 0;

  if (ypbc_module->data_module) {
    for (size_t i = 0; i < ypbc_module->data_module->child_msg_count; i++) {
      if (ypbc_module->data_module->child_msgs[i]->pb_element_tag == element_id) {
        n_name = ypbc_module->data_module->child_msgs[i]->yang_node_name;
        break;
      }
    }
  }

  if (ypbc_module->rpci_module) {
    for (size_t i = 0; i < ypbc_module->rpci_module->child_msg_count; i++) {
      if (ypbc_module->rpci_module->child_msgs[i]->pb_element_tag == element_id) {
        n_name = ypbc_module->rpci_module->child_msgs[i]->yang_node_name;
        break;
      }
    }
  }

  if (ypbc_module->rpco_module) {
    for (size_t i = 0; i < ypbc_module->rpco_module->child_msg_count; i++) {
      if (ypbc_module->rpco_module->child_msgs[i]->pb_element_tag == element_id) {
        n_name = ypbc_module->rpco_module->child_msgs[i]->yang_node_name;
        break;
      }
    }
  }

  if (ypbc_module->notif_module) {
    for (size_t i = 0; i < ypbc_module->notif_module->child_msg_count; i++) {
      if (ypbc_module->notif_module->child_msgs[i]->pb_element_tag == element_id) {
        n_name = ypbc_module->notif_module->child_msgs[i]->yang_node_name;
      }
    }
  }

  if (nullptr == n_name) {
    return nullptr;
  }

  return search_node(n_name, get_ns());
}

rw_status_t YangModel::register_ypbc_schema(
  const rw_yang_pb_schema_t* ypbc_schema)
{
  if (!adt_schema_registered_) {
    // ATTN: Should be in constructor?
    bool ret = false;

    ret = app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_SCHEMA_NODE,
                              &adt_schema_node_);

    ret = app_data_get_token (YANGMODEL_ANNOTATION_KEY, YANGMODEL_ANNOTATION_SCHEMA_MODULE,
                              &adt_schema_module_);

    adt_schema_registered_ = true;
    RW_ASSERT (ret);
  }

  if (ypbc_schema_ == ypbc_schema) {
    return RW_STATUS_SUCCESS;
  }
  //RW_ASSERT(nullptr == ypbc_schema_); // cannot change the schema!
  ypbc_schema_ = ypbc_schema;

  NamespaceManager& nm_mgr=NamespaceManager::get_global();

  // Find the module that this schema corresponds to, and annotate it
  for (size_t i = 0; i < ypbc_schema_->module_count; i++) {
    const rw_yang_pb_module_t* ypbc_module = ypbc_schema_->modules[i];
    RW_ASSERT(nullptr != ypbc_module->ns);
    if (!nm_mgr.ns_is_dynamic(ypbc_module->ns)) { // ignore dynamic modules!!
      YangModule *ym = search_module_ns(ypbc_module->ns);
      if (!ym) {
        return RW_STATUS_FAILURE;
      }
      // populate namespace hash in the namespace mgr for latter lookups
      nm_mgr.get_ns_hash(ym->get_ns()); // ignore the return value
      rw_status_t rs = ym->register_ypbc_module(ypbc_module);
      RW_ASSERT(RW_STATUS_SUCCESS == rs);
    }
  }

  return RW_STATUS_SUCCESS;
}

const rw_yang_pb_schema_t* YangModel::get_ypbc_schema()
{
  return ypbc_schema_;
}

/*
 * Helpfer function to find a message in a list of messages
 */


const rw_yang_pb_msgdesc_t *
rw_yang_pb_schema_get_top_level_message (const rw_yang_pb_schema_t* schema,
                       const char *ns,
                       const char *name)
{
  const rw_yang_pb_module_t *module = nullptr;

  for (size_t i = 0; i < schema->module_count; i++) {
    if (!strcmp (schema->modules[i]->ns, ns)) {
      module = schema->modules[i];
      break;
    }
  }

  if (nullptr == module) {
    return nullptr;
  }
  const rw_yang_pb_msgdesc_t *msg = nullptr;

  if (module->data_module) {
    msg = rw_yang_pb_msg_get_child_message (module->data_module, ns, name);
    if (msg) {
      return msg;
    }
  }

  if (module->rpci_module) {
    msg = rw_yang_pb_msg_get_child_message (module->data_module, ns, name);
    if (msg) {
      return msg;
    }
  }

  if (module->rpco_module) {
    msg = rw_yang_pb_msg_get_child_message (module->rpco_module, ns, name);
    if (msg) {
      return msg;
    }
  }

  if (module->notif_module) {
    msg = rw_yang_pb_msg_get_child_message (module->notif_module, ns, name);
    if (msg) {
      return msg;
    }
  }

  return nullptr;
}

const rw_yang_pb_msgdesc_t *
rw_yang_pb_msg_get_child_message (const rw_yang_pb_msgdesc_t *parent,
                                  const char *ns,
                                  const char *name)
{
  for (size_t i = 0; i < parent->child_msg_count; i++) {

    const rw_yang_pb_msgdesc_t *child = parent->child_msgs[i];
    RW_ASSERT(child);
    RW_ASSERT(child->module);
    RW_ASSERT(child->yang_node_name);
    RW_ASSERT(child->module->ns);

    if (!strcmp (ns, child->module->ns) && !strcmp (name, child->yang_node_name)) {
      return child;
    }
  }

  return nullptr;

}



/*****************************************************************************/
// GI Support

#define _RW_YANGMODEL_GI_BOX_MAGIC_(what) \
  static rw_yang_##what##_t *rw_yang_##what##_ref(rw_yang_##what##_t *boxed); \
  static void rw_yang_##what##_unref(rw_yang_##what##_t *boxed); \
   \
  G_DEFINE_BOXED_TYPE(rw_yang_##what##_t, \
                      rw_yang_##what, \
                      rw_yang_##what##_ref, \
                      rw_yang_##what##_unref); \
   \
  static rw_yang_##what##_t *rw_yang_##what##_ref(rw_yang_##what##_t *boxed) \
  { \
      /*g_atomic_int_inc(&boxed->ref_cnt); */ \
        return boxed; \
  } \
   \
  static void rw_yang_##what##_unref(rw_yang_##what##_t *boxed) \
  {  \
      /*if (!g_atomic_int_dec_and_test(&boxed->ref_cnt)) */\
      { \
        return; \
      } \
  }

#define _RW_YANGMODEL_GI_PTR_MAGIC_(what) \
  G_DEFINE_POINTER_TYPE(rw_yang_##what##_t, \
                        rw_yang_##what)

_RW_YANGMODEL_GI_BOX_MAGIC_(model)
_RW_YANGMODEL_GI_BOX_MAGIC_(module)
_RW_YANGMODEL_GI_BOX_MAGIC_(node)
_RW_YANGMODEL_GI_BOX_MAGIC_(type)
_RW_YANGMODEL_GI_BOX_MAGIC_(key)
_RW_YANGMODEL_GI_BOX_MAGIC_(augment)
_RW_YANGMODEL_GI_BOX_MAGIC_(value)
_RW_YANGMODEL_GI_BOX_MAGIC_(bits)
_RW_YANGMODEL_GI_BOX_MAGIC_(extension)
_RW_YANGMODEL_GI_BOX_MAGIC_(typed_enum)
_RW_YANGMODEL_GI_BOX_MAGIC_(grouping)
_RW_YANGMODEL_GI_BOX_MAGIC_(uses)

_RW_YANGMODEL_GI_PTR_MAGIC_(path_element)

_RW_YANGMODEL_GI_PTR_MAGIC_(pb_group_root)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_module)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_rpcdef)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_msgdesc_union)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_msgdesc)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_enumdesc)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_flddesc)
_RW_YANGMODEL_GI_PTR_MAGIC_(pb_schema)

G_DEFINE_POINTER_TYPE(ProtobufCMessage, protobuf_c_message);

#undef _RW_YANG_ENUM_MAGIC_
#define _RW_YANG_ENUM_MAGIC_(WHAT)  { RW_YANG_##WHAT, #WHAT, #WHAT }

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 */
#undef _RW_YANG_ENUM_MAGIC2_
#define _RW_YANG_ENUM_MAGIC2_(what, WHAT) \
static const GEnumValue _rw_yang_##what##_enum_values[] = { \
  RW_YANG_##WHAT##_VALUES, \
  { 0, NULL, NULL} \
}; \
GType rw_yang_##what##_get_type(void) \
{  \
  static GType type = 0; /* G_TYPE_INVALID */ \
 \
  if (!type) \
    type = g_enum_register_static("rw_yang_"#what"_t", _rw_yang_##what##_enum_values); \
 \
  return type; \
}


_RW_YANG_ENUM_MAGIC2_(stmt_type, STMT_TYPE)
_RW_YANG_ENUM_MAGIC2_(leaf_type, LEAF_TYPE)
_RW_YANG_ENUM_MAGIC2_(log_level, LOG_LEVEL)

#if 0
// The above MACROs create code similar to this commented blocks
static const GEnumValue _rw_yang_stmt_type_enum_values[] = {
  { RW_YANG_STMT_TYPE_NULL, "STMT_TYPE_NULL", "STMT_TYPE_NULL" },
  { RW_YANG_STMT_TYPE_FIRST, "STMT_TYPE_FIRST", "STMT_TYPE_FIRST" },
  { RW_YANG_STMT_TYPE_ROOT, "STMT_TYPE_ROOT", "STMT_TYPE_ROOT" },
  { RW_YANG_STMT_TYPE_NA, "", "" },
  { RW_YANG_STMT_TYPE_ANYXML, "", "" },
  { RW_YANG_STMT_TYPE_CONTAINER, "", "" },
  { RW_YANG_STMT_TYPE_LEAF, "", "" },
  { RW_YANG_STMT_TYPE_LEAF_LIST, "", "" },
  { RW_YANG_STMT_TYPE_LIST, "", "" },
  { RW_YANG_STMT_TYPE_CHOICE, "", "" },
  { RW_YANG_STMT_TYPE_CASE, "", "" },
  { RW_YANG_STMT_TYPE_RPC, "", "" },
  { RW_YANG_STMT_TYPE_RPCIO, "", "" },
  { RW_YANG_STMT_TYPE_NOTIF, "", "" },
  { RW_YANG_STMT_TYPE_GROUPING, "", "" },
  { RW_YANG_STMT_TYPE_LAST, "", "" },
  { RW_YANG_STMT_TYPE_END, "", "" },
  { 0, NULL, NULL}
};
GType rw_yang_stmt_type_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("rw_yang_stmt_type_t", _rw_yang_stmt_type_enum_values);

  return type;
}
#endif


// End yangmodel.cpp
