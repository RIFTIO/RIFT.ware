
/*-
 * RiftIO Inc.
 * Tunnel Poll Mode Driver for PIOT (Packet I/O Tool Kit)  Developed under
 * DPDK RTE
 *
 *  Author(s): Krishnamoney Elayathu
 *  Creation Date: 10/8/2014
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_dev.h>
#include <rte_string_fns.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <net/if.h>
#include <linux/filter.h>
#include <sys/ioctl.h> 
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <linux/if_tun.h>

#include "rw_pmd_tun.h"

struct rw_pmd_tun_rx_queue {
  struct pmd_internals *internal;
  struct rte_mempool *mb_pool;
  volatile unsigned long rx_pkts;
  volatile unsigned long err_pkts;
};

struct rw_pmd_tun_tx_queue {
  struct pmd_internals *internal;
  volatile unsigned long tx_pkts;
  volatile unsigned long err_pkts;
};

#define RTE_PMD_TUN_MAX_RX_QUEUES 16
#define RTE_PMD_TUN_MAX_TX_QUEUES 16

struct pmd_internals {
  unsigned nb_rx_queues;
  unsigned nb_tx_queues;
  struct   rw_pmd_tun_rx_queue rx_queue[RTE_PMD_TUN_MAX_RX_QUEUES];
  struct   rw_pmd_tun_rx_queue tx_queue[RTE_PMD_TUN_MAX_TX_QUEUES];
  int      fd; /* fd for tun interface */
  char     ifname[RW_PMD_TUN_IFNAME_LEN]; 
  uint8_t  mac_address[ETHER_ADDR_LEN];
};

static const char *drivername = "TUNNEL PMD";
static struct rte_eth_link pmd_link = {
 .link_speed = 10000,
 .link_duplex = ETH_LINK_FULL_DUPLEX,
 .link_status = 0
};

static uint16_t
rw_pmd_tun_rx_burst(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
  struct rw_pmd_tun_rx_queue *r = q;
  int fd;
  struct rte_mbuf *m;
  int rc = 0, nb_rx = 0;
  struct rte_pktmbuf_pool_private *mbp_priv;
  int buf_size;


  ASSERT(r);
  ASSERT( r->internal);
  fd = r->internal->fd;
  ASSERT(fd);

  /* get the space available for data in the mbuf, is there a better way? */
  mbp_priv = (struct rte_pktmbuf_pool_private *)
        ((char *)r->mb_pool + sizeof(struct rte_mempool));
  buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
        RTE_PKTMBUF_HEADROOM);

   /* read multiple frames in a loop */ 
  do {
    m  = rte_pktmbuf_alloc(r->mb_pool);  
    if (unlikely(m == NULL)) {
      break;  
    }
    rc = read(fd, rte_pktmbuf_mtod(m, char*), buf_size);  
    if (rc > 0) {
      if (likely(rc <= buf_size)) {
         /* data already copied to m */ 
        rte_pktmbuf_data_len(m) = rc;
        rte_pktmbuf_pkt_len(m) = rc;

        
         bufs[nb_rx] = m;
         nb_rx++;
       }       
       else {  
         /* PMD Raw packet will not fit in the mbuf, so drop packet */
         RTE_LOG(ERR, PMD, 
           "RAW PMD packet %d bytes will not fit in mbuf (%d bytes)\n",
           rc, buf_size);
         rte_pktmbuf_free(m);
         break;  
      }
    }
    else {  
         rte_pktmbuf_free(m);
    }
  } while ((rc > 0) && (nb_rx < nb_bufs));
 
  return(nb_rx);
}

static void
rw_pmd_tun_mac_addr_add(struct rte_eth_dev *dev,
                         struct ether_addr *mac_addr,
                         uint32_t index __rte_unused,
                         uint32_t vmdq  __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  ether_addr_copy((struct ether_addr *)mac_addr, (struct ether_addr *)internals->mac_address);
  
}


static uint16_t
rw_pmd_tun_tx_burst(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
  struct rw_pmd_tun_tx_queue *r = q;
  int fd;
  uint16_t nb_tx = 0,i;
  struct rte_mbuf *tx_pkt;
  struct rte_mbuf     *m_seg;
  int iov_index = 0;  
  int len_send;

  if (NULL == r->internal) {
    printf("Internel NULL\n");
    return -1;
  }

  fd = r->internal->fd;

  if (0 == fd) {
    printf("fd NULL\n");
    return -1;
  }

  /* TX loop */
  for (i = 0; i < nb_bufs; i++) {
    tx_pkt = *bufs++;

    iov_index = 0;
    m_seg = tx_pkt;
    len_send = 0;
    /* Handle packet coming in multiple segments */
    do {
      len_send = rte_pktmbuf_pkt_len(m_seg);
        iov_index ++;
        if (unlikely(iov_index >= 16)) {
        /*
         * Send only up to 16 segments. 
         */
         break;
        }
        len_send = write(fd, rte_pktmbuf_mtod(m_seg, char*), rte_pktmbuf_data_len(m_seg));
        if (len_send != rte_pktmbuf_data_len(m_seg)) {
         break;
        }
        m_seg = m_seg->next;
    } while (m_seg != NULL);
    nb_tx ++;
    rte_pktmbuf_free(tx_pkt);
  }
  return(nb_tx);
}

static int
rw_pmd_tun_dev_configure(struct rte_eth_dev *dev __rte_unused) 
{ 
  return 0; 
}

static int
rw_pmd_tun_dev_start(struct rte_eth_dev *dev)
{
  dev->data->dev_link.link_status = 1;
  return 0;
}

static void
rw_pmd_tun_dev_stop(struct rte_eth_dev *dev)
{
  dev->data->dev_link.link_status = 0;
}

static int
rw_pmd_tun_rx_queue_setup(struct rte_eth_dev *dev,
                          uint16_t rx_queue_id,
                          uint16_t nb_rx_desc __rte_unused,
                          unsigned int socket_id __rte_unused,
                          const struct rte_eth_rxconf *rx_conf __rte_unused,
                          struct rte_mempool *mb_pool)
{
  struct pmd_internals *internals = dev->data->dev_private;

  internals->rx_queue[rx_queue_id].mb_pool = mb_pool;
  dev->data->rx_queues[rx_queue_id] = &internals->rx_queue[rx_queue_id];
  return 0;
}

static int
rw_pmd_tun_tx_queue_setup(struct rte_eth_dev *dev, 
                          uint16_t tx_queue_id,
                          uint16_t nb_tx_desc __rte_unused,
                          unsigned int socket_id __rte_unused,
                          const struct rte_eth_txconf *tx_conf __rte_unused)
{
  struct pmd_internals *internals;

  if (dev->data == NULL) {
    printf("dev->data is NULL\n");
    return -1;
  }
  internals = dev->data->dev_private;
  if (internals == NULL) {
    printf("internals is NULL\n");
    return -1;
  }

  dev->data->tx_queues[tx_queue_id] = &internals->tx_queue[tx_queue_id];
  return 0;
}


static void
rw_pmd_tun_dev_info(struct rte_eth_dev *dev,
                    struct rte_eth_dev_info *dev_info)
{
  struct pmd_internals *internals = dev->data->dev_private;

  dev_info->driver_name = drivername;
  dev_info->max_mac_addrs = 1;
  dev_info->max_rx_pktlen = (uint32_t)-1;
  dev_info->max_rx_queues = (uint16_t)internals->nb_rx_queues;
  dev_info->max_tx_queues = (uint16_t)internals->nb_tx_queues;
  dev_info->min_rx_bufsize = 0;
  dev_info->pci_dev = NULL;
}

static void
rw_pmd_tun_stats_get(struct rte_eth_dev *dev, 
                       struct rte_eth_stats *stats)
{
  const struct pmd_internals *internal = dev->data->dev_private;
  FILE *f;
  char buff[512];
  char ifname[32];
  char *bp, *np;
  uint64_t rx_dropped, rx_fifo_errors, rx_frame_errors,rx_compressed, tx_dropped, tx_fifo_errors, collisions, tx_carrier_errors, tx_compressed;

  memset(stats, 0, sizeof(*stats));

  f = fopen("/proc/net/dev", "r");
  if (f == NULL) {
    printf("Can not open file for stats %s\n", "/proc/net/dev");
    return;
  }

  /* skip 2 lines */
  fgets(buff, sizeof(buff), f);
  fgets(buff, sizeof(buff), f);

  while (fgets(buff, sizeof(buff), f)) {
     bp = buff;
     np = ifname;
        /* skip any spaces */
     while (isspace(*bp)) {
       bp++;
     }
     while (*bp != ':') {
       *np ++ = *bp++;
     }
     *np = '\0';

     bp++;  /* skip ":" */
     while (isspace(*bp)) {
       bp++;
     }
    
     if (strcmp(internal->ifname, ifname) == 0) {
        sscanf(bp,
        "%Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
              (long long unsigned int *)&stats->ibytes,
              (long long unsigned int *) &stats->ipackets,
              (long long unsigned int *)&stats->ierrors,
              (long long unsigned int *)&rx_dropped,
              (long long unsigned int *)&rx_fifo_errors,
              (long long unsigned int *)&rx_frame_errors,
              (long long unsigned int *)&rx_compressed,
              (long long unsigned int *)&stats->obytes,
              (long long unsigned int *)&stats->opackets,
              (long long unsigned int *)&stats->oerrors,
              (long long unsigned int *)&tx_dropped,
              (long long unsigned int *)&tx_fifo_errors,
              (long long unsigned int *)&collisions,
              (long long unsigned int *)&tx_carrier_errors,
              (long long unsigned int *)&tx_compressed);
     }

  }
  fclose(f);
}

static void
rw_pmd_tun_stats_reset(struct rte_eth_dev *dev __rte_unused)
{
  /*
   * This to be implemented - TBD
   */
  printf("rw_pmd_tun_queue_release called\n");
  return;
}

static void
rw_pmd_tun_queue_release(void *q __rte_unused) 
{ 
  /*
   * This to be implemented - TBD
   */
  printf("rw_pmd_tun_queue_release called\n");
  return; 
}

static int
rw_pmd_tun_link_update(struct rte_eth_dev *dev,
                       int wait_to_complete __rte_unused) 
{

  struct rte_eth_link *link = &(dev->data->dev_link);

  link->link_speed = 1000;
  return 0;
}

static void
rw_pmd_tun_dev_close(struct rte_eth_dev *dev)
{
  struct pmd_internals *internal = dev->data->dev_private;
  ASSERT(internal->fd);
  close(internal->fd);
  internal->fd = -1;
  return;
}

static void
rw_pmd_tun_promiscuous_enable(struct rte_eth_dev *dev)
{
  struct pmd_internals *internal = dev->data->dev_private;
  struct ifreq ethreq;

  ASSERT(internal->fd);

  return;


  // set network card to promiscuos
  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, internal->ifname, IFNAMSIZ);
  if (ioctl(internal->fd,SIOCGIFFLAGS, &ethreq) == -1) {
     perror("ioctl");
  }
  ethreq.ifr_flags |= IFF_PROMISC;
  if (ioctl(internal->fd, SIOCSIFFLAGS, &ethreq) == -1) {
    perror("ioctl");
  }
  return;
}


static void
rw_pmd_tun_promiscuous_disable(struct rte_eth_dev *dev)
{
  struct pmd_internals *internal = dev->data->dev_private;
  struct ifreq ethreq;

  ASSERT(internal->fd);

  return;

  // set network card to promiscuos
  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, internal->ifname, IFNAMSIZ);
  if (ioctl(internal->fd,SIOCGIFFLAGS, &ethreq) == -1) {
     perror("ioctl");
  }
  ethreq.ifr_flags |= ~IFF_PROMISC;
  if (ioctl(internal->fd, SIOCSIFFLAGS, &ethreq) == -1) {
    perror("ioctl");
  }
  return;
}

static int
rw_pmd_tun_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
  ASSERT(dev);
  ASSERT(fc_conf);

  /* TBD */

  return 0;
}

static struct eth_dev_ops ops = {
    .dev_start = rw_pmd_tun_dev_start,
    .dev_stop = rw_pmd_tun_dev_stop,
    .dev_close = rw_pmd_tun_dev_close,
    .promiscuous_enable   =  rw_pmd_tun_promiscuous_enable,
    .promiscuous_disable  =  rw_pmd_tun_promiscuous_disable,
    .dev_configure = rw_pmd_tun_dev_configure,
    .dev_infos_get = rw_pmd_tun_dev_info,
    .rx_queue_setup = rw_pmd_tun_rx_queue_setup,
    .tx_queue_setup = rw_pmd_tun_tx_queue_setup,
    .rx_queue_release = rw_pmd_tun_queue_release,
    .tx_queue_release = rw_pmd_tun_queue_release,
    .link_update = rw_pmd_tun_link_update,
    .stats_get = rw_pmd_tun_stats_get,
    .stats_reset = rw_pmd_tun_stats_reset,
    .flow_ctrl_set = rw_pmd_tun_flow_ctrl_set,
    .mac_addr_add = rw_pmd_tun_mac_addr_add,
    .mac_addr_remove = NULL,
};

static int 
rw_pmd_tun_open(struct pmd_internals *internal,
                       const char *ifname) 
{

  int fd;
  int flags=0;
  struct ifreq ethreq;
  //struct ethtool_cmd edata;

  if (ifname == NULL) {
    return(-1);
  }

  if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
    perror("open /dev/net/tun");
    exit(-1);
  }

  internal->fd = fd;
  ASSERT(fd);

  bzero(&ethreq , sizeof(ethreq));
  strncpy(ethreq.ifr_name, ifname, IFNAMSIZ);
  ethreq.ifr_flags = IFF_TAP | IFF_NO_PI;
  if (ioctl(fd,TUNSETIFF, &ethreq) == -1) {
     perror("ioctl");
     close(fd);
    exit(-1);
  }

#if 0
  if (ioctl(fd, SIOCGIFHWADDR, &ethreq) == 0) {
    ether_addr_copy((struct ether_addr *)&(ethreq.ifr_hwaddr.sa_data), (struct ether_addr *)internal->mac_address);
  }
#endif

  flags = fcntl(fd, F_GETFL, 0); 
  if (flags == -1 ) {
    perror("fnctl");
    close(fd);
    exit(-1);
  }
  flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (flags == -1 ) {
    perror("fnctl");
    close(fd);
    exit(-1);
  }
  return(0); 
}

static int
rw_pmd_tun_create_eth_dev(const char *ifname,
                            const unsigned nb_rx_queues,
                            const unsigned nb_tx_queues,
                            const unsigned numa_node)
{
  struct rte_eth_dev_data *data = NULL;
  struct rte_pci_device *pci_dev = NULL;
  struct pmd_internals *internals = NULL;
  struct rte_eth_dev *eth_dev = NULL;
  unsigned i;

  /* do some paramter checking, only support 1 Tx/Rx queue currently */
  if (nb_rx_queues != 1)
    goto error;
  if (nb_tx_queues > RTE_PMD_TUN_MAX_TX_QUEUES)
    goto error;

  RTE_LOG(INFO, PMD, "Initiating Tunnel ethdev %u\n",
      numa_node);

  /* now do all data allocation - for eth_dev structure, dummy pci driver
   * and internal (private) data
   */
  data = rte_zmalloc_socket(NULL, sizeof(*data), 0, numa_node);
  if (data == NULL)
    goto error;

  pci_dev = rte_zmalloc_socket(NULL, sizeof(*pci_dev), 0, numa_node);
  if (pci_dev == NULL)
    goto error;

  internals = rte_zmalloc_socket(NULL, sizeof(*internals), 0, numa_node);
  if (internals == NULL)
    goto error;

  /* reserve an ethdev entry */
  eth_dev = rte_eth_dev_allocate(ifname, RTE_ETH_DEV_VIRTUAL);
  if (eth_dev == NULL)
    goto error;

  /* now put it all together
   * - store queue data in internals,
   * - store numa_node info in pci_driver
   * - point eth_dev_data to internals and pci_driver
   * - and point eth_dev structure to new eth_dev_data structure
   */
  /* NOTE: we'll replace the data element, of originally allocated eth_dev
   * so the rings are local per-process */

  internals->nb_rx_queues = nb_rx_queues;
  internals->nb_tx_queues = nb_tx_queues;
  strncpy(internals->ifname, ifname, sizeof(internals->ifname));
  if (rw_pmd_tun_open(internals,ifname) != 0) {
    RTE_LOG(ERR, PMD, "TUN PMD rw_pmd_tun_open failed\n");
    return -1;
  }

  for (i=0; i<internals->nb_rx_queues; i++) {
    internals->rx_queue[i].internal = internals;
  }
  for (i=0; i<internals->nb_tx_queues; i++) {
    internals->tx_queue[i].internal = internals;
  }

  pci_dev->numa_node = numa_node;

  data->dev_private = internals;
  data->port_id = eth_dev->data->port_id;
  data->nb_rx_queues = (uint16_t)nb_rx_queues;
  data->nb_tx_queues = (uint16_t)nb_tx_queues;
  data->dev_link = pmd_link;
  data->mac_addrs = rte_zmalloc("ethtun", ETHER_ADDR_LEN, 0);
  if (data->mac_addrs == NULL) {
    RTE_LOG(ERR, PMD, "Failed to allocate %d bytes needed to store MAC addresses", ETHER_ADDR_LEN);
  } 
  ether_addr_copy((struct ether_addr *)internals->mac_address, (struct ether_addr *)data->mac_addrs);

  eth_dev ->data = data;
  eth_dev ->dev_ops = &ops;
  eth_dev ->pci_dev = pci_dev;

  /* finally assign rx and tx ops */
  eth_dev->rx_pkt_burst = rw_pmd_tun_rx_burst;
  eth_dev->tx_pkt_burst = rw_pmd_tun_tx_burst;

  return 0;

error:
  RTE_LOG(ERR, PMD, "TUN PMD rw_pmd_tun_create_eth_dev failed\n");

  if (data)
    rte_free(data);
  if (pci_dev)
    rte_free(pci_dev);
  if (internals)
    rte_free(internals);
  return -1;
}

/*
 * Initialization for tun Poll Mode Driver
 * 'param' is name of the kernel ethernet interface
 * to be opened for tun connection
 */

int
rw_pmd_tun_init(const char *name __rte_unused, const char *params)
{

  if (params == NULL) {
    RTE_LOG(INFO, PMD, "No Interface specificified" 
            "tun  PMD driver\n");
    return -1;
  }

/*
 * Can not have more than 1 Rx Queue as there is no RSS supprt 
 */

  return(rw_pmd_tun_create_eth_dev(params, 1, RTE_PMD_TUN_MAX_TX_QUEUES, rte_socket_id()));
}

static struct rte_driver pmd_tun_drv = {
	.name = "eth_tun",
	.type = PMD_VDEV,
	.init = rw_pmd_tun_init,
};

PMD_REGISTER_DRIVER(pmd_tun_drv);

