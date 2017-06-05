/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_virt_eth_switch.c
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 06/12/2014
 * @simple virtual ethernet switch for PMD eth_sim ports
 *
 * @details
 */

#include <sys/queue.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_rwlock.h>
#include <arpa/inet.h>
#include <rte_jhash.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_ether.h>
#include <rte_byteorder.h>
#include "rw_pmd_eth_sim.h"
#include "rw_virt_eth_switch.h"


/*
 * list of ports
 */
TAILQ_HEAD(ves_ports_head, rw_ves_port) rw_ves_port_list;
uint8_t  rw_ves_initialized = 0;
rw_ves_ports_hash_list_t rw_ves_port_hash_table[RW_VES_NUM_PORTS_HASH_BUCKETS];


void rw_ves_init(void)
{
  uint32_t i;

//write lock TBD ???

  if (!rw_ves_initialized) {
    TAILQ_INIT(&(rw_ves_port_list));
    for (i=0; i<sizeof(rw_ves_port_hash_table)/sizeof(rw_ves_port_hash_table[0]); i++) {
      TAILQ_INIT(&(rw_ves_port_hash_table[i].ports_list));
    }
    rw_ves_initialized = 1;
  }
}

int
rw_ves_add_port(uint64_t port_iden,
                struct ether_addr *mac_addr, int num_rx_queues,
                struct eth_sim_rx_queue *rx_rng_queues)
{
  struct rw_ves_port *ves_port, *t = NULL;
  rw_ves_ports_hash_list_t *p;
  uint32_t h;

  ves_port = rte_zmalloc_socket(NULL, sizeof(*ves_port), 0, rte_socket_id());
  if (NULL == ves_port) {
    return -1;
  }
  ves_port->port_iden = port_iden;
  ves_port->mac_addr = *mac_addr;
  ves_port->num_rx_queues = num_rx_queues;
  ves_port->rx_rng_queues = rx_rng_queues;
  ves_port->rss_enabled = 1;

/* locking TBD ??? */

  h = RW_VES_GET_HASH_INDEX(&(mac_addr->addr_bytes[0]), sizeof(mac_addr->addr_bytes)); 
  p = &(rw_ves_port_hash_table[h]);
  TAILQ_FOREACH(t, &(p->ports_list), next_hash) {
    if (memcmp(&(t->mac_addr.addr_bytes[0]), &(mac_addr->addr_bytes[0]), sizeof(mac_addr->addr_bytes)) == 0) {
      break;
    }
  }
  if (t) {
    TAILQ_REMOVE(&(p->ports_list), t, next_hash);
    TAILQ_REMOVE(&(rw_ves_port_list), t, next_element);
  }
  TAILQ_INSERT_TAIL(&(p->ports_list), ves_port, next_hash); 

  TAILQ_INSERT_TAIL(&(rw_ves_port_list), ves_port, next_element);

  return 0;
}


int
rw_ves_del_port(uint64_t port_iden __rte_unused,
                struct ether_addr *mac_addr)
{
  struct rw_ves_port *t = NULL;
  rw_ves_ports_hash_list_t *p;
  uint32_t h;
  
  h = RW_VES_GET_HASH_INDEX(&(mac_addr->addr_bytes[0]), sizeof(mac_addr->addr_bytes)); 
  p = &(rw_ves_port_hash_table[h]);
  TAILQ_FOREACH(t, &(p->ports_list), next_hash) {
    if (memcmp(&(t->mac_addr.addr_bytes[0]), &(mac_addr->addr_bytes[0]), sizeof(mac_addr->addr_bytes)) == 0) {
      break;
    }
  }
  if (t) {
    TAILQ_REMOVE(&(p->ports_list), t, next_hash);
  }
  return 0;
}


static inline uint16_t rw_ves_rss_hash(struct rw_ves_port *t,
                                       struct ether_hdr *eth)
{
  uint16_t indx, eth_type;
  struct ipv4_hdr *ipv4;
  struct ipv6_hdr *ipv6;
  struct udp_hdr *udp_header;
  struct tcp_hdr *tcp_header;
  uint32_t port;
  uint8_t  l2_len;

  eth_type = rte_be_to_cpu_16(eth->ether_type);
  l2_len  = sizeof(struct ether_hdr);
  if (eth_type == ETHER_TYPE_VLAN) { /* Only single VLAN label supported here */
    l2_len  += sizeof(struct vlan_hdr);
    eth_type = rte_be_to_cpu_16(*(uint16_t *) ((uintptr_t)&eth->ether_type +
                                  sizeof(struct vlan_hdr)));
  }

  switch (eth_type) {
    case ETHER_TYPE_IPv4:
      ipv4 = (struct ipv4_hdr *)  (((uint8_t *)eth)+l2_len);
      if (!ipv4->fragment_offset) {
        switch(ipv4->next_proto_id) {
          case IPPROTO_UDP:
            udp_header = (struct udp_hdr *) ((uint8_t *)ipv4 + ((ipv4->version_ihl &0x0F) *4));
            port  = udp_header->src_port; /* not doing ntohs anyway hash input */
            port  = (port<<16)|udp_header->dst_port;
            indx = rte_jhash_3words(ipv4->src_addr,ipv4->dst_addr,port,IPPROTO_UDP);
            break;
          case IPPROTO_TCP:
            tcp_header = (struct tcp_hdr *) ((uint8_t *)ipv4 + ((ipv4->version_ihl &0x0F) *4));
            port  = tcp_header->src_port;
            port  = (port<<16)|tcp_header->dst_port;
            indx = rte_jhash_3words(ipv4->src_addr,ipv4->dst_addr,port,IPPROTO_TCP);
            break;
          default:
            indx = rte_jhash2(&(ipv4->src_addr),2,0);
        }
      }
      else {
        indx = rte_jhash2(&(ipv4->src_addr),2,0);
      }
    break;
    case ETHER_TYPE_IPv6:
      ipv6 = (struct ipv6_hdr *)   (((uint8_t *)eth)+l2_len);
      switch(ipv6->proto) {
        case IPPROTO_UDP:
          udp_header = (struct udp_hdr *) (ipv6+1);
          port  = udp_header->src_port;
          port  = (port<<16)|udp_header->dst_port;
          indx = rte_jhash(ipv6->src_addr, sizeof(ipv6->src_addr), IPPROTO_UDP);
          indx = rte_jhash(ipv6->dst_addr, sizeof(ipv6->dst_addr), indx);
          indx = rte_jhash_1word(port,indx);
        break;
        case IPPROTO_TCP:
          tcp_header = (struct tcp_hdr *) (ipv6 +1);
          port  = tcp_header->src_port;
          port  = (port<<16)|tcp_header->dst_port;
          indx = rte_jhash(ipv6->src_addr, sizeof(ipv6->src_addr), IPPROTO_TCP);
          indx = rte_jhash(ipv6->dst_addr, sizeof(ipv6->dst_addr), indx);
          indx = rte_jhash_1word(port,indx);
        break;
        default:
          indx = rte_jhash(ipv6->src_addr, sizeof(ipv6->src_addr), 0);
          indx = rte_jhash(ipv6->dst_addr, sizeof(ipv6->dst_addr), indx);
      }
    break;
    default:
      indx = 0;
  }
  
  return indx%(t->num_rx_queues);
}


int
rw_ves_switch_pkt(uint64_t port_iden, struct rte_mbuf * pkt,
                  struct eth_sim_tx_queue *src_t)
{
  struct ether_hdr *eth;
  struct rw_ves_port *t = NULL;
  rw_ves_ports_hash_list_t *p;
  uint32_t h;
  uint16_t *b;
  int ret = -1;
  uint16_t idx = 0;

  /* readlock - TBD */

  eth = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

   
  b = (uint16_t *)(&(eth->d_addr.addr_bytes[0]));
  //Still need to add ipv4 mac multicast address
  //01:00:5E:00:00:00 - 01:00:5E:7F:FF:FF
  if ((b[0] == 0xffff && b[1] == 0xffff && b[2] == 0xffff) ||
      (b[0] == 0x3333 && (ntohs(eth->ether_type) == 0x86dd)) ||
      (htons(b[0]) == 0x0180 && htons(b[1]) == 0xc200 && htons(b[2]) == 0x0002)){
    /* bcast - duplicate packet and send to all ports */
    ret = 0;
    src_t->tx_pkts.cnt ++;
    src_t->tx_bytes.cnt += rte_pktmbuf_pkt_len(pkt);
    
    TAILQ_FOREACH(t, &(rw_ves_port_list), next_element) {
      if (port_iden != t->port_iden) {
        struct rte_mbuf *copy;
        copy = NULL;
        idx = t->rss_enabled?rw_ves_rss_hash(t, eth):0;
        if (t->rx_rng_queues[idx].mb_pool) {
          copy = rte_pktmbuf_duplicate(pkt, t->rx_rng_queues[idx].mb_pool);
        }
        if (copy) {
          switch (ntohs(eth->ether_type)) {
            case ETHER_TYPE_IPv4: copy->ol_flags |= PKT_RX_IPV4_HDR; break;
            case ETHER_TYPE_IPv6: copy->ol_flags |= PKT_RX_IPV6_HDR; break;
            case ETHER_TYPE_VLAN: copy->ol_flags |= PKT_RX_VLAN_PKT; break;
            default:
            break;
          }

          if (rte_ring_enqueue(t->rx_rng_queues[idx].rng, copy) != 0) {
             /* TBD - handle  -EDQUOT ??? */
            rte_pktmbuf_free(copy);
          } 
        }
      }
    }
  } 
  else {
    h = RW_VES_GET_HASH_INDEX(&(eth->d_addr.addr_bytes[0]), sizeof(eth->d_addr.addr_bytes));
    p = &(rw_ves_port_hash_table[h]);
    TAILQ_FOREACH(t, &(p->ports_list), next_hash) {
      if (memcmp(&(t->mac_addr.addr_bytes[0]), &(eth->d_addr.addr_bytes[0]), sizeof(eth->d_addr.addr_bytes)) == 0) {
        break;
      }
    }
    if (t) {
      struct rte_mbuf *copy;
      copy = NULL;
      idx = t->rss_enabled?rw_ves_rss_hash(t, eth):0;
      if (t->rx_rng_queues[idx].mb_pool) {
        copy = rte_pktmbuf_duplicate(pkt, t->rx_rng_queues[idx].mb_pool);
      }
      if (copy) {
        switch (ntohs(eth->ether_type)) {
          case ETHER_TYPE_IPv4: copy->ol_flags |= PKT_RX_IPV4_HDR; break;
          case ETHER_TYPE_IPv6: copy->ol_flags |= PKT_RX_IPV6_HDR; break;
          case ETHER_TYPE_VLAN: copy->ol_flags |= PKT_RX_VLAN_PKT; break;
          default:
          break;
        }
        if (rte_ring_enqueue(t->rx_rng_queues[idx].rng, copy) != 0) {
           rte_pktmbuf_free(copy);
           src_t->err_pkts.cnt ++;
        }
        else {
          ret = 0;
          src_t->tx_pkts.cnt ++;
          src_t->tx_bytes.cnt += rte_pktmbuf_pkt_len(pkt);
        }
      }
    }else{
      /*Not a valid mac. consume the buffer but increment the err count*/
      ret = 0;
      src_t->err_pkts.cnt ++;
    }
  }

  if (ret == 0){
    rte_pktmbuf_free(pkt);
  }
  
  return ret;
}

