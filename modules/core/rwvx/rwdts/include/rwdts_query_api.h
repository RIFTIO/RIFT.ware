
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
 * @file rwdts_query_api.h
 * @brief Query API for RW.DTS
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */

#ifndef __RWDTS_QUERY_API_H
#define __RWDTS_QUERY_API_H

#include <rwlib.h>
#include <rwtrace.h>
#include <rwtasklet.h>
#include <rwsched.h>
#include <rwsched_object.h>
#include <rwsched_queue.h>
#include <rwsched_source.h>
#include <rwsched/cfrunloop.h>
#include <rwsched/cfsocket.h>
#include <rwmsg.h>
#include <rw_keyspec.h>
#include <rw_sklist.h>
#include <uthash.h>
#include <rwdts.h>
#include <rwdts_keyspec.h>
#include <rwdts_api_gi_enum.h>
#include <rwdts_api_gi.h>

__BEGIN_DECLS

/*
 * harcoded name of the router -- temporary this will go away
 */
#define RWDTS_HARDCODED_ROUTER_ID 1


typedef struct rwdts_appconf_s rwdts_appconf_t;
typedef struct rwdts_api_s rwdts_api_t;
typedef struct rwtasklet_info_s rwtasklet_info_t;
typedef struct rwdts_xact_s rwdts_xact_t;
typedef struct rwdts_xact_block_s rwdts_xact_block_t;
typedef struct rwdts_member_data_elem_s rwdts_member_data_elem_t;
typedef struct rwdts_query_result_s rwdts_query_result_t;
typedef struct rwdts_query_error_s rwdts_query_error_t;
typedef struct rwdts_state_change_cb_s rwdts_state_change_cb_t;
typedef struct rwdts_event_cb_s rwdts_event_cb_t;
typedef struct rwdts_shard_info_detail_s rwdts_shard_info_detail_t;
typedef struct rwdts_shard_db_num_info_s rwdts_shard_db_num_info_t;
typedef struct rwdts_member_event_cb_s  rwdts_member_event_cb_t;
typedef struct rwdts_shard_info_s rwdts_shard_info_t;
typedef struct rwdts_member_reg_handle_s*  rwdts_member_reg_handle_t;
typedef struct rwdts_member_registration_s rwdts_member_registration_t;

void rwdts_xact_unref(rwdts_xact_t *boxed, const char *file, int line);
rwdts_xact_t *rwdts_xact_ref(rwdts_xact_t *boxed, const char *file, int line);

void rwdts_xact_block_unref(rwdts_xact_block_t *boxed, const char *file, int line);
rwdts_xact_block_t *rwdts_xact_block_ref(rwdts_xact_block_t *boxed, const char *file, int line);


/*!
 *  Create a block.  C API uses this one; Gi has a wrapper
 *  (block_create) with other glue.
 */
rwdts_xact_block_t*
rwdts_xact_block_create(rwdts_xact_t *xact);


/*!
 * internal API for tests -  do not use - Use rwdts_api_init() instead
 */
rwdts_api_t*
rwdts_api_init_internal(rwtasklet_info_ptr_t           rw_tasklet_info,
                        rwsched_dispatch_queue_t       q,
                        rwsched_tasklet_ptr_t          tasklet,
                        rwsched_instance_ptr_t         sched,
                        rwmsg_endpoint_t*              ep,
                        const char*                    my_path,
                        uint64_t                       router_idx,
                        uint32_t                       flags,
                        const rwdts_state_change_cb_t* cb);
/*!
 * Set the schema for this API handle
 *
 * @param apih DTS API handle
 * @param schema The yangpbc schema
 *
 */
void rwdts_api_set_ypbc_schema(rwdts_api_t *apih, const rw_yang_pb_schema_t* schema);

/*!
 * Get the schema associated with this API handle
 *
 * @param apih DTS API handle
 *
 */
const rw_yang_pb_schema_t* rwdts_api_get_ypbc_schema(rwdts_api_t *apih);

/*!
 * Add a ypbc schema to the list of schema's
 * in the API handle.
 *
 * @param apih DTS API handle
 * @param schema The ypbc schema to add
 *
 * @return RW_STATUS_SUCCESS or a failure code
 */
rw_status_t rwdts_api_add_ypbc_schema(rwdts_api_t*  apih,
                                      const rw_yang_pb_schema_t* schema);


/*!
 * Deinitializes the DTS API handle,
 * The passed API handle is and associated resources are released.
 * The caller should not use the handle after this API call.
 *
 * @return RW_STATUS_SUCCESS or a failure code
 */
rw_status_t rwdts_api_deinit(rwdts_api_t *apih);


typedef struct rwdts_xact_result_s rwdts_xact_result_t;

enum rwdts_xact_state_e {
  RWDTS_XACT_STATE_INVALID = 0,
  RWDTS_XACT_STATE_base = 0x1212,
  RWDTS_XACT_STATE_UNSENT,
  RWDTS_XACT_STATE_PENDING,
  RWDTS_XACT_STATE_DONE,
  RWDTS_XACT_STATE_ABORT,
  RWDTS_XACT_STATE_end,

  RWDTS_XACT_STATE_FIRST = RWDTS_XACT_STATE_base+1,
  RWDTS_XACT_STATE_LAST = RWDTS_XACT_STATE_end-1,
  RWDTS_XACT_STATE_COUNT = RWDTS_XACT_STATE_end - RWDTS_XACT_STATE_base-1
};
typedef enum rwdts_xact_state_e rwdts_xact_state_t;


typedef struct rwdts_xact_block_s rwdts_xact_block_t;

enum rwdts_xact_query_result_state_e {
  RWDTS_XACT_RESULT_STATE_INVALID = 0x0,
  RWDTS_XACT_RESULT_STATE_base = 0x4560,
  RWDTS_XACT_RESULT_STATE_PENDING,
  RWDTS_XACT_RESULT_STATE_DONE,
  RWDTS_XACT_RESULT_STATE_end,

  RWDTS_XACT_RESULT_STATE_FIRST = RWDTS_XACT_RESULT_STATE_base+1,
  RWDTS_XACT_RESULT_STATE_LAST = RWDTS_XACT_RESULT_STATE_end-1,
  RWDTS_XACT_RESULT_STATE_COUNT = RWDTS_XACT_RESULT_STATE_end-RWDTS_XACT_RESULT_STATE_base-1
};
typedef enum rwdts_xact_query_result_state_e rwdts_xact_query_result_state_t;

/*!
 * Transatction result
 */

struct rwdts_xact_result_track_s {
  RWDtsXactResult *result;
  rw_dl_element_t res_elem;
};
typedef struct rwdts_xact_result_track_s rwdts_xact_result_track_t;


/*!
 * transaction  status
 */
struct rwdts_xact_status_s {
  RWDtsXactMainState status;   /*!< Overall transaction state */
  bool            responses_done;     /*!< If true, there will **NOT** be more events and/or results on this xact. */
  bool            xact_done;     /*!< If true, the xact can be destroyed. */
  rwdts_xact_block_t *block;     /*!< block pointer */
  //...
  // exceptions / errors are where?
  //...
  int ref_cnt;
};

/*!
 * transaction  state
 */
enum rwdts_xact_result_state_e {
  RWDTS_XACT_COMMIT,
  RWDTS_XACT_ABORT
};

/*!
 * transaction result structure
 */
struct rwdts_xact_result_s {
  /* This struct adds little, skip it and be able to return direct from response for pb cases? */
  uint32_t more_results:1;
  uint32_t more_queries:1;
  uint32_t more_blocks:1;
  uint32_t _pad:29;

  enum rwdts_xact_result_state_e state;
  uint32_t blockidx;
  // X of Y queries?
  // ..??

  rw_dl_element_t elem;
  RWDtsQueryResult *result;
};
typedef struct rwdts_xact_result_s rwdts_xact_result_t;

/*!
 * transaction callback result structure
 */
struct rwdts_xact_callback_result_s {
   uint32_t          more_results:1;
   uint32_t          _pad:31;
   uint32_t          n_results;
   ProtobufCMessage  **results;
};

typedef struct  rwdts_xact_callback_result_s rwdts_xact_callback_result_t;

typedef void
(*rwdts_query_event_cb_routine)(rwdts_xact_t* xact,
                                rwdts_xact_status_t* status,
                                void*         user_data);
typedef void
(*rwdts_api_state_changed)(rwdts_api_t*  apih,
                           rwdts_state_t state,
                           void*         user_data);
/*!
 * Transaction query event callback
 * DTS invokes the cb routine registered with the ud on query results/status changes
 */
struct rwdts_event_cb_s {
  rwdts_query_event_cb_routine cb;
  const void*                  ud;
};

/*!
 *  The DTS state change callback structure for rwdts_api_init.
 *  The application can register a callback  and user  data while
 *  calling rwdts_api_init(). DTS will invoke this callback with
 *  the registered user data on DTS state changes.
 *
 */
struct rwdts_state_change_cb_s {
  rwdts_api_state_changed cb;
  void*                   ud;
};

/*!
 * INTERNAL Begin a DTS xact block and Query based on the the passed keyspec and  protobuf message and appends the
 * block to the ongoing transaction.
 * The passed protobuf is owned by the transaction.
 * ATTN  - Should the passed objects be reference counted and shared?
 *
 * @param xact    DTS xact handle
 * @param key     The key associated with the query - API copies the contents
 * @param xact_parent  Parent transaction if this is a sub transaction, null otherwise
 * @param msg     The message associated with this protobuf -- must be present  - Api copies the contents of the message
 * @param corrid  A correlation id that the caller can use to match responses.
 * @param dbg     DTS Debug entry - Caller owns the param
 * @param action  DTS  action
 * @param cb      Callback  for the query. API copies the contents - caller owns cb.
 * @param flags   Flags associated with this query
 *
 * @return  A status indicating success or failure.
 *
 */
rw_status_t
rwdts_advise_append_query_proto(rwdts_xact_t*                xact,
                                const rw_keyspec_path_t*     keyspec,
                                rwdts_xact_t*                xact_parent,
                                const ProtobufCMessage*      msg,
                                uint64_t                     corrid,
                                RWDtsDebug*                  dbg,
                                RWDtsQueryAction             action,
                                const rwdts_event_cb_t*      cb,
                                uint32_t                     flags,
                                rwdts_member_registration_t* reg);



/*! Deintializes a DTS transaction handle and frees the passed structure
 *
 *
 *  @return a status indicating success or failure
 */

rw_status_t rwdts_xact_deinit(rwdts_xact_t *xact);



/*! Add a query block to a (sub)transaction.
 *  Member-side subx in future sprint; mere appending more
 *  queries to the toplevel transaction probably up front. */

rwdts_xact_t *rwdts_xact_append(rwdts_api_t*      apih,
                                rwdts_xact_t*     xact,
                                RWDtsXactBlock*   block_in,
                                rwdts_event_cb_t* cb);


/*!
 * This API call finalizes a transaction block.
 *
 * After this API is called, no more top level explicit subtransactions can be added.
 * There could still be implicitly generated subtrasanactions after this call.
 * The get_result on a this transaction can be called after this API is called.
 *
 * @param xact DTS transaction block handle
 *
 * @return status RW Status indication
 *
 */
rw_status_t rwdts_xact_block_end(rwdts_xact_block_t *block);

/*!
 * Get the status of this transaction
 *
 * This API provides diagnostic information and progress detail on a currently running transaction
 * and/or subtransaction. The transaction can be in any state other  than destroyed.
 *
 * @param xact DTS transaction handle
 * @param status_out status of the transaction
 *
 * @return status RW Status indication
 *
 */
rw_status_t rwdts_xact_get_status(rwdts_xact_t*        xact,
                                  rwdts_xact_status_t* status_out);


/*!
 * Gets the result of the in responder's format
 *
 * This is the non-blocking variant of the API
 *
 * @param xact DTS transaction handle
 * @param corrid when corrid is non zero this API returns results for the query with the passed corrid
 * @param result results -- DTS owns the results -- will be maintained until xact is freed
 *
 * @return status RW Status indication
 */
rw_status_t rwdts_xact_get_result(rwdts_xact_t*         xact,
                                  uint64_t              corrid,
                                  rwdts_xact_result_t** const result);

/*!
 * Gets the result of the transaction as an XML
 *
 * This is the blocking variant of rwdts_xact_get_result_xml()
 *
 * @param xact DTS transaction handle
 * @param corrid when corrid is non zero this API returns results for the query with the passed corrid
 * @param result results -- DTS owns the results which will be freed when the transaction is freed.
 *
 * @return status RW Status indication
 */
rw_status_t rwdts_xact_get_result_xml(rwdts_xact_t*         xact,
                                      uint64_t              corrid,
                                      rwdts_xact_result_t** const result);


/*!
 * Get the result of the available protos for result
 * @param  xact DTS transaction handle
 * @param when block is not NULL we return result for queries within particular block
 * @param corrid when corrid is non zero this API returns results for the query with the passed corrid
 * @return the number of results available in the transaction immediately available through rwdts_xact_get_result_pb().
 *         zero when there are no results available.
 *
 */
uint32_t
rwdts_xact_get_available_results_count(rwdts_xact_t* xact,
                                       rwdts_xact_block_t   *blk,
                                       uint64_t      corrid);

/*!
 * Get the number of errors in the transaction
 * @param  xact DTS transaction handle
 * @param when block is not NULL we return result for queries within particular block
 * @param corrid when corrid is non zero this API returns results for the query with the passed corrid
 * @return the number of results available in the transaction immediately available through rwdts_xact_get_result_pb().
 *         zero when there are no results available.
 *
 */
uint32_t
rwdts_xact_get_available_errors_count(rwdts_xact_t* xact,
                                      uint64_t      corrid);

/*!
 * Gets the errors for the transaction
 *
 * This is the non-blocking variant of the API
 *
 * @param xact DTS transaction handle
 * @param corrid when corrid is non zero this API returns errors for the query with the passed corrid
 * @param error -- DTS owns the errors -- will be maintained until xact is freed
 *
 * @return status RW Status indication
 *         Call rwdts_xact_free_error to destroy the returned object
 *
 * Call the rwdts_xact_get_available_errors_count before calling this function to reset the
 * index used to return the next error in the list
 */
rw_status_t rwdts_xact_get_error(rwdts_xact_t*         xact,
                                 uint64_t              corrid,
                                 rwdts_query_error_t** const error);

/*!
 *  Get the error cause
 *  @param  query_error The query error structure
 *  @return rw_status_t corresponding to the query error.
 */
rw_status_t
rwdts_xact_get_error_cause(const rwdts_query_error_t* query_error);

/*!
 *  Get the error string
 *  @param  query_error The query error structure
 *  @return string corresponding to the query error.
 *           The API owns the string
 */
const char*
rwdts_xact_get_error_errstr(const rwdts_query_error_t* query_error);


/*!
 *  Get the keyspec associated with the error
 *  @param  query_error The query error structure
 *  @return  Keyspec associated with the error. The API owns the keyspec
 */
const rw_keyspec_path_t*
rwdts_xact_get_error_keyspec(const rwdts_query_error_t* query_error);

/*!
 *  Get the xpath associated with the error
 *  @param query_error The query error structure.
 *  @return a string containing the xpath information for the query
 *          The API owns the string
 */
const char*
rwdts_xact_get_error_xpath(const rwdts_query_error_t* query_error);

/*!
 *  Get the keystr associated with the error
 *  @param query_error The query error structure.
 *  @return a string containing the key information for the query
 *          The API owns the string
 */
const char*
rwdts_xact_get_error_keystr(const rwdts_query_error_t* query_error);

/*!
 * Aborts a transaction
 *
 * This API aborts and destroys a transaction.  Before rwdts_xact_end()  has been called,
 * transactions may be reliably aborted. After rwdts_xact_end()  has been called
 * abort is best-effort.
 *
 * @param xact DTS transaction handle
 * @param status rw_Status_t to say why abort is called
 * @param errstr Error string for more information on abort
 *
 * @return status RW Status indication
 */
rw_status_t rwdts_xact_abort(rwdts_xact_t* xact,
                             rw_status_t status,
                             const char*  errstr);

/*!
 * Aborts a transaction block
 *
 * This API aborts and destroys a transaction.  Before rwdts_xact_end()  has been called,
 * transactions may be reliably aborted. After rwdts_xact_end()  has been called
 * abort is best-effort.
 *
 * @param xact DTS transaction block handle
 * @param status rw_Status_t to say why abort is called
 * @param errstr Error string for more information on abort
 *
 * @return status RW Status indication
 */
rw_status_t rwdts_xact_block_abort(rwdts_xact_block_t* block,
                                   rw_status_t status,
                                   const char*  errstr);

/*!
 * Gets the result of the transaction as a protobuf
 * The serialized protobuf data is converted to protobuf using the descriptor
 * passed to this API.
 *
 * @param xact      DTS Transaction handle
 * @param corrid    When corrid is non zero this API returns results for the query with the passed corrid
   @param desc      The protobuf descriptor for the expected results
 * @param msgs      An array of result protobufs of length size_result - The array and contents are owned by DTS
 * @param keyspecs  Used to return  an array of keyspecs corresponding to the result array when present.
 *                  Both msgs and keyspecs arrays should be of the same size  as specified by size.
 * @param index     The index at which the results need to be fecthed. Passing 0 as index will fetch all available results (upto size_result).
 *                  when the no of results exceeds size_result use index  to iterate and fetch the next set of results.
 * @param size      The size of the passed result array and keyspec array.
 * @param n_result  The number of elements in the returned result array on successfull execution of the API.
 * @return status   A status indication of whether ressult retrival was successfull.
 */
rw_status_t rwdts_xact_get_result_pb( rwdts_xact_t*                     xact,
                                      uint64_t                          corrid,
                                      const ProtobufCMessageDescriptor* desc,
                                      ProtobufCMessage**                msgs,
                                      rw_keyspec_path_t**                    keyspecs,
                                      size_t                            index,
                                      size_t                            size_result,
                                      uint32_t*                         n_result);

/*!
 *  Get the number of blocks in a transaction
 *
 *
 * @param xact DTS transaction handle
 *
 * @return  The number of blocks in this transaction
 */
uint32_t
rwdts_xact_get_block_count(rwdts_xact_t *xact);

/*!
 * resets all ap related statistics
 *
 * @param apih DTS API handle
 */
void
rwdts_api_reset_stats(rwdts_api_t *apih);


/*!
 * Get the status of this transaction block
 *
 * This API provides diagnostic information and progress detail on a currently running transaction
 * and/or subtransaction block. The transaction can be in any state other  than destroyed.
 *
 * @param xact DTS transaction block handle
 * @param status_out status of the transaction
 *
 * @return status RW Status indication
 *
 */


rw_status_t rwdts_xact_block_get_status(rwdts_xact_block_t*  block,
                                        rwdts_xact_status_t* status_out);

rwdts_member_reg_handle_t
rwdts_member_reg_handle_ref(rwdts_member_reg_handle_t boxed);

void
rwdts_member_reg_handle_unref(rwdts_member_reg_handle_t boxed);


__END_DECLS


#endif /* __RWDTS_QUERY_API_H */
