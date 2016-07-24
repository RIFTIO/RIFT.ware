/* STANDARD_RIFT_IO_COPYRIGHT */                                                                                                         
/**
 * @file print-rw-vfabric.c
 * @author Sanil Puthiyandyil
 * @date 8/29/2014
 * @Riftware Virtual fabric packet print functions
 *
 * @details
 */


/*
 * Copyright (c) 1991, 1993, 1994, 1995, 1996, 1997
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

//#ifndef lint
//static const char rcsid[] _U_ =
//    "@(#) $Header: /tcpdump/master/tcpdump/print-rw-vfabric.c,v 1.20 2014-08-29 $";
//#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>

#include "rw-vfabric.h"
#include "interface.h"
#include "extract.h"

#include "ip.h"


static char tstr[] = " [|rwvfabric]";

static const struct tok vf_msgexttype2str[] = {
	{ RW_VFABRIC_EXT_H_PADDING, 	  "VF-EXT-PADDING" },
	{ RW_VFABRIC_EXT_H_IN_PDU_INFO,	"VF-EXT-INPDU-INFO" },
	{ RW_VFABRIC_EXT_H_PORT_TX,	    "VF-EXT-PORT-TX" },
	{ RW_VFABRIC_EXT_H_FWD_TX,	    "VF-EXT-POLICY-FWD" },
	{ RW_VFABRIC_EXT_H_IN_LOOKUP,	  "VF-EXT-IN-LOOKUP" },
	{ RW_VFABRIC_EXT_H_DLVR_APP,	  "VF-EXT-DLVR-APP" },
	{ RW_VFABRIC_EXT_H_DLVR_KERN,	  "VF-EXT-DLVR-KERN" },
	{ RW_VFABRIC_EXT_H_IP_REASM,	  "VF-EXT-IP-REASM" },
	{ RW_VFABRIC_EXT_H_ECHO,	      "VF-EXT-ECHO" },
	{ 0,			                      NULL }
};

static const struct tok vf_payloadtype2str[] = {
	{ RW_VFABRIC_PAYLOAD_ETH,		"ETHERNET" },
	{ RW_VFABRIC_PAYLOAD_IPV4,	"IPv4" },
	{ RW_VFABRIC_PAYLOAD_IPV6,	"IPv6" },
	{ 0,				                NULL }
};

static const struct tok vf_encap2str[] = {
	{ RW_VFABRIC_PLOAD_ENCAP_NONE,	    "None" },
	{ RW_VFABRIC_PLOAD_ENCAP_8021PQ,	  "802.1.PQ" },
	{ RW_VFABRIC_PLOAD_ENCAP_8021AD,	  "802.1.AD" },
	{ RW_VFABRIC_PLOAD_ENCAP_MPLS_STACK,"MPLS-STACK" },
	{ 0,				                         NULL }
};

static const struct tok vf_policyfwd2str[] = {
  { RW_VFABRIC_FWD_L3_SIMPLE,      "L3-SIMPLE" },
  { RW_VFABRIC_FWD_L3_NEXTHOP,     "L3-NEXTHOP" },
  { RW_VFABRIC_FWD_VRF,            "VRF" },
  { RW_VFABRIC_FWD_MPLS,           "MPLS" },
  { RW_VFABRIC_FWD_VLAN,           "VLAN" },
  { RW_VFABRIC_FWD_TUN,            "TUNNEL" },
  { 0,                             NULL }
};

void
rw_vf_print(const u_char *dat, u_int length)
{
	const u_char *ptr = dat;
	u_int cnt = 0;			/* total octets consumed */
  u_int8_t total_ext_len, ext_type, ext_len, payload;


  printf("RW-VIRTUAL-FABRIC: ");

	TCHECK2(*ptr, 1);	/*  Version & TTL */
  printf("ver: %2u, ", RW_VFABRIC_GET_VERSION(*ptr));
  printf("ttl: %2u, ", RW_VFABRIC_GET_TTL(*ptr));
	ptr += 1;
	cnt += 1;

	TCHECK2(*ptr, 1);	/*  tos */
  printf("tos: 0x%x, ", *ptr);
	ptr += 1;
	cnt += 1;

	TCHECK2(*ptr, 1);	/*  pathid */
  printf("pathid: %2u, ", *ptr);
	ptr += 1;
	cnt += 1;

	TCHECK2(*ptr, 1);	/*  quick-flags */
  printf("quick-flags: 0x%x, ", *ptr);
	ptr += 1;
	cnt += 1;

	TCHECK2(*ptr, 4);	/* src-vfapid */
  printf("src vfapid: %u, ", EXTRACT_32BITS(ptr));
	ptr += 4;
	cnt += 4;

	TCHECK2(*ptr, 4);	/* dst-vfapid */
  printf("dst vfapid: %u, ", EXTRACT_32BITS(ptr));
	ptr += 4;
	cnt += 4;

	TCHECK2(*ptr, 2);	/* payload len */
  printf("payload len: %u, ", EXTRACT_16BITS(ptr));
	ptr += 2;
	cnt += 2;

	TCHECK2(*ptr, 1);	/* payload type */
  printf("payload type: %s, ", tok2str(vf_payloadtype2str, "PAYLOAD-TYPE#%u", *ptr));
  payload = *ptr;
	ptr += 1;
	cnt += 1;

	TCHECK2(*ptr, 1);	/* ext len */
  printf("total ext len: %u\n", *ptr);
  total_ext_len = *ptr;
	ptr += 1;
	cnt += 1;
  
  while (total_ext_len >= 2) {
    TCHECK2(*ptr, 1); /* ext_type */                                                                             
    printf("  ext type: %s, ", tok2str(vf_msgexttype2str, "EXT-TYPE#%u", *ptr));
    ext_type = *ptr;
    ptr += 1;
    cnt += 1;

    TCHECK2(*ptr, 1); /* ext_len */                                                                             
    printf("ext len: %u, ", *ptr);
    ext_len = *ptr;
    ptr += 1;
    cnt += 1;

    switch (ext_type) {
      case RW_VFABRIC_EXT_H_PADDING:
        TCHECK2(*ptr, ext_len); /* padding */                                                                             
        ptr += ext_len;
        cnt += ext_len;
        break;

      case RW_VFABRIC_EXT_H_IN_PDU_INFO:
	      TCHECK2(*ptr, 1);	/* encap type */
        printf("payload type: %s, ", tok2str(vf_encap2str, "ENCAP-TYPE#%u", *ptr));
	      ptr += 1;
	      cnt += 1;
        
	      TCHECK2(*ptr, 1);	/* l3_offset */
        printf("l3 ofset: %u, " , *ptr);
	      ptr += 1;
	      cnt += 1;

	      TCHECK2(*ptr, 4);	/* ncid */
        printf("ncid: %u, " , EXTRACT_32BITS(ptr));
	      ptr += 4;
	      cnt += 4;

	      TCHECK2(*ptr, 4);	/* lportid */
        printf("lportid: %u, " , EXTRACT_32BITS(ptr));
	      ptr += 4;
	      cnt += 4;

	      TCHECK2(*ptr, 4);	/* portmeta */
        printf("portmeta: %u, " , EXTRACT_32BITS(ptr));
	      ptr += 4;
	      cnt += 4;
        /* PDU */
        break;

      case RW_VFABRIC_EXT_H_PORT_TX:
	      TCHECK2(*ptr, 2);	/* pad */
	      ptr += 2;
	      cnt += 2;

	      TCHECK2(*ptr, 4);	/* lportid */
        printf("lportid: %u, " , EXTRACT_32BITS(ptr));
	      ptr += 4;
	      cnt += 4;
        /* PDU */
        goto printvfpdu;
        return;

      case RW_VFABRIC_EXT_H_FWD_TX:
	      TCHECK2(*ptr, 1);	/* fwd type */
        printf("policy fwd: %s, ", tok2str(vf_policyfwd2str, "FWD-TYPE#%u", *ptr));
	      ptr += 1;
	      cnt += 1;

	      TCHECK2(*ptr, 1);	/* pad */
	      ptr += 1;
	      cnt += 1;

	      TCHECK2(*ptr, 4);	/* ncid */
        printf("ncid: %u, " , EXTRACT_32BITS(ptr));
	      ptr += 4;
	      cnt += 4;

        /* PDU */
        goto printvfpdu;
        return;

      case RW_VFABRIC_EXT_H_IN_LOOKUP:
      case RW_VFABRIC_EXT_H_IP_REASM:
        TCHECK2(*ptr, 2); /* pad */
        ptr += 2;
        cnt += 2;

        TCHECK2(*ptr, 4); /* vsapid */                                                                                        
        printf("working-signature: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;
        /* PDU */
        goto printvfpdu;
        return;

      case RW_VFABRIC_EXT_H_DLVR_APP:
        TCHECK2(*ptr, 2); /* pad */
        ptr += 2;
        cnt += 2;

        TCHECK2(*ptr, 4); /* vsapid */                                                                                        
        printf("vsapid: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        TCHECK2(*ptr, 4); /* vsapid handle*/                                                                   
        printf("vsap handle: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        TCHECK2(*ptr, 4); /* flowid msw*/                                                                   
        printf("flowid msw: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        TCHECK2(*ptr, 4); /* flowid lsw*/                                                                   
        printf("flowid lsw: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        /* PDU */
        goto printvfpdu;
        return;

      case RW_VFABRIC_EXT_H_DLVR_KERN:
        TCHECK2(*ptr, 2); /* pad */
        ptr += 2;
        cnt += 2;

        TCHECK2(*ptr, 4); /* vsapid */                                                                                        
        printf("vsapid: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;
        /* PDU */
        goto printvfpdu;
       return;

      case RW_VFABRIC_EXT_H_ECHO:
        TCHECK2(*ptr, 1); /* sub-type */
        if (*ptr == RW_VFABRIC_ECHO_REQUEST) {
          printf("sub-type: Echo-Request, ");
        }
        else 
        if (*ptr == RW_VFABRIC_ECHO_REQUEST) {
          printf("sub-type: Echo-Response, ");
        }
        else {
          printf("sub-type: Unknown, ");
        }
        ptr += 1;
        cnt += 1;
      
        TCHECK2(*ptr, 1); /* pad */
        ptr += 1;
        cnt += 1;

        TCHECK2(*ptr, 4); /* id */                                                                   
        printf("identifier: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        TCHECK2(*ptr, 4); /* ts msw */                                                                   
        printf("timestamp msw: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;

        TCHECK2(*ptr, 4); /* ts lsw */                                                                   
        printf("timestamp msw: %u, " , EXTRACT_32BITS(ptr));
        ptr += 4;
        cnt += 4;
        return;

       default:
        TCHECK2(*ptr, ext_len); /* padding */                                                                             
        ptr += ext_len;
        cnt += ext_len;
        break;
    }
    if (total_ext_len > (ext_len + 2)) {
      total_ext_len -= (ext_len + 2);
    }
    else {
     total_ext_len = 0;
    }
  }
  return;


 printvfpdu:
  switch(payload) {
    case RW_VFABRIC_PAYLOAD_ETH:
       ether_print(gndo, ptr, length - cnt, length - cnt, NULL, NULL);
       break;
    case RW_VFABRIC_PAYLOAD_IPV4:
      ip_print(gndo, ptr, length - cnt);
      break;
#ifdef INET6
    case RW_VFABRIC_PAYLOAD_IPV6:
      ip6_print(gndo, ptr, length - cnt);
      break;
#endif
    default:
      break;
  }

	return;

 trunc:
	printf("%s", tstr);
}
