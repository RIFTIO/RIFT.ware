
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
 * @file rwuagent_xml_mgmt_system.hpp
 *
 * XML Management agent handler
 */

#ifndef CORE_MGMT_RWUAGENT_XML_MGMT_HPP_
#define CORE_MGMT_RWUAGENT_XML_MGMT_HPP_

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

#include "rwuagent_mgmt_system.hpp"

namespace rw_uagent {

// Fwd Decl
class SbReqRpc;
class SbReqGet;
class XMLConfigWriter;

/*!
 * Management system specialized for XML based transactions.
 *
 */

class XMLMgmtSystem final: public BaseMgmtSystem
{
public:
  XMLMgmtSystem(Instance* instance);

  ~XMLMgmtSystem();

public:
  // Plugin registration entries
  static const char* ID;
  static NbPluginRegister<XMLMgmtSystem> registrator;

public:
  bool system_startup() override;

  void start_mgmt_instance() override;

  rw_status_t system_init() override {
    return RW_STATUS_SUCCESS;
  }

  void proceed_to_next_state() override {
    return;
  }

  void on_ha_state_change_event(HAEventType event) override {
    // XML mode HA not supported yet
    RW_ASSERT_NOT_REACHED ();
    return;
  }

  bool is_in_transition() const noexcept override {
    return false;
  }

  const char* get_component_name() override {
    return "XMLMgmtSystem";
  }

  const std::string& mgmt_dir() const noexcept override {
    return xml_dir_;
  }

  bool is_under_reload() const noexcept override {
    return under_reload_;
  }

  bool is_instance_ready() const noexcept override {
    return inst_ready_;
  }

  bool is_ready_for_nb_clients() const noexcept override {
    return ready_for_nb_clients_;
  }

  rwdts_member_rsp_code_t
  handle_notification(const ProtobufCMessage* msg) override;

  rw_status_t create_mgmt_directory() override {
    return RW_STATUS_SUCCESS;
  }

  StartStatus handle_rpc(SbReqRpc* parent_rpc, const ProtobufCMessage* msg) override;

  StartStatus handle_get(SbReqGet* parent_get, XMLNode* node) override;

  rw_status_t prepare_config(rw_yang::XMLDocument* dom) override;

  /*!
   * Synchronous configuration commit
   */
  rw_status_t commit_config() override;

  static void reload_trx_complete_cb(void* ctx);

private:
  rw_yang::XMLDocument::uptr_t reload_configuration();

private:
  std::string xml_dir_;
  bool inst_ready_ = false;
  bool ready_for_nb_clients_ = false;
  bool under_reload_ = false;
  std::unique_ptr<XMLConfigWriter> config_writer_ = nullptr;
};


/*!
 * XMLConfigWriter is a heper class for XMLMgmtSystem
 * which takes care of commiting the configuration maintained
 * by the Instance to a persistent file.
 */
class XMLConfigWriter
{
public:
  XMLConfigWriter(const XMLMgmtSystem* mgmt, Instance* instance);

  // disable copy
  XMLConfigWriter(const XMLConfigWriter&) = delete;
  XMLConfigWriter& operator=(const XMLConfigWriter&) = delete;

public: // Serial number operations
  uint64_t serial_number() const noexcept {
    return serial_num_;
  }

public:
  /*!
   * Writes the configuration to a temp file
   * cfgX.xml.tmp, where X can be one of
   * (0, 1, 2....NUM_CONFIG_FILES).
   * The file chosen goes by the logic serial_num_ % NUM_CONFIG_FILES.
   */
  rw_status_t prepare_config(rw_yang::XMLDocument* dom);

  /*!
   * Renames the temp configuration file cfgX.xml.tmp to
   * cfgX.xml.
   * Once the configuration file is renamed, the symlink
   * file 'latest' is updated to point to the latest configuration
   * XML file and the serial number gets incremented by one.
   */
  rw_status_t commit_config();

  /*!
   * Gets the serial number number from the
   * root node.
   */
  uint64_t get_serial_number(XMLNode* root);

  /*!
   * Sets the serial number maintainer by this class
   */
  void set_serial_number(uint64_t sno) {
    serial_num_ = sno;
  }

private:
  void set_root_node_serial_number();

  rw_status_t persist_to_tmp_file();
  rw_status_t update_symlink();
  rw_status_t rename_config();
  void remove_temp_file();

public:
  static const char* const IDENTIFIER_NAME;
  static const char* const CFG_FILE_PREFIX;
  static const int NUM_CONFIG_FILES;

private:
  const XMLMgmtSystem* mgmt_ = nullptr;
  Instance* instance_ = nullptr;
  RwMemlogBuffer memlog_buf_;

  XMLNode* root_ = nullptr;
  uint64_t serial_num_ = 0;
  // Temporary config file name
  std::string cfg_file_;
};

}// end namespace

#endif
