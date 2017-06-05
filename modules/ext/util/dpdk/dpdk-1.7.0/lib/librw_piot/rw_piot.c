/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_piot.c
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 02/03/2014
 * @brief Network Fastpath Packet I/O Tool Kit (PIOT) utility routines
 * 
 * @details
 */

#include "rw_piot.h"


rw_piot_global_config_t rw_piot_global_config;

rw_piot_device_t *
rw_piot_device_alloc(rw_piot_devgroup_t *devgroup)
{
  int i;
  rw_piot_device_t *dev = NULL;

  for (i=0; i<RWPIOT_MAX_DEVICES; i++) {
    if (0 == rw_piot_global_config.device[i].used) {
      dev = &rw_piot_global_config.device[i];
      dev->used = 1;
      dev->device_group = devgroup;
      dev->device_group->num_open_instances ++;
      dev->piot_api_handle = RWPIOT_GENERATE_HANDLE(dev);
      break;
    }
  }
  ASSERT(RWPIOT_VALID_DEVICE(dev));
  return(dev);
}

int
rw_piot_device_free(rw_piot_device_t *dev)
{
  ASSERT(RWPIOT_VALID_DEVICE(dev));
  if ((NULL == dev) || (0 == dev->used)) {
    return -1;
  }
  dev->used = 0;
  dev->piot_api_handle = 0;
  dev->device_group->num_open_instances --;
  dev->device_group = NULL;
  return 0;
}

int
rw_piot_config_device(rw_piot_device_t *dev,
                      rw_piot_open_request_info_t *req,
                      rw_piot_open_response_info_t *rsp)
{
  struct rte_eth_dev_info dev_info;

  ASSERT(RWPIOT_VALID_DEVICE(dev));

  bzero(rsp, sizeof(*rsp));
  bzero(&dev_info, sizeof(dev_info));

  rte_eth_dev_info_get(dev->rte_port_id, &dev_info);
  if (0 == dev_info.max_rx_queues ||  0 == dev_info.max_tx_queues) {
    printf("Eth device return 0 max rx/tx queues\n");
    return -1;
  } 
  rsp->num_rx_queues = req->num_rx_queues;
  if (rsp->num_rx_queues > dev_info.max_rx_queues) {
    rsp->num_rx_queues = dev_info.max_rx_queues;
  }
  rsp->num_tx_queues = req->num_tx_queues;
  if (rsp->num_tx_queues > dev_info.max_tx_queues) {
    rsp->num_tx_queues = dev_info.max_tx_queues;
  }

  if (!dev_info.rx_offload_capa) {
    req->dev_conf.rxmode.hw_ip_checksum = 0;
  }

  if (dev_info.pci_dev->driver) {
    if (!(dev_info.pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)) {
      req->dev_conf.intr_conf.lsc = 0;
    }
  }

  return(rte_eth_dev_configure(dev->rte_port_id,  rsp->num_rx_queues,  rsp->num_tx_queues, &req->dev_conf));
}


