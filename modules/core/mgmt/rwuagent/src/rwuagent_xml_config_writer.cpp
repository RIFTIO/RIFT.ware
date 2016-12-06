
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


/*
 * @file rwuagent_xml_config_writer.cpp
 *
 * XML config writer
 */

#include <fstream>
#include "rwuagent_xml_mgmt_system.hpp"

using namespace rw_uagent;
namespace fs = boost::filesystem;

const char* const XMLConfigWriter::IDENTIFIER_NAME  = "serial_number";
const char* const XMLConfigWriter::CFG_FILE_PREFIX  = "cfg";
const int XMLConfigWriter::NUM_CONFIG_FILES  = 3;


XMLConfigWriter::XMLConfigWriter(const XMLMgmtSystem* mgmt, Instance* instance)
  : mgmt_(mgmt)
  , instance_(instance)
  , memlog_buf_(
      instance_->get_memlog_inst(),
      "XMLConfigWriter",
      reinterpret_cast<intptr_t>(this) )
{
}

rw_status_t XMLConfigWriter::prepare_config(rw_yang::XMLDocument* dom)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "prepare config");
  RW_ASSERT (dom);
  rw_status_t res = RW_STATUS_SUCCESS;

  root_ = dom->get_root_node();
  if (!root_) {
    RW_MA_INST_LOG (instance_, InstanceError,
        "Root node not found in dom");
    return RW_STATUS_FAILURE;
  }

  res = persist_to_tmp_file();

  if (res != RW_STATUS_SUCCESS) {
    std::string log_str;
    log_str = "Prepare configuration failed to write configuration.";
    log_str += "Serial number of configuration: " + std::to_string(serial_num_);
    RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t XMLConfigWriter::commit_config()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle get");
  rw_status_t res = RW_STATUS_SUCCESS;

  res = rename_config();
  if (res != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG (instance_, InstanceError, "Failed to rename temp config file");
    return RW_STATUS_FAILURE;
  }

  res = update_symlink();
  if (res != RW_STATUS_SUCCESS) {
    RW_MA_INST_LOG (instance_, InstanceError, "Failed to update the latest symlink");
    return RW_STATUS_FAILURE;
  }

  serial_num_++;
  // Update the serial number in the root node
  set_root_node_serial_number();

  return res;
}

uint64_t XMLConfigWriter::get_serial_number(XMLNode* root)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "get serial number");
  uint64_t id = 0;

  if (!root->has_attribute(IDENTIFIER_NAME)) return 0;

  std::string log_str;
  std::string value = root->get_attribute_value(IDENTIFIER_NAME);
  std::stringstream ss(value);
  ss >> id;
  if (ss.fail()) {
    std::string log_str;
    log_str = "Failed to convert string to integer: " + ss.str();
    RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
    return 0;
  }

  RW_MA_INST_LOG (instance_, InstanceDebug, (log_str="Get config id= "+ value).c_str());
  return id;
}


void XMLConfigWriter::set_root_node_serial_number()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "set serial number",
      RWMEMLOG_ARG_PRINTF_INTPTR("id=%" PRIdPTR, (intptr_t)serial_num_));

  std::string log_str;
  RW_MA_INST_LOG (instance_, InstanceDebug, 
      (log_str="Set config id= "+std::to_string(serial_num_)).c_str());

  instance_->dom()->get_root_node()->set_attribute(IDENTIFIER_NAME, std::to_string(serial_num_).c_str());
}


rw_status_t XMLConfigWriter::persist_to_tmp_file()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "persist to tmp file");
  std::string log_str;

  remove_temp_file();

  auto xml_ws = mgmt_->mgmt_dir();
  int cfg_id = serial_num_ % NUM_CONFIG_FILES;

  cfg_file_ = xml_ws + "/" + CFG_FILE_PREFIX + std::to_string(cfg_id) + ".xml.tmp";
  RW_MA_INST_LOG (instance_, InstanceDebug, (log_str="Saving config to "+cfg_file_).c_str());

  rw_status_t res = root_->to_file(cfg_file_.c_str());
  if (res != RW_STATUS_SUCCESS) {
    log_str = "Writing configuration to file " + cfg_file_ + " failed." + std::strerror(errno);
    RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG (instance_, InstanceDebug, "Config written to temp file");

  return RW_STATUS_SUCCESS;
}

rw_status_t XMLConfigWriter::rename_config()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "rename config");
  std::string commit_file = mgmt_->mgmt_dir() + "/" + fs::path(cfg_file_).stem().string();

  try {
    fs::rename(cfg_file_, commit_file);
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return RW_STATUS_FAILURE;
  }

  cfg_file_ = commit_file;

  return RW_STATUS_SUCCESS;
}

rw_status_t XMLConfigWriter::update_symlink()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "update symlink");

  std::string log_str;
  std::string symlink_name = mgmt_->mgmt_dir() + "/latest";
  fs::path old_path;

  try {
    old_path = fs::current_path();
    fs::current_path(fs::path(cfg_file_).parent_path());
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return RW_STATUS_FAILURE;
  }

  try {
    fs::remove(symlink_name);
    fs::create_symlink(fs::path(cfg_file_).filename(), symlink_name);
    fs::current_path(old_path);
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return RW_STATUS_FAILURE;
  }

  RW_MA_INST_LOG (instance_, InstanceDebug, (log_str="Updated symlink to "+cfg_file_).c_str());
  return RW_STATUS_SUCCESS;
}

void XMLConfigWriter::remove_temp_file()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "remove temp file");
  auto& xml_dir = mgmt_->mgmt_dir();

  try {
    for (fs::directory_iterator it(xml_dir);
         it != fs::directory_iterator();
         ++it)
    {
      if (it->path().extension() == ".tmp") {
        std::string log_str;
        log_str = "Removed temp file " + it->path().string();
        RW_MA_INST_LOG (instance_, InstanceNotice, log_str.c_str());
        fs::remove(it->path());
      }
    }
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
  }
}

