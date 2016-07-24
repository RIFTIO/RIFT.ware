
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rw_netlink.c
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @Fastpath netlink functionality
 *
 * @details
 */
#include <stdio.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <rwlib.h>
#include <rw_ip.h>
#include <rwnetlink.h>
#include <sys/inotify.h>
#include "netlink/netlink.h"
#include "netlink/msg.h"
#include "netlink/cache.h"
#include "netlink/route/link.h"
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_tunnel.h>
#include <linux/route.h>
#include <linux/ipv6_route.h>
#include <linux/veth.h>
#include <linux/if_arp.h>
#include <linux/if_tun.h>
#include <linux/ipv6.h>
#include <linux/xfrm.h>

static int rw_netlink_route_recv_valid(struct nl_msg *msg, void *arg);
static int rw_netlink_netfilter_recv_valid(struct nl_msg *msg, void *arg);
static int rw_netlink_generic_recv_valid(struct nl_msg *msg, void *arg);
static int rw_netlink_xfrm_recv_valid(struct nl_msg *msg, void *arg);
static int rw_netlink_route_recv_validmsg(rw_netlink_handle_proto_t *proto_handle,
                                          struct nlmsghdr *nlhdr, void *arg);
static int rw_netlink_xfrm_recv_validmsg(rw_netlink_handle_proto_t *proto_handle,
                                         struct nlmsghdr *nlhdr, void *arg);

typedef struct rw_netlink_proto_cmds{
  uint32_t cmd;
  uint32_t group;
}rw_netlink_proto_cmds_t;

typedef struct rw_netlink_proto_defs{
  uint32_t                 proto;
  rw_netlink_proto_cmds_t  cmds[RW_NETLINK_MAX_CALLBACKS];
  uint32_t                 cmd_base;
  netlink_filter_f         filter;
  uint32_t                 allgroups;
}rw_netlink_proto_defs_t;

#define nl_mgrp(group) (group)?(1 << (group - 1)):0


rw_netlink_proto_defs_t rw_netlink_proto_defs[] =
{
  [NETLINK_ROUTE].proto = NETLINK_ROUTE,
  [NETLINK_ROUTE].cmd_base = RTM_BASE,
  [NETLINK_ROUTE].filter = rw_netlink_route_recv_validmsg,
  [NETLINK_ROUTE].allgroups =  (RTMGRP_LINK | RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR |
                                RTMGRP_NEIGH| RTMGRP_IPV6_ROUTE | RTMGRP_IPV6_IFADDR),
  [NETLINK_ROUTE].cmds = {
    [RTM_NEWLINK-RTM_BASE] = {RTM_NEWLINK, nl_mgrp(RTNLGRP_LINK)},
    [RTM_DELLINK-RTM_BASE] = {RTM_DELLINK, nl_mgrp(RTNLGRP_LINK)},
    [RTM_GETLINK-RTM_BASE] = {RTM_GETLINK, nl_mgrp(RTNLGRP_LINK)},
    [RTM_SETLINK-RTM_BASE] = {RTM_SETLINK, nl_mgrp(RTNLGRP_LINK)},
    [RTM_NEWROUTE-RTM_BASE] = {RTM_NEWROUTE, nl_mgrp(RTNLGRP_IPV4_ROUTE) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_DELROUTE-RTM_BASE] = {RTM_DELROUTE, nl_mgrp(RTNLGRP_IPV4_ROUTE) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_GETROUTE-RTM_BASE] = {RTM_GETROUTE, nl_mgrp(RTNLGRP_IPV4_ROUTE) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_NEWADDR-RTM_BASE] = {RTM_NEWADDR, nl_mgrp(RTNLGRP_IPV4_IFADDR) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_DELADDR-RTM_BASE] = {RTM_DELADDR, nl_mgrp(RTNLGRP_IPV4_IFADDR) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_GETADDR-RTM_BASE] = {RTM_GETADDR, nl_mgrp(RTNLGRP_IPV4_IFADDR) | nl_mgrp(RTNLGRP_IPV6_ROUTE)},
    [RTM_NEWNEIGH-RTM_BASE] = {RTM_NEWNEIGH, nl_mgrp(RTNLGRP_NEIGH)},
    [RTM_DELNEIGH-RTM_BASE] = {RTM_DELNEIGH, nl_mgrp(RTNLGRP_NEIGH)},
    [RTM_GETNEIGH-RTM_BASE] = {RTM_GETNEIGH, nl_mgrp(RTNLGRP_NEIGH)},
  },
  [NETLINK_XFRM].proto = NETLINK_XFRM,
  [NETLINK_XFRM].cmd_base = XFRM_MSG_BASE,
  [NETLINK_XFRM].filter = rw_netlink_xfrm_recv_validmsg,
  [NETLINK_XFRM].allgroups =  ( nl_mgrp(XFRMNLGRP_SA) | nl_mgrp(XFRMNLGRP_POLICY)),
  [NETLINK_XFRM].cmds = {
    [XFRM_MSG_NEWSA-XFRM_MSG_BASE] = {XFRM_MSG_NEWSA, nl_mgrp(XFRMNLGRP_SA)},
    [XFRM_MSG_DELSA-XFRM_MSG_BASE] = {XFRM_MSG_DELSA, nl_mgrp(XFRMNLGRP_SA)},
    [XFRM_MSG_UPDSA-XFRM_MSG_BASE] = {XFRM_MSG_UPDSA, nl_mgrp(XFRMNLGRP_SA)},
    [XFRM_MSG_NEWPOLICY-XFRM_MSG_BASE] = {XFRM_MSG_NEWPOLICY, nl_mgrp(XFRMNLGRP_POLICY)},
    [XFRM_MSG_UPDPOLICY-XFRM_MSG_BASE] = {XFRM_MSG_UPDPOLICY, nl_mgrp(XFRMNLGRP_POLICY)},
    [XFRM_MSG_DELPOLICY-XFRM_MSG_BASE] = {XFRM_MSG_DELPOLICY, nl_mgrp(XFRMNLGRP_POLICY)},
  }
  
};

static inline void* nlmsg_end_ptr(struct nlmsghdr *nlhdr)
{
  return (void *) (((char*)(nlhdr)) + NLMSG_ALIGN((nlhdr)->nlmsg_len));
}

/**
 * This function is called to add a new attribute to a netlink message
 * @param[in]  n  -  netlink message pointer to which we need to add attribute
 * @param[in]  maxlen  - maximum buffer space for the netlink message
 * @param[out] type - type of attribute to add
 * @param[in ] data - data of the attribute
 * @param[in ] len - length of the attribute
 *
 * @returns 0  if success
 * @returns -1 if failure
 */
static int rw_netlink_add_attr(struct nlmsghdr *n, uint32_t maxlen, int type,
                               const void *data, int len)
{
  int total_attr_len = RTA_LENGTH(len);
  struct rtattr *rta;
    
  if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(total_attr_len) > maxlen) {
    return RW_STATUS_FAILURE;
  }
  rta = (struct rtattr *)nlmsg_end_ptr(n);
  rta->rta_type = type;
  rta->rta_len = total_attr_len;
  memcpy(RTA_DATA(rta), data, len);
  n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(total_attr_len);
  
  return RW_STATUS_SUCCESS;
}


static inline void* rw_netlink_add_base(struct nlmsghdr *n, int size, int pad)
{
  int totallen;
  void *ifa = nlmsg_end_ptr(n);

  totallen = pad ? ((size + (pad - 1)) & ~(pad - 1)) : size;

  /*check here akki*/
  
  n->nlmsg_len += totallen;
  
  memset(ifa, 0, totallen);
  
  return ifa;
}


static inline struct rtattr* rw_netlink_nested_attr_start(struct nlmsghdr *n, int type)
{
  struct rtattr *attr;
  
  attr = (struct rtattr *)nlmsg_end_ptr(n);
  
  attr->rta_type = type;
  attr->rta_len = RTA_LENGTH(0);
  
  n->nlmsg_len += NLMSG_ALIGN(sizeof(struct rtattr));
  
  return attr;
}

static inline void
rw_netlink_nested_attr_end(struct nlmsghdr *n,
                           struct rtattr *attr)
{
  int len;
  int pad;
  
  len =  nlmsg_end_ptr(n) - (void *)attr;

  attr->rta_len = len;
  pad = NLMSG_ALIGN( n->nlmsg_len) -  n->nlmsg_len;
  if (pad){
    rw_netlink_add_base(n, pad, 0);
  }
  return;
}

/**************************************************************************************
 *                         STATIC
 *************************************************************************************/
static const u_char rw_netlink_maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
                                            0xf8, 0xfc, 0xfe, 0xff};
/**
 * This function is called to convert a prefixlen/masklen to in_addr format. This function
 * will be moved appropriately to a different library
 */
static void
rw_netlink_ipv4_masklen2ip (int masklen, struct in_addr *netmask)
{
  u_char *pnt;
  int bit;
  int offset;
  
  pnt = (unsigned char *) netmask;
  
  offset = masklen / 8;
  bit = masklen % 8;
  
  while (offset--)
    *pnt++ = 0xff;

  if (bit){
    *pnt = rw_netlink_maskbit[bit];
  }
}

/**
 * This function is called to convert a ipv4 address to masklen format. This function
 * will be moved appropriately to a different library
 */
unsigned char
rw_netlink_ip_masklen (uint32_t address)
{
  unsigned char len;
  unsigned char *pnt;
  unsigned char *end;
  unsigned char val;

  len = 0;
  pnt = (unsigned char *) &address;
  end = pnt + 4;

  while ((pnt < end) && (*pnt == 0xff))
  {
    len+= 8;
    pnt++;
  } 

  if (pnt < end)
  {
    val = *pnt;
    while (val)
    {
      len++;
      val <<= 1;
    }
  }
  return len;
}

/**
 * This function parses the netlink message for rt-attributes and puts them in a table
 * @param[in]  rta  - pointer to the data received over the socket typecasted to rtattr
 * @param[in]  len  - length of the attributes received in bytes (for the data)
 * @param[out] tb   - table with attributes
 * @param[in ] line - maximum size of the table
 *
 * @returns 0  always
 */
static int rw_parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
  memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
  while (RTA_OK(rta, len)) {
    if ((rta->rta_type <= max) && (!tb[rta->rta_type])){
      tb[rta->rta_type] = rta;
    }
    rta = RTA_NEXT(rta,len);
  }
  return 0;
}

/**************************************************************************************
 *                         RECEIVE
 *************************************************************************************/
#ifdef RWNETLINK_OWN
/**
 * This function is called when there is some data to be read in the netlink socket.
 * @param[in]  netlink_proto_handle  - netlink protocol handle which has socket
 * information to read
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_read_ow(rw_netlink_handle_proto_t *proto_handle, int end_on_done,
                               void *arg,
                               int (*filter)(rw_netlink_handle_proto_t *,
                                             struct nlmsghdr *, void *))
{
  int retval;
  struct msghdr msg;
  struct iovec iov;
  struct nlmsghdr *nlhdr;
  int totallen;
  struct sockaddr_nl nla = {0};
  int num_reads = 0, num_msgs = 0;
  
  iov.iov_base = (void *)proto_handle->read_buffer;
  msg.msg_name = (void *)&(nla);
  msg.msg_namelen = sizeof(nla);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  
  while (1){
    /*Maxmium 10 reads*/
    if (num_reads == 10){
      break;
    }
    iov.iov_len = RW_NETLINK_BUFFER_SIZE;
    retval = recvmsg (nl_socket_get_fd(proto_handle->sk),
                      &msg, MSG_DONTWAIT);
    num_reads++;
    if (retval < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN){
        return RW_STATUS_SUCCESS;
      }
      return RW_STATUS_FAILURE;
    }
    if (retval == 0){
      return RW_STATUS_SUCCESS;
    }
    if (msg.msg_namelen != sizeof(nla)) {
      return RW_STATUS_FAILURE;
    }
    
    totallen = retval;
    nlhdr = (struct nlmsghdr *)proto_handle->read_buffer;
    while(nlmsg_ok(nlhdr, totallen)){
      uint32_t msglen = nlhdr->nlmsg_len;
      uint32_t datalen = msglen - sizeof(struct nlmsghdr);
      
      if (datalen > totallen){
        return RW_STATUS_FAILURE;
      }
      num_msgs++;
      
      if (nlhdr->nlmsg_type == NLMSG_DONE){
        if (end_on_done){
          return RW_STATUS_SUCCESS;
        }
        goto skip;
      }
      if ((nlhdr->nlmsg_type == NLMSG_ERROR) ||
          (nlhdr->nlmsg_type == NLMSG_NOOP) ||
          (nlhdr->nlmsg_type == NLMSG_OVERRUN)) {
        goto skip;
      }
      
      if (!filter){
        rw_netlink_proto_defs[proto_handle->proto].filter(proto_handle, nlhdr, arg);
      }else{
        (*filter)(proto_handle, nlhdr, arg);
      }
   skip:
      nlhdr = nlmsg_next(nlhdr, &totallen);
    }
    
    if (msg.msg_flags & MSG_TRUNC) {
      continue;
    }
    if (totallen){
      RW_CRASH();
      return RW_STATUS_FAILURE;
    }
  }
  
  return RW_STATUS_SUCCESS;
}

#endif

/**
 * This function is called when there is some data to be read in the netlink socket.
 * @param[in]  netlink_proto_handle  - netlink protocol handle which has socket
 * information to read
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_read(rw_netlink_handle_proto_t *proto_handle)
{
  int ret;
#ifdef RWNETLINK_OWN
  ret = rw_netlink_read_ow(proto_handle, 0, NULL, NULL);
#else
  ret = nl_recvmsgs_default(proto_handle->sk);
#endif
  if (ret){
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


/**
 * This function is called when there is a valid netlink message received
 * @param[in] msg - netlink message recieved
 * @param[in] proto-handle - protocol handle received.
 */
static int rw_netlink_netfilter_recv_valid(struct nl_msg *msg, void *arg)
{
  rw_netlink_handle_proto_t *proto_handle = (rw_netlink_handle_proto_t *)arg;
  
  RW_ASSERT(msg);
  RW_ASSERT(proto_handle);
  
  return NL_OK;
}




/**
 * This function is called when there is a valid netlink message received
 * @param[in] msg - netlink message recieved
 * @param[in] proto-handle - protocol handle received.
 */
static int rw_netlink_xfrm_recv_valid(struct nl_msg *msg, void *arg)
{
  rw_netlink_handle_proto_t *proto_handle = (rw_netlink_handle_proto_t *)arg;
  
  RW_ASSERT(msg);
  RW_ASSERT(proto_handle);
  
  return NL_OK;
}


/**
 * This function is called when there is a valid netlink message received
 * @param[in] msg - netlink message recieved
 * @param[in] proto-handle - protocol handle received.
 */
static int rw_netlink_generic_recv_valid(struct nl_msg *msg, void *arg)
{
  rw_netlink_handle_proto_t *proto_handle = (rw_netlink_handle_proto_t *)arg;
  RW_ASSERT(msg);
  RW_ASSERT(proto_handle);
  
  return NL_OK;
}


/**
 * This function is called whenever there is an error message received from the kernel
 */
static int rw_netlink_route_recv_error(struct sockaddr_nl *nla,
                                       struct nlmsgerr *nlerr, void *arg)
{
  /*AKKI. Log a message detailing the netlink message for whch the error was receved*/
  return NL_SKIP;
}

/**
 * This function is called whenever a netlink message which is valid is recievd
 * in protocol NETLINK_ROUTE
 */
static
int rw_netlink_route_recv_validmsg(rw_netlink_handle_proto_t *proto_handle,
                                   struct nlmsghdr *nlhdr, void *arg)
{
  uint32_t msglen = nlhdr->nlmsg_len;
  int base = rw_netlink_proto_defs[NETLINK_ROUTE].cmd_base;
  
  switch(nlhdr->nlmsg_type){
    base = rw_netlink_proto_defs[proto_handle->proto].cmd_base;
    case RTM_NEWROUTE:
    case RTM_DELROUTE:
      {
        struct rtmsg *r = NLMSG_DATA(nlhdr);
        struct rtattr *tb[RTA_MAX+1];
        rw_netlink_route_entry_t *route;
        struct sockaddr_in6 skin6;
        struct sockaddr *sockaddr = (struct sockaddr*)&skin6;
        
        rw_parse_rtattr(tb,RTA_MAX, RTM_RTA(r), msglen - NLMSG_LENGTH(sizeof(*r)));

        if (tb[RTA_MULTIPATH]){
          route = RW_MALLOC0(sizeof(*route) +
                             (sizeof(rw_netlink_route_nh_entry_t) * 10));//AKKI fix this
        }else{
          route = RW_MALLOC0(sizeof(*route) + sizeof(rw_netlink_route_nh_entry_t));
        }
        
        if (tb[RTA_DST]){
          sockaddr->sa_family = r->rtm_family;
          memcpy(&sockaddr->sa_data,
                 RTA_DATA(tb[RTA_DST]),
                 RTA_PAYLOAD(tb[RTA_DST]));
        }else{
          //default route...
          memset(sockaddr, 0, sizeof(skin6));
          sockaddr->sa_family = r->rtm_family;
        }
        RW_PREFIX_FROM_SOCKADDR(&route->prefix, sockaddr);
        route->prefix.masklen = r->rtm_dst_len;
        
        if (nlhdr->nlmsg_flags & NLM_F_REPLACE){
          route->replace = 1;
        }else{
          route->replace = 0;
        }
        route->protocol = r->rtm_protocol;
        if (tb[RTA_MULTIPATH]){
          struct rtnexthop *nh = RTA_DATA(tb[RTA_MULTIPATH]);
          int len;
          struct rtattr *subtb[RTA_MAX+1];
          
          route->num_nexthops = 0;
          len = RTA_PAYLOAD(tb[RTA_MULTIPATH]);
          
          while (len >= sizeof(*nh)) {
            if (nh->rtnh_len > len)
              break;
            if (nh->rtnh_len > sizeof(*nh)) {
              rw_parse_rtattr(subtb,
                              RTA_MAX,
                              //RTM_RTA(nh),
                              RTNH_DATA(nh), 
                              nh->rtnh_len - sizeof(*nh));
              if (subtb[RTA_GATEWAY]){
                sockaddr->sa_family = r->rtm_family;
                memcpy(&sockaddr->sa_data,
                       RTA_DATA(subtb[RTA_GATEWAY]),
                       RTA_PAYLOAD(subtb[RTA_GATEWAY]));
                RW_IP_FROM_SOCKADDR(&route->nexthops[route->num_nexthops].gateway,
                                    sockaddr);
              }   
            }
            route->nexthops[route->num_nexthops].ifindex = nh->rtnh_ifindex;
            route->num_nexthops++;
            len -= NLMSG_ALIGN(nh->rtnh_len);
            nh = RTNH_NEXT(nh);
          }
        }else{
          route->num_nexthops = 1;
          if (tb[RTA_GATEWAY]){
            sockaddr->sa_family = r->rtm_family;
            memcpy(&sockaddr->sa_data,
                   RTA_DATA(tb[RTA_GATEWAY]),
                   RTA_PAYLOAD(tb[RTA_GATEWAY]));
            RW_IP_FROM_SOCKADDR(&route->nexthops[0].gateway, sockaddr);
          }
          if (tb[RTA_OIF]){
            memcpy(&route->nexthops[0].ifindex,
                   RTA_DATA(tb[RTA_OIF]),
                   RTA_PAYLOAD(tb[RTA_OIF]));
          }
        }
        
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base](proto_handle->handle->user_data,
                                                     (void *)route);
        }
        RW_FREE(route);
      }
      break;
    case RTM_NEWLINK:
    case RTM_DELLINK:
      {
        struct ifinfomsg *ifi = NLMSG_DATA(nlhdr);
        struct rtattr * tb[IFLA_MAX+1];
        rw_netlink_link_entry_t link_entry;
        
        rw_parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi),
                        msglen - NLMSG_LENGTH(sizeof(*ifi)));
        link_entry.ifindex = ifi->ifi_index;
        if (tb[IFLA_IFNAME]){
          link_entry.ifname =  RTA_DATA(tb[IFLA_IFNAME]);
        }else{
          link_entry.ifname  = NULL;
        }
        link_entry.flags = ifi->ifi_flags;
        link_entry.type = ifi->ifi_type;
        if (tb[IFLA_MTU]){
          link_entry.mtu = *(uint32_t*)RTA_DATA(tb[IFLA_MTU]);
        }else{
          link_entry.mtu= 0;
        }
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base](proto_handle->handle->user_data,
                                                     (void *)&link_entry);
        }
      }
      break;
    case RTM_NEWADDR:
    case RTM_DELADDR:
      {
        struct ifaddrmsg *ifa = NLMSG_DATA(nlhdr);
        struct rtattr *tb[IFA_MAX+1];
        rw_netlink_link_address_entry_t addr_entry;
        struct sockaddr_in6 skin6;
        struct sockaddr *sockaddr = (struct sockaddr *)&skin6;
        
        rw_parse_rtattr(tb,IFA_MAX, IFA_RTA(ifa),
                        msglen - NLMSG_LENGTH(sizeof(*ifa)));
        
        addr_entry.ifindex = ifa->ifa_index;
        addr_entry.mask = ifa->ifa_prefixlen;

        if (tb[IFA_LOCAL]){
          sockaddr->sa_family = ifa->ifa_family;
          memcpy(&sockaddr->sa_data,
                 RTA_DATA(tb[IFA_LOCAL]),
                 RTA_PAYLOAD(tb[IFA_LOCAL]));
          RW_IP_FROM_SOCKADDR(&addr_entry.address,sockaddr);
          if (tb[IFA_ADDRESS]){
            sockaddr->sa_family = ifa->ifa_family;
            memcpy(&sockaddr->sa_data,
                   RTA_DATA(tb[IFA_ADDRESS]),
                   RTA_PAYLOAD(tb[IFA_ADDRESS]));
            RW_IP_FROM_SOCKADDR(&addr_entry.remote_address, sockaddr);
          }else if (tb[IFA_LOCAL]) {
            rw_ip_prefix_t network;
            rw_ip_addr_copy((rw_ip_addr_t *)&network, &addr_entry.address);
            network.masklen = addr_entry.mask;
            rw_ip_prefix_apply_mask(&network);
            rw_ip_addr_copy(&addr_entry.remote_address, (rw_ip_addr_t *)&network);
          }
          if (proto_handle->cb[nlhdr->nlmsg_type - base]){
            proto_handle->cb[nlhdr->nlmsg_type - base](proto_handle->handle->user_data,
                                                       (void *)&addr_entry);
          }
        }
      }
      break;
    case RTM_NEWNEIGH:
    case RTM_DELNEIGH:
      {
        struct ndmsg *r = NLMSG_DATA(nlhdr);
        struct rtattr * tb[NDA_MAX+1];
        rw_netlink_neigh_entry_t arp_entry;
        struct sockaddr_in6 skin6;
        struct sockaddr *sockaddr = (struct sockaddr*)&skin6;
        
        rw_parse_rtattr(tb, NDA_MAX, NDA_RTA(r),
                        msglen - NLMSG_LENGTH(sizeof(*r)));
        arp_entry.ifindex = r->ndm_ifindex;
        if ((r->ndm_state != NUD_FAILED) &&
            (nlhdr->nlmsg_type == RTM_NEWNEIGH)){
          arp_entry.arp_state = RW_ARP_RESOLVED;
        }else{
          arp_entry.arp_state = RW_ARP_FAILED;
        }
        arp_entry.nud_state = r->ndm_state;
        if (tb[NDA_LLADDR]){
          memcpy(&arp_entry.mac.addr[0], RTA_DATA(tb[NDA_LLADDR]),
                 RTA_PAYLOAD(tb[NDA_LLADDR]));
        }else{
          if (arp_entry.arp_state == RW_ARP_RESOLVED)
            break;
        }
        
        if (tb[NDA_DST]){
          sockaddr->sa_family = r->ndm_family;
          memcpy(&sockaddr->sa_data,
                 RTA_DATA(tb[NDA_DST]),
                 RTA_PAYLOAD(tb[NDA_DST]));
          RW_IP_FROM_SOCKADDR(&arp_entry.address,
                              sockaddr);
        }
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base](proto_handle->handle->user_data,
                                                     (void *)&arp_entry);
        }
      }
      break;
    default:
      break;
  }
  return 0;
}

static int
rw_netlink_xfrm_convert_template_to_rw(rw_netlink_xfrm_template_info_t *info,
                                       struct xfrm_user_tmpl *tmpl,
                                       struct xfrm_usersa_info *xsinfo)
{
  
  int family = tmpl->family;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr *)&skin6;
  int addrsize = 0;

  if (family == AF_UNSPEC){
    family = xsinfo->family;
  }
  sockaddr->sa_family = family;
  
  switch(family){
    case AF_INET:
      addrsize = 4;
      break;
    case AF_INET6:
      addrsize = 16;
      break;
    default:
      RW_CRASH();
      return -1;
  };


  memcpy(&sockaddr->sa_data,
         &tmpl->saddr.a4, addrsize);
  RW_IP_FROM_SOCKADDR(&info->src,
                      sockaddr);
  memcpy(&sockaddr->sa_data,
         &tmpl->id.daddr.a4, addrsize);
  RW_IP_FROM_SOCKADDR(&info->dest,
                      sockaddr);
  info->spi      = tmpl->id.spi;
  info->proto    = tmpl->id.proto;
  
  info->reqid    = tmpl->reqid;
  info->mode     = tmpl->mode;
  info->share     = tmpl->share;
  info->optional     = tmpl->optional;
  info->aalgos    = tmpl->aalgos;
  info->ealgos    = tmpl->ealgos;
  info->calgos    = tmpl->calgos;
  return 0;
}

static void
rw_netlink_xfrm_add_algo_to_rw(rw_netlink_xfrm_sa_info_t *xsinfo,
                               rw_xfrm_algo_type_t type,
                               char *name, uint32_t keylen, uint32_t icv_len,
                               char *key)
{
  rw_netlink_xfrm_auth_info_t     **auth_info = &xsinfo->auth_info[xsinfo->num_auth];

  *auth_info = RW_MALLOC0(sizeof(**auth_info) + (keylen/8) + 1);
  (*auth_info)->alg_type = type;
  strncpy((*auth_info)->alg_name, name, sizeof((*auth_info)->alg_name));
  (*auth_info)->alg_key_len = keylen;
  (*auth_info)->trunc_icv_len.alg_icv_len = icv_len;
  memcpy((*auth_info)->alg_key, key, (keylen/8));
  
  xsinfo->num_auth++;
}


static void
rw_netlink_xfrm_add_algo_to_policy(rw_netlink_xfrm_policy_info_t *xpinfo,
                                   rw_xfrm_algo_type_t type,
                                   char *name, uint32_t keylen, uint32_t icv_len,
                                   char *key)
{
  rw_netlink_xfrm_auth_info_t     **auth_info = &xpinfo->auth_info[xpinfo->num_auth];

  *auth_info = RW_MALLOC0(sizeof(**auth_info) + (keylen/8) + 1);
  (*auth_info)->alg_type = type;
  strncpy((*auth_info)->alg_name, name, sizeof((*auth_info)->alg_name));
  (*auth_info)->alg_key_len = keylen;
  (*auth_info)->trunc_icv_len.alg_icv_len = icv_len;
  memcpy((*auth_info)->alg_key, key, (keylen/8));
  
  xpinfo->num_auth++;
}

static int
rw_netlink_xfrm_convert_selector_to_rw(rw_netlink_xfrm_selector_info_t *info,
                                       struct xfrm_selector *sel,
                                       int pref_family)
{
  
  int family = sel->family;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr *)&skin6;
  int addrsize = 0;


  if (family == AF_UNSPEC){
    family = pref_family;
  }
  sockaddr->sa_family = family;
  
  switch(family){
    case AF_INET:
      addrsize = 4;
      break;
    case AF_INET6:
      addrsize = 16;
      break;
    default:
      RW_CRASH();
      return -1;
  };
  memcpy(&sockaddr->sa_data,
         &sel->saddr.a4, addrsize);
  RW_PREFIX_FROM_SOCKADDR(&info->src, sockaddr);  
  info->src.masklen = sel->prefixlen_s;
  
  memcpy(&sockaddr->sa_data,
         &sel->daddr.a4, addrsize);
  RW_PREFIX_FROM_SOCKADDR(&info->dest, sockaddr);
  
  info->dest.masklen = sel->prefixlen_d;
  info->protocol = sel->proto;
  info->dport = ntohs(sel->dport);
  info->sport = ntohs(sel->sport);
  info->ifindex = sel->ifindex;
  info->kernelid = sel->user;
  info->dport_mask = ntohs(sel->dport); //akki
  info->sport_mask = ntohs(sel->sport); //akki

  return 0;
}

static int
rw_netlink_xfrm_convert_nlsa_to_rw( rw_netlink_xfrm_sa_info_t *info,
                                    struct rtattr **tb,
                                    struct xfrm_usersa_info *xsinfo)
{
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr *)&skin6;
  int addrsize = 0;
  int ret;
  rw_netlink_xfrm_selector_info_t selinfo;
  
  memset(&selinfo, 0, sizeof(selinfo));
  
  sockaddr->sa_family = xsinfo->family;
  
  switch(xsinfo->family){
    case AF_INET:
      addrsize = 4;
      break;
    case AF_INET6:
      addrsize = 16;
      break;
    default:
      RW_CRASH();
      return -1;
  };
  
  memcpy(&sockaddr->sa_data,
         &xsinfo->saddr.a4, addrsize);
  RW_IP_FROM_SOCKADDR(&info->src,
                      sockaddr);
  memcpy(&sockaddr->sa_data,
         &xsinfo->id.daddr.a4, addrsize);
  RW_IP_FROM_SOCKADDR(&info->dest,
                      sockaddr);
  
  info->protocol = xsinfo->id.proto;
  info->spi      = xsinfo->id.spi;
  info->seq      = xsinfo->seq;
  info->reqid    = xsinfo->reqid;
  info->mode     = xsinfo->mode;
  info->replay_window = xsinfo->replay_window;
  info->flags = xsinfo->flags;

  if (memcmp(&selinfo, &xsinfo->sel, sizeof(selinfo))){
    info->selector_info = RW_MALLOC0(sizeof(rw_netlink_xfrm_selector_info_t));
    ret = rw_netlink_xfrm_convert_selector_to_rw(info->selector_info,
                                                 &xsinfo->sel, xsinfo->family);
    if (ret){
      RW_CRASH();
      return -1;
    }
  }else{
    info->selector_info = NULL;
  }
  
  if (tb[XFRMA_TMPL]) {
    info->template_info = RW_MALLOC0(sizeof(rw_netlink_xfrm_template_info_t));
    ret = rw_netlink_xfrm_convert_template_to_rw(info->template_info,
                                                 (struct xfrm_user_tmpl *)tb[XFRMA_TMPL], xsinfo);
    if (ret){
      RW_CRASH();
      return -1;
    }
  }else{
    info->template_info = NULL;
  }

  {
    struct rtaddr *rta;
    
    if (tb[XFRMA_ALG_AUTH] && !tb[XFRMA_ALG_AUTH_TRUNC]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AUTH];
      rw_netlink_xfrm_add_algo_to_rw(info,
                                     RW_XFRM_ALGO_TYPE_AUTH,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                     (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_AUTH_TRUNC]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AUTH_TRUNC];
      rw_netlink_xfrm_add_algo_to_rw(info,
                                     RW_XFRM_ALGO_TYPE_TRUNC,
                                     ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_name,
                                     ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_key_len,
                                     ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_trunc_len,
                                     (char *)((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_AEAD]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AEAD];
      rw_netlink_xfrm_add_algo_to_rw(info,
                                     RW_XFRM_ALGO_TYPE_AEAD,
                                     ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_name,
                                     ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_key_len,
                                     ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_icv_len,
                                     (char *)((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_CRYPT]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_CRYPT];
      rw_netlink_xfrm_add_algo_to_rw(info,
                                     RW_XFRM_ALGO_TYPE_CRYPT,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                     (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);
    }

    if (tb[XFRMA_ALG_COMP]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_COMP];
      rw_netlink_xfrm_add_algo_to_rw(info,
                                     RW_XFRM_ALGO_TYPE_COMP,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                     ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                     (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);  
    }
  }

  
  return 0;
}




static int
rw_netlink_xfrm_convert_nlpolicy_to_rw( rw_netlink_xfrm_policy_info_t *info,
                                        struct rtattr **tb,
                                        struct xfrm_userpolicy_info *xpinfo)
{
  int ret;
  rw_netlink_xfrm_selector_info_t selinfo;
  memset(&selinfo, 0, sizeof(selinfo));
  
  info->index = xpinfo->index;
  info->priority = xpinfo->priority;
  info->dir = xpinfo->dir;
  info->action = xpinfo->action;
  info->share = xpinfo->share;
  info->flags = xpinfo->flags;

  if (memcmp(&selinfo, &xpinfo->sel, sizeof(selinfo))){
    info->selector_info = RW_MALLOC0(sizeof(rw_netlink_xfrm_selector_info_t));
    ret = rw_netlink_xfrm_convert_selector_to_rw(info->selector_info,
                                                 &xpinfo->sel, xpinfo->sel.family);
    if (ret){
      RW_CRASH();
      return -1;
    }
  }else{
    info->selector_info = NULL;
  }
  if (tb[XFRMA_POLICY_TYPE]) {
    struct xfrm_userpolicy_type *upt;

    upt = (struct xfrm_userpolicy_type *)RTA_DATA(tb[XFRMA_POLICY_TYPE]);
    info->type = upt->type;
  }

  {
    struct rtaddr *rta;
    
    if (tb[XFRMA_ALG_AUTH] && !tb[XFRMA_ALG_AUTH_TRUNC]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AUTH];
      rw_netlink_xfrm_add_algo_to_policy(info,
                                         RW_XFRM_ALGO_TYPE_AUTH,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                         (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_AUTH_TRUNC]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AUTH_TRUNC];
      rw_netlink_xfrm_add_algo_to_policy(info,
                                         RW_XFRM_ALGO_TYPE_TRUNC,
                                         ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_name,
                                         ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_key_len,
                                         ((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_trunc_len,
                                         (char *)((struct xfrm_algo_auth *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_AEAD]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_AEAD];
      rw_netlink_xfrm_add_algo_to_policy(info,
                                         RW_XFRM_ALGO_TYPE_AEAD,
                                         ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_name,
                                         ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_key_len,
                                         ((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_icv_len,
                                         (char *)((struct xfrm_algo_aead *) RTA_DATA(rta))->alg_key);
    }
  
    if (tb[XFRMA_ALG_CRYPT]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_CRYPT];
      rw_netlink_xfrm_add_algo_to_policy(info,
                                         RW_XFRM_ALGO_TYPE_CRYPT,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                         (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);
    }
    
    if (tb[XFRMA_ALG_COMP]) {
      rta = (struct rtaddr *)tb[XFRMA_ALG_COMP];
      rw_netlink_xfrm_add_algo_to_policy(info,
                                         RW_XFRM_ALGO_TYPE_COMP,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_name,
                                         ((struct xfrm_algo *) RTA_DATA(rta))->alg_key_len, 0,
                                         (char *)((struct xfrm_algo *) RTA_DATA(rta))->alg_key);  
    }
  }

  
  return 0;
}

/**
 * This function is called whenever a netlink message which is valid is recievd
 * in protocol NETLINK_ROUTE
 */
static
int rw_netlink_xfrm_recv_validmsg(rw_netlink_handle_proto_t *proto_handle,
                                  struct nlmsghdr *nlhdr, void *arg)
{
  uint32_t msglen = nlhdr->nlmsg_len;
  int base = rw_netlink_proto_defs[NETLINK_XFRM].cmd_base;
  struct rtattr * tb[XFRMA_MAX+1];
  struct rtattr * rta;
  int ret;
  rw_netlink_xfrm_sa_info_t info;
  struct xfrm_usersa_info *xsinfo = NULL;
  struct xfrm_usersa_id	*xsid = NULL;
  rw_netlink_xfrm_policy_info_t pinfo;
  struct xfrm_userpolicy_info *xpinfo = NULL;
  struct xfrm_userpolicy_id *xpid = NULL;
  
  

  switch(nlhdr->nlmsg_type){
    base = rw_netlink_proto_defs[proto_handle->proto].cmd_base;
    
    case XFRM_MSG_NEWSA:
    case XFRM_MSG_UPDSA:
      {
        memset(&info, 0, sizeof(info));
        xsinfo = NLMSG_DATA(nlhdr);
        rta = ((struct rtattr*)(((char*)(xsinfo)) + NLMSG_ALIGN(sizeof(struct xfrm_usersa_info))));
        rw_parse_rtattr(tb, XFRMA_MAX, rta, 
                        msglen - NLMSG_SPACE(sizeof(*xsinfo)));
        ret = rw_netlink_xfrm_convert_nlsa_to_rw(&info, tb, xsinfo);
        if (ret){
          RW_CRASH();
          return -1;
        }

        
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base]
              (proto_handle->handle->user_data,
               (void *)&info);
        }

        for (ret = 0; ret < info.num_auth; ret++){
          RW_FREE(info.auth_info[ret]);
        }
        if (info.selector_info){
          RW_FREE(info.selector_info);
        }
        if (info.template_info){
          RW_FREE(info.template_info);
        }
      }
      break;
    case XFRM_MSG_DELSA:
      {
        memset(&info, 0, sizeof(info));
        xsid = NLMSG_DATA(nlhdr);
        rta = ((struct rtattr*)(((char*)(xsid)) + NLMSG_ALIGN(sizeof(struct xfrm_usersa_id))));
        rw_parse_rtattr(tb, XFRMA_MAX, rta, 
                        msglen - NLMSG_SPACE(sizeof(*xsid)));
        xsinfo = RTA_DATA(tb[XFRMA_SA]);
        RW_ASSERT(xsinfo);
        ret = rw_netlink_xfrm_convert_nlsa_to_rw(&info, tb, xsinfo);
        if (ret){
          RW_CRASH();
          return -1;
        }
        
        
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base]
              (proto_handle->handle->user_data,
               (void *)&info);
        }
        for (ret = 0; ret < info.num_auth; ret++){
          RW_FREE(info.auth_info[ret]);
        }
        if (info.selector_info){
          RW_FREE(info.selector_info);
        }
        if (info.template_info){
          RW_FREE(info.template_info);
        }
      }
      break;
    case XFRM_MSG_NEWPOLICY:
    case XFRM_MSG_UPDPOLICY:
      {
        memset(&pinfo, 0, sizeof(pinfo));
        xpinfo = NLMSG_DATA(nlhdr);
        rta = ((struct rtattr*)(((char*)(xpinfo)) + NLMSG_ALIGN(sizeof(struct xfrm_userpolicy_info))));
        rw_parse_rtattr(tb, XFRMA_MAX, rta, 
                        msglen - NLMSG_SPACE(sizeof(*xpinfo)));
        
        ret = rw_netlink_xfrm_convert_nlpolicy_to_rw(&pinfo, tb, xpinfo);
        if (ret){
          RW_CRASH();
          return -1;
        }
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base]
              (proto_handle->handle->user_data,
               (void *)&pinfo);
        }
        for (ret = 0; ret < pinfo.num_auth; ret++){
          RW_FREE(pinfo.auth_info[ret]);
        }
        if (pinfo.selector_info){
          RW_FREE(pinfo.selector_info);
        }
      }
      break;
    case XFRM_MSG_DELPOLICY:
      {
        memset(&pinfo, 0, sizeof(info));
        xpid = NLMSG_DATA(nlhdr);
        rta = ((struct rtattr*)(((char*)(xpid)) + NLMSG_ALIGN(sizeof(struct xfrm_userpolicy_id))));
        rw_parse_rtattr(tb, XFRMA_MAX, rta, 
                        msglen - NLMSG_SPACE(sizeof(*xpid)));
        xpinfo = (struct xfrm_userpolicy_info *)RTA_DATA(tb[XFRMA_POLICY]);
        RW_ASSERT(xpinfo);
        ret = rw_netlink_xfrm_convert_nlpolicy_to_rw(&pinfo, tb, xpinfo);
        if (ret){
          RW_CRASH();
          return -1;
        }
        
        if (proto_handle->cb[nlhdr->nlmsg_type - base]){
          proto_handle->cb[nlhdr->nlmsg_type - base]
              (proto_handle->handle->user_data,
               (void *)&pinfo);
        }
        for (ret = 0; ret < pinfo.num_auth; ret++){
          RW_FREE(pinfo.auth_info[ret]);
        }
        if (pinfo.selector_info){
          RW_FREE(pinfo.selector_info);
        }
      }
      break;
    default:
      break;
  }
  return 0;
}



/**
 * This function is called when there is a valid netlink message received
 * @param[in] msg - netlink message recieved
 * @param[in] proto-handle - protocol handle received.
 */
static int rw_netlink_route_recv_valid(struct nl_msg *msg, void *arg)
{
  rw_netlink_handle_proto_t *proto_handle = (rw_netlink_handle_proto_t *)arg;
  struct nlmsghdr *nlhdr = nlmsg_hdr(msg);

  return rw_netlink_route_recv_validmsg(proto_handle, nlhdr, NULL);
}



/**************************************************************************************
 *                         INIT/TERMINATE
 *************************************************************************************/
static uint32_t rw_netlink_get_local_port(rw_netlink_handle_t *handle,
                                          rw_netlink_socket_type_t type)
{
  uint32_t pid;
  if (handle->local_id){
    pid = handle->local_id & 0x3FFFFF;
  }else{
    pid = getpid() & 0x3FFFFF;
  }

  return (pid + (type << 22));
}

static rw_status_t rw_netlink_socket_connect(rw_netlink_handle_t *handle,
                                             struct nl_sock *sk, int protocol,
                                             rw_netlink_socket_type_t type)
{
  int currfd = rwnetns_get_current_netfd();

  rwnetns_change(handle->namespace_name);
  nl_socket_set_local_port(sk, rw_netlink_get_local_port(handle, type));
  nl_connect(sk, protocol);
  
  rwnetns_change_fd(currfd);

  close(currfd);
  return RW_STATUS_SUCCESS;
}

/**
 * This function is called to close a netlink socket for a particular netlink protocol
 * @param[in]  proto  - netlink protocol with which to open the AF_NETLINK socket.
 * @param[in ] handle - netlink handle
 */
void rw_netlink_handle_proto_terminate(rw_netlink_handle_proto_t *proto_handle)
{
  if (proto_handle->sk){
    nl_socket_free(proto_handle->sk);
    proto_handle->sk = 0;
  }
#ifdef RWNETLINK_OWN
  if (proto_handle->read_buffer){
    RW_FREE(proto_handle->read_buffer);
    proto_handle->read_buffer = NULL;
  }
#endif
  RW_FREE(proto_handle->head_write_buffer);
  RW_FREE(proto_handle);
}


static rw_netlink_handle_proto_t*
rw_netlink_handle_proto_init_internal(rw_netlink_handle_t *handle,
                                      uint32_t proto, rw_netlink_socket_type_t type)
{
  rw_netlink_handle_proto_t *proto_handle;
  int retval;
  struct nl_cb *cb, *orig;
  uint32_t groups = rw_netlink_proto_defs[proto].allgroups;
  
  
  RW_ASSERT(handle);

  proto_handle = RW_MALLOC0(sizeof(*proto_handle));
  
  proto_handle->handle = handle;
  proto_handle->proto = proto;
  
  /*Open the netlink socket*/
  proto_handle->sk = nl_socket_alloc();
  if (!proto_handle->sk){
    goto ret;
  }
  nl_join_groups(proto_handle->sk, groups);

  /*Just the kernel*/
  nl_socket_set_peer_port(proto_handle->sk, 0);

  retval = rw_netlink_socket_connect(handle, proto_handle->sk, proto, type);
  if (retval != RW_STATUS_SUCCESS){
    goto error;
  }
  
  /* Required to receive async event notifications */
  nl_socket_disable_seq_check(proto_handle->sk);
      
  retval = nl_socket_set_nonblocking(proto_handle->sk);
  if (retval < 0){
    goto error;
  }

  nl_socket_disable_auto_ack(proto_handle->sk);

  retval = nl_socket_set_buffer_size(proto_handle->sk, RW_NETLINK_SOCKET_BUFFER_SIZE,
                                     RW_NETLINK_SOCKET_BUFFER_SIZE);
  if (retval < 0){
    goto error;
  }
  
  nl_socket_set_msg_buf_size(proto_handle->sk, RW_NETLINK_BUFFER_SIZE);


  /*Set the callbacks based on the proto*/
  orig =  nl_socket_get_cb(proto_handle->sk);
  if (!orig){
    goto error;
  }
  cb = nl_cb_clone(orig);
  nl_cb_put(orig);
  if (!cb){
    goto error;
  }
  /*Move the registered callbacks to the top*/
  switch(proto){
    case NETLINK_ROUTE:
      nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                rw_netlink_route_recv_valid, proto_handle);
      nl_cb_err(cb, NL_CB_CUSTOM, rw_netlink_route_recv_error, proto_handle);
      break;
    case NETLINK_GENERIC:
      nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                rw_netlink_generic_recv_valid, proto_handle);
      break;
    case NETLINK_NETFILTER:
      nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                rw_netlink_netfilter_recv_valid, proto_handle);
      break;
    case NETLINK_XFRM:
      nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
                rw_netlink_xfrm_recv_valid, proto_handle);
      break;
    default:
      RW_CRASH();
  }
  nl_socket_set_cb(proto_handle->sk, cb);

  /*Buffers*/
#ifdef RWNETLINK_OWN
  proto_handle->read_buffer = RW_MALLOC(RW_NETLINK_BUFFER_SIZE);
#endif
  proto_handle->head_write_buffer = proto_handle->write_buffer =
      RW_MALLOC(RW_NETLINK_BUFFER_SIZE);
  proto_handle->total_send_buf_size = RW_NETLINK_BUFFER_SIZE;
  proto_handle->bundle = 0;
  
  proto_handle->send_seq = 0;
  RW_ASSERT(proto_handle->write_buffer);

ret:
  return proto_handle;
error:
  nl_socket_free(proto_handle->sk);
  proto_handle->sk = NULL;
  goto ret;
}


/**
 * This function initializes a netlink socket for a particular netlink protocol. For now,
 * this function is getting called for NETLINK_ROUTE. When we support MPLS, we can call
 * same fucntion for NETLINK_GENERIC. This function currently does not schedule a read
 * event. For now, the application must do the poll and check for reads.
 * @param[in]  proto  - netlink protocol with which to open the AF_NETLINK socket.
 * @param[in]  groups - groups to be registered with the socket when binding.
 * @param[in ] handle - netlink handle
 * @returns handle for the protoocol(NETLINK_ROUTE)  if success
 * @returns NULL
 */
rw_netlink_handle_proto_t* rw_netlink_handle_proto_init(rw_netlink_handle_t *handle,
                                                        uint32_t proto)
{
  if (handle->proto_netlink[proto]){
    return handle->proto_netlink[proto]; 
  }
  
  handle->proto_netlink[proto] =
      rw_netlink_handle_proto_init_internal(handle, proto,
                                            RW_NETLINK_CONNECT);

  return handle->proto_netlink[proto];
}

/**
 * This function opens the necessary netlink sockets depending on the callbacks registered
 * The groups assigned to each netlink socket also depends on the callbacks registered.
 * The function returns a handle. One member of the handle is the "user_data" which could
 * a back pointer to the application datatype that is calling it.
 * @param[in]  user_data  - userdata referenced by the netlink handle that is returned.
 * @param[in]  network namespace name
 * @returns netlink_handle  if success
 * @returns NULL if failure
 */
rw_netlink_handle_t* rw_netlink_handle_init(void *userdata, char *name,
                                            uint32_t namespace_id)
{
  rw_netlink_handle_t *handle;
  
  handle = RW_MALLOC0(sizeof(*handle));
  if (name){
    strcpy(handle->namespace_name, name);
  }
  handle->namespace_id = 0;
  handle->user_data = userdata;

#if 0
  handle->ioctl_sock_fd = rwnetns_socket(name, AF_INET, SOCK_DGRAM, 0);
  if (handle->ioctl_sock_fd <= 0){
    handle->ioctl_sock_fd = 0;
    rw_netlink_terminate(handle);
    handle = NULL;
  }

  handle->ioctl6_sock_fd = rwnetns_socket(name, AF_INET6, SOCK_DGRAM, 0);
  if (handle->ioctl6_sock_fd <= 0){
    handle->ioctl6_sock_fd = 0;
    rw_netlink_terminate(handle);
    handle = NULL;
  }
#endif
  return handle;
}


void rw_netlink_handle_set_local_id(rw_netlink_handle_t *handle,
                                    uint32_t id)
{
  handle->local_id = id;
}

/**
 * This function closes the netlink sockets that are opened. The handle is also freed.
 * @param[in]  netlink_handle  - handle to free
 */
void rw_netlink_terminate( rw_netlink_handle_t *handle)
{
  int i;
  
  RW_ASSERT(handle);
  for (i = 0; i < MAX_LINKS; i++){
    if (handle->proto_netlink[i]) {
      rw_netlink_handle_proto_terminate(handle->proto_netlink[i]);
      handle->proto_netlink[i] = NULL;
    }
  }
  if (handle->ioctl_sock_fd > 0){
    RW_ASSERT(rw_netlink_get_sock_fd(handle, NETLINK_ROUTE) != handle->ioctl_sock_fd);
    close(handle->ioctl_sock_fd);
    handle->ioctl_sock_fd = 0;
  }
  if (handle->ioctl6_sock_fd > 0){
    close(handle->ioctl6_sock_fd);
    handle->ioctl6_sock_fd = 0;
  }
  
  RW_FREE(handle);
  return;
}
/**
 * This function is called to register callbacks for given netlink protocol and
 * netlink command
 * @param[in] - handle - netlink handle
 * @param[in] - proto  - netlink protocol
 * @param[in] - cmd    - netlink command
 * @param[in] - cb     - callback
 */
void rw_netlink_register_callback(rw_netlink_handle_t *handle, uint32_t proto,
                                  int cmd, rw_netlink_cb_t cb)
{
  uint32_t base;
  rw_netlink_handle_proto_t *proto_handle;
  
  RW_ASSERT(handle);
  base =  rw_netlink_proto_defs[proto].cmd_base;
  proto_handle = handle->proto_netlink[proto];
  if (!handle->proto_netlink[proto]){
    handle->proto_netlink[proto] = proto_handle =
        rw_netlink_handle_proto_init_internal(handle,
                                              proto,RW_NETLINK_CONNECT);
  }
  RW_ASSERT((cmd-base) < RW_NETLINK_MAX_CALLBACKS);
  if (proto_handle){
    handle->proto_netlink[proto]->cb[cmd-base] = cb;
    /*Since the command will belong to a group, we can automatically join the group here
     */
    if (proto_handle->sk){
      nl_socket_add_memberships(proto_handle->sk,
                                rw_netlink_proto_defs[proto].cmds[cmd-base].group);
    }
  }
  return;
}


rw_status_t
rw_netlink_netns_read(rw_netlink_handle_t *handle)
{
  char buf[4096];
  ssize_t len;
  struct inotify_event *event;

  if (!handle->netns_fd){
    return RW_STATUS_FAILURE;
  }

  len  = read(handle->netns_fd, buf, sizeof(buf));
  if (len < 0) {
    return RW_STATUS_FAILURE;
  }
  for (event = (struct inotify_event *)buf;
		     (char *)event < &buf[len];
       event = (struct inotify_event *)((char *)event + sizeof(*event) + event->len)) {
    if (event->mask & IN_CREATE) {
      if (handle->add_netns_cb){
        handle->add_netns_cb(handle->user_data, event->name);
      }
    }
    if (event->mask & IN_DELETE){
      if (handle->del_netns_cb){
        handle->del_netns_cb(handle->user_data, event->name);
      }
    }
  }

  return RW_STATUS_SUCCESS;
}


int
rw_netlink_register_netns_callback(rw_netlink_handle_t *handle,
                                   rw_netlink_cb_t add_cb,
                                   rw_netlink_cb_t del_cb)
{
  struct dirent *entry;
  DIR *dir;
  
  if (!handle->netns_fd){
    handle->netns_fd = inotify_init();
    if (handle->netns_fd < 0) {
      return 0;
    }
  
    if (inotify_add_watch(handle->netns_fd, RWNETNS_DIR, IN_CREATE | IN_DELETE) < 0) {
      close(handle->netns_fd);
      handle->netns_fd = 0;
      return 0;
    }
  }

  dir = opendir(RWNETNS_DIR);
  if (!dir){
    return 0;
  }
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0)
      continue;
    if (strcmp(entry->d_name, "..") == 0)
      continue;
    if (add_cb){
      add_cb(handle->user_data, entry->d_name);
    }
  }
  closedir(dir);
  
  handle->add_netns_cb = add_cb;
  handle->del_netns_cb = del_cb;

  return handle->netns_fd;
}

/**************************************************************************************
 *                         Send NETLINK_ROUTE
 *************************************************************************************/                                                       
/**
 * This function is called to delete/add an address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  cmd - add/delete address
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
static rw_status_t rw_netlink_update_interface_address(rw_netlink_handle_t *handle,
                                                       int cmd,
                                                       rw_netlink_link_address_entry_t *addr_entry,
                                                       uint32_t nl_flags)
{
  struct nlmsghdr *nlhdr;
  struct ifaddrmsg *ifa;
  int ret = 0;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr*)&skin6;

  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  if (!handle->proto_netlink[NETLINK_ROUTE]->sk){
    return RW_STATUS_FAILURE;
  }
  
  /*Populate the header*/
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST|nl_flags;
  nlhdr->nlmsg_type = cmd;
  
  /*Populate the inetrface address message*/
  ifa = (struct ifaddrmsg *)rw_netlink_add_base(nlhdr, sizeof(*ifa), 0);

  ifa->ifa_scope = RT_SCOPE_UNIVERSE;
  ifa->ifa_family = RW_IP_ADDR_FAMILY(&addr_entry->address);
  ifa->ifa_index = addr_entry->ifindex;
  ifa->ifa_prefixlen = addr_entry->mask;
  ifa->ifa_flags = 0;
  RW_IP_TO_SOCKADDR(&addr_entry->address,
                    sockaddr);
  /*Add the attributes*/
  rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                      IFA_LOCAL, 
                      &sockaddr->sa_data,
                      RW_IS_ADDR_IPV4(&addr_entry->address)?4:16);
  
  if (RW_IP_VALID_NONZERO(&addr_entry->remote_address)){
    RW_IP_TO_SOCKADDR(&addr_entry->remote_address,
                      sockaddr);
    rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                        IFA_ADDRESS, 
                        &sockaddr->sa_data,
                        RW_IS_ADDR_IPV4(&addr_entry->address)?4:16);
  }else{
    /*There is no remote-address. Assign the network for address*/
    rw_ip_prefix_t network;

    rw_ip_addr_copy((rw_ip_addr_t *)&network, &addr_entry->address);
    network.masklen = addr_entry->mask;
    rw_ip_prefix_apply_mask(&network);
    RW_IP_TO_SOCKADDR((rw_ip_addr_t*)&network,
                      sockaddr);
    rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                        IFA_ADDRESS, 
                        &sockaddr->sa_data,
                        RW_IS_ADDR_IPV4(&addr_entry->address)?4:16);
  }

  
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}

/**
 * This function is called to delete an address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 
 */
rw_status_t rw_netlink_interface_address_delete(rw_netlink_handle_t *handle,
                                                rw_netlink_link_address_entry_t *addr_entry)
{
  return rw_netlink_update_interface_address(handle, RTM_DELADDR,
                                             addr_entry,
                                             NLM_F_CREATE|NLM_F_REPLACE);
}


/**
 * This function is called to add an address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_interface_address_add(rw_netlink_handle_t *handle,
                                             rw_netlink_link_address_entry_t *addr_entry)
{
  return rw_netlink_update_interface_address(handle, RTM_NEWADDR, 
                                             addr_entry,
                                             NLM_F_CREATE|NLM_F_REPLACE);
}






/**
 * This function is called to update a remote address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
static rw_status_t rw_netlink_update_interface_remote_address(rw_netlink_handle_t *handle,
                                                              int cmd,
                                                              rw_netlink_link_address_entry_t *addr_entry,
                                                              uint32_t nl_flags)
{
  struct nlmsghdr *nlhdr;
  struct ifaddrmsg *ifa;
  int ret = 0;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr*)&skin6;

  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  if (!handle->proto_netlink[NETLINK_ROUTE]->sk){
    return RW_STATUS_FAILURE;
  }
  
  /*Populate the header*/
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST|nl_flags;
  nlhdr->nlmsg_type = cmd;
  
  /*Populate the inetrface address message*/
  ifa = (struct ifaddrmsg *)rw_netlink_add_base(nlhdr, sizeof(*ifa), 0);
  
  ifa->ifa_scope = RT_SCOPE_UNIVERSE;
  ifa->ifa_family = RW_IP_ADDR_FAMILY(&addr_entry->address);
  ifa->ifa_index = addr_entry->ifindex;
  ifa->ifa_prefixlen = addr_entry->mask;
  ifa->ifa_flags = 0;
  RW_IP_TO_SOCKADDR(&addr_entry->address,
                    sockaddr);
  /*Add the attributes*/
  rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                      IFA_ADDRESS,
                      &sockaddr->sa_data,
                      RW_IS_ADDR_IPV4(&addr_entry->address)?4:16);
  
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;
}


/**
 * This function is called to delete a remote address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_interface_remote_address_delete(rw_netlink_handle_t *handle,
                                                       rw_netlink_link_address_entry_t *addr_entry)
{
  return rw_netlink_update_interface_remote_address(handle, RTM_DELADDR,
                                                    addr_entry,
                                                    NLM_F_CREATE|NLM_F_REPLACE);
}

/**
 * This function is called to add a remote address and mask to an interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_interface_remote_address_add(rw_netlink_handle_t *handle,
                                                    rw_netlink_link_address_entry_t *addr_entry)
{
  return rw_netlink_update_interface_remote_address(handle, RTM_NEWADDR, 
                                                    addr_entry,
                                                    NLM_F_CREATE|NLM_F_REPLACE);
}

/**
 * Create a new kernel veth device
 * @param[in] handle
 * @param[in] name		name of the veth device or NULL
 * @param[in] peer_name	name of its peer or NULL
 * @param[in peer_namespace      net namespace where the peer should be created
 *
 * Creates a new veth device pair in the kernel and move the peer
 * to the network namespace specified.
 *
 * @return 0 on success or a negative error code
 */
int rw_netlink_create_veth_pair(rw_netlink_handle_t *handle,
                                const char *name, const char *peername,
                                char *peer_namespace)
{
  int netns_fd = 0;
  struct ifinfomsg *ifi;
  struct nlmsghdr *nlhdr;
  int ret = 0;
  struct rtattr *info;
  char kind[6];
  
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  if (!handle->proto_netlink[NETLINK_ROUTE]->sk){
    return RW_STATUS_FAILURE;
  }
  if (peer_namespace){
    netns_fd = rwnetns_get_netfd(peer_namespace);
    if (netns_fd <= 0){
      return RW_STATUS_FAILURE;
    }
  }
  
  /*Populate the header*/
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
  nlhdr->nlmsg_type = RTM_NEWLINK;
  
  /*Populate the inetrface address message*/
  ifi = (struct ifinfomsg *)rw_netlink_add_base(nlhdr, sizeof(*ifi), 0);
  ifi->ifi_family = AF_UNSPEC;
  
  if (name){
    rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                        IFLA_IFNAME,
                        name, (strlen(name) + 1));
  }
  info = rw_netlink_nested_attr_start(nlhdr, IFLA_LINKINFO);

  sprintf(kind, "veth");
  rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                      IFLA_INFO_KIND,
                      kind, strlen(kind)+1);

  /*Fill in the peer details*/
  {
    struct ifinfomsg *peer_ifi;
    struct rtattr *peer_data, *info_peer;
    
    peer_data = rw_netlink_nested_attr_start(nlhdr, IFLA_INFO_DATA);
    
    info_peer = rw_netlink_nested_attr_start(nlhdr, VETH_INFO_PEER);
    
    peer_ifi = rw_netlink_add_base(nlhdr, sizeof(*peer_ifi), 0);
    peer_ifi->ifi_family = AF_UNSPEC;
    if (peername){
      rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                          IFLA_IFNAME,
                          peername, strlen(peername) + 1);
    }
    if (netns_fd > 0){
      rw_netlink_add_attr(nlhdr, 1024,
                          IFLA_NET_NS_FD,
                          &netns_fd, 4);
    }
    rw_netlink_nested_attr_end(nlhdr, info_peer);
    rw_netlink_nested_attr_end(nlhdr, peer_data);
  }
  
  rw_netlink_nested_attr_end(nlhdr, info);
  
  
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    if (netns_fd){
      close(netns_fd);
    }
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  if (netns_fd){
    close(netns_fd);
  }
  return RW_STATUS_SUCCESS;
}



/**
 * This function is called to update a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @param[in]  cmd    - add/del
 * @param[in]  flags  - replace/create..
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
static rw_status_t rw_netlink_update_route(rw_netlink_handle_t *handle,
                                           rw_netlink_route_entry_t *route, int cmd,
                                           uint32_t nl_flags)
{
  struct nlmsghdr *nlhdr;
  struct rtmsg 	   *rmsg;
  int ret = 0;
  struct rtattr *rta;
  struct rtnexthop *rtnh;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr*)&skin6;
  int family_len;
  
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  if (!handle->proto_netlink[NETLINK_ROUTE]->sk){
    return RW_STATUS_FAILURE;
  }
  
  /*Populate the header*/
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST|nl_flags;
  nlhdr->nlmsg_type = cmd;
  
  /*Populate the inetrface address message*/
  rmsg = (struct rtmsg *)rw_netlink_add_base(nlhdr, sizeof(*rmsg), 0);
  rmsg->rtm_family = RW_IP_ADDR_FAMILY(&route->prefix);
  
  rmsg->rtm_table = RT_TABLE_MAIN;
  rmsg->rtm_scope = RT_SCOPE_UNIVERSE;
  rmsg->rtm_type = RTN_UNICAST;
  rmsg->rtm_protocol = route->protocol;
  
  rmsg->rtm_dst_len = route->prefix.masklen;
  if (RW_IS_PREFIX_IPV4(&route->prefix)){
    family_len = 4;
  }else{
    family_len = 16;
  }
  if (route->replace) {
  nlhdr->nlmsg_flags |= NLM_F_REPLACE;
  }
  RW_IP_TO_SOCKADDR((rw_ip_addr_t *)&route->prefix, sockaddr);  
  rw_netlink_add_attr(nlhdr, 1024, 
                      RTA_DST, 
                      &sockaddr->sa_data,
                      family_len);
 if (route->num_nexthops) { 
  rta = rw_netlink_nested_attr_start(nlhdr, RTA_MULTIPATH);
  for (ret = 0; ret < route->num_nexthops; ret++){
    rtnh = rw_netlink_add_base(nlhdr, sizeof(*rtnh), 4);
    
    rtnh->rtnh_ifindex = route->nexthops[ret].ifindex;
    
    if (RW_IP_VALID_NONZERO(&route->nexthops[ret].gateway)){
      RW_IP_TO_SOCKADDR(&route->nexthops[ret].gateway, sockaddr);
      rw_netlink_add_attr(nlhdr, 1024,
                          RTA_GATEWAY,
                          &sockaddr->sa_data,
                          family_len);
    }
    rtnh->rtnh_len = 
        nlmsg_end_ptr(nlhdr) - (void *) rtnh;
  }
  rw_netlink_nested_attr_end(nlhdr, rta);
 }
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;  
}

/**
 * This function is called to add a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_add_route(rw_netlink_handle_t *handle,
                                 rw_netlink_route_entry_t *route)
{
  return rw_netlink_update_route(handle, route, RTM_NEWROUTE,
                                 NLM_F_REQUEST|NLM_F_CREATE);
}


/**
 * This function is called to delete a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_del_route(rw_netlink_handle_t *handle,
                                 rw_netlink_route_entry_t *route)
{
  return rw_netlink_update_route(handle, route, RTM_DELROUTE,
                                 NLM_F_REQUEST);
}




/**
 * This function is called to update a neighbor to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  neighbor  - neighbor to be added/deleted
 * @param[in]  cmd    - add/del
 * @param[in]  flags  - replace/create..
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
static rw_status_t rw_netlink_update_neigh(rw_netlink_handle_t *handle,
                                           rw_netlink_neigh_entry_t *neigh, int cmd,
                                           uint32_t nl_flags)
{
  struct nlmsghdr *nlhdr;
  struct ndmsg   *rmsg;
  int ret = 0;
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr*)&skin6;
  int family_len;
  
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  if (!handle->proto_netlink[NETLINK_ROUTE]->sk){
    return RW_STATUS_FAILURE;
  }
  
  /*Populate the header*/
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST|nl_flags;
  nlhdr->nlmsg_type = cmd;
  
  /*Populate the inetrface address message*/
  rmsg = (struct ndmsg *)rw_netlink_add_base(nlhdr, sizeof(*rmsg), 0);
  rmsg->ndm_family = RW_IP_ADDR_FAMILY(&neigh->address);
  if (cmd == RTM_NEWNEIGH){
    rmsg->ndm_flags |= NTF_USE;
  }
  if (RW_IS_ADDR_IPV4(&neigh->address)){
    family_len = 4;
  }else{
    family_len = 16;
  }
  RW_IP_TO_SOCKADDR((rw_ip_addr_t *)&neigh->address, sockaddr);  
  rw_netlink_add_attr(nlhdr, 1024, 
                      NDA_DST, 
                      &sockaddr->sa_data,
                      family_len);
  
  rw_netlink_add_attr(nlhdr, 1024,
                      NDA_LLADDR,
                      &neigh->mac.addr[0], 6);
  rmsg->ndm_ifindex = neigh->ifindex;
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;  
}

/**
 * This function is called to add a neighbor to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_add_neigh(rw_netlink_handle_t *handle,
                                 rw_netlink_neigh_entry_t *neigh)
{
  return rw_netlink_update_neigh(handle, neigh, RTM_NEWNEIGH,
                                 NLM_F_REQUEST);
}


/**
 * This function is called to delete a neighbor to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_del_neigh(rw_netlink_handle_t *handle,
                                 rw_netlink_neigh_entry_t *neigh)
{
  return rw_netlink_update_neigh(handle, neigh, RTM_DELNEIGH,
                                 NLM_F_REQUEST);
}







/**
 * This function is called to add a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_ioctl_add_v4_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route_entry)
{
  int sockfd;
  struct rtentry route;
  struct sockaddr_in *addr;
  int err = 0;
  int ret;
  char devname[64];
  
  if (!handle->ioctl_sock_fd){
    sockfd = handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                                    AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }else{
    sockfd = handle->ioctl_sock_fd;
  }
   
  memset(&route, 0, sizeof(route));
  route.rt_dev = devname;
  addr = (struct sockaddr_in*) &route.rt_dst;
  RW_PREFIX_TO_SOCKADDRIN(&route_entry->prefix, addr);
  addr = (struct sockaddr_in*) &route.rt_genmask;
  rw_netlink_ipv4_masklen2ip(route_entry->prefix.masklen, &addr->sin_addr);
  for (ret = 0; ret < route_entry->num_nexthops; ret++){
    addr = (struct sockaddr_in*) &route.rt_gateway;
    if (RW_IP_VALID_NONZERO(&route_entry->nexthops[ret].gateway)){
      RW_IP_TO_SOCKADDRIN(&route_entry->nexthops[ret].gateway, addr);
      route.rt_flags |= RTF_GATEWAY;
    }
    strncpy(route.rt_dev, route_entry->nexthops[ret].ifname, 20);
    //            sizeof(route->rt_dev));
    route.rt_flags |= RTF_UP;
    route.rt_metric = 0;
    if ((err = ioctl(sockfd, SIOCADDRT, &route)) != 0) {
      return RW_STATUS_FAILURE;
    }  
  }
  return RW_STATUS_SUCCESS;
}


/**
 * This function is called to delete a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_ioctl_del_v4_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route_entry)
{
  int sockfd;
  struct rtentry route;
  struct sockaddr_in *addr;
  int err = 0;
  int ret;
  char devname[64];
  
  if (!handle->ioctl_sock_fd){
    sockfd = handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                                    AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }else{
    sockfd = handle->ioctl_sock_fd;
  }
   
  memset(&route, 0, sizeof(route));
  route.rt_dev = devname;
  addr = (struct sockaddr_in*) &route.rt_dst;
  RW_PREFIX_TO_SOCKADDRIN(&route_entry->prefix, addr);
  addr = (struct sockaddr_in*) &route.rt_genmask;
  rw_netlink_ipv4_masklen2ip(route_entry->prefix.masklen, &addr->sin_addr);
  for (ret = 0; ret < route_entry->num_nexthops; ret++){
    addr = (struct sockaddr_in*) &route.rt_gateway;
    if (RW_IP_VALID_NONZERO(&route_entry->nexthops[ret].gateway)){
      RW_IP_TO_SOCKADDRIN(&route_entry->nexthops[ret].gateway, addr);
      route.rt_flags |= RTF_GATEWAY;
    }
    strncpy(route.rt_dev, route_entry->nexthops[ret].ifname, 20);
    //            sizeof(route->rt_dev));
    
    route.rt_metric = 0;
    if ((err = ioctl(sockfd, SIOCDELRT, &route)) != 0) {
      return RW_STATUS_FAILURE;
    }  
  }
  return RW_STATUS_SUCCESS;
}



/**
 * This function is called to delete a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_ioctl_del_v6_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route_entry)
{
  int sockfd;
  struct in6_rtmsg route;
  int ret;


  sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sockfd < 0){
    return RW_STATUS_FAILURE;
  }
   
  memset(&route, 0, sizeof(route));

  RW_PREFIX_TO_IN6ADDR(&route_entry->prefix, &route.rtmsg_dst);
  route.rtmsg_dst_len = route_entry->prefix.masklen;
  for (ret = 0; ret < route_entry->num_nexthops; ret++){
    if (RW_IP_VALID_NONZERO(&route_entry->nexthops[ret].gateway)){
      RW_IP_TO_IN6ADDR(&route_entry->nexthops[ret].gateway, &route.rtmsg_gateway);
      route.rtmsg_flags |= RTF_GATEWAY;
    }
    route.rtmsg_ifindex = route_entry->nexthops[ret].ifindex;
    
    if ((ioctl(sockfd, SIOCDELRT, &route)) != 0) {
      close(sockfd);
      return RW_STATUS_FAILURE;
    }  
  }
  close(sockfd);
  return RW_STATUS_SUCCESS;
}

/**
 * This function is called to delete a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_ioctl_add_v6_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route_entry)
{
  int sockfd;
  struct in6_rtmsg route;
  int ret;


  sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
  if (sockfd < 0){
    return RW_STATUS_FAILURE;
  }
   
  memset(&route, 0, sizeof(route));

  RW_PREFIX_TO_IN6ADDR(&route_entry->prefix, &route.rtmsg_dst);
  route.rtmsg_dst_len = route_entry->prefix.masklen;
  for (ret = 0; ret < route_entry->num_nexthops; ret++){
    if (RW_IP_VALID_NONZERO(&route_entry->nexthops[ret].gateway)){
      RW_IP_TO_IN6ADDR(&route_entry->nexthops[ret].gateway, &route.rtmsg_gateway);
      route.rtmsg_flags |= RTF_GATEWAY;
    }
    route.rtmsg_ifindex = route_entry->nexthops[ret].ifindex;
    route.rtmsg_flags |= RTF_UP;
    if ((ioctl(sockfd, SIOCADDRT, &route)) != 0) {
      close(sockfd);
      return RW_STATUS_FAILURE;
    }  
  }
  close(sockfd);
  return RW_STATUS_SUCCESS;
}


/**************************************************************************************
 *                         Send NETLINK_GENERIC
 *************************************************************************************/                                                       
/**************************************************************************************
 *                         Send NETLINK_NETFILTER
 *************************************************************************************/                                                       

/**
* This function is called to get the socket fd of the netlink socket that was opened for
* this protocol
* @param[in] handle - netlink handle
* @param[in] proto  - protocol
* @return > 0 if fd is present
* @return 0 if no fd
*/
int
rw_netlink_get_sock_fd(rw_netlink_handle_t *handle, int proto)
{
  RW_ASSERT(handle);
  if (!handle->proto_netlink[proto]){
    return 0;
  }
  if (!handle->proto_netlink[proto]->sk){
    return 0;
  }
  return nl_socket_get_fd(handle->proto_netlink[proto]->sk);
}

/**************************************************************************************
 *                         BUNDLING
 *************************************************************************************/
/**
 * This function is called when there is no more buffer space left to bundle or
 * if the bundling is stopped
 *
 * @param[in] handle - handle
 * @param[in] proto - netlink protocol which needs to start bundling
 */
static rw_status_t rw_netlink_flush_sendbuf(rw_netlink_handle_t *handle, uint32_t proto)
{
  int ret = 0, msglen;

  
  msglen = (handle->proto_netlink[NETLINK_ROUTE]->write_buffer -
            handle->proto_netlink[NETLINK_ROUTE]->head_write_buffer);
  if (!msglen)
    return RW_STATUS_SUCCESS;
  
  ret = nl_sendto(handle->proto_netlink[NETLINK_ROUTE]->sk,
                  handle->proto_netlink[NETLINK_ROUTE]->head_write_buffer,
                  msglen);
  handle->proto_netlink[NETLINK_ROUTE]->write_buffer = 
      handle->proto_netlink[NETLINK_ROUTE]->head_write_buffer;
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  
  return RW_STATUS_SUCCESS;
}

/**
 * This fucntion is called to start bundling of messages on a particular netlink
 * socket
 * @param[in] handle - handle
 * @param[in] proto - netlink protocol which needs to start bundling
 */
void rw_netlink_start_bundle(rw_netlink_handle_t *handle, uint32_t proto)
{
  rw_netlink_handle_proto_t *proto_handle;
  proto_handle = handle->proto_netlink[proto];
  RW_ASSERT(proto_handle);

  proto_handle->bundle = 1;
}

/**
 * This fucntion is called to start bundling of messages on a particular netlink
 * socket
 * @param[in] handle - handle
 * @param[in] proto - netlink protocol which needs to start bundling
 */
void rw_netlink_stop_bundle(rw_netlink_handle_t *handle, uint32_t proto)
{
  rw_netlink_handle_proto_t *proto_handle;
  proto_handle = handle->proto_netlink[proto];
  RW_ASSERT(proto_handle);

  rw_netlink_flush_sendbuf(handle, proto);
  
  proto_handle->bundle = 0;
}


/**************************************************************************************
 *                         IOCTLS
 *************************************************************************************/
/**
 * This function is called to get the ifindex of an interface given the name
 * @param[in] handle = netlink handle
 * @param[in] name - interface name
 * @return ifindex - value > 0
 * @return 0 if failed
 */
uint32_t rw_ioctl_get_ifindex(rw_netlink_handle_t *handle,
                              const char *name)
{
  struct ifreq ifr;
  int ret;

  RW_ASSERT(name);
  RW_ASSERT(handle);
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }
  
  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, name, IFNAMSIZ);

  ret = ioctl(handle->ioctl_sock_fd, SIOCGIFINDEX, (char *)&ifr); 
  if (ret == 0){
    return ifr.ifr_ifindex;    
  }
  
  return 0;  
}




int
rw_ioctl_get_ifname(rw_netlink_handle_t *handle,
                    uint32_t ifindex, char *name, int size)
{
  struct ifreq ifr;
  int ret;
  
  RW_ASSERT(name);
  RW_ASSERT(handle);
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }
  
  memset(&ifr, 0, sizeof(struct ifreq));
  ifr.ifr_ifindex = ifindex;

  ret = ioctl(handle->ioctl_sock_fd, 
              SIOCGIFNAME, (char *)&ifr); 
  if (ret == 0){
    strncpy(name, ifr.ifr_name, size);
  }
  
  return ret;  
}


/**
 * This function is called to add an remote address to the link
 * @param[in]  name - interface name to add address
 * @param[in]  ifindex - interface ifindex to add address
 * @param[in]  address - address to add
 * @param[in]  name - masklen to add
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_remote_address_add(rw_netlink_handle_t *handle,
                                                  const char *name,
                                                  uint32_t ifindex,
                                                  rw_ip_addr_t *remote)
{
  int ret;
  
  if (RW_IS_ADDR_IPV4(remote)){
    struct ifreq ifr;
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;

    RW_ASSERT(name);
    RW_ASSERT(handle);
    if (!handle->ioctl_sock_fd){
      handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                             AF_INET, SOCK_DGRAM, 0);
      if (handle->ioctl_sock_fd <= 0){
        handle->ioctl_sock_fd = 0;
        return RW_STATUS_FAILURE;
      }
    }
    
    memset(&ifr, 0, sizeof(struct ifreq));
    
    if (ifindex) {
      ifr.ifr_ifindex = ifindex;
    }
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    
    RW_IP_TO_SOCKADDRIN(remote, addr);
    
    ret = ioctl(handle->ioctl_sock_fd,
                SIOCSIFDSTADDR, (char *)&ifr); 
    if (ret){
      return RW_STATUS_FAILURE;
    }
  }else if (RW_IS_ADDR_IPV6(remote)){
    struct in6_ifreq ireq;
    int fd;
    
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0){
      return RW_STATUS_FAILURE;
    }
        
    memset(&ireq, 0, sizeof(ireq));
    RW_ASSERT(ifindex);
    ireq.ifr6_ifindex = ifindex;
    RW_IP_TO_IN6ADDR(remote, &ireq.ifr6_addr);
    ret = ioctl(fd,
                SIOCSIFDSTADDR, (char *)&ireq); 
    if (ret){
      close(fd);
      return RW_STATUS_FAILURE;
    }
    close(fd);
  }
  return RW_STATUS_SUCCESS;
}

/**
 * This function is called to add an address and mask to an interface.
 * @param[in]  name - interface name to add address
 * @param[in]  ifindex - interface ifindex to add address
 * @param[in]  address - address to add
 * @param[in]  name - masklen to add
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_address_add(rw_netlink_handle_t *handle,
                                           const char *name, uint32_t ifindex,
                                           rw_ip_prefix_t *prefix)
{
  int ret;
  
  if (RW_IS_ADDR_IPV4(prefix)){
    struct ifreq ifr;
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    struct sockaddr_in *mask = (struct sockaddr_in *)&ifr. ifr_netmask;

    RW_ASSERT(handle);
    if (!handle->ioctl_sock_fd){
      handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                             AF_INET, SOCK_DGRAM, 0);
      if (handle->ioctl_sock_fd <= 0){
        handle->ioctl_sock_fd = 0;
        return RW_STATUS_FAILURE;
      }
    }
    
    memset(&ifr, 0, sizeof(struct ifreq));
    
    if (ifindex) {
      ifr.ifr_ifindex = ifindex;
    }
    if (name){
      strncpy(ifr.ifr_name, name, IFNAMSIZ);
    }
    RW_PREFIX_TO_SOCKADDRIN(prefix, addr);
    
    ret = ioctl(handle->ioctl_sock_fd, SIOCSIFADDR, (char *)&ifr); 
    if (ret == 0){
      memset(mask, 0, sizeof(struct sockaddr_in));
      mask->sin_family = AF_INET;
      rw_netlink_ipv4_masklen2ip(prefix->masklen, &mask->sin_addr);
      ret = ioctl(handle->ioctl_sock_fd, SIOCSIFNETMASK, (char *)&ifr); 
    }
    if (ret)
      return RW_STATUS_FAILURE;
  }else if (RW_IS_ADDR_IPV6(prefix)){
    struct in6_ifreq ireq;
    
    if (!handle->ioctl6_sock_fd){
      handle->ioctl6_sock_fd = rwnetns_socket(handle->namespace_name,
                                              AF_INET6, SOCK_DGRAM, 0);
      if (handle->ioctl6_sock_fd <= 0){
        handle->ioctl6_sock_fd = 0;
        return RW_STATUS_FAILURE;
      }
    }
        
    memset(&ireq, 0, sizeof(ireq));
    RW_ASSERT(ifindex);
    ireq.ifr6_ifindex = ifindex;
    RW_PREFIX_TO_IN6ADDR(prefix, &ireq.ifr6_addr);
    
    ireq.ifr6_prefixlen = prefix->masklen;
    
    ret = ioctl(handle->ioctl6_sock_fd, SIOCSIFADDR, (char *)&ireq); 
    if (ret){
      return RW_STATUS_FAILURE;
    }
  }
  return RW_STATUS_SUCCESS;
}

/**
 * This function is called to bring an interface up given an ifname and ifindex. If
 * the ifindex is not given then an ioctl is done to get the ifindex and the ifindex is
 * returned. On failure 0 is returned.
 *
 * @param[in]  name - interface name to bring up
 * @param[in]  ifindex - interface ifindex to bring up
 * @returns 0 if failed
 * @returns ifindex if success
 */
uint32_t rw_ioctl_interface_up(rw_netlink_handle_t *handle,
                               const char *name, uint32_t ifindex)
{
  struct ifreq ifr;
  int ret;
  
  if (!ifindex){
    ifindex = rw_ioctl_get_ifindex(handle, name);
    if (!ifindex){
      return 0;
    }
  }
  RW_ASSERT(handle);
  
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return 0;
    }
  }
  
  memset(&ifr, 0, sizeof ifr);
  ifr.ifr_ifindex = ifindex;
  strncpy(ifr.ifr_name, name, IFNAMSIZ);
  
  ret = ioctl(handle->ioctl_sock_fd, SIOCGIFFLAGS, &ifr);
  if (ret) {
    return 0;
  }
  
  ifr.ifr_flags &= ~(IFF_UP);
  ifr.ifr_flags |=  IFF_UP;
  
  ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
  if (ret){
    return 0;
  }
  return ifindex;
}


/**
 * This function is called to bring an interface down given an ifname and ifindex.
 *
 * @param[in]  name - interface name to bring up
 * @param[in]  ifindex - interface ifindex to bring up
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_down(rw_netlink_handle_t *handle,
                                    const char *name, uint32_t ifindex)
{
  struct ifreq ifr;
  int ret;
  
  if (!ifindex){
    ifindex = rw_ioctl_get_ifindex(handle, name);
    if (!ifindex){
      return RW_STATUS_FAILURE;
    }
  }
  RW_ASSERT(handle);
  
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }
  memset(&ifr, 0, sizeof ifr);
  ifr.ifr_ifindex = ifindex;
  strncpy(ifr.ifr_name, name, IFNAMSIZ);
  ret = ioctl(handle->ioctl_sock_fd, SIOCGIFFLAGS, &ifr);
  if (ret) {
    return RW_STATUS_FAILURE;
  }
  
  ifr.ifr_flags &= ~(IFF_UP);
    
  ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
  if (ret)
    return RW_STATUS_FAILURE;
  
  return RW_STATUS_SUCCESS;
}

/**
 * This functiuon is called to set the mac address of a kernel interface
 */
rw_status_t rw_ioctl_set_mac(rw_netlink_handle_t *handle,
                             const char *name, uint32_t ifindex,
                             rw_mac_addr_t *mac)
{
  struct ifreq ifr;
  int ret;
  int up = 0;
  

  RW_ASSERT(handle);
  
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }
  if (!ifindex){
    ifindex = rw_ioctl_get_ifindex(handle, name);
    if (!ifindex){
      return RW_STATUS_FAILURE;
    }
  }
  
  memset(&ifr, 0, sizeof ifr);
  ifr.ifr_ifindex = ifindex;
  strncpy(ifr.ifr_name, name, IFNAMSIZ);
  
  ret = ioctl(handle->ioctl_sock_fd, SIOCGIFFLAGS, &ifr);
  if (ret) {
    return RW_STATUS_FAILURE;
  }
  if (ifr.ifr_flags & IFF_UP){
    up = 1;
    ifr.ifr_flags &= ~(IFF_UP);
    
    ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
    if (ret){
      return RW_STATUS_FAILURE;
    }
  }
  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  memcpy(&ifr.ifr_hwaddr.sa_data, mac->addr, 6); //AKKI define this.
  ret = ioctl(handle->ioctl_sock_fd, SIOCSIFHWADDR, &ifr);
  if (ret){
    return RW_STATUS_FAILURE;
  }

  if (up){
    ifr.ifr_flags |=  IFF_UP;
    
    ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
    if (ret) {
      return RW_STATUS_FAILURE;
    }    
  }
  return RW_STATUS_SUCCESS;
}



/**
 * This functiuon is called to set the mac address of a kernel interface
 */
rw_status_t rw_ioctl_set_mtu(rw_netlink_handle_t *handle,
                             const char *name, uint32_t ifindex,
                             uint32_t mtu)
{
  struct ifreq ifr;
  int ret;

  RW_ASSERT(handle);
  
  if (!handle->ioctl_sock_fd){
    handle->ioctl_sock_fd = rwnetns_socket(handle->namespace_name,
                                           AF_INET, SOCK_DGRAM, 0);
    if (handle->ioctl_sock_fd <= 0){
      handle->ioctl_sock_fd = 0;
      return RW_STATUS_FAILURE;
    }
  }
  if (!ifindex){
    ifindex = rw_ioctl_get_ifindex(handle, name);
    if (!ifindex){
      return RW_STATUS_FAILURE;
    }
  }
  
  memset(&ifr, 0, sizeof ifr);
  ifr.ifr_ifindex = ifindex;
  strncpy(ifr.ifr_name, name, IFNAMSIZ);
#if 0
  int up = 0;
  ret = ioctl(handle->ioctl_sock_fd, SIOCGIFFLAGS, &ifr);
  if (ret) {
    return RW_STATUS_FAILURE;
  }
  if (ifr.ifr_flags & IFF_UP){
    up = 1;
    ifr.ifr_flags &= ~(IFF_UP);
    
    ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
    if (ret){
      return RW_STATUS_FAILURE;
    }
  }
#endif
  ifr.ifr_mtu = mtu;
  ret = ioctl(handle->ioctl_sock_fd, SIOCSIFMTU, &ifr);
  if (ret){
    return RW_STATUS_FAILURE;
  }
#if 0
  if (up){
    ifr.ifr_flags |=  IFF_UP;
    
    ret = ioctl(handle->ioctl_sock_fd, SIOCSIFFLAGS, &ifr);
    if (ret) {
      return RW_STATUS_FAILURE;
    }    
  }
#endif
  return RW_STATUS_SUCCESS;
}




int
rw_ioctl_create_tun_device(rw_netlink_handle_t *handle,
                           const char *name)
{
  struct ifreq ethreq;
  int fd;
  int flags=0;
  
  fd = rwnetns_open(handle->namespace_name,
                    "/dev/net/tun", O_RDWR);
  if (fd <= 0){
    return 0;
  }
  
  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, name, IFNAMSIZ);
  ethreq.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (ioctl(fd,TUNSETIFF, &ethreq) != 0) {
    close(fd);
    return 0;
  }
  flags = fcntl(fd, F_GETFL, 0); 
  if (flags == -1 ) {
    close(fd);
    return 0;
  }
  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1 ) {
    close(fd);
    return 0;
  }
  
  return fd;
}




static void
rw_netlink_convert_tunnel_param(rw_netlink_tunnel_params_t *tparam,
                                struct ip_tunnel_parm *p)
{
  p->iph.version = 4;
  p->iph.ihl = 5;
#ifndef IP_DF
#define IP_DF		0x4000		/* Flag: "Don't Fragment"	*/
#endif
  p->iph.frag_off = htons(IP_DF);

  switch(tparam->type){
    case RW_NETLINK_TUNNEL_TYPE_GRE:
      p->iph.protocol = IPPROTO_GRE;
      if (RW_IP_VALID_NONZERO(&tparam->local)){
        p->iph.saddr = htonl(RW_IPV4_ADDR(&tparam->local));
      }
      
      if (RW_IP_VALID_NONZERO(&tparam->remote)){
        p->iph.daddr = htonl(RW_IPV4_ADDR(&tparam->remote));
      }
      if (tparam->key){
        p->i_flags |= GRE_KEY;
        p->i_key = tparam->key;
      }
      if (tparam->okey){
        p->o_flags |= GRE_KEY;
        p->o_key = tparam->okey;
      }
      /*
      	if (p->i_key == 0 && IN_MULTICAST(ntohl(p->iph.daddr))) {
        p->i_key = p->iph.daddr;
        p->i_flags |= GRE_KEY;
	}
	if (p->o_key == 0 && IN_MULTICAST(ntohl(p->iph.daddr))) {
        p->o_key = p->iph.daddr;
        p->o_flags |= GRE_KEY;
	}
      */
      break;
    case RW_NETLINK_TUNNEL_TYPE_SIT:
      p->iph.protocol = IPPROTO_IPV6;
      break;
    case RW_NETLINK_TUNNEL_TYPE_IPIP:
      p->iph.protocol = IPPROTO_IPIP;
     break;
  }
  strncpy(p->name, tparam->tunnelname, IFNAMSIZ);
  if (tparam->phy_ifindex){
    p->link = tparam->phy_ifindex;
  }
  if (tparam->iflags){
    p->i_flags |= tparam->iflags;
  }
  if (tparam->oflags){
    p->o_flags |= tparam->oflags;
  }
  if (tparam->ttl){
    p->iph.ttl = tparam->ttl;
  }
  if (tparam->tos){
    p->iph.tos =tparam->tos;
  }

  
  if (p->iph.protocol == IPPROTO_IPIP || p->iph.protocol == IPPROTO_IPV6) {
    if ((p->i_flags & GRE_KEY) || (p->o_flags & GRE_KEY)) {
      RW_CRASH();
    }
  }
  
  return;
}

rw_status_t
rw_ioctl_create_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam)
{
  int ret = 0;
  struct ip_tunnel_parm p;
  struct ifreq ifr;
  int fd;
  rw_status_t status = RW_STATUS_SUCCESS;
  
  memset(&ifr, 0, sizeof(ifr));
  
  memset(&p, 0, sizeof(p));
  ifr.ifr_ifru.ifru_data = &p;
  rw_netlink_convert_tunnel_param(tparam, &p);
  
  switch(tparam->type){
    case RW_NETLINK_TUNNEL_TYPE_GRE:
      strncpy(ifr.ifr_name, "gre0",IFNAMSIZ);
      fd = handle->ioctl_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_SIT:
      strncpy(ifr.ifr_name, "sit0",IFNAMSIZ);
      fd = handle->ioctl6_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_IPIP:
      strncpy(ifr.ifr_name, "tunl0",IFNAMSIZ);
      fd = handle->ioctl_sock_fd;
      break;
  }
  if (fd <= 0){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
  ret  = ioctl(fd, SIOCADDTUNNEL, &ifr);
  if (ret){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
ret:

  return status;
}

rw_status_t
rw_ioctl_change_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam)
{
  int fd;
  int ret = 0;
  struct ip_tunnel_parm p;
  struct ifreq ifr;
  rw_status_t status;
  
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, tparam->tunnelname, IFNAMSIZ);
  ifr.ifr_ifru.ifru_data = &p;
  memset(&p, 0, sizeof(p));
  rw_netlink_convert_tunnel_param(tparam, &p);
  
  switch(tparam->type){
    case RW_NETLINK_TUNNEL_TYPE_GRE:
      fd = handle->ioctl_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_SIT:
      fd = handle->ioctl6_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_IPIP:
      fd = handle->ioctl_sock_fd;
      break;
  }
  if (fd <= 0){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
   ret  = ioctl(fd, SIOCCHGTUNNEL, &ifr);
  if (ret){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
ret:

  return status;
}


rw_status_t
rw_ioctl_delete_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam)
{
  int fd;
  int ret = 0;
  struct ip_tunnel_parm p;
  struct ifreq ifr;
  rw_status_t status;
  
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, tparam->tunnelname, IFNAMSIZ);
  ifr.ifr_ifru.ifru_data = &p;
  memset(&p, 0, sizeof(p));
  rw_netlink_convert_tunnel_param(tparam, &p);
  
  switch(tparam->type){
    case RW_NETLINK_TUNNEL_TYPE_GRE:
      fd = handle->ioctl_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_SIT:
      fd = handle->ioctl6_sock_fd;
      break;
    case RW_NETLINK_TUNNEL_TYPE_IPIP:
      fd = handle->ioctl_sock_fd;
      break;
  }
  if (fd <= 0){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
  ret  = ioctl(fd, SIOCDELTUNNEL, &ifr);
  if (ret){
    status =  RW_STATUS_FAILURE;
    goto ret;
  }
ret:

  return status;
}

static rw_status_t
rw_netlink_update_tunnel_device(rw_netlink_handle_t *handle,
                                rw_netlink_tunnel_params_t *tparam, int cmd,
                                uint32_t nl_flags)
{
  struct nlmsghdr *nlhdr;
  struct ifinfomsg *ifi;
  int ret = 0;
  struct nl_sock *sk;
  struct rtattr *kinfo, *info;
  char kind[6];
  
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    return RW_STATUS_FAILURE;
  }
  sk = handle->proto_netlink[NETLINK_ROUTE]->sk;
  if (!sk){
    return RW_STATUS_FAILURE;
  }
  nlhdr = (struct nlmsghdr *)handle->proto_netlink[NETLINK_ROUTE]->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_flags = NLM_F_REQUEST|nl_flags;
  nlhdr->nlmsg_type = cmd;

  ifi = (struct ifinfomsg *)rw_netlink_add_base(nlhdr, sizeof(*ifi), 0);
  memset(ifi, 0, sizeof(*ifi));
  rw_netlink_add_attr(nlhdr, 1024,
                      IFLA_IFNAME, tparam->tunnelname, strlen(tparam->tunnelname)+1);
  switch(tparam->type){
    case RW_NETLINK_TUNNEL_TYPE_GRE:
      ifi->ifi_family = AF_INET;
      ifi->ifi_index = 0;//do we need to get the gre0??
      if (cmd == RTM_NEWLINK){
        info = rw_netlink_nested_attr_start(nlhdr, IFLA_LINKINFO);
        
        sprintf(kind, "gre");
        rw_netlink_add_attr(nlhdr, 1024, //AKKI fix this
                            IFLA_INFO_KIND,
                            kind, strlen(kind)+1);
        kinfo = rw_netlink_nested_attr_start(nlhdr, IFLA_INFO_DATA);
        if (tparam->phy_ifindex){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_LINK, &tparam->phy_ifindex, 4);
        }
        if (tparam->iflags){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_IFLAGS, &tparam->iflags, sizeof(tparam->iflags));
        }
        if (tparam->oflags){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_OFLAGS, &tparam->oflags, sizeof(tparam->oflags));
        }
        if (tparam->key){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_IKEY, &tparam->key, 4);
        }
        if (tparam->okey){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_OKEY, &tparam->okey, 4);
        }
        if (RW_IP_VALID_NONZERO(&tparam->local)){
          uint32_t local = htonl(RW_IPV4_ADDR(&tparam->local));
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_LOCAL, (&local), 4);
        }
        if (RW_IP_VALID_NONZERO(&tparam->remote)){
          uint32_t remote =htonl( RW_IPV4_ADDR(&tparam->remote));
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_REMOTE, (&remote), 4);
        }
        if (tparam->ttl){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_TTL, &tparam->ttl, 1);
        }
        if (tparam->tos){
          rw_netlink_add_attr(nlhdr, 1024,
                              IFLA_GRE_TOS, &tparam->tos, 1);
        }
        rw_netlink_nested_attr_end(nlhdr, kinfo);
        rw_netlink_nested_attr_end(nlhdr, info);
      }
      break;
    case RW_NETLINK_TUNNEL_TYPE_SIT:
      return RW_STATUS_SUCCESS;
      break;
    case RW_NETLINK_TUNNEL_TYPE_IPIP:
      return RW_STATUS_SUCCESS;
      break;
  }
  
  ret = nl_sendto(sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    return RW_STATUS_FAILURE;
  }
  return RW_STATUS_SUCCESS;  
  
  /*
    err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache);
    if ( err < 0) {
    return RW_STATUS_FAILURE;
    }
    
    if_index = rtnl_link_name2i(link_cache, "eno16777736");
    if (!if_index) {
    fprintf(stderr, "Unable to lookup eno16777736");
    return -1;
    }
    link = rtnl_link_ipgre_alloc();
    if(!link) {
    return RW_STATUS_FAILURE;
    }
    rtnl_link_set_name(link, tparam->tunnelname);
    if (tparam->phy_ifindex){
    rtnl_link_ipgre_set_link(link, tparam->phy_ifindex);
    }
    
    inet_pton(AF_INET, "192.168.254.12", &local.s_addr);
    rtnl_link_ipgre_set_local(link, local.s_addr);
    
    inet_pton(AF_INET, "192.168.254.13", &remote.s_addr);
    rtnl_link_ipgre_set_remote(link, remote.s_addr);
    
    rtnl_link_ipgre_set_ttl(link, 64);
    err = rtnl_link_add(sk, link, NLM_F_CREATE);
    if (err < 0) {
    nl_perror(err, "Unable to add link");
    return err;
    }
    rtnl_link_put(link);
  */
}


rw_status_t
rw_netlink_change_tunnel_device(rw_netlink_handle_t *handle,
                                rw_netlink_tunnel_params_t *tparam)
{
  //akki first send using newlink and if it fails send with setlink
  /*
    err = nl_send_auto_complete(sk, msg);
    if (err < 0)
    goto errout;
    
    err = wait_for_ack(sk);
    if (err == -NLE_OPNOTSUPP && msg->nm_nlh->nlmsg_type == RTM_NEWLINK) {
    msg->nm_nlh->nlmsg_type = RTM_SETLINK;
    goto retry;
    }
  */
  return rw_netlink_update_tunnel_device(handle, tparam, RTM_SETLINK, 0);
}

rw_status_t
rw_netlink_delete_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam)
{
  return rw_netlink_update_tunnel_device(handle, tparam, RTM_DELLINK, 0);
}


rw_status_t
rw_netlink_create_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam)
{
  return rw_netlink_update_tunnel_device(handle, tparam, RTM_NEWLINK, NLM_F_CREATE);
}


int
rw_ioctl_create_tap_device(rw_netlink_handle_t *handle,
                           const char *name)
{
  struct ifreq ethreq;
  int fd;
  int flags=0;
  
  fd = rwnetns_open(handle->namespace_name,
                    "/dev/net/tun", O_RDWR);
  if (fd <= 0){
    return 0;
  }
  
  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, name, IFNAMSIZ);
  ethreq.ifr_flags = IFF_TAP | IFF_NO_PI;
  if (ioctl(fd,TUNSETIFF, &ethreq) != 0) {
    close(fd);
    return 0;
  }
  
  flags = fcntl(fd, F_GETFL, 0); 
  if (flags == -1 ) {
    close(fd);
    return 0;
  }

  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1 ) {
    close(fd);
    return 0;
  }
  
  return fd;
}



struct rw_netlink_if_info_get{
  rw_ip_addr_t *ip;
  rw_netlink_link_entry_t *if_info;
};

static int rw_netlink_check_interface_address(rw_netlink_handle_proto_t *proto_handle,
                                              struct nlmsghdr *h, void *arg)
{
  struct rw_netlink_if_info_get *info = (struct rw_netlink_if_info_get *)arg;
  int len;
  struct ifaddrmsg *ifa= NLMSG_DATA(h);
  struct rtattr *tb[IFA_MAX + 1];
  struct sockaddr_in6 skin6;
  struct sockaddr *sockaddr = (struct sockaddr *)&skin6;
  rw_ip_addr_t gotip;
  
  if (h->nlmsg_type != RTM_NEWADDR && h->nlmsg_type != RTM_DELADDR){
    return 0;
  }
  len = h->nlmsg_len - NLMSG_LENGTH (sizeof (struct ifaddrmsg));
  if (len < 0){
    return -1;
  }
  memset (tb, 0, sizeof tb);
  rw_parse_rtattr(tb,IFA_MAX, IFA_RTA(ifa),
                  len - NLMSG_LENGTH(sizeof(*ifa)));
  if (tb[IFA_LOCAL]){
    sockaddr->sa_family = ifa->ifa_family;
    memcpy(&sockaddr->sa_data,
           RTA_DATA(tb[IFA_LOCAL]),
           RTA_PAYLOAD(tb[IFA_LOCAL]));
    RW_IP_FROM_SOCKADDR(&gotip,sockaddr);

    if (rw_ip_addr_cmp(info->ip, &gotip) == 0){
      //fill the ifinfo
      info->if_info->ifindex = ifa->ifa_index;
    }
  }


  return 0;
}

rw_status_t rw_netlink_get_intf_info_from_ip(rw_netlink_handle_t *handle,
                                             rw_ip_addr_t *ip,
                                             rw_netlink_link_entry_t *info)
{
  struct nlmsghdr *nlhdr;
  struct rtgenmsg *g;
  rw_netlink_handle_proto_t *proto_handle;
  int ret;
  struct rw_netlink_if_info_get infoget;
  rw_status_t status = RW_STATUS_SUCCESS;
  
  infoget.ip = ip;
  infoget.if_info = info;
  
  memset(info, 0, sizeof(*info));
  proto_handle = rw_netlink_handle_proto_init_internal(handle, NETLINK_ROUTE,
                                                       RW_NETLINK_LISTEN);
  
  if (!proto_handle){
    status = RW_STATUS_FAILURE;
    goto ret;
  }
  
  nlhdr = (struct nlmsghdr *)proto_handle->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_type = RTM_GETADDR;
  nlhdr->nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  
  g  = (struct rtgenmsg *)rw_netlink_add_base(nlhdr, sizeof(*g), 0);
  if (RW_IS_ADDR_IPV4(ip)) {
    g->rtgen_family = AF_INET;
  }else{
    g->rtgen_family = AF_INET6;
  }
  ret = nl_sendto(proto_handle->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto ret;
  }

  /*Now start parsing the responses from the kernel*/
  status = rw_netlink_read_ow(proto_handle, 1, &infoget,
                              rw_netlink_check_interface_address);
  if (status != RW_STATUS_SUCCESS){
    goto ret;
  }
  if (!info->ifindex){
    status = RW_STATUS_FAILURE;
    goto ret;
  }
  ret = rw_ioctl_get_ifname(handle, info->ifindex,
                            info->ifname, RW_NETLINK_IFNAMSIZ);
  if (ret){
    status = RW_STATUS_FAILURE;
    goto ret;
  }
  status = RW_STATUS_SUCCESS;
  
ret:
  if (proto_handle){
    rw_netlink_handle_proto_terminate(proto_handle);
  }
  return ret;
}





rw_status_t
rw_netlink_get_link(rw_netlink_handle_t *handle)
{
  struct nlmsghdr *nlhdr;
  struct rtgenmsg *g;
  rw_netlink_handle_proto_t *proto_handle;
  int ret;
  rw_status_t status = RW_STATUS_SUCCESS;
  
  proto_handle = handle->proto_netlink[NETLINK_ROUTE];
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    handle->proto_netlink[NETLINK_ROUTE] = proto_handle =
        rw_netlink_handle_proto_init_internal(handle,
                                              NETLINK_ROUTE,
                                              RW_NETLINK_CONNECT);
  }
  if (!proto_handle){
    status = RW_STATUS_FAILURE;
    goto ret;
  }
  
  nlhdr = (struct nlmsghdr *)proto_handle->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_type = RTM_GETLINK;
  nlhdr->nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  
  g  = (struct rtgenmsg *)rw_netlink_add_base(nlhdr, sizeof(*g), 0);
  g->rtgen_family = AF_INET;
  //g->rtgen_family = AF_INET6;
  
  ret = nl_sendto(proto_handle->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto ret;
  }

ret:
  return status;
}


rw_status_t
rw_netlink_get_link_addr(rw_netlink_handle_t *handle)
{
  struct nlmsghdr *nlhdr;
  struct rtgenmsg *g;
  rw_netlink_handle_proto_t *proto_handle;
  int ret;
  rw_status_t status = RW_STATUS_SUCCESS;
  
  proto_handle = handle->proto_netlink[NETLINK_ROUTE];
  if (!handle->proto_netlink[NETLINK_ROUTE]){
    handle->proto_netlink[NETLINK_ROUTE] = proto_handle =
        rw_netlink_handle_proto_init_internal(handle,
                                              NETLINK_ROUTE,RW_NETLINK_CONNECT);
  }
  if (!proto_handle){
    status = RW_STATUS_FAILURE;
    goto ret;
  }
  
  nlhdr = (struct nlmsghdr *)proto_handle->write_buffer;
  memset(nlhdr, 0, sizeof(*nlhdr));
  nlhdr->nlmsg_len = NLMSG_ALIGN(NLMSG_HDRLEN);
  nlhdr->nlmsg_type = RTM_GETADDR;
  nlhdr->nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  
  g  = (struct rtgenmsg *)rw_netlink_add_base(nlhdr, sizeof(*g), 0);
  g->rtgen_family = AF_INET;
  //g->rtgen_family = AF_INET6;
  
  ret = nl_sendto(proto_handle->sk,
                  nlhdr, nlhdr->nlmsg_len);
  if (ret < 0){
    RW_CRASH();
    status = RW_STATUS_FAILURE;
    goto ret;
  }

ret:
  return status;
}
