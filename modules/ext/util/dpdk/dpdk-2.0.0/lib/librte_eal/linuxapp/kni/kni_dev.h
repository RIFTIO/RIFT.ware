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

#ifndef _KNI_DEV_H_
#define _KNI_DEV_H_

#include <linux/if.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#ifdef RTE_KNI_VHOST
#include <net/sock.h>
#endif
#ifdef RTE_LIBRW_PIOT
#ifdef RTE_LIBRW_NOHUGE
#include "kni_netlink.h"
#endif
#endif/*piot*/
#include <exec-env/rte_kni_common.h>
#define KNI_KTHREAD_RESCHEDULE_INTERVAL 5 /* us */

/**
 * A structure describing the private information for a kni device.
 */

struct kni_dev {
	/* kni list */
	struct list_head list;

	struct net_device_stats stats;
	int status;
	uint16_t group_id;           /* Group ID of a group of KNI devices */
	unsigned core_id;            /* Core ID to bind */
	char name[RTE_KNI_NAMESIZE]; /* Network device name */
	struct task_struct *pthread;

	/* wait queue for req/resp */
	wait_queue_head_t wq;
	struct mutex sync_lock;

	/* PCI device id */
	uint16_t device_id;

	/* kni device */
	struct net_device *net_dev;
	struct net_device *lad_dev;
	struct pci_dev *pci_dev;

	/* queue for packets to be sent out */
	void *tx_q;

	/* queue for the packets received */
	void *rx_q;

	/* queue for the allocated mbufs those can be used to save sk buffs */
	void *alloc_q;

	/* free queue for the mbufs to be freed */
	void *free_q;

	/* request queue */
	void *req_q;

	/* response queue */
	void *resp_q;

	void * sync_kva;
	void *sync_va;

	void *mbuf_kva;
	void *mbuf_va;

	/* mbuf size */
	unsigned mbuf_size;

	/* synchro for request processing */
	unsigned long synchro;

#ifdef RTE_KNI_VHOST
	struct kni_vhost_queue* vhost_queue;
	volatile enum {
		BE_STOP = 0x1,
		BE_START = 0x2,
		BE_FINISH = 0x4,
	}vq_status;
#endif
#ifdef RTE_LIBRW_PIOT
  char           netns_name[64];
  uint64_t       rx_treat_as_tx; //not
  uint64_t       rx_treat_as_tx_delivered;//not
  uint64_t       rx_treat_as_tx_filtered; //not
  uint64_t       rx_only; //not
  uint64_t       rx_filtered; //not
  uint64_t       rx_delivered;
  uint64_t       tx_no_txq;
  uint64_t       tx_no_allocq;
  uint64_t       tx_enq_fail;
  uint64_t       tx_deq_fail;
  uint64_t       rx_drop_noroute;//not
  uint64_t       tx_attempted;



  uint64_t       nl_tx_queued;
  uint64_t       nl_tx_dequeued;
  uint64_t       nl_rx_queued;
  uint64_t       nl_rx_dequeued;
  
  unsigned char	 *dev_addr;
  uint8_t        no_data;
  uint8_t        no_tx;
  uint8_t        no_pci;
  uint8_t        loopback;
  uint8_t        no_user_ring;
  uint8_t        always_up;
  uint16_t       mtu;
  uint16_t       vlanid;
  char           mac[6];
#ifdef RTE_LIBRW_NOHUGE
  struct sk_buff_head    skb_tx_queue;
  struct sk_buff_head    skb_rx_queue;
  uint8_t        nohuge;
  uint32_t       nl_pid;
#endif
#endif /*piot*/
};

#define KNI_ERR(args...) printk(KERN_DEBUG "KNI: Error: " args)
#define KNI_PRINT(args...) printk(KERN_DEBUG "KNI: " args)
#ifdef RTE_KNI_KO_DEBUG
	#define KNI_DBG(args...) printk(KERN_DEBUG "KNI: " args)
#else
	#define KNI_DBG(args...)
#endif

#ifdef RTE_KNI_VHOST
unsigned int
kni_poll(struct file *file, struct socket *sock, poll_table * wait);
int kni_chk_vhost_rx(struct kni_dev *kni);
int kni_vhost_init(struct kni_dev *kni);
int kni_vhost_backend_release(struct kni_dev *kni);

struct kni_vhost_queue {
	struct sock sk;
	struct socket *sock;
	int vnet_hdr_sz;
	struct kni_dev *kni;
	int sockfd;
	unsigned int flags;
	struct sk_buff* cache;
	struct rte_kni_fifo* fifo;
};

#endif

#ifdef RTE_KNI_VHOST_DEBUG_RX
	#define KNI_DBG_RX(args...) printk(KERN_DEBUG "KNI RX: " args)
#else
	#define KNI_DBG_RX(args...)
#endif

#ifdef RTE_KNI_VHOST_DEBUG_TX
	#define KNI_DBG_TX(args...) printk(KERN_DEBUG "KNI TX: " args)
#else
	#define KNI_DBG_TX(args...)
#endif
#ifdef RTE_LIBRW_PIOT
void kni_net_process_rx_packet(struct sk_buff *skb,
                                 struct net_device *dev,
                                 struct rw_kni_mbuf_metadata *meta_data);
#ifdef RTE_LIBRW_NOHUGE
void kni_net_rx_netlink(struct kni_dev *kni);
void kni_net_tx_netlink(struct kni_dev *kni);
#endif
#endif
#endif

