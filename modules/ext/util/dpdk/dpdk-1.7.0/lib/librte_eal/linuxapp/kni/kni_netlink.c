#include <linux/kernel.h>
#include <linux/module.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <exec-env/rte_kni_common.h>
#include "kni_dev.h"
#include <rte_config.h>
#include "kni_netlink.h"



static DEFINE_MUTEX(kni_nl_mutex);

int
kni_nl_unicast(int pid, struct sk_buff *skb_in,
               struct net_device *dev)
{
  struct sk_buff *skb;
  struct nlmsghdr *nlh;
  struct net *net = dev_net(dev);
  struct kni_net_namespace *kni_net = net_generic(net, kni_net_id);
  struct rw_kni_mbuf_metadata *meta_data;
  int size, err;
  unsigned char *data;
  
  size = NLMSG_ALIGN(sizeof(*meta_data) + skb_in->len);
  
  skb = nlmsg_new(size, GFP_KERNEL);
  if (skb == NULL){
    return -ENOMEM;
  }
  nlh = nlmsg_put(skb, pid, 0, KNI_NETLINK_MSG_TX, 0, 0);
  if (nlh == NULL){
    goto nlmsg_failure;
  }
  err = skb_linearize(skb_in);
  if (unlikely(err)){
    goto nlmsg_failure;
  }
  meta_data = (struct rw_kni_mbuf_metadata *)nlmsg_data(nlh);;
  memset(meta_data, 0, sizeof(*meta_data));
  data = (unsigned char *)(meta_data +1);
  RW_KNI_VF_SET_MDATA_PAYLOAD(meta_data,
                              skb_in->protocol);
  RW_KNI_VF_SET_MDATA_LPORTID(meta_data,
                              dev->ifindex);
  RW_KNI_VF_SET_MDATA_PARSED_LEN(meta_data,
                                 skb_in->len);
  memcpy(data, skb_in->data, skb_in->len); //akki
  skb_put(skb, size);
  
  nlmsg_end(skb, nlh);
  
  return nlmsg_unicast(kni_net->netlink_sock, skb, pid);
  
nlmsg_failure:

   nlmsg_cancel(skb, nlh);
   kfree_skb(skb);
   return -1;
}


static int
kni_nl_process(struct sk_buff *skb,
               struct nlmsghdr *nlh)
{
  struct net *net = sock_net(skb->sk);
  struct rw_kni_mbuf_metadata *meta_data;
  uint32_t ifindex;
  struct net_device *dev;
  struct kni_dev *kni;
  struct sk_buff *skb_out;
  int pkt_len;
  
  meta_data = (struct rw_kni_mbuf_metadata *)nlmsg_data(nlh);
  ifindex = RW_KNI_VF_GET_MDATA_LPORTID(meta_data);
  pkt_len = RW_KNI_VF_GET_MDATA_PARSED_LEN(meta_data);
  //akki no rtnl lock here. have to take it??
  dev =dev_get_by_index(net, ifindex);
  if (!dev){
    if (net_ratelimit()){
      printk(KERN_INFO "No device found when reveiveing netlink packet in kni %d\n",
             ifindex);
    }
    return 0;
  }
  kni  = netdev_priv(dev);

  skb_out = dev_alloc_skb(pkt_len + sizeof(*meta_data) + 2);
  if (!skb_out) {
    /* Update statistics */
    kni->stats.rx_dropped++;
    return 0;
  }
  /* Align IP on 16B boundary */
  skb_reserve(skb_out, 2);
  memcpy(skb_put(skb_out, pkt_len+sizeof(*meta_data)),
         meta_data, (pkt_len + sizeof(*meta_data)));
  skb_queue_tail(&kni->skb_rx_queue,
                 skb_out);
  kni->nl_rx_queued++;
  
  if (dev){
    dev_put(dev);
  }
  return 0;
}

void
kni_nl_callback(struct sk_buff *skb)
{
  mutex_lock(&kni_nl_mutex);
  netlink_rcv_skb(skb, &kni_nl_process);
  mutex_unlock(&kni_nl_mutex);
}

int 
kni_netlink_net_init(struct net *net)
{
  struct sock *sk;
  struct netlink_kernel_cfg cfg = {
    .groups		= KNI_NETLINK_MSG_TX,
    .input		= kni_nl_callback,
    .cb_mutex	= &kni_nl_mutex, //akki this can be per net..
    .flags		= NL_CFG_F_NONROOT_RECV,
  };

  struct kni_net_namespace *kni_net = 
      net_generic(net, kni_net_id);

  sk = netlink_kernel_create(net,
                             KNI_NETLINK_PROTOCOL, &cfg);
  if (!sk){
    return -ENOMEM;
  }

  kni_net->netlink_sock = sk;

  return 0;
}

void kni_netlink_net_exit(struct net *net)
{
  struct kni_net_namespace *kni_net = 
      net_generic(net, kni_net_id);
  if (kni_net->netlink_sock){
    netlink_kernel_release(kni_net->netlink_sock);
    kni_net->netlink_sock = NULL;
  }
}


void kni_net_rx_netlink(struct kni_dev *kni)
{
  struct sk_buff *skb;
  struct rw_kni_mbuf_metadata *meta_data;

  while ((skb = skb_dequeue(&kni->skb_rx_queue)) != NULL) {
    kni->nl_rx_dequeued++;
    meta_data =  (struct rw_kni_mbuf_metadata *)skb->data;
    skb->dev = kni->net_dev;
    __skb_pull(skb, sizeof(struct rw_kni_mbuf_metadata));
    kni_net_process_rx_packet(skb, kni->net_dev, meta_data);
    //kni->stats.rx_bytes += pkt_len;
  }
  return;
}


void kni_net_tx_netlink(struct kni_dev *kni)
{
  struct sk_buff *skb;
  int err;
  
  while ((skb = skb_dequeue(&kni->skb_tx_queue)) != NULL) {
    kni->nl_tx_dequeued++;
    err = kni_nl_unicast(kni->nl_pid, skb, kni->net_dev);
    if (err){
      kni->stats.tx_dropped++;
    }else{
      kni->stats.tx_packets++;
    }
    dev_kfree_skb(skb);
  }
  return;
}


