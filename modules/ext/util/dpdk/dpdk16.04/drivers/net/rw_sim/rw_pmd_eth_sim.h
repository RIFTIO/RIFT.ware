
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

#ifndef _RW_PMD_ETH_SIM_H_
#define _RW_PMD_ETH_SIM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ring.h>
#include <rte_ether.h>
  
#define RTE_PMD_ETH_SIM_PARAM_NAME "eth_sim"

#define RW_PMD_ETH_SIM_MAX_RX_QUEUES 16
#define RW_PMD_ETH_SIM_MAX_TX_QUEUES 40

struct eth_sim_rx_queue {
	struct rte_ring *rng;
        struct rte_eth_dev *dev;
	rte_atomic64_t rx_pkts;
	rte_atomic64_t rx_bytes;
	rte_atomic64_t err_pkts;
  struct rte_mempool *mb_pool;
};

struct eth_sim_tx_queue {
        struct rte_eth_dev *dev;
        rte_atomic64_t tx_pkts;
        rte_atomic64_t tx_bytes;
        rte_atomic64_t err_pkts;
};


struct pmd_internals {
	unsigned nb_rx_queues;
	unsigned nb_tx_queues;

	struct eth_sim_rx_queue rx_ring_queues[RW_PMD_ETH_SIM_MAX_RX_QUEUES];
	struct eth_sim_tx_queue tx_ring_queues[RW_PMD_ETH_SIM_MAX_TX_QUEUES];
         
        struct ether_addr eth_addr;
};



  int pmd_eth_sim_device_from_ring(const char *ifname, struct rte_ring * const rx_queues[],
		const unsigned nb_rx_queues,
		const unsigned nb_tx_queues,
		const unsigned numa_node);


/**
 * For use by the EAL only. Called as part of EAL init to set up any dummy NICs
 * configured on command line.
 */
int rte_pmd_eth_sim_init(const char *name, const char *params);

#ifdef __cplusplus
}
#endif

#endif
