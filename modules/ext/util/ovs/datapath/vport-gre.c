/*
 * Copyright (c) 2007-2012 Nicira, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include <linux/kconfig.h>
#if IS_ENABLED(CONFIG_NET_IPGRE_DEMUX)
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/in_route.h>
#include <linux/inetdevice.h>
#include <linux/jhash.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/rculist.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/route.h>
#include <net/xfrm.h>

#include <net/icmp.h>
#include <net/ip.h>
#include <net/ip_tunnels.h>
#include <net/gre.h>
#include <net/protocol.h>

#include "datapath.h"
#include "vport.h"

static struct vport_ops ovs_gre_vport_ops;
static struct vport_ops ovs_gre64_vport_ops;

/* Returns the least-significant 32 bits of a __be64. */
static __be32 be64_get_low32(__be64 x)
{
#ifdef __BIG_ENDIAN
	return (__force __be32)x;
#else
	return (__force __be32)((__force u64)x >> 32);
#endif
}

static __be16 filter_tnl_flags(__be16 flags)
{
	return flags & (TUNNEL_CSUM | TUNNEL_KEY);
}

static struct sk_buff *__build_header(struct sk_buff *skb,
				      int tunnel_hlen,
				      __be32 seq, __be16 gre64_flag)
{
	const struct ovs_key_ipv4_tunnel *tun_key;
	struct tnl_ptk_info tpi;

	tun_key = &OVS_CB(skb)->egress_tun_info->tunnel;
	skb = gre_handle_offloads(skb, !!(tun_key->tun_flags & TUNNEL_CSUM));
	if (IS_ERR(skb))
		return skb;

	tpi.flags = filter_tnl_flags(tun_key->tun_flags) | gre64_flag;

	tpi.proto = htons(ETH_P_TEB);
	tpi.key = be64_get_low32(tun_key->tun_id);
	tpi.seq = seq;
	gre_build_header(skb, &tpi, tunnel_hlen);

	return skb;
}

static __be64 key_to_tunnel_id(__be32 key, __be32 seq)
{
#ifdef __BIG_ENDIAN
	return (__force __be64)((__force u64)seq << 32 | (__force u32)key);
#else
	return (__force __be64)((__force u64)key << 32 | (__force u32)seq);
#endif
}

/* Called with rcu_read_lock and BH disabled. */
static int gre_rcv(struct sk_buff *skb,
		   const struct tnl_ptk_info *tpi)
{
	struct ovs_tunnel_info tun_info;
	struct ovs_net *ovs_net;
	struct vport *vport;
	__be64 key;

	ovs_net = net_generic(dev_net(skb->dev), ovs_net_id);
	if ((tpi->flags & TUNNEL_KEY) && (tpi->flags & TUNNEL_SEQ))
		vport = rcu_dereference(ovs_net->vport_net.gre64_vport);
	else
		vport = rcu_dereference(ovs_net->vport_net.gre_vport);
	if (unlikely(!vport))
		return PACKET_REJECT;

	key = key_to_tunnel_id(tpi->key, tpi->seq);
	ovs_flow_tun_info_init(&tun_info, ip_hdr(skb), 0, 0, key,
			       filter_tnl_flags(tpi->flags), NULL, 0);

	ovs_vport_receive(vport, skb, &tun_info);
	return PACKET_RCVD;
}

/* Called with rcu_read_lock and BH disabled. */
static int gre_err(struct sk_buff *skb, u32 info,
		   const struct tnl_ptk_info *tpi)
{
	struct ovs_net *ovs_net;
	struct vport *vport;

	ovs_net = net_generic(dev_net(skb->dev), ovs_net_id);
	if ((tpi->flags & TUNNEL_KEY) && (tpi->flags & TUNNEL_SEQ))
		vport = rcu_dereference(ovs_net->vport_net.gre64_vport);
	else
		vport = rcu_dereference(ovs_net->vport_net.gre_vport);

	if (unlikely(!vport))
		return PACKET_REJECT;
	else
		return PACKET_RCVD;
}

static int __send(struct vport *vport, struct sk_buff *skb,
		  int tunnel_hlen,
		  __be32 seq, __be16 gre64_flag)
{
	struct ovs_key_ipv4_tunnel *tun_key;
	struct rtable *rt;
	int min_headroom;
	__be16 df;
	__be32 saddr;
	int err;

	/* Route lookup */
	tun_key = &OVS_CB(skb)->egress_tun_info->tunnel;
	saddr = tun_key->ipv4_src;
	rt = find_route(ovs_dp_get_net(vport->dp),
			&saddr, tun_key->ipv4_dst,
			IPPROTO_GRE, tun_key->ipv4_tos,
			skb->mark);
	if (IS_ERR(rt)) {
		err = PTR_ERR(rt);
		goto error;
	}

	min_headroom = LL_RESERVED_SPACE(rt_dst(rt).dev) + rt_dst(rt).header_len
			+ tunnel_hlen + sizeof(struct iphdr)
			+ (skb_vlan_tag_present(skb) ? VLAN_HLEN : 0);

	if (skb_headroom(skb) < min_headroom || skb_header_cloned(skb)) {
		int head_delta = SKB_DATA_ALIGN(min_headroom -
						skb_headroom(skb) +
						16);
		err = pskb_expand_head(skb, max_t(int, head_delta, 0),
					0, GFP_ATOMIC);
		if (unlikely(err))
			goto err_free_rt;
	}

	if (skb_vlan_tag_present(skb)) {
		if (unlikely(!vlan_insert_tag_set_proto(skb,
							skb->vlan_proto,
							skb_vlan_tag_get(skb)))) {
			err = -ENOMEM;
			skb = NULL;
			goto err_free_rt;
		}
		vlan_set_tci(skb, 0);
	}

	/* Push Tunnel header. */
	skb = __build_header(skb, tunnel_hlen, seq, gre64_flag);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		skb = NULL;
		goto err_free_rt;
	}

	df = tun_key->tun_flags & TUNNEL_DONT_FRAGMENT ? htons(IP_DF) : 0;
	skb->ignore_df = 1;

	return iptunnel_xmit(skb->sk, rt, skb, saddr,
			     tun_key->ipv4_dst, IPPROTO_GRE,
			     tun_key->ipv4_tos,
			     tun_key->ipv4_ttl, df, false);
err_free_rt:
	ip_rt_put(rt);
error:
	kfree_skb(skb);
	return err;
}

static struct gre_cisco_protocol gre_protocol = {
	.handler	= gre_rcv,
	.err_handler	= gre_err,
	.priority	= 1,
};

static int gre_ports;
static int gre_init(void)
{
	int err;

	gre_ports++;
	if (gre_ports > 1)
		return 0;

	err = gre_cisco_register(&gre_protocol);
	if (err)
		pr_warn("cannot register gre protocol handler\n");

	return err;
}

static void gre_exit(void)
{
	gre_ports--;
	if (gre_ports > 0)
		return;

	gre_cisco_unregister(&gre_protocol);
}

static const char *gre_get_name(const struct vport *vport)
{
	return vport_priv(vport);
}

static struct vport *gre_create(const struct vport_parms *parms)
{
	struct net *net = ovs_dp_get_net(parms->dp);
	struct ovs_net *ovs_net;
	struct vport *vport;
	int err;

	err = gre_init();
	if (err)
		return ERR_PTR(err);

	ovs_net = net_generic(net, ovs_net_id);
	if (ovsl_dereference(ovs_net->vport_net.gre_vport)) {
		vport = ERR_PTR(-EEXIST);
		goto error;
	}

	vport = ovs_vport_alloc(IFNAMSIZ, &ovs_gre_vport_ops, parms);
	if (IS_ERR(vport))
		goto error;

	strncpy(vport_priv(vport), parms->name, IFNAMSIZ);
	rcu_assign_pointer(ovs_net->vport_net.gre_vport, vport);
	return vport;

error:
	gre_exit();
	return vport;
}

static void gre_tnl_destroy(struct vport *vport)
{
	struct net *net = ovs_dp_get_net(vport->dp);
	struct ovs_net *ovs_net;

	ovs_net = net_generic(net, ovs_net_id);

	RCU_INIT_POINTER(ovs_net->vport_net.gre_vport, NULL);
	ovs_vport_deferred_free(vport);
	gre_exit();
}

static int gre_send(struct vport *vport, struct sk_buff *skb)
{
	int hlen;

	if (unlikely(!OVS_CB(skb)->egress_tun_info)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	hlen = ip_gre_calc_hlen(OVS_CB(skb)->egress_tun_info->tunnel.tun_flags);

	return __send(vport, skb, hlen, 0, 0);
}

static int gre_get_egress_tun_info(struct vport *vport, struct sk_buff *skb,
				   struct ovs_tunnel_info *egress_tun_info)
{
	return ovs_tunnel_get_egress_info(egress_tun_info,
					  ovs_dp_get_net(vport->dp),
					  OVS_CB(skb)->egress_tun_info,
					  IPPROTO_GRE, skb->mark, 0, 0);
}

static struct vport_ops ovs_gre_vport_ops = {
	.type			= OVS_VPORT_TYPE_GRE,
	.create			= gre_create,
	.destroy		= gre_tnl_destroy,
	.get_name		= gre_get_name,
	.send			= gre_send,
	.get_egress_tun_info	= gre_get_egress_tun_info,
	.owner			= THIS_MODULE,
};

/* GRE64 vport. */
static struct vport *gre64_create(const struct vport_parms *parms)
{
	struct net *net = ovs_dp_get_net(parms->dp);
	struct ovs_net *ovs_net;
	struct vport *vport;
	int err;

	err = gre_init();
	if (err)
		return ERR_PTR(err);

	ovs_net = net_generic(net, ovs_net_id);
	if (ovsl_dereference(ovs_net->vport_net.gre64_vport)) {
		vport = ERR_PTR(-EEXIST);
		goto error;
	}

	vport = ovs_vport_alloc(IFNAMSIZ, &ovs_gre64_vport_ops, parms);
	if (IS_ERR(vport))
		goto error;

	strncpy(vport_priv(vport), parms->name, IFNAMSIZ);
	rcu_assign_pointer(ovs_net->vport_net.gre64_vport, vport);
	return vport;
error:
	gre_exit();
	return vport;
}

static void gre64_tnl_destroy(struct vport *vport)
{
	struct net *net = ovs_dp_get_net(vport->dp);
	struct ovs_net *ovs_net;

	ovs_net = net_generic(net, ovs_net_id);

	rcu_assign_pointer(ovs_net->vport_net.gre64_vport, NULL);
	ovs_vport_deferred_free(vport);
	gre_exit();
}

static __be32 be64_get_high32(__be64 x)
{
#ifdef __BIG_ENDIAN
	return (__force __be32)((__force u64)x >> 32);
#else
	return (__force __be32)x;
#endif
}

static int gre64_send(struct vport *vport, struct sk_buff *skb)
{
	int hlen = GRE_HEADER_SECTION +		/* GRE Hdr */
		   GRE_HEADER_SECTION +		/* GRE Key */
		   GRE_HEADER_SECTION;		/* GRE SEQ */
	__be32 seq;

	if (unlikely(!OVS_CB(skb)->egress_tun_info)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	if (OVS_CB(skb)->egress_tun_info->tunnel.tun_flags & TUNNEL_CSUM)
		hlen += GRE_HEADER_SECTION;

	seq = be64_get_high32(OVS_CB(skb)->egress_tun_info->tunnel.tun_id);
	return __send(vport, skb, hlen, seq, (TUNNEL_KEY|TUNNEL_SEQ));
}

static struct vport_ops ovs_gre64_vport_ops = {
	.type			= OVS_VPORT_TYPE_GRE64,
	.create			= gre64_create,
	.destroy		= gre64_tnl_destroy,
	.get_name		= gre_get_name,
	.send			= gre64_send,
	.get_egress_tun_info	= gre_get_egress_tun_info,
	.owner			= THIS_MODULE,
};

static int __init ovs_gre_tnl_init(void)
{
	int err;

	err = ovs_vport_ops_register(&ovs_gre_vport_ops);
	if (err < 0)
		return err;

	err = ovs_vport_ops_register(&ovs_gre64_vport_ops);
	if (err < 0)
		ovs_vport_ops_unregister(&ovs_gre_vport_ops);

	return err;
}

static void __exit ovs_gre_tnl_exit(void)
{
	ovs_vport_ops_unregister(&ovs_gre64_vport_ops);
	ovs_vport_ops_unregister(&ovs_gre_vport_ops);
}

module_init(ovs_gre_tnl_init);
module_exit(ovs_gre_tnl_exit);

MODULE_DESCRIPTION("OVS: GRE switching port");
MODULE_LICENSE("GPL");
MODULE_ALIAS("vport-type-3");
MODULE_ALIAS("vport-type-104");
#endif
