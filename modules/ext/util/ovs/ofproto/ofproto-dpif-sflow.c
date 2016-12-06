/*
 * Copyright (c) 2009, 2010, 2011, 2012, 2013, 2014, 2015 Nicira, Inc.
 * Copyright (c) 2009 InMon Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>
#include "ofproto-dpif-sflow.h"
#include <inttypes.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdlib.h>
#include "collectors.h"
#include "compiler.h"
#include "dpif.h"
#include "hash.h"
#include "hmap.h"
#include "netdev.h"
#include "netlink.h"
#include "ofpbuf.h"
#include "ofproto.h"
#include "packets.h"
#include "poll-loop.h"
#include "ovs-router.h"
#include "route-table.h"
#include "sflow_api.h"
#include "socket-util.h"
#include "timeval.h"
#include "openvswitch/vlog.h"
#include "lib/odp-util.h"
#include "ofproto-provider.h"
#include "lacp.h"

VLOG_DEFINE_THIS_MODULE(sflow);

static struct ovs_mutex mutex;

/* This global var is used to determine which sFlow
   sub-agent should send the datapath counters. */
#define SFLOW_GC_SUBID_UNCLAIMED (uint32_t)-1
static uint32_t sflow_global_counters_subid = SFLOW_GC_SUBID_UNCLAIMED;

struct dpif_sflow_port {
    struct hmap_node hmap_node; /* In struct dpif_sflow's "ports" hmap. */
    SFLDataSource_instance dsi; /* sFlow library's notion of port number. */
    struct ofport *ofport;      /* To retrive port stats. */
    odp_port_t odp_port;
};

struct dpif_sflow {
    struct collectors *collectors;
    SFLAgent *sflow_agent;
    struct ofproto_sflow_options *options;
    time_t next_tick;
    size_t n_flood, n_all;
    struct hmap ports;          /* Contains "struct dpif_sflow_port"s. */
    uint32_t probability;
    struct ovs_refcount ref_cnt;
};

static void dpif_sflow_del_port__(struct dpif_sflow *,
                                  struct dpif_sflow_port *);

#define RECEIVER_INDEX 1

static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);

static bool
nullable_string_is_equal(const char *a, const char *b)
{
    return a ? b && !strcmp(a, b) : !b;
}

static bool
ofproto_sflow_options_equal(const struct ofproto_sflow_options *a,
                         const struct ofproto_sflow_options *b)
{
    return (sset_equals(&a->targets, &b->targets)
            && a->sampling_rate == b->sampling_rate
            && a->polling_interval == b->polling_interval
            && a->header_len == b->header_len
            && a->sub_id == b->sub_id
            && nullable_string_is_equal(a->agent_device, b->agent_device)
            && nullable_string_is_equal(a->control_ip, b->control_ip));
}

static struct ofproto_sflow_options *
ofproto_sflow_options_clone(const struct ofproto_sflow_options *old)
{
    struct ofproto_sflow_options *new = xmemdup(old, sizeof *old);
    sset_clone(&new->targets, &old->targets);
    new->agent_device = old->agent_device ? xstrdup(old->agent_device) : NULL;
    new->control_ip = old->control_ip ? xstrdup(old->control_ip) : NULL;
    return new;
}

static void
ofproto_sflow_options_destroy(struct ofproto_sflow_options *options)
{
    if (options) {
        sset_destroy(&options->targets);
        free(options->agent_device);
        free(options->control_ip);
        free(options);
    }
}

/* sFlow library callback to allocate memory. */
static void *
sflow_agent_alloc_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                     size_t bytes)
{
    return calloc(1, bytes);
}

/* sFlow library callback to free memory. */
static int
sflow_agent_free_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                    void *obj)
{
    free(obj);
    return 0;
}

/* sFlow library callback to report error. */
static void
sflow_agent_error_cb(void *magic OVS_UNUSED, SFLAgent *agent OVS_UNUSED,
                     char *msg)
{
    VLOG_WARN("sFlow agent error: %s", msg);
}

/* sFlow library callback to send datagram. */
static void
sflow_agent_send_packet_cb(void *ds_, SFLAgent *agent OVS_UNUSED,
                           SFLReceiver *receiver OVS_UNUSED, u_char *pkt,
                           uint32_t pktLen)
{
    struct dpif_sflow *ds = ds_;
    collectors_send(ds->collectors, pkt, pktLen);
}

static struct dpif_sflow_port *
dpif_sflow_find_port(const struct dpif_sflow *ds, odp_port_t odp_port)
    OVS_REQUIRES(mutex)
{
    struct dpif_sflow_port *dsp;

    HMAP_FOR_EACH_IN_BUCKET (dsp, hmap_node, hash_odp_port(odp_port),
                             &ds->ports) {
        if (dsp->odp_port == odp_port) {
            return dsp;
        }
    }
    return NULL;
}

/* Call to get the datapath stats. Modeled after the dpctl utility.
 *
 * It might be more efficient for this module to be given a handle it can use
 * to get these stats more efficiently, but this is only going to be called
 * once every 20-30 seconds.  Return number of datapaths found (normally expect
 * 1). */
static int
sflow_get_dp_stats(struct dpif_sflow *ds OVS_UNUSED,
                   struct dpif_dp_stats *dp_totals)
{
    struct sset types;
    const char *type;
    int count = 0;

    memset(dp_totals, 0, sizeof *dp_totals);
    sset_init(&types);
    dp_enumerate_types(&types);
    SSET_FOR_EACH (type, &types) {
        struct sset names;
        const char *name;
        sset_init(&names);
        if (dp_enumerate_names(type, &names) == 0) {
            SSET_FOR_EACH (name, &names) {
                struct dpif *dpif;
                if (dpif_open(name, type, &dpif) == 0) {
                    struct dpif_dp_stats dp_stats;
                    if (dpif_get_dp_stats(dpif, &dp_stats) == 0) {
                        count++;
                        dp_totals->n_hit += dp_stats.n_hit;
                        dp_totals->n_missed += dp_stats.n_missed;
                        dp_totals->n_lost += dp_stats.n_lost;
                        dp_totals->n_flows += dp_stats.n_flows;
                        dp_totals->n_mask_hit += dp_stats.n_mask_hit;
                        dp_totals->n_masks += dp_stats.n_masks;
                    }
                    dpif_close(dpif);
                }
            }
            sset_destroy(&names);
        }
    }
    sset_destroy(&types);
    return count;
}

/* If there are multiple bridges defined then we need some
   minimal artibration to decide which one should send the
   global counters.  This function allows each sub-agent to
   ask if he should do it or not. */
static bool
sflow_global_counters_subid_test(uint32_t subid)
    OVS_REQUIRES(mutex)
{
    if (sflow_global_counters_subid == SFLOW_GC_SUBID_UNCLAIMED) {
        /* The role is up for grabs. */
        sflow_global_counters_subid = subid;
    }
    return (sflow_global_counters_subid == subid);
}

static void
sflow_global_counters_subid_clear(uint32_t subid)
    OVS_REQUIRES(mutex)
{
    if (sflow_global_counters_subid == subid) {
        /* The sub-agent that was sending global counters
           is going away, so reset to allow another
           to take over. */
        sflow_global_counters_subid = SFLOW_GC_SUBID_UNCLAIMED;
    }
}

static void
sflow_agent_get_global_counters(void *ds_, SFLPoller *poller,
                                SFL_COUNTERS_SAMPLE_TYPE *cs)
    OVS_REQUIRES(mutex)
{
    struct dpif_sflow *ds = ds_;
    SFLCounters_sample_element dp_elem, res_elem;
    struct dpif_dp_stats dp_totals;
    struct rusage usage;

    if (!sflow_global_counters_subid_test(poller->agent->subId)) {
        /* Another sub-agent is currently responsible for this. */
        return;
    }

    /* datapath stats */
    if (sflow_get_dp_stats(ds, &dp_totals)) {
        dp_elem.tag = SFLCOUNTERS_OVSDP;
        dp_elem.counterBlock.ovsdp.n_hit = dp_totals.n_hit;
        dp_elem.counterBlock.ovsdp.n_missed = dp_totals.n_missed;
        dp_elem.counterBlock.ovsdp.n_lost = dp_totals.n_lost;
        dp_elem.counterBlock.ovsdp.n_mask_hit = dp_totals.n_mask_hit;
        dp_elem.counterBlock.ovsdp.n_flows = dp_totals.n_flows;
        dp_elem.counterBlock.ovsdp.n_masks = dp_totals.n_masks;
        SFLADD_ELEMENT(cs, &dp_elem);
    }

    /* resource usage */
    getrusage(RUSAGE_SELF, &usage);
    res_elem.tag = SFLCOUNTERS_APP_RESOURCES;
    res_elem.counterBlock.appResources.user_time
        = timeval_to_msec(&usage.ru_utime);
    res_elem.counterBlock.appResources.system_time
        = timeval_to_msec(&usage.ru_stime);
    res_elem.counterBlock.appResources.mem_used = (usage.ru_maxrss * 1024);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.mem_max);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.fd_open);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.fd_max);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.conn_open);
    SFL_UNDEF_GAUGE(res_elem.counterBlock.appResources.conn_max);

    SFLADD_ELEMENT(cs, &res_elem);
    sfl_poller_writeCountersSample(poller, cs);
}

static void
sflow_agent_get_counters(void *ds_, SFLPoller *poller,
                         SFL_COUNTERS_SAMPLE_TYPE *cs)
    OVS_REQUIRES(mutex)
{
    struct dpif_sflow *ds = ds_;
    SFLCounters_sample_element elem, lacp_elem, of_elem, name_elem;
    enum netdev_features current;
    struct dpif_sflow_port *dsp;
    SFLIf_counters *counters;
    struct netdev_stats stats;
    enum netdev_flags flags;
    struct lacp_slave_stats lacp_stats;
    const char *ifName;

    dsp = dpif_sflow_find_port(ds, u32_to_odp(poller->bridgePort));
    if (!dsp) {
        return;
    }

    elem.tag = SFLCOUNTERS_GENERIC;
    counters = &elem.counterBlock.generic;
    counters->ifIndex = SFL_DS_INDEX(poller->dsi);
    counters->ifType = 6;
    if (!netdev_get_features(dsp->ofport->netdev, &current, NULL, NULL, NULL)) {
        /* The values of ifDirection come from MAU MIB (RFC 2668): 0 = unknown,
           1 = full-duplex, 2 = half-duplex, 3 = in, 4=out */
        counters->ifSpeed = netdev_features_to_bps(current, 0);
        counters->ifDirection = (netdev_features_is_full_duplex(current)
                                 ? 1 : 2);
    } else {
        counters->ifSpeed = 100000000;
        counters->ifDirection = 0;
    }
    if (!netdev_get_flags(dsp->ofport->netdev, &flags) && flags & NETDEV_UP) {
        counters->ifStatus = 1; /* ifAdminStatus up. */
        if (netdev_get_carrier(dsp->ofport->netdev)) {
            counters->ifStatus |= 2; /* ifOperStatus us. */
        }
    } else {
        counters->ifStatus = 0;  /* Down. */
    }

    /* XXX
       1. Is the multicast counter filled in?
       2. Does the multicast counter include broadcasts?
       3. Does the rx_packets counter include multicasts/broadcasts?
    */
    ofproto_port_get_stats(dsp->ofport, &stats);
    counters->ifInOctets = stats.rx_bytes;
    counters->ifInUcastPkts = stats.rx_packets;
    counters->ifInMulticastPkts = stats.multicast;
    counters->ifInBroadcastPkts = -1;
    counters->ifInDiscards = stats.rx_dropped;
    counters->ifInErrors = stats.rx_errors;
    counters->ifInUnknownProtos = -1;
    counters->ifOutOctets = stats.tx_bytes;
    counters->ifOutUcastPkts = stats.tx_packets;
    counters->ifOutMulticastPkts = -1;
    counters->ifOutBroadcastPkts = -1;
    counters->ifOutDiscards = stats.tx_dropped;
    counters->ifOutErrors = stats.tx_errors;
    counters->ifPromiscuousMode = 0;

    SFLADD_ELEMENT(cs, &elem);

    /* Include LACP counters and identifiers if this port is part of a LAG. */
    if (ofproto_port_get_lacp_stats(dsp->ofport, &lacp_stats) == 0) {
	memset(&lacp_elem, 0, sizeof lacp_elem);
	lacp_elem.tag = SFLCOUNTERS_LACP;
	memcpy(&lacp_elem.counterBlock.lacp.actorSystemID,
	       lacp_stats.dot3adAggPortActorSystemID,
	       ETH_ADDR_LEN);
	memcpy(&lacp_elem.counterBlock.lacp.partnerSystemID,
	       lacp_stats.dot3adAggPortPartnerOperSystemID,
	       ETH_ADDR_LEN);
	lacp_elem.counterBlock.lacp.attachedAggID =
	    lacp_stats.dot3adAggPortAttachedAggID;
	lacp_elem.counterBlock.lacp.portState.v.actorAdmin =
	    lacp_stats.dot3adAggPortActorAdminState;
	lacp_elem.counterBlock.lacp.portState.v.actorOper =
	    lacp_stats.dot3adAggPortActorOperState;
	lacp_elem.counterBlock.lacp.portState.v.partnerAdmin =
	    lacp_stats.dot3adAggPortPartnerAdminState;
	lacp_elem.counterBlock.lacp.portState.v.partnerOper =
	    lacp_stats.dot3adAggPortPartnerOperState;
	lacp_elem.counterBlock.lacp.LACPDUsRx =
	    lacp_stats.dot3adAggPortStatsLACPDUsRx;
	SFL_UNDEF_COUNTER(lacp_elem.counterBlock.lacp.markerPDUsRx);
	SFL_UNDEF_COUNTER(lacp_elem.counterBlock.lacp.markerResponsePDUsRx);
	SFL_UNDEF_COUNTER(lacp_elem.counterBlock.lacp.unknownRx);
	lacp_elem.counterBlock.lacp.illegalRx =
	    lacp_stats.dot3adAggPortStatsIllegalRx;
	lacp_elem.counterBlock.lacp.LACPDUsTx =
	    lacp_stats.dot3adAggPortStatsLACPDUsTx;
	SFL_UNDEF_COUNTER(lacp_elem.counterBlock.lacp.markerPDUsTx);
	SFL_UNDEF_COUNTER(lacp_elem.counterBlock.lacp.markerResponsePDUsTx);
	SFLADD_ELEMENT(cs, &lacp_elem);
    }

    /* Include Port name. */
    if ((ifName = netdev_get_name(dsp->ofport->netdev)) != NULL) {
	memset(&name_elem, 0, sizeof name_elem);
	name_elem.tag = SFLCOUNTERS_PORTNAME;
	name_elem.counterBlock.portName.portName.str = (char *)ifName;
	name_elem.counterBlock.portName.portName.len = strlen(ifName);
	SFLADD_ELEMENT(cs, &name_elem);
    }

    /* Include OpenFlow DPID and openflow port number. */
    memset(&of_elem, 0, sizeof of_elem);
    of_elem.tag = SFLCOUNTERS_OPENFLOWPORT;
    of_elem.counterBlock.ofPort.datapath_id =
	ofproto_get_datapath_id(dsp->ofport->ofproto);
    of_elem.counterBlock.ofPort.port_no =
      (OVS_FORCE uint32_t)dsp->ofport->ofp_port;
    SFLADD_ELEMENT(cs, &of_elem);

    sfl_poller_writeCountersSample(poller, cs);
}

/* Obtains an address to use for the local sFlow agent and stores it into
 * '*agent_addr'.  Returns true if successful, false on failure.
 *
 * The sFlow agent address should be a local IP address that is persistent and
 * reachable over the network, if possible.  The IP address associated with
 * 'agent_device' is used if it has one, and otherwise 'control_ip', the IP
 * address used to talk to the controller.  If the agent device is not
 * specified then it is figured out by taking a look at the routing table based
 * on 'targets'. */
static bool
sflow_choose_agent_address(const char *agent_device,
                           const struct sset *targets,
                           const char *control_ip,
                           SFLAddress *agent_addr)
{
    const char *target;
    struct in_addr in4;

    memset(agent_addr, 0, sizeof *agent_addr);
    agent_addr->type = SFLADDRESSTYPE_IP_V4;

    if (agent_device) {
        if (!netdev_get_in4_by_name(agent_device, &in4)) {
            goto success;
        }
    }

    SSET_FOR_EACH (target, targets) {
        union {
            struct sockaddr_storage ss;
            struct sockaddr_in sin;
        } sa;
        char name[IFNAMSIZ];

        if (inet_parse_active(target, SFL_DEFAULT_COLLECTOR_PORT, &sa.ss)
            && sa.ss.ss_family == AF_INET) {
            ovs_be32 gw;

            if (ovs_router_lookup(sa.sin.sin_addr.s_addr, name, &gw)
                && !netdev_get_in4_by_name(name, &in4)) {
                goto success;
            }
        }
    }

    if (control_ip && !lookup_ip(control_ip, &in4)) {
        goto success;
    }

    VLOG_ERR("could not determine IP address for sFlow agent");
    return false;

success:
    agent_addr->address.ip_v4.addr = (OVS_FORCE uint32_t) in4.s_addr;
    return true;
}

static void
dpif_sflow_clear__(struct dpif_sflow *ds) OVS_REQUIRES(mutex)
{
    if (ds->sflow_agent) {
        sflow_global_counters_subid_clear(ds->sflow_agent->subId);
        sfl_agent_release(ds->sflow_agent);
        free(ds->sflow_agent);
        ds->sflow_agent = NULL;
    }
    collectors_destroy(ds->collectors);
    ds->collectors = NULL;
    ofproto_sflow_options_destroy(ds->options);
    ds->options = NULL;

    /* Turn off sampling to save CPU cycles. */
    ds->probability = 0;
}

void
dpif_sflow_clear(struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    dpif_sflow_clear__(ds);
    ovs_mutex_unlock(&mutex);
}

bool
dpif_sflow_is_enabled(const struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    bool enabled;

    ovs_mutex_lock(&mutex);
    enabled = ds->collectors != NULL;
    ovs_mutex_unlock(&mutex);
    return enabled;
}

struct dpif_sflow *
dpif_sflow_create(void)
{
    static struct ovsthread_once once = OVSTHREAD_ONCE_INITIALIZER;
    struct dpif_sflow *ds;

    if (ovsthread_once_start(&once)) {
        ovs_mutex_init_recursive(&mutex);
        ovsthread_once_done(&once);
    }

    ds = xcalloc(1, sizeof *ds);
    ds->next_tick = time_now() + 1;
    hmap_init(&ds->ports);
    ds->probability = 0;
    ovs_refcount_init(&ds->ref_cnt);

    return ds;
}

struct dpif_sflow *
dpif_sflow_ref(const struct dpif_sflow *ds_)
{
    struct dpif_sflow *ds = CONST_CAST(struct dpif_sflow *, ds_);
    if (ds) {
        ovs_refcount_ref(&ds->ref_cnt);
    }
    return ds;
}

/* 32-bit fraction of packets to sample with.  A value of 0 samples no packets,
 * a value of %UINT32_MAX samples all packets and intermediate values sample
 * intermediate fractions of packets. */
uint32_t
dpif_sflow_get_probability(const struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    uint32_t probability;
    ovs_mutex_lock(&mutex);
    probability = ds->probability;
    ovs_mutex_unlock(&mutex);
    return probability;
}

void
dpif_sflow_unref(struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    if (ds && ovs_refcount_unref_relaxed(&ds->ref_cnt) == 1) {
        struct dpif_sflow_port *dsp, *next;

        dpif_sflow_clear(ds);
        HMAP_FOR_EACH_SAFE (dsp, next, hmap_node, &ds->ports) {
            dpif_sflow_del_port__(ds, dsp);
        }
        hmap_destroy(&ds->ports);
        free(ds);
    }
}

static void
dpif_sflow_add_poller(struct dpif_sflow *ds, struct dpif_sflow_port *dsp)
    OVS_REQUIRES(mutex)
{
    SFLPoller *poller = sfl_agent_addPoller(ds->sflow_agent, &dsp->dsi, ds,
                                            sflow_agent_get_counters);
    sfl_poller_set_sFlowCpInterval(poller, ds->options->polling_interval);
    sfl_poller_set_sFlowCpReceiver(poller, RECEIVER_INDEX);
    sfl_poller_set_bridgePort(poller, odp_to_u32(dsp->odp_port));
}

void
dpif_sflow_add_port(struct dpif_sflow *ds, struct ofport *ofport,
                    odp_port_t odp_port) OVS_EXCLUDED(mutex)
{
    struct dpif_sflow_port *dsp;
    int ifindex;

    ovs_mutex_lock(&mutex);
    dpif_sflow_del_port(ds, odp_port);

    ifindex = netdev_get_ifindex(ofport->netdev);

    if (ifindex <= 0) {
        /* Not an ifindex port, so do not add a cross-reference to it here */
        goto out;
    }

    /* Add to table of ports. */
    dsp = xmalloc(sizeof *dsp);
    dsp->ofport = ofport;
    dsp->odp_port = odp_port;
    SFL_DS_SET(dsp->dsi, SFL_DSCLASS_IFINDEX, ifindex, 0);
    hmap_insert(&ds->ports, &dsp->hmap_node, hash_odp_port(odp_port));

    /* Add poller. */
    if (ds->sflow_agent) {
        dpif_sflow_add_poller(ds, dsp);
    }

out:
    ovs_mutex_unlock(&mutex);
}

static void
dpif_sflow_del_port__(struct dpif_sflow *ds, struct dpif_sflow_port *dsp)
    OVS_REQUIRES(mutex)
{
    if (ds->sflow_agent) {
        sfl_agent_removePoller(ds->sflow_agent, &dsp->dsi);
        sfl_agent_removeSampler(ds->sflow_agent, &dsp->dsi);
    }
    hmap_remove(&ds->ports, &dsp->hmap_node);
    free(dsp);
}

void
dpif_sflow_del_port(struct dpif_sflow *ds, odp_port_t odp_port)
    OVS_EXCLUDED(mutex)
{
    struct dpif_sflow_port *dsp;

    ovs_mutex_lock(&mutex);
    dsp = dpif_sflow_find_port(ds, odp_port);
    if (dsp) {
        dpif_sflow_del_port__(ds, dsp);
    }
    ovs_mutex_unlock(&mutex);
}

void
dpif_sflow_set_options(struct dpif_sflow *ds,
                       const struct ofproto_sflow_options *options)
    OVS_EXCLUDED(mutex)
{
    struct dpif_sflow_port *dsp;
    bool options_changed;
    SFLReceiver *receiver;
    SFLAddress agentIP;
    time_t now;
    SFLDataSource_instance dsi;
    uint32_t dsIndex;
    SFLSampler *sampler;
    SFLPoller *poller;

    ovs_mutex_lock(&mutex);
    if (sset_is_empty(&options->targets) || !options->sampling_rate) {
        /* No point in doing any work if there are no targets or nothing to
         * sample. */
        dpif_sflow_clear__(ds);
        goto out;
    }

    options_changed = (!ds->options
                       || !ofproto_sflow_options_equal(options, ds->options));

    /* Configure collectors if options have changed or if we're shortchanged in
     * collectors (which indicates that opening one or more of the configured
     * collectors failed, so that we should retry). */
    if (options_changed
        || collectors_count(ds->collectors) < sset_count(&options->targets)) {
        collectors_destroy(ds->collectors);
        collectors_create(&options->targets, SFL_DEFAULT_COLLECTOR_PORT,
                          &ds->collectors);
        if (ds->collectors == NULL) {
            VLOG_WARN_RL(&rl, "no collectors could be initialized, "
                         "sFlow disabled");
            dpif_sflow_clear__(ds);
            goto out;
        }
    }

    /* Choose agent IP address and agent device (if not yet setup) */
    if (!sflow_choose_agent_address(options->agent_device,
                                    &options->targets,
                                    options->control_ip, &agentIP)) {
        dpif_sflow_clear__(ds);
        goto out;
    }

    /* Avoid reconfiguring if options didn't change. */
    if (!options_changed) {
        goto out;
    }
    ofproto_sflow_options_destroy(ds->options);
    ds->options = ofproto_sflow_options_clone(options);

    /* Create agent. */
    VLOG_INFO("creating sFlow agent %d", options->sub_id);
    if (ds->sflow_agent) {
        sflow_global_counters_subid_clear(ds->sflow_agent->subId);
        sfl_agent_release(ds->sflow_agent);
    }
    ds->sflow_agent = xcalloc(1, sizeof *ds->sflow_agent);
    now = time_wall();
    sfl_agent_init(ds->sflow_agent,
                   &agentIP,
                   options->sub_id,
                   now,         /* Boot time. */
                   now,         /* Current time. */
                   ds,          /* Pointer supplied to callbacks. */
                   sflow_agent_alloc_cb,
                   sflow_agent_free_cb,
                   sflow_agent_error_cb,
                   sflow_agent_send_packet_cb);

    receiver = sfl_agent_addReceiver(ds->sflow_agent);
    sfl_receiver_set_sFlowRcvrOwner(receiver, "Open vSwitch sFlow");
    sfl_receiver_set_sFlowRcvrTimeout(receiver, 0xffffffff);

    /* Set the sampling_rate down in the datapath. */
    ds->probability = MAX(1, UINT32_MAX / ds->options->sampling_rate);

    /* Add a single sampler for the bridge. This appears as a PHYSICAL_ENTITY
       because it is associated with the hypervisor, and interacts with the server
       hardware directly.  The sub_id is used to distinguish this sampler from
       others on other bridges within the same agent. */
    dsIndex = 1000 + options->sub_id;
    SFL_DS_SET(dsi, SFL_DSCLASS_PHYSICAL_ENTITY, dsIndex, 0);
    sampler = sfl_agent_addSampler(ds->sflow_agent, &dsi);
    sfl_sampler_set_sFlowFsPacketSamplingRate(sampler, ds->options->sampling_rate);
    sfl_sampler_set_sFlowFsMaximumHeaderSize(sampler, ds->options->header_len);
    sfl_sampler_set_sFlowFsReceiver(sampler, RECEIVER_INDEX);

    /* Add a counter poller for the bridge so we can use it to send
       global counters such as datapath cache hit/miss stats. */
    poller = sfl_agent_addPoller(ds->sflow_agent, &dsi, ds,
                                 sflow_agent_get_global_counters);
    sfl_poller_set_sFlowCpInterval(poller, ds->options->polling_interval);
    sfl_poller_set_sFlowCpReceiver(poller, RECEIVER_INDEX);

    /* Add pollers for the currently known ifindex-ports */
    HMAP_FOR_EACH (dsp, hmap_node, &ds->ports) {
        dpif_sflow_add_poller(ds, dsp);
    }


out:
    ovs_mutex_unlock(&mutex);
}

int
dpif_sflow_odp_port_to_ifindex(const struct dpif_sflow *ds,
                               odp_port_t odp_port) OVS_EXCLUDED(mutex)
{
    struct dpif_sflow_port *dsp;
    int ret;

    ovs_mutex_lock(&mutex);
    dsp = dpif_sflow_find_port(ds, odp_port);
    ret = dsp ? SFL_DS_INDEX(dsp->dsi) : 0;
    ovs_mutex_unlock(&mutex);
    return ret;
}

void
dpif_sflow_received(struct dpif_sflow *ds, const struct dp_packet *packet,
                    const struct flow *flow, odp_port_t odp_in_port,
                    const union user_action_cookie *cookie)
    OVS_EXCLUDED(mutex)
{
    SFL_FLOW_SAMPLE_TYPE fs;
    SFLFlow_sample_element hdrElem;
    SFLSampled_header *header;
    SFLFlow_sample_element switchElem;
    SFLSampler *sampler;
    struct dpif_sflow_port *in_dsp;
    ovs_be16 vlan_tci;

    ovs_mutex_lock(&mutex);
    sampler = ds->sflow_agent->samplers;
    if (!sampler) {
        goto out;
    }

    /* Build a flow sample. */
    memset(&fs, 0, sizeof fs);

    /* Look up the input ifIndex if this port has one. Otherwise just
     * leave it as 0 (meaning 'unknown') and continue. */
    in_dsp = dpif_sflow_find_port(ds, odp_in_port);
    if (in_dsp) {
        fs.input = SFL_DS_INDEX(in_dsp->dsi);
    }

    /* Make the assumption that the random number generator in the datapath converges
     * to the configured mean, and just increment the samplePool by the configured
     * sampling rate every time. */
    sampler->samplePool += sfl_sampler_get_sFlowFsPacketSamplingRate(sampler);

    /* Sampled header. */
    memset(&hdrElem, 0, sizeof hdrElem);
    hdrElem.tag = SFLFLOW_HEADER;
    header = &hdrElem.flowType.header;
    header->header_protocol = SFLHEADER_ETHERNET_ISO8023;
    /* The frame_length should include the Ethernet FCS (4 bytes),
     * but it has already been stripped,  so we need to add 4 here. */
    header->frame_length = dp_packet_size(packet) + 4;
    /* Ethernet FCS stripped off. */
    header->stripped = 4;
    header->header_length = MIN(dp_packet_size(packet),
                                sampler->sFlowFsMaximumHeaderSize);
    header->header_bytes = dp_packet_data(packet);

    /* Add extended switch element. */
    memset(&switchElem, 0, sizeof(switchElem));
    switchElem.tag = SFLFLOW_EX_SWITCH;
    switchElem.flowType.sw.src_vlan = vlan_tci_to_vid(flow->vlan_tci);
    switchElem.flowType.sw.src_priority = vlan_tci_to_pcp(flow->vlan_tci);

    /* Retrieve data from user_action_cookie. */
    vlan_tci = cookie->sflow.vlan_tci;
    switchElem.flowType.sw.dst_vlan = vlan_tci_to_vid(vlan_tci);
    switchElem.flowType.sw.dst_priority = vlan_tci_to_pcp(vlan_tci);

    fs.output = cookie->sflow.output;

    /* Submit the flow sample to be encoded into the next datagram. */
    SFLADD_ELEMENT(&fs, &hdrElem);
    SFLADD_ELEMENT(&fs, &switchElem);
    sfl_sampler_writeFlowSample(sampler, &fs);

out:
    ovs_mutex_unlock(&mutex);
}

void
dpif_sflow_run(struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    if (ds->collectors != NULL) {
        time_t now = time_now();
        route_table_run();
        if (now >= ds->next_tick) {
            sfl_agent_tick(ds->sflow_agent, time_wall());
            ds->next_tick = now + 1;
        }
    }
    ovs_mutex_unlock(&mutex);
}

void
dpif_sflow_wait(struct dpif_sflow *ds) OVS_EXCLUDED(mutex)
{
    ovs_mutex_lock(&mutex);
    if (ds->collectors != NULL) {
        poll_timer_wait_until(ds->next_tick * 1000LL);
    }
    ovs_mutex_unlock(&mutex);
}
