
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_signature.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG signature object
 */

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/types.h>

#include <unistd.h>
#include <sys/syscall.h>

#include <rwlib.h>
#include <rw_rand.h>

#include "rwmsg_int.h"

rwmsg_signature_t *rwmsg_signature_create(rwmsg_endpoint_t *ep,
					  rwmsg_payfmt_t payfmt,
					  uint32_t methno,
					  rwmsg_priority_t pri) {
  rwmsg_signature_t *sig;
  sig = (rwmsg_signature_t*)RW_MALLOC0_TYPE(sizeof(*sig), rwmsg_signature_t);
  sig->pri = pri;
  sig->payt = payfmt;
  sig->methno = methno;

  /* Lookup default timeout given the request priority value */ 
  int32_t defto = 0;
  char ppath[512];
  sprintf(ppath, "/rwmsg/signature/defto/%d", (int)pri);
  if (RW_STATUS_SUCCESS != rwmsg_endpoint_get_property_int(ep, ppath, &defto)
      || defto <= 0) {
    defto = 60*100;		/* 60s, 6000cs */
  }
  sig->timeo = (uint32_t)defto;

  switch (payfmt) {
  case RWMSG_PAYFMT_RAW:
  case RWMSG_PAYFMT_TEXT:
  case RWMSG_PAYFMT_PBAPI:
    break;
  default:
    RW_CRASH();
    break;
  }

  ck_pr_inc_32(&ep->stat.objects.signatures);
  sig->refct = 1;
  return sig;
}

void rwmsg_signature_set_timeout(rwmsg_signature_t *sig, uint32_t ms) {
  sig->timeo = ms / 10;
}

void rwmsg_signature_destroy(rwmsg_endpoint_t *ep,
			     rwmsg_signature_t *sig) {
  if (!sig->refct) {
    RW_FREE_TYPE(sig, rwmsg_signature_t);
    ck_pr_dec_32(&ep->stat.objects.signatures);
  }
}

void rwmsg_signature_release(rwmsg_endpoint_t *ep,
			     rwmsg_signature_t *sig) {
  bool zero=0;
  ck_pr_dec_32_zero(&sig->refct, &zero);
  if (zero) {
    rwmsg_garbage(&ep->gc, RWMSG_OBJTYPE_SIGNATURE, sig, NULL, NULL);
  }
}
