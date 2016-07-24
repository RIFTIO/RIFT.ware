

/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_clichan.h
 * @brief Client channel for RW.Msg component
 *
 * @details
\anchor clichan
 * ## Overview of Client Channels
 * A client channel is the handle needed to send requests to a
 * particular rwmsg_destination_t.  Requests are created on a clichan,
 * then submitted with rwmsg_clichan_send(), and, if the channel is
 * bound to a scheduler artifact, response callbacks are dispatched as
 * responses arrive.
 *
 * ### Flow Control
 * A clichan provides optional window-based flow control, specific to
 * each srvchan in each request's destination+method.  This consists
 * of an actively managed transmit window value available in the
 * clichan.  If "feedme" callbacks are registered, this transmit
 * window will be rigorously enforced on that clichan and flow.  If
 * "feedme" callbacks are not registered, the client may send any
 * number of requests.
 *
 * With enforced flow control, at any given time, certain requests to
 * certain destinations may be temporarily unsendable.  Similarly,
 * outgoing buffers and the channel to the broker can assert flow
 * control.  In both cases send calls will return
 * RW_STATUS_BACKPRESSURE.
 *
 * After a send attempt has failed with RW_STATUS_BACKPRESSURE, a
 * "feedme" event will be generated when requests to that
 * destination+method might work again.  The callback for
 * these flow control events can be registered with
 * rwmsg_clichan_feedme_callback().
 *
 * ## PBAPI Protobuf Services
 * The Client side of a Service as defined in rift-extended Protobuf
 * declarations may be atached to a clichan, enabling use of the
 * `rift-protoc-c` generated client API functions.

 *
 * ### Example
 * Given the Protobuf declaration:
~~~{.proto}
service Test {
  option (rw_srvopts) = { srvnum:100 };
  rpc Increment(test_req) returns(test_rsp) {
    option (rw_methopts) = { methno:1 blocking:true};
  };
  rpc Fail(test_req) returns(test_nop) {
    option (rw_methopts) =  { methno:2 pri:RW_METHPRI_MEDIUM blocking:true};
  };
}
~~~
 * The generated code will include a Test_Client type which can be
 * initialized and connected to a clichan as shown below.
~~~{.c}
rwmsg_clichan_t *cc = rwmsg_clichan_create(endpoint);
rwmsg_clichan_bind_rwsched_cfrunloop(sc, rwsched_taskletinfo);

Test_Client mycli;
TEST__INITCLIENT(&mycli);
rwmsg_clichan_add_service(cc, &mycli);
~~~
  * The added Protobuf service client may then be used with the `rift-protoc-c` provided client functions:
~~~{.c}
rw_status_t s;
rwmsg_request_t *rwreq;
rwmsg_destination_t *dest = rwmsg_destination_create(endpoint, 
                                                     RWMSG_ADDRTYPE_UNICAST,
                                                     "/MY/NAMESERVICE/PATHNAME/1");
rwmsg_closure_t clo = { .pbrsp = (rwmsg_pbapi_cb)myincrementcb, .ud=NULL };

// Send a regular asynchronous request
TestReq input;
test_req__init(&input);         // The generated constructor for TestReq
input.value = 1;
s = test__increment(&mycli, dest, &input, clo, NULL);
~~~
* Optionally `&rwreq` could be passed as the last argument to capture the request handle for inspection, cancellation, etc.
*
* The response callback `myincrementcb` looks like this:
~~~{.c}
static void myincrementcb(TestRsp *rsp, rwmsg_request_t *rwreq, void *ud) {
  if (rsp) {
    printf("%d", rsp->value);
  } else {
    // Failure.  Interrogate rwreq for status, RTT, etc.
  } 
}
~~~
* To send blocking requests with the PBAPI stubs:
~~~{.c}
// Send a blocking request instead.  The _b binding only exists for rpc clauses 
// with blocking:true in the protobuf declaration.
s = test__increment_b(&mycli, dest, &input, &rwreq);

TestRsp *rsp;
s = rwmsg_request_get_response(rwreq, &rsp);
// rsp is valid until the next get_response or blocking call in this thread
~~~
 *
 * ## RWSched Interaction
 * When bound to a rwsched_CFRunloop, synchronous aka blocking
 * requests are available with rwmsg_clichan_send_blocking().  These
 * block waiting for a response, and prevent other rwsched_CFRunloop
 * timer and socket sources from firing; extended support for all
 * CFRunloop sources is planned.  Events in other threads driven by
 * rwsched's libdispatch component do not block while in
 * rwmsg_clichan_send_blocking().
 *
 * ### Threading
 * Concurrent use of a single clichan from multiple threads
 * is not supported.  Each thread, scheduler queue, or shard should
 * have its own.
 */

#ifndef __RWMSG_CLICHAN_H
#define __RWMSG_CLICHAN_H


__BEGIN_DECLS


/* 
 * ClientChannel object
 */


/**
 * Create a client channel.
 * 
 * @return The new channel
 */
rwmsg_clichan_t *rwmsg_clichan_create(rwmsg_endpoint_t *ep);


/**
 * Bind this clichan to an rwsched queue.
 *
 * @param q The target queue to which channel events, most notably
 * response arrival callbacks, will be directed.  Response and other
 * callbacks will occur within the context of this queue; if a serial
 * queue then other events will not occur in parallel.  NULL may be
 * used to clear the binding.
 *
 */
rw_status_t rwmsg_clichan_bind_rwsched_queue(rwmsg_clichan_t *cc, rwsched_dispatch_queue_t q);

/**
 * Bind this clichan to the tasklet context from rwsched/cfrunloop.
 * This will result in events executing in the cfrunloop context aka
 * the main rwsched dispatch queue, and enable blocking requests.
 * Blocking requests will work from withing the cfrunloop execution
 * context only.
 *
 * @param taskletifo The rwsched tasklet tracking object.  NULL may be
 * used to clear the binding.
 */
rw_status_t rwmsg_clichan_bind_rwsched_cfrunloop(rwmsg_clichan_t *cc, rwsched_tasklet_ptr_t taskletifo);


/*?? implicit: void rwmsg_clichan_add_signature(rwmsg_clichan_t cc, rwmsg_signature_t sig);*/

/**
 * Convenience name for casting protobuf response callbacks to the generic protobuf message type
 */
typedef void (*rwmsg_pbapi_cb)(ProtobufCMessage *, rwmsg_request_t *, void *);

/**
 * Connect a RW Protobuf "PBAPI" service object to the clichan.
 * rift-protoc-c's generated method binding functions may then be
 * called with the client service object, and the requests will go
 * through this channel.
 *
 * @param srv A protoc-c declared service object which has been
 * initialized with the __INITCLIENT() macro.
 */
void rwmsg_clichan_add_service(rwmsg_clichan_t *cc, ProtobufCService *srv);

/* Default settings attached to the channel so requests don't have to
   set them every time. */
void rwmsg_clichan_setdef_callback(rwmsg_clichan_t *cc,
                                   rwmsg_signature_t *sig,
                                   rwmsg_closure_t *cb);

void rwmsg_clichan_setdef_coherency(rwmsg_clichan_t *cc, uint8_t co);
/* ?? part of sig: void rwmsg_clichan_setdef_priority(rwmsg_clichan_t cc, rwmsg_priority_t pri); */
/* ?? fundamentaly a per-req thing: void rwmsg_clichan_setdef_aggregate(rwmsg_clichan_t cc, rwmsg_bool_t agg); */

#if 0
/* TBD The default channel, use for send to unknown, or even for most
   xmits from main tasklet thread.  A dedicated rwmsg_clistream_t
   object in the dest is used. */
#define RWMSG_CLICHAN_DEFAULT (NULL)
#endif

/**
 * Send a request into a client channel.  Flow control state or other
 * conditions may prevent success of the send, in which case it is up
 * to the caller to resend the request later, or delete the request
 * now and do something else.
 * @return Status of the send, notably might be RW_STATUS_BACKPRESSURE.
 */
rw_status_t rwmsg_clichan_send(rwmsg_clichan_t *cc,
			       rwmsg_destination_t *dest,
			       rwmsg_request_t *req);

rwmsg_bool_t rwmsg_clichan_can_send(rwmsg_clichan_t *cc, 
                                    rwmsg_destination_t *dest, 
                                    rwmsg_signature_t *sig);

uint32_t rwmsg_clichan_can_send_howmuch(rwmsg_clichan_t *cc,
                                        rwmsg_destination_t *dest,
                                        uint32_t methno,
                                        rwmsg_payfmt_t payt);

/*!
 * Returns SUCCESS if the state is NN_CONNECTED; OR the cc is local-only
 * otherwise returns NOTCONNECTED
 */
rw_status_t rwmsg_clichan_connection_status(rwmsg_clichan_t *cc);

/**
 * Set a callback to be called after an RW_STATUS_BACKPRESSURE error
 * when the broker socket is once again available to write to or the
 * destination-specific transmit window opens up.
 *
 * Can only be called on a clichan which is bound to a scheduler
 * context, either a rwsched queue or to a cfrunloop.  Will assert
 * otherwise.
 *
 * MUST BE CALLED FROM WITHIN THE BOUND SCHEDULING CONTEXT -- either
 * in the cfrunloop thread, or in an event fired by the bound rwsched
 * serial queue.  Calling this directly from a response callback will
 * always be the proper context.  (This is also the obvious place to
 * learn the flowid needed).
 *
 * See also rwmsg_destination_feedme_callback() to set a feedme
 * callback for the default flow used before responses to a particular
 * destination-signature have been received.
 *
 * @param dest Which destination for this callback.  Use NULL to set a
 * default callback.
 *
 * @param flowid Flow identifier defining which rwmsg_srvchann_t in
 * the destination to callback for.  This value can be obtained with
 * the rwmsg_request_get_response_flowid() call once the first
 * response has arrived.
 *
 * @param cb void feedme(void *ud, rwmsg_destination_t dest,
 * rwmsg_flowid_t flowid) member of the standard closure struct, and
 * ud value to be returned with the callback.  The dest and flowid
 * values to the callback indicates which destination(s) requests
 * might now succeed.  Note that the returned destination object will
 * be a copy; a string comparison of the pathname is necessary to
 * identify matches (FUGLY -> FIXME, add dest compare func that uses
 * the hash).
 */
rw_status_t rwmsg_clichan_feedme_callback(rwmsg_clichan_t *cc,
					  rwmsg_destination_t *dest,
					  rwmsg_flowid_t flowid,
					  rwmsg_closure_t *cb);


/**
 * Send a request into a client channel and wait for the response.
 *
 * @return Status of the send, notably might be RW_STATUS_BACKPRESSURE or
 * RW_STATUS_TIMEOUT.  In the timeout case the request ceases to
 * be valid and should be abandoned.  (FUGLY -> FIXME)
 */

rw_status_t rwmsg_clichan_send_blocking(rwmsg_clichan_t *cc,
					rwmsg_destination_t *dest,
					rwmsg_request_t *req);
/**
 * Receive response message(s) from a client channel.  The appropriate
 * callbacks will be called with the request handles for each
 * response.  Does not block.  Normally scheduler binding induces
 * clichans to recv automagically, so this function is not usually
 * called by app code.
 * @return The number of responses received on the clichan.  Can be zero.
 */
uint32_t rwmsg_clichan_recv(rwmsg_clichan_t *cc);

/**
 * Stop the channel and release it with rwmsg_clichan_release().  Call
 * once to match rwmsg_clichan_create().
 */
void rwmsg_clichan_halt(rwmsg_clichan_t *cc);

/**
 * Decrement reference count for a clichan, free it when there are no
 * more references.  App code probably should use rwmsg_clichan_halt().
 */ 
void rwmsg_clichan_release(rwmsg_clichan_t *cc);

/**
 * Return the endpoint from a clichan.  Bumps reference count -- release with rwmsg_endpoint_release()!!!
 */
rwmsg_endpoint_t *rwmsg_clichan_get_endpoint(rwmsg_clichan_t *cc);

#if 0
/* Function underlying generated client-side stubs from protoc-c, actual declaration lives in rift-protobuf-c.h */
#define RWMSG_CLICHAN_PROTOC_FLAG_BLOCKING (1<<31)
rw_status_t rwmsg_clichan_send_protoc_internal(rwmsg_clichan_t *cc,
					       rwmsg_destination_t dest,
					       uint32_t flags,
					       ProtobufCService *srv,
					       uint32_t methidx, /* not methno, idx */
					       ProtobufCMessage *msg,
					       rwmsg_closure_t *closure,
					       rwmsg_request_t **req_out);
#endif

/* Also probably need routines to direct pre-packed protobufs, XML
   messages, etc into service+method destinations.  Mainly for
   bindings in python etc? or use in the broker?  */

/* Reset the stream cache assiciated with the pathhash on this cc */
void rwmsg_clichan_stream_reset(rwmsg_clichan_t *cc,
                                rwmsg_destination_t *dt);


__END_DECLS

#endif // __RWMSG_CLICHAN_H
