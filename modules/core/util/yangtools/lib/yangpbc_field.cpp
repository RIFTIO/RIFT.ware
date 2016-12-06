
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
 * @file yangpbc_field.cpp
 *
 * Convert yang schema to google protobuf: proto field support
 */

#include "rwlib.h"
#include "yangpbc.hpp"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>

#include <fstream>
#include <ios>
#include <iostream>
#include <mutex>
#include <sstream>
namespace rw_yang {

PbField::map_parse_tag_t PbField::map_parse_tag;


void PbField::init_tables()
{
  // protobuf-fld-tag
  map_parse_tag[tag_t::UNSET] = &PbField::parse_tag_unset;
  map_parse_tag[tag_t::SEL_AUTO] = &PbField::parse_tag_auto;
  map_parse_tag[tag_t::SEL_FIELD] = &PbField::parse_tag_field;
  map_parse_tag[tag_t::SEL_BASE] = &PbField::parse_tag_base;
  map_parse_tag[tag_t::SEL_VALUE] = &PbField::parse_tag_value;
}

PbField::PbField(
  PbMessage* owning_pbmsg,
  PbNode* pbnode,
  PbMessage* field_pbmsg)
: pbnode_(pbnode),
  owning_pbmsg_(owning_pbmsg),
  field_pbmsg_(field_pbmsg)
{
  RW_ASSERT(owning_pbmsg_);
  RW_ASSERT(pbnode_);
  RW_ASSERT(get_pbmodel());
  RW_ASSERT(get_pbmod());

  auto ynode = pbnode_->get_ynode();
  RW_ASSERT(ynode);
  if (ynode->is_key() ||
      pbnode->is_leaf_list()) {
    proto_key_type_ = std::string("YangKeyspecKey") + PbModel::mangle_typename(pbnode_->get_proto_fieldname());
  }
}

PbModel* PbField::get_pbmodel() const
{
  return pbnode_->get_pbmodel();
}

PbModule* PbField::get_pbmod() const
{
  return pbnode_->get_pbmod();
}

PbModule* PbField::get_schema_defining_pbmod() const
{
  return pbnode_->get_schema_defining_pbmod();
}

PbNode* PbField::get_pbnode() const
{
  return pbnode_;
}

void PbField::set_field_pbmsg(PbMessage* pbmsg)
{
  field_pbmsg_ = pbmsg;
}

PbMessage* PbField::get_field_pbmsg() const
{
  return field_pbmsg_;
}

PbMessage* PbField::get_field_pbmsg_eq() const
{
  if (field_pbmsg_) {
    return field_pbmsg_->get_eq_pbmsg();
  }
  return nullptr;
}

PbMessage* PbField::get_field_pbmsg_eq_local_only() const
{
  PbMessage* pbmsg = get_field_pbmsg_eq();
  if (pbmsg && pbmsg->is_local_message()) {
    return pbmsg;
  }
  return nullptr;
}

PbMessage* PbField::get_owning_pbmsg() const
{
  return owning_pbmsg_;
}

YangNode* PbField::get_ynode() const
{
  return pbnode_->get_ynode();
}

rw_yang_stmt_type_t PbField::get_yang_stmt_type() const
{
  return pbnode_->get_yang_stmt_type();
}

YangValue* PbField::get_yvalue() const
{
  return get_ynode()->get_first_value();
}

unsigned PbField::get_tag_value() const
{
  return tag_value_;
}

bool PbField::is_message() const
{
  auto ynode = pbnode_->get_ynode();
  return !ynode->is_leafy();
}

bool PbField::is_inline_message() const
{
  return is_message() && pbnode_->is_field_inline();
}

bool PbField::is_field_inline() const
{
  return pbnode_->is_field_inline();
}

bool PbField::is_stringy() const
{
  return pbnode_->is_stringy();
}

bool PbField::is_bytes() const
{
  return (PbFieldType::pbbytes == pbnode_->get_pb_field_type());
}

void PbField::set_proto_fieldname(std::string proto_fieldname)
{
  RW_ASSERT(0 == proto_fieldname_.length());
  proto_fieldname_ = proto_fieldname;
}

std::string PbField::get_proto_fieldname() const
{
  if (0 == proto_fieldname_.length()) {
    proto_fieldname_ = pbnode_->get_proto_fieldname();
  }
  return proto_fieldname_;
}

std::string PbField::get_pbc_fieldname() const
{
  return PbModel::mangle_to_lowercase(get_proto_fieldname());
}

std::string PbField::get_proto_typename() const
{
  auto ynode = pbnode_->get_ynode();
  if (!ynode->is_leafy()) {
    RW_ASSERT(field_pbmsg_);
    return field_pbmsg_->get_proto_message_path();
  }
  std::string proto_typename = pbnode_->get_proto_simple_typename();
  RW_ASSERT(proto_typename.length());
  return proto_typename;
}

std::string PbField::get_pbc_typename() const
{
  auto ynode = pbnode_->get_ynode();
  if (!ynode->is_leafy()) {
    RW_ASSERT(field_pbmsg_);
    return field_pbmsg_->get_pbc_message_typename();
  }

  std::string pbc_typename = pbnode_->get_pbc_simple_typename();
  RW_ASSERT(pbc_typename.length());
  return pbc_typename;
}

std::string PbField::get_gi_typename(bool use_const) const
{
  auto ynode = pbnode_->get_ynode();
  if (!ynode->is_leafy()) {
    RW_ASSERT(field_pbmsg_);
    return field_pbmsg_->get_gi_typename() + "*";
  }
  std::string gi_typename = pbnode_->get_gi_simple_typename();
  RW_ASSERT(gi_typename.length());
  if (use_const && gi_typename == "char*") {
    gi_typename.insert(0, "const ");
  }
  return gi_typename;
}

std::string PbField::get_pbc_schema_key_descriptor()
{
  if (0 == pbc_schema_key_descriptor_.length()) {
    RW_ASSERT(owning_pbmsg_);
    std::string key_descriptor
      =   "yang_keyspec_key_"
        + PbModel::mangle_to_underscore(PbModel::mangle_typename(get_proto_fieldname()))
        + "__descriptor";
    if (get_yang_stmt_type() == RW_YANG_STMT_TYPE_LEAF_LIST) {
      pbc_schema_key_descriptor_
          = field_pbmsg_->get_pbc_schema_global(PbMessage::output_t::path_entry, key_descriptor.c_str());
    } else {
      pbc_schema_key_descriptor_
          = owning_pbmsg_->get_pbc_schema_global(PbMessage::output_t::path_entry, key_descriptor.c_str());
    }
  }
  return pbc_schema_key_descriptor_;
}

bool PbField::is_equivalent(
  PbField* other_pbfld,
  const char** reason,
  PbModule* schema_pbmod) const
{
  const char* junk = nullptr;
  if (nullptr == reason) {
    reason = &junk;
  }
  *reason = nullptr;

  if (   (tag_value_ != other_pbfld->tag_value_
          && (*reason = "tag value"))
      || ((field_pbmsg_ == nullptr) != (other_pbfld->field_pbmsg_ == nullptr)
          && (*reason = "proto field message existence"))
      || (get_pbmod() != other_pbfld->get_pbmod()
          && (*reason = "defining yang module"))) {
    return false;
  }

  if (!pbnode_->is_equivalent(other_pbfld->pbnode_, reason, schema_pbmod)) {
    return false;
  }

  if (   field_pbmsg_
      && other_pbfld->field_pbmsg_
      && !field_pbmsg_->is_equivalent(other_pbfld->field_pbmsg_, reason, nullptr, schema_pbmod)) {
    return false;
  }

  return true;
}

PbField::field_size_info_t PbField::get_pbc_size() const
{
  RW_ASSERT(parse_complete_);
  size_t field_size = 0;
  bool pointy = false;
  size_t pointer_size = sizeof(void*);
  size_t align_size = sizeof(int);

  switch (pbnode_->get_pb_field_type()) {
    case PbFieldType::pbmessage:
      pointy = true;
      field_size = field_pbmsg_->get_pbc_size();
      break;

    case PbFieldType::pbbool:
      field_size = sizeof(protobuf_c_boolean);
      align_size = sizeof(protobuf_c_boolean);
      break;

    case PbFieldType::pbenum:
      field_size = sizeof(int);
      align_size = sizeof(int);
      break;

    case PbFieldType::pbfloat:
      field_size = sizeof(float);
      align_size = sizeof(float);
      break;

    case PbFieldType::pbint32:
    case PbFieldType::pbuint32:
    case PbFieldType::pbsint32:
    case PbFieldType::pbfixed32:
    case PbFieldType::pbsfixed32:
      field_size = sizeof(uint32_t);
      align_size = sizeof(uint32_t);
      break;

    case PbFieldType::pbint64:
    case PbFieldType::pbuint64:
    case PbFieldType::pbsint64:
    case PbFieldType::pbfixed64:
    case PbFieldType::pbsfixed64:
      field_size = sizeof(uint64_t);
      align_size = sizeof(uint64_t);
      break;

    case PbFieldType::pbdouble:
      field_size = sizeof(double);
      align_size = sizeof(int);
      break;

    case PbFieldType::pbbytes:
      pointy = true;
      if (!pbnode_->is_field_inline()) {
        pointer_size = sizeof(ProtobufCBinaryData);
      } else {
        field_size = pbnode_->get_string_max();
      }
      break;

    case PbFieldType::pbstring:
      pointy = true;
      field_size = pbnode_->get_string_max();
      align_size = 1;
      break;

    case PbFieldType::hack_bits:
    case PbFieldType::hack_union:
      pointy = true;
      field_size = pbnode_->get_string_max();
      field_size = field_size ? field_size : 8; /* ATTN: This is totally bogus, needed for bits. */
      align_size = 1;
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  field_size_info_t retval{};

  if (pbnode_->get_yext_field_c_type()) {
    if (pbnode_->is_field_inline()) {
      // ATTN: Have no idea how big the field is!
      pointy = false;
      field_size = sizeof(void*);
      align_size = sizeof(void*);
    } else {
      pointy = true;
      field_size = sizeof(void*);
      align_size = sizeof(void*);
    }
  }

  if (get_ynode()->is_listy()) {
    if (pbnode_->is_field_inline()) {
      retval.field_size = field_size * pbnode_->get_inline_max();
      retval.align_size = align_size;
      retval.has_size = sizeof(size_t);
      return retval;
    }

    retval.field_size = sizeof(void*);
    retval.align_size = sizeof(void*);
    retval.has_size = sizeof(size_t);
    return retval;
  }

  if (pbnode_->is_field_inline()) {
    retval.field_size = field_size;
    retval.align_size = align_size;
    if (pbnode_->is_optional()) {
      retval.has_size = sizeof(protobuf_c_boolean);
    }
    return retval;
  }

  if (pointy) {
    retval.field_size = pointer_size;
    retval.align_size = sizeof(void*);
    return retval;
  }

  retval.field_size = field_size;
  retval.align_size = align_size;
  retval.has_size = sizeof(protobuf_c_boolean);
  return retval;
}

bool PbField::has_pbc_quantifier_field() const
{
  if (get_ynode()->is_listy()) {
    return true;
  }

  if (pbnode_->is_optional()) {
    if (pbnode_->is_field_inline()) {
      return true;
    }
    if (pbnode_->get_yext_field_c_type()) {
      return false;
    }
    switch (pbnode_->get_pb_field_type()) {
      case PbFieldType::pbmessage:
      case PbFieldType::pbstring:
      case PbFieldType::hack_bits:
      case PbFieldType::hack_union:
        break;
      default:
        return true;
    }
  }
  return false;
}

std::string PbField::get_pbc_quantifier_field() const
{
  if (get_ynode()->is_listy()) {
    return std::string("n_") + get_pbc_fieldname();
  }
  if (has_pbc_quantifier_field()) {
    return std::string("has_") + get_pbc_fieldname();
  }
  return "";
}

void PbField::parse_start()
{
  RW_ASSERT(!parse_complete_);
  if (!parse_started_) {
    parse_started_ = true;
    RW_ASSERT(pbnode_);

    if (field_pbmsg_) {
      RW_ASSERT(PbFieldType::pbmessage == pbnode_->get_pb_field_type());
    } else {
      if (PbFieldType::pbmessage == pbnode_->get_pb_field_type()) {
        field_pbmsg_ = get_pbmodel()->alloc_pbmsg(pbnode_, owning_pbmsg_);
        RW_ASSERT(field_pbmsg_);
      } else if (pbnode_->is_leaf_list()) {
        // Creating a PbMessage makes it easy to generate keyspec and
        // path-entry types for leaf-lists. This is a kind of hack, but it does
        // avoid lot of code duplication.
        field_pbmsg_ = get_pbmodel()->alloc_pbmsg(pbnode_, owning_pbmsg_, PbMsgType::message);
        RW_ASSERT(field_pbmsg_);
      }
    }

    if (field_pbmsg_) {
      field_pbmsg_->parse();
    }
  }
}

void PbField::parse_complete()
{
  RW_ASSERT(!parse_complete_);
  RW_ASSERT(parse_started_);

  // Parse the tag
  for (unsigned limit = 0; 1; ++limit) {
    RW_ASSERT(limit < 10); // parsing should stop on its own
    auto pi = map_parse_tag.find(tag_);
    RW_ASSERT(pi != map_parse_tag.end());
    parser_control_t pc = (this->*(pi->second))();
    switch (pc) {
      case parser_control_t::DO_REDO:
        continue;
      case parser_control_t::DO_SKIP_TO_SIBLING:
      case parser_control_t::DO_NEXT:
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
    break;
  }
  parse_complete_ = true;
}

parser_control_t PbField::parse_noop()
{
  return parser_control_t::DO_NEXT;
}

parser_control_t PbField::parse_tag_unset()
{
  // No extension?
  auto yext_field_tag = pbnode_->get_yext_field_tag();
  if (nullptr == yext_field_tag) {
    tag_ = tag_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  const char* value = yext_field_tag->get_value();
  if (value[0] == '\0') {
    goto error;
  }

  if (0 == strcmp(value,"auto")) {
    tag_ = tag_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  {
    const char* number = value;
    const char* plus = strchr(value, '+');
    if (plus == value) {
      tag_ = tag_t::SEL_BASE;
      number = plus+1;
      if (number[0] == '\0') {
        goto error;
      }
    } else if (nullptr != plus) {
      tag_ = tag_t::SEL_FIELD;
      tag_field_ = std::string(value,plus-value);
      number = plus+1;
      if (number[0] == '\0') {
        goto error;
      }
    } else {
      tag_ = tag_t::SEL_VALUE;
    }

    // Is the number actually a number?
    intmax_t tag_value = 0;
    unsigned consumed = 0;
    int retval = sscanf(number, "%jd%n", &tag_value, &consumed);
    if (retval == 1 && consumed == strlen(number) && tag_value > 0 && tag_value < UINT32_MAX) {
      tag_value_ = tag_value;
      return parser_control_t::DO_REDO; // perform adjustments...
    }
  }

error:
  std::ostringstream oss;
  oss << "Bad value for " << yext_field_tag->get_name() << " extension: '"
      << value << "'";
  get_pbmodel()->error( yext_field_tag->get_location(), oss.str() );
  tag_ = tag_t::SEL_AUTO;
  return parser_control_t::DO_REDO;
}

parser_control_t PbField::parse_tag_auto()
{
  RW_ASSERT(owning_pbmsg_);
  RW_ASSERT(pbnode_);
  tag_value_ = pbnode_->get_tag_number();
  tag_ = tag_t::SEL_VALUE;
  return parser_control_t::DO_REDO;
}

parser_control_t PbField::parse_tag_field()
{
  // ATTN - No support for this now.
  RW_ASSERT_NOT_REACHED();

  auto yext_field_tag = pbnode_->get_yext_field_tag();
  RW_ASSERT(yext_field_tag);
  RW_ASSERT(owning_pbmsg_);

  RW_ASSERT(tag_field_.size());
  PbField* other_pbfld = owning_pbmsg_->get_field_by_name(tag_field_);
  if (!other_pbfld) {
    std::ostringstream oss;
    oss << "Field '" << tag_field_ << "' not found in message";
    get_pbmodel()->error( yext_field_tag->get_location(), oss.str() );
    tag_ = tag_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  if (tag_t::SEL_VALUE != other_pbfld->tag_) {
    std::ostringstream oss;
    oss << "Field '" << tag_field_ << "' tag cannot depend on following field's tag";
    get_pbmodel()->error( yext_field_tag->get_location(), oss.str() );
    tag_ = tag_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  tag_value_ = other_pbfld->tag_value_ + 1;
  tag_ = tag_t::SEL_VALUE;
  return parser_control_t::DO_REDO;
}

parser_control_t PbField::parse_tag_base()
{
  // ATTN - No support for base now.
  RW_ASSERT_NOT_REACHED();

  auto yext_field_tag = pbnode_->get_yext_field_tag();
  auto yext_msg_tag_base = pbnode_->get_yext_msg_tag_base();

  // No base?
  if (nullptr == yext_msg_tag_base) {
    std::ostringstream oss;
    oss << "Base-relative tag extension used without specifying "
        << RW_YANG_PB_EXT_MSG_TAG_BASE
        << " in the uses or grouping statement";
    get_pbmodel()->error( yext_field_tag->get_location(), oss.str() );
    tag_ = tag_t::SEL_AUTO;
    return parser_control_t::DO_REDO;
  }

  // Is the value actually a number?
  const char* value = yext_msg_tag_base->get_value();
  intmax_t base = 0;
  unsigned consumed = 0;
  int retval = sscanf(value, "%jd%n", &base, &consumed);
  if (   retval == 1
      && consumed == strlen(value)
      && base > 0
      && base < UINT32_MAX) {
    tag_value_ += base;
    tag_ = tag_t::SEL_VALUE;
    return parser_control_t::DO_NEXT;
  }

  std::ostringstream oss;
  oss <<"Bad value for " << yext_field_tag->get_name() << " extension: '"
      << value << "'";
  get_pbmodel()->error( yext_field_tag->get_location(), oss.str() );
  tag_ = tag_t::SEL_AUTO;
  return parser_control_t::DO_REDO;
}

parser_control_t PbField::parse_tag_value()
{
  RW_ASSERT(tag_value_);
  RW_ASSERT(owning_pbmsg_);

  // ATTN  - this is not sufficient anymore

#if 0
  if (tag_value_ <= owning_pbmsg_->last_tag_) {
    std::ostringstream oss;
    oss << "Tag value for leaf " << get_ynode()->get_name()
        << " (" << tag_value_ << ") is duplicate or less than previous"
        << " tag in message (" << owning_pbmsg_->last_tag_ << ")";
    get_pbmodel()->error( get_ynode()->get_location(), oss.str() );
    return parser_control_t::DO_NEXT;
  }
  if (tag_value_ > owning_pbmsg_->last_tag_) {
    owning_pbmsg_->last_tag_ = tag_value_;
  }
#endif
  return parser_control_t::DO_NEXT;
}

void PbField::output_proto_key_message(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  os << pad1 << "message " << proto_key_type_ << " {" << std::endl;
  os << pad2 << "optional string string_key = 1;" << std::endl;
  output_proto_field_impl(proto_output_style_t::SEL_SCHEMA_KEY, os, indent+2);
  os << pad1 << "}" << std::endl;
}

void PbField::output_proto_key_field(unsigned n, std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  char buf[16];
  int bytes = snprintf(buf, sizeof(buf), "%02u", n);
  RW_ASSERT(bytes > 0 && bytes < (int)sizeof(buf));
  os << pad1 << "optional " << proto_key_type_ << " key" << buf << " = " << 100+n
     << " [ (rw_fopts) = { inline:true } ];" << std::endl;
}

void PbField::output_proto_schema_field(std::ostream& os, unsigned indent)
{
  output_proto_field_impl(proto_output_style_t::SEL_SCHEMA, os, indent);
}

void PbField::output_proto_field(std::ostream& os, unsigned indent)
{
  output_proto_field_impl(proto_output_style_t::SEL_OLD_STYLE, os, indent);
}

void PbField::output_proto_field_impl(proto_output_style_t style, std::ostream& os, unsigned indent)
{
  bool auto_default = false;
  std::string pad1(indent,' ');
  os << pad1;
  if (style == proto_output_style_t::SEL_SCHEMA_KEY) {
    auto_default = true;
    os << "required ";
  } else if (get_ynode()->is_listy()) {
    os << "repeated ";
  } else if (pbnode_->is_optional()) {
    os << "optional ";
  } else {
    auto_default = true;
    os << "optional ";
  }

  std::ostringstream rw_fopts;
  std::ostringstream opts;

  if (pbnode_->is_field_inline()) {
    rw_fopts << " inline:true";
  }

  auto inline_max = pbnode_->get_inline_max();
  if (inline_max) {
    rw_fopts << " inline_max:" << inline_max;
  }

  auto string_max = pbnode_->get_string_max();
  if (string_max) {
    rw_fopts << " string_max:" << string_max;
  }

  YangExtension* yext_field_c_type = pbnode_->get_yext_field_c_type();
  if (yext_field_c_type) {
    rw_fopts << " c_type:\"" << yext_field_c_type->get_value() << "\"";
  }

  if ((style != proto_output_style_t::SEL_SCHEMA_KEY) && pbnode_->is_key()) {
    rw_fopts << " key:true";
  }

  if (pbnode_->get_yext_notif_log_common()) {
    rw_fopts << " log_common:true";
  }

  if (pbnode_->get_yext_field_merge_behavior()) {
      if (pbnode_->is_leaf_list() || pbnode_->is_listy()) {
          rw_fopts << " merge_behavior:" << "\"" <<
              pbnode_->get_yext_field_merge_behavior()->get_value()
              << "\"";
      }
  }

  /*
   * ATTN: TGS: I don't think this code makes much sense.  Why is
   * event-id an extension instead of a default value?
   */
  if ((!strcmp(get_ynode()->get_name() , EVENT_ID_LEAF_NAME)) &&
     (pbnode_->get_yext_notif_log_common())) {
      uint32_t event_id = owning_pbmsg_->get_event_id();
      if (event_id ) {
        auto_default = false;
        if (opts.str().size()) {
         opts << ", ";
        }
        opts << "default=" << event_id;
      }
  }

  if (!pbnode_->is_listy()) {
    if (pbnode_->has_default() || auto_default) {
      std::string default_value = pbnode_->get_proto_default();
      if (default_value.length()) {
        if (opts.str().size()) {
          opts << ", ";
        }
        opts << "default=" << default_value;
      }
    }
  }

  if (rw_fopts.str().size()) {
    if (opts.str().size()) {
      opts << ", ";
    }
    opts << "(rw_fopts) = {" << rw_fopts.str() << " }";
  }

  os << get_proto_typename() << " " << pbnode_->get_proto_fieldname() << " = " << tag_value_;
  if (opts.str().size()) {
    os << " [ " << opts.str() << " ]";
  }
  os << ';' << std::endl;
}

void PbField::output_h_meta_data_macro(std::ostream& os, unsigned indent)
{
  std::string pad(indent+2,' ');
  std::string eol(", \\\n");

  auto max_elements = pbnode_->get_max_elements();
  RW_ASSERT(max_elements);

  os << "#define " << owning_pbmsg_->get_ypbc_global("m_field") << "_"
     << pbnode_->get_proto_fieldname() << "() \\" << std::endl;
  os << pad << owning_pbmsg_->get_rwpb_long_alias() << eol;
  os << pad << "\"" <<  pbnode_->get_ynode()->get_name() << "\"" << eol;
  os << pad << owning_pbmsg_->get_pbmod()->get_ypbc_global("g_module") << eol;
  os << pad << owning_pbmsg_->get_pbc_message_upper(PbMessage::output_t::message,"") << "field_"
     << pbnode_->get_proto_fieldname() << eol;
  os << pad << tag_value_ << eol;
  os << pad << max_elements << std::endl;
  os << std::endl;
}

void PbField::output_cpp_schema_key_init(std::ostream& os, unsigned indent, std::string offset)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  RW_ASSERT(offset.length());

  os << pad1 << "{" << std::endl;
  os << pad2 << "{PROTOBUF_C_MESSAGE_INIT(&" << get_pbc_schema_key_descriptor()
      << ", PROTOBUF_C_FLAG_REF_STATE_INNER, "
      << offset << ")}," << std::endl;
  os << pad2 << "nullptr, /*string_key*/" << std::endl;
  output_cpp_schema_default_init(os, indent+2);
  os << pad1 << "}," << std::endl;
}

void PbField::output_cpp_schema_default_init(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  os << pad1;
  if (!get_ynode()->is_listy()) {
    if (has_pbc_quantifier_field()) {
      os << pad1 << "false, ";
    }

    std::string def_val = pbnode_->get_c_default_init();
    if (def_val.length()) {
      os << def_val << ", ";
    }
  }
  os << "/* " << get_pbc_fieldname() << " */" << std::endl;
}

void PbField::output_cpp_ypbc_flddesc_body(std::ostream& os, unsigned indent, unsigned pbc_order, unsigned yang_order)
{
  std::string pad(indent, ' ');
  os << pad << "reinterpret_cast<const rw_yang_pb_module_t *>(&" << 
      get_schema_defining_pbmod()->get_ypbc_global("g_module") << ")," << std::endl;
  os << pad << "reinterpret_cast<const rw_yang_pb_msgdesc_t *>(&" <<
      owning_pbmsg_->get_ypbc_global("g_msg_msgdesc") << ")," << std::endl;
  os << pad << "&" << owning_pbmsg_->get_pbc_message_global("field_descriptors") <<
      "[" << pbc_order-1 << "]," << std::endl;
  os << pad << "\"" <<  pbnode_->get_ynode()->get_name() << "\"," << std::endl;
  os << pad << "RW_YANG_STMT_TYPE_" << rw_yang_stmt_type_string(get_yang_stmt_type()) << "," << std::endl;
  os << pad << "RW_YANG_LEAF_TYPE_" << rw_yang_leaf_type_string(pbnode_->get_effective_yang_leaf_type())
            << "," << std::endl;
  os << pad << yang_order << ", " << std::endl;
  os << pad << pbc_order << std::endl;
}

} // namespace rw_yang

// End of yangpbc_field.cpp
