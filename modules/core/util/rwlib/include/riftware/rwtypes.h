
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


/**
 * @file rwtypes.h
 * @author Rajesh Velandy (rajesh.velandy@riftio.com)
 * @date 04/23/14
 * @header file for defining types that can be used to extend in protobuf definitions
 */
#ifndef __RWTYPES_H__
#define __RWTYPES_H__

#include <arpa/inet.h>
#include <stdio.h>
#include <rwtypes_gi.h>

#include <protobuf-c/rift-protobuf-c.h>


#define RW_IPV4 AF_INET
#define RW_IPV6 AF_INET6

__BEGIN_DECLS

// DEFINITION OF rw_ip_addr_t
typedef uint8_t rw_ip_version_t;

#define RW_IP_VERSION(_addr_) (_addr_)->ip_v
#define RW_IS_ADDR_IPV4(_addr_) (((_addr_)->ip_v == RW_IPV4)? TRUE:FALSE)
#define RW_IS_ADDR_IPV6(_addr_) (((_addr_)->ip_v == RW_IPV6)? TRUE:FALSE)
#define RW_IP_ADDR_FAMILY(addr_p)  ((addr_p)->ip_v)
#define RW_IPV4_ADDR(_addr_) ((_addr_)->u.v4.addr)
#define RW_IPV6_ADDR(_addr_) ((_addr_)->u.v6.addr)
#define RW_CMP_ADDRESS(_a, _b)                                          \
  (((_a)->ip_v != (_b)->ip_v)? FALSE:                                   \
   (RW_IS_ADDR_IPV4(_a)? ((_a)->u.v4.addr == (_b)->u.v4.addr) :         \
    (!memcmp((_a)->u.v6.addr,                                           \
             (_b)->u.v6.addr, sizeof((_a)->u.v6.addr)))))


typedef struct rw_ipV4_addr_s {
  uint32_t    addr;
}rw_ipV4_addr_t;

extern const struct ProtobufCCTypeDescriptor rw_ipV4_addr_t_helper;
#define rw_ipV4_addr_t_STATIC_INIT {}

typedef struct rw_ipV6_addr_s {
  uint32_t    addr[4];
}rw_ipV6_addr_t;

extern const struct ProtobufCCTypeDescriptor rw_ipV6_addr_t_helper;
#define rw_ipV6_addr_t_STATIC_INIT {}

typedef struct rw_ip_addr_s {
  rw_ip_version_t ip_v;
  union {
    rw_ipV6_addr_t   v6;
    rw_ipV4_addr_t   v4;
  } u;
  uint8_t pad[7];
} rw_ip_addr_t;
char* rw_ip_addr_t_get_str(char* str, const rw_ip_addr_t *ip_addr);

int rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(void *p, const char *str, size_t str_len);
int rw_c_type_helper_rw_ip_addr_t_get_str_from_impl(char* str, const void *p);

extern const struct ProtobufCCTypeDescriptor rw_ip_addr_t_helper;
#define rw_ip_addr_t_STATIC_INIT {RW_IPV6}


// The following macro is used to define the maximum length of the string  representation of the C-Type.
// Since the c_type<->str routines are not passing the length argument there is an assumption that the callee
// allocates a buffer of the following length before calling   the helper callbacks.

#define  RW_C_TYPE_MAX_XML_STR_LEN 128

#define RW_SET_IPV4_ADDR(addr_p, v4_addr)  ((addr_p)->ip_v = RW_IPV4, (addr_p)->u.v4.addr = v4_addr)
#define RW_SET_IPV6_ADDR(addr_p, v6_addr)  do { \
    uint32_t *__v6_addr = (uint32_t*)v6_addr;               \
    (addr_p)->ip_v = RW_IPV6;                               \
    (addr_p)->u.v6.addr[0] = (__v6_addr[0]);           \
    (addr_p)->u.v6.addr[1] = (__v6_addr[1]);           \
    (addr_p)->u.v6.addr[2] = (__v6_addr[2]);           \
    (addr_p)->u.v6.addr[3] = (__v6_addr[3]);           \
  }while(0)
#define RW_IP_ADDR(_addr_) (&(_addr_)->u)

// rw_mac_addr_t RELATED
typedef struct rw_mac_addr_s {
  unsigned char addr[6];
  unsigned char pad[2];
} rw_mac_addr_t;

int  rw_c_type_helper_rw_mac_addr_t_set_from_str_impl(void *p, const char *str, size_t str_len);
int rw_c_type_helper_rw_mac_addr_t_get_str_from_impl(char* str, const void *p);

extern const struct ProtobufCCTypeDescriptor rw_mac_addr_t_helper;
#define rw_mac_addr_t_STATIC_INIT {}

#define RW_MAC_ADDRSTRLEN (18)


#define RW_PREFIX_ADDR(_addr_p) &((_addr_p)->u)
#define RW_IS_PREFIX_IPV4(_addr_) (((_addr_)->ip_v == RW_IPV4)? TRUE:FALSE)
#define RW_IS_PREFIX_IPV6(_addr_) (((_addr_)->ip_v == RW_IPV6)? TRUE:FALSE)
#define RW_IP_PREFIX_FAMILY(addr_p)  ((addr_p)->ip_v)
#define RW_SET_PREFIX_IPV4_ADDR(addr_p, v4_addr)  ((addr_p)->ip_v = RW_IPV4, (addr_p)->u.v4.addr = v4_addr)
#define RW_SET_PREFIX_IPV6_ADDR(addr_p, v6_addr)  do { \
    uint32_t *__v6_addr = (uint32_t*)v6_addr;           \
    (addr_p)->ip_v = RW_IPV6;                               \
    (addr_p)->u.v6.addr[0] = (__v6_addr[0]);           \
    (addr_p)->u.v6.addr[1] = (__v6_addr[1]);                \
    (addr_p)->u.v6.addr[2] = (__v6_addr[2]);           \
    (addr_p)->u.v6.addr[3] = (__v6_addr[3]);      \
  }while(0)

typedef struct rw_ip_prefix_s
{
  rw_ip_version_t ip_v;
  union 
  {
    rw_ipV6_addr_t   v6;
    rw_ipV4_addr_t   v4;
    uint8_t         data;
  } u;
  uint8_t         masklen;
}rw_ip_prefix_t;

int rw_c_type_helper_rw_ip_prefix_t_set_from_str_impl(void *p, const char *str, size_t str_len);
int rw_c_type_helper_rw_ip_prefix_t_get_str_from_impl(char* str, const void *p);

extern const struct ProtobufCCTypeDescriptor rw_ip_prefix_t_helper;
#define rw_ip_prefix_t_STATIC_INIT {RW_IPV6}

#define RW_PREFIX_ADDRSTRLEN (INET6_ADDRSTRLEN+4)

extern const struct ProtobufCCTypeDescriptor rw_ipV4_prefix_t_helper;
#define rw_ipV4_prefix_t_STATIC_INIT {}
#define RW_IPV4_PREFIX_ADDRSTRLEN (INET_ADDRSTRLEN+4)

extern const struct ProtobufCCTypeDescriptor rw_ipV6_prefix_t_helper;
#define rw_ipV6_prefix_t_STATIC_INIT {}
#define RW_IPV6_PREFIX_ADDRSTRLEN (INET6_ADDRSTRLEN+4)

typedef struct rw_call_id_s 
{
  uint64_t callid;
  uint64_t groupid;
  time_t   call_arrived_time;
  uint32_t serial_no;
} rw_call_id_t;

/* Always ensure that any changes made to this sctructure should also be made in
 * rwtcpdump_pkt_info_s structure defined in tcpdum directory netdissect.h file
 * */
typedef struct rw_pkt_info_s
{
  uint8_t packet_direction;
  uint8_t packet_type;
  uint16_t sport;
  uint16_t dport;
  uint64_t ip_header;
  uint32_t fragment;
  ProtobufCBinaryData packet_data;
} rw_pkt_info_t;

extern const struct ProtobufCCTypeDescriptor rw_call_id_t_helper;
#define rw_call_id_t_STATIC_INIT {}

#define RW_CALL_ID_STRLEN (20+20+10+1+1+1)

__END_DECLS

#endif /*  RW_TYPES_H__ */
