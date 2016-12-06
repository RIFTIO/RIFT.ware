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
