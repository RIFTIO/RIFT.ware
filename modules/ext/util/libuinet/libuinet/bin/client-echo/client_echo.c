 /* Copyright (c) 2014 Patrick Kelsey. All rights reserved.
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

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include "uinet_api.h"

#define EV_STANDALONE 1
#define EV_UINET_ENABLE 1
#include <ev.h>


struct connection_context {
	ev_uinet watcher;
};


struct echo_context {
	struct ev_loop *loop;
	unsigned int listen_cdom;
	struct uinet_socket *listener;
	ev_uinet listen_watcher;
	int verbose;
};

struct interface_config {
	char *ifname;
	char alias[UINET_IF_NAMESIZE];
	unsigned int cdom;
	int thread_create_result;
	pthread_t thread;
	struct ev_loop *loop;
	int promisc;
	int type;
	int instance;
	char *alias_prefix;
};

struct client_config {
	char *listen_addr;
	int listen_port;
	char *peer_addr;
	int peer_port;
	struct interface_config *interface;
	int verbose;
	struct echo_context *echo;
	int addrany;
};


static __inline int imin(int a, int b) { return (a < b ? a : b); }

static void
echo_cb(struct ev_loop *loop, ev_uinet *w, int revents)
{
#define BUFFER_SIZE (64*1024)
	char buffer[BUFFER_SIZE];
	struct uinet_iovec iov;
	struct uinet_uio uio;
	int max_read;
	int max_write;
	int read_size;
	int error;

	max_read = uinet_soreadable(w->so, 0);
	if (max_read <= 0) {
		/* the watcher should never be invoked if there is no error and there no bytes to be read */
		assert(max_read != 0);
		printf("can't read, closing\n");
		goto err;
	} else {
		max_write = uinet_sowritable(w->so, 0);
		if (-1 == max_write) {
			printf("can't write, closing\n");
			goto err;
		} else {
			read_size = imin(imin(max_read, max_write), BUFFER_SIZE);

			uio.uio_iov = &iov;
			iov.iov_base = buffer;
			iov.iov_len = read_size;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_resid = read_size;
	
			error = uinet_soreceive(w->so, NULL, &uio, NULL);
			if (0 != error) {
				printf("read error (%d), closing\n", error);
				goto err;
			}

			assert(uio.uio_resid == 0);

			uio.uio_iov = &iov;
			iov.iov_base = buffer;
			iov.iov_len = read_size;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_resid = read_size;
			error = uinet_sosend(w->so, NULL, &uio, 0);
			if (0 != error) {
				printf("write error (%d), closing\n", error);
				goto err;
			}

			if (max_write < max_read) {
				/* limited by write space, so change to a
				 * write watch on the socket, if we aren't
				 * already one.
				 */
				if (w->events & EV_READ) {
					ev_uinet_stop(loop, w);
					w->events = EV_WRITE;
					ev_uinet_start(loop, w);
				}
				/* else, continue as a write watch */
			} else if (!(w->events & EV_READ)) {
				/* write space wasn't a limitation this
				 * time, so switch back to waiting on
				 * EV_READ
				 */
				ev_uinet_stop(loop, w);
				w->events = EV_READ;
				ev_uinet_start(loop, w);
			}
			/* else, continue as a read watcher */
		}
	}

	return;

err:
	ev_uinet_stop(loop, w);
	uinet_soclose(w->so);
}


static void
outbound_connect_cb(struct ev_loop *loop, ev_uinet *w, int revents)
{
	struct echo_context *echo = w->data;
	struct ev_uinet_ctx *soctx = NULL;
	int error;

	ev_uinet_stop(loop,w);


	if (uinet_sowritable(w->so, 0) >= 0) {
		printf("conect cb .. connection  succeeded\n");
		
		ev_init(w, echo_cb);
   	 	w->events = EV_READ;
		ev_uinet_start(loop, w);
	}
	else {
		int error;
		perror("Connect failed");
		error = uinet_sogeterror(w->so);
		printf("Error is %d",error);
		goto fail;
		

	}

	return;
fail:
	if (soctx) ev_uinet_detach(soctx);
}


static struct echo_context *
create_echo(struct ev_loop *loop, struct client_config *cfg)
{
	struct echo_context *echo = NULL;
	struct uinet_socket *listener = NULL;
	struct ev_uinet_ctx *soctx = NULL;
	struct uinet_in_addr addr;
	struct uinet_in_addr peer_addr;
	int error;
	struct uinet_sockaddr_in sin;

	if (uinet_inet_pton(UINET_AF_INET, cfg->listen_addr, &addr) <= 0) {
		printf("Malformed address %s\n", cfg->listen_addr);
		goto fail;
	}
	if (uinet_inet_pton(UINET_AF_INET, cfg->peer_addr, &peer_addr) <= 0) {
		printf("Malformed address %s\n", cfg->peer_addr);
		goto fail;
	}

	error = uinet_socreate(UINET_PF_INET, &listener, UINET_SOCK_STREAM, 0);
	if (0 != error) {
		printf("Listen socket creation failed (%d)\n", error);
		goto fail;
	}

	soctx = ev_uinet_attach(listener);
	if (NULL == soctx) {
		printf("Failed to alloc libev socket context\n");
		goto fail;
	}

	if (cfg->interface->promisc) {
		if ((error = uinet_make_socket_promiscuous(listener, cfg->interface->cdom))) {
			printf("Failed to make listen socket promiscuous (%d)\n", error);
			goto fail;
		}
	}

	uinet_sosetnonblocking(listener, 1);

	echo = malloc(sizeof(struct echo_context));
	if (NULL == echo) {
		goto fail;
	}


	/* Listen on all VLANs */
	if ((error = uinet_setl2info2(listener, NULL, NULL, UINET_INL2I_TAG_ANY, NULL))) {
		printf("Listen socket L2 info set failed (%d)\n", error);
		goto fail;
	}

	echo->loop = loop;
	echo->listener = listener;
	echo->verbose = cfg->verbose;
	echo->listen_cdom = cfg->interface->cdom;

	memset(&sin, 0, sizeof(struct uinet_sockaddr_in));
	sin.sin_len = sizeof(struct uinet_sockaddr_in);
	sin.sin_family = UINET_AF_INET;
	sin.sin_addr = addr;
	sin.sin_port = htons(cfg->listen_port);
	error = uinet_sobind(listener, (struct uinet_sockaddr *)&sin);
	if (0 != error) {
		printf("bind failed\n");
		goto fail;
	}


	memset(&sin, 0, sizeof(struct uinet_sockaddr_in));
	sin.sin_len = sizeof(struct uinet_sockaddr_in);
	sin.sin_family = UINET_AF_INET;
	sin.sin_addr = peer_addr;
	sin.sin_port = htons(cfg->peer_port);
	error = uinet_soconnect(listener, (struct uinet_sockaddr *)&sin);	
	if (error) {
		if (UINET_EINPROGRESS == error) {
			error = 0;
		} else {
			printf("Connect to failed (%d)\n", error);
			goto fail;
		}
	}

	if (echo->verbose) {
		char buf[32];

		printf("Listening on %s:%u\n", uinet_inet_ntoa(addr, buf, sizeof(buf)), cfg->listen_port);
	}

	ev_init(&echo->listen_watcher, outbound_connect_cb);
	ev_uinet_set(&echo->listen_watcher, soctx, EV_WRITE);
	echo->listen_watcher.data = echo;
	ev_uinet_start(loop, &echo->listen_watcher);

	return (echo);

fail:
	if (soctx) ev_uinet_detach(soctx);
	if (listener) uinet_soclose(listener);
	if (echo) free(echo);

	return (NULL);
}


void *interface_thread_start(void *arg)
{
	struct interface_config *cfg = arg;

	uinet_initialize_thread();

	ev_run(cfg->loop, 0);

	uinet_finalize_thread();

	return (NULL);
}


static void
usage(const char *progname)
{

	printf("Usage: %s [options]\n", progname);
	printf("    -h                   show usage\n");
	printf("    -i ifname            specify network interface\n");
	printf("    -l inaddr            listen address\n");
	printf("    -P                   put interface into Promiscuous INET mode\n");
	printf("    -p port              listen port [0, 65535]\n");
	printf("    -r port              peer port [0, 65535]\n");
	printf("    -f peer address      peer port [0, 65535]\n");

	printf("    -v                   be verbose\n");
}


int main (int argc, char **argv)
{
	int ch;
	char *progname = argv[0];
#define MIN_INTERFACES 1
#define MAX_INTERFACES 64
	struct interface_config interfaces[MAX_INTERFACES];
#define MIN_SERVERS 1
#define MAX_SERVERS 64	
	struct client_config clients[MAX_SERVERS];
	int num_interfaces = 0;
	int num_clients = 0;
	int interface_client_count = 0;
	int verbose = 0;
	unsigned int i;
	int error;
	struct uinet_in_addr tmpinaddr;
	int ifpcap_count = 0;

	memset(interfaces, 0, sizeof(interfaces));
	memset(clients, 0, sizeof(clients));

#if 0
	for (i = 0; i < MAX_INTERFACES; i++) {
		interfaces[i].type = UINET_IFTYPE_NETMAP;
	}
#endif

	for (i = 0; i < MAX_SERVERS; i++) {
		clients[i].listen_port = -1;
	}

	while ((ch = getopt(argc, argv, "hi:f:r:l:Pp:t:v")) != -1) {
		switch (ch) {
		case 'h':
			usage(progname);
			return (0);
		case 'i':
			if (MAX_INTERFACES == num_interfaces) {
				printf("Maximum number of interfaces is %u\n", MAX_INTERFACES);
				return (1);
			} else {
				interfaces[num_interfaces].ifname = optarg;
				interfaces[num_interfaces].cdom = num_interfaces + 1;
				num_interfaces++;
				interface_client_count = 0;
			}
			break;
		case 'l':
			if (0 == num_interfaces) {
				printf("No interface specified\n");
				return (1);
			} else if (MAX_SERVERS == num_clients) {
				printf("Maximum number of clients is %u\n", MAX_SERVERS);
				return (1);
			} else {
				clients[num_clients].listen_addr = optarg;
				clients[num_clients].interface = &interfaces[num_interfaces - 1];
				num_clients++;
				interface_client_count++;
			}
			break;
		case 'f':
			if (0 == num_interfaces) {
				printf("No interface specified\n");
				return (1);
			} else if (MAX_SERVERS == num_clients) {
				printf("Maximum number of clients is %u\n", MAX_SERVERS);
				return (1);
			} else {
				clients[num_clients].peer_addr = optarg;
			}
			break;
		case 'P':
			if (0 == num_interfaces) {
				printf("No interface specified\n");
				return (1);
			} else {
				interfaces[num_interfaces - 1].promisc = 1;
			}
			break;
		case 'p':
			if (0 == interface_client_count) {
				printf("No listen address specified\n");
				return (1);
			} else {
				clients[num_clients - 1].listen_port = strtoul(optarg, NULL, 10);
			}
			break;
		case 'r':
			if (0 == interface_client_count) {
				printf("No listen address specified\n");
				return (1);
			} else {
				clients[num_clients - 1].peer_port = strtoul(optarg, NULL, 10);
			}
			break;
		case 't':
			if (0 == num_interfaces) {
				printf("No interface specified\n");
				return (1);
			} else if (0 == strcmp(optarg, "pcap")) {
				interfaces[num_interfaces - 1].type = UINET_IFTYPE_PCAP;
			} else if(0 == strcmp(optarg, "raw")) { 
				interfaces[num_interfaces - 1].type = UINET_IFTYPE_RAW;
			}
			else {
				printf("Unknown interface type %s\n", optarg);
				return (1);
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
			printf("Unknown option \"%c\"\n", ch);
		case '?':
			usage(progname);
			return (1);
		}
	}
	argc -= optind;
	argv += optind;

	if (num_interfaces < MIN_INTERFACES) {
		printf("Specify at least %u interface%s\n", MIN_INTERFACES, MIN_INTERFACES == 1 ? "" : "s");
		return (1);
	}

	if (num_clients < MIN_SERVERS) {
		printf("Specify at least %u listen address%s\n", MIN_SERVERS, MIN_SERVERS == 1 ? "" : "es");
		return (1);
	}

	for (i = 0; i < num_clients; i++) {
		if (-1 == clients[i].listen_port) {
			printf("No listen port specified for interface %s, listen address %s\n",
			       clients[i].interface->ifname, clients[i].listen_addr);
			return (1);
		}

		if (clients[i].listen_port < 0 || clients[i].listen_port > 65535) {
			printf("Listen port for interface %s, listen address %s is out of range [0, 65535]\n",
			       clients[i].interface->ifname, clients[i].listen_addr);
			return (1);
		}

		if (0 == clients[i].listen_port)
			clients[i].interface->promisc = 1;

		if (uinet_inet_pton(UINET_AF_INET, clients[i].listen_addr, &tmpinaddr) <= 0) {
			printf("%s is not a valid listen address\n", clients[i].listen_addr);
			return (1);
		}

		if (tmpinaddr.s_addr == UINET_INADDR_ANY) {
			clients[i].addrany = 1;
			clients[i].interface->promisc = 1;
		}
	}
	
	
	uinet_init(1, 128*1024, 0);

	for (i = 0; i < num_interfaces; i++) {
		switch (interfaces[i].type) {
		case UINET_IFTYPE_PCAP:
			interfaces[i].alias_prefix = "pcap";
			interfaces[i].instance = ifpcap_count;
			ifpcap_count++;
			break;
		case UINET_IFTYPE_RAW:
			interfaces[i].alias_prefix = "raw";
			interfaces[i].instance = ifpcap_count;
			ifpcap_count++;
			break;
		default:
			printf("Unknown interface type %d\n", interfaces[i].type);
			return (1);
			break;
		}

		snprintf(interfaces[i].alias, UINET_IF_NAMESIZE, "%s%d", interfaces[i].alias_prefix, interfaces[i].instance);

		if (verbose) {
			printf("Creating interface %s, Promiscuous INET %s, cdom=%u\n",
			       interfaces[i].alias, interfaces[i].promisc ? "enabled" : "disabled",
			       interfaces[i].promisc ? interfaces[i].cdom : 0);
		}

		error = uinet_ifcreate(interfaces[i].type, interfaces[i].ifname, interfaces[i].alias,
				       interfaces[i].promisc ? interfaces[i].cdom : 0,
				       0, NULL);
		if (0 != error) {
			printf("Failed to create interface %s (%d)\n", interfaces[i].alias, error);
		}

		interfaces[i].loop = ev_loop_new(EVFLAG_AUTO);
		if (NULL == interfaces[i].loop) {
			printf("Failed to create event loop interface %s\n", interfaces[i].alias);
			break;
		}
		
	}
	
	for (i = 0; i < num_clients; i++) {
		if (!clients[i].addrany) {
			if (verbose) {
				printf("Adding address %s to interface %s\n", clients[i].listen_addr, clients[i].interface->alias);
			}
			
			error = uinet_interface_add_alias(clients[i].interface->alias, clients[i].listen_addr, "", "");
			if (error) {
				printf("Adding alias %s to interface %s failed (%d)\n", clients[i].listen_addr, clients[i].interface->alias, error);
			}
		}
	}

	for (i = 0; i < num_clients; i++) {
		if (verbose) {
			printf("Creating echo client at %s:%d on interface %s\n",
			       clients[i].listen_addr, clients[i].listen_port,
			       clients[i].interface->alias);
		}

		clients[i].verbose = verbose;

		clients[i].echo = create_echo(clients[i].interface->loop, &clients[i]);
		if (NULL == clients[i].echo) {
			printf("Failed to create echo client at %s:%d on interface %s\n",
			       clients[i].listen_addr, clients[i].listen_port,
			       clients[i].interface->alias);
			break;
		}
	}

	for (i = 0; i < num_interfaces; i++) {
		if (verbose) {
			printf("Bringing up interface %s\n", interfaces[i].alias);
		}

		error = uinet_interface_up(interfaces[i].alias, 1, interfaces[i].promisc);
		if (0 != error) {
			printf("Failed to bring up interface %s (%d)\n", interfaces[i].alias, error);
		}

		if (verbose)
			printf("Creating interface thread for interface %s\n", interfaces[i].alias);

		interfaces[i].thread_create_result = pthread_create(&interfaces[i].thread, NULL,
								    interface_thread_start, &interfaces[i]);
	}

	for (i = 0; i < num_interfaces; i++) {
		if (0 == interfaces[i].thread_create_result)
			pthread_join(interfaces[i].thread, NULL);
	}

	for (i = 0; i < num_interfaces; i++) {
		uinet_ifdestroy_byname(interfaces[i].ifname);
	}

	return (0);
}
