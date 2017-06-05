/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_virt_eth_switch.h
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 06/12/2014
 * @simple virtual ethernet switch for PMD eth_sim ports
 *
 * @details
 */

#ifndef _RW_VIRT_ETH_SWITCH_H_
#define _RW_VIRT_ETH_SWITCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ring.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#define RW_VES_NUM_PORTS_HASH_BUCKETS 1024

  //forward declaration
  struct eth_sim_tx_queue;
  
struct rw_ves_port {
  uint64_t          port_iden;   /*** port idefntifier */
  struct ether_addr mac_addr;    /*** mac-address of the port */
  struct eth_sim_rx_queue *rx_rng_queues;
  int               num_rx_queues;
  uint8_t rss_enabled:1;
  TAILQ_ENTRY(rw_ves_port) next_element;
  TAILQ_ENTRY(rw_ves_port) next_hash;
};

typedef struct {
  TAILQ_HEAD(ports_head, rw_ves_port) ports_list;
} rw_ves_ports_hash_list_t;


#define RW_VES_GET_HASH_INDEX(_val, _len)   (rte_jhash(_val, _len, 0) % RW_VES_NUM_PORTS_HASH_BUCKETS)

void rw_ves_init(void);
int rw_ves_add_port(uint64_t port_iden, struct ether_addr *mac_addr,
                    int num_rx_queues,
                    struct eth_sim_rx_queue *rx_rng_queues);
int rw_ves_switch_pkt(uint64_t port_iden, struct rte_mbuf * pkt,
                        struct eth_sim_tx_queue *src_t);
int
rw_ves_del_port(uint64_t port_iden,
                struct ether_addr *mac_addr);


#ifdef __cplusplus
}
#endif

#endif
