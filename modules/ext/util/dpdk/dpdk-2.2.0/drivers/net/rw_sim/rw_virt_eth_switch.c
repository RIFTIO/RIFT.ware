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
#include <rte_arp.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <net/if.h>
#include <sys/ioctl.h> 
#include <linux/if_tun.h>
#include <net/route.h>

/*
 * list of ports
 */
TAILQ_HEAD(ves_ports_head, rw_ves_port) rw_ves_port_list;
uint8_t  rw_ves_initialized = 0;
rw_ves_ports_hash_list_t rw_ves_port_hash_table[RW_VES_NUM_PORTS_HASH_BUCKETS];

int rw_ves_raw_fd = -1;
char rw_ves_tap_ifname[32];
struct ether_addr rw_ves_tap_mac_addr;
static int rw_ves_open_tap_device(void);
static int rw_ves_send_packet_on_tap_device(int fd, struct rte_mbuf *tx_pkt);
static int rw_ves_receive_pkt_from_tap_device(int fd);
static int rw_ves_add_route_to_tap_device(uint32_t host,char *ifname);


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
   
    char *env = getenv("RW_ETHSIM_ENABLE_TAP");
    if(env && rw_ves_raw_fd == -1 && getuid() == 0) {
      fprintf(stderr,"RW_ETHSIM_ENABLE_TAP is set; Enabling TAP device in VES\n");
      rw_ves_raw_fd = rw_ves_open_tap_device();
      if(rw_ves_raw_fd == -1) {
        fprintf(stderr,"Opening tap device in virt eth switch failed with error %s\n",strerror(errno));
      }
    }
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
            indx = rte_jhash_32b(&(ipv4->src_addr),2,0);
        }
      }
      else {
        indx = rte_jhash_32b(&(ipv4->src_addr),2,0);
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

    /* Send bcast pkt also externally using tap device incase these are ARP pkts to resolve external IPs */
    rw_ves_send_packet_on_tap_device(rw_ves_raw_fd,pkt);
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
    }else if( rw_ves_raw_fd != -1 && memcmp(&(eth->d_addr.addr_bytes[0]), &(rw_ves_tap_mac_addr.addr_bytes[0]), sizeof(eth->d_addr.addr_bytes)) == 0){
      /* Internal tap device mac. Send pkt out externally through tap device */
      ret = rw_ves_send_packet_on_tap_device(rw_ves_raw_fd,pkt);
      if(ret != 0) {
        ret = 0;
        src_t->err_pkts.cnt ++;
      }
    }
    else {
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

static int rw_ves_open_tap_device(void)
{
  int fd,s;
  int flags=0;
  struct ifreq ethreq;
  const char *ifname = NULL;
  snprintf(rw_ves_tap_ifname,sizeof(rw_ves_tap_ifname),"tap-%d",getpid());
  ifname = rw_ves_tap_ifname;

  if (ifname == NULL) {
    return(-1);
  }

  if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
    perror("open /dev/net/tun");
    return -1;
  }

  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, ifname, IFNAMSIZ);
  ethreq.ifr_flags = IFF_TAP | IFF_NO_PI;
  if (ioctl(fd,TUNSETIFF, &ethreq) == -1) {
     perror("ioctl");
     close(fd);
    return -1;
  }

  /* Set the fd to non block mode */
  flags = fcntl(fd, F_GETFL, 0); 
  if (flags == -1 ) {
    perror("fnctl");
    close(fd);
    return -1;
  }
  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1 ) {
    perror("fnctl");
    close(fd);
    return -1;
  }

  /* Make tap device interface UP */
  bzero(&ethreq , sizeof(ethreq));
  strcpy(ethreq.ifr_name, ifname);

  if ( (s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    close(fd);
    close(s);
    return -1;
  }

  // Get interface flags
  if (ioctl(s, SIOCGIFFLAGS, &ethreq) < 0) {
    perror("cannot get interface flags");
    close(fd);
    close(s);
    return -1;
  }

  // Turn on interface
  ethreq.ifr_flags |= IFF_UP;
  if (ioctl(s, SIOCSIFFLAGS, &ethreq) < 0) {
    fprintf(stderr, "ifup: failed ");
    close(fd);
    close(s);
    return -1;
  }

  if (ioctl(s, SIOCGIFHWADDR, &ethreq) == 0) {
    ether_addr_copy((struct ether_addr *)&(ethreq.ifr_hwaddr.sa_data), (struct ether_addr *)&rw_ves_tap_mac_addr);
  }

  if(s) {
    close(s);
  }
    
  return fd;
}

static int rw_ves_send_packet_on_tap_device(int fd, struct rte_mbuf *tx_pkt) 
{
  struct rte_mbuf     *m_seg;
  int len_send;
  int pkt_len;
  char *pkt;

  if(rw_ves_raw_fd == -1 || rw_ves_tap_ifname[0] == '\0') {
    return -1;
  }
  

  struct ether_hdr *eth;
  eth = rte_pktmbuf_mtod(tx_pkt, struct ether_hdr *);

  /* Check for ARP request and add sender IP route to tap device to get any response back */
  /* All the local IPs route get added to tap device as result of this */
  if(ntohs(eth->ether_type) == ETHER_TYPE_ARP) {
    struct arp_hdr    *arp = (struct arp_hdr *)&eth[1];
    if(ntohs(arp->arp_op) == ARP_OP_REQUEST) {
      /* TODO - Check if sender IP route is already added before triggering again */
      rw_ves_add_route_to_tap_device(ntohl(arp->arp_data.arp_sip),rw_ves_tap_ifname);
    }
  }

  m_seg = tx_pkt;
  len_send = 0;
  pkt_len = 0;
  pkt = NULL;
  if(m_seg->next != NULL) {
    int tot_len = rte_pktmbuf_pkt_len(tx_pkt);
    do {
      char buf[tot_len];
      pkt = &buf[0];
      memcpy(pkt+pkt_len,rte_pktmbuf_mtod(m_seg, char*),rte_pktmbuf_data_len(m_seg));
      pkt_len += rte_pktmbuf_data_len(m_seg);
      m_seg = m_seg->next;
    } while (m_seg != NULL);
  }
  else {
    pkt = rte_pktmbuf_mtod(m_seg, char*);
    pkt_len = rte_pktmbuf_data_len(m_seg);
  }


  len_send = write(fd, pkt, pkt_len);
  if (len_send != pkt_len) {
    fprintf(stderr,"Packet sending on ves tap device failed for fd: %d sent len: %d pkt len: %d",fd,len_send,pkt_len);
  }

  return 0;
}

void rw_ves_receive_external_pkts(void)
{
  if(rw_ves_raw_fd != -1) {
    rw_ves_receive_pkt_from_tap_device(rw_ves_raw_fd);
  }

}

static int rw_ves_receive_pkt_from_tap_device(int fd) 
{
  int rc = 0, nb_rx = 0;
  char buf[4096];
  int buf_size = 4096;
  uint16_t nb_bufs = 16;
  struct rte_pktmbuf_pool_private *mbp_priv;
  int allowed_size = 0;

  do {
    /* get the space available for data in the mbuf, is there a better way? */
    rc = read(fd, (char *)buf, buf_size);  
    if (rc > 0) {
      if (likely(rc <= buf_size)) {
        struct rw_ves_port *t = NULL;
        rw_ves_ports_hash_list_t *p;
        uint32_t h;
        struct ether_hdr *eth = (struct ether_hdr *)buf;
        nb_rx++;

        uint16_t *b;
        b = (uint16_t *)(&(eth->d_addr.addr_bytes[0]));
        /* If dst MAC is broadcast address; send to all ports */
        if ((b[0] == 0xffff && b[1] == 0xffff && b[2] == 0xffff)) {
          TAILQ_FOREACH(t, &(rw_ves_port_list), next_element) {
            struct rte_mbuf *m = NULL;
            uint16_t idx = 0;
            idx = t->rss_enabled?rw_ves_rss_hash(t, eth):0;
            if (t->rx_rng_queues[idx].mb_pool) {
              mbp_priv = (struct rte_pktmbuf_pool_private *)
                  ((char *)t->rx_rng_queues[idx].mb_pool + sizeof(struct rte_mempool));
              allowed_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
                                         RTE_PKTMBUF_HEADROOM);
              if(rc <= allowed_size) {
                m   = rte_pktmbuf_alloc(t->rx_rng_queues[idx].mb_pool);
                unsigned char *pkt = rte_pktmbuf_mtod(m, unsigned char *);
                memcpy(pkt,buf,rc);
                rte_pktmbuf_pkt_len(m) = rc;
                rte_pktmbuf_data_len(m) = rc;

                if (rte_ring_enqueue(t->rx_rng_queues[idx].rng, m) != 0) {
                  /* TBD - handle  -EDQUOT ??? */
                  fprintf(stderr,"Rx packet failed queued\n");
                  rte_pktmbuf_free(m);
                } 
              }
              else {
                fprintf(stderr,"Rx pkt size read %d exceeds mb pool allowed size: %d\n",rc,allowed_size);
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

          /* If received pkt dest MAC belongs locally known MAC; enqueue the pkt to corresponding queue */
          if (t) {
            struct rte_mbuf *m = NULL;
            uint16_t idx = 0;
            idx = t->rss_enabled?rw_ves_rss_hash(t, eth):0;
            if (t->rx_rng_queues[idx].mb_pool) {
              mbp_priv = (struct rte_pktmbuf_pool_private *)
                  ((char *)t->rx_rng_queues[idx].mb_pool + sizeof(struct rte_mempool));
              allowed_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
                                         RTE_PKTMBUF_HEADROOM);
              if(rc <= allowed_size) {
                m   = rte_pktmbuf_alloc(t->rx_rng_queues[idx].mb_pool);
                unsigned char *pkt = rte_pktmbuf_mtod(m, unsigned char *);
                memcpy(pkt,buf,rc);
                rte_pktmbuf_pkt_len(m) = rc;
                rte_pktmbuf_data_len(m) = rc;

                if (rte_ring_enqueue(t->rx_rng_queues[idx].rng, m) != 0) {
                  /* TBD - handle  -EDQUOT ??? */
                  fprintf(stderr,"Rx packet failed queued\n");
                  rte_pktmbuf_free(m);
                } 
              }
              else {
                fprintf(stderr,"Rx pkt size read %d exceeds mb pool allowed size: %d\n",rc,allowed_size);
              }
            }
          }
        }
      }
      else {
        /* PMD Raw packet will not fit in the mbuf, so drop packet */
        fprintf(stderr,"Received pkt exceeds allowed size %d received size: %d\n",buf_size,rc);
        break;
      }
    }
  }while ((rc > 0) && (nb_rx < nb_bufs));

  return 0;
}

static int rw_ves_add_route_to_tap_device(uint32_t host,char *ifname)
{
   // create the control socket.
   int fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_IP );
 
   struct rtentry route;
   memset( &route, 0, sizeof( route ) );
 
   // set the gateway to 0.
   struct sockaddr_in *addr = (struct sockaddr_in *)&route.rt_gateway;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0;
 
   // set the host we are adding route for
   addr = (struct sockaddr_in*) &route.rt_dst;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = htonl(host);
 
   // Set the mask. 
   addr = (struct sockaddr_in*) &route.rt_genmask;
   addr->sin_family = AF_INET;
   addr->sin_addr.s_addr = 0xFFFFFFFF;
 
   route.rt_flags = RTF_UP | RTF_HOST;
   route.rt_metric = 0;

  // Set the device to tap device name 
   route.rt_dev = (char *)ifname;
 
   if ( ioctl( fd, SIOCADDRT, &route ) )
   {
      close( fd );
      return -1;
   }
 
   close( fd );
   return 0; 
}

