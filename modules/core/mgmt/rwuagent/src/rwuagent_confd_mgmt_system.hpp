/* STANDARD_RIFT_IO_COPYRIGHT */
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
  void set_ip_addr(const std::string& path,
                   const std::string& elem_name,
                   const std::string& elem_value);

  /*!
   * Add the predefined Netconf notification streams to the Confd conf file.
   */
  void add_netconf_notification_streams(
                const std::vector<netconf_stream_info_t>& streams);

  /*!
   * Configure confd high availability mechanism
   */
  void enable_confd_ha(bool enable);

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
 * ConfdHAMgr manages the actions and the callbacks required to 
 * configure Confd in HA mode correctly.
 * It makes use of confd HA library to talk and configure confd
 * in HA mode.
 */
class ConfdHAMgr
{
public:
  ConfdHAMgr(ConfdMgmtSystem* mgmt, Instance* instance);
  ConfdHAMgr(const ConfdHAMgr&) = delete;
  void operator=(const ConfdHAMgr&) = delete;
  ~ConfdHAMgr();

public:
  /*!
   * Creates the socket for communicating with confd
   * and connects to confd using ha api.
   * Caller needs to retry in case of failure
   */
  rw_status_t ha_init();

  /*!
   * Event callback when the state of the agent changes
   * Received when the state of the agent changes from
   * standby to master OR when a new master is elected.
   */
  void on_ha_state_change_event(HAEventType event);

  /*!
   * Called by the state machine to prepare confd
   * in either master or standby state.
   */
  void set_confd_ha_state();

  /*!
   * Predicate to check if Agent has recieved details
   * about the master VM. This information
   * is mainly required for the standby instance.
   */
  bool has_master_details() noexcept;

  /*!
   * Subscribes with DTS to receive details of master VM.
   * (IP and instance name)
   * The subscription is done only when the Agent
   * is running as a standby in the cluster.
   */
  void subscribe_for_master_mgmt_info();

private:
  // HA specific callbacks

  // Callback registerd with DTS for receiving change
  // in master VM details
  static void active_mgmt_info_cb(const char*, const char*, void*);

  static void set_confd_ha_state_cb(void* ctx);

  static void update_master_confd_ip_cb(void* ctx);

  void update_master_confd_ip();

  // Make the confd managed by this agent
  // as master. 
  bool make_confd_master();
  // Make the confd managed by this agent
  // as standby. Also provides the information
  // related to master (IP/nodeid)
  bool make_confd_standby();

private:
  struct confd_ha_master_info_t {
    std::string node_id;
    std::string ip;
  };

  confd_ha_master_info_t master_info_;

  ConfdMgmtSystem* mgmt_ = nullptr;
  Instance* instance_ = nullptr;
  RwMemlogBuffer memlog_buf_;
  int ha_sock_ = -1;
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
    close_sockets();
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

  void on_ha_state_change_event(HAEventType event) override;

  bool is_in_transition() const noexcept override {
    return in_transition_;
  }

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
  friend class ConfdHAMgr;

  enum confd_phase_t {
    CONFD_PHASE_0 = 0,
    CONFD_PHASE_1,
    CONFD_HA_STATE,
    SUBSCRIBE,
    CONFD_PHASE_2,
    RELOAD,
    DONE,
    TRANSITIONING, // in between phase change
    PUBLISH,
  };

  std::array<const char*, 9> phase_2_str {{
    "CONFD_PHASE_0", "CONFD_PHASE_1", "CONFD_HA_STATE", "SUBSCRIBE",
    "CONFD_PHASE_2", "RELOAD", "DONE", "TRANSITIONING", "PUBLISH",
  }};

private:
  static void retry_mgmt_start_cb(void* ctx);

  void build_master_state_machine();
  void build_standby_state_machine();

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

  // confd subscription
  static void setup_confd_subscription_cb(void* ctx);
  void setup_confd_subscription();

  // confd ha mode configuration
  static void configure_confd_ha_cb(void* ctx);
  void configure_confd_ha();

  // State-2 callback
  static void start_confd_phase_2_cb(void* ctx);
  void start_confd_phase_2();

  // State-3 callback
  static void start_confd_reload_cb(void* ctx);
  void start_confd_reload();

  // Publish Agent state
  static void publish_agent_ready_state_cb(void* ctx);
  void publish_agent_ready_state();

  void standby_to_master_transition();

  confd_phase_t curr_phase() const noexcept {
    return state_.first;
  }
  confd_phase_t next_phase() const noexcept {
    return state_.second;
  }

  const std::string& hostname() const noexcept {
    return hostname_;
  }

private:
  // Callback function pointer
  typedef void (*MFP)(void*);

  // pair.first = curr_state
  // pair.second = next_state
  using State = std::pair<confd_phase_t, confd_phase_t>;
  State state_{CONFD_PHASE_0, CONFD_PHASE_1};
  std::map<State, MFP> state_mc_;

  int sock_ = -1;
  int read_sock_ = -1;

  std::string confd_dir_;
  bool is_under_reload_ = false;
  bool inst_ready_ = false;
  bool ready_for_nb_clients_ = false;
  bool in_transition_ = false;

  /// The hostname of the VM where confd is running
  std::string hostname_;

  /// Confd Deamon that supports data retrieval and actions
  std::unique_ptr<ConfdDaemon> confd_daemon_ = nullptr;

  // Takes care of creating confd workspace and
  // generating confd configuration file
  ConfdWorkspaceMgr ws_mgr_;

  // Takes care of HA related callbacks and actions
  ConfdHAMgr ha_mgr_;

  // ATTN: dont be lazy, create protobuf
  // ATTN: will this leave whitespace in the DOM?
  constexpr static const char* manifest_cfg_ =
      "<data>"
      "<rw-manifest:manifest xmlns:rw-manifest=\"http://riftio.com/ns/riftware-1.0/rw-manifest\">"
      "  <rw-manifest:inventory>"
      "    <rw-manifest:component>"
      "      <rw-manifest:component-name>COMPONENT_NAME</rw-manifest:component-name>"
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
