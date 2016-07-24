
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmsg_destination.c
 * @author Grant Taylor <grant.taylor@riftio.com>
 * @date 2014/01/26
 * @brief RWMSG destination object
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

rwmsg_destination_t *rwmsg_destination_create(rwmsg_endpoint_t *ep,
					      rwmsg_addrtype_t atype, 
					      const char *addrpath) {
  RW_ASSERT_TYPE(ep, rwmsg_endpoint_t);
  rwmsg_destination_t *dt = NULL;

  int plen = strlen(addrpath);
  if (plen >= RWMSG_PATH_MAX) {
    goto ret;
  }

  dt = (rwmsg_destination_t*)RW_MALLOC0_TYPE(sizeof(*dt), rwmsg_destination_t);
  dt->ep = ep;
  dt->refct = 1;

  RWMSG_EP_LOCK(ep);
  RW_DL_INSERT_HEAD(&ep->track.destinations, dt, trackelem);
  RWMSG_EP_UNLOCK(ep);
  ck_pr_inc_32(&ep->stat.objects.destinations);
  
  RW_ASSERT(atype == RWMSG_ADDRTYPE_UNICAST);
  dt->destination_ct=1;
  dt->atype = atype;
  strncpy(dt->apath, addrpath, sizeof(dt->apath));
  dt->apath[sizeof(dt->apath)-1]='\0';
  dt->apathhash = rw_hashlittle64(dt->apath, plen);

  dt->defstream.tx_win = 0;	/* never used without feedme cb */
  dt->defstream.defstream = TRUE;
  dt->defstream.refct = 1;
  dt->defstream.dest = dt;	/* no ref, this stream not destroyed in rwmsg_clichan.c */

 ret:  
  return dt;
}

void rwmsg_destination_set_noconndontq(rwmsg_destination_t *dt) {
  RW_ASSERT_TYPE(dt, rwmsg_destination_t);
  dt->noconndontq = 1;
}

void rwmsg_destination_unset_noconndontq(rwmsg_destination_t *dt) {
  RW_ASSERT_TYPE(dt, rwmsg_destination_t);
  dt->noconndontq = 0;
}

void rwmsg_destination_destroy(rwmsg_destination_t *dt) {
  RW_ASSERT_TYPE(dt, rwmsg_destination_t);
  if (!dt->refct) {
    rwmsg_endpoint_t *ep = dt->ep;
    RW_ASSERT(ep);

    //?? TBD rwmsg_endpoint_del_srvchan_method_binding(ep, sc, ...);

    RWMSG_EP_LOCK(ep);
    RW_DL_REMOVE(&ep->track.destinations, dt, trackelem);
    RWMSG_EP_UNLOCK(ep);
    ck_pr_dec_32(&ep->stat.objects.destinations);
    
    if (dt->localep) {
      rwmsg_endpoint_release(dt->localep);
      dt->localep = NULL;
    }
    
    if (dt->defstream.localsc) {
      _RWMSG_CH_DEBUG_(&dt->defstream.localsc->ch, "--");
      rwmsg_srvchan_release(dt->defstream.localsc);
      dt->defstream.localsc = NULL;
    }
    
    RW_FREE_TYPE(dt, rwmsg_destination_t);
    dt = NULL;
  }
  return;
}

void rwmsg_destination_release(rwmsg_destination_t *dt) {
  RW_ASSERT_TYPE(dt, rwmsg_destination_t);
  bool zero=0;
  ck_pr_dec_32_zero(&dt->refct, &zero);
  if (zero) {
    rwmsg_garbage(&dt->ep->gc, RWMSG_OBJTYPE_DESTINATION, dt, NULL, NULL);
  }
}

void rwmsg_destination_feedme_callback(rwmsg_destination_t *dest, 
				       rwmsg_closure_t *cb) {
  RW_ASSERT_TYPE(dest, rwmsg_destination_t);
  if (cb) {
    memcpy(&dest->defstream.cb, cb, sizeof(dest->defstream.cb));
    int defwin = 1;
    rwmsg_endpoint_get_property_int(dest->ep, "/rwmsg/destination/defwin", &defwin);
    dest->defstream.tx_win = defwin;
    dest->feedme_count ++;
  } else {
    memset(&dest->defstream.cb, 0, sizeof(dest->defstream.cb));
    dest->defstream.tx_win = 0;	/* never be used without feedme cb */
    dest->feedme_count --;
  }
}
