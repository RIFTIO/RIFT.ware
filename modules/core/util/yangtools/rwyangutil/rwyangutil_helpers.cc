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

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include "rwyangutil.h"
#include "rwyangutil_helpers.hpp"

namespace fs = boost::filesystem;

namespace rwyangutil {

std::string get_rift_artifacts()
{
  auto rift_artifacts = getenv("RIFT_ARTIFACTS");
  if (!rift_artifacts) return "/home/rift/.artifacts";
  return rift_artifacts;
}

std::vector<std::string> get_filestems(std::vector<std::string> const & paths,
                                       std::string const & directory)
{
  std::vector<std::string> filestems;

  for (fs::recursive_directory_iterator it(directory);
       it != fs::recursive_directory_iterator();
       ++it) {
    std::string const filestem = it->path().stem().string();
    if (std::find(paths.begin(), paths.end(), filestem) != paths.end()) {
      continue;
    }

    filestems.push_back(filestem);
  }
  std::sort(filestems.begin(), filestems.end());
  return filestems;
}

bool fs_create_directory(const std::string& path)
{
  try {
    if (!fs::create_directory(path)) {
      return false;
    }
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while creating directory: "
              << path << " "
              << e.what() << std::endl;
    return false;
  }

  return true;
}

bool fs_create_directories(const std::string& path)
{
  // ATTN: Would prefer fs::create_directories, but it is buggy.
  // https://svn.boost.org/trac/boost/ticket/7258
  // Fixed in boost 60.0.
  fs::path fs_path(path);
  fs::path full_path;
  for (auto dir: fs_path) {
    boost::system::error_code ec;
    full_path /= dir;
    bool exists = fs::exists(full_path, ec);
    bool is_dir = fs::is_directory(full_path, ec);

    if (exists) {
      if (is_dir) {
        continue;
      }

      bool rv = fs_remove(full_path.string());
      if (!rv) {
        return rv;
      }
    }

    bool rv = fs_create_directory(full_path.string());
    if (!rv) {
      return rv;
    }
  }

  return true;
}

bool fs_create_hardlinks(const std::string& spath,
                         const std::string& dpath)
{
  for (fs::directory_iterator file(spath); file != fs::directory_iterator(); ++file) {
    try {
      fs::create_hard_link(file->path(), dpath + "/" + file->path().filename().string());
    } catch (const fs::filesystem_error& e) {
      std::cerr << "Exception while creating symbolic link to: "
                << file->path().string() << ": "
                << e.what() << std::endl;
      return false;
    }
  }
  return true;
}

bool fs_empty_the_directory(const std::string& dpath)
{
  for (fs::directory_iterator file(dpath); file != fs::directory_iterator(); ++file) {
    fs::remove_all(file->path());
  }
  return true;
}

bool fs_remove_directory(const std::string& dpath)
{
  try {
    fs::remove_all(dpath);
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while removing the dir: "
              << dpath << " "
              << e.what() << std::endl;
    return false;
  }
  return true;
}

std::string fs_read_symlink(const std::string& sym_link)
{
  std::string target;

  try {
    fs::path const tpath = fs::read_symlink(sym_link);
    if (!tpath.empty()) {
      target = tpath.string();
    }
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while reading the symlink: "
              << sym_link << " "
              << e.what() << std::endl;
    return target;
  }

  return target;
}

bool fs_create_symlink(const std::string& target,
                       const std::string& link,
                       bool const force)
{
  try {
    if (force) {
      /*
       * boost::filesystem::create_symlink can't do a forced operation
       * so to avoid the delete just make a temporary and rename it.
       */
      fs::path const temporary_link = link + ".tmp";
      fs::create_symlink(target, temporary_link);
      fs::rename(temporary_link, link);
    } else {
      fs::create_symlink(target, link);
    }
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while creating sym link to "
              << target << " as " << link << " "
              << e.what() << std::endl;
    return false;
  }

  return true;
}

bool fs_rename(const std::string& old_path,
               const std::string& new_path)
{
  try {
    fs::rename(old_path, new_path);
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while renaming "
              << old_path << " to " << new_path << " "
              << e.what() << std::endl;
    return false;
  }

  return true;
}

bool fs_remove(const std::string& path)
{
  try {
    fs::remove(path);
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Exception while removing "
              << path << " "
              << e.what() << std::endl;
    return false;
  }

  return true;
}

bool is_rooted_path()
{
  auto rift_prod_mode = getenv("RIFT_PRODUCTION_MODE");
  if (rift_prod_mode) return true;

  static std::array<const char*, 5> probable_vals {
     "/", "/home/rift", "/home/rift/.install", "/usr/rift/build/fc20_debug/install/usr/rift", "/usr/rift"
  };
  auto it = std::find(probable_vals.begin(), probable_vals.end(), get_rift_install());

  return it != probable_vals.end();
}

std::string get_rift_install()
{
  auto rift_install = getenv("RIFT_INSTALL");
  if (!rift_install) return "/home/rift/.install";
  return rift_install;
}

std::string get_rift_var_root()
{
  auto rift_var_root = getenv("RIFT_VAR_ROOT");
  if (!rift_var_root) {
    return get_rift_install();
  }
  return rift_var_root;
}

// ATTN: Should have proper error handling!
std::set<std::string> read_northbound_schema_listing(
    std::string const & rift_root,
    std::vector<std::string> const & northbound_listings)
{
  std::set<std::string> northbound_schema_listing;

  for (auto const& northbound_listing_filename: northbound_listings) {
    std::string northbound_listing_path
      = rift_root + "/usr/data/manifest/" + northbound_listing_filename;

    std::ifstream northbound_listing_file(northbound_listing_path);
    std::string line;
    while (std::getline( northbound_listing_file, line )) {
      northbound_schema_listing.insert(line);
    }
  }

  return northbound_schema_listing;
}

std::string execute_command_and_get_result(const std::string& cmd)
{
  std::string result;
  FILE* pipe_f = popen(cmd.c_str(), "r");
  if (!pipe_f) {
    return result;
  }
  std::shared_ptr<FILE> pipe_sp(pipe_f, pclose);

  size_t const buffer_size = 128;
  char buffer[buffer_size];

  while (!feof(pipe_f)) {
    if (fgets(buffer, buffer_size, pipe_f) != NULL)
      result += buffer;
  }

  return result;
}

std::string get_so_file_for_module(const std::string& module_name)
{
  auto prefix_directory = get_rift_install() + "/usr";
  const std::string& cmd = "pkg-config --define-variable=prefix=" + prefix_directory
                           + " --libs " + module_name;

  auto result = execute_command_and_get_result(cmd);
  auto lpos = result.find("-l");
  if (lpos == std::string::npos) return std::string();

  auto Lpos = result.find("-L");
  if (Lpos == std::string::npos) return std::string();

  auto split_pos = result.find(" ");
  if (split_pos == std::string::npos) return std::string();

  std::string so_file = "";
  if (lpos > Lpos){
      so_file = result.substr(2, split_pos - 2) + "/lib"
                    + result.substr(lpos + 2, result.length() - (lpos + 2) - 2) + ".so";
  } else {
      so_file = result.substr(Lpos + 2, result.length() - (Lpos + 2) - 3) + "/lib"
                    + result.substr(lpos + 2, split_pos - (lpos + 2)) + ".so";
  }

  return so_file;
}

bool validate_module_consistency(const std::string& module_name,
                                 const std::string& mangled_name,
                                 const std::string& upper_to_lower,
                                 std::string& error_str)
{
  //ATTN: Ignore ietf for now
  // fxs files are not found at correct path
  if (module_name.find("ietf") == 0) return true;

  std::string meta_name = module_name + RW_SCHEMA_META_FILE_SUFFIX;
  std::string rift_install = get_rift_install();

  std::string version_dir = rift_install + "/" + RW_SCHEMA_VER_LATEST_PATH;
  auto meta_info = rift_install + "/" + RW_INSTALL_YANG_PATH;

  std::ifstream inf(meta_info + "/" + meta_name);
  if (!inf) {
    error_str = meta_name + " file not found";
    return false;
  }
  auto get_sha_sum = [](const std::string& file){
    //ATTN: if the algorithm to compute hash is changed here,
    //please change it in rift_schema_info.cmake as well!
    std::string cmd = "sha1sum " + file + " | cut -d ' ' -f 1";
    return execute_command_and_get_result(cmd);
  };

  // Get shared-object name for the module
  std::string so_file = get_so_file_for_module(module_name);
  if (!so_file.length()) {
    error_str = meta_name + " so file not found: ";
    error_str += std::strerror(errno);
    return false;
  }

  std::map<std::string, std::vector<std::string>> prefix_path_map = {
    {module_name + ".yang",    {rift_install + "/" + RW_INSTALL_YANG_PATH}},
#ifdef CONFD_ENABLED
    {module_name + ".fxs",     {rift_install + "/" + RW_INSTALL_YANG_PATH}},
#endif
    {module_name + ".pc",      {rift_install + "/usr/lib/pkgconfig/"}},
    {mangled_name + "Yang-1.0.typelib",     {rift_install + "/usr/lib/rift/girepository-1.0/"}},
    {fs::path(so_file).filename().string(), {rift_install + "/usr/lib/"}},
  };

  std::string line;
  while (std::getline(inf, line)) {
    size_t pos = line.find(':');
    if (pos == std::string::npos) {
      error_str = meta_name + " ':' not found";
      return false;
    }

    auto file_name = line.substr(0, pos);

    // Compare the hashes from the file entry
    auto it = prefix_path_map.find(file_name);
    if (it == prefix_path_map.end()) {
      error_str += file_name + "Not found.";
      continue;
    }

    auto sha1 = line.substr(pos + 1, line.length() - pos);

    std::string file_path = prefix_path_map[file_name][0] + "/" + file_name;
    if (!fs::exists(file_path)) {
      continue;
    }

    auto sha_res = get_sha_sum(file_path);
    sha_res = sha_res.substr(0, sha_res.find('\n'));

    if (sha_res != sha1) {
      error_str = "Hashes do not match. ";
      error_str += "Mismatch: " + line + " : " + sha_res;
      return false;
    }

    prefix_path_map.erase(it);
  }

  if (prefix_path_map.size() != 0) {
    error_str = "Not all meta information present in ";
    error_str += meta_name;
    return false;
  }
  return true;
}

}
