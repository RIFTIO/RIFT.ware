
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
 * @file rwdts_member_api.h
 * @brief Member API for RW.DTS
 * @author Rajesh Velandy(rajesh.velandy@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_MEMBER_API_H
#define __RWDTS_MEMBER_API_H

#ifndef __GI_SCANNER__
#include <rwlib.h>
#include <rwtrace.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>
#include <rwmsg.h>
#include <rwdts_api_gi_enum.h>
#include <rw_keyspec.h>
#include <rw_sklist.h>
#include <uthash.h>

#endif /* __GI_SCANNER__ */

__BEGIN_DECLS

#ifndef __GI_SCANNER__



typedef struct rwdts_group_s rwdts_group_t;
typedef struct rwdts_member_reg_s  rwdts_member_reg_t;

/*!
 * Opaque member registration handle returned by  rwdts_member_register().
 * The registration handle and associated data can be destroyed by calling rwdts_member_deregister()
 */
struct rwdts_member_reg_handle_s {
};

/*!
 * An opaque structure to pass the cursor info back and forth  between member and DTS member API.
 */

struct rwdts_member_cursor_s {
};
#endif /* __GI_SCANNER__ */

typedef struct rwdts_member_cursor_s rwdts_member_cursor_t;

// ATTN -- The following definition need to go away once the rift_add_proto target has support for generating
// introspection related files.
#ifdef __GI_SCANNER__
typedef enum _RWDtsEventRsp {
  RWDTS_EVTRSP_NULL  = 0,
  RWDTS_EVTRSP_ASYNC = 1,
  RWDTS_EVTRSP_NA    = 2,
  RWDTS_EVTRSP_ACK   = 3,
  RWDTS_EVTRSP_NACK  = 4,
  RWDTS_EVTRSP_INTERNAL = 5
} RWDtsEventRsp;

typedef enum _RWDtsQueryAction {
  RWDTS_QUERY_CREATE = 1,
  RWDTS_QUERY_READ   = 2,
  RWDTS_QUERY_UPDATE = 3,
  RWDTS_QUERY_DELETE = 4,
  RWDTS_QUERY_RPC    = 5
    _PROTOBUF_C_FORCE_ENUM_TO_BE_INT_SIZE(RWDTS_QUERY_ACTION)
} RWDtsQueryAction;

#endif /* __GI_SCANNER__ */

/*
 * DTS Member API defintiions
 */

typedef struct  _RWDtsQuery* rwdts_query_handle_t;


/*!
 * DTS registration handle
 */
typedef struct rwdts_member_reg_handle_s* rwdts_member_reg_handle_t;

typedef struct rwdts_member_data_record_s rwdts_member_data_record_t;

/*!
 * An indication of member operation
 */

enum rwdts_member_op_s {
  RWDTS_MEMBER_OP_CREATE = 990, /*!< create operation -- Fails if the object is already created */
  RWDTS_MEMBER_OP_READ   = 991, /*!< read operation */
  RWDTS_MEMBER_OP_UPDATE = 992, /*!< update operation -- When flag has RWDTS_FLAG_REPLACE  object is created if not already e     xists */
  RWDTS_MEMBER_OP_DELETE = 993, /*!< delete operation */
  RWDTS_MEMBER_OP_RPC    = 994  /*! RPC operation */
};

typedef enum rwdts_member_op_s rwdts_member_op_t;

/*!
 * Structure used by the member to respond to  query request in both the sync/async
 * A scenarios. In the sync case the member will invoke this API from the  registration callbacks
 */

struct rwdts_member_query_rsp_s {
  rw_keyspec_path_t    *ks;          /*!< keyspec for this reponse */
  uint32_t         n_msgs;      /*!< number of messages in this respose */
  ProtobufCMessage **msgs;      /*!< The messages an atrray of pointers to protobufs - caller owns the array and contents */
  uint32_t         flags:31;    /*!< flags associated with this reponse */
  uint32_t         more;        /* FLAG to indicate more results */
  uint32_t         corr_id_len; /*!< corrleation id length of correlation id if any present in the query */
  uint8_t          *corr_id;    /*!< correlation id if any present in the query */
  void            *getnext_ptr; /*! pointer used by app to hold getnext related information */
  RWDtsEventRsp    evtrsp;      /*!< Query event response */
};

/*!
 * Structure used by the member to respond to  query request in both the sync/async
 * A scenarios. In the sync case the member will invoke this API from the  registration callbacks
 */

typedef struct rwdts_xact_info_s {
  /*! DTS API handle */
  rwdts_api_t*              apih;
  /* DTS tranaction handle */
  rwdts_xact_t*             xact;
  /*! DTS query  handle if applicable*/
  rwdts_query_handle_t      queryh;
  /*! DTS member transaction event */
  rwdts_member_event_t      event;
  /*! DTS registration that matched this transaction */
  rwdts_member_reg_handle_t regh;
  /*! transactional or non transactional */
  bool transactional;
  /*! User data associated with the registration */
  void*                     ud;
  /*! User data returned by registration group's xact_init */
  void*                     scratch;
} rwdts_xact_info_t;

typedef struct rwdts_commit_record_s rwdts_commit_record_t;

/*!
 * DTS sends an array of this structure in the precommit/commit/abort callbacks.
 */
struct rwdts_commit_record_s {
  rw_keyspec_path_t*        ks;     /*! [in] Keyspec for this commit record -- ATTN -- should this change to minikey */
  rw_keyspec_path_t*        in_ks;  /*! [in] Incoming Keyspec for this commit record */
  rwdts_member_op_t         op;     /*! [in] Operation for this transaction */
  ProtobufCMessage*         msg;    /*! [in] The protobuf message associated with this commit record */
  void*                     ud;     /*! [in] userdata associated with the registration */
};

typedef void* (*xact_init_callback)(rwdts_group_t*  grp,
                                    rwdts_xact_t*   xact,
                                    void*           user_data);

typedef void (*xact_deinit_callback)(rwdts_group_t* grp,
                                     rwdts_xact_t*  xact,
                                     void*          user_data,
                                     void*          scratch);

typedef void (*recov_event_callback)(rwdts_group_t* grp,
                                     rwdts_member_reg_handle_t reg,
                                     void*           user_data);

typedef  rwdts_member_rsp_code_t
(*xact_event_callback)(rwdts_api_t*         apih,
                       rwdts_group_t*       grp,
                       rwdts_xact_t*        xact,
                       rwdts_member_event_t xact_event,
                       void*                ctx,
                       void*                scratch);
/*!
 *  Group registration structure
 */
typedef struct  rwdts_group_cbset_s {
  /* Constant context for all transactions.  Typically the tasklet
     instance pointer, or for a library implementing a group, the
     library's main pointer. */
  void *ctx;

  /* Ctor/dtor code for per-transaction-per-group application state
     (aka "scratch").  _init returns scratch; _deinit takes it and
     frees it. */
  xact_init_callback   xact_init;
  xact_deinit_callback xact_deinit;

  /* The fundamentally transaction-wide events PRECOMMIT SUBCOMMIT
     COMMIT ABORT can be handled via this callback registered here.
     These events are issued once per xact as opposed to once per
     registration.  With reg groups this becomes semantically
     preferable for many functions. */
  xact_event_callback xact_event;

  /* This callback function is called on recovery of the group
   * registration */
  recov_event_callback recov_event;

  /* Mainly for Gi binding, leave as zeroes */
  GDestroyNotify xact_init_dtor;
  GDestroyNotify xact_deinit_dtor;
  GDestroyNotify xact_event_dtor;

  /* Per-registration events occur via the usual callbacks, which now include a scratch pointer */
} rwdts_group_cbset_t;

/*!
 * Get keyspec from a commit record
 * @param crec commit record
 *
 * @return the keyspec from commit record
 */
#define RWDTS_MEMBER_CREC_KEYSPEC(crec) ((crec)->ks)

/*!
 * Get operation from a commit record
 * @param crec commit record
 *
 * @return the operation from commit record
 */
#define RWDTS_MEMBER_CREC_OP_CODe(crec) ((crec)->op)

/*!
 * Get Message pointer from commit record
 * @param crec commit record
 *
 * @return the message pointer from commit record
 */
#define RWDTS_MEMBER_CREC_MSG(crec) ((crec)->msg)

/*!
 * Get user data from the commit record
 * @param crec commit record
 *
 * @return the message pointer from commit record
 */
#define RWDTS_MEMBER_CREC_USER_DATA(crec) ((crec)->ud)

typedef struct rwdts_member_event_cb_s  rwdts_member_event_cb_t;
typedef struct rwdts_member_query_rsp_s rwdts_member_query_rsp_t;
typedef struct rwdts_member_registration_s rwdts_member_registration_t;
typedef struct rwdts_appconf_s rwdts_appconf_t;
typedef struct rwdts_api_s rwdts_api_t;
typedef struct rwtasklet_info_s rwtasklet_info_t;
typedef struct rwdts_xact_s rwdts_xact_t;
typedef struct rwdts_xact_block_s rwdts_xact_block_t;
typedef struct rwdts_member_data_elem_s rwdts_member_data_elem_t;
typedef struct rwdts_query_result_s rwdts_query_result_t;
typedef struct rwdts_state_change_cb_s rwdts_state_change_cb_t;
typedef struct rwdts_event_cb_s rwdts_event_cb_t;
typedef struct rwdts_shard_info_detail_s rwdts_shard_info_detail_t;
typedef struct rwdts_shard_db_num_info_s rwdts_shard_db_num_info_t;
typedef struct rwdts_shard_info_s rwdts_shard_info_t;
typedef struct rwdts_member_reg_handle_s*  rwdts_member_reg_handle_t;
typedef struct rwdts_group_s rwdts_group_t;


/*!
 * Registers the specified keypath as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when an
 * advise transaction is received matching the registered keyspec.
 *
 * @param apih    DTS API handle
 * @param keyspec The keyspec for which the registration is being made
 * @param cb      The callback routine registered for this keyspec
 * @param desc    The message descriptor for the mesage associated with the keyspec
 * @param flags   Flags associated with this registration.
 * @param shard   Sharding details.
 *
 * @return        A  handle to the member registration.
 */

rwdts_member_reg_handle_t
rwdts_member_register(rwdts_xact_t*                     xact,
                      rwdts_api_t*  apih,
                      const rw_keyspec_path_t*          keyspec,
                      const rwdts_member_event_cb_t*    cb,
                      const ProtobufCMessageDescriptor* desc,
                      uint32_t                          flags,
                      const rwdts_shard_info_detail_t*  shard_detail);

/*!
 * Deregister registration(s) based on the passed keyspec.
 * All registrations matching the passed keyspec  will be deregistered.
 *
 * @param apih     DTS API handle
 * @param keyspec  The keyspec for which the deregistration is being made
 *
 * @return         status of deregistration.
 */
rw_status_t
rwdts_member_deregister_keyspec(rwdts_api_t*  apih,
                                rw_keyspec_path_t* keyspec);

/*!
 * Deregister  the registration based on registration handle
 *
 * @param regh     Keyspec registration handle
 *
 * @return         status of deregistration.
 */
rw_status_t
rwdts_member_deregister(rwdts_member_reg_handle_t  regh);

/*!
 * Deregister the member at the router.
 * This API should be used by a member to remove the registrations of another expired member.
 *
 * @param apih     DTS API handle of the proxy member
 * @param path     MSG path of the  member to be deregistered.
 * @param recovery Indicate the recovery action of the dregister member
 *
 * @return         status.
 */
rw_status_t
rwdts_member_deregister_path(rwdts_api_t *apih,
                             char*        path,
                             vcs_recovery_type recovery_action);

/*!
 * Should this be here?
 * ATTN -- Prashanth to look into this
 */

void
rwdts_get_shard_db_info_sched(rwdts_api_t* apih,
                              void*        callbk_fn);

/*!
 * The following group of APIs collectively are called the DTS data member APIs.
 * They are used by the members to manipulate data associated with a publisher registration.
 * The  publisher data is maintained in different ways within a member.
 *   1) All data resides within the API .
 *   2) All data resides within the Application.
 *   3) Hybrid mode where data is split between API and the application based on registration.
 */

/*!
 * An opaque structure representing an outboard object
 */

enum rwdts_member_data_flags_s {
  RWDTS_MBBR_DATA_VOLATILE = 0x1, /*! The  data is volatitle  such as stats */
  RWDTS_MBBR_DATA_KEYED    = 0x2, /*! This data is keyed - results in a get next call as opposed to get */
};

typedef  enum rwdts_member_data_flags_s rwdts_member_data_flags_t;

/*!
 * A structure to maintain DTS member data
 */
typedef struct rwdts_member_data_obj_t {
  // Important - the data field shoudl be the first member of this structure
  void *data ; /*! < actual data object */
} rwdts_member_data_obj_t;


typedef rwdts_member_data_obj_t* (*rwdts_member_get_next_obj_fn)(rwdts_member_data_obj_t *current);


typedef rw_status_t (*xact_outboard_member_data_cb)(
    rwdts_xact_t*                  xact,           /*! [in] transaction when available */
    rwdts_member_reg_handle_t      reg,            /*! [in] registration handle when available */
    rw_keyspec_path_t*             keyspec,        /*! [in] Keyspec associated with this callback  */
    rwdts_member_data_obj_t*       obj,            /*! [in] the object that  is requested by DTS */
    void*                          ud);            /*! [in] user data if any registered against the keyspec */

/*!
 * A structure for passing data member  API callbacks.
 */

typedef struct rwdts_member_data_cb_s {
  xact_outboard_member_data_cb get_request;
  void*                        ud;
} rwdts_member_data_cb_t;


/*!
 * Create a outboard object corresponding to the keyspec within the API.
 * DTS maintains a reference to this object until rwdts_member_delete_outboard_obj() is called.
 * ATTN --  not supported
 *
 * @param apih    DTS API handle
 * @param xact    DTS transaction - when non null the update is done in the  transaction's contexnt
 * @param reg     DTS member registration handle.
 * @param keypsec Keyspec to the object to be created
 * @param datap   A pointer to the data that application holds -- The API calls the get_data
 * @param cb      registered callback routines for this keyspec
 * @param flags   creation flags for this object -- RWDTS_MBR_PROTO| RWDTS_MBR_LISTY
 *
 * @return        The newly created DTS object if successfull. Returns NULL otherwise
 *
 */
rwdts_member_data_obj_t*
rwdts_member_create_outboard_obj(rwdts_api_t*                apih,
                                 rwdts_xact_t*               xact,
                                 rwdts_member_reg_handle_t   reg,
                                 rw_keyspec_path_t*          keyspec,
                                 void*                       datap,
                                 rwdts_member_data_cb_t*     cb,
                                 uint32_t                    flags);

/*!
 * Update an outboard member object with the passed protobuf message;
 * DTS maintains a reference to this object until rwdts_member_delete_outboard_obj() is called.
 * ATTN --  not supported
 *
 * @param apih    DTS API handle
 * @param xact    DTS transaction - when non null the update is done in the  transaction's contexnt
 * @param reg     DTS member registration handle.
 * @param obj     The object that needs to be updated
 * @param datap   A pointer to the data that application holds -- The API calls the get_data
 *
 * @return the updated object
 */

rwdts_member_data_obj_t*
rwdts_member_update_outboard_obj(rwdts_api_t*                apih,
                                 rwdts_xact_t*               xact,
                                 rwdts_member_reg_handle_t   reg,
                                 rwdts_member_data_obj_t* obj,
                                 void*                       datap);

/*!
 * Delete an outboard member object .
 * DTS maintains a reference to this object until rwdts_member_delete_outboard_obj() is called.
 * ATTN --  not supported
 *
 * @param apih DTS API handle
 * @param xact DTS transaction - when non null the update is done in the  transaction's contexnt
 * @param reg  DTS member registration handle.
 * @param obj  The object that needs to be updated
 * @param msg  The protobuf message with which the object need to be udpated.
 *
 * @return     Nothing
 */

void
rwdts_member_delete_outboard_obj(rwdts_api_t*                apih,
                                 rwdts_xact_t*               xact,
                                 rwdts_member_reg_handle_t*  reg,
                                 rwdts_member_data_obj_t*    obj);


/*!
 * This API is used by the application to respond to a get request for an outboard list data
 * ATTN --  not supported
 *
 * @param apih DTS API handle
 * @param xact DTS transaction - when non null the update is done in the  transaction's contexnt
 * @param reg  DTS member registration handle.
 * @param obj  DTS outboard list object
 * @param num_msgs Number of messages in the respons
 * @param msgs Responses as an array of protobuf -- DTS owns the passed proto array
 * @param more Are there more elements in the list ? TRUE passed here causes DTS to  continue invoking get request.
 *
 * @return RW status
 */

rw_status_t
rwdts_member_outboard_respond_obj_req(rwdts_api_t*                     apih,
                                      rwdts_xact_t*                    xact,
                                      rwdts_member_reg_handle_t*       reg,
                                      rwdts_member_data_obj_t*         obj,
                                      ProtobufCMessage*                msg);

/*!
 * This API is used by the application to respond to a get request for an outboard list data
 * ATTN --  not supported
 *
 * @param apih DTS API handle
 * @param xact DTS transaction - when non null the update is done in the  transaction's contexnt
 * @param reg  DTS member registration handle.
 * @param obj  DTS outboard list object
 *
 */
rw_status_t
rwdts_member_get_outboard_obj(rwdts_api_t*                apih,
                              rwdts_xact_t*               xact,
                              rwdts_member_reg_handle_t*  reg,
                              rwdts_member_data_obj_t*    obj);


/*!
 * Send a response from the member to a router
 * Member can use this function to repond to a request from the router with a reponse.
 * Use this routine send the response from registration callback to repond synchronously
 * or the reponse can be sent asynchronously later.
 *
 * @param xact   DTS transaction handle
 * @param queryh DTS Query handle passed from DTS member API to the APP to be passed back
 * @param rsp    Response structure containing the keyspec, and protobuf responses to the query.
 *               The structure and contents is owned by the caller.
 *
 * ATTN -- need additional APIs if the reponse is not in proto format TBD
 *
 */

rw_status_t
rwdts_member_send_response(rwdts_xact_t*                   xact,
                           rwdts_query_handle_t            queryh,
                           const rwdts_member_query_rsp_t* rsp);
rw_status_t
rwdts_member_send_response_more(rwdts_xact_t*                   xact,
                                rwdts_query_handle_t            queryh,
                                const rwdts_member_query_rsp_t* rsp,
                                void *getnext_ptr);

rw_status_t
rwdts_member_send_response_sync(rwdts_xact_t*                   xact,
                           rwdts_query_handle_t            queryh,
                           const rwdts_member_query_rsp_t* rsp);
/*!
 * Send a single response from the member to a router.
 * Member can use this function to repond to a request from the router with a single reponse.
 * Use this routine send the response from registration callback to repond synchronously
 * or the reponse can be sent asynchronously later.
 *
 * @param xact   DTS transaction handle
 * @param queryh DTS Query handle passed from DTS member API to the APP to be passed back
 * @param msg    A protobuf containing the response.
 *
 *
 */

rw_status_t
rwdts_member_send_response_single(rwdts_xact_t*            xact,
                                  rwdts_query_handle_t     queryh,
                                  const ProtobufCMessage*  rsp);

/*!
 * Get the action associated with the query handle
 *
 * @param  The query  handle associated with the query
 *
 * @return The action associated with this query
 */

RWDtsQueryAction
rwdts_member_get_query_action(rwdts_query_handle_t queryh);

/*!
 * Get the flags associated with the query handle
 *
 * @param  The query  handle associated with the query
 *
 * @return The action associated with this query
 */

uint32_t
rwdts_member_get_query_flags(rwdts_query_handle_t queryh);

#ifndef __GI_SCANNER__
/*!
 * Create a list element at a registraton point.
 * The API will fail, if the registration is a list boundary.
 * This function need to be executed in the scheduler queue where DTS is running.
 *
 * @param xact DTS transaction handle
 * @param reg  Registration handle at which this object is being created
 * @param mk   The minikey at which this object needs to be created
 * @param msg  The object to be created as protobuf message
 *
 * @return     A status indicating succes/failure
 *
 */

rw_status_t
rwdts_member_data_create_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    reg,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg);
#endif /* __GI_SCANNER__*/


#ifndef __GI_SCANNER__
/*!
 * Delete a list element at a registraton point. The API will fail, if the
 * registration is not a list boundary.
 *
 * @param xact DTS transaction handle
 * @param reg Registration handle at which this object is being updated.
 * @param mk The minikey of the object which need to be deleted.
 * @param msg The object to be created as protobuf message
 *
 * @return  A status indicating succes/failure
 *
 */

rw_status_t
rwdts_member_data_delete_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    reg,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg);
#endif /* __GI_SCANNER__ */

#ifndef __GI_SCANNER__
/*!
 * Update a list element at a registraton point. The API will fail, if the
 * registration is not a list boundary.
 *
 * @param xact   DTS transaction handle
 * @param reg    Registration handle at which this object is being updated.
 * @param mk     The minikey of the object which need to be updated.
 * @param msg    The object to be created as protobuf message
 * @param flags  flags indicate how to update this node - RWDTS_FLAG_MERGE|RWDTS_FLAG_REPLACE - default op is to merge
 *
 * @return       A status indicating succes/failure
 *
 */

rw_status_t
rwdts_member_data_update_list_element_mk(rwdts_xact_t*                xact,
                                         rwdts_member_reg_handle_t    reg,
                                         rw_schema_minikey_opaque_t*  mk,
                                         const ProtobufCMessage*      msg,
                                         uint32_t                     flags);
#endif /* __GI_SCANNER__ */

/*
 * Delete the  member data cursors
 * @param xact -- When passed the cursor  associated with the xact is reset
 * @param reg --  When passed the cursor  associated with the registration is reset
 *
 * @return no value
 */
void
rwdts_member_data_delete_cursors(rwdts_xact_t* xact,
                                 rwdts_member_reg_handle_t regh);

/*!
 * Get cursor at a element registration
 *
 * @param xact DTS transaction handle - When present curosor is associated with the transaction
 * @param reg Registration handle at which this object is being retrieved
 * @param msg The object to be created as protobuf message
 * @param cursor passing a null value for cursor causes the API to return the first element in the list
 */



rwdts_member_cursor_t*
rwdts_member_data_get_cursor(rwdts_xact_t*             xact,
                             rwdts_member_reg_handle_t reg);

/*!
 * Gets the next list element at a registration point
 * When the cursor is based on transaction, the next element at registration
 * point within the transaction is returned.
 *
 *
 * @param xact DTS transaction handle
 * @param reg Registration handle at which this object is being retrieved
 * @param pe The keyspec path entry  of the object which need to be deleted.
 * @param cursor opened using rwdts_member_data_get_cursor()
 *
 *
 * @returns ProtobufCMessage
 */
const ProtobufCMessage*
rwdts_member_data_get_next_list_element(rwdts_xact_t*             xact,
                                        rwdts_member_reg_handle_t regh,
                                        rwdts_member_cursor_t*    cursor,
                                        rw_keyspec_path_t**       keyspec);

/*!
 * Gets a list element at a specific key
 * The API will fail, if the registration is not a list boundary.
 *
 * @param xact DTS transaction handle
 * @param reg Registration handle at which this object is being retrieved
 * @param pe The keyspec path entry  of the object which need to be retrieved.
 * @param in_ks The keyspec point at which the message is expected.
 * @param out_keyspec out keyspec
 * @param out_msg out message
 *
 * @returns ProtobufCMessage
 */

rw_status_t
rwdts_member_data_get_list_element(rwdts_xact_t*              xact,
                                   rwdts_member_reg_handle_t  reg,
                                   rw_keyspec_entry_t*        pe,
                                   rw_keyspec_path_t**        out_keyspec,
                                   const ProtobufCMessage**   out_msg);

#ifndef __GI_SCANNER__
/*!
 * Gets a list element at a specific key
 * The API will fail, if the registration is not a list boundary.
 *
 * @param xact DTS transaction handle
 * @param reg Registration handle at which this object is being retrieved
 * @param mk The minikey of the object which need to be retrieved.
 */

const ProtobufCMessage*
rwdts_member_data_get_list_element_mk(rwdts_xact_t*                 xact,
                                      rwdts_member_reg_handle_t     reg,
                                      rw_schema_minikey_opaque_t*   mk);
#endif /* __GI_SCANNER__ */

/*!
 * Gets a list element at a specific key
 * The API will fail, if the registration is not a list boundary.
 *
 * @param xact DTS transaction handle
 * @param reg Registration handle at which this object is being retrieved
 * @param in_ks The keyspec point at which the message is expected.
 * @param out_msg The message associated with the requested keyspec
 * @param out_keyspec The fully qualified keyspec of the object which need to be retrieved.
 */

rw_status_t
rwdts_member_data_get_list_element_w_key(rwdts_xact_t*             xact,
                                         rwdts_member_reg_handle_t regh,
                                         rw_keyspec_path_t*        obj_keyspec,
                                         rw_keyspec_path_t**       out_keyspec,
                                         const ProtobufCMessage**  out_msg);

/*!
 *  Gets the number of elements at a specific registration
 *  This API will provide 0/1 for non listy objects
 *  For listy regisrtation this will provide the number of elements
 *  in the list
 *
 *  @param xact DTS transaction handle when present the function provides the count within the transaction
 *  @param reg  The registration for which the data count is requested.
 */

uint32_t
rwdts_member_data_get_count(rwdts_xact_t*             xact,
                            rwdts_member_reg_handle_t  reg);

/*!
 *  Gets the shard id and db number information for a keyspec and shard-key
 *  This API will provide list of shard id and DB number information
 *  for a keyspec and shard-key
 *
 *  @param apih              api handle
 *  @param keyspec           Keyspec
 *  @param shard_detail      Shard key information
 *  @param shard_db_num_info shard-id and db-number will be updated in this structure
 */
void
rwdts_member_get_shard_db_info_keyspec(rwdts_api_t*              apih,
                                       rw_keyspec_path_t*        keyspec,
                                       rwdts_shard_info_detail_t *shard_detail,
                                       rwdts_shard_db_num_info_t *shard_db_num_info);
/*!
 *  Delete the Shard table
 *  This API will delete the shard table information
 *
 *  @param apih              api handle
 */

void
rwdts_member_delete_shard_db_info(rwdts_api_t*    apih);

/*!
 *  Promote the registration from SUBSCRIBER to PUBLISHER
 *  This API will promote the registration from SUBSCRIBER
 *  to PUBLISHER
 *
 *  @param ud              registration handle
 */
void
rwdts_send_sub_promotion_to_router(void *ud);

__END_DECLS


#endif /* __RWDTS_MEMBER_API_H */
