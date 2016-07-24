/*-
 *   This file is provided under a dual BSD/LGPLv2 license.  When using or
 *   redistributing this file, you may do so under either license.
 *
 *   GNU LESSER GENERAL PUBLIC LICENSE
 *
 *   Copyright(c) 2007-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2.1 of the GNU Lesser General Public License
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *   Contact Information:
 *   Intel Corporation
 *
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _RTE_KNI_COMMON_H_
#define _RTE_KNI_COMMON_H_

#ifdef __KERNEL__
#include <linux/if.h>
#endif
#ifdef RTE_LIBRW_PIOT
#include <linux/filter.h>
#endif
/**
 * KNI name is part of memzone name.
 */
#define RTE_KNI_NAMESIZE 32

/*
 * Request id.
 */
enum rte_kni_req_id {
	RTE_KNI_REQ_UNKNOWN = 0,
	RTE_KNI_REQ_CHANGE_MTU,
	RTE_KNI_REQ_CFG_NETWORK_IF,
	RTE_KNI_REQ_MAX,
};

/*
 * Structure for KNI request.
 */
struct rte_kni_request {
	uint32_t req_id;             /**< Request id */
	union {
		uint32_t new_mtu;    /**< New MTU */
		uint8_t if_up;       /**< 1: interface up, 0: interface down */
	};
	int32_t result;               /**< Result for processing request */
} __attribute__((__packed__));

/*
 * Fifo struct mapped in a shared memory. It describes a circular buffer FIFO
 * Write and read should wrap around. Fifo is empty when write == read
 * Writing should never overwrite the read position
 */
struct rte_kni_fifo {
	volatile unsigned write;     /**< Next position to be written*/
	volatile unsigned read;      /**< Next position to be read */
	unsigned len;                /**< Circular buffer length */
	unsigned elem_size;          /**< Pointer size - for 32/64 bit OS */
	void * volatile buffer[0];   /**< The buffer contains mbuf pointers */
};

#ifdef RTE_LIBRW_PIOT
/* define a set of marker types that can be used to refer to set points in the
 * mbuf */
typedef void    *MARKER[0];   /**< generic marker for a point in a structure */
typedef uint64_t MARKER64[0]; /**< marker that allows us to overwrite 8 bytes
                               * with a single assignment */

struct rw_kni_mbuf_metadata{
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

    uint32_t parsed_len;
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
#define RW_FPATH_KNI_MAX_MBUF_SIZE 2688 /*RW_FPATH_MAX_PKT_LEN*/
#define RW_FPATH_KNI_MAX_SEGS      10
#endif
/*
 * The kernel image of the rte_mbuf struct, with only the relevant fields.
 * Padding is necessary to assure the offsets of these fields
 */
struct rte_kni_mbuf {
	void *buf_addr __attribute__((__aligned__(64)));
        char pad0[10];
	uint16_t data_off;      /**< Start address of data in segment buffer. */
#ifdef RTE_LIBRW_PIOT
  	union {
#ifdef RTE_MBUF_REFCNT
		uint16_t refcnt_atomic; /**< Atomically accessed refcnt */
		uint16_t refcnt;              /**< Non-atomically accessed refcnt */
#endif
		uint16_t refcnt_reserved;     /**< Do not use this field */
	};
	uint8_t nb_segs;          /**< Number of segments. */
	uint8_t port;             /**< Input port. */
#else
	char pad1[4];
#endif
	uint64_t ol_flags;      /**< Offload features. */
	char pad2[2];
	uint16_t data_len;      /**< Amount of data in segment buffer. */
	uint32_t pkt_len;       /**< Total pkt len: sum of all segment data_len. */
#ifdef RTE_LIBRW_PIOT
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
  MARKER cacheline1 __attribute__((__aligned__(64)));
#endif
  
  /* fields on second cache line */
  union{
    char pad3[8];
#ifdef RTE_LIBRW_PIOT
    struct rw_kni_mbuf_metadata meta_data;
#endif
  };
  void *pool;
  void *next;
}__attribute__((__aligned__));;

/*
 * Struct used to create a KNI device. Passed to the kernel in IOCTL call
 */

struct rte_kni_device_info {
	char name[RTE_KNI_NAMESIZE];  /**< Network device name for KNI */

	phys_addr_t tx_phys;
	phys_addr_t rx_phys;
	phys_addr_t alloc_phys;
	phys_addr_t free_phys;

	/* Used by Ethtool */
	phys_addr_t req_phys;
	phys_addr_t resp_phys;
	phys_addr_t sync_phys;
	void * sync_va;

	/* mbuf mempool */
	void * mbuf_va;
	phys_addr_t mbuf_phys;

	/* PCI info */
	uint16_t vendor_id;           /**< Vendor ID or PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or PCI_ANY_ID. */
	uint8_t bus;                  /**< Device bus */
	uint8_t devid;                /**< Device ID */
	uint8_t function;             /**< Device function. */

	uint16_t group_id;            /**< Group ID */
	uint32_t core_id;             /**< core ID to bind for kernel thread */

	uint8_t force_bind : 1;       /**< Flag for kernel thread binding */

	/* mbuf size */
	unsigned mbuf_size;
#ifdef RTE_LIBRW_PIOT
  int     netns_fd;
  char    netns_name[64];
  uint8_t no_data :1;        /**< Flag to indicate whether the device supports
                                ethtool*/
  uint8_t no_pci:1;             /**< Flag to indicate whether the device has pci*/
  uint8_t always_up:1;
  uint8_t no_tx:1;
  uint8_t loopback:1;
  uint8_t no_user_ring:1;
#ifdef RTE_LIBRW_NOHUGE
  uint8_t   nohuge;
  uint32_t  nl_pid;
#endif
  uint32_t ifindex; /**<ifindex to be used by the kernel when creating the netdevice*/
  uint16_t mtu;
  uint16_t vlanid;
  char    mac[6];
  uint16_t unused;
#endif
};

#define KNI_DEVICE "kni"

#define RTE_KNI_IOCTL_TEST    _IOWR(0, 1, int)
#define RTE_KNI_IOCTL_CREATE  _IOWR(0, 2, struct rte_kni_device_info)
#define RTE_KNI_IOCTL_RELEASE _IOWR(0, 3, struct rte_kni_device_info)
#ifdef RTE_LIBRW_PIOT
#define RTE_KNI_IOCTL_GET_PACKET_SOCKET_INFO _IOWR(0, 4, struct rte_kni_packet_socket_info)
#define RTE_KNI_IOCTL_CLEAR_PACKET_COUNTERS _IOWR(0, 5, int)

#define RW_KNI_VF_FLAG_MDATA_L3_OFFET   0x00000001
#define RW_KNI_VF_FLAG_MDATA_VFTTL      0x00000002
#define RW_KNI_VF_FLAG_MDATA_PATHID     0x00000004
#define RW_KNI_VF_FLAG_MDATA_ENCAP      0x00000008
#define RW_KNI_VF_FLAG_MDATA_PAYLOAD    0x00000010
#define RW_KNI_VF_FLAG_MDATA_LPORTID    0x00000020
#define RW_KNI_VF_FLAG_MDATA_PORTMETA   0x00000040
#define RW_KNI_VF_FLAG_MDATA_NCID       0x00000080
#define RW_KNI_VF_FLAG_MDATA_NH_POLICY  0x00000100
#define RW_KNI_VF_FLAG_MDATA_PARSED_LEN 0x00000200 //AKKi this need take a flag since this is internal to ipfp
#define RW_KNI_VF_FLAG_MDATA_TRACE_ENA  0x00000400
#define RW_KNI_VF_FLAG_MDATA_KNI_FILTER 0x00000800
#define RW_KNI_VF_FLAG_MDATA_TRAFGEN_COHERENCY 0x00001000
#define RW_KNI_VF_FLAG_MDATA_FLOW_CACHE_TARGET 0x00002000
#define RW_KNI_VF_FLAG_MDATA_NH_LPORTID  0x00004000

  
#define RW_KNI_VF_VALID_MDATA_L3_OFFET(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_L3_OFFET)
#define RW_KNI_VF_SET_MDATA_L3_OFFET(_m, _v)    { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_L3_OFFET); (_m)->l3_offset = (_v); }
#define RW_KNI_VF_GET_MDATA_L3_OFFET(_m)        ((_m)->l3_offset)
#define RW_KNI_VF_UNSET_MDATA_L3_OFFET(_m)      ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_L3_OFFET))

#define RW_KNI_VF_VALID_MDATA_VFTTL(_m)         ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_VFTTL)
#define RW_KNI_VF_SET_MDATA_VFTTL(_m, _v)       { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_VFTTL); (_m)->vf_ttl = (_v); }
#define RW_KNI_VF_GET_MDATA_VFTTL(_m)           ((_m)->vf_ttl)
#define RW_KNI_VF_UNSET_MDATA_VFTTL(_m)         ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_VFTTL))

#define RW_KNI_VF_VALID_MDATA_PATHID(_m)        ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_PATHID)
#define RW_KNI_VF_SET_MDATA_PATHID(_m, _v)      { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_PATHID); (_m)->vf_pathid = (_v); }
#define RW_KNI_VF_GET_MDATA_PATHID(_m)          ((_m)->vf_pathid)
#define RW_KNI_VF_UNSET_MDATA_PATHID(_m)        ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_PATHID))

#define RW_KNI_VF_VALID_MDATA_ENCAP(_m)         ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_ENCAP)
#define RW_KNI_VF_SET_MDATA_ENCAP(_m, _v)       { (_m)->mdata_flag |= ( RW_KNI_VF_FLAG_MDATA_ENCAP); (_m)->vf_encap = (_v); }
#define RW_KNI_VF_GET_MDATA_ENCAP(_m)           ((_m)->vf_encap)
#define RW_KNI_VF_UNSET_MDATA_ENCAP(_m)         ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_ENCAP))

#define RW_KNI_VF_VALID_MDATA_PAYLOAD(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_PAYLOAD)
#define RW_KNI_VF_SET_MDATA_PAYLOAD(_m, _v)    { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_PAYLOAD); (_m)->payload = (_v); }
#define RW_KNI_VF_GET_MDATA_PAYLOAD(_m)        ((_m)->payload)
#define RW_KNI_VF_UNSET_MDATA_PAYLOAD(_m)      ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_PAYLOAD))

#define RW_KNI_VF_VALID_MDATA_LPORTID(_m)       ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_LPORTID)
#define RW_KNI_VF_SET_MDATA_LPORTID(_m, _v)     { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_LPORTID); (_m)->vf_lportid = (_v); }
#define RW_KNI_VF_GET_MDATA_LPORTID(_m)         ((_m)->vf_lportid)
#define RW_KNI_VF_UNSET_MDATA_LPORTID(_m)       ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_LPORTID))

#define RW_KNI_VF_VALID_MDATA_PORTMETA(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_PORTMETA)
#define RW_KNI_VF_SET_MDATA_PORTMETA(_m, _v)    { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_PORTMETA); (_m)->vf_portmeta = (_v); }
#define RW_KNI_VF_GET_MDATA_PORTMETA(_m)        ((_m)->vf_portmeta)
#define RW_KNI_VF_UNSET_MDATA_PORTMETA(_m)      ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_PORTMETA))

#define RW_KNI_VF_VALID_MDATA_NCID(_m)          ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_NCID)
#define RW_KNI_VF_SET_MDATA_NCID(_m, _v)        { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_NCID); (_m)->vf_ncid = (_v); }
#define RW_KNI_VF_GET_MDATA_NCID(_m)            ((_m)->vf_ncid)
#define RW_KNI_VF_UNSET_MDATA_NCID(_m)          ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_NCID))

#define RW_KNI_VF_VALID_MDATA_NH_POLICY(_m)     ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_NH_POLICY)
#define RW_KNI_VF_SET_MDATA_NH_POLICY(_m, _v, size)   { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_NH_POLICY); \
    memcpy((_m)->vf_nh_policy_fwd, (_v), (size)); }
#define RW_KNI_VF_GET_MDATA_NH_POLICY(_m)       (&((_m)->vf_nh_policy_fwd[0]))
#define RW_KNI_VF_UNSET_MDATA_NH_POLICY(_m)     ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_NH_POLICY))
#define RW_KNI_VF_VALID_MDATA_NH_LPORTID(_m)     ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_NH_LPORTID)
#define RW_KNI_VF_SET_MDATA_NH_LPORTID(_m, _v)   { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_NH_LPORTID); (_m)->vf_nh_policy_lportid = (_v); } 
#define RW_KNI_VF_GET_MDATA_NH_LPORTID(_m)       (((_m)->vf_nh_policy_lportid))
#define RW_KNI_VF_UNSET_MDATA_NH_LPORTID(_m)     ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_NH_LPORTID))
#define RW_KNI_VF_VALID_MDATA_PARSED_LEN(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_PARSED_LEN)
#define RW_KNI_VF_SET_MDATA_PARSED_LEN(_m, _v)    { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_PARSED_LEN); (_m)->parsed_len = (_v); }
#define RW_KNI_VF_GET_MDATA_PARSED_LEN(_m)        ((_m)->parsed_len)
#define RW_KNI_VF_UNSET_MDATA_PARSED_LEN(_m)      ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_PARSED_LEN))

#define RW_KNI_VF_VALID_MDATA_TRACE_ENA(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_TRACE_ENA)
#define RW_KNI_VF_SET_MDATA_TRACE_ENA(_m)        ((_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_TRACE_ENA))
#define RW_KNI_VF_UNSET_MDATA_TRACE_ENA(_m)      ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_TRACE_ENA))

#define RW_KNI_VF_VALID_MDATA_KNI_FILTER_FLAG(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_KNI_FILTER)
#define RW_KNI_VF_SET_MDATA_KNI_FILTER_FLAG(_m, flag)        {((_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_KNI_FILTER)); (_m)->kni_filter_flag |= (flag); }
#define RW_KNI_VF_GET_MDATA_KNI_FILTER_FLAG(_m)       (((_m)->kni_filter_flag))
#define RW_KNI_VF_UNSET_MDATA_KNI_FILTER_FLAG(_m)      {((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_KNI_FILTER)); (_m)->kni_filter_flag =0;}  
#define RW_KNI_VF_VALID_MDATA_TRAFGEN_COHERENCY(_m)      ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_TRAFGEN_COHERENCY)
#define RW_KNI_VF_SET_MDATA_TRAFGEN_COHERENCY(_m, flag)        {((_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_TRAFGEN_COHERENCY)); (_m)->trafgen_coherency = (flag); }
#define RW_KNI_VF_GET_MDATA_TRAFGEN_COHERENCY(_m)       (((_m)->trafgen_coherency))
#define RW_KNI_VF_UNSET_MDATA_TRAFGEN_COHERENCY(_m)      {((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_TRAFGEN_COHERENCY)); (_m)->trafgen_coherency =0;}  


#define RW_KNI_VF_VALID_MDATA_FLOW_CACHE_TARGET(_m)         ((_m)->mdata_flag & RW_KNI_VF_FLAG_MDATA_FLOW_CACHE_TARGET)
#define RW_KNI_VF_SET_MDATA_FLOW_CACHE_TARGET(_m, _v)       { (_m)->mdata_flag |= (RW_KNI_VF_FLAG_MDATA_FLOW_CACHE_TARGET); (_m)->vf_flow_cache_target = (_v); }                                            
#define RW_KNI_VF_GET_MDATA_FLOW_CACHE_TARGET(_m)           ((_m)->vf_flow_cache_target)
#define RW_KNI_VF_UNSET_MDATA_FLOW_CACHE_TARGET(_m)         ((_m)->mdata_flag &= (~RW_KNI_VF_FLAG_MDATA_FLOW_CACHE_TARGET))



/**
 * Struct used to get the filters set on the AF_PACKET socket.
 * Passed to the kernel in IOCTL call. This should work for linux
 */
struct rte_kni_packet_socket_info {
        char              name[RTE_KNI_NAMESIZE];  /**< Network device name for KNI */
        uint32_t          inode_id; /**< Inode-id of the AF_PACKET socket */
        void              *skt; /**< Socket pointer for verification that we have the correct inode */
        uint16_t          len; /**< lenght of the instructions*/
        struct sock_filter filter[BPF_MAXINSNS];/**< BPF filters for linux */
};


#endif /*RTE_LIBRW_PIOT*/


#endif /* _RTE_KNI_COMMON_H_ */
