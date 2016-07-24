/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 1/22/16
 * 
 */

#ifndef __RWYANGUTIL_FILE_PROTO_OPS_H__
#define __RWYANGUTIL_FILE_PROTO_OPS_H__

#include <functional>
#include <string>
#include <vector>
#include <map>

namespace rwyangutil {
class FileProtoOps
{
 public:

  typedef std::map<std::string, std::function<bool(FileProtoOps*, std::vector<std::string> const & )>> command_func_map_t;

  bool execute_command(std::string const & command, std::vector<std::string> const & params);

  bool validate_command(std::string const & command);

  FileProtoOps();

 private:

  // exposed via cli arguments
  bool archive_mgmt_persist_workspace(std::vector<std::string> const & params);
  bool remove_persist_mgmt_workspace(std::vector<std::string> const & params);
  bool remove_unique_mgmt_workspace(std::vector<std::string> const & params);

  bool create_lock_file(std::vector<std::string> const & params);
  bool delete_lock_file(std::vector<std::string> const & params);

  bool create_new_version_directory(std::vector<std::string> const & params);
  bool create_schema_directory(std::vector<std::string> const & params);
  bool remove_schema_directory(std::vector<std::string> const & params);
  bool init_schema_directory(std::vector<std::string> const & params);
  bool prune_schema_directory(std::vector<std::string> const & params);

 private:
  command_func_map_t command_map;
};

}

#endif
