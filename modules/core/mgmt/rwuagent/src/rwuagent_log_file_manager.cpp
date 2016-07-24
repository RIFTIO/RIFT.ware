
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_log_file_manager.cpp
 *
 * Confd logging action manager.
 */

#include <string>
#include <sstream>

#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "rwyangutil.h"

#include "rwuagent.hpp"
#include "rwuagent_log_file_manager.hpp"

namespace {

std::string get_timestamp()
{
  const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();

  const boost::format f = boost::format("%02d-%02d-%s  %02d:%02d:%02d")
                          % now.date().year_month_day().day.as_number()
                          % now.date().year_month_day().month.as_number()
                          % now.date().year_month_day().year
                          % now.time_of_day().hours()
                          % now.time_of_day().minutes()
                          % now.time_of_day().seconds();

  return f.str();
}

}

using namespace rw_uagent;

LogFileManager::LogFileManager(Instance* instance):
    instance_(instance),
    memlog_buf_(instance_->get_memlog_inst(),
                "LogFileManager",
                reinterpret_cast<intptr_t>(this)),
    log_file_writers_()
{
  postrotate_script_ = rwyangutil::get_rift_install() + "/usr/local/confd/bin/confd_cmd -c reopen_logs";
  config_file_ = "/etc/logrotate.d/rwuagent";
  std::string const log_dir = rwyangutil::get_rift_artifacts();
  transaction_log_file_ = log_dir + "/rwuagent_transaction.log";
  failure_log_file_ = log_dir + "/rwuagent_transaction_failures.log";

  log_file_writers_[transaction_log_file_] = instance->create_async_file_writer(transaction_log_file_);
  log_file_writers_[failure_log_file_] = instance->create_async_file_writer(failure_log_file_);

  logs_.emplace_back(transaction_log_file_);
  logs_.emplace_back(failure_log_file_);

  write_to_file(transaction_log_file_, "## system start ##\n");
  write_to_file(failure_log_file_, "## system start ##\n");
}

LogFileManager::~LogFileManager()
{
  rwdts_member_deregister(sub_regh_);
}

void LogFileManager::add_confd_log_files() {
  
  std::string const log_dir = rwyangutil::get_rift_install() + "/" + instance_->mgmt_handler()->mgmt_dir() +
                              "/usr/local/confd/var/confd/log/";

  logs_.emplace_back(log_dir + "audit.log");
  logs_.emplace_back(log_dir + "confd.log");
  logs_.emplace_back(log_dir + "devel.log");
  logs_.emplace_back(log_dir + "netconf.log");


}

  
rw_status_t LogFileManager::register_config()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "register logrotate config");
  RW_MA_INST_LOG (instance_, InstanceDebug, "Registering logrotate config");

  RWPB_M_PATHSPEC_DECL_INIT(RwMgmtagt_LogrotateConf, cfg_pathspec);
  auto keyspec = (rw_keyspec_path_t *)&cfg_pathspec;
  rw_keyspec_path_set_category (keyspec, nullptr, RW_SCHEMA_CATEGORY_CONFIG);

  // Create DTS registration as subscriber
  rwdts_member_event_cb_t sub_reg_cb = {};
  sub_reg_cb.ud = this;
  sub_reg_cb.cb.prepare = &LogFileManager::create_logrotate_cfg_cb;

  rwdts_api_t* apih = instance_->dts_api();
  RW_ASSERT (apih);

  sub_regh_ = rwdts_member_register(
      nullptr,
      apih,
      keyspec,
      &sub_reg_cb,
      RWPB_G_MSG_PBCMD(RwMgmtagt_LogrotateConf),
      RWDTS_FLAG_SUBSCRIBER,
      nullptr);

  if (!sub_regh_) {
    RW_MA_INST_LOG (instance_, InstanceError,
                    "Failed to register logrotate config as subscriber");
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}


rwdts_member_rsp_code_t LogFileManager::create_logrotate_cfg_cb(
    const rwdts_xact_info_t * xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t * keyspec,
    const ProtobufCMessage * msg,
    uint32_t credits,
    void * get_next_key)
{
  RW_ASSERT(xact_info);
  RW_ASSERT(xact_info->ud);
  auto self = static_cast<LogFileManager*>( xact_info->ud );
  
  return self->create_logrotate_config( msg );
}


rwdts_member_rsp_code_t LogFileManager::create_logrotate_config(const ProtobufCMessage* msg)
    
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "create logrotate config");

  if (!is_production()) {
    RW_MA_INST_LOG (instance_, InstanceDebug,
                    "Non production mode, not creating the logrotate config");
    return RWDTS_ACTION_OK;
  }

  RW_ASSERT (msg);
  auto logrotate_cfg = (LogFileManager::LogrotateConf *)msg;

  std::ofstream ofile(config_file_.c_str());
  chmod(config_file_.c_str(), 0644);

  if (!ofile) {
    std::string err_log("Failed: ");
    err_log += strerror(errno);
    RW_MA_INST_LOG (instance_, InstanceError, err_log.c_str());
    return RWDTS_ACTION_NOT_OK;
  }
  auto fmt_write = [&ofile](const char* item, const std::string indent = "") {
    ofile << item << "\n";
  };

  auto wr_common_attribs = [logrotate_cfg, &fmt_write, &ofile]() {
    fmt_write ("missingok", "\t");
    if (logrotate_cfg->compress) fmt_write ("compress");
    ofile << "\tsize " <<  logrotate_cfg->size << "M\n";
    fmt_write ("su root root", "\t");
    ofile << "\trotate " << logrotate_cfg->rotations << "\n";
  };

  // Log files to be rotated
  for (const auto& file: logs_) {
    fmt_write (file.c_str());
  }


  if (has_confd_logs_) {
    /// confd has a post-rotate script
    fmt_write ("{");
    {
      wr_common_attribs ();
      fmt_write ("sharedscripts");
      fmt_write ("postrotate");
      ofile << "\t\t" << postrotate_script_ << " > /dev/null\n";
      fmt_write ("endscript");
    }
    fmt_write ("}");

    // Special rule for netconf.trace file as confd does not
    // reopen file handle for trace log
    // Using copy truncate option instead
    auto nc_trace_file = get_rift_install() + "/" + instance_->mgmt_handler()->mgmt_dir() +
                         "/usr/local/confd/var/confd/log/netconf.trace";
    fmt_write (nc_trace_file.c_str());
    fmt_write ("{");
    {
      wr_common_attribs ();
      fmt_write ("copytruncate");
    }
  
    fmt_write ("}");
  }

  if (!ofile) {
    RW_MA_INST_LOG (instance_, InstanceError, 
                    "Failure while writing to configuration file");
    unlink (config_file_.c_str());
    return RWDTS_ACTION_NOT_OK;
  }

  ofile.close();
  return RWDTS_ACTION_OK;
}


StartStatus LogFileManager::show_logs(SbReqRpc* rpc,
                                      const RWPB_T_MSG(RwMgmtagt_input_ShowAgentLogs)* req)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "show logs");
  RW_ASSERT (req);
  RW_MA_INST_LOG(instance_, InstanceInfo, "Reading management system logs.");

  if (req->has_string) {
    return output_to_string(rpc);
  } else {
    return output_to_file(rpc, req->file);
  }
}

std::string LogFileManager::get_log_records(const std::string& file)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "get log records");
  RW_MA_INST_LOG (instance_, InstanceDebug, "Get log records from log file");

  std::string records;
  std::ifstream in(file, std::ios::in | std::ios::binary);
  if (in) {
    std::ostringstream contents;
    contents << in.rdbuf();
    records = std::move(contents.str());
    in.close();
  } else {
    std::string log("Log file not found: ");
    log += file;
    RW_MA_INST_LOG (instance_, InstanceError, log.c_str());
  }
  return records;
}

StartStatus LogFileManager::output_to_string(SbReqRpc* rpc)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "output to string");
  RW_MA_INST_LOG (instance_, InstanceDebug, "Output logs records to string");

  RWPB_M_MSG_DECL_INIT(RwMgmtagt_AgentLogsOutput, output);
  output.n_result = logs_.size();
  output.result = (RWPB_T_MSG(RwMgmtagt_AgentLogsOutput_Result) **)
                  RW_MALLOC (sizeof (RWPB_T_MSG(RwMgmtagt_AgentLogsOutput_Result)*) * output.n_result);

  std::vector<RWPB_T_MSG(RwMgmtagt_AgentLogsOutput_Result)>
      results (output.n_result);
  std::vector<std::string> records(output.n_result);
  size_t idx = 0;

  for (auto& file: logs_) {
    RWPB_M_MSG_DECL_INIT(RwMgmtagt_AgentLogsOutput_Result, res);
    records[idx] = get_log_records(file);
    res.log_records = &records[idx][0];
    res.log_name = &logs_[idx][0];
    results[idx] = res;

    output.result[idx] = &results[idx];
    idx++;
  }

  // Send the response
  rpc->internal_done(
      &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_AgentLogsOutput)->rw_keyspec_path_t, &output.base);

  RW_FREE (output.result);

  return StartStatus::Done;
}

StartStatus LogFileManager::output_to_file(SbReqRpc* rpc,
                                           const std::string& file_name)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "output to file");
  RW_MA_INST_LOG (instance_, InstanceDebug, "Output logs records to file");

  std::string ofile = get_rift_install() + "/" + file_name;
  std::ofstream out(ofile);

  for (auto& file: logs_) {
    out << "------" << file << "-----------\n";
    out << get_log_records(file);
    out << "-------------------------------\n";
  }
  out.close();

  if (!out) {
    // error
    NetconfErrorList nc_errors;
    NetconfError& err = nc_errors.add_error();
    std::string log("Error while writing to file: ");
    log += ofile;
    err.set_error_message(log.c_str());
    rpc->internal_done(&nc_errors);
  } else {
    RWPB_M_MSG_DECL_INIT(RwMgmtagt_AgentLogsOutput, output);
    output.n_result = 1;
    output.result = (RWPB_T_MSG(RwMgmtagt_AgentLogsOutput_Result) **)
                    RW_MALLOC (sizeof (RWPB_T_MSG(RwMgmtagt_AgentLogsOutput_Result)*) * output.n_result);

    RWPB_M_MSG_DECL_INIT(RwMgmtagt_AgentLogsOutput_Result, res);
    output.result[0] = &res;

    res.log_name = (char*)"All files";
    std::string rec("Log records saved to file: ");
    rec += ofile;
    res.log_records = &rec[0];

    rpc->internal_done(
        &RWPB_G_PATHSPEC_VALUE(RwMgmtagt_AgentLogsOutput)->rw_keyspec_path_t, &output.base);
    RW_FREE (output.result);
  }

  return StartStatus::Done;
}

void LogFileManager::log_string(void* req, const std::string & tag, const std::string & str)
{
  log_string(req, tag.c_str(), str.c_str());
}

void LogFileManager::log_string(void* req, const char * tag, const char * str)
{
  write_to_file(transaction_log_file_, req, tag, str);
}

void LogFileManager::log_failure(void* req, const std::string & tag, const std::string & str)
{
  log_failure(req, tag.c_str(), str.c_str());
}

void LogFileManager::log_failure(void* req, const char * tag, const char * str)
{
  write_to_file(failure_log_file_, req, tag, str);
}

void LogFileManager::write_to_file(std::string const & filename, void* req, const char * tag, const char * str)
{
  std::stringstream logmessage;

  logmessage << get_timestamp() << LOG_SEPARATOR;
  logmessage << req << LOG_SEPARATOR;
  logmessage << tag << LOG_SEPARATOR;
  logmessage << str << std::endl;

  std::string const message = logmessage.str();
  
  write_to_file(filename, message);

}

void LogFileManager::write_to_file(std::string const & filename, std::string const & payload)
{
  write_to_file(filename, payload.c_str());
}

void LogFileManager::write_to_file(std::string const & filename, const char * payload)
{
  std::string const message(payload);

  log_file_writers_[filename]->write_message(message);  

}
