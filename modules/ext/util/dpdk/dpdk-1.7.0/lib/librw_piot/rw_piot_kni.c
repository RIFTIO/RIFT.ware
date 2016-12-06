
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
 * @file rw_piot_kni.c
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 02/12/2014
 * @brief Network Fastpath Packet I/O Tool Kit (PIOT) API - KNI (Kernel Network Interface) related API functions
 * 
 * @details
 */

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
#include <rte_kni.h>

#include "eal_private.h"
#include "rte_ethdev.h"

#include "rw_piot.h"

#define MAX_PACKET_SZ           2048
/* Total octets in ethernet header */
#define KNI_ENET_HEADER_SIZE    14
/* Total octets in the FCS */
#define KNI_ENET_FCS_SIZE       4

extern rw_piot_global_config_t rw_piot_global_config;

/*
 * Create KNI for specified device
 */
struct rte_kni *
rw_piot_kni_create(rw_piot_api_handle_t api_handle,
                   struct rte_kni_conf *conf,
                   struct rte_mempool * pktmbuf_pool)
{
  struct rte_kni_ops ops;
  struct rte_eth_dev_info dev_info;
  struct rte_kni *kni;
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  
  if ((conf->name[0] == 0)) {
    ASSERT(rw_piot_dev);
    rte_snprintf(conf->name, RTE_KNI_NAMESIZE,"vEth%u", rw_piot_dev->rte_port_id);
  }
  conf->mbuf_size = MAX_PACKET_SZ;

  memset(&dev_info, 0, sizeof(dev_info));
  memset(&ops, 0, sizeof(ops));
  if (rw_piot_dev) {
    rte_eth_dev_info_get(rw_piot_dev->rte_port_id, &dev_info);
    conf->group_id = (uint16_t)rw_piot_dev->rte_port_id;
    conf->addr = dev_info.pci_dev->addr;
    conf->id = dev_info.pci_dev->id;
    ops.port_id = rw_piot_dev->rte_port_id;
  }
  ops.change_mtu = rw_piot_kni_change_mtu;
  ops.config_network_if = rw_piot_kni_config_network_interface;
  
  kni = rte_kni_alloc(pktmbuf_pool, conf, &ops); 
  
  if (NULL == kni) {
    printf("rte_kni_alloc returned failure\n");
    return NULL;
  }

  return kni;
}

int 
rw_piot_kni_destroy(struct rte_kni *kni)
{
  rte_kni_release(kni);
  return(0); 
}

unsigned
rw_piot_kni_tx_burst(struct rte_kni *kni,
                     struct rte_mbuf **mbufs, 
                     unsigned num)
{
  return(rte_kni_tx_burst(kni, mbufs, num));
}

unsigned
rw_piot_kni_rx_burst(struct rte_kni *kni,
                     struct rte_mbuf **mbufs, 
                     unsigned num)
{
  return(rte_kni_rx_burst(kni, mbufs, num));
}

int
rw_piot_kni_handle_request(struct rte_kni *kni)
{
  return(rte_kni_handle_request(kni));
}

/* Callback for request of changing MTU */
int
rw_piot_kni_change_mtu(uint8_t port_id, unsigned new_mtu)
{
        //int ret;
        //struct rte_eth_conf conf;
        //struct rte_eth_conf port_conf;
        int i;
        rw_piot_device_t *rw_piot_dev = NULL;

        for (i=0; i<RWPIOT_MAX_DEVICES; i++) {
          if ((rw_piot_global_config.device[i].used) && (rw_piot_global_config.device[i].rte_port_id == port_id)) {  
            rw_piot_dev = &rw_piot_global_config.device[i];
          }
        }
 
       if (NULL == rw_piot_dev) {
         printf("Could not find port, rw_piot_kni_change_mtu\n");
         return -1;
       }

        printf("Change MTU of port %d to %u\n",port_id, new_mtu);

#if 0
        /* Stop specific port */
        rte_eth_dev_stop(port_id);
       
        /*
         * How to get port conf here. This will reset the port conf ??? - TBD
         */
        bzero(&port_conf, sizeof(port_conf));

        memcpy(&conf, &port_conf, sizeof(conf));
        /* Set new MTU */
        if (new_mtu > ETHER_MAX_LEN)
                conf.rxmode.jumbo_frame = 1;
        else
                conf.rxmode.jumbo_frame = 0;

        /* mtu + length of header + length of FCS = max pkt length */
        conf.rxmode.max_rx_pkt_len = new_mtu + KNI_ENET_HEADER_SIZE +
            KNI_ENET_FCS_SIZE;
        ret = rte_eth_dev_configure(port_id, 1, 1, &conf);
        if (ret < 0) {
                printf("Fail to reconfigure port %d\n", port_id);
                return ret;
        }
        /* Restart specific port */
        ret = rte_eth_dev_start(port_id);
        if (ret < 0) {
                printf("Fail to restart port %d\n", port_id);
                return ret;
        }
#endif
        /*
         * Call the callback to inform upper layer
         */
#if 0
        if (rw_piot_dev->kni_change_mtu) {
          rw_piot_dev->kni_change_mtu(rw_piot_dev->piot_api_handle, new_mtu);
        }
#endif   
        return 0;
}

/* Callback for request of configuring network interface up/down */
int
rw_piot_kni_config_network_interface(uint8_t port_id, uint8_t if_up)
{
        int ret = 0, i;
        rw_piot_device_t *rw_piot_dev = NULL;

        for (i=0; i<RWPIOT_MAX_DEVICES; i++) {
          if ((rw_piot_global_config.device[i].used) && (rw_piot_global_config.device[i].rte_port_id == port_id)) {
            rw_piot_dev = &rw_piot_global_config.device[i];
          }
        }

       if (NULL == rw_piot_dev) {
         printf("Could not find port, rw_piot_kni_change_mtu\n");
         return -1;
       }


        printf("Configure network interface of %d %s\n", port_id, if_up ? "up" : "down");

#if 0
        if (if_up != 0) { /* Configure network interface up */
                rte_eth_dev_stop(port_id);
                ret = rte_eth_dev_start(port_id);
        } else /* Configure network interface down */
                rte_eth_dev_stop(port_id);

        if (ret < 0)
                printf("Failed to start port %d\n", port_id);

        if (rw_piot_dev->kni_config_network_if) {
          rw_piot_dev->kni_config_network_if(rw_piot_dev->piot_api_handle, if_up);
        }

#endif
        return ret;
}


int rw_fpath_piot_kni_fifo_get(void *fifo, void **mbufs, int num)
{
  return rte_kni_fifo_get(fifo, mbufs, num);
}


int rw_fpath_piot_kni_fifo_put(void *fifo, void **mbufs, int num)
{
  return rte_kni_fifo_put(fifo, mbufs, num);
}
