
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*
 * @file rwuagent_confd_mgmt_system.hpp
 *
 * Confd management system
 */
#ifndef CORE_MGMT_RWUAGENT_CONFD_MGMT_SYSTEM_HPP_
#define CORE_MGMT_RWUAGENT_CONFD_MGMT_SYSTEM_HPP_

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

#include "rwuagent.hpp"
#include "rwuagent_mgmt_system.hpp"

namespace rw_uagent {

class ConfdDaemon;
class ConfdMgmtSystem;
class ConfdWorkspaceMgr;


/*!
 * Workspace manager for confd process.
 * It creates a new workspace for confd
 * based on whether persist/unique flag is on/off.
 * Copies all the required files needed by confd runtime
 * from the installation.
 * Generates the confd configuration file, rw_confd.conf
 * from the prototype configuration file.
 */
class ConfdWorkspaceMgr
{
public:
  ConfdWorkspaceMgr(const ConfdMgmtSystem* mgmt, Instance* inst);

  ConfdWorkspaceMgr(const ConfdWorkspaceMgr&) = delete;
  void operator=(const ConfdWorkspaceMgr&) =delete;

public:
  /*!
   * Creates the directory structure required by
   * confd and copies the required files
   * from the installation.
   * Also generates the confd configuration
   * file, rw_confd.conf from the prototype file.
   */
  rw_status_t create_mgmt_directory();

  /*!
   * Generates the confd configuration file,
   * rw_confd.conf from the prototype conf file.
   * The protototype file is read from
   * ${RIFT_INSTALL}/etc/rw_confd_prototype.conf file
   * and written at confd root workspace.
   */
  rw_status_t generate_config_file();

private:
  bool set_schema_load_path();
  void set_directory_path(const std::string& path,
                          const std::string& elem_name,
                          const std::string& elem_value);

  /*!
   * Add the predefined Netconf notification streams to the Confd conf file.
   */
  void add_netconf_notification_streams(
                const std::vector<netconf_stream_info_t>& streams);

  // Enable/Disable a feature in config file
  void set_feature(const std::string& confd_node, bool value);
  bool create_confd_workspace();
  bool copy_dir(const boost::filesystem::path& source, const boost::filesystem::path& destination);

private:

  const ConfdMgmtSystem* mgmt_ = nullptr;
  Instance* instance_ = nullptr;
  RwMemlogBuffer memlog_buf_;
  // XML DOM of prototype config file
  rw_yang::XMLDocument::uptr_t confd_conf_dom_  = nullptr;
  std::string confd_dir_;
};


/*!
 * Management system specialized for Confd interaction.
 * It makes use of MAAPI API's to talk with confd and
 * to monitor its progress.
 * Confd startup is divided into multiple phases:
 * 1. Phase-0:
 *    This is the phase in which Confd will be started.
 *    Once initialized in this phase, uAgent can start
 *    with data provider/notification registrations.
 *
 * 2. Phase-1:
 *    Once uAgent is done with doing the registrations and
 *    setting up the socket connections, it will signal
 *    the Confd daemon to proceed to Phase-1. In this
 *    phase, Confd basically initializes the CDB.
 *    Once Confd is done initializing this phase, uAgent can
 *    now add CDB subscribers.
 *    It will immidiately signal Confd to proceed to phase-2.
 *
 * 3. Phase-2:
 *    In this phase, Confd will bind and start listening to
 *    NETCONF, CLI, REST etc addresses / ports.
 *    After this, the management system can be said to be
 *    ready for accepting traffic.
 *
 */
class ConfdMgmtSystem final: public BaseMgmtSystem
{
public:
  ConfdMgmtSystem(Instance* instance);

  ~ConfdMgmtSystem() {
    close(sock_);
    close(read_sock_);
  }

public:
  using CB = void(*)(void*);

  ConfdDaemon* confd_daemon() const noexcept {
    return confd_daemon_.get();
  }

public:
  // Plugin registration entries
  static const char* ID;
  static NbPluginRegister<ConfdMgmtSystem> registrator;

public:
  bool system_startup() override;

  void start_mgmt_instance() override;

  rw_status_t system_init() override;

  void proceed_to_next_state() override;

  rwdts_member_rsp_code_t handle_notification(const ProtobufCMessage* msg) override;

  StartStatus handle_rpc(SbReqRpc* parent_rpc, const ProtobufCMessage* msg) override;

  StartStatus handle_get(SbReqGet* parent_get, XMLNode* node) override;

  void start_upgrade(int modules) override;

  rw_status_t annotate_schema() override;

  rw_status_t prepare_config(rw_yang::XMLDocument* dom) override {
    return RW_STATUS_SUCCESS;
  }

  rw_status_t commit_config() override {
    return RW_STATUS_SUCCESS;
  }

  const char* get_component_name() override {
    return "confd";
  }

  const std::string& mgmt_dir() const noexcept override {
    return confd_dir_;
  }

  bool is_under_reload() const noexcept override {
    return is_under_reload_;
  }

  bool is_instance_ready() const noexcept override {
    return inst_ready_;
  }

  bool is_ready_for_nb_clients() const noexcept override {
    return ready_for_nb_clients_;
  }

  rw_status_t create_mgmt_directory() override;


private:
  enum confd_phase_t {
    PHASE_0 = 0,
    PHASE_1,
    PHASE_2,
    RELOAD,
    DONE,
    TRANSITIONING, // in between phase change
  };

  std::array<const char*, 6> phase_2_str {{
    "PHASE_0", "PHASE_1", "PHASE_2", "RELOAD",
    "DONE", "TRANSITIONING"
  }};

private:
  static void retry_mgmt_start_cb(void* ctx);

  static void config_mgmt_start_cb(
       rwdts_xact_t* xact,
       rwdts_xact_status_t* xact_status,
       void* user_data);

  void config_mgmt_start(
       rwdts_xact_t*        xact,
       rwdts_xact_status_t* xact_status);

  /*!
   * Creates a configuration Item in the agent
   * config DOM for vstart RPC to work.
   */
  void create_proxy_manifest_config();

  void close_sockets();
  void retry_phase_cb(CB cb);

  // State-1 callback
  static void start_confd_phase_1_cb(void* ctx);
  void start_confd_phase_1();

  // State-2 callback
  static void start_confd_phase_2_cb(void* ctx);
  void start_confd_phase_2();

  // State-3 callback
  static void start_confd_reload_cb(void* ctx);
  void start_confd_reload();

  confd_phase_t curr_phase() const noexcept {
    return state_.first;
  }
  confd_phase_t next_phase() const noexcept {
    return state_.second;
  }

private:
  // Callback function pointer
  typedef void (*MFP)(void*);

  // pair.first = curr_state
  // pair.second = next_state
  using State = std::pair<confd_phase_t, confd_phase_t>;
  State state_{PHASE_0, PHASE_1};
  std::map<State, MFP> state_mc_;

  int sock_ = -1;
  int read_sock_ = -1;

  std::string confd_dir_;
  bool is_under_reload_ = false;
  bool inst_ready_ = false;
  bool ready_for_nb_clients_ = false;

  /// Confd Deamon that supports data retrieval and actions
  std::unique_ptr<ConfdDaemon> confd_daemon_ = nullptr;

  // Takes care of creating confd workspace and
  // generating confd configuration file
  ConfdWorkspaceMgr ws_mgr_;

  // ATTN: dont be lazy, create protobuf
  // ATTN: will this leave whitespace in the DOM?
  constexpr static const char* manifest_cfg_ =
      "<data>"
      "<rw-manifest:manifest xmlns:rw-manifest=\"http://riftio.com/ns/riftware-1.0/rw-manifest\">"
      "  <rw-manifest:inventory>"
      "    <rw-manifest:component>"
      "      <rw-manifest:component-name>confd</rw-manifest:component-name>"
      "      <rw-manifest:component-type>PROC</rw-manifest:component-type>"
      "      <rwvcstypes:native-proc xmlns:rwvcstypes=\"http://riftio.com/ns/riftware-1.0/rw-manifest\">"                                         "      <rwvcstypes:exe-path>RIFT_INSTALL/usr/local/confd/bin/confd</rwvcstypes:exe-path>"
      "      <rwvcstypes:args>--conf CONFD_DIR/rw_confd.conf"
                            " --smp 4 --start-phase0 --foreground</rwvcstypes:args>"
      "      </rwvcstypes:native-proc>"
      "    </rw-manifest:component>"
      "  </rw-manifest:inventory>"
      "</rw-manifest:manifest>"
      "</data>";
};

} // end namespace

#endif //CORE_MGMT_RWUAGENT_CONFD_MGMT_SYSTEM_HPP_
