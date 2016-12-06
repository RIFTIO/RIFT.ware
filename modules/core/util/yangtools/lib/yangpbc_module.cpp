
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
 * @file yangpbc.cpp
 *
 * Convert yang schema to google protobuf: proto module support
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
#include <algorithm>

namespace rw_yang {

PbModule::PbModule(PbModel* pbmodel, YangModule* ymod)
: pbmodel_(pbmodel),
  ymod_(ymod)
{
  RW_ASSERT(pbmodel_);
  RW_ASSERT(ymod_);
}

PbModel* PbModule::get_pbmodel() const
{
  return pbmodel_;
}

YangModule* PbModule::get_ymod() const
{
  return ymod_;
}

rw_namespace_id_t PbModule::get_nsid() const
{
  return nsid_;
}

PbModule* PbModule::get_local_lci_pbmod()
{
  if (!lci_local_pbmod_) {
    PbModule* lci_pbmod = this;
    lci_pbmod = lci_pbmod->least_common_importer(data_module_pbmsg_->get_lci_pbmod());
    lci_pbmod = lci_pbmod->least_common_importer(rpci_module_pbmsg_->get_lci_pbmod());
    lci_pbmod = lci_pbmod->least_common_importer(rpco_module_pbmsg_->get_lci_pbmod());
    lci_pbmod = lci_pbmod->least_common_importer(notif_module_pbmsg_->get_lci_pbmod());
    lci_pbmod = lci_pbmod->least_common_importer(group_root_pbmsg_->get_lci_pbmod());
    lci_local_pbmod_ = lci_pbmod;
  }
  return lci_local_pbmod_;
}

PbModule* PbModule::get_lci_pbmod()
{
  if (!lci_pbmod_) {
    lci_pbmod_ = get_local_lci_pbmod();
    for (auto pbmod : augment_pbmods_) {
      auto other = pbmod->get_local_lci_pbmod();
      lci_pbmod_ = lci_pbmod_->least_common_importer(other);
    }
  }

  return lci_pbmod_;
}

PbNode* PbModule::get_root_pbnode() const
{
  return root_pbnode_.get();
}

PbMessage* PbModule::get_data_module_pbmsg() const
{
  return data_module_pbmsg_;
}

unsigned PbModule::has_data_module_fields() const
{
  return data_module_pbmsg_->has_fields();
}

unsigned PbModule::has_data_module_messages() const
{
  return data_module_pbmsg_->has_messages();
}

PbMessage* PbModule::get_rpci_module_pbmsg() const
{
  return rpci_module_pbmsg_;
}

PbMessage* PbModule::get_rpco_module_pbmsg() const
{
  return rpco_module_pbmsg_;
}

unsigned PbModule::has_rpc_messages() const
{
  return rpci_module_pbmsg_->has_messages();
}

PbMessage* PbModule::get_notif_module_pbmsg() const
{
  return notif_module_pbmsg_;
}

unsigned PbModule::has_notif_module_messages() const
{
  return notif_module_pbmsg_->has_messages();
}

PbMessage* PbModule::get_group_root_pbmsg() const
{
  return group_root_pbmsg_;
}

unsigned PbModule::has_group_root_messages() const
{
  return group_root_pbmsg_->has_messages();
}

bool PbModule::is_import_needed() const
{
  return need_to_import_;
}

void PbModule::need_to_import()
{
  need_to_import_ = true;
}

bool PbModule::has_top_level() const
{
  /*
     ATTN: TGS: Need to figure this out
  */
  return true;
}

bool PbModule::has_schema() const
{
  return    this == pbmodel_->get_schema_pbmod()
         || has_top_level() // ATTN: ALWAYS TRUE!!
         || has_group_root_messages()
         || has_data_module_fields()
         || has_rpc_messages()
         || has_notif_module_messages();
}

bool PbModule::is_local()
{
  return get_lci_pbmod() == pbmodel_->get_schema_pbmod();
}

const std::string& PbModule::get_proto_package_name() const
{
  RW_ASSERT(proto_package_name_.length());
  return proto_package_name_;
}

std::string PbModule::get_proto_typename()
{
  RW_ASSERT(proto_package_name_.length());
  return PbModel::mangle_typename(proto_package_name_);
}

std::string PbModule::get_proto_fieldname()
{
  return PbModel::mangle_to_underscore(get_proto_package_name());
}

void PbModule::populate_direct_imports()
{
  RW_ASSERT(ymod_);

  // Iterate over the module's imports
  for (YangImportIter yi =  ymod_->import_begin(); yi != ymod_->import_end(); ++yi) {
    YangModule* other_ymod = &(*yi);
    PbModule* other_pbmod = pbmodel_->module_lookup(other_ymod);
    RW_ASSERT(other_pbmod);

    // Ignore return status - populate_direct_imports() can be called multiple times per module
    direct_imports_.emplace(other_pbmod);
  }
}

void PbModule::populate_transitive_imports(
  PbModule* other_pbmod)
{
  RW_ASSERT(other_pbmod);

  if (other_pbmod != this) {
    auto status = transitive_imports_.emplace(other_pbmod);
    RW_ASSERT(status.second); // should not be a duplicate

    status = other_pbmod->transitive_importers_.emplace(this);
    RW_ASSERT(status.second); // should not be a duplicate
  }

  // Iterate over the other module's imports
  for (auto const& pbmi: other_pbmod->direct_imports_) {
    PbModule* transitive_pbmod = &*pbmi;
    RW_ASSERT(transitive_pbmod);

    if (   this != transitive_pbmod
        && transitive_imports_.find(transitive_pbmod) == transitive_imports_.end()) {
      populate_transitive_imports(transitive_pbmod);
    }
  }
}

PbModule* PbModule::least_common_importer(PbModule* other)
{
  RW_ASSERT(other);

  // Same?
  if (this == other) {
    return this;
  }

  // other is an import of this?
  auto mi = transitive_imports_.find(other);
  if (mi != transitive_imports_.end()) {
    return this;
  }

  // this is an import of other?
  if (other->imports_transitive(this)) {
    return other;
  }

  // Iterate over all my transitive importers looking for candidate LCIs
  module_set_t candidates;
  for (auto importer: transitive_importers_) {
    if (importer->transitive_imports_.find(other) != transitive_imports_.end()) {
      candidates.emplace(importer);
    }
  }

  /*
   * Need to whittle the list of candidates down to a best choice.  The
   * best choice is a candidate that is not imported by any other
   * candidate.  The answer is not necessarily unique - there can be
   * multiple LCIs.  But once the extras have been eliminated, we can
   * just pick any of the ones that remain.
   */
  auto ai = candidates.begin();
  auto bi = candidates.begin();
  while (candidates.size() > 1) {
    if (ai == bi) {
      ++bi;
    }

    if (bi == candidates.end()) {
      ++ai;
      bi = candidates.begin();
      continue;
    }

    if (ai == candidates.end()) {
      break;
    }

    PbModule* moda = *ai;
    PbModule* modb = *bi;
    mi = moda->transitive_imports_.find(modb);
    if (mi != moda->transitive_imports_.end()) {
      // moda is not the LCI because it imports modb
      ai = candidates.erase(ai);
      bi = candidates.begin();
      continue;
    }

    mi = modb->transitive_imports_.find(moda);
    if (mi != modb->transitive_imports_.end()) {
      // modb is not the LCI because it imports moda
      bi = candidates.erase(bi);
      continue;
    }
    ++bi;
  }

  RW_ASSERT(candidates.size()); // there must be a common importer
  return *candidates.begin();
}

bool PbModule::imports_direct(PbModule* pbmod)
{
  RW_ASSERT(pbmod);
  return direct_imports_.find(pbmod) != direct_imports_.end();
}

bool PbModule::imports_transitive(PbModule* pbmod)
{
  RW_ASSERT(pbmod);
  return transitive_imports_.find(pbmod) != transitive_imports_.end();
}

bool PbModule::equals_or_imports_direct(PbModule* pbmod)
{
  RW_ASSERT(pbmod);
  return    this == pbmod
         || direct_imports_.find(pbmod) != direct_imports_.end();
}

bool PbModule::equals_or_imports_transitive(PbModule* pbmod)
{
  RW_ASSERT(pbmod);
  return    this == pbmod
         || transitive_imports_.find(pbmod) != transitive_imports_.end();
}

bool PbModule::is_ordered_before(
  PbModule* other_pbmod,
  PbModule* schema_pbmod)
{
  RW_ASSERT(other_pbmod);

  /*
   * This function is effectively "less-than", as viewed through the
   * import dependencies.
   */
  if (this == other_pbmod) {
    // Equal is not before.
    return false;
  }

  if (this->imports_transitive(other_pbmod)) {
    // This module imports the other module.  Therefore, the other must be first.
    return false;
  }

  if (other_pbmod->imports_transitive(this)) {
    // The other module imports this module.  Therefore, the this must be first.
    return true;
  }

  if (schema_pbmod) {
    if (!schema_pbmod->equals_or_imports_transitive(this)) {
      // This module is not visible in the given schema.  Don't consider it ordered before.
      return false;
    }

    if (!schema_pbmod->equals_or_imports_transitive(other_pbmod)) {
      // The other module is not visible in the given schema.  Consider this module first.
      return true;
    }
  }

  // The modules are unrelated - use the ordering in the global schema
  if (get_pbmodel()->module_is_ordered_before(this, other_pbmod)) {
    return true;
  }
  return false;
}

std::string PbModule::get_ypbc_global(
  const char* discriminator)
{
  if (0 == ypbc_global_base_.length()) {
    ypbc_global_base_ = get_lci_pbmod()->get_proto_typename() + "_" + get_proto_typename();
  }
  if (discriminator) {
    return std::string("rw_ypbc_") + ypbc_global_base_ + "_" + discriminator;
  }
  return std::string("rw_ypbc_") + ypbc_global_base_;
}

void PbModule::add_augment_pbmod(PbModule* other)
{
  augment_pbmods_.insert(other);
}

void PbModule::find_augmented_nodes()
{
  // Iterate over the augments, determining which ones have protobuf conversions
  RW_ASSERT(ymod_);
  for (YangAugmentIter ai = ymod_->augment_begin(); ai != ymod_->augment_end(); ++ai) {
    YangNode* ynode = ai->get_target_node();
    switch (ynode->get_stmt_type()) {
      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
        /*
         * The choice/case nodes are treated as if the augment target
         * is the enclosing node.  N.B.: the enclosing node MAY BE THE
         * ROOT!
         */
        ynode = ynode->get_parent();
        break;
      default:
        break;
    }

    if (ynode) {
      PbNode* pbnode = pbmodel_->lookup_node(ynode);
      if (!pbnode) {
        pbmodel_->populate_augment_root(ynode, &pbnode);
      }
      RW_ASSERT(pbnode);
      pbnode->add_augment_mod(this);
      pbnode->get_schema_defining_pbmod()->add_augment_pbmod(this);
    }
  }
}

void PbModule::enum_add(
  enum_type_uptr_t pbenum)
{
  RW_ASSERT(pbenum);
  enum_type_list_.emplace_back(std::move(pbenum));
}

void PbModule::find_equivalent_enums()
{
  if (0 == enum_type_list_.size()) {
    return;
  }

  // Save all enums by typename.
  PbEnumType::enum_typename_map_t enum_types_list;
  for (auto const& pbenum: enum_type_list_) {
    enum_types_list.emplace(pbenum->get_proto_enclosing_typename(), pbenum.get());
  }

  // Ensure that all enums (whether typedefed or untyped ones have unique typenames, or are equivalent).
  PbEnumType* set_first_pbenum = nullptr;
  auto ei = enum_types_list.begin();
  for (auto nei = ei; ei != enum_types_list.end(); ei = nei) {
    nei = ei;
    ++nei;
    PbEnumType* pbenum = ei->second;
    if (set_first_pbenum) {
      if (set_first_pbenum->get_proto_enclosing_typename() == pbenum->get_proto_enclosing_typename()) {
        if (!set_first_pbenum->is_equivalent(pbenum)) {
          std::ostringstream oss;
          oss << "enum type " << set_first_pbenum->get_proto_enclosing_typename()
              << " already defined, original at " << set_first_pbenum->get_location();
          get_pbmodel()->error( pbenum->get_location(), oss.str() );
        } else {
          pbenum->set_equivalent_pbenum(set_first_pbenum);
        }
        enum_types_list.erase(ei);
        continue;
      }
    }
    set_first_pbenum = pbenum;
  }
}

void PbModule::save_top_level_grouping(
  PbNode* pbnode,
  PbMessage* pbmsg)
{
  RW_ASSERT(pbnode);
  RW_ASSERT(pbmsg);

  PbEqMsgSet* pbeq = pbmsg->get_pbeq();
  RW_ASSERT(pbeq);
  ordered_grouping_pbeqs_.emplace_back(pbeq);
}

void PbModule::find_grouping_order()
{
  PbEqMsgSet::order_dependencies(&ordered_grouping_pbeqs_);
}

void PbModule::parse_start()
{
  if (proto_package_name_.size()) {
    // Already parsed
    return;
  }

  RW_ASSERT(ymod_);
  nsid_ = pbmodel_->get_ns_mgr().get_ns_hash(ymod_->get_ns());

  proto_package_name_ = PbModel::mangle_identifier(ymod_->get_name());

  RW_ASSERT(pbmodel_);
  YangModel* ymodel = pbmodel_->get_ymodel();
  RW_ASSERT(ymodel);
  YangNode* yroot = ymodel->get_root_node();
  RW_ASSERT(yroot);
  root_pbnode_.reset(new PbNode(this, yroot));

  data_module_pbmsg_ = pbmodel_->alloc_pbmsg(root_pbnode_.get(), nullptr, PbMsgType::data_module);
  rpci_module_pbmsg_ = pbmodel_->alloc_pbmsg(root_pbnode_.get(), nullptr, PbMsgType::rpci_module);
  rpco_module_pbmsg_ = pbmodel_->alloc_pbmsg(root_pbnode_.get(), nullptr, PbMsgType::rpco_module);
  notif_module_pbmsg_ = pbmodel_->alloc_pbmsg(root_pbnode_.get(), nullptr, PbMsgType::notif_module);
  group_root_pbmsg_ = pbmodel_->alloc_pbmsg(root_pbnode_.get(), nullptr, PbMsgType::group_root);

  for (YangExtensionIter xi = ymod_->extension_begin();
       xi != ymod_->extension_end();
       ++xi) {
    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_PACKAGE_NAME)) {
      if (yext_package_name_) {
        std::ostringstream oss;
        oss << "Ignoring extra " << xi->get_name()
            << " extension on module " << ymod_->get_name()
            << ", original at " << yext_package_name_->get_location();
        pbmodel_->warning( xi->get_location(), oss.str() );
        continue;
      }
      if (!PbModel::is_identifier(xi->get_value())) {
        std::ostringstream oss;
        oss << "Extension " << xi->get_name()
            << " on module " << ymod_->get_name()
            << " must be valid C identifier: " << xi->get_value();
        pbmodel_->error( xi->get_location(), oss.str() );
        continue;
      }
      yext_package_name_ = &*xi;
      proto_package_name_ = yext_package_name_->get_value();
      continue;
    }

    if (xi->is_match(RW_YANG_PB_MODULE, RW_YANG_PB_EXT_FILE_PBC_INCLUDE)) {
      const char *value = xi->get_value();
      RW_ASSERT(value);
      // ATTN: Perform any validations for the path string.
      pbc_inc_files_.push_back(value);
      continue;
    }
  }

  if (ymod_->was_loaded_explicitly()) {
    pbmodel_->set_schema(this);
  }

  // Find all typedef'ed enums
  for (auto ytei = ymod_->typed_enum_begin(); ytei != ymod_->typed_enum_end(); ++ytei) {
    PbEnumType* pbenum = pbmodel_->enum_get(&*ytei, this);
    RW_ASSERT(pbenum);
    pbenum->parse();
  }

  root_pbnode_->populate_module_root();
}

void PbModule::parse_nodes()
{
  root_pbnode_->parse_root();
}

void PbModule::parse_messages()
{
  data_module_pbmsg_->parse();
  rpci_module_pbmsg_->parse();
  rpco_module_pbmsg_->parse();
  notif_module_pbmsg_->parse();
  group_root_pbmsg_->parse();
}

void PbModule::find_schema_imports()
{
  if (   !has_data_module_fields()
      && !has_rpc_messages()
      && !has_notif_module_messages()) {
    return;
  }

  if (has_data_module_fields()) {
    data_module_pbmsg_->find_schema_imports();
  }

  if (has_rpc_messages()) {
    rpci_module_pbmsg_->find_schema_imports();
    rpco_module_pbmsg_->find_schema_imports();
  }

  if (has_notif_module_messages()) {
    notif_module_pbmsg_->find_schema_imports();
  }

  if (has_group_root_messages()) {
    group_root_pbmsg_->find_schema_imports();
  }

/*
  PbModule* schema_pbmod = get_pbmodel()->get_schema_pbmod();
  if (this != schema_pbmod) {
    need_to_import();
  }

  if (data_module_pbmsg_->get_lci_pbmod() == schema_pbmod) {
    data_module_pbmsg_->find_schema_imports();
  }

  if (rpci_module_pbmsg_->get_lci_pbmod() == schema_pbmod) {
    rpci_module_pbmsg_->find_schema_imports();
  }

  if (rpco_module_pbmsg_->get_lci_pbmod() == schema_pbmod) {
    rpco_module_pbmsg_->find_schema_imports();
  }

  if (notif_module_pbmsg_->get_lci_pbmod() == schema_pbmod) {
    notif_module_pbmsg_->find_schema_imports();
  }

  if (group_root_pbmsg_->get_lci_pbmod() == schema_pbmod) {
    group_root_pbmsg_->find_schema_imports();
  }
 */
}

void PbModule::output_proto_file_options(
  std::ostream& os,
  unsigned indent)
{
  std::string pad1(indent,' ');

  os << pad1 << "option (rw_fileopts) = {\n";
  os << pad1 << "  gen_gi:true\n";// Generate Gi bindings.
  os << pad1 << "  c_include:\"rw_schema.pb-c.h\"\n";
  for (auto const& inc_file: pbc_inc_files_) {
    os << pad1 << "  c_include:\"" << inc_file << "\"\n";
  }
  os << pad1 << "};\n";
}

void PbModule::output_proto_enums(
  std::ostream& os,
  unsigned indent)
{
  if (0 == enum_type_list_.size()) {
    return;
  }

  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  /*
   * The enums are emitted in their own top-level scope, in order to
   * avoid name contamination with other messages, and also to make it
   * easy to find them (with a well-known top-level package symbol).
   * The presumed uniqueness of the global top-level symbol also
   * prevents ambiguities that might be caused by lower-level message
   * definitions that might share the same message type name as the
   * enum type name.
   */

  os << pad1 << "message YangEnum" << std::endl;
  os << pad1 << "{" << std::endl;
  os << pad2 << "option (rw_msgopts) = { suppress:true }; " << std::endl;
  bool first = false;
  for (auto const& pbenum: enum_type_list_) {
    if (!first) {
      os << std::endl;
      first = false;
    }
    if (pbenum->is_suppressed()) {
      continue;
    }
    pbenum->output_proto_enum(os,indent+2);
  }
  os << pad1 << "}" << std::endl;
  os << std::endl;
}

void PbModule::output_proto_data_module_as_field(
    std::ostream& os,
    unsigned indent)
{
  std::string pad(indent,' ');

  if (has_data_module_fields()) {
    os << pad << "optional ";
    std::string proto_mpath = data_module_pbmsg_->get_proto_message_path();
    // Replace the package_name in the front with the schema package name
    std::size_t dot = proto_mpath.find('.', 1);
    RW_ASSERT(dot != std::string::npos);
    proto_mpath.replace(0, dot, pbmodel_->get_schema_pbmod()->get_proto_package_name());

    os << proto_mpath << " " << get_proto_fieldname()
        << " = " << nsid_ << ";" << std::endl;
  }
}

void PbModule::output_proto_rpci_module_as_field(
    std::ostream& os,
    unsigned indent)
{
  std::string pad(indent,' ');

  if (has_rpc_messages()) {
    os << pad << "optional ";
    std::string proto_mpath = rpci_module_pbmsg_->get_proto_message_path();
    // Replace the package_name in the front with the schema package name
    std::size_t dot = proto_mpath.find('.', 1);
    RW_ASSERT(dot != std::string::npos);
    proto_mpath.replace(0, dot, pbmodel_->get_schema_pbmod()->get_proto_package_name());

    os << proto_mpath << " " << get_proto_fieldname()
        << " = " << nsid_ << ";" << std::endl;
  }
}

void PbModule::output_proto_rpco_module_as_field(
    std::ostream& os,
    unsigned indent)
{
  std::string pad(indent,' ');

  if (has_rpc_messages()) {
    os << pad << "optional ";
    std::string proto_mpath = rpco_module_pbmsg_->get_proto_message_path();
    // Replace the package_name in the front with the schema package name
    std::size_t dot = proto_mpath.find('.', 1);
    RW_ASSERT(dot != std::string::npos);
    proto_mpath.replace(0, dot, pbmodel_->get_schema_pbmod()->get_proto_package_name());

    os << proto_mpath << " " << get_proto_fieldname()
        << " = " << nsid_ << ";" << std::endl;
  }
}

void PbModule::output_proto_notif_module_as_field(
    std::ostream& os,
    unsigned indent)
{
  std::string pad(indent,' ');

  if (has_notif_module_messages()) {
    os << pad << "optional ";
    std::string proto_mpath = notif_module_pbmsg_->get_proto_message_path();
    // Replace the package_name in the front with the schema package name
    std::size_t dot = proto_mpath.find('.', 1);
    RW_ASSERT(dot != std::string::npos);
    proto_mpath.replace(0, dot, pbmodel_->get_schema_pbmod()->get_proto_package_name());

    os << proto_mpath << " " << get_proto_fieldname()
        << " = " << nsid_ << ";" << std::endl;
  }
}

void PbModule::output_proto_data_module(
  std::ostream& os,
  unsigned indent)
{
  if (has_data_module_fields()) {
    data_module_pbmsg_->output_proto_message(PbMessage::output_t::message, os, indent);
  }
}

void PbModule::output_proto_rpci_module(
  std::ostream& os,
  unsigned indent)
{
  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_proto_message(PbMessage::output_t::message, os, indent);
  }
}

void PbModule::output_proto_rpco_module(
  std::ostream& os,
  unsigned indent)
{
  if (has_rpc_messages()) {
    rpco_module_pbmsg_->output_proto_message(PbMessage::output_t::message, os, indent);
  }
}

void PbModule::output_proto_notif_module(
  std::ostream& os,
  unsigned indent)
{
  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_proto_message(PbMessage::output_t::message, os, indent);
  }
}

void PbModule::output_proto_group_root(
  std::ostream& os,
  unsigned indent)
{
  if (0 == ordered_grouping_pbeqs_.size()) {
    return;
  }

  std::string pad1(indent,' ');

  /*
   * The groupings are emitted in their own top-level scope, in order to
   * avoid name contamination with other messages, and also to make it
   * easy to find them (with a well-known top-level package symbol).
   * The presumed uniqueness of the global top-level symbol also
   * prevents ambiguities that might be caused by lower-level message
   * definitions that might share the same message type name as the
   * enum type name.
   */

  os << pad1 << "message YangGroup" << std::endl;
  os << pad1 << "{" << std::endl;
  bool first = false;
  for (auto const& pbeq: ordered_grouping_pbeqs_) {
    PbMessage* pbmsg = pbeq->get_preferred_msg(this);
    if (!first) {
      os << std::endl;
      first = false;
    }
    pbmsg->output_proto_message(PbMessage::output_t::message, os, indent+2);
  }
  os << pad1 << "}" << std::endl;
  os << std::endl;
}

void PbModule::output_proto_schema(
  PbMessage::output_t output_style,
  std::ostream& os,
  unsigned indent)
{
  if (!has_schema()) {
    return;
  }

  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  std::string pad3(indent+4,' ');

  os << pad1 << "message " << get_proto_typename() << " /* "
     << ymod_->get_ns() << " */" << std::endl;
  os << pad1 << "{" << std::endl;
  os << pad2 << "option (rw_msgopts) = { suppress:true }; " << std::endl;

  if (has_group_root_messages()) {
    os << std::endl;
    group_root_pbmsg_->output_proto_message(output_style, os, indent+2);
  }

  if (has_data_module_fields()) {
    os << std::endl;
    data_module_pbmsg_->output_proto_message(output_style, os, indent+2);
  }

  if (has_rpc_messages()) {
    /*
       ATTN: TGS: This requires special handling: should not output RPC
       root here: should separate input from output
     */
    os << std::endl;
    rpci_module_pbmsg_->output_proto_message(output_style, os, indent+2);
    rpco_module_pbmsg_->output_proto_message(output_style, os, indent+2);
  }

  if (has_notif_module_messages()) {
    os << std::endl;
    notif_module_pbmsg_->output_proto_message(output_style, os, indent+2);
  }

  os << pad1 << "} /* " << get_proto_typename() << " */" << std::endl;
}

void PbModule::output_h_enums(
  std::ostream& os,
  unsigned indent)
{
  if (0 == enum_type_list_.size()) {
    return;
  }

  for (auto const& pbenum: enum_type_list_) {
    if (pbenum->is_suppressed()) {
      continue;
    }
    pbenum->output_h_enum(os,indent);
  }
  os << std::endl;
}

void PbModule::output_h_struct_tags(
  std::ostream& os,
  unsigned indent)
{
  if (has_data_module_fields()) {
    data_module_pbmsg_->output_h_struct_tags(os, indent);
  }
  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_h_struct_tags(os, indent);
    rpco_module_pbmsg_->output_h_struct_tags(os, indent);
  }
  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_h_struct_tags(os, indent);
  }
  if (has_group_root_messages()) {
    group_root_pbmsg_->output_h_struct_tags(os, indent);
  }

  if (!is_local() || !has_schema()) {
    return;
  }

  std::string pad1(indent,' ');
  os << pad1 << "typedef struct " << get_ypbc_global("t_module") << " "
     << get_ypbc_global("t_module") << ";" << std::endl;
  os << std::endl;
}

void PbModule::output_h_structs(
  std::ostream& os,
  unsigned indent)
{
  if (has_data_module_fields()) {
    data_module_pbmsg_->output_h_structs(os, indent);
  }
  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_h_structs(os, indent);
    rpco_module_pbmsg_->output_h_structs(os, indent);
  }
  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_h_structs(os, indent);
  }
  if (has_group_root_messages()) {
    group_root_pbmsg_->output_h_structs(os, indent);
  }

  if (!is_local() || !has_schema()) {
    return;
  }

  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  os << pad1 << "struct " << get_ypbc_global("t_module") << " {" << std::endl;
  os << pad2 << "const " << pbmodel_->get_ypbc_global("t_schema") << "* schema;" << std::endl;
  os << pad2 << "const char* module_name;" << std::endl;
  os << pad2 << "const char* revision;" << std::endl;
  os << pad2 << "const char* ns;" << std::endl;
  os << pad2 << "const char* prefix;" << std::endl;

  if (has_data_module_fields()) {
    os << pad2 << "const " << data_module_pbmsg_->get_ypbc_global("t_msg_msgdesc") << "* data_module;" << std::endl;
  } else {
    os << pad2 << "const void* data_module;" << std::endl;
  }

  if (has_group_root_messages()) {
    os << pad2 << "const " << group_root_pbmsg_->get_ypbc_global("t_group_root") << "* group_root;" << std::endl;
  } else {
    os << pad2 << "const void* group_root;" << std::endl;
  }

  if (has_rpc_messages()) {
    os << pad2 << "const " << rpci_module_pbmsg_->get_ypbc_global("t_msg_msgdesc") << "* rpci_module;" << std::endl;
    os << pad2 << "const " << rpco_module_pbmsg_->get_ypbc_global("t_msg_msgdesc") << "* rpco_module;" << std::endl;
  } else {
    os << pad2 << "const void* rpci_module;" << std::endl;
    os << pad2 << "const void* rpco_module;" << std::endl;
  }

  if (has_notif_module_messages()) {
    os << pad2 << "const " << notif_module_pbmsg_->get_ypbc_global("t_msg_msgdesc") << "* notif_module;" << std::endl;
  } else {
    os << pad2 << "const void* notif_module;" << std::endl;
  }

  os << pad1 << "};" << std::endl;
  os << pad1 << "extern const " << get_ypbc_global("t_module") << " "
     << get_ypbc_global("g_module") << ";" << std::endl;
  if (this == pbmodel_->get_schema_pbmod()) {
    os << pad1 << "#define rw_ypbc_" << get_proto_typename() << "() rw_ypbc_"
       << get_proto_typename() << "_" << get_proto_typename() << std::endl;
  }

  os << std::endl;
}

void PbModule::output_h_meta_data_macros(
    std::ostream& os,
    unsigned indent)
{
  if (has_data_module_fields()) {
    data_module_pbmsg_->output_h_meta_data_macro(os, indent);
  }
  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_h_meta_data_macro(os, indent);
    rpco_module_pbmsg_->output_h_meta_data_macro(os, indent);
  }
  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_h_meta_data_macro(os, indent);
  }
}

void PbModule::output_cpp_enums(
  std::ostream& os,
  unsigned indent)
{
  if (0 == enum_type_list_.size()) {
    return;
  }

  for (auto const& pbenum: enum_type_list_) {
    if (pbenum->is_suppressed()) {
      continue;
    }
    pbenum->output_cpp_enum(os,indent);
  }
  os << std::endl;
}

void PbModule::output_cpp_descs(
  std::ostream& os,
  unsigned indent)
{
  if (!is_local() || !has_schema()) {
    return;
  }

  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  if (has_data_module_fields()) {
    data_module_pbmsg_->output_cpp_msgdesc_define(os, indent);
  }
  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_cpp_msgdesc_define(os, indent);
    rpco_module_pbmsg_->output_cpp_msgdesc_define(os, indent);
  }
  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_cpp_msgdesc_define(os, indent);
  }
  if (has_group_root_messages()) {
    group_root_pbmsg_->output_cpp_msgdesc_define(os, indent);
  }

  os << pad1 << "const " << get_ypbc_global("t_module") << " "
     << get_ypbc_global("g_module") << " = {" << std::endl;

  os << pad2 << "&" << pbmodel_->get_ypbc_global("g_schema") << "," << std::endl;
  os << pad2 << "\"" << ymod_->get_name() << "\"," << std::endl;
  os << pad2 << "\"\"," << std::endl; // ATTN: revision not yet supported.
  os << pad2 << "\"" << ymod_->get_ns() << "\"," << std::endl;
  os << pad2 << "\"" << ymod_->get_prefix() << "\"," << std::endl;

  if (has_data_module_messages()) {
    os << pad2 << "&" << data_module_pbmsg_->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
  } else {
    os << pad2 << "nullptr," << std::endl;
  }

  if (has_group_root_messages()) {
    os << pad2 << "&" << group_root_pbmsg_->get_ypbc_global("g_group_root") << "," << std::endl;
  } else {
    os << pad2 << "nullptr," << std::endl;
  }

  if (has_rpc_messages()) {
    os << pad2 << "&" << rpci_module_pbmsg_->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
    os << pad2 << "&" << rpco_module_pbmsg_->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
  } else {
    os << pad2 << "nullptr," << std::endl;
    os << pad2 << "nullptr," << std::endl;
  }

  if (has_notif_module_messages()) {
    os << pad2 << "&" << notif_module_pbmsg_->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
  } else {
    os << pad2 << "nullptr," << std::endl;
  }

  os << pad1 << "};" << std::endl;
  os << std::endl;
}

void PbModule::output_gi_h(
  std::ostream& os,
  unsigned indent)
{
  if (!is_local() || !has_schema()) {
    return;
  }

  if (has_data_module_messages()) {
    data_module_pbmsg_->output_gi_h_decls(os, indent);
    os << std::endl;
  }

  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_gi_h_decls(os, indent);
    os << std::endl;
    rpco_module_pbmsg_->output_gi_h_decls(os, indent);
    os << std::endl;
  }

  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_gi_h_decls(os, indent);
    os << std::endl;
  }
}

void PbModule::output_gi_c(
  std::ostream& os,
  unsigned indent)
{
  if (!is_local() || !has_schema()) {
    return;
  }

  if (has_data_module_messages()) {
    data_module_pbmsg_->output_gi_c_funcs(os, indent);
    os << std::endl;
  }

  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_gi_c_funcs(os, indent);
    os << std::endl;
    rpco_module_pbmsg_->output_gi_c_funcs(os, indent);
    os << std::endl;
  }

  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_gi_c_funcs(os, indent);
    os << std::endl;
  }
}

bool PbModule::traverse(PbModelVisitor* visitor)
{
  if (has_data_module_messages()) {
    if(!visitor->visit_message_enter(data_module_pbmsg_)) {
      return false;
    }
    if (!data_module_pbmsg_->traverse(visitor)) {
      return false;
    }
    if (!visitor->visit_message_exit(data_module_pbmsg_)) {
      return false;
    }
  }
  if (has_rpc_messages()) {
    if(!visitor->visit_message_enter(rpci_module_pbmsg_)) {
      return false;
    }
    if (!rpci_module_pbmsg_->traverse(visitor)) {
      return false;
    }
    if (!visitor->visit_message_exit(rpci_module_pbmsg_)) {
      return false;
    }
    if(!visitor->visit_message_enter(rpco_module_pbmsg_)) {
      return false;
    }
    if (!rpco_module_pbmsg_->traverse(visitor)) {
      return false;
    }
    if (!visitor->visit_message_exit(rpco_module_pbmsg_)) {
      return false;
    }
  }
  if (has_notif_module_messages()) {
    if(!visitor->visit_message_enter(notif_module_pbmsg_)) {
      return false;
    }
    if (!notif_module_pbmsg_->traverse(visitor)) {
      return false;
    }
    if (!visitor->visit_message_exit(notif_module_pbmsg_)) {
      return false;
    }
  }
  return true;
}

bool PbModule::output_doc(
  const doc_file_t file_type,
  std::ostream& os,
  unsigned indent,
  PbMessage::doc_t doc_style,
  unsigned chapter )
{
  if (!is_local() || !has_schema()) {
    return false;
  }

  std::string str_chapter = std::to_string(chapter);
  pbmodel_->output_doc_heading(file_type, os, indent, doc_style,
    str_chapter, std::string("module ") + get_ymod()->get_name() );

  switch (doc_style) {
    case PbMessage::doc_t::api_entry:
    case PbMessage::doc_t::user_entry:
      os << std::endl;
      break;

    default:
      break;
  }

  str_chapter.push_back('.');
  unsigned sub_chapter = 1;

  if (has_data_module_messages()) {
    data_module_pbmsg_->output_doc_message(
      file_type, os, indent, doc_style,
      str_chapter + std::to_string(sub_chapter++) );
  }

  if (has_rpc_messages()) {
    rpci_module_pbmsg_->output_doc_message(
      file_type, os, indent, doc_style,
      str_chapter + std::to_string(sub_chapter++) );
    rpco_module_pbmsg_->output_doc_message(
      file_type, os, indent, doc_style,
      str_chapter + std::to_string(sub_chapter++) );
  }

  if (has_notif_module_messages()) {
    notif_module_pbmsg_->output_doc_message(
      file_type, os, indent, doc_style,
      str_chapter + std::to_string(sub_chapter++) );
  }

  return true;
}


} // namespace rw_yang

// End of yangpbc_module.cpp
