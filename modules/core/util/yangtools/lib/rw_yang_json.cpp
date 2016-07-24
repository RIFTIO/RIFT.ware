
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*!
 * @file rw_yang_json.cpp
 *
 * Converts Yang Node to JSON string
 */

#include <iostream>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include "rw_yang_json.h"
#include "yangmodel_gi.h"

using namespace rw_yang;

// YangNode C APIs
void rw_yang_node_to_json_schema(rw_yang_node_t* ynode, char** ostr, bool_t pretty_print)
{
  auto yn = static_cast<YangNode*>(ynode);
  std::string json = yn->to_json_schema(pretty_print);
  if (!json.length() || json == "{}") {
    json = "";
  }

  char* new_str = strdup(json.c_str());
  *ostr = new_str;
}

std::string YangNode::to_json_schema(bool pretty_print)
{
  std::ostringstream oss;
  JsonPrinter printer(oss, this, pretty_print);
  oss << "{";
  printer.convert_to_json();
  oss << "}";

  // Could we reuse the buffer of stringstream
  // rather than copying it ?
  // Not clear from standard if stringstream is required 
  // to store the content in contiguous memory
  return oss.str();
}

std::string ctolower(const std::string& str)
{
  std::string nstr(str);
  std::transform(nstr.begin(), nstr.end(), nstr.begin(), ::tolower);
  return nstr;
}

void JsonPrinter::convert_to_json()
{
  if (!yn_->is_config()) {
    return;
  }

  if (first_call_) {
    fmt_.print_key(yn_->get_name());
    first_call_ = false;
  }

  fmt_.begin_object();
  print_common_attribs();
  auto stmt_type = yn_->get_stmt_type();

  switch (stmt_type)
  {
    case RW_YANG_STMT_TYPE_LEAF:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      print_leaf_node();
      break;

    case RW_YANG_STMT_TYPE_LIST:
      print_json_list();
      break;

    case RW_YANG_STMT_TYPE_CONTAINER:
      print_json_container();
      break;

    case RW_YANG_STMT_TYPE_ANYXML:
      break;

    default:
      RW_ASSERT_NOT_REACHED ();
  };

  fmt_.end_object();
 
}


void JsonPrinter::print_leaf_node()
{
  fmt_.print_key("data-type");
  auto leaf_type = yn_->get_type()->get_leaf_type();

  switch (leaf_type) {
    case RW_YANG_LEAF_TYPE_ENUM:
      print_leaf_node_enum();
      break;
    case RW_YANG_LEAF_TYPE_UNION:
      print_leaf_node_union();
      break;
    case RW_YANG_LEAF_TYPE_LEAFREF:
      print_leaf_node_leafref();
      break;
    case RW_YANG_LEAF_TYPE_BITS:
      print_leaf_node_bits();
      break;
    case RW_YANG_LEAF_TYPE_IDREF:
      print_leaf_node_idref();
      break;
    default:
      fmt_.print_value(
          ctolower(rw_yang_leaf_type_string(
              yn_->get_type()->get_leaf_type()
              )).c_str());
      break;
  };

  fmt_.seperator();
  fmt_.print_string("\"properties\": []");
}

void JsonPrinter::print_leaf_node_enum()
{
  fmt_.begin_object();
  fmt_.print_key("enumeration");
  fmt_.begin_object();
  fmt_.print_key("enum");
  fmt_.begin_object();

  auto value_iter = yn_->value_begin();
  while (value_iter != yn_->value_end())
  {
    fmt_.print_key(value_iter->get_name());
    fmt_.begin_object();
    fmt_.print_key_value("value", value_iter->get_position());
    fmt_.end_object();

    if (++value_iter != yn_->value_end()) {
      fmt_.seperator();
    }
  }

  fmt_.end_object();
  fmt_.end_object();
  fmt_.end_object();
}

void JsonPrinter::print_leaf_node_bits()
{
  fmt_.begin_object();
  fmt_.print_key("bits");
  fmt_.begin_object();
  fmt_.print_key("bit");
  fmt_.begin_object();

  auto value_iter = yn_->value_begin();
  while (value_iter != yn_->value_end())
  {
    fmt_.print_key(value_iter->get_name());
    fmt_.begin_object();
    fmt_.print_key_value("position", value_iter->get_position());
    fmt_.end_object();
    if (++value_iter != yn_->value_end()) {
      fmt_.seperator();
    }
  }

  fmt_.end_object();
  fmt_.end_object();
  fmt_.end_object();
}

void JsonPrinter::print_leaf_node_union()
{
  fmt_.begin_object();
  fmt_.print_key("union");
  fmt_.begin_object_list();

  auto value_iter = yn_->get_type()->value_begin();

  while (value_iter != yn_->get_type()->value_end())
  {
    fmt_.print_value(value_iter->get_name());
    if (++value_iter != yn_->get_type()->value_end()) {
      fmt_.seperator();
    }
  }
  fmt_.end_object_list();
  fmt_.end_object();
}

void JsonPrinter::print_leaf_node_leafref()
{
  fmt_.begin_object();
  fmt_.print_key("leafref");
  fmt_.begin_object();
  auto path = yn_->get_leafref_path_str();
  fmt_.print_key_value("path", path.c_str());
  fmt_.end_object();
  fmt_.end_object();
}

void JsonPrinter::print_leaf_node_idref()
{
  fmt_.begin_object();
  fmt_.print_key("idref");
  fmt_.begin_object();

  auto value_iter = yn_->get_type()->value_begin();
  if (value_iter == yn_->get_type()->value_end()) {
    fmt_.end_object();
    return;
  }

  std::string base_name;
  if (value_iter->get_prefix()) {
    base_name = value_iter->get_prefix() + std::string(":");
  }
  base_name += std::string(value_iter->get_name());
  fmt_.print_key_value("base", base_name.c_str());
  fmt_.end_object();
  fmt_.end_object();
}


void JsonPrinter::print_common_attribs()
{
  auto stmt_type = yn_->get_stmt_type();

  fmt_.print_key_value_sep("name", yn_->get_rest_element_name().c_str());

  fmt_.print_key_value_sep("type",
        ctolower(rw_yang_stmt_type_string(stmt_type)).c_str());
  fmt_.print_key_value_sep("description", yn_->get_description());

  switch (stmt_type)
  {
    case RW_YANG_STMT_TYPE_LIST:
    case RW_YANG_STMT_TYPE_LEAF_LIST:
      fmt_.print_key_value_sep("cardinality", "0..N");
      break;
    default:
     fmt_.print_key_value_sep("cardinality", "0..1");
  };

  if (yn_->is_mandatory()) {
    fmt_.print_key_value_sep("mandatory", "true");
  }
}

void JsonPrinter::print_json_list()
{
  //Print list keys
  fmt_.print_key("keys");
  fmt_.begin_object_list();
  auto key_iter = yn_->key_begin();
  while (key_iter != yn_->key_end()) {
    fmt_.begin_object();
    fmt_.print_key_value("value", key_iter->get_key_node()->get_name());
    fmt_.end_object();
    if (++key_iter != yn_->key_end()) {
      fmt_.seperator();
    }
  }
  fmt_.end_object_list();
  fmt_.seperator();

  fmt_.print_key("properties");
  fmt_.begin_object_list();

  auto yn_end = yn_->child_end();
  bool first = true;
  for (auto yn_iter = yn_->child_begin(); yn_iter != yn_end; ++yn_iter) {       
    yn_ = &(*yn_iter);
    if (!yn_->is_config()){
      continue;
    }

    if (!first) {
      fmt_.seperator();
    } else {
      first = false;
    }

    convert_to_json();
  }
  fmt_.end_object_list();
}

void JsonPrinter::print_json_container()
{
  fmt_.print_key("properties");
  fmt_.begin_object_list();

  auto yn_end = yn_->child_end();

  bool first = true;
  for (auto yn_iter = yn_->child_begin(); yn_iter != yn_end; ++yn_iter) {       
    yn_ = &(*yn_iter);
    if (!yn_->is_config()){
      continue;
    }

    if (!first) {
      fmt_.seperator();
    } else {
      first = false;
    }


    convert_to_json();
  }
  fmt_.end_object_list();
}
