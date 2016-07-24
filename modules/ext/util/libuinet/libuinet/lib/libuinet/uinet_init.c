/*-
 * Copyright (c) 2010 Kip Macy
 * All rights reserved.
 * Copyright (c) 2013 Patrick Kelsey. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived in part from libplebnet's pn_init.c.
 *
 */

#include "opt_param.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/pcpu.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/smp.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include "uinet_internal.h"
#include "uinet_host_interface.h"


pid_t     getpid(void);
char *strndup(const char *str, size_t len);
unsigned int     sleep(unsigned int seconds);

extern void mi_startup(void);

extern void uinet_init_thread0(void);
extern void mutex_init(void);

#ifndef RIFT_UINET
static void shutdown_helper(void *arg);
static void one_sighandling_thread(void *arg);

pthread_mutex_t init_lock;
pthread_cond_t init_cond;

static struct thread *shutdown_helper_thread;
static struct thread *at_least_one_sighandling_thread;
static struct uhi_msg shutdown_helper_msg;
#endif
char uinet_intialised = FALSE;
char uinet_init_completed = FALSE;

struct uinet_instance uinst0;

int
uinet_init(unsigned int ncpus, unsigned int nmbclusters, struct uinet_instance_cfg *inst_cfg)
{
	struct thread *td;
	char tmpbuf[32];
	int boot_pages;
	int num_hash_buckets;
//#ifndef RIFT_TIMER
#if 1
	caddr_t v;
#endif

  if(uinet_intialised == TRUE) {
    return (0);
  }
  uinet_intialised = TRUE;

	if (ncpus > MAXCPU) {
		printf("Limiting number of CPUs to %u\n", MAXCPU);
		ncpus = MAXCPU;
	} else if (0 == ncpus) {
		printf("Setting number of CPUs to 1\n");
		ncpus = 1;
	}

	printf("uinet starting: cpus=%u, nmbclusters=%u\n", ncpus, nmbclusters);

	snprintf(tmpbuf, sizeof(tmpbuf), "%u", nmbclusters);
	setenv("kern.ipc.nmbclusters", tmpbuf);

	/* The env var kern.ncallout will get read in proc0_init(), but
	 * that's after we init the callwheel below.  So we set it here for
	 * consistency, but the operative setting is the direct assignment
	 * below.
	 */
        ncallout = HZ * 3600;
	snprintf(tmpbuf, sizeof(tmpbuf), "%u", ncallout);
	setenv("kern.ncallout", tmpbuf);

	/* Assuming maxsockets will be set to nmbclusters, the following
	 * sets the TCP tcbhash size so that perfectly uniform hashing would
	 * result in a maximum bucket depth of about 16.
	 */
	num_hash_buckets = 1;
	while (num_hash_buckets < nmbclusters / 16)
		num_hash_buckets <<= 1;
	snprintf(tmpbuf, sizeof(tmpbuf), "%u", num_hash_buckets);	
	setenv("net.inet.tcp.tcbhashsize", tmpbuf);

	snprintf(tmpbuf, sizeof(tmpbuf), "%u", 2048);
	setenv("net.inet.tcp.syncache.hashsize", tmpbuf);

	boot_pages = 16;  /* number of pages made available for uma to bootstrap itself */

	mp_ncpus = ncpus;
	mp_maxid = mp_ncpus - 1;

	uhi_set_num_cpus(mp_ncpus);

        /* vm_init bits */
	
	/* first get size required, then alloc memory, then give that memory to the second call */
//#ifndef RIFT_TIMER
#if 1
  v = 0;
  v = kern_timeout_callwheel_alloc(v,NULL);
  kern_timeout_callwheel_alloc(malloc(round_page((vm_offset_t)v), M_DEVBUF, M_ZERO),NULL);
  kern_timeout_callwheel_init(NULL);
#endif

	uinet_init_thread0();

#ifdef RIFT_UINET
  void *mem = uhi_mmap(NULL, boot_pages*PAGE_SIZE, UHI_PROT_READ|UHI_PROT_WRITE, UHI_MAP_ANON|UHI_MAP_PRIVATE, -1, 0);
  uma_startup(mem,boot_pages);
#else
  uma_startup(malloc(boot_pages*PAGE_SIZE, M_DEVBUF, M_ZERO), boot_pages);
#endif
	uma_startup2();

	/* XXX any need to tune this? */
	num_hash_buckets = 8192;  /* power of 2.  32 bytes per bucket on a 64-bit system, so no need to skimp */
	uma_page_slab_hash = malloc(sizeof(struct uma_page)*num_hash_buckets, M_DEVBUF, M_ZERO);
	uma_page_mask = num_hash_buckets - 1;

#ifndef RIFT_UINET
	pthread_mutex_init(&init_lock, NULL);
	pthread_cond_init(&init_cond, NULL);
#endif
	mutex_init();
        mi_startup();
	sx_init(&proctree_lock, "proctree");
 
	td = curthread;

	/* XXX - would very much like to do better than this */
	/* give all configuration threads time to complete initialization
	 * before continuing
	 */
	sleep(1);

	uinet_instance_init(&uinst0, vnet0, inst_cfg);

#ifndef RIFT_UINET
	if (uhi_msg_init(&shutdown_helper_msg, 1, 0) != 0)
		printf("Failed to init shutdown helper message - there will be no shutdown helper thread\n");
	else if (kthread_add(shutdown_helper, &shutdown_helper_msg, NULL, &shutdown_helper_thread, 0, 0, "shutdown_helper"))
		printf("Failed to create shutdown helper thread\n");

	/*
	 * XXX This should be configurable - applications that arrange for a
	 * particular thread to process all signals will not want this.
	 */
	if (kthread_add(one_sighandling_thread, NULL, NULL, &at_least_one_sighandling_thread, 0, 0, "one_sighandler"))
		printf("Failed to create at least one signal handling thread\n");
	uhi_mask_all_signals();

	printf("maxusers=%d\n", maxusers);
	printf("maxfiles=%d\n", maxfiles);
	printf("maxsockets=%d\n", maxsockets);
	printf("nmbclusters=%d\n", nmbclusters);
#endif

  uinet_init_completed = TRUE;
	return (0);
}


#ifndef RIFT_UINET
static void
one_sighandling_thread(void *arg)
{
	uhi_unmask_all_signals();

	for (;;) {
		uhi_nanosleep(60 * UHI_NSEC_PER_SEC);
	}
}

static void
shutdown_helper(void *arg)
{
	struct uhi_msg *msg = arg;
	uint8_t signo;
	int lock_attempts;
	int have_lock;
	VNET_ITERATOR_DECL(vnet_iter);
	struct uinet_instance *uinst;
	int shutdown_complete = 0;

	if (msg) {

		/*
		 * Loop to respond to multiple messages, but only shutdown
		 * once.  This allows multiple, possibly concurrent,
		 * executions of uinet_shutdown() to result in one shutdown
		 * and none of the calls to uinet_shutdown() to block
		 * indefinitely.  This provides nice behavior when
		 * uinet_shutdown() is called from a signal handler in a
		 * multi-threaded application that is not carefully policing
		 * signal masks in all the threads.
		 */
		for (;;) {
			if (uhi_msg_wait(msg, &signo) == 0) {
				if (!shutdown_complete) {
					printf("\nuinet shutting down");
					if (signo)
						printf(" from signal handler (signal %u)",
						       signo);
					printf("\n");
				
					printf("Shutting down all uinet instances...\n");
					/*
					 * We may be shutting down as a
					 * result of a signal occurring
					 * while another thread is holding
					 * the vnet list lock, so attempt to
					 * acquire the lock in a way that
					 * will avoid getting stuck.
					 */ 
					lock_attempts = 0;
					have_lock = 0;
					while ((lock_attempts < 5) && !(have_lock = VNET_LIST_TRY_RLOCK())) {
						printf("Waiting for vnet list lock...\n");
						uhi_nanosleep(UHI_NSEC_PER_SEC);
						lock_attempts++;
					}
					if (lock_attempts > 0 && have_lock)
						printf("Acquired vnet list lock\n");
					if (!have_lock)
						printf("Proceeding without vnet list lock\n");
#ifdef VIMAGE
					VNET_FOREACH(vnet_iter) {
						uinst = vnet_iter->vnet_uinet;
						uinet_instance_shutdown(uinst);
					}
#else
					uinet_instance_shutdown(uinet_instance_default());
#endif
					if (have_lock)
						VNET_LIST_RUNLOCK();
			
					printf("uinet shutdown complete\n");
				
					shutdown_complete = 1;
				}

				uhi_msg_rsp_send(msg, NULL);
			} else {
				printf("Failed to receive shutdown message\n");
			}
		}
	}

	printf("Shutdown helper thread exiting\n");
}

void __attribute__((weak))
uinet_shutdown(unsigned int signo)
{
	uint8_t signo_msg = signo;

	uhi_msg_send(&shutdown_helper_msg, &signo_msg);
	uhi_msg_rsp_wait(&shutdown_helper_msg, NULL);

	/*
	 * uinet_shutdown() may in general be invoked from a signal handler,
	 * and multi-threaded applications may in general have the same
	 * signal handler running concurrently in multiple threads, and thus
	 * multiple instances of uinet_shutdown() may be running
	 * concurrently if it is called from a signal handler in a
	 * multi-threaded application.  Absent a portable multiprocessor
	 * memory barrier API to use to ensure uinet_shutdown() only ever
	 * effectively runs once, there is no safe way to destroy
	 * shutdown_helper_msg, so for now we leak whatever small bit of
	 * context might be associated with the message.
	 */
#ifndef RIFT_UINET
	 uhi_msg_destroy(&shutdown_helper_msg); 
#endif
}


void
uinet_install_sighandlers(void)
{
	uhi_install_sighandlers();
}
#endif
