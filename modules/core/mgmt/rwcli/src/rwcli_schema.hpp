
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
 * @file rwcli_schema.hpp
 * @author Balaji Rajappa (balaji.rajappa@riftio.com)
 * @date 10/14/2015
 * @brief Schema Manager for RW.CLI
 *
 * SchemaManager maintains the schemas loaded by the RW.CLI.
 */

#ifndef __RWCLI_SCHEMA_HPP__
#define __RWCLI_SCHEMA_HPP__

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "rwlib.h"
#include "yangncx.hpp"

// ATTN: Do we need a new namespace rw_cli?
namespace rw_yang {

/**
 * SchemaManager loads and maintains the schemas. 
 */
class SchemaManager
{
public:
 
  /**
   * Constructor for SchemaManager.
   *
   * Creates a SchemaUpater which monitors for schema change
   */ 
  SchemaManager();

  /**
   * Specialized constructor used for setting a custom schema directory.
   */
  SchemaManager(const std::string& schema_path);

  /**
   * Default destructor
   */
  ~SchemaManager() = default;

public:

  /**
   * Load all the yang modules present in the latest
   * valid schema version directory.
   */
  bool load_all_schemas();


  /**
   * Get the latest version cli manifest directory.
   */
  std::string get_cli_manifest_dir();

  /**
   * Load the schema into the YangModel.
   *
   * This will make the schema available of the RW.CLI.
   */
  rw_status_t load_schema(const std::string& schema_name);

  /**
   * Prints the schemas loaded into the provided output stream.
   */
  void show_schemas(std::ostream& os);

public:
  const char* RW_BASE_MODEL = "rw-base";

  /**
   * Stores the names of loaded schemas
   */
  std::unordered_set<std::string> loaded_schema_;

  /**
   * Collection of all the loaded YangModules, which will be used by the CLI for
   * parsing and completion.
   */
  YangModel::ptr_t model_;

  /**
   * Schema directory which can be used for monitoring
   */
  std::string schema_path_;

  /**
   * The rift_install path
   */
  std::string rift_install_;

  /**
   * The rift_var_root path
   */
  std::string rift_var_root_;

  /**
   * The last time the schema directory changed
   */
  std::time_t last_schema_change_;

};



} //namespace rw_yang

#endif // __RWCLI_SCHEMA_HPP__
