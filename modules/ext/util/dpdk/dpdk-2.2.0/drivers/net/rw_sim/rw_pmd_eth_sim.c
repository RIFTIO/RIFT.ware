
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
 * @file rw_fpath_init.c
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 06/12/2014
 * @Ethernet Sim PMD driver
 *
 * @details
 */

#include <rte_mbuf.h>
#include <rte_dev.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include "rw_pmd_eth_sim.h"
#include "rw_virt_eth_switch.h"

static const char *drivername = "Eth-Sim PMD";
static struct rte_eth_link pmd_link = {
		.link_speed = 10000,
		.link_duplex = ETH_LINK_FULL_DUPLEX,
		.link_status = 0
};
static void
pmd_eth_sim_dev_info(struct rte_eth_dev *dev,
                     struct rte_eth_dev_info *dev_info);

#define PMD_ETH_SIM_MAX_TX_BURST 32

static uint16_t
pmd_eth_sim_rx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
	void **ptrs = (void *)&bufs[0];
        uint32_t i;
	struct eth_sim_rx_queue *r = q;

  /* JIRA 6806 -  Before we read pkts from queue; read any external pkts on ves tap device */
  rw_ves_receive_external_pkts();

	const uint16_t nb_rx = (uint16_t)rte_ring_dequeue_burst(r->rng, 
			ptrs, nb_bufs);
	if (r->rng->flags & RING_F_SC_DEQ)
		r->rx_pkts.cnt += nb_rx;
	else
		rte_atomic64_add(&(r->rx_pkts), nb_rx);
        /* does the buffer get freed by now -- TBD */
        for (i = 0; i <nb_rx; i++) {
	  if (r->rng->flags & RING_F_SC_DEQ) {
            r->rx_bytes.cnt += rte_pktmbuf_pkt_len(bufs[i]);
          }
          else {
            rte_atomic64_add(&(r->rx_bytes), rte_pktmbuf_pkt_len(bufs[i]));
          }
          if (rte_pktmbuf_pkt_len(bufs[i]) == 0){
            ;
          }
        }
	return nb_rx;
}

static uint16_t
pmd_eth_sim_tx(void *q, struct rte_mbuf **bufs, uint16_t nb_bufs)
{
  struct eth_sim_tx_queue *t = q;
  uint32_t i;
    
  for (i = 0; i <nb_bufs; i++) {
    if (rw_ves_switch_pkt((uint64_t)(t->dev), bufs[i], t) != 0) {
      break;
    }
  }
  
  return i;
}

static int
pmd_eth_sim_dev_configure(struct rte_eth_dev *dev) 
{ 
	struct pmd_internals *internals = dev->data->dev_private;

  internals->nb_rx_queues = dev->data->nb_rx_queues;
  return 0; 
}

static int
pmd_eth_sim_dev_start(struct rte_eth_dev *dev)
{
  struct pmd_internals *internals = dev->data->dev_private;
  uint32_t i;
  static struct ether_addr null_mac_addr = {{0, 0, 0, 0, 0, 0}};
  struct rte_eth_dev_info dev_info;
  
  pmd_eth_sim_dev_info(dev, &dev_info);
  
  dev->data->dev_link.link_status = 1;
  for (i = 0; i < dev_info.max_mac_addrs; i++){
    if (memcmp(&null_mac_addr, &dev->data->mac_addrs[i],
               ETHER_ADDR_LEN) != 0){
      rw_ves_add_port((uint64_t)dev, &(dev->data->mac_addrs[i]),
                      internals->nb_rx_queues,
                      internals->rx_ring_queues);
    }
  }
  return 0;
}

static void
pmd_eth_sim_dev_stop(struct rte_eth_dev *dev)
{
  uint32_t i;
  static struct ether_addr null_mac_addr = {{0, 0, 0, 0, 0, 0}};
  struct rte_eth_dev_info dev_info;
  
  pmd_eth_sim_dev_info(dev, &dev_info);
  dev->data->dev_link.link_status = 0;
  for (i = 0; i < dev_info.max_mac_addrs; i++){
    if (memcmp(&null_mac_addr, &dev->data->mac_addrs[i], ETHER_ADDR_LEN) != 0){
      rw_ves_del_port((uint64_t)dev, &(dev->data->mac_addrs[i]));
    }
  }

}

static void
pmd_eth_sim_dev_close(struct rte_eth_dev *dev)
{
  pmd_eth_sim_dev_stop(dev);

  /*TBD*/
  //free the memory and release the port-id
  return;
}

static int
pmd_eth_sim_rx_queue_setup(struct rte_eth_dev *dev,uint16_t rx_queue_id,
				    uint16_t nb_rx_desc __rte_unused,
				    unsigned int socket_id __rte_unused,
				    const struct rte_eth_rxconf *rx_conf __rte_unused,
				    struct rte_mempool *mb_pool)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->rx_queues[rx_queue_id] = &internals->rx_ring_queues[rx_queue_id];
        internals->rx_ring_queues[rx_queue_id].mb_pool = mb_pool;
	return 0;
}

static int
pmd_eth_sim_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
				    uint16_t nb_tx_desc __rte_unused,
				    unsigned int socket_id __rte_unused,
				    const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev->data->tx_queues[tx_queue_id] = &internals->tx_ring_queues[tx_queue_id];
	return 0;
}


static void
pmd_eth_sim_dev_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct pmd_internals *internals = dev->data->dev_private;
	dev_info->driver_name = drivername;
	dev_info->max_mac_addrs = 2;
	dev_info->max_rx_pktlen = (uint32_t)-1;
	dev_info->max_rx_queues = (uint16_t)internals->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)internals->nb_tx_queues;
	dev_info->min_rx_bufsize = 0;
	dev_info->pci_dev = NULL;
}

static void
pmd_eth_sim_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *igb_stats)
{
	unsigned i;
	uint64_t rx_total = 0, tx_total = 0, tx_err_total = 0;
	uint64_t rx_bytes_total = 0, tx_bytes_total = 0;
	const struct pmd_internals *internal = dev->data->dev_private;

	memset(igb_stats, 0, sizeof(*igb_stats));
	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_rx_queues; i++) {
		igb_stats->q_ipackets[i] = internal->rx_ring_queues[i].rx_pkts.cnt;
		rx_total += igb_stats->q_ipackets[i];
	        igb_stats->q_ibytes[i] = internal->rx_ring_queues[i].rx_bytes.cnt;
                rx_bytes_total += igb_stats->q_ibytes[i];
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS &&
			i < internal->nb_tx_queues; i++) {
		igb_stats->q_opackets[i] = internal->tx_ring_queues[i].tx_pkts.cnt;
		igb_stats->q_errors[i] = internal->tx_ring_queues[i].err_pkts.cnt;
		tx_total += igb_stats->q_opackets[i];
		tx_err_total += igb_stats->q_errors[i];
	        igb_stats->q_obytes[i] = internal->tx_ring_queues[i].tx_bytes.cnt;
                tx_bytes_total += igb_stats->q_obytes[i];
	}

	igb_stats->ipackets = rx_total;
	igb_stats->opackets = tx_total;
	igb_stats->ibytes = rx_bytes_total;
	igb_stats->obytes = tx_bytes_total;
	igb_stats->oerrors = tx_err_total;
}

static void
pmd_eth_sim_stats_reset(struct rte_eth_dev *dev)
{
	unsigned i;
	struct pmd_internals *internal = dev->data->dev_private;
	for (i = 0; i < internal->nb_rx_queues; i++) {
		internal->rx_ring_queues[i].rx_pkts.cnt = 0;
		internal->rx_ring_queues[i].rx_bytes.cnt = 0;
        }
	for (i = 0; i < internal->nb_tx_queues; i++) {
		internal->tx_ring_queues[i].tx_pkts.cnt = 0;
		internal->tx_ring_queues[i].err_pkts.cnt = 0;
		internal->tx_ring_queues[i].tx_bytes.cnt = 0;
	}
}

static void
pmd_eth_sim_queue_release(void *q __rte_unused) { ; }

static int
pmd_eth_sim_link_update(struct rte_eth_dev *dev __rte_unused,
		int wait_to_complete __rte_unused) { return 0; }

static void
pmd_eth_sim_mac_addr_add(struct rte_eth_dev *dev,
                         struct ether_addr *mac_addr,
                         uint32_t index __rte_unused,
                         uint32_t vmdq  __rte_unused)
{
  struct pmd_internals *internals = dev->data->dev_private;
  
  rw_ves_add_port((uint64_t)dev, mac_addr,
                  internals->nb_rx_queues,
                  internals->rx_ring_queues);
}

static void
pmd_eth_sim_mac_addr_remove(struct rte_eth_dev *dev,
                            uint32_t index)
{
  rw_ves_del_port((uint64_t)dev, &dev->data->mac_addrs[index]);
}

static int
pmd_eth_sim_flow_ctrl_get(struct rte_eth_dev *dev __rte_unused,
                          struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}

static int
pmd_eth_sim_flow_ctrl_set(struct rte_eth_dev *dev __rte_unused,
                          struct rte_eth_fc_conf *fc_conf __rte_unused)
{
  return 0;
}


static struct eth_dev_ops ops = {
  .dev_start = pmd_eth_sim_dev_start,
  .dev_stop = pmd_eth_sim_dev_stop,
  .dev_close            = pmd_eth_sim_dev_close,
  .dev_configure = pmd_eth_sim_dev_configure,
  .dev_infos_get = pmd_eth_sim_dev_info,
  .rx_queue_setup = pmd_eth_sim_rx_queue_setup,
  .tx_queue_setup = pmd_eth_sim_tx_queue_setup,
  .rx_queue_release = pmd_eth_sim_queue_release,
  .tx_queue_release = pmd_eth_sim_queue_release,
  .link_update = pmd_eth_sim_link_update,
  .stats_get = pmd_eth_sim_stats_get,
  .stats_reset = pmd_eth_sim_stats_reset,
  .mac_addr_add = pmd_eth_sim_mac_addr_add,
  .mac_addr_remove = pmd_eth_sim_mac_addr_remove,
  .flow_ctrl_get        = pmd_eth_sim_flow_ctrl_get,
  .flow_ctrl_set        = pmd_eth_sim_flow_ctrl_set,
};

int
pmd_eth_sim_device_from_ring(const char *ifname, struct rte_ring *const rx_queues[],
		const unsigned nb_rx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node)
{
	struct rte_eth_dev_data *data = NULL;
	struct rte_pci_device *pci_dev = NULL;
	struct pmd_internals *internals = NULL;
	struct rte_eth_dev *eth_dev = NULL;
	unsigned i;

	/* do some paramter checking */
	if (rx_queues == NULL && nb_rx_queues > 0)
		goto error;

	RTE_LOG(INFO, PMD, "Creating rings-backed ethdev on numa socket %u\n",
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
	for (i = 0; i < nb_rx_queues; i++) {
		internals->rx_ring_queues[i].rng = rx_queues[i];
		internals->rx_ring_queues[i].dev = eth_dev;
	}
	for (i = 0; i < nb_tx_queues; i++) {
		internals->tx_ring_queues[i].dev = eth_dev;
	}

	pci_dev->numa_node = numa_node;

	data->dev_private = internals;
	data->port_id = eth_dev->data->port_id;
	data->nb_rx_queues = (uint16_t)nb_rx_queues;
	data->nb_tx_queues = (uint16_t)nb_tx_queues;
	data->dev_link = pmd_link;
	data->mac_addrs = &(internals->eth_addr);

	eth_dev->data = data;
	eth_dev->dev_ops = &ops;
	eth_dev->pci_dev = pci_dev;

	/* finally assign rx and tx ops */
	eth_dev->rx_pkt_burst = pmd_eth_sim_rx;
	eth_dev->tx_pkt_burst = pmd_eth_sim_tx;

	return 0;

error:
	if (data)
		rte_free(data);
	if (pci_dev)
		rte_free(pci_dev);
	if (internals)
		rte_free(internals);
	return -1;
}

static int
pmd_eth_sim_dev_create(const char *name, const unsigned numa_node)
{
	/* rx and tx are so-called from point of view of first port.
	 * They are inverted from the point of view of second port
	 */
	struct rte_ring *rx[RW_PMD_ETH_SIM_MAX_RX_QUEUES];
	unsigned i;
	char rng_name[RTE_RING_NAMESIZE];
	unsigned num_rx_rings = RW_PMD_ETH_SIM_MAX_RX_QUEUES;

	for (i = 0; i < num_rx_rings; i++) {
		rte_snprintf(rng_name, sizeof(rng_name), "ETH_RXTX%u_%s", i, name);
		rx[i] = rte_ring_create(rng_name, 1024, numa_node, RING_F_SC_DEQ);
		if (rx[i] == NULL)
			return -1;
	}

	if (pmd_eth_sim_device_from_ring(name,
                                         rx, num_rx_rings, RW_PMD_ETH_SIM_MAX_TX_QUEUES, numa_node))
		return -1;

	return 0;
}


int
rte_pmd_eth_sim_init(const char *name, const char *params  __rte_unused)
{
	RTE_LOG(INFO, PMD, "Initializing eth-sim ethernet device name %s\n", name);

        rw_ves_init();

	pmd_eth_sim_dev_create(name, rte_socket_id());

	return 0;
}


static struct rte_driver pmd_sim_drv = {
	.name = "eth_sim",
	.type = PMD_VDEV,
	.init = rte_pmd_eth_sim_init,
};

PMD_REGISTER_DRIVER(pmd_sim_drv);
