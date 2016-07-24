
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwyangutil_directory_helpers.cc
 * @brief Routines for interacting with the schema directory
 */

#include <algorithm>
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
#include <tuple>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/scope_exit.hpp>

#include "rwyangutil.h"
#include "rwyangutil_helpers.hpp"

namespace fs = boost::filesystem;

namespace rwyangutil {

/*
 * ATTN: Turn this into a proper generic object, generally available,
 * with accessors and string caching, with support for all
 * schema-related install and var/rift queries.
 */
struct SchemaPaths
{
  SchemaPaths();

  std::string rift_install;
  std::string install_yang_dir;
  std::string install_manifest_dir;
  std::string schema_root_dir;
  std::string lock_dir;
  std::string lock_file;
  std::string tmp_dir;
  std::string version_dir;
  std::string version_latest_link;
  std::string schema_all_tmp; // ATTN: Bogus, delete me!
  std::string schema_northbound_tmp; // ATTN: Bogus, delete me!

  static const std::vector<std::string> tmp_directories;
  static const std::vector<std::string> top_level_directories;
  static const std::vector<std::string> version_directories;
  static const std::map<std::string, const char*> fext_map;
};

SchemaPaths::SchemaPaths()
: rift_install(get_rift_install()),
  install_yang_dir( rift_install + "/" + RW_INSTALL_YANG_PATH ),
  install_manifest_dir( rift_install + "/" + RW_INSTALL_MANIFEST_PATH ),
  schema_root_dir( rift_install + "/" + RW_SCHEMA_ROOT_PATH ),
  lock_dir( rift_install + "/" + RW_SCHEMA_LOCK_PATH ),
  lock_file( rift_install + "/" + RW_SCHEMA_LOCK_PATH + "/" + RW_SCHEMA_LOCK_FILENAME ),
  tmp_dir( rift_install + "/" + RW_SCHEMA_TMP_PATH ),
  version_dir( rift_install + "/" + RW_SCHEMA_VER_PATH ),
  version_latest_link( rift_install + "/" + RW_SCHEMA_VER_LATEST_PATH ),
  schema_all_tmp( rift_install + "/" + RW_SCHEMA_TMP_ALL_PATH ),
  schema_northbound_tmp( rift_install + "/" + RW_SCHEMA_TMP_NB_PATH )
{
}

const std::vector<std::string> SchemaPaths::tmp_directories{
// ATTN: ifdef needs to be runtime decision based on agent mode.
#ifdef CONFD_ENABLED
  "fxs",
#endif
  "lib",
  "xml",
  "yang",
};

const std::vector<std::string> SchemaPaths::top_level_directories{
// ATTN: ifdef needs to be runtime decision based on agent mode.
  "cli",
#ifdef CONFD_ENABLED
  "fxs",
#endif
  "lib",
  "lock",
  "meta",
  "tmp",
  "version",
  "xml",
  "yang",
};

const std::vector<std::string> SchemaPaths::version_directories{
// ATTN: ifdef needs to be runtime decision based on agent mode.
  "cli",
#ifdef CONFD_ENABLED
  "fxs",
#endif
  "lib",
  "meta",
  "xml",
  "yang",
};

const std::map<std::string, const char*> SchemaPaths::fext_map{
  { ".cli.ssi.sh", "cli" },
  { ".cli.xml", "cli" },
  { ".dsdl", "xml" },
#ifdef CONFD_ENABLED
  { ".fxs", "fxs" },
#endif
  { ".txt", "meta" },
  { ".yang", "yang" },
};

std::tuple<unsigned, unsigned> get_max_version_number_and_count()
{
  unsigned max   = 0;
  unsigned count = 0;
  SchemaPaths const paths = SchemaPaths();

  for (fs::directory_iterator it(paths.version_dir); it != fs::directory_iterator(); ++it) {
    const std::string & name = it->path().string();
    if (!fs::is_directory(name)) {
      continue;
    }

    const std::string & version = fs::basename(name);
    if (!std::all_of(version.begin(), version.end(), [](char c) {return std::isxdigit(c);})) {
      continue;
    }

    count++;
    unsigned num = 0;
    std::stringstream strm("0x" + version);
    strm >> std::hex >> num;
    if (num > max) {
      max = num;
    }
  }

  return std::make_tuple(max, count);
}

bool cleanup_excess_version_dirs()
{
  unsigned vmax = 0;
  unsigned vcount = 0;

  std::tie (vmax, vcount) = get_max_version_number_and_count();
  if (vcount <= 8) {
    return true;
  }

  unsigned svnum  = vmax - vcount + 1;
  unsigned to_del = vcount - 8;
  SchemaPaths const paths = SchemaPaths();

  for (unsigned vern = svnum; vern < (svnum + to_del); ++vern) {

    std::ostringstream strm;
    strm << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << vern;
    std::string version_dir = paths.version_dir + "/" + strm.str();

    fs_remove_directory(version_dir);
  }

  return true;
}

bool cleanup_lock_file(std::string const & lock_file)
{
  if (!fs::exists(lock_file)) {
    return true;
  }

  return remove_lock_file();
}

bool cleanup_stale_version_dirs()
{
  unsigned vmax = 0;
  unsigned vcount = 0;

  std::tie (vmax, vcount) = get_max_version_number_and_count();
  SchemaPaths const paths = SchemaPaths();

  unsigned svnum  = vmax - vcount + 1;
  for (unsigned vern = svnum; vern < (svnum + vcount); ++vern) {

    std::ostringstream strm;
    strm << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << vern;

    std::string version_dir = paths.version_dir + "/" + strm.str();
    std::string stamp_file = version_dir + "/stamp";

    if (!fs::exists(stamp_file) && (vern != vmax)) {
      fs_remove_directory(version_dir);
    }
  }

  return true;
}

bool schema_directory_is_old(std::string const & schema_directory,
                             std::vector<std::string> const & schema_listings)
{
  SchemaPaths const paths = SchemaPaths();

  /*
   * ATTN: Huh?  The "oldness" of the schema directory is based on
   * time?  No, it should be based on the existence or not of the
   * files!
   */
  std::time_t const schema_directory_age = fs::last_write_time(schema_directory);

  for (unsigned i = 0; i < schema_listings.size(); ++i) {
    std::string const install_manifest_dir
      =    paths.install_manifest_dir
        + "/"
        + schema_listings[i];

    std::time_t const schema_listing_age = fs::last_write_time(install_manifest_dir);
    if (schema_directory_age < schema_listing_age) {
       return true;
    }
  }

  return false;
}

bool schema_listing_is_different_from_directory(std::string const & schema_directory,
                                                std::vector<std::string> const & schema_listings)
{
  SchemaPaths const paths;

  std::set<std::string> expected_nb_schema = read_northbound_schema_listing(paths.rift_install,
                                                                            schema_listings);

  fs::path const northbound_path = fs::path(schema_directory) / "version" / "latest" / "northbound" / "yang";

  // collect schema that are on disk
  std::set<std::string> actual_nb_schema;
  std::for_each(fs::directory_iterator(northbound_path),
                fs::directory_iterator(), // default constructor is end()
                [&actual_nb_schema](const fs::directory_entry& yangfile)
                {
                  std::string const module_name = yangfile.path().stem().string();
                  actual_nb_schema.insert(module_name);
                });

  // do set difference
  std::set<std::string> missing_schema;
  std::set_difference(expected_nb_schema.begin(),
                      expected_nb_schema.end(),
                      actual_nb_schema.begin(),
                      actual_nb_schema.end(),
                      std::inserter(missing_schema, missing_schema.begin()));

  return missing_schema.size() > 0;
}
bool create_lock_file()
{
  try {
    SchemaPaths const paths = SchemaPaths();

    // check liveness
    if (fs::exists(paths.lock_file)) {
      auto last_write_timer = fs::last_write_time(paths.lock_file);
      auto now = std::time(nullptr);

      if ((now - last_write_timer) >= LIVENESS_THRESH) {
        remove_lock_file();
      }
    }

    // if parent directory doesn't exist, create it
    if (!fs::exists(paths.lock_dir)) {
      if (!fs_create_directories(paths.lock_dir)) {
        return false;
      }
      fs::permissions(paths.lock_dir, fs::all_all);
    }

    const int fd = open(paths.lock_file.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0760);
    if (fd == -1) {
      return false;
    }

    return true;
  } catch (const fs::filesystem_error& e) {
    // swallow exceptions in this function
    return false;
  }
}

bool remove_lock_file()
{
  SchemaPaths const paths = SchemaPaths();
  bool const status = fs_remove(paths.lock_file);

  return status;
}

/*
 * ATTN: incomplete - was changing create_schema_directory to build in
 * place, rather than blow-away and rebuild
 */
std::vector<std::string> get_schema_subdir_set()
{
  SchemaPaths const paths = SchemaPaths();

  std::vector<std::string> dirs;
  for (auto path : paths.top_level_directories) {
    dirs.emplace_back( std::string(RW_SCHEMA_ROOT_PATH) + "/" + path );
  }

  return dirs;
}

/*
 * ATTN: incomplete - was changing create_schema_directory to build in
 * place, rather than blow-away and rebuild
 */
std::vector<std::string> get_latest_subdir_set()
{
  SchemaPaths const paths = SchemaPaths();

  std::vector<std::string> dirs;
  dirs.emplace_back( std::string(RW_SCHEMA_VER_LATEST_ALL_PATH) );
  dirs.emplace_back( std::string(RW_SCHEMA_VER_LATEST_NB_PATH) );

  for (auto path : paths.version_directories) {
    dirs.emplace_back( std::string(RW_SCHEMA_VER_LATEST_ALL_PATH) + "/" + path );
    dirs.emplace_back( std::string(RW_SCHEMA_VER_LATEST_NB_PATH) + "/" + path );
  }

  return dirs;
}

bool widen_northbound_schema(std::vector<std::string> const & schema_listing_files)
{
  SchemaPaths const paths = SchemaPaths();  
  std::set<std::string> nb_module_list
    = read_northbound_schema_listing(
        paths.rift_install,
        schema_listing_files);

  
#ifdef CONFD_ENABLED
  std::set<std::string> const northbound_extensions = {".yang", ".fxs"};
#else
  std::set<std::string> const northbound_extensions = {".yang"};
#endif
  for (fs::directory_iterator it(paths.install_yang_dir); it != fs::directory_iterator(); ++it) {

    if (!fs::is_regular_file(it->path()) &&
        !fs::is_symlink(it->path()))  {
      continue;
    }

    std::string ext;
    fs::path fn = it->path().filename();

    // ATTN: this isn't actually looping, is it?
    for (; !fn.extension().empty(); fn = fn.stem()) {
      ext = fn.extension().string() + ext;
    }

    if (northbound_extensions.count(ext) == 0) {
      // only need to widen the northbound extensions
      continue;
    }

    auto fd_map = paths.fext_map.find(ext);
    if (fd_map != paths.fext_map.end()) {

      std::string target_path = it->path().string();

      if (fs::is_symlink(it->path())) {

        const std::string tpath = fs_read_symlink(it->path().string());
        if (!tpath.length()) {
          std::cerr << "Failed to read the sym link " << it->path().string() << std::endl;
          return false;
        }

        target_path = fs::canonical(tpath, paths.install_yang_dir).string();
      }

      // make link into /northbound schema directory
      std::string const file_stem = it->path().filename().stem().string();
      if (nb_module_list.count(file_stem)) {
        std::string northbound_target_dir = paths.version_latest_link +  "/northbound/" + std::string(fd_map->second);
        std::string northbound_tsymb_link = northbound_target_dir + "/" + it->path().filename().string();

        if (fs::exists(northbound_tsymb_link)) {
          continue;
        }

        if (!fs_create_symlink(target_path, northbound_tsymb_link)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool create_schema_directory(std::vector<std::string> const & nb_listings)
{
  SchemaPaths const paths = SchemaPaths();

  std::vector<std::string> schema_listing_files;
  for (unsigned i = 0; i < nb_listings.size(); ++i) {
    schema_listing_files.push_back(nb_listings[i]);
  }

  bool lock_directory_exists = false;
  if (fs::exists(paths.schema_root_dir)) {
    /*
     * If there is only one subdirectory of the schema directory it is the lock
     * directory, and the rest of it needs to be created.
     */
    size_t subdirectory_count = 0;
    for (fs::directory_iterator it(paths.schema_root_dir);
         it != fs::directory_iterator();
         ++it) {
      if (fs::is_directory(it->path())) {
        subdirectory_count++;
      }
    }

    if (subdirectory_count > 1) {
      // directory structure has been created
      if (schema_directory_is_old(paths.schema_root_dir, schema_listing_files)
          || schema_listing_is_different_from_directory(paths.schema_root_dir, schema_listing_files)) {
        return widen_northbound_schema(schema_listing_files);
      } else {
        // do some maintenance on the existing structure
        cleanup_lock_file(paths.lock_file);
        return true;
      }
    } else {
      // only the lock directory has been created
      lock_directory_exists = true;
    }
  }

  /* Check to make sure the image schema path exists */
  if (!fs::exists(paths.install_yang_dir)) {
    std::cerr << "Image schema path does not exists " << paths.install_yang_dir << std::endl;
    return false;
  }

  std::set<std::string> nb_module_list
    = read_northbound_schema_listing(
        paths.rift_install,
        schema_listing_files);

  /* Create a random directory, create links to the image schema files, and rename it. */
  boost::uuids::uuid const uuid = boost::uuids::random_generator()();
  std::string suuid = boost::uuids::to_string(uuid);

  suuid.erase(std::remove_if(suuid.begin(), suuid.end(), [](char c)
                             { if (c == '-') { return true; } return false; }), suuid.end());

  std::ostringstream random_directory;
  random_directory << paths.rift_install << "/" << RW_SCHEMA_INIT_TMP_PREFIX << suuid;
  std::string random_spath = random_directory.str();
  BOOST_SCOPE_EXIT(random_spath) {
    boost::system::error_code ec;
    fs::remove_all(random_spath, ec);
  } BOOST_SCOPE_EXIT_END

  if (!fs_create_directories(random_spath)) {
    return false;
  }

  // ATTN:- Fix this.Setting permissions to 777 for now, so that make clean works.
  fs::permissions(random_spath, fs::all_all);

  // create top level directories
  for (auto path : paths.top_level_directories) {
    std::string const top_path = random_spath + "/" + path;

    if (!fs_create_directory(top_path)){
      return false;
    }
    fs::permissions(top_path, fs::all_all);

    if (lock_directory_exists && path == "lock") {
      // make lock file in temporary directory so it's still valid when the directory is copied
      std::string const new_lock_file = top_path + "/" + RW_SCHEMA_LOCK_FILENAME;
      auto fd = open(new_lock_file.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0760);
      if (fd == -1) {
        std::cerr << "Failed to create temporary lock file " << new_lock_file
                  << ": " << std::strerror(errno) << std::endl;
        return false;
      }
      continue;
    }
  }

  // create all and northbound tmp directories
  if (!create_tmp_directory_tree(random_spath)){
    return false;
  }

  // create all and northbound version directories
  std::ostringstream strm;
  strm << std::setfill('0') << std::setw(8) << 0;
  std::string version_path(random_spath + "/version/" + strm.str());

  for (auto path : paths.version_directories) {
    std::string const all_path = version_path + "/all/" + path;
    std::string const northbound_path = version_path + "/northbound/" + path;

    if (!fs_create_directories(all_path)){
      return false;
    }
    if (!fs_create_directories(northbound_path)) {
      return false;
    }

    fs::permissions(all_path, fs::all_all);
    fs::permissions(northbound_path, fs::all_all);
  }

  // Create symlinks for the image schema files from /usr/data/yang.
  for ( fs::directory_iterator it(paths.install_yang_dir); it != fs::directory_iterator(); ++it) {

    if (!fs::is_regular_file(it->path()) &&
        !fs::is_symlink(it->path()))  {
      continue;
    }

    std::string ext;
    fs::path fn = it->path().filename();

    // ATTN: this isn't actually looping, is it?
    for (; !fn.extension().empty(); fn = fn.stem()) {
      ext = fn.extension().string() + ext;
    }

    auto fd_map = paths.fext_map.find(ext);
    if (fd_map != paths.fext_map.end()) {

      std::string target_path = it->path().string();

      if (fs::is_symlink(it->path())) {

        const std::string tpath = fs_read_symlink(it->path().string());
        if (!tpath.length()) {
          std::cerr << "Failed to read the sym link " << it->path().string() << std::endl;
          return false;
        }

        target_path = fs::canonical(tpath, paths.install_yang_dir).string();
      }

      // make link into top-level directory schema directory
      std::string target_dir = random_spath + "/" + fd_map->second;
      std::string tsymb_link = target_dir + "/" + it->path().filename().string();

      if (!fs_create_symlink(target_path, tsymb_link)) {
        return false;
      }

      // make link into /all schema directory
      std::string all_target_dir = version_path + "/all/" + fd_map->second;
      std::string all_tsymb_link = all_target_dir + "/" + it->path().filename().string();

      if (!fs_create_symlink(target_path, all_tsymb_link)) {
        return false;
      }

      // make link into /northbound schema directory
      std::string const file_stem = it->path().filename().stem().string();
      if (nb_module_list.count(file_stem)) {
        std::string northbound_target_dir = version_path + "/northbound/" + fd_map->second;
        std::string northbound_tsymb_link = northbound_target_dir + "/" + it->path().filename().string();

        if (!fs_create_symlink(target_path, northbound_tsymb_link)) {
          return false;
        }
      }
    }
  }

  if (lock_directory_exists) {
    // only move the contents, excluding the /lock directory
    for (auto path : paths.top_level_directories) {
      if (path == "lock") {
        continue;
      }
      std::string const tmp_dir = random_spath + "/" + path;
      std::string const perm_path = paths.schema_root_dir + "/" + path;
      fs_rename(tmp_dir, perm_path);
    }
  } else {
    fs_rename(random_spath, paths.schema_root_dir);
  }

  // make latest directory
  std::string const initial_version_path = paths.rift_install + "/" + RW_SCHEMA_VER_PATH + "/00000000";
  if (!fs_create_symlink(initial_version_path, paths.version_latest_link, true/*force*/)) {
    return false;
  }

  // recursively chmod 777 the schema directory
  fs::directory_iterator dir_end;
  for(fs::recursive_directory_iterator dir(paths.schema_root_dir), dir_end; dir!=dir_end ;++dir) {
    if (fs::is_directory(dir->path())) {
      fs::permissions(dir->path(), fs::all_all);
    }
  }

  if (!fs::exists(paths.schema_root_dir) || !fs::is_directory(paths.schema_root_dir)) {
    std::cerr << "Failed to create schema directory " << paths.schema_root_dir << std::endl;
    return false;
  }

  return true;
}

/**
 * Create the /tmp directory structure
 * @param base_path the base path to the /tmp directory
 */
bool create_tmp_directory_tree(std::string const & base_path)
{
  SchemaPaths const paths = SchemaPaths();
  std::string const all_tmp_base = base_path + "/tmp/all";
  std::string const northbound_tmp_base = base_path + "/tmp/northbound";

  for (std::string const & path : paths.tmp_directories) {

    std::string const all_tmp = all_tmp_base + "/" + path;
    std::string const northbound_tmp = northbound_tmp_base + "/" + path;

    if (!fs_create_directories(all_tmp)){
      fs_remove_directory(all_tmp);
      return false;
    }
    if (!fs_create_directories(northbound_tmp)){
      fs_remove_directory(all_tmp);
      return false;
    }
  }

  return true;
}

bool remove_schema_directory()
{
  SchemaPaths const paths = SchemaPaths();
  if (!fs::exists(paths.schema_root_dir)) {
    return true;
  }

  return fs_remove_directory(paths.schema_root_dir);
}

bool update_version_directory()
{
  unsigned curr_ver = 0, count = 0;
  std::tie (curr_ver, count) = get_max_version_number_and_count();

  std::ostringstream new_strm;
  new_strm << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << (curr_ver + 1);

  std::ostringstream old_strm;
  old_strm << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << curr_ver;

  SchemaPaths const paths = SchemaPaths();
  std::string new_version_path(paths.version_dir +  "/" + new_strm.str());
  std::string old_version_path(paths.version_dir + "/" + old_strm.str());

  // if /tmp is a subset of the current version we have no work to do
  std::vector<std::string> tmp_all_filestems = get_filestems(paths.top_level_directories, paths.schema_all_tmp);
  std::vector<std::string> tmp_nb_filestems = get_filestems(paths.top_level_directories, paths.schema_northbound_tmp);

  std::string const current_all = paths.version_latest_link + "/all";
  std::string const current_nb = paths.version_latest_link + "/northbound";
  std::vector<std::string> current_all_filestems = get_filestems(paths.top_level_directories, current_all);
  std::vector<std::string> current_nb_filestems = get_filestems(paths.top_level_directories, current_nb);

  if ( std::includes(current_all_filestems.begin(), current_all_filestems.end(),
                     tmp_all_filestems.begin(), tmp_all_filestems.end())
       && std::includes(current_nb_filestems.begin(), current_nb_filestems.end(),
                        tmp_nb_filestems.begin(), tmp_nb_filestems.end()))
  {
    // Cleanup files from tmp directory
    fs_empty_the_directory(paths.tmp_dir);
    if (!create_tmp_directory_tree(paths.schema_root_dir)){
      std::cerr << "Creation of tmp directory structure failed\n";
      return false;
    }
    return true;
  }

  // make sure /tmp has all the right directories
  for (auto path : paths.tmp_directories) {
    std::string const all_path = paths.schema_all_tmp + "/" + path;
    std::string const northbound_path = paths.schema_northbound_tmp + "/" + path;

    if (!fs::exists(all_path)) {
      if (!fs_create_directories(all_path)){
        return false;
      }
    }
    fs::permissions(all_path, fs::all_all);

    if (!fs::exists(northbound_path)) {
      if (!fs_create_directories(northbound_path)){
        return false;
      }
    }
    fs::permissions(northbound_path, fs::all_all);

    std::string const old_all_path = old_version_path + "/all/" + path;
    std::string const old_northbound_path = old_version_path + "/northbound/" + path;

    // Create symlinks for files into /all subdirectory
    auto res1 = fs_create_hardlinks(old_all_path, all_path);
    if (!res1) {
      return false;
    }

    // Create symlinks for base schema files into /northbound subdirectory
    auto res2 = fs_create_hardlinks(old_northbound_path, northbound_path);
    if (!res2) {
      return false;
    }
  }

  // Move /tmp directory into new version directory
  if (!fs_rename(paths.tmp_dir, new_version_path)) {
    return false;
  }

  // Create all and northbound tmp directories
  if (!create_tmp_directory_tree(paths.schema_root_dir)){
    return false;
  }

  // Make symlink to latest
  if (!fs_create_symlink(new_version_path, paths.version_latest_link, true/*force*/)) {
    return false;
  }

  // Create a stamp file
  std::string const stamp_filename = new_version_path + "/stamp";
  const int fd = open(stamp_filename.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0770);
  if (fd == -1) {
    std::cerr << "Failed to create stamp file: " << std::strerror(errno) << std::endl;;
    return false;
  }
  close(fd);
  fs::permissions(stamp_filename, fs::all_all);

  (void)remove_lock_file();
  return true;
}

bool archive_mgmt_persist_workspace(const char* prefix)
{
  SchemaPaths const paths = SchemaPaths();

  for (fs::directory_iterator entry(paths.rift_install);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;

    auto dir_name = entry->path().filename().string();
    auto pos = dir_name.find(prefix);

    if (pos != 0) continue;
    // rename the persist directory
    auto mod_time = fs::last_write_time(entry->path());
    auto epoch = std::chrono::duration_cast<std::chrono::seconds> (
        std::chrono::system_clock::from_time_t(mod_time).time_since_epoch()).count();

    auto new_dir_name = paths.rift_install+ "/ar_" + dir_name + "_" + std::to_string(epoch);

    fs::rename(entry->path(), fs::path(new_dir_name));
    if (!fs::exists(new_dir_name)) {
      std::cerr << "Rename failed: " << new_dir_name << ": "
                << std::strerror(errno)<< std::endl;
      return false;
    }
  }

  return true;
}

bool remove_mgmt_workspace(const char* prefix)
{
  SchemaPaths const paths = SchemaPaths();

  for (fs::directory_iterator entry(paths.rift_install);
       entry != fs::directory_iterator(); ++entry)
  {
    if (!fs::is_directory(entry->path())) continue;

    const std::string dir_name = entry->path().filename().string();
    const size_t pos = dir_name.find(prefix);

    if (pos != 0) continue;

    if (!fs_remove_directory(entry->path().string())) {
      return false;
    }
  }
  return true;
}


bool remove_unique_mgmt_workspace()
{
  // Remove the non persisting directories and the archived one as well
  auto ret1 = remove_mgmt_workspace(RW_SCHEMA_MGMT_TEST_PREFIX);
  auto ret2 = remove_mgmt_workspace(RW_SCHEMA_MGMT_ARCHIVE_PREFIX);

  return ret1 && ret2;
}

bool remove_persist_mgmt_workspace()
{
  return remove_mgmt_workspace(RW_SCHEMA_MGMT_PERSIST_PREFIX);
}

bool archive_mgmt_persist_workspace()
{
  return archive_mgmt_persist_workspace(RW_SCHEMA_MGMT_PERSIST_PREFIX);
}

bool prune_schema_directory()
{
  cleanup_excess_version_dirs();
  cleanup_stale_version_dirs();
  return true;
}

}
