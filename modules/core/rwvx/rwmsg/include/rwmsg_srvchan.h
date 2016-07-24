
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_srvchan.h
 * @brief Server channels for RW.Msg
 *
 * @details
\anchor srvchan
 * ## Overview of Server Channels
 * Requests arrive on server channels via callback.  Each request must
 * be responded to, typically with rwmsg_request_send_response().
 * Arbitrary methods and/or IDL-declared services may be added to a
 * service channel with rwmsg_srvchan_add_method() and
 * rwmsg_srvchan_add_service().  
 *
 * All requests handled by a single srvchan are flow controlled
 * together as a unit.
 *
 * ## RWSched Binding
 *
 * Each srvchan should be bound to a single scheduler-provided
 * execution context, eg an rwsched target queue, a cfrunloop, etc.
 * It is possible to bind multiple srvchans to a single execution
 * context.  This is advisable for functionally orthogonal services
 * which need to share an execution context but have independent flow
 * control.
 * 
 * See rwmsg_srvchan_bind_rwsched_queue() and rwmsg_srvchan_bind_rwsched_cfrunloop().
 *
 * ### Threads
 *
 * There is intended to be minimal sharing between srvchans, to reap
 * the advantages of thread affinity and avoid memory and cache
 * performance pitfalls on NUMA platforms.  It is suggested to create
 * and bind at least one srvchan per thread or rwsched queue or
 * rwsched cfrunloop context.  It is permissible to send responses to
 * a srvchan from any arbitrary thread.
 *
 * ## Flow Control
 * 
 * Srvchans may implement flow control by calling
 * rwmsg_srvchan_pause() and rwmsg_srvchan_resume().
 *
 * ## PBAPI / Protobuf Service Binding
 *
 * Services may be declared in Protobuf language, using rift
 * extensions, and bound directly to srvchans and clichans to enable
 * painless connection of all methods, generation of client stub
 * functions, etc.
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
 * The generated code will include a Test_Service type which can be
 * initialized and connected to a srvchan as follows:
~~~{.c}
rwmsc_srvchan_t *sc = rwmsg_srvchan_create(ep);
rwmsg_srvchan_bind_rwsched_cfrunloop(sc, rwsched_taskletinfo);

Test_Service mysrv;
TEST__INITSERVER(&mysrv, mypfx_);
rwmsg_srvchan_add_service(sc, "/MY/NAMESERVICE/PATHNAME/1", &mysrv, self);
~~~
  * The added Protobuf service will call, in the rwsched_CFRunloop context, these two callbacks:
~~~{.c}
void mypfx_increment(Test_Service *mysrv, 
                     const TestReq *input,
		     void *self,
                     TestRsp_Closure clo, 
                     rwmsg_request_t *rwreq) {
  rw_status_t s;
  TestRsp rsp;
  test_rsp__init(&rsp);       // The generated constructor for TestRsp
  rsp.status = 1;
  s = clo(&rsp, rwreq);
  return;
  // Or equivalently
  s = rwmsg_request_send_response_pbapi(rwreq, (ProtobufCMessage*)&rsp);
}
void mypfx_fail(Test_Service *mysrv, 
                const TestReq *input, 
		void *self,
                TestNop_Closure clo, 
                rwmsg_request_t *rwreq) {
  //...
}
~~~
 *
 */

#ifndef __RWMSG_SRVCHAN_H
#define __RWMSG_SRVCHAN_H


__BEGIN_DECLS

#if 1
/* hack for doxy parser */
#endif

/*
 * ServerChannel object
 */

/**
 * Create a server channel.  
 *
 * It comes with a single reference on it; call rwmsg_srvchan_halt to
 * terminate, deference, and eventually free.
 */
rwmsg_srvchan_t *rwmsg_srvchan_create(rwmsg_endpoint_t *ep);


/**
 * Unbind all methods from a srvchan, including all protobuf service
 * methods.  This must be called before the final release, as each
 * bound method or protobuf service also has a references to the
 * srvchan.
 *
 * This also releases a reference; call this once to match the
 * rwmsg_srvchan_create() call.
 *
 */
void rwmsg_srvchan_halt(rwmsg_srvchan_t *sc);

/**
 * Bind this srvchan to an rwsched queue.
 *
 * @param q The target queue to which channel events, most notably
 * request arrival callbacks, will execute.  NULL may be used to clear
 * the binding.
 */
rw_status_t rwmsg_srvchan_bind_rwsched_queue(rwmsg_srvchan_t *sc, rwsched_dispatch_queue_t q);

/**
 * Bind this srvchan to rwsched's taskletized cfrunloop.  This will
 * result in events executing in the cfrunloop context.  Blocking
 * requests will work from withing the cfrunloop execution context
 * only.
 *
 * @param taskletifo The rwsched tasklet tracking object.  NULL may be
 * used to clear the binding.
 */
rw_status_t rwmsg_srvchan_bind_rwsched_cfrunloop(rwmsg_srvchan_t *sc, rwsched_tasklet_ptr_t taskletinfo);


/**
 * Bind a method to a srvchan.  This is the payload format agnostic
 * version; for protobuf formatted methods you probably want to use
 * rwmsg_srvchan_add_service() to bind an entire RWMSG_PAYFMT_PBAPI
 * Rift-Protobuf Service declaration to the srvchan.
 *
 * @param method A method created with rwmsg_method_create().
 */
rw_status_t rwmsg_srvchan_add_method(rwmsg_srvchan_t *sc, rwmsg_method_t *method);


/** 
 * Bind an entire protobuf service to a srvchan, to use
 * RWMSG_PAYFMT_PBAPI-format messages.  
 *
 * @param srv A ProtobufCService value (.base element of Foo_Service structure)
 * 
 * @param context A value to be passed back to service callbacks.
 */
rw_status_t rwmsg_srvchan_add_service(rwmsg_srvchan_t *sc,
				      const char *path,
				      ProtobufCService *srv,
				      void *context);

/**
 * Receive message(s) from a service channel.  Each request will be
 * dispatched to the registered method callback.
 * @return Number of requests received (may be less than dispatched if there are unbound methods).
 */
uint32_t rwmsg_srvchan_recv(rwmsg_srvchan_t *sc);

/**
 * Dereference a srvchan.  When the refct hits zero the srvchan will
 * be deleted.
 */
void rwmsg_srvchan_release(rwmsg_srvchan_t *sc);

/**
 * Internal use only (?).  Send a handle containing a response.  Callable from any thread.
 */
rw_status_t rwmsg_srvchan_send(rwmsg_srvchan_t *sc, rwmsg_request_t *req);

/**
 * Pause new request arrival on this srvchan.
 */
rw_status_t rwmsg_srvchan_pause(rwmsg_srvchan_t *sc);

/**
 * Resume new request arrival on this srvchan.
 */
rw_status_t rwmsg_srvchan_resume(rwmsg_srvchan_t *sc);

__END_DECLS

#endif // __RWMSG_SRVCHAN_H
