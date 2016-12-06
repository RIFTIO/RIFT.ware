/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_MBUF_H_
#define _RTE_MBUF_H_

/**
 * @file
 * RTE Mbuf
 *
 * The mbuf library provides the ability to create and destroy buffers
 * that may be used by the RTE application to store message
 * buffers. The message buffers are stored in a mempool, using the
 * RTE mempool library.
 *
 * This library provide an API to allocate/free packet mbufs, which are
 * used to carry network packets.
 *
 * To understand the concepts of packet buffers or mbufs, you
 * should read "TCP/IP Illustrated, Volume 2: The Implementation,
 * Addison-Wesley, 1995, ISBN 0-201-63354-X from Richard Stevens"
 * http://www.kohala.com/start/tcpipiv2.html
 */

#include <stdint.h>
#include <rte_mempool.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>

#ifdef __cplusplus
extern "C" {
#endif

/* deprecated feature, renamed in RTE_MBUF_REFCNT */
#pragma GCC poison RTE_MBUF_SCATTER_GATHER

/*
 * Packet Offload Features Flags. It also carry packet type information.
 * Critical resources. Both rx/tx shared these bits. Be cautious on any change
 *
 * - RX flags start at bit position zero, and get added to the left of previous
 *   flags.
 * - The most-significant 8 bits are reserved for generic mbuf flags
 * - TX flags therefore start at bit position 55 (i.e. 63-8), and new flags get
 *   added to the right of the previously defined flags
 */
#define PKT_RX_VLAN_PKT      (1ULL << 0)  /**< RX packet is a 802.1q VLAN packet. */
#define PKT_RX_RSS_HASH      (1ULL << 1)  /**< RX packet with RSS hash result. */
#define PKT_RX_FDIR          (1ULL << 2)  /**< RX packet with FDIR infos. */
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)  /**< L4 cksum of RX pkt. is not OK. */
#define PKT_RX_IP_CKSUM_BAD  (1ULL << 4)  /**< IP cksum of RX pkt. is not OK. */
#define PKT_RX_EIP_CKSUM_BAD (0ULL << 0)  /**< External IP header checksum error. */
#define PKT_RX_OVERSIZE      (0ULL << 0)  /**< Num of desc of an RX pkt oversize. */
#define PKT_RX_HBUF_OVERFLOW (0ULL << 0)  /**< Header buffer overflow. */
#define PKT_RX_RECIP_ERR     (0ULL << 0)  /**< Hardware processing error. */
#define PKT_RX_MAC_ERR       (0ULL << 0)  /**< MAC error. */
#define PKT_RX_IPV4_HDR      (1ULL << 5)  /**< RX packet with IPv4 header. */
#define PKT_RX_IPV4_HDR_EXT  (1ULL << 6)  /**< RX packet with extended IPv4 header. */
#define PKT_RX_IPV6_HDR      (1ULL << 7)  /**< RX packet with IPv6 header. */
#define PKT_RX_IPV6_HDR_EXT  (1ULL << 8)  /**< RX packet with extended IPv6 header. */
#define PKT_RX_IEEE1588_PTP  (1ULL << 9)  /**< RX IEEE1588 L2 Ethernet PT Packet. */
#define PKT_RX_IEEE1588_TMST (1ULL << 10) /**< RX IEEE1588 L2/L4 timestamped packet.*/

#define PKT_TX_VLAN_PKT      (1ULL << 55) /**< TX packet is a 802.1q VLAN packet. */
#define PKT_TX_IP_CKSUM      (1ULL << 54) /**< IP cksum of TX pkt. computed by NIC. */
#define PKT_TX_IPV4_CSUM     PKT_TX_IP_CKSUM /**< Alias of PKT_TX_IP_CKSUM. */
#define PKT_TX_IPV4          PKT_RX_IPV4_HDR /**< IPv4 with no IP checksum offload. */
#define PKT_TX_IPV6          PKT_RX_IPV6_HDR /**< IPv6 packet */

/*
 * Bits 52+53 used for L4 packet type with checksum enabled.
 *     00: Reserved
 *     01: TCP checksum
 *     10: SCTP checksum
 *     11: UDP checksum
 */
#define PKT_TX_L4_NO_CKSUM   (0ULL << 52) /**< Disable L4 cksum of TX pkt. */
#define PKT_TX_TCP_CKSUM     (1ULL << 52) /**< TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    (2ULL << 52) /**< SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     (3ULL << 52) /**< UDP cksum of TX pkt. computed by NIC. */
#define PKT_TX_L4_MASK       (3ULL << 52) /**< Mask for L4 cksum offload request. */

/* Bit 51 - IEEE1588*/
#define PKT_TX_IEEE1588_TMST (1ULL << 51) /**< TX IEEE1588 packet to timestamp. */

/* Use final bit of flags to indicate a control mbuf */
#define CTRL_MBUF_FLAG       (1ULL << 63) /**< Mbuf contains control data */

/**
 * Bit Mask to indicate what bits required for building TX context
 */
#define PKT_TX_OFFLOAD_MASK (PKT_TX_VLAN_PKT | PKT_TX_IP_CKSUM | PKT_TX_L4_MASK)

/* define a set of marker types that can be used to refer to set points in the
 * mbuf */
typedef void    *MARKER[0];   /**< generic marker for a point in a structure */
typedef uint64_t MARKER64[0]; /**< marker that allows us to overwrite 8 bytes
                               * with a single assignment */
#ifdef RTE_LIBRW_PIOT
  struct rw_mbuf_metadata{
    uint8_t kni_filter_flag:4;             /**< Used by Kni to indicate to the kernel whether to drop the packets or treat it as outgoing. */
#define RW_FPATH_PKT_KNI_SEND_RX           0x01
#define RW_FPATH_PKT_KNI_DISCARD_ON_RX     0x02
#define RW_FPATH_PKT_KNI_TREAT_AS_TX       0x04
#define RW_FPATH_PKT_KNI_ALREADY_FILTERED  0x08
    uint8_t trafgen_coherency:4; //coherency used by trafgen
    uint8_t  l3_offset;
    uint8_t  vf_ttl;
    uint8_t  vf_pathid;
    uint8_t  vf_encap;
    uint8_t  pad;
    uint16_t payload;

    uint32_t  parsed_len;
    uint32_t mdata_flag;
    uint32_t vf_lportid;
    uint32_t vf_portmeta;
    uint32_t vf_ncid;
    uint32_t vf_nh_policy_fwd[4];
    uint32_t vf_nh_policy_lportid;
    uint32_t vf_flow_cache_target;
    
    //uint32_t vf_signid;
    //uint32_t vf_vsapid;
    //uint32_t vf_vsap_handle;
    //uint32_t vf_flowid_msw;
    //uint32_t vf_flowid_lsw;    
  };
#endif
/**
 * The generic rte_mbuf, containing a packet mbuf.
 */
struct rte_mbuf {
	MARKER cacheline0;

	void *buf_addr;           /**< Virtual address of segment buffer. */
	phys_addr_t buf_physaddr; /**< Physical address of segment buffer. */

	/* next 8 bytes are initialised on RX descriptor rearm */
	MARKER64 rearm_data;
	uint16_t buf_len;         /**< Length of segment buffer. */
	uint16_t data_off;

	/**
	 * 16-bit Reference counter.
	 * It should only be accessed using the following functions:
	 * rte_mbuf_refcnt_update(), rte_mbuf_refcnt_read(), and
	 * rte_mbuf_refcnt_set(). The functionality of these functions (atomic,
	 * or non-atomic) is controlled by the CONFIG_RTE_MBUF_REFCNT_ATOMIC
	 * config option.
	 */
	union {
#ifdef RTE_MBUF_REFCNT
		rte_atomic16_t refcnt_atomic; /**< Atomically accessed refcnt */
		uint16_t refcnt;              /**< Non-atomically accessed refcnt */
#endif
		uint16_t refcnt_reserved;     /**< Do not use this field */
	};
	uint8_t nb_segs;          /**< Number of segments. */
	uint8_t port;             /**< Input port. */

	uint64_t ol_flags;        /**< Offload features. */

	/* remaining bytes are set on RX when pulling packet from descriptor */
	MARKER rx_descriptor_fields1;
	uint16_t reserved2;       /**< Unused field. Required for padding */
	uint16_t data_len;        /**< Amount of data in segment buffer. */
	uint32_t pkt_len;         /**< Total pkt len: sum of all segments. */
	uint16_t vlan_tci;        /**< VLAN Tag Control Identifier (CPU order) */
	uint16_t reserved;
	union {
		uint32_t rss;     /**< RSS hash result if RSS enabled */
		struct {
			uint16_t hash;
			uint16_t id;
		} fdir;           /**< Filter identifier if FDIR enabled */
		uint32_t sched;   /**< Hierarchical scheduler */
	} hash;                   /**< hash information */

	/* second cache line - fields only used in slow path or on TX */
	MARKER cacheline1 __rte_cache_aligned;
  
	union {
          void *userdata;   /**< Can be used for external metadata */
          uint64_t udata64; /**< Allow 8-byte userdata on 32-bit */
#ifdef RTE_LIBRW_PIOT
          struct rw_mbuf_metadata rw_data;
#endif
	};

	struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
	struct rte_mbuf *next;    /**< Next segment of scattered packet. */

	/* fields to support TX offloads */
	union {
		uint16_t l2_l3_len; /**< combined l2/l3 lengths as single var */
		struct {
			uint16_t l3_len:9;      /**< L3 (IP) Header Length. */
			uint16_t l2_len:7;      /**< L2 (MAC) Header Length. */
		};
	};
} __rte_cache_aligned;

/**
 * Given the buf_addr returns the pointer to corresponding mbuf.
 */
#define RTE_MBUF_FROM_BADDR(ba)     (((struct rte_mbuf *)(ba)) - 1)

/**
 * Given the pointer to mbuf returns an address where it's  buf_addr
 * should point to.
 */
#define RTE_MBUF_TO_BADDR(mb)       (((struct rte_mbuf *)(mb)) + 1)

/**
 * Returns TRUE if given mbuf is indirect, or FALSE otherwise.
 */
#define RTE_MBUF_INDIRECT(mb)   (RTE_MBUF_FROM_BADDR((mb)->buf_addr) != (mb))

/**
 * Returns TRUE if given mbuf is direct, or FALSE otherwise.
 */
#define RTE_MBUF_DIRECT(mb)     (RTE_MBUF_FROM_BADDR((mb)->buf_addr) == (mb))


/**
 * Private data in case of pktmbuf pool.
 *
 * A structure that contains some pktmbuf_pool-specific data that are
 * appended after the mempool structure (in private data).
 */
struct rte_pktmbuf_pool_private {
	uint16_t mbuf_data_room_size; /**< Size of data space in each mbuf.*/
};

#ifdef RTE_LIBRTE_MBUF_DEBUG

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) rte_mbuf_sanity_check(m, is_h)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, is_h)	do {       \
	if ((m) != NULL)                                   \
		rte_mbuf_sanity_check(m, is_h);          \
} while (0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                                         \
if (!(exp)) {                                                        \
	rte_panic("line%d\tassert \"" #exp "\" failed\n", __LINE__); \
}

#else /*  RTE_LIBRTE_MBUF_DEBUG */

/**  check mbuf type in debug mode */
#define __rte_mbuf_sanity_check(m, is_h) do { } while (0)

/**  check mbuf type in debug mode if mbuf pointer is not null */
#define __rte_mbuf_sanity_check_raw(m, is_h) do { } while (0)

/**  MBUF asserts in debug mode */
#define RTE_MBUF_ASSERT(exp)                do { } while (0)

#endif /*  RTE_LIBRTE_MBUF_DEBUG */

#ifdef RTE_MBUF_REFCNT
#ifdef RTE_MBUF_REFCNT_ATOMIC

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 * @param m
 *   Mbuf to update
 * @param value
 *   Value to add/subtract
 * @return
 *   Updated value
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	return (uint16_t)(rte_atomic16_add_return(&m->refcnt_atomic, value));
}

/**
 * Reads the value of an mbuf's refcnt.
 * @param m
 *   Mbuf to read
 * @return
 *   Reference count number.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return (uint16_t)(rte_atomic16_read(&m->refcnt_atomic));
}

/**
 * Sets an mbuf's refcnt to a defined value.
 * @param m
 *   Mbuf to update
 * @param new_value
 *   Value set
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	rte_atomic16_set(&m->refcnt_atomic, new_value);
}

#else /* ! RTE_MBUF_REFCNT_ATOMIC */

/**
 * Adds given value to an mbuf's refcnt and returns its new value.
 */
static inline uint16_t
rte_mbuf_refcnt_update(struct rte_mbuf *m, int16_t value)
{
	m->refcnt = (uint16_t)(m->refcnt + value);
	return m->refcnt;
}

/**
 * Reads the value of an mbuf's refcnt.
 */
static inline uint16_t
rte_mbuf_refcnt_read(const struct rte_mbuf *m)
{
	return m->refcnt;
}

/**
 * Sets an mbuf's refcnt to the defined value.
 */
static inline void
rte_mbuf_refcnt_set(struct rte_mbuf *m, uint16_t new_value)
{
	m->refcnt = new_value;
}

#endif /* RTE_MBUF_REFCNT_ATOMIC */

/** Mbuf prefetch */
#define RTE_MBUF_PREFETCH_TO_FREE(m) do {       \
	if ((m) != NULL)                        \
		rte_prefetch0(m);               \
} while (0)

#else /* ! RTE_MBUF_REFCNT */

/** Mbuf prefetch */
#define RTE_MBUF_PREFETCH_TO_FREE(m) do { } while(0)

#define rte_mbuf_refcnt_set(m,v) do { } while(0)

#endif /* RTE_MBUF_REFCNT */


/**
 * Sanity checks on an mbuf.
 *
 * Check the consistency of the given mbuf. The function will cause a
 * panic if corruption is detected.
 *
 * @param m
 *   The mbuf to be checked.
 * @param is_header
 *   True if the mbuf is a packet header, false if it is a sub-segment
 *   of a packet (in this case, some fields like nb_segs are not checked)
 */
void
rte_mbuf_sanity_check(const struct rte_mbuf *m, int is_header);

/**
 * @internal Allocate a new mbuf from mempool *mp*.
 * The use of that function is reserved for RTE internal needs.
 * Please use rte_pktmbuf_alloc().
 *
 * @param mp
 *   The mempool from which mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *__rte_mbuf_raw_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	void *mb = NULL;
	if (rte_mempool_get(mp, &mb) < 0)
		return NULL;
	m = (struct rte_mbuf *)mb;
#ifdef RTE_MBUF_REFCNT
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
	rte_mbuf_refcnt_set(m, 1);
#endif /* RTE_MBUF_REFCNT */
	return (m);
}

/**
 * @internal Put mbuf back into its original mempool.
 * The use of that function is reserved for RTE internal needs.
 * Please use rte_pktmbuf_free().
 *
 * @param m
 *   The mbuf to be freed.
 */
static inline void __attribute__((always_inline))
__rte_mbuf_raw_free(struct rte_mbuf *m)
{
#ifdef RTE_MBUF_REFCNT
	RTE_MBUF_ASSERT(rte_mbuf_refcnt_read(m) == 0);
#endif /* RTE_MBUF_REFCNT */
#ifdef RTE_LIBRW_PIOT
        m->rw_data.payload = 0;
        m->rw_data.vf_encap = 0;
        m->rw_data.mdata_flag = 0;
#endif
	rte_mempool_put(m->pool, m);
}

/* Operations on ctrl mbuf */

/**
 * The control mbuf constructor.
 *
 * This function initializes some fields in an mbuf structure that are
 * not modified by the user once created (mbuf type, origin pool, buffer
 * start address, and so on). This function is given as a callback function
 * to rte_mempool_create() at pool creation time.
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 * @param m
 *   The mbuf to initialize.
 * @param i
 *   The index of the mbuf in the pool table.
 */
void rte_ctrlmbuf_init(struct rte_mempool *mp, void *opaque_arg,
		void *m, unsigned i);

/**
 * Allocate a new mbuf (type is ctrl) from mempool *mp*.
 *
 * This new mbuf is initialized with data pointing to the beginning of
 * buffer, and with a length of zero.
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
#define rte_ctrlmbuf_alloc(mp) rte_pktmbuf_alloc(mp)

/**
 * Free a control mbuf back into its original mempool.
 *
 * @param m
 *   The control mbuf to be freed.
 */
#define rte_ctrlmbuf_free(m) rte_pktmbuf_free(m)

/**
 * A macro that returns the pointer to the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_data(m) ((char *)((m)->buf_addr) + (m)->data_off)

/**
 * A macro that returns the length of the carried data.
 *
 * The value that can be read or assigned.
 *
 * @param m
 *   The control mbuf.
 */
#define rte_ctrlmbuf_len(m) rte_pktmbuf_data_len(m)

/**
 * Tests if an mbuf is a control mbuf
 *
 * @param m
 *   The mbuf to be tested
 * @return
 *   - True (1) if the mbuf is a control mbuf
 *   - False(0) otherwise
 */
static inline int
rte_is_ctrlmbuf(struct rte_mbuf *m)
{
	return (!!(m->ol_flags & CTRL_MBUF_FLAG));
}

/* Operations on pkt mbuf */

/**
 * The packet mbuf constructor.
 *
 * This function initializes some fields in the mbuf structure that are
 * not modified by the user once created (origin pool, buffer start
 * address, and so on). This function is given as a callback function to
 * rte_mempool_create() at pool creation time.
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 * @param m
 *   The mbuf to initialize.
 * @param i
 *   The index of the mbuf in the pool table.
 */
void rte_pktmbuf_init(struct rte_mempool *mp, void *opaque_arg,
		      void *m, unsigned i);


/**
 * A  packet mbuf pool constructor.
 *
 * This function initializes the mempool private data in the case of a
 * pktmbuf pool. This private data is needed by the driver. The
 * function is given as a callback function to rte_mempool_create() at
 * pool creation. It can be extended by the user, for example, to
 * provide another packet size.
 *
 * @param mp
 *   The mempool from which mbufs originate.
 * @param opaque_arg
 *   A pointer that can be used by the user to retrieve useful information
 *   for mbuf initialization. This pointer comes from the ``init_arg``
 *   parameter of rte_mempool_create().
 */
void rte_pktmbuf_pool_init(struct rte_mempool *mp, void *opaque_arg);

/**
 * Reset the fields of a packet mbuf to their default values.
 *
 * The given mbuf must have only one segment.
 *
 * @param m
 *   The packet mbuf to be resetted.
 */
static inline void rte_pktmbuf_reset(struct rte_mbuf *m)
{
	m->next = NULL;
	m->pkt_len = 0;
	m->l2_l3_len = 0;
	m->vlan_tci = 0;
	m->nb_segs = 1;
	m->port = 0xff;
#ifdef RTE_LIBRW_PIOT
        m->rw_data.payload = 0;
        m->rw_data.vf_encap = 0;
        m->rw_data.mdata_flag = 0;
#endif
	m->ol_flags = 0;
	m->data_off = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;

	m->data_len = 0;
	__rte_mbuf_sanity_check(m, 1);
}

/**
 * Allocate a new mbuf from a mempool.
 *
 * This new mbuf contains one segment, which has a length of 0. The pointer
 * to data is initialized to have some bytes of headroom in the buffer
 * (if buffer size allows).
 *
 * @param mp
 *   The mempool from which the mbuf is allocated.
 * @return
 *   - The pointer to the new mbuf on success.
 *   - NULL if allocation failed.
 */
static inline struct rte_mbuf *rte_pktmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;
	if ((m = __rte_mbuf_raw_alloc(mp)) != NULL)
		rte_pktmbuf_reset(m);
	return (m);
}

#ifdef RTE_MBUF_REFCNT

/**
 * Attach packet mbuf to another packet mbuf.
 * After attachment we refer the mbuf we attached as 'indirect',
 * while mbuf we attached to as 'direct'.
 * Right now, not supported:
 *  - attachment to indirect mbuf (e.g. - md  has to be direct).
 *  - attachment for already indirect mbuf (e.g. - mi has to be direct).
 *  - mbuf we trying to attach (mi) is used by someone else
 *    e.g. it's reference counter is greater then 1.
 *
 * @param mi
 *   The indirect packet mbuf.
 * @param md
 *   The direct packet mbuf.
 */

static inline void rte_pktmbuf_attach(struct rte_mbuf *mi, struct rte_mbuf *md)
{
	RTE_MBUF_ASSERT(RTE_MBUF_DIRECT(md) &&
	    RTE_MBUF_DIRECT(mi) &&
	    rte_mbuf_refcnt_read(mi) == 1);

	rte_mbuf_refcnt_update(md, 1);
	mi->buf_physaddr = md->buf_physaddr;
	mi->buf_addr = md->buf_addr;
	mi->buf_len = md->buf_len;

	mi->next = md->next;
	mi->data_off = md->data_off;
	mi->data_len = md->data_len;
	mi->port = md->port;
	mi->vlan_tci = md->vlan_tci;
	mi->l2_l3_len = md->l2_l3_len;
	mi->hash = md->hash;

	mi->next = NULL;
	mi->pkt_len = mi->data_len;
	mi->nb_segs = 1;
	mi->ol_flags = md->ol_flags;

	__rte_mbuf_sanity_check(mi, 1);
	__rte_mbuf_sanity_check(md, 0);
}

/**
 * Detach an indirect packet mbuf -
 *  - restore original mbuf address and length values.
 *  - reset pktmbuf data and data_len to their default values.
 *  All other fields of the given packet mbuf will be left intact.
 *
 * @param m
 *   The indirect attached packet mbuf.
 */

static inline void rte_pktmbuf_detach(struct rte_mbuf *m)
{
	const struct rte_mempool *mp = m->pool;
	void *buf = RTE_MBUF_TO_BADDR(m);
	uint32_t buf_len = mp->elt_size - sizeof(*m);
	m->buf_physaddr = rte_mempool_virt2phy(mp, m) + sizeof (*m);

	m->buf_addr = buf;
	m->buf_len = (uint16_t)buf_len;

	m->data_off = (RTE_PKTMBUF_HEADROOM <= m->buf_len) ?
			RTE_PKTMBUF_HEADROOM : m->buf_len;

	m->data_len = 0;
}

#endif /* RTE_MBUF_REFCNT */


static inline struct rte_mbuf* __attribute__((always_inline))
__rte_pktmbuf_prefree_seg(struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 0);

#ifdef RTE_MBUF_REFCNT
	if (likely (rte_mbuf_refcnt_read(m) == 1) ||
			likely (rte_mbuf_refcnt_update(m, -1) == 0)) {
		struct rte_mbuf *md = RTE_MBUF_FROM_BADDR(m->buf_addr);

		rte_mbuf_refcnt_set(m, 0);

		/* if this is an indirect mbuf, then
		 *  - detach mbuf
		 *  - free attached mbuf segment
		 */
		if (unlikely (md != m)) {
			rte_pktmbuf_detach(m);
			if (rte_mbuf_refcnt_update(md, -1) == 0)
				__rte_mbuf_raw_free(md);
		}
#endif
		return(m);
#ifdef RTE_MBUF_REFCNT
	}
	return (NULL);
#endif
}

/**
 * Free a segment of a packet mbuf into its original mempool.
 *
 * Free an mbuf, without parsing other segments in case of chained
 * buffers.
 *
 * @param m
 *   The packet mbuf segment to be freed.
 */
static inline void __attribute__((always_inline))
rte_pktmbuf_free_seg(struct rte_mbuf *m)
{
	if (likely(NULL != (m = __rte_pktmbuf_prefree_seg(m)))) {
		m->next = NULL;
		__rte_mbuf_raw_free(m);
	}
}

/**
 * Free a packet mbuf back into its original mempool.
 *
 * Free an mbuf, and all its segments in case of chained buffers. Each
 * segment is added back into its original mempool.
 *
 * @param m
 *   The packet mbuf to be freed.
 */
static inline void rte_pktmbuf_free(struct rte_mbuf *m)
{
	struct rte_mbuf *m_next;

	__rte_mbuf_sanity_check(m, 1);

	while (m != NULL) {
		m_next = m->next;
		rte_pktmbuf_free_seg(m);
		m = m_next;
	}
}

#ifdef RTE_MBUF_REFCNT

/**
 * Creates a "clone" of the given packet mbuf.
 *
 * Walks through all segments of the given packet mbuf, and for each of them:
 *  - Creates a new packet mbuf from the given pool.
 *  - Attaches newly created mbuf to the segment.
 * Then updates pkt_len and nb_segs of the "clone" packet mbuf to match values
 * from the original packet mbuf.
 *
 * @param md
 *   The packet mbuf to be cloned.
 * @param mp
 *   The mempool from which the "clone" mbufs are allocated.
 * @return
 *   - The pointer to the new "clone" mbuf on success.
 *   - NULL if allocation fails.
 */
static inline struct rte_mbuf *rte_pktmbuf_clone(struct rte_mbuf *md,
		struct rte_mempool *mp)
{
	struct rte_mbuf *mc, *mi, **prev;
	uint32_t pktlen;
	uint8_t nseg;

	if (unlikely ((mc = rte_pktmbuf_alloc(mp)) == NULL))
		return (NULL);

	mi = mc;
	prev = &mi->next;
	pktlen = md->pkt_len;
	nseg = 0;

	do {
		nseg++;
		rte_pktmbuf_attach(mi, md);
		*prev = mi;
		prev = &mi->next;
	} while ((md = md->next) != NULL &&
	    (mi = rte_pktmbuf_alloc(mp)) != NULL);

	*prev = NULL;
	mc->nb_segs = nseg;
	mc->pkt_len = pktlen;

	/* Allocation of new indirect segment failed */
	if (unlikely (mi == NULL)) {
		rte_pktmbuf_free(mc);
		return (NULL);
	}

	__rte_mbuf_sanity_check(mc, 1);
	return (mc);
}

/**
 * Adds given value to the refcnt of all packet mbuf segments.
 *
 * Walks through all segments of given packet mbuf and for each of them
 * invokes rte_mbuf_refcnt_update().
 *
 * @param m
 *   The packet mbuf whose refcnt to be updated.
 * @param v
 *   The value to add to the mbuf's segments refcnt.
 */
static inline void rte_pktmbuf_refcnt_update(struct rte_mbuf *m, int16_t v)
{
	__rte_mbuf_sanity_check(m, 1);

	do {
		rte_mbuf_refcnt_update(m, v);
	} while ((m = m->next) != NULL);
}

#endif /* RTE_MBUF_REFCNT */

/**
 * Get the headroom in a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the headroom.
 */
static inline uint16_t rte_pktmbuf_headroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return m->data_off;
}

/**
 * Get the tailroom of a packet mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The length of the tailroom.
 */
static inline uint16_t rte_pktmbuf_tailroom(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return (uint16_t)(m->buf_len - rte_pktmbuf_headroom(m) -
			  m->data_len);
}

/**
 * Get the last segment of the packet.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   The last segment of the given mbuf.
 */
static inline struct rte_mbuf *rte_pktmbuf_lastseg(struct rte_mbuf *m)
{
	struct rte_mbuf *m2 = (struct rte_mbuf *)m;

	__rte_mbuf_sanity_check(m, 1);
	while (m2->next != NULL)
		m2 = m2->next;
	return m2;
}

#ifdef RTE_LIBRW_PIOT
#define rte_pktmbuf_data(m)   ((char *)(m)->buf_addr + (m)->data_off)
#define rte_pktmbuf_next(m)   (m)->next
#define rte_pktmbuf_nsegs(m)  (m)->nb_segs
#define rte_pktmbuf_inport(m) (m)->port
#endif
/**
 * A macro that points to the start of the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that m_headlen(m) is large enough to
 * read its data.
 *
 * @param m
 *   The packet mbuf.
 * @param t
 *   The type to cast the result into.
 */
#define rte_pktmbuf_mtod(m, t) ((t)((char *)(m)->buf_addr + (m)->data_off))

/**
 * A macro that returns the length of the packet.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_pkt_len(m) ((m)->pkt_len)

/**
 * A macro that returns the length of the segment.
 *
 * The value can be read or assigned.
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_data_len(m) ((m)->data_len)

/**
 * Prepend len bytes to an mbuf data area.
 *
 * Returns a pointer to the new
 * data start address. If there is not enough headroom in the first
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The pkt mbuf.
 * @param len
 *   The amount of data to prepend (in bytes).
 * @return
 *   A pointer to the start of the newly prepended data, or
 *   NULL if there is not enough headroom space in the first segment
 */
static inline char *rte_pktmbuf_prepend(struct rte_mbuf *m,
					uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > rte_pktmbuf_headroom(m)))
		return NULL;

	m->data_off -= len;
	m->data_len = (uint16_t)(m->data_len + len);
	m->pkt_len  = (m->pkt_len + len);

	return (char *)m->buf_addr + m->data_off;
}

/**
 * Append len bytes to an mbuf.
 *
 * Append len bytes to an mbuf and return a pointer to the start address
 * of the added data. If there is not enough tailroom in the last
 * segment, the function will return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to append (in bytes).
 * @return
 *   A pointer to the start of the newly appended data, or
 *   NULL if there is not enough tailroom space in the last segment
 */
static inline char *rte_pktmbuf_append(struct rte_mbuf *m, uint16_t len)
{
	void *tail;
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > rte_pktmbuf_tailroom(m_last)))
		return NULL;

	tail = (char *)m_last->buf_addr + m_last->data_off + m_last->data_len;
	m_last->data_len = (uint16_t)(m_last->data_len + len);
	m->pkt_len  = (m->pkt_len + len);
	return (char*) tail;
}

/**
 * Remove len bytes at the beginning of an mbuf.
 *
 * Returns a pointer to the start address of the new data area. If the
 * length is greater than the length of the first segment, then the
 * function will fail and return NULL, without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   A pointer to the new start of the data.
 */
static inline char *rte_pktmbuf_adj(struct rte_mbuf *m, uint16_t len)
{
	__rte_mbuf_sanity_check(m, 1);

	if (unlikely(len > m->data_len))
		return NULL;

	m->data_len = (uint16_t)(m->data_len - len);
	m->data_off += len;
	m->pkt_len  = (m->pkt_len - len);
	return (char *)m->buf_addr + m->data_off;
}

/**
 * Remove len bytes of data at the end of the mbuf.
 *
 * If the length is greater than the length of the last segment, the
 * function will fail and return -1 without modifying the mbuf.
 *
 * @param m
 *   The packet mbuf.
 * @param len
 *   The amount of data to remove (in bytes).
 * @return
 *   - 0: On success.
 *   - -1: On error.
 */
static inline int rte_pktmbuf_trim(struct rte_mbuf *m, uint16_t len)
{
	struct rte_mbuf *m_last;

	__rte_mbuf_sanity_check(m, 1);

	m_last = rte_pktmbuf_lastseg(m);
	if (unlikely(len > m_last->data_len))
		return -1;

	m_last->data_len = (uint16_t)(m_last->data_len - len);
	m->pkt_len  = (m->pkt_len - len);
	return 0;
}

/**
 * Test if mbuf data is contiguous.
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 1, if all data is contiguous (one segment).
 *   - 0, if there is several segments.
 */
static inline int rte_pktmbuf_is_contiguous(const struct rte_mbuf *m)
{
	__rte_mbuf_sanity_check(m, 1);
	return !!(m->nb_segs == 1);
}

/**
 * Dump an mbuf structure to the console.
 *
 * Dump all fields for the given packet mbuf and all its associated
 * segments (in the case of a chained buffer).
 *
 * @param f
 *   A pointer to a file for output
 * @param m
 *   The packet mbuf.
 * @param dump_len
 *   If dump_len != 0, also dump the "dump_len" first data bytes of
 *   the packet.
 */
void rte_pktmbuf_dump(FILE *f, const struct rte_mbuf *m, unsigned dump_len);

#ifdef RTE_LIBRW_PIOT
/*
 * Duplicate a pkt by allocating new mbuf from the given pool
 */

#include <string.h>

#define RW_VF_FLAG_MDATA_L3_OFFET   0x00000001
#define RW_VF_FLAG_MDATA_VFTTL      0x00000002
#define RW_VF_FLAG_MDATA_PATHID     0x00000004
#define RW_VF_FLAG_MDATA_ENCAP      0x00000008
#define RW_VF_FLAG_MDATA_PAYLOAD    0x00000010
#define RW_VF_FLAG_MDATA_LPORTID    0x00000020
#define RW_VF_FLAG_MDATA_PORTMETA   0x00000040
#define RW_VF_FLAG_MDATA_NCID       0x00000080
#define RW_VF_FLAG_MDATA_NH_POLICY  0x00000100
#define RW_VF_FLAG_MDATA_PARSED_LEN 0x00000200 //AKKi this need take a flag since this is internal to ipfp
#define RW_VF_FLAG_MDATA_TRACE_ENA  0x00000400
#define RW_VF_FLAG_MDATA_KNI_FILTER 0x00000800
#define RW_VF_FLAG_MDATA_TRAFGEN_COHERENCY 0x00001000
#define RW_VF_FLAG_MDATA_FLOW_CACHE_TARGET 0x00002000
#define RW_VF_FLAG_MDATA_NH_LPORTID  0x00004000

#define RW_VF_MDATA_FLAG(_m)  ((_m)->rw_data.mdata_flag)


#define RW_VF_SET_MDATA_PORT_RX_DATA(_m, _encap, _protocol, _lport, _ncr, _offset, _parsed_len) \
  {                                                                     \
    (_m)->rw_data.l3_offset = (_offset);                                \
    (_m)->rw_data.vf_encap  = (_encap);                                 \
    (_m)->rw_data.payload   = (_protocol);                              \
    (_m)->rw_data.vf_lportid = (_lport);                                \
    (_m)->rw_data.vf_ncid    = (_ncr);                                  \
    (_m)->rw_data.parsed_len = (_parsed_len);                           \
    (_m)->rw_data.mdata_flag = (RW_VF_FLAG_MDATA_L3_OFFET | RW_VF_FLAG_MDATA_PAYLOAD | RW_VF_FLAG_MDATA_ENCAP | RW_VF_FLAG_MDATA_LPORTID | RW_VF_FLAG_MDATA_NCID | RW_VF_FLAG_MDATA_PARSED_LEN); \
  }

  
#define RW_VF_VALID_MDATA_L3_OFFET(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_L3_OFFET)
#define RW_VF_SET_MDATA_L3_OFFET(_m, _v)    { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_L3_OFFET); (_m)->rw_data.l3_offset = (_v); }
#define RW_VF_GET_MDATA_L3_OFFET(_m)        ((_m)->rw_data.l3_offset)
#define RW_VF_UNSET_MDATA_L3_OFFET(_m)      ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_L3_OFFET))

#define RW_VF_VALID_MDATA_VFTTL(_m)         ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_VFTTL)
#define RW_VF_SET_MDATA_VFTTL(_m, _v)       { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_VFTTL); (_m)->rw_data.vf_ttl = (_v); }
#define RW_VF_GET_MDATA_VFTTL(_m)           ((_m)->rw_data.vf_ttl)
#define RW_VF_UNSET_MDATA_VFTTL(_m)         ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_VFTTL))

#define RW_VF_VALID_MDATA_PATHID(_m)        ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_PATHID)
#define RW_VF_SET_MDATA_PATHID(_m, _v)      { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_PATHID); (_m)->rw_data.vf_pathid = (_v); }
#define RW_VF_GET_MDATA_PATHID(_m)          ((_m)->rw_data.vf_pathid)
#define RW_VF_UNSET_MDATA_PATHID(_m)        ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_PATHID))

#define RW_VF_VALID_MDATA_ENCAP(_m)         ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_ENCAP)
#define RW_VF_SET_MDATA_ENCAP(_m, _v)       { (_m)->rw_data.mdata_flag |= ( RW_VF_FLAG_MDATA_ENCAP); (_m)->rw_data.vf_encap = (_v); }
#define RW_VF_GET_MDATA_ENCAP(_m)           ((_m)->rw_data.vf_encap)
#define RW_VF_UNSET_MDATA_ENCAP(_m)         ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_ENCAP))

#define RW_VF_VALID_MDATA_PAYLOAD(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_PAYLOAD)
#define RW_VF_SET_MDATA_PAYLOAD(_m, _v)    { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_PAYLOAD); (_m)->rw_data.payload = (_v); }
#define RW_VF_GET_MDATA_PAYLOAD(_m)        ((_m)->rw_data.payload)
#define RW_VF_UNSET_MDATA_PAYLOAD(_m)      ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_PAYLOAD))

#define RW_VF_VALID_MDATA_LPORTID(_m)       ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_LPORTID)
#define RW_VF_SET_MDATA_LPORTID(_m, _v)     { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_LPORTID); (_m)->rw_data.vf_lportid = (_v); }
#define RW_VF_GET_MDATA_LPORTID(_m)         ((_m)->rw_data.vf_lportid)
#define RW_VF_UNSET_MDATA_LPORTID(_m)       ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_LPORTID))

#define RW_VF_VALID_MDATA_PORTMETA(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_PORTMETA)
#define RW_VF_SET_MDATA_PORTMETA(_m, _v)    { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_PORTMETA); (_m)->rw_data.vf_portmeta = (_v); }
#define RW_VF_GET_MDATA_PORTMETA(_m)        ((_m)->rw_data.vf_portmeta)
#define RW_VF_UNSET_MDATA_PORTMETA(_m)      ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_PORTMETA))

#define RW_VF_VALID_MDATA_NCID(_m)          ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_NCID)
#define RW_VF_SET_MDATA_NCID(_m, _v)        { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_NCID); (_m)->rw_data.vf_ncid = (_v); }
#define RW_VF_GET_MDATA_NCID(_m)            ((_m)->rw_data.vf_ncid)
#define RW_VF_UNSET_MDATA_NCID(_m)          ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_NCID))

#define RW_VF_VALID_MDATA_NH_POLICY(_m)     ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_NH_POLICY)
#define RW_VF_SET_MDATA_NH_POLICY(_m, _v, size)   { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_NH_POLICY); \
    memcpy((_m)->rw_data.vf_nh_policy_fwd, (_v), (size)); }
#define RW_VF_SET_MDATA_NH_POLICY_ZERO(_m)   { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_NH_POLICY); \
    memset((_m)->rw_data.vf_nh_policy_fwd, 0, sizeof((_m)->rw_data.vf_nh_policy_fwd)); }
#define RW_VF_GET_MDATA_NH_POLICY(_m)       (&((_m)->rw_data.vf_nh_policy_fwd[0]))
#define RW_VF_UNSET_MDATA_NH_POLICY(_m)     ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_NH_POLICY))
#define RW_VF_VALID_MDATA_NH_LPORTID(_m)     ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_NH_LPORTID)
#define RW_VF_SET_MDATA_NH_LPORTID(_m, _v)   { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_NH_LPORTID); (_m)->rw_data.vf_nh_policy_lportid = (_v); }
#define RW_VF_GET_MDATA_NH_LPORTID(_m)       (((_m)->rw_data.vf_nh_policy_lportid))
#define RW_VF_UNSET_MDATA_NH_LPORTID(_m)     ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_NH_LPORTID))
#define RW_VF_VALID_MDATA_PARSED_LEN(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_PARSED_LEN)
#define RW_VF_SET_MDATA_PARSED_LEN(_m, _v)    { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_PARSED_LEN); (_m)->rw_data.parsed_len = (_v); }
#define RW_VF_GET_MDATA_PARSED_LEN(_m)        ((_m)->rw_data.parsed_len)
#define RW_VF_UNSET_MDATA_PARSED_LEN(_m)      ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_PARSED_LEN))

#define RW_VF_VALID_MDATA_TRACE_ENA(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_TRACE_ENA)
#define RW_VF_SET_MDATA_TRACE_ENA(_m)        ((_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_TRACE_ENA))
#define RW_VF_UNSET_MDATA_TRACE_ENA(_m)      ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_TRACE_ENA))

#define RW_VF_VALID_MDATA_KNI_FILTER_FLAG(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_KNI_FILTER)
#define RW_VF_SET_MDATA_KNI_FILTER_FLAG(_m, flag)        {((_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_KNI_FILTER)); (_m)->rw_data.kni_filter_flag |= (flag); }
#define RW_VF_GET_MDATA_KNI_FILTER_FLAG(_m)       (((_m)->rw_data.kni_filter_flag))
#define RW_VF_UNSET_MDATA_KNI_FILTER_FLAG(_m)      {((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_KNI_FILTER)); (_m)->rw_data.kni_filter_flag =0;}


#define RW_VF_VALID_MDATA_TRAFGEN_COHERENCY(_m)      ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_TRAFGEN_COHERENCY)
#define RW_VF_SET_MDATA_TRAFGEN_COHERENCY(_m, flag)        {((_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_TRAFGEN_COHERENCY)); (_m)->rw_data.trafgen_coherency = (flag); }
#define RW_VF_GET_MDATA_TRAFGEN_COHERENCY(_m)       (((_m)->rw_data.trafgen_coherency))
#define RW_VF_UNSET_MDATA_TRAFGEN_COHERENCY(_m)      {((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_TRAFGEN_COHERENCY)); (_m)->rw_data.trafgen_coherency =0;}  

#define RW_VF_VALID_MDATA_FLOW_CACHE_TARGET(_m)         ((_m)->rw_data.mdata_flag & RW_VF_FLAG_MDATA_FLOW_CACHE_TARGET)
#define RW_VF_SET_MDATA_FLOW_CACHE_TARGET(_m, _v)       { (_m)->rw_data.mdata_flag |= (RW_VF_FLAG_MDATA_FLOW_CACHE_TARGET); (_m)->rw_data.vf_flow_cache_target = (_v); }
#define RW_VF_GET_MDATA_FLOW_CACHE_TARGET(_m)           ((_m)->rw_data.vf_flow_cache_target)
#define RW_VF_UNSET_MDATA_FLOW_CACHE_TARGET(_m)         ((_m)->rw_data.mdata_flag &= (~RW_VF_FLAG_MDATA_FLOW_CACHE_TARGET))

static inline void rw_vfabric_copy_data(struct rte_mbuf *to, const struct rte_mbuf *from){
  RW_VF_MDATA_FLAG(to) = RW_VF_MDATA_FLAG(from);
  if (RW_VF_VALID_MDATA_L3_OFFET(from)){
    RW_VF_SET_MDATA_L3_OFFET(to, RW_VF_GET_MDATA_L3_OFFET(from));
  }
  if (RW_VF_VALID_MDATA_VFTTL(from)){
    RW_VF_SET_MDATA_VFTTL(to, RW_VF_GET_MDATA_VFTTL(from));
  }
  if (RW_VF_VALID_MDATA_PATHID(from)){
    RW_VF_SET_MDATA_PATHID(to, RW_VF_GET_MDATA_PATHID(from));
  }
  if (RW_VF_VALID_MDATA_ENCAP(from)){
    RW_VF_SET_MDATA_ENCAP(to, RW_VF_GET_MDATA_ENCAP(from));
  }
  if (RW_VF_VALID_MDATA_PAYLOAD(from)){
    RW_VF_SET_MDATA_PAYLOAD(to, RW_VF_GET_MDATA_PAYLOAD(from));
  }
  if (RW_VF_VALID_MDATA_LPORTID(from)){
    RW_VF_SET_MDATA_LPORTID(to, RW_VF_GET_MDATA_LPORTID(from));
  }
  if (RW_VF_VALID_MDATA_PORTMETA(from)){
    RW_VF_SET_MDATA_PORTMETA(to, RW_VF_GET_MDATA_PORTMETA(from));
  }
  if (RW_VF_VALID_MDATA_NCID(from)){
    RW_VF_SET_MDATA_NCID(to, RW_VF_GET_MDATA_NCID(from));
  }
  if (RW_VF_VALID_MDATA_NH_POLICY(from)){
    RW_VF_SET_MDATA_NH_POLICY(to, RW_VF_GET_MDATA_NH_POLICY(from), 16);//AKKI
  }
  if (RW_VF_VALID_MDATA_NH_LPORTID(from)){
    RW_VF_SET_MDATA_NH_LPORTID(to, RW_VF_GET_MDATA_NH_LPORTID(from));
  }
  if (RW_VF_VALID_MDATA_KNI_FILTER_FLAG(from)){
    RW_VF_SET_MDATA_KNI_FILTER_FLAG(to,
                                    RW_VF_GET_MDATA_KNI_FILTER_FLAG(from));
  }
  if (RW_VF_VALID_MDATA_FLOW_CACHE_TARGET(from)){
    RW_VF_SET_MDATA_FLOW_CACHE_TARGET(to, RW_VF_GET_MDATA_FLOW_CACHE_TARGET(from));
  }
}


static inline struct rte_mbuf* rte_pktmbuf_allocate_size(struct rte_mempool *mp, int size){
  struct rte_mbuf *copy;
  uint8_t nseg;
  struct rte_mbuf *mi, **prev;
  
  copy = rte_pktmbuf_alloc(mp);
  
  if (!copy){
    return NULL;
  }
  nseg = (size/ rte_pktmbuf_tailroom(copy));
  mi = copy;
  prev = &mi->next;
  
  while (nseg > 0){
    mi =  rte_pktmbuf_alloc(mp);
    
    if (!mi){
      rte_pktmbuf_free(copy);
      return NULL;
    }
    
    *prev = mi;
    prev = &mi->next;
    copy->nb_segs++;
    nseg--;
  }
  return copy;
}

#if 0
static inline struct rte_mbuf * rte_pktmbuf_duplicate(const struct rte_mbuf *m,
                                                      struct rte_mempool *mp)
{
  struct rte_mbuf *copy;
  struct rte_mbuf *mi, **prev;
  uint32_t pktlen;
  uint8_t nseg;

  copy = rte_pktmbuf_alloc(mp);

  if (!copy){
    return NULL;
  }

  mi = copy;
  prev = &mi->next;
  pktlen = rte_pktmbuf_pkt_len(m);
  nseg = 0;
  do {
    nseg++;
    RTE_MBUF_ASSERT(RTE_MBUF_DIRECT(m) &&
                    RTE_MBUF_DIRECT(mi) &&
                    rte_mbuf_refcnt_read(mi) == 1);

    rte_pktmbuf_data_len(mi) = rte_pktmbuf_data_len(m);
    {
      void *to, *from;
      to = rte_pktmbuf_mtod(mi, void*);
      from = rte_pktmbuf_mtod(m, void*);
      memcpy(to, from, rte_pktmbuf_data_len(m));
    }
    rw_vfabric_copy_data(mi, m);
    
    mi->next = NULL;
    rte_pktmbuf_pkt_len(mi)= rte_pktmbuf_pkt_len(m);
    mi->nb_segs = 1;

    *prev = mi;
    prev = &mi->next;

  }while ((m = m->next) != NULL &&
          (mi = rte_pktmbuf_alloc(mp)) != NULL);

  *prev = NULL;
  copy->nb_segs = nseg;
  rte_pktmbuf_pkt_len(copy)  = pktlen;

  if (unlikely (mi == NULL)) {
    rte_pktmbuf_free(copy);
    return (NULL);
  }

  __rte_mbuf_sanity_check(copy, 1);

  return copy;
}
#else

static inline struct rte_mbuf* rte_pktmbuf_duplicate(const struct rte_mbuf *m,
                                                     struct rte_mempool *mp)
{
  struct rte_mbuf *mi,*copy = NULL;
  unsigned char *to, *from;
  int len, copylen, remlen, next, datalen;

  copy = rte_pktmbuf_allocate_size(mp, rte_pktmbuf_pkt_len(m));
  
  if (!copy){
    return NULL;
  }
  
  copy->pkt_len =rte_pktmbuf_pkt_len(m);
  mi = copy;
  //Need the topmost vf data
  rw_vfabric_copy_data(mi, m);
  to = rte_pktmbuf_mtod(mi, unsigned char*);
  remlen = rte_pktmbuf_tailroom(mi);
  next = 0;
  datalen = 0;
  
  while(m){
    len = rte_pktmbuf_data_len(m);
    from = rte_pktmbuf_mtod(m, unsigned char*);
    
    while (len > 0){
      copylen = len;
      if (copylen > remlen){
        copylen = remlen;
        next = 1;
      }
      memcpy(to, from, copylen);
      to += copylen;
      from += copylen;
      len -= copylen;
      remlen -= copylen;
      datalen += copylen;
      if (next){
        mi->data_len = datalen;
        mi = mi->next;
        to = rte_pktmbuf_mtod(mi, unsigned char*);
        remlen = rte_pktmbuf_tailroom(mi);
        next = 0;
        datalen = 0;
      }
    }
    m = m->next;
  }
  mi->data_len = datalen;
  return copy;
}
#endif
  
static inline struct rte_mbuf* rte_pktmbuf_linearize_self(struct rte_mbuf *m)
{
  struct rte_mbuf *copy = NULL;
  unsigned char *to, *from;
  int len, remlen;
  
  copy = m;
  m = m->next;
  
  to = rte_pktmbuf_mtod(copy, unsigned char*);
  to += rte_pktmbuf_data_len(copy);
  remlen = rte_pktmbuf_tailroom(copy);
  
  while(m){
    len = rte_pktmbuf_data_len(m);
    from = rte_pktmbuf_mtod(m, unsigned char*);
    RTE_MBUF_ASSERT((len > remlen));
    memcpy(to, from, len);
    to += len;
    remlen -= len;
    copy->data_len += len;
    m = m->next;
  }
  RTE_MBUF_ASSERT((rte_pktmbuf_data_len(copy) != rte_pktmbuf_pkt_len(copy)));
  copy->nb_segs = 1;
  m = copy->next;
  copy->next = NULL;
  if (m){
    rte_pktmbuf_free(m);
  }
  
  return copy;
}
  
  
#endif

#ifdef __cplusplus
}

#endif

#endif /* _RTE_MBUF_H_ */
