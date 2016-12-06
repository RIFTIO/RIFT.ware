/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/kthread.h>
#include <linux/rwsem.h>
#ifdef RTE_LIBRW_PIOT
#include <linux/filter.h>
#include <linux/socket.h>
#include <linux/reciprocal_div.h>
#include <net/sock.h>
#endif
#include <exec-env/rte_kni_common.h>
#include "kni_dev.h"
#include <rte_config.h>

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Kernel Module for managing kni devices");

#define KNI_RX_LOOP_NUM 1000


#ifdef RTE_LIBRW_PIOT
#define KNI_MAX_DEVICES 128
#ifdef RTE_LIBRW_NOHUGE
int kni_net_id __read_mostly;
#endif
static int  kni_proc_init(void);
static void kni_proc_exit(void);
extern void kni_net_ip_init(struct net_device *dev);
extern void kni_net_lb_init(struct net_device *dev);
rx_handler_result_t rw_fpath_kni_handle_frame(struct sk_buff **pskb);
#else
#define KNI_MAX_DEVICES 32
#endif

extern void kni_net_rx(struct kni_dev *kni);
extern void kni_net_init(struct net_device *dev);
extern void kni_net_config_lo_mode(char *lo_str);
extern void kni_net_poll_resp(struct kni_dev *kni);
extern void kni_set_ethtool_ops(struct net_device *netdev);

extern int ixgbe_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
extern void ixgbe_kni_remove(struct pci_dev *pdev);
extern int igb_kni_probe(struct pci_dev *pdev, struct net_device **lad_dev);
extern void igb_kni_remove(struct pci_dev *pdev);

static int kni_open(struct inode *inode, struct file *file);
static int kni_release(struct inode *inode, struct file *file);
static int kni_ioctl(struct inode *inode, unsigned int ioctl_num,
					unsigned long ioctl_param);
static int kni_compat_ioctl(struct inode *inode, unsigned int ioctl_num,
						unsigned long ioctl_param);
static int kni_dev_remove(struct kni_dev *dev);

static int __init kni_parse_kthread_mode(void);

/* KNI processing for single kernel thread mode */
static int kni_thread_single(void *unused);
/* KNI processing for multiple kernel thread mode */
static int kni_thread_multiple(void *param);

static struct file_operations kni_fops = {
	.owner = THIS_MODULE,
	.open = kni_open,
	.release = kni_release,
	.unlocked_ioctl = (void *)kni_ioctl,
	.compat_ioctl = (void *)kni_compat_ioctl,
};

static struct miscdevice kni_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = KNI_DEVICE,
	.fops = &kni_fops,
};

/* loopback mode */
static char *lo_mode = NULL;

#ifdef RTE_LIBRW_PIOT
/* Kernel thread mode */
static char *kthread_mode = "multiple";
#else
static char *kthread_mode = NULL;
#endif
static unsigned multiple_kthread_on = 0;

#define KNI_DEV_IN_USE_BIT_NUM 0 /* Bit number for device in use */

static volatile unsigned long device_in_use; /* device in use flag */
static struct task_struct *kni_kthread;

/* kni list lock */
static DECLARE_RWSEM(kni_list_lock);

/* kni list */
static struct list_head kni_list_head = LIST_HEAD_INIT(kni_list_head);
#ifdef RTE_LIBRW_PIOT
/**
 * This function is called to revert the filyer to bpf format.
 * This is such a hack. Check for changes in net/core/filter.c.
 * moving it here since the symbol sk_decode_filter is not exported
*/
static void kni_sk_decode_filter(struct sock_filter *filt, struct sock_filter *to)
{
  static const u16 decodes[] = {
    [BPF_S_ALU_ADD_K]	= BPF_ALU|BPF_ADD|BPF_K,
    [BPF_S_ALU_ADD_X]	= BPF_ALU|BPF_ADD|BPF_X,
    [BPF_S_ALU_SUB_K]	= BPF_ALU|BPF_SUB|BPF_K,
    [BPF_S_ALU_SUB_X]	= BPF_ALU|BPF_SUB|BPF_X,
    [BPF_S_ALU_MUL_K]	= BPF_ALU|BPF_MUL|BPF_K,
    [BPF_S_ALU_MUL_X]	= BPF_ALU|BPF_MUL|BPF_X,
    [BPF_S_ALU_DIV_X]	= BPF_ALU|BPF_DIV|BPF_X,
    [BPF_S_ALU_MOD_K]	= BPF_ALU|BPF_MOD|BPF_K,
    [BPF_S_ALU_MOD_X]	= BPF_ALU|BPF_MOD|BPF_X,
    [BPF_S_ALU_AND_K]	= BPF_ALU|BPF_AND|BPF_K,
    [BPF_S_ALU_AND_X]	= BPF_ALU|BPF_AND|BPF_X,
    [BPF_S_ALU_OR_K]	= BPF_ALU|BPF_OR|BPF_K,
    [BPF_S_ALU_OR_X]	= BPF_ALU|BPF_OR|BPF_X,
    [BPF_S_ALU_XOR_K]	= BPF_ALU|BPF_XOR|BPF_K,
    [BPF_S_ALU_XOR_X]	= BPF_ALU|BPF_XOR|BPF_X,
    [BPF_S_ALU_LSH_K]	= BPF_ALU|BPF_LSH|BPF_K,
    [BPF_S_ALU_LSH_X]	= BPF_ALU|BPF_LSH|BPF_X,
    [BPF_S_ALU_RSH_K]	= BPF_ALU|BPF_RSH|BPF_K,
    [BPF_S_ALU_RSH_X]	= BPF_ALU|BPF_RSH|BPF_X,
    [BPF_S_ALU_NEG]		= BPF_ALU|BPF_NEG,
    [BPF_S_LD_W_ABS]	= BPF_LD|BPF_W|BPF_ABS,
    [BPF_S_LD_H_ABS]	= BPF_LD|BPF_H|BPF_ABS,
    [BPF_S_LD_B_ABS]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_PROTOCOL]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_PKTTYPE]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_IFINDEX]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_NLATTR]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_NLATTR_NEST]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_MARK]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_QUEUE]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_HATYPE]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_RXHASH]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_CPU]		= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_ALU_XOR_X]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_SECCOMP_LD_W] = BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_VLAN_TAG]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_VLAN_TAG_PRESENT] = BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_ANC_PAY_OFFSET]	= BPF_LD|BPF_B|BPF_ABS,
    [BPF_S_LD_W_LEN]	= BPF_LD|BPF_W|BPF_LEN,
    [BPF_S_LD_W_IND]	= BPF_LD|BPF_W|BPF_IND,
    [BPF_S_LD_H_IND]	= BPF_LD|BPF_H|BPF_IND,
    [BPF_S_LD_B_IND]	= BPF_LD|BPF_B|BPF_IND,
    [BPF_S_LD_IMM]		= BPF_LD|BPF_IMM,
    [BPF_S_LDX_W_LEN]	= BPF_LDX|BPF_W|BPF_LEN,
    [BPF_S_LDX_B_MSH]	= BPF_LDX|BPF_B|BPF_MSH,
    [BPF_S_LDX_IMM]		= BPF_LDX|BPF_IMM,
    [BPF_S_MISC_TAX]	= BPF_MISC|BPF_TAX,
    [BPF_S_MISC_TXA]	= BPF_MISC|BPF_TXA,
    [BPF_S_RET_K]		= BPF_RET|BPF_K,
    [BPF_S_RET_A]		= BPF_RET|BPF_A,
    [BPF_S_ALU_DIV_K]	= BPF_ALU|BPF_DIV|BPF_K,
    [BPF_S_LD_MEM]		= BPF_LD|BPF_MEM,
    [BPF_S_LDX_MEM]		= BPF_LDX|BPF_MEM,
    [BPF_S_ST]		= BPF_ST,
    [BPF_S_STX]		= BPF_STX,
    [BPF_S_JMP_JA]		= BPF_JMP|BPF_JA,
    [BPF_S_JMP_JEQ_K]	= BPF_JMP|BPF_JEQ|BPF_K,
    [BPF_S_JMP_JEQ_X]	= BPF_JMP|BPF_JEQ|BPF_X,
    [BPF_S_JMP_JGE_K]	= BPF_JMP|BPF_JGE|BPF_K,
    [BPF_S_JMP_JGE_X]	= BPF_JMP|BPF_JGE|BPF_X,
    [BPF_S_JMP_JGT_K]	= BPF_JMP|BPF_JGT|BPF_K,
    [BPF_S_JMP_JGT_X]	= BPF_JMP|BPF_JGT|BPF_X,
    [BPF_S_JMP_JSET_K]	= BPF_JMP|BPF_JSET|BPF_K,
    [BPF_S_JMP_JSET_X]	= BPF_JMP|BPF_JSET|BPF_X,
  };
  u16 code;
  
  code = filt->code;
  
  to->code = decodes[code];
  to->jt = filt->jt;
  to->jf = filt->jf;
  
  if (code == BPF_S_ALU_DIV_K) {
    /*
     * When loaded this rule user gave us X, which was
     * translated into R = r(X). Now we calculate the
     * RR = r(R) and report it back. If next time this
     * value is loaded and RRR = r(RR) is calculated
     * then the R == RRR will be true.
     *
     * One exception. X == 1 translates into R == 0 and
     * we can't calculate RR out of it with r().
     */
    
    if (filt->k == 0)
      to->k = 1;
    else
      to->k = reciprocal_value(filt->k);
    
    BUG_ON(reciprocal_value(to->k) != filt->k);
  } else
    to->k = filt->k;
}

static int
kni_ioctl_clear_packet_counters(unsigned int ioctl_num, unsigned long ioctl_param)
{
  int ret = -EINVAL;
  struct kni_dev *dev, *n;
  
  
  down_write(&kni_list_lock);
  list_for_each_entry_safe(dev, n, &kni_list_head, list) {
    dev->rx_treat_as_tx = 0;
    dev->rx_treat_as_tx_delivered= 0;
    dev->rx_treat_as_tx_filtered= 0;
    dev->rx_only= 0;
    dev->rx_filtered= 0;
    dev->rx_delivered= 0;
    dev->tx_no_txq= 0;
    dev->tx_no_allocq= 0;
    dev->tx_enq_fail= 0;
    dev->tx_deq_fail= 0;
    dev->rx_drop_noroute= 0;
    memset(&dev->stats, 0, sizeof(dev->stats));
  }
  up_write(&kni_list_lock);
  ret = 0;

  return ret;
}
    
/**
 * ioctl handler to get the packet socket filters to the userspace
 * @param[in]  ioctl_num - the ioctl number
 * @param[in]  ioctl_param - userspace memory with inputs about the inode-id and skt
 * @param[out] ioctl_param - userspace memory to where the filters are copied
 *
 * @returns 0  if success
 * @returns -errno if error
 */
static int
kni_ioctl_get_packet_socket_info(unsigned int ioctl_num, unsigned long ioctl_param)
{
	int ret = -EINVAL;
	struct rte_kni_packet_socket_info *dev_info;
        unsigned short filterlen = 0;
        struct sock *sk;
        
	if (_IOC_SIZE(ioctl_num) > sizeof(*dev_info)){
          KNI_ERR("wrong param size in kni_ioctl_get_socket_info");
          return -EINVAL;
        }
        
        dev_info = (struct rte_kni_packet_socket_info *)kmalloc(sizeof(*dev_info),
                                                                GFP_KERNEL);
        if (!dev_info){
          KNI_ERR("alloc failed to copy user params in kni_ioctl_get_socket_info");
          return -EINVAL;
        }
	ret = copy_from_user(dev_info, (void *)ioctl_param, sizeof(*dev_info));
	if (ret) {
          KNI_ERR("copy_from_user in kni_ioctl_get_socket_info");
          ret = -EIO;
          goto error;
	}

        sk  = (struct sock *)dev_info->skt;
        //AKKI we need more checks here to confirm and also this is such a hack. we need to get the sk from the inode number and not
        //ptr deferenceing.
        if (sock_i_ino(sk) == dev_info->inode_id) {
          struct sk_filter *filter;
          int i, ret;
          
          lock_sock(sk);
          filter = rcu_dereference_protected(sk->sk_filter,
                                             sock_owned_by_user(sk));
          if (filter && filter->len &&
              (dev_info->len >= filter->len)){
            filterlen = filter->len;
            if (copy_to_user((void __user*)&((struct rte_kni_packet_socket_info *)ioctl_param)->len,
                             (const void*)&filterlen, sizeof(unsigned short))){
              release_sock(sk);
              ret = -EFAULT;
              goto error;
            }
            for (i = 0; i < filter->len; i++) {
              struct sock_filter fb;
              kni_sk_decode_filter(&filter->insns[i], &fb);
              if (copy_to_user( (void __user*)&((struct rte_kni_packet_socket_info *)ioctl_param)->filter[i],
                                (const void *)&fb, sizeof(fb))){
                release_sock(sk);
                ret = -EFAULT;
                goto error;
              }
            }
          }else{
            filterlen = 0;
            if (copy_to_user((void __user*)&((struct rte_kni_packet_socket_info *)ioctl_param)->len,
                             (const void *)&filterlen, sizeof(unsigned short))){
              release_sock(sk);
              ret = -EFAULT;
              goto error;
            }
          }
          release_sock(sk);
        }else{
          ret = -EINVAL;
          goto error;
        }
        ret = 0;
error:
        kfree(dev_info);
        return ret;
}
static int kni_stats_show(struct seq_file *seq, void *v)
{
  struct kni_dev *kni, *n;
 
  seq_puts(seq,
           "Index  Name      Net-namespace     rx  rx-as-tx   rx-as-tx-filter rx-as-tx-delv  rx-only  rx-filter  rx-delv  rx-no-skb tx-attempt tx tx-dropped tx-no-txq tx-no-allocq tx-enq-fail tx-deq-fail tx-noroute nl-tx-queue nl-tx-dequeue nl-rx-queued nl-rx-dequeued\n");

  down_read(&kni_list_lock);
  list_for_each_entry_safe(kni, n, &kni_list_head, list) {
    seq_printf(seq, "%-5d %-15s %s     %lu %llu %llu %llu %llu %llu %llu %lu %llu %lu %lu %llu %llu %llu %llu %llu %llu %llu  %llu %llu\n",
               kni->net_dev->ifindex,
               kni->name, 
               kni->netns_name,
               kni->stats.rx_packets, kni->rx_treat_as_tx,
               kni->rx_treat_as_tx_filtered, kni->rx_treat_as_tx_delivered,
               kni->rx_only, kni->rx_filtered, kni->rx_delivered,
               kni->stats.rx_dropped,
               kni->tx_attempted,
               kni->stats.tx_packets,
               kni->stats.tx_dropped, kni->tx_no_txq, kni->tx_no_allocq,
               kni->tx_enq_fail, kni->tx_deq_fail, kni->rx_drop_noroute,
               kni->nl_tx_queued, kni->nl_tx_dequeued,
               kni->nl_rx_queued, kni->nl_rx_dequeued);
  }
  up_read(&kni_list_lock);

  return 0;
}


static int kni_seq_open(struct inode *inode, struct file *file)
{
  return single_open_net(inode, file, kni_stats_show);
}


static const struct file_operations kni_seq_fops = {
	.owner		= THIS_MODULE,
	.open           = kni_seq_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release	= single_release_net,
};

static int __net_init kni_netns_init(struct net *net)
{
  if (!proc_create("kni", 0, net->proc_net, &kni_seq_fops))
    return -ENOMEM;
#ifdef RTE_LIBRW_NOHUGE
  kni_netlink_net_init(net); 
#endif
  return 0;
}

static void __net_exit kni_netns_exit(struct net *net)
{
  remove_proc_entry("kni", net->proc_net);
#ifdef RTE_LIBRW_NOHUGE
  kni_netlink_net_exit(net);
#endif
}


static struct pernet_operations kni_net_ops = {
	.init = kni_netns_init,
	.exit = kni_netns_exit,
#ifdef RTE_LIBRW_NOHUGE
        .id = &kni_net_id,
        .size = sizeof(struct kni_net_namespace),
#endif
};

static int kni_proc_init(void)
{
  return register_pernet_subsys(&kni_net_ops);
}

static void kni_proc_exit(void){
  return unregister_pernet_subsys(&kni_net_ops);
}

#endif /*PIOT*/
static int __init
kni_init(void)
{
	KNI_PRINT("######## DPDK kni module loading ########\n");

	if (kni_parse_kthread_mode() < 0) {
		KNI_ERR("Invalid parameter for kthread_mode\n");
		return -EINVAL;
	}

	if (misc_register(&kni_misc) != 0) {
		KNI_ERR("Misc registration failed\n");
		return -EPERM;
	}

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use);

	/* Configure the lo mode according to the input parameter */
	kni_net_config_lo_mode(lo_mode);

	KNI_PRINT("######## DPDK kni module loaded  ########\n");
#ifdef RTE_LIBRW_PIOT
        kni_proc_init();
#endif
	return 0;
}

static void __exit
kni_exit(void)
{
#ifdef RTE_LIBRW_PIOT
  kni_proc_exit();
#endif
  misc_deregister(&kni_misc);
	KNI_PRINT("####### DPDK kni module unloaded  #######\n");
}

static int __init
kni_parse_kthread_mode(void)
{
	if (!kthread_mode)
		return 0;

	if (strcmp(kthread_mode, "single") == 0)
		return 0;
	else if (strcmp(kthread_mode, "multiple") == 0)
		multiple_kthread_on = 1;
	else
		return -1;

	return 0;
}

static int
kni_open(struct inode *inode, struct file *file)
{
	/* kni device can be opened by one user only, test and set bit */
	if (test_and_set_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use))
		return -EBUSY;

	/* Create kernel thread for single mode */
	if (multiple_kthread_on == 0) {
		KNI_PRINT("Single kernel thread for all KNI devices\n");
		/* Create kernel thread for RX */
		kni_kthread = kthread_run(kni_thread_single, NULL,
						"kni_single");
		if (IS_ERR(kni_kthread)) {
			KNI_ERR("Unable to create kernel threaed\n");
			return PTR_ERR(kni_kthread);
		}
	} else
		KNI_PRINT("Multiple kernel thread mode enabled\n");

	KNI_PRINT("/dev/kni opened\n");

	return 0;
}

static int
kni_release(struct inode *inode, struct file *file)
{
	struct kni_dev *dev, *n;

	/* Stop kernel thread for single mode */
	if (multiple_kthread_on == 0) {
		/* Stop kernel thread */
		kthread_stop(kni_kthread);
		kni_kthread = NULL;
	}

	down_write(&kni_list_lock);
	list_for_each_entry_safe(dev, n, &kni_list_head, list) {
		/* Stop kernel thread for multiple mode */
		if (multiple_kthread_on && dev->pthread != NULL) {
			kthread_stop(dev->pthread);
			dev->pthread = NULL;
		}

#ifdef RTE_KNI_VHOST
		kni_vhost_backend_release(dev);
#endif
		kni_dev_remove(dev);
		list_del(&dev->list);
	}
	up_write(&kni_list_lock);

	/* Clear the bit of device in use */
	clear_bit(KNI_DEV_IN_USE_BIT_NUM, &device_in_use);

	KNI_PRINT("/dev/kni closed\n");

	return 0;
}

static int
kni_thread_single(void *unused)
{
	int j;
	struct kni_dev *dev, *n;

	while (!kthread_should_stop()) {
		down_read(&kni_list_lock);
		for (j = 0; j < KNI_RX_LOOP_NUM; j++) {
			list_for_each_entry_safe(dev, n,
					&kni_list_head, list) {
#ifdef RTE_KNI_VHOST
				kni_chk_vhost_rx(dev);
#else
				kni_net_rx(dev);
#endif
#ifndef RTE_LIBRW_PIOT
				kni_net_poll_resp(dev);
#endif
			}
		}
		up_read(&kni_list_lock);
#ifdef RTE_KNI_PREEMPT_DEFAULT
		/* reschedule out for a while */
		schedule_timeout_interruptible(usecs_to_jiffies( \
				KNI_KTHREAD_RESCHEDULE_INTERVAL));
#endif
	}

	return 0;
}

static int
kni_thread_multiple(void *param)
{
	int j;
	struct kni_dev *dev = (struct kni_dev *)param;

	while (!kthread_should_stop()) {
		for (j = 0; j < KNI_RX_LOOP_NUM; j++) {
#ifndef RTE_LIBRW_PIOT
#ifdef RTE_KNI_VHOST
			kni_chk_vhost_rx(dev);
#else
			kni_net_rx(dev);
#endif
			kni_net_poll_resp(dev);
#else
#ifdef RTE_LIBRW_NOHUGE
                        if (dev->nohuge){
                          kni_net_rx_netlink(dev);
                          kni_net_tx_netlink(dev);
                        }else{
                          kni_net_rx(dev);
                        }
#else
                        kni_net_rx(dev);
#endif
#endif //piot
		}
#ifdef RTE_KNI_PREEMPT_DEFAULT
		schedule_timeout_interruptible(usecs_to_jiffies( \
				KNI_KTHREAD_RESCHEDULE_INTERVAL));
#endif
	}

	return 0;
}

static int
kni_dev_remove(struct kni_dev *dev)
{
	if (!dev)
		return -ENODEV;

	switch (dev->device_id) {
	#define RTE_PCI_DEV_ID_DECL_IGB(vend, dev) case (dev):
	#include <rte_pci_dev_ids.h>
		igb_kni_remove(dev->pci_dev);
		break;
	#define RTE_PCI_DEV_ID_DECL_IXGBE(vend, dev) case (dev):
	#include <rte_pci_dev_ids.h>
		ixgbe_kni_remove(dev->pci_dev);
		break;
	default:
		break;
	}
#ifdef RTE_LIBRW_PIOT
        if (dev->dev_addr){
          kfree(dev->dev_addr);
          dev->dev_addr = NULL;
        }
#endif
	if (dev->net_dev) {
#if 0
          rtnl_lock();
          if (dev_net(dev->net_dev)->loopback_dev == dev->net_dev){
            struct net_device *lodev = __dev_get_by_name(dev_net(dev->net_dev), "lo");
            printk(KERN_INFO "Setting the loopback device back to lo from %s\n",
                   dev->net_dev->name);
            dev_net(dev->net_dev)->loopback_dev = lodev;
          }
          rtnl_unlock();
#endif
          unregister_netdev(dev->net_dev);
          free_netdev(dev->net_dev);
	}

	return 0;
}

static int
kni_check_param(struct kni_dev *kni, struct rte_kni_device_info *dev)
{
	if (!kni || !dev)
		return -1;

	/* Check if network name has been used */
	if (!strncmp(kni->name, dev->name, RTE_KNI_NAMESIZE)) {
		KNI_ERR("KNI name %s duplicated\n", dev->name);
		return -1;
	}

	return 0;
}

static int
kni_ioctl_create(unsigned int ioctl_num, unsigned long ioctl_param)
{
	int ret;
	struct rte_kni_device_info dev_info;
	struct pci_dev *pci = NULL;
	struct pci_dev *found_pci = NULL;
	struct net_device *net_dev = NULL;
	struct net_device *lad_dev = NULL;
	struct kni_dev *kni, *dev, *n;
	struct net *net;

	printk(KERN_INFO "KNI: Creating kni...\n");
	/* Check the buffer size, to avoid warning */
	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info)){
          printk(KERN_INFO "KNI: Creating kni... failed size mismatch\n");
          return -EINVAL;
        }

	/* Copy kni info from user space */
	ret = copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info));
	if (ret) {
          printk(KERN_INFO "KNI: Creating kni... failed copy from the user \n");
          KNI_ERR("copy_from_user in kni_ioctl_create");
          return -EIO;
	}

	/**
	 * Check if the cpu core id is valid for binding,
	 * for multiple kernel thread mode.
	 */
	if (multiple_kthread_on && dev_info.force_bind &&
            !cpu_online(dev_info.core_id)) {
                printk(KERN_INFO "KNI: Creating kni... failed force_bind, no cpu \n");
		KNI_ERR("cpu %u is not online\n", dev_info.core_id);
		return -EINVAL;
	}

	/* Check if it has been created */
	down_read(&kni_list_lock);
	list_for_each_entry_safe(dev, n, &kni_list_head, list) {
		if (kni_check_param(dev, &dev_info) < 0) {
			up_read(&kni_list_lock);
                        printk(KERN_INFO "KNI: Creating kni... failed Already created\n");
			return -EINVAL;
		}
	}
	up_read(&kni_list_lock);
#ifdef RTE_LIBRW_PIOT
        if (dev_info.no_tx || dev_info.no_pci || dev_info.no_data){
          if (dev_info.loopback){
            net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
                                   kni_net_lb_init);
          }else{
            net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
                                   kni_net_ip_init);
          }
        }else{
          net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
                                 kni_net_init);
        }
#else
	net_dev = alloc_netdev(sizeof(struct kni_dev), dev_info.name,
#ifdef NET_NAME_UNKNOWN
							NET_NAME_UNKNOWN,
#endif
							kni_net_init);
#endif//piot
	if (net_dev == NULL) {
                printk(KERN_INFO "KNI: Creating kni... failed BUSY\n");
		KNI_ERR("error allocating device \"%s\"\n", dev_info.name);
		return -EBUSY;
	}
#ifdef RTE_LIBRW_PIOT
        {
          pid_t pid = task_pid_vnr(current);
          if ((int)pid <= 0){
            printk(KERN_INFO "KNI: Creating kni... resolveing error with the current in a different namespace using 0x%x\n", dev_info.pid);
            pid = dev_info.pid;
          }
          net = get_net_ns_by_pid(pid);
          if (IS_ERR(net)) {
            printk(KERN_INFO "KNI: Creating kni... failed no netnamepsace for pid 0x%x\n",
                   pid);
            free_netdev(net_dev);
            return PTR_ERR(net);
          }
          dev_net_set(net_dev, net);
          put_net(net);
        }
#endif
	kni = netdev_priv(net_dev);

	kni->net_dev = net_dev;
	kni->group_id = dev_info.group_id;
	kni->core_id = dev_info.core_id;
        
	strncpy(kni->name, dev_info.name, RTE_KNI_NAMESIZE);
#ifdef RTE_LIBRW_PIOT
        kni->no_data = dev_info.no_data;
        kni->no_pci = dev_info.no_pci;
        kni->no_tx  = dev_info.no_tx;
        kni->always_up = dev_info.always_up;
        kni->loopback = dev_info.loopback;
        kni->no_user_ring = dev_info.no_user_ring;
        kni->vlanid = dev_info.vlanid;
        kni->dev_addr = (unsigned char *)kmalloc(ETH_ALEN,
                                                 GFP_KERNEL);
        memcpy(kni->dev_addr, dev_info.mac, ETH_ALEN);
#ifdef RTE_LIBRW_NOHUGE
        kni->nohuge = dev_info.nohuge;
        kni->nl_pid = dev_info.nl_pid;
        if (dev_info.nohuge){
          skb_queue_head_init(&kni->skb_rx_queue);
          skb_queue_head_init(&kni->skb_tx_queue);
          goto pci_check;
        }
#endif
#endif
	/* Translate user space info into kernel space info */
	kni->tx_q = phys_to_virt(dev_info.tx_phys);
	kni->rx_q = phys_to_virt(dev_info.rx_phys);
	kni->alloc_q = phys_to_virt(dev_info.alloc_phys);
	kni->free_q = phys_to_virt(dev_info.free_phys);
#ifndef RTE_LIBRW_PIOT
	kni->req_q = phys_to_virt(dev_info.req_phys);
	kni->resp_q = phys_to_virt(dev_info.resp_phys);
#endif
	kni->sync_va = dev_info.sync_va;
	kni->sync_kva = phys_to_virt(dev_info.sync_phys);

	kni->mbuf_kva = phys_to_virt(dev_info.mbuf_phys);
	kni->mbuf_va = dev_info.mbuf_va;

#ifdef RTE_KNI_VHOST
	kni->vhost_queue = NULL;
	kni->vq_status = BE_STOP;
#endif
	kni->mbuf_size = dev_info.mbuf_size;

	KNI_PRINT("tx_phys:      0x%016llx, tx_q addr:      0x%p\n",
		(unsigned long long) dev_info.tx_phys, kni->tx_q);
	KNI_PRINT("rx_phys:      0x%016llx, rx_q addr:      0x%p\n",
		(unsigned long long) dev_info.rx_phys, kni->rx_q);
	KNI_PRINT("alloc_phys:   0x%016llx, alloc_q addr:   0x%p\n",
		(unsigned long long) dev_info.alloc_phys, kni->alloc_q);
	KNI_PRINT("free_phys:    0x%016llx, free_q addr:    0x%p\n",
		(unsigned long long) dev_info.free_phys, kni->free_q);
#ifndef RTE_LIBRW_PIOT
	KNI_PRINT("req_phys:     0x%016llx, req_q addr:     0x%p\n",
		(unsigned long long) dev_info.req_phys, kni->req_q);
	KNI_PRINT("resp_phys:    0x%016llx, resp_q addr:    0x%p\n",
		(unsigned long long) dev_info.resp_phys, kni->resp_q);
#endif
	KNI_PRINT("mbuf_phys:    0x%016llx, mbuf_kva:       0x%p\n",
		(unsigned long long) dev_info.mbuf_phys, kni->mbuf_kva);
	KNI_PRINT("mbuf_va:      0x%p\n", dev_info.mbuf_va);
	KNI_PRINT("mbuf_size:    %u\n", kni->mbuf_size);

	KNI_DBG("PCI: %02x:%02x.%02x %04x:%04x\n",
					dev_info.bus,
					dev_info.devid,
					dev_info.function,
					dev_info.vendor_id,
					dev_info.device_id);
#ifdef RTE_LIBRW_PIOT
#ifdef RTE_LIBRW_NOHUGE
pci_check:
#endif
        if (dev_info.no_pci){
          kni->lad_dev = NULL;
          goto register_netdev;
        }
#endif
	pci = pci_get_device(dev_info.vendor_id, dev_info.device_id, NULL);

	/* Support Ethtool */
	while (pci) {
		KNI_PRINT("pci_bus: %02x:%02x:%02x \n",
					pci->bus->number,
					PCI_SLOT(pci->devfn),
					PCI_FUNC(pci->devfn));

		if ((pci->bus->number == dev_info.bus) &&
			(PCI_SLOT(pci->devfn) == dev_info.devid) &&
			(PCI_FUNC(pci->devfn) == dev_info.function)) {
			found_pci = pci;
			switch (dev_info.device_id) {
			#define RTE_PCI_DEV_ID_DECL_IGB(vend, dev) case (dev):
			#include <rte_pci_dev_ids.h>
				ret = igb_kni_probe(found_pci, &lad_dev);
				break;
			#define RTE_PCI_DEV_ID_DECL_IXGBE(vend, dev) \
							case (dev):
			#include <rte_pci_dev_ids.h>
				ret = ixgbe_kni_probe(found_pci, &lad_dev);
				break;
			default:
				ret = -1;
				break;
			}

			KNI_DBG("PCI found: pci=0x%p, lad_dev=0x%p\n",
							pci, lad_dev);
			if (ret == 0) {
				kni->lad_dev = lad_dev;
				kni_set_ethtool_ops(kni->net_dev);
			} else {
				KNI_ERR("Device not supported by ethtool");
				kni->lad_dev = NULL;
			}

			kni->pci_dev = found_pci;
			kni->device_id = dev_info.device_id;
			break;
		}
		pci = pci_get_device(dev_info.vendor_id,
				dev_info.device_id, pci);
	}
	if (pci)
		pci_dev_put(pci);
#ifdef RTE_LIBRW_PIOT
register_netdev:
        strncpy(kni->netns_name, dev_info.netns_name, sizeof(kni->netns_name));
        net_dev->ifindex = dev_info.ifindex;
#endif
	ret = register_netdev(net_dev);
	if (ret) {
                printk(KERN_INFO "KNI: Creating kni... failed register_netdev \n");
		KNI_ERR("error %i registering device \"%s\"\n",
					ret, dev_info.name);
		kni_dev_remove(kni);
		return -ENODEV;
	}

#ifdef RTE_KNI_VHOST
	kni_vhost_init(kni);
#endif
#ifdef RTE_LIBRW_PIOT
        rtnl_lock();
#if 0
        if (dev_info.loopback) {
          if (&init_net != dev_net(net_dev)) {
            if (dev_net(net_dev)->loopback_dev){
              printk(KERN_INFO "Setting the loopback device to %s\n", net_dev->name);
              dev_net(net_dev)->loopback_dev = net_dev;
            }
          }
        }
#endif
        ret = netdev_rx_handler_register(net_dev, rw_fpath_kni_handle_frame,
					 net_dev);
	if (ret) {
          printk(KERN_INFO "KNI: Creating kni... failed cannot assign rx-handler\n");
          KNI_ERR("Error %d calling netdev_rx_handler_register\n", ret);
          rtnl_unlock();
          kni_dev_remove(kni);
          return -EINVAL;
	}
        if (dev_info.always_up){
          dev_open(net_dev);
        }
        /*By default loopback interface's mtu is set to zero in the init.
          So when the open is done, ipv4 does not assgin 127.0.0.1 to the
          loopback address.
          Once the device has been open, set the mtu for the loopback.
          This is so that the loopback does not
          take the 127.0.0.1 address. For ipv6, the kernel checks the ARPHDR state*/
        if (dev_info.mtu){
          dev_set_mtu(net_dev, dev_info.mtu);
          kni->mtu = dev_info.mtu;
        }
        rtnl_unlock();
#endif
	/**
	 * Create a new kernel thread for multiple mode, set its core affinity,
	 * and finally wake it up.
	 */
	if (multiple_kthread_on) {
		kni->pthread = kthread_create(kni_thread_multiple,
					      (void *)kni,
					      "kni_%s", kni->name);
		if (IS_ERR(kni->pthread)) {
                        printk(KERN_INFO "KNI: Creating kni... failed could not create lthread\n");                 
			kni_dev_remove(kni);
			return -ECANCELED;
		}
		if (dev_info.force_bind)
			kthread_bind(kni->pthread, kni->core_id);
		wake_up_process(kni->pthread);
	}

	down_write(&kni_list_lock);
	list_add(&kni->list, &kni_list_head);
	up_write(&kni_list_lock);

	return 0;
}

static int
kni_ioctl_release(unsigned int ioctl_num, unsigned long ioctl_param)
{
	int ret = -EINVAL;
	struct kni_dev *dev, *n;
	struct rte_kni_device_info dev_info;

	if (_IOC_SIZE(ioctl_num) > sizeof(dev_info))
			return -EINVAL;

	ret = copy_from_user(&dev_info, (void *)ioctl_param, sizeof(dev_info));
	if (ret) {
		KNI_ERR("copy_from_user in kni_ioctl_release");
		return -EIO;
	}

	/* Release the network device according to its name */
	if (strlen(dev_info.name) == 0)
		return ret;

	down_write(&kni_list_lock);
	list_for_each_entry_safe(dev, n, &kni_list_head, list) {
		if (strncmp(dev->name, dev_info.name, RTE_KNI_NAMESIZE) != 0)
			continue;

		if (multiple_kthread_on && dev->pthread != NULL) {
			kthread_stop(dev->pthread);
			dev->pthread = NULL;
		}

#ifdef RTE_KNI_VHOST
		kni_vhost_backend_release(dev);
#endif
		kni_dev_remove(dev);
		list_del(&dev->list);
		ret = 0;
		break;
	}
	up_write(&kni_list_lock);
	printk(KERN_INFO "KNI: %s release kni named %s\n",
		(ret == 0 ? "Successfully" : "Unsuccessfully"), dev_info.name);

	return ret;
}

static int
kni_ioctl(struct inode *inode,
	unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int ret = -EINVAL;

	KNI_DBG("IOCTL num=0x%0x param=0x%0lx \n", ioctl_num, ioctl_param);

	/*
	 * Switch according to the ioctl called
	 */
	switch (_IOC_NR(ioctl_num)) {
	case _IOC_NR(RTE_KNI_IOCTL_TEST):
		/* For test only, not used */
		break;
	case _IOC_NR(RTE_KNI_IOCTL_CREATE):
		ret = kni_ioctl_create(ioctl_num, ioctl_param);
		break;
	case _IOC_NR(RTE_KNI_IOCTL_RELEASE):
		ret = kni_ioctl_release(ioctl_num, ioctl_param);
		break;
#ifdef RTE_LIBRW_PIOT
        case _IOC_NR(RTE_KNI_IOCTL_GET_PACKET_SOCKET_INFO):
                ret = kni_ioctl_get_packet_socket_info(ioctl_num, ioctl_param);
                break;
          case _IOC_NR(RTE_KNI_IOCTL_CLEAR_PACKET_COUNTERS):
            ret = kni_ioctl_clear_packet_counters(ioctl_num, ioctl_param);
            break;
#endif
	default:
		KNI_DBG("IOCTL default \n");
		break;
	}

	return ret;
}

static int
kni_compat_ioctl(struct inode *inode,
		unsigned int ioctl_num,
		unsigned long ioctl_param)
{
	/* 32 bits app on 64 bits OS to be supported later */
	KNI_PRINT("Not implemented.\n");

	return -EINVAL;
}

module_init(kni_init);
module_exit(kni_exit);

module_param(lo_mode, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(lo_mode,
"KNI loopback mode (default=lo_mode_none):\n"
"    lo_mode_none        Kernel loopback disabled\n"
"    lo_mode_fifo        Enable kernel loopback with fifo\n"
"    lo_mode_fifo_skb    Enable kernel loopback with fifo and skb buffer\n"
"\n"
);

module_param(kthread_mode, charp, S_IRUGO);
MODULE_PARM_DESC(kthread_mode,
"Kernel thread mode (default=single):\n"
"    single    Single kernel thread mode enabled.\n"
"    multiple  Multiple kernel thread mode enabled.\n"
"\n"
);

