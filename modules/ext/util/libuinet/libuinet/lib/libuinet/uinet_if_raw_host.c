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

#if defined(__linux__)
/*
 * To expose required facilities in net/if.h.
 */
#define _GNU_SOURCE
#endif /* __linux__ */

#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "uinet_host_interface.h"
#include "uinet_if_raw_host.h"


struct if_raw_host_context {
        int fd;
	const char *ifname;
	if_raw_handler pkthandler;
	void *pkthandlerarg;
	uint64_t last_packet_delivery;
	uint64_t last_packet_timestamp;		
	//char errbuf[PCAP_ERRBUF_SIZE];
};


struct if_raw_host_context *
if_raw_create_handle(const char *ifname, unsigned int isfile, if_raw_handler handler, void *handlerarg)
{
	struct if_raw_host_context *ctx;
	//int dlt;

	ctx = calloc(1, sizeof(*ctx));
	if (NULL == ctx)
		goto fail;

	ctx->fd = socket (AF_INET, SOCK_RAW,IPPROTO_TCP);
	//printf("In func %s fd is %d\n",__func__,ctx->fd);

	if(ctx->fd == -1) {
		perror("socket() failed");
	}

	{				/* lets do it the ugly way.. */
		int one = 1;
		const int *val = &one;
		if (setsockopt (ctx->fd, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0)
		       printf ("Warning: Cannot set HDRINCL!\n");
	}

	ctx->pkthandler = handler;
	ctx->pkthandlerarg = handlerarg;

	return ctx;
fail:
	if (ctx)
		free(ctx);

	return (NULL);
}


void
if_raw_destroy_handle(struct if_raw_host_context *ctx)
{
	close(ctx->fd);
	free(ctx);
}

void calc_proto_cksum(char *buf,unsigned int size) 
{
  struct ip *iphdr;
  struct tcphdr *tcp = NULL;
  struct udphdr *udp = NULL;
  uint32_t rcvd_cksum;
  uint32_t cksum=0;
  int i=0;
  int off=0, l4_len = 0;
  int pad = 0;

  iphdr = (struct ip *)(buf);
  if(iphdr->ip_p == IPPROTO_UDP) {
    udp = (struct udphdr *)(iphdr+1);
    udp->check = 0;
  }
  else if(iphdr->ip_p == IPPROTO_TCP) {
    tcp =  (struct tcphdr *) (iphdr+1);
    tcp->check = 0;	
  }
  else {
    return;
  }

  rcvd_cksum = ((ntohl(iphdr->ip_dst.s_addr))&0xFFFF) + (((ntohl(iphdr->ip_dst.s_addr))>>16)&0xFFFF) +
               ((ntohl(iphdr->ip_src.s_addr))&0xFFFF) + (((ntohl(iphdr->ip_src.s_addr))>>16)&0xFFFF) +
                iphdr->ip_p + ntohs(iphdr->ip_len) - sizeof(struct ip);
  //printf("Rcvd cksum is %x\n",rcvd_cksum);	

  if(size&1) {
	  buf[size] = 0;
	  pad = 1;
  }
  off = sizeof(*iphdr);
  l4_len = size-sizeof(struct ip);
  for (i=0;i<l4_len+pad;i=i+2) {
	  cksum +=  (((buf[off+i]<<8)&0xFF00)+(buf[off+i+1]&0xFF));
  }
  cksum+=rcvd_cksum;

  while (cksum>>16)
	  cksum = (cksum & 0xFFFF)+(cksum >> 16);
  cksum = ~cksum;
  if(udp) {
    udp->check = htons(cksum);
  }
  else if(tcp) {
    tcp->check = htons(cksum);
  }
  return;
  //printf("checksum value computed is %x\n",cksum);
}

int
if_raw_sendpacket(struct if_raw_host_context *ctx, uint8_t *buf, unsigned int size)
{
  struct sockaddr_in sin;
  struct ip *iphdr;
#if 0
  struct tcphdr *tcp = NULL;
  uint32_t rcvd_cksum;
  uint32_t cksum=0;
  int i=0;
  int off=0, tcp_len = 0;
  int pad = 0;
#endif

  calc_proto_cksum(buf,size);


  iphdr = (struct ip *)(buf);
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr =  inet_addr("127.0.0.1");

#if 0	
   // Calculate the checksum
   printf("Calculating the chcksum\n");
  tcp =  (struct tcphdr *) (iphdr+1);
//  cksum = tcp->th_sum;
  rcvd_cksum = ntohs(tcp->check);
  if(size&1) {
	  buf[size] = 0;
	  pad = 1;
  }
  tcp->check = 0;	
  off = sizeof(*iphdr);
  tcp_len = size-sizeof(struct iphdr);
  for (i=0;i<tcp_len+pad;i=i+2) {
	  cksum +=  (((buf[off+i]<<8)&0xFF00)+(buf[off+i+1]&0xFF));
  }
	cksum+=rcvd_cksum;

	  while (cksum>>16)
		  cksum = (cksum & 0xFFFF)+(cksum >> 16);
	  cksum = ~cksum;
	  printf("checksum value computed is %x\n",cksum);
	tcp->check = htons(cksum);
#endif

  sin.sin_addr.s_addr = iphdr->ip_dst.s_addr;
  //printf("Send address is %x \n",sin.sin_addr.s_addr);

	if (sendto (ctx->fd,		/* our socket */
		    buf,	/* the buffer containing headers and data */
		    size,	/* total length of our datagram */
	  	    0,		/* routing flags, normally always 0 */
			//NULL,0) <0)
	        	(struct sockaddr *) &sin,	/* socket addr, just like in */
				sizeof (sin)) < 0)		/* a normal send() */
		printf ("error\n");
	else
		printf (".");
	return size;
}




#if 0
static void
if_raw_packet_handler(struct if_raw_host_context *ctx, const struct raw_pkthdr *pkthdr, const unsigned char *pkt)
{
	ctx->pkthandler(ctx->pkthandlerarg, pkt, pkthdr->caplen);
}
#endif

int
if_raw_loop(struct if_raw_host_context *ctx)
{
	char buf[2048];
	struct sockaddr from;
	socklen_t fromlen;
	//int cnt = 0;
	int msg_len = 0;
        struct pollfd pollset[1];
	//struct iphdr *ip;
	

	memset(pollset,0,sizeof(pollset));
	pollset[0].fd = ctx->fd;
	pollset[0].events = POLLIN;
	//printf("Suresh: doing poll in fn %s\n",__func__);

	while(1) {
		if( poll(pollset, 1,-1) > 0) { 
			//printf("Suresh: returned from poll in fn %s, revents: %x, events: %x\n",__func__, pollset[0].revents, pollset[0].events);

			msg_len = recvfrom (ctx->fd, &buf[0], IP_MAXPACKET, 0, (struct sockaddr *) &from, &fromlen);

			if(msg_len == -1) {
				printf("Error while receiving\n");
			}
			else {

				ctx->pkthandler(ctx->pkthandlerarg, buf, msg_len);
			}
		}
	}
	memset(pollset,0,sizeof(pollset));
	pollset[0].fd = ctx->fd;
	pollset[0].events = POLLIN;
	return 0;
}


