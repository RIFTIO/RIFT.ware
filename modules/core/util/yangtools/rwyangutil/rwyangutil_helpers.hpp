/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 * Creation Date: 1/22/16
 * 
 */

#ifndef __RWYANGUTIL_HELPERS_H__
#define __RWYANGUTIL_HELPERS_H__

#include <string>
#include <vector>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

namespace fs = boost::filesystem;

namespace rwyangutil {

/**
 * Create the /tmp directory structure
 * @param base_path the base path to the /tmp directory
 */
bool create_tmp_directory_tree(std::string const & base_path);

std::vector<std::string> get_filestems(std::vector<std::string> const & paths,
                                       std::string const & directory);

bool fs_create_directories(const std::string& path);

bool fs_create_directory(const std::string& path);

bool fs_create_hardlinks(const std::string& spath, const std::string& dpath);

bool fs_create_symlink(const std::string& target, const std::string& link, bool const force=false);

bool fs_empty_the_directory(const std::string& dpath);

std::string fs_read_symlink(const std::string& sym_link);

bool fs_remove(const std::string& path);

bool fs_remove_directory(const std::string& dpath);

bool fs_rename(const std::string& old_path, const std::string& new_path);

}

#endif
