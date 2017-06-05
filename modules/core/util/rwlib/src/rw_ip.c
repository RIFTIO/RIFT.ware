
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

/**
 * @file rw_ip.c
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 05/01/2014
 * @brief IP routines for compare/prefix match...
 * 
 * @details 
 *
 */
#include <rwtypes.h>
#include <rw_ip.h>

char* rw_ip_addr_t_get_str(char* str, const rw_ip_addr_t *ip_addr)
{
  RW_ASSERT(ip_addr);
  RW_ASSERT((ip_addr->ip_v == RW_IPV6) || (ip_addr->ip_v == RW_IPV4));

  if (ip_addr->ip_v == RW_IPV6) {
    struct in6_addr in6;
    int i;
    for (i = 0; i < 4; i++){
      in6.s6_addr32[i] = htonl(ip_addr->u.v6.addr[i]);
    }
    inet_ntop(AF_INET6, &(in6), str, INET6_ADDRSTRLEN);
  } else { // RW_IPV4
    struct in_addr in;
    in.s_addr = htonl(ip_addr->u.v4.addr);
    inet_ntop(AF_INET, &(in), str, INET6_ADDRSTRLEN);
  }
  return str;
}


char* rw_ip_prefix_t_get_str(char* str, const rw_ip_prefix_t *prefix)
{
  char maskstr[5];
  
  RW_ASSERT(prefix);
  RW_ASSERT((prefix->ip_v == RW_IPV6) || (prefix->ip_v == RW_IPV4));

  if (prefix->ip_v == RW_IPV6) {
    struct in6_addr in6;
    int i;
    for (i = 0; i < 4; i++){
      in6.s6_addr32[i] = htonl(prefix->u.v6.addr[i]);
    }
    inet_ntop(AF_INET6, &(in6), str, INET6_ADDRSTRLEN);
  } else { // RW_IPV4
    struct in_addr in;
    in.s_addr = htonl(prefix->u.v4.addr);
    inet_ntop(AF_INET, &(in), str, INET6_ADDRSTRLEN);
  }
  
  sprintf(maskstr, "/%d", prefix->masklen);
  strcat(str, maskstr);
  
  return str;
}

char* rw_mac_addr_t_get_str(char* str, const rw_mac_addr_t *mac)
{
  RW_ASSERT(mac);
  sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", 
          mac->addr[0] & 0xff,
          mac->addr[1]& 0xff, 
          mac->addr[2]& 0xff, 
          mac->addr[3]& 0xff, 
          mac->addr[4]& 0xff, 
          mac->addr[5]& 0xff);
  
  return str;
}


int rw_ip_addr_cmp(const void *v1, const void *v2)
{
  const rw_ip_addr_t *a1 = (const rw_ip_addr_t *)v1;
  const rw_ip_addr_t *a2 = (const rw_ip_addr_t *)v2;
  RW_ASSERT((a1->ip_v == RW_IPV4) ||
            (a1->ip_v == RW_IPV6));

  RW_ASSERT((a2->ip_v == RW_IPV4) ||
            (a2->ip_v == RW_IPV6));

  if (a1->ip_v < a2->ip_v)
    return -1;
  if (a1->ip_v > a2->ip_v)
    return 1;

  if (a1->ip_v == RW_IPV4){
    if (a1->u.v4.addr < a2->u.v4.addr)
    return -1;
  if (a1->u.v4.addr > a2->u.v4.addr)
    return 1;
  }else{
    int i;
    for (i = 0; i < 4; i++){
      if (a1->u.v6.addr[i] < a2->u.v6.addr[i])
        return -1;
      if (a1->u.v6.addr[i] > a2->u.v6.addr[i])
        return 1;
    }
  }

  return 0;
}


void rw_ip_prefix_apply_mask(rw_ip_prefix_t *p)
{
  RW_ASSERT((p->ip_v == RW_IPV4) ||
            (p->ip_v == RW_IPV6));
  
  if (p->ip_v == RW_IPV4){
    if (p->masklen)
      p->u.v4.addr = ((p->u.v4.addr >> (32-p->masklen))<<(32-p->masklen));
    else
      p->u.v4.addr = 0;
  }else{
    unsigned int index, bit;
    int i;
    
    if (p->masklen == 128)
      return;    
    index = (p->masklen /32);
    bit = p->masklen & 0x1f;
    if (bit)
      p->u.v6.addr[index] = ((p->u.v6.addr[index] >> (32-bit))<<(32-bit));
    for (i = index+1; i < 4; i++){
      p->u.v6.addr[i] = 0;
    }
  }

  return;
}

/**
 * This fucntino is called to check if the prefix (second argument) matches the a 
 * certain prefix (first agument)
 */
int
rw_ip_prefix_match (const rw_ip_prefix_t *r, const rw_ip_prefix_t *p)
{
  RW_ASSERT(r);
  RW_ASSERT(p);

  if (r->ip_v != p->ip_v){
    return 0;
  }
  
  if (r->masklen > p->masklen){
    return 0;
  }

  if (r->ip_v == RW_IPV4){
    if ((((r->u.v4.addr ^ p->u.v4.addr) >> (32 - r->masklen)) << (32 - r->masklen)) ==0)
      return 1;
  }else{
    unsigned int index, bit;
    int i;
    index = (r->masklen /32);
    bit = r->masklen & 0x1f;
    for (i = 0; i < index; i++){
      if (r->u.v6.addr[i] ^ p->u.v6.addr[i]){
        return 0;
      }
    }
    if (index == 4)
      return 1;
    if ((((r->u.v6.addr[index] ^ p->u.v6.addr[index]) >> (32 - bit)) << (32 - bit)) ==0)
      return 1;
  }
  return 0;
}

int rw_ip_prefix_match_ipv4(const rw_ip_prefix_t *r, uint32_t src)
{
  rw_ip_prefix_t p;

  p.ip_v = RW_IPV4;
  p.u.v4.addr = src;
  p.masklen = 32;
  
  return rw_ip_prefix_match(r, &p);
}

int rw_ip_prefix_match_ipv6(const rw_ip_prefix_t *r, uint32_t *src)
{
  rw_ip_prefix_t p;
  int i;

  p.ip_v = RW_IPV6;
  for (i = 0; i < 4; i++){
    p.u.v6.addr[i] = src[i];
  }
  p.masklen = 128;
  return rw_ip_prefix_match(r, &p);
}


int rw_ip_prefix_match_ipv6_data(const rw_ip_prefix_t *r, uint32_t *src)
{
  rw_ip_prefix_t p;
  int i;

  p.ip_v = RW_IPV6;
  for (i = 0; i < 4; i++){
    p.u.v6.addr[i] = ntohl(src[i]);
  }
  p.masklen = 128;
  return rw_ip_prefix_match(r, &p);
}
/**
 * This fucntino is called to copy the prefix from the src to the dest.
 * The dest is the first argument
 */
void rw_ip_prefix_copy (rw_ip_prefix_t *dest, const rw_ip_prefix_t *src)
{
  
  dest->ip_v = src->ip_v;
  RW_ASSERT((src->ip_v == RW_IPV4) ||
            (src->ip_v == RW_IPV6));
  if (src->ip_v == RW_IPV4)
    dest->u.v4 = src->u.v4;
  else if (src->ip_v == RW_IPV6){
    dest->u.v6 = src->u.v6;;
  }

  dest->masklen = src->masklen;
  return;
}


/**
 * This fucntino is called to copy the prefix from the src to the dest.
 * The dest is the first argument
 */
void rw_ip_addr_copy (rw_ip_addr_t *dest, const rw_ip_addr_t *src)
{
  
  dest->ip_v = src->ip_v;
  RW_ASSERT((src->ip_v == RW_IPV4) ||
            (src->ip_v == RW_IPV6));
  if (src->ip_v == RW_IPV4)
    dest->u.v4 = src->u.v4;
  else if (src->ip_v == RW_IPV6){
    dest->u.v6 = src->u.v6;;
  }
  return;
}


int rw_ip_prefix_cmp(const void *v1, const void *v2)
{
  const rw_ip_prefix_t *a1 = (const rw_ip_prefix_t *)v1;
  const rw_ip_prefix_t *a2 = (const rw_ip_prefix_t *)v2;
  RW_ASSERT((a1->ip_v == RW_IPV4) ||
            (a1->ip_v == RW_IPV6));

  RW_ASSERT((a2->ip_v == RW_IPV4) ||
            (a2->ip_v == RW_IPV6));


  if (a1->ip_v < a2->ip_v)
    return -1;
  if (a1->ip_v > a2->ip_v)
    return 1;

  if (a1->masklen < a2->masklen)
    return -1;
  if (a1->masklen > a2->masklen)
    return 1;

  if (a1->ip_v == RW_IPV4){
    if (a1->u.v4.addr < a2->u.v4.addr)
    return -1;
  if (a1->u.v4.addr > a2->u.v4.addr)
    return 1;
  }else{
    int i;
    for (i = 0; i < 4; i++){
      if (a1->u.v6.addr[i] < a2->u.v6.addr[i])
        return -1;
      if (a1->u.v6.addr[i] > a2->u.v6.addr[i])
        return 1;
    }
  }

  return 0;
}


int
rw_ip_addr_from_str(const char *str, rw_ip_addr_t *ip)
{
  return rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(ip, str, 0);
}


int rw_prefix_from_str(const char *str, rw_ip_prefix_t *ip)
{
  return rw_c_type_helper_rw_ip_prefix_t_set_from_str_impl(ip, str, 0);
}


rw_ip_addr_type_t rw_ipv4_addr_type(uint32_t addr)
{
  rw_ip_addr_type_t type = RW_IP_ADDR_TYPE_UNICAST;

  if (addr == 0){
    type = RW_IP_ADDR_TYPE_ANY;
  }else if ((addr & 0xf0000000) == 0xe0000000){
    type = RW_IP_ADDR_TYPE_MULTICAST;
  }else if (addr == 0xffffffff){
    type = RW_IP_ADDR_TYPE_BROADCAST;
  }else if ((addr & 0xff000000) == 0x7f000000){
    type = RW_IP_ADDR_TYPE_LOOPBACK;
  }
  return type;
}

rw_ip_addr_type_t rw_ipv6_addr_type(uint32_t *addr)
{
  rw_ip_addr_type_t type = RW_IP_ADDR_TYPE_UNICAST;
  
  if ((addr[0] & 0xE0000000) !=
      0x00000000 &&
      (addr[0] & 0xE0000000) !=
      0xE0000000){
    type = RW_IP_ADDR_TYPE_UNICAST;
  }else if ((addr[0] & 0xFF000000) ==
            0xFF000000) {
    type =  RW_IP_ADDR_TYPE_MULTICAST;
  }else if ((addr[0] & 0xFFC00000) ==
            0xFE800000){
    type = RW_IP_ADDR_TYPE_LINKLOCAL;
  }else if ((addr[0] & 0xFFC00000) ==
            0xFEC00000){
    type = RW_IP_ADDR_TYPE_UNICAST;//SITELOCAL
  }else if ((addr[0] & 0xFE000000) ==
            0xFC000000){
    type = RW_IP_ADDR_TYPE_UNICAST;
  } if ((addr[0] | addr[1]) == 0) {
    if (addr[2] == 0) {
      if (addr[3] == 0){
        type = RW_IP_ADDR_TYPE_ANY;
      }else if (addr[3] == 0x00000001){
        type = RW_IP_ADDR_TYPE_LOOPBACK;
      }
      type = RW_IP_ADDR_TYPE_UNICAST;
    } else if (addr[2] == 0x0000ffff){
      type = RW_IP_ADDR_TYPE_MAPPED;
    }
    type = RW_IP_ADDR_TYPE_UNICAST;
  }
    
  return type;
}

rw_ip_addr_type_t rw_ip_addr_type(rw_ip_addr_t *rwip)
{
  rw_ip_addr_type_t type = RW_IP_ADDR_TYPE_UNICAST;

  switch(RW_IP_ADDR_FAMILY(rwip)){
    case RW_IPV4:
      type = rw_ipv4_addr_type(rwip->u.v4.addr);
      break;
    case RW_IPV6:
      type = rw_ipv6_addr_type(&rwip->u.v6.addr[0]);
      break;
    default:
      RW_CRASH();
      break;
  }
  
  return type;
}

void rw_mac_addr_copy(rw_mac_addr_t *dest, const rw_mac_addr_t *src)
{
  RW_ASSERT(dest);
  RW_ASSERT(src);
  memcpy(dest, src, sizeof(*src));
}
