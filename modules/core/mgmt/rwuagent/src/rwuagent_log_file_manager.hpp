
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
 * @file rwuagent_confd.hpp
 *
 * Management agent log integration header.
 */

#ifndef CORE_MGMT_RWUAGENT_LOG_FILE_MANAGER_HPP_
#define CORE_MGMT_RWUAGENT_LOG_FILE_MANAGER_HPP_

#include <string>
#include <sstream>

#include "rwuagent.hpp"

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

namespace rw_uagent {

class Instance;

/**
 * Confd Log
 * LogFileManager class manages the actions on confd log files.
 * 1. Logrotate configuration for confd logs.
 * 2. Show logs rpc command
 */
class LogFileManager
{
public:
  using LogrotateConf = RWPB_T_MSG(RwMgmtagt_LogrotateConf);

  LogFileManager(Instance*);
  ~LogFileManager();

  // non-copyable and non-assignable
  LogFileManager(const LogFileManager&) = delete;
  void operator=(LogFileManager) = delete;

public:
  /*!
   * Register as a subscriber for the
   * logrotate config.
   */
  rw_status_t register_config();

  /*!
   * Called by show-agent-logs RPC
   */
  StartStatus show_logs(SbReqRpc* rpc,
                        const RWPB_T_MSG(RwMgmtagt_input_ShowAgentLogs)* req);

  void add_confd_log_files();

  /// construct the log string and write it to the logfile
  void log_string(void* req, const std::string & tag, const std::string & str);
  /// construct the log string and write it to the logfile
  void log_string(void* req, const char * tag, const char * str);

  /// construct the failure string and write it to the failure logfile
  void log_failure(void* req, const std::string & tag, const std::string & str);
  /// construct the failure string and write it to the failure logfile
  void log_failure(void* req, const char * tag, const char * str);
private:

  const std::string LOG_SEPARATOR = " | ";

  /// appends the payload to the given file with a timestamp and a newline
  void write_to_file(std::string const & filename, void* req, const char * tag, const char * str);

  /// appends the payload to the given file with a newline
  void write_to_file(std::string const & filename, std::string const & payload);

  /// appends the payload to the given file with a newline
  void write_to_file(std::string const & filename, const char * payload);

  /*!
   * Creates logrotate config file.
   * The file is created only in production mode.
   * In non-production mode, the callback does nothing.
   */
  rwdts_member_rsp_code_t create_logrotate_config(const ProtobufCMessage*);

  std::string get_log_records(const std::string& file);

  StartStatus output_to_string(SbReqRpc* rpc);

  StartStatus output_to_file(SbReqRpc* rpc,
                             const std::string& file_name);

  static rwdts_member_rsp_code_t create_logrotate_cfg_cb(
                        const rwdts_xact_info_t* xact_info,
                        RWDtsQueryAction action,
                        const rw_keyspec_path_t* keyspec,
                        const ProtobufCMessage* msg,
                        uint32_t credits,
                        void* get_next_key);
private: 
  // Non owning data members
  Instance* instance_ = nullptr;

  // Owning data members
  RwMemlogBuffer memlog_buf_;
  // Subscriber registration handle
  rwdts_member_reg_handle_t sub_regh_ = {};
  // script which needs to be run after file is rotated
  std::string postrotate_script_;
  // List of log files which needs to be rotated
  std::vector<std::string> logs_;

  /// the logrotate config file
  std::string config_file_;

  /// file used to record agent transactions
  std::string transaction_log_file_;

  /// file used to record agent transaction failures
  std::string failure_log_file_;

  bool has_confd_logs_ = false;

  // map filenames to log writers
  std::map<std::string, std::unique_ptr<AsyncFileWriter>> log_file_writers_;

};
}
#endif
