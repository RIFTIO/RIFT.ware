/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwuagent_confd.hpp
 *
 * Management agent confd integration header.
 */

#ifndef CORE_MGMT_RWUAGENT_CONFD_HPP_
#define CORE_MGMT_RWUAGENT_CONFD_HPP_

#include "rwuagent.hpp"

#include <confd_lib.h>
#include <confd_cdb.h>
#include <confd_dp.h>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>

#include <rw_confd_upgrade.hpp>
#include <rw_confd_annotate.hpp>
#include <confd_xml.h>
#include <rw-mgmtagt.confd.h>
#include "rw-mgmtagt-confd.pb-c.h"


namespace rw_uagent {

const uint8_t RW_MAX_CONFD_KEYS = 20;
const int RWUAGENT_MAX_CONFD_STATS_CLIENT = 300;
const int RWUAGENT_MAX_CONFD_STATS_DAEMON = 300;
const size_t RWUAGENT_CONFD_MULTI_GET_MAX_NODES = 20;
const size_t RWUAGENT_CONFD_MULTI_GET_MAX_TLV = 200;
const int RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH = 1024;

const int MAX_UAGENT_CONFD_CONNECTION_ATTEMPT = 20;
const int RWUAGENT_RETRY_CONFD_CONNECTION_TIMEOUT_SEC = 3;

const uint8_t RW_MAX_CONFD_WORKERS = 20;
const uint8_t RWUAGENT_DAEMON_DEFAULT_POOL_SIZE = 10;


typedef enum rw_confd_hkeypath_cmp_t
{
  HKEYPATH_EQUAL = 20150107,
  HKEYPATH_IS_PARENT,
  HKEYPATH_IS_CHILD,
  HKEYPATH_DIVERGES
} rw_confd_hkeypath_cmp_t;

typedef enum rw_confd_dp_state_t
{
  GET_FIRST_KEY = 0xF00BA3,
  GET_FIRST_OBJECT,
  GET_ELEMENT,
  GET_OBJECT,
  DOM_READY
} rw_confd_dp_state_t;

typedef enum rw_confd_client_type_t
{
  INVALID = 0xBA3F00,
  CLI,
  RESTCONF,
  NETCONF,
  MAAPI_UPGRADE,
} rw_confd_client_type_t;

typedef RWPB_E(RwMgmtagtConfd_CallbackType) RwMgmtagt_Confd_CallbackType;

inline
void getdatetime(struct confd_datetime *datetime)
{
    struct tm tm;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &tm);

    memset(datetime, 0, sizeof(*datetime));
    datetime->year = 1900 + tm.tm_year;
    datetime->month = tm.tm_mon + 1;
    datetime->day = tm.tm_mday;
    datetime->sec = tm.tm_sec;
    datetime->micro = tv.tv_usec;
    datetime->timezone = 0;
    datetime->timezone_minutes = 0;
    datetime->hour = tm.tm_hour;
    datetime->min = tm.tm_min;
}


class NbReqConfdConfig;
class NbReqConfdDataProvider;
class NbReqConfdRpcExec;
class ConfdDaemon;
class ConfdDomStats;
class LogFileManager;
struct NodeMap;
struct OperationalDomMap;

/*!
 * Dispatch source context used for callbacks
 * from confd sockets.
 */
struct DispatchSrcContext
{
  // Unique-ptr for auto cleanup.
  typedef std::unique_ptr<DispatchSrcContext> dsctx_uptr_t;

  // The confd deamon back pointer.
  ConfdDaemon* confd_;

  // The worker socket file descriptor.
  int confd_fd_;

  // The dispatch queue source created for the socket.
  rwsched_dispatch_source_t dp_src_;
};


/**
 * ConfdUpgradeContext as the name indicates keeps the
 * contextual information required to perform the
 * upgrade.
 * Note that, it owns the serial dispatch queue
 * which will be used for performing upgrades so as to
 * free the main thread.
 * Use of serial queue also gurantees that at no time
 * two or more upgrades would be performed
 */
 struct ConfdUpgradeContext
 {
   /// Flag to check if upgrade must be done or not
   bool ready_for_upgrade_ = true;
   
   ///Store the version of yang schema fxs files
   uint32_t schema_version_ = 1;
   
   /// Upgrade manager pointer
   rw_yang::ConfdUpgradeMgr *upgrade_mgr_ = nullptr;
   
   /// Serial dispatch queue for running upgrades
   rwsched_dispatch_queue_t serial_upgrade_q_;
 };


/**
 * Confd Configuration NbReq
 *
 * The confd Configuration NbReq subscribes to CDB events, and process the
 * callbacks from CDB.
 *
 * As there can be atmost one commit that can be done on a CDB at a time, there
 * will be one Confd Configuration Handler per UAGENT instance. This object
 * owns the FD, the CF Source and CF Socket objects.
 *
 * CDB calls into the configuration handler during COMMIT phase. In the prepare
 * phase, the UAGENT uses functionality of tthe configuration handler object to
 * distribute the configuration to applications connected to the UAGENT. Once all
 * connected applications respond, UAGENT responds with a synch done call to CONFD.
 *
 */

class NbReqConfdConfig
: public NbReq
{
 public:
  /**
   * ATTN: The constructor does not do much. This is to avoid the need to handle
   * exceptions.
   */
  NbReqConfdConfig (
      Instance *instance  /**< [in] Instance to which client belongs to */
                     );

  ~NbReqConfdConfig();

  NbReqConfdConfig(const NbReqConfdConfig&) = delete;
  NbReqConfdConfig& operator = (const NbReqConfdConfig&) = delete;

  /**
   * Connect to CDB, and subscribe to events from it
   */
 public:

  // Sets up the schema for confd
  // and start confd_config instance
  rw_status_t setup_confd_subscription();

  // Annotate the YANG model with tailf hash values
  void annotate_yang_model_confd();

  rw_status_t setup();

  /// reload configuration from CDB during startup
  static void reload_configuration(void* ctxt);

  /// Async dispatch of reload_configuration
  static void async_reload_configuration(NbReqConfdConfig* self);

  /// process a specific subscription
  void process_confd_subscription();

  /// build a dom, and send it to all subscribers after doing necessary splits
  void prepare(int *subscription_points, int length, int flags);

  /**
   * The core foundation call back when data is available on the subscription
   * socket.
   */
  static void cfcb_confd_sub_event(
    rwsched_CFSocketRef s, ///< [in] The server socket
    CFSocketCallBackType callbackType, ///< [in] The callback type, should be Accept
    CFDataRef address, ///< [in] The remote client network address
    const void* data, ///< [in] The fd of the accepted connection
    void* vconfig ///< [in] The NbReqConfdConfig object
  );

  // Called back asynchronously once the 
  // DTS transaction is committed successfully.
  // Also calls the API to commit the changes in the
  // transaction to the Agent DOM.
  StartStatus respond(
    SbReq* sbreq
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

  /// execute the test that was configured
  rw_yang_netconf_op_status_t execute();

  /// Create a socket connection to confd
  rw_status_t connect(
      int *fd, /**< FD created for the connection */
      cdb_sock_type type /**< type of CDB connection */
  );

  // Close a CF socket
  void close_cf_socket(rwsched_CFSocketRef s);

 private:
  void commit_changes();

  // Aborts the SbReq DTS transaction
  // Called when Confd aborts the commit operation
  void abort_changes();

 public:
  /// fd assigned for the confd subscription socket
  int sub_fd_ = RWUAGENT_INVALID_SOCK_ID;

  /// fd assigned for the confd data  socket
  int data_fd_ = RWUAGENT_INVALID_SOCK_ID;

  /// 
  int reload_fd_ = RWUAGENT_INVALID_SOCK_ID;

  /// The confd subscription port rwsched socket reference
  rwsched_CFSocketRef sub_sock_ref_;

  /// The confd subscription port rwsched socket reference
  rwsched_CFRunLoopSourceRef sub_sock_src_;

  /// The rwmsg response closure, needed to send back response
  NetconfRsp_Closure rsp_closure_;

  /// The rwmsg closure data, needed to send back response
  void* closure_data_ = nullptr;

  std::string response_;


  /// A flag to determine whether the constructor got valid data.
  bool valid_ = true;

  /// A flag to determine whether this transaction has to be aborted
  bool abort_ = false;

  /// The subscription id assigned by confd when subscribing
  int sub_id_ = 0;

  /// The DTS transaction in progress - at most one. Should be null to start new config
  SbReqEditConfig *xact_ = nullptr;

  /// Serial queue for performing configuration reload from CDB
  rwsched_dispatch_queue_t reload_q_;

 private:
  typedef std::unordered_map<rwsched_CFSocketRef,rwsched_CFRunLoopSourceRef> cf_src_map_t;

  /// A mapping from RW-sched assigned handles to sources
  cf_src_map_t cf_srcs_;

};

class ConfdDomStats
{
 public:
  RWPB_T_MSG(RwMgmtagtConfd_CachedDom)  dom_params_;
  struct timeval                   create_time_;
  struct timeval                   last_access_time_;

  typedef std::map<RwMgmtagt_Confd_CallbackType,RWPB_T_MSG(RwMgmtagtConfd_Callbacks) *  > cb_map_t;
  cb_map_t  callbacks_;

  ConfdDomStats (const char *hkeypath,
                 const char *dts_key);
  ~ConfdDomStats();
  void log_state (RwMgmtagt_Confd_CallbackType type);
  RWPB_T_MSG(RwMgmtagtConfd_CachedDom) *get_pbcm ();
};

/**
 * Confd Deamon
 *
 * A confd Deamon maps one to one with a confd_deamon structure. The UAGENT has
 * a single deamon which is used to register for data and action callbacks. Each
 * client that attaches through confd then creates a transaction within the
 * deamon.
 */

class ConfdDaemon
{
 public:
  /**
   * Constructor - with instance that owns this object
   */
  ConfdDaemon (Instance *instance);

  /**
   * Destructor - frees up any confd memory allocated
   */
  ~ConfdDaemon();

  /*!
   * Tries to establish connection with confd.
   * Retries until the connection gets establish
   * for which it has to wait till confd comes up.
   */
  void try_confd_connection();

  /// Connect to a confd server. Does the data provider registrations
  /// and sets up notification socket
  rw_status_t setup_confd_connection();

  /**
   * The setup function allocates a confd deamon object
   */

  rw_status_t setup();

    /// Setup a pool of worker sockets
  rw_status_t setup_confd_worker_pool();

  /// Registers for the supported Netconf Notification streams with Confd
  rw_status_t setup_notifications();

  /// assig a worker to a transaction
  int assign_confd_worker();

  /// transaction init callback for operational data
  int init_confd_trans(struct confd_trans_ctx *ctxt);

  /// transaction finish callback for operational data
  int finish_confd_trans(struct confd_trans_ctx *ctxt);

  /// initialize for an action point
  int init_confd_rpc(struct confd_user_info *ctxt);

  /// Event callback for worker sockets.
  static void cfcb_confd_worker_event(void* ud);

  /// Event callback for control socket.
  static void cfcb_confd_control_event(void* ud);

  /// process a data request from confd
  void process_confd_data_req (DispatchSrcContext *s);

  void clear_statistics();

  rwdts_member_rsp_code_t handle_notification(const ProtobufCMessage * msg);

  /// Utility routing that returns the notification context for the given
  /// notification yang node.
  confd_notification_ctx* get_notification_context(
                            const std::string& node_name,
                            const std::string& node_ns);

  // Called from SbReqGet
  rw_yang_netconf_op_status_t get_confd_daemon(XMLNode* node);

  // Called from SbReqGet
  rw_yang_netconf_op_status_t get_dom_refresh_period(XMLNode* node);

  /// Start Confd CDB upgrade
  void start_upgrade(size_t n_modules);

  /// Sends the notification to Confd via the pre-established notification
  /// context.
  rwdts_member_rsp_code_t send_notification_to_confd(
           confd_notification_ctx* notify_ctxt,
           rw_yang::ConfdTLVBuilder& builder, 
           struct xml_tag xtag);

  LogFileManager* confd_log() const noexcept {
    return confd_log_.get();
  }

  NbReqConfdConfig* confd_config() const noexcept {
    return confd_config_.get();
  }

  private:
  void async_execute_on_main_thread(dispatch_function_t task,
      void* user_data);

  rwsched_dispatch_queue_t get_dp_q(int index) const noexcept {
    return dp_q_pool_[index % dp_q_pool_.size()];
  }

 public:
  Instance *instance_;

  /// fd assigned for the confd control socket
  int control_fd_ = RWUAGENT_INVALID_SOCK_ID;

  /// The confd control port rwsched socket reference
  rwsched_dispatch_source_t control_sock_src_;

  /// confd "daemon" data
  struct confd_daemon_ctx *daemon_ctxt_ = nullptr;

  /// Mapping the notification stream name to confd notification context
  std::map<std::string, confd_notification_ctx*> notification_ctxt_map_;

  // List of data provider clients
  std::list<NbReqConfdDataProvider *> dp_clients_;

  std::list<std::unique_ptr<ConfdDomStats>> dom_stats_;

  ConfdUpgradeContext upgrade_ctxt_;

 private:
  // The data used by worker pool API. This implementation can change at any time.
  std::vector<int> worker_fd_vec_;

  // Pool of serial queues for dispatching confd
  // transations.
  // All transactions on a worker socket will go to
  // a particular serial queue from the pool.
  std::vector<rwsched_dispatch_queue_t> dp_q_pool_;

  // Confd transaction that is being handled currently
  struct confd_trans_ctx *ctxt_ = nullptr;

  // Last allocated confd worker index
  uint8_t last_alloced_index_ = 0;

  // Confd log manager
  std::unique_ptr<LogFileManager> confd_log_ = nullptr;

  // Confd configuration handler
  std::unique_ptr<NbReqConfdConfig> confd_config_ = nullptr;

  /// Number of attempts made to connect with Confd
  uint8_t confd_connection_attempts_ = 0;

protected:
  /// rwmemlog logging buffer
  RwMemlogBuffer memlog_buf_;

};

/*
 * Internal container class which owns the
 * XMLDocument pointer and maintains
 * a map of its underlying node pointers
 * to a counter
 */
struct NodeMap
{
public:
  using node_map = boost::bimaps::bimap<
                       boost::bimaps::unordered_set_of<uint32_t>,
                       boost::bimaps::unordered_set_of<const rw_yang::XMLNode*>
                   >;
  using node_value_type = node_map::value_type;

  friend struct OperationalDomMap;

public:
  NodeMap() = delete;
  NodeMap(const NodeMap& other) = delete;
  void operator=(const NodeMap& other) = delete;

  NodeMap(NodeMap&& other): doc_(std::move(other.doc_))
                                , node_counter_(other.node_counter_)
                                , node_map_(std::move(other.node_map_))
                                , keypath_(std::move(other.keypath_))
                                , creation_time_(clock_t::now())
  {}

  NodeMap(rw_yang::XMLDocument::uptr_t&& doc,
             const char* keypath): doc_(std::move(doc))
                                 , keypath_(keypath)
  {}

  // Get the counter value of the mapped node
  uint32_t get_counter(const rw_yang::XMLNode* node) const noexcept;

  rw_yang::XMLNode* get_node(uint32_t counter) const noexcept;
  rw_yang::XMLDocument* get_dom() const noexcept;

  // Add the node pointer to the node_map_
  // and returns the updated node_counter
  uint32_t add_pointer(const rw_yang::XMLNode* node);
  void add_keypath(const confd_hkeypath_t *keypath);

  bool is_keypath_present(const confd_hkeypath_t *keypath) const;
  bool is_keypath_present(const char* keypath) const;

  std::string debug_print();

private:
  bool node_exists(const rw_yang::XMLNode* node) const noexcept;
  bool node_exists(uint32_t counter) const noexcept;

private:
  rw_yang::XMLDocument::uptr_t doc_ = nullptr; // Owner
  // The counter that is passed down to
  // confd as next-pointer
  uint32_t node_counter_ = 0;
  node_map node_map_;
  // Base keypath associated with the
  // created DOM
  std::string keypath_;
  // DOM creation time
  clock_t::time_point creation_time_;
  // Keypaths being serviced from doc_
  std::unordered_set<std::string> keypath_set_;
};


/*
 * Maintains the map of currently
 * used dom pointers in a session.
 * Also keeps track of the keyspec
 * which is used and maps it to the
 * corresponding DOM pointer
 * It also has the logic of perfoming
 * cleanup/flushing of DOM object.
 */
struct OperationalDomMap
{
public:
  OperationalDomMap() = default;
  OperationalDomMap(const OperationalDomMap& other) = delete;
  void operator=(const OperationalDomMap& other) = delete;

  // Create a new DOM and add the mappings to the
  // corresponding maps maintained by this struct
  void add_dom(rw_yang::XMLDocument::uptr_t&& dom,
               const char* keypath);

  rw_yang::XMLDocument* dom(const confd_hkeypath_t *keypath) const;
  rw_yang::XMLDocument* dom(const char* keypath) const noexcept;

  boost::optional<NodeMap&> node(uint32_t dom_counter);
  boost::optional<NodeMap&> node(const confd_hkeypath_t *keypath);
  boost::optional<NodeMap&> node(const rw_yang::XMLDocument*);

  uint32_t dom_counter(const confd_hkeypath_t *keypath) const;
  uint32_t dom_counter(const rw_yang::XMLDocument*) const noexcept;

  rw_yang::XMLDocument* dom_with_keypath(const char* keypath) const;

  void remove_dom(const rw_yang::XMLDocument* doc);

  /// Removes the DOM associated with keypath if its expired.
  /// Returns 'true' if the DOM is removed
  /// Returns 'false' if DOM was not removed since its was not expired
  bool remove_dom_if_expired(const confd_hkeypath_t *keypath,
                             uint32_t expiry_period_ms);

  void cleanup_if_expired(rw_confd_client_type_t type, uint32_t cli_refresh_period,
                          uint32_t nc_rest_refresh_period);

  std::string debug_print();

private:
  rw_yang::XMLDocument* dom(uint32_t counter) const noexcept;

  uint32_t dom_counter(const char* keypath) const noexcept;

private:
  uint32_t dom_counter_ = 0;
  std::map<uint32_t, NodeMap> dom_map_;
  std::map<const rw_yang::XMLDocument*, uint32_t> dom_counter_map_;
  std::map<std::string, uint32_t> list_ctxt_map_;

  using dm_value_type  = std::map<uint32_t, NodeMap>::value_type;
  using lcm_value_type = std::map<std::string, uint32_t>::value_type;
  using dcm_value_type = std::map<const rw_yang::XMLDocument*, uint32_t>::value_type;
};


class NbReqConfdDataProvider
: public NbReq
{

 public:
  NbReqConfdDataProvider (ConfdDaemon *daemon, struct confd_trans_ctx *ctxt,
                          rwsched_dispatch_queue_t current_dispatch_q,
                          uint32_t cli_refresh_period, uint32_t nc_rest_refresh_period);
  ~NbReqConfdDataProvider();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

  rwsched_dispatch_queue_t get_execution_q() const override
  { return current_dispatch_q_; }    // Default Implementation

  /// transaction finish callback for operational data
  int finish_confd_trans(struct confd_trans_ctx *ctxt);


  /// get next processing
  int confd_get_next(struct confd_trans_ctx *tctx,
                     confd_hkeypath_t *keypath,
                     long next);

  /// get element processing
  int confd_get_elem(struct confd_trans_ctx *ctxt,
                     confd_hkeypath_t *keypath);

    /// Check presence of empty and presence containers
  int confd_get_exists(struct confd_trans_ctx *ctxt,
                       confd_hkeypath_t *keypath);


  /// get object processing
  int confd_get_object(struct confd_trans_ctx *ctxt,
                       confd_hkeypath_t *keypath);

  int confd_get_next_object(struct confd_trans_ctx *ctxt,
                            confd_hkeypath_t *keypath,
                            long next);


  /// Send a node to confd - for get_object
  int send_node_to_confd (rw_yang::XMLNode *node,
                          confd_hkeypath_t *keypath);

  /// Send a set of nodes to confd
  int send_multiple_nodes_to_confd (rw_yang::XMLNode *node,
                                    confd_hkeypath_t *keypath);

  /// get the case of an choice
  int confd_get_case(struct confd_trans_ctx *ctxt,
                     confd_hkeypath_t *keypath,
                     confd_value_t *choice);

  /// Check if the current operational DOM is sufficient to serve the request from confd
  bool is_dom_sufficient (confd_hkeypath_t* &keypath, bool do_cleanup = true);

  /// retreive operational data
  int retrieve_operational_data(struct confd_trans_ctx *ctxt,
                                confd_hkeypath_t *keypath);

  void log_state (RwMgmtagt_Confd_CallbackType type,
                  const char *hkey_path,
                  const char *dts_ks = nullptr);

 private:
  void start_dom_reclamation_timer();
  void stop_dom_reclamation_timer();
  void do_dom_cleanup(bool);

 public:
  ConfdDaemon *daemon_;

  /// The keypath that generated the current dom
  confd_hkeypath_t *keypath_ = nullptr;

  /// The state of the DOM currently
  rw_confd_dp_state_t state_;

  struct confd_trans_ctx *ctxt_;

  // statistics
  std::list<std::unique_ptr<ConfdDomStats>> dom_stats_;

  /// client type
  rw_confd_client_type_t type_ = INVALID;

  /// Indicator for cleanup
  bool cleanup_indicator_ = false;

  // Debug information
  rw_yang::XMLNode *node_ = nullptr;

 private:
  // Operational DOM Map
  OperationalDomMap op_dom_;

  rw_yang::XMLDocument* dom_ptr_ = nullptr;

  // DOM cleanup timer
  rwsched_dispatch_source_t dom_timer_ = nullptr;

  // Serial dispatch queue on which this DP is executing
  rwsched_dispatch_queue_t current_dispatch_q_;

  uint32_t cli_dom_refresh_period_msec_;
  uint32_t nc_rest_dom_refresh_period_msec_;

 private:
  static void cleanup_expired_dom_cb(
    void *user_context);

  static rw_confd_hkeypath_cmp_t hkeypath_cmp(
    const confd_hkeypath_t *hk1,
    const confd_hkeypath_t *hk2);

  int convert_node_to_confd(
    rw_yang::XMLNode *node,
    struct confd_cs_node *cs_node,
    confd_tag_value_t **array,
    bool multiple);
};


class NbReqConfdRpcExec
: public NbReq
{

 public:
  NbReqConfdRpcExec (ConfdDaemon *daemon, confd_user_info *ctxt);
  ~NbReqConfdRpcExec();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

  int set_timeout(int timeout_sec) override;

  /// execute a request
  int execute(
      struct xml_tag *name,         /**< Name given in annotation */
      confd_hkeypath_t *kp,         /**< Path to the action point - null for RPC */
      confd_tag_value_t *params,    /**< List of parameters */
      int nparams                 /**< Number of parameters */
  );

 public:
  ConfdDaemon *daemon_;

  struct confd_cs_node *rpc_root_;

  struct confd_user_info *ctxt_;
};


inline void
NbReqConfdDataProvider::do_dom_cleanup(bool do_cleanup)
{
  if (do_cleanup && cleanup_indicator_) {
    op_dom_.cleanup_if_expired(type_, cli_dom_refresh_period_msec_,
                               nc_rest_dom_refresh_period_msec_);
    cleanup_indicator_ = false;
    start_dom_reclamation_timer();
  }
}

inline uint32_t
NodeMap::get_counter(const rw_yang::XMLNode* node) const noexcept
{
  if (node_exists(node)) {
    return node_map_.right.at(node);
  }
  return 0;
}

inline rw_yang::XMLNode*
NodeMap::get_node(uint32_t counter) const noexcept
{
  if (node_exists(counter)) {
    return const_cast<rw_yang::XMLNode*>(node_map_.left.at(counter));
  }
  return nullptr;
}

inline uint32_t
NodeMap::add_pointer(const rw_yang::XMLNode* node)
{
  auto cnt = get_counter(node);
  if (cnt != 0 || node == nullptr) return cnt;
  node_map_.insert( node_value_type(++node_counter_, node) );
  return node_counter_;
}

inline void
NodeMap::add_keypath(const confd_hkeypath_t *keypath)
{
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof(confd_hkeypath_buffer) - 1, keypath);

  keypath_set_.emplace(confd_hkeypath_buffer);
}

inline bool
NodeMap::is_keypath_present(const confd_hkeypath_t *keypath) const
{
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof(confd_hkeypath_buffer) - 1, keypath);

  return is_keypath_present(confd_hkeypath_buffer);
}

inline bool
NodeMap::is_keypath_present(const char* keypath) const
{
  return keypath_set_.find(keypath) == keypath_set_.end()
    ? false : true;
}

inline rw_yang::XMLDocument*
NodeMap::get_dom() const noexcept
{
  return doc_.get();
}

inline bool
NodeMap::node_exists(const rw_yang::XMLNode* node) const noexcept
{
  return node_map_.right.find(node) ==
                node_map_.right.end() ? false : true;
}

inline bool
NodeMap::node_exists(uint32_t counter) const noexcept
{
  return node_map_.left.find(counter) ==
                node_map_.left.end() ? false : true;
}

inline boost::optional<NodeMap&>
OperationalDomMap::node(uint32_t dom_counter)
{
  auto it = dom_map_.find(dom_counter);
  if (it != dom_map_.end()) {
    return it->second;
  }
  return boost::none;
}

inline boost::optional<NodeMap&>
OperationalDomMap::node(const confd_hkeypath_t *keypath)
{
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof(confd_hkeypath_buffer) - 1, keypath);

  return node(dom_counter(confd_hkeypath_buffer));
}

inline boost::optional<NodeMap&>
OperationalDomMap::node(const rw_yang::XMLDocument* dom)
{
  return node(dom_counter(dom));
}

inline void
OperationalDomMap::remove_dom(const rw_yang::XMLDocument* doc)
{
  if (!doc) return;

  auto it = dom_counter_map_.find(doc);
  if (it == dom_counter_map_.end()) return;

  auto cnter = it->second;
  list_ctxt_map_.erase(node(doc).get().keypath_);
  dom_counter_map_.erase(doc);
  dom_map_.erase(cnter);
}

inline rw_yang::XMLDocument*
OperationalDomMap::dom(uint32_t counter) const noexcept
{
  auto it = dom_map_.find(counter);
  return it == dom_map_.end() ? nullptr : it->second.get_dom();
}

inline uint32_t
OperationalDomMap::dom_counter(const char* keypath) const noexcept
{
  if (nullptr == keypath) return 0;

  auto it = list_ctxt_map_.find(keypath);
  if (it == list_ctxt_map_.end() || !dom(it->second)) {
    return 0;
  }
  return it->second;
}

inline uint32_t
OperationalDomMap::dom_counter(const confd_hkeypath_t *keypath) const
{
  RW_ASSERT(keypath);
  char confd_hkeypath_buffer[RWUAGENT_HKEYPATH_DEBUG_BUFFER_LENGTH];
  confd_pp_kpath(confd_hkeypath_buffer, sizeof(confd_hkeypath_buffer) - 1, keypath);

  return dom_counter(confd_hkeypath_buffer);
}

inline uint32_t
OperationalDomMap::dom_counter(
                const rw_yang::XMLDocument* dom) const noexcept
{
  auto it = dom_counter_map_.find(dom);
  if (it == dom_counter_map_.end()) {
    return 0;
  }
  return it->second;
}

inline rw_yang::XMLDocument*
OperationalDomMap::dom_with_keypath(const char* keypath) const
{
  for (auto &delem : dom_map_) {
    if (delem.second.is_keypath_present(keypath)) {
      return delem.second.doc_.get();
    }
  }
  return nullptr;
}

} // end namespace rwuagent

#endif // CORE_MGMT_RWUAGENT_CONFD_HPP_
