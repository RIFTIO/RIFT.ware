/* STANDARD_RIFT_IO_COPYRIGHT */
/*
 * @file rwuagent_mgmt_system.hpp
 *
 * Management agent startup phase handler
 */

#ifndef CORE_MGMT_RWUAGENT_STARTUP_HPP_
#define CORE_MGMT_RWUAGENT_STARTUP_HPP_

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include <map>
#include <cstring>
#include <memory>
#include <utility>
#include <functional>

#include <rwmemlog.h>
#include <rwmemlog_mgmt.h>
#include <rwmemlogdts.h>
#include <rw_xml.h>
#include "rwuagent.hpp"

namespace rw_uagent {

class SbReqRpc;
class BaseMgmtSystem;

/*!
 * Plugin registration interface
 */
class IPluginRegister
{
public:
  IPluginRegister() = default;
  IPluginRegister(const IPluginRegister&) = delete;
  void operator=(const IPluginRegister&) = delete;
  virtual BaseMgmtSystem* CreateObject(Instance*) = 0;
  virtual ~IPluginRegister() {}
};


/*!
 * MgmtSystemFactory is a singleton which
 * enables registration of Nb plugins
 * via NbPluginRegister
 */
class MgmtSystemFactory
{
public:
  /*!
   * Return the singleton instance
   */
  static MgmtSystemFactory& get();

  /*!
   * Register the provided ID and the constructor
   * object in the plugin registration map
   */
  bool register_nb(const std::string& id, IPluginRegister* c);

  /*!
   * Checks if the plugin by the name 'id'
   * is registered in the factory or not.
   */
  bool is_registered(const std::string& id) const;

  /*
   * Creates the object againt the registered ID
   * Asserts if the object is not found in the
   * plugin registration map.
   * The caller should take the owner ship of
   * the returned BaseMgmtSystem instance.
   */
  std::unique_ptr<BaseMgmtSystem>
  create_object(const std::string& id, Instance* inst);

private:
  MgmtSystemFactory() {}
  MgmtSystemFactory(const MgmtSystemFactory&);
  void operator=(const MgmtSystemFactory&);

private:

  std::map<std::string, IPluginRegister*> plugin_map_;
};



/*!
 * Concrete implementation of registration interface.
 * On construction, registers the management system class
 * with the factory and provides with an API to construct
 * an object of the registered type.
 */
template <typename Product>
class NbPluginRegister: public IPluginRegister
{
public:
  NbPluginRegister(): IPluginRegister() {
    MgmtSystemFactory::get().register_nb(Product::ID, this);
  }

  BaseMgmtSystem* CreateObject(Instance* inst) {
    return static_cast<BaseMgmtSystem*>(new Product(inst));
  }
};


//----------------------------------------------------------------------------

/*!
 * Management agent system base class. This is
 * a pure virtual class. Concrete implementations exist for Confd
 * and libnetconf servers.
 * An instance of the concrete implementation will be created and
 * owned by the rwuagent Instance class. This instance will be used
 * to make the confd/libnetconf daemon progress through its startup/
 * initialization phase till it opens up its northbound interfaces
 * (NETCONF/CLI/REST etc).
 *
 * The need for such a management system class is to make the bootup of
 * of management agent in sync with the Confd/libnetconf server
 * startup.
 *
 * In case of Confd, it would be started in phase-0 and would be
 * moved to next phase as and when management agent proceeds through
 * its startup phase.
 *
 * Also the concrete implementation would be aware of system specific
 * information.
 */
class BaseMgmtSystem
{
public:
  BaseMgmtSystem(Instance*, const char*);

  /// This class and its derived classes would be
  // non copyable and non assignable
  BaseMgmtSystem(const BaseMgmtSystem& other) = delete;
  void operator=(const BaseMgmtSystem& other) = delete;

  virtual ~BaseMgmtSystem() = default;

public:
  /*!
   * Called/Scheduled from the Agent startup code.
   * Calls the system_startup API of the concrete
   * implementation.
   */
  static void system_startup_cb(void* ctx);

  /*!
   * Does the initialization of the mgmt system by creating instances
   * of objects required for creating Nb transactions.  Will be called
   * periodically and repeatedly until it returns true.
   */
   virtual bool system_startup() = 0;

   /*!
    * Do the system initialization.
    * Called after the management system is detected
    * to be running via is_instance_ready API.
    */
   virtual rw_status_t system_init() = 0;

  /*!
   * Calls the correct callback as per the current state
   * of the state machine.
   * State machine is managed and implemented by the
   * concrete implementation.
   */
  virtual void proceed_to_next_state() = 0;

  /*!
   * Starts the configuration management process via
   * VCS by making an RPC call via DTS.
   */
  virtual void start_mgmt_instance() = 0;

  /*!
   * To be called when agent receives a subscriber promotion
   * event from HA manager.
   * Concrete implementation should implement the necessary 
   * actions.
   */
  virtual void on_ha_state_change_event(HAEventType event) = 0;

  /*!
   * Returns true if Agent is transitioning from
   * standby mode to master mode.
   */
  virtual bool is_in_transition() const noexcept = 0;

  virtual rw_status_t annotate_schema() {
    return RW_STATUS_SUCCESS;
  }

  virtual void start_upgrade(int modules) {
    return;
  }

  /*!
   * Forwards the notification to the concrete implementation
   * to handle
   */
  virtual rwdts_member_rsp_code_t
  handle_notification(const ProtobufCMessage* msg) = 0;

  /*!
   * Creates the directory structure required by the management system.
   */
  virtual rw_status_t create_mgmt_directory() = 0;

  /*!
   * Forwards the RPC to the concreate implementation to handle.
   *
   * ATTN: The management system should register for all of its
   * specific RPCs explicitly and directly.  The core should not be
   * intercepting them.
   */
  virtual StartStatus handle_rpc(SbReqRpc* parent_rpc, const ProtobufCMessage* msg) = 0;

  /*!
   * Forwards the get request call to the concrete implementation.
   *
   * ATTN: The management system should register for all of its
   * specific RPCs explicitly and directly.  The core should not be
   * intercepting them.
   *
   * ATTN: The way the agent handles gets for local data must be fixed
   * to be published through DTS as any other application.  RIFT-8794
   */
  virtual StartStatus handle_get(SbReqGet* parent_get, rw_yang::XMLNode* node) = 0;

  /*!
   * Get the management component name.
   */
  virtual const char* get_component_name() = 0;

  /*!
   * Prepare phase of a config transaction.
   */
  virtual rw_status_t prepare_config(rw_yang::XMLDocument* dom) = 0;

  /*!
   * Commit phase of a config transaction.
   * This function must be left to do only trivial tasks.
   * Most of the initial tasks must be done during
   * prepare_config.
   */
  virtual rw_status_t commit_config() = 0;

  /*!
   * Gets the management system directory from which
   * it works.
   */
  virtual const std::string& mgmt_dir() const noexcept = 0;

  /*!
   * Returns true if the management system is under
   * reload mode.
   */
  virtual bool is_under_reload() const noexcept = 0;

  /*!
   * Checks if the mgmt system is ready to accept requests
   * from Nb agents
   */
  virtual bool is_ready_for_nb_clients() const noexcept = 0;

  /*!
   * Checks if the mgmt instance has been started or not.
   */
  virtual bool is_instance_ready() const noexcept = 0;

  /*!
   * Returns the pointer to the log file manager instance
   */
  LogFileManager * log_file_manager() const noexcept {
    return log_file_manager_.get();
  }

protected:
  Instance* instance_ = nullptr;
  RwMemlogBuffer memlog_buf_;
  /// log file manager
  std::unique_ptr<LogFileManager> log_file_manager_;

private:
  // Create config file for logrotate
  rw_status_t do_create_logrotate_config(
      RWPB_T_MSG(RwMgmtagt_LogrotateConf)*);
};

}

#endif
