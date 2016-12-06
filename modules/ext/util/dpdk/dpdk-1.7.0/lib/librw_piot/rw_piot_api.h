
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
 * @file rw_piot_api.h
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 02/03/2014
 * @brief Network Fastpath Packet I/O Tool Kit (PIOT) API
 * 
 * @details This file contains routines that provide a Packet I/O Toolkit
 * API ontop of DPDK, raw-sockets, ring-buffers, and libPCAP
 */

#ifndef _RW_PIOT_API_H_
#define _RW_PIOT_API_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <getopt.h>
#include <sys/file.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/io.h>
#include <sys/user.h>
#include <linux/binfmts.h>
#include <linux/filter.h>
 
#include <rte_config.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_random.h>
#include <rte_cycles.h>
#include <rte_string_fns.h>
#include <rte_cpuflags.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_common.h>
#include <rte_version.h>
#include <rte_atomic.h>
#include <rte_timer.h>
#include <rte_kni.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_lpm.h>
#include "rte_ethdev.h"

#include <assert.h>

__BEGIN_DECLS

#ifndef ASSERT
#define ASSERT assert
#endif

#ifndef DP_ASSERT
#define DP_ASSERT assert
#endif


/* Define this value as multiple of 2 */
#define RWPIOT_MAX_DEVICES RTE_MAX_ETHPORTS
#define RWPIOT_MAX_KNI_DEVICES 128
#define RW_PIOT_DEVICE_NAME_LEN 256
#define RW_PIOT_LPORT_DEVICE_NAME_LEN 32
#define RW_PIOT_DEVICE_GRP_NAME_LEN 32

typedef void * rw_piot_api_handle_t;

typedef struct rte_eth_conf    rw_piot_port_conf_t;
typedef struct rte_eth_rxconf  rw_piot_rxport_conf_t;
typedef struct rte_eth_txconf  rw_piot_txport_conf_t;
typedef struct rte_eth_fc_conf rw_piot_port_fc_conf_t;

typedef struct rte_eth_link    rw_piot_link_info_t;
typedef struct rte_eth_stats   rw_piot_link_stats_t;

typedef enum {
  PIOT_PCI,
  PIOT_NON_PCI
} rw_piot_device_catagory_t;

typedef struct {
  char device_group_name[RW_PIOT_DEVICE_GRP_NAME_LEN];
  rw_piot_device_catagory_t device_group_type;
  int num_open_instances;
} rw_piot_devgroup_t;

typedef struct  {
  uint8_t used;
  rw_piot_api_handle_t piot_api_handle;
  uint32_t rte_port_id;
  char device_name[RW_PIOT_DEVICE_NAME_LEN];
  rw_piot_devgroup_t *device_group;
} rw_piot_device_t;

#define RWPIOT_GET_DEVICE(handle) ((rw_piot_device_t *)(handle))

#define RWPIOT_VALID_DEVICE(dev)  ((dev != NULL) && dev->used)

typedef struct {
  uint16_t num_rx_queues; /*< # of RX queues requested for the port */
  uint16_t num_tx_queues; /*< # of TX queues requested for the port */

  /* Event Callback fn - TBD */

  rw_piot_port_conf_t dev_conf; /*< Initial Dev Configuration Requested */
} rw_piot_open_request_info_t;


typedef struct {
  uint16_t num_rx_queues;
  uint16_t num_tx_queues;
  int      NUMA_affinity;
  /* Interrupt Event poll info */
} rw_piot_open_response_info_t;


typedef struct rw_piot_lcore_info_s {
  uint8_t allocated;
} rw_piot_lcore_info_t;

extern rw_piot_lcore_info_t rw_piot_lcore[RTE_MAX_LCORE];

#define RW_PIOT_LCORE_DETECTED(_id) (((_id) < RTE_MAX_LCORE) && lcore_config[(_id)].detected)
#define RW_PIOT_LCORE_AVAILABLE(_id) (((_id) < RTE_MAX_LCORE) && lcore_config[(_id)].detected && !(rw_piot_lcore[(_id)].allocated))
#define RW_PIOT_LCORE_ALLOCATE(_id) (rw_piot_lcore[(_id)].allocated = 1)
#define RW_PIOT_LCORE_FREE(_id) (rw_piot_lcore[(_id)].allocated = 0)
#define RW_PIOT_LCORE_FOREACH(_id)  RTE_LCORE_FOREACH(_id)
#define RW_PIOT_LCORE_GET_SOCKET(_id) (lcore_config[(_id)].socket_id) 
#define RW_PIOT_LCORE_GET_COREID(_id) (lcore_config[(_id)].core_id)

#define RW_PIOT_GET_MBUF_RX_OFFLOAD_FLAG(_buf) (((_buf)->ol_flags) & ~PKT_TX_OFFLOAD_MASK)
#define RW_PIOT_SET_MBUF_TX_OFFLOAD_FLAG(_buf, _flag) ((_buf)->ol_flags |= ((_flag) & PKT_TX_OFFLOAD_MASK))
#define RW_PIOT_SET_MBUF_TX_OFFLOAD_CSUM(_buf, _flag) ((_buf)->ol_flags |= ((PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_SCTP_CKSUM | PKT_TX_UDP_CKSUM) & PKT_TX_OFFLOAD_MASK))

#define RW_PIOT_RX_OFFLOAD_CAP_IPV4_CKSUM(_rx_cap)  ((_rx_cap) & DEV_RX_OFFLOAD_IPV4_CKSUM)
#define RW_PIOT_RX_OFFLOAD_CAP_UDP_CKSUM(_rx_cap)   ((_rx_cap) & DEV_RX_OFFLOAD_UDP_CKSUM)
#define RW_PIOT_RX_OFFLOAD_CAP_TCP_CKSUM(_rx_cap)   ((_rx_cap) & DEV_RX_OFFLOAD_TCP_CKSUM)
#define RW_PIOT_TX_OFFLOAD_CAP_IPV4_CKSUM(_tx_cap)  ((_tx_cap) & DEV_TX_OFFLOAD_IPV4_CKSUM)
#define RW_PIOT_TX_OFFLOAD_CAP_UDP_CKSUM(_tx_cap)   ((_tx_cap) & DEV_TX_OFFLOAD_UDP_CKSUM)
#define RW_PIOT_TX_OFFLOAD_CAP_TCP_CKSUM(_tx_cap)   ((_tx_cap) & DEV_TX_OFFLOAD_TCP_CKSUM)
#define RW_PIOT_TX_OFFLOAD_CAP_SCTP_CKSUM(_tx_cap)  ((_tx_cap) & DEV_TX_OFFLOAD_SCTP_CKSUM)

static __inline__ int
rw_piot_burst_rx(rw_piot_api_handle_t api_handle,
                 uint8_t qid,
                 struct rte_mbuf **pkts,
                 unsigned int pkt_cnt)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  DP_ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    printf("Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_rx_burst(rw_piot_dev->rte_port_id, qid, pkts, pkt_cnt));
}

static __inline__ int
rw_piot_burst_tx(rw_piot_api_handle_t api_handle,
                 uint8_t qid,
                 struct rte_mbuf **pkts,
                 unsigned int pkt_cnt)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  DP_ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    printf("Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_tx_burst(rw_piot_dev->rte_port_id, qid, pkts, pkt_cnt));
}


typedef void  (* f_rw_piot_log_t)(void *instance_ptr,  uint32_t loglevel, const char *s);

int rw_piot_init(int argc, char **argv, void *instance_ptr, f_rw_piot_log_t log_fn);

rw_piot_api_handle_t
rw_piot_open(char *device_name,
             rw_piot_open_request_info_t *req_info,
             rw_piot_open_response_info_t *rsp_info);
             

int
rw_piot_close(rw_piot_api_handle_t api_handle);
/**
 * This function called by piot will make an ioctl to the kernel to get the socket
 * filters if any
 * @param[in]  kni  - kni instance
 * @param[in]  inode-id - inode-id of the AF_PACKET socket
 * @param[in]  skt - socket pointer for verification
 * @param[out] len - number of filters
 * @param[out ] filter - the actual filters
 *
 * @returns 0  if success
 * @returns -1 if error
 */
int rw_piot_get_packet_socket_info(struct rte_kni *kni, uint32_t inode_id,
                                   void *sk, unsigned short *len, 
                                   struct sock_filter *skt_filter);
int rw_piot_clear_kni_packet_counters(void);
  
int 
rw_piot_rx_queue_setup(rw_piot_api_handle_t api_handle,
                       uint16_t rx_queue_id,
                       uint16_t nb_rx_desc, 
                       unsigned int socket_id,
                       const struct rte_eth_rxconf *rx_conf,
                       struct rte_mempool *mp);
int 
rw_piot_tx_queue_setup(rw_piot_api_handle_t api_handle,
                       uint16_t tx_queue_id,
                       uint16_t nb_tx_desc, 
                       unsigned int socket_id,
                       const struct rte_eth_txconf *tx_conf);
int 
rw_piot_dev_start(rw_piot_api_handle_t api_handle);

int 
rw_piot_dev_stop(rw_piot_api_handle_t api_handle);

uint8_t
rw_piot_dev_count(void);

int
rw_piot_thread_set_affinity(void);

void
rw_piot_get_link_info(rw_piot_api_handle_t api_handle, 
                      rw_piot_link_info_t *eth_link_info);

int
rw_piot_set_led(rw_piot_api_handle_t api_handle, int on);

void
rw_piot_get_link_stats(rw_piot_api_handle_t api_handle, 
                       rw_piot_link_stats_t *stats);

void 
rw_piot_reset_link_stats(rw_piot_api_handle_t api_handle);

static inline void
rw_piot_get_macaddr(rw_piot_api_handle_t api_handle,
		    struct ether_addr *mac_addr)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  DP_ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  DP_ASSERT(mac_addr);
  rte_eth_macaddr_get(rw_piot_dev->rte_port_id, mac_addr);
  return;
}

uint32_t
rw_piot_get_rte_port_id(rw_piot_api_handle_t api_handle);


void
rw_piot_set_allmulticast(rw_piot_api_handle_t api_handle, int on);
void
rw_piot_set_promiscuous(rw_piot_api_handle_t api_handle, int on);

int
rw_piot_set_flow_ctrl(rw_piot_api_handle_t api_handle, rw_piot_port_fc_conf_t *fc_cfgp);
int
rw_piot_get_flow_ctrl(rw_piot_api_handle_t api_handle,
		      rw_piot_port_fc_conf_t *fc_cfgp);
struct rte_kni*
rw_piot_kni_create(rw_piot_api_handle_t api_handle, 
                   struct rte_kni_conf *conf,
                   struct rte_mempool * pktmbuf_pool);
int
rw_piot_kni_change_mtu(uint8_t port_id, unsigned new_mtu);
int
rw_piot_kni_config_network_interface(uint8_t port_id, uint8_t if_up);
int
rw_piot_kni_destroy(struct rte_kni *kni);

unsigned
rw_piot_kni_tx_burst(struct rte_kni *kni,
                     struct rte_mbuf **mbufs,
                     unsigned num);

unsigned
rw_piot_kni_rx_burst(struct rte_kni *kni,
                     struct rte_mbuf **mbufs,
                     unsigned num);

int
rw_piot_kni_handle_request(struct rte_kni *kni);

int rw_piot_set_tx_queue_stats_mapping(rw_piot_api_handle_t api_handle, uint16_t tx_queue_id, uint8_t stat_idx);
int rw_piot_set_rx_queue_stats_mapping(rw_piot_api_handle_t api_handle, uint16_t rx_queue_id, uint8_t stat_idx);

int rw_piot_dev_callback_register(rw_piot_api_handle_t api_handle,
                                  enum rte_eth_event_type event,
                                  rte_eth_dev_cb_fn cb_fn, void *cb_arg);
int rw_piot_dev_callback_unregister(rw_piot_api_handle_t api_handle,
                                    enum rte_eth_event_type event,
                                    rte_eth_dev_cb_fn cb_fn, void *cb_arg);
void
rw_piot_service_link_interrupts(void);
int rw_piot_is_not_uio(rw_piot_api_handle_t api_handle);
int rw_piot_is_ring(rw_piot_api_handle_t api_handle);
int rw_piot_is_raw(rw_piot_api_handle_t api_handle);
int rw_piot_mac_addr_add(rw_piot_api_handle_t api_handle, struct ether_addr *addr, uint32_t pool);
int
rw_piot_mac_addr_delete(rw_piot_api_handle_t api_handle, struct ether_addr *addr);
void rw_piot_exit_cleanup(void);

int rw_piot_get_device_info(rw_piot_api_handle_t api_handle, struct rte_eth_dev_info *dev_info);
int rw_piot_get_device_offload_capability(rw_piot_api_handle_t api_handle, uint32_t  *rx_offload_capa, uint32_t  *tx_offload_capa);

int rw_piot_update_mtu(rw_piot_api_handle_t api_handle, uint32_t mtu);

int rw_fpath_piot_kni_fifo_get(void *fifo, void **mbufs, int num);
int rw_fpath_piot_kni_fifo_put(void *fifo, void **mbufs, int num);
int
rw_piot_dev_reconfigure(rw_piot_api_handle_t api_handle,
                        rw_piot_open_request_info_t *req_info,
                        rw_piot_open_response_info_t *rsp_info);
__END_DECLS

#endif
