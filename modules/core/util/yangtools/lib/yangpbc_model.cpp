
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file yangpbc_model.cpp
 *
 * Convert yang schema to google protobuf: model global support, proto file generation
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

std::string PbModel::mangle_identifier(const char* id)
{
  return YangModel::mangle_to_c_identifier(id);
}

std::string PbModel::mangle_typename(const char* id)
{
  return YangModel::mangle_to_camel_case(id);
}

std::string PbModel::mangle_to_underscore(const char* id)
{
  return YangModel::mangle_camel_case_to_underscore(id);
}

std::string PbModel::mangle_to_uppercase(const char* id)
{
  std::string retval(id);
  std::transform(retval.begin(), retval.end(), retval.begin(), ::toupper);
  return retval;
}

std::string PbModel::mangle_to_lowercase(const char* id)
{
  std::string retval(id);
  std::transform(retval.begin(), retval.end(), retval.begin(), ::tolower);
  return retval;
}

bool PbModel::is_identifier(const char* id)
{
  return YangModel::mangle_is_c_identifier(id);
}

bool PbModel::is_proto_typename(const char* id)
{
  return YangModel::mangle_is_camel_case(id);
}

bool PbModel::is_proto_fieldname(const char* id)
{
  return YangModel::mangle_is_lower_case(id);
}

void PbModel::init_tables()
{
  PbNode::init_tables();
  PbMessage::init_tables();
  PbField::init_tables();
}

PbModel::PbModel(YangModel* ymodel)
    : ymodel_(ymodel),
      ns_mgr_(NamespaceManager::get_global()),
      pbeqmgr_(this)
{
  RW_ASSERT(ymodel_);
  static std::once_flag init_flag;
  std::call_once(init_flag,PbModel::init_tables);

  bool okay = ymodel->app_data_get_token("yangpbc.cpp","pbnode", &pbnode_adt_);
  RW_ASSERT(okay);
}

YangModel* PbModel::get_ymodel() const
{
  return ymodel_;
}

PbModule* PbModel::get_schema_pbmod() const
{
  return schema_pbmod_;
}

NamespaceManager& PbModel::get_ns_mgr() const
{
  return ns_mgr_;
}

const PbModel::adt_pbnode_t PbModel::get_pbnode_adt() const
{
  return pbnode_adt_;
}

PbEqMgr* PbModel::get_pbeqmgr()
{
  return &pbeqmgr_;
}

const std::string& PbModel::get_proto_schema_package_name() const
{
  return proto_schema_package_name_;
}

unsigned PbModel::get_errors() const
{
  return errors_;
}

unsigned PbModel::get_warnings() const
{
  return warnings_;
}

bool PbModel::is_failed() const
{
  return failed_;
}

PbModel::module_citer_t PbModel::modules_begin() const
{
  return modules_by_dependencies_.cbegin();
}

PbModel::module_citer_t PbModel::modules_end() const
{
  return modules_by_dependencies_.cend();
}

std::string PbModel::get_gi_c_identifier_base()
{
  return std::string("rw_ypbc_gi_") + schema_pbmod_->get_proto_typename();
}

std::string PbModel::get_gi_c_identifier(
    const char* operation)
{
  if (!operation) {
    return get_gi_c_identifier_base();
  }
  return get_gi_c_identifier_base() + "_" + operation;
}

std::string PbModel::get_gi_c_global_func_name(
    const char* operation)
{
  RW_ASSERT(operation);
  return
      std::string("rw_ypbc_gi__")
      + YangModel::mangle_camel_case_to_underscore(
          schema_pbmod_->get_proto_typename().c_str())
      + "_" + operation;
}

void PbModel::set_schema(PbModule* pbmod)
{
  RW_ASSERT(!schema_pbmod_); // can only set the schema once!
  schema_pbmod_ = pbmod;

  proto_schema_package_name_ = pbmod->get_proto_package_name();
  proto_module_typename_ = pbmod->get_proto_typename();

  // ATTN: Deprecated in favor of schemaconst, but still used in rwutcli
  msglist_var_ = get_ypbc_global("g_msglist");
}

PbModule* PbModel::module_load(const char* yang_filename)
{
  RW_ASSERT(yang_filename);
  failed_ = false;
  errors_ = 0;
  warnings_ = 0;

  // Load the module.
  std::string module_name = YangModel::filename_to_module(yang_filename);
  YangModule* loaded_ymod = ymodel_->load_module(module_name.c_str());
  if (nullptr == loaded_ymod) {
    std::cout << "Unable to load module '" << module_name << "'" << std::endl;
    return nullptr;
  }
  RW_ASSERT(loaded_ymod->was_loaded_explicitly()); // Didn't load explicitly?  Huh?
  if (module_name != loaded_ymod->get_name()) {
    std::ostringstream oss;
    oss << "Yang requires file basename to equal module name: '"
        << module_name << "' != '"
        << loaded_ymod->get_name() << "'";
    error(loaded_ymod->get_location(), oss.str());
  }

  std::set<std::string> skip;
  skip.emplace("http://netconfcentral.org/ns/yuma-app-common");
  skip.emplace("http://netconfcentral.org/ns/yuma-ncx");
  skip.emplace("http://netconfcentral.org/ns/yuma-types");
  skip.emplace("");

  // Insert all the imported modules
  for (auto ymi = ymodel_->module_begin(); ymi != ymodel_->module_end(); ++ymi) {
    YangModule* ymod = &*ymi;
    auto skip_it = skip.find(ymod->get_ns());
    if (skip_it != skip.end()) {
      continue;
    }
    if (nullptr == module_lookup(ymod)) {
      PbModule* pbmod = new PbModule(this, ymod);
      modules_.emplace_back(pbmod);

      auto status2 = modules_by_ymod_.emplace(ymod, pbmod);
      RW_ASSERT(status2.second); // should have verified uniqueuness before calling new

      auto other_pbmod = modules_by_ns_.add_symbol(ymod->get_ns(), pbmod);
      RW_ASSERT(nullptr == other_pbmod); // the yang model should have failed to load
    }
  }

  PbModule* loaded_pbmod = module_lookup(loaded_ymod);
  RW_ASSERT(loaded_pbmod); // should have just loaded it

  // Parse the loaded module first, in order to properly set the schema
  // root names.
  loaded_pbmod->parse_start();

  // Parse the module-level statements
  for (const auto& pbmod: modules_) {
    pbmod->parse_start();
  }

  // Populate all the direct module imports (from yang)
  for (const auto& pbmod: modules_) {
    pbmod->populate_direct_imports();
  }

  // Populate all the transitive module imports (imports of imports of imports...)
  for (const auto& pbmod: modules_) {
    pbmod->populate_transitive_imports(pbmod.get());
  }

  // Populate all the module dependencies
  PbModule::module_set_t needs_depends;
  for (const auto& pbmod: modules_) {
    // Add to the module set to consider
    needs_depends.emplace(pbmod.get());
  }

  for (const auto& other_pbmod: modules_by_dependencies_) {
    // Remove from the module set to consider - already calculated dependency order
    needs_depends.erase(other_pbmod);
  }

  if (needs_depends.size()) {
    /*
     * Need to check each module, one by one, by looking for modules
     * that do not depend on any of the remaining modules.
     */
    auto ai = needs_depends.begin();
    while (needs_depends.size()) {
      PbModule* moda = *ai;

      auto bi = needs_depends.begin();
      for (; bi != needs_depends.end(); ++bi) {
        if (ai == bi) {
          continue;
        }
        if (moda->imports_transitive(*bi)) {
          // moda depends on modb, so skip to the next ai.
          break;
        }
      }

      if (bi == needs_depends.end()) {
        modules_by_dependencies_.emplace_back(moda);
        needs_depends.erase(ai);
        ai = needs_depends.begin();
      } else {
        ++ai;
        RW_ASSERT(ai != needs_depends.end());
      }
    }
  }

  return loaded_pbmod;
}

PbModule* PbModel::module_lookup(
    YangModule* ymod)
{
  RW_ASSERT(ymod);

  auto pbmi = modules_by_ymod_.find(ymod);
  if (pbmi == modules_by_ymod_.end()) {
    return nullptr;
  }
  return pbmi->second;
}

PbModule* PbModel::module_lookup(
    std::string ns)
{
  auto pbmod = modules_by_ns_.get_symbol(ns);
  if (pbmod) {
    RW_ASSERT(pbmod);
    return pbmod;
  }
  return nullptr;
}

bool PbModel::module_is_ordered_before(
    PbModule* a_pbmod,
    PbModule* b_pbmod) const
{
  RW_ASSERT(a_pbmod);
  RW_ASSERT(b_pbmod);
  for (const auto& check_pbmod: modules_by_dependencies_) {
    if (check_pbmod == b_pbmod) {
      return false;
    }
    if (check_pbmod == a_pbmod) {
      return true;
    }
  }
  RW_ASSERT_NOT_REACHED();
}

void PbModel::populate_augment_root(
    YangNode* ynode,
    PbNode** pbnode)
{
  RW_ASSERT(ynode);
  RW_ASSERT(pbnode);

  // Already know the node?
  *pbnode = lookup_node(ynode);
  if (*pbnode) {
    return;
  }

  /*
   * Parent node cannot be the yang root node!  There is an assumption
   * in this code that the module root-level nodes have all been
   * populated before any attempt to find an augment node chain.  This
   * assumption avoids having to figure out which copy of the yang root
   * node is the parent (there are many PbNodes mapped to the single
   * yang root node).  See PbNode::populate_module_root().
   */
  YangNode* yparent = ynode->get_parent();
  RW_ASSERT(yparent != ymodel_->get_root_node());

  // Recurse
  PbNode* parent_pbnode = nullptr;
  populate_augment_root(yparent, &parent_pbnode);

  // Create the node
  *pbnode = get_node(ynode, parent_pbnode);
  RW_ASSERT(*pbnode);
}

PbNode* PbModel::get_node(YangNode* ynode, PbNode* parent_pbnode)
{
  // Already know the node?
  RW_ASSERT(ynode);
  PbNode* pbnode = lookup_node(ynode);
  if (pbnode) {
    return pbnode;
  }

  /*
   * Populate all the nodes in parent from the first node to the target
   * node.  This must be done in order to preserve the children in yang
   * order.  The yang order is needed in parser's output stage in order
   * to generate a yang order array.  The yang order array is needed
   * because protobuf-c will reorder the fields from the order in the
   * proto file to ordered by tag number, due proto wire format rules
   * (tag order).  However, XML conversion requires yang order, not tag
   * order, so a translation array is needed.
   */
  if (   parent_pbnode
         && RW_YANG_STMT_TYPE_ROOT != parent_pbnode->get_yang_stmt_type()) {
    YangNodeIter yni = parent_pbnode->get_ynode()->child_begin();
    auto pbi = parent_pbnode->child_last();
    if (pbi != parent_pbnode->child_begin()) {
      yni = YangNodeIter((*pbi)->get_ynode());
      ++yni;
    }
    for (/*yni already set*/; yni != YangNodeIter(); ++yni) {
      YangNode* other_ynode = &*yni;
      RW_ASSERT(other_ynode);
      if (other_ynode == ynode) {
        goto create;
      }
      pbnode = get_node(other_ynode, parent_pbnode);
      RW_ASSERT(pbnode);
    }
    RW_ASSERT_NOT_REACHED(); // must find the child
  }

create:
  // Find the module.  Must exist (this should never be called on yang model root).
  YangModule* ymod = ynode->get_module_orig();
  RW_ASSERT(ymod);
  PbModule* pbmod = module_lookup(ymod);
  RW_ASSERT(pbmod);

  pbnode = new PbNode(pbmod, ynode, parent_pbnode);
  node_map_.emplace(ynode, pbnode);

  if (parent_pbnode) {
    parent_pbnode->child_add(pbnode);
  }
  return pbnode;
}

PbNode* PbModel::lookup_node(YangNode* ynode)
{
  RW_ASSERT(ynode);
  auto ni = node_map_.find(ynode);
  if (ni != node_map_.end()) {
    return ni->second;
  }
  return nullptr;
}

PbMessage* PbModel::alloc_pbmsg(
    PbNode* pbnode,
    PbMessage* parent_pbmsg,
    PbMsgType pb_msg_type)
{
  RW_ASSERT(pbnode);
  PbMessage* pbmsg = new PbMessage(pbnode, parent_pbmsg, pb_msg_type);
  owned_pbmsgs_.emplace_back(pbmsg);
  return pbmsg;
}

void PbModel::error(const std::string& location, const std::string& text)
{
  std::cout << location << ": Error: " << text << std::endl;
  failed_ = true;
  ++errors_;
}

void PbModel::warning(const std::string& location, const std::string& text)
{
  if (!nowarn_) {
    std::cout << location << ": Warning: " << text << std::endl;
    if (werror_) {
      failed_ = true;
      ++errors_;
    } else {
      ++warnings_;
    }
  }
}

std::string PbModel::get_ypbc_global(
    const char* discriminator)
{
  RW_ASSERT(discriminator);
  return std::string("rw_ypbc_") + schema_pbmod_->get_proto_typename() + "_" + discriminator;
}

// The following functions are sort of hack.
// JIRA 9224 created to clean this up.
std::string PbModel::get_pbc_message_global(
    const char* discriminator,
    PbMessage::output_t output_style)
{
  RW_ASSERT(discriminator);
  std::string smod = mangle_to_lowercase(mangle_to_underscore(schema_pbmod_->get_proto_package_name()));
  switch (output_style) {
    case PbMessage::output_t::message:
      smod += "__yang_data__"; // Currently root supported only for data.
      break;
    case PbMessage::output_t::path_entry:
      smod += "__yang_keyspec_entry__";
      break;
    case PbMessage::output_t::path_spec:
      smod += "__yang_keyspec_path__";
      break;
    case PbMessage::output_t::dom_path:
      smod += "__yang_keyspec_dom_path__";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  return smod + discriminator;
}

std::string PbModel::get_pbc_schema_typename(
    PbMessage::output_t output_style)
{
  std::string smod = mangle_typename(schema_pbmod_->get_proto_package_name());
  switch (output_style) {
    case PbMessage::output_t::path_entry:
      smod += "__YangKeyspecEntry";
      break;
    case PbMessage::output_t::path_spec:
      smod += "__YangKeyspecPath";
      break;
    case PbMessage::output_t::dom_path:
      smod += "__YangKeyspecDomPath";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return smod;
}

void PbModel::output_h_structs(
    std::ostream& os,
    unsigned indent,
    PbMessage::output_t output_style)
{
  std::string pad1(indent+2, ' ');
  std::string pad2(indent+4, ' ');

  /*
   * ATTN:- Support only for data root!
   * This is not the correct way to generate the msg descriptors
   * We should create data_root_ PbMessages..  That is a lot of change!
   * So considering this as an temporary hack. Please come back and fix!!
   */
  std::string discriminatort = "", discriminatorv = "";
  switch (output_style) {
    case PbMessage::output_t::message:
      discriminatort = "data_t_msg_msgdesc";
      discriminatorv = "data_g_msg_msgdesc";
      break;
    case PbMessage::output_t::path_spec:
      discriminatort = "data_t_path_msgdesc";
      discriminatorv = "data_g_path_msgdesc";
      break;
    case PbMessage::output_t::path_entry:
      discriminatort = "data_t_entry_msgdesc";
      discriminatorv = "data_g_entry_msgdesc";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  os << "struct " << get_ypbc_global(discriminatort.c_str()) << std::endl;
  os << "{ " << std::endl;
  if (output_style == PbMessage::output_t::message) {
    os << pad1 << "struct " << std::endl;
    os << pad1 << "{" << std::endl;
  } else {
    pad2 = pad1;
  }
  os << pad2 << "void *module;" << std::endl;
  os << pad2 << "const char* yang_node_name;" << std::endl;
  os << pad2 << "const uint32_t pb_element_tag;" << std::endl;
  os << pad2 << "const size_t num_fields; " << std::endl;
  os << pad2 << "const rw_yang_pb_flddesc_t* ypbc_flddescs;" << std::endl;
  os << pad2 << "const rw_yang_pb_msgdesc_t* parent;" << std::endl;
  os << pad2 << "const ProtobufCMessageDescriptor* pbc_mdesc;" << std::endl;
  switch (output_style) {
    case PbMessage::output_t::message:
      os << pad2 << "const " << get_ypbc_global("data_t_entry_msgdesc")
          << " *schema_entry_ypbc_desc;" << std::endl;
      os << pad2 << "const " << get_ypbc_global("data_t_path_msgdesc")
          << " *schema_path_ypbc_desc;" << std::endl;
      break;
    case PbMessage::output_t::path_spec:
    case PbMessage::output_t::path_entry:
      os << pad2 << "const void* schema_entry_ypbc_desc;" << std::endl;
      os << pad2 << "const void* schema_path_ypbc_desc;" << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  os << pad2 << "const void* schema_list_only_ypbc_desc;" << std::endl;
  os << pad2 << "const void* schema_entry_value;" << std::endl;
  os << pad2 << "const void* schema_path_value;" << std::endl;
  os << pad2 << "const rw_yang_pb_msgdesc_t* const* child_msgs;" << std::endl;
  os << pad2 << "size_t child_msg_count;" << std::endl;
  os << pad2 << "rw_yang_pb_msg_type_t msg_type;" << std::endl;

  switch (output_style) {
    case PbMessage::output_t::message:
      os << pad2 << "void *u;" << std::endl;
      break;
    case PbMessage::output_t::path_spec:
    case PbMessage::output_t::path_entry:
      os << pad2 << "const " << get_ypbc_global("data_t_msg_msgdesc")
          << " *msg_msgdesc;" << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (output_style == PbMessage::output_t::message) {
    os << pad1 << "} ypbc;" << std::endl;
  }

  if (output_style == PbMessage::output_t::message) {
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->has_data_module_fields()) {
        os << pad1 << "const " << pbmod->get_data_module_pbmsg()->get_ypbc_global("t_msg_msgdesc")
            << "* " << pbmod->get_proto_typename() << ";" << std::endl;
      }
    }
  }
  os << "};" << std::endl;
  os << "extern const " << get_ypbc_global(discriminatort.c_str()) << " "
      << get_ypbc_global(discriminatorv.c_str()) << ";" << std::endl;
}

void PbModel::output_cpp_schema(
    std::ostream& os,
    unsigned indent,
    PbMessage::output_t output_style)
{
  std::string pad1(indent+2, ' ');
  std::string pad2(indent+4, ' ');
  std::string pad3(indent+6, ' ');
  std::string pad4(indent+8, ' ');

  std::string discriminator = "";
  switch (output_style) {
    case PbMessage::output_t::path_spec:
      discriminator = "data_g_path_value";
      break;
    case PbMessage::output_t::path_entry:
      discriminator = "data_g_entry_value";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  os << "const " << get_pbc_schema_typename(output_style)
      << " " << get_ypbc_global(discriminator.c_str()) << " =" << std::endl;

  os << "{" << std::endl;
  os << pad1 << "{PROTOBUF_C_MESSAGE_INIT(&"
      << get_pbc_message_global("descriptor", output_style)
      << ", PROTOBUF_C_FLAG_REF_STATE_GLOBAL , 0)}," << std::endl;

  if (output_style == PbMessage::output_t::path_spec) {
    os << pad1 << "true," << std::endl;
    os << pad1 << "{" << std::endl;
    os << pad2 << "{PROTOBUF_C_MESSAGE_INIT(&"
        << get_pbc_message_global("descriptor", PbMessage::output_t::dom_path)
        << ", PROTOBUF_C_FLAG_REF_STATE_INNER , "
        << " offsetof(" << get_pbc_schema_typename(output_style)
        << ", dompath))}," << std::endl;

    os << pad2 << "true," << std::endl;
    os << pad2 << "true," << std::endl;
    os << pad2 << "RW_SCHEMA_CATEGORY_ANY," << std::endl;
    os << pad2 << "true," << std::endl;
    os << pad2 << "{" << std::endl;
    os << pad3 << "{PROTOBUF_C_MESSAGE_INIT(&"
        << get_pbc_message_global("descriptor", PbMessage::output_t::path_entry)
        << ", PROTOBUF_C_FLAG_REF_STATE_INNER , "
        << " offsetof(" << get_pbc_schema_typename(PbMessage::output_t::dom_path)
        << ", path_root))}," << std::endl;
  } else {
    pad4 = pad2;
    pad3 = pad1;
  }

  os << pad3 << "{" << std::endl;
  os << pad4 << "{PROTOBUF_C_MESSAGE_INIT(&rw_schema_element_id__descriptor,PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof("
      << get_pbc_schema_typename(PbMessage::output_t::path_entry)
      << ", element_id))}," << std::endl;
  os << pad4 << "0," << std::endl;
  os << pad4 << "0," << std::endl;
  os << pad4 << "RW_SCHEMA_ELEMENT_TYPE_ROOT," << std::endl;
  os << pad3 << "}," << std::endl;
  if (output_style == PbMessage::output_t::path_spec) {
    os << pad2 << "}," << std::endl;
    os << pad1 << "}," << std::endl;
    os << pad1 << "false," << std::endl;
    os << pad1 << "{ 0, nullptr, }," << std::endl;
    os << pad1 << "nullptr" << std::endl;
  }
  os << "};" << std::endl;
}

void PbModel::output_cpp_descs(
    std::ostream& os,
    unsigned indent,
    PbMessage::output_t output_style)
{
  std::string pad1(indent+1, ' ');
  std::string pad2(indent+2, ' ');
  std::string pad3(indent+3, ' ');

  std::string discriminatort = "", discriminatorv = "";
  switch (output_style) {
    case PbMessage::output_t::message:
      discriminatort = "data_t_msg_msgdesc";
      discriminatorv = "data_g_msg_msgdesc";
      break;
    case PbMessage::output_t::path_spec:
      discriminatort = "data_t_path_msgdesc";
      discriminatorv = "data_g_path_msgdesc";
      break;
    case PbMessage::output_t::path_entry:
      discriminatort = "data_t_entry_msgdesc";
      discriminatorv = "data_g_entry_msgdesc";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
  // Support only for data root!!
  os << "const " << get_ypbc_global(discriminatort.c_str()) << " "
      << get_ypbc_global(discriminatorv.c_str()) << " = " << std::endl;

  os << "{" << std::endl;
  if (output_style == PbMessage::output_t::message) {
    os << pad2 << "{" << std::endl;
  } else {
    pad3 = pad2;
  }
  os << pad3 << "nullptr," << std::endl; /* Module, null for schema descs */
  os << pad3 << "\"data\"," << std::endl;
  os << pad3 << "0," << std::endl; /* Element tag */
  os << pad3 << "0," << std::endl; /* num of fields */
  os << pad3 << "nullptr," << std::endl; /* Field Descs */
  os << pad3 << "nullptr," << std::endl; /* parent */
  os << pad3 << "&" << get_pbc_message_global("descriptor", output_style) << "," << std::endl; /* pbc_desc */

  switch (output_style) {
    case PbMessage::output_t::message:
      os << pad3 << "&" << get_ypbc_global("data_g_entry_msgdesc")
          << "," << std::endl;
      os << pad3 << "&" << get_ypbc_global("data_g_path_msgdesc")
          << "," << std::endl;
      os << pad3 << "nullptr," << std::endl;
      os << pad3 << "&" << get_ypbc_global("data_g_entry_value")
          << "," << std::endl;
      os << pad3 << "&" << get_ypbc_global("data_g_path_value")
          << "," << std::endl;
      break;
    case PbMessage::output_t::path_spec:
    case PbMessage::output_t::path_entry:
      os << pad3 << "nullptr," << std::endl; /* entry ypbc mdesc */
      os << pad3 << "nullptr," << std::endl; /* pathspec ypbc mdesc */
      os << pad3 << "nullptr," << std::endl; /* listonly ypbc mdesc */
      os << pad3 << "nullptr," << std::endl; /* entry value */
      os << pad3 << "nullptr," << std::endl; /* path value */
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (output_style == PbMessage::output_t::message) {
    unsigned mcount = 0;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->has_data_module_fields()) {
        mcount++;
      }
    }

    os << pad3 << "reinterpret_cast<const rw_yang_pb_msgdesc_t* const*>("
        << "reinterpret_cast<const char*>(&" << get_ypbc_global("data_g_msg_msgdesc")
        << ".ypbc) + sizeof(rw_yang_pb_msgdesc_t))," << std::endl;
    os << pad3 << mcount << "," << std::endl;
  } else {
    os << pad3 << "nullptr," << std::endl;
    os << pad3 << "0," << std::endl;
  }

  switch (output_style) {
    case PbMessage::output_t::message:
      os << pad3 << "RW_YANGPBC_MSG_TYPE_ROOT," << std::endl;
      os << pad3 << "nullptr," << std::endl; /* void *u */
      break;
    case PbMessage::output_t::path_spec:
      os << pad3 << "RW_YANGPBC_MSG_TYPE_SCHEMA_PATH," << std::endl;
      os << pad3 << "&" << get_ypbc_global("data_g_msg_msgdesc") << "," << std::endl;
      break;
    case PbMessage::output_t::path_entry:
      os << pad3 << "RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY," << std::endl;
      os << pad3 << "&" << get_ypbc_global("data_g_msg_msgdesc") << "," << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (output_style == PbMessage::output_t::message) {
    os << pad2 << "}," << std::endl;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->has_data_module_fields()) {
        os << pad2 << "&" << pbmod->get_data_module_pbmsg()->get_ypbc_global("g_msg_msgdesc")
            << "," << std::endl;
      }
    }
  }

  os << "};" << std::endl;
}

std::string PbModel::register_c_global(
    std::string name)
{
  RW_ASSERT(name.length());
  std::string base_name(name);

  unsigned suffix = 0;
  while(1) {
    auto vi = static_vars_.find(name);
    if (vi == static_vars_.end()) {
      static_vars_.emplace(name);
      return name;
    }

    // Append a suffix for uniqueness
    char buf[16];
    int bytes = snprintf(buf, sizeof(buf), "_%u", ++suffix);
    RW_ASSERT(bytes>1 && (unsigned)bytes<sizeof(buf));
    name = base_name + &buf[0];
    RW_ASSERT(suffix < 10000); // for sanity
  }
}

PbEnumType* PbModel::enum_get(
    YangTypedEnum* ytenum,
    PbModule* pbmod)
{
  RW_ASSERT(ytenum);
  RW_ASSERT(pbmod);

  auto ei = enum_ytenum_map_.find(ytenum);
  if (ei != enum_ytenum_map_.end()) {
    return ei->second;
  }

  PbEnumType::enum_type_uptr_t pbenum_uptr(new PbEnumType(ytenum, pbmod));
  PbEnumType* pbenum = pbenum_uptr.get();
  pbmod->enum_add(std::move(pbenum_uptr));
  return pbenum;
}

PbEnumType* PbModel::enum_get(
    YangType* ytype,
    PbNode* pbnode)
{
  RW_ASSERT(ytype);
  RW_ASSERT(pbnode);

  YangTypedEnum* ytenum = ytype->get_typed_enum();
  if (ytenum) {
    auto ei = enum_ytenum_map_.find(ytenum);
    if (ei != enum_ytenum_map_.end()) {
      return ei->second;
    }
  }

  auto ti = enum_ytype_map_.find(ytype);
  if (ti != enum_ytype_map_.end()) {
    return ti->second;
  }

  PbEnumType::enum_type_uptr_t pbenum_uptr(new PbEnumType(ytype, pbnode));
  PbEnumType* pbenum = pbenum_uptr.get();
  PbModule* pbmod = pbenum->get_pbmod();
  RW_ASSERT(pbmod);
  pbmod->enum_add(std::move(pbenum_uptr));

  pbenum->parse();
  return pbenum;
}

void PbModel::enum_add(
    PbEnumType* pbenum)
{
  RW_ASSERT(pbenum);
  YangType* ytype = pbenum->get_ytype();
  if (ytype) {
    auto status = enum_ytype_map_.emplace(ytype, pbenum);
    RW_ASSERT(status.second); // otherwise, should not have bothered creating another copy
  }

  YangTypedEnum* ytenum = pbenum->get_ytenum();
  if (ytenum) {
    auto status = enum_ytenum_map_.emplace(ytenum, pbenum);
    RW_ASSERT(status.second); // otherwise, should not have bothered creating another copy
  }
}

void PbModel::parse_nodes()
{
  // Find and mark all the augmented nodes.
  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->find_augmented_nodes();
  }

  // Parse all the nodes
  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->parse_nodes();
  }

  // Parse all the messages
  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->parse_messages();
    if (pbmod->has_data_module_fields()) {
      has_data_ = true;
    }
    if (pbmod->has_rpc_messages()) {
      has_rpcs_ = true;
    }
    if (pbmod->has_notif_module_messages()) {
      has_notif_ = true;
    }
  }

  // Find equivalent messages and output order.
  pbeqmgr_.find_preferred_msgs();

  // Determine the top-level messages ordering
  find_top_level_order();

  // Order the top-level groupings
  schema_pbmod_->find_grouping_order();

  // Determine which enums have equivalences
  find_equivalent_enums();

  // Determine proto imports.
  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->find_schema_imports();
  }
}

void PbModel::find_equivalent_enums()
{
  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->find_equivalent_enums();
  }
}

void PbModel::save_top_level_equivalence(
    PbEqMsgSet* pbeq)
{
  RW_ASSERT(pbeq);
  top_level_pbeqs_.emplace_back(pbeq);
}

void PbModel::find_top_level_order()
{
#if ATTN_TGS_TRY_KEEPING_ALL_TOPS__TEMPORARILY_NEEDED_FOR_GI
  // Strip all the conflicts out of the list of top-level sets.
  for (auto tei = top_level_pbeqs_.begin(); tei != top_level_pbeqs_.end(); /*none*/) {
    if (!(*tei)->get_preferred_msg(schema_pbmod_)->is_local_top_level_message()) {
      /* ATTN: Complain about this? */
      tei = top_level_pbeqs_.erase(tei);
    } else {
      ++tei;
    }
  }
#endif

  // Reorder by dependencies.
  PbEqMsgSet::order_dependencies(&top_level_pbeqs_);
}

parser_control_t PbModel::output_proto(const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;
    filename.append(".proto");

    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  parser_control_t retval = parser_control_t::DO_NEXT;
  RW_ASSERT(schema_pbmod_);

  os << "// ** AUTO GENERTATED FILE(by yangpbc) from "
     << schema_pbmod_->get_ymod()->get_name() << ".yang "
     << "** DO NOT EDIT **" << std::endl;
  os << std::endl;
  os << "package " << proto_schema_package_name_ << ";" << std::endl;
  os << std::endl;

  os << "import \"rwpbapi.proto\";" << std::endl;
  bool needs_rw_schema = false;
  for (const auto& pbmod: modules_by_dependencies_) {
    if (pbmod != schema_pbmod_) {
      if (pbmod->is_import_needed()) {
        os << "import \"" << pbmod->get_ymod()->get_name() << ".proto\";" << std::endl;
      }

      if (   pbmod->has_data_module_fields()
          || pbmod->has_rpc_messages()
          || pbmod->has_notif_module_messages()) {
        needs_rw_schema = true;
      }
    }
  }

  if (   needs_rw_schema
      || schema_pbmod_->has_data_module_fields()
      || schema_pbmod_->has_rpc_messages()
      || schema_pbmod_->has_notif_module_messages()) {
    os << "import \"rw_schema.proto\";" << std::endl;
  }
  os << std::endl;

  os << "option cc_generic_services = true;" << std::endl;

  // Output any rift specific file options.
  schema_pbmod_->output_proto_file_options(os, 0/*indent*/);
  os << std::endl;

  // Output enumeration types only for the schema package.
  schema_pbmod_->output_proto_enums(os, 0/*indent*/);

  // ATTN: RIFT-2788: // Output union types for the schema module.

  // ATTN: Want a separate config tree?
  if (has_data_) {
    os << "message YangData" << std::endl;
    os << "{" << std::endl;

    // Output the yangpbc message descriptor.
    std::ostringstream opts;
    opts << " ypbc_msg:\"" << get_ypbc_global("data_g_msg_msgdesc") << "\"";
    os << "  option (rw_msgopts) = {" << opts.str() << " };" << std::endl;

    bool first = true;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (first) {
        first = false;
      } else {
        os << std::endl;
      }
      pbmod->output_proto_data_module(os, 2/*indent*/);
    }
    os << std::endl;
    // Output the module root messages as fields.
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_proto_data_module_as_field(os, 2/*indent*/);
    }
    os << "}" << std::endl;
    os << std::endl;
  }

  if (has_rpcs_) {
    os << "message YangInput" << std::endl;
    os << "{" << std::endl;
    //os << "  option (rw_msgopts) = { suppress:true }; " << std::endl;
    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_rpci_module(os, 2/*indent*/);
    }
    os << std::endl;
    // Output the module root messages as fields.
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_proto_rpci_module_as_field(os, 2/*indent*/);
    }
    os << "}" << std::endl;
    os << std::endl;
  }

  if (has_rpcs_) {
    os << "message YangOutput" << std::endl;
    os << "{" << std::endl;
    //os << "  option (rw_msgopts) = { suppress:true }; " << std::endl;
    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_rpco_module(os, 2/*indent*/);
    }
    os << std::endl;
    // Output the module root messages as fields.
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_proto_rpco_module_as_field(os, 2/*indent*/);
    }
    os << "}" << std::endl;
    os << std::endl;
  }

  if (has_notif_) {
    os << "message YangNotif" << std::endl;
    os << "{" << std::endl;
    //os << "  option (rw_msgopts) = { suppress:true }; " << std::endl;
    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_notif_module(os, 2/*indent*/);
    }
    os << std::endl;
    // Output the module root messages as fields.
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_proto_notif_module_as_field(os, 2/*indent*/);
    }
    os << "}" << std::endl;
    os << std::endl;
  }

  /*
   * Output the groupings only for the schema package.
   *
   * ATTN: grouping is temporarily ordered after everything else.  This
   * has the effect of forcing grouping instantiation to occur only
   * rarely, which is considered desireable only from the point of view
   * of rw_xml/yangmodel.  XML conversion currently requires a rooted
   * path in order to find the yang metadata.  This requirement will
   * eventually change, at which point groupings will move up, after
   * top-level, but before data, rpc, and notif.
   *
   * ATTN: TGS: actually, grouping is currently suppressed, due to
   * problems encountered during the great renaming.  I don't remember
   * what those problems were - perhaps in was difficulties handling
   * type-puns before I the c-typedef extension?
   */
  schema_pbmod_->output_proto_group_root(os, 0/*indent*/);

  if (has_data_ || has_rpcs_ || has_notif_) {
    os << "message YangKeyspecEntry" << std::endl;
    os << "{" << std::endl;

    if (has_data_) {
      std::ostringstream opts;
      opts << " ypbc_msg:\"" << get_ypbc_global("data_g_entry_msgdesc") << "\"";
      os << "  option (rw_msgopts) = {" << opts.str() << " base_typename:\"rw_keyspec_entry_t\" };" << std::endl;
    } else {
      os << "  option (rw_msgopts) = { suppress:true }; " << std::endl;
    }

    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_schema(PbMessage::output_t::path_entry, os, 2/*indent*/);
    }

    if (has_data_) {
      os << "  required RwSchemaElementId element_id = 1 [ (rw_fopts) = {inline:true} ];" << std::endl;
    }
    os << "}" << std::endl;
    os << std::endl;

    os << "message YangKeyspecDomPath" << std::endl;
    os << "{" << std::endl;
    if (!has_data_) {
      os << "  option (rw_msgopts) = { suppress:true hide_from_gi:true }; " << std::endl;
    }
    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_schema(PbMessage::output_t::dom_path, os, 2/*indent*/);
    }
    if (has_data_) {
      // output keyspec for schema root (data for now). ATTN:- This message
      // hierarchy will be changed to support other roots.
      os << std::endl;
      os << "  required bool rooted = 1;" << std::endl;
      os << "  optional RwSchemaCategory category = 2;" << std::endl;
      os << "  optional ." << proto_schema_package_name_
          << ".YangKeyspecEntry path_root = 98 [ (rw_fopts) = {inline:true} ];" << std::endl;
    }
    os << "}" << std::endl;
    os << std::endl;

    os << "message YangKeyspecPath" << std::endl;
    os << "{" << std::endl;

    if (has_data_) {
      std::ostringstream opts;
      opts << " ypbc_msg:\"" << get_ypbc_global("data_g_path_msgdesc") << "\"";
      os << "  option (rw_msgopts) = {" << opts.str() << " base_typename:\"rw_keyspec_path_t\" };" << std::endl;
    } else {
      os << "  option (rw_msgopts) = { suppress:true hide_from_gi:true }; " << std::endl;
    }

    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_schema(PbMessage::output_t::path_spec, os, 2/*indent*/);
    }
    if (has_data_) {
      // Output keyspec for schema data root
      os << std::endl;
      os << "  optional ." << proto_schema_package_name_
          << ".YangKeyspecDomPath dompath = 1 [ (rw_fopts) = {inline:true} ]; " << std::endl;
      os << "  optional bytes binpath  = 2;" << std::endl;
      os << "  optional string strpath = 99;" << std::endl;
    }
    os << "}" << std::endl;
    os << std::endl;

    os << "message YangListOnly" << std::endl;
    os << "{" << std::endl;
    os << "  option (rw_msgopts) = { suppress:true hide_from_gi:true }; " << std::endl;
    for (const auto& pbmod: modules_by_dependencies_) {
      os << std::endl;
      pbmod->output_proto_schema(PbMessage::output_t::list_only, os, 2/*indent*/);
    }
    os << "}" << std::endl;
    os << std::endl;
  }

  os.close();
  return retval;
}

parser_control_t PbModel::output_cpp_file(const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;
    filename.append(".ypbc.cpp");

    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  // ATTN: plain filename should be a member variable or function somewhere
  std::string file_name = basename;
  const size_t last_slash_idx = file_name.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    file_name.erase(0, last_slash_idx + 1);
  }

  os << "// ** AUTO GENERTATED FILE (by yangpbc) from "
     << schema_pbmod_->get_ymod()->get_name() << ".yang "
     << "** DO NOT EDIT **" << std::endl;
  os << "#include <sys/cdefs.h>" << std::endl;
  os << "#include <yangmodel.h>" << std::endl;
  os << "#include <rw_xml.h>" << std::endl;
  os << std::endl;
  os << "#include \"" << file_name << ".pb-c.h\"" << std::endl;
  os << "#include \"" << file_name << ".ypbc.h\"" << std::endl;
  os << std::endl;

  // Output enumerations for the schema module.
  schema_pbmod_->output_cpp_enums(os, 0/*indent*/);

  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->output_cpp_descs(os, 0/*indent*/);
  }

  // Output schema data root msg descs.
  if (has_data_) {

    output_cpp_schema(os, 0, PbMessage::output_t::path_spec);
    os << std::endl;

    output_cpp_schema(os, 0, PbMessage::output_t::path_entry);
    os << std::endl;

    output_cpp_descs(os, 0, PbMessage::output_t::message);
    os << std::endl;

    output_cpp_descs(os, 0, PbMessage::output_t::path_spec);
    os << std::endl;

    output_cpp_descs(os, 0, PbMessage::output_t::path_entry);
    os << std::endl;
  }
  os << std::endl;

  os << "const rw_yang_pb_msgdesc_t* const " << msglist_var_ << "[] = {" << std::endl;
  for (auto const& pbeq: top_level_pbeqs_) {
    PbMessage* preferred_pbmsg = pbeq->get_preferred_msg(schema_pbmod_);
    if (preferred_pbmsg && preferred_pbmsg->is_local_top_level_message()) {
      os << "  reinterpret_cast<const rw_yang_pb_msgdesc_t*>(&"
         << preferred_pbmsg->get_ypbc_global("g_msg_msgdesc") << ")," << std::endl;
    }
  }

  os << "  nullptr" << std::endl;
  os << "};" << std::endl;
  os << std::endl;

  os << "const " << get_ypbc_global("t_schema") << " " << get_ypbc_global("g_schema") << " = {" << std::endl;
  os << "  &" << schema_pbmod_->get_ypbc_global("g_module") << "," << std::endl;
  if (has_data_) {
    os << "  &" << get_ypbc_global("data_g_msg_msgdesc") << "," << std::endl;
  } else {
    os << " nullptr," << std::endl;
  }
  os << "  &" << msglist_var_ << "[0]," << std::endl;
  os << "  reinterpret_cast<const rw_yang_pb_module_t* const*>("
     << "reinterpret_cast<const char*>(&" << get_ypbc_global("g_schema")
     << ")+sizeof(rw_yang_pb_schema_t))," << std::endl;
  os << "  " << modules_by_dependencies_.size() << "," << std::endl;

  for (const auto& pbmod: modules_by_dependencies_) {
    os << "  &" << pbmod->get_ypbc_global("g_module") << "," << std::endl;
  }
  os << "};" << std::endl;
  os << std::endl;
  os.close();
  return parser_control_t::DO_NEXT;
}

parser_control_t PbModel::output_gi_c_file(const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;
    filename.append(".ypbc.gi.c");

    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  std::string pad2(2, ' ');

  std::string pbch_file_name = basename;
  const size_t last_slash_idx = pbch_file_name.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    pbch_file_name.erase(0, last_slash_idx + 1);
  }
  os << "// ** AUTO GENERTATED FILE (by yangpbc) from "
     << schema_pbmod_->get_ymod()->get_name() << ".yang " << std::endl
     << "// ** DO NOT EDIT **" << std::endl;
  os << "#include \"" << pbch_file_name << ".ypbc.h\"" << std::endl;
  os << std::endl;

  os << std::endl;
  os << "const rw_yang_pb_schema_t* " << get_gi_c_global_func_name("get_schema") << "(void)" << std::endl;
  os << "{" << std::endl;
  os << "  extern const struct " << get_ypbc_global("t_schema") << " "
     << get_ypbc_global("g_schema") << ";" << std::endl;
  os << "  return (const rw_yang_pb_schema_t*)&" << get_ypbc_global("g_schema") << ";" << std::endl;
  os << "}" << std::endl;
  os << std::endl;

  if (has_data_) {
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_gi_c(os, 0/*indent*/);
    }
  }

  os.close();

  return parser_control_t::DO_NEXT;
}

std::string PbModel::get_gi_prefix(const char* discriminator) const
{
  RW_ASSERT(discriminator);
  return   std::string("rw_ypbc_") + discriminator + "_"
      + schema_pbmod_->get_proto_typename();
}

parser_control_t PbModel::output_h_file(const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;
    filename.append(".ypbc.h");

    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  std::string pbch_file_name = basename;
  const size_t last_slash_idx = pbch_file_name.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    pbch_file_name.erase(0, last_slash_idx + 1);
  }
  std::string pbcgih_file_name = pbch_file_name;
  pbcgih_file_name.append(".pb-c.h");
  pbch_file_name.append(".pb-c.h");


  std::string incguard("RW_YANGPBC_");
  incguard.append(proto_schema_package_name_);
  incguard.append("_H_");

  os << "// ** AUTO GENERTATED FILE (by yangpbc) from "
     << schema_pbmod_->get_ymod()->get_name() << ".yang "
     << "** DO NOT EDIT **" << std::endl;
  os << std::endl;
  os << "#ifndef " << incguard << std::endl;
  os << "#define " << incguard << std::endl;
  os << std::endl;

  os << "#ifndef __GI_SCANNER__"              << std::endl;
  os << "#include <yangmodel.h>" << std::endl;
  os << "#include <rw_xml.h>" << std::endl;
  os << "#include <sys/cdefs.h>" << std::endl;
  os << std::endl;

  for (const auto& pbmod: modules_by_dependencies_) {
    if (   schema_pbmod_ != pbmod
           && schema_pbmod_->imports_direct(pbmod)) {
      os << "#include \"" << pbmod->get_ymod()->get_name() << ".pb-c.h\"" << std::endl;
      os << "#include \"" << pbmod->get_ymod()->get_name() << ".ypbc.h\"" << std::endl;
    }
  }
  // gi.h includes
  os << "// gi.h includes"              << std::endl;
  os << "#include <sys/cdefs.h>"              << std::endl;
  os << "#include <stdint.h>"                 << std::endl;
  os << "#include <glib-object.h>"            << std::endl;
  os << "#include <yangmodel.h>"           << std::endl;
  os << "#include <rw_keyspec.h>"          << std::endl;
  os << "#include <yangpb_gi.h>"              << std::endl;
  os << "#include <rw_schema.pb-c.h>"      << std::endl;
  os << std::endl;

  os << "#endif // __GI_SCANNER__"              << std::endl;
  os << "#include <" << pbch_file_name << ">" << std::endl;
  os << std::endl;

  os << "__BEGIN_DECLS" << std::endl;
  os << std::endl;

  os << "#ifndef __GI_SCANNER__"              << std::endl;

  os << "extern const rw_yang_pb_msgdesc_t* const " << msglist_var_ << "[];" << std::endl;
  os << std::endl;

  os << "typedef struct " << get_ypbc_global("t_schema") << " "
     << get_ypbc_global("t_schema") << ";" << std::endl;

  if (has_data_) {
    os << "typedef struct " << get_ypbc_global("data_t_msg_msgdesc") << " "
        << get_ypbc_global("data_t_msg_msgdesc") << ";" << std::endl;
    os << "typedef struct " << get_ypbc_global("data_t_path_msgdesc") << " "
        << get_ypbc_global("data_t_path_msgdesc") << ";" << std::endl;
    os << "typedef struct " << get_ypbc_global("data_t_entry_msgdesc") << " "
        << get_ypbc_global("data_t_entry_msgdesc") << ";" << std::endl;
  }
  os << std::endl;

  schema_pbmod_->output_h_enums(os, 0/*indent*/);

  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->output_h_struct_tags(os, 0/*indent*/);
  }
  os << std::endl;

  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->output_h_structs(os, 0/*indent*/);
  }
  os << std::endl;

  // Output schema root msgs types
  if (has_data_) {
    output_h_structs(os, 0, PbMessage::output_t::message);
    os << std::endl;
    output_h_structs(os, 0, PbMessage::output_t::path_entry);
    os << std::endl;
    output_h_structs(os, 0, PbMessage::output_t::path_spec);
    os << std::endl;
  }

  os << "struct " << get_ypbc_global("t_schema") << " {" << std::endl;
  os << "  const " << schema_pbmod_->get_ypbc_global("t_module") << "* schema_module;" << std::endl;
  if (has_data_) {
    os << "  const " << get_ypbc_global("data_t_msg_msgdesc") << "* data_root;" << std::endl;
  } else {
    os << "  const void *data_root;" << std::endl;
  }
  os << "  const rw_yang_pb_msgdesc_t* const* top_msglist;" << std::endl;
  os << "  const rw_yang_pb_module_t* const* modules;" << std::endl;
  os << "  size_t module_count;" << std::endl;

  for (const auto& pbmod: modules_by_dependencies_) {
    os << "  const " << pbmod->get_ypbc_global("t_module") << "* "
       << pbmod->get_proto_fieldname() << ";" << std::endl;
  }

  os << "};" << std::endl;

  os << "extern const " << get_ypbc_global("t_schema") << " "
     << get_ypbc_global("g_schema") << ";" << std::endl;
  os << std::endl;

  os << "/**** Meta data Macros ******/" << std::endl;

  for (const auto& pbmod: modules_by_dependencies_) {
    pbmod->output_h_meta_data_macros(os, 0/*indent*/);
  }
  os << std::endl;

  // Generate what used to be in the .gi.h files
  os << "#endif // __GI_SCANNER__" << std::endl;
  os <<"// The following used to be in the .gi.h files\n";

  os << "/**" << std::endl;
  os << " * " << get_gi_c_global_func_name("get_schema") << ":" << std::endl;
  os << " * Returns: (type RwYangPb.Schema):" << std::endl;
  os << " */" << std::endl;
  os << "const rw_yang_pb_schema_t* " << get_gi_c_global_func_name("get_schema") << "(void);" << std::endl;
  os << std::endl;

  os << std::endl;


  if (has_data_) {
    for (const auto& pbmod: modules_by_dependencies_) {
      pbmod->output_gi_h(os, 0/*indent*/);
    }
  }

  os << "__END_DECLS" << std::endl;

  os << "#endif" << std::endl;
  os.close();

  return parser_control_t::DO_NEXT;
}

parser_control_t PbModel::output_doc_user_file(const doc_file_t file_type, const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;

    switch (file_type) {
      case doc_file_t::TEXT:
        filename.append(".doc-user.txt");
        break;
      case doc_file_t::HTML:
        filename.append(".doc-user.html");
        break;
    }
    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  std::string doc_user_file_name = basename;
  const size_t last_slash_idx = doc_user_file_name.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    doc_user_file_name.erase(0, last_slash_idx + 1);
  }

  if (file_type == doc_file_t::HTML) {
    os << "<title>";
  }

  os << "User documentation for yang module "
     << schema_pbmod_->get_ymod()->get_name() << ".yang";

  if (file_type == doc_file_t::HTML) {
    os << "</title>";
  }

  os << std::endl << std::endl;

  // ATTN: List of schema files
  // ATTN: List of direct imports
  // ATTN: List of transitive imports
  if (file_type == doc_file_t::HTML) {
    os << "<ol>" << std::endl;
  }

  if (has_data_) {
    unsigned chapter = 1;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->output_doc(file_type, os, 0/*indent*/,
            PbMessage::doc_t::user_toc,
            chapter)) {
        ++chapter;
      }
    }
    os << "\n\n";
  }
  if (file_type == doc_file_t::HTML) {
    os << "</ol>" << std::endl;
  }

  if (has_data_) {
    unsigned chapter = 1;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->output_doc(file_type, os, 0/*indent*/,
            PbMessage::doc_t::user_entry,
            chapter)) {
        ++chapter;
      }
    }
  }

  os.close();

  return parser_control_t::DO_NEXT;
}

parser_control_t PbModel::output_doc_api_file(const doc_file_t file_type, const char* basename)
{
  std::ofstream os;
  if (basename) {
    std::string filename = basename;
    switch (file_type) {
      case doc_file_t::TEXT:
        filename.append(".doc-api.txt");
        break;
      case doc_file_t::HTML:
        filename.append(".doc-api.html");
        break;
    }
    os.open(filename,std::ios_base::trunc);

    if (!os.is_open()) {
      error(filename, "Unable to open file");
      return parser_control_t::DO_SKIP_TO_SIBLING;
    }
  } else {
    basename = "unittest";
  }

  std::string doc_api_file_name = basename;
  const size_t last_slash_idx = doc_api_file_name.find_last_of("\\/");
  if (std::string::npos != last_slash_idx) {
    doc_api_file_name.erase(0, last_slash_idx + 1);
  }

  if (file_type == doc_file_t::HTML) {
    os << "<title>";
  }

  os << "Programmer's API documentation for yang module "
     << schema_pbmod_->get_ymod()->get_name() << ".yang\n\n";

  if (file_type == doc_file_t::HTML) {
    os << "</title>";
  }

  os << std::endl << std::endl;

  if (file_type == doc_file_t::HTML) {
    os << "<ol>" << std::endl;
  }

  output_doc_heading(file_type, os, 0/*indent*/, PbMessage::doc_t::api_toc, "1", "Schema Globals" );
  os << "\n";
  if (has_data_) {
    unsigned chapter = 2;

    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->output_doc(file_type, os, 0/*indent*/,
            PbMessage::doc_t::api_toc,
            chapter)) {
        ++chapter;
      }
    }

  }
  if (file_type == doc_file_t::HTML) {
    os << "</ol>" << std::endl;
  }

  os << "\n\n";

  std::string str_chapter = "1";
  output_doc_heading(file_type, os, 0/*indent*/, PbMessage::doc_t::api_entry, "1", "Schema Globals" );
  output_doc_field( file_type, os, 2/*indent*/, "1",
    "schema_type",
    "Global Schema Type",
    get_ypbc_global("t_schema") );
  output_doc_field( file_type, os, 2/*indent*/, "1",
    "schema_pointer",
    "Global Schema Pointer",
    get_ypbc_global("g_schema") );

  if (has_data_) {
    unsigned chapter = 2;
    for (const auto& pbmod: modules_by_dependencies_) {
      if (pbmod->output_doc(file_type, os, 0/*indent*/,
            PbMessage::doc_t::api_entry,
            chapter)) {
        ++chapter;
      }
    }
  }

  os.close();

  return parser_control_t::DO_NEXT;
}

void PbModel::output_doc_heading(
  const doc_file_t file_type,
  std::ostream& os,
  unsigned indent,
  PbMessage::doc_t doc_style,
  const std::string& chapter,
  const std::string& title )
{

  switch (doc_style) {
    case PbMessage::doc_t::api_toc:
    case PbMessage::doc_t::user_toc: {
      static constexpr size_t WIDTH = 80 - 2;
      unsigned depth = std::count(chapter.begin(), chapter.end(), '.');

      std::string pad(indent + depth*2, ' ');
      std::string sep(2, ' ');
      auto width = sep.length() + pad.length() + title.length() + chapter.length();
      if (width < WIDTH) {
        sep.append(WIDTH-width, ' ');
      }

      if (file_type == doc_file_t::HTML) {
        os << "<li> <A href=\"#" << chapter << "\"> " << std::endl;
      }
      
      os << pad << title << sep << chapter;

      if (file_type == doc_file_t::HTML) {
        os << "</A></li>" << std::endl;
      } else {
        os << std::endl;
      }

      break;
    }
    case PbMessage::doc_t::api_entry:
    case PbMessage::doc_t::user_entry: {
      if (file_type == doc_file_t::HTML) {
        os << "<h1><A name=\"" << chapter << "\"> " << std::endl;
      }

      std::string pad(indent, ' ');
      os << pad << chapter << "  " << title;
      if (file_type == doc_file_t::HTML) {
        os << "</A></h1>" << std::endl;
      } else {
        os << std::endl;
      }

      break;
    }
    default:
      RW_ASSERT_NOT_REACHED();
  }

}

void PbModel::output_doc_field(
  const doc_file_t file_type,    
  std::ostream& os,
  unsigned indent,
  const std::string& chapter,
  const std::string& doc_key,
  const std::string& entry_type,
  const std::string& entry_data )
{
  auto adjusted_data = YangModel::adjust_indent_normal(indent+2, entry_data);
  if (adjusted_data.length()) {
    std::string pad(indent, ' ');

    if (file_type == doc_file_t::HTML) {
      os << "<p>";
    }

    os << pad << entry_type << "\n"
       << adjusted_data;

    if (file_type == doc_file_t::HTML) {
      os << "</p>";
    } else {
      os << std::endl;
    }
  }
}


} // namespace rw_yang

// End of yangpbc_model.cpp
