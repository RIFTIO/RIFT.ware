
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file util.hpp
 * @author Tim Mortsolf (tim.mortsolf@riftio.com)
 * @date 07/20/2013
 * @brief Utility functions for string manipulation
 */

#include <string>
#include <sstream>
#include <vector>
#include <memory>

#ifndef __RW_YANGTOOL_LIB_UTIL_HPP_
#define  __RW_YANGTOOL_LIB_UTIL_HPP_

namespace rw_yang
{

__attribute__((always_inline))
extern inline void split_line(std::string &s,
                              char delim,
                              std::vector<std::string> &elements)
{
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    if (item == std::string("\n")) {
      continue;
    }
    if (*item.rbegin() == '\n') {
      // item.erase(item.end(), 1);
      item.erase(item.end() - 1, item.end());
    }
    elements.push_back(item);
  }
}

/**
 * Strip leading whitespace from a string.
 *
 * @param[in] str Reference to the string to be trimmed. Modified in-place.
 * @param[in] loc Locale to use (optional).
 * @return        void
 */
void ltrim(std::string& str, const std::locale& loc = std::locale());
__attribute__((always_inline))
extern inline void ltrim(std::string& str, const std::locale& loc)
{
  std::string::size_type pos = 0;
  while (pos < str.size() && isspace(str[pos], loc)) {
    pos++;
  }
  str.erase(0, pos);
}
 
/**
 * Strip trailing whitespace from a string.
 *
 * @param[in] str Reference to the string to be trimmed. Modified in-place.
 * @param[in] loc Locale to use (optional).
 * @return        void
 */
void rtrim(std::string& str, const std::locale& loc = std::locale());
__attribute__((always_inline))
extern inline void rtrim(std::string& str, const std::locale& loc)
{
  std::string::size_type pos = str.size();
  while (pos > 0 && isspace(str[pos - 1], loc)) {
    pos--;
  }
  str.erase(pos);
}
 
/**
 * Strip both leading and trailing whitespace from a string.
 *
 * @param[in] str Reference to the string to be trimmed. Modified in-place.
 * @param[in] loc Locale to use (optional).
 * @return        void
 */
void btrim(std::string& str, const std::locale& loc = std::locale());
__attribute__((always_inline))
extern inline void btrim(std::string& str, const std::locale& loc)
{
  ltrim(str, loc);
  rtrim(str, loc);
}

__attribute__((always_inline))
extern inline std::string repeat( const std::string &word, int times )
{
  std::string result ;
  result.reserve(times*word.length()); // avoid repeated reallocation
  for (int a = 0; a < times; a++) {
    result += word ;
  }
  return result ;
}




/**
 * Static cast a unique_ptr<> to another unique_ptr<>.  Ownership moves
 * from the original unique_ptr<> to the returned unique_ptr<>.
 *
 * ATTN: This is half-baked!  It is dangerous when the deleters are
 * different!
 *
 * @param from [in] Reference to source unique_ptr<>.
 * @return A unique_ptr<>.  nullptr if original is nullptr.
 */
template <
  class to_uptr_t,
  class from_uptr_t,
  class to_element_t = typename to_uptr_t::element_type,
  class to_deleter_t = typename to_uptr_t::deleter_type,
  class to_pointer_t = typename to_uptr_t::pointer,
  class from_pointer_t = typename std::remove_reference<from_uptr_t>::type::pointer>
to_uptr_t static_cast_move(from_uptr_t&& from) noexcept
{
  from_pointer_t from_p = from.get();
  to_pointer_t to_p = static_cast<to_pointer_t>(from_p);
  from.release();
  return std::unique_ptr<to_element_t,to_deleter_t>(to_p);
}


/**
 * Dynamic cast a unique_ptr<> to another unique_ptr<>.  Ownership
 * moves from the original unique_ptr<> to the returned unique_ptr<> if
 * the dynamic cast is successful.  Otherwise, ownership stays with the
 * old unique_ptr<>.
 *
 * ATTN: This is half-baked!  It is dangerous when the deleters are
 * different!
 *
 * @param from [in] Reference to source unique_ptr<>.
 * @return A unique_ptr<>.  nullptr if original is nullptr or the if
 *   the dynamic cast fails.
 */
template <
  class to_uptr_t,
  class from_uptr_t,
  class to_element_t = typename to_uptr_t::element_type,
  class to_deleter_t = typename to_uptr_t::deleter_type,
  class to_pointer_t = typename to_uptr_t::pointer,
  class from_pointer_t = typename std::remove_reference<from_uptr_t>::type::pointer>
to_uptr_t dynamic_cast_move(from_uptr_t&& from) noexcept
{
  from_pointer_t from_p = from.get();
  to_pointer_t to_p = dynamic_cast<to_pointer_t>(from_p);
  if (!to_p) {
    return to_uptr_t();
  }
  from.release();
  return std::unique_ptr<to_element_t,to_deleter_t>(to_p);
}


} // namespace rw_yang


/**
 * Status of parsing a line with quotes and spaces. 
 */
typedef enum rw_cli_string_parse_status_e {
  RW_CLI_STRING_PARSE_STATUS_OK,
  RW_CLI_STRING_PARSE_STATUS_QUOTES_NOT_CLOSED,
  RW_CLI_STRING_PARSE_STATUS_BLANK_WORD_IN_QUOTES,
  RW_CLI_STRING_PARSE_STATUS_INVALID_CHARACTERS,
}rw_cli_string_parse_status_t;

/**
 * Parse a line of CLI.
 * The input is a string with words split by tabs or spaces. Quoted strings
 * could also be present. Substrings with quotes are considered to be one
 * string in the returned array
 *
 * @return RW_CLI_STRING_PARSE_STATUS_OK on success
 * @return RW_CLI_STRING_PARSE_STATUS_QUOTES_NOT_CLOSED when quotes dont match
 * @return RW_CLI_STRING_PARSE_STATUS_BLANK_WORD_IN_QUOTES when a quoted string
 *         is empty
 */

extern inline rw_cli_string_parse_status_t
parse_input_line(
    std::string &s, /** @param[in] string to be parsed */
    std::vector<std::string> &elements) /** @param[out] list of parsed expressions */
{
  std::string item;

  std::string::iterator it = s.begin();

  while (it != s.end()) {
    while (::isspace (*it) && it != s.end()) {
      it++;
    }

    /* Quoted String */
    if (*it == '"') {
      std::string::iterator esc = it;
      *it++;
      while (it != s.end() && (*it != '"' || *esc == '\\')) {
        item.push_back (*it);
        it++;
        esc++;
      }

      if (it == s.end()) {        
        return RW_CLI_STRING_PARSE_STATUS_QUOTES_NOT_CLOSED;
      }

      if (!item.length()) {
        return RW_CLI_STRING_PARSE_STATUS_BLANK_WORD_IN_QUOTES;
      }

      // add the closing quotes
      elements.push_back(item);
      item.clear();
      it++;
    }

    while (!isspace(*it) && (*it != '"') && it != s.end()) {
      item.push_back (*it);
      it++;
    }
      
    if (item.length()) {
      elements.push_back(item);
      item.clear();
    }
  }
  return RW_CLI_STRING_PARSE_STATUS_OK;
}

#endif
