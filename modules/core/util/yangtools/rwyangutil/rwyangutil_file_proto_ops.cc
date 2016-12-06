
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
 * @file rw_file_proto_ops.cc
 * @author Arun Muralidharan
 * @date 01/10/2015
 * @brief App library file protocol operations
 * @details App library file protocol operations
 */

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "rwyangutil.h"
#include "rwyangutil_argument_parser.hpp"
#include "rwyangutil_file_proto_ops.hpp"
#include "rwyangutil_helpers.hpp"

namespace fs = boost::filesystem;
namespace rwyangutil {

bool FileProtoOps::validate_command(std::string const & command)
{
  if (command_map.find(command) != command_map.end()) {
    return true;
  }
  return false;
}

bool FileProtoOps::execute_command(std::string const & command, std::vector<std::string> const & params)
{
  auto command_handler = command_map.find(command);
  if (command_handler != command_map.end()) {
    return command_handler->second(this, params);
  }

  return false;
}


FileProtoOps::FileProtoOps()
{
  command_map["--lock-file-create"]         = &FileProtoOps::create_lock_file;
  command_map["--lock-file-delete"]         = &FileProtoOps::delete_lock_file;
  command_map["--version-dir-create"]       = &FileProtoOps::create_new_version_directory;
  command_map["--create-schema-dir"]        = &FileProtoOps::create_schema_directory;
  command_map["--remove-schema-dir"]        = &FileProtoOps::remove_schema_directory;
  command_map["--prune-schema-dir"]         = &FileProtoOps::prune_schema_directory;
  command_map["--rm-unique-mgmt-ws"]        = &FileProtoOps::remove_unique_mgmt_workspace;
  command_map["--archive-mgmt-persist-ws"]  = &FileProtoOps::archive_mgmt_persist_workspace;
  command_map["--rm-persist-mgmt-ws"]       = &FileProtoOps::remove_persist_mgmt_workspace;
  command_map["--get-rift-var-root"]        = &FileProtoOps::get_rift_var_root_path;
}

bool FileProtoOps::create_lock_file(std::vector<std::string> const & params)
{
  return rwyangutil::create_lock_file();
}

bool FileProtoOps::delete_lock_file(std::vector<std::string> const & params)
{
  return rwyangutil::remove_lock_file();
}

bool FileProtoOps::create_new_version_directory(std::vector<std::string> const & params)
{
  return rwyangutil::update_version_directory();
}

bool FileProtoOps::remove_schema_directory(std::vector<std::string> const & params)
{
  return rwyangutil::remove_schema_directory();
}

bool FileProtoOps::create_schema_directory(std::vector<std::string> const & params)
{
  if (params.size() < 1) {
    std::cerr << "Create schema directory requires 1 or more northbound schema listing files" << std::endl;
    return false;
  }
  return rwyangutil::create_schema_directory(params);
}

bool FileProtoOps::remove_unique_mgmt_workspace(std::vector<std::string> const & params)
{
  bool ok;
  std::string dir_name;
  std::tie(ok, dir_name) = form_rift_var_root_path(params);

  if (!ok) return false;

  return rwyangutil::remove_unique_mgmt_workspace(std::move(dir_name));
}

bool FileProtoOps::remove_persist_mgmt_workspace(std::vector<std::string> const & params)
{
  bool ok;
  std::string dir_name;
  std::tie(ok, dir_name) = form_rift_var_root_path(params);

  if (!ok) return false;
  return rwyangutil::remove_persist_mgmt_workspace(std::move(dir_name));
}

bool FileProtoOps::archive_mgmt_persist_workspace(std::vector<std::string> const & params)
{
  bool ok;
  std::string dir_name;
  std::tie(ok, dir_name) = form_rift_var_root_path(params);

  if (!ok) return false;
  return rwyangutil::archive_mgmt_persist_workspace(std::move(dir_name));
}

bool FileProtoOps::prune_schema_directory(std::vector<std::string> const & params)
{
  return rwyangutil::prune_schema_directory();
}

bool FileProtoOps::get_rift_var_root_path(std::vector<std::string> const & params)
{
  std::cout << form_rift_var_root_path_impl(params) << std::endl;
  return true;
}

std::string FileProtoOps::form_rift_var_root_path_impl(std::vector<std::string> const & params)
{
  auto rift_var_root = getenv("RIFT_VAR_ROOT");
  if (rift_var_root)    { return std::string(rift_var_root); }
  if (is_rooted_path()) { return get_rift_install(); }

  std::string test_name;
  const char* err_msg = nullptr;

  for (auto& arg : params) {
    std::vector<std::string> arg_val;
    boost::split(arg_val, arg, boost::is_any_of(":"));
    
    if (arg_val.size() != 2) {
      err_msg = "Invalid arguments given";
      std::cerr << err_msg << std::endl;
      throw err_msg;
    }
    
    boost::trim(arg_val[0]);
    boost::trim(arg_val[1]);
    
    if (arg_val[0] == "test-name") test_name = arg_val[1];
  }

  if (!test_name.length()) {
    err_msg = "Test name not provided";
    std::cerr << err_msg << std::endl;;
    throw err_msg;
  }
  
  auto path_to_ws = get_rift_install() + "/var/rift/";
  std::string test_dir_name;
  auto find_pattern = test_name;
  
  if (!fs::exists(path_to_ws)) {
    err_msg = "Path <rift_install>/var/rift does not exist";
    std::cerr << err_msg << std::endl;
    throw err_msg;
  }
  
  for (fs::directory_iterator it(path_to_ws); it != fs::directory_iterator(); ++it) {
    const auto& dir_name = it->path().string();
    if (!fs::is_directory(dir_name)) continue;
      if (dir_name.find(find_pattern) != std::string::npos) {
        test_dir_name = std::move(dir_name);
        break;
      }
  }

  if (!test_dir_name.length()) throw "Test directory name not found";

  return test_dir_name;
}

std::pair<bool, std::string> FileProtoOps::form_rift_var_root_path(std::vector<std::string> const & params)
{
  std::string test_dir;

  try {
    test_dir = form_rift_var_root_path_impl(params);
  } catch (const char* exp_msg) {
    std::cerr << "ERROR: " << exp_msg << std::endl;
    return std::make_pair(false, "");
  }

  return std::make_pair(true, test_dir);
}

}

