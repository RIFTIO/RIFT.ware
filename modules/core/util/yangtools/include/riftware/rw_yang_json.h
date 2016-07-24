#ifndef __RW_YANG_JSON_HPP__
#define __RW_YANG_JSON_HPP__

#include <array>
#include <type_traits>
#include <ostream>
#include "yangmodel.h"

namespace rw_yang {

class YangNode;

static const char* indent = "  "; // 2 spaces

class JsonFormatter
{
public:
  JsonFormatter(std::ostream& os, bool pprint):
    os_(os),
    pprint_(pprint) {}
  JsonFormatter(const JsonFormatter&) = delete;
  void operator=(const JsonFormatter&) = delete;

public:
  void begin_object()
  { 
    os_ << "{"; 
    ++stk_level_;
    newline();
  }

  void seperator() 
  {
    os_ << ","; 
    newline();
  }

  void end_object() 
  { 
    --stk_level_;
    newline();
    os_ << "}";
  }

  void begin_object_list() 
  { 
    os_ << "["; 
    ++stk_level_;
    newline();
  }

  void end_object_list() 
  { 
    --stk_level_;
    newline();
    os_ << "]";
  }

  template <typename ValueT>
  void print_key_value(const char* key, 
                       ValueT value)
  {
    print_key(key);
    print_value(value);
  }

  template <typename ValueT>
  void print_key_value_sep(const char* key,
                           ValueT value)
  {
    print_key_value(key, value);
    seperator();
  }

  void print_key(const char* key)
  {
    quote(key) << ": ";
  }

  void print_value(const char* value)
  {
    //manage_indentation();
    if (value) {
      quote(value);
    }
  }

  template <typename ValueT,
           typename = typename std::enable_if<
                        std::is_arithmetic<
                          typename std::remove_reference<
                                   ValueT>::type
                      >::value>::type
           >
  void print_value(ValueT value)
  {
    //manage_indentation();
    os_ << value;
  }

  void print_string(const char* str)
  {
    newline();
    os_ << str;
  }

  int get_stack_level() {
    return stk_level_;
  }

private:
  void manage_indentation()
  {
    if (pprint_) {
      for (int i = 0; i < stk_level_; i++) os_ << indent;
    }
  }

  void newline()
  {
    if (pprint_) {
      os_ << "\n";
    }
    manage_indentation();
  }

  std::ostream& quote(const char* str)
  {
    return (os_ << "\"" << YangModel::utf8_to_escaped_json_string(str) << "\"");
  }

private:
  std::ostream& os_;
  bool pprint_ = false;
  int stk_level_ = 0;
};



class JsonPrinter
{
public:
  JsonPrinter(std::ostream& os, YangNode* yn, bool pprint = false): 
    fmt_(os, pprint), yn_(yn) {}

public:
  void convert_to_json();

private:
  void print_json_list();
  void print_json_container();
  void print_common_attribs();

  void print_leaf_node();
  void print_leaf_node_enum();
  void print_leaf_node_union();
  void print_leaf_node_leafref();
  void print_leaf_node_bits();
  void print_leaf_node_idref();

private:
  bool first_call_ = true;
  JsonFormatter fmt_;
  YangNode* yn_ = nullptr;
};


};

#endif
