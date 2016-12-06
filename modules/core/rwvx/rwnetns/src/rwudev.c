
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
 * @file rw_netlink.c
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @Fastpath netlink functionality
 *
 * @details
 */
#include <stdio.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <rwlib.h>
#include <rw_ip.h>
#include <rwudev.h>
#include <sys/inotify.h>
#include "netlink/netlink.h"
#include "netlink/msg.h"
#include "netlink/cache.h"
#include "netlink/route/link.h"
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_tunnel.h>
#include <linux/route.h>
#include <linux/ipv6_route.h>
#include <linux/veth.h>
#include <linux/if_arp.h>
#include <linux/if_tun.h>
#include <linux/ipv6.h>
#include <linux/xfrm.h>
#include <libudev.h>

int
rw_pci_address_cmp(void *a1, void *a2)
{
  rw_pci_address_t *v1 = (rw_pci_address_t *)a1;
  rw_pci_address_t *v2 = (rw_pci_address_t *)a2;

  if (v1->domain > v2->domain){
    return 1;
  }

  if (v1->domain < v2->domain){
    return -1;
  }
  
  if (v1->bus > v2->bus){
    return 1;
  }

  if (v1->bus < v2->bus){
    return -1;
  }
  
  if (v1->devid > v2->devid){
    return 1;
  }

  if (v1->devid < v2->devid){
    return -1;
  }

  if (v1->function > v2->function){
    return 1;
  }

  if (v1->function < v2->function){
    return -1;
  }
  
  return 0;
}


/*********************** TAKEN FROM dpdk code base************************************/
int
rw_strsplit(char *string, int stringlen,
	     char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if (string == NULL || tokens == NULL)
          goto einval_error;

	for (i = 0; i < stringlen; i++) {
          if (string[i] == '\0' || tok >= maxtokens)
            break;
          if (tokstart) {
            tokstart = 0;
            tokens[tok++] = &string[i];
          }
          if (string[i] == delim) {
            string[i] = '\0';
            tokstart = 1;
          }
	}
	return tok;
        
einval_error:
	return -1;
}

int
rw_parse_pci_addr_format(char *buf, int bufsize,
                         rw_pci_address_t *pci_addr)
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
  
  if (rw_strsplit(buf_copy, bufsize, splitaddr.str, PCI_FMT_NVAL, ':')
      != PCI_FMT_NVAL - 1)
    goto error;
  /* final split is on '.' between devid and function */
  splitaddr.function = strchr(splitaddr.devid,'.');
  if (splitaddr.function == NULL)
    goto error;
  *splitaddr.function++ = '\0';
  
  /* now convert to int values */
  strncpy(pci_addr->pci_name, buf, sizeof(pci_addr->pci_name));
  pci_addr->domain = (uint16_t)strtoul(splitaddr.domain, NULL, 16);
  pci_addr->bus = (uint8_t)strtoul(splitaddr.bus, NULL, 16);
  pci_addr->devid = (uint8_t)strtoul(splitaddr.devid, NULL, 16);
  pci_addr->function = (uint8_t)strtoul(splitaddr.function, NULL, 10);

  free(buf_copy); /* free the copy made with strdup */
  return 0;
error:
  free(buf_copy);
  return -1;
}

/*********************** TAKEN FROM dpdk code base************************************/
int
rw_sys_populate_pci_from_path(const char *path, rw_pci_address_t *pci_addr)
{
  char copy[256];
  char *tokens[12];
  int num_tokens;
  int i;
  int ret = -1;
  
  snprintf(copy, sizeof(copy), "%s", path);
  num_tokens = rw_strsplit(copy, sizeof(copy), tokens, 12, '/');
  for (i =0; i < num_tokens; i++){
    ret = rw_parse_pci_addr_format(tokens[i], strlen(tokens[i]), pci_addr);
    if (ret == 0)
      break;
  }

  return ret;
}


int
rw_netlink_get_pci_addr(const char *ifname, rw_pci_address_t *pci_addr)
{
  struct udev *udev = NULL;
  struct udev_enumerate *enumerate = NULL;
  struct udev_list_entry *devices, *dev_list_entry;
  int ret = -1;
  
  udev = udev_new();
  if (!udev) {
    ret = -1;
    goto ret;
  }

  enumerate = udev_enumerate_new(udev);
  udev_enumerate_add_match_subsystem(enumerate, "net");
  udev_enumerate_add_match_sysname(enumerate, ifname);
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *path;
    
    path = udev_list_entry_get_name(dev_list_entry);
    ret = rw_sys_populate_pci_from_path(path, pci_addr);
    if (!ret){
      break;
    }
  }

ret:
  if (enumerate){
    udev_enumerate_unref(enumerate);
  }
  if (udev){
    udev_unref(udev);
  }
  return ret;
}

rw_pci_device_t*
rw_udev_lookup_device(rw_udev_handle_t *handle,
                      rw_pci_address_t *pci)
{
  rw_status_t status;
  rw_pci_device_t *dev;

  status = RW_SKLIST_LOOKUP_BY_KEY(&(handle->dev_list), pci,
                                   (void *)&dev);
  if (status != RW_STATUS_SUCCESS){
    return NULL;
  }
  return dev;
}



static rw_status_t
rw_udev_delete_device(rw_udev_handle_t *handle,
                      rw_pci_address_t *pci)
{
  rw_pci_device_t *dev;
  rw_status_t status;
  
  dev = rw_udev_lookup_device(handle, pci);
  if (!dev){
    return RW_STATUS_FAILURE;
  }
  status = RW_SKLIST_REMOVE_BY_KEY(&(handle->dev_list), &dev->pci,
                                   (void *)&dev);
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status == RW_STATUS_SUCCESS){
    RW_FREE(dev);
  }
  return RW_STATUS_SUCCESS;
}


static rw_pci_device_t*
rw_udev_insert_device(rw_udev_handle_t *handle,
                      rw_pci_address_t *pci)
{
  rw_status_t status;
  rw_pci_device_t *dev;

  dev = rw_udev_lookup_device(handle, pci);
  if (dev){
    return dev;
  }

  dev = RW_MALLOC0(sizeof(*dev));
  dev->pci = *pci; //struct copy
  status =  RW_SKLIST_INSERT(&(handle->dev_list), dev);
  if (status != RW_STATUS_SUCCESS){
    RW_FREE(dev);
    return NULL;
  }

  return dev;
}


static void
rw_udev_update_device(struct udev_device *udev,
                      rw_pci_device_t *dev)
{
  return;
}

rw_status_t
rw_udev_handle_terminate(rw_udev_handle_t *handle)
{
  rw_pci_device_t *dev;

  if (handle->udev){
    udev_unref(handle->udev);
    handle->udev = NULL;
  }
  if (handle->mon){
    
    handle->mon = NULL;
  }
  //LOOP through the skiplist and delete the elements
  dev =  RW_SKLIST_HEAD(&(handle->dev_list), rw_pci_device_t);
  while(dev){
    rw_udev_delete_device(handle, &dev->pci);
    dev =  RW_SKLIST_HEAD(&(handle->dev_list), rw_pci_device_t);
  }
  
  RW_FREE(handle);
  
  return RW_STATUS_SUCCESS;
}


rw_status_t
rw_udev_register_cb(rw_udev_handle_t *handle,
                    void *userdata, rw_udev_cb_t cb)
{
  struct udev_enumerate *enumerate = NULL;
  struct udev_list_entry *devices, *dev_list_entry;
  int ret = -1;
  rw_status_t status = RW_STATUS_SUCCESS;
  rw_pci_address_t pci_addr;
  rw_pci_device_t *dev;
  struct udev_device *udevice;
  
  handle->cb = cb;
  handle->userdata = userdata;
  
  //walk throight he list and call the callback..
  enumerate = udev_enumerate_new(handle->udev);
  if (!enumerate){
    goto free_and_ret;
  }
  udev_enumerate_add_match_subsystem(enumerate, "net");
  udev_enumerate_scan_devices(enumerate);
  devices = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *path;
    path = udev_list_entry_get_name(dev_list_entry);
    ret = rw_sys_populate_pci_from_path(path, &pci_addr);
    if (!ret){
      
    }else{
      udevice = udev_device_new_from_syspath(handle->udev, path);
      dev = rw_udev_insert_device(handle, &pci_addr);
      rw_udev_update_device(udevice, dev);
      if (dev){
        handle->cb(handle->userdata, dev, RW_UDEV_DEVICE_ADD);
      }
      udev_device_unref(udevice);
    }
  }
  
  handle->mon = udev_monitor_new_from_netlink(handle->udev, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(handle->mon, "net", NULL);
  udev_monitor_enable_receiving(handle->mon);
  /* Get the file descriptor (fd) for the monitor.
     This fd will get passed to select() */
  handle->fd = udev_monitor_get_fd(handle->mon);

  
ret:
  return status;
  
free_and_ret:
  if (enumerate){
    udev_enumerate_unref(enumerate);
    enumerate = NULL;
  }
  status = RW_STATUS_FAILURE;
  goto ret;
}

rw_status_t
rw_udev_recv_device(rw_udev_handle_t *handle)
{
  struct udev_device *dev;
  
  dev = udev_monitor_receive_device(handle->mon);
  if (dev) {
    printf("Got Device\n");
    printf("   Node: %s\n", udev_device_get_devnode(dev));
    printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
    printf("   Devtype: %s\n", udev_device_get_devtype(dev));
    
    printf("   Action: %s\n", udev_device_get_action(dev));
    udev_device_unref(dev);
  }
  
  return RW_STATUS_SUCCESS;
}

rw_udev_handle_t*
rw_udev_handle_init(void *userdata, rw_udev_cb_t cb)
{
  rw_udev_handle_t *handle;
  
  handle = RW_MALLOC0(sizeof(*handle));
  
  RW_SKLIST_PARAMS_DECL(pci_list1_,
                        rw_pci_device_t,
                        pci,
                        rw_pci_address_cmp,
                        element);
  RW_SKLIST_INIT(&(handle->dev_list),&pci_list1_);
  
  handle->udev = udev_new();
  if (!handle->udev) {
    goto free_and_ret;
  }

  if (cb){
    rw_udev_register_cb(handle, userdata, cb);
  }
ret:
  return handle;
  
free_and_ret:
  if (handle->udev){
    udev_unref(handle->udev);
    handle->udev = NULL;
  }

  RW_FREE(handle);
  handle = NULL;
  goto ret;
}

