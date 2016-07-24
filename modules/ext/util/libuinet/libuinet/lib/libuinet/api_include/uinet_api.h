/*
 * Copyright (c) 2014 Patrick Kelsey. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef	_UINET_API_H_
#define	_UINET_API_H_

#ifdef __cplusplus
extern "C" {
#endif


#ifndef __USE_BSD
#define __USE_BSD 1
#endif
#ifdef _SYS_TYPES_H
#undef _SYS_TYPES_H
#endif
#include <sys/types.h>
#include "uinet_api_errno.h"
#include "uinet_api_types.h"
#include "uinet_queue.h"

void  uinet_finalize_thread(void);
int   uinet_getl2info(struct uinet_socket *so, struct uinet_in_l2info *l2i);
int   uinet_getifstat(uinet_instance_t uinst, const char *name, struct uinet_ifstat *stat);
void  uinet_gettcpstat(uinet_instance_t uinst, struct uinet_tcpstat *stat);
char *uinet_inet_ntoa(struct uinet_in_addr in, char *buf, unsigned int size);
const char *uinet_inet_ntop(int af, const void *src, char *dst, unsigned int size);
int   uinet_inet_pton(int af, const char *src, void *dst);
int   uinet_inet6_enabled(void);
int   uinet_init(unsigned int ncpus, unsigned int nmbclusters, struct uinet_instance_cfg *inst_cfg);
int   uinet_initialize_thread(void);
void  uinet_install_sighandlers(void);
int   uinet_interface_add_alias(uinet_instance_t uinst, const char *name, const char *addr, const char *braddr, const char *mask);
int   uinet_interface_create(uinet_instance_t uinst, const char *name);
int   uinet_interface_up(uinet_instance_t uinst, const char *name, unsigned int promisc, unsigned int promiscinet);
int   uinet_l2tagstack_cmp(const struct uinet_in_l2tagstack *ts1, const struct uinet_in_l2tagstack *ts2);
uint32_t uinet_l2tagstack_hash(const struct uinet_in_l2tagstack *ts);
int   uinet_mac_aton(const char *macstr, uint8_t *macout);
int   uinet_make_socket_passive(struct uinet_socket *so);
int   uinet_make_socket_promiscuous(struct uinet_socket *so, unsigned int fib);
uinet_pool_t uinet_pool_create(char *name, int size, uinet_pool_ctor ctor, uinet_pool_dtor dtor,
			       uinet_pool_init init, uinet_pool_fini fini, int align, uint16_t flags);
void *uinet_pool_alloc_arg(uinet_pool_t pool, void *arg, int flags);
static inline void *uinet_pool_alloc(uinet_pool_t pool, int flags);
static inline void *
uinet_pool_alloc(uinet_pool_t pool, int flags)
{
	return uinet_pool_alloc_arg(pool, NULL, flags);
}
void  uinet_pool_free_arg(uinet_pool_t pool, void *item, void *arg);
static inline void  uinet_pool_free(uinet_pool_t pool, void *item);
static inline void
uinet_pool_free(uinet_pool_t pool, void *item)
{
	uinet_pool_free_arg(pool, item, NULL);
}
void  uinet_pool_destroy(uinet_pool_t pool);
int   uinet_pool_set_max(uinet_pool_t pool, int nitems);
int   uinet_pool_get_max(uinet_pool_t pool);
int   uinet_pool_get_cur(uinet_pool_t pool);
int   uinet_setl2info(struct uinet_socket *so, struct uinet_in_l2info *l2i);
int   uinet_setl2info2(struct uinet_socket *so, const uint8_t *local_addr, const uint8_t *foreign_addr,
		       uint16_t flags, const struct uinet_in_l2tagstack *tagstack);
void  uinet_shutdown(unsigned int signo);
int   uinet_soaccept(struct uinet_socket *listener, struct uinet_sockaddr **nam, struct uinet_socket **aso);
int   uinet_soallocuserctx(struct uinet_socket *so);
int   uinet_sobind(struct uinet_socket *so, struct uinet_sockaddr *nam);
int   uinet_soclose(struct uinet_socket *so);
int   uinet_soconnect(struct uinet_socket *so, struct uinet_sockaddr *nam);
int   uinet_socreate(uinet_instance_t uinst, int dom, struct uinet_socket **aso, int type, int proto);
void  uinet_sogetconninfo(struct uinet_socket *so, struct uinet_in_conninfo *inc);
int   uinet_sogeterror(struct uinet_socket *so);
uinet_instance_t uinet_sogetinstance(struct uinet_socket *so);
struct uinet_socket *uinet_sogetpassivepeer(struct uinet_socket *so);
int   uinet_sogetsockopt(struct uinet_socket *so, int level, int optname, void *optval, unsigned int *optlen);
int   uinet_sogetstate(struct uinet_socket *so);
void *uinet_sogetuserctx(struct uinet_socket *so, int key);
int   uinet_solisten(struct uinet_socket *so, int backlog);
int   uinet_soreadable(struct uinet_socket *so, unsigned int in_upcall);
int   uinet_sowritable(struct uinet_socket *so, unsigned int in_upcall);
int   uinet_soreceive(struct uinet_socket *so, struct uinet_sockaddr **psa, struct uinet_uio *uio, int *flagsp);
int uinet_soreceivemsg(struct uinet_socket *so, struct uinet_msghdr *msg,int *flagsp,ssize_t *rcvd_len);
void  uinet_sosetnonblocking(struct uinet_socket *so, unsigned int nonblocking);
int   uinet_sosetsockopt(struct uinet_socket *so, int level, int optname, void *optval, unsigned int optlen);
void  uinet_sosetupcallprep(struct uinet_socket *so,
			    void (*soup_accept)(struct uinet_socket *, void *), void *soup_accept_arg,
			    void (*soup_receive)(struct uinet_socket *, void *, int64_t, int64_t), void *soup_receive_arg,
			    void (*soup_send)(struct uinet_socket *, void *, int64_t), void *soup_send_arg);
void  uinet_sosetuserctx(struct uinet_socket *so, int key, void *ctx);
int   uinet_sosend(struct uinet_socket *so, struct uinet_sockaddr *addr, struct uinet_uio *uio, int flags);
int uinet_sosendmsg(struct uinet_socket *so, struct uinet_msghdr *mp,int flags,ssize_t *len);
int   uinet_soshutdown(struct uinet_socket *so, int how);
int   uinet_sogetpeeraddr(struct uinet_socket *so, struct uinet_sockaddr **sa);
int   uinet_sogetsockaddr(struct uinet_socket *so, struct uinet_sockaddr **sa);
void  uinet_free_sockaddr(struct uinet_sockaddr *sa);
void  uinet_soupcall_lock(struct uinet_socket *so, int which);
void  uinet_soupcall_clear(struct uinet_socket *so, int which);
void  uinet_soupcall_clear_locked(struct uinet_socket *so, int which);
void  uinet_soupcall_set(struct uinet_socket *so, int which, int (*func)(struct uinet_socket *, void *, int), void *arg);
void  uinet_soupcall_set_locked(struct uinet_socket *so, int which, int (*func)(struct uinet_socket *, void *, int), void *arg);
void  uinet_soupcall_unlock(struct uinet_socket *so, int which);
int   uinet_sysctlbyname(uinet_instance_t uinst, const char *name, char *oldp, size_t *oldplen,
			 const char *newp, size_t newplen, size_t *retval, int flags);
int   uinet_sysctl(uinet_instance_t uinst, const int *name, u_int namelen, void *oldp, size_t *oldplen,
		   const void *newp, size_t newplen, size_t *retval, int flags);
void  uinet_synfilter_getconninfo(uinet_api_synfilter_cookie_t cookie, struct uinet_in_conninfo *inc);
void  uinet_synfilter_getl2info(uinet_api_synfilter_cookie_t cookie, struct uinet_in_l2info *l2i);
void  uinet_synfilter_setl2info(uinet_api_synfilter_cookie_t cookie, struct uinet_in_l2info *l2i);
void  uinet_synfilter_setaltfib(uinet_api_synfilter_cookie_t cookie, unsigned int altfib);
void  uinet_synfilter_go_active_on_timeout(uinet_api_synfilter_cookie_t cookie, unsigned int ms);
int   uinet_synfilter_install(struct uinet_socket *so, uinet_api_synfilter_callback_t callback, void *arg);
uinet_synf_deferral_t uinet_synfilter_deferral_alloc(struct uinet_socket *so, uinet_api_synfilter_cookie_t cookie);
int   uinet_synfilter_deferral_deliver(struct uinet_socket *so, uinet_synf_deferral_t deferral, int decision);
void  uinet_synfilter_deferral_free(uinet_synf_deferral_t deferral);
uinet_api_synfilter_cookie_t uinet_synfilter_deferral_get_cookie(uinet_synf_deferral_t deferral);
int uinet_register_pfil_in(uinet_instance_t uinst, uinet_pfil_cb_t cb, void *arg, const char *ifname);

const char * uinet_mbuf_data(const struct uinet_mbuf *);
size_t uinet_mbuf_len(const struct uinet_mbuf *);
int uinet_if_xmit(uinet_if_t uif, const char *buf, int len);

int uinet_lock_log_set_file(const char *file);
int uinet_lock_log_enable(void);
int uinet_lock_log_disable(void);

void uinet_packet_receive_handler(struct uinet_instance *uinst,uint8_t *buf, unsigned int size);
void   uinet_set_transmit_cback(void (*transmit_cback)(void *userdata,uint8_t *pkt, uint32_t len));
uint16_t uinet_sogetprotocol(struct uinet_socket *so);

int
uinet_packet_send_handler(struct uinet_socket *so, uint8_t *buf, unsigned int size,struct uinet_sockaddr *uso_addr, int flags);

void uinet_set_callout_context(void *callout);
void uinet_callout_tick(void *);

void *uinet_alloc_callout(void);
void uinet_set_callout_cback(void *(*callout_cback)(void));
void *uinet_alloc_curthread(void);
void uinet_set_curthread_cback(void *(*curthread_cback)(void));

int uinet_sononblocking(struct uinet_socket *so);
/*
 *  Create a network interface with the given name, of the given type, in
 *  the given connection domain, and bound to the given cpu.
 *
 *  type	is the type of interface to create.  This determines the
 *		interface driver that will attach to the given configstr.
 *
 *  configstr	is a driver-specific configuration string.
 *
 *  		UINET_IFTYPE_NETMAP - vale<n>:<m> or <hostifname> or
 *  		    <hostifname>:<qno>, where queue 0 is implied by
 *		    a configstr of <hostifname>
 *
 *  alias	is any user-supplied string, or NULL.  If a string is supplied,
 *	        it must be unique among all the other aliases and driver-assigned
 *		names.  Passing an empty string is the same as passing NULL.
 *
 *  cdom	is the connection domain for ifname.  When looking up an
 *		inbound packet on ifname, only protocol control blocks in
 *		the same connection domain will be searched.
 *
 *  cpu		is the cpu number on which to perform stack processing on
 *		packets received on ifname.  -1 means leave it up to the
 *		scheduler.
 *
 *  cookie	is a pointer to an opaque reference that, if not NULL, will be
 *		set to something that corresponds to the interface that is
 *		created, or NULL if creation fails.
 *
 *
 *  Return values:
 *
 *  0			Interface created successfully
 *
 *  UINET_ENXIO		Unable to configure the inteface
 *
 *  UINET_ENOMEM	No memory available for interface creation
 *
 *  UINET_EEXIST	An interface with the given name or cdom already exists
 *
 *  UINET_EINVAL	Malformed ifname, or cpu not in range [-1, num_cpu-1]
 */
int uinet_ifcreate(uinet_instance_t uinst, uinet_iftype_t type, const char *configstr,
		   const char *alias, unsigned int cdom, int cpu, uinet_if_t *uif);


/*
 *  Destroy the network interface specified by the cookie.
 *
 *
 *  Return values:
 *
 *  0			Interface destroyed successfully
 *
 *  UINET_ENXIO		Unable to destroy the interface
 *
 *  UINET_EINVAL	Invalid cookie
 */
int uinet_ifdestroy(uinet_if_t uif);


/*
 *  Destroy the network interface with the given name.
 *
 *  name	can be either the user-specified alias, or the driver-assigned
 *		name returned by uinet_ifgenericname().
 *
 *  Return values:
 *
 *  0			Interface destroyed successfully
 *
 *  UINET_ENXIO		Unable to destroy the interface
 *
 *  UINET_EINVAL	No interface with the given name found
 */
int uinet_ifdestroy_byname(uinet_instance_t uinst, const char *ifname);


/*
 *  Retrieve the user-assigned alias or driver-assigned generic name for the
 *  interface specified by cookie.
 *
 *
 *  Return values:
 *
 *  ""			No alias was assigned or cookie was invalid.
 *
 *  <non-empty string>	The alias or driver-assigned name
 *
 */
const char *uinet_ifaliasname(uinet_if_t uif);
const char *uinet_ifgenericname(uinet_if_t uif);


/*
 *  Configure UDP and TCP blackholing.
 */
int uinet_config_blackhole(uinet_instance_t uinst, uinet_blackhole_t action);


void uinet_instance_default_cfg(struct uinet_instance_cfg *cfg);
uinet_instance_t uinet_instance_create(struct uinet_instance_cfg *cfg);
uinet_instance_t uinet_instance_default(void);
void uinet_instance_destroy(uinet_instance_t uinst);

#define UINET_BATCH_EVENT_START  0
#define UINET_BATCH_EVENT_FINISH 1

int uinet_if_set_batch_event_handler(uinet_if_t uif,
				     void (*handler)(void *arg, int event),
				     void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _UINET_API_H_ */
