/* STANDARD_RIFT_IO_COPYRIGHT */
/*
 * @file rwuagent_xml_mgmt_system.cpp
 *
 * XML Management agent handler
 */

#include <chrono>
#include <sstream>
#include "rwuagent_xml_mgmt_system.hpp"
#include "rw_xml_dom_merger.hpp"
#include "rw_xml_validate_dom.hpp"


using namespace rw_uagent;
namespace fs = boost::filesystem;

const char* XMLMgmtSystem::ID = "XMLMgmtSystem";
NbPluginRegister<XMLMgmtSystem> XMLMgmtSystem::registrator;

XMLMgmtSystem::XMLMgmtSystem(Instance* instance):
  BaseMgmtSystem(instance, "XMLMgmtSystem"),
  config_writer_(new XMLConfigWriter(this, instance_))
{
}

XMLMgmtSystem::~XMLMgmtSystem()
{
}

bool XMLMgmtSystem::system_startup()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "system startup");

  auto rift_var_root = get_rift_var_root();
  char hostname[RWUAGENT_MAX_HOSTNAME_SZ];
  hostname[RWUAGENT_MAX_HOSTNAME_SZ - 1] = 0;
  int res = gethostname(hostname, RWUAGENT_MAX_HOSTNAME_SZ - 2);
  RW_ASSERT(res != -1);

  if (instance_->non_persist_ws_) {
    unsigned long seconds_since_epoch =
      std::chrono::duration_cast<std::chrono::seconds>
          (std::chrono::system_clock::now().time_since_epoch()).count();

    std::ostringstream oss;
    oss << RW_SCHEMA_MGMT_TEST_PREFIX << &hostname[0] << "." << seconds_since_epoch;
    xml_dir_ = std::move(oss.str());
  } else {

    rwtasklet_info_ptr_t tasklet_info = instance_->rwtasklet();

    if (   tasklet_info
        && tasklet_info->rwvcs
        && tasklet_info->rwvcs->pb_rwmanifest
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt
        && tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->persist_dir_name) {

      xml_dir_ = tasklet_info->rwvcs->pb_rwmanifest->bootstrap_phase->rwmgmt->persist_dir_name;

      std::size_t pos = xml_dir_.find(RW_SCHEMA_MGMT_PERSIST_PREFIX);
      if (pos == std::string::npos || pos != 0) {
        xml_dir_.insert(0, RW_SCHEMA_MGMT_PERSIST_PREFIX);
      }

    } else {
      std::ostringstream oss;
      oss << RW_SCHEMA_MGMT_PERSIST_PREFIX << &hostname[0];
      xml_dir_ = std::move(oss.str());
    }
  }
  xml_dir_ = rift_var_root + "/" + xml_dir_;

  // Create the directory
  bool failed = false;
  try {
    std::array<std::string, 2> paths = {xml_dir_, xml_dir_ + "/var/agent/log"};
    for (auto& path : paths) {
      if (!fs::exists(path)) {
        if (!fs::create_directories(path)) {
          failed = true;
          std::string log_str;
          log_str = "Failed to create directory: " + path + ". ";
          log_str += std::strerror(errno);
          RW_MA_INST_LOG (instance_, InstanceError, log_str.c_str());
          break;
        }
      }
    }
  } catch (const fs::filesystem_error& e) {
    failed = true;
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
  }

  if (failed) {
    return false;
  }

  log_file_manager_.reset(new LogFileManager(instance_));

  log_file_manager_->register_config();
  return true;
}

void XMLMgmtSystem::reload_trx_complete_cb(void* ctx)
{
  auto* self = static_cast<XMLMgmtSystem*>(ctx);
  RW_ASSERT (self);

  self->under_reload_ = false;
  self->inst_ready_ = true;
  self->ready_for_nb_clients_ = true;
  self->instance_->dts()->publish_config_state();
  self->instance_->dts()->publish_internal_hb_event();
}

void XMLMgmtSystem::start_mgmt_instance()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "start mgmt instance");
  RW_MA_INST_LOG (instance_, InstanceCritInfo, "Start reloading configuration");
  under_reload_ = true;
  rw_yang::XMLDocument::uptr_t reload_dom = reload_configuration();

  XMLNode* root_node = nullptr;
  if (reload_dom) {
    root_node = reload_dom->get_root_node();
  }

  if (!root_node || !root_node->get_first_child()) {
    // No configuration reloaded

    // Fill defaults to the reload dom by calling copy_and_merge
    rw_yang::XMLDocument::uptr_t empty_dom = \
                  instance_->xml_mgr()->create_document();
    rw_yang::XMLDocMerger merger(instance_->xml_mgr(), empty_dom.get());
    reload_dom = std::move(merger.copy_and_merge(empty_dom.get()));

    if (reload_dom) {
      root_node = reload_dom->get_root_node();
    }

    // No defaults!
    if (!root_node || !root_node->get_first_child()) {
      reload_trx_complete_cb(this);
      return;
    }
  }

  // nbreq will be deleted in its own context
  // ATTN: Inefficient converting it back to string again
  auto nbreq = new NbReqXmlReload(instance_, this, reload_dom->to_string());
  nbreq->execute();
}

rwdts_member_rsp_code_t
XMLMgmtSystem::handle_notification(const ProtobufCMessage* msg)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle notification");
  return RWDTS_ACTION_OK;
}

StartStatus XMLMgmtSystem::handle_rpc(SbReqRpc* parent_rpc, const ProtobufCMessage* msg)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle rpc");
  parent_rpc->internal_done();
  return StartStatus::Done;
}

StartStatus XMLMgmtSystem::handle_get(SbReqGet* parent_get, XMLNode* node)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "handle get");
  return StartStatus::Done;
}

rw_status_t XMLMgmtSystem::prepare_config(rw_yang::XMLDocument* dom)
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "prepare config");
  return config_writer_->prepare_config(dom);
}

rw_status_t XMLMgmtSystem::commit_config()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "commit config");
  return config_writer_->commit_config();
}

rw_yang::XMLDocument::uptr_t XMLMgmtSystem::reload_configuration()
{
  RWMEMLOG (memlog_buf_, RWMEMLOG_MEM2, "reload config");

  std::string latest = xml_dir_ + "/latest";
  std::string cfg_file;
  rw_yang::XMLDocument::uptr_t merged_dom;

  try {
    fs::path tpath = xml_dir_ + "/" + fs::read_symlink(latest).string();
    if (!tpath.empty()) {
      cfg_file = tpath.string();
    }
  } catch (const fs::filesystem_error& e) {
    RW_MA_INST_LOG (instance_, InstanceCritInfo, e.what());
  }

  if (!cfg_file.length()) {
    // Check for *.xml file with the highest serial_num
    uint64_t max_ser_num = 0;
    std::string cfg_to_load;

    for (int i = 0; i < XMLConfigWriter::NUM_CONFIG_FILES; i++) {
      std::string cfg = xml_dir_ + "/" + "cfg" + std::to_string(i) + ".xml";
      if (!fs::exists(cfg)) continue;

      auto dom(instance_->xml_mgr()->create_document_from_file(cfg.c_str(), false));
      if (!dom || !dom->get_root_node()) continue;

      uint64_t id = config_writer_->get_serial_number(dom->get_root_node());
      if (id > max_ser_num) {
        max_ser_num = id;
        cfg_to_load = cfg;
      }
    }

    if (max_ser_num == 0 || !cfg_to_load.length()) {
      RW_MA_INST_LOG (instance_, InstanceError, "No configuration file found for reload");
      return merged_dom;
    }

    // Update latest as the cfg file with the highest serial number
    cfg_file = cfg_to_load;
  }

  try {
    rw_yang::XMLDocument::uptr_t empty_dom = \
                  instance_->xml_mgr()->create_document();
    rw_yang::XMLDocument::uptr_t config_dom = \
                  instance_->xml_mgr()->create_document_from_file(
                                           cfg_file.c_str(), false);
    rw_yang::XMLDocMerger merger(instance_->xml_mgr(), empty_dom.get());

    // A copy-merged DOM will be devoid of formatting whitespace
    merged_dom = std::move(merger.copy_and_merge(config_dom.get()));
    if (merged_dom) {
      rw_yang::ValidationStatus result = rw_yang::validate_dom(merged_dom.get());
      if (result.status() != RW_STATUS_SUCCESS) {
        std::stringstream ss;
        ss << "Reload Config Validation failed. Reason: " << result.reason();
        RW_MA_INST_LOG (instance_, InstanceError, ss.str().c_str());
      }
    } else {
      RW_MA_INST_LOG (instance_, InstanceError, 
            "Reload config copy-merge failed");
    }
  } catch (const std::exception& e) {
    RW_MA_INST_LOG (instance_, InstanceError, e.what());
    return merged_dom;
  }

  if (!merged_dom) {
    RW_MA_INST_LOG (instance_, InstanceError, "Config reload failed");
    return merged_dom;
  }

  // Update the config writer serial number
  config_writer_->set_serial_number(
      config_writer_->get_serial_number(merged_dom->get_root_node()));

  RW_MA_INST_LOG (instance_, InstanceInfo, "Configuration reload finished.");

  return merged_dom;
}
