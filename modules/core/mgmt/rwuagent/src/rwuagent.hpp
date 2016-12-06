
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


/*!
 * @file rwuagent.hpp
 * @brief Private micro-agent header file
 */

#ifndef CORE_MGMT_RWUAGENT_HPP_
#define CORE_MGMT_RWUAGENT_HPP_

#include <array>
#include <chrono>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/optional.hpp>

#include <rw-manifest.pb-c.h>
#include <rw-mgmt-schema.pb-c.h>
#include <rw-mgmtagt-dts.pb-c.h>
#include <rw-mgmtagt-log.pb-c.h>
#include <rw-mgmtagt.pb-c.h>
#include <rw_app_data.hpp>
#include <rw_xml.h>
#include <rwdts.h>
#include <rwmemlog.h>
#include <rwmemlog_mgmt.h>
#include <rwmemlogdts.h>
#include <rwmsg.h>
#include <rwha_dts_api.h>
#include <rwyangutil_argument_parser.hpp>
#include <yangncx.hpp>

#include "async_file_writer.hpp"
#include "queue_manager.hpp"
#include "rwuagent.h"
#include "rwuagent_dynamic_schema.hpp"
#include "rwuagent_request_mode.hpp"


namespace rw_uagent {

extern const char *uagent_yang_ns;
extern const char *uagent_dts_yang_ns;
extern const char *uagent_confd_yang_ns;

const int RWUAGENT_MAX_CMD_STATS = 10;
const int RWUAGENT_KS_DEBUG_BUFFER_LENGTH = 1024;
const int RWUAGENT_INVALID_SOCK_ID = -1;
const int RWUAGENT_NOTIFICATION_TIMEOUT = 10;

// DOM cleanup timer related constants
static const int RWUAGENT_DOM_CLI_TIMER_PERIOD_MSEC = 120000;
static const int RWUAGENT_DOM_NC_REST_TIMER_PERIOD_MSEC = 250;
static const uint64_t CRITICAL_TASKLETS_WAIT_TIME = NSEC_PER_SEC * 60 * 7; // 7 Mins

constexpr const uint32_t RWUAGENT_SCHEMA_MAX_VAL = std::numeric_limits<uint32_t>::max();

constexpr const int RWUAGENT_MAX_HOSTNAME_SZ = 64;



class MsgClient;
class NbReq;
class NbReqDts;
class SbReq;
class SbReqEditConfig;
class SbReqGet;
class SbReqRpc;
class DtsMember;
class Instance;
class BaseMgmtSystem;
class LogFileManager;

/// YangNode app data
struct northbound_dts_registrations_app_data {
  std::string module_name;
  rwdts_member_reg_handle_t registration;
  bool propagated;
  northbound_dts_registrations_app_data(std::string name,
                                        rwdts_member_reg_handle_t reg)
      : module_name(name),
        registration(reg),
        propagated(false)
  {}
};

typedef rw_yang::AppDataToken<northbound_dts_registrations_app_data *> adt_northbound_dts_registrations_t;

constexpr const char * rwdynschema_app_data_namespace = "rwdynschema";
constexpr const char * rwdynschema_dts_registrations_extension = "dts_registrations";

/// NbReq token value. No local meaning.
typedef intptr_t token_t;
typedef std::chrono::high_resolution_clock clock_t;

enum class StartStatus {
  Async = 0x5748,
  Done,
};

enum AgentHAMode {
  HA_MASTER = 0,
  HA_SLAVE,
};

enum HAEventType {
  STANDBY_TO_MASTER_TRANSITION = 0,
  CHANGE_MASTER,
};

typedef enum dom_stats_state_t {
  RW_UAGENT_NETCONF_REQ_RVCVD,
  RW_UAGENT_NETCONF_PARSE_REQUEST,
  RW_UAGENT_NETCONF_DTS_XACT_START,
  RW_UAGENT_NETCONF_DTS_XACT_DONE,
  RW_UAGENT_NETCONF_RESPONSE_BUILT
} dom_stats_state_t;

typedef RWPB_E(RwNetconf_ErrorType) RwNetconf_ErrorType;
typedef RWPB_E(IetfNetconf_ErrorTagType) IetfNetconf_ErrorTagType;
typedef RWPB_E(IetfNetconf_ErrorSeverityType) IetfNetconf_ErrorSeverityType;
typedef RWPB_E(RwMgmtagt_SbReqType) RwMgmtagt_SbReqType;
typedef RWPB_E(RwMgmtagt_NbReqType) RwMgmtagt_NbReqType;
typedef RWPB_E(RwMgmtSchema_ApplicationState) RwMgmtSchema_ApplicationState;
typedef RWPB_E(RwManifest_RwmgmtAgentMode) RwmgmtAgentMode;

typedef RWPB_T_MSG(RwMgmtagt_data_Uagent_LastError_RpcError) IetfNetconfErrorReply;
typedef RWPB_T_MSG(RwMgmtagtDts_input_MgmtAgentDts_PbRequest) RwMgmtagtDts_PbRequest;
typedef RWPB_T_MSG(RwMgmtagtDts_output_MgmtAgentDts_PbRequest) RwMgmtagtDts_PbResponse;

typedef UniquePtrProtobufCMessage<>::uptr_t pbcm_uptr_t;
typedef UniquePtrKeySpecPath::uptr_t ks_uptr_t;

typedef std::pair<rw_keyspec_path_t*, ProtobufCMessage *> BlockContents;


/*
 * Logging Macros for UAGENT
 */
#define RW_MA_Paste3(a,b,c) a##_##b##_##c

#define RW_MA_INST_LOG( instance_, evvtt, ... ) \
  RW_MA_INST_LOG_Step1( \
    instance_, \
    RW_MA_Paste3(RwMgmtagtLog, \
    notif, \
    evvtt), \
    __VA_ARGS__ )

#define RW_MA_INST_LOG_Step1(instance_, evvtt, ... ) \
  RWLOG_EVENT( \
    (instance_)->rwlog(), \
    evvtt, \
    (uint64_t)instance_, \
    __VA_ARGS__)

#define RW_MA_NBREQ_LOG( nbreq_, evvtt, ... ) \
  RW_MA_INST_NBREQ_LOG_Step1( \
    nbreq_, \
    RW_MA_Paste3(RwMgmtagtLog, \
    notif, \
    evvtt), \
    __VA_ARGS__ )

#define RW_MA_INST_NBREQ_LOG_Step1( nbreq_, evvtt, ... ) \
  RWLOG_EVENT( \
    nbreq_->instance()->rwlog(), \
    evvtt, \
    nbreq_->nbreq_type(), \
    (uint64_t) nbreq_, \
    __VA_ARGS__ )

#define RW_MA_NBREQ_LOG_FD( nbreq_, ... ) \
  RWLOG_EVENT( \
    nbreq_->instance()->rwlog(), \
    RwMgmtagtLog_notif_ClientFdLog, \
    nbreq_->nbreq_type(), \
    (uint64_t) nbreq_, \
    __VA_ARGS__ )

#define RW_MA_INST_DETAIL( instance_, ... ) \
  RWLOG_EVENT( \
    (instance_)->rwlog(), \
    RwMgmtagtLog_notif_InstanceDebugDetailed, \
    (uint64_t)instance_, \
    __VA_ARGS__ )

#define RW_MA_SBREQ_LOG( sbreq_, ... ) \
  RWLOG_EVENT( \
    (sbreq_->nbreq_->instance())->rwlog(), \
    RwMgmtagtLog_notif_ClientSbReqDebug, \
    sbreq_->sbreq_type(), \
    (uint64_t)sbreq_, \
    sbreq_->nbreq_->nbreq_type(), \
    (uint64_t) sbreq_->nbreq_, \
    __VA_ARGS__ )

#define RW_MA_DOMMGR_LOG( instance_, evvtt, ... ) \
  RW_MA_DOMMGR_LOG_Step1( \
    (instance_), \
    RW_MA_Paste3(RwMgmtagtLog, \
    notif, \
    evvtt), \
    __VA_ARGS__ )

#define RW_MA_DOMMGR_LOG_Step1( instance_, evvtt, ... ) \
  RWLOG_EVENT( \
    (instance_)->rwlog(), \
    evvtt, \
    __VA_ARGS__ )

/*!
 * Information about the supported Netconf Notification stream
 */
typedef struct {
  std::string name;        ///< Name of the event stream
  std::string description; ///< Event stream description
  bool replay_support;     ///< supports replay or not
} netconf_stream_info_t;

/*!
 * Utility class to tie yang node name and namespace together, so that together
 * it can be used as a key. Used in Instance::notification_stream_map_.
 */
class NotificationYangNode {
 public:
  /*!
   * Constructor for NotificationYangNode
   * @param [in] name   Yang Node name
   * @param [in] ns     Yang Node's namespace
   */
  NotificationYangNode(const std::string& name, const std::string& ns)
    : name_(name),
      ns_(ns)
  {
  }

  /*!
   * Comparison operator required by std::map to perform key comparisons
   */
  bool operator < (const NotificationYangNode& other) const {
    int res = name_.compare(other.name_);
    if (res < 0) {
      return true;
    }
    if (res > 0) {
      return false;
    }
    return ns_ < other.ns_;
  }

  std::string name_;   ///< Yang Node local name
  std::string ns_;     ///< Yang Node's namespace
};

static inline
std::string get_rift_install()
{
  auto rift_install = getenv("RIFT_INSTALL");
  if (!rift_install) return "/home/rift/.install";
  return rift_install;
}

static inline
std::string get_rift_var_root()
{
  auto rift_var_root = getenv("RIFT_VAR_ROOT");
  if (!rift_var_root) return "/home/rift/.install";
  return rift_var_root;
}

/*!
 * Predicate to check if the agent is running
 * in production or not.
 */
static inline
bool is_production()
{
  auto rift_prod_mode = getenv("RIFT_PRODUCTION_MODE");
  if (rift_prod_mode) return true;

  std::array<const char*, 3> probable_vals {
    "/", "/home/rift", "/home/rift/.install"
  };
  auto it = std::find(probable_vals.begin(), probable_vals.end(), get_rift_install());

  return it != probable_vals.end();
}

void rw_management_agent_xml_log_cb(void *user_data,
                                    rw_xml_log_level_e level,
                                    const char *fn,
                                    const char *log_msg);


/*!
 * Encapsulation for 'rcp-error' element specified in the Netconf RFC.
 * There can be mutliple NetconfError's sent in a single error response.
 */
class NetconfError
{
public:
  NetconfError(
    RwNetconf_ErrorType errType = RW_NETCONF_ERROR_TYPE_APPLICATION,
    IetfNetconf_ErrorTagType errTag = IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED
  );
  ~NetconfError();

  // non-copyable and non-assignable
  NetconfError(const NetconfError&) = delete;
  void operator=(const NetconfError&) = delete;

  /*!
   * Defines the conceptual layer where the error occurred.
   * Possible values are transport, rpc, protocol, application
   */
  NetconfError& set_type(RwNetconf_ErrorType type);

  /*!
   * Contains the enum identifying the error condition. Possible values are
   * provided in Netconf RFC 6241 Appendix A.
   */
  NetconfError& set_tag(IetfNetconf_ErrorTagType tag);

  /*!
   * Get the IETF Netconf error tag
   */
  IetfNetconf_ErrorTagType get_tag() const;

  /*!
   * Converts the RW netconf status (which is also based on Netconf error_tag)
   * to Netconf error_tag value.
   */
  NetconfError& set_rw_error_tag(rw_yang_netconf_op_status_t status);

  /*!
   * Converts the RW status to Netconf error_tag value.
   */
  NetconfError& set_rw_error_tag(rw_status_t rwstatus);

  /*!
   * Converts the error tag value to RW netconf status.
   */
  rw_yang_netconf_op_status_t get_rw_nc_error_tag() const;

  /*!
   * Identifies the error severity. Possible values are error and warning.
   * Not currently used. Reserved for future use.
   */
  NetconfError& set_severity(IetfNetconf_ErrorSeverityType severity);

  /*!
   * Contains a string identifying the data-model-specific or
   * implementation-specific error condition, if one exists.
   */
  NetconfError& set_app_tag(const char* app_tag);

  /*!
   * Get the application tag
   */
  const char* get_app_tag() const;

  /*!
   * Contains the absolute XPath expression identifying the element path to
   * the node that is associated with the error.
   */
  NetconfError& set_error_path(const char* err_path);

  /*!
   * Get the xpath for the error
   */
  const char* get_error_path() const;

  /*!
   * Contains a string suitable for human display that describes the error
   * condition.
   */
  NetconfError& set_error_message(const char* err_msg);


  NetconfError& set_errno(const char* sysc);

  /*!
   * Get the message for the error
   */
  const char* get_error_message() const;

  /*!
   * Contains protocol- or data-model-specific error content in XML format.
   */
  NetconfError& set_error_info(const char* err_info);

  // Used internally by the NetconfErrorList while forming XML
  IetfNetconfErrorReply* get_pb_error();

private:

  /*!
   * Yang model defined protobuf object for 'rpc-error' element
   */
  IetfNetconfErrorReply error_;
};

/*!
 * Encapsulation for the list of 'rpc-error' elements
 *
 * Sample usage:
 * <code>
 *   NetconfErrorList nc_err_list(2); // two errors to be reported
 *   nc_err_list.add_error()
 *     .set_type(RW_YANG_TYPES_ERROR_TYPE_PROTOCOL)
 *     .set_tag(RW_YANG_TYPES_ERROR_TAG_MISSING_ATTRIBUTE)
 *     .set_error_message("Attribute Message-Id missing");
 *
 *   nc_err_list.add_error()
 *     .set_type(RW_YANG_TYPES_ERROR_TYPE_APPLICATION)
 *     .set_tag(RW_YANG_TYPES_ERROR_TAG_ACCESS_DENIED)
 *     .set_error_message("Access to the specified resource denied");
 * </code>
 */
class NetconfErrorList
{
public:
  /*!
   * Initializes a NetconfError list.
   */
  NetconfErrorList();

  /*!
   * Add an error to the error list.
   */
  NetconfError& add_error(
    RwNetconf_ErrorType errType = RW_NETCONF_ERROR_TYPE_APPLICATION,
    IetfNetconf_ErrorTagType errTag = IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED
  );

  /*!
   * Return the number of errors in the list.
   */
  size_t length() const
  {
    return errors_.size();
  }

  /*!
   * Access error entries
   */
  const std::list<NetconfError>& get_errors() const;

  /*!
   * Converts the rpc-error protocol buffer to XML format.
   */
  bool to_xml(rw_yang::XMLManager* xml_mgr, std::string& xml);

private:

  /*!
   * An array of NetconfError's that will be sent with the error response.
   */
  std::list<NetconfError> errors_;
};

// The command and its stats in a tuple
typedef struct CommandStat {
  std::string  request;
  RWPB_T_MSG(RwMgmtagt_SpecificStatistics_ProcessingTimes) statistics;
} CommandStat;

typedef std::list<CommandStat> CommandStats;

typedef struct OperationalStats { //  OperationalStats
  struct timeval                 start_time_;
  RWPB_T_MSG(RwMgmtagt_Statistics)     statistics;
  CommandStats                        commands;
} OperationalStats;

}


// ATTN: This is ugly, unwind the dependencies some more!
#include "rwuagent_nb_req.hpp"
#include "rwuagent_sb_req.hpp"
#include "rwuagent_mgmt_system.hpp"


namespace rw_uagent {

/*!
 * Dts instance.
 */
class DtsMember
{
 public:
  /*!
   * Construct the DTS member API.
   */
  DtsMember(
    Instance* instance );

  /*!
   * Destroy the DTS member API.
   */
  ~DtsMember();

 private:
  // cannot copy
  DtsMember(const DtsMember&) = delete;
  DtsMember& operator=(const DtsMember&) = delete;

 public:
  /*!
   * Get the memlog buffer for DtsMember.
   */
  rwmemlog_buffer_t** get_memlog_ptr()
  {
    return &memlog_buf_;
  }

  /*!
   * Return the DTS handle.
   */
  rwdts_api_t* api() const
  {
    return api_;
  }

  /*!
   * Get flags for a transaction.
   */
  RWDtsXactFlag get_flags();

  /*!
   * Check if DTS is ready for queries.
   */
  bool is_ready() const;

  /*!
   * Trace next DTS transaction.
   */
  void trace_next();

  /*!
   * Register the RPC callback.
   */
  void register_rpc();

  /*!
   * Registers the nodes defined for the provided model
   * It's only performed when Agent is working as a Master
   * in cluster mode.
   */
  void load_registrations(rw_yang::YangModel* model);

  void do_manifest_registration();

  /*!
   * Remove all existing registrations
   */
  void remove_registrations();

  /*!
   * Publish the state of management agent.
   * By default, sends the status as true indicating
   * that management agent is ready to accept requests.
   */
  void publish_config_state(bool state = true);

  // Publishes the heartbeat state node
  // Added for debugging purpose only
  void publish_internal_hb_event();

private:
  static void schema_registration_ready_cb(rwdts_member_reg_handle_t  regh,
                                            rw_status_t                reg_state,
                                            void*                      user_data);

  static void state_change_cb(
    rwdts_api_t* apih,
    rwdts_state_t state,
    void* ud );

  static void config_state_cb(
    rwdts_xact_t* xact,
    rwdts_xact_status_t* xact_status,
    void* ud);

  static void retry_config_state_cb(void* ctx);

  void state_change(
    rwdts_state_t state );

  void dts_state_init();

  void dts_state_config();

  void register_agent_config_state();

  static rwdts_member_rsp_code_t notification_recv_cb(
      const rwdts_xact_info_t * xact_info,
      RWDtsQueryAction action,
      const rw_keyspec_path_t * keyspec,
      const ProtobufCMessage * msg,
      uint32_t credits,
      void * get_next_key);

  static rwdts_member_rsp_code_t get_config_cb(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg,
    uint32_t credits,
    void* getnext_ptr );

  rwdts_member_rsp_code_t get_config(
    rwdts_xact_t* xact,
    rwdts_query_handle_t qhdl,
    const rw_keyspec_path_t* ks );

  static rwdts_member_rsp_code_t rpc_cb(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks_in,
    const ProtobufCMessage* msg,
    uint32_t credits,
    void* getnext_ptr );

  rwdts_member_rsp_code_t rpc(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks_in,
    const ProtobufCMessage* msg );

  static rwdts_member_rsp_code_t config_state_prepare_cb(
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks_in,
    const ProtobufCMessage* msg,
    uint32_t credits,
    void* getnext_ptr);

  rwdts_member_rsp_code_t config_state_respond(
    rwdts_xact_t* xact,
    rwdts_query_handle_t qhdl,
    const rw_keyspec_path_t* ks );

  // Functions related to internal hearbeat

  // Registers the heartbeat node as both publisher
  // and subscriber
  void register_internal_heartbeat();

  // Publishes the heartbeat state node
  static void retry_publish_internal_hb_event_cb(void* ctx);

  static void internal_hb_publish_cb(
      rwdts_xact_t* xact,
      rwdts_xact_status_t* xact_status,
      void* ud);

  // Performs the required action (logging in this case)
  // on receiving a published event
  static rwdts_member_rsp_code_t internal_hb_subscription_cb(
      const rwdts_xact_info_t * xact_info,
      RWDtsQueryAction action,
      const rw_keyspec_path_t * keyspec,
      const ProtobufCMessage * msg,
      uint32_t credits,
      void * get_next_key);

private:

  //! The owning uAgent.
  Instance* instance_ = nullptr;

  //! rwmemlog logging buffer
  RwMemlogBuffer memlog_buf_;

  //! DTS API handle
  rwdts_api_t* api_ = nullptr;

  //! Has registration with DTS completed?
  bool ready_ = false;

  //! A flag to be set on the next DTS transaction
  RWDtsXactFlag flags_ = RWDTS_XACT_FLAG_NONE;

  //! List of DTS registrations (config and notification)
  std::list<rwdts_member_reg_handle_t> all_dts_regs_;

  /// App data token for dts registrations on northbound config elements
  adt_northbound_dts_registrations_t dts_registrations_token_;
};


/*!
 * An internal messaging based client.  All messaging-based clients
 * funnel through a single instance of this class.
 */
class MsgClient
{
public:
  MsgClient(Instance* instance);
  ~MsgClient();

  // cannot copy
  MsgClient(const MsgClient&) = delete;
  MsgClient& operator=(const MsgClient&) = delete;

public:

  /*!
   * rwmsg callback for RwUAgent.netconf_request
   */
  static void reqcb_netconf_request(
    /// [in] service descriptor
    RwUAgent_Service* srv,
    /// [in] the request to forward
    const NetconfReq* req,
    /// [in] Instance pointer
    void* vinstance,
    /// [in] the request closure, indicates how to respond
    NetconfRsp_Closure rsp_closure,
    /// [in] the closure data, mus hand back to rwmsg
    void* closure_data
  );

  /// Received a forward request from messaging.
  void netconf_request(
    /// [in] The request
    const NetconfReq* req,
    /// [in] The rwmsg closure
    NetconfRsp_Closure rsp_closure,
    /// [in] The rwmsg closure data
    void* closure_data
  );

private:

  //! The owning uAgent.
  Instance* instance_ = nullptr;

  //! rwmemlog logging buffer
  RwMemlogBuffer memlog_buf_;

  /// The messaging endpoint for the uAgent
  rwmsg_endpoint_t *endpoint_ = nullptr;

  /// The server channel (and, effectively, the execution context)
  rwmsg_srvchan_t *srvchan_ = nullptr;

  /// The uAgent service descriptor
  RwUAgent_Service rwua_srv_;
};


/*!
 * RW.uAgent instance.  Responsible for:
 *  - holding tasklet instance data
 *  - holding messaging server and endpoint data
 *  - accepting new connections (thus creating a NbReq)
 *  - instantiating a MsgClient (thus creating the one-and-only
 *    internal-massaging based client)
 *  - managing Requests on behalf of Clients
 * - statistics
 *  - ATTN: maintaining a DOM
 *  - ATTN: responding directly to DOM-get requests
 */
class Instance : public AsyncFileWriterFactory,
                 public QueueManager
{
public:
  Instance(rwuagent_instance_t rwuai);
  ~Instance();

  // cannot copy
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

public:
  /// Return the rwmemlog instance so child objects can allocate their own buffers
  rwmemlog_instance_t* get_memlog_inst();
  /// Return the rwmemlog buffer so callbacks can log with the instance's buffer
  rwmemlog_buffer_t* get_memlog_buf();
  rwmemlog_buffer_t** get_memlog_ptr();
  RequestMode get_request_mode();

public:
  /// Get the tasklet info instance
  rwtasklet_info_ptr_t rwtasklet();

  /// Get the rwsched instance
  rwsched_instance_ptr_t rwsched();

  /// Get the rwsched instance
  rwsched_tasklet_ptr_t rwsched_tasklet();

  /// Get the rwtrace instance
  rwtrace_ctx_t* rwtrace();

  /// Get the rwlog instance
  rwlog_ctx_t* rwlog();

  /// Get the rwvcs instance
  struct rwvcs_instance_s* rwvcs();

  //! Get the DTS API handle.
  DtsMember* dts() const noexcept
  {
    return dts_.get();
  }

  //! Get the DTS API handle.
  rwdts_api_t* dts_api() const noexcept
  {
    auto api = dts_->api();
    RW_ASSERT(api);
    return api;
  }

  //! Get the current schema.
  const rw_yang_pb_schema_t* ypbc_schema() const noexcept
  {
    return ypbc_schema_;
  }

  //! Get the XML manager.
  rw_yang::XMLManager* xml_mgr() const noexcept
  {
    return xml_mgr_.get();
  }

  //! Get the yang model.
  rw_yang::YangModel* yang_model() const noexcept
  {
    return xml_mgr_->get_yang_model();
  }

  // ATTN: This needs to go into another object?
  //! Get the configuration DOM.
  rw_yang::XMLDocument* dom() const noexcept
  {
    return dom_.get();
  }

  void reset_dom(rw_yang::XMLDocument::uptr_t new_dom)
  {
    dom_ = std::move(new_dom);
  }

  //! Determine if the DTS member API is ready for queries.
  bool dts_ready() const noexcept
  {
    return dts_ && dts_->is_ready();
  }
  BaseMgmtSystem* mgmt_handler() const noexcept
  {
    return mgmt_handler_.get();
  }

  AgentHAMode ha_mode() const noexcept {
    return ha_mode_;
  }

  bool is_ha_master() const noexcept {
    return ha_mode() == HA_MASTER;
  }

  void update_ha_mode(AgentHAMode new_mode) noexcept {
    ha_mode_ = new_mode;
  }

  /*!
   * Start the instance. All the startup initializations
   * are asynchronously dispatched to async_start
   * function.
   */
  void start();

  /*!
   * Asynchronously invoked by the start() member
   * function. Creates messaging endpoints, DTS
   * registrations, confd connection, et cetera...
   */
  static void async_start(void* ctxt);

  static void async_start_dts(void* ctxt);

  static void async_start_mgmt_system(void* ctxt);

  /*!
   * Called from DTS when all the critical tasklets
   * are ready.
   */
  static void tasks_ready_cb(void* ctx);

  /*!
   * Waits for the critical tasklets to come into Running
   * state before proceeding with the startup state machine.
   * It waits for CRITICAL_TASKLETS_WAIT_TIME mins(default)
   * before getting timed out on waiting for critical tasklets.
   */
  rw_status_t wait_for_critical_tasklets();

  /*!
   * Subscribe for notification from VCS about VM-state
   * changes.
   */
  void subscribe_for_vm_state_notification();

  static void agent_ha_state_change_cb(
      void* ud, vcs_vm_state prev_state, vcs_vm_state new_state);

  /*!
   * These functions are used to enqueue and
   * dequeue PB requests to serialize their processing.
   * On calling enqueue, the pb-request will be appended to the 
   * queue and the request at the start of the queue would be
   * dispatched for processing.
   * On calling dequeue, the entry from the front will be removed
   * and if their are further entries in the queue, they would
   * be processed in similar manner.
   */
  void enqueue_pb_request(NbReqInternal*);
  void dequeue_pb_request();
  static void dispatch_next_pb_req(void* ud);

  /*!
   * Register the nodes from the new schema
   * after dynamic schema loading
   */
  static void dyn_schema_dts_registration(
            void *ctxt);

  /// Initialize the rwmsg service servers
  void start_messaging_client();

  /// setup the DOM from the modules
  void setup_dom(const char *module_name);

  /// Reloads the dom with the new module included
  rw_yang::YangModule* load_module(const char *module_name);

  rw_status_t handle_dynamic_schema_update(const int batch_size,
                                           rwdynschema_module_t * modules);

  /// Schedule the module loading the appropriate queue
  void perform_dynamic_schema_update();

  /// Load and merge all the pending schema modules
  static void perform_dynamic_schema_update_load_modules(void * context);

  /// Schedule the dts registrations on the appropriate queue
  static void perform_dynamic_schema_update_end(void * context);

  /// Returns true if we've gotten a reg_ready callback for every module we're registered for
  bool registrations_propagated();

  /// reg_ready dts callback for top-level module registrations
  void schema_registration_ready(rwdts_member_reg_handle_t regh);

  // Update dynamic schema loading state
  void update_dyn_state(RwMgmtSchema_ApplicationState state,
                        const std::string& str);

  // Update dynamic schema loading state with err string set to ' ' (space)
  void update_dyn_state(RwMgmtSchema_ApplicationState state);

  /*!
   * Update uagent statistics
   */
  void update_stats (RwMgmtagt_SbReqType type,
                     const char *req,
                     RWPB_T_MSG(RwMgmtagt_SpecificStatistics_ProcessingTimes) *sbreq_stats);

  /*!
   * Used to ask the instance if a given module is part of the exported northbound schema.
   * @param module_name the name of the module to be checked
   * @return true if the module with the given name is exported
   */
  bool module_is_exported(std::string const & module_name);

  /*!
   * Given the Yang name and namspace, this method fetches the notification
   * stream.
   * @param [in] node_name   Yang node name
   * @param [in] node_ns     Yang node namespace 
   * @returns The notifications stream on successful match. Otherwise empty
   * string.
   */
  std::string lookup_notification_stream(
                    const std::string& node_name,
                    const std::string& node_ns);

private:
  rw_status_t fill_in_confd_info();
  bool parse_cmd_args(const rwyangutil::ArgumentParser&);

  /*!
   * Timer functions for ticking wait for
   * critical tasklets callback.
   */
  static void tasks_ready_timer_expire_cb(void* ctx);
  void start_tasks_ready_timer();
  void tasks_ready_timer_expire();
  void stop_tasks_ready_timer();

  /*!
   * Called once the criticals tasklets are ready
   * OR when the timer gets timed out waiting for the
   * critical tasklets to come up.
   */
  virtual void tasks_ready();

  /*!
     Attempt to start management instance.
   */
  virtual void try_start_mgmt_instance();
  static void try_start_mgmt_instance_cb(void* ctx);


private:
  static const size_t MEMORY_LOGGER_INITIAL_POOL_SIZE = 12ul;
  RwMemlogInstance memlog_inst_;
  RwMemlogBuffer memlog_buf_;

  AgentHAMode ha_mode_ = HA_SLAVE;

  // Critical tasks timer
  rwsched_dispatch_source_t tasks_ready_timer_ = nullptr;

public: // ATTN: private

  // ATTN: Is this really needed?  It is in tasklet
  std::string instance_name_;

  // ATTN: Why public?
  /// Dynamic schema loading state
  struct schema_state {
    RwMgmtSchema_ApplicationState state;
    std::string err_string;
  };

  schema_state schema_state_{RW_MGMT_SCHEMA_APPLICATION_STATE_INITIALIZING, " "};

  /// The plugin component and instance
  rwuagent_instance_t rwuai_;

public: // ATTN: private
  /// The Confd Port to connect to
  in_port_t confd_port_ = 0;

  /// Confd IPC Unix domain socket
  std::string confd_unix_socket_;

  struct sockaddr_in confd_inet_addr_;

  struct sockaddr *confd_addr_ = nullptr;
  size_t confd_addr_size_ = 0;

  // schema which needs to be loaded
  std::string yang_schema_;


  // Each type has its own stats, and CommandStats
  typedef std::unique_ptr<OperationalStats> op_stats_uptr_t;
  std::array <op_stats_uptr_t, RW_MGMTAGT_SB_REQ_TYPE_MAXIMUM> statistics_;

  /// Stores the last error encountered by uagent and returns it when requested
  std::string last_error_;

  bool non_persist_ws_ = false;

  ///
  uint32_t cli_dom_refresh_period_msec_ = RWUAGENT_DOM_CLI_TIMER_PERIOD_MSEC;
  uint32_t nc_rest_refresh_period_msec_ = RWUAGENT_DOM_NC_REST_TIMER_PERIOD_MSEC;

  // ATTN: Belongs in driver?
  /// List of modules which needs to be loaded for dynamic
  // schema update.
  typedef struct {
    std::string module_name;
    std::string so_filename;
    bool exported = false;
  } module_details_t;

  std::list<module_details_t> pending_schema_modules_;

  std::atomic<bool> initializing_composite_schema_;

  /// Contains the list of supported Netconf streams by UAgent
  std::vector<netconf_stream_info_t> netconf_streams_;

  /// YangNode name+ns to the notification-stream mapping
  std::map<NotificationYangNode, std::string> notification_stream_map_;

private:

  /// XML document factory manager
  rw_yang::XMLManager::uptr_t xml_mgr_;

  /// The configuration DOM
  rw_yang::XMLDocument::uptr_t dom_;

  const rw_yang_pb_schema_t* ypbc_schema_ = nullptr;

  /// DTS API manager
  std::unique_ptr<DtsMember> dts_ = nullptr;

  /// Messaging client
  std::unique_ptr<MsgClient> msg_client_ = nullptr;

  std::unique_ptr<DynamicSchemaDriver> schema_driver_ = nullptr;

  /// Northbound management system interface.
  std::unique_ptr<BaseMgmtSystem> mgmt_handler_;

  /// A list used to hold temporary model objects
  std::vector<std::unique_ptr<rw_yang::YangModelNcx>> tmp_models_;

 public:
  /// Callback to execute after finishing perform_dynamic_schema_update_load_modules
  void(*after_modules_loaded_cb_)(void*) = nullptr;

  /// Flag to say wether all the modules were loaded or not
  bool loading_modules_success_ = true;

  /// A vector to hold the module app data
  /// set should only be modified from the main dispatch queue
  std::vector<northbound_dts_registrations_app_data> exported_modules_app_data_;
  using exported_modules_app_data_itr_t =  std::vector<northbound_dts_registrations_app_data>::iterator;
 private:
  /// A set to hold the module names that are exported
  /// This set should only be modified from the main dispatch queue
  std::set<std::string> exported_modules_;

  /// The uagent mode confd or xml
  RequestMode request_mode_;

  /// Queue to serialize PB requests
  std::deque<NbReqInternal*> pb_req_q_;

 public:
  LogFileManager * log_file_manager() {
    return mgmt_handler()->log_file_manager();
  }

  // AsyncFileWriterFactory
  std::unique_ptr<AsyncFileWriter> create_async_file_writer(std::string const & name) override;

  // QueueManager
 private:
  void create_queue(QueueType const & type) override;

 public:
  void release_queue(QueueType const & type) override;
  rwsched_dispatch_queue_t get_queue(QueueType const & type) override;
};

/*!
 * ProtoSplitter is a XMLVisitor object that checks whether a node is at a level
 * of application subscription, and if it is, adds it to the a transaction as
 * a query.
 *
 * The ProtoSplitter provides an intermediate solution on applying a DOM of
 * configuration changes to Riftware components. The components are currently
 * implemented to process a CLI line worth of changes in one message.
 *
 * The eventual solution will be similar, the only difference being that the
 * nodes at which the split is done will be the highest node at which an
 * application has registered.
 *
 * This might have been better fit in the XML files, but dts files cannot be
 * accessed due to dependency issues.Also, this functionality is required only
 * in the management agent
 */

class ProtoSplitter
    :public rw_yang::XMLVisitor
{
 private:
  /// The edit config SbReq instance which instantiated
  // this class.
  SbReqEditConfig* edit_cfg_ = nullptr;
  
  /// The transaction to which protos are to be added
  rwdts_xact_t* xact_ = nullptr;
  /// Atleast one message was created
  bool created_block_ = false;
  /// A flag to be set on the next DTS transaction
  RWDtsXactFlag dts_flags_ = RWDTS_XACT_FLAG_NONE;

  rw_yang::XMLNode* root_node_ = nullptr;

  /// The collection of created keyspecs and protobufs
  std::list<BlockContents> transaction_contents_;

  /// The used to allow outside ownership of the contents
  std::list<BlockContents> & block_collector_;

  /// True if the ProtoSplitter owns the transaction contents
  bool own_contents_ = true;

  /// If true create and execute the dts block queries.
  bool create_blocks_ = true;

  // Cannot copy
  ProtoSplitter(const ProtoSplitter&) = delete;
  ProtoSplitter& operator=(const ProtoSplitter&) = delete;
public:

  /// constructor to make and execute the xact
  ProtoSplitter(
    SbReqEditConfig* edit_cfg,
    /** Transaction to which the XML is to be split */
    rwdts_xact_t *xact,
    /** [in] the underlying YANG model for the splitter */
    rw_yang::YangModel *model,
    RWDtsXactFlag flag,
    /** [in] tag of the extension to split at */
    rw_yang::XMLNode* root_node
  );

  /// constructor to not make the xact and only store the contents of the blocks
  ProtoSplitter(
    SbReqEditConfig* edit_cfg,
    /** Transaction to which the XML is to be split */
    rwdts_xact_t *xact,
    /** [in] the underlying YANG model for the splitter */
    rw_yang::YangModel *model,
    RWDtsXactFlag flag,
    /** [in] tag of the extension to split at */
    rw_yang::XMLNode* root_node,
    /** [in] used to transfer ownership of the transaction contents */
    std::list<BlockContents> & transaction_contents
  );


  /// trivial  destructor
  virtual ~ProtoSplitter();

  ///@see XMLVisitor::visit
  rw_yang::rw_xml_next_action_t visit(rw_yang::XMLNode* node,
                                      rw_yang::XMLNode** path,
                                      int32_t level);

  bool executed_xact();

};

/*!
 * The ConfigPublishRegistrar helps the uagent register with DTS to provide
 * configuration data for DTS members. Once a configuration transaction
 * is committed by DTS, the management agent walks that data and registers
 * to provide data at different registration points.
 */
class ConfigPublishRegistrar
    :public rw_yang::XMLVisitor
{

 public:
  /** The app data type created from
     the namespace and tag. XML is split
     at nodes which have this app data*/
  rw_yang::AppDataToken<const char*> app_data_;

  /** The API handle at which registrations have to be made */
  Instance *instance_;

 public:
  ConfigPublishRegistrar (
      /** [in] The API handle at which registrations have to be made */
      Instance *inst,
      /** [in] the underlying YANG model for the splitter */
      rw_yang::YangModel *model,
      /** [in] name space of the extension to split at */
      const char *ns,
      /** [in] tag of the extension to split at */
      const char *extension
  );

  virtual ~ConfigPublishRegistrar() {};

  ConfigPublishRegistrar(const ConfigPublishRegistrar&) = delete;
  ConfigPublishRegistrar& operator = (const ConfigPublishRegistrar&) = delete;

 public:
  ///@see XMLVisitor::visit
  rw_yang::rw_xml_next_action_t visit(rw_yang::XMLNode* node,
                                      rw_yang::XMLNode** path,
                                      int32_t level);
};
}
__BEGIN_DECLS

/*!
 * Compatibility structure for C plugin interface.
 */
struct rwuagent_instance_s {
  CFRuntimeBase _base;
  rwuagent_component_t component;
  rw_uagent::Instance* instance;
  rwtasklet_info_ptr_t rwtasklet_info;
};

__END_DECLS


namespace rw_uagent {

inline rwtasklet_info_ptr_t Instance::rwtasklet()
{
  RW_ASSERT(rwuai_ && rwuai_->rwtasklet_info);
  return rwuai_->rwtasklet_info;
}

inline rwsched_instance_ptr_t Instance::rwsched()
{
  rwtasklet_info_ptr_t tasklet = rwtasklet();
  RW_ASSERT(tasklet->rwsched_instance);
  return tasklet->rwsched_instance;
}

inline rwsched_tasklet_ptr_t Instance::rwsched_tasklet()
{
  rwtasklet_info_ptr_t tasklet = rwtasklet();
  RW_ASSERT(tasklet->rwsched_tasklet_info);
  return tasklet->rwsched_tasklet_info;
}

inline rwtrace_ctx_t* Instance::rwtrace()
{
  rwtasklet_info_ptr_t tasklet = rwtasklet();
  RW_ASSERT(tasklet->rwtrace_instance);
  return tasklet->rwtrace_instance;
}

inline rwlog_ctx_t* Instance::rwlog()
{
  rwtasklet_info_ptr_t tasklet = rwtasklet();
  RW_ASSERT(tasklet->rwlog_instance);
  return tasklet->rwlog_instance;
}

inline struct rwvcs_instance_s* Instance::rwvcs()
{
  rwtasklet_info_ptr_t tasklet = rwtasklet();
  RW_ASSERT(tasklet->rwvcs);
  return tasklet->rwvcs;
}

inline rwmemlog_instance_t * Instance::get_memlog_inst()
{
  return memlog_inst_;
}

inline rwmemlog_buffer_t * Instance::get_memlog_buf()
{
  return memlog_buf_;
}

inline rwmemlog_buffer_t** Instance::get_memlog_ptr()
{
  return &memlog_buf_;
}

inline RequestMode Instance::get_request_mode()
{
  return request_mode_;
}

inline
void Instance::update_dyn_state(RwMgmtSchema_ApplicationState state,
                                const std::string& str)
{
  schema_state_.state = state;
  schema_state_.err_string = str;
}

inline
void Instance::update_dyn_state(RwMgmtSchema_ApplicationState state)
{
  // ATTN: Single space?
  update_dyn_state(state, " ");
}

} // end namespace rwuagent

#endif // CORE_MGMT_RWUAGENT_HPP_
