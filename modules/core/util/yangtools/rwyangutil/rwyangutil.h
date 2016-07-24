
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



#ifndef _RWYANGUTIL_H_
#define _RWYANGUTIL_H_

#ifdef __cplusplus
#include <set>
#include <string>
#include <vector>
#include <tuple>
#endif

#include <rw_schema_defs.h>

// Lock file liveness, in seconds
#define LIVENESS_THRESH (60)

// Version directory staleness, in seconds
#define STALENESS_THRESH (60)


#ifdef __cplusplus

namespace rwyangutil {

bool create_lock_file();
bool remove_lock_file();

std::vector<std::string> get_schema_subdir_set();
std::vector<std::string> get_latest_subdir_set();
bool create_schema_directory(std::vector<std::string> const & files);
bool widen_northbound_schema(std::vector<std::string> const & files);
bool remove_schema_directory();
bool update_version_directory();

bool archive_mgmt_persist_workspace();
bool remove_persist_mgmt_workspace();
bool remove_unique_mgmt_workspace();

bool prune_schema_directory();

std::set<std::string> read_northbound_schema_listing(
  std::string const & rift_root,
  std::vector<std::string> const & northbound_listings);

std::string get_rift_artifacts();
std::string get_rift_install();

std::string execute_command_and_get_result(const std::string& cmd);

/*
 * Gets the Shared object file provided the module name
 * by executing the pkg-config command.
 * Returns the absolute path of the shared object file.
 */
std::string get_so_file_for_module(const std::string& module_name);

/*
 * Validates the yang module by comparing the hash
 * of the module against the meta info file for the
 * module.
 */
bool validate_module_consistency(const std::string& module_name,
                                 const std::string& mangled_name,
                                 const std::string& upper_to_lower,
                                 std::string& error_str);

std::tuple<unsigned, unsigned> get_max_version_number_and_count();


}
#endif

#endif
