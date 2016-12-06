

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


/**
 * @file rwmsg_request.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Request object for RW.Msg component
 *
 * @details The rwmsg_request_t object represents a single
 * request-response transaction, including addressing information,
 * transport parameters, and both request and response messages.  This
 * is true in both server contexts and client contexts. 
 */

#ifndef __RWMSG_REQUEST_H
#define __RWMSG_REQUEST_H


__BEGIN_DECLS

/*
 * Request object
 */

/* Prep and send requests: */

/**
 * Create a request to send out the given client or broclient channel.
 */
rwmsg_request_t *rwmsg_request_create(rwmsg_clichan_t *cc);

/**
 * Explicitly delete a request.  Note that only unsent (or cancelled)
 * requests should ever be deleted; once a request has been sent into
 * a clichan, it will be automatically deleted after the response
 * arrives.
 * @return TRUE if refct hit zero and req was freed
 */
rwmsg_bool_t rwmsg_request_release(rwmsg_request_t *req);

/**
 * Load this request payload into a request.  Use this for
 * RWMSG_PAYFMT_RAW or RWMSG_PAYFMT_TEXT.
 *
 * (For responses, see
 * rwmsg_request_send_response).
 */
rw_status_t rwmsg_request_set_payload(rwmsg_request_t *req,
				      const void *ptr,
				      uint32_t pay_sz);
/**
 * Fill in the signature for the request 
 */
void rwmsg_request_set_signature(rwmsg_request_t *req,
                                 rwmsg_signature_t *sig);
/**
 * Set the response callback for the request.
 */
void rwmsg_request_set_callback(rwmsg_request_t *req,
				rwmsg_closure_t *cb);

#if 0
 void rwmsg_request_set_priority(rwmsg_request_t *req,
				 rwmsg_priority_t pri); /* part of signature, so this is moot? */
 void rwmsg_request_set_coherency(rwmsg_request_t *req,
				  uint8_t coher);
 void rwmsg_request_set_aggregate(rwmsg_request_t *req,
				  rwmsg_bool_t on);

 /* For outstanding requests and/or responses: */
 void *rwmsg_request_get_userdata(rwmsg_request_t *req);
 uint32_t rwmsg_request_get_destination_count(rwmsg_request_t *req);
 rwmsg_signature_t *rwmsg_request_get_signature(rwmsg_request_t *req);
#endif

/**
 * Cancel this outstanding request.  There will be no callback, it
 * will just not be answered.  It is undefined if the destination got 
 * the request or not, or if it was processed and answered.
 */
rw_status_t rwmsg_request_cancel(rwmsg_request_t *req);

/**
 * Return the raw response payload from a request handle.  Do not
 * modify.
 * @return The pointer returned is only valid until the next call to
 * rwmsg_request_get_response_payload or the end of the callback.
 */
const void *rwmsg_request_get_response_payload(rwmsg_request_t *req, uint32_t *len_out); /* thr specific valid until next call or msg disposition */

#ifdef __RWMSG_BNC_RESPONSE_W_PAYLOAD
/**
 * Return the raw response payload from a request handle.  Do not
 * modify.
 * @return The pointer returned is only valid until the next call to
 * rwmsg_request_get_bnc_response_payload or the end of the callback.
 */
const void *rwmsg_request_get_bnc_response_payload(rwmsg_request_t *req, uint32_t *len_out); /* thr specific valid until next call or msg disposition */
#endif


/**
 * Return the payload format of the response in this request object.
 */
rwmsg_payfmt_t rwmsg_request_get_response_payfmt(rwmsg_request_t *req);


/**
 *  Vaguely opaque ID value for the client that sent a
 *  rwmsg_request_t, useful for servers to sift and sort by client.
 *  Treat as gibberish, memcmp the whole struct.
 */ 
typedef struct {
#define RWMSG_REQUEST_CLIENT_ID_MAGIC 0x4598aabb
  uint32_t _something_magic;
  union {
    uint64_t _something_local;
    uint32_t _something_else;
  };
} rwmsg_request_client_id_t;

/**
 * Return amostly-opqaue albeit flat identifier for the original client channel.
 */
rw_status_t rwmsg_request_get_request_client_id(rwmsg_request_t *req, rwmsg_request_client_id_t *cliid);

#define RWMSG_REQUEST_CLIENT_ID_VALID(c) ((c) && (c)->_something_magic == RWMSG_REQUEST_CLIENT_ID_MAGIC)

/**
 * Return the srvchan-assigned flow control ID found in this response,
 * or zero if none assigned.
 */
rwmsg_flowid_t rwmsg_request_get_response_flowid(rwmsg_request_t *req);

/**
 * Return the deserialized version of the payload.  
 *
 * @param buf For typical Protobuf messages of payload format PBAPI,
 * the value of buf is set to a pointer to the protoc-c C language
 * structure for the message.  This structure is valid until the next
 * call to _get_response in the thread.
 *
 * @return A status, if not RW_STATUS_SUCCESS there is no response,
 * it does not deserialize properly, etc.
 */
rw_status_t rwmsg_request_get_response(rwmsg_request_t *req,
				       const void ** buf); /* thread-specific valid only until next call */

/**
 * Return the bounce code, if any, from a completed request.
 * 
 * @param req The request to get the bounce code from.
 */
rwmsg_bounce_t rwmsg_request_get_response_bounce(rwmsg_request_t *req);

/**
 * Return the round-trip-time
 * returning milliseconds between response/bounce time and send time
 *
 * @param req The request to get the rtt
 */
uint64_t rwmsg_request_get_response_rtt(rwmsg_request_t *req);

#if 0
typedef struct {
  //   uint32_t destct;
  //   uint32_t actct;
  //   uint32_t rtt_ms;
  rwmsg_bounce_t bnc;
} rwmsg_response_detail_t;
/**
 * Return detail about a completed request (response or bounce).
 * 
 * @param req The request to get detail about.
 * @param det Pointer to a detail structure to be filled in with the detail.
 */
void rwmsg_request_get_response_detail(rwmsg_request_t *req,
				       rwmsg_response_detail_t *det);
 const char *rwmsg_request_get_response_text(rwmsg_request_t *req); /* thr specific valid until next call or msg disposition */
#endif

/* Receiving requests and sending responses from servers: */
/**
 * Return the raw request payload from a request handle.  Do not
 * modify.
 * @return The pointer returned is only valid until the next call to
 * rwmsg_request_get_request_payload, rwmsg_request_send_response, or
 * the end of the callback.
 */
const void *rwmsg_request_get_request_payload(rwmsg_request_t *req, uint32_t *len_out); /* thr specific valid until next call or msg disposition */

/**
 * Get the deserialized request.  For typicaly Protobuf messages of
 * payload format PBAPI, the value of buf will be set to a pointer to
 * the protoc-c C langauge strucure for the message.  This structure
 * is valid until the next call to rwmsg_request_get_request() in the
 * thread.
 *
 * @param req The request handle.
 *
 * @param buf The address of a pointer to be filled in with a pointer
 * to the request data.  Any value returned will magically free itself
 * as describedabove.
 *
 * @return A status code, if not RW_STATUS_SUCCESS there is no
 * request payload, it does not deserialize properly, etc.
 */
rw_status_t rwmsg_request_get_request(rwmsg_request_t *req,
				      void **buf); /* thread-specific valid only until next call */
#if 0
const char *rwmsg_request_get_request_text(rwmsg_request_t *req); /* thread specific valid until next call or msg disposition */
#endif

/**
 * Send this response to a request received from a srvchan.  This may be called from any thread as needed to implement concurrent request handling.
 * @return Generally there is no end to end flow control on responses,
 * however next hop flow control may still trigger an
 * RW_STATUS_BACKPRESSURE return condition.  Improved semantics in this
 * area TBD.
 */
rw_status_t rwmsg_request_send_response(rwmsg_request_t *req,
					const void *rsp,
					uint32_t rsp_sz);

/**
 * Send this response to a Prootbuf aka RWMSG_PAYFMT_PBAPI request
 * received from a srvchan.  This may be called from any thread as
 * needed to implement concurrent request handling.  
 * @return Generally
 * there is no end to end flow control on responses, however next hop
 * flow control may still trigger an RW_STATUS_BACKPRESSURE return
 * condition.  Improved semantics in this area TBD.
 */
rw_status_t rwmsg_request_send_response_pbapi(rwmsg_request_t *req,
					      const ProtobufCMessage *rsp);

typedef struct rwmemlog_buffer_t rwmemlog_buffer_t;
void rwmsg_request_memlog_hdr(rwmemlog_buffer_t **rwml_buffer,
                              rwmsg_request_t    *req,
                              const char         *func_name,
                              const int          line,
                              const char         *string);
__END_DECLS

#endif // __RWMSG_REQUEST_H
