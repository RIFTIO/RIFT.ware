/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw-vfabric.h
 * @author Sanil Puthiyandyil
 * @date 8/29/2014
 * @Riftware Virtual fabric packet format definition
 *
 * @details
 */

#ifndef _RWVFABRIC_H_
#define _RWVFABRIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RW_VFABRIC_DECODE 1
#define RW_VIRTUAL_FABRIC_UDP_PORT  65456
#define RW_VFABRIC_VERSION          0x0
#define RW_VFABRIC_DEFAULT_TTL      8

#define RW_VFABRIC_GET_VERSION(_x)   (((_x)>>5) & 0x07)
#define RW_VFABRIC_GET_TTL(_x)       ((_x) & 0x1F)

typedef struct  {
  uint8_t   ver_ttl;        /***< version and ttl */
  uint8_t   tos;            /***< tos/qos */
  uint8_t   path_id;        /***< path identifier */
  uint8_t   quick_flags;    /***< header flags */
  uint32_t  src_vfapid;     /***< src vfapid */
  uint32_t  dst_vfapid;     /***< dst vfapid */
  uint16_t  payload_len;    /***< payload len */
  uint8_t   payload_type;   /***< payload type */
  uint8_t   ext_len;        /***< total extension header len */
} __attribute__((__packed__)) rw_vfabric_header_t;

#define RW_VFABRIC_PAYLOAD_ETH    0
#define RW_VFABRIC_PAYLOAD_IPV4   1
#define RW_VFABRIC_PAYLOAD_IPV6   2


#define RW_VFABRIC_EXT_H_PADDING     0
#define RW_VFABRIC_EXT_H_IN_PDU_INFO 1
#define RW_VFABRIC_EXT_H_PORT_TX     2
#define RW_VFABRIC_EXT_H_FWD_TX      3
#define RW_VFABRIC_EXT_H_IN_LOOKUP   4
#define RW_VFABRIC_EXT_H_DLVR_APP    5
#define RW_VFABRIC_EXT_H_DLVR_KERN   6
#define RW_VFABRIC_EXT_H_IP_REASM    7
#define RW_VFABRIC_EXT_H_ECHO        8

typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint8_t  val[0];
} __attribute__((__packed__)) rw_fabric_ext_h_generic_t;

typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint8_t  val[0];
} __attribute__((__packed__)) rw_fabric_ext_h_padding_t;

#define RW_VFABRIC_PLOAD_ENCAP_NONE       0
#define RW_VFABRIC_PLOAD_ENCAP_8021PQ     1
#define RW_VFABRIC_PLOAD_ENCAP_8021AD     2
#define RW_VFABRIC_PLOAD_ENCAP_MPLS_STACK 3

typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint8_t  encap;
  uint8_t  l3_offset;
  uint32_t nc_id;
  uint32_t lport_id;
  uint32_t port_meta;
  uint8_t  opt_encap[0];
} __attribute__((__packed__)) rw_fabric_ext_h_in_pdu_info_t;


typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint16_t pad2;
  uint32_t lport_id;
} __attribute__((__packed__)) rw_fabric_ext_h_port_tx_t;

#define RW_VFABRIC_FWD_L3_SIMPLE   0
#define RW_VFABRIC_FWD_L3_NEXTHOP  1
#define RW_VFABRIC_FWD_VRF         3
#define RW_VFABRIC_FWD_MPLS        4
#define RW_VFABRIC_FWD_VLAN        5
#define RW_VFABRIC_FWD_TUN         6

typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint8_t  fwd_type;
  uint8_t  pad2;
  uint32_t nc_id;
  uint8_t  opt_info[0];
} __attribute__((__packed__)) rw_fabric_ext_h_fwd_tx_t;


typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint16_t pad2;
  uint32_t working_sign_id;
} __attribute__((__packed__)) rw_fabric_ext_h_in_lookup_t;


typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint16_t pad2;
  uint32_t vsap_id;
  uint32_t vsap_handle;
  uint32_t flowid_msw;
  uint32_t flowid_lsw;
} __attribute__((__packed__)) rw_fabric_ext_h_dlvr_app_t;


typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint16_t pad2;
  uint32_t vsap_id;
  /* more fields - TBD */
  uint8_t  opt_info[0];
} __attribute__((__packed__)) rw_fabric_ext_h_dlvr_kernel_t;


typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint16_t pad2;
  uint32_t working_sign_id;
} __attribute__((__packed__)) rw_fabric_ext_h_ip_reasm_t;


#define RW_VFABRIC_ECHO_REQUEST  0
#define RW_VFABRIC_ECHO_RESPONSE 1

typedef struct {
  uint8_t  type;
  uint8_t  len;
  uint8_t  sub_type;
  uint8_t  pad2;
  uint32_t identifier;
  uint32_t timestamp_msw;
  uint32_t timestamp_lsw;
} __attribute__((__packed__)) rw_fabric_ext_h_echo_t;

void
rw_vf_print(const u_char *dat, u_int length);

#endif /* _RWVFABRIC_H_ */
