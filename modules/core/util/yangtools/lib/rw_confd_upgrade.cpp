
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
 * @file   rw_confd_upgrade.cpp
 * @author Arun Muralidharan
 * @date   21/08/2015
 * @brief  RW Confd in-service upgrade manager
*/

#include "rw_confd_upgrade.hpp"
#include "rwlib.h"

#include <confd_lib.h>
#include <confd_cdb.h>
#include <confd_dp.h>
#include <confd_maapi.h>

#include <iomanip>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <errno.h>

#include <rw_schema_defs.h>

using namespace rw_yang;
namespace fs = boost::filesystem;

// This is required only because we are copying ietf*yang files to
// usr/data/yang but not creating its *.fxs files there
static const char* ADD_CONFD_LOAD_PATH = "usr/local/confd/etc/confd";

static const int RW_MAAPI_TIMEO_SECS = 1; // seconds

bool
FSHelper::create_directory(const std::string& dir_name)
{
  try {
    if (!fs::create_directory(dir_name)) {
      return false;
    }
  } catch (const fs::filesystem_error& ex) {
    return false;
  }

  return true;
}

bool
FSHelper::create_directories(const std::string& path)
{
  try {
    if (!fs::create_directories(path)) {
      return false;
    }
  } catch (const fs::filesystem_error& ex) {
    return false;
  }

  return true;
}

bool
FSHelper::rename_directory(const std::string& orig_name,
                           const std::string& new_name)
{
  try {
    fs::rename(orig_name, new_name);

  } catch (const fs::filesystem_error& ex) {
    return false;
  }

  return true;
}

bool
FSHelper::remove_directory(const std::string& dir_name)
{
  try {
    fs::remove_all(dir_name);

  } catch (const fs::filesystem_error& ex) {
    return false;
  }
  return true;
}

bool
FSHelper::create_sym_link(const std::string& sym_link_name,
                          const std::string& dir_name)
{
  try {

    fs::create_symlink(dir_name, sym_link_name);

  } catch (const fs::filesystem_error& ex) {
    return false;
  }
  return true;
}

bool
FSHelper::remove_sym_link(const std::string& sym_link_name)
{
  try {
    if (!fs::remove(sym_link_name)) {
    }
  } catch (const fs::filesystem_error& ex) {
    return false;
  }

  return true;
}

bool
FSHelper::recreate_sym_link(const std::string& sym_link_name,
                            const std::string& dir_name)
{
  if (!remove_sym_link(sym_link_name)) {
    return false;
  }
  if (!create_sym_link(sym_link_name, dir_name)) {
    return false;
  }

  return true;
}

std::string
FSHelper::get_sym_link_name(const std::string& sym_link)
{

  if (!fs::is_symlink(sym_link)) {
    return std::string();
  }

  try {
    char buf[1024];
    ssize_t len;
    if ((len = readlink(sym_link.c_str(), buf, sizeof(buf) - 1)) != -1) {
      buf[len] = '\0';
      return std::string(buf);
    }
    return std::string();
    //return std::move(fs::read_symlink(sym_link).string());

  } catch (const fs::filesystem_error& exp) {
    return std::string();
  }

  return std::string();
}

bool
FSHelper::create_hardlinks(const std::string& source_dir,
                           const std::string& target_dir)
{
  if (!(fs::exists(source_dir) && fs::exists(target_dir))) {
    return false;
  }

  try {
    for (fs::directory_iterator file(source_dir); 
            file != fs::directory_iterator(); ++file) 
    {
      create_hard_link(file->path(), target_dir + "/" + file->path().filename().string());
    }
  } catch (const fs::filesystem_error& exp) {
    return false;
  }

  return true;
}

bool
FSHelper::create_softlinks(const std::string& source_dir,
                           const std::string& target_dir)
{
  if (!(fs::exists(source_dir) && fs::exists(target_dir))) {
    return false;
  }

  try {
    for (fs::directory_iterator file(source_dir);
            file != fs::directory_iterator(); ++file)
    {
      create_symlink(file->path(), target_dir + "/" + file->path().filename().string());
    }
  } catch (const fs::filesystem_error& exp) {
    return false;
  }

  return true;
}

bool
FSHelper::compare_link_name_to_directory(const std::string& link_name,
                                         const std::string& dir_name)
{
  // pre-process both linked_name and oss.str()
  std::vector<std::string> tokens_1;
  std::vector<std::string> tokens_2;

  boost::split(tokens_1, link_name, boost::is_any_of("/"));
  boost::split(tokens_2, dir_name, boost::is_any_of("/"));

  size_t idx_1 = tokens_1.size() - 1, idx_2 = tokens_2.size() - 1;

  if (tokens_1[idx_1].size() == 0) idx_1--;
  if (tokens_2[idx_2].size() == 0) idx_2--;

  if (!tokens_1[idx_1].compare(std::move(tokens_2[idx_2]))) {
    return true;
  }  

  return false;
}

bool
FSHelper::copy_all_files(const std::string& source_dir,
                         const std::string& dest_dir)
{
  if (!(fs::exists(source_dir) && fs::exists(dest_dir))) {
    return false;
  }
 
  try { 
    for (fs::directory_iterator file(source_dir);
           file != fs::directory_iterator(); ++file)
    {
      fs::copy_file(file->path().string(), dest_dir + "/" + file->path().filename().string());
    }
  } catch (const fs::filesystem_error& exp) {
    return false;
  }

  return true;
}

bool
FSHelper::remove_all_files(const std::string& dir)
{
  if (!fs::exists(dir)) {
    return false;
  }

  for (fs::directory_iterator file(dir);
            file != fs::directory_iterator(); ++file)
  {
    fs::remove_all(file->path());
  }

  return true;
}


ConfdUpgradeMgr::ConfdUpgradeMgr(uint32_t version, const struct sockaddr *addr,
                                 size_t addr_size): version_(version)
                                                  , confd_addr_size_(addr_size)
{
  const char* rift_install = getenv("RIFT_INSTALL");
  const char* rift_var_root = getenv("RIFT_VAR_ROOT");
  RW_ASSERT ( rift_install and rift_var_root );

  rift_install_ = std::string(rift_install);
  rift_var_root_ = std::string(rift_var_root);

  yang_root_ = rift_var_root_ + "/" + RW_SCHEMA_ROOT_PATH;
  yang_sl_   = rift_var_root_ + "/" + RW_SCHEMA_VER_LATEST_NB_PATH;
  ver_dir_   = rift_var_root_ + "/" + RW_SCHEMA_VER_PATH;
  latest_    = rift_var_root_ + "/" + RW_SCHEMA_VER_LATEST_PATH;

  if (addr) {
    memcpy(&confd_addr_in_, addr, confd_addr_size_);
    confd_addr_ = (struct sockaddr *)&confd_addr_in_;
  }
}

ConfdUpgradeMgr::~ConfdUpgradeMgr()
{
  if (maapi_sock_ < 0) {
    close(maapi_sock_);
  }
}

uint32_t
ConfdUpgradeMgr::get_max_version_linked()
{
  auto linked_name = FSHelper::get_sym_link_name(latest_);
  if (linked_name.length() == 0) {
    return 0;
  }

  const auto& ver_str = fs::basename(linked_name);
  if (!std::all_of(ver_str.begin(), ver_str.end(),
                 [](char c) {return std::isdigit(c);})) {
    return 0;
  }

  std::stringstream strm("0x" + ver_str);
  uint32_t num = 0;

  strm >> std::hex >> num;
  return num;
}

uint32_t
ConfdUpgradeMgr::get_max_version_unlinked()
{
 
  uint32_t max = 0; 
  for (fs::directory_iterator file(ver_dir_);
            file != fs::directory_iterator(); ++file)
  {
    const auto& dir_name = file->path().string();
    if (fs::is_symlink(dir_name)) {
      continue;
    }

    uint32_t num = 0;
    const auto& ver_str = fs::basename(dir_name);
    if (!std::all_of(ver_str.begin(), ver_str.end(), 
                [](char c) {return std::isdigit(c);})) {
      return 0;
    }

    std::stringstream strm("0x" + ver_str);
    strm >> std::hex >> num;
    if (num > max) {
      max = num;
    }
  }

  return max;
}


void
ConfdUpgradeMgr::set_log_cb(rw_confd_upgrade_log_cb cb, void *user_data)
{
  log_cb_ = cb;
  log_cb_user_data_ = user_data;
}

bool
ConfdUpgradeMgr::start_upgrade()
{
  std::ostringstream msg;

  if (!start_confd_upgrade()) {
    msg << "Error: Confd in-service upgrade failed" << "\n";
    RW_CONFD_MGR_ERROR_STRING(this, "Error: Confd in-service upgrade failed");
    send_progress_msg(msg);
  }

  return true;
}

bool
ConfdUpgradeMgr::start_confd_upgrade()
{
  std::ostringstream msg;
  msg << "\n>>> System upgrade is starting.\n";
  msg << ">>> Sessions in configure mode will be forced into operational mode.\n";
  msg << ">>> No configuration changes can be performed until ";
  msg << "upgrade has completed.\n";

  RW_CONFD_MGR_INFO(this, msg);
  send_progress_msg(msg);

  if (!maapi_sock_connect()) {
    RW_CONFD_MGR_ERROR_STRING(this, "Connection to Confd failed. Upgrade failed.");
    return false; 
  }

  if (maapi_load_schemas(maapi_sock_) != CONFD_OK ||
      set_user_session() != CONFD_OK)
  {
    msg << "\nError: mapi_load_schemas failed: " << 
      confd_lasterr();
    RW_CONFD_MGR_ERROR(this, msg);
    return false;
  }

  if (maapi_init_upgrade(maapi_sock_, RW_MAAPI_TIMEO_SECS,
              MAAPI_UPGRADE_KILL_ON_TIMEOUT) != CONFD_OK)
  {
    msg << "\nError: mapi_init_upgrade failed: " <<
      confd_lasterr();
    RW_CONFD_MGR_ERROR(this, msg);
    return false;
  }

  // ATTN: why do we need to change the confd load path at runtime?
  const auto& base_path = yang_sl_;

  const auto& path_1 = base_path + "/fxs";
  const auto& path_2 = base_path + "/xml";
  const auto& path_3 = base_path + "/lib";
  const auto& path_4 = base_path + "/yang";
  const auto& path_5 = rift_install_ + "/" + ADD_CONFD_LOAD_PATH;

  const char* load_paths[] = {
    path_1.c_str(),
    path_2.c_str(),
    path_3.c_str(),
    path_4.c_str(),
    path_5.c_str(),
  };

  if (maapi_perform_upgrade(maapi_sock_, 
        &load_paths[0], sizeof(load_paths)/sizeof(const char*)) != CONFD_OK)
  {
    msg << "\nError: mapi_perform_upgrade failed: " <<
      confd_lasterr();
    RW_CONFD_MGR_ERROR(this, msg);
    return false;
  }

  ready_for_commit_ = true;

  return true;
}

bool
ConfdUpgradeMgr::commit()
{
  std::ostringstream msg;

  if (!ready_for_commit_) {
    msg << "\nError: commit called incorrectly.";
    RW_CONFD_MGR_ERROR(this, msg);
    return false;
  }

  if (maapi_commit_upgrade(maapi_sock_) != CONFD_OK) {
    msg << "\nError: mapi_commit_upgrade failed: " <<
      confd_lasterr();
    RW_CONFD_MGR_ERROR(this, msg);
    return false;
  }

  ready_for_commit_ = false;
  msg << "\nConfd upgrade completed successfully";
  send_progress_msg(msg);
  return true;
}

int
ConfdUpgradeMgr::set_user_session()
{
  const char *user = "admin";
  const char *groups[] = {"admin"};
  const char *context = "system";
  struct confd_ip ip;
  const char* usid_env = nullptr;

  /* must use usid from CLI to be allowed to run across upgrade */
  if ((usid_env = getenv("CONFD_MAAPI_USID")) != NULL) {
    usid_env_ = usid_env;
    return maapi_set_user_session(maapi_sock_, std::stoi(usid_env_.c_str()));
  } else {
    ip.af = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &ip.ip.v4);
    return maapi_start_user_session(maapi_sock_, user, context, groups, 1,
            &ip, CONFD_PROTO_TCP);
  }

  return -1;
}

bool
ConfdUpgradeMgr::maapi_sock_connect()
{
  maapi_sock_ = socket(confd_addr_->sa_family, SOCK_STREAM | SOCK_CLOEXEC, 0);

  if (maapi_sock_ < 0) {
    char buffer[1024];
    if (strerror_r(errno, buffer, sizeof(buffer) == -1 && errno == ERANGE)) {
      return false;
    }
    std::string log_str = "Error: MAAPI socket could not be created: " + std::string(buffer);
    RW_CONFD_MGR_ERROR_STRING(this, log_str.c_str());
    return false;
  }

  if (maapi_connect(maapi_sock_, confd_addr_, 
        confd_addr_size_) != CONFD_OK) {
    std::string log_str = "Error: mapi_connect failed: " + std::string(confd_lasterr());
    RW_CONFD_MGR_ERROR_STRING(this, log_str.c_str());
    return false;
  }

  return true;
}

void
ConfdUpgradeMgr::send_progress_msg(std::ostringstream& msg)
{
  maapi_prio_message(maapi_sock_, "all", msg.str().c_str());
  msg.str(std::string());
}
