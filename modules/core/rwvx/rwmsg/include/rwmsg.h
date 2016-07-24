
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg.h
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 * @brief Top level API include for RW.Msg component
 */

#ifndef __RWMSG_H__
#define __RWMSG_H__

#define RIFT_PROTOC_RWMSG_BINDINGS 1
#include <protobuf-c/rift-protobuf-c.h>

#include "rwlib.h"
#include "rw_dl.h"
#include "rwsched.h"
#include "rwtrace.h"
#include "rwsched_object.h"
#include "rwsched_queue.h"

#define __RWMSG_BNC_RESPONSE_W_PAYLOAD

#include "rwmsg_api.h"

/*! \mainpage RWMsg

This is the API reference and quickstart documentation
for RWMsg in C.  For excruciating detail please consult the full
[RWMsg Specification](http://www.eng.riftio.com/riftdocs/nightly/documentation/riftio/html/riftio_messaging_middleware.html).

## Overview

### Tasklets
Each tasklet in the system has a single rwmsg_endpoint_t toplevel
object.  Messages, visible as rwmsg_request_t objects, come and go
through channels of type rwmsg_srvchan_t and rwmsg_clichan_t.
Channels may be bound to either flavor of RWSched interface.  The
CFRunloop flavor supports both regular asynchronous messaging and
synchronous request-response behavior aka blocking.  The rwsched
dispatch flavor supports various multithreaded models by binding to an
rwsched queue.

### Addressing

Messages are addressed to a (Nameservice Path, Payload Format, Method
Number) tuple.  A NS path is a string, handled with the
rwmsg_destination_t object.  The payload format is one of a handful of
supported formats such as `RWMSG_PAYFMT_RAW` or `RWMSG_PAYFMT_TEXT`.
The method number is a 32 bit method number, in a number space
specific to the payload format.

### Protobuf Support

The Protobuf binding, aka PBAPI, uses the dedicated payload format
`RWMSG_PAYFMT_PBAPI` and manages the RWMsg method numbering
automatically based on values from the RW--extended Protobuf
declarations.  Protobuf-declared services may be bound to RWMsg
channels with the functions rwmsg_clichan_add_service() and
rwmsg_srvchan_add_service().


## Quick Start

For simple examples of establishing a service or making a request read
about \ref srvchan and \ref clichan, as well as the page on \ref
protobuf.

### More Information

<DL>
 <DT>[Type System Extensions](http://www.eng.riftio.com/riftdocs/nightly/documentation/riftio/html/riftio_messaging_middleware.html#_type_system_extensions)
 <DD>Information on the RWMsg Protobuf extension syntax.

 <DT>[Protobuf Language Guide](http://developers.google.com/protocol-buffers/docs/proto)
 <DD>Documentation for the standard Protobuf language.

 <DT>[Functions](globals_func.html)
 <DD>List of documented functions.

 <DT>[RWMsg Headers](files.html)
 <DD>Browse the API documentation by header.
</DL>

 */

/** \page protobuf Protobuf and PBAPI Code Generation

 * ### Message Example
 * Given the Protobuf declaration:
~~~{.proto}
message test_req {
  option (rw_msgopts) = { flat:true };
  required int32 body = 1;
}
~~~
The rift-protoc-c compiler will generate among other things the following useful definitions:
~~~{.c}
// The message declared as test_req becomes TestReq:
typedef struct TestReq TestReq;
struct TestReq
{
  ProtobufCMessage base;
  int32_t body;
};

// The test_req__init function is a constructor which must be called to initialize all 
// messages being sent.
void   test_req__init(TestReq         *message);

// This walks the C form of a message and returns the encoded size.
size_t test_req__get_packed_size(const TestReq   *message);

// Pack and unpack functions will encode a message.  The _usebody flavor uses a caller-
// provided space.  The RWMsg PBAPI bindings normally handle packing and unpacking for you.
size_t test_req__pack()
size_t test_req__pack_to_buffer()
TestReq *test_req__unpack()
TestReq *test_req__unpack_usebody()

// Deallocate a message obtained from unpack.  The RWMsg PBAPI bindings normally handle this.
void   test_req__free_unpacked()
void   test_req__free_unpacked_usebody()

// Typesafe typedef for a server routine to call with a response.  This closure function is
// provided as an argument to service routines.  Servers may also use the void*-accepting
// rwmsg_srvchan_send_response_pbapi().  Doing so avoids a need for servers which defer 
// responses beyond the lifespan of the service callback to stash the closure function pointer.
typedef rw_status_t (*TestRsp_Closure)
                 (const TestRsp *message,
                  void *closure_data);
~~~

 * ### Service Example
 * Given the Protobuf declaration:
~~~{.proto}
service Test {
  option (rw_srvopts) = { srvnum:100 };
  rpc Increment(test_req) returns(test_rsp) {
    option (rw_methopts) = { methno:1 blocking:true};
  };
}
~~~
The rift-protoc-c compiler will generate among other things the following useful definitions:
~~~{.c}
// Declaration of a Protobuf service handle.  This is to be initialized using the 
// TEST__INITSERVER() macro, and bound to a srvchan with rwmsg_srvchan_add_service().
typedef struct Test_Service Test_Service;
struct Test_Service
{
  ProtobufCService base;
  void (*increment)(Test_Service *service,
                    const TestReq *input,
                    void *usercontext,
                    TestRsp_Closure closure,
                    void *closure_data);
  // ...function pointer entry for each method, all filled in by __INITSERVER()
};
#define TEST__INITSERVER(srv, function_prefix__)                                          \
  do {                                                                                    \
    (srv)->base.descriptor = &test__descriptor;                                           \
    :                                                                                     \
    :                                                                                     \
    (srv)->increment = (void (*)(Test_Service *, const TestReq *, void *,                 \
                                 TestRsp_Closure, void *))function_prefix__ ## increment; \
  } while(0)


// Declaration of a Protobuf client handle.  This is to be initialized using the
// TEST__INITCLIENT() macro, and bound to a clichan with rwmsg_clichan_add_service().
typedef struct Test_Client Test_Client;
struct Test_Client
{
  ProtobufCService base;
};
#define TEST__INITCLIENT(srv) // ...


// Protobuf client routines for the service; call these to actually send a request.  
// The _b one is blocking.
rw_status_t test__increment(Test_Client *service,
                            rwmsg_destination_t dest,
                            const TestReq *input,
                            rwmsg_closure_t *closure,
                            rwmsg_request_t **req_out);
rw_status_t test__increment_b(Test_Client *service,
                              rwmsg_destination_t dest,
                              const TestReq *input,
                              rwmsg_request_t **req_out);
~~~

 */

#endif // __RWMSG_H__

