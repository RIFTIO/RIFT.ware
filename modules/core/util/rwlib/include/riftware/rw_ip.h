
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rw_ip.h
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 05/5/2014
 * @header 
 */
#ifndef __RW_IP_H__
#define __RW_IP_H__

__BEGIN_DECLS

#include <rwlib.h>
#define RW_IP_MAX_STR_LEN 64 //same for prefix/ip

#define RW_IP_VALID(ip) (((ip)->ip_v == RW_IPV4) || ((ip)->ip_v == RW_IPV6))

#define RW_IP_VALID_NONZERO(ip) ((((ip)->ip_v == RW_IPV4) && ((ip)->u.v4.addr != 0)) || \
                                 (((ip)->ip_v == RW_IPV6) &&  \
                                  (((ip)->u.v6.addr[0] != 0) ||         \
                                   ((ip)->u.v6.addr[1] != 0) || ((ip)->u.v6.addr[2] != 0) || \
                                   ((ip)->u.v6.addr[3] != 0))))

#define RW_IP_VALID_ZERO(ip) ((((ip)->ip_v == RW_IPV4) && ((ip)->u.v4.addr == 0)) || \
                              (((ip)->ip_v == RW_IPV6) &&               \
                               (((ip)->u.v6.addr[0] == 0) &&            \
                                ((ip)->u.v6.addr[1] == 0) &&            \
                                ((ip)->u.v6.addr[2] == 0) &&            \
                                ((ip)->u.v6.addr[3] == 0))))

#define RW_PREFIX_DEFAULT(ip) ((((ip)->ip_v == RW_IPV4) && ((ip)->u.v4.addr == 0) && \
                                ((ip)->masklen == 0)) ||                \
                               (((ip)->ip_v == RW_IPV6) &&              \
                                (((ip)->u.v6.addr[0] == 0) &&           \
                                 ((ip)->u.v6.addr[1] == 0) &&           \
                                 ((ip)->u.v6.addr[2] == 0) &&           \
                                 ((ip)->u.v6.addr[3] == 0) &&           \
                                 ((ip)->masklen == 0))))
#define RW_PREFIX_MAX_MASKLEN(ip) (((ip)->ip_v == RW_IPV4)?32:(((ip)->ip_v == RW_IPV6)?128:32))

static inline void RW_IP_TO_SOCKADDR(rw_ip_addr_t *rwip,
                                     struct sockaddr *sockaddr) 
{                                                         
  int __i;
  uint32_t *__data;
  RW_ASSERT(((rwip)->ip_v == RW_IPV4) ||((rwip)->ip_v == RW_IPV6));
  (sockaddr)->sa_family = (rwip)->ip_v;
  __data = (uint32_t*)(sockaddr)->sa_data;
  if ((rwip)->ip_v == RW_IPV4){
    *__data = htonl((rwip)->u.v4.addr);
  }else{
    for (__i=0; __i < 4; __i++){
      __data[__i] = htonl((rwip)->u.v6.addr[__i]);
    }
  }
}


static inline void RW_IP_TO_SOCKADDR_PORT(rw_ip_addr_t *rwip,
                                          uint16_t port,
                                          struct sockaddr *sockaddr) 
{                                                         
  int __i;                                                
  RW_ASSERT(((rwip)->ip_v == RW_IPV4) ||((rwip)->ip_v == RW_IPV6));
  (sockaddr)->sa_family = (rwip)->ip_v;                             
  if ((rwip)->ip_v == RW_IPV4){
    ((struct sockaddr_in*)sockaddr)->sin_addr.s_addr = htonl((rwip)->u.v4.addr);
    ((struct sockaddr_in*)sockaddr)->sin_port = htons(port);
  }else{
    for (__i=0; __i < 4; __i++){
      ((struct sockaddr_in6*)sockaddr)->sin6_addr.s6_addr32[__i] = htonl((rwip)->u.v6.addr[__i]);
      ((struct sockaddr_in6*)sockaddr)->sin6_port = htons(port);
    }
  }
}

static inline void RW_IP_FROM_SOCKADDR(rw_ip_addr_t *rwip,
				       struct sockaddr *sockaddr) 
{                                                         
  int __i;                                                
  uint32_t *__data;
  RW_ASSERT(((sockaddr)->sa_family== RW_IPV4) ||((sockaddr)->sa_family == RW_IPV6));   
  (rwip)->ip_v = (sockaddr)->sa_family;
  __data = (uint32_t *)(sockaddr)->sa_data;
  if ((rwip)->ip_v == RW_IPV4 ){
    (rwip)->u.v4.addr = ntohl(*__data);
  }else{
    for (__i=0; __i < 4; __i++){
       (rwip)->u.v6.addr[__i] = ntohl(__data[__i]);
    }
  }
}



static inline void RW_PREFIX_FROM_SOCKADDR(rw_ip_prefix_t *rwip,
					   struct sockaddr *sockaddr) 
{                                                         
  int __i;                                                
  uint32_t *__data;
  RW_ASSERT(((sockaddr)->sa_family== RW_IPV4) ||((sockaddr)->sa_family == RW_IPV6));   
  (rwip)->ip_v = (sockaddr)->sa_family;
  __data = (uint32_t *)(sockaddr)->sa_data;
  if ((rwip)->ip_v == RW_IPV4){
    (rwip)->u.v4.addr = ntohl(*__data);
  }else{
    for (__i=0; __i < 4; __i++){
       (rwip)->u.v6.addr[__i] = ntohl(__data[__i]);
    }
    (rwip)->masklen = 128;
  }
}


#define RW_IP_TO_SOCKADDRIN(rwip, sockaddr)               \
  do{                                                     \
  RW_ASSERT((rwip)->ip_v == RW_IPV4);                     \
  (sockaddr)->sin_family = (rwip)->ip_v;                  \
  (sockaddr)->sin_addr.s_addr = htonl((rwip)->u.v4.addr); \
  }while(0);


#define RW_IP_FROM_SOCKADDRIN(rwip, sockaddr)                              \
  do{                                                                   \
    RW_ASSERT((sockaddr)->sin_family == RW_IPV4);                       \
    (rwip)->ip_v = (sockaddr)->sin_family;                              \
    (rwip)->u.v4.addr = ntohl((sockaddr)->sin_addr.s_addr);             \
  }while(0);


#define RW_PREFIX_TO_SOCKADDRIN(rwip, sockaddr)               \
  do{                                                         \
    RW_ASSERT((rwip)->ip_v == RW_IPV4);                       \
    (sockaddr)->sin_family = (rwip)->ip_v;                \
    (sockaddr)->sin_addr.s_addr = htonl((rwip)->u.v4.addr);     \
  }while(0);

#define RW_PREFIX_FROM_SOCKADDRIN(rwip, sockaddr)                              \
  do{                                                                   \
    RW_ASSERT((sockaddr)->sin_family == RW_IPV4);                       \
    (rwip)->ip_v = (sockaddr)->sin_family;                              \
    (rwip)->u.v4.addr = ntohl((sockaddr)->sin_addr.s_addr);             \
  }while(0);


#define RW_IP_TO_IN6ADDR(rwip, sockaddr)               \
  {                                                    \
  int __i;                                                \
  do{                                                      \
    RW_ASSERT((rwip)->ip_v == RW_IPV6);                           \
    for (__i=0; __i < 4; __i++)                                    \
      (sockaddr)->s6_addr32[__i] = htonl((rwip)->u.v6.addr[__i]);      \
  }while(0); \
  }

#define RW_IP_FROM_IN6ADDR(rwip, sockaddr)         \
  {                                             \
    int __i;                                                            \
    do{                                                                 \
      (rwip)->ip_v = RW_IPV6;                                          \
      for (__i=0; __i < 4; __i++)                                       \
        (rwip)->u.v6.addr[__i] = ntohl((sockaddr)->s6_addr32[__i]);         \
    }while(0);                                                          \
  }

#define RW_PREFIX_TO_IN6ADDR(rwip, sockaddr)    \
  {                                             \
  int __i;                                                    \
  do{                                                                   \
    for (__i=0; __i < 4; __i++)                                         \
      (sockaddr)->s6_addr32[__i] = htonl((rwip)->u.v6.addr[__i]);           \
  }while(0); \
  }

#define RW_PREFIX_FROM_IN6ADDR(rwip, sockaddr)    \
  {                                             \
  int __i;                                                              \
  do{                                                                   \
    (rwip)->ip_v = RW_IPV6;                                            \
    for (__i=0; __i < 4; __i++)                                         \
      (rwip)->u.v6.addr[__i] = ntohl((sockaddr)->s6_addr32[__i]);           \
  }while(0); \
  }


static inline int  RW_IP_ADDR_CMP(rw_ip_addr_t *a1, rw_ip_addr_t *a2)
{
  if ((a1)->ip_v != (a2)->ip_v)
    return 1;
  if ((a1)->ip_v == RW_IPV4){
    return ((a1->u.v4.addr == a2->u.v4.addr)?0:1);
  }else{
    return memcmp(&a1->u.v6, &a2->u.v6, sizeof(a1->u.v6));
  }
}


static inline void rw_ip_addr_cpy(rw_ip_addr_t *dest, rw_ip_addr_t *src)
{
  dest->ip_v = src->ip_v;
  if (src->ip_v == RW_IPV4){
    dest->u.v4.addr = src->u.v4.addr;
  }else{
    memcpy(&dest->u.v6, &src->u.v6, sizeof(src->u.v6));
  }
}

void rw_ip_prefix_apply_mask(rw_ip_prefix_t *p);
int rw_ip_prefix_match(const rw_ip_prefix_t *r, const rw_ip_prefix_t *p);
int rw_ip_prefix_match_ipv4(const rw_ip_prefix_t *r, uint32_t src);
int rw_ip_prefix_match_ipv6(const rw_ip_prefix_t *r, uint32_t *src);
int rw_ip_prefix_match_ipv6_data(const rw_ip_prefix_t *r, uint32_t *src);
int rw_ip_addr_cmp(const void *v1, const void *v2);
void rw_ip_prefix_copy(rw_ip_prefix_t *dest, const rw_ip_prefix_t *src);
void rw_ip_addr_copy(rw_ip_addr_t *dest, const rw_ip_addr_t *src);
char* rw_ip_addr_t_get_str(char* str, const rw_ip_addr_t *ip_addr);
char* rw_mac_addr_t_get_str(char* str, const rw_mac_addr_t *mac);
char* rw_ip_prefix_t_get_str(char* str, const rw_ip_prefix_t *ip_pref);
int rw_ip_prefix_cmp(const void *v1, const void *v2);
int rw_ip_addr_from_str(const char *str, rw_ip_addr_t *ip);
int rw_prefix_from_str(const char *str, rw_ip_prefix_t *ip);
void rw_mac_addr_copy(rw_mac_addr_t *dest, const rw_mac_addr_t *src);
typedef enum rw_ip_addr_type{
  RW_IP_ADDR_TYPE_UNICAST,
  RW_IP_ADDR_TYPE_LINKLOCAL,
  RW_IP_ADDR_TYPE_MULTICAST,
  RW_IP_ADDR_TYPE_LOOPBACK,
  RW_IP_ADDR_TYPE_ANY,
  RW_IP_ADDR_TYPE_MAPPED,
  RW_IP_ADDR_TYPE_BROADCAST
}rw_ip_addr_type_t;


rw_ip_addr_type_t rw_ip_addr_type(rw_ip_addr_t *rwip);
rw_ip_addr_type_t rw_ipv6_addr_type(uint32_t *addr);
rw_ip_addr_type_t rw_ipv4_addr_type(uint32_t addr);

typedef union sockaddr_rw{
  struct sockaddr_in  in;
  struct sockaddr_in6 in6;
}sockaddr_rw_t;

#define RW_SOCKADDR_LEN(sockaddr)  (((sockaddr)->in.sin_family == RW_IPV4)? sizeof((sockaddr)->in):sizeof((sockaddr)->in6))


#define RW_PREFIX_IS_HOST(ip) (((ip)->ip_v == RW_IPV4)? (((ip)->masklen == 32)? 1:0):(((ip)->masklen == 128)?1:0))

#define RW_PREFIX_SET_HOST(ip) if ((ip)->ip_v == RW_IPV4)  ((ip)->masklen = 32); else ((ip)->masklen = 128);
__END_DECLS

#endif /* __RW_IP_H__ */
