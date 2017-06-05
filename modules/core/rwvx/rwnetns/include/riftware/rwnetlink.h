/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_netlink.h
 * @author Srinivas Akkipeddi (srinivas.akkipeddi@riftio.com)
 * @date 03/20/2014
 * @brief Netlink library to be used by applications to receive/send netlink/ioctls
 *
 * @details 
 *
 * Applications should use this library to send/receive netlink messages. Applications
 * can use the same library to do some network ioctls like adding/updating/deleting
 * interfaces/routes/arps/mpls. The following example shows an example of how netlink
 * library can be used.
 * For now, we will be using the netlink library to add/receive link/address/route/arp
 * If for some reason, we want to include other features such as
 *  1. table sync for routes/inferfaces..
 *  2. ACK mechanism for the sending netlink messages
 * we might want to consider dynamically linking the libnl library instead.
 *
 * @code
 * include "rw_netlink.h"
 *
 * rw_status_t status;
 * rw_netlink_handle_t *handle = rw_netlink_handle_init(user_data, arp_add_callback_func, arp_del_callback_func, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL. NULL;
 *
 * status = rwtrace_ctx_category_destination_set(ctx,
 *                                           RWTRACE_CATEGORY_SCHED,
 *                                           RWTRACE_DESTINATION_CONSOLE);
 *
 * ASSERT_EQ(status, RW_STATUS_SUCCESS);
 *
 * ASSERT_TRUE(ConsoleCapture::start_stdout_capture() >= 0);
 * RWTRACE_DEBUG(ctx, RWTRACE_CATEGORY_SCHED, "Testing RWTRACE_DEBUG");
 * @endcode
 */

#ifndef __RW_NETLINK_H__
#define __RW_NETLINK_H__

__BEGIN_DECLS

#define RWNETLINK_OWN
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/xfrm.h>
#include <rwnetns.h>

#define NDA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#define RW_NETLINK_IFNAMSIZ 20
#define RW_NETLINK_SOCKET_BUFFER_SIZE 65536
#define RW_NETLINK_BUFFER_SIZE        16384
#define RW_NETLINK_MAX_CALLBACKS      32

typedef enum rw_netlink_socket_type{
  RW_NETLINK_CONNECT = 1,
  RW_NETLINK_LISTEN
}rw_netlink_socket_type_t;

typedef enum rw_netlink_arp_state{
  RW_ARP_FAILED = 1,
  RW_ARP_RESOLVED
}rw_arp_state_t;


typedef struct {
  rw_ip_prefix_t          dest;
  rw_ip_prefix_t          src;
  int16_t                 dport;
  int16_t                 dport_mask;
  int16_t                 sport;
  int16_t                 sport_mask;
  uint32_t                ifindex;
  uint32_t                kernelid;
  uint8_t                 protocol;
}rw_netlink_xfrm_selector_info_t;


typedef struct {
  rw_ip_addr_t            dest;
  rw_ip_addr_t            src;
  uint32_t                spi;
  uint32_t                reqid;
  uint8_t                 mode;
  uint8_t                 share;
  uint8_t                 optional;
  uint8_t                 aalgos;
  uint8_t                 ealgos;
  uint8_t                 calgos;
  uint8_t                 proto;
}rw_netlink_xfrm_template_info_t;

typedef enum{
  RW_XFRM_ALGO_TYPE_CRYPT,
  RW_XFRM_ALGO_TYPE_AUTH,
  RW_XFRM_ALGO_TYPE_COMP,
  RW_XFRM_ALGO_TYPE_AEAD,
  RW_XFRM_ALGO_TYPE_TRUNC,
  RW_XFRM_ALGO_TYPE_MAX
}rw_xfrm_algo_type_t;

typedef struct {
  rw_xfrm_algo_type_t alg_type;
  char		      alg_name[64];
  unsigned int	      alg_key_len;	/* in bits */
  union{
    unsigned int	      alg_icv_len;	/* in bits */
    unsigned int	      alg_trunc_len;	/* in bits */
  }trunc_icv_len;
  char		      alg_key[0];
}rw_netlink_xfrm_auth_info_t;

//AKKI same as the yang.. Need to use the yang here..
typedef enum rw_netlink_tunnel_type{
  RW_NETLINK_TUNNEL_TYPE_GRE = 1,
  RW_NETLINK_TUNNEL_TYPE_SIT = 2,
  RW_NETLINK_TUNNEL_TYPE_IPIP = 3
}rw_netlink_tunnel_type_t;

typedef struct rw_netlink_tunnel_params{
  char                   tunnelname[RW_NETLINK_IFNAMSIZ];
  rw_netlink_tunnel_type_t type;
  rw_ip_addr_t           local;
  rw_ip_addr_t           remote;
  char                   namespace_name[RW_MAX_NETNS_NAME];
  uint32_t               phy_ifindex;
  //gre below
  uint16_t               iflags;
  uint16_t               oflags;
  uint32_t               key;
  uint32_t               okey;
  uint32_t               ttl;
  uint32_t               tos;
  //uint32_t               pmtudisc;
}rw_netlink_tunnel_params_t;


/**
 * This type is sent as a argument to the registered callback for XFRM SA add/deletes
 */
typedef struct {
  rw_ip_addr_t           dest;
  rw_ip_addr_t           src;
  int32_t                spi;
  uint32_t               seq;
  uint32_t               reqid;
  uint8_t                mode;
  uint8_t                protocol;
  uint8_t                replay_window;
  uint8_t                flags;
  rw_netlink_xfrm_selector_info_t *selector_info;
  rw_netlink_xfrm_template_info_t *template_info;
  rw_netlink_xfrm_auth_info_t     *auth_info[RW_XFRM_ALGO_TYPE_MAX];
  int                             num_auth;
}rw_netlink_xfrm_sa_info_t;



/**
 * This type is sent as a argument to the registered callback for XFRM Policy add/deletes
 */
typedef struct {
  uint32_t                    index;
  uint32_t                    priority;
  uint8_t                     dir;
  uint8_t                     action;
  uint8_t                     flags;
  uint8_t                     share;
  uint8_t                     type;
  rw_netlink_xfrm_selector_info_t      *selector_info;
  rw_netlink_xfrm_auth_info_t          *auth_info[RW_XFRM_ALGO_TYPE_MAX];
  int                             num_auth;
}rw_netlink_xfrm_policy_info_t;


/**
 * This type is sent as a argument to the registered callback for neigh add/deletes
 */
typedef struct {
  rw_arp_state_t arp_state; /**< Arp state failed/resolved*/
  rw_ip_addr_t   address; /**< address. will be changed to the rw type for ip-address*/
  rw_mac_addr_t  mac; /**< mac address. Will be changed to the rw type for mac-addr*/
  uint32_t       ifindex; /**< ifindex of the logical interface in the kernel*/
  uint32_t       nud_state; /**< The actual arp state in the kernel for parties interested*/
}rw_netlink_neigh_entry_t;

/**
 * This type is sent as a argument to the registered callback for link add/delete
 */
typedef struct {
  uint32_t      ifindex; /**< Ifindex of the link that is added/deleted*/
  char          *ifname;
  uint32_t      flags; /**< Interface state flags such as IFF_UP, IFF_MULTICAST..*/
  uint16_t      type; /**< Interface type*/
  uint32_t      mtu;
  rw_mac_addr_t mac;
}rw_netlink_link_entry_t;


/**
 * This type is sent as a argument to the registered callback for link address add/delete
 */
typedef struct {
  uint32_t      ifindex; /**< Ifindex of the link that is added/deleted*/
  rw_ip_addr_t  address; /**< address. will be changed to the rw type for ip-address*/
  unsigned char mask; /**< masklen. will be changed with the above member to represent a prefix*/
  rw_ip_addr_t  remote_address;
}rw_netlink_link_address_entry_t;


/**
 * This type is sent as a argument to the registered callback for route add/delete
 */
typedef struct{
  rw_ip_addr_t    gateway; /**<gateway address for the nexthop*/
  uint32_t        ifindex; /**< ifindex of the nexthop*/
  char            ifname[RW_NETLINK_IFNAMSIZ];
}rw_netlink_route_nh_entry_t;

/**
 * This type is sent as a argument to the registered callback for route add/delete
 */
typedef struct {
  rw_ip_prefix_t               prefix;
  uint8_t                      replace:1;/**< flags indicating whether it is replace*/
  uint8_t                      protocol; /**<protocol of the route*/
  uint8_t                      num_nexthops; /**< num of nexthops*/
  rw_netlink_route_nh_entry_t  nexthops[]; /** nexthops*/
}rw_netlink_route_entry_t;



struct rw_netlink_handle;

typedef rw_status_t (*rw_netlink_cb_t)(void *userdata,
                                       void *arg);
/**
 * This type is to represent a netlink socket for a particular netlink protocol.
 * It is a static member in the netlink handle for every netlink protocol.
 */
typedef struct {
  struct rw_netlink_handle *handle; /**< Back pointer to the netlink handle */
  void                     *sk; /**< pointer to the socket structure in the libnl*/
  uint32_t                  groups; /** < groups registered to this netlink socket*/
  int                       proto; /**< Protocol that this handle represents*/
  uint32_t                  send_seq; /**< Send sequence number*/

  rw_netlink_cb_t           cb[RW_NETLINK_MAX_CALLBACKS]; /**< list of callbacks for different commands*/
#ifdef RWNETLINK_OWN
  char                            *read_buffer;
#endif
  char                            *head_write_buffer;/** < Send buffer*/
  char                            *write_buffer; /** < Send buffer. if bundling next message should start from here*/
  
  unsigned char                   bundle;  /**< Boolean to indicate if bundling is turned on/off*/
  int                             total_send_buf_size; /**< Total size of the write buffer space*/
  
} rw_netlink_handle_proto_t;


typedef int (*netlink_filter_f)(rw_netlink_handle_proto_t *,
                              struct nlmsghdr *, void *);

/**
 * This type is the handle returned to the application when it initializes netlink
 * library
 */
typedef struct rw_netlink_handle {
  char                               namespace_name[RW_MAX_NETNS_NAME]; /** Name of the network namespace*/
  uint32_t                           namespace_id; /**<Kernel Namespace id */
  void                               *user_data; /**< Back pointer provided by the user*/
  uint32_t                           local_id;
  rw_netlink_handle_proto_t          *proto_netlink[MAX_LINKS]; /**<Different protocol families in netlink*/
  
  int                          ioctl_sock_fd;
  int                          ioctl6_sock_fd;
  int                          netns_fd;
  rw_netlink_cb_t              add_netns_cb;
  rw_netlink_cb_t              del_netns_cb;
}rw_netlink_handle_t;

/**
 * This function opens the necessary netlink sockets depending on the callbacks registered
 * The groups assigned to each netlink socket also depends on the callbacks registered.
 * The function returns a handle. One member of the handle is the "user_data" which could
 * a back pointer to the application datatype that is calling it.
 * @param[in]  user_data  - userdata referenced by the netlink handle that is returned.
 * @returns netlink_handle  if success
 * @returns NULL if failure
 */
rw_netlink_handle_t* rw_netlink_handle_init(void *userdata, char *name,
                                            uint32_t namespace_id);

/*The id will be used to set the local port for netlink socket instead of
  get_pid*/
void rw_netlink_handle_set_local_id(rw_netlink_handle_t *handle,
                                    uint32_t id);

/**
 * This function is called to add an address and mask to an interface.
 * @param[in]  name - interface name to add address
 * @param[in]  ifindex - interface ifindex to add address
 * @param[in]  address - address to add
 * @param[in]  name - masklen to add
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_address_add(rw_netlink_handle_t *handle,
                                           const char *name, uint32_t ifindex,
                                           rw_ip_prefix_t *prefix);

/**
 * This function is called to add an remote address to the link
 * @param[in]  name - interface name to add address
 * @param[in]  ifindex - interface ifindex to add address
 * @param[in]  address - address to add
 * @param[in]  name - masklen to add
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_remote_address_add(rw_netlink_handle_t *handle,
                                                  const char *name,
                                                  uint32_t ifindex,
                                                  rw_ip_addr_t *addr);

/**
 * This function is called when there is some data to be read in the netlink socket.
 * @param[in]  netlink_proto_handle  - netlink protocol handle which has socket
 * information to read
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_read(rw_netlink_handle_proto_t *proto_handle);

/**
 * This function is called to bring an interface up given an ifname and ifindex.
 *
 * @param[in]  name - interface name to bring up
 * @param[in]  ifindex - interface ifindex to bring up
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
uint32_t rw_ioctl_interface_up(rw_netlink_handle_t *handle,
                               const char *name, uint32_t ifindex);
/**
 * This function is called to bring an interface down given an ifname and ifindex.
 *
 * @param[in]  name - interface name to bring up
 * @param[in]  ifindex - interface ifindex to bring up
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_ioctl_interface_down(rw_netlink_handle_t *handle,
                                    const char *name, uint32_t ifindex);
/**
 * This function is called to get the ifindex of an interface given the name
 * @param[in] handle = netlink handle
 * @param[in] name - interface name
 * @return ifindex - value > 0
 * @return 0 if failed
 */
uint32_t rw_ioctl_get_ifindex(rw_netlink_handle_t *handle,
                              const char *name);

int
rw_ioctl_get_ifname(rw_netlink_handle_t *handle,
                    uint32_t ifindex, char *name, int size);

/**
 * This function is called to delete a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_del_route(rw_netlink_handle_t *handle,
                                 rw_netlink_route_entry_t *route);
/**
 * This function is called to add a ip route to the kernel using netlink
 * @param[in]  handle - netlink handle
 * @param[in]  route  - route to be added/deleted
 * @return RW_STATUS_SUCCESS
 * @return RW_STATUS_FAILURE
 */
rw_status_t rw_netlink_add_route(rw_netlink_handle_t *handle,
                                 rw_netlink_route_entry_t *route);


rw_status_t rw_netlink_add_neigh(rw_netlink_handle_t *handle,
                                 rw_netlink_neigh_entry_t *neigh);
rw_status_t rw_netlink_del_neigh(rw_netlink_handle_t *handle,
                                 rw_netlink_neigh_entry_t *neigh);

/**
 * This function is called to delete an local-address and mask to an
 * interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_interface_address_delete(rw_netlink_handle_t *handle,
                                                rw_netlink_link_address_entry_t *addr_entry);

/**
 * This function is called to delete an remote-address and mask to an
 * interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t
rw_netlink_interface_remote_address_delete(rw_netlink_handle_t *handle,
                                        rw_netlink_link_address_entry_t *addr_entry);

/**
 * This function is called to add an remote-address and mask to an
 * interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t
rw_netlink_interface_remote_address_add(rw_netlink_handle_t *handle,
                                        rw_netlink_link_address_entry_t *addr_entry);


unsigned char rw_netlink_ip_masklen (uint32_t address);
/**
 * This function closes the netlink sockets that are opened. The handle is also freed.
 * @param[in]  netlink_handle  - handle to free
 */
void rw_netlink_terminate( rw_netlink_handle_t *handle);

/**
 * This function is called to add an local-address and mask to an
 * interface.
 * @param[in]  handle - netlink handle
 * @param[in]  address-entry
 * @returns RW_STATUS_SUCCESS  if success
 * @returns RW_STATUS_FAILURE if error
 */
rw_status_t rw_netlink_interface_address_add(rw_netlink_handle_t *handle,
                                             rw_netlink_link_address_entry_t *addr_entry);

/**
 * This function is called to get the socket fd of the netlink socket that was opened for
 * this protocol
 * @param[in] handle - netlink handle
 * @param[in] proto  - protocol
 * @return > 0 if fd is present
 * @return 0 if no fd
 */
int
rw_netlink_get_sock_fd(rw_netlink_handle_t *handle, int proto);

/**
 * This function is called to close a netlink socket for a particular netlink protocol
 * @param[in]  proto  - netlink protocol with which to open the AF_NETLINK socket.
 * @param[in ] handle - netlink handle
 */
void rw_netlink_handle_proto_terminate(rw_netlink_handle_proto_t *proto_handle);

/**
 * This function initializes a netlink socket for a particular netlink protocol. For now,
 * this function is getting called for NETLINK_ROUTE. When we support MPLS, we can call
 * same fucntion for NETLINK_GENERIC. This function currently does not schedule a read
 * event. For now, the application must do the poll and check for reads.
 * @param[in]  proto  - netlink protocol with which to open the AF_NETLINK socket.
 * @param[in]  groups - groups to be registered with the socket when binding.
 * @param[in ] handle - netlink handle
 * @returns handle for the protoocol(NETLINK_ROUTE)  if success
 * @returns NULL
 */
rw_netlink_handle_proto_t* rw_netlink_handle_proto_init(rw_netlink_handle_t *handle,
                                                        uint32_t proto);

/**
 * This function is called to register callbacks for given netlink protocol and
 * netlink command
 * @param[in] - handle - netlink handle
 * @param[in] - proto  - netlink protocol
 * @param[in] - cmd    - netlink command
 * @param[in] - cb     - callback
 */
void rw_netlink_register_callback(rw_netlink_handle_t *handle, uint32_t proto,
                                  int cmd, rw_netlink_cb_t cb);

/**
 * This fucntion is called to start bundling of messages on a particular netlink
 * socket
 */
void rw_netlink_start_bundle(rw_netlink_handle_t *handle, uint32_t proto);

/**
 * This fucntion is called to start bundling of messages on a particular netlink
 * socket
 */
void rw_netlink_stop_bundle(rw_netlink_handle_t *handle, uint32_t proto);

rw_status_t rw_ioctl_set_mac(rw_netlink_handle_t *handle,
                             const char *name, uint32_t ifindex,
                             rw_mac_addr_t *mac);
rw_status_t rw_ioctl_set_mtu(rw_netlink_handle_t *handle,
                             const char *name, uint32_t ifindex,
                             uint32_t mtu);


int
rw_ioctl_create_tap_device(rw_netlink_handle_t *handle,
                           const char *name);
rw_status_t
rw_netlink_create_tunnel_device(rw_netlink_handle_t *handle,
                                rw_netlink_tunnel_params_t *tparam);
rw_status_t
rw_netlink_change_tunnel_device(rw_netlink_handle_t *handle,
                                rw_netlink_tunnel_params_t *tparam);
rw_status_t
rw_netlink_delete_tunnel_device(rw_netlink_handle_t *handle,
                                rw_netlink_tunnel_params_t *tparam);

rw_status_t
rw_ioctl_create_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam);
rw_status_t
rw_ioctl_change_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam);
rw_status_t
rw_ioctl_delete_tunnel_device(rw_netlink_handle_t *handle,
                              rw_netlink_tunnel_params_t *tparam);

int
rw_ioctl_create_tun_device(rw_netlink_handle_t *handle,
                           const char *name);

int rw_netlink_create_veth_pair(rw_netlink_handle_t *handle,
                                const char *name, const char *peername,
                                char *peer_namespace);

rw_status_t rw_netlink_get_intf_info_from_ip(rw_netlink_handle_t *handle,
                                             rw_ip_addr_t *ip,
                                             rw_netlink_link_entry_t *info);
rw_status_t rw_ioctl_add_v4_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route);
rw_status_t rw_ioctl_del_v4_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route);

rw_status_t rw_ioctl_add_v6_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route);
rw_status_t rw_ioctl_del_v6_route(rw_netlink_handle_t *handle,
                                  rw_netlink_route_entry_t *route);
rw_status_t
rw_netlink_netns_read(rw_netlink_handle_t *handle);
int
rw_netlink_register_netns_callback(rw_netlink_handle_t *handle,
                                   rw_netlink_cb_t add_cb,
                                   rw_netlink_cb_t del_cb);
#ifdef RWNETLINK_OWN
rw_status_t rw_netlink_read_ow(rw_netlink_handle_proto_t *proto_handle,
                               int end_on_done, void *arg,
                               int (*filter)(rw_netlink_handle_proto_t *,
                                             struct nlmsghdr *, void *));
#endif



rw_status_t
rw_netlink_get_link(rw_netlink_handle_t *handle);
rw_status_t
rw_netlink_get_link_addr(rw_netlink_handle_t *handle);

__END_DECLS

#endif /* __RW_NETLINK_H__ */

