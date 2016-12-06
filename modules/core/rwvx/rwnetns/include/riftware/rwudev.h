/**
 * @file rwsystemd.h
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @brief Netlink library to be used by applications to receive/send netlink/ioctls
 *
 * @details 
 */

#ifndef __RW_SYSTEMD_H__
#define __RW_SYSTEMD_H__
#include <rw_sklist.h>

__BEGIN_DECLS

#define RW_MAX_PCI_ADDRESS_NAME 64
typedef struct rw_pci_address{
  char      pci_name[RW_MAX_PCI_ADDRESS_NAME];
  uint16_t domain;
  uint8_t bus;
  uint8_t devid;
  uint8_t function;
}rw_pci_address_t;


typedef struct rw_pci_device{
  rw_pci_address_t pci;
  rw_mac_addr_t    mac;
  uint32_t         change_flags;
  rw_sklist_element_t element;
}rw_pci_device_t;

typedef enum rw_udev_action{
  RW_UDEV_DEVICE_ADD,
  RW_UDEV_DEVICE_DELETE,
  RW_UDEV_DEVICE_MOVE,
  RW_UDEV_DEVICE_CHANGE
}rw_udev_action_t;

typedef enum rw_udev_attribute{
  RW_UDEV_DEVICE_ATTR_ADDRESS = 1,
  RW_UDEV_DEVICE_ATTR_MAX
}rw_udev_attribute_t;

typedef rw_status_t (*rw_udev_cb_t)(void *userdata,
                                    rw_pci_device_t *dev,
                                    rw_udev_action_t action);

typedef struct rw_udev_handle{
  rw_sklist_t         dev_list;
  struct udev         *udev;
  struct udev_monitor *mon;
  int                 fd;
  void                *userdata;
  rw_udev_cb_t        cb;
}rw_udev_handle_t;

int
rw_netlink_get_pci_addr(const char *ifname, rw_pci_address_t *addr);
int
rw_strsplit(char *string, int stringlen,
            char **tokens, int maxtokens, char delim);
int
rw_parse_pci_addr_format(char *buf, int bufsize,
                         rw_pci_address_t *pci_addr);
rw_udev_handle_t*
rw_udev_handle_init(void *userdata, rw_udev_cb_t cb);
rw_status_t
rw_udev_recv_device(rw_udev_handle_t *handle);
rw_status_t
rw_udev_handle_terminate(rw_udev_handle_t *handle);
rw_pci_device_t*
rw_udev_lookup_device(rw_udev_handle_t *handle,
                      rw_pci_address_t *pci);
rw_status_t
rw_udev_register_cb(rw_udev_handle_t *handle,
                    void *userdata, rw_udev_cb_t cb);
__END_DECLS

#endif /* __RW_SYSTEMD_H__ */

