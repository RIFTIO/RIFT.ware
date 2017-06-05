/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file yangpbc_node.cpp
 *
 * Convert yang schema to google protobuf: yang node support
 */

#include "rwlib.h"
#include "yangpbc.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>

#include <boost/scoped_array.hpp>

#include <fstream>
#include <ios>
#include <iostream>
#include <mutex>
#include <sstream>
#include <algorithm>

namespace rw_yang {

static inline std::string c_escape_string(
  const char* input,
  unsigned len)
{
/*
  ATTN: Belongs somewhere common!  Probably yangmodel
*/
  // Maximumun possible length. All characters could be non-printable plus 1
  // byte for null termination.
  unsigned dest_len = (len * 4) + 1;
  boost::scoped_array<char> dest(new char[dest_len]);
  RW_ASSERT(dest);
  unsigned used = 0;
  for (unsigned i = 0; i < len; i++) {
    switch(input[i]) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n'; break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r'; break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't'; break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default: {
        if (!isprint(input[i])) {
          char oct[10];
          // octal escaping for non printable characters.
          sprintf(oct, "\\%03o", static_cast<uint8_t>(input[i]));
          for (unsigned j = 0; j < 4; j++) {
            dest[used++] = oct[j];
          }
        } else {
          dest[used++] = input[i];
        }
      }
    }
  }

  RW_ASSERT(used <= dest_len);
  dest[used] = '\0';
  return std::string(dest.get());
}

PbNode::map_value_inline_t PbNode::map_value_inline;
PbNode::map_parse_inline_t PbNode::map_parse_inline;
PbNode::map_value_inline_max_t PbNode::map_value_inline_max;
PbNode::map_parse_inline_max_t PbNode::map_parse_inline_max;
PbNode::map_value_string_max_t PbNode::map_value_string_max;
PbNode::map_parse_string_max_t PbNode::map_parse_string_max;
PbNode::map_yang_leaf_type_t PbNode::map_yang_leaf_type;
PbNode::map_value_type_t PbNode::map_value_type;
PbNode::type_allowed_t PbNode::type_allowed;
PbNode::map_string_pbftype_t PbNode::map_string_pbftype;
PbNode::map_string_pbftype_t PbNode::map_default_pbftype;
PbNode::map_string_pbftype_t PbNode::map_string_pbctype;
PbNode::map_flat_value_t PbNode::map_flat_value;
PbNode::map_parse_flat_t PbNode::map_parse_flat;


void PbNode::init_tables()
{
  // protobuf-fld-inline
  map_value_inline["true"] = inline_t::SEL_TRUE;
  map_value_inline["false"] = inline_t::SEL_FALSE;
  map_value_inline["auto"] = inline_t::SEL_AUTO;

  map_parse_inline[inline_t::UNSET] = &PbNode::parse_inline_unset;
  map_parse_inline[inline_t::SEL_AUTO] = &PbNode::parse_inline_auto;
  map_parse_inline[inline_t::SEL_TRUE] = &PbNode::parse_noop;
  map_parse_inline[inline_t::SEL_FALSE] = &PbNode::parse_noop;


  // protobuf-fld-inline-max
  map_value_inline_max["yang"] = inline_max_t::SEL_YANG;
  map_value_inline_max["none"] = inline_max_t::SEL_NONE;

  map_parse_inline_max[inline_max_t::UNSET] = &PbNode::parse_inline_max_unset;
  map_parse_inline_max[inline_max_t::SEL_YANG] = &PbNode::parse_inline_max_yang;
  map_parse_inline_max[inline_max_t::SEL_NONE] = &PbNode::parse_noop;
  map_parse_inline_max[inline_max_t::SEL_VALUE] = &PbNode::parse_noop;


  // protobuf-fld-string-max
  map_value_string_max["yang"] = string_max_t::SEL_YANG;
  map_value_string_max["none"] = string_max_t::SEL_NONE;
  map_value_string_max["octet"] = string_max_t::SEL_OCTET;
  map_value_string_max["utf8"] = string_max_t::SEL_UTF8;

  map_parse_string_max[string_max_t::UNSET] = &PbNode::parse_string_max_unset;
  map_parse_string_max[string_max_t::SEL_YANG] = &PbNode::parse_string_max_yang;
  map_parse_string_max[string_max_t::SEL_NONE] = &PbNode::parse_noop;
  map_parse_string_max[string_max_t::SEL_OCTET] = &PbNode::parse_noop;
  map_parse_string_max[string_max_t::SEL_UTF8] = &PbNode::parse_noop;
  map_parse_string_max[string_max_t::SEL_VALUE] = &PbNode::parse_noop;
  map_parse_string_max[string_max_t::SEL_OCTET_YANG] = &PbNode::parse_string_max_yang;
  map_parse_string_max[string_max_t::SEL_OCTET_VALUE] = &PbNode::parse_string_max_octet_value;
  map_parse_string_max[string_max_t::SEL_UTF8_YANG] = &PbNode::parse_string_max_yang;
  map_parse_string_max[string_max_t::SEL_UTF8_VALUE] = &PbNode::parse_string_max_utf8_value;


  // yang leaf type
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_INT8] = PbFieldType::pbsint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_INT16] = PbFieldType::pbsint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_INT32] = PbFieldType::pbsint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_INT64] = PbFieldType::pbsint64;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_UINT8] = PbFieldType::pbuint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_UINT16] = PbFieldType::pbuint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_UINT32] = PbFieldType::pbuint32;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_UINT64] = PbFieldType::pbuint64;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_DECIMAL64] = PbFieldType::pbdouble;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_EMPTY] = PbFieldType::pbbool;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_BOOLEAN] = PbFieldType::pbbool;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_STRING] = PbFieldType::pbstring;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_IDREF] = PbFieldType::pbstring; // Cannot be UTF-8
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_INSTANCE_ID] = PbFieldType::pbstring;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_BITS] = PbFieldType::hack_bits;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_BINARY] = PbFieldType::pbbytes;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_ANYXML] = PbFieldType::pbstring;
  map_yang_leaf_type[RW_YANG_LEAF_TYPE_UNION] = PbFieldType::hack_union;


  // protobuf-fld-type
  map_value_type["double"] = PbFieldType::pbdouble;
  map_value_type["float"] = PbFieldType::pbfloat;
  map_value_type["int32"] = PbFieldType::pbint32;
  map_value_type["uint32"] = PbFieldType::pbuint32;
  map_value_type["sint32"] = PbFieldType::pbsint32;
  map_value_type["fixed32"] = PbFieldType::pbfixed32;
  map_value_type["sfixed32"] = PbFieldType::pbsfixed32;
  map_value_type["int64"] = PbFieldType::pbint64;
  map_value_type["uint64"] = PbFieldType::pbuint64;
  map_value_type["sint64"] = PbFieldType::pbsint64;
  map_value_type["fixed64"] = PbFieldType::pbfixed64;
  map_value_type["sfixed64"] = PbFieldType::pbsfixed64;
  map_value_type["bool"] = PbFieldType::pbbool;
  map_value_type["bytes"] = PbFieldType::pbbytes;
  map_value_type["string"] = PbFieldType::pbstring;
  map_value_type["auto"] = PbFieldType::automatic;

  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT8, PbFieldType::pbint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT8, PbFieldType::pbsint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT8, PbFieldType::pbsfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT16, PbFieldType::pbint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT16, PbFieldType::pbsint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT16, PbFieldType::pbsfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT32, PbFieldType::pbint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT32, PbFieldType::pbsint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT32, PbFieldType::pbsfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT64, PbFieldType::pbint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT64, PbFieldType::pbsint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INT64, PbFieldType::pbsfixed64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT8, PbFieldType::pbuint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT8, PbFieldType::pbfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT16, PbFieldType::pbuint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT16, PbFieldType::pbfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT32, PbFieldType::pbuint32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT32, PbFieldType::pbfixed32) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT64, PbFieldType::pbuint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UINT64, PbFieldType::pbfixed64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbuint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbsint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbint64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbfixed64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbsfixed64) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbdouble) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_DECIMAL64, PbFieldType::pbfloat) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_EMPTY, PbFieldType::pbbool) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_BOOLEAN, PbFieldType::pbbool) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_ENUM, PbFieldType::pbenum) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_STRING, PbFieldType::pbstring) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_IDREF, PbFieldType::pbstring) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_INSTANCE_ID, PbFieldType::pbstring) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_ANYXML, PbFieldType::pbstring) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_BITS, PbFieldType::hack_bits) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_BITS, PbFieldType::pbbytes) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_BINARY, PbFieldType::pbbytes) );
  type_allowed.insert( std::make_pair(RW_YANG_LEAF_TYPE_UNION, PbFieldType::hack_union) );


  // output
  map_string_pbftype[PbFieldType::pbdouble] = "double";
  map_string_pbftype[PbFieldType::pbfloat] = "float";
  map_string_pbftype[PbFieldType::pbint32] = "int32";
  map_string_pbftype[PbFieldType::pbuint32] = "uint32";
  map_string_pbftype[PbFieldType::pbsint32] = "sint32";
  map_string_pbftype[PbFieldType::pbfixed32] = "fixed32";
  map_string_pbftype[PbFieldType::pbsfixed32] = "sfixed32";
  map_string_pbftype[PbFieldType::pbint64] = "int64";
  map_string_pbftype[PbFieldType::pbuint64] = "uint64";
  map_string_pbftype[PbFieldType::pbsint64] = "sint64";
  map_string_pbftype[PbFieldType::pbfixed64] = "fixed64";
  map_string_pbftype[PbFieldType::pbsfixed64] = "sfixed64";
  map_string_pbftype[PbFieldType::pbbool] = "bool";
  map_string_pbftype[PbFieldType::pbbytes] = "bytes";
  map_string_pbftype[PbFieldType::pbstring] = "string";
  map_string_pbftype[PbFieldType::hack_bits] = "string";
  map_string_pbftype[PbFieldType::hack_union] = "string";

  // output
  map_string_pbctype[PbFieldType::pbdouble] = "double";
  map_string_pbctype[PbFieldType::pbfloat] = "float";
  map_string_pbctype[PbFieldType::pbint32] = "int32_t";
  map_string_pbctype[PbFieldType::pbuint32] = "uint32_t";
  map_string_pbctype[PbFieldType::pbsint32] = "int32_t";
  map_string_pbctype[PbFieldType::pbfixed32] = "uint32_t";
  map_string_pbctype[PbFieldType::pbsfixed32] = "int32_t";
  map_string_pbctype[PbFieldType::pbint64] = "int64_t";
  map_string_pbctype[PbFieldType::pbuint64] = "uint64_t";
  map_string_pbctype[PbFieldType::pbsint64] = "int64_t";
  map_string_pbctype[PbFieldType::pbfixed64] = "uint64_t";
  map_string_pbctype[PbFieldType::pbsfixed64] = "int64_t";
  map_string_pbctype[PbFieldType::pbbool] = "int";
  map_string_pbctype[PbFieldType::pbstring] = "char*";
  map_string_pbctype[PbFieldType::hack_bits] = "char*";
  map_string_pbctype[PbFieldType::hack_union] = "char*";

  // output
  map_default_pbftype[PbFieldType::pbdouble] = "0.0";
  map_default_pbftype[PbFieldType::pbfloat] = "0.0";
  map_default_pbftype[PbFieldType::pbint32] = "0";
  map_default_pbftype[PbFieldType::pbuint32] = "0";
  map_default_pbftype[PbFieldType::pbsint32] = "0";
  map_default_pbftype[PbFieldType::pbfixed32] = "0";
  map_default_pbftype[PbFieldType::pbsfixed32] = "0";
  map_default_pbftype[PbFieldType::pbint64] = "0";
  map_default_pbftype[PbFieldType::pbuint64] = "0";
  map_default_pbftype[PbFieldType::pbsint64] = "0";
  map_default_pbftype[PbFieldType::pbfixed64] = "0";
  map_default_pbftype[PbFieldType::pbsfixed64] = "0";
  map_default_pbftype[PbFieldType::pbbool] = "false";
  map_default_pbftype[PbFieldType::pbbytes] = "\"\"";
  map_default_pbftype[PbFieldType::pbstring] = "\"\"";
  map_default_pbftype[PbFieldType::hack_bits] = "\"\"";
  map_default_pbftype[PbFieldType::hack_union] = "\"\"";


  // protobuf-msg-flat
  map_flat_value["true"] = flat_t::SEL_TRUE;
  map_flat_value["false"] = flat_t::SEL_FALSE;
  map_flat_value["auto"] = flat_t::SEL_AUTO;

  map_parse_flat[flat_t::UNSET] = &PbNode::parse_flat_unset;
  map_parse_flat[flat_t::SEL_AUTO] = &PbNode::parse_flat_auto;
  map_parse_flat[flat_t::SEL_TRUE] = &PbNode::parse_noop;
  map_parse_flat[flat_t::SEL_FALSE] = &PbNode::parse_noop;
}

PbNode::PbNode(
  PbModule* pbmod,
  YangNode* ynode,
  PbNode* parent_pbnode)
: pbmod_(pbmod),
  lci_pbmod_(pbmod),
  parent_pbnode_(parent_pbnode),
  ynode_(ynode)
{
  RW_ASSERT(ynode_);
  RW_ASSERT(pbmod_);
  pbmodel_ = pbmod_->get_pbmodel();

  switch (ynode_->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_NULL:
    case RW_YANG_STMT_TYPE_NA:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE: {
      std::ostringstream oss;
      oss << "Unexpected yang statement type "
          << rw_yang_stmt_type_string(ynode_->get_stmt_type());
      pbmodel_->error(ynode_->get_location(), oss.str());
      break;
    }
    case RW_YANG_STMT_TYPE_LIST:
      has_list_descendants_ = true;
      break;
    case RW_YANG_STMT_TYPE_ROOT:
    case RW_YANG_STMT_TYPE_ANYXML:
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
      break;
    default:
      RW_ASSERT_NOT_REACHED(); // invalid statement type
  }

  if (parent_pbnode_) {
    schema_defining_pbmod_ = parent_pbnode_->schema_defining_pbmod_;
  } else {
    schema_defining_pbmod_ = pbmod_;
  }

  reference_pbmods_.emplace(pbmod_);

  if (ynode_ != pbmodel_->get_ymodel()->get_root_node()) {
    YangModule* other_ymod = ynode_->get_module();
    RW_ASSERT(other_ymod);
    PbModule* other_pbmod = pbmodel_->module_lookup(other_ymod);
    RW_ASSERT(other_pbmod);
    if (other_pbmod != pbmod_) {
      add_augment_mod(other_pbmod);
    }

    YangAugment* yaug = ynode_->get_augment();
    if (yaug) {
      YangModule* aug_ymod = ynode_->get_module();
      RW_ASSERT(aug_ymod);
      PbModule* augment_pbmod = pbmodel_->module_lookup(aug_ymod);
      if (augment_pbmod->imports_transitive(schema_defining_pbmod_)) {
        schema_defining_pbmod_ = augment_pbmod;
      }
    }
    add_augment_mod(schema_defining_pbmod_);
  }
}

PbModel* PbNode::get_pbmodel() const
{
  return pbmod_->get_pbmodel();
}

PbModule* PbNode::get_pbmod() const
{
  return pbmod_;
}

PbModule* PbNode::get_schema_defining_pbmod() const
{
  return schema_defining_pbmod_;
}

PbModule* PbNode::get_lci_pbmod() const
{
  return lci_pbmod_;
}

PbNode* PbNode::get_parent_pbnode() const
{
  return parent_pbnode_;
}

PbEnumType* PbNode::get_pbenum() const
{
  return pbenum_;
}

YangNode* PbNode::get_ynode() const
{
  return ynode_;
}

YangValue* PbNode::get_yvalue() const
{
  return ynode_->get_first_value();
}

YangType* PbNode::get_effective_ytype() const
{
  YangNode* ynode = ynode_;
  RW_ASSERT(ynode);
  YangType* ytype = ynode->get_type();
  if (!ytype) {
    return ytype;
  }

  rw_yang_leaf_type_t leaf_type = ytype->get_leaf_type();

  /*
   * Leafref is an extra special case, because the effective type is
   * indirect - need to walk the chain of leafrefs until the ultimate
   * target type is found.
   */
  while (RW_YANG_LEAF_TYPE_LEAFREF == leaf_type) {
    ynode = ynode->get_leafref_ref();
    if (!ynode) {
      /*
       * ATTN: leafref in grouping fails ref lookup, because ref
       * resolution requires grouping instantiation by a uses into
       * data/rpc/notif.  Just call it string for now.  Hack.  Will
       * have to be fixed for better grouping support.
       */
      return ytype;
    }

    ytype = ynode->get_type();
    RW_ASSERT(ytype);
    leaf_type = ytype->get_leaf_type();
  }
  return ytype;
}

YangExtension* PbNode::get_yext_msg_new() const
{
  return yext_msg_new_;
}

YangExtension* PbNode::get_yext_msg_tag_base() const
{
  return yext_msg_tag_base_;
}

YangExtension* PbNode::get_yext_field_tag() const
{
  return yext_field_tag_;
}

YangExtension* PbNode::get_yext_field_c_type() const
{
  return yext_field_c_type_;
}

YangExtension* PbNode::get_yext_notif_log_common() const
{
  return yext_notif_log_common_;
}

YangExtension* PbNode::get_yext_notif_log_event_id() const
{
  return yext_notif_log_event_id_;
}

YangExtension* PbNode::get_yext_field_merge_behavior() const
{
    return yext_field_merge_behavior_;
}

rw_yang_stmt_type_t PbNode::get_yang_stmt_type() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_stmt_type();
}

rw_yang_leaf_type_t PbNode::get_effective_yang_leaf_type() const
{
  YangType* ytype = get_effective_ytype();
  if (!ytype) {
    return RW_YANG_LEAF_TYPE_NULL;
  }

  /*
   * Consider unresolved LEAFREF to be STRING.  This can happen for
   * leafrefs in groupings, because the target is unknown until the
   * uses statment.  ATTN: grouping support needed.
   */
  rw_yang_leaf_type_t leaf_type = ytype->get_leaf_type();
  if (RW_YANG_LEAF_TYPE_LEAFREF == leaf_type) {
    return RW_YANG_LEAF_TYPE_STRING;
  }

  return leaf_type;
}

std::string PbNode::get_schema_element_type() const
{
  std::string schema_element_type("RW_SCHEMA_ELEMENT_TYPE_");
  switch(ynode_->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_LIST: {
      schema_element_type.append("LISTY_");
      unsigned num_keys = get_num_keys();
      schema_element_type.append(std::to_string(num_keys));

      if (num_keys == 1) {
        YangNode* yknode = ynode_->key_begin()->get_key_node();
        RW_ASSERT(yknode);
        PbNode* key_pbnode = pbmodel_->lookup_node(yknode);
        RW_ASSERT(key_pbnode);
        RW_ASSERT(key_pbnode->is_key())

        switch (key_pbnode->get_pb_field_type()) {
          case PbFieldType::pbdouble:
            schema_element_type.append("_DOUBLE");
            break;
          case PbFieldType::pbfloat:
            schema_element_type.append("_FLOAT");
            break;
          case PbFieldType::pbint32:
            schema_element_type.append("_INT32");
            break;
          case PbFieldType::pbuint32:
            schema_element_type.append("_UINT32");
            break;
          case PbFieldType::pbsint32:
            schema_element_type.append("_SINT32");
            break;
          case PbFieldType::pbfixed32:
            schema_element_type.append("_FIXED32");
            break;
          case PbFieldType::pbsfixed32:
            schema_element_type.append("_SFIXED32");
            break;
          case PbFieldType::pbint64:
            schema_element_type.append("_INT64");
            break;
          case PbFieldType::pbuint64:
            schema_element_type.append("_UINT64");
            break;
          case PbFieldType::pbsint64:
            schema_element_type.append("_SINT64");
            break;
          case PbFieldType::pbfixed64:
            schema_element_type.append("_FIXED64");
            break;
          case PbFieldType::pbsfixed64:
            schema_element_type.append("_SFIXED64");
            break;
          case PbFieldType::pbbool:
            schema_element_type.append("_BOOL");
            break;
          case PbFieldType::pbbytes:
            if (key_pbnode->string_max_limit_ && key_pbnode->string_max_limit_ <= RWPB_MINIKEY_MAX_BKEY_LEN) {
              schema_element_type.append("_BYTES");
            }
            break;
          case PbFieldType::pbstring:
            if (key_pbnode->string_max_limit_ && key_pbnode->string_max_limit_ <= RWPB_MINIKEY_MAX_SKEY_LEN) {
              schema_element_type.append("_STRING");
            }
            break;
          case PbFieldType::hack_bits:
          case PbFieldType::hack_union:
          case PbFieldType::pbenum:
          case PbFieldType::pbmessage:
            break;

          case PbFieldType::illegal_:
          case PbFieldType::unset:
          case PbFieldType::automatic:
            RW_ASSERT_NOT_REACHED();
        }
      }
      break;
    }
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      schema_element_type.append("LEAF_LIST");
      break;
    case RW_YANG_STMT_TYPE_ROOT:
      schema_element_type.append("MODULE_ROOT");
      break;
    case RW_YANG_STMT_TYPE_CONTAINER:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_RPCIO:
    case RW_YANG_STMT_TYPE_NOTIF:
      schema_element_type.append("CONTAINER");
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  return schema_element_type;
}

std::string PbNode::get_yang_description() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_description();
}

std::string PbNode::get_yang_fieldname() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_name();
}

std::string PbNode::get_proto_fieldname() const
{
  if (yext_field_name_) {
    return yext_field_name_->get_value();
  }
  RW_ASSERT(ynode_);
  switch (ynode_->get_stmt_type()) {
    case RW_YANG_STMT_TYPE_ROOT:
      return pbmod_->get_proto_fieldname();
    case RW_YANG_STMT_TYPE_RPCIO:
      RW_ASSERT(parent_pbnode_);
      return parent_pbnode_->get_proto_fieldname();
    default:
      break;
  }
  return PbModel::mangle_identifier(ynode_->get_name());
}

std::string PbNode::get_pbc_fieldname() const
{
  return PbModel::mangle_to_lowercase(get_proto_fieldname());
}

std::string PbNode::get_xml_element_name() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_xml_element_name();
}

std::string PbNode::get_xpath_element_name() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_xpath_element_name();
}

std::string PbNode::get_xpath_element_with_predicate() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_rw_xpath_element_with_sample_predicate();
}

std::string PbNode::get_rwrest_path_element() const
{
  RW_ASSERT(ynode_);
  return ynode_->get_rwrest_path_element();
}

std::string PbNode::get_proto_msg_new_typename() const
{
  RW_ASSERT(is_message() ||
            is_leaf_list()); // Any other way?
  if (yext_msg_new_) {
    return yext_msg_new_->get_value();
  }
  return "";
}

std::string PbNode::get_proto_msg_typename() const
{
  RW_ASSERT(is_message() ||
            is_leaf_list()); // Any other way?
  if (yext_msg_name_) {
    return yext_msg_name_->get_value();
  }
  return PbModel::mangle_typename(get_proto_fieldname());
}

std::string PbNode::get_proto_simple_typename() const
{
  if (is_message()) {
    return "";
  }
  if (pbenum_) {
    return pbenum_->get_proto_type_path();
  }
  auto msi = map_string_pbftype.find(pb_field_type_);
  RW_ASSERT(msi != map_string_pbftype.end());
  return msi->second;
}

std::string PbNode::get_pbc_simple_typename() const
{
  if (is_message()) {
    return "";
  }
  if (pbenum_) {
    return pbenum_->get_pbc_typename();
  }
  if (yext_field_c_type_) {
    return yext_field_c_type_->get_value();
  }
  if (PbFieldType::pbbytes == pb_field_type_) {
    if (is_field_inline()) {
      return "ProtobufCFlatBinaryData";
    }
    return "ProtobufCBinaryData";
  }
  auto msi = map_string_pbctype.find(pb_field_type_);
  RW_ASSERT(msi != map_string_pbctype.end());
  return msi->second;
}

std::string PbNode::get_gi_simple_typename() const
{
  if (pbenum_) {
    return "const char*";
  } else if (PbFieldType::pbbytes == pb_field_type_) {
    return "guint8*";
  }
  return get_pbc_simple_typename();
}

bool PbNode::is_message() const
{
  return pb_field_type_ == PbFieldType::pbmessage;
}

bool PbNode::is_field_inline() const
{
  if (inline_t::SEL_TRUE == inline_) {
    return true;
  }
  RW_ASSERT (inline_t::SEL_FALSE == inline_);
  return false;
}

bool PbNode::is_stringy() const
{
  return    pb_field_type_ == PbFieldType::hack_bits
         || pb_field_type_ == PbFieldType::hack_union
         || pb_field_type_ == PbFieldType::pbstring
         || pb_field_type_ == PbFieldType::pbbytes;
}

bool PbNode::is_listy() const
{
  return ynode_->is_listy();
}

bool PbNode::is_leaf_list() const
{
  return (ynode_->is_listy() && ynode_->is_leafy());
}

bool PbNode::is_optional() const
{
  return optional_;
}

bool PbNode::is_key() const
{
  return ynode_->is_key();
}

bool PbNode::is_flat() const
{
  if (flat_t::SEL_TRUE == flat_) {
    return true;
  }
  RW_ASSERT(flat_t::SEL_FALSE == flat_);
  return false;
}

bool PbNode::has_list_descendants() const
{
  return has_list_descendants_;
}

bool PbNode::has_op_data() const
{
  return has_op_data_;
}

bool PbNode::has_keys() const
{
  return    ynode_->get_stmt_type() == RW_YANG_STMT_TYPE_LIST
         && ynode_->key_begin() != ynode_->key_end();
}

unsigned PbNode::get_num_keys() const
{
  if (!has_keys()) {
    return 0;
  }

  unsigned num_keys = 0;
  for (YangKeyIter yki = ynode_->key_begin(); yki != ynode_->key_end(); ++yki) {
    ++num_keys;
  }
  return num_keys;
}

uint32_t PbNode::get_max_elements() const
{
  if (max_elements_) {
    return max_elements_;
  }
  return 1;
}

uint32_t PbNode::get_inline_max() const
{
  if (inline_max_t::SEL_VALUE == inline_max_) {
    RW_ASSERT(inline_max_limit_);
    return inline_max_limit_;
  }
  RW_ASSERT(inline_max_ == inline_max_t::UNSET || inline_max_ == inline_max_t::SEL_NONE);
  return 0;
}

uint64_t PbNode::get_string_max() const
{
  if (string_max_t::SEL_VALUE == string_max_) {
    RW_ASSERT(string_max_limit_);
    if (PbFieldType::pbbytes == get_pb_field_type()) {
      return string_max_limit_;
    }
    // plus 1 for NUL.
    return string_max_limit_ + 1;
  }
  RW_ASSERT(string_max_ == string_max_t::UNSET || string_max_ == string_max_t::SEL_NONE);
  return 0;
}

uint32_t PbNode::get_event_id() const
{
  return notif_log_event_id_;
}

bool PbNode::has_default() const
{
  const char* default_value = ynode_->get_default_value();
  return nullptr != default_value && '\0' != default_value[0];
}

std::string PbNode::get_proto_default() const
{
  const char* def_val = ynode_->get_default_value();
  if (def_val) {
    switch (get_pb_field_type()) {
      case PbFieldType::pbdouble:
      case PbFieldType::pbfloat:
      case PbFieldType::pbint32:
      case PbFieldType::pbuint32:
      case PbFieldType::pbsint32:
      case PbFieldType::pbfixed32:
      case PbFieldType::pbsfixed32:
      case PbFieldType::pbint64:
      case PbFieldType::pbuint64:
      case PbFieldType::pbsint64:
      case PbFieldType::pbfixed64:
      case PbFieldType::pbsfixed64:
      case PbFieldType::pbbool:
        // Already in an acceptible format
        return def_val;

      case PbFieldType::pbstring:
        return std::string("\"") + c_escape_string(def_val, strlen(def_val)) + "\"";

      case PbFieldType::pbbytes: {
        /*
         * The default value is in base64.  convert to raw string,
         * escape it and output in the proto.
         */
        auto bin_len = rw_yang_base64_get_decoded_len(def_val, strlen(def_val));
        boost::scoped_array<char> binary(new char[bin_len+1]);
        rw_status_t status = rw_yang_base64_decode(def_val, strlen(def_val), binary.get(), bin_len+1);
        RW_ASSERT(status == RW_STATUS_SUCCESS);
        // Convert the binary data to a printable string by escaping it.
        return std::string("\"") + c_escape_string(binary.get(), bin_len) + "\"";
      }
      case PbFieldType::pbenum: {
        RW_ASSERT(pbenum_);
        PbEnumValue* def_pbval = pbenum_->get_value(def_val);
        if (!def_pbval) {
          break;
        }
        return def_pbval->get_proto_enumerator();
      }
      case PbFieldType::hack_bits:
      case PbFieldType::hack_union:
        return std::string("\"") + c_escape_string(def_val, strlen(def_val)) + "\"";

      case PbFieldType::pbmessage:
        RW_ASSERT_NOT_REACHED(); // ATTN: not sure what to do here

      default:
        RW_ASSERT_NOT_REACHED(); // these are bugs
    }
  }

  auto mdi = map_default_pbftype.find(get_pb_field_type());
  if (mdi == map_default_pbftype.end()) {
    /*
     * ATTN: Not sure what to do here.  There is a possibly sensible
     * answer for enum: the first enumerator
     * Messages have no reasonable default.  Return nothing.
     */
    return "";
  }
  return mdi->second;
}

std::string PbNode::get_c_default_init() const
{
  /*
   * In general, the proto default can be used for the C default, due
   * to language compatibility.  However, a few tricky cases exist.
   */
  switch (get_pb_field_type()) {
    case PbFieldType::pbenum: {
      RW_ASSERT(pbenum_);
      PbEnumValue* def_pbval = pbenum_->get_value(ynode_->get_default_value());
      if (!def_pbval) {
        break;
      }
      return def_pbval->get_pbc_enumerator();
    }
    case PbFieldType::pbbytes:
    case PbFieldType::pbmessage:
      /*
         ATTN: Need to come up with better answers here
       */
      return "";
    default:
      break;
  }

  if (yext_field_c_type_) {
      /* ATTN: Cannot handle actual default values - can only initialize to common static value */
    if (is_field_inline()) {
      return std::string(yext_field_c_type_->get_value()) + "_STATIC_INIT";
    }
    return "nullptr";
  }

  std::string def_val = get_proto_default();
  if (def_val.length() && *def_val.begin() == '"') {
    if (!is_field_inline()) {
      return std::string("(char*)") + def_val;
    }
  }
  return def_val;
}

uint32_t PbNode::get_pbc_size_warning_limit() const
{
  if (pbc_size_max_set_) {
    return pbc_size_limit_;
  }
  return PBC_SIZE_MAX_WARN;
}

uint32_t PbNode::get_pbc_size_error_limit() const
{
  return pbc_size_limit_;
}

PbFieldType PbNode::get_pb_field_type() const
{
  return pb_field_type_;
}

void PbNode::child_add(PbNode* child_pbnode)
{
  RW_ASSERT(child_pbnode);
  child_pbnodes_.emplace_back(child_pbnode);
}

PbNode::node_citer_t PbNode::child_begin() const
{
  return child_pbnodes_.cbegin();
}

PbNode::node_citer_t PbNode::child_end() const
{
  return child_pbnodes_.cend();
}

PbNode::node_citer_t PbNode::child_last() const
{
  auto i = child_pbnodes_.cend();
  if (i == child_pbnodes_.cbegin()) {
    return i;
  }
  return --i;
}

#if 0
uint32_t PbNode::get_tag_number()
{
  RW_ASSERT(ynode_);
  RW_ASSERT(pbmod_);

  /*
   * ATTN: Find a better location for this documentation.
   *
   * ATTN: This is a temporary mapping.  The long-term mapping will
   * remove the namespace from the mapping, opting to separate fields
   * from different namespaces by putting them in sub-messages tagged
   * by namespace ID.
   *
   * Current (temporary) tag numbering scheme:
   *                 2         11  1  11         0
   *                 8         87  4  10         0
   *   Reserved    : 000000000000001RRRRRRRRRRRRRR
   *   NS Root Node: 000000000001000000NNNNNNNNNNN
   *   Others      : NNNNNNNNNNNTTTTTTTTTTTTTTTTTT
   *
   *   1RRRRRRRRRRRRRR    - Reserved by proto implementation (actual reservation
   *                        is smaller, this is just to keep it simple).
   *   NNNNNNNNNNN        - Namespace ID, 11 bits, 1<N<2048.
   *   TTTTTTTTTTTTTTTTTT - Per-module statement number, 18 bits, 1<T<262144.
   */
  if (RW_YANG_STMT_TYPE_ROOT == ynode_->get_stmt_type()) {
    return (1<<17) + pbmod_->get_nsid();
  }

  uint32_t node_tag = ynode_->get_node_tag();
  uint32_t ns_tag = pbmod_->get_nsid();

  // ATTN: There needs to be a description of these constants somewhere
  RW_ASSERT(node_tag < (1 << 18));  // Does this fit in 18 bits
  RW_ASSERT(ns_tag   < (1 << 11));  // Does this fit in 11 bits

  return (ns_tag << 18) | node_tag;
}
#endif

uint32_t PbNode::get_tag_number()
{
  RW_ASSERT(ynode_);
  RW_ASSERT(pbmod_);

  if (RW_YANG_STMT_TYPE_ROOT == ynode_->get_stmt_type()) {
    return pbmod_->get_nsid();
  }

  uint32_t node_tag = ynode_->get_node_tag();
  uint32_t ns_tag   = pbmod_->get_nsid() & 0xFFF; // Take Lower 12 bits

  RW_ASSERT(node_tag < (1 << 17));  // Does this fit in 17 bits
  RW_ASSERT(ns_tag   < (1 << 12));  // Does this fit in 12 bits

  return (ns_tag << 17) | node_tag;
}

bool PbNode::is_equivalent(
  PbNode* other_pbnode,
  const char** reason,
  PbModule* schema_pbmod) const
{
  UNUSED(schema_pbmod);

  const char* junk = nullptr;
  if (nullptr == reason) {
    reason = &junk;
  }
  *reason = nullptr;

  if (   (pb_field_type_ != other_pbnode->pb_field_type_
          && (*reason = "proto field type"))
      || (inline_ != other_pbnode->inline_
          && (*reason = "protobuf-c field inline"))
      || (inline_max_ != other_pbnode->inline_max_
          && (*reason = "protobuf-c field inline size"))
      || (inline_max_limit_ != other_pbnode->inline_max_limit_
          && (*reason = "protobuf-c field inline size limit"))
      || (string_max_ != other_pbnode->string_max_
          && (*reason = "protobuf-c field string size"))
      || (string_max_limit_ != other_pbnode->string_max_limit_
          && (*reason = "protobuf-c field string size limit"))
      || (optional_ != other_pbnode->optional_
          && (*reason = "proto optional/required"))
      || ((pbenum_ == nullptr) != (other_pbnode->pbenum_ == nullptr)
          && (*reason = "proto field enum type defined"))
      || ((!yext_msg_new_) != (!other_pbnode->yext_msg_new_)
          && (*reason = "msg-new extension existence"))
      || (ynode_->get_node_tag() != other_pbnode->ynode_->get_node_tag()
          && (*reason = "yang node tag"))
      || (ynode_->get_stmt_type() != other_pbnode->ynode_->get_stmt_type()
          && (*reason = "yang statement type"))
      || (ynode_->is_key() != other_pbnode->ynode_->is_key()
          && (*reason = "yang key"))
      || (flat_ != other_pbnode->flat_
          && (*reason = "flat extension"))
#if 0
      || (0 != strcmp(ynode_->get_prefix(), other_pbnode->ynode_->get_prefix())
          && (*reason = "yang namespace prefix"))
#endif
      || (0 != strcmp(ynode_->get_name(), other_pbnode->ynode_->get_name())
          && (*reason = "yang element name"))
     // ATTN: Also other yang refine targets: mandatory, config, presence, num-elements
     ) {
    return false;
  }

  if (pbenum_) {
    RW_ASSERT(other_pbnode->pbenum_);
    if (!pbenum_->is_equivalent(other_pbnode->pbenum_)) {
      *reason = "yang enums are not equivalent";
      return false;
    }
  }

  const char* mydv = ynode_->get_default_value();
  const char* odv = other_pbnode->ynode_->get_default_value();
  if (mydv || odv) {
    if (   ((nullptr == mydv) != (nullptr == odv)
            && (*reason = "yang default value exists"))
        || (0 != strcmp(mydv, odv)
            && (*reason = "yang default value"))) {
      return false;
    }
  }

  return true;
}

void PbNode::add_augment_mod(
  PbModule* other_pbmod)
{
  RW_ASSERT(other_pbmod);

  // Always do it and don't care about return value
  reference_pbmods_.emplace(other_pbmod);

  if (pbmod_->equals_or_imports_transitive(other_pbmod)) {
    return;
  }

  auto status = augment_pbmods_.emplace(other_pbmod);
  if (status.second) {
    /**
     * Have not seen this one before.  Keep track of LCI.  It is not
     * necessary to determine the LCIs for the node as viewed by every
     * package.  It is only necessary to determine what the current
     * schema sees as the LCI, because that is the only information
     * that is needed to generate the correct message references.  If
     * the current schema package is the LCI, then the message must be
     * (re)generated in the schema package.  Otherwise, references to
     * the message should be resolved to the LCI package.
     */
    RW_ASSERT(lci_pbmod_);
    lci_pbmod_ = lci_pbmod_->least_common_importer(other_pbmod);
  }
}

YangExtension* PbNode::find_ext(const char* module, const char* ext)
{
  RW_ASSERT(module);
  RW_ASSERT(ext);
  YangExtension* yext = nullptr;
  YangNode* ynode = ynode_;
  RW_ASSERT(ynode);

  // ATTN: What about dups?  Should complain if we see them.
  // ATTN: dup complaints depend on the extension?

  yext = ynode->get_first_extension();
  if (nullptr != yext) {
    yext = yext->search(module, ext);
    if (nullptr != yext) {
      return yext;
    }
  }

  if (ynode->is_leafy()) {
    // ATTN: Should iterate all the values?
    YangValue* yvalue = get_yvalue();
    if (yvalue) {
      yext = yvalue->get_first_extension();
      if (nullptr != yext) {
        yext = yext->search(module, ext);
        if (nullptr != yext) {
          return yext;
        }
      }
    }

    YangType* ytype = ynode->get_type();
    while (1) {
      yext = ytype->get_first_extension();
      if (nullptr != yext) {
        yext = yext->search(module, ext);
        if (nullptr != yext) {
          return yext;
        }
      }

      if (RW_YANG_LEAF_TYPE_LEAFREF != ytype->get_leaf_type()) {
        break;
      }

      // If leafref, also need to iterate the chain to find extensions.
      ynode = ynode->get_leafref_ref();
      if (!ynode) {
        // ATTN: leafref in grouping fail ref lookup, because that requires instantiation by a uses.
        break;
      }

      // Check the target node's extensions now; the enclosing loop will check the target type
      yext = ynode->get_first_extension();
      if (nullptr != yext) {
        yext = yext->search(module, ext);
        if (nullptr != yext) {
          return yext;
        }
      }

      ytype = ynode->get_type();
      RW_ASSERT(ytype);
    }
  }
  return yext;
}

void PbNode::populate_module_root()
{
  RW_ASSERT(!parse_started_);

  RW_ASSERT(pbmod_);
  YangModule* ymod = pbmod_->get_ymod();
  RW_ASSERT(ymod);
  for (auto yni = ymod->node_begin(); yni != ymod->node_end(); ++yni) {
    PbNode* pbnode = pbmodel_->get_node(&*yni, this);
    RW_ASSERT(pbnode);
  }

  for (auto yni = ymod->grouping_begin(); yni != ymod->grouping_end(); ++yni) {
    PbNode* pbnode = pbmodel_->get_node(&*yni, this);
    RW_ASSERT(pbnode);
  }
}

void PbNode::parse_root()
{
  RW_ASSERT(!parse_started_);
  parse_started_ = true;
  parse_element();

  for (auto const& pbnode: child_pbnodes_) {
    pbnode->parse_nonroot();
  }

  parse_complete();
}

void PbNode::parse_nonroot()
{
  if (parse_started_) {
    return;
  }
  parse_started_ = true;

  parse_extensions();
  parse_element();
  parse_children();
  parse_complete();
}

void PbNode::parse_extensions()
{
  RW_ASSERT(parse_started_);
  RW_ASSERT(!parse_complete_);
  RW_ASSERT(ynode_);
  rw_yang_stmt_type_t stmt_type = ynode_->get_stmt_type();

  for (YangExtensionIter xi = ynode_->extension_begin();
       xi != ynode_->extension_end();
       ++xi) {
    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_NEW)) {
      if (yext_msg_new_) {
        std::ostringstream oss;
        oss << "Ignoring extra " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", original at " << yext_msg_new_->get_location();
        pbmodel_->warning( xi->get_location(), oss.str() );
        continue;
      }
      if (!PbModel::is_proto_typename(xi->get_value())) {
        std::ostringstream oss;
        oss << "Extension " << xi->get_name()
            << " on node " << ynode_->get_name()
            << " must be valid proto typename: " << xi->get_value();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_GROUPING:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_CHOICE:
        case RW_YANG_STMT_TYPE_CASE:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;

        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_msg_new_ = &*xi;
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_NAME)) {
      if (yext_msg_name_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_msg_name_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }
      const char* value = xi->get_value();
      if (!PbModel::is_proto_typename(value)) {
        std::ostringstream oss;
        oss << "Extension " << xi->get_name()
            << " on node " << ynode_->get_name()
            << " must be valid proto typename: " << value;
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_GROUPING:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_CHOICE:
        case RW_YANG_STMT_TYPE_CASE:
        case RW_YANG_STMT_TYPE_RPC:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_msg_name_ = &*xi;
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_FLAT)) {
      if (yext_msg_flat_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_msg_flat_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_CHOICE:
        case RW_YANG_STMT_TYPE_CASE:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_msg_flat_ = &*xi;
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_PROTO_MAX_SIZE)) {
      if (yext_msg_proto_max_size_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_msg_proto_max_size_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_GROUPING:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_msg_proto_max_size_ = &*xi;

      const char* value = xi->get_value();
      if (value[0] != '\0') {
        intmax_t max_size = 0;
        unsigned consumed = 0;
        int retval = sscanf(value, "%jd%n", &max_size, &consumed);
        if (   retval == 1
            && consumed == strlen(value)
            && max_size > 0
            && max_size < UINT32_MAX) {
          pbc_size_limit_ = max_size;
          pbc_size_max_set_ = true;
          continue;
        }
      }

      std::ostringstream oss;
      oss << "Bad value for " << xi->get_name() << " extension: '"
          << xi->get_value() << "'";
      pbmodel_->error( xi->get_location(), oss.str() );
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_NAME)) {
      if (yext_field_name_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_field_name_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }
      if (!PbModel::is_identifier(xi->get_value())) {
        std::ostringstream oss;
        oss << "Extension " << xi->get_name()
            << " on node " << ynode_->get_name()
            << " must be valid C identifier: " << xi->get_value();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_ANYXML:
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_GROUPING:
        case RW_YANG_STMT_TYPE_LEAF:
        case RW_YANG_STMT_TYPE_LEAF_LIST:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_CHOICE:
        case RW_YANG_STMT_TYPE_CASE:
        case RW_YANG_STMT_TYPE_RPC:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_field_name_ = &*xi;
      continue;
    }


    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_TAG)) {
      if (yext_field_tag_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_field_tag_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_ANYXML:
        case RW_YANG_STMT_TYPE_CONTAINER:
        case RW_YANG_STMT_TYPE_GROUPING:
        case RW_YANG_STMT_TYPE_LEAF:
        case RW_YANG_STMT_TYPE_LEAF_LIST:
        case RW_YANG_STMT_TYPE_LIST:
        case RW_YANG_STMT_TYPE_CHOICE:
        case RW_YANG_STMT_TYPE_CASE:
        case RW_YANG_STMT_TYPE_RPC:
        case RW_YANG_STMT_TYPE_RPCIO:
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_field_tag_ = &*xi;
      continue;
    }

    if (xi->is_match(RW_YANG_NOTIFY_MODULE, RW_YANG_NOTIFY_EXT_LOG_EVENT_ID)) {
      if (yext_notif_log_event_id_) {
        std::ostringstream oss;
        oss << "More than one " << xi->get_name()
            << " extension on node " << ynode_->get_name()
            << ", first at " << yext_notif_log_event_id_->get_location();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }

      switch (stmt_type) {
        case RW_YANG_STMT_TYPE_NOTIF:
          break;
        default:
          std::ostringstream oss;
          oss << "Unexpected yang statement type "
              << rw_yang_stmt_type_string(stmt_type)
              << " for extension " << xi->get_name();
          pbmodel_->error(xi->get_location(), oss.str());
          continue;
      }
      yext_notif_log_event_id_ = &*xi;
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FIELD_MERGE_BEHAVIOR)) {
        if (yext_field_merge_behavior_) {
            std::ostringstream oss;
            oss << "More than one " << xi->get_name()
                << " extension on node " << ynode_->get_name()
                << ", first at " << yext_field_merge_behavior_->get_location();
            pbmodel_->error( xi->get_location(), oss.str() );
            continue;
        }

        switch (stmt_type) {
            // Applicable only for PB repeated fields
            case RW_YANG_STMT_TYPE_LIST:
            case RW_YANG_STMT_TYPE_LEAF_LIST:
                break;
            default:
                std::ostringstream oss;
                oss << "Unexpected yang statement type "
                    << rw_yang_stmt_type_string(stmt_type)
                    << " for extension " << xi->get_name();
                pbmodel_->error(xi->get_location(), oss.str());
                continue;
        }

        if (strcmp(xi->get_value(), "default") &&
            strcmp(xi->get_value(), "by-keys") &&
            strcmp(xi->get_value(), "none")) {

            std::ostringstream oss;
            oss << "Unexpected yang extension value for "
                << RW_YANG_PB_EXT_FIELD_MERGE_BEHAVIOR
                << ". Extension value is \'" << xi->get_value()
                << "\'.";
            pbmodel_->error(xi->get_location(), oss.str());
            continue;
        }

        yext_field_merge_behavior_ = &*xi;
        continue;
    }

  }

  yext_field_inline_ = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_INLINE);
  if (yext_field_inline_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ANYXML:
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_LIST:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
      case RW_YANG_STMT_TYPE_RPCIO:
      case RW_YANG_STMT_TYPE_NOTIF:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_field_inline_->get_name();
        pbmodel_->error(yext_field_inline_->get_location(), oss.str());
    }
  }

  yext_field_inline_max_ = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_INLINE_MAX);
  if (yext_field_inline_max_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_LIST:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_field_inline_max_->get_name();
        pbmodel_->error(yext_field_inline_max_->get_location(), oss.str());
    }
  }

  yext_field_string_max_ = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_STRING_MAX);
  if (yext_field_string_max_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ANYXML:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_field_string_max_->get_name();
        pbmodel_->error(yext_field_string_max_->get_location(), oss.str());
    }
  }

  yext_field_type_ = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_TYPE);
  if (yext_field_type_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ANYXML:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_field_type_->get_name();
        pbmodel_->error(yext_field_type_->get_location(), oss.str());
    }
  }

  yext_field_c_type_ = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FLD_C_TYPE);
  if (yext_field_c_type_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_ANYXML:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_field_c_type_->get_name();
        pbmodel_->error(yext_field_c_type_->get_location(), oss.str());
    }
  }

  yext_notif_log_common_ = find_ext(RW_YANG_NOTIFY_MODULE, RW_YANG_NOTIFY_EXT_LOG_COMMON);
  if (yext_notif_log_common_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
      case RW_YANG_STMT_TYPE_LIST:
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_notif_log_common_->get_name();
        pbmodel_->error(yext_notif_log_common_->get_location(), oss.str());
    }
  }

  yext_notif_log_event_id_ = find_ext(RW_YANG_NOTIFY_MODULE, RW_YANG_NOTIFY_EXT_LOG_EVENT_ID);
  if (yext_notif_log_event_id_) {
    switch (stmt_type) {
      case RW_YANG_STMT_TYPE_NOTIF:
        /* ATTN: And only when an immediate child of a notification? */
        break;
      default:
        std::ostringstream oss;
        oss << "Unexpected yang statement type "
            << rw_yang_stmt_type_string(stmt_type)
            << " for extension " << yext_notif_log_event_id_->get_name();
        pbmodel_->error(yext_notif_log_event_id_->get_location(), oss.str());
    }

    if (has_default()) {
      std::ostringstream oss;
      oss << "Extension " << yext_notif_log_event_id_->get_name()
          << " cannot be used with a default value";
      pbmodel_->error(yext_notif_log_event_id_->get_location(), oss.str());
      yext_notif_log_event_id_ = nullptr;
    }
  }
}

void PbNode::parse_element()
{
  RW_ASSERT(ynode_);

  if (ynode_->is_key()) {
    // A mandatory Yang node other than key should be optional
    // CLI on a mode level container will send only the modified fields which
    // may not include mandatory fields.
    optional_ = false;
  }

  if (!ynode_->is_config()) {
    has_op_data_ = true;
  }

  // ATTN: Need to do something about presense?

  if (ynode_->is_listy()) {
    max_elements_ = ynode_->get_max_elements();
    RW_ASSERT(max_elements_);
  } else {
    max_elements_ = 1;
  }

  // Do flat extension processing.  Must happen before inline.
  for (unsigned limit = 0; 1; ++limit) {
    RW_ASSERT(limit < 10); // parsing should stop on its own
    auto pi = map_parse_flat.find(flat_);
    RW_ASSERT(pi != map_parse_flat.end());
    parser_control_t pc = (this->*(pi->second))();
    switch (pc) {
      case parser_control_t::DO_REDO:
        continue;
      case parser_control_t::DO_NEXT:
      case parser_control_t::DO_SKIP_TO_SIBLING:
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
    break;
  }

  // Parse the inline extension.
  for (unsigned limit = 0; 1; ++limit) {
    RW_ASSERT(limit < 10); // parsing should stop on its own
    auto pi = map_parse_inline.find(inline_);
    RW_ASSERT(pi != map_parse_inline.end());
    parser_control_t pc = (this->*(pi->second))();
    switch (pc) {
      case parser_control_t::DO_REDO:
        continue;
      case parser_control_t::DO_NEXT:
      case parser_control_t::DO_SKIP_TO_SIBLING:
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
    break;
  }

  if (ynode_->is_listy()) {
    // Parse the inline-max extension.
    for (unsigned limit = 0; 1; ++limit) {
      RW_ASSERT(limit < 10); // parsing should stop on its own
      auto pi = map_parse_inline_max.find(inline_max_);
      RW_ASSERT(pi != map_parse_inline_max.end());
      parser_control_t pc = (this->*(pi->second))();
      switch (pc) {
        case parser_control_t::DO_REDO:
          continue;
        case parser_control_t::DO_NEXT:
        case parser_control_t::DO_SKIP_TO_SIBLING:
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      break;
    }
  }

  /* ATTN: Must have event-id table in PbModel, and must validate that they are all unique! */
  if (yext_notif_log_event_id_) {
    const char* value = yext_notif_log_event_id_->get_value();
    RW_ASSERT(value);
    uintmax_t event_id = 0;
    unsigned consumed = 0;
    int retval = sscanf(value, "%ju%n", &event_id, &consumed);
    if (   retval != 1
        || consumed != strlen(value)
        || event_id == 0
        || event_id >= UINT32_MAX) {
      // Bad value.
      std::ostringstream oss;
      oss << "Bad value for " << yext_notif_log_event_id_->get_name() << " extension: '"
          << value << "'";
      pbmodel_->error(yext_notif_log_event_id_->get_location(), oss.str());
    } else {
      notif_log_event_id_ = (uint32_t)event_id;
    }
  }

  parse_pb_field_type();

  if (ynode_->is_leafy()) {
    // Parse the string-max extension.
    for (unsigned limit = 0; 1; ++limit) {
      RW_ASSERT(limit < 10); // parsing should stop on its own
      auto pi = map_parse_string_max.find(string_max_);
      RW_ASSERT(pi != map_parse_string_max.end());
      parser_control_t pc = (this->*(pi->second))();
      switch (pc) {
        case parser_control_t::DO_REDO:
          continue;
        case parser_control_t::DO_NEXT:
        case parser_control_t::DO_SKIP_TO_SIBLING:
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      break;
    }
  }
}

void PbNode::parse_children()
{
  RW_ASSERT(parse_started_);
  RW_ASSERT(!parse_complete_);
  RW_ASSERT(ynode_);

  if (!ynode_->is_leafy()) {
    for (YangNodeIter yni = ynode_->child_begin(); yni != ynode_->child_end(); ++yni) {
      PbNode* pbnode = pbmodel_->get_node(&*yni, this);
      pbnode->parse_nonroot();
      if (pbnode->has_list_descendants()) {
        has_list_descendants_ = true;
      }
      if (pbnode->has_op_data()) {
        has_op_data_ = true;
      }
    }
  }
}

void PbNode::parse_complete()
{
  RW_ASSERT(parse_started_);
  RW_ASSERT(!parse_complete_);

  // Apply all of the children augments to this node.
  // Save all the module references.
  for (auto const& child_pbnode: child_pbnodes_) {
    for (auto const& other_pbmod: child_pbnode->augment_pbmods_) {
      add_augment_mod(other_pbmod);
    }
    reference_pbmods_.insert(
      child_pbnode->reference_pbmods_.begin(),
      child_pbnode->reference_pbmods_.end());
  }
  parse_complete_ = true;
}

parser_control_t PbNode::parse_noop()
{
  return parser_control_t::DO_NEXT;
}

parser_control_t PbNode::parse_inline_unset()
{
  if (!yext_field_inline_) {
    inline_ = inline_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  auto mvi = map_value_inline.find(yext_field_inline_->get_value());
  if (mvi != map_value_inline.end()) {
    inline_ = mvi->second;
    return parser_control_t::DO_REDO;
  }

  std::ostringstream oss;
  oss << "Bad value for " << yext_field_inline_->get_name() << " extension: '"
      << yext_field_inline_->get_value() << "'";
  pbmodel_->error(yext_field_inline_->get_location(), oss.str());

  // Assume auto for now.
  inline_ = inline_t::SEL_AUTO;
  return parser_control_t::DO_REDO;
}

parser_control_t PbNode::parse_inline_auto()
{
  if (parent_pbnode_ && parent_pbnode_->is_flat()) {
    inline_ = inline_t::SEL_TRUE;
  } else {
    inline_ = inline_t::SEL_FALSE;
  }
  return parser_control_t::DO_NEXT;
}

parser_control_t PbNode::parse_inline_max_unset()
{
  if (!yext_field_inline_max_) {
    inline_max_ = inline_max_t::SEL_YANG;
    return parser_control_t::DO_REDO;
  }

  // Is the extension value a keyword?
  const char* value = yext_field_inline_max_->get_value();
  auto mvi = map_value_inline_max.find(value);
  if (mvi != map_value_inline_max.end()) {
    inline_max_ = mvi->second;
    return parser_control_t::DO_REDO;
  }

  // Is the extension value a number?
  intmax_t limit = 0;
  unsigned consumed = 0;
  int retval = sscanf(value, "%jd%n", &limit, &consumed);
  if (retval == 1 && consumed == strlen(value) && limit >= 2 && limit < UINT32_MAX) {
    inline_max_limit_ = (uint32_t)limit;
    inline_max_ = inline_max_t::SEL_VALUE;

    if (inline_max_limit_ > max_elements_) {
      std::ostringstream oss;
      oss << "Extension " << RW_YANG_PB_EXT_FLD_INLINE_MAX
          << " specifies larger value than yang max-elements: "
          << inline_max_limit_ << " < " << max_elements_;
      pbmodel_->warning(yext_field_inline_max_->get_location(), oss.str());
    }

    return parser_control_t::DO_NEXT;
  }

  // Bad value.
  std::ostringstream oss;
  oss << "Bad value for " << yext_field_inline_max_->get_name() << " extension: '"
      << value << "'";
  pbmodel_->error(yext_field_inline_max_->get_location(), oss.str());

  // Default to yang limits.
  inline_max_ = inline_max_t::SEL_YANG;
  return parser_control_t::DO_REDO;
}

parser_control_t PbNode::parse_inline_max_yang()
{
  if (inline_ != inline_t::SEL_TRUE) {
    inline_max_ = inline_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  if (max_elements_ == UINT32_MAX) {
    inline_max_ = inline_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  if (max_elements_ > (1024*1024)) {
    std::ostringstream oss;
    oss << "Refusing to inline huge max-elements " << max_elements_;
    pbmodel_->warning( get_ynode()->get_location(), oss.str() );
    inline_max_ = inline_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  if (max_elements_ > 1024) {
    std::ostringstream oss;
    oss << "Inlining very large max-elements " << max_elements_;
    pbmodel_->warning( get_ynode()->get_location(), oss.str() );
    pbmodel_->warning( get_ynode()->get_location(), "  specify explicit inline limit to suppress this message" );
  }

  inline_max_limit_ = max_elements_;
  inline_max_ = inline_max_t::SEL_VALUE;
  return parser_control_t::DO_NEXT;
}

parser_control_t PbNode::parse_string_max_unset()
{
  if (!get_yvalue()) {
    // ATTN: IDREF
    if (yext_field_string_max_) {
      if (pb_field_type_ == PbFieldType::pbbool) {
        std::ostringstream oss;
        oss << "Bad extension: " << yext_field_string_max_->get_name() << " on non-stringy leaf";
        pbmodel_->error( yext_field_string_max_->get_location(), oss.str() );
        string_max_ = string_max_t::SEL_NONE;
        return parser_control_t::DO_NEXT;
      }
    } else {
      string_max_ = string_max_t::SEL_NONE;
      return parser_control_t::DO_NEXT;
    }
  } else {
    switch (get_yvalue()->get_leaf_type()) {
      case RW_YANG_LEAF_TYPE_INT8:
      case RW_YANG_LEAF_TYPE_INT16:
      case RW_YANG_LEAF_TYPE_INT32:
      case RW_YANG_LEAF_TYPE_INT64:
      case RW_YANG_LEAF_TYPE_UINT8:
      case RW_YANG_LEAF_TYPE_UINT16:
      case RW_YANG_LEAF_TYPE_UINT32:
      case RW_YANG_LEAF_TYPE_UINT64:
      case RW_YANG_LEAF_TYPE_DECIMAL64:
      case RW_YANG_LEAF_TYPE_EMPTY:
      case RW_YANG_LEAF_TYPE_BOOLEAN:
      case RW_YANG_LEAF_TYPE_ENUM:
      case RW_YANG_LEAF_TYPE_LEAFREF:
        // These are not stringy
        if (yext_field_string_max_) {
          std::ostringstream oss;
          oss << "Bad extension: " << yext_field_string_max_->get_name() << " on non-stringy leaf";
          pbmodel_->error( yext_field_string_max_->get_location(), oss.str() );
        }
        string_max_ = string_max_t::SEL_NONE;
        return parser_control_t::DO_NEXT;

      case RW_YANG_LEAF_TYPE_STRING:
      case RW_YANG_LEAF_TYPE_IDREF:
      case RW_YANG_LEAF_TYPE_INSTANCE_ID:
      case RW_YANG_LEAF_TYPE_BITS:
      case RW_YANG_LEAF_TYPE_BINARY:
      case RW_YANG_LEAF_TYPE_ANYXML:
        if (!yext_field_string_max_) {
          string_max_ = string_max_t::SEL_YANG;
          return parser_control_t::DO_REDO;
        }
        break;

      default:
        RW_ASSERT_NOT_REACHED();
    }
  }

  // Is the extension value a keyword?
  const char* value = yext_field_string_max_->get_value();
  auto mvi = map_value_string_max.find(value);
  if (mvi == map_value_string_max.end()) {
    // Is the extension value 2 words starting with an encoding
    // specifier?
    if (0 == strncmp(value,"utf8 ",5)) {
      string_max_ = string_max_t::SEL_UTF8;
      value += 5;
      if (value[0]) {
        goto error;
      }
      mvi = map_value_string_max.find(value);
    } else if (0 == strncmp(value,"octet ",6)) {
      string_max_ = string_max_t::SEL_OCTET;
      value += 6;
      if (value[0]) {
        goto error;
      }
      mvi = map_value_string_max.find(value);
    }
  }

  if (mvi != map_value_string_max.end()) {
    switch (mvi->second) {
      case string_max_t::SEL_OCTET:
      case string_max_t::SEL_UTF8:
        // Already specified encoding?
        switch (string_max_) {
          case string_max_t::SEL_OCTET:
          case string_max_t::SEL_UTF8:
            goto error;
          default:
            break;
        }
        string_max_ = mvi->second;
        break;

      case string_max_t::SEL_YANG:
        break;

      case string_max_t::SEL_NONE:
        // Specified encoding with none?
        if (string_max_t::UNSET != string_max_) {
          goto error;
        }
        string_max_ = mvi->second;
        return parser_control_t::DO_NEXT;

      default:
        RW_ASSERT_NOT_REACHED();
    }

    // Yang.
    switch (string_max_) {
      case string_max_t::UNSET:
        string_max_ = string_max_t::SEL_YANG;
        break;
      case string_max_t::SEL_OCTET:
        string_max_ = string_max_t::SEL_OCTET_YANG;
        break;
      case string_max_t::SEL_UTF8:
        string_max_ = string_max_t::SEL_UTF8_YANG;
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
    return parser_control_t::DO_REDO;
  }

  // Is the extension value a number?
  {
    intmax_t limit = 0;
    unsigned consumed = 0;
    int retval = sscanf(value, "%jd%n", &limit, &consumed);
    if (retval == 1 && consumed == strlen(value) && limit >= 1 && limit < INT64_MAX) {
      string_max_limit_ = (uint64_t)limit;
      switch (string_max_) {
        case string_max_t::UNSET:
          string_max_ = string_max_t::SEL_VALUE;
          break;
        case string_max_t::SEL_OCTET:
          string_max_ = string_max_t::SEL_OCTET_VALUE;
          break;
        case string_max_t::SEL_UTF8:
          string_max_ = string_max_t::SEL_UTF8_VALUE;
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      return parser_control_t::DO_REDO;
    }
  }

error:
  // Bad value.
  std::ostringstream oss;
  oss << "Bad value for " << yext_field_string_max_->get_name() << "extension: '"
      << yext_field_string_max_->get_value() << "'";
  pbmodel_->error( yext_field_string_max_->get_location(), oss.str() );

  // Default to yang limits.
  string_max_ = string_max_t::SEL_YANG;
  return parser_control_t::DO_REDO;
}

parser_control_t PbNode::parse_string_max_yang()
{
  string_max_limit_ = get_yvalue()->get_max_length();
  if (0 == string_max_limit_) {
    string_max_ = string_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  if (inline_ != inline_t::SEL_TRUE) {
    string_max_ = string_max_t::SEL_NONE;
    string_max_limit_ = 0;
    return parser_control_t::DO_NEXT;
  }

  switch (string_max_) {
    case string_max_t::SEL_OCTET_YANG:
      string_max_ = string_max_t::SEL_OCTET_VALUE;
      break;
    case string_max_t::SEL_UTF8_YANG:
      string_max_ = string_max_t::SEL_UTF8_VALUE;
      break;
    case string_max_t::SEL_YANG:
      // Determine the default encoding from the leaf type.
      switch (get_yvalue()->get_leaf_type()) {
        case RW_YANG_LEAF_TYPE_BITS:
        case RW_YANG_LEAF_TYPE_BINARY:
        case RW_YANG_LEAF_TYPE_IDREF:
          string_max_ = string_max_t::SEL_OCTET_VALUE;
          break;
        case RW_YANG_LEAF_TYPE_STRING:
        case RW_YANG_LEAF_TYPE_INSTANCE_ID:
        case RW_YANG_LEAF_TYPE_ANYXML:
          string_max_ = string_max_t::SEL_UTF8_VALUE;
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return parser_control_t::DO_REDO;
}

parser_control_t PbNode::parse_string_max_octet_value()
{
  RW_ASSERT(string_max_limit_);
  string_max_limit_ *= 4;
  if (string_max_limit_ > 256*1024*1024) {
    std::ostringstream oss;
    oss << "Refusing to inline huge string max " << string_max_limit_;
    pbmodel_->error( get_ynode()->get_location(), oss.str() );
    inline_max_ = inline_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  string_max_ = string_max_t::SEL_VALUE;
  return parser_control_t::DO_NEXT;
}

parser_control_t PbNode::parse_string_max_utf8_value()
{
  RW_ASSERT(string_max_limit_);

  uint64_t new_max = string_max_limit_ * 4;
  if (new_max < string_max_limit_) {
    std::ostringstream oss;
    oss << "Refusing to inline huge string max " << string_max_limit_;
    pbmodel_->error( get_ynode()->get_location(), oss.str() );
    inline_max_ = inline_max_t::SEL_NONE;
    return parser_control_t::DO_NEXT;
  }

  string_max_ = string_max_t::SEL_OCTET_VALUE;
  return parser_control_t::DO_REDO;
}

void PbNode::parse_pb_field_type()
{
  if (!ynode_->is_leafy()) {
    pb_field_type_ = PbFieldType::pbmessage;
    return;
  }

  rw_yang_leaf_type_t leaf_type = get_effective_yang_leaf_type();
  switch (leaf_type) {
    case RW_YANG_LEAF_TYPE_ENUM: {
      pb_field_type_ = PbFieldType::pbenum;
      pbenum_ = get_pbmodel()->enum_get(get_effective_ytype(), this);
      break;
    }
    case RW_YANG_LEAF_TYPE_INT8:
    case RW_YANG_LEAF_TYPE_INT16:
    case RW_YANG_LEAF_TYPE_INT32:
    case RW_YANG_LEAF_TYPE_INT64:
    case RW_YANG_LEAF_TYPE_UINT8:
    case RW_YANG_LEAF_TYPE_UINT16:
    case RW_YANG_LEAF_TYPE_UINT32:
    case RW_YANG_LEAF_TYPE_UINT64:
    case RW_YANG_LEAF_TYPE_DECIMAL64:
    case RW_YANG_LEAF_TYPE_EMPTY:
    case RW_YANG_LEAF_TYPE_BOOLEAN:
    case RW_YANG_LEAF_TYPE_STRING:
    case RW_YANG_LEAF_TYPE_IDREF:
    case RW_YANG_LEAF_TYPE_INSTANCE_ID:
    case RW_YANG_LEAF_TYPE_BITS:
    case RW_YANG_LEAF_TYPE_BINARY:
    case RW_YANG_LEAF_TYPE_ANYXML:
    case RW_YANG_LEAF_TYPE_UNION: {
      // Choose automatically
      auto i = map_yang_leaf_type.find(leaf_type);
      RW_ASSERT(i != map_yang_leaf_type.end());
      pb_field_type_ = i->second;
      break;
    }
    default:
      RW_ASSERT_NOT_REACHED();
  }

  // Forced field type extension?
  if (nullptr != yext_field_type_) {
    const char* value = yext_field_type_->get_value();
    auto i = map_value_type.find(value);
    if (i == map_value_type.end()) {
      std::ostringstream oss;
      oss << "Bad value for " << yext_field_type_->get_name() << " extension: '"
          << yext_field_type_->get_value() << "'";
      pbmodel_->error(yext_field_type_->get_location(), oss.str());
      pb_field_type_ = PbFieldType::automatic;
    } else {
      pb_field_type_ = i->second;
    }
  }

  // Mapping is okay?
  auto ok = type_allowed.find( std::make_pair(leaf_type, pb_field_type_) );
  if (ok == type_allowed.end()) {
    RW_ASSERT(yext_field_type_);
    std::ostringstream oss;
    oss << "Bad type in " << yext_field_type_->get_name() << " extension: "
        << yext_field_type_->get_value() << ": not allowed for yang leaf type "
        << rw_yang_leaf_type_string(leaf_type);
    pbmodel_->error( yext_field_type_->get_location(), oss.str() );

    // Set to string, just so that the parser can finish.
    pb_field_type_ = PbFieldType::pbstring;
  }
}

parser_control_t PbNode::parse_flat_unset()
{
  RW_ASSERT(pbmodel_);
  RW_ASSERT(ynode_);

  YangExtension* yext_msg_flat_ = ynode_->search_extension(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_MSG_FLAT);
  if (yext_msg_flat_) {
    auto vi = map_flat_value.find(yext_msg_flat_->get_value());
    if (vi != map_flat_value.end()) {
      flat_ = vi->second;
      return parser_control_t::DO_REDO;
    }

    // ATTN: Look in enclosing scopes: submodule, module

    std::ostringstream oss;
    oss << "Bad value for " << yext_msg_flat_->get_name() << " extension: '"
        << yext_msg_flat_->get_value() << "'";
    pbmodel_->error( yext_msg_flat_->get_location(), oss.str() );

    // Assume auto for now.
  }

  // Didn't find one set.  The default is auto.
  flat_ = flat_t::SEL_AUTO;
  return parser_control_t::DO_REDO;
}

parser_control_t PbNode::parse_flat_auto()
{
  if (parent_pbnode_ && parent_pbnode_->is_flat()) {
    flat_ = flat_t::SEL_TRUE;
    return parser_control_t::DO_NEXT;
  }

  flat_ = flat_t::SEL_FALSE;
  return parser_control_t::DO_NEXT;
}


} // namespace rw_yang

// End of yangpbc_node.cpp
