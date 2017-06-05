/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwuagent_nb_req.hpp
 *
 * Management agent northbound request definitions
 */

#ifndef CORE_MGMT_RWUAGENT_NB_REQ_HPP_
#define CORE_MGMT_RWUAGENT_NB_REQ_HPP_

#include "rwuagent.h"
#include "rwuagent_request_mode.hpp"

namespace rw_uagent {

/*!
 * Management agent northbound request, base class.  This is a pure
 * vitual class.  Concrete implementations exist for various clients
 * and request types.
 *
 * Management agent receives northbound requests from its clients.  The
 * NbReq objects parse the client's request and then issue zero or more
 * SbReqs in order to satisfy the northbound request.
 *
 * Although the NbReq creates and invokes the SbReqs, the NbReq MUST
 * NOT keep a pointer to the SbReq - NbReq does not own SbReq.  SbReq
 * will self-destruct when done, after making the NbReq respond()
 * callback During the respond() callback, NbReq will have access to
 * the SbReq.
 *
 * All NbReq initiation APIs (and, by extention, the APIs they may call
 * directly) are assumed to be able to complete either immediately or
 * asyncnronously.  All such APIs must return a StartStatus to indicate
 * which has occurred.  If synchronous, the NbReq may have already been
 * destroyed upon return.
 *
 * There are several flavors of callback into NbReq, all named
 * respond().  The flavors take different kinds of response data - the
 * DTS transaction, a DOM, a keyspec+message, or errors specified in
 * various ways.  The default implementations all try to convert the
 * response into a DOM or an error list.  All NbReq classes must
 * minimally implement the DOM respond() and the error list respond().
 *
 * The conversion of SbReq response data into a form that the NbReq
 * would like to have occurs in the respond() methods.  They exist in
 * the NbReq in order to remove the old requirement that the SbReq
 * convert the response to DOM.  It is now theoretically possible to
 * avoid DOM conversion on response, but that only happens in one case
 * to support the new Internal requests.
 *
 * Every kind of NbReq must implement a minimum of two respond()
 * functions - the DOM-based success response, and the error list
 * failure response.  These a pure virtual.  Eventually, we will
 * refactor the agent to remove the DOM requirement, because it is an
 * expensive operation and a double-conversion on most requests
 * (PB->DOM, and then DOM->confd - this is dumb, but we have not had
 * time to fix it yet).
 *
 * A NbReq may also re-implement any of the other respond() functions
 * defined in the base NbReq.  The base NbReq has default
 * implementations that convert various kinds of responses into either
 * a DOM or an error list.  However, a specific flavor of NbReq may
 * have a more efficient path for certain kinds of responses -
 * therefore they can be overridden.
 */
class NbReq
{
 public:
  NbReq(
    Instance* instance,
    const char* type,
    RwMgmtagt_NbReqType nbreq_type );

  virtual ~NbReq();

  // cannot copy
  NbReq(const NbReq&) = delete;
  NbReq& operator=(const NbReq&) = delete;

 public:
  virtual StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) = 0;

  virtual StartStatus respond(
    SbReq* sbreq
  );

  virtual StartStatus respond(
    SbReq* sbreq,
    rwdts_xact_t *xact
  );

  virtual StartStatus respond(
    SbReq* sbreq,
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg
  );

  virtual StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) = 0;

  virtual StartStatus respond(
    SbReq* sbreq,
    const std::string& error_str
  );

  virtual int set_timeout(int timeout_sec)
  { return timeout_sec; } // Empty implementation.

  virtual rwsched_dispatch_queue_t get_execution_q() const;

 public:
  RwMgmtagt_NbReqType nbreq_type() const
  {
    return nbreq_type_;
  }

  Instance* instance() const
  {
    return instance_;
  }

 protected:
  /*!
   * Merges the changes obtained from SBreq instance
   * into instance config DOM on COMMIT event
   * from confd.
   */
  void commit_changes();

 protected:
  /// The owning uAgent.
  Instance* instance_;

  /// rwmemlog logging buffer
  RwMemlogBuffer memlog_buf_;

  /// The type of client.
  RwMgmtagt_NbReqType nbreq_type_;

  /* Below members are valid only for Nb instances
   * working with SbEditCfg
   */
  /// The data borrowed from SBreq during its response callback
  rw_yang::XMLDocument::uptr_t sb_delta_ = nullptr;
  
  /// The delete ks list borrowed from SBReq during its response callback
  std::list<UniquePtrKeySpecPath::uptr_t> sb_delete_ks_;
};


/*!
 * An internal messaging based client.  All messaging-based clients
 * funnel through a single instance of this class.
 */
class NbReqMsg
: public NbReq
{
public:
  NbReqMsg(
    //! [in] The request
    Instance* instance,
    //! [in] The request
    const NetconfReq* req,
    //! [in] The rwmsg closure
    NetconfRsp_Closure rsp_closure,
    //! [in] The rwmsg closure data
    void* closure_data
  );

  ~NbReqMsg();

private:
  // cannot copy
  NbReqMsg(const NbReqMsg&) = delete;
  NbReqMsg& operator=(const NbReqMsg&) = delete;

public:

  /// Execute the request.
  StartStatus execute();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

  rwsched_dispatch_queue_t get_execution_q() const override;

private:

  StartStatus send_response(
    std::string response );

public:

  // ATTN: Need or drop?
  const NetconfReq* req_;

  // The netconf operation to perform
  NetconfOperations operation_;

  std::string request_str_;

  /// The rwmsg response closure, needed to send back response
  NetconfRsp_Closure rsp_closure_;

  /// The rwmsg closure data, needed to send back response
  void* closure_data_;
};


/*!
 * An internal messaging based client.  All messaging-based clients
 * funnel through a single instance of this class.
 */
class NbReqDts
: public NbReq
{
public:
  NbReqDts(
    Instance* instance,
    const rwdts_xact_info_t* xact_info,
    RWDtsQueryAction action,
    const rw_keyspec_path_t* ks_in,
    const ProtobufCMessage* msg );
  ~NbReqDts();

  // cannot copy
  NbReqDts(const NbReqDts&) = delete;
  NbReqDts& operator=(const NbReqDts&) = delete;

public:
  StartStatus execute();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

private:
  StartStatus send_error(
    rw_yang_netconf_op_status_t ncs );

private:
  const rwdts_xact_info_t* xact_info_ = nullptr;
  RWDtsQueryAction action_ = RWDTS_QUERY_INVALID;
  const rw_keyspec_path_t* ks_in_ = nullptr;
  const ProtobufCMessage* msg_ = nullptr;
};


/*!
 * An internal messaging based client.  All messaging-based clients
 * funnel through a single instance of this class.
 */
class NbReqInternal
: public NbReq
{
public:
  NbReqInternal(
    Instance* instance,
    SbReqRpc* parent_rpc,
    const RwMgmtagtDts_PbRequest* pb_req );
  ~NbReqInternal();

  // cannot copy
  NbReqInternal(const NbReqInternal&) = delete;
  NbReqInternal& operator=(const NbReqInternal&) = delete;

public:
  StartStatus execute();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg
  ) override;

  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

private:

  std::string get_response_string(
      rw_yang::XMLDocument::uptr_t rsp_dom);

  void send_success(
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg );

  void send_error(
    rw_yang_netconf_op_status_t ncs,
    const std::string& err_str );

  void send_error(
    NetconfErrorList* nc_errors );

  void async_dequeue_pb_req();

  static void dequeue_pb_req_cb(void*);

private:
  SbReqRpc* parent_rpc_;
  const RwMgmtagtDts_PbRequest* pb_req_;
  RequestMode request_mode_;
};


// Fwd Decl.
class XMLMgmtSystem;

/*!
 * An internal northbound client instance
 * used only for handliing DTS transactions during reload
 * of XML configuration.
 */
class NbReqXmlReload
  : public NbReq
{
public:
  NbReqXmlReload(Instance* instance, XMLMgmtSystem* mgmt, std::string reload_xml);
  ~NbReqXmlReload();

  // cannot copy
  NbReqXmlReload(const NbReqXmlReload&) = delete;
  void operator=(const NbReqXmlReload&) = delete;

public:
  StartStatus execute();

  StartStatus respond(
    SbReq* sbreq,
    rw_yang::XMLDocument::uptr_t rsp_dom
  ) override;
  
  StartStatus respond(
    SbReq* sbreq,
    const rw_keyspec_path_t* ks,
    const ProtobufCMessage* msg
  ) override;
  
  StartStatus respond(
    SbReq* sbreq,
    NetconfErrorList* nc_errors
  ) override;

private:
  XMLMgmtSystem* mgmt_ = nullptr;

  // XML String that is to be reloaded
  std::string reload_xml_;
};


} // end namespace rwuagent

#endif // CORE_MGMT_RWUAGENT_NB_REQ_HPP_
