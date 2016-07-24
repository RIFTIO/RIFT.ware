
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*!
 * @file rwuagent_sb_req.hpp
 *
 * Management agent southbound request definitions
 */

#ifndef CORE_MGMT_RWUAGENT_SB_REQ_HPP_
#define CORE_MGMT_RWUAGENT_SB_REQ_HPP_

#include "rwuagent.h"
#include "rwuagent_request_mode.hpp"

#include <rwdts.h>


namespace rw_uagent {

// Confd transaction timeout for show-system-info RPC
static const int SSI_CONFD_ACTION_TIMEOUT_SEC = 300; // ATTN:- This has to be changed to periodically push it.

// Confd transaction timeout for show-agent-logs RPC
static const int SHOW_LOGS_ACTION_TIMEOUT_SEC = 300;


/*!
 * Management agent southbound request, base class.  This is a pure
 * vitual class.  Concrete implementations exist for NETCONF
 * operations.
 *
 * Southbound requests typically require DTS transactions.  If a DTS
 * transaction is required, the southbound request:
 *   . Translates data sent by the client to message and keyspec.  For
 *     configuration (edit-config) operations, this could mean
 *     splitting a DOM fragement send by the client into multiple
 *     protobuf messages.
 *   . Starts an appropriate transaction to DTS.
 *   . Gathers the results from DTS.
 *   . Translates the results into data that is understood by the client.
 *
 * To create and invoke a SB request, create the SbReqX object and then
 * call start_xact().  It may or may not complete synchronously, as
 * indicated by return value.  The NbReq is NEVER responsible for
 * deleting the SbReq.  The NbReq does not own the SbReq.  SbReq will
 * self-destruct when done (after making the NbReq response callback).
 *
 * Inside any specific flavor of SbReq, the code must call either
 * done_with_error() or done_with_success() to complete the SB request,
 * and make the callback into the NB request.  The done APIs are
 * terminal - they will destroy the SbReq.
 *
 * SbReq must make a callback into NbReq when done.  NbReq will have
 * access to the SbReq in this callback, and can check whether the
 * SbReq has completed synchronously or asynchronously.  This is
 * necessary in some cases (such as confd callbacks).
 *
 * The SbReq base returns results to the northbound request via the
 * NbReq respond() method most suited to the kind of results produced.
 * The NbReq will convert the result to its preferred form.
 *
 * SbReqs may complete synchronously or asynchronously.  NbReqs must
 * wait for the SbReq to complete before sending a response to the
 * client.  Responses are indicated solely by a call to one of the
 * NbReq respond() methods, and exactly one such callback must be made
 * by every SbReq - regardless of whether the SbReq completes
 * synchronously or asynchronously.
 *
 * All SbReq initiation APIs (and, by extention, the APIs they may call
 * directly) are assumed to be able to complete either immediately or
 * asyncnronously.  All such APIs must return a StartStatus to indicate
 * which has occurred.  If synchronous, the SbReq will have already
 * been destroyed upon return.
 */
class SbReq
{
 public:
  /*!
   * Constructor - sets the instance and client members
   */
  SbReq (
    /** [in] Instance that this transaction belongs to */
    Instance* instance,

    /** [in] NbReq that initiated this transaction */
    NbReq* nbreq,

    /** [in] The transaction type */
    RwMgmtagt_SbReqType sbreq_type,

    /** [in] The request mode*/
    RequestMode request_mode,

    /** [in] Name for the trace buffer. */
    const char* trace_name,

    /** [in] Request starting this sbreq. can be null */
    const char* req = ""
  );

  /*!
   * Start the transaction.
   *
   * @return status of the transaction start
   */
  StartStatus start_xact();

  /*!
   * Abort a DTS transaction.
   */
  virtual void abort();

  /*!
   * Type : Used for logging
   */
  RwMgmtagt_SbReqType sbreq_type()
  {
    return sbreq_type_;
  }

  /*!
   * Get the description of the request.
   */
  std::string req() const
  {
    return req_;
  }

  /*!
   * Did the transaction complete asynchronously?
   */
  bool was_async() const
  {
    return was_async_;
  }

  /*!
   * Update the time stamp for a particular event
   */
  void update_stats(
    dom_stats_state_t state
  );

  /*!
   * Get the uagent instance pointer
   */
  Instance* instance() const
  {
    return instance_;
  }

  /*!
   * Get the pointer to the on-going DTS xact.
   */
  rwdts_xact_t* dts_xact() const
  {
    return xact_;
  }

 protected:

  virtual ~SbReq();

  /*!
   * Send response as this object.  The northbound request will know
   * what to do.
   */
  StartStatus done_with_success();

  /*!
   * Send response as specified DOM.
   */
  StartStatus done_with_success(
    //! [in] The DOM of the response.
    rw_yang::XMLDocument::uptr_t rsp_dom );

  /*!
   * Send response as DTS transaction.
   */
  StartStatus done_with_success(
    //! [in] The completed DTS transaction.
    rwdts_xact_t *xact );

  /*!
   * Send response as defined by a keyspec and message.
   */
  StartStatus done_with_success(
    //! [in] The keyspec
    const rw_keyspec_path_t* ks,
    //! [in] The message
    const ProtobufCMessage* msg );

  /*!
   * Send a previously recorded error response, recorded with
   * add_error().  Previousl call to add_error() is a pre-condition -
   * this will crash if there are no recorded errors.
   */
  StartStatus done_with_error();

  /*!
   * Send an error message response.
   */
  StartStatus done_with_error(
    const std::string& error_str );

  /*!
   * Send specific error list as a response.
   */
  StartStatus done_with_error(
    //! [in] The list of errors encountered.
    NetconfErrorList* nc_errors );

  /*!
   * Add an error to the response, but don't send response yet.
   */
  NetconfError& add_error(
    RwNetconf_ErrorType errType = RW_NETCONF_ERROR_TYPE_APPLICATION,
    IetfNetconf_ErrorTagType errTag = IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED
  );

 private:
  static void rwdts_xact_end_wrapper (void *xact);

  static void async_delete(void *ud);

 protected:
  /*!
   * Internal start - subclasses implement this method.  The method
   * MUST return an indication of whether the southbound request
   * completed immediately or not.
   */
  virtual StartStatus start_xact_int() = 0;

  virtual void schedule_async_delete();

 protected:
  /*!
   * The transcation type, for logging.
   */
  RwMgmtagt_SbReqType sbreq_type_;

  /*!
   * The instance that this transaction belongs to.
   */
  Instance* instance_ = nullptr;

  /*!
   * The memory logger for this transaction.
   * ATTN: Borrow NbReq's?
   */
  RwMemlogBuffer memlog_buf_;

  /*!
   * The client that initiated this transaction through UAGENT
   */
  NbReq* nbreq_ = nullptr;

  /*!
   * The DTS member interface.
   */
  DtsMember* dts_ = nullptr;

  /*!
   * Flags to be sent to DTS
   */
  RWDtsXactFlag dts_flags_ = RWDTS_XACT_FLAG_NONE;

  /*!
   * The request that started this transaction. Is null for edit config. Used
   * for state gathering purposes
   */
  std::string req_;

  /*!
   * The transaction that is run in DTS.
   */
  rwdts_xact_t* xact_ = nullptr;

  struct timeval last_stat_time_;

  RWPB_T_MSG(RwMgmtagt_SpecificStatistics_ProcessingTimes) statistics_;

  /*!
   * Running list of errors found while processing the transaction.
   */
  NetconfErrorList nc_errors_;

  /*!
   * Will be set to true if the transaction will complete
   * asynchronously, as indicated by the start functions.
   */
  bool was_async_ = false;

  /*!
   * The response has been sent.  Will be set by all of the done
   * functions.  Must be true before the destructor runs.
   */
  bool responded_ = false;

  /// name for tracing and logging
  std::string const trace_name_;

  /// The request mode
  RequestMode request_mode_;
};

class SbReqEditConfig
: public SbReq
{
 public:
  /*!
   * Build a DTS EditConfig transaction. Some parts of the intended
   * configuration changes are required for building the DTS transaction. Once
   * a transaction is build, it should be possible to add new xml fragments to
   * it, as and when the UAGENT supports such clients. To this end, the building
   * of a transaction is delayed till a call to start_xact() is made.
   */
  SbReqEditConfig (
      Instance* instance,
      NbReq* nbreq,
      RequestMode request_mode,
      const char *xml_fragment);  /**< [in] A string that has the xml fragment
                                   that forms part of a configuration change*/

  /*!
   * Build a DTS EditConfig delete transaction for the provided
   * keyspec.
   */
  SbReqEditConfig (
      Instance* instance,
      NbReq* nbreq,
      RequestMode request_mode,
      UniquePtrKeySpecPath::uptr_t delete_ks /**< [in] The keyspec to be deleted
                                               called from pb-request delete operation*/
      );

  /*!
   * Build a DTS EditConfig transaction. Some parts of the intended
   * configuration changes are required for building the DTS transaction. Once
   * a transaction is build, it should be possible to add new xml fragments to
   * it, as and when the UAGENT supports such clients. To this end, the building
   * of a transaction is delayed till a call to start_xact() is made.
   *
   * This particular constructor uses a builder object. The builder object
   * encapuslates a DOM of merges, and a list of keyspecs for deletes.
   */
  SbReqEditConfig (
      Instance* instance,
      NbReq* nbreq,
      RequestMode request_mode,
      rw_yang::XMLBuilder *builder);


  virtual ~SbReqEditConfig();

  /*!
   * Transfers the collected delta over the transaction
   * to the caller.
   */
  rw_yang::XMLDocument::uptr_t move_delta();

  /*!
   * Transfers all the delete keyspecs created over the
   * transaction to the caller.
   */
  std::list<UniquePtrKeySpecPath::uptr_t> move_deletes();

 private:
  /*!
   * @see SbReq::Start()
   * The start of a transaction creates a DTS transaction. The DOM is split into
   * multiple protobuf messages. The protobuf messages are added as seperate
   * queries in the first transaction block. The transaction is then sent to DTS
   * for completion.
   */
  StartStatus start_xact_int() override;

  /*!
   */
  static void start_xact_xml_agent_prepare_xact(void * context);
  /*!
   */
  static void start_xact_xml_agent_execute_xact(void * context);

  /*!
   */
  StartStatus collect_xact_contents_and_validate_dom_and_save_dom();

  /*!
   * Build and execute the DTS transaction for xml-mode.
   */
  StartStatus build_and_execute_transaction();

  /*!
   * Build the delete xact blocks.
   */
  bool add_deletes_to_xact();

  /*!
   * @see SbReq::CommitCb()
   * Once an EditConfiguration operation is committed by DTS, the delta that is
   * held by the transaction is committed to the running configuration stored by
   * the agent. The client that initiated this transaction is then notified
   * on the status of the transaction.
   */
  virtual void commit_cb(
      rwdts_xact_t *xact);

  static void dts_config_event_cb(
    rwdts_xact_t *xact,
    rwdts_xact_status_t* xact_status,
    void *ud);

 private:

  /*!
   * Used to hold the state of the transaction.
   */
  bool success_ = true;

  /*!
   * The configuration changes that are part of the NETCONF edit-config
   * operation.  The current clients provide the DOM in one
   * interaction.  Future clients could construct a DOM at the uagent
   * through multiple transactions before doing a commit.
   *
   * The configuration changes are local to the agent before a commit
   * is completed through DTS.  Once DTS commits a transaction, the
   * changes are merged to the Instances configuration DOM.
   */
  rw_yang::XMLDocument::uptr_t delta_;

  /*!
   * A list of keyspecs that are to be deleted as part of this
   * transaction.
   */
  std::list<UniquePtrKeySpecPath::uptr_t>  delete_ks_;

  /*!
   * Modified instance dom with the delta_ changes applied
   */
  rw_yang::XMLDocument::uptr_t new_instance_dom_;

  std::list<BlockContents> xml_agent_xact_contents_;

};

class SbReqGet
: public SbReq
{
 public:

  /*!
   * A discriminator for the various async dispatch
   * callbak types.
   */
  enum class async_dispatch_t
  {
    DTS_QUERY = 1,
    DTS_CCB   = 2,
  };

  /*!
   * Build a DTS RPC transaction. The XML fragment has to be valid according to
   * YANG.
   */
  SbReqGet (
    Instance* instance,
    NbReq* nbreq,
    RequestMode request_mode,
    /**
     * [in] A string that has the xml fragment that forms part of a
     * configuration change
     */
    const char *xml_fragment,
    const bool is_internal_request = false
  );

  /*!
   * Build a DTS RPC transaction with the DOM passed in. The transaction
   * owns the DOM.
   */
  SbReqGet (
      Instance* instance,
      NbReq* nbreq,
      RequestMode request_mode,
      rw_yang::XMLDocument::uptr_t dom,
      const bool is_internal_request = false
  );

  /*!
   * Set the async dispatch callback type.
   */
  void set_async_dt(async_dispatch_t type)
  {
    ad_type_ = type;
  }

  /*!
   * Get the async dispatch callback type.
   */
  async_dispatch_t get_async_dt() const
  {
    return ad_type_;
  }


 private:
  virtual ~SbReqGet();

  StartStatus start_xact_int() override;

  void start_dts_xact_int();

  StartStatus start_xact_uagent_get_request();

  rw_yang_netconf_op_status_t get_dom_refresh_period(rw_yang::XMLNode *node);
  rw_yang_netconf_op_status_t get_confd_daemon(rw_yang::XMLNode *node);
  rw_yang_netconf_op_status_t get_statistics(rw_yang::XMLNode *node);
  rw_yang_netconf_op_status_t get_specific(rw_yang::XMLNode *node);

 private:

  static void dts_get_event_cb(
    rwdts_xact_t *xact,
    rwdts_xact_status_t* xact_status,
    void *ud);

  static void async_dispatch_f(void* cb_data);

  void commit_cb(
      rwdts_xact_t *xact);

 private:
  /*!
   * A protobuf is constructred when the GET transaction is created. This is
   * used to create a DTS transaction that is send to a client
   */
  pbcm_uptr_t message_;

  /*!
   * A keyspec is constructured when the GET transcation is created. This
   * is stored here temporiarly for aysnc dispatch.
   */
  ks_uptr_t query_ks_;

  /*!
   * Many async callbacks. A discriminator to handle all in a single
   * function.
   */
  async_dispatch_t ad_type_;

  /*!
   * The results of a DTS transaction is made available as a set of protobufs.
   * before a response can be send to a requesting client, a DOM has to be
   * created to merge the information in the protobuf messages.
   */
  rw_yang::XMLDocument::uptr_t rsp_dom_;

  /*!
   * Set to true if this is a southbound request kicked off by the internal
   * northbound interface.
   */
  bool is_internal_request_;
};

class SbReqRpc
: public SbReq
{
 public:
  /*!
   * Build a DTS RPC transaction.  The XML fragment has to be valid
   * according to YANG.
   */
  SbReqRpc (
    Instance* instance,
    NbReq* nbreq,
    RequestMode request_mode,
    /*!
     * [in] A string that has the xml fragment that forms part of a
     * configuration change
     */
    const char *xml_fragment );

  /*!
   * Build a DTS RPC transaction with the DOM passed in.  The
   * transaction owns the DOM.
   */
  SbReqRpc (
      Instance* instance,
      NbReq* nbreq,
      RequestMode request_mode,
      rw_yang::XMLDocument* rpc_dom );

  /*!
   * Completion callback for NbReqInternal to use for success.
   */
  void internal_done(
    //! [in] The keyspec
    const rw_keyspec_path_t* ks,
    //! [in] The message
    const ProtobufCMessage* msg );

  /*!
   * Completion callback for NbReqInternal to use for errors.
   */
  void internal_done(
    //! [in] The list of errors
    NetconfErrorList* nc_errors );

  /*!
   * Completion callback for NbReqInternal to use for success.
   */
  void internal_done();

  const pbcm_uptr_t& rpc_input() const
  {
    return rpc_input_;
  }

 private:

  ~SbReqRpc();

  void init_rpc_input(
    rw_yang::XMLNode* node );

  StartStatus start_xact_int() override;

  StartStatus start_ns_mgmtagt_dts();

  StartStatus start_ns_mgmtagt();

  StartStatus start_pb_request();

  StartStatus start_ssi();

  StartStatus show_agent_logs();

  StartStatus start_xact_dts();

  static void dts_complete_cb(
    rwdts_xact_t* xact,
    rwdts_xact_status_t* xact_status,
    void* ud);

  static void dts_cb_async(void* ud);

  static void start_xact_async(void *ud);

 private:
  /*!
   * A protobuf is constructed when the RPC transaction is created.
   * This is used to create a DTS transaction that is send to a client
   */
  pbcm_uptr_t rpc_input_;
};

/*!
 * The show-system-info CLI support class.
 * It forks CLI process and executes the commands
 * in the ssd file and returns the result as a string
 * or saves the output to the file.
 */
class ShowSysInfo
{
 public:

  /*!
   * Constructor, the parent SbReqRpc is passed here.
   */
  ShowSysInfo(
      Instance* instance,
      SbReqRpc* parent_rpc,
      const RWPB_T_MSG(RwMgmtagt_input_ShowSystemInfo)* sysinfo_in);

  /*!
   * Destructor called after the chid is successfully reaped.
   */
  ~ShowSysInfo();

  /*!
   * Execute the command
   */
  StartStatus execute();

  /*!
   * Called in the child process after fork.
   */
  void execute_child_process();

  /*!
   * Send response after the child has finished executing.
   */
  void cleanup_and_send_response();

  /*!
   * Callback function when the pipe has become read ready.
   */
  static void pipe_ready(void* ctx );

  /*!
   * Timer callback to poll for child status.
   */
  static void poll_for_child_status( void* ctx );

  /*!
   * Async delete callback.
   */
  static void async_self_delete( void* ctx );

  /*!
   * Try reaping the child, by calling waitid.
   */
  void try_reap_child();

  /*!
   * Send Error response.
   */
  StartStatus respond_with_error();

  /*!
   * Read the data from the pipe.
   */
  void read_from_pipe();

  /*!
   * Add a netconf error.
   */
  NetconfError& add_error(
    RwNetconf_ErrorType errType = RW_NETCONF_ERROR_TYPE_APPLICATION,
    IetfNetconf_ErrorTagType errTag = IETF_NETCONF_ERROR_TAG_TYPE_OPERATION_FAILED
  );

  Instance* instance() { return instance_; }

  /*!
   * return whether the response is already sent or not.
   */
  bool get_responded() { return responded_; }

 private:

  /*! The uagent instance pointer */
  Instance* instance_;

  /*! The  memlog_buf_ pointer. */
  RwMemlogBuffer memlog_buf_;

  /*! The parent rpc who created me */
  SbReqRpc* parent_rpc_;

  /*! Pointer to the show-sysinfo rpc request */
  const RWPB_T_MSG(RwMgmtagt_input_ShowSystemInfo)* sysinfo_in_;

  /*! Read side of the pipe */
  int parent_read_fd_ = -1;

  /*! Write size of the pipe */
  int child_write_fd_ = -1;

  /*! The child pid */
  int child_pid_ = -1;

  /*! The child status poll timer */
  rwsched_dispatch_source_t child_status_poll_timer_ = nullptr;

  /*! The dispatch source for child_write_fd_ */
  rwsched_dispatch_source_t child_read_src_ = nullptr;

  /*! The read buffer for storing the output */
  char *output_buff_ = nullptr;

  /*! The total bytes read from the pipe */
  size_t rsp_bytes_ = 0;

  /*! The size of the output buffer */
  size_t curr_buff_sz_ = 0;

  /*! Netconf error list. */
  NetconfErrorList nc_errors_;

  /*! A flag to indicate whether the rpc was responded. */
  bool responded_ = false;

  /*! Send SSI as string */
  bool ssi_as_string_ = false;

  /*! Save SSI to file */
  std::string ssi_fname_;
};

}
// end namespace rwuagent

#endif // CORE_MGMT_RWUAGENT_SB_REQ_HPP_
