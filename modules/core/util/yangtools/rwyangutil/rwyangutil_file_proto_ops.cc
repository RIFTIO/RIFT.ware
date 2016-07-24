
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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

  command_map["--rm-unique-mgmt-ws"]       = &FileProtoOps::remove_unique_mgmt_workspace;
  command_map["--archive-mgmt-persist-ws"] = &FileProtoOps::archive_mgmt_persist_workspace;
  command_map["--rm-persist-mgmt-ws"]      = &FileProtoOps::remove_persist_mgmt_workspace;
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
  return rwyangutil::remove_unique_mgmt_workspace();
}

bool FileProtoOps::remove_persist_mgmt_workspace(std::vector<std::string> const & params)
{
  return rwyangutil::remove_persist_mgmt_workspace();
}

bool FileProtoOps::archive_mgmt_persist_workspace(std::vector<std::string> const & params)
{
  return rwyangutil::archive_mgmt_persist_workspace();
}

bool FileProtoOps::prune_schema_directory(std::vector<std::string> const & params)
{
  return rwyangutil::prune_schema_directory();
}
}

