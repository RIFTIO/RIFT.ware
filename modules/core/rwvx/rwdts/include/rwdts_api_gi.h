/* STANDARD_RIFT_IO_COPYRIGHT */
/*!
 * @file rwdts_api_gi.h
 * @brief Introspectable API for RW.DTS
 * @author Anil Gunturu (anil.gunturu@riftio.com) / Rajesh Velandy (rajesh.velandy@riftio.com)
 * @date 2015/02/21
 *
 *

\page core Core API Introduction

\anchor api_gi

## DTS Core API Introduction

The DTS Core API include the Member and Query APIs, as well as the
related Keyspec APIs from the yangtools package.

Core API features are uniformly available in all Gi-supported
languages.

For an introduction to DTS and an overview of key DTS concepts, see
\ref overview.

### Query API

Clients execute queries using either the single query interface
rwdts_api_query() or the full interface involving transaction
(rwdts_xact_t) and block (rwdts_xact_block_t) objects.

#### Simple Queries

A single-expression query can be run like this:

~~~{.c}
static void
myapp_query_cb(rwdts_xact_t *xact,
               rwdts_xact_status_t* xact_status,
               void *user_data) {

  // There may be result(s) available now
  rwdts_query_result_t *res = NULL;
  while (res = rwdts_xact_query_result(xact, 0)) {
    const char *path = rwdts_query_result_get_xpath(res);
    const MyAppPb *pb = (MyAppPb*)rwdts_query_result_get_protobuf(res);
    fprintf(stderr, "path=%s pb=%p\n", path, pb);
  }

  // The transaction may be done now
  if (xact_status->xact_done) {
    switch (xact_status->state) {
      case RWDTS_XACT_ABORTED:
      case RWDTS_XACT_FAILURE:
        // Get most probable error
        char *xpath = NULL;
        rw_status_t rs;
        char *errstr = NULL;
        if (RW_STATUS_SUCCESS == rwdts_xact_query_error_heuristic(xact, 0, &xpath, &rs, &errstr)) {
          fprintf(stderr, "Error: path=%s, cause=%u, str=%s\n", xpath, cause, errstr);
          free(xpath);
          free(errstr);
        }
        // Get the number of errors
        uint32_t count = rwdts_xact_query_error_count(xact, 0);
        fprintf(stderr, "Error: count=%u\n", count);
        // Get each error
        rwdts_query_error_t *err = NULL;
        while (err = rwdts_xact_query_error(xact, 0)) {
          const char *path = rwdts_query_error_get_xpath(err);
          rw_status_t cause = rwdts_query_error_get_cause(err);
          const char *errstr = rwdts_query_error_get_errstr(err);
          const RwDtsErrorEntry *pb = (RwDtsErrorEntry*) rwdts_query_error_get_protbuf(err);
          fprintf(stderr, "Error: path=%s, cause=%d, str=%s, pb=%p\n", path, cause,
                  errstr, pb);
          rwdts_query_error_destroy(err);
          err = NULL;
        }
        break;
      default:
        break;
    }
  }
}

int foo() {
  rwdts_xact_t *xact;
  // Let's get foo/bar[5]
  xact = rwdts_api_query(apih,
                         "D,/rwfoo:rwfoo/rwfoo:rwbar[rwfoo:rwkey='5']",
                         RWDTS_ACTION_READ,
                         RWDTS_FLAG_MERGE,   // return whole objects at the query point
                         myapp_query_cb,
                         NULL,  // userdata
                         NULL); // payload, for set/update
  RW_ASSERT(xact);
}
~~~

Or equivalently in Python:
~~~{.py}
def foo():
    // Let's get foo/bar[5]
    xpath = "D,/rwfoo:rwfoo/rwfoo:rwbar[rwfoo:rwkey='5']"
    res_iter = yield from dts.query_read(xpath, rwdts.Flag.MERGE)

    // There may be multiple results
    for res in res_iter:
        try:
            result = yield from res
            // Do something with the result
        except rift.tasklets.dts.TransactionFailed as e:
            // Prints the most probable reason for failure in the trace
            print (str(e))
            // Get all the results
            for path, status, error_msg in e.query_results():
                print(error_msg)

~~~

#### Compound Queries

Compound queries are multi-query and multi-step transactions built
using a "block" abstraction.  Each transaction is a list of query
blocks, each block containing one or more individual queries.  Blocks
execute, to completion, in order.  Queries within a block execute in
parallel, in no defined order.

Results from a block are available as unsorted individual query
results, merged or unmerged; or as one monolithic merged block-wide
result.  Block-wide merged results will be typed at a common
denomonator in the schema tree, which may not coincide with the types
returned by any one query in the block (TBD, probably going to be rooted).

~~~{.c}
int foo() {
  rwdts_xact_t *xact = rwdts_api_xact_create(apih, 0, xact_cb, self);
  rwdts_xact_block_t *blk = rwdts_xact_block_create(xact);
  rw_status_t rs;
  rs = rwdts_xact_block_add_query(blk,
                                  xpath,
                                  RWDTS_ACTION_READ,
                                  0, // flags
                                  1, // our nonzero correlation value, query ID within the block
                                  NULL); // payload
  rs = rwdts_xact_block_add_query(blk,
                                  xpath2,
                                  RWDTS_ACTION_READ,
                                  0, // flags
                                  2, // our nonzero correlation value, query ID within the block
                                  NULL); // payload

  // We could add additional queries and/or blocks.

  // Send this block off for execution in sequence with other blocks originated here.
  rwdtx_xact_block_execute(blk, 0, foo_blk_cb, userdata, NULL);

  // This transaction will have no more queries added by us.
  //
  // Participating members may still run sub-queries and side-effect
  // queries within the scope of the transaction.
  //
  rwdts_xact_commit(xact, foo_xact_cb, userdata, NULL);
}

static void
foo_xact_cb(rwdts_xact_t *xact,
            rwdts_xact_status_t* xact_status,
            void *user_data) {

  // If there are results here, it is because there was a missing
  // block-level callback or one of them did not consume all results.

  // The transaction may be done now.
  if (xact_status->xact_done) {
    switch (xact_status->state) {
      case RWDTS_XACT_ABORTED:
      case RWDTS_XACT_FAILURE:
        // Check for errors here
        break;
      default:
        break;
    }
  }
}

static void
foo_blk_cb(rwdts_xact_t *xact,
           rwdts_xact_status_t* xact_status,
           void *user_data) {

  // There may be result(s) available now, get any ones from any query (corrid 0 is "any")
  rwdts_query_result_t *res = NULL;
  while (res = rwdts_block_query_result(xact_status->block, 0)) {
    const char *path = rwdts_query_result_get_xpath(res);
    const MyAppPb *pb = (MyAppPb*)rwdts_query_result_get_protobuf(res);
    fprintf(stderr, "path=%s pb=%p\n", path, pb);
  }

  // The transaction may be done now.  However, we don't especially care, we leave that
  // to the xact-wide callback.
}

~~~

Or equivalently in Python:

~~~{.py}
with dts.transaction(flags=rwdts.Flag.TRACE) as xact:
    block = xact.block_create()

    // Co-relation ID of the transactions in the Block
    corrid1 = block.add_query_read(xpath) // Autogenerated corrid 1
    corrid2 = block.add_query_read(xpath) // Autogenerated corrid 2

    res_iter = yield from block.execute(flags=rwdts.Flag.MERGE|rwdts.Flag.TRACE, now=True)

    // There may be result(s) available now, get any ones from any query (corrid 0 is "any")
    for i in res_iter: // (or) res_iter([1, 2]) or // res_iter(1)
        try:
            r = yield from i
            // do something with the results
        except rift.tasklets.dts.TransactionFailed:
            // Handle exceptions
~~~


### Keyspec API

While queries nominally execute against XPath expressions, in practice
these are compiled down to a binary/Protobuf representation called a
"keyspec".  After registration, members fielding queries are advised
to deal mainly in keyspecs.  This avoids scattered or ad hoc parsing
and erratic expression evaluation beheaviors.  Also, keyspecs are
typesafe, and in C many errors will occur at compile time that would
not otherwise be obvious until runtime.

For interfaces where an application is building up a full keyspec to
invoke a DTS API, it may be simpler to use an xpath expression for
keys; this tends to be true at registration and query time.  However
in cases where application is given a keyspec from DTS, or where
application has to set keys on an existing partially filled keyspec,
it is usually easier to work with the binary/protobuf keyspec.  The
application can extract keys from the keyspec handed to it by DTS and
can fill in missing keys for writes.

For example consider a scenario where an application is registered
at rwmodule:rwfoo[rwmodule:rwfookey='xyz']/rwmodule:rwbar[rwmodule:rwbarkey=*].

Notice how in the above registration all the keys are filled in except
the last path entry - in this instance rwmodule:rwbar.  This
registration is considered as wildcarded at rwbar and DTS will invoke
the registered callback for any key value of rwbarkey.

Let us assume that the application gets a create for
rwmodule:rwfoo[rwmodule:rwfookey='xyz']/rwmodule:rwbar[rwfoo:rwbarkey='abc'].

When DTS receives a query matching the above expression, it invokes
the prepare callback the application registered against this
keyspec. In the callback it provides the application the full keyspec
and the message associated with the keyspec.

In C the application callback will look similar to the following:

~~~{.c}

rwdts_member_rsp_code_t
rw_foo_bar_callback(const rwdts_xact_info_t* xact_info,
                    RWDtsQueryAction         action,
                    const rw_keyspec_path_t* ks,
                    const ProtobufCMessage*  msg)
{

  // This is a create for the foo bar message
  switch(action) {
    case RWDTS_QUERY_CREATE:
     // Extract the keys for Bar.
     // Gets the rwfoo context corresonding to rwfookey='xyz'
     RwfooCtxt* my_foo_ctxt = (RwfooCtxt*)xact_info->ud;

     // Declare and cast the incoming message to a format that the application understands
     // This macro also checks type protobuf descriptor compatibility.
     RWPB_M_MSG_DECL_PTR_CAST(RwModule_Rwbar, rw_bar_msg, msg);

     // Get the  path entry corresponding to bar
     RWPB_M_PATHENTRY_DECL_GET_PTR(RwModule_Rwbar, rw_bar_pe, ks);


     // The following piece of code checks if the rwbarkey field is set to  "abc"
     // then creates a new my_foo_bar_ctxt from the following in the my_foo_ctxt,
     // Key for foobar  - rw_bar_pe->rwbarkey
     // rw_bar_msg from DTS that contains the message as a protobuf from DTS

     if (strcmp(rw_bar_pe->rwbarkey, "abc")) {
       RwfooBarCtxt* my_foo_bar_ctxt = (RwfooBarCtxt*)create_foo_bar_context(my_foo_ctxt,
                                                                             rw_bar_pe->rwbarkey,
                                                                             rw_bar_msg);
     }
     break;
    case RWDTS_QUERY_READ:
    case RWDTS_QUERY_UPDATE:
    case RWDTS_QUERY_DELTE:
      break;
  }
}
~~~

The application can use RWPB_M_PATHENTRY_DECL_GET_PTR to get the keys
path entry at any path.  For instance, if in the previous example the
rwfoo was wildcarded, then the application may have to extract
rwfookey to find the RwFooCtxt.

The c code then  would look something like the following,

~~~{.c}
  // Get the  path entry corresponding to foo
  RWPB_M_PATHENTRY_DECL_GET_PTR(RwModule_Rwfoo, rw_foo_pe, ks);
  if (strcmp(rw_foo_pe->rwfookey, "xyz")) {
    // Do something for this rw_foo_pe ...
  }
~~~

The same set of APIs can be used to set keys. The keyspecs returned by
DTS are usually are const.  So the application will need to create a
non const keyspec before it can start changing the keys.

~~~{.c}
  // Make a copy of the keyspec in stack that can be  modified
  RWPB_M_PATHSPEC_DECL_COPY(RwModule_Rwbar, new_ks, ks);

  // Get the path entry that need whose keys need to be set
  RWPB_M_PATHENTRY_DECL_GET_PTR(RwModule_Rwbar, writable_bar_pe, &new_ks);

  // Update the key
  strcpy(writable_bar_pe->rwbarkey, "newkey");

~~~

In case of python there will be a context corresponding each yang node
that the python app can use to get the concrete messages and path
elements and keyspecs.

~~~{.py}
from gi.repository import RwModule

def rw_foo_bar_app_cb(self, xact_info, action, keyspec, pbcm):
  # create a context for Rwbar
  ctx = RwModule.Rwbar.context()

  # Get a concrete message correponding to the context from the generic protobuf message
  cc_msg = ctx.msg_copy(pbcm)

  # Get a concrete path entry  correeponding to the context from the keyspec
  cc_pe = ctx.pe_lookup_copy(ks)

  if (cc_pe.key00.rwbarkey == "abc"):
    my_foo_bar_ctxt = create_foo_bar_context(my_foo_ctxt, cc_pe.key00.rwbarkey, cc_msg)
~~~


See also the [Yang Keyspec Reference](../../yangtools/html/index.html), most notably [RiftWare schema path specifiers](../../yangtools/html/group___rw_keyspec.html#func-members).

### Member API

Members register with the DTS API.  Each individual registration
represents a specific point in the overall data schema; registrations
may be made either as publisher or subscriber.

Registrations should be associated with registration groups.  Groups
associate multiple registrations together into a single transaction
handling entity associated with a configuration or data object.  The
group-themed member APIs also provide consolidated transaction event
processing callbacks as well as pre- and post-transaction transient
storage management.

Registrations may include wildcards.  With the advent of the sharding
API keyed registrations will be equivalent to wildcard registrations
with single-key shards.  See also \ref sharding.

TBD Registrations are relative; parent of a component passes in the
leading path.  Callbacks include the relative path offset against the
provided keyspec.

Note that some configuration, bulk operational data, and other common
use cases have higher level convenience APIs.

See also rwdts_member_group_create(), rwdts_member_register(); AppConf
and AppData for higher level API examples.


#### Member API Example

Here is a simple publishing member implementing two objects.

~~~{.c.}
// setup / registration
self_setup(my_pub_t *self) {
  rwdts_group_t *grp = rwdts_api_group_create(apih, xact_init, xact_deinit, xact_event, self);

  rw_status_t rs;

  // router, a virtual object that we implement everything for
  rs = rwdts_group_register_xpath(grp,
                                  "D,/rw-base:colony[rw-base:colony='apple']/rw-base:router[rw-base:router='*']",
                                  self_ready1,
                                  xact_query1,
                                  NULL,    //  \
                                  NULL,    //   + these precommit/commit/abort events are often better handled once, xact-wide, in xact_event
                                  NULL,    //  /
                                  RWDTS_FLAG_PUBLISHER|RWDTS_FLAG_PREP_READ,
                                  self,
                                  &self->reg1);
  // interface, lives in the in-memory "member data" so we don't have to implement reads
  rs = rwdts_group_register_xpath(grp,
                                  "D,/rw-base:colony[rw-base:colony='apple']/rw-base:interface[rw-base:iface='*']",
                                  self_ready2,
                                  xact_query2,
                                  NULL,
                                  NULL,
                                  NULL,
                                  RWDTS_FLAG_PUBLISHER,
                                  self,
                                  &self->reg2);
  rwdts_group_phase_complete(grp, RWDTS_GROUP_PHASE_REGISTER);
}

// self_ready1 -- reconcile / recovery callback
static void self_ready1() {
  // TBD
}
static void self_ready2() {
  // TBD
}

// xact_init
static void *xact_init(rwdts_group_t *grp,
                       rwdts_xact_t *xact,
                       void *self) {
  self_xact_t *sx = RW_MALLOC0(sizeof(self_xact_t));
  sc->self = self;
  //...
}

void xact_query1_more(rwdts_xact_info_t *xact_info, void *another_rsp) {
  void *self_xact = xact_info->scratch;
  void *self = self_xact->self;
  if (error) {
    rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_NACK, NULL, NULL);      // all done, unsuccessfully
  } else if (!another_rsp) {
    rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_ACK, NULL, NULL);       // all done, successfully
  } else {
    void *key = build_keyspec(another_rsp);
    rsp = {key, another_rsp};
    rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_MORE, key, another_rsp);  // probably more to come
  }
}

// xact_query1, a "prepare" callback to evaluate queries against /colony/router
rwdts_member_rsp_code_t xact_query1(const rwdts_xact_info_t *xact_info,
                                    RWDtsQueryAction action,
                                    const rw_keyspec_path_t *keyspec,
                                    const ProtobufCMessage *msg,
                                    void *ctx) {
  void *self_xact = xact_info->scratch;
  void *self = ctx;
  RW_ASSERT(self_xact->self == self);
  switch (action) {
    case RWDTS_QUERY_READ:
      // there is no msg, this is a read
      if (lots of items) {
        // do something that will eventually hand answers to xact_query1_more()
        self_compute_answers(xact_query1_more, xact_info);
        return RWDTS_ACTION_ASYNC;
      } else if (a handful of items) {
        rwdts_xact_info_respond_keyspec(...);
        rwdts_xact_info_respond_keyspec(...);
        return RWDTS_ACTION_ACK;
      } else if (no such item) {
        return RWDTS_ACTION_ACK;
      } else if (not our key) {
        return RWDTS_ACTION_NA;
      } else if (we want to fail this transaction for some reason) {
        // Set the error so the client can get more information
        rwdts_xact_info_send_error_xpath(xact_info->xact, RW_STATUS_FAILURE,
                                         xpath, "Error description");
        return RWDTS_ACTION_NACK;
      }
      break;
    case RWDTS_QUERY_CREATE:
      // there is a msg, this is a create
      if (key / payload makes sense) {
        // make the new thing in a transient xact-associated location
        push(self_xact->updated_routers, self_router_new(keyspec, msg);
        rwdts_xact_info_respond_keyspec(xact_info, RWDTS_XACT_RSP_CODE_ACK, ..just the new key..., NULL);
        return RWDTS_ACTION_ASYNC;
      } else if (not our key) {
        return RWDTS_ACTION_NA;
      } else if (dud payload, error, unpossible, etc) {
        // Set the error so the client can get more information
        rwdts_xact_info_send_error_xpath(xact_info->xact, RW_STATUS_FAILURE,
                                         xpath, "Error description");
        return RWDTS_ACTION_NACK;
      }
      break;
    case RWDTS_QUERY_UPDATE:
      // basically same as CREATE
      break;
    case RWDTS_QUERY_DELETE:
      // there is no msg, this is a delete
      if (no such keyed item(s)) {
        return RWDTS_ACTION_NA
      } else {
        // delete the thing(s)
        return RWDTS_ACTION_ACK;
      }
      break;
    case RWDTS_QUERY_RPC:
      // You're on your own for RPCs.
      // These have key, payload in and out
      break;
    default:
      break;
  }
}

// xact_query2, a "prepare" callback to evaluate queries against /colony/interface
rwdts_member_rsp_code_t xact_query2(const rwdts_xact_info_t *xact_info,
                                    RWDtsQueryAction action,
                                    const rw_keyspec_path_t *keyspec,
                                    const ProtobufCMessage *msg,
                                    void *ctx) {
  void *self_xact = xact_info->scratch;
  void *self = ctx;
  RW_ASSERT(self_xact->self == self);
  switch (action) {
    case RWDTS_QUERY_READ:
      // DTS normally handles reads
      RW_CRASH();
      break;

    default:
    // ...

  }
}

// xact_event -- group-wide pan-query transaction events like PRECOMMIT, COMMIT, ABORT
rwdts_member_rsp_code_t xact_event(rwdts_api_t*         apih,
                                   rwdts_group_t*       grp,
                                   rwdts_xact_t*        xact,
                                   rwdts_member_event_t xact_event,
                                   void*                ctx,
                                   void*                scratch) {
  switch(xact_event) {
    case RWDTS_MEMBER_EVENT_PRECOMMIT:
      // last chance to call the whole thing off
      if (!self_validate_xact(ctx, scratch)) {
        // Set the error so the client can get more information
        rwdts_xact_info_send_error_xpath(xact_info->xact, RW_STATUS_FAILURE,
                                         xpath, "Error description");
        return RWDTS_XACT_RSP_CODE_NACK;
      }
      return RWDTS_XACT_RSP_CODE_ACK;

    case RWDTS_MEMBER_EVENT_COMMIT:
      // actually make permanent any tentative changes
      self_commit_routers(ctx, scratch->updated_routers);
      self_commit_interfaces(ctx, scratch->updated_interfaces);
      return RWDTS_XACT_RSP_CODE_ACK;

    case RWDTS_MEMBER_EVENT_ABORT:
      // toss any tentative changes
      self_toss_routers(ctx, scratch->updated_routers);
      self_toss_routers(ctx, scratch->updated_interfaces);
      return RWDTS_XACT_RSP_CODE_ACK;
      break;
    default:
      break;
  }
}


// xact_deinit
static void xact_deinit(rwdts_group_t *grp,
                        rwdts_xact_t *xact,
                        void *self,
                        void *self_xact) {
  RW_ASSERT(self);
  RW_ASSERT(self_xact);
  RW_ASSERT(self == self_xact->self);
  self_free_updated_routers(self_xact->updated_routers);
  self_free_updated_interfaces(self_xact->updated_interfaces);
  RW_FREE(self_xact);
}

~~~
~~~{.py.}
def on_init(group, xact, scratch):
    # Init steps

def on_event(dts, group, xact, xact_event, scratch):
  # return appropriate flag based on xact_event,eg
  if xact_event == rwdts.MemberEvent.PRECOMMIT:
      return rwdts.MemberRspCode.ACTION_OK
  if xact_event == rwdts.MemberEvent.COMMIT:
    ....

def on_deinit(group,  xact, scratch):
  # Clean up steps

# Handler that wraps all Group related callbacks such as xact_init, xact_deinit, xact_event
group_handler = rift.tasklets.Group.Handler(
                    on_init=on_init, on_event=on_event, on_deinit=on_deinit)


@asyncio.coroutine
def on_ready1(reg, status, _):
    # Reg ready steps

@asyncio.coroutine
def on_prepare1(xact_info, query_action, ks_path, pbcm, _):
    # Respond or send appropriate Flag
    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret1)  # ret1 -> message

reg_handler1 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare1)

@asyncio.coroutine
def on_ready2(reg, status, _):
    # Reg ready steps

@asyncio.coroutine
def on_prepare2(xact_info, query_action, ks_path, pbcm, _):
    # Respond or send appropriate Flag
    xact_info.respond_xpath(rwdts.XactRspCode.ACK, xpath, ret1)  # ret1 -> message

reg_handler2 = rift.tasklets.DTS.RegistrationHandler(
                    on_ready=on_ready, on_prepare=on_prepare2)

with dts.group_create(handler=group_handler) as grp:
    grp.register("D,/rw-base:colony[rw-base:colony='apple']/rw-base:interface[rw-base:iface='*']",
        reg_handler1,
        flags=rwdts.Flag.PUBLISHER)
    grp.register("D,/rw-base:colony[rw-base:colony='apple']/rw-base:router[rw-base:router='*']",
        reg_handler2,
        flags=rwdts.Flag.PUBLISHER)

~~~

\page detail Core API Detail

\anchor coredet

## DTS Core API Detail

### Query Correlation

Each individual query in a transaction has a number of identifiers.

Clients can identify a query via the tuple (rwdts_xact_t,
rwdts_xact_block_t, correlation_id).  The first two are transaction
and block objects constructed with the Query API; the third,
Correlation ID, is an application-provided nonzero uint64_t which has
block scope.  It is passed into rwdts_xact_block_add_query().  Using
zero indicates that there is no Correlation ID, useful for merged
result query blocks or other situations where differentiating by query
is unimportant (retrieval of like objects from two lists, for
example).

Each query elicits a result stream accessed via
rwdts_xact_block_query_result(block, corrid).  Requesting results for
corrid zero yields any results in the block.

Each query's result stream is ended when
rwdts_xact_block_get_query_more_results() returns FALSE.  This
explicit end of results/responses_done condition is not the same as a
lack of results from rwdts_xact_block_query_result(), which signifies
only that there are no results on hand in the local process at the
moment.

### Members

#### Nested Queries

Members are able to execute additional queries while processing
prepare events.  The rwdts_xact_block_create() call may be used to
create a new block attached to an arriving transaction, and
rwdts_xact_block_execute() or execute_immediate() may be used to
submit the block's queries and receive a callback with the results.
If the results are needed to satisfy a query, execute_immediate() is
probably necessary; otherwise execute() may be used to schedule the
block's queries to occur at some time before the transaction commits.


#### Streaming Results

Members may asynchronously return data in a streaming fashion.  While
the API will take as many responses as it is given, any member
returning large numbers of items should comply with the streaming
"get-next" aspects of the Member API.

Firstly, the member should always use the return code
RWDTS_ACTION_ASYNC from a prepare callback which will be returning
many read results.  Then the member should pass in responses using
rwdts_member_send_response(), again with the ASYNC code.

[ Below here implementation is suspect and/or broken; new interface TBD ]

Each invokation of prepare is given an rwdts_xact_info_t object.  This
object includes a "credits" value which should be used to govern the
return of bulk data.  As many responses as the credit value should be
returned for each invokation of a prepare callback.

The prepare callback will be reinvoked for the query at least as many
times as the credit value is run down by the sending of that many
responses.  The app should generally wait for this before continuing
to send more responses after running out the previously available
credit value.  This ensures reasonable flow control behavior through
DTS.

Finally, the member should indicate the end of results by passing
RWDTS_ACTION_ACK into rwdts_member_send_response(), either together
with a final result or afterwards by itself.

##### Streaming Result Cursor

[ Below here implementation is suspect and/or broken; new interface TBD ]

While streaming results back, it may be necessary to save app-internal
"cursor" state around the last result sent or the next result to send.

This app cursor state is associated with the 4 element tuple
(transaction, block, query, member registration).

The result API is threadsafe, but unable to affirmatively order
results submitted from multiple threads.

.......

Returning results: itsa list! return ASYNC; do { response(XX, ASYNC) }
while (!more); Store cursor in group xact object, keyed on (??) id,
should be an xact-reg-query match id from API?


#### Registration callback keying semantics

Relative keying; TBD.

See [RiftWare schema path specifiers](../../yangtools/html/group___rw_keyspec.html#func-members).

Typed pathelem objects.  See ...what... TBD

Keys in, filled in wildcards, sometimes above registration point.

Keys out, always filled in, one key per returned or modified object.


#### Registration data semantics

Any registration which is contained within another represents a
pruning point in the containing registration.  This is directly driven
by application code.  Thus a router, which in the schema contains
interfaces, may be a distinct object for registration and query
evaluation purposes from the interface object(s) contained within.

This is implicitly driven by application logic; an application
registering for contexts is presumed to "know" that interfaces are
implemented by other code against another registration.

Best practice is generally to register at all "listy" points.

Alternatively, an application might implement a containing object like
"router" and the child objects "interface" within a single
registration.  In this case the application code is responsible for
handling all queries relating to routers and/or interfaces.  In some
cases only interfaces would be involved, usually requiring some
iterative evaluation of the query key in application code to find the
correct router and the right interface within.

In short, publishers are responsible for responding to READ queries
with a chunk of data a) rooted at their registration, b) scoped to
include only their own object(s), and c) ideally limited to items
requested by the given keyspec's trailing keying data (ie, the bits of
key after the registration point).  Point (c) is technically optional
but fairly desirable.

Note that READ requests will not usually include a payload or message;
sub-registration keying is available and should be found FROM THE
QUERY KEY.

Members implemented with the API's provided in-memory database (aka
"inboard" data) will automatically comply with (a)-(c) for read
operations, because DTS will respond to READs on the application's
behalf.  Create, Update, Delete operations may be variously keyed and
must obey the query-provided key.

Member code implemented with virtual data callbacks (aka "outboard"
data) to respond to CRUD operations must comply with (a)-(b).  Support
for item (c) is required for write C_UD operations and optional for
Read.

KEYONLY/NOP flag, no payload, just keys.  Would a NOP flag provide
superset utility?  TBD


#### Sharding

\anchor sharding

Sharding is implemented as an overlay atop wildcarded keys.  There are
two layers which operate together to provide sharding-driven
scalability and relisiency features.

##### Member Sharding

At the member layer, the application may respond to any query with
RWDTS_EVTRSP_NA.  This informs DTS that this query is not handled at
this member-registration; if all queries that hit a particular member
return NA, the member will be excluded from future communication about
this transaction.

##### Sharding API - TBD

At the DTS layer, the Sharding API accepts sharding descriptors
defining which subset of key values for any wildcard point(s) in a
registration's keypath actual apply.  This installed shard information
is then propagated up to the DTS routing system and used to pre-filter
query delivery and drive scalable heirarchical delivery.



\page recovery Recovery Support

\anchor recovery


## DTS Recovery Support

Gear Dog document this!



 */

#ifndef __RWDTS_API_GI_H__
#define __RWDTS_API_GI_H__

#include <glib.h>
#include <glib-object.h>
#include <rwtypes_gi.h>
#include <rw_keyspec_gi.h>
#include <rwsched_gi.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_api_gi_enum.h>

/*
 * Most of the following typedefs are needed by GI
 */

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
typedef struct rwdts_registration_s rwdts_registration_t;
typedef struct rwdts_xact_info_s rwdts_xact_info_t;
typedef struct rwdts_member_reg_handle_s* rwdts_member_reg_handle_t;
typedef struct  _RWDtsQuery* rwdts_query_handle_t;
typedef struct rwdts_xact_status_s rwdts_xact_status_t;
typedef struct rwdts_member_cursor_s rwdts_member_cursor_t;
typedef struct rwdts_shard_s rwdts_shard_t;
typedef struct rwdts_shard_s rwdts_shard_handle_t;
typedef struct rwdts_appdata_s rwdts_appdata_t;
typedef struct rwdts_shard_s* rwdts_shard_hdl_ptr_t;
typedef struct rwdts_appdata_cursor_s rwdts_appdata_cursor_t;
typedef struct rwdts_api_config_ready_data_s* rwdts_api_config_ready_data_ptr_t;
typedef struct rwdts_api_config_ready_handle_s *rwdts_api_config_ready_handle_ptr_t;


#ifndef __GI_SCANNER__
#include <rwdts_query_api.h>
#include <rwdts_member_api.h>
#include <rwdts.h>
#endif


__BEGIN_DECLS


GType rwdts_api_get_type(void);
GType rwdts_xact_get_type(void);
GType rwdts_xact_block_get_type(void);
GType rwdts_query_result_get_type(void);
GType rwdts_query_error_get_type(void);
GType rwdts_group_get_type(void);
GType rwdts_member_reg_handle_get_type(void);
GType rwdts_appconf_get_type(void);
GType rwdts_xact_status_get_type(void);
GType rwdts_xact_info_get_type(void);
GType rwdts_member_cursor_get_type(void);
GType rwdts_audit_result_get_type(void);
GType rwdts_shard_handle_get_type(void);
GType rwdts_appdata_cursor_get_type(void);

/// @cond GI_SCANNER

/*!
 * Query result structure
 * This structure is used to return  single reulst of a query
 * A single query can return multiple of these structures
 *
 */

typedef struct rwdts_query_result_s {
  ProtobufCMessage* message;
  rw_keyspec_path_t* keyspec;
  char*              key_xpath;
  rwdts_api_t*       apih;
  uint64_t           corrid;
  uint32_t           type;
} rwdts_query_result_t;

typedef union rwdts_shard_keyfunc_params_u {
  uint32_t hash_permute;
  uint32_t block_sz;
} rwdts_shard_keyfunc_params_t;

typedef struct rwdts_shard_range_params_s {
  int64_t start_range;
  int64_t end_range;
} rwdts_shard_range_params_t;

typedef struct rwdts_shard_hash_params_s {
  uint32_t chunk_count;
} rwdts_shard_hash_params_t;

typedef struct rwdts_shard_consistent_params_s {
  uint32_t chunk_count;
} rwdts_shard_consistent_params_t;

typedef uint64_t rwdts_chunk_id_t;

typedef struct rwdts_shard_null_params_s {
  rwdts_chunk_id_t chunk_id;
} rwdts_shard_null_params_t;

struct rwdts_shard_ident_params_s {
  uint8_t keyin[512];
  size_t keylen;
};

typedef union rwdts_shard_flavor_params_u {
  struct rwdts_shard_range_params_s range;
  struct rwdts_shard_hash_params_s hash;
  struct rwdts_shard_consistent_params_s consistent;
  struct rwdts_shard_null_params_s nullparams;
  struct rwdts_shard_ident_params_s identparams;
} rwdts_shard_flavor_params_t;

/**
 * xact:
 * @xact_info:
 * @user_data:
 */
/// @endcond

// FIXME add block.  Also can the xact status object pleasingly be an argument?

typedef void
(*rwdts_query_event_cb_routine)(rwdts_xact_t* xact,
                                rwdts_xact_status_t* status,   /* want to replace this with state */
                                void*         user_data);

/*!
 * Gets the result of a transaction.  The API will provide the next
 * results until all the available results are exhausted.  See also
 * the block-specific calls rwdts_xact_block_...() which add query
 * blocks and return results by block.
 *
 * @param  xact      DTS Transaction handle
 * @param  corrid    When corrid is non zero this API returns results for the query with the passed corrid
 * @return The query result if present.
 *
 * The rwdts_query_result_t returned by this API is owned by the
 * API. The application must extract the mesage and keyspecs using the
 * follwoing getter themed APIs.
 *
 * - rwdts_query_result_get_protobuf()
 * - rwdts_query_result_get_xpath()
 * - rwdts_query_result_get_keyspec()
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_query_result_gi:(rename-to rwdts_xact_query_result)
 * @xact:
 * @corrid: (nullable) (optional)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_query_result_t*
rwdts_xact_query_result_gi(rwdts_xact_t* xact,
                           uint64_t      corrid);
/// @cond GI_SCANNER
/**
 * rwdts_xact_query_result:
 * @xact:
 * @corrid: (nullable) (optional)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_query_result_t*
rwdts_xact_query_result(rwdts_xact_t* xact,
                        uint64_t      corrid);
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_query_result_gi:(rename-to rwdts_xact_block_query_result)
 * @blk:
 * @corrid: (nullable) (optional)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_query_result_t*
rwdts_xact_block_query_result_gi(rwdts_xact_block_t* blk,
                                 uint64_t            corrid);
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_query_result:
 * @blk:
 * @corrid: (nullable) (optional)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_query_result_t*
rwdts_xact_block_query_result(rwdts_xact_block_t* blk,
                              uint64_t            corrid);
/*!

 Set the trace bit on a transaction.  This can only be set in the
 transaction originator.  Traced transactionsa emit events into the
 logging subsystem, and report all events back to the originator,
 where tehy are dumped to console.

 @param xact Transaction handle.
 @return Status code
*/
/// @cond GI_SCANNER
/**
 * rwdts_xact_trace:
 * @xact:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_trace(rwdts_xact_t* xact);

/*!
 *  Get the protobuf associated with the result
 *  @param  query_result The query result structure
 *
 *  @return  protobuf corresponding to the query result. The API owns the  protobuf
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_result_get_protobuf:
 * Returns: (type ProtobufC.Message) (transfer none)
 */
/// @endcond
const ProtobufCMessage*
rwdts_query_result_get_protobuf(const rwdts_query_result_t* query_result);


#ifndef __GI_SCANNER__
/*!
 *  Get the value associated with the result. To be used with
 *  XPath reduction queries.
 *  @param  query_result The query result structure
 *
 *  @return Return the status of the operation and updates
 *          type with the type of value and value with the
 *          pointer to the actual value. User need to release this pointer.
 */
rw_status_t
rwdts_query_result_get_value(const rwdts_query_result_t* query_result,
                             ProtobufCType *type,
                             void **value);
#endif // __GI_SCANNER__

/// @cond GI_SCANNER
/**
 * rwdts_get_router_conn_state:
 * @apih:
 * returns: (type RWDtsRtrConnState) (transfer none)
 */
/// @endcond
RWDtsRtrConnState
rwdts_get_router_conn_state(rwdts_api_t   *apih);

/*
 *  rwdts_json_from_pbcm: Gied only for testing
*/

/// @cond GI_SCANNER
/**
 * rwdts_json_from_pbcm:
 * @pbcm:
 * Returns: (type utf8) (nullable) (transfer none)
 */
/// @endcond
gchar*
rwdts_json_from_pbcm(const ProtobufCMessage* pbcm);

/*
 * Do not expose keyspec through GI for now
 */

#ifndef __GI_SCANNER__

/*!
 *  Get the keyspec associated with the result
 *  @param  query_result The query result structure
 *
 *  @return  XML corresponding to the query result. The API owns the keyspec
 */

/// @cond GI_SCANNER
/**
 * rwdts_query_result_get_keyspec:
 * Returns: (type RwKeyspec.Path) (transfer none)
 */
/// @endcond
const rw_keyspec_path_t*
rwdts_query_result_get_keyspec(const rwdts_query_result_t* query_result);

#endif /* __GI_SCANNER__ */

/*!
 *  Get the xpath associated with the result
 *  @param query_result The query result structure.
 *
 *  @return a string containing the xpath information for the query - The caller owns the string
 */
const char*
rwdts_query_result_get_xpath(const rwdts_query_result_t* query_result);

/*!
 * Get the most probable casue from all the errors in the the transaction
 * for the corrid.
 *
 * @param  xact      DTS Transaction handle
 * @param  corrid    Find error matching the corrid
 * @param  xpath     Xpath associated with the error
 * @param  err_cause most probable cause for error
 * @param  errstr    error string associated with the error
 * @return status code success or failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_get_error_heuristic:
 * @xpath: (out) (nullable) (transfer full)
 * @cause: (out) (type RwTypes.RwStatus)(transfer none)
 * @errstr: (out) (nullable) (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_get_error_heuristic (rwdts_xact_t* xact,
                                uint64_t corrid,
                                char **xpath,
                                rw_status_t *cause,
                                char **errstr);

/*!
 * Get the most probable casue from all the errors in the the transaction
 * for the corrid.
 *
 * @param  xact      DTS Transaction handle
 * @param  corrid    Find error matching the corrid
 * @return number of errors in xact
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_get_error_count:
 * @xact:
 * @corrid:
 * Returns:
 */
/// @endcond
uint32_t
rwdts_xact_get_error_count (rwdts_xact_t* xact,
                            uint64_t corrid);

/*!
 * Gets the errors for a transaction.  The API will provide the next
 * errors until all the available errors are exhausted.  See also
 * the block-specific calls rwdts_xact_block_...() which add query
 * blocks and return errors by block.
 *
 * @param  xact      DTS Transaction handle
 * @param  corrid    When corrid is non zero this API returns results for the query
 *                   with the passed corrid
 * @return The query error if present.
 *
 * The rwdts_query_error_t returned by this API is owned by the
 * API. The application must extract the mesage and keyspecs using the
 * following getter themed APIs.
 *
 * - rwdts_query_error_get_cause()
 * - rwdts_query_error_get_protobuf()
 * - rwdts_query_error_get_xml()
 * - rwdts_query_error_get_xpath()
 * - rwdts_query_error_get_keyspec()
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_query_error:
 * @xact:
 * @corrid:
 * Returns: (nullable)
 */
/// @endcond
rwdts_query_error_t*
rwdts_xact_query_error(rwdts_xact_t* xact,
                       uint64_t      corrid);

/*!
 *  Get the error cause
 *  @param  query_error The query error structure
 *
 *  @return rw_status_t corresponding to the query error.
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_cause:
 * @query_error:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_query_error_get_cause(const rwdts_query_error_t* query_error);

/*!
 *  Get the error string
 *  @param  query_error The query error structure
 *
 *  @return string corresponding to the query error.
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_errstr:
 * @query_error:
 * Returns: (nullable) (transfer none)
 */
/// @endcond
const char*
rwdts_query_error_get_errstr(const rwdts_query_error_t* query_error);

/*!
 *  Get the protobuf associated with the error
 *  @param  query_error The query error structure
 *
 *  @return  protobuf corresponding to the query error. The API owns the  protobuf
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_protobuf:
 * @query_error:
 * Returns: (type ProtobufC.Message) (transfer none)
 */
/// @endcond
const ProtobufCMessage*
rwdts_query_error_get_protobuf(const rwdts_query_error_t* query_error);

#ifndef __GI_SCANNER__

/*!
 *  FIXME: Does no work currently as there is no schema associated with error
 *  protobuf
 *  Get the XML associated with the error
 *  @param  query_error The query error structure
 *
 *  @param  query_error The query error structure
 *
 *  @return  XML corresponding to the query error. The caller owns the result XML string
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_xml:
 * @query_error:
 * Returns: (type utf8) (nullable) (transfer full)
 */
/// @endcond
gchar*
rwdts_query_error_get_xml(const rwdts_query_error_t* query_error);

/*
 * Do not expose keyspec through GI for now
 */

/*!
 *  Get the keyspec associated with the error
 *  @param  query_error The query error structure
 *
 *  @return  Keyspec associated with the error. The API owns the keyspec
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_keyspec:
 * @query_error: (type gpointer)
 * Returns: (type RwKeyspec.Path) (transfer none)
 */
/// @endcond
const rw_keyspec_path_t*
rwdts_query_error_get_keyspec(const rwdts_query_error_t* query_error);

#endif /* __GI_SCANNER__ */

/*!
 *  Get the xpath associated with the error
 *  @param query_error The query result structure.
 *
 *  @return a string containing the xpath information for the query - The caller owns the string
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_xpath:
 * @query_error:
 * Returns: (nullable) (transfer none)
 */
/// @endcond
const char*
rwdts_query_error_get_xpath(const rwdts_query_error_t* query_error);

/*!
 *  Get the keystr associated with the error
 *  @param query_error The query result structure.
 *
 *  @return a string containing the keystr information for the query - The caller owns the string
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_get_keystr:
 * @query_error:
 * Returns: (nullable) (transfer none)
 */
/// @endcond
const char*
rwdts_query_error_get_keystr(const rwdts_query_error_t* query_error);

/*!
 *  Destroy the query error structure
 *  @param  query_error The query error structure
 *
 *  @return rw_status_t status of the destroy operation
 */
/// @cond GI_SCANNER
/**
 * rwdts_query_error_destroy:
 * @query_error:
 * Returns:
 */
/// @endcond
void
rwdts_query_error_destroy(rwdts_query_error_t* query_error);

/*!
 *
Members register this callback with rwdts_api_new to received notice
of DTS state changes.  These states drive member recovery and a
systemwide bootstrap state machine.

A DTS member goes through the following state transitions:

RW_DTS_STATE_INIT  ---> RW_DTS_STATE_REGN_COMPLETE   --->
RW_DTS_STATE_CONFIG ---> RW_DTS_STATE_RUN

State               | Description
------------------- | ---------------
RW_DTS_STATE_INIT   | DTS is finished initializing. (VCS state INITIALIZING)
RW_DTS_STATE_REGN_COMPLETE  | The application has registered all initial registrations, and is waiting for DTS to complete recovery of any subscribed / datastore data.  (VCS state RECOVERING)
RW_DTS_STATE_CONFIG  | DTS is finished recovering the initial registrations, and is waiting for the App to complete any application recovery actions.  (VCS state RECOVERING)
RW_DTS_STATE_RUN     | The application is done recovering and is running.  (VCS state RUNNING)

Members receive state events named for the state, when that state is entered.

Members must call rwdts_api_set_state() or in Python apih.state=FOO to
enter the states REGN_COMPLETE and RUN.  Tasklet which do not do so
within a reasonable time after startup will be shot by VCS.

These names are supposed to be DTS_FOO and APP_FOO.  Also these API
calls should deal in events not state names.  All a bit ugly.

 @param apih  DTS api handle
 @param state DTS api state
 @param ud    User data that was specified during api_init

 @return      no return value

*/
/// @cond GI_SCANNER
/**
 * apih:
 * @state:
 * @user_data:
 */
/// @endcond
typedef void
(*rwdts_api_state_changed)(rwdts_api_t*  apih,
                           rwdts_state_t state,
                           void*         user_data);

/*!
 * Create a new DTS API handle. This handle serves as a global instance
 * for DTS data within the member and most of the DTS APIs use this reference
 * directly or indirectly.
 *
 * The handle keeps references to the passed tasklet and contents until the
 * API handle is deinited.
 *
 * It is intended that unrelated modules within a tasklet will share
 * this one API handle.  The Query API's block mechanism and the
 * Member API registration group mechanism provide the necessary
 * segregation of transaction-related events into separate callbacks
 * and contexts.  Certain tasklet-wide events, such as overall
 * bootstrap state, are handled on a tasklet-global basis and will
 * require a coherent tasklet-wide interaction from one point in the
 * code.
 *
 * The API handle and associated data can be released using
 * rwdts_api_deinit in C, or conventional unreference in managed
 * language bindings.
 *
 * @param tasklet The tasklet for which this DTS instance is being initialized
 * @param schema The yang comiple time schema pointer.  This is a
 *                const data structure coming out of yang compilation.
 *                This schema argument is a placeholder and may need
 *                to be the composite schema for now.
 * @param cb      DTS state change related callback structure.
 *
 * @return new API handle if successfull - NULL otherwise
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_new:
 * @tasklet:       (type RwTasklet.Info)
 * @schema:        (type RwYangPb.Schema)
 * @callback:      (scope notified)(destroy api_destroyed)
 * @user_data:
 * @api_destroyed: (scope async) (nullable)
 *
 */
/// @endcond
rwdts_api_t*
rwdts_api_new(const rwtasklet_info_t*        tasklet,
              const rw_yang_pb_schema_t*     schema,
              rwdts_api_state_changed        callback,
              gpointer                       user_data,
              GDestroyNotify                 api_destroyed);

/*!
 Delete the DTS API handle.

 @apih API handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_deinit:
 * @apih:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_deinit(rwdts_api_t* apih);


/*!
   Conveniently execute a single query transaction.

  @param flags Or bitset of options; see enum RWDtsFlag

 */
/// @cond GI_SCANNER
/**
 * rwdts_api_query:
 * @callback: (scope notified) (closure user_data)
 * @user_data: (type gpointer)
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_query(rwdts_api_t*                api,
    const char*                  xpath,
    RWDtsQueryAction             action,
    uint32_t                     flags,
    rwdts_query_event_cb_routine callback,
    const void*                  user_data,
    const ProtobufCMessage*      msg);


/*!
 Set the API bootstrap state.  This is used to drive the bootstrat/recovery sequence for the tasklet.

 @param state  New state
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_set_state:
 * @apih:
 * @state:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_set_state(rwdts_api_t* apih,
                    rwdts_state_t state);

/*!
 Get the API bootstrap state.
*/
/// @cond GI_SCANNER
/**
 * rwdts_api_get_state:
 * @apih:
 */
/// @endcond
rwdts_state_t
rwdts_api_get_state(rwdts_api_t* apih);

/*!
 Get the string associated with DTS state.
*/
/// @cond GI_SCANNER
/**
 * rwdts_state_str:
 * @state:  DTS state
 */
/// @endcond
const char *
rwdts_state_str(rwdts_state_t state);

/*!
 Print dts api information
*/
/// @cond GI_SCANNER
/**
 * rwdts_api_print:
 * @apih:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_print(rwdts_api_t* apih);

/*!
 * Query API DTS - Run a single DTS query.
 *
 * This API starts a simple one-query transaction.
 *
 * @param apih    DTS API handle
 * @param xpath   The key to query as an xpath expression.
 * @param action  DTS  action
 * @param flags   Flags associated with this query
 * @param cb      Callback  for the query
 * @param msg     The payload associated with this query
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_query_gi: (rename-to rwdts_api_query)
 * @callback: (scope notified) (destroy ud_dtor)  (closure user_data)
 * @user_data: (type gpointer)
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_query_gi(rwdts_api_t*                api,
                   const char*                  xpath,
                   RWDtsQueryAction             action,
                   uint32_t                     flags,
                   rwdts_query_event_cb_routine callback,
                   const void*                  user_data,
                   GDestroyNotify ud_dtor,
                   const ProtobufCMessage*      msg);

#ifndef __GI_SCANNER__
/*!
 * Query API DTS -  Build and run a simple DTS transaction that has a single query.
 *
 * This API starts a single query transaction.  Identical to rwdts_api_query, except keyed with a keyspec.
 *
 * @param apih    DTS API handle
 * @param keyspec Keyspec associated with the data
 * @param action  DTS  action - See rwdts_query_action_t
 * @param flags   Flags associated with this query
 * @param cb      Callback  for the query
 * @param msg     The payload associated with this query
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_query_ks:
 * @keyspec:   (type RwKeyspec.Path) (transfer full)
 * @callback:  (scope notified)
 * @user_data:
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_query_ks(rwdts_api_t*                 apih,
                   const rw_keyspec_path_t*     keyspec,
                   RWDtsQueryAction             action,
                   uint32_t                     flags,
                   rwdts_query_event_cb_routine callback,
                   gpointer                     user_data,
                   const ProtobufCMessage*      msg);


/*!
 * Query API DTS - Run a single DTS query with reduction
 *
 * This API starts a simple one-query transaction.
 *
 * @param apih    DTS API handle
 * @param xpath   The key to query as an xpath with reduction expression.
 * @param action  DTS  action
 * @param flags   Flags associated with this query
 * @param cb      Callback  for the query
 * @param msg     The payload associated with this query
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_query_reduction:
 * @callback: (scope notified) (destroy ud_dtor)  (closure user_data)
 * @user_data: (type gpointer)
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_query_reduction(rwdts_api_t*                 api,
                          const char*                  xpath,
                          RWDtsQueryAction             action,
                          uint32_t                     flags,
                          rwdts_query_event_cb_routine callback,
                          const void*                  user_data,
                          const ProtobufCMessage*      msg);

#endif

/*!
 * Query API DTS - Run a single DTS query with reduction
 *
 * This API starts a simple one-query transaction.
 *
 * @param apih     DTS API handle
 * @param xpath    The key to query as an xpath with reduction expression.
 * @param action   DTS  action
 * @param flags    Flags associated with this query
 * @param cb       Callback  for the query
 * @param msg      The payload associated with this query
 * @param interval Query repeat interval in seconds
 * @param count    Number of times to repeat the query, 0 for infinite
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_query_reduction:
 * @callback: (scope notified) (destroy ud_dtor)  (closure user_data)
 * @user_data: (type gpointer)
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_query_poll(rwdts_api_t*                 api,
                     const char*                  xpath,
                     RWDtsQueryAction             action,
                     uint32_t                     flags,
                     rwdts_query_event_cb_routine callback,
                     const void*                  user_data,
                     GDestroyNotify ud_dtor,
                     const ProtobufCMessage*      msg,
                     uint64_t                     interval,
                     uint64_t                     count);


/*!
 * This API creates a transaction object.  Use it to make and execute
 * blocks of queries, or to initiate transaction member data CRUD calls.
 *
 * This transaction object is the same object received in a member
 * when evaluating queries.
 *
 * @param apih    DTS API handle
 * @param flags   DTS XACT Flags
 * @param cb      DTS xaction wide callback
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_xact_create:
 * @callback: (scope notified) (nullable)
 * @user_data: (nullable)
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_xact_create(rwdts_api_t *api,
                      uint32_t flags,
                      rwdts_query_event_cb_routine callback,
                      const void *user_data);
/// @cond GI_SCANNER
/**
 * rwdts_api_xact_create_gi: (rename-to rwdts_api_xact_create)
 * @callback: (scope notified) (destroy ud_dtor) (nullable)
 * @user_data: (nullable)
 * @ud_dtor: (nullable)
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_xact_create_gi(rwdts_api_t *api,
                         uint32_t flags,
                         rwdts_query_event_cb_routine callback,
                         const void *user_data,
                         GDestroyNotify ud_dtor);

/*!
 * This API is used to create transaction object with support for
 * XPath based result reduction.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_xact_create_reduction:
 * @callback: (scope notified) (destroy ud_dtor) (nullable)
 * @user_data: (nullable)
 * @ud_dtor: (nullable)
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_xact_create_reduction(rwdts_api_t *api,
                                uint32_t flags,
                                rwdts_query_event_cb_routine callback,
                                const void *user_data,
                                GDestroyNotify ud_dtor);

/*!
 * Called by the transaction originator after entering all blocks
 * and/or queries to signify that the transaction is completely
 * entered and therefore ready to commit.  DTS will not commit the
 * transaction and return a final execution status event until the
 * transaction has been completely entered.
 *
 * Similar behavior is implemented for nontransactional multiblock
 * queries; the containing "transaction" will not finish until the
 * commit signal is given.  These nontransactional "transactions"
 * proceed immediately to a completion status, skipping multiphase
 * precommit and commit actions.
 *
 * As a more concise alternative, include the RWDTS_XACT_FLAG_END flag
 * on a the execute call of the final block.  The singleton query API
 * rwdts_api_query() does this.
 *
 * BUG need alternative of an END flag for final block's execute call
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_commit:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_commit(rwdts_xact_t* xact);


/// @cond GI_SCANNER
/**
 * rwdts_api_trace:
 * @apih:
 * @path:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_api_trace(rwdts_api_t *apih, char *path);

/*!
 * Create a new transaction block.  Add queries with
 * rwdts_xact_block_add_query(); run it with
 * rwdts_xact_block_execute().
 *
 * @param xact DTS transaction handle
 *
 * @return  address of allocated xact block
 */
rwdts_xact_block_t*
rwdts_xact_block_create(rwdts_xact_t *xact);

/// @cond GI_SCANNER
/**
 * rwdts_xact_block_create_gi: (rename-to rwdts_xact_block_create)
 * @xact:
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_block_t*
rwdts_xact_block_create_gi(rwdts_xact_t *xact);



/*!
 *  Add a query to a block.  Queries within a block run concurrently;
 *  use another block if ordering is required.
 *
 *
 * @param block Xact block
 * @param xpath xpath string
 * @param action Query action
 * @param flags See RwDtsFlag
 * @param corrid Correlation id.  Nonzero IDs may be used to retrieve specific query results within the block.
 * @param msg Message associated with the query
 *
 * @return  RW Status
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_add_query:
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_block_add_query(rwdts_xact_block_t* block,
                           const char* xpath,
                           RWDtsQueryAction action,
                           uint32_t flags,
                           uint64_t corrid,
                           const ProtobufCMessage* msg);



/*!
 *  Add/append a xact block to the xact handle, keyed with a keyspec.
 *  This variant is handy in various scenarios in member code, where
 *  compile-time checking or scenarios involving computed or inherited
 *  keys may occur.
 *
 * @param block Xact block
 * @param key Keyspec
 * @param action Query action
 * @param flags
 * @param corrid Correlation id
 * @param msg Message associated with the query
 *
 * @return  RW Status
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_add_query_ks:
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_block_add_query_ks(rwdts_xact_block_t* block,
                              const rw_keyspec_path_t* key,
                              RWDtsQueryAction action,
                              uint32_t flags,
                              uint64_t corrid,
                              const ProtobufCMessage* msg);


/*!
 *  Add a query with result reduction expression to a block.
 *
 * @param block Xact block
 * @param xpath xpath string
 * @param action Query action
 * @param flags See RwDtsFlag
 * @param corrid Correlation id.  Nonzero IDs may be used to retrieve specific query results within the block.
 * @param msg Message associated with the query
 *
 * @return  RW Status
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_add_query_reduction:
 * @msg: (type ProtobufC.Message) (transfer none) (nullable) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_block_add_query_reduction(rwdts_xact_block_t* block,
                                     const char* xpath,
                                     RWDtsQueryAction action,
                                     uint32_t flags,
                                     uint64_t corrid,
                                     const ProtobufCMessage* msg);


/// @cond GI_SCANNER
/**
 * rwdts_xact_block_execute_immediate:
 * @callback: (scope notified) 
 * @user_data:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_block_execute_immediate(rwdts_xact_block_t *block,
                                               uint32_t flags,
                                               rwdts_query_event_cb_routine callback,
                                               const void *user_data);
/*!
 * Sends a block of queries off to DTS for evaluation.  This block
 * will be executed immediately.  Use block_execute instead to queue
 * this block for ordered execution in the future.
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_execute_immediate_gi: (rename-to rwdts_xact_block_execute_immediate)
 * @callback: (scope notified) (destroy ud_dtor)
 * @user_data:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_block_execute_immediate_gi(rwdts_xact_block_t *block,
                                        uint32_t flags,
                                        rwdts_query_event_cb_routine callback,
                                        const void *user_data,
                                        GDestroyNotify ud_dtor);

/// @cond GI_SCANNER
/**
 * rwdts_xact_block_execute: 
 * @block:
 * @flags:
 * @callback: (scope notified) 
 * @user_data:
 * @after_block: (nullable) (transfer none) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_block_execute(rwdts_xact_block_t *block,
                                     uint32_t flags,
                                     rwdts_query_event_cb_routine callback,
                                     const void *user_data,
                                     rwdts_xact_block_t *after_block);
/*!
 * Sends a block of queries off to DTS for evaluation.  This block
 * will be executed after the execution of after_block has completed.
 * Specify NULL for (or omit) after_block to append this query at or
 * near the end of the current set of blocks.
 *
 * @param after_block Execute after this block
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_execute_gi: (rename-to rwdts_xact_block_execute)
 * @callback: (scope notified) (destroy ud_dtor)
 * @user_data:
 * @after_block: (nullable) (transfer none) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_block_execute_gi(rwdts_xact_block_t *block,
                                        uint32_t flags,
                                        rwdts_query_event_cb_routine callback,
                                        const void *user_data,
                                        GDestroyNotify ud_dtor,
                                        rwdts_xact_block_t *after_block);

/*!
 * Sends a block of queries off to DTS for evaluation.
 * API to be used with other result reduction APIs to support
 * result reduction expressions.
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_execute_reduction:
 * @callback: (scope notified) (destroy ud_dtor)
 * @user_data:
 * @after_block: (nullable) (transfer none) (optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_block_execute_reduction(rwdts_xact_block_t *block,
                                   uint32_t flags,
                                   rwdts_query_event_cb_routine callback,
                                   const void *user_data,
                                   GDestroyNotify ud_dtor,
                                   rwdts_xact_block_t *after_block);


/*!
 * Get the status of this transaction block.
 *
 * This API provides diagnostic information and progress detail on a
 * currently running transaction block. The transaction can be in any
 * state other than destroyed.
 *
 * @param blk DTS transaction block handle
 * @param status_out status of the transaction
 *
 * @return status RW Status indication
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_get_status_gi: (rename-to rwdts_xact_block_get_status)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_xact_status_t *rwdts_xact_block_get_status_gi(rwdts_xact_block_t*  block);

/// @cond GI_SCANNER
/**
 * rwdts_xact_block_get_status: 
 * Returns: (type RwTypes.RwStatus) (transfer full)
 */
/// @endcond
/* C version, status_out is typically on caller's stack */
rw_status_t rwdts_xact_block_get_status(rwdts_xact_block_t*  block,
                                        rwdts_xact_status_t *state_out);



/*!
 * Get the status of this transaction.
 *
 * This API provides diagnostic information and progress detail on a
 * currently running transaction. The transaction can be in any state
 * other than destroyed.
 *
 * @param xact DTS transaction
 * @param status_out status of the transaction
 *
 * @return status RW Status indication
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_get_status_gi: (rename-to rwdts_xact_get_status)
 * Returns: (transfer full)
 */
/// @endcond
rwdts_xact_status_t *rwdts_xact_get_status_gi(rwdts_xact_t *xact);

/// @cond GI_SCANNER
/**
 * rwdts_xact_get_status: 
 * Returns: (type RwTypes.RwStatus) (transfer full)
 */
/// @endcond
/* C version, status_out is typically on caller's stack */
rw_status_t rwdts_xact_get_status(rwdts_xact_t *xact,
                                  rwdts_xact_status_t *state_out);

/*!
 * Get the more flag for this transaction.
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_get_more_results:
 * @xact: (transfer none)
 * Returns: (transfer none)
 */
/// @endcond
gboolean rwdts_xact_get_more_results(rwdts_xact_t*  xact);

/// @cond GI_SCANNER
/**
 * rwdts_xact_get_id:
 * @xact: (transfer none)
 * Returns: (transfer full)
 */
/// @endcond
char* rwdts_xact_get_id(const rwdts_xact_t *xact);

/*!
 * Get the more flag for this block
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_get_more_results:
 * @block: (transfer none)
 * Returns: (transfer none)
 */
/// @endcond
gboolean rwdts_xact_block_get_more_results(rwdts_xact_block_t *block);

/*!
 * Get the more flag for query within a block
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_block_get_query_more_results:
 * @block: (transfer none)
 * @corrid: (transfer none)
 * Returns: (transfer none)
 */
/// @endcond
gboolean rwdts_xact_block_get_query_more_results(rwdts_xact_block_t *block,uint64_t corrid);

/// @cond GI_SCANNER
/**
 * rwdts_xact_status_get_status:
 * @status: (transfer none)
 * Returns: (transfer full)
 */
/// @endcond
RWDtsXactMainState rwdts_xact_status_get_status(rwdts_xact_status_t *status);

/// @cond GI_SCANNER
/**
 * rwdts_xact_status_get_responses_done:
 * @status: (transfer none)
 * Returns: (transfer none)
 */
/// @endcond
gboolean rwdts_xact_status_get_responses_done(rwdts_xact_status_t *status);

/// @cond GI_SCANNER
/**
 * rwdts_xact_status_get_xact_done:
 * @status: (transfer none)
 * Returns: (transfer none)
 */
/// @endcond
gboolean rwdts_xact_status_get_xact_done(rwdts_xact_status_t *status);

/// @cond GI_SCANNER
/**
 * rwdts_xact_status_get_block:
 * @status: (transfer none)
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_block_t*
rwdts_xact_status_get_block(rwdts_xact_status_t *status);


/*!
 * Abort a DTS transaction from client
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_abort:
 * @xact: (transfer none)
 * @status: (type RwTypes.RwStatus)
 * @errstr: error string (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t rwdts_xact_abort(rwdts_xact_t* xact,
                             rw_status_t status,
                             const char*  errstr);


/******** Member API items **********/

/*!
 *
 * DTS invokes this callback to issue prepares of queries for a transaction
 * This callback is invoked for every query within a transaction received by the DTS member.
 *
 * @param xact_info     DTS transaction related parameters
 * @param action        The action associated with this query
 * @param keyspec       The keyspec associated with this query
 * @param msg           The data ssociated with this query as a protobuf - present for Create and Update
 * @param credits       Max number of entries the member can send
 * @param get_next_key  The next key to fecth
 * @param user_data     User data associated with the registration
 *
 */

/*
 * ATTN - pass these as gpointers for now
 * @keyspec:   (type RwKeyspec.Path)(transfer full)
 * @msg:       (type ProtobufC.Message)
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_prepare_callback:
 * @xact_info:
 * @action:
 * @keyspec:   (type RwKeyspec.Path) (transfer full)
 * @msg:       (type ProtobufC.Message)
 * @user_data:
 *
 */
/// @endcond
typedef
rwdts_member_rsp_code_t (*rwdts_member_prepare_callback)(const rwdts_xact_info_t* xact_info,
                                                         RWDtsQueryAction         action,
                                                         const rw_keyspec_path_t* keyspec,
                                                         const ProtobufCMessage*  msg,
                                                         void*                    user_data);
/*!
 *
 * The DTS transaction precommit  callback.
 * DTS invokes this callback to issue pre-commit for a transaction.
 *
 * @param xact_info  DTS transaction related parameters
 * @param user_data  User data associated with this callback
 *
 * @return           Nothing
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_precommit_callback:
 * @xact_info:
 * @user_data:
 *
 */
/// @endcond
typedef
rwdts_member_rsp_code_t (*rwdts_member_precommit_callback)(const rwdts_xact_info_t* xact_info,
                                                           void*                    user_data);
/*!
 *
 * The DTS transaction commit  callback.
 * DTS invokes this callback to issue commit for a transaction.
 *
 * @param xact_info  DTS transaction related parameters
 * @param user_data  User data associated with this callback
 *
 * @return           No return value
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_commit_callback:
 * @xact_info:
 * @user_data:
 */
/// @endcond
typedef
rwdts_member_rsp_code_t (*rwdts_member_commit_callback)(const rwdts_xact_info_t* xact_info,
                                                        void*                    user_data);

/*!
 *
 * The DTS transaction abort  callback.
 * DTS invokes this callback to issue an abort  for a transaction
 *
 * @param xact_info  DTS transaction related parameters
 * @param user_data  User data associated with this callback
 *
 * @return           No return value
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_abort_callback:
 * @xact_info:
 * @user_data:
 */
/// @endcond
typedef
rwdts_member_rsp_code_t (*rwdts_member_abort_callback)(const rwdts_xact_info_t* xact_info,
                                                       void*                    user_data);


/*!
 * DTS invokes this callback when a registration is ready for use in the application.
 * Both publisher and subscriber registrations receive this callback.
 * publishers and subscribers should expect to receive  transactions against this
 * registration after the reg_ready is called.
 *
 *
 * @param  regh      DTS registration handle
 * @param  reg_state rw_status indicating the status of this registration add.
 * @param  user_data The user data specified at the time of registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_ready_callback:
 * @regh:      
 * @reg_state: (type RwTypes.RwStatus)
 * @user_data: (nullable)
 */
/// @endcond
typedef
void (*rwdts_member_reg_ready_callback)(rwdts_member_reg_handle_t  regh,
                                        rw_status_t                reg_state,
                                        void*                      user_data);

/*!
 * A structure used for registering member callbacks
 */

struct rwdts_member_reg_s {
  rwdts_member_reg_ready_callback  reg_ready;
  rwdts_member_prepare_callback    prepare;
  rwdts_member_precommit_callback  precommit;
  rwdts_member_commit_callback     commit;
  rwdts_member_abort_callback      abort;
  void*                            user_data;
};

typedef struct rwdts_member_reg_s rwdts_member_reg_t;

/*!
 * Registers the specified keyspec as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih    DTS API handle.
 * @param keyspec The keyspec for which the registration is being made.
 * @param xact    A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param desc    The message descriptor for the mesage associated with the keyspec.
 * @param flags   Flags associated with this registration.
 * @param flavor  Shard flavor default is NULL shard flavor.
 * @param keyin   Chunk key for IDENT flavor.
 * @param keylen  key length.
 * @param index   keyspec shard init index.
 * @param start   range shard start range.
 * @param end     range shard end range.
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_keyspec_shard:
 * @apih:
 * @keyspec:(type RwKeyspec.Path) (transfer full)
 * @xact: (nullable)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)
 * @precommit: (scope notified)(closure user_data)(nullable)
 * @commit:    (scope notified)(closure user_data)(nullable)
 * @abort:     (scope notified)(closure user_data)(nullable)
 * @flags:
 * @flavor:
 * @keyin: (type guint64)
 * @keylen:
 * @index:
 * @start: (type gint64)
 * @end:   (type gint64)
 * @user_data: (nullable)
 * @reg_handle : (out callee-allocates) (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_member_register_keyspec_shard(rwdts_api_t*               apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_shard_flavor               flavor,
                                  uint8_t                         *keyin,
                                  size_t                           keylen,
                                  int                              index,
                                  int64_t                          start,
                                  int64_t                          end,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle);
/*!
 * Registers the specified xpath as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih       DTS API handle.
 * @param xpath      The xpath for which the registration is being made.
 * @param xact       A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param flags      Flags associated with this registration.
 * @param flavor     Shard flavor - default is null shard flavor
 * @param keyin      Shard chunk key
 * @param keylen     key length
 * @param index      index of shard registration point
 * @param start      start key for  range shard registration point
 * @param end        end key for rante shard registration point
 * @return           A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_xpath:
 * @apih:
 * @xpath:
 * @xact: (nullable)
 * @flags:
 * @flavor:
 * @keyin: (type guint64)
 * @keylen:
 * @index:
 * @start: (type gint64)
 * @end: (type gint64)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)
 * @precommit: (scope notified)(closure user_data)(nullable)
 * @commit:    (scope notified)(closure user_data)(nullable)
 * @abort:     (scope notified)(closure user_data)(nullable)
 * @user_data:  (nullable)
 * @reg_handle: (out callee-allocates) (transfer full):
 * Returns:     (type RwTypes.RwStatus) (transfer none):
 */
/// @endcond
rw_status_t
rwdts_api_member_register_xpath(rwdts_api_t*                     apih,
                                const char*                      xpath,
                                rwdts_xact_t*                    xact,
                                uint32_t                         flags,
                                rwdts_shard_flavor               flavor,
                                uint8_t                         *keyin,
                                size_t                           keylen,
                                int                              index,
                                int64_t                          start,
                                int64_t                          end,
                                rwdts_member_reg_ready_callback  reg_ready,
                                rwdts_member_prepare_callback    prepare,
                                rwdts_member_precommit_callback  precommit,
                                rwdts_member_commit_callback     commit,
                                rwdts_member_abort_callback      abort,
                                void*                            user_data,
                                rwdts_member_reg_handle_t*       reg_handle);

/*!
 * Registers the specified keyspec as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih    DTS API handle.
 * @param keyspec The keyspec for which the registration is being made.
 * @param xact    A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param desc    The message descriptor for the mesage associated with the keyspec.
 * @param flags   Flags associated with this registration.
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_keyspec:
 * @apih:
 * @keyspec:(type RwKeyspec.Path) (transfer full)
 * @xact: (nullable)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)
 * @precommit: (scope notified)(closure user_data)(nullable)
 * @commit:    (scope notified)(closure user_data)(nullable)
 * @abort:     (scope notified)(closure user_data)(nullable)
 * @flags:
 * @user_data: (nullable)
 * @reg_handle : (out callee-allocates) (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_member_register_keyspec(rwdts_api_t*                     apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle);

typedef struct rwdts_group_s rwdts_group_t;
/*!

 * Registers the specified keypath as a subscriber/publisher/rpc to
 * DTS, as part of a group.
 *
 * See also the group-less rwdts_member_register().
 *
 * @param group   DTS Registration Group  ( needed for reg-spanning xact callbacks, et al )
 * @param keyspec The keyspec for which the registration is being made (to be made a path)
 * @param cb      The callback routine registered for this keyspec
 * @param desc    The message descriptor for the message associated with the keyspec (predefined by the path)
 * @param flags   Flags associated with this registration.
 * @param shard_detail   Sharding details.   TBD
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_group_register_keyspec:
 * @group:
 * @keyspec:    (type gpointer)
 * @reg_ready:  (scope async)(closure user_data)(nullable)
 * @prepare:    (scope notified)(closure user_data)(nullable)
 * @precommit:  (scope notified)(closure user_data)(nullable)
 * @commit:     (scope notified)(closure user_data)(nullable)
 * @abort:      (scope notified)(closure user_data)(nullable)
 * @flags:
 * @user_data:  (optional)(nullable)
 * @reg_handle: (out callee-allocates) (transfer full)
 * Returns:    (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_group_register_keyspec(rwdts_group_t*                  group,
                             const rw_keyspec_path_t*        keyspec,
                             rwdts_member_reg_ready_callback reg_ready,
                             rwdts_member_prepare_callback   prepare,
                             rwdts_member_precommit_callback precommit,
                             rwdts_member_commit_callback    commit,
                             rwdts_member_abort_callback     abort,
                             uint32_t                        flags,
                             void*                           user_data,
                             rwdts_member_reg_handle_t*      reg_handle);
/*!
 * Registers the specified keyspec as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih    DTS API handle.
 * @param keyspec The keyspec for which the registration is being made.
 * @param xact    A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param desc    The message descriptor for the mesage associated with the keyspec.
 * @param flags   Flags associated with this registration.
 * @param flavor  Shard flavor default is NULL shard flavor.
 * @param keyin   Chunk key for IDENT flavor.
 * @param keylen  key length.
 * @param index   keyspec shard init index.
 * @param start   range shard start range.
 * @param end     range shard end range.
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_keyspec_shard_gi: (rename-to rwdts_api_member_register_keyspec_shard)
 * @apih:
 * @keyspec:(type RwKeyspec.Path) (transfer full)
 * @xact: (nullable)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @precommit: (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @commit:    (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @abort:     (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @flags:
 * @flavor:
 * @keyin: (type guint64)
 * @keylen:
 * @index:
 * @start: (type gint64)
 * @end:   (type gint64)
 * @user_data: (nullable)
 * @reg_handle : (out callee-allocates) (transfer full)
 * @dtor: (scope async)(nullable):
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_member_register_keyspec_shard_gi(rwdts_api_t*               apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_shard_flavor               flavor,
                                  uint8_t                         *keyin,
                                  size_t                           keylen,
                                  int                              index,
                                  int64_t                          start,
                                  int64_t                          end,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle,
                                  GDestroyNotify                   dtor);


/*!
 * Registers the specified keyspec as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih    DTS API handle.
 * @param keyspec The keyspec for which the registration is being made.
 * @param xact    A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param desc    The message descriptor for the mesage associated with the keyspec.
 * @param flags   Flags associated with this registration.
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_keyspec_gi: (rename-to rwdts_api_member_register_keyspec)
 * @apih:
 * @keyspec:(type RwKeyspec.Path) (transfer full)
 * @xact: (nullable)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @precommit: (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @commit:    (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @abort:     (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @flags:
 * @user_data: (nullable)
 * @reg_handle : (out callee-allocates) (transfer full)
 * @dtor: (scope async)(nullable):
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_member_register_keyspec_gi(rwdts_api_t*                     apih,
                                  const rw_keyspec_path_t*         keyspec,
                                  rwdts_xact_t*                    xact,
                                  uint32_t                         flags,
                                  rwdts_member_reg_ready_callback  reg_ready,
                                  rwdts_member_prepare_callback    prepare,
                                  rwdts_member_precommit_callback  precommit,
                                  rwdts_member_commit_callback     commit,
                                  rwdts_member_abort_callback      abort,
                                  void*                            user_data,
                                  rwdts_member_reg_handle_t*       reg_handle,
                                  GDestroyNotify                   dtor);

/*!
 * Registers the specified xpath as a subscriber/publisher/rpc to DTS.
 * DTS member API will invoke the specified member event callback when a
 * transaction is received matching the registered keyspec.
 *
 * @param apih       DTS API handle.
 * @param xpath      The xpath for which the registration is being made.
 * @param xact       A transaction to register within.  Timing of regisration effectiveness varies by flavor
 * @param reg_ready  Registration ready callback - When present DTS invokes this callback with cached data
 * @param prepare    prepare callback - When present DTS invokes this callback during prepare phase for every query matching the registration
 * @param precommit  precommit callback - When present DTS invokes this callback when transaction moves to the precommit phase  that matched this registration
 * @param commit     commit callback - When present DTS invokes this callback when transaction moves to the commit phase  that matched this registration
 * @param abort      abort callback - When present DTS invokes this callback when transaction moves to the abort phase  that matched this registration
 * @param flags      Flags associated with this registration.
 * @param flavor     Shard flavor - default is null shard flavor 
 * @param keyin      Shard chunk key  
 * @param keylen     key length 
 * @param index      index of shard registration point  
 * @param start      start key for  range shard registration point  
 * @param end        end key for rante shard registration point  
 * @return           A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_member_register_xpath_gi: (rename-to rwdts_api_member_register_xpath)
 * @apih:
 * @xpath:
 * @xact: (nullable)
 * @flags:
 * @flavor:
 * @keyin: (type guint64)
 * @keylen:
 * @index:
 * @start: (type gint64)
 * @end: (type gint64)
 * @reg_ready: (scope async)(closure user_data)(nullable)
 * @prepare:   (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @precommit: (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @commit:    (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @abort:     (scope notified)(closure user_data)(nullable)(destroy dtor)
 * @user_data:  (nullable)
 * @reg_handle: (out callee-allocates) (transfer full):
 * @dtor:(scope async)(nullable):
 * Returns:     (type RwTypes.RwStatus) (transfer none):
 */
/// @endcond
rw_status_t
rwdts_api_member_register_xpath_gi(rwdts_api_t*                     apih,
                                const char*                      xpath,
                                rwdts_xact_t*                    xact,
                                uint32_t                         flags,
                                rwdts_shard_flavor               flavor,
                                uint8_t                         *keyin,
                                size_t                           keylen,
                                int                              index,
                                int64_t                          start,
                                int64_t                          end,
                                rwdts_member_reg_ready_callback  reg_ready,
                                rwdts_member_prepare_callback    prepare,
                                rwdts_member_precommit_callback  precommit,
                                rwdts_member_commit_callback     commit,
                                rwdts_member_abort_callback      abort,
                                void*                            user_data,
                                rwdts_member_reg_handle_t*       reg_handle,
                                GDestroyNotify                   dtor);

/* There are 0-n groups per apih */

/* There are 0-1 groups permanently referenced by each registration */

/* There are 0-n groups referenced by each transaction, which
   references are added as queries hit registration bearing groups,
   and end as the xact ends */


/*!
 *  DTS group registration structure used in rwdts_group_create() to create a group of DTS registrations
 */


/*!
 *  xact init callback invoked for a new DTS transaction.
 *  Constructs per-transaction per-group application state (aka "scratch")
 *  @param grp  the dts transaction group object
 *  @param xact the dts transaction
 *  @param user_data The user data associated with this callback.
 *                   Constant context for all transactions.
 *                   Typically the tasklet instance pointer,
 *                   or for a library implementing a group, the library's main pointer.
 */

/// @cond GI_SCANNER
/**
 * xact_init_callback:
 * @grp:
 * @xact:
 * @user_data:
 * Returns: (type GValue)(transfer full)
 */
/// @endcond
typedef void* (*xact_init_callback)(rwdts_group_t* grp,
                                    rwdts_xact_t*  xact,
                                    void*          user_data);

/*!
 *  xact init callback invoked for a new DTS transaction.
 *  Constructs per-transaction per-group application state (aka "scratch")
 *  @param grp  the dts transaction group object
 *  @param xact the dts transaction
 *  @param user_data The user data associated with this callback.
 *                   Constant context for all transactions.
 *                   Typically the tasklet instance pointer,
 *                   or for a library implementing a group, the library's main pointer.
 */

/// @cond GI_SCANNER
/**
 * xact_deinit_callback_gi:
 * @grp:
 * @xact:
 * @ctx:
 * @user_data:
 * @scratch:(type GValue)
 */
/// @endcond

typedef void (*xact_deinit_callback_gi)(rwdts_group_t* grp,
                                        rwdts_xact_t*  xact,
                                        void*          user_data,
                                        void *ctx,
                                        GValue*        scratch);
typedef void (*xact_deinit_callback)(rwdts_group_t* grp,
                                     rwdts_xact_t*  xact,
                                     void*          user_data,
                                     void*          scratch);


/* The fundamentally transaction-wide events PRECOMMIT SUBCOMMIT
    COMMIT ABORT can be handled via this callback registered here.
    These events are issued once per xact as opposed to once per
    registration.  With reg groups this becomes semantically
    preferable for many functions. */

/// @cond GI_SCANNER
/**
 * xact_event_callback:
 * @apih:
 * @grp:
 * @xact:
 * @xact_event:
 * @ctx:
 * @scratch:   (type GValue)
 */
/// @endcond
typedef  rwdts_member_rsp_code_t
(*xact_event_callback_gi)(rwdts_api_t*         apih,
                          rwdts_group_t*       grp,
                          rwdts_xact_t*        xact,
                          rwdts_member_event_t xact_event,
                          void*                ctx,
                          GValue *scratch);
typedef  rwdts_member_rsp_code_t
(*xact_event_callback)(rwdts_api_t*         apih,
                       rwdts_group_t*       grp,
                       rwdts_xact_t*        xact,
                       rwdts_member_event_t xact_event,
                       void*                ctx,
                       void*                scratch);

/*!
 * Create a registration group.
 *
 * @param apih The DTS Member API handle.
 * @param cbset A callback set to be copied into the reg group
 */

/// @cond GI_SCANNER
/**
 * rwdts_api_group_create_gi: (rename-to rwdts_api_group_create)
 * @apih:
 * @xact_init:   (scope notified) (closure ctx) (nullable) (destroy xact_init_dtor)
 * @xact_deinit: (scope notified) (closure ctx) (nullable) (destroy xact_deinit_dtor)
 * @xact_event:  (scope notified) (closure ctx) (nullable) (destroy xact_event_dtor)
 * @ctx:
 * @group:       (out) (transfer full)
 * @xact_init_dtor:   (scope async) (nullable)
 * @xact_deinit_dtor: (scope async) (nullable)
 * @xact_event_dtor:  (scope async) (nullable)
 *
 * Returns:    (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_group_create_gi(rwdts_api_t* apih,
                          xact_init_callback    xact_init,
                          xact_deinit_callback  xact_deinit,
                          xact_event_callback   xact_event,
                          void*                 ctx,
                          rwdts_group_t**       group,
                          GDestroyNotify        xact_init_dtor,
                          GDestroyNotify        xact_deinit_dtor,
                          GDestroyNotify        xact_event_dtor);

/// @cond GI_SCANNER
/**
 * rwdts_api_group_create:
 * @apih:
 * @xact_init:   (scope notified) (closure ctx) (nullable) 
 * @xact_deinit: (scope notified) (closure ctx) (nullable) 
 * @xact_event:  (scope notified) (closure ctx) (nullable) 
 * @ctx:
 * @group:       (out) (transfer full)
 *
 * Returns:    (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_group_create(rwdts_api_t* apih,
                       xact_init_callback    xact_init,
                       xact_deinit_callback  xact_deinit,
                       xact_event_callback   xact_event,
                       void*                 ctx,
                       rwdts_group_t**       group);

/*!
 * Destroy a registration group.  Note that registrations must have
 * been cleared out already.
 *
 * @param apih The DTS Member API handle.
 * @param group The group to destroy
 */
void rwdts_group_destroy(rwdts_group_t *group);

/*!

 * Registers the specified keypath as a subscriber/publisher/rpc to
 * DTS, as part of a group. Python / Gi function 
 *
 * See also the group-less rwdts_member_register().
 *
 * @param group   DTS Registration Group  ( needed for reg-spanning xact callbacks, et al )
 * @param keyspec The keyspec for which the registration is being made (to be made a path)
 * @param cb      The callback routine registered for this keyspec
 * @param desc    The message descriptor for the message associated with the keyspec (predefined by the path)
 * @param flags   Flags associated with this registration.
 * @param shard_detail   Sharding details.   TBD
 *
 * @return        A  handle to the member registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_group_register_keyspec_gi: (rename-to rwdts_group_register_keyspec)
 * @group:
 * @keyspec:    (type gpointer)
 * @reg_ready:  (scope async)(closure user_data)(nullable)
 * @prepare:    (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @precommit:  (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @commit:     (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @abort:      (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @user_data:  (optional)(nullable)
 * @reg_handle: (out callee-allocates) (transfer full)
 * @reg_deleted:(scope async)(nullable)
 * Returns:    (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_group_register_keyspec_gi(rwdts_group_t*                  group,
                             const rw_keyspec_path_t*        keyspec,
                             rwdts_member_reg_ready_callback reg_ready,
                             rwdts_member_prepare_callback   prepare,
                             rwdts_member_precommit_callback precommit,
                             rwdts_member_commit_callback    commit,
                             rwdts_member_abort_callback     abort,
                             uint32_t                        flags,
                             void*                           user_data,
                             rwdts_member_reg_handle_t*      reg_handle,
                             GDestroyNotify                  reg_deleted);

/// @cond GI_SCANNER
/**
 * rwdts_group_register_xpath_gi: (rename-to rwdts_group_register_xpath)
 * @group:
 * @xpath:
 * @reg_ready:  (scope async)(closure user_data)(nullable)
 * @prepare:    (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @precommit:  (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @commit:     (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @abort:      (scope notified)(closure user_data)(nullable)(destroy reg_deleted)
 * @user_data:  (nullable) (optional)
 * @reg_handle: (out callee-allocates) (transfer full)
 * @reg_deleted:(scope async)(nullable)
 * Returns:     (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_group_register_xpath_gi(rwdts_group_t*                  group,
                           const char*                     xpath,
                           rwdts_member_reg_ready_callback reg_ready,
                           rwdts_member_prepare_callback   prepare,
                           rwdts_member_precommit_callback precommit,
                           rwdts_member_commit_callback    commit,
                           rwdts_member_abort_callback     abort,
                           uint32_t                        flags,
                           void*                           user_data,
                           rwdts_member_reg_handle_t*      reg_handle,
                           GDestroyNotify                  reg_deleted);
/// @cond GI_SCANNER
/**
 * rwdts_group_register_xpath:
 * @group:
 * @xpath:
 * @reg_ready:  (scope async)(closure user_data)(nullable)
 * @prepare:    (scope notified)(closure user_data)(nullable)
 * @precommit:  (scope notified)(closure user_data)(nullable)
 * @commit:     (scope notified)(closure user_data)(nullable)
 * @abort:      (scope notified)(closure user_data)(nullable)
 * @user_data:  (nullable) (optional)
 * @reg_handle: (out callee-allocates) (transfer full)
 * Returns:     (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_group_register_xpath(rwdts_group_t*                  group,
                           const char*                     xpath,
                           rwdts_member_reg_ready_callback reg_ready,
                           rwdts_member_prepare_callback   prepare,
                           rwdts_member_precommit_callback precommit,
                           rwdts_member_commit_callback    commit,
                           rwdts_member_abort_callback     abort,
                           uint32_t                        flags,
                           void*                           user_data,
                           rwdts_member_reg_handle_t*      reg_handle);

/*!
 * Signal to the api when an application execute phase is complete.
 *
 * Currently there is only one phase defined: registrations.  The
 * group registration will be transactionally applied (and cached
 * loaded etc) when the app uses this API to signal that group
 * registration is complete.

 * @param group     The registration group
 * @param phase     RWDTS_GROUP_PHASE_REGISTER
 */

void
rwdts_group_phase_complete(rwdts_group_t*      group,
                           rwdts_group_phase_t phase);


/* ?? Should not this call be collapsed with the other emerging state transition call? */


/// @cond GI_SCANNER
/**
 * rwdts_xact_info_get_credits:
 * @xact_info:
 */
/// @endcond
uint32_t
rwdts_xact_info_get_credits(const rwdts_xact_info_t *xact_info);

/// @cond GI_SCANNER
/**
 * rwdts_xact_info_get_xact:
 * @xact_info:
 * returns: (transfer full)
 */
/// @endcond
rwdts_xact_t*
rwdts_xact_info_get_xact(const rwdts_xact_info_t *xact_info);

/// @cond GI_SCANNER
/**
 * rwdts_xact_info_get_api:
 * @xact_info:
 * Returns : (transfer none)
 */
/// @endcond
rwdts_api_t*
rwdts_xact_info_get_api(const rwdts_xact_info_t *xact_info);

/// @cond GI_SCANNER
/**
 * rwdts_xact_info_get_apy:
 * @xact_info:
 * Returns : (transfer none)
 */
/// @endcond
RWDtsQueryAction
rwdts_xact_info_get_query_action(const rwdts_xact_info_t *xact_info);


#ifndef __GI_SCANNER__
/*
 *  !!!!!!    Important  !!!!!!!!!
 *
 *  The callbacks in the structure should correpond to the events defined in rwdts_member_event_t (rwdts_query_api.h)
 *  Do not change the order  of these callbacks or add/delete entries without changing rwdts_member_event_t
 */

typedef rwdts_member_rsp_code_t
(*rwdts_member_prepare_cb)(const rwdts_xact_info_t* xact_info, /*! [in] DTS transaction related parameters */
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t* keyspec,   /*! [in] Keyspec associated with this query  */
                           const ProtobufCMessage*  msg        /*! [in] The data associated with the Query as proto */
);
/*!
 *
 * DTS invokes this callback to issue prepares of queries for a transaction
 * This callback is invoked for every query within a transaction received by the DTS member.
 *
 * @param xact_info  DTS transaction related parameters
 * @param action     The action associated with this query
 * @param keyspec    The keyspec associated with this query
 * @param msg        The data ssociated with this query as a protobuf - present for Create and Update
 * @param credits    Max number of entries the member can send
 * @param get_next_key TBD
 */
/// @cond GI_SCANNER
/*
 * rwdts_member_getnext_cb:
 * @xact_info: (transfer full)
 * @action:  
 * @keyspec: (transfer full)
 * @msg:  (transfer full)
 * @credits: 
 * @get_next_key: (transfer full)
 * 
 * @return           See rwdts_member_rsp_code_t
 */
/// @endcond

typedef rwdts_member_rsp_code_t
(*rwdts_member_getnext_cb)(const rwdts_xact_info_t* xact_info, /*! [in] DTS transaction related parameters */
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t* keyspec,   /*! [in] Keyspec associated with this query  */
                           const ProtobufCMessage*  msg,       /*! [in] The data associated with the Query as proto */
                           uint32_t                 credits,
                           void*                    get_next_key);

/*!
 *
 * The DTS transaction precommit  callback.
 * DTS invokes this callback to issue pre-commit for a transaction.
 *
 * @param xact_info  DTS transaction related parameters
 * @param n_crec     Number of pre commit records
 * @param crec       An array of pre commit records
 *
 * @return           See rwdts_member_rsp_code_t.
 */

typedef rwdts_member_rsp_code_t (*rwdts_member_precommit_cb)(
    const rwdts_xact_info_t*      xact_info, /*! [in] DTS transaction related parameters */
    uint32_t                      n_crec,    /*! [in] Number of commit records */
    const rwdts_commit_record_t** crec       /*! [in] commit records */
);

/*!
 *
 * The DTS transaction commit  callback.
 * DTS invokes this callback to issue commit for a transaction.
 *
 * @param xact_info  DTS transaction related parameters
 * @param n_crec     Number of commit records
 * @param crec       An array of commit records
 *
 * @return           See rwdts_member_rsp_code_t.
 */

typedef rwdts_member_rsp_code_t (*rwdts_member_commit_cb)(
    const rwdts_xact_info_t*      xact_info,/*! [in] DTS transaction related parameters */
    uint32_t                      n_crec,   /*! [in] Number of commit records */
    const rwdts_commit_record_t** crec      /*! [in] commit records */
);

/*!
 *
 * The DTS transaction abort  callback.
 * DTS invokes this callback to issue an abort  for a transaction
 *
 * @param xact_info  DTS transaction related parameters
 * @param n_crec     Number of commit records
 * @param crec       An array of commit records
 *
 * @return        See rwdts_member_rsp_code_t.
 */

typedef rwdts_member_rsp_code_t (*rwdts_member_abort_cb)(
    const rwdts_xact_info_t*      xact_info,/*! [in] DTS transaction related parameters */
    uint32_t                      n_crec,   /*! [in] Number of commit records */
    const rwdts_commit_record_t** crec      /*! [in] commit records */
);


/*!
 *
 * DTS invokes this callback when it receives cached object  that the member has
 * subscribed to. For listy registrations the callback will be issued for every object
 * in the list.
 * Do not  use -- being obsoleted by  rwdts_reg_ready_cb
 *
 * @param  regh    DTS registration handle
 * @param  keyspec Keyspec associated with the data
 * @param  msg     The data associated with this calback
 * @param  ur      The user data specified at the time of registration.
 */

typedef void (*rwdts_reg_ready_old_cb) (
    rwdts_member_reg_handle_t  regh,     /*! [in] registration handle */
    const rw_keyspec_path_t*   keyspec,  /*! [in] Keyspec associated with this query  */
    const ProtobufCMessage*    msg,      /*! [in] The data associated with the Query as proto */
    void*                      ud);      /*! [in] user data if any registered against the keyspec */

/*!
 *
 * DTS invokes this callback when it receives cached object  that the member has
 * subscribed to. For listy registrations the callback will be issued for every object
 * in the list.
 *
 * @param  regh    DTS registration handle
 * @param  keyspec Keyspec associated with the data
 * @param  msg     The data associated with this calback
 * @param  ur      The user data specified at the time of registration.
 */

typedef void (*rwdts_reg_ready_cb) (
    rwdts_member_reg_handle_t  regh,       /*! [in] registration handle */
    rw_status_t                reg_status, /*! [in] registration ready callback */
    void*                      ud);        /*! [in] user data if any registered against the keyspec */
/*
 *  !!!!!!    Important  !!!!!!!!!
 *
 *  The callbacks in the structure should correpond to the events defined in rwdts_member_event_t (rwdts_query_api.h)
 *  Do not change the order  of these callbacks or add/delete entries without changing rwdts_member_event_t
 */
/*!
 * DTS member transaction related callbacks and associated user data.
 * DTS members use this structure to register keyspecs as publisher/subscriber through rwdts_member_register.
 * DTS invokes these callbacks when an incoming transaction macthes the registration.
 *
 * These callbacks are invoked when a member is registered as a subscriber an advise query is received from the
 * publisher. The members respond to each state in the xact state machine using the rwdts_member_rsp_code_t.
 *
 * All these callbacks are optional and a member may choose not to register any of these callbacks. The absense of
 * a registered callback is considered by DTS as an ACK.
 *
 *
 */

struct rwdts_member_event_cb_s {
  struct {
    rwdts_member_getnext_cb   prepare;
    rwdts_member_prepare_cb   child;
    rwdts_member_precommit_cb precommit;
    rwdts_member_commit_cb    commit;
    rwdts_member_abort_cb     abort;
    // ATTN -- the following is obsolete - this is maintained to allow app backward compatibility.
    rwdts_reg_ready_old_cb    reg_ready_old; // RIFT-6776 obsoleted callbacks with msg/keyspecs.
    rwdts_reg_ready_cb        reg_ready;
    GDestroyNotify            dtor;
  } cb;
  void *ud;
};
#endif /* __GI_SCANNER__ */

/*!
 * A structure that encapsulates all the registration information into a
 * structure. This is to enable applications make a static array of most of
 * the things to register at compile time, fill in others (mostly the ud part?)
 */
typedef struct rwdts_registration_s {
  rw_keyspec_path_t*                keyspec; /*!< The keyspec being registered */
  const ProtobufCMessageDescriptor* desc;    /*!< Descriptor of message to be retured */
  uint32_t                          flags;   /*!< Flags to use for this registration */
  rwdts_member_event_cb_t           callback;/*!< List of callbacks, with userdata */
} rwdts_registration_t;


/*!
 * Send response an incoming prepare using xapth  as key
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_info_respond_keyspec:
 * @xact_info:
 * @rsp_code:
 * @keyspec: (nullable)(type RwKeyspec.Path) (transfer none)
 * @msg:     (nullable)(type ProtobufC.Message)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_info_respond_keyspec(const rwdts_xact_info_t* xact_info,
                                rwdts_xact_rsp_code_t    rsp_code,
                                const rw_keyspec_path_t* keyspec,
                                const ProtobufCMessage*  msg);

/*!
 * returns true if the query is 3 phase transactional
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_info_transactional:
 * @xact_info:
 *
 * Returns: (type gboolean)
 **/
/// @endcond
gboolean
rwdts_xact_info_transactional(const rwdts_xact_info_t *xact_info);

/*!
 * Send error response to
 * an incoming prepare using keyspec
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_info_send_error_keyspec:
 * @xact_info:
 * @status: (type RwTypes.RwStatus)
 * @keyspec: (type RwKeyspec.Path) (nullable) (transfer none)
 * @errstr: (nullable) (transfer none)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_info_send_error_keyspec(const rwdts_xact_info_t* xact_info,
                                   rw_status_t status,
                                   const rw_keyspec_path_t* keyspec,
                                   const char*  errstr);

/*!
 * Send error response to
 * an incoming prepare using xpath
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_info_send_error_xpath:
 * @xact_info:
 * @status: (type RwTypes.RwStatus)
 * @xpath: (nullable) (transfer none)
 * @errstr: (nullable) (transfer none)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond

rw_status_t
rwdts_xact_info_send_error_xpath(const rwdts_xact_info_t*  xact_info,
                                 rw_status_t status,
                                 const char*  xpath,
                                 const char*  errstr);

/*!
 * Send response to an incoming prepare/commit/abort using xapth as key
 *
 * @param xact_info  transaction context
 * @param rsp_code   response code
 * @param xpath      xpath if present
 * @param msg        The response mesage if present
 *
 * @return rw status indicating success or failure code
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_xact_info_respond_xpath:
 * @xact_info:
 * @rsp_code:
 * @xpath:  (nullable)
 * @msg:    (type ProtobufC.Message)(nullable)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_xact_info_respond_xpath(const rwdts_xact_info_t* xact_info,
                              rwdts_xact_rsp_code_t    rsp_code,
                              const char*              xpath,
                              const ProtobufCMessage*  msg);


/*!
 * This callback is invoked by DTS when Appconf  gets a new transaction
 *
 * @param ac    Appconf handle
 * @param xact
 * @param ctx   the context associated with the appconf  that was specified during create
 * @return nothing
 *
 */
/// @cond GI_SCANNER
/**
 * appconf_xact_init_gi:
 * @ac:
 * @xact:
 * @user_data:
 * Returns: (transfer full)
 */
/// @endcond
typedef GValue *(*appconf_xact_init_gi)(rwdts_appconf_t* ac,
                                        rwdts_xact_t*    xact,
                                        void*          user_data);
typedef void *(*appconf_xact_init)(rwdts_appconf_t* ac,
                                   rwdts_xact_t*    xact,
                                   void*            ctx);

/*!
 * This callback is invoked by DTS when Appconf  initiated transaction is finished
 *
 * @param ac    Appconf handle
 * @param xact
 * @param ctx   the context associated with the appconf  that was specified during create
 * @return nothing
 *
 */
/// @cond GI_SCANNER
/**
 * appconf_xact_deinit_gi:
 * @ac:
 * @xact:
 * @user_data:
 * @scratch: (transfer full)
 */
/// @endcond
typedef void (*appconf_xact_deinit_gi)(rwdts_appconf_t* ac,
                                       rwdts_xact_t*    xact,
                                       void *user_data,
                                       GValue* scratch);
typedef void (*appconf_xact_deinit)(rwdts_appconf_t* ac,
                                    rwdts_xact_t*    xact,
                                    void*            ctx,
                                    void*            scratch);

/*!
 * This callback is invoked by DTS when Appconf  starts validation phase
 *
 * @param ac    Appconf handle
 * @param xact
 * @param ctx   the context associated with the appconf  that was specified during create
 * @return nothing
 *
 */
/// @cond GI_SCANNER
/**
 * appconf_config_validate_gi:
 * @apih:
 * @ac:
 * @user_data:
 * @xact:
 * @scratch: (transfer full)
 */
/// @endcond
typedef void (*appconf_config_validate_gi)(rwdts_api_t*     apih,
                                           rwdts_appconf_t* ac,
                                           rwdts_xact_t*    xact,
                                           void *user_data,
                                           GValue *scratch);
typedef void (*appconf_config_validate)(rwdts_api_t*     apih,
                                        rwdts_appconf_t* ac,
                                        rwdts_xact_t*    xact,
                                        void*            ctx,
                                        void*            scratch);

/*!
 * This callbaclk is invoked by DTS when Appconf  starts apply phase
 *
 * @param ac    Appconf handle
 * @param xact
 * @param ctx   the context associated with the appconf  that was specified during create
 * @return nothing
 *
 */
/// @cond GI_SCANNER
/**
 * appconf_config_apply_gi:
 * @apih:
 * @ac:
 * @xact: (transfer none)(nullable)
 * @action:
 * @user_data:
 * @scratch: (transfer full)(nullable)
 */
/// @endcond
typedef void (*appconf_config_apply_gi)(rwdts_api_t *apih,
                                        rwdts_appconf_t* ac,
                                        rwdts_xact_t*    xact,
                                        rwdts_appconf_action_t action,
                                        void*            user_data,
                                        GValue*     scratch);
typedef void (*appconf_config_apply)(rwdts_api_t *apih,
                                     rwdts_appconf_t* ac,
                                     rwdts_xact_t*    xact,
                                     rwdts_appconf_action_t action,
                                     void*            ctx,
                                     void*            scratch);

/*!
 * This callbaclk is invoked by DTS when Appconf  gets a prepare
 *
 * @param apih         DTS API handle
 * @param ac           Appconf handle
 * @param xact
 * @param xact_info    DTS Query handle   # Caution only valid until end of last response / prepare (?)
 * @param keyspec      DTS Pb message
 * @param ctx          The context associated with the appconf  that was specified during create
 * @param scratch_in   The scratch associated with this transaction
 * @return nothing
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_prepare_cb_gi:
 * @apih: (transfer none)
 * @ac: (transfer none)
 * @xact: (transfer none)
 * @xact_info: (transfer full)
 * @keyspec: (type RwKeyspec.Path)(transfer full)
 * @pbmsg: (type ProtobufC.Message)(nullable)
 * @user_data:
 * @scratch_in: (transfer full)
 * @prepare_ud: (nullable) (optional)
 */
/// @endcond
typedef void (*rwdts_appconf_prepare_cb_gi)(rwdts_api_t*            apih,
                                            rwdts_appconf_t*        ac,
                                            rwdts_xact_t*           xact,
                                            const rwdts_xact_info_t *xact_info,
                                            rw_keyspec_path_t*      keyspec,
                                            const ProtobufCMessage* pbmsg,
                                            void*                   user_data,
                                            GValue*                 scratch_in,
                                            void *prepare_ud);
typedef void (*rwdts_appconf_prepare_cb_t)(rwdts_api_t*            apih,
                                           rwdts_appconf_t*        ac,
                                           rwdts_xact_t*           xact,
                                           const rwdts_xact_info_t *xact_info,
                                           rw_keyspec_path_t*      keyspec,
                                           const ProtobufCMessage* pbmsg,
                                           void*                   ctx,
                                           void*                   scratch_in);
/*!
 * Create an AppConf group.
 *
 * @param apih    The DTS Member API handle.
 * @param init    The transaction init callback
 * @param deinit  The transaction init callback
 * @param valiate The transaction validate callback
 * @param apply   The transaction apply callback
 * @param appconf The appconf handle
 * @param appconf_destroyed  A callback when appconf is destroyed
 *
 * @return rw_status_t
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_appconf_group_create_gi: (rename-to rwdts_api_appconf_group_create)
 * @xact: (nullable)
 * @init:     (scope notified) (closure user_data) (nullable) (destroy xact_init_dtor)
 * @deinit:   (scope notified) (closure user_data) (nullable) (destroy xact_deinit_dtor)
 * @validate: (scope notified) (closure user_data) (nullable) (destroy config_validate_dtor)
 * @apply:    (scope notified) (closure user_data) (nullable) (destroy config_apply_dtor)
 * @user_data:
 * @appconf:  (out) (transfer full)
 * @xact_init_dtor: (scope async) (nullable)
 * @xact_deinit_dtor: (scope async) (nullable)
 * @config_validate_dtor: (scope async) (nullable)
 * @config_apply_dtor: (scope async) (nullable)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_appconf_group_create_gi(rwdts_api_t*            apih,
                                  rwdts_xact_t*           xact,
                                  appconf_xact_init_gi    init,
                                  appconf_xact_deinit_gi  deinit,
                                  appconf_config_validate_gi validate,
                                  appconf_config_apply_gi    apply,
                                  void*                   user_data,
                                  rwdts_appconf_t**       appconf,
                                  GDestroyNotify xact_init_dtor,
                                  GDestroyNotify xact_deinit_dtor,
                                  GDestroyNotify config_validate_dtor,
                                  GDestroyNotify config_apply_dtor);
/// @cond GI_SCANNER
/**
 * rwdts_api_appconf_group_create:
 * @xact: (nullable)
 * @init:     (scope notified) (closure user_data) (nullable) 
 * @deinit:   (scope notified) (closure user_data) (nullable)
 * @validate: (scope notified) (closure user_data) (nullable)
 * @apply:    (scope notified) (closure user_data) (nullable)
 * @user_data:
 * @appconf:  (out) (transfer full)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_appconf_group_create(rwdts_api_t*            apih,
                               rwdts_xact_t*           xact,
                               appconf_xact_init       init,
                               appconf_xact_deinit     deinit,
                               appconf_config_validate validate,
                               appconf_config_apply    apply,
                               void*                   user_data,
                               rwdts_appconf_t**       appconf);

/*!
 * Register an appconf using xpath
 *
 * @param ac      appconf handle
 * @param ks      keyspec associated woith this registration
 * @param flags   The flags associated with the registration
 * @param prepare The prepare callback associated with this registration
 *
 * @return rw_status indicating success or failure code
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_register_xpath_gi: (rename-to rwdts_appconf_register_xpath)
 * @ac:
 * @xpath:
 * @flags:
 * @prepare:(scope notified)(nullable)(closure nothing)(destroy prepare_dtor)
 * @nothing:
 * @prepare_dtor: (scope async) (nullable)
 * @handle: (out callee-allocates)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_appconf_register_xpath_gi(rwdts_appconf_t*              ac,
                                const char*                   xpath,
                                uint32_t                      flags,
                                rwdts_appconf_prepare_cb_gi   prepare,
                                void *nothing,
                                GDestroyNotify prepare_dtor,
                                rwdts_member_reg_handle_t*    handle);
/// @cond GI_SCANNER
/**
 * rwdts_appconf_register_xpath:
 * @ac:
 * @xpath:
 * @flags:
 * @prepare:(scope notified)(nullable)
 * @handle: (out callee-allocates)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_appconf_register_xpath(rwdts_appconf_t*              ac,
                             const char*                   xpath,
                             uint32_t                      flags,
                             rwdts_appconf_prepare_cb_t    prepare,
                             rwdts_member_reg_handle_t*    handle);
/*!
 * Register an appconf using keyspec
 *
 * @param ac      appconf handle
 * @param ks      keyspec associated woith this registration
 * @param flags   The flags associated with the registration
 * @param prepare The prepare callback associated with this registration
 *
 * @return rw_status indicating success or failure code
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_register_keyspec_gi: (rename-to rwdts_appconf_register_keyspec)
 * @ac:
 * @ks:
 * @flags:
 * @prepare:(scope notified)(nullable)(closure nothing)(destroy prepare_dtor)
 * @nothing:
 * @prepare_dtor:
 * @handle: (out callee-allocates) (transfer full)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_appconf_register_keyspec_gi(rwdts_appconf_t*              ac,
                                  const rw_keyspec_path_t*      ks,
                                  uint32_t                      flags,
                                  rwdts_appconf_prepare_cb_gi   prepare,
                                  void*                         nothing,
                                  GDestroyNotify                prepare_dtor,
                                  rwdts_member_reg_handle_t*    handle);
/// @cond GI_SCANNER
/**
 * rwdts_appconf_register_keyspec:
 * @ac:
 * @ks:
 * @flags:
 * @prepare:(scope notified)(nullable)
 * @handle: (out callee-allocates) (transfer full)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
rw_status_t
rwdts_appconf_register_keyspec(rwdts_appconf_t*              ac,
                               const rw_keyspec_path_t*      ks,
                               uint32_t                      flags,
                               rwdts_appconf_prepare_cb_t    prepare,
                               rwdts_member_reg_handle_t*    handle);

/*!
 * Signal to the AppConf layer when an application execute phase is
 * complete.  Currently there is only one phase defined:
 * registrations.  Appconf will proceed to playback / install the
 * current configuration when registration completes.
 * @param ac     The appconf group
 * @param phase RWDTS_APPCONF_PHASE_REGISTER
 */

/// @cond GI_SCANNER
/**
 * rwdts_appconf_phase_complete:
 * @ac:
 * @phase:
 */
/// @endcond
void
rwdts_appconf_phase_complete(rwdts_appconf_t*           ac,
                             rwdts_appconf_phase_t      phase);

/*!
 * Signal the successful treatment of a prepare action.
 * @param ac     The appconf group
 * @param xact_info  The xact query-specific info handle from the orginal prepare call.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_prepare_complete_ok:
 * @ac:
 * @xact_info:
 */
/// @endcond
void rwdts_appconf_prepare_complete_ok(rwdts_appconf_t*     ac,
                                       const rwdts_xact_info_t*   xact_info);

/*!
 * Signal that a prepare action did not apply to this group.  If all
 * prepare callbacks so return na, this group will be excluded
 * outright from various validate/install/precommit/commit/abort events.
 *
 * @param ac     The appconf group
 * @param xact_info  The xact query-specific info handle from the orginal prepare call.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_prepare_complete_na:
 * @ac:
 * @xact_info:
 */
/// @endcond
void rwdts_appconf_prepare_complete_na(rwdts_appconf_t*     ac,
                                       const rwdts_xact_info_t*   xact_info);

/*!
 * Signal an error with a prepare action.
 * @param ac     The appconf group
 * @param xact_info  The query handle from the orginal prepare call.
 * @param rs The general status code value for the error.
 * @param errstr A human-readable string.  The string is purely for
 *               debugging; production user interfaces will tend be
 *               graphical and/or i18n, so do not rely on the string
 *               in practice.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_prepare_complete_fail:
 * @ac:
 * @xact_info:
 * @rs: (type RwTypes.RwStatus) (transfer none)
 * @errstr:
 */
/// @endcond
void rwdts_appconf_prepare_complete_fail(rwdts_appconf_t*     ac,
                                         const rwdts_xact_info_t   *xact_info,
                                         rw_status_t          rs,
                                         const char *         errstr);

/*!
 * Signal trouble in transaction execution after the prepare phase.
 * While there are individual per-transaction-per-appconf callbacks in
 * these later phases, each phase may still generate any number of
 * issues or errors starting from zero.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appconf_xact_add_issue:
 * @ac:
 * @xact: (nullable)
 * @rs: (type RwTypes.RwStatus) (transfer none)
 * @errstr:
 */
/// @endcond
void rwdts_appconf_xact_add_issue(rwdts_appconf_t*          ac,
                                  rwdts_xact_t*             xact,
                                  rw_status_t               rs,
                                  const char*               errstr);



/*
 * Data member APIs for GI
 */

/*!
 * Create an element at a registraton point.
 * This function need to be executed in the scheduler queue where DTS is running.
 *
 * @param reg  Registration handle at which this object is being created
 * @param xact DTS transaction handle
 * @param pe   The keyspec path entry at which this object needs to be created
 * @param msg  The object to be created as protobuf message
 *
 * @return     A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_create_element:
 * @regh:
 * @pe:     (type gpointer)(nullable)
 * @msg:    (type ProtobufC.Message)
 * @xact:   (nullable)(optional)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_create_element(rwdts_member_reg_handle_t regh,
                                       rw_keyspec_entry_t*       pe,
                                       const ProtobufCMessage*   msg,
                                       rwdts_xact_t*             xact);
/*!
 * Create an element at a registraton point with keyspec
 * This function need to be executed in the scheduler queue where DTS is running.
 *
 * @param regh Registration handle at which this object is being created
 * @param xact DTS transaction handle
 * @param pe   The keyspec path entry at which this object needs to be created
 * @param msg  The object to be created as protobuf message
 *
 * @return     A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_create_element_keyspec:
 * @regh:
 * @keyspec:(type RwKeyspec.Path)(nullable)
 * @msg:    (type ProtobufC.Message)
 * @xact:   (nullable)(optional)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_create_element_keyspec(rwdts_member_reg_handle_t regh,
                                               rw_keyspec_path_t*        keyspec,
                                               const ProtobufCMessage*   msg,
                                               rwdts_xact_t*             xact);

typedef struct rwdts_member_data_advise_cb_s rwdts_member_data_advise_cb_t;
/*!
 * Create/Update/Delete advise at registraton point with keyspec and provide
 * specified callback and userdata
 *
 * @param regh    Registration handle at which this object is being updated.
 * @param action  DTS action
 * @param keyspec The keyspec path entry of the object that need to be updated
 * @param msg     The object to be created as protobuf message
 * @param xact    The DTS transaction handle
 * @param member_advise_cb Member block advise callback fn and userdata
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_data_block_advise_w_cb:
 * @keyspec: (type RwKeyspec.Path)(transfer full)
 * @msg:     (type ProtobufC.Message)
 * @xact:   (nullable)(optional)
 * @member_advise_cb: (nullable)(optional)
 * @Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_data_block_advise_w_cb(rwdts_member_reg_handle_t regh,
                                    RWDtsQueryAction          action,
                                    const rw_keyspec_path_t*  keyspec,
                                    const ProtobufCMessage*   msg,
                                    rwdts_xact_t*             xact,
                                    rwdts_event_cb_t*         member_advise_cb);
/*!
 * Create an element at a registraton point with xpath.
 * This function need to be executed in the scheduler queue where DTS is running.
 *
 * @param regh Registration handle at which this object is being created
 * @param xact DTS transaction handle
 * @param pe   The keyspec path entry at which this object needs to be created
 * @param msg  The object to be created as protobuf message
 *
 * @return     A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_create_element_xpath:
 * @regh:
 * @xpath:
 * @msg:   (type ProtobufC.Message)
 * @xact:  (nullable)(optional)
 * Returns:(type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_create_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             rwdts_xact_t*             xact);
/*!
 * Update an element at registraton point.
 *
 * @param regh  Registration handle at which this object is being updated.
 * @param xact  DTS transaction handle
 * @param pe    The keyspec path entry  of the object which need to be updated.
 * @param msg   The object to be created as protobuf message
 * @param flags flags indicate how to update this node - RWDTS_FLAG_MERGE|RWDTS_FLAG_REPLACE - default op is to merge
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_update_element:
 * @regh:
 * @pe:  (type gpointer)(nullable)
 * @msg: (type ProtobufC.Message)
 * @xact:(nullable)(optional)
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_update_element(rwdts_member_reg_handle_t regh,
                                       rw_keyspec_entry_t*       pe,
                                       const ProtobufCMessage*   msg,
                                       uint32_t                  flags,
                                       rwdts_xact_t*             xact);

/*!
 * Update an element at registraton point with keyspec.
 *
 * @param regh    Registration handle at which this object is being updated.
 * @param xact    DTS transaction handle
 * @param keyspec The keyspec path entry of the object that need to be updated
 * @param msg     The object to be created as protobuf message
 * @param flags   flags indicate how to update this node - RWDTS_FLAG_MERGE|RWDTS_FLAG_REPLACE - default op is to merge
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_update_element_keyspec:
 * @regh:
 * @keyspec: (type RwKeyspec.Path)(transfer full)
 * @msg:     (type ProtobufC.Message)
 * @xact:    (nullable)(optional)
 * @Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_update_element_keyspec(rwdts_member_reg_handle_t regh,
                                               const rw_keyspec_path_t*  keyspec,
                                               const ProtobufCMessage*   msg,
                                               uint32_t                  flags,
                                               rwdts_xact_t*             xact);
/*!
 * Update an element at registraton point with xpath.
 *
 * @param regh  Registration handle at which this object is being updated.
 * @param xact  DTS transaction handle
 * @param xpath The xpath for this object
 * @param msg   The object to be created as protobuf message
 * @param flags flags indicate how to update this node - RWDTS_FLAG_MERGE|RWDTS_FLAG_REPLACE - default op is to merge
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_update_element_xpath:
 * @regh:
 * @xpath:
 * @msg:     (type ProtobufC.Message)
 * @xact:    (nullable)(optional)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_update_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             uint32_t                  flags,
                                             rwdts_xact_t*             xact);

/*!
 * Delete an element at a registration point
 *
 * @param regh  Registration handle at which this object is being updated.
 * @param xact  DTS transaction handle
 * @param xpath The xpath for this object
 * @param msg   The object to be created as protobuf message
 * @param flags flags indicate how to update this node - RWDTS_FLAG_MERGE|RWDTS_FLAG_REPLACE - default op is to merge
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_delete_element:
 * @regh:
 * @pe:      (type gpointer) (nullable)
 * @msg:     (type ProtobufC.Message)(nullable)(optional)
 * @xact:    (nullable)(optional)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_delete_element(rwdts_member_reg_handle_t regh,
                                       const rw_keyspec_entry_t* pe,
                                       const ProtobufCMessage*   msg,
                                       rwdts_xact_t*             xact);
/*!
 * Delete an element at a registration point using keyspec
 *
 * @param regh  Registration handle at which this object is being updated.
 * @param xact  DTS transaction handle
 * @param xpath The xpath associated with this object
 * @param msg   The object to be created as protobuf message
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_delete_element_keyspec:
 * @regh:
 * @keyspec: (type RwKeyspec.Path)
 * @msg:     (type ProtobufC.Message)(optional)
 * @xact:    (nullable)(optional)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_delete_element_keyspec(rwdts_member_reg_handle_t regh,
                                               const rw_keyspec_path_t*  keyspec,
                                               const ProtobufCMessage*   msg,
                                               rwdts_xact_t*             xact);
/*!
 * Delete an element at a registration point using xpath
 *
 * @param regh  Registration handle at which this object is being updated.
 * @param xact  DTS transaction handle
 * @param xpath The xpath for this object
 * @param msg   The object to be created as protobuf message
 *
 * @return      A status indicating succes/failure
 *
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_delete_element_xpath:
 * @regh:
 * @xpath:
 * @msg:     (type ProtobufC.Message)(nullable)(optional)
 * @xact:    (nullable)(optional)
 *
 * Returns:  (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
//
rw_status_t
rwdts_member_reg_handle_delete_element_xpath(rwdts_member_reg_handle_t regh,
                                             const char*               xpath,
                                             const ProtobufCMessage*   msg,
                                             rwdts_xact_t*             xact);
/*!
 * Gets an element at registration point  with  path entry
 *
 * @param regh Registration handle at which this object is being retrieved
 * @param xact DTS transaction handle
 * @param pe The keyspec path entry  of the object which need to be retrieved.
 * @param out_keyspec output keyspec
 *
 * @returns ProtobufCMessage
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_element:
 * @regh:
 * @pe: (nullable)
 * @out_msg: (out) (type ProtobufC.Message)
 * @out_keyspec: (out) (type RwKeyspec.Path) (transfer none)
 *
 * Returns: (type RwTypes.RwStatus)(transfer none)
 */
/// @endcond

rw_status_t
rwdts_member_reg_handle_get_element(rwdts_member_reg_handle_t  regh,
                                    rw_keyspec_entry_t*        pe,
                                    rwdts_xact_t*              xact,
                                    rw_keyspec_path_t**        out_keyspec,
                                    const ProtobufCMessage**   out_msg);
/*!
 * Gets an element at registration point  with keyspec
 *
 * @param regh Registration handle at which this object is being retrieved
 * @param xact DTS transaction handle
 * @param keyspec The keyspec path of the object which need to be retrieved.
 * @param out_msg output message
 * @param out_keyspec output keyspec
 *
 * @returns a status indicating success or failure
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_element_keyspec:
 * @regh:
 * @keyspec: (type RwKeyspec.Path) (transfer none)(nullable)
 * @xact: (nullable)(optional)
 * @out_msg: (out) (type ProtobufC.Message)
 * @out_keyspec: (out)(type RwKeyspec.Path) (transfer none)
 *
 * Returns: (type RwTypes.RwStatus)(transfer none)
 */
/// @endcond

rw_status_t
rwdts_member_reg_handle_get_element_keyspec(rwdts_member_reg_handle_t  regh,
                                            rw_keyspec_path_t*         keyspec,
                                            rwdts_xact_t*              xact,
                                            rw_keyspec_path_t**        out_keyspec,
                                            const ProtobufCMessage**   out_msg);
/*!
 * Gets an element at registration point  with xpath
 *
 * @param regh Registration handle at which this object is being retrieved
 * @param xact DTS transaction handle
 * @param pe The xpath key of the object which need to be retrieved.
 * @param out_keyspec output keyspec
 *
 * @returns ProtobufCMessage
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_element_xpath:
 * @regh:
 * @xpath:(nullable)
 * @xact:(nullable)(optional)
 * @out_msg: (out) (type ProtobufC.Message)
 * @out_keyspec:(out)(type RwKeyspec.Path) (transfer full)
 *
 * Returns: (type RwTypes.RwStatus)(transfer none)
 */
/// @endcond

rw_status_t
rwdts_member_reg_handle_get_element_xpath(rwdts_member_reg_handle_t  regh,
                                          const char*                xpath,
                                          rwdts_xact_t*              xact,
                                          rw_keyspec_path_t**        out_keyspec,
                                          const ProtobufCMessage**   out_msg);

/*!
 * Get the cursor at a element registration
 * @param regh Registration handle at which this object is being retrieved
 * @param cursor passing a null value for cursor causes the API to return the first element in the list
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_cursor:
 * @regh:
 * @xact:(nullable)(optional)
 *
 */
/// @endcond

rwdts_member_cursor_t*
rwdts_member_reg_handle_get_cursor(rwdts_member_reg_handle_t regh,
                                   rwdts_xact_t*             xact);


/*!
 *  Delete the cursor(s) allocated using rwdts_member_reg_handle_get_cursor
 * @param regh Registration handle at which this object is being retrieved
 * @param cursor passing a null value for cursor causes the API to return the first element in the list
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_delete_cursors:
 * @regh:
 * @xact:(nullable)(optional)
 *
 */
/// @endcond

void
rwdts_member_reg_handle_delete_cursors(rwdts_member_reg_handle_t regh,
                                       rwdts_xact_t*             xact);

/*!
 * Reset the cursor to the begining of the list.
 * @param cursor/iteror object to be reset
 */

void
rwdts_member_cursor_reset(rwdts_member_cursor_t *cursor);

/*!
 * Gets the next list element at a registration point
 * When the cursor is based on transaction, the next element at registration
 * point within the transaction is returned.
 *
 *
 * @param regh Registration handle at which this object is being retrieved
 * @param xact DTS transaction handle
 * @param cursor opened using rwdts_member_data_get_cursor()
 * @param out_keyspec the keyspec associated with the object
 *
 * @return ProtobufCMessage
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_next_element:
 * @out_keyspec:(out)(type RwKeyspec.Path) (transfer none)
 * @xact: (nullable)(optional)
 *
 * Returns: (type ProtobufC.Message)
 *
 */
/// @endcond
const ProtobufCMessage*
rwdts_member_reg_handle_get_next_element(rwdts_member_reg_handle_t regh,
                                         rwdts_member_cursor_t*    cursor,
                                         rwdts_xact_t*             xact,
                                         rw_keyspec_path_t**       out_keyspec);
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_deregister:
 * @regh:
 * Returns: (type RwTypes.RwStatus) (transfer none):
 */
/// @endcond
rw_status_t
rwdts_member_reg_handle_deregister(rwdts_member_reg_handle_t regh);

/*!
 * Gets  a keyspec from an xpath
 *
 *
 * @param apih DTS api handle
 * @param out_keyspec the keyspec associated with the xpath
 *
 * @return  Rw status
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_keyspec_from_xpath:
 * @apih:
 * @out_keyspec:(out)(type RwKeyspec.Path) (transfer full)
 *
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_api_keyspec_from_xpath(rwdts_api_t*        apih,
                             const char*         xpath,
                             rw_keyspec_path_t** out_keyspec);

/*!
 * This API is invoked by DTS to retreive the next PE key for the application data-store
 * when the application has registered to use the AppData.
 * 
 *
 * @param  pe PathEntry key.
 * @param  pe_out The next pathentry in the Appdata datastore
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_getnext_pe:
 * @pe:      
 * @pe_out: (out) (transfer full)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_getnext_pe) (rw_keyspec_entry_t *pe,
                                            rw_keyspec_entry_t **pe_out,
                                            void *ud);

/*!
 * This API is invoked by DTS to get the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 * 
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry as key  
 * @param  out_ref Data out provided by the Appdata for the PE key.
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_pe_rwref_take:
 * @shard:
 * @pe:   
 * @out_ref: (out) (type ProtobufC.Message) (transfer none) 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_pe_rwref_take)(rwdts_shard_handle_t *shard,
                                                   rw_keyspec_entry_t *pe,
                                                   ProtobufCMessage **out_ref,
                                                   void *ud);

/*!
 * This API is invoked by DTS to release the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 * 
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry as key 
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_pe_rwref_put:
 * @shard:   
 * @pe:      
 * @out_ref:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_pe_rwref_put)(rwdts_shard_handle_t *shard,
                                                  rw_keyspec_entry_t *pe,
                                                  ProtobufCMessage *out_ref,
                                                  void *ud);

/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has 
 * registered to use the AppData.
 * 
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry as key 
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_pe_create:
 * @shard:   
 * @pe:      
 * @newmsg:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_pe_create)(rwdts_shard_handle_t *shard,
                                               rw_keyspec_entry_t *pe,
                                               ProtobufCMessage *newmsg,
                                               void *ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 * 
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry as key 
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_pe_delete:
 * @shard:   
 * @pe:      
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_pe_delete)(rwdts_shard_handle_t *shard,
                                               rw_keyspec_entry_t *pe,
                                               void *ud);

/*!
 * This API is invoked by application to register for AppData with PathEntry and key
 * and safe way of accessing the data.
 * 
 * @param  shard   Shard handle
 * @param  getnext AppData Pathentry getnext API
 * @param  take    AppData Pathentry take API
 * @param  put     AppData Pathentry put API 
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_safe_pe:
 * @shard:   
 * @getnext: (scope notified) (closure ud) (nullable) (destroy dtor)
 * @take:    (scope notified) (closure ud) (nullable) (destroy dtor)
 * @put:     (scope notified) (closure ud) (nullable) (destroy dtor)
 * @create:  (scope notified) (closure ud) (nullable) (destroy dtor)
 * @delete_fn: (scope notified) (closure ud) (nullable) (destroy dtor)
 * @ud:      (nullable)
 * @dtor: (scope async) (nullable):
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_safe_pe(rwdts_shard_handle_t *shard,
                                            rwdts_appdata_cb_getnext_pe getnext,
                                            rwdts_appdata_cb_safe_pe_rwref_take take,
                                            rwdts_appdata_cb_safe_pe_rwref_put put, 
                                            rwdts_appdata_cb_safe_pe_create create,
                                            rwdts_appdata_cb_safe_pe_delete delete_fn,
                                            void* ud,
                                            GDestroyNotify dtor);


/*!
 * This API is invoked by DTS to retreive the next Keyspec key for the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  ks_in   Keyspec key.
 * @param  ks_out  The next keyspec in the Appdata datastore
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_getnext_keyspec:
 * @ks_in:    
 * @ks_out_callerfrees: (type RwKeyspec.Path)(out callee-allocates)(transfer full)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_getnext_keyspec)(const rw_keyspec_path_t* ks_in,
                                                rw_keyspec_path_t** ks_out_callerfrees,
                                                void* ud);

/*!
 * This API is invoked by DTS to get the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec as key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_ks_rwref_take:
 * @shard:   
 * @ks:      (type RwKeyspec.Path)
 * @out_ref:  (out) (type ProtobufC.Message)(transfer none)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_ks_rwref_take)(rwdts_shard_handle_t* shard,
                                                   rw_keyspec_path_t* ks,
                                                   ProtobufCMessage** out_ref,
                                                   void* ud);

/*!
 * This API is invoked by DTS to release the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec as key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_ks_rwref_put:
 * @shard:   
 * @ks:      (type RwKeyspec.Path)
 * @out_ref:  (type ProtobufC.Message)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_ks_rwref_put)(rwdts_shard_handle_t* shard,
                                                  rw_keyspec_path_t* ks,
                                                  ProtobufCMessage* out_ref,
                                                  void* ud);

/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (KS)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec as key
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_ks_create:
 * @shard:   
 * @ks:      (type RwKeyspec.Path)
 * @newmsg:  (type ProtobufC.Message)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_ks_create)(rwdts_shard_handle_t* shard,
                                               rw_keyspec_path_t* ks,
                                               ProtobufCMessage* newmsg,
                                               void* ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (KS)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec as key
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_ks_delete:
 * @shard:   
 * @ks:      (type RwKeyspec.Path)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_ks_delete)(rwdts_shard_handle_t* shard,
                                               rw_keyspec_path_t* ks,
                                               void* ud);


/*!
 * This API is invoked by application to register for AppData with Keyspec and key
 * and safe way of accessing the data.
 *
 * @param  shard_handle  Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  take    AppData Pathentry take API
 * @param  put     AppData Pathentry put API
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 * @param  dtor    Destroy function
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_safe_keyspec:
 * @shard_handle:
 * @getnext: (scope notified) (closure ud) (nullable) (destroy dtor)
 * @take:    (scope notified) (closure ud) (nullable) (destroy dtor)
 * @put:     (scope notified) (closure ud) (nullable) (destroy dtor)
 * @create:  (scope notified) (closure ud) (nullable) (destroy dtor)
 * @delete_fn: (scope notified) (closure ud) (nullable) (destroy dtor)
 * @ud:    (nullable)
 * @dtor: (scope async) (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_safe_keyspec(rwdts_shard_handle_t* shard_handle,
                                                 rwdts_appdata_cb_getnext_keyspec getnext,
                                                 rwdts_appdata_cb_safe_ks_rwref_take take,
                                                 rwdts_appdata_cb_safe_ks_rwref_put put,
                                                 rwdts_appdata_cb_safe_ks_create create,
                                                 rwdts_appdata_cb_safe_ks_delete delete_fn,
                                                 void* ud,
                                                 GDestroyNotify dtor);


/*!
 * This API is invoked by DTS to retreive the next Minikey key for the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  minikey   Minikey key.
 * @param  minikey_out  The next minikey in the Appdata datastore
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_getnext_minikey:
 * @minikey: (type RwKeyspec.RwSchemaMinikey)    
 * @minikey_out:  (type RwKeyspec.RwSchemaMinikey)(out callee-allocates)(transfer full)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_getnext_minikey)(rw_schema_minikey_t *minikey,
                                                rw_schema_minikey_t **minikey_out,
                                                void *ud);

/*!
 * This API is invoked by DTS to get the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey as key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_mk_rwref_take:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @out_ref:  (out) (type ProtobufC.Message)(transfer none)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_mk_rwref_take)(rwdts_shard_handle_t *shard,
                                                   rw_schema_minikey_t *minikey,
                                                   ProtobufCMessage **out_ref,
                                                   void *ud);

/*!
 * This API is invoked by DTS to release the location (pointer) of the application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey as key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_mk_rwref_put:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @out_ref: (type ProtobufC.Message)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_mk_rwref_put)(rwdts_shard_handle_t *shard,
                                                  rw_schema_minikey_t *minikey,
                                                  ProtobufCMessage *out_ref,
                                            void *ud);

/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (MK)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey as key
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_mk_create:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @newmsg:  (type ProtobufC.Message)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_mk_create)(rwdts_shard_handle_t *shard,
                                               rw_schema_minikey_t *minikey,
                                               ProtobufCMessage *newmsg,
                                               void *ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (MK)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey as key
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_safe_mk_delete:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_safe_mk_delete)(rwdts_shard_handle_t *shard,
                                               rw_schema_minikey_t *minikey,
                                               void *ud);


/*!
 * This API is invoked by application to register for AppData with Minikey and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  take    AppData Pathentry take API
 * @param  put     AppData Pathentry put API
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_safe_minikey:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @take:    (scope notified)(closure ud)(nullable)(destroy dtor)
 * @put:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @create:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @delete_fn: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @dtor: (scope async)(nullable)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_safe_minikey(rwdts_shard_handle_t *shard,
                                                 rwdts_appdata_cb_getnext_minikey getnext,
                                                 rwdts_appdata_cb_safe_mk_rwref_take take,
                                                 rwdts_appdata_cb_safe_mk_rwref_put put,
                                                 rwdts_appdata_cb_safe_mk_create create,
                                                 rwdts_appdata_cb_safe_mk_delete delete_fn,
                                                 void* ud,
                                                 GDestroyNotify dtor);


/*!
 * This API is invoked by DTS to get the data from application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntrys key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_pe_rwref_get:
 * @shard:   
 * @pe:      
 * @ref_out: (out) (type ProtobufC.Message) (transfer none)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_pe_rwref_get)(rwdts_shard_handle_t* shard,
                                                    rw_keyspec_entry_t* pe,
                                                    ProtobufCMessage** ref_out,
                                                    void* ud);


/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_pe_create:
 * @shard:   
 * @pe: 
 * @newmsg:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_pe_create)(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_entry_t* pe,
                                                 ProtobufCMessage* newmsg,
                                                 void* ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_pe_delete:
 * @shard:   
 * @pe:      
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_pe_delete)(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_entry_t* pe,
                                                 void* ud);


/*!
 * This API is invoked by application to register for AppData with PathEntry and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  get     AppData Pathentry get API
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_unsafe_pe:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @get:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @create:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @delete_fn: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @dtor: (scope async)(nullable)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_unsafe_pe(rwdts_shard_handle_t* shard,
                                              rwdts_appdata_cb_getnext_pe getnext,
                                              rwdts_appdata_cb_unsafe_pe_rwref_get get,
                                              rwdts_appdata_cb_unsafe_pe_create create,
                                              rwdts_appdata_cb_unsafe_pe_delete delete_fn,
                                              void* ud,
                                              GDestroyNotify dtor);


/*!
 * This API is invoked by DTS to get the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  copy_output Data copied out from application datastore for the PE key.
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_pe_copy_get:
 * @shard:   
 * @pe: 
 * @copy_output: (out) (type ProtobufC.Message) (transfer none)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_pe_copy_get)(rwdts_shard_handle_t* shard,
                                            rw_keyspec_entry_t* pe,
                                            ProtobufCMessage** copy_output,
                                            void* ud);


/*!
 * This API is invoked by DTS to create/delete the data aganist the given key (PE)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  pbdelta Data to be created/updated/deleted in the AppData datastore for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_pe_pbdelta:
 * @shard:   
 * @pe: 
 * @pbdelta:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_pe_pbdelta)(rwdts_shard_handle_t* shard,
                                           rw_keyspec_entry_t* pe,
                                           ProtobufCMessage* pbdelta,
                                           void* ud);

/*!
 * This API is invoked by application to register for AppData with PathEntry and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  getnext AppData Pathentry getnext API
 * @param  copy    AppData Pathentry get API
 * @param  pbdelta  AppData Pathentry create/update/delete API
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  dtor    Destroyer function
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_queue_pe:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @copy:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @pbdelta:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @ud:        (nullable)
 * @dtor: (scope async)(nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_queue_pe(rwdts_shard_handle_t* shard,
                                             rwdts_appdata_cb_getnext_pe getnext,
                                             rwdts_appdata_cb_pe_copy_get copy,
                                             rwdts_appdata_cb_pe_pbdelta pbdelta,
                                             void* ud,
                                             GDestroyNotify  dtor);

/*!
 * This API is invoked by DTS to get the data aganist the given key (keyspec)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  shard   Shard handle
 * @param  ks      Keyspec key
 * @param  copy_output Data copied out from application datastore for the PE key.
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_ks_copy_get:
 * @shard:  
 * @ks: 
 * @copy_output:(out) (type ProtobufC.Message) (transfer none) 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_ks_copy_get)(rwdts_shard_handle_t* shard,
                                            rw_keyspec_path_t* ks,
                                            ProtobufCMessage** copy_output,
                                            void* ud);


/*!
 * This API is invoked by DTS to create/delete the data aganist the given key (keyspec)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec key
 * @param  pbdelta Data to be created/updated/deleted in the AppData datastore for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_ks_pbdelta:
 * @shard:   
 * @ks: 
 * @pbdelta:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_ks_pbdelta)(rwdts_shard_handle_t* shard,
                                           rw_keyspec_path_t* ks,
                                           ProtobufCMessage* pbdelta,
                                           void* ud);

/*!
 * This API is invoked by application to register for AppData with Keyspec and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  copy    AppData Pathentry get API
 * @param  pbdelta  AppData Pathentry create/update/delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_queue_keyspec:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @copy:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @pbdelta:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @dtor: (scope async)(nullable)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_queue_keyspec(rwdts_shard_handle_t* shard,
                                                  rwdts_appdata_cb_getnext_keyspec getnext,
                                                  rwdts_appdata_cb_ks_copy_get copy,
                                                  rwdts_appdata_cb_ks_pbdelta pbdelta,
                                                  void* ud,
                                                  GDestroyNotify  dtor);


/*!
 * This API is invoked by DTS to get the data aganist the given key (minikey)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey      Minikey key
 * @param  copy_output Data copied out from application datastore for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_mk_copy_get:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @copy_output: (out) (type ProtobufC.Message) (transfer none)  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_mk_copy_get)(rwdts_shard_handle_t* shard,
                                            rw_schema_minikey_t* minikey,
                                            ProtobufCMessage** copy_output,
                                            void* ud);


/*!
 * This API is invoked by DTS to create/delete the data aganist the given key (minikey)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  pbdelta Data to be created/updated/deleted in the AppData datastore for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_mk_pbdelta:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @pbdelta:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_mk_pbdelta)(rwdts_shard_handle_t* shard,
                                           rw_schema_minikey_t* minikey,
                                           ProtobufCMessage* pbdelta,
                                           void* ud);

/*!
 * This API is invoked by application to register for AppData with Minikey and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  copy    AppData Pathentry get API
 * @param  pbdelta  AppData Pathentry create/update/delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_queue_minikey:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @copy:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @pbdelta:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @ud:        (nullable)
 * @dtor: (scope async)(nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_queue_minikey(rwdts_shard_handle_t* shard,
                                                  rwdts_appdata_cb_getnext_minikey getnext,
                                                  rwdts_appdata_cb_mk_copy_get copy,
                                                  rwdts_appdata_cb_mk_pbdelta pbdelta,
                                                  void* ud,
                                                  GDestroyNotify  dtor);


/*!
 * This API is invoked by DTS to get the data from application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_ks_rwref_get:
 * @shard:   
 * @ks:      
 * @ref_out: (out) (type ProtobufC.Message) (transfer none) 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_ks_rwref_get)(rwdts_shard_handle_t* shard,
                                                    rw_keyspec_path_t* ks,
                                                    ProtobufCMessage** ref_out,
                                                    void* ud);


/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (keyspec)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec key
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_ks_create:
 * @shard:   
 * @ks: 
 * @newmsg:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_ks_create)(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_path_t* ks,
                                                 ProtobufCMessage* newmsg,
                                                 void* ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (keyspec)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  ks      Keyspec key
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_ks_delete:
 * @shard:   
 * @ks:      
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_ks_delete)(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_path_t* ks,
                                                 void* ud);


/*!
 * This API is invoked by application to register for AppData with Keyspec and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  get     AppData Pathentry get API
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_unsafe_keyspec:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @get:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @create:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @delete_fn: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @dtor: (scope async)(nullable)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_unsafe_keyspec(rwdts_shard_handle_t* shard,
                                                   rwdts_appdata_cb_getnext_keyspec getnext,
                                                   rwdts_appdata_cb_unsafe_ks_rwref_get get,
                                                   rwdts_appdata_cb_unsafe_ks_create create,
                                                   rwdts_appdata_cb_unsafe_ks_delete delete_fn,
                                                   void* ud,
                                                   GDestroyNotify dtor);


/*!
 * This API is invoked by DTS to get the data from application data-store
 * when the application has registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  out_ref Data out provided by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_mk_rwref_get:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @ref_out: (out) (type ProtobufC.Message) (transfer none) 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_mk_rwref_get)(rwdts_shard_handle_t* shard,
                                                    rw_schema_minikey_t* minikey,
                                                    ProtobufCMessage** ref_out,
                                                    void* ud);


/*!
 * This API is invoked by DTS to create an entry for the data aganist the given key (minikey)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  newmsg Data to be created by the Appdata for the PE key.
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_mk_create:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @newmsg:  
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_mk_create)(rwdts_shard_handle_t* shard,
                                                 rw_schema_minikey_t* minikey,
                                                 ProtobufCMessage* newmsg,
                                                 void* ud);

/*!
 * This API is invoked by DTS to delete an entry for the data aganist the given key (minikey)
 * in the application data-store. This is called only when the application has
 * registered to use the AppData.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 */
/// @cond GI_SCANNER
/**
 * rwdts_appdata_cb_unsafe_mk_delete:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
typedef
rw_status_t (*rwdts_appdata_cb_unsafe_mk_delete)(rwdts_shard_handle_t* shard,
                                                 rw_schema_minikey_t* minikey,
                                                 void* ud);


/*!
 * This API is invoked by application to register for AppData with Keyspec and key
 * and safe way of accessing the data.
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  getnext AppData Pathentry getnext API
 * @param  get     AppData Pathentry get API
 * @param  create  AppData Pathentry create API
 * @param  delete_fn AppData Pathentry delete API
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_unsafe_minikey:
 * @shard:   
 * @getnext: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @get:     (scope notified)(closure ud)(nullable)(destroy dtor)
 * @create:  (scope notified)(closure ud)(nullable)(destroy dtor)
 * @delete_fn: (scope notified)(closure ud)(nullable)(destroy dtor)
 * @dtor: (scope async)(nullable)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_unsafe_minikey(rwdts_shard_handle_t* shard,
                                                  rwdts_appdata_cb_getnext_minikey getnext,
                                                  rwdts_appdata_cb_unsafe_mk_rwref_get get,
                                                  rwdts_appdata_cb_unsafe_mk_create create,
                                                  rwdts_appdata_cb_unsafe_mk_delete delete_fn,
                                                  void* ud,
                                                  GDestroyNotify dtor);


/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  pbcm    ProtoBuf message
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_minikey_pbref_install:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey) (transfer none)
 * @pbcm:   (transfer full) 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t 
rwdts_shard_handle_appdata_register_minikey_pbref_install(rwdts_shard_handle_t* shard,
                                                          rw_schema_minikey_t* minikey,
                                                          ProtobufCMessage* pbcm,
                                                          void* ud);

/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *
 *
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  pbcm    ProtoBuf message
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_minikey_pbref_uninstall:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t 
rwdts_shard_handle_appdata_register_minikey_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                            rw_schema_minikey_t* minikey,
                                                            void* ud);

/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  pbcm    ProtoBuf message
 * @param  ud      The user data specified at the time of AppData registration.
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_keyspec_pbref_install:
 * @shard:   
 * @keyspec: 
 * @pbcm: (transfer full)   
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_keyspec_pbref_install(rwdts_shard_handle_t* shard,
                                                          rw_keyspec_path_t* keyspec,
                                                          ProtobufCMessage* pbcm,
                                                          void* ud);

/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *                                     
 * @param  shard   Shard handle                                     
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  keyspec Keyspec key
 * @param  pbcm    ProtoBuf message
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_keyspec_pbref_uninstall:
 * @shard:   
 * @keyspec: 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_keyspec_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                            rw_keyspec_path_t* keyspec,
                                                            void* ud);

/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *
 *
 * @param  shard   Shard handle
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  pathentry PathEntry key
 * @param  pbcm    ProtoBuf message
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_pathentry_pbref_install:
 * @shard:   
 * @pathentry: 
 * @pbcm: (transfer full)   
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_pathentry_pbref_install(rwdts_shard_handle_t* shard,
                                                            rw_keyspec_entry_t* pathentry,
                                                            ProtobufCMessage* pbcm,
                                                            void* ud);

/*!
 * This API is invoked by AppData Create/Delete API to hold a reference to the ProtoBuf
 * msg in AppData space. The reference is held till the uninstall is called.
 *                                     
 * @param  shard   Shard handle                                     
 * @param  ud      The user data specified at the time of AppData registration.
 * @param  pathentry PathEntry key
 * @param  pbcm    ProtoBuf message
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_register_pathentry_pbref_uninstall:
 * @shard:   
 * @pathentry: 
 * @ud:        (nullable)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_register_pathentry_pbref_uninstall(rwdts_shard_handle_t* shard,
                                                              rw_keyspec_entry_t* pathentry,
                                                              void* ud);

/*!
 * This API can be invoked to get the data store by the application for
 * a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  out_msg Message given by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_get_element_keyspec:
 * @shard:   (transfer full)
 * @keyspec: (transfer full)
 * @out_msg: (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_get_element_keyspec(rwdts_shard_handle_t* shard,
                                               rw_keyspec_path_t* keyspec,
                                               ProtobufCMessage** out_msg);

/*!
 * This API can be invoked to get the data store by the application for
 * a given pathentry.
 *
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  out_msg Message given by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_get_element_pathentry:
 * @shard:   (transfer full)
 * @pe:      (transfer full)
 * @out_msg: (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_get_element_pathentry(rwdts_shard_handle_t* shard,
                                                 rw_keyspec_entry_t* pe,
                                                 ProtobufCMessage** out_msg);

/*!
 * This API can be invoked to get the data store by the application for
 * a given minikey.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * param   out_msg Message given by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_get_element_minikey:
 * @shard:   (transfer full)
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_get_element_minikey(rwdts_shard_handle_t* shard,
                                               rw_schema_minikey_t* minikey,
                                               ProtobufCMessage** out_msg);


/*!
 * This API can be invoked to create an element in the data store by
 * the application for a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_create_element_xpath:
 * @shard:   (transfer full)
 * @xpath: (transfer full)
 * @msg:     (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_create_element_xpath(rwdts_shard_handle_t* shard,
                                                const char* xpath,
                                                const ProtobufCMessage*  msg);

/*!
 * This API can be invoked to create an element in the data store by
 * the application for a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_create_element_keyspec:
 * @shard:   (transfer full)
 * @keyspec: (transfer full)
 * @msg:     (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_create_element_keyspec(rwdts_shard_handle_t* shard,
                                                  rw_keyspec_path_t* keyspec,
                                                  const ProtobufCMessage*  msg);

/*!
 * This API can be invoked to create an element in the data store by
 * the application for a given pathentry.
 *
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_create_element_pathentry:
 * @shard:   
 * @pe:      
 * @msg: 
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_create_element_pathentry(rwdts_shard_handle_t* shard,
                                                    rw_keyspec_entry_t* pe,
                                                    const ProtobufCMessage* msg);

/*!
 * This API can be invoked to create an element in the data store by
 * the application for a given minikey.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * param   out_msg Message given by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_create_element_minikey:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_create_element_minikey(rwdts_shard_handle_t* shard,
                                                  rw_schema_minikey_t* minikey,
                                                  const ProtobufCMessage* msg);

/*!
 * This API can be invoked to delete an element in the data store by
 * the application for a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_delete_element_keyspec:
 * @shard:   (transfer full)
 * @keyspec: (transfer full)
 * @msg:     (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_delete_element_keyspec(rwdts_shard_handle_t* shard,
                                                  rw_keyspec_path_t* keyspec,
                                                  ProtobufCMessage*  msg);

/*!
 * This API can be invoked to delete an element in the data store by
 * the application for a given pathentry.
 *
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_delete_element_pathentry:
 * @shard:   (transfer full)
 * @pe:      (transfer full)
 * @msg: (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_delete_element_pathentry(rwdts_shard_handle_t* shard,
                                                    rw_keyspec_entry_t* pe,
                                                    ProtobufCMessage* msg);

/*!
 * This API can be invoked to delete an element in the data store by
 * the application for a given minikey.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * param   out_msg Message given by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_delete_element_minikey:
 * @shard:   
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_delete_element_minikey(rwdts_shard_handle_t* shard,
                                                  rw_schema_minikey_t* minikey,
                                                  ProtobufCMessage* msg);

/*!
 * This API should be invoked by the "take" callback to release the pointer of 
 * of the application data.
 *
 *
 * @param  ctx   Shard handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_safe_put_exec:
 * @shard:   (transfer full)
 * Returns: 
 */
/// @endcond
void 
rwdts_shard_handle_appdata_safe_put_exec(rwdts_shard_handle_t* shard, uint32_t safe_id);

/* Audit related APIs */

typedef struct rwdts_audit_result_s rwdts_audit_result_t;

/*!
 *  An optional  callback invoked after the audit is completed.
 *  See the arguments for rwdts_member_reg_handle_audit_begin()
 *  rwdts_audit_done_cb_t:
 *  audit_result: (transfer full)
 */
/// @cond GI_SCANNER
/**
 * rwdts_audit_done_cb_t:
 * @apih:
 * @regh:
 * @audit_result:
 * @user_data:
 * Returns: 
 */
/// @endcond
typedef void (*rwdts_audit_done_cb_t)(rwdts_api_t*                apih,
                                      rwdts_member_reg_handle_t   regh,
                                      const rwdts_audit_result_t* audit_result,
                                      const void*                 user_data);
/*!
 *  start an audit of this registration.
 *  When this is a scubscriber registration  with caching enabled the
 *  cached data is audited against the DTS pubslihed data and any inconsistencies
 *  are reported.
 *
 *  When the registration is a subscriber registration with outboard data
 *  the data is fecthed from the publisher and replayed to the application -- ATTN not implemented.
 *
 *  When the registration is a publisher registration wih inboard data
 *  the data is fetched from the data store and compared against the member's inboard data.
 *  ATTN - need implementation.
 *
 *  When  the registration is a publisher registration with outboard data
 *  the data is fecthed from the data store and replayed back to application - ATTN need implementation.
 *
 */

/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_audit:
 * @audit_done : (scope async)(closure user_data)
 * @user_data  : (optional)(nullable)
 * Returns     : (type RwTypes.RwStatus) (transfer none)
 *
 */
/// @endcond
rw_status_t
rwdts_member_reg_handle_audit(rwdts_member_reg_handle_t        regh,
                              rwdts_audit_action_t             action,
                              rwdts_audit_done_cb_t            audit_done,
                              void*                            user_data);

/*!
 *  start an audit of an API handle.
 *  This API starts audit for all the registrations within an API handle.
 *  For every scubscriber registration  with caching enabled the
 *  cached data is audited against the DTS pubslihed data and any inconsistencies
 *  are reported.
 *
 *  When the registration is a subscriber registration with outboard data
 *  the data is fecthed from the publisher and replayed to the application -- ATTN not implemented.
 *
 *  When the registration is a publisher registration wih inboard data
 *  the data is fetched from the data store and compared against the member's inboard data.
 *  ATTN - need implementation.
 *
 *  When  the registration is a publisher registration with outboard data
 *  the data is fecthed from the data store and replayed back to application - ATTN need implementation.
 *
 */

/// @cond GI_SCANNER
/**
 * rwdts_api_audit:
 * @audit_done  : (scope async)(closure user_data)
 * @user_data   : (optional)(nullable)
 * Returns      : (type RwTypes.RwStatus) (transfer none)
 *
 */
/// @endcond
rw_status_t
rwdts_api_audit(rwdts_api_t*                     apih,
                rwdts_audit_action_t             action,
                rwdts_audit_done_cb_t            audit_done,
                void*                            user_data);

/*!
 *  Add an issue to a running audit.
 *  The application will use this API to record any issues found during the
 *  app audit callback.
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_audit_add_issue:
 * @rs     : (type RwTypes.RwStatus) (transfer none)
 * @errstr : (transfer none)
 */
/// @endcond
void
rwdts_member_reg_handle_audit_add_issue(rwdts_member_reg_handle_t    regh,
                                        rw_status_t                  rs,
                                        const char*                  errstr);
/*!
 *  Gets audit status from audit result
 */
rwdts_audit_status_t
rwdts_audit_result_get_audit_status(rwdts_audit_result_t *audit_result);

/*!
 *  Gets audit message from audit result
 */
/// @cond GI_SCANNER
/**
 * rwdts_audit_result_get_audit_msg:
 * @Returns : (transfer none)(nullable)
 */
/// @endcond
const char*
rwdts_audit_result_get_audit_msg(rwdts_audit_result_t *audit_result);

// ATTN -- Do we need an rwdts_member_reg_handle_audit_cancel ?

/*!
 *  Gets audit message from audit result
 */
char *
rwdts_api_client_path(const rwdts_api_t* apih);


/*!
 *
 * DTS invokes this callback to issue prepares of queries for a transaction
 * This callback is invoked for every query within a transaction received by the DTS member.
 *
 * @param xact_info  DTS transaction related parameters
 * @param action     The action associated with this query
 * @param keyspec    The keyspec associated with this query
 * @param msg        The data ssociated with this query as a protobuf - present for Create and Update
 * @param credits    Max number of entries the member can send
 * @param get_next_key TBD
 */
/// @cond GI_SCANNER
/*
 * rwdts_member_getnext_cb:
 * @xact_info: (transfer full)
 * @action:  
 * @keyspec: (transfer full)
 * @msg:  (transfer full)
 * @credits: 
 * @get_next_key: (transfer full)
 * 
 * @return           See rwdts_member_rsp_code_t
 */
/// @endcond

typedef rwdts_member_rsp_code_t
(*rwdts_member_getnext_cb)(const rwdts_xact_info_t* xact_info, /*! [in] DTS transaction related parameters */
                           RWDtsQueryAction         action,
                           const rw_keyspec_path_t* keyspec,   /*! [in] Keyspec associated with this query  */
                           const ProtobufCMessage*  msg,       /*! [in] The data associated with the Query as proto */
                           uint32_t                 credits,
                           void*                    get_next_key);

/*!
 * This API can be invoked to create a shard handle for a member
 * registration handle
 *
 *
 * @param  reg   Reg handle
 * @param  xact  xact 
 * @param  flags Reg flags
 * @param  flavour Shard flavour
 * @param  flavour_params Shard flavout params
 * @param  hashfunc Shard hash function
 * @param  keyfunc_params Shard hash func params
 * @param  anypolicy Shard anycast policy
 * @param  prepare Prepare callback
 * @param  shard_handle The shard handle
 * @param  dtor Destroyer function
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_new:
 * @reg: (transfer full)(nullable)
 * @flags
 * @flavour
 * @flavor_params: (nullable)
 * @hashfunc
 * @keyfunc_params: (nullable)
 * @anypolicy
 * @prepare: (scope notified)(nullable)(destroy dtor)
 * @dtor: (scope async) (nullable)
 */
/// @endcond
rwdts_shard_handle_t*
rwdts_shard_handle_new(rwdts_member_reg_handle_t reg,
                       uint32_t flags,
                       rwdts_shard_flavor flavor,
                       union rwdts_shard_flavor_params_u *flavor_params,
                       enum rwdts_shard_keyfunc_e hashfunc,
                       union rwdts_shard_keyfunc_params_u *keyfunc_params,
                       enum rwdts_shard_anycast_policy_e anypolicy,
                       rwdts_member_getnext_cb prepare,
                       GDestroyNotify dtor); 

/*!
 Print dts shard information
*/
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_print:
 * @shard_handle:
 * returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_print(rwdts_shard_handle_t* shard_handle);

/*!
 Print dts shard information
*/
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_get_shard:
 * @regh:
 * Returns:
 */
/// @endcond
rwdts_shard_handle_t *
rwdts_member_reg_handle_get_shard(rwdts_member_reg_handle_t regh);


/*!
 * This API can be invoked to update an element in the data store by
 * the application for a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_update_element_keyspec:
 * @shard:
 * @keyspec:
 * @msg:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_update_element_keyspec(rwdts_shard_handle_t* shard,
                                                  rw_keyspec_path_t* keyspec,
                                                  const ProtobufCMessage*  msg);


/*!
 * This API can be invoked to update an element in the data store by
 * the application for a given pathentry.
 *
 *
 * @param  shard   Shard handle
 * @param  pe      PathEntry key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_update_element_pathentry:
 * @shard:
 * @pe:
 * @msg:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_update_element_pathentry(rwdts_shard_handle_t* shard,
                                                    rw_keyspec_entry_t* pe,
                                                    const ProtobufCMessage*  msg);




/*!
 * This API can be invoked to update an element in the data store by
 * the application for a given minikey.
 *
 *
 * @param  shard   Shard handle
 * @param  minikey Minikey key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_update_element_minikey:
 * @shard:
 * @minikey: (type RwKeyspec.RwSchemaMinikey)
 * @msg:
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rwdts_shard_handle_appdata_update_element_minikey(rwdts_shard_handle_t* shard,
                                                  rw_schema_minikey_t* minikey,
                                                  const ProtobufCMessage*  msg);


/*!
 * This API can be invoked to get the next keyspec associated with the element
 * stored in AppData store
 *
 *
 * @param  shard   Shard handle
 * @param  cursorh Appdata cursor
 * @param  keyspec Keyspec
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_getnext_list_element_keyspec:
 * @shard_tab:
 * @cursorh:
 * @keyspec: (out) (type RwKeyspec.Path)
 * Returns: (type ProtobufC.Message) (transfer none)
 */
/// @endcond
const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_keyspec(rwdts_shard_handle_t*    shard_tab,
                                                        rwdts_appdata_cursor_t*    cursorh,
                                                        rw_keyspec_path_t**       keyspec);



/*!
 * This API can be invoked to update an element in the data store by
 * the application for a given keyspec.
 *
 *
 * @param  shard   Shard handle
 * @param  keyspec Keyspec key
 * @param  msg     Message to be by the application
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_ident_key_gi:
 * @apih:
 * @xpath:
 * @index:
 * @keylen:  (out)
 * @returns: (type guint64)
 */
/// @endcond

uint8_t *
rwdts_api_ident_key_gi( rwdts_api_t *apih, char * xpath, int index, size_t *keylen);


/*!
 * This API can be invoked to get the next keyspec associated with the element
 * stored in AppData store
 *
 *
 * @param  shard   Shard handle
 * @param  cursorh Appdata cursor
 * @param  minikey Minikey
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_getnext_list_element_minikey:
 * @shard_tab:
 * @cursorh:
 * @minikey: (out) (type RwKeyspec.RwSchemaMinikey)
 * Returns: (type ProtobufC.Message) (transfer none)
 */
/// @endcond
const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_minikey(rwdts_shard_handle_t*    shard_tab,
                                                        rwdts_appdata_cursor_t*   cursorh,
                                                        rw_schema_minikey_t** minikey);

/*!
 * This API can be invoked to get the next keyspec associated with the element
 * stored in AppData store
 *
 *
 * @param  shard   Shard handle
 * @param  cursorh Appdata cursor
 * @param  pathentry Pathentry
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_getnext_list_element_pathentry:
 * @shard_tab:
 * @cursorh:
 * @pathentry: (out) (type RwKeyspec.Entry)
 * Returns: (type ProtobufC.Message) (transfer none)
 */
/// @endcond
const ProtobufCMessage*
rwdts_shard_handle_appdata_getnext_list_element_pathentry(rwdts_shard_handle_t*    shard_tab,
                                                          rwdts_appdata_cursor_t*   cursorh,
                                                          rw_keyspec_entry_t**     pathentry);


/*!
 * This API can be invoked to get the cursor of the Appdata Data-store
 *                                                 
 *                                                 
 * @param  shard   Shard handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_get_current_cursor:
 * @shard:   
 * Returns: 
 */
/// @endcond
rwdts_appdata_cursor_t*
rwdts_shard_handle_appdata_get_current_cursor(rwdts_shard_handle_t* shard);

/*!
 * This API can be invoked to reset the cursor of the Appdata Data-store
 *                                                 
 *                                                 
 * @param  shard   Shard handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_handle_appdata_reset_cursor:
 * @shard:   
 */
/// @endcond
void
rwdts_shard_handle_appdata_reset_cursor(rwdts_shard_handle_t* shard);

/*!
 * This API can be invoked to create shard handle for a reg handle
 *
 *
 * @param  reg   Reg handle
 * @param  xact  xact 
 * @param  flags Reg flags
 * @param  flavour Shard flavour
 * @param  flavour_params Shard flavout params
 * @param  hashfunc Shard hash function
 * @param  keyfunc_params Shard hash func params
 * @param  anypolicy Shard anycast policy
 * @param  prepare Prepare callback
 * @param  shard_handle The shard handle
 * @param  dtor Destroyer function
 */
/// @cond GI_SCANNER
/**
 * rwdts_shard_key_init:
 * @reg:
 * @xact: (nullable)
 * @flags
 * @idx: 
 * @parent:
 * @flavour
 * @flavor_params: (nullable)
 * @hashfunc
 * @keyfunc_params: (nullable)
 * @anypolicy
 * @prepare: (scope notified)(nullable)
 */
/// @endcond

rwdts_shard_handle_t*
rwdts_shard_key_init(rwdts_member_reg_handle_t reg,
                     rwdts_xact_t *xact,
                     uint32_t flags,
                     int  idx,
                     rwdts_shard_t **parent,
                     rwdts_shard_flavor flavor,
                     union rwdts_shard_flavor_params_u *flavor_params,
                     enum rwdts_shard_keyfunc_e hashfunc,
                     union rwdts_shard_keyfunc_params_u *keyfunc_params,
                     enum rwdts_shard_anycast_policy_e anypolicy,
                     rwdts_member_getnext_cb prepare);

/*!
 *  Promote the registration from SUBSCRIBER to PUBLISHER
 *  This API will promote the registration from SUBSCRIBER
 *  to PUBLISHER
 *
 *  @param ud              registration handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_registration_subscriber_promotion
 * @regh:
 */
/// @endcond
void
rwdts_registration_subscriber_promotion(rwdts_member_reg_handle_t regh);

/*!
 * This API provides the rootshared from the API handle
 *                                                 
 *                                                 
 * @param  regh  Reg handle
 * @param  shard Shard handle
 */
/// @cond GI_SCANNER
/**
 * rwdts_member_reg_handle_update_shard:
 * @regh:   
 * @shard:
 * Returns: 
 */
/// @endcond
void
rwdts_member_reg_handle_update_shard(rwdts_member_reg_handle_t regh,
                                     rwdts_shard_handle_t* shard);

#ifndef __GI_SCANNER__
/*!
 * This API provides the rootshared from the API handle
 *                                                 
 *                                                 
 * @param  apih  API handle
 */
rwdts_shard_t**
rwdts_shard_get_root_parent(rwdts_api_t* apih);

#endif // __G_SCANNER__

/*!
 * This API can be used by the application to trigger a notification
 *                                                 
 *                                                 
 * @param  apih  API handle
 * @param  xpath xpath string
 * @param  msg   notification/alarm information
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_send_notification:
 * @apih:   
 * @msg:
 * Returns: (transfer none)
 */
/// @endcond
rwdts_xact_t*
rwdts_api_send_notification(rwdts_api_t* apih,
                            const ProtobufCMessage* msg);

typedef void (*rwdts_api_config_ready_cb_fn) (void *userdata) ;
/*!
 * This API can be used by the application to get config readiness
 * check callback with userdata
 *                                                 
 *                                                 
 * @param  apih  API handle
 * @param  config_ready_cb the callback function to be invoked
 * @param  userdata userdata for the call back function
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_config_ready_register:
 * @apih:   
 * @config_ready_cb: (scope call)
 * @userdata:(nullable)(optional)
 * Returns: (transfer none)
 */
/// @endcond
rwdts_api_config_ready_handle_ptr_t
rwdts_api_config_ready_register(
    rwdts_api_t *apih,
    rwdts_api_config_ready_cb_fn config_ready_cb,
    void *userdata);

/*!
 * This API can be used by the application to print current state
 * of various tasks awaiting RUNNING state
 *                                                 
 *                                                 
 * @param  rwdts_api_config_ready_handle_ptr_t handle returned while invoking 
 *                                             rwdts_api_config_ready_register
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_config_ready_current_state:
 * @rwdts_api_config_ready_handle:
 */
/// @endcond
void
rwdts_api_config_ready_current_state(rwdts_api_config_ready_handle_ptr_t rwdts_api_config_ready_handle);

/*!
 * Return the associated DTS Api Handle of tasklet Info
 *
 * @param tasklet The tasklet for which this DTS instance is being initialized
 * @return API associated with the tasklet_info
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_find_dtshandle:
 * @tasklet:       (type RwTasklet.Info)
 * Returns: (transfer none)
 *
 */
/// @endcond
rwdts_api_t*
rwdts_api_find_dtshandle(const rwtasklet_info_t*        tasklet);
/*!
 * Return the associated DTS Reg handle for keystr
 *
 * @param apih: api handle 
 * @param xpath: xpath of the reg handle to be lookedup
 * @return reg handle associated with the xpath
 */
/// @cond GI_SCANNER
/**
 * rwdts_api_find_reg_handle_for_xpath
 * @apih: (transfer none)
 * @xpath:
 * Returns: regh (transfer none)
 *
 */
/// @endcond
rwdts_member_reg_handle_t
rwdts_api_find_reg_handle_for_xpath (rwdts_api_t *apih,
                                     const char  *xpath);

#ifndef __GI_SCANNER__
typedef struct rwdts_api_vm_state_cb_s {
  void (*cb_fn) (void *ud, vcs_vm_state p_state, vcs_vm_state n_state);
  void *ud;
} rwdts_api_vm_state_cb_t;

/*!
 * send xact errstr
 *
 * @param apih  : DTS Api handle
 * @param xact  : xact handle
 * @param status  : rw status
 * @param err_str : error string
 */

void 
rwdts_api_member_send_xact_error(rwdts_api_t*                    apih,
                                 rwdts_xact_t*                   xact,
                                 rw_status_t                     status,
                                 char *                          errstr);
#endif // __GI_SCANNER__
typedef struct rwvcs_instance_s * rwvcs_instance_ptr_t;
#ifndef __GI_SCANNER__
/*!
 * Return the vcs handle
 *
 * @param apih  : DTS Api handle
 * Returns: VCS instance handle (transfer none)
 */
rwvcs_instance_ptr_t
rwdts_api_get_vcs_inst(rwdts_api_t *apih);
#endif // __GI_SCANNER__

/*!
 * Return the vcs instance name
 *
 * @param apih  : DTS Api handle
 * Returns: Instance name (transfer full)
 */
char *
rwdts_api_get_instance_name(rwdts_api_t *apih);

/*!
 * Return the uagent state  registration handle
 *
 * @param apih  : DTS Api handle
 * Returns: reg handle
 */
rwdts_member_reg_handle_t
rwdts_api_get_uagent_state_regh(rwdts_api_t *apih);
/*!
 * Set the uagent state  registration handle
 *
 * @param apih  : DTS Api handle
 * @param regh  : Reg handle to be set
 */
void
rwdts_api_set_uagent_state_regh(rwdts_api_t *apih,
                                rwdts_member_reg_handle_t regh);
__END_DECLS
#endif /* __RWDTS_API_GI_H__ */
