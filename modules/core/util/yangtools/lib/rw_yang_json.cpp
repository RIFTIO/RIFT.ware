
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
  bool end_here = false;
  bool end_case = false;

  YangNode* ychoice = yn_->get_choice();
  if (ychoice) {
    // Condition checks if its the first time we are encountering a choice
    // or we encountered a new choice statement
    if (choice_stk_.empty() || ychoice != choice_stk_.top()) {
      // This is a nested choice, indicate that we do not
      // have to end the case statement.
      if (!choice_stk_.empty()) {
        end_case = true;
      }
      // Track the choice statement
      choice_stk_.push(ychoice);
      print_case_choice_header(ychoice);
      inside_choice_ = true;
      curr_case_ =  nullptr;
    }
  }

  if (inside_choice_) {
    auto ycase = yn_->get_case();
    if (ycase) {
      // A new case statement or another case statement
      if (curr_case_ != ycase) {
        // This is a successive case statement, end the
        // previous case.
        if (curr_case_ && !end_case) {
          fmt_.print_string("{}\n");
          print_end_case_choice();// end case
          fmt_.seperator();
        }
        print_case_choice_header(ycase);
        curr_case_ = ycase;
      }
      // Reset end_case
      end_case = false;
      auto next = yn_->get_next_sibling();
      // Last case statement
      if (!next || next->get_case() == nullptr) {
        end_here = true;
      }
      // Nested choice statement
      if (next && next->get_choice() != choice_stk_.top()) {
        end_here = true;
      }
      // End the current case
      if (!end_here && next && next->get_case() != curr_case_) {
        end_case = true;
      }
    }
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
  if (end_here) {
    print_end_case_choice();// end case
    print_end_case_choice();// end choice
    end_here = false;
    curr_case_ = nullptr;
    choice_stk_.pop();
    inside_choice_ = !(choice_stk_.empty());
  }
  if (end_case) {
    print_end_case_choice();
    curr_case_ = nullptr;
  }
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
  fmt_.print_key("key");
  fmt_.begin_object_list();
  auto key_iter = yn_->key_begin();
  while (key_iter != yn_->key_end()) {
    fmt_.print_value(key_iter->get_key_node()->get_name());
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

void JsonPrinter::print_case_choice_header(YangNode* node)
{
  RW_ASSERT (node);
  auto tmp = yn_;
  yn_ = node;
  fmt_.begin_object();
  print_common_attribs();
  yn_ = tmp;
  fmt_.print_key("properties");
  fmt_.begin_object_list();
}

void JsonPrinter::print_end_case_choice()
{
  fmt_.end_object_list();
  fmt_.end_object();
}
