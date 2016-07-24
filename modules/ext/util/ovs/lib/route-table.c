/*
 * Copyright (c) 2011, 2012, 2013, 2014 Nicira, Inc.
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

#include "route-table.h"

#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#include "hash.h"
#include "netlink.h"
#include "netlink-notifier.h"
#include "netlink-socket.h"
#include "ofpbuf.h"
#include "ovs-router.h"
#include "rtnetlink-link.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(route_table);

struct route_data {
    /* Copied from struct rtmsg. */
    unsigned char rtm_dst_len;

    /* Extracted from Netlink attributes. */
    ovs_be32 rta_dst; /* 0 if missing. */
    ovs_be32 rta_gw;
    char ifname[IFNAMSIZ]; /* Interface name. */
};

/* A digested version of a route message sent down by the kernel to indicate
 * that a route has changed. */
struct route_table_msg {
    bool relevant;        /* Should this message be processed? */
    int nlmsg_type;       /* e.g. RTM_NEWROUTE, RTM_DELROUTE. */
    struct route_data rd; /* Data parsed from this message. */
};

static struct ovs_mutex route_table_mutex = OVS_MUTEX_INITIALIZER;
static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(5, 20);

/* Global change number for route-table, which should be incremented
 * every time route_table_reset() is called.  */
static uint64_t rt_change_seq;

static struct nln *nln = NULL;
static struct route_table_msg rtmsg;
static struct nln_notifier *route_notifier = NULL;
static struct nln_notifier *name_notifier = NULL;

static bool route_table_valid = false;

static int route_table_reset(void);
static void route_table_handle_msg(const struct route_table_msg *);
static bool route_table_parse(struct ofpbuf *, struct route_table_msg *);
static void route_table_change(const struct route_table_msg *, void *);
static void route_map_clear(void);

static void name_table_init(void);
static void name_table_change(const struct rtnetlink_link_change *, void *);

uint64_t
route_table_get_change_seq(void)
{
    return rt_change_seq;
}

/* Users of the route_table module should register themselves with this
 * function before making any other route_table function calls. */
void
route_table_init(void)
    OVS_EXCLUDED(route_table_mutex)
{
    ovs_mutex_lock(&route_table_mutex);
    ovs_assert(!nln);
    ovs_assert(!route_notifier);

    ovs_router_init();
    nln = nln_create(NETLINK_ROUTE, RTNLGRP_IPV4_ROUTE,
                     (nln_parse_func *) route_table_parse, &rtmsg);

    route_notifier =
        nln_notifier_create(nln, (nln_notify_func *) route_table_change,
                            NULL);

    route_table_reset();
    name_table_init();

    ovs_mutex_unlock(&route_table_mutex);
}

/* Run periodically to update the locally maintained routing table. */
void
route_table_run(void)
    OVS_EXCLUDED(route_table_mutex)
{
    ovs_mutex_lock(&route_table_mutex);
    if (nln) {
        rtnetlink_link_run();
        nln_run(nln);

        if (!route_table_valid) {
            route_table_reset();
        }
    }
    ovs_mutex_unlock(&route_table_mutex);
}

/* Causes poll_block() to wake up when route_table updates are required. */
void
route_table_wait(void)
    OVS_EXCLUDED(route_table_mutex)
{
    ovs_mutex_lock(&route_table_mutex);
    if (nln) {
        rtnetlink_link_wait();
        nln_wait(nln);
    }
    ovs_mutex_unlock(&route_table_mutex);
}

static int
route_table_reset(void)
{
    struct nl_dump dump;
    struct rtgenmsg *rtmsg;
    uint64_t reply_stub[NL_DUMP_BUFSIZE / 8];
    struct ofpbuf request, reply, buf;

    route_map_clear();
    route_table_valid = true;
    rt_change_seq++;

    ofpbuf_init(&request, 0);

    nl_msg_put_nlmsghdr(&request, sizeof *rtmsg, RTM_GETROUTE, NLM_F_REQUEST);

    rtmsg = ofpbuf_put_zeros(&request, sizeof *rtmsg);
    rtmsg->rtgen_family = AF_INET;

    nl_dump_start(&dump, NETLINK_ROUTE, &request);
    ofpbuf_uninit(&request);

    ofpbuf_use_stub(&buf, reply_stub, sizeof reply_stub);
    while (nl_dump_next(&dump, &reply, &buf)) {
        struct route_table_msg msg;

        if (route_table_parse(&reply, &msg)) {
            route_table_handle_msg(&msg);
        }
    }
    ofpbuf_uninit(&buf);

    return nl_dump_done(&dump);
}


static bool
route_table_parse(struct ofpbuf *buf, struct route_table_msg *change)
{
    bool parsed;

    static const struct nl_policy policy[] = {
        [RTA_DST] = { .type = NL_A_U32, .optional = true  },
        [RTA_OIF] = { .type = NL_A_U32, .optional = false },
        [RTA_GATEWAY] = { .type = NL_A_U32, .optional = true },
    };

    struct nlattr *attrs[ARRAY_SIZE(policy)];

    parsed = nl_policy_parse(buf, NLMSG_HDRLEN + sizeof(struct rtmsg),
                             policy, attrs, ARRAY_SIZE(policy));

    if (parsed) {
        const struct rtmsg *rtm;
        const struct nlmsghdr *nlmsg;
        int rta_oif;      /* Output interface index. */

        nlmsg = buf->data;
        rtm = ofpbuf_at(buf, NLMSG_HDRLEN, sizeof *rtm);

        if (rtm->rtm_family != AF_INET) {
            VLOG_DBG_RL(&rl, "received non AF_INET rtnetlink route message");
            return false;
        }

        memset(change, 0, sizeof *change);
        change->relevant = true;

        if (rtm->rtm_scope == RT_SCOPE_NOWHERE) {
            change->relevant = false;
        }

        if (rtm->rtm_type != RTN_UNICAST &&
            rtm->rtm_type != RTN_LOCAL) {
            change->relevant = false;
        }
        change->nlmsg_type     = nlmsg->nlmsg_type;
        change->rd.rtm_dst_len = rtm->rtm_dst_len;
        rta_oif = nl_attr_get_u32(attrs[RTA_OIF]);

        if (!if_indextoname(rta_oif, change->rd.ifname)) {
            int error = errno;

            VLOG_DBG_RL(&rl, "Could not find interface name[%u]: %s",
                        rta_oif, ovs_strerror(error));
            return false;
        }

        if (attrs[RTA_DST]) {
            change->rd.rta_dst = nl_attr_get_be32(attrs[RTA_DST]);
        }
        if (attrs[RTA_GATEWAY]) {
            change->rd.rta_gw = nl_attr_get_be32(attrs[RTA_GATEWAY]);
        }


    } else {
        VLOG_DBG_RL(&rl, "received unparseable rtnetlink route message");
    }

    return parsed;
}

static void
route_table_change(const struct route_table_msg *change OVS_UNUSED,
                   void *aux OVS_UNUSED)
{
    route_table_valid = false;
}

static void
route_table_handle_msg(const struct route_table_msg *change)
{
    if (change->relevant && change->nlmsg_type == RTM_NEWROUTE) {
        const struct route_data *rd = &change->rd;

        ovs_router_insert(rd->rta_dst, rd->rtm_dst_len,
                          rd->ifname, rd->rta_gw);
    }
}

static void
route_map_clear(void)
{
    ovs_router_flush();
}

bool
route_table_fallback_lookup(ovs_be32 ip_dst OVS_UNUSED,
                            char output_bridge[] OVS_UNUSED,
                            ovs_be32 *gw)
{
    *gw = 0;
    return false;
}


/* name_table . */

static void
name_table_init(void)
{
    name_notifier = rtnetlink_link_notifier_create(name_table_change, NULL);
}


static void
name_table_change(const struct rtnetlink_link_change *change OVS_UNUSED,
                  void *aux OVS_UNUSED)
{
    /* Changes to interface status can cause routing table changes that some
     * versions of the linux kernel do not advertise for some reason. */
    route_table_valid = false;
}
