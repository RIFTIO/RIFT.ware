
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*
* @file rwuagent_confd_workspace.cpp
*
* Management Confd workspace handler
*/

#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include "rw_schema_defs.h"
#include "rwuagent_mgmt_system.hpp"
#include "rw-manifest.pb-c.h"

#define CONFD_BASE_PATH ("/usr/local/confd")
#define CONFD_LOG_PATH ("/var/confd/log/")

using namespace rw_uagent;
namespace fs = boost::filesystem;

ConfdWorkspaceMgr::ConfdWorkspaceMgr(const ConfdMgmtSystem* mgmt, 
                                     Instance* inst): 
    mgmt_(mgmt),
    instance_(inst),
    memlog_buf_(
      inst->get_memlog_inst(),
      "ConfdWorkspaceMgr",
      reinterpret_cast<intptr_t>(this))
{
}

rw_status_t ConfdWorkspaceMgr::generate_config_file()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "generate config file");
  auto prototype_conf_file = get_rift_install() + "/" + RW_SCHEMA_CONFD_PROTOTYPE_CONF;

  auto confd_dir = mgmt_->mgmt_dir();

  confd_conf_dom_ = std::move(instance_->xml_mgr()->create_document_from_file(
                              prototype_conf_file.c_str(), false));

  if (!confd_conf_dom_) {
    RW_MA_INST_LOG (instance_, InstanceError, 
        "Failed to convert prototype conf file to DOM");
    return RW_STATUS_FAILURE;
  }

  if (!set_schema_load_path()) {
    RW_MA_INST_LOG (instance_, InstanceError, "Failed to set load path");
    return RW_STATUS_FAILURE;
  }

  set_directory_path("", "stateDir", confd_dir + "/var/confd/state");
  set_directory_path("cdb", "dbDir", confd_dir + "/var/confd/cdb");
  set_directory_path("rollback", "directory", confd_dir + "/var/confd/rollback");
  set_directory_path(
      "netconf/capabilities/url/file", "rootDir", confd_dir + "/var/confd/state");
  set_directory_path(
      "aaa", "sshServerKeyDir", get_rift_install() + "/usr/local/confd/etc/confd/ssh");

  auto confd_log_path = confd_dir + CONFD_LOG_PATH;
  set_directory_path("logs/webuiAccessLog", "dir", confd_log_path);

  set_feature("cli", true);
  set_feature("rest", true);
  set_feature("snmpAgent", false);

  auto vcs_inst = instance_->rwvcs();
  RwManifest__YangEnum__NetconfTrace__E trace_e = RW_MANIFEST_NETCONF_TRACE_AUTO;
  if (vcs_inst
      && vcs_inst->pb_rwmanifest
      && vcs_inst->pb_rwmanifest->init_phase
      && vcs_inst->pb_rwmanifest->init_phase->settings
      && vcs_inst->pb_rwmanifest->init_phase->settings->mgmt_agent
      && vcs_inst->pb_rwmanifest->init_phase->settings->mgmt_agent->has_netconf_trace)
  {
    trace_e = vcs_inst->pb_rwmanifest->init_phase->settings->mgmt_agent->netconf_trace;
  }

  switch (trace_e) {
    case RW_MANIFEST_NETCONF_TRACE_ENABLE:
      set_feature("logs/netconfTraceLog", true);
      break;
    case RW_MANIFEST_NETCONF_TRACE_DISABLE:
      set_feature("logs/netconfTraceLog", false);
      break;
    default:
      {
        if (is_production()) {
          set_feature("logs/netconfTraceLog", false);
        } else {
          set_feature("logs/netconfTraceLog", true);
        }
      }
  };

  std::map<const char*, const char*> log_types = {
    {"logs/confdLog/file",     "confd.log"},
    {"logs/developerLog/file", "devel.log"},
    {"logs/auditLog/file",     "audit.log"},
    {"logs/netconfLog/file",   "netconf.log"},
    {"logs/snmpLog/file",      "snmp.log"}
  };

  for (auto& log_type : log_types) {
    set_directory_path(log_type.first, "name", confd_log_path + log_type.second);
  }

  set_directory_path("logs/netconfTraceLog", "filename", confd_log_path + "netconf.trace");
  set_directory_path("datastores/candidate", "filename", confd_dir + "/var/confd/candidate/candidate.db");

  add_netconf_notification_streams(instance_->netconf_streams_);

  return confd_conf_dom_->to_file((confd_dir + "/rw_confd.conf").c_str());
}


bool ConfdWorkspaceMgr::set_schema_load_path()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "set schema load path");

  XMLNode* root_node = confd_conf_dom_->get_root_node();
  RW_ASSERT (root_node);

  XMLNode* load_path = root_node->find("loadPath");
  if (!load_path) {
    load_path = root_node->add_child("loadPath");
    RW_ASSERT (load_path);
  }
  auto latest_ver_dir = get_rift_install() + "/" + RW_SCHEMA_VER_LATEST_NB_PATH + "/fxs";

  if (!fs::exists(latest_ver_dir)) {
    RW_MA_INST_LOG (instance_, InstanceError, "Schema directory does not exist.");
    return false;
  }

  auto confd_yang_dir = get_rift_install() + "/usr/local/confd/etc/confd/";
  XMLNode* dir = load_path->add_child("dir", confd_yang_dir.c_str());
  RW_ASSERT(dir);
  XMLNode* dir2 = load_path->add_child("dir", latest_ver_dir.c_str());
  RW_ASSERT(dir2);

  return true;
}

void ConfdWorkspaceMgr::set_directory_path(
    const std::string& node_path,
    const std::string& elem_name,
    const std::string& elem_value)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "set directory path");

  std::vector<std::string> path_vec;
  if (node_path.length()) {
    boost::split(path_vec, node_path, boost::is_any_of("/"));
  }

  XMLNode* root_node = confd_conf_dom_->get_root_node();
  RW_ASSERT (root_node);

  XMLNode* node = root_node;
  for (auto& path : path_vec) {
    node = node->find(path.c_str());
    RW_ASSERT (node);
  }

  XMLNode* child_node = node->add_child(elem_name.c_str(), elem_value.c_str());
  RW_ASSERT (child_node);
}

void ConfdWorkspaceMgr::set_feature(const std::string& conf_node, bool value)
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "set feature");

  std::vector<std::string> path_vec;
  if (conf_node.length()) {
    boost::split(path_vec, conf_node, boost::is_any_of("/"));
  }
  
  XMLNode* root_node = confd_conf_dom_->get_root_node();
  RW_ASSERT (root_node);
  
  XMLNode* node = root_node;
  for (auto& path : path_vec) {
    node = node->find(path.c_str());
    if (!node) break;
  }

  if (!node) {
    std::string log_str;
    log_str = "Feature path does not exist in prototype config: " + conf_node;
    RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
    return;
  }

  XMLNode* enabled = nullptr;
  if (value) {
    enabled = node->add_child("enabled", "true");
  } else {
    enabled = node->add_child("enabled", "false");
  }

  RW_ASSERT (enabled);
}


bool ConfdWorkspaceMgr::create_confd_workspace()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "create confd workspace");
  auto rift_install = get_rift_install();
  auto confd_dir = mgmt_->mgmt_dir();

  std::array<const char*, 5> sub_dirs = {
    "/var/confd/candidate",
    "/var/confd/cdb",
    "/var/confd/log",
    "/var/confd/rollback",
    "/var/confd/state"
  };

  try {
    for (auto sdir : sub_dirs) {
      auto dest_dir = confd_dir + sdir;

      if (!fs::create_directories(dest_dir)) {
        std::string log_str;
        log_str = "Failed to create directory: " + dest_dir + ". ";
        log_str += std::strerror(errno);
        RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
        return false;
      }
    }
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return false;
  }

  try {
    // Copy aaa_init.xml file
    fs::copy_file(rift_install + CONFD_BASE_PATH + "/var/confd/cdb/aaa_init.xml",
        confd_dir + "/var/confd/cdb/aaa_init.xml");

    // Install the oper user aaa restrictions
    fs::copy_file(rift_install + "/usr/data/security/oper_user_restrictions.xml",
        confd_dir + "/var/confd/cdb/oper_user_restrictions.xml");

  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return false;
  }

  return true;
}


rw_status_t ConfdWorkspaceMgr::create_mgmt_directory()
{
  RWMEMLOG(memlog_buf_, RWMEMLOG_MEM2, "create mgmt directory");
  auto confd_dir = mgmt_->mgmt_dir();

  if (!confd_dir.length()) {
    RW_MA_INST_LOG (instance_, InstanceCritInfo, "Workspace not yet created. Retrying.");
    return RW_STATUS_FAILURE;
  }

  if (fs::exists(confd_dir))
  {
    auto confd_init_file = confd_dir + "/" + CONFD_INIT_FILE;

    if (!fs::exists(confd_init_file)) {
      RW_MA_INST_LOG (instance_, InstanceCritInfo,
          "Confd init file not found. Perhaps confd was not initialized"
          " completely in its previous run. Deleting existing confd directory.");
      try {
        fs::remove_all(confd_dir);
      } catch (const fs::filesystem_error& e) {
        RW_MA_INST_LOG (instance_, InstanceError, e.what());
      }

    } else {
      std::string log_str;
      log_str = "Confd workspace is " + confd_dir;
      RW_MA_INST_LOG (instance_, InstanceInfo, log_str.c_str());
      return RW_STATUS_SUCCESS;
    }
  }

  // Create confd workspace
  if (create_confd_workspace() &&
      (generate_config_file() == RW_STATUS_SUCCESS))
  {
    std::string log_str;
    log_str = "Confd workspace " + confd_dir +
      " created successfully";
    RW_MA_INST_LOG (instance_, InstanceCritInfo, log_str.c_str());
  } else {
    RW_MA_INST_LOG (instance_, InstanceError, "Failed to create confd workspace");
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

void ConfdWorkspaceMgr::add_netconf_notification_streams(
                const std::vector<netconf_stream_info_t>& streams)
{
  XMLNode* root_node = confd_conf_dom_->get_root_node();
  RW_ASSERT (root_node);
  
  XMLNode* node = root_node->find("notifications");
  RW_ASSERT (node);

  node = node->find("eventStreams");
  RW_ASSERT (node);

  for (const auto& info: streams) {
    XMLNode* xml_stream = node->add_child("stream");
    xml_stream->add_child("name", info.name.c_str());
    xml_stream->add_child("description", info.description.c_str());

    if (info.replay_support) {
      xml_stream->add_child("replaySupport", "true");
      XMLNode* replay_info = xml_stream->add_child("builtinReplayStore");
      replay_info->add_child("enabled", "true");
      replay_info->add_child("dir", mgmt_->mgmt_dir().c_str());
      replay_info->add_child("maxSize", "S1M");
      replay_info->add_child("maxFiles", "5");
    } else {
      xml_stream->add_child("replaySupport", "false");
    }
  }
}

