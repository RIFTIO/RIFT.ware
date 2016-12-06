/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PIOT  API testing using RING PMD drivers
 */

/*
 * Author: Sanil Puthiyandyil
 *         Riftio Inc
 */

#ifdef RTE_TEST_RW_PIOT

#include <stdio.h>

#include <rte_eth_ring.h>
#include <rte_ethdev.h>
#include <assert.h>
#include <rw_piot_api.h>

#undef ASSERT
#define ASSERT assert

static struct rte_mempool *mp;
static uint8_t idx;

#define SOCKET0 0

#define RING_SIZE 256

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   512

rw_piot_api_handle_t piot_handle[8];
int test_rw_piot(int argc, char **argv);

struct {
   void *handle;
}  my_instance;

static int
test_rw_piot_setup_queues(void)
{
  int ret;
  struct rte_eth_link link;
  
  ret = rw_piot_tx_queue_setup(piot_handle[0], 0, RING_SIZE, SOCKET0, NULL);
  ASSERT(ret>=0);
  ret = rw_piot_rx_queue_setup(piot_handle[0], 0, RING_SIZE, SOCKET0, NULL, mp); 
  ASSERT(ret>=0);

  ret = rw_piot_tx_queue_setup(piot_handle[1], 0, RING_SIZE, SOCKET0, NULL);
  ASSERT(ret>=0);
  ret = rw_piot_rx_queue_setup(piot_handle[1], 0, RING_SIZE, SOCKET0, NULL, mp); 
  ASSERT(ret>=0);

  ret = rw_piot_tx_queue_setup(piot_handle[2], 0, RING_SIZE, SOCKET0, NULL);
  ASSERT(ret>=0);
  ret = rw_piot_rx_queue_setup(piot_handle[2], 0, RING_SIZE, SOCKET0, NULL, mp); 
  ASSERT(ret>=0);

  rw_piot_get_link_info(piot_handle[0], &link);
  rw_piot_get_link_info(piot_handle[1], &link);
  rw_piot_get_link_info(piot_handle[2], &link);

  return 0;
}

static int
test_rw_piot_send_packets(void)
{
  struct rte_mbuf  bufs[RING_SIZE];
  struct rte_mbuf *pbufs[RING_SIZE];
  int i;

  printf("Testing ring pmd RX/TX\n");

  for (i = 0; i < RING_SIZE/2; i++)
    pbufs[i] = &bufs[i];

  if (rw_piot_burst_tx(piot_handle[0], 0, pbufs, RING_SIZE/2) < RING_SIZE/2) {
    printf("test_rw_piot_send_packets: Failed to transmit packet burst\n");
    return -1;
  }

  if (rw_piot_burst_rx(piot_handle[0], 0, pbufs, RING_SIZE) != RING_SIZE/2) {
    printf("test_rw_piot_send_packets: Failed to receive packet burst\n");
    return -1;
  }

  for (i = 0; i < RING_SIZE/2; i++)
    if (pbufs[i] != &bufs[i]) {
      printf("test_rw_piot_send_packets: Error: received data does not match that transmitted\n");
      return -1;
    }

  return 0;
}

static int
test_get_stats(void)
{
  struct rte_eth_stats stats;
  struct rte_mbuf buf, *pbuf = &buf;

  printf("Testing PIOT PMD stats\n");

  /* check stats of RXTX port, should all be zero */
  rw_piot_get_link_stats(piot_handle[1], &stats);
  if (stats.ipackets != 0 || stats.opackets != 0 ||
      stats.ibytes != 0 || stats.obytes != 0 ||
      stats.ierrors != 0 || stats.oerrors != 0) {
    printf("Error:  port stats are not zero\n");
    return -1;
  }

  /* send and receive 1 packet and check for stats update */
  if (rw_piot_burst_tx(piot_handle[1], 0, &pbuf, 1) != 1) {
    printf("Error sending packet to port\n");
    return -1;
  }
  if (rw_piot_burst_rx(piot_handle[1], 0, &pbuf, 1) != 1) {
    printf("Error receiving packet from port\n");
    return -1;
  }

  rw_piot_get_link_stats(piot_handle[1], &stats);

  if (stats.ipackets != 1 || stats.opackets != 1 ||
      stats.ibytes != 0 || stats.obytes != 0 ||
      stats.ierrors != 0 || stats.oerrors != 0) {
    printf("Error: port stats are not as expected\n");
    return -1;
  }
  return 0;
}

static int
test_stats_reset(void)
{
  struct rte_eth_stats stats;
  struct rte_mbuf buf, *pbuf = &buf;

  printf("Testing PIOT PMD stats reset\n");

  rw_piot_reset_link_stats(piot_handle[2]);
  
  /* check stats of RXTX port, should all be zero */
  rw_piot_get_link_stats(piot_handle[2], &stats);
  if (stats.ipackets != 0 || stats.opackets != 0 ||
      stats.ibytes != 0 || stats.obytes != 0 ||
      stats.ierrors != 0 || stats.oerrors != 0) {
    printf("Error: port stats are not zero\n");
    return -1;
  }

  /* send and receive 1 packet and check for stats update */
  if (rw_piot_burst_tx(piot_handle[2], 0, &pbuf, 1) != 1) {
    printf("Error sending packet to port\n");
    return -1;
  }

  if (rw_piot_burst_rx(piot_handle[2], 0, &pbuf, 1) != 1) {
    printf("Error receiving packet from RXTX port\n");
    return -1;
  }

  rw_piot_get_link_stats(piot_handle[2], &stats);
  if (stats.ipackets != 1 || stats.opackets != 1 ||
      stats.ibytes != 0 || stats.obytes != 0 ||
      stats.ierrors != 0 || stats.oerrors != 0) {
    printf("Error: port stats are not as expected\n");
    return -1;
  }

  rw_piot_reset_link_stats(piot_handle[2]);
  
  /* check stats of RXTX port, should all be zero */
  rw_piot_get_link_stats(piot_handle[2], &stats);
  if (stats.ipackets != 0 || stats.opackets != 0 ||
      stats.ibytes != 0 || stats.obytes != 0 ||
      stats.ierrors != 0 || stats.oerrors != 0) {
    printf("Error: port stats are not zero\n");
    return -1;
  }

  return 0;
}

int
test_rw_piot(int argc, char **argv)
{
  int ret;
  rw_piot_open_request_info_t req_param;
  rw_piot_open_response_info_t rsp_param;
  char devname[256];

  /* Init PIOT */
  ret = rw_piot_init(argc, argv, (void *)&my_instance, NULL);
  ASSERT(ret>=0);
  printf("PIOT Init Successful\n");

  idx = rw_piot_dev_count();
  /* No devices opened yet */
  ASSERT(idx == 0);

  /* Open first Ring device */
  bzero(&req_param, sizeof(req_param));
  req_param.num_rx_queues = 1;
  req_param.num_tx_queues = 1;
  strcpy(devname, "eth_ring:name=ring0");
  piot_handle[idx] = rw_piot_open(devname, &req_param, &rsp_param);
  ASSERT(piot_handle[idx] != 0);
  ASSERT(rw_piot_dev_count() == 1);
  printf("PIOT device open %s - Successful\n", devname);


  /* Open second Ring device */
  idx ++;
  bzero(&req_param, sizeof(req_param));
  req_param.num_rx_queues = 4;
  req_param.num_tx_queues = 4;
  strcpy(devname, "eth_ring:name=ring1");
  piot_handle[idx] = rw_piot_open(devname, &req_param, &rsp_param);
  ASSERT(piot_handle[idx] != 0);
  ASSERT(rw_piot_dev_count() == 2);
  printf("PIOT device open %s - Successful\n", devname);

  /* Open third Ring device */
  idx ++;
  bzero(&req_param, sizeof(req_param));
  req_param.num_rx_queues = 16;
  req_param.num_tx_queues = 16;
  strcpy(devname, "eth_ring:name=ring2");
  piot_handle[idx] = rw_piot_open(devname, &req_param, &rsp_param);
  ASSERT(piot_handle[idx] != 0);
  ASSERT(rw_piot_dev_count() == 3);
  printf("PIOT device open %s - Successful\n", devname);

  mp = rte_mempool_create("mbuf_pool", NB_MBUF,
      MBUF_SIZE, 32,
      sizeof(struct rte_pktmbuf_pool_private),
      rte_pktmbuf_pool_init, NULL,
      rte_pktmbuf_init, NULL,
      rte_socket_id(), 0);

  ASSERT(mp);


  ASSERT(test_rw_piot_setup_queues() >=0);
  printf("PIOT Queue setup Successful\n");


  ret = rw_piot_dev_start(piot_handle[0]);
  ASSERT(ret == 0);
  ret = rw_piot_dev_start(piot_handle[1]);
  ASSERT(ret == 0);
  ret = rw_piot_dev_start(piot_handle[2]);
  ASSERT(ret == 0);
  printf("PIOT device start Successful\n");

  ASSERT(test_rw_piot_send_packets() >= 0);
  printf("PIOT RX/TX  - Successful\n");

  ASSERT(test_get_stats() >= 0);
  printf("PIOT Get Stats  - Successful\n");

  ASSERT(test_stats_reset() >= 0);
  printf("PIOT Reset Stats  - Successful\n");

  rw_piot_dev_stop(piot_handle[0]);
  rw_piot_dev_stop(piot_handle[1]);
  rw_piot_dev_stop(piot_handle[2]);
  printf("PIOT Stopping Devices  - Successful\n");

  return 0;
}


int
main(int argc, char **argv)
{
  ASSERT(test_rw_piot(argc, argv) == 0);

  return 0;
}

#else

int
test_rw_piot(int argc, char **argv)
{
  return 0;
}

#endif

