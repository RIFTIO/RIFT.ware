
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
 * @file yangpbc_enum.cpp
 *
 * Convert yang schema to google protobuf: proto enum support
 */

#include "rwlib.h"
#include "yangpbc.hpp"
#include <sstream>

namespace rw_yang {


PbEnumValue::PbEnumValue(
  PbEnumType* pbenum,
  YangValue* yvalue)
: pbenum_(pbenum),
  yvalue_(yvalue)
{
  RW_ASSERT(pbenum_);
  RW_ASSERT(yvalue_);

  YangExtension *yext = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_ENUM_NAME);
  if (yext) {
    raw_enumerator_ = yext->get_value();
  } else {
    raw_enumerator_ = PbModel::mangle_identifier(yvalue_->get_name());
  }
}

PbModel* PbEnumValue::get_pbmodel() const
{
  return pbenum_->get_pbmodel();
}

PbModule* PbEnumValue::get_pbmod() const
{
  return pbenum_->get_pbmod();
}

PbEnumType* PbEnumValue::get_pbenum() const
{
  return pbenum_;
}

YangValue* PbEnumValue::get_yvalue() const
{
  return yvalue_;
}

int PbEnumValue::get_int_value() const
{
  return yvalue_->get_position();
}

std::string PbEnumValue::get_yang_enumerator() const
{
  return yvalue_->get_name();
}

std::string PbEnumValue::get_raw_enumerator() const
{
  return raw_enumerator_;
}

void PbEnumValue::set_raw_enumerator(std::string raw_enumerator)
{
  raw_enumerator_ = raw_enumerator;
}

std::string PbEnumValue::get_proto_enumerator() const
{
  if (0 == proto_enumerator_.length()) {
    proto_enumerator_ = raw_enumerator_;
  }
  return proto_enumerator_;
}

std::string PbEnumValue::get_pbc_enumerator() const
{
  if (0 == pbc_enumerator_.length()) {
    pbc_enumerator_ = pbenum_->get_proto_c_prefix() + "_"
      + PbModel::mangle_to_uppercase(raw_enumerator_);
  }
  return pbc_enumerator_;
}

bool PbEnumValue::is_equivalent(
  PbEnumValue* other) const
{
  if (   get_yang_enumerator() !=  other->get_yang_enumerator()
      || get_int_value() != other->get_int_value()
      || raw_enumerator_ != other->raw_enumerator_) {
    return false;
  }
  return true;
}

void PbEnumValue::parse()
{
  // nothing to do now
}

YangExtension* PbEnumValue::find_ext(
    const char* ns,
    const char* ext)
{
  YangExtension *yext = yvalue_->get_first_extension();
  if (yext) {
    yext = yext->search(ns, ext);
  }
  return yext;
}

/*****************************************************************************/
PbEnumType::PbEnumType(
  YangTypedEnum* ytenum,
  PbModule* pbmod)
: pbmod_(pbmod),
  ytenum_(ytenum),
  equivalent_pbenum_(this)
{
  RW_ASSERT(pbmod_);
  RW_ASSERT(ytenum_);

  const char* name = ytenum_->get_name();
  RW_ASSERT(name);

  YangExtension *yext = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_ENUM_TYPE);

  if (yext) {
    proto_enclosing_type_ = yext->get_value();
    /* ATTN: Verify that it is a good identifier */
  } else {
    proto_enclosing_type_ = PbModel::mangle_typename(PbModel::mangle_identifier(name));
  }
}

PbEnumType::PbEnumType(
  YangType* ytype,
  PbNode* pbnode)
: scoped_pbnode_(pbnode),
  ytype_(ytype),
  equivalent_pbenum_(this)
{
  RW_ASSERT(scoped_pbnode_);
  RW_ASSERT(ytype_);

  pbmod_ = scoped_pbnode_->get_pbmod();
  ytenum_ = ytype_->get_typed_enum();

  const char* ns = ytype_->get_ns();
  if (ns) {
    PbModule* type_pbmod = get_pbmodel()->module_lookup(ns);
    if (type_pbmod) {
      pbmod_ = type_pbmod;
      RW_ASSERT(type_pbmod);
    }
  }

  YangNode* ynode = scoped_pbnode_->get_ynode();
  RW_ASSERT(ynode);
  const char* name = ynode->get_name();
  RW_ASSERT(name);
  // Check whether a explicit typename is given.
  YangExtension *yext = find_ext(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_ENUM_TYPE);
  if (yext) {
    proto_enclosing_type_ = yext->get_value();
    /* ATTN: Verify that it is a good identifier */
  } else {
    proto_enclosing_type_ = PbModel::mangle_typename(PbModel::mangle_identifier(name));
  }

  /*
     ATTN: Can you tell the difference between an enum defined in a
     grouping and reused and a simple inline enum?
  */
}

YangExtension* PbEnumType::find_ext(
    const char *ns,
    const char *ext)
{
  YangExtension *yext = nullptr;
  if (ytenum_) {
    yext = ytenum_->get_first_extension();
  } else {
    yext = ytype_->get_first_extension();
  }
  if (yext) {
    yext = yext->search(ns, ext);
  }
  return yext;
}

PbModel* PbEnumType::get_pbmodel() const
{
  return pbmod_->get_pbmodel();
}

PbModule* PbEnumType::get_pbmod() const
{
  return pbmod_;
}

PbNode* PbEnumType::get_scoped_pbnode() const
{
  return scoped_pbnode_;
}

YangType* PbEnumType::get_ytype() const
{
  return ytype_;
}

YangTypedEnum* PbEnumType::get_ytenum() const
{
  return ytenum_;
}

bool PbEnumType::has_aliases() const
{
  return has_aliases_;
}

PbEnumType::enum_value_iter_t PbEnumType::begin() const
{
  return value_list_.cbegin();
}

PbEnumType::enum_value_iter_t PbEnumType::end() const
{
  return value_list_.cend();
}

PbEnumType* PbEnumType::get_equivalent_pbenum() const
{
  return equivalent_pbenum_;
}

void PbEnumType::set_equivalent_pbenum(PbEnumType* equivalent_pbenum)
{
  equivalent_pbenum_ = equivalent_pbenum;
}

bool PbEnumType::is_suppressed() const
{
  return this != equivalent_pbenum_;
}

bool PbEnumType::is_typed() const
{
  return nullptr != ytenum_;
}

void PbEnumType::make_proto_path_list() const
{
  if (proto_path_list_.size()) {
    return;
  }
  proto_path_list_.emplace_back(pbmod_->get_proto_package_name());
  proto_path_list_.emplace_back("YangEnum");
  proto_path_list_.emplace_back(proto_enclosing_type_);
  proto_path_list_.emplace_back("e");
}

std::string PbEnumType::get_location() const
{
  if (ytype_) {
    return ytype_->get_location();
  }
  RW_ASSERT(ytenum_);
  return ytenum_->get_location();
}

std::string PbEnumType::get_proto_enclosing_typename() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_proto_enclosing_typename();
  }
  return proto_enclosing_type_;
}

std::string PbEnumType::get_proto_type_path() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_proto_type_path();
  }
  make_proto_path_list();
  if (0 == proto_type_path_.length()) {
    make_proto_path_list();
    for (auto const& s: proto_path_list_) {
      proto_type_path_ = proto_type_path_ + "." + s;
    }
  }
  return proto_type_path_;
}

std::string PbEnumType::get_proto_c_prefix() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_proto_c_prefix();
  }
  if (proto_c_prefix_.length()) {
    return proto_c_prefix_;
  }
  proto_c_prefix_
    =   PbModel::mangle_to_uppercase(PbModel::mangle_to_underscore(pbmod_->get_proto_package_name())) + "_"
      + PbModel::mangle_to_uppercase(PbModel::mangle_to_underscore(get_proto_enclosing_typename()));
  return proto_c_prefix_;
}

std::string PbEnumType::get_pbc_typename() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_pbc_typename();
  }
  if (0 == pbc_type_.length()) {
    make_proto_path_list();
    bool first = true;
    for (auto const& s: proto_path_list_) {
      pbc_type_ = pbc_type_ + (first?"":"__") + PbModel::mangle_typename(s);
      first = false;
    }
  }
  return pbc_type_;
}

std::string PbEnumType::get_pbc_enum_desc() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_pbc_enum_desc();
  }
  return make_pbc_name("descriptor");
}

std::string PbEnumType::get_ypbc_var_enumdesc() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_ypbc_var_enumdesc();
  }
  if (ypbc_var_enumdesc_.length()) {
    return ypbc_var_enumdesc_;
  }
  return ypbc_var_enumdesc_ = get_ypbc_global("g_enumdesc");
}

std::string PbEnumType::get_ypbc_var_enumsort() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_ypbc_var_enumsort();
  }
  if (ypbc_var_enumsort_.length()) {
    return ypbc_var_enumsort_;
  }
  return ypbc_var_enumsort_ = get_ypbc_global("g_enumsort");
}

std::string PbEnumType::get_ypbc_var_enumstr() const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_ypbc_var_enumstr();
  }
  if (ypbc_var_enumstr_.length()) {
    return ypbc_var_enumstr_;
  }
  return ypbc_var_enumstr_ = get_ypbc_global("g_enumstr");
}

PbEnumValue* PbEnumType::get_value(
  const char* yang_def_val) const
{
  for (auto const& pbvalue: value_list_) {
    if (!yang_def_val) {
      // Just take the first one.
      return pbvalue.get();
    }
    const char *yn = pbvalue->get_yvalue()->get_name();
    if (0 == strcmp(yn, yang_def_val)) {
      return pbvalue.get();
    }
  }
  return nullptr;
}

std::string PbEnumType::make_pbc_name(const char* suffix) const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->make_pbc_name(suffix);
  }
  if (0 == pbc_basename_.length()) {
    make_proto_path_list();
    for (auto const& s: proto_path_list_) {
      pbc_basename_ = pbc_basename_ + PbModel::mangle_to_underscore(s) + "__" ;
    }
    RW_ASSERT(pbc_basename_.length());
  }
  return pbc_basename_ + suffix;
}

std::string PbEnumType::make_gi_name(const char* suffix) const
{
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->make_gi_name(suffix);
  }
  if (0 == gi_basename_.length()) {
    gi_basename_ = get_ypbc_global("gi") + "_";
  }
  return gi_basename_ + suffix;
}

std::string PbEnumType::make_gi_name(const std::string& discriminator) const
{
  return make_gi_name(discriminator.c_str());
}

std::string PbEnumType::get_ypbc_global(const std::string& discriminator) const
{
  return get_ypbc_global(discriminator.c_str());
}

std::string PbEnumType::get_ypbc_global(
  const char* discriminator) const
{
  RW_ASSERT(discriminator);
  if (this != equivalent_pbenum_) {
    return equivalent_pbenum_->get_ypbc_global(discriminator);
  }

  // ATTN: Check global symbols
  std::string name;
  PbModule* schema_pbmod = get_pbmodel()->get_schema_pbmod();
  if (schema_pbmod != pbmod_) {
     name  =   std::string("rw_ypbc_")
	 + get_pbmodel()->get_schema_pbmod()->get_proto_typename() + "_"
	 + pbmod_->get_proto_typename() + "_"
	 + proto_enclosing_type_ + "_"
	 + discriminator;
  } else {
     name  =   std::string("rw_ypbc_")
	 + pbmod_->get_proto_typename() + "_"
	 + proto_enclosing_type_ + "_"
	 + discriminator;
  }
  return name;
}

bool PbEnumType::is_equivalent(
  PbEnumType* other) const
{
  if (   pbmod_ != other->pbmod_
      || ytenum_ != other->ytenum_
      || proto_enclosing_type_ != other->proto_enclosing_type_
      || value_list_.size() != other->value_list_.size()) {
    return false;
  }

  auto mvi = value_list_.begin();
  auto ovi = other->value_list_.begin();
  while (mvi != value_list_.end()) {
    if (!mvi->get()->is_equivalent(ovi->get())) {
      return false;
    }
    ++mvi;
    ++ovi;
  }
  return true;
}

void PbEnumType::parse()
{
  if (parse_complete_) {
    return;
  }
  get_pbmodel()->enum_add(this);

  YangValueIter start;
  YangValueIter end;
  if (ytenum_) {
    // Typed enum
    start = ytenum_->enum_value_begin();
    end = ytenum_->enum_value_end();
  } else {
    // Local enum
    start = ytype_->value_begin();
    end = ytype_->value_end();
  }

  PbEnumValue::enum_value_int_set_t value_int_set;
  for (YangValueIter yvi = start; yvi != end; yvi++) {
    /*
       ATTN: Check that the value is still in the type?
       ATTN: This is needed when an enum exists in a union
       ATTN: Although, we already need much better support for unions
     */
    if (yvi->get_leaf_type() != RW_YANG_LEAF_TYPE_ENUM) {
      RW_ASSERT_NOT_REACHED(); // do not support unions yet
    }

    PbEnumValue::enum_value_uptr_t value_uptr(new PbEnumValue(this, &*yvi));
    PbEnumValue* value = value_uptr.get();
    value_list_.emplace_back(std::move(value_uptr));
    if (value_stab_.add_symbol(value->get_raw_enumerator(), value)) {
      std::ostringstream oss;
      oss << "Enummeration type " << get_proto_enclosing_typename() << " has duplicate enum names:"
          << value->get_raw_enumerator();
      get_pbmodel()->error( get_location(), oss.str() );
    }

    if (!has_aliases_) {
      auto status = value_int_set.emplace(yvi->get_position());
      if (!status.second) {
        has_aliases_ = true;
      }
    }
    value->parse();
  }

  if (0 == value_list_.size()) {
    std::ostringstream oss;
    oss << "Enummeration type " << get_proto_enclosing_typename() << " has no values";
    get_pbmodel()->error( get_location(), oss.str() );
  }

  parse_complete_ = true;
}

void PbEnumType::output_proto_enum(
  std::ostream& os,
  unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  std::string pad3(indent+4,' ');

  /*
   * The reason why the enum is placed in a message and then in a
   * generic enum type named 'e' is that we are trying to avoid the
   * google-protobuf C++ scoping error.  Enumeration identifiers are
   * treated as identifiers in the enum type's enclosing scope
   * (according to C/C++ enum scoping rules [pre C++11 enum class]).
   * The extra level gives every yang enum its own identifier scope,
   * which allows a closer mapping form yang to protobuf.
   */
  os << pad1 << "message " << get_proto_enclosing_typename() << std::endl;
  os << pad1 << "{" << std::endl;
  os << pad2 << "option (rw_msgopts) = { suppress:true };" << std::endl;
  os << pad2 << "enum e" << std::endl;
  os << pad2 << "{" << std::endl;
  if (has_aliases_) {
    os << pad3 << "option allow_alias = true;" << std::endl;
  }
  os << pad3 << "option (rw_enumopts) = { c_prefix:\"" << get_proto_c_prefix()
     << "\" ypbc_enum:\"" << get_ypbc_var_enumdesc() << "\" };" << std::endl;
  for (auto const& pbvalue: value_list_) {
    os << pad3 << pbvalue->get_proto_enumerator() << " = "
       << pbvalue->get_int_value() << ";" << std::endl;
  }
  os << pad2 << "}" << std::endl;
  os << pad1 << "}" << std::endl;
}

void PbEnumType::output_h_enum(
  std::ostream& os,
  unsigned indent)
{
  std::string pad1(indent,' ');
  os << pad1 << "extern const rw_yang_pb_enumdesc_t "
     << get_ypbc_var_enumdesc() << ";" << std::endl;
  os << pad1 << "typedef " << get_pbc_typename() << " "
     << get_ypbc_global("t_enum") << ";" << std::endl;
}

void PbEnumType::output_cpp_enum(
  std::ostream& os,
  unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  // Need to generate order mappings in order to do this
  PbEnumValue::enum_value_sorted_t value_by_proto_name;
  PbEnumValue::enum_value_sorted_t value_by_yang_name;
  for (auto const& pbvalue: value_list_) {
    auto status = value_by_proto_name.emplace(pbvalue->get_raw_enumerator(), pbvalue.get());
    RW_ASSERT(status.second);
  }

  // Generate the yang enumerator strings array
  os << pad1 << "static const char* const " << get_ypbc_var_enumstr() << "[] =" << std::endl;
  os << pad1 << "{" << std::endl;
  unsigned i = 0;
  for (auto const& me: value_by_proto_name) {
    PbEnumValue* pbvalue = me.second;
    os << pad2 << "\"" << pbvalue->get_yang_enumerator() << "\"," << std::endl;
    pbvalue->set_index_by_proto_name(i++);
    auto status = value_by_yang_name.emplace(pbvalue->get_yang_enumerator(), pbvalue);
    RW_ASSERT(status.second);
  }
  os << pad1 << "};" << std::endl;

  // Generate the enumerator strings yang-sort-order array
  os << pad1 << "static const uint32_t " << get_ypbc_var_enumsort() << "[] =" << std::endl;
  os << pad1 << "{ " << std::endl;
  os << pad1 << " ";
  for (auto const& me: value_by_yang_name) {
    os << " " << me.second->get_index_by_proto_name() << ",";
  }
  os << std::endl;
  os << "};" << std::endl;

  // Now output the ypbc enum descriptor
  os << pad1 << "const rw_yang_pb_enumdesc_t " << get_ypbc_var_enumdesc() << " =" << std::endl;
  os << pad1 << "{ " << std::endl;
  os << pad2 << "reinterpret_cast<const rw_yang_pb_module_t*>(&"
     << get_pbmod()->get_ypbc_global("g_module") << ")," << std::endl;
  if (ytenum_) {
    os << pad2 << "\"" << ytenum_->get_name() << "\"," << std::endl;
  } else {
    os << pad2 << "\"" << scoped_pbnode_->get_ynode()->get_name() << "\"," << std::endl;
  }
  os << pad2 << "&" << get_pbc_enum_desc() << "," << std::endl;
  os << pad2 << get_ypbc_var_enumstr() << "," << std::endl;
  os << pad2 << get_ypbc_var_enumsort() << "," << std::endl;
  os << pad1 << "};" << std::endl;
  os << std::endl;
}


} // namespace rw_yang

// End of yangpbc_enum.cpp
