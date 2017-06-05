/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_piot_api.c
 * @author Sanil Puthiyandyil (sanil.puthiyandyil@riftio.com)
 * @date 02/03/2014
 * @brief Network Fastpath Packet I/O Tool Kit (PIOT) API
 * 
 * @details This file contains routines that provide a Packet I/O Toolkit
 * API ontop of DPDK, raw-sockets, ring-buffers, and libPCAP
 */

#define _GNU_SOURCE 
#include <sched.h>

#include "rw_piot.h"
#include "rte_dev.h"



static rw_piot_devgroup_t  rw_piot_devgroup[] = { {"eth_uio", PIOT_PCI,     0},
                                                  {"eth_raw", PIOT_NON_PCI, 0},
                                                  {"eth_ring",PIOT_NON_PCI, 0},
                                                  {"eth_pcap",PIOT_NON_PCI, 0},
                                                  {"eth_sim", PIOT_NON_PCI, 0},
                                                  {"eth_tun", PIOT_NON_PCI, 0}
                                                };
#define NUM_SUPPORTED_DEVICETYPES (sizeof(rw_piot_devgroup)/sizeof(rw_piot_devgroup[0]))

extern rw_piot_global_config_t rw_piot_global_config;
extern int dpdk_system_setup(int no_huge, int req_memory);

static uint8_t piot_initialized = 0;

rw_piot_lcore_info_t rw_piot_lcore[RTE_MAX_LCORE];

#define RW_PIOT_LOG(level, ...) ({ \
                                          char msg[256];                                    \
                                          snprintf(msg, sizeof(msg), __VA_ARGS__);  \
                                          rw_piot_log_handler(level, msg);                  \
                                         })

static void
rw_piot_log_handler(uint32_t loglevel, const char *buf)
{
  
  if (rw_piot_global_config.log_fn) {
     rw_piot_global_config.log_fn(rw_piot_global_config.instance_ptr, loglevel, buf);
  }
  else {
    printf("FPATH: ");
    fflush(stdout);
    fwrite(buf, 1, strlen(buf), stdout);
    fflush(stdout);
  }
}                                


/*
 * Initialize PIOT Module
 */
int
rw_piot_init(int argc, char **argv, void *instance_ptr, f_rw_piot_log_t log_fn)
{

  int rc, ret;
  int i, no_huge = 0, memory_req = 0;
  struct rte_config *cfg = rte_eal_get_configuration();

/*
 * TBD - thread safety
 */

  if (piot_initialized) {
    int num_arg;

    for (num_arg = 0; num_arg < argc; num_arg ++) {
      if (strcmp(argv[num_arg], "--") == 0) {
        break;
      } 
    }
    return num_arg;
  }
  piot_initialized = 1;


  memset(&rw_piot_global_config, 0, sizeof(rw_piot_global_config));
  ASSERT(instance_ptr);
  rw_piot_global_config.instance_ptr = instance_ptr;
  rw_piot_global_config.log_fn = log_fn;

  memset(&(rw_piot_lcore[0]), 0, sizeof(rw_piot_lcore));
  
  for (i=0; i<argc; i++) {
    if (strcmp("--no-huge", argv[i]) == 0) {
      no_huge = 1;
      RW_PIOT_LOG(RTE_LOG_INFO, "PIOT: Huge pages disabled by --no-huge cmdarg\n");
    }
    if (strcmp("-m", argv[i]) == 0) {
      if ((i+1) <argc) {
        memory_req = atoi(argv[i+1]);
        RW_PIOT_LOG(RTE_LOG_INFO, "PIOT: -m cmdarg setting requested memory to %d mb\n", memory_req);
      }
    }
    if (strcmp("--", argv[i]) == 0) {
      break;
    }
  }
  /*
   * setup system environment for DPDK
   */
  rc = dpdk_system_setup(no_huge, memory_req);
  if (rc < 0) {
    return rc;
  }

  rte_set_application_log_hook(rw_piot_log_handler);

  /*
   * Init DPDK EAL without doing thread related inits
   */
  ret = rte_eal_init_no_thread(argc, argv);
  if (ret < 0) {
    return ret;
  }
  if (geteuid() == 0){
    rte_kni_init(RWPIOT_MAX_KNI_DEVICES);
  }
 /*
  * Assign master lcore-id. Should be passed in the init - TBD
  */

  cfg->master_lcore = 0;  /* This wil be fixed with RW.Sched integration -  TBD */

  /* set the lcore ID in per-lcore memory area */
  RTE_PER_LCORE(_lcore_id) = cfg->master_lcore;

  /* set CPU affinity for master thread ??? TBD*/
  // if (eal_thread_set_affinity() < 0)
  //    rte_panic("cannot set affinity\n");

  rte_timer_subsystem_init();

  return ret; /* number of args consumed by rte_eal_init_no_thread */
}

static int
rwpiot_pci_addr_format(char *buf, int bufsize)
{
#ifndef PCI_FMT_NVAL
 #define PCI_FMT_NVAL 4
#endif
  /* first split on ':' */
  union splitaddr {
    struct {
      char *domain;
      char *bus;
      char *devid;
      char *function;
    };
    char *str[PCI_FMT_NVAL]; /* last element-separator is "." not ":" */
  } splitaddr;
  
  char *buf_copy = strndup(buf, bufsize);
  if (buf_copy == NULL)
    return -1;
  
  if (rte_strsplit(buf_copy, bufsize, splitaddr.str, PCI_FMT_NVAL, ':')
      != PCI_FMT_NVAL - 1)
    goto error;
  /* final split is on '.' between devid and function */
  splitaddr.function = strchr(splitaddr.devid,'.');
  if (splitaddr.function == NULL)
    goto error;
  *splitaddr.function++ = '\0';
  
  /* now convert to int values */
  errno = 0;
  if (errno != 0)
    goto error;
  
  free(buf_copy); /* free the copy made with strdup */
  return 0;
error:
  free(buf_copy);
  return -1;
}


/*
 * Open a PIOT device and return a non_zero handle
 * Input:
 *   Device name mush be of the format "<device-type>:<devicespecific-info>"
 *   Supported types:
 *                    "eth_uio:pci=<n>:<n>:<n>.<n>"
 *                    "eth_raw:iface=<name>"
 *                    "eth_ring:name=<name>"
 *                    "eth_pcap:tx_pcap (or tx_iface)=<name>;rx_pcap (or rx_iface)=name=" 
 *                    "eth_sim:iface=<name>"
 *                    "eth_tun:iface=<name>"
 * Return:
 *   non_zero rw_piot_api_handle_t, if success
 *   0, if failure
 */

rw_piot_api_handle_t
rw_piot_open(char *device_name,
             rw_piot_open_request_info_t *req_info,
             rw_piot_open_response_info_t *rsp_info)
{
  int n;
  uint32_t i;
  char *devname_split[2];
  char *split_args[2];
  char *dev_arg;
  char received_name[RW_PIOT_DEVICE_NAME_LEN];
  struct rte_eth_dev *eth_dev = NULL;
  rw_piot_device_t *rw_piot_dev = NULL;

  ASSERT(device_name);
  ASSERT(req_info);
  ASSERT(rsp_info);

  if (NULL == device_name ||
      NULL == req_info ||
      NULL == rsp_info) {
    ASSERT(0);
    /* failure */
    return(0);
  }

  /*
   * copy device name to a buffer, which can be give to split
   */
  strncpy(received_name, device_name, sizeof(received_name));

  /* parse the device name */
  //printf("split %s\n", received_name);
  n = rte_strsplit(received_name, strlen(received_name), devname_split, 2, ':'); 
  if (n != 2) {
     RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: split failed %s, %d\n", device_name, n);
     ASSERT(0);
    return(0);
  }

  //printf("Token 1 %s Token 2 %s \n",devname_split[0], devname_split[1]);

  /* check the device is already opened */
 for (i=0; i<RWPIOT_MAX_DEVICES; i++) {
    if (rw_piot_global_config.device[i].used) {
      if (!strncmp(received_name, rw_piot_global_config.device[i].device_name, sizeof(rw_piot_global_config.device[i].device_name))) {
        RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: failed, found already opened device\n");
        ASSERT(0);
        return(0);
      }
    }
  }

   if (strcmp(devname_split[0], "eth_pcap"))  {
     /* if the device is not pcap, strip-off = and characters before */
      n = rte_strsplit(devname_split[1], strlen(devname_split[1]), split_args, 2, '='); 
      if (n != 2) {
          return 0;
       }
       dev_arg = split_args[1];
   }
   else {
       dev_arg = devname_split[1]; 
   }

   for (i=0; i<NUM_SUPPORTED_DEVICETYPES; i++) {
     if (!strncmp(rw_piot_devgroup[i].device_group_name, devname_split[0], 32)) {
       char dev_inst_name[RW_PIOT_DEVICE_NAME_LEN];

       sprintf(dev_inst_name, "%s%d",devname_split[0], rw_piot_devgroup[i].num_open_instances);
       switch (rw_piot_devgroup[i].device_group_type) {
         case PIOT_PCI: {
           struct rte_pci_device *pci_dev;
           if (rwpiot_pci_addr_format(dev_arg, strlen(dev_arg))){
             RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: BAD pci address %s\n", dev_arg);
             return 0;
           }
           //printf("pci address %s\n",dev_arg);
           pci_dev = rte_eal_pci_probe_by_pci_addr(dev_arg);
             if (pci_dev) {
               eth_dev = rte_eth_dev_find_dev_by_pci(pci_dev);
               if (eth_dev == NULL) {
                 RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: rte_eth_dev_find_dev_by_pci failed\n");
               }
             }
             else {
               RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: rte_eal_pci_probe_by_pci_addr failed\n");
             }
           }
         break;
         case PIOT_NON_PCI: {
           req_info->dev_conf.intr_conf.lsc = 0;
           //printf("Non-PCI %s %s\n", dev_inst_name, dev_arg);
           if (!rte_eal_non_pci_ethdev_init_by_devname(devname_split[0],
                                                       dev_inst_name, dev_arg)) {
             /*
              * Caution - DIRTY Code here.
              * Assumption is not to change DPDDK existing code, but can add new
              * functions to DPDK.
              * This is the only way to get the eth_dev for the init we have
              * done here from dpdk
              * This will be cleaned up later - TBD
              */
             //printf("Non-PCI Init success\n");
             eth_dev = rte_eth_dev_get_last_eth_dev(); /* Assume the last eth_dev is added for this case, BAD code, temporarily */
             if (!eth_dev) {
               RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: Non-PCI couldnot find ethdev\n");
             }
           }
         }
         break;
         default:
           break;
       }
       break;
     }
   }

   if (eth_dev) {
     rw_piot_dev = rw_piot_device_alloc(&rw_piot_devgroup[i]);
     if (rw_piot_dev) {
       rw_piot_dev->rte_port_id = eth_dev->data->port_id;
       strncpy(rw_piot_dev->device_name, device_name, sizeof(rw_piot_dev->device_name));
       rsp_info->NUMA_affinity = rte_eth_dev_socket_id( eth_dev->data->port_id);
       return(rw_piot_dev->piot_api_handle);
     }
     else {
       RW_PIOT_LOG(RTE_LOG_CRIT, "PIOT: rw_piot_open: rw_piot_device_alloc failed\n");
     }
   }
        
   return(0);
}

int
rw_piot_close(rw_piot_api_handle_t api_handle) 
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    printf("Could not fine device by handle\n");
    return -1;
  }

  /*
   * Close the device
   */
  rte_eth_dev_close(rw_piot_dev->rte_port_id);

  rw_piot_device_free(rw_piot_dev);
  return 0;
}

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
int rw_piot_get_packet_socket_info(struct rte_kni *kni,
                                   uint32_t inode_id,
                                   void *skt, unsigned short *len, 
                                   struct sock_filter *skt_filter)
{
  if (*len < BPF_MAXINSNS){
    return -1;
  }

#ifdef RTE_LIBRW_PIOT
  return rte_kni_get_packet_socket_info(kni, inode_id, skt, len, skt_filter);
#else
  return 0;
#endif
  
  return -1; 
}

int rw_piot_clear_kni_packet_counters(void)
{
  return rte_kni_clear_packet_counters();
}



int 
rw_piot_rx_queue_setup(rw_piot_api_handle_t api_handle,
                       uint16_t rx_queue_id,
                       uint16_t nb_rx_desc, 
                       unsigned int socket_id,
                       const struct rte_eth_rxconf *rx_conf,
                       struct rte_mempool *mp)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_rx_queue_setup(rw_piot_dev->rte_port_id, rx_queue_id, nb_rx_desc, socket_id, rx_conf, mp));
}


int 
rw_piot_tx_queue_setup(rw_piot_api_handle_t api_handle,
                       uint16_t tx_queue_id,
                       uint16_t nb_tx_desc, 
                       unsigned int socket_id,
                       const struct rte_eth_txconf *tx_conf)

{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_tx_queue_setup(rw_piot_dev->rte_port_id, tx_queue_id, nb_tx_desc, socket_id, tx_conf));
}

int 
rw_piot_dev_start(rw_piot_api_handle_t api_handle)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_dev_start(rw_piot_dev->rte_port_id));
}

int
rw_piot_dev_reconfigure(rw_piot_api_handle_t api_handle,
                        rw_piot_open_request_info_t *req_info,
                        rw_piot_open_response_info_t *rsp_info)
{
  int ret;
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  if (rw_piot_dev->device_group->device_group_type != PIOT_PCI) {
    req_info->dev_conf.intr_conf.lsc = 0;
  }
  ret = rw_piot_config_device(api_handle, req_info, rsp_info);

  return ret;
}

int 
rw_piot_dev_stop(rw_piot_api_handle_t api_handle)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  rte_eth_dev_stop(rw_piot_dev->rte_port_id);
  return 0;
}

int rw_piot_is_not_uio(rw_piot_api_handle_t api_handle)
{
 rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev ){
    return 0;
  }
  if (strstr(rw_piot_dev->device_group->device_group_name, "uio"))
    return 0;
  
  return 1;
}


int rw_piot_is_ring(rw_piot_api_handle_t api_handle)
{
 rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev ){
    return 0;
  }
  if (strstr(rw_piot_dev->device_group->device_group_name, "ring"))
    return 1;
  
  return 0;
}



int rw_piot_is_raw(rw_piot_api_handle_t api_handle)
{
 rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev ){
    return 0;
  }
  if (strstr(rw_piot_dev->device_group->device_group_name, "raw"))
    return 1;
  
  return 0;
}



int rw_piot_is_virtio(rw_piot_api_handle_t api_handle)
{
 rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
 struct rte_eth_dev_info dev_info;
 
 memset(&dev_info, 0, sizeof(dev_info));

 ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
 if (NULL == rw_piot_dev ){
   return 0;
 }
 rte_eth_dev_info_get(rw_piot_dev->rte_port_id, &dev_info);
 if (strstr(dev_info.driver_name, "rte_virtio_pmd")){
   return 1;
 }
 
 return 0;
}

void
rw_piot_get_hw_link_info(rw_piot_api_handle_t api_handle, 
                         rw_piot_link_info_t *eth_link_info, int wait)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev || NULL == eth_link_info) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle or invalid input param\n");
    return;
  }
  rte_eth_hw_link_get(rw_piot_dev->rte_port_id, eth_link_info, wait);
}

void 
rw_piot_get_link_info(rw_piot_api_handle_t api_handle,
                      rw_piot_link_info_t *eth_link_info, int wait)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev || NULL == eth_link_info) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle or invalid input param\n");
    return;
  }
  if (wait){
    rte_eth_link_get(rw_piot_dev->rte_port_id, eth_link_info);
  }else{
    rte_eth_link_get_nowait(rw_piot_dev->rte_port_id, eth_link_info);
  }
  return;
}

void 
rw_piot_get_link_stats(rw_piot_api_handle_t api_handle, rw_piot_link_stats_t *stats)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev || NULL == stats) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle or invalid input param\n");
    return;
  }
  rte_eth_stats_get(rw_piot_dev->rte_port_id, stats);
  return;
}

int
rw_piot_get_extended_stats(rw_piot_api_handle_t api_handle,
                           struct rte_eth_xstats *xstats, unsigned n)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle or invalid input param\n");
    return -1;
  }
  return rte_eth_xstats_get(rw_piot_dev->rte_port_id,
                            xstats, n);
}


void 
rw_piot_reset_link_stats(rw_piot_api_handle_t api_handle)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle or invalid input param\n");
    return;
  }
  rte_eth_stats_reset(rw_piot_dev->rte_port_id);
  rte_eth_xstats_reset(rw_piot_dev->rte_port_id);
  return;
}


uint8_t
rw_piot_dev_count(void)
{
  return(rte_eth_dev_count());
}


int 
rw_piot_set_led(rw_piot_api_handle_t api_handle, int on) 
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  int ret;
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  if (on) {
    ret = rte_eth_led_on(rw_piot_dev->rte_port_id);
  }
  else {
    ret = rte_eth_led_off(rw_piot_dev->rte_port_id);
  }
  return ret;
}


int
rw_piot_set_mc_addr_list(rw_piot_api_handle_t api_handle,
                         struct ether_addr *mc_addr_set,
                         uint32_t nb_mc_addr)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle to set multicast addresses\n");
    return -1;
  }

  
  return rte_eth_dev_set_mc_addr_list(rw_piot_dev->rte_port_id, mc_addr_set, nb_mc_addr);  
}

void
rw_piot_set_allmulticast(rw_piot_api_handle_t api_handle, int on)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  if (!RWPIOT_VALID_DEVICE(rw_piot_dev)){
    return;
  }
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return;
  }
  if (on) {
    rte_eth_allmulticast_enable(rw_piot_dev->rte_port_id);
  }
  else {
    rte_eth_allmulticast_disable(rw_piot_dev->rte_port_id);
  }
  return;  
}
    
void 
rw_piot_set_promiscuous(rw_piot_api_handle_t api_handle, int on) 
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return;
  }
  if (on) {
     rte_eth_promiscuous_enable(rw_piot_dev->rte_port_id);
  }
  else {
     rte_eth_promiscuous_disable(rw_piot_dev->rte_port_id);
  }
  return;
}

/**
 * rw_piot_set_flow_ctrl()
 *
 * Setup flow-control on an Ethernet device
 */
int
rw_piot_set_flow_ctrl(rw_piot_api_handle_t api_handle,
		      rw_piot_port_fc_conf_t *fc_cfgp)
{
  int rc = 0;
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(NULL != fc_cfgp);
  

  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    rc = -1;
  }
  else {
    if (rw_piot_dev->device_group->device_group_type == PIOT_PCI) {
      rc = rte_eth_dev_flow_ctrl_set(rw_piot_dev->rte_port_id, fc_cfgp);
      if (rc != 0) {
        RW_PIOT_LOG(RTE_LOG_ERR, "PIOT rte_eth_dev_flow_ctrl_set() returned error_code=%d\n",rc);
      }
    }
  }

  return rc;
}



/**
 * rw_piot_get_flow_ctrl()
 *
 * Get flow-control on an Ethernet device
 */
int
rw_piot_get_flow_ctrl(rw_piot_api_handle_t api_handle,
		      rw_piot_port_fc_conf_t *fc_cfgp)
{
  int rc = 0;
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(NULL != fc_cfgp);
  

  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    rc = -1;
  }
  else {
    if (rw_piot_dev->device_group->device_group_type == PIOT_PCI) {
      rc = rte_eth_dev_flow_ctrl_get(rw_piot_dev->rte_port_id, fc_cfgp);
      if (rc != 0) {
        RW_PIOT_LOG(RTE_LOG_ERR, "PIOT rte_eth_dev_flow_ctrl_set() returned error_code=%d\n",rc);
      }
    }
  }
  
  return rc;
}

int
rw_piot_set_tx_queue_stats_mapping(rw_piot_api_handle_t api_handle, uint16_t tx_queue_id, uint8_t stat_idx)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  int ret;
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  ret = rte_eth_dev_set_tx_queue_stats_mapping(rw_piot_dev->rte_port_id, tx_queue_id, stat_idx);
  return(ret);
}


int
rw_piot_set_rx_queue_stats_mapping(rw_piot_api_handle_t api_handle, uint16_t rx_queue_id, uint8_t stat_idx)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  int ret;
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  ret = rte_eth_dev_set_rx_queue_stats_mapping(rw_piot_dev->rte_port_id, rx_queue_id, stat_idx);
  return(ret);
}

int
rw_piot_update_mtu(rw_piot_api_handle_t api_handle, uint32_t mtu)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  if (!RWPIOT_VALID_DEVICE(rw_piot_dev))
    return 0;
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_dev_set_mtu(rw_piot_dev->rte_port_id, mtu));  
}


int
rw_piot_update_vlan_filter(rw_piot_api_handle_t api_handle, uint16_t vlan_id,
                           int on)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  if (!RWPIOT_VALID_DEVICE(rw_piot_dev))
    return 0;
  
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }

  return rte_eth_dev_vlan_filter(rw_piot_dev->rte_port_id,
                                 vlan_id, on);
}

int
rw_piot_update_vlan_strip(rw_piot_api_handle_t api_handle,int offload_mask)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);

  if (!RWPIOT_VALID_DEVICE(rw_piot_dev))
    return 0;
  
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return rte_eth_dev_set_vlan_offload(rw_piot_dev->rte_port_id, offload_mask);
}


int
rw_piot_get_vlan_offload(rw_piot_api_handle_t api_handle)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return rte_eth_dev_get_vlan_offload(rw_piot_dev->rte_port_id);
}


int
rw_piot_mac_addr_add(rw_piot_api_handle_t api_handle, struct ether_addr *addr, uint32_t pool)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_dev_mac_addr_add(rw_piot_dev->rte_port_id, addr, pool));
}

int
rw_piot_mac_addr_delete(rw_piot_api_handle_t api_handle, struct ether_addr *addr)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  if (!RWPIOT_VALID_DEVICE(rw_piot_dev))
    return 0;
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_dev_mac_addr_remove(rw_piot_dev->rte_port_id, addr));
}


int rw_piot_dev_callback_register(rw_piot_api_handle_t api_handle,
                                  enum rte_eth_event_type event,
                                  rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  if (rw_piot_dev->device_group->device_group_type == PIOT_PCI) {
    return(rte_eth_dev_callback_register(rw_piot_dev->rte_port_id, event, cb_fn, cb_arg));
  }
  return(0);
}

uint32_t
rw_piot_get_rte_port_id(rw_piot_api_handle_t api_handle)
{
   rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  DP_ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  return rw_piot_dev->rte_port_id;
}

int rw_piot_dev_callback_unregister(rw_piot_api_handle_t api_handle,
                                  enum rte_eth_event_type event,
                                  rte_eth_dev_cb_fn cb_fn, void *cb_arg)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  return(rte_eth_dev_callback_unregister(rw_piot_dev->rte_port_id, event, cb_fn, cb_arg));
}

int 
rw_piot_get_device_info(rw_piot_api_handle_t api_handle,
                        struct rte_eth_dev_info *dev_info)
{
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  ASSERT(dev_info);
  rte_eth_dev_info_get(rw_piot_dev->rte_port_id, dev_info);
  return 0;
}


int
rw_piot_get_device_offload_capability(rw_piot_api_handle_t api_handle,
                                      uint32_t  *rx_offload_capa,                   
                                      uint32_t  *tx_offload_capa)
{
  struct rte_eth_dev_info dev_info;
  rw_piot_device_t *rw_piot_dev = RWPIOT_GET_DEVICE(api_handle);
  ASSERT(RWPIOT_VALID_DEVICE(rw_piot_dev));
  if (NULL == rw_piot_dev) {
    RW_PIOT_LOG(RTE_LOG_ERR, "PIOT Could not find device by handle\n");
    return -1;
  }
  ASSERT(rx_offload_capa);
  ASSERT(tx_offload_capa);
  memset(&dev_info, 0, sizeof(dev_info));
  rte_eth_dev_info_get(rw_piot_dev->rte_port_id, &dev_info);
  *rx_offload_capa = dev_info.rx_offload_capa;
  *tx_offload_capa = dev_info.tx_offload_capa;
  return 0;
}


void
rw_piot_service_link_interrupts(void)
{
  eal_intr_thread_main(NULL);
}

/* set affinity for current thread */
int
rw_piot_thread_set_affinity(void)
{
        int s;
        pthread_t thread;

/*
 * According to the section VERSIONS of the CPU_ALLOC man page:
 *
 * The CPU_ZERO(), CPU_SET(), CPU_CLR(), and CPU_ISSET() macros were added
 * in glibc 2.3.3.
 *
 * CPU_COUNT() first appeared in glibc 2.6.
 *
 * CPU_AND(),     CPU_OR(),     CPU_XOR(),    CPU_EQUAL(),    CPU_ALLOC(),
 * CPU_ALLOC_SIZE(), CPU_FREE(), CPU_ZERO_S(),  CPU_SET_S(),  CPU_CLR_S(),
 * CPU_ISSET_S(),  CPU_AND_S(), CPU_OR_S(), CPU_XOR_S(), and CPU_EQUAL_S()
 * first appeared in glibc 2.7.
 */
#if defined(CPU_ALLOC)
        size_t size;
        cpu_set_t *cpusetp;

        cpusetp = CPU_ALLOC(RTE_MAX_LCORE);
        if (cpusetp == NULL) {
                RTE_LOG(ERR, EAL, "CPU_ALLOC failed\n");
                return -1;
        }

        size = CPU_ALLOC_SIZE(RTE_MAX_LCORE);
        CPU_ZERO_S(size, cpusetp);
        CPU_SET_S(rte_lcore_id(), size, cpusetp);

        thread = pthread_self();
        s = pthread_setaffinity_np(thread, size, cpusetp);
        if (s != 0) {
                RTE_LOG(ERR, EAL, "pthread_setaffinity_np failed\n");
                CPU_FREE(cpusetp);
                return -1;
        }

        CPU_FREE(cpusetp);
#else /* CPU_ALLOC */
        cpu_set_t cpuset;

        CPU_ZERO( &cpuset );
        CPU_SET( rte_lcore_id(), &cpuset );

        thread = pthread_self();
        s = pthread_setaffinity_np(thread, sizeof( cpuset ), &cpuset);
        if (s != 0) {
                RTE_LOG(ERR, EAL, "pthread_setaffinity_np failed\n");
                return -1;
        }
#endif
        return 0;
}



void 
rw_piot_exit_cleanup(void)
{
   eal_exit_cleanup();
}
