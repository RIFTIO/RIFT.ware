#ifndef _KNI_NETLINK_H_
#define _KNI_NETLINK_H_

#ifndef KNI_NETLINK_PROTOCOL
#define KNI_NETLINK_PROTOCOL 31
#endif
extern int kni_net_id __read_mostly;

enum kni_netlink_msg_types {
   KNI_NETLINK_MSG_BASE = NLMSG_MIN_TYPE,
   KNI_NETLINK_MSG_TX = KNI_NETLINK_MSG_BASE,
   KNI_NETLINK_MSG_MAX
};

enum kni_netlink_attr {
  KNI_NETLINK_UNSPEC,
  KNI_NETLINK_METADATA,
  KNI_NETLINK_DATA,
  __KNI_NETLINK_MAX,
};


struct kni_net_namespace{
  struct sock *netlink_sock;
};

#define KNI_NETLINK_MAX (__KNI_NETLINK_MAX - 1)

#define KNI_NETLINK_GRP 1
int 
kni_netlink_net_init(struct net *net);
void kni_netlink_net_exit(struct net *net);
int
kni_nl_unicast(int pid, struct sk_buff *skb,
               struct net_device *dev);

#endif

