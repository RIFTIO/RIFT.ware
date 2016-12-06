#ifndef __LINUX_NETDEV_FEATURES_WRAPPER_H
#define __LINUX_NETDEV_FEATURES_WRAPPER_H

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,0)
#include_next <linux/netdev_features.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define NETIF_F_HW_VLAN_CTAG_TX NETIF_F_HW_VLAN_TX
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
#define NETIF_F_GSO_ENCAP_ALL	 0

#else

#ifndef NETIF_F_GSO_GRE
#define NETIF_F_GSO_GRE 0
#endif

#ifndef NETIF_F_GSO_GRE_CSUM
#define NETIF_F_GSO_GRE_CSUM 0
#endif

#ifndef NETIF_F_GSO_IPIP
#define NETIF_F_GSO_IPIP 0
#endif

#ifndef NETIF_F_GSO_SIT
#define NETIF_F_GSO_SIT 0
#endif

#ifndef NETIF_F_GSO_UDP_TUNNEL
#define NETIF_F_GSO_UDP_TUNNEL 0
#endif

#ifndef NETIF_F_GSO_UDP_TUNNEL_CSUM
#define NETIF_F_GSO_UDP_TUNNEL_CSUM 0
#endif

#ifndef NETIF_F_GSO_MPLS
#define NETIF_F_GSO_MPLS 0
#endif

#ifndef NETIF_F_GSO_ENCAP_ALL
#define NETIF_F_GSO_ENCAP_ALL	(NETIF_F_GSO_GRE |			\
				 NETIF_F_GSO_GRE_CSUM |			\
				 NETIF_F_GSO_IPIP |			\
				 NETIF_F_GSO_SIT |			\
				 NETIF_F_GSO_UDP_TUNNEL |		\
				 NETIF_F_GSO_UDP_TUNNEL_CSUM |		\
				 NETIF_F_GSO_MPLS)
#endif

#endif

#endif
