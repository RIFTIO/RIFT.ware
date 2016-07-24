
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_types.c
 * @author Rajesh Velandy(rajesh.velandy@riftio.com)
 * @date 05/01/2014
 * @brief Implementation routines for RW defined types
 * 
 * @details 
 *
 */

#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "rwtypes.h"
#include <inttypes.h>
#include <rw_ip.h>
#include "rwlib.h"

// IMPLEMENTATION FOR rw_ip_addr_t

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_ip_addr_t_set_from_str_impl(void *p, const char *str, size_t str_len)
{
  rw_ip_addr_t *ip_addr = (rw_ip_addr_t*)p;
  
  RW_ASSERT(ip_addr != NULL);
  RW_ASSERT(str != NULL);

  memset(ip_addr, 0, sizeof(*ip_addr));

  // Check if this is an IPv6 address
  if (strchr(str, ':')) {
    struct in6_addr in6;
    int i;
    ip_addr->ip_v = RW_IPV6;
    
    if (inet_pton(AF_INET6, str, &(in6)) <= 0) {
      return -1;
    }
    for (i = 0; i < 4; i++){
      ip_addr->u.v6.addr[i] = ntohl(in6.s6_addr32[i]);
    }
  } else {
    struct in_addr in;
    // Assume IPv4 address
    ip_addr->ip_v = RW_IPV4;
    if (inet_aton(str, (&in)) <= 0){
      return -1;
    }
    ip_addr->u.v4.addr = ntohl(in.s_addr);
  }
  return(0);
}

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_ip_addr_t_get_str_from_impl(char* str, const void *p) 
{
  rw_ip_addr_t *ip_addr = (rw_ip_addr_t*)p;

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
  return(0);
}

static size_t rw_ip_addr_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_addr_t* ip_addr = (const rw_ip_addr_t*)void_member;
  char str[INET6_ADDRSTRLEN*2];
  rw_ip_addr_t_get_str(str, ip_addr);
  return strlen(str);
}

static size_t rw_ip_addr_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ip_addr_t* ip_addr = (const rw_ip_addr_t*)void_member;
  char str[INET6_ADDRSTRLEN*2];
  rw_ip_addr_t_get_str(str, ip_addr);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ip_addr_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ip_addr_t* ip_addr = (rw_ip_addr_t*)void_member;

  char str[INET6_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;

  memset(ip_addr, 0, sizeof(*ip_addr));

  // Check if this is an IPv6 address
  if (strchr(str, ':')) {
    struct in6_addr in6;
    int i;
    ip_addr->ip_v = RW_IPV6;

    if (inet_pton(AF_INET6, str, &(in6)) <= 0) {
      return FALSE;
    }
    for (i = 0; i < 4; i++){
      ip_addr->u.v6.addr[i] = ntohl(in6.s6_addr32[i]);
    }
  } else {
    struct in_addr in;
    // Assume IPv4 address
    ip_addr->ip_v = RW_IPV4;
    if (inet_aton(str, (&in)) <= 0){
      return FALSE;
    }
    ip_addr->u.v4.addr = ntohl(in.s_addr);
  }
  return TRUE;
}

static protobuf_c_boolean rw_ip_addr_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_addr_t* ip_addr = (const rw_ip_addr_t*)void_member;
  if (*str_size < INET6_ADDRSTRLEN) {
    return FALSE;
  }
  rw_ip_addr_t_get_str(str, ip_addr);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ip_addr_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ip_addr_t_unpack(instance,
                             ctypedesc,
                             fielddesc,
                             str_len,
                             (const uint8_t*)str,
                             maybe_clear,
                             void_member);
}

static protobuf_c_boolean rw_ip_addr_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_member);
  rw_ip_addr_t* ip_addr = (rw_ip_addr_t*)void_member;
  memset(ip_addr, 0, sizeof(*ip_addr));
  ip_addr->ip_v = RW_IPV6;
  return TRUE;
}

static int rw_ip_addr_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_addr_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ip_addr_t* a = (const rw_ip_addr_t*)void_a;
  const rw_ip_addr_t* b = (const rw_ip_addr_t*)void_b;
  return rw_ip_addr_cmp(a, b);
}

const ProtobufCCTypeDescriptor rw_ip_addr_t_helper = {
  &rw_ip_addr_t_get_packed_size,
  &rw_ip_addr_t_pack,
  &rw_ip_addr_t_unpack,
  &rw_ip_addr_t_to_string,
  &rw_ip_addr_t_from_string,
  &rw_ip_addr_t_init_usebody,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ip_addr_t_compare,
};


//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_ipV4_addr_t

static size_t rw_ipV4_addr_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ipV4_addr_t* ipv4_addr = (const rw_ipV4_addr_t*)void_member;

  char str[INET_ADDRSTRLEN*2];
  struct in_addr in;
  in.s_addr = htonl(ipv4_addr->addr);
  inet_ntop(AF_INET, &in, str, INET_ADDRSTRLEN);
  return strlen(str);
}

static size_t rw_ipV4_addr_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ipV4_addr_t* ipv4_addr = (const rw_ipV4_addr_t*)void_member;

  char str[INET_ADDRSTRLEN*2];
  struct in_addr in;
  in.s_addr = htonl(ipv4_addr->addr);
  inet_ntop(AF_INET, &(in), str, INET_ADDRSTRLEN);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ipV4_addr_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ipV4_addr_t* ipv4_addr = (rw_ipV4_addr_t*)void_member;

  char str[INET_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(ipv4_addr, 0, sizeof(*ipv4_addr));

  struct in_addr in_addr;
  if (inet_aton(str, &in_addr) <= 0){
    return FALSE;
  }
  ipv4_addr->addr = ntohl(in_addr.s_addr);

  return TRUE;
}

static protobuf_c_boolean rw_ipV4_addr_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ipV4_addr_t* ipv4_addr = (const rw_ipV4_addr_t*)void_member;
  if (*str_size < INET_ADDRSTRLEN) {
    return FALSE;
  }

  struct in_addr in_addr;
  in_addr.s_addr = htonl(ipv4_addr->addr);
  inet_ntop(AF_INET, &in_addr, str, INET_ADDRSTRLEN);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ipV4_addr_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ipV4_addr_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static int rw_ipV4_addr_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_addr_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ipV4_addr_t* a = (const rw_ipV4_addr_t*)void_a;
  const rw_ipV4_addr_t* b = (const rw_ipV4_addr_t*)void_b;
  return (int)a->addr - (int)b->addr;
}

const ProtobufCCTypeDescriptor rw_ipV4_addr_t_helper = {
  &rw_ipV4_addr_t_get_packed_size,
  &rw_ipV4_addr_t_pack,
  &rw_ipV4_addr_t_unpack,
  &rw_ipV4_addr_t_to_string,
  &rw_ipV4_addr_t_from_string,
  &protobuf_c_ctype_init_memset,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ipV4_addr_t_compare,
};

//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_ipV6_addr_t

static size_t rw_ipV6_addr_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ipV6_addr_t* ipv6_addr = (const rw_ipV6_addr_t*)void_member;

  char str[INET6_ADDRSTRLEN*2];
  struct in6_addr in6;
  int i;
  for (i = 0; i < 4; i++){
    in6.s6_addr32[i] = htonl(ipv6_addr->addr[i]);
  }
  inet_ntop(AF_INET6, &(in6), str, INET6_ADDRSTRLEN);
  return strlen(str);
}

static size_t rw_ipV6_addr_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ipV6_addr_t* ipv6_addr = (const rw_ipV6_addr_t*)void_member;

  char str[INET6_ADDRSTRLEN*2];
  struct in6_addr in6;
  int i;
  for (i = 0; i < 4; i++){
    in6.s6_addr32[i] = htonl(ipv6_addr->addr[i]);
  }
  inet_ntop(AF_INET6, &(in6), str, INET6_ADDRSTRLEN);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ipV6_addr_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ipV6_addr_t* ipv6_addr = (rw_ipV6_addr_t*)void_member;

  char str[INET6_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(ipv6_addr, 0, sizeof(*ipv6_addr));

  struct in6_addr in6;
  int i;

  if (inet_pton(AF_INET6, str, &(in6)) <= 0) {
    return FALSE;
  }
  for (i = 0; i < 4; i++){
    ipv6_addr->addr[i] = ntohl(in6.s6_addr32[i]);
  }

  return TRUE;
}

static protobuf_c_boolean rw_ipV6_addr_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_ipV6_addr_t* ipv6_addr = (const rw_ipV6_addr_t*)void_member;
  if (*str_size < INET6_ADDRSTRLEN) {
    return FALSE;
  }

  struct in6_addr in6;
  int i;
  for (i = 0; i < 4; i++){
    in6.s6_addr32[i] = htonl(ipv6_addr->addr[i]);
  }
  inet_ntop(AF_INET6, &(in6), str, INET6_ADDRSTRLEN);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ipV6_addr_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ipV6_addr_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static int rw_ipV6_addr_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_addr_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ipV6_addr_t* a = (const rw_ipV6_addr_t*)void_a;
  const rw_ipV6_addr_t* b = (const rw_ipV6_addr_t*)void_b;
  return memcmp(a, b, sizeof(rw_ipV6_addr_t));
}

const ProtobufCCTypeDescriptor rw_ipV6_addr_t_helper = {
  &rw_ipV6_addr_t_get_packed_size,
  &rw_ipV6_addr_t_pack,
  &rw_ipV6_addr_t_unpack,
  &rw_ipV6_addr_t_to_string,
  &rw_ipV6_addr_t_from_string,
  &protobuf_c_ctype_init_memset,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ipV6_addr_t_compare,
};



//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_mac_addr_t
/*
  This function accepts MAC address in string format and returns in bytes. 
  it relies on size of byAddress and it must be greater than 6  bytes.
  If MAC address string is not in valid format, it returns NULL.

  NOTE: tolower function is used and it is locale dependent.
*/

unsigned char* ConverMacAddressStringIntoByte(char *pszMACAddress,
                                              unsigned char* pbyAddress,
                                              const char cSep) //Bytes separator in MAC address string like 00-aa-bb-cc-dd-ee
{
  int iConunter;
  for (iConunter = 0; iConunter < 6; ++iConunter)
  {
    unsigned int iNumber = 0;
    char ch;

    //Convert letter into lower case.
    ch = tolower (*pszMACAddress++);

    if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
    {
      return NULL;
    }

    //Convert into number. 
    //       a. If character is digit then ch - '0'
    //  b. else (ch - 'a' + 10) it is done 
    //  because addition of 10 takes correct value.
    iNumber = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
    ch = tolower (*pszMACAddress);

    if ((iConunter < 5 && ch != cSep) || 
      (iConunter == 5 && ch != '\0' && !isspace (ch)))
    {
      ++pszMACAddress;

      if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
      {
        return NULL;
      }

      iNumber <<= 4;
      iNumber += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
      ch = *pszMACAddress;

      if (iConunter < 5 && ch != cSep)
      {
        return NULL;
      }
    }
    /* Store result.  */
    pbyAddress[iConunter] = (unsigned char) iNumber;
    /* Skip cSep.  */
    ++pszMACAddress;
  }
  return pbyAddress;
}

/*
  This function converts Mac Address in Bytes to String format.
  It does not validate any string size and pointers.

  It returns MAC address in string format.
*/

char *ConvertMacAddressInBytesToString(char *pszMACAddress,
                                       unsigned char *pbyMacAddressInBytes,
                                       const char cSep) //Bytes separator in MAC address string like 00-aa-bb-cc-dd-ee
{
  sprintf(pszMACAddress, "%02x%c%02x%c%02x%c%02x%c%02x%c%02x", 
            pbyMacAddressInBytes[0] & 0xff,
            cSep,
            pbyMacAddressInBytes[1]& 0xff, 
            cSep,
            pbyMacAddressInBytes[2]& 0xff, 
            cSep,
            pbyMacAddressInBytes[3]& 0xff, 
            cSep,
            pbyMacAddressInBytes[4]& 0xff, 
            cSep,
            pbyMacAddressInBytes[5]& 0xff);

  return pszMACAddress;
}

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_mac_addr_t_set_from_str_impl(void *p, const char *str, size_t str_len)
{
  rw_mac_addr_t *mac_addr = (rw_mac_addr_t*)p;
  RW_ASSERT(mac_addr != NULL);
  RW_ASSERT(str != NULL);

  memset(mac_addr, 0, sizeof(*mac_addr));

  // Assume MAC-address in the format 00-aa-bb-cc-dd-ee
  ConverMacAddressStringIntoByte((char*)str,
                                 (unsigned char*)mac_addr->addr,
                                 ':'); //Bytes separator in MAC address string like 00-aa-bb-cc-dd-ee
  return(0);
}

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_mac_addr_t_get_str_from_impl(char* str, const void *p) 
{
  rw_mac_addr_t *mac_addr = (rw_mac_addr_t*)p;

  RW_ASSERT(mac_addr);

  ConvertMacAddressInBytesToString(str,
                                   mac_addr->addr,
                                   ':'); //Bytes separator in MAC address string like 00-aa-bb-cc-dd-ee

  return(0);
}

static size_t rw_mac_addr_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_mac_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_mac_addr_t* mac_addr = (const rw_mac_addr_t*)void_member;
  char str[RW_MAC_ADDRSTRLEN*2];
  rw_mac_addr_t_get_str(str, mac_addr);
  return strlen(str);
}

static size_t rw_mac_addr_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_mac_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_mac_addr_t* mac_addr = (const rw_mac_addr_t*)void_member;
  char str[RW_MAC_ADDRSTRLEN*2];
  rw_mac_addr_t_get_str(str, mac_addr);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_mac_addr_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_mac_addr_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_mac_addr_t* mac_addr = (rw_mac_addr_t*)void_member;

  char str[RW_MAC_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;

  memset(mac_addr, 0, sizeof(*mac_addr));

  unsigned char* status
    = ConverMacAddressStringIntoByte(str, (unsigned char*)mac_addr->addr, ':');
  return status != NULL;
}

static protobuf_c_boolean rw_mac_addr_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_mac_addr_t_helper);
  RW_ASSERT(void_member);
  const rw_mac_addr_t* mac_addr = (const rw_mac_addr_t*)void_member;
  if (*str_size < RW_MAC_ADDRSTRLEN) {
    return FALSE;
  }
  rw_mac_addr_t_get_str(str, mac_addr);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_mac_addr_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_mac_addr_t_unpack(instance,
                              ctypedesc,
                              fielddesc,
                              str_len,
                              (const uint8_t*)str,
                              maybe_clear,
                              void_member);
}

const ProtobufCCTypeDescriptor rw_mac_addr_t_helper = {
  &rw_mac_addr_t_get_packed_size,
  &rw_mac_addr_t_pack,
  &rw_mac_addr_t_unpack,
  &rw_mac_addr_t_to_string,
  &rw_mac_addr_t_from_string,
  &protobuf_c_ctype_init_memset,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &protobuf_c_ctype_compare_memcmp,
};



//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_ip_prefix_t

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_ip_prefix_t_set_from_str_impl(void *p, const char *str, size_t str_len)
{
  rw_ip_prefix_t *ip_addr = (rw_ip_prefix_t*)p;
  char *slash;
  uint8_t res;
  char tmp_array[128];
  
  RW_ASSERT(ip_addr != NULL);
  RW_ASSERT(str != NULL);

  memset(ip_addr, 0, sizeof(*ip_addr));

  strncpy(tmp_array, str, sizeof(tmp_array));
  tmp_array[sizeof(tmp_array) - 1] = '\0';

  slash = strchr(tmp_array, '/');
  if (slash){
    *slash = 0;
  }
  // Check if this is an IPv6 address
  if (strchr(tmp_array, ':')) {
    struct in6_addr in6;
    int i;
    ip_addr->ip_v = RW_IPV6;
    
    if (inet_pton(AF_INET6, tmp_array, &(in6)) <= 0) {
      return -1;
    }
    for (i = 0; i < 4; i++){
      ip_addr->u.v6.addr[i] = ntohl(in6.s6_addr32[i]);
    }
  } else {
    struct in_addr in;
    // Assume IPv4 address
    ip_addr->ip_v = RW_IPV4;
    if (inet_aton(tmp_array, (&in)) <= 0){
      return -1;
    }
    ip_addr->u.v4.addr = ntohl(in.s_addr);
  }
  if (slash){
    res = strtoul(slash+1, NULL,0);
    ip_addr->masklen = (uint8_t)res;
  }
  return(0);
}

// ATTN: Obsolete. Please replace uses of this with proper c-type usage
int rw_c_type_helper_rw_ip_prefix_t_get_str_from_impl(char* str, const void *p) 
{
  rw_ip_prefix_t *ip_addr = (rw_ip_prefix_t*)p;
  char maskstr[5];
  
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
  sprintf(maskstr, "/%d", ip_addr->masklen);
  strcat(str, maskstr);
  return(0);
}

static size_t rw_ip_prefix_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ip_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ip_prefaddr);
  return strlen(str);
}

static size_t rw_ip_prefix_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ip_prefix_t* ip_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ip_prefaddr);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ip_prefix_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ip_prefix_t* ip_prefaddr = (rw_ip_prefix_t*)void_member;

  char str[RW_PREFIX_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(ip_prefaddr, 0, sizeof(*ip_prefaddr));

  char* slash = strchr(str, '/');
  if (slash){
    *slash = 0;
  }
  // Check if this is an IPv6 address
  if (strchr(str, ':')) {
    struct in6_addr in6;
    int i;
    ip_prefaddr->ip_v = RW_IPV6;
    
    if (inet_pton(AF_INET6, str, &(in6)) <= 0) {
      return FALSE;
    }
    for (i = 0; i < 4; i++){
      ip_prefaddr->u.v6.addr[i] = ntohl(in6.s6_addr32[i]);
    }
  } else {
    struct in_addr in;
    // Assume IPv4 address
    ip_prefaddr->ip_v = RW_IPV4;
    if (inet_aton(str, (&in)) <= 0){
      return FALSE;
    }
    ip_prefaddr->u.v4.addr = ntohl(in.s_addr);
  }
  if (slash){
    char *end = NULL;
    unsigned long res = strtoul(slash+1, &end, 0);
    if (end && *end != '\0') {
      return FALSE;
    }
    ip_prefaddr->masklen = (uint8_t)res;
  }
  return TRUE;
}

static protobuf_c_boolean rw_ip_prefix_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ip_prefaddr = (const rw_ip_prefix_t*)void_member;
  if (*str_size < RW_PREFIX_ADDRSTRLEN) {
    return FALSE;
  }
  rw_ip_prefix_t_get_str(str, ip_prefaddr);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ip_prefix_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ip_prefix_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static protobuf_c_boolean rw_ip_prefix_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_member);
  rw_ip_prefix_t* ip_prefaddr = (rw_ip_prefix_t*)void_member;
  memset(ip_prefaddr, 0, sizeof(*ip_prefaddr));
  ip_prefaddr->ip_v = RW_IPV6;
  return TRUE;
}

static int rw_ip_prefix_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ip_prefix_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ip_prefix_t* a = (const rw_ip_prefix_t*)void_a;
  const rw_ip_prefix_t* b = (const rw_ip_prefix_t*)void_b;
  return rw_ip_prefix_cmp(a, b);
}

const ProtobufCCTypeDescriptor rw_ip_prefix_t_helper = {
  &rw_ip_prefix_t_get_packed_size,
  &rw_ip_prefix_t_pack,
  &rw_ip_prefix_t_unpack,
  &rw_ip_prefix_t_to_string,
  &rw_ip_prefix_t_from_string,
  &rw_ip_prefix_t_init_usebody,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ip_prefix_t_compare,
};

//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_ipV4_prefix_t

static size_t rw_ipV4_prefix_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ipv4_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ipv4_prefaddr);
  return strlen(str);
}

static size_t rw_ipV4_prefix_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ip_prefix_t* ipv4_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ipv4_prefaddr);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ipV4_prefix_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ip_prefix_t* ipv4_prefaddr = (rw_ip_prefix_t*)void_member;

  char str[RW_PREFIX_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(ipv4_prefaddr, 0, sizeof(*ipv4_prefaddr));

  char* slash = strchr(str, '/');
  if (slash){
    *slash = 0;
  }
  struct in_addr in4;
  if (inet_aton(str, (&in4)) <= 0){
    return FALSE;
  }
  ipv4_prefaddr->ip_v = RW_IPV4;
  ipv4_prefaddr->u.v4.addr = ntohl(in4.s_addr);
  if (slash){
    char *end = NULL;
    unsigned long res = strtoul(slash+1, &end, 0);
    if (end && *end != '\0') {
      return FALSE;
    }
    ipv4_prefaddr->masklen = (uint8_t)res;
  }
  return TRUE;
}

static protobuf_c_boolean rw_ipV4_prefix_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ipv4_prefaddr = (const rw_ip_prefix_t*)void_member;
  if (*str_size < RW_IPV4_PREFIX_ADDRSTRLEN) {
    return FALSE;
  }
  rw_ip_prefix_t_get_str(str, ipv4_prefaddr); 
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ipV4_prefix_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ipV4_prefix_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static protobuf_c_boolean rw_ipV4_prefix_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_member);
  rw_ip_prefix_t* ipv4_prefaddr = (rw_ip_prefix_t*)void_member;
  memset(ipv4_prefaddr, 0, sizeof(*ipv4_prefaddr));
  ipv4_prefaddr->ip_v = RW_IPV4;
  return TRUE;
}

static int rw_ipV4_prefix_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV4_prefix_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ip_prefix_t* a = (const rw_ip_prefix_t*)void_a;
  const rw_ip_prefix_t* b = (const rw_ip_prefix_t*)void_b;
  return rw_ip_prefix_cmp(a, b);  
}

const ProtobufCCTypeDescriptor rw_ipV4_prefix_t_helper = {
  &rw_ipV4_prefix_t_get_packed_size,
  &rw_ipV4_prefix_t_pack,
  &rw_ipV4_prefix_t_unpack,
  &rw_ipV4_prefix_t_to_string,
  &rw_ipV4_prefix_t_from_string,
  &rw_ipV4_prefix_t_init_usebody,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ipV4_prefix_t_compare,
};

//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_ipV6_prefix_t

static size_t rw_ipV6_prefix_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ipv6_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ipv6_prefaddr);
  return strlen(str);
}

static size_t rw_ipV6_prefix_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_ip_prefix_t* ipv6_prefaddr = (const rw_ip_prefix_t*)void_member;
  char str[RW_PREFIX_ADDRSTRLEN*2];
  rw_ip_prefix_t_get_str(str, ipv6_prefaddr);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_ipV6_prefix_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_ip_prefix_t* ipv6_prefaddr = (rw_ip_prefix_t*)void_member;

  char str[RW_PREFIX_ADDRSTRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(ipv6_prefaddr, 0, sizeof(*ipv6_prefaddr));

  char* slash = strchr(str, '/');
  if (slash){
    *slash = 0;
  }
  struct in6_addr in6;
  int i;
  if (inet_pton(AF_INET6, str, &(in6)) <= 0){
    return FALSE;
  }
  ipv6_prefaddr->ip_v = RW_IPV6;
  for (i = 0; i < 4; i++){
    ipv6_prefaddr->u.v6.addr[i] = ntohl(in6.s6_addr32[i]);
  }
  if (slash){
    char *end = NULL;
    unsigned long res = strtoul(slash+1, &end, 0);
    if (end && *end != '\0') {
      return FALSE;
    }
    ipv6_prefaddr->masklen = (uint8_t)res;
  }
  return TRUE;
}

static protobuf_c_boolean rw_ipV6_prefix_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_member);
  const rw_ip_prefix_t* ipv6_prefaddr = (const rw_ip_prefix_t*)void_member;
  if (*str_size < RW_PREFIX_ADDRSTRLEN) {
    return FALSE;
  }
  rw_ip_prefix_t_get_str(str, ipv6_prefaddr); 
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_ipV6_prefix_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_ipV6_prefix_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static protobuf_c_boolean rw_ipV6_prefix_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_member);
  rw_ip_prefix_t* ipv6_prefaddr = (rw_ip_prefix_t*)void_member;
  memset(ipv6_prefaddr, 0, sizeof(*ipv6_prefaddr));
  ipv6_prefaddr->ip_v = RW_IPV6;
  return TRUE;
}

static int rw_ipV6_prefix_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_ipV6_prefix_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_ip_prefix_t* a = (const rw_ip_prefix_t*)void_a;
  const rw_ip_prefix_t* b = (const rw_ip_prefix_t*)void_b;
  return rw_ip_addr_cmp(a, b); 
}

const ProtobufCCTypeDescriptor rw_ipV6_prefix_t_helper = {
  &rw_ipV6_prefix_t_get_packed_size,
  &rw_ipV6_prefix_t_pack,
  &rw_ipV6_prefix_t_unpack,
  &rw_ipV6_prefix_t_to_string,
  &rw_ipV6_prefix_t_from_string,
  &rw_ipV6_prefix_t_init_usebody,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_ipV6_prefix_t_compare,
};

//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION FOR rw_call_id_t

static char* rw_call_id_t_get_str(char* str, const rw_call_id_t *call_id)
{
  RW_ASSERT(str);
  RW_ASSERT(call_id);
  sprintf(str, "%lu:%lu:%ld", call_id->groupid, call_id->callid, call_id->call_arrived_time);
  return str;
}

static size_t rw_call_id_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_call_id_t_helper);
  RW_ASSERT(void_member);
  const rw_call_id_t* call_id = (const rw_call_id_t*)void_member;
  char str[RW_CALL_ID_STRLEN*2];
  rw_call_id_t_get_str(str, call_id);
  return strlen(str);
}

static size_t rw_call_id_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_call_id_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const rw_call_id_t* call_id = (const rw_call_id_t*)void_member;
  char str[RW_CALL_ID_STRLEN*2];
  rw_call_id_t_get_str(str, call_id);
  size_t len = strlen(str);
  memcpy(out, str, len);
  return len;
}

static protobuf_c_boolean rw_call_id_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_call_id_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  rw_call_id_t* call_id = (rw_call_id_t*)void_member;

  char str[RW_CALL_ID_STRLEN*2];
  if (in_size >= sizeof(str)) {
    return FALSE;
  }
  memcpy(str, in, in_size);
  str[in_size] = 0;
  memset(call_id, 0, sizeof(*call_id));

  uint64_t t;
  int consumed = 0;
  int retval = sscanf(str, "%" SCNu64 ":%" SCNu64 ":%" SCNu64 "%n",
                      &call_id->groupid, &call_id->callid, &t, &consumed);
  if (   retval != 3
      && consumed != strlen(str)) {
    return FALSE;
  }
  call_id->call_arrived_time = (time_t)t;
  return TRUE;
}

static protobuf_c_boolean rw_call_id_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_call_id_t_helper);
  RW_ASSERT(void_member);
  const rw_call_id_t* call_id = (const rw_call_id_t*)void_member;
  if (*str_size < RW_CALL_ID_STRLEN) {
    return FALSE;
  }
  rw_call_id_t_get_str(str, call_id);
  *str_size = strlen(str);
  return TRUE;
}

static protobuf_c_boolean rw_call_id_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  /* Same as unpack */
  return rw_call_id_t_unpack(instance,
                             ctypedesc,
                             fielddesc,
                             str_len,
                             (const uint8_t*)str,
                             maybe_clear,
                             void_member);
}

static int rw_call_id_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &rw_call_id_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const rw_call_id_t* a = (const rw_call_id_t*)void_a;
  const rw_call_id_t* b = (const rw_call_id_t*)void_b;
  if (a->groupid < b->groupid) {
    return -1;
  } else if (a->groupid > b->groupid) {
    return 1;
  }

  if (a->callid < b->callid) {
    return -1;
  } else if (a->callid > b->callid) {
    return 1;
  }

  return 0;
}

const ProtobufCCTypeDescriptor rw_call_id_t_helper = {
  &rw_call_id_t_get_packed_size,
  &rw_call_id_t_pack,
  &rw_call_id_t_unpack,
  &rw_call_id_t_to_string,
  &rw_call_id_t_from_string,
  &protobuf_c_ctype_init_memset,
  NULL,
  NULL,
  &protobuf_c_ctype_deep_copy_memcpy,
  &rw_call_id_t_compare,
};

