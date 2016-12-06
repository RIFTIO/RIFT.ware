
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
 * @file rwdts.h
 * @brief Top level include for RW.DTS API.
 *
 *
 * @author Grant Taylor (grant.taylor@riftio.com)
 * @date 2014/01/26
 */


/*! \mainpage DTS Overview and Concepts

\anchor overview

This is the API reference and quickstart documentation for RW.DTS.

# Overview

The RiftWare Distributed Transaction System provides a transactional
publish/subscribe middleware facility to participating tasklets.
Transaction data and keying is based around the RiftWare yang-based
schema, and is interoperable with XML technologies.

## API Overview

The DTS API has two main parts.

The Query API allows callers to execute transaction queries.  DTS
queries involve a key specifier, usually compiled from an XPath
expression; a verb, from a Create/Read/Update/Delete ("CRUD") themed
set; an optional data payload; and an optional reduction expression or
script.

The Member API allows tasklets to participate in transaction
execution.  Participating members are either (or both) publishers, who
conceptually own and implement the data for which they have
registered; or subscribers, who conceptually implement side-effects as
published data changes.  Members may also issue queries with the Query
API.

## Central Concepts

### Keying

DTS evaluates XPath expressions down to a compiled representation
called a keyspec.  Generally query and registration APIs take XPath
inputs, whereas runtime member actions and query results are in
keyspec form for efficiency.

In order to support reusable components, members usually operate on
the trailing portion of the overall keyspec path which they implement;
this trailing portion is defined as the part below the key registered
with the Member API.  In some cases member code will need to look at
parent key information in the keyspec to fully qualify which object is
being referenced in a query.  However in most cases only keys beneath
the registration point need substantive member evaluation.

Members typically register at a point in the schema containing
multiple objects; the simplest variation on this theme is a
registration path at a keyed point and which includes a single
wildcard at the end.  This is effectively a table.  More detailed
registrations may include sharding details.


#### Sharding

Keys containing wildcards permit the distributed implementation of
member behavior.  Member code cooperates to agree about which specific
member implements which objects within a registered wildcarded key
path, and members exclude themselves from participation in queries
involving another member's data (using the "Not Applicable" NA
response code).

In practice this scheme of wildcarded member self-exclusion does not
scale.  Therefore, for any point in a registered keypath which
contains a wildcarded key, the registered member may attach sharding
information.  Sharding information describes which key values at that
point are implemented by which specific member.  DTS uses the sharding
information to route queries efficiently across a large distributed
system.

Sharding information may be updated dynamically and transactionally as
the system runs.  This supports a variety of application-level
elasticity behaviors.  Sharded keypaths may also specify a pool of
default members to receive CREATE queries.  This allows
application-specific distribution and resource management logic to
execute at object creation time.

See also \sharding.

### Data

DTS data is represented as a tree of Protobuf objects, defined by a
schema expressed in yang.  The entire tree, or any branch thereof, is
directly convertible to XML; query results are uniformly available in
either format.

Individual members implement branches of the data tree by registering
with the DTS Member API.

Beneath the registration point members must understand the data they
implement as well as the keying and other schema attributes of the
implemented data.  Members are nominally responsible for implementing
the DTS-defined suite of "CRUD" operations as well as various
modifers.  In practice many of the requisite member behaviors can be
left to the DTS Member API and associated convenience framework APIs.

Members may implement data by actually implementing data structures
internally, interacting with DTS in a manner similar to "virtual
tables" under a traditional database.  In this case the data is
considered "outboard" data relative to the DTS API.

Members may also implement data by using the DTS Member Data API, in
which case the data lives within the DTS Member API and is considered
"inboard" data.  In this case DTS has more direct visibility into the
data, and accordingly DTS can implement READ operations and other
behaviors.  Member participation is still required for any necessary
side-effect or application behavior.

The "inboard" member data API is supported for both published and
subscribed data.  In the latter case the inboard data functions as an
automatically maintained cache of the subscription data.  In the
former case the inboard data functions as an in-memory database.


### Operations

DTS provides Create, Read, Update, and Delete operations.  Secondary
behaviors are included via a set of flags functioning as adverbs in
the query.  For example, a KEYONLY flag provides for the return of
impacted object keys but no actual object data in the payload.  An
ANYCAST flag provides for query delivery to exactly one destination of
a set.  Collectively the base verbs and adverbs smooth the
implementation of common use cases and recovery operations.

Beyond raw data operations, a data reduction mechanism is defined.
This executes script code (XQuery or Python) as part query execution,
so as to provide a summary or otherwise reduced or transformed result
to the client.  TBD (!)

DTS also supports RPC operations as defined in the yang schema.  RPCs
involve a registrant or registrants plus an input and an output data
object.  Most elements of keying, sharding, payload data, and member
participation are the same as for other operations, however the
aggregate semantics of an RPC are application-defined.  While they are
not disallowed within a transaction context, RPCs are inherently not
transactional.

For the full set of operations, see the Query API in \ref api_gi.


### Publishers

Publishers conceptually implement the objects being queried.

A code-free publishing member, ie one which has registered for data
but has not provided any callbacks, has essentially the behavior of a
transactional database; the member will store and manipulate data as
CRUD queries arrive.  As data changes this fact will be implicitly
published to subscribers as part of the transaction.

More interesting publishing members also implement
application-specific behavior to go with the data.  For example, the
management agent publishes configuration, so in addition to storing
the configuration and informing the rest of the system of
configuraiton changes via that publication, the management agent
implements a Netconf interface which permits non-DTS management
applications to manipulate the system configuration.

Publishers may request the DTS API to provide persistence for
published data.  DTS implements persistence via a distributed
Key-Value database (currently Redis).  The DTS KV datastore
automatically participates in transactions, notably including
multi-phase commit semantics.

Publishers may also implement persistence by themselves; or may
manually implement persistence using the DTS KV datastore API
directly.  This later scenario is suitable for members which need to
persist only a subset of the involved data.

For the gory details on publishers, see the Member API in \ref api_di_member.


### Subscribers

Subscribers conceptually perform side-effects when objects elsewhere
in the system are modified.

A code-free subscriber does nothing.

More interesting subscribers will approve or veto published queries as
part of transactions, and will execute necessary side-effects at
transaction commit time.  For example, code within the fpath tasklet
will subscribe to interface configuration data, and will actually
create, delete, or modify interface(s) as the configuration changes
are committed.

Subscribers may execute additional sub-queries, subscriber
notifications, and other distributed actions all within the umbrella
of the DTS transaction.  Thus all behaviors associated with a
transactional query will, or will not, take place when the transaction
completes.

Subscribers may request the DTS API to operate a cache of subscribed
data locally; often this provides a more complete view of subscribed
data objects than is visible in a subscription notification's
delta-themed payload.

For the gory details on subscribers, see the Member API in \ref
api_di_member.


### Transactions

DTS transactions are multi-phase distributed transactions.
Transactions pass through several states, beginning with actual query
execution (PREPARE), and ending up globally applied (COMMIT), globally
not applied (ABORT), or in an inconsistent/error state (FAILURE).

Transactions are composed of a series of queries, grouped into blocks.
The query blocks are normally executed in sequence; within each block
the queries are executed in parallel.  Alternatively query blocks may
be issued for immediate execution within the context of an ongoing
transaction.  This permits the results of one query to inform the
results of another.

Blocks may be added to a running transaction at any time by any
participant (member or query client) during the PREPARE phase; among
other things this forms the basis of subscriber notification, and
permits query satisfaction to be implemented by further subqueries
originated in any participating member.

The PREPARE phase ends when a) the transaction originator has made a
commit call AND b) all queries have been completed.

For each transaction, application members participate in three main
transaction events.

First is a PREPARE state, during which members examine the individual
data queries, make tentative state changes including executing further
data queries, and approve or reject the transaction.  Second is a
PRECOMMIT state, during which all members agree that the tentative
state from the end of the PREPARE phase is valid.  Finally is the
COMMIT state, during which all members execute the agreed state
changes.

Before COMMIT time, any participating member may signal an ABORT, at
which point the transaction is halted and all members throw away any
tentative state.  This is not strictly an error condition, although it
does represent a transaction that did not take effect and therefore
some condition the query originator should handle.

An abort or error at COMMIT time leaves the participating members in
an inconsistent state and the transaction returns a FAILURE condition.
Recovery from such an inconsistent state is beyond the scope of DTS,
as it may require systemwide audit, reconciliation, and/or recovery
actions.



## Use Cases

### Configuration

Configuration is typically implemented with the Appconf framework
API.

Conceptually the management agent task is the publisher of all
configuration.  It provides Netconf and other interfaces to
configuration data, as well as persistent storage.

All configurable tasklets in the system register as subscribing
members to the configuration elements which they need.  Each
configuration change is then published to subscribing members, who
leverage the DTS-provided PUBLISH/PRECOMMIT/COMMIT transaction
sequence to tentatively agree, reject, and/or execute the
configuration changes.

The Appconf framework provides a simplified configuration-centric
interface to this transaction flow.  The application is given the
opportunity to approve or reject changes, and then told to apply the
changes or revert to the original configuration as transaction flow
dictates.

See the \ref appconf documentation for more detail.

### Operational Data

Operational data includes the sum total of inspectable data about
system and application state.  Statistics is a common and slightly
special case within the general operational data category.

Much operational data is not directly modifiable via DTS interfaces.
That is, the CREATE/UPDATE/DELETE operations will only occur as
implicit behavior triggered by some operational, configuration, or
management event.  Such data can use the Appdata framework, which
provides the member with simplified READ handling.

Conceptually appdata participants register for the data item(s) they
implement.  If the member stores data within the DTS API, DTS will
completely handle READ requests.  If the member stores data within the
application, or dynamically generates the data on demand, DTS will
provide a simplified "fill in this object" callback to the application.

In certain cases DTS can directly reference application-owned data to
eliminate callback code.

As an implementation convenience for C applications some "collapsed"
Protobuf structures are available for statistics data; these have only
the numbers with no Pb-specific overhead, and are thus suitable for
implementation use within application code.  Python applications can
implement Gi-provided Protobuf structures directly, however reference
counting issues currently prevent seamless access to Python-owned
structures from the DTS API.

See the \ref appdata documentation for more detail.

### Publish/Subscribe

Intra-application communication is generally structured as a series of
pub/sub interactions between application components.  Commonly these
interactions are used to propogate tables between members and to
implement distributed object behavior in response to configuration or
application events.

DTS provides general-purpose publish/subscribe features.  These are
available with or without transactional semantics.  Fundamentally,
subscribers receive tuples of keys and delta-structured payloads
describing changes to the Protobuf data tree, originating from the
publishers implementing the involved data.

See the ??apptable?? documentation for more detail.


## More Information

For an introduction to the Core API, see \ref api_gi.

*/

#ifndef __RWDTS_H
#define __RWDTS_H

#include <rw_dts_int.pb-c.h>
#include <rwdts_api_gi.h>
#include <rwdts_keyspec.h>
#include <rwdts_query_api.h>
#include <rwdts_member_api.h>
#include <rwdts_appstats_api.h>
#include <rwdts_appconf_api.h>
#include <rwdts_kv_light_api_gi.h>

/// @cond BEGIN_DECLS
__BEGIN_DECLS
/// @endcond

/*
 * Functions to be used to simulate dts functionalilty in whitebox testing where
 * DTS router functionality is required
 */

/*!
 * init router for testing
 */
void *rwdts_test_router_init(rwmsg_endpoint_t *ep,
                             rwsched_dispatch_queue_t rwq,
                             void *taskletinfo,
                             int instance_id,
                             const char *rwmsgpath);


/*!
 * deinit  a router instance created through rwdts_test_router_init
 */
void rwdts_test_router_deinit(void *dts);


/// @cond END_DECLS
__END_DECLS
/// @endcond


#endif /* __RWDTS_H */
