/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 1/22/16
 * 
 */

#ifndef __RWYANGUTIL_ARGUMENT_PARSER_H__
#define __RWYANGUTIL_ARGUMENT_PARSER_H__

#include <string>
#include <list>
#include <utility>
#include <vector>
#include <map>

namespace rwyangutil {

/*!
 * ArgumentParser
 * Parser for command line commands and arguments
 */
class ArgumentParser
{
 private:
  std::map<std::string, std::vector<std::string>> arguments_;
  std::vector<std::string> ordered_arguments_;

  void parse_arguments(const char* const* argv, size_t argc);
 public:
  /*
   * Constructor
   * Takes a pointer to 2D character array and 
   * its size.
   */
  ArgumentParser(const char* const* argv, size_t argc);

  ArgumentParser(const ArgumentParser& other) = delete;
  void operator=(const ArgumentParser& other) = delete;

 public:
  typedef typename std::vector<std::string>::const_iterator const_iterator;

  void print_help() const;

  /*!
   * Exposes iterator of the underlying
   * container which holds the commands and
   * arguments.
   */
  const_iterator begin() const;
  const_iterator end() const;

  /*
   * Gets all the arguments associated with the option.
   * Throws std::out-of-range exception if option not
   * found in the internal container.
   */
  std::vector<std::string> const & get_params_list(std::string const & option) const;

  /*
   * Gets the argument associated with the option.
   * Returns the first argument if there are many arguments
   * to the option.
   * Throws std::out-of-range exception if option not
   * found in the internal container OR when no argument
   * is peresent for the option.
   */
  const std::string& get_param(std::string const & option) const;


  /*!
   * Checks if a command line command was provided or not
   */
  bool exists(const std::string& option) const;
};
}

#endif
