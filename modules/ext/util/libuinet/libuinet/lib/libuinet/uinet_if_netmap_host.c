/*
 * Copyright (c) 2013 Patrick Kelsey. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#if defined(__linux__)
/*
 * To expose required facilities in net/if.h.
 */
#define _GNU_SOURCE
#endif /* __linux__ */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#if defined(__FreeBSD__)
#include <sys/sockio.h>
#endif /*  __FreeBSD__ */
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#if defined(__linux__)
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/version.h>
#endif /* __linux__ */
#include <netinet/in.h>

#if defined(__FreeBSD__)
#include <net/if.h>
#endif /*  __FreeBSD__ */


#if defined(__linux__)
#include <net/if.h>
#ifndef IFNAMSIZ
#define IFNAMSIZ IF_NAMESIZE
#endif
#endif /* __linux__ */

#include <net/netmap.h>
#include <net/netmap_user.h>

#include "uinet_if_netmap_host.h"
#include "uinet_host_interface.h"


/* XXX The netmap host interface should be converted to NETMAP_API >= 10
 * semantics and internally translate to NETMAP_API < 10.  Currently it is
 * done the other way.
 */
#if NETMAP_API >= 10
#define NETMAP_RING_NEXT(r, i) nm_ring_next((r), (i))
#endif

struct if_netmap_host_context {
	int fd;
	int cfgfd;
	int isvale;
	const char *ifname;
	struct nmreq req;
	void *mem;
	struct netmap_ring *hw_rx_ring;
	struct netmap_ring *hw_tx_ring;
};


struct if_netmap_host_context *
if_netmap_register_if(int nmfd, const char *ifname, unsigned int isvale, unsigned int qno)
{
	struct if_netmap_host_context *ctx;

	ctx = calloc(1, sizeof(struct if_netmap_host_context));
	if (NULL == ctx)
		return (NULL);
	
	ctx->fd = nmfd;

	ctx->cfgfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (-1 == ctx->cfgfd)
		goto fail;

	ctx->isvale = isvale;
	ctx->ifname = ifname;

	/*
	 * Disable TCP and checksum offload, which can impact throughput
	 * and also cause packets to be dropped or modified gratuitously.
	 *
	 * Also disable VLAN offload/filtering - we want to talk straight to
	 * the wire.
	 *
	 */

	if (!ctx->isvale) {
		if (0 != if_netmap_set_offload(ctx, 0)) {
			goto fail;
		}

		if (0 != if_netmap_set_promisc(ctx, 1)) {
			goto fail;
		}
	}

	ctx->req.nr_version = NETMAP_API;
#if NETMAP_API < 10
	ctx->req.nr_ringid = NETMAP_NO_TX_POLL | NETMAP_HW_RING | qno;
#else
	ctx->req.nr_ringid = NETMAP_NO_TX_POLL | qno;
	ctx->req.nr_flags = NR_REG_ONE_NIC;
#endif
	snprintf(ctx->req.nr_name, sizeof(ctx->req.nr_name), "%s", ifname);

	if (-1 == ioctl(ctx->fd, NIOCREGIF, &ctx->req)) {
		goto fail;
	} 

	ctx->mem = uhi_mmap(NULL, ctx->req.nr_memsize, UHI_PROT_READ | UHI_PROT_WRITE, UHI_MAP_NOCORE | UHI_MAP_SHARED, ctx->fd, 0);
	if (MAP_FAILED == ctx->mem) {
		goto fail;
	}

	ctx->hw_rx_ring = NETMAP_RXRING(NETMAP_IF(ctx->mem, ctx->req.nr_offset), qno);
	ctx->hw_tx_ring = NETMAP_TXRING(NETMAP_IF(ctx->mem, ctx->req.nr_offset), qno);

#if NETMAP_API < 10
	/* NIOCREGIF will reset the hardware rings, but the reserved count
	 * might still be non-zero from a previous user's activities
	 */
	ctx->hw_rx_ring->reserved = 0;
#endif

	return (ctx);

fail:
	free(ctx);
	return(NULL);
}


void
if_netmap_deregister_if(struct if_netmap_host_context *ctx)
{
	if (!ctx->isvale)
		if_netmap_set_promisc(ctx, 0);

	munmap(ctx->mem, ctx->req.nr_memsize);
	close (ctx->cfgfd);
	free(ctx);
}


void
if_netmap_rxupdate(struct if_netmap_host_context *ctx, const uint32_t *avail, const uint32_t *cur, const uint32_t *reserved)
{
	struct netmap_ring *rxr = ctx->hw_rx_ring;

	if (cur) rxr->cur = *cur;

#if NETMAP_API < 10
	if (avail) rxr->avail = *avail;
	if (reserved) rxr->reserved = *reserved;
#else
	if (reserved) {
		rxr->head = rxr->cur - *reserved;
		if ((int)rxr->head < 0)
			rxr->head += rxr->num_slots;
	}
#endif
}


uint32_t
if_netmap_rxavail(struct if_netmap_host_context *ctx)
{
#if NETMAP_API < 10
	return (ctx->hw_rx_ring->avail);
#else
	return (nm_ring_space(ctx->hw_rx_ring));
#endif
}


uint32_t
if_netmap_rxcur(struct if_netmap_host_context *ctx)
{
	return (ctx->hw_rx_ring->cur);
}


uint32_t
if_netmap_rxreserved(struct if_netmap_host_context *ctx)
{
#if NETMAP_API < 10
	return (ctx->hw_rx_ring->reserved);
#else
	int ret = ctx->hw_rx_ring->cur - ctx->hw_rx_ring->head;
	if (ret < 0)
		ret += ctx->hw_rx_ring->num_slots;
	return (ret);
#endif
}


uint32_t
if_netmap_rxslots(struct if_netmap_host_context *ctx)
{
	return (ctx->req.nr_rx_slots);
}


uint32_t
if_netmap_rxbufsize(struct if_netmap_host_context *ctx)
{
	return (ctx->hw_rx_ring->nr_buf_size);
}


void *
if_netmap_rxslot(struct if_netmap_host_context *ctx, uint32_t *slotno, uint32_t *len, uint32_t *index)
{
	struct netmap_ring *rxr = ctx->hw_rx_ring;
	uint32_t cur = *slotno;

	*slotno = NETMAP_RING_NEXT(rxr, cur); 
	*len = rxr->slot[cur].len;
	*index = rxr->slot[cur].buf_idx;
	return (NETMAP_BUF(rxr, rxr->slot[cur].buf_idx));
}


void
if_netmap_rxsetslot(struct if_netmap_host_context *ctx, uint32_t *slotno, uint32_t index)
{
	struct netmap_ring *rxr = ctx->hw_rx_ring;
	uint32_t cur = *slotno;

	rxr->slot[cur].buf_idx = index;
	rxr->slot[cur].flags |= NS_BUF_CHANGED;
	*slotno = NETMAP_RING_NEXT(rxr, cur);
}


int
if_netmap_txsync(struct if_netmap_host_context *ctx, const uint32_t *avail, const uint32_t *cur)
{
	struct netmap_ring *txr = ctx->hw_tx_ring;

#if NETMAP_API < 10
	if (cur) txr->cur = *cur;
	if (avail) txr->avail = *avail;
#else
	if (cur)
		txr->head = txr->cur = *cur;
#endif

	return (ioctl(ctx->fd, NIOCTXSYNC, NULL) == -1 ? errno : 0);
}


uint32_t
if_netmap_txavail(struct if_netmap_host_context *ctx)
{
#if NETMAP_API < 10
	return (ctx->hw_tx_ring->avail);
#else
	return (nm_ring_space(ctx->hw_tx_ring));
#endif
}


uint32_t
if_netmap_txcur(struct if_netmap_host_context *ctx)
{
	return (ctx->hw_tx_ring->cur);
}


uint32_t
if_netmap_txslots(struct if_netmap_host_context *ctx)
{
	return (ctx->req.nr_tx_slots);
}


void *
if_netmap_txslot(struct if_netmap_host_context *ctx, uint32_t *slotno, uint32_t len)
{
	struct netmap_ring *txr = ctx->hw_tx_ring;
	uint32_t cur = *slotno;
		
	assert(len <= txr->nr_buf_size);

	txr->slot[cur].len = len;
	*slotno = NETMAP_RING_NEXT(txr, cur); 
	return (NETMAP_BUF(txr, txr->slot[cur].buf_idx));
}


#if defined(__linux__)
static int
if_netmap_ethtool_set_flag(struct if_netmap_host_context *ctx, struct ifreq *ifr, uint32_t flag, int on)
{
	struct ethtool_value etv;

	ifr->ifr_data = (void *)&etv;

	etv.cmd = ETHTOOL_GFLAGS;
	if (-1 == ioctl(ctx->cfgfd, SIOCETHTOOL, ifr)) {
		printf("ethtool get flags failed (%d)\n", errno);
		return (-1);
	}

	if (etv.data ^ flag) {
		
		if (on) 
			etv.data |= flag;
		else
			etv.data &= ~flag;

		etv.cmd = ETHTOOL_SFLAGS;
		if (-1 == ioctl(ctx->cfgfd, SIOCETHTOOL, ifr)) {
			if (EOPNOTSUPP != errno) {
				printf("ethtool set flag 0x%08x failed (%d)\n", flag, errno);
				return (-1);
			}
		}
	}

	return (0);
}


static int
if_netmap_ethtool_set_discrete(struct if_netmap_host_context *ctx, struct ifreq *ifr, int getcmd, int setcmd, int on)
{
	struct ethtool_value etv;

	ifr->ifr_data = (void *)&etv;

	etv.cmd = getcmd;
	if (-1 == ioctl(ctx->cfgfd, SIOCETHTOOL, ifr)) {
		printf("ethtool discrete get 0x%08x failed (%d)\n", getcmd, errno);
		return (-1);
	}

	if ((!etv.cmd && on) || (etv.cmd && !on)) {
		etv.data = on;

		etv.cmd = setcmd;
		if (-1 == ioctl(ctx->cfgfd, SIOCETHTOOL, ifr)) {
			if (EOPNOTSUPP != errno) {
				printf("ethtool discrete set 0x%08x failed %d\n", setcmd, errno);
				return (-1);
			}
		}
	}

	return (0);
}
#endif /* __linux__ */


int
if_netmap_set_offload(struct if_netmap_host_context *ctx, int on)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof ifr);
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ctx->ifname);

#if defined(__FreeBSD__)
	
	if (-1 == ioctl(ctx->cfgfd, SIOCGIFCAP, &ifr)) {
		perror("get interface capabilities failed");
		return (-1);
	}

	ifr.ifr_reqcap = ifr.ifr_curcap;

	if (on)
		ifr.ifr_reqcap |= IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE | IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWTSO;
	else
		ifr.ifr_reqcap &= ~(IFCAP_HWCSUM | IFCAP_TSO | IFCAP_TOE | IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWTSO);

	if (-1 == ioctl(ctx->cfgfd, SIOCSIFCAP, &ifr)) {
		perror("set interface capabilities failed");
		return (-1);
	}
#elif defined(__linux__)

	/* XXX 
	 * Apparently there's no way to disable VLAN offload before 2.6.37?
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
	if_netmap_ethtool_set_flag(ctx, &ifr, ETH_FLAG_RXVLAN, on);
	if_netmap_ethtool_set_flag(ctx, &ifr, ETH_FLAG_TXVLAN, on);
#endif
	if_netmap_ethtool_set_flag(ctx, &ifr, ETH_FLAG_NTUPLE, on);
	if_netmap_ethtool_set_flag(ctx, &ifr, ETH_FLAG_RXHASH, on);

	if_netmap_ethtool_set_discrete(ctx, &ifr, ETHTOOL_GRXCSUM, ETHTOOL_SRXCSUM, on);
	if_netmap_ethtool_set_discrete(ctx, &ifr, ETHTOOL_GTXCSUM, ETHTOOL_STXCSUM, on);
	if_netmap_ethtool_set_discrete(ctx, &ifr, ETHTOOL_GTSO, ETHTOOL_STSO, on);
	if_netmap_ethtool_set_discrete(ctx, &ifr, ETHTOOL_GUFO, ETHTOOL_SUFO, on);

#else
#error  Add support for modifying interface offload functions on this platform.
#endif /* __FreeBSD__ */

	return (0);
}


int
if_netmap_set_promisc(struct if_netmap_host_context *ctx, int on)
{
	struct ifreq ifr;
	int rv;

	memset(&ifr, 0, sizeof ifr);
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ctx->ifname);
	rv = ioctl(ctx->cfgfd, SIOCGIFFLAGS, &ifr);
	if (rv == -1) {
		perror("get interface flags failed");
		return (-1);
	}

#if defined(__FreeBSD__)
	uint32_t flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);

	if (on)
		flags |= IFF_PPROMISC;
	else
		flags &= ~IFF_PPROMISC;

	ifr.ifr_flags = flags & 0xffff;
	ifr.ifr_flagshigh = (flags >> 16) & 0xffff;
#elif defined(__linux__)
	ifr.ifr_flags |= IFF_PROMISC;
#else
#error  Add support for putting an interface into promiscuous mode on this platform.
#endif /* __FreeBSD__ */

	rv = ioctl(ctx->cfgfd, SIOCSIFFLAGS, &ifr);
	if (rv == -1) {
		perror("set interface flags failed");
		return (-1);
	}

	return (0);
}

