
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file yangpbc_message.cpp
 *
 * Convert yang schema to google protobuf: proto message support
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

void PbMessage::init_tables()
{
}

PbMessage::PbMessage(
  PbNode* pbnode,
  PbMessage* parent_pbmsg,
  PbMsgType pb_msg_type)
: pbnode_(pbnode),
  parent_pbmsg_(parent_pbmsg),
  pb_msg_type_(pb_msg_type)
{
  RW_ASSERT(pbnode_);

  if (PbMsgType::automatic == pb_msg_type_) {
    switch (pbnode_->get_yang_stmt_type()) {
      case RW_YANG_STMT_TYPE_ROOT:
      case RW_YANG_STMT_TYPE_RPC:
        RW_ASSERT_NOT_REACHED(); // must explicitly specify

      case RW_YANG_STMT_TYPE_CONTAINER:
      case RW_YANG_STMT_TYPE_LIST:
        pb_msg_type_ = PbMsgType::message;
        break;

      case RW_YANG_STMT_TYPE_GROUPING:
        pb_msg_type_ = PbMsgType::group;
        break;

      case RW_YANG_STMT_TYPE_LEAF:
      case RW_YANG_STMT_TYPE_LEAF_LIST:
        // ATTN: Will have to support unions as messages.
        RW_ASSERT_NOT_REACHED();

      case RW_YANG_STMT_TYPE_CHOICE:
      case RW_YANG_STMT_TYPE_CASE:
        // ATTN: Will have to support choice and case as first class someday.
        RW_ASSERT_NOT_REACHED();

      case RW_YANG_STMT_TYPE_RPCIO:
        if (pbnode_->get_yang_fieldname() == "input") {
          pb_msg_type_ = PbMsgType::rpc_input;
        } else {
          pb_msg_type_ = PbMsgType::rpc_output;
        }
        break;

      case RW_YANG_STMT_TYPE_NOTIF:
        pb_msg_type_ = PbMsgType::notif;
        break;

      default:
        RW_ASSERT_NOT_REACHED();
    }
  }
}

PbModel* PbMessage::get_pbmodel() const
{
  return pbnode_->get_pbmodel();
}

PbModule* PbMessage::get_pbmod() const
{
  return pbnode_->get_pbmod();
}

PbModule* PbMessage::get_schema_defining_pbmod() const
{
  return pbnode_->get_schema_defining_pbmod();
}

PbModule* PbMessage::get_lci_pbmod() const
{
  return pbnode_->get_lci_pbmod();
}

PbNode* PbMessage::get_pbnode() const
{
  return pbnode_;
}

PbMessage* PbMessage::get_parent_pbmsg() const
{
  return parent_pbmsg_;
}

PbMessage* PbMessage::get_rpc_input_pbmsg() const
{
  RW_ASSERT(PbMsgType::rpc_output == pb_msg_type_);
  PbModule* pbmod = get_pbmod();
  RW_ASSERT(pbmod);
  PbMessage* rpci_module_pbmsg = pbmod->get_rpci_module_pbmsg();
  RW_ASSERT(rpci_module_pbmsg);
  PbField* input_pbfld = rpci_module_pbmsg->get_field_by_name(pbnode_->get_proto_fieldname());
  RW_ASSERT(input_pbfld);
  PbMessage* input_pbmsg = input_pbfld->get_field_pbmsg();
  RW_ASSERT(input_pbmsg);
  return input_pbmsg;
}

PbMessage* PbMessage::get_rpc_output_pbmsg() const
{
  RW_ASSERT(PbMsgType::rpc_input == pb_msg_type_);
  PbModule* pbmod = get_pbmod();
  RW_ASSERT(pbmod);
  PbMessage* rpco_module_pbmsg = pbmod->get_rpco_module_pbmsg();
  RW_ASSERT(rpco_module_pbmsg);
  PbField* output_pbfld = rpco_module_pbmsg->get_field_by_name(pbnode_->get_proto_fieldname());
  RW_ASSERT(output_pbfld);
  PbMessage* output_pbmsg = output_pbfld->get_field_pbmsg();
  RW_ASSERT(output_pbmsg);
  return output_pbmsg;
}

YangNode* PbMessage::get_ynode() const
{
  return pbnode_->get_ynode();
}

std::string PbMessage::get_proto_msg_new_typename()
{
  if (0 == proto_msg_new_typename_.length()) {
    RW_ASSERT(parse_complete_);
    if (is_real_top_level_message()) {
      RW_ASSERT(is_chosen_eq_);
      proto_msg_new_typename_ = pbnode_->get_proto_msg_new_typename();
    } else {
      proto_msg_new_typename_ = get_proto_typename();
    }
    RW_ASSERT(proto_msg_new_typename_.length());
  }
  return proto_msg_new_typename_;
}

std::string PbMessage::get_proto_typename()
{
  if (0 == proto_typename_.length()) {
    RW_ASSERT(parse_complete_);
    RW_ASSERT(is_good(pb_msg_type_));
    switch (pb_msg_type_) {
      case PbMsgType::package:
      case PbMsgType::top_root:
      case PbMsgType::data_root:
      case PbMsgType::rpci_root:
      case PbMsgType::rpco_root:
      case PbMsgType::notif_root:
        RW_ASSERT_NOT_REACHED(); // not implemented

      case PbMsgType::message:
      case PbMsgType::notif:
      case PbMsgType::group:
        proto_typename_ = pbnode_->get_proto_msg_typename();
        break;

      case PbMsgType::rpc_input:
      case PbMsgType::rpc_output: {
        PbNode* parent_pbnode = pbnode_->get_parent_pbnode();
        RW_ASSERT(parent_pbnode);
        proto_typename_ = parent_pbnode->get_proto_msg_typename();
        break;
      }
      case PbMsgType::data_module:
      case PbMsgType::rpci_module:
      case PbMsgType::rpco_module:
      case PbMsgType::notif_module:
      case PbMsgType::group_root:
        proto_typename_ = get_pbmod()->get_proto_typename();
        break;

      case PbMsgType::illegal_:
      case PbMsgType::unset:
      case PbMsgType::automatic:
      case PbMsgType::last_:
        RW_ASSERT_NOT_REACHED();
    }
    RW_ASSERT(proto_typename_.length());
  }
  return proto_typename_;
}

PbMessage::path_list_t PbMessage::get_proto_message_path_list()
{
  path_list_t path_list;
  const char* schema_top = nullptr;
  bool add_mod = true;

  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::group:
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_proto_message_path_list();
      path_list.pop_front();
      path_list.emplace_front(get_lci_pbmod()->get_proto_package_name());
      path_list.emplace_back(get_proto_typename());
      break;

    case PbMsgType::data_module:
      schema_top = "YangData";
      goto top_common;
    case PbMsgType::rpci_module:
      schema_top = "YangInput";
      goto top_common;
    case PbMsgType::rpco_module:
      schema_top = "YangOutput";
      goto top_common;
    case PbMsgType::notif_module:
      schema_top = "YangNotif";
      goto top_common;
    case PbMsgType::group_root:
      schema_top = "YangGroup";
      add_mod = false;
      goto top_common;
    top_common:
      path_list.emplace_back(get_lci_pbmod()->get_proto_package_name());
      path_list.emplace_back(schema_top);
      if (add_mod) {
        path_list.emplace_back(get_proto_typename());
      }
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_proto_message_path()
{
  if (0 == proto_message_path_.length()) {
    for (auto const& s: get_proto_message_path_list()) {
      proto_message_path_ = proto_message_path_ + "." + s;
    }
  }
  return proto_message_path_;
}

PbMessage::path_list_t PbMessage::get_proto_schema_path_list(
  output_t output_style)
{
  path_list_t path_list;
  const char* schema_top = nullptr;

  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::group:
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_proto_schema_path_list(output_style);
      path_list.pop_front();
      path_list.emplace_front(get_lci_pbmod()->get_proto_package_name());
      path_list.emplace_back(get_proto_typename());
      break;

    case PbMsgType::data_module:
      schema_top = "Data";
      goto top_common;
    case PbMsgType::rpci_module:
      schema_top = "RpcInput";
      goto top_common;
    case PbMsgType::rpco_module:
      schema_top = "RpcOutput";
      goto top_common;
    case PbMsgType::notif_module:
      schema_top = "Notif";
      goto top_common;
    case PbMsgType::group_root:
      schema_top = "Group";
      goto top_common;
    top_common:
      path_list.emplace_back(get_lci_pbmod()->get_proto_package_name());
      switch (output_style) {
        case output_t::path_entry:
          path_list.emplace_back("YangKeyspecEntry");
          break;
        case output_t::dom_path:
          path_list.emplace_back("YangKeyspecDomPath");
          break;
        case output_t::path_spec:
          path_list.emplace_back("YangKeyspecPath");
          break;
        case output_t::list_only:
          path_list.emplace_back("YangListOnly");
          break;
        case output_t::message:
          schema_top = nullptr;
          switch (pb_msg_type_) {
            case PbMsgType::data_module:
              path_list.emplace_back("YangData");
              break;
            case PbMsgType::rpci_module:
              path_list.emplace_back("YangInput");
              break;
            case PbMsgType::rpco_module:
              path_list.emplace_back("YangOutput");
              break;
            case PbMsgType::notif_module:
              path_list.emplace_back("YangNotif");
              break;
            case PbMsgType::group_root:
              break;
            default:
              RW_ASSERT_NOT_REACHED();
          }
          break;
        default:
          RW_ASSERT_NOT_REACHED();
      }
      path_list.emplace_back(get_proto_typename());
      if (schema_top) {
        path_list.emplace_back(schema_top);
      }
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_proto_schema_path(
  output_t output_style)
{
  std::string proto_schema_path;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    proto_schema_path = proto_schema_path + "." + s;
  }
  return proto_schema_path;
}

std::string PbMessage::get_pbc_message_typename()
{
  if (0 == pbc_message_typename_.length()) {
    bool first = true;
    for (auto const& s: get_proto_message_path_list()) {
      pbc_message_typename_ = pbc_message_typename_ + (first?"":"__")
        + PbModel::mangle_typename(s);
      first = false;
    }
  }
  return pbc_message_typename_;
}

std::string PbMessage::get_pbc_message_global(
  const char* suffix)
{
  RW_ASSERT(suffix);
  if (0 == pbc_message_basename_.length()) {
    for (auto const& s: get_proto_message_path_list()) {
      pbc_message_basename_ = pbc_message_basename_
        + PbModel::mangle_to_lowercase(PbModel::mangle_to_underscore(s)) + "__";
    }
  }
  return pbc_message_basename_ + suffix;
}

std::string PbMessage::get_pbc_message_upper(
  output_t output_style,
  const char* suffix)
{
  std::string lower;
  switch (output_style) {
    case output_t::message:
      lower = get_pbc_message_global("");
      break;
    case output_t::path_entry:
    case output_t::dom_path:
    case output_t::path_spec:
    case output_t::list_only:
      lower = get_pbc_schema_global(output_style,"");
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return PbModel::mangle_to_uppercase(lower.c_str()) + suffix;
}

std::string PbMessage::get_pbc_schema_typename(
  output_t output_style)
{
  std::string pbc_schema_typename_base;
  bool first = true;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    pbc_schema_typename_base = pbc_schema_typename_base + (first?"":"__")
      + PbModel::mangle_typename(s);
    first = false;
  }
  return pbc_schema_typename_base;
}

std::string PbMessage::get_pbc_schema_desc_typename(
  output_t output_style)
{
  std::string pbc_schema_desc_typename = "RwProtobufCMessageDescriptor_";
  bool first = true;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    pbc_schema_desc_typename = pbc_schema_desc_typename
      + (first?"":"__") + PbModel::mangle_typename(s);
    first = false;
  }
  return pbc_schema_desc_typename;
}

std::string PbMessage::get_pbc_schema_global(
  output_t output_style,
  const char* suffix)
{
  RW_ASSERT(suffix);
  std::string pbc_schema_basename_base;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    pbc_schema_basename_base = pbc_schema_basename_base
      + PbModel::mangle_to_lowercase(PbModel::mangle_to_underscore(s)) + "__";
  }
  return pbc_schema_basename_base + suffix;
}

std::string PbMessage::get_pbc_schema_desc_global(
  output_t output_style,
  const char* suffix)
{
  RW_ASSERT(suffix);
  std::string pbc_schema_basename_base;
  bool first = true;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    pbc_schema_basename_base = pbc_schema_basename_base
      + PbModel::mangle_to_lowercase(PbModel::mangle_to_underscore(s)) + "__"
      + (first ? "pbcmd__" : "");
    first = false;
  }
  return pbc_schema_basename_base + suffix;;
}

PbMessage::path_list_t PbMessage::get_ypbc_path_list()
{
  path_list_t path_list;
  const char* schema_top = nullptr;
  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::group:
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_ypbc_path_list();
      path_list.pop_front();
      path_list.emplace_front(get_lci_pbmod()->get_proto_typename());
      path_list.emplace_back(get_proto_typename());
      break;

    case PbMsgType::data_module:
      schema_top = "data";
      goto top_common;
    case PbMsgType::rpci_module:
      schema_top = "input";
      goto top_common;
    case PbMsgType::rpco_module:
      schema_top = "output";
      goto top_common;
    case PbMsgType::notif_module:
      schema_top = "notif";
      goto top_common;
    case PbMsgType::group_root:
      schema_top = "group";
      goto top_common;
    top_common:
      path_list.emplace_back(get_lci_pbmod()->get_proto_typename());
      path_list.emplace_back(get_proto_typename());
      path_list.emplace_back(schema_top);
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

PbMessage::path_list_t PbMessage::get_ypbc_msg_new_path_list()
{
  path_list_t path_list;
  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::group:
      // ATTN: Does is_chosen_eq_ imply is_real_top_level_message()?
      if (is_real_top_level_message() && is_chosen_eq_) {
        path_list.emplace_back(get_lci_pbmod()->get_proto_typename());
        path_list.emplace_back(get_proto_msg_new_typename());
        break;
      }
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_ypbc_msg_new_path_list();
      if (path_list.size()) {
        path_list.pop_front();
        path_list.emplace_front(get_lci_pbmod()->get_proto_typename());
        path_list.emplace_back(get_proto_typename());
      }
      break;

    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
    case PbMsgType::group_root:
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_rwpb_long_alias()
{
  if (0 == ypbc_path_.length()) {
    bool first = true;
    for (auto const& s: get_ypbc_path_list()) {
      ypbc_path_ = ypbc_path_ + (first?"":"_") + s;
      first = false;
    }
    RW_ASSERT(ypbc_path_.length());
    // ATTN: Need a model function to register a path and verify uniqueness
  }
  return ypbc_path_;
}

std::string PbMessage::get_ypbc_long_alias()
{
  return std::string("rw_ypbc_") + get_rwpb_long_alias();
}

std::string PbMessage::get_rwpb_short_alias()
{
  path_list_t path_list = get_ypbc_path_list();
  RW_ASSERT(path_list.size() >= 2);
  std::string schmod = get_pbmodel()->get_schema_pbmod()->get_proto_typename();
  auto tok = path_list.begin();
  if (*tok != schmod) {
    return "";
  }
  if (*++tok != schmod) {
    return "";
  }

  std::string rwpb_short_alias;
  path_list.pop_front();
  RW_ASSERT(path_list.size());
  for (auto const& s: path_list) {
    rwpb_short_alias
      = rwpb_short_alias + (rwpb_short_alias.length() ? "_" : "") + s;
  }
  return rwpb_short_alias;
}

std::string PbMessage::get_ypbc_short_alias()
{
  std::string ypbc_short_alias = get_rwpb_short_alias();
  if (ypbc_short_alias.length()) {
    return std::string("rw_ypbc_") + ypbc_short_alias;
  }
  return "";
}

std::string PbMessage::get_rwpb_msg_new_alias()
{
  path_list_t path_list = get_ypbc_msg_new_path_list();
  if (!path_list.size()) {
    return "";
  }

  std::string rwpb_msg_new_alias = "";
  for (auto const& s: path_list) {
    rwpb_msg_new_alias
      = rwpb_msg_new_alias + (rwpb_msg_new_alias.length() ? "_" : "") + s;
  }
  return rwpb_msg_new_alias;
}

std::string PbMessage::get_ypbc_msg_new_alias()
{
  std::string ypbc_msg_new_alias = get_rwpb_msg_new_alias();
  if (ypbc_msg_new_alias.length()) {
    return std::string("rw_ypbc_") + ypbc_msg_new_alias;
  }
  return "";
}

std::string PbMessage::get_proto_msg_new_path()
{
  path_list_t path_list = get_ypbc_msg_new_path_list();
  if (!path_list.size()) {
    return "";
  }

  std::string msg_new_path = "";
  for (auto const& s: path_list) {
    if (msg_new_path != "")
      msg_new_path = msg_new_path + ".";
    msg_new_path = msg_new_path + s;
  }

  return msg_new_path;
}

std::string PbMessage::get_rwpb_imported_alias()
{
  path_list_t path_list = get_ypbc_path_list();
  RW_ASSERT(path_list.size() >= 3);
  std::string schmod = get_pbmodel()->get_schema_pbmod()->get_proto_typename();
  auto tok = path_list.begin();
  RW_ASSERT(*tok != schmod);

  std::string rwpb_imported_alias;
  path_list.pop_front();
  RW_ASSERT(path_list.size());
  path_list.emplace_front(schmod);
  for (auto const& s: path_list) {
    rwpb_imported_alias
      = rwpb_imported_alias + (rwpb_imported_alias.length() ? "_" : "") + s;
  }
  return rwpb_imported_alias;
}

std::string PbMessage::get_ypbc_imported_alias()
{
  std::string ypbc_imported_alias = get_rwpb_imported_alias();
  if (ypbc_imported_alias.length()) {
    return std::string("rw_ypbc_") + ypbc_imported_alias;
  }
  return "";
}

std::string PbMessage::get_rwpb_imported_msg_new_alias()
{
  path_list_t path_list = get_ypbc_msg_new_path_list();
  if (!path_list.size()) {
    return "";
  }

  RW_ASSERT(path_list.size() >= 2);
  std::string schmod = get_pbmodel()->get_schema_pbmod()->get_proto_typename();
  auto tok = path_list.begin();
  RW_ASSERT(*tok != schmod);

  std::string rwpb_imported_msg_new_alias;
  path_list.pop_front();
  RW_ASSERT(path_list.size());
  path_list.emplace_front(schmod);
  for (auto const& s: path_list) {
    rwpb_imported_msg_new_alias
      = rwpb_imported_msg_new_alias + (rwpb_imported_msg_new_alias.length() ? "_" : "") + s;
  }
  return rwpb_imported_msg_new_alias;
}

std::string PbMessage::get_ypbc_imported_msg_new_alias()
{
  std::string ypbc_imported_msg_new_alias = get_rwpb_imported_msg_new_alias();
  if (ypbc_imported_msg_new_alias.length()) {
    return std::string("rw_ypbc_") + ypbc_imported_msg_new_alias;
  }
  return "";
}

std::string PbMessage::get_ypbc_global(
  const char* discriminator)
{
  if (discriminator) {
    return std::string("rw_ypbc_") + get_rwpb_long_alias() + "_" + discriminator;
  }
  return std::string("rw_ypbc_") + get_rwpb_long_alias();
}

PbMessage::path_list_t PbMessage::get_gi_typename_path_list(
  output_t output_style)
{
  RW_ASSERT(is_good(pb_msg_type_));

  path_list_t path_list;
  if (output_t::message == output_style) {
    if (is_real_top_level_message() && is_chosen_eq_) {
      path_list.emplace_back(get_lci_pbmod()->get_proto_typename());
      path_list.emplace_back(get_proto_msg_new_typename());
      return path_list;
    }
  } else {
    return get_proto_schema_path_list(output_style);
  }

  const char* schema_top = nullptr;
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::group:

    case PbMsgType::message:
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_gi_typename_path_list();
      if (path_list.size()) {
        path_list.emplace_back(get_proto_typename());
      }
      break;

    case PbMsgType::data_module:
      schema_top = "YangData";
      goto top_common;
    case PbMsgType::rpci_module:
      schema_top = "YangInput";
      goto top_common;
    case PbMsgType::rpco_module:
      schema_top = "YangOutput";
      goto top_common;
    case PbMsgType::notif_module:
      schema_top = "YangNotif";
      goto top_common;
    case PbMsgType::group_root:
      schema_top = "YangGroup";
      goto top_common;
    top_common:
      path_list.emplace_back(get_lci_pbmod()->get_proto_typename());
      path_list.emplace_back(schema_top);
      path_list.emplace_back(get_proto_typename());
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_gi_typename(
  output_t output_style)
{
  path_list_t path_list = get_gi_typename_path_list(output_style);
  RW_ASSERT(path_list.size() >= 2);

  std::string gi_typename = "rwpb_gi";
  for (auto const& s: path_list) {
    gi_typename = gi_typename + "_" + PbModel::mangle_typename(s);
  }
  return gi_typename;
}

std::string PbMessage::get_gi_desc_typename(
  output_t output_style)
{
  std::string gi_desc_typename;
  bool first = true;
  for (auto const& s: get_proto_schema_path_list(output_style)) {
    gi_desc_typename
      =    gi_desc_typename
         + (first?"rwpb_gi_":"_")
         + PbModel::mangle_typename(s)
         + (first?"_PBCMD":"");
    first = false;
  }
  return gi_desc_typename;
}

std::string PbMessage::get_gi_python_typename(
  output_t output_style)
{
  path_list_t path_list = get_gi_typename_path_list(output_style);
  RW_ASSERT(path_list.size() >= 2);

  std::string gi_python_typename = *path_list.begin() + "Yang.";
  path_list.pop_front();

  const char* sep = "";
  for (auto const& s: path_list) {
    gi_python_typename = gi_python_typename + sep + PbModel::mangle_typename(s);
    sep = "_";
  }
  return gi_python_typename;
}

std::string PbMessage::get_gi_c_identifier(
    const char* operation,
    output_t output_style,
    const char* field)
{
  std::string gi_c_identifier = get_pbc_schema_global(output_style, "gi");
  if (operation) {
    gi_c_identifier = gi_c_identifier + "_" + operation;
  }
  if (field) {
    gi_c_identifier = gi_c_identifier + "_" + field;
  }
  return gi_c_identifier;
}

std::string PbMessage::get_gi_c_desc_identifier(
    const char* operation,
    output_t output_style,
    const char* field)
{
  std::string gi_c_desc_identifier = get_pbc_schema_desc_global(output_style, "gi");
  if (operation) {
    gi_c_desc_identifier = gi_c_desc_identifier + "_" + operation;
  }
  if (field) {
    gi_c_desc_identifier = gi_c_desc_identifier + "_" + field;
  }
  return gi_c_desc_identifier;
}

PbMessage::path_list_t PbMessage::get_xpath_path_list()
{
  path_list_t path_list;
  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
    case PbMsgType::group_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif: {
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_xpath_path_list();
      auto element = pbnode_->get_xpath_element_name();
      if (element.length()) {
        path_list.emplace_back(element);
      }
      break;
    }
    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
    case PbMsgType::group:
      // Tops are empty.
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_xpath_path()
{
  if (0 == xpath_path_.length()) {
    for (auto const& s: get_xpath_path_list()) {
      xpath_path_ = xpath_path_ + "/" + s;
    }
    RW_ASSERT(xpath_path_.length());
  }
  return xpath_path_;
}

PbMessage::path_list_t PbMessage::get_xpath_keyed_path_list()
{
  path_list_t path_list;
  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
    case PbMsgType::group_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif: {
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_xpath_keyed_path_list();
      auto element = pbnode_->get_xpath_element_with_predicate();
      if (element.length()) {
        path_list.emplace_back(element);
      }
      break;
    }
    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
    case PbMsgType::group:
      // Tops are empty.
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_xpath_keyed_path()
{
  if (0 == xpath_keyed_path_.length()) {
    for (auto const& s: get_xpath_keyed_path_list()) {
      xpath_keyed_path_ = xpath_keyed_path_ + "/" + s;
    }
    RW_ASSERT(xpath_keyed_path_.length());
  }
  return xpath_keyed_path_;
}

PbMessage::path_list_t PbMessage::get_rwrest_uri_path_list()
{
  path_list_t path_list;
  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
    case PbMsgType::group_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif: {
      RW_ASSERT(parent_pbmsg_);
      path_list = parent_pbmsg_->get_rwrest_uri_path_list();
      auto element = pbnode_->get_rwrest_path_element();
      if (element.length()) {
        path_list.emplace_back(element);
      }
      break;
    }
    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
    case PbMsgType::group:
      // Tops are empty.
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return path_list;
}

std::string PbMessage::get_rwrest_uri_path()
{
  if (0 == rwrest_uri_path_.length()) {
    for (auto const& s: get_rwrest_uri_path_list()) {
      rwrest_uri_path_ = rwrest_uri_path_ + "/" + s;
    }
    RW_ASSERT(rwrest_uri_path_.length());
  }
  return rwrest_uri_path_;
}

PbField* PbMessage::get_field_by_name(std::string fieldname) const
{
  return field_stab_.get_symbol(fieldname);
}

PbMessage::field_citer_t PbMessage::field_begin() const
{
  return field_list_.begin();
}

PbMessage::field_citer_t PbMessage::field_end() const
{
  return field_list_.end();
}

unsigned PbMessage::has_fields() const
{
  return field_list_.size();
}

unsigned PbMessage::has_messages() const
{
  return message_children_;
}

rw_yang_stmt_type_t PbMessage::get_yang_stmt_type() const
{
  return pbnode_->get_yang_stmt_type();
}

void PbMessage::set_pbeq(PbEqMsgSet* pbeq)
{
  // Can only do this once.  Just before parsing completes.
  RW_ASSERT(!parse_complete_);
  RW_ASSERT(pbeq);
  RW_ASSERT(!pbeq_);
  pbeq_ = pbeq;
}

PbEqMsgSet* PbMessage::get_pbeq() const
{
  RW_ASSERT(pbeq_);
  return pbeq_;
}

PbMessage* PbMessage::get_eq_pbmsg()
{
  RW_ASSERT(parse_complete_);
  if (is_root_message()) {
    return this;
  }

  RW_ASSERT(pbeq_);
  PbMessage* eq_pbmsg = pbeq_->get_preferred_msg(get_lci_pbmod());
  if (eq_pbmsg) {
    return eq_pbmsg;
  }

  return this;
}

void PbMessage::set_chosen_eq()
{
  is_chosen_eq_ = true;
}

bool PbMessage::has_conflict() const
{
  return pbeq_->has_conflict(get_pbmodel()->get_schema_pbmod());
}

bool PbMessage::is_parse_complete()
{
  return parse_complete_;
}

bool PbMessage::is_root_message() const
{
  if (parent_pbmsg_) {
    return false;
  }
  RW_ASSERT(get_pbmodel()->get_ymodel()->get_root_node() == pbnode_->get_ynode());
  return true;
}

bool PbMessage::is_top_level_message() const
{
  return pbnode_->get_yext_msg_new();
}

bool PbMessage::is_local_message() const
{
  return get_lci_pbmod() == get_pbmodel()->get_schema_pbmod();
}

bool PbMessage::is_real_top_level_message() const
{
  RW_ASSERT(pbeq_);
  return is_top_level_message() && !pbeq_->has_conflict(get_lci_pbmod());
}

bool PbMessage::is_local_top_level_message() const
{
  return is_local_message() && is_real_top_level_message();
}

bool PbMessage::is_scoped_message()
{
  RW_ASSERT(parse_complete_);
  if (is_root_message()) {
    return true;
  }
  if (is_real_top_level_message()) {
    return false;
  }
  if (get_eq_pbmsg() == this) {
    return true;
  }
  return false;
}

bool PbMessage::is_local_scoped_message()
{
  return is_local_message() && is_scoped_message();
}

bool PbMessage::is_local_defining_message()
{
  return    !is_pbc_suppressed()
         && is_local_message()
         && get_eq_pbmsg() == this;
}

bool PbMessage::is_local_schema_defining_message() const
{
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::group:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::message:
    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
    case PbMsgType::group_root:
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return    is_local_message()
         || pbnode_->get_schema_defining_pbmod() == get_pbmodel()->get_schema_pbmod();
}

bool PbMessage::is_pbc_suppressed(output_t output_style) const
{
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::data_module:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::notif_module:
      switch (output_style) {
        case output_t::message:
          break;
        default:
          return true;
      }
      break;

    case PbMsgType::group_root:
      return true;

    case PbMsgType::group:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::message:
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return false;
}

bool PbMessage::is_schema_suppressed() const
{
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::data_module:
    case PbMsgType::message:
    case PbMsgType::rpci_module:
    case PbMsgType::rpc_input:
    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
    case PbMsgType::notif_module:
    case PbMsgType::notif:
    case PbMsgType::group:
      break;

    case PbMsgType::group_root:
      return true;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
  return false;
}

PbMsgType PbMessage::get_pb_msg_type() const
{
  return pb_msg_type_;
}

PbMsgType PbMessage::get_category_msg_type() const
{
  PbMsgType retval = pb_msg_type_;
  if (PbMsgType::message != retval) {
    return retval;
  }
  if (parent_pbmsg_) {
    retval = parent_pbmsg_->get_category_msg_type();
  }
  return retval;
}

bool PbMessage::is_equivalent(
  PbMessage* other_pbmsg,
  const char** reason,
  PbField** field,
  PbModule* schema_pbmod) const
{
  RW_ASSERT(other_pbmsg);

  const char* junk = nullptr;
  if (nullptr == reason) {
    reason = &junk;
  }
  *reason = nullptr;
  PbField* junkf = nullptr;
  if (nullptr == field) {
    field = &junkf;
  }
  *field = nullptr;

  // Quick tests
  if (   (ypbc_msg_type_ != other_pbmsg->ypbc_msg_type_
          && (*reason = "message type"))
      || (message_children_ != other_pbmsg->message_children_
          && (*reason = "number of child messages"))
      || (field_list_.size() != other_pbmsg->field_list_.size()
          && (*reason = "number of fields"))) {
    return false;
  }

  if (!pbnode_->is_equivalent(other_pbmsg->pbnode_, reason, schema_pbmod)) {
    return false;
  }

  // Hard tests - have to compare all the fields
  auto a = field_list_.begin();
  auto b = other_pbmsg->field_list_.begin();
  while (a != field_list_.end() && b != other_pbmsg->field_list_.end()) {
    PbField* a_pbfld = a->get();
    PbField* b_pbfld = b->get();

    if (schema_pbmod) {
      bool a_is_in = schema_pbmod->equals_or_imports_transitive(a_pbfld->get_pbmod());
      bool b_is_in = schema_pbmod->equals_or_imports_transitive(b_pbfld->get_pbmod());
      if (a_is_in != b_is_in) {
        *field = a_pbfld;
        *reason = "differential augmentation/uses inclusion";
        return false;
      }
    }

    if (!a_pbfld->is_equivalent(b_pbfld, reason, schema_pbmod)) {
      *field = a_pbfld;
      return false;
    }
    ++a;
    ++b;
  }

  RW_ASSERT(a == field_list_.end() && b == other_pbmsg->field_list_.end());
  return true;
}

PbMessage* PbMessage::equivalent_which_comes_first(
  PbMessage* other_pbmsg,
  PbModule* schema_pbmod)
{
  RW_ASSERT(this != other_pbmsg); // makes no sense to compare self

  RW_ASSERT(other_pbmsg);
  PbNode* this_pbnode = pbnode_;
  RW_ASSERT(this_pbnode);
  PbNode* other_pbnode = other_pbmsg->pbnode_;
  RW_ASSERT(other_pbnode);

  /*
   * Start by comparing the module ordering.  Prefer the module that is
   * ordered first, by import dependencies.
   */
  PbModule* this_lci_pbmod = this_pbnode->get_lci_pbmod();
  RW_ASSERT(this_lci_pbmod);
  PbModule* other_lci_pbmod = other_pbnode->get_lci_pbmod();
  RW_ASSERT(other_lci_pbmod);
  if (this_lci_pbmod != other_lci_pbmod) {
    if (this_lci_pbmod->is_ordered_before(other_lci_pbmod)) {
      return this;
    }
    return other_pbmsg;
  }

  /*
   * Next, check the message category node type.  This is the node type
   * of the first ancestor that does not have the message node type.
   * The category matters because, within a single module, the highest
   * level ordering of messages depends on category.
   */
  PbMsgType this_type = get_category_msg_type();
  PbMsgType other_type = other_pbmsg->get_category_msg_type();
  if (other_type < this_type) {
    return other_pbmsg;
  } else if (this_type < other_type) {
    return this;
  }

  /*
   * Same module and same category.  Find the deepest message in
   * common, and then order these messages by their definition order
   * within the common message.
   *
   * This code is allowed to assume that the two messages do not have a
   * ancestor-descendant relationship.  Such a relationship would
   * violate the assumed equivalence, and so it doesn't make sense.
   */
  RW_ASSERT(is_equivalent(other_pbmsg, nullptr, nullptr, schema_pbmod));

  std::map<PbMessage*,PbMessage*> this_parent_child;
  for (PbMessage* child = this, *parent = parent_pbmsg_;
       parent;
       (child = parent), (parent = parent->parent_pbmsg_)) {
    auto status = this_parent_child.emplace(parent,child);
    RW_ASSERT(status.second);
  }

  // Now find the first common ancestor with other.
  for (PbMessage* other_child = other_pbmsg, *other_parent = other_pbmsg->parent_pbmsg_;
       other_parent;
       (other_child = other_parent), (other_parent = other_parent->parent_pbmsg_)) {
    auto pci = this_parent_child.find(other_parent);
    if (pci != this_parent_child.end()) {
      /*
       * Found the common ancestor message.  Search the fields to
       * determine which one comes first.
       */
      for (auto fi = other_parent->field_begin();
           fi != other_parent->field_end();
           ++fi) {
        PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
        if (other_child == field_pbmsg) {
          return other_pbmsg;
        }
        if (pci->second == field_pbmsg) {
          return this;
        }
      }
      RW_ASSERT_NOT_REACHED(); // should have found it
    }
  }
  RW_ASSERT_NOT_REACHED(); // should have found an ancestor node
}

uint32_t PbMessage::get_pbc_size()
{
  if (pbc_size_) {
    return pbc_size_;
  }

  const uint32_t pad = sizeof(int);
  uint32_t padding_left = 0;
  pbc_size_ = sizeof(ProtobufCMessage);

  for (const auto& fi: field_list_) {
    auto size = fi->get_pbc_size();
    RW_ASSERT(size.align_size);
    RW_ASSERT(size.field_size);

    if (size.has_size <= padding_left) {
      // Place has in the padding
      padding_left -= size.has_size;
      pbc_size_ += size.has_size;
    } else if (size.has_size) {
      // Eat the remaining padding and insert the has field
      pbc_size_ += padding_left;
      pbc_size_ += size.has_size;
      padding_left = pad - size.has_size;
    }

    // Eat some padding?
    uint32_t extra_padding = padding_left % size.align_size;
    if (extra_padding) {
      padding_left -= extra_padding;
      pbc_size_ += extra_padding;
    }

    pbc_size_ += size.field_size;
    padding_left = (pad - (pbc_size_ & (pad-1))) & (pad-1);
  }
  return pbc_size_;
}

void PbMessage::validate_pbc_size()
{
  uint32_t pbc_size = get_pbc_size();

  if (pbc_size > pbnode_->get_pbc_size_error_limit()) {
    std::ostringstream oss;
    oss << "The protobuf-c struct " << pbnode_->get_proto_msg_typename()
        << " has " << pbc_size << " bytes, which is more than the allowed max "
        << pbnode_->get_pbc_size_error_limit();
    get_pbmodel()->error(get_ynode()->get_location(), oss.str());
    return;
  }

  if (pbc_size > pbnode_->get_pbc_size_warning_limit()) {
    std::ostringstream oss;
    oss << "The protobuf-c struct " << pbnode_->get_proto_msg_typename()
        << " has " << pbc_size << " bytes, which is more than the preferred max "
        << pbnode_->get_pbc_size_warning_limit();
    get_pbmodel()->warning(get_ynode()->get_location(), oss.str());
  }
}

PbField* PbMessage::add_field(
  PbNode* field_pbnode,
  PbMsgType pb_msg_type)
{
  RW_ASSERT(field_pbnode);
  PbField* pbfld = new PbField(this, field_pbnode, nullptr);
                               
  field_list_.emplace_back(pbfld);
  return pbfld;
}

void PbMessage::parse()
{
  /*
   * Parsing steps:
   *  - Parse the node tree in its entirety.
   *  - Create top-level messages for the message parse (schema-level,
   *    module-level).
   *  - Parse the known (schema, module) messages:
   *     - Message parsers filter the node tree to the nodes that apply
   *       to the message type.
   *     - For each node that passes the filter, call add_field().
   *        - If there is a need to have a special message type, create
   *          the message and save it against the field.  Otherwise,
   *          the message will be created automatically when parsing
   *          the field.
   *     - Once all the fields have been created, issue parse_start on
   *       all of them.
   *     - Once parse_start completes on all the fields, issue
   *       parse_complete on all of them.
   */
  switch (pb_msg_type_) {
    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
      // these are bugs
      RW_ASSERT_NOT_REACHED();

    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::notif_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
      // these are not implemented
      RW_ASSERT_NOT_REACHED();

    case PbMsgType::group_root:
      parse_pb_msg_type_group_root();
      break;

    case PbMsgType::message:
    case PbMsgType::group:
    case PbMsgType::data_module:
    case PbMsgType::notif:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
      parse_pb_msg_type_message();
      break;

    case PbMsgType::notif_module:
      parse_pb_msg_type_notif_module();
      break;

    case PbMsgType::rpci_module:
      parse_pb_msg_type_rpci_module();
      break;

    case PbMsgType::rpco_module:
      parse_pb_msg_type_rpco_module();
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }
}

bool PbMessage::generate_field_descs() const
{
  switch (pb_msg_type_) {
    case PbMsgType::group:
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
    case PbMsgType::notif:
    case PbMsgType::message:
    case PbMsgType::data_module:
    case PbMsgType::notif_module:
      return true;
    default:
      break;
  }

  return false;
}

void PbMessage::parse_pb_msg_type_message()
{
  parse_start();
  parse_add_children_filter(stmt_mask_data());
  parse_complete();
}

void PbMessage::parse_pb_msg_type_group_root()
{
  parse_start();
  // ATTN: needed for grouping
  // ATTN: parse_add_children_filter(stmt_mask_grouping());
  parse_complete();

  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    get_pbmod()->save_top_level_grouping((*fi)->get_pbnode(), (*fi)->get_field_pbmsg());
  }
}

void PbMessage::parse_pb_msg_type_rpci_module()
{
  parse_start();
  parse_add_children_rpc("input");
  parse_complete();
}

void PbMessage::parse_pb_msg_type_rpco_module()
{
  parse_start();
  parse_add_children_rpc("output");
  parse_complete();
}

void PbMessage::parse_pb_msg_type_notif_module()
{
  parse_start();
  parse_add_children_filter(stmt_mask_notification());
  parse_complete();
}

void PbMessage::parse_start()
{
  RW_ASSERT(!parse_started_);
  parse_started_ = true;

  const rw_yang_stmt_type_t stmt_type = get_yang_stmt_type();
  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_ROOT:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_MODULE_ROOT;
      break;

    case RW_YANG_STMT_TYPE_CONTAINER:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_CONTAINER;
      break;

    case RW_YANG_STMT_TYPE_LIST:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_LIST;
      break;

    case RW_YANG_STMT_TYPE_LEAF_LIST:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_LEAF_LIST;
      break;

    case RW_YANG_STMT_TYPE_RPCIO:
      if (std::string("input") == pbnode_->get_yang_fieldname()) {
        ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_RPC_INPUT;
      } else {
        ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_RPC_OUTPUT;
      }
      break;

    case RW_YANG_STMT_TYPE_GROUPING:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_GROUPING;
      break;

    case RW_YANG_STMT_TYPE_NOTIF:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_NOTIFICATION;
      break;

    case RW_YANG_STMT_TYPE_RPC:
      ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_NULL;
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }
}

void PbMessage::parse_add_children_filter(
  uint64_t filter_mask)
{
  RW_ASSERT(parse_started_);
  RW_ASSERT(!parse_complete_);
  RW_ASSERT(filter_mask);

  for (auto ni = pbnode_->child_begin(); ni != pbnode_->child_end(); ++ni) {
    PbNode* child_pbnode = *ni;
    rw_yang_stmt_type_t stmt_type = child_pbnode->get_yang_stmt_type();
    RW_ASSERT(rw_yang_stmt_type_is_good(stmt_type));
    uint64_t mask = uint64_t(1) << rw_yang_stmt_type_to_index(stmt_type);
    if (filter_mask & mask) {
      add_field(child_pbnode);
    }
  }
}

void PbMessage::parse_add_children_rpc(
  const char* type)
{
  RW_ASSERT(type);

  for (auto ni = pbnode_->child_begin(); ni != pbnode_->child_end(); ++ni) {
    PbNode* child_pbnode = *ni;
    rw_yang_stmt_type_t stmt_type = child_pbnode->get_yang_stmt_type();
    if (stmt_type == RW_YANG_STMT_TYPE_RPC) {
      YangNode* rpc_ynode = child_pbnode->get_ynode();
      RW_ASSERT(rpc_ynode);
      YangNode* type_ynode = rpc_ynode->search_child(type);
      RW_ASSERT(type_ynode);
      PbNode* type_pbnode = get_pbmodel()->lookup_node(type_ynode);
      RW_ASSERT(type_pbnode);
      add_field(type_pbnode);
    }
  }
}

void PbMessage::parse_complete()
{
  RW_ASSERT(parse_started_);
  RW_ASSERT(!parse_complete_);

  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    PbField* pbfld = fi->get();
    PbField* other_pbfld = field_stab_.add_symbol(pbfld->get_proto_fieldname(), pbfld);
    if (other_pbfld) {
      std::ostringstream oss;
      YangNode* other_ynode = other_pbfld->get_ynode();
      YangNode* ynode = pbfld->get_ynode();
      RW_ASSERT(other_ynode);
      oss << "Duplicate mangled field names " << pbfld->get_proto_fieldname()
          << ": " << ynode->get_name()
          << " duplicates " << other_ynode->get_name()
          << " at " << other_ynode->get_location();
      get_pbmodel()->error(ynode->get_location(), oss.str());
    }

    (*fi)->parse_start();
  }

  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    (*fi)->parse_complete();
    if ((*fi)->get_field_pbmsg()) {
      ++message_children_;
    }
  }

  get_pbmodel()->get_pbeqmgr()->parse_equivalence(this);
  validate_pbc_size();

  parse_complete_ = true;
}

void PbMessage::find_schema_imports()
{
  RW_ASSERT(parse_complete_);
  PbModule* schema_pbmod = get_pbmodel()->get_schema_pbmod();

  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
    if (field_pbmsg) {
      PbModule* lci_pbmod = field_pbmsg->get_lci_pbmod();
      if (lci_pbmod != schema_pbmod) {
        lci_pbmod->need_to_import();
      } else {
        field_pbmsg->find_schema_imports();
      }
      continue;
    }

    PbEnumType* pbenum = (*fi)->get_pbnode()->get_pbenum();
    if (pbenum) {
      pbenum->get_pbmod()->need_to_import();
      continue;
    }
  }
}

uint32_t PbMessage::get_event_id() const
{
  if (get_yang_stmt_type() == RW_YANG_STMT_TYPE_NOTIF) {
    return (pbnode_->get_event_id());
  }
  else if (parent_pbmsg_) {
      return(parent_pbmsg_->get_event_id());
  }
  return(0);
}

void PbMessage::output_proto_message(output_t output_style, std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  rw_yang_stmt_type_t stmt_type = get_yang_stmt_type();

  bool suppress = false;
  std::ostringstream opts;

  bool schema_top = false;
  switch (output_style) {
    case output_t::path_entry:
    case output_t::path_spec:
    case output_t::dom_path:
    case output_t::list_only:
      switch (pb_msg_type_) {
        case PbMsgType::data_module:
        case PbMsgType::rpci_module:
        case PbMsgType::rpco_module:
        case PbMsgType::notif_module:
          if (is_local_schema_defining_message()) {
            break;
          }
          // else fall through
        case PbMsgType::group_root:
          schema_top = true;
          break;
        default:
          break;
      }
    default:
      break;
  }

  std::string proto_typename;
  switch (output_style) {
    case output_t::message:
      proto_typename = get_proto_typename();
      if (PbMsgType::group_root == pb_msg_type_) {
        suppress = true;
      }
      if (!suppress) {
        opts << " ypbc_msg:\"" << get_ypbc_global("g_msg_msgdesc") << "\"";
        if (pbnode_->is_flat()) {
          opts << " flat:true";
        }
        if (pbnode_->has_keys()) {
          opts << " has_keys:true";
        }
        if (pbnode_->get_yext_notif_log_event_id()) {
          opts << " log_event_type:true";
        }
        PbMessage* eq_pbmsg = get_eq_pbmsg();
        if (this != eq_pbmsg) {
          opts << " c_typedef:\"" << eq_pbmsg->get_pbc_message_typename() << "\"";
        }
        /*
           ATTN: TGS: use c_typename to rename top-level things?
         */
        std::string msg_new_path = get_proto_msg_new_path();
        if (msg_new_path.length()) {
          opts << " msg_new_path:\"" << msg_new_path << "\"";
        }
      }
      break;

    case output_t::path_entry:
      proto_typename = get_proto_typename();
      if (!schema_top) {
        opts << " ypbc_msg:\"" << get_ypbc_global("g_entry_msgdesc") << "\"";
        opts << " base_typename:\"rw_keyspec_entry_t\"";
      }
      break;

    case output_t::path_spec:
      proto_typename = get_proto_typename();
      if (!schema_top) {
        opts << " ypbc_msg:\"" << get_ypbc_global("g_path_msgdesc") << "\"";
        opts << " base_typename:\"rw_keyspec_path_t\"";
      }
      break;

    case output_t::dom_path:
      // No specialized msgdesc
      proto_typename = get_proto_typename();
      break;

    case output_t::list_only:
      proto_typename = get_proto_typename();
      if (RW_YANG_STMT_TYPE_LIST != stmt_type) {
        suppress = true;
      }
      if (!suppress) {
        opts << " ypbc_msg:\"" << get_ypbc_global("g_list_msgdesc") << "\"";
      }
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (schema_top || suppress) {
    opts << " suppress:true";
  }

  switch (output_style) {
    case output_t::path_entry:
    case output_t::path_spec:
    case output_t::dom_path:
    case output_t::list_only:
      switch (pb_msg_type_) {
        case PbMsgType::data_module:
          proto_typename = "Data";
          break;
        case PbMsgType::rpci_module:
          proto_typename = "RpcInput";
          break;
        case PbMsgType::rpco_module:
          proto_typename = "RpcOutput";
          break;
        case PbMsgType::notif_module:
          proto_typename = "Notif";
          break;
        case PbMsgType::group_root:
          proto_typename = "Group";
          break;
        default:
          break;
      }
    default:
      break;
  }

  os << pad1 << "message " << proto_typename << std::endl;
  os << pad1 << "{" << std::endl;

  if (opts.str().size()) {
    os << pad2 << "option (rw_msgopts) = {" << opts.str() << " };" << std::endl;
    os << std::endl;
  }

  // Output the embedded messages, if any
  switch (output_style) {
    case output_t::message:
      for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
        PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
        if (field_pbmsg &&
            field_pbmsg->is_local_schema_defining_message() &&
            field_pbmsg->get_yang_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST) { // No proto message for leaf-list
          field_pbmsg->output_proto_message(output_style, os, indent+2);
          os << std::endl;
        }
      }
      break;

    case output_t::path_entry:
    case output_t::dom_path:
    case output_t::path_spec:
      for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
        PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
        if (field_pbmsg && field_pbmsg->is_local_schema_defining_message()) {
          field_pbmsg->output_proto_message(output_style, os, indent+2);
          os << std::endl;
        }
      }
      break;

    case output_t::list_only:
      for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
        PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
        if (   field_pbmsg
            && field_pbmsg->is_local_schema_defining_message()
            && field_pbmsg->get_pbnode()->has_list_descendants()) {
          field_pbmsg->output_proto_message(output_style, os, indent+2);
          os << std::endl;
        }
      }
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  // Output the key mesages.
  if (   output_t::path_entry == output_style
      && is_local_schema_defining_message()) {

    if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
      unsigned key_n = 0;
      for (YangKeyIter yki = get_ynode()->key_begin(); yki != get_ynode()->key_end(); ++yki, ++key_n) {
        // Find the yang node
        YangNode* yknode = yki->get_key_node();
        RW_ASSERT(yknode);

        // Find the pb node
        PbNode* pbnode = get_pbmodel()->lookup_node(yknode);
        RW_ASSERT(pbnode);

        // Find the field
        PbField* pbfld = field_stab_.get_symbol(pbnode->get_proto_fieldname());
        RW_ASSERT(pbfld);

        // And output the message...
        pbfld->output_proto_key_message(os, indent+2);
        os << std::endl;
      }
    } else if (RW_YANG_STMT_TYPE_LEAF_LIST == stmt_type) {
      // Find my own field.
      PbField* my_pbfld = parent_pbmsg_->get_field_by_name(pbnode_->get_proto_fieldname());
      my_pbfld->output_proto_key_message(os, indent+2);
      os << std::endl;
    }
  }

  switch (output_style) {
    case output_t::message:
      for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
        (*fi)->output_proto_field(os, indent+2);
      }
      break;

    case output_t::path_entry:
      if (schema_top) {
        break;
      }
      os << pad2 << "required RwSchemaElementId element_id = 1 [ (rw_fopts) = {inline:true} ];" << std::endl;

      // Output sub-messages for the keys
      if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
        unsigned key_n = 0;
        for (YangKeyIter yki = get_ynode()->key_begin(); yki != get_ynode()->key_end(); ++yki, ++key_n) {
          YangNode* yknode = yki->get_key_node();
          RW_ASSERT(yknode);
          PbNode* pbnode = get_pbmodel()->lookup_node(yknode);
          RW_ASSERT(pbnode);
          PbField* pbfld = field_stab_.get_symbol(pbnode->get_proto_fieldname());
          RW_ASSERT(pbfld);
          pbfld->output_proto_key_field(key_n, os, indent+2);
        }
      } else if (RW_YANG_STMT_TYPE_LEAF_LIST == stmt_type) {
        PbField* my_pbfld = parent_pbmsg_->get_field_by_name(pbnode_->get_proto_fieldname());
        my_pbfld->output_proto_key_field(0, os, indent+2);
      }
      break;

    case output_t::dom_path: {
      if (schema_top) {
        break;
      }
      os << pad2 << "required bool rooted = 1;" << std::endl;
      os << pad2 << "optional RwSchemaCategory category = 2;" << std::endl;

      std::list<PbMessage*> path;
      for (PbMessage* m = this; m; m = m->parent_pbmsg_) {
        if (!m->is_root_message()) {
          path.emplace_front(m);
        }
      }
      unsigned path_n = 0;
      unsigned tag_n = 100;
      for (auto const& pi: path) {
        char buf[4096];
        int bytes = snprintf(buf, sizeof(buf), "path%03u = %u", path_n, tag_n);
        RW_ASSERT(bytes > 0 && bytes < (int)sizeof(buf));
        os << pad2 << "optional " << pi->get_proto_schema_path(output_t::path_entry)
           << " " << buf << " [ (rw_fopts) = {inline:true} ];" << std::endl;
        ++path_n;
        ++tag_n;
      }

      if (is_root_message()) {
        switch (pb_msg_type_) {
          case PbMsgType::data_module:
          case PbMsgType::rpci_module:
          case PbMsgType::rpco_module:
          case PbMsgType::notif_module:
            os << pad2 << "optional " << get_proto_schema_path(output_t::path_entry)
               << " path_module = 99 [ (rw_fopts) = {inline:true} ];" << std::endl;
            break;
          default:
            RW_ASSERT_NOT_REACHED(); // Other roots not supported now!
        }
      }
      /*
       * Output an additional path entry to be able to refer to any leaf's
       * inside the message.
       */
      if (RW_YANG_STMT_TYPE_LEAF_LIST != stmt_type &&
          RW_YANG_STMT_TYPE_ROOT      != stmt_type) {
        char buf[512];
        int bytes = snprintf(buf, sizeof(buf), "path%03u = %u", path_n, tag_n);
        RW_ASSERT((bytes > 0) && (bytes < (int)sizeof(buf)));
        os << pad2 << "optional RwSchemaPathEntry " << buf << ";" << std::endl;
      }
      break;
    }
    case output_t::path_spec:
      if (schema_top) {
        break;
      }
      os << pad2 << "optional " << get_proto_schema_path(output_t::dom_path)
         << " dompath = 1 [ (rw_fopts) = {inline:true} ];" << std::endl;
      os << pad2 << "optional bytes binpath = 2;" << std::endl;
      os << pad2 << "optional string strpath = 99;" << std::endl;
      break;

    case output_t::list_only:
      if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
        os << pad2 << "repeated " << get_proto_message_path() << " " << pbnode_->get_proto_fieldname()
           << " = " << pbnode_->get_tag_number() << ";" << std::endl;
      }
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  os << pad1 << "}" << std::endl;
}

void PbMessage::output_h_struct_tags(std::ostream& os, unsigned indent)
{
  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg) {
      child_pbmsg->output_h_struct_tags(os, indent);
    }
  }

  std::string pad1(indent,' ');
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::rpc_input:
      if (is_local_defining_message()) {
        os << pad1 << "typedef struct " << get_ypbc_global("t_rpcdef") << " "
           << get_ypbc_global("t_rpcdef") << ";" << std::endl;
      }
      goto also_message;

    case PbMsgType::data_module:
    case PbMsgType::message:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
    case PbMsgType::notif_module:
    case PbMsgType::notif:
    case PbMsgType::group:
    also_message:
      if (is_local_schema_defining_message()) {
        os << pad1 << "typedef struct " << get_ypbc_global("t_msg_msgdesc") << " "
           << get_ypbc_global("t_msg_msgdesc") << ";" << std::endl;
        if (!is_schema_suppressed()) {
          os << pad1 << "typedef struct " << get_ypbc_global("t_entry_msgdesc") << " "
             << get_ypbc_global("t_entry_msgdesc") << ";" << std::endl;
          os << pad1 << "typedef struct " << get_ypbc_global("t_path_msgdesc") << " "
             << get_ypbc_global("t_path_msgdesc") << ";" << std::endl;

          if (RW_YANG_STMT_TYPE_LIST == get_yang_stmt_type()) {
            os << pad1 << "typedef struct " << get_ypbc_global("t_list_msgdesc") << " "
               << get_ypbc_global("t_list_msgdesc") << ";" << std::endl;
          }
        }
      }
      break;

    case PbMsgType::group_root:
      if (is_local_message() && has_messages()) {
        os << pad1 << "typedef struct " << get_ypbc_global("t_group_root") << " "
           << get_ypbc_global("t_group_root") << ";" << std::endl;
      }
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
}

void PbMessage::output_h_structs(std::ostream& os, unsigned indent)
{
  rw_yang_stmt_type_t stmt_type = get_yang_stmt_type();

  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg) {
      child_pbmsg->output_h_structs(os, indent);
    }
  }
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  std::string pad3(indent+4,' ');

  RW_ASSERT(is_good(pb_msg_type_));
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::rpc_input:
      if (is_local_schema_defining_message()) {
        os << pad1 << "struct " << get_ypbc_global("t_rpcdef") << std::endl;
        os << pad1 << "{" << std::endl;
        os << pad2 << "const " << get_pbmod()->get_ypbc_global("t_module")
           << "* module;" << std::endl;
        os << pad2 << "const char* yang_node_name;" << std::endl;
        os << pad2 << "const " << get_ypbc_global("t_msg_msgdesc")
           << "* input;" << std::endl;
        os << pad2 << "const " << get_rpc_output_pbmsg()->get_ypbc_global("t_msg_msgdesc")
           << "* output;" << std::endl;
        os << pad1 << "};" << std::endl;
        os << pad1 << "extern const " << get_ypbc_global("t_rpcdef") << " "
           << get_ypbc_global("g_rpcdef") << ";" << std::endl;
        os << std::endl;
      }
      goto also_message;

    case PbMsgType::data_module:
    case PbMsgType::message:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
    case PbMsgType::notif_module:
    case PbMsgType::notif:
    case PbMsgType::group:
    also_message:
      if (is_local_schema_defining_message()) {
        os << pad1 << "struct " << get_ypbc_global("t_msg_msgdesc") << std::endl;
        os << pad1 << "{" << std::endl;
        os << pad2 << "struct" << std::endl;
        os << pad2 << "{" << std::endl;
        output_h_ypbc_msgdesc_body(output_t::message, os, indent+4);
        os << pad2 << "} ypbc;" << std::endl;
        output_h_struct_children(output_t::message, os, indent+2);
        os << pad1 << "};" << std::endl;
        os << pad1 << "extern const " << get_ypbc_global("t_msg_msgdesc") << " "
           << get_ypbc_global("g_msg_msgdesc") << ";" << std::endl;
        // Prevent redefinition and bare use
        os << pad1 << "#define " << get_ypbc_global() << "() "
           << get_ypbc_global() << std::endl;
        if (!has_conflict()) {
          std::string alias = get_ypbc_short_alias();
          if (alias.length()) {
            os << pad1 << "#define " << alias << " "
               << get_ypbc_global() << std::endl;
          }
          alias = get_ypbc_msg_new_alias();
          if (alias.length()) {
            os << pad1 << "#define " << alias << " "
               << get_ypbc_global() << std::endl;
          }
        }

        if (RW_YANG_STMT_TYPE_LEAF_LIST != stmt_type) {
          os << pad1 << "#define " << get_ypbc_global("t_msg_pbcm") << " "
             << get_pbc_message_typename() << std::endl;
          os << pad1 << "#define " << get_ypbc_global("g_msg_pbcmd") << " "
             << get_pbc_message_global("descriptor") << std::endl;
          os << pad1 << "#define " << get_ypbc_global("t_msg_pbcc") << " "
             << get_pbc_message_typename() << "_Closure" << std::endl;
          os << std::endl;
        }

        if (!is_schema_suppressed()) {
          os << pad1 << "struct " << get_ypbc_global("t_entry_msgdesc") << std::endl;
          os << pad1 << "{" << std::endl;
          output_h_ypbc_msgdesc_body(output_t::path_entry, os, indent+2);
          os << pad1 << "};" << std::endl;
          os << pad1 << "extern const " << get_ypbc_global("t_entry_msgdesc") << " "
             << get_ypbc_global("g_entry_msgdesc") << ";" << std::endl;
          os << pad1 << "extern const " << get_pbc_schema_typename(output_t::path_entry)
             << " " << get_ypbc_global("g_entry_value") << ";" << std::endl;
          os << pad1 << "#define " << get_ypbc_global("t_entry_pbcm") << " "
             << get_pbc_schema_typename(output_t::path_entry) << std::endl;
          os << pad1 << "#define " << get_ypbc_global("g_entry_pbcmd") << " "
             << get_pbc_schema_global(output_t::path_entry, "descriptor") << std::endl;
          os << std::endl;

          os << pad1 << "struct " << get_ypbc_global("t_path_msgdesc") << std::endl;
          os << pad1 << "{" << std::endl;
          output_h_ypbc_msgdesc_body(output_t::path_spec, os, indent+2);
          os << pad1 << "};" << std::endl;
          os << pad1 << "extern const " << get_ypbc_global("t_path_msgdesc") << " "
             << get_ypbc_global("g_path_msgdesc") << ";" << std::endl;
          os << pad1 << "extern const " << get_pbc_schema_typename(output_t::path_spec)
             << " " << get_ypbc_global("g_path_value") << ";" << std::endl;
          os << pad1 << "#define " << get_ypbc_global("t_path_pbcm") << " "
             << get_pbc_schema_typename(output_t::path_spec) << std::endl;
          os << pad1 << "#define " << get_ypbc_global("g_path_pbcmd") << " "
             << get_pbc_schema_global(output_t::path_spec, "descriptor") << std::endl;
          os << std::endl;

          if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
            os << pad1 << "struct " << get_ypbc_global("t_list_msgdesc") << std::endl;
            os << pad1 << "{" << std::endl;
            output_h_ypbc_msgdesc_body(output_t::list_only, os, indent+2);
            os << pad1 << "};" << std::endl;
            os << pad1 << "extern const " << get_ypbc_global("t_list_msgdesc") << " "
               << get_ypbc_global("g_list_msgdesc") << ";" << std::endl;
            os << pad1 << "#define " << get_ypbc_global("t_list_pbcm") << " "
               << get_pbc_schema_typename(output_t::list_only) << std::endl;
            os << pad1 << "#define " << get_ypbc_global("g_list_pbcmd") << " "
               << get_pbc_schema_global(output_t::list_only, "descriptor") << std::endl;
            os << pad1 << "#define " << get_ypbc_global("f_list_pbcm_init") << " "
               << get_pbc_schema_global(output_t::list_only, "init") << std::endl;
            if (pbnode_->has_keys()) {
              os << pad1 << "#define " << get_ypbc_global("t_mk") << " "
                 << get_pbc_message_typename() << "__MiniKey" << std::endl;
              os << pad1 << "#define " << get_ypbc_global("f_mk_init") << " "
                 << get_pbc_message_global("mini_key__init") << std::endl;
              os << pad1 << "#define " << get_ypbc_global("m_mk_decl_init") << " "
                 << get_pbc_message_upper(output_t::message, "MINI_KEY__DEF_INIT") << std::endl;
            }

            os << std::endl;
          }
        }
      } else if (!is_pbc_suppressed() || !is_schema_suppressed()) {
        if (!has_conflict()) {
          std::string alias = get_ypbc_imported_alias();
          if (alias.length()) {
            os << pad1 << "#define " << alias << " "
               << get_ypbc_global() << std::endl;
            os << std::endl;
          }
          alias = get_ypbc_imported_msg_new_alias();
          if (alias.length()) {
            os << pad1 << "#define " << alias << " "
               << get_ypbc_global() << std::endl;
            os << std::endl;
          }
        }
      }
      break;
    case PbMsgType::group_root:
      if (is_local_message() && has_messages()) {
        os << pad1 << "struct " << get_ypbc_global("t_group_root") << std::endl;
        os << pad1 << "{" << std::endl;
        os << pad2 << "const " << get_pbmod()->get_ypbc_global("t_module") << "* module;" << std::endl;
        os << pad2 << "const rw_yang_pb_msgdesc_t* const* msgs;" << std::endl;
        os << pad2 << "size_t msg_count;" << std::endl;
        output_h_struct_children(output_t::message, os, indent+2);
        os << pad1 << "};" << std::endl;
        os << pad1 << "extern const " << get_ypbc_global("t_group_root") << " "
           << get_ypbc_global("g_group_root") << ";" << std::endl;
        os << std::endl;
      }
      return;
    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }

  if (RW_YANG_STMT_TYPE_LEAF_LIST != stmt_type) { // No proto-msg for leaf-list

    /* Output inline functions */
    if (is_local_schema_defining_message()) {
      /*
       * PBC* f_msg_init(
       *   PBC* msg)
       * {
       *   if (!msg)
       *     msg = malloc(sizeof(PBC));
       *   PBC_INIT(msg);
       *   return msg;
       * }
       */
      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_message_typename() << "* " << get_ypbc_global("f_msg_init") << "(" << std::endl;
      os << pad2 << get_pbc_message_typename() << "* msg)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << "if (!msg)" << std::endl;
      os << pad3 << "msg = (" << get_pbc_message_typename() << "*)protobuf_c_instance_alloc(NULL, sizeof("
         << get_pbc_message_typename() << "));" << std::endl;
      os << pad2 << get_pbc_message_global("init") << "(msg);" << std::endl;
      os << pad2 << "return msg;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_message_typename() << "* " << get_ypbc_global("f_msg_safecast") << "(" << std::endl;
      os << pad2 << "void* msg)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << "RW_ASSERT(msg && *(void**)msg == &"
         << get_pbc_message_global("descriptor") << ");" << std::endl;
      os << pad2 << "return (" << get_pbc_message_typename() << "*)msg;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;
    }

    if (is_local_schema_defining_message() && !is_schema_suppressed()) {
      /*
       * PBC f_entry_init(
       *   const PBC* in,
       *   PBC* out)
       * {
       *   PBC rv = in ? *in : GBL;
       *   if (out)
       *     *out = rv;
       *   return rv;
       * }
       */
      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_entry) << " "
          << get_ypbc_global("f_entry_init") << "(" << std::endl;
      os << pad2 << "const " << get_pbc_schema_typename(output_t::path_entry) << "* in," << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_entry) << "* out)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_entry) << " rv = {{PROTOBUF_C_MESSAGE_INIT(&"
                 << get_pbc_message_global("descriptor") << ", PROTOBUF_C_FLAG_REF_STATE_OWNED, 0)}};" << std::endl;
      os << pad2 << "if (in) protobuf_c_message_memcpy(&rv.base, &in->base);" << std::endl;
      os << pad2 << "else protobuf_c_message_memcpy(&rv.base, &"
                 << get_ypbc_global("g_entry_value") << ".base);" << std::endl;
      os << pad2 << "if (out)" << std::endl;
      os << pad3 << "*out = rv;" << std::endl;
      os << pad2 << "return rv;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

#if ATTN_TGS_NEED_TO_DEFINE_f_entry_init_set
      /*
       * PBC f_entry_init_set(
       *   const PBC* in,
       *   PBC* out,
       *   KEYS)
       * {
       *   PBC rv = in ? *in : GBL;
       *   SET KEYS
       *   if (out)
       *     *out = rv;
       *   return rv;
       * }
       */
      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_entry) << " "
          << get_ypbc_global("f_entry_init_set") << "(" << std::endl;
      os << pad2 << "const " << get_pbc_schema_typename(output_t::path_entry) << "* in," << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_entry) << "* out" << std::endl;
ATTN: KEYS
          os << pad2 << ")" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_entry) << " rv = in ? *in : "
          << get_ypbc_global("g_entry_value") << ";" << std::endl;
ATTN: KEYS
          os << pad2 << "if (out)" << std::endl;
      os << pad3 << "*out = rv;" << std::endl;
      os << pad2 << "return rv;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;
#endif


      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_entry) << "* " << get_ypbc_global("f_entry_safecast") << "(" << std::endl;
      os << pad2 << "void* msg)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << "RW_ASSERT(msg && *(void**)msg == &"
          << get_pbc_schema_global(output_t::path_entry, "descriptor") << ");" << std::endl;
      os << pad2 << "return (" << get_pbc_schema_typename(output_t::path_entry) << "*)msg;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

      /*
       * PBC f_path_init(
       *   const PBC* in,
       *   PBC* out)
       * {
       *   PBC rv = in ? *in : GBL;
       *   if (out)
       *     *out = rv;
       *   return rv;
       * }
       */
      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_spec) << " "
          << get_ypbc_global("f_path_init") << "(" << std::endl;
      os << pad2 << "const " << get_pbc_schema_typename(output_t::path_spec) << "* in," << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_spec) << "* out)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_spec) << " rv = {{PROTOBUF_C_MESSAGE_INIT(&"
                 << get_pbc_message_global("descriptor") << ", PROTOBUF_C_FLAG_REF_STATE_OWNED, 0)}};" << std::endl;
      os << pad2 << "if (in) protobuf_c_message_memcpy(&rv.base, &in->base);" << std::endl;
      os << pad2 << "else protobuf_c_message_memcpy(&rv.base, &"
                 << get_ypbc_global("g_path_value") << ".base);" << std::endl;
      os << pad2 << "if (out)" << std::endl;
      os << pad3 << "*out = rv;" << std::endl;
      os << pad2 << "return rv;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

      /*
       * PBC f_path_init_set(
       *   const PBC* in,
       *   PBC* out,
       *   KEYS)
       * {
       *   PBC rv = in ? *in : GBL;
       *   SET KEYS
       *   if (out)
       *     *out = rv;
       *   return rv;
       * }
       */
      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_spec) << " "
          << get_ypbc_global("f_path_init_set") << "(" << std::endl;
      os << pad2 << "const " << get_pbc_schema_typename(output_t::path_spec) << "* in," << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_spec) << "* out," << std::endl;
      os << pad2 << "RwSchemaCategory category)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << get_pbc_schema_typename(output_t::path_spec) << " rv = {{PROTOBUF_C_MESSAGE_INIT(&"
                 << get_pbc_message_global("descriptor") << ", PROTOBUF_C_FLAG_REF_STATE_OWNED, 0)}};" << std::endl;
      os << pad2 << "if (in) protobuf_c_message_memcpy(&rv.base, &in->base);" << std::endl;
      os << pad2 << "else protobuf_c_message_memcpy(&rv.base, &"
                 << get_ypbc_global("g_path_value") << ".base);" << std::endl;
      os << pad2 << "rv.dompath.category = category;" << std::endl;
      os << pad2 << "if (out)" << std::endl;
      os << pad3 << "*out = rv;" << std::endl;
      os << pad2 << "return rv;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

      os << pad1 << "__attribute__((always_inline)) static inline" << std::endl;
      os << pad1 << get_pbc_schema_typename(output_t::path_spec) << "* " << get_ypbc_global("f_path_safecast") << "(" << std::endl;
      os << pad2 << "void* msg)" << std::endl;
      os << pad1 << "{" << std::endl;
      os << pad2 << "RW_ASSERT(msg && *(void**)msg == &"
          << get_pbc_schema_global(output_t::path_spec, "descriptor") << ");" << std::endl;
      os << pad2 << "return (" << get_pbc_schema_typename(output_t::path_spec) << "*)msg;" << std::endl;
      os << pad1 << "}" << std::endl;
      os << std::endl;

#if ATTN_TGS_NEED_TO_DEFINE_f_minikeys
ATTN: minikeys
#endif
    }
  }
}

void PbMessage::output_h_ypbc_msgdesc_body(output_t output_style, std::ostream& os, unsigned indent)
{
  const rw_yang_stmt_type_t stmt_type = get_yang_stmt_type();
  std::string pad1(indent,' ');

  os << pad1 << "const " << get_schema_defining_pbmod()->get_ypbc_global("t_module") << "* module;" << std::endl;
  os << pad1 << "const char* yang_node_name;" << std::endl;
  os << pad1 << "const uint32_t pb_element_tag;"<< std::endl;
  switch (output_style) {
    case output_t::message:
      os << pad1 << "const size_t num_fields;" << std::endl;
      os << pad1 << "const rw_yang_pb_flddesc_t* ypbc_flddescs;" << std::endl;
      break;

    case output_t::list_only:
      os << pad1 << "const size_t num_fields;" << std::endl;
      os << pad1 << "const void* ypbc_flddescs;" << std::endl;
      break;

    case output_t::path_entry:
    case output_t::path_spec:
      os << pad1 << "const size_t num_fields;" << std::endl;
      os << pad1 << "const void* ypbc_flddescs;" << std::endl;
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (   parent_pbmsg_
      && !parent_pbmsg_->is_root_message()
      && RW_YANG_STMT_TYPE_RPCIO != stmt_type) {
    switch (output_style) {
      case output_t::message:
        os << pad1 << "const " << parent_pbmsg_->get_ypbc_global("t_msg_msgdesc") << "* parent;" << std::endl;
        break;
      case output_t::path_entry:
        os << pad1 << "const " << parent_pbmsg_->get_ypbc_global("t_entry_msgdesc") << "* parent;" << std::endl;
        break;
      case output_t::path_spec:
        os << pad1 << "const " << parent_pbmsg_->get_ypbc_global("t_path_msgdesc") << "* parent;" << std::endl;
        break;
      case output_t::list_only:
        os << pad1 << "const rw_yang_pb_msgdesc_t* parent;" << std::endl;
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
  } else {
    os << pad1 << "const rw_yang_pb_msgdesc_t* parent;" << std::endl;
  }

  os << pad1 << "const ProtobufCMessageDescriptor* pbc_mdesc;" << std::endl;

  if (   is_local_schema_defining_message()
      && !is_schema_suppressed()
      && output_t::message == output_style) {
    os << pad1 << "const " << get_ypbc_global("t_entry_msgdesc") << "* schema_entry_ypbc_desc;" << std::endl;
    os << pad1 << "const " << get_ypbc_global("t_path_msgdesc") << "* schema_path_ypbc_desc;" << std::endl;
    if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
      os << pad1 << "const " << get_ypbc_global("t_list_msgdesc") << "* schema_list_only_ypbc_desc;" << std::endl;
    } else {
      os << pad1 << "const void* schema_list_only_ypbc_desc; " << std::endl;
    }
    os << pad1 << "const " << get_pbc_schema_typename(output_t::path_entry)
       << "* schema_entry_value;" << std::endl;
    os << pad1 << "const " << get_pbc_schema_typename(output_t::path_spec)
       << "* schema_path_value;" << std::endl;
  } else {
    os << pad1 << "const void* schema_entry_ypbc_desc;" << std::endl;
    os << pad1 << "const void* schema_path_ypbc_desc;" << std::endl;
    os << pad1 << "const void* schema_list_only_ypbc_desc;" << std::endl;
    os << pad1 << "const void* schema_entry_value;" << std::endl;
    os << pad1 << "const void* schema_path_value;" << std::endl;
  }

  if (output_t::message == output_style) {
    os << pad1 << "const rw_yang_pb_msgdesc_t* const* child_msgs;" << std::endl;
    os << pad1 << "size_t child_msg_count;" << std::endl;
  } else {
    os << pad1 << "const void* child_msgs;" << std::endl;
    os << pad1 << "size_t child_msg_count;" << std::endl;
  }

  os << pad1 << "rw_yang_pb_msg_type_t msg_type;" << std::endl;

  switch (output_style) {
    case output_t::message:
      switch (pb_msg_type_) {
        case PbMsgType::rpc_input:
          os << pad1 << "const " << get_ypbc_global("t_rpcdef")
             << "* rpcdef;" << std::endl;
          break;
        case PbMsgType::rpc_output:
          os << pad1 << "const " << get_rpc_input_pbmsg()->get_ypbc_global("t_rpcdef")
             << "* rpcdef;" << std::endl;
          break;
        default:
          os << pad1 << "void* u;" << std::endl;
          break;
      }
      break;

    case output_t::list_only:
    case output_t::path_entry:
    case output_t::path_spec:
      os << pad1 << "const " << get_ypbc_global("t_msg_msgdesc")
         << "* msg_msgdesc;" << std::endl;
      break;

    default:
      RW_ASSERT_NOT_REACHED();
  }
}

void PbMessage::output_h_struct_children(output_t output_style, std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  switch (pb_msg_type_) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::data_module:
    case PbMsgType::message:
    case PbMsgType::rpci_module:
    case PbMsgType::rpc_input:
    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
    case PbMsgType::notif_module:
    case PbMsgType::notif:
    case PbMsgType::group_root:
    case PbMsgType::group:
      for (auto const& pbfld: field_list_) {
        PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
        if (child_pbmsg) {
          os << pad1 << "const "
             << child_pbmsg->get_ypbc_global("t_msg_msgdesc") << "* "
             << pbfld->get_proto_fieldname() << ";" << std::endl;
        }
      }
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }
}

void PbMessage::output_h_meta_data_macro(std::ostream& os, unsigned indent)
{
  if (!is_local_schema_defining_message()) {
    return;
  }

  std::string pad(indent+2,' ');
  std::string eol(", \\\n");

  os << "#define " << get_ypbc_global("m_msg") << "() \\" << std::endl;
  os << pad << "\"" << pbnode_->get_yang_fieldname() << "\"" << eol;
  os << pad << get_pbc_message_upper(output_t::message, "msg") << eol;
  os << pad << get_ypbc_global("t_msg_msgdesc") << eol;
  os << pad << get_ypbc_global("t_path_msgdesc") << eol;
  os << pad << get_ypbc_global("t_entry_msgdesc") << eol;
  os << pad << get_pbc_message_typename() << eol;
  os << pad << get_pbc_schema_typename(output_t::path_spec) << eol;
  os << pad << get_pbc_schema_typename(output_t::path_entry) << eol;
  os << pad << get_pbc_message_typename() << "_Closure" << eol;
  os << pad << get_ypbc_global("g_msg_msgdesc") << eol;
  os << pad << get_ypbc_global("g_path_msgdesc") << eol;
  os << pad << get_ypbc_global("g_entry_msgdesc") << eol;
  os << pad << get_pbc_message_global("descriptor") << eol;
  os << pad << get_pbc_schema_global(output_t::path_spec, "descriptor") << eol;
  os << pad << get_pbc_schema_global(output_t::path_entry, "descriptor") << eol;
  os << pad << get_ypbc_global("g_path_value") << eol;
  os << pad << get_ypbc_global("g_entry_value") << eol;
  os << pad << get_pbc_message_upper(output_t::path_entry, "msg") << eol;
  os << pad << get_pbc_message_upper(output_t::path_spec, "msg") << eol;
  os << pad << get_pbc_message_upper(output_t::list_only, "msg") << eol;

  os << std::endl;

  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    (*fi)->output_h_meta_data_macro(os, indent);
  }

  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg &&
        child_pbmsg->get_yang_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST) {
      child_pbmsg->output_h_meta_data_macro(os, indent);
    }
  }
}


void PbMessage::output_cpp_schema_path_entry_init(char eoinit, std::ostream& os, unsigned indent, std::string offset)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');
  std::string pad3(indent+4,' ');

  auto stmt_type = get_yang_stmt_type();
  bool inner = false;
  std::string ref_state("PROTOBUF_C_FLAG_REF_STATE_GLOBAL");
  if (!offset.empty()) {
    ref_state = "PROTOBUF_C_FLAG_REF_STATE_INNER";
    inner = true;
  }
  os << pad1 << "{" << std::endl;
  os << pad2 << "{PROTOBUF_C_MESSAGE_INIT(&"
      << get_pbc_schema_global(output_t::path_entry, "descriptor")
      << ", " << ref_state << ", ";
  if (inner) {
    os << offset << ")}," << std::endl;
  } else {
    os << "0)}," << std::endl;
  }
  os << pad2 << "{" << std::endl;
  os << pad3 << "{PROTOBUF_C_MESSAGE_INIT(&rw_schema_element_id__descriptor,"
             << "PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof("
             << get_pbc_schema_typename(output_t::path_entry) << ", element_id))}," << std::endl;
  os << pad3 << get_pbmod()->get_nsid() << "," << std::endl;
  os << pad3 << pbnode_->get_tag_number() << "," << std::endl;
  os << pad3 << pbnode_->get_schema_element_type() << "," << std::endl;
  os << pad2 << "}," << std::endl;

  // Output inits for the keys

  if (stmt_type == RW_YANG_STMT_TYPE_LIST) {
    unsigned key_n = 0;
    for (YangKeyIter yki = get_ynode()->key_begin(); yki != get_ynode()->key_end(); ++yki, ++key_n) {
      // Find the yang node
      YangNode* yknode = yki->get_key_node();
      RW_ASSERT(yknode);

      // Find the pb node
      PbNode* pbnode = get_pbmodel()->lookup_node(yknode);
      RW_ASSERT(pbnode);

      // Find the field
      PbField* pbfld = field_stab_.get_symbol(pbnode->get_proto_fieldname());
      RW_ASSERT(pbfld);

      os << pad2 << "false," << std::endl;

      char kfname[16];
      int bytes = snprintf(kfname, sizeof(kfname), "key%02u", key_n);
      RW_ASSERT(bytes > 0 && bytes < (int)sizeof(kfname));

      std::ostringstream offset;
      offset << "offsetof(" << get_pbc_schema_typename(output_t::path_entry)
          << ", " << kfname << ")";

      // And output the message...
      pbfld->output_cpp_schema_key_init(os, indent+2, offset.str());
    }
  } else if (stmt_type == RW_YANG_STMT_TYPE_LEAF_LIST) {
    PbField* my_pbfld = parent_pbmsg_->get_field_by_name(pbnode_->get_proto_fieldname());
    os << pad2 << "false," << std::endl;

    std::ostringstream offset;
    offset << "offsetof(" << get_pbc_schema_typename(output_t::path_entry)
        << ", key00)";

    my_pbfld->output_cpp_schema_key_init(os, indent+2, offset.str());
  }
  os << pad1 << "}" << eoinit << std::endl;
}

void PbMessage::output_cpp_schema_dompath_init(char eoinit, std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  os << pad1 << "{" << std::endl;
  os << pad2 << "{PROTOBUF_C_MESSAGE_INIT(&"
     << get_pbc_schema_global(output_t::dom_path, "descriptor")
     << ", PROTOBUF_C_FLAG_REF_STATE_INNER, offsetof("
     << get_pbc_schema_typename(output_t::path_spec) << ", dompath))}," << std::endl;
  os << pad2 << "true," << std::endl; // ATTN: Only true if in a data hierarchy
  os << pad2 << "true," << std::endl; // has_category
  switch (get_category_msg_type()) {
    case PbMsgType::package:
    case PbMsgType::top_root:
    case PbMsgType::data_root:
    case PbMsgType::rpci_root:
    case PbMsgType::rpco_root:
    case PbMsgType::notif_root:
      RW_ASSERT_NOT_REACHED(); // not implemented

    case PbMsgType::message:
      RW_ASSERT_NOT_REACHED(); // should never see these

    case PbMsgType::data_module:
    case PbMsgType::group_root:
    case PbMsgType::group:
      os << pad2 << "RW_SCHEMA_CATEGORY_ANY," << std::endl;
      break;

    case PbMsgType::rpci_module:
    case PbMsgType::rpc_input:
      os << pad2 << "RW_SCHEMA_CATEGORY_RPC_INPUT," << std::endl;
      break;

    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
      os << pad2 << "RW_SCHEMA_CATEGORY_RPC_OUTPUT," << std::endl;
      break;

    case PbMsgType::notif_module:
    case PbMsgType::notif:
      os << pad2 << "RW_SCHEMA_CATEGORY_NOTIFICATION," << std::endl;
      break;

    case PbMsgType::illegal_:
    case PbMsgType::unset:
    case PbMsgType::automatic:
    case PbMsgType::last_:
      RW_ASSERT_NOT_REACHED();
  }

  // Build the DOM path.  Need to output root first, so build a list first to reverse.
  // ATTN: We build this list several times in the code...  should be a function or a member?
  std::list<PbMessage*> path;
  for (PbMessage* m = this; m; m = m->parent_pbmsg_) {
    if (!m->is_root_message()) {
      path.emplace_front(m);
    }
  }

  uint32_t path_n = 0;
  for (auto const& pi: path) {
    os << pad2 << "true," << std::endl;

    char fname[4096];
    int bytes = snprintf(fname, sizeof(fname), "path%03u", path_n++);
    RW_ASSERT(bytes > 0 && bytes < (int)sizeof(fname));

    std::ostringstream offset;
    offset << "offsetof(" << get_pbc_schema_typename(output_t::dom_path)
         << ", " << fname << ")";

    pi->output_cpp_schema_path_entry_init(',', os, indent+2, offset.str());
  }

  if (is_root_message()) {
    switch (pb_msg_type_) {
      case PbMsgType::data_module:
      case PbMsgType::rpci_module:
      case PbMsgType::rpco_module:
      case PbMsgType::notif_module: {
        std::ostringstream offset;
        os << pad2 << "true," << std::endl;
        offset << "offsetof(" << get_pbc_schema_typename(output_t::dom_path)
            << ", path_module)";
        output_cpp_schema_path_entry_init(',', os, indent+2, offset.str());
        break;
      }
      default:
        RW_ASSERT_NOT_REACHED();
    }
  }

  // Ok, we now have an addition optional generic path entry. Initialize it to
  // NULL.
  if (get_yang_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST &&
      get_yang_stmt_type() != RW_YANG_STMT_TYPE_ROOT) {
    os << pad2 << "nullptr," << std::endl;
  }

  os << pad1 << "}" << eoinit << std::endl;
}

void PbMessage::output_cpp_schema_path_spec_init(char eoinit, std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  os << pad1 << "{" << std::endl;
  os << pad2 << "{PROTOBUF_C_MESSAGE_INIT(&"
     << get_pbc_schema_global(output_t::path_spec, "descriptor")
     << ", PROTOBUF_C_FLAG_REF_STATE_GLOBAL , 0)}," << std::endl;
  os << pad2 << "true," << std::endl; // has_dompath

  output_cpp_schema_dompath_init(',', os, indent+2);

  os << pad2 << "false," << std::endl; // has_binpath
  os << pad2 << "{ 0, nullptr, }," << std::endl; // binpath
  os << pad2 << "nullptr," << std::endl; // strpath
  os << pad1 << "}" << eoinit << std::endl;
}

void PbMessage::output_cpp_field_order(std::ostream& os, unsigned indent)
{
  if (field_list_.size()) {
    std::string pad1(indent,' ');
    PbField::field_by_tag_t fields_sorted_by_tag;
    for (auto const& pbfld: field_list_) {
      RW_ASSERT(pbfld);
      RW_ASSERT(pbfld->get_tag_value() != 0);
      fields_sorted_by_tag[pbfld->get_tag_value()] = pbfld.get();
    }

    os << pad1 << "static const uint32_t " << get_ypbc_global("g_yangorder") << "[] = {" << std::endl;
    os << pad1 << " ";
    for (auto const& pbfld: field_list_) {
      os << " " << std::distance(fields_sorted_by_tag.begin(),fields_sorted_by_tag.find(pbfld->get_tag_value()))+1 << ",";
    }
    os << std::endl;
    os << pad1 << "};" << std::endl;
    os << std::endl;
  }
}

void PbMessage::output_cpp_ypbc_field_descs(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2, ' ');

  if (field_list_.size()) {
    PbField::field_by_tag_t fields_sorted_by_tag;
    for (auto const& pbfld: field_list_) {
      RW_ASSERT(pbfld);
      RW_ASSERT(pbfld->get_tag_value() != 0);
      fields_sorted_by_tag[pbfld->get_tag_value()] = pbfld.get();
    }

    os << pad1 << "static const rw_yang_pb_flddesc_t " << get_ypbc_global("g_fld_descs") << "[] = " << std::endl;
    os << pad1 << "{" << std::endl;
    unsigned yang_order = 1;
    for (auto const& pbfld: field_list_) {
      os << pad2 << "{" << std::endl;
      unsigned pbc_order = std::distance(fields_sorted_by_tag.begin(),fields_sorted_by_tag.find(pbfld->get_tag_value()))+1;
      pbfld->output_cpp_ypbc_flddesc_body(os, indent+4, pbc_order, yang_order);
      os << pad2 << "}," << std::endl;
      yang_order++;
    }
    os << pad1 << "};" << std::endl;
    os << pad1 << std::endl;
  }
}

void PbMessage::output_cpp_ypbc_msgdesc_body(output_t output_style, std::ostream& os, unsigned indent)
{
  const rw_yang_stmt_type_t stmt_type = get_yang_stmt_type();
  std::string pad1(indent,' ');

  /*** const rw_yang_pb_module_t* module ***/
  os << pad1 << "&" << get_schema_defining_pbmod()->get_ypbc_global("g_module") << "," << std::endl;

  /*** const char* yang_node_name ***/
  switch (pb_msg_type_) {
    case PbMsgType::rpc_input:
    case PbMsgType::rpc_output:
      RW_ASSERT(pbnode_->get_parent_pbnode());
      os << pad1 << "\"" << pbnode_->get_parent_pbnode()->get_yang_fieldname()
         << "\"," << std::endl;
      break;
    default:
      os << pad1 << "\"" << pbnode_->get_yang_fieldname() << "\"," << std::endl;
      break;
  }

  /*** const uint32_t pb_element_tag ***/
  os << pad1 << pbnode_->get_tag_number() <<","<<std::endl;

  /*** const size_t num_fields ***/
  /*** const uint32_t* field_index ***/
  switch (output_style) {
    case output_t::message: {
      if (generate_field_descs()) {
        os << pad1 << field_list_.size() << "," << std::endl;
        if (field_list_.size()) {
          os << pad1 << get_ypbc_global("g_fld_descs") << "," << std::endl;
        } else {
          os << pad1 << "nullptr," << std::endl;
        }
      } else {
        os << pad1 << "0," << std::endl;
        os << pad1 << "nullptr," << std::endl;
      }
      break;
    }
    case output_t::list_only:
    case output_t::path_entry:
    case output_t::path_spec:
      os << pad1 << "0," << std::endl;
      os << pad1 << "nullptr," << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  /*** const rw_yang_pb_msgdesc_t* parent ***/
  if (   parent_pbmsg_
      && !parent_pbmsg_->is_root_message()
      && RW_YANG_STMT_TYPE_RPCIO != stmt_type) {
    switch (output_style) {
      case output_t::message:
        os << pad1 << "&" << parent_pbmsg_->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
        break;
      case output_t::path_entry:
        os << pad1 << "&" << parent_pbmsg_->get_ypbc_global("g_entry_msgdesc") << "," << std::endl;
        break;
      case output_t::path_spec:
        os << pad1 << "&" << parent_pbmsg_->get_ypbc_global("g_path_msgdesc") << "," << std::endl;
        break;
      case output_t::list_only:
        os << pad1 << "nullptr," << std::endl;
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }
  } else {
    os << pad1 << "nullptr," << std::endl;
  }

  /*** const ProtobufCMessageDescriptor* pbc_mdesc ***/
  switch (output_style) {
    case output_t::message:
      if (stmt_type == RW_YANG_STMT_TYPE_LEAF_LIST) {
        // No proto message.
        os << pad1 << "nullptr," << std::endl;
      } else {
        os << pad1 << "&" << get_pbc_message_global("descriptor") << "," << std::endl;
      }
      break;
    case output_t::path_entry:
    case output_t::path_spec:
    case output_t::list_only:
      os << pad1 << "&" << get_pbc_schema_global(output_style, "descriptor") << "," << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  /*** const rw_yang_pb_msgdesc_t* schema_entry_ypbc_desc ***/
  /*** const rw_yang_pb_msgdesc_t* schema_path_ypbc_desc ***/
  /*** const rw_yang_pb_msgdesc_t* schema_list_only_ypbc_desc ***/
  /*** const ProtobufCMessage* schema_entry_value ***/
  /*** const ProtobufCMessage* schema_path_value ***/
  if (   is_local_schema_defining_message()
      && !is_schema_suppressed()
      && output_t::message == output_style) {
    os << pad1 << "&" << get_ypbc_global("g_entry_msgdesc") << "," << std::endl;
    os << pad1 << "&" << get_ypbc_global("g_path_msgdesc") << "," << std::endl;
    if (RW_YANG_STMT_TYPE_LIST == stmt_type) {
      os << pad1 << "&" << get_ypbc_global("g_list_msgdesc") << "," << std::endl;
    } else {
      os << pad1 << "nullptr," << std::endl;
    }
    os << pad1 << "&" << get_ypbc_global("g_entry_value") << "," << std::endl;
    os << pad1 << "&" << get_ypbc_global("g_path_value") << "," << std::endl;
  } else {
    os << pad1 << "nullptr," << std::endl;
    os << pad1 << "nullptr," << std::endl;
    os << pad1 << "nullptr," << std::endl;
    os << pad1 << "nullptr," << std::endl;
    os << pad1 << "nullptr," << std::endl;
  }

  /*** const rw_yang_pb_msgdesc_t* const* child_msgs ***/
  /*** size_t child_msg_count ***/
  if (   output_t::message == output_style
      && message_children_) {
    os << pad1 << "reinterpret_cast<const rw_yang_pb_msgdesc_t* const*>("
       << "reinterpret_cast<const char*>(&" << get_ypbc_global("g_msg_msgdesc")
       << ".ypbc) + sizeof(rw_yang_pb_msgdesc_t))," << std::endl;
    os << pad1 << message_children_ << "," << std::endl;
  } else {
    os << pad1 << "nullptr," << std::endl;
    os << pad1 << "0," << std::endl;
  }

  /*** rw_yang_pb_msg_type_t msg_type ***/
  switch (output_style) {
    case output_t::message: {
      const char* type_str = rw_yang_pb_msg_type_string(ypbc_msg_type_);
      RW_ASSERT(type_str && type_str[0] != '\0');
      os << pad1 << "RW_YANGPBC_MSG_TYPE_" << type_str << "," << std::endl;
      break;
    }
    case output_t::path_entry:
      os << pad1 << "RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY," << std::endl;
      break;
    case output_t::path_spec:
      os << pad1 << "RW_YANGPBC_MSG_TYPE_SCHEMA_PATH," << std::endl;
      break;
    case output_t::list_only:
      os << pad1 << "RW_YANGPBC_MSG_TYPE_SCHEMA_LIST_ONLY," << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  /*** const rw_yang_pb_msgdesc_union_t* u ***/
  switch (output_style) {
    case output_t::message:
      switch (pb_msg_type_) {
        case PbMsgType::rpc_input:
          os << pad1 << "&" << get_ypbc_global("g_rpcdef") << "," << std::endl;
          break;
        case PbMsgType::rpc_output:
          os << pad1 << "&" << get_rpc_input_pbmsg()->get_ypbc_global("g_rpcdef") << "," << std::endl;
          break;
        default:
          os << pad1 << "nullptr," << std::endl;
          break;
      }
      break;
    case output_t::path_entry:
    case output_t::path_spec:
    case output_t::list_only:
      os << pad1 << "&" << get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
}

void PbMessage::output_cpp_msgdesc_define(std::ostream& os, unsigned indent)
{
  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg) {
      child_pbmsg->output_cpp_msgdesc_define(os, indent);
    }
  }

  switch (pb_msg_type_) {
    case PbMsgType::rpc_input:
      output_cpp_msgdesc_define_rpc_input(os, indent);
      output_cpp_msgdesc_define_message(os, indent);
      break;

    case PbMsgType::data_module:
    case PbMsgType::message:
    case PbMsgType::rpci_module:
    case PbMsgType::rpco_module:
    case PbMsgType::rpc_output:
    case PbMsgType::notif_module:
    case PbMsgType::notif:
    case PbMsgType::group:
      output_cpp_msgdesc_define_message(os, indent);
      break;

    case PbMsgType::group_root:
      output_cpp_msgdesc_define_group_root(os, indent);
      break;

    default:
      RW_ASSERT_NOT_REACHED(); // not implemented
  }
}

void PbMessage::output_cpp_msgdesc_define_rpc_input(std::ostream& os, unsigned indent)
{
  if (is_local_message()) {
    std::string pad1(indent,' ');
    std::string pad2(indent+2,' ');

    os << pad1 << "const " << get_ypbc_global("t_rpcdef") << " "
       << get_ypbc_global("g_rpcdef") << " =" << std::endl;
    os << pad1 << "{" << std::endl;
    os << pad2 << "&" << get_pbmod()->get_ypbc_global("g_module") << "," << std::endl;
    os << pad2 << "\"" << pbnode_->get_yang_fieldname() << "\"," << std::endl;
    os << pad2 << "&" << get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
    os << pad2 << "&" << get_rpc_output_pbmsg()->get_ypbc_global("g_msg_msgdesc") << "," << std::endl;
    os << pad1 << "};" << std::endl;
    os << std::endl;
  }
}

void PbMessage::output_cpp_msgdesc_define_group_root(std::ostream& os, unsigned indent)
{
  if (is_local_message() && has_messages()) {
    std::string pad1(indent,' ');
    std::string pad2(indent+2,' ');

    os << pad1 << "const " << get_ypbc_global("t_group_root") << " "
       << get_ypbc_global("g_group_root") << " =" << std::endl;
    os << pad1 << "{" << std::endl;
    os << pad2 << "&" << get_pbmod()->get_ypbc_global("g_module") << "," << std::endl;
    os << pad2 << "reinterpret_cast<const rw_yang_pb_msgdesc_t* const*>("
       << "reinterpret_cast<const char*>(&" << get_ypbc_global("g_group_root")
       << ")+sizeof(rw_yang_pb_group_root_t))," << std::endl;
    os << pad2 << has_messages() << "," << std::endl;

    // Output the group message references.
    for (auto const& pbfld: field_list_) {
      PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
      if (child_pbmsg) {
        os << pad2 << "&" << child_pbmsg->get_ypbc_global("g_msg_msgdesc")
           << "," << std::endl;
      }
    }

    os << pad1 << "};" << std::endl;
    os << std::endl;
  }
}

void PbMessage::output_cpp_msgdesc_define_message(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');
  std::string pad2(indent+2,' ');

  if (is_local_schema_defining_message()) {

    if (generate_field_descs()) {
      output_cpp_ypbc_field_descs(os, indent);
    }

    os << pad1 << "const " << get_ypbc_global("t_msg_msgdesc") << " "
       << get_ypbc_global("g_msg_msgdesc") << " =" << std::endl;
    os << pad1 << "{" << std::endl;

    os << pad2 << "{" << std::endl;
    output_cpp_ypbc_msgdesc_body(output_t::message, os, indent+4);
    os << pad2 << "}," << std::endl;

    // Output the child message references.
    for (auto const& pbfld: field_list_) {
      PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
      if (child_pbmsg) {
        os << pad2 << "&" << child_pbmsg->get_ypbc_global("g_msg_msgdesc")
           << "," << std::endl;
      }
    }
    os << pad1 << "};" << std::endl;
    os << std::endl;
  }

  if (   is_local_schema_defining_message()
      && !is_schema_suppressed()) {
    os << pad1 << "const " << get_ypbc_global("t_entry_msgdesc") << " "
       << get_ypbc_global("g_entry_msgdesc") << " =" << std::endl;
    os << pad1 << "{" << std::endl;
    output_cpp_ypbc_msgdesc_body(output_t::path_entry, os, indent+2);
    os << pad1 << "};" << std::endl;
    os << std::endl;
    os << pad1 << "const " << get_pbc_schema_typename(output_t::path_entry)
       << " " << get_ypbc_global("g_entry_value") << " =" << std::endl;
    output_cpp_schema_path_entry_init(';', os, indent, std::string(""));
    os << std::endl;

    os << pad1 << "const " << get_ypbc_global("t_path_msgdesc") << " "
       << get_ypbc_global("g_path_msgdesc") << " =" << std::endl;
    os << pad1 << "{" << std::endl;
    output_cpp_ypbc_msgdesc_body(output_t::path_spec, os, indent+2);
    os << pad1 << "};" << std::endl;
    os << std::endl;

    os << pad1 << "const " << get_pbc_schema_typename(output_t::path_spec)
       << " " << get_ypbc_global("g_path_value") << " =" << std::endl;
    output_cpp_schema_path_spec_init(';', os, indent);
    os << std::endl;

    if (RW_YANG_STMT_TYPE_LIST == get_yang_stmt_type()) {
      os << pad1 << "const " << get_ypbc_global("t_list_msgdesc") << " "
         << get_ypbc_global("g_list_msgdesc") << " =" << std::endl;
      os << pad1 << "{" << std::endl;
      output_cpp_ypbc_msgdesc_body(output_t::list_only, os, indent+2);
      os << pad1 << "};" << std::endl;
      os << std::endl;
    }
  }
}

void PbMessage::output_gi_to_xml_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  // Output the comment block with annotation
  os << "/**"                                         << std::endl
     << " *" << get_gi_c_identifier("to_xml") << ":"    << std::endl
     << " *@boxed:"                                   << std::endl
     << " *@ymodel      : (in):"                      << std::endl
     << " **/"                                        << std::endl;
  os << pad1 << "gchar* " << get_gi_c_identifier("to_xml")
             << "(" << get_gi_typename() << "* boxed, "
             << "void * ymodel);"  << std::endl;

  return;
}

void PbMessage::output_gi_to_xml_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent+2, ' ');

  os << pad1 << "gchar* " << get_gi_c_identifier("to_xml")
             << "(" << get_gi_typename() << "* boxed, "
             << "void * ymodel)"  << std::endl;

  os << pad1 << "{" << std::endl
     << pad2 << "rw_xml_manager_t *rw_xml_mgr = rw_xml_manager_create_xerces_model(ymodel);" << std::endl
     << pad2 << "gchar*xml_str = rw_xml_manager_pb_to_xml_alloc_string(rw_xml_mgr, boxed->s.message);" << std::endl
     << pad2 << "rw_xml_manager_release(rw_xml_mgr);" << std::endl
     << pad2 << "return xml_str;" << std::endl
     << pad1 << "}" << std::endl << std::endl;
}

void PbMessage::output_gi_to_xml_v2_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  // Output the comment block with annotation
  os << "/**"                                         << std::endl
     << " *" << get_gi_c_identifier("to_xml_v2") << ":"    << std::endl
     << " *@boxed:"                                   << std::endl
     << " *@ymodel      : (in):"                      << std::endl
     << " *@pretty_print: (in) (nullable):"               << std::endl
     << " **/"                                        << std::endl;
  os << pad1 << "gchar* " << get_gi_c_identifier("to_xml_v2")
             << "(" << get_gi_typename() << "* boxed, "
             << "rw_yang_model_t * ymodel,"
             << "gboolean pretty_print);"  << std::endl;
}

void PbMessage::output_gi_to_xml_v2_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent+2, ' ');

  os << pad1 << "gchar* " << get_gi_c_identifier("to_xml_v2")
             << "(" << get_gi_typename() << "* boxed, "
             << "rw_yang_model_t * ymodel,"
             << "gboolean pretty_print)"  << std::endl;

  os << pad1 << "{" << std::endl
     << pad2 << "rw_xml_manager_t *rw_xml_mgr = rw_xml_manager_create_xerces_model(ymodel);" << std::endl
     << pad2 << "gchar*xml_str = rw_xml_manager_pb_to_xml_alloc_string_pretty(rw_xml_mgr, boxed->s.message, pretty_print);" << std::endl
     << pad2 << "rw_xml_manager_release(rw_xml_mgr);" << std::endl
     << pad2 << "return xml_str;" << std::endl
     << pad1 << "}" << std::endl << std::endl;
}


void PbMessage::output_gi_from_xml_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  // Output the comment block with annotation
  os << "/**"                                         << std::endl
     << " * " << get_gi_c_identifier("from_xml") << ":" << std::endl
     << " *@boxed:"                                   << std::endl
     << " *@ymodel: (in):"                            << std::endl
     << " *@xml_string: (in):"                        << std::endl
     << " **/"                                        << std::endl;
  os << pad1 << "void " << get_gi_c_identifier("from_xml")
             << "(" << get_gi_typename() << "* boxed, "
             << "void * ymodel, const gchar *xml_string, GError **error);" << std::endl;
}

void PbMessage::output_gi_from_xml_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent + 2, ' ');
  std::string pad3(indent + 4, ' ');
  std::string domain = PbModel::mangle_to_uppercase(get_pbmodel()->get_schema_pbmod()->get_proto_package_name());

  os << pad1 << "void " << get_gi_c_identifier("from_xml")
             << "(" << get_gi_typename() << "* boxed, "
             << "void * ymodel, const gchar *xml_string, GError **error)" << std::endl;

  os << pad1 << "{" << std::endl
     << pad2 << "rw_xml_manager_t *rw_xml_mgr = rw_xml_manager_create_xerces_model(ymodel);" << std::endl
     << pad2 << "char *errstr = NULL;" << std::endl
     << pad2 << "rw_status_t status = rw_xml_manager_xml_to_pb(rw_xml_mgr, xml_string, &boxed->s.message->base, &errstr);" << std::endl
     << pad2 << "if (status) { " << std::endl
     << pad3 << "RW_ASSERT(errstr);" << std::endl
     << pad3 << "PROTOBUF_C_GI_RAISE_EXCEPTION(error, " << domain << ", PROTO_GI_ERROR_XML_TO_PB_FAILURE,"
                "\"%s (%d)\\n\", errstr, status );" << std::endl
     << pad2 << "}" << std::endl
     << pad2 << "rw_xml_manager_release(rw_xml_mgr);" << std::endl
     << pad1 << "}" << std::endl << std::endl;
}

void PbMessage::output_gi_from_xml_v2_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  // Output the comment block with annotation
  os << "/**"                                         << std::endl
     << " * " << get_gi_c_identifier("from_xml_v2") << ":" << std::endl
     << " *@boxed:"                                   << std::endl
     << " *@ymodel: (in):"                            << std::endl
     << " *@xml_string: (in):"                        << std::endl
     << " **/"                                        << std::endl;
  os << pad1 << "void " << get_gi_c_identifier("from_xml_v2")
             << "(" << get_gi_typename() << "* boxed, "
             << "rw_yang_model_t * ymodel, const gchar *xml_string, GError **error);" << std::endl;
}

void PbMessage::output_gi_from_xml_v2_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent + 2, ' ');
  std::string pad3(indent + 4, ' ');
  std::string domain = PbModel::mangle_to_uppercase(get_pbmodel()->get_schema_pbmod()->get_proto_package_name());

  os << pad1 << "void " << get_gi_c_identifier("from_xml_v2")
             << "(" << get_gi_typename() << "* boxed, "
             << "rw_yang_model_t * ymodel, const gchar *xml_string, GError **error)" << std::endl;

  os << pad1 << "{" << std::endl
     << pad2 << "rw_xml_manager_t *rw_xml_mgr = rw_xml_manager_create_xerces_model(ymodel);" << std::endl
     << pad2 << "char *errstr = NULL;" << std::endl
     << pad2 << "rw_status_t status = rw_xml_manager_xml_to_pb(rw_xml_mgr, xml_string, &boxed->s.message->base, &errstr);" << std::endl
     << pad2 << "if (status) { " << std::endl
     << pad3 << "RW_ASSERT(errstr); " << std::endl
     << pad3 << "PROTOBUF_C_GI_RAISE_EXCEPTION(error, " << domain << ", PROTO_GI_ERROR_XML_TO_PB_FAILURE,"
                "\"%s (%d)\\n\", errstr, status );" << std::endl
     << pad2 << "}" << std::endl
     << pad2 << "rw_xml_manager_release(rw_xml_mgr);" << std::endl
     << pad1 << "}" << std::endl << std::endl;
  return;
}

void PbMessage::output_gi_to_pb_cli_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  // Output the comment block with annotation
  os << "/**"                                         << std::endl
     << " *" << get_gi_c_identifier("to_pb_cli") << ":"    << std::endl
     << " *@boxed:"                                   << std::endl
     << " *@ymodel      : (in):"                      << std::endl
     << " **/"                                        << std::endl;
  os << pad1 << "gchar* " << get_gi_c_identifier("to_pb_cli")
             << "(" << get_gi_typename() + "* boxed, "
             << "void * ymodel);" << std::endl;

  return;
}

void PbMessage::output_gi_to_pb_cli_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent+2, ' ');

  os << pad1 << "gchar* " << get_gi_c_identifier("to_pb_cli")
             << "(" << get_gi_typename() + "* boxed, "
             << "void * ymodel)" << std::endl;

  os << pad1 << "{" << std::endl
     << pad2 << "char *cli_str = rw_pb_to_cli(ymodel,(ProtobufCMessage *)boxed->s.message);" << std::endl
     << pad2 << "return cli_str;" << std::endl
     << pad1 << "}" << std::endl << std::endl;
}

void PbMessage::output_gi_desc_keyspec_to_entry_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  os << pad1 << "/**\n"
     << pad1 << " * " << get_gi_c_desc_identifier("keyspec_to_entry") << ":\n"
     << pad1 << " * Returns: (transfer full)\n"
     << pad1 << " **/\n"
     << pad1 << get_gi_typename(output_t::path_entry) << "* "
             << get_gi_c_desc_identifier("keyspec_to_entry")
             << "(const " << get_gi_desc_typename() << "* pbcmd, rw_keyspec_path_t* ks);\n"
     << "\n";
}

void PbMessage::output_gi_desc_keyspec_to_entry_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  os << pad1 << get_gi_typename(output_t::path_entry) << "* "
             << get_gi_c_desc_identifier("keyspec_to_entry")
             << "(const " << get_gi_desc_typename() << "* pbcmd, rw_keyspec_path_t* ks)\n"
     << pad1 << "{\n"
     << pad1 << "  return (" << get_gi_typename(output_t::path_entry) << "*)\n"
     << pad1 << "    rw_keyspec_entry_check_and_create_gi_dup_of_type(ks, NULL, &pbcmd->base);\n"
     << pad1 << "}\n\n";
}

void PbMessage::output_gi_desc_keyspec_leaf_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  os << pad1 << "/**\n"
     << pad1 << " * " << get_gi_c_desc_identifier("keyspec_leaf") << ":\n"
     << pad1 << " * @pbcmd: \n"
     << pad1 << " * @ks: (transfer none) \n"
     << pad1 << " * Returns: (transfer none) (nullable) \n"
     << pad1 << " **/\n"
     << pad1 << "const gchar* "
             << get_gi_c_desc_identifier("keyspec_leaf")
             << "(const " << get_gi_desc_typename() << "* pbcmd, rw_keyspec_path_t *ks);\n"
             << std::endl;
}

void PbMessage::output_gi_desc_keyspec_leaf_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  os << pad1 << "const gchar* "
             << get_gi_c_desc_identifier("keyspec_leaf")
             << "(const " << get_gi_desc_typename() << "* pbcmd, rw_keyspec_path_t *ks)\n"
     << pad1 << "{\n"
     << pad1 << "  return rw_keyspec_path_leaf_name(ks, NULL, &pbcmd->base); \n"
     << pad1 << "}\n\n";
}

void PbMessage::output_gi_desc_retrieve_descriptor_method_decl(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');

  os << "/**" << std::endl;
  os << " * " << get_gi_c_desc_identifier("retrieve_descriptor") << ":" << std::endl
     << " * @pbcmd:" << std::endl
     << " * Returns: (type ProtobufC.MessageDescriptor)" << std::endl
     << " **/" << std::endl;
  os << pad1 << "const ProtobufCMessageDescriptor* " << get_gi_c_desc_identifier("retrieve_descriptor")
     << "(" << get_gi_desc_typename() << "* pbcmd);" << std::endl;
}

void PbMessage::output_gi_desc_retrieve_descriptor_method(std::ostream& os, unsigned indent)
{
  std::string pad1(indent, ' ');
  std::string pad2(indent+2, ' ');

  os << pad1 << "const ProtobufCMessageDescriptor* " << get_gi_c_desc_identifier("retrieve_descriptor")
             << "(" << get_gi_desc_typename() << "* pbcmd)"  << std::endl;
  os << pad1 << "{" << std::endl
     << pad1 << "  return &pbcmd->base;"<< std::endl
     << pad1 << "}" << std::endl << std::endl;
}

void PbMessage::output_gi_h_decls(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg &&
        child_pbmsg->get_yang_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST) {
      child_pbmsg->output_gi_h_decls(os, indent);
    }
  }

  if (   is_local_schema_defining_message()
      && !is_schema_suppressed()) {
    output_gi_from_xml_method_decl(os, indent);
    output_gi_from_xml_v2_method_decl(os, indent);
    output_gi_to_xml_method_decl(os, indent);
    output_gi_to_xml_v2_method_decl(os, indent);
    output_gi_to_pb_cli_method_decl(os, indent);

    output_gi_desc_keyspec_to_entry_method_decl(os, indent);
    output_gi_desc_keyspec_leaf_method_decl(os, indent);
    output_gi_desc_retrieve_descriptor_method_decl(os, indent);

    os << std::endl;
  }
}

void PbMessage::output_gi_c_funcs(std::ostream& os, unsigned indent)
{
  std::string pad1(indent,' ');

  for (auto const& pbfld: field_list_) {
    PbMessage* child_pbmsg = pbfld->get_field_pbmsg();
    if (child_pbmsg &&
        child_pbmsg->get_yang_stmt_type() != RW_YANG_STMT_TYPE_LEAF_LIST) {
      child_pbmsg->output_gi_c_funcs(os, indent);
    }
  }

  if (   is_local_schema_defining_message()
      && !is_schema_suppressed()) {
    output_gi_from_xml_method(os, indent);
    output_gi_from_xml_v2_method(os, indent);
    output_gi_to_xml_method(os, indent);
    output_gi_to_xml_v2_method(os, indent);
    output_gi_to_pb_cli_method(os, indent);

    output_gi_desc_keyspec_to_entry_method(os, indent);
    output_gi_desc_keyspec_leaf_method(os, indent);
    output_gi_desc_retrieve_descriptor_method(os, indent);

    os << std::endl;
  }
}

void PbMessage::output_doc_message(
  const doc_file_t file_type,
  std::ostream& os,
  unsigned indent,
  doc_t doc_style,
  const std::string& chapter )
{
  auto pbmodel = get_pbmodel();

  std::string title;
  auto stmt_type = pbnode_->get_yang_stmt_type();
  auto category_msg_type = get_category_msg_type();
  switch (stmt_type) {
    case RW_YANG_STMT_TYPE_CONTAINER:
      title = "container " + pbnode_->get_xml_element_name();
      break;
    case RW_YANG_STMT_TYPE_LIST:
      title = "list " + pbnode_->get_xml_element_name();
      break;
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      title = "leaf-list " + pbnode_->get_xml_element_name();
      break;
    case RW_YANG_STMT_TYPE_RPCIO:
      if (pbnode_->get_yang_fieldname() == "input") {
        title = "rpc input " + pbnode_->get_parent_pbnode()->get_xml_element_name();
      } else {
        title = "rpc output " + pbnode_->get_parent_pbnode()->get_xml_element_name();
      }
      break;
    case RW_YANG_STMT_TYPE_NOTIF:
      title = "notification " + pbnode_->get_xml_element_name();
      break;

    case RW_YANG_STMT_TYPE_ROOT:
      switch (category_msg_type) {
        case PbMsgType::package:
        case PbMsgType::top_root:
        case PbMsgType::data_root:
        case PbMsgType::rpci_root:
        case PbMsgType::rpco_root:
        case PbMsgType::notif_root:
          RW_ASSERT_NOT_REACHED(); // not implemented

        case PbMsgType::message:
        case PbMsgType::rpc_input:
        case PbMsgType::rpc_output:
        case PbMsgType::notif:
        case PbMsgType::group_root:
        case PbMsgType::group:
          RW_ASSERT_NOT_REACHED(); // not expected

        case PbMsgType::data_module:
          title = get_pbmod()->get_ymod()->get_name() + std::string(" data");
          break;

        case PbMsgType::rpci_module:
          title = get_pbmod()->get_ymod()->get_name() + std::string(" rpc input");
          break;

        case PbMsgType::rpco_module:
          title = get_pbmod()->get_ymod()->get_name() + std::string(" rpc output");
          break;

        case PbMsgType::notif_module:
          title = get_pbmod()->get_ymod()->get_name() + std::string(" notification");
          break;

        case PbMsgType::illegal_:
        case PbMsgType::unset:
        case PbMsgType::automatic:
        case PbMsgType::last_:
          RW_ASSERT_NOT_REACHED();
      }
      break;

    case RW_YANG_STMT_TYPE_RPC:
    case RW_YANG_STMT_TYPE_GROUPING:
    case RW_YANG_STMT_TYPE_CHOICE:
    case RW_YANG_STMT_TYPE_CASE:
      RW_ASSERT_NOT_REACHED();

    default:
      RW_ASSERT_NOT_REACHED();
  }

  pbmodel->output_doc_heading( file_type, os, indent, doc_style, chapter, title );

  indent += 2;
  if (RW_YANG_STMT_TYPE_ROOT == stmt_type) {
    const char* ydesc = get_schema_defining_pbmod()->get_ymod()->get_description();
    std::string desc;
    if (ydesc && ydesc[0]) {
      desc = YangModel::adjust_indent_yang( indent, ydesc );
    }

    switch (doc_style) {
      case doc_t::api_entry:
      case doc_t::user_entry:
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "description",
          "Description",
          desc );
        /*
         * ATTN:
         *   Module NS
         *   Module file
         *   Various code identifiers
         */
      default:
        break;
    }
  } else {
    auto ynode = get_ynode();
    RW_ASSERT(ynode);
    switch (doc_style) {
      case doc_t::api_entry:
      case doc_t::user_entry: {
        const char* ydesc = ynode->get_description();
        std::string desc;
        if (ydesc && ydesc[0]) {
          desc = YangModel::adjust_indent_yang( indent, ydesc );
        }
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "description",
          "Description",
          desc );

        // generate leaf descriptions
        std::stringstream leaves;
        for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
          YangNode * ynode = (*fi)->get_ynode(); 
          const rw_yang_stmt_type_t field_stmt_type = ynode->get_stmt_type();
          if (field_stmt_type != RW_YANG_STMT_TYPE_LEAF) {
            continue;
          }

          YangType * type = ynode->get_type();
          RW_ASSERT(type);

          const rw_yang_leaf_type_t field_leaf_type = type->get_leaf_type();
          std::string const description = ynode->get_description();
          
          leaves << "name: " <<  ynode->get_name() 
                 << ", type: " << rw_yang_leaf_type_string(field_leaf_type);

          if (description.length() >0) {
            leaves << ", description: " << description;
          }

          leaves << std::endl;
            
        }
        std::string const leaves_desc = YangModel::adjust_indent_yang( indent, leaves.str() );
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "leaves",
          "Leaves",
          leaves_desc );

        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "xpath_path",
          "XPath Path",
          get_xpath_path() );
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "xpath_keyed_path",
          "XPath Keyed Path",
          get_xpath_keyed_path() );
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "rw_rest_uri_path",
          "RW.REST URI Path",
          get_rwrest_uri_path() );
        break;
      }
      default:
        break;
    }
    
    switch (doc_style) {
      case doc_t::api_entry: {
        std::string prefix;
        switch (get_category_msg_type()) {
          case PbMsgType::package:
          case PbMsgType::top_root:
          case PbMsgType::data_root:
          case PbMsgType::rpci_root:
          case PbMsgType::rpco_root:
          case PbMsgType::notif_root:
            RW_ASSERT_NOT_REACHED(); // not implemented

          case PbMsgType::message:
            RW_ASSERT_NOT_REACHED(); // should never see these

          case PbMsgType::data_module:
          case PbMsgType::group_root:
          case PbMsgType::group:
            if (ynode->is_config()) {
              prefix = "C,";
              pbmodel->output_doc_field( file_type, os, indent, chapter,
                "rw_xpath_path",
                "RW Keyspec XPath Path",
                prefix + get_xpath_path() );
              pbmodel->output_doc_field( file_type, os, indent, chapter,
                "rw_xpath_keyed_path",
                "RW Keyspec XPath Keyed Path",
                prefix + get_xpath_keyed_path() );
            }
            if (pbnode_->has_op_data()) {
              prefix = "D,";
              goto output_rw_xpath;
            }
            break;

          case PbMsgType::rpci_module:
          case PbMsgType::rpc_input:
            prefix = "I,";
            goto output_rw_xpath;

          case PbMsgType::rpco_module:
          case PbMsgType::rpc_output:
            prefix = "O,";
            goto output_rw_xpath;

          case PbMsgType::notif_module:
          case PbMsgType::notif:
            prefix = "N,";
          output_rw_xpath:
            pbmodel->output_doc_field( file_type, os, indent, chapter,
              "rw_xpath_path",
              "RW Keyspec XPath Path",
              prefix + get_xpath_path() );
            pbmodel->output_doc_field( file_type, os, indent, chapter,
              "rw_xpath_keyed_path",
              "RW Keyspec XPath Keyed Path",
              prefix + get_xpath_keyed_path() );
            break;

          case PbMsgType::illegal_:
          case PbMsgType::unset:
          case PbMsgType::automatic:
          case PbMsgType::last_:
            RW_ASSERT_NOT_REACHED();
        }

        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "protobuf_type",
          "Protobuf Type",
          get_proto_message_path() );
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "gi_python_type",
          "Python Proto-GI Type",
          get_gi_python_typename() );

        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "c_pbc_struct",
          "C Protobuf-C Struct Type",
          get_pbc_message_typename() );

        std::string rwpb = get_rwpb_long_alias();
        if (!has_conflict()) {
          std::string alias = get_rwpb_short_alias();
          if (alias.length()) {
            rwpb.push_back('\n');
            rwpb.append(alias);
          }
          alias = get_rwpb_msg_new_alias();
          if (alias.length()) {
            rwpb.push_back('\n');
            rwpb.append(alias);
          }
        }
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "c_rwpb_identifiers",
          "C RWPB Identifiers",
          rwpb );

        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "c_ypbc_identifiers",
          "YPBC Base Identifier",
          get_ypbc_long_alias() );

        /*
         * ATTN
         * Yang hierarchy:
         *   rw-base:colony/                rw-base.yang:123
         *     rw-base:network-context/     rw-base.yang:456
         *       rw-fpath:interface/        rw-fpath.yang:1985
         *                                  grouping rw-fpath.yang:345 uses rw-fpath.yang:524
         *         rw-fpath:vrf/            rw-fpath.yang:2113
         * Source line numbers:
         *   The element itself
         *   grouping/uses
         *   choice/case
         *   augment
         *   Parent elements
         * Keyspecs:
         *   C:
         *     KS struct
         *     DP struct
         *     PE struct
         *     LO struct
         *   Protobuf:
         *     KS full path
         *     DP full path
         *     PE full path
         *     LO full path
         * CONFD tag and NS numbers
         * CONFD hkeypath string
         * PROTO tag and NS numbers
         * PROTO tag paths
         *
         * C Code read example:
         * C Code write example:
         * Py code read example:
         * Py code write example:
         */
        break;
      }
      case doc_t::user_entry:
        if (ynode->is_config()) {
          pbmodel->output_doc_field( file_type, os, indent, chapter,
            "json_config_sample",
            "JSON Config Sample",
            ynode->get_rwrest_json_sample_element(0, 2, true) );
        }
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "json_sample",
          "JSON Sample",
          ynode->get_rwrest_json_sample_element(0, 2, false) );
        // ATTN: Need to differentiate between POST/PUT/GET?
        // ATTN: Keys only deep example?

        if (ynode->is_config()) {
          pbmodel->output_doc_field( file_type, os, indent, chapter,
            "xml_config_sample",
            "XML Config Sample",
            ynode->get_xml_sample_element(0, 2, true, true) );
        }
        pbmodel->output_doc_field( file_type, os, indent, chapter,
          "xml_sample",
          "XML Sample",
          ynode->get_xml_sample_element(0, 2, false, true) );
        // ATTN: Need to differentiate between POST/PUT/GET?

        /*
         * ATTN:
         * CLI commands example:
         */
        break;

      default:
        break;
    }
  }

  switch (doc_style) {
    case doc_t::api_entry:
    case doc_t::user_entry:
      os << std::endl;
      break;

    default:
      break;
  }
  indent -= 2;

  // Output the embedded messages, if any
  unsigned sub_chapter = 1;
  for (auto fi = field_list_.begin(); fi != field_list_.end(); ++fi) {
    PbMessage* field_pbmsg = (*fi)->get_field_pbmsg();
    if (field_pbmsg) {
      field_pbmsg->output_doc_message( file_type, os, indent, doc_style,
        chapter + "." + std::to_string(sub_chapter)),
      ++sub_chapter;
    }
  }
}

} // namespace rw_yang

// End of yangpbc_message.cpp
