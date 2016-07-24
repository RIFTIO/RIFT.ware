/* Intel Ethernet Switch Host Interface Driver
 * Copyright(c) 2013 - 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 */

#include "fm10k.h"
#include <net/arp.h>

/**
 * ies_type_trans - determine packet's protocol ID and insert ies header
 * @skb: received data buffer
 * @dev: receiving network device
 *
 * This function is a wrapper for eth_type_trans.  The result of this
 * function is that the ies stored in the context block is transferred to
 * the start of the frame and the mac header is padded to account for it.
 */
__be16 ies_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	__be64 *ies;

	/* if we are not ies interface then just return eth_type_trans */
	if (!fm10k_is_ies(dev))
		return eth_type_trans(skb, dev);

	/* perform some basic accounting to cleanup and prep sk_buff */
	skb_reset_mac_header(skb);
	skb->pkt_type = PACKET_OTHERHOST;

	/* create room for IES header */
	BUG_ON((skb_mac_header(skb) - skb->head) < (sizeof(struct fm10k_ies)));
	skb->mac_header -= sizeof(struct fm10k_ies);
	ies = (__be64 *)skb_mac_header(skb);

	/* Place the timestamp and FTAG in the newly allocated space */
	ies[0] = cpu_to_be64(le64_to_cpu(FM10K_CB(skb)->tstamp));
	ies[1] = cpu_to_be64(le64_to_cpu(FM10K_CB(skb)->fi.ftag));

	/* return our own proprietary protocol */
	return htons(ETH_P_IES);
}

static int ies_rcv(struct sk_buff *skb, struct net_device __always_unused *dev,
		   struct packet_type __always_unused *pt,
		   struct net_device __always_unused *orig_dev)
{
	/* free the skb, as there is no real protocol to handle it */
	dev_kfree_skb(skb);

	return 0;
}

struct packet_type ies_packet_type __read_mostly = {
	.type = htons(ETH_P_IES),
	.func = ies_rcv,
};
